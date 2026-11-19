@echo off
rem ============================================================================
rem  deploy_qt.bat - copy the Qt runtime DLLs next to the executables
rem
rem  Usage (from anywhere):  tools\deploy_qt.bat
rem
rem  Why this is needed:
rem    patient.exe and admin.exe are linked against Qt6Core/Gui/Widgets. When you
rem    double-click them from Explorer, Windows does NOT have Qt's bin directory
rem    on PATH, so the loader fails with:
rem        "Cannot find Qt6Widgets.dll"  (0xc0000135)
rem
rem    Two ways to fix that:
rem      1) run them through tools\run_qt.bat - it prepends Qt\bin to PATH
rem      2) deploy the DLLs next to the exe with windeployqt  <-- this script
rem
rem    Option 2 makes the exe self-contained, which is what you want for a demo:
rem    you can double-click it, or copy build\ to another machine that has no Qt.
rem
rem  Note: build\ is in .gitignore, so the copied DLLs are never committed.
rem        The repo stays small; this script recreates them after a clean build.
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "QTDIR=D:\Qt\6.8.3\msvc2022_64"
set "WINDEPLOYQT=%QTDIR%\bin\windeployqt.exe"

if not exist "%WINDEPLOYQT%" (
    echo [ERROR] windeployqt not found at %WINDEPLOYQT%
    echo         Edit QTDIR at the top of this script.
    exit /b 1
)
if not exist "%ROOT%\build\patient.exe" (
    echo [ERROR] patient.exe not found. Build first: tools\build_qt.bat
    exit /b 1
)

echo [deploy] patient.exe
"%WINDEPLOYQT%" --release --no-translations --no-opengl-sw "%ROOT%\build\patient.exe" >nul
if errorlevel 1 (
    echo [ERROR] windeployqt failed for patient.exe
    exit /b 1
)

echo [deploy] admin.exe
"%WINDEPLOYQT%" --release --no-translations --no-opengl-sw "%ROOT%\build\admin.exe" >nul
if errorlevel 1 (
    echo [ERROR] windeployqt failed for admin.exe
    exit /b 1
)

echo.
echo [OK] Qt runtime deployed into %ROOT%\build
echo      You can now double-click patient.exe / admin.exe directly.
exit /b 0
