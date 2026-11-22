#include <core/map_generator.hpp>
#include <include/map.hpp>
#include <vector>
#include <include/direction.hpp>
#include <include/random.hpp>
#include <include/util.hpp>

namespace gen::core {

using std::vector;
using std::pair;
using std::swap;

double Map_Generator::Pcity = 0.06;
double Map_Generator::Pmountain = 0.15;
double Map_Generator::Pswamp = 0.03;
int Map_Generator::LcityUnit = 30;
int Map_Generator::RcityUnit = 55;

namespace {  // namespace

int visitCityCapital(Point p, vector<vector<char>> &vis, const GameMap<Terrain> &map) {
    if(vis[p.x][p.y] != 0) return 0;
    vis[p.x][p.y] = 1;
    Tile<Terrain> tile = map.get(p);
    int total = 0;
    if(tile.terrain == Terrain::City || tile.terrain == Terrain::Capital) total ++;
    for(int way = 0; way < 4; way ++) {
        Point nxt = nextPoint(p, way);
        if(map.isInside(nxt) && vis[nxt.x][nxt.y] == 0) {
            total += visitCityCapital(nxt, vis, map);
        } 
    }
    return total;
}

Tile<Terrain> generatorTile() {
    double rnd = random01();
    static const vector<pair<double, Terrain>> v = Map_Generator::buildP();
    for(int i = 0; i < int(v.size()); i ++) {
        if(rnd <= v[i].first) {
            if(v[i].second == Terrain::City) {
                return Tile(v[i].second, NEUTRAL, randomInt(Map_Generator::LcityUnit, Map_Generator::RcityUnit));
            }
            return Tile(v[i].second, NEUTRAL, 0);
        }
    }
    assert(false && "ERROR: Tile generatorTile");
    return Tile(Terrain::Blank, NEUTRAL, 0);
}

} // namespace

bool Map_Generator::isConnected(const GameMap<Terrain> &map) {
    int totalCityCapital = 0;
    Point start(-1, -1);
    for(int x = 0; x < map.R; x ++) {
        for(int y = 0; y < map.C; y ++) {
            Tile<Terrain> tile = map.get(x, y);
            if(tile.terrain == Terrain::City || tile.terrain == Terrain::Capital) {
                totalCityCapital ++;
                if(totalCityCapital == 1) start = Point(x, y);
            }
        }
    }
    vector<vector<char>> vis(map.R, vector<char>(map.C, 0));
    int totalVisiting = visitCityCapital(start, vis, map);
    return totalCityCapital == totalVisiting;
}

void Map_Generator::_generator1v1Balancedly(GameMap<Terrain> &map) {
    const int R = map.R, C = map.C;
    bool isOdd = bool(R % 2 == 1);
    if(isOdd) { // 奇数
        int x = (R - 1) / 2;
        for(int y = 0; y < C; y ++) {
            map.set(x, y, generatorTile());
        }
    }
    Point capitalPosition = Point(randomInt(0, R / 2 - 1), randomInt(0, C - 1));

    // 首都的ID
    int capPlayerID[2] = {0, 1};
    if(randomInt(0, 1) == 1) swap(capPlayerID[0], capPlayerID[1]);

    for(int x = 0; x < R / 2; x ++) {
        for(int y = 0; y < C; y ++) {
            Point xy(x, y);
            if(xy == capitalPosition) { // 首都
                map.set(x, y, Tile(Terrain::Capital, capPlayerID[0], 1));
            }
            else {
                Tile<Terrain> thisTile = generatorTile();
                map.set(x, y, thisTile);
            }
        }
    }
    for(int x = (R + 1) / 2; x < R; x ++) {
        for(int y = 0; y < C; y ++) {
            int mirrorX = (isOdd ? R - 1 - x : R - x);
            int mirrorY = C - y;
            Tile tile = map.get(mirrorX, mirrorY);
            if(tile.terrain == Terrain::Capital) {
                tile.owner = capPlayerID[1];
            }
            map.set(x, y, map.get(mirrorX, mirrorY));
        }
    }
}

void Map_Generator::generator1v1Balancedly(GameMap<Terrain> &map) {
    do {
        _generator1v1Balancedly(map);
    }while(isConnected(map));
}

}
