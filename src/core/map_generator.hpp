/*
    地图生成器
*/
#pragma once
#include <core/map.hpp>
#include <vector>
#include <cassert>

namespace gen::core {

class Map_Generator {
private:
    static bool isConnected(const GameMap &map);
    static void _generator1v1Balancedly(GameMap &map);
public:
    // 生成概率 之后可调
    static double Pcity, Pmountain, Pswamp;

    // 城市初始兵力范围
    static int LcityUnit, RcityUnit;

    inline static std::vector<std::pair<double, Terrain>> buildP() {
        std::vector<std::pair<double, Terrain>> v;
        double sums = 0.0;
        v.push_back(std::make_pair(Pcity, Terrain::City));
        sums += Pcity;
        v.push_back(std::make_pair(Pmountain, Terrain::Mountain));
        sums += Pmountain;
        v.push_back(std::make_pair(Pswamp, Terrain::Swamp));
        sums += Pswamp;
        // 构建空地概率
        assert(sums < 1.0);
        v.push_back(std::make_pair(1.0-sums, Terrain::Blank));
        return v;
    }
    
    static void generator1v1Balancedly(GameMap &map);
};

}