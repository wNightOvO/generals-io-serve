/*
    游戏配置
    从 JSON 文件读取配置 校验后供整个程序使用
*/

#pragma once
#include <include/type.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace gen::core {

// 配置错误 配置文件缺失 语法错误 类型错误 取值越界都会抛出本异常
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string &message): std::runtime_error(message) {}
};

// 游戏模式
enum class GameMode {
    Duel, // 1v1 两名玩家两支队伍
    Team, // 2v2 四名玩家两支队伍
    FFA // 混战 每名玩家一支队伍
};

// 返回模式名 例如 "1v1" "2v2" "ffa"
const char* modeName(GameMode mode);

// 由模式名解析模式 大小写不敏感 成功返回 true
bool modeFromName(const std::string &name, GameMode &mode);

// 地图相关配置
struct MapConfig {
    int rows = 20; // 地图行数
    int cols = 20; // 地图列数
    std::array<bool, TERRAIN_COUNT> banned{}; // 禁用地形 下标为地形编号 true 表示禁用
    std::uint32_t seed = 0; // 随机种子 0 表示每次运行随机
    double cityProbability = 0.06; // 城市出现概率
    double mountainProbability = 0.15; // 山区出现概率
    double swampProbability = 0.03; // 沼泽出现概率
    int cityUnitsMin = 30; // 中立城市初始最小兵力
    int cityUnitsMax = 55; // 中立城市初始最大兵力
};

// 对局配置
struct GameConfig {
    GameMode mode = GameMode::Duel; // 游戏模式
    int maxTurns = 1500; // 最大回合数
    int gamesPerMatch = 1; // 一场比赛包含多少局
    int ffaPlayerCount = 4; // FFA 模式玩家数
    int initTimeLimitMs = 2000; // 初始化时限 毫秒
    int turnTimeLimitMs = 50; // 每回合时限 毫秒
    int maxViolations = 5; // 连续违规上限 达到后判负 0 表示不判负
};

// 比赛配置
struct MatchConfig {
    bool autoMatch = false; // 是否开启自动匹配
    std::vector<std::vector<std::string>> teams; // 显式队伍配置 每个元素是一支队伍 内容为玩家 id
    double playerWeight = 1.0; // 玩家分数权重
    double teamWeight = 0.0; // 队伍分数权重
};

// 界面配置
struct UiConfig {
    bool enabled = true; // 是否显示界面
    int cellSize = 36; // 每个格子的边长 像素
    int turnDelayMs = 30; // 每回合画面停留时间 毫秒
    int resultDelayMs = 1200; // 终局画面停留时间 毫秒
};

// 日志配置
struct LogConfig {
    std::string directory = "logs"; // 日志目录
    bool recordMoves = true; // 是否记录玩家操作
};

// 全部配置
struct Config {
    MapConfig map;
    GameConfig game;
    MatchConfig match;
    UiConfig ui;
    LogConfig log;

    // 从文件读取配置 缺失或非法时抛出 ConfigError
    static Config load(const std::filesystem::path &filePath);

    // 校验配置取值 非法时抛出 ConfigError
    void validate() const;

    // 返回当前模式需要的玩家数 1v1 为 2 2v2 为 4 FFA 为 ffaPlayerCount
    int requiredPlayerCount() const {
        switch(game.mode) {
            case GameMode::Duel: return 2;
            case GameMode::Team: return 4;
            case GameMode::FFA: return game.ffaPlayerCount;
            default: return 2;
        }
    }
};

// 返回配置的简要说明 用于写入日志
std::string describeConfig(const Config &config);

} // namespace gen::core
