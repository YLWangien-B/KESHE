@echo off
rem ============================================================================
rem  fix-encoding.bat - prepend a UTF-8 BOM to every source file under src\ and
rem                    tools\ (run this after adding or editing a source file)
rem
rem  Why: MSVC reads sources using the system code page (936/GBK on a Simplified
rem  Chinese Windows). A BOM-less UTF-8 file containing Chinese comments is then
rem  mis-parsed: a multi-byte sequence can end in 0x5C, which the preprocessor
rem  treats as a line continuation and swallows the following line of code.
rem  With a BOM, MSVC, g++ and clang all decode the file as UTF-8.
rem ============================================================================
setlocal enabledelayedexpansion

set "TOOLDIR=%~dp0"
if "%TOOLDIR:~-1%"=="\" set "TOOLDIR=%TOOLDIR:~0,-1%"
for %%i in ("%TOOLDIR%") do set "ROOT=%%~dpi"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$bom=[byte[]](0xEF,0xBB,0xBF); $n=0;" ^
  "foreach ($root in @('%ROOT%\src','%ROOT%\tools')) {" ^
  "  Get-ChildItem -Path $root -Recurse -Include *.h,*.cpp | ForEach-Object {" ^
  "    $b=[System.IO.File]::ReadAllBytes($_.FullName);" ^
  "    if ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) { return }" ^
  "    $a=New-Object byte[] ($b.Length+3); [Array]::Copy($bom,0,$a,0,3); [Array]::Copy($b,0,$a,3,$b.Length);" ^
  "    [System.IO.File]::WriteAllBytes($_.FullName,$a); Write-Host ('BOM added: ' + $_.Name); $n++ } };" ^
  "Write-Host ('done, ' + $n + ' file(s) updated')"

exit /b 0
