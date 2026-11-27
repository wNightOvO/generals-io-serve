#include <windows.h>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <vector>
#include <core/game.hpp>
#include <core/map_generator.hpp>
#include <include/move_operation.hpp>
#include <io/io.hpp>
#include <ui/ui.hpp>
#include <include/logger.hpp>
#include <stdexcept>
#include <memory>
#include <format>

namespace gen { // namespace gen

namespace { // namespace 

namespace fs = std::filesystem;
using fs::path;
using std::string;
using io::ChannelRegistry;
using io::Channel;
using io::AIChannel;
using core::Game;
using core::Rank;
using std::unique_ptr;
using std::make_unique;
using std::format;
using std::vector;

const string BATTLE_DIRECTORY_STR = "battle";
const path BATTLE_DIRECTORY = path(BATTLE_DIRECTORY_STR);
const int MAX_TURN = 1500;

// 用于创建AI玩家
Channel* createAIPlayer(const string &playerName, int playerID, int teamID) {
    logger.info(format("Function: createAIPlayer\nplayerName: {}  playerID: {}  teamID: {}\n", playerName, playerID, teamID));
    return new AIChannel(playerName, playerID, teamID, playerName+".exe"); 
} 

// 获得本轮的所有移动操作
void getMoveOperation(vector<MoveOperation> &v, const ChannelRegistry &channelRegistry) {
    v.clear();
    for(auto &channel : channelRegistry) {
        v.push_back(channel->readMoveOperation());
    }
}

void startGameSolo(const string &playerName1, const string &playerName2) {
    ChannelRegistry channelRegistry;
    
    // 创建两个AI玩家
    Channel *player1 = createAIPlayer(playerName1, 0, 0);
    channelRegistry.insert(player1);
    Channel *player2 = createAIPlayer(playerName2, 1, 1);
    channelRegistry.insert(player2);

    const int SIZE = 20;

    // 创建游戏
    Game game(SIZE, SIZE);

    // 注册玩家
    game.registrePlayer(player1->getPlayerID(), player1->getTeamID());
    game.registrePlayer(player2->getPlayerID(), player2->getTeamID());

    // 初始化地图
    game.initializeGame(core::Map_Generator::generator1v1Balancedly);

    while(game.getCurrentTurn() < MAX_TURN) {
        vector<MoveOperation> v;
        getMoveOperation(v, channelRegistry);
        bool gameover = game.nextTurn(v);
        if(gameover) break;
    }

    const vector<Rank> playerRank = game.getPlayerRank();
    const vector<Rank> teamRank = game.getTeamRank();

    
}

} // namespace

} // namespace gen

int main() {
    SetConsoleOutputCP(CP_UTF8); // 设为UTF-8编码
	gen::logger.info("Hello World!");
    system("pause");
    return 0;
}