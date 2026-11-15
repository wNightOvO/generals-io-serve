/*
    游戏主体逻辑
*/

#pragma once
#include <functional>
#include "map.hpp"
#include "player.hpp"
#include "rule.hpp"

namespace gen::core {

class Game {
private:
    int R, C;
    GameMap map;
    int currentTurn;
    PlayerRegistry playerRegistry;
public:
    Game(int rows, int cols); // 构造函数 回合0为起始回合

    void registrePlayer(int playerID, int teamID) {
        playerRegistry.registerPlayer(playerID, teamID);
    }

    // 注册完全部玩家后初始化游戏 传入地图生成器
    void initializeGame(std::function<void(GameMap&)> mapGen) {
        mapGen(map);
        playerRegistry.registerMoveQueue(); // 构造移动队列
    }

    // 进入下一回合 执行处理 传入玩家移动序列
    void nextTurn(const std::vector<MoveOperation> &movement);
};


}