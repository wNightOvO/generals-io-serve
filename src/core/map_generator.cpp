/*
    地图生成器的实现
    根据游戏模式和配置生成地图 放置首都 保证连通
*/

#include <core/map_generator.hpp>
#include <include/direction.hpp>
#include <include/random.hpp>
#include <include/util.hpp>
#include <algorithm>
#include <cassert>
#include <queue>
#include <vector>

namespace gen::core {

using std::vector;

namespace { // namespace

// 在指定位置放置一个首都 兵力为 1 归属为玩家
void putCapital(GameMap<Terrain> &map, const Point &point, int playerID) {
    map.at(point) = Tile<Terrain>(Terrain::Capital, playerID, 1);
}

// 收集地图上全部首都的坐标
vector<Point> collectCapitals(const GameMap<Terrain> &map) {
    vector<Point> capitals;
    for(int x = 0; x < map.R; ++ x) {
        for(int y = 0; y < map.C; ++ y) {
            if(map.at(x, y).terrain == Terrain::Capital) {
                capitals.push_back(Point(x, y));
            }
        }
    }
    return capitals;
}

// 清空首都周围一圈的山区 避免开局被围死
void clearCapitalRing(GameMap<Terrain> &map, const Point &center) {
    for(int dx = -1; dx <= 1; ++ dx) {
        for(int dy = -1; dy <= 1; ++ dy) {
            Point point = nextPoint(center, dx, dy);
            if(map.isInside(point) == false) continue;
            Tile<Terrain> &tile = map.at(point);
            if(tile.terrain == Terrain::Mountain) {
                tile = Tile<Terrain>(Terrain::Blank, NEUTRAL, 0);
            }
        }
    }
}

} // namespace

// 生成随机地形的一个格子 只使用 enabled 为 true 的地形
Tile<Terrain> MapGenerator::randomTile(const MapGenParams &params) {
    // 各地形基础权重 空地权重为剩余概率 首都单独放置不参与随机
    double weight[TERRAIN_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};
    weight[static_cast<int>(Terrain::Blank)] = 1.0 - params.cityProbability - params.mountainProbability - params.swampProbability;
    weight[static_cast<int>(Terrain::Mountain)] = params.mountainProbability;
    weight[static_cast<int>(Terrain::Swamp)] = params.swampProbability;
    weight[static_cast<int>(Terrain::City)] = params.cityProbability;
    double total = 0.0; // 允许出现的地形权重总和
    for(int index = 0; index < TERRAIN_COUNT; ++ index) {
        if(weight[index] < 0.0) weight[index] = 0.0;
        if(index == static_cast<int>(Terrain::Capital)) weight[index] = 0.0; // 首都不随机生成
        if(params.enabled[index] == false) weight[index] = 0.0; // 禁用地形权重清零
        total += weight[index];
    }
    if(total <= 0.0) {
        // 没有可用地形时退回空地 配置校验应当避免这种情况
        return Tile<Terrain>(Terrain::Blank, NEUTRAL, 0);
    }
    Terrain terrain = Terrain::Blank;
    double roll = random01() * total; // 按启用地形重新归一化的随机数
    for(int index = 0; index < TERRAIN_COUNT; ++ index) {
        if(weight[index] <= 0.0) continue;
        terrain = static_cast<Terrain>(index);
        if(roll < weight[index]) break;
        roll -= weight[index];
    }
    if(terrain == Terrain::City) {
        return Tile<Terrain>(Terrain::City, NEUTRAL, randomInt(params.cityUnitsMin, params.cityUnitsMax));
    }
    return Tile<Terrain>(terrain, NEUTRAL, 0);
}

// 放置首都 按游戏模式决定摆放方式
void MapGenerator::placeCapitals(GameMap<Terrain> &map, GameMode mode,
                                 const vector<int> &playerIDs,
                                 const vector<int> &playerTeamIDs) {
    const int playerCount = static_cast<int>(playerIDs.size());
    if(playerCount <= 0) return;
    const int R = map.R;
    const int C = map.C;
    vector<Point> capitals; // 已经放置的首都
    vector<char> used(static_cast<std::size_t>(R) * C, 0); // 已经占用的位置
    auto put = [&](const Point &point, int playerID) {
        if(map.isInside(point) == false) return false;
        int index = point.x * C + point.y;
        if(used[index] != 0) return false;
        used[index] = 1;
        putCapital(map, point, playerID);
        capitals.push_back(point);
        return true;
    };
    // 随机分散地放置一个首都 尽量远离已有首都
    auto placeRandomly = [&](int playerID) {
        if(static_cast<int>(capitals.size()) >= R * C) return false; // 地图已经放满
        Point best(0, 0); // 最优候选点
        int bestDistance = -1; // 最优候选点到最近首都的距离
        int samples = std::min(256, std::max(64, R * C / 2)); // 采样次数
        for(int attempt = 0; attempt < samples; ++ attempt) {
            Point candidate(randomInt(0, R - 1), randomInt(0, C - 1));
            if(used[candidate.x * C + candidate.y] != 0) continue;
            int nearest = R + C; // 候选点到最近首都的距离
            for(const Point &capital : capitals) {
                nearest = std::min(nearest, manhattanDistance(candidate, capital));
            }
            if(nearest > bestDistance) {
                bestDistance = nearest;
                best = candidate;
            }
        }
        if(bestDistance < 0) {
            // 随机采样没有找到空位时 顺序查找
            bool found = false;
            for(int x = 0; x < R && found == false; ++ x) {
                for(int y = 0; y < C && found == false; ++ y) {
                    if(used[x * C + y] != 0) continue;
                    best = Point(x, y);
                    found = true;
                }
            }
            if(found == false) return false;
        }
        return put(best, playerID);
    };
    if(mode == GameMode::Duel || mode == GameMode::Team) {
        // 对称布局 先取左上区域的一点 再取中心对称点与左右镜像点
        int xMax = R / 2 - 1; // 合法列范围上界 保证镜像点不重合
        int yMax = C / 2 - 1;
        if(xMax < 0) xMax = 0;
        if(yMax < 0) yMax = 0;
        int x = randomInt(0, xMax);
        int y = randomInt(0, yMax);
        vector<Point> spots;
        spots.push_back(Point(x, y)); // 左上
        spots.push_back(Point(R - 1 - x, C - 1 - y)); // 右下 与左上中心对称 同队
        if(mode == GameMode::Team) {
            spots.push_back(Point(x, C - 1 - y)); // 右上
            spots.push_back(Point(R - 1 - x, y)); // 左下 与右上中心对称 同队
        }
        vector<int> order(playerCount, 0); // 按队伍分组后的玩家顺序
        for(int index = 0; index < playerCount; ++ index) order[index] = index;
        if(mode == GameMode::Team) {
            std::stable_sort(order.begin(), order.end(), [&](int left, int right) {
                int teamLeft = (left < static_cast<int>(playerTeamIDs.size())) ? playerTeamIDs[left] : 0;
                int teamRight = (right < static_cast<int>(playerTeamIDs.size())) ? playerTeamIDs[right] : 0;
                if(teamLeft != teamRight) return teamLeft < teamRight;
                return left < right;
            });
        }
        vector<char> placedPlayer(playerCount, 0); // 玩家是否已经放置首都
        int limit = std::min(playerCount, static_cast<int>(spots.size()));
        for(int index = 0; index < limit; ++ index) {
            int playerIndex = order[index];
            if(put(spots[index], playerIDs[playerIndex])) placedPlayer[playerIndex] = 1;
        }
        // 布局方式没有覆盖到的玩家随机补放
        for(int index = 0; index < playerCount; ++ index) {
            if(placedPlayer[index] == 0) placeRandomly(playerIDs[index]);
        }
    }
    else {
        // FFA 随机分散放置全部首都
        for(int index = 0; index < playerCount; ++ index) {
            placeRandomly(playerIDs[index]);
        }
    }
    // 放置完成后清空首都周围一圈的山区
    for(const Point &capital : capitals) {
        clearCapitalRing(map, capital);
    }
}

// 判断全部首都能否在不经过山区的情况下互相到达
bool MapGenerator::isConnected(const GameMap<Terrain> &map) {
    vector<Point> capitals = collectCapitals(map);
    if(capitals.size() <= 1) return true;
    const int R = map.R;
    const int C = map.C;
    vector<char> visited(static_cast<std::size_t>(R) * C, 0); // 格子是否到达过
    std::queue<Point> queue;
    Point start = capitals[0];
    visited[start.x * C + start.y] = 1;
    queue.push(start);
    int reached = 1; // 从第一个首都出发能到达的首都数量
    while(queue.empty() == false) {
        Point current = queue.front();
        queue.pop();
        for(int way = 0; way < DIRECTION_COUNT; ++ way) {
            Point next = nextPoint(current, way);
            if(map.isInside(next) == false) continue;
            int index = next.x * C + next.y;
            if(visited[index] != 0) continue;
            const Tile<Terrain> &tile = map.at(next);
            if(tile.terrain == Terrain::Mountain) continue; // 山区不可通过
            visited[index] = 1;
            if(tile.terrain == Terrain::Capital) ++ reached;
            queue.push(next);
        }
    }
    return reached == static_cast<int>(capitals.size());
}

// 把全部首都强制连通 用于多次生成后仍不连通时的兜底 沿直线把山区改成空地
void MapGenerator::forceConnect(GameMap<Terrain> &map) {
    vector<Point> capitals = collectCapitals(map);
    if(capitals.size() <= 1) return;
    // 把一格山区改成空地 同时修改中心对称格 保持地图公平
    auto carve = [&](const Point &point) {
        if(map.isInside(point) == false) return;
        if(map.at(point).terrain == Terrain::Mountain) {
            map.at(point) = Tile<Terrain>(Terrain::Blank, NEUTRAL, 0);
        }
        Point mirror(map.R - 1 - point.x, map.C - 1 - point.y);
        if(map.isInside(mirror) && map.at(mirror).terrain == Terrain::Mountain) {
            map.at(mirror) = Tile<Terrain>(Terrain::Blank, NEUTRAL, 0);
        }
    };
    Point origin = capitals[0];
    for(std::size_t index = 1; index < capitals.size(); ++ index) {
        Point current = origin;
        Point target = capitals[index];
        carve(current);
        while(current != target) {
            if(current.x != target.x) {
                current.x += (target.x > current.x) ? 1 : -1;
            }
            else if(current.y != target.y) {
                current.y += (target.y > current.y) ? 1 : -1;
            }
            carve(current);
        }
    }
}

// 生成地图 多次尝试 直到全部首都连通 仍失败则强制打通
void MapGenerator::generate(GameMap<Terrain> &map, GameMode mode,
                            const vector<int> &playerIDs,
                            const vector<int> &playerTeamIDs,
                            const MapGenParams &params) {
    assert(map.R > 0 && map.C > 0); // 地图尺寸必须已经设置好
    assert(playerIDs.size() == playerTeamIDs.size());
    if(map.R <= 0 || map.C <= 0 || playerIDs.empty()) return;
    const int MAX_ATTEMPTS = 30; // 最大重新生成次数
    const bool symmetric = (mode == GameMode::Duel || mode == GameMode::Team); // 是否需要中心对称
    bool connected = false; // 全部首都是否连通
    for(int attempt = 0; attempt < MAX_ATTEMPTS; ++ attempt) {
        if(symmetric) {
            // 先生成一半 再逐格镜像到另一半 保证公平
            for(int x = 0; x < map.R; ++ x) {
                for(int y = 0; y < map.C; ++ y) {
                    int mirrorX = map.R - 1 - x;
                    int mirrorY = map.C - 1 - y;
                    if(x > mirrorX) continue;
                    if(x == mirrorX && y > mirrorY) continue;
                    Tile<Terrain> tile = randomTile(params);
                    map.at(x, y) = tile;
                    if(x != mirrorX || y != mirrorY) {
                        map.at(mirrorX, mirrorY) = tile;
                    }
                }
            }
        }
        else {
            // FFA 全图随机生成
            for(int x = 0; x < map.R; ++ x) {
                for(int y = 0; y < map.C; ++ y) {
                    map.at(x, y) = randomTile(params);
                }
            }
        }
        placeCapitals(map, mode, playerIDs, playerTeamIDs);
        if(isConnected(map)) {
            connected = true;
            break;
        }
    }
    if(connected == false) {
        // 多次生成仍不连通 沿直线把首都之间的山区改成空地
        forceConnect(map);
    }
}

// 统计地图内各种地形的数量
std::array<int, TERRAIN_COUNT> MapGenerator::countTerrains(const GameMap<Terrain> &map) {
    std::array<int, TERRAIN_COUNT> counts{};
    for(const Tile<Terrain> &tile : map.tiles()) {
        int index = static_cast<int>(tile.terrain);
        if(index >= 0 && index < TERRAIN_COUNT) ++ counts[index];
    }
    return counts;
}

} // namespace gen::core
