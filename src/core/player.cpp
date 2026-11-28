/*
    玩家 队伍 和玩家注册表的实现
*/

#include <core/player.hpp>
#include <include/random.hpp>
#include <algorithm>
#include <cassert>

namespace gen::core {

// 注册一名玩家 玩家编号必须等于当前玩家数 队伍编号必须连续分配
void PlayerRegistry::registerPlayer(int playerID, int teamID, const std::string &name) {
    assert(playerID == playerCount()); // 玩家编号必须等于当前玩家数
    assert(teamID >= 0 && teamID <= teamCount()); // 队伍编号从 0 开始连续分配
    Player player;
    player.playerID = playerID;
    player.teamID = teamID;
    player.name = name;
    player.alive = true;
    player.units = 0;
    player.tiles = 0;
    player.deathTurn = -1;
    player.deathUnits = 0;
    players_.push_back(player);
    ++ alivePlayerCount_;
    if(teamID == teamCount()) {
        // 新队伍 编号等于当前队伍数
        Team team;
        team.teamID = teamID;
        team.players.push_back(playerID);
        team.alive = true;
        team.alivePlayers = 1;
        team.units = 0;
        team.tiles = 0;
        team.deathTurn = -1;
        teams_.push_back(team);
        ++ aliveTeamCount_;
    }
    else {
        // 已有队伍
        Team &team = teams_[teamID];
        team.players.push_back(playerID);
        ++ team.alivePlayers;
    }
}

// 注册完全部玩家后调用 构造随机的初始移动优先级
void PlayerRegistry::finalize() {
    moveOrder_.clear();
    for(int playerID = 0; playerID < playerCount(); ++ playerID) {
        moveOrder_.push_back(playerID);
    }
    if(moveOrder_.size() > 1) {
        randomShuffle(moveOrder_.begin(), moveOrder_.end());
    }
}

Player* PlayerRegistry::atPlayer(int playerID) {
    assert(playerID >= 0 && playerID < playerCount()); // 越界是程序错误
    if(playerID < 0 || playerID >= playerCount()) return nullptr;
    return &players_[playerID];
}

const Player* PlayerRegistry::atPlayer(int playerID) const {
    assert(playerID >= 0 && playerID < playerCount()); // 越界是程序错误
    if(playerID < 0 || playerID >= playerCount()) return nullptr;
    return &players_[playerID];
}

Team* PlayerRegistry::atTeam(int teamID) {
    assert(teamID >= 0 && teamID < teamCount()); // 越界是程序错误
    if(teamID < 0 || teamID >= teamCount()) return nullptr;
    return &teams_[teamID];
}

const Team* PlayerRegistry::atTeam(int teamID) const {
    assert(teamID >= 0 && teamID < teamCount()); // 越界是程序错误
    if(teamID < 0 || teamID >= teamCount()) return nullptr;
    return &teams_[teamID];
}

// 把移动优先级轮转一位 当前最高优先级玩家变为最低
void PlayerRegistry::rotateMoveOrder() {
    if(moveOrder_.size() > 1) {
        std::rotate(moveOrder_.begin(), moveOrder_.begin() + 1, moveOrder_.end());
    }
}

// 判断两名玩家是否属于同一队伍 中立与未知不属于任何队伍
bool PlayerRegistry::sameTeam(int playerID1, int playerID2) const {
    if(playerID1 < 0 || playerID2 < 0) return false;
    if(playerID1 >= playerCount() || playerID2 >= playerCount()) return false;
    return players_[playerID1].teamID == players_[playerID2].teamID;
}

// 修改玩家和队伍的统计值
void PlayerRegistry::changeStat(int playerID, int deltaUnits, int deltaTiles) {
    if(playerID < 0 || playerID >= playerCount()) return;
    Player &player = players_[playerID];
    if(player.alive == false) return; // 已淘汰玩家不再累计统计
    player.units += deltaUnits;
    player.tiles += deltaTiles;
    if(player.teamID >= 0 && player.teamID < teamCount()) {
        Team &team = teams_[player.teamID];
        team.units += deltaUnits;
        team.tiles += deltaTiles;
    }
}

// 清空全部玩家的统计值 用于初始化后重新统计
void PlayerRegistry::clearStats() {
    for(Player &player : players_) {
        player.units = 0;
        player.tiles = 0;
    }
    for(Team &team : teams_) {
        team.units = 0;
        team.tiles = 0;
    }
}

// 标记玩家死亡 记录死亡回合和死亡时兵力 清空统计并更新队伍状态
void PlayerRegistry::killPlayer(int playerID, int turn) {
    if(playerID < 0 || playerID >= playerCount()) return;
    Player &player = players_[playerID];
    if(player.alive == false) return; // 已经死亡
    player.alive = false;
    player.deathTurn = turn;
    player.deathUnits = player.units; // 记录死亡时兵力
    Team &team = teams_[player.teamID];
    team.units -= player.units;
    team.tiles -= player.tiles;
    player.units = 0;
    player.tiles = 0;
    if(alivePlayerCount_ > 0) -- alivePlayerCount_;
    if(team.alivePlayers > 0) -- team.alivePlayers;
    if(team.alivePlayers == 0) {
        // 队伍无人存活 记录队伍死亡回合
        team.alive = false;
        team.deathTurn = turn;
        if(aliveTeamCount_ > 0) -- aliveTeamCount_;
    }
}

} // namespace gen::core
