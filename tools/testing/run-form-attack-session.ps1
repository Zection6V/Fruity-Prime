param(
    [string]$Build = "$PSScriptRoot/../build/out/msvc-Release",
    [int]$Seconds = 70,
    [int]$Port = 28172,
    [int]$LagMs = 250,
    [ValidateSet('AI', 'Scripted')][string]$Driver = 'AI'
)
$ErrorActionPreference = 'Stop'
$buildPath = (Resolve-Path -LiteralPath $Build).Path
$exe = Join-Path $buildPath 'FruityPrime.exe'
$out = Join-Path $buildPath ('form-attacks/' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$rotation = Join-Path $out 'rotation.txt'
$room = if ($Driver -eq 'Scripted') { 'TEST ARENA' } else { 'MP1 SANCTORUS' }
Set-Content -LiteralPath $rotation -Value "$room | Battle | 9999 | 999" -Encoding ascii
$savedPhase = $env:MPHREAD_PHASE
$savedDebug = $env:FRUITY_DEBUG_LOG_DIRECTORY
$owned = @()
$result = 0
try {
    $probe = [System.Net.Sockets.UdpClient]::new($Port)
    $probe.Dispose()
    $env:MPHREAD_PHASE = if ($Driver -eq 'Scripted') { 'FormBombCycle' } else { 'Idle' }
    $env:FRUITY_DEBUG_LOG_DIRECTORY = $out
    $common = @('-noupdate', '-formtrace')
    $start = {
        param($Name, $Arguments)
        $process = Start-Process -FilePath $exe -ArgumentList $Arguments -WorkingDirectory $buildPath `
            -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $out "$Name.stdout.txt") `
            -RedirectStandardError (Join-Path $out "$Name.stderr.txt")
        $null = $process.Handle
        return $process
    }
    $owned += & $start 'server' (@('-server', '-port', $Port, '-players', 8, '-rotation', $rotation, '-nomaster') + $common)
    Start-Sleep -Seconds 2
    $hunters = @('Samus', 'Kanden', 'Spire', 'Noxus', 'Trace', 'Sylux', 'Weavel')
    for ($i = 0; $i -lt $hunters.Count; ++$i) {
        $clientArgs = @('-netcheck', '127.0.0.1', '-port', $Port, '-name', "FA$i$($hunters[$i])",
            '-hunter', $hunters[$i], '-headless', '-seconds', ($Seconds * 3 + 30), '-netlag', $LagMs) + $common
        if ($Driver -eq 'AI') { $clientArgs += @('-ai', 2) }
        $owned += & $start "AI$i" $clientArgs
        Start-Sleep -Milliseconds 400
    }
    $env:MPHREAD_PHASE = 'Idle'
    $observer = & $start 'observer' (@('-netcheck', '127.0.0.1', '-port', $Port, '-name', 'FormObserver',
        '-hunter', 'Kanden', '-seconds', $Seconds, '-shots', $out, '-size', '960x720', '-rhi', 'opengl') + $common)
    $owned += $observer
    Write-Output "Running seven AI peers + rendered observer: $out"
    $deadline = (Get-Date).AddSeconds($Seconds * 3 + 60)
    while (-not $observer.HasExited) {
        if ((Get-Date) -gt $deadline) { throw 'Rendered observer timed out.' }
        Start-Sleep -Milliseconds 500
    }
    $observer.Refresh()
    if ($observer.ExitCode -notin @(0, 1)) { throw "Observer exit=$($observer.ExitCode)" }
    $log = Join-Path $buildPath 'netlog-FormObserver.txt'
    Copy-Item -LiteralPath $log -Destination (Join-Path $out 'observer.netlog.txt')
    $draws = @(Get-Content -LiteralPath $log | Where-Object { $_ -match '\[form-draw\]' })
    $invalid = @($draws | Where-Object { $_ -match ' alt=0 ' })
    $seen = @($draws | ForEach-Object { if ($_ -match ' hunter=(\w+) ') { $Matches[1] } } | Sort-Object -Unique)
    $intentLog = Join-Path $buildPath 'netlog-server.txt'
    Copy-Item -LiteralPath $intentLog -Destination (Join-Path $out 'server.netlog.txt')
    $received = @(Get-Content -LiteralPath $intentLog | Where-Object { $_ -match 'morph press received' })
    $morphSlots = @($received | ForEach-Object { if ($_ -match 'slot (\d+) morph press') { [int]$Matches[1] } } | Sort-Object -Unique)
    # A held alt attack is one press and then a hold. An owner re-pressing on
    # consecutive frames is input being cleared between steps -- what kept a
    # headless AI client's Vhoscythe from ever finishing its startup.
    $repressStorms = @()
    for ($i = 0; $i -lt $hunters.Count; ++$i) {
        $ownerLog = Join-Path $buildPath "netlog-FA$i$($hunters[$i]).txt"
        if (-not (Test-Path -LiteralPath $ownerLog)) { continue }
        $run = 0; $worst = 0; $last = -2
        # The AI clients are still running and writing; read without locking them out.
        $stream = [System.IO.File]::Open($ownerLog, 'Open', 'Read', 'ReadWrite')
        $reader = [System.IO.StreamReader]::new($stream)
        $lines = try { $reader.ReadToEnd() -split "`n" } finally { $reader.Dispose() }
        foreach ($line in $lines) {
            if ($line -notmatch '\[altattack-observed\] slot=\d+ role=owner .*press=true held=true .*frame=(\d+)') { continue }
            $frame = [int]$Matches[1]
            $run = if ($frame -eq $last + 1) { $run + 1 } else { 1 }
            $last = $frame
            if ($run -gt $worst) { $worst = $run }
        }
        if ($worst -gt 3) { $repressStorms += "$($hunters[$i])=$worst" }
    }
    $images = @(Get-ChildItem -LiteralPath $out -Filter '*.png')
    $summary = [ordered]@{ Draws = $draws.Count; BipedAttacks = $invalid.Count; HuntersWithDrawnAttacks = $seen;
        ServerMorphPresses = $received.Count; ServerMorphSlots = $morphSlots; RepressStorms = $repressStorms;
        Captures = $images.Count; Output = $out }
    $summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'result.json')
    $summary | ConvertTo-Json | Write-Output
    if ($draws.Count -eq 0 -or $invalid.Count -gt 0 -or $received.Count -eq 0 -or $images.Count -eq 0) { $result = 1 }
    foreach ($storm in $repressStorms) { Write-Output "Held alt attack re-pressed on consecutive frames: $storm"; $result = 1 }
    foreach ($slot in 0..6) {
        if ($slot -notin $morphSlots) { Write-Output "Missing server morph input coverage: slot $slot"; $result = 1 }
    }
    # AI decisions depend on combat. The scripted arm additionally guarantees
    # each melee attack instead of mistaking missing coverage for a pass.
    $required = if ($Driver -eq 'Scripted') { @('Spire', 'Noxus', 'Trace', 'Weavel') } else { @('Trace') }
    foreach ($hunter in $required) {
        if ($hunter -notin $seen) { Write-Output "Missing rendered attack coverage: $hunter"; $result = 1 }
    }
} finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    }
    $env:MPHREAD_PHASE = $savedPhase
    $env:FRUITY_DEBUG_LOG_DIRECTORY = $savedDebug
}
exit $result
