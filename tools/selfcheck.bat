@echo off
rem ============================================================================
rem  selfcheck.bat - build and run the data structure debug program
rem
rem  这个程序只测数据结构本身（顺序表、环形队列、散列表、二维数组、
rem  带权图、带权二部图、插入排序），不涉及任何业务，也不需要数据文件。
rem
rem  usage:
rem    tools\selfcheck.bat                  run all structures
rem    tools\selfcheck.bat SeqList          run one structure
rem    tools\selfcheck.bat SeqList HashMap  run several
rem    tools\selfcheck.bat list             show what can be tested
rem
rem  Exit code 0 = everything passed, 1 = something failed.
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

rem stdin bound to nul for the same reason as run.bat: build.bat calls
rem vcvars64.bat, whose child cmd.exe would otherwise eat the inherited stdin.
if not exist "%ROOT%\build\selfcheck.exe" (
    echo [info] selfcheck.exe not found, building first...
    call "%TOOLDIR%\build.bat" selfcheck < nul
    if errorlevel 1 exit /b 1
)

pushd "%ROOT%"
build\selfcheck.exe %*
set "RC=%errorlevel%"
popd
exit /b %RC%
