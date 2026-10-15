$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../../../../')).Path
$build = Join-Path $repo 'tools/build/out/msvc-Release'
$sandbox = Join-Path $build 'modentry-exit-launcher-fix'
if (Test-Path -LiteralPath $sandbox) { throw "Sandbox already exists: $sandbox" }
New-Item -ItemType Directory -Path $sandbox | Out-Null
foreach ($file in Get-ChildItem -LiteralPath $build -File) {
    if ($file.Extension -eq '.dll' -or $file.Name -in @('FruityPrime.exe','settings.json','settings.txt','launcher.txt','qt.conf')) {
        Copy-Item -LiteralPath $file.FullName -Destination $sandbox
    }
}
foreach ($dir in @('Assets','qml','platforms','imageformats','iconengines','generic','networkinformation','tls')) {
    if (Test-Path -LiteralPath (Join-Path $build $dir)) {
        New-Item -ItemType Junction -Path (Join-Path $sandbox $dir) -Target (Join-Path $build $dir) | Out-Null
    }
}
$sourceData = Join-Path $repo 'native/build-mingw-direct/files/AMHP1'
$data = Join-Path $sandbox 'data'
New-Item -ItemType Directory -Path $data | Out-Null
foreach ($dir in Get-ChildItem -LiteralPath $sourceData -Directory) {
    if ($dir.Name -notin @('_archives','levels')) {
        New-Item -ItemType Junction -Path (Join-Path $data $dir.Name) -Target $dir.FullName | Out-Null
    }
}
$archives = New-Item -ItemType Directory -Path (Join-Path $data '_archives')
foreach ($item in Get-ChildItem -LiteralPath (Join-Path $sourceData '_archives')) {
    $kind = if ($item.PSIsContainer) { 'Junction' } else { 'HardLink' }
    New-Item -ItemType $kind -Path (Join-Path $archives.FullName $item.Name) -Target $item.FullName | Out-Null
}
New-Item -ItemType Directory -Path (Join-Path $data 'levels') | Out-Null
foreach ($dir in Get-ChildItem -LiteralPath (Join-Path $sourceData 'levels') -Directory) {
    if ($dir.Name -in @('entities','nodeData')) {
        $target = New-Item -ItemType Directory -Path (Join-Path $data "levels/$($dir.Name)")
        Copy-Item -Path (Join-Path $dir.FullName '*') -Destination $target.FullName -Recurse
    } else {
        New-Item -ItemType Junction -Path (Join-Path $data "levels/$($dir.Name)") -Target $dir.FullName | Out-Null
    }
}
$paths = Get-Content -LiteralPath (Join-Path $build 'paths.txt')
$paths -replace '^AMHP1=.*$', "AMHP1=$($data.Replace('\','/'))" | Set-Content -LiteralPath (Join-Path $sandbox 'paths.txt') -Encoding utf8
New-Item -ItemType Directory -Path (Join-Path $sandbox 'maps') | Out-Null
Write-Output $sandbox
