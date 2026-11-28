/*
    程序总编排
    发现玩家 组织比赛 逐局运行 汇总排名
*/

#include <core/runner.hpp>

#include <core/game.hpp>
#include <core/map_generator.hpp>
#include <core/recorder.hpp>
#include <core/rule.hpp>
#include <core/score.hpp>
#include <include/logger.hpp>
#include <include/map.hpp>
#include <include/random.hpp>
#include <include/type.hpp>
#include <include/util.hpp>
#include <io/io.hpp>
#include <io/protocol.hpp>
#include <ui/ui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace gen::core {

namespace { // namespace

namespace fs = std::filesystem;

using gen::io::ActionResult;
using gen::io::ActionStatus;
using gen::io::AIChannel;
using std::format;
using std::size_t;
using std::string;
using std::vector;

// 一个可用玩家的信息
struct PlayerSlot {
    string id; // 玩家 id 即文件夹名
    fs::path exePath; // 可执行文件路径
    fs::path workDir; // 工作目录
};

// 一场比赛的玩家安排
struct MatchPlan {
    vector<PlayerSlot> players; // 按玩家编号排列
    vector<int> teamIDs; // 按玩家编号排列的队伍编号 为空表示每局随机分配
};

// 运行上下文
struct RunContext {
    const Config &config; // 配置
    const RunOptions &options; // 运行选项
    std::uint32_t baseSeed = 0; // 基础随机种子
    int matchCount = 0; // 已运行比赛数
    int gameCount = 0; // 已运行局数
    bool uiEnabled = false; // 是否显示界面
    bool quit = false; // 用户是否请求退出
    ScoreBoard score; // 计分板
    fs::path logDirectory; // 日志目录

    RunContext(const Config &config, const RunOptions &options): config(config), options(options) {}
};

// 一局中的通道状态
struct ChannelState {
    std::unique_ptr<AIChannel> channel; // 通道
    int violations = 0; // 连续违规次数
};

// 判断玩家 id 是否合法 只允许大小写英文字母 数字 下划线 短横线 长度 1 到 64
bool isValidPlayerId(const string &id) {
    if(id.empty() || id.size() > 64) return false;
    for(char c : id) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if(!ok) return false;
    }
    return true;
}

// 返回已用毫秒数 参数 start 为起始时间点
long long elapsedMs(std::chrono::steady_clock::time_point start) {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

// 睡眠指定毫秒数
void sleepMs(int milliseconds) {
    if(milliseconds <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// 界面模式下等待 同时处理按键 用户请求退出时返回 false
bool waitWithUi(int milliseconds) {
    int waited = 0;
    while(waited < milliseconds) {
        ui::Renderer::poll();
        if(ui::Renderer::quitRequested()) return false;
        sleepMs(10);
        waited += 10;
    }
    ui::Renderer::poll();
    return ui::Renderer::quitRequested() == false;
}

// 处理暂停 用户请求退出时返回 false
bool waitForResume() {
    while(ui::Renderer::paused()) {
        ui::Renderer::poll();
        if(ui::Renderer::quitRequested()) return false;
        sleepMs(20);
    }
    return true;
}

// 扫描 battle 目录 返回全部可用玩家 按 id 升序
vector<PlayerSlot> discoverPlayers(const fs::path &battleDirectory) {
    vector<PlayerSlot> result;
    std::error_code error;
    if(fs::exists(battleDirectory, error) == false) {
        logger.error("找不到 battle 目录 路径 " + fs::absolute(battleDirectory).string());
        return result;
    }
    for(const auto &entry : fs::directory_iterator(battleDirectory, error)) {
        if(entry.is_directory() == false) continue;
        string id = entry.path().filename().string();
        if(isValidPlayerId(id) == false) {
            logger.warning("忽略非法玩家目录名 " + id + " 只允许大小写英文字母 数字 下划线 短横线");
            continue;
        }
        fs::path exePath = entry.path() / (id + ".exe");
        if(fs::exists(exePath) == false || fs::is_regular_file(exePath) == false) {
            logger.warning("忽略玩家 " + id + " 缺少可执行文件 " + exePath.string());
            continue;
        }
        result.push_back(PlayerSlot{id, exePath, entry.path()});
    }
    std::sort(result.begin(), result.end(), [](const PlayerSlot &a, const PlayerSlot &b) {
        return a.id < b.id;
    });
    return result;
}

// 在已发现玩家中按 id 查找 找不到返回 false
bool findPlayer(const vector<PlayerSlot> &roster, const string &id, PlayerSlot &slot) {
    for(const auto &player : roster) {
        if(player.id == id) {
            slot = player;
            return true;
        }
    }
    return false;
}

// 组合枚举 从 n 个数中选 k 个 结果按字典序 参数 n 为总数 参数 k 为选择数
vector<vector<int>> combinations(int n, int k) {
    vector<vector<int>> result;
    if(k <= 0 || k > n) return result;
    vector<int> index(k);
    std::iota(index.begin(), index.end(), 0);
    while(true) {
        result.push_back(index);
        int position = k - 1;
        while(position >= 0 && index[position] == n - k + position) position --;
        if(position < 0) break;
        index[position] ++;
        for(int i = position + 1; i < k; i ++) index[i] = index[i - 1] + 1;
    }
    return result;
}

// 按模式随机分配队伍 返回按玩家编号排列的队伍编号
vector<int> assignTeams(GameMode mode, int playerCount) {
    vector<int> order(playerCount);
    std::iota(order.begin(), order.end(), 0);
    randomShuffle(order.begin(), order.end());
    vector<int> teams(playerCount, 0);
    switch(mode) {
        case GameMode::Duel:
            for(int i = 0; i < playerCount; i ++) teams[order[i]] = i;
            break;
        case GameMode::Team:
            for(int i = 0; i < playerCount; i ++) teams[order[i]] = i / 2;
            break;
        case GameMode::FFA:
            for(int i = 0; i < playerCount; i ++) teams[order[i]] = i;
            break;
        default:
            for(int i = 0; i < playerCount; i ++) teams[order[i]] = i;
            break;
    }
    return teams;
}

// 由配置构建地图生成参数
MapGenParams makeGenParams(const Config &config) {
    MapGenParams params;
    for(int i = 0; i < TERRAIN_COUNT; i ++) {
        params.enabled[i] = config.map.banned[i] == false;
    }
    params.cityProbability = config.map.cityProbability;
    params.mountainProbability = config.map.mountainProbability;
    params.swampProbability = config.map.swampProbability;
    params.cityUnitsMin = config.map.cityUnitsMin;
    params.cityUnitsMax = config.map.cityUnitsMax;
    return params;
}

// 生成全局视野 所有格子都是真实信息
GameMap<VisibleTerrain> globalVision(const Game &game) {
    const GameMap<Terrain> &map = game.map();
    GameMap<VisibleTerrain> view(map.R, map.C);
    for(int x = 0; x < map.R; x ++) {
        for(int y = 0; y < map.C; y ++) {
            const Tile<Terrain> &tile = map.at(x, y);
            Tile<VisibleTerrain> visible;
            visible.terrain = toVisibleTerrain(tile.terrain);
            visible.owner = tile.owner;
            visible.unit = tile.unit;
            view.set(x, y, visible);
        }
    }
    return view;
}

// 构造界面一帧的数据
ui::FrameInfo makeFrame(const RunContext &context, const Game &game, int turn,
                        int matchIndex, int matchCount, int gameIndex, int gameCount,
                        bool gameOver, bool victory) {
    ui::FrameInfo frame;
    frame.rows = game.rows();
    frame.cols = game.cols();
    frame.turn = turn;
    frame.maxTurns = game.maxTurns();
    frame.modeName = modeName(context.config.game.mode);
    frame.matchIndex = matchIndex;
    frame.matchCount = matchCount;
    frame.gameIndex = gameIndex;
    frame.gameCount = gameCount;
    frame.gameOver = gameOver;
    frame.victory = victory;

    const PlayerRegistry &registry = game.registry();
    for(const Player &player : registry.players()) {
        ui::PlayerInfo info;
        info.playerID = player.playerID;
        info.teamID = player.teamID;
        info.name = player.name;
        info.alive = player.alive;
        info.units = player.units;
        info.tiles = player.tiles;
        frame.players.push_back(info);
    }

    vector<Rank> teamRanks = gameOver ? game.finalTeamRank() : game.teamLeaderboard();
    for(const Team &team : registry.teams()) {
        ui::TeamInfo info;
        info.teamID = team.teamID;
        info.units = team.units;
        info.tiles = team.tiles;
        info.rank = 0;
        for(const Rank &rank : teamRanks) {
            if(rank.id == team.teamID) info.rank = rank.rank;
        }
        frame.teams.push_back(info);
    }
    frame.teamRanks = teamRanks;
    frame.playerRanks = gameOver ? game.finalPlayerRank() : game.playerLeaderboard();

    int visionMode = ui::Renderer::visionMode();
    if(visionMode >= 0 && visionMode < registry.teamCount()) {
        frame.view = game.teamVision(visionMode);
    }
    else {
        frame.view = globalVision(game);
    }
    return frame;
}

// 把玩家排名表转成可读文本 参数 names 为玩家 id 列表
string formatPlayerRanking(const vector<Rank> &ranks, const vector<string> &names) {
    string text;
    for(size_t i = 0; i < ranks.size(); i ++) {
        if(i > 0) text += "  ";
        int id = ranks[i].id;
        string name = (id >= 0 && id < static_cast<int>(names.size())) ? names[id] : ("玩家" + std::to_string(id));
        text += std::to_string(ranks[i].rank) + " " + name;
    }
    return text;
}

// 把队伍排名表转成可读文本
string formatTeamRanking(const vector<Rank> &ranks) {
    string text;
    for(size_t i = 0; i < ranks.size(); i ++) {
        if(i > 0) text += "  ";
        text += std::to_string(ranks[i].rank) + " 队伍" + std::to_string(ranks[i].id);
    }
    return text;
}

// 打印并记录累计排名
void printScoreTable(const RunContext &context, const string &title) {
    vector<ScoreEntry> entries = context.score.ranking();
    std::string text = "\n== " + title + " ==\n";
    text += "名次  玩家  局数  总分  第一名\n";
    for(const ScoreEntry &entry : entries) {
        text += format("{:>4}  {:<16}  {:>2}  {:>8.2f}  {:>2}\n", entry.rank, entry.name, entry.games, entry.total, entry.firstPlaces);
    }
    std::cout << text << std::flush;
    logger.info(text);
    std::ofstream out(context.logDirectory / "score.log", std::ios::out | std::ios::app);
    if(out.is_open()) {
        out << text;
        out.flush();
    }
}

// 记录地图地形统计 并检查每名玩家是否恰有一个首都
void logMapInfo(const Game &game) {
    std::array<int, TERRAIN_COUNT> counts = MapGenerator::countTerrains(game.map());
    string stats = "地形统计";
    for(int i = 0; i < TERRAIN_COUNT; i ++) {
        stats += " " + string(terrainToChinese(static_cast<Terrain>(i))) + " " + std::to_string(counts[i]);
    }
    logger.info(stats);

    vector<int> capitals(game.registry().playerCount(), 0);
    for(const Tile<Terrain> &tile : game.map().tiles()) {
        if(tile.terrain == Terrain::Capital && tile.owner >= 0 && tile.owner < static_cast<int>(capitals.size())) {
            capitals[tile.owner] ++;
        }
    }
    for(int i = 0; i < static_cast<int>(capitals.size()); i ++) {
        if(capitals[i] != 1) {
            logger.error(format("玩家 {} {} 的首都数量为 {}", i, game.registry().atPlayer(i)->name, capitals[i]));
        }
    }
}

// 生成比赛计划 失败返回 false
bool buildMatchPlans(const Config &config, const vector<PlayerSlot> &roster, vector<MatchPlan> &plans) {
    plans.clear();
    const int required = config.requiredPlayerCount();
    if(required <= 0) {
        logger.error("配置错误的玩家数 " + std::to_string(required));
        return false;
    }
    if(config.match.autoMatch) {
        if(static_cast<int>(roster.size()) < required) {
            logger.error(format("自动匹配需要 {} 名玩家 但 battle 目录只有 {} 名可用玩家", required, roster.size()));
            return false;
        }
        if(roster.size() > 300) {
            logger.error("自动匹配玩家数量超过 300 无法保证可运行");
            return false;
        }
        vector<vector<int>> indexGroups = combinations(static_cast<int>(roster.size()), required);
        if(indexGroups.size() > 2000) {
            logger.warning(format("自动匹配共 {} 种玩家组合 运行时间可能较长", indexGroups.size()));
        }
        for(const auto &group : indexGroups) {
            MatchPlan plan;
            for(int index : group) plan.players.push_back(roster[index]);
            plans.push_back(plan);
        }
        return true;
    }

    if(config.match.teams.empty()) {
        logger.error("未开启自动匹配时必须配置 match.teams");
        return false;
    }
    MatchPlan plan;
    int teamID = 0;
    for(const auto &teamPlayers : config.match.teams) {
        for(const string &id : teamPlayers) {
            PlayerSlot slot;
            if(findPlayer(roster, id, slot) == false) {
                logger.error("队伍配置引用的玩家不可用 " + id + " 请检查 battle/" + id + "/" + id + ".exe 是否存在");
                return false;
            }
            plan.players.push_back(slot);
            plan.teamIDs.push_back(teamID);
        }
        teamID ++;
    }
    plans.push_back(plan);
    return true;
}

// 运行一局 返回 0 继续 1 中止全部比赛
int runGame(RunContext &context, const MatchPlan &plan, int matchIndex, int matchCount,
            int gameIndex, int gameCount) {
    const Config &config = context.config;
    const int playerCount = static_cast<int>(plan.players.size());

    vector<string> names;
    for(const auto &slot : plan.players) names.push_back(slot.id);
    vector<int> teamIDs = plan.teamIDs;
    if(teamIDs.empty()) teamIDs = assignTeams(config.game.mode, playerCount);

    // 按队伍编号重新排列玩家 保证注册时队伍编号从 0 开始连续出现
    vector<int> sortOrder(playerCount);
    std::iota(sortOrder.begin(), sortOrder.end(), 0);
    std::stable_sort(sortOrder.begin(), sortOrder.end(), [&](int left, int right) {
        if(teamIDs[left] != teamIDs[right]) return teamIDs[left] < teamIDs[right];
        return left < right;
    });
    vector<PlayerSlot> slots;
    vector<string> sortedNames;
    vector<int> sortedTeams;
    for(int index : sortOrder) {
        slots.push_back(plan.players[index]);
        sortedNames.push_back(names[index]);
        sortedTeams.push_back(teamIDs[index]);
    }
    names = std::move(sortedNames);
    teamIDs = std::move(sortedTeams);

    std::uint32_t gameSeed = context.baseSeed
        ^ static_cast<std::uint32_t>(matchIndex * 1000003u)
        ^ static_cast<std::uint32_t>(gameIndex * 9176u);
    setSeed(gameSeed);

    Game game(config.map.rows, config.map.cols, config.game.maxTurns);
    for(int i = 0; i < playerCount; i ++) {
        game.registerPlayer(i, teamIDs[i], names[i]);
    }
    vector<int> playerIDs;
    vector<int> playerTeams;
    for(int i = 0; i < playerCount; i ++) {
        playerIDs.push_back(i);
        playerTeams.push_back(teamIDs[i]);
    }
    MapGenParams params = makeGenParams(config);
    game.initialize([&](GameMap<Terrain> &map) {
        MapGenerator::generate(map, config.game.mode, playerIDs, playerTeams, params);
    });

    string head = format("比赛 {}/{} 第 {}/{} 局 开始 模式 {} 地图 {}x{} 种子 {}",
                         matchIndex, matchCount, gameIndex, gameCount, modeName(config.game.mode),
                         game.rows(), game.cols(), gameSeed);
    logger.info(head);
    std::cout << head << std::endl;
    logMapInfo(game);

    MoveRecorder recorder;
    if(config.log.recordMoves) {
        fs::path moveDirectory = context.logDirectory / "moves";
        std::error_code error;
        fs::create_directories(moveDirectory, error);
        fs::path path = moveDirectory / format("match_{:02}_game_{:02}.log", matchIndex, gameIndex);
        recorder.open(path, true);
    }
    recorder.beginGame(matchIndex, gameIndex, modeName(config.game.mode), game.rows(), game.cols(), gameSeed, game.registry());

    // 启动全部 AI 程序
    vector<ChannelState> states(playerCount);
    for(int i = 0; i < playerCount; i ++) {
        auto channel = std::make_unique<AIChannel>(names[i], i, teamIDs[i]);
        fs::path stderrDirectory = context.logDirectory / "ai";
        std::error_code error;
        fs::create_directories(stderrDirectory, error);
        fs::path stderrPath = stderrDirectory / format("match_{:02}_game_{:02}_{}.stderr.txt", matchIndex, gameIndex, names[i]);
        if(channel->start(slots[i].exePath, slots[i].workDir, stderrPath) == false) {
            logger.error(format("玩家 {} 启动失败 路径 {}", names[i], slots[i].exePath.string()));
            recorder.recordEvent(format("玩家 {} {} 启动失败", i, names[i]));
            game.forfeitPlayer(i);
        }
        else {
            states[i].channel = std::move(channel);
        }
    }

    // 判负指定玩家 并关闭其通道
    auto forfeitPlayer = [&](int playerID, const string &reason) {
        if(playerID < 0 || playerID >= playerCount) return;
        if(game.registry().atPlayer(playerID)->alive == false) return;
        logger.warning(format("玩家 {} {} 判负 原因 {}", playerID, names[playerID], reason));
        recorder.recordEvent(format("玩家 {} {} 判负 原因 {}", playerID, names[playerID], reason));
        game.forfeitPlayer(playerID);
        if(states[playerID].channel) {
            states[playerID].channel->closeInput();
            states[playerID].channel->waitExit(200);
            states[playerID].channel->terminate();
            states[playerID].channel.reset();
        }
    };

    // 构造当前移动优先级的玩家指针列表
    auto orderedPlayers = [&]() {
        vector<const Player*> ordered;
        for(int playerID : game.registry().moveOrder()) {
            ordered.push_back(game.registry().atPlayer(playerID));
        }
        return ordered;
    };

    // 初始化流程
    vector<std::chrono::steady_clock::time_point> initTimes(playerCount);
    for(int i = 0; i < playerCount; i ++) {
        if(!states[i].channel) continue;
        bool sent = states[i].channel->sendFrame(io::protocol::PLAYER_SELF,
                                                 io::protocol::playerSelfPayload(*game.registry().atPlayer(i)));
        sent = sent && states[i].channel->sendFrame(io::protocol::PLAYERS, io::protocol::playersPayload(orderedPlayers()));
        sent = sent && states[i].channel->sendFrame(io::protocol::MAP_META, io::protocol::mapMetaPayload(game.rows(), game.cols()));
        sent = sent && states[i].channel->sendFrame(io::protocol::MAP, io::protocol::mapPayload(game.teamVision(teamIDs[i])));
        sent = sent && states[i].channel->sendFrame(io::protocol::INIT, {});
        if(sent == false) {
            forfeitPlayer(i, "初始化消息发送失败");
            continue;
        }
        initTimes[i] = std::chrono::steady_clock::now();
    }
    for(int i = 0; i < playerCount; i ++) {
        if(!states[i].channel) continue;
        long long remaining = config.game.initTimeLimitMs - elapsedMs(initTimes[i]);
        if(remaining < 1) remaining = 1;
        string line;
        bool readOk = states[i].channel->readLine(line, static_cast<int>(remaining));
        if(readOk == false) {
            forfeitPlayer(i, "初始化超时或程序退出");
            continue;
        }
        if(line != "READY") {
            forfeitPlayer(i, "初始化输出不是 READY 实际为 " + line);
            continue;
        }
        logger.info(format("玩家 {} {} 初始化完成", i, names[i]));
    }

    // 结束一局 发送 GAME_OVER 并结算
    auto finishGame = [&](bool userQuit) {
        vector<Rank> playerRank = game.finalPlayerRank();
        vector<Rank> teamRank = game.finalTeamRank();
        if(playerRank.empty()) {
            logger.warning("游戏结束时最终玩家排名为空 使用当前排名替代");
            playerRank = game.playerLeaderboard();
            teamRank = game.teamLeaderboard();
        }
        for(int i = 0; i < playerCount; i ++) {
            if(!states[i].channel) continue;
            states[i].channel->sendFrame(io::protocol::GAME_OVER,
                                         io::protocol::gameOverPayload(game.registry(), playerRank, teamRank));
            states[i].channel->closeInput();
            states[i].channel->waitExit(300);
            states[i].channel->terminate();
            states[i].channel.reset();
        }
        if(userQuit) {
            recorder.recordEvent("用户请求退出 本局不计分");
            recorder.close();
            return;
        }
        recorder.endGame(playerRank, teamRank, game.finishedByVictory());
        recorder.close();

        if(game.isOver()) {
            context.score.addGame(names, teamIDs, playerRank, teamRank);
        }

        string result = format("比赛 {}/{} 第 {}/{} 局 结束 回合 {} 结束原因 {}",
                               matchIndex, matchCount, gameIndex, gameCount, game.currentTurn(),
                               game.finishedByVictory() ? "只剩一支队伍" : "到达最大回合数");
        string playerText = "玩家排名 " + formatPlayerRanking(playerRank, names);
        string teamText = "队伍排名 " + formatTeamRanking(teamRank);
        logger.info(result);
        logger.info(playerText);
        logger.info(teamText);
        std::cout << result << "\n  " << playerText << "\n  " << teamText << std::endl;

        if(context.uiEnabled) {
            ui::FrameInfo frame = makeFrame(context, game, game.currentTurn(), matchIndex, matchCount,
                                            gameIndex, gameCount, true, game.finishedByVictory());
            ui::Renderer::render(frame);
            waitWithUi(config.ui.resultDelayMs);
        }
    };

    if(game.isOver()) {
        finishGame(false);
        return context.quit ? 1 : 0;
    }

    // 回合循环
    while(game.isOver() == false) {
        int displayTurn = game.currentTurn() + 1;

        // 显示当前状态
        if(context.uiEnabled) {
            ui::FrameInfo frame = makeFrame(context, game, displayTurn - 1, matchIndex, matchCount,
                                            gameIndex, gameCount, false, false);
            ui::Renderer::render(frame);
            ui::Renderer::poll();
            if(ui::Renderer::quitRequested() || waitForResume() == false) {
                context.quit = true;
                break;
            }
        }

        // 发送回合消息和开始计时标志
        vector<const Player*> ordered = orderedPlayers();
        vector<Rank> teamRanks = game.teamLeaderboard();
        vector<std::chrono::steady_clock::time_point> goTimes(playerCount);
        for(int i = 0; i < playerCount; i ++) {
            if(!states[i].channel) continue;
            if(game.registry().atPlayer(i)->alive == false) continue;
            bool sent = states[i].channel->sendFrame(io::protocol::TURN,
                          io::protocol::turnPayload(displayTurn, ordered, teamRanks, game.teamVision(teamIDs[i])));
            sent = sent && states[i].channel->sendFrame(io::protocol::GO, {});
            if(sent == false) {
                forfeitPlayer(i, "回合消息发送失败");
                continue;
            }
            goTimes[i] = std::chrono::steady_clock::now();
        }

        GameMap<Terrain> before = game.map();
        vector<MoveOperation> moves;
        vector<ActionRecord> actions(playerCount);
        for(int i = 0; i < playerCount; i ++) actions[i].playerID = i;

        for(int i = 0; i < playerCount; i ++) {
            if(game.isOver()) break;
            if(!states[i].channel) continue;
            if(game.registry().atPlayer(i)->alive == false) continue;
            long long remaining = config.game.turnTimeLimitMs - elapsedMs(goTimes[i]);
            if(remaining < 1) remaining = 1;
            ActionResult result = states[i].channel->readAction(static_cast<int>(remaining));
            switch(result.status) {
                case ActionStatus::Pass:
                    actions[i].pass = true;
                    states[i].violations = 0;
                    break;
                case ActionStatus::Move:
                    // 用本回合开始前的地图预校验 棋盘上的非法移动按 PASS 计一次违规
                    if(Rule::isValidMove(before, i, result.move) == false) {
                        actions[i].pass = true;
                        actions[i].note = "非法移动 " + result.move.source.toString() + " 到 " + result.move.dest.toString();
                        states[i].violations ++;
                        logger.warning(format("玩家 {} {} 第 {} 回合非法移动 源格 {} 目标格 {}",
                                              i, names[i], displayTurn,
                                              result.move.source.toString(), result.move.dest.toString()));
                        if(config.game.maxViolations > 0 && states[i].violations >= config.game.maxViolations) {
                            forfeitPlayer(i, "连续违规次数达到上限");
                        }
                    }
                    else {
                        actions[i].hasMove = true;
                        actions[i].source = result.move.source;
                        actions[i].dest = result.move.dest;
                        actions[i].half = result.move.half;
                        moves.push_back(result.move);
                        states[i].violations = 0;
                    }
                    break;
                case ActionStatus::Invalid:
                    actions[i].pass = true;
                    actions[i].note = "非法输出 " + result.detail;
                    states[i].violations ++;
                    logger.warning(format("玩家 {} {} 第 {} 回合非法输出 {}", i, names[i], displayTurn, result.detail));
                    if(config.game.maxViolations > 0 && states[i].violations >= config.game.maxViolations) {
                        forfeitPlayer(i, "连续违规次数达到上限");
                    }
                    break;
                case ActionStatus::Timeout:
                    actions[i].pass = true;
                    actions[i].note = "超时未输出";
                    states[i].violations ++;
                    logger.warning(format("玩家 {} {} 第 {} 回合超时", i, names[i], displayTurn));
                    if(config.game.maxViolations > 0 && states[i].violations >= config.game.maxViolations) {
                        forfeitPlayer(i, "连续违规次数达到上限");
                    }
                    break;
                case ActionStatus::Eof:
                    actions[i].pass = true;
                    actions[i].note = "程序输出结束";
                    forfeitPlayer(i, "程序输出结束");
                    break;
                case ActionStatus::Crashed:
                    actions[i].pass = true;
                    actions[i].note = "程序崩溃或异常退出";
                    forfeitPlayer(i, "程序崩溃或异常退出");
                    break;
                default:
                    actions[i].pass = true;
                    break;
            }
        }

        bool over = false;
        if(game.isOver() == false) {
            over = game.nextTurn(moves);
        }
        else {
            over = true;
        }
        recorder.recordTurn(displayTurn, actions, before, game.map(), game.registry());

        // 显示本回合结果并控制节奏
        if(context.uiEnabled) {
            ui::FrameInfo frame = makeFrame(context, game, game.currentTurn(), matchIndex, matchCount,
                                            gameIndex, gameCount, over, game.finishedByVictory());
            ui::Renderer::render(frame);
            ui::Renderer::poll();
            if(ui::Renderer::quitRequested() || waitForResume() == false) {
                context.quit = true;
                break;
            }
            if(waitWithUi(ui::Renderer::delayMs()) == false) {
                context.quit = true;
                break;
            }
        }
        if(over) break;
    }

    if(context.quit) {
        finishGame(true);
        return 1;
    }
    finishGame(false);
    return 0;
}

// 运行全部比赛 返回进程退出码
int runAll(RunContext &context) {
    const Config &config = context.config;

    context.uiEnabled = context.options.enableUi && config.ui.enabled;
    std::error_code error;
    context.logDirectory = fs::path(config.log.directory);
    fs::create_directories(context.logDirectory, error);
    if(error) {
        logger.error("无法创建日志目录 " + context.logDirectory.string() + " 原因 " + error.message());
        return 1;
    }

    logger.info("开始加载配置\n" + describeConfig(config));
    setSeed(context.baseSeed);
    logger.info(format("本次运行基础随机种子 {}", context.baseSeed));

    vector<PlayerSlot> roster = discoverPlayers("battle");
    if(roster.empty()) {
        logger.error("battle 目录下没有可用玩家 请先构建示例 AI");
        std::cout << "错误 battle 目录下没有可用玩家 请先运行 build.bat 构建示例 AI" << std::endl;
        return 1;
    }
    logger.info(format("发现 {} 名玩家", roster.size()));
    if(roster.size() > 32767) {
        logger.error("玩家数量超过协议上限 32767");
        return 1;
    }

    vector<MatchPlan> plans;
    if(buildMatchPlans(config, roster, plans) == false) {
        return 1;
    }
    if(plans.empty()) {
        logger.error("没有可运行的比赛");
        return 1;
    }
    int matchCount = static_cast<int>(plans.size());
    logger.info(format("共 {} 场比赛 每场 {} 局", matchCount, config.game.gamesPerMatch));

    context.score.configure(config.match.playerWeight, config.match.teamWeight);
    std::ofstream(context.logDirectory / "score.log", std::ios::out | std::ios::trunc).close();

    if(context.uiEnabled) {
        ui::Renderer::open(config.map.rows, config.map.cols, config.ui.cellSize);
        // 把配置里的画面速度应用到界面
        ui::Renderer::adjustDelay(config.ui.turnDelayMs - ui::Renderer::delayMs());
    }

    int result = 0;
    bool aborted = false;
    for(int matchIndex = 1; matchIndex <= matchCount && aborted == false; matchIndex ++) {
        const MatchPlan &plan = plans[matchIndex - 1];
        string playerNames;
        for(size_t i = 0; i < plan.players.size(); i ++) {
            if(i > 0) playerNames += ", ";
            playerNames += plan.players[i].id;
        }
        logger.info(format("比赛 {}/{} 参赛玩家 {}", matchIndex, matchCount, playerNames));
        for(int gameIndex = 1; gameIndex <= config.game.gamesPerMatch; gameIndex ++) {
            context.matchCount = matchIndex;
            context.gameCount ++;
            int code = runGame(context, plan, matchIndex, matchCount, gameIndex, config.game.gamesPerMatch);
            if(code != 0) {
                result = code;
                aborted = true;
                break;
            }
        }
        if(aborted == false) {
            printScoreTable(context, format("比赛 {}/{} 结束后的累计排名", matchIndex, matchCount));
        }
    }
    if(aborted == false) {
        printScoreTable(context, "全部比赛最终排名");
        logger.info(format("全部比赛结束 比赛 {} 局 {}", matchCount, context.gameCount));
    }
    else {
        logger.warning("比赛被用户中止");
    }
    if(context.uiEnabled) {
        ui::Renderer::close();
    }
    logger.info(format("运行结束 比赛 {} 局 {} 退出码 {}", context.matchCount, context.gameCount, result));
    return result;
}

} // namespace

Runner::Runner(Config config, RunOptions options)
    : config_(std::move(config)), options_(std::move(options)) {}

int Runner::run() {
    RunContext context(config_, options_);
    if(options_.seed != 0) {
        context.baseSeed = options_.seed;
    }
    else if(config_.map.seed != 0) {
        context.baseSeed = config_.map.seed;
    }
    else {
        context.baseSeed = makeTimeSeed();
    }
    int result = runAll(context);
    runMatchCount_ = context.matchCount;
    runGameCount_ = context.gameCount;
    return result;
}

} // namespace gen::core
