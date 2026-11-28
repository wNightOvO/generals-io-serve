/*
    generals.io 演示玩家 demo_random 随机型 AI

    本文件是给 2v2 与 FFA 测试用的简单对手 自带完整协议读写代码 可独立编译
    策略特点 每回合在全部合法移动里随机选一个 用来测试协议与随机局面

    编译方法 双击本目录 build.bat
    或命令行执行
    g++ -std=gnu++20 -O2 -static -finput-charset=UTF-8 -fexec-charset=UTF-8 -o demo_random.exe demo_random.cpp

    协议要点
    游戏程序通过标准输入发长度前缀帧 uint32 length 加 payload
    payload[0] 为消息类型 所有整数小端序 读取时必须循环读满
    收到 INIT 后输出一行 READY 并刷新 每回合收到 GO 后输出一行动作并刷新
    动作只有 PASS 与 M x1 y1 x2 y2 两种 本演示使用普通移动
    标准输出只能有 READY 与动作行 调试信息只能写标准错误
*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
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

// AI 状态 随机型玩家只需要视野与自己的编号
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

// 读取 map_payload 更新视野
bool readMapPayload(Cursor &cursor, State &state) {
    int count = state.cellCount();
    if (count <= 0) return false;
    if (static_cast<int>(state.viewTerrain.size()) != count) {
        state.viewTerrain.assign(count, terrain::FOG_BLANK);
        state.viewOwner.assign(count, -2);
        state.viewUnits.assign(count, 0);
    }
    for (int i = 0; i < count; ++i) state.viewTerrain[i] = cursor.u8();
    for (int i = 0; i < count; ++i) state.viewOwner[i] = cursor.i16();
    for (int i = 0; i < count; ++i) state.viewUnits[i] = static_cast<int>(cursor.u32());
    return cursor.ok;
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
    }
    return true;
}

// TURN 消息 只保留视野地图与回合数
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

// ==================== 随机型策略 ====================

bool isFriendlyOwner(const State &state, int owner) {
    if (owner < 0) return false;
    if (owner == state.selfId) return true;
    if (state.selfTeam < 0) return false;
    for (const PlayerInfo &player : state.players) {
        if (player.id == owner) return player.team == state.selfTeam;
    }
    return false;
}

bool isVisible(const State &state, int idx) {
    return state.viewOwner[idx] != -2;
}

// 返回格子的地形 只使用本回合视野 相邻格在规则内一定可见
int visibleTerrain(const State &state, int idx) {
    return state.viewTerrain[idx];
}

// 一条候选移动 用格子下标表示
struct MoveCandidate {
    int from = 0;
    int to = 0;
};

// 收集全部合法移动
// 合法条件 源格为己方或队友 兵力大于等于 2 目标在界内 曼哈顿距离 1 不是山区
std::vector<MoveCandidate> collectLegalMoves(const State &state) {
    std::vector<MoveCandidate> moves;
    int count = state.cellCount();
    for (int from = 0; from < count; ++from) {
        if (!isVisible(state, from)) continue;
        if (state.viewOwner[from] != state.selfId) continue; // 协议要求动作源格为己方占领
        if (state.viewUnits[from] < 2) continue;
        int fromX = from / state.cols;
        int fromY = from % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int toX = fromX + DIR_X[dir];
            int toY = fromY + DIR_Y[dir];
            if (!state.inside(toX, toY)) continue;
            int to = state.index(toX, toY);
            if (!isVisible(state, to)) continue;
            if (visibleTerrain(state, to) == terrain::MOUNTAIN) continue;
            moves.push_back(MoveCandidate{from, to});
        }
    }
    return moves;
}

// 每回合用回合数做种子 保证同回合结果可复现 方便调试
Action chooseAction(const State &state) {
    if (!state.ready || state.rows <= 0 || state.cols <= 0) return Action{};
    std::vector<MoveCandidate> moves = collectLegalMoves(state);
    if (moves.empty()) return Action{};
    std::uint32_t seed = state.turn * 1000003u + static_cast<std::uint32_t>(state.selfId + 1) * 97u;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
    const MoveCandidate &chosen = moves[pick(rng)];
    return Action{true, chosen.from, chosen.to};
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
//@HINT:COMPILING:正在编译 demo_random.cpp
//@HINT:NOGXX:未找到 g++ 编译器 请先安装 MinGW-w64 并把它的 bin 目录加入 PATH
//@HINT:NOGXXTIP:安装和配置方法见仓库 README.md
//@HINT:FAILED:编译失败 请查看上面的编译器错误信息
//@HINT:NOOUTPUT:编译器返回成功 但没有生成可执行文件
//@HINT:SUCCESS:编译成功 已生成可执行文件
