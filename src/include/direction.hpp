/*
    方向定义
    方向数组按 上 左 右 下 排列
*/

#pragma once
#include <include/type.hpp>
#include <cstdint>
#include <cassert>

namespace gen {

// 四个方向的坐标偏移 第一维为方向 第二维 0 为行偏移 1 为列偏移
inline constexpr int directions[4][2] = {
    {-1, 0}, // 上
    {0, -1}, // 左
    {0, 1}, // 右
    {1, 0} // 下
};

// 方向编号
enum class Direction : std::uint8_t {
    Up = 0,
    Left = 1,
    Right = 2,
    Down = 3
};

// 方向数量
inline constexpr int DIRECTION_COUNT = 4;

// 判断方向编号是否合法
inline bool isValidDirection(int direction) {
    return direction >= 0 && direction < DIRECTION_COUNT;
}

} // namespace gen
