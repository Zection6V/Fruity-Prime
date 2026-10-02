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
| R2: shader ABI | `SceneShaderAbi.def` を本番5 programs / 55 constants / 7 textures の共通定義にし、OpenGL texture units と Vulkan 生成 manifest / 実 SPIR-V / native layouts を接続。実バイナリの type・offset・stride・set/binding・vertex location を build 時に検証。std140 packing は Vulkan 内部。単体9/9と固定14画像の一致を確認 |
| R3: PipelineLayout | 複数グループの記述を値で所有。両 backend の generic pipeline / binding を共通 GPU fixture で検証。cache key に全グループを含め、本番 Vulkan scene も Frame / Material / Draw / Post の4 layouts を使う。全 format / failure stress は R19 で続ける |
| R4 / R5: submission と resize | 共通 `SubmissionSerial` / `SubmissionProgress` に Vulkan timeline と OpenGL GLsync scheduler を接続。frame number を retirement の証拠にしない。Vulkan は Buffer / Sampler / Image / ImageView / Pipeline を実際の送信完了で破棄し、resize は先に画像・全ビューを確保して旧世代を retire。OpenGL はフレーム外の command / resource release も実際の stream marker で覆う。GL marker の集約、残る ownership / format stress は後続で扱う |
| R6 / R7: OpenGL command / sampler | Buffer / vertex・index binding / Draw / DrawIndexed / GPU Copy / BindingSet と独立 sampler・value cache を実装。Windows scene / transient geometry を同じ Buffer / CommandList / VAO 経由へ接続。共通 GPU fixture と旧新7画像の一致を確認。単一2D画像以外の範囲、packed depth/stencil copy、recording 契約の統一は残る |
| R8: Session 寿命 | Vulkan の意図的に解放しない `VulkanScene` を削除。切替で scene / UI → commands → swapchain → device / context → window の順に解放する。OpenGL device は Session が単独所有。Vulkan も device 終了時に全 native owner を閉じ、shared state を context 非依存の CPU descriptor にする。両 backend で未送信 copy / 旧 wrapper を残す8回の shutdown / recreate を検査。swapchain は Session 終了前に解放する caller 契約を維持。device loss / admission failure の teardown は R17 / Phase H で続ける |
| R9: presentation | request と実際の mode / capabilities を分離。typed acquire / present status を実装し、frame loop で利用。最小化・明示的 close request は一時停止、API の device / surface loss は別分類。OpenGL の generic conformance coverage は R19 で拡張する |
| R10: Vulkan 責務分離 | `VulkanFrameScheduler` が queue submit / completion、`VulkanDescriptorAllocator` が slot ごとの pools、`VulkanUploadArena` が mapped pages / suballocation / flush / completion 後 reset / close を所有。pipeline library は R11 の専用 owner。frame slot / VMA resource factory / probe の分離は残る |
| R12: memory budget | 未完了。upload の8 MB batch threshold / high water reuse は heap budget admission ではない。cross-backend snapshot / pure decision / telemetry と大きい resource の admission を続ける |
| R13: upload | Vulkan の scene uniforms / transient geometry / texture と GPU-only buffer upload を slot ごとの persistent mapped arena へ統一。描画外の writes は専用 transfer stream で batch し、consumer / frame / readback / release の順序境界で submit。static mesh は GPU-only destination。CPU fake と実 GPU の再利用・コピー順・overflow・切替を検査。将来の API の機構は追加しない |
| R11: native pipeline library | 既存 semantic cache を維持し、専用 `VulkanPipelineCache` を全 RHI native graphics pipeline 生成へ接続。identity / framing / checksum / size gate、atomic disk replacement、driver rejection / native cache 不可時の fallback と deterministic close を実装。CPU fault dispatch と実 GPU の cold / warm・破損・保存失敗を検証。速度向上・cache hit の計測は未実施。OpenGL は既存 linked-program cache、Metal / D3D12 は将来対応 |
| R14: eligibility / admission | Vulkan passive probe は instance / physical device の確認で止まり、logical device / queue を作らない。incoming Session の device / swapchain 生成が active admission。失敗注入による復旧検証は残る |
| R15: GL vertex interface | Windows scene / transient / launcher UI を explicit input と RHI Buffer / CommandList / VAO へ統一。desktop wrapper の conventional array / current-value mirror を除去。本番 GLSL declarations と共通 ABI、実 SPIR-V vertex location の一致を検査。GPU composite / 旧新14画像の一致を確認。既存の backend 間 caption 差は画像 gate に残る |
| R16: readback | 未対応。非同期 ticket と lifetime / backpressure policy が必要 |
| R17: error | Vulkan の device loss / surface loss / OOM を `BackendError` へ分類。presentation は typed status を返す。native code を presentation facade まで保持する改善・故障注入は残る |
| R18: 診断 | 未対応。共通 debug label / timestamp interface が必要 |
| R19 / Phase H | 同じ fixture で両 backend を検証する `-rhiconformance` を追加。lifetime gate に sampler / VAO を追加。操作・新旧 effect の切替 fixture と動く bot の stress を分けて記録。通常の session teardown は両方8回検査。全 format / recording / failure / presentation ownership stress は引き続き拡張する |
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

## OpenGL device の Session 所有と終了後の wrapper（R8）

OpenGL device の process 単位の所有を除去し、各 `BackendSession` の
`unique_ptr<GraphicsDevice>` にした。current context の lookup は借用 pointer だけを保持し、
同じ context を二つの Session が所有しようとした場合は拒否する。
Session 終了時は自分の context を current にして GPU の完了を待ち、native object を解放する。
window が先に閉じられた場合も、その context が有効な間に native state を閉じる。
その後に残った wrapper の destructor は、他の context に対して削除命令を出さない。
pipeline / layout / binding set は session の lifetime token を検査し、
allocator が device の同じアドレスを再利用しても旧世代を新 device と誤認しない。
旧 texture の view 作成・resize、旧 pipeline / binding の利用も拒否する。

linked OpenGL program は pipeline が所有する。shader wrapper を先に解放しても
作成済み executable は描画に使える。Vulkan の作成済み native pipeline も
descriptor に borrowed shader pointer を残さない。deferred scene adapter の所有は R2 で続ける。

Golden Capture の再実行で、既存 harness が7個の窓を順に作り、lazy scene Session を
残している経路を検出した。最初だけ撮影に成功し、次の窓から
`The current context has no OpenGL session` になった。
閉じた context の device は既に無効なので、次の `Session::Device` で新しい device を生成する。
harness の画像判定は変更しない。失敗ログ `C:/tmp/gp/architecture-session-golden-opengl.log`
も保持し、修正後の7画像で回帰を確認した。

### 検証と再実行

- MSVC Release build 成功。CTest 5/5 と shader interface audit（22 sources）成功。
- `C:/tmp/gp/architecture-session-window-rhiconformance.log`:
  両 backend の共通 GPU fixture が PASS。pipeline 作成後に public shader を解放してから
  描画・色・buffer / texture transfer を検査する。
  OpenGL はさらに8回の shutdown / recreate を検査した。
  Buffer / Texture / Renderbuffer / Shader / Program / Sampler / FBO / VAO の名前を
  終了前に `glIs*` で確認し、context を残して Session を終了した後は実際に存在しないことを検査。
  explicit shutdown と window 側の defensive close を交互に使う。
  新 device が旧 logical texture handle を再利用した後に旧 wrapper を破棄し、
  新 resource の数と readback 内容が変わらないこと、旧 binding の拒否も検査する。
  Vulkan validation errors=0。Vulkan の強制 late-wrapper teardown stress はまだ実装していない。
- `C:/tmp/gp/architecture-session-window-thumbnailwindowcheck.log`:
  window composite / 色 / 上下方向 / inherited color / release=0 が PASS。
- `C:/tmp/gp/architecture-session-lifetime40-{opengl,vulkan}.log`:
  Alinos Perch の読み込み・描画・解放が両方40/40 PASS。
  毎回 resource / retired=0、Completed=Submitted、最後2 frame の明示 host wait=0。
  最後は OpenGL 69,640/69,640、Vulkan 25,520/25,520 submissions。
  CPU process private memory の測定増分は0.8984 / 0.4477 MB/cycle。
  前回 R4 の測定より大きく、メモリ改善や長時間安定の証拠とはしない。
  native object の解放 gate と CPU allocator / driver の process memory は分けて観測する。
- `C:/tmp/gp/architecture-session-final-golden-{opengl,vulkan}/`:
  7候補ずつ撮影成功。R4 の `architecture-glsubmission-golden-{opengl,vulkan}` と比較し、
  全14画像の decoded RGB 差分0。
  記録 `C:/tmp/gp/architecture-session-final-golden-comparison.txt`。
- 切替: `C:/tmp/gp/architecture-session-bots-20261002-093038/`。
  動く bot の通常 fixture は両開始 backend で front 1回、同じ試合内3回が PASS。
  同じ scene / geometry / visibility を維持して simulation が進む。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  texture-only source は切替中に保持され、scene 終了時に解放。validation error はない。
- `C:/tmp/gp/architecture-session-final-held-20261002-094140/`:
  窓の再作成修正後も held fixture が両開始 backend で PASS。
  全6回の effect 数と source 寿命は上記と同じ。最後の画像で黄色 impact を確認し、
  OpenGL 開始の画像では青い Lockjaw core も確認した。
  Vulkan 開始の最終画像では手前の hunter が core の一部を遮っている。

再実行は R4 の PowerShell 手順と上記の切替手順を使う。
`-rhiconformance` に OpenGL の8回の終了・再作成検査が含まれる。
Golden Capture は backend ごとに別の出力ディレクトリを指定する。
Android build / 実機、remote CI、device loss / OOM 故障注入は今回未実行。
Metal / D3D12 は将来対応。R8 は Vulkan 側の終了後の wrapper と shared device state の
context 寿命を整理するまで、全体完了とは扱わない。

## Vulkan native state の Session 終了（R8）

前節で残していた `VulkanDeviceState` の raw context 参照を終了境界で切り離した。
resource wrapper が shared state を保持しても native device / context は延命しない。
native owner の登録は借用であり、constructor が失敗した場合も registration は解放される。
終了は記録中の command を submit → GPU 完了 → command list の ring / descriptor pool /
cache と native command pool / fence → shader / pipeline / layout / sampler / image / view /
buffer → frame descriptor pool / fence → VMA allocator / scheduler → context の順。
通常の resource release はこれまでどおり submission retirement を使い、
device 終了時だけ、完了を待った native state を即時解放する。
終了後の texture handle / native name は無効化する。古い commands は recording を拒否し、
新 device は古い pipeline / layout / set / texture / view を受け入れない。
texture が view wrapper より先に破棄される場合も、view の destructor が解放済み texture を参照しない。

`Session::Shutdown` は明示的に context を終了してから validation 件数を保存する。
これにより、`vkDestroyDevice` が報告する未解放 child object も検査結果に含まれる。
swapchain / Skia UI は従来の caller 契約どおり、Session 終了前に解放する。
device loss / OOM の故障注入と、swapchain が caller の終了順を破る場合の検査は後続。

### 検証と再実行

- MSVC Release build 成功。CTest 5/5、shader interface audit（22 sources）成功。
- `C:/tmp/gp/architecture-vksession-final-view-conformance.log`:
  両 backend の共通 GPU fixture と8回ずつの shutdown / recreate が PASS。
  copy を記録して `End` を呼ばず、buffer / texture / view / sampler / shader /
  pipeline / layout / binding set / commands の wrapper を残したまま Session を終了する。
  Vulkan は全 cycle で Khronos validation が有効。
  終了時に copy の submission が進み Completed=Submitted、native owner / resource count /
  retired=0、VMA の終了直前の allocation count=0、context / allocator / scheduler の参照なし。
  shutdown を2回呼び、新 device が旧 logical texture handle を再利用した後に
  古い wrapper を破棄しても、新 buffer の GPU copy / readback 内容と resource count が維持される。
  incoming session で再描画して色が一致し、終了時の validation errors=0。
- 作成済み pipeline の `Desc` は shader の借用 pointer を持たず、値の state を保持する。
  resource 診断の古い pointer identity 判定が失敗したため、この明示契約に合わせて更新した。
  全 state の一致、4 binding groups、異なる state の区別、shader code の内容での cache 再利用、
  不正 pipeline の拒否は引き続き検査する。
  shader wrapper 解放後の実際の描画は共通 conformance fixture で検査する。
- `C:/tmp/gp/architecture-vksession-final-vulkanresourcecheck.log`:
  buffer / image upload / copy / readback / resize、全 shader modules / pipeline state /
  descriptor allocation / frame reuse が PASS、live=0、validation errors=0。
- `C:/tmp/gp/architecture-vksession-final-vulkanpresentcheck.log` と
  `architecture-vksession-final-vulkanpresentfallbackcheck.log`:
  resize / fullscreen / minimize / restore / presentation / shutdown が PASS、validation errors=0。
  fallback-retired-releases=6。
- `C:/tmp/gp/architecture-vksession-final-bots-20261002-095535/`:
  両開始 backend で front 1回、同じ Alinos Perch の Settings / Apply / Resume 切替3回が PASS。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  同じ scene / window geometry / visibility と simulation の継続、source の保持・終了時の解放を確認。
- `C:/tmp/gp/architecture-vksession-final-golden-{opengl,vulkan}/`:
  各7候補の撮影が成功。前節の `architecture-session-final-golden-{opengl,vulkan}` と比較し、
  全14画像の decoded RGB 差分0。
  記録 `C:/tmp/gp/architecture-vksession-final-golden-comparison.txt`。
- `C:/tmp/gp/architecture-vksession-lifetime40-vulkan.log`:
  Alinos Perch 40/40 PASS。全 release の resource / retired=0、Completed=Submitted。
  最後は25,520/25,520 submissions、最後2 frame の明示 host wait=0。
  CPU private memory peak は342→351 MB、0.4365 MB/cycle。
  VRAM / 長時間の上限やメモリ改善の証拠とはしない。

再実行は R4 のコマンドと既存の切替手順を使う。`-rhiconformance` は両 backend の
未送信 copy を残した8回の終了・再作成検査を含む。
途中の `architecture-vksession-{first,second,third}-conformance.log` は保持する。
fixture の readback buffer への CPU upload、copy 前の resource state、cache の明示解放を
修正した最終 fixture が上記ログであり、API の memory usage / state 検査は弱めていない。
Android build / 実機、remote CI、device loss / OOM / admission failure の故障注入は未実行。
Metal / D3D12 は将来対応。R2 / R10～R13 / R16～R19 などの残項目は対応中。

## Vulkan driver pipeline cache（R11）

`VulkanPipelineKey` → shared native pipeline の既存 semantic cache を維持し、
driver の compilation data を `VulkanPipelineCache.hpp/.cpp` の専用 owner に分離した。
scene の variant と共通 RHI pipeline はこの owner の `CreatePipeline` を通る。
生成・data retrieval・保存を mutex で直列化し、device 終了時に保存・破棄する。
cache なしの pipeline 生成も可能で、cache hint の失敗と本来の pipeline 生成エラーは区別する。
実際の pipeline の device loss 等は従来の `BackendError` に伝わり、cache が隠さない。

desktop の保存先は user data directory 内の `render-cache/vulkan-pipelines.bin`。
Windows の portable build では exe の隣になる。検査用の
`FRUITY_VK_PIPELINE_CACHE_DIR` がある場合はそのディレクトリを使う。
Android は同じ native cache owner を使うが、今回 disk persistence は追加しない。
生成データは `.gitignore` の対象。

保存形式は64 byte の明示 little-endian header と、driver が返した payload の組。
magic / schema / header size / vendor / device / driver version / pointer size / cache UUID /
payload length / checksum を検査し、native Vulkan header の identity も検査する。
読み込み・保存 payload の上限は64 MiB。長さの過大申告や途中の data は driver に渡さない。
この checksum は破損検出であり、認証の仕組みではない。
一時ファイルに書いてから atomic replacement し、同時に起動した process 同士が
不完全な data を読むことを避ける。最後の保存が勝つため cache entry の完全な union は保証しない。
保存不能の場合は前の file を維持する。

古い・不一致・破損した保存 data は捨てて空で生成する。
driver が initial data を拒否した場合は空で再試行し、native cache が生成できない場合や
optional cache entry point がない場合は `VK_NULL_HANDLE` で pipeline を生成する。
`vkGetPipelineCacheData` の `VK_INCOMPLETE` は bounded retry し、保存失敗は起動失敗にしない。
`Loaded` は initial data を受け取った cache の生成成功を示すだけで、driver cache hit とは区別する。
契約確認: [cache header](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineCacheHeaderVersionOne.html)、
[data retrieval](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPipelineCacheData.html)、
[native pipeline creation](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateGraphicsPipelines.html)。
参考はレビューが挙げた [melonPrimeDS の cache owner](https://github.com/ag-advania/melonPrimeDS/blob/c4165c87416902bb13b670e3147ecf05988017ed/src/VulkanPipelineCache.h)。

### 検証と再実行

- MSVC Release build、CTest 6/6、shader interface audit（22 sources）成功。
  `FruityPrime.VulkanPipelineCache` は Vulkan headers がある build で実行する CPU dispatch 検査。
  すべての truncation / 各 byte の bit corruption、trailing data、device / driver / UUID / schema /
  pointer size / native header、上限、cold / warm、driver rejection、cache 生成失敗、
  optional entry point 欠如、uncached compile、`VK_INCOMPLETE` retry、保存失敗、
  pipeline device loss の伝達、二つの thread の128生成、二重 close を検査する。
- `C:/tmp/gp/architecture-nativecache-gpu-20261002-101503/{cold,warm}.log`:
  実 GPU の共通 conformance と両 backend の8回の session teardown が PASS。
  cold process の最初は initial-bytes=0、保存は15,869 bytes。
  次の process の最初は initial-bytes=15,869。file は64 byte framing込み15,933 bytes。
  cache が pipeline 生成へ渡り、GPU の描画・readback が成功し、終了時 validation errors=0。
  driver の cache-hit feedback / compile 時間は計測しておらず、速度向上の証拠とはしない。
- 同じ evidence directory の `corrupt.log` / `identity.log` / `truncated.log`:
  実 driver が返した保存 file の破損・driver 不一致・truncation を作り、
  native loading 前に捨てたこと（initial-bytes=0）と resource check の PASS を確認。
  live=0、validation errors=0、各12 native pipeline を生成し185,982 bytes を保存。
- `unwritable.log`: 保存 directory の代わりに regular file を置いて検査。
  saved-bytes=0 でも resource check が PASS、既存 file の内容を維持、validation errors=0。
  native driver の initial-data rejection / cache allocation failure は CPU dispatch で検査し、
  実 GPU の故障として注入したという記録ではない。
- `C:/tmp/gp/architecture-nativecache-final-bots-20261002-101820/`:
  両開始 backend で front 1回、同じ Alinos Perch の Settings / Apply / Resume 切替3回が PASS。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  scene / geometry / visibility と simulation の継続、source の保持・終了時の解放を確認。
- `C:/tmp/gp/architecture-nativecache-final-golden-{opengl,vulkan}/`:
  各7候補を撮影。前節の `architecture-vksession-final-golden-{opengl,vulkan}` と比較し、
  全14画像の decoded RGB 差分0。
  記録 `C:/tmp/gp/architecture-nativecache-final-golden-comparison.txt`。

repo root の PowerShell で、新しい output directory を選んで:

```powershell
ctest --test-dir tools/build/out/msvc-Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CPU contract checks failed' }
$savedCacheDirectory = $env:FRUITY_VK_PIPELINE_CACHE_DIR
$cacheEvidence = 'C:/tmp/gp/repeat-nativecache-' + (Get-Date -Format yyyyMMdd-HHmmss)
New-Item -ItemType Directory -Path $cacheEvidence -Force | Out-Null
Push-Location tools/build/out/msvc-Release
try {
    $env:FRUITY_VK_PIPELINE_CACHE_DIR = "$cacheEvidence/data"
    foreach ($phase in @('cold', 'warm')) {
        'q' | & .\FruityPrime.exe -rhiconformance -noupdate *> "$cacheEvidence/$phase.log"
        if ($LASTEXITCODE -ne 0) { throw "$phase GPU check failed" }
    }
} finally {
    $env:FRUITY_VK_PIPELINE_CACHE_DIR = $savedCacheDirectory
    Pop-Location
}
```

固定画像と試合中の操作は既存の Golden Capture / 切替手順を使う。
Android build / 実機、remote CI、native driver の故障注入は今回未実行。
R2 / R10 / R12 / R13 / R16～R19 などの残項目は引き続き対応する。
Metal / D3D12 は将来対応であり、今回追加していない。


## Vulkan descriptor allocator の分離（R10）

`VulkanDescriptorAllocator.hpp/.cpp` が submission slot ごとの descriptor pool を所有する。
generic `BindingSet` の frame slot と、scene command list の2つの slot が同じ実装を使う。
pool の確保・native descriptor type ごとの capacity admission・overflow・bulk reset・解放を
`VulkanGraphicsDevice.cpp` から取り出した。command / frame の fence と queue submission は
引き続き caller と `VulkanFrameScheduler` が担当し、allocator は queue を待たない。

- native pool を作る前に CPU page storage を reserve する。retention の allocation failure で
  確保済み native pool を失う順序を避ける。
- 1つの layout が要求する全 descriptor counts を確認し、default capacity より大きい
  layout も1つの新しい page に収める。合計 count の uint32 overflow / 不正な type / count=0 は
  native call 前に拒否する。
- `maxSets` と各 type の残 capacity を別々に数える。既存 page の exhaustion / fragmentation
  は次の page を使う。新しい page でも失敗した場合はその native error を返し、
  1回の request で pool を無制限に作る loop にしない。
- host / device OOM を descriptor exhaustion として扱わない。失敗した admission は
  既存 page の所有を変えず、reset failure の途中では allocation を許可しない。
- 成功した queue submit の `SubmissionSerial` を allocator に記録する。
  `ResetAfterCompletion` は timeline の実完了が last use に達していなければ、native reset 前に拒否する。
  frame 数や slot index を完了の代わりにしない。未送信 recording は caller が終了／破棄してから reset する。
- scene / generic の capacity と submission slots は共有しない。page は各 slot の high water まで
  保持し、実完了後に main / overflow とも再利用する。Session teardown で明示的に閉じ、
  late wrapper の破棄から native API を呼ばない。Session witness に page count=0 を追加した。

仕様確認: [pool reset は全 use の完了後](https://docs.vulkan.org/refpages/latest/refpages/source/vkResetDescriptorPool.html)、
[pool exhaustion と system/device memory failure の区別](https://docs.vulkan.org/refpages/latest/refpages/source/vkAllocateDescriptorSets.html)。

### 検証と再実行

MSVC Release build、CTest **7/7**、shader interface audit **22 sources** が成功。
`FruityPrime.VulkanDescriptorAllocator` は native dispatch の CPU fake を使う。
GPU / game files / Vulkan loader を起動せず、実際の driver OOM を注入した証拠とは区別する。

単体検査は type capacity と set capacity の別々の exhaustion、全5 type の大きい layout、
completion 前の reset 拒否、overflow page の同一 native handle 再利用、独立2 slots、
fragmentation / fresh-page failure の bounded growth、create / allocation / reset の native error 保持、
failed admission の所有保持、double close / ended allocator の拒否を含む。

repo root の PowerShell、game files / paths.txt と MSVC Release build 配置済み:

```powershell
ctest --test-dir tools/build/out/msvc-Release --output-on-failure
python tools/check-phase5-shader-interface.py
Push-Location tools/build/out/msvc-Release
try {
    & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI conformance failed' }
    & .\FruityPrime.exe -vulkanresourcecheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Vulkan descriptor/resource regression failed' }
    & .\FruityPrime.exe -gpulifetime 'AD2 ALINOS PERCH' -cycles 40 -frames 3 -rhi vulkan -vkvalidation -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Vulkan lifetime regression failed' }
} finally { Pop-Location }
```

実 GPU は NVIDIA GeForce RTX 5070 Ti、Khronos validation 有効:

- `C:/tmp/gp/architecture-descriptor-final-conformance.log`: 共通 GPU fixture と両 backend の
  未送信 copy / old wrappers を残した Session shutdown / recreate 各8回 PASS。
  Vulkan native descriptor pages を含め解放し、validation errors=0。
- `C:/tmp/gp/architecture-descriptor-resourcecheck.log`: descriptor arrays / alignment / fresh sets /
  overflow pool growth / frame reuse / GPU bind submit、64回の clear / bind / resize / release PASS。
  最後は live=0 / retired=0 / completed=submitted / errors=0、churn device-wide waits=0。
- `C:/tmp/gp/architecture-descriptor-final-bots-20261002-124352/`: 動く bots の Alinos Perch で
  OpenGL 開始 / Vulkan 開始の両方、front screen 1回と同じ試合で Settings 保存 / Resume 各3回 PASS。
  全6回 scene / window geometry / visibility / simulation の進行を維持。
  definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  texture-only source は切替中 alive、scene 解放時に released。両最終画像で黄色 impact / 青い Lockjaw core を目視。
- `C:/tmp/gp/architecture-descriptor-final-golden-{opengl,vulkan}/`: 7ケースずつ capture gate PASS。
  R11 実装後の `architecture-nativecache-final-golden-{opengl,vulkan}` と比較し、全14画像で
  decoded RGB の差0 bytes。比較表は `architecture-descriptor-final-golden-comparison.txt`。
- `C:/tmp/gp/architecture-descriptor-final-lifetime40-vulkan.log`: 40/40 PASS。
  毎回解放後の resource / retired は0、最後の submitted=completed=25520、最後2 frames の host waits=0。
  CPU private memory peak は340→348 MB、0.3961 MB/cycle。VRAM budget や速度向上の計測ではない。

R10 全体は未完了。frame slot / VMA memory / upload の change axis は続けて分離する。
page の high water 保持は admission budget / eviction の完成ではなく、R12 の残作業。
R2 の本番 shader ABI、R13 の upload ring、R16 の非同期 readback、R17 の実 driver 故障注入も残る。
Android build / 実機、remote CI は未実行。Metal / D3D12 は将来対応。

## 本番 shader ABI の接続（R2 / R3 / R15）

`SceneShaderAbi.def` に論理 binding、constant の型と配列数、texture / sampler の組と
OpenGL texture unit を定義する。C++ の共通 ABI header と shader 生成ツールが同じ定義を読む。
native API の buffer offset や std140 padding はここへ持ち込まない。
対象は main / composite / cel / shift / backdrop の5 programs、55 constants、7 textures。

- 生成ツールは本番 `Shaders.cpp` の GLSL declarations と型・配列数・名前を照合する。
  未定義 uniform / vertex attribute、重複 binding、desktop vertex location の相違で停止する。
  OpenGL の sampler 初期化と backdrop も共通 texture unit を使う。
- Vulkan は Frame / Material / Draw / Post の4 descriptor set layouts を使う。
  使わない group の空 layout も保持し、scene pipeline の cache key に全 layout を含める。
  textures は manifest の明示 unit / group / image binding / sampler binding で接続する。
- std140 は Vulkan の生成 adapter が計算する。embed 前に `reflect_scene_spirv.py` が
  コンパイル済み10 SPIR-V modules を独立に読み、実際の type / count / offset /
  array stride / column-major matrix stride / block size / set / binding / vertex input を照合する。
  runtime が使う member table と block 内の table の一致も要求する。
  これは interface の検査であり、SPIR-V の全 instruction validity を検証するツールではない。
  仕様参照: [Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)。
- `VulkanSceneUniforms` は生成 metadata を使う本番 packer。
  不正な型・サイズ・配列幅・重複・範囲外を拒否し、配列の padding と隣接 constant を保持する。
  値が変わった block だけ generation を進める。描画時もその block だけを ring へ書き、
  影響した group に新しい descriptor set を作る。GPU 使用中の set は上書きしない。
  command slot / ring の再利用時には upload / set cache を無効化する。
- 未使用の手書き std140 prototype と raw shader snippets を削除した。
  prototype の検査に代えて、本番 declarations / 生成 metadata / 実 packer / 実 SPIR-V を検査する。
  gameplay / AI / effect lifetime の判定は変更しない。

### 単体検査と再実行

MSVC Release build、CTest **9/9**、shader interface audit **20 sources** が成功。
22→20 は未使用の prototype snippets 2つの削除によるもので、本番 shader の削減ではない。

`FruityPrime.VulkanSceneUniforms` は全5 programs / 55 constants を本番 packer へ書き、
実 offset / stride / matrix order、配列の上限、padding / 隣接値保持、block ごとの generation、
同じ値で再 upload しないこと、不正 metadata / write の拒否を検査する。GPU は不要。
`FruityPrime.SceneShaderAbiDrift` は生成済みの実10 modules を使い、set / binding /
offset / array・matrix stride / vertex location / type / count / image dimension /
entry stage / binary framing / manifest / stale schema を壊して拒否を確認する7ケース。
OpenGL-only build の `ShaderInterface` も本番 GLSL と共通定義を照合する。

repo root の PowerShell、MSVC Release build と game files / paths.txt 配置済み:

```powershell
ctest --test-dir tools/build/out/msvc-Release --output-on-failure
python tools/check-phase5-shader-interface.py
Push-Location tools/build/out/msvc-Release
try {
    & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI conformance failed' }
    & .\FruityPrime.exe -vulkanresourcecheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Vulkan resource regression failed' }
    & .\FruityPrime.exe -gpulifetime 'AD2 ALINOS PERCH' -cycles 40 -frames 3 -rhi vulkan -vkvalidation -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Vulkan lifetime regression failed' }
    foreach ($backend in @('opengl', 'vulkan')) {
        $captureDirectory = "C:/tmp/gp/sceneabi-repeat-$backend-$(Get-Date -Format yyyyMMdd-HHmmss)"
        & .\FruityPrime.exe -goldencapture all -goldendir $captureDirectory -rhi $backend -vkvalidation -noupdate
        if ($LASTEXITCODE -ne 0) { throw "$backend capture failed" }
    }
} finally { Pop-Location }
```

Settings 保存 / Resume を使う切替は
[既存の Windows 再実行手順](old/Fruity-Prime-CPP-Renderer-Hot-Switch-Fix-2026-10-01.md#repeating-the-effect-regression-on-windows)
を使う。settings file と環境変数を保存・復元する。通常の動く bots の試験と
`FRUITY_SWITCHCHECK_HOLD_ACTORS=1` の effect fixture を区別して記録する。
診断ログには新旧 bomb entity の生存・flags・countdown を追加した。
粒子が0のとき、描画だけが消えたのか entity が通常の試合処理で終わったのかを追えるようにする。

### 今回の実 GPU 結果

NVIDIA GeForce RTX 5070 Ti、Khronos validation 有効。

- `C:/tmp/gp/architecture-sceneabi-final-conformance.log`: 共通 GPU fixture と
  両 backend の未送信 copy / old wrappers を残した shutdown / recreate 各8回 PASS。
- `C:/tmp/gp/architecture-sceneabi-final-resourcecheck.log`: 実4 group layouts と
  descriptor / resource churn 64回 PASS。live / retired / validation errors=0。
- `C:/tmp/gp/architecture-sceneabi-final-golden-{opengl,vulkan}/`: 7ケースずつ PASS。
  直前の descriptor allocator 実装後画像と比べ、全14画像の decoded RGB 差0 bytes。
  比較表は `C:/tmp/gp/architecture-sceneabi-final-golden-comparison.txt`。
- `C:/tmp/gp/architecture-sceneabi-final-botdiag-20261002-133151/`: 動く bots の Alinos Perch で
  両 backend 開始、front screen 各1回と同じ試合の切替各3回 PASS。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  新旧 bomb entity は alive、flags=0。scene / window geometry / visibility を維持し、
  simulation frame が進む。texture-only source は切替中 alive、scene 解放時 released。
- `C:/tmp/gp/architecture-sceneabi-final-held-20261002-132946/`: actor controls を止めた
  fixture も両方向・全6切替 PASS。最終画像で黄色 impact と青い Lockjaw core を目視。
- `C:/tmp/gp/architecture-sceneabi-final-lifetime40-vulkan.log`: 40/40 PASS、毎回
  resource / retired=0、最後の completed=submitted=25520、最後2 frames の host waits=0。
  CPU private memory peak 342→345 MB、0.151 MB/cycle。VRAM budget / 性能向上の測定ではない。
- `C:/tmp/gp/architecture-sceneabi-final-vulkanpresentcheck.log` と
  `architecture-sceneabi-final-vulkanpresentfallbackcheck.log`: resize / fullscreen /
  minimize / restore / shutdown PASS、errors=0、fallback-retired-releases=6。

最初の動く bots の試験 `C:/tmp/gp/architecture-sceneabi-final-bots-20261002-132448/` は
OpenGL 開始が PASS、Vulkan 開始の最後だけ新旧 bomb の particles / elements が0で exit 1。
その時も definitions=105/105、impact=2、simulation / scene は継続し、validation error はない。
以前の動く bots stress でも出た観測であるが、今回は失敗時の entity 状態を記録していないため
原因を断定できない。failed log は保持する。後続の通常 trial と held fixture の成功で
この失敗が解決済みとは扱わず、Phase H の actor / effect lifetime stress の追跡に残す。

R10 の frame slot / memory / upload 分離、R12 budget、R13 upload ring、R16 async readback、
R17～R19 の故障・診断・ownership stress などは引き続き残る。
Android build / 実機と remote CI は未実行。Metal / D3D12 は今回追加しない。

## Vulkan persistent upload の統一（R10 / R13）

`VulkanUploadArena.hpp/.cpp` が1つの submission slot の staging pages を所有する。
scene constants / transient geometry / texture upload と、描画外の buffer / texture upload が
同じ page allocation / alignment / flush / completion / close の実装を使う。
queue / command pool / fence は caller、実際の submission serial は `VulkanFrameScheduler` が所有する。

- VMA の `HOST_ACCESS_SEQUENTIAL_WRITE | MAPPED` で page を一度確保する。
  CPU pointer は page の寿命中保持し、upload ごとの map / unmap を行わない。
  native admission 前に CPU page retention を reserve し、不完全な mapping は native page を解放して拒否する。
  これは GPU への転送用 staging であり、renderer 切替の復元用 texture backup を新設したものではない。
- size / alignment / reserved-byte overflow を確認し、任意 alignment と page overflow を扱う。
  大きい request は十分な1 page に収める。dirty ranges を page ごとにまとめて submit 前に flush し、
  部分的な flush failure は残りの dirty range を保持する。
- 成功した submit の serial を記録する。実 completion より早い reset / allocation は拒否する。
  完了後は default / overflow pages を再利用し、Session 終了で全部閉じる。
  caller は unsubmitted recording を終了／破棄してから reset、GPU を drain／破棄してから close する。
- 描画中の GPU-only buffer / texture write は、その位置の command stream に copy を挿入する。
  描画外では device が専用 transfer command list を保持し、小さい writes をまとめる。
  8 MB の batch threshold、別 command list の開始、frame 終了、readback、resource release、
  Session teardown が submit の境界。ring は2 slots を交換し、再利用時に必要な completion を待つ。
  upload ごとに staging Buffer / temporary CommandList を作る fallback を除去した。
- `CpuToGpu` destination 自体も persistently mapped にする。static mesh の vertex / index destination は
  `GpuOnly + TransferDst` にし、copy 後に vertex / index state へ transition する。
  static mesh ごとに mapped host destination を保持する構成は採用しない。
- transfer command list の lifetime count は scene command list と区別する。
  transfer stream の保持で scene の window target 解放を妨げない。
  Session teardown witness は upload command lists / pages の0も要求する。

仕様確認: [VMA persistent mapping / flush](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html)。
VMA が non-coherent atom alignment を扱う。queue submission の後も memory を mapped のまま保持できる。
GPU-only は配置の意図であり、UMA / BAR も含め、常に CPU から不可視の heap に置かれるとの保証ではない。

### 検証と再実行

MSVC Release build、CTest **10/10**、shader interface audit **20 sources** が成功。
`FruityPrime.VulkanUploadArena` は native API / GPU を起動しない dispatch fixture。
任意 alignment、slice address / byte の保持、oversized admission、default / overflow page の再利用、
completion 前の拒否、独立2 slots、create / mapping / partial flush failure、dirty range の再試行、
double close / late calls を検査する。実 driver の OOM 注入とは区別する。

repo root の PowerShell、MSVC Release build と game files / paths.txt 配置済み:

```powershell
ctest --test-dir tools/build/out/msvc-Release --output-on-failure
python tools/check-phase5-shader-interface.py
Push-Location tools/build/out/msvc-Release
try {
    & .\FruityPrime.exe -vulkanresourcecheck -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'upload/resource regression failed' }
    & .\FruityPrime.exe -rhiconformance -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'RHI ownership regression failed' }
    & .\FruityPrime.exe -gpulifetime 'AD2 ALINOS PERCH' -cycles 40 -frames 3 -rhi vulkan -vkvalidation -noupdate
    if ($LASTEXITCODE -ne 0) { throw 'Vulkan lifetime regression failed' }
} finally { Pop-Location }
```

固定 Golden Capture と Settings 保存 / Resume は直前の shader ABI 節の再実行手順を使う。
GPU resource check は64回の buffer / texture writes で warmup 後の page creation が増えないこと、
小さい writes が1つの stream にまとまること、recording 中の更新前後のコピーが各々正しいことを検査する。
8 MB 超の upload も2 slots の warmup 後は page creation が増えず、全 bytes が readback と一致することを要求する。
通常 upload の device-wide waits が増えないことも要求する。readback 自体の同期化は R16 の残作業。

実 GPU は NVIDIA GeForce RTX 5070 Ti、Khronos validation 有効:

- `C:/tmp/gp/architecture-uploadarena-staticgpu-vulkanresourcecheck.log`: 上記 upload gate と
  64回の clear / descriptor bind / resize / release PASS、live / retired / validation errors=0。
  RGB32Float の sampled image はこの GPU の指定 usage では未対応と native query が返すため、
  実 GPU float texture は RGBA32Float で検査した。12-byte alignment は CPU fixture で検査する。
  RGB32Float の実 transfer 成功を主張しない。
- `C:/tmp/gp/architecture-uploadarena-staticgpu-rhiconformance.log`: 共通 GPU fixture、両 backend の
  pending copy / old wrappers を残した shutdown / recreate 各8回 PASS。
  upload pages と VMA allocations を閉じ、validation errors=0。
- `C:/tmp/gp/architecture-uploadarena-staticgpu-golden-{opengl,vulkan}/`: 各7ケース PASS。
  shader ABI 接続後の直前画像と比較し、全14画像の decoded RGB 差0 bytes。
  比較表は `C:/tmp/gp/architecture-uploadarena-staticgpu-golden-comparison.txt`。
- `C:/tmp/gp/architecture-uploadarena-staticgpu-bots-20261002-140708/`: 動く bots の Alinos Perch で
  両 backend 開始、front screen 各1回と同じ試合の Settings 保存 / Resume 各3回 PASS。
  全6回 definitions=105/105、impact / new bomb / existing bomb 各2 particles。
  新旧 bomb entity は alive / flags=0。scene / window geometry / visibility を保持し、
  simulation frame は進む。texture-only source は切替中 alive、scene 解放時 released。
  最終画像で黄色 impact / 青い Lockjaw core を目視した。validation error はない。
- `C:/tmp/gp/architecture-uploadarena-staticgpu-lifetime40-vulkan.log`: 40/40 PASS、毎回
  resource / retired=0、最後の completed=submitted=320、最後2 frames の host waits=0。
  CPU private memory peak 330→332 MB、0.0869 MB/cycle。
  この fixture の送信数は直前の25520から320へ減ったが、fps / wall time / VRAM budget の改善を測った結果ではない。

途中の実装では static mesh を `CpuToGpu` の mapped destination として保持していた。
その40回 gateは counts / trend とも PASS したが private peak は574→577 MBで、
絶対量が大きかった。`C:/tmp/gp/architecture-uploadarena-batched-lifetime40-vulkan.log` に保持する。
static mesh を GPU-only destination に変更した最終構成の330→332 MBと区別し、
安定した count / trend だけでメモリ使用量が改善したとの判断はしない。

R10 の frame slot / VMA memory factory、R12 の budget admission、R16 の async readback、
R17～R19 の故障・診断・ownership / format stress は残る。
upload の page high water 保持は全 workload の memory budget / eviction 保証ではない。
Android build / 実機と remote CI は未実行。Metal / D3D12 は将来対応。
