/*
    日志类
    负责创建日志目录 写入带时间戳的日志
    全局只创建一个日志对象 logger 在 main 函数中初始化
*/

#pragma once
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace gen {

// 日志级别
enum class LogLevel {
    Info,
    Warning,
    Error
};

// 日志类 线程安全 每写一行立即刷新 便于崩溃后查看
class Logger {
private:
    std::mutex mutex_; // 保护日志文件
    std::ofstream out_; // 日志文件
    std::filesystem::path path_; // 日志文件路径
    bool opened_ = false; // 日志文件是否打开
    bool echoInfo_ = false; // 是否把普通信息同时输出到标准错误

    // 返回当前时间的可读字符串 格式为 年-月-日 时:分:秒.毫秒
    static std::string currentTime() {
        auto now = std::chrono::system_clock::now();
        auto seconds = std::chrono::system_clock::to_time_t(now);
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        std::tm timeBuffer{};
        std::tm *local = std::localtime(&seconds);
        if(local != nullptr) timeBuffer = *local;
        char text[64] = {0};
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                      timeBuffer.tm_year + 1900, timeBuffer.tm_mon + 1, timeBuffer.tm_mday,
                      timeBuffer.tm_hour, timeBuffer.tm_min, timeBuffer.tm_sec,
                      static_cast<int>(milliseconds));
        return std::string(text);
    }

    // 返回级别的显示名
    static const char* levelName(LogLevel level) {
        switch(level) {
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            default: return "INFO";
        }
    }

    // 不加锁地写入一条日志 调用者必须已经持有锁
    void writeUnlocked(LogLevel level, const std::string &message) {
        std::string time = currentTime();
        std::string line = "[" + time + "][" + levelName(level) + "] ";
        std::string body = message;
        std::string::size_type start = 0;
        bool first = true;
        while(true) {
            std::string::size_type end = body.find('\n', start);
            std::string piece = (end == std::string::npos) ? body.substr(start) : body.substr(start, end - start);
            if(opened_) {
                out_ << (first ? line : "    ") << piece << '\n';
                out_.flush();
            }
            first = false;
            if(end == std::string::npos) break;
            start = end + 1;
        }
        if(opened_ == false) {
            std::cerr << line << message << std::endl;
        }
    }

public:
    Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator = (const Logger&) = delete;

    // 初始化日志 参数 directory 为日志目录 不存在时自动创建 参数 fileName 为日志文件名
    // 成功返回 true 失败时在标准错误输出提示并返回 false
    bool init(const std::filesystem::path &directory, const std::string &fileName = "serve.log") {
        std::lock_guard<std::mutex> lock(mutex_);
        if(opened_ && out_.is_open()) {
            out_.flush();
            out_.close();
        }
        opened_ = false;
        std::error_code error;
        if(directory.empty() == false && std::filesystem::exists(directory, error) == false) {
            std::filesystem::create_directories(directory, error);
            if(error) {
                std::cerr << "无法创建日志目录 " << directory.string() << " 原因 " << error.message() << std::endl;
                return false;
            }
        }
        path_ = directory / fileName;
        out_.open(path_, std::ios::out | std::ios::trunc);
        if(!out_.is_open()) {
            std::cerr << "无法打开日志文件 " << path_.string() << std::endl;
            return false;
        }
        opened_ = true;
        writeUnlocked(LogLevel::Info, "日志文件创建成功 路径 " + path_.string());
        return true;
    }

    // 是否已经初始化
    bool isOpen() const {
        return opened_;
    }

    // 返回日志文件路径
    std::filesystem::path path() const {
        return path_;
    }

    // 设置是否把普通信息同时输出到标准错误 参数 value 为 true 时输出
    void setEchoInfo(bool value) {
        echoInfo_ = value;
    }

    // 写入普通信息
    void info(const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writeUnlocked(LogLevel::Info, message);
        }
        if(echoInfo_) std::cerr << "[INFO] " << message << std::endl;
    }

    // 写入警告信息 同时输出到标准错误
    void warning(const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writeUnlocked(LogLevel::Warning, message);
        }
        std::cerr << "[WARN] " << message << std::endl;
    }

    // 写入错误信息 同时输出到标准错误
    void error(const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writeUnlocked(LogLevel::Error, message);
        }
        std::cerr << "[ERROR] " << message << std::endl;
    }

    // 立即刷新日志文件
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(opened_) out_.flush();
    }

    // 关闭日志文件
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(opened_) {
            out_.flush();
            out_.close();
            opened_ = false;
        }
    }

    ~Logger() {
        close();
    }
};

// 全局日志对象
inline Logger logger;

} // namespace gen
