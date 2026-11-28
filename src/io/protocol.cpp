/*
    通信协议实现
    负责把游戏状态编码为小端序协议负载
    消息格式见 LLM_notice/开发计划.md 第五节
*/

#include <io/protocol.hpp>

#include <include/logger.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gen::io::protocol { // namespace gen::io::protocol

namespace { // namespace

// 玩家 id 最大字节数 协议规定不超过 64
constexpr std::size_t MAX_ID_BYTES = 64;

// 检查单帧负载长度 参数 payloadSize 为负载字节数 未超限返回 true
// 超过上限时记录错误日志并返回 false
bool checkFrameSize(std::size_t payloadSize) {
    if(payloadSize + 1 > MAX_FRAME_BYTES) {
        logger.error("协议负载超过单帧长度上限 负载字节数 " + std::to_string(payloadSize)
                     + " 上限 " + std::to_string(MAX_FRAME_BYTES));
        return false;
    }
    return true;
}

// 向缓冲区追加玩家 id 的长度与字节 超过 64 字节时截断
void appendPlayerName(std::vector<std::uint8_t> &payload, const std::string &name) {
    std::size_t length = std::min<std::size_t>(name.size(), MAX_ID_BYTES);
    appendU8(payload, static_cast<std::uint8_t>(length));
    payload.insert(payload.end(), name.begin(), name.begin() + static_cast<std::ptrdiff_t>(length));
}

// 把可能为负的统计值转成协议使用的 uint32 负数按 0 处理
std::uint32_t statToU32(int value) {
    if(value < 0) {
        logger.warning("协议统计值为负 按 0 处理 原值 " + std::to_string(value));
        return 0;
    }
    return static_cast<std::uint32_t>(value);
}

// 把编号或排名转成 uint16 越界时截断并记录警告 参数 field 为字段名
std::uint16_t smallToU16(int value, const char *field) {
    if(value < 0) {
        logger.warning("协议字段 " + std::string(field) + " 为负 按 0 处理 原值 " + std::to_string(value));
        return 0;
    }
    if(value > 0xFFFF) {
        logger.warning("协议字段 " + std::string(field) + " 超过 uint16 上限 按上限处理 原值 " + std::to_string(value));
        return 0xFFFF;
    }
    return static_cast<std::uint16_t>(value);
}

// 把条目数量转成 uint16 超过上限时截断并记录错误日志 参数 field 为字段名
std::uint16_t countToU16(std::size_t count, const char *field) {
    if(count > 0xFFFF) {
        logger.error("协议字段 " + std::string(field) + " 条目数量超过 uint16 上限 按上限截断 原数量 "
                     + std::to_string(count));
        return 0xFFFF;
    }
    return static_cast<std::uint16_t>(count);
}

} // namespace

void appendU8(std::vector<std::uint8_t> &buffer, std::uint8_t value) {
    buffer.push_back(value);
}

void appendU16(std::vector<std::uint8_t> &buffer, std::uint16_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void appendU32(std::vector<std::uint8_t> &buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void appendI16(std::vector<std::uint8_t> &buffer, std::int16_t value) {
    appendU16(buffer, static_cast<std::uint16_t>(value));
}

void appendI32(std::vector<std::uint8_t> &buffer, std::int32_t value) {
    appendU32(buffer, static_cast<std::uint32_t>(value));
}

std::vector<std::uint8_t> playerSelfPayload(const core::Player &self) {
    std::vector<std::uint8_t> payload;
    payload.reserve(8 + std::min<std::size_t>(self.name.size(), MAX_ID_BYTES));
    appendU16(payload, smallToU16(self.playerID, "玩家编号"));
    appendU16(payload, smallToU16(self.teamID, "队伍编号"));
    appendPlayerName(payload, self.name);
    checkFrameSize(payload.size());
    return payload;
}

std::vector<std::uint8_t> playersPayload(const std::vector<const core::Player*> &orderedPlayers) {
    const std::size_t playerCount = std::min<std::size_t>(orderedPlayers.size(), 0xFFFF);
    if(orderedPlayers.size() > 0xFFFF) {
        logger.error("PLAYERS 消息玩家数量超过 uint16 上限 按上限截断 原数量 " + std::to_string(orderedPlayers.size()));
    }
    std::vector<std::uint8_t> payload;
    payload.reserve(2 + playerCount * 8);
    appendU16(payload, static_cast<std::uint16_t>(playerCount));
    for(std::size_t i = 0; i < playerCount; i ++) {
        const core::Player *player = orderedPlayers[i];
        if(player == nullptr) {
            logger.error("PLAYERS 消息收到空玩家指针 下标 " + std::to_string(i));
            appendU16(payload, 0);
            appendU16(payload, 0);
            appendU8(payload, 0);
            continue;
        }
        appendU16(payload, smallToU16(player->playerID, "玩家编号"));
        appendU16(payload, smallToU16(player->teamID, "队伍编号"));
        appendPlayerName(payload, player->name);
    }
    checkFrameSize(payload.size());
    return payload;
}

std::vector<std::uint8_t> mapMetaPayload(int rows, int cols) {
    std::vector<std::uint8_t> payload;
    payload.reserve(4);
    appendU16(payload, smallToU16(rows, "地图行数"));
    appendU16(payload, smallToU16(cols, "地图列数"));
    return payload;
}

std::vector<std::uint8_t> mapPayload(const core::GameMap<VisibleTerrain> &view) {
    const std::size_t tileCount = static_cast<std::size_t>(view.R) * static_cast<std::size_t>(view.C);
    std::vector<std::uint8_t> payload;
    payload.reserve(tileCount * 7);
    // 地形 每格 1 字节 编码 0 到 7 与 VisibleTerrain 一致
    for(const auto &tile : view.tiles()) {
        appendU8(payload, static_cast<std::uint8_t>(static_cast<int>(tile.terrain)));
    }
    // 归属 每格 2 字节 -1 中立 -2 未知 其余为玩家编号
    for(const auto &tile : view.tiles()) {
        appendI16(payload, static_cast<std::int16_t>(tile.owner));
    }
    // 兵力 每格 4 字节 未知格由调用方填 0 负数按 0 处理
    for(const auto &tile : view.tiles()) {
        appendU32(payload, statToU32(tile.unit));
    }
    checkFrameSize(payload.size());
    return payload;
}

std::vector<std::uint8_t> turnPayload(int turn,
                                      const std::vector<const core::Player*> &orderedPlayers,
                                      const std::vector<Rank> &teamRanks,
                                      const core::GameMap<VisibleTerrain> &view) {
    const std::size_t playerCount = std::min<std::size_t>(orderedPlayers.size(), 0xFFFF);
    const std::size_t teamCount = std::min<std::size_t>(teamRanks.size(), 0xFFFF);
    const std::size_t tileCount = static_cast<std::size_t>(view.R) * static_cast<std::size_t>(view.C);
    if(orderedPlayers.size() > 0xFFFF) {
        logger.error("TURN 消息玩家数量超过 uint16 上限 按上限截断 原数量 " + std::to_string(orderedPlayers.size()));
    }
    if(teamRanks.size() > 0xFFFF) {
        logger.error("TURN 消息队伍数量超过 uint16 上限 按上限截断 原数量 " + std::to_string(teamRanks.size()));
    }
    std::vector<std::uint8_t> payload;
    payload.reserve(4 + 2 + playerCount * 13 + 2 + teamCount * 4 + tileCount * 7);
    if(turn < 0) {
        logger.warning("TURN 消息回合数为负 按 0 处理 原值 " + std::to_string(turn));
    }
    appendU32(payload, static_cast<std::uint32_t>(turn < 0 ? 0 : turn));
    appendU16(payload, static_cast<std::uint16_t>(playerCount));
    // 玩家状态 按当前移动优先级从高到低
    for(std::size_t i = 0; i < playerCount; i ++) {
        const core::Player *player = orderedPlayers[i];
        if(player == nullptr) {
            logger.error("TURN 消息收到空玩家指针 下标 " + std::to_string(i));
            appendU16(payload, 0);
            appendU16(payload, 0);
            appendU8(payload, 0);
            appendU32(payload, 0);
            appendU32(payload, 0);
            continue;
        }
        appendU16(payload, smallToU16(player->playerID, "玩家编号"));
        appendU16(payload, smallToU16(player->teamID, "队伍编号"));
        appendU8(payload, player->alive ? 1 : 0);
        appendU32(payload, statToU32(player->tiles));
        appendU32(payload, statToU32(player->units));
    }
    // 队伍排名 覆盖全部队伍 从高到低 rank 1 为第一名
    appendU16(payload, static_cast<std::uint16_t>(teamCount));
    for(std::size_t i = 0; i < teamCount; i ++) {
        appendU16(payload, smallToU16(teamRanks[i].id, "队伍编号"));
        appendU16(payload, smallToU16(teamRanks[i].rank, "队伍排名"));
    }
    // 最后追加视野地图
    std::vector<std::uint8_t> map = mapPayload(view);
    payload.insert(payload.end(), map.begin(), map.end());
    checkFrameSize(payload.size());
    return payload;
}

std::vector<std::uint8_t> gameOverPayload(const core::PlayerRegistry &registry,
                                          const std::vector<Rank> &playerRank,
                                          const std::vector<Rank> &teamRank) {
    const std::size_t playerCount = std::min<std::size_t>(playerRank.size(), 0xFFFF);
    const std::size_t teamCount = std::min<std::size_t>(teamRank.size(), 0xFFFF);
    const int registeredPlayers = registry.playerCount();
    if(playerRank.size() > 0xFFFF) {
        logger.error("GAME_OVER 消息玩家数量超过 uint16 上限 按上限截断 原数量 " + std::to_string(playerRank.size()));
    }
    if(teamRank.size() > 0xFFFF) {
        logger.error("GAME_OVER 消息队伍数量超过 uint16 上限 按上限截断 原数量 " + std::to_string(teamRank.size()));
    }
    std::vector<std::uint8_t> payload;
    payload.reserve(2 + playerCount * 8 + 2 + teamCount * 4);
    appendU16(payload, countToU16(playerRank.size(), "玩家排名"));
    // 玩家排名 从高到低 deathTurn 为 -1 表示存活
    for(std::size_t i = 0; i < playerCount; i ++) {
        const Rank &entry = playerRank[i];
        int deathTurn = -1;
        if(entry.id >= 0 && entry.id < registeredPlayers) {
            deathTurn = registry.players()[static_cast<std::size_t>(entry.id)].deathTurn;
        }
        else {
            logger.warning("GAME_OVER 消息包含非法玩家编号 " + std::to_string(entry.id) + " 死亡回合按 -1 处理");
        }
        appendU16(payload, smallToU16(entry.id, "玩家编号"));
        appendU16(payload, smallToU16(entry.rank, "玩家排名"));
        appendI32(payload, static_cast<std::int32_t>(deathTurn));
    }
    appendU16(payload, countToU16(teamRank.size(), "队伍排名"));
    // 队伍排名 从高到低 rank 1 为第一名
    for(std::size_t i = 0; i < teamCount; i ++) {
        appendU16(payload, smallToU16(teamRank[i].id, "队伍编号"));
        appendU16(payload, smallToU16(teamRank[i].rank, "队伍排名"));
    }
    checkFrameSize(payload.size());
    return payload;
}

} // namespace gen::io::protocol
