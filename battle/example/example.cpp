/*
    generals.io 示例 AI 参考实现

    本文件是一个完整可运行的 AI 程序 用来给后来写 AI 的人参考

    一 如何编译
        在 battle/example 目录下双击 build.bat
        或者手动执行下面这一行命令
        g++ -std=gnu++20 -O2 -static -finput-charset=UTF-8 -fexec-charset=UTF-8 -o example.exe example.cpp
        编译完成后得到 battle/example/example.exe 游戏程序会把它当作玩家 example
        本程序不需要任何命令行参数 直接启动即可 游戏程序会设置好工作目录为 battle/example

    二 读取游戏程序消息
        游戏程序通过标准输入 stdin 发给 AI 的消息是长度前缀帧
        格式为 uint32 length 加 uint8 payload[length]
        所有整数都是小端序 payload[0] 是消息类型
        读一帧必须做两件事
        第一 先读满 4 字节长度
        第二 再循环读满 length 字节 不能假设一次读取一定读满
        本文件用 readExact 与 readFrame 完成这两步 并处理短读
        Windows 下还必须先用 _setmode 把标准输入输出切换到二进制模式
        否则文本模式会把字节 0x1A 当成文件结束 还会把 CRLF 当成一个换行 破坏二进制帧
        消息类型与负载格式见 readFrame 下面的处理函数以及仓库的选手须知
        初始化流程为 PLAYER_SELF PLAYERS MAP_META MAP INIT
        收到 INIT 后要输出一行 READY 并立即刷新
        之后每回合先收到 TURN 再收到 GO 收到 GO 后 50 毫秒内输出一行动作并刷新

    三 向游戏程序写动作
        游戏程序读取 AI 的标准输出 stdout 每回合只读取第一行
        动作行只允许 PASS M x1 y1 x2 y2 H x1 y1 x2 y2 三种
        坐标从 0 开始 x 为行 y 为列
        M 为普通移动 源格保留 1 个兵力 移动 u - 1 个兵力
        H 为一半移动 源格保留 floor(u / 2) 个兵力 移动 u - floor(u / 2) 个兵力
        没有把握时输出 PASS
        标准输出只能出现 READY 与动作行 调试信息只能写标准错误 stderr
        本文件把 stdout 的写入集中到 writeAction 与初始化处 便于检查

    四 策略思路
        1 维护记忆地图 与本回合视野合并 得到已知地形 已知归属 已知兵力
        2 用 BFS 计算三张距离场 到敌方 到中立城市 到可扩张中立格
        3 枚举己方每个兵力大于等于 2 的格子 计算向四个方向移动的得分
        4 得分优先级 敌方首都 中立城市 敌方城市 敌方领地 中立格 向敌方推进 合并
        5 选择得分最高的一个动作 没有合适动作就 PASS
        单回合决策只做 400 格的 BFS 与枚举 远小于 50 毫秒

    五 代码约定
        注释中文 少标点 空格分隔
        变量名首单词小写 后续单词首字母大写
        全部只用 C++ 标准库 静态编译 不依赖外部库
*/

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <string>
#include <vector>

// Windows 下需要把标准输入输出切换到二进制模式 见 main 函数开头的说明
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

// ==================== 协议常量 ====================

// 协议消息类型 编号与开发计划第五节的最终版协议一致
namespace proto {

inline constexpr std::uint8_t PLAYER_SELF = 1; // 自己的信息
inline constexpr std::uint8_t PLAYERS = 2;     // 全部玩家 按初始移动优先级从高到低
inline constexpr std::uint8_t MAP_META = 3;    // 地图大小
inline constexpr std::uint8_t MAP = 4;         // 初始视野地图
inline constexpr std::uint8_t INIT = 5;        // 初始化开始 AI 回复 READY
inline constexpr std::uint8_t TURN = 6;        // 回合状态
inline constexpr std::uint8_t GO = 7;          // 开始计时 AI 回复动作行
inline constexpr std::uint8_t GAME_OVER = 8;   // 最终排名 发送后关闭连接

// 单帧最大长度 防止读到异常长度后分配过大内存
inline constexpr std::uint32_t MAX_FRAME_BYTES = 64u * 1024u * 1024u;

} // namespace proto

// ==================== 地形编码 ====================

// 地形编码 0 到 7 与协议一致 0 到 4 为真实地形 5 到 7 为视野外疑似地形
namespace terrain {

inline constexpr int BLANK = 0;       // 空地
inline constexpr int MOUNTAIN = 1;    // 山区 不能进入
inline constexpr int SWAMP = 2;       // 沼泽 每回合减 1 个兵力 最低减到 1
inline constexpr int CITY = 3;        // 城市
inline constexpr int CAPITAL = 4;     // 首都
inline constexpr int FOG_BLANK = 5;   // 疑似空地 空地或首都
inline constexpr int FOG_MOUNTAIN = 6; // 疑似山区 山区或城市
inline constexpr int FOG_SWAMP = 7;   // 视野外沼泽 语义同沼泽

} // namespace terrain

// 四个移动方向的坐标增量 顺序为 上 下 左 右
constexpr int DIR_X[4] = {-1, 1, 0, 0};
constexpr int DIR_Y[4] = {0, 0, -1, 1};

// 距离场里表示不可达的无穷大
constexpr int INF = 1 << 20;

// 调试输出开关 默认关闭 打开时只写标准错误 绝不污染协议
constexpr bool DEBUG_LOG = false;

// 调试输出 参数 format 为格式串 用法同 fprintf
void debugLog(const char *format, ...) {
    if (!DEBUG_LOG) return; // 默认关闭 不会产生任何输出
    std::va_list args;
    va_start(args, format);
    std::fprintf(stderr, "[example] ");
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

// ==================== 字节读取层 ====================

// 循环读满 size 字节 处理短读
// 参数 dst 为目标缓冲区 参数 size 为需要读取的字节数
// 返回 true 表示读满 返回 false 表示输入结束或出错
bool readExact(std::uint8_t *dst, std::size_t size) {
    std::size_t got = 0;
    while (got < size) {
        std::size_t once = std::fread(dst + got, 1, size - got, stdin);
        if (once == 0) return false; // 标准输入关闭 游戏程序结束
        got += once;
    }
    return true;
}

// 读取一帧 先读 4 字节小端长度 再循环读满 payload
// 参数 payload 输出负载 其中 payload[0] 为消息类型
// 返回 true 表示读到完整一帧 返回 false 表示输入结束或长度非法
// 这一函数是本程序读取游戏程序消息的唯一入口 必须处理短读
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

// 帧内游标 按小端序读取字段 越界后 ok 变为 false
struct Cursor {
    const std::uint8_t *data = nullptr; // 负载首地址
    std::size_t size = 0;               // 负载字节数
    std::size_t pos = 0;                // 当前读取位置
    bool ok = true;                     // 是否仍然合法

    // 判断剩余字节是否足够 n 个
    bool remain(std::size_t n) const {
        return ok && pos + n <= size;
    }

    // 读 uint8
    std::uint8_t u8() {
        if (!remain(1)) {
            ok = false;
            return 0;
        }
        return data[pos++];
    }

    // 读小端 uint16
    std::uint16_t u16() {
        if (!remain(2)) {
            ok = false;
            return 0;
        }
        std::uint16_t low = data[pos];
        std::uint16_t high = data[pos + 1];
        pos += 2;
        return static_cast<std::uint16_t>(low | (high << 8));
    }

    // 读小端 uint32
    std::uint32_t u32() {
        if (!remain(4)) {
            ok = false;
            return 0;
        }
        std::uint32_t value = static_cast<std::uint32_t>(data[pos]) |
                              (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
                              (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
                              (static_cast<std::uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return value;
    }

    // 读小端 int16 负数用补码表示
    std::int16_t i16() {
        return static_cast<std::int16_t>(u16());
    }

    // 读小端 int32 负数用补码表示
    std::int32_t i32() {
        return static_cast<std::int32_t>(u32());
    }

    // 读 length 个字节的字符串 玩家 id 为 ASCII
    std::string text(std::size_t length) {
        if (!remain(length)) {
            ok = false;
            return {};
        }
        std::string result(reinterpret_cast<const char *>(data + pos), length);
        pos += length;
        return result;
    }

    // 读 uint8 长度前缀的玩家 id
    std::string id() {
        std::uint8_t length = u8();
        return text(length);
    }
};

// ==================== 数据结构 ====================

// 玩家信息 地图上的归属编号就是 playerId
struct PlayerInfo {
    int id = 0;                // 玩家编号 从 0 开始
    int team = 0;              // 队伍编号 从 0 开始
    int alive = 1;             // 1 存活 0 已淘汰
    std::uint32_t tiles = 0;   // 当前占领格子数 仅 TURN 帧有效
    std::uint32_t units = 0;   // 当前总兵力 仅 TURN 帧有效
};

// 队伍排名 一条记录表示某队伍当前排第几 排名从 1 开始
struct TeamRank {
    int team = 0; // 队伍编号
    int rank = 0; // 排名 1 为第一名
};

// 动作种类 对应协议里的三种动作行
enum class ActionKind {
    PASS, // 不移动
    MOVE, // M x1 y1 x2 y2 普通移动 源格保留 1 个兵力
    HALF  // H x1 y1 x2 y2 一半移动 源格保留 floor(u / 2) 个兵力
};

// 一个动作 用格子下标表示 输出时再换算成行列坐标
struct Action {
    ActionKind kind = ActionKind::PASS;
    int from = -1; // 源格下标
    int to = -1;   // 目标格下标
};

// AI 维护的全部状态
struct State {
    int selfId = -1;          // 自己的玩家编号
    int selfTeam = -1;        // 自己的队伍编号
    std::string selfName;     // 自己的文件夹名

    std::vector<PlayerInfo> players; // 全部玩家 按移动优先级从高到低
    std::vector<TeamRank> teamRanks; // 当前队伍排名 从高到低

    int rows = 0; // 地图行数
    int cols = 0; // 地图列数

    // 本回合视野 地形编码 0 到 7 归属 -2 表示未知 -1 中立 其余为玩家编号 兵力未知为 0
    std::vector<int> viewTerrain;
    std::vector<int> viewOwner;
    std::vector<int> viewUnits;

    // 记忆地图 记录曾经看到的信息 视野外也能用于规划
    std::vector<int> memoryTerrain; // 记忆地形 0 到 4 为真实地形 5 到 7 为疑似地形 -1 为完全未知
    std::vector<int> memoryOwner;   // 最后一次看到的归属 -2 表示从未看到
    std::vector<int> memoryUnits;   // 最后一次看到的兵力

    bool ready = false;         // 是否已经收到 INIT 并完成初始化
    bool gameOver = false;      // 是否已经收到 GAME_OVER
    std::uint32_t turn = 0;     // 当前回合数 从 1 开始

    // 返回格子总数
    int cellCount() const {
        return rows * cols;
    }

    // 判断坐标是否在地图内
    bool inside(int x, int y) const {
        return x >= 0 && x < rows && y >= 0 && y < cols;
    }

    // 把行列坐标换算成行优先下标
    int index(int x, int y) const {
        return x * cols + y;
    }
};

// ==================== 地图负载解析 ====================

// 读取一个 map_payload 并刷新视野 同时更新记忆地图
// 参数 cursor 为帧内游标 参数 state 为 AI 状态
// 返回 true 表示解析成功 返回 false 表示负载不合法
// map_payload 格式为 uint8 terrain[R * C] 加 int16 owner[R * C] 加 uint32 units[R * C]
bool readMapPayload(Cursor &cursor, State &state) {
    int count = state.cellCount();
    if (count <= 0) return false;
    if (static_cast<int>(state.viewTerrain.size()) != count) {
        // 地图尺寸第一次出现或者变化时初始化全部数组
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

    // 合并到记忆地图
    // 视野内格子本次信息为真实值 直接覆盖记忆
    // 视野外格子只有疑似地形 首次看到时记录疑似地形 用于后续路径规划
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

// ==================== 协议消息处理 ====================

// 处理 PLAYER_SELF 消息 记录自己的编号 队伍 名字
bool handlePlayerSelf(Cursor &cursor, State &state) {
    state.selfId = cursor.u16();
    state.selfTeam = cursor.u16();
    state.selfName = cursor.id();
    debugLog("self id %d team %d name %s", state.selfId, state.selfTeam, state.selfName.c_str());
    return cursor.ok;
}

// 处理 PLAYERS 消息 记录全部玩家 顺序为初始移动优先级从高到低
bool handlePlayers(Cursor &cursor, State &state) {
    std::uint16_t count = cursor.u16();
    std::vector<PlayerInfo> players;
    players.reserve(count);
    for (std::uint16_t i = 0; i < count && cursor.ok; ++i) {
        PlayerInfo player;
        player.id = cursor.u16();
        player.team = cursor.u16();
        player.alive = 1;
        (void) cursor.id(); // 玩家名字对策略无用 读出只是为了消耗字节
        players.push_back(player);
    }
    if (!cursor.ok) return false;
    state.players = std::move(players);
    return true;
}

// 处理 MAP_META 消息 记录地图大小
bool handleMapMeta(Cursor &cursor, State &state) {
    int rows = cursor.u16();
    int cols = cursor.u16();
    if (!cursor.ok || rows <= 0 || cols <= 0) return false;
    if (rows != state.rows || cols != state.cols) {
        // 地图尺寸变化 清空旧数据 等待接下来的 map_payload 重新填充
        state.rows = rows;
        state.cols = cols;
        state.viewTerrain.clear();
        state.viewOwner.clear();
        state.viewUnits.clear();
        state.memoryTerrain.clear();
        state.memoryOwner.clear();
        state.memoryUnits.clear();
    }
    debugLog("map %d x %d", rows, cols);
    return true;
}

// 处理 MAP 消息 读取初始视野地图
bool handleMap(Cursor &cursor, State &state) {
    return readMapPayload(cursor, state);
}

// 处理 TURN 消息 读取回合数 玩家状态 队伍排名 视野地图
// 参数 cursor 为帧内游标 参数 state 为 AI 状态
// 返回 true 表示解析成功
bool handleTurn(Cursor &cursor, State &state) {
    std::uint32_t turn = cursor.u32();
    std::uint16_t playerCount = cursor.u16();
    std::vector<PlayerInfo> players;
    players.reserve(playerCount);
    for (std::uint16_t i = 0; i < playerCount && cursor.ok; ++i) {
        PlayerInfo player;
        player.id = cursor.u16();
        player.team = cursor.u16();
        player.alive = cursor.u8();
        player.tiles = cursor.u32();
        player.units = cursor.u32();
        players.push_back(player);
    }
    std::uint16_t teamCount = cursor.u16();
    std::vector<TeamRank> ranks;
    ranks.reserve(teamCount);
    for (std::uint16_t i = 0; i < teamCount && cursor.ok; ++i) {
        TeamRank rank;
        rank.team = cursor.u16();
        rank.rank = cursor.u16();
        ranks.push_back(rank);
    }
    if (!cursor.ok) return false;
    if (!readMapPayload(cursor, state)) return false;
    state.turn = turn;
    state.players = std::move(players);
    state.teamRanks = std::move(ranks);
    return true;
}

// 处理 GAME_OVER 消息 读取最终排名后程序退出
bool handleGameOver(Cursor &cursor, State &state) {
    std::uint16_t playerCount = cursor.u16();
    for (std::uint16_t i = 0; i < playerCount && cursor.ok; ++i) {
        int playerId = cursor.u16();
        int rank = cursor.u16();
        int deathTurn = cursor.i32();
        debugLog("game over player %d rank %d death %d", playerId, rank, deathTurn);
    }
    std::uint16_t teamCount = cursor.u16();
    for (std::uint16_t i = 0; i < teamCount && cursor.ok; ++i) {
        int teamId = cursor.u16();
        int rank = cursor.u16();
        debugLog("game over team %d rank %d", teamId, rank);
    }
    state.gameOver = true;
    return cursor.ok;
}

// ==================== 地图工具 ====================

// 判断归属是否为自己或队友 参数 owner 为归属编号 负数表示中立或未知
bool isFriendlyOwner(const State &state, int owner) {
    if (owner < 0) return false;
    if (owner == state.selfId) return true;
    if (state.selfTeam < 0) return false;
    for (const PlayerInfo &player : state.players) {
        if (player.id == owner) return player.team == state.selfTeam;
    }
    return false;
}

// 判断归属是否为敌人 参数 owner 为归属编号
bool isEnemyOwner(const State &state, int owner) {
    return owner >= 0 && !isFriendlyOwner(state, owner);
}

// 判断格子本回合是否在视野内 协议规定未知格 owner 为 -2
bool isVisible(const State &state, int idx) {
    return state.viewOwner[idx] != -2;
}

// 返回格子的已知归属 优先本回合视野 视野外使用记忆
int currentOwner(const State &state, int idx) {
    if (state.viewOwner[idx] != -2) return state.viewOwner[idx];
    return state.memoryOwner[idx];
}

// 返回格子的已知兵力 视野外使用最后一次看到的兵力
int currentUnits(const State &state, int idx) {
    if (isVisible(state, idx)) return state.viewUnits[idx];
    return state.memoryUnits[idx];
}

// 返回格子的已知地形 视野外使用记忆 完全未知时返回疑似空地
int currentTerrain(const State &state, int idx) {
    if (isVisible(state, idx)) return state.viewTerrain[idx];
    int remembered = state.memoryTerrain[idx];
    return remembered >= 0 ? remembered : terrain::FOG_BLANK;
}

// 判断地形能否进入 山区与疑似山区不可进入 疑似山区可能是城市 但保守绕开
bool isPassable(int terrainCode) {
    return terrainCode != terrain::MOUNTAIN && terrainCode != terrain::FOG_MOUNTAIN;
}

// 判断格子上下左右是否存在敌人
bool hasEnemyNeighbor(const State &state, int idx) {
    int x = idx / state.cols;
    int y = idx % state.cols;
    for (int dir = 0; dir < 4; ++dir) {
        int nextX = x + DIR_X[dir];
        int nextY = y + DIR_Y[dir];
        if (!state.inside(nextX, nextY)) continue;
        int owner = currentOwner(state, state.index(nextX, nextY));
        if (isEnemyOwner(state, owner)) return true;
    }
    return false;
}

// 从若干起点做 BFS 返回距离场 不可达为 INF
// 参数 seeds 为起点下标数组 参数 blockMountain 为 true 时绕开山区与疑似山区
// 返回每一格到最近起点的曼哈顿步数 用于向目标推进
std::vector<int> bfsDistance(const State &state, const std::vector<int> &seeds, bool blockMountain) {
    int count = state.cellCount();
    std::vector<int> dist(count, INF);
    if (count <= 0) return dist;
    std::queue<int> queue;
    for (int seed : seeds) {
        if (seed < 0 || seed >= count) continue;
        if (dist[seed] != INF) continue;
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
            if (blockMountain && terrainCode >= 0 && !isPassable(terrainCode)) continue;
            dist[next] = dist[cur] + 1;
            queue.push(next);
        }
    }
    return dist;
}

// 收集敌方相关起点 包含敌方占领格与敌方首都 用于计算向敌人的距离场
std::vector<int> collectEnemySeeds(const State &state) {
    std::vector<int> seeds;
    int count = state.cellCount();
    for (int i = 0; i < count; ++i) {
        if (isEnemyOwner(state, currentOwner(state, i))) seeds.push_back(i);
    }
    return seeds;
}

// 收集中立城市起点
std::vector<int> collectNeutralCitySeeds(const State &state) {
    std::vector<int> seeds;
    int count = state.cellCount();
    for (int i = 0; i < count; ++i) {
        int owner = currentOwner(state, i);
        if (owner != -1) continue; // 只关心中立格
        if (currentTerrain(state, i) == terrain::CITY) seeds.push_back(i);
    }
    return seeds;
}

// 收集中立可通行格起点 用于扩张距离场
std::vector<int> collectNeutralSeeds(const State &state) {
    std::vector<int> seeds;
    int count = state.cellCount();
    for (int i = 0; i < count; ++i) {
        if (currentOwner(state, i) != -1) continue;
        int terrainCode = currentTerrain(state, i);
        if (isPassable(terrainCode)) seeds.push_back(i);
    }
    return seeds;
}

// 收集从未见过的格子起点 用于还没有发现敌人时的探索距离场
std::vector<int> collectUnknownSeeds(const State &state) {
    std::vector<int> seeds;
    int count = state.cellCount();
    for (int i = 0; i < count; ++i) {
        if (state.memoryOwner[i] == -2) seeds.push_back(i);
    }
    return seeds;
}

// ==================== 策略决策 ====================

// 计算向敌方推进的收益 参数 distEnemy 为向敌人的距离场
// 返回从 from 走到 to 时距离减少了多少 减少越多越靠近敌人
int advanceGain(const std::vector<int> &distEnemy, int from, int to) {
    int fromDist = distEnemy[from];
    int toDist = distEnemy[to];
    if (fromDist >= INF || toDist >= INF) return 0;
    return fromDist - toDist;
}

// 计算占领一个目标的分数
// 参数 terrainTo 为目标地形 参数 ownerTo 为目标归属
// 参数 unitsFrom 为源格兵力 参数 advance 为向敌方推进的步数
// 分数越高越优先 敌方首都最高 然后中立城市 敌方城市 敌方领地 中立格
int scoreCapture(const State &state, int terrainTo, int ownerTo, int unitsFrom, int advance) {
    (void) state;
    if (terrainTo == terrain::CAPITAL && isEnemyOwner(state, ownerTo)) {
        return 1000000 + unitsFrom; // 占领敌方首都直接获胜
    }
    if (terrainTo == terrain::CITY) {
        if (ownerTo == -1) return 60000 + unitsFrom * 10 + advance * 100; // 中立城市 兵力产量高
        return 40000 + unitsFrom * 10 + advance * 100;                    // 敌方城市
    }
    if (terrainTo == terrain::SWAMP) {
        return 4000 + unitsFrom * 2 + advance * 100; // 沼泽会减兵 扩张价值低
    }
    if (ownerTo >= 0) return 30000 + unitsFrom * 10 + advance * 100; // 敌方普通领地
    return 8000 + unitsFrom * 5 + advance * 100; // 中立空地 优先向敌人方向扩张
}

// 计算把一个己方格子的兵力合并到相邻己方格子的分数
// 合并可以把分散兵力集中起来 但会放弃源格的兵力 所以只给较低的分数
// 参数 distEnemy 为向敌人的距离场 参数 useHalf 为是否保留一半兵力
// 参数 unitsTo 为目标格兵力 只在目标不比源格弱时合并 避免两个格子之间来回倒兵
int scoreMerge(const std::vector<int> &distEnemy, int from, int to, int unitsFrom, int unitsTo, bool useHalf) {
    int advance = advanceGain(distEnemy, from, to);
    if (advance > 0) {
        // 向敌人方向合并 相当于把兵力往前线集中
        return 600 + advance * 100 + unitsFrom * 2 + (useHalf ? 150 : 0);
    }
    if (unitsFrom >= 4 && unitsTo >= unitsFrom) {
        // 没有推进方向时 把兵力并进更大的己方堆 分数较低
        return 120 + unitsFrom + (useHalf ? 50 : 0);
    }
    return 0;
}

// 选择本回合动作 策略主函数
// 返回 PASS 表示不动 返回 MOVE 或 HALF 表示移动某个源格
// 决策顺序 先找能立刻占领的目标 没有目标时再向敌人或未探索区域推进
Action chooseAction(const State &state) {
    if (!state.ready || state.rows <= 0 || state.cols <= 0) return Action{};

    // 计算距离场 分别到敌人 到中立城市 到中立可扩张格 到未探索格
    std::vector<int> enemySeeds = collectEnemySeeds(state);
    std::vector<int> citySeeds = collectNeutralCitySeeds(state);
    std::vector<int> neutralSeeds = collectNeutralSeeds(state);
    std::vector<int> unknownSeeds = collectUnknownSeeds(state);
    std::vector<int> distEnemy = bfsDistance(state, enemySeeds, true);
    std::vector<int> distCity = bfsDistance(state, citySeeds, true);
    std::vector<int> distNeutral = bfsDistance(state, neutralSeeds, true);
    std::vector<int> distUnknown = bfsDistance(state, unknownSeeds, true);
    (void) distCity;
    (void) distNeutral;

    int bestScore = 0;
    Action bestAction;
    int count = state.cellCount();

    // 第一轮 枚举每一个己方格子的四个方向 找能立刻占领或合并的目标
    for (int from = 0; from < count; ++from) {
        // 开发计划的协议要求动作源格必须为己方占领 所以只用自己编号的格子
        if (currentOwner(state, from) != state.selfId) continue;
        int unitsFrom = currentUnits(state, from);
        if (unitsFrom < 2) continue; // 兵力小于 2 无法移动 协议要求源格兵力大于等于 2
        bool sourceBorder = hasEnemyNeighbor(state, from); // 源格是否贴着敌人
        int fromX = from / state.cols;
        int fromY = from % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int toX = fromX + DIR_X[dir];
            int toY = fromY + DIR_Y[dir];
            if (!state.inside(toX, toY)) continue;
            int to = state.index(toX, toY);
            int terrainTo = currentTerrain(state, to);
            if (terrainTo == terrain::MOUNTAIN) continue; // 山区不能进入
            if (!isVisible(state, to)) continue;          // 相邻格按规则一定可见 不可见时保守跳过
            int ownerTo = currentOwner(state, to);
            int unitsTo = currentUnits(state, to);

            if (isFriendlyOwner(state, ownerTo)) {
                // 目标是己方或队友 移动后兵力相加 归属不变
                // 如果源格贴着敌人 用一半移动保留 floor(u/2) 个兵力防守
                bool useHalf = sourceBorder && unitsFrom >= 4;
                int score = scoreMerge(distEnemy, from, to, unitsFrom, unitsTo, useHalf);
                if (score > bestScore) {
                    bestScore = score;
                    bestAction = Action{useHalf ? ActionKind::HALF : ActionKind::MOVE, from, to};
                }
            } else {
                // 目标是敌方或中立 普通移动带走 u - 1 个兵力 只有能占领才考虑
                int moved = unitsFrom - 1;
                if (moved <= unitsTo) continue; // 兵力不足 不送死
                int advance = advanceGain(distEnemy, from, to);
                int score = scoreCapture(state, terrainTo, ownerTo, unitsFrom, advance);
                if (enemySeeds.empty()) {
                    // 还没有发现任何敌人时 优先向未探索区域扩张 避免在小区域里打转
                    score += advanceGain(distUnknown, from, to) * 60;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestAction = Action{ActionKind::MOVE, from, to};
                }
            }
        }
    }
    if (bestScore > 0) return bestAction;

    // 第二轮 没有能立刻占领的目标 让兵力沿距离场推进 允许穿过自己的领地
    // 有敌人信息时目标为敌人 否则目标为未探索区域
    const std::vector<int> &distGoal = enemySeeds.empty() ? distUnknown : distEnemy;
    int bestPathScore = 0;
    Action pathAction;
    for (int from = 0; from < count; ++from) {
        if (currentOwner(state, from) != state.selfId) continue;
        int unitsFrom = currentUnits(state, from);
        if (unitsFrom < 2) continue;
        if (distGoal[from] >= INF) continue;
        bool sourceBorder = hasEnemyNeighbor(state, from);
        int fromX = from / state.cols;
        int fromY = from % state.cols;
        for (int dir = 0; dir < 4; ++dir) {
            int toX = fromX + DIR_X[dir];
            int toY = fromY + DIR_Y[dir];
            if (!state.inside(toX, toY)) continue;
            int to = state.index(toX, toY);
            if (!isVisible(state, to)) continue;
            if (!isPassable(currentTerrain(state, to))) continue;
            if (!isFriendlyOwner(state, currentOwner(state, to))) continue; // 只穿过自己的领地
            if (distGoal[to] >= distGoal[from]) continue;                   // 必须更接近目标
            int gain = distGoal[from] - distGoal[to];
            int score = 800 + gain * 200 + unitsFrom * 2;
            // 源格贴着敌人时用一半移动 保留 floor(u/2) 兵力防守
            bool useHalf = sourceBorder && unitsFrom >= 4;
            if (score > bestPathScore) {
                bestPathScore = score;
                pathAction = Action{useHalf ? ActionKind::HALF : ActionKind::MOVE, from, to};
            }
        }
    }
    return bestPathScore > 0 ? pathAction : Action{};
}

// ==================== 动作输出 ====================

// 把动作写成协议要求的一行并立即刷新标准输出
// 参数 state 为 AI 状态 用于把格子下标换算成行列坐标
// 标准输出只能出现 READY 和一行动作 这里是动作输出的唯一入口
void writeAction(const State &state, const Action &action) {
    switch (action.kind) {
        case ActionKind::MOVE:
        case ActionKind::HALF: {
            int fromX = action.from / state.cols;
            int fromY = action.from % state.cols;
            int toX = action.to / state.cols;
            int toY = action.to % state.cols;
            char line[64] = {};
            std::snprintf(line, sizeof(line), "%c %d %d %d %d\n",
                          action.kind == ActionKind::MOVE ? 'M' : 'H',
                          fromX, fromY, toX, toY);
            std::fputs(line, stdout);
            break;
        }
        case ActionKind::PASS:
        default:
            std::fputs("PASS\n", stdout);
            break;
    }
    std::fflush(stdout); // 每回合必须立即刷新 否则游戏程序收不到动作
}

// ==================== 主循环 ====================

// 程序入口 不需要命令行参数 直接与游戏程序通信
int main() {
    // 重要 Windows 的 C 运行库默认把标准流当文本模式
    // 文本模式会把字节 0x1A 当成文件结束 还会把 CRLF 当成一个换行
    // 二进制帧里随时可能出现 0x1A 与 CRLF 不切换模式会读到错误的帧甚至提前结束
    // 所以程序启动第一件事就是把标准输入与标准输出切换到二进制模式
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    State state;
    std::vector<std::uint8_t> frame;

    // 循环读取游戏程序的每一帧 读到输入结束为止
    while (readFrame(frame)) {
        Cursor cursor{frame.data(), frame.size(), 0, true};
        std::uint8_t type = cursor.u8();
        if (!cursor.ok) continue; // 空帧或损坏帧 跳过

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
                handleMap(cursor, state);
                break;
            case proto::INIT: {
                // 初始化开始 这里可以预计算 本示例只需要标记完成
                // 收到 INIT 后必须在 2 秒内输出一行 READY 并刷新
                state.ready = true;
                std::fputs("READY\n", stdout);
                std::fflush(stdout);
                debugLog("init done");
                break;
            }
            case proto::TURN:
                handleTurn(cursor, state);
                break;
            case proto::GO: {
                // 收到 GO 后开始计时 50 毫秒内必须输出动作行
                Action action = chooseAction(state);
                writeAction(state, action);
                break;
            }
            case proto::GAME_OVER:
                handleGameOver(cursor, state);
                return 0;
            default:
                // 未知消息类型 忽略整帧 保持向前兼容
                break;
        }
    }
    return 0;
}

// ==================== 构建脚本提示数据 ====================
// 下面的注释行不是代码 是给 build.bat 读取的中文提示
// build.bat 在缺少编译器或编译失败时用 findstr 从本文件取出对应提示
// 提示行格式为 //@HINT:提示名:中文内容 修改提示时请保持这个格式
//@HINT:COMPILING:正在编译 example.cpp
//@HINT:NOGXX:未找到 g++ 编译器 请先安装 MinGW-w64 并把它的 bin 目录加入 PATH
//@HINT:NOGXXTIP:安装和配置方法见仓库 README.md
//@HINT:FAILED:编译失败 请查看上面的编译器错误信息
//@HINT:NOOUTPUT:编译器返回成功 但没有生成可执行文件
//@HINT:SUCCESS:编译成功 已生成可执行文件
