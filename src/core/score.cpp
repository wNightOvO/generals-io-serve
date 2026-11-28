/*
    计分板实现
    按局累计每名玩家的加权总分 并给出排名
*/

#include <core/score.hpp>
#include <include/logger.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace gen::core {

namespace { // 匿名命名空间

// 在排名表中查找编号对应的名次 找不到或名次非法时返回 0
int findRank(const std::vector<Rank> &rankList, int id) {
    for(const Rank &entry : rankList) {
        if(entry.id == id) return entry.rank;
    }
    return 0;
}

} // 匿名命名空间

void ScoreBoard::configure(double playerWeight, double teamWeight) {
    playerWeight_ = playerWeight;
    teamWeight_ = teamWeight;
}

void ScoreBoard::clear() {
    entries_.clear();
}

void ScoreBoard::addGame(const std::vector<std::string> &playerNames,
                         const std::vector<int> &playerTeams,
                         const std::vector<Rank> &playerRank,
                         const std::vector<Rank> &teamRank) {
    int playerCount = static_cast<int>(playerNames.size());
    // 队伍数量以队伍排名表长度为准 排名表为空时按玩家队伍编号推算
    int teamCount = static_cast<int>(teamRank.size());
    if(teamCount == 0) {
        for(int teamID : playerTeams) {
            if(teamID + 1 > teamCount) teamCount = teamID + 1;
        }
    }
    for(int playerID = 0; playerID < playerCount; ++playerID) {
        const std::string &playerName = playerNames[playerID];
        int playerPlace = findRank(playerRank, playerID);
        if(playerPlace < 1 || playerPlace > playerCount) {
            logger.error("计分跳过玩家 " + playerName + " 编号 " + std::to_string(playerID)
                         + " 原因 缺少有效玩家名次 名次 " + std::to_string(playerPlace));
            continue;
        }
        // 玩家分数 = 玩家数 - 玩家名次 + 1
        double playerScore = static_cast<double>(playerCount - playerPlace + 1);
        double teamScore = 0.0;
        if(teamWeight_ != 0.0) {
            if(playerID >= static_cast<int>(playerTeams.size())) {
                logger.error("计分跳过玩家 " + playerName + " 编号 " + std::to_string(playerID)
                             + " 原因 缺少队伍编号");
                continue;
            }
            int teamID = playerTeams[playerID];
            int teamPlace = findRank(teamRank, teamID);
            if(teamPlace < 1 || teamPlace > teamCount) {
                logger.error("计分跳过玩家 " + playerName + " 队伍 " + std::to_string(teamID)
                             + " 原因 缺少有效队伍名次 名次 " + std::to_string(teamPlace));
                continue;
            }
            // 队伍分数 = 队伍数 - 队伍名次 + 1
            teamScore = static_cast<double>(teamCount - teamPlace + 1);
        }
        double gameScore = playerWeight_ * playerScore + teamWeight_ * teamScore;
        // 按玩家名查找累计成绩 首次出现时新建
        ScoreEntry *entry = nullptr;
        for(ScoreEntry &item : entries_) {
            if(item.name == playerName) {
                entry = &item;
                break;
            }
        }
        if(entry == nullptr) {
            entries_.push_back(ScoreEntry());
            entry = &entries_.back();
            entry->name = playerName;
        }
        entry->total += gameScore;
        entry->games += 1;
        if(playerPlace == 1) entry->firstPlaces += 1;
        entry->lastGame = gameScore;
    }
}

std::vector<ScoreEntry> ScoreBoard::ranking() const {
    std::vector<ScoreEntry> result = entries_;
    std::sort(result.begin(), result.end(), [](const ScoreEntry &left, const ScoreEntry &right) {
        if(left.total != right.total) return left.total > right.total;
        return left.name < right.name;
    });
    for(int i = 0; i < static_cast<int>(result.size()); ++i) {
        result[i].rank = i + 1;
    }
    return result;
}

} // namespace gen::core
