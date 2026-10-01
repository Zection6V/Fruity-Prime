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
| R3: PipelineLayout | 複数グループの記述を値で所有。Vulkan の generic pipeline と `SetBindingSet` が複数グループを扱う。cache key に全グループを含める。OpenGL の binding 実装と scene の生成 shader adapter の整理は残る |
| R4 / R5: submission と resize | 共通 `SubmissionSerial` / `SubmissionProgress` と Vulkan graphics queue の timeline scheduler を追加。Buffer / Sampler / Image / ImageView / Pipeline を実際の送信完了で破棄。texture resize は先に画像・全ビューを確保して交換し、旧世代を retire。通常の破棄・resize から全 GPU 待機を除去。OpenGL の既存 frame fence を共通 submission 契約へ移す作業は残る |
| R6 / R7: OpenGL command / sampler | 未対応。Buffer / Draw / Copy / BindingSet の実装、geometry の command 経由化、独立 sampler が必要 |
| R8: Session 寿命 | Vulkan の意図的に解放しない `VulkanScene` を削除。切替で scene / UI → commands → swapchain → device / context → window の順に解放する。OpenGL は既存 context device を Session の明示終了で解放。所有のさらなる整理・stress の resource count gate は残る |
| R9: presentation | request と実際の mode / capabilities を分離。typed acquire / present status を実装し、frame loop で利用。最小化・明示的 close request は一時停止、API の device / surface loss は別分類。OpenGL の generic conformance coverage は R19 で拡張する |
| R10～R13 | `VulkanFrameScheduler` を独立させ、実際の queue submit / completion を担当。descriptor / frame slot / memory / upload の分離は残る。native pipeline cache / budget / upload ring は未対応 |
| R14: eligibility / admission | Vulkan passive probe は instance / physical device の確認で止まり、logical device / queue を作らない。incoming Session の device / swapchain 生成が active admission。失敗注入による復旧検証は残る |
| R15: GL vertex interface | 未対応。既存 explicit shader input は維持。client array mirror / alias 依存の除去と VAO 整理が必要 |
| R16: readback | 未対応。非同期 ticket と lifetime / backpressure policy が必要 |
| R17: error | Vulkan の device loss / surface loss / OOM を `BackendError` へ分類。presentation は typed status を返す。native code を presentation facade まで保持する改善・故障注入は残る |
| R18: 診断 | 未対応。共通 debug label / timestamp interface が必要 |
| R19 / Phase H | 既存の両方向の試合内切替回帰を維持。共通 conformance、繰り返し session teardown、resize / load / unload の resource baseline・retirement queue gate を拡張する |
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
