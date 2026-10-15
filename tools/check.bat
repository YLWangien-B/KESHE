@echo off
rem ============================================================================
rem  check.bat - run the console test suite and compare against the expected output
rem
rem  Usage (from anywhere):  tools\check.bat
rem
rem  What it does:
rem    1. builds hospital.exe if missing
rem    2. feeds _test\in.txt to hospital.exe (drives all seven features plus a
rem       few edge cases: unknown code, cancelled ticket, same start/end room,
rem       pass-the-queue twice, empty doctor list for a department)
rem    3. compares the output to _test\expected.txt line by line
rem
rem  Exit code 0 = identical, 1 = differences (line numbers printed).
rem
rem  Why this exists: the console UI prints every data structure's effect on
rem  screen (triage scores, remaining quota, queue numbers, route nodes...).
rem  If a data-structure change alters any behaviour, this catches it as a
rem  concrete diff instead of a vague "looks fine".
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

if not exist "%ROOT%\build\hospital.exe" (
    echo [check] building hospital.exe first...
    call "%TOOLDIR%\build.bat" || exit /b 1
)

if not exist "%ROOT%\_test\in.txt" (
    echo [ERROR] %ROOT%\_test\in.txt not found
    exit /b 1
)

pushd "%ROOT%"
"build\hospital.exe" data < "_test\in.txt" > "_test\actual.txt" 2>&1
set "RC=!errorlevel!"
popd

if not "!RC!"=="0" (
    echo [ERROR] hospital.exe exited with !RC!
    type "%ROOT%\_test\actual.txt"
    exit /b 1
)

fc /b "%ROOT%\_test\expected.txt" "%ROOT%\_test\actual.txt" >nul
if errorlevel 1 (
    echo [FAIL] output differs from expected. First differences:
    echo.
    fc "%ROOT%\_test\expected.txt" "%ROOT%\_test\actual.txt"
    echo.
    echo   expected: _test\expected.txt
    echo   actual:   _test\actual.txt
    exit /b 1
)

echo [PASS] console output matches expected.txt exactly
exit /b 0
