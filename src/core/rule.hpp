/*
    处理游戏操作和规则
*/
#pragma once
#include <include/map.hpp>
#include <core/player.hpp>
#include <include/direction.hpp>
#include <include/move_operation.hpp>
#include <include/type.hpp>

namespace gen::core {

class Rule {
private:
    static void occupy(GameMap<Terrain> &map, PlayerRegistry &playerRegistry, Tile<Terrain> &destTile, int sourceID, int moveUnits); // 占领某个地块

    static void beatPlayer(GameMap<Terrain> &map, PlayerRegistry &playerRegistry, int defeatenID, int sourceID); // 击败玩家

    static bool isValidMove(const GameMap<Terrain> &map, const MoveOperation &op); // 判断移动是否合法
public:
    static void applyTurnPassage(GameMap<Terrain> &map, PlayerRegistry &playerRegistry, int &currentTurn); // 回合（时间）流逝  并会修改时间

    static void applyMove(GameMap<Terrain> &map, PlayerRegistry &playerRegistry, const MoveOperation &op); // 执行移动操作
};

// 将方向转换为坐标偏移量 
inline Point directionToPoint(int d) {
    return Point(directions[d][0], directions[d][1]);
}

// 计算移动后的位置
inline Point calcDestination(const Point &source, int d) {
    return source + directionToPoint(d);
}

}
