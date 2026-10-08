# Steep or distant surfaces turn into dense diagonal streaks (moiré) — on hold

Status: **On hold** (2026-10-08). The fix is known; it was deliberately deferred.

## Symptom

In AD2 ALINOS PERCH, the top face of the block in the middle is covered in
dense diagonal lines instead of its texture ("斜線がいっぱいになる"). On the DS the
same face shows only a few soft diagonal bands.

Observed on the C++ build (`FruityPrime.exe`), Vulkan, about 2000 px wide, at
1441 FPS.

## What was ruled out

- **Wrap mode:** `-mapmaterials "AD2 ALINOS PERCH"` now prints each material's
  wrap mode, texgen mode, texture scale and texcoord animation. None of the 28
  materials uses clamp, so this is not edge texels being smeared past a clamp.
- **Texcoord decode:** `RendererGeometry.cpp`'s `TEXCOORD` handling is
  `s16 / 16 / textureSize` in float, with nothing lost to rounding.
- **Repeat mapping:** `Scene::SamplerFor` maps Clamp/Mirror/Repeat to the RHI
  address modes correctly.

## Cause (diagnosis)

The face's UV mapping in the game's own data stretches the texture along one
direction and compresses it along the other. The DS shows that too, as soft
banding. The difference is that **world textures have no mipmaps** and are
drawn with nearest filtering:

- At 256×192 the compressed texels blur together into a few bands.
- At PC resolution every thin texel is sampled on its own, with nothing
  averaging them, and the result is moiré.

Expected tell: the lines crawl or flicker when the camera moves slowly. This
has not been checked in game yet.

## Proposed fix (not implemented)

1. Generate mip levels when a world texture is uploaded. `TextureDesc::mipLevels`
   and `SamplerDesc::mipFilter` / `maxAnisotropy` already exist in the RHI.
2. Keep **nearest magnification**, so textures up close still look as pixelated
   as on the DS. Use mipmapped minification, nearest or linear between levels.
3. Optionally, anisotropic filtering (`Capabilities::supportsAnisotropy`) to
   keep steep surfaces sharp.
4. Optionally, a setting to compare it against the current look.

Where it reaches:
- the OpenGL and Vulkan texture upload paths;
- the GPU rebuild when the renderer is switched in a running match
  (textures are recreated under the same handles; see CLAUDE.md, `-rhi`);
- palette-override and HUD textures (these probably keep a single level);
- every map's look at a distance, not just this room.

## Repro

1. Offline match on AD2 ALINOS PERCH.
2. Look at the top of the central block from close range at a shallow angle.
