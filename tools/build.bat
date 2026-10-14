@echo off
rem ============================================================================
rem  build.bat - build the project with MSVC (Visual Studio 2022)
rem  usage: build.bat             -> build build\hospital.exe       (字符界面主程序)
rem         build.bat gui         -> build build\hospital_gui.exe   (图形界面主程序)
rem         build.bat selfcheck   -> build build\selfcheck.exe
rem         build.bat gen         -> build build\gen_data.exe
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

rem ---------------------------------------------------------------------------
rem  图形界面主程序（Dear ImGui + DirectX 11）
rem
rem  与字符界面主程序共用 src 下的数据结构层、数据层、业务层，
rem  只是界面层换成了 gui\src 下的一套代码；第三方库 ImGui 在 third_party\imgui。
rem
rem  三点讲究：
rem    1) 先 pushd 到工程根，源码一律用相对路径。cl.exe 收到带中文的绝对路径时
rem       会按系统代码页转换，然后报「无法打开源文件」—— 相对路径避开这个问题。
rem       所以这一段的开关要重新写一遍，不能直接引用上面那个 %CFLAGS%。
rem    2) /I 后面不加引号：cmd 会把引号一起传给 cl，于是报「无法打开包括文件」。
rem    3) ImGui 的源码单独 /c 成各自的 .obj 并降到 /W3 —— 它们是给多平台准备的，
rem       在 /W4 下会刷屏；自己写的界面代码仍然是 /W4。
rem ---------------------------------------------------------------------------
if /I "%~1"=="gui" (
    pushd "%ROOT%"
    if not exist "build\obj_gui" mkdir "build\obj_gui"

    rem ---- 自己的界面代码 + 共用的数据层实例 ----
    cl /nologo /W4 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c ^
        gui\src\gui_main.cpp gui\src\gui_app.cpp gui\src\gui_font.cpp gui\src\gui_state.cpp ^
        gui\src\panel_common.cpp gui\src\panel_triage.cpp gui\src\panel_doctor.cpp ^
        gui\src\panel_booking.cpp gui\src\panel_manage.cpp gui\src\panel_schedule.cpp ^
        gui\src\panel_route.cpp gui\src\panel_queue.cpp gui\src\panel_floor.cpp ^
        gui\src\panel_about.cpp src\db.cpp
    if errorlevel 1 ( popd & exit /b 1 )

    rem 这批 .obj 默认落在工程根，挪进 build\obj_gui，别把源码目录弄脏
    move /y *.obj "build\obj_gui\" >nul

    rem ---- Dear ImGui：核心四个 + 两个后端，分两批编 ----
    cl /nologo /W3 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c /Fo:build\obj_gui\ ^
        third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp ^
        third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp
    if errorlevel 1 ( popd & exit /b 1 )

    cl /nologo /W3 /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc /Igui\src /Ithird_party\imgui /Ithird_party\imgui\backends /c /Fo:build\obj_gui\ ^
        third_party\imgui\backends\imgui_impl_win32.cpp third_party\imgui\backends\imgui_impl_dx11.cpp
    if errorlevel 1 ( popd & exit /b 1 )

    rem ---- 链接 ----
    link /nologo /OUT:"build\hospital_gui.exe" ^
        "build\obj_gui\gui_main.obj" "build\obj_gui\gui_app.obj" "build\obj_gui\gui_font.obj" ^
        "build\obj_gui\gui_state.obj" "build\obj_gui\panel_common.obj" ^
        "build\obj_gui\panel_triage.obj" "build\obj_gui\panel_doctor.obj" ^
        "build\obj_gui\panel_booking.obj" "build\obj_gui\panel_manage.obj" ^
        "build\obj_gui\panel_schedule.obj" "build\obj_gui\panel_route.obj" ^
        "build\obj_gui\panel_queue.obj" "build\obj_gui\panel_floor.obj" ^
        "build\obj_gui\panel_about.obj" "build\obj_gui\db.obj" ^
        "build\obj_gui\imgui.obj" "build\obj_gui\imgui_draw.obj" ^
        "build\obj_gui\imgui_tables.obj" "build\obj_gui\imgui_widgets.obj" ^
        "build\obj_gui\imgui_impl_win32.obj" "build\obj_gui\imgui_impl_dx11.obj" ^
        d3d11.lib d3dcompiler.lib dxgi.lib
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)

rem probe: build a temporary diagnostic program src\_probe.cpp + tools\_probe.cpp
rem (not part of the deliverable; used while investigating data structure behaviour)
if /I "%~1"=="probe" (
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
rem self-check program, so its definition exists in exactly one place.
cl %CFLAGS% /Fe:hospital.exe "%ROOT%\src\main.cpp" "%ROOT%\src\db.cpp"
set "RC=!errorlevel!"
popd
exit /b %RC%
