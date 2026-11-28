/*
    玩家操作记录实现
    把每一回合的玩家动作和地图变化写成中文纯文本 便于人直接阅读回看
    文件编码 UTF-8 每写一段立即刷新
*/

#include <core/recorder.hpp>
#include <include/logger.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <system_error>
#include <vector>

namespace gen::core {

namespace { // 匿名命名空间

// 返回当前时间的可读字符串 格式为 年-月-日 时:分:秒
std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm timeBuffer{};
    std::tm *local = std::localtime(&seconds);
    if(local != nullptr) timeBuffer = *local;
    char text[64] = {0};
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d",
                  timeBuffer.tm_year + 1900, timeBuffer.tm_mon + 1, timeBuffer.tm_mday,
                  timeBuffer.tm_hour, timeBuffer.tm_min, timeBuffer.tm_sec);
    return std::string(text);
}

// 在玩家注册表中按编号查找玩家 找不到返回 nullptr
const Player* findPlayer(const PlayerRegistry &registry, int playerID) {
    for(const Player &player : registry.players()) {
        if(player.playerID == playerID) return &player;
    }
    return nullptr;
}

// 返回记录文件用的长分割线
const char* longSeparator() {
    return "============================================================";
}

// 返回记录文件用的短分割线
const char* shortSeparator() {
    return "------------------------------------------------------------";
}

} // 匿名命名空间

MoveRecorder::~MoveRecorder() {
    close();
}

bool MoveRecorder::open(const std::filesystem::path &filePath, bool enabled) {
    close();
    path_ = filePath;
    enabled_ = enabled;
    if(enabled_ == false) {
        logger.info("操作记录未启用 不写入文件");
        return false;
    }
    if(path_.empty()) {
        logger.error("操作记录文件路径为空 无法打开");
        enabled_ = false;
        return false;
    }
    std::error_code error;
    std::filesystem::path parent = path_.parent_path();
    if(parent.empty() == false) {
        std::filesystem::create_directories(parent, error);
        if(error) {
            logger.error("无法创建操作记录目录 " + parent.string() + " 原因 " + error.message());
            enabled_ = false;
            return false;
        }
    }
    // 以清空方式打开 每个对局一个文件
    out_.open(path_, std::ios::out | std::ios::trunc);
    if(out_.is_open() == false) {
        logger.error("无法打开操作记录文件 " + path_.string());
        enabled_ = false;
        return false;
    }
    logger.info("操作记录文件已创建 路径 " + path_.string());
    return true;
}

void MoveRecorder::close() {
    if(out_.is_open()) {
        out_.flush();
        out_.close();
    }
    enabled_ = false;
}

std::string MoveRecorder::tileText(const Tile<Terrain> &tile) const {
    std::string ownerText;
    if(tile.owner == NEUTRAL) ownerText = "中立";
    else if(tile.owner == UNKNOWN) ownerText = "未知";
    else ownerText = "玩家 " + std::to_string(tile.owner);
    return std::string(terrainToChinese(tile.terrain)) + " 归属 " + ownerText
           + " 兵力 " + std::to_string(tile.unit);
}

void MoveRecorder::beginGame(int matchNumber, int gameNumber, const std::string &modeName,
                             int rows, int cols, std::uint32_t seed,
                             const PlayerRegistry &registry) {
    if(out_.is_open() == false) return;
    out_ << longSeparator() << '\n';
    out_ << "generals.io 对局操作记录" << '\n';
    out_ << "生成时间 " << currentTime() << " 文件编码 UTF-8" << '\n';
    out_ << "比赛序号 " << matchNumber << " 局序号 " << gameNumber << '\n';
    out_ << "模式 " << modeName << '\n';
    out_ << "地图大小 " << rows << " 行 " << cols << " 列" << '\n';
    out_ << "随机种子 " << seed << '\n';
    out_ << "玩家数量 " << registry.playerCount()
         << " 队伍数量 " << registry.teamCount() << '\n';
    for(const Player &player : registry.players()) {
        out_ << "玩家 " << player.playerID << " " << player.name
             << " 队伍 " << player.teamID << '\n';
    }
    for(const Team &team : registry.teams()) {
        out_ << "队伍 " << team.teamID << " 成员";
        for(int playerID : team.players) out_ << " " << playerID;
        out_ << '\n';
    }
    out_ << "说明 移动动作中的兵力为本次移动的兵力 一半兵力表示只移动一半" << '\n';
    out_ << longSeparator() << '\n';
    out_.flush();
}

void MoveRecorder::recordTurn(int turn, const std::vector<ActionRecord> &actions,
                              const GameMap<Terrain> &before, const GameMap<Terrain> &after,
                              const PlayerRegistry &registry) {
    if(out_.is_open() == false) return;
    out_ << shortSeparator() << '\n';
    out_ << "第 " << turn << " 回合" << '\n';
    int actionLines = 0;
    for(const ActionRecord &action : actions) {
        const Player *player = findPlayer(registry, action.playerID);
        bool hasInfo = action.hasMove || action.pass || action.note.empty() == false;
        // 已经淘汰且没有任何动作信息的玩家不重复记录 避免文件充满无意义内容
        if(player != nullptr && player->alive == false && hasInfo == false) continue;
        std::string text;
        if(player == nullptr) text = "玩家 " + std::to_string(action.playerID) + " 未知";
        else text = "玩家 " + std::to_string(action.playerID) + " " + player->name;
        if(action.pass) {
            text += " 本回合不移动";
        } else if(action.hasMove) {
            // 按移动规则推算本次移动兵力 普通移动预留 1 兵力 一半移动预留一半
            int movedUnits = 0;
            bool unitsKnown = before.isInside(action.source);
            if(unitsKnown) {
                int sourceUnits = before.at(action.source).unit;
                if(action.half) movedUnits = sourceUnits - sourceUnits / 2;
                else movedUnits = sourceUnits - 1;
            }
            text += " 从 " + action.source.toString() + " 移动到 " + action.dest.toString();
            if(action.half) text += " 一半兵力 ";
            else text += " 兵力 ";
            if(unitsKnown) text += std::to_string(movedUnits);
            else text += "未知";
        } else {
            text += " 本回合无动作";
        }
        if(action.note.empty() == false) text += " 原因 " + action.note;
        out_ << text << '\n';
        ++actionLines;
    }
    if(actionLines == 0) out_ << "本回合没有玩家动作" << '\n';
    out_ << "地图变化" << '\n';
    if(before.R != after.R || before.C != after.C) {
        logger.error("操作记录发现地图大小不一致 修改前 "
                     + std::to_string(before.R) + " 行 " + std::to_string(before.C) + " 列 修改后 "
                     + std::to_string(after.R) + " 行 " + std::to_string(after.C) + " 列");
        out_ << "地图大小不一致 无法逐格比较" << '\n';
    } else {
        // 按行列顺序比较每一个格子 只输出发生变化的格子
        int changes = 0;
        for(int x = 0; x < before.R; ++x) {
            for(int y = 0; y < before.C; ++y) {
                const Tile<Terrain> &oldTile = before.at(x, y);
                const Tile<Terrain> &newTile = after.at(x, y);
                if(oldTile.terrain == newTile.terrain && oldTile.owner == newTile.owner
                   && oldTile.unit == newTile.unit) {
                    continue;
                }
                ++changes;
                Point point(x, y);
                out_ << point.toString() << " 旧 " << tileText(oldTile)
                     << " 新 " << tileText(newTile) << '\n';
            }
        }
        if(changes == 0) out_ << "本回合地图无变化" << '\n';
        else out_ << "本回合地图变化格子数 " << changes << '\n';
    }
    out_.flush();
}

void MoveRecorder::recordEvent(const std::string &text) {
    if(out_.is_open() == false) return;
    out_ << "事件 " << text << '\n';
    out_.flush();
}

void MoveRecorder::endGame(const std::vector<Rank> &playerRank, const std::vector<Rank> &teamRank,
                           bool byVictory) {
    if(out_.is_open() == false) return;
    out_ << shortSeparator() << '\n';
    out_ << "本局结束" << '\n';
    out_ << "结束原因 " << (byVictory ? "胜利条件达成 一方获胜" : "到达最大回合数") << '\n';
    std::vector<Rank> players = playerRank;
    std::sort(players.begin(), players.end(), [](const Rank &left, const Rank &right) {
        return left.rank < right.rank;
    });
    out_ << "最终玩家排名" << '\n';
    if(players.empty()) out_ << "无玩家排名" << '\n';
    for(const Rank &entry : players) {
        out_ << "第 " << entry.rank << " 名 玩家 " << entry.id << '\n';
    }
    std::vector<Rank> teams = teamRank;
    std::sort(teams.begin(), teams.end(), [](const Rank &left, const Rank &right) {
        return left.rank < right.rank;
    });
    out_ << "最终队伍排名" << '\n';
    if(teams.empty()) out_ << "无队伍排名" << '\n';
    for(const Rank &entry : teams) {
        out_ << "第 " << entry.rank << " 名 队伍 " << entry.id << '\n';
    }
    out_ << longSeparator() << '\n';
    out_.flush();
}

} // namespace gen::core
