# The headshot rig

Code: `Mods/Network/HitRig.cs`, scripts in `tools/hitrig/`. Built for one
question the rest of the harness cannot answer: **does a headshot the shooter
sees survive the authority's answer to it, on a 270 ms line?**

## Why the feature tour could not answer it

`NetTestScript` measures whether everything crosses the wire, and it is the
wrong instrument here in four separate ways:

| | |
|---|---|
| it fires in one phase of fifteen | a 90 s run lands **7** player overlaps |
| it closes to `PreferredRange` 4 units before firing | the long shot is never taken |
| it aims at `ModAimTarget` | which is the centre of the body sphere -- `PlayerVolumes[h,0].SpherePosition` is `(0,0,0)`, the **bottom third** of a hunter |
| it switches weapons every phase | the Imperialist is held for a fifteenth of the run |

And the report it produces cannot see the fault even when it happens:
`hit prediction: N confirmed` counts *hits*. A hit the shooter resolved as a
headshot and the authority resolved as a body shot is **confirmed** — the
prediction is retired, the percentage does not move, and the player watched an
instant kill turn into 72 damage.

## The geometry the whole thing turns on

From `BeamProjectileEntity.cs` and `Metadata/Player.cs`, for every hunter in
the table:

```
minPickupHeight  -2048/4096 = -0.50    the capsule's bottom, relative to Position
maxPickupHeight   4505/4096 =  1.0998  its top
bipedColRadius    2048/4096 =  0.50    plus the beam's own CylinderRadius
```

A biped is a cylinder **1.60 units tall** from `Position.Y - 0.5`, and the
headshot test is

```csharp
anyRes.Position.Y - player.Position.Y >= Fixed.ToFloat(player.Values.MaxPickupHeight) - 0.3f
```

so the band is `[+0.7999, +1.0998]` — **0.30 units, the top 18.75% of a
hunter**. That is the number every measurement here is compared against: a
vertical disagreement of a third of a unit between two machines is the whole
band.

The Imperialist is the weapon the complaint is about for two reasons, both in
`Metadata/Weapons.cs`:

- it is the **only** beam that scores a headshot at any range — every other
  weapon is `travel.LengthSquared <= 15 * 15`;
- `unchargedDamage: 72` against `headshotDamage: 200`, so on 100 health the
  headshot is an instant kill and the body shot is two thirds of one.

It is also effectively hitscan (`unchargedSpeed` 819200 = **200 units/frame**,
`unchargedLifespan` 2), which is worth knowing: the projectile catch-up loop
resolves it on its first step, so for this weapon the rewind *position* is the
entire mechanism.

## What the rig does

Three modes. Two of them split the clients into roles by slot parity, so both
machines agree without asking — even slots shoot, odd slots run.

- **The runner** stays in front of the gun and stays in the air. `-hitrig jump`
  jumps every 24 frames; `-hitrig sniper` every 90. It does not evade: the
  question is not whether a sniper can track somebody.
- **The sniper** holds the Imperialist for the whole run (`ModArmZoomWeapon`,
  re-armed every frame), zoomed (it deals half damage unzoomed, so an unzoomed
  run measures a different weapon), aims at `Position + 0.95` — the middle of
  the band, not its top — and holds the trigger, which fires once per
  `shotCooldown` 60. **One shot a second is the ceiling on the sample rate**,
  so a run wants four minutes, not one.

**`-hitrig duel` is the third, and it has no runner: everybody snipes
everybody at close range.** It is the only scenario that produces what the kill
arbitration exists for — two players killing each other inside a round trip, so
that on the machine keeping score one of them is already dead when the other's
shot arrives. Neither of the other two modes can, because in both of them one
of the two clients is holding a weapon it never fires at anybody.

Its cadence is the **server's** clock rather than the client's own frame
counter, and that is the whole trick: clients join seconds apart, so a local
counter has them firing at unrelated moments and the trade never happens. Every
client sees roughly the same snapshot number at the same time, so keying the tap
to `NetSession.LastSnapshotFrame` puts the two triggers within a latency of each
other. Read the run for `void (dead shooter)` in the server's claim line —
shots the authority refused because somebody had been put down in a strictly
earlier world — against the trades that were allowed to stand.

```bash
HITRIG_CLIENT_EXTRA="-netloss 2 -debuglog" \
  tools/hitrig/run-local.sh duel duel 150 350:80 45 "TEST ARENA"
```

`ModSetAmmo(ModAmmoCap, 0)` every frame: the Imperialist costs 20 UA a shot
against a cap of 400, and a sniper holding a range walks over no pickups. The
first version of this rig ran dry after twenty shots and the rest of the run
measured an empty gun, which reads in every report as a sniper who stopped
hitting.

Aim goes through `ModAimDeltaTowards`, which solves for the convergence point —
a shot travels towards a point a fixed distance down the aim ray, so aiming
straight at something further away lands low, and on a 0.3-unit band low is a
body shot.

**`-hitrig all` is the fourth, native only**, and the one the shooter-authoritative
work was measured with ([NETWORK-SHOOTER-AUTHORITY](../multiplayer/NETWORK-SHOOTER-AUTHORITY.md)).
The odd slot rides `TEST PADS`'s jump pads without stopping -- it walks back onto
the nearest pad every time it lands -- and shoots back while airborne; the even
slot holds range and fires each of the nine weapons for 20 s in turn, ammo
refilled. It is the only mode in which both players shoot with every weapon at a
target that is always in the air.

## The map

`TEST ARENA` (`maps/arena/arena.json`), because it guarantees the one thing the
scenario needs and no cartridge room does: **line of sight from anywhere to
anywhere**. The first run of this rig went to `MP6 HEADSHOT`, which has the jump
pads, and spent 321 triggers firing at a wall 53 units away — 92% of frames "on
target" and two hits in ninety seconds.

Its limit is size: 40 units square, so `-hitrig sniper` holds 34 units rather
than the 60+ a cartridge room could offer. 34 is still past the 15 at which
every other weapon stops scoring headshots, and it is a real long shot for this
game.

**`TEST PADS` (`maps/pads/pads.json`) is the same room with four jump pads**
throwing hunters across the middle to the pad opposite, in a box raised to 23
units for the arc. It exists because the arena's own answer to "is the target
moving" is a jump every 90 frames, and the case the whole question turns on is a
target crossing fast *and* vertically -- which on a 0.30-unit headshot band is
where a frame of disagreement between two machines is the whole band. The two
maps differ by the pads and the ceiling and by nothing else, so TEST ARENA is
its control. Regenerate with `FruityPrime -mapgen "TEST PADS"`; `-maptest
"TEST PADS" -players 4` reports `jumppads 4 (4/4 launched)` when it is working.

**It earned its keep on the first run.** The same scenario at a 320 ms round
trip under the old 400 ms ceiling, the two maps side by side:

| | clamped | clamp error, worst vertical |
|---|---|---|
| TEST ARENA | 80.0% | **0.35** units |
| TEST PADS | 87.5% | **0.81** units |

0.81 against a headshot band 0.30 units tall is nearly three bands: a player
riding a pad is put most of a body away from where their shooter saw them. The
arena's own runner, jumping on a cadence, reaches 0.377 units a frame and the
clamp error stops at 0.35 — the pads are where the quantity actually gets
large, and no cartridge room the harness can hold a duel in offers the same.

## Running it

```bash
tools/hitrig/stage.sh                      # freeze the build the rig runs from
tools/hitrig/bench.sh 240 270:60 2         # the local A/B, four arms, two modes
tools/hitrig/bench-p7.sh 120 320:80 2      # protocol 7 against protocol 6, both maps
HITRIG_JP_PASS=... tools/hitrig/bench-japan.sh 240   # the same over the real line
python3 tools/hitrig/summarise.py <run-dir>...       # the table
```

`bench-p7.sh` is the one to run after touching lag compensation, hit claims or
puppet smoothing. Four arms: `TEST ARENA` and `TEST PADS`, each at protocol 6's
behaviour (`-maxrewind 24 -noclaims -nointerp -relayedpuppets
-nodeathprediction`) and at the defaults. It also prints the authority's claim
line and the clients' smoothing line per arm, which `summarise.py` does not
read. The p7 arms run with `-debuglog`, because on a jump pad "the shot went
through him" has to be told from "the rewind went to the wrong frame" and only
the log separates them.

**`stage.sh` is not a convenience.** .NET maps its assemblies into memory, so a
rebuild that replaces `FruityPrime.dll` while a run is in flight takes every
client and the server down mid-match, silently, leaving empty logs and a
summary that reads as "the scenario produced nothing". Two runs were lost that
way before it existed.

**Injected latency needs jitter to reproduce the real line.** `-netlag 270`
alone never asks for more than 17 frames of rewind and so never reaches the
400 ms ceiling at all; the Japan server's real distribution is mean 19.6 with
the worst pinned at exactly 24, which is jitter pushing the tail into it.
`-netlag 270:60 -netloss 2` reproduces the shape. And without the loss the
`-pressage` arm has nothing to correct, since a trigger pull is only ever stale
when the packet that carried it did not arrive.

## Reading the table

| Column | |
|---|---|
| `clamped` / `clamp%` / `refused` | shots the ceiling took, and how many frames of rewind it refused each |
| `worstY` | the runner's worst vertical speed, in units/frame. Compare against **0.30** |
| `pred` / `conf%` | predictions made here and the share the authority agreed with — the old number |
| `unpred` / `local%` | hits the authority credited that this machine never resolved. **`local%` is the honest hit-registration figure**: `conf%` is confirmed over predicted, so a client that predicts one hit and gets it right reads 100% while missing sixty-nine others |
| `hsPred` / `hsOk` / `hsDown` / `hs%` | headshots resolved here, agreed by the authority, and **downgraded to body shots** — the reported fault |

The authority's numbers exist only in the server's log. On a dedicated server
every client correctly reports `lag compensation: on, nothing to compensate` —
they compensate nothing, the server does — so `summarise.py` reads
`server.log` for a local run and `authority.log`, pulled back over SSH, for a
Japan one.

## Traps

- **The Imperialist kills.** 200 on a headshot against 100 health, so the
  runner dies on most connecting head shots and respawns; a four-minute arm is
  not four minutes of shooting. Judge sample size from `rewound`, which counts
  every shot the authority resolved, not from the run length.
- **`Triggers` is approximate.** It counts press edges and on-target seconds,
  and the weapon's own cooldown decides how many of them become shots. Use it
  to tell "the sniper never had a shot" from "the sniper missed", not as a
  denominator.
- **A rotation clears every counter.** `NetHitPrediction.ForgetSlot` and
  `NetDamage.ResetForRoomChange` are per-match, so an arm that rotates
  underneath itself reports half a run. `run-local.sh` writes a one-map
  rotation at 20 minutes for this reason.

## Where a hit lands (`-hitlog`)

The tables above count hits. They cannot say whether the shooter and the
victim saw the **same** hit in the **same place**, which is what a player
means by "it felt right". `-hitlog FILE` (`Mods/Network/HitLocation`) writes
one CSV row per event on every machine, the point given in the victim's body
frame -- `dy` above Position, `hpct` as a share of the capsule (the head band
is the top 18.75%), `ang` the bearing from the way the victim faces:

| row | written by | what |
|---|---|---|
| `hit` | shooter's client | its shot met a puppet (`;1` in `extra`: swallowed by invulnerability, nothing claimed) |
| `hit` | victim's client | a remote shot drawn meeting this player (`;synth`: an impact drawn on the spot for a confirmed shot that was not in the air) |
| `pass` / `miss` | victim's client | an unconfirmed remote shot let through / one that came closest without touching |
| `dmg` | victim's client | the authority's damage arriving |
| `hit` | authority | its own copy of a remote player's shot, which no longer counts (the shadow) |
| `claim` | authority | judged (`extra`: history position; verdict) or applied (verdict 100) |

Rows join on the shot key (shooter, victim, weapon, launch frame); a remote
shot drawn on the victim's machine carries the ack of the intent that fired it
(`ModShooterAck`), which is the shooter's launch frame within a frame or two.
`tools/hitrig/hitloc.py RUN...` prints the joined table, one column per run,
all weapons but the Shock Coil (which ticks; it has its own section) and then
weapon by weapon.

**The maps** (`maps/wells`, `wellsstill`, `lanes`). Two players 12 units
apart -- inside the 15 at which every weapon scores headshots -- each held in
a glass cell: brushes with `"noBeams": true, "visible": false`, which stop a
player and let every shot through, so no knockback can move the target out
of the measurement. **TEST WELLS** puts a vertical jump pad in each cell,
bouncing one player to 6.2 units and the other to about 4.6, so the two never
fall into step with each other or with the round trip (a pad's `speed` is not
its launch velocity: these were measured with `highest`, not solved).
**TEST WELLS STILL** is the control, nobody moving; **TEST LANES** the
horizontal case, strafing. `-hitrig wells` / `lanes` makes both players shoot,
all nine weapons in turn (20 s each, so a run wants 180 s and 360 s is two
cycles), aiming at the head and at the chest five seconds each, and puts each
slot back in its own cell (a client picks its own spawn before it can see
anybody).

**Headless.** `-netcheck ... -headless` runs a client with no window and no
GPU, stepping the simulation at 60 Hz by the wall clock like the dedicated
server. The windowed client could not run on the WSL box at all (OpenGL
memory admission, no Vulkan backend in that build), and a netcode measurement
has no business depending on a driver.

```bash
HITLOC_BIN=~/fp-hitloc/bin-after tools/hitrig/hitloc-local.sh after-wells "TEST WELLS" wells 360
python3 tools/hitrig/hitloc.py ~/fp-hitloc/runs/base-wells ~/fp-hitloc/runs/after-wells
```

`hitloc-local.sh` runs a native server and two headless clients on the
loopback, the line made up by the clients (`HITLOC_LAG`, default `250:40`, the
Japan line's shape; `HITLOC_LOSS`). `hitloc-japan.sh` runs the same against
one of the bench box's unlisted servers.
