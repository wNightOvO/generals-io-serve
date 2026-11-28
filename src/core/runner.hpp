/*
    程序总编排
    读取配置 发现玩家 组织比赛 逐局运行 统计排名
*/

#pragma once
#include <core/config.hpp>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace gen::core {

// 命令行选项
struct RunOptions {
    std::filesystem::path configPath = "config/example.json"; // 配置文件路径
    bool enableUi = true; // 是否显示界面
    std::uint32_t seed = 0; // 随机种子 0 表示随机
};

// 程序总编排器
class Runner {
private:
    Config config_; // 配置
    RunOptions options_; // 运行选项
    std::uint32_t baseSeed_ = 0; // 基础随机种子
    int runMatchCount_ = 0; // 实际运行的比赛数
    int runGameCount_ = 0; // 实际运行的局数
public:
    // 构造 参数 config 为已校验配置 参数 options 为运行选项
    Runner(Config config, RunOptions options);

    // 运行全部比赛 返回进程退出码 0 成功 1 环境或配置错误 2 运行错误
    int run();

    // 返回实际运行的比赛数
    int matchCount() const {
        return runMatchCount_;
    }

    // 返回实际运行的局数
    int gameCount() const {
        return runGameCount_;
    }
};

} // namespace gen::core
