@echo off
setlocal EnableExtensions
rem Double-click helper: runs build.bat and writes the full output to
rem tools\build\out\build.log. With no arguments it runs "deps" first.
set "SCRIPT_DIR=%~dp0"
if not exist "%SCRIPT_DIR%out" mkdir "%SCRIPT_DIR%out"
set "LOG=%SCRIPT_DIR%out\build.log"
set "ARGS=%*"
if "%ARGS%"=="" set "ARGS=deps"
echo Building... (log: %LOG%)
call "%SCRIPT_DIR%build.bat" %ARGS% > "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
echo exit code: %RC%>> "%LOG%"
powershell -NoProfile -Command "Get-Content -Tail 40 '%LOG%'"
echo.
echo exit code: %RC%
pause
exit /b %RC%
