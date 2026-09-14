- [Fixed](#Fixed)
- [Vector3Fx](#Vector3Fx)
- [ColorRgb](#ColorRgb)
- [CollisionVolume](#CollisionVolume)

## Fixed

- Size: 0x4 (4)

`Fixed` (`fx32`) is a fixed-point number, usually represented as a signed integer. The fixed-point numbers found in MPH structs have 1 sign bit, 19 bits for integral digits, and 12 bits for fractional digits. `4096` (`1 << 12`) is equal to floating-point `1.0`.

To convert to floating-point, divide the integer by `1 << 12` as a float. To convert back, multiply the float by `1 << 12` and truncate to an integer.

## Vector3Fx

- Size: 0xC (12)

`Vector3Fx` (`vec3fx32`) is a 3-element vector of `Fixed` values.

| Offset | Size | Type | Name
:- | :- | :- | :-
0x00 | 4 | fx32| X
0x04 | 4 | fx32 | Y
0x08 | 4 | fx32 | Z

## ColorRgb

- Size: 0x3 (3)

`ColorRgb` is a 3-element struct of Nintendo DS color values. NDS color values are in the range `0` to `31` (5-bit color). `0` is equivalent to a float color value of `0.0`, and `31` is equivalent to `1.0`.

| Offset | Size | Type | Name
:- | :- | :- | :-
0x00 | 1 | byte | Red
0x01 | 1 | byte | Green
0x02 | 1 | byte | Blue

## CollisionVolume

- Size: 0x40 (64)

`CollisionVolume` is a union of collision volume structs. It starts with a 4-byte type ID, and then contains the union of box, cylinder, and sphere, which have respective sizes 0x3C (60), 0x20 (32), and 0x10 (16).

| Offset | Size | Type | Name |
| :- | :- | :- | :- |
| 0x00 | 4 | uint | Type |
| **Box** | | | |
| 0x04 | 12 | vec3 | BoxVector1 |
| 0x10 | 12 | vec3 | BoxVector2 |
| 0x1C | 12 | vec3 | BoxVector3 |
| 0x28 | 12 | vec3 | BoxPosition |
| 0x34 | 4 | fx32 | BoxDot1 |
| 0x38 | 4 | fx32 | BoxDot2 |
| 0x3C | 4 | fx32 | BoxDot3 |
| **Cylinder** | | | |
| 0x04 | 12 | vec3 | CylinderVector |
| 0x10 | 12 | vec3 | CylinderPosition |
| 0x1C | 4 | vec3 | CylinderRadius |
| 0x20 | 4 | vec3 | CylinderDot |
| **Sphere** | | | |
| 0x04 | 12 | vec3 | SpherePosition |
| 0x10 | 4 | fx32 | SphereRadius |
