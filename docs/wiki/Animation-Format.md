| Structs |  |
| :- | :- |
| <ul><li>[AnimationHeader](#AnimationHeader)</li><li>[NodeAnimationGroup](#NodeAnimationGroup)</li><li>[MaterialAnimationGroup](#MaterialAnimationGroup)</li><li>[TexcoordAnimationGroup](#TexcoordAnimationGroup)</li><li>[TextureAnimationGroup](#TextureAnimationGroup)</li></ul> | <ul><li>[NodeAnimation](#NodeAnimation)</li><li>[MaterialAnimation](#MaterialAnimation)</li><li>[TexcoordAnimation](#TexcoordAnimation)</li><li>[TextureAnimation](#TextureAnimation)</li></ul> |

There are four types of animation: node, material, texcoord, and texture. Each of these types has a corresponding animation group type which holds the LUT values and other information, and which references one or more animation definitions which specify what portion of the LUTs to use and which model element to apply them to.

LUT lengths are not specified in the file. Either compare the start index and length values from each animation definition to see what the maximum index is (to get all used values), or compare various offset fields to infer the end point (to get all values regardless of whether they're used).

### Node Animation

Node animations control the translation, rotation, and scale of [[nodes|Model Format#Node]]. Unlike other animation types, node animation definitions are implicitly associated with nodes by index, rather than having a name which specifies the element they apply to.

### Material Aniamtion

Material animations control the diffuse, ambient, and specular colors and opacity of [[materials|Model Format#Material]]. Each animation definition has a name string which identifies the material it applies to.

### Texcoord Animation

Texcoord animations control the translation, rotation, and scale of texture coordinates (UVs). Each animation definition has a name string which identifies the material it applies to.

### Texture Animations

Texture animations control the active texture and palette used by a [[material|Model Format#Material]]. Each animation definition has a name string which identifies the material it applies to. Texture animations are only used in First Hunt.

## AnimationHeader

- Size: 0x18 (24)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | NodeGroupOffset | Offset to node group offsets
0x04 | 4 | uint | UnusedGroupOffset | Offset to unused group offsets
0x08 | 4 | uint | MaterialGroupOffset | Offset to material group offsets
0x0C | 4 | uint | TexcoordGroupOffset | Offset to texcoord group offsets
0x10 | 4 | uint | TextureGroupOffset | Offset to texture group offsets
0x14 | 2 | ushort | Count | Maximum count of any group type
0x16 | 2 | --- | --- | Padding

At each group type offset is a list of offsets with length equal to `Count`. The first `n` entries in each list will be offsets to a group of the given type, where `n` is the actual count of that group type, which is always less than or equal to `Count`. The remaining entries will be 0, so `n` can be determined by reading values until 0 is encountered. If there are no groups of a given type in the file, all the entries in the list will be 0; this is always the case for the unused group type.

## NodeAnimationGroup

- Size: 0x14 (20)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | FrameCount | Animation length in frames
0x04 | 4 | uint | ScaleLutOffset | Offset to scale LUT
0x08 | 4 | uint | RotateLutOffset | Offset to rotation LUT
0x0C | 4 | uint | TranslateLutOffset | Offset to translation LUT
0x10 | 4 | uint | AnimationOffset | Offset to animation definitions

`ScaleLutOffset` and `TranslateLutOffset` point to lists of `fx32` values. `RotateLutOffset` points to a list of `ushort` values. The latter is padded to 4 bytes.

Unlike other group types, this struct does not contain an animation definition count. Instead, this value can be assumed to be equal to the number of nodes in the model.

## MaterialAnimationGroup

- Size: 0x14 (20)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | FrameCount | Animation length in frames
0x04 | 4 | uint | ColorLutOffset | Offset to color LUT
0x08 | 4 | uint | AnimationCount | Number of animation definitions
0x0C | 4 | uint | AnimationOffset | Offset to animation definitions
0x10 | 2 | ushort | AnimationFrame | Current animation frame at runtime
0x12 | 2 | ushort | Unused12 | Possibly another frame index

`ColorLutOffset` points to a list of `byte` values. The list is padded to 4 bytes.

In First Hunt, `AnimationFrame` is a `uint` and there is no `Unused12` field.

## TexcoordAnimationGroup

- Size: 0x1C (28)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | FrameCount | Animation length in frames
0x04 | 4 | uint | ScaleLutOffset | Offset to scale LUT
0x08 | 4 | uint | RotateLutOffset | Offset to rotation LUT
0x0C | 4 | uint | TranslateLutOffset | Offset to translation LUT
0x10 | 4 | uint | AnimationCount | Number of animation definitions
0x14 | 4 | uint | AnimationOffset | Offset to animation definitions
0x18 | 2 | ushort | AnimationFrame | Current animation frame at runtime
0x1A | 2 | ushort | Unused1A | Possibly another frame index

`ScaleLutOffset` and `TranslateLutOffset` point to lists of `fx32` values. `RotateLutOffset` points to a list of `ushort` values. The latter is padded to 4 bytes.

In First Hunt, `AnimationFrame` is a `uint` and there is no `Unused1A` field.

## TextureAnimationGroup

- Size: 0x20 (32)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 2 | uint | FrameCount | Animation length in frames
0x02 | 2 | uint | FrameIndexCount | Number of frame indices
0x04 | 2 | uint | TextureIdCount | Number of texture indices
0x06 | 2 | uint | PaletteIdCount | Number of palette indices
0x08 | 2 | uint | AnimationCount | Number of animation definitions
0x0A | 2 | ushort | UnusedA | Unknown
0x0C | 4 | uint | FrameIndexOffset | Offset to frame indices
0x10 | 4 | uint | TextureIdOffset | Offset to texture indices
0x14 | 4 | uint | PaletteIdOffset | Offset to palette indices
0x18 | 4 | uint | AnimationOffset | Offset to animation definitions
0x1A | 2 | ushort | AnimationFrame | Current animation frame at runtime
0x1C | 2 | ushort | Unused1C | Possibly another frame index

`FrameIndexOffset`, `TextureIdOffset`, and `PaletteIdOffset` each point to a list of `ushort` values with length equal to `FrameIndexCount`, `TextureIdCount`, and `PaletteIdCount`, respectively. The third list is padded to 4 bytes.

In First Hunt, `AnimationFrame` is a `uint` and there is no `Unused1C` field.

## NodeAnimation

- Size: 0x30 (48)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 1 | byte | ScaleBlendX | LUT interpolation amount for scale X component
0x01 | 1 | byte | ScaleBlendY | LUT interpolation amount for scale Y component
0x02 | 1 | byte | ScaleBlendZ | LUT interpolation amount for scale Z component
0x03 | 1 | byte | Flags | Set at runtime; used to skip default S/R/T values
0x04 | 2 | ushort | ScaleLutLengthX | Number of LUT values used for scale X component
0x06 | 2 | ushort | ScaleLutLengthY | Number of LUT values used for scale Y component
0x08 | 2 | ushort | ScaleLutLengthZ | Number of LUT values used for scale Z component
0x0A | 2 | ushort | ScaleLutIndexX | LUT start index for scale X component
0x0C | 2 | ushort | ScaleLutIndexY | LUT start index for scale Y component
0x0E | 2 | ushort | ScaleLutIndexZ | LUT start index for scale Z component
0x10 | 1 | byte | RotateBlendX | LUT interpolation amount for rotation X component
0x11 | 1 | byte | RotateBlendY | LUT interpolation amount for rotation Y component
0x12 | 1 | byte | RotateBlendZ | LUT interpolation amount for rotation Z component
0x13 | 1 | --- | --- | Padding
0x14 | 2 | ushort | RotateLutLengthX | Number of LUT values used for rotation X component
0x16 | 2 | ushort | RotateLutLengthY | Number of LUT values used for rotation Y component
0x18 | 2 | ushort | RotateLutLengthZ | Number of LUT values used for rotation Z component
0x1A | 2 | ushort | RotateLutIndexX | LUT start index for rotation X component
0x1C | 2 | ushort | RotateLutIndexY | LUT start index for rotation Y component
0x1E | 2 | ushort | RotateLutIndexZ | LUT start index for rotation Z component
0x20 | 1 | byte | TranslateBlendX | LUT interpolation amount for translation X component
0x21 | 1 | byte | TranslateBlendY | LUT interpolation amount for translation Y component
0x22 | 1 | byte | TranslateBlendZ | LUT interpolation amount for translation Z component
0x23 | 1 | --- | --- | Padding
0x24 | 2 | ushort | TranslateLutLengthX | Number of LUT values used for translation X component
0x26 | 2 | ushort | TranslateLutLengthY | Number of LUT values used for translation Y component
0x28 | 2 | ushort | TranslateLutLengthZ | Number of LUT values used for translation Z component
0x2A | 2 | ushort | TranslateLutIndexX | LUT start index for translation X component
0x2C | 2 | ushort | TranslateLutIndexY | LUT start index for translation Y component
0x2E | 2 | ushort | TranslateLutIndexZ | LUT start index for translation Z component

## MaterialAnimation

- Size: 0x8C (140)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 64 | char[64] | Name | Name of the material to apply to
0x40 | 4 | uint | Unused40 | Always `0x01`
0x44 | 1 | byte | DiffuseBlendR | LUT interpolation amount for diffuse R component
0x45 | 1 | byte | DiffuseBlendG | LUT interpolation amount for diffuse G component
0x46 | 1 | byte | DiffuseBlendB | LUT interpolation amount for diffuse B component
0x47 | 1 | byte | Unused47 | Always `0x00`
0x48 | 2 | ushort | DiffuseLutLengthR | Number of LUT values used for diffuse R component
0x4A | 2 | ushort | DiffuseLutLengthG | Number of LUT values used for diffuse G component
0x4C | 2 | ushort | DiffuseLutLengthB | Number of LUT values used for diffuse B component
0x4E | 2 | ushort | DiffuseLutIndexR | LUT start index for diffuse R component
0x50 | 2 | ushort | DiffuseLutIndexG | LUT start index for diffuse G component
0x52 | 2 | ushort | DiffuseLutIndexB | LUT start index for diffuse B component
0x54 | 1 | byte | AmbientBlendR | LUT interpolation amount for ambient R component
0x55 | 1 | byte | AmbientBlendG | LUT interpolation amount for ambient G component
0x56 | 1 | byte | AmbientBlendB | LUT interpolation amount for ambient B component
0x57 | 1 | byte | Unused57 | Always `0xFF`
0x58 | 2 | ushort | AmbientLutLengthR | Number of LUT values used for ambient R component
0x5A | 2 | ushort | AmbientLutLengthG | Number of LUT values used for ambient G component
0x5C | 2 | ushort | AmbientLutLengthB | Number of LUT values used for ambient B component
0x5E | 2 | ushort | AmbientLutIndexR | LUT start index for ambient R component
0x60 | 2 | ushort | AmbientLutIndexG | LUT start index for ambient G component
0x62 | 2 | ushort | AmbientLutIndexB | LUT start index for ambient B component
0x64 | 1 | byte | SpecularBlendR | LUT interpolation amount for specular B component
0x65 | 1 | byte | SpecularBlendG | LUT interpolation amount for specular B component
0x66 | 1 | byte | SpecularBlendB | LUT interpolation amount for specular B component
0x67 | 1 | byte | Unused67 | Always `0x00`
0x68 | 2 | ushort | SpecularLutLengthR | Number of LUT values used for specular R component
0x6A | 2 | ushort | SpecularLutLengthG | Number of LUT values used for specular G component
0x6C | 2 | ushort | SpecularLutLengthB | Number of LUT values used for specular B component
0x6E | 2 | ushort | SpecularLutIndexR | LUT start index for specular R component
0x70 | 2 | ushort | SpecularLutIndexG | LUT start index for specular G component
0x72 | 2 | ushort | SpecularLutIndexB | LUT start index for specular B component
0x74 | 4 | uint | Unused74 | Unknown
0x78 | 4 | uint | Unused78 | Unknown
0x7C | 4 | uint | Unused7C | Unknown
0x80 | 4 | uint | Unused80 | Unknown
0x84 | 1 | byte | AlphaBlend | LUT interpolation amount for alpha
0x85 | 1 | byte | Unused85 | Always `0x01` in FH, `0xC1` in MPH
0x86 | 2 | ushort | AlphaLutLength | Number of LUT values used for alpha
0x88 | 2 | ushort | AlphaLutIndex | LUT start index for alpha
0x8A | 2 | ushort | MaterialId | Set at runtime

## TexcoordAnimation

- Size: 0x3C (60)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 32 | char[32] | Name | Name of the material to apply to
0x20 | 1 | byte | ScaleBlendS | LUT interpolation amount for scale S component
0x21 | 1 | byte | ScaleBlendT | LUT interpolation amount for scale T component
0x22 | 2 | ushort | ScaleLutLengthS | Number of LUT values used for scale S component
0x24 | 2 | ushort | ScaleLutLengthT | Number of LUT values used for scale T component
0x26 | 2 | ushort | ScaleLutIndexS | LUT start index for scale S component
0x28 | 2 | ushort | ScaleLutIndexT | LUT start index for scale T component
0x2A | 1 | byte | RotateBlendZ | LUT interpolation amount for rotation
0x2B | 1 | byte | Unused2B | Always `0x00` in FH, `0xFF` in MPH
0x2C | 2 | ushort | RotateLutLengthZ | Number of LUT values used for rotation
0x2E | 2 | ushort | RotateLutIndexZ | LUT start index for rotation
0x30 | 1 | byte | TranslateBlendS | LUT interpolation amount for translation S component
0x31 | 1 | byte | TranslateBlendT | LUT interpolation amount for translation T component
0x32 | 2 | ushort | TranslateLutLengthS | Number of LUT values used for translation S component
0x34 | 2 | ushort | TranslateLutLengthT | Number of LUT values used for translation T component
0x36 | 2 | ushort | TranslateLutIndexS | LUT start index for translation S component
0x38 | 2 | ushort | TranslateLutIndexT | LUT start index for translation S component
0x3A | 2 | --- | --- | Padding

## TextureAnimation

- Size: 0x2C (44)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 32 | char[32] | Name | Name of the material to apply to
0x20 | 2 | ushort | Count | Number of values in the animation
0x22 | 2 | ushort | StartIndex | Start index in the list of values
0x24 | 2 | ushort | MinimumPaletteId | Set at runtime
0x26 | 2 | ushort | MaterialId | Runtime index of the material to apply to
0x28 | 2 | ushort | MinimumTextureId | Set at runtime
0x2A | 2 | --- | --- | Padding

`MinimumPaletteId` and `MinimumTextureId` are computed at runtime and used by FH to select the texture and palette values for a given frame, but they are unnecessary. MPH uses the same data but calculates the values without using those fields (although MPH does not actually use texture animations in practice).