@echo off
rem ============================================================================
rem  gui_selftest.bat - run the graphical program in self-test mode
rem
rem  It drives the whole UI programmatically: selects symptoms, submits a
rem  booking, cancels it, queries a route, checks in / calls / passes / finishes
rem  a waiting-queue patient, and visits the floor plan and the data overview.
rem  Every step goes through the SAME apply() function a real mouse click uses,
rem  so this is not a separate "test-only" code path.
rem
rem  Why not simulate mouse clicks instead: synthesized clicks would have to be
rem  mapped through DPI scaling, the window border and ImGui's own coordinate
rem  space. Driving ImGui's state directly is far more reliable and repeatable.
rem
rem  Usage (from anywhere):  tools\gui_selftest.bat  [data-directory]
rem  NOTE: comments are ASCII-only on purpose (cmd reads .bat with the system
rem  code page; UTF-8 Chinese comments get garbled and partially executed).
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if not exist "%ROOT%\build\hospital_gui.exe" (
    echo [info] hospital_gui.exe not found, building first...
    call "%TOOLDIR%\build.bat" gui < nul
    if errorlevel 1 exit /b 1
)

if not exist "%ROOT%\data\symptoms.txt" (
    echo [info] test data not found, generating first...
    call "%TOOLDIR%\build.bat" gen < nul
    if errorlevel 1 exit /b 1
    pushd "%ROOT%"
    build\gen_data.exe data
    popd
)

set "DATADIR=%1"
if "%DATADIR%"=="" set "DATADIR=data"

pushd "%ROOT%"
echo.
echo ==== GUI self-test: driving all seven functions ====
echo ==== close the window when the log says the test is finished ====
echo.
build\hospital_gui.exe --selftest %DATADIR%
set "RC=%errorlevel%"
popd
exit /b %RC%
