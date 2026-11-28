/*
    通信通道实现
    负责创建管道 启动 AI 进程 发送长度前缀帧 读取动作行
*/

#include <io/io.hpp>

#include <include/logger.hpp>
#include <include/unique_handle.hpp>
#include <io/protocol.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace gen::io { // namespace gen::io

namespace fs = std::filesystem;

namespace { // namespace

// 单次读取的最大字节数
constexpr std::size_t READ_CHUNK_BYTES = 4096;
// 单行输出最大字节数 动作行远小于该值
constexpr std::size_t MAX_LINE_BYTES = 64 * 1024;
// 输出缓冲最大字节数 超过时清空并记录日志
constexpr std::size_t MAX_BUFFER_BYTES = 1024 * 1024;
// 单次轮询最多读取的字节数 避免持续输出时长时间不检查超时
constexpr std::size_t DRAIN_LIMIT_BYTES = 256 * 1024;
// 管道内核缓冲区大小
constexpr std::size_t PIPE_BUFFER_BYTES = 64 * 1024;
// 前若干毫秒让步式轮询 之后改为短睡眠 兼顾响应延迟与处理器占用
constexpr int POLL_SPIN_MS = 2;

// 读取一行结果的内部状态
enum class ReadLineStatus {
    Line, // 成功读到一行
    Timeout, // 超时
    Eof, // 输出管道断开
    Crashed, // 进程已退出
    TooLong // 单行输出超过长度上限
};

// 判断字符是否为空白
bool isSpaceChar(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n'
        || value == '\v' || value == '\f';
}

// 去除行首尾的空白字符与回车
void trimLine(std::string &line) {
    std::size_t begin = 0;
    std::size_t end = line.size();
    while(begin < end && isSpaceChar(line[begin])) begin ++;
    while(end > begin && isSpaceChar(line[end - 1])) end --;
    line = line.substr(begin, end - begin);
}

// 循环写入直到全部字节写完 成功返回 true
bool writeAll(HANDLE handle, const void *data, std::size_t size) {
    const char *cursor = static_cast<const char*>(data);
    std::size_t remaining = size;
    while(remaining > 0) {
        DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, 1u << 20));
        DWORD written = 0;
        if(WriteFile(handle, cursor, chunk, &written, nullptr) == FALSE) return false;
        if(written == 0) return false;
        cursor += written;
        remaining -= written;
    }
    return true;
}

// 给命令行参数加引号 路径含空格时必须加
std::string quoteArgument(const std::string &value) {
    bool needQuote = value.empty()
        || value.find(' ') != std::string::npos
        || value.find('\t') != std::string::npos
        || value.find('"') != std::string::npos;
    if(needQuote == false) return value;
    std::string result = "\"";
    for(char c : value) {
        if(c == '"') result += "\\\"";
        else result += c;
    }
    result += "\"";
    return result;
}

// 把整数解析结果写入 value 要求记号完整合法 成功返回 true
bool parseCoordinate(const std::string &token, int &value) {
    if(token.empty()) return false;
    int result = 0;
    const char *begin = token.data();
    const char *end = begin + token.size();
    std::from_chars_result outcome = std::from_chars(begin, end, result);
    if(outcome.ec != std::errc() || outcome.ptr != end) return false;
    value = result;
    return true;
}

// 把一行文本按空白切成记号
std::vector<std::string> splitTokens(const std::string &line) {
    std::vector<std::string> tokens;
    std::size_t position = 0;
    while(position < line.size()) {
        while(position < line.size() && isSpaceChar(line[position])) position ++;
        std::size_t begin = position;
        while(position < line.size() && isSpaceChar(line[position]) == false) position ++;
        if(position > begin) tokens.push_back(line.substr(begin, position - begin));
    }
    return tokens;
}

// 拼接 Windows 错误码文本 用于日志
std::string errorCodeText(DWORD error) {
    return "错误码 " + std::to_string(error);
}

// 返回可执行文件路径的绝对形式 失败时返回原路径
fs::path absolutePath(const fs::path &path) {
    std::error_code error;
    fs::path result = fs::absolute(path, error);
    if(error) return path;
    return result;
}

} // namespace

struct AIChannel::Impl { // 内部实现 保存句柄 缓冲与进程状态
    UniqueHandle process; // 子进程句柄
    UniqueHandle stdinWrite; // 父进程写入端 对应子进程标准输入
    UniqueHandle stdoutRead; // 父进程读取端 对应子进程标准输出
    std::string buffer; // 已读取但尚未解析的输出
    std::string name; // 玩家 id 用于日志
    bool started = false; // 是否已经启动
    bool exitChecked = false; // 是否已经取到退出码
    bool exitLogged = false; // 是否已经记录过进程退出
    bool extraLineWarned = false; // 是否已经警告过单次输出多行
    unsigned long exitCode = 0; // 进程退出码

    // 判断进程是否已经退出 已退出时参数 code 返回退出码
    bool hasExited(unsigned long &code) {
        if(process.valid() == false) {
            code = exitCode;
            return true;
        }
        if(exitChecked) {
            code = exitCode;
            return true;
        }
        if(WaitForSingleObject(process.get(), 0) != WAIT_OBJECT_0) return false;
        exitChecked = true;
        DWORD value = 0;
        GetExitCodeProcess(process.get(), &value);
        exitCode = value;
        code = value;
        return true;
    }

    // 记录进程退出事件 参数 code 为退出码 参数 abnormal 表示是否异常退出
    void noteExit(unsigned long code, bool abnormal) {
        if(exitLogged) return;
        exitLogged = true;
        std::string message = "玩家 " + name + " 进程已退出 退出码 " + std::to_string(code);
        if(abnormal) logger.error(message);
        else logger.info(message);
    }

    // 把管道内当前可读数据全部读入缓冲区 管道断开返回 false
    bool drain() {
        std::size_t total = 0;
        for(;;) {
            DWORD available = 0;
            if(PeekNamedPipe(stdoutRead.get(), nullptr, 0, nullptr, &available, nullptr) == FALSE) {
                return false;
            }
            if(available == 0) return true;
            if(total >= DRAIN_LIMIT_BYTES) return true;
            DWORD want = static_cast<DWORD>(std::min<std::size_t>(available, READ_CHUNK_BYTES));
            char chunk[READ_CHUNK_BYTES] = {0};
            DWORD got = 0;
            if(ReadFile(stdoutRead.get(), chunk, want, &got, nullptr) == FALSE) return false;
            if(got == 0) return false;
            buffer.append(chunk, static_cast<std::size_t>(got));
            total += got;
            if(buffer.size() > MAX_LINE_BYTES && buffer.find('\n') == std::string::npos) return true;
        }
    }

    // 判断缓冲区首行是否超过长度上限
    bool firstLineTooLong() const {
        std::size_t position = buffer.find('\n');
        if(position != std::string::npos) return position > MAX_LINE_BYTES;
        return buffer.size() > MAX_LINE_BYTES;
    }

    // 从缓冲区取出一行 行内容已去除首尾空白与回车 成功返回 true
    // 缓冲区内多出的内容保留到下一次读取 只在第一次发现时记录警告
    bool takeLine(std::string &line) {
        std::size_t position = buffer.find('\n');
        if(position == std::string::npos) return false;
        line.assign(buffer, 0, position);
        trimLine(line);
        buffer.erase(0, position + 1);
        if(extraLineWarned == false && buffer.find('\n') != std::string::npos) {
            extraLineWarned = true;
            logger.warning("玩家 " + name + " 单次输出包含多行 多出的内容保留到下一次读取");
        }
        return true;
    }

    // 丢弃缓冲区并记录日志 参数 reason 为原因
    void dropBuffer(const std::string &reason) {
        if(buffer.empty()) return;
        logger.warning("玩家 " + name + " 丢弃输出缓冲区 " + std::to_string(buffer.size()) + " 字节 原因 " + reason);
        buffer.clear();
    }

    // 读取一行并返回内部状态 参数 line 返回读取到的行
    ReadLineStatus readLineStatus(std::string &line, int timeoutMs) {
        line.clear();
        if(started == false) {
            logger.error("玩家 " + name + " 通信通道未启动 无法读取输出");
            return ReadLineStatus::Crashed;
        }
        using Clock = std::chrono::steady_clock;
        Clock::time_point begin = Clock::now();
        int waitMs = timeoutMs < 0 ? 0 : timeoutMs;
        Clock::time_point deadline = begin + std::chrono::milliseconds(waitMs);
        for(;;) {
            if(firstLineTooLong()) {
                std::string reason = "单行输出超过上限 " + std::to_string(MAX_LINE_BYTES) + " 字节";
                dropBuffer(reason);
                return ReadLineStatus::TooLong;
            }
            if(takeLine(line)) return ReadLineStatus::Line;
            bool pipeOpen = drain();
            if(firstLineTooLong()) {
                std::string reason = "单行输出超过上限 " + std::to_string(MAX_LINE_BYTES) + " 字节";
                dropBuffer(reason);
                return ReadLineStatus::TooLong;
            }
            if(takeLine(line)) return ReadLineStatus::Line;
            if(buffer.size() > MAX_BUFFER_BYTES) {
                dropBuffer("输出缓冲超过上限 " + std::to_string(MAX_BUFFER_BYTES) + " 字节");
            }
            if(pipeOpen == false) {
                unsigned long code = 0;
                if(hasExited(code)) {
                    noteExit(code, true);
                    return ReadLineStatus::Crashed;
                }
                logger.warning("玩家 " + name + " 输出管道已断开");
                return ReadLineStatus::Eof;
            }
            unsigned long code = 0;
            if(hasExited(code)) {
                noteExit(code, true);
                dropBuffer("进程退出时输出未成完整行");
                return ReadLineStatus::Crashed;
            }
            Clock::time_point now = Clock::now();
            if(now >= deadline) {
                std::string message = "玩家 " + name + " 读取输出超时 已等待 " + std::to_string(waitMs) + " 毫秒";
                if(buffer.empty() == false) {
                    message += " 缓冲区内残留 " + std::to_string(buffer.size()) + " 字节未成行";
                }
                logger.warning(message);
                return ReadLineStatus::Timeout;
            }
            if(now - begin < std::chrono::milliseconds(POLL_SPIN_MS)) {
                Sleep(0);
            }
            else {
                Sleep(1);
            }
        }
    }
};

Channel::Channel(std::string id, int playerID, int teamID)
    : id_(std::move(id)), playerID_(playerID), teamID_(teamID) {}

const char* actionStatusName(ActionStatus status) {
    switch(status) {
        case ActionStatus::Pass: return "不移动";
        case ActionStatus::Move: return "移动";
        case ActionStatus::Invalid: return "非法输出";
        case ActionStatus::Timeout: return "超时";
        case ActionStatus::Eof: return "输出结束";
        case ActionStatus::Crashed: return "进程崩溃";
        default: return "未知状态";
    }
}

AIChannel::AIChannel(std::string id, int playerID, int teamID)
    : Channel(std::move(id), playerID, teamID), impl_(std::make_unique<Impl>()) {
    impl_->name = this->id();
}

AIChannel::~AIChannel() {
    if(impl_ == nullptr) return;
    if(impl_->process.valid() && isRunning()) {
        logger.warning("玩家 " + impl_->name + " 通信通道析构时进程仍在运行 强制结束");
        terminate();
    }
    // impl_ 的智能句柄自动关闭全部句柄
    impl_.reset();
}

bool AIChannel::start(const fs::path &exePath, const fs::path &workDir, const fs::path &stderrPath) {
    if(impl_ == nullptr) return false;
    if(impl_->started) {
        logger.error("玩家 " + impl_->name + " 已经启动 不能重复启动");
        return false;
    }
    std::error_code error;
    if(fs::exists(exePath, error) == false || fs::is_regular_file(exePath, error) == false) {
        logger.error("玩家 " + impl_->name + " 可执行文件不存在 路径 " + exePath.string());
        return false;
    }
    fs::path exeAbsolute = absolutePath(exePath);
    fs::path workAbsolute = workDir.empty() ? fs::current_path(error) : absolutePath(workDir);
    if(error) workAbsolute = fs::current_path();
    if(fs::exists(workAbsolute, error) == false || fs::is_directory(workAbsolute, error) == false) {
        logger.error("玩家 " + impl_->name + " 工作目录不存在 路径 " + workAbsolute.string());
        return false;
    }

    // 标准错误文件 打开后交给子进程继承 父进程在启动完成后关闭
    fs::path stderrAbsolute = absolutePath(stderrPath);
    fs::path stderrParent = stderrAbsolute.parent_path();
    if(stderrParent.empty() == false) {
        fs::create_directories(stderrParent, error);
        if(error) {
            logger.error("玩家 " + impl_->name + " 无法创建 AI 调试输出目录 路径 " + stderrParent.string()
                         + " 原因 " + error.message());
            return false;
        }
    }
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    UniqueHandle stderrFile(CreateFileW(stderrAbsolute.wstring().c_str(),
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ,
                                        &attributes,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr));
    if(stderrFile.valid() == false) {
        logger.error("玩家 " + impl_->name + " 无法创建 AI 调试输出文件 路径 " + stderrAbsolute.string()
                     + " " + errorCodeText(GetLastError()));
        return false;
    }

    // 子进程标准输入 父进程写入 子进程读取
    HANDLE stdinReadRaw = nullptr;
    HANDLE stdinWriteRaw = nullptr;
    if(CreatePipe(&stdinReadRaw, &stdinWriteRaw, &attributes, static_cast<DWORD>(PIPE_BUFFER_BYTES)) == FALSE) {
        logger.error("玩家 " + impl_->name + " 创建标准输入管道失败 " + errorCodeText(GetLastError()));
        return false;
    }
    UniqueHandle childStdinRead(stdinReadRaw);
    UniqueHandle parentStdinWrite(stdinWriteRaw);
    if(SetHandleInformation(parentStdinWrite.get(), HANDLE_FLAG_INHERIT, 0) == FALSE) {
        logger.error("玩家 " + impl_->name + " 设置标准输入管道不可继承失败 " + errorCodeText(GetLastError()));
        return false;
    }

    // 子进程标准输出 子进程写入 父进程读取
    HANDLE stdoutReadRaw = nullptr;
    HANDLE stdoutWriteRaw = nullptr;
    if(CreatePipe(&stdoutReadRaw, &stdoutWriteRaw, &attributes, static_cast<DWORD>(PIPE_BUFFER_BYTES)) == FALSE) {
        logger.error("玩家 " + impl_->name + " 创建标准输出管道失败 " + errorCodeText(GetLastError()));
        return false;
    }
    UniqueHandle parentStdoutRead(stdoutReadRaw);
    UniqueHandle childStdoutWrite(stdoutWriteRaw);
    if(SetHandleInformation(parentStdoutRead.get(), HANDLE_FLAG_INHERIT, 0) == FALSE) {
        logger.error("玩家 " + impl_->name + " 设置标准输出管道不可继承失败 " + errorCodeText(GetLastError()));
        return false;
    }

    // 启动子进程 隐藏窗口 不创建控制台
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.hStdInput = childStdinRead.get();
    startup.hStdOutput = childStdoutWrite.get();
    startup.hStdError = stderrFile.get();
    startup.wShowWindow = SW_HIDE;
    std::string applicationName = exeAbsolute.string();
    std::string commandLine = quoteArgument(applicationName);
    std::string workingDirectory = workAbsolute.string();
    PROCESS_INFORMATION processInfo{};
    if(CreateProcessA(applicationName.c_str(),
                      commandLine.data(),
                      nullptr,
                      nullptr,
                      TRUE,
                      CREATE_NO_WINDOW,
                      nullptr,
                      workingDirectory.c_str(),
                      &startup,
                      &processInfo) == FALSE) {
        logger.error("玩家 " + impl_->name + " 启动 AI 进程失败 可执行文件 " + exeAbsolute.string()
                     + " 工作目录 " + workingDirectory + " " + errorCodeText(GetLastError()));
        return false;
    }
    UniqueHandle processHandle(processInfo.hProcess);
    UniqueHandle threadHandle(processInfo.hThread);
    threadHandle.reset();

    // 父进程只保留自己的读写端 子进程端在函数结束时自动关闭
    impl_->process = std::move(processHandle);
    impl_->stdinWrite = std::move(parentStdinWrite);
    impl_->stdoutRead = std::move(parentStdoutRead);
    impl_->buffer.clear();
    impl_->started = true;
    impl_->exitChecked = false;
    impl_->exitLogged = false;
    impl_->extraLineWarned = false;
    logger.info("玩家 " + impl_->name + " 启动成功 进程号 " + std::to_string(processInfo.dwProcessId)
                + " 可执行文件 " + exeAbsolute.string()
                + " 工作目录 " + workingDirectory
                + " 调试输出 " + stderrAbsolute.string());
    return true;
}

bool AIChannel::sendFrame(std::uint8_t type, const std::vector<std::uint8_t> &payload) {
    if(impl_ == nullptr || impl_->started == false || impl_->stdinWrite.valid() == false) {
        logger.error("玩家 " + (impl_ == nullptr ? std::string("未知") : impl_->name)
                     + " 发送消息失败 通信通道未就绪或输入方向已关闭");
        return false;
    }
    if(payload.size() + 1 > protocol::MAX_FRAME_BYTES) {
        logger.error("玩家 " + impl_->name + " 发送消息超过单帧长度上限 负载字节数 "
                     + std::to_string(payload.size()) + " 上限 " + std::to_string(protocol::MAX_FRAME_BYTES));
        return false;
    }
    std::uint32_t length = static_cast<std::uint32_t>(payload.size()) + 1u;
    std::uint8_t header[5];
    header[0] = static_cast<std::uint8_t>(length & 0xFFu);
    header[1] = static_cast<std::uint8_t>((length >> 8) & 0xFFu);
    header[2] = static_cast<std::uint8_t>((length >> 16) & 0xFFu);
    header[3] = static_cast<std::uint8_t>((length >> 24) & 0xFFu);
    header[4] = type;
    if(writeAll(impl_->stdinWrite.get(), header, sizeof(header)) == false) {
        logger.error("玩家 " + impl_->name + " 写入消息头部失败 消息类型 " + std::to_string(type)
                     + " " + errorCodeText(GetLastError()));
        return false;
    }
    if(payload.empty() == false
       && writeAll(impl_->stdinWrite.get(), payload.data(), payload.size()) == false) {
        logger.error("玩家 " + impl_->name + " 写入消息负载失败 消息类型 " + std::to_string(type)
                     + " 负载字节数 " + std::to_string(payload.size())
                     + " " + errorCodeText(GetLastError()));
        return false;
    }
    return true;
}

bool AIChannel::readLine(std::string &line, int timeoutMs) {
    if(impl_ == nullptr) {
        line.clear();
        return false;
    }
    return impl_->readLineStatus(line, timeoutMs) == ReadLineStatus::Line;
}

ActionResult AIChannel::readAction(int timeoutMs) {
    ActionResult result;
    std::string line;
    if(impl_ == nullptr) {
        result.status = ActionStatus::Invalid;
        result.detail = "通信通道未初始化";
        logger.error("读取动作失败 通信通道未初始化");
        return result;
    }
    ReadLineStatus status = impl_->readLineStatus(line, timeoutMs);
    switch(status) {
        case ReadLineStatus::Timeout:
            result.status = ActionStatus::Timeout;
            result.detail = "读取动作超时";
            return result;
        case ReadLineStatus::Eof:
            result.status = ActionStatus::Eof;
            result.detail = "AI 输出已结束";
            return result;
        case ReadLineStatus::Crashed:
            result.status = ActionStatus::Crashed;
            result.detail = "AI 进程已退出";
            return result;
        case ReadLineStatus::TooLong:
            result.status = ActionStatus::Invalid;
            result.detail = "动作行超过长度上限 已丢弃";
            return result;
        case ReadLineStatus::Line:
            break;
        default:
            result.status = ActionStatus::Invalid;
            result.detail = "未知读取状态";
            return result;
    }
    result.detail = line;
    if(line.empty()) {
        result.status = ActionStatus::Invalid;
        result.detail = "动作行为空";
        logger.warning("玩家 " + impl_->name + " 输出空动作行");
        return result;
    }
    if(line == "PASS") {
        result.status = ActionStatus::Pass;
        result.detail = "PASS";
        return result;
    }
    std::vector<std::string> tokens = splitTokens(line);
    bool isMove = tokens.empty() == false && (tokens[0] == "M" || tokens[0] == "H");
    if(isMove == false) {
        result.status = ActionStatus::Invalid;
        result.detail = "无法识别的动作行 " + line;
        logger.warning("玩家 " + impl_->name + " 输出无法识别的动作行 " + line);
        return result;
    }
    if(tokens.size() != 5) {
        result.status = ActionStatus::Invalid;
        result.detail = "移动动作参数数量错误 " + line;
        logger.warning("玩家 " + impl_->name + " 输出移动动作参数数量错误 " + line);
        return result;
    }
    int values[4] = {0, 0, 0, 0};
    for(int i = 0; i < 4; i ++) {
        if(parseCoordinate(tokens[static_cast<std::size_t>(i) + 1], values[i]) == false) {
            result.status = ActionStatus::Invalid;
            result.detail = "移动动作坐标不是合法整数 " + line;
            logger.warning("玩家 " + impl_->name + " 输出移动动作坐标不是合法整数 " + line);
            return result;
        }
    }
    result.status = ActionStatus::Move;
    result.move = MoveOperation(playerID(), Point(values[0], values[1]), Point(values[2], values[3]), tokens[0] == "H");
    result.detail = line;
    return result;
}

void AIChannel::closeInput() {
    if(impl_ == nullptr) return;
    if(impl_->stdinWrite.valid() == false) return;
    impl_->stdinWrite.reset();
    logger.info("玩家 " + impl_->name + " 输入方向已关闭 用于通知 AI 对局结束");
}

bool AIChannel::isRunning() const {
    if(impl_ == nullptr || impl_->process.valid() == false) return false;
    return WaitForSingleObject(impl_->process.get(), 0) == WAIT_TIMEOUT;
}

bool AIChannel::waitExit(int timeoutMs) {
    if(impl_ == nullptr || impl_->process.valid() == false) return true;
    DWORD waitMs = timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs);
    DWORD state = WaitForSingleObject(impl_->process.get(), waitMs);
    if(state == WAIT_OBJECT_0) {
        unsigned long code = 0;
        if(impl_->hasExited(code)) impl_->noteExit(code, false);
        return true;
    }
    if(state == WAIT_TIMEOUT) return false;
    logger.error("玩家 " + impl_->name + " 等待进程退出失败 " + errorCodeText(GetLastError()));
    return false;
}

void AIChannel::terminate() {
    if(impl_ == nullptr || impl_->process.valid() == false) return;
    unsigned long code = 0;
    if(impl_->hasExited(code)) {
        impl_->noteExit(code, false);
        return;
    }
    logger.warning("玩家 " + impl_->name + " 强制结束进程");
    impl_->stdinWrite.reset();
    if(TerminateProcess(impl_->process.get(), 1) == FALSE) {
        logger.error("玩家 " + impl_->name + " 强制结束进程失败 " + errorCodeText(GetLastError()));
        return;
    }
    WaitForSingleObject(impl_->process.get(), 3000);
    if(impl_->hasExited(code)) impl_->noteExit(code, false);
}

} // namespace gen::io
