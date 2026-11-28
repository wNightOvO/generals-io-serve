@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set NO_PAUSE=0
if /i "%~1"=="--no-pause" set NO_PAUSE=1
echo ============================================
echo   generals.io serve build script
echo ============================================
echo.

where g++ >nul 2>nul
if errorlevel 1 goto :no_compiler

echo [1/3] Check compiler
g++ --version
echo.

echo [2/3] Build serve.exe
if exist serve.exe del /q serve.exe
g++ -std=gnu++20 -finput-charset=UTF-8 -fexec-charset=UTF-8 -static -O2 -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter -Isrc -Ilib -o serve.exe src\main.cpp src\core\*.cpp src\io\*.cpp src\ui\*.cpp -Llib\easyx\lib -leasyx -lgdi32 -lole32 -lwinmm -lgdiplus
if errorlevel 1 goto :build_fail
if not exist serve.exe goto :build_fail
echo [OK] serve.exe built
echo.

echo [3/3] Build sample players
set AI_FAIL=0
for %%d in (example demo_expand demo_random demo_rush) do (
    if exist battle\%%d\build.bat (
        echo Build player %%d
        call battle\%%d\build.bat --no-pause
        if errorlevel 1 (
            echo [WARN] player %%d build failed
            set AI_FAIL=1
        ) else (
            echo [OK] player %%d built
        )
    ) else (
        echo [WARN] battle\%%d\build.bat not found, skip player %%d
        set AI_FAIL=1
    )
)
echo.
echo ============================================
echo  Build finished
echo ============================================
echo Run with UI      serve.exe --config config\example.json
echo Run without UI   serve.exe --config config\example.json --no-ui
echo Logs             logs\serve.log
if "%AI_FAIL%"=="1" echo [TIP] some sample players were not built, multi-player modes may fail
if "%NO_PAUSE%"=="0" pause
exit /b 0

:no_compiler
echo [ERROR] g++ compiler not found.
echo Install MinGW-w64 and add its bin directory to PATH.
echo Download https://github.com/niXman/mingw-builds-binaries/releases
echo Open a NEW terminal after install and run: g++ --version
if "%NO_PAUSE%"=="0" pause
exit /b 1

:build_fail
echo [ERROR] serve.exe build failed. Read the compiler errors above.
if "%NO_PAUSE%"=="0" pause
exit /b 1
