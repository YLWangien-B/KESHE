@echo off
rem ============================================================================
rem  selfcheck.bat - build and run the self-check program
rem
rem  usage:
rem    tools\selfcheck.bat              run all 10 groups
rem    tools\selfcheck.bat 3            run group 3 only
rem    tools\selfcheck.bat 1 2 6        run groups 1, 2 and 6
rem    tools\selfcheck.bat nodata       use another data directory for groups 8~10
rem
rem  Exit code 0 = everything passed, 1 = something failed.
rem  Must be started from the project root (the program loads data\ relative to
rem  the current directory), so this script switches there first.
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

rem groups 8~10 need the data files
if not exist "%ROOT%\data\symptoms.txt" (
    echo [info] test data not found, generating first...
    call "%TOOLDIR%\build.bat" gen < nul
    if errorlevel 1 exit /b 1
    pushd "%ROOT%"
    build\gen_data.exe data
    popd
)

pushd "%ROOT%"
build\selfcheck.exe %*
set "RC=%errorlevel%"
popd
exit /b %RC%
