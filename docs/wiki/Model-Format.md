| Structs | Enums |
| :- | :- |
| <ul><li>[Header](#Header)</li><li>[Mesh](#Mesh)</li><li>[DisplayList](#DisplayList)</li><li>[Material](#Material)</li><li>[Texture](#Texture)</li><li>[Palette](#Palette)</li><li>[Node](#Node)</li></ul> | <ul><li>[CullingMode](#CullingMode)</li><li>[RepeatMode](#RepeatMode)</li><li>[PolygonMode](#PolygonMode)</li><li>[RenderMode](#RenderMode)</li><li>[TexgenMode](#TexgenMode)</li><li>[TextureFormat](#TextureFormat)</li><li>[BillboardMode](#BillboardMode)</li></ul> |

See also: [[Types]], [[Render Instructions]]

Model files begin with one `Header` struct. The header has offsets and counts to `Mesh`, `Material`, `Texture`, `Palette`, `Node`, and `TextureMatrix` structs. There is also an offset to `DisplayList` structs, and the count for display lists is the same as the number of meshes.

Texture and palette data can be found in either the model file or an external [[texture file]]. Animation data can be found in an external [[animation file|Animation Format]]. Some models have multiple [[recolors|recolor]], which are alternate sets of texture/palette data found in separate external files.

## Header

- Size: 0x64 (100)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | ScaleFactor | See `ScaleBase`
0x04 | 4 | fx32 | ScaleBase | `Scale = ScaleBase * (1 << ScaleFactor)`
0x08 | 4 | uint | PrimitiveCount | Count of triangles plus quads in all meshes
0x0C | 4 | uint | VertexCount | Count of vertices in all meshes
0x10 | 4 | uint | MaterialOffset | Offset to `Material` metadata
0x14 | 4 | uint | DlistOffset | Offset to `DisplayList` metadata
0x18 | 4 | uint | NodeOffset | Offset to `Node` metadata
0x1C | 2 | ushort | NodeWeightCount | Number of node IDs in the weight list
0x1E | 1 | byte | Flags | Set at runtime
0x1F | 1 | --- | --- | Padding
0x20 | 4 | uint | NodeWeightOffset | Offset to node weight IDs
0x24 | 4 | uint | MeshOffset | Offset to `Mesh` metadata
0x28 | 2 | ushort | TextureCount | Number of `Texture` structs
0x2A | 2 | --- | --- | Padding
0x2C | 4 | uint | TextureOffset | Offset to `Texture` metadata
0x30 | 2 | ushort | PaletteCount | Number of `Palette` structs
0x32 | 2 | --- | --- | Padding
0x34 | 4 | uint | PaletteOffset | Offset to `Palette` metadata
0x38 | 4 | uint | NodePosCounts | Offset to unused node position scale counts
0x3C | 4 | uint | NodePosScales | Offset to unused node position scales
0x40 | 4 | uint | NodeInitialPos | Offset to unused node initial positions
0x44 | 4 | uint | NodePos | Offset to unused node positions
0x48 | 2 | ushort | MaterialCount | Number of `Material` structs
0x4A | 2 | ushort | NodeCount | Number of `Node` structs
0x4C | 4 | uint | TextureMatrixOffset | Set at runtime
0x50 | 4 | uint | NodeAnimationOffset | Offset to `NodeAnimationGroup` metadata
0x54 | 4 | uint | TextureCoordinateAnimations | Offset to `TexcoordAnimationGroup` metadata
0x58 | 4 | uint | MaterialAnimations | Offset to `MaterialAnimationGroup` metadata
0x5C | 4 | uint | TextureAnimations | Offset to `TextureAnimationGroup` metadata
0x60 | 2 | ushort | MeshCount | Number of `Mesh` structs
0x62 | 2 | ushort | MatrixCount | Number of `TextureMatrix` structs

There are as many `int`s at the `NodePosCounts` offset as there are node weights. If `NodePos` or `NodeInitialPos` were used, there would be as many `vec3`s at each offset as there are nodes. If `NodePosScales` were used, there would be a number of `fx32`s at that offset based on the related `NodePosCounts` values.

`Flags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Lighting | Whether the model uses lighting
1 | 0x2 | Transform | Whether node transforms need re-computing
2 | 0x4 | --- | Unused
3 | 0x8 | --- | Unused
4 | 0x10 | --- | Unused
5 | 0x20 | --- | Unused
6 | 0x40 | --- | Unused
7 | 0x80 | --- | Unused

The `Flags` field is always 0 in model files.

Weight:

The list found at `NodeWeightOffset`, whose element count is equal to `NodeWeightCount`, holds the node IDs which correspond to matrix stack indices. For example, if the first value (index 0) is 3, that means the transform of the bone at index 3 should be loaded when a dlist calls `MTX_RESTORE` with an index of 0.

Texture matrices:

At runtime, this points to a list of matrices which hold the UV transforms computed from the initial values of material struct properties. Materials have an index into this list for which matrix to use; one matrix can be shared between materials in a model if they have the same UV transform values. Additionally, there is one hard-coded matrix set at runtime for the shield material of the Alimbic Capsule model.

## Mesh

- Size: 0x4 (4)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 2 | ushort | MaterialId | Index of this mesh's `Material`
0x02 | 2 | ushort | DlistId | Index of this mesh's `DisplayList`

## DisplayList

- Size: 0x20 (32)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Offset | Offset to render instruction data
0x04 | 4 | uint | Size | Size of data in bytes
0x08 | 12 | vec3 | MinCoordinates | Minimum X/Y/Z vertex coordinates
0x14 | 12 | vec3 | MaxCoordinates | Maximum X/Y/Z vertex coordinates

TODO: Document exactly what transforms apply to get the correct min/max coordinates

## Material

- Size: 0x84 (132)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 64 | char[] | Name | Material name
0x40 | 1 | byte | Lighting | Lighting mode
0x41 | 1 | byte | Culling | [Culling mode](#CullingMode)
0x42 | 1 | byte | Alpha | Material opacity (0 to 31)
0x43 | 1 | byte | Wireframe | Whether meshes using this material should be rendered as wireframes
0x44 | 2 | ushort | PaletteId | Index of this material's `Palette`
0x46 | 2 | ushort | TextureId | Index of this material's `Texture`
0x48 | 1 | byte | XRepeat | Horizontal texcoord [repeat mode](#RepeatMode)
0x49 | 1 | byte | YRepeat | Vertical texcoord [repeat mode](#RepeatMode)
0x4A | 3 | ColorRgb | Diffuse | Diffuse lighting color
0x4D | 3 | ColorRgb | Ambient | Ambient lighting color
0x50 | 3 | ColorRgb | Specular | Specular lighting color
0x53 | 1 | --- | --- | Padding
0x54 | 4 | uint | PolygonMode | [Polygon mode](#PolygonMode)
0x58 | 1 | byte | RenderMode | [Render mode](#RenderMode)
0x59 | 1 | byte | AnimationFlags | Material animation flags
0x5A | 2 | --- | --- | Padding
0x5C | 4 | uint | TexcoordTransformMode | [Texgen mode](#TexgenMode)
0x60 | 2 | ushort | TexcoordAnimationId | Texcoord animation ID at runtime
0x62 | 2 | --- | --- | Padding
0x64 | 4 | uint | MatrixId | Index of this material's `TextureMatrix`
0x68 | 4 | fx32 | ScaleS | Horizontal texcoord scale
0x6C | 4 | fx32 | ScaleT | Vertical texcoord scale
0x70 | 2 | ushort | RotateZ | Texcoord rotation
0x72 | 2 | --- | --- | Padding
0x74 | 4 | fx32 | TranslateS | Horizontal texcoord translation
0x78 | 4 | fx32 | TranslateT | Vertical texcoord translation
0x7C | 2 | ushort | MaterialAnimationId | Material animation ID at runtime
0x7E | 2 | ushort | TextureAnimationId | Texture animation ID at runtime
0x80 | 1 | byte | PackedRepeatMode | Packed native X/Y repeat at runtime
0x81 | 1 | --- | --- | Padding
0x82 | 2 | --- | --- | Padding

Any non-zero `Lighting` value indicates lighting is enabled. The values of 3 and 5 are possibly leftover flags for setting lights 0-3 individually. In the final game, only lights 0 and 1 are used, and if materials have lighting enabled, they are always affected by both.

`AnimationFlags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Color | Disable color animation
1 | 0x2 | Alpha | Disable alpha animation
2 | 0x4 | --- | Unused
3 | 0x8 | --- | Unused
4 | 0x10 | --- | Unused
5 | 0x20 | --- | Unused
6 | 0x40 | --- | Unused
7 | 0x80 | --- | Unused

The AnimationFlags field is set to 0 at runtime, so these flags are never used.

## Texture

- Size: 0x28 (40)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 1 | byte | Format | [Texture format](#TextureFormat)
0x01 | 1 | --- | --- | Padding
0x02 | 2 | ushort | Width | Image width
0x04 | 2 | ushort | Height | Image height
0x06 | 2 | --- | --- | Padding
0x08 | 4 | uint | ImageOffset | Offset to image data
0x0C | 4 | uint | ImageSize | Data size in bytes
0x10 | 4 | uint | UnusedOffset | Offset to unused data
0x14 | 4 | uint | UnusedCount | Count or size of unused data
0x18 | 4 | uint | VramOffset | VRAM offset at runtime
0x1C | 4 | uint | Opaque | Opacity flag
0x20 | 4 | uint | SkipVram | Whether to skip sending to VRAM (boolean; always 0)
0x24 | 1 | byte | PackedSize | Packed native width/height at runtime
0x25 | 1 | byte | NativeTextureFormat | Native format at runtime
0x26 | 2 | ushort | ObjectRef | VRAM object reference at runtime

## Palette

- Size: 0x10 (16)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Offset | Offset to palette data
0x04 | 4 | uint | Size | Size of palette data in bytes
0x08 | 4 | uint | VramOffset | VRAM offset at runtime
0x0C | 4 | ushort | ObjectRef | VRAM object reference at runtime

## Node

- Size: 0xF0 (240)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 64 | char[] | Name | Node name
0x40 | 2 | ushort | ParentId | Parent node index
0x42 | 2 | ushort | ChildId | Child node index
0x44 | 2 | ushort | NextId | Next node index
0x46 | 2 | --- | --- | Padding
0x48 | 4 | uint | Enabled | Whether this node should be rendered
0x4C | 2 | ushort | MeshCount | Number of `Mesh` structs
0x4E | 2 | ushort | MeshId | Index of the first mesh multiplied by 2
0x50 | 12 | vec3 | Scale | Node scale
0x5C | 2 | short | AngleX | Node X rotation
0x5E | 2 | short | AngleY | Node Y rotation
0x60 | 2 | short | AngleZ | Node Z rotation
0x62 | 2 | --- | --- | Padding
0x64 | 12 | vec3 | Position | Node position
0x70 | 4 | fx32 | BoundingRadius | Bounding radius used by some [[entities]]
0x74 | 12 | vec3 | MinCoordinates | Minimum X/Y/Z vertex coordinates
0x80 | 12 | vec3 | MaxCoordinates | Maximum X/Y/Z vertex coordinates
0x8C | 1 | byte | Billboard | [Billboard mode](#BillboardMode)
0x8D | 1 | --- | --- | Padding
0x8E | 2 | --- | --- | Padding
0x90 | 48 | mat43 | Transform | Node transform at runtime
0xC0 | 4 | uint | BeforeTransform | Runtime pointer to transform applied before parent
0xC4 | 4 | uint | AfterTransform | Runtime pointer to transform applied after parent
0xC8 | 4 | --- | --- | Unused
0xCC | 4 | --- | --- | Unused
0xD0 | 4 | --- | --- | Unused
0xD4 | 4 | --- | --- | Unused
0xD8 | 4 | --- | --- | Unused
0xDC | 4 | --- | --- | Unused
0xE0 | 4 | --- | --- | Unused
0xE4 | 4 | --- | --- | Unused
0xE8 | 4 | --- | --- | Unused
0xEC | 4 | --- | --- | Unused

The min/max coordinates are based on the min/max coordinates of the meshes attached to this node (or influenced by it, in the case of matrix stack skeleton transforms). See the [mesh](#Mesh) section for details.

## CullingMode

- Type: `byte`

| Value | Name | Description |
:- | :- | :-
0 | Neither | Face culling is disabled
1 | Front | Front faces are culled
2 | Back | Back faces are culled

## RepeatMode

- Type: `byte`

| Value | Name | Description |
:- | :- | :-
0 | Clamp | Texcoords are clamped between `0.0` and `1.0`
1 | Repeat | Texcoords repeat from `0.0` to `1.0`
2 | Mirror | Texcoords ping-pong between `0.0` and `1.0`

## PolygonMode

- Type: `uint`

| Value | Name | Description |
:- | :- | :-
0 | Modulate | Vertex color is multiplied by texture color
1 | Decal | Texture alpha is used as ratio between texture and vertex color
2 | Highlight | Texture color is multiplied by and added with toon table value indexed by vertex red

PolygonMode 3 would correspond to "Shadow," which is not used in model files by MPH or FH. PolygonMode 2 could indicate either "Toon" or "Highlight" shading depending on a per-frame display setting; MPH always uses Highlight.

## RenderMode

- Type: `byte`

| Value | Name | Description |
:- | :- | :-
0 | Normal | Default rendering
1 | Decal | Material only renders on another surface
2 | Translucent | Material renders with transparency
3 | Translucent | Material renders with transparency
4 | Translucent | Material renders with transparency

The render mode is used as a polygon ID by the game, so a value above 2 indicates a manually advanced polygon ID for a specific material.

## TexgenMode

- Type: `uint`

| Value | Name | Description
:- | :- | :-
0 | None | Default texcoord use
1 | Texcoord | Texcoord is multiplied by a matrix using material parameters
2 | Normal | Normal is multiplied by a matrix including the texcoord
3 | Vertex | Vertex is multiplied by a matrix including the texcoord

## TextureFormat

- Type: `ushort`

| Value | Name | Description
:- | :- | :-
0 | Palette2Bit | Palette with 4 palette entries per byte
1 | Palette4Bit | Palette with 2 palette entries per byte
2 | Palette8Bit | Palette with 1 palette entry per byte
4 | PaletteA5I3 | Palette with 3 bits for index and 5 bits for alpha
5 | DirectRgb | 5 bits each for red/blue/green and 1 bit for alpha
6 | PaletteA3I5 | Palette with 5 bits for index and 3 bits for alpha

Format 3 would correspond to "4x4-Texel Compressed Texture," which is not used by MPH or FH.

## BillboardMode

- Type: `byte`

| Value | Name | Description
:- | :- | :-
0 | None | Not a billboard
1 | Sphere | Spherical billboard
2 | Cylinder | Cylindrical billboard
