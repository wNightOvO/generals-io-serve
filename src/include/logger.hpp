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

namespace fs = std::filesystem;

class Logger {
private:
    std::ofstream out;
public:
    inline static const std::string LOG_FILE = "logs";

    Logger(const std::string &filename) {
        if(!fs::exists(LOG_FILE)) {
            fs::create_directory(LOG_FILE);
        }
        out.open(fs::path(LOG_FILE) / filename);
        this->info(std::format("日志 {} 创建成功",filename));
    }

    ~Logger() {
        if(out.is_open()) {
            out << std::flush;
            out.close();
        }
    }

    void info(const std::string &msg) {
        out << "INFO: " << msg << "\n";
    }

    void warning(const std::string &msg) {
        out << "!WARNING: " << msg << "\n";
    }

    void error(const std::string &msg) {
        out << "!!!ERROR: " << msg << "\n";
    }
};

Logger logger("log.txt");

}