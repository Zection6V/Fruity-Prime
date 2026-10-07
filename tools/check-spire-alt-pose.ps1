# Asset-free source probe for the Spire collision pose ownership invariant.
$root = Split-Path -Parent $PSScriptRoot
$process = Get-Content -Raw (Join-Path $root 'src/MphRead/Entities/Players/PlayerProcess.cs')
$draw = Get-Content -Raw (Join-Path $root 'src/MphRead/Entities/Players/PlayerDraw.cs')
$inputCode = Get-Content -Raw (Join-Path $root 'src/MphRead/Entities/Players/PlayerInput.cs')
$nativeProcess = Get-Content -Raw (Join-Path $root 'src/MphRead.Native/Entities/Players/PlayerProcess.cpp')
$nativeInput = Get-Content -Raw (Join-Path $root 'src/MphRead.Native/Entities/Players/PlayerInput.cpp')
$nativeCollision = Get-Content -Raw (Join-Path $root 'src/MphRead.Native/Entities/Players/PlayerCollision.cpp')

$checks = @(
    @{ Name = 'pose follows alt animation update'; Ok = $process -match '(?s)UpdateAnimFrames\(_altModel\);\s*}\s*if \(Hunter == Hunter\.Spire && Flags2\.TestFlag\(PlayerFlags2\.AltAttack\)\)\s*{\s*UpdateSpireAltCollisionPose\(\);' },
    @{ Name = 'simulation writes both rock positions from animated nodes'; Ok = $process -match '(?s)void UpdateSpireAltCollisionPose\(\)\s*{\s*//[^\r\n]*\s*AnimateSpireAltAttack\(\);\s*_spireRockPosL = _spireAltNodes\[0\]!\.Animation\.Row3\.Xyz \+ Position;\s*_spireRockPosR = _spireAltNodes\[1\]!\.Animation\.Row3\.Xyz \+ Position;' },
    @{ Name = 'draw does not write collision positions'; Ok = $draw -notmatch '_spireRockPos[LR]\s*=' },
    @{ Name = 'attack startup initializes both positions'; Ok = $inputCode -match '(?s)_altModel\.SetAnimation\(\(int\)SpireAltAnim\.Attack, AnimFlags\.NoLoop\);\s*_soundSource\.PlaySfx\(SfxId\.SPIRE_ALT_ATTACK\);\s*_spireRockPosR = Position;\s*_spireRockPosL = Position;' }
)

$checks += @(
    @{ Name = 'native headless pose records only on shared even phase'; Ok = $nativeProcess -match '(?s)UpdateSpireAltCollisionPose\(\).*?AnimateSpireAltAttack\(\);.*?IsNativeCollisionStep\(frame\).*?_dialancheNativeCollision.Record' },
    @{ Name = 'native attack resets both continuous rocks and history'; Ok = $nativeInput -match '(?s)_spireRockPosR = static_cast<Vector3>\(Position\);\s*_spireRockPosL = static_cast<Vector3>\(Position\);\s*_dialancheNativeCollision.Reset\(static_cast<Vector3>\(Position\)\);' },
    @{ Name = 'native attack consumers never read continuous rocks'; Ok = $nativeCollision -notmatch '_spireRockPos[LR]' },
    @{ Name = 'native overlap consumes strictly older history'; Ok = $nativeCollision -match '(?s)DialancheHitsVolume.*?IsNativeCollisionStep\(frame\).*?PoseForHit' }
)

foreach ($check in $checks) {
    if (-not $check.Ok) {
        Write-Error "SPIREPOSE FAIL $($check.Name)"
        exit 1
    }
    Write-Output "SPIREPOSE ok $($check.Name)"
}
