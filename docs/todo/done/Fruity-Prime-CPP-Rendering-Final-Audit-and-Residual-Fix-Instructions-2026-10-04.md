# Fruity Prime C++ Rendering 最終監査・残作業修正指示

- Repository: `Zection6V/Fruity-Prime`
- Branch: `develop3_rendering`
- 監査固定 HEAD: `438624afc48919937d460aab426fd3daa217c07e`
- 監査日: 2026-10-04
- 対象: C++版 OpenGL / Vulkan
- 前回計画: `docs/todo/Fruity-Prime-CPP-Rendering-Bottleneck-Audit-and-Fix-Plan-2026-10-04.md`
- 実行記録: `docs/todo/Fruity-Prime-CPP-Rendering-Bottleneck-Progress-2026-10-04.md`

---

## 1. 結論

前回監査で対象にした **Vulkan 約1000 FPS → 約500 FPSの主要回帰は修正済み** と判断する。

現行HEADでは、同一系統の実対戦fixtureで Vulkan が **1642.20 FPS / mean loop 0.604562 ms / mean present 0.092049 ms**、別GPU診断で scene time 約 **0.199275 ms** を記録している。前回の必須基準 850 FPS、目標950 FPSを大幅に上回る。

OpenGLも約500 FPS前後から **815～840 FPS級**まで改善した。GPU sceneは約0.647 ms、CPU loopは約1.21～1.23 msなので、まだCPU frontend余地はあるが、前回の重大回帰とは別の追加最適化領域である。

現時点で新しいP0 performance defectは見つからない。残件は主として最終verificationと、追加で詰めるなら行うCPU allocation / OpenGL uniform write削減である。

---

## 2. 前回P0/P1の完了確認

### Vulkan descriptor / uniform hot path

旧構造の、

```text
毎draw
→ full DescriptorKey構築
→ 64-word hash
→ unordered_map / map lookup
→ node allocation / pointer chasing
```

は撤去されている。

現在は、

- dense semantic uniform slot
- program / block recording generation
- descriptor epoch
- group version
- bounded texture-token state
- fixed program/group state

で管理される。

現行テレメトリでは、

```text
DrawScene C++ allocations      0
Uniform C++ allocations        0
Material C++ allocations       0
Descriptor C++ allocations     0
Submit C++ allocations         0
native descriptor allocations  0
descriptor group overflow      0
uniform string lookup          0
vector growth                  0
```

が確認済み。

**この部分を再設計し直さないこと。**

### Vulkan Reflex Off

Offでexplicit submission attributionを残したことで、NVIDIA 617.14上の `vkQueuePresentKHR` が約1.3 msまで膨らむ問題は解消済み。

現在は、

```text
Off:
  implicit NVIDIA frame attribution
  explicit submission IDなし
  native sleepなし
  generic latest-submission waitなし
  measurement marker / KHR present IDは維持

On / On + Boost:
  native Reflex pacing authority
  explicit application frame identity
```

に分離されている。

修正直後のnative Offでも1182.49 / 1189.08 FPS、その後のdense state最適化後は1550～1642 FPSまで上昇している。

### Vulkan submit helper

`VulkanFrameScheduler::Submit()` はretained signal storageを使い、capacity不足時だけgrowする。submission thread固定・reentrancy拒否も入っている。steady-state vector growthは0。

### Vulkan acquire / present

acquire直後のCPU fence waitは撤去済み。

現行は、

```text
frame slot completionをpoll
→ busy時だけslot reuse wait
→ vkAcquireNextImageKHR(imageAvailable semaphore, fence=null)
→ CPUではacquire completionを待たない
→ GPU submitがimageAvailableをwait
```

となっている。

fallback swapchain retirementも、acquire時点のretirement ceilingを記録し、そのframeの**実際のsubmission fence完了**でのみ古いretired prefixを解放する。

present completionはfixed ringで、poll優先・ring exhaustion時だけwait。

最終fixtureではsteady host wait / device idleとも0。

### Event pump / SRP

Qt window loopがevent dispatchを所有し、telemetryでも **1 event pump / accepted frame**。Vulkan swapchainからplatform event処理を所有する旧構造は解消済み。

### OpenGL

前回の主要問題は修正済み。

- `mat_alpha` / `alpha_test` locationはprogram creation時に一度だけ取得
- warm draw pathの `glGetUniformLocation` = 0
- warm draw pathのcurrent-program query = 0
- program ownerがalpha値を保持し、同値writeを抑止
- VAOはcontext ownerが保持し、draw後にVAO 0へ戻さない
- same VAO native bindを抑止
- Qt Quick等はexplicit external-GL boundaryでstate invalidation
- immutable binding limitsはdevice-owned cache
- generic BindingSet snapshotはshared immutable storageを利用

OpenGL memory admissionもframe単位snapshotへ変更され、大量NVX free-memory queryを除去。OpenGLは約534 FPSから約826～840 FPSへ改善した。

---

## 3. 現時点の正当性証拠

現行ソースのローカル記録では次がPASS。

- MSVC Release
- CTest 22/22
- static OpenGL / RHI audits
- RHI conformance
- resource validation
- Vulkan validation error 0
- OpenGL GPU lifetime 3/3
- Vulkan GPU lifetime 3/3
- released live/retired GPU objects 0
- Settings経由 active-match renderer switch
- forced fallback retirement
- backdrop parity
- Golden Capture OpenGL/Vulkan 7/7
- OpenGL Golden PNG 7/7 exact hash match
- native Reflex Off / On / On + Boost基本toggle
- resize / minimize / restoreを含むnative presentation checks

---

## 4. exact-HEAD GitHub Actions

監査HEAD:

`438624afc48919937d460aab426fd3daa217c07e`

### build_cpp

Run `37205560369`: **SUCCESS**

12 jobsすべて成功。

- Windows / MSVC
- Linux / GCC
- macOS / Clang
- Android native contract
- Android arm64-v8a
- Android x86_64
- Android APK
- frontend OpenGL static audit
- legacy OpenGL static audit
- shader interface static audit
- RHI backend isolation
- OpenGL dependency classification

同SHAのpush run `37205557680` は開始直後cancelledだが、同じSHAのPR runがterminal SUCCESSなのでbuild gateとして問題にしない。

### golden_parity_adapter

Run `37205557611`

監査時点: **IN PROGRESS**

完了済み:

- Compile shared adapter / phase3: SUCCESS
- Compile shared adapter / phase4: SUCCESS

未完:

- Phase 4 / Windows runner-owned CMake build with shared adapter: IN PROGRESS

したがって、現時点ではgolden adapter全体をgreenとはまだ扱わない。

---

# 5. 必須残件 P0-A: production Reflex failure/fallback matrix

CPU fake unit testでは既に、

- extension unavailable
- marker function unavailable
- semaphore creation failure
- set-mode failure
- sleep failure
- wait failure

を検査し、native failure時に、

```text
requested = On + Boostを保持
effective = Generic On
provider = Generic
authority = Generic
boostSupported = false
fallback reasonあり
```

になることを確認している。

ただし進捗記録では**production runtimeのfailure/fallback matrixが未完**。

既存 `FRUITY_REFLEX_TEST_FAILURE` を使い、実GPU executableで最低限:

```text
semaphore
set-mode
sleep
wait
```

を実行する。

各failureで必須確認:

```text
crashなし
device unnecessary recreationなし
requested mode保持
effective Generic On
provider Generic
authority Generic
fallback reason正しい
simulation進行
render進行
present進行
Settings toggle可能
renderer switch後もrequested mode保持
validation error 0
shutdown clean
```

特に、

```text
native Reflex pacing
+
generic WaitForLatestSubmission
```

が同一frameで二重authorityにならないこと。

extension/function unavailableをhardware上で強制できない場合は、既存CPU contract testをauthoritative evidenceとしてよい。

---

# 6. 必須残件 P0-B: exact-HEAD golden adapter terminal gate

Run `37205557611` がterminalになるまでshared Golden gateを完了扱いにしない。

必要結果:

```text
run conclusion = success
Phase 4 Windows runner-owned CMake build = success
```

失敗時だけjob logに基づき個別修正する。

**現在のrenderer sourceを予防的に変更しないこと。**

---

# 7. 必須残件 P0-C: final presentation matrix

個別テストは多数PASSしているが、progress表では最終revision matrixが未完扱い。

`438624af...` で最終1セットにまとめる。

最低限:

```text
OpenGL: Immediate / FIFO / Mailbox fallback semantics
Vulkan: Immediate / FIFO / Mailbox if exposed
```

各経路で、

```text
normal
resize
fullscreen
windowed return
minimize
restore
accepted present
accepted + ResizeRequired
rejected + ResizeRequired
renderer switch
clean shutdown
```

を確認。

**selectable exclusive fullscreen実装は今回のbottleneck taskのblockerにしない。**
これは別タスクとして後段に残す。

---

# 8. 必須残件 P0-D: final shipping hot-path audit

exact HEADのshipping条件で最後に確認する。

条件:

```text
Low Latency Off
Unlimited
Immediate
active match
validation OFF
GPU profile OFF
FRUITY_RENDER_METRICS OFF
FRUITY_PERF_TELEMETRY OFF
FRUITY_NEW_TELEMETRY OFF
```

diagnostic runでは別途以下を再確認:

```text
DrawScene allocation       0
Uniform allocation         0
Material allocation        0
Descriptor allocation      0
Submit allocation          0
native descriptor alloc    0
descriptor overflow        0
vector growth              0
host wait                  0
device idle                0
uniform string lookup      0
event pump                 1/frame
```

diagnostic buildのFPSをshipping acceptance値と混ぜない。

---

# 9. P1: scene traversal 約81 allocations/frame

これは現在残っている最大のCPU allocation課題。

最終telemetry:

```text
SceneRender:
  約2 allocations/frame
  約96 bytes/frame

Traversal:
  約81 allocations/frame
```

ただしbackend hot pathではない。

`Traversal` は概ね、

```text
Scene::OnDrawFrame
  LoadAndUnload
  RenderItem pool recycle
  UpdateProjection
  GetDrawItems
    Room GetDrawInfo
    Player Draw
    Entity GetDrawInfo
    Effects / particles
    AddRenderItem
```

を含む。

## 9.1 まずcallsiteを特定する

現時点では81 allocationsの発生源は未証明。

候補:

- RenderItem pool / deque block churn
- particle/effect temporary arrays
- ManagedArray
- entity draw helperのtemporary vector
- shared ownership temporary objects

`_renderItemAlloc = 200` でRenderItem本体は初期確保されるため、`GetRenderItem()`の`make_shared`が毎frame81回だとは断定しない。

### 必須診断

`FRUITY_NEW_TELEMETRY` にTraversal / SceneRender限定のsampled allocation callsite histogramを追加。

要件:

```text
frame内console outputなし
frame内symbolizationなし
普通のbuildではcompile-out
固定容量のsample storage
終了後だけDbgHelp等でsymbolize
```

推奨:

```text
1/64 または 1/128 allocationをsample
最大1024 unique address chains
top 20 callsitesを出力
```

出力:

```text
phase
calls
bytes
function
file
line
```

---

# 10. P1-A: RenderItem poolが原因なら

callsite証拠で、

```text
std::deque block allocation
std::queue backing deque
RenderItem overflow
```

が上位なら、Scene-owned slab / vector free-listへ変更する。

推奨:

```text
persistent RenderItem pages
+
vector<RenderItem*> free
+
vector<RenderItem*> used
```

またはaddress stability不要ならreserve済みvector。

要件:

- active scene中にshrinkしない
- high-waterを保持
- overflowはgrow可能
- RenderItem address/lifetime条件を維持
- preview ownershipを維持
- `Points` pool return順序を維持
- draw orderを変更しない

**allocation削減のためにdraw itemを省略・mergeしない。**

---

# 11. P1-B: particle/effectが原因なら

temporary geometry / arraysをowner lifetimeへ移し、

```text
clear()
+
capacity retain
```

または既存array poolを利用する。

禁止:

- particle count削減
- effect省略
- effect lifetime変更
- draw order変更
- GPUが参照中のCPU storage再利用

submission-owned GPU upload storageとの寿命境界を維持する。

---

# 12. P1-C: SceneRender 2 allocations / 96 bytes

量は小さいため、callsiteを特定する前に構造変更しない。

もし毎frame同型のtemporary object/vectorならpersistent scratch化する。

必要なownership transferや非steady conditional pathなら残してよい。

目標は形式的な「allocation 0」ではなく、**避けられるhot allocator churnを除去すること**。

---

# 13. P1-D: OpenGL 約8050 uniform writes/frame

最終telemetryではOpenGL native uniform writeが約8050/frame残る。

次の追加最適化候補。

ただしwrite数だけを見てskipしてはいけない。

まずsemanticごとに:

```text
requested writes
native writes
same-value suppressed
changed-value writes
program switches
```

をcountする。

分類例:

- matrix stack
- view/projection
- material
- lights
- fog
- texture matrix
- HUD
- post-process

同一program / locationへ**同一値のnative write**が大量に存在すると証明されたsemanticだけ、program-owned last-value / generationで抑止する。

`mat_alpha` / `alpha_test` の既存方式を参考にする。

禁止:

- 一律uniform skip
- programを跨いだdirty state共有
- epsilon比較
- pointer identityだけのmatrix比較
- visual parityを変える近似

---

# 14. P1-E: OpenGL BindingSlot

generic `ApplyBindingSet()` は現在も各entryでlogical layoutを走査してphysical slotを算出する。

純CPU処理だが、generic binding pathが実ゲームの主要hotspotという証拠はまだない。

profilingで上位に出た場合のみ、BindingLayout作成時に、

```text
(group, binding, arrayElement, type)
→ physical slot
```

をprecomputeし、bind時をO(1)化する。

上位でなければ触らない。

---

# 15. P2: Vulkan 5 queue submits/frame

最終telemetryでは約5 submit / accepted frame。

Vulkanは1642 FPSなので修正必須ではない。

さらにlow-cycle化する場合はsubmit reasonを分類する。

例:

```text
scene
window/composite
transfer
external Qt
timeline / external marker
```

同一queueかつ、

- ordering semanticを維持
- resource lifetimeを維持
- readback boundaryを維持
- Reflex attribution/markerを維持
- Qt ownershipを維持

できるものだけ統合候補にする。

**submit数を減らす目的だけでownership boundaryを壊さない。**

---

# 16. 残るhistorical SHA計測はblockerから外す

progress文書では、

- `ec6e98b`
- `f2597d92`
- `e03978dc`
- clean `9ce34a39`

が未計測。

しかし既に、

1. `dc1ffe` → `dd69cb` matched A/Bで初期回帰を再現
2. Reflex Off present stallを直接測定
3. attribution A/Bで原因分離
4. descriptor/uniform costを実測
5. 現HEADで過去性能を大幅超過
6. Golden / lifetime / validationがPASS

している。

よって残るhistorical SHA計測は**forensic chronology用の任意作業**へ降格してよい。

製品側の完了を止めない。

---

# 17. selectable exclusive fullscreen

今回のrender bottleneck修正とは別タスク。

以前の方針どおりFPS改善後の後段で扱う。

今回の完了判定をexclusive fullscreenで止めない。

---

# 18. 最終優先順位

## Must close

1. production Reflex failure/fallback runtime matrix
2. exact-HEAD `golden_parity_adapter` terminal SUCCESS
3. exact-HEAD final presentation matrix
4. exact-HEAD shipping hot-path audit

ここで重大failureがなければ、元のrendering bottleneck taskはCLOSEDでよい。

## High-value follow-up

5. Traversal約81 allocations/frameのcallsite特定
6. top allocationだけpersistent scratch/pool化
7. SceneRender 2 allocations/frameのcallsite特定
8. OpenGL uniform writesをchanged/redundant別に分類
9. 証拠のあるredundant writeだけ抑止

## Optional

10. Vulkan 5 submits/frameのreason分類
11. 安全なsubmitだけ統合
12. 残るhistorical SHAのforensic計測

---

# 19. 完了基準

## Vulkan

```text
Low Latency Off
Unlimited
Immediate
active match
diagnostics OFF
mean FPS >= 950
mean loop <= 1.05 ms
GPU scene regressionなし
present stallなし
steady host wait 0
steady device idle 0
DrawScene allocation 0
descriptor overflow 0
validation error 0
Golden parity PASS
lifetime clean
```

現在の性能値は既に大幅PASS。

## OpenGL

```text
>= 800 FPS級を再現
hot glGetUniformLocation 0
hot current-program query 0
redundant VAO unbind 0
Draw/DrawIndexed allocation 0
host wait 0
device idle 0
conformance clean
Golden pixels維持
```

現在の実測はこれを満たす。

OpenGLを「完全GPU-boundになるまで」を今回のblockerにはしない。

---

# 20. 禁止事項

現状から以下の大規模変更は行わない。

- Vulkan descriptor architecture再設計
- Vulkan scene ABI再設計
- frames-in-flightを根拠なく増やす
- swapchain ownership再設計
- Reflex controller全面再設計
- Qt Quick置換
- CPU rendering fallback
- synchronous readback
- indiscriminate draw batching
- effect / HUD / primitive省略
- visual parityを崩す近似

現在はarchitectureを再度揺らす段階ではない。

**残った具体的な測定値だけを狙って修正する。**

---

# 21. 最終判定

### Vulkan

**PASS。主要性能作業は完了。**

約500 FPSへの回帰は解消され、現行は1600 FPS級。

descriptor/uniform hot path、Reflex Off attribution、submit allocation、WSI host waits、event ownershipの主要問題は修正済み。

### OpenGL

**PASS。追加最適化余地あり。**

800 FPS超まで改善し、前回の明確なdriver-query/state-churn問題は解消済み。

追加で詰めるなら、

1. Traversal allocations
2. uniform write redundancy
3. 残存integer query
4. generic binding CPU計算

の順で、必ず計測してから触る。

### SRP / architecture

**PASS。**

pacing、event pump、swapchain、submission tracking、descriptor state、external GL interopの責務境界は妥当。

大規模再設計は不要。

### Task status

**Implementation: substantially complete**

残る必須作業はclosure/verification。

```text
Reflex runtime failure matrix
golden adapter terminal result
final presentation matrix
final shipping hot-path audit
```

これらがPASSすれば、元の

**低サイクル・ローオーバーヘッド・ベストプラクティス・SRP・ハイパフォーマンス化**

の性能回帰修正タスクは完了扱いでよい。

Traversal allocation削減とOpenGL uniform write削減は、その後の追加最適化として分離する。
