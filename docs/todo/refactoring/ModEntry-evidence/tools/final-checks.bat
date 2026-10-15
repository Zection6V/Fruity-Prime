@echo off
set "PATH=%SystemRoot%\system32;%SystemRoot%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;C:\Program Files\Git\cmd"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\Admin\Documents\git\Fruity-Prime\tools\build\out\msvc-Release
for %%F in (ServerCommands ModEntry) do (
  ninja -t commands "CMakeFiles\fruity_mphread_native.dir\src\MphRead.Native\Mods\Entry\%%F.cpp.obj" 2>nul | findstr /i "cl.exe" > "%~dp0cmd-%%F.txt"
  ninja -t commands "CMakeFiles\fruity_mphread_native.dir\src\MphRead.Native\Mods\%%F.cpp.obj" 2>nul | findstr /i "cl.exe" >> "%~dp0cmd-%%F.txt"
)
echo == MPHREAD_SERVER syntax check
"C:\msys64\mingw64\bin\python.exe" -I "%~dp0server-define-check.py" "%~dp0." ServerCommands ModEntry
echo == compile timing
"C:\msys64\mingw64\bin\python.exe" -I "%~dp0measure-compile.py" 2
