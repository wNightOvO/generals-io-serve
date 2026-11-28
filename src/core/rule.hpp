/*
    游戏规则
    负责校验移动 执行移动 处理兵力生成与消失 处理玩家淘汰
*/

#pragma once
#include <include/map.hpp>
#include <include/move_operation.hpp>
#include <include/type.hpp>
#include <core/player.hpp>

namespace gen::core {

// 一次移动产生的效果 用于记录日志
struct MoveEffect {
    bool applied = false; // 是否实际执行
    int movedUnits = 0; // 移动的兵力数量
    bool capturedCapital = false; // 是否占领了敌方首都
    int defeatedPlayer = -1; // 被击败的玩家编号 -1 表示没有
};

// 规则类 全部函数为静态函数 不保存状态
class Rule {
public:
    // 计算一次移动实际移动的兵力 参数 sourceUnits 为源格兵力 参数 half 为是否一半移动
    // 普通移动返回 sourceUnits - 1 一半移动返回 sourceUnits - sourceUnits / 2
    static int moveUnits(int sourceUnits, bool half);

    // 判断移动是否合法 参数 map 为地图 参数 playerID 为玩家编号 参数 op 为移动
    static bool isValidMove(const GameMap<Terrain> &map, int playerID, const MoveOperation &op);

    // 执行一次移动 自动校验 非法时不做任何修改并返回 applied 为 false 的效果
    // 参数 turn 为当前回合 用于记录死亡回合
    static MoveEffect applyMove(GameMap<Terrain> &map, PlayerRegistry &registry, const MoveOperation &op, int turn);

    // 执行回合结束后的兵力生成与消失 参数 turn 为当前回合
    static void applyEconomy(GameMap<Terrain> &map, PlayerRegistry &registry, int turn);

    // 让玩家判负并移出对局 该玩家全部格子变为中立且兵力清零 参数 turn 为判负回合
    static void eliminatePlayer(GameMap<Terrain> &map, PlayerRegistry &registry, int playerID, int turn);
};

} // namespace gen::core
