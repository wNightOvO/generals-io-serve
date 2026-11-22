/*
    工具
*/

#pragma once
#include <include/type.hpp>
#include <include/direction.hpp>
#include <include/random.hpp>
#include <cmath>

namespace gen {

inline Point nextPoint(const Point &p, int dx, int dy) {
    return Point(p.x + dx, p.y + dy);
}

inline Point nextPoint(const Point &p, int way) {
    assert(way >= 0 && way <= 3);
    return nextPoint(p, directions[way][0], directions[way][1]);
}

// 曼哈顿距离
inline int mhtDistance(const Point &u, const Point &v) {
    return std::abs(u.x - v.x) + std::abs(u.y - v.y);
}

// 切比雪夫距离
inline int cbsvDistance(const Point &u, const Point &v) {
    return std::max(std::abs(u.x - v.x), std::abs(u.y - v.y));
}

}