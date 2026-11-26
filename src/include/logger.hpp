/*
    日志类
    并创建一个对象
*/

#pragma once
#include <fstream>
#include <cstring>
#include <filesystem>
#include <format>

namespace gen {

class Logger {
private:
    std::ofstream out;
public:
    inline static const std::string LOG_DIRECTORY_STR = "logs";
    inline static const std::filesystem::path LOG_DIRECTORY = std::filesystem::path(LOG_DIRECTORY_STR);

    Logger(const std::string &filename) {
        if(!std::filesystem::exists(LOG_DIRECTORY)) {
            std::filesystem::create_directory(LOG_DIRECTORY);
        }
        std::filesystem::path logPath = LOG_DIRECTORY / filename;
        out.open(logPath);
        this->info(std::format("日志 {} 创建成功。路径：{}。", filename, logPath.string()));
    }

    ~Logger() {
        if(out.is_open()) {
            out << std::flush;
            out.close();
        }
    }

    void info(const std::string &msg) {
        out << "\tINFO:\n" << msg << "\n";
    }

    void warning(const std::string &msg) {
        out << "\t!WARNING:\n" << msg << "\n";
    }

    void error(const std::string &msg) {
        out << "\t!!!ERROR:\n" << msg << "\n";
    }
};

inline Logger logger("log.txt");

}