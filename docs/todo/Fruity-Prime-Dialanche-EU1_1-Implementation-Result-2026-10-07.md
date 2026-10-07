# Dialanche EU1.1 実装・検証結果 — 2026-10-07

対象ブランチ: `develop5_morphBall`。基準HEAD: `17da22216f686c930da07b40a4ab410650b77c76`。
指示書と配下の10件の補足資料（EU1.1アドレス照合、入力、初期化、pose、overlap、damage、終了処理を含む）を確認した。

## 実装

Player / Halfturret / Enemy / DoorのDialanche overlapは、共通の
`DialancheHitsVolume`を通す。scene frameが非ゼロの偶数の場合だけ判定し、
native tickは`frame / 2`とする。左右の半径はともに0.5、左右ORで1回のreactionとする。

`DialancheNativeCollision`は初期位置と2世代のsampleをPlayerごとに保持する。
攻撃開始時に両岩とhistoryをPositionへresetする。continuous岩位置のheadless更新は継続し、
偶数frameだけhistoryへ記録する。判定はcurrent tickより古い最新sampleを選び、
current tickのRecord前後で結果が変わらない。同tickの再Recordもpreviousを破壊しない。
64bitのhelperは104 bytesで、追加heap allocation、lock、動的target container、frameごとのlogはない。

Door拡張は、従来の扉のapertureと深さ±1.25のcontact領域をcylinderとして表し、
保存した左右の岩sphereでoverlapする。現在のPlayer bodyが領域外でも保存sampleが重なれば判定する。
bodyの物理collisionは従来の処理を継続する。hit後のShotOpen、palette8の
`Unlock(true, true)`、専用hit SFXは既存処理を使う。

入力press edge、NoLoop、attack basis、base damage8、Story bot2/3/5、
NoSfx / NoDmgInvuln / Halfturret、turret radius0.45、TakeDamageの倍率・無敵・team処理、
Spireの終了処理は維持した。LoS、cooldown、全target共通hit latchは追加していない。
Intent / PlayerState / DamageEvent / replayの形式とProtocolVersion16は変更していない。
新しいROMアドレス表記はEU1.1のみ。

## 実行結果

| 検証 | 結果 |
|---|---|
| MSVC Release `fruity_prime`と関連test targets | build成功 |
| 関連CTest6件 | 6/6 PASS（DialancheNativeCollision、NativeTouchState、IntentTouchPayload、RawMouseMotion、WindowsRawMouseInput、VulkanNvidiaReflex） |
| GCC C++20 pure helper test | PASS |
| Android arm64 Clang | helper、collision、input、process、combat check、spire pose checkの6 translation unitsのcompile成功 |
| source gate `tools/check-spire-alt-pose.ps1` | 8/8 PASS |
| asset-backed `-dialanchecheck` | 28 production assertions PASS |
| `tools/check-dialanche-combat.ps1` | authority + 独立したUDP clientプロセス2つ、両方PASS |
| `tools/check-dialanche-network.ps1` | 通常のDedicatedServerへのjoin、Spire/Samusの2 clients、両方exit0。authority/両clientsで11 events・最終HP11一致 |
| `-spireposecheck "AD2 ALINOS PERCH"` | headless PASS、active44 frames、native samples22、左右ともmoving22、same-tick非公開/odd不更新PASS |
| `-maptest "AD2 ALINOS PERCH" -players 8 -hunter Spire -bots -seconds 8` | exit0、480frames、spawn8/8、simulation継続 |

ゲームdataは`paths.txt`のAMHP1（EU1.1）を使用した。
Windows/Linux/macOS CIにはpure helper testを追加した。
実装commit `3395328f12c14474a7feef4f94483ca2f48a7c29`の
[CI run](https://github.com/Zection6V/Fruity-Prime/actions/runs/37584120805)では、
macOS / Clang、Android NDK arm64-v8a / x86_64、APKと各static auditが成功した。
記録時点ではWindows / MSVC、Linux / GCC、Android emulator startup smokeが実行中で、全CI成功は未確認。
ローカルAndroid検証は上記objectsのcross compileであり、ローカルAPK buildや実機操作は行っていない。
追加したHitRig、NetCheckClient、ModEntryもAndroid arm64でcompileした。
maptestは描画を伴うsmoke testであり、ROM画像とのpixel比較や操作感の実機評価ではない。

## production受入項目

| 指示書の項目 | 証拠 |
|---|---|
| 26: Reset、same tick非公開、next tick、processing order | pure unit test。duplicate/stale Record、再attack Reset、frame0/odd/evenも検証 |
| 27: Player連続接触 | `CheckPlayerCollision -> CheckAltAttackHit1 -> TakeDamage`、201/202で100→92・event1・SFX1、203/204で92→84・event2・SFX2 |
| 28: 非native siblingだけの接触 | damage/SFX0。native接触では8 damage |
| 29/31: 複数target、左右OR | 同tickの2 targetに各8。両岩が同じvolumeへ重なっても1 event/channel |
| 30/34: body / Halfturret | 同native tickに2 events / SFX2、owner100→88、turret100→96。0.949でhit、0.951でmiss（0.5+0.45） |
| 32/33: 共通damageとStory bot | Double Damageで1 event・16 damage。bot level0/1/2で2/3/5 damage・SFX1 |
| 35: SFX周期 | recording audio backendで実際の専用PlaySfx要求を計数。非native stepは0 |
| 36: Enemy拡張 | 既存CheckAltAttackHitEnemy1 / EnemyInstanceEntity::TakeDamage / EnemyTakeDamageを通し100→92→84、nativeごとにreaction/SFX1 |
| 37: Door拡張 | 既存AltAttackHitDoor / Unlockを通しShotOpen・Unlocked・SFX1。siblingは0。current contact sampleを隠し、次tickで公開してreaction1 |
| 38: headless更新 | SpireAltPoseCheckで実入力intentからmorph / AltAttack。44 active frames中22 native samples |
| 共通geometry / processing order | PlayerがRecord前、Halfturret・Enemy・DoorがRecord後でも同じprevious contact sampleへhit。新current sampleは遠方に置いて検出 |
| EndAltAttack | Spireのflag clear、cooldown0 |
| 39: Player network | production BroadcastSnapshotから3 snapshotsをUDP送信し、2独立プロセスのclient handler / NetPlayerBridge / NetDamage::Replayで100→92→84、events0→1→2 |
| 39: 通常join経路 | DedicatedServer、Spire client、Samus clientを起動。Welcome / intent / prediction / damage replayを通し、authorityの11 event identitiesと両clientsの11 replays、最終HP11が一致 |

UDP検証の受信側は、既存playback入口へ受信packetを投入し、通常のclient snapshot handlerを実行する。
これは独立プロセス・実UDP・既存packet serializer/reader・damage replayの検証であり、
DedicatedServerへの通常のjoin/Welcome/intent交換を再現するテストではない。
duplicate snapshotsとdamage historyの再送も重複damageを生成しなかった。

通常join検証の`-hitrig dialanche`は既存HitRigから移動・morph・AltAttackの入力を送る。
forced form、pose書換え、damage注入は行わない。default prediction / claimsも有効のまま。
authority-hitの全11件はweapon None・damage8で、native位相に一致し、同frame/victimの二重body eventはなかった。
連続contactはauthority frame255/257/259/261/263/265でも各1件を確認した。
NetFrameはgameplay前、Scene.FrameCountはgameplay後に進むため、logのauthorityFrameは奇数になる。
targetのspawn HP99から11回×8 damageで11となった。event数は接近・入力timingで変わるため、
scriptは2件以上、全event identity一致、native位相、base8、各tick/bodyの最大1件、最終HP一致を検証する。
Enemyの計数は既存reaction callback、Doorの計数はShotOpenと実際のSFX要求で行い、
架空のEnemy/Door用NetDamage eventは追加していない。

再実行:

```powershell
ctest --test-dir tools/build/out/msvc-Release -R '^FruityPrime\.(DialancheNativeCollision|NativeTouchState|IntentTouchPayload|VulkanNvidiaReflex|RawMouseMotion|WindowsRawMouseInput)$' --output-on-failure --no-tests=error
./tools/check-spire-alt-pose.ps1
./tools/check-dialanche-combat.ps1
# TEST ARENAのrecipeを実行binaryのmaps/arena/へ配置して実行する。
./tools/check-dialanche-network.ps1
# ゲームdataを参照するpaths.txtは実行binaryの横へ配置する。
./tools/build/out/msvc-Release/FruityPrime.exe -spireposecheck "AD2 ALINOS PERCH" -noupdate
```

詳細logsはignoredの`tools/build/out/dialanche-validation/{authority,peer-0,peer-1}.log`、
`dialanche-build.log`、`dialanche-android-compile.log`、`dialanche-spirepose.log`、
`dialanche-maptest.log`にある。
通常joinのlogsは`tools/build/out/dialanche-live/`のserver / spire / target logsと
`netlog-{server,DialancheLiveSpire,DialancheLiveTarget}.txt`に保存した。

## 指示書の前提差と残る受入条件

指示書18/39は「既存Enemy damage/state同期」「既存Door state同期」を前提としている。
基準HEADにはその経路がない。`NetSession::BroadcastSnapshot`はPlayerState、match time、
health pickup spawn stateを送信する。`HandleSnapshot`もその構成を受信する。
`NetDamage`はPlayerEntityのslot/life単位、`WorldEvents`はjump pad / teleport計数であり、
Enemy HP / Door stateの同期ではない。Door自体もSinglePlayerを前提に構築される。

したがって**Enemy/Doorのネット同期受入は未達**として残す。
全対象のproduction判定とPlayer network parityは上記のとおりPASSした。
Enemy/Doorの新規ネット同期を加えるには、今回の「新packet fieldなし・format変更なし」制約との
整合を別途決める必要がある。この前提差を、Enemy/Doorのネット検証成功や全受入完了として扱わない。
