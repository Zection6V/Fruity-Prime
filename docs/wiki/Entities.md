Entities are game objects which follow a common class pattern to update and draw every frame. Almost everything which is not a [[room|Rooms]] or [[effect|Effects]] is an entity.

## MPH Entities

| ID | Entity | Notes |
:--|:--|:--
0 | Platform | Internally referred to as "mover"
1 | Object | 
2 | Player Spawn | 
3 | Door | 
4 | Item Spawn | 
5 | Item | Not found in entity files
6 | Enemy Spawn | 
7 | [Trigger](https://github.com/NoneGiven/MphRead/wiki/Entity:-Trigger) | 
8 | [Area Volume](https://github.com/NoneGiven/MphRead/wiki/Entity:-AreaVolume)| 
9 | Jump Pad | 
10 | Point Module | Unused leftover from FH
11 | Morph Camera | 
12 | Octolith Flag | 
13 | Flag Base | 
14 | Teleporter | 
15 | Defense Node | 
16 | Light Source | 
17 | Artifact | 
18 | Camera Sequence | 
19 | Force Field | 
21 | Beam Effect | Not found in entity files
22 | Bomb | Not found in entity files
23 | Enemy | Not found in entity files
24 | Halfturret | Not found in entity files
25 | Player | Not found in entity files
26 | Beam Projectile | Not found in entity files

Entity type 20 appears to be missing/unused. Entity type 27 is used internally to indicate the head item of an entity list.

## FH Entities

| ID | Entity | Notes |
:--|:--|:--
0 | Unknown 0 | Unused
1 | Player Spawn | 
2 | Unknown 2 | Unused
3 | Door | 
4 | Item Spawn | 
5 | Item | Not found in entity files
6 | Enemy Spawn | 
7 | Effect | Not found in entity files
8 | Bomb | Not found in entity files
9 | Trigger | 
10 | Area Volume | 
11 | Platform | Internally referred to as "mover"
12 | Jump Pad | 
13 | Point Module | 
14 | Morph Camera | 
15 | Enemy | Not found in entity files
16 | Player | Not found in entity files
17 | Beam Projectile | Not found in entity files

The unused entities 0 and 2 appear to be partially implemented and may have contained debug-only code. Entity type 18 is used internally to indicate the head item of an entity list.

## MPH Entity File Format

The MPH entity file contains a header, followed by entity entries, followed by entity data. The last entity entry is always a dummy record with a data offset field of 0, indicating the end of the entry list.

### File Header

- Size: 0x24 (36)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Version | Always 2
0x04 | 32 | ushort[] | LayerCounts | Number of entities included by the mask for each room layer index 0-15

### Entity Entry

- Size: 0x18 (24)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 16 | char[] | NodeName | Name of the partial room node this entity belongs to
0x10 | 2 | ushort | LayerMask | Bit field indicating whether this entity should be included for a given room layer
0x12 | 2 | ushort | Length | Size of this entity's data including the data header (always the same for a given entity type)
0x14 | 4 | uint | DataOffset | Offset to this entity's data, starting with the data header

## FH Entity File Format

The First Hunt entity file contains a version, followed by entity entries, followed by entity data. The last entity entry is always a dummy record with a data offset field of 0, indicating the end of the entry list.

### File Header

- Size: 0x4 (4)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Version | Always 1

### Entity Entry

- Size: 0x14 (20)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 16 | char[] | NodeName | Name of the partial room node this entity belongs to
0x10 | 4 | uint | DataOffset | Offset to this entity's data, starting with the data header

## Reading Entities

To parse the entity file, the list of entity entries should first be iterated. In MPH, for a given room layer index in the range, the layer mask will indicate whether this entity should be loaded. Room layers control different setups for the same room; for example, loading different entities before and defeating an area boss, or loading different entities in different multiplayer modes. Given the layer index, the corresponding bit in the mask is set if the entity should be loaded, and cleared otherwise. For example, a layer mask of 13 (`1101`) would load the entity in room layers 0, 2, and 3, but not in room layer 1. First Hunt does not have room layers.

If the entity should be loaded, its data offset can be followed to find its data. The layouts of each entity's data structure can be found in their individual wiki pages.

In MPH, the length values from the array in the file header can be used to validate the correct number of entities were loaded for the given room layer. This can also allow an early out when loading entities, as once the count has been reached, there is no need to check the masks on any remaining entity entries.