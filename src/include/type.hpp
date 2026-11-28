/*
    游戏通用类型
    包括地形 视野地形 坐标 排名
*/

#pragma once
#include <string>
#include <magic_enum/magic_enum.hpp>

namespace gen {

// 真实地形 编码 0 到 4 与通信协议一致
enum class Terrain : int {
    Blank = 0, // 空地
    Mountain = 1, // 山区
    Swamp = 2, // 沼泽
    City = 3, // 城市
    Capital = 4 // 首都
};

// 地形种类数量
inline constexpr int TERRAIN_COUNT = 5;

// 玩家可见地形 编码 0 到 7 与通信协议一致
enum class VisibleTerrain : int {
    Blank = 0, // 空地
    Mountain = 1, // 山区
    Swamp = 2, // 沼泽
    City = 3, // 城市
    Capital = 4, // 首都
    FogBlank = 5, // 疑似空地 真实为空地或首都
    FogMountain = 6, // 疑似山区 真实为山区或城市
    FogSwamp = 7 // 视野外沼泽
};

// 可见地形种类数量
inline constexpr int VISIBLE_TERRAIN_COUNT = 8;

// 返回地形的英文名 例如 Blank 返回 "Blank" 用于日志与调试
inline const char* terrainToName(Terrain terrain) {
    auto name = magic_enum::enum_name(terrain);
    return name.empty() ? "Unknown" : name.data();
}

// 返回地形的中文名 用于日志与界面 参数 terrain 为地形 返回常量字符串
inline const char* terrainToChinese(Terrain terrain) {
    switch(terrain) {
        case Terrain::Blank: return "空地";
        case Terrain::Mountain: return "山区";
        case Terrain::Swamp: return "沼泽";
        case Terrain::City: return "城市";
        case Terrain::Capital: return "首都";
        default: return "未知";
    }
}

// 返回可见地形的中文名 用于界面 参数 terrain 为可见地形 返回常量字符串
inline const char* visibleTerrainToChinese(VisibleTerrain terrain) {
    switch(terrain) {
        case VisibleTerrain::Blank: return "空地";
        case VisibleTerrain::Mountain: return "山区";
        case VisibleTerrain::Swamp: return "沼泽";
        case VisibleTerrain::City: return "城市";
        case VisibleTerrain::Capital: return "首都";
        case VisibleTerrain::FogBlank: return "疑似空地";
        case VisibleTerrain::FogMountain: return "疑似山区";
        case VisibleTerrain::FogSwamp: return "沼泽";
        default: return "未知";
    }
}

// 由配置中的地形名解析地形 大小写不敏感 解析成功返回 true 并写入 terrain
inline bool terrainFromName(const std::string &name, Terrain &terrain) {
    auto value = magic_enum::enum_cast<Terrain>(name, magic_enum::case_insensitive);
    if(!value.has_value()) return false;
    terrain = *value;
    return true;
}

// 把真实地形转换为可见地形 视野内使用
inline VisibleTerrain toVisibleTerrain(Terrain terrain) {
    switch(terrain) {
        case Terrain::Blank: return VisibleTerrain::Blank;
        case Terrain::Mountain: return VisibleTerrain::Mountain;
        case Terrain::Swamp: return VisibleTerrain::Swamp;
        case Terrain::City: return VisibleTerrain::City;
        case Terrain::Capital: return VisibleTerrain::Capital;
        default: return VisibleTerrain::FogBlank;
    }
}

// 把真实地形转换为视线外使用的疑似地形
inline VisibleTerrain toFogTerrain(Terrain terrain) {
    switch(terrain) {
        case Terrain::Blank:
        case Terrain::Capital:
            return VisibleTerrain::FogBlank;
        case Terrain::Mountain:
        case Terrain::City:
            return VisibleTerrain::FogMountain;
        case Terrain::Swamp:
            return VisibleTerrain::FogSwamp;
        default:
            return VisibleTerrain::FogBlank;
    }
}

// 坐标 行列均从 0 开始 x 为行 y 为列
struct Point {
    int x = 0;
    int y = 0;

    Point() = default;

    Point(int x, int y): x(x), y(y) {}

    bool operator == (const Point &other) const {
        return x == other.x && y == other.y;
    }

    bool operator != (const Point &other) const {
        return !(*this == other);
    }

    // 返回可读的坐标字符串 例如 "(3, 5)"
    std::string toString() const {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

// 排名条目 id 的含义由使用场景决定 可以是玩家编号或队伍编号 rank 从 1 开始
struct Rank {
    int id = 0;
    int rank = 0;
};

} // namespace gen
