/*
    定义游戏的地图
*/
#pragma once
#include <include/type.hpp>
#include <vector>
#include <cassert>
#include <magic_enum/magic_enum.hpp>
#include <iostream>

namespace gen::core {

inline constexpr int NEUTRAL = -1; // 中立单位所有者为 -1 

template<class T>
struct Tile {
    T terrain;
    int owner = NEUTRAL;
    int unit = 0;

    Tile() {}

    Tile(T terrain, int owner, int unit): terrain(terrain), owner(owner), unit(unit) {}

    bool isNeutral() const {
        return owner == NEUTRAL;
    }

    std::string toString() const {
        return "{terrain: " + magic_enum::enum_name(terrain) + ", owner: " + std::to_string(owner) + ", unit: " + std::to_string(unit) + "}";
    }

    // 简略写法 第一位表示占有者（小写字母，中立为_） 第二位表示类型（见type.hpp） 第三、四位表示兵力数量（暂时十进制 后续考虑更大）
    std::string toSimpleString() const {
        std::string s = (owner == NEUTRAL ? "_" : std::string('a'+owner));
        s += static_cast<char>(terrain);
        if(unit > 99) s += "99";
        else if(unit <= 9) s += "0" + std::to_string(unit);
        else s += std::to_string(unit); 
        return s;
    }
};

// 地图坐标 0-index
template<class T>
class GameMap {
private:
    std::vector<Tile<T>> tiles; // 存储地图的二维数组
public:
    int R, C;

    GameMap(int R, int C) {
        tiles.resize(R * C, Tile<T>());
    }

    Tile<T>& at(int x, int y){ //返回该格的引用
        assert(isInside(x, y));
        return tiles[x * C + y];
    }

    Tile<T>& at(const Point &p) {
        return at(p.x, p.y);
    }

    Tile<T> get(int x, int y) const { //返回该格的值
        assert(isInside(x, y));
        return tiles[x * C + y];
    }

    Tile<T> get(const Point &p) const {
        return get(p.x, p.y);
    }

    void set(int x, int y, const Tile<T> &tile) { //设置该格的值
        assert(isInside(x, y));
        tiles[x * C + y] = tile;
    }

    void set(const Point &p, const Tile<T> &tile) {
        set(p.x, p.y, tile);
    }

    bool isInside(int x, int y) const {
        return x >= 0 && x < R && y >= 0 && y < C;
    }

    bool isInside(const Point &p) const {
        return isInside(p.x, p.y);
    }

    Tile<T>* operator[](int x) {
        assert(x >= 0 && x < R);
        return &tiles[x * C];
    }

    std::vector<Tile<T>>& atTiles() {
        return tiles;
    } 

    // 简单形式输出便于调试
    std::ostringstream toSimpleOstringstream() const {
        std::ostringstream ostr;
        for(int x = 0; x < R; x ++) {
            for(int y = 0; y < R; y ++) {
                ostr << get(Point(x, y)).toSimpleString() << ' ';
            }
            ostr << std::endl;
        }
        return ostr;
    }
};

}
