# Weavel Alt Form EU1.1 — 実装・受入記録

対象: Native C++、`develop6_weavelAlt`。
指示書: `mphCodex/mphAnalysis/Control/WeavelAltForm/Fruity-Prime-Weavel-AltForm-ROM-Parity-Implementation-Guide-EU1_1.md`。
基準HEAD: `387111e84ab642898e8768eb8e8844d8e91c996d`。

## 完了判定

Phase 1～8は実装済みで、以下のローカル受入検証は合格。最終code commitのdesktop CIを残す。
ユーザー指定によりC# mirrorは対象外。

| 要件 | 受入を証明する検証 | 現状 |
|---|---|---|
| Phase 1 raw factor / damage / floor | 6144、6083、5412、3948、2867、巨大damage | production damage hook / helperのCTest合格 |
| Phase 2 fresh spawn / EquipInfo | freeze・burn・target・velocity・Story overrideを設定し再enter。defaultへreset | CTestと実assetのexit/re-enter合格。burn effect unlinkも確認 |
| Phase 3 recovery / thresholds / decision | 54 native ticks、freeze中停止、15/15/13/10/7、odd stepで発射なし | CTestとproduction gameplay統合検証合格 |
| Phase 4 lunge edge / cooldown | cooldown1受理、2拒否、拒否edge消費、hold、odd edge保持、bot/remote共通入口 | production ProcessInputとremote press historyの統合検証合格 |
| Phase 5 form lifecycle | 100/101 split、alive merge、dead継続、重複forceでHP不変 | local force、通常enter/exit、morph/unmorph途中の復旧すべて合格 |
| Phase 6 explicit turret state | active/dead/bipedのround trip、remote activationでsplitなし、HP/position/grounded適用 | wire round trip、production publisher/receiver/replica合格。重複・逆順snapshotと古いlifeを拒否 |
| Phase 6 protocol | version17、旧16拒否、PlayerStateサイズ、複数player境界、MaxPacketSize | 128 bytes/player。実UDP serverが旧16をReasonProtocolで拒否。8人＋56 health spawnersの全配信合格 |
| Phase 7 timers / physics | target30、freeze75/15、burn150/every8、Story65/<60、shot timer、native physics/sweep | native timer、owner u8 shot timer、native gravity/sweep、実地形への着地合格 |
| Phase 8 fixed constants | Story lunge 1228/4096、1843/4096 | production Story bot入力・移動で確認。通常移動のgravity/damping適用後の速度も検証 |
| Windows / Linux / macOS CI | 今回の最終code commitのjobsとtest logs | 3 desktop jobsにCTest追加済み。最終code commitをpush後、実行結果を確認する |

projectile authorityの推測変更、LoS追加、touch clock流用は行わない。
手動操作・実機ROM比較、ローカルbuild、asset-backed checks、CIはそれぞれ別の証拠として扱う。

## 2026-10-08 最終実装とローカル受入

責務はraw fire arithmetic、scene共通native cadence、edge latch、player form/life、turret mechanics、wire replicationに分離。
`PlayerWeavel.cpp`でform lifecycleとnative owner timerを扱い、turretはlocal spawnとsnapshot activationを分ける。
snapshot activationはauthority HPを採用し、split/mergeを再実行しない。Altでturretが死んだ状態も保持する。
sceneではturretがownerより先に処理されるため、owner native timerをframe guard付きで共通advanceし、両者の呼出しで二重decrementしない。

protocol 17は既存114 bytesの後ろにflags/HP/positionの14 bytesを追加。8人を全員送った上でhealth-spawner tailをpacket容量内で巡回配信し、受信cacheは同じmatchの他spawnerを保持する。56個を4 snapshotsで全配信するケースをproduction publisherで検証。

| 検証 | 結果 |
|---|---|
| MSVC Release game / parity targets | build成功 |
| CTest WeavelAltFormParity / DialancheNativeCollision / IntentTouchPayload / NativeTouchState | 4/4成功、Weavelは179 checks＋NetHealthSync assertions |
| `tools/check-weavel-alt.ps1` | 実assetsのproduction checks 383件成功、旧16の実UDP拒否成功、scene step failures=0 |
| `tools/check-dialanche-combat.ps1` | 28 production assertions＋独立UDP peers 2件成功 |
| `tools/check-dialanche-network.ps1 -SpireSeconds 8 -TargetSeconds 10` | authority / 2 clientsでevents=4、HP=97一致、native phase / base8 / 同tick重複body eventなし |
| `-lockjawenemycheck "MP3 PROVING GROUND"` | 93 checks成功 |
| Android arm64関連7 translation units | compile成功。既存warningsあり。APK/device未検証 |

ローカル再現コマンド:

```powershell
ctest --test-dir tools/build/out/msvc-Release -C Release -R 'FruityPrime\.(WeavelAltFormParity|DialancheNativeCollision|IntentTouchPayload|NativeTouchState)$' --output-on-failure --no-tests=error
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-weavel-alt.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-dialanche-combat.ps1 -OutputDirectory tools/build/out/weavel-final-dialanche
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-dialanche-network.ps1 -SpireSeconds 8 -TargetSeconds 10 -OutputDirectory tools/build/out/weavel-short-network
```

実asset検証には実行binary隣の`paths.txt`とEU1.1 game assetsが必要。asset-free CTestを3 desktop CIで実行する。
旧16拒否はNative `NetTransport`と`DedicatedServer`を実UDPで接続して検証する。PowerShell UDP probeでは応答timeoutが発生したため採用していない。
ダイアランチの既定16秒/18秒の再実行は17 damage events後の死亡・再出現でlatest-eventが0になり、fixtureの累計event-id比較に失敗した。今回の回帰証拠は8秒/10秒の成功run。scriptへ時間指定とWindows process handle保持を追加し、検証対象の判定自体は緩和していない。
ignored logs: `tools/build/out/weavel-validation`、`weavel-final-dialanche`、`weavel-short-network`、`weavel-final-android.log`、`weavel-protocol-android.log`。
下記checkpointは中間時点の履歴であり、現在の未完了項目を示すものではない。

## 2026-10-08 checkpoint

ユーザーのcommit/push指示に対する中間保存。指示書全体の完了を意味しない。

### 実装

- `HalfturretFireRate`にraw factor damage / recovery / thresholdの算術を分離。
- `ResetForSpawn`を`Initialize`冒頭で実行し、前lifeのtarget、affliction、velocity、timer、Story weapon overrideを除去。
- `NativeGameplayClock`はscene frameの非zero偶数を使用。input/touchのresetでphaseが変わらない。
- turretのdecision / 内部timer / physicsをnative tickで1回実行。非native stepではstateを保持。
- ownerの共通`TimeSinceShot`は現時点では60Hzのまま。threshold comparisonで2倍換算しており、u8 native timerへの移行は残件。
- protocolは16のまま。明示turret snapshot / force lifecycle / lunge latchは次の変更で扱う。

### 実行結果

- MSVC Release: `fruity_prime`と`fruity_weavel_alt_form_parity_tests`をbuild成功。
- CTest: WeavelAltFormParity、DialancheNativeCollision、IntentTouchPayload、NativeTouchStateの4/4成功。
- WeavelAltFormParity: 154 checks成功。実objectのdamage hook、reset、EquipInfo default復元を含む。ゲームassetsは使用しない。
- `-lockjawenemycheck "MP3 PROVING GROUND"`: 93 checks成功。
- `check-dialanche-combat.ps1`: 28 production assertionsと独立UDP peers 2件成功。
- `check-dialanche-network.ps1`: authority / clientsでevents=4、HP=67一致。
- Android arm64: HalfturretEntity translation unit compile成功、既存の24 warnings。APK build / device testは未実施。
- `-simcheck "MP3 PROVING GROUND" -players 8 -seconds 5`: `Bad optional access`で失敗、spawned 0/8。同じassetsを使用した保存済み`FruityPrime-SrpCheck.exe`（2026-10-07 19:11生成）でも同じ失敗。今回のWeavel acceptance成功の証拠には含めない。

ローカルlogsはignored `tools/build/out/weavel-*` に保存。
