Effects are the way MPH draws particle systems, bullet decals, some beam graphics, and various other things. Effect data is found in `*_PS.bin` files in the `effects` directory as well as in the `effects` archive. Related model files are found in the `effectsBase` archive and the `models` folder. See [[Effect IDs]] for more information.

## PS File Format

The `PS` files each contain one effect definition, where an effect is defined as a collection of effect elements, with function metadata and parameter lists.

### Header

* Size: 0x24 (28)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | Unused | Always has a value of `0xCCCCCCCC`
0x04 | 4 | uint | Function Count | Number of function metadata blocks
0x08 | 4 | uint | Function Offset | Offset to function metadata offset list
0x0C | 4 | uint | Unused Count | Also a number of functions, but not used
0x10 | 4 | uint | Unused Offset | Also an offset to functions, but not used
0x14 | 4 | uint | Element Count | Number of effect elements
0x18 | 4 | uint | Element Offset | Offset to effect element offset list

The unused function count and offsets are processed by the game when loading the file, but they are never used for anything after that.

### Effect Functions

The function offset at `0x08` of the header points to a list of offsets with length equal to the count at `0x04`. Each of these offsets points to a function metadata block, but not strictly to the start of the data. The blocks have the following format:

```
param 0 value
param 1 value
param 2 value
...
param n value
func ID
param offset
```

The offsets in the aforementioned list point to the func ID. All fields are 4 byte `uint`.

To locate the parameters, add 4 to the func ID offset to reach the location of the param offset value. If this value is 0, the function has no parameter list. If the value is non-zero, it is an offset to the param 0 value. To determine the number of parameters, subtract the param offset from the func ID offset and divide by 4.

Example from `blastCapHit_PS.bin`:

```
 0x04: 0D 00 00 00 (13 functions)
 0x08: 24 02 00 00 (offsets at 0x224)

 0xA8: 00 00 00 00 (param 0 = 0)
 0xAC: 00 40 00 00 (param 1 = 0x4000)
 0xB0: 00 00 00 00 (param 2 = 0)
 0xB4: 28 00 00 00 (func ID 40)
 0xB8: A8 00 00 00 (param offset 0xA8)

0x224: B4 00 00 00 (func 0 ID at 0xB4)
0x228: BC 00 00 00 (func 1 ID at 0xBC)
0x22C: DC 00 00 00 (func 2 ID at 0xDC)
...
```

### Elements

The element offset at `0x18` of the header points to a list of offsets with length equal to the count at `0x14`. Each of these offsets points to an effect element struct.

* Size: 0x74 (132)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 32 | char[] | Name | Name of the effect element
0x20 | 32 | char[] | Model Name | Name of the model for this element's particle graphics
0x40 | 4 | uint | Particle Count | Number of particle definitions
0x44 | 4 | uint | Particle Offset | Offset to list of particle name offsets
0x48 | 4 | uint | Flags | Effect element flags
0x4C | 12 | vec3 | Acceleration | Acceleration value for spawned particles
0x58 | 4 | uint | Child Effect ID | Effect spawned when a particle expires, or 0 if none
0x5C | 4 | fx32 | Lifespan | Length of time over which the effect will spawn particles
0x60 | 4 | fx32 | Drain Time | Extra decrease value for lifespan
0x64 | 4 | fx32 | Buffer Time | Extra increase value for lifespan
0x68 | 4 | uint | Draw Type | ID for method by which particles will be drawn
0x6C | 4 | uint | Function Count | Number of functions this element uses
0x70 | 4 | uint | Function Offset | Offset to list of element function metadata

* Effect element flags

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Use transform | Particles will update with the element's transform (e.g. rotation) each frame
1 | 0x2 | Use acceleration | The element acceleration value will be added to particle speed each frame
2 | 0x4 | Use mesh | Particles will draw a mesh from their model rather than a flat texture
3 | 0x8 | Unit spawn vecs | Unit X/Y vecs will be used instead of the spawn vectors passed to the element
4 | 0x10 | Keep alive | While set, an expired element will stay alive until its particles have all expired
5 | 0x20 | Extend particles | Use drain/buffer times to extend particle lifespans
6 | 0x40 | Check collision | Expire/destroy particle if it collides while moving
7 | 0x80 | Spawn child | Spawn child effect on particle expiration
8 | 0x100 | Destroy on detach | When set, detaching an effect element entry will destroy it immediately
19 | 0x80000 | Extend element | Use drain/buffer times to extend element lifespan
20 | 0x100000 | Draw enabled | Draw if set, skip drawing if cleared at runtime

### Element Functions

The function offset at `0x70` of the element struct points to a list of integer pairs with length equal to the count at `0x6C`. Each pair consists of an action ID and a function metadata offset. The offset follows the same rules as described above in [Effect Functions](#Effect-Functions), pointing to the function ID field of a function metadata block.

| ID | Action |
:- | :-
9 | Set particle definition ID
14 | Increase particle amount
15 | Set new particle speed
16 | Set new particle position
17 | Set new particle lifespan
18 | Update particle speed
19 | Set particle alpha
20 | Set particle red
21 | Set particle green
22 | Set particle blue
23 | Set particle scale
24 | Set particle rotation
25 | Set particle read-only field 1
26 | Set particle read-only field 2
27 | Set particle read-only field 3
28 | Set particle read-only field 4
29 | Set particle read/write field 1
30 | Set particle read/write field 2
31 | Set particle read/write field 3
32 | Set particle read/write field 4

### Particle Definitions

The particle name offset at `0x44` of the element struct points to a list of offsets with length equal to the count at `0x40`. Each of these offsets points to a name string with a maximum length of 16 characters, each of which represents a particle definition that the element will be able to spawn.

A particle definition is parsed by loading the model referred to by the element's model name string at `0x20` and finding the node with the same name as the particle name. The particle definition then contains a reference to the node and the material ID of the mesh associated with that node.

## Spawning & Management

Effect, element, and particle instances are spawned and managed in different ways. Each of these parts of an effect has an associated "entry" type which are used to keep track of and process spawned instances.

### Effect Element Entry

When an effect is spawned, each of its elements gets an entry in a linked list of effect elements entries. This is the primary list which the game iterates in order to process and draw effects. Each entry has its own transform, timers, and state, as well as a reference to a linked list of the particles which it has spawned.

### Effect Particle Entry

When an effect element spawns a particle, an entry is placed into a linked list to keep track of its timers, position, and other state. When each element is processed, it iterates its own list of particles in order to process and then draw them.

### Effect Entry

When an entity spawns an effect and wants to keep a reference to it, it requests an effect entry. The entry holds a pointer to the list of effect element entries belonging to the effect. The entity can use the effect entry in order to update the element transforms when the entity moves, or to destroy the effect when needed. Each element entry also holds a reference to its parent effect entry if any, and may change its behavior when it's present (such as not destroying itself on expiration, so the entity can decide what to do with it).

### Single Particle Entry

Separately from the effect system, any entity can specify a standalone particle to be drawn on that frame. These are maintained in a linked list for the duration of the frame, and then cleared after being drawn. They hold a position, a color, and a particle definition reference.

## Processing

Effect processing is done for every effect element entry on every frame. Note that time amounts in effect processing use fixed-point elapsed time values in seconds rather than frame counts, which allows most processing to work independent of the frame rate. However, there is some logic that relies on one-time events happening "this frame" or "next frame."

The order of operations for processing is as follows:

* For each element entry:
  * If the element expiration time has been reached, and nothing is blocking destruction, destroy the element
  * Else, if particle spawning is still enabled, increase particle amount
  * For each particle to be spawned:
    * Create a particle entry and add it to the element's list
    * Call functions to set the new particle's properties
  * For each particle entry in the list:
    * If the particle expiration time has been reached, and nothing is blocking destruction, destroy the particle
    * If the particle is destroyed, and child effect spawning is enabled, spawn the child effect
    * Else, call functions to update the particle's properties

### Element Lifespan

#### Expiration

When an effect is spawned, each element entry records its spawn time as the game's global elapsed time value, and sets an expiration time equal to the spawn time plus the lifespan value. When an element is processed, it checks the current global elapsed time against its expiration time to see if it has been reached. When an element reaches its expiration time, by default it is destroyed. This means detaching its list of particle entries and removing the element entry from the global list, so the element is no longer processed and its particles are no longer processed or drawn.

An element which is spawned during effect processing (that is, a child effect) has its creation time increased by an elapsed time value equivalent to one frame.

#### Keep Alive

There are two factors that can prevent element destruction upon expiration: element flags bit 4 (keep alive) or the presence of an effect entry. In both cases, the expired element is still processed and its particles are still processed and drawn, but it cannot spawn any new particles. The keep alive flag will allow the element to stay alive until all its spawned particles have expired, at which point it will be destroyed. The presence of an effect entry will keep the element alive indefinitely, until the entity holding the effect entry reference decides to destroy it.

#### Buffer/Drain Time

If an element is not expired, flags bit 19 (extend element) may extend its expiration time. When this flag is set, if the time since creation exceeds the element's buffer time value,  the element's creation and expiration times are both increased by the buffer time value and decreased by the drain time value. This essentially renews the element each time its age reaches the buffer time value, resetting its timers as if it has just been created, but also shortening its renewed lifespan by the drain time value each time this happens.

The purpose of keeping the element alive by resetting the timers, rather than just having a longer lifespan to begin with, is to enable a looping cycle of state values, as many effect functions return values based on the current percentage of the element's or particle's lifespan which has elapsed.

### Element Transform

By default, an effect element entry keeps the position and rotation it was spawned with. If an entity spawned the effect and obtained an effect entry, it can manually update each element's transforms to match its own. Additionally, a matrix pointer can be passed when spawning the effect, in which case the element will use it during the processing step to multiply its transform by the matrix at the pointer's destination. This only occurs if the element is non-expired.

### Particle Spawning

#### Amount Field

A non-expired element will call the function assigned to action 14 (increase particle amount) in order to increase its particle amount field. After the increase, a number of particles equal to the whole number part of the value will be spawned, and the value will be decreased by that amount. For example, if the value after increase is `2.5`, two particles will be spawned, and the value will be reduced to `0.5`.

#### Entry Creation

New particles are initially created with a particle definition index of 0. Particles are also given a portion value indicating the percentage of this element's particles spawned so far this frame. For example, if an element is spawning four particles, the first will have a portion of `0.0`, the second will have `0.25`, the third `0.5`, and the fourth `0.75`. This value may be used by function calls during spawning and/or processing.

#### Transform

If a function is assigned to action 16 (set new particle position), it is called to set the initial position of the particle. Otherwise, the X/Y/Z position values are all set to zero.

If a function is assigned to action 15 (set new particle speed), it is called to set the initial speed of the particle. Otherwise, the X/Y/Z speed values are all set to zero.

If element flags bit 0 (use transform) is cleared, the new particle's position and speed vectors are multiplied by the element's transform matrix. If the flag is set, the element transform will be accounted for later in particle drawing instead of in this one-time spawn usage.

#### Read-Only Fields

If functions are assigned to actions 25 (set particle read-only field 1), 26 (set particle read-only field 2), 27 (set particle read-only field 3), or 28 (set particle read-only field 4), they are called to set the corresponding field of the particle. Otherwise, each field is set to the corresponding field of the element entry.

#### Lifespan

Like elements, particles are assigned the current global elapsed time as their creation time value. If a function is assigned to action 17 (set new particle lifespan), it is called to set the lifespan, and the particle's expiration time is based on that value. Otherwise, the lifespan is set to the element's lifespan value, and the particle's expiration time is based on that.

Note: In the case where a function is called here, the element's lifespan value is temporarily treated as `1.0` during the function call instead of its actual value.

#### One-Time Set Functions

For several fields of the new particle, the function assigned to the relevant action is called on spawn if that function is a constant return function (func ID 4 for vectors and func ID 42 for integers). The function is then unassigned from the action so it is not called again during particle processing. See [Functions/Actions](#FunctionsActions) below for more information about function types.

If there is no function assigned, or the function assigned is not a constant return function, a default value is set for some fields.

| ID | Action | Type | Default
:- | :- | :- | :-
18 | Update particle speed | Vector | N/A
20 | Set particle red | Integer | `1.0`
21 | Set particle green | Integer | `1.0`
22 | Set particle blue | Integer | `1.0`
19 | Set particle alpha | Integer | `1.0`
23 | Set particle scale | Integer | `0.0`
24 | Set particle rotation | Integer | N/A
29 | Set particle read/write field 1 | Integer | N/A
30 | Set particle read/write field 2 | Integer | N/A
31 | Set particle read/write field 3 | Integer | N/A
32 | Set particle read/write field 4 | Integer | N/A

### Particle Processing

#### Lifespan

Particles follow similar lifespan rules to elements, checking the global elapsed time against their expiration time field. A particle that reaches its expiration time is always destroyed and removed from processing/drawing. Non-expired particles can have their lifespans renewed using the [Buffer/Drain Time](#BufferDrain-Time) values of the element. In order for this to apply to particles, both the element flags bits 19 (extend element) and 5 (extend particle) must be set.

#### Processing Functions

If a function is assigned to one of various actions, it is called every frame during processing to set the corresponding field of the particle. If no function is assigned, the field is not updated during processing.

| ID | Action |
:- | :-
29 | Set particle read/write field 1
30 | Set particle read/write field 2
31 | Set particle read/write field 3
32 | Set particle read/write field 4
9 | Set particle definition ID
18 | Update particle speed
20 | Set particle red
21 | Set particle green
22 | Set particle blue
19 | Set particle alpha
23 | Set particle scale
24 | Set particle rotation

Notes:
* The particle definition ID returned from the function call is truncated from fixed-point to a whole number
* If the particle definition ID is exceeds the element's particle definition count, it is set to the highest valid index
* If the alpha value is less than zero after the function call, it is set to zero

#### Movement

If element flags bit 1 (use acceleration) is set, the element's acceleration value is added to the particle's speed field. The particle's position is updated by adding its speed, scaled with the elapsed time for a single frame. If element flags bit 6 (check collision) is set, collision is checked between the particle's current and intended next positions. If collision is found, the intended next position is changed to the point of collision, and the particle's expiration time is updated so it will be considered expired on the next frame.

#### Child Effect Spawning

If the particle is expired and about to be destroyed, and element flags bit 7 (spawn child effect) is set, the element's child effect is spawned. The spawn position of the new effect is the particle's current position, and the up vector is the negation of the particle's current speed.

In practice, this is only used by one effect, `hangingDrip`, which has one particle that moves downward and spawns its child effect, `hangingSplash`, upon collision with the ground.

## Drawing

### Functions

Each element sets a pointer to a "set vectors" function and a "draw" function based on its draw type and some flags bits.

* If element flags bit 2 (use mesh) is set:
  * Use draw func `DC`
  * If draw type is 3:
    * Use vecs func `D4`
  * Else:
    * Use vecs func `D8`

* Else, if bit 2 is cleared:

<table><tbody>
<tr><th>Draw Type</th><th>Vecs Func</th><th colspan="2">Draw Func</th></tr>
<tr><th></th><th></th><th>Bit 0 set</th><th>Bit 0 cleared</th></tr>
<tr><td>1</td><td><code>B0</code></td><td><code>B4</code></td><td><code>B8</code></td></tr>
<tr><td>2</td><td><code>BC</code></td><td><code>B4</code></td><td><code>B8</code></td></tr>
<tr><td>3</td><td><code>C0</code></td><td colspan="2"><code>C4</code></td></tr>
<tr><td>4</td><td><code>B0</code></td><td><code>C8</code></td><td><code>CC</code></td></tr>
<tr><td>5</td><td><code>BC</code></td><td><code>C8</code></td><td><code>CC</code></td></tr>
<tr><td>6</td><td><code>C0</code></td><td colspan="2"><code>D0</code></td></tr>
</tbody></table>

Vecs:

* `B0`: Sets vectors for ordinary spherical billboard rendering of a flat quad.
* `BC`: Sets vectors for rendering a flat quad which always faces the unit Y direction.
* `C0`: Sets vectors for spherical billboard rendering of a flat quad, where the facing direction is perpendicular to the camera direction instead of parallel.
* `D4`: Sets vectors for non-billboard rendering of a model node. Not used by any effects in the game.
* `D8`: Sets vectors for spherical billboard rendering of a model node.

Draw:

* `B8`/`B4`: Draw quad with/without accounting for the element transform.
* `CC`/`C8`: Draw quad with/without accounting for the element transform, accounting for particle rotation.
* `C4`/`D0`: Draw quad accounting for particle rotation, if the particle's speed is non-zero.
* `DC`: Draw the node associated with the particle definition.

With the exception of draw func `DC`, particles are drawn by computing the four vertices of a quad and their associated UVs, and rendering the quad using the material ID from the particle definition. Which particle definition to draw for a given particle entry depends on the particle definition ID, which is set to 0 on spawn and can be updated by a function call during processing.

If element flags bit 0 (use transform) is set, the element transform is multiplied with the modelview transform matrix during the draw step each frame. This flag can also cause a different draw func pointer to be set, which accounts for the different ways the element transform is applied (once on spawn if the flag is cleared, or every frame if the flag is set).

## Functions/Actions

When a function assigned to an action is called, it can do one or more of the following: return a constant, return a value from its parameter list, return a field of the currently processed effect element entry (and/or effect particle entry if the function is being called for a particle), or calculate a value using all of the above. The most common values used in calculation are the lifespan of the element/particle, especially its current age as percentage of its total lifespan.

Functions also often use parameters as offsets (converted to pointers at runtime) into the function metadata blocks to call other functions. For example, func ID 17 calls the functions pointed to by its parameters 0 and 1 to get two vectors, adds the vectors together, and returns the result.

Normally, the parameters for the "child" function invocation work the same way as the outer function: the metadata block contains an offset (pointer at runtime) to a function, and that field is immediately followed by the offset to that function's parameters; the "parent" function's parameter is the offset to the former location, so it can simply add 4 to the offset/pointer and pass that value to the function it invokes. However, function 13 works differently when it calls its "child" function: it ignores the parameter offset found after the "child" function offset, and instead uses its own second parameter as an offset into the function metadata blocks to locate the parameter list.

There are two types of functions: vector functions which take a pointer to a `vec3` as input and modify it, and integer functions which return a fixed-point integer value. For vector functions, function 04 is considered a constant return function because the values it sets on the vector come directly from its parameter list, with no calls to other functions or calculations using the element/particle fields. Likewise, function 42 is the constant return integer function.

If a function accesses a field of the currently processed particle, then in practice it is only called for particles and not elements. If such a function was called for an element, it would access stale values from the previously processed or drawn particle.

### Func ID mapping

Functions 00 through 20 are vector functions, while functions 21 through 49 are integer functions. Most func IDs map to a specific function, but there are some gaps and overlap:

* IDs 0 and 12 map to empty functions
* IDs 1 and 2 map to the same function
* IDs 21 and 28 map to the same function