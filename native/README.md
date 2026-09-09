# Native C++ build compatibility entry

The native C++ implementation and its tests now live under
`src/MphRead.Native`, mirroring the managed `src/MphRead` categories. This
directory remains the compatibility build entry for existing commands and
MinGW users. The original C# projects remain in place while behavior is
migrated and checked against the existing client; removing the managed
implementation before parity would make the protocol and game behavior
impossible to compare.

The native gameplay source is split by responsibility. `Entities/gameplay.cpp`
is only the session constructor and fixed-step order; combat, enemy dispatch,
room environment/messages, items, objectives, players, projectiles/effects,
story state, and snapshots are separate `gameplay_*.cpp` modules. The enemy
directory has a same-named native pair for every managed `Entities/Enemies`
source file. This is a source-layout/parity aid, not a claim that every C#
state machine has already reached behavioral 1:1 parity.
The fixed and runtime entity implementations are also split into matching
translation units (`EntityBase.cpp`, `PlatformEntity.cpp`, `RoomEntity.cpp`,
`RuntimeEntityBase.cpp`, `BeamProjectileEntity.cpp`, and the corresponding
entity files); there is no second grouped implementation to drift from them.
The source contract reports all 272 managed source pairs; the 83 `Entities`
files are implemented units; no non-entity pair is left as a name-only
`BoundaryOnly` unit. Behavioral gaps are now recorded by subsystem.

 The native migration currently has two usable boundaries: a headless UDP relay
server and a Windows OpenGL room/session preview. The server is deliberately
dependency-free beyond the platform socket API, so it can run on Windows,
Linux, macOS, and ARM64 without bringing the renderer or launcher to a server
machine. Its wire structures mirror `src/MphRead/Mods/Network/NetProtocol.cs`:
the native server implements the current Hello/Welcome, status, intent,
snapshot, authority, chat, roster, and map-rotation boundary. The native
client can perform the same Hello/Welcome and status/list probes, so this
boundary can be checked against the managed implementation.

The preview now also has a native match-state boundary: its fixed-timestep
session updates the local clock and objective timers, `GameState` synchronizes
authoritative player/team counters, rebuilds result-slot ordering for all
supported modes, and evaluates Battle, Survival, Capture, Bounty, Nodes,
Defender, and Prime Hunter completion rules. A networked client adopts the
server's `MatchState` and sends `MatchEnd` only from the authority. The camera,
results-screen presentation, and remaining dynamic entity message graph are
still separate layers.
The native scene boundary now decodes the managed fixed layouts for platforms,
objects, doors, trigger/area volumes, jump pads, teleporters, lights, artifacts,
camera sequences, and force fields in addition to spawns and objectives. The
fixed-timestep session consumes active jump pads and multiplayer teleporters,
including per-slot re-trigger gating, and applies player-facing area effects
(damage, death, gravity, form locks, and biped locks) plus volume/automatic/
threshold trigger messages. Those ROM definitions now affect player movement
and environment activation rather than remaining raw bytes; full entity message
graphs and beam-driven trigger checks are still outside this preview.
JumpPad control-lock frames and the replicated player Frozen flag are honored
by the fixed-step input/fire path as well.
The native `Formats/EntityEnemy` boundary now decodes the complete 512-byte
MPH enemy-spawn payload for all thirteen managed spawner layouts, including
war-wasp movement vectors, enemy-hunter settings, and raw collision volumes;
the 268-byte First Hunt payload is decoded as well. Active story spawns now
resolve their cartridge model through the native enemy catalog and use the
model-instance animation path in the Win32 preview. The fixed-step session
has authored controllers for War Wasp, Zoomer, Temroid, Petrasyl1-4,
Shriekbat, Geemer, Barbed War Wasp, PsychoBit, both Voldrum variants, Fire Spawn, Alimbic Turret,
Quadtroid, CrashPillar, both Ithrak variants, and the static Carnivorous Plant
S07 controller. Slench S10 now has a typed three-phase profile/state machine
for its room variants, including shielded damage gating, volley projectiles,
roaming contact damage, S10.Index-paired turret activation, independent
shot/salvo cooldowns, and a native child
hierarchy for the shield plus three synapses. Child transforms, shield damage
forwarding, synapse health/heal/death/respawn states, and child-prioritized
beam collision are active; original animation choreography, nest presentation,
and the remaining individual enemy behavior classes are still a separate
runtime migration.
ItemSpawn records are also active in the session: spawn delay/interval/count,
activation messages, pickup range, health/ammo/weapon pickups, temporary
DoubleDamage/Cloak/Deathalt effects, energy/ammo expansions, and multiplayer
death drops are represented in native state. The Windows preview resolves all
23 managed standard item asset names from the cartridge and draws active items
with their spinning/floating models; a colored marker is retained only as a
missing-resource fallback. Weapon visual metadata, projectile trail/model
selection, local muzzle/impact effect events, and the complete 243-file
metadata effect catalog (including the common 33-resource weapon closure) are
native. Mesh particles resolve their cartridge model/node resources in the
Win32 preview; non-mesh particles use the cartridge B8/C4/CC/D0 geometry
families, rotation, and material repeat modes. The
Effects evaluator covers the supported scalar/vector expression graph used by
effect element actions, and
`Formats/effect_runtime.*` now expands those resources into native
element/particle lifetimes, action updates, acceleration, collision callbacks,
and child-effect requests. Item pickup effect sequencing and the remaining
individual enemy behavior classes remain separate runtime work; the simulation sound-cue bridge
and ROM-backed SFX dispatch are native.
The native `Strings` boundary parses the managed 12-byte string-table records,
including the ScanLog header and game-message prefix/split rules. The Win32
scan visor uses the real ROM's 332 categorized entries to emit the correct
nearby lore/bioform/object/equipment icons for room entities, items, and
enemies, with discovered entries using dim variants. Target selection,
the animated target box, basic progress/out-of-range presentation, and a basic
ScanLog text panel are native; room collision line-of-sight validation and the
visor toggle are native as well. Scan completion pauses the native scene and
accept/cancel input updates or leaves the story logbook accordingly, while
page-accurate dialog rendering and visor audio flow remain separate HUD work.
The local scan control is not serialized
into network intents and is supplied by keyboard `Q` or gamepad `X` while the
scan visor is active.
The native client also has the managed-compatible three-line chat log: network
messages are retained for ten seconds with a one-second fade, `T` opens the
desktop prompt, `Enter` sends, `Escape` cancels, and the opening key is not
inserted into the message. Offline previews echo locally; connected clients
send the server-stamped `ChatPacket`.

The headless native netcheck can also write the managed-compatible FPDM demo
stream. It records received datagrams plus the local `SlotIntent` synthesis so
the file contains the same packet boundary the managed recorder exposes:

```text
FruityPrimeServer -netcheck 127.0.0.1 -port 27888 -seconds 5 -recorddemo
FruityPrimeServer -netcheck 127.0.0.1 -port 27888 -seconds 5 -recorddemo ^
  -demoout export/_demos/check.fpdemo
FruityPrimeServer -demoinfo export/_demos/check.fpdemo
FruityPrimeServer -demoinfo export/_demos/check.fpdemo -replay ^
  -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds"
```

Network reproduction can be applied to one native client without putting a
proxy in front of the server. `-netlag 200` adds 200 ms round trip,
`-netlag 200:40` adds up to 40 ms of positive jitter per direction, and
`-netloss 5` drops 5% of datagrams in each direction. Reports label these
values as simulated rather than real-line measurements.

The headless check also preserves the managed spectator test boundary:
`-spectate` enters immediately, `-spectate 30` enters on the thirty-second
mark, and `-rejoin 45` returns to play on that mark. The client stays connected
and keeps sending intents, but suppresses gameplay buttons and publishes
`SpectatingState`; the native authority turns that into the replicated hidden,
non-target `FlagSpectating` state. The report includes the local spectating
frame count and transition frames. The Win32 preview exposes the same basic
operation with `F6` (enter / watch the next active player) and `F7` (rejoin);
the full managed free-camera and room-transition presentation is still outside
this native preview.

The native writer uses stored raw-DEFLATE blocks, which `DeflateStream` can
read, and the native reader accepts the fixed/dynamic raw-DEFLATE blocks that
the managed writer emits. `-replay` reopens the stream, loads the recorded room
from the supplied NDS, and feeds MatchState, Roster, SlotIntent, and Snapshot
records through the native fixed-timestep session. It reports the same stream
health signals as the managed headless check (snapshot coverage, repeated
snapshots, and the longest no-snapshot run). The Windows host also accepts
`-replay FILE -rom ROM` and displays that stream through the native room,
  camera, HUD, and scoreboard. Advanced free-camera movement, room-transition
  presentation, and the Android playback path remain on the managed side. The Win32 game host accepts the same `-recorddemo` and
`-demoout FILE` switches when it is connected to a match; it records received
datagrams and supplements an authority recording with its own Snapshot, just
as the managed recorder does. An offline preview has no network stream to
record, so it does not create an empty demo.

## Build

From the repository root:

```text
cmake -S native -B native/build-mingw -G "MinGW Makefiles" ^
  -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-mingw --parallel 4
ctest --test-dir native/build-mingw -C Release --output-on-failure
```

The same setup is available as `native/build-mingw.ps1`; it always selects
`C:\mingw64\bin\g++.exe` and `mingw32-make.exe`:

```powershell
.\native\build-mingw.ps1 -RomPath 'C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds'
```

The same helper can use the source-facing tree directly:

```powershell
.\native\build-mingw.ps1 -SourceDirectory src -BuildDirectory src/build-mingw `
  -RomPath 'C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds'
```

If CMake is not installed or should not be involved at all, `native/Makefile`
is the standalone MinGW path. It calls only `C:\mingw64\bin\g++.exe`,
`ar.exe`, and `mingw32-make.exe`:

```powershell
$env:FRUITY_PRIME_TEST_NDS = 'C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds'
C:\mingw64\bin\mingw32-make.exe -C native all --jobs=4
C:\mingw64\bin\mingw32-make.exe -C native test
```

The network test uses loopback UDP. Windows may require allowing the newly
built test or server executable through the Private-network firewall profile;
the build does not change firewall rules. A blocked test is reported as a
failed test rather than being treated as a pass.

To include the real-cartridge integration test, pass the ROM path while
configuring. The ROM stays outside this repository; the test extracts to a
temporary directory and removes it after checking the result:

```text
cmake -S native -B native/build-mingw -G "MinGW Makefiles" ^
  -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe ^
  "-DFRUITY_PRIME_TEST_NDS=C:/DSMPH/melonPrimeDS/all roms/allRoms/0367 - Metroid Prime - Hunters (USA) (Rev 1).nds"
ctest --test-dir native/build-mingw --output-on-failure
```

The repository ignores `*.nds`, the extraction folders (`files/`,
`_archives/`, `extracted/`, and `*_extracted/`), `paths.txt`,
`netcheck-shots/`, `netlog-*.txt`, `_demos/`, the native/managed export scratch folders,
and `native/rom-test-output/`, so local cartridges and their derived files are
not accidentally committed.

On Windows, add `C:\mingw64\bin` to `PATH` while running the executable or
CTest so the MinGW runtime DLLs can be found. The CMake generator itself may
come from the installed CMake distribution; compilation and linking use the
`g++` and `mingw32-make` binaries above.

The Windows outputs are `FruityPrime.exe` (game host) and
`FruityPrimeServer.exe`; non-Windows outputs follow the release naming rule
and are `FruityPrime`.

The Windows game host currently previews native model geometry. With an
extracted model file:

```text
FruityPrime -model extracted\models\some_model.bin
```

It can also load a model directly from the supplied NDS through a compressed
archive, without creating an extracted ROM tree:

```text
FruityPrime -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -archive archives/Samus.arc -entry Samus_lod0_Model.bin
```

The native host can load room boundaries directly from the same ROM.
`UNIT1_C0` uses the room's model, external texture/palette, collision, and
entity resources and feeds them to the fixed-timestep native session. All 27
standard multiplayer rooms are also covered by the ROM integration test, which
loads and classifies their typed entity records, checks collision data, and
decodes every model display list. The
native host loads all eight hunter biped/alt-form model sets from the managed
archive/recolor paths (Guardian reuses Samus's alt model), combines them with
each hunter's `pal_01` external model table and texture, and draws the active
hunter/form at the simulated player position. `WASD` moves, `Space` jumps,
`M` holds alt form, the left mouse button fires, and mouse movement turns
while held. `-bots N` adds up to seven deterministic local practice bots:

```text
FruityPrime -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -room UNIT1_C0 -bots 3 -seconds 5
```

The same host resolves and draws active static room entities (doors/locks,
platforms, objects, jump pads/beams, item/artifact bases, and teleporters)
through the native metadata/model catalog. Shared texture-share and explicit
`img_00` recolor resources are combined before decoding; the supplied ROM
audit covers 106 unique static-entity model dependencies with no missing
records.
Projectile state also keeps the managed previous position and ten-sample
trail history plus the weapon draw-function/color metadata. The Windows
OpenGL preview resolves the ROM's `iceShard`, `energyBeam`, `trail`,
`electroTrail`, and `arcWelder` assets for projectile models and billboards; a
colored line is kept as the missing-resource fallback. `BeamEffectEntity` now
maps the three model variants and NoSplat effect substitutions, while the
preview parses the complete 243-file effect catalog and draws mesh particles
from their cartridge model/node resources; effect 78 still prefers the
cartridge `iceWave` model when available. Non-mesh particles now use the
cartridge draw-function geometry families, particle rotation, and material
repeat modes; the dynamic beam graph remains separate migration work.
`Sound/sseq_player.*` now interprets the core SSEQ track graph and can render
basic PCM/PSG/noise events through the portable float boundary.

The native `Formats/movie.*` implementation now covers the managed
`Formats/Movie.cs` VXDS boundary: header/extradata, LPC codebooks, seek and
quantizer tables, MSB-first frame bitstreams, CAVLC residuals, inter-frame
prediction, DCT/YUV reconstruction, and the 128-sample LPC audio frames. The
real-cartridge `FruityPrimeNativeMovieTests` check decodes
`movies/01_bot.vx` (1,243 video frames and 14,275 audio frames in the supplied
ROM). `movie::Player` now owns the decoded timeline, pause/seek/end state, and
frame clock. The Windows OpenGL host accepts `-rom ROM -movie NAME` (or a
direct `.vx` path) and presents the decoded movie with aspect-preserving
letterboxing; `-seconds` remains available for bounded smoke/capture runs.
Audio device integration and scene-triggered cutscene scheduling remain
separate migration layers.

```text
FruityPrime -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -movie 01_bot.vx -seconds 2
```

The same decoder is available from the native utility. `-movieinfo` reports the
decoded stream, and `-export movie` mirrors the managed movie export by writing
numbered PNG frames plus `audio.wav` under the selected output directory:

```text
FruityPrimeServer -movieinfo "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -movie 01_bot.vx
FruityPrimeServer -export movie ^
  -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -movie 01_bot.vx -out export\_movies
```

Offline bots are driven by `Entities/Players/player_ai.*`: they select the
nearest valid opponent, respect team mode, choose a weapon only when its ammo
gate is usable, strafe and close distance, and vary firing/jump timing by
`-bot-level 0|1|2` (easy/normal/hard). The same value is available in the
native Win32 launcher and is persisted as `bot_level` in `launcher.txt`.
The stateful AI layer also retains the managed flag masks, button histories,
25-entry aggro table, 20-depth execution contexts, and weighted personality
transitions; unsupported Func3 IDs are counted rather than reported as fully
ported.

`-seconds N` bounds a game-host smoke run and exits cleanly after the native
window has rendered and simulated for that duration.

The Battle preview accepts `-mode MODE`, `-time SECONDS`, and `-goal POINTS`
for local match testing. The default mode is Battle (`3`); the team-mode
values use the managed mode table. Zero disables the corresponding limit.
`-friendlyfire` enables same-team damage for a local team-mode preview;
networked sessions replace these preview defaults with the server's
`MatchState`.
The native mode table also carries the managed defaults for Battle (7:00/7),
Survival (15:00/2 lives), Capture (15:00/5), Bounty (15:00/3), Nodes
(15:00/70), and the time-goal modes Defender/Prime Hunter (15:00/1:30).
Modes whose goal is not a point total do not use the point-goal ending path.

When present beside the executable, `Savedata/settings.json` is also read in
the managed `MenuSettings` shape for `Mode`, `TimeLimit`, `PointGoal`,
`TeamPlay`, and `FriendlyFire`. Explicit command-line values take precedence;
malformed or missing settings keep the native defaults.

The portable launcher save layer in
`src/MphRead.Native/Mods/Launcher/Portable` reads and writes the managed
`Savedata/save001.json` through `save003.json` format. It lists each slot's
area, health, and octoliths, resumes from the checkpoint room, and applies the
same commit cleanup as the managed build. Invalid JSON is treated as an empty
slot; the native save test covers round trips, slot listing, and that cleanup.

The Windows host also polls the first XInput controller through a dynamically
loaded `xinput1_4.dll`/`xinput1_3.dll`, so MinGW does not need an XInput import
library. The canonical mapping is A jump/boost, B morph, RT fire, LT zoom,
Y scan visor, bumpers or left/right D-pad cycle weapons, and the left stick
moves (including roll directions). The right stick uses a radial dead zone and
squared look response; keyboard, mouse, and controller input are combined.
The host reads the managed-compatible `controls.txt` values for mouse
sensitivity/inversion and gamepad dead-zone/look/Y-inversion. `1`-`9` select a
weapon, `Q`/`E` cycle, right-click zooms, `V` toggles the scan visor, and
`Tab` opens the native scoreboard. In a multiplayer preview, `T` opens chat,
`Enter` sends, and `Escape` cancels. The gamepad's Power Beam, Missile, and
Back bindings feed the same direct-select/scoreboard paths. The preview still
does not expose an editor for those values or the full pause/menu actions.

`ESC` pauses and resumes the native preview without destroying the session;
closing the window still exits. A paused network client sends an idle frame so
the relay does not mistake a local pause for a disconnect. The full managed
pause/settings menu is still not part of this preview.

With no ROM or room required, `-gamepad` watches the first four XInput slots
and prints the normalized axes, trigger/button state, and native logical action
bits. It returns success only after seeing a controller and a non-neutral input:

```text
FruityPrime -gamepad -seconds 15
```

Launching the Windows executable with no arguments, or with `-launcher`, opens
the native Win32 launcher. It lets the player select the ROM, room, player
name, hunter, and offline bot count; the selected values are stored in
`launcher.txt` beside the executable and reused on the next launch. The
launcher defaults to `UNIT1_C0`. An explicit `-rom`/`-room` invocation remains
available for scripts and tests.

The Windows host can join the native relay after loading a room. `-connect`
uses the same Hello/Welcome/Identify boundary as the command-line client;
`-name` and `-hunter` select the local roster identity:

```text
FruityPrime -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -room UNIT1_C0 -connect 127.0.0.1 -port 27888 -name Player -hunter 0
```

Open a recorded match in the native Windows host. The room is taken from the
recorded MatchState; `-room` is only needed when testing an explicit asset
alias:

```text
FruityPrime -replay export/_demos/check.fpdemo ^
  -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds"
```

The standard multiplayer room catalog is also native. It contains the 27
current C# metadata entries (IDs 93-119), and `-room` resolves their archive,
model, texture, collision, and entity paths from the ROM:

```text
FruityPrimeServer -rooms
FruityPrimeServer -roominfo "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -room "MP1 SANCTORUS"
```

Run a local, unlisted server with no game files:

```text
FruityPrimeServer -server -players 8 -nomaster
```

Use `-seconds N` for a bounded smoke run. The default rotation file is created
beside the executable on first run, just as the managed server does.

Inspect a running server or the directory from the native command line:

```text
FruityPrimeServer -status 127.0.0.1 -port 27888
FruityPrimeServer -servers -master net.livetek.fr -masterport 27889
FruityPrimeServer -connect 127.0.0.1 -port 27888 -name Player -hunter 0
```

The first native format boundary is also available without a game window:

```text
FruityPrimeServer -rominfo game.nds
FruityPrimeServer -extract-rom game.nds -out files
FruityPrimeServer -archive-info sound.arc
FruityPrimeServer -extract-archive sound.arc -out extracted
FruityPrimeServer -collision-info room_collision.bin
FruityPrimeServer -entity-info room_entities.bin
FruityPrimeServer -entity-repack room_entities.bin -out room_entities.repacked.bin
```

The same `Formats` boundary reads multiplayer navigation node data directly
from extracted files or an NDS byte stream. Version 6 tables and First Hunt
version 0 tables expose their nested lists, shared values, node colors, and
closest-node lookup; the native test also walks every supported node-data file
in the supplied ROM. Room-node/frustum references are available through the
native `Culling` value types.

The native exporter can write both the existing Wavefront OBJ/MTL pair and a
static COLLADA 1.4.1 scene from the same decoded model.  Both include decoded
positions, normals, UVs, material colors, and every mesh primitive:

```text
FruityPrimeServer -model-export-obj model.bin -out export/model.obj
FruityPrimeServer -model-export-collada model.bin -out export/model.dae
```

The model boundary is bidirectional as well. `RepackModel` writes the managed
model header, node/material/display-list tables, animation payload, inline or
external FPTX/FPAL resources, and capped or uncapped bounds. The serializer is
checked by byte-for-byte synthetic round trips and by a real-cartridge Samus
model round trip. The utility exposes it without opening a game window:

```text
FruityPrimeServer -model-repack model.bin -out model.repacked.bin
FruityPrimeServer -model-repack model.bin -out model.repacked.bin ^
  -separate-textures -texture-out export/model.fptx -bounds uncapped
```

Camera-editor sequences are also a native format boundary. The parser checks
the fixed header and 100-byte keyframes, exposes authored node references and
camera values, and samples hold/move timing with the managed easing rules:

```text
FruityPrimeServer -camseq-info "C:\\DSMPH\\melonPrimeDS\\all roms\\allRoms\\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds"
FruityPrimeServer -camseq-info files -sequence cameraEditor/unit1_land_intro.bin
```

The sampled path keeps entity-relative transforms caller-owned because those
need a live scene/entity table. The native metadata catalog also includes all
52 cartridge enemy records, including scan IDs, death effects, and the packed
damage-effectiveness table. Active story spawns now run through a generic
family controller in the fixed-step session (target selection, room-aware
movement, health, and contact damage); exact per-enemy state machines and
linked boss/ranged effects remain a later runtime layer.

The complete native sound catalog can be inspected from a ROM or extracted
asset directory. It covers the two sample tables, BGM/SFX selectors, 3D
falloff, sound slots, room/intermission music, SFX scripts, DGN parameters,
and named SDAT streams:

```text
FruityPrimeServer -soundinfo "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds"
```

`-sseqinfo` runs the same native SSEQ timeline and basic voice renderer for a
selected sequence, which checks a ROM-backed sound path without opening a game
window:

```text
FruityPrimeServer -sseqinfo "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" -sequence 0 -seconds 1
```

The same Sound library contains a dependency-free `SoftwareMixer` for the
managed SFX boundary: PCM8/PCM16 buffers, source queues, loop points, pitch,
gain, distance attenuation, and stereo placement are mixed into float blocks.
`Mods/Sound/sfx_mixer.*` exposes `pump()`, and the MinGW/Windows build connects
that block to a fixed four-buffer WinMM PCM queue through
`Sound::Win32AudioOutput`. It is optional and remains safe when no default
audio device exists. The Windows host initializes it when a ROM-backed room is
started, dispatches `gameplay::Session::sound_events()` for shots, impacts,
damage/death, spawns, jumps, pickups, and enemy hits, and pumps it once per
rendered frame.
`Sound/sfx_runtime.*` connects the catalog to the voice mixer: it lazily
decodes PCM samples, dispatches the encoded sample/script IDs, schedules SFX
script entries at the cartridge's 30 Hz timing, and refreshes source spatial
parameters, gain, pitch, and script pan. DGN volume/pitch curves are evaluated
per update as well. `Sound/sseq_player.*` now executes the core SSEQ command
interpreter (track allocation/opening, waits, jumps, calls/loops, variables,
tempo and note controls) and renders PCM/PSG/noise voices for an offline or
platform-neutral consumer. `Sound/music_runtime.*` consumes the music
controller commands, renders a bounded selected-track SSEQ buffer, and loops
it through the native software mixer; the Windows game host starts it with the
room's first music track. Full DS channel allocation/envelope/interpolation,
environment voice arbitration, per-frame track-volume mixing, and
OpenAL/Android device backends are the remaining audio-runtime steps.
`Sound/music.*` mirrors the managed `Music` state machine for track selection,
queued sequences, encounter suspension, escape/event tempo ramps, volume fades,
and per-track fades; it emits the commands consumed by the native runtime.

Q3 maps can be converted into a native recipe and its adjacent level/texture
files in one command. On Windows MinGW, JPG/PNG/TGA images in the PK3 are
decoded through WIC, reduced to the FPTX palette format, and then reopened by
the normal `-mapgen` path:

```text
FruityPrimeServer -q3convert level.pk3 -map LEVELNAME -name ROOM ^
  [-noclip] [-scale N] [-texsize N] [-out DIR]
```

Custom brush maps can now be generated without Visual Studio or a ROM-backed
asset copy. The native `MapGen` parser accepts the managed JSON recipe shape,
and the brush builder writes model, MPH collision, entity, animation, and
navigation-node binaries that the native format readers can reopen:

```text
native/build-mingw-direct/FruityPrimeServer.exe -mapgen maps/arena/arena.json
native/build-mingw-direct/FruityPrimeServer.exe -mapgen "TEST ARENA" -out export/_mapgen
```

The output defaults to `export/_mapgen` and is ignored. The native generator
covers brush recipes and Quake 3 version-46 `.bsp`/deflated `.pk3` imports,
including surface conversion, clipped brush collision, map entities, trigger
push pads, supported multiplayer pickups, and FPTX palette-indexed textures
when the recipe names a pack. Borrowing `textureSource` assets and baking
source images into FPTX are automatic for `-q3convert` on Windows; an explicit
FPTX pack remains the portable/bundled path on other platforms.

For a real cartridge-backed room/gameplay smoke test, `-maptest` loads the
room directly from the NDS, creates up to eight native players, advances the
fixed-step session (including room item spawners), and optionally decodes every
model/texture/palette batch:

```text
FruityPrimeServer -maptest UNIT1_C0 ^
  -rom "C:\DSMPH\melonPrimeDS\all roms\allRoms\0367 - Metroid Prime - Hunters (USA) (Rev 1).nds" ^
  -players 8 -bots -renderprobe -seconds 1
```

## Migration boundary

Implemented in this milestone:

- C++20/CMake build and native server entry point.
- MinGW-built Windows native game host with the model geometry path.
- Native room loading and a deterministic fixed-timestep player session driven
  from the ROM's model, collision, and entity records.
- Native ItemSpawn lifecycle and player pickup inventory state, including
  standard health/ammo/weapon items, timed multiplayer drops, and the pickup
  message target boundary.
- Cross-platform UDP transport with a bounded receive queue.
- Exact little-endian/network-order packet codecs and size guards.
- Checked model readers and writer: materials, textures, palettes,
  display-list commands, geometry batches, animation payloads, inline/external
  FPTX/FPAL resources, and bounds-mode serialization.
- Native fixed-format coverage for cartridge enums, raw records, Frontend
  menus, Effects functions/elements, AI personality trees, and camera-editor
  sequences. Camera paths include duration/easing sampling and a real-ROM
  probe; entity-relative camera transforms remain scene-owned.
- Native `Formats/EntityEnemy` decoding of the complete MPH/FH enemy-spawn
  records, including all managed union layouts and collision-volume fields.
- Native enemy metadata catalog matching all 52 cartridge entries, including
  scan IDs, death effects, asset-facing item metadata, and packed damage
  effectiveness decoding.
- Managed-compatible `Formats/NodeData` version 6 and First Hunt version 0
  readers, including nested offsets, shared values, closest-node lookup, and
  the room-node/frustum `Culling` records.
- Native room entities are classified by cartridge type; player/item/objective
  spawn payloads are decoded into typed C++ records and drive spawn selection.
- Checked MPH/First Hunt collision tables and version 1/2 entity tables.
- NDS FNT/FAT traversal and LZ-0x10 decompression for the compressed archive
  resources found in real cartridges.
- Native cartridge-backed `-maptest` room/gameplay probe with multi-player
  fixed-step simulation and optional complete render-resource decoding.
- Native FPDM replay inspection through the ROM-backed gameplay session,
  including room transitions, typed intent/snapshot decoding, ordering guards,
  and snapshot-coverage diagnostics.
- External room texture/palette resources and fixed-function OpenGL texture
  upload for the native Windows preview.
- Native standard multiplayer room catalog (C# metadata IDs 93-119), used by
  both the room inspection command and the Windows room host.
- Native Samus biped/alt-form model loading with the external recolor table and
  texture, player position rendering, Win32 mouse look, dynamically loaded
  XInput gamepad support, and keyboard/mouse/gamepad input-to-intent mapping in
  the Windows room host.
- Native match-flow state machine for the Battle preview: local time limit and
  point-goal ending, server `MatchState` adoption, authority-only `MatchEnd`,
  and end-state HUD feedback. The native session also consumes ROM-backed Node
  and Octolith flag volumes for Nodes, Defender, Capture, and Bounty scoring.
- Native player-facing AreaVolume effects (damage, death, gravity, form/biped
  locks) and volume/automatic/threshold TriggerVolume dispatch, including
  parent/child activation links.
- Native JumpPad control-lock timing and Frozen-state input/fire suppression.
- Native chat log and Win32 text-input path: server-stamped receive, local
  echo, bounded compose text, three-line expiry/fade, and `ChatPacket` send.
- Native `Mods/MapGen` JSON recipe parser and generator: brush and Quake 3
  BSP/PK3 surface conversion, clipped collision, version-2 entities,
  version-6 navigation nodes, and derived animation output, with format
  round-trip tests against the checked-in arena and Dust II recipe.
- Native Q3 conversion command: PK3/BSP recipe generation, source-level copy,
  spawn fallback selection, scale/vertex-precision calculation, and Windows
  WIC JPG/PNG/TGA-to-FPTX baking with a dependency-free TGA/PPM decoder for
  portable test fixtures.
- Native `Export` geometry output for both Wavefront OBJ/MTL and static
  COLLADA 1.4.1 scenes, plus the Blender `mph_common.py` scripting layer for
  recolors, materials, node weights, and decoded animation tables.
- Native desktop update installer boundary: ZIP/tar.gz package staging with
  path/CRC validation, executable permission handling, and the staged
  `-applyupdate` copy/restart handoff. Unstamped local builds remain page-only.
- Managed-compatible native `controls.txt` loading for mouse and gamepad
  sensitivity, dead zones, inversion settings, and custom pad bindings.
- MPH polygon sphere sweeps used by the native player session, including
  collision-flag filtering.
- Dedicated relay lifecycle: join/refusal, slot reuse, authority promotion,
  snapshots, intents, pings, roster, status, chat rate limiting, timeout,
  match timing, and map rotation.
- Master heartbeat/farewell packets.
- Master directory: server listing, expiry, public-address rewriting, and
  directory-started hosted games.

Still managed and intentionally not claimed as fully ported:

- The full game loop, complete renderer integration, full DS-fidelity SSEQ
  channel playback, and platform audio backends,
  and asset-backed full HUD/match-mode behavior; item lifecycle and cartridge
  item model submission plus the first ROM-backed gameplay SFX cues are native,
  while complete item audiovisual sequencing is not yet ported.
  Active story enemy model submission is now wired through the catalog, and the
  native session has staged typed controllers for a subset of story enemies,
  including Slench's shield/volley/roam cycle. Individual enemy
  animation/state behavior, linked boss-part choreography, and effects are not
  yet gameplay parity. Player model
  animation playback, animated texture/palette rebinding, and material
  color/alpha updates are connected for all eight hunters in the Windows
  preview, but this is still a native
  room/session preview with movement, weapon/zoom input, projectile and
  contact combat, a native HUD/scoreboard, and the basic Battle match-flow
  state machine, not gameplay parity.
- The full Avalonia launcher, pause/settings UI, and the remaining launcher
  flows; the native entry point currently covers first-run ROM selection and a
  remembered preview room.
- The Android activity, GLES bridge, touch controls, demo library, map
  importer, and advanced spectator/room-transition playback presentation.
  Native FPDM read/write, headless `-replay`, and the basic Windows replay
  room/camera/HUD path are present.
- The complete native gameplay client and the remaining dynamic per-entity
  interactions/results presentation sequence; all fixed standard room payloads are decoded,
  and the current client covers the roster, intent, snapshot, projectile
  boundary, player-facing volume effects, and typed spawn/objective records,
  while the session is not yet the full managed game loop.

Those remain the next migration layers. Each layer must be validated against
the current C# implementation before the corresponding managed path is
retired.
