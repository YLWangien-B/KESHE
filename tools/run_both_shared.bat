@echo off
rem ============================================================================
rem  run_both_shared.bat - open BOTH programs in shared mode (demo helper)
rem
rem  Starts the admin program first (it initialises the shared region) and then
rem  the patient program (it reuses that region). Two windows open side by side.
rem
rem  Use this when you want to show the two ends talking to each other:
rem    - book on the patient side  -> the admin side sees it immediately
rem    - stop a clinic on the admin side -> the patient side shows the
rem      rescheduled status
rem    - call a number on the admin side -> the patient queue updates
rem
rem  NOTE: ASCII-only comments on purpose (cmd.exe reads .bat with code page 936).
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"

echo [both] starting admin and patient in shared mode...
start "Hospital - admin" cmd /c ""%TOOLDIR%\run_qt.bat" admin --shared & pause"
timeout /t 2 /nobreak >nul
start "Hospital - patient" cmd /c ""%TOOLDIR%\run_qt.bat" patient --shared & pause"
echo [both] two windows opened. Close them when done.
exit /b 0