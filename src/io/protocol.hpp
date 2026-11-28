/*
    通信协议
    负责把游戏状态编码为协议负载 消息构造规则见 选手须知.md
*/

#pragma once
#include <include/map.hpp>
#include <include/type.hpp>
#include <core/player.hpp>
#include <cstdint>
#include <vector>

namespace gen::io::protocol {

// 消息类型编号 与 选手须知.md 一致
inline constexpr std::uint8_t PLAYER_SELF = 1;
inline constexpr std::uint8_t PLAYERS = 2;
inline constexpr std::uint8_t MAP_META = 3;
inline constexpr std::uint8_t MAP = 4;
inline constexpr std::uint8_t INIT = 5;
inline constexpr std::uint8_t TURN = 6;
inline constexpr std::uint8_t GO = 7;
inline constexpr std::uint8_t GAME_OVER = 8;

// 一帧最大长度限制 超过视为程序错误
inline constexpr std::uint32_t MAX_FRAME_BYTES = 64u * 1024u * 1024u;

// 以下函数向缓冲区追加小端序整数
void appendU8(std::vector<std::uint8_t> &buffer, std::uint8_t value);
void appendU16(std::vector<std::uint8_t> &buffer, std::uint16_t value);
void appendU32(std::vector<std::uint8_t> &buffer, std::uint32_t value);
void appendI16(std::vector<std::uint8_t> &buffer, std::int16_t value);
void appendI32(std::vector<std::uint8_t> &buffer, std::int32_t value);

// 返回 PLAYER_SELF 消息负载 参数 self 为玩家自己
std::vector<std::uint8_t> playerSelfPayload(const core::Player &self);

// 返回 PLAYERS 消息负载 参数 orderedPlayers 按移动优先级从高到低
std::vector<std::uint8_t> playersPayload(const std::vector<const core::Player*> &orderedPlayers);

// 返回 MAP_META 消息负载
std::vector<std::uint8_t> mapMetaPayload(int rows, int cols);

// 返回地图负载 map_payload
std::vector<std::uint8_t> mapPayload(const core::GameMap<VisibleTerrain> &view);

// 返回 TURN 消息负载
// 参数 turn 为回合数 参数 orderedPlayers 按当前移动优先级从高到低
// 参数 teamRanks 为队伍排名 参数 view 为该队伍的视野
std::vector<std::uint8_t> turnPayload(int turn,
                                      const std::vector<const core::Player*> &orderedPlayers,
                                      const std::vector<Rank> &teamRanks,
                                      const core::GameMap<VisibleTerrain> &view);

// 返回 GAME_OVER 消息负载
// 参数 registry 为玩家注册表 参数 playerRank 与 teamRank 为最终排名
std::vector<std::uint8_t> gameOverPayload(const core::PlayerRegistry &registry,
                                          const std::vector<Rank> &playerRank,
                                          const std::vector<Rank> &teamRank);

} // namespace gen::io::protocol
