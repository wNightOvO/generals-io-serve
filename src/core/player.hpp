/*
    玩家 队伍 和玩家注册表
*/

#pragma once
#include <include/type.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace gen::core {

// 玩家 编号从 0 开始且等于在注册表中的下标
struct Player {
    int playerID = 0; // 玩家编号
    int teamID = 0; // 队伍编号
    std::string name; // 玩家 id 即文件夹名
    bool alive = true; // 是否存活
    int units = 0; // 当前占领格子上的兵力总数
    int tiles = 0; // 当前占领格子数
    int deathTurn = -1; // 死亡回合 -1 表示存活
    int deathUnits = 0; // 死亡时的兵力总数 用于最终排名
};

// 队伍 编号从 0 开始且等于在注册表中的下标
struct Team {
    int teamID = 0; // 队伍编号
    std::vector<int> players; // 队伍中的玩家编号
    bool alive = true; // 队伍是否存活
    int alivePlayers = 0; // 存活玩家数
    int units = 0; // 队伍总兵力
    int tiles = 0; // 队伍总占领格子数
    int deathTurn = -1; // 队伍死亡回合 -1 表示存活
};

// 玩家注册表 负责维护玩家 队伍 移动优先级
class PlayerRegistry {
private:
    std::vector<Player> players_; // 玩家列表 下标即玩家编号
    std::vector<Team> teams_; // 队伍列表 下标即队伍编号
    std::vector<int> moveOrder_; // 当前移动优先级 从高到低
    int alivePlayerCount_ = 0; // 存活玩家数
    int aliveTeamCount_ = 0; // 存活队伍数
public:
    PlayerRegistry() = default;

    // 注册一名玩家 参数 playerID 必须等于当前玩家数 参数 teamID 从 0 递增 参数 name 为玩家 id
    void registerPlayer(int playerID, int teamID, const std::string &name);

    // 注册完全部玩家后调用 构造随机的初始移动优先级
    void finalize();

    // 返回玩家数量
    int playerCount() const {
        return static_cast<int>(players_.size());
    }

    // 返回队伍数量
    int teamCount() const {
        return static_cast<int>(teams_.size());
    }

    // 返回玩家 参数 playerID 为玩家编号 越界是程序错误
    Player* atPlayer(int playerID);

    // 返回玩家 常量版本
    const Player* atPlayer(int playerID) const;

    // 返回队伍 参数 teamID 为队伍编号 越界是程序错误
    Team* atTeam(int teamID);

    // 返回队伍 常量版本
    const Team* atTeam(int teamID) const;

    // 返回全部玩家 下标即玩家编号
    std::vector<Player>& players() {
        return players_;
    }

    // 返回全部玩家 常量版本
    const std::vector<Player>& players() const {
        return players_;
    }

    // 返回全部队伍 下标即队伍编号
    std::vector<Team>& teams() {
        return teams_;
    }

    // 返回全部队伍 常量版本
    const std::vector<Team>& teams() const {
        return teams_;
    }

    // 返回当前移动优先级 从高到低
    const std::vector<int>& moveOrder() const {
        return moveOrder_;
    }

    // 把移动优先级轮转一位 当前最高优先级玩家变为最低
    void rotateMoveOrder();

    // 判断两名玩家是否属于同一队伍 参数含 NEUTRAL 时返回 false
    bool sameTeam(int playerID1, int playerID2) const;

    // 修改玩家和队伍的统计值 参数 deltaUnits 为兵力变化 参数 deltaTiles 为格子数变化 可以为负
    void changeStat(int playerID, int deltaUnits, int deltaTiles);

    // 清空全部玩家的统计值 用于初始化后重新统计
    void clearStats();

    // 标记玩家死亡 参数 turn 为死亡回合 清空该玩家的统计值并更新队伍状态
    void killPlayer(int playerID, int turn);

    // 返回存活玩家数
    int alivePlayerCount() const {
        return alivePlayerCount_;
    }

    // 返回存活队伍数
    int aliveTeamCount() const {
        return aliveTeamCount_;
    }
};

} // namespace gen::core
