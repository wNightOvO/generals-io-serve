@echo off
chcp 65001 >nul
rem generals.io demo_expand AI build script
rem double click to build demo_expand/demo_expand.exe
rem run build.bat --no-pause to skip the pause at the end
rem the chinese hint text lives in demo_expand.cpp marked with //@HINT
setlocal
set "here=%~dp0"
set "target=demo_expand"
set "exePath=%here%%target%.exe"
set "code=0"

rem step 1 check the g++ compiler
where g++ >nul 2>nul
if errorlevel 1 goto noCompiler

call :hint COMPILING
pushd "%here%"
g++ -std=gnu++20 -O2 -static -finput-charset=UTF-8 -fexec-charset=UTF-8 -o %target%.exe %target%.cpp
set "buildCode=%errorlevel%"
popd
if not "%buildCode%"=="0" goto buildFailed
if not exist "%exePath%" goto noOutput

call :hint SUCCESS
echo %exePath%
goto finish

:noCompiler
call :hint NOGXX
call :hint NOGXXTIP
set "code=1"
goto finish

:buildFailed
call :hint FAILED
set "code=1"
goto finish

:noOutput
call :hint NOOUTPUT
set "code=1"
goto finish

rem read one chinese hint line from the cpp source
rem hint name is the first argument such as COMPILING or FAILED
:hint
for /f "tokens=3* delims=:" %%A in ('findstr /b /c:"//@HINT:%~1:" "%~dp0%target%.cpp" 2^>nul') do echo %%A
goto :eof

:finish
if /i not "%~1"=="--no-pause" pause
exit /b %code%
