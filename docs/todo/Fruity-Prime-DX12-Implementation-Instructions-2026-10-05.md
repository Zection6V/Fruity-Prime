# Fruity Prime C++ Direct3D 12 + NVIDIA Reflex 実装指示

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop4_config`
- 調査時HEAD: `15be03c36f618104fb01d381c2fef17129692b37`
- 作成日: 2026-10-05
- 対象: C++版のみ
- 対象OS: Windows 10/11 x64
- 新規Renderer: Direct3D 12
- 低遅延機能: Off / On / On + Boost
- NVIDIA実装: NVAPI Direct3D Reflex
- UI: 既存Qt Quickを維持
- 既存Renderer: OpenGL / Vulkanを破壊しない

---

# 0. この文書の目的

`develop4_config` の現在のRHI設計に Direct3D 12 backend を追加し、既存の OpenGL / Vulkan と同じ frontend・scene・resource contract の下で動作させる。

DX12版は単なる「画面が出るbackend」にしない。

最終的に以下を満たすこと。

```text
Renderer:
    OpenGL
    Vulkan
    Direct3D 12
    Auto

Low Latency:
    Off
    On
    On + Boost

Display:
    VSync On / Off
    FPS limit
```

Direct3D 12ではNVIDIA GPU上で NVIDIA Reflex Low Latency をネイティブ実装する。

```text
Off
    Reflex measurement markers: ON
    native sleep/pacing: OFF

On
    Reflex Low Latency: ON
    Boost: OFF
    native pacing authority: NVIDIA

On + Boost
    Reflex Low Latency: ON
    Boost: ON
    native pacing authority: NVIDIA
```

NVIDIA Reflex非対応環境では既存のRHI共通fallbackに従う。

```text
requested On
    -> effective On
    -> provider Generic

requested On + Boost
    -> effective On
    -> provider Generic
    -> fallbackReason を保持
```

重要なのは、DX12専用の別ループを作らず、現在の以下のRHI契約へDX12を適合させることである。

```text
GraphicsDevice
CommandList
Swapchain
BackendSession
BackendProvider
ResourceState
SubmissionSerial
FrameContext
LowLatencyMode
LowLatencyCapabilities
LowLatencyDiagnostics
PresentationScheduler
WindowUi
SceneShaderAbi
```

---

# 1. 現状の設計上の前提

## 1.1 `GraphicsBackend::D3D12` は既に存在する

`src/MphRead.Native/NativeRuntime/Rhi/Backend.hpp` には既に概念としてD3D12が存在する。

```cpp
enum class GraphicsBackend : std::uint8_t
{
    OpenGl,
    Vulkan,
    Metal,
    D3D12
};
```

したがって、新しい上位renderer abstractionを追加してはいけない。

D3D12は既存RHIの3番目の実backendとして実装する。

---

## 1.2 現在はregistry/factoryがD3D12を拒否している

現在の主な未実装点は以下。

```text
BackendRegistry.cpp
    D3D12 -> nullptr

BackendFactory.cpp
    D3D12 swapchain -> not implemented

SceneBackend.cpp
    SceneBackendRequest に D3D12 がない
    parser が d3d12 / dx12 を認識しない

SettingsModel.cpp
    Renderer:
        OpenGL
        Vulkan
        Auto
```

ここを段階的に開通させる。

---

## 1.3 `ShaderCodeFormat::Dxil` は既に存在する

`Resources.hpp` には既に以下がある。

```cpp
enum class ShaderCodeFormat : std::uint8_t
{
    SpirV,
    GlslSource,
    Dxil,
    MetalLibrary
};
```

DX12実装のために別のshader abstractionを追加しないこと。

DX12 backendでは `ShaderCodeFormat::Dxil` を正式に使用する。

---

## 1.4 現在のLow Latency contractを変更しない

現在の共通契約は以下。

```cpp
LowLatencyMode::Off
LowLatencyMode::On
LowLatencyMode::OnBoost
```

provider:

```cpp
LowLatencyProvider::None
LowLatencyProvider::Generic
LowLatencyProvider::Nvidia
LowLatencyProvider::Amd
```

authority:

```cpp
PacingAuthority::Generic
PacingAuthority::Native
```

marker:

```text
InputSample
SimulationStart
SimulationEnd
RenderSubmitStart
RenderSubmitEnd
PresentStart
PresentEnd
```

DX12 Reflex用の独自enumや独自設定値を作ってはいけない。

---

# 2. 絶対に維持する設計原則

## 2.1 SRP

役割を以下の単位で分離する。

```text
D3D12Context
    adapter / device / queue / debug layer / DRED

D3D12GraphicsDevice
    RHI GraphicsDevice implementation

D3D12CommandList
    recording / barriers / draw / copies

D3D12Resources
    Buffer / Texture / View / Sampler

D3D12Descriptors
    descriptor heap allocation / recycling

D3D12Pipeline
    root signature / PSO

D3D12FrameSlots
    allocator / command list slot / fence ownership

D3D12UploadArena
    persistent upload ring

D3D12Readback
    asynchronous readback

D3D12Swapchain
    DXGI swapchain / backbuffers / RTV / presentation

D3D12NvidiaReflex
    NVAPI Reflex only

D3D12Interop
    Qt Quick external device/resource interop
```

`D3D12GraphicsDevice.cpp` 1ファイルへ全部押し込まないこと。

---

## 2.2 frontendにD3D12 native objectを漏らさない

禁止:

```cpp
Scene.cpp
RendererGeometry.cpp
Mods/Render/*
```

から以下を直接触ること。

```cpp
ID3D12Device
ID3D12Resource
ID3D12GraphicsCommandList
IDXGISwapChain
D3D12_RESOURCE_STATES
D3D12_CPU_DESCRIPTOR_HANDLE
D3D12_GPU_DESCRIPTOR_HANDLE
```

native typeを知ってよいのは原則として:

```text
NativeRuntime/Rhi/D3D12/*
QtのD3D12 interop境界
backend diagnostic fixture
```

のみ。

---

## 2.3 VulkanをコピーしてAPI名だけ置換しない

VulkanとD3D12は似ている部分があるが、以下は設計が異なる。

```text
VkDescriptorSet
    != D3D12 descriptor heap + root descriptor table

VkPipelineLayout
    != D3D12 root signature

VkRenderPass / dynamic rendering
    != OMSetRenderTargets

VkFence/timeline semaphore
    != ID3D12Fence

VkSwapchainKHR
    != IDXGISwapChain4

VMA
    != D3D12MA

VK_NV_low_latency2
    != NVAPI Direct3D Reflex
```

共通化するのはRHI contractまでにする。

native object lifecycleを無理に1実装へ統合しない。

---

# 3. 推奨ファイル構成

追加:

```text
src/MphRead.Native/NativeRuntime/Rhi/D3D12/
    D3D12BackendProvider.cpp

    D3D12Context.hpp
    D3D12Context.cpp

    D3D12Result.hpp
    D3D12Result.cpp

    D3D12GraphicsDevice.hpp
    D3D12GraphicsDevice.cpp

    D3D12Resources.hpp
    D3D12Resources.cpp

    D3D12Descriptors.hpp
    D3D12Descriptors.cpp

    D3D12Pipeline.hpp
    D3D12Pipeline.cpp

    D3D12CommandList.hpp
    D3D12CommandList.cpp

    D3D12FrameSlots.hpp
    D3D12FrameSlots.cpp

    D3D12UploadArena.hpp
    D3D12UploadArena.cpp

    D3D12Readback.hpp
    D3D12Readback.cpp

    D3D12Swapchain.hpp
    D3D12Swapchain.cpp

    D3D12NvidiaReflex.hpp
    D3D12NvidiaReflex.cpp

    D3D12Interop.hpp
    D3D12Interop.cpp

    D3D12ShaderInterface.hpp
    D3D12ShaderInterface.cpp

    D3D12SceneShaders.hpp
    D3D12SceneShaders.cpp
```

test:

```text
src/MphRead.Native/Testing/
    TestD3D12ResourceState.cpp
    TestD3D12Descriptors.cpp
    TestD3D12FrameSlots.cpp
    TestD3D12UploadArena.cpp
    TestD3D12ShaderInterface.cpp
    TestD3D12NvidiaReflex.cpp
```

build:

```text
cmake/
    FruityD3D12Shaders.cmake

tools/
    generate-d3d12-scene-shaders.py
    verify-d3d12-shader-abi.py
```

ファイル数は多少変更してよいが、責務の境界は維持すること。

---

# 4. Build system

## 4.1 Windows + MSVCを正式なDX12 toolchainとする

DX12 production backendはWindows/MSVCを正式経路にする。

MinGWでDX12をproduction requirementにしない。

既存の:

```text
tools/build/build-cpp.bat msvc
.github/workflows/build_cpp.yml
.github/workflows/native-cpp-windows.yml
```

をDX12のcanonical buildとする。

---

## 4.2 CMake option

追加:

```cmake
option(FRUITY_REQUIRE_D3D12 "Require Direct3D 12 development support" OFF)
```

Windows MSVCでbackendを構築できる場合:

```cmake
target_compile_definitions(fruity_mphread_native PRIVATE
    FRUITY_HAS_D3D12
)
```

非WindowsではD3D12 sourceをcompile対象外にする。

```cmake
if(WIN32)
    ...
endif()
```

Linux/macOS/AndroidへD3D12 header dependencyを漏らさない。

---

## 4.3 Windows native libraries

production target:

```text
d3d12.lib
dxgi.lib
dxguid.lib
```

DXCをproduction runtime依存にしない。

shaderはbuild-timeでDXILへcompileしてC++ headerへembedする。

production実行時に:

```text
dxcompiler.dll
dxil.dll
```

を必要としない設計を優先する。

---

## 4.4 vcpkg

Windows dependency listへ候補として追加:

```text
directx-dxc:x64-windows
spirv-cross:x64-windows
d3d12-memory-allocator:x64-windows
```

更新対象:

```text
tools/ci/dependencies/windows-game.txt
tools/ci/dependencies/windows-owners.txt
```

2026-10-05時点のvcpkgには以下が存在する。

```text
directx-dxc
spirv-cross
d3d12-memory-allocator
```

D3D12MAはresource allocation backendとして採用する。

---

## 4.5 NVAPI dependency

NVAPIはNVIDIA公式SDKを使用する。

推奨:

```text
third_party/nvapi
```

を公式 `NVIDIA/nvapi` の固定commitを指すgit submoduleにする。

重要:

```text
floating main禁止
configure時のlatest download禁止
runtime download禁止
```

固定SHAを使用する。

CI checkoutではsubmoduleを取得する。

例:

```yaml
- uses: actions/checkout@v5
  with:
    submodules: recursive
```

NVAPI production link:

```text
nvapi64.lib
```

actual implementationはNVIDIA driver側に存在するため、アプリへNVAPI DLLを同梱しない。

---

# 5. Phase 1: Backend registration と passive probe

このPhaseではまだ画面を描かない。

## 5.1 registry

`BackendRegistry.cpp`:

```cpp
#if defined(FRUITY_HAS_D3D12)
namespace MphRead::NativeRuntime::Rhi::D3D12
{
    const BackendProvider* ProviderInstance() noexcept;
}
#endif
```

switch:

```cpp
case GraphicsBackend::D3D12:
#if defined(FRUITY_HAS_D3D12)
    return D3D12::ProviderInstance();
#else
    return nullptr;
#endif
```

---

## 5.2 `D3D12BackendProvider`

実装:

```cpp
class D3D12BackendProvider final : public BackendProvider
{
public:
    GraphicsBackend Backend() const noexcept override
    {
        return GraphicsBackend::D3D12;
    }

    std::string ProbePassive(bool windowUi) const override;
    std::unique_ptr<BackendSession> CreateSession(
        const BackendSessionOptions& options) const override;
};
```

---

## 5.3 passive probeでdeviceを常駐作成しない

`ProbePassive()` は軽量probeにする。

確認:

```text
Windowsである
dxgi.dll / d3d12.dllが利用可能
hardware adapterが最低1つ存在
D3D12CreateDevice(..., FL11_0, ...) が通る
```

`windowUi=true` の場合はQt Quick D3D12との互換条件も満たすこと。

software adapterは通常候補から外す。

WARPはdeveloper diagnosticを明示指定した場合だけ許可する。

---

## 5.4 adapter selection

可能なら:

```cpp
IDXGIFactory6::EnumAdapterByGpuPreference(
    ...,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
    ...
)
```

を使う。

除外:

```text
DXGI_ADAPTER_FLAG3_SOFTWARE
```

通常選択順:

```text
1. hardware
2. high performance preference
3. D3D12CreateDevice probe成功
```

adapter LUIDをstartup logに残す。

---

## Phase 1 acceptance

```text
-rhicontract
```

がWindows buildで以下を出せること。

```text
opengl=compiled
vulkan=compiled
d3d12=compiled
ui=qt
```

GPU無しCIでもcompile contractを確認できるようにする。

非Windows:

```text
d3d12=absent
```

で正常。

---

# 6. Phase 2: Device / queue / fence foundation

## 6.1 debug layerはdevice作成前

validation有効時:

```cpp
D3D12GetDebugInterface(...)
ID3D12Debug::EnableDebugLayer()
```

必ず `D3D12CreateDevice` より前。

optional developer envでGPU-based validationを許可してよい。

ただし通常CIで常時GPU-based validationを有効にして極端に遅くしない。

---

## 6.2 DRED

device creation前に可能なら:

```cpp
D3D12GetDebugInterface(
    IID_PPV_ARGS(ID3D12DeviceRemovedExtendedDataSettings*)
)
```

breadcrumbs:

```text
Enable
```

page fault:

```text
Enable
```

device lost時は:

```text
GetDeviceRemovedReason
DRED auto breadcrumbs
DRED page fault
```

をdiagnosticへ含める。

---

## 6.3 device

minimum:

```text
D3D_FEATURE_LEVEL_11_0
```

とする。

Fruity Primeの現在の描画機能にFL12固有機能は不要。

DX12 APIを使うこととFeature Level 12.0を強制することは別である。

Qt Quick D3D12とのinteropに必要なinterface availabilityもprobeする。

---

## 6.4 queue

Phase 1ではqueueを増やさない。

```text
1 x D3D12_COMMAND_LIST_TYPE_DIRECT
```

のみ。

copy queue / compute queueは後回し。

理由:

```text
queue ownership
cross-queue fence
resource state transfer
Qt Quick interop
Reflex marker ordering
```

を最初から複雑化しないため。

---

## 6.5 one global submission fence

deviceにつき:

```text
ID3D12Fence
std::uint64_t nextSubmissionSerial
HANDLE fenceEvent
```

を持つ。

RHIの:

```cpp
SubmissionSerial
```

へそのまま対応させる。

queue submission:

```text
ExecuteCommandLists
Signal(fence, serial)
```

completed:

```cpp
fence->GetCompletedValue()
```

---

## 6.6 `FramesInFlight == 2`

共通定義をそのまま使う。

各slot:

```text
ID3D12CommandAllocator
command list reusable state
upload page/ring cursor
shader-visible descriptor pages
last submitted fence serial
```

allocator reset条件:

```text
GetCompletedValue() >= slot.lastSubmission
```

これを満たす前にresetしてはいけない。

---

## 6.7 `WaitForLatestSubmission`

実装要件:

```text
poll first
already complete -> immediate true
pending -> SetEventOnCompletion
bounded timeout
timeout -> false
success -> collect retirement
```

`PresentationScheduler::FrameBudgetWait` と互換にする。

device-wide `WaitIdle()` をlow latency waitに使ってはいけない。

---

## Phase 2 acceptance

CPU fixtureで:

```text
submission serial monotonic
slot reuse before fence completion rejected
retirement only after completed serial
bounded wait timeout works
WaitIdle closes all retired objects
```

を確認。

---

# 7. Phase 3: D3D12MA memory system

## 7.1 allocator

`D3D12MA::Allocator` をdevice単位で1個持つ。

作成時:

```text
ID3D12Device
IDXGIAdapter
```

を渡す。

resourceごとにallocatorを作成してはいけない。

---

## 7.2 MemoryUsage mapping

### `GpuOnly`

```text
D3D12_HEAP_TYPE_DEFAULT
```

### `CpuToGpu`

```text
D3D12_HEAP_TYPE_UPLOAD
persistently mapped
```

### `GpuToCpu`

```text
D3D12_HEAP_TYPE_READBACK
```

---

## 7.3 upload/readback textureをdirect mapped textureにしない

texture data:

```text
CPU
 -> upload buffer
 -> CopyTextureRegion
 -> DEFAULT heap texture
```

readback:

```text
DEFAULT heap texture
 -> CopyTextureRegion
 -> READBACK buffer
 -> CPU
```

---

## 7.4 common memory budgetへ接続

`IDXGIAdapter3::QueryVideoMemoryInfo` を使い:

```text
DXGI_MEMORY_SEGMENT_GROUP_LOCAL
DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL
```

から:

```cpp
MemoryBudgetSnapshot
```

を構成する。

`MemoryBudget.hpp` の共通admission policyを再利用する。

backend独自の別ルールを作らない。

---

# 8. Phase 4: Resource implementation

## 8.1 Buffer

native owner:

```text
D3D12MA::Allocation
ID3D12Resource
BufferDesc
logical ResourceState
last SubmissionSerial
```

upload/readback bufferの場合はheap typeに応じたnative initial stateを持つ。

---

## 8.2 Texture

保持:

```text
TextureDesc
D3D12MA::Allocation
ID3D12Resource
native DXGI_FORMAT
logical ResourceState
TextureHandle
last SubmissionSerial
```

`TextureHandle` はnative pointerへreinterpret castしない。

既存RHIと同様にbackend-managed stable identityを使う。

---

## 8.3 format mapping

最低限以下を実装。

```text
R8Unorm          -> DXGI_FORMAT_R8_UNORM
RG8Unorm         -> DXGI_FORMAT_R8G8_UNORM
RGBA8Unorm       -> DXGI_FORMAT_R8G8B8A8_UNORM
RGBA8Srgb        -> DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
BGRA8Unorm       -> DXGI_FORMAT_B8G8R8A8_UNORM
BGRA8Srgb        -> DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
R16Float         -> DXGI_FORMAT_R16_FLOAT
RG16Float        -> DXGI_FORMAT_R16G16_FLOAT
RGBA16Float      -> DXGI_FORMAT_R16G16B16A16_FLOAT
R32Float         -> DXGI_FORMAT_R32_FLOAT
RG32Float        -> DXGI_FORMAT_R32G32_FLOAT
RGBA32Float      -> DXGI_FORMAT_R32G32B32A32_FLOAT

D16Unorm         -> DXGI_FORMAT_D16_UNORM
D24UnormS8Uint   -> DXGI_FORMAT_D24_UNORM_S8_UINT
D32Float         -> DXGI_FORMAT_D32_FLOAT
D32FloatS8Uint   -> DXGI_FORMAT_D32_FLOAT_S8X24_UINT
```

---

## 8.4 `RGB8Unorm`

DXGIに通常の3channel `RGB8` texture formatはない。

内部native representation:

```text
DXGI_FORMAT_R8G8B8A8_UNORM
```

を使い:

```text
upload RGB -> RGBA expansion
readback RGBA -> RGB contraction
```

をbackendで行う。

frontendへRGBA化を漏らさない。

---

## 8.5 `RGB32Float`

必要なら:

```text
DXGI_FORMAT_R32G32B32A32_FLOAT
```

へexpandする。

RHI上の論理formatは `RGB32Float` のまま維持する。

---

## 8.6 depth/stencil

D24S8をSRV / DSV両方で使う場合:

```text
resource:
    DXGI_FORMAT_R24G8_TYPELESS

DSV:
    DXGI_FORMAT_D24_UNORM_S8_UINT

depth SRV:
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS

stencil SRV:
    DXGI_FORMAT_X24_TYPELESS_G8_UINT
```

のようにtypeless resource + typed viewで処理する。

packed depth/stencil transfer capabilityはplane copyを実装してtestが通るまで `false` にする。

---

# 9. Phase 5: ResourceState mapping

共通logical stateをnativeへ変換する。

```text
Common
    -> D3D12_RESOURCE_STATE_COMMON

VertexBuffer
    -> D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER

ConstantBuffer
    -> D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER

IndexBuffer
    -> D3D12_RESOURCE_STATE_INDEX_BUFFER

ShaderRead
    -> D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
     | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE

ShaderWrite
    -> D3D12_RESOURCE_STATE_UNORDERED_ACCESS

ColorAttachment
    -> D3D12_RESOURCE_STATE_RENDER_TARGET

DepthStencilRead
    -> D3D12_RESOURCE_STATE_DEPTH_READ

DepthStencilWrite
    -> D3D12_RESOURCE_STATE_DEPTH_WRITE

CopySrc
    -> D3D12_RESOURCE_STATE_COPY_SOURCE

CopyDst
    -> D3D12_RESOURCE_STATE_COPY_DEST

Present
    -> D3D12_RESOURCE_STATE_PRESENT
```

read-only composite statesはbitwise ORする。

---

## 9.1 `Common` と `Present` をlogical trackerで区別する

D3D12ではnative valueが同値になるケースがあっても:

```text
RHI Common
RHI Present
```

は意味が異なる。

logical stateをnative integerだけで保存してはいけない。

特に:

```text
ordinary texture -> Present
```

はRHI contract違反としてrejectし続ける。

---

## 9.2 legacy barriersを最初に使う

Phase 1 productionでは:

```cpp
ID3D12GraphicsCommandList::ResourceBarrier
D3D12_RESOURCE_BARRIER_TYPE_TRANSITION
```

を使用する。

Enhanced Barriersはparity確立後のoptimization phaseへ分離する。

最初から:

```text
legacy barrier path
enhanced barrier path
```

の2系統を持たない。

---

## 9.3 UAV ordering

同じresourceを:

```text
ShaderWrite
 -> ShaderWrite
```

で継続利用する必要がある場合、logical Transitionはequal stateを許さないため、必要なUAV ordering barrierはbackend内部のdispatch/draw boundary policyとして実装する。

将来computeを増やす際に扱う。

不要なら今は実装範囲を広げない。

---

# 10. Phase 6: Descriptor system

D3D12で最もhot-pathを汚しやすい部分なので、最初からbounded allocatorにする。

## 10.1 descriptor heap種類

CPU-visible resource descriptors:

```text
CBV_SRV_UAV
SAMPLER
RTV
DSV
```

shader-visible:

```text
CBV_SRV_UAV
SAMPLER
```

---

## 10.2 RTV/DSV

texture/view作成時にCPU descriptorを作成。

drawごとにRTV/DSV descriptorを作らない。

---

## 10.3 shader-visible page

各command/frame slotへ:

```text
CBV_SRV_UAV page
SAMPLER page
```

を持つ。

pageはGPU completion後にrecycleする。

毎draw:

```text
CreateDescriptorHeap
```

禁止。

---

## 10.4 BindingLayout mapping

logical group indexをHLSL register spaceへ1:1で対応。

```text
group 0 -> space0
group 1 -> space1
group 2 -> space2
group 3 -> space3
```

binding type:

```text
UniformBuffer
    b{binding}, space{group}

SampledTexture
    t{binding}, space{group}

StorageBuffer / StorageTexture
    u{binding}, space{group}

Sampler
    s{binding}, space{group}
```

`SceneShaderAbi.def` がsingle source of truth。

---

## 10.5 small constants

現在:

```text
DrawSmall
group = Draw
binding = 1
```

これをD3D12 root constantsへmapする。

例:

```text
b1, space2
```

root parameter:

```cpp
D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS
```

`smallConstantBytes` の4byte単位で設定。

現在の:

```cpp
SmallDrawConstants
```

は16 bytesなので4 DWORD。

---

## 10.6 root signature

Root Signature 1.1を使用。

pipelineの `PipelineLayout` から生成する。

scene専用にhard-codeしない。

各groupについて:

```text
non-sampler descriptor table
sampler descriptor table
```

必要なものだけroot parameterへ入れる。

small constantsはroot constants。

---

## 10.7 descriptor copy cache

`SetBindingSet` のたびに無条件copyしない。

cache key例:

```text
BindingSet native identity
current command slot
descriptor page generation
```

同一page上で同じbinding setなら再利用。

ただしresource内容が書き換わってdescriptor identityが変わる場合はgenerationを更新する。

---

## 10.8 heap overflow

shader-visible pageが枯渇した場合:

```text
1. 新pageへswitch
2. SetDescriptorHeaps
3. root descriptor table stateをinvalidate
4. 必要なtableを再bind
```

を行う。

`SetDescriptorHeaps` は既存root table bindingへ影響するため、単にheap pointerだけ変更してはいけない。

---

# 11. Phase 7: Pipeline / PSO

## 11.1 one `GraphicsPipelineDesc` -> one cached PSO identity

PSO keyへ最低限含める。

```text
vertex DXIL digest
fragment DXIL digest
root signature/layout
topology
rasterizer
depth/stencil
vertex input layout
blend state
RTV formats
DSV format
sample count
alpha test variant state
small constants size
```

---

## 11.2 topology

RHI:

```text
PointList
LineList
LineStrip
TriangleList
TriangleStrip
```

PSOの:

```text
D3D12_PRIMITIVE_TOPOLOGY_TYPE
```

とdraw時の:

```text
D3D_PRIMITIVE_TOPOLOGY
```

を両方正しく設定する。

---

## 11.3 rasterizer

mapping:

```text
CullMode
FrontFace
FillMode
DepthBias
SlopeScaledDepthBias
```

を行う。

`depthClampEnable` はDX12の `DepthClipEnable` と完全同義ではないため、現行描画で必要な意味をconformance fixtureで固定する。

test無しに「同じ」と仮定しない。

---

## 11.4 blend

`BlendAttachmentDesc` をそのまま:

```text
D3D12_RENDER_TARGET_BLEND_DESC
```

へ変換。

独立blend attachmentを正しく処理する。

---

## 11.5 alpha test

D3D12 fixed function alpha testを作らない。

現在と同じshader-side:

```text
SmallDrawConstants.alphaTest
```

を使用する。

---

# 12. Phase 8: Shader pipeline

ここは最重要。

## 12.1 禁止事項

以下を禁止する。

```text
Shaders.cpp
    +
手書きD3D12Shaders.hlsl
```

の二重管理。

理由:

```text
uniform drift
vertex semantic drift
matrix packing drift
alpha test drift
fog drift
cel shading drift
opening disruption effect drift
```

が起きるため。

---

## 12.2 single logical ABIを維持

single source:

```text
src/MphRead.Native/Shaders.cpp
src/MphRead.Native/NativeRuntime/Rhi/SceneShaderAbi.def
src/MphRead.Native/NativeRuntime/Rhi/VertexSemantics.hpp
```

を維持する。

---

## 12.3 推奨生成経路

既存Vulkan generatorの共通部分を再利用して:

```text
Shaders.cpp
    ↓
scene_shader_abi.py
    ↓
modern GLSL intermediate
    ↓
glslc
    ↓
SPIR-V
    ↓
SPIRV-Cross
    ↓
generated HLSL
    ↓
DXC
    ↓
DXIL
    ↓
embedded C++ header
```

とする。

HLSL自体は生成物。

人間が直接編集しない。

---

## 12.4 Vulkan push constantsだけをそのままround-tripしない

現行Vulkan generatorではsmall constantsが:

```glsl
layout(push_constant)
```

になる。

このままSPIR-V -> HLSLへ変換すると、logical:

```text
group 2
binding 1
```

情報をDX12 mappingへ復元しにくい。

したがってgeneratorを共通化し、D3D12 intermediateではsmall constantsを通常UBOとして保持する。

例:

```glsl
layout(std140, set=2, binding=1) uniform SceneDrawSmall
{
    float mat_alpha;
    int alpha_test;
};
```

SPIRV-Cross後:

```hlsl
cbuffer SceneDrawSmall : register(b1, space2)
```

となる形をcontractにする。

runtimeではこのcbuffer registerへroot constantsをbindする。

---

## 12.5 resource mapping

SM 5.1以降ではSPIRV-Crossのdescriptor setをHLSL register spaceへ対応できる。

目標:

```text
set=0,binding=0 UBO
    -> b0, space0

set=1,binding=1 image
    -> t1, space1

set=1,binding=2 sampler
    -> s2, space1

set=2,binding=1 small
    -> b1, space2
```

自動register割当へ任せず、最終HLSL / DXIL reflectionで固定する。

---

## 12.6 vertex semantics

共通location:

```text
0 Position
1 Normal
2 Color
3 TexCoord
4 TexCoord1
```

DX12:

```text
0 -> POSITION
1 -> NORMAL
2 -> COLOR0
3 -> TEXCOORD0
4 -> TEXCOORD1
```

を生成時に固定。

`VertexSemantics.hpp` から生成するか、generator testで一致を強制する。

---

## 12.7 Shader Model

初期target:

```text
vs_6_0
ps_6_0
```

で十分。

不要にSM 6.8等へ上げない。

---

## 12.8 DXC

Release例:

```text
dxc
    -E main
    -T vs_6_0 / ps_6_0
    -Ges
    -WX
    -O3
```

Debug:

```text
-Od
-Zi
```

matrix packingについて:

```text
-Zpr
-Zpc
```

を根拠なく強制しない。

SPIR-V -> HLSL生成物の実際のmatrix layoutとDXIL reflectionをtestし、現在のC++ `Matrix4` contractと一致させる。

---

## 12.9 clip space

現在Vulkan generatorはOpenGL由来shaderのZを:

```text
[-w, +w]
    ->
[0, +w]
```

へ変換している。

D3D clip depthも `[0,w]` なのでこのmodern clip conversionは共有可能。

一方、Y方向についてはDX12 backend側で勝手な二重flipを入れない。

以下で固定する。

```text
RHI viewport convention
generated shader output
D3D viewport transform
Golden Capture
```

Golden Captureが一致する変換を唯一の仕様とする。

---

## 12.10 shader ABI test

Windows CIでDXIL reflectionを行い以下を検証。

```text
5 programs
10 stages

input semantics
resource register
register space
resource type
constant block size
array length
small constant register
output target
```

既存:

```text
FruityPrime.ShaderInterface
SceneShaderAbiDrift
```

と同等以上のgateを追加。

---

# 13. Phase 9: CommandList

## 13.1 `Begin()`

以下を実行。

```text
slot readiness確認
allocator reset
command list reset
descriptor page reset/reuse
recording=true
```

GPU未完了slotを待たずにresetしてはいけない。

optional producerでは `TryBegin()` contractを維持する。

---

## 13.2 `BeginRendering()`

DX12では:

```text
OMSetRenderTargets
ClearRenderTargetView
ClearDepthStencilView
RSSetViewports
RSSetScissorRects
```

を使う。

Vulkanのrender pass objectを模倣しない。

`loadOp`:

```text
Clear
    -> clear

Load
    -> preserve

DontCare
    -> discard semanticsを許可
```

`storeOp DontCare` は初期実装では正しさを優先して保持してもよい。

最適化はparity後。

---

## 13.3 swapchain rendering

`RenderingInfo.swapchain == true` の場合:

```text
current backbuffer
current RTV
swapchain-owned depth
```

をtargetにする。

通常texture renderingとコードパスを可能な限り共有する。

---

## 13.4 SetPipeline

bind:

```text
ID3D12PipelineState
ID3D12RootSignature
IA primitive topology
```

state cacheを持ち、同一PSO/root signatureを毎draw再bindしない。

---

## 13.5 VB / IB

```text
IASetVertexBuffers
IASetIndexBuffer
```

RHI offsetを正確に反映。

---

## 13.6 viewport / scissor

RHI conventionとD3D12 conventionの差を1か所で変換。

frontend側でbackend別Y補正をしない。

---

## 13.7 copies

```text
CopyBufferRegion
CopyTextureRegion
```

を使用。

texture footprintは:

```cpp
ID3D12Device::GetCopyableFootprints
```

で取得する。

hard-code pitch禁止。

---

# 14. Phase 10: Upload arena

## 14.1 per-slot persistent upload arena

各frame slotにpersistently mapped upload buffer pageを持つ。

```text
CPU writes
    -> memcpy
GPU reads
    -> CBV / copy source
```

---

## 14.2 alignment

constant buffer:

```text
256 bytes
```

texture rows:

```text
D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
```

placement:

```text
D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
```

を守る。

---

## 14.3 large upload

ringに入らない大規模uploadはdedicated upload allocationへfallback。

そのallocationは:

```text
submission serial
```

でretireする。

GPU完了前にfreeしない。

---

# 15. Phase 11: Readback

## 15.1 synchronous `ReadColor`

内部:

```text
source transition -> CopySrc
CopyTextureRegion -> READBACK buffer
submit
fence wait
map
row convert
vertical/orientation contractへ変換
```

既存RHIの:

```text
bottom row first
```

contractを守る。

---

## 15.2 async readback

既存:

```cpp
ReadbackQueue
ReadbackTicket
ReadbackTransfer
```

を使う。

DX12 transfer:

```text
READBACK buffer
fence serial
IsReady:
    GetCompletedValue >= serial
CopyResult:
    Map + copy
```

ticket rejection時にsynchronous fallbackしてはいけない。

---

# 16. Phase 12: DXGI swapchain

## 16.1 flip model

使用:

```text
DXGI_SWAP_EFFECT_FLIP_DISCARD
```

BufferCount:

```text
FramesInFlight
= 2
```

を基本にする。

---

## 16.2 creation

```cpp
IDXGIFactory2::CreateSwapChainForHwnd
```

native HWNDはQt `QWindow::winId()` を通じて取得する。

swapchain backendはQt objectを知らず、渡されたnative handleだけ使用する。

---

## 16.3 tearing capability

factory:

```cpp
CheckFeatureSupport(
    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
    ...
)
```

がtrueの場合だけ:

```text
DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
```

を使用。

---

## 16.4 VSync

RHI:

```text
PresentMode::Fifo
```

DXGI:

```cpp
Present(1, 0)
```

---

## 16.5 VSync Off

RHI:

```text
PresentMode::Immediate
```

windowed/borderless + tearing supported:

```cpp
Present(0, DXGI_PRESENT_ALLOW_TEARING)
```

それ以外:

```cpp
Present(0, 0)
```

---

## 16.6 Mailbox

DXGIにはVulkanのMAILBOXと完全同等のmodeがない。

したがって:

```text
PresentationCapabilities.mailbox = false
```

を原則とする。

requested Mailboxを黙って「native mailbox対応」と報告してはいけない。

必要ならbackend policyでImmediateへ解決し、actual modeを正しくreportする。

---

## 16.7 frame latency waitable object

`DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` は利用可能だが、Reflexと二重pacingを起こさないこと。

重要:

```text
Native Reflex active
    -> waitable objectを別のlow-latency sleepとして毎frame待たない

Generic pacing
    -> fence / scheduler contractに従う
```

Reflex Onで:

```text
NVAPI Sleep
+
DXGI waitable wait
+
WaitForLatestSubmission
```

の三重待ちを絶対に作らない。

初期実装ではwaitable objectをswapchainに持っても、game-loop pacing authorityとして使わない方が安全。

必要性が性能測定で証明されてから追加する。

---

## 16.8 resize

`ResizeBuffers` 前:

```text
current backbuffer references release
RTV release
relevant submissions completion確認
```

device-wide idleを毎resize以外の通常frameで使わない。

resize後:

```text
reacquire buffers
recreate RTV
recreate depth target
logical state = Present
generation++
```

---

## 16.9 minimize

size:

```text
0 x 0
```

ならswapchainを壊さずsuspended扱い。

```text
TryAcquireTexture
    -> TemporarilyUnavailable

TryPresent
    -> TemporarilyUnavailable

AbandonLowLatencyFrame
```

fake present markerを生成しない。

---

## 16.10 DXGI result mapping

例:

```text
S_OK
    -> Ready, accepted=true

DXGI_STATUS_OCCLUDED
    -> TemporarilyUnavailable

DXGI_ERROR_DEVICE_REMOVED
DXGI_ERROR_DEVICE_RESET
DXGI_ERROR_DEVICE_HUNG
    -> DeviceLost

invalid HWND/output loss
    -> SurfaceLost相当
```

`BackendError`へ:

```text
backend = D3D12
kind
HRESULT
message
```

を保存。

---

# 17. Phase 13: Exclusive fullscreen

既存:

```cpp
FullscreenExclusive::Monitor()
```

はWindowsの `HMONITOR` を保持している。

D3D12では対応する `IDXGIOutput` を探し:

```cpp
IDXGISwapChain::SetFullscreenState(TRUE, output)
```

を使用。

退出:

```cpp
SetFullscreenState(FALSE, nullptr)
```

exclusive時:

```text
tearing flag/present parameterの制約
mode switch
window geometry
alt-tab
device loss
```

をtestする。

renderer switch前には必ずexclusiveをreleaseする。

---

# 18. Phase 14: Qt window mode

現在:

```cpp
enum class GraphicsWindowMode
{
    OpenGL,
    NoApi
};
```

で `NoApi` が実質Vulkanとして扱われている。

DX12追加時は曖昧になる。

推奨変更:

```cpp
enum class GraphicsWindowMode : std::uint8_t
{
    OpenGL,
    Vulkan,
    D3D12
};
```

---

## 18.1 Qt surface type

OpenGL:

```cpp
QSurface::OpenGLSurface
```

Vulkan:

```cpp
QSurface::VulkanSurface
```

D3D12:

```text
native HWNDを持つ通常QWindow
VulkanSurfaceを要求しない
```

WindowsではRasterSurface等のnative-window保持に適したsurface typeを使う。

DX12 backend自身がDXGI swapchainを作成する。

---

# 19. Phase 15: Qt Quick D3D12 interop

これは「QtをCPU bitmapへ落としてupload」ではなく、GPU-to-GPUで実装する。

## 19.1 target architecture

```text
Fruity D3D12 device
Fruity D3D12 direct queue
        |
        +-- game render
        |
        +-- Qt Quick offscreen render
        |
        +-- game composite
```

同一deviceを共有する。

可能なら同一queueを共有する。

---

## 19.2 `UiHost`

現在は:

```text
_vulkan
else OpenGL
```

になっているため:

```text
_vulkan
_d3d12
OpenGL
```

へ明示分岐する。

---

## 19.3 graphics API

D3D12:

```cpp
QQuickWindow::setGraphicsApi(
    QSGRendererInterface::Direct3D12
);
```

rendererの `ID3D12Device` をQtへadoptする。

Qt APIとして:

```cpp
QQuickGraphicsDevice::fromDeviceAndContext(...)
```

はD3D12 deviceも扱える。

ただし最終的にはqueue所有権を曖昧にしない設計を優先する。

---

## 19.4 queue共有

最良:

```text
rendererのID3D12Device
rendererのID3D12CommandQueue
```

をQt RHIへimportする。

Qt 6.11系の `QRhiD3D12NativeHandles` が:

```text
dev
commandQueue
```

を保持可能なので、Qt private RHI API使用が許容できるなら:

```text
existing device + existing command queue
    -> QRhi
    -> QQuickGraphicsDevice::fromRhi
```

を推奨する。

これにより別queue間のmanual fence choreographyを避けられる。

---

## 19.5 private Qt APIを避ける場合

Qtが別command queueを作る構成にする場合は必ず:

```text
game queue Signal(F1)
Qt queue Wait(F1)

Qt render

Qt queue Signal(F2)
game queue Wait(F2)
```

を行う。

CPU `WaitForSingleObject` でQt完了を待つ設計にしない。

---

## 19.6 render target

RHIで作ったtextureを:

```text
RGBA8Unorm
Sampled
ColorAttachment
TransferSrc
TransferDst
```

として作成。

external transition:

```text
-> D3D12_RESOURCE_STATE_RENDER_TARGET
```

Qt:

```cpp
QQuickRenderTarget::fromD3D12Texture(
    ID3D12Resource*,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    size
)
```

を使用。

Qt render後:

```text
logical ColorAttachment stateをRHIへadopt
game composite前に ShaderReadへtransition
```

---

## 19.7 interop API

Vulkanの既存interoperabilityと対称にする。

例:

```cpp
namespace Rhi::D3D12
{
    struct InteropDevice
    {
        void* Device;
        void* CommandQueue;
        std::uint64_t AdapterLuid;
    };

    struct InteropResource
    {
        void* Resource;
        std::uint32_t State;
        std::uint32_t Format;
    };

    InteropDevice DescribeDevice(GraphicsDevice&);
    InteropResource PrepareForExternal(
        GraphicsDevice&,
        Texture&,
        ResourceState target);

    void BeginExternalSubmit(GraphicsDevice&);
    void AdoptExternalState(Texture&, ResourceState);
}
```

frontendへD3D12 headersを漏らさないため、Qt boundaryでcastする。

---

## 19.8 `UiOverlay.cpp`

現在scene-presented overlay storage helper名が `Vk()` になっているが、中身はRHI textureでありVulkan専用ではない。

DX12追加時に:

```text
Vk()
```

を:

```text
SceneOverlay()
```

または:

```text
RhiWindow()
```

へrenameする。

意味と名前を一致させる。

---

# 20. Phase 16: Scene shaders / WindowUi

`CreateSceneWindowUi(GraphicsDevice&)` はD3D12でも非nullを返す。

D3D12 WindowUi:

```text
Begin
    current swapchain target bind

DrawTexture
    fullscreen quad
    premultiplied alpha対応

DrawBackdrop
    backdrop program

End
```

をRHI resource/pipelineだけで実装する。

Qt-specific drawing codeをD3D12 WindowUiへ入れない。

---

# 21. Phase 17: NVIDIA Reflex on D3D12

## 21.1 実装方式

Direct3D 12では:

```text
NVAPI
```

を使用する。

Vulkanの:

```text
VK_NV_low_latency2
```

をD3D12へ無理に共通化しない。

共通なのは:

```text
RHI LowLatency contract
frame lifecycle
marker lifecycle
diagnostics
pacing authority
```

まで。

---

## 21.2 class

```text
D3D12NvidiaReflex
```

を `D3D12Swapchain` が所有する。

Vulkan版と同じくswapchain lifetimeに結び付ける。

ただしNVAPIのglobal initialization自体はprocess-global ownershipを分離する。

---

## 21.3 NVAPI lifetime

NVAPI initializationはprocess globalとして扱う。

例:

```text
NvApiRuntime
    call_once NvAPI_Initialize
    refcount active D3D12 Reflex controllers
    unload only after last controller
```

renderer switch:

```text
D3D12 -> Vulkan -> D3D12
```

で無秩序に `NvAPI_Unload()` を挟まない。

---

## 21.4 dispatch abstraction

unit test用にfunction dispatchを注入可能にする。

概念:

```cpp
struct NvApiReflexDispatch
{
    NvAPI_Status (*SetSleepMode)(IUnknown*, NV_SET_SLEEP_MODE_PARAMS*);
    NvAPI_Status (*GetSleepStatus)(IUnknown*, NV_GET_SLEEP_STATUS_PARAMS*);
    NvAPI_Status (*Sleep)(IUnknown*);
    NvAPI_Status (*SetLatencyMarker)(IUnknown*, NV_LATENCY_MARKER_PARAMS*);
    NvAPI_Status (*GetLatency)(IUnknown*, NV_LATENCY_RESULT_PARAMS*);
};
```

実際の公式signatureへ合わせること。

unit testがNVIDIA GPUなしでcontroller state machineを検証できるようにする。

---

# 22. Reflex capability detection

## 22.1 PCI vendor IDだけで判断しない

悪い例:

```cpp
if (vendorId == NVIDIA)
    caps = NvidiaReflex;
```

禁止。

最終authorityはNVAPI call capability。

---

## 22.2 probe

live device作成後:

```text
NvAPI_Initialize
GetSleepStatus
SetSleepMode(Off)
marker path availability
```

を確認。

usableなら:

```cpp
LowLatencyCapabilities{
    .supported = true,
    .boostSupported = true,
    .provider = LowLatencyProvider::Nvidia
}
```

相当。

NVAPI unsupported:

```text
Generic
boost=false
fallback reason
```

へ移行。

---

# 23. Reflex mode mapping

## Off

```text
bLowLatencyMode = false
bLowLatencyBoost = false
```

ただしmeasurement frameは維持する。

---

## On

```text
bLowLatencyMode = true
bLowLatencyBoost = false
```

---

## On + Boost

```text
bLowLatencyMode = true
bLowLatencyBoost = true
```

---

## minimum interval

既存共通:

```cpp
ReflexMinimumIntervalUs(...)
```

の結果をそのまま渡す。

DX12で別計算しない。

---

## marker optimization

SDKの現行contractに従いmarkersを利用する設定を有効にする。

例:

```text
bUseMarkersToOptimize = true
```

その他のoptional tuning fieldはSDK sampleと現行documentationに一致させる。

根拠なく独自値を入れない。

---

# 24. Reflex frame lifecycle

Vulkan版で確立済みのlifecycleをD3D12でも同じにする。

## 24.1 `MeasurementAvailable`

mode Offでもtrueになり得る。

```text
NVAPI marker path usable
```

ならmeasurement frameを開く。

---

## 24.2 `PacingActive`

```text
effective On / OnBoost
+
native NVIDIA provider
```

の場合だけtrue。

---

## 24.3 `BeginFrame`

概念:

```cpp
bool D3D12NvidiaReflex::BeginFrame()
{
    if (!MeasurementAvailable())
        return true;

    if (frameAlreadyOpen)
        return true;

    frameId = ++sequence;
    markerMask = 0;

    if (!PacingActive())
    {
        ready = true;
        return true;
    }

    // Exactly once.
    if (NvAPI_D3D_Sleep(...) fails)
    {
        DisableNativePacingForRuntimeFailure(...);
        ClearPendingAdmission();
        return false;
    }

    ready = true;
    return true;
}
```

---

## 24.4 sleep位置

必須:

```text
NvAPI_D3D_Sleep
    ↓
fresh OS/input collection
    ↓
InputSample marker
    ↓
simulation
```

inputを先に読んでからSleepしてはいけない。

現在の `Renderer.cpp::BeforeFrame()` / `OnInputSample()` contractへ合わせる。

---

# 25. Reflex markers

1 logical frameにつき最大1回ずつ。

```text
InputSample
SimulationStart
SimulationEnd
RenderSubmitStart
RenderSubmitEnd
PresentStart
PresentEnd
```

---

## 25.1 frame ID

64-bit monotonically increasing ID。

swapchain recreate後にold frame IDをreuseしない。

minimizeによるabandon後も次frameは必ず新しいID。

---

## 25.2 InputSample

既存:

```cpp
RenderWindow::OnInputSample()
```

から来る共通hookを使う。

DX12 backend独自input hookを作らない。

---

## 25.3 SimulationStart / End

現在のrenderer側marker位置を維持。

backend側でsimulation timingを推測しない。

---

## 25.4 RenderSubmitStart / End

D3D12 queue:

```text
RenderSubmitStart
ExecuteCommandLists
Signal submission fence
RenderSubmitEnd
```

の順にする。

複数native command listを1 logical frameでsubmitする場合でも、Reflexのlogical render-submit markerを無秩序に増殖させない。

1 game frameのmain render submit boundaryを定義する。

---

## 25.5 PresentStart / End

```text
PresentStart
IDXGISwapChain::Present
PresentEnd
```

API callそのものをmarkerで囲む。

ただしlogical measurement completionは:

```text
accepted present
```

に基づく。

---

# 26. Reflex abandon

以下では:

```cpp
AbandonLowLatencyFrame()
```

を必ず呼ぶ。

```text
zero-sized framebuffer
window minimized and drawable unavailable
swapchain closed
pre-present acquire failure
surface unavailable
renderer switch
device-loss path before accepted present
```

abandon:

```text
current frame id clear
ready clear
pending sleep clear
marker mask clear
abandonedMeasurementFrames++
```

禁止:

```text
fake PresentStart
fake PresentEnd
completedMeasurementFrames++
```

---

# 27. Reflex runtime failure fallback

native call failure:

```text
SetSleepMode failure
Sleep failure
GetSleepStatus failure
SetLatencyMarker critical failure
```

でprocessを落とさない。

同じframeで:

```text
native authority lost
    ↓
ResolveLowLatency
    ↓
Generic On
    ↓
WaitForLatestSubmission
```

へ移行。

requested modeは保持。

例:

```text
requested = OnBoost
effective = On
provider = Generic
authority = Generic
reason = "NVAPI sleep failed ..."
```

renderer/backendを後で再作成し、native capabilityが戻ればrequested OnBoostへ復帰可能にする。

---

# 28. Native ReflexとGeneric waitを同時使用しない

これはmust-fix invariant。

禁止:

```text
NvAPI_D3D_Sleep
+
SceneDevice().WaitForLatestSubmission(...)
```

を同じhealthy native frameで両方行うこと。

現在のrendererは:

```cpp
PacingAuthority::Native
```

ならgeneric waitをスキップする。

DX12側はその契約に適合すること。

---

# 29. DXGI pacingとの二重待ちも禁止

さらに:

```text
NvAPI_D3D_Sleep
+
DXGI_FRAME_LATENCY_WAITABLE_OBJECT wait
```

を毎framepacingとして重ねない。

また:

```text
Sleep()
WaitForLatestSubmission()
WaitableObject()
```

の三重待ちは絶対に禁止。

Reflex OnでFPSが大幅に下がった場合、最初に「別のwaitが重なっていないか」を検証する。

---

# 30. VSync + Reflex

現在の共通rendererはVSyncがONかつnative Reflex時:

```text
display refresh period
```

を `minimumIntervalUs` の下限へ含める。

DX12でもそのまま使用。

例:

```text
540 Hz

refresh period:
    ceil(1,000,000 / 540)
```

を共通helperが計算。

DX12 backend独自で:

```text
Present(1)だからminimumInterval=0
```

のように上書きしない。

---

# 31. Reflex telemetry

既存の:

```text
[reflex-metrics]
[reflex-pacing]
```

形式をbackend-neutralに維持。

D3D12でも以下が読めること。

```text
requested
effective
provider
authority
boost_supported
fallback reason
minimum_interval_us
frame id
completed
abandoned
sleep calls
marker calls
timing reports
```

---

## 31.1 timing query cadence

timing取得は:

```text
completedMeasurementFrames
```

を基準にする。

例:

```text
120 completed framesごと
```

`SleepCalls % 120` を使わない。

Offではsleepが0でもmarker measurementは動くため。

---

## 31.2 Offでもmarkerを維持

必須。

```text
Off:
    SleepCalls unchanged
    low latency mode false
    InputSample marker
    SimulationStart/End
    RenderSubmitStart/End
    PresentStart/End
```

これによりOff/Onのlatency比較を可能にする。

---

# 32. NVIDIA API object ownership

Direct3D 12ではReflex APIごとに公式SDKが要求するobjectを厳密に使用する。

実装時にNVIDIAの現在のReflex SDK sampleをauthorityとする。

設計上は:

```text
SetSleepMode / GetSleepStatus / Sleep
    -> D3D12 device側object

SetLatencyMarker
    -> D3D12 graphics command queue側object
       (現行NVIDIA D3D12 integrationに合わせる)

GetLatency
    -> SDKが指定するdevice object
```

とし、function typedefだけを見て適当なIUnknownを渡さない。

wrapper内部でobject選択を1か所に固定する。

---

# 33. Reflex controller unit test

NVIDIA GPUなしでfake dispatchにより以下を全てtest。

```text
Off opens measurement frame
Off does not sleep
On sleeps exactly once
OnBoost sends boost=true
mode unchanged does not spam SetSleepMode
minimumInterval change updates mode
7 markers exactly once
duplicate marker ignored
FinishFrame closes exactly once
AbandonFrame has no fake present markers
next frame ID > abandoned frame ID
sleep failure -> native disabled
same-frame generic fallback signal
timing poll at 120 completed frames
Off timing polling does not run every frame
swapchain generation does not recreate for mode toggle
```

Vulkan版のcontroller regressionと同程度にする。

---

# 34. Phase 18: BackendSession

D3D12 sessionは:

```text
D3D12Context
D3D12GraphicsDevice
scene shaders
WindowUi factory
validation state
```

を所有。

`PresentsWindow()`:

```text
true
```

。

`CreateSwapchain()`:

```text
D3D12Swapchain
```

。

`Shutdown()`:

```text
stop Reflex
release UI interop
release swapchain
wait only required outstanding GPU work
collect retirement
release device objects
```

順序を固定。

---

# 35. Scene backend request

`SceneBackendRequest`:

```cpp
enum class SceneBackendRequest : std::uint8_t
{
    OpenGL,
    Vulkan,
    D3D12,
    Auto
};
```

parser:

```text
opengl
gl

vulkan
vk

d3d12
dx12
direct3d12

auto
```

canonical saved string:

```text
d3d12
```

。

---

# 36. `Auto` policy

DX12を追加した直後にAuto優先順位を変えない。

初期:

```text
Auto:
    existing Vulkan -> OpenGL policyを維持
```

DX12 acceptanceが全て閉じてからWindowsのみ:

```text
D3D12
    ↓
Vulkan
    ↓
OpenGL
```

へ変更するかを別commitで判断する。

renderer実装commitとdefault policy変更commitを分ける。

理由:

```text
regression原因を分離
rollback容易
existing user behavior維持
```

---

# 37. Settings

`SettingsModel.cpp` のRenderer rowを:

```text
OpenGL
Vulkan
Direct3D 12
Auto
```

へ変更。

mapping:

```cpp
{"opengl", "vulkan", "d3d12", "auto"}
```

---

## 37.1 Low Latency UI

現在の:

```text
Off
On
On + Boost
```

をそのまま使う。

DX12専用の:

```text
NVIDIA Reflex
```

checkboxを追加しない。

---

## 37.2 runtime renderer switching

以下を必須。

```text
OpenGL -> D3D12
Vulkan -> D3D12
D3D12 -> OpenGL
D3D12 -> Vulkan
```

switch時:

```text
scene simulation object保持
game state保持
GPU resourceだけrelease/recreate
Qt host recreate
swapchain recreate
requested Low Latency保持
VSync保持
FPS cap保持
```

---

# 38. Renderer switch cleanup

現在の:

```text
ReleaseGpuForSwitch
BeforeRendererSwitch
UiOverlay::Release
swapchain reset
DetachSceneWindow
window reset
```

contractを使う。

D3D12だけ別のglobal cleanup entry pointをfrontendから呼ばせない。

D3D12 resource destructorはretirement queueとの整合を保つ。

switch前に必要ならsession shutdown boundaryでGPU completionを取る。

---

# 39. Device lost

D3D12 device lostを単なる `std::runtime_error` にしない。

```cpp
BackendError(
    GraphicsBackend::D3D12,
    BackendErrorKind::DeviceLost,
    HRESULT,
    ...
)
```

へ変換。

messageに:

```text
HRESULT
GetDeviceRemovedReason
adapter
driver information
DRED breadcrumbs summary
DRED page fault summary
```

を含める。

renderer recovery contractへ渡せるようにする。

---

# 40. Validation / InfoQueue

validation enable時:

```text
D3D12 debug layer
ID3D12InfoQueue
```

を有効にする。

count:

```text
CORRUPTION
ERROR
```

を `ValidationErrors()` へ反映。

warningはログには出すが、既存CI policyと合わせてerror countへ含めるか明示する。

終了時:

```text
validation errors == 0
```

をruntime gateにする。

---

# 41. Debug labels

common:

```text
BeginDebugLabel
EndDebugLabel
InsertDebugMarker
```

をD3D12 command list eventへmapする。

PIX runtimeが無くても最低限D3D12 event metadataとして機能する形にする。

---

# 42. GPU timestamps

```text
D3D12_QUERY_HEAP_TYPE_TIMESTAMP
ResolveQueryData
readback buffer
GetTimestampFrequency
```

を使用。

`Capabilities.supportsTimestampQueries = true` は実装完了後のみ。

---

# 43. AdapterDescription

startup log例:

```text
Direct3D 12
NVIDIA GeForce RTX 5070 Ti
vendor=0x10de
device=...
luid=...
dedicated=...
shared=...
feature-level=...
```

を出す。

---

# 44. Command hot-path rules

以下をdraw hot-pathから排除する。

```text
CreateCommittedResource
D3D12MA allocation
CreateDescriptorHeap
CreateRootSignature
CreateGraphicsPipelineState
D3D12SerializeVersionedRootSignature
shader compilation
DXC invocation
NVAPI initialization
device query
adapter enumeration
device-wide wait
```

draw hot-pathで許可:

```text
descriptor copy into preallocated page
root table bind
root constants
VB/IB bind
viewport/scissor
Draw/DrawIndexed
```

---

# 45. Performance rules

## 45.1 no per-frame device-wide idle

禁止:

```text
WaitIdle
```

を毎frame呼ぶ。

---

## 45.2 no upload allocation per uniform

uniform updateはper-frame upload arenaへ書く。

small constantsはroot constants。

---

## 45.3 no PSO creation in gameplay loop

PSO cache missをtelemetryで数えられるようにする。

warm stateでPSO create countが増え続けないこと。

---

## 45.4 no descriptor heap churn

descriptor heap/page generation countをmetrics化してよい。

steady stateで毎draw heap createしていないことを確認する。

---

# 46. C++ ownership rules

COMは:

```cpp
Microsoft::WRL::ComPtr<T>
```

またはプロジェクトで統一するCOM RAII wrapperを使う。

raw COM pointer ownershipを混在させない。

外部borrowed pointerは明示して所有しない。

---

# 47. Error helper

`D3D12Result`等で:

```cpp
CheckHr(hr, "CreateGraphicsPipelineState")
```

を用意。

必ず:

```text
operation
HRESULT hex
decoded category
device removed reason if applicable
```

を含める。

---

# 48. CI追加

Windows/MSVC jobへ追加。

## 48.1 compile

```text
FRUITY_HAS_D3D12
D3D12 production sources
DXIL embedded shaders
NVAPI controller
```

をcompile。

---

## 48.2 CPU tests

最低:

```text
FruityPrime.D3D12ResourceState
FruityPrime.D3D12Descriptors
FruityPrime.D3D12FrameSlots
FruityPrime.D3D12UploadArena
FruityPrime.D3D12ShaderInterface
FruityPrime.D3D12NvidiaReflex
```

---

## 48.3 backend contract

Windows:

```text
opengl=compiled
vulkan=compiled
d3d12=compiled
ui=qt
```

を必須。

---

## 48.4 shader reproducibility

独立2回生成:

```text
generated HLSL identical
generated DXIL identical
binding manifest identical
embedded header identical
```

を確認。

DXCのcontainer metadataに非決定値が入る場合は、reproducible compile optionか、意味のあるsectionを正規化して比較する。

「毎回byte identical」を根拠なく要求してfalse failureを作らない。

ただしsource digest / reflection ABIは必ず一致させる。

---

# 49. Windows runtime diagnostics

production executableへdeveloper-only gateを追加。

例:

```text
-d3d12check
-d3d12resourcecheck
-d3d12presentcheck
-d3d12reflexcheck
```

名前は既存Vulkan diagnostics命名へ合わせてよい。

---

# 50. `-d3d12check`

確認:

```text
factory
adapter
device
queue
fence
command allocator
command list
one buffer
clean shutdown
debug errors=0
```

---

# 51. `-d3d12resourcecheck`

確認:

```text
buffer upload
buffer copy
buffer readback

texture upload
texture readback
texture resize

state transitions

RGB8 expansion/contraction
depth target

resource release
retirement

live=0
validation errors=0
```

---

# 52. `-d3d12presentcheck`

確認:

```text
clear present
resize
windowed
borderless
exclusive fullscreen
minimize
restore
VSync On
VSync Off
tearing supported / unsupported handling
clean shutdown
validation errors=0
```

---

# 53. `-d3d12reflexcheck`

NVIDIA GPUで:

```text
Off
On
OnBoost
Off
```

をruntime切替。

device/swapchain recreationなしでmodeが変わること。

matrix:

```text
Present:
    Fifo
    Immediate

Cap:
    60
    144
    240
    Unlimited

VSync:
    Off
    On

LowLatency:
    Off
    On
    OnBoost
```

---

# 54. Reflex acceptance values

NVIDIA対応環境:

## Off

```text
requested=Off
effective=Off
provider=Nvidia
authority=Generic or measurement-only state
sleepCalls does not increase
markers increase
```

`provider`表示について既存共通contractに合わせること。

Offでnative measurementが利用できることとnative pacing authorityを持つことは別。

---

## On

```text
requested=On
effective=On
provider=Nvidia
authority=Native
boost_supported=1
reason=
```

NVAPI:

```text
lowLatencyMode=true
lowLatencyBoost=false
```

---

## On + Boost

```text
requested=OnBoost
effective=OnBoost
provider=Nvidia
authority=Native
boost_supported=1
reason=
```

NVAPI:

```text
lowLatencyMode=true
lowLatencyBoost=true
```

---

# 55. Reflex FPS acceptance

最重要performance gate。

条件:

```text
VSync Off
FPS cap Unlimited
same scene
same resolution
same GPU clock/power condition
same renderer
```

比較:

```text
Off
On
On + Boost
```

NVIDIAの現行integration checklistに合わせ、`On` が `Off` に対して大きくFPSを落とす場合は不合格。

目安:

```text
On:
    Offから約4%以内のFPS差を目標

On + Boost:
    boost動作により多少の差は許容するが、
    重大なframe pacing regressionは禁止
```

特に:

```text
Onだけ半分近いFPS
refresh rateの1/2付近へ固定
一定msのCPU wait
```

が出たら、まず二重pacingを疑う。

---

# 56. 既存Vulkan Reflexから引き継ぐ教訓

`develop4_config` ではVulkan Reflexで、explicit submission attributionによりdriver側waitが約1frame遅れ、On時FPS ceilingが発生した履歴がある。

DX12 implementationでは同種の「Reflexのために追加した同期情報が別のpacingを発生させる」設計を避ける。

DX12ではNVAPI Direct3D Reflexのdocumented frame ID / marker / sleep contractのみ使用する。

独自の:

```text
extra fence wait
extra queue drain
extra maximum frame latency wait
extra present wait
```

をReflex実装へ混ぜない。

---

# 57. Reflex external verification

CIはAPI lifecycleまでは確認できるが、最終hardware gateではNVIDIA toolsを使う。

確認対象:

```text
marker ordering
sleep mode
boost mode
PC latency
simulation latency
render submit latency
driver latency
render queue latency
GPU latency
```

OnがOffよりlatencyを下げることを実機で記録する。

---

# 58. Golden Capture

DX12を「実装完了」とする前にVulkan/OpenGLとの描画parityを確立する。

比較対象:

```text
main scene
HUD
helmet
cel
composite
shift/disruption opening effect
backdrop
launcher Qt overlay
hunter preview
```

---

## 58.1 exact RGB差

既存Golden Capture harnessへDX12を追加する。

同一:

```text
source SHA
game assets
capture point
resolution
render scale
settings
```

で比較。

---

## 58.2 ずれが出た場合

以下の順で切り分ける。

```text
1. shader ABI
2. matrix/clip transform
3. vertex semantics
4. color/sRGB format
5. sampler
6. blend
7. depth compare
8. stencil
9. texture row orientation
10. Qt overlay premultiplied alpha
```

shaderを書き換えて「見た目を合わせる」前にcontract mismatchを特定する。

---

# 59. D3D12-specific conformance cases

最低以下をfixture化。

```text
RGBA8 UNORM vs SRGB
BGRA8 swapchain
D24S8
D32
blend premultiplied
alpha test EqualOne
alpha test LessThanOne
front/back culling
clockwise/counterclockwise
line / triangle strip
scissor
viewport
depth bias
texture nearest/linear
clamp/repeat
uniform arrays
32 matrix stack
64 shift table
192 white table
```

---

# 60. Qt acceptance

Direct3D 12 rendererで:

```text
launcher starts
front screen renders
settings renders
hunter preview renders
match starts
pause menu renders over match
end panel renders
return to launcher
```

全てGPU path。

CPU screenshot/uploadを通常pathにしない。

---

# 61. Renderer switch acceptance

各方向を最低20回loopしてresource lifetimeを見る。

```text
OpenGL -> D3D12 -> OpenGL
Vulkan -> D3D12 -> Vulkan
D3D12 -> Vulkan -> D3D12
```

確認:

```text
scene pointer maintained where contract requires
simulation continues
GPU resources recreated
Qt host recreated
no stale descriptor
no stale ID3D12Resource
no old command queue use
no NVAPI controller dangling
live resource count returns
```

---

# 62. Fullscreen acceptance

Windows:

```text
windowed
borderless
exclusive
Alt+Tab
restore
resize
monitor move
exclusive -> windowed
renderer switch while windowed
renderer switch after exclusive exit
```

device loss / black frame / frozen presentなし。

---

# 63. VSync acceptance

Direct3D12:

```text
VSync On + LowLatency Off
VSync On + LowLatency On
VSync On + LowLatency OnBoost

VSync Off + LowLatency Off
VSync Off + LowLatency On
VSync Off + LowLatency OnBoost
```

VSyncとLow Latencyを独立して変更できる。

設定値同士を暗黙連動させない。

---

# 64. FPS cap acceptance

```text
60
120
144
240
Unlimited
```

を:

```text
Off
On
OnBoost
```

で確認。

native Reflex時はcommon `minimumIntervalUs` がauthority。

さらに別のCPU sleep capを重ねない。

---

# 65. Resource lifetime acceptance

shutdown時:

```text
live textures = 0
live buffers = 0
live samplers = 0
live PSOs/root signatures = 0 or backend cache owner released
retirement queue = 0
readback queue closed
descriptor pages released
swapchain buffers released
NVAPI controller closed
```

をdiagnosticで証明する。

---

# 66. CIを跨ぐ注意

D3D12 production codeを追加しても:

```text
Linux GCC
macOS Clang
Android NDK
```

を壊してはいけない。

全D3D12 includeは:

```cpp
#if defined(_WIN32) && defined(FRUITY_HAS_D3D12)
```

等の適切なboundary内。

common headerへ:

```cpp
#include <d3d12.h>
#include <dxgi1_6.h>
```

を漏らさない。

---

# 67. `tools/check-rhi-isolation.py`

現在の:

```text
Native GL and Vulkan only inside their backends
```

というstatic auditを:

```text
Native GL / Vulkan / D3D12 only inside backend/approved interop boundary
```

へ拡張する。

禁止native token例:

```text
ID3D12
D3D12_
IDXGI
DXGI_
```

がfrontendへ出ていないかCIで検査する。

Qt D3D12 interop fileは明示allow-listする。

---

# 68. Documentation / diagnostics naming

startup:

```text
[render] backend requested d3d12, selected d3d12, ...
[d3d12] adapter ...
[d3d12] feature level ...
[d3d12] validation ...
[presentation] ...
[reflex-pacing] ...
```

に統一。

`dx12` と `d3d12` がログで混在しないようcanonical nameは `d3d12` にする。

UI表示だけ:

```text
Direct3D 12
```

。

---

# 69. 実装Phaseとcommit分割

推奨commit sequence。

## Commit A: build + registry skeleton

```text
FRUITY_HAS_D3D12
dependencies
provider registration
-rhicontract
no rendering
```

Gate:

```text
all platforms build
```

---

## Commit B: device + queue + fence

```text
adapter
device
debug
DRED
queue
submission serial
frame slots
```

Gate:

```text
foundation diagnostic
validation 0
```

---

## Commit C: resources + memory + barriers

```text
D3D12MA
buffer
texture
views
sampler
transitions
upload/readback
```

Gate:

```text
resourcecheck PASS
```

---

## Commit D: descriptors + PSO + command list

```text
root signature
descriptor pages
pipeline
draw
```

Gate:

```text
RHI conformance PASS
```

---

## Commit E: DXIL pipeline

```text
generated HLSL
DXC
embedded DXIL
reflection test
```

Gate:

```text
shader ABI PASS
first offscreen triangle PASS
```

---

## Commit F: swapchain

```text
DXGI flip discard
present
resize
VSync
tearing
fullscreen
```

Gate:

```text
presentcheck PASS
```

---

## Commit G: Qt Quick D3D12

```text
shared device
shared/external queue synchronization
fromD3D12Texture
overlay composite
```

Gate:

```text
launcher/pause/end panel PASS
```

---

## Commit H: renderer selection/switch

```text
-rhi d3d12
Settings Direct3D 12
runtime switch
```

Gate:

```text
OpenGL/Vulkan/D3D12 switch PASS
```

---

## Commit I: NVAPI Reflex

```text
Off markers
On
OnBoost
native pacing
fallback
telemetry
```

Gate:

```text
unit regression
RTX hardware reflexcheck
```

---

## Commit J: parity/performance closeout

```text
Golden Capture
FPS
latency
long switch loop
resource leak
CI
```

Auto priority変更をする場合はさらに別commit。

---

# 70. Must-close acceptance checklist

DX12 taskをCLOSEDにするには全て必要。

```text
[ ] Windows/MSVC production build
[ ] OpenGL build unaffected
[ ] Vulkan build unaffected
[ ] Linux build
[ ] macOS build
[ ] Android builds

[ ] -rhi d3d12 starts
[ ] Direct3D 12 visible in Settings
[ ] Settings Cancel restores renderer/latency behavior
[ ] runtime renderer switching works

[ ] resourcecheck
[ ] presentation check
[ ] validation errors = 0
[ ] DRED diagnostic path tested by controlled failure if feasible

[ ] generated DXIL only
[ ] no manually-maintained duplicate HLSL scene shaders
[ ] DXIL ABI reflection PASS
[ ] Golden Capture parity established

[ ] Qt Quick launcher on D3D12
[ ] Qt pause UI on D3D12
[ ] Qt end panel on D3D12
[ ] no CPU fallback in normal UI path

[ ] VSync On independent
[ ] VSync Off independent
[ ] FPS caps work

[ ] Reflex Off markers
[ ] Reflex On native
[ ] Reflex On + Boost native
[ ] Off -> On -> OnBoost -> Off without device recreate
[ ] native Reflex and generic wait mutually exclusive
[ ] Reflex runtime failure -> Generic On
[ ] abandoned frame lifecycle PASS
[ ] timing polling cadence PASS

[ ] On FPS regression within expected Reflex budget
[ ] no 1/2-refresh or fixed-wait FPS ceiling
[ ] hardware latency measurement confirms On lowers latency

[ ] zero stale D3D12 resources on switch
[ ] zero dangling Qt external resource
[ ] zero dangling NVAPI controller
[ ] clean shutdown
```

---

# 71. 明確なNG設計

## NG 1

```text
DX12 backendだからScene.cppへD3D12分岐
```

禁止。

---

## NG 2

```text
D3D12用HLSLを手動コピー
```

禁止。

---

## NG 3

```text
毎draw descriptor heap作成
```

禁止。

---

## NG 4

```text
毎frame WaitIdle
```

禁止。

---

## NG 5

```text
Reflex On
+
WaitForLatestSubmission
```

禁止。

---

## NG 6

```text
Reflex On
+
DXGI frame latency wait
```

を無条件で重ねる。

禁止。

---

## NG 7

```text
OnBoost unsupported
    -> silently OnBoost表示のまま何もしない
```

禁止。

effective/provider/reasonを正しくreportする。

---

## NG 8

```text
mode toggle
    -> device recreate
```

禁止。

Off / On / OnBoostはlive変更。

---

## NG 9

```text
minimize
    -> old Reflex frame ID保持
```

禁止。

abandonする。

---

## NG 10

```text
NVAPI failure
    -> crash
```

禁止。

Genericへfallback。

---

## NG 11

```text
Qt QuickをCPU imageへreadback
    -> texture upload
```

を通常runtime pathにする。

禁止。

---

## NG 12

```text
D3D12_RESOURCE_STATE_PRESENT == COMMONだから
logical stateも同一扱い
```

禁止。

---

## NG 13

```text
Auto renderer priority変更
+
DX12 implementation
```

を同じ大commitで行う。

禁止。

---

# 72. 推奨最終状態

最終的な構成は以下。

```text
                      Frontend / Scene
                            |
                            v
                    Backend-neutral RHI
                            |
          +-----------------+-----------------+
          |                 |                 |
          v                 v                 v
       OpenGL            Vulkan            D3D12
                            |                 |
                            |                 +-- DXGI
                            |                 +-- D3D12MA
                            |                 +-- DXIL
                            |                 +-- Qt Quick D3D12
                            |                 +-- NVAPI Reflex
                            |
                            +-- VK_NV_low_latency2
```

Low Latency:

```text
Renderer.cpp
    |
    +-- requested Off / On / OnBoost
    |
    +-- ResolveLowLatency()
            |
            +-- Vulkan NVIDIA
            |      -> VK_NV_low_latency2
            |
            +-- D3D12 NVIDIA
            |      -> NVAPI Reflex
            |
            +-- unsupported
                   -> Generic pacing
```

重要なのは:

```text
UI mode
frame lifecycle
markers
fallback
diagnostics
pacing authority
```

は共通に保ち、

```text
native API
sleep mechanism
presentation API
resource ownership
```

だけをbackend側へ閉じ込めることである。

---

# 73. 優先順位

実装優先順位は必ず:

```text
1. Correctness
2. Resource lifetime
3. Validation clean
4. Shader parity
5. Presentation correctness
6. Qt integration
7. Reflex correctness
8. Performance
9. Optional D3D12-specific optimization
```

とする。

最初から:

```text
Enhanced Barriers
copy queue
async compute
bindless
ExecuteIndirect
mesh shaders
```

へ広げない。

Fruity Primeの現在のRHI contractを正確にDX12へ移すことが先。

---

# 74. Phase close order

実作業では以下の順で完遂する。

```text
P0  Build + dependency + registry
P1  Adapter/device/queue/fence
P2  Resource/memory/state
P3  Descriptor/root signature/PSO
P4  Command list/upload/readback
P5  Shader generation + DXIL ABI
P6  Offscreen conformance
P7  DXGI presentation
P8  Qt Quick interop
P9  Runtime renderer switch
P10 NVIDIA Reflex Off/On/OnBoost
P11 Golden Capture parity
P12 Performance/latency validation
P13 Auto rollout decision
```

各PhaseのgateがPASSするまで次へ進めない。

---

# 75. 参考資料

## Fruity Prime

- `https://github.com/Zection6V/Fruity-Prime/tree/develop4_config`

特に確認対象:

```text
src/MphRead.Native/NativeRuntime/Rhi/
src/MphRead.Native/NativeRuntime/Rhi/Vulkan/
src/MphRead.Native/NativeRuntime/Rhi/SceneBackend.cpp
src/MphRead.Native/NativeRuntime/Rhi/LowLatency.hpp
src/MphRead.Native/NativeRuntime/Rhi/Swapchain.hpp
src/MphRead.Native/NativeRuntime/Rhi/PresentationScheduler.hpp
src/MphRead.Native/NativeRuntime/Rhi/SceneShaderAbi.def
src/MphRead.Native/NativeRuntime/Rhi/VertexSemantics.hpp
src/MphRead.Native/Renderer.cpp
src/MphRead.Native.Qt/Platform/QtRendererPlatform.cpp
src/MphRead.Native.Qt/Shell/UiHost.cpp
src/MphRead.Native/Mods/Render/UiOverlay.cpp
CMakeLists.txt
.github/workflows/build_cpp.yml
```

## NVIDIA Reflex

- NVIDIA Reflex SDK  
  `https://developer.nvidia.com/performance-rendering-tools/reflex`

- NVIDIA NVAPI  
  `https://github.com/NVIDIA/nvapi`

Reflex SDKのDirect3D integration sampleを、NVAPI function/object usageの最終authorityとして扱う。

## Microsoft Direct3D 12 / DXGI

- DXGI flip model guidance  
  `https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model`

- DXGI swap chain flags  
  `https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_chain_flag`

- Direct3D 12 programming guide  
  `https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide`

## Qt Quick

- QQuickRenderTarget  
  `https://doc.qt.io/qt-6/qquickrendertarget.html`

- QQuickGraphicsDevice  
  `https://doc.qt.io/qt-6/qquickgraphicsdevice.html`

`QQuickRenderTarget::fromD3D12Texture()` はQt 6.6以降で利用可能。

## Shader toolchain

- SPIRV-Cross  
  `https://github.com/KhronosGroup/SPIRV-Cross`

- DirectX Shader Compiler  
  `https://github.com/microsoft/DirectXShaderCompiler`

## D3D12 memory

- D3D12 Memory Allocator  
  `https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator`

---

# 76. 最終実装判断

Fruity Primeの現状では、DX12は次の形で実装するのが最も整合的である。

```text
Direct3D 12:
    native RHI backend

Memory:
    D3D12MA

Presentation:
    DXGI flip-discard

Shaders:
    existing SceneShaderAbi
        -> generated modern shader
        -> SPIR-V intermediate
        -> generated HLSL
        -> DXC
        -> embedded DXIL

Qt:
    same D3D12 device/resource
    GPU-only offscreen render/composite

Low Latency:
    common LowLatencyMode
        +
    D3D12NvidiaReflex
        +
    NVAPI

Fallback:
    existing Generic pacing

Validation:
    D3D12 debug layer
    InfoQueue
    DRED

Lifetime:
    existing SubmissionSerial / FramesInFlight / retirement contract
```

この構成ならDX12はOpenGL/Vulkanと同じfrontendを共有しつつ、Direct3D 12のnative best practiceを守れる。

また、NVIDIA ReflexについてもVulkan版とUI・telemetry・fallbackの意味を一致させたまま、D3D12ではNVAPIがnative pacingを所有する。

実装完了の判定は「DX12で起動した」ではなく:

```text
RHI conformance
+
Golden Capture parity
+
Qt GPU interop
+
VSync/FPS control
+
Reflex Off/On/OnBoost lifecycle
+
no double pacing
+
clean validation
+
clean resource lifetime
+
hardware FPS/latency validation
```

が全て閉じた時点とする。
