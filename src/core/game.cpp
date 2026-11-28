/*
    游戏主体逻辑的实现
    维护一局游戏的全部状态 提供回合推进 视野 排名接口
*/

#include <core/game.hpp>
#include <core/rule.hpp>
#include <include/util.hpp>
#include <algorithm>
#include <cassert>
#include <vector>

namespace gen::core {

// 构造一局游戏 地图尺寸先确定 生成器在 initialize 中填充
Game::Game(int rows, int cols, int maxTurns) {
    maxTurns_ = maxTurns;
    currentTurn_ = 0;
    over_ = false;
    victory_ = false;
    map_.resize(rows, cols);
}

// 注册一名玩家 必须在 initialize 前调用
void Game::registerPlayer(int playerID, int teamID, const std::string &name) {
    registry_.registerPlayer(playerID, teamID, name);
}

// 初始化游戏 生成地图 统计初始状态 构造移动优先级
void Game::initialize(std::function<void(GameMap<Terrain>&)> mapGenerator) {
    currentTurn_ = 0;
    over_ = false;
    victory_ = false;
    finalPlayerRank_.clear();
    finalTeamRank_.clear();
    mapGenerator(map_);
    recountStats();
    registry_.finalize();
}

// 由当前地图重新统计玩家和队伍的兵力与格子数
void Game::recountStats() {
    registry_.clearStats();
    const int playerCount = registry_.playerCount();
    for(const Tile<Terrain> &tile : map_.tiles()) {
        if(tile.owner >= 0 && tile.owner < playerCount) {
            registry_.changeStat(tile.owner, tile.unit, 1);
        }
        else {
            assert(tile.owner == NEUTRAL); // 实际地图只有中立和玩家两种归属
        }
    }
}

// 返回按名次排列的队伍编号 存活优先 兵力多优先 格子多优先 编号小优先
std::vector<int> Game::sortedTeamIDs(bool aliveFirst) const {
    std::vector<int> teamIDs;
    for(int teamID = 0; teamID < registry_.teamCount(); ++ teamID) {
        teamIDs.push_back(teamID);
    }
    std::sort(teamIDs.begin(), teamIDs.end(), [&](int left, int right) {
        const Team *a = registry_.atTeam(left);
        const Team *b = registry_.atTeam(right);
        if(a->alive != b->alive) {
            // 存活队伍在前 或按调用者要求反过来
            return aliveFirst ? (a->alive && !b->alive) : (!a->alive && b->alive);
        }
        if(a->alive) {
            if(a->units != b->units) return a->units > b->units; // 兵力降序
            if(a->tiles != b->tiles) return a->tiles > b->tiles; // 格子数降序
            return left < right; // 队伍编号升序
        }
        if(a->deathTurn != b->deathTurn) return a->deathTurn > b->deathTurn; // 死亡回合降序
        return left < right; // 队伍编号升序
    });
    return teamIDs;
}

// 返回按名次排列的玩家编号 存活优先 兵力多优先 格子多优先 编号小优先
std::vector<int> Game::sortedPlayerIDs(bool aliveFirst) const {
    std::vector<int> playerIDs;
    for(int playerID = 0; playerID < registry_.playerCount(); ++ playerID) {
        playerIDs.push_back(playerID);
    }
    std::sort(playerIDs.begin(), playerIDs.end(), [&](int left, int right) {
        const Player *a = registry_.atPlayer(left);
        const Player *b = registry_.atPlayer(right);
        if(a->alive != b->alive) {
            // 存活玩家在前 或按调用者要求反过来
            return aliveFirst ? (a->alive && !b->alive) : (!a->alive && b->alive);
        }
        if(a->alive) {
            if(a->units != b->units) return a->units > b->units; // 兵力降序
            if(a->tiles != b->tiles) return a->tiles > b->tiles; // 格子数降序
            return left < right; // 玩家编号升序
        }
        if(a->deathTurn != b->deathTurn) return a->deathTurn > b->deathTurn; // 死亡回合降序
        if(a->deathUnits != b->deathUnits) return a->deathUnits > b->deathUnits; // 死亡时兵力降序
        return left < right; // 玩家编号升序
    });
    return playerIDs;
}

// 计算最终排名 并在游戏结束时调用
void Game::buildFinalRank() {
    finalPlayerRank_.clear();
    finalTeamRank_.clear();
    // 队伍最终排名 获胜队伍 或存活队伍按兵力 格子数降序 已死亡队伍按死亡回合降序
    std::vector<int> teamOrder = sortedTeamIDs(true);
    for(int index = 0; index < static_cast<int>(teamOrder.size()); ++ index) {
        finalTeamRank_.push_back(Rank{teamOrder[index], index + 1});
    }
    std::vector<int> teamFinalRank(registry_.teamCount(), 0); // 队伍编号到最终名次的映射
    for(int index = 0; index < static_cast<int>(teamOrder.size()); ++ index) {
        teamFinalRank[teamOrder[index]] = index + 1;
    }
    // 已死亡玩家共用的排序规则 死亡回合降序 死亡时兵力降序 玩家编号升序
    auto deathLess = [&](int left, int right) {
        const Player *a = registry_.atPlayer(left);
        const Player *b = registry_.atPlayer(right);
        if(a->deathTurn != b->deathTurn) return a->deathTurn > b->deathTurn;
        if(a->deathUnits != b->deathUnits) return a->deathUnits > b->deathUnits;
        return left < right;
    };
    std::vector<int> playerOrder;
    if(victory_) {
        // 因胜利结束 获胜队伍玩家占据最前 队内按最终兵力降序 玩家编号升序
        int winnerTeam = -1; // 唯一存活队伍的编号
        for(int teamID = 0; teamID < registry_.teamCount(); ++ teamID) {
            const Team *team = registry_.atTeam(teamID);
            if(team->alive) {
                winnerTeam = teamID;
                break;
            }
        }
        if(winnerTeam >= 0) {
            std::vector<int> winners = registry_.atTeam(winnerTeam)->players;
            std::sort(winners.begin(), winners.end(), [&](int left, int right) {
                const Player *a = registry_.atPlayer(left);
                const Player *b = registry_.atPlayer(right);
                if(a->units != b->units) return a->units > b->units; // 最终兵力降序
                return left < right; // 玩家编号升序
            });
            for(int playerID : winners) playerOrder.push_back(playerID);
        }
        // 其余玩家按死亡回合降序 死亡时兵力降序 玩家编号升序
        std::vector<int> others;
        for(int playerID = 0; playerID < registry_.playerCount(); ++ playerID) {
            if(winnerTeam >= 0 && registry_.atPlayer(playerID)->teamID == winnerTeam) continue;
            others.push_back(playerID);
        }
        std::sort(others.begin(), others.end(), deathLess);
        for(int playerID : others) playerOrder.push_back(playerID);
    }
    else {
        // 到达最大回合数结束 存活玩家先按队伍排名 再按兵力降序 玩家编号升序
        std::vector<int> alivePlayers;
        std::vector<int> deadPlayers;
        for(int playerID = 0; playerID < registry_.playerCount(); ++ playerID) {
            if(registry_.atPlayer(playerID)->alive) alivePlayers.push_back(playerID);
            else deadPlayers.push_back(playerID);
        }
        std::sort(alivePlayers.begin(), alivePlayers.end(), [&](int left, int right) {
            int teamLeft = registry_.atPlayer(left)->teamID;
            int teamRight = registry_.atPlayer(right)->teamID;
            if(teamFinalRank[teamLeft] != teamFinalRank[teamRight]) {
                return teamFinalRank[teamLeft] < teamFinalRank[teamRight]; // 队伍排名靠前
            }
            const Player *a = registry_.atPlayer(left);
            const Player *b = registry_.atPlayer(right);
            if(a->units != b->units) return a->units > b->units; // 玩家兵力降序
            return left < right; // 玩家编号升序
        });
        std::sort(deadPlayers.begin(), deadPlayers.end(), deathLess);
        for(int playerID : alivePlayers) playerOrder.push_back(playerID);
        for(int playerID : deadPlayers) playerOrder.push_back(playerID);
    }
    for(int index = 0; index < static_cast<int>(playerOrder.size()); ++ index) {
        finalPlayerRank_.push_back(Rank{playerOrder[index], index + 1});
    }
}

// 推进一回合 参数 moves 为本回合全部玩家的移动 返回游戏是否结束
bool Game::nextTurn(const std::vector<MoveOperation> &moves) {
    if(over_) return true;
    ++ currentTurn_;
    // 按当前移动优先级依次执行移动 按玩家编号匹配 找不到对应移动视为跳过
    for(int playerID : registry_.moveOrder()) {
        const Player *player = registry_.atPlayer(playerID);
        if(player == nullptr || player->alive == false) continue;
        const MoveOperation *move = nullptr;
        for(const MoveOperation &candidate : moves) {
            if(candidate.playerID == playerID) {
                move = &candidate;
                break;
            }
        }
        if(move == nullptr) continue;
        Rule::applyMove(map_, registry_, *move, currentTurn_);
    }
    // 处理兵力生成与消失
    Rule::applyEconomy(map_, registry_, currentTurn_);
    // 判断胜负 剩余队伍数为 1 时结束 为 0 时也要正常结束
    if(registry_.aliveTeamCount() <= 1) {
        over_ = true;
        victory_ = registry_.aliveTeamCount() == 1;
        buildFinalRank();
        return true;
    }
    // 轮转移动优先级
    registry_.rotateMoveOrder();
    // 判断是否到达最大回合数
    if(currentTurn_ >= maxTurns_) {
        over_ = true;
        victory_ = false;
        buildFinalRank();
        return true;
    }
    return false;
}

// 判负指定玩家 返回游戏是否结束
bool Game::forfeitPlayer(int playerID) {
    if(playerID < 0 || playerID >= registry_.playerCount()) return over_;
    const Player *player = registry_.atPlayer(playerID);
    if(player == nullptr || player->alive == false) return over_; // 已经死亡的不重复判负
    // 死亡回合记为正在进行的这一回合
    Rule::eliminatePlayer(map_, registry_, playerID, currentTurn_ + 1);
    const int aliveTeams = registry_.aliveTeamCount();
    if(aliveTeams <= 1) {
        // 存活队伍数降到 1 或 0 都正常结束
        over_ = true;
        victory_ = aliveTeams == 1;
        buildFinalRank();
        return true;
    }
    if(over_) {
        // 已经结束的局继续判负玩家时刷新最终排名 不改变原有结束原因
        buildFinalRank();
        return true;
    }
    return false;
}

// 返回当前队伍排名 已结束游戏时返回最终队伍排名
std::vector<Rank> Game::teamLeaderboard() const {
    if(over_) return finalTeamRank_;
    std::vector<int> order = sortedTeamIDs(true);
    std::vector<Rank> result;
    result.reserve(order.size());
    for(int index = 0; index < static_cast<int>(order.size()); ++ index) {
        result.push_back(Rank{order[index], index + 1});
    }
    return result;
}

// 返回当前玩家排名 已结束游戏时返回最终玩家排名
std::vector<Rank> Game::playerLeaderboard() const {
    if(over_) return finalPlayerRank_;
    std::vector<int> order = sortedPlayerIDs(true);
    std::vector<Rank> result;
    result.reserve(order.size());
    for(int index = 0; index < static_cast<int>(order.size()); ++ index) {
        result.push_back(Rank{order[index], index + 1});
    }
    return result;
}

// 返回指定队伍的视野
GameMap<VisibleTerrain> Game::teamVision(int teamID) const {
    const int R = map_.R;
    const int C = map_.C;
    const int playerCount = registry_.playerCount();
    GameMap<VisibleTerrain> vision(R, C);
    std::vector<char> inTeam(playerCount, 0); // 玩家是否属于该队伍
    bool hasTeam = false;
    if(teamID >= 0 && teamID < registry_.teamCount()) {
        const Team *team = registry_.atTeam(teamID);
        for(int playerID : team->players) {
            if(playerID >= 0 && playerID < playerCount) {
                inTeam[playerID] = 1;
                hasTeam = true;
            }
        }
    }
    std::vector<char> visible(static_cast<std::size_t>(R) * C, 0); // 格子是否在视野内
    if(hasTeam) {
        // 先确定该队伍全部玩家占领的格子 再把周围一圈标记为可见
        for(int x = 0; x < R; ++ x) {
            for(int y = 0; y < C; ++ y) {
                const Tile<Terrain> &tile = map_.at(x, y);
                if(tile.owner < 0 || tile.owner >= playerCount || inTeam[tile.owner] == 0) continue;
                for(int dx = -1; dx <= 1; ++ dx) {
                    for(int dy = -1; dy <= 1; ++ dy) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if(map_.isInside(nx, ny) == false) continue;
                        visible[static_cast<std::size_t>(nx) * C + ny] = 1;
                    }
                }
            }
        }
    }
    for(int x = 0; x < R; ++ x) {
        for(int y = 0; y < C; ++ y) {
            const Tile<Terrain> &source = map_.at(x, y);
            Tile<VisibleTerrain> &target = vision.at(x, y);
            if(visible[static_cast<std::size_t>(x) * C + y]) {
                // 可见格发送真实地形 归属和兵力
                target.terrain = toVisibleTerrain(source.terrain);
                target.owner = source.owner;
                target.unit = source.unit;
            }
            else {
                // 不可见格发送疑似地形 归属未知 兵力为 0
                target.terrain = toFogTerrain(source.terrain);
                target.owner = UNKNOWN;
                target.unit = 0;
            }
        }
    }
    return vision;
}

} // namespace gen::core
