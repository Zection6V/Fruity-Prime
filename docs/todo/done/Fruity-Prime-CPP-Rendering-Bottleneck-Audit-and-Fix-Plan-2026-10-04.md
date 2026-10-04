# Fruity Prime C++ 描画スループット・ボトルネック監査／修正指示

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop3_rendering`
- 監査固定 HEAD: `9ce34a391ad8bb2d08848e8189e413eccf28d2a4`
- 監査日: 2026-10-04
- 対象: C++版 OpenGL / Vulkan
- 主目的: **低サイクル、ローオーバーヘッド、SRP、高スループット、不要なCPU/GPU同期なし、steady-state hot pathで不要な動的確保なし**
- 優先順位: **描画正確性・安定性を維持したまま、CPU frontend overheadを除去する**

---

## 1. 結論

今回の低下は、単純な「500 FPS cap」が主因ではない。

旧設定で `Unlimited` が 500 FPS として保存・解釈される問題は、commit
`f2597d9257661f7eadaf46f4c1d33c0eb44f0729`
`Make native Unlimited FPS truly uncapped and migrate saved settings`
で既に修正されている。現行の計測記録でも `fps_cap=unlimited`、Immediate present の状態で Vulkan / OpenGL が500 FPS前後に留まっている。

さらに、同じ描画アーキテクチャの過去のFPS-only計測では、Alinos Perch、2560×1439、Unlimited、Immediate、validationなし、GPU profilingなしの条件で Vulkan が約946～995 FPS、代表値973.3 FPSを記録している。GPU scene timeも約0.20 msだった。

したがって、Vulkanの現在の約500 FPS、すなわち約2.0 ms/frameは、GPU shader / fill-rateが主因ではなく、**CPU側の1フレーム生成・command recording・state bookkeeping・同期境界で増加したコストが主因**である。

監査結果を重要度順にまとめると次の通り。

| 優先度 | 領域 | 判定 | 根拠 |
|---|---|---|---|
| P0 | Vulkan `DrawScene()` の semantic descriptor / uniform cache | **最重要のCPU hot-path問題** | per-drawで大きなkey生成、hash、`unordered_map`、`map`、persistent-material lookupを行う。直近のnode再利用だけで約16%改善したため、CPU container/bookkeeping負荷が実測で確認済み |
| P0 | OpenGL small constants | **明確なdriver-query hot path** | draw更新時に `GL_CURRENT_PROGRAM` を照会し、`mat_alpha` の `glGetUniformLocation` を繰り返す。`alpha_test`側もprogram照会を行う |
| P0 | OpenGL VAO/state churn | **明確な不要API churn** | `Draw` / `DrawIndexed` が各draw後に `BindVertexArray(0)`。次drawで再bindするため状態localityを自ら破壊している |
| P1 | Vulkan queue submit helper | **steady-state heap allocation** | `VulkanFrameScheduler::Submit()` が毎submitで `std::vector<VkSemaphoreSubmitInfo>` を生成・copy・pushする |
| P1 | Vulkan WSI acquisition / reuse | **条件付きCPU serialization** | frame fenceに加え、環境によって acquire fenceをacquire直後にCPU waitし、image last-frame / present fenceもhost waitする |
| P1 | Vulkan platform event ownership | **SRP違反＋重複処理** | Qt window loopが毎frame `processEvents()` する一方、Vulkan `SubmitAndPresent()` 末尾でも `RendererPlatform::ProcessEvents()` |
| P1 | Generic Low Latency | **意図的なthroughput limiter** | On時はlatest submission完了を最大2 ms host waitする。Low Latency Offの現行500 FPS測定の原因ではないが、On時に高FPSを要求してはいけない構造 |
| 除外 | FPS cap | 現行主因ではない | `Unlimited=-1` と旧500設定migrationは修正済み |
| 除外 | Qt Quick menu rendering | 通常対戦中の主因ではない | menu非表示時 `UiHost::Tick()` はQt Quick初期化・render前にreturn |
| 除外 | Vulkan GPU shader負荷 | 主因ではない | 過去同系統条件でGPU scene約0.20 ms、Vulkan約950～995 FPS |

**根本的な設計問題は、driver call削減のために導入したCPU semantic cacheが、毎drawで「内容を再構築してhashして検索する」設計になっていること。**

Vulkanではdriver workを減らしている一方で、CPU側に次の処理を追加している。

- resource identity / generationの収集
- descriptor keyの構築
- key全体のhash
- node-based `unordered_map` lookup
- node-based `map` lookup
- persistent material hash lookup
- pipeline-layout compatibility scan
- per-generation cache clear
- node再利用／allocator管理

この方向は「native call数」だけを見ると改善に見えるが、Fruity PrimeのようにGPU sceneが0.2 ms程度と軽い場合、**CPU側の数百ns～数µs級の処理をdrawごとに積み上げる方が高くつく**。

直近HEADの `std::pmr::unsynchronized_pool_resource` 化により Vulkan が約428～433 FPSから約498～503 FPSへ改善したこと自体が、CPU cache node / associative-container overheadが実際のボトルネックである強い証拠である。ただしpool resourceは「malloc/freeを軽くした」だけであり、key構築、hash、pointer chasing、tree lookup自体は残っている。

---

# 2. 性能回帰の証拠

## 2.1 過去のVulkanは約1000 FPS出ていた

現行リポジトリ内の
`docs/todo/done/Fruity-Prime-Rendering-Architecture-Implementation.md`
には、R18 FPS計測として次の条件が記録されている。

- Alinos Perch
- spawned Sylux + bots
- 2560×1439
- resolution scale 100
- pause 0
- focus 1
- FPS Counter Off
- Unlimited
- Immediate
- validation Off
- GPU profiling Off

記録値の一例:

| Renderer | FPS | mean frame interval | CPU loop | present CPU |
|---|---:|---:|---:|---:|
| OpenGL | 283.8 | 3.524 ms | 3.518 ms | 0.090 ms |
| Vulkan | **973.3** | **1.027 ms** | **1.025 ms** | 0.086 ms |

別の記録でも Vulkan は次の範囲。

- 995.1 FPS
- 960.0 FPS
- 955.2～992.0 FPS
- 963.8 FPS
- 946.0 FPS
- 平均979.30 FPS

GPU scene sampleは約0.20～0.22 ms。

よって「以前は1000 FPS近く出ていた」はリポジトリ内の過去計測と一致する。

---

## 2.2 現在のVulkanは約500 FPS

現行HEADの
`docs/todo/done/Fruity-Prime-Gpu-Mesh-Draw-Optimization-2026-10-04.md`
では、同じくWindows / RTX 5070 Ti / MSVC Release / Qt / Alinos Perch / Unlimited / Immediate / Low Latency Offの実対戦条件で、VulkanのCPU cache node再利用前後を測定している。

| Vulkan | 変更前 | 変更後 |
|---|---:|---:|
| 平均FPS run 1 | 427.81 | **498.33** |
| 平均FPS run 2 | 433.21 | **503.19** |
| 最大FPS | 459.08 / 493.34 | 533.31 / 546.51 |

約16%改善しているが、1000 FPSへの回復はしていない。

500 FPSは約2.0 ms/frameであり、過去の約1.0 ms/frameからほぼ倍増している。

---

## 2.3 OpenGLもCPU hot path除去で大幅改善済み

同じ文書ではOpenGLのVAO cache検索用scratchで毎draw発生していたheap allocationを除去した結果、

- 381.25 / 386.52 FPS
- → **518.93 / 518.01 FPS**

へ約35%改善している。

つまりOpenGLもGPUだけでなく、**C++側の小さな動的確保・検索コストが大量drawで積み上がっていたことが実測で確定している。**

---

# 3. 回帰タイムライン

性能を追う際は、少なくとも次のcommitを固定点として比較する。

| SHA | 内容 | 性能監査上の意味 |
|---|---|---|
| `dc1ffe004dadeba8f938d5519eb154206b667f06` | backdrop parity fix | `dd69cb`直前。descriptor semantic cache導入前の比較点 |
| `dd69cb2467e6fdc322f4f7ad73f52ac7ba31df31` | descriptor最適化＋generic low latency | Vulkan hot pathが大きく変化した最重要境界 |
| `3d72fbb275a197ae90d2442c41a7d0a57f0afb32` | NVIDIA Reflex | pacing / submit attribution変更境界 |
| `cc0d2e90fb775c6959d3553e596677361d181a9d` | Qt menus merge | Qt opt-in境界 |
| `ec6e98b50953b37d6b69d996042874ce152e3a27` | Qt Quickへ全面置換 | platform loop / UI ownership変更境界 |
| `f2597d9257661f7eadaf46f4c1d33c0eb44f0729` | Unlimited修正 | 500 cap誤解釈を除外できる境界 |
| `e03978dc6b19aef667f8bf6a53596a4bbd41834d` | mesh draw削減＋GL scratch再利用 | OpenGL約35%改善 |
| `9ce34a391ad8bb2d08848e8189e413eccf28d2a4` | Vulkan cache node再利用 | Vulkan約16%改善、今回の監査HEAD |

**修正着手前に `dc1ffe` → `dd69cb` の同一条件A/Bを必ず取ること。**

現在の静的監査と後続改善幅から `dd69cb` で導入されたCPU semantic cacheが最重要候補だが、正確な「最初の回帰commit」を確定するには、この1境界のA/Bを取るのが最短である。

Qt全面移行の寄与を分離するため、その後 `3d72fbb`、`cc0d2e90`、`ec6e98b` も同一fixtureで比較する。

---

# 4. Vulkan P0: `DrawScene()` のCPU semantic cacheを再設計する

対象:

`src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanCommandListInternal.inc`

## 4.1 導入前

`dc1ffe...` の `VulkanCommandList::DrawScene()` は概ね次の構造だった。

1. texture state確認
2. pipeline variant取得
3. block `Generation` が変わった時だけuniform slice allocate / memcpy
4. 対象groupがdirtyならdescriptor setをallocate / update
5. descriptor set bind
6. vertex/index bind
7. draw

重要なのは、**uniform更新判定が既存の`Generation`比較だけで完結していたこと**。

---

## 4.2 現行

`dd69cb...` 以降は、上記に加えてhot pathで次を行う。

- texture identity / generation / sampler identity収集
- pipeline layout prefix compatibility走査
- `PersistentMaterialSet()` lookup
- `_uniformCache` lookup
- `DescriptorKey` の構築
- buffer pointer / offset / rangeをkeyへ詰める
- texture identity / generation / view / sampler / layoutをkeyへ詰める
- `DescriptorHash` でword配列全体をhash
- `_descriptorCache.find()`
- miss時 `_descriptorCache.emplace()`
- `_boundSceneSets` 比較

現行HEADではnode allocatorをpool化したが、

- hash計算
- key構築
- `std::unordered_map` bucketアクセス
- `std::map` tree traversal
- pointer chasing
- branch
- cache miss

は残る。

GPUが約0.2 msしか使わないworkloadでは、このCPU bookkeepingが支配的になる。

---

## 4.3 修正方針

### 必須: hot pathを「content hash」から「dirty/version driven」に戻す

描画時にdescriptor内容からidentityを再構築して検索してはいけない。

renderer / material / resource stateが変化した時点でversionを進め、draw側は固定長のversion比較だけを行う。

望ましい責務:

```text
Material / Texture / Sampler mutation
        ↓
generation increment
        ↓
Scene binding state marks group dirty
        ↓
next draw only descriptor update
        ↓
subsequent draws are fixed-array compare + existing descriptor handle reuse
```

### `_uniformCache` のnode-based mapをhot pathから撤去

現在:

`std::pmr::map<std::array<std::uint64_t, 3>, RingSlice>`

これはpool allocatorにしてもtree lookupが残る。

既に各blockに `Generation` があるため、command slot / program / blockごとに次を固定配列またはcontiguous vectorで保持する。

- last generation
- current ring slice
- owning command-slot generation

block generationが変わらない限りlookupをしない。

### `_descriptorCache` の大規模semantic hashをhot pathから撤去

最低限、programの各groupに固定stateを持たせる。

保持項目:

- current descriptor set
- program generation
- uniform generation tuple
- texture generation tuple
- sampler generation tuple
- command-slot generation
- dirty bit

比較対象はpointer-richなfull descriptor keyではなく、既にresource ownerが持つmonotonic generationとsmall integer identityに限定する。

非連続material間のdescriptor再利用が必要なら、次の優先順位とする。

1. material object自身にpersistent descriptor stateを所有させる
2. stable material IDで直接indexできるdense table
3. bounded flat/open-address table
4. 最後の手段としてnode-based hash table

**4を通常drawの第一経路にしないこと。**

---

## 4.4 persistent material cache

persistent materialという考え自体は残してよい。

問題は、drawのたびに「このdescriptor内容は以前と同じか」を大きなsemantic keyから再計算することである。

material owner側で次を持つ設計に変更する。

- stable material ID
- material generation
- texture binding generation
- sampler generation

material mutation時だけgenerationを更新する。

Vulkan backendは `material ID + generation` の変化だけを見てdescriptorを更新する。

これによりSRPも改善する。

- material owner: material内容の変更検知
- Vulkan binding cache: native descriptorの生成・保持
- command list: 既に確定したnative stateをrecordするだけ

---

## 4.5 small constants

Vulkan push constantsへの分離は維持してよい。

`mat_alpha` / `alpha_test` のような小さく頻繁な値をUBO allocation / descriptor更新から外す方針は正しい。

ただしsmall constants更新判定も固定サイズbyte比較またはversion比較に限定し、associative cacheへ流さない。

---

## 4.6 成功条件

steady-state gameplayで:

- `DrawScene()` 内のheap allocation: **0**
- `DrawScene()` 内のnode-based map insertion: **0**
- unchanged drawにおけるdescriptor key全再構築: **0**
- unchanged materialにおけるdescriptor update: **0**
- unchanged descriptor setにおけるnative bind: **0**
- resource mutation後の最初のdrawだけ正しく更新
- renderer switch / texture resize / resource destructionでstale handleなし

---

# 5. OpenGL P0: driver queryをdraw hot pathから完全撤去する

対象:

`src/MphRead.Native/NativeRuntime/Rhi/OpenGL/OpenGlDevice.cpp`

現行 `SetSmallConstants()` は、small constants更新のたびに次を行う。

- `RestoreDrawState()`
- `ApplyAlphaTest()`
- `GL::GetInteger(CurrentProgram)`
- `GL::GetUniformLocation(program, "mat_alpha")`
- `GL::Uniform1(...)`

さらに `ApplyAlphaTest()` でもdesktop側で `GL::GetInteger(CurrentProgram)` を行う。

`alpha_test` locationは一度見つけた後cacheされるが、`mat_alpha` locationは毎回照会される。

これは高FPS rendererのhot pathとして避けるべきである。

---

## 5.1 修正

program link / creation時に次を1回だけ解決する。

- `mat_alpha` location
- `alpha_test` location

`OpenGlProgramStorage` または同等のnative program ownerに保持する。

command listは現在適用済みpipeline/programを既に知っているため、`GL_CURRENT_PROGRAM` を問い合わせない。

さらにcommand list側で:

- last material alpha
- last alpha test mode
- current native program

を保持し、値が変化した時だけ `glUniform1*` を発行する。

### 必須不変条件

steady-state scene draw中に:

- `glGetIntegerv(GL_CURRENT_PROGRAM)` を呼ばない
- `glGetUniformLocation` を呼ばない
- immutable device limitを `glGetIntegerv` で再照会しない

---

# 6. OpenGL P0: VAOを各draw後に0へ戻さない

対象:

`src/MphRead.Native/NativeRuntime/Rhi/OpenGL/OpenGlCommandsInternal.inc`

現行:

- draw前 `BindVertexArray(VertexArray())`
- draw
- draw後 `BindVertexArray(0)`

`DrawIndexed()` も同じ。

これは次drawがほぼ必ず別のbindを必要とするため、不要なdriver callを増やしている。

---

## 6.1 修正

command listにcurrent VAOを保持する。

- desired VAO == current VAOならbindしない
- 変わった時だけbind
- command list終了時にも原則0へ戻さない
- external OpenGL consumerへ制御を渡す境界だけ明示的にinvalidateする

Qt Quickとの共存のために「毎draw defensive reset」を使ってはいけない。

代わりにSRPとして明確なinterop boundaryを作る。

```text
RHI owns GL state
    ↓
BeginExternalGlInterop()
    state cache invalidation / required save
    ↓
Qt Quick
    ↓
EndExternalGlInterop()
    state cache invalidation / required restore
    ↓
RHI owns GL state again
```

現行 `GlStateGuard` が必要な状態保存を行うなら、その境界でcommand-list cacheを明示的にdirtyにする。

---

# 7. OpenGL P1: immutable limitsを毎bindingでqueryしない

`ApplyBindingSet()` ではbinding種別に応じて、

- max uniform/storage buffer bindings
- max sampler/texture units
- max image units

などを `GL::GetInteger()` で確認する経路がある。

これらはcontext/device lifetime中に変化しない。

device初期化時に一度だけCapabilitiesへ格納し、binding時は整数比較だけにする。

**driver queryをvalidationの代用品としてhot pathに置かないこと。**

---

# 8. OpenGL P1: generic BindingSet snapshotのheap allocationを除去する

generic `SetBindingSet()` はbinding snapshotを `std::make_unique<OpenGlBindingSet>` で複製する。

scene path以外でも高頻度に通る可能性があるなら、次のいずれかへ変更する。

1. immutable binding objectのstable lifetime handle
2. inline fixed-size snapshot
3. preallocated command-list scratch
4. arena high-water reuse

steady-stateのbinding変更でsystem heapへ出ないこと。

---

# 9. Vulkan P1: `VulkanFrameScheduler::Submit()` のheap allocationを除去する

対象:

`src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanFrameScheduler.hpp`

現行 `Submit()`:

- local `std::vector<VkSemaphoreSubmitInfo>`
- incoming signal arrayを `assign`
- submission timeline signalを `push_back`
- `vkQueueSubmit2`

つまりqueue submitのたびにcapacity不足ならheap allocationが起こる。

queue submit自体はCPUコストが高い処理であり、その直前に追加のallocator churnを置く理由はない。

---

## 9.1 修正

次のいずれか。

### 第一候補

scheduler所有のreusable scratch vectorを持ち、一度到達したhigh-water capacityを再利用する。

条件:

- graphics queue submission threadを明示する
- reentrant使用しない
- `clear()`のみでcapacity維持
- required capacityを事前reserve

### signal数に小さなhard upper boundを定義できる場合

fixed-size `std::array` を使う。

どちらでも、steady-state `Submit()` のheap allocationを0にする。

---

# 10. Vulkan P1: swapchain acquisitionをCPU admission barrierにしない

対象:

`src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSwapchain.cpp`

現行 `AcquireNextTexture()` は複数のhost waitを持つ。

1. frame slot再利用の `vkWaitForFences(frame)`
2. `vkAcquireNextImageKHR`
3. fallback環境では acquire fenceを**acquire直後に `vkWaitForFences`**
4. imageの `lastFrame` が別frame fenceならhost wait
5. present fence pendingなら `WaitForPresent()`

1はframe resource reuseのための通常のframes-in-flight制御。

しかし3～5は、steady-state throughput pathではCPUとGPU/WSIを必要以上に直列化し得る。

---

## 10.1 acquire completion

通常のswapchain renderingでは、`vkAcquireNextImageKHR` のimage-available semaphoreをsubmit側でwaitすればGPU execution dependencyを表現できる。

CPUがそのsemaphore signalを待ってからcommand bufferをrecordする必要はない。

acquire fenceがold-swapchain retirement証明のために存在しているなら、**retirement責務を通常frame admissionから分離する。**

---

## 10.2 image `lastFrame` host wait

swapchain imageへrecordするcommand自体はCPUで先に作れる。

execution safetyはimage-available semaphoreで制御する。

per-imageで別のCPU-owned transient resourceを書き換えている場合だけ、そのresource ownerのcompletion fenceを待つ。

「swapchain imageを再取得した」ことだけを理由に前frame fenceをhost waitしない。

---

## 10.3 present fence

`VK_EXT_swapchain_maintenance1` のpresent fenceをresource retirementに使う方針は維持できる。

ただしper-image fenceを毎回、

- wait
- reset
- present

の直列chainにしない。

候補:

- present-completion fence ring
- zero-time pollでfree fenceを回収
- retired swapchain ownership専用のfence
- capacityが本当に枯渇した時だけbounded wait

steady-stateで `present_wait_count` が継続的に増える構造を避ける。

---

# 11. Vulkan P1: event processingの所有者をwindow loopへ一本化する

Qt window loopは既に毎frame:

`QCoreApplication::processEvents(QEventLoop::AllEvents)`

を呼ぶ。

一方、Vulkan `SubmitAndPresent()` の最後にもdesktopで:

`RendererPlatform::ProcessEvents()`

がある。

これはpresentation backendがwindow event pump責務を持っており、SRP上も不自然。

## 修正

event processingはwindow loopだけが所有する。

swapchain / RHI側から `ProcessEvents()` を削除する。

例外は、window loopが停止する可能性のある明示的なbounded wait中にresponsive性維持のためにcallbackを入れる場合だけ。

その場合もRHIが直接platform APIを呼ばず、admission controllerからevent-service callbackを受け取る。

---

# 12. Generic Low Latencyの2 ms waitは「性能バグ」ではなく「意図的throughput制限」

対象:

- `Renderer.cpp::BeforeFrame()`
- `PresentationScheduler::FrameBudgetWait`
- `VulkanFrameScheduler::WaitForLatest()`
- `OpenGlFrameScheduler::WaitForLatest()`

Generic Low Latency Onではlatest submitted workが完了するまで最大2 ms host waitする。

これはCPU run-aheadを1 frame程度へ抑える目的であり、throughput最大化とは逆のpolicy。

重要:

**現行の約500 FPS実測はLow Latency Offで取得されているため、この2 ms waitを現在の主因と断定してはいけない。**

ただしLow Latency On時は、500 FPS前後あるいはそれ以下になることが仕様上あり得る。

---

## 12.1 policyを明確に分離する

### Off

目的:

- maximum throughput
- CPU/GPU overlap最大
- generic latest-submission waitなし
- native latency sleepなし

### On / On + Boost

目的:

- lower queue depth
- lower latency
- throughput低下を許容

NVIDIA Reflexがauthorityならgeneric `WaitForLatestSubmission()` を同時に使わない。

この分離は既存方針を維持する。

設定画面の表示でも、Onは「高速化」ではなく「queue depthを制限してlatencyを抑える」機能であることを内部仕様上明確にする。

---

# 13. Qt Quickは通常対戦中の主因ではない

`src/MphRead.Native.Qt/Shell/UiHost.cpp`

`UiHost::Tick()` はmenu非表示時:

- `UiOverlay::Visible(false)`
- `LauncherHunter::Wanted(false)`
- return

となる。

Qt Quickの:

- `polishItems`
- `beginFrame`
- `sync`
- `render`
- `endFrame`

は通らない。

したがって、menuを閉じた実対戦FPSの約500を「QMLを毎frame描いているから」と説明するのは誤り。

ただしQt platform loopのevent dispatch costは別問題なので、profiling phaseで測定する。

---

# 14. 推奨SRP構成

次の責務分割を目標にする。

## `FramePacingController`

責務:

- FPS cap
- display deadline
- Low Latency policy
- Reflex authority
- frame admission

禁止:

- descriptor管理
- swapchain resource lifetime
- platform event API直接呼出

---

## `WindowEventPump`

責務:

- Qt / GLFW event dispatch
- input freshness
- close / resize / focus

禁止:

- GPU submission
- present fence管理
- descriptor管理

---

## `SwapchainPresenter`

責務:

- acquire
- submit wait/signal semaphore relationship
- present
- WSI status
- swapchain recreation request

禁止:

- processEvents
- simulation
- generic frame pacing
- unrelated device-wide wait

---

## `SubmissionTracker`

責務:

- queue serial
- completion polling
- frame-slot resource reuse

原則:

- poll first
- host wait only when本当に再利用対象resourceが枯渇
- hot pathでsystem heap allocationなし

---

## `SceneBindingCache`

責務:

- native binding stateのdirty/version tracking

原則:

- fixed / contiguous storage
- material/resource mutationでdirty
- drawではO(1) integer compare
- content-derived large hashをdrawごとに作らない

---

## `ExternalGraphicsInteropBoundary`

責務:

- Qt Quickなど外部rendererとのstate ownership移譲
- OpenGL state cache invalidation
- Vulkan ownership transition

原則:

- interopが実際に起きた時だけinvalidate
- every draw / every passのdefensive resetは禁止

---

# 15. 修正実施順

## Phase 0: 正確な回帰境界を固定

同一PC・同一driver・同一ゲーム条件で次のSHAを順にFPS-only計測する。

1. `dc1ffe004dadeba8f938d5519eb154206b667f06`
2. `dd69cb2467e6fdc322f4f7ad73f52ac7ba31df31`
3. `3d72fbb275a197ae90d2442c41a7d0a57f0afb32`
4. `cc0d2e90fb775c6959d3553e596677361d181a9d`
5. `ec6e98b50953b37d6b69d996042874ce152e3a27`
6. `f2597d9257661f7eadaf46f4c1d33c0eb44f0729`
7. `e03978dc6b19aef667f8bf6a53596a4bbd41834d`
8. `9ce34a391ad8bb2d08848e8189e413eccf28d2a4`

特に1→2を最優先。

---

## Phase 1: Vulkan command-recording CPU costを除去

1. `_uniformCache` node-based lookupを撤去
2. descriptor semantic key再構築を撤去
3. material / resource generation driven cacheへ変更
4. persistent material cacheをowner identity drivenへ変更
5. current bind suppressionは維持
6. push constantsは維持
7. steady-state allocation=0をassert / telemetryで確認

**ここがVulkan 1000 FPS回復の最優先作業。**

---

## Phase 2: OpenGL hot driver queryを除去

1. uniform locationをprogram作成時に解決
2. `GL_CURRENT_PROGRAM` query撤去
3. alpha/material value cache
4. immutable limitsをCapabilitiesへ移動
5. VAO bind cache
6. draw後 `BindVertexArray(0)` 撤去
7. interop boundaryだけ明示invalidate

---

## Phase 3: submission / presentation micro-overhead

1. `VulkanFrameScheduler::Submit()` allocation=0
2. swapchainからplatform event pumpを削除
3. acquire fence immediate host waitを通常pathから外す
4. image last-frame host waitの必要性をresource単位で再判定
5. present-fence recyclingをnonblocking化
6. frame slot starvationだけを真のhost wait境界にする

---

## Phase 4: throughput / latency policyを分離

Low Latency Off:

- maximum throughput
- no generic latest submission wait

Low Latency On:

- bounded run-ahead
- latency優先

Native Reflex:

- native authority
- generic waitと二重制御しない

---

# 16. 計測方法

FPS比較はvalidation / GPU profileを外す。

基準コマンド:

```powershell
.\FruityPrime.exe -launcher -rhi vulkan -fpsmeasure C:/tmp/vulkan-fps.csv -fpscap unlimited -noupdate
.\FruityPrime.exe -launcher -rhi opengl -fpsmeasure C:/tmp/opengl-fps.csv -fpscap unlimited -noupdate
```

条件:

- 同一room
- 同一hunter
- 同一bot数
- 同一spawn状態
- 同一camera
- 同一resolution
- 同一resolution scale
- 同一fog/cel
- FPS Counter Off
- pause=0
- focus=1
- `main_active=1`
- Low Latency Off
- `present_requested=Immediate`
- `present_actual=Immediate`

segment最初の2秒はwarmupとして除外する。

---

# 17. CPU phase telemetryを追加する場合のルール

現行 `FRUITY_RENDER_METRICS=1` のように大量console出力すると、その出力自体がbenchmarkを壊す。

profilingはsampling方式にする。

推奨phase:

- frame admission
- input/event pump
- simulation
- scene traversal
- `DrawScene` total
- uniform state
- persistent material lookup
- descriptor state
- command recording
- queue submit
- acquire
- present
- UI overlay

計時頻度:

- 1/128 frame
- または1/256 frame

aggregateだけを1秒ごと、または終了時に出力する。

drawごとの `steady_clock::now()` は本番比較では使わない。

---

# 18. allocator telemetry

少なくともdebug/perf modeで次を取る。

- allocations per frame
- bytes allocated per frame
- descriptor-cache lookup count
- descriptor-cache hit / miss
- uniform-cache lookup count
- persistent material lookup count
- `std::vector` growth count
- OpenGL `glGet*` call count
- VAO bind count
- Vulkan host wait count
- Vulkan host wait ns
- queue submit count
- event-pump count per rendered frame

最終目標:

| 項目 | Low Latency Off steady-state |
|---|---:|
| `DrawScene` heap allocation | 0 |
| queue submit helper heap allocation | 0 |
| OpenGL `glGetUniformLocation` | 0 |
| OpenGL `GL_CURRENT_PROGRAM` query | 0 |
| unchanged VAO redundant bind | 0 |
| Vulkan generic latest-submission host wait | 0 |
| event pump | 1 ownership point/frame |
| device-wide idle wait | 0 |

---

# 19. 性能受入基準

## Vulkan

過去実績が約946～995 FPSなので、同じ実対戦条件で次を目標にする。

### 必須

- 平均850 FPS以上
- mean frame interval 1.18 ms以下
- GPU scene timeを悪化させない
- present CPU timeを過去水準から大きく悪化させない
- validation clean
- visual parity維持

### 目標

- 平均950 FPS以上
- mean loop約1.05 ms以下

### 注意

1000 FPSという数字自体を目的にして、描画を省略したりsimulationを壊したりしてはいけない。

同じworkloadを処理した上で回復すること。

---

## OpenGL

過去のGPU scene timeが約1.3～1.6 msだった計測もあるため、OpenGLで1000 FPSを固定目標にするのは妥当ではない。

OpenGLは:

- CPU frontendがGPUより遅くならない
- driver queryによるstallをなくす
- redundant state callsをなくす

ことを成功条件とする。

現行約519 FPSから、同じGPU workloadでCPU側の明確なhotspotsを除去し、GPU-boundへ近付ける。

初期性能目標として650 FPS級を置いてよいが、最終判定はGPU scene timeとCPU loop timeの比較で行う。

---

# 20. 正当性回帰gate

性能修正後も次を必須にする。

## Build / unit

- Windows MSVC Release
- Linux GCC
- macOS Clang
- Android NDK
- CTest full suite

## RHI

- `-rhiconformance`
- Vulkan resource check
- Vulkan validation
- GPU lifetime cycles

## presentation

- Immediate
- FIFO
- Mailbox
- resize
- fullscreen
- minimize / restore
- renderer switch OpenGL → Vulkan → OpenGL

## Low Latency

- Off
- On
- On + Boost
- Reflex available
- Reflex unavailable fallback
- runtime toggle
- no double pacing authority

## image correctness

- Golden Capture
- OpenGL/Vulkan parity
- HUD
- transparent
- decal
- particle
- trail
- fade
- disruption
- backdrop distortion

---

# 21. 禁止事項

性能回復のために次を行わない。

- 描画primitiveの省略
- shader effectの省略
- CPU renderingへのfallback
- textureのCPU-side毎frame copy
- synchronous readback
- hot pathの `vkDeviceWaitIdle`
- hot pathの `vkQueueWaitIdle`
- drawごとの `glGet*`
- drawごとのsystem heap allocation
- semantic descriptor keyの毎draw全再構築
- `std::map` / node-based containerを最頻出draw判定の第一経路にする
- swapchainからplatform event loopを所有する
- Low Latency Offでlatest submissionを毎frame待つ
- metrics/console spamをFPS比較runに混ぜる
- validation ONのFPSをvalidation OFF baselineと比較する

---

# 22. 「low-cycle / no-bottleneck」の定義

このプロジェクトでは次の状態を「no-bottleneck」に近い状態と定義する。

### CPU

- state変更がないdrawはfixed-array compare中心
- hot path allocation 0
- immutable情報のdriver query 0
- resource mutation時だけcache update
- queue submit helper allocation 0

### GPU

- unnecessary idle / full-pipeline drainなし
- semaphore/fenceで必要最小限の依存
- Low Latency OffではCPU/GPU overlapを維持

### architecture

- window event
- pacing
- swapchain
- submission tracking
- resource lifetime
- binding cache
- UI interop

の責務を混ぜない。

### measurement

「API call数が減った」だけを最適化成功と扱わず、必ず:

- FPS
- CPU loop time
- GPU scene time
- present CPU time
- allocation
- host waits

を同時に見る。

---

# 23. 最終判断

### Vulkan

1000 FPS近くから約500 FPSへの低下は、GPU performance regressionよりも**CPU frontend regression**として扱うべきである。

最も重要なのは、`dd69cb...` 系で導入されたdescriptor / uniform semantic cachingを、
「毎drawで内容をhashして検索するcache」から
「mutation時にdirtyにし、draw時はversionを固定配列で比較するcache」
へ変えること。

`9ce34a...` のPMR node再利用で約16%回復したため、allocator/container overheadが実際の性能に効いていることは既に実測で示されている。

### OpenGL

OpenGLは直近のVAO scratch改善だけで約35%向上しており、同じ問題系統が確認済み。

残る明確なhotspotは:

- `GL_CURRENT_PROGRAM` query
- `glGetUniformLocation`
- repeated `glUniform`
- draw後VAO unbind
- immutable limit query
- generic binding snapshot allocation

である。

### Presentation / latency

Generic Low Latencyの2 ms waitはLow Latency Onの意図的policyであり、Offの約500 FPS原因とは分離する。

Vulkan WSIのhost waits、submit-side vector allocation、重複event pumpはP1として除去する。

---

# 24. 実装者への最短指示

1. **まず `dc1ffe` vs `dd69cb` をLow Latency Off / Unlimited / ImmediateでA/B測定する。**
2. **Vulkan `DrawScene()` から `_uniformCache` tree lookupとfull `DescriptorKey` hash lookupをsteady-state経路から外す。**
3. **material/resource generation drivenの固定stateへ置き換える。**
4. **OpenGLのdraw hot pathから `GL_CURRENT_PROGRAM` / `glGetUniformLocation` を完全撤去する。**
5. **OpenGL VAOをdrawごとに0へ戻さず、explicit interop boundaryでのみstateをinvalidateする。**
6. **`VulkanFrameScheduler::Submit()` のvector allocationを0にする。**
7. **Vulkan swapchainからevent pumpingを削除する。**
8. **acquire / present completionのhost waitを通常frame admissionから分離する。**
9. **同一fixtureでFPS-onlyを再測定する。**
10. **Vulkan 850 FPS未満ならsampling profilerで残りCPU phaseを特定し、950 FPS級まで1つずつ潰す。**
11. **性能gate後にvalidation / RHI / lifetime / Golden Captureを別runで通す。**
12. **性能改善と正当性改善を同じ測定runで混ぜない。**

---

# 25. 参照

## Fruity Prime

- Current audit HEAD<br>
  https://github.com/Zection6V/Fruity-Prime/commit/9ce34a391ad8bb2d08848e8189e413eccf28d2a4

- Vulkan command list<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanCommandListInternal.inc

- Vulkan frame scheduler<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanFrameScheduler.hpp

- Vulkan swapchain<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSwapchain.cpp

- OpenGL device<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/src/MphRead.Native/NativeRuntime/Rhi/OpenGL/OpenGlDevice.cpp

- OpenGL command path<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/src/MphRead.Native/NativeRuntime/Rhi/OpenGL/OpenGlCommandsInternal.inc

- Current performance record<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/docs/todo/done/Fruity-Prime-Gpu-Mesh-Draw-Optimization-2026-10-04.md

- Historical rendering implementation / FPS records<br>
  https://github.com/Zection6V/Fruity-Prime/blob/9ce34a391ad8bb2d08848e8189e413eccf28d2a4/docs/todo/done/Fruity-Prime-Rendering-Architecture-Implementation.md

## Vulkan best practices

- Khronos Vulkan Samples: Synchronizing the CPU and GPU<br>
  https://docs.vulkan.org/samples/latest/samples/performance/wait_idle/README.html

- Khronos Vulkan Guide: Profiling<br>
  https://docs.vulkan.org/guide/latest/profiling.html

- Khronos: Frames in flight<br>
  https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html

- NVIDIA: Vulkan Dos and Don'ts<br>
  https://developer.nvidia.com/blog/vulkan-dos-donts/

Khronosは不要なCPU/GPU同期とpipeline drainを避け、frame resourceをfence ringで管理することを推奨している。NVIDIAもqueue submission完了を待たず次のworkを準備すること、command recording / descriptor update / allocationのCPU costを意識することを明示している。

---

# 26. 監査ステータス

- FPS cap誤設定: **既に修正済み**
- Vulkan GPU bottleneck説: **現在の主要因としては否定**
- Qt Quick menu毎frame描画説: **通常対戦中は否定**
- Vulkan CPU descriptor/uniform bookkeeping: **最重要改善対象**
- OpenGL driver query / state churn: **明確な改善対象**
- Vulkan submit allocation: **明確な改善対象**
- Vulkan WSI host wait: **条件付き改善対象**
- Generic Low Latency 2 ms wait: **Low Latency On時のみ意図的throughput limiter**
- 正確な最初の回帰commit: **`dc1ffe` vs `dd69cb` A/Bで最終確定する**
- 修正実装: **この文書では未実施**
