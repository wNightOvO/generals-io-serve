/*
    程序入口
    解析命令行参数 读取配置 初始化日志 启动运行器
*/

#include <core/config.hpp>
#include <core/runner.hpp>
#include <include/logger.hpp>

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

namespace { // namespace

using gen::logger;
using gen::core::Config;
using gen::core::ConfigError;
using gen::core::RunOptions;
using gen::core::Runner;
using std::format;
using std::string;

// 打印帮助信息
void showHelp() {
    std::cout << "generals.io serve 使用说明\n"
              << "  serve.exe [选项]\n"
              << "选项\n"
              << "  --config <路径>  指定配置文件 默认 config/example.json\n"
              << "  --no-ui          不显示图形界面\n"
              << "  --seed <数字>    指定随机种子 便于复现\n"
              << "  --no-pause       出错时不等待按键 便于自动测试\n"
              << "  --help           显示本帮助\n";
}

// 是否是交互式控制台 双击运行时为真
bool isInteractiveConsole() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD inputMode = 0;
    DWORD outputMode = 0;
    if(GetConsoleMode(input, &inputMode) == FALSE) return false;
    if(GetConsoleMode(output, &outputMode) == FALSE) return false;
    return true;
}

// 出错时等待用户按回车 便于双击运行时看清提示
void pauseIfNeeded(bool pause) {
    if(pause == false) return;
    if(isInteractiveConsole() == false) return;
    std::cout << "按回车键退出" << std::flush;
    std::cin.get();
}

// 解析命令行参数 成功返回 0 显示帮助返回 1 参数错误返回 2
int parseArguments(int argc, char **argv, RunOptions &options, bool &pauseOnError) {
    pauseOnError = true;
    for(int i = 1; i < argc; i ++) {
        string argument = argv[i];
        if(argument == "--help" || argument == "-h") {
            return 1;
        }
        else if(argument == "--config" || argument == "-c") {
            if(i + 1 >= argc) {
                std::cerr << "参数 --config 后面缺少配置文件路径" << std::endl;
                return 2;
            }
            options.configPath = argv[++ i];
        }
        else if(argument == "--no-ui") {
            options.enableUi = false;
        }
        else if(argument == "--no-pause") {
            pauseOnError = false;
        }
        else if(argument == "--seed") {
            if(i + 1 >= argc) {
                std::cerr << "参数 --seed 后面缺少随机种子" << std::endl;
                return 2;
            }
            string text = argv[++ i];
            try {
                unsigned long long value = std::stoull(text);
                if(value > 0xffffffffull) {
                    std::cerr << "随机种子超出范围 " << text << std::endl;
                    return 2;
                }
                options.seed = static_cast<std::uint32_t>(value);
            }
            catch(const std::exception &) {
                std::cerr << "随机种子不是合法数字 " << text << std::endl;
                return 2;
            }
        }
        else {
            std::cerr << "未知参数 " << argument << std::endl;
            return 2;
        }
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("generals.io serve");

    RunOptions options;
    bool pauseOnError = true;
    int parseResult = parseArguments(argc, argv, options, pauseOnError);
    if(parseResult == 1) {
        showHelp();
        return 0;
    }
    if(parseResult == 2) {
        showHelp();
        pauseIfNeeded(pauseOnError);
        return 1;
    }

    Config config;
    try {
        config = Config::load(options.configPath);
    }
    catch(const ConfigError &error) {
        std::cerr << "配置错误 " << error.what() << std::endl;
        std::cerr << "配置文件 " << std::filesystem::absolute(options.configPath).string() << std::endl;
        pauseIfNeeded(pauseOnError);
        return 1;
    }
    catch(const std::exception &error) {
        std::cerr << "读取配置失败 " << error.what() << std::endl;
        pauseIfNeeded(pauseOnError);
        return 1;
    }

    try {
        if(logger.init(config.log.directory, "serve.log") == false) {
            std::cerr << "无法初始化日志 请检查目录 " << config.log.directory << " 是否可写" << std::endl;
            pauseIfNeeded(pauseOnError);
            return 1;
        }
        logger.info(format("程序启动 配置文件 {} 界面 {} 随机种子 {}",
                           std::filesystem::absolute(options.configPath).string(),
                           options.enableUi && config.ui.enabled ? "开启" : "关闭",
                           options.seed));
        Runner runner(config, options);
        int code = runner.run();
        logger.info(format("程序结束 退出码 {}", code));
        logger.close();
        if(code != 0) {
            pauseIfNeeded(pauseOnError);
        }
        return code;
    }
    catch(const std::exception &error) {
        logger.error(format("程序异常 {}", error.what()));
        std::cerr << "程序异常 " << error.what() << std::endl;
        logger.close();
        pauseIfNeeded(pauseOnError);
        return 2;
    }
}
