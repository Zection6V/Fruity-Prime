# レンダリングアーキテクチャ対応記録

更新日: 2026-10-02

対象: [比較レビュー](Fruity-Prime-Rendering-Architecture-Review.md)。比較時の固定コミットは変更しない。
今回は Windows の OpenGL / Vulkan を実装・検証対象にする。ユーザー確認により、
Metal / D3D12 の実装（Phase F / G）と macOS 実機検証は将来の対応に残す。
共通契約に将来の API 名があることは、そのバックエンドが実装済みという意味ではない。

## 進捗

レビュー全体の対応は **進行中**。この記録の最初の区切りは共通境界の整理であり、
Phase A～E / H または R1～R20 がすべて完了したという記録ではない。

| 項目 | 現在の実装・残作業 |
|---|---|
| R1: 選択と Provider | `SceneBackendKind` を `GraphicsBackend` へ統合。OpenGL / Vulkan Provider と Session を登録し、shader・mesh・transient geometry・UI・表示生成を委譲。共通描画経路への統一は R6 / R15 で続ける |
| R2: shader ABI | `SceneShaderSources` と論理グループ・binding を共通 RHI へ移動。std140 packing は Vulkan 側に残す。`ShaderDesc` に形式を明示。**本番の生成 manifest / SPIR-V と論理グループの接続・検証は残る** |
| R3: PipelineLayout | 複数グループの記述を値で所有。両 backend の generic pipeline / binding を共通 GPU fixture で検証。cache key に全グループを含める。scene の生成 shader adapter の整理は残る |
| R4 / R5: submission と resize | 共通 `SubmissionSerial` / `SubmissionProgress` に Vulkan timeline と OpenGL GLsync scheduler を接続。frame number を retirement の証拠にしない。Vulkan は Buffer / Sampler / Image / ImageView / Pipeline を実際の送信完了で破棄し、resize は先に画像・全ビューを確保して旧世代を retire。OpenGL はフレーム外の command / resource release も実際の stream marker で覆う。GL marker の集約、残る ownership / format stress は後続で扱う |
| R6 / R7: OpenGL command / sampler | Buffer / vertex・index binding / Draw / DrawIndexed / GPU Copy / BindingSet と独立 sampler・value cache を実装。Windows scene / transient geometry を同じ Buffer / CommandList / VAO 経由へ接続。共通 GPU fixture と旧新7画像の一致を確認。単一2D画像以外の範囲、packed depth/stencil copy、recording 契約の統一、本番 shader ABI 接続は残る |
| R8: Session 寿命 | Vulkan の意図的に解放しない `VulkanScene` を削除。切替で scene / UI → commands → swapchain → device / context → window の順に解放する。OpenGL は既存 context device を Session の明示終了で解放。所有のさらなる整理・stress の resource count gate は残る |
| R9: presentation | request と実際の mode / capabilities を分離。typed acquire / present status を実装し、frame loop で利用。最小化・明示的 close request は一時停止、API の device / surface loss は別分類。OpenGL の generic conformance coverage は R19 で拡張する |
| R10～R13 | `VulkanFrameScheduler` を独立させ、実際の queue submit / completion を担当。descriptor / frame slot / memory / upload の分離は残る。native pipeline cache / budget / upload ring は未対応 |
| R14: eligibility / admission | Vulkan passive probe は instance / physical device の確認で止まり、logical device / queue を作らない。incoming Session の device / swapchain 生成が active admission。失敗注入による復旧検証は残る |
| R15: GL vertex interface | Windows scene / transient / launcher UI を explicit input と RHI Buffer / CommandList / VAO へ統一。desktop wrapper の conventional array / current-value mirror を除去し、頂点位置を Vulkan と共通化。GPU composite / 旧新14画像の一致を確認。本番 binding ABI の接続は R2、既存の backend 間 caption 差は画像 gate に残る |
| R16: readback | 未対応。非同期 ticket と lifetime / backpressure policy が必要 |
| R17: error | Vulkan の device loss / surface loss / OOM を `BackendError` へ分類。presentation は typed status を返す。native code を presentation facade まで保持する改善・故障注入は残る |
| R18: 診断 | 未対応。共通 debug label / timestamp interface が必要 |
| R19 / Phase H | 同じ fixture で両 backend を検証する `-rhiconformance` を追加。lifetime gate に sampler / VAO を追加。操作・新旧 effect の切替 fixture と動く bot の stress を分けて記録。全 format / recording / failure / session teardown stress は引き続き拡張する |
| R20: optional pacing | 将来の vendor extension を core RHI に追加しない方針を維持。既存 pacing と optional controller の境界を後続で確認する |

## 最初の基盤変更

- `SceneBackend.hpp` は OpenGL / Vulkan 固有 shader header を include しない。
- `BackendSession.hpp` は API の native 型を持たない。実装は各バックエンドの Provider 内に置く。
- `PipelineLayout` は binding layout の記述をコピーして保持する。入力 layout の一時オブジェクトや pointer identity が cache key を支配しない。
- Vulkan generic pipeline の診断で4グループを bind し、範囲外グループを拒否する。
- `BackendError` は backend・分類・native code を保持する。OOM / 未分類エラーを一時的な表示不可として隠さない。
- close request は drawable の破棄ではない。シェルの切替操作が close request を使うため、この差を typed presentation で扱う。
- Windows の切替回帰に含まれる着弾エフェクト・新旧 Lockjaw bomb・texture source の寿命チェックは維持する。

## 検証の記録

MSVC Release build、CTest 5/5、`tools/check-phase5-shader-interface.py` が成功。
GPU: NVIDIA GeForce RTX 5070 Ti / driver 617.14。Khronos validation 有効。

| 実動作検証 | 結果・証拠 |
|---|---|
| `-vulkanresourcecheck` | exit 0、live=0、validation errors=0。4グループの generic pipeline / binding 診断を含む。ログ `C:/tmp/gp/architecture-vulkanresourcecheck.log` |
| `-vulkanpresentcheck` | exit 0、resize / fullscreen / minimize / restore / requested-resolved mode / typed acquire-present / shutdown、errors=0。ログ `C:/tmp/gp/architecture-vulkanpresentcheck.log` |
| `-vulkanpresentfallbackcheck` | exit 0、errors=0、fallback-retired-releases=6。ログ `C:/tmp/gp/architecture-vulkanpresentfallbackcheck.log` |

基盤変更後の試合内切替は OpenGL 開始・Vulkan 開始とも exit 0。
各プロセスで front screen の切替1回と、同じ Alinos Perch の試合で Settings から3回の切替・Resume を通した。
全6回で scene / window geometry / visibility を維持し、simulation frame が進み、
effect definitions は105/105、新しい impact / bomb と既存 bomb は各2 particles。
texture-only source は切替中に保持され、scene 終了時に解放された。
Khronos validation は有効で、両ログに `VUID` / `Validation Error` / failed-step はない。

最終ログ・画像は `C:/tmp/gp/architecture-foundation-20261002-003241/`。
各開始バックエンド7枚、合計14枚。両方の `switch-5-match.png` を目視し、
黄色の impact と青い Lockjaw core の描画を確認した。
これは動的な試合の切替回帰であり、固定 frame の Golden Capture 全体の pixel parity の証拠とは区別する。
途中で検出した close request の誤分類を修正してから再実行した最終結果である。
本番の切替テスト手順は [既存の再実行手順](old/Fruity-Prime-CPP-Renderer-Hot-Switch-Fix-2026-10-01.md#repeating-the-effect-regression-on-windows) にある。
Android は今回ビルド・実動作検証していない。enum 統合に必要な参照の更新だけを含む。
remote CI と、実際の device loss / surface loss / OOM 故障注入は未実行。

## Vulkan の submission retirement / resize

`SubmissionSerial` は simulation / presentation frame や command slot と独立した値。
native submit が成功した後だけ進め、timeline の実際の完了値で release callback を回収する。
descriptor frame marker・generic/scene command list・binding 診断の送信を scheduler に接続した。
swapchain の blit と Skia の外部描画も、同じ graphics queue 上の後続 marker で対象画像の寿命を覆う。
present queue の swapchain / present semaphore retirement は既存の別契約を維持する。
marker は `ALL_COMMANDS` の signal scope を使う。
仕様確認: [vkQueueSubmit2 の synchronization scope](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit2.html)。

resource destructor は recorded use を submit して logical binding を忘れ、native handle と
VMA allocation だけを retirement queue に渡す。callback が device state を所有しないので循環参照を作らない。
通常の Buffer / Sampler / Texture / View / Pipeline の破棄で `WaitScene` / device idle を呼ばない。
command ring slot の再利用に必要な fence 待機、明示 `WaitIdle`、session teardown は別の境界として残る。
同期 readback / upload の改善は R13 / R16 の残作業であり、今回除去済みと扱わない。

texture resize は新しい画像・既存 public view に対応する新しい native view・sampled view の
確保が全部成功してから交換する。public Texture / TextureView / TextureHandle は変えない。
すでに submit した descriptor / rendering は旧世代を完了まで保持し、後続 descriptor は新世代を見る。
現在の rendering target が保持する native view も交換時に更新する。
確保失敗では元の画像・サイズ・ビューが残る。

再実行手順（repo root の PowerShell、MSVC Release build 済み、game files / paths.txt 配置済み）:

```powershell
Push-Location tools/build/out/msvc-Release
try {
    & .\FruityPrime.exe -vulkanresourcecheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'resource regression failed' }
    foreach ($backend in @('opengl', 'vulkan')) {
        & .\FruityPrime.exe -gpulifetime 'AD2 ALINOS PERCH' -cycles 40 -frames 3 -rhi $backend -vkvalidation -noupdate
        if ($LASTEXITCODE -ne 0) { throw "$backend lifetime regression failed" }
    }
} finally { Pop-Location }
```

`-vulkanresourcecheck` は 64 回の clear / descriptor bind / resize / release を、
clear の完了を待たずに通す。ビューの object identity、native 世代の交換、範囲外サイズでの失敗後の
元画像保持、queue への retirement、通常 churn の `DeviceWideWaits` 不変を検査する。
最後の明示 `WaitIdle` 後に resource baseline・`Retired=0`・`Completed=Submitted` を要求する。
CPU 単体テストでは failed submit で番号を消費しないこと、古い completion が値を戻さないこと、
未来の completion / submission gap の拒否、retire 順が逆でも last use で回収することを検査する。

今回の結果:

- MSVC Release build / CTest 5/5 / shader interface audit 成功。
- `C:/tmp/gp/architecture-retirement-vulkanresourcecheck.log`: 64 cycles PASS、churn device-wide waits=0、retired=0、validation errors=0。
- `C:/tmp/gp/architecture-retirement-vulkanpresentcheck.log`: presentation PASS、validation errors=0。
- `C:/tmp/gp/architecture-retirement-vulkanpresentfallbackcheck.log`: PASS、fallback-retired-releases=6、errors=0。
- `C:/tmp/gp/architecture-retirement-20261002-010404/`: OpenGL / Vulkan 開始の切替各3回 PASS。全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。両方の最終画像で黄色 impact と青い bomb core を目視。game source は scene 終了時に解放。
- `C:/tmp/gp/architecture-retirement-lifetime40-opengl.log`: 40/40 PASS、毎回 resource / retired=0、private memory の測定増分 0.238 MB/回。
- `C:/tmp/gp/architecture-retirement-lifetime40-vulkan.log`: 40/40 PASS、毎回 resource / retired=0、private memory の測定増分 0.162 MB/回。

短い OpenGL 12 回測定は resource / retired count はすべて0だったが、private memory peak が
208→242 MB（5.67 MB/回）となり memory gate が FAIL。
`C:/tmp/gp/architecture-retirement-lifetime-opengl.log` に保存。
同じ閾値・同じ実装の40回測定では PASS したが、短い測定の失敗を削除したり、
40回の結果を無制限の長時間メモリ保証として扱ったりしない。
OpenGL submission 移行・非同期 readback・責務分離・Phase H の拡張は引き続き残る。

## OpenGL 共通コマンドと scene geometry の接続

Windows のメッシュと transient geometry は API の buffer 名を保持しない。
Provider が受け取った `GraphicsDevice` / `CommandList` を使い、RHI `Buffer` の所有と
`SetVertexBuffer` / `SetIndexBuffer` / `DrawIndexed` へ接続した。
scene shader / pass state を維持する backend 内の adapter が頂点レイアウトを明示する。
通常 generic draw は pipeline が必須。この adapter は GPU rebuild 中に先に bind された
scene program も扱う。完全な本番 shader ABI 統一が済んだという意味ではない。

- quads / strips / fans / line loop の index helper を両 backend で共有。
  単体テストは交互の winding、fan の先頭、loop の閉じ方、不完全な末尾を検査する。
- static / inherited / mixed の色・法線と display-list terminal state を保持。
  transient stream は容量が足りる間は同じ2つの native buffer 名を使う。
  全範囲の `WriteBuffer` は storage を orphan し、部分更新は残りの内容を保持する。
- VAO cache は vertex layout / native buffer / offset / index buffer で区別し、buffer retirement で無効化。
- sampler は texture parameter の書換えではなく native sampler object を独立して bind。
  同じ description の native object を weak cache で共有し、最後の使用者で retire する。
- GPU buffer copy と PBO image transfer は行 pitch の余白も扱う。
  新しい CPU texture backup は作らない。モデルの解読・mixed attribute の生成・upload 元は CPU 側にある。
- Android geometry は従来の GLES draw wrapper を内部ファイルに残す。
  今回 Android は build / 実機検証していない。共通 device の変更を含むので無変更とは主張しない。

### 共通 GPU fixture の再実行

repo root の PowerShell、MSVC Release build 済み。この検査自体には game files は不要。

```powershell
Push-Location tools/build/out/msvc-Release
try {
    & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI conformance failed' }
} finally { Pop-Location }
```

今回の MSVC build は **OpenGL PASS と Vulkan PASS の両行**を要求する。
Vulkan は Khronos validation を有効にし、shutdown を含め errors=0 を検査する。
同じ fixture / 同じ論理 shader ABI を使用し、GLSL / SPIR-V の生成だけが backend 別。

非整列 offset の buffer GPU copy、余白のある row pitch の image 往復、4グループの layout
（3グループを実際に使用・最後は空）、UBO / sampled image / sampler、宣言順に依存しない
logical binding、非 index draw と `firstIndex=1` の index draw、範囲外 group の拒否を検査する。
同じ赤青2 texelの画像を **1回の draw 内で2つの unit** に bind し、nearest の青と
linear の紫が同時に出ることを readback する。次の draw で2つを交換する。
texture parameter の共有によって両方の sampling が同じになる実装を検出できる。
release / `TrimCaches` / `WaitIdle` 後は `LiveObjects=0` / `Retired=0` を要求する。
通常描画が readback / idle を必要とするという意味ではない。

結果: `C:/tmp/gp/architecture-glgeometry-final-rhiconformance.log`、両 backend PASS。
MSVC Release build / CTest 5/5 / shader audit 18 sources も成功。
`-vulkanresourcecheck` / `-vulkanpresentcheck` / `-vulkanpresentfallbackcheck` は PASS、errors=0、
fallback-retired-releases=6。ログは `C:/tmp/gp/architecture-glgeometry-<check>.log`。

### scene の画像と lifetime の回帰

同じ common device 実装で旧 geometry と新 geometry を一時的に切り替え、
`-goldencapture all` を同じ inputs / harness / 1600x900 で実行した。
transparent-object / decal / particle / trail / hud / fade / whiteout-disruption の7ケースで
**decoded RGB の差は0 bytes**。geometry 移行を切り分ける比較であり、
`a29ceb` 全体との cross-revision 比較や、OpenGL / Vulkan 全画素一致の証拠ではない。
旧画像: `C:/tmp/gp/architecture-legacy-geometry-golden-opengl/`。
新画像: `C:/tmp/gp/architecture-glgeometry-final-golden-opengl/`。

両 backend の7ケースはそれぞれ capture gate を通った。
backend 間には各画像で303 pixels / 909 bytes の差があり、同じ画面下部の文字範囲に集中する。
旧新 OpenGL geometry が完全一致するため、geometry 移行の新しい差とは扱わない。
完全解消は R15 / Phase E の画像 gate に残す。

既述の40回 lifetime 手順を再実行し、Sampler / VAO も release=0 の対象にした。
毎回すべての live object / retired count が0。private memory の測定増分は
OpenGL 0.145 MB/回、Vulkan 0.303 MB/回、両方40/40 PASS。
ログ: `C:/tmp/gp/architecture-glgeometry-final-lifetime40-<backend>.log`。
長時間・他 GPU・全 format の保証とは区別する。

### effect fixture と動く bot の切替を分ける

既存の `FRUITY_SWITCHCHECK=1` は7体の bot も動く。
新旧ボムの粒子数を固定して比較する場合は
`FRUITY_SWITCHCHECK_HOLD_ACTORS=1` で非 main の7体の controls を hold する。
8体の actor、scene、physics / simulation は維持する。通常のユーザー操作には適用されない。
game files / paths.txt を配置済みの build で、設定と環境変数を保存・復元して両方向を実行する手順:

```powershell
$captureRoot = 'C:/tmp/gp/rhi-switch-' + (Get-Date -Format yyyyMMdd-HHmmss)
New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null
$savedSwitch = $env:FRUITY_SWITCHCHECK
$savedRoom = $env:FRUITY_SHOT_ROOM
$savedHold = $env:FRUITY_SWITCHCHECK_HOLD_ACTORS
Push-Location tools/build/out/msvc-Release
$prefsPath = Join-Path $PWD 'launcher.txt'
$hadPrefs = Test-Path -LiteralPath $prefsPath
$prefsBytes = if ($hadPrefs) { [IO.File]::ReadAllBytes($prefsPath) } else { $null }
try {
    $env:FRUITY_SWITCHCHECK = '1'
    $env:FRUITY_SHOT_ROOM = 'AD2 ALINOS PERCH'
    $env:FRUITY_SWITCHCHECK_HOLD_ACTORS = '1'
    foreach ($backend in @('opengl', 'vulkan')) {
        $shots = Join-Path $captureRoot $backend
        'q' | & .\FruityPrime.exe -shellshot $shots -rhi $backend -vkvalidation -fpscap 60 -noupdate *> "$captureRoot-$backend.log"
        if ($LASTEXITCODE -ne 0) { throw "$backend switch fixture failed" }
    }
} finally {
    if ($hadPrefs) { [IO.File]::WriteAllBytes($prefsPath, $prefsBytes) }
    elseif (Test-Path -LiteralPath $prefsPath) { Remove-Item -LiteralPath $prefsPath }
    $env:FRUITY_SWITCHCHECK = $savedSwitch
    $env:FRUITY_SHOT_ROOM = $savedRoom
    $env:FRUITY_SWITCHCHECK_HOLD_ACTORS = $savedHold
    Pop-Location
}
```

`C:/tmp/gp/architecture-glgeometry-held-20261002-074848/`:
両開始 backend とも exit 0、front 切替1回、同じ Alinos Perch で Settings / Apply / Resume の切替3回。
全6回で simulation が進み、definitions=105/105、impact / new bomb / existing bomb は各2 particles、
source は scene 終了時に解放。両最終画像で黄色 impact / 青い bomb core を目視。

動く bot の検査は置き換えない。今回の通常 mode は OpenGL 開始で全3回 PASS。
Vulkan 開始は最終の bomb が0になった試行と、診断付きで全3回 PASS した2試行の両方がある。
`architecture-glgeometry-final-20261002-074551/vulkan.log` などの失敗を残す。
BombEntity の一時的な Destroy 診断は元に戻した。
通常の命中・爆発も起きる code path があるため、現時点で0 particles の原因を確定したり、
動く bot の stress が全試行 PASS と主張したりしない。Phase H で actor / fixture 寿命の観測を拡張する。

## Desktop OpenGL の conventional vertex input 除去（Phase E）

scene に続き、launcher overlay と背景写真を `OpenGlWindowDraw` の
explicit shader / RHI Buffer / Pipeline / CommandList / VAO 経由へ移した。
Skia の native texture は同じ GL context で1回の draw の間だけ借りる。
既存の GPU surface をそのまま sample し、UI の毎フレームの CPU readback / texture copy は追加しない。
背景の移動 shader が利用できない場合の静止画 fallback は維持する。

Desktop `GL.cpp` の conventional array enable / pointer mirror と
native `glColor` / `glNormal` / `glTexCoord` による current-value mirror を削除。
Desktop の頂点位置を Position=0 / Normal=1 / Color=2 / TexCoord=3 / TexCoord1=4 にそろえ、
Vulkan と同じ明示入力にした。array draw 後の未定義な native current value を読まず、
scene が明示した色・法線・UV の値を context device が保持する。
これは scalar の描画状態であり、texture の画素を CPU に複製する仕組みではない。
Android の頂点位置と既存 UI 実装は変更せず、専用 include に保持する。
Android のビルド・実動作は今回未実施。

静的 audit は desktop conventional input の呼び出しと desktop / Vulkan の頂点位置不一致も拒否する。
GL 2.1 向け thumbnail 診断は explicit GLSL 1.20 / generic input を使う。
新しい composite 診断は通常の `-thumbnailwindowcheck` に含め、GL 2.1 専用 mode では実行しない。
GL 2.1 環境そのものは今回検証していない。

### 検証

- MSVC Release build、CTest 5/5、shader interface audit（22 source）が成功。
- `-rhiconformance`: OpenGL / Vulkan とも PASS、release=0、Vulkan validation errors=0。
  ログ `C:/tmp/gp/architecture-phasee-final-rhiconformance.log`。
- `-thumbnailwindowcheck`: explicit shader の色 readback と、実際の window composite を検証。
  半透明赤 / 緑の2行の texture を青背景へ合成し、premultiplied / opaque の色と上下の向きを確認。
  explicit inherited color の維持、buffer / shader / program / sampler / VAO / texture の release=0、
  native error=0。ログ `C:/tmp/gp/architecture-phasee-composite-check.log`。
- Golden Capture の7 candidate を両 backend で撮影し、各 backend の変更前画像と比較。
  OpenGL は `architecture-glgeometry-final-golden-opengl`、Vulkan は
  `architecture-glgeometry-golden-vulkan` を比較元にした。
  新画像は `C:/tmp/gp/architecture-phasee-final-golden-{opengl,vulkan}/`。
  全14画像で decoded RGB の相違 byte=0。比較記録
  `C:/tmp/gp/architecture-phasee-golden-comparison.txt`。
  同一 harness の変更前後の比較であり、両 backend 間の完全一致を新たに主張するものではない。
- `C:/tmp/gp/architecture-phasee-held-20261002-082118/`:
  7体の非 main controls を hold した fixture は両開始 backend で成功。
- `C:/tmp/gp/architecture-phasee-bots-20261002-082625/`:
  7体の bot が動く通常 fixture も、今回は両開始 backend で成功。
  各 process で front 1回、同じ Alinos Perch で Settings / Apply / Resume の切替3回。
  全6回で同じ scene / window geometry / visibility を維持して simulation が進み、
  effect definitions=105/105、impact / new bomb / existing bomb は各2 particles。
  texture-only source は切替中に保持され、scene 終了時に解放。
  両ログに VUID / Validation Error / failed-step はない。
  held fixture の両最終画像で黄色 impact と青い Lockjaw core を目視。
  front 画像で UI と背景写真も確認。
  以前の bot fixture の失敗の原因が今回確定したという記録ではなく、Phase H の観測は継続する。

再実行は上記の切替手順を使う。動く bot の通常 mode では
`FRUITY_SWITCHCHECK_HOLD_ACTORS` を未設定にする。
composite / core GPU / Golden Capture は game binary のディレクトリから実行:

```powershell
Push-Location tools/build/out/msvc-Release
try {
    'q' | & .\FruityPrime.exe -thumbnailwindowcheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Window composite check failed' }
    'q' | & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI conformance failed' }
    foreach ($backend in @('opengl', 'vulkan')) {
        'q' | & .\FruityPrime.exe -goldencapture all -goldendir "C:/tmp/gp/repeat-phasee-$backend" -rhi $backend -vkvalidation -noupdate
        if ($LASTEXITCODE -ne 0) { throw "$backend Golden Capture failed" }
    }
} finally { Pop-Location }
```

この Phase E 時点では、R2 の本番 binding / generated manifest、R4 の OpenGL submission 契約、
R8 の完全な session ownership、R10～R13、R16 / R18 と Phase H の残項目は引き続き対応する。
Metal / D3D12（Phase F / G）と macOS 実機検証は今回の完了条件に含めない。

## OpenGL の実際の submission completion（R4）

`OpenGlFrameScheduler` を独立させ、GL context の stream marker を共通
`SubmissionSerial` に対応させた。frame slot は再利用の順序だけを扱い、
`RetirementQueue` は typed submission token のみを受け取る。
`CommandList::End` と frame 終了は marker を挿入して flush する。
resource release は、それ以前の RHI / native / Skia の仕事を覆う marker を挿入して retire する。
frame を開かずに upload / copy / release した場合も同じ契約を使う。

fence の作成に失敗した場合は番号を消費せず、native error を `BackendError` に保持する。
zero-time poll の timeout は完了ではない。slot の blocking wait は対象 marker だけを待ち、
timeout で device 全体の `Finish` に切り替えない。WAIT_FAILED も完了として扱わない。
通常の resource release は GPU を待たず、明示 `WaitIdle` / device teardown は待機して解放する。
GL 2.1 診断用 context で ARB_sync が使えない場合は、実際の `Finish` による同期 fallback を残す。
sync 対応 context に必要な entry point がない場合は Unsupported として報告する。
この fallback の CPU 契約は検査したが、GL 2.1 実機は未検証。
仕様確認: [glFenceSync](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glFenceSync.xhtml)、
[glClientWaitSync](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClientWaitSync.xhtml)。

現段階では native / Skia の全使用箇所の通知に依存しないよう、resource ごとに保守的な
marker を挿入する。40 cycles の OpenGL では合計69,640 marker になった。
marker の集約や upload / readback の暗黙 driver wait は今後の改善対象であり、
今回の変更を速度向上や完全非同期化の証拠にしない。
`HostWaits` は明示的な scheduler wait の計数で、driver 内部の upload / map の待機時間は含まない。

### 検証と再実行

- MSVC Release build、CTest 5/5、shader interface audit（22 sources）成功。
  CPU dispatch fixture は、未完了 marker の保持、失敗で番号を消費しないこと、
  completed prefix のみ回収、timeout の再試行、WAIT_FAILED 時の保持、明示 idle を検査する。
- `C:/tmp/gp/architecture-glsubmission-final-rhiconformance.log`:
  OpenGL / Vulkan とも PASS。追加した16回のフレーム外 copy / readback は内容一致、
  frame number 不変のまま submission が進み、通常 release の明示 wait は増えない。
  最後の明示 idle で `Completed=Submitted`、live / retired=0。Vulkan validation errors=0。
- `C:/tmp/gp/architecture-glsubmission-final-thumbnail.log`:
  実際の window composite / 色 / 上下方向 / inherited color / release=0 が PASS。
- `C:/tmp/gp/architecture-glsubmission-lifetime40-{opengl,vulkan}.log`:
  Alinos Perch の読み込み・描画・解放が両方40/40 PASS。毎回、全 resource / retired=0、
  `Completed=Submitted`。各 cycle の最後の2 frame の明示 host wait=0。
  最後は OpenGL 69,640/69,640、Vulkan 25,520/25,520 submissions。
  private memory の測定増分はそれぞれ0.0715 / 0.1928 MB/cycle。
  これは CPU process private memory であり、VRAM 使用量や長時間の上限を保証する測定ではない。
- Golden Capture の7候補を両 backend で再撮影。
  `architecture-phasee-final-golden-{opengl,vulkan}` と
  `architecture-glsubmission-golden-{opengl,vulkan}` の全14画像で decoded RGB 差分0。
  記録 `C:/tmp/gp/architecture-glsubmission-golden-comparison.txt`。
  以前からある backend 間の caption 差についての判定は変えない。
- 切替テスト: `C:/tmp/gp/architecture-glsubmission-final-bots-20261002-091241/`。
  両開始 backend で front 1回と、同じ試合内の Settings / Apply / Resume 3回が PASS。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  scene / window geometry / visibility を維持して simulation が進み、texture-only source は
  切替中に保持・scene 終了時に解放。VUID / Validation Error はなし。
- `C:/tmp/gp/architecture-glsubmission-final-held-20261002-091409/`:
  非 main controls を hold した効果 fixture も両開始 backend で PASS。
  全6回で同じ効果数と source 寿命を確認し、両方の最後の画像で impact と bomb の描画を目視。
  通常 bot / held の全4 process で試合前の hunter preview も PASS。

試合前の UI 検査で固定 draw 数の待機だけでは再描画前に先へ進む失敗が出た。
右キーが PlayScreen に届くことを診断で確認したが、その後の map click / hunter preview が
揃わない試行があった。UI の dispatcher / redraw が実時間を使うため、試合前5箇所は
従来の draw 数に加え `frames / 60` 秒の待機も要求する `WaitUi` にした。
クリックや preview の失敗判定は維持し、失敗時は step / surface / candidate bounds と PNG を残す。
失敗ログは `architecture-glsubmission-{bots,held,clickdiag,focusdiag,playdiag-repeat,boundsdiag}-*`
に残してある。動く bot の別の寿命問題がすべて解決したという判定ではない。

repo root の PowerShell、MSVC Release と game files / paths.txt を用意した状態で:

```powershell
ctest --test-dir tools/build/out/msvc-Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CPU contract checks failed' }
python tools/check-phase5-shader-interface.py
if ($LASTEXITCODE -ne 0) { throw 'Shader interface audit failed' }
Push-Location tools/build/out/msvc-Release
try {
    'q' | & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI conformance failed' }
    'q' | & .\FruityPrime.exe -thumbnailwindowcheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Window composite failed' }
    foreach ($backend in @('opengl', 'vulkan')) {
        'q' | & .\FruityPrime.exe -gpulifetime 'AD2 ALINOS PERCH' -cycles 40 -frames 3 -rhi $backend -vkvalidation -noupdate
        if ($LASTEXITCODE -ne 0) { throw "$backend lifetime failed" }
    }
} finally { Pop-Location }
```

Golden Capture は Phase E の手順、切替操作は上記リンク先の再実行手順を使用する。
通常 bot stress は `FRUITY_SWITCHCHECK_HOLD_ACTORS` を未設定、効果だけを隔離する fixture は `1`。
設定ファイルは試験前の byte 列を保管し、終了後に復元する。
Android build / 実機、remote CI、実際の device loss / OOM 故障注入は今回未実行。
R2 / R8 / R10～R13 / R16～R19 などの残項目は引き続き対応する。
