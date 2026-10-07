# Weavel Alt Form EU1.1 — 実装・受入記録

対象: Native C++、`develop6_weavelAlt`。
指示書: `mphCodex/mphAnalysis/Control/WeavelAltForm/Fruity-Prime-Weavel-AltForm-ROM-Parity-Implementation-Guide-EU1_1.md`。
基準HEAD: `387111e84ab642898e8768eb8e8844d8e91c996d`。

## 完了判定

本記録は進行中。各項目を実装と実行結果の両方で確認するまで完了扱いにしない。
ユーザー指定によりC# mirrorは対象外。

| 要件 | 受入を証明する検証 | 現状 |
|---|---|---|
| Phase 1 raw factor / damage / floor | 6144、6083、5412、3948、2867、巨大damage | production damage hook / helperのCTest合格 |
| Phase 2 fresh spawn / EquipInfo | freeze・burn・target・velocity・Story overrideを設定し再enter。defaultへreset | 実装済み。dirty objectのproduction ResetForSpawnはCTest合格。実assetでのexit/re-enterとburn effect unlinkは未検証 |
| Phase 3 recovery / thresholds / decision | 54 native ticks、freeze中停止、15/15/13/10/7、odd stepで発射なし | arithmetic / common scene phaseのCTest合格。freeze中停止・発射decisionの統合gateは未検証 |
| Phase 4 lunge edge / cooldown | cooldown1受理、2拒否、拒否edge消費、hold、odd edge保持、bot/remote共通入口 | 未検証 |
| Phase 5 form lifecycle | 100/101 split、alive merge、dead継続、重複forceでHP不変 | 未検証 |
| Phase 6 explicit turret state | active/dead/bipedのround trip、remote activationでsplitなし、HP/position/grounded適用 | 未検証 |
| Phase 6 protocol | version17、旧16拒否、PlayerStateサイズ、複数player境界、MaxPacketSize | 未検証 |
| Phase 7 timers / physics | target30、freeze75/15、burn150/every8、Story65/<60、shot timer、native physics/sweep | turret内部はnative化済み。freeze75/15・target30のhookはCTest合格。physics / burn / Storyの統合gateとowner shot timer native化は未完了 |
| Phase 8 fixed constants | Story lunge 1228/4096、1843/4096 | 未検証 |
| Windows / Linux / macOS CI | 今回の最終code commitのjobsとtest logs | 新CTestを3 desktop jobsへ追加。リモート実行結果は未確認 |

projectile authorityの推測変更、LoS追加、touch clock流用は行わない。
手動操作・実機ROM比較、ローカルbuild、asset-backed checks、CIはそれぞれ別の証拠として扱う。

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
