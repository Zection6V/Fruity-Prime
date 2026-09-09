[CmdletBinding()]
param(
    [string] $RomPath = '',
    [string] $BuildDirectory = 'src/build-mingw',
    [string] $SourceDirectory = 'src',
    [switch] $SkipTests
)

$nativeMingwRoot = 'C:\mingw64'
$nativeMingwBin = Join-Path $nativeMingwRoot 'bin'
$nativeCompiler = Join-Path $nativeMingwBin 'g++.exe'
$nativeMake = Join-Path $nativeMingwBin 'mingw32-make.exe'
$nativeCmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$nativeCmakeCandidates = @(
    'C:\msys64\mingw64\bin\cmake.exe'
    'C:\Users\Admin\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe'
    if ($nativeCmakeCommand) { $nativeCmakeCommand.Path }
    'C:\Program Files\CMake\bin\cmake.exe'
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
$nativeCmakeCandidates = @($nativeCmakeCandidates | Select-Object -Unique)

if (-not (Test-Path -LiteralPath $nativeCompiler)) {
    throw "MinGW compiler was not found: $nativeCompiler"
}
if (-not (Test-Path -LiteralPath $nativeMake)) {
    throw "MinGW make was not found: $nativeMake"
}
function Test-MingwMakefilesGenerator([string] $CmakePath) {
    $help = (& $CmakePath --help 2>$null | Out-String)
    return $help -match 'MinGW Makefiles'
}
if ($RomPath -and -not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "NDS ROM was not found: $RomPath"
}
if (-not (Test-Path -LiteralPath (Join-Path $SourceDirectory 'CMakeLists.txt') -PathType Leaf)) {
    throw "C++ source directory was not found: $SourceDirectory"
}

$nativeCmake = $nativeCmakeCandidates |
    Where-Object { Test-MingwMakefilesGenerator $_ } |
    Select-Object -First 1

if (-not $nativeCmake) {
    Write-Host 'No CMake with the MinGW Makefiles generator was found.'
    Write-Host 'Falling back to the verified standalone MinGW Makefile.'
    $env:PATH = "$nativeMingwBin;$env:PATH"
    if ($RomPath) {
        $env:FRUITY_PRIME_TEST_NDS = $RomPath
    }
    $nativeGoals = if ($SkipTests) { @('server', 'game') } else { @('all') }
    & $nativeMake -C native @nativeGoals --jobs=4
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    if (-not $SkipTests) {
        & $nativeMake -C native test --jobs=4
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    exit 0
}

$env:PATH = "$nativeMingwBin;$env:PATH"
$nativeConfigureArguments = @(
    '-S', $SourceDirectory,
    '-B', $BuildDirectory,
    '-G', 'MinGW Makefiles',
    "-DCMAKE_CXX_COMPILER=$nativeCompiler",
    "-DCMAKE_MAKE_PROGRAM=$nativeMake",
    '-DCMAKE_BUILD_TYPE=Release'
)
if ($RomPath) {
    $nativeConfigureArguments += "-DFRUITY_PRIME_TEST_NDS=$RomPath"
}
$nativeZlibInclude = Join-Path $nativeMingwRoot 'x86_64-w64-mingw32\include'
$nativeZlibLibrary = Join-Path $nativeMingwRoot 'x86_64-w64-mingw32\lib\libz.a'
if ((Test-Path -LiteralPath (Join-Path $nativeZlibInclude 'zlib.h')) -and (Test-Path -LiteralPath $nativeZlibLibrary)) {
    $nativeZlibIncludeArgument = $nativeZlibInclude -replace '\\', '/'
    $nativeZlibLibraryArgument = $nativeZlibLibrary -replace '\\', '/'
    $nativeConfigureArguments += "-DZLIB_INCLUDE_DIR=$nativeZlibIncludeArgument"
    $nativeConfigureArguments += "-DZLIB_LIBRARY=$nativeZlibLibraryArgument"
}

Write-Host "CMake: $nativeCmake"
Write-Host "C++ compiler: $nativeCompiler"
& $nativeCmake @nativeConfigureArguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $nativeCmake --build $BuildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipTests) {
    $nativeCtest = Join-Path (Split-Path -Parent $nativeCmake) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $nativeCtest)) {
        $nativeCtest = 'ctest.exe'
    }
    & $nativeCtest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
