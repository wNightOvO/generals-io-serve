/*
    游戏规则的实现
    负责校验移动 执行移动 处理兵力生成与消失 处理玩家淘汰
*/

#include <core/rule.hpp>
#include <include/util.hpp>
#include <algorithm>
#include <cassert>
#include <vector>

namespace gen::core {

namespace { // namespace

// 调试用 校验玩家与队伍的统计值是否和地图完全一致
void checkStats(const GameMap<Terrain> &map, const PlayerRegistry &registry) {
#ifndef NDEBUG
    const int playerCount = registry.playerCount();
    std::vector<int> units(playerCount, 0); // 地图上每名玩家的兵力总数
    std::vector<int> tiles(playerCount, 0); // 地图上每名玩家的格子总数
    for(const Tile<Terrain> &tile : map.tiles()) {
        if(tile.owner >= 0) {
            assert(tile.owner < playerCount); // 不能出现未知玩家
            units[tile.owner] += tile.unit;
            ++ tiles[tile.owner];
        }
        else {
            assert(tile.owner == NEUTRAL); // 实际地图只有中立和玩家两种归属
        }
    }
    for(int playerID = 0; playerID < playerCount; ++ playerID) {
        const Player *player = registry.atPlayer(playerID);
        if(player->alive) {
            assert(player->units == units[playerID]);
            assert(player->tiles == tiles[playerID]);
        }
        else {
            assert(player->units == 0);
            assert(player->tiles == 0);
            assert(units[playerID] == 0);
        }
    }
    for(int teamID = 0; teamID < registry.teamCount(); ++ teamID) {
        const Team *team = registry.atTeam(teamID);
        int teamUnits = 0; // 队伍兵力总数
        int teamTiles = 0; // 队伍格子总数
        int alivePlayers = 0; // 队伍存活玩家数
        for(int playerID : team->players) {
            teamUnits += units[playerID];
            teamTiles += tiles[playerID];
            if(registry.atPlayer(playerID)->alive) ++ alivePlayers;
        }
        assert(team->units == teamUnits);
        assert(team->tiles == teamTiles);
        assert(team->alivePlayers == alivePlayers);
        assert(team->alive == (alivePlayers > 0));
    }
#else
    (void) map;
    (void) registry;
#endif
}

} // namespace

// 计算一次移动实际移动的兵力 一半移动时源格保留 floor(u / 2)
int Rule::moveUnits(int sourceUnits, bool half) {
    if(sourceUnits <= 0) return 0;
    if(half) return sourceUnits - sourceUnits / 2;
    return sourceUnits - 1;
}

// 判断移动是否合法
bool Rule::isValidMove(const GameMap<Terrain> &map, int playerID, const MoveOperation &op) {
    if(playerID < 0) return false;
    if(map.isInside(op.source) == false) return false; // 源格在地图内
    if(map.isInside(op.dest) == false) return false; // 目标格在地图内
    if(manhattanDistance(op.source, op.dest) != 1) return false; // 目标格与源格相邻
    const Tile<Terrain> &sourceTile = map.at(op.source);
    const Tile<Terrain> &destTile = map.at(op.dest);
    if(sourceTile.owner != playerID) return false; // 源格属于该玩家
    if(sourceTile.unit < 2) return false; // 源格兵力大于等于 2
    if(destTile.terrain == Terrain::Mountain) return false; // 目标格不是山区
    return true;
}

// 执行一次移动 自动校验 非法时不做任何修改
MoveEffect Rule::applyMove(GameMap<Terrain> &map, PlayerRegistry &registry, const MoveOperation &op, int turn) {
    MoveEffect effect;
    if(isValidMove(map, op.playerID, op) == false) return effect;
    const int sourceID = op.playerID;
    Tile<Terrain> &sourceTile = map.at(op.source);
    Tile<Terrain> &destTile = map.at(op.dest);
    const int move = moveUnits(sourceTile.unit, op.half);
    effect.movedUnits = move;
    // 先减少源格兵力并更新统计
    sourceTile.unit -= move;
    registry.changeStat(sourceID, -move, 0);
    if(registry.sameTeam(sourceID, destTile.owner)) {
        // 目标为己方或队友 兵力相加 归属不变
        destTile.unit += move;
        registry.changeStat(destTile.owner, move, 0);
    }
    else if(move <= destTile.unit) {
        // 打不过 目标兵力减少 归属不变
        destTile.unit -= move;
        if(destTile.isNeutral() == false) {
            registry.changeStat(destTile.owner, -move, 0);
        }
    }
    else {
        // 打得过 占领目标格
        const int oldOwner = destTile.owner;
        const bool wasCapital = destTile.terrain == Terrain::Capital;
        const int remainUnits = move - destTile.unit;
        if(oldOwner != NEUTRAL) {
            // 目标原本属于其他玩家 先移除该格子的统计
            registry.changeStat(oldOwner, -destTile.unit, -1);
        }
        destTile.owner = sourceID;
        destTile.unit = remainUnits;
        registry.changeStat(sourceID, remainUnits, 1);
        if(wasCapital && oldOwner != NEUTRAL) {
            // 占领了敌方首都 被占领首都变为城市 该玩家淘汰
            effect.capturedCapital = true;
            effect.defeatedPlayer = oldOwner;
            destTile.terrain = Terrain::City;
            int sumUnits = 0; // 吞并得到的兵力总数
            int sumTiles = 0; // 吞并得到的格子总数
            for(Tile<Terrain> &tile : map.tiles()) {
                if(tile.owner == oldOwner) {
                    // 非首都格兵力向上取整减半
                    tile.unit = (tile.unit + 1) / 2;
                    tile.owner = sourceID;
                    sumUnits += tile.unit;
                    ++ sumTiles;
                }
            }
            if(sumTiles > 0) {
                registry.changeStat(sourceID, sumUnits, sumTiles);
            }
            registry.killPlayer(oldOwner, turn); // 记录死亡 清空统计
        }
    }
    effect.applied = true;
    checkStats(map, registry);
    return effect;
}

// 执行回合结束后的兵力生成与消失
void Rule::applyEconomy(GameMap<Terrain> &map, PlayerRegistry &registry, int turn) {
    const int playerCount = registry.playerCount();
    std::vector<int> deltaUnits(playerCount, 0); // 每名玩家的兵力变化
    for(Tile<Terrain> &tile : map.tiles()) {
        if(tile.owner < 0 || tile.owner >= playerCount) continue; // 中立格子不产生也不消失
        int delta = 0;
        switch(tile.terrain) {
            case Terrain::Blank:
                if(turn % 50 == 0) delta = 1; // 空地在 50 的倍数回合加 1
                break;
            case Terrain::City:
            case Terrain::Capital:
                if(turn % 2 == 0) delta = 1; // 城市与首都在偶数回合加 1
                break;
            case Terrain::Swamp:
                if(tile.unit > 1) delta = -1; // 沼泽兵力大于 1 时减 1
                break;
            case Terrain::Mountain:
            default:
                break;
        }
        tile.unit += delta;
        deltaUnits[tile.owner] += delta;
    }
    for(int playerID = 0; playerID < playerCount; ++ playerID) {
        if(deltaUnits[playerID] != 0) {
            registry.changeStat(playerID, deltaUnits[playerID], 0);
        }
    }
    checkStats(map, registry);
}

// 让玩家判负并移出对局 全部格子变为中立且兵力清零
void Rule::eliminatePlayer(GameMap<Terrain> &map, PlayerRegistry &registry, int playerID, int turn) {
    if(playerID < 0 || playerID >= registry.playerCount()) return;
    const Player *player = registry.atPlayer(playerID);
    if(player == nullptr || player->alive == false) return; // 玩家不存在或已经死亡
    for(Tile<Terrain> &tile : map.tiles()) {
        if(tile.owner == playerID) {
            tile.owner = NEUTRAL;
            tile.unit = 0;
        }
    }
    registry.killPlayer(playerID, turn); // 记录死亡 清空统计
    checkStats(map, registry);
}

} // namespace gen::core
