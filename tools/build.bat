@echo off
rem ============================================================================
rem  build.bat - build the project with MSVC (Visual Studio 2022)
rem
rem  usage: build.bat             -> build\hospital.exe     (console UI)
rem         build.bat gen         -> build\gen_data.exe
rem         build.bat clean       -> remove the build directory
rem
rem  The Qt graphical UI is NOT built here: it needs CMake (Qt ships its own
rem  CMake config file), so it has its own script:
rem         tools\build_qt.bat    -> build\hospital_qt.exe
rem
rem  NOTE: comments are ASCII-only on purpose. cmd.exe reads a .bat with the
rem  system code page (936 on a Simplified Chinese Windows); a UTF-8 file with
rem  Chinese comments gets garbled and parts of it are executed as commands.
rem  Chinese text is only used inside echo statements, never inside an if() block.
rem ============================================================================
setlocal enabledelayedexpansion

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build"

rem ---- target argument (default: hospital) ----
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=hospital"

if /I "%TARGET%"=="clean" goto :do_clean
if /I "%TARGET%"=="hospital" goto :do_build
if /I "%TARGET%"=="gen" goto :do_build

echo [ERROR] unknown target: %TARGET%
echo.
echo    usage: build.bat               -^> build\hospital.exe       (console UI)
echo           build.bat gen           -^> build\gen_data.exe
echo           build.bat clean         -^> remove the build directory
echo.
echo    the Qt graphical UI has its own script: tools\build_qt.bat
exit /b 1

:do_clean
if exist "%BUILD%" rmdir /s /q "%BUILD%"
echo [clean] removed %BUILD%
exit /b 0

:do_build
rem ---- locate vcvars64.bat: prefer vswhere, fall back to well-known paths ----
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

if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%"

rem /utf-8: sources carry a UTF-8 BOM, and narrow string literals must also be
rem emitted as UTF-8 so that generated data files and runtime comparisons agree.
set "CFLAGS=/nologo /W4 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\src""
set "RC=0"

if /I "%TARGET%"=="gen" (
    cl %CFLAGS% /Fe:gen_data.exe /Fo:gen_data.obj "%ROOT%\tools\gen_data.cpp"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

rem hospital + db: db.cpp holds the single instance of Hospital, so its
rem definition exists in exactly one place and is shared with the Qt program
rem (which compiles the same file - see qt\CMakeLists.txt).
cl %CFLAGS% /Fe:hospital.exe "%ROOT%\src\main.cpp" "%ROOT%\src\db.cpp"
set "RC=!errorlevel!"
popd
exit /b %RC%
