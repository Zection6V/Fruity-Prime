MphRead can visualize various forms of information that are hidden during gameplay, such as invisible entities, collision meshes, and entity volumes. Refer to the [[Viewer Controls]] guide for a full list of the keyboard shortcuts available for navigating these visualization modes.

Contents:

* [Invisible Entities](#Invisible-Entities)
* [Volumes & Planes](#Volumes--Planes)
* [Collision](#Collision)
* [Selection](#Selection)
* [Miscellaneous](#Miscellaneous)

## Invisible Entities

Several entity types are always invisible, and a few others can either have models or be invisible. Invisible entities still have a position in 3D space, and often have an orientation value as well. Toggling invisible entity display with `I` will make MphRead render these entities with a recolored version of [pick_wpn_missile](https://user-images.githubusercontent.com/2163967/93549020-7550c800-f936-11ea-80d4-6b944e62d6ef.png), an unused model in MPH.

### Platform

Always invisible: **No**

<a href="https://user-images.githubusercontent.com/2163967/109569442-76298080-7ab6-11eb-8851-662349ed1cce.png"><img src="https://user-images.githubusercontent.com/2163967/109569442-76298080-7ab6-11eb-8851-662349ed1cce.png" height="300" alt="platform-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109569443-76c21700-7ab6-11eb-8c4c-604b8714aeb3.png"><img src="https://user-images.githubusercontent.com/2163967/109569443-76c21700-7ab6-11eb-8c4c-604b8714aeb3.png" height="300" alt="platform-2"></a>

Invisible platforms are used for spawning beams hazards, or as targets for other entities to track (such as effect-spawning objects, or beams spawned by other platforms).

### Object

Always invisible: **No**

<a href="https://user-images.githubusercontent.com/2163967/109569787-fe0f8a80-7ab6-11eb-8273-404b32a6f71c.png"><img src="https://user-images.githubusercontent.com/2163967/109569787-fe0f8a80-7ab6-11eb-8273-404b32a6f71c.png" height="300" alt="object-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109569788-fe0f8a80-7ab6-11eb-8dd8-b5473b20fa2e.png"><img src="https://user-images.githubusercontent.com/2163967/109569788-fe0f8a80-7ab6-11eb-8dd8-b5473b20fa2e.png" height="300" alt="object-2"></a>

Invisible objects are used to spawn [[effects]], or as scan points for environmental features.

Note that invisible scan point objects are not the same as the objects used for lore scans. The former have no model, while the latter use the [AlimbicGhost_01](https://user-images.githubusercontent.com/2163967/87832725-a2a8c900-c854-11ea-9925-5265f93056b7.png) model while the Scan Visor is active. Additionally, some objects use the [GhostSwitch](https://user-images.githubusercontent.com/2163967/88707497-4fe6d100-d0e0-11ea-8c9b-1654cd287c17.png) model while the Scan Visor is active.

### Player Spawn

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109569834-0ff12d80-7ab7-11eb-8033-2f34adee756f.png"><img src="https://user-images.githubusercontent.com/2163967/109569834-0ff12d80-7ab7-11eb-8033-2f34adee756f.png" height="300" alt="player-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109569836-1089c400-7ab7-11eb-9dcd-d1f2e767d403.png"><img src="https://user-images.githubusercontent.com/2163967/109569836-1089c400-7ab7-11eb-9dcd-d1f2e767d403.png" height="300" alt="player-2"></a>

Player spawns are invisible entities located at coordinates where players can spawn. The player receives the orientation value of the chosen spawn point as well as its position when spawning there.

### Item Spawn

Always invisible: **No**

<a href="https://user-images.githubusercontent.com/2163967/109569856-1c758600-7ab7-11eb-9b39-70fd402d9f74.png"><img src="https://user-images.githubusercontent.com/2163967/109569856-1c758600-7ab7-11eb-9b39-70fd402d9f74.png" height="300" alt="item-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109569858-1c758600-7ab7-11eb-83a2-562374531867.png"><img src="https://user-images.githubusercontent.com/2163967/109569858-1c758600-7ab7-11eb-83a2-562374531867.png" height="300" alt="item-2"></a>

Item spawns hold information for spawning item instances. Most are invisible, but some in single-player mode use the [items_base](https://user-images.githubusercontent.com/2163967/87885123-6a82c100-c9e1-11ea-84e6-7a93b80b5e44.png) model.

### Enemy Spawn

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109569938-4038cc00-7ab7-11eb-8d04-290a749007a8.png"><img src="https://user-images.githubusercontent.com/2163967/109569938-4038cc00-7ab7-11eb-8d04-290a749007a8.png" height="300" alt="enemy-1"></a>

Enemy spawns hold information for spawning enemy instances. Enemy spawns are always invisible, but they can also spawn a special enemy type during initialization that serves as the visible spawner model. These use the [EnemySpawner](https://user-images.githubusercontent.com/2163967/93549011-741f9b00-f936-11ea-991f-c4fe65884dd7.png) model by default, and use the [PlantCarnivarous_Pod](https://user-images.githubusercontent.com/2163967/94982877-2bf2a200-050c-11eb-9341-ad7e29490624.png) model when spawning Barbed War Wasps.

### Trigger

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109569956-475fda00-7ab7-11eb-9830-6b0620c9d295.png"><img src="https://user-images.githubusercontent.com/2163967/109569956-475fda00-7ab7-11eb-9830-6b0620c9d295.png" height="300" alt="trigger-1"></a>

Triggers are invisible entities which send [[messages|Entity Messaging]] to other entities. There are five different [[subtypes|Entity: Trigger#Trigger subtypes]], one of which has an associated volume that serves as the trigger area.

### [[Area Volume|Entity: AreaVolume]]

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109569974-4e86e800-7ab7-11eb-8ae4-e41fa3b5bdc7.png"><img src="https://user-images.githubusercontent.com/2163967/109569974-4e86e800-7ab7-11eb-8ae4-e41fa3b5bdc7.png" height="300" alt="area-1"></a>

Area volumes are invisible entities which are similar to the volume subtype of triggers. They have an associated volume and send [[messages|Entity Messaging]] when the volume is entered or exited.

### Morph Camera

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109569990-53e43280-7ab7-11eb-9d8d-518bf08c9243.png"><img src="https://user-images.githubusercontent.com/2163967/109569990-53e43280-7ab7-11eb-9d8d-518bf08c9243.png" height="300" alt="morph-1"></a>

Morph camera entities are invisible entities with an associated volume. The game camera becomes fixed at the entity's position and swivels to face the player while they are within the volume and in alt form.

### Flag Base

Always invisible: **No**

<a href="https://user-images.githubusercontent.com/2163967/109570011-5d6d9a80-7ab7-11eb-982f-daabeb5820b8.png"><img src="https://user-images.githubusercontent.com/2163967/109570011-5d6d9a80-7ab7-11eb-982f-daabeb5820b8.png" height="300" alt="flag-1"></a>

A flag base entity is the dropoff point for scoring in Capture and Bounty modes. It has an associated volume used to determine when a player has dropped off an Octolith flag.

This entity is technically only visible in Bounty mode. In that mode, it uses the [flagbase_cap](https://user-images.githubusercontent.com/2163967/94982716-ee414980-050a-11eb-913b-b10ca6507490.png) model, while the [flagbase_bounty](https://user-images.githubusercontent.com/2163967/94982715-ee414980-050a-11eb-9d17-bdc4d7b3ee41.png) model at the pickup point is drawn elsewhere by an Octolith flag entity. In Capture mode, each team has a flag base entity and an Octolith flag entity at the same position, and in that mode, the latter is responsible for drawing the [flagbase_ctf](https://user-images.githubusercontent.com/2163967/87858033-346a1200-c8f9-11ea-8c18-b115442a0d37.png) model.

### Teleporter

Always invisible: **No**

<a href="https://user-images.githubusercontent.com/2163967/109570062-75451e80-7ab7-11eb-9d2a-3a0c71f6d0b6.png"><img src="https://user-images.githubusercontent.com/2163967/109570062-75451e80-7ab7-11eb-9d2a-3a0c71f6d0b6.png" height="300" alt="teleporter-1"></a>

Teleporters are usually visible, and use one of the [TeleporterMP](https://user-images.githubusercontent.com/2163967/93549031-771a8b80-f936-11ea-8c56-f7bc6091d479.png), [TeleporterSmall](https://user-images.githubusercontent.com/2163967/87833315-9c671c80-c855-11ea-9aa1-b5f490ceec13.png), or [Teleporter](https://user-images.githubusercontent.com/2163967/87833312-9c671c80-c855-11ea-9d04-7672a5a65ec7.png) models. In practice, the only invisible teleporters are some small teleporters in single-player. Additionally, because single-player teleporters require a destination teleporter in the target room, one-way teleports are accomplished with a disabled, invisible teleporter as the destination.

<a href="https://user-images.githubusercontent.com/2163967/109570081-82faa400-7ab7-11eb-9b92-ee4659f605e5.png"><img src="https://user-images.githubusercontent.com/2163967/109570081-82faa400-7ab7-11eb-9b92-ee4659f605e5.png" height="300" alt="destination-1"></a>

In multiplayer, each teleporter has an invisible destination point within the arena, and this can also be visualized by MphRead.

### Light Source

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109570088-87bf5800-7ab7-11eb-8942-c79af73db052.png"><img src="https://user-images.githubusercontent.com/2163967/109570088-87bf5800-7ab7-11eb-8942-c79af73db052.png" height="300" alt="light-1"></a>

Light source entities are associated with volumes which override the room light colors on player entities inside them.

### Camera Sequence

Always invisible: **Yes**

<a href="https://user-images.githubusercontent.com/2163967/109570108-8db53900-7ab7-11eb-926d-f631bbb3a185.png"><img src="https://user-images.githubusercontent.com/2163967/109570108-8db53900-7ab7-11eb-926d-f631bbb3a185.png" height="300" alt="camera-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109570110-8db53900-7ab7-11eb-9f09-54aca64dbf36.png"><img src="https://user-images.githubusercontent.com/2163967/109570110-8db53900-7ab7-11eb-9f09-54aca64dbf36.png" height="300" alt="camera-2"></a>

Some camera sequence entities are responsible for initiating in-game cutscenes, while others behave similarly to morph camera entities. The latter entity type overrides the game camera in order to track the player in alt form, but unlike morph cameras, the camera is not fixed at one point. All camera sequence entities must be activated externally (usually by a trigger or area volume) as they don't have associated volumes of their own.

## Volumes & Planes

Many entities have associated volumes that are used to check for collision with the player or other entities. MphRead can cycle through various types of volume and plane display with `Z`.

Note: `Alt+H` can be used to limit volume display to the selected entity, if any. See the section below for information about entity selection.

### Light Volumes

* Index 1 (Light 1)
* Index 2 (Light 2)

<a href="https://user-images.githubusercontent.com/2163967/109574732-7d548c80-7abe-11eb-8719-8ca929cf28f5.png"><img src="https://user-images.githubusercontent.com/2163967/109574732-7d548c80-7abe-11eb-8719-8ca929cf28f5.png" height="300" alt="light-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109574735-7e85b980-7abe-11eb-9438-8ba01d3928b9.png"><img src="https://user-images.githubusercontent.com/2163967/109574735-7e85b980-7abe-11eb-9438-8ba01d3928b9.png" height="300" alt="light-vol-2"></a>

Light source entities have an associated volume that will override one or both of the room lights for player entities inside it. MphRead colors the volume according to the light color. If one of the entity's lights is disabled, meaning the room light is left unchanged, the volume will be colored black.

### Trigger Volumes

* Index 3 (Parent event)
* Index 4 (Child event)

<a href="https://user-images.githubusercontent.com/2163967/109574763-8b0a1200-7abe-11eb-99ca-834b9a93bb1f.png"><img src="https://user-images.githubusercontent.com/2163967/109574763-8b0a1200-7abe-11eb-99ca-834b9a93bb1f.png" height="300" alt="trigger-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109574765-8ba2a880-7abe-11eb-938d-ed43ea172582.png"><img src="https://user-images.githubusercontent.com/2163967/109574765-8ba2a880-7abe-11eb-938d-ed43ea172582.png" height="300" alt="trigger-vol-2"></a>

If a trigger entity has the volume subtype, MphRead can display it colored according to the messages it will send. If the message is of type 0 (none), the volume will be colored black.

### Area Volumes

* Index 5 (Inside event)
* Index 6 (Exit event)

<a href="https://user-images.githubusercontent.com/2163967/90986125-87656380-e54e-11ea-9f34-d87398dab312.png"><img src="https://user-images.githubusercontent.com/2163967/90986125-87656380-e54e-11ea-9f34-d87398dab312.png" height="300" alt="area-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/90986128-8a605400-e54e-11ea-8a28-6d76a140194c.png"><img src="https://user-images.githubusercontent.com/2163967/90986128-8a605400-e54e-11ea-8a28-6d76a140194c.png" height="300" alt="area-vol-2"></a>

MphRead will display area volumes colored according to the messages they send. If a message is of type 0 (none), the volume will be colored black. Examples of the colors used can be found on the [[entity page|Entity: AreaVolume#Examples]].

### Morph Camera Volumes

* Index 7

<a href="https://user-images.githubusercontent.com/2163967/109574956-d4f2f800-7abe-11eb-9d05-267f258a8322.png"><img src="https://user-images.githubusercontent.com/2163967/109574956-d4f2f800-7abe-11eb-9d05-267f258a8322.png" height="300" alt="morph-vol-1"></a>

The associated volume of a morph camera entity will be colored yellow. The morph camera becomes active while the player is in alt form and inside this volume.

### Jump Pad Volumes

* Index 8

<a href="https://user-images.githubusercontent.com/2163967/109575055-0e2b6800-7abf-11eb-9cf5-b93cb03f1558.png"><img src="https://user-images.githubusercontent.com/2163967/109575055-0e2b6800-7abf-11eb-9cf5-b93cb03f1558.png" height="300" alt="jump-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109575057-0ec3fe80-7abf-11eb-902e-e6002eee6bd3.png"><img src="https://user-images.githubusercontent.com/2163967/109575057-0ec3fe80-7abf-11eb-902e-e6002eee6bd3.png" height="300" alt="jump-vol-2"></a>

Jump pads have an associated volume which is used to test for collision with the player and activate the jump pad acceleration. MphRead displays this volume in green.

### Teleporter Volumes

* Index 9

<a href="https://user-images.githubusercontent.com/2163967/141692822-b1055cc6-372f-4a00-998d-00b9f47d1e97.png"><img src="https://user-images.githubusercontent.com/2163967/141692822-b1055cc6-372f-4a00-998d-00b9f47d1e97.png" height="300" alt="tele-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/141692824-8d499d25-508d-4141-bf21-4fbfdbbace48.png"><img src="https://user-images.githubusercontent.com/2163967/141692824-8d499d25-508d-4141-bf21-4fbfdbbace48.png" height="300" alt="tele-vol-2"></a>

Teleporters will warp the player when they enter a certain spherical area. After being teleported, the player must move away a slightly greater distance before they can activate the teleporter again. The former area is visualized in red. The latter is not currently visualized, nor is the teleporter's animation activation radius.

### Enemy Hit Zones

* Index 10

<a href="https://user-images.githubusercontent.com/2163967/150605346-f1b6693f-77ab-45ed-8018-ac87d6b0d2f5.png"><img src="https://user-images.githubusercontent.com/2163967/150605346-f1b6693f-77ab-45ed-8018-ac87d6b0d2f5.png" height="300" alt="hit-zone-1"></a> <a href="https://user-images.githubusercontent.com/2163967/150605348-6f0a2673-4aa0-435b-a586-d88ecf2c784c.png"><img src="https://user-images.githubusercontent.com/2163967/150605348-6f0a2673-4aa0-435b-a586-d88ecf2c784c.png" height="300" alt="hit-zone-2"></a>

Most enemies have volumes that are checked for collision with beams in order to inflict damage, and often for collision with the player as well. These volumes are displayed in red. A handful of enemies spawn a special child enemy type to act as either a weak spot or a secondary collision volume.

### Object Effect Volumes

* Index 11

<a href="https://user-images.githubusercontent.com/2163967/109575108-2b603680-7abf-11eb-8b34-5116a9b57ade.png"><img src="https://user-images.githubusercontent.com/2163967/109575108-2b603680-7abf-11eb-8b34-5116a9b57ade.png" height="300" alt="effect-vol-1"></a>

Some objects use a volume to determine whether to spawn their effect based on where the camera is. MphRead displays these volumes in red.

### Flag Base Volumes

* Index 12

<a href="https://user-images.githubusercontent.com/2163967/109575214-58144e00-7abf-11eb-858e-7729c99c6461.png"><img src="https://user-images.githubusercontent.com/2163967/109575214-58144e00-7abf-11eb-858e-7729c99c6461.png" height="300" alt="flag-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109575215-58ace480-7abf-11eb-9429-b41f6b861d83.png"><img src="https://user-images.githubusercontent.com/2163967/109575215-58ace480-7abf-11eb-9429-b41f6b861d83.png" height="300" alt="flag-vol-2"></a>

The flag base entity uses a volume and checks for player collision for its dropoff point. These volumes are displayed in white.

### Defense Node Volumes

* Index 13

<a href="https://user-images.githubusercontent.com/2163967/109575390-b17c7d00-7abf-11eb-9940-8767548471e7.png"><img src="https://user-images.githubusercontent.com/2163967/109575390-b17c7d00-7abf-11eb-9940-8767548471e7.png" height="300" alt="defense-vol-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109575395-b2adaa00-7abf-11eb-94ec-ce1721bb33ab.png"><img src="https://user-images.githubusercontent.com/2163967/109575395-b2adaa00-7abf-11eb-94ec-ce1721bb33ab.png" height="300" alt="defense-vol-2"></a>

The nodes in the Defender and Nodes game modes use a volume to determine whether a player is occupying them. These are displayed in white.

### Room Kill Plane

* Index 14

<a href="https://user-images.githubusercontent.com/2163967/109575584-189a3180-7ac0-11eb-9694-4f4e8f80706b.png"><img src="https://user-images.githubusercontent.com/2163967/109575584-189a3180-7ac0-11eb-9694-4f4e8f80706b.png" height="300" alt="kill-plane-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109575585-189a3180-7ac0-11eb-932b-b507d763910f.png"><img src="https://user-images.githubusercontent.com/2163967/109575585-189a3180-7ac0-11eb-932b-b507d763910f.png" height="300" alt="kill-plane-2"></a>

Each room has a height value defined in metadata, below which players will be automatically killed. This is displayed in purple. Note that separately, area volumes can send a message to kill a player on contact.

### Coordinate Limits

* Index 15 (Player)
* Index 16 (Camera)

<a href="https://user-images.githubusercontent.com/2163967/141671962-b304dabc-2576-4e0c-8dc0-892ce2b4fdc6.png"><img src="https://user-images.githubusercontent.com/2163967/141671962-b304dabc-2576-4e0c-8dc0-892ce2b4fdc6.png" height="300" alt="coord-limit-1"></a> <a href="https://user-images.githubusercontent.com/2163967/141671959-54748e08-4f56-4fb0-ae71-42982405da53.png"><img src="https://user-images.githubusercontent.com/2163967/141671959-54748e08-4f56-4fb0-ae71-42982405da53.png" height="300" alt="coord-limit-2"></a>

Multiplayer rooms have defined coordinate minimums and maximums beyond which the player or free camera can't move. This creates a bounding box for habitable space in the room. If a player falls below the minimum Y coordinate, they are automatically killed. If the player or camera attempts to move past any of the other limits, they are instantly clamped back to them. The player bounding box is visualized in red, and the camera bounding box in pink.

### Room Node Bounds

* Index 17

<a href="https://user-images.githubusercontent.com/2163967/150604732-3136e8ed-83d4-4126-8d6f-78c1d1597d8f.png"><img src="https://user-images.githubusercontent.com/2163967/150604732-3136e8ed-83d4-4126-8d6f-78c1d1597d8f.png" height="300" alt="node-bounds-1"></a>

Room model nodes are culled (left out of the drawing process) based on the position of the camera and whether it is looking at a certain axis-aligned bounding box around each node. These volumes are displayed in red for the node which is currently selected. See the section below for information about node selection.

### Room Portals

* Index 18

<a href="https://user-images.githubusercontent.com/2163967/109575734-6747cb80-7ac0-11eb-925b-ab101c51db79.png"><img src="https://user-images.githubusercontent.com/2163967/109575734-6747cb80-7ac0-11eb-925b-ab101c51db79.png" height="300" alt="portal-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109575737-6878f880-7ac0-11eb-8433-238824782547.png"><img src="https://user-images.githubusercontent.com/2163967/109575737-6878f880-7ac0-11eb-8433-238824782547.png" height="300" alt="portal-2"></a>

Portals are planes which are used to control the partial rendering of rooms when the camera crosses them. Some are normally rendered as the hexagonal force fields that become more transparent the closer to player is to them, and some are invisible. When display is enabled, the force field portals will be rendered in purple, and the invisible portals will be rendered in green.

## Collision

Use `Alt+K` to enter collision display mode. A menu will be printed in the console showing the various display options. Use `K` to cycle through the options and `J` to update the value of the selected option.

It may be helpful to use entity selection to toggle visibility of a room/entity in order to see collision clearly. Additionally, `Alt+Q` can be used to cycle displaying collision faces and edges, edges only, and faces only.

### Color

The color option allows choosing how to color the collision display. By default, all collision is red. The option can be cycled to color by entity type, by terrain type, or by entity interaction.

<a href="https://user-images.githubusercontent.com/2163967/109593740-89047b00-7adf-11eb-9192-66073520ad01.png"><img src="https://user-images.githubusercontent.com/2163967/109593740-89047b00-7adf-11eb-9192-66073520ad01.png" height="300" alt="col-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109593743-89047b00-7adf-11eb-9fed-91cce923199b.png"><img src="https://user-images.githubusercontent.com/2163967/109593743-89047b00-7adf-11eb-9fed-91cce923199b.png" height="300" alt="col-2"></a>

The opacity setting allows toggling between semi-transparent and opaque display of collision faces.

### Collision Types

#### Entity

Cycle between displaying collision for the room, platforms, objects, or all three.

<a href="https://user-images.githubusercontent.com/2163967/109583164-6e290b00-7acd-11eb-9238-60de58e67e2c.png"><img src="https://user-images.githubusercontent.com/2163967/109583164-6e290b00-7acd-11eb-9238-60de58e67e2c.png" height="300" alt="col-3"></a>

When coloring by entity type, room collision is yellow, platform collision is teal, and object collision is magenta.

#### Terrain

Cycle between displaying all terrain or only terrain of each type: metal, orange holo, green holo, blue holo, ice, snow, sand, rock, lava, acid, Gorea, unknown.

<a href="https://user-images.githubusercontent.com/2163967/109583291-ae888900-7acd-11eb-8d2c-730f59004561.png"><img src="https://user-images.githubusercontent.com/2163967/109583291-ae888900-7acd-11eb-8d2c-730f59004561.png" height="300" alt="col-4"></a> <a href="https://user-images.githubusercontent.com/2163967/109585273-32904000-7ad1-11eb-854d-851550083518.png"><img src="https://user-images.githubusercontent.com/2163967/109585273-32904000-7ad1-11eb-854d-851550083518.png" height="300" alt="col-5"></a>

When coloring by terrain type, metal is gray, orange holo is orange, green holo is green, blue holo is blue, ice is light blue, snow is white, sand is yellow, rock is brown, lava is salmon, acid is pink, and Gorea is purple. The unknown 12th terrain type is never used in game.

#### Interaction

Cycle between displaying collision which interacts with anything, players, beams, or both.

<a href="https://user-images.githubusercontent.com/2163967/109583900-a5e48280-7ace-11eb-970e-d2920bb94923.png"><img src="https://user-images.githubusercontent.com/2163967/109583900-a5e48280-7ace-11eb-970e-d2920bb94923.png" height="300" alt="col-6"></a>

When coloring by entity interaction, collision that interacts only with players is yellow, collision that interacts only with beams is green, and collision that interacts with both is purple.

## Selection

Enter selection mode with `M` and use `+` or `-` to cycle through items. Information about the current selection will be printed to the console. The camera position can also be displayed while in selection mode by enabling it with `Alt+C`. Otherwise, this feature is off by default to prevent excessive flickering in the console window as the camera moves.

See [[Viewer Controls|Viewer Controls#Selection]] for a full list of the available commands.

### Entity Selection

<a href="https://user-images.githubusercontent.com/2163967/109589403-44291600-7ad8-11eb-9a64-eede4b06001a.png"><img src="https://user-images.githubusercontent.com/2163967/109589403-44291600-7ad8-11eb-9a64-eede4b06001a.png" height="300" alt="sel-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109589402-43907f80-7ad8-11eb-9098-f2623b84e889.png"><img src="https://user-images.githubusercontent.com/2163967/109589402-43907f80-7ad8-11eb-9098-f2623b84e889.png" height="300" alt="sel-2"></a>

The first level of selection operates on entities. The first selection will always be the room (if any), followed by each entity in the order they were loaded. Selected entities will be highlighted in white. When an entity is selected, it can be manipulated by holding `Alt` and pressing the same keys used to move the camera.

Pressing `0` will toggle the entity's visibility. Pressing `Alt+0` will toggle the entity's active state; some entities may have gameplay logic controlled by this state. For example, activating a camera sequence entity will cause its associated cutscene to begin.

<a href="https://user-images.githubusercontent.com/2163967/109590056-5061a300-7ad9-11eb-8883-4a63ed29e4b9.png"><img src="https://user-images.githubusercontent.com/2163967/109590056-5061a300-7ad9-11eb-8883-4a63ed29e4b9.png" height="300" alt="sel-9"></a>

Press `X` to move the camera position to the selected entity's position. If an entity has a parent entity, that entity will be highlighted in red; if it has a child entity, that entity will be highlighted in blue. Press `Ctrl+X` to look at the parent entity if any, and press `Ctrl+Shift+X` to look at the child entity if any.

Note that in order to select invisible entities, invisible entity display must be toggled on.

### Model Selection

<a href="https://user-images.githubusercontent.com/2163967/109589405-44291600-7ad8-11eb-9c43-bf410c163da5.png"><img src="https://user-images.githubusercontent.com/2163967/109589405-44291600-7ad8-11eb-9c43-bf410c163da5.png" height="300" alt="sel-3"></a> <a href="https://user-images.githubusercontent.com/2163967/109589407-44c1ac80-7ad8-11eb-9045-57a025af5458.png"><img src="https://user-images.githubusercontent.com/2163967/109589407-44c1ac80-7ad8-11eb-9045-57a025af5458.png" height="300" alt="sel-4"></a>

When an entity is selected, press `M` to move one step down the selection hierarchy to the first model of the entity (a single entity can have one or more models). Selected models will be highlighted in pale yellow.

### Node Selection

<a href="https://user-images.githubusercontent.com/2163967/109589717-be599a80-7ad8-11eb-8e3f-f517e0c58281.png"><img src="https://user-images.githubusercontent.com/2163967/109589717-be599a80-7ad8-11eb-8e3f-f517e0c58281.png" height="300" alt="sel-5"></a> <a href="https://user-images.githubusercontent.com/2163967/109589718-bef23100-7ad8-11eb-8e67-d8e8f7a69d49.png"><img src="https://user-images.githubusercontent.com/2163967/109589718-bef23100-7ad8-11eb-8e67-d8e8f7a69d49.png" height="300" alt="sel-6"></a>

When a model is selected, press `M` to advance the selection hierarchy to the first node of the model. Selecting a node will highlight all the meshes belonging to that node in pale green.

### Mesh Selection

<a href="https://user-images.githubusercontent.com/2163967/109589838-f6f97400-7ad8-11eb-9210-77878df74fe0.png"><img src="https://user-images.githubusercontent.com/2163967/109589838-f6f97400-7ad8-11eb-9210-77878df74fe0.png" height="300" alt="sel-7"></a> <a href="https://user-images.githubusercontent.com/2163967/109589839-f6f97400-7ad8-11eb-80cb-4af6f4074036.png"><img src="https://user-images.githubusercontent.com/2163967/109589839-f6f97400-7ad8-11eb-80cb-4af6f4074036.png" height="300" alt="sel-8"></a>

When a mesh is selected, press `M` to advance the selection hierarchy to the first mesh belonging to the node. Selected meshes will be highlighted in pale pink.

## Miscellaneous

### Room Node Transforms

Room nodes can have transform values in the model file, but these values are not applied in game. Consequently, a few room nodes appear out of place, presumably due to their transform not being baked into the display list by mistake. `N` can be used to toggle room node transforms to see where the nodes were likely intended to appear.

<a href="https://user-images.githubusercontent.com/2163967/109591160-0aa5da00-7adb-11eb-8111-bc480758c95f.png"><img src="https://user-images.githubusercontent.com/2163967/109591160-0aa5da00-7adb-11eb-8111-bc480758c95f.png" height="300" alt="room-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109590806-9ec37180-7ada-11eb-9cab-448900d7d3bc.png"><img src="https://user-images.githubusercontent.com/2163967/109590806-9ec37180-7ada-11eb-9cab-448900d7d3bc.png" height="300" alt="room-2"></a>
<br>
This location is missing the geometry for some ice crystals. They appear underneath the map instead.
<br><br><a href="https://user-images.githubusercontent.com/2163967/109591159-0aa5da00-7adb-11eb-8ced-3f4d3f9d28c5.png"><img src="https://user-images.githubusercontent.com/2163967/109591159-0aa5da00-7adb-11eb-8ced-3f4d3f9d28c5.png" height="300" alt="room-3"></a> <a href="https://user-images.githubusercontent.com/2163967/109591161-0b3e7080-7adb-11eb-95ee-617f701dbe86.png"><img src="https://user-images.githubusercontent.com/2163967/109591161-0b3e7080-7adb-11eb-95ee-617f701dbe86.png" height="300" alt="room-4"></a>
<br>
Collision for the ice is always present. With room node transforms toggled on, the geometry appears where the collision is.

### Showing All Room Nodes

<a href="https://user-images.githubusercontent.com/2163967/109591510-a8010e00-7adb-11eb-95ac-d4d118bd012f.png"><img src="https://user-images.githubusercontent.com/2163967/109591510-a8010e00-7adb-11eb-95ac-d4d118bd012f.png" height="300" alt="node-1"></a> <a href="https://user-images.githubusercontent.com/2163967/109591512-a8010e00-7adb-11eb-8b76-e84e42968bb9.png"><img src="https://user-images.githubusercontent.com/2163967/109591512-a8010e00-7adb-11eb-8b76-e84e42968bb9.png" height="300" alt="node-2"></a>

Normally, rooms are drawn by starting from a few specific root nodes -- partial room nodes and portal nodes -- and rendering down the tree from there. This leaves some nodes in the room models out of the drawing process altogether. These nodes, never visible in game, can be enabled with `Alt+N`.