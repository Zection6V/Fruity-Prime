@echo off
setlocal
rem Instagib uses the same defaults and process tracking as start.bat.
rem Run with "stop" to stop this launcher session.
if /i "%~1"=="stop" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start.ps1" -Stop
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start.ps1" -Instagib %*
)
set "TEST_EXIT=%ERRORLEVEL%"
if not "%TEST_EXIT%"=="0" pause
exit /b %TEST_EXIT%
