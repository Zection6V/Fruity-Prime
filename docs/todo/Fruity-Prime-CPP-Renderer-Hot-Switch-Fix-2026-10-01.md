# Desktop renderer switching: disappearing window fix (2026-10-01)

## Problem and repair

With `fd717d49`, switching either way could appear to close the application.
The replacement GLFW window is created with `StartVisible = false`, but
`RenderWindow::_startedHidden` remained false after revealing the original
window. Consequently `Reveal()` never showed the replacement, even though
the existing match continued simulating and rendering into it. Screenshot
readback and scene/frame identity alone did not detect this.

Reset the reveal lifecycle when recreating the platform window. Preserve its
current client size, location, border, maximized state and fullscreen floating
state directly, including when preference-based geometry recording is disabled.
The scene, player objects, network session and launcher screens survive.

The outgoing OpenGL device is now destroyed while its context is still current,
after scene/UI resources have been released. Previously `unique_ptr::release()`
abandoned the device allocation and its caches on every switch.

## Texture recovery and GPU rendering

Model textures now retain shared references and texture/palette/recolor IDs,
rather than a second expanded pixel image for each uploaded model texture.
On switching, reconstruct each texture from the original model asset under
the existing handle. The temporary expanded upload data is freed afterwards.
Model unload and scene release also remove these recovery records.

The first local fix used weak references and was committed as `095cdc29`
before push. A real settings switch exposed `A texture's source model expired
during renderer switching`: some textures outlive the uploading model instance,
and releasing the GPU mesh cache can release the last remaining model owner.
The recovery record must own the original model for the lifetime of the
scene-owned texture. Shared ownership preserves that asset without creating
a duplicate expanded pixel image. Model data contains no reference to the
scene, and model unload/scene release remove these owners.

HUD/dynamic uploads whose callers supply temporary pixel arrays still retain
recovery data. In the tested Alinos Perch match with Sylux and seven bots,
638 of 689 textures were restored from original model data; the remaining
51 textures retained 4,567,808 bytes (4.36 MiB). This is recovery storage,
not CPU rendering. Ordinary scene drawing remains on the selected GPU backend;
the change introduces no per-frame GPU readback or CPU rendering fallback.
Cross-API GPU memory sharing and eliminating all HUD recovery storage are not
implemented by this fix.

## Regression coverage and results

`FRUITY_SWITCHCHECK=1 -shellshot` now spawns the main player with seven active
bots, opens the pause menu, clicks Settings, changes the Renderer row, clicks
Apply, then clicks Resume. It repeats that settings path three times in the
same match. It checks the actual `GLFW_VISIBLE` attribute, backend change,
scene identity, advancing simulation frames, unchanged window geometry and
that the pause UI is hidden after Resume. Screenshots are taken after resuming.

The follow-up regression also uploads a texture from an uncached HUD model,
without creating a mesh, then releases the uploading model instance before
switching. The weak-reference version fails deterministically with the same
exception (`C:/tmp/gp/switch-source-before.log`, exit 1). The repaired version
must keep that source alive through every switch and resumed frame.
The final matrix passed all six source-retention checks and the source-release
check after ending the match through the normal production queue, in each
of the four configurations below. The synthetic probe adds one model texture
(690 total, 639 restored from original model data); recovery bytes remain
4,567,808, with no added expanded pixel copy.

Before the repair, the visibility assertions failed for the front-screen switch
and all three match switches (`switchfix-visible-before.log`, exit 1), while
the scene/frame assertions passed. After the repair:

| Offline Alinos Perch, Sylux + 7 bots | Result |
|---|---|
| Windowed, OpenGL start | exit 0, 3 settings switches + resumes |
| Windowed, Vulkan start | exit 0, 3 settings switches + resumes |
| Borderless fullscreen, OpenGL start | exit 0, 3 settings switches + resumes |
| Borderless fullscreen, Vulkan start | exit 0, 3 settings switches + resumes |

Each run also switched once on the front screen. All four runs passed visibility,
backend, scene continuity and geometry checks; no Vulkan validation messages.
Six PNGs per run, 24 total. Resumed game screenshots were visually inspected.

Build: `tools\build\build-cpp.bat msvc Release` passed. CTest: 5/5 passed.
Executable: `tools/build/out/msvc-Release/FruityPrime.exe`.
Final test logs and PNGs: `C:/tmp/gp/switch-source-v2-{windowed,borderless}-{opengl,vulkan}`
(logs have the same prefix plus `.log`). Runtime launcher preferences were
restored after the matrix.

```powershell
$env:FRUITY_SWITCHCHECK = '1'
$env:FRUITY_SHOT_ROOM = 'AD2 ALINOS PERCH'
# Run from tools/build/out/msvc-Release; use a fresh output directory.
.\FruityPrime.exe -shellshot C:/tmp/switch-check -rhi vulkan -vkvalidation -fpscap 60 -noupdate -debuglog
```

Android retains its next-start renderer setting. Online switching, unavailable
backend rollback, long-duration memory behavior and other GPUs/platforms were
not exercised by this matrix. The OS window is still recreated, so a brief
window transition remains possible.
