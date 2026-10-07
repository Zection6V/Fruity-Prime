param(
    [string]$Executable = 'tools/build/out/msvc-Release/FruityPrime.exe',
    [string]$OutputDirectory = 'tools/build/out/dialanche-live',
    [ValidateRange(1, 60)][int]$SpireSeconds = 16,
    [ValidateRange(1, 60)][int]$TargetSeconds = 18
)

# Real DedicatedServer / Welcome / intent / prediction / damage replay.
# TEST ARENA's existing recipe must be next to the executable in maps/arena.
$ErrorActionPreference = 'Stop'
$taskExe = (Resolve-Path -LiteralPath $Executable).Path
$taskGame = Split-Path $taskExe
$taskOut = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$probe = [Net.Sockets.UdpClient]::new(0)
$port = $probe.Client.LocalEndPoint.Port
$probe.Dispose()
$rotation = Join-Path $taskOut 'rotation.txt'
[IO.File]::WriteAllText($rotation, 'TEST ARENA | Battle | 30 | 1000')
$taskProcesses = @()
try {
    $generator = Start-Process $taskExe -ArgumentList @('-mapgen','"TEST ARENA"','-noupdate') -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $taskOut 'mapgen.log') -RedirectStandardError (Join-Path $taskOut 'mapgen.err')
    if ($generator.ExitCode -ne 0) { throw 'TEST ARENA generation failed' }
    $server = Start-Process $taskExe -ArgumentList @('-server','-port',$port,'-players','2','-rotation',('"' + $rotation + '"'),'-nomaster','-noautoupdate','-noupdate','-debuglog') -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOut 'server.log') -RedirectStandardError (Join-Path $taskOut 'server.err')
    $taskProcesses += $server
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        if ($server.HasExited) { throw 'Server exited before ready' }
        $ready = Select-String -LiteralPath (Join-Path $taskOut 'server.log') -SimpleMatch 'listening on UDP' -Quiet
        if (!$ready) { Start-Sleep -Milliseconds 100 }
    } until ($ready -or [DateTime]::UtcNow -ge $deadline)
    if (!$ready) { throw 'Server did not become ready' }
    $spire = Start-Process $taskExe -ArgumentList @('-netcheck','127.0.0.1','-port',$port,'-name','DialancheLiveSpire','-hunter','Spire','-hitrig','dialanche','-seconds',$SpireSeconds,'-noupdate','-debuglog') -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOut 'spire.log') -RedirectStandardError (Join-Path $taskOut 'spire.err')
    $null = $spire.Handle
    $taskProcesses += $spire
    Start-Sleep -Milliseconds 200
    $target = Start-Process $taskExe -ArgumentList @('-netcheck','127.0.0.1','-port',$port,'-name','DialancheLiveTarget','-hunter','Samus','-hitrig','dialanche','-seconds',$TargetSeconds,'-noupdate','-debuglog') -WorkingDirectory $taskGame -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskOut 'target.log') -RedirectStandardError (Join-Path $taskOut 'target.err')
    $null = $target.Handle
    $taskProcesses += $target
    foreach ($client in @($spire,$target)) {
        if (!$client.WaitForExit(45000)) { throw 'Client timed out' }
        if ($client.ExitCode -ne 0) { throw "Client failed: exit $($client.ExitCode)" }
    }
    foreach ($name in @('server','DialancheLiveSpire','DialancheLiveTarget')) {
        Copy-Item -LiteralPath (Join-Path $taskGame "netlog-$name.txt") -Destination (Join-Path $taskOut "netlog-$name.txt")
    }
    $authority = Get-Content -LiteralPath (Join-Path $taskOut 'netlog-server.txt')
    $eventPattern = '\[damage-(?:publish|replay)\] epoch=(\d+) match=(\d+) victim=([\d/]+) event=(\d+)'
    function Read-Events($lines) {
        @($lines | ForEach-Object {
            if ($_ -match $eventPattern) { "$($Matches[1])/$($Matches[2])/$($Matches[3])/$($Matches[4])" }
        })
    }
    $published = Read-Events $authority
    if ($published.Count -lt 2) { throw 'Insufficient production Dialanche hits' }
    foreach ($name in @('DialancheLiveSpire','DialancheLiveTarget')) {
        $replayed = Read-Events (Get-Content -LiteralPath (Join-Path $taskOut "netlog-$name.txt"))
        if ($replayed.Count -ne $published.Count -or (Compare-Object $published $replayed)) { throw "$name damage identities/count disagree" }
    }
    $hits = @($authority | Select-String 'stage=authority-hit weapon=-1 authorityFrame=(\d+) victim=(\d+) damage=(\d+)')
    if ($hits.Count -ne $published.Count) { throw 'Published damage includes an unexpected source' }
    $ticks = @{}
    foreach ($hit in $hits) {
        $groups = $hit.Matches[0].Groups
        $frame = [long]$groups[1].Value
        # NetFrame advances before gameplay; Scene.FrameCount after gameplay.
        if ($frame % 2 -ne 1 -or $groups[3].Value -ne '8') { throw 'Hit escaped native phase or base8 damage' }
        $key = "$($frame)/$($groups[2].Value)"
        if ($ticks.ContainsKey($key)) { throw "Repeated body damage on native tick $key" }
        $ticks[$key] = $true
    }
    $healthValues = @()
    foreach ($name in @('server','DialancheLiveSpire','DialancheLiveTarget')) {
        $states = @(Get-Content -LiteralPath (Join-Path $taskOut "netlog-$name.txt") | Select-String 'health slot=1 .*authorityHP=(\d+) entityHP=(\d+)')
        if (!$states) { throw "$name has no target health state" }
        $groups = $states[-1].Matches[0].Groups
        if ($groups[1].Value -ne $groups[2].Value) { throw "$name authority/entity HP disagree" }
        $healthValues += $groups[1].Value
    }
    if (@($healthValues | Select-Object -Unique).Count -ne 1) { throw 'Authority and both clients have different final HP' }
    Write-Output "DIALANCHE NETWORK PASS authority/clients events=$($published.Count) HP=$($healthValues[0]); native phase/base8/one body event per tick"
} finally {
    foreach ($process in $taskProcesses) { if (!$process.HasExited) { Stop-Process -Id $process.Id } }
}
