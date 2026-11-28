/*
    地图生成器
    根据游戏模式和配置生成地图 放置首都 保证连通
*/

#pragma once
#include <include/map.hpp>
#include <include/type.hpp>
#include <core/config.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace gen::core {

// 地图生成参数
struct MapGenParams {
    std::array<bool, TERRAIN_COUNT> enabled{{true, true, true, true, true}}; // 允许生成的地形 下标为地形编号
    double cityProbability = 0.06; // 城市概率
    double mountainProbability = 0.15; // 山区概率
    double swampProbability = 0.03; // 沼泽概率
    int cityUnitsMin = 30; // 城市初始最小兵力
    int cityUnitsMax = 55; // 城市初始最大兵力
};

// 地图生成器 全部函数为静态函数
class MapGenerator {
private:
    // 生成随机地形的一个格子 只使用 enabled 为 true 的地形
    static Tile<Terrain> randomTile(const MapGenParams &params);

    // 放置首都 参数 map 为地图 参数 mode 为模式 参数 playerIDs 为玩家编号 参数 playerTeamIDs 为队伍编号
    static void placeCapitals(GameMap<Terrain> &map, GameMode mode,
                              const std::vector<int> &playerIDs,
                              const std::vector<int> &playerTeamIDs);

    // 判断全部首都能否在不经过山区的情况下互相到达
    static bool isConnected(const GameMap<Terrain> &map);

    // 把全部首都强制连通 用于多次生成后仍不连通时的兜底
    static void forceConnect(GameMap<Terrain> &map);
public:
    // 生成地图 参数 map 的尺寸必须已经设置好 参数 playerIDs 与 playerTeamIDs 一一对应
    static void generate(GameMap<Terrain> &map, GameMode mode,
                         const std::vector<int> &playerIDs,
                         const std::vector<int> &playerTeamIDs,
                         const MapGenParams &params);

    // 统计地图内各种地形的数量 返回数组下标为地形编号
    static std::array<int, TERRAIN_COUNT> countTerrains(const GameMap<Terrain> &map);
};

} // namespace gen::core
