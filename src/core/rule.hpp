/*
    处理游戏操作和规则
*/
#pragma once
#include <core/map.hpp>
#include <core/player.hpp>
#include <include/direction.hpp>
#include <include/move_operation.hpp>

namespace gen::core {

class Rule {
private:
    static void occupy(GameMap &map, PlayerRegistry &playerRegistry, Tile &destTile, int sourceID, int moveUnits); // 占领某个地块

    static void beatPlayer(GameMap &map, PlayerRegistry &playerRegistry, int defeatenID, int sourceID); // 击败玩家

    static bool isValidMove(const GameMap &map, const MoveOperation &op); // 判断移动是否合法
public:
    static void applyTurnPassage(GameMap &map, PlayerRegistry &playerRegistry, int &currentTurn); // 回合（时间）流逝  并会修改时间

    static void applyMove(GameMap &map, PlayerRegistry &playerRegistry, const MoveOperation &op); // 执行移动操作
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
