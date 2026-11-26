#include <windows.h>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <core/game.hpp>
#include <core/map_generator.hpp>
#include <io/io.hpp>
#include <ui/ui.hpp>
#include <include/logger.hpp>
#include <stdexcept>
#include <memory>

namespace gen { // namespace gen

namespace { // namespace 

namespace fs = std::filesystem;
using fs::path;
using std::string;
using io::Channel;
using core::Game;
using std::unique_ptr;
using std::make_unique;

const string BATTLE_DIRECTORY_STR = "battle";
const path BATTLE_DIRECTORY = path(BATTLE_DIRECTORY_STR);

void startGame(const string &playerName1, const string &playerName2) {
    unique_ptr<Channel> player1 = make_unique<io::AIChannel>(playerName1, 0, 0, playerName1+".exe");
    unique_ptr<Channel> player2 = make_unique<io::AIChannel>(playerName1, 1, 1, playerName2+".exe");
    const int SIZE = 20;
    Game game(SIZE, SIZE);
    game.registrePlayer(player1->playerID, player1->teamID);
    game.registrePlayer(player2->playerID, player2->teamID);
    game.initializeGame(core::Map_Generator::generator1v1Balancedly);
}

} // namespace

} // namespace gen

int main() {
    SetConsoleOutputCP(CP_UTF8); // 设为UTF-8编码
	gen::logger.info("Hello World!");
    system("pause");
    return 0;
}