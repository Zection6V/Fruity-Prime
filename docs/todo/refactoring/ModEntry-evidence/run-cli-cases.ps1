# ModEntry CLI contract capture (ME-00 baseline and every later stage).
#
# Runs one FruityPrime executable over a fixed list of command lines and
# writes, per case, stdout, stderr, the process exit code and a hash of any
# artifact the case names. Two runs (baseline and candidate) are compared with
# compare-cli-cases.ps1.
#
# Only cases that are local and harmless are listed: nothing here contacts a
# public service, installs anything, starts the launcher or the updater. The
# loopback server cases bind 127.0.0.1 only.
#
#   pwsh -File run-cli-cases.ps1 -Exe <FruityPrime.exe> -Label baseline
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Label,
    [string]$Only = '',
    # 'cli' (default): the cases above. 'gpu': commands that open a window or
    # render a room, for a machine with a GPU and the game files.
    [string]$Set = 'cli'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $root "runs\$Label"
$sandbox = Join-Path $root "runs\$Label-sandbox"
foreach ($d in @($outDir, $sandbox)) {
    if (Test-Path $d) { Remove-Item -Recurse -Force $d }
    New-Item -ItemType Directory -Force $d | Out-Null
}
New-Item -ItemType Directory -Force (Join-Path $sandbox 'emptymaps') | Out-Null

# name | timeout seconds | arguments | artifact (relative to sandbox, optional)
$cases = @(
    # --- setup-before: TryHandleHeadless ---------------------------------
    @{ N = 'D01-rhicontract';            T = 60;  A = @('-rhicontract', 'contract.txt');                    F = 'contract.txt' }
    @{ N = 'D01b-rhicontract-smoketest'; T = 60;  A = @('-rhicontract', 'contract2.txt', '-smoketest');   F = 'contract2.txt' }
    @{ N = 'D01c-rhicontract-badpath';   T = 60;  A = @('-rhicontract', 'no\such\dir\c.txt') }
    @{ N = 'D02-smoketest-mechanics';    T = 120; A = @('-smoketest', '-mechanics') }
    @{ N = 'D03-pointer-frametiming';    T = 120; A = @('-pointercheck', '-frametimingcheck') }
    @{ N = 'D04-frametiming-badsize';    T = 120; A = @('-frametimingcheck', '-size', 'broken') }
    @{ N = 'D05-credits-update';         T = 60;  A = @('-credits', '-update') }
    @{ N = 'D06-mapbundle-update';       T = 60;  A = @('-mapdir', 'emptymaps', '-mapbundle', '-update') }
    @{ N = 'D10-nointerp-relayed';       T = 60;  A = @('-nointerp', '-relayedpuppets', '-credits') }
    @{ N = 'D12-nativeaim-bogus';        T = 60;  A = @('-nativeaim', 'bogus', '-credits') }
    @{ N = 'D13-netlag-invalid';         T = 60;  A = @('-netlag', 'INVALID', '-credits') }
    @{ N = 'D13b-netspike-invalid';      T = 60;  A = @('-netspike', 'x', '-credits') }
    @{ N = 'D13c-netloss-invalid';       T = 60;  A = @('-netloss', 'x', '-credits') }
    @{ N = 'D13d-netjitter-invalid';     T = 60;  A = @('-netjitter', 'x', '-credits') }
    @{ N = 'D13e-netlag-ok';             T = 60;  A = @('-netlag', '200:40', '-netloss', '5', '-credits') }
    @{ N = 'NET-all-options';            T = 60;  A = @('-homingtrace', '-nounlagged', '-snapshotpuppets', '-clientpin',
                                                       '-pressage', '-hitrig', 'sniper', '-rigmorph', '-rigspeed', '2',
                                                       '-rigrange', '20', '-rigstrafe', '60', '-maxrewind', '30',
                                                       '-nohitprediction', '-nohitmarker', '-servershots',
                                                       '-noobservedimpacts', '-noconfirmedimpacts', '-noretarget',
                                                       '-nodeathprediction', '-noclaims', '-nointerp', '-relayedpuppets',
                                                       '-credits') }
    @{ N = 'NET-refusals';               T = 60;  A = @('-hitrig', 'bogus', '-rigspeed', '9', '-rigrange', '1',
                                                       '-rigstrafe', '1', '-maxrewind', '99999', '-credits') }
    @{ N = 'A12-doubledash-credits';     T = 60;  A = @('--', '--CREDITS') }
    @{ N = 'D11-render-overrides';       T = 60;  A = @('-fpscap', 'display', '-vsync', 'off', '-rhi', 'opengl', '-credits') }
    @{ N = 'D15-classicframes';          T = 180; A = @('-classicframes', 'classic', 'bench:1:1');           F = 'classic' }
    @{ N = 'D16-gamepad';                T = 60;  A = @('-gamepad', '-seconds', '1') }
    @{ N = 'UI-uidesign';                T = 60;  A = @('-uidesign', 'uidesign') }
    @{ N = 'UI-uibench-refused';         T = 60;  A = @('-uibench', '-uibenchslow') }
    @{ N = 'UI-uibench-size-refused';    T = 60;  A = @('-uibench', '-uibenchsize', '10x10') }
    @{ N = 'UI-tapcheck';                T = 120; A = @('-tapcheck') }
    @{ N = 'SRV-servers-loopback';       T = 60;  A = @('-servers', '-master', '127.0.0.1', '-masterport', '1') }
    @{ N = 'D09-servers-hosts';          T = 60;  A = @('-servers', '-hosts', '-master', '127.0.0.1', '-masterport', '1') }
    @{ N = 'SRV-hosts-loopback';         T = 60;  A = @('-hosts', '-master', '127.0.0.1', '-masterport', '1') }
    # --- setup-after: TryHandle -------------------------------------------
    @{ N = 'POST-mechanics';             T = 120; A = @('-mechanics') }
    @{ N = 'POST-rooms';                 T = 120; A = @('-rooms') }
    @{ N = 'POST-q3maps-missing';        T = 60;  A = @('-q3maps', 'missing.pk3') }
    @{ N = 'POST-q3shaders-missing';     T = 60;  A = @('-q3shaders', 'missing.pk3') }
    @{ N = 'POST-mapcheck-missing';      T = 60;  A = @('-mapcheck', 'NO SUCH ROOM') }
    @{ N = 'POST-mapitems-missing';      T = 60;  A = @('-mapitems', 'NO SUCH ROOM') }
    @{ N = 'POST-mapmaterials';          T = 120; A = @('-mapmaterials', 'MP3 PROVING GROUND') }
    @{ N = 'POST-altprobe-noat';         T = 60;  A = @('-altprobe', 'MP3 PROVING GROUND') }
    @{ N = 'D20a-shotat-commasize';      T = 60;  A = @('-shotat', 'MP3 PROVING GROUND', '-size', '960,540') }
    @{ N = 'D19a-mapgen-none';           T = 60;  A = @('-mapdir', 'emptymaps', '-mapgen', 'NOPE') }
    @{ N = 'POST-demoinfo-missing';      T = 60;  A = @('-demoinfo', 'missing.fpdemo') }
    @{ N = 'POST-dpstest';               T = 180; A = @('-dpstest', 'TEST ARENA', '-seconds', '1') }
    @{ N = 'POST-simcheck';              T = 180; A = @('-simcheck', 'TEST ARENA', '-players', '2', '-seconds', '1') }
    @{ N = 'D18-movieheadlesscheck';     T = 300; A = @('-movieheadlesscheck') }
    @{ N = 'D17-netcombat-precedence';   T = 600; A = @('-netcombatcheck', 'TEST ARENA') }
    @{ N = 'POST-hostgame-loopback';     T = 120; A = @('-hostgame', 'TEST ARENA', '-master', '127.0.0.1', '-masterport', '1', '-name', 'x') }
    @{ N = 'D23-unknown';                T = 60;  A = @('-zzzunknown') }
)

$gpuCases = @(
    @{ N = 'G-maptest';     T = 300; A = @('-maptest', 'TEST ARENA', '-players', '2', '-seconds', '3', '-size', '640x360') }
    @{ N = 'G-maptest-bots'; T = 300; A = @('-maptest', 'TEST ARENA', '-players', '4', '-seconds', '3', '-bots') }
    @{ N = 'G-shotat';      T = 300; A = @('-shotat', 'MP3 PROVING GROUND', '-at', '0,8,0', '-look', '10,8,10', '-out', "$sandbox\shot.png", '-size', '320,180'); F = 'shot.png' }
    @{ N = 'G-gpulifetime'; T = 300; A = @('-gpulifetime', 'TEST ARENA', '-cycles', '1', '-frames', '10') }
)
if ($Set -eq 'gpu') { $cases = $gpuCases }

# Cases the instructions class as manual: they open windows, start the
# launcher or write into the build's own thumbnail cache (backed up and put
# back around the run). Nothing here reaches the network: -update on a local
# build stops before any request, -hostlocal is told -nomaster.
$manualCases = @(
    @{ N = 'M-D14-update-noupdate';  T = 60;  A = @('-update', '-noupdate') }
    @{ N = 'M-D07-launcher-server';  T = 15;  A = @('-launcher', '-server', '-port', '27996', '-nomaster', '-noupdate') }
    @{ N = 'M-vulkancheck';          T = 120; A = @('-vulkancheck', '-noupdate') }
    @{ N = 'M-vulkanresourcecheck';  T = 120; A = @('-vulkanresourcecheck', '-noupdate') }
    @{ N = 'M-vulkanpresentcheck';   T = 180; A = @('-vulkanpresentcheck', '-noupdate') }
    @{ N = 'M-vulkanpresentfallback'; T = 180; A = @('-vulkanpresentfallbackcheck', '-noupdate') }
    @{ N = 'M-reflexcheck';          T = 180; A = @('-reflexcheck', '-noupdate') }
    @{ N = 'M-presentconformance';   T = 180; A = @('-presentconformance', '-noupdate') }
    @{ N = 'M-rhiconformance';       T = 180; A = @('-rhiconformance', '-noupdate') }
    @{ N = 'M-glfwpathcheck';        T = 120; A = @('-glfwpathcheck', '-noupdate') }
    @{ N = 'M-thumbnailwindowcheck'; T = 180; A = @('-thumbnailwindowcheck', '-noupdate') }
    @{ N = 'M-windowcheck';          T = 120; A = @('-windowcheck', '-noupdate') }
    @{ N = 'M-gamepadcheck';         T = 120; A = @('-gamepadcheck', '-noupdate') }
    @{ N = 'M-backdropparity';       T = 300; A = @('-backdropparity', 'backdrop', '-noupdate'); F = 'backdrop' }
    @{ N = 'M-goldencapture-hud';    T = 300; A = @('-goldencapture', 'hud', '-goldendir', "$sandbox\golden", '-noupdate'); F = 'golden' }
    @{ N = 'M-shellshot';            T = 600; A = @('-shellshot', "$sandbox\shell", '-noupdate'); F = 'shell' }
    @{ N = 'M-hostlocal';            T = 120; A = @('-hostlocal', 'TEST ARENA', '-nomaster', '-seconds', '3', '-noupdate') }
    @{ N = 'M-D22-thumbnail-twice';  T = 300; A = @('-thumbnail', 'TEST ARENA', '-thumbnail', 'MP3 PROVING GROUND', '-size', '320x240', '-noupdate') }
)
$thumbCache = Join-Path (Split-Path -Parent $Exe) 'thumbnails'
$thumbBackup = Join-Path $sandbox 'thumbnails-backup'
if ($Set -eq 'manual') {
    $cases = $manualCases
    if (Test-Path $thumbCache) { Copy-Item -Recurse $thumbCache $thumbBackup }
}

$summary = @()
foreach ($c in $cases) {
    if ($Only -and $c.N -notlike $Only) { continue }
    $stdout = Join-Path $outDir "$($c.N).out.txt"
    $stderr = Join-Path $outDir "$($c.N).err.txt"
    $argLine = ($c.A | ForEach-Object { if ($_ -match '[\s"]' -or $_ -eq '') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join ' '
    # A process that dies with an access violation (0xC0000005) while it is
    # already exiting is a pre-existing fault of the baseline build too (about
    # one run in ten on the fast setup-after commands; see the record). It is
    # retried and counted rather than compared, so a real difference is not
    # hidden behind it and it does not hide one either.
    $crashRetries = 0
    for (;;) {
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $p = Start-Process -FilePath $Exe -ArgumentList $argLine -WorkingDirectory $sandbox `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr -NoNewWindow -PassThru
        $handle = $p.Handle  # keeps ExitCode readable after exit
        $code = $null
        if (-not $p.WaitForExit($c.T * 1000)) {
            try { $p.Kill($true) } catch {}
            $code = 'TIMEOUT'
        } else {
            $p.WaitForExit()
            $code = $p.ExitCode
        }
        $watch.Stop()
        if ($code -ne -1073741819 -or $crashRetries -ge 5) { break }
        $crashRetries++
    }
    $artifact = ''
    if ($c.F) {
        $path = Join-Path $sandbox $c.F
        if (Test-Path $path -PathType Leaf) {
            $artifact = (Get-FileHash -Algorithm SHA256 $path).Hash
        } elseif (Test-Path $path -PathType Container) {
            $artifact = 'files=' + @(Get-ChildItem -Recurse -File $path).Count
        } else {
            $artifact = 'missing'
        }
    }
    Set-Content -Encoding utf8 (Join-Path $outDir "$($c.N).code.txt") "$code"
    $summary += [pscustomobject]@{ Case = $c.N; Exit = $code; Seconds = [math]::Round($watch.Elapsed.TotalSeconds, 1); Artifact = $artifact; ExitCrashRetries = $crashRetries; Args = $argLine }
    Write-Host ("{0,-28} exit={1,-8} {2,6}s {3} {4}" -f $c.N, $code, [math]::Round($watch.Elapsed.TotalSeconds, 1), $artifact, $(if ($crashRetries) { "(exit-crash retries: $crashRetries)" } else { '' }))
}
$summary | Export-Csv -NoTypeInformation -Encoding utf8 (Join-Path $outDir 'summary.csv')
if ($Set -eq 'manual' -and (Test-Path $thumbBackup)) {
    Remove-Item -Recurse -Force $thumbCache
    Copy-Item -Recurse $thumbBackup $thumbCache
    Write-Host "thumbnail cache restored"
}
