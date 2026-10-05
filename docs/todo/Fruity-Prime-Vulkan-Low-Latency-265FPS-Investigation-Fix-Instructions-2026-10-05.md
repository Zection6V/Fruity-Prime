# Fruity-Prime C++ Vulkan Low Latency `ON` 265 FPS 付近頭打ち調査・修正指示

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop3_rendering`
- 調査固定 HEAD: `edf9bb3547069916cce0f81899836bbf7e8afe23`
- 調査日: 2026-10-05
- 対象: C++ / Vulkan / NVIDIA Reflex (`VK_NV_low_latency2`)
- 事象: `Low Latency = On` 時に FPS が約 265 FPS 付近で頭打ちに見える

---

# 1. 結論

## 1.1 `265 FPS` を直接指定する Fruity-Prime 側の固定 cap は見つからない

現行 C++ 実装を固定 HEAD で追跡した結果、`265`、`~265 Hz`、`~3.77 ms` をハードコードしている経路は確認できなかった。

Qt Settings の FPS limit 候補も以下であり、265 FPS は存在しない。

```text
Display (VSync)
30
60
75
90
100
120
144
165
180
200
240
Unlimited
```

`FrameTiming::Unlimited = -1` のとき、`RenderWindow::ApplyFrameRateSettings()` は NVIDIA Reflex へ

```cpp
minimumIntervalUs = 0
```

を渡す。

したがって **FPS limit が `Unlimited` であるにもかかわらず 265 FPS 付近へ張り付く場合、Fruity-Prime の通常の FPS cap 設定が 265 を作っている可能性は低い。**

---

## 1.2 native NVIDIA Reflex 使用時は generic presentation limiter も無効化されている

`ResolveLowLatency()` は NVIDIA provider を

```text
PacingAuthority::Native
```

として扱う。

`PresentationScheduler::Deadline()` は authority が Native の場合、

```cpp
return now;
```

となるため、共通 `SleepForPresentation()` は待たない。

さらに `RenderWindow::BeforeFrame()` も Native authority では generic の

```cpp
WaitForLatestSubmission(...)
```

へ進まず、その frame の pacing authority を Reflex 側へ一本化している。

つまり現行コードには少なくとも、

```text
generic frame limiter
+
generic PresentationScheduler
+
NVIDIA Reflex
```

という明白な二重 pacing は確認できない。

---

## 1.3 現時点の本命は `VK_NV_low_latency2` の native pacing

現行フレーム開始経路は概念的に以下。

```text
RenderWindow::BeforeFrame()
  ↓
VulkanSwapchain::BeginLowLatencyFrame()
  ↓
VulkanNvidiaReflex::BeginFrame()
  ↓
vkLatencySleepNV()
  ↓
vkWaitSemaphores(Reflex timeline)
  ↓
fresh input sample
  ↓
simulation
  ↓
render / submit
  ↓
present
```

`Low Latency = On` ではこの native Reflex pacing が毎 application-rendered frame で有効になる。

したがって 265 FPS が ON 時だけ出る場合、最も疑うべき箇所は

```text
vkLatencySleepNV
+
その sleep semaphore の signal 時刻
+
NVIDIA driver / WSI / display pacing
```

であり、単純な `FrameRateCap=265` ではない。

---

# 2. 265 FPS という数字の意味

265 FPS の frame time は約:

```text
1 / 265 s
≈ 3.7736 ms
≈ 3774 us
```

もし Fruity-Prime が明示 cap として 265 を Reflex へ渡しているなら、

```cpp
minimumIntervalUs ≈ 3774
```

になるはず。

しかし通常の `Unlimited` は:

```cpp
cap = -1
intervalUs = 0
```

である。

したがって実機ログで、

```text
minimumIntervalUs=0
```

なのに 265 FPS 前後へ収束するなら、

> 265 はアプリが指定した FPS cap ではなく、native pacing の結果

と判定できる。

逆に `minimumIntervalUs` が 0 でなければ、まず cap propagation を修正する。

---

# 3. NVIDIA / Vulkan 仕様との照合

## 3.1 `minimumIntervalUs`

最新 Vulkan specification の `VK_NV_low_latency2` では、

```text
minimumIntervalUs
```

は `vkLatencySleepNV` が swapchain の `vkQueuePresentKHR` 間隔として強制する値。

よって:

```text
Unlimited
    minimumIntervalUs = 0

explicit 240 FPS
    minimumIntervalUs = ceil(1,000,000 / 240)
                      = 4167 us
```

という現行 Fruity-Prime の mapping 自体は妥当。

---

## 3.2 Reflex sleep の正しい位置

Vulkan specification は `vkLatencySleepNV` を fresh input sampling より前、present 間に一度呼ぶ構成を要求している。

現行 Fruity-Prime は `BeforeFrame()` から native sleep を完了させた後に input sample へ進むため、配置思想は仕様に合っている。

したがって FPS を戻すだけのために、

```text
sleep を input 後へ移動
sleep を無視
sleep を worker に投げて待たない
On 表示のまま実質 Reflex を無効化
```

という修正は禁止。

それを行うと FPS 表示だけは戻っても NVIDIA Reflex ではなくなる。

---

## 3.3 G-SYNC + VSync + Reflex の driver-side cap

NVIDIA は公式 latency guide で、

```text
G-SYNC
+
VSync ON
+
NVIDIA Reflex
```

の組み合わせでは、VSync backpressure を避けるため **driver が refresh rate より下へ自動的に frame rate を制限する** と説明している。

したがって、NVIDIA Control Panel 側または別レイヤーで VSync が強制されている場合、

```text
Fruity requested present mode = Immediate
```

だけでは「完全に uncapped」と断定できない。

265 FPS が display refresh より少し下にある場合、この driver policy も必ず A/B 対象に含める。

ただし現在の調査資料には、この実機の monitor refresh rate / G-SYNC / NVIDIA Control Panel VSync 状態を確定できる記録がないため、**265 = G-SYNC cap と断定してはいけない。**

---

# 4. melonPrimeDS との比較

`ag-advania/melonPrimeDS` では、同じ RTX 5070 Ti / Vulkan Reflex 系統で既に非常に近い現象を実測している。

公開されている調査記録では、focus-controlled 条件で概ね:

```text
Vulkan Reflex Off
    ~534 FPS

Vulkan Reflex On
    ~344-348 FPS
```

となり、`vkWaitSemaphores` 自体は非常に短く、native Reflex sleep 周辺で frame cadence 相当の待ちが発生していた。

重要なのは、

> melonPrimeDS でも「Reflex On が単純な app FPS limiter を設定している」のではなく、native Vulkan Reflex pacing 有効化後に FPS が大きく低下した

という点。

Fruity-Prime は現在、melonPrimeDS と同じく

```text
VK_NV_low_latency2
VkSwapchainLatencyCreateInfoNV
vkSetLatencySleepModeNV
vkLatencySleepNV
dedicated timeline semaphore
input-before pacing
present ID
submission attribution
latency markers
```

を持つため、今回の 265 FPS も同系列である可能性が高い。

---

# 5. ただし「driver の仕様」で片付けてはいけない理由

NVIDIA の現在の Streamline SDK FAQ は Reflex の FPS impact について、目安として:

```text
Reflex On        <= 約4%
On + Boost       <= 約7%
```

を示している。

Fruity-Prime の `Off` が仮に 800～1000 FPS 級で、`On` が 265 FPS に落ちるなら、これは 4% の範囲ではない。

よって、

```text
「ReflexだからFPSが落ちるのは正常」
```

で CLOSED にしてはいけない。

以下を分離して根本原因を確定する必要がある。

```text
A. Fruity explicit frame interval
B. generic pacing の残存
C. present mode / VSync / G-SYNC policy
D. vkLatencySleepNV call 自体
E. sleep semaphore wait
F. incorrect frame attribution / marker span
G. swapchain latency opt-in
H. driver 固有挙動
```

---

# 6. 現行 source 監査結果

## 6.1 Qt FPS limit UI

`src/MphRead.Native.Qt/Shell/SettingsModel.cpp`

固定 HEAD の 86～92 行付近:

```cpp
const std::array<FpsLimitStop, 13> FpsLimitStops{{
    {"Display (VSync)", Render::FrameTiming::DisplayRate},
    {"30 fps", 30}, {"60 fps", 60}, {"75 fps", 75}, {"90 fps", 90},
    {"100 fps", 100}, {"120 fps", 120}, {"144 fps", 144},
    {"165 fps", 165}, {"180 fps", 180}, {"200 fps", 200},
    {"240 fps", 240}, {"Unlimited", Render::FrameTiming::Unlimited}
}};
```

265 なし。

---

## 6.2 FPS cap internal representation

`src/MphRead.Native/Mods/Render/FrameTiming.hpp`

```cpp
DisplayRate = 0
Unlimited   = -1
MinCap      = 30
MaxCap      = 500
```

`FrameTiming.cpp` でも `Unlimited` は `-1` のまま保持。

---

## 6.3 Reflex interval mapping

`src/MphRead.Native/Renderer.cpp`

`RenderWindow::ApplyFrameRateSettings()`:

```cpp
const std::int32_t cap =
    Mods::Diagnostics::FramePerformance::EffectiveCap(
        Mods::Render::FrameTiming::FrameRateCap());

_swapchain->ConfigureLowLatency(
    Mods::Launcher::LauncherPrefs::LowLatency(),
    cap > 0
        ? static_cast<std::uint32_t>(
            (1'000'000ULL + cap - 1) / cap)
        : 0);
```

従って:

```text
Unlimited (-1) -> 0 us
DisplayRate (0) -> 0 us
240 -> 4167 us
144 -> 6945 us
```

265 固定なし。

---

## 6.4 Native authority では generic scheduler deadline 無効

`src/MphRead.Native/NativeRuntime/Rhi/PresentationScheduler.hpp`

```cpp
if (_configuration.authority == PacingAuthority::Native || !_hasDeadline)
    return now;
```

したがって Qt main loop の:

```cpp
SleepForPresentation(_presentation.Deadline(now));
```

は native Reflex 時には実質 no-op。

---

## 6.5 Native authority では generic one-frame budget wait も使わない

`RenderWindow::BeforeFrame()`:

```cpp
if (!_swapchain->BeginLowLatencyFrame())
    return false;

state = ResolveLowLatency(...);

if (state.effective == LowLatencyMode::Off)
    return true;

if (state.authority == PacingAuthority::Native)
    return true;

return SceneDevice().WaitForLatestSubmission(...);
```

二重 authority は確認できない。

---

## 6.6 Reflex native mode

`src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanNvidiaReflex.cpp`

```cpp
info.lowLatencyMode =
    _mode != LowLatencyMode::Off;

info.lowLatencyBoost =
    _mode == LowLatencyMode::OnBoost;

info.minimumIntervalUs =
    _mode == LowLatencyMode::Off ? 0 : _interval;
```

`On` では native NVIDIA Reflex が実際に有効。

---

## 6.7 Native frame admission

同ファイル:

```cpp
vkLatencySleepNV(...)
vkWaitSemaphores(...)
```

を frame 開始時に実行。

`vkWaitSemaphores` は 2 ms timeout の bounded poll だが、timeout 後は同じ Reflex sleep を再発行せず、同じ timeline value を次の `BeforeFrame()` で再 poll する。

この設計そのものは

```text
one Reflex sleep request per frame
```

を維持している。

ただし実際に何 ms を失っているかの telemetry が不足している。

---

# 7. Frame attribution 監査

ここは今回追加で確認した。

## 7.1 Scene submit は Reflex present ID を付与している

`VulkanGraphicsDevice.cpp` で `VulkanFrameScheduler` へ:

```cpp
return v.reflex
    ? v.reflex->SubmissionId()
    : ...
```

を渡している。

`VulkanFrameScheduler::Submit()` は revision 3 以上で:

```cpp
VkLatencySubmissionPresentIdNV
```

を各 RHI queue submit へ chain する。

したがって「final present submit だけ ID が付いて scene submit が無関係」という単純な欠陥ではない。

---

## 7.2 RenderSubmit marker span

最初の scene-side scheduler submit で:

```text
RENDERSUBMIT_START
```

を発行。

marker は frame 内で de-duplicate される。

最終 swapchain submit 後に:

```text
RENDERSUBMIT_END
```

を発行するため、conceptually:

```text
first application queue submit
  ↓
...
all scene submissions
...
  ↓
final presentation submission
```

を span している。

この構造は合理的であり、現時点で明白な marker ordering bug は確認できない。

ただし driver pacing への影響を完全に否定するには A/B が必要。

---

# 8. 最優先修正: Reflex pacing telemetry を追加する

現状の:

```text
sleepCalls
waitCalls
modeCalls
markerCalls
```

だけでは、

```text
どこで 3.7 ms 消えたか
```

が分からない。

`FRUITY_RENDER_METRICS=1` の opt-in 時だけ、少なくとも以下を追加する。

## 8.1 `VulkanNvidiaReflex` 追加計測

追加 counters:

```cpp
struct ReflexPacingStatistics
{
    uint64_t sleepCallCount;
    uint64_t sleepCallNanoseconds;

    uint64_t waitCallCount;
    uint64_t waitCallNanoseconds;

    uint64_t waitTimeoutCount;

    uint64_t frameAdmissionCount;
    uint64_t frameAdmissionNanoseconds;
};
```

可能なら aggregate だけでなく ring buffer / histogram を持ち:

```text
p50
p95
p99
max
```

を 120 completed frames ごとに出す。

必須計測区間:

```text
T0
vkLatencySleepNV
T1
vkWaitSemaphores
T2
frame ready
```

出力:

```text
reflex_sleep_call_us
reflex_wait_us
reflex_admission_us
reflex_wait_timeouts
```

---

# 9. 必須 runtime provenance log

同じ 120-frame report に以下を必ず含める。

```text
requested_low_latency_mode
effective_low_latency_mode
provider
pacing_authority

frame_rate_cap
minimum_interval_us

requested_present_mode
resolved_present_mode

qt_screen_refresh_hz

swapchain_image_count
VK_NV_low_latency2_revision

reflex_frame_id
completed_frames
abandoned_frames
```

例:

```text
[reflex-pacing]
mode=On
provider=Nvidia
authority=Native
cap=unlimited
minimum_interval_us=0
present_requested=Immediate
present_actual=Immediate
screen_hz=...
sleep_call_p50_us=...
wait_p50_us=...
admission_p50_us=...
fps=...
```

この1行だけで「265 が誰の cap か」をかなり絞れるようにする。

---

# 10. 判定ルール

## Case A — `minimumIntervalUs ~= 3774`

これはアプリ側 explicit cap。

修正対象:

```text
FrameTiming
settings persistence
benchmark override
ApplyFrameRateSettings
```

を追う。

`Unlimited` なのに非ゼロ interval が出た場合は **P0 bug**。

Acceptance:

```text
Unlimited
    minimumIntervalUs == 0
```

を unit / integration test で固定。

---

## Case B — `minimumIntervalUs=0` + `present_actual=FIFO`

アプリまたは swapchain resolution が VSync path へ入っている。

調査:

```text
FrameRateCap が DisplayRate に戻っていないか
present-mode fallback
surface present mode capability
runtime setting apply
renderer switch後の stale mode
```

Unlimited 時の要求は:

```text
Immediate
```

であるべき。

surface が Immediate をサポートする環境では:

```text
requested=Immediate
actual=Immediate
```

を acceptance にする。

---

## Case C — `minimumIntervalUs=0` + `Immediate` + Reflex admission 約3.7 ms

これが今回の本命。

この場合、265 FPS は Fruity の explicit cap ではなく

```text
VK_NV_low_latency2 native pacing
```

から発生している。

さらに:

```text
sleep call
wait semaphore
```

のどちらが時間を持っているかで分岐する。

---

## Case D — app Immediate だが G-SYNC/VSync driver policy が疑わしい

実機 A/B:

```text
1. NVIDIA Control Panel VSync Off
2. G-SYNC/VRR Off
3. 両方通常設定
```

で同一 binary / scene / resolution / window mode を比較する。

ここは source を変更して「直した」扱いにしない。

NVIDIA が documented している driver-side auto-cap と一致するなら、その挙動をアプリ内 fixed 265 cap と誤分類しない。

---

# 11. Developer-only A/B hooks を追加

原因切り分け用。

通常ユーザー設定には出さない。

## 11.1 Swapchain latency opt-in A/B

現在 `VulkanSwapchain.cpp` は extension available なら mode Off でも:

```cpp
VkSwapchainLatencyCreateInfoNV
latencyModeEnable = VK_TRUE;
```

を chain する。

以下の developer-only env を追加:

```text
FRUITY_REFLEX_DISABLE_SWAPCHAIN_LATENCY_MODE=1
```

true のときだけ `VkSwapchainLatencyCreateInfoNV` を chain しない。

目的:

```text
swapchain latency opt-in 自体が cadence を変えているか
```

を On / Off 両方で比較。

**既定値は必ず現在どおり enabled。**

---

## 11.2 Explicit submission attribution A/B

revision 3 以上では現在:

```cpp
VkLatencySubmissionPresentIdNV
```

を付ける。

developer-only:

```text
FRUITY_REFLEX_DISABLE_EXPLICIT_SUBMISSION_ID=1
```

で nonzero explicit attribution だけを止め、implicit attribution control を取れるようにする。

目的:

```text
incorrect attribution / driver interaction
```

の切り分け。

production default を変更してはいけない。

---

## 11.3 Native sleep bypass は diagnostic のみ

どうしても必要なら:

```text
FRUITY_REFLEX_DIAGNOSTIC_BYPASS_SLEEP=1
```

を developer build / metrics build 限定で追加可能。

ただしこの arm は:

```text
Reflex OFF 相当の性能対照
```

であり、

```text
Reflex On の修正版
```

ではない。

UI の On と組み合わせて shipping してはいけない。

---

# 12. A/B matrix

同じ binary / same source SHA で行う。

最低限:

| Run | Low Latency | FPS limit | Present | Diagnostic |
|---|---|---|---|---|
| A0 | Off | Unlimited | Immediate | none |
| A1 | On | Unlimited | Immediate | none |
| A2 | On + Boost | Unlimited | Immediate | none |
| B1 | On | Unlimited | Immediate | disable swapchain latency opt-in |
| B2 | On | Unlimited | Immediate | disable explicit submission ID |
| B3 | On | Unlimited | Immediate | diagnostic sleep bypass |
| C1 | On | 240 | Immediate | none |
| C2 | On | 144 | Immediate | none |
| D1 | On | Display | FIFO | none |

各 run で:

```text
FPS median
frame p50/p95/p99
sleep-call p50/p95
sleep-wait p50/p95
admission p50/p95
present p50/p95
GPU scene ms
minimumIntervalUs
actual present mode
screen Hz
```

を記録。

---

# 13. 実行中フォーカスを固定する

melonPrimeDS の過去調査では foreground / background で Reflex sleep と FPS が大きく変わり、誤判定につながった。

したがって正式 A/B は必ず:

```text
same window focus
same scene
same camera
same resolution scale
same FPS counter state
same HUD
same NVIDIA settings
same window mode
same duration
```

で行う。

run 中に build / shader compile / 別 GPU workload を走らせない。

---

# 14. Production 修正判断

## 14.1 Fruity bug が見つかった場合

以下のいずれかが見つかったら source fix を実施。

```text
Unlimited なのに minimumIntervalUs != 0
generic PresentationScheduler が Native と同時に待つ
generic GPU budget wait が Native と同時に待つ
Immediate 要求なのに stale code で FIFO を選ぶ
Reflex sleep を1 frameで複数発行
present ID が frame 間で再利用
scene submits に異なる present ID を付与
marker order が逆転/欠落
```

修正後は unit test で再発防止。

---

## 14.2 Native Reflex 自体が大幅 FPS loss を作る場合

以下が成立:

```text
minimumIntervalUs = 0
present_actual = Immediate
generic wait = 0
frame attribution valid
markers valid
native Reflex admission が FPS loss のほぼ全て
```

かつ Off 比で大幅に低下するなら、

> Fruity の fixed cap bug ではなく native Reflex pacing problem

と確定する。

ただし UX として放置もしない。

### 推奨 production policy

**Reflex と generic low latency を同じ「On」の裏で自動的に混同しない。**

現在の logical type は:

```text
Mode:
    Off
    On
    OnBoost

Provider:
    Generic
    Nvidia
```

をすでに分離しているため、この設計を活かし provider preference を明示可能にする。

例:

```text
Low Latency
    Off
    On
    On + Boost

Provider
    Auto
    Generic
    NVIDIA Reflex
```

または UI をより明確に:

```text
Off
Generic Low Latency
NVIDIA Reflex
NVIDIA Reflex + Boost
```

重要なのは、

```text
FPS を戻すために「NVIDIA Reflex」と表示しながら native sleep を消す
```

ことを絶対にしないこと。

ユーザーが

```text
uncapped FPS priority
```

を選ぶなら Generic、

```text
native NVIDIA latency pacing
```

を選ぶなら Reflex、

という正直な選択にする。

### 保存データ migration

既存値:

```text
0 = Off
1 = On
2 = OnBoost
```

を壊さない。

provider preference を別 field として追加するのが最小リスク。

例:

```text
LowLatency=On
LowLatencyProvider=Auto
```

---

# 15. 「自動 fallback して FPS を戻す」は禁止

例えば:

```text
On にした
↓
FPS が20%以上落ちた
↓
内部で勝手に Generic へ変更
```

は採用しない。

理由:

```text
実行環境依存
測定ノイズ依存
モード表示と実体が一致しない
Reflex 検証不能
再現性低下
```

明示 provider policy の方が SRP / observability / reproducibility の全てで優れる。

---

# 16. Telemetry implementation details

## 16.1 Normal build への steady-state overhead を入れない

現在と同様:

```cpp
std::getenv("FRUITY_RENDER_METRICS")
```

で opt-in。

通常ビルドでは:

```text
chrono now()
histogram update
percentile storage
extra log
```

を hot path に入れない。

---

## 16.2 No per-frame allocation

統計用 storage は固定長 ring または running aggregate。

禁止:

```text
std::vector grow per frame
string formatting per frame
iostream per frame
```

120-frame report のときだけ文字列化。

---

# 17. Unit tests

## 17.1 `TestVulkanNvidiaReflex`

追加:

```text
Unlimited interval 0
240 FPS -> 4167 us
144 FPS -> 6945 us
On -> lowLatencyMode=true / boost=false
OnBoost -> true / true
Off -> interval 0
timeout poll does not call vkLatencySleepNV twice
next frame gets monotonic present ID
```

---

## 17.2 Presentation scheduler

追加または保持:

```text
authority=Native
    Deadline(now) == now

authority=Native
    generic period does not cap

authority=Generic + explicit cap
    deadline follows cap
```

---

## 17.3 Runtime cap mapping

`ApplyFrameRateSettings()` の pure helper を切り出してテスト可能にするのが望ましい。

例:

```cpp
uint32_t ReflexMinimumIntervalUs(int cap);
```

期待:

```text
-1 -> 0
 0 -> 0
30 -> 33334
60 -> 16667
144 -> 6945
240 -> 4167
500 -> 2000
```

---

# 18. Runtime acceptance criteria

## Gate 1 — no hidden 265 cap

`Unlimited + Reflex On`:

```text
minimumIntervalUs == 0
```

必須。

---

## Gate 2 — no double pacing

Native provider:

```text
generic presentation sleep time == 0
generic latest-submission pacing wait == 0
```

低遅延 authority としては Reflex のみ。

---

## Gate 3 — explicit cap accuracy

```text
144 cap -> around 144 FPS
240 cap -> around 240 FPS
```

Reflex の `minimumIntervalUs` と整合。

---

## Gate 4 — Off baseline

Off で既存 Vulkan throughput を退行させない。

Reflex telemetry の追加だけで Off FPS が目に見えて低下してはいけない。

---

## Gate 5 — NVIDIA Reflex FPS impact

NVIDIA の現行 guidance を guardrail として:

```text
On:
    Off 比 FPS loss <= 4% を目標

On + Boost:
    Off 比 FPS loss <= 7% を目標
```

これを大幅に超える場合は PASS にしない。

ただし Vulkan native extension と Streamline の実装差があるため、この数字は absolute correctness rule ではなく **異常検知 gate** として扱う。

265 FPS へ大幅落下する状態を「Reflexだから」で PASS にしない。

---

## Gate 6 — latency correctness

FPS だけ戻して合格にしない。

少なくとも:

```text
vkGetLatencyTimingsNV
input sample marker
simulation markers
render submit markers
present markers
frame/present ID
```

が一貫していること。

---

## Gate 7 — Vulkan validation

```text
VUID = 0
SYNC-HAZARD = 0
DEVICE_LOST = 0
```

---

# 19. NG 修正

## NG 1 — `vkLatencySleepNV` を削除

Reflex On ではなくなる。

## NG 2 — `vkWaitSemaphores` を待たない

同上。

## NG 3 — input sampling 後へ sleep を移動

low-latency contract を壊す。

## NG 4 — FPS counter を補正して見かけ上 265 を消す

論外。

## NG 5 — `minimumIntervalUs` を arbitrary 1 us にする

0 が uncapped 意味なので不要。

## NG 6 — frames-in-flight を増やして FPS を稼ぐ

latency を悪化させる方向。

## NG 7 — generic wait と native wait を同時利用

二重 pacing。

## NG 8 — On 表示のまま internally Off / Generic へ silent fallback

機能偽装になる。

---

# 20. 推奨作業順序

```text
Phase 0
    Current SHAを固定
    Off / On / OnBoost の baseline 採取

Phase 1
    Reflex sleep-call / wait / admission telemetry追加
    cap / interval / present / Hz provenance追加

Phase 2
    Unlimitedで minimumIntervalUs=0 を実機確認
    generic waitゼロを確認

Phase 3
    disable-swapchain-latency A/B
    disable-explicit-submission-id A/B
    driver VSync/G-SYNC A/B

Phase 4
    原因別 source fix

Phase 5
    必要なら provider preference をUI/保存設定へ追加

Phase 6
    CTest
    Vulkan validation
    renderer switch
    resize/fullscreen/minimize restore
    performance A/B
```

---

# 21. 現時点の判定

現時点で証明済み:

```text
[PROVEN]
265 FPS 固定値は通常設定経路に存在しない。

[PROVEN]
Unlimited は Reflex minimumIntervalUs=0 へ mapping される。

[PROVEN]
Native Reflex authority 時、
generic PresentationScheduler deadline は待たない。

[PROVEN]
Native Reflex authority 時、
generic WaitForLatestSubmission を低遅延 pacing として重ねない。

[PROVEN]
Low Latency On は NVIDIA 対応 Vulkan 環境で
native VK_NV_low_latency2 pacing を実際に使用する。

[PROVEN]
scene queue submissions も revision 3 explicit attribution path を持つ。

[PROVEN]
Qt FPS UI に 265 FPS preset はない。
```

現時点で未証明:

```text
[OPEN]
この実機で monitor refresh rate が何 Hz か。

[OPEN]
NVIDIA Control Panel 側 VSync / G-SYNC の状態。

[OPEN]
265 FPS 時の minimumIntervalUs の実値。

[OPEN]
265 FPS 時の actual VkPresentModeKHR。

[OPEN]
265 FPS 時に時間を消費しているのが
vkLatencySleepNV call 自体か semaphore wait か。

[OPEN]
swapchain latency opt-in / explicit attribution の
driver-side contribution。
```

したがって現在最も正確な結論は:

> **「Vulkan Low Latency ON に Fruity-Prime 固有の 265 FPS cap がある」証拠はない。**
>
> **一方、ON で native NVIDIA Reflex pacing が frame admission を所有する設計であり、265 FPS の実効 ceiling はこの経路から生じている可能性が最も高い。**
>
> **大幅な FPS 低下は正常扱いせず、sleep / wait / WSI / driver policy を分離計測し、Fruity 側で修正可能な要因があれば修正する。native Reflex 固有挙動だった場合は Generic と NVIDIA Reflex をユーザーが正直に選べる provider policy を用意する。**

---

# 22. 参考資料

## Fruity-Prime

- `src/MphRead.Native/Renderer.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/LowLatency.hpp`
- `src/MphRead.Native/NativeRuntime/Rhi/PresentationScheduler.hpp`
- `src/MphRead.Native/NativeRuntime/Rhi/PresentationSleep.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanNvidiaReflex.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanNvidiaReflex.hpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanFrameScheduler.hpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanGraphicsDevice.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSwapchain.cpp`
- `src/MphRead.Native/Mods/Render/FrameTiming.cpp`
- `src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp`
- `src/MphRead.Native.Qt/Shell/SettingsModel.cpp`

## melonPrimeDS

- `docs/development/performance/vulkan-low-latency.md`
- `docs/audit/reflex_investigation_handoff_2026-08-24.md`
- `docs/archive/audits/rendering/2026-08/vulkan_reflex_on_preissue_ab_2026-08-24.md`
- `docs/archive/audits/rendering/2026-08/dx12_reflex_latency_verification_2026-08-24.md`

Repository:

```text
https://github.com/ag-advania/melonPrimeDS
```

## NVIDIA / Vulkan

- NVIDIA System Latency Optimization Guide  
  `https://www.nvidia.com/en-us/geforce/guides/gfecnt/202010/system-latency-optimization-guide/`

- Vulkan Specification / `VK_NV_low_latency2` WSI section  
  `https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html`

- Vulkan `VK_NV_low_latency2` reference  
  `https://docs.vulkan.org/refpages/latest/refpages/source/VK_NV_low_latency2.html`

- NVIDIA Streamline SDK FAQ  
  `https://developer.nvidia.com/rtx/streamline/get-started`
