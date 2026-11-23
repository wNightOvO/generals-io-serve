/*
    IO 总交互板块
*/

#pragma once
#include <include/logger.hpp>
#include <include/move_operation.hpp>
#include <filesystem>

namespace gen::io {

class Channel {
private:
    std::string id;
    int playerID, teamID;
public:
    Channel(std::string id, int playerID, int teamID): id(id), playerID(playerID), teamID(teamID) {}

    virtual ~Channel() {}

    virtual MoveOperation readMoveOperation() = 0;
};

class AIChannel : Channel {
private:
    std::filesystem::path path;
public:
    inline static const std::string BATTLE_FILE = "battle";

    AIChannel(std::string id, int playerID, int teamID, std::filesystem::path path): Channel(id, playerID, teamID), path(path) {}

    // 用 filename 构造 带后缀
    AIChannel(std::string id, int playerID, int teamID, std::string filename): AIChannel(id, playerID, teamID, std::filesystem::path(BATTLE_FILE) / filename) {}

    MoveOperation readMoveOperation() override;
};

}