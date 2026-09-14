In MPH, collision meshes exist for rooms, platforms, and objects. Each mesh is defined in a separate `*_Collision.bin` file known as `wc01`. Meshes are made of one or more `CollisionData` structs which represent faces, each containing various parameters that describe the collision, and referencing a list of points to define the three or more vertices that make up the face. Each face also references a `vec4` which describes the plane the face lies in to aid in efficient collision detection, and all of this data is iterated using partitions associated with the `CollisionEntry` structs.

Partitions are cubes with a side length of 4 units. The first partition (corresponding to entry index 0) has its minimum bounds at the `MinPosition` coordinate. Each successive index corresponds to an advance of the cube's coordinates by 4 first in the positive X direction, then positive Z, then positive Y. The X coordinate resets and the Z coordinate increases once after reaching the last X partition in a row (defined by `PartsX`), and so to for the Z coordinate resetting and the Y coordinate increasing.

The collision file also includes `CollisionPortal` structs, which are planes that control which partial room sections are currently being rendered when the camera passes through them. In 1P mode, portals are also created at each door for room transitions, independently of those defined in the collision file.

# Structures

## Header

- Size: 0x54 (84)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | char[] | Magic | `wc01`
0x04 | 4 | uint | PointCount | Count of point vectors
0x08 | 4 | uint | PointOffset | Offset to point vectors
0x0C | 4 | uint | PlaneCount | Count of plane vectors
0x10 | 4 | uint | PlaneOffset | Offset to plane vectors
0x14 | 4 | uint | PointIndexCount | Counts of point indices
0x18 | 4 | uint | PointIndexOffset | Offset to point indices
0x1C | 4 | uint | DataCount | Count of data structs
0x20 | 4 | uint | DataOffset | Offset to data structs
0x24 | 4 | uint | DataIndexCount | Count of data indices
0x28 | 4 | uint | DataIndexOffset | Offset to data indices
0x2C | 4 | uint | PartsX | Number of partitions in X axis
0x30 | 4 | uint | PartsY | Number of partitions in Y axis
0x34 | 4 | uint | PartsZ | Number of partitions in Z axis
0x38 | 12 | vec3 | MinPosition | Minimum coordinates of the collision mesh
0x44 | 4 | uint | EntryCount | Count of entry structs
0x48 | 4 | uint | EntryOffset | Offset to entry structs
0x4C | 4 | uint | PortalCount | Count of portal structs
0x50 | 4 | uint | PortalOffset | Offset to portal structs

## CollisionData

- Size: 0x10 (16)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | int | Counter | Counter used at runtime
0x04 | 2 | ushort | PlaneIndex | Index of vector describing the plane this face lies in
0x06 | 2 | ushort | CollisionFlags | Collision flags
0x08 | 2 | ushort | LayerMask | Which room layers include this collision
0x0A | 2 | --- | --- | Padding
0x0C | 2 | ushort | PointIndexCount | Number of points in this face
0x0E | 2 | ushort | PointStartIndex | List index of this face's points

Bits 0 and 1 of the layer mask are a separate value indicating the majority axis of this collision data's plane's normal: 0 for X, 1 for Y, and 3 for Z. Bit 2 is a flag indicating the collision should always be used regardless of the room's layer mask. All other bits are part of the actual layer mask (and when layer filtering occurs, they are always compared against values from the room/mode that have their lower 3 bits cleared).

`CollisionFlags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Damaging | Hazardous terrain (not lava)
1 | 0x2 |  | 
2 | 0x4 |  | 
3-4 |  | Slipperiness | Index 0-3 determining friction factor
5-8 |  | Terrain type | ID 0-10 indicating type of the surface
9 | 0x200 | ReflectBeams | Whether colliding beams are reflected
10 | 0x400 |  | 
11 | 0x100 |  | 
12 | 0x1000 |  | 
13 | 0x2000 | IgnorePlayers | Whether players ignore this terrain
14 | 0x4000 | IgnoreBeams | Whether beams ignore this terrain
15 | 0x8000 | IgnoreScan | Whether this terrain does not block the Scan Visor

Terrain types:

| ID | Name |
:- | :-
0 | Metal
1 | Orange Holo
2 | Green Holo
3 | Blue Holo
4 | Ice
5 | Snow
6 | Sand
7 | Rock
8 | Lava
9 | Acid
10 | Gorea

Some code appears to check for terrain with type 11, but this type is never used in practice. Type values from 12 to 15 are also possible, but would not be handled specially by the game.

## CollisionEntry

- Size: 0x4 (4)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 2 | ushort | DataCount | Count of data structs
0x04 | 2 | ushort | DataStartIndex | List index of data structs

## CollisionPortal

- Size: 0xE0 (224)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 40 | char[] | Name | Portal name
0x28 | 24 | char[] | NodeName1 | Side 0 room node name
0x40 | 24 | char[] | NodeName1 | Side 1 room node name
0x58 | 12 | vec3 | Point1 | First point
0x64 | 12 | vec3 | Point2 | Second point
0x70 | 12 | vec3 | Point3 | Third point
0x7C | 12 | vec3 | Point4 | Fourth point
0x88 | 16 | vec4 | Plane1 | First plane
0x98 | 16 | vec4 | Plane2 | Second plane
0xA8 | 16 | vec4 | Plane3 | Third plane
0xB8 | 16 | vec4 | Plane4 | Fourth plane
0xC8 | 16 | vec4 | Plane | Portal plane
0xD8 | 2 | ushort | Flags | Always 0; set at runtime
0xDA | 2 | ushort | LayerMask | Which room layers include this portal
0xDC | 2 | ushort | PointCount | Always 4
0xDE | 1 | --- | --- | Unused
0xDF | 1 | --- | --- | Unused

The first four plane structs are related the portal's edges, while the final plane struct describes the plane the portal lies in. For example, `Plane1` describes a plane perpendicular to `Plane` in which the edge between `Point2` and `Point1` lies, `Plane2` describes a plane perpendicular to `Plane` in which the edge between `Point3` and `Point2` lies, and so on.

Like with `CollisionData`, bit 2 of the layer mask is a flag indicating the portal should always be used regardless of the room's layer mask.