@echo off
rem ============================================================================
rem  build_qt.bat - build the Qt 6 graphical UI: patient.exe and admin.exe
rem
rem  Usage (from anywhere):  tools\build_qt.bat  [clean]
rem
rem  Requirements already on this machine:
rem    Qt 6.8.3 MSVC 2022 64-bit   D:\Qt\6.8.3\msvc2022_64
rem    CMake  + Ninja (bundled with Qt at D:\Qt\Tools\Ninja)
rem    Visual Studio 2022 Build Tools (vcvars64.bat)
rem
rem  Why CMake instead of the raw cl.exe calls used for the other targets:
rem  Qt ships its own CMake config (Qt6Config.cmake) which carries the include
rem  paths, compile definitions, moc rules and the list of Win32 libraries to
rem  link. Re-doing that by hand with cl.exe would be hundreds of lines of
rem  guesswork. CMake is the officially supported way to build a Qt app.
rem
rem  The business layer and data-structure layer are NOT rebuilt here: they are
rem  header-only (src\*.h). Only src\db.cpp is compiled, because it holds the
rem  single instance of Hospital. That is the whole point of the layering.
rem
rem  NOTE: comments are ASCII-only on purpose. cmd.exe reads a .bat with the
rem  system code page (936 on a Simplified Chinese Windows), so a UTF-8 file
rem  with Chinese comments gets garbled and parts of it are executed.
rem ============================================================================
setlocal enabledelayedexpansion

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "QTDIR=D:\Qt\6.8.3\msvc2022_64"
set "NINJA=D:\Qt\Tools\Ninja\ninja.exe"
set "BUILDDIR=%ROOT%\build\qt"

if /I "%~1"=="clean" (
    if exist "%BUILDDIR%" rmdir /s /q "%BUILDDIR%"
    echo [clean] removed %BUILDDIR%
    exit /b 0
)

rem ---- sanity checks: fail loudly with an actionable message ----
if not exist "%QTDIR%\bin\qmake.exe" (
    echo [ERROR] Qt not found at %QTDIR%
    echo         Edit QTDIR at the top of this script.
    exit /b 1
)
if not exist "%NINJA%" (
    echo [ERROR] ninja not found at %NINJA%
    echo         Install it via the Qt Maintenance Tool, or edit NINJA above.
    exit /b 1
)

rem ---- locate vcvars64.bat ----
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%i"
    )
    if defined VSINSTALL if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "D:\visual studio\community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=D:\visual studio\community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not defined VCVARS (
    echo [ERROR] vcvars64.bat not found. Install "Desktop development with C++".
    exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] failed to initialise MSVC environment
    exit /b 1
)

rem ---- configure (only if not configured yet) ----
if not exist "%BUILDDIR%\CMakeCache.txt" (
    echo [cmake] configuring Qt project...
    cmake -S "%ROOT%\qt" -B "%BUILDDIR%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="%QTDIR%" ^
        -DCMAKE_MAKE_PROGRAM="%NINJA%"
    if errorlevel 1 (
        echo [ERROR] cmake configure failed
        exit /b 1
    )
)

rem ---- build ----
cmake --build "%BUILDDIR%" --parallel
if errorlevel 1 exit /b 1

echo.
echo [OK] %ROOT%\build\patient.exe   (患者端)
echo [OK] %ROOT%\build\admin.exe     (管理端)
echo      Run them with:  tools\run_qt.bat patient
echo                      tools\run_qt.bat admin
echo      Add --shared to either one to share the same data:
echo                      tools\run_qt.bat patient --shared
exit /b 0
