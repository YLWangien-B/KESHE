@echo off
rem ============================================================================
rem  run_qt.bat - build (if needed) and run the Qt graphical program
rem
rem  Usage (from anywhere):
rem      tools\run_qt.bat  [patient|admin] [--shared] [data-directory]
rem
rem  Examples:
rem      tools\run_qt.bat                     patient, single process
rem      tools\run_qt.bat admin               admin, single process
rem      tools\run_qt.bat admin   --shared    shares data with the other program
rem      tools\run_qt.bat patient --shared
rem
rem  Two things this script must do beyond launching the exe:
rem    1. switch the working directory to the project root, because the program
rem       looks for data\ relative to wherever it is started from;
rem    2. put Qt's DLL directory on PATH, otherwise the exe fails with
rem       "Qt6Widgets.dll was not found".
rem
rem  ARGUMENT HANDLING (this used to be broken):
rem    The old version did
rem        if not "%ROLE%"=="patient" if not "%ROLE%"=="admin" ( ... ) else ( shift )
rem    and then passed "%1" to the exe. When ROLE *was* patient/admin the first
rem    IF was false, so the whole compound statement - including its ELSE - was
rem    skipped and SHIFT never ran. "%1" was therefore still the role name, and
rem    "admin --shared" reached the program as just "admin" - the program saw no
rem    --shared flag and silently ran in single-process mode.
rem
rem    The version below never relies on SHIFT. It captures each argument into a
rem    variable first, decides which one is the role, and forwards the rest
rem    verbatim. %1..%3 are plenty: the program takes at most two options.
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

rem ---- 1. capture the raw arguments (before any decision) --------------------
set "A1=%~1"
set "A2=%~2"
set "A3=%~3"

rem ---- 2. decide the role, and what to forward to the exe --------------------
rem     If A1 is a role name, it is consumed; the rest is forwarded.
rem     Otherwise A1 is already an option, so keep the default role and
rem     forward everything.
set "ROLE=patient"
if /I "%A1%"=="patient" goto :got_role
if /I "%A1%"=="admin"   goto :got_role

set "PASS=%A1% %A2% %A3%"
goto :role_done

:got_role
set "ROLE=%A1%"
set "PASS=%A2% %A3%"

:role_done
rem trim: collapse runs of spaces so the exe does not see empty arguments
set "PASS=%PASS:  = %"

rem ---- 3. build / data / runtime environment --------------------------------
if not exist "%ROOT%\build\%ROLE%.exe" (
    call "%TOOLDIR%\build_qt.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%ROOT%\data\symptoms.txt" (
    echo [data] test data missing, generating...
    if exist "%ROOT%\tools\build.bat" call "%TOOLDIR%\build.bat" gen
    if exist "%ROOT%\build\gen_data.exe" "%ROOT%\build\gen_data.exe" "%ROOT%\data"
)

set "PATH=%QTDIR%\bin;%PATH%"
cd /d "%ROOT%"

rem ---- 4. launch ------------------------------------------------------------
echo [run] build\%ROLE%.exe %PASS%
"%ROOT%\build\%ROLE%.exe" %PASS%
exit /b %errorlevel%
