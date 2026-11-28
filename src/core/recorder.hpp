/*
    玩家操作记录
    把每一回合的玩家动作和地图变化写成人可以直接阅读的文本
*/

#pragma once
#include <include/map.hpp>
#include <include/type.hpp>
#include <core/player.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace gen::core {

// 一名玩家一回合的动作
struct ActionRecord {
    int playerID = -1; // 玩家编号
    bool hasMove = false; // 是否有实际移动
    bool pass = false; // 是否主动放弃移动
    Point source; // 源格
    Point dest; // 目标格
    bool half = false; // 是否一半移动
    std::string note; // 附加说明 例如 超时 非法 崩溃
};

// 玩家操作记录器 每个对局写一个文本文件
class MoveRecorder {
private:
    std::ofstream out_; // 输出文件
    bool enabled_ = false; // 是否启用
    std::filesystem::path path_; // 文件路径

    // 返回格子的简短描述 用于变化记录
    std::string tileText(const Tile<Terrain> &tile) const;
public:
    MoveRecorder() = default;

    ~MoveRecorder();

    // 打开记录文件 参数 filePath 为文件路径 参数 enabled 为 false 时不写文件
    bool open(const std::filesystem::path &filePath, bool enabled);

    // 开始一局记录 参数 modeName 为模式名 参数 mapText 为地图说明 参数 registry 为玩家注册表
    void beginGame(int matchNumber, int gameNumber, const std::string &modeName,
                   int rows, int cols, std::uint32_t seed, const PlayerRegistry &registry);

    // 记录一回合 参数 turn 为回合 参数 actions 为玩家动作 参数 before 修改前地图 参数 after 修改后地图
    void recordTurn(int turn, const std::vector<ActionRecord> &actions,
                    const GameMap<Terrain> &before, const GameMap<Terrain> &after,
                    const PlayerRegistry &registry);

    // 记录一条事件 例如 玩家死亡 到达最大回合
    void recordEvent(const std::string &text);

    // 结束一局记录 参数 playerRank 为玩家排名 参数 teamRank 为队伍排名 参数 byVictory 是否因胜利结束
    void endGame(const std::vector<Rank> &playerRank, const std::vector<Rank> &teamRank, bool byVictory);

    // 关闭文件
    void close();

    // 是否处于打开状态
    bool isOpen() const {
        return out_.is_open();
    }
};

} // namespace gen::core
