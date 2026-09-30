@echo off
setlocal EnableExtensions

rem Build the C# Android APK from any working directory.
rem Usage: tools\build\build-android-cs.bat [Debug|Release]

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"

if /i "%CONFIG%"=="help" goto :usage
if /i "%CONFIG%"=="-h" goto :usage
if /i "%CONFIG%"=="--help" goto :usage
if /i "%CONFIG%"=="Debug" goto :config_valid
if /i "%CONFIG%"=="Release" goto :config_valid
goto :usage_error
:config_valid
if not "%~2"=="" goto :usage_error

if not defined ANDROID_HOME if defined ANDROID_SDK_ROOT set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
if not defined ANDROID_HOME set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
if not defined ANDROID_SDK_ROOT set "ANDROID_SDK_ROOT=%ANDROID_HOME%"

if not defined JAVA_HOME (
    for /d %%J in ("%ProgramFiles%\Microsoft\jdk-17*") do (
        if not defined JAVA_HOME if exist "%%~fJ\bin\javac.exe" set "JAVA_HOME=%%~fJ"
    )
)

if not exist "%REPO_ROOT%\src\MphRead.Android\MphRead.Android.csproj" (
    echo [android-cs] ERROR: Android project not found under "%REPO_ROOT%".
    exit /b 1
)
if not exist "%ANDROID_HOME%\platforms\android-36\android.jar" (
    echo [android-cs] ERROR: Android API 36 was not found under "%ANDROID_HOME%".
    echo [android-cs]        Install it with sdkmanager or the .NET InstallAndroidDependencies target.
    exit /b 1
)
if not exist "%ANDROID_HOME%\build-tools\36.0.0\aapt.exe" (
    echo [android-cs] ERROR: Android Build Tools 36.0.0 were not found under "%ANDROID_HOME%".
    exit /b 1
)
if not exist "%JAVA_HOME%\bin\javac.exe" (
    echo [android-cs] ERROR: A JDK was not found. Set JAVA_HOME to JDK 17.
    exit /b 1
)
where dotnet >nul 2>nul
if errorlevel 1 (
    echo [android-cs] ERROR: dotnet was not found on PATH.
    exit /b 1
)

pushd "%REPO_ROOT%" || exit /b 1
echo [android-cs] Repository : %REPO_ROOT%
echo [android-cs] Configuration: %CONFIG%
echo [android-cs] SDK        : %ANDROID_HOME%
echo [android-cs] JDK        : %JAVA_HOME%
echo.

rem EmbedAssembliesIntoApk makes the Debug APK installable without fast deployment.
dotnet build "src\MphRead.Android\MphRead.Android.csproj" -c "%CONFIG%" ^
    -p:AndroidPackageFormat=apk ^
    -p:EmbedAssembliesIntoApk=true ^
    "-p:AndroidSdkDirectory=%ANDROID_HOME%" ^
    "-p:JavaSdkDirectory=%JAVA_HOME%"
set "BUILD_RC=%ERRORLEVEL%"

if "%BUILD_RC%"=="0" (
    echo.
    echo [android-cs] Build succeeded.
    echo [android-cs] APK: src\MphRead.Android\bin\%CONFIG%\net10.0-android36.0\fr.livetek.fruityprime-Signed.apk
) else (
    echo.
    echo [android-cs] Build failed with exit code %BUILD_RC%.
)

popd
exit /b %BUILD_RC%

:usage
echo Usage: tools\build\build-android-cs.bat [Debug^|Release]
echo        Default configuration: Debug
exit /b 0

:usage_error
echo [android-cs] ERROR: Configuration must be Debug or Release.
call :usage
exit /b 2
