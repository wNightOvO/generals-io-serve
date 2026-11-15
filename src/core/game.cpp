#include "game.hpp"

namespace gen::core {

using std::vector;
using std::queue;

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


}