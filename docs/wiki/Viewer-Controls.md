Contents:

* [Camera](#Camera)
* [Features](#Features)
* [Selection](#Selection)

See also: [[Visualization Guide]]

### Camera

There are three camera modes: **pivot**, **roam**, and **player**. Pivot is the default when viewing individual models, roam is the default when viewing a room, and player is the default when a player entity is loaded into the room.

When the camera is in pivot or roam mode, by default, the player cannot be controlled with inputs. Use `Shift`+`P` to switch between the "all," "player only," and "camera only" input modes. "Player only" is effectively a locked camera.

Pivot camera controls:

| Control | Description |
:- | :-
Click & drag | Free rotate
`Up`, `Left`,<br>`Down`, `Right` | Rotate
Scroll up | Zoom in
Scroll down | Zoom out
Hold `Shift` | Fast movement
`R` | Reset camera
`P` | Switch camera mode

Roam camera controls:

| Control | Description |
:- | :-
`W`, `A`, `S`, `D` | Move forward, left, back, right
`Space`, `V` | Move up, down
Click & drag | Free look
`Up`, `Left`,<br>`Down`, `Right` | Look
Hold `Shift` | Fast movement
`R` | Reset camera
`P` | Switch camera mode

### Player Camera

When in player camera mode, the controls correspond to in-game controls. In the future, there will be more control options available as well as a button remapping feature.

| Control | Description |
:- | :-
`W`, `A`, `S`, `D` | Move forward, left, back, right
`Space` | Jump or Morph Ball boost
Left click | Shoot
Right click | Zoom (Imperialist)
`C` | Switch forms
`Q` | Bomb or alt form attack
`E` | Switch visors
`Q` (hold) | Use Scan Visor
Tab | Show scoreboard
Scroll down | Next affinity weapon
Scroll up | Previous affinity weapon
Middle click (hold) | Open weapon menu
`1` | Switch to Power Beam
`2` | Switch to Missiles
  | _(when Free Weapon Select is enabled:)_
`3` | Switch to Volt Driver
`4` | Switch to Battlehammer
`5` | Switch to Imperialist
`6` | Switch to Judicator
`7` | Switch to Magmaul
`8` | Switch to Shock Coil
`9` | Switch to Omega Cannon

When the weapon menu is open, you can select a weapon by placing the mouse cursor over it, then releasing the weapon menu button. The Free Weapon Select "switch to weapon" keybinds are a [cheat](https://github.com/NoneGiven/MphRead/wiki/Setup:-Gameplay-Options#features-cheats-and-bugfixes) option where using them to switch weapons will automatically make them available and restore the corresponding ammo.

### Features

Various viewer features can be accessed with key combinations.

#### System

| Control | Description |
:- | :-
`Shift`+`5` | Save screenshot
`Ctrl`+`Shift`+`R` | Begin/end frame dump
`Enter` | Toggle frame advance
`.` | Advance one frame
`Ctrl`+`O` | Initiate model load prompt
`Esc` | Exit the viewer

Screenshots and frame dumps will be saved to your export path. Frame dumps can be converted to videos using a tool like [ffmpeg](https://ffmpeg.org/).

Frame advance suspends all animation and allows advancing one frame at a time.

The model load prompt allows you to enter text in the console window. For loading, specify a model name followed by an optional recolor index after a space. To load a First Hunt model, enter another space followed by `-fh`.

#### Display options

| Control | Description |
:- | :-
`T` | Toggle textures
`Ctrl`+`C` | Toggle vertex colors
`Ctrl`+`Q` | Toggle wireframe
`Alt+Q` | Toggle collision edges
`B` | Toggle face culling
`F` | Toggle texture filtering
`L` | Toggle lighting
`G`  | Toggle fog
`Alt`+`G` | Toggle far clip distance
`N` | Toggle room node transforms
`Alt+N` | Toggle hidden nodes
`I` | Toggle invisible entities
`Y` | Toggle nodedata (AI pathing) display
`Shift`+`E` | Toggle Scan Visor entities
`Alt`+`P` | Advance active point modules
`Alt`+`K` | Toggle collision display

When enabled, the hidden nodes option will display nodes that are never seen in-game because of the way room rendering works. The invisible entities option has three settings: off, placeholder, and all. "Placeholder" enables placeholder models for entities with no models, while "all" will additionally reveal entities that have models but would not normally be drawn due to their current active state.

#### Volumes

| Control | Description |
:- | :-
`Z`, `Shift`+`Z` | Cycle volume display forward/back
`Alt`+`H` | Toggle hiding of volumes that don't belong to the currently selected entity

Volume display will cycle through the following options (starting with "None" by default):

* Light sources (color 1)
* Light sources (color 2)
* Trigger volumes (parent event)
* Trigger volumes (child event)
* Area volumes (inside event)
* Area volumes (exit event)
* Morph cameras
* Jump pads
* Teleporters
* Enemy hit zones
* Objects
* Flag bases
* Defense nodes
* Room kill plane
* Player coordinate limits
* Camera coordinate limits
* Room node bounds
* Portals

### Selection

There are four selection modes in a hierarchy: entity, model, node, and mesh. `M` will cycle from no selection (the default) through each of these modes in turn. Information about the selection will be displayed in the console, and various commands are available to manipulate the selected item.

See the relevant section of the [[Visualization Guide|Visualization Guide#Selection]] for more information about the mode.

Common selection controls:

| Control | Description |
:- | :-
`M`, `Shift`+`M` | Cycle selection mode forward/back
`Ctrl`+`Shift`+`M` | Clear selection
`H` | Toggle selection highlighting
`+` | Select next item
`-` | Select previous item
`0` | Toggle visibility of selection
`X` | Look at selection
`Ctrl`+`U` | Unload selected entity

Selection highlighting (enabled by default) will make the selected item flash white (entity), yellow (model), green (node), or pink (mesh).

Entity selection controls:

| Control | Description |
:- | :-
`Ctrl`+`X` | Look at parent entity
`Ctrl`+`Shift`+`X` | Look at child entity
`2` | Show next recolor
`1` | Show previous recolor
(Hold `Alt`)+ | 
`W`, `A`, `S`, `D` | Move forward, left, back, right
`Space`, `V` | Move up, down
`Up`, `Left`,<br>`Down`, `Right` | Rotate

If selection highlighting is enabled, the selected entity's parent will be highlighted in red and its child will be highlighted in blue.

Model selection controls:

| Control | Description |
:- | :-
`Alt`+`+` | Select next node animation
`Alt`+`-` | Select previous node animation
`Ctrl`+`Alt`+`+` | Select next material animation
`Ctrl`+`Alt`+`-` | Select previous material animation

Node selection controls:

| Control | Description |
:- | :-
`Shift`+`+` | Select next room node
`Shift`+`-` | Select previous room node

These controls only apply if the selected node belongs to a room model.

### Collision

The controls for navigating the collision menu are displayed on the screen when collision view is enabled. See the [collision section of the visualization guide](https://github.com/NoneGiven/MphRead/wiki/Visualization-Guide#Collision) for an explanation of these options.