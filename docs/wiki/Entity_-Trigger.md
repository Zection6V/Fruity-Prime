A trigger is an invisible [[entity|Entities]] which sends [[messages|Entity Messaging]] to other entities. Different [subtypes](#Trigger-subtypes) have different rules for activation. Triggers are often used with the volume subtype in order to initiate a room event when the player (or sometimes player beams) enters a certain area; this usage is similar to [[area volumes|Entity: AreaVolume]]. Unlike area volumes, trigger volumes usually activate only once when entered, and do not activate again until the player exits and re-enters (if they are not one-use). [[Trigger flags|Entity: AreaVolume#flags]] determine how the volume checks for collision with other entities.

When triggered, triggers will send one message to their parent entity if any, and another to their child entity if any. Relay subtype triggers are often chained together, e.g. with a volume subtype initiating the message, and then a child relay passing the message first to their parent entity (some room entity listening for the message) and then to their child (another relay, which itself has a relevant parent entity, and potentially another trigger as its child).

Entity type: 7

See also: Visualization Guide ([[Part 1|Visualization Guide#trigger]], [[Part 2|Visualization Guide#trigger-volumes]])

### File struct

- Size: 0x9C (156)

| Offset | Size | Type | Name | Description |
:-- | :-- | :-- | :-- | :--
0x00 | 40 | EntityDataHeader | Header | 
0x28 | 64 | CollisionVolume | Volume | Effective area, if any
0x68 | 2 | --- | Unused68 | 
0x6A | 1 | byte | Active | Boolean
0x6B | 1 | byte | AlwaysActive | Boolean
0x6C | 1 | byte | DeactivateAfterUse | Boolean
0x6D | 1 | --- | --- | Padding
0x6E | 2 | ushort | RepeatDelay | 
0x70 | 2 | ushort | TriggerDelay | 
0x72 | 2 | ushort | GlobalStateBits | 
0x74 | 4 | uint | TriggerFlags | See <a href="https://github.com/NoneGiven/MphRead/wiki/Entity:-AreaVolume#flags">here</a> for info
0x78 | 4 | uint | Threshold | 
0x7C | 2 | short | ParentId | Entity ID or -1
0x7E | 2 | --- | --- | Padding
0x80 | 4 | uint | ParentMessageType | Message type ID to send to parent
0x84 | 4 | uint | ParentMessageParam1 | Message-specific data
0x88 | 4 | uint | ParentMessageParam2 | Message-specific data
0x8C | 2 | short | ChildId | Entity ID or -1
0x8E | 2 | --- | --- | Padding
0x90 | 4 | uint | ChildMessageType | Message type ID to send to child
0x94 | 4 | uint | ChildMessageParam1 | Message-specific data
0x98 | 4 | uint | ChildMessageParam2 | Message-specific data

### Runtime class

- Size: 0x78 (120)

| Offset | Size | Type | Name | Description |
:-- | :-- | :-- | :-- | :--
0x00 | 24 | CEntity | Entity | Runtime entity base class
0x18 | 1 | byte | Flags | 
0x19 | 1 | byte | Subtype | 
0x1A | 2 | ushort | TriggerDelay | 
0x1C | 2 | ushort | GlobalStateBits | 
0x1E | 2 | --- | --- | Padding
0x20 | 4 | uint | Threshold | 
0x24 | 4 | uint | TriggerCount | 
0x28 | 4 | uint | TriggerFlags | Same as file struct
0x2C | 4 | ptr | Data | Pointer to file struct
0x30 | 4 | uint | Parent | Pointer to parent entity (Initially set to `ParentId`)
0x34 | 4 | uint | Child | Pointer to child entity (Initially set to `ChildId`)
0x38 | 64 | CollisionVolume | Volume | Current effective area, if any

### Trigger subtypes

| ID | Type | Description |
:-- | :-- | :--
0 | Volume | Triggered by player or beam collision with the volume
1 | Threshold | Triggered after a number of messages received
2 | Relay | Can't be triggered; passes received messages to parent/child
3 | Automatic | Triggers automatically (after delay, if any)
4 | State | Triggers based on global state bits

#### Examples

* **Volume**: A volume that checks for collision with the player, and sends one message to lock doors and another to activate a camera sequence.
* **Threshold**: A trigger which receives messages each time an enemy in the room is destroyed, and unlocks a door after 5 are destroyed.
* **Relay**: A series of relays which receive the message to deactivate force fields, each passing the message to one of several force field entities.
* **Automatic**: A trigger which starts video playback for the boss intro movie after the boss's room loads.
* **State**: A trigger which activates a teleporter in the room only if the area's boss has been defeated.
