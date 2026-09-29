# Native renderer — the frame's render pass sequence

What the C++ renderer (`src/MphRead.Native`) does to the GPU in one frame, in
order, stated as RHI rendering scopes (`CommandList::BeginRendering` /
`EndRendering`) with their attachments, load/store ops and pipelines. This is
the contract the OpenGL backend implements today and the Vulkan backend has to
implement: every scope below maps one-to-one onto a `vkCmdBeginRendering`
(dynamic rendering) with the same attachments, `loadOp`/`storeOp` and
`renderArea`.

Targets (`Scene::CreateSceneTargets`, `Scene::UpdateDepthAttachment`):

| Name | Format | Usage |
|---|---|---|
| SceneColor | RGB8 | colour attachment, sampled by the composite, copied into CelColor |
| SceneDepthStencil | D24S8 | depth/stencil attachment only |
| CelDepth | D24S8 | replaces SceneDepthStencil while the cel outline is on; sampled by the outline |
| CelColor | RGB8 | a copy of SceneColor, sampled by the outline |
| Window | the swapchain image | the composite, the HUD and the fade |

"Scene target" below is SceneColor over CelDepth when the cel outline is on
and SceneDepthStencil otherwise.

## The sequence

| # | Scope | Colour | Depth | Stencil | Pipelines / work |
|---|---|---|---|---|---|
| 1 | Scene target | **Clear** to the room's clear colour | **Clear** 1.0 | **Clear** 0 | `Opaque` (non-decal items), `Decal` (decal items), `TranslucentStencil` (translucent items, stencil reference = the item's polygon id) |
| 2 | Scene target | Load | **Clear** 1.0 | Load (the ids pass 1 wrote) | `DepthRebuild` (non-decal items, depth only), `TranslucentNotEqual`, `TranslucentEqual` (translucent items, reference = polygon id), `AfterScene` |
| 3 | Scene target, render area = the preview corner | **Clear** to the preview's background | **Clear** 1.0 | Load | `Preview` (the results screen's hunter). Only while the preview is up |
| 4 | Scene target | Load | Load | Load | HUD models (helmet, damage and filter models) |
| 5 | Scene target → copy | — | — | — | SceneColor is copied into CelColor (`CopyColorAttachmentToTexture`). Only with the cel outline |
| 6 | SceneColor only | Load | — | — | The cel outline: reads CelColor and CelDepth, writes SceneColor. Only with the cel outline |
| 7 | Window | **Clear** to the room's clear colour | — | — | The composite of SceneColor (RTT or disruption/whiteout shift program), then the HUD layers, the HUD objects, a movie frame and the fade |

Everything is stored: nothing in the sequence discards an attachment, and
stencil survives scope 1 → 2 on purpose — the translucent resolve passes test
against the polygon ids the stencil pre-pass wrote before the depth was
cleared.

## Why each boundary is where it is

- **Scope 1 → 2, the depth clear.** Translucent surfaces are first drawn with no
  colour to stamp their polygon ids into the stencil where they are nearest,
  then the depth is thrown away and rebuilt from the opaque geometry alone, so
  that each translucent polygon can be drawn over what is behind it but not
  over another translucent polygon with the same id. The clear is the only
  thing that has to happen between the two halves, and it is a depth-only
  `LoadOp::Clear` with colour and stencil loaded.
- **Scope 3, the preview.** The hunter in the results screen's corner is drawn
  into the scene target with its own projection. Its render area confines the
  clear and the draws to the corner; nothing else of the frame is touched.
- **Scopes 5–6, the cel outline.** The outline reads the finished scene colour
  and depth, so the colour is copied out first and the outline writes back
  over SceneColor with no depth attached — CelDepth is being read.
- **Scope 7, the window.** The composite covers the whole window, so the
  clear is invisible; it is there so the window is defined where a
  translucent composite would let it show.

## Pipeline variants

`Scene::DescribeScenePass` is the table. Each item's culling (none/back/front,
none when face culling is switched off), fill (solid/wireframe) and line
width are laid on top by `Scene::ScenePipeline`; the stencil reference is
dynamic (`SetStencilReference`).

| Variant | Depth | Stencil | Blend | Colour write | Alpha test | Depth bias |
|---|---|---|---|---|---|---|
| Opaque | test Less, write | Always, zero/zero/zero | off | all | EqualOne | — |
| Decal | test LEqual, write | Always, zero/zero/zero | SrcAlpha, 1−SrcAlpha | all | — | −1 / −1 |
| TranslucentStencil | test LEqual, write | Greater, keep/keep/replace | on | none | LessThanOne | — |
| DepthRebuild | test LEqual, write | Always, keep | on | none | EqualOne | — |
| TranslucentNotEqual | test LEqual, no write | NotEqual, keep | SrcAlpha, 1−SrcAlpha | all | LessThanOne | — |
| TranslucentEqual | test LEqual, no write | Equal, keep | SrcAlpha, 1−SrcAlpha | all | LessThanOne | — |
| AfterScene | test LEqual, write | off | SrcAlpha, 1−SrcAlpha | all | — | — |
| Preview | test Less, write | off | SrcAlpha, 1−SrcAlpha | all | — | — |

The alpha test is a discard in the fragment shader against the colour's
8-bit value (`alpha_test`), which is exactly what fixed-function
`glAlphaFunc(GL_EQUAL/GL_LESS, 1.0)` did; Vulkan has no alpha test and uses
the same shader logic.

## What differs from the pre-RHI renderer

One thing, deliberately: the scene target used to be cleared with whatever
`glClearColor` the previous frame had left behind, and the hunter preview
(`glClearColor(0,0,0,0)` after its own clear) and the launcher overlay
(`glClearColor(0,0,0,1)`) both leave one. It is now always the room's clear
colour. The two only differ in the First Hunt rooms whose fog is their sky,
where the void behind the level turned black for a frame after the pause
menu or the results screen drew; every multiplayer room clears to black
either way.
