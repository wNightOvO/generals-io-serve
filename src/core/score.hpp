/*
    计分板
    按局累计每名玩家的加权总分 并给出排名
*/

#pragma once
#include <include/type.hpp>
#include <string>
#include <vector>

namespace gen::core {

// 一名玩家的累计成绩
struct ScoreEntry {
    std::string name; // 玩家 id
    int rank = 0; // 名次 从 1 开始 由 ranking 计算
    double total = 0.0; // 加权总分
    int games = 0; // 参加局数
    int firstPlaces = 0; // 第一名次数
    double lastGame = 0.0; // 最近一局得分
};

// 计分板 记录本程序全部比赛的累计成绩
class ScoreBoard {
private:
    double playerWeight_ = 1.0; // 玩家分数权重
    double teamWeight_ = 0.0; // 队伍分数权重
    std::vector<ScoreEntry> entries_; // 全部玩家成绩 按首次出现顺序
public:
    // 设置权重 参数 playerWeight 为玩家分数权重 参数 teamWeight 为队伍分数权重
    void configure(double playerWeight, double teamWeight);

    // 清除全部成绩
    void clear();

    // 返回玩家分数权重
    double playerWeight() const {
        return playerWeight_;
    }

    // 返回队伍分数权重
    double teamWeight() const {
        return teamWeight_;
    }

    // 记录一局成绩
    // 参数 playerNames 按玩家编号排列的玩家 id
    // 参数 playerTeams 按玩家编号排列的玩家所属队伍编号
    // 参数 playerRank 本局玩家排名 playerRank 元素的 id 为玩家编号
    // 参数 teamRank 本局队伍排名 teamRank 元素的 id 为队伍编号
    void addGame(const std::vector<std::string> &playerNames,
                 const std::vector<int> &playerTeams,
                 const std::vector<Rank> &playerRank,
                 const std::vector<Rank> &teamRank);

    // 返回按总分降序排列的成绩 同分时按玩家 id 升序 已填好 rank
    std::vector<ScoreEntry> ranking() const;

    // 返回全部原始成绩
    const std::vector<ScoreEntry>& entries() const {
        return entries_;
    }
};

} // namespace gen::core
