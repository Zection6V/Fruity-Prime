<# Production UDP form/bomb cycles with a dedicated authority and real clients. #>
param(
    [int]$Clients = 2,
    [int]$Bots = 0,
    [string[]]$Hunters = @('Samus', 'Weavel'),
    [int]$Seconds = 24,
    [int]$LagMs = 0,
    [int]$JitterMs = 0,
    [double]$LossPercent = 0,
    [double]$ReorderPercent = 0,
    [int]$Port = 28040,
    [switch]$NoInterpolation,
    [switch]$SnapshotOnly,
    [switch]$SpectateRejoin,
    [switch]$RecordDemo,
    [string]$Build = "$PSScriptRoot/../build/out/msvc-Release"
)
$ErrorActionPreference = 'Stop'
if ($Clients -lt 2 -or $Clients -gt 8) { throw 'Clients must be between 2 and 8.' }
if ($Bots -lt 0 -or $Bots + $Clients -gt 8) { throw 'Clients and bots must fit in eight slots.' }
$Hunters = @($Hunters | ForEach-Object { $_ -split ',' })
$buildPath = (Resolve-Path $Build).Path
$exe = Join-Path $buildPath 'FruityPrime.exe'
$out = Join-Path $buildPath ("form-transition/" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$rotation = Join-Path $out 'rotation.txt'
Set-Content -LiteralPath $rotation -Value 'TEST ARENA | Battle | 9999 | 999' -Encoding ascii
$oldPhase = $env:MPHREAD_PHASE
$oldDebugDirectory = $env:FRUITY_DEBUG_LOG_DIRECTORY
$processes = @()
$names = @()
$server = $null
$code = 0
try {
    $env:MPHREAD_PHASE = 'FormBombCycle'
    $env:FRUITY_DEBUG_LOG_DIRECTORY = $out
    $serverArgs = @('-server', '-port', $Port, '-players', 8, '-rotation', $rotation, '-nomaster', '-noupdate', '-formtrace')
    if ($Bots -gt 0) { $serverArgs += @('-bots', $Bots) }
    $server = Start-Process $exe -ArgumentList $serverArgs -WorkingDirectory $buildPath -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $out 'server.stdout.txt') -RedirectStandardError (Join-Path $out 'server.stderr.txt')
    Start-Sleep -Seconds 3
    if ($server.HasExited) { throw "Server exited: $($server.ExitCode)" }
    for ($i = 0; $i -lt $Clients; ++$i) {
        $hunter = $Hunters[$i % $Hunters.Count]
        $name = "F$i$hunter"
        $names += $name
        $clientArgs = @('-netcheck', '127.0.0.1', '-port', $Port, '-name', $name,
            '-hunter', $hunter, '-headless', '-seconds', $Seconds, '-noupdate', '-formtrace')
        if ($LagMs -gt 0) { $clientArgs += @('-netlag', "${LagMs}:${JitterMs}") }
        if ($LossPercent -gt 0) { $clientArgs += @('-netloss', "$LossPercent%") }
        if ($ReorderPercent -gt 0) { $clientArgs += @('-netreorder', "$ReorderPercent%") }
        if ($NoInterpolation) { $clientArgs += '-nointerp' }
        if ($SnapshotOnly) { $clientArgs += '-noformevents' }
        # Demo filenames have second precision and share the export directory.
        if ($RecordDemo -and $i -eq 0) { $clientArgs += '-recorddemo' }
        if ($SpectateRejoin -and $i -eq 1) { $clientArgs += @('-spectate', 7, '-rejoin', 12) }
        $process = Start-Process $exe -ArgumentList $clientArgs -WorkingDirectory $buildPath -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $out "client$i.stdout.txt") -RedirectStandardError (Join-Path $out "client$i.stderr.txt")
        $null = $process.Handle
        $processes += $process
        Start-Sleep -Milliseconds 400
    }
    $deadline = (Get-Date).AddSeconds($Seconds + 90)
    while (@($processes | Where-Object { -not $_.HasExited }).Count -gt 0) {
        if ((Get-Date) -ge $deadline) { throw 'Clients did not finish before the deadline.' }
        Start-Sleep -Milliseconds 500
    }
    for ($i = 0; $i -lt $processes.Count; ++$i) {
        $processes[$i].Refresh()
        $output = Get-Content -LiteralPath (Join-Path $out "client$i.stdout.txt") -Raw
        # This phase deliberately stands still and does not shoot other peers.
        $unexpected = @($output -split "`n" | Where-Object {
            $_ -match 'FAIL:' -and $_ -notmatch 'never took a single hit|no other player was on the map and moving'
        })
        if ($processes[$i].ExitCode -notin @(0, 1) -or $output -notmatch 'authority=False' -or
            $output -notmatch 'RESULT:' -or $unexpected.Count -gt 0) {
            $code = 1
            Write-Output "Client $i failed (exit=$($processes[$i].ExitCode)). See $out/client$i.stdout.txt"
        }
    }
} finally {
    foreach ($owned in @($processes) + @($server)) {
        if ($null -ne $owned -and -not $owned.HasExited) { Stop-Process -Id $owned.Id -Force }
    }
    $env:MPHREAD_PHASE = $oldPhase
    $env:FRUITY_DEBUG_LOG_DIRECTORY = $oldDebugDirectory
}
Copy-Item -LiteralPath (Join-Path $buildPath 'netlog-server.txt') -Destination (Join-Path $out 'server.netlog.txt')
$clientLogs = @()
for ($i = 0; $i -lt $names.Count; ++$i) {
    $copy = Join-Path $out "client$i.netlog.txt"
    Copy-Item -LiteralPath (Join-Path $buildPath "netlog-$($names[$i]).txt") -Destination $copy
    $clientLogs += $copy
}
$options = @()
if ($NoInterpolation -or $SnapshotOnly) { $options += '--allow-no-samples' }
python -I "$PSScriptRoot/compare-form-transition-trace.py" (Join-Path $out 'server.netlog.txt') @clientLogs @options --json (Join-Path $out 'form-result.json')
if ($LASTEXITCODE -ne 0) { $code = 1 }
Write-Output "output: $out"
exit $code
