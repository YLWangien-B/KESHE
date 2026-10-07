@echo off
rem ============================================================================
rem  run.bat - build (if needed) and run the program
rem  Must be started from the project root: the program loads data\ relative to
rem  the current directory, so tools\run.bat switches to the project root first.
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

rem NOTE: build.bat is invoked with stdin bound to nul on purpose. build.bat calls
rem vcvars64.bat, which starts a cmd.exe child process; that child consumes the
rem inherited stdin redirection (e.g. tools\run.bat < case.txt), leaving this
rem script with an exhausted stdin - the menu would then flash by and exit.
rem This is the same pitfall as calling system("chcp ...") from the program.
if not exist "%ROOT%\build\hospital.exe" (
    echo [info] hospital.exe not found, building first...
    call "%TOOLDIR%\build.bat" < nul
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
build\hospital.exe
set "RC=%errorlevel%"
popd
exit /b %RC%
