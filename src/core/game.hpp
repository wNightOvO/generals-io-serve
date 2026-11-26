/*
    游戏主体逻辑
*/

#pragma once
#include <functional>
#include <include/map.hpp>
#include <core/player.hpp>
#include <core/rule.hpp>
#include <include/type.hpp>
#include <include/logger.hpp>

namespace gen::core {

class Game {
private:
    int R, C;
    GameMap<Terrain> map;
    int currentTurn;
    int alivePlayer;
    int aliveTeam;
    PlayerRegistry playerRegistry;

    struct Rank {
        int id;
        int ranking;

        Rank() = default;

        Rank(int id_, int ranking_): id(id_), ranking(ranking_) {}
    };

    std::vector<Rank> playerRank;
    std::vector<Rank> teamRank;
    std::vector<char> diedPlayer; // 已死亡的玩家
    std::vector<char> diedTeam; // 已死亡的队伍

    // 进入下一回合 执行处理 传入玩家移动序列
    void treatTurn(const std::vector<MoveOperation> &movement);

    // 处理死亡玩家 并处理排名
    void treatRanking();

    // 处理游戏结束后排名 处理胜利玩家 队伍
    void treatGameoverRanking();
public:
    Game(int rows, int cols): map(rows, cols) { // 构造函数 回合0为起始回合
        R = rows;
        C = cols;
        currentTurn = 0;
        alivePlayer = 0;
        aliveTeam = 0;
        playerRank.clear();
        teamRank.clear();
    } 

    void registrePlayer(int playerID, int teamID) {
        playerRegistry.registerPlayer(playerID, teamID);
    }

    // 注册完全部玩家后初始化游戏 传入地图生成器
    void initializeGame(std::function<void(GameMap<Terrain>&)> mapGen) {
        mapGen(map);
        // 存活玩家和队伍数量为初始数量
        alivePlayer = playerRegistry.getPlayerCount();
        aliveTeam = playerRegistry.getTeamCount();
        playerRegistry.registerMoveQueue(); // 构造移动队列
        diedPlayer.assign(playerRegistry.getPlayerCount(), 0); // 构造diedPlayer
        diedTeam.assign(playerRegistry.getTeamCount(), 0); // 构造diedTeam
    }

    // 判断是否结束
    bool checkGameover() const;

    // 进入下回合 并处理事件和排名 传入移动序列 返回游戏是否结束
    bool nextTurn(const std::vector<MoveOperation> &movement);

    // 获取队伍视野
    const GameMap<VisibleTerrain> getTeamVision(int teamID) const;
};


}
