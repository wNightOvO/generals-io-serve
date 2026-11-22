#include <core/game.hpp>
#include <include/util.hpp>
#include <include/logger.hpp>
#include <format>

namespace gen::core {

using std::vector;
using std::queue;
using std::format;

Game::Game(int rows, int cols): map(rows, cols), playerRegistry() {
    R = rows;
    C = cols;
    currentTurn = 0;
}

void Game::nextTurn(const vector<MoveOperation> &movement) {
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

const GameMap<VisibleTerrain> Game::getTeamVision(int teamID) {
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
