/*
    IO 总交互板块
*/

#pragma once
#include <include/logger.hpp>
#include <include/move_operation.hpp>
#include <filesystem>
#include <vector>
#include <memory>

namespace gen::io {

class Channel {
private:
    std::string id;
    int playerID, teamID;
public:
    Channel(std::string id, int playerID, int teamID): id(id), playerID(playerID), teamID(teamID) {}

    int getPlayerID() const {
        return playerID;
    }

    int getTeamID() const {
        return teamID;
    }

    virtual ~Channel() {}

    virtual MoveOperation readMoveOperation() = 0;
};

// 注册表
class ChannelRegistry {
private:
    std::vector<std::unique_ptr<Channel>> channels;
public:
    ChannelRegistry() = default;
    ~ChannelRegistry() = default;

    void insert(Channel *channel) {
        channels.push_back(std::unique_ptr<Channel>(channel));
    }

    // 移动构造 会清空 ptr
    void insert(std::unique_ptr<Channel> ptr) {
        channels.push_back(std::move(ptr));
    }

    size_t size() const {
        return channels.size();
    }

    Channel* at(int i) {
        return channels.at(i).get();
    }

    const Channel* get(int i) const {
        return channels.at(i).get();
    }

    Channel* operator[] (int i) {
        return channels[i].get();
    }

    const Channel* operator[] (int i) const {
        return channels[i].get();
    }

    bool empty() const {
        return channels.empty();
    }

    /*
        迭代器部分
    */

    auto begin() {
        return channels.begin();
    }

    auto begin() const {
        return channels.cbegin();
    }

    auto end() {
        return channels.end();
    }

    auto end() const {
        return channels.cend();
    }
};

class AIChannel : public Channel {
private:
    std::filesystem::path path;
public:
    inline static const std::string BATTLE_DIRECTORY_STR = "battle";

    inline static const std::filesystem::path BATTLE_DIRECTORY = std::filesystem::path(BATTLE_DIRECTORY_STR);

    AIChannel(std::string id, int playerID, int teamID, std::filesystem::path path): Channel(id, playerID, teamID), path(path) {}

    // 用 filename 构造 带后缀
    AIChannel(std::string id, int playerID, int teamID, std::string filename): AIChannel(id, playerID, teamID, BATTLE_DIRECTORY / filename) {}

    std::filesystem::path getPath() const {
        return path;
    }

    MoveOperation readMoveOperation() override;
};

}