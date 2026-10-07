param(
    [string]$Executable = 'tools/build/out/msvc-Release/FruityPrime.exe',
    [string]$Room = 'AD2 ALINOS PERCH',
    [string]$OutputDirectory = 'tools/build/out/dialanche-validation'
)

# Asset-backed production collision plus two independent UDP snapshot clients.
$ErrorActionPreference = 'Stop'
$taskExe = (Resolve-Path -LiteralPath $Executable).Path
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskProcesses = @()
$taskPorts = @()
try {
    foreach ($index in 0..1) {
        $probe = [Net.Sockets.UdpClient]::new(0)
        $taskPorts += $probe.Client.LocalEndPoint.Port
        $probe.Dispose()
        $taskProcesses += Start-Process -FilePath $taskExe -ArgumentList @(
            '-dialanchepeercheck', ('"' + $Room + '"'), '-port', $taskPorts[$index], '-noupdate'
        ) -WorkingDirectory (Split-Path $taskExe) -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $taskOutput "peer-$index.log") `
            -RedirectStandardError (Join-Path $taskOutput "peer-$index.err")
        $null = $taskProcesses[-1].Handle
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        $ready = $true
        foreach ($index in 0..1) {
            if ($taskProcesses[$index].HasExited) { throw "Peer $index exited before ready" }
            $ready = $ready -and (Select-String -LiteralPath (Join-Path $taskOutput "peer-$index.log") -SimpleMatch 'DIALANCHE PEER ready UDP' -Quiet)
        }
        if (!$ready) { Start-Sleep -Milliseconds 100 }
    } until ($ready -or [DateTime]::UtcNow -ge $deadline)
    if (!$ready) { throw 'UDP peers did not become ready' }
    $authority = Start-Process -FilePath $taskExe -ArgumentList @(
        '-dialanchecheck', ('"' + $Room + '"'), '-dialanchepeera', $taskPorts[0], '-dialanchepeerb', $taskPorts[1], '-noupdate'
    ) -WorkingDirectory (Split-Path $taskExe) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $taskOutput 'authority.log') `
        -RedirectStandardError (Join-Path $taskOutput 'authority.err')
    $taskProcesses += $authority
    $null = $authority.Handle
    foreach ($process in $taskProcesses) {
        if (!$process.WaitForExit(45000)) { throw "Process $($process.Id) timed out" }
        if ($process.ExitCode -ne 0) { throw "Process $($process.Id) failed (exit $($process.ExitCode)); logs: $taskOutput" }
    }
    foreach ($index in 0..1) {
        $line = Select-String -LiteralPath (Join-Path $taskOutput "peer-$index.log") -Pattern '^DIALANCHE PEER PASS HP=84 events=2 snapshots=3$'
        if (!$line) { throw "Peer $index did not confirm HP/event parity" }
        Write-Output "Peer $index`: $($line.Line)"
    }
    Get-Content -LiteralPath (Join-Path $taskOutput 'authority.log') | Where-Object { $_ -match '^DIALANCHE PASS' }
} finally {
    foreach ($process in $taskProcesses) { if (!$process.HasExited) { Stop-Process -Id $process.Id } }
}
