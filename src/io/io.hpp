/*
    IO 总交互板块
*/

#pragma once
#include <include/logger.hpp>
#include <include/move_operation.hpp>
#include <filesystem>

namespace gen::io {

class AIChannel {
private:
    std::string id;
    int playerID, teamID;
public:
};

}