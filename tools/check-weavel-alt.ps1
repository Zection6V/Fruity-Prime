param(
    [string]$Executable = 'tools/build/out/msvc-Release/FruityPrime.exe',
    [string]$Room = 'MP3 PROVING GROUND',
    [string]$OutputDirectory = 'tools/build/out/weavel-validation'
)

$ErrorActionPreference = 'Stop'
$taskExe = (Resolve-Path -LiteralPath $Executable).Path
$taskGame = Split-Path $taskExe
$taskOut = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$assetLog = Join-Path $taskOut 'asset.log'
$assetCheck = Start-Process -FilePath $taskExe -ArgumentList @('-weavelaltcheck', ('"' + $Room + '"'), '-noupdate') `
    -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput $assetLog -RedirectStandardError (Join-Path $taskOut 'asset.err')
if ($assetCheck.ExitCode -ne 0) {
    Get-Content -LiteralPath $assetLog -Tail 12
    $first = Get-Content -LiteralPath $assetLog -First 1
    if ($first -match 'standard error is being written to (.+)$') { Get-Content -LiteralPath $Matches[1] -Tail 8 }
    throw "Weavel asset check exited $($assetCheck.ExitCode)"
}
Get-Content -LiteralPath $assetLog | Select-String 'WEAVEL PASS (\d+ production checks|old16 refused)'
