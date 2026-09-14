Memory locations are for the EUR title (AMHP) and apply to player 1 (or single-player mode). Players 1-4 are contiguous in memory, so the size of the player struct (0xF30) can be used as an offset to access them.

## Player

| Offset | RAM Location | Size | Type | Description |
:- | :- | :- | :- | :-
0x00 | 0x020DB034 | 2 | ushort | Entity type
0x1C | 0x020DB050 | 4 | fx32 | X position
0x20 | 0x020DB054 | 4 | fx32 | Y position
0x24 | 0x020DB058 | 4 | fx32 | Z position
0x28 | 0x020DB050 | 4 | fx32 | Previous X position
0x2C | 0x020DB054 | 4 | fx32 | Previous Y position
0x30 | 0x020DB058 | 4 | fx32 | Previous Z position
0x34 | 0x020DB068 | 4 | fx32 | X speed
0x38 | 0x020DB06C | 4 | fx32 | Y speed
0x3C | 0x020DB070 | 4 | fx32 | Z speed
0x40 | 0x020DB074 | 4 | fx32 | Previous X speed
0x44 | 0x020DB078 | 4 | fx32 | Previous Y speed
0x48 | 0x020DB07C | 4 | fx32 | Previous Z speed
0xDA | 0x020DB10E | 2 | ushort | Health
0xDC | 0x020DB110 | 2 | ushort | Health max
0x98 | 0x020DB12C | 2 | short | Current gravity
0x148 | 0x020DB17C | 1 | byte | Boost Ball charge level
0x149 | 0x020DB17D | 1 | byte | Boost Ball damage
0x14A | 0x020DB17E | 1 | byte | Boost Ball cooldown timer
0x14C | 0x020DB180 | 2 | ushort | UA
0x14E | 0x020DB182 | 2 | ushort | Missile ammo
0x150 | 0x020DB184 | 2 | ushort | UA max
0x152 | 0x020DB186 | 2 | ushort | Missile max
0x158 | 0x020DB18C | 1 | byte | Weapon in slot 0
0x159 | 0x020DB18D | 1 | byte | Weapon in slot 1
0x15A | 0x020DB18E | 1 | byte | Weapon in slot 2
0x15B | 0x020DB18F | 1 | byte | Gun animation
0x15C | 0x020DB190 | 72 | struct | Gun model
0x2C0 | 0x020DB2F4 | 72 | struct | Hunter model
0x308 | 0x020DB33C | 72 | struct | Gun smoke model
0x364 | 0x020DB398 | 156 | struct | Player controls
0x400 | 0x020DB434 | 1 | byte | Hunter ID
0x404 | 0x020DB438 | 4 | uint | Pointer to player values struct
0x434 | 0x020DB468 | 1 | byte | Frames since last shot
0x43C | 0x020DB470 | 2 | ushort | Shock Coil timer
0x440 | 0x020DB474 | 4 | uint | Last Shock Coil target
0x464 | 0x020DB498 | 72 | struct | Player input struct
0x4B0 | 0x020DB4E4 | 2 | ushort | Double Damage timer
0x4B2 | 0x020DB4E6 | 2 | ushort | Cloak timer
0x4B4 | 0x020DB4E8 | 2 | ushort | Deathalt timer
0x4C4 | 0x020DB4F8 | 4 | uint | Some flags
0x4C8 | 0x020DB4FC | 4 | uint | More flags
0x4CE | 0x020DB502 | 1 | byte | Weapon equipped
0x4CF | 0x020DB503 | 1 | byte | Weapon selection
0x4D2 | 0x020DB506 | 2 | ushort | Available weapons bit field
0x4D4 | 0x020DB508 | 2 | ushort | Available charges bit field
0x4F4 | 0x020DB528 | 48 | mtx43 | Some matrix
0x53C | 0x020DB570 | 1 | byte | Bomb refill timer
0x53D | 0x020DB571 | 1 | byte | Bomb count
0x55C | 0x020DB590 | 284 | CameraInfo | Camera info struct
0x694 | 0x020DB6C8 | 31 | struct | Light info
0x6C0 | 0x020DB6F4 | 4 | uint | Last jump pad pointer
0x708 | 0x020DB73C | 300 | struct | Alt form fields union
0x84C | 0x020DB880 | 1 | byte | Load flags
0x84D | 0x020DB881 | 1 | byte | Slot index
0x84E | 0x020DB882 | 1 | byte | Whether player is a bot
0x860 | 0x020DB894 | 2 | ushort | Weapon charge level

* Equip info struct (size = 0x14) starts at 0x850 (0x020DB884)

Various alt form values:

* Lockjaw active bomb count
* Some Dialanche attack timer
* Something movement-related for Stinglarva

The Lockjaw bomb cooldown (and the long delay after using too many bombs) are handled by some timer other than `0x53C`.

`0x4B2` is also used for the time spent standing still as Trace before his natural cloaking ability activates.

## Weapons

| ID | Weapon |
:- | :-
0 | Power Beam
1 | Volt Driver
2 | Missile
3 | Battlehammer
4 | Imperialist
5 | Judicator
6 | Magmaul
7 | Shock Coil
8 | Omega Cannon

Field `0x4CE` holds the ID of the currently equipped weapon. Field `0x4CF` holds the ID of the weapon which will be equipped when the weapon selection menu is closed, provided it is an available weapon. In practice, this is the same as the currently equipped weapon ID, until a weapon is highlighted in the weapon selection menu.

If a bit is set in field `0x4D2`, the corresponding weapon 0-8 is available to equip. If a bit is set in field `0x4D4`, the corresponding weapon 0-8 can be charged (if it is a weapon with a charge shot). In practice, these two bit fields always have the same value, and the default value is `00000101`.

## Ammo

In single-player mode, UA and missile ammo values have a scale of 10; that is, the actual missile ammo values at `0x14E` and `0x152` and the actual UA values at `0x14C` and `0x150` are 10 times the displayed amount. Uncharged shots consume 10 ammo (displayed as 1) and charged shots consume 30 (displayed as 3). The maximum missile value is 950 (95 missiles) and maximum UA value is 4000 (400 UA) with all expansions. Displayed ammo values are the integer result of dividing the ammo value by the ammo scale.

In multiplayer mode, ammo values have varying scales. The maximum missile value and UA values are 599 (59 missiles, varying UA).

| Weapon | Uncharged Scale | Charged Scale | Effective Max Ammo | Effective Charge Cost |
:- | :- | :- | :- | :-
Volt Driver | 5 | 25 | 119 | 5
Missile | 10 | 15 | 59 | 1.5
Battlehammer | 4 | - | 149 | -
Battlehammer<br>(Affinity) | 5 | - | 119 | -
Imperialist | 20 | - | 29 | -
Judicator | 5 | 25 | 119 | 5
Magmaul | 10 | 20 | 59 | 2
Shock Coil | 10 | - | 59 | -

| Pickup | 1P Value | MP Value
:- | :- | :-
Small UA | 100 | 50
Big UA | 250 | 100
Small missile | 100 | 50
Big missile | 250 | 100

Big ammo pickups are found in maps. Small ammo pickups are dropped by defeated enemies and hunters.

Note: When acquiring Prime Hunter status while not playing as Weavel, the displayed ammo value for the Battlehammer will continue to use a scale of 4, but the ammo cost will be 5. This makes it appear as though shots sometimes consume 1 ammo and sometimes consume 2. A similar effect occurs with charged missiles since the charge cost is not a multiple of the ammo scale.

In single-player, the Omega Cannon has unlimited ammo. In multiplayer, it is always unequipped/removed after being fired once.

## Weapon Info

There are two weapon info tables, one for single-player and one for multiplayer, each holding 18 of these structs. The first 9 are weapon info for the weapons in ID order, and the second 9 are weapon info for the same weapons with their affinity weapon stats. The single-player table is copied from the multiplayer table, with a few small adjustments.

- Size: 0xF0 (240)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 1 | byte | Weapon | Which weapon the beam is classified as
0x01 | 1 | byte | WeaponType | Usually the same as Weapon, but can indicate platform/enemy beam instead
0x02 | 2 | byte[2] | DrawFuncIds | Uncharged/charged draw function ID
0x04 | 4 | ushort[2] | Colors | Uncharged/charged color
0x08 | 4 | uint | Flags | Weapon flags
0x0C | 2 | ushort | SplashDamage | Damage for indirect hits
0x0E | 2 | ushort | MinChargeSplashDmg | Splash damage for minimum charge level
0x10 | 2 | ushort | MaxChargeSplashDmg | Splash damage for full charge level
0x12 | 2 | byte[2] | SplashDmgInterp | Uncharged/charged splash damage interpolation mode
0x14 | 1 | byte | Cooldown | Time in frames between shots
0x15 | 1 | byte | AutofireCooldown | Time in frames between shots when holding the trigger
0x16 | 1 | byte | AmmoType | 0 - none/UA, 1 - Missiles
0x17 | 2 | byte[2] | ColEffects | Uncharged/charged collision effect IDs
0x19 | 2 | byte[2] | MuzzleEffects | Uncharged/charged muzzle flash effect IDs
0x1B | 2 | byte[2] | DmgDirTypes | Uncharged/charged mode for calculating damage direction
0x1D | 2 | byte[2] | DamageInterp | Uncharged/charged damage interpolation mode by traveled/max distance
0x1F | 2 | byte[2] | Afflictions | Uncharged/charged affliction bits
0x21 | 1 | --- | --- | Padding
0x22 | 2 | ushort | MinCharge | Minimum amount for partial charge shot (if enabled)
0x24 | 2 | ushort | MaxCharge | Full charge amount
0x26 | 2 | ushort | AmmoCost | Ammo value consumed for uncharged shots
0x28 | 2 | ushort | MinChargeCost | Ammo value for minimum charge shots
0x2A | 2 | ushort | MaxChargeCost | Ammo value for maximum charge shots
0x2C | 2 | ushort | Damage | Uncharged shot damage
0x2E | 2 | ushort | MinChargeDamage | Minimum charge shot damage
0x30 | 2 | ushort | MaxChargeDamage | Full charge shot damage
0x32 | 2 | ushort | HeadshotDamage | Uncharged headshot damage
0x34 | 2 | ushort | MinChargeHsDamage | Minimum charge headshot damage
0x36 | 2 | ushort | MaxChargeHsDamage | Full charge headshot damage
0x38 | 2 | ushort | Lifespan | Lifespan in frames for uncharged shot
0x3A | 2 | ushort | MinChargeLifespan | Lifespan in frames for minimum charge shot
0x3C | 2 | ushort | MaxChargeLifespan | Lifespan in frames for full charge shot
0x3E | 4 | ushort[2] | SpeedDecayTimes | Uncharged/charged age at which speed decay begins
0x42 | 2 | --- | --- | Padding
0x44 | 4 | ushort[2] | SpeedInterp | Uncharged/charged speed interpolation mode
0x48 | 4 | fx32 | DamageDirMagnitude | Magnitude of uncharged damage direction vector
0x4C | 4 | fx32 | MinChargeDmgDirMag | Damage direction magnitude of minimum charge shot
0x50 | 4 | fx32 | MaxChargeDmgDirMag | Damage direction magnitude of full charge shot
0x54 | 4 | fx32 | ZoomFov | Half FOV value when this weapon is zoomed in
0x58 | 4 | fx32 | Radius | Cylinder radius for hit detection when uncharged
0x5C | 4 | fx32 | MinChargeRadius | Cylinder radius for hit detection at minimum charge
0x60 | 4 | fx32 | MaxChargeRadius | Cylinder radius for hit detection at maximum charge
0x64 | 4 | fx32 | Speed | Speed of uncharged shot
0x68 | 4 | fx32 | MinChargeSpeed | Speed of minimum charge shot
0x6C | 4 | fx32 | MaxChargeSpeed | Speed of full charge shot
0x70 | 4 | fx32 | FinalSpeed | Speed value to decay toward when uncharged
0x74 | 4 | fx32 | MinChargeFinalSpeed | Speed value to decay toward at minimum charge
0x78 | 4 | fx32 | MaxChargeFinalSpeed | Speed value to decay toward at maximum charge
0x7C | 4 | fx32 | Gravity | Gravity applied to uncharged shot
0x80 | 4 | fx32 | MinChargeGravity | Gravity applied to minimum charge shot
0x84 | 4 | fx32 | MaxChargeGravity | Gravity applied to full charge shot
0x88 | 4 | fx32 | UnchargedHoming | Homing strength of uncharged shot
0x8C | 4 | fx32 | MinChargeHoming | Homing strength of minimum charge shot
0x90 | 4 | fx32 | MaxChargeHoming | Homing strength of full charge shot
0x94 | 4 | fx32 | HomingRange | Maximum range when selecting homing target
0x98 | 4 | fx32 | HomingTolerance | Angle tolerance when selecting homing target
0x9C | 4 | fx32 | Scale | Scale of uncharged shot
0xA0 | 4 | fx32 | MinChargeScale | Scale of minimum charge shot
0xA4 | 4 | fx32 | MaxChargeScale | Scale of full charge shot
0xA8 | 4 | fx32 | Distance | Max travel distance of uncharged shot
0xAC | 4 | fx32 | MinChargeDistance | Max travel distance of minimum charge shot
0xB0 | 4 | fx32 | MaxChargeDistance | Max travel distance of full charge shot
0xB4 | 4 | uint | Spread | Max spread angle of uncharged shot
0xB8 | 4 | uint | MinChargeSpread | Max spread angle of min charge shot
0xBC | 4 | uint | MaxChargeSpread | Max spread angle of full charge shot
0xC0 | 4 | fx32 | RicochetLossH | Horizontal speed reduction factor when uncharged shot ricochets
0xC4 | 4 | fx32 | MinChargeRicochetLossH | Horizontal speed reduction factor when min charge shot ricochets
0xC8 | 4 | fx32 | MaxChargeRicochetLossH | Horizontal speed reduction factor when max charge shot ricochets
0xCC | 4 | fx32 | RicochetLossV | Vertical speed reduction factor when uncharged shot ricochets
0xD0 | 4 | fx32 | MinChargeRicochetLossV | Vertical speed reduction factor when min charge shot ricochets
0xD4 | 4 | fx32 | MaxChargeRicochetLossV | Vertical speed reduction factor when max charge shot ricochets
0xD8 | 8 | uint[2] | RicochetWeapons | Pointers to uncharged/charged ricochet child weapon info
0xE0 | 2 | ushort | ProjectileCount | Projectile count for uncharged shot
0xE2 | 2 | ushort | MinChargeProjectileCount | Projectile count for min charge shot
0xE4 | 2 | ushort | MaxChargeProjectileCount | Projectile count for full charge shot
0xE6 | 2 | ushort | SmokeStart | Counter value at which gun smoke should appear
0xE8 | 2 | ushort | SmokeMin | Counter value above which gun smoke should persist
0xEA | 2 | ushort | SmokeDrain | Value counter decreases by
0xEC | 2 | ushort | SmokeAmount | Value counter increases by for uncharged shot
0xEE | 2 | ushort | SmokeAmountCharge | Value counter increases by for charged shot

The Imperialist does half damage when not scoped.

### Weapon Flags

Bits 0 through 7 are an unsigned 8-bit priority value, used to choose a weapon when forced to switch (upon running out of ammo, for example).

Bits 24 and 25 are an unsigned 2-bit index into a list of radius values to use when checking for uncharged player beams colliding with destructible enemy beams. Bits 26 and 27 are used for this purpose instead when the player beam is charged.

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
8 | 0x100 | Partial charge enabled | 
9 | 0x200 | Charge enabled | 
10 | 0x400 | Repeat fire | When holding the fire button while below the minimum charge for a charge shot
11 | 0x800 | Zoom enabled | 
12 | 0x1000 | Ricochet enabled (uncharged) | 
13 | 0x2000 | Ricochet enabled (charged) | 
14 | 0x4000 | Auto release | Automatically fires when at max charge level with an opponent close to the gun barrel
15 | 0x8000 | Self damage (uncharged) | Projectile can damage the owner (unrelated to splash damage)
16 | 0x10000 | Self damage (charged) | 
17 | 0x20000 | Force effect (uncharged) | Spawn beam collision effect when expiring or colliding with other beams
18 | 0x40000 | Force effect (charged) | 
19 | 0x80000 | AoE (uncharged) | Collision check occurs instantly in area of effect when beam is spawned
20 | 0x100000 | AoE (charged) | 
21 | 0x200000 | Continuous | Beam is continually fired on each frame
22 | 0x400000 | Destructible (uncharged) | 
23 | 0x800000 | Destructible (charged) | 
28 | 0x10000000 | Life drain (uncharged) | 
29 | 0x20000000 | Life drain (charged) | 
30 | 0x40000000 | Surface collision | Whether the beam interacts with terrain/entity collision
31 | 0x80000000 | --- | Unused

## Hunters

| ID | Weapon |
:- | :-
0 | Samus
1 | Kanden
2 | Trace
3 | Sylux
4 | Noxus
5 | Spire
6 | Weavel
7 | Guardian

An ID of 8 in enemy spawners indicates a random hunter or Guardian encounter.

## Flags

`some_flags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Standing | Set while the player is on a flat surface
1 | 0x2 | Standing prev | Set if bit 0 was set on the previous frame
2 | 0x4 | No unmorph | Set while overhead collision prevents unmorphing
3 | 0x8 | No unmorph prev | Set if bit 4 was set on the previous frame
4 | 0x10 | Lateral collision | Set while moving horizontally into collision (terrain or entity)
5 | 0x20 | Acid | Set while on acid terrain
6 | 0x40 | Lava | Set while on lava terrain
7 | 0x80 | Entity collision | Set while moving into or standing on entity collision
8 | 0x100 | Used jump | Set when the midair jump has been used
9 | 0x200 | Alt form | Set while in alt form
10 | 0x400 | Alt form prev | Set if bit 10 was set on the previous frame
11 | 0x800 | Morphing | Set while changing from biped to alt form
12 | 0x1000 | Unmorphing | Set while changing from alt to biped form
13 | 0x2000 | Strafing | Set while strafing in biped and some alt forms
14 | 0x4000 | Free look | Set while free look is enabled (TODO: is this implemented?)
15 | 0x8000 | Free look prev | Set if bit 14 was set on the previous frame
16 | 0x10000 | Shot uncharged | Set if an uncharged shot was fired this frame
17 | 0x20000 | Shot Missile | Set if a Missile was fired this frame
18 | 0x40000 | Shot charged | Set if a charged shot was fired this frame
19 | 0x80000 | Gun open | Set while the gun has an "open" (Missile) animation
20 | 0x100000 | Grounded | Set while on the ground, even with terrain too steep to stand on
21 | 0x200000 | Grounded prev | Set if bit 20 was set on the previous frame
22 | 0x400000 | Walking | Set while moving in biped form on the ground
23 | 0x800000 | Biped movement | Set while moving in biped form (ground or air)
24 | 0x1000000 | Block aim input | Set for 9 frames after tapping a touch screen button
25 | 0x2000000 | Weapon menu open | Set while the weapon switch menu is open
26 | 0x4000000 | Boosting | Set while Boost Ball is active
27 | 0x8000000 | Can touch boost | Set while inputs are accepted for touch boost
28 | 0x10000000 | Jump pad | Set while experiencing jump pad acceleration
29 | 0x20000000 | Alt movement direction override | Set by camera sequences/camera positions
30 | 0x40000000 |  | Never set?
31 | 0x80000000 | Gun smoke | Set while gun smoke appears

`more_flags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Weapon charged | Set when creating full charge effect, cleared when beginning new charge
1 | 0x2 | Hide player model | Set while respawning, game over, etc.
2 | 0x4 | Weapon firing | Set while in biped and inputting shoot
3 | 0x8 | Alt form attack | Applies to Dialanche, Triskelion, Vhoscythe, and Halfturret
4 | 0x10 | Biped stuck | (Unused) Prevents moving in biped or switching forms
5 | 0x20 | Halfturret| Set while turret is active
6 | 0x40 | Cloaking | Set while the cloaking pickup is active
7 | 0x80 | Gravity override | Skip updating gravity value for this frame
8 | 0x100 | Biped still | Prevents moving in biped
9 | 0x200 | No form switch | Prevents switching forms
10 | 0x400 | Alt form gravity | Also applied after switch to biped until Y speed is zero or less
11 | 0x800 | LOD 1 | Set when player is drawn in lower level of detail
12 | 0x1000 | Draw type | Cleared if player is drawn first-person, set if in third-person
13 | 0x2000 | Coward | Set while revealed on radar for idling
14 | 0x4000 | Coward prev | Set if bit 13 was set on the previous frame
15 | 0x8000 | Spire climb | Set while moving into a wall such that it can be climbed
16 | 0x10000 | No shots fired | Cleared when first firing a weapon
17 | 0x20000 | WFC Omega Cannon | Set when Omega Cannon needs to be unequipped on WFC

`load_flags`:

| Bit | Mask | Flag | Description |
:- | :- | :- | :-
0 | 0x1 | Connected | Set while the player is present and accepting inputs
1 | 0x2 | Has connected | Set if bit 0 has ever been set
2 | 0x4 | Disconnected | Set when bit 1 is set but bit 0 is cleared
3 | 0x8 | Initial load | Set if the player/slot is active when the game is loaded
4 | 0x10 |  | Something to do with connection status, copying state and/or inputs
5 | 0x20 | Active | Set for player entities that are being updated
6 | 0x40 | Slot active | Set when the player slot is loaded
7 | 0x80 | Spawned | Set when first spawning

## Biped Animations

| ID | Description |
:- | :-
0 | Morphing
1 | Idle flourish
2 | Walking forward
3 | Unmorphing
4 | Damage recoil forward
5 | Damage recoil backward
6 | Damage recoil right
7 | Damage recoil left
8 | Idle
9 | Landing
10 | Landing left
11 | Landing right
12 | Jumping
13 | Jumping backward
14 | Jumping forward
15 | Jumping left
16 | Jumping right
17 | Unused shooting/charging animation
18 | Walking backward
19 | Spawning
20 | Strafing left
21 | Strafing right
22 | Turning
23 | Charging beam
24 | Firing charged beam
25 | Firing uncharged beam

Animations 0-3 come from each hunter's individual animation file, while the rest come from the shared animation file. For animation sharing and similar purposes, Trace and Noxus are considered to have the "Noxus" shape, while all other hunters are considered to have the "Samus" shape.

## Gun Animations

| ID | Description |
:- | :-
0 | Holding full charge
1 | Shooting charged beam
2 | Charging beam
3 | Idle
4 | Switch beam?
5 | Fully charged Missile
6 | Charging Missile
7 | Missiles closing
8 | Missiles opening
9 | Transition from shooting to missiles open?
10 | Shooting Missile
11 | Put away/take out
12 | Shooting uncharged beam
