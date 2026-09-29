@echo off
setlocal EnableExtensions

rem Build the native (C++) Android APK, the way .github/workflows/native-cpp-android.yml does:
rem   1. vcpkg builds curl/libarchive/freetype/libjpeg-turbo/zlib for each Android ABI
rem   2. CMake builds libFruityPrime.so for each ABI
rem   3. the libraries are copied into android\app\src\main\jniLibs
rem   4. Gradle assembles and signs the release APK (development key)
rem
rem Usage: tools\build\build-android-cpp.bat [all^|arm64-v8a^|x86_64] [apk^|libs]
rem   apk  (default) also runs Gradle and leaves tools\build\out\FruityPrime.apk
rem   libs           stops after the shared libraries (no Gradle needed)
rem
rem Needs: Android SDK (platform, build-tools, NDK, CMake), JDK 17, vcpkg (VCPKG_ROOT,
rem default C:\vcpkg) and Gradle 8.9 (GRADLE_HOME, default C:\Gradle\gradle-8.9, or on PATH).

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
for %%I in ("%SCRIPT_DIR%out") do set "OUT_ROOT=%%~fI"
set "ABI_FILTER=%~1"
if not defined ABI_FILTER set "ABI_FILTER=all"
set "TARGET=%~2"
if not defined TARGET set "TARGET=apk"
if /i "%ABI_FILTER%"=="help" goto :usage
if /i "%ABI_FILTER%"=="-h" goto :usage
if /i "%ABI_FILTER%"=="--help" goto :usage
if not "%~3"=="" goto :usage_error
if /i not "%TARGET%"=="apk" if /i not "%TARGET%"=="libs" goto :usage_error

if not defined ANDROID_HOME if defined ANDROID_SDK_ROOT set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
if not defined ANDROID_HOME set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
if not defined ANDROID_SDK_ROOT set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
if not defined ANDROID_CMAKE_VERSION set "ANDROID_CMAKE_VERSION=3.31.6"
if not defined BUILD_JOBS set "BUILD_JOBS=%NUMBER_OF_PROCESSORS%"
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"

if not defined JAVA_HOME (
    for /d %%J in ("%ProgramFiles%\Microsoft\jdk-17*") do (
        if not defined JAVA_HOME if exist "%%~fJ\bin\javac.exe" set "JAVA_HOME=%%~fJ"
    )
)
if not defined JAVA_HOME (
    echo [android-cpp] ERROR: JAVA_HOME is not set and no JDK 17 was found.
    exit /b 1
)

set "CMAKE_BIN=%ANDROID_HOME%\cmake\%ANDROID_CMAKE_VERSION%\bin"
if not defined CMAKE_EXE if exist "%CMAKE_BIN%\cmake.exe" set "CMAKE_EXE=%CMAKE_BIN%\cmake.exe"
if not defined CMAKE_EXE (
    for /f "delims=" %%C in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%C"
)
if not defined CMAKE_EXE (
    echo [android-cpp] ERROR: cmake.exe was not found. Install the Android SDK CMake package.
    exit /b 1
)
if not exist "%CMAKE_EXE%" (
    echo [android-cpp] ERROR: CMake does not exist: "%CMAKE_EXE%".
    exit /b 1
)
for %%I in ("%CMAKE_EXE%") do set "CMAKE_BIN=%%~dpI"
set "PATH=%CMAKE_BIN%;%PATH%"

set "NINJA_EXE=%CMAKE_BIN%ninja.exe"
if not exist "%NINJA_EXE%" (
    set "NINJA_EXE="
    for /f "delims=" %%N in ('where ninja 2^>nul') do if not defined NINJA_EXE set "NINJA_EXE=%%N"
)
if not defined NINJA_EXE (
    echo [android-cpp] ERROR: ninja.exe was not found. Install it with Android SDK CMake or add it to PATH.
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [android-cpp] ERROR: vcpkg.exe was not found under "%VCPKG_ROOT%".
    echo [android-cpp]        git clone https://github.com/microsoft/vcpkg.git C:\vcpkg ^&^& C:\vcpkg\bootstrap-vcpkg.bat
    exit /b 1
)

if not exist "%REPO_ROOT%\cmake\ExportAndroidConfig.cmake" (
    echo [android-cpp] ERROR: The shared Android build contract was not found.
    exit /b 1
)
if not exist "%OUT_ROOT%" mkdir "%OUT_ROOT%"
set "CONTRACT_FILE=%TEMP%\fruity-android-contract-%RANDOM%.txt"
"%CMAKE_EXE%" "-DOUTPUT_FILE=%CONTRACT_FILE%" -P "%REPO_ROOT%\cmake\ExportAndroidConfig.cmake"
if errorlevel 1 goto :contract_error
if not exist "%CONTRACT_FILE%" goto :contract_error
for /f "usebackq tokens=1,* delims==" %%A in ("%CONTRACT_FILE%") do set "%%A=%%B"
del /q "%CONTRACT_FILE%" >nul 2>nul

set "ABI_VALID=0"
if /i "%ABI_FILTER%"=="all" set "ABI_VALID=1"
for %%A in (%FRUITY_ANDROID_ABIS%) do if /i "%ABI_FILTER%"=="%%A" set "ABI_VALID=1"
if "%ABI_VALID%"=="0" goto :usage_error

if not defined ANDROID_NDK_HOME if defined ANDROID_NDK_ROOT set "ANDROID_NDK_HOME=%ANDROID_NDK_ROOT%"
if not defined ANDROID_NDK_HOME set "ANDROID_NDK_HOME=%ANDROID_HOME%\ndk\%ANDROID_NDK_VERSION%"

if not exist "%ANDROID_HOME%\platforms\android-%ANDROID_COMPILE_SDK%\android.jar" (
    echo [android-cpp] ERROR: Android API %ANDROID_COMPILE_SDK% was not found under "%ANDROID_HOME%".
    exit /b 1
)
if not exist "%ANDROID_HOME%\build-tools\%ANDROID_BUILD_TOOLS%\aapt.exe" (
    echo [android-cpp] ERROR: Build Tools %ANDROID_BUILD_TOOLS% were not found under "%ANDROID_HOME%".
    exit /b 1
)
if not exist "%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" (
    echo [android-cpp] ERROR: The NDK %ANDROID_NDK_VERSION% toolchain was not found at "%ANDROID_NDK_HOME%".
    exit /b 1
)
findstr /c:"Pkg.Revision = %ANDROID_NDK_VERSION%" "%ANDROID_NDK_HOME%\source.properties" >nul 2>nul
if errorlevel 1 (
    echo [android-cpp] ERROR: "%ANDROID_NDK_HOME%" is not NDK %ANDROID_NDK_VERSION%.
    exit /b 1
)

set "GRADLE_CMD="
if /i "%TARGET%"=="apk" (
    if defined GRADLE_HOME if exist "%GRADLE_HOME%\bin\gradle.bat" set "GRADLE_CMD=%GRADLE_HOME%\bin\gradle.bat"
    if not defined GRADLE_CMD if exist "C:\Gradle\gradle-8.9\bin\gradle.bat" set "GRADLE_CMD=C:\Gradle\gradle-8.9\bin\gradle.bat"
    if not defined GRADLE_CMD for /f "delims=" %%G in ('where gradle.bat 2^>nul') do if not defined GRADLE_CMD set "GRADLE_CMD=%%G"
    if not defined GRADLE_CMD (
        echo [android-cpp] ERROR: Gradle was not found. Install Gradle 8.9 and set GRADLE_HOME, or use the "libs" target.
        exit /b 1
    )
)

pushd "%REPO_ROOT%" || exit /b 1
echo [android-cpp] Repository : %REPO_ROOT%
echo [android-cpp] SDK        : %ANDROID_HOME%
echo [android-cpp] NDK        : %ANDROID_NDK_HOME%
echo [android-cpp] CMake      : %CMAKE_EXE%
echo [android-cpp] vcpkg      : %VCPKG_ROOT%
echo [android-cpp] ABIs       : %FRUITY_ANDROID_ABIS%
echo [android-cpp] Target     : %TARGET%
echo.

set "BUILT_ANY=0"
for %%A in (%FRUITY_ANDROID_ABIS%) do (
    call :maybe_build_abi "%%A"
    if errorlevel 1 goto :build_error
)

if /i "%TARGET%"=="libs" goto :done

set "FRUITY_ANDROID_VERSION_NAME=0.0.0-local"
set "FRUITY_ANDROID_VERSION_CODE=1"
set "ANDROID_MIN_API=%FRUITY_ANDROID_MIN_API%"
echo [android-cpp] Assembling the APK with Gradle...
pushd "%REPO_ROOT%\android" || goto :build_error
call "%GRADLE_CMD%" --no-daemon --stacktrace :app:assembleRelease
if errorlevel 1 (
    popd
    goto :build_error
)
popd
set "APK=%REPO_ROOT%\android\app\build\outputs\apk\release\app-release.apk"
if not exist "%APK%" (
    echo [android-cpp] ERROR: Gradle finished but "%APK%" does not exist.
    goto :build_error
)
"%ANDROID_HOME%\build-tools\%ANDROID_BUILD_TOOLS%\apksigner.bat" verify --print-certs "%APK%"
if errorlevel 1 goto :build_error
copy /y "%APK%" "%OUT_ROOT%\FruityPrime.apk" >nul
echo.
echo [android-cpp] APK: %OUT_ROOT%\FruityPrime.apk

:done
echo.
echo [android-cpp] Android C++ build succeeded.
popd
exit /b 0

:maybe_build_abi
if /i "%ABI_FILTER%"=="all" goto :selected_abi
if /i "%ABI_FILTER%"=="%~1" goto :selected_abi
exit /b 0

:selected_abi
call :build_abi "%~1"
exit /b %ERRORLEVEL%

:build_abi
set "ABI=%~1"
set "TRIPLET="
if /i "%ABI%"=="arm64-v8a" set "TRIPLET=arm64-android"
if /i "%ABI%"=="x86_64" set "TRIPLET=x64-android"
if not defined TRIPLET (
    echo [android-cpp] ERROR: no vcpkg triplet is known for ABI %ABI%.
    exit /b 1
)
set "BUILD_DIR=%OUT_ROOT%\android-cpp-%ABI%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [android-cpp] Installing vcpkg libraries for %ABI% (%TRIPLET%)...
"%VCPKG_ROOT%\vcpkg.exe" install ^
    "curl:%TRIPLET%" ^
    "libarchive[core,bzip2,crypto,lz4,lzma,zstd]:%TRIPLET%" ^
    "zlib:%TRIPLET%" ^
    "freetype:%TRIPLET%" ^
    "libjpeg-turbo:%TRIPLET%" ^
    --clean-after-build
if errorlevel 1 exit /b 1

echo [android-cpp] Configuring %ABI%...
"%CMAKE_EXE%" -S "%REPO_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    "-DCMAKE_MAKE_PROGRAM=%NINJA_EXE%" ^
    "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
    "-DVCPKG_TARGET_TRIPLET=%TRIPLET%" ^
    "-DANDROID_ABI=%ABI%" ^
    "-DANDROID_PLATFORM=android-%FRUITY_ANDROID_MIN_API%" ^
    "-DANDROID_STL=c++_static" ^
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
if errorlevel 1 exit /b 1

echo [android-cpp] Building %ABI%...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --parallel %BUILD_JOBS% --target fruity_mphread_native_android
if errorlevel 1 exit /b 1
if not exist "%BUILD_DIR%\libFruityPrime.so" (
    echo [android-cpp] ERROR: "%BUILD_DIR%\libFruityPrime.so" was not produced.
    exit /b 1
)

if not exist "%REPO_ROOT%\android\app\src\main\jniLibs\%ABI%" mkdir "%REPO_ROOT%\android\app\src\main\jniLibs\%ABI%"
copy /y "%BUILD_DIR%\libFruityPrime.so" "%REPO_ROOT%\android\app\src\main\jniLibs\%ABI%\libFruityPrime.so" >nul
if errorlevel 1 exit /b 1
echo [android-cpp] %ABI% build succeeded.
exit /b 0

:contract_error
echo [android-cpp] ERROR: Could not read cmake\ExportAndroidConfig.cmake.
exit /b 1

:build_error
echo.
echo [android-cpp] Android C++ build failed.
popd
exit /b 1

:usage
echo Usage: tools\build\build-android-cpp.bat [all^|arm64-v8a^|x86_64] [apk^|libs]
echo        Default: both ABIs from cmake\FruityNativeConfig.cmake, then the APK
exit /b 0

:usage_error
echo [android-cpp] ERROR: Choose all or one ABI listed by cmake\FruityNativeConfig.cmake, then apk or libs.
call :usage
exit /b 2
