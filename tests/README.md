# 验收测试说明

本目录保存项目的自动化验收测试。

## 运行方式

需要 Windows PowerShell 5.1 或更高版本，并且 `g++` 在 PATH 中。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

如果安装了 PowerShell 7，也可以这样运行。

```powershell
pwsh -File tests\run_tests.ps1
```

可选参数

- `-SkipBuild` 跳过干净状态构建测试，直接使用已有的 `serve.exe`。
- `-SkipUi` 跳过界面冒烟测试。
- `-KeepLogs` 保留上一次测试输出。

## 输出位置

- `build/test_output/report.md` 测试报告。
- `build/test_output/*.stdout.txt` 与 `*.stderr.txt` 各个测试的标准输出与标准错误。
- `build/test_output/logs_<测试名>/` 生成配置对应的日志目录。
- `build/test_output/configs/` 本次测试生成的配置。

验收完成后把 `build/test_output/report.md` 的内容归档到 `LLM_notice/测试报告.md`。

## 测试内容

1. 干净状态构建。删除 `serve.exe` 与示例玩家可执行文件后运行 `build.bat`。
2. 构建产物检查。检查示例玩家可执行文件与静态链接情况。
3. `config/example.json` 运行。检查最终排名、对局结束、操作记录、计分日志。
4. 1v1、2v2、FFA 三种模式。
5. 自动匹配。4 名玩家 1v1 应有 6 场比赛。
6. 禁用地形。禁用山区、沼泽、城市后地形统计应为 0。
7. 最大回合数。设置 5 回合后应在第 5 回合结束。
8. AI 超时、AI 崩溃、AI 非法输出、AI 非法移动。都应以判负处理并写入日志。
9. 配置缺失、日志目录自动创建、玩家可执行文件缺失、自动匹配人数不足。
10. 界面冒烟测试。启动界面模式 5 秒，进程应保持运行。

## 测试用 AI

`tests/ai/` 下保存四个异常行为 AI 的源码。测试脚本会把它们编译到 `battle/test_timeout`、`battle/test_crash`、`battle/test_invalid`、`battle/test_illegal_move`，测试结束后自动删除。

## 新增测试

在 `tests/run_tests.ps1` 中增加一段场景，使用 `New-TestConfig` 生成配置，使用 `Invoke-Serve` 运行，使用 `Check` 记录结果即可。测试结束会自动生成报告。
