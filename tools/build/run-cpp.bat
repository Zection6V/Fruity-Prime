@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ======================================================================
rem  Start the FruityPrime.exe that build-cpp.bat / build-cpp-and-log.bat
rem  produced under tools\build\out\<toolchain>-<config>\.
rem
rem  Usage:
rem    tools\build\run-cpp.bat [msys2^|msvc] [Release^|Debug^|...] [vulkan^|opengl^|auto] [-- extra args]
rem
rem    No toolchain/config: the most recently built FruityPrime.exe is used.
rem    Renderer defaults to vulkan (the same as "build-cpp.bat run").
rem    Anything after "--" is passed to the game as is.
rem ======================================================================

set "SCRIPT_DIR=%~dp0"
set "OUT_ROOT=%SCRIPT_DIR%out"
set "FILTER_TC="
set "FILTER_CFG="
set "RHI=vulkan"
set "EXTRA="

:parse
if "%~1"=="" goto :parsed
if "%~1"=="--" goto :rest
if /i "%~1"=="msys2"          (set "FILTER_TC=msys2"          & goto :next)
if /i "%~1"=="mingw"          (set "FILTER_TC=msys2"          & goto :next)
if /i "%~1"=="msvc"           (set "FILTER_TC=msvc"           & goto :next)
if /i "%~1"=="Release"        (set "FILTER_CFG=Release"       & goto :next)
if /i "%~1"=="Debug"          (set "FILTER_CFG=Debug"         & goto :next)
if /i "%~1"=="RelWithDebInfo" (set "FILTER_CFG=RelWithDebInfo"& goto :next)
if /i "%~1"=="MinSizeRel"     (set "FILTER_CFG=MinSizeRel"    & goto :next)
if /i "%~1"=="vulkan"         (set "RHI=vulkan"               & goto :next)
if /i "%~1"=="opengl"         (set "RHI=opengl"               & goto :next)
if /i "%~1"=="auto"           (set "RHI=auto"                 & goto :next)
echo [run] ERROR: unknown argument "%~1"
echo Usage: tools\build\run-cpp.bat [msys2^|msvc] [Release^|Debug^|RelWithDebInfo^|MinSizeRel] [vulkan^|opengl^|auto] [-- extra args]
exit /b 1
:next
shift
goto :parse
:rest
shift
:rest_loop
if "%~1"=="" goto :parsed
set "EXTRA=!EXTRA! %1"
shift
goto :rest_loop
:parsed

if not exist "%OUT_ROOT%" (
    echo [run] ERROR: "%OUT_ROOT%" does not exist. Build first with build-cpp.bat.
    exit /b 1
)

rem Newest FruityPrime.exe in a build-cpp.bat folder (msvc-*, msys2-*)
rem matching the filters; the other folders in out\ are test captures.
set "EXE="
for /f "usebackq delims=" %%E in (`powershell -NoProfile -Command ^
  "Get-ChildItem -Path '%OUT_ROOT%' -Directory | Where-Object { ($_.Name -like 'msvc-*' -or $_.Name -like 'msys2-*') -and ('%FILTER_TC%' -eq '' -or $_.Name -like '%FILTER_TC%-*') -and ('%FILTER_CFG%' -eq '' -or $_.Name -like '*-%FILTER_CFG%') } | ForEach-Object { Join-Path $_.FullName 'FruityPrime.exe' } | Where-Object { Test-Path $_ } | Get-Item | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName"`) do set "EXE=%%E"

if not defined EXE (
    echo [run] ERROR: no FruityPrime.exe found under "%OUT_ROOT%" ^(toolchain=%FILTER_TC% config=%FILTER_CFG%^).
    echo [run]        Build first: tools\build\build-cpp.bat
    exit /b 1
)
for %%I in ("%EXE%") do set "BUILD_DIR=%%~dpI"
set "BUILD_DIR=%BUILD_DIR:~0,-1%"

rem paths.txt has to sit beside the exe; borrow one from another build.
if not exist "%BUILD_DIR%\paths.txt" (
    for /d %%D in ("%OUT_ROOT%\*") do (
        if not exist "%BUILD_DIR%\paths.txt" if exist "%%D\paths.txt" (
            copy /y "%%D\paths.txt" "%BUILD_DIR%\paths.txt" >nul
            echo [run] paths.txt copied from %%D
        )
    )
)
if not exist "%BUILD_DIR%\paths.txt" echo [run] No paths.txt yet: the game will ask for the game files.

echo [run] Starting %EXE% -launcher -rhi %RHI%%EXTRA%
start "" /d "%BUILD_DIR%" "%EXE%" -launcher -rhi %RHI%%EXTRA%
exit /b 0
