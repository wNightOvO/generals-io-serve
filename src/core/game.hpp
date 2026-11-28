/*
    游戏主体逻辑
    维护一局游戏的全部状态 提供回合推进 视野 排名接口
*/

#pragma once
#include <include/map.hpp>
#include <include/move_operation.hpp>
#include <include/type.hpp>
#include <core/player.hpp>
#include <functional>
#include <string>
#include <vector>

namespace gen::core {

// 一局游戏 回合数从 0 开始 第一次 nextTurn 后变为 1
class Game {
private:
    int maxTurns_ = 1500; // 最大回合数
    int currentTurn_ = 0; // 当前回合数
    bool over_ = false; // 游戏是否结束
    bool victory_ = false; // 是否因胜利条件结束 否则为到达最大回合数
    GameMap<Terrain> map_; // 当前地图
    PlayerRegistry registry_; // 玩家注册表
    std::vector<Rank> finalPlayerRank_; // 最终玩家排名
    std::vector<Rank> finalTeamRank_; // 最终队伍排名

    // 由当前地图重新统计玩家和队伍的兵力与格子数
    void recountStats();

    // 计算最终排名 并在结束时调用
    void buildFinalRank();

    // 计算一名玩家当前排名用的排序键 存活优先 兵力多优先 玩家编号小优先
    std::vector<int> sortedPlayerIDs(bool aliveFirst) const;

    // 计算一支队伍当前排名用的排序键 存活优先 兵力多优先 队伍编号小优先
    std::vector<int> sortedTeamIDs(bool aliveFirst) const;
public:
    // 构造一局游戏 参数 rows cols 为地图大小 参数 maxTurns 为最大回合数
    Game(int rows, int cols, int maxTurns);

    // 注册一名玩家 必须在 initialize 前调用
    void registerPlayer(int playerID, int teamID, const std::string &name);

    // 初始化游戏 参数 mapGenerator 负责生成地图并放置首都 调用后统计初始状态 构造移动优先级
    void initialize(std::function<void(GameMap<Terrain>&)> mapGenerator);

    // 返回行数
    int rows() const {
        return map_.R;
    }

    // 返回列数
    int cols() const {
        return map_.C;
    }

    // 返回最大回合数
    int maxTurns() const {
        return maxTurns_;
    }

    // 返回当前回合数
    int currentTurn() const {
        return currentTurn_;
    }

    // 游戏是否结束
    bool isOver() const {
        return over_;
    }

    // 是否因胜利条件结束 参数未结束或无胜负时为 false
    bool finishedByVictory() const {
        return victory_;
    }

    // 推进一回合 参数 moves 为本回合全部玩家的移动 返回游戏是否结束
    bool nextTurn(const std::vector<MoveOperation> &moves);

    // 判负指定玩家 返回游戏是否结束 参数 playerID 为玩家编号
    bool forfeitPlayer(int playerID);

    // 返回地图
    GameMap<Terrain>& map() {
        return map_;
    }

    // 返回地图 常量版本
    const GameMap<Terrain>& map() const {
        return map_;
    }

    // 返回玩家注册表
    PlayerRegistry& registry() {
        return registry_;
    }

    // 返回玩家注册表 常量版本
    const PlayerRegistry& registry() const {
        return registry_;
    }

    // 返回指定队伍的视野 参数 teamID 为队伍编号
    GameMap<VisibleTerrain> teamVision(int teamID) const;

    // 返回当前队伍排名 覆盖全部队伍 从高到低 已结束游戏时返回最终队伍排名
    std::vector<Rank> teamLeaderboard() const;

    // 返回当前玩家排名 覆盖全部玩家 从高到低 已结束游戏时返回最终玩家排名
    std::vector<Rank> playerLeaderboard() const;

    // 返回最终玩家排名 仅在游戏结束后有效
    const std::vector<Rank>& finalPlayerRank() const {
        return finalPlayerRank_;
    }

    // 返回最终队伍排名 仅在游戏结束后有效
    const std::vector<Rank>& finalTeamRank() const {
        return finalTeamRank_;
    }
};

} // namespace gen::core
