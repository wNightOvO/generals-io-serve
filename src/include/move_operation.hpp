/*
    移动操作
*/

#pragma once
#include <include/type.hpp>

namespace gen {

// 一次移动操作 源格到目标格 直线相邻
struct MoveOperation {
    int playerID = 0; // 执行移动的玩家编号
    Point source; // 源格坐标
    Point dest; // 目标格坐标
    bool half = false; // 是否移动一半兵力 true 时源格保留 floor(u / 2)

    MoveOperation() = default;

    MoveOperation(int playerID, Point source, Point dest, bool half)
        : playerID(playerID), source(source), dest(dest), half(half) {}
};

} // namespace gen
