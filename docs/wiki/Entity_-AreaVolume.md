An area volume is an invisible [[entity|Entities]] which sends [[messages|Entity Messaging]] when other entities enter or exit its associated [[collision volume|Types#CollisionVolume]]. Area volumes are similar to [[triggers|Entity: Trigger]], with the main differences being that area volumes always use a collision volume rather than having different means of triggering, and that the entry message is sent repeatedly while an entity is inside it, rather than only on the first frame it enters. The exit message is sent once on the frame an entity leaves the volume.

Because of these characteristics, area volumes are usually used for persistent areas of effect, such as part of a room that damages the player, a low-gravity zone, or an area where the player can't switch forms. They are less often used to trigger room events in single-player mode. Another unique feature of area volumes is the priority and overlap system, which is used in Head Shot to create a volume of normal gravity inside the structure, encompassed in a larger volume of low gravity which only takes effect if the player is not also inside the inner volume.

The [trigger flags](#flags) field determines what can trigger this entity; in practice, area volumes are never set to be triggered by beams. Normally, the entry message is sent to the parent entity, and the exit message is set the child entity. However, the following message types are always sent instead to the player who triggered the volume (or player who owns the beam which did so): damage, death, gravity, prevent form switch, and Drip Moat platform lock.

Entity type: 8

See also: Visualization Guide ([[Part 1|Visualization Guide#area-volume]], [[Part 2|Visualization Guide#area-volumes]])

### File struct

- Size: 0x94 (148)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 40 | EntityDataHeader | Header | 
0x28 | 64 | CollisionVolume | Volume | Effective area
0x68 | 2 | --- | Unused68 | 
0x6A | 1 | byte | Active | Boolean
0x6B | 1 | byte | AlwaysActive | Boolean
0x6C | 1 | byte | AllowOverlap | Boolean; whether to trigger if player is in more than one
0x6D | 1 | byte | MessageDelay | Number of frames to queue outgoing message (0 or 1)
0x6E | 2 | --- | Unused6E | Boolean?
0x70 | 4 | uint | EntryMessageType | Message type ID to send when player is inside
0x74 | 4 | uint | EntryMessageParam1 | Message-specific data
0x78 | 4 | uint | EntryMessageParam2 | Message-specific data
0x7C | 2 | short | ParentId | Entity ID or -1
0x7E | 2 | --- | --- | Padding
0x80 | 4 | uint | ExitMessageType | Message type ID to send when player is outside
0x84 | 4 | uint | ExitMessageParam1 | Message-specific data
0x88 | 4 | uint | ExitMessageParam2 | Message-specific data
0x8C | 2 | short | ChildId | Entity ID or -1
0x8E | 2 | ushort | Cooldown | 
0x90 | 4 | byte | Priority | Boolean
0x94 | 4 | uint | Flags | 

### Runtime class

- Size: 0xBC (188)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 24 | CEntity | Entity | Runtime entity base class
0x18 | 12 | vec3 | Position | 
0x24 | 12 | vec3 | Facing | Unit facing avector
0x30 | 12 | vec3 | Up | Unit up vector
0x3C | 1 | byte | Active | Boolean
0x3D | 1 | byte | AllowOverlap | Boolean; whether to trigger if player is in more than one
0x3E | 1 | byte | MessageDelay | Number of frames to queue outgoing message (0 or 1)
0x3F | 4 | byte[4] | TriggeredSlots | Which players have triggered this, indexed by player slot
0x43 | 4 | byte[4] | PrioritySlots | Indexed by player slot
0x47 | 1 | --- | --- | Padding
0x48 | 4 | uint | EntryMessageType | Message type ID to send when player is inside
0x4C | 4 | uint | EntryMessageParam1 | Message-specific data
0x50 | 4 | uint | EntryMessageParam2 | Message-specific data
0x54 | 4 | uint | ExitMessageType | Message type ID to send when player is outside
0x58 | 4 | uint | ExitMessageParam1 | Message-specific data
0x5C | 4 | uint | ExitMessageParam2 | Message-specific data
0x60 | 4 | uint | Flags | Same as file struct
0x64 | 2 | ushort | Priority | 
0x66 | 2 | ushort | Cooldown | 
0x68 | 8 | ushort[4] | CooldownSlots | Indexed by player slot
0x70 | 4 | uint | Parent | Pointer to parent entity (Initially set to `ParentId`)
0x74 | 4 | uint | Child | Pointer to child entity (Initially set to `ChildId`)
0x78 | 4 | uint | NodeRef | Pointer to node ref
0x7C | 64 | Collision volume | Volume | Effective area

### Flags

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0-7 |  | Beams | Triggered by the corresponding beam if `1 << beam_id` is set
8 | 0x100 | Charged | If set, beams only trigger if charged
9 | 0x200 | Biped | Triggered by player in biped form
10 | 0x400 | Alt | Triggered by player in alt form
12 | 0x1000 | Bots | If cleared, players only trigger if not bots 

Only players and player beams are checked for collision with the volume. Weavel's Halfturret does not count as a player.

Note: The Omega Cannon (beam ID 8) will trigger volumes with the Charged flag set, platform beams (beam ID 9) will trigger volumes with the Biped flag set, and enemy beams (beam ID 10) will trigger volumes with the Alt flag set (subject also to the Charged flag). These behaviors are not observable in practice with either area volumes or triggers.

## Examples

### Entry Type 0 - None

Orange is used here to distinguish the volume. MphRead displays these volumes in black.

<img src="https://user-images.githubusercontent.com/2163967/90986112-82a0af80-e54e-11ea-96de-57332989d153.png" height="400" alt="00-01"> <img src="https://user-images.githubusercontent.com/2163967/90986113-83394600-e54e-11ea-9168-e63becd803b1.png" height="400" alt="00-02">

### Entry Type 5 - SetActive

<img src="https://user-images.githubusercontent.com/2163967/90986120-86343680-e54e-11ea-9a09-02c9ac1da1fe.png" height="400" alt="5-01">

### Entry Type 7 - Damage

<img src="https://user-images.githubusercontent.com/2163967/90986124-86cccd00-e54e-11ea-89ce-20a706fceed6.png" height="400" alt="7-01"> <img src="https://user-images.githubusercontent.com/2163967/90986125-87656380-e54e-11ea-9f34-d87398dab312.png" height="400" alt="7-02">
<img src="https://user-images.githubusercontent.com/2163967/90986126-87656380-e54e-11ea-8c87-dbe368494565.png" height="400" alt="7-03">

### Entry Type 15 - Gravity

<img src="https://user-images.githubusercontent.com/2163967/90986128-8a605400-e54e-11ea-8a28-6d76a140194c.png" height="400" alt="15-01"> <img src="https://user-images.githubusercontent.com/2163967/90986129-8a605400-e54e-11ea-8986-a71f168ec85c.png" height="400" alt="15-02">

### Entry Type 18 - Activate

<img src="https://user-images.githubusercontent.com/2163967/90986130-8af8ea80-e54e-11ea-8daf-fb89833f185f.png" height="400" alt="18-01"> <img src="https://user-images.githubusercontent.com/2163967/90986131-8af8ea80-e54e-11ea-8dfe-bfb27e0c4179.png" height="400" alt="18-02">
<img src="https://user-images.githubusercontent.com/2163967/90986132-8b918100-e54e-11ea-8cd3-0215cb8debbd.png" height="400" alt="18-03">

### Entry Type 21 - Death

<img src="https://user-images.githubusercontent.com/2163967/90986134-8d5b4480-e54e-11ea-8218-f5e083916e33.png" height="400" alt="21-01"> <img src="https://user-images.githubusercontent.com/2163967/90986137-8e8c7180-e54e-11ea-8c6a-83dbd127fc51.png" height="400" alt="21-02">
<img src="https://user-images.githubusercontent.com/2163967/90986139-90eecb80-e54e-11ea-8cb7-5dfc4e9335b1.png" height="400" alt="21-03">

### Entry Type 23 - Ship Hatch

<img src="https://user-images.githubusercontent.com/2163967/90986140-91876200-e54e-11ea-975d-4f940ed9f2db.png" height="400" alt="23-01">

### Entry Type 25 - Unknown/Unused

<img src="https://user-images.githubusercontent.com/2163967/90986141-921ff880-e54e-11ea-89c6-8eac26bfdda8.png" height="400" alt="25-01">

### Entry Type 35 - Prevent Form Switch

<img src="https://user-images.githubusercontent.com/2163967/90986142-93512580-e54e-11ea-8995-f5f48046aa0c.png" height="400" alt="35-01"> <img src="https://user-images.githubusercontent.com/2163967/90986143-93e9bc00-e54e-11ea-8827-0ed78a9c9d30.png" height="400" alt="35-02">
<img src="https://user-images.githubusercontent.com/2163967/90986145-94825280-e54e-11ea-805e-72230ff43f38.png" height="400" alt="35-03">

### Entry Type 44 - Platform Wakeup

<img src="https://user-images.githubusercontent.com/2163967/90986146-94825280-e54e-11ea-9051-697219cb3bde.png" height="400" alt="44-01"> <img src="https://user-images.githubusercontent.com/2163967/90986147-951ae900-e54e-11ea-8156-dd6ba0d9d5ed.png" height="400" alt="44-02">

### Entry Type 46 - Drip Moat Platform Lock

<img src="https://user-images.githubusercontent.com/2163967/90986151-9a783380-e54e-11ea-9587-5536a8cba3cf.png" height="400" alt="46-01">

### Entry Type 56 - Unlock Oubliette

<img src="https://user-images.githubusercontent.com/2163967/90986152-9a783380-e54e-11ea-9b1c-67ed4428ca80.png" height="400" alt="56-01">

### Entry Type 57 - Checkpoint

<img src="https://user-images.githubusercontent.com/2163967/90986153-9b10ca00-e54e-11ea-9594-4524a0e3d1d0.png" height="400" alt="57-01"> <img src="https://user-images.githubusercontent.com/2163967/90986154-9ba96080-e54e-11ea-81a4-0b9e55bb52ac.png" height="400" alt="57-02">
<img src="https://user-images.githubusercontent.com/2163967/90986159-9ea45100-e54e-11ea-936e-254a97e26ca4.png" height="400" alt="57-03">

### Entry Type 58 - Update Escape Sequence

<img src="https://user-images.githubusercontent.com/2163967/90986160-9ea45100-e54e-11ea-8ae0-d5cf60dc3cba.png" height="400" alt="58-01">
