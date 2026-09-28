# Fruity-Prime C++版 OpenGL → RHI → Vulkan 実装計画
## Phase-by-Phase Worker AI Execution Plan

- Repository: `Zection6V/Fruity-Prime`
- Branch: `develop3_rendering`
- Baseline HEAD: `bb8f619da7abbe614ea60765006f290a60938f98`
- Baseline date: 2026-09-28
- Scope: **C++版のみ**
- Immediate implementation targets:
  - Desktop OpenGL
  - Desktop Vulkan
  - Android Vulkan
- Existing Android OpenGLES: **維持する。今回の新規RHI実装の破壊対象にしない**
- Future backends:
  - Metal
  - Direct3D 12
- C#版: **変更禁止**
- Primary owner:
  - `src/MphRead.Native`
- Android platform owner:
  - `src/MphRead.Native.Android`

---

# 0. この文書の役割

この文書は設計メモではなく、**ワーカーAIへPhase単位でそのまま渡して実装を進めるための実行計画**である。

一度に全Phaseを実装させない。

基本運用:

```text
Phase Nを依頼
↓
Phase Nだけ実装
↓
静的監査
↓
build
↓
可能なruntime検証
↓
commit
↓
exact commit CI確認
↓
Phase N完了判定
↓
Phase N+1
```

各Phaseは、前Phaseの完成状態を前提とする。

Phaseを飛ばしてVulkan実装を先に作ってはならない。

---

# 1. 今回の最終目標

今回完成させる構造:

```text
Game / Scene / Entity / HUD / Launcher
                  │
                  ▼
          Renderer Frontend
                  │
                  ▼
        Explicit Render Passes
                  │
                  ▼
                 RHI
        ┌─────────┴─────────┐
        ▼                   ▼
     OpenGL               Vulkan
        │                   │
        └─────────┬─────────┘
                  ▼
                 GPU
```

将来的にはRHIの下だけを追加する。

```text
                 RHI
    ┌────────┬────────┬────────┬────────┐
    ▼        ▼        ▼        ▼
 OpenGL   Vulkan    Metal    D3D12
```

将来Metal/D3D12を追加するとき、

```text
Scene
Renderer Frontend
RenderItem
Material
Mesh
HUD
Render pass ordering
```

を原則変更しなくてよい構造を今回の段階で作る。

---

# 2. 今回やらないこと

今回のOpenGL/Vulkan作業へ以下を混ぜない。

- Metal backend実装
- D3D12 backend実装
- ray tracing
- mesh shader
- bindless全面移行
- async compute
- dedicated transfer queue最適化
- parallel command recording
- renderer threadの大規模再設計
- ECS化
- Render Graph全面導入
- gameplay rewrite
- UI rewrite
- C#版変更
- Android UIの無関係な全面書き換え
- shader表現の芸術的リファクタ
- 無関係な性能最適化

今回の優先順位:

```text
Correctness
↓
Backend separation
↓
OpenGL parity
↓
Vulkan parity
↓
Stability
↓
Performance tuning
```

---

# 3. 現行develop3_renderingの重要なBaseline

Baseline:

```text
develop3_rendering
bb8f619da7abbe614ea60765006f290a60938f98
```

現行C++ Rendererの主な特徴:

```text
src/MphRead.Native/Renderer.cpp
約 6611 lines
GL:: 呼び出し 約 1104 箇所

GL::Begin 約 34
GL::End 約 31
display-list related calls 約 8
GL::UseProgram 約 9
GL::BindTexture 約 40
GL::BindFramebuffer 約 10
GL::Uniform* 約 115
GL::ReadPixels 約 3
```

これは目安であり、実作業開始時に必ず再計測する。

現行構造:

```text
Renderer.cpp
  ↓ direct
OpenTK::Graphics::OpenGL::GL
```

また、

```text
NativeRuntime/Skia/SkiaGpu.cpp
```

もOpenGL GaneshとOpenGL stateに直接依存している。

現行CMake desktop:

```text
find_package(OpenGL REQUIRED)
OpenGL::GL
glfw
Skia[gl,freetype]
```

を前提としている。

現行shader:

```text
GLSL 120
gl_Vertex
gl_Normal
gl_Color
gl_MultiTexCoord0
```

を使用する。

現行desktop window:

```text
GLFW OpenGL compatibility context
```

を作成する。

---

# 4. 最重要設計ルール

## 4.1 OpenGL wrapperをRHIと呼ばない

禁止:

```cpp
rhi.EnableBlend();
rhi.DisableDepth();
rhi.BindTexture();
rhi.UseProgram();
rhi.Begin();
rhi.Vertex();
rhi.End();
```

これはRHIではない。

Vulkan、Metal、D3D12へ不自然なOpenGL state machineを強制する。

---

## 4.2 RHIは明示的GPU API型にする

共通概念:

```text
GraphicsDevice
CommandList
Buffer
Texture
TextureView
Sampler
Shader
GraphicsPipeline
BindingLayout
BindingSet
Swapchain
Fence / completion value
FrameContext
ResourceState
Capabilities
```

---

## 4.3 Backend固有handleをFrontendへ漏らさない

通常コードで禁止:

```text
GLuint
VkImage
VkBuffer
VkPipeline
VkDescriptorSet
VkCommandBuffer
ID3D12Resource
MTLTexture
```

許可範囲:

```text
NativeRuntime/Rhi/OpenGL/*
NativeRuntime/Rhi/Vulkan/*
明示されたSkia interop adapter
platform surface adapter
```

---

## 4.4 Vulkan固有概念をRHI APIにしない

将来D3D12/Metalを追加するため、以下を共通公開API名にしない。

禁止例:

```text
DescriptorSet
DescriptorPool
VkImageLayout
PipelineBarrier2
VkQueue
VkSemaphore
```

共通側では:

```text
BindingSet
BindingAllocator
ResourceState
Transition
GraphicsQueue abstraction
Completion/Fence abstraction
```

とする。

---

## 4.5 Metal/D3D12用stub backendを作らない

今回作るbackend:

```text
OpenGL
Vulkan
```

のみ。

以下は禁止:

```text
MetalDevice.cpp
D3D12Device.cpp
return Unsupported; だけの巨大stub
dummy backend
```

ただしenumやfactoryが将来拡張できる設計にはしてよい。

---

## 4.6 Capabilityで差分を扱う

Frontendで禁止:

```cpp
if (backend == Vulkan) { ... }
if (backend == OpenGL) { ... }
```

必要な差は:

```cpp
if (device.Capabilities().SupportsX) { ... }
```

またはbackend内部で吸収する。

---

# 5. 将来Metal/D3D12へ拡張できるRHI契約

## 5.1 Backend type

例:

```cpp
enum class GraphicsBackend
{
    OpenGL,
    Vulkan,
    Metal,
    D3D12
};
```

今回実装するfactory:

```text
OpenGL
Vulkan
```

のみ。

Metal/D3D12選択要求が来た場合は明確なunsupported error。

dummy implementationへ進まない。

---

## 5.2 Buffer

共通descriptor例:

```cpp
struct BufferDesc
{
    std::size_t Size;
    BufferUsage Usage;
    MemoryUsage Memory;
    std::string DebugName;
};
```

`BufferUsage`:

```text
Vertex
Index
Constant
Storage
TransferSrc
TransferDst
Readback
```

Metal/D3D12でも成立する意味だけを持たせる。

---

## 5.3 Texture

例:

```cpp
struct TextureDesc
{
    uint32_t Width;
    uint32_t Height;
    uint32_t MipLevels;
    TextureFormat Format;
    TextureUsage Usage;
    SampleCount Samples;
    std::string DebugName;
};
```

Usage:

```text
Sampled
ColorAttachment
DepthStencilAttachment
TransferSrc
TransferDst
Storage
Present
```

---

## 5.4 Resource State

共通状態:

```text
Undefined
CopySource
CopyDestination
VertexBuffer
IndexBuffer
ConstantBuffer
ShaderResource
StorageRead
StorageWrite
ColorAttachment
DepthWrite
DepthRead
Present
```

Backend変換:

```text
OpenGL
    → logical state / state cache / memory barrier where required

Vulkan
    → stage
    → access
    → image layout

D3D12 future
    → D3D12_RESOURCE_STATES

Metal future
    → usage / encoder ordering / barriers
```

---

## 5.5 Binding model

共通:

```text
BindingLayout
BindingSet
```

用途:

```text
Frame bindings
Material bindings
Draw/Object bindings
```

Vulkan:

```text
VkDescriptorSetLayout
VkDescriptorSet
```

OpenGL:

```text
texture units
UBO binding points
sampler objects
```

Future D3D12:

```text
Root Signature
Descriptor Table
```

Future Metal:

```text
buffer/texture/sampler slots
argument buffer if later selected
```

---

## 5.6 GraphicsPipelineDesc

共通:

```text
Shader set
Vertex layout
Topology
Rasterizer state
Depth state
Stencil state
Blend state
Color formats
Depth format
Sample count
Binding layout
```

OpenGL backendはこれを、

```text
GL program
+
state bundle
```

へ変換する。

Vulkan backendは、

```text
VkPipeline
```

へ変換する。

---

# 6. 推奨ファイル構成

今回作る:

```text
src/MphRead.Native/
└─ NativeRuntime/
   └─ Rhi/
      ├─ Backend.hpp
      ├─ BackendFactory.hpp
      ├─ BackendFactory.cpp
      ├─ GraphicsDevice.hpp
      ├─ CommandList.hpp
      ├─ Resources.hpp
      ├─ Pipeline.hpp
      ├─ Bindings.hpp
      ├─ Swapchain.hpp
      ├─ FrameContext.hpp
      ├─ Capabilities.hpp
      ├─ ResourceState.hpp
      ├─ Formats.hpp
      │
      ├─ OpenGL/
      │  ├─ GlDevice.hpp
      │  ├─ GlDevice.cpp
      │  ├─ GlCommandList.hpp
      │  ├─ GlCommandList.cpp
      │  ├─ GlResources.hpp
      │  ├─ GlResources.cpp
      │  ├─ GlPipeline.hpp
      │  ├─ GlPipeline.cpp
      │  ├─ GlBindings.hpp
      │  ├─ GlBindings.cpp
      │  ├─ GlSwapchain.hpp
      │  └─ GlSwapchain.cpp
      │
      └─ Vulkan/
         ├─ VkDevice.hpp
         ├─ VkDevice.cpp
         ├─ VkInstance.hpp
         ├─ VkInstance.cpp
         ├─ VkCommandList.hpp
         ├─ VkCommandList.cpp
         ├─ VkResources.hpp
         ├─ VkResources.cpp
         ├─ VkPipeline.hpp
         ├─ VkPipeline.cpp
         ├─ VkBindings.hpp
         ├─ VkBindings.cpp
         ├─ VkSwapchain.hpp
         ├─ VkSwapchain.cpp
         ├─ VkMemory.hpp
         ├─ VkMemory.cpp
         ├─ VkSynchronization.hpp
         └─ VkSynchronization.cpp
```

ファイル数は必要に応じて調整してよい。

ただし、

```text
Rhi/
OpenGL/
Vulkan/
```

の境界は維持する。

---

# 7. Worker AI共通作業規則

各Phaseを担当するAIは必ず以下を行う。

## 作業開始前

1. `develop3_rendering` 最新HEADを取得
2. 前PhaseのcommitがHEADに含まれることを確認
3. 対象ファイルを全読
4. 関連直接caller/calleeを確認
5. 現行挙動を理解してから編集開始

---

## 編集中

禁止:

- unrelated cleanup
- unrelated rename
- formatting-only大量変更
- force push
- C#変更
- APIを通すだけのdummy return
- broad `catch (...) {}` で不具合隠蔽
- silent fallback
- unsupported pathを「成功」と返す
- CIだけ通すための機能無効化

---

## Phase終了時

必ず記録:

```text
Base SHA
Final SHA
Changed files
Added files
Deleted files
Build commands
Test commands
Runtime tests
Static audits
Known limitations
CI run IDs
```

---

# 8. Phase 0 — Baseline固定・計測・Golden capture

## 目的

変更前OpenGLの正解状態を固定する。

このPhaseではrenderer behaviorを変更しない。

---

## 対象

主に:

```text
docs/
tools/
必要ならdebug-only capture helper
```

---

## 作業

### 0.1 最新GL依存集計

以下を記録:

```bash
rg -n "\bGL::" src/MphRead.Native
rg -n "GL::Begin|GL::End" src/MphRead.Native
rg -n "GenLists|NewList|CallList|DeleteLists" src/MphRead.Native
rg -n "gl_Vertex|gl_Normal|gl_Color|gl_MultiTexCoord" src/MphRead.Native
```

対象外分類:

```text
OpenGL backend候補
Skia GL interop
diagnostics
game renderer direct dependency
```

---

### 0.2 GPU resource ownership一覧

`Renderer.hpp/.cpp` から以下の所有者を記録:

```text
shader program
texture
framebuffer
renderbuffer
display list
depth texture
cel texture
screen texture
HUD textures
mask textures
model textures
```

---

### 0.3 Frame orderを書き出す

現行:

```text
simulation
Scene::OnDrawFrame
Scene::OnRenderFrame
Skia Shell::TickUi
UiOverlay
LauncherHunter
SwapBuffers
AfterRenderFrame
```

を実ソースから確認して文書化する。

---

### 0.4 Render pass semanticsを書き出す

特に:

```text
opaque
decal
translucent stencil pre-pass
depth clear
opaque depth rebuild
translucent pass
preview
HUD model
cel outline
RTT composite
HUD objects
fade
UI
present
```

を正確に記録する。

---

### 0.5 Golden image候補

最低限:

```text
Launcher
Offline map
Hunter
room geometry
transparent object
decal
particle
trail
HUD
pause menu
Map Vote
cel off
cel on
cel outline
fog on
fog off
fade
whiteout/disruption
end screen
```

同じ:

```text
resolution
camera
hunter
map
settings
```

で比較できるよう固定する。

---

## 完了条件

- [ ] Baseline SHA記録
- [ ] GL依存数記録
- [ ] render order記録
- [ ] resource ownership表完成
- [ ] golden capture条件固定
- [ ] renderer behavior変更なし

---

## Commit例

```text
Document native renderer baseline before RHI migration
```

---

# 9. Phase 1 — RHI Core型だけ導入

## 目的

まだrenderer挙動を変えず、将来OpenGL/Vulkan/Metal/D3D12で共用できるRHI契約を追加する。

---

## 重要

このPhaseではRenderer.cppをRHIへ全面移行しない。

まず型を固定する。

---

## 追加候補

```text
NativeRuntime/Rhi/Backend.hpp
NativeRuntime/Rhi/GraphicsDevice.hpp
NativeRuntime/Rhi/CommandList.hpp
NativeRuntime/Rhi/Resources.hpp
NativeRuntime/Rhi/Pipeline.hpp
NativeRuntime/Rhi/Bindings.hpp
NativeRuntime/Rhi/ResourceState.hpp
NativeRuntime/Rhi/Capabilities.hpp
NativeRuntime/Rhi/Swapchain.hpp
```

---

## 実装

### 1.1 Handle設計

推奨:

```text
move-only owning object
または
typed opaque handle + device ownership
```

避ける:

```text
int textureId
void* nativeHandle
uint64_t nativeObject
```

をFrontend APIへ公開すること。

---

### 1.2 Resource descriptors

作る:

```text
BufferDesc
TextureDesc
TextureViewDesc
SamplerDesc
ShaderDesc
GraphicsPipelineDesc
BindingLayoutDesc
BindingSetDesc
SwapchainDesc
```

---

### 1.3 Enum

最低限:

```text
GraphicsBackend
BufferUsage
TextureUsage
TextureFormat
MemoryUsage
ShaderStage
PrimitiveTopology
CullMode
FrontFace
FillMode
CompareOp
StencilOp
BlendFactor
BlendOp
ColorWriteMask
ResourceState
LoadOp
StoreOp
```

---

### 1.4 CommandList contract

最低限:

```text
Begin()
End()

BeginRendering()
EndRendering()

SetPipeline()
SetViewport()
SetScissor()

SetVertexBuffer()
SetIndexBuffer()

SetBindingSet()

SetStencilReference()

Draw()
DrawIndexed()

CopyBuffer()
CopyBufferToTexture()
CopyTextureToBuffer()

Transition()
```

---

### 1.5 将来APIに備える

RHI interfaceへOpenGL/Vulkan固有名を入れない。

将来:

```text
Metal
D3D12
```

をbackend実装だけで追加できることをコードレビューする。

---

## Unit/static tests

最低限compile test。

可能なら:

```text
descriptor equality
pipeline key hashing
format mapping helper
resource-state validation
```

を純C++ test化する。

---

## 完了条件

- [ ] RHI core compile
- [ ] Renderer behavior変更なし
- [ ] raw GL/Vulkan typeがcommon RHI headerにない
- [ ] Metal/D3D12を追加可能なinterface
- [ ] dummy Metal/D3D12 implementationなし

---

## Commit例

```text
Add backend-neutral RHI core contracts
```

---

# 10. Phase 2 — Backend FactoryとWindow/Presentation分離

## 目的

現在:

```text
Window creation
=
OpenGL context creation
```

になっている構造を分離する。

---

## 対象

```text
Renderer.hpp
NativeRuntime/OpenTK/RendererPlatform.cpp
Mods/Render/DesktopGlContext.*
NativeRuntime/Rhi/BackendFactory.*
NativeRuntime/Rhi/Swapchain.*
```

---

## 2.1 Window責務

残す:

```text
OS window
event loop
input
focus
size
position
fullscreen
native handle
```

外す:

```text
必ずOpenGL contextを作る
SwapBuffersが唯一のpresent手段
```

---

## 2.2 Window creation mode

導入:

```text
GraphicsWindowMode::OpenGL
GraphicsWindowMode::NoApi
```

OpenGL:

```text
GLFW_OPENGL_API
context hints
make current
swap interval
```

Vulkan:

```text
GLFW_CLIENT_API = GLFW_NO_API
```

---

## 2.3 Presentation ownership

最終的に:

```text
RHI Swapchain::Present()
```

がpresentを担当。

OpenGL backend:

```text
glfwSwapBuffers
```

Vulkan backend:

```text
vkQueuePresentKHR
```

---

## 2.4 既存OpenGL pathを壊さない

このPhase終了時点ではOpenGL rendererは旧描画ロジックのままでよい。

目的はwindow/presentation boundaryのみ。

---

## 完了条件

- [ ] OpenGL window従来動作
- [ ] NoApi windowを生成可能
- [ ] Vulkan device未実装でもNoApi window compile
- [ ] input/window behaviorに差なし
- [ ] RendererPlatformにVulkan型なし

---

## Runtime test

```text
launcher open
resize
maximize
fullscreen
minimize
restore
focus
mouse capture
close
```

---

## Commit例

```text
Separate native window ownership from graphics presentation
```

---

# 11. Phase 3 — Geometry Decoder導入

## 目的

最大のVulkan blockerである:

```text
GL::Begin
GL::Vertex
GL::Normal
GL::Color
GL::TexCoord
display lists
```

をRenderer frontendから排除できるデータ形式へ変換する。

このPhaseではまだOpenGLで描画する。

---

## 対象

主に:

```text
Renderer.cpp
Renderer.hpp
Formats / Model data direct dependencies
新規 RendererGeometry.*
または NativeRuntime/Rhi-independent geometry helper
```

---

## 3.1 Vertex format

最低限:

```cpp
struct SceneVertex
{
    Vector3 Position;
    Vector3 Normal;
    Vector4 Color;
    Vector2 TexCoord;
    uint32_t MatrixIndex;
};
```

packing最適化は後回し。

---

## 3.2 Geometry decoder

現行 `DoDlist()` のstate machineを、

```text
OpenGL immediate command emitter
```

から、

```text
CPU geometry builder
```

へ置換する。

入力:

```text
RenderInstructionList
texture width
texture height
texgen
isRoom
```

出力:

```text
vertices
indices
topology ranges
```

---

## 3.3 Current attribute stateを再現

RenderInstructionは状態持続型なので、

```text
current color
current normal
current texcoord
current position
current matrix index
```

を持つ。

Vertex命令が来た瞬間の状態をvertexへ焼く。

---

## 3.4 Primitive変換

対応:

```text
Triangles
Quads
TriangleStrip
QuadStrip
```

Primitive単位でrangeを保持してもよい。

GPU backendが理解しやすい形として最終的に:

```text
Triangles
Lines
```

中心へnormalizeしてよい。

ただし意味が完全一致すること。

---

## 3.5 Quads

OpenGL GL_QUADS依存を排除するためtriangleへ展開。

三角形分割方向は現行描画結果から決める。

勝手に対角線を選ばない。

---

## 3.6 Strip

TriangleStripではwinding parityに注意。

QuadStripでは元OpenGL semanticsを忠実に展開する。

---

## 3.7 MTX_RESTORE

現行:

```text
matrixId
```

をtexcoord Z経由でshaderへ渡している。

これを明示的:

```text
MatrixIndex
```

vertex attributeへする。

現行bit mask:

```text
& 0x1F
```

とclamp意味を維持する。

---

## 3.8 DIF_AMB

現行shaderはcolor alphaを特殊sentinelとして利用している。

意味を分析し、

```text
explicit flag
```

へ変更するのが望ましい。

ただし最初はvertex color alphaの既存意味を維持してもよい。

---

## 3.9 Decoder test

現行GL streamと新geometry outputを比較できるtestを作る。

最低限:

```text
single triangle
quad
triangle strip
quad strip
COLOR inheritance
NORMAL inheritance
TEXCOORD inheritance
VTX_16
VTX_10
VTX_XY
VTX_XZ
VTX_YZ
VTX_DIFF
MTX_RESTORE
DIF_AMB
```

---

## 完了条件

- [ ] Geometry decoder存在
- [ ] Decoder pure C++ test可能
- [ ] 全RenderInstruction対応
- [ ] 現行OpenGL描画結果をまだ維持
- [ ] Vulkan code未導入

---

## Commit例

```text
Decode NDS render instructions into explicit geometry
```

---

# 12. Phase 4 — OpenGL Display List撤去・VBO/IBO化

## 目的

OpenGL自身をmodern GPU buffer modelへ移す。

Vulkanより先に実施する。

---

## 4.1 Model GPU cache

新しいbackend-neutral identity:

```text
GpuMesh
```

概念:

```text
VertexBuffer
IndexBuffer
IndexCount
Topology
```

---

## 4.2 Ownership

禁止:

```text
Mesh::ListId = GLuint
```

Model/Mesh domain objectへOpenGL IDを保存しない。

推奨:

```text
GpuMeshCache
key = Model identity + Mesh identity
```

---

## 4.3 OpenGL buffer implementation

最低限:

```text
glGenBuffers
glBindBuffer
glBufferData
glVertexAttribPointer
glEnableVertexAttribArray
glDrawElements / glDrawArrays
```

OpenGL compatibility contextをこのPhaseで即削除する必要はない。

---

## 4.4 Dynamic geometry

対象:

```text
particle
trail
collision/debug shapes
fullscreen quad
temporary HUD geometry
```

方法:

```text
per-frame transient vertex/index buffer
```

初期実装はsimple orphan/updateでもよい。

後でpersistent mappingへ最適化可能。

---

## 4.5 Display List完全撤去

削除対象:

```text
GenLists
NewList
EndList
CallList
DeleteLists

_displayLists
_displayListModels
Mesh::ListId GPU ownership
```

---

## 静的監査

Phase終了時:

```bash
rg -n "GenLists|NewList|EndList|CallList|DeleteLists" src/MphRead.Native
```

許容結果:

```text
0
```

診断コードに残す必要も基本ない。

---

## 完了条件

- [ ] Game sceneがVBO/IBOで描画
- [ ] display listsゼロ
- [ ] immediate mode model renderingゼロ
- [ ] screenshot parity確認
- [ ] model unload/reload正常
- [ ] hunter preview正常
- [ ] match終了後preview破損なし

---

## Commit例

```text
Replace OpenGL display lists with explicit mesh buffers
```

---

# 13. Phase 5 — Explicit Vertex Input + Shader Interface modernization

## 目的

GLSL 120 built-in input依存を排除し、OpenGL/Vulkanで同じvertex semanticsを使う。

---

## 5.1 Built-in attributes撤去

対象:

```text
gl_Vertex
gl_Normal
gl_Color
gl_MultiTexCoord0
```

明示入力へ変更。

GLSL 120を維持する場合でも:

```text
attribute
```

を使える。

Vulkan shaderではlocation明示。

---

## 5.2 Semantic contract

固定:

```text
location 0: Position
location 1: Normal
location 2: Color
location 3: TexCoord
location 4: MatrixIndex
```

実際のlocationは変更してよいが、共通定義を1箇所に置く。

---

## 5.3 Constant blocks

現行115前後のGL uniform呼び出しを分類する。

最低限:

```text
FrameConstants
SceneConstants
MaterialConstants
DrawConstants
Hud/PostConstants
```

---

## 5.4 Matrix stack

現行:

```text
mat4[32] mtx_stack
```

意味を維持。

最初はUniform/UBOでよい。

SSBO化は不要。

---

## 5.5 Shader source strategy

今回推奨:

### Stage A

OpenGL:

```text
GLSL
```

Vulkan:

```text
SPIR-V
```

ただしshader interface definitionは共有する。

### Stage B

Vulkan parity完成後、必要ならcanonical shader sourceを統一する。

将来D3D12/Metalを考えるなら候補:

```text
HLSL/Slang
→ SPIR-V
→ DXIL
→ MSL
→ GLSL
```

しかし今回のOpenGL/Vulkan完成をshader-toolchain全面刷新でブロックしない。

---

## 5.6 ShaderLocations

現行OpenGL location ID集合:

```text
ShaderLocations
```

をFrontend契約として残さない。

OpenGL backend内部でlocation cacheとして存在するのは可。

Frontendは:

```text
constant struct
binding slot
semantic binding
```

を使う。

---

## 完了条件

- [ ] built-in vertex attribute依存撤去
- [ ] OpenGLで描画一致
- [ ] backend-neutral constant structures
- [ ] Renderer frontendからuniform location concept撤去開始
- [ ] future Vulkan shader interface確定

---

## Commit例

```text
Use explicit vertex and shader interfaces for native rendering
```

---

# 14. Phase 6 — OpenGL Resource RHI化

## 目的

Texture/Buffer/Sampler/RenderTargetをRHI resourceへ移す。

---

## 対象

特に:

```text
_screenTexture
_celTexture
_depthTexture
_renderBuffer
_frameBuffer
_celFrameBuffer
_celFrameBufferColor

model textures
mask textures
HUD textures
```

---

## 6.1 TextureHandle

Sceneから:

```text
int textureId
```

を排除する方向へ進める。

---

## 6.2 Texture upload

現行:

```text
CPU palette decode
↓
GL::TexImage2D
```

を:

```text
CPU decoded pixels
↓
GraphicsDevice::CreateTexture
または UploadTexture
```

へする。

CPU decode自体は残してよい。

---

## 6.3 Sampler separation

texture objectへfilter/wrapを暗黙格納する設計から、

```text
Texture
Sampler
```

を分離できるRHIにする。

OpenGL backendでは必要ならtexture parameterとして内部実装してもよい。

---

## 6.4 Render target

共通名:

```text
SceneColor
SceneDepthStencil
CelColor
CelDepth
```

---

## 6.5 Depth format

RHI:

```text
D24S8
D32S8
```

等の意味型。

OpenGL backend mappingを実装。

Vulkan mappingは後Phase。

---

## 完了条件

- [ ] Scene resource fieldsがRHI handle化
- [ ] OpenGL resource creation backend内
- [ ] model texture upload backend内
- [ ] FBO構築 backend内
- [ ] resize正常
- [ ] cel depth attachment切替正常

---

## Commit例

```text
Move OpenGL render resources behind the RHI
```

---

# 15. Phase 7 — Pipeline State RHI化

## 目的

現行OpenGL state machineを、Vulkan/Metal/D3D12対応可能なPipeline Stateへ整理する。

---

## 7.1 現行state分類

調査対象:

```text
DepthFunc
DepthMask
BlendFunc
AlphaFunc
StencilFunc
StencilOp
StencilMask
ColorMask
PolygonOffset
CullFace
PolygonMode
```

---

## 7.2 Fixed pipeline variants

最初は実際に必要なvariantだけ作る。

例:

```text
SceneOpaque
SceneDecal
SceneTranslucentStencil
SceneTranslucentColor
HudModel
CelOutline
FullscreenComposite
Fade
DebugLines
```

---

## 7.3 Alpha Test

OpenGL fixed function:

```text
AlphaFunc Equal 1.0
AlphaFunc Less 1.0
```

をshader logicへ移す。

共通:

```text
AlphaMode::Disabled
AlphaMode::EqualOne
AlphaMode::LessThanOne
```

境界を変更しない。

---

## 7.4 Stencil reference

stencil referenceはdynamic stateとしてRHIに残してよい。

Stencil ops/compareはPipelineDescへ。

---

## 7.5 Color write mask

translucent prepassの:

```text
ColorMask(false,false,false,false)
```

をPipeline stateへ。

---

## 完了条件

- [ ] RenderItem描画時にGL state callをFrontendが直接しない
- [ ] pipeline variants整理
- [ ] alpha behavior一致
- [ ] stencil behavior一致
- [ ] decals一致
- [ ] translucent ordering一致

---

## Commit例

```text
Express native scene state through RHI graphics pipelines
```

---

# 16. Phase 8 — Explicit Render Pass Sequence化

## 目的

巨大な `Scene::OnRenderFrame()` のGPU操作順を、backend-neutralなpass sequenceへ整理する。

Render Graphはまだ作らない。

---

## 8.1 Pass候補

```text
SceneOpaquePass
SceneDecalPass
TranslucentStencilPrePass
DepthRebuildPass
TranslucentResolvePass
PreviewPass
HudModelPass
CelOutlinePass
PostProcessPass
Hud2DPass
FadePass
UiCompositePass
Present
```

---

## 8.2 重要

passクラス乱立が目的ではない。

重要なのは:

```text
attachment
pipeline
resource state
load/store
draw ordering
```

が明確になること。

---

## 8.3 BeginRendering

RHI:

```text
BeginRendering(RenderingInfo)
```

RenderingInfo:

```text
Color attachment
Depth/stencil attachment
LoadOp
StoreOp
Clear value
```

Vulkan Dynamic Renderingへ自然に対応できる形。

---

## 8.4 Depth clear

現行途中の:

```text
depth clear
```

をpass boundaryとして表現。

---

## 8.5 Stencil preservation

stencil load/store semanticsを明示。

OpenGLでは暗黙だった部分をRHI上で明文化する。

---

## 完了条件

- [ ] Scene GPU sequenceが明示Pass化
- [ ] OpenGL output parity
- [ ] pass order documentation更新
- [ ] Vulkan Dynamic Renderingへ変換可能

---

## Commit例

```text
Make native scene render passes explicit
```

---

# 17. Phase 9 — OpenGL CommandList完全化

## 目的

`Renderer.cpp` の直接GL呼び出しをOpenGL backendへ押し込む。

---

## 作業

Scene/Renderer frontendから:

```text
GL::Clear
GL::Viewport
GL::BindTexture
GL::UseProgram
GL::Uniform
GL::Enable
GL::Disable
GL::Blend*
GL::Stencil*
GL::Depth*
GL::BindFramebuffer
GL::Draw*
```

等を削る。

代わり:

```text
CommandList
GraphicsPipeline
BindingSet
Buffer
Texture
```

---

## Skiaは例外

このPhaseでは:

```text
NativeRuntime/Skia/SkiaGpu.cpp
```

のOpenGL interopは残してよい。

ただし例外として文書化。

---

## 静的監査

```bash
rg -n "\bGL::" src/MphRead.Native/Renderer.cpp
```

目標:

```text
0
```

または本当に診断だけの明示例外。

---

## 完了条件

- [ ] Renderer.cpp direct GL ≈ 0
- [ ] Renderer.hpp OpenGL include不要
- [ ] Scene raw GL IDsなし
- [ ] OpenGL描画完全動作
- [ ] Skia GLのみbackend-specific exception

---

## Commit例

```text
Route native scene rendering entirely through OpenGL RHI
```

---

# 18. Phase 10 — FrameContext・GPU Lifetime・Deferred Destruction

## 目的

Vulkan導入前にGPU lifetime contractを確定する。

---

## 10.1 FrameContext

2 frames in flightを初期値とする。

共通:

```text
FrameContext[0]
FrameContext[1]
```

各frame:

```text
command resources
transient upload
binding allocator
retirement/completion value
deferred delete list
```

---

## 10.2 OpenGL implementation

OpenGLにはVulkan同等のframe fenceが必須ではないが、上位契約を合わせる。

必要なら:

```text
GLsync
```

を使用。

少なくともdeferred-destruction API契約を成立させる。

---

## 10.3 Resource destroy

API:

```text
Destroy requested
↓
retire queue
↓
GPU completion
↓
native destroy
```

---

## 10.4 UnloadGl replacement

現行 `UnloadGl()` の責務を:

```text
Scene GPU resource release
```

へ改名・再整理する。

GL固有名をFrontendから減らす。

---

## 完了条件

- [ ] Scene unload安全
- [ ] match end安全
- [ ] preview shared resource破損なし
- [ ] repeated load/unload leakなし
- [ ] lifetime contract Vulkan対応

---

## Commit例

```text
Add frame retirement and deferred GPU resource destruction
```

---

# 19. Phase 11 — OpenGL RHI完成Gate

## 目的

Vulkan実装前の必須Gate。

ここでOpenGL版を一度完成扱いにする。

---

## 必須静的監査

```bash
rg -n "\bGL::" src/MphRead.Native \
  -g "*.cpp" -g "*.hpp"
```

全結果分類:

```text
A OpenGL RHI backend
B Skia GL interop
C diagnostics
D legacy invalid
```

D = 0。

---

## 必須runtime

```text
Launcher
offline game
online/local game if available
map change
hunter change
HUD
pause
Map Vote
end screen
cel
fog
fullscreen
resize
minimize restore
scene unload/reload
```

---

## 必須parity

Phase 0 goldenと比較。

重大差分ゼロ。

---

## Vulkanへ進む条件

以下全てYes:

- [ ] immediate modeなし
- [ ] display listなし
- [ ] Renderer.cpp direct GLなし
- [ ] resource RHI化
- [ ] pipeline RHI化
- [ ] pass明示化
- [ ] OpenGL stable
- [ ] CI green

一つでもNoならVulkan Phaseへ進まない。

---

# 20. Phase 12 — Vulkan Instance / Device / Debug bring-up

## 目的

まだgame sceneを描画しない。

Vulkan backendの基盤だけ作る。

---

## Vulkan baseline

推奨:

```text
Vulkan 1.3
```

理由:

```text
Dynamic Rendering core
Synchronization2 core
modern explicit rendering path
```

Timeline semaphoreはVulkan 1.2でcore。

互換性を広げる必要がある場合:

```text
Vulkan 1.2
+
VK_KHR_dynamic_rendering
+
VK_KHR_synchronization2
```

も許容できる設計にする。

---

## 12.1 Instance

実装:

```text
VkInstance
required instance extensions
debug utils
validation layer optional
```

---

## 12.2 Validation

Debug:

```text
VK_LAYER_KHRONOS_validation
```

利用可能なら有効。

Releaseで必須にしない。

---

## 12.3 Physical device selection

評価:

```text
graphics support
present support
required Vulkan version
required features
swapchain extension
format support
memory
```

---

## 12.4 Device

初期:

```text
one graphics queue
present queue
```

可能なら同一family。

---

## 12.5 Capabilities

RHI `Capabilities`へ変換。

Vulkan feature structをFrontendへ漏らさない。

---

## 12.6 Debug naming

`VK_EXT_debug_utils` で:

```text
Buffer
Image
Pipeline
Descriptor
CommandBuffer
```

へdebug name。

---

## 完了条件

- [ ] Vulkan instance creation
- [ ] physical GPU列挙
- [ ] device creation
- [ ] queue取得
- [ ] validation重大エラーなし
- [ ] clean shutdown
- [ ] OpenGL build/runtime unaffected

---

## Commit例

```text
Add Vulkan instance and device backend foundation
```

---

# 21. Phase 13 — Vulkan Surface / Swapchain / Present

## 目的

clear colorだけをVulkanでpresentできるところまで進める。

---

## Desktop

GLFW:

```text
GLFW_NO_API
glfwCreateWindowSurface
```

---

## Swapchain

選択:

```text
surface format
present mode
extent
image count
```

---

## VSync

RHI:

```text
VSync on/off
```

Vulkan:

```text
FIFO
MAILBOX
IMMEDIATE
```

へbackend mapping。

---

## Swapchain image ownership

swapchain imageはRHIがdestroyしない。

wrapする。

---

## Resize

処理:

```text
VK_ERROR_OUT_OF_DATE_KHR
VK_SUBOPTIMAL_KHR
framebuffer resize
0x0/minimized
```

---

## Frame sync

初期:

```text
2 frames in flight
imageAvailable binary semaphore
renderFinished binary semaphore
frame fence
```

Timelineを使ってframe retirementを補助してよい。

presentation binary semaphoreは必要に応じて維持。

---

## Clear-only test

```text
Acquire
Transition
BeginRendering
Clear
EndRendering
Transition Present
Submit
Present
```

---

## 完了条件

- [ ] Vulkan window表示
- [ ] clear color present
- [ ] resize
- [ ] fullscreen
- [ ] minimize/restore
- [ ] validation clean
- [ ] shutdown clean

---

## Commit例

```text
Add Vulkan swapchain and frame presentation
```

---

# 22. Phase 14 — Vulkan Memory / Buffer / Texture / Upload

## 目的

RHI resourceをVulkan objectへ実装する。

---

## 14.1 Memory allocator

選択肢:

```text
VMA
または
project-owned allocator
```

VMA採用は推奨可能。

ただしRHI public APIへVMA型を漏らさない。

---

## 14.2 Buffer

対応:

```text
Vertex
Index
Constant
TransferSrc
TransferDst
Readback
```

---

## 14.3 Texture

対応:

```text
sampled
color attachment
depth/stencil
transfer
```

---

## 14.4 ImageView

textureとviewを分離。

---

## 14.5 Staging

upload:

```text
CPU
↓ staging buffer
↓ vkCmdCopyBuffer / vkCmdCopyBufferToImage
↓ transition
↓ GPU resource
```

---

## 14.6 ResourceState mapping

集中管理helperを作る。

禁止:

各call siteで独自に:

```text
oldLayout
newLayout
srcAccess
dstAccess
```

を手書き乱立。

---

## 14.7 Synchronization2

`vkCmdPipelineBarrier2` 系を基本とする。

transitionはactive rendering scope外で行う。

---

## 完了条件

- [ ] Buffer RHI実装
- [ ] Texture RHI実装
- [ ] staging upload
- [ ] readback buffer
- [ ] resource transition helper
- [ ] validation clean
- [ ] leakなし

---

## Commit例

```text
Implement Vulkan RHI buffers textures and uploads
```

---

# 23. Phase 15 — Vulkan Descriptor / Binding model

## 目的

共通BindingLayout/BindingSetをVulkan descriptorへ変換する。

---

## 15.1 Layout

例:

```text
Frame set
Material set
Draw set
```

---

## 15.2 Descriptor pool

最初は:

```text
per-frame descriptor pool
```

が安全。

Frame fence完了後reset。

---

## 15.3 更新

GPU使用中descriptorを更新しない。

frame safe pointで更新。

---

## 15.4 Texture/Sampler

combined image samplerまたは分離descriptorを採用。

RHI semanticsを優先。

---

## 15.5 Constant buffer alignment

`minUniformBufferOffsetAlignment` を考慮。

---

## 完了条件

- [ ] BindingLayout Vulkan mapping
- [ ] BindingSet Vulkan mapping
- [ ] per-frame safe allocator
- [ ] descriptor lifetime errorなし
- [ ] frontend Vulkan descriptor awarenessなし

---

## Commit例

```text
Implement Vulkan RHI resource bindings
```

---

# 24. Phase 16 — Vulkan Shader / Pipeline

## 目的

OpenGLで固定したshader semanticsをVulkanへ実装する。

---

## 16.1 SPIR-V build

CMakeにshader compile stepを追加。

候補:

```text
glslc
glslangValidator
DXC -spirv
Slang
```

一つを明示的に選ぶ。

CIも同じtoolchainを使用。

---

## 16.2 Reflection

初期実装ではreflection必須ではない。

binding contractをC++/shader shared constantsで固定してもよい。

将来reflection導入可能。

---

## 16.3 Pipeline

RHI `GraphicsPipelineDesc` から:

```text
VkPipelineLayout
VkPipeline
```

を生成。

Dynamic Rendering:

```text
VkPipelineRenderingCreateInfo
```

利用。

---

## 16.4 Cache

`GraphicsPipelineDesc` hashでcache。

drawごとのpipeline作成禁止。

---

## 16.5 Shader parity

必須:

```text
lighting
fog
texgen
matrix stack
palette override
flat color
alpha test
material alpha
material mode
cel
shift
whiteout
fade
```

---

## 完了条件

- [ ] SPIR-V reproducible build
- [ ] pipeline creation
- [ ] descriptor/pipeline layout一致
- [ ] validation clean
- [ ] shader semantics documented

---

## Commit例

```text
Add Vulkan shaders and graphics pipeline implementation
```

---

# 25. Phase 17 — Vulkan Main Scene描画

## 目的

UIなしでgame sceneをVulkan描画する。

---

## 実装順

### 17.1 Static mesh

まず:

```text
room
hunter/model
```

---

### 17.2 Textures

model textures。

---

### 17.3 Lighting

OpenGL parity。

---

### 17.4 Opaque

まずopaqueだけ。

---

### 17.5 Decal

polygon offset相当。

---

### 17.6 Translucent + Stencil

現行アルゴリズムをそのまま移植。

簡略化禁止。

---

### 17.7 Dynamic geometry

```text
particles
trails
debug geometry
```

---

## 完了条件

- [ ] room表示
- [ ] model表示
- [ ] textures正常
- [ ] no flipped UV
- [ ] correct winding
- [ ] depth正常
- [ ] decals正常
- [ ] translucent正常
- [ ] stencil正常
- [ ] particles/trails正常

---

## Commit分割推奨

1 commitに全部入れず:

```text
Render opaque scene through Vulkan
Add Vulkan decals and translucent passes
Add Vulkan dynamic scene geometry
```

---

# 26. Phase 18 — Vulkan Cel / RTT / Post / HUD

## 目的

OpenGL固有だった後段処理をVulkan化する。

---

## 18.1 SceneColor

offscreen color attachment。

---

## 18.2 Depth

cel outline用sampleable depth。

---

## 18.3 Cel outline

transition:

```text
DepthWrite
↓
DepthRead / ShaderResource
```

---

## 18.4 RTT composite

fullscreen triangle/quad。

---

## 18.5 Shift / whiteout

現行table/factor semanticsを維持。

---

## 18.6 HUD

```text
HUD models
HUD layers
HUD objects
mask texture
```

---

## 18.7 Fade

fullscreen pass。

---

## 完了条件

- [ ] cel shading
- [ ] outline
- [ ] HUD
- [ ] mask
- [ ] whiteout
- [ ] disruption
- [ ] fade
- [ ] scoreboard
- [ ] pause game background
- [ ] OpenGL comparison pass

---

# 27. Phase 19 — Skia Vulkan GPU Integration

## 目的

VulkanモードでもLauncher / Pause / Map Vote / End ScreenをGPU描画する。

hidden OpenGL contextは禁止。

CPU full-frame fallback禁止。

---

## 現行

```text
Skia Ganesh GL
```

今回:

```text
OpenGL backend
    → Skia Ganesh GL

Vulkan backend
    → Skia Ganesh Vulkan
```

---

## 19.1 Skia build

現在のCIは概ね:

```text
skia[gl,freetype]
```

前提。

Vulkan backendを明示的に有効にする必要がある。

Skia Vulkan capabilityをconfigure時に検出する。

「headerがあるだけ」で成功扱いしない。

---

## 19.2 Shared device

Skia自身に別VkDeviceを作らせない。

原則:

```text
RHI Vulkan VkInstance
RHI Vulkan VkPhysicalDevice
RHI Vulkan VkDevice
RHI Vulkan graphics queue
```

をnarrow interop adapter経由でSkiaへ渡す。

---

## 19.3 Interop boundary

例:

```text
NativeRuntime/Skia/VulkanInterop.*
```

のみがraw Vk handleを見てよい。

Common Skia canvas APIへVk型を漏らさない。

---

## 19.4 UI target

推奨:

```text
RHI-owned UiColor Vulkan image
↓
Skia wraps image
↓
UI draw
↓
Skia flush/submit
↓
RHI waits/orders correctly
↓
UiColor transition ShaderResource
↓
UiComposite pass
```

---

## 19.5 Synchronization

最重要。

Skia公式仕様上、client-owned VkImageをSkiaへ渡す場合、client側が必要な同期/barrierを担当する。

必要な設計:

```text
RHI state before Skia
↓
Skia expected layout
↓
Skia draw/submit
↓
Skia final image layout取得
↓
RHI state trackerへ反映
↓
RHI composite
```

---

## 19.6 Queue ordering

初期は同一graphics queueを優先。

複数queue最適化は禁止。

---

## 19.7 Frame order

維持:

```text
Scene
HUD/Post
Shell TickUi
Ui image
UiOverlay / LauncherHunter
Present
```

UI offscreen生成のsubmission時刻は内部的に変わっても、visual semanticsを変えない。

---

## 19.8 Launcher-only path

Sceneがない場合も:

```text
UI
↓
Vulkan present
```

が成立すること。

---

## 完了条件

- [ ] Launcher Vulkan
- [ ] Settings Vulkan
- [ ] Pause Vulkan
- [ ] Map Vote Vulkan
- [ ] End Screen Vulkan
- [ ] hidden GL contextなし
- [ ] CPU full-frame fallbackなし
- [ ] Skia/Vulkan sync validation clean
- [ ] texture corruptionなし

---

## Commit例

```text
Render native Skia UI through the Vulkan backend
```

---

# 28. Phase 20 — Vulkan Readback / Screenshot / Recording

## 目的

`GL::ReadPixels`依存をbackend-neutral化する。

---

## 対象

```text
ReadWindowBuffer
ReadSceneTarget
screenshot
recording
thumbnail/capture
cel calibration where applicable
```

---

## Vulkan path

```text
Image
↓ transition TransferSrc
↓ CopyImageToBuffer
↓ fence/completion
↓ mapped readback
↓ CPU image
```

---

## 注意

通常frameを毎回GPU idleしない。

必要時だけreadback。

---

## Row pitch

Vulkan buffer layoutのrow alignmentを正しく処理。

---

## Orientation

OpenGL/Vulkanでvertical orientation差を統一。

Frontendには同じtop/bottom conventionを返す。

---

## 完了条件

- [ ] screenshot一致
- [ ] recording一致
- [ ] scene target capture一致
- [ ] unnecessary WaitIdleなし
- [ ] no leak

---

# 29. Phase 21 — Backend Selection

## 目的

ユーザーがOpenGL/Vulkanを選択できるようにする。

---

## 選択値

```text
OpenGL
Vulkan
Auto
```

---

## 明示選択

Vulkan指定:

```text
Vulkan init fail
↓
error
```

silent OpenGL fallback禁止。

---

## Auto

初期推奨:

Desktop Windows/Linux:

```text
Vulkan requirements satisfied
    → Vulkan
otherwise
    → OpenGL
```

ただし既存ユーザー体験を優先して最初はOpenGL defaultでもよい。

Rollout policyは別。

---

## Logging

起動時:

```text
Requested backend
Selected backend
GPU
API version
driver
swapchain format
depth format
frames in flight
validation status
```

---

## Runtime hot switching

今回の必須条件にはしない。

まずstartup selectionを完成させる。

ただしRHI ownershipは将来hot switchingできる構造にする。

---

## 完了条件

- [ ] explicit OpenGL
- [ ] explicit Vulkan
- [ ] Auto
- [ ] error visible
- [ ] no silent fallback

---

# 30. Phase 22 — Android Vulkan Platform Integration

## 目的

既存Android GLESを維持したまま、共通Vulkan backendをAndroidへ接続する。

---

## 重要

Vulkan実装をAndroid用に複製しない。

共有:

```text
VkDevice implementation
VkResources
VkPipeline
VkBindings
VkSynchronization
```

Android専用:

```text
surface
window lifecycle
app lifecycle
```

---

## 22.1 ANativeWindow

platform adapterで保持。

---

## 22.2 Surface

```text
VkAndroidSurfaceCreateInfoKHR
vkCreateAndroidSurfaceKHR
```

---

## 22.3 Lifecycle

独立状態:

```text
App alive
Device alive
Surface alive
Swapchain alive
Game state alive
```

---

## 22.4 Surface loss

surface破棄時:

```text
swapchain-dependent objects release
surface release
```

Device/game stateを不用意に破棄しない。

---

## 22.5 Resume

surface再生成後:

```text
surface
swapchain
backbuffer-dependent resources
```

再構築。

---

## 22.6 Orientation

```text
surface pixel extent
UI coordinates
touch mapping
projection
```

を一致させる。

---

## 22.7 GLES coexistence

Android:

```text
OpenGLES
Vulkan
Auto
```

明示Vulkan失敗でsilent GLES fallback禁止。

---

## 22.8 Android Skia/UI

Vulkanモードでhidden GLES contextを作らない。

GPU Vulkan pathを使う。

---

## 完了条件

- [ ] Android Vulkan scene
- [ ] Android Vulkan HUD/UI
- [ ] pause/resume
- [ ] surface destroy/recreate
- [ ] orientation
- [ ] touch mapping
- [ ] Vulkan validation clean
- [ ] GLES build/run維持

---

# 31. Phase 23 — CI分割

## 目的

OpenGL/Vulkan双方を継続的にbuild gateへする。

---

## Windows

最低限:

```text
OpenGL build
Vulkan build
```

可能なら同一binaryに両backendを含める。

それでもcompile definitions/optionsの検証を分ける。

---

## Linux

```text
OpenGL
Vulkan
```

必要package:

```text
Vulkan headers
loader
shader compiler
validation/dev package as appropriate
Skia Vulkan
```

---

## macOS

今回:

```text
OpenGL build維持
```

Vulkan via MoltenVKは任意。

将来Metal Phaseでnative backend追加。

macOS Vulkanを今回の必須Gateにしなくてもよい。

---

## Android

```text
GLES existing build
Vulkan build
```

ABIs:

repository contractに従う。

---

## Shader build

SPIR-V compilationもCIで実行。

precompiled stale shaderをcommitしてcompile stepを避けない。

---

## 完了条件

- [ ] Windows CI green
- [ ] Linux CI green
- [ ] macOS OpenGL CI green
- [ ] Android GLES CI green
- [ ] Android Vulkan CI green
- [ ] shader build reproducible

---

# 32. Phase 24 — Cross-backend Golden Parity

## 目的

「Vulkanで動く」ではなく「OpenGLと同じ結果」を確認する。

---

## 比較項目

### Geometry

- winding
- quad split
- strips
- matrix index
- normals

### Texture

- UV
- filter
- wrap
- palette
- alpha

### Depth

- near/far
- depth compare
- depth write

### Stencil

- polygon ID
- translucent ordering

### Blending

- decals
- translucent
- HUD

### Shader

- lighting
- fog
- texgen
- cel
- fade
- whiteout

### Coordinate systems

- framebuffer Y orientation
- clip-space depth
- front-face winding
- texture origin

---

## Vulkan/OpenGL座標差

特に確認:

```text
clip space
Y inversion
depth range
viewport convention
framebuffer origin
```

場当たり的に各shaderへflipを追加しない。

共通coordinate conventionを1箇所で定義する。

---

## 許容差

pixel-perfectが不可能なdriver差はtoleranceを定義。

ただし以下は不許可:

```text
missing pixels
wrong textures
wrong UV
different geometry
broken stencil
broken alpha
UI offset
black frame
flipped output
```

---

## 完了条件

全golden testで重大差分なし。

---

# 33. Phase 25 — Stability / Lifetime Stress

## 目的

過去のPC freezeやresource lifetime問題を再発させない。

---

## Stress

最低限:

```text
match start/end repeat
map change repeat
launcher/game repeat
resize repeat
fullscreen repeat
minimize/restore repeat
Map Vote repeat
backend startup alternate
Android pause/resume repeat
Android surface recreate repeat
```

---

## Monitor

```text
CPU memory
GPU memory
handle count
buffer count
texture count
pipeline count
descriptor allocation
command pools
fences
semaphores
```

---

## Vulkan validation

ゼロにする:

```text
use-after-free
destroy-in-use
layout mismatch
invalid descriptor
bad access masks
bad stage masks
command buffer misuse
swapchain lifetime error
```

---

## 完了条件

長時間増加傾向なし。

---

# 34. Phase 26 — OpenGL/Vulkan Architecture Freeze

## 目的

次のMetal/D3D12追加前にRHI contractを安定化する。

---

## 最終監査

### Common RHI

禁止type:

```bash
rg -n "Vk[A-Z]|vk[A-Z]|GLuint|OpenGL::GL|ID3D12|MTL[A-Z]" \
  src/MphRead.Native/NativeRuntime/Rhi \
  -g "*.hpp" -g "*.cpp"
```

backend directory以外でraw API typeがないこと。

---

### Renderer frontend

```bash
rg -n "\bGL::|Vk[A-Z]|vk[A-Z]" \
  src/MphRead.Native/Renderer.cpp \
  src/MphRead.Native/Renderer.hpp
```

目標:

```text
0
```

---

### Backend isolation

OpenGL native calls:

```text
Rhi/OpenGL
Skia/OpenGL interop
```

以外にない。

Vulkan native calls:

```text
Rhi/Vulkan
Skia/Vulkan interop
platform surface adapter
```

以外にない。

---

## Metal追加シミュレーションレビュー

コードを書かずにレビューする。

質問:

```text
MetalDeviceを追加するときGraphicsDevice interface変更が必要か？
MetalSwapchainを追加するときRenderer.cpp変更が必要か？
MetalPipelineを追加するときScene変更が必要か？
MetalTextureを追加するときMaterial変更が必要か？
```

理想:

```text
No
```

必要ならRHI contractの問題として修正。

---

## D3D12追加シミュレーションレビュー

同じく:

```text
D3D12Device
D3D12CommandList
D3D12Pipeline
D3D12Bindings
D3D12Swapchain
```

を追加するだけでFrontendが成立するか確認。

---

# 35. Phase間の依存関係

```text
Phase 0 Baseline
   ↓
Phase 1 RHI Core
   ↓
Phase 2 Window/Presentation
   ↓
Phase 3 Geometry Decoder
   ↓
Phase 4 OpenGL VBO/IBO
   ↓
Phase 5 Shader Interface
   ↓
Phase 6 Resource RHI
   ↓
Phase 7 Pipeline State
   ↓
Phase 8 Render Pass Sequence
   ↓
Phase 9 OpenGL CommandList
   ↓
Phase 10 Frame Lifetime
   ↓
Phase 11 OpenGL Gate
   ↓
Phase 12 Vulkan Device
   ↓
Phase 13 Vulkan Swapchain
   ↓
Phase 14 Vulkan Resources
   ↓
Phase 15 Vulkan Bindings
   ↓
Phase 16 Vulkan Shader/Pipeline
   ↓
Phase 17 Vulkan Scene
   ↓
Phase 18 Vulkan Post/HUD
   ↓
Phase 19 Skia Vulkan
   ↓
Phase 20 Readback
   ↓
Phase 21 Backend Selection
   ↓
Phase 22 Android Vulkan
   ↓
Phase 23 CI
   ↓
Phase 24 Golden Parity
   ↓
Phase 25 Stress
   ↓
Phase 26 Architecture Freeze
```

---

# 36. Worker AIにPhaseを渡すときのテンプレート

毎回以下の形式で依頼する。

```text
Repository:
https://github.com/Zection6V/Fruity-Prime

Branch:
develop3_rendering

Task:
OpenGL/Vulkan RHI移行計画の Phase N を完遂してください。

Plan:
docs/app_design/Fruity-Prime-CPP-OpenGL-Vulkan-RHI-Phase-Plan-2026-09-28.md

Requirements:
- Phase Nの範囲だけ実装
- develop3_renderingの最新HEADから開始
- 他作業者のcommitを保持
- C#版は変更禁止
- force-push禁止
- dummy/stubで完了扱い禁止
- build成功だけで完了扱い禁止
- PhaseのDefinition of Doneを全項目確認
- 未実施runtime testは未実施と明記
- 最後にcommit SHA / files / build / tests / CIを報告
```

---

# 37. Phase完了報告フォーマット

Worker AIは最後に必ず以下を返す。

```markdown
## Phase N Result

### Status
PASS / PARTIAL / BLOCKED

### Base
- branch:
- base SHA:

### Result
- final SHA:
- commit:

### Changed files
- ...

### Implementation
- ...

### Static audit
- command:
- result:

### Build
- Windows:
- Linux:
- macOS:
- Android:

### Runtime
- tested:
- not tested:

### CI
- workflow:
- run ID:
- exact SHA:
- result:

### Remaining issues
- ...

### Phase N+1 readiness
READY / NOT READY
```

---

# 38. Phase失敗時のルール

Phaseが完成しなかった場合:

```text
PARTIAL
```

として止める。

禁止:

```text
未完成のまま次Phaseへ進む
```

特に以下が残ったら次へ進まない。

```text
render regression
texture corruption
validation error
lifetime crash
CI failure
known Vulkan misuse
```

---

# 39. OpenGLからVulkanへ移す際の危険箇所リスト

## Immediate mode state inheritance

現行OpenGLは:

```text
Color
Normal
TexCoord
```

が次vertexまで持続する。

decoderで再現する。

---

## Quad semantics

GL_QUADSを適当にtriangle化しない。

---

## Matrix index

現在texcoord Zへ隠している意味を落とさない。

---

## Alpha test

fixed functionを削除しただけにしない。

shader discardへ移す。

---

## Stencil

translucent renderingの中心。

「見た目大体同じ」に簡略化禁止。

---

## Depth clear timing

frame途中のdepth clearを忘れない。

---

## Color mask

translucent stencil prepassで重要。

---

## Polygon offset

decal z-fighting対策。

---

## Texture origin

OpenGL/Vulkan差で上下反転しやすい。

---

## Clip depth

OpenGLとVulkanのNDC差を共通projection policyで吸収する。

---

## Skia state

OpenGLではglobal state restore問題。

Vulkanではimage layout/synchronization問題へ変わる。

---

## UI texture ownership

Skiaが作る/使うimageとRHIがcompositeするimageのownershipを曖昧にしない。

---

# 40. Performance方針

最初のVulkan版でOpenGLより速いことは完成条件ではない。

ただし明らかなanti-patternは禁止。

禁止:

```text
vkDeviceWaitIdle every frame
vkQueueWaitIdle every frame
pipeline create every draw
descriptor pool create every draw
buffer create every draw
image create every frame unnecessarily
map/unmap tiny buffer for every vertex
one submission per draw
```

初期推奨:

```text
1 graphics submission per main frame
必要最小限のSkia submission
2 frames in flight
per-frame transient upload
pipeline cache
descriptor pool per frame
```

---

# 41. Vulkan API方針

今回のVulkan実装ではmodern pathを優先。

推奨:

```text
Dynamic Rendering
Synchronization2
Timeline Semaphore where useful
Debug Utils
Pipeline cache
```

Vulkan 1.3ではDynamic RenderingとSynchronization2がcore。

Timeline Semaphoreは1.2でcore。

ただしAPI機能をRHI frontendへ漏らさない。

---

# 42. Skia Vulkan方針

SkiaはOpenGL/Vulkan両GPU backendを持つ。

重要:

```text
SkiaはVulkan deviceを自動作成しない
```

client側がdevice/queueを提供する構成を使う。

RHI Vulkan deviceとSkia Vulkan contextは同一deviceを共有する。

Skiaが使用するRHI-owned imageのlayout/synchronizationはclient側責任として扱う。

この境界は必ず専用interop layerに閉じ込める。

---

# 43. Build system最終イメージ

CMake概念:

```cmake
option(FRUITY_RENDERER_OPENGL "Build OpenGL backend" ON)
option(FRUITY_RENDERER_VULKAN "Build Vulkan backend" ON)
```

Targets概念:

```text
fruity_rhi
fruity_rhi_opengl
fruity_rhi_vulkan
fruity_mphread_native
```

Dependencies:

```text
fruity_rhi
    no OpenGL
    no Vulkan

fruity_rhi_opengl
    OpenGL
    GLFW

fruity_rhi_vulkan
    Vulkan
    GLFW desktop
    Vulkan Android loader on Android
```

Skia backend dependenciesも選択backendごとに明示。

---

# 44. Definition of Done — 今回全体

## Architecture

- [ ] Renderer frontend API-neutral
- [ ] RHI OpenGL-shapedではない
- [ ] OpenGL backend独立
- [ ] Vulkan backend独立
- [ ] Future Metal/D3D12追加でFrontend変更不要
- [ ] raw handle leakなし

## OpenGL

- [ ] immediate mode撤去
- [ ] display list撤去
- [ ] VBO/IBO
- [ ] explicit shader input
- [ ] RHI resources
- [ ] RHI pipeline
- [ ] RHI command list
- [ ] visual parity

## Vulkan Desktop

- [ ] instance/device
- [ ] swapchain
- [ ] buffer/texture
- [ ] descriptor/bindings
- [ ] pipeline
- [ ] scene
- [ ] stencil/translucency
- [ ] cel
- [ ] HUD
- [ ] Skia UI
- [ ] readback
- [ ] resize/fullscreen
- [ ] validation clean

## Android Vulkan

- [ ] shared Vulkan backend
- [ ] Android surface adapter
- [ ] scene
- [ ] HUD/UI
- [ ] lifecycle
- [ ] orientation
- [ ] GLES維持

## Stability

- [ ] resource leakなし
- [ ] repeated scene load安全
- [ ] match end安全
- [ ] Map Vote安全
- [ ] long-run freeze regressionなし

## CI

- [ ] Windows
- [ ] Linux
- [ ] macOS OpenGL
- [ ] Android GLES
- [ ] Android Vulkan

---

# 45. 今回の最重要原則

ワーカーAIは常に以下を判断基準にする。

```text
「この変更はVulkanを追加するためのOpenGL特例か？」
```

Yesなら再設計を検討する。

正しい構造:

```text
Game
↓
Renderer Frontend
↓
RHI semantics
↓
backend implementation
```

誤った構造:

```text
Game
↓
OpenGL behavior
↓
if Vulkan
↓
OpenGL emulation
```

---

# 46. 最終的な完成イメージ

今回:

```text
                         Fruity-Prime C++

Game / Scene / HUD / Launcher
              │
              ▼
        Renderer Frontend
              │
              ▼
        Explicit Passes
              │
              ▼
             RHI
       ┌──────┴──────┐
       ▼             ▼
    OpenGL         Vulkan
       │             │
       └──────┬──────┘
              ▼
             GPU
```

将来:

```text
             RHI
   ┌─────────┼─────────┬─────────┐
   ▼         ▼         ▼         ▼
OpenGL    Vulkan     Metal      D3D12
```

今回の実装が正しければ、

```text
Metal追加
D3D12追加
```

はRendererの再設計ではなく、**backend追加作業**になる。

これを今回のアーキテクチャ成功条件とする。

---

# 47. 参考仕様・公式資料

Vulkan Documentation Project:

```text
https://docs.vulkan.org/
```

Vulkan Dynamic Rendering:

```text
https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_dynamic_rendering.html
```

Vulkan Synchronization2:

```text
https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html
```

Vulkan Synchronization:

```text
https://docs.vulkan.org/guide/latest/synchronization.html
```

Vulkan core revisions:

```text
https://docs.vulkan.org/spec/latest/appendices/versions.html
```

Skia Vulkan backend:

```text
https://skia.org/docs/user/special/vulkan/
```

Skia GPU canvas/context guidance:

```text
https://skia.org/docs/user/api/skcanvas_creation/
```

---

# 48. この計画と既存マルチバックエンド指示書の関係

既存:

```text
docs/app_design/Fruity-Prime-CPP-MultiBackend-RHI-Work-Instructions-2026-09-28.md
```

は最終的なmulti-backend思想を示す。

この文書はそのうち:

```text
OpenGL
Vulkan
Android Vulkan
```

を先行して完成させるための詳細なexecution planである。

Metal/D3D12を今回実装しないことは、multi-backend設計思想を放棄する意味ではない。

むしろ、

```text
OpenGL + Vulkan
```

の2つでRHI抽象が本当に成立することを先に証明してから、

```text
Metal
D3D12
```

を追加する。

この順序を厳守する。
