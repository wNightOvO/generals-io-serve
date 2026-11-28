/*
    generals.io 演示玩家 demo_expand 扩张型 AI

    本文件是给 2v2 与 FFA 测试用的简单对手 自带完整协议读写代码 可独立编译
    策略特点 占空地 抢中立城市 尽量不主动打硬仗

    编译方法 双击本目录 build.bat
    或命令行执行
    g++ -std=gnu++20 -O2 -static -finput-charset=UTF-8 -fexec-charset=UTF-8 -o demo_expand.exe demo_expand.cpp

    协议要点
    游戏程序通过标准输入发长度前缀帧 uint32 length 加 payload
    payload[0] 为消息类型 所有整数小端序 读取时必须循环读满
    收到 INIT 后输出一行 READY 并刷新 每回合收到 GO 后输出一行动作并刷新
    动作只有 PASS 与 M x1 y1 x2 y2 两种 本演示使用普通移动
    标准输出只能有 READY 与动作行 调试信息只能写标准错误
*/

#include <cstdint>
#include <cstdio>
#include <queue>
#include <string>
#include <vector>

// Windows 下需要把标准输入输出切换到二进制模式 见 main 函数开头的说明
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

// 协议消息类型 与开发计划第五节一致
namespace proto {
inline constexpr std::uint8_t PLAYER_SELF = 1;
inline constexpr std::uint8_t PLAYERS = 2;
inline constexpr std::uint8_t MAP_META = 3;
inline constexpr std::uint8_t MAP = 4;
inline constexpr std::uint8_t INIT = 5;
inline constexpr std::uint8_t TURN = 6;
inline constexpr std::uint8_t GO = 7;
inline constexpr std::uint8_t GAME_OVER = 8;
inline constexpr std::uint32_t MAX_FRAME_BYTES = 64u * 1024u * 1024u;
} // namespace proto

// 地形编码 0 到 4 真实地形 5 到 7 疑似地形
namespace terrain {
inline constexpr int BLANK = 0;
inline constexpr int MOUNTAIN = 1;
inline constexpr int SWAMP = 2;
inline constexpr int CITY = 3;
inline constexpr int CAPITAL = 4;
inline constexpr int FOG_BLANK = 5;
inline constexpr int FOG_MOUNTAIN = 6;
inline constexpr int FOG_SWAMP = 7;
} // namespace terrain

constexpr int DIR_X[4] = {-1, 1, 0, 0};
constexpr int DIR_Y[4] = {0, 0, -1, 1};
constexpr int INF = 1 << 20;

// 循环读满 size 字节 处理短读
bool readExact(std::uint8_t *dst, std::size_t size) {
    std::size_t got = 0;
    while (got < size) {
        std::size_t once = std::fread(dst + got, 1, size - got, stdin);
        if (once == 0) return false;
        got += once;
    }
    return true;
}

// 读取一帧长度前缀消息
bool readFrame(std::vector<std::uint8_t> &payload) {
    std::uint8_t head[4] = {};
    if (!readExact(head, 4)) return false;
    std::uint32_t length = static_cast<std::uint32_t>(head[0]) |
                           (static_cast<std::uint32_t>(head[1]) << 8) |
                           (static_cast<std::uint32_t>(head[2]) << 16) |
                           (static_cast<std::uint32_t>(head[3]) << 24);
    if (length == 0 || length > proto::MAX_FRAME_BYTES) return false;
    payload.resize(length);
    return readExact(payload.data(), length);
}

// 帧内游标 越界后 ok 变为 false
struct Cursor {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
    bool ok = true;

    bool remain(std::size_t n) const { return ok && pos + n <= size; }
    std::uint8_t u8() {
        if (!remain(1)) { ok = false; return 0; }
        return data[pos++];
    }
    std::uint16_t u16() {
        if (!remain(2)) { ok = false; return 0; }
        std::uint16_t low = data[pos];
        std::uint16_t high = data[pos + 1];
        pos += 2;
        return static_cast<std::uint16_t>(low | (high << 8));
    }
    std::uint32_t u32() {
        if (!remain(4)) { ok = false; return 0; }
        std::uint32_t value = static_cast<std::uint32_t>(data[pos]) |
                              (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
                              (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
                              (static_cast<std::uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return value;
    }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    std::string text(std::size_t length) {
        if (!remain(length)) { ok = false; return {}; }
        std::string result(reinterpret_cast<const char *>(data + pos), length);
        pos += length;
        return result;
    }
    std::string id() {
        std::uint8_t length = u8();
        return text(length);
    }
};

// 玩家信息
struct PlayerInfo {
    int id = 0;
    int team = 0;
};

// AI 状态
struct State {
    int selfId = -1;
    int selfTeam = -1;
    std::string selfName;
    std::vector<PlayerInfo> players;
    int rows = 0;
    int cols = 0;
    std::vector<int> viewTerrain;
    std::vector<int> viewOwner;
    std::vector<int> viewUnits;
    std::vector<int> memoryTerrain;
    std::vector<int> memoryOwner;
    std::vector<int> memoryUnits;
    bool ready = false;
    std::uint32_t turn = 0;

    int cellCount() const { return rows * cols; }
    bool inside(int x, int y) const { return x >= 0 && x < rows && y >= 0 && y < cols; }
    int index(int x, int y) const { return x * cols + y; }
};

// 动作 空动作表示 PASS
struct Action {
    bool move = false;
    int from = -1;
    int to = -1;
};

// 读取 map_payload 并更新视野与记忆
bool readMapPayload(Cursor &cursor, State &state) {
    int count = state.cellCount();
    if (count <= 0) return false;
    if (static_cast<int>(state.viewTerrain.size()) != count) {
        state.viewTerrain.assign(count, terrain::FOG_BLANK);
        state.viewOwner.assign(count, -2);
        state.viewUnits.assign(count, 0);
        state.memoryTerrain.assign(count, -1);
        state.memoryOwner.assign(count, -2);
        state.memoryUnits.assign(count, 0);
    }
    for (int i = 0; i < count; ++i) state.viewTerrain[i] = cursor.u8();
    for (int i = 0; i < count; ++i) state.viewOwner[i] = cursor.i16();
    for (int i = 0; i < count; ++i) state.viewUnits[i] = static_cast<int>(cursor.u32());
    if (!cursor.ok) return false;
    for (int i = 0; i < count; ++i) {
        if (state.viewOwner[i] != -2) {
            state.memoryTerrain[i] = state.viewTerrain[i];
            state.memoryOwner[i] = state.viewOwner[i];
            state.memoryUnits[i] = state.viewUnits[i];
        } else if (state.memoryTerrain[i] < 0) {
            state.memoryTerrain[i] = state.viewTerrain[i];
        }
    }
    return true;
}

bool handlePlayerSelf(Cursor &cursor, State &state) {
    state.selfId = cursor.u16();
    state.selfTeam = cursor.u16();
    state.selfName = cursor.id();
    return cursor.ok;
}

bool handlePlayers(Cursor &cursor, State &state) {
    std::uint16_t count = cursor.u16();
    std::vector<PlayerInfo> players;
    players.reserve(count);
    for (std::uint16_t i = 0; i < count && cursor.ok; ++i) {
        PlayerInfo player;
        player.id = cursor.u16();
        player.team = cursor.u16();
        (void) cursor.id();
        players.push_back(player);
    }
    if (!cursor.ok) return false;
    state.players = std::move(players);
    return true;
}

bool handleMapMeta(Cursor &cursor, State &state) {
    int rows = cursor.u16();
    int cols = cursor.u16();
    if (!cursor.ok || rows <= 0 || cols <= 0) return false;
    if (rows != state.rows || cols != state.cols) {
        state.rows = rows;
        state.cols = cols;
        state.viewTerrain.clear();
        state.viewOwner.clear();
        state.viewUnits.clear();
        state.memoryTerrain.clear();
        state.memoryOwner.clear();
        state.memoryUnits.clear();
    }
    return true;
}

// TURN 消息 本演示只需要视野地图 其余字段照读跳过
bool handleTurn(Cursor &cursor, State &state) {
    std::uint32_t turn = cursor.u32();
    std::uint16_t playerCount = cursor.u16();
    for (std::uint16_t i = 0; i < playerCount && cursor.ok; ++i) {
        (void) cursor.u16();
        (void) cursor.u16();
        (void) cursor.u8();
        (void) cursor.u32();
        (void) cursor.u32();
    }
    std::uint16_t teamCount = cursor.u16();
    for (std::uint16_t i = 0; i < teamCount && cursor.ok; ++i) {
        (void) cursor.u16();
        (void) cursor.u16();
    }
    if (!cursor.ok) return false;
    if (!readMapPayload(cursor, state)) return false;
    state.turn = turn;
    return true;
}

// ==================== 地图工具 ====================

bool isFriendlyOwner(const State &state, int owner) {
    if (owner < 0) return false;
    if (owner == state.selfId) return true;
    if (state.selfTeam < 0) return false;
    for (const PlayerInfo &player : state.players) {
        if (player.id == owner) return player.team == state.selfTeam;
    }
    return false;
}

bool isEnemyOwner(const State &state, int owner) {
    return owner >= 0 && !isFriendlyOwner(state, owner);
}

bool isVisible(const State &state, int idx) {
    return state.viewOwner[idx] != -2;
}

int currentOwner(const State &state, int idx) {
    if (state.viewOwner[idx] != -2) return state.viewOwner[idx];
    return state.memoryOwner[idx];
}

int currentUnits(const State &state, int idx) {
    if (isVisible(state, idx)) return state.viewUnits[idx];
    return state.memoryUnits[idx];
}

int currentTerrain(const State &state, int idx) {
    if (isVisible(state, idx)) return state.viewTerrain[idx];
    int remembered = state.memoryTerrain[idx];
    return remembered >= 0 ? remembered : terrain::FOG_BLANK;
}

bool isPassable(int terrainCode) {
    return terrainCode != terrain::MOUNTAIN && terrainCode != terrain::FOG_MOUNTAIN;
}

// 从若干起点做 BFS 返回距离场 绕开山区与疑似山区
std::vector<int> bfsDistance(const State &state, const std::vector<int> &seeds) {
    int count = state.cellCount();
    std::vector<int> dist(count, INF);
    std::queue<int> queue;
    for (int seed : seeds) {
        if (seed < 0 || seed >= count || dist[seed] != INF) continue;
        dist[seed] = 0;
        queue.push(seed);
    }
    while (!queue.empty()) {
        int cur = queue.front();
        queue.pop();
        int x = cur / state.cols;
        int y = cur % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int nextX = x + DIR_X[dir];
            int nextY = y + DIR_Y[dir];
            if (!state.inside(nextX, nextY)) continue;
            int next = state.index(nextX, nextY);
            if (dist[next] != INF) continue;
            int terrainCode = state.memoryTerrain[next];
            if (terrainCode >= 0 && !isPassable(terrainCode)) continue;
            dist[next] = dist[cur] + 1;
            queue.push(next);
        }
    }
    return dist;
}

// ==================== 扩张型策略 ====================

// 收集中立可通行格 作为扩张目标
std::vector<int> collectNeutralSeeds(const State &state) {
    std::vector<int> seeds;
    int count = state.cellCount();
    for (int i = 0; i < count; ++i) {
        if (currentOwner(state, i) != -1) continue;
        if (isPassable(currentTerrain(state, i))) seeds.push_back(i);
    }
    return seeds;
}

// 选择本回合动作 优先占中立格 其次抢中立城市 再次机会攻击 最后向最近中立格推进
Action chooseAction(const State &state) {
    if (!state.ready || state.rows <= 0 || state.cols <= 0) return Action{};
    int count = state.cellCount();
    std::vector<int> distNeutral = bfsDistance(state, collectNeutralSeeds(state));

    int bestScore = 0;
    Action best;
    for (int from = 0; from < count; ++from) {
        // 协议要求动作源格必须为己方占领
        if (currentOwner(state, from) != state.selfId) continue;
        int unitsFrom = currentUnits(state, from);
        if (unitsFrom < 2) continue;
        int fromX = from / state.cols;
        int fromY = from % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int toX = fromX + DIR_X[dir];
            int toY = fromY + DIR_Y[dir];
            if (!state.inside(toX, toY)) continue;
            int to = state.index(toX, toY);
            if (!isVisible(state, to)) continue;
            int terrainTo = currentTerrain(state, to);
            if (!isPassable(terrainTo)) continue;
            int ownerTo = currentOwner(state, to);
            int unitsTo = currentUnits(state, to);
            if (isFriendlyOwner(state, ownerTo)) continue; // 扩张型不合并 直接找空地
            int moved = unitsFrom - 1;
            if (moved <= unitsTo) continue;
            int score = 0;
            if (terrainTo == terrain::CITY) {
                score = ownerTo == -1 ? 90000 + unitsFrom : 12000 + unitsFrom; // 中立城市优先
            } else if (ownerTo == -1) {
                score = 20000 + unitsFrom - unitsTo; // 中立空地
                if (terrainTo == terrain::SWAMP) score = 3000 + unitsFrom; // 沼泽价值低
            } else {
                // 敌人领地 只有优势很大才顺手打
                if (moved > unitsTo + 2) score = 8000 + unitsFrom;
            }
            if (score > bestScore) {
                bestScore = score;
                best = Action{true, from, to};
            }
        }
    }
    if (bestScore > 0) return best;

    // 不能直接扩张时 向最近的中立格方向推进 允许穿过自己的领地
    int bestAdvance = 0;
    Action advance;
    for (int from = 0; from < count; ++from) {
        if (currentOwner(state, from) != state.selfId) continue;
        int unitsFrom = currentUnits(state, from);
        if (unitsFrom < 3) continue;
        if (distNeutral[from] >= INF) continue;
        int fromX = from / state.cols;
        int fromY = from % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int toX = fromX + DIR_X[dir];
            int toY = fromY + DIR_Y[dir];
            if (!state.inside(toX, toY)) continue;
            int to = state.index(toX, toY);
            if (!isVisible(state, to)) continue;
            if (!isPassable(currentTerrain(state, to))) continue;
            if (!isFriendlyOwner(state, currentOwner(state, to))) continue;
            if (distNeutral[to] >= distNeutral[from]) continue;
            int gain = distNeutral[from] - distNeutral[to];
            int score = 500 + gain * 100 + unitsFrom;
            if (score > bestAdvance) {
                bestAdvance = score;
                advance = Action{true, from, to};
            }
        }
    }
    return bestAdvance > 0 ? advance : Action{};
}

// 输出动作并刷新 标准输出只允许出现动作行
void writeAction(const State &state, const Action &action) {
    if (!action.move) {
        std::fputs("PASS\n", stdout);
    } else {
        char line[64] = {};
        std::snprintf(line, sizeof(line), "M %d %d %d %d\n",
                      action.from / state.cols, action.from % state.cols,
                      action.to / state.cols, action.to % state.cols);
        std::fputs(line, stdout);
    }
    std::fflush(stdout);
}

int main() {
    // 文本模式会把字节 0x1A 当文件结束 还会把 CRLF 当换行 破坏二进制帧
    // 所以启动第一件事就是把标准输入与标准输出切换到二进制模式
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    State state;
    std::vector<std::uint8_t> frame;
    while (readFrame(frame)) {
        Cursor cursor{frame.data(), frame.size(), 0, true};
        std::uint8_t type = cursor.u8();
        if (!cursor.ok) continue;
        switch (type) {
            case proto::PLAYER_SELF:
                handlePlayerSelf(cursor, state);
                break;
            case proto::PLAYERS:
                handlePlayers(cursor, state);
                break;
            case proto::MAP_META:
                handleMapMeta(cursor, state);
                break;
            case proto::MAP:
                readMapPayload(cursor, state);
                break;
            case proto::INIT:
                state.ready = true;
                std::fputs("READY\n", stdout);
                std::fflush(stdout);
                break;
            case proto::TURN:
                handleTurn(cursor, state);
                break;
            case proto::GO:
                writeAction(state, chooseAction(state));
                break;
            case proto::GAME_OVER:
                return 0;
            default:
                break;
        }
    }
    return 0;
}

// ==================== 构建脚本提示数据 ====================
// 下面的注释行不是代码 是给 build.bat 读取的中文提示
// 提示行格式为 //@HINT:提示名:中文内容 修改提示时请保持这个格式
//@HINT:COMPILING:正在编译 demo_expand.cpp
//@HINT:NOGXX:未找到 g++ 编译器 请先安装 MinGW-w64 并把它的 bin 目录加入 PATH
//@HINT:NOGXXTIP:安装和配置方法见仓库 README.md
//@HINT:FAILED:编译失败 请查看上面的编译器错误信息
//@HINT:NOOUTPUT:编译器返回成功 但没有生成可执行文件
//@HINT:SUCCESS:编译成功 已生成可执行文件
