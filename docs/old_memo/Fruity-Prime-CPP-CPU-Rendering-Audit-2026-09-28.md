# Fruity-Prime C++版 CPUレンダリング残存監査

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop2`
- 監査基準コミット: `df5e7b29bd642ab8a7b14c547c090d3c0a9a0533`
- 監査日: 2026-09-28
- 対象: C++版のみ
- C#版との比較: 実施していない

> [!IMPORTANT]
> **撤去作業完了後の状態（2026-09-28）**
>
> この文書の本文はCPUレンダリング撤去前の監査結果を保存したもの。
> `develop2` のライブ経路については、この監査を受けて以下を実施した。
>
> - `DeckTile` の full tile / chrome / ground の全 `RenderTargetBitmap` bakeを撤去し、Ganesh/OpenGLの現在の `DrawingContext` へ直接描画。
> - `MapShot` の `DecodeToWidth()` CPU resamplingを撤去。画像は一度decodeし、表示サイズへのsamplingはGPU側で実施。
> - desktop launcherの30Hz `NoiseField`生成・noise texture uploadを撤去し、`BackdropFragmentShader` 内のprocedural noiseへ移行。
> - `MovingBackdrop` のCPU RGBA bitmap生成を撤去し、GPU-backed vector gradient animationへ変更。
> - `UiCapture` / `UiBench` / `GamepadUiChecks` のsoftware TopLevelは診断・検証専用として残存し、通常ライブ経路からは到達しない。
> - `DeckTile` / `DeckButton` / `ServerRow` のpointer追従回転（tilt/lean）を撤去。回転変換下では Ganesh が blur 影と角丸 clip を解析的に描けず CPU mask rasterize + upload に落ちるため、live UI の変換は scale + translate のみとした。`GpuSurface::DrawBoxShadow` は角丸矩形を `clipRRect`/`drawRRect` で描き、解析的 blur に乗せる。
> - JPEG/PNG decodeや `glReadPixels()` はCPU rasterizerではないため対象外。
>
> **結論: 通常のC++ライブ経路（試合中Map Voteを含む）から、監査で確認されたCPU rasterization / procedural pixel generationは撤去済み。**

## 結論

**C++版にはCPUレンダリング経路が残っている。**

ただし、ゲーム本体の3Dシーン描画がCPU Software Rendererへ戻っているわけではない。

現在の構成は大きく分けると以下の通り。

1. **ゲーム本体3Dレンダリング**
   - OpenGL/GLESによるGPU描画。
   - CPU Software Rendererとしてゲーム画面全体を描く経路は確認できなかった。

2. **デスクトップの通常UI TopLevel**
   - Skia Ganesh + OpenGLのGPU描画がデフォルト。
   - `TopLevel::_gpuRendering = true`。
   - 通常の `UiSurface` は `GpuRendering(false)` を呼んでいない。
   - 完成したGanesh側GPUテクスチャを `UiOverlay::UseTexture()` へ渡して合成している。
   - したがって、通常UI全体を毎フレームCPUで描いてから `glTexSubImage2D` する旧方式にはなっていない。

3. **ただし、通常UI内部の一部はCPUオフスクリーン描画を使用している**
   - 特に `DeckTile`。
   - マップカードや投票画面にも使われるため、**試合中でもCPU Rasterizerが実行され得る**。
   - ここが今回の監査で最も重要な残存経路。

4. **診断、キャプチャ、ベンチマーク用には明示的なSoftware/CPU TopLevelが残っている**
   - `UiCapture`
   - `UiBench`
   - `GamepadUiChecks`

5. **CPUで画像を生成してGPUへアップロードする処理も複数残っている**
   - Launcher noise
   - マップ画像のCPU縮小
   - JPEG/PNG decode
   - 一部のプレビュー画像生成
   - これらは「ゲーム3DのCPUレンダリング」とは別だが、CPU負荷とCPU→GPU転送は発生する。

---

# 1. CPU Rasterizer本体は残存している

## 対象

`src/MphRead.Native/NativeRuntime/Skia/Skia.cpp`  
`src/MphRead.Native/NativeRuntime/Skia/Skia.hpp`  
`src/MphRead.Native/NativeRuntime/Skia/SkiaText.cpp`

`Skia.hpp` には現在もCPU bitmap-backed canvasを残す設計が明記されている。

主なCPU実装:

- `Skia::Bitmap`
- `Bitmap::_pixels`
- `Bitmap::Clear`
- `Bitmap::ClearRect`
- `Canvas(Bitmap&)`
- `Canvas::Rasterize`
- `Canvas::BlendSpan`
- `Canvas::Blend`
- CPU版 `FillPath`
- CPU版 `StrokePath`
- CPU版 `DrawBitmap`
- CPU版 `DrawBoxShadow`
- CPU版 `DrawText`
- Glyph rasterization

代表箇所:

- `Skia.cpp:522` 付近: CPU pixel buffer確保
- `Skia.cpp:1476` 付近: `Canvas::Rasterize`
- `Skia.cpp:1635` 付近: `Canvas::BlendSpan`
- `Skia.cpp:1815` 付近: CPU `DrawBitmap`
- `Skia.cpp:2232` 付近: `DrawBoxShadow`
- `Skia.cpp:2501` 付近: `DrawText`
- `SkiaText.cpp:212` 付近: glyph rasterize

### 判定

**残存。**

これは単なる古い宣言だけではなく、後述する `RenderTargetBitmap` や診断処理から現在も到達可能。

---

# 2. 通常のデスクトップTopLevelはGPU

## 対象

`src/MphRead.Native/NativeRuntime/Avalonia/TopLevel.hpp`  
`src/MphRead.Native/NativeRuntime/Avalonia/TopLevel.cpp`  
`src/MphRead.Native/Mods/Launcher/Gui/UiSurface.cpp`  
`src/MphRead.Native/NativeRuntime/Skia/SkiaGpu.cpp`

`TopLevel` は以下を両方保持している。

```text
Skia::GpuSurface _surface;
Skia::Bitmap _pixels;
bool _gpuRendering = true;
```

`TopLevel::Render()` は `_gpuRendering` によって分岐する。

GPU側:

```text
_surface.BeginFrame()
Skia::Canvas canvas(_surface)
...
_surface.EndFrame()
```

CPU側:

```text
_pixels.Resize(...)
Skia::Canvas canvas(_pixels)
```

通常のデスクトップ `UiSurface` はCPUモードへ切り替えていない。

`UiSurface::Tick()` では描画後に

```text
_impl.TextureId()
UiOverlay::UseTexture(...)
```

を使っている。

つまり通常のUI TopLevelはCPU pixel bufferをOpenGLへ毎回アップロードする方式ではなく、GaneshのGPU surfaceをそのまま利用する。

### 判定

**正常。通常デスクトップUIのトップレベルはGPU。**

---

# 3. 重要: `DeckTile` にライブCPUオフスクリーン描画が残っている

## 対象

`src/MphRead.Native/Mods/Launcher/Gui/DeckTile.cpp`

これは今回の監査で最重要。

`DeckTile` は通常のGPU TopLevelの内部に存在するが、カード内容をCPU `RenderTargetBitmap` へ事前描画してキャッシュしている。

### 3.1 カード全体キャッシュ

`DeckTile.cpp:377` 付近:

```text
std::shared_ptr<Media::Imaging::RenderTargetBitmap> image;

image = std::make_shared<Media::Imaging::RenderTargetBitmap>(...)
auto into = image->CreateDrawingContext();
...
drawContent(*into);
```

`RenderTargetBitmap` はGPU surfaceではない。

`TopLevel.cpp:845` 付近:

```text
RenderTargetBitmap(...)
    : Bitmap(std::make_shared<Skia::Bitmap>(...))
```

さらに:

```text
CreateDrawingContext()
    -> Skia::Canvas(*_pixels)
```

なので、ここは明確な**CPU rasterization**。

### 3.2 Chromeキャッシュ

`DeckTile::Chrome()` でもCPU `RenderTargetBitmap` を生成。

`DeckTile.cpp:475` 付近。

カードの背景、ring、shadowなどがCPU側で描画される。

### 3.3 Groundキャッシュ

`DeckTile::Bake()` でもCPU `RenderTargetBitmap` を生成。

`DeckTile.cpp:513` 付近。

マップ画像、drift、scrimなどをCPU側で合成する。

### 3.4 GPU TopLevel上での挙動

CPUで完成した `RenderTargetBitmap` は最終的にGPU側DrawingContextから `DrawImage()` される。

`SkiaGpu.cpp` の `GpuSurface::ImageFor()` はCPU bitmapから

```text
SkPixmap
SkImages::RasterFromPixmapCopy(...)
```

または

```text
SkImage::MakeRasterCopy(...)
```

を作る。

その画像をGaneshがGPU描画に使用する。

したがって処理は概念的に以下。

```text
CPUでカードをRasterize
        ↓
CPU Bitmap
        ↓
Ganesh用SkImage化
        ↓
GPUへ画像転送
        ↓
OpenGL上で合成
```

### 影響範囲

`DeckTile` は少なくとも以下で利用されている。

- `PlayScreen`
- マップ選択カード
- Map Vote
- `EndPanelView`

`InGameMenu::OpenVote()` は `PlayScreen::Face::Vote` を開くため、**試合中のMap VoteでもこのCPUオフスクリーン描画へ到達可能**。

### 判定

**ライブ経路。CPUレンダリング残存。優先度: 高。**

---

# 4. `BakedBackdrop` に旧CPU RenderTargetBitmap実装が残存

## 対象

`src/MphRead.Native/Mods/Launcher/Gui/BakedBackdrop.cpp`

内部では:

```text
RenderTargetBitmap
target->Render(*layers)
```

を使い、背景をCPU側でbakeする完全なSoftware/off-screen経路が残っている。

ただし現在の `UiLayout::Backdrop()` は以下の方針へ変更済み。

- 背景gradientをGaneshで直接描く。
- 旧full-window CPU `RenderTargetBitmap` を避ける。

`UiLayout.cpp` 内にもその意図がコメントされている。

今回 `Mods/Launcher/Gui` 配下の全 `.cpp` を横断した範囲では、`BakedBackdrop` を現在生成している呼び出しは確認できなかった。

### 判定

**CPUレンダラ実装は残っているが、現行GUIでは休眠状態と判断。**

削除候補。

---

# 5. `MapShot::DecodeToWidth()` はCPUで画像縮小

## 対象

`src/MphRead.Native/Mods/Launcher/Gui/MapShot.cpp`  
`src/MphRead.Native/NativeRuntime/Avalonia/Media.cpp`

`MapShot` はマップサムネイルを読み込む際に

```text
Bitmap::DecodeToWidth(...)
```

を使用。

`Media.cpp:678` 付近では:

```text
auto scaled = std::make_shared<Skia::Bitmap>(width, height);
Skia::Canvas canvas(*scaled);
canvas.DrawBitmap(...)
```

となっている。

これはCPU bitmapに対するCPU resampling。

ただしキャッシュされるため、通常は毎フレームではなく画像ロード/キャッシュミス時。

### 判定

**CPU画像レンダリング残存。頻度は低い。**

---

# 6. Launcher背景ノイズはCPU生成

## 対象

`src/MphRead.Native/Mods/Render/NoiseField.cpp`  
`src/MphRead.Native/Mods/Render/LauncherNoise.cpp`  
`src/MphRead.Native/Mods/Render/LauncherPhoto.cpp`

`NoiseField::Fill()` はCPUループで全noise pixelを生成している。

概念:

```text
for y
    for x
        Noise(...)
        Noise(...)
        Noise(...)
        RGB pixel生成
```

`NoiseField::Gap = 33 ms`。

つまり最大で約30Hz周期でCPU側pixel fieldを再生成する。

その後 `LauncherNoise::Upload()` が

```text
GL::TexImage2D(...)
```

でGPUへ転送する。

### 重要な到達条件

デスクトップでは `Shell::Run()` が:

```text
LauncherPhoto::Enabled(true);
```

を設定する。

ゲームシーンが存在しない場合:

`Renderer.cpp:6061` 付近

```text
UiOverlay::DrawAlone(...)
```

が呼ばれる。

`DrawAlone()` は:

```text
LauncherPhoto::Draw(...)
```

を呼ぶ。

`LauncherPhoto::Draw()` は:

```text
LauncherNoise::Step(...)
```

を実行する。

一方、ゲームシーンが存在する通常試合中は:

`Renderer.cpp:6158` 付近

```text
UiOverlay::Draw(...)
LauncherHunter::Draw(...)
```

だけで、`DrawAlone()` は使われない。

### 判定

**CPU pixel generationは残っている。**

ただし通常デスクトップでは主に**ゲームシーンが無いランチャー画面**で動く。

ゲーム本体3Dレンダリングではない。

---

# 7. `MovingBackdrop` もCPU noise bitmap生成経路を保持

## 対象

`src/MphRead.Native/Mods/Launcher/Gui/MovingBackdrop.cpp`

`MovingBackdrop` も `NoiseField` をCPUで更新し、

```text
Media::Imaging::Bitmap::FromPremultipliedRgba(...)
```

でCPU bitmapを生成する。

タイマー間隔は `NoiseField::Gap`、つまり33 ms。

ただしデスクトップ通常起動では `LauncherPhoto::Enabled(true)` のため、

```text
UiLayout::PhotoDrawnBelow() == true
```

となり、`UiLayout::Backdrop()` は `MovingBackdrop` をvisual treeへ追加しない。

`MovingBackdrop` を追加するのは `PhotoDrawnBelow() == false` 側。

### 判定

**コードとしてはライブ可能なCPU生成経路だが、通常デスクトップShellでは基本的に使われない。**

Android側や特殊条件では別扱い。

---

# 8. 診断系は明示的にCPU TopLevelを使用

以下は意図的なSoftware rendering。

## `UiCapture`

`src/MphRead.Native/Mods/Launcher/Gui/UiCapture.cpp`

```text
topLevel.GpuRendering(false);
```

コメントでも:

```text
Capture is intentionally software/off-screen.
```

と明記。

コマンドラインの `uishot` で利用。

### 判定

**CPUレンダリング。診断/画像生成専用。**

---

## `UiBench`

`src/MphRead.Native/Mods/Launcher/Gui/UiBench.cpp`

FastRig:

```text
_impl.GpuRendering(false);
```

SlowRig:

```text
_impl.GpuRendering(false);
```

コメントでもCPU frame copyを測る設計と明記されている。

コマンドラインの `uibench` で利用。

### 判定

**CPUレンダリング。ベンチマーク専用。**

---

## `GamepadUiChecks`

`src/MphRead.Native/Mods/Launcher/Gui/GamepadUiChecks.cpp`

`Pump()` 内:

```text
topLevel.GpuRendering(false);
```

コメント:

```text
These checks inspect raw pixels; keep their off-screen root on
the software path.
```

### 判定

**CPUレンダリング。UI検証専用。**

---

# 9. `RenderTargetBitmap` API自体が完全なCPU描画APIとして残っている

## 対象

`src/MphRead.Native/NativeRuntime/Avalonia/TopLevel.cpp`

実装:

```text
RenderTargetBitmap(...)
    -> std::make_shared<Skia::Bitmap>()

CreateDrawingContext()
    -> Skia::Canvas(*_pixels)

Render(Visual&)
    -> Skia::Canvas canvas(*_pixels)
```

したがって、今後別のUIコードが `RenderTargetBitmap` を使うだけでもCPU rasterizationが再び通常経路へ入る。

### 判定

**CPU backendそのものが公開APIとして残存。**

---

# 10. GPU→CPU Readbackは存在するがCPUレンダリングではない

以下はSoftware Rendererではない。

## `Scene::ReadWindowBuffer`

`Renderer.cpp`:

```text
GL::ReadPixels(...)
```

## `Scene::ReadSceneTarget`

同じく:

```text
GL::ReadPixels(...)
```

## `Scene::CalibrateInk`

Cel shadingのdepth/outline calibrationで `GL::ReadPixels()` を使用。

## `ScreenCapture`

スクリーンショットやサムネイル保存時にGPU framebufferをCPU memoryへ読み戻す。

### 判定

**CPUレンダリングではない。**

ただし `glReadPixels()` はGPU pipeline stallを起こし得るため、性能調査では別項目として監視すべき。

通常のフレーム描画全体をCPUで行っている証拠ではない。

---

# 11. CPU texture preprocessingも存在する

これもSoftware Rendererとは区別する必要がある。

## `Scene::BindTexture`

`Renderer.cpp` ではmodel textureをCPU側で `std::vector<uint32_t>` へ展開し、

```text
GL::TexImage2D(...)
```

でGPUへアップロードする。

これはtexture decode/palette expansion/uploadであり、3D rasterizationではない。

## `LauncherPhoto`

JPEGをCPU bitmapへdecodeして、その後OpenGL textureへアップロードする。

## `MapThumbnail`

PNGをCPU decodeし、CPUループで指定サイズへ縮小してからGPU textureへアップロードする。

1フレームにつき1件までのdecode guardがあり、結果はcacheされる。

## `HunterStand`

取得済みpixelデータをCPU側でRGBAへ並び替え、bitmapを作る。

### 判定

**CPU画像処理。CPUレンダリングとは別だがCPU→GPU転送源にはなる。**

---

# 12. Android側について

## 関連ファイル

`src/MphRead.Native.Android/AndroidUiSurface.cpp`  
`src/MphRead.Native.Android/AndroidUiOverlay.cpp`  
`src/MphRead.Native.Android/GameView.cpp`

Android側には以下の構造が存在する。

```text
AndroidUiSurface::Painted()
    -> _impl->Pixels()
    -> CPU frameへcopy

AndroidUiSurface::TakeFrame()
    -> 別vectorへcopy

GameView::DrawUi()
    -> AndroidUiOverlay::Upload(...)

AndroidUiOverlay::Upload()
    -> glTexImage2D / glTexSubImage2D
```

つまりソース上は

```text
UI pixels
 ↓
CPU buffer
 ↓
CPU buffer copy
 ↓
GLES texture upload
 ↓
GPU composite
```

というSoftware frame-copy型の構造を保持している。

ただし注意点として、AndroidのCMakeでは:

```text
NativeRuntime/Avalonia/*.cpp
NativeRuntime/Skia/*.cpp
```

が `fruity_mphread_native` から除外されている。

また `AndroidUiSurface` 自体では `GpuRendering(false)` を明示していない一方、`TopLevel` のデフォルトはGPU。

したがってAndroidについては、デスクトップのCPU Rasterizerがそのまま確実に稼働していると断定するのではなく、**CPU frame-copy/uploadを前提にしたコードが残っており、現在のAndroidビルド構成との整合性を別途確認すべき状態**と判断する。

### 判定

**CPU pixel-copy/upload経路がソース上に残存。デスクトップとは別問題。**

---

# 13. CPUレンダリング残存箇所の分類

| 優先度 | 経路 | 通常実行 | 試合中到達 | 種類 |
|---|---|---:|---:|---|
| **高** | `DeckTile` full tile cache | Yes | **Yes** | CPU RenderTargetBitmap |
| **高** | `DeckTile::Chrome` | Yes | **Yes** | CPU RenderTargetBitmap |
| **高** | `DeckTile::Bake` | Yes | **Yes** | CPU RenderTargetBitmap |
| 中 | `LauncherNoise` | Yes | No | CPU procedural pixels → GPU upload |
| 中 | `MapShot::DecodeToWidth` | キャッシュミス時 | Map Voteで可能 | CPU resampling |
| 低 | `MovingBackdrop` | 通常desktopではNo | 通常No | CPU procedural pixels |
| 低 | `BakedBackdrop` | 現行呼び出し未確認 | No | 休眠CPU RenderTargetBitmap |
| 診断 | `UiCapture` | No | No | 強制CPU TopLevel |
| 診断 | `UiBench` | No | No | 強制CPU TopLevel |
| 診断 | `GamepadUiChecks` | No | No | 強制CPU TopLevel |
| 別分類 | `ScreenCapture` / `ReadPixels` | 必要時 | 可能 | GPU→CPU readback |
| 別分類 | texture decode/upload | 必要時 | 可能 | CPU画像処理 |

---

# 14. 試合レンダリングについての最終判定

## 3Dゲームシーン

**GPU OpenGL描画。CPU Software Renderer残存の証拠なし。**

ゲームシーン描画後の流れも:

```text
_scene->OnDrawFrame()
_scene->OnRenderFrame()
Shell::TickUi()
UiOverlay::Draw()
LauncherHunter::Draw()
SwapBuffers()
```

となっている。

コメントにも、ゲームのmap/model/HUD renderingは既存OpenGL pathのままであることが明記されている。

## ただし試合中UI

**CPUレンダリングが完全に排除されているわけではない。**

Map Voteなどで `DeckTile` を作成すると、

```text
CPU RenderTargetBitmap
 ↓
Ganesh image
 ↓
OpenGL texture/composite
```

が発生する。

したがって、

> 「C++版は試合中を含め完全GPUレンダリングになっているか」

への答えは **No**。

> 「ゲームの3Dシーン自体がCPUレンダリングされているか」

への答えは **No**。

---

# 15. 最近のSkia/OpenGL state問題との関係

`DeckTile` のCPU `RenderTargetBitmap` 自体はOpenGL stateを直接変更しない。

ただし、そのCPU bitmapをGPU TopLevelへ描画すると `SkiaGpu::ImageFor()` を経由してGanesh側へ画像が渡される。

この際、Ganeshはraster imageのGPU uploadを行う可能性がある。

現在の `SkiaGpu.cpp` がOpenGLの:

- framebuffer
- texture binding
- active texture
- pixel pack/unpack state
- PBO binding
- blend
- depth
- stencil
- viewport
- scissor
- その他Ganeshが変更するstate

を保存/復元しているのは、この経路との干渉を防ぐために重要。

したがって `DeckTile` CPU raster自体を「ゲームtexture破損の原因」と断定はできないが、

**Map Vote出現時はCPU bitmap生成 → Ganesh image uploadが増えるため、SkiaとゲームOpenGL stateが交差する代表的なタイミングである。**

最近の「Map Vote出現時にSkia UIがゲームレンダリングへ影響する」という症状を調べる場合、この経路は優先監視対象。

---

# 16. CPUレンダリングをライブ経路から完全撤去する場合の修正優先順位

## P0: `DeckTile` の `RenderTargetBitmap` キャッシュを撤去またはGPU化

対象:

- full tile cache
- `Chrome()`
- `Bake()`

最も単純な安全策は、Ganesh TopLevel上ではCPU bitmapへbakeせず、`drawContent(context)` を直接実行すること。

より高度な最適化を行うなら、CPU `RenderTargetBitmap` ではなくGPU-backed surface/image cacheを用意する。

### 理由

これが現在確認できた唯一の明確な**通常UIかつ試合中到達可能なCPU rasterization**。

---

## P1: Map thumbnailのCPU縮小を避ける

`Bitmap::DecodeToWidth()` のCPU `Skia::Canvas(Bitmap)` resamplingをやめる。

候補:

- 元画像をGPUへアップロードしてGPU sampling
- thumbnail生成時点で目的サイズを保存
- GPU側cacheへ直接配置

---

## P1: Launcher noiseをGPU shader化

現在はnoise値そのものをCPUで30Hz程度生成してtexture uploadしている。

noise生成までfragment shaderへ移せば:

```text
CPU Fill
CPU RGB buffer
glTexImage2D
```

を削除できる。

---

## P2: 休眠CPUコードを削除またはbuild flag化

対象:

- `BakedBackdrop`
- CPU TopLevel
- 汎用 `RenderTargetBitmap`
- CPU `Canvas` rasterizer

ただし `UiCapture`、`UiBench`、`GamepadUiChecks` を維持するなら、それら専用のbuild optionとして隔離する方が安全。

例:

```text
FRUITY_ENABLE_SOFTWARE_UI_TOOLS
```

通常release buildからCPU rasterizer自体を外せるようにすると、将来誤ってライブUIから再利用されることも防げる。

---

# 17. 最終まとめ

## 確認できたこと

- ゲーム3DシーンはOpenGL/GLES。
- デスクトップ通常UI TopLevelはSkia Ganesh GPU surface。
- 旧「UI全体をCPU描画して毎フレームupload」方式はデスクトップ通常経路では使われていない。
- **CPU rasterizer本体は現在も残存。**
- **`DeckTile` がライブUIからCPU `RenderTargetBitmap` を実際に使用している。**
- **Map Voteは試合中にも `DeckTile` を使うため、試合中CPU rasterizationはゼロではない。**
- Launcher noiseはCPUでpixel生成してGPU upload。
- `UiCapture`、`UiBench`、`GamepadUiChecks` は意図的にSoftware pathを使用。
- `glReadPixels` 系は残っているが、これはCPU renderingではなくGPU readback。
- `BakedBackdrop` はCPU実装が残っているものの、現行GUIからの利用は確認できなかった。

## 一番重要な結論

現在のC++版を

> 「ゲームもUIもすべてGPUだけでレンダリングしており、CPU rasterizationは一切ない」

とは言えない。

より正確には:

> **ゲーム3DはGPU。通常デスクトップUIの最終compositorもGPU。しかしUI内部の一部キャッシュ生成、画像縮小、procedural texture生成、診断処理にはCPUレンダリング/CPU pixel生成が残っている。特に `DeckTile` は試合中のMap Voteでも到達可能。**

CPUレンダリングをライブ経路から完全に無くすなら、最優先は `DeckTile.cpp` の3系統の `RenderTargetBitmap` 使用箇所。
