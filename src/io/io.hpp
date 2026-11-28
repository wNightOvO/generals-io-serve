/*
    游戏程序与 AI 程序的通信通道
    负责启动 AI 进程 发送长度前缀帧 读取动作行
*/

#pragma once
#include <include/move_operation.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace gen::io {

// 读取动作的结果状态
enum class ActionStatus {
    Pass, // 主动不移动
    Move, // 合法移动
    Invalid, // 非法输出或非法动作
    Timeout, // 超时
    Eof, // 输出结束
    Crashed // 进程崩溃或异常退出
};

// 返回状态的中文名 用于日志
const char* actionStatusName(ActionStatus status);

// 一次读取动作的结果
struct ActionResult {
    ActionStatus status = ActionStatus::Pass; // 状态
    MoveOperation move; // status 为 Move 时有效
    std::string detail; // 说明 用于日志
};

// 通信通道基类
class Channel {
private:
    std::string id_; // 玩家 id
    int playerID_ = 0; // 玩家编号
    int teamID_ = 0; // 队伍编号
public:
    Channel(std::string id, int playerID, int teamID);

    virtual ~Channel() = default;

    // 返回玩家 id
    const std::string& id() const {
        return id_;
    }

    // 返回玩家编号
    int playerID() const {
        return playerID_;
    }

    // 返回队伍编号
    int teamID() const {
        return teamID_;
    }

    // 启动 AI 程序
    // 参数 exePath 为可执行文件路径 参数 workDir 为工作目录 参数 stderrPath 为 AI 调试输出保存路径
    // 成功返回 true
    virtual bool start(const std::filesystem::path &exePath,
                       const std::filesystem::path &workDir,
                       const std::filesystem::path &stderrPath) = 0;

    // 发送一条消息 参数 type 为消息类型 参数 payload 为负载 不含类型字节 成功返回 true
    virtual bool sendFrame(std::uint8_t type, const std::vector<std::uint8_t> &payload) = 0;

    // 读取一行文本 参数 line 返回读取到的行 不含换行 参数 timeoutMs 为超时毫秒数
    // 成功读到一行返回 true 超时 输出结束或进程崩溃返回 false
    virtual bool readLine(std::string &line, int timeoutMs) = 0;

    // 读取一个动作 参数 timeoutMs 为超时毫秒数
    virtual ActionResult readAction(int timeoutMs) = 0;

    // 关闭发送方向 通知 AI 程序不会再有新消息 用于对局结束
    virtual void closeInput() = 0;

    // 进程是否仍在运行
    virtual bool isRunning() const = 0;

    // 等待进程退出 参数 timeoutMs 为超时毫秒数 已退出返回 true
    virtual bool waitExit(int timeoutMs) = 0;

    // 强制结束进程
    virtual void terminate() = 0;
};

// 与外部 AI 可执行文件通信的通道
class AIChannel : public Channel {
public:
    AIChannel(std::string id, int playerID, int teamID);

    ~AIChannel() override;

    AIChannel(const AIChannel&) = delete;
    AIChannel& operator = (const AIChannel&) = delete;

    bool start(const std::filesystem::path &exePath,
               const std::filesystem::path &workDir,
               const std::filesystem::path &stderrPath) override;

    bool sendFrame(std::uint8_t type, const std::vector<std::uint8_t> &payload) override;

    bool readLine(std::string &line, int timeoutMs) override;

    ActionResult readAction(int timeoutMs) override;

    void closeInput() override;

    bool isRunning() const override;

    bool waitExit(int timeoutMs) override;

    void terminate() override;
private:
    struct Impl; // 内部实现 保存进程与句柄
    std::unique_ptr<Impl> impl_; // 内部实现指针
};

} // namespace gen::io
