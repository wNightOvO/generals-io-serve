/*
    游戏配置实现
    使用 JSON 解析器读取配置文件 校验后供整个程序使用
    读取失败 类型错误 取值越界都会抛出 ConfigError 消息为中文并带配置键路径
*/

#include <core/config.hpp>
#include <include/json.hpp>
#include <include/logger.hpp>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gen::core {

namespace { // namespace

using gen::json::Value;

// 根节点允许出现的配置段
const std::set<std::string> ROOT_SECTIONS = {"map", "game", "match", "ui", "log"};

// map 段允许出现的键
const std::set<std::string> MAP_KEYS = {
    "rows", "cols", "bannedTerrains", "seed", "cityProbability", "mountainProbability",
    "swampProbability", "cityUnitsMin", "cityUnitsMax"
};

// game 段允许出现的键
const std::set<std::string> GAME_KEYS = {
    "mode", "maxTurns", "gamesPerMatch", "ffaPlayerCount", "initTimeLimitMs",
    "turnTimeLimitMs", "maxViolations"
};

// match 段允许出现的键
const std::set<std::string> MATCH_KEYS = {"autoMatch", "teams", "playerWeight", "teamWeight"};

// ui 段允许出现的键
const std::set<std::string> UI_KEYS = {"enabled", "cellSize", "turnDelayMs", "resultDelayMs"};

// log 段允许出现的键
const std::set<std::string> LOG_KEYS = {"directory", "recordMoves"};

// 概率之和允许的浮点误差
constexpr double PROBABILITY_EPSILON = 1e-9;

// 玩家 id 最大长度
constexpr std::size_t PLAYER_ID_MAX_LENGTH = 64;

// 把数字格式化为可读文本 保留 6 位有效数字
std::string formatNumber(double value) {
    std::ostringstream stream;
    stream.precision(6);
    stream << value;
    return stream.str();
}

// 把布尔值格式化为配置中的写法
const char* boolText(bool value) {
    return value ? "true" : "false";
}

// 抛出带键路径的类型错误
[[noreturn]] void throwTypeError(const std::string &path, const char *expected, const Value &value) {
    throw ConfigError("配置键 " + path + " 必须是" + expected + " 实际为" + value.typeName());
}

// 取配置段 段不存在时写警告并返回空指针 段存在但不是对象时抛出 ConfigError
const Value* requireSection(const Value &root, const std::string &key) {
    const Value *section = root.find(key);
    if(section == nullptr) {
        logger.warning("配置缺少配置段 " + key + " 该段全部使用默认值");
        return nullptr;
    }
    if(section->isObject() == false) throwTypeError(key, "对象", *section);
    return section;
}

// 对配置段中的未知键写警告
void warnUnknownKeys(const Value &section, const std::string &sectionPath, const std::set<std::string> &knownKeys) {
    for(const auto &entry : section.asObject()) {
        if(knownKeys.find(entry.first) == knownKeys.end()) {
            logger.warning("配置段 " + sectionPath + " 中存在未知键 " + entry.first + " 已忽略");
        }
    }
}

// 读取整数配置 键缺失时写警告并保留默认值
bool readInteger(const Value &section, const std::string &key, const std::string &path, int &value) {
    const Value *member = section.find(key);
    if(member == nullptr) {
        logger.warning("配置键 " + path + " 缺失 使用默认值 " + std::to_string(value));
        return false;
    }
    if(member->isNumber() == false) throwTypeError(path, "整数", *member);
    if(member->isInteger() == false) {
        throw ConfigError("配置键 " + path + " 必须是整数 实际为 " + formatNumber(member->asNumber()));
    }
    std::int64_t number = member->asInteger();
    if(number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        throw ConfigError("配置键 " + path + " 超出 32 位整数范围 当前为 " + std::to_string(number));
    }
    value = static_cast<int>(number);
    return true;
}

// 读取 32 位无符号整数配置 用于随机种子
bool readUint32(const Value &section, const std::string &key, const std::string &path, std::uint32_t &value) {
    const Value *member = section.find(key);
    if(member == nullptr) {
        logger.warning("配置键 " + path + " 缺失 使用默认值 " + std::to_string(value));
        return false;
    }
    if(member->isNumber() == false) throwTypeError(path, "整数", *member);
    if(member->isInteger() == false) {
        throw ConfigError("配置键 " + path + " 必须是整数 实际为 " + formatNumber(member->asNumber()));
    }
    std::int64_t number = member->asInteger();
    if(number < 0 || number > 4294967295LL) {
        throw ConfigError("配置键 " + path + " 必须是 0 到 4294967295 的整数 当前为 " + std::to_string(number));
    }
    value = static_cast<std::uint32_t>(number);
    return true;
}

// 读取数字配置 键缺失时写警告并保留默认值
bool readNumber(const Value &section, const std::string &key, const std::string &path, double &value) {
    const Value *member = section.find(key);
    if(member == nullptr) {
        logger.warning("配置键 " + path + " 缺失 使用默认值 " + formatNumber(value));
        return false;
    }
    if(member->isNumber() == false) throwTypeError(path, "数字", *member);
    value = member->asNumber();
    return true;
}

// 读取布尔配置 键缺失时写警告并保留默认值
bool readBool(const Value &section, const std::string &key, const std::string &path, bool &value) {
    const Value *member = section.find(key);
    if(member == nullptr) {
        logger.warning("配置键 " + path + " 缺失 使用默认值 " + boolText(value));
        return false;
    }
    if(member->isBool() == false) throwTypeError(path, "布尔值", *member);
    value = member->asBool();
    return true;
}

// 读取字符串配置 键缺失时写警告并保留默认值
bool readString(const Value &section, const std::string &key, const std::string &path, std::string &value) {
    const Value *member = section.find(key);
    if(member == nullptr) {
        std::string shown = value.empty() ? "空字符串" : value;
        logger.warning("配置键 " + path + " 缺失 使用默认值 " + shown);
        return false;
    }
    if(member->isString() == false) throwTypeError(path, "字符串", *member);
    value = member->asString();
    return true;
}

// 读取禁用地形列表 元素为地形英文名 大小写不敏感
void readBannedTerrains(const Value &section, MapConfig &config) {
    const Value *member = section.find("bannedTerrains");
    if(member == nullptr) {
        logger.warning("配置键 map.bannedTerrains 缺失 使用默认值 不禁用任何地形");
        return;
    }
    if(member->isArray() == false) throwTypeError("map.bannedTerrains", "数组", *member);
    const Value::Array &items = member->asArray();
    for(std::size_t index = 0; index < items.size(); ++index) {
        std::string path = "map.bannedTerrains[" + std::to_string(index) + "]";
        if(items[index].isString() == false) throwTypeError(path, "字符串", items[index]);
        const std::string &name = items[index].asString();
        Terrain terrain = Terrain::Blank;
        if(terrainFromName(name, terrain) == false) {
            throw ConfigError("配置键 " + path + " 的地形名 " + name + " 非法 合法值为 blank mountain swamp city capital 大小写不敏感");
        }
        if(terrain == Terrain::Blank) {
            throw ConfigError("配置键 " + path + " 不允许禁用空地 blank 空地是必需地形");
        }
        if(terrain == Terrain::Capital) {
            throw ConfigError("配置键 " + path + " 不允许禁用首都 capital 首都是必需地形");
        }
        config.banned[static_cast<int>(terrain)] = true;
    }
}

// 读取显式队伍配置 外层数组为队伍 内层数组为玩家 id
void readTeams(const Value &section, MatchConfig &config) {
    const Value *member = section.find("teams");
    if(member == nullptr) {
        logger.warning("配置键 match.teams 缺失 使用默认值 空数组");
        return;
    }
    if(member->isArray() == false) throwTypeError("match.teams", "数组", *member);
    const Value::Array &teamValues = member->asArray();
    std::vector<std::vector<std::string>> teams;
    teams.reserve(teamValues.size());
    for(std::size_t teamIndex = 0; teamIndex < teamValues.size(); ++teamIndex) {
        std::string teamPath = "match.teams[" + std::to_string(teamIndex) + "]";
        if(teamValues[teamIndex].isArray() == false) throwTypeError(teamPath, "数组", teamValues[teamIndex]);
        const Value::Array &playerValues = teamValues[teamIndex].asArray();
        std::vector<std::string> players;
        players.reserve(playerValues.size());
        for(std::size_t playerIndex = 0; playerIndex < playerValues.size(); ++playerIndex) {
            std::string playerPath = teamPath + "[" + std::to_string(playerIndex) + "]";
            if(playerValues[playerIndex].isString() == false) throwTypeError(playerPath, "字符串", playerValues[playerIndex]);
            players.push_back(playerValues[playerIndex].asString());
        }
        teams.push_back(std::move(players));
    }
    config.teams = std::move(teams);
}

// 读取 map 段
void loadMap(const Value &root, MapConfig &config) {
    const Value *section = requireSection(root, "map");
    if(section == nullptr) return;
    readInteger(*section, "rows", "map.rows", config.rows);
    readInteger(*section, "cols", "map.cols", config.cols);
    readBannedTerrains(*section, config);
    readUint32(*section, "seed", "map.seed", config.seed);
    readNumber(*section, "cityProbability", "map.cityProbability", config.cityProbability);
    readNumber(*section, "mountainProbability", "map.mountainProbability", config.mountainProbability);
    readNumber(*section, "swampProbability", "map.swampProbability", config.swampProbability);
    readInteger(*section, "cityUnitsMin", "map.cityUnitsMin", config.cityUnitsMin);
    readInteger(*section, "cityUnitsMax", "map.cityUnitsMax", config.cityUnitsMax);
    warnUnknownKeys(*section, "map", MAP_KEYS);
}

// 读取 game 段
void loadGame(const Value &root, GameConfig &config) {
    const Value *section = requireSection(root, "game");
    if(section == nullptr) return;
    const Value *modeMember = section->find("mode");
    if(modeMember == nullptr) {
        logger.warning("配置键 game.mode 缺失 使用默认值 " + std::string(modeName(config.mode)));
    } else if(modeMember->isString() == false) {
        throwTypeError("game.mode", "字符串", *modeMember);
    } else {
        GameMode mode = config.mode;
        const std::string &name = modeMember->asString();
        if(modeFromName(name, mode) == false) {
            throw ConfigError("配置键 game.mode 的取值 " + name + " 非法 合法值为 1v1 2v2 ffa 大小写不敏感");
        }
        config.mode = mode;
    }
    readInteger(*section, "maxTurns", "game.maxTurns", config.maxTurns);
    readInteger(*section, "gamesPerMatch", "game.gamesPerMatch", config.gamesPerMatch);
    readInteger(*section, "ffaPlayerCount", "game.ffaPlayerCount", config.ffaPlayerCount);
    readInteger(*section, "initTimeLimitMs", "game.initTimeLimitMs", config.initTimeLimitMs);
    readInteger(*section, "turnTimeLimitMs", "game.turnTimeLimitMs", config.turnTimeLimitMs);
    readInteger(*section, "maxViolations", "game.maxViolations", config.maxViolations);
    warnUnknownKeys(*section, "game", GAME_KEYS);
}

// 读取 match 段
void loadMatch(const Value &root, MatchConfig &config) {
    const Value *section = requireSection(root, "match");
    if(section == nullptr) return;
    readBool(*section, "autoMatch", "match.autoMatch", config.autoMatch);
    readTeams(*section, config);
    readNumber(*section, "playerWeight", "match.playerWeight", config.playerWeight);
    readNumber(*section, "teamWeight", "match.teamWeight", config.teamWeight);
    warnUnknownKeys(*section, "match", MATCH_KEYS);
}

// 读取 ui 段
void loadUi(const Value &root, UiConfig &config) {
    const Value *section = requireSection(root, "ui");
    if(section == nullptr) return;
    readBool(*section, "enabled", "ui.enabled", config.enabled);
    readInteger(*section, "cellSize", "ui.cellSize", config.cellSize);
    readInteger(*section, "turnDelayMs", "ui.turnDelayMs", config.turnDelayMs);
    readInteger(*section, "resultDelayMs", "ui.resultDelayMs", config.resultDelayMs);
    warnUnknownKeys(*section, "ui", UI_KEYS);
}

// 读取 log 段
void loadLog(const Value &root, LogConfig &config) {
    const Value *section = requireSection(root, "log");
    if(section == nullptr) return;
    readString(*section, "directory", "log.directory", config.directory);
    readBool(*section, "recordMoves", "log.recordMoves", config.recordMoves);
    warnUnknownKeys(*section, "log", LOG_KEYS);
}

// 校验整数范围 越界抛出 ConfigError
void requireRange(const char *path, int value, int minValue, int maxValue) {
    if(value < minValue || value > maxValue) {
        throw ConfigError("配置键 " + std::string(path) + " 必须是 " + std::to_string(minValue) + " 到 " +
                          std::to_string(maxValue) + " 的整数 当前为 " + std::to_string(value));
    }
}

// 校验概率范围 越界抛出 ConfigError
void requireProbability(const char *path, double value) {
    if(value < 0.0 || value > 1.0) {
        throw ConfigError("配置键 " + std::string(path) + " 必须是 0 到 1 之间的数字 当前为 " + formatNumber(value));
    }
}

// 校验权重不小于 0
void requireWeight(const char *path, double value) {
    if(value < 0.0) {
        throw ConfigError("配置键 " + std::string(path) + " 不能小于 0 当前为 " + formatNumber(value));
    }
}

// 玩家 id 是否合法 只含大小写英文字母 数字 下划线 短横线 长度 1 到 64
bool isValidPlayerId(const std::string &id) {
    if(id.empty() || id.size() > PLAYER_ID_MAX_LENGTH) return false;
    for(char ch : id) {
        bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        bool digit = (ch >= '0' && ch <= '9');
        bool other = (ch == '_' || ch == '-');
        if(letter == false && digit == false && other == false) return false;
    }
    return true;
}

// 校验队伍数量与每队人数
void requireTeamShape(const std::vector<std::vector<std::string>> &teams, int expectedTeams,
                      int expectedPlayersPerTeam, const char *modeText) {
    if(static_cast<int>(teams.size()) != expectedTeams) {
        throw ConfigError("配置键 match.teams 在 " + std::string(modeText) + " 模式下必须是 " +
                          std::to_string(expectedTeams) + " 支队伍 当前为 " + std::to_string(teams.size()) + " 支");
    }
    for(std::size_t teamIndex = 0; teamIndex < teams.size(); ++teamIndex) {
        std::string path = "match.teams[" + std::to_string(teamIndex) + "]";
        if(static_cast<int>(teams[teamIndex].size()) != expectedPlayersPerTeam) {
            throw ConfigError("配置键 " + path + " 在 " + std::string(modeText) + " 模式下必须包含 " +
                              std::to_string(expectedPlayersPerTeam) + " 名玩家 当前为 " +
                              std::to_string(teams[teamIndex].size()) + " 名");
        }
    }
}

// 校验显式队伍配置 玩家 id 合法且不重复 队伍数与人数符合模式
void validateTeams(const Config &config) {
    const std::vector<std::vector<std::string>> &teams = config.match.teams;
    if(teams.empty()) {
        throw ConfigError("配置键 match.teams 不能为空 未开启自动匹配时必须显式配置队伍");
    }
    std::map<std::string, std::string> seen; // 记录玩家 id 首次出现的位置
    for(std::size_t teamIndex = 0; teamIndex < teams.size(); ++teamIndex) {
        for(std::size_t playerIndex = 0; playerIndex < teams[teamIndex].size(); ++playerIndex) {
            const std::string &id = teams[teamIndex][playerIndex];
            std::string path = "match.teams[" + std::to_string(teamIndex) + "][" + std::to_string(playerIndex) + "]";
            if(isValidPlayerId(id) == false) {
                throw ConfigError("配置键 " + path + " 的玩家 id " + id +
                                  " 非法 只允许大小写英文字母 数字 下划线 短横线 长度 1 到 64");
            }
            auto hit = seen.find(id);
            if(hit != seen.end()) {
                throw ConfigError("配置键 " + path + " 的玩家 id " + id + " 重复 首次出现在 " + hit->second);
            }
            seen[id] = path;
        }
    }
    switch(config.game.mode) {
        case GameMode::Duel:
            requireTeamShape(teams, 2, 1, "1v1");
            break;
        case GameMode::Team:
            requireTeamShape(teams, 2, 2, "2v2");
            break;
        case GameMode::FFA:
            requireTeamShape(teams, config.game.ffaPlayerCount, 1, "ffa");
            break;
        default:
            requireTeamShape(teams, 2, 1, "1v1");
            break;
    }
}

} // namespace

const char* modeName(GameMode mode) {
    switch(mode) {
        case GameMode::Duel: return "1v1";
        case GameMode::Team: return "2v2";
        case GameMode::FFA: return "ffa";
        default: return "1v1";
    }
}

bool modeFromName(const std::string &name, GameMode &mode) {
    std::string lower;
    lower.reserve(name.size());
    for(char ch : name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if(lower == "1v1") {
        mode = GameMode::Duel;
        return true;
    }
    if(lower == "2v2") {
        mode = GameMode::Team;
        return true;
    }
    if(lower == "ffa") {
        mode = GameMode::FFA;
        return true;
    }
    return false;
}

Config Config::load(const std::filesystem::path &filePath) {
    const std::string pathText = filePath.string();
    std::error_code error;
    bool exists = std::filesystem::exists(filePath, error);
    if(exists == false) {
        if(error) throw ConfigError("配置文件 " + pathText + " 检查失败 原因 " + error.message());
        throw ConfigError("配置文件不存在 " + pathText);
    }
    Value root;
    try {
        root = gen::json::parseFile(filePath);
    } catch(const gen::json::ParseError &parseError) {
        if(parseError.line() > 0) {
            throw ConfigError("解析配置文件 " + pathText + " 失败 " + parseError.what());
        }
        throw ConfigError("读取配置文件 " + pathText + " 失败 " + parseError.what());
    }
    if(root.isObject() == false) {
        throw ConfigError("配置文件 " + pathText + " 的根节点必须是对象 实际为" + root.typeName());
    }
    Config config; // 先使用结构体默认值 缺失的键保留默认值
    try {
        loadMap(root, config.map);
        loadGame(root, config.game);
        loadMatch(root, config.match);
        loadUi(root, config.ui);
        loadLog(root, config.log);
        for(const auto &entry : root.asObject()) {
            if(ROOT_SECTIONS.find(entry.first) == ROOT_SECTIONS.end()) {
                logger.warning("配置文件根节点中存在未知段 " + entry.first + " 已忽略");
            }
        }
    } catch(const gen::json::ValueError &valueError) {
        throw ConfigError("配置文件 " + pathText + " 中的值类型错误 " + valueError.what());
    }
    config.validate();
    logger.info("配置文件读取成功 路径 " + pathText);
    return config;
}

void Config::validate() const {
    // 地图配置
    requireRange("map.rows", map.rows, 5, 200);
    requireRange("map.cols", map.cols, 5, 200);
    if(map.banned[static_cast<int>(Terrain::Blank)]) {
        throw ConfigError("配置键 map.bannedTerrains 不允许禁用空地 blank");
    }
    if(map.banned[static_cast<int>(Terrain::Capital)]) {
        throw ConfigError("配置键 map.bannedTerrains 不允许禁用首都 capital");
    }
    requireProbability("map.cityProbability", map.cityProbability);
    requireProbability("map.mountainProbability", map.mountainProbability);
    requireProbability("map.swampProbability", map.swampProbability);
    double probabilitySum = map.cityProbability + map.mountainProbability + map.swampProbability;
    if(probabilitySum > 1.0 + PROBABILITY_EPSILON) {
        throw ConfigError("配置键 map.cityProbability map.mountainProbability map.swampProbability 之和不能超过 1 当前为 " +
                          formatNumber(probabilitySum));
    }
    requireRange("map.cityUnitsMin", map.cityUnitsMin, 1, 100000);
    requireRange("map.cityUnitsMax", map.cityUnitsMax, 1, 100000);
    if(map.cityUnitsMin > map.cityUnitsMax) {
        throw ConfigError("配置键 map.cityUnitsMin 不能大于 map.cityUnitsMax 当前为 " +
                          std::to_string(map.cityUnitsMin) + " 与 " + std::to_string(map.cityUnitsMax));
    }

    // 对局配置
    requireRange("game.maxTurns", game.maxTurns, 1, 1000000);
    requireRange("game.gamesPerMatch", game.gamesPerMatch, 1, 1000);
    requireRange("game.ffaPlayerCount", game.ffaPlayerCount, 2, 16);
    requireRange("game.initTimeLimitMs", game.initTimeLimitMs, 1, 60000);
    requireRange("game.turnTimeLimitMs", game.turnTimeLimitMs, 1, 60000);
    requireRange("game.maxViolations", game.maxViolations, 0, 1000);

    // 比赛配置
    requireWeight("match.playerWeight", match.playerWeight);
    requireWeight("match.teamWeight", match.teamWeight);
    if(match.playerWeight + match.teamWeight <= 0.0) {
        logger.warning("配置键 match.playerWeight 与 match.teamWeight 之和为 0 所有玩家总分将全部为 0");
    }
    if(match.autoMatch) {
        if(match.teams.empty() == false) {
            logger.warning("配置键 match.teams 在开启自动匹配时会被忽略");
        }
    } else {
        validateTeams(*this);
    }

    // 界面配置
    requireRange("ui.cellSize", ui.cellSize, 8, 128);
    requireRange("ui.turnDelayMs", ui.turnDelayMs, 0, 60000);
    requireRange("ui.resultDelayMs", ui.resultDelayMs, 0, 60000);

    // 日志配置
    if(log.directory.empty()) {
        throw ConfigError("配置键 log.directory 不能为空");
    }
}

std::string describeConfig(const Config &config) {
    std::ostringstream stream;
    stream << "游戏模式 " << modeName(config.game.mode) << " 需要玩家数 " << config.requiredPlayerCount() << '\n';
    stream << "地图大小 " << config.map.rows << " 行 " << config.map.cols << " 列\n";
    if(config.map.seed == 0) {
        stream << "随机种子 0 每次运行随机\n";
    } else {
        stream << "随机种子 " << config.map.seed << '\n';
    }
    stream << "地形概率 城市 " << formatNumber(config.map.cityProbability) << " 山区 " <<
              formatNumber(config.map.mountainProbability) << " 沼泽 " << formatNumber(config.map.swampProbability) << '\n';
    stream << "中立城市兵力 " << config.map.cityUnitsMin << " 到 " << config.map.cityUnitsMax << '\n';
    stream << "禁用地形";
    bool hasBanned = false;
    for(int index = 0; index < TERRAIN_COUNT; ++index) {
        if(config.map.banned[index] == false) continue;
        stream << ' ' << terrainToChinese(static_cast<Terrain>(index));
        hasBanned = true;
    }
    if(hasBanned == false) stream << " 无";
    stream << '\n';
    stream << "最大回合数 " << config.game.maxTurns << " 每场比赛局数 " << config.game.gamesPerMatch << '\n';
    stream << "初始化时限 " << config.game.initTimeLimitMs << " 毫秒 每回合时限 " <<
              config.game.turnTimeLimitMs << " 毫秒\n";
    stream << "违规上限 " << config.game.maxViolations << " 次 0 表示不判负\n";
    if(config.game.mode == GameMode::FFA) {
        stream << "混战玩家数 " << config.game.ffaPlayerCount << '\n';
    }
    stream << "自动匹配 " << (config.match.autoMatch ? "开启" : "关闭") << '\n';
    if(config.match.autoMatch) {
        stream << "显式队伍 自动匹配时忽略\n";
    } else {
        stream << "显式队伍";
        for(std::size_t teamIndex = 0; teamIndex < config.match.teams.size(); ++teamIndex) {
            if(teamIndex == 0) stream << ' ';
            else stream << " | ";
            for(std::size_t playerIndex = 0; playerIndex < config.match.teams[teamIndex].size(); ++playerIndex) {
                if(playerIndex != 0) stream << ' ';
                stream << config.match.teams[teamIndex][playerIndex];
            }
        }
        stream << '\n';
    }
    stream << "玩家权重 " << formatNumber(config.match.playerWeight) << " 队伍权重 " <<
              formatNumber(config.match.teamWeight) << '\n';
    stream << "界面 " << (config.ui.enabled ? "开启" : "关闭") << " 格子边长 " << config.ui.cellSize <<
              " 像素 回合停留 " << config.ui.turnDelayMs << " 毫秒 终局停留 " << config.ui.resultDelayMs << " 毫秒\n";
    stream << "日志目录 " << config.log.directory << " 记录操作 " << (config.log.recordMoves ? "开启" : "关闭");
    return stream.str();
}

} // namespace gen::core
