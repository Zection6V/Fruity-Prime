# One ModEntry stage gate: the CLI contract against the baseline, the flag
# literal index against HEAD, and the full CTest suite of the current build.
#
#   pwsh -File verify-stage.ps1 -Label me02
param([Parameter(Mandatory = $true)][string]$Label, [switch]$Server)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path (Join-Path $here '..\..\..\..')).Path
$out = Join-Path $repo 'tools\build\out\msvc-Release'
$exe = Join-Path $out "FruityPrime-$Label.exe"
Copy-Item (Join-Path $out 'FruityPrime.exe') $exe -Force

$log = Join-Path $here "gate-$Label.log"
"== $Label  $(Get-Date -Format s)" | Out-File -Encoding utf8 $log
"exe sha256 $((Get-FileHash -Algorithm SHA256 $exe).Hash)" | Out-File -Append -Encoding utf8 $log

& pwsh -NoProfile -File (Join-Path $here 'run-cli-cases.ps1') -Exe $exe -Label $Label | Out-File -Append -Encoding utf8 $log
Push-Location $here
try {
    "-- CLI contract vs baseline" | Out-File -Append -Encoding utf8 $log
    python -I compare-cli-cases.py runs/baseline "runs/$Label" | Out-File -Append -Encoding utf8 $log
    $cli = $LASTEXITCODE
    "-- flag literal index vs HEAD" | Out-File -Append -Encoding utf8 $log
    python -I flag-index.py $repo 2>$null | Out-File -Encoding utf8 "flag-index-$Label.tsv"
    $flagDiff = Compare-Object (Get-Content flag-index-baseline.tsv) (Get-Content "flag-index-$Label.tsv")
    if ($flagDiff) { $flagDiff | Out-String | Out-File -Append -Encoding utf8 $log; $flags = 1 } else { "identical" | Out-File -Append -Encoding utf8 $log; $flags = 0 }
    $loop = 0
    if ($Server) {
        "-- server loopback vs baseline" | Out-File -Append -Encoding utf8 $log
        python -I server-loopback.py $exe $Label
        python -I compare-server-loopback.py runs/baseline-server "runs/$Label-server" | Out-File -Append -Encoding utf8 $log
        $loop = $LASTEXITCODE
    }
} finally { Pop-Location }

"-- ctest" | Out-File -Append -Encoding utf8 $log
$ctest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir $out -j 8 --timeout 600 2>&1 | Select-String -Pattern 'tests passed|Failed|\*\*\*|ModEntry' | ForEach-Object { $_.Line } | Out-File -Append -Encoding utf8 $log
$tests = $LASTEXITCODE

"-- result cli=$cli flags=$flags ctest=$tests loopback=$loop" | Out-File -Append -Encoding utf8 $log
Get-Content $log | Select-String -Pattern 'DIFF|MISSING|same,|identical|<=|=>|tests passed|Failed|result' | ForEach-Object { $_.Line }
if ($cli -or $flags -or $tests -or $loop) { exit 1 }
