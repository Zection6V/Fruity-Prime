Render instructions are commands which are used by the Nintendo DS GPU to render models. The instructions used by MPH models can have arity (argument count) between 0 and 2. Arguments are always 4 bytes (32 bits) in size.

Render instruction metadata is contained in [[DisplayList|Model Format#DisplayList]] structs in the model file. Render instructions outside of `DisplayList` data are used by MPH for other purposes, such as [[lighting]], and may include codes not listed here.

Color and alpha values are [[5-bit|Types#ColorRgb]] (0 to 31) values. Vertex, normal, and texcoord values are [[fixed-point|Types#Fixed]], but with differing numbers of bits for each part, which are outlined below.

| Instructions |  |  |  |  |  |  |
:- | :- | :- | :- | :- | :- | :-
[NOP](#NOP) | [MTX_RESTORE](#MTX_RESTORE) | [COLOR](#COLOR) | [NORMAL](#NORMAL) | [TEXCOORD](#TEXCOORD) | [VTX_16](#VTX_16) | [VTX_10](#VTX_10)
[VTX_XY](#VTX_XY) | [VTX_XZ](#VTX_XZ) | [VTX_YZ](#VTX_YZ) | [VTX_DIFF](#VTX_DIFF) | [DIF_AMB](#DIF_AMB) | [BEGIN_VTXS](#BEGIN_VTXS) | [END_VTXS](#END_VTXS)

## NOP

| ID | Code | Arity | Description
:- | :- | :- | :-
0x400 | NOP | 0 | Do nothing

## MTX_RESTORE

| ID | Code | Arity | Description
:- | :- | :- | :-
0x450 | MTX_RESTORE | 1 | Set the current matrix to the one at the given stack index

**Argument 0**

- Matrix stack index

| Bits | Count | Description |
:- | :- | :-
0 - 4 | 5 | Matrix stack index (0 to 30)
5 - 31 | 27 | Unused

This is used to apply node transforms to different vertices within a display list. See the section on [[model rendering]] for more information.

## COLOR

| ID | Code | Arity | Description
:- | :- | :- | :-
0x480 | COLOR | 1 | Set the vertex color of the next vertex

**Argument 0**

- Vertex color

| Bits | Count | Description |
:- | :- | :-
0 - 4 | 5 | Red color value
5 - 9 | 5 | Green color value
10 - 14 | 5 | Blue color value
15 - 31 | 17 | Unused

## NORMAL

| ID | Code | Arity | Description
:- | :- | :- | :-
0x484 | NORMAL | 1 | Set the normal vector of the next vertex

**Argument 0**

- Normal vector

| Bits | Count | Description |
:- | :- | :-
0 - 9 | 10 | X component
10 - 19 | 10 | Y component
20 - 29 | 10 | Z component
30 - 31 | 2 | Unused

The vector components have 1 sign bit, 0 bits for integral digits, and 9 bits for fractional digits.

## TEXCOORD

| ID | Code | Arity | Description
:- | :- | :- | :-
0x488 | TEXCOORD | 1 | Set texcoords (UVs) of the next vertex

**Argument 0**

- Texcoord values

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | S coordinate
16 - 31 | 16 | T coordinate

The UV coordinates have 1 sign bit, 11 bits for integral digits, and 4 bits for fractional digits.

## VTX_16

| ID | Code | Arity | Description
:- | :- | :- | :-
0x48C | VTX_16 | 2 | Set the next vertex coordinate

**Argument 0**

- XY coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | X coordinate
16 - 31 | 16 | Y coordinate

**Argument 1**

- Z coordinate

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | Z coordinate
16 - 31 | 16 | Unused

The vertex coordinates have 1 sign bit, 3 bits for integral digits, and 12 bits for fractional digits.

## VTX_10

| ID | Code | Arity | Description
:- | :- | :- | :-
0x490 | VTX_10 | 1 | Set the next vertex coordinate

**Argument 0**

- XYZ coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 9 | 10 | X coordinate
10 - 19 | 10 | Y coordinate
20 - 29 | 10 | Z coordinate
30 - 31 | 2 | Unused

The vertex coordinates have 1 sign bit, 3 bits for integral digits, and 6 bits for fractional digits.

## VTX_XY

| ID | Code | Arity | Description
:- | :- | :- | :-
0x494 | VTX_XY | 1 | Set the XY coordinates of the next vertex; keep previous Z coordinate

**Argument 0**

- XY coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | X coordinate
16 - 31 | 16 | Y coordinate

The vertex coordinates have 1 sign bit, 3 bits for integral digits, and 12 bits for fractional digits.

## VTX_XZ

| ID | Code | Arity | Description
:- | :- | :- | :-
0x498 | VTX_XZ | 1 | Set the XZ coordinates of the next vertex; keep previous Y coordinate

**Argument 0**

- XZ coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | X coordinate
16 - 31 | 16 | Z coordinate

The vertex coordinates have 1 sign bit, 3 bits for integral digits, and 12 bits for fractional digits.

## VTX_YZ

| ID | Code | Arity | Description
:- | :- | :- | :-
0x49C | VTX_YZ | 1 | Set the YZ coordinates of the next vertex; keep previous X coordinate

**Argument 0**

- YZ coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 15 | 16 | Y coordinate
16 - 31 | 16 | Z coordinate

The vertex coordinates have 1 sign bit, 3 bits for integral digits, and 12 bits for fractional digits.

## VTX_DIFF

| ID | Code | Arity | Description
:- | :- | :- | :-
0x4A0 | VTX_DIFF | 1 | Set the coordinates of the next vertex relative to the previous coordinates

**Argument 0**

- XYZ coordinates

| Bits | Count | Description |
:- | :- | :-
0 - 9 | 10 | Relative X coordinate
10 - 19 | 10 | Relative Y coordinate
20 - 29 | 10 | Relative Z coordinate
30 - 31 | 2 | Unused

The vertex coordinates have 1 sign bit, 0 bits for integral digits, and 9 bits for fractional digits.

## DIF_AMB

| ID | Code | Arity | Description
:- | :- | :- | :-
0x4C0 | DIF_AMB | 1 | Set the diffuse and ambient lighting colors of the next vertex

**Argument 0**

- Diffuse and ambient colors

| Bits | Count | Description |
:- | :- | :-
0 - 4 | 5 | Diffuse red color value
5 - 9 | 5 | Diffuse green color value
10 - 14 | 5 | Diffuse blue color value
15 | 1 | Whether to also set diffuse color as vertex color
16 - 20 | 5 | Ambient red color value
21 - 25 | 5 | Ambient green color value
26 - 30 | 5 | Ambient blue color value
31 | 1 | Unused

When this command is called from display lists, bit 15 of the argument is never set.

## BEGIN_VTXS

| ID | Code | Arity | Description
:- | :- | :- | :-
0x500 | BEGIN_VTXS | 1 | Begin specifying vertices for next polygon

**Argument 0**

- Polygon primitive type

| Bits | Count | Description |
:- | :- | :-
0 - 1 | 2 | Primitive type ID
2 - 31 | 30 | Unused

| ID | Primitive Type |
:- | :-
0 | Triangles
1 | Quads
2 | Triangle strip
3 | Quad strip

## END_VTXS

| ID | Code | Arity | Description
:- | :- | :- | :-
0x504 | END_VTXS | 0 | End specifying vertices for current polygon
