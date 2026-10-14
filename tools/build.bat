@echo off
rem ============================================================================
rem  build.bat - build the project with MSVC (Visual Studio 2022)
rem
rem  usage: build.bat             -> build\hospital.exe       (console UI)
rem         build.bat gui         -> build\hospital_gui.exe   (graphical UI)
rem         build.bat gen         -> build\gen_data.exe
rem         build.bat clean       -> remove the build directory
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
if /I "%TARGET%"=="gui" goto :do_build
if /I "%TARGET%"=="gen" goto :do_build
if /I "%TARGET%"=="probe" goto :do_build

echo [ERROR] unknown target: %TARGET%
echo.
echo    usage: build.bat               -^> build\hospital.exe       (console UI)
echo           build.bat gui           -^> build\hospital_gui.exe   (graphical UI)
echo           build.bat gen           -^> build\gen_data.exe
echo           build.bat clean         -^> remove the build directory
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
if not defined VCVARS if exist "F:\Visual studio\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=F:\Visual studio\Community\VC\Auxiliary\Build\vcvars64.bat"
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

if /I "%TARGET%"=="gui" goto :do_gui

rem probe: build a temporary diagnostic program src\_probe.cpp + tools\_probe.cpp
rem (development scaffolding, not part of the deliverable)
if /I "%TARGET%"=="probe" (
    set "PROBE_SRC=%ROOT%\src\_probe.cpp"
    if not exist "!PROBE_SRC!" set "PROBE_SRC=%ROOT%\tools\_probe.cpp"
    if not exist "!PROBE_SRC!" (
        echo [ERROR] no probe source found: put src\_probe.cpp or tools\_probe.cpp first
        popd
        exit /b 1
    )
    cl %CFLAGS% /Fe:probe.exe /Fo:probe.obj "!PROBE_SRC!"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

rem hospital + db: db.cpp holds the single instance of Hospital, shared with the
rem graphical program too, so its definition exists in exactly one place.
cl %CFLAGS% /Fe:hospital.exe "%ROOT%\src\main.cpp" "%ROOT%\src\db.cpp"
set "RC=!errorlevel!"
popd
exit /b %RC%

rem ---------------------------------------------------------------------------
rem  graphical UI (Dear ImGui + DirectX 11)
rem
rem  Shares the data-structure layer, data layer and business layer in src\ with
rem  the console UI; only the UI layer differs (gui\src). This block works from
rem  the project root with relative paths on purpose: cl.exe mangles a non-ASCII
rem  absolute path through the system code page and then cannot open the file.
rem  /I must not be quoted either - cmd passes the quotes through to cl.
rem  ImGui sources are compiled separately at /W3 (they target many platforms
rem  and would flood /W4); our own code stays at /W4.
rem ---------------------------------------------------------------------------
:do_gui
pushd "%ROOT%"
if not exist "build\obj_gui" mkdir "build\obj_gui"

cl /nologo /W4 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c ^
    gui\src\gui_main.cpp gui\src\gui_app.cpp gui\src\gui_font.cpp gui\src\gui_state.cpp ^
    gui\src\panel_common.cpp gui\src\panel_triage.cpp gui\src\panel_doctor.cpp ^
    gui\src\panel_booking.cpp gui\src\panel_manage.cpp gui\src\panel_schedule.cpp ^
    gui\src\panel_route.cpp gui\src\panel_queue.cpp ^
    gui\src\panel_about.cpp src\db.cpp
if errorlevel 1 ( popd & exit /b 1 )

rem the .obj files land in the project root by default; move them into build\obj_gui
move /y *.obj "build\obj_gui\" >nul

cl /nologo /W3 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c /Fo:build\obj_gui\ ^
    third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp ^
    third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp
if errorlevel 1 ( popd & exit /b 1 )

cl /nologo /W3 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c /Fo:build\obj_gui\ ^
    third_party\imgui\backends\imgui_impl_win32.cpp third_party\imgui\backends\imgui_impl_dx11.cpp
if errorlevel 1 ( popd & exit /b 1 )

link /nologo /OUT:"build\hospital_gui.exe" ^
    "build\obj_gui\gui_main.obj" "build\obj_gui\gui_app.obj" "build\obj_gui\gui_font.obj" ^
    "build\obj_gui\gui_state.obj" "build\obj_gui\panel_common.obj" ^
    "build\obj_gui\panel_triage.obj" "build\obj_gui\panel_doctor.obj" ^
    "build\obj_gui\panel_booking.obj" "build\obj_gui\panel_manage.obj" ^
    "build\obj_gui\panel_schedule.obj" "build\obj_gui\panel_route.obj" ^
    "build\obj_gui\panel_queue.obj" ^
    "build\obj_gui\panel_about.obj" "build\obj_gui\db.obj" ^
    "build\obj_gui\imgui.obj" "build\obj_gui\imgui_draw.obj" ^
    "build\obj_gui\imgui_tables.obj" "build\obj_gui\imgui_widgets.obj" ^
    "build\obj_gui\imgui_impl_win32.obj" "build\obj_gui\imgui_impl_dx11.obj" ^
    d3d11.lib d3dcompiler.lib dxgi.lib
set "RC=!errorlevel!"
popd
exit /b !RC!
