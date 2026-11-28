# generals.io 离线对战服务器

本项目是一个 generals.io 的离线对战服务器。它不是 generals.io 官方项目，与 generals.io 官方无关。

它可以在本机运行 generals.io 对局。玩家不用写客户端，只要写一个 AI 程序。服务器负责生成地图、推进回合、判定胜负、显示画面、统计排名。

主要功能：

- 支持 1v1、2v2、FFA 三种模式。
- 支持自动匹配。开启后自动枚举 `battle/` 下的玩家组合，组织比赛。
- 支持计分。按排名给玩家和队伍算加权总分。
- 支持图形界面。可以看到地图、地形、兵力、排名。
- 自带示例 AI。写 AI 的人可以参考示例代码。
- 支持无界面模式。方便自动测试和批量跑局。
- 全部静态编译。运行时不依赖 MinGW 的动态库。

本项目的运行程序叫 `serve.exe`。

## 目录

1. [项目是什么](#1-项目是什么)
2. [如何查看 md 文档](#2-如何查看-md-文档)
3. [如何安装或配置 MinGW](#3-如何安装或配置-mingw)
4. [如何验证编译环境](#4-如何验证编译环境)
5. [如何构建](#5-如何构建)
6. [如何修改配置](#6-如何修改配置)
7. [如何放置玩家 AI](#7-如何放置玩家-ai)
8. [如何运行一局游戏](#8-如何运行一局游戏)
9. [如何查看日志](#9-如何查看日志)
10. [如何继续开发](#10-如何继续开发)
11. [如何运行测试](#11-如何运行测试)
12. [常见问题](#12-常见问题)
13. [文档要求](#13-文档要求)

## 1. 项目是什么

generals.io 是一个多人策略游戏。规则见 [generals.io游戏规则](generals.io游戏规则.md)。

本项目的运行方式：

1. 用 `serve.exe` 启动服务器。
2. 服务器读取配置，生成地图。
3. 服务器启动各个玩家 AI 程序。
4. 服务器和 AI 程序通过标准输入、标准输出互相传消息。
5. 每回合 AI 输出一个动作，服务器执行动作。
6. 一局结束后服务器算分，输出排名。

玩家 AI 是一个独立的可执行程序。服务器把它当作外部进程运行，不把 AI 代码编进 `serve.exe`。

相关文档：

- 想写 AI，看 [选手须知](选手须知.md)。
- 想了解游戏规则，看 [generals.io游戏规则](generals.io游戏规则.md)。
- 想改代码，看 [开发文档](doc/文档.md)。
- 想改配置，看 [配置说明](doc/配置说明.md)。
- 想了解构建环境，看 [构建与环境](doc/构建与环境.md)。
- 想看测试方案，看 [测试说明](doc/测试说明.md)。

## 2. 如何查看 md 文档

本仓库的文档都是 Markdown 文件，后缀为 `.md`。推荐用下面两种工具查看。

### 2.1 用 VS Code 查看

1. 下载并安装 VS Code，地址是 <https://code.visualstudio.com/>。
2. 打开 VS Code，选择 `文件`，再选择 `打开文件夹`，选中本仓库根目录。
3. 在左侧文件列表里点击任意 `.md` 文件。
4. 按 `Ctrl+Shift+V` 打开预览。也可以右键文件，选择 `打开预览`。
5. 预览窗口会显示标题、列表、表格等格式。

VS Code 自带的预览不能显示数学公式。需要看公式时，安装一个 Markdown 数学插件，例如 Markdown Preview Enhanced。安装后重新打开预览即可。

### 2.2 用 Obsidian 查看

1. 下载并安装 Obsidian，地址是 <https://obsidian.md/>。
2. 打开 Obsidian，选择 `打开本地仓库`，选中本仓库根目录。
3. 在左侧文件列表里点击任意 `.md` 文件。
4. Obsidian 会直接显示排版好的文档，并且支持数学公式。

仓库里已有 `.obsidian` 目录，所以 Obsidian 可以把它当作仓库直接打开。

### 2.3 其他工具

Windows 记事本、浏览器插件、Typora 等工具也可以打开 `.md` 文件。公式显示效果以 Obsidian 最好。

## 3. 如何安装或配置 MinGW

编译本项目需要 64 位 MinGW-w64 GCC，并且要支持 C++20。本项目在 `g++ 16.2.0` 上测试通过。

代码用到了 C++20 的 `<format>` 等特性。建议使用 GCC 13 或更高版本。

### 3.1 下载

可以从下面两个地址下载：

- <https://github.com/niXman/mingw-builds-binaries/releases>
- <https://winlibs.com/>

下载时注意：

- 选 64 位版本。文件名里通常带 `x86_64` 或 `x86_64-w64-mingw32`。
- 选带 `g++` 的完整版本，不要只装一个精简包。
- 选 release 版本，不要选 debug 版本。
- UCRT 和 MSVCRT 两种运行时都可以。拿不准就选 UCRT。

### 3.2 解压

1. 把下载的压缩包解压到一个固定目录，例如 `D:\mingw64`。
2. 注意路径不要有中文和空格，例如不要放在 `C:\Program Files` 下。
3. 打开解压后的目录，确认里面有 `bin` 文件夹。
4. 再打开 `bin` 文件夹，确认里面有 `g++.exe` 和 `gcc.exe`。

### 3.3 把 bin 目录加入 PATH

1. 按 `Win + S` 打开搜索，输入 `环境变量`。
2. 选择 `编辑系统环境变量`。
3. 在弹出窗口点 `环境变量`。
4. 在 `系统变量` 列表里找到 `Path`，选中它，点 `编辑`。
5. 点 `新建`，填入 MinGW 的 bin 目录完整路径，例如 `D:\mingw64\bin`。
6. 点三次 `确定`，关闭全部窗口。

改完 PATH 后，必须关闭所有旧命令行窗口。旧窗口不会读取新的 PATH。

### 3.4 测试用的 PowerShell

测试脚本使用 Windows 自带的 Windows PowerShell 5.1 即可，不需要额外安装 PowerShell 7。测试脚本已经兼容 5.1。

运行测试时主推下面这条命令：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

参数说明：

- `powershell` 是 Windows 自带的命令。
- `-NoProfile` 不加载个人配置，避免环境差异。
- `-ExecutionPolicy Bypass` 只对本次运行放宽脚本执行策略。
- `-File` 指定要运行的脚本。

如果电脑上已经安装了 PowerShell 7，也可以用 `pwsh`：

```pwsh
pwsh -File tests\run_tests.ps1
```

注意：测试脚本文件必须保存为 UTF-8 BOM 编码。Windows PowerShell 5.1 读取没有 BOM 的 UTF-8 脚本时可能按 ANSI 解码，中文会乱码。用 VS Code 修改脚本时，右下角编码选择 `UTF-8 with BOM` 再保存。

## 4. 如何验证编译环境

1. 按 `Win + R`，输入 `cmd`，按回车，打开命令行。
2. 输入下面的命令，按回车。

```bat
g++ --version
```

3. 再输入下面的命令，按回车。

```bat
gcc --version
```

4. 正常时会显示版本号，例如 `g++ (x86_64-win32-seh-rev0, Built by MinGW-Builds project) 16.2.0`。
5. 只要能显示版本号，说明编译器已经可用。

如果提示 `'g++' 不是内部或外部命令`，说明 PATH 没有配好。请回到第 3 节重新配置，并确认已经打开新的命令行窗口。

## 5. 如何构建

### 5.1 双击构建

1. 打开仓库根目录。
2. 双击 `build.bat`。
3. 窗口会依次做三件事。
   - `[1/3] Check compiler`：检查编译器，打印版本。
   - `[2/3] Build serve.exe`：编译主程序。
   - `[3/3] Build sample players`：编译 `battle/` 下的示例玩家。
4. 最后显示 `Build finished` 就是成功。
5. 按任意键关闭窗口。

构建成功后，根目录会生成 `serve.exe`。示例玩家会生成在各自的目录，例如 `battle/example/example.exe`。

### 5.2 命令行构建

打开命令行，进入仓库根目录，运行：

```bat
build.bat
```

自动测试或不想手动按键盘时，可以加参数：

```bat
build.bat --no-pause
```

这样构建结束后窗口不会暂停，方便脚本调用。

### 5.3 失败提示

`build.bat` 为了避免中文编码问题，提示信息使用英文。常见提示的中文含义如下：

| 英文提示 | 中文含义 | 处理方法 |
| --- | --- | --- |
| `[ERROR] g++ compiler not found.` | 找不到 g++ 编译器 | 检查 MinGW 是否安装，检查 PATH |
| `Install MinGW-w64 and add its bin directory to PATH.` | 安装 MinGW-w64，把 bin 目录加入 PATH | 按第 3 节操作 |
| `Download https://github.com/niXman/...` | 这里是下载地址 | 打开地址下载 |
| `Open a NEW terminal after install and run: g++ --version` | 安装后打开新命令行验证 | 关闭旧窗口，重新打开 |
| `[ERROR] serve.exe build failed. Read the compiler errors above.` | 主程序编译失败，看上面的编译器错误 | 按编译器提示修改 |
| `[WARN] battle\xxx\build.bat not found, skip player xxx` | 缺少示例玩家构建脚本，跳过该玩家 | 检查 `battle/xxx` 目录是否完整 |
| `[WARN] player xxx build failed` | 示例玩家编译失败 | 看该玩家目录下的构建输出 |
| `[TIP] some sample players were not built` | 部分示例玩家没构建成功，多人模式可能无法运行 | 先解决示例玩家构建问题 |

注意：示例玩家是多人模式测试用的。缺少示例玩家时，1v1、2v2、FFA 可能因为找不到玩家而无法开始比赛。

## 6. 如何修改配置

配置文件都放在 `config` 文件夹。默认使用 `config/example.json`。

仓库自带预设：

| 文件 | 用途 |
| --- | --- |
| `config/example.json` | 默认配置，展示全部配置键 |
| `config/1v1.json` | 1v1 模式预设 |
| `config/2v2.json` | 2v2 模式预设 |
| `config/ffa.json` | FFA 模式预设 |
| `config/auto.json` | 自动匹配预设 |

`config` 下只有上面这五份预设配置：`example.json`、`1v1.json`、`2v2.json`、`ffa.json`、`auto.json`。没有其他配置文件。

运行其他预设时加 `--config` 参数，例如：

```bat
serve.exe --config config\ffa.json
```

配置文件是 JSON。内容分为五段：`map`、`game`、`match`、`ui`、`log`。

主要配置键概览：

| 配置段 | 主要内容 |
| --- | --- |
| `map` | 地图行列数、禁用地形、随机种子、地形概率、城市兵力 |
| `game` | 模式、最大回合数、每场比赛局数、FFA 玩家数、时限、违规上限 |
| `match` | 是否自动匹配、队伍与玩家列表、玩家分数权重、队伍分数权重 |
| `ui` | 是否显示界面、格子大小、每回合停留时间、终局停留时间 |
| `log` | 日志目录、是否记录玩家操作 |

每个键的作用、取值范围、写法见 [配置说明](doc/配置说明.md)。

提示：

- 缺少某个键时，程序使用默认值，并在日志里写警告。
- 键名写错会被当作未知键，程序写警告并忽略。
- 配置类型错误或取值越界时，程序打印中文错误信息，退出码为 1。
- 想让比赛跑快一点，可以改小 `game.maxTurns`，或者把 `ui.enabled` 设为 `false`。

## 7. 如何放置玩家 AI

### 7.1 目录规则

玩家 AI 放在 `battle` 文件夹。规则是：

```text
battle/<id>/<id>.exe
```

其中 `<id>` 是玩家昵称。命名规则：

- 只能包含大小写英文字母、数字、下划线、短横线。
- 长度 1 到 64。
- 文件夹名必须和可执行文件名一致。
- 如果文件夹名和 exe 文件名不一致，这个玩家会被忽略，并在日志里写警告。

例如玩家 `example` 的目录结构：

```text
battle/example/
├─ example.cpp
├─ build.bat
└─ example.exe
```

### 7.2 工作目录与通信

服务器启动玩家程序时：

- 可执行文件路径是 `battle/<id>/<id>.exe`。
- 工作目录是 `battle/<id>`。
- 不加额外命令行参数。
- 服务器通过玩家程序的标准输入发送消息。
- 玩家程序通过标准输出发送动作。
- 玩家程序的标准错误会写入 `logs/ai/` 下的文件。
- 标准输出只能输出协议内容。调试信息必须写到标准错误。

玩家目录里还可以放一张 `icon.png`，作为界面里的玩家底图。图片要求正方形。比例不对时程序会警告并自动裁切。详见 [选手须知](选手须知.md)。

通信协议见 [选手须知](选手须知.md)。协议内容包括消息类型、帧格式、坐标、地形编码、动作格式、超时与异常处理。

### 7.3 示例 AI

`battle/example/example.cpp` 是示例 AI，注释完整，适合作为入门参考。

编译示例 AI 的两种方式：

- 双击 `battle/example/build.bat`。
- 在根目录运行 `build.bat`，它会自动编译示例玩家。

编译完成后得到 `battle/example/example.exe`。

### 7.4 在配置里使用玩家

关闭自动匹配时，要在 `match.teams` 里写玩家 id，例如：

```json
"teams": [["example"], ["demo_expand"]]
```

开启自动匹配时，`match.teams` 会被忽略。服务器会枚举 `battle` 下的全部可用玩家。

### 7.5 安全限制

当前版本只做了超时、崩溃、非法输出的处理。玩家程序以当前用户权限运行，没有系统级沙箱。

所以只运行可信的 AI 程序。不要运行来路不明的程序。

## 8. 如何运行一局游戏

### 8.1 带界面运行

双击 `serve.exe` 可以直接运行，默认读取 `config/example.json`。

也可以在命令行运行：

```bat
serve.exe --config config\example.json
```

运行 2v2、FFA、自动匹配时换成对应配置：

```bat
serve.exe --config config\2v2.json
serve.exe --config config\ffa.json
serve.exe --config config\auto.json
```

界面按键：

| 按键 | 作用 |
| --- | --- |
| `0` | 切换到全局视野 |
| `1` 到 `9` | 切换到对应队伍的视野 |
| `空格` | 暂停或继续 |
| `+` | 加快播放速度，减少每回合停留时间 |
| `-` | 减慢播放速度，增加每回合停留时间 |
| `Esc` | 退出 |

注意：`Esc` 会中止本次运行。当前局不计分，后面没跑的比赛也不会继续。

### 8.2 无界面运行

自动测试或快速跑局时使用无界面模式：

```bat
serve.exe --config config\example.json --no-ui
```

无界面模式下不创建窗口，只输出文字结果。

### 8.3 其他命令行参数

| 参数 | 作用 |
| --- | --- |
| `--config <路径>` | 指定配置文件，默认 `config/example.json` |
| `--no-ui` | 不显示图形界面 |
| `--seed <数字>` | 指定随机种子，方便复现同一张地图 |
| `--no-pause` | 出错时不等待按键，方便自动测试 |
| `--help` 或 `-h` | 显示帮助 |

查看帮助：

```bat
serve.exe --help
```

### 8.4 运行结果

每局结束后，控制台会输出：

- 本局结束回合数。
- 结束原因。只剩一支队伍，或者到达最大回合数。
- 玩家排名。
- 队伍排名。

每场比赛结束后会输出累计排名表。全部比赛结束后输出最终排名表。

排名也会写进 `logs/score.log`。

### 8.5 程序退出码

| 退出码 | 含义 |
| --- | --- |
| 0 | 正常结束 |
| 1 | 环境错误或配置错误 |
| 2 | 运行错误 |

## 9. 如何查看日志

默认日志目录是 `logs`。目录不存在时，程序会自动创建。日志目录可以在 `log.directory` 里修改。

| 路径 | 内容 |
| --- | --- |
| `logs/serve.log` | 主日志。记录启动、配置、比赛信息、玩家判负、错误等 |
| `logs/score.log` | 计分日志。每场比赛后追加累计排名表 |
| `logs/moves/` | 玩家操作记录。每局一个文件，人类可直接阅读 |
| `logs/ai/` | 玩家 AI 的标准错误输出。每个玩家每局一个文件 |

文件名规则：

- 操作记录：`logs/moves/match_01_game_01.log`。
- AI 调试输出：`logs/ai/match_01_game_01_example.stderr.txt`。

说明：

- `serve.log` 每行带时间戳和级别，级别有 `INFO`、`WARN`、`ERROR`。
- `moves` 记录每回合的玩家动作、移动兵力、地图变化格子。用中文纯文本，UTF-8 编码。
- `score.log` 只在比赛结束后写入。
- 把 `log.recordMoves` 设为 `false` 可以关闭操作记录。
- 玩家 AI 的调试信息写在 `logs/ai/`。这样不会污染通信协议。

## 10. 如何继续开发

### 10.1 目录总览

| 路径 | 用途 |
| --- | --- |
| `src/` | 全部源代码 |
| `src/include/` | 公共头文件。类型、地图、日志、JSON、工具 |
| `src/core/` | 核心逻辑。配置、玩家、规则、游戏、地图生成、计分、记录、总编排 |
| `src/io/` | 与玩家 AI 的通信。进程管理、管道读写、协议编码 |
| `src/ui/` | 图形界面。使用 EasyX |
| `lib/` | 外部库。EasyX 与 magic_enum |
| `assets/` | 游戏资源。当前为空，留给以后使用 |
| `battle/` | 玩家 AI 目录 |
| `config/` | 配置文件与预设 |
| `doc/` | 开发文档 |
| `logs/` | 运行日志，由程序生成 |
| `tests/` | 测试脚本与测试用 AI |
| `LLM_notice/` | 开发过程记录与测试报告 |
| `build/` | 临时构建与测试输出，已被 git 忽略 |

### 10.2 各模块入口

| 模块 | 入口文件 | 作用 |
| --- | --- | --- |
| 程序入口 | `src/main.cpp` | 解析参数、读配置、初始化日志、启动总编排 |
| 总编排 | `src/core/runner.cpp` | 发现玩家、组织比赛、跑局、统计排名 |
| 游戏主体 | `src/core/game.cpp` | 维护一局状态、推进回合、计算视野与排名 |
| 规则 | `src/core/rule.cpp` | 校验移动、执行移动、生成兵力、淘汰玩家 |
| 地图生成 | `src/core/map_generator.cpp` | 生成地形、放置首都、保证连通 |
| 配置 | `src/core/config.cpp` | 读取与校验 JSON 配置 |
| 计分 | `src/core/score.cpp` | 累计加权总分并排名 |
| 操作记录 | `src/core/recorder.cpp` | 写人类可读的操作记录 |
| 通信 | `src/io/io.cpp` | 启动 AI 进程、管道读写 |
| 协议 | `src/io/protocol.cpp` | 把游戏状态编码为字节消息 |
| 界面 | `src/ui/ui.cpp` | EasyX 窗口绘制与按键处理 |

调用关系：

```text
main
  -> Runner
       -> Game
            -> Rule
            -> MapGenerator
       -> AIChannel
            -> protocol
       -> MoveRecorder
       -> ScoreBoard
       -> Renderer
```

### 10.3 源代码文件一览

`src` 下共有 32 个源文件。

| 文件 | 作用 |
| --- | --- |
| `src/main.cpp` | 程序入口。解析参数、读配置、初始化日志、启动总编排 |
| `src/include/type.hpp` | 地形、可见地形、坐标、排名等通用类型 |
| `src/include/map.hpp` | 二维格子容器 `Tile` 与 `GameMap` |
| `src/include/direction.hpp` | 四方向常量与方向编号 |
| `src/include/util.hpp` | 坐标、距离、数值裁剪工具 |
| `src/include/random.hpp` | 全局随机数与种子控制 |
| `src/include/logger.hpp` | 线程安全日志类与全局日志对象 |
| `src/include/json.hpp` | 单头文件 JSON 解析器 |
| `src/include/unique_handle.hpp` | Windows 句柄的 RAII 包装 |
| `src/include/move_operation.hpp` | 一次移动操作的数据结构 |
| `src/core/config.hpp` | 配置结构与 `Config` 类 |
| `src/core/config.cpp` | 读取、补默认值、校验 JSON 配置 |
| `src/core/player.hpp` | 玩家、队伍、玩家注册表 |
| `src/core/player.cpp` | 注册、统计、移动优先级、淘汰 |
| `src/core/rule.hpp` | 规则接口与 `MoveEffect` |
| `src/core/rule.cpp` | 移动校验、执行、兵力增减、淘汰 |
| `src/core/game.hpp` | 一局游戏的状态与接口 |
| `src/core/game.cpp` | 回合推进、视野、排名 |
| `src/core/map_generator.hpp` | 地图生成参数与生成器 |
| `src/core/map_generator.cpp` | 地形生成、首都放置、连通性 |
| `src/core/score.hpp` | 计分板与成绩条目 |
| `src/core/score.cpp` | 加权累计总分与排名 |
| `src/core/recorder.hpp` | 操作记录器与动作结构 |
| `src/core/recorder.cpp` | 写人类可读的操作记录 |
| `src/core/runner.hpp` | 运行选项与总编排接口 |
| `src/core/runner.cpp` | 发现玩家、组织比赛、跑局、判负、汇总 |
| `src/io/io.hpp` | 通信通道接口、动作状态 |
| `src/io/io.cpp` | 管道读写、进程启动、超时处理 |
| `src/io/protocol.hpp` | 消息类型与编码函数声明 |
| `src/io/protocol.cpp` | 小端序协议编码实现 |
| `src/ui/ui.hpp` | 画面数据结构与 `Renderer` 接口 |
| `src/ui/ui.cpp` | EasyX 窗口、绘制、按键处理 |

测试用 AI 在 `tests/ai/` 下，共四个 AI 源码和一个共用头文件：

| 文件 | 作用 |
| --- | --- |
| `tests/ai/timeout_ai.cpp` | 故意超时的 AI |
| `tests/ai/crash_ai.cpp` | 收到回合消息后退出并返回非零退出码的 AI |
| `tests/ai/invalid_ai.cpp` | 输出非协议文本的 AI |
| `tests/ai/illegal_move_ai.cpp` | 输出格式合法但棋盘上非法的移动的 AI |
| `tests/ai/protocol_util.hpp` | 测试 AI 共用的读帧、写行工具 |

### 10.4 开发文档

完整开发文档见 [doc/文档.md](doc/文档.md)。里面有：

- 目录与文件逐个说明。
- 每个头文件的公开函数、参数、返回值。
- 每个类的公开成员函数。
- 配置结构。
- 通信协议位置。
- 构建参数说明。
- 测试方案。

### 10.5 代码规范摘要

- 注释用中文。少用标点，多用空格。
- 变量名和函数名用小驼峰，第一个单词小写。
- 类名用大驼峰，全部单词首字母大写。
- 命名空间按头文件实际情况使用。基础类型与工具在 `gen`，核心游戏类型与逻辑在 `gen::core`，通信在 `gen::io`，界面在 `gen::ui`。
- 头文件后缀用 `.hpp`。
- 公开函数必须在头文件里写注释，说明作用、参数、返回值。
- 公开变量必须写注释，说明类型和用途。
- 逻辑简单的成员函数可以内联，写在 `.hpp` 里。
- 不使用需要运行时动态库的外部库。全部静态编译。
- 新增外部库时，头文件放 `lib/库名/`，静态库放 `lib/库名/lib/`。
- 调试输出不能写进 AI 协议的标准输出。
- 修改代码后先构建，再跑测试。

## 11. 如何运行测试

测试脚本是 `tests/run_tests.ps1`。它用无界面模式跑全部验收测试。

前置条件：

- 已经按第 3 节配置好 MinGW。
- 系统自带的 Windows PowerShell 5.1 即可，不需要安装 PowerShell 7。
- 在仓库根目录运行。

运行命令：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

如果已经安装了 PowerShell 7，也可以用：

```pwsh
pwsh -File tests\run_tests.ps1
```

如果修改过测试脚本，注意保存为 UTF-8 BOM 编码。Windows PowerShell 5.1 读取没有 BOM 的 UTF-8 脚本时可能乱码。

测试脚本会做什么：

- 从干净状态调用 `build.bat --no-pause` 构建。
- 用 `--no-ui` 模式跑各种配置。
- 检查退出码、日志内容和输出文件。
- 汇总通过和失败项。

结果位置：

| 路径 | 内容 |
| --- | --- |
| `build/test_output/` | 每个测试项的单独输出和日志 |
| `build/test_output/report.md` | 测试汇总报告。首次运行或每次运行后生成 |
| `LLM_notice/测试报告.md` | 验收结束后的归档位置。人工把 report.md 的内容复制过去 |

测试脚本还支持三个可选参数：

| 参数 | 作用 |
| --- | --- |
| `-SkipBuild` | 跳过干净构建，直接使用已有的 `serve.exe` |
| `-SkipUi` | 跳过界面冒烟测试 |
| `-KeepLogs` | 保留上一次的测试输出 |

测试项包括构建、示例配置、1v1、2v2、FFA、自动匹配、禁用地形、最大回合数、AI 超时、AI 崩溃、AI 非法输出、AI 非法移动、日志目录缺失、配置缺失、玩家可执行文件缺失、自动匹配人数不足、界面冒烟测试。当前脚本共 47 项断言，最近一次验收全部通过。

详细说明见 [测试说明](doc/测试说明.md)。

如果当前仓库还没有 `tests/run_tests.ps1`，说明测试脚本尚未就绪。以仓库实际文件为准。

## 12. 常见问题

### 12.1 找不到 g++

现象：`g++ --version` 提示不是内部或外部命令，或者 `build.bat` 报 `g++ compiler not found`。

处理：

1. 检查 MinGW 的 `bin` 目录是否加入 PATH。
2. 检查路径是否写错，例如 `D:\mingw64\bin`。
3. 关闭全部旧命令行窗口，重新打开。
4. 运行 `where g++`，确认能找到 g++。
5. 确认电脑上装的是 64 位版本。

### 12.2 示例 AI 未构建

现象：程序提示 `battle 目录下没有可用玩家`，或者提示某个玩家不可用。

处理：

1. 运行根目录 `build.bat`。
2. 检查 `battle/example/example.exe` 是否生成。
3. 检查多人模式用到的玩家目录是否存在，例如 `demo_expand`、`demo_random`、`demo_rush`。
4. 每个玩家目录里必须有 `<id>.exe`，并且名字和文件夹名一致。

### 12.3 配置错误

现象：程序打印 `配置错误 ...`，然后退出。

处理：

1. 看错误信息里的配置键路径，例如 `game.maxTurns`。
2. 对照 `config/example.json` 检查键名、类型、取值。
3. 检查 JSON 语法，注意逗号、引号、括号。
4. 检查文件编码。建议保存为 UTF-8。
5. 用 `config/example.json` 先跑一遍，确认能运行。

### 12.4 日志目录权限

现象：程序提示 `无法初始化日志` 或 `无法创建日志目录`。

处理：

1. 检查 `log.directory` 指向的路径是否可写。
2. 不要把日志目录指向系统目录，例如 `C:\Windows`。
3. 可以改成相对路径，例如 `logs`。
4. 检查是否有杀毒软件拦截了文件夹创建。

### 12.5 比赛太长

现象：一局跑很久，尤其是 1500 回合加界面播放时。

处理：

1. 用 `--no-ui` 关闭界面。
2. 改小 `game.maxTurns`，例如 100。
3. 改小 `ui.turnDelayMs`，例如 0。
4. 用 `--seed` 固定种子，方便复现。
5. 只跑一个模式，不要同时开自动匹配。

### 12.6 中文显示乱码

现象：命令行里中文变成乱码。

处理：

1. 使用 Windows Terminal 或新版命令提示符。
2. 运行 `chcp 65001` 切换到 UTF-8 代码页。
3. `build.bat` 的提示特意使用英文，避免乱码。
4. 日志文件是 UTF-8，用 VS Code 打开。

### 12.7 AI 总是超时

现象：日志里出现 `超时`，AI 被判违规。

处理：

1. 检查 AI 每回合计算量。默认时限是 50 毫秒。
2. 用 `-O2` 编译 AI。
3. 在 `game.turnTimeLimitMs` 里适当放宽时限。
4. 检查 AI 是否在每回合都刷新了输出缓冲。

### 12.8 双击窗口一闪而过

现象：双击 `serve.exe` 后窗口立刻关闭，看不到错误。

处理：

1. 打开命令行，手动运行 `serve.exe --config config\example.json`。
2. 出错时程序会打印中文原因。
3. 检查配置文件是否存在。
4. 检查日志目录是否可写。

## 13. 文档要求

本仓库的文档使用 Obsidian 支持的 Markdown 语法。

公式写法：

- 行内公式用 `$` 包裹。例如玩家分数为 $N - rank + 1$。
- 行间公式用 `$$` 包裹。例如：

$$
score = w_{player} \times (N - rank_{player} + 1)
      + w_{team} \times (T - rank_{team} + 1)
$$

其中 $N$ 为玩家总数，$T$ 为队伍总数，$w_{player}$ 与 $w_{team}$ 为配置里的权重。

写作要求：

- 语言通俗。
- 多用短句。
- 不用长难句。
- 正文不用破折号。
- 多用逗号、顿号、句号分句。
- 表格保持简洁。
