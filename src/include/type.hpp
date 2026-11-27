/*
    定义游戏的类型
*/
#pragma once
#include <cstdint>
#include <include/logger.hpp>
#include <magic_enum/magic_enum.hpp>
#include <format>

namespace gen {

// 地形
enum class Terrain : char {
    Blank = '_',
    Swamp = 'S',
    Mountain = 'M',
    Capital = 'K',
    City = 'C'
};

// 玩家可以看见的地形
enum class VisibleTerrain : char {
    Blank = '_',
    Swamp = 'S',
    Mountain = 'M',
    Capital = 'K',
    City = 'C',
    FogBlank = '_',
    FogMountain = 'M'
};

inline VisibleTerrain toVisibleTerrain(Terrain terrain) {
    switch(terrain) {
        case Terrain::Blank:
            return VisibleTerrain::Blank;
        case Terrain::Capital:
            return VisibleTerrain::Capital;
        case Terrain::City:
            return VisibleTerrain::City;
        case Terrain::Mountain:
            return VisibleTerrain::Mountain;
        case Terrain::Swamp:
            return VisibleTerrain::Swamp;
        default:
            logger.error(std::format("VisibleTerrain toVisibleTerrain(Terrain terrain): 不存在的类型 {}",magic_enum::enum_name<Terrain>(terrain)));
            return VisibleTerrain::Blank;
    }
}

// 坐标
struct Point {
    int x = 0;
    int y = 0;

    Point() = default;

    Point(int x, int y): x(x), y(y) {}

    bool operator == (const Point &o) const {
        return x == o.x && y == o.y;
    }

    bool operator != (const Point &o) const {
        return !(*this == o);
    }

    Point operator + (const Point &o) const {
        return Point(x + o.x, y + o.y);
    }

    Point operator - (const Point &o) const {
        return Point(x - o.x, y - o.y);
    }

    std::string toString() {
        return "("+std::to_string(x)+", "+std::to_string(y)+")";
    }
};

}
