@echo off
rem ============================================================================
rem  run_demo.bat - open ONE admin window and TWO patient windows, shared mode
rem
rem  Usage:
rem      tools\run_demo.bat          start admin + 2 patients
rem      tools\run_demo.bat clean    delete data\state.bin first (fresh data)
rem
rem  Window layout:
rem      window 1  admin     starts FIRST - it initialises the shared region
rem      window 2  patient   log in as one patient
rem      window 3  patient   log in as a DIFFERENT patient
rem
rem  Why the admin goes first: the first process to call shared::open() creates
rem  data\state.bin and loads the text data into it. The other two then just map
rem  the same file and reuse it. (Even if all three started at once it would
rem  still be correct - open() holds the named mutex while initialising - but
rem  starting the admin first makes the startup log much easier to read.)
rem
rem  Capacity: the shared region has 4 peer slots, so 3 windows is well within
rem  the limit.
rem
rem  NOTE: comments are ASCII-only on purpose. cmd.exe reads a .bat with the
rem  system code page (936 on Simplified Chinese Windows), so UTF-8 Chinese
rem  comments would be garbled and parts of them executed.
rem ============================================================================
setlocal

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if not exist "%ROOT%\build\patient.exe" (
    echo [demo] executables not found - building first...
    call "%TOOLDIR%\build_qt.bat"
    if errorlevel 1 (
        echo [demo] build failed
        exit /b 1
    )
)

if /I "%~1"=="clean" (
    if exist "%ROOT%\data\state.bin" (
        del /q "%ROOT%\data\state.bin"
        echo [demo] removed data\state.bin  ^(starting from the original text data^)
    ) else (
        echo [demo] data\state.bin was already absent
    )
)

echo.
echo [demo] 1/3  admin window ...
start "Hospital - admin" cmd /k ""%TOOLDIR%\run_qt.bat" admin --shared"
ping -n 4 127.0.0.1 >nul

echo [demo] 2/3  patient window 1 ...
start "Hospital - patient 1" cmd /k ""%TOOLDIR%\run_qt.bat" patient --shared"
ping -n 3 127.0.0.1 >nul

echo [demo] 3/3  patient window 2 ...
start "Hospital - patient 2" cmd /k ""%TOOLDIR%\run_qt.bat" patient --shared"
ping -n 3 127.0.0.1 >nul

echo.
echo ============================================================
echo  Three windows are open:
echo    admin      - queue / schedule / patients / data
echo    patient 1  - log in as one patient
echo    patient 2  - log in as a DIFFERENT patient
echo.
echo  Check the bottom-left status bar of any window:
echo    [shared ... peer 1]   means the two ends are connected
echo    [single process]      means --shared was lost
echo.
echo  Reset demo data:   tools\run_demo.bat clean
echo  Close all:         just close the three windows
echo ============================================================
exit /b 0
