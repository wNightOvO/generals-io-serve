#include <core/game.hpp>
#include <include/util.hpp>
#include <include/logger.hpp>
#include <format>

namespace gen::core {

using std::vector;
using std::queue;
using std::format;
using std::ostringstream;
using std::string;

void Game::treatTurn(const vector<MoveOperation> &movement) {
    queue<Player*> moveQueue = playerRegistry.getMoveQueue();
    playerRegistry.nextMoveQueue();
    while(!moveQueue.empty()) { // 处理移动操作
        const Player *p = moveQueue.front();
        moveQueue.pop();
        const MoveOperation *thisMovement = nullptr;
        for(const auto &i : movement) {
            if(i.playerID == p->playerID) {
                thisMovement = &i;
                break;
            }
        }
        if(thisMovement == nullptr) continue;
        Rule::applyMove(map, playerRegistry, *thisMovement);
    }
    // 处理兵力事件
    Rule::applyTurnPassage(map, playerRegistry, currentTurn);
}

void Game::treatRanking() {
    for(int playerID = 0; playerID < playerRegistry.getPlayerCount(); playerID ++) {
        Player *player = playerRegistry.atPlayer(playerID);
        if(diedPlayer[playerID]) continue; // 已统计死亡
        if(player->alive) continue; // 没死
        alivePlayer --;
    }
    for(int playerID = 0; playerID < playerRegistry.getPlayerCount(); playerID ++) {
        Player *player = playerRegistry.atPlayer(playerID);
        if(diedPlayer[playerID]) continue; // 已统计死亡
        if(player->alive) continue; // 没死
        diedPlayer[playerID] = 1;
        playerRank.push_back(Rank(playerID, alivePlayer));
    }

    for(int teamID = 0; teamID < playerRegistry.getTeamCount(); teamID ++) {
        Team *team = playerRegistry.atTeam(teamID);
        if(diedTeam[teamID]) continue; // 已统计
        if(team->alive) continue; // 未死亡
        aliveTeam --;
    }
    for(int teamID = 0; teamID < playerRegistry.getTeamCount(); teamID ++) {
        Team *team = playerRegistry.atTeam(teamID);
        if(diedTeam[teamID]) continue; // 已统计
        if(team->alive) continue; // 未死亡
        diedTeam[teamID] = 1;
        teamRank.push_back(Rank(teamID, aliveTeam));
    }
}

void Game::treatGameoverRanking() {
    if(checkGameover() == false) {
        logger.warning("Game::treatGameoverRanking: 检查游戏未结束");
        return;
    }

    treatRanking(); // 先处理当前回合的死亡玩家 队伍

    for(int playerID = 0; playerID < playerRegistry.getPlayerCount(); playerID ++) {
        Player *player = playerRegistry.atPlayer(playerID);
        if(player->alive == false) continue;
        playerRank.push_back(Rank(playerID, 1));
    }

    for(int teamID = 0; teamID < playerRegistry.getTeamCount(); teamID ++) {
        Team *team = playerRegistry.atTeam(teamID);
        if(team->alive == false) continue;
        teamRank.push_back(Rank(teamID, 1));
    }
}

bool Game::checkGameover() const {
    assert(aliveTeam >= 1 && alivePlayer >= 1);
    return aliveTeam == 1;
}

bool Game::nextTurn(const std::vector<MoveOperation> &movement) {
    treatTurn(movement);
    if(checkGameover()) {
        treatGameoverRanking();
        logger.info("Game::nextTurn: 游戏结束");
        return true;
    }
    treatRanking();
    return false;
}

namespace { // namespace

bool isVisible(const Point &p, const GameMap<Terrain> &map, const vector<char> &inMap) {
    for(int dx = -1; dx <= 1; dx ++) {
        for(int dy = -1; dy <= 1; dy ++) {
            Point nxt = nextPoint(p, dx, dy);
            if(map.isInside(nxt) == false) continue;
            Tile tile = map.get(nxt);
            if(tile.isNeutral() == false && inMap[tile.owner]) return true;
        }
    }
    return false;
}

} // namespace

const GameMap<VisibleTerrain> Game::getTeamVision(int teamID) const {
    const Team *team = playerRegistry.atTeam(teamID);
    vector<char> inTeam(team->players.size(), 0); // 判断玩家是否是该小队
    for(int playerID : team->players) {
        inTeam[playerID] = 1;
    }
    int R = map.R, C = map.C;
    GameMap<VisibleTerrain> vision(R, C);
    for(int x = 0; x < R; x ++) {
        for(int y = 0; y < C; y ++) {
            Tile tile = map.get(x, y);
            Tile<VisibleTerrain> pt;
            if(isVisible(Point(x, y), map, inTeam)) {
                pt.owner = tile.owner;
                pt.unit = tile.unit;
                pt.terrain = toVisibleTerrain(tile.terrain);
                vision.set(x, y, pt);
            }
            else {
                pt.owner = NEUTRAL;
                pt.unit = 0;
                switch(tile.terrain) {
                    case Terrain::Blank:
                    case Terrain::Capital:
                        pt.terrain = VisibleTerrain::FogBlank;
                    case Terrain::City:
                    case Terrain::Mountain:
                        pt.terrain = VisibleTerrain::FogMountain;
                    case Terrain::Swamp:
                        pt.terrain = VisibleTerrain::Swamp;
                    default:
                        logger.error(format("const GameMap<VisibleTerrain>& Game::getTeamVision(int teamID): 未知类型 {}",magic_enum::enum_name(tile.terrain)));
                        pt.terrain = VisibleTerrain::FogBlank;
                }
                vision.set(x, y, pt);
            }
        }
    }
    return vision;
}

}
