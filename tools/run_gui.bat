@echo off
rem ============================================================================
rem  run_gui.bat - build (if needed) and run the graphical program
rem
rem  Usage (from anywhere):  tools\run_gui.bat  [data-directory]
rem
rem  The program loads data\ relative to the current directory, so this script
rem  switches to the project root first.
rem  Sister script of run.bat: one runs the console UI, one runs the graphical UI.
rem  Both share the same data-structure layer, data layer and business layer.
rem
rem  NOTE: comments are kept ASCII-only on purpose. cmd.exe reads a .bat with the
rem  system code page (936 on a Simplified Chinese Windows), so a UTF-8 file with
rem  Chinese comments gets garbled and parts of it are executed as commands.
rem  (run.bat, build.bat and selfcheck.bat follow the same rule.)
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

rem Bind stdin to nul while calling build.bat: it calls vcvars64.bat, which
rem starts a cmd.exe child that consumes the inherited stdin redirection.
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

pushd "%ROOT%"
build\hospital_gui.exe %1
set "RC=%errorlevel%"
popd
exit /b %RC%
