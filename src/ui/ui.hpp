/*
    UI 渲染接口
    使用 EasyX 绘制地图 排名 和玩家信息
*/

#pragma once
#include <include/map.hpp>
#include <include/type.hpp>
#include <string>
#include <vector>

namespace gen::ui {

// 玩家展示信息
struct PlayerInfo {
    int playerID = 0; // 玩家编号
    int teamID = 0; // 队伍编号
    std::string name; // 玩家 id
    bool alive = true; // 是否存活
    int units = 0; // 兵力总数
    int tiles = 0; // 格子数
};

// 队伍展示信息
struct TeamInfo {
    int teamID = 0; // 队伍编号
    int rank = 0; // 名次 从 1 开始
    int units = 0; // 队伍兵力总数
    int tiles = 0; // 队伍格子数
};

// 一帧画面所需的全部数据
struct FrameInfo {
    int rows = 0; // 地图行数
    int cols = 0; // 地图列数
    int turn = 0; // 当前回合
    int maxTurns = 0; // 最大回合
    std::string modeName; // 模式名 例如 1v1
    int matchIndex = 1; // 当前比赛序号 从 1 开始
    int matchCount = 1; // 比赛总数
    int gameIndex = 1; // 当前局序号 从 1 开始
    int gameCount = 1; // 一场比赛的局数
    std::vector<PlayerInfo> players; // 全部玩家
    std::vector<TeamInfo> teams; // 全部队伍
    std::vector<Rank> playerRanks; // 玩家最终排名 游戏结束后使用
    std::vector<Rank> teamRanks; // 队伍最终排名 游戏结束后使用
    core::GameMap<VisibleTerrain> view; // 当前显示的地图视野
    bool gameOver = false; // 是否处于终局展示
    bool victory = false; // 终局是否因胜利结束
};

// 渲染器 全部函数为静态函数
class Renderer {
public:
    // 创建窗口 参数 rows cols 为地图大小 参数 cellSize 为格子边长
    static void open(int rows, int cols, int cellSize);

    // 关闭窗口
    static void close();

    // 窗口是否已经创建
    static bool isOpen();

    // 处理窗口消息和按键 必须每帧调用
    static void poll();

    // 绘制一帧
    static void render(const FrameInfo &frame);

    // 用户是否请求退出
    static bool quitRequested();

    // 是否处于暂停状态
    static bool paused();

    // 设置暂停状态
    static void setPaused(bool value);

    // 返回当前视野模式 -1 为全局视野 否则为队伍编号
    static int visionMode();

    // 设置视野模式 参数 value 为 -1 表示全局视野 否则为队伍编号
    static void setVisionMode(int value);

    // 返回当前每回合画面间隔 毫秒
    static int delayMs();

    // 调整每回合画面间隔 参数 deltaMs 为增量 可以为负
    static void adjustDelay(int deltaMs);
};

} // namespace gen::ui
