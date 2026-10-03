# Fruity Prime Vulkan 最適化レビュー — `cpp-port` + melonPrimeDS

## 1. 目的

`liveteklol/Fruity-Prime` の `cpp-port` ブランチと `ag-advania/melonPrimeDS` の Vulkan / renderer infrastructure を調査し、現在の `Zection6V/Fruity-Prime` `develop3_rendering` に取り込む価値がある最適化を整理する。

本書の目的は他実装をそのまま移植することではない。現在の `develop3_rendering` 側の RHI、GPU resource lifetime、upload arena、pipeline cache、swapchain retirement、memory admission などの設計を維持しながら、`cpp-port` の draw hot path と melonPrimeDS の descriptor / presentation / low-latency / publication 設計から、Fruity Prime に適する部分だけを抽出する。

## 2. 比較対象

### liveteklol/Fruity-Prime

- Branch: `cpp-port`
- Commit: `17f80dc860244a26a4ed7ee6b8b1ce2ce8e1b05b`

主な確認対象:

- `cpp/src/render/VulkanWindow.h`
- `cpp/src/render/VulkanWindow.cpp`
- `cpp/src/render/SceneRenderer.h`
- `cpp/src/render/SceneRenderer.cpp`


### ag-advania/melonPrimeDS

- Branch: `main`
- Commit: `c4165c87416902bb13b670e3147ecf05988017ed`

主な確認対象:

- `src/VulkanSync.h`
- `src/VulkanSync.cpp`
- `src/VulkanDescriptors.h`
- `src/VulkanDescriptors.cpp`
- `src/VulkanPresentPacer.h`
- `src/VulkanPresentPacer.cpp`
- `src/VulkanPresentPacingPolicy.h`
- `src/VulkanPresenterFrameBudget.h`
- `src/VulkanMemoryAdmission.h`
- `src/VulkanMemoryTelemetry.h`
- `src/VulkanPipelineCache.h`
- `src/VulkanGpuTimestamp.h`
- `src/RendererOutputRing.h`
- `src/RendererOutputRing.cpp`
- `src/GPU3D_Vulkan.cpp`
- `src/frontend/qt_sdl/MelonPrimeVulkanPresenter.cpp`
- `src/DX12UploadRing.h`
- `src/DX12DescriptorRing.h`
- `src/DX12CommandContext.h`

### Zection6V/Fruity-Prime

- Branch: `develop3_rendering`
- Commit: `6d0590077b6569c268907959bed62342868e6e7a`

主な確認対象:

- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanGraphicsDevice.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanPipelineInternal.inc`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanPipelineCache.*`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanUploadArena.*`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSwapchain.cpp`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSceneInternal.inc`
- `src/MphRead.Native/NativeRuntime/Rhi/Vulkan/VulkanSceneUniforms.hpp`

---

# 3. 結論

`cpp-port` からそのまま移植すべき Vulkan 基盤はほとんどない。

現在の `develop3_rendering` は以下の点ですでに `cpp-port` より高度である。

- submission serial に基づく resource lifetime
- deferred retirement
- persistent upload arena
- frame-local descriptor allocator
- VMA ベース memory management
- persistent native pipeline cache
- pipeline cache の GPU / driver / UUID 検証
- dynamic rendering
- Synchronization2
- swapchain replacement の deferred retirement
- typed presentation status
- GPU diagnostics

一方で、**draw hot path に関しては `cpp-port` の方が単純かつ無駄の少ない部分があり、melonPrimeDS にはそれをさらに体系化した descriptor / presenter 最適化がある。**

特に参考価値が高いのは以下である。

1. redundant pipeline / descriptor bind の抑制
2. per-draw small constants の push constants 化
3. descriptor set を更新頻度で分割する
4. 固定 ABI の descriptor set を起動時に preallocate する
5. bounded persistent texture descriptor cache + frame-local fallback
6. renderer / presenter の backpressure を non-blocking に扱う
7. presentation の run-ahead を明示的な frame budget で制御する
8. `VK_KHR_present_wait2` / `VK_EXT_present_timing` 等を capability-driven に使う
9. specialization constants で「まれにしか変わらない shader 定数」を compile-time fold する
10. GPU timestamp / descriptor / present telemetry を shipping hot path から分離する

したがって最適な方針は、

> `develop3_rendering` の Vulkan 基盤を維持したまま、`cpp-port` の hot-path simplicity と melonPrimeDS の frequency-aware resource / descriptor / presentation policy を RHI 設計へ昇華して取り込む

ことである。

---

# 4. 比較概要

| 項目 | `cpp-port` | `develop3_rendering` | 評価 |
|---|---|---|---|
| Frames in flight | 2 | 2 | 同等 |
| Per-draw constants | Push constants | Uniform block + upload slice + descriptor | `cpp-port` を参考 |
| Pipeline bind cache | あり | Scene path にあり | おおむね同等 |
| Descriptor bind cache | あり | 改善余地あり | `cpp-port` を参考 |
| Texture descriptor cache | Scene lifetime cache | frame-local descriptor allocation中心 | `cpp-port` の思想を参考 |
| Dynamic upload | per-frame mapped buffer | persistent `VulkanUploadArena` | 現行が優秀 |
| Texture upload | immediate submit + `vkQueueWaitIdle` | scheduled upload / serial lifetime | 現行が優秀 |
| Resource destruction | `vkDeviceWaitIdle` 依存あり | deferred retirement | 現行が優秀 |
| Memory allocation | raw `vkAllocateMemory` | VMA + admission / telemetry | 現行が優秀 |
| Pipeline cache | process内 `VkPipelineCache` | persistent validated cache | 現行が大幅に優秀 |
| Rendering model | legacy render pass | dynamic rendering | 現行が優秀 |
| Synchronization | Vulkan 1.x style | Synchronization2 | 現行が優秀 |
| Present pacing | application-side pacingあり | presentation専用pacerは限定的 | `cpp-port` を参考 |
| Swapchain recreation | `vkDeviceWaitIdle` | deferred old-chain retirement | 現行が優秀 |

---

# 5. P1: Redundant descriptor-set bind の削減

## 5.1 `cpp-port` の実装

`SceneRenderer::recordPass()` は現在 bind されている pipeline と descriptor set をローカルに記録している。

概念的には以下である。

```cpp
VkPipeline bound = VK_NULL_HANDLE;
VkDescriptorSet boundSet = VK_NULL_HANDLE;

if (pipeline != bound)
{
    vkCmdBindPipeline(...);
    bound = pipeline;
}

if (item.textureSet != boundSet)
{
    vkCmdBindDescriptorSets(...);
    boundSet = item.textureSet;
}
```

同じ material / texture が連続する場合、不要な `vkCmdBindDescriptorSets()` が記録されない。

## 5.2 現行 `develop3_rendering`

pipeline についてはすでに、

```cpp
if (native.Native() != _boundNative)
{
    vkCmdBindPipeline(...);
    _boundNative = native.Native();
}
```

という抑制が存在する。

一方 descriptor set は `DrawScene()` 内で logical group ごとに処理され、set が再生成されていない場合でも最終的に `vkCmdBindDescriptorSets()` が呼ばれる。

したがって、

```cpp
std::array<VkDescriptorSet, SceneShaderAbi::GroupCount> _boundSets{};
```

のような command-list local state を追加し、

```cpp
if (_boundSets[group] != _sets[group])
{
    vkCmdBindDescriptorSets(...);
    _boundSets[group] = _sets[group];
}
```

とする価値がある。

## 5.3 Reset 条件

以下では `_boundSets` を invalidation する必要がある。

- native pipeline layout が変更されたとき
- scene program が変更されたとき
- command buffer recycle / reset 時
- pipeline variant が変更され、layout compatibility が保証されない場合
- resource destruction により referenced descriptor state が invalidated された場合

ただし Vulkan の pipeline layout compatibility を正しく利用できるなら、必要以上に reset しない方がよい。

## 5.4 優先度

**P1**

実装コストが小さく、RHI の意味論を変更せずに command recording overhead を削減できる。

---

# 6. P1: Per-draw small constants を Push Constants に分離

## 6.1 `cpp-port` の設計

`cpp-port` は draw ごとに変化する値を `DrawConstants` にまとめ、`vkCmdPushConstants()` で送っている。

含まれる値は概ね以下である。

- texture matrix
- diffuse
- ambient
- specular
- alpha
- lighting enable
- polygon mode
- texture enable
- light index
- billboard mode
- pass
- matrix base

これにより draw ごとに、

- uniform buffer slice allocation
- `memcpy`
- descriptor set invalidation
- descriptor allocation
- descriptor update

を行う必要がない。

## 6.2 現行 `develop3_rendering`

現在の scene constant path は、

```text
logical constant write
↓
VulkanSceneUniforms::Block.Generation 更新
↓
VulkanUploadArena から slice 確保
↓
block data memcpy
↓
descriptor group invalidation
↓
descriptor set allocate
↓
vkUpdateDescriptorSets
↓
vkCmdBindDescriptorSets
```

となる可能性がある。

この設計は汎用性と正確性に優れている一方、数十 byte 程度の per-draw data には重い。

## 6.3 推奨分類

constants を更新頻度で明示的に分類する。

```text
Frame Constants
    View
    Projection
    global timing

Scene Constants
    room lights
    fog
    scene-wide state

Material Constants
    material colors
    texture-related state
    alpha / lighting mode

Draw Constants
    matrix index / base
    billboard
    small flags
    draw-local override
```

そのうち `Draw Constants` の小さい部分のみ push constants にする。

## 6.4 RHI として定義する

Vulkan 専用 API を renderer 上位層へ露出させるべきではない。

例えば、

```cpp
CommandList::SetSmallConstants(...)
```

のような logical operation を RHI に追加する。

各 backend では以下へ変換できる。

```text
Vulkan
    vkCmdPushConstants

D3D12
    Root Constants

Metal
    setVertexBytes / setFragmentBytes

OpenGL
    glUniform / small UBO
```

このため、これは Vulkan 固有最適化ではなく、将来の Metal / D3D12 にも有効な RHI 改善となる。

## 6.5 注意点

`cpp-port` の `DrawConstants` をそのままコピーする必要はない。

push constant size は GPU ごとの上限があり、Vulkan の最低保証は 128 bytes である。

したがって、本当に draw ごとに変更される小さな値のみを push constant 化するべきである。

大きな matrix stack などは buffer に残す。

## 6.6 優先度

**P1**

---

# 7. P1～P2: Frame-local Descriptor Binding Cache

## 7.1 `cpp-port` の特徴

`textureSetFor()` は texture / sampler の組み合わせに対して descriptor set をキャッシュしている。

概念的な key は、

```text
model
recolor
textureId
paletteId
sampler mode
```

である。

同じ material state が再利用される場合、

```text
vkAllocateDescriptorSets
vkUpdateDescriptorSets
```

自体が発生しない。

## 7.2 現行実装の特徴

現在の `develop3_rendering` では `VulkanDescriptorAllocator` により frame-local descriptor lifetime はよく管理されている。

一方で scene hot path は、

```text
texture A
texture B
texture A
```

のような sequence で A に戻った場合、同じ descriptor contents であっても新しい set を構築する可能性がある。

現在保持しているのは主として「現在 draw に必要な set」であり、「この frame 中に以前生成した同一 descriptor contents」を検索する semantic cache ではない。

## 7.3 推奨設計

frame-local に以下のような key を持つ。

```text
DescriptorBindingKey
    program / layout identity
    group
    uniform slice identity
    texture image views
    samplers
```

そして、

```text
DescriptorBindingKey
    ↓
VkDescriptorSet
```

を frame slot 内でキャッシュする。

frame fence が完了して descriptor pool を reset するとき、cache も破棄する。

## 7.4 なぜ persistent cache にしないか

最初から scene lifetime cache にすると、

- texture destruction
- sampler destruction
- shader reload
- pipeline layout change
- backend restart
- scene lifetime

との invalidation が複雑になる。

現行の frame-local descriptor allocator と整合させ、descriptor cache も frame-local にする方が安全である。

## 7.5 Push Constants との相性

per-draw constants を push constants へ移すと descriptor group 内で変化する resource が減る。

例えば material group が、

```text
texture
sampler
```

中心になる。

その結果 descriptor cache の hit rate が高くなる。

したがって、

1. push constants
2. descriptor cache
3. redundant bind elimination

はセットで進める価値が高い。

## 7.6 優先度

**P1～P2**

---

# 8. P2: Constant / Buffer を更新頻度で分類する

`cpp-port` では、

```cpp
m_uniformBuffers[2]
m_matrixBuffers[2]
m_dynamicVertexBuffers[2]
m_hudVertexBuffers[2]
```

のように frames-in-flight ごとに CPU visible buffer を保持し、fence が完了した slot へ直接 `memcpy` する。

これは非常に単純で安い。

一方 `develop3_rendering` の `VulkanUploadArena` は、

- persistently mapped page
- dirty range tracking
- submission serial
- reset after completion
- page reuse

を持っており、基盤としてはこちらの方が優秀である。

したがって per-frame buffer implementation をそのまま移植するのではなく、resource を update frequency で分類する思想だけ取り入れる。

推奨分類:

```text
Static GPU Resource
    model vertex/index
    static textures

Frame Persistent
    view/projection
    frame-global values

Scene Persistent
    room lighting
    fog
    long-lived material state

Material
    texture/sampler
    material parameters

Draw Transient
    small constants
    transient geometry
```

---

# 9. P2: FIFO Presentation Pacing

## 9.1 `cpp-port`

`VulkanWindow.cpp` では FIFO present 時、display refresh rate を基準に次 frame の deadline を計算している。

Windows では high-resolution waitable timer を使用し、deadline 直前まで sleep したあと短い yield loop を使う。

目的は FPS を増やすことではない。

主な狙いは、

- FIFO acquire / present 内の driver busy wait を減らす
- CPU usage を減らす
- power consumption を減らす
- frame pacing を安定させる

ことである。

## 9.2 現行 Fruity Prime

現在の `FrameTiming` は、

- 60 Hz simulation
- FPS cap
- catch-up
- stalls
- dropped simulation steps
- measured render FPS

を扱っている。

これは game loop timing であり、presentation deadline control とは責務が異なる。

## 9.3 推奨設計

backend-neutral な `PresentPacer` または `PresentationScheduler` を設ける。

責務:

```text
requested frame cap
display refresh
present mode
last present deadline
CPU sleep strategy
```

backend mapping:

```text
Vulkan
    FIFO / MAILBOX / IMMEDIATE

D3D12
    DXGI Present / waitable swapchain

Metal
    CAMetalDrawable / display link

OpenGL
    swap interval
```

Vulkan 専用 `sleepUntil()` として固定しない方が将来設計として良い。

## 9.4 優先度

**P2**

---

# 10. 現行 Fruity Prime を維持すべき部分

## 10.1 Upload Arena

`cpp-port` の texture upload は、

```text
staging buffer作成
↓
vkAllocateCommandBuffers
↓
copy
↓
vkQueueSubmit
↓
vkQueueWaitIdle
↓
command buffer破棄
↓
staging buffer破棄
```

である。

これは runtime texture creation が増えた場合に GPU pipeline stall の原因になる。

現行の、

```text
VulkanUploadArena
VulkanFrameScheduler
SubmissionSerial
deferred retirement
```

を維持すべきである。

## 10.2 Resource Lifetime

`cpp-port` では以下で `vkDeviceWaitIdle()` が使用される。

- swapchain recreation
- scene replacement
- vertex buffer rebuild
- shutdown
- offscreen grab の一部

通常の resource lifetime 管理に device-wide idle を使うべきではない。

現行の、

```text
SubmissionSerial
↓
completion tracking
↓
RetirementQueue
↓
safe destruction
```

の方向が正しい。

## 10.3 Memory Allocation

`cpp-port` は resource 単位で `vkAllocateMemory()` を呼ぶ設計が中心である。

現行では、

- VMA
- allocation telemetry
- memory admission
- allocation accounting

が存在する。

現行を維持する。

## 10.4 Pipeline Cache

`cpp-port` は単純な `vkCreatePipelineCache(...)` を利用する。

現行の `VulkanPipelineCache` はさらに、

- disk persistence
- vendor ID 検証
- device ID 検証
- driver version 検証
- `pipelineCacheUUID` 検証
- payload size validation
- checksum
- corrupt cache fallback
- atomic replacement
- cache statistics

を持つ。

現行の方が大幅に優れている。

## 10.5 Dynamic Rendering

`cpp-port` は従来の `VkRenderPass` / `VkFramebuffer` モデルを使用する。

現行は dynamic rendering を使用している。

将来の RHI、Metal、D3D12 との conceptual mapping も考えると、dynamic rendering ベースを維持する方がよい。

## 10.6 Swapchain Retirement

`cpp-port` は resize 時に `vkDeviceWaitIdle()` を使う。

現行は old swapchain を retire し、

- present fence
- image reacquisition
- fallback retirement

などで安全な destruction timing を決定する。

現行の方が高度である。

---

# 11. 推奨 Hot Path

最終的には scene draw を以下へ近づける。

```text
Draw
 │
 ├─ Pipeline / PSO changed?
 │      └─ Yes → bind
 │
 ├─ Binding group changed?
 │      └─ Yes
 │           ├─ frame cache lookup
 │           ├─ miss → allocate/update
 │           └─ bind only when different
 │
 ├─ Small draw constants changed?
 │      └─ Yes → push/root/setBytes
 │
 ├─ Vertex/index stream changed?
 │      └─ Yes → bind
 │
 └─ DrawIndexed
```

重要なのは「API call を減らす」ことそのものではなく、logical state transition が発生したときだけ native command を記録することである。

---

# 12. 将来の Metal / D3D12 を考えた対応

今回参考にできる最適化は Vulkan 固有実装として追加しない方がよい。

## 12.1 Small Constants

```text
RHI SmallConstants
    Vulkan → Push Constants
    D3D12  → Root Constants
    Metal  → setVertexBytes / setFragmentBytes
    OpenGL → uniforms / compact UBO
```

## 12.2 Descriptor / Binding Cache

```text
RHI Binding Group Cache
    Vulkan → VkDescriptorSet
    D3D12  → descriptor table / root binding
    Metal  → buffer / texture / sampler binding state
    OpenGL → texture unit / buffer binding state
```

## 12.3 Pipeline Bind Cache

```text
RHI Pipeline State Cache
    Vulkan → VkPipeline
    D3D12  → ID3D12PipelineState
    Metal  → MTLRenderPipelineState
    OpenGL → program + fixed-function state
```

## 12.4 Presentation Pacing

```text
PresentationScheduler
    Vulkan → VkSwapchainKHR
    D3D12  → DXGI swapchain
    Metal  → CAMetalLayer
    OpenGL → platform swap interval
```

この形にすることで Vulkan 最適化が backend-specific hack にならない。

---

# 13. 実装優先順位

## P1-A: Redundant descriptor bind elimination

実施内容:

- command list に currently bound descriptor groups を保持
- identical set の再bindを回避
- pipeline layout change 時のみ適切に invalidate

リスク:

- 低

期待効果:

- CPU command recording overhead削減
- driver overhead削減

## P1-B: Small Draw Constants

実施内容:

- scene constants を更新頻度で分類
- draw-local small values を logical small constants として分離
- Vulkan は push constants へ mapping

リスク:

- 中

注意:

- screenshot parity を維持
- shader ABI layout を固定
- Vulkan 最低保証サイズ内に抑える

## P1-C: Frame-local Descriptor Cache

実施内容:

- descriptor contents の structural key を定義
- frame slot ごとの cache
- pool reset と同時に cache reset

リスク:

- 中

期待効果:

- repeated material / texture draw で descriptor allocation/update 削減

## P2-A: Update-frequency Resource Classification

実施内容:

- Frame
- Scene
- Material
- Draw
- Static

の lifetime / update domain を RHI 内で明文化する。

既存 UploadArena は維持する。

## P2-B: Presentation Pacer

実施内容:

- `FrameTiming` とは別の presentation scheduling layer
- display refresh / frame cap / present mode を統合
- platform-specific waiting mechanism を backend implementation へ分離

---

# 14. 実施しないもの

以下は `cpp-port` から移植しない。

- resource upload ごとの `vkQueueWaitIdle`
- resize ごとの `vkDeviceWaitIdle`
- scene replacement ごとの `vkDeviceWaitIdle`
- raw `vkAllocateMemory` 中心の resource management
- old-style render-pass architecture
- non-persistent pipeline cache
- renderer 内部に直接 Vulkan-specific presentation pacing policy を埋め込む設計

---

# 15. `cpp-port` 単独評価

`cpp-port` は Vulkan renderer 全体として現在の `develop3_rendering` より洗練されているわけではない。

むしろ、

```text
memory
lifetime
upload
pipeline cache
swapchain
synchronization
RHI abstraction
```

については現在の `develop3_rendering` の方が優れている。

しかし `cpp-port` は draw hot path が非常に直接的で、

```text
state changed?
    ↓ yes
native command
```

という最適化が分かりやすく実装されている。

現在の Fruity Prime は RHI を汎用化した結果、一部 hot path で、

```text
logical state
↓
buffer slice
↓
descriptor reconstruction
↓
descriptor bind
```

というコストを負っている。

したがって今後の最適化では RHI を崩すのではなく、

> **RHI 内部へ state caching と frequency-aware binding model を追加する**

のが最も良い。

最も効果が期待できる組み合わせは、

```text
Small Draw Constants
+
Frame-local Descriptor Cache
+
Redundant Binding Elimination
```

である。

この3点は Vulkan の CPU overhead を減らすだけでなく、将来追加する Metal / D3D12 の command encoding にも自然に対応できるため、Fruity Prime 全体の renderer architecture 改善として採用価値が高い。

---

# 16. melonPrimeDS 追加調査の結論

melonPrimeDS の Vulkan 実装には、現在の Fruity Prime にそのまま必要な基盤と、すでに Fruity Prime が同等以上を持つ基盤が混在している。

まず、以下は **現行 Fruity Prime がすでに同等以上** であり、melonPrimeDS から追加移植する必要はない。

- deferred resource destruction
- frame / submission completion tracking
- GPU memory admission
- live memory budget
- persistent Vulkan pipeline cache
- GPU timestamp query の基盤
- upload ring / persistent mapped upload
- swapchain lifetime の fence-based retirement

特に現行 Fruity Prime は、

```text
VulkanFrameScheduler
SubmissionSerial
RetirementQueue
VulkanUploadArena
VulkanMemory + VMA
VulkanPipelineCache
VulkanDescriptorAllocator
```

を持つため、この部分を melonPrimeDS 型へ置き換える意味はない。

一方、melonPrimeDS から追加で参考にする価値が高いのは、

```text
descriptor update-frequency architecture
descriptor preallocation
persistent descriptor cache
non-blocking presenter backpressure
one-frame presentation budget
present-wait / target-time pacing
renderer-output lease ring
specialization constants
telemetry separation
```

である。

---

# 17. P1: Descriptor Set を更新頻度で分割する

## 17.1 melonPrimeDS の設計

`VulkanDescriptors.h` は descriptor binding contract を明示的に、

```text
set 0
    per-frame / rasterizer resources

set 1
    texture resources
```

へ分けている。

コード内コメントでも、

> set 0 は最大でも frame ごとに一度変わる  
> set 1 は texture binding の変更ごとに変わる

という設計意図が明示されている。

重要なのは単なる set 番号ではなく、

> **update frequency が異なる resource を同じ descriptor set に入れない**

ことである。

もし frame-global buffer と per-material texture が同一 set に入っていれば、texture が変わるたびに本来不変な frame descriptor まで再構築する必要がある。

## 17.2 Fruity Prime への適用

Fruity Prime の logical binding groups も、

```text
Frame
Scene
Material
Draw
Post
```

のような update-frequency domain と一致させるべきである。

最低限、

```text
Frame / Scene
    低頻度

Material / Texture
    中頻度

Draw Small Constants
    Push Constants
```

へ分ける。

これにより descriptor cache の invalidation 範囲も狭くなる。

## 17.3 Metal / D3D12 への対応

この設計は Vulkan 専用ではない。

```text
Vulkan
    descriptor sets

D3D12
    descriptor tables / root parameters

Metal
    argument / buffer / texture binding groups

OpenGL
    UBO + texture-unit state
```

の全 backend で同じ logical frequency domain を利用できる。

## 17.4 優先度

**P1**

Push Constants と Descriptor Cache の前提になるため、先に logical grouping を固定する価値が高い。

---

# 18. P1: Fixed ABI Descriptor の Preallocation

## 18.1 melonPrimeDS の方式

melonPrimeDS の `DescriptorPool` は、固定された rasterizer ABI に必要な descriptor set を renderer startup 時にまとめて確保する。

特徴:

```text
pool size
    実際の binding type count × 必要 set 数から算出

VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    使用しない

descriptor sets
    startup 時に全 allocate

per-frame
    allocate しない
```

これにより hot path から `vkAllocateDescriptorSets()` を完全に排除している。

さらに個別 free を使わないため descriptor pool fragmentation の経路も消している。

## 18.2 現行 Fruity Prime との違い

現行の `VulkanDescriptorAllocator` は非常に安全な汎用 allocator であり、

```text
submission slot
    ↓
descriptor pool pages
    ↓
Allocate()
    ↓
fence completion
    ↓
vkResetDescriptorPool()
```

という構造になっている。

これは generic RHI には適している。

ただし SceneShaderAbi のように、

- layout が固定
- 最大 group 数が固定
- 1 frame の必要 set 数を上限化できる

hot path まで毎 frame allocate する必要はない。

## 18.3 推奨するハイブリッド方式

`VulkanDescriptorAllocator` を捨てない。

その上に、

```text
FixedSceneDescriptorSlots
    preallocated

Generic / overflow / diagnostic
    VulkanDescriptorAllocator
```

を置く。

つまり、

```text
fast path
    preallocated slot reuse

slow / generic path
    allocator fallback
```

にする。

これなら RHI の汎用性を失わず、scene renderer の deterministic workload だけ最適化できる。

## 18.4 優先度

**P1**

ただし「最大必要数が明確な binding domain」に限定する。

---

# 19. P1: Persistent Texture Descriptor Cache + Frame-local Fallback

## 19.1 melonPrimeDS の実装

`GPU3D_Vulkan.cpp::AcquireTextureSet()` は、

```text
texture identity
+
sampler
```

を key にした bounded hash cache を持つ。

cache hit:

```text
descriptor updateなし
descriptor allocateなし
既存 VkDescriptorSet を返す
```

cache miss では、persistent descriptor slot に空きがある限りそこへ書く。

persistent capacity が埋まった場合は、

```text
per-frame texture descriptor set
```

へ fallback する。

つまり、

```text
bounded persistent cache
    +
safe per-frame fallback
```

である。

これは `cpp-port` の単純な scene-lifetime descriptor cache より安全である。

## 19.2 Fruity Prime への修正案

前章で推奨した frame-local descriptor cache を基本としつつ、安定した resource identity を持てる場合のみ第二段階として persistent cache を追加する。

推奨順:

```text
Stage 1
    frame-local DescriptorBindingKey cache

Stage 2
    stable Texture/Sampler identity を導入

Stage 3
    bounded persistent Material/Texture cache

Stage 4
    capacity miss は frame-local allocator へ fallback
```

persistent cache の key には raw pointer だけを使わず、

```text
resource identity
generation
view identity
sampler identity
layout identity
```

を含めるべきである。

## 19.3 なぜ bounded にするか

unbounded persistent descriptor cache は、

- texture churn
- dynamic assets
- shader reload
- backend recreation

で増え続ける可能性がある。

melonPrimeDS のように明示的 capacity を設け、

> cache overflow は correctness failure ではなく per-frame fallback

とする方がよい。

## 19.4 優先度

**P1**

Redundant descriptor bind elimination と組み合わせると効果が高い。

---

# 20. P2: Non-blocking Backpressure / Frame Drop Path

## 20.1 melonPrimeDS の方式

`VulkanSync::FrameRing` には通常の blocking `BeginFrame()` だけでなく、

```text
TryBeginFrame()
```

がある。

次の slot の fence がまだ完了していれば使用し、busy なら即座に失敗する。

重要なのは、

> 「GPU が遅れている = 必ず CPU が待つ」

にしない点である。

compositor / presenter のように、

- 古い frame を再表示できる
- 1 frame drop の方が latency 上有利
- simulation correctness に影響しない

経路では待たずに skip できる。

## 20.2 Fruity Prime への適用条件

game scene の primary rendering 自体を無条件で drop するべきではない。

適用対象は、

- optional compositor work
- preview
- thumbnail
- duplicated presentation work
- repeated unchanged frame
- capture preview
- nonessential post-processing

などに限定する。

RHI としては、

```cpp
TryAcquireSubmissionSlot()
```

や、

```cpp
TryBeginNonEssentialWork()
```

のような non-blocking seam が考えられる。

## 20.3 メリット

GPU saturation 時に、

```text
CPU waits
    ↓
input sampling delay
    ↓
latency increase
```

となるのを避けられる。

## 20.4 優先度

**P2**

低 latency モードを本格実装するときに価値が高い。

---

# 21. P2: RendererOutputRing の Lease / Publication Model

## 21.1 melonPrimeDS の方式

`RendererOutputRing` は Vulkan / DX12 共通の backend-neutral な publication ring である。

各 slot について、

```text
published?
presenter が lease 中?
GPU work が完了?
```

を確認し、安全な slot のみ producer が上書きする。

`FindFreeSlot()` は、

- 現在 published 中の slot
- presenter が lease 中の slot
- GPU がまだ使用中の slot

を除外する。

presenter は `AcquireLease()` により refcount を持つ。

## 21.2 Fruity Prime にとっての意味

将来、

```text
Renderer
    ↓
backend-neutral output
    ↓
Presenter / UI compositor
```

を明確に分離する場合に有効である。

特に Metal / D3D12 / Vulkan が同じ presentation contract を共有するなら、

> GPU object を知らない publication protocol

として使える。

## 21.3 直ちに導入する必要はない

現在の Fruity Prime が renderer と presenter を同一 submission path で十分に扱えているなら、ring を追加するだけでは複雑性が増える。

したがって導入条件は、

- renderer と window presentation の非同期化
- cross-backend presenter 共通化
- screenshot / stream / UI consumer との同時利用

が必要になった時点とする。

## 21.4 優先度

**P2～P3 / 条件付き**

---

# 22. P2: Presenter One-Frame Budget と Present Wait

## 22.1 melonPrimeDS の設計

melonPrimeDS は frames-in-flight と swapchain image count を別概念として扱っている。

```text
frames in flight
    CPU が GPU より何 frame 先行できるか

swapchain image count
    presentation surface が持つ image 数
```

を明確に分離している。

さらに low-latency presenter では、

```text
WaitForLatestSubmittedFrame()
```

を使用し、

> 「次に再利用する古い slot」ではなく「直近に submit した frame」

を bounded wait の対象にする。

2-slot ring で next reusable slot だけを見ると、CPU が実質 2 frame 先行できるためである。

## 22.2 Present Wait

`VulkanPresentPacer` は、

- `VK_KHR_present_wait2`
- `VK_KHR_present_wait`
- `VK_EXT_present_timing`
- `VK_GOOGLE_display_timing`
- latest-ready
- NVIDIA low-latency authority
- AMD Anti-Lag authority

を capability / policy に応じて分離している。

特に重要なのは、

```text
previous present を待つ
```

ことと、

```text
this present の target display time を指定する
```

ことを別 mechanism として扱っている点である。

## 22.3 Fruity Prime への推奨

前回の単純な application-side `sleepUntil()` より、

```text
Level 1
    generic CPU deadline pacer

Level 2
    present wait / present ID

Level 3
    target-time scheduling

Level 4
    vendor low-latency API
```

という capability ladder を作る方がよい。

vendor API が active の場合は generic pacer と二重制御しない。

## 22.4 優先度

**P2**

FPS throughput より input-to-photon latency と pacing stability の改善項目。

---

# 23. P3: Specialization Constants の限定利用

## 23.1 melonPrimeDS

`GPU3D_Vulkan.cpp` は、

- screen width
- screen height
- max work tiles

を `VkSpecializationInfo` で pipeline creation 時に与える。

shader 内でこれらから導出される値は SPIR-V specialization により compile-time fold され、runtime 演算を減らせる。

## 23.2 Fruity Prime で使える場所

以下のような、

> pipeline lifetime 中ほぼ不変

な値には有効である。

例:

```text
render-scale class
feature variant
MSAA sample class
rare backend capability branch
static post-process mode
```

## 23.3 使うべきでない場所

頻繁に変わる値を specialization constant にすると pipeline variant が増える。

以下には向かない。

- per-frame viewport size
- camera
- material
- animation
- frequently resized window dimensions
- per-draw state

Fruity Prime はすでに semantic pipeline cache を持つため、specialization key を追加する場合は必ず cache key に含める必要がある。

## 23.4 優先度

**P3 / 条件付き**

profile で shader ALU / branch が実際に問題になった場合のみ。

---

# 24. P2～P3: Telemetry を Hot Path から分離する

melonPrimeDS は GPU timestamp、memory telemetry、descriptor counters などを developer / telemetry build に限定できるようにしている。

特に、

```text
shipping build
    no detailed allocation counters
    no timestamp query fields

telemetry build
    query pools
    stage timing
    descriptor update counters
    allocation buckets
```

という分離が明確である。

Fruity Prime は現在 GPU diagnostics を強化しているため、今後 profiling counter が増えた場合でも、

> 計測のために production hot path の lock / allocation / query を増やさない

という原則を維持した方がよい。

推奨:

```text
always-on
    correctness counters
    fatal diagnostics
    minimal submission statistics

developer-only
    GPU timestamps
    descriptor hit/miss
    per-stage CPU timers
    allocation histograms
    present timing details
```

優先度は **P2～P3**。

---

# 25. melonPrimeDS から「確認材料にはなるが追加不要」な項目

## 25.1 Deferred destruction

melonPrimeDS の `DeferredDestroyQueue` は excellent reference だが、現行 Fruity Prime にはすでに `SubmissionSerial` と retirement queue がある。

追加不要。

## 25.2 Memory admission

melonPrimeDS は live budget、allocation-count limit、largest-allocation limit、安全 reserve を pure policy として評価する。

現行 Fruity Prime の `VulkanMemory` も VMA budget、live heap budget、pending reservation、allocation admission をすでに持つ。

設計確認には使えるが追加移植は不要。

## 25.3 Pipeline cache

melonPrimeDS は device identity を検証して persistent `VkPipelineCache` を扱う。

現行 Fruity Prime の `VulkanPipelineCache` は、

- vendor
- device
- driver
- UUID
- checksum
- atomic file replacement

まで持つ。

現行維持。

## 25.4 Upload ring

melonPrimeDS / DX12 の persistently mapped linear upload ring は良い設計だが、Fruity Prime の `VulkanUploadArena` はすでに同じ目的をより汎用的に実装している。

現行維持。

---

# 26. 統合後の推奨実装順序

## P1-1: Update-frequency Binding Groups

最初に、

```text
Frame
Scene
Material
Draw
```

の resource 更新頻度を ABI と RHI で明確にする。

これが後続最適化の土台。

## P1-2: Small Draw Constants

draw-local small values を push/root/inline constants へ移す。

```text
Vulkan → Push Constants
D3D12  → Root Constants
Metal  → set*Bytes
OpenGL → small uniforms / UBO
```

## P1-3: Redundant Binding Elimination

- pipeline
- descriptor group
- vertex/index streams

について、logical state が変化したときだけ native bind command を記録する。

## P1-4: Frame-local Descriptor Cache

まず安全な frame-local cache を導入する。

## P1-5: Fixed ABI Descriptor Preallocation

最大数が決定できる scene groups について、startup / frame-slot creation 時に set を preallocate する。

generic allocator は fallback として残す。

## P1-6: Bounded Persistent Material / Texture Descriptor Cache

stable resource identity / generation contract を確立した後に導入する。

capacity miss は frame-local fallback。

## P2-1: Presentation Scheduler

`FrameTiming` と分離して、

```text
display deadline
present mode
present wait
target-time scheduling
```

を扱う。

## P2-2: Non-blocking Optional Work

presenter / preview / duplicated frame / optional compositor work には `TryBegin` 系の non-blocking admission を追加する。

## P2-3: One-frame Low-Latency Budget

低 latency mode では latest submitted frame を基準に CPU run-ahead を制御する。

## P2-4: Backend-neutral Output Publication

renderer / presenter の分離が必要になった時点で `RendererOutputRing` 型の lease protocol を導入する。

## P3: Specialization / Vendor Features

profiling evidence が出てから、

- specialization constants
- `VK_KHR_present_wait2`
- `VK_EXT_present_timing`
- `VK_GOOGLE_display_timing`
- `VK_NV_low_latency2`
- AMD Anti-Lag

を capability-driven に追加する。

---

# 27. 統合後の推奨 Hot Path

理想的な scene draw は以下になる。

```text
Draw
 │
 ├─ pipeline variant lookup
 │
 ├─ pipeline changed?
 │      └─ bind only if changed
 │
 ├─ material binding key lookup
 │      ├─ persistent cache hit
 │      ├─ frame cache hit
 │      └─ miss → preallocated/fallback descriptor update
 │
 ├─ descriptor group changed?
 │      └─ bind only if changed
 │
 ├─ small draw constants changed?
 │      └─ push/root/inline constants
 │
 ├─ vertex/index stream changed?
 │      └─ bind only if changed
 │
 └─ DrawIndexed
```

frame boundary:

```text
wait only for slot actually being reused
↓
retire completed GPU resources
↓
reset transient upload/descriptor state
↓
record
↓
submit
```

presentation:

```text
renderer output ready
↓
presenter slot available?
 ├─ yes → present
 └─ no  → wait only when policy requires
          otherwise reuse/drop optional presentation work
```

---

# 28. 統合最終評価

3実装を比較すると役割が明確である。

`cpp-port` から参考にするべきものは、

```text
simple draw-state caching
push constants
direct hot-path design
basic application pacing
```

である。

melonPrimeDS から参考にするべきものは、

```text
update-frequency descriptor architecture
descriptor preallocation
bounded persistent descriptor cache
non-blocking backpressure
presentation frame budget
capability-driven present pacing
backend-neutral output lease ring
optional specialization
telemetry separation
```

である。

一方、現在の Fruity Prime が維持すべき強みは、

```text
generic RHI
SubmissionSerial
timeline-backed scheduler
deferred retirement
VMA
memory admission
persistent pipeline cache
dynamic rendering
Synchronization2
typed presentation lifecycle
cross-backend diagnostics
```

である。

したがって Fruity Prime の Vulkan 最適化は、

> **基盤を melonPrimeDS や cpp-port に置き換えるのではなく、現在の RHI の上に「frequency-aware state caching」と「latency-aware presentation policy」を追加する**

のが最も整合性が高い。

性能面で最初に狙うべき組み合わせは、

```text
Update-frequency Binding Groups
+
Small Draw Constants
+
Descriptor Preallocation / Cache
+
Redundant Binding Elimination
```

である。

その後、

```text
Presentation Scheduler
+
Non-blocking Optional Work
+
One-frame Low-Latency Budget
```

へ進む。

この順序なら、Vulkan の CPU overhead と latency を改善しつつ、将来の Metal / D3D12 backend でも同じ RHI policy を再利用できる。

