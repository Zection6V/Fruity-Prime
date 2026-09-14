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

<table>
<thead>
<tr><th>Offset</th><th>Size</th><th>Type</th><th>Name</th></tr>
</thead>
<tbody>
<tr><td>0x00</td><td>4</td><td>uint</td><td>Type</td></tr>
</tbody>
<tbody>
<tr><td colspan="4"><b>Box</b></td></tr>
</tbody>
<tbody>
<tr><td>0x04</td><td>12</td><td>vec3</td><td>BoxVector1</td></tr>
<tr><td>0x10</td><td>12</td><td>vec3</td><td>BoxVector2</td></tr>
<tr><td>0x1C</td><td>12</td><td>vec3</td><td>BoxVector3</td></tr>
<tr><td>0x28</td><td>12</td><td>vec3</td><td>BoxPosition</td></tr>
<tr><td>0x34</td><td>4</td><td>fx32</td><td>BoxDot1</td></tr>
<tr><td>0x38</td><td>4</td><td>fx32</td><td>BoxDot2</td></tr>
<tr><td>0x3C</td><td>4</td><td>fx32</td><td>BoxDot3</td></tr>
</tbody>
<tbody>
<tr><td colspan="4"><b>Cylinder</b></td></tr>
</tbody>
<tbody>
<tr><td>0x04</td><td>12</td><td>vec3</td><td>CylinderVector</td></tr>
<tr><td>0x10</td><td>12</td><td>vec3</td><td>CylinderPosition</td></tr>
<tr><td>0x1C</td><td>4</td><td>vec3</td><td>CylinderRadius</td></tr>
<tr><td>0x20</td><td>4</td><td>vec3</td><td>CylinderDot</td></tr>
</tbody>
<tbody>
<tr><td colspan="4"><b>Sphere</b></td></tr>
</tbody>
<tbody>
<tr><td>0x04</td><td>12</td><td>vec3</td><td>SpherePosition</td></tr>
<tr><td>0x10</td><td>4</td><td>fx32</td><td>SphereRadius</td></tr>
</tbody>
</table>
