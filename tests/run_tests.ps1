# generals.io serve 验收测试脚本
# 用法 powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1
# 可选参数 -SkipBuild 跳过干净构建测试  -SkipUi 跳过界面冒烟测试  -KeepLogs 保留上次输出

[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$SkipUi,
    [switch]$KeepLogs
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$ServePath = Join-Path $RepoRoot 'serve.exe'
$OutputDir = Join-Path $RepoRoot 'build\test_output'
$ConfigDir = Join-Path $OutputDir 'configs'
$script:Results = [System.Collections.Generic.List[object]]::new()
$script:TempBattleDirs = @()

# 打印并记录一条测试结果
function Check {
    param([string]$Name, [bool]$Condition, [string]$Detail = '')
    $state = if($Condition) { 'PASS' } else { 'FAIL' }
    $color = if($Condition) { 'Green' } else { 'Red' }
    Write-Host ("[{0}] {1} {2}" -f $state, $Name, $Detail) -ForegroundColor $color
    $script:Results.Add([pscustomobject]@{ Name = $Name; State = $state; Detail = $Detail })
}

# 生成测试配置 以 config/example.json 为模板
function New-TestConfig {
    param([string]$Name, [scriptblock]$Modify)
    $config = Get-Content (Join-Path $RepoRoot 'config\example.json') -Raw | ConvertFrom-Json
    $config.ui.enabled = $false
    $config.log.directory = "build/test_output/logs_$Name"
    if($Modify) { & $Modify $config }
    if(-not (Test-Path $ConfigDir)) {
        New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
    }
    $path = Join-Path $ConfigDir "$Name.json"
    $json = $config | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($path, $json, (New-Object System.Text.UTF8Encoding($false)))
    return $path
}

# 运行 serve.exe 无界面模式 返回退出码与输出文件路径
function Invoke-Serve {
    param([string]$Name, [string]$ConfigPath, [string[]]$ExtraArgs = @())
    $stdout = Join-Path $OutputDir "$Name.stdout.txt"
    $stderr = Join-Path $OutputDir "$Name.stderr.txt"
    $commandLine = 'serve.exe --config "' + $ConfigPath + '" --no-ui --no-pause'
    if($ExtraArgs.Count -gt 0) {
        $commandLine += ' ' + ($ExtraArgs -join ' ')
    }
    $commandLine += ' > "' + $stdout + '" 2> "' + $stderr + '"'
    & cmd.exe /c $commandLine | Out-Null
    return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Stdout = $stdout; Stderr = $stderr }
}

# 读取测试配置对应的日志内容
function Get-TestLog {
    param([string]$Name)
    $path = Join-Path $OutputDir "logs_$Name\serve.log"
    if(Test-Path $path) { return (Get-Content $path -Raw -Encoding utf8) }
    return ''
}

# 编译一个测试用异常 AI 到 battle 目录
function Install-TestPlayer {
    param([string]$Id, [string]$SourceRelative)
    $directory = Join-Path $RepoRoot "battle\$Id"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $exe = Join-Path $directory "$Id.exe"
    $source = Join-Path $RepoRoot $SourceRelative
    & g++ -std=gnu++20 -O2 -static -o $exe $source
    if($LASTEXITCODE -ne 0) { throw "编译测试玩家失败 $Id" }
    $script:TempBattleDirs += $directory
}

# 删除测试用异常 AI 目录 避免影响后续测试的玩家枚举
function Remove-TestPlayers {
    foreach($id in @('test_timeout', 'test_crash', 'test_invalid', 'test_illegal_move')) {
        Remove-Item (Join-Path $RepoRoot "battle\$id") -Recurse -Force -ErrorAction SilentlyContinue
    }
    $script:TempBattleDirs = @()
}

# 输出测试报告
function Write-Report {
    $passed = @($script:Results | Where-Object { $_.State -eq 'PASS' }).Count
    $failed = @($script:Results | Where-Object { $_.State -eq 'FAIL' }).Count
    $lines = @()
    $lines += '# 验收测试报告'
    $lines += ''
    $lines += '运行时间 ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    $lines += ''
    $lines += '| 测试项 | 结果 | 说明 |'
    $lines += '| --- | --- | --- |'
    foreach($item in $script:Results) {
        $lines += '| ' + $item.Name + ' | ' + $item.State + ' | ' + $item.Detail + ' |'
    }
    $lines += ''
    $lines += "通过 $passed 项 失败 $failed 项"
    $reportPath = Join-Path $OutputDir 'report.md'
    $lines | Set-Content -Path $reportPath -Encoding utf8
    Write-Host ''
    Write-Host "测试结束 通过 $passed 项 失败 $failed 项" -ForegroundColor $(if($failed -eq 0) { 'Green' } else { 'Red' })
    Write-Host "报告文件 $reportPath"
    return $failed
}

try {
    # 准备输出目录
    if((Test-Path $OutputDir) -and (-not $KeepLogs)) {
        Remove-Item $OutputDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null

    # 清理可能残留的测试玩家
    foreach($id in @('test_timeout', 'test_crash', 'test_invalid', 'test_illegal_move')) {
        Remove-Item (Join-Path $RepoRoot "battle\$id") -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host '== 检查运行环境 =='
    $compiler = Get-Command g++ -ErrorAction SilentlyContinue
    Check '编译器可用' ($null -ne $compiler) '需要 g++ 在 PATH 中'
    if($null -eq $compiler) { throw '找不到 g++ 无法继续' }

    # 一 干净状态构建
    if(-not $SkipBuild) {
        Write-Host '== 干净状态构建 =='
        Remove-Item $ServePath -Force -ErrorAction SilentlyContinue
        foreach($id in @('example', 'demo_expand', 'demo_random', 'demo_rush')) {
            Remove-Item (Join-Path $RepoRoot "battle\$id\$id.exe") -Force -ErrorAction SilentlyContinue
        }
        $buildStdout = Join-Path $OutputDir 'build.stdout.txt'
        $buildStderr = Join-Path $OutputDir 'build.stderr.txt'
        $buildCommand = 'build.bat --no-pause > "' + $buildStdout + '" 2> "' + $buildStderr + '"'
        & cmd.exe /c $buildCommand | Out-Null
        $buildExit = $LASTEXITCODE
        $built = Test-Path $ServePath
        Check '干净状态构建' ($buildExit -eq 0 -and $built) ("build.bat exit=" + $buildExit + " serve.exe=" + $built)
    }
    if(-not (Test-Path $ServePath)) {
        throw 'serve.exe 不存在 无法继续测试'
    }

    Write-Host '== 检查构建产物 =='
    foreach($id in @('example', 'demo_expand', 'demo_random', 'demo_rush')) {
        $exe = Join-Path $RepoRoot "battle\$id\$id.exe"
        Check "示例玩家 $id 可执行文件存在" (Test-Path $exe) $exe
    }
    $objdump = Get-Command objdump -ErrorAction SilentlyContinue
    if($null -ne $objdump) {
        $imports = & $objdump.Source -p $ServePath 2>$null | Select-String 'DLL Name'
        $badImports = @($imports | Where-Object { $_.Line -match 'libstdc\+\+|libgcc|libwinpthread' })
        Check '静态链接检查' ($badImports.Count -eq 0) ($badImports -join ' ')
    }
    else {
        Check '静态链接检查' $true '缺少 objdump 跳过'
    }

    # 二 运行 config/example.json 原始配置
    Write-Host '== 运行 config/example.json =='
    $exampleResult = Invoke-Serve 'example' (Join-Path $RepoRoot 'config\example.json')
    Check '示例配置运行' ($exampleResult.ExitCode -eq 0) ("exit=" + $exampleResult.ExitCode)
    $exampleLogPath = Join-Path $RepoRoot 'logs\serve.log'
    if(Test-Path $exampleLogPath) {
        Copy-Item $exampleLogPath (Join-Path $OutputDir 'example.serve.log') -Force
        $exampleLog = Get-Content $exampleLogPath -Raw -Encoding utf8
        Check '示例配置日志完整' ($exampleLog -match '全部比赛最终排名') '应包含最终排名'
        Check '示例配置完成对局' ($exampleLog -match '局 结束') '应包含对局结束记录'
    }
    else {
        Check '示例配置日志完整' $false '找不到 logs\serve.log'
    }
    $exampleMoves = Join-Path $RepoRoot 'logs\moves\match_01_game_01.log'
    Check '玩家操作记录生成' (Test-Path $exampleMoves) $exampleMoves
    if(Test-Path $exampleMoves) {
        $movesText = Get-Content $exampleMoves -Raw -Encoding utf8
        Check '玩家操作记录内容' ($movesText -match '第 1 回合' -and $movesText -match '地图变化') '应包含回合与地图变化'
        Copy-Item $exampleMoves (Join-Path $OutputDir 'example.moves.log') -Force
    }
    $scorePath = Join-Path $RepoRoot 'logs\score.log'
    Check '计分日志生成' (Test-Path $scorePath) $scorePath

    # 三 各游戏模式
    Write-Host '== 1v1 模式 =='
    $duelResult = Invoke-Serve 'mode_1v1' (Join-Path $RepoRoot 'config\1v1.json')
    Check '1v1 模式运行' ($duelResult.ExitCode -eq 0) ("exit=" + $duelResult.ExitCode)
    $duelLog = Get-Content (Join-Path $RepoRoot 'logs\serve.log') -Raw -Encoding utf8 -ErrorAction SilentlyContinue
    Check '1v1 模式日志' ($duelLog -match '模式 1v1') '应包含模式名'

    Write-Host '== 2v2 模式 =='
    $teamResult = Invoke-Serve 'mode_2v2' (Join-Path $RepoRoot 'config\2v2.json')
    Check '2v2 模式运行' ($teamResult.ExitCode -eq 0) ("exit=" + $teamResult.ExitCode)
    $teamLog = Get-Content (Join-Path $RepoRoot 'logs\serve.log') -Raw -Encoding utf8 -ErrorAction SilentlyContinue
    Check '2v2 模式日志' ($teamLog -match '模式 2v2') '应包含模式名'
    Check '2v2 队伍排名' ($teamLog -match '队伍排名 1 队伍') '应包含队伍排名'

    Write-Host '== FFA 模式 =='
    $ffaResult = Invoke-Serve 'mode_ffa' (Join-Path $RepoRoot 'config\ffa.json')
    Check 'FFA 模式运行' ($ffaResult.ExitCode -eq 0) ("exit=" + $ffaResult.ExitCode)
    $ffaLog = Get-Content (Join-Path $RepoRoot 'logs\serve.log') -Raw -Encoding utf8 -ErrorAction SilentlyContinue
    Check 'FFA 模式日志' ($ffaLog -match '模式 ffa') '应包含模式名'
    Check 'FFA 使用配置的玩家数' ($ffaLog -match '参赛玩家') '应包含参赛玩家列表'

    Write-Host '== 自动匹配 =='
    $autoResult = Invoke-Serve 'auto' (Join-Path $RepoRoot 'config\auto.json')
    Check '自动匹配运行' ($autoResult.ExitCode -eq 0) ("exit=" + $autoResult.ExitCode)
    $autoLog = Get-Content (Join-Path $RepoRoot 'logs\serve.log') -Raw -Encoding utf8 -ErrorAction SilentlyContinue
    Check '自动匹配枚举玩家' ($autoLog -match '发现 4 名玩家') '应发现 4 名玩家'
    Check '自动匹配生成组合' ($autoLog -match '共 6 场比赛') '4 名玩家 1v1 应有 6 场比赛'

    # 四 禁用地形
    Write-Host '== 禁用地形 =='
    $bannedConfig = New-TestConfig 'banned' {
        param($config)
        $config.map.bannedTerrains = @('mountain', 'swamp', 'city')
        $config.game.maxTurns = 3
    }
    $bannedResult = Invoke-Serve 'banned' $bannedConfig
    $bannedLog = Get-TestLog 'banned'
    Check '禁用地形运行' ($bannedResult.ExitCode -eq 0) ("exit=" + $bannedResult.ExitCode)
    Check '禁用山区生效' ($bannedLog -match '山区 0' -and $bannedLog -notmatch '山区 [1-9]') '山区数量应为 0'
    Check '禁用沼泽生效' ($bannedLog -match '沼泽 0' -and $bannedLog -notmatch '沼泽 [1-9]') '沼泽数量应为 0'
    Check '禁用城市生效' ($bannedLog -match '城市 0' -and $bannedLog -notmatch '城市 [1-9]') '中立城市数量应为 0'

    # 五 最大回合数
    Write-Host '== 最大回合数 =='
    $maxTurnConfig = New-TestConfig 'maxturns' {
        param($config)
        $config.game.maxTurns = 5
    }
    $maxTurnResult = Invoke-Serve 'maxturns' $maxTurnConfig
    $maxTurnLog = Get-TestLog 'maxturns'
    Check '最大回合数运行' ($maxTurnResult.ExitCode -eq 0) ("exit=" + $maxTurnResult.ExitCode)
    Check '最大回合数生效' ($maxTurnLog -match '到达最大回合数' -and $maxTurnLog -match '回合 5') '应在第 5 回合结束'

    # 六 异常 AI 测试
    Write-Host '== 安装异常测试 AI =='
    Install-TestPlayer -Id 'test_timeout' -SourceRelative 'tests\ai\timeout_ai.cpp'
    Install-TestPlayer -Id 'test_crash' -SourceRelative 'tests\ai\crash_ai.cpp'
    Install-TestPlayer -Id 'test_invalid' -SourceRelative 'tests\ai\invalid_ai.cpp'
    Install-TestPlayer -Id 'test_illegal_move' -SourceRelative 'tests\ai\illegal_move_ai.cpp'

    Write-Host '== AI 超时 =='
    $timeoutConfig = New-TestConfig 'timeout' {
        param($config)
        $teams = @()
        $teams += ,@('example')
        $teams += ,@('test_timeout')
        $config.match.teams = $teams
        $config.game.maxTurns = 3
        $config.game.turnTimeLimitMs = 50
        $config.game.maxViolations = 1
    }
    $timeoutResult = Invoke-Serve 'timeout' $timeoutConfig
    $timeoutLog = Get-TestLog 'timeout'
    Check 'AI 超时运行' ($timeoutResult.ExitCode -eq 0) ("exit=" + $timeoutResult.ExitCode)
    Check 'AI 超时被处理' ($timeoutLog -match '超时' -and $timeoutLog -match '判负') '日志应包含超时与判负'

    Write-Host '== AI 崩溃 =='
    $crashConfig = New-TestConfig 'crash' {
        param($config)
        $teams = @()
        $teams += ,@('example')
        $teams += ,@('test_crash')
        $config.match.teams = $teams
        $config.game.maxTurns = 3
        $config.game.maxViolations = 1
    }
    $crashResult = Invoke-Serve 'crash' $crashConfig
    $crashLog = Get-TestLog 'crash'
    Check 'AI 崩溃运行' ($crashResult.ExitCode -eq 0) ("exit=" + $crashResult.ExitCode)
    Check 'AI 崩溃被处理' ($crashLog -match '程序输出结束|程序崩溃' -and $crashLog -match '判负') '日志应包含崩溃与判负'

    Write-Host '== AI 非法输出 =='
    $invalidConfig = New-TestConfig 'invalid' {
        param($config)
        $teams = @()
        $teams += ,@('example')
        $teams += ,@('test_invalid')
        $config.match.teams = $teams
        $config.game.maxTurns = 3
        $config.game.maxViolations = 1
    }
    $invalidResult = Invoke-Serve 'invalid' $invalidConfig
    $invalidLog = Get-TestLog 'invalid'
    Check 'AI 非法输出运行' ($invalidResult.ExitCode -eq 0) ("exit=" + $invalidResult.ExitCode)
    Check 'AI 非法输出被处理' ($invalidLog -match '非法输出' -and $invalidLog -match '判负') '日志应包含非法输出与判负'

    Write-Host '== AI 非法移动 =='
    $illegalMoveConfig = New-TestConfig 'illegal_move' {
        param($config)
        $teams = @()
        $teams += ,@('example')
        $teams += ,@('test_illegal_move')
        $config.match.teams = $teams
        $config.game.maxTurns = 3
        $config.game.maxViolations = 1
    }
    $illegalMoveResult = Invoke-Serve 'illegal_move' $illegalMoveConfig
    $illegalMoveLog = Get-TestLog 'illegal_move'
    Check 'AI 非法移动运行' ($illegalMoveResult.ExitCode -eq 0) ("exit=" + $illegalMoveResult.ExitCode)
    Check 'AI 非法移动被处理' ($illegalMoveLog -match '非法移动' -and $illegalMoveLog -match '判负') '日志应包含非法移动与判负'

    # 删除测试玩家 保证后续玩家枚举只包含示例玩家
    Remove-TestPlayers

    # 七 错误处理
    Write-Host '== 配置缺失 =='
    $missingConfigResult = Invoke-Serve 'missing_config' (Join-Path $ConfigDir 'no_such_config.json')
    Check '配置缺失退出码' ($missingConfigResult.ExitCode -eq 1) ("exit=" + $missingConfigResult.ExitCode)
    $missingConfigStderr = Get-Content $missingConfigResult.Stderr -Raw -Encoding utf8 -ErrorAction SilentlyContinue
    Check '配置缺失提示' ($missingConfigStderr -match '配置错误') '标准错误应包含配置错误'

    Write-Host '== 日志目录自动创建 =='
    $logDirPath = Join-Path $RepoRoot 'build\test_output\logs_logdir\deep\nested'
    Remove-Item $logDirPath -Recurse -Force -ErrorAction SilentlyContinue
    $logDirConfig = New-TestConfig 'logdir' {
        param($config)
        $config.log.directory = 'build/test_output/logs_logdir/deep/nested'
    }
    $logDirResult = Invoke-Serve 'logdir' $logDirConfig
    Check '日志目录自动创建' ($logDirResult.ExitCode -eq 0 -and (Test-Path (Join-Path $logDirPath 'serve.log'))) $logDirPath

    Write-Host '== 玩家可执行文件缺失 =='
    $missingPlayerConfig = New-TestConfig 'missing_player' {
        param($config)
        $teams = @()
        $teams += ,@('example')
        $teams += ,@('missing_player_xyz')
        $config.match.teams = $teams
    }
    $missingPlayerResult = Invoke-Serve 'missing_player' $missingPlayerConfig
    Check '玩家缺失退出码' ($missingPlayerResult.ExitCode -eq 1) ("exit=" + $missingPlayerResult.ExitCode)
    $missingPlayerLog = Get-TestLog 'missing_player'
    Check '玩家缺失提示' ($missingPlayerLog -match '不可用|缺少可执行文件') '日志应说明玩家不可用'

    Write-Host '== 自动匹配人数不足 =='
    $insufficientConfig = New-TestConfig 'insufficient' {
        param($config)
        $config.match.autoMatch = $true
        $config.match.teams = @()
        $config.game.mode = 'ffa'
        $config.game.ffaPlayerCount = 6
    }
    $insufficientResult = Invoke-Serve 'insufficient' $insufficientConfig
    Check '人数不足退出码' ($insufficientResult.ExitCode -eq 1) ("exit=" + $insufficientResult.ExitCode)
    $insufficientLog = Get-TestLog 'insufficient'
    Check '人数不足提示' ($insufficientLog -match '自动匹配需要 6 名玩家') '日志应说明人数不足'

    # 八 界面冒烟测试
    if(-not $SkipUi) {
        Write-Host '== 界面冒烟测试 =='
        if([Environment]::UserInteractive) {
            $uiConfig = New-TestConfig 'ui' {
                param($config)
                $config.ui.enabled = $true
                $config.game.maxTurns = 1500
            }
            $uiOut = Join-Path $OutputDir 'ui.stdout.txt'
            $uiErr = Join-Path $OutputDir 'ui.stderr.txt'
            $uiJob = Start-Job -ScriptBlock {
                param($root, $configPath, $stdoutPath, $stderrPath)
                Set-Location $root
                $line = 'serve.exe --config "' + $configPath + '" --no-pause > "' + $stdoutPath + '" 2> "' + $stderrPath + '"'
                cmd.exe /c $line | Out-Null
            } -ArgumentList $RepoRoot, $uiConfig, $uiOut, $uiErr
            Start-Sleep -Seconds 5
            $uiProcesses = @(Get-Process -Name 'serve' -ErrorAction SilentlyContinue)
            $uiRunning = $uiProcesses.Count -gt 0
            if($uiRunning) {
                $uiProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
                Start-Sleep -Milliseconds 500
            }
            Stop-Job $uiJob -ErrorAction SilentlyContinue
            Remove-Job $uiJob -Force -ErrorAction SilentlyContinue
            $uiLog = Get-TestLog 'ui'
            Check '界面模式启动' $uiRunning '进程在 5 秒内保持运行'
            Check '界面窗口初始化' ($uiLog -match '界面窗口') '日志应记录界面窗口'
        }
        else {
            Check '界面模式启动' $true '当前无交互桌面 跳过'
        }
    }
}
catch {
    Check '测试脚本异常' $false $_.Exception.Message
}
finally {
    foreach($directory in $script:TempBattleDirs) {
        Remove-Item $directory -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$failedCount = Write-Report
if($failedCount -gt 0) { exit 1 }
exit 0
