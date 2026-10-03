@echo off
setlocal EnableExtensions

rem Build the non-Android C# solution from any working directory.
rem Usage: tools\build\build-cs.bat [Debug|Release]

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"

if /i "%CONFIG%"=="help" goto :usage
if /i "%CONFIG%"=="-h" goto :usage
if /i "%CONFIG%"=="--help" goto :usage
if /i "%CONFIG%"=="Debug" goto :config_valid
if /i "%CONFIG%"=="Release" goto :config_valid
goto :usage_error

:config_valid
if not "%~2"=="" goto :usage_error

if not exist "%REPO_ROOT%\src\MphRead.sln" (
    echo [build-cs] ERROR: Non-Android solution not found under "%REPO_ROOT%".
    exit /b 1
)
where dotnet >nul 2>nul
if errorlevel 1 (
    echo [build-cs] ERROR: dotnet was not found on PATH.
    exit /b 1
)

pushd "%REPO_ROOT%" || exit /b 1
echo [build-cs] Repository   : %REPO_ROOT%
echo [build-cs] Configuration: %CONFIG%
echo [build-cs] Solution     : src\MphRead.sln (non-Android)
echo.

dotnet build "src\MphRead.sln" -c "%CONFIG%"
set "BUILD_RC=%ERRORLEVEL%"

if "%BUILD_RC%"=="0" (
    echo.
    echo [build-cs] Build succeeded.
) else (
    echo.
    echo [build-cs] Build failed with exit code %BUILD_RC%.
)

popd
exit /b %BUILD_RC%

:usage
echo Usage: tools\build\build-cs.bat [Debug^|Release]
echo        Default configuration: Release
exit /b 0

:usage_error
echo [build-cs] ERROR: Configuration must be Debug or Release.
call :usage
exit /b 2
