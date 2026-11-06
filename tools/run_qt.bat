@echo off
rem ============================================================================
rem  run_qt.bat - build (if needed) and run the Qt graphical program
rem
rem  Usage (from anywhere):  tools\run_qt.bat  [patient|admin] [--shared] [data-directory]
rem
rem  Examples:
rem      tools\run_qt.bat                 patient, single process
rem      tools\run_qt.bat admin           admin, single process
rem      tools\run_qt.bat patient --shared    shares data with the other program
rem
rem  Two things this script must do beyond launching the exe:
rem    1. switch the working directory to the project root, because the program
rem       looks for data\ relative to wherever it is started from;
rem    2. put Qt's DLL directory on PATH, otherwise the exe fails with
rem       "Qt6Widgets.dll was not found". (The alternative is to copy the DLLs
rem       next to the exe with windeployqt, which is what you would do when
rem       handing the program to someone else.)
rem
rem  NOTE: comments are ASCII-only on purpose. cmd.exe reads a .bat with the
rem  system code page (936 on a Simplified Chinese Windows), so a UTF-8 file
rem  with Chinese comments gets garbled and parts of it are executed.
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "QTDIR=D:\Qt\6.8.3\msvc2022_64"

set "ROLE=%~1"
if "%ROLE%"=="" set "ROLE=patient"
if /I not "%ROLE%"=="patient" if /I not "%ROLE%"=="admin" (
    set "ROLE=patient"
) else (
    shift
)

if not exist "%ROOT%\build\%ROLE%.exe" (
    call "%TOOLDIR%\build_qt.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%ROOT%\data\symptoms.txt" (
    echo [data] test data missing, generating...
    if exist "%ROOT%\tools\build.bat" (
        call "%TOOLDIR%\build.bat" gen
        if exist "%ROOT%\build\gen_data.exe" "%ROOT%\build\gen_data.exe" "%ROOT%\data"
    )
)

set "PATH=%QTDIR%\bin;%PATH%"
cd /d "%ROOT%"
"%ROOT%\build\%ROLE%.exe" %1
exit /b %errorlevel%
