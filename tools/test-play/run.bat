@echo off
setlocal

if "%~1"=="" (
    echo Usage: tools\test-play\run.bat -scenario-flag [game options...]
    pause
    exit /b 2
)

set "GAME_DIR=%~dp0..\build\out\msvc-Release"
for %%I in ("%GAME_DIR%") do set "GAME_DIR=%%~fI"
set "GAME_EXE=%GAME_DIR%\FruityPrime.exe"

if not exist "%GAME_EXE%" (
    echo FruityPrime.exe was not found: "%GAME_EXE%"
    echo Build it with tools\build\build-cpp.bat msvc Release first.
    pause
    exit /b 1
)
if not exist "%GAME_DIR%\paths.txt" (
    echo paths.txt was not found beside FruityPrime.exe:
    echo "%GAME_DIR%\paths.txt"
    echo Set up the Native build with your extracted game files first.
    pause
    exit /b 1
)

pushd "%GAME_DIR%"
start "" /wait "%GAME_EXE%" %*
set "GAME_EXIT=%ERRORLEVEL%"
popd
exit /b %GAME_EXIT%
