Messages (or events) are passed between entities. Messages can either be dispatched on the frame they are created, or queued to be dispatched a certain number of frames later. The message queue has a capacity of 40 elements. When a message is dispatched, it is processed by the target entity's `handle_message` function. Some functions also iterate the queue to search for messages of certain types.
## Message struct

* Size: 0x24 (36)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Type | Type ID
0x04 | 4 | uint | Unused4 | Appears to be an unused byte field with 3 bytes of padding
0x08 | 4 | uint | Sender | Pointer to sending entity
0x0C | 4 | uint | Target | Pointer to target entity
0x10 | 4 | uint | Param1 | Message-specific data
0x14 | 4 | uint | Param2 | Message-specific data
0x18 | 4 | uint | DispatchFrame | Frame to dispatch on
0x1C | 4 | uint | CreatedFrame | Frame created on
0x20 | 4 | uint | Processed | Boolean value for processed state

The `Processed` field is automatically set to 0 when a message is a queued, and set to 1 when it is dispatched from the queue. If the message is dispatched immediately, the field is not used. Message creation involves passing the type (and optionally, frame count) parameters along with a smaller struct containing only the `Sender`, `Target`, `Param1`, and `Param2` fields.

## Message types

| ID | Type | Notes |
:- | :- | :-
0 | [None](#None) | No event | 
5 | [SetActive](#SetActive) | Activates or deactivates the target
6 | [Destroyed](#Destroyed) | Inform a parent or other entity that the sender was destroyed
7 | Damage | Inflict repeating damage to the player
9 | Trigger | Sent to trigger volumes of subtype 1, decrements their counter to activation
12 | UpdateMusic | 
15 | Gravity | Update player's gravity value each frame
16 | Unlock | Unlock a door or force field
17 | Lock | Lock a door or force field
18 | [Activate](#Activate) | Activates the target
19 | Complete | Force a multiplayer match to end
20 | Impact | Sent when a beam or bomb hits something or detonates
21 | Death | Kill the player
22 | Unused22 | Referenced by code in area volumes, but never sent or checked
23 | ShipHatch | Display the ship entry prompt
24 | Unused24 | Sent by some alt form movement code, but never checked
25 | Unused25 | Sent by an area volume in the test level, but never checked
26 | [ShowPrompt](#ShowPrompt) | Show game message with "ok" or "yes/no" prompt
27 | [ShowWarning](#ShowWarning) | Show game message overlay with orange palette
28 | [ShowOverlay](#ShowOverlay) | Show game message overlay with normal palette
29 | MoveItemSpawner | Used by enemy hunters to move item/artifact spawner to where they died
30 | SetCamSeqAi | Initializes bot AI (e.g. goes from standing there in cutscene to attacking)
31 | PlayerCollideWith | Sent to entities when player collides with them
32 | BeamCollideWith | Sent to entities when beam collides with them
33 | UnlockConnectors | Locks the doors created on the other side of connectors
34 | LockConnectors | Unlocks the doors created on the other side of connectors
35 | PreventFormSwitch | Do not allow switching forms while player is inside
36 |  | Updates Gorea's phase 2 spawn location?
42 | SetTriggerState | Set global entity state used for ship teleporters
43 | ClearTriggerState | Functional, but unused
44 | PlatformWakeup | Sent to platform to start its motion
45 | PlatformSleep | Sent to platform to stop its motion
46 | DripMoatPlatform | Update whether the player is locked into the Drip Moat platform
48 |  | 48-51 are messages sent by Slench synpases to Slench turrets
49 |  | 
50 |  | 
51 |  | 
52 | SetBeamReflection | Functional, but unused
53 | SetPlatformIndex | Change the current index of the platform's motion routine
54 | PlaySfxScript | Play the SFX script specified by the ID
56 | UnlockOubliette | Check story state and potentially unlock Oubliette
57 | Checkpoint | Sets the sender as the location for a player to respawn in 1P
58 | EscapeUpdate1 | Start an escape (or Fuel Stack) sequence with a specified duration, or pause or cancel it
59 | SetSeekPlayerY | Update the platform's player height-seeking behavior
60 | LoadOubliette | 
61 | EscapeUpdate2 | Same as EscapeUpdate1, but the duration parameter is in frames instead of seconds

## None

Type: 0

| | Type | Description
:- | :- | :-
Param1 | --- | Unused
Param2 | --- | Unused

This message type is used in entity metadata to indicate that no message should be created. There are no entities which attempt to handle `None` messages.

## SetActive

Type: 5

| | Type | Description
:- | :- | :-
Param1 | bool | Target should activate if `true`, deactivate if `false`
Param2 |  | 

Instructs the target entity to change its active state.

## Destroyed

Type: 6

| | Type | Description
:- | :- | :-
Param1 |  | 
Param2 |  | Unused?

Generated when an entity is destroyed/despawned, often by returning 0 from its process function.

## Activate

Type: 18

| | Type | Description
:- | :- | :-
Param1 |  | Unused?
Param2 |  | Unused?

Instructs the target entity to activate. Entities should do the same thing when receiving this message as when receiving a `SetActive` message with `Param1` set to `true`.

## ShowPrompt

Type: 26

| | Type | Description
:- | :- | :-
Param1 | uint | Game message ID
Param2 | bool | Shows a "yes/no" prompt if `true`, "ok" prompt if `false`

Shows a dialog prompt with the specified game message ID. The prefix character of the game message is used to determine the message's appearance and sound effects.

## ShowWarning

Type: 27

| | Type | Description
:- | :- | :-
Param1 | uint | Game message ID
Param2 | uint | Duration in frames

Shows a dialog overlay with the specified game message ID using the warning color palette. The prefix character of the game message is ignored. If `Param2` is 0, it is set to a default value of 15.

## ShowOverlay

Type: 28

| | Type | Description
:- | :- | :-
Param1 | uint | Game message ID
Param2 | uint | Duration in frames

Shows a dialog overlay with the specified game message ID using the normal color palette. The prefix character of the game message is ignored.