/*
    游戏地图
    以行优先一维数组保存二维格子
*/

#pragma once
#include <include/type.hpp>
#include <vector>
#include <string>
#include <cassert>

namespace gen::core {

// 中立格子所有者的编号
inline constexpr int NEUTRAL = -1;

// 视野外格子的未知所有者编号
inline constexpr int UNKNOWN = -2;

// 一个格子 terrain 为地形 owner 为所有者编号 unit 为兵力
template<class T>
struct Tile {
    T terrain{};
    int owner = NEUTRAL;
    int unit = 0;

    Tile() = default;

    Tile(T terrain, int owner, int unit): terrain(terrain), owner(owner), unit(unit) {}

    // 判断是否为中立格子
    bool isNeutral() const {
        return owner == NEUTRAL;
    }

    // 判断格子是否由 playerID 占领
    bool isOwnedBy(int playerID) const {
        return owner == playerID;
    }

    // 返回可读的格子信息 用于日志
    std::string toString() const {
        return "{terrain: " + std::string(terrainToName(static_cast<Terrain>(static_cast<int>(terrain))))
            + ", owner: " + std::to_string(owner) + ", unit: " + std::to_string(unit) + "}";
    }
};

// 二维地图 rows 为行数 cols 为列数 行优先存储
template<class T>
class GameMap {
private:
    std::vector<Tile<T>> tiles_; // 行优先保存全部格子
public:
    int R = 0; // 行数
    int C = 0; // 列数

    GameMap() = default;

    GameMap(int rows, int cols) {
        resize(rows, cols);
    }

    // 修改地图大小并重置全部格子
    void resize(int rows, int cols) {
        R = rows;
        C = cols;
        tiles_.assign(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), Tile<T>());
    }

    // 返回格子引用 调用者保证坐标在地图内
    Tile<T>& at(int x, int y) {
        assert(isInside(x, y));
        return tiles_[static_cast<std::size_t>(x) * static_cast<std::size_t>(C) + static_cast<std::size_t>(y)];
    }

    // 返回格子引用的常量版本
    const Tile<T>& at(int x, int y) const {
        assert(isInside(x, y));
        return tiles_[static_cast<std::size_t>(x) * static_cast<std::size_t>(C) + static_cast<std::size_t>(y)];
    }

    // 返回坐标对应格子的引用
    Tile<T>& at(const Point &point) {
        return at(point.x, point.y);
    }

    // 返回坐标对应格子的引用 常量版本
    const Tile<T>& at(const Point &point) const {
        return at(point.x, point.y);
    }

    // 返回格子值的拷贝
    Tile<T> get(int x, int y) const {
        return at(x, y);
    }

    // 返回格子值的拷贝 参数为坐标
    Tile<T> get(const Point &point) const {
        return at(point);
    }

    // 设置格子的值
    void set(int x, int y, const Tile<T> &tile) {
        at(x, y) = tile;
    }

    // 设置格子的值 参数为坐标
    void set(const Point &point, const Tile<T> &tile) {
        at(point) = tile;
    }

    // 判断坐标是否在地图内
    bool isInside(int x, int y) const {
        return x >= 0 && x < R && y >= 0 && y < C;
    }

    // 判断坐标是否在地图内 参数为坐标
    bool isInside(const Point &point) const {
        return isInside(point.x, point.y);
    }

    // 返回全部格子 供遍历使用
    std::vector<Tile<T>>& tiles() {
        return tiles_;
    }

    // 返回全部格子 常量版本
    const std::vector<Tile<T>>& tiles() const {
        return tiles_;
    }

    // 返回格子总数
    int tileCount() const {
        return R * C;
    }
};

} // namespace gen::core
