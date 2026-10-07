@echo off
rem ============================================================================
rem  build.bat - build the project with MSVC (Visual Studio 2022)
rem  usage: build.bat             -> build build\hospital.exe
rem         build.bat selfcheck   -> build build\selfcheck.exe
rem         build.bat clean       -> remove build directory
rem ============================================================================
setlocal enabledelayedexpansion

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build"

if /I "%~1"=="clean" (
    if exist "%BUILD%" rmdir /s /q "%BUILD%"
    echo [clean] removed %BUILD%
    exit /b 0
)

rem ---- locate vcvars64.bat: prefer vswhere, fall back to well-known paths ----
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%i"
    )
    if defined VSINSTALL if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "F:\Visual studio\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=F:\Visual studio\Community\VC\Auxiliary\Build\vcvars64.bat"
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

if /I "%~1"=="selfcheck" (
    cl %CFLAGS% /Fe:selfcheck.exe "%ROOT%\src\selfcheck.cpp" "%ROOT%\src\db.cpp"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

if /I "%~1"=="gen" (
    cl %CFLAGS% /Fe:gen_data.exe /Fo:gen_data.obj "%ROOT%\tools\gen_data.cpp"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

rem probe: build the temporary diagnostic program src\_probe.cpp (not part of the
rem deliverable; used while investigating data structure behaviour)
if /I "%~1"=="probe" (
    cl %CFLAGS% /Fe:probe.exe /Fo:probe.obj "%ROOT%\src\_probe.cpp"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

rem hospital + db: db.cpp holds the single instance of Hospital, shared with the
rem self-check program, so its definition exists in exactly one place.
cl %CFLAGS% /Fe:hospital.exe "%ROOT%\src\main.cpp" "%ROOT%\src\db.cpp"
set "RC=!errorlevel!"
popd
exit /b %RC%
