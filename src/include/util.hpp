/*
    常用工具函数
*/

#pragma once
#include <include/type.hpp>
#include <include/direction.hpp>
#include <algorithm>
#include <cmath>

namespace gen {

// 返回坐标加上偏移量后的新坐标
inline Point nextPoint(const Point &point, int dx, int dy) {
    return Point(point.x + dx, point.y + dy);
}

// 返回坐标沿方向 way 走一格后的新坐标 参数 way 为 0 到 3
inline Point nextPoint(const Point &point, int way) {
    assert(way >= 0 && way < DIRECTION_COUNT);
    return nextPoint(point, directions[way][0], directions[way][1]);
}

// 返回两点曼哈顿距离
inline int manhattanDistance(const Point &a, const Point &b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// 返回两点切比雪夫距离
inline int chebyshevDistance(const Point &a, const Point &b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

// 把 value 限制在 low 与 high 之间 返回限制后的值
inline int clampInt(int value, int low, int high) {
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

// 把 value 限制在 low 与 high 之间 返回限制后的浮点值
inline double clampDouble(double value, double low, double high) {
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

} // namespace gen
