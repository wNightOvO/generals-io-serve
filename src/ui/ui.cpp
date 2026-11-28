/*
    界面渲染实现
    使用 EasyX 绘制地图 地形 兵力 顶部信息条 与右侧排名信息栏
    源码与执行字符集为 UTF-8 EasyX ANSI 版按系统代码页解释文本
    所以所有中文文本在绘制前统一做 UTF-8 到系统 ANSI 的转换
*/

#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ui/ui.hpp>
#include <include/logger.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <easyx/easyx.h>

namespace gen::ui {

namespace {

// ============================================================
// 布局与行为常量
// ============================================================

constexpr int TOP_BAR_HEIGHT = 56; // 顶部信息条高度 像素
constexpr int PANEL_WIDTH = 300; // 右侧信息栏宽度 像素
constexpr int MIN_CELL_SIZE = 8; // 格子最小边长 像素
constexpr int DEFAULT_CELL_SIZE = 32; // 未指定格子边长时的默认值
constexpr int DEFAULT_DELAY_MS = 30; // 默认每回合画面间隔 毫秒
constexpr int DELAY_STEP_MS = 20; // 每次调整画面间隔的毫秒数
constexpr int DELAY_MIN_MS = 0; // 画面间隔下限
constexpr int DELAY_MAX_MS = 500; // 画面间隔上限
constexpr int UNKNOWN_TEAM_LIMIT = 16; // 队伍数未知时视野取值上限 保存 -1 到 15
constexpr int PANEL_PADDING = 10; // 信息栏内边距 像素
constexpr int ICON_THUMB_SIZE = 16; // 信息栏玩家图标缩略图边长 像素
constexpr int FOG_DARKEN_PERCENT = 38; // 视野外格子压暗比例 百分比
constexpr int MAX_TEAM_ROWS = 10; // 信息栏最多显示的队伍行数
constexpr int MAX_PLAYER_ROWS = 16; // 信息栏最多显示的玩家行数
constexpr int MESSAGE_DRAIN_LIMIT = 256; // 单次轮询最多处理的消息数
constexpr double PI = 3.14159265358979323846;

// ============================================================
// 颜色
// ============================================================

constexpr COLORREF COLOR_WINDOW_BACKGROUND = RGB(18, 20, 26); // 窗口底色
constexpr COLORREF COLOR_TOP_BAR = RGB(30, 33, 42); // 顶部信息条底色
constexpr COLORREF COLOR_PANEL = RGB(24, 26, 33); // 信息栏底色
constexpr COLORREF COLOR_PANEL_LINE = RGB(52, 58, 74); // 信息栏分隔线
constexpr COLORREF COLOR_TOP_BAR_LINE = RGB(64, 116, 178); // 顶部信息条底部亮线
constexpr COLORREF COLOR_GRID = RGB(13, 14, 18); // 网格线
constexpr COLORREF COLOR_NEUTRAL = RGB(66, 70, 80); // 中立格底图
constexpr COLORREF COLOR_TEXT = RGB(232, 234, 240); // 主要文字
constexpr COLORREF COLOR_TEXT_DIM = RGB(158, 166, 184); // 次要文字
constexpr COLORREF COLOR_TEXT_DARK = RGB(18, 19, 24); // 文字阴影
constexpr COLORREF COLOR_ACCENT = RGB(96, 176, 255); // 强调色 视野
constexpr COLORREF COLOR_GOLD = RGB(255, 208, 96); // 标题金色
constexpr COLORREF COLOR_ALIVE = RGB(132, 226, 152); // 存活绿色
constexpr COLORREF COLOR_DEAD = RGB(138, 140, 152); // 淘汰灰色
constexpr COLORREF COLOR_WARN = RGB(255, 150, 116); // 警告红色
constexpr COLORREF COLOR_ROW_HIGHLIGHT = RGB(44, 70, 112); // 选中行背景
constexpr COLORREF COLOR_FOG_MARK = RGB(150, 160, 182); // 视野外问号
constexpr COLORREF COLOR_FOG_TRIANGLE = RGB(122, 132, 154); // 视野外三角形
constexpr COLORREF COLOR_FOG_WAVE = RGB(96, 134, 112); // 视野外波浪

// 玩家纯色底图调色板 至少 12 种 区分度大
constexpr int PLAYER_PALETTE_SIZE = 16;
constexpr COLORREF PLAYER_PALETTE[PLAYER_PALETTE_SIZE] = {
    RGB(232, 62, 62), // 红
    RGB(58, 128, 236), // 蓝
    RGB(62, 190, 92), // 绿
    RGB(246, 182, 40), // 金黄
    RGB(170, 82, 222), // 紫
    RGB(42, 206, 206), // 青
    RGB(250, 132, 42), // 橙
    RGB(240, 92, 172), // 粉
    RGB(150, 212, 52), // 黄绿
    RGB(184, 132, 92), // 棕
    RGB(104, 122, 202), // 蓝灰
    RGB(212, 212, 222), // 浅灰
    RGB(0, 152, 132), // 墨绿
    RGB(162, 42, 102), // 紫红
    RGB(122, 202, 132), // 薄荷
    RGB(92, 94, 116) // 深灰蓝
};

// ============================================================
// 全局状态
// 窗口相关状态只在主线程读写 对外暴露的四个状态用原子量保证安全
// ============================================================

bool g_opened = false; // 窗口是否已创建
HWND g_window = nullptr; // 窗口句柄
std::atomic<WNDPROC> g_oldWindowProc{nullptr}; // EasyX 原来的窗口过程 窗口线程会读取
bool g_batchDrawing = false; // 是否已经开启批量绘制

int g_rows = 0; // 地图行数
int g_cols = 0; // 地图列数
int g_cellSize = DEFAULT_CELL_SIZE; // 格子边长
int g_mapWidth = 0; // 地图区域宽度 像素
int g_mapHeight = 0; // 地图区域高度 像素
int g_windowWidth = 0; // 窗口宽度
int g_windowHeight = 0; // 窗口高度

std::atomic<bool> g_quitRequested{false}; // 用户是否请求退出
std::atomic<bool> g_paused{false}; // 是否暂停
std::atomic<int> g_visionMode{-1}; // -1 为全局视野 否则为队伍编号
std::atomic<int> g_delayMs{DEFAULT_DELAY_MS}; // 每回合画面间隔
std::atomic<int> g_teamCount{0}; // 已知队伍数 0 表示未知

// 已加载并缩放好的玩家图标 格子图标用于地图 缩略图用于信息栏
struct IconEntry {
    std::unique_ptr<IMAGE> cellIcon; // 边长等于格子边长的图标
    std::unique_ptr<IMAGE> thumbIcon; // 信息栏玩家缩略图
};

std::map<std::string, IconEntry> g_iconCache; // 玩家图标 两个指针都为空表示加载失败
std::set<std::string> g_iconAspectWarned; // 已警告过比例异常的玩家
int g_lastMatchIndex = -1; // 上次绘制时的比赛序号 用于换比赛时清理图标缓存
bool g_sizeMismatchWarned = false; // 是否已经警告过视野尺寸与窗口尺寸不一致
bool g_bufferUnavailableWarned = false; // 是否已经警告过绘图缓冲不可用

struct RowHitRect {
    RECT rect{}; // 行区域
    int teamID = -1; // 对应队伍编号
};

std::vector<RowHitRect> g_teamRowHits; // 上次绘制的队伍行位置 供鼠标点击使用

// ============================================================
// 编码转换
// ============================================================

// 把 UTF-8 字符串转换为系统 ANSI 字符串 EasyX ANSI 版按系统代码页解释文本
std::string toAnsi(const std::string &utf8) {
    if(utf8.empty()) return std::string();
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if(wideLength <= 0) return utf8;
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), wideLength);
    int ansiLength = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), wideLength, nullptr, 0, nullptr, nullptr);
    if(ansiLength <= 0) return utf8;
    std::string ansi(static_cast<std::size_t>(ansiLength), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), wideLength, ansi.data(), ansiLength, nullptr, nullptr);
    return ansi;
}

// 判断字符串是否包含非 ASCII 字节
bool hasNonAscii(const std::string &text) {
    for(unsigned char byte : text) {
        if(byte >= 0x80) return true;
    }
    return false;
}

// 返回可直接交给 EasyX 使用的 ANSI C 字符串
// 纯 ASCII 直接返回 含中文时转换一次并缓存 避免每帧重复转换
const char* ansiCstr(const std::string &utf8) {
    if(utf8.empty()) return "";
    if(hasNonAscii(utf8) == false) return utf8.c_str();
    static std::unordered_map<std::string, std::string> cache;
    auto found = cache.find(utf8);
    if(found != cache.end()) return found->second.c_str();
    auto inserted = cache.emplace(utf8, toAnsi(utf8));
    return inserted.first->second.c_str();
}

// 返回中文字体名 微软雅黑
const std::string& chineseFace() {
    static const std::string face = toAnsi("微软雅黑");
    return face;
}

// 返回等宽字体名 Consolas
const std::string& monoFace() {
    static const std::string face = toAnsi("Consolas");
    return face;
}

// ============================================================
// 文本绘制工具
// ============================================================

// 设置文本字体 参数 height 为字号高度 像素
void useFont(int height, const std::string &face) {
    settextstyle(height, 0, face.c_str());
    setbkmode(TRANSPARENT);
}

// 在指定位置绘制一行文本 返回文本宽度
// 参数 shadow 为 true 时先绘制深色阴影 保证在任意底图上清晰
int drawTextRaw(int x, int y, const std::string &text, int height, const std::string &face,
                COLORREF color, bool shadow) {
    if(height < 6) height = 6;
    useFont(height, face);
    int width = textwidth(ansiCstr(text));
    if(shadow) {
        settextcolor(COLOR_TEXT_DARK);
        outtextxy(x + 1, y + 1, ansiCstr(text));
    }
    settextcolor(color);
    outtextxy(x, y, ansiCstr(text));
    return width;
}

// 绘制文本 自动缩小字号直到宽度不超过 maxWidth
int drawTextFit(int x, int y, const std::string &text, int height, int maxWidth,
                const std::string &face, COLORREF color, bool shadow) {
    if(maxWidth < 12) maxWidth = 12;
    int chosen = height;
    while(chosen > 8) {
        useFont(chosen, face);
        if(textwidth(ansiCstr(text)) <= maxWidth) break;
        chosen --;
    }
    return drawTextRaw(x, y, text, chosen, face, color, shadow);
}

// 绘制右对齐文本 返回左端 x 坐标
int drawTextRight(int right, int y, const std::string &text, int height,
                  const std::string &face, COLORREF color, bool shadow) {
    useFont(height, face);
    int width = textwidth(ansiCstr(text));
    drawTextRaw(right - width, y, text, height, face, color, shadow);
    return right - width;
}

// 在矩形区域中居中绘制文本 参数 left top width height 为区域
void drawTextInRect(int left, int top, int width, int height, const std::string &text,
                    int fontHeight, const std::string &face, COLORREF color, bool shadow) {
    if(fontHeight < 6) fontHeight = 6;
    useFont(fontHeight, face);
    int textWidth = textwidth(ansiCstr(text));
    int textHeight = textheight(ansiCstr(text));
    int x = left + (width - textWidth) / 2;
    int y = top + (height - textHeight) / 2;
    if(x < left) x = left;
    if(y < top) y = top;
    if(shadow) {
        settextcolor(COLOR_TEXT_DARK);
        outtextxy(x + 1, y + 1, ansiCstr(text));
    }
    settextcolor(color);
    outtextxy(x, y, ansiCstr(text));
}

// ============================================================
// 颜色工具
// ============================================================

// 按比例调整颜色亮度 返回调整后的颜色
COLORREF shiftBrightness(COLORREF color, double factor) {
    int red = static_cast<int>(GetRValue(color) * factor);
    int green = static_cast<int>(GetGValue(color) * factor);
    int blue = static_cast<int>(GetBValue(color) * factor);
    red = std::clamp(red, 0, 255);
    green = std::clamp(green, 0, 255);
    blue = std::clamp(blue, 0, 255);
    return RGB(red, green, blue);
}

// 按队伍编号取调色板颜色 参数 index 可以为负
COLORREF paletteColor(int index) {
    int position = index % PLAYER_PALETTE_SIZE;
    if(position < 0) position += PLAYER_PALETTE_SIZE;
    return PLAYER_PALETTE[position];
}

// ============================================================
// 玩家数据工具
// ============================================================

// 返回玩家纯色底图颜色 同队玩家用相近色相 不同亮度区分
COLORREF playerBaseColor(const FrameInfo &frame, const PlayerInfo &player) {
    int teamID = player.teamID;
    COLORREF color = paletteColor(teamID >= 0 ? teamID : player.playerID);
    int teammateIndex = 0;
    for(const PlayerInfo &other : frame.players) {
        if(other.playerID == player.playerID) continue;
        if(other.teamID != player.teamID) continue;
        if(other.playerID < player.playerID) teammateIndex ++;
    }
    double factor = 1.0 - 0.30 * teammateIndex;
    if(factor < 0.40) factor = 0.40;
    return shiftBrightness(color, factor);
}

// 判断格子是否在视野之外
bool isFogTile(const core::Tile<VisibleTerrain> &tile) {
    if(tile.owner == core::UNKNOWN) return true;
    return static_cast<int>(tile.terrain) >= static_cast<int>(VisibleTerrain::FogBlank);
}

// ============================================================
// 玩家图标
// ============================================================

// 判断玩家 id 是否可以安全地拼进文件路径
bool isSafePlayerName(const std::string &name) {
    if(name.empty()) return false;
    if(name == "." || name == "..") return false;
    for(char character : name) {
        switch(character) {
            case '/': case '\\': case ':': case '*': case '?':
            case '"': case '<': case '>': case '|': case '\0':
                return false;
            default:
                break;
        }
    }
    return true;
}

// 加载并缩放玩家图标 返回边长等于格子边长的图标 失败返回空指针
// 参数 backgroundColor 用作透明图标的底色 参数 name 为玩家 id
// 返回缓存条目 首次调用时尝试加载 失败后不再重复尝试
IconEntry& iconEntry(const std::string &name, COLORREF backgroundColor) {
    auto found = g_iconCache.find(name);
    if(found != g_iconCache.end()) return found->second;
    g_iconCache[name] = IconEntry{}; // 先占位 失败时不再重复尝试
    if(isSafePlayerName(name) == false) return g_iconCache[name];

    std::filesystem::path path = std::filesystem::path("battle") / name / "icon.png";
    std::error_code error;
    if(std::filesystem::exists(path, error) == false || std::filesystem::is_regular_file(path, error) == false) {
        logger.info("玩家 " + name + " 没有 icon.png 使用纯色底图");
        return g_iconCache[name];
    }

    IMAGE source;
    std::string ansiPath = toAnsi(path.string());
    loadimage(&source, ansiPath.c_str());
    int width = source.getwidth();
    int height = source.getheight();
    if(width <= 0 || height <= 0) {
        logger.warning("玩家 " + name + " 的 icon.png 加载失败 使用纯色底图 路径 " + path.string());
        return g_iconCache[name];
    }

    if(width != height && g_iconAspectWarned.count(name) == 0) {
        g_iconAspectWarned.insert(name);
        logger.warning("玩家 " + name + " 的 icon.png 宽高比例不是正方形 宽 " + std::to_string(width)
                       + " 高 " + std::to_string(height) + " 已居中裁切");
    }

    // 取中间正方形区域 分别缩放到格子大小与信息栏缩略图大小 保持比例不拉伸
    int square = std::min(width, height);
    int sourceX = (width - square) / 2;
    int sourceY = (height - square) / 2;
    auto cellImage = std::make_unique<IMAGE>(g_cellSize, g_cellSize);
    SetWorkingImage(cellImage.get());
    setfillcolor(backgroundColor);
    solidrectangle(0, 0, g_cellSize, g_cellSize);
    putimage(0, 0, g_cellSize, g_cellSize, &source, sourceX, sourceY);
    auto thumbImage = std::make_unique<IMAGE>(ICON_THUMB_SIZE, ICON_THUMB_SIZE);
    SetWorkingImage(thumbImage.get());
    setfillcolor(backgroundColor);
    solidrectangle(0, 0, ICON_THUMB_SIZE, ICON_THUMB_SIZE);
    putimage(0, 0, ICON_THUMB_SIZE, ICON_THUMB_SIZE, &source, sourceX, sourceY);
    SetWorkingImage(nullptr);

    g_iconCache[name].cellIcon = std::move(cellImage);
    g_iconCache[name].thumbIcon = std::move(thumbImage);
    return g_iconCache[name];
}

// 返回玩家地图底图 失败返回空指针
const IMAGE* playerIcon(const std::string &name, COLORREF backgroundColor) {
    return iconEntry(name, backgroundColor).cellIcon.get();
}

// 返回玩家信息栏缩略图 失败返回空指针
const IMAGE* playerThumb(const std::string &name, COLORREF backgroundColor) {
    return iconEntry(name, backgroundColor).thumbIcon.get();
}

// 比赛序号变化时清理图标缓存 队伍重新分配后底色可能变化
// 比例警告不清理 保证同一玩家在整个进程内只警告一次
void syncMatch(const FrameInfo &frame) {
    if(frame.matchIndex == g_lastMatchIndex) return;
    g_lastMatchIndex = frame.matchIndex;
    g_iconCache.clear();
}

// ============================================================
// 地形简笔画
// ============================================================

// 绘制五角星 参数 centerX centerY 为中心 radius 为外半径
void drawStar(int centerX, int centerY, int radius, COLORREF fillColor, COLORREF lineColor) {
    if(radius < 2) radius = 2;
    POINT points[10];
    for(int i = 0; i < 10; i ++) {
        double angle = -PI / 2.0 + i * PI / 5.0;
        double length = (i % 2 == 0) ? static_cast<double>(radius) : radius * 0.45;
        points[i] = POINT{ centerX + static_cast<int>(std::lround(std::cos(angle) * length)),
                           centerY + static_cast<int>(std::lround(std::sin(angle) * length)) };
    }
    setfillcolor(fillColor);
    setlinecolor(lineColor);
    fillpolygon(points, 10);
}

// 绘制三角山 参数 fog 为 true 时使用视野外配色
void drawMountain(int left, int top, int size, bool fog) {
    int centerX = left + size / 2;
    int centerY = top + size / 2;
    int halfBase = std::max(2, size * 34 / 100);
    int height = std::max(3, size * 66 / 100);
    POINT points[3] = {
        POINT{ centerX, centerY - height / 2 },
        POINT{ centerX - halfBase, centerY + height / 2 },
        POINT{ centerX + halfBase, centerY + height / 2 }
    };
    if(fog) {
        setfillcolor(RGB(32, 34, 42));
        setlinecolor(COLOR_FOG_TRIANGLE);
    }
    else {
        setfillcolor(RGB(56, 58, 66));
        setlinecolor(RGB(238, 240, 246));
    }
    fillpolygon(points, 3);
    if(fog == false && size >= 18) {
        // 白色雪顶 增加辨识度
        POINT snow[3] = {
            POINT{ centerX, centerY - height / 2 + 1 },
            POINT{ centerX - halfBase / 3, centerY - height / 2 + height / 3 },
            POINT{ centerX + halfBase / 3, centerY - height / 2 + height / 3 }
        };
        setfillcolor(RGB(222, 228, 238));
        setlinecolor(RGB(222, 228, 238));
        fillpolygon(snow, 3);
    }
}

// 绘制沼泽波浪 参数 fog 为 true 时使用视野外配色
void drawSwamp(int left, int top, int size, bool fog) {
    if(size < 6) return;
    int margin = std::max(1, size / 7);
    int startX = left + margin;
    int endX = left + size - margin;
    int amplitude = std::max(1, size / 14);
    int lineCount = size >= 16 ? 3 : 2;
    COLORREF color = fog ? COLOR_FOG_WAVE : RGB(176, 228, 186);
    constexpr int POINT_COUNT = 9;
    POINT points[POINT_COUNT];
    for(int lineIndex = 0; lineIndex < lineCount; lineIndex ++) {
        int baseY = top + size / 2 + (lineIndex - 1) * (size / 4);
        for(int i = 0; i < POINT_COUNT; i ++) {
            double ratio = static_cast<double>(i) / static_cast<double>(POINT_COUNT - 1);
            int x = startX + static_cast<int>((endX - startX) * ratio);
            int y = baseY + static_cast<int>(std::sin(ratio * 2.0 * PI) * amplitude);
            points[i] = POINT{ x, y };
        }
        setlinecolor(RGB(20, 24, 22));
        polyline(points, POINT_COUNT);
        setlinecolor(color);
        polyline(points, POINT_COUNT);
    }
}

// 绘制城市或首都 参数 capital 为 true 时在城市上方加金色五角星
// 图形只占格子中部 留出边缘显示玩家底图颜色
void drawCity(int left, int top, int size, bool capital) {
    if(size < 6) return;
    int centerX = left + size / 2;
    if(size >= 18) {
        int inset = size * 26 / 100;
        int roofTop = top + size * 36 / 100;
        int wallTop = top + size * 54 / 100;
        int wallBottom = top + size * 82 / 100;
        int bodyLeft = left + inset;
        int bodyRight = left + size - inset;
        // 屋顶
        POINT roof[3] = {
            POINT{ centerX, roofTop },
            POINT{ bodyLeft - 1, wallTop },
            POINT{ bodyRight + 1, wallTop }
        };
        setfillcolor(RGB(226, 228, 236));
        setlinecolor(RGB(22, 22, 28));
        fillpolygon(roof, 3);
        // 墙体
        setfillcolor(RGB(226, 228, 236));
        setlinecolor(RGB(22, 22, 28));
        fillrectangle(bodyLeft, wallTop, bodyRight, wallBottom);
        // 门
        int doorWidth = std::max(2, size / 10);
        int doorHeight = std::max(2, size / 6);
        setfillcolor(RGB(96, 72, 50));
        solidrectangle(centerX - doorWidth / 2, wallBottom - doorHeight,
                       centerX + doorWidth / 2, wallBottom);
    }
    else {
        int inset = size / 3;
        setfillcolor(RGB(222, 224, 232));
        setlinecolor(RGB(22, 22, 28));
        fillrectangle(left + inset, top + inset, left + size - inset, top + size - inset);
    }
    if(capital) {
        int starY = (size >= 18) ? (top + size * 20 / 100) : (top + size / 3);
        int radius = std::max(2, size * (size >= 18 ? 16 : 20) / 100);
        drawStar(centerX, starY, radius, COLOR_GOLD, RGB(30, 24, 10));
    }
}

// 绘制视野外疑似地形标记
void drawFogMark(VisibleTerrain terrain, int left, int top, int size) {
    switch(terrain) {
        case VisibleTerrain::FogMountain:
            drawMountain(left, top, size, true);
            break;
        case VisibleTerrain::FogSwamp:
            drawSwamp(left, top, size, true);
            break;
        case VisibleTerrain::FogBlank:
        default: {
            int fontHeight = std::max(8, size * 62 / 100);
            drawTextInRect(left, top, size, size, "?", fontHeight, monoFace(), COLOR_FOG_MARK, true);
            break;
        }
    }
}

// 绘制视野内真实地形 空地不绘制
void drawVisibleTerrain(VisibleTerrain terrain, int left, int top, int size) {
    switch(terrain) {
        case VisibleTerrain::Mountain:
            drawMountain(left, top, size, false);
            break;
        case VisibleTerrain::Swamp:
            drawSwamp(left, top, size, false);
            break;
        case VisibleTerrain::City:
            drawCity(left, top, size, false);
            break;
        case VisibleTerrain::Capital:
            drawCity(left, top, size, true);
            break;
        case VisibleTerrain::Blank:
        default:
            // 空地不绘制 保持底图干净
            break;
    }
}

// ============================================================
// 视野外格子压暗
// ============================================================

struct FogCell {
    int left = 0; // 左边界
    int top = 0; // 上边界
};

// 把视野外格子的底图按比例压暗 形成半透明暗色覆盖效果
void darkenFogCells(const std::vector<FogCell> &cells, int cellSize) {
    if(cells.empty()) return;
    DWORD* buffer = GetImageBuffer();
    if(buffer == nullptr) {
        if(g_bufferUnavailableWarned == false) {
            g_bufferUnavailableWarned = true;
            logger.warning("无法获取绘图缓冲区 视野外格子改为纯色覆盖");
        }
        setfillcolor(RGB(22, 24, 30));
        for(const FogCell &cell : cells) {
            solidrectangle(cell.left, cell.top, cell.left + cellSize - 1, cell.top + cellSize - 1);
        }
        return;
    }

    int stride = getwidth();
    int height = getheight();
    for(const FogCell &cell : cells) {
        int minX = std::max(0, cell.left);
        int minY = std::max(0, cell.top);
        int maxX = std::min(stride - 1, cell.left + cellSize - 1);
        int maxY = std::min(height - 1, cell.top + cellSize - 1);
        for(int y = minY; y <= maxY; y ++) {
            DWORD* row = buffer + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
            for(int x = minX; x <= maxX; x ++) {
                DWORD color = row[x];
                // 缓冲区像素格式为 0x00RRGGBB
                int red = static_cast<int>((color >> 16) & 0xFF) * FOG_DARKEN_PERCENT / 100;
                int green = static_cast<int>((color >> 8) & 0xFF) * FOG_DARKEN_PERCENT / 100;
                int blue = static_cast<int>(color & 0xFF) * FOG_DARKEN_PERCENT / 100;
                row[x] = static_cast<DWORD>((red << 16) | (green << 8) | blue);
            }
        }
    }
}

// ============================================================
// 地图绘制
// ============================================================

// 计算兵力数字字号 参数 cell 为格子边长 digits 为数字位数
int unitFontHeight(int cell, int digits) {
    if(digits < 1) digits = 1;
    int byHeight = cell * 45 / 100;
    int byWidth = (cell - 2) * 20 / (11 * digits);
    int height = std::min(byHeight, byWidth);
    return std::clamp(height, 7, 26);
}

// 绘制地图区域
void drawMap(const FrameInfo &frame) {
    const int cell = g_cellSize;
    const int mapTop = TOP_BAR_HEIGHT;
    int viewRows = frame.view.R;
    int viewCols = frame.view.C;
    int drawRows = std::min(g_rows, viewRows);
    int drawCols = std::min(g_cols, viewCols);
    if(viewRows > 0 && viewCols > 0 && (viewRows != g_rows || viewCols != g_cols) && g_sizeMismatchWarned == false) {
        g_sizeMismatchWarned = true;
        logger.warning("地图尺寸与窗口尺寸不一致 窗口 " + std::to_string(g_rows) + "x" + std::to_string(g_cols)
                       + " 视野 " + std::to_string(viewRows) + "x" + std::to_string(viewCols) + " 已按较小范围显示");
    }

    // 预先计算并加载每名玩家的底图样式
    struct PlayerStyle {
        COLORREF color = COLOR_NEUTRAL; // 纯色底图颜色
        const IMAGE* icon = nullptr; // 图标 空表示没有
    };
    std::map<int, PlayerStyle> styles;
    for(const PlayerInfo &player : frame.players) {
        PlayerStyle style;
        style.color = playerBaseColor(frame, player);
        style.icon = playerIcon(player.name, style.color);
        styles[player.playerID] = style;
    }

    // 第一遍 绘制全部格子底图 记录视野外格子
    std::vector<FogCell> fogCells;
    fogCells.reserve(static_cast<std::size_t>(drawRows) * static_cast<std::size_t>(std::max(drawCols, 0)));
    for(int x = 0; x < drawRows; x ++) {
        for(int y = 0; y < drawCols; y ++) {
            int left = y * cell;
            int top = mapTop + x * cell;
            const core::Tile<VisibleTerrain> &tile = frame.view.at(x, y);
            if(isFogTile(tile)) {
                setfillcolor(COLOR_NEUTRAL);
                solidrectangle(left, top, left + cell - 1, top + cell - 1);
                fogCells.push_back(FogCell{ left, top });
                continue;
            }
            if(tile.owner == core::NEUTRAL) {
                setfillcolor(COLOR_NEUTRAL);
                solidrectangle(left, top, left + cell - 1, top + cell - 1);
                continue;
            }
            auto style = styles.find(tile.owner);
            if(style != styles.end() && style->second.icon != nullptr) {
                putimage(left, top, style->second.icon);
            }
            else {
                COLORREF color = (style != styles.end()) ? style->second.color : paletteColor(tile.owner);
                setfillcolor(color);
                solidrectangle(left, top, left + cell - 1, top + cell - 1);
            }
        }
    }

    // 视野外格子压暗
    darkenFogCells(fogCells, cell);

    // 网格线
    setlinecolor(COLOR_GRID);
    for(int x = 0; x <= drawRows; x ++) {
        int lineY = mapTop + x * cell;
        if(lineY >= mapTop && lineY <= mapTop + g_mapHeight) {
            line(0, lineY, g_mapWidth - 1, lineY);
        }
    }
    for(int y = 0; y <= drawCols; y ++) {
        int lineX = y * cell;
        if(lineX >= 0 && lineX < g_mapWidth) {
            line(lineX, mapTop, lineX, mapTop + g_mapHeight - 1);
        }
    }

    // 第二遍 绘制地形简笔画
    for(int x = 0; x < drawRows; x ++) {
        for(int y = 0; y < drawCols; y ++) {
            int left = y * cell;
            int top = mapTop + x * cell;
            const core::Tile<VisibleTerrain> &tile = frame.view.at(x, y);
            if(isFogTile(tile)) {
                drawFogMark(tile.terrain, left, top, cell);
            }
            else {
                drawVisibleTerrain(tile.terrain, left, top, cell);
            }
        }
    }

    // 第三遍 绘制兵力数字
    int currentHeight = -1;
    for(int x = 0; x < drawRows; x ++) {
        for(int y = 0; y < drawCols; y ++) {
            int left = y * cell;
            int top = mapTop + x * cell;
            const core::Tile<VisibleTerrain> &tile = frame.view.at(x, y);
            if(isFogTile(tile)) continue;
            if(tile.unit <= 0) continue;
            std::string text = std::to_string(tile.unit);
            int height = unitFontHeight(cell, static_cast<int>(text.size()));
            if(height != currentHeight) {
                useFont(height, monoFace());
                currentHeight = height;
            }
            int textWidth = textwidth(ansiCstr(text));
            int textHeight = textheight(ansiCstr(text));
            int textX = left + (cell - textWidth) / 2;
            int textY = top + (cell - textHeight) / 2;
            if(textX < left) textX = left;
            if(textY < top) textY = top;
            // 先画一圈深色描边 再画白色数字 保证在任意底图与地形上都能看清
            settextcolor(COLOR_TEXT_DARK);
            outtextxy(textX - 1, textY, ansiCstr(text));
            outtextxy(textX + 1, textY, ansiCstr(text));
            outtextxy(textX, textY - 1, ansiCstr(text));
            outtextxy(textX, textY + 1, ansiCstr(text));
            settextcolor(COLOR_TEXT);
            outtextxy(textX, textY, ansiCstr(text));
        }
    }
}

// ============================================================
// 顶部信息条
// ============================================================

// 返回视野的描述文本
std::string visionText(int vision) {
    if(vision < 0) return "全局视野";
    return "队伍 " + std::to_string(vision) + " 视野";
}

// 绘制顶部信息条
void drawTopBar(const FrameInfo &frame) {
    setfillcolor(COLOR_TOP_BAR);
    solidrectangle(0, 0, g_windowWidth - 1, TOP_BAR_HEIGHT - 1);
    setlinecolor(COLOR_TOP_BAR_LINE);
    line(0, TOP_BAR_HEIGHT - 1, g_windowWidth - 1, TOP_BAR_HEIGHT - 1);

    int maxWidth = std::max(80, g_windowWidth - 2 * PANEL_PADDING);
    std::string mode = frame.modeName.empty() ? std::string("未知") : frame.modeName;
    std::string info = "比赛 " + std::to_string(frame.matchIndex) + "/" + std::to_string(frame.matchCount)
                       + "   第 " + std::to_string(frame.gameIndex) + "/" + std::to_string(frame.gameCount) + " 局"
                       + "   回合 " + std::to_string(frame.turn) + "/" + std::to_string(frame.maxTurns)
                       + "   模式 " + mode;
    drawTextFit(PANEL_PADDING, 7, info, 17, maxWidth, chineseFace(), COLOR_TEXT, false);

    std::string hint = "空格 暂停   0 全局视野   1-9 队伍视野   +/- 调整间隔   Esc 退出";
    std::string state;
    if(frame.gameOver) state = "对局结束 按任意键继续";
    else if(g_paused.load()) state = "已暂停";
    state += (state.empty() ? "" : "   ") + visionText(g_visionMode.load());
    state += "   间隔 " + std::to_string(g_delayMs.load()) + " ms";

    // 先量出状态文字宽度 再绘制左侧提示
    int stateHeight = 13;
    useFont(stateHeight, chineseFace());
    int stateWidth = textwidth(ansiCstr(state));
    int stateLeft = g_windowWidth - PANEL_PADDING - stateWidth;
    if(stateLeft < 0) stateLeft = 0;
    drawTextFit(PANEL_PADDING, 34, hint, 13, std::max(60, stateLeft - 2 * PANEL_PADDING - 8),
                chineseFace(), COLOR_TEXT_DIM, false);

    COLORREF stateColor = COLOR_TEXT_DIM;
    if(frame.gameOver) stateColor = COLOR_GOLD;
    else if(g_paused.load()) stateColor = COLOR_GOLD;
    else if(g_quitRequested.load()) stateColor = COLOR_WARN;
    drawTextRight(g_windowWidth - PANEL_PADDING, 34, state, stateHeight, chineseFace(), stateColor, false);
}

// ============================================================
// 右侧信息栏
// ============================================================

// 按名次排序队伍 名次为 0 的排在最后
std::vector<const TeamInfo*> sortedTeams(const FrameInfo &frame) {
    std::vector<const TeamInfo*> teams;
    teams.reserve(frame.teams.size());
    for(const TeamInfo &team : frame.teams) teams.push_back(&team);
    std::sort(teams.begin(), teams.end(), [](const TeamInfo *left, const TeamInfo *right) {
        int leftRank = left->rank > 0 ? left->rank : 100000 + left->teamID;
        int rightRank = right->rank > 0 ? right->rank : 100000 + right->teamID;
        if(leftRank != rightRank) return leftRank < rightRank;
        return left->teamID < right->teamID;
    });
    return teams;
}

// 按名次排序玩家 参数 ranks 为玩家排名 空时按编号排序
std::vector<const PlayerInfo*> sortedPlayers(const FrameInfo &frame) {
    std::vector<const PlayerInfo*> players;
    players.reserve(frame.players.size());
    for(const PlayerInfo &player : frame.players) players.push_back(&player);
    if(frame.playerRanks.empty() == false) {
        std::vector<const PlayerInfo*> ordered;
        ordered.reserve(players.size());
        for(const Rank &rank : frame.playerRanks) {
            for(const PlayerInfo *player : players) {
                if(player->playerID == rank.id) {
                    ordered.push_back(player);
                    break;
                }
            }
        }
        if(ordered.size() == players.size()) return ordered;
    }
    std::sort(players.begin(), players.end(), [](const PlayerInfo *left, const PlayerInfo *right) {
        if(left->units != right->units) return left->units > right->units;
        return left->playerID < right->playerID;
    });
    return players;
}

// 返回玩家名次 找不到返回 0
int playerRank(const FrameInfo &frame, int playerID) {
    for(const Rank &rank : frame.playerRanks) {
        if(rank.id == playerID) return rank.rank;
    }
    return 0;
}

// 绘制信息栏一行背景 参数 highlight 为 true 时使用高亮色
void drawRowBackground(int left, int right, int top, int height, bool highlight) {
    if(highlight == false) return;
    setfillcolor(COLOR_ROW_HIGHLIGHT);
    solidrectangle(left - 4, top - 2, right + 4, top + height - 4);
}

// 绘制玩家图标缩略图或纯色色块 返回绘制宽度
int drawPlayerMarker(const PlayerInfo &player, COLORREF color, int left, int top) {
    const IMAGE *thumb = playerThumb(player.name, color);
    if(thumb != nullptr) {
        putimage(left, top, thumb);
        return ICON_THUMB_SIZE;
    }
    setfillcolor(color);
    solidrectangle(left, top + 1, left + ICON_THUMB_SIZE - 1, top + ICON_THUMB_SIZE - 2);
    setlinecolor(COLOR_TEXT_DARK);
    rectangle(left, top + 1, left + ICON_THUMB_SIZE - 1, top + ICON_THUMB_SIZE - 2);
    return ICON_THUMB_SIZE;
}

// 绘制普通对局的信息栏内容
void drawNormalPanel(const FrameInfo &frame, int left, int right, int &y, int bottom) {
    const int baseFont = std::clamp(PANEL_WIDTH / 18, 13, 18);
    const int rowFont = std::clamp(baseFont - 2, 11, baseFont);
    const int rowHeight = rowFont + 6;
    const int maxTextWidth = right - left;

    // 赛场信息
    drawTextRaw(left, y, "赛场信息", baseFont, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 6;
    std::string mode = frame.modeName.empty() ? std::string("未知") : frame.modeName;
    drawTextRaw(left, y, "比赛 " + std::to_string(frame.matchIndex) + "/" + std::to_string(frame.matchCount)
                         + "   局 " + std::to_string(frame.gameIndex) + "/" + std::to_string(frame.gameCount),
                rowFont, chineseFace(), COLOR_TEXT, false);
    y += rowHeight;
    drawTextRaw(left, y, "回合 " + std::to_string(frame.turn) + "/" + std::to_string(frame.maxTurns),
                rowFont, chineseFace(), COLOR_TEXT, false);
    y += rowHeight;
    std::string modeText = "模式 " + mode;
    drawTextFit(left, y, modeText, rowFont, maxTextWidth, chineseFace(), COLOR_TEXT, false);
    y += rowHeight;

    // 视野
    drawTextFit(left, y, visionText(g_visionMode.load()), rowFont, maxTextWidth, chineseFace(), COLOR_ACCENT, false);
    y += rowHeight + 2;

    setlinecolor(COLOR_PANEL_LINE);
    line(left, y, right, y);
    y += 8;

    // 队伍排名
    drawTextRaw(left, y, "队伍排名", baseFont, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 4;
    std::vector<const TeamInfo*> teams = sortedTeams(frame);
    int teamShown = 0;
    for(const TeamInfo *team : teams) {
        if(teamShown >= MAX_TEAM_ROWS) break;
        if(y + rowHeight > bottom) break;
        int rank = team->rank > 0 ? team->rank : teamShown + 1;
        std::string text = std::to_string(rank) + " 队伍" + std::to_string(team->teamID)
                           + "   兵 " + std::to_string(team->units) + "   格 " + std::to_string(team->tiles);
        bool highlight = (g_visionMode.load() == team->teamID);
        drawRowBackground(left, right, y, rowHeight, highlight);
        drawTextFit(left + 2, y, text, rowFont, maxTextWidth - 4, chineseFace(),
                    highlight ? COLOR_ACCENT : COLOR_TEXT, false);
        g_teamRowHits.push_back(RowHitRect{ RECT{ left - 4, y - 2, right + 4, y + rowHeight - 4 }, team->teamID });
        y += rowHeight;
        teamShown ++;
    }
    if(teamShown == 0) {
        drawTextRaw(left, y, "暂无数据", rowFont, chineseFace(), COLOR_TEXT_DIM, false);
        y += rowHeight;
    }
    else if(teamShown < static_cast<int>(teams.size())) {
        drawTextRaw(left, y, "...  共 " + std::to_string(teams.size()) + " 支队伍", rowFont, chineseFace(), COLOR_TEXT_DIM, false);
        y += rowHeight;
    }

    y += 4;
    setlinecolor(COLOR_PANEL_LINE);
    line(left, y, right, y);
    y += 8;

    // 玩家列表
    drawTextRaw(left, y, "玩家列表", baseFont, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 4;
    std::vector<const PlayerInfo*> players = sortedPlayers(frame);
    int playerShown = 0;
    for(const PlayerInfo *player : players) {
        if(playerShown >= MAX_PLAYER_ROWS) break;
        if(y + rowHeight > bottom) break;
        bool highlight = (g_visionMode.load() >= 0 && g_visionMode.load() == player->teamID);
        if(highlight) drawRowBackground(left, right, y, rowHeight, true);
        COLORREF color = playerBaseColor(frame, *player);
        int markerWidth = drawPlayerMarker(*player, color, left, y + 1);
        std::string text = "P" + std::to_string(player->playerID) + " " + player->name
                           + "   队" + std::to_string(player->teamID)
                           + " 兵 " + std::to_string(player->units)
                           + " 格 " + std::to_string(player->tiles);
        std::string status = player->alive ? " 存活" : " 淘汰";
        COLORREF textColor = player->alive ? COLOR_TEXT : COLOR_DEAD;
        useFont(rowFont, chineseFace());
        int statusWidth = textwidth(ansiCstr(status));
        std::string shown = text;
        int available = maxTextWidth - markerWidth - 4 - statusWidth;
        if(available > 10) {
            while(shown.size() > 1 && textwidth(ansiCstr(shown)) > available) {
                shown.pop_back();
            }
        }
        int used = drawTextRaw(left + markerWidth + 4, y, shown, rowFont, chineseFace(),
                               highlight ? COLOR_ACCENT : textColor, false);
        drawTextRaw(left + markerWidth + 4 + used, y, status, rowFont, chineseFace(),
                    player->alive ? COLOR_ALIVE : COLOR_DEAD, false);
        y += rowHeight;
        playerShown ++;
    }
    if(playerShown == 0) {
        drawTextRaw(left, y, "暂无数据", rowFont, chineseFace(), COLOR_TEXT_DIM, false);
        y += rowHeight;
    }
    else if(playerShown < static_cast<int>(players.size())) {
        drawTextRaw(left, y, "...  共 " + std::to_string(players.size()) + " 名玩家", rowFont, chineseFace(), COLOR_TEXT_DIM, false);
        y += rowHeight;
    }
}

// 绘制终局信息栏内容 显示最终玩家排名与队伍排名
void drawGameOverPanel(const FrameInfo &frame, int left, int right, int &y, int bottom) {
    const int baseFont = std::clamp(PANEL_WIDTH / 18, 13, 18);
    const int rowFont = std::clamp(baseFont - 2, 11, baseFont);
    const int rowHeight = rowFont + 6;
    const int maxTextWidth = right - left;

    drawTextRaw(left, y, "对局结束", baseFont + 2, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 10;
    std::string mode = frame.modeName.empty() ? std::string("未知") : frame.modeName;
    drawTextRaw(left, y, "第 " + std::to_string(frame.gameIndex) + "/" + std::to_string(frame.gameCount)
                         + " 局   模式 " + mode,
                rowFont, chineseFace(), COLOR_TEXT, false);
    y += rowHeight + 4;

    setlinecolor(COLOR_PANEL_LINE);
    line(left, y, right, y);
    y += 8;

    // 最终玩家排名
    drawTextRaw(left, y, "最终玩家排名", baseFont, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 4;
    std::vector<const PlayerInfo*> players = sortedPlayers(frame);
    int playerShown = 0;
    for(const PlayerInfo *player : players) {
        if(y + rowHeight > bottom - rowHeight * 4) break;
        int rank = playerRank(frame, player->playerID);
        if(rank <= 0) rank = playerShown + 1;
        COLORREF color = playerBaseColor(frame, *player);
        int markerWidth = drawPlayerMarker(*player, color, left, y + 1);
        std::string text = std::to_string(rank) + " " + player->name
                           + "   队" + std::to_string(player->teamID)
                           + " 兵 " + std::to_string(player->units)
                           + " 格 " + std::to_string(player->tiles);
        std::string shown = text;
        int available = maxTextWidth - markerWidth - 4;
        if(available > 10) {
            useFont(rowFont, chineseFace());
            while(shown.size() > 1 && textwidth(ansiCstr(shown)) > available) shown.pop_back();
        }
        drawTextRaw(left + markerWidth + 4, y, shown, rowFont, chineseFace(),
                    player->alive ? COLOR_TEXT : COLOR_DEAD, false);
        y += rowHeight;
        playerShown ++;
    }
    if(playerShown == 0) {
        drawTextRaw(left, y, "暂无数据", rowFont, chineseFace(), COLOR_TEXT_DIM, false);
        y += rowHeight;
    }

    y += 4;
    setlinecolor(COLOR_PANEL_LINE);
    line(left, y, right, y);
    y += 8;

    // 最终队伍排名
    drawTextRaw(left, y, "最终队伍排名", baseFont, chineseFace(), COLOR_GOLD, false);
    y += baseFont + 4;
    std::vector<const TeamInfo*> teams = sortedTeams(frame);
    int teamShown = 0;
    for(const TeamInfo *team : teams) {
        if(y + rowHeight > bottom - baseFont - 4) break;
        int rank = team->rank > 0 ? team->rank : teamShown + 1;
        std::string text = std::to_string(rank) + " 队伍" + std::to_string(team->teamID)
                           + "   兵 " + std::to_string(team->units) + "   格 " + std::to_string(team->tiles);
        drawTextFit(left + 2, y, text, rowFont, maxTextWidth - 4, chineseFace(), COLOR_TEXT, false);
        y += rowHeight;
        teamShown ++;
    }

    // 底部提示
    int promptY = std::max(y + 6, bottom - baseFont - 4);
    drawTextFit(left, promptY, "按任意键继续", rowFont, maxTextWidth, chineseFace(), COLOR_TEXT_DIM, false);
}

// 绘制右侧信息栏
void drawPanel(const FrameInfo &frame) {
    g_teamRowHits.clear();
    int panelLeft = g_mapWidth;
    setfillcolor(COLOR_PANEL);
    solidrectangle(panelLeft, TOP_BAR_HEIGHT, g_windowWidth - 1, g_windowHeight - 1);
    setlinecolor(COLOR_PANEL_LINE);
    line(panelLeft, TOP_BAR_HEIGHT, panelLeft, g_windowHeight - 1);

    int left = panelLeft + PANEL_PADDING;
    int right = g_windowWidth - PANEL_PADDING;
    if(right - left < 40) return;
    int y = TOP_BAR_HEIGHT + 8;
    int bottom = g_windowHeight - 8;
    if(frame.gameOver) {
        drawGameOverPanel(frame, left, right, y, bottom);
    }
    else {
        drawNormalPanel(frame, left, right, y, bottom);
    }
}

// ============================================================
// 地图中间横幅
// ============================================================

// 在地图区域中央绘制提示横幅 参数 title 为主标题 subtitle 为副标题
void drawBanner(const std::string &title, const std::string &subtitle, COLORREF accent) {
    if(g_mapWidth < 160 || g_mapHeight < 90) return;
    int width = std::min(g_mapWidth - 32, std::max(220, g_mapWidth * 6 / 10));
    int height = 82;
    int left = (g_mapWidth - width) / 2;
    int top = TOP_BAR_HEIGHT + (g_mapHeight - height) / 2;
    setfillcolor(RGB(16, 18, 24));
    solidrectangle(left, top, left + width - 1, top + height - 1);
    setlinestyle(PS_SOLID, 2);
    setlinecolor(accent);
    rectangle(left, top, left + width - 1, top + height - 1);
    setlinestyle(PS_SOLID, 1);
    int fontHeight = std::clamp(width / 16, 16, 30);
    drawTextInRect(left, top + 10, width, 36, title, fontHeight, chineseFace(), accent, false);
    drawTextInRect(left, top + 46, width, 26, subtitle, std::max(11, fontHeight - 6), chineseFace(), COLOR_TEXT, false);
}

// ============================================================
// 状态更新与消息处理
// ============================================================

// 根据一帧数据更新已知队伍数并钳制当前视野
void updateTeamCount(const FrameInfo &frame) {
    int count = static_cast<int>(frame.teams.size());
    for(const TeamInfo &team : frame.teams) {
        if(team.teamID >= 0 && team.teamID + 1 > count) count = team.teamID + 1;
    }
    for(const PlayerInfo &player : frame.players) {
        if(player.teamID >= 0 && player.teamID + 1 > count) count = player.teamID + 1;
    }
    if(count <= 0) return;
    g_teamCount.store(count);
    int vision = g_visionMode.load();
    if(vision >= count) g_visionMode.store(count - 1);
}

// 处理一个按键 参数 vkCode 为虚拟键码
void handleKeyMessage(const ExMessage &message) {
    if(message.prevdown) return; // 忽略按住不放的重复按键
    UINT vkCode = message.vkcode;
    switch(vkCode) {
        case VK_SPACE:
            Renderer::setPaused(g_paused.load() == false);
            return;
        case VK_ESCAPE:
            g_quitRequested.store(true);
            return;
        case VK_OEM_PLUS:
        case VK_ADD:
        case '=':
            Renderer::adjustDelay(DELAY_STEP_MS);
            return;
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
        case '-':
            Renderer::adjustDelay(-DELAY_STEP_MS);
            return;
        default:
            break;
    }
    int digit = -1;
    if(vkCode >= '0' && vkCode <= '9') digit = static_cast<int>(vkCode - '0');
    else if(vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9) digit = static_cast<int>(vkCode - VK_NUMPAD0);
    if(digit < 0) return;
    if(digit == 0) Renderer::setVisionMode(-1);
    else Renderer::setVisionMode(digit - 1);
}

// 判断点是否落在矩形内
bool pointInRect(const RECT &rect, int x, int y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

// 处理鼠标按下 参数 leftButton 表示是否为左键
void handleMouseDownMessage(const ExMessage &message, bool leftButton) {
    int x = static_cast<int>(message.x);
    int y = static_cast<int>(message.y);
    if(leftButton == false) {
        Renderer::setVisionMode(-1); // 右键恢复全局视野
        return;
    }
    for(const RowHitRect &hit : g_teamRowHits) {
        if(pointInRect(hit.rect, x, y)) {
            Renderer::setVisionMode(hit.teamID);
            return;
        }
    }
    if(y < TOP_BAR_HEIGHT) {
        Renderer::setVisionMode(-1); // 点击顶部信息条恢复全局视野
    }
}

// 处理鼠标滚轮 参数 wheel 为滚轮增量
void handleWheelMessage(const ExMessage &message) {
    int steps = static_cast<int>(message.wheel) / WHEEL_DELTA;
    if(steps == 0) return;
    Renderer::adjustDelay(steps * DELAY_STEP_MS);
}

// 窗口过程 关窗按钮只记录退出请求 不真的销毁窗口 由 runner 决定何时关闭
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if(message == WM_CLOSE) {
        g_quitRequested.store(true);
        return 0;
    }
    WNDPROC previousProc = g_oldWindowProc.load();
    if(previousProc != nullptr) {
        return CallWindowProcA(previousProc, window, message, wParam, lParam);
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

// 清空主线程消息队列 避免无窗口时消息堆积
void drainThreadMessages() {
    MSG message;
    int guard = 0;
    while(PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if(++ guard >= MESSAGE_DRAIN_LIMIT) break;
    }
}

} // namespace

// ============================================================
// 对外接口
// ============================================================

void Renderer::open(int rows, int cols, int cellSize) {
    if(g_opened) close();

    int rowCount = rows > 0 ? rows : 1;
    int colCount = cols > 0 ? cols : 1;
    if(rowCount != rows || colCount != cols) {
        logger.warning("界面地图尺寸非法 已修正为 " + std::to_string(rowCount) + "x" + std::to_string(colCount));
    }

    int cell = cellSize > 0 ? cellSize : DEFAULT_CELL_SIZE;
    if(cell < MIN_CELL_SIZE) {
        logger.warning("界面格子边长过小 已修正为 " + std::to_string(MIN_CELL_SIZE));
        cell = MIN_CELL_SIZE;
    }

    // 窗口超出屏幕时按比例缩小格子边长 最小保留 8 像素
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int frameMarginWidth = 24; // 窗口左右边框预留
    const int frameMarginHeight = 96; // 标题栏与任务栏预留
    long long maxMapWidth = static_cast<long long>(screenWidth) - PANEL_WIDTH - frameMarginWidth;
    long long maxMapHeight = static_cast<long long>(screenHeight) - TOP_BAR_HEIGHT - frameMarginHeight;
    if(maxMapWidth < MIN_CELL_SIZE) maxMapWidth = MIN_CELL_SIZE;
    if(maxMapHeight < MIN_CELL_SIZE) maxMapHeight = MIN_CELL_SIZE;
    long long wantedWidth = static_cast<long long>(colCount) * cell;
    long long wantedHeight = static_cast<long long>(rowCount) * cell;
    if(wantedWidth > maxMapWidth || wantedHeight > maxMapHeight) {
        double scale = 1.0;
        if(wantedWidth > maxMapWidth) {
            scale = std::min(scale, static_cast<double>(maxMapWidth) / static_cast<double>(wantedWidth));
        }
        if(wantedHeight > maxMapHeight) {
            scale = std::min(scale, static_cast<double>(maxMapHeight) / static_cast<double>(wantedHeight));
        }
        int scaled = static_cast<int>(std::floor(static_cast<double>(cell) * scale));
        if(scaled < MIN_CELL_SIZE) scaled = MIN_CELL_SIZE;
        if(scaled != cell) {
            logger.info("界面窗口超出屏幕 格子边长从 " + std::to_string(cell) + " 缩小为 " + std::to_string(scaled));
            cell = scaled;
        }
    }

    g_rows = rowCount;
    g_cols = colCount;
    g_cellSize = cell;
    g_mapWidth = colCount * cell;
    g_mapHeight = rowCount * cell;
    g_windowWidth = PANEL_WIDTH + g_mapWidth;
    g_windowHeight = TOP_BAR_HEIGHT + g_mapHeight;
    if(g_windowWidth > screenWidth || g_windowHeight > screenHeight) {
        logger.warning("界面窗口仍然超出屏幕 宽 " + std::to_string(g_windowWidth) + " 高 " + std::to_string(g_windowHeight)
                       + " 屏幕宽 " + std::to_string(screenWidth) + " 高 " + std::to_string(screenHeight));
    }

    HWND window = initgraph(g_windowWidth, g_windowHeight, EX_SHOWCONSOLE);
    if(window == nullptr) {
        logger.error("创建界面窗口失败 宽 " + std::to_string(g_windowWidth) + " 高 " + std::to_string(g_windowHeight));
        return;
    }
    g_window = window;

    // 记录原窗口过程并换成自己的 用于拦截关窗按钮
    WNDPROC previousProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window, GWLP_WNDPROC));
    g_oldWindowProc.store(previousProc);
    if(SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(windowProc)) == 0 && previousProc == nullptr) {
        logger.warning("无法安装窗口消息钩子 关窗按钮可能直接关闭窗口");
    }
    SetWindowTextA(window, ansiCstr("generals.io serve"));

    // 重置本次打开的状态
    g_quitRequested.store(false);
    g_paused.store(false);
    g_teamCount.store(0);
    g_lastMatchIndex = -1;
    g_sizeMismatchWarned = false;
    g_bufferUnavailableWarned = false;
    g_iconCache.clear();
    g_iconAspectWarned.clear();
    g_teamRowHits.clear();

    setbkcolor(COLOR_WINDOW_BACKGROUND);
    cleardevice();
    BeginBatchDraw();
    g_batchDrawing = true;
    g_opened = true;
    FlushBatchDraw();
    logger.info("界面窗口创建成功 宽 " + std::to_string(g_windowWidth) + " 高 " + std::to_string(g_windowHeight)
                + " 网格 " + std::to_string(rowCount) + "x" + std::to_string(colCount)
                + " 格子边长 " + std::to_string(cell));
}

void Renderer::close() {
    if(g_opened == false) {
        g_iconCache.clear();
        return;
    }
    g_opened = false;
    WNDPROC previousProc = g_oldWindowProc.load();
    if(g_window != nullptr && previousProc != nullptr) {
        SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousProc));
    }
    g_window = nullptr;
    if(g_batchDrawing) {
        EndBatchDraw();
        g_batchDrawing = false;
    }
    closegraph();
    g_iconCache.clear();
    g_iconAspectWarned.clear();
    g_teamRowHits.clear();
    logger.info("界面窗口已关闭");
}

bool Renderer::isOpen() {
    return g_opened;
}

void Renderer::poll() {
    if(g_opened == false) {
        drainThreadMessages();
        return;
    }
    ExMessage message;
    int guard = 0;
    while(peekmessage(&message, EX_KEY | EX_MOUSE, true)) {
        if(++ guard > MESSAGE_DRAIN_LIMIT) break;
        switch(message.message) {
            case WM_KEYDOWN:
                handleKeyMessage(message);
                break;
            case WM_LBUTTONDOWN:
                handleMouseDownMessage(message, true);
                break;
            case WM_RBUTTONDOWN:
                handleMouseDownMessage(message, false);
                break;
            case WM_MOUSEWHEEL:
                handleWheelMessage(message);
                break;
            default:
                break;
        }
    }
    drainThreadMessages();
}

void Renderer::render(const FrameInfo &frame) {
    if(g_opened == false || g_window == nullptr) return;
    updateTeamCount(frame);
    syncMatch(frame);

    setbkcolor(COLOR_WINDOW_BACKGROUND);
    cleardevice();
    drawMap(frame);
    drawTopBar(frame);
    drawPanel(frame);
    if(frame.gameOver) {
        drawBanner("对局结束", "按任意键继续", COLOR_GOLD);
    }
    else if(g_paused.load()) {
        drawBanner("已暂停", "按空格继续", COLOR_GOLD);
    }
    FlushBatchDraw();
}

bool Renderer::quitRequested() {
    return g_quitRequested.load();
}

bool Renderer::paused() {
    return g_paused.load();
}

void Renderer::setPaused(bool value) {
    g_paused.store(value);
}

int Renderer::visionMode() {
    return g_visionMode.load();
}

void Renderer::setVisionMode(int value) {
    int upper = UNKNOWN_TEAM_LIMIT - 1;
    int known = g_teamCount.load();
    if(known > 0) upper = known - 1;
    if(upper < 0) upper = 0;
    int clamped = value;
    if(clamped < -1) clamped = -1;
    if(clamped > upper) clamped = upper;
    g_visionMode.store(clamped);
}

int Renderer::delayMs() {
    return g_delayMs.load();
}

void Renderer::adjustDelay(int deltaMs) {
    int value = g_delayMs.load() + deltaMs;
    value = std::clamp(value, DELAY_MIN_MS, DELAY_MAX_MS);
    g_delayMs.store(value);
}

} // namespace gen::ui
