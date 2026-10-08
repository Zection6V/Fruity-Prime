# Sylux muzzle guard — Method A

Native C++実装。銃モデル、銃口位置、`MuzzleOffset`は変更しない。発射が成立したSyluxのSurfaceCollision武器だけ、銃軸の`0x651..0x800`（431/4096）を1回検査する。通常の弾薬・charge・continuous phase計算を使い、弾不足、cooldown、銃下げ、projectiles=0、他hunter、反射子弾では追加検査しない。散弾は検査結果を共有する。

## すぐ元の動作に戻す

起動前に環境変数を設定する。再ビルド不要。設定はプロセス起動時に読み、通常は有効。

```powershell
$env:FRUITY_SYLUX_MUZZLE_GUARD = '0'
& .\FruityPrime.exe -noupdate
```

再び有効にする場合は`Remove-Item Env:FRUITY_SYLUX_MUZZLE_GUARD`して起動し直す。比較時はサーバー・クライアントを同じ設定で再起動する。OFFでは短区間検査・pre-hit・遮蔽claim制限を通らず、共有した通常collision resolverを使う。保存設定や通信packetの変更はない。

コードを戻す場合の基点は`9bc796edbc310c35e0a5f03d28f7e7d1cb4e2334`。下表の実装とテストに加え、CMakeのテスト登録と`ModEntry.cpp`の`-netcombatcheck`登録が今回の範囲。後続の変更まで消す一括resetは使わない。

## 責務と接続点

| ファイル（src/MphRead.Native以下） | 責務 |
|---|---|
| `Mods/Combat/SyluxMuzzleGuard.hpp/.cpp` | 短区間生成、入力検証、起動スイッチ、任意の測定。Sylux固有の入口はここに集約 |
| `Mods/Combat/BeamObstacleTrace.hpp/.cpp` | 既存map/Object/Platform、閉じたDoor、active ForceFieldの最短接触。通常beamと共有 |
| `Entities/Players/PlayerInput.cpp` | 実際に計算済みのgun/muzzle/remote originから始点を渡す小さなhook |
| `Entities/BeamProjectileEntity.hpp/.cpp` | ammo成立・弾数確定後に1回query。既存resolverでspawn時に着弾。homing、ice wave、splash、continuousは壁裏への先行damageを防ぐ |
| `Mods/Network/MuzzleObstructionHistory.hpp` | 固定8×1024bucket、shot全identityとweaponの遮蔽履歴。反射子のbeam種別を引き継ぐ。heap確保なし |
| `Mods/Network/NetHitClaims.hpp/.cpp` | pendingと適用直前で履歴を再確認。遮蔽shotの未照合claimは拒否、同一shot/weaponのauthority命中は既存ledgerで照合 |
| `Mods/Network/NetDamage.cpp` | 既存authority命中ledgerへshot keyとweaponを渡すhook |
| `Testing/TestSyluxMuzzleGuard.cpp` | 純粋幾何、Door/ForceFieldの実ヘルパー、履歴identity/expire/reset |
| `Testing/NetCombatCheckSylux.cpp`, `Testing/SyluxGuardWeaponFixture.hpp` | 実Spawn、TryFireWeapon、claim race、開放射撃OFF/ON比較。既存NetCombatCheckへ接続 |

shotごとの新しいvector、常時ログ整形、追加thread、GPU readback、逆向きrayはない。queryのscratchは値で受け取り、pre-hitをbeamへ持ち越さない。通常の発射位置とRNG経路を維持する。履歴はroom/authority/slot/lifeの既存reset経路で破棄する。

遮蔽shotのclaimはdirect/splash分類をpacketだけから推測しない。実際の合法splash/ricochetはauthorityが適用し、同一key/weaponのledgerで重複を消す。近隣frameの開放射撃のledgerを借りることはできない。

## 検証（2026-10-09）

Windows MSVC Release、基点SHAからの未コミット作業ツリー。`tools/build/out/msvc-Release`を使用。ログは同ディレクトリの`sylux-build.log`、`sylux-ctest.log`、`sylux-combat.log`。ゲーム資産は既存`paths.txt`のAMHP1。

| 項目 | 結果 |
|---|---|
| Windows Release全ビルド | PASS |
| CTest全体 | PASS 31/31（Sylux専用テストを含む） |
| `-netcombatcheck "MP3 PROVING GROUND" -noupdate` | PASS 252 assertions。実map短区間、壁裏HP、9武器の通常/charge、弾不足/partial/continuous、散弾、pool再利用、4 authority slot、BOT属性、合法壁手前Missile splash、claim遅延/再送/ledgerを検証 |
| 開放射撃のOFF/ON一致 | PASS：同一回数でRNG1/RNG2、origin、velocityが一致 |
| Android arm64 Native | PASS：NDK 27.2 / Qt 6.11.2 / API 28 / RelWithDebInfo、`fruity_mphread_native_android`。`build/native-android-arm64-v8a/libFruityPrime_arm64-v8a.so`生成。ログ：`sylux-android-configure.log`、`sylux-android-build.log`。APK/端末試験は含まない |
| 実通信4クライアント、loss/reorder、host migration | UNVERIFIED。headlessの4slotはUDP peer試験ではない |
| 実描画の銃/着弾/charge/Shock Coil音、手動薄壁/角/二面/動的Object/Platform/実Door/ForceField | UNVERIFIED。位置不変・幾何helper・実mapの自動試験とは区別する |
| affinity/Prime Hunter切替、object-heavy全map、実BOT AIの壁際試験 | UNVERIFIED |
| Linux/macOS CI、ASan/UBSan、Android端末/APK起動 | UNVERIFIED |
| 完全な性能受入（TryFireWeapon、simulation step、FPS/frametime、1/4人・Noxus・BOT） | UNVERIFIED。短queryとheadless Spawnの測定のみ |

測定は`FRUITY_SYLUX_MUZZLE_GUARD_METRICS=1`で任意に有効化できる。通常はclock/counter更新を行わない。`-netcombatcheck`は検証中だけ計測を有効にし、終了時に戻す。short queryは2048標本、Spawn比較は各1024標本・64回warmup・1 shot/frame・70 entities・NoMuzzle・描画なし。Spawn比較の両群で計測counterをOFF、pool解放は計時外とする。これを描画FPSや全mapの性能保証に置き換えない。

| headless測定（ns） | mean | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| short query | — | 1700 | 1800 | 1900 |
| 開放Spawn OFF | 3892 | 3800 | 4100 | 5900 |
| 開放Spawn ON | 5646 | 5500 | 5900 | 7500 |

この標本のSpawn平均差は1.754µs。観測値であり性能上限ではない。GPU描画・ネットワーク負荷・長時間spikeを含まない。

実装ガイド：`mphCodex/mphAnalysis/Weapons/Sylux-Wall-Pierce/Sylux-Wall-Pierce-Method-A-Complete-Implementation-Guide-FruityPrime-2026-10-09.md`。コードは実装済みだが、上記UNVERIFIEDを残すためガイドの全受入ゲートをcloseしたとは扱わない。
