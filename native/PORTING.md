# C++ port ledger

The request is a native C++ conversion, not a C++/CLI wrapper. The native
source tree is `src/MphRead.Native`; this `native/` directory only keeps the
compatibility build entry and migration notes. The repository therefore keeps
the C# implementation as the reference until the native replacement covers
the same behavior.

## Current evidence

- The managed repository contains 272 C# files and approximately 190,036 lines
  under `src/MphRead` alone.
- There are four managed project files: the game/server, Android head, NCSF
  player library, and network test tool.
- There were no C++ sources or native build files before this migration.
- MinGW validation uses `C:\mingw64\bin\g++.exe` and
  `C:\mingw64\bin\mingw32-make.exe`; Visual Studio's compiler is not part of
  the native build.
- The managed game depends on Avalonia, OpenTK, ReFuel.StbImage, OpenAL Soft,
  and platform-specific Android APIs. These need explicit C++ replacements;
  they are not language-level translations.

The gameplay implementation is now split by responsibility rather than
concentrated in one generated-looking file: `Entities/gameplay.cpp` contains
only session construction and fixed-step ordering. The player, projectile,
environment/message, objective, item, enemy-dispatch, combat, story, and
snapshot boundaries are separate `gameplay_*.cpp` modules. The 42 managed
enemy files also have same-named native `.cpp`/`.hpp` pairs. This is a source
layout and audit milestone; it is not behavioral 1:1 completion.
The fixed-room and dynamic runtime entity definitions are split the same way:
class-oriented files under `Entities/` and player input/collision files under
`Entities/Players/`; the former grouped translation units were removed after
their definitions were moved. The source-tree contract checks these units as
well as the enemy pairs.
It also reports the wider tree explicitly: all 272 managed files have a
   same-relative native pair, the 83 `Entities` files contain no `BoundaryOnly`
   unit, and no source unit remains name-only boundary work.

## Layer order

1. Network protocol, dedicated relay, and master directory.
2. Common binary/value types and ROM/archive readers, then model, collision,
   and entity table readers.
3. Simulation ownership, entity lifecycle, and deterministic test harness.
4. OpenGL/GLES renderer and platform window/input/audio adapters.
5. Launcher, settings persistence, demos, and tools. The native entry point
   now has a Win32 ROM/room/player selection screen and remembered preview
   settings; the FPDM reader/writer, headless `-demoinfo`/`-recorddemo`, and
   ROM-backed headless `-replay` boundary are also native.
6. Android application head and packaging.
7. Differential tests, packaged builds, and retirement of the C# projects.

The managed client remains the compatibility oracle while layers 2-6 are
implemented. Native CTest now covers the packet codecs, logical input mapping,
synthetic DS FNT/FAT
walking, archive round trips, model fixed records, both collision header
families, both entity table versions, the dedicated-client handshake/status
path, master listings, and (when `FRUITY_PRIME_TEST_NDS` is configured) a real
NDS FNT/FAT read, compressed `Samus.arc` archive read, complete extraction, and
re-read check plus all 27 standard multiplayer scene loads containing their
model, collision, and entity records, full model display-list decoding, and a
fixed-timestep player
movement/snapshot round trip with an MPH sphere-sweep collision query. The
same format suite now parses the managed `NodeData` version 6/First Hunt
version 0 layouts, preserves nested node/value offsets, and checks the
renderer-facing culling reference records; the supplied ROM contains 117
node-data candidates, of which 116 are parsed and one known version 4 file is
left unsupported just like the C# reader.
The native fixed-format layer also mirrors the managed enum/raw-record
definitions, Frontend menu tables, Effects function/element tables, and AI
personality tree reader. `camera_sequence` reads the authored
`cameraEditor/*.bin` header/keyframes and samples hold/move timing, easing,
FOV, and roll. `camera::Playback` resolves entity-relative poses through a
scene-supplied callback, and `scene_runtime::Scene` owns the ROM-backed file
and playback lifecycle, typed target/message dispatch, fade requests, and
form-lock state. The supplied ROM currently decodes all 25 AI roots and
the camera sequence integration fixture. Enemy metadata covers all 52
cartridge records, and every managed enemy source now has an explicitly
dispatched native module descriptor. The descriptor status distinguishes
implemented/shared/partial controllers and the spawner boundary, so the
remaining behavioral gaps are visible rather than hidden in a generic file.
`Formats/movie.*` now mirrors the managed `Formats/Movie.cs` VXDS decoder:
header/extradata, LPC books, seek/quantizer tables, frame bitstreams, CAVLC
residuals, inter-frame prediction, DCT/YUV reconstruction, and 128-sample LPC
audio frames are native. The supplied cartridge test decodes
`movies/01_bot.vx` with 1,243 video frames and 14,275 audio frames. Cutscene
`movie::Player` now supplies decoded timeline playback, pause/seek/end state,
and the Windows OpenGL host presents `-rom ROM -movie NAME` with aspect-
preserving letterboxing. Scene-triggered scheduling and platform audio output
are still separate layers.
The utility exposes the same boundary through `-movieinfo` and `-export movie`,
with the native exporter writing numbered PNG frames and PCM16 `audio.wav`.
Native model scripting now also emits the Blender-side `mph_common.py` control
script from the decoded material, node, recolor, and animation tables; the
source-facing `Export/Scripting.cpp` implementation is included in the direct
MinGW library and the export test.
The desktop update boundary is native as well: release packages are staged
under `.update/staged`, ZIP/tar.gz entries are checked for safe paths and CRC,
and `-applyupdate` performs the second-process copy/restart handoff while
preserving user files in the installation directory. Local, unstamped builds
remain ineligible for in-place installation.
The new `Entities/Enemies/enemy_catalog.*` boundary records the managed
source/class name, behavior family, and message/collision flags for each of
those 52 IDs, so room loading and diagnostics no longer lose the authored type.
The 42 source-file pairs are checked by the source-tree contract test; the
remaining per-enemy behavior is tracked as the status in each descriptor.
The `Formats/EntityEnemy` reader also decodes the complete MPH 512-byte and
First Hunt 268-byte enemy-spawn records, including all managed union layouts;
it is available to the scene caller without changing the existing raw payload
ownership. The runtime layer now contains the native `EnemySpawnEntity`
lifecycle boundary as well: initial/cooldown waits, player-distance
suspension, batch/concurrent/total limits, out-of-range destruction
accounting, and Activate/SetActive handling. `gameplay::Session` turns
accepted requests into `EnemyState` records and exposes damage/destruction
hooks. PlayerSpawn records now retain active/availability/orientation state and
the native fixed-step session applies the managed safe-distance/cooldown
respawn selection. Authored controllers for `00_WarWasp`, `01_Zoomer`,
`02_Temroid`, the shared `03_Petrasyl1`-`06_Petrasyl4` flying controller,
`12_Geemer` surface path, `10_BarbedWarWasp`, `11_Shriekbat`, the S06
`18_AlimbicTurret`, `23_PsychoBit`, `35_Voldrum2`/`36_Voldrum1`, `39_FireSpawn`, lethal-cloud `16_Blastcap`,
`37_Quadtroid`, `38_CrashPillar`, both `46_LesserIthrak`/`47_GreaterIthrak`,
the static S07 `51_CarnivorousPlant` controller, and a staged S10 `41_Slench`
shield/volley/roam controller now decode their cartridge payloads and run in
the native session. Prime Hunter's periodic
twenty-frame damage is native as well. The other individual enemy AI classes
and the Slench shield/nest/synapse transform/message graph remain later parity
work. Spawner
completion now
does persist the room-state, encounter-bit, and boss-escape side effects into
the native StorySave. `SceneSetup` now mirrors the managed AreaHunters update
and S09 random-hunter selection, including persisted encounter blocking, and
the direct Adventure host materializes selected hunter bots. Hunter
death/respawn and the full parent transform/message graph remain later parity
work.
`runtime::EntityPool` now also dispatches the raw cartridge SetActive, Damage,
Impact, Death, Destroyed, and turret activation messages to dynamic item,
bomb, enemy, spawner, and half-turret entities through `Scene`; the complete
beam collision/effect graph and per-enemy behavior classes remain later work.
The `Entities/Players/player_profile.*` boundary now preserves all eight
managed `PlayerValues` records and `AvailableArray`; the native fixed-step
session consumes the per-hunter movement scale and alt-form strafe rule. The
player AI now also has a stateful partial matching `AiFlags2/3/4`, input
histories, the 25-entry aggro table, 20 execution contexts, and weighted
personality transitions. Collision visibility is conservative and the full
Func1/2/3/4 action graph plus camera/draw/HUD/sound behavior remain later
parity work.
`Mods/Render/render_mods.*` also carries the managed preview-camera basis and
DS-tiled player-icon bounds; `scene_runtime::Scene::set_preview_camera` uses
that boundary for room previews.
The root `Renderer` boundary now also carries the managed camera modes,
pivot/roam pose math, perspective/frustum state, collision-display enums, and
the black/white fade controller. It produces matrices and overlay opacity
without touching OpenGL, leaving only backend-specific submission to the
window host.
The Win32 host now consumes the native `WindowMode` state as well: saved
`launcher.txt` startup mode, `-fullscreen`/`-borderless`/`-windowed`,
F11/Alt+Enter borderless toggling, pause-aware topmost state, and the
`-nohelmet`/`-prohud`/`-cel`/`-fog`/`-fps`/`-celbands`/`-celedge`/
`-resolution-scale` render overrides are wired before the render loop.
`Mods/Launcher/Gui/gui_model.*` now carries a toolkit-neutral page/navigation
model for the managed splash, first-run game-files gate, home, map/demo picker,
settings, and pause surfaces. The Win32 dialog still owns the controls, while
Avalonia and Android widget implementations remain later frontend work.
`Mods/Launcher/Portable/text_launcher.*` now provides the platform-neutral
terminal choice flow over the existing `launcher.txt` and game-file setup
formats. Continue/New game reads the managed save slot format and resolves its
checkpoint room into a local Story-mode plan. `match_start.*` converts offline,
story, and network results into one shared mode/rule/player roster preparation
path, and the Windows OpenGL host consumes that path too; story inventory and
packed room-state restoration now reach the native session/world boundary.
AreaHunters and S09 hunter selection are now restored in the direct Adventure
path; hunter death/respawn message integration, the remaining platform
render-loop, and full GUI adapters are still separate.
The Windows host's direct `-adventure -rom FILE [-save-slot 1..3] [-newgame]`
path now uses the same save reader, restores health/ammo/weapon state into the
local session, applies persistent room state to static and gameplay environment
records, rebuilds AreaHunters, materializes selected S09 hunter spawners, and
starts at the resolved checkpoint room. The remaining hunter death/respawn
message graph and the complete GUI remain later parity work.
The native room catalog now also carries every current story-room definition
(IDs 27-92) alongside the 27 multiplayer definitions (IDs 93-119). The
combined `find_room()` lookup is used by the Scene facade and room/map CLI
paths, and the supplied cartridge validates all 66 story rooms before the
multiplayer sweep. When a definition carries `NodePath`, `Room::load` also
parses and retains its native navigation table; the same run currently checks
2,758 story navigation nodes.
Model animation resources are now decoded alongside those room models: the
four managed group offset tables, all raw animation records, fixed/turn LUTs,
and the GuardBot1 UV data correction are retained in `model::AnimationResults`.
`Formats/model_instance.*` mirrors managed animation selection, frame stepping,
node hierarchy/matrix-stack construction, and material/texture/UV sampling.
The Windows OpenGL host now loads all eight hunter model sets, including the
managed Guardian-to-SamusAlt reuse, external `pal_01` recolor resources, and
each hunter's local/shared animation files. Player primitives are keyed by
hunter and form; node matrix IDs, UV transforms, animated texture/palette
rebinding, and animated material color/alpha are applied during submission.
`Assets/model_catalog.*` and `Metadata/entity_metadata.*` now mirror the
managed model/object/platform/door/jump-pad lookup boundary. The Windows
OpenGL host submits static room-entity models for doors/locks, platforms,
objects, jump pads/beams, item/artifact bases, and teleporters; shared
`*_TextureShare_img_Model.bin` and explicit `img_00` recolor resources are
combined before decode. The supplied cartridge audit covers all 106 unique
static-entity model dependencies with zero missing or invalid records. Full
shader/material parity and the remaining dynamic entity effects remain later
renderer work.
`runtime::BeamProjectile` now keeps the managed previous position, ten-point
trail history, draw-function ID, and DS color. The Win32/OpenGL path loads the
cartridge `iceShard`, `energyBeam`, `trail`, `electroTrail`, and `arcWelder`
resources and draws the corresponding projectile model or billboard, falling
back to a colored line when a resource is unavailable. Weapon visual metadata
now carries the managed draw/color/collision/muzzle columns; projectile
spawns and impacts produce native effect events, and the supplied ROM parses
the complete 243-file metadata effect catalog (including the common
33-resource weapon closure) with no missing files. The Win32 path resolves
particle model/node meshes, and non-mesh particles use the cartridge B8/C4/CC/D0
geometry families, rotation, and material repeat modes. The
`BeamEffectEntity` mapping covers the three model variants and NoSplat's two
effect substitutions; the Win32 path prefers the cartridge `iceWave` model for
effect 78 and otherwise uses a parsed-resource-scaled billboard. The native
Effects expression evaluator now covers the supported scalar/vector function
graph used by element actions. `Formats/effect_runtime.*` expands those
resources into element/particle lifetimes, spawn/update actions, acceleration,
collision callbacks, and child-effect requests; the dynamic entity graph
remains later parity work. The shared gameplay session now mirrors the frame-local
Scene.AddSingleParticle boundary as well: active projectile fuzzballs and the
early death burst resolve the cartridge particle nodes/materials, with an
untextured billboard reserved for missing optional assets.
`Strings` now parses normal/game-message/ScanLog tables with the managed
12-byte record layout, and the Win32 scan visor loads all 332 categorized
entries from the supplied ROM. Nearby visible room entities, items, and
enemies emit the matching lore/bioform/object/equipment/red icon variants;
target selection, the animated scan box, basic progress/out-of-range state, and
a basic ScanLog text panel are native; room collision line-of-sight validation
and the visor toggle are native as well. Scan completion pauses the native
scene and accept/cancel input updates or leaves the story logbook accordingly;
page-accurate dialog rendering and visor audio flow remain later HUD work. The
local scan control stays outside the
network intent bitfield and is supplied by keyboard `Q` or gamepad `X` while
the scan visor is active.
managed `src/MphRead` root files also have native source counterparts: the
root `Features`/`GameState`/`Messaging` modules implement persisted feature
values, the public match-stat/mode rule state, frame/trigger state, and a
fixed-size delayed queue; `Memory*` and
`Read` implement the portable bounds-checked data boundary; `Scene` and
`SceneSetup` connect that state to ROM-backed rooms; and `Renderer`, `Shaders`,
`Menu`, `Selection`, `Strings`, `Program`, and `Test` provide the portable
frontend/tooling boundaries. `GameState` now synchronizes fixed-step player
and team counters from the native snapshot, rebuilds mode-specific result
ordering, evaluates the scene-independent match clock and completion rules
for every supported multiplayer mode, and owns the story `EnterShip`/
escape-state reset mutations. The Win32 message sink forwards those process-wide
mutations before updating the fixed world and save image. These are functional native layers, not a claim
that every managed renderer, Avalonia screen, generated memory class, or
Android surface has reached parity.
The `MemoryClass` boundary now also carries the managed primitive accessors,
32-bit pointer representation, 12.4 fixed-point vector/matrix conversion, and
typed scalar/enum array views; all of them remain backed by the checked native
`Buffer` rather than raw host memory.
The
Windows preview also resolves the room's external texture/palette resource,
the Samus biped and alt-form resources, and the Win32 keyboard/mouse/gamepad
input-to-intent path. XInput is loaded dynamically and its raw flags are
converted to the same canonical Xbox-shaped button layout used by the managed
input layer. The preview now consumes the managed-compatible `controls.txt`
scalar settings for mouse sensitivity/inversion and gamepad dead-zone/look/Y
inversion, plus the twelve `pad_*` bindings, and forwards direct weapon
selection, zoom state, the native scoreboard, and the basic Battle match-flow
state machine. The latter owns the local timer/point-goal transition, blocks
same-team damage unless friendly fire is enabled, and adopts the network
`MatchState`/authority `MatchEnd` boundary. Pause/menu actions, the results
camera/sequence presentation, and an interactive binding editor remain outside
this boundary; the Win32 preview now has the basic ESC pause toggle,
`-mode`/`-time`/`-goal` test overrides, and keeps a network session alive while
paused.
The native frontend-independent chat log now matches the managed lifetime
contract (three visible lines, ten-second hold, one-second fade), and the
Win32 host routes `T`/text/Enter/Escape through it while sending and receiving
the additive `ChatPacket` network message. Full chat localization and the
Android soft-keyboard path remain outside this boundary.
The native settings reader also imports the managed `Savedata/settings.json`
match fields used by that preview, while explicit command-line values win.
The native `Mods/Launcher/Portable` save store reads and writes the managed
`Savedata/save###.json` slots, exposes the three-slot area/health/octolith
summary used by a launcher, and applies the same commit-time safety cleanup.
Its parser is intentionally dependency-free; a malformed slot is treated as
empty so one damaged save cannot prevent the other slots from being listed.
The match-flow mode table carries the managed time/point defaults, keeps
Survival on its lives rule, and exposes authoritative objective-time end hooks
for Defender and Prime Hunter. The native session now consumes ROM-backed
NodeDefense and OctolithFlag/FlagBase volumes for Nodes, Defender, Capture, and
Bounty scoring, and consumes the managed standard entity layouts for platforms,
objects, doors, trigger/area volumes, jump pads, morph cameras, teleporters,
lights, artifacts, camera sequences, and force fields. Active jump pads,
multiplayer teleporter re-entry, and story cross-room teleporter reloads are
wired into the fixed-timestep Scene/gameplay boundary. Player-facing
AreaVolume effects (damage, death, gravity, form
locks, and biped locks) and volume/automatic/threshold TriggerVolume dispatch
are also native, including parent/child activation links. Beam-driven trigger
checks, the remaining dynamic entity message graph, and the results
camera/sequence presentation are still being migrated. JumpPad control-lock
timing and replicated Frozen-state input/fire suppression are also enforced by
the native fixed-step path.
ItemSpawn records now drive native spawn delay/interval/count state, pickup
range and inventory effects, timed multiplayer death drops, and collected
message dispatch. The Win32/OpenGL host resolves all 23 managed standard item
asset names from the cartridge and renders active items with the native
spinning/floating model path; colored markers remain the missing-resource
fallback. The fixed-step session emits host-neutral sound cues for projectile
firing and impact, player damage/death/spawn/jump, item spawn/pickup, and
enemy damage/death. The Windows host dispatches those cues through the ROM
sound metadata and `SfxRuntime`, then pumps the WinMM output queue; complete
item pickup visual/effect sequencing remains renderer work.
The fixed room records are retained in a native polymorphic
`static_entities::World` owned by `Scene`; platforms, doors, objects, volumes,
jump pads, teleporters, flags, artifacts, lights, force fields, point modules,
and player/item spawns have lifecycle/update/message handlers. Authored parent
links are resolved after movement so children follow moving platforms and keep
volume geometry in world space. Relay trigger volumes forward incoming
messages to their parent and child using the same delayed Scene queue. This
keeps the room-level EntityBase boundary executable. The Win32/OpenGL host also
resolves the static model catalog against the cartridge, including shared
texture resources, and draws active static entities from that world. Player/
enemy behavior and the remaining beam/result message graph continue as
separate migration work.
The scene integration tests load all 27 standard multiplayer room definitions
from the native room catalog, classify every cartridge entity, decode the
typed standard entity records, validate their collision volumes, and decode
every room mesh display list. The real-cartridge run currently validates 1,284
entities across the 27 rooms, including 721 typed records.
The native `-maptest` command also loads a selected room from the real NDS,
simulates up to eight players, and can decode all room geometry and texture /
palette resources in the same run. Its machine-readable summary includes the
active native item count.
The Win32 host can record a connected session to the same FPDM stream: received
datagrams are recorded verbatim, while an authority adds its own Snapshot and
the local player adds its SlotIntent. Offline previews intentionally do not
create an empty recording.
The native headless replay command can then reopen a recording, load the room
named by its MatchState from the same NDS, apply its typed intents and
snapshots through `gameplay::Session`, and report the snapshot coverage and
no-snapshot gap used by the managed `DemoInfo -replay` check. The Windows host
also feeds the same records to its native room, camera, HUD, and scoreboard;
basic network spectator state is native: the headless check supports the
managed `-spectate`/`-rejoin` schedule, and the Win32 preview can enter, cycle
targets, and rejoin while replicating `FlagSpectating`. Full free-camera
movement, room-transition presentation, and Android playback remain to be
ported.
The real-ROM tests use temporary data only and never copy the cartridge into
the repository. Native netcheck demo output defaults under `export/_demos/`,
which is ignored along with the ROM extraction outputs.
A native HUD asset test now walks the managed virtual `_archives/...` paths in
the same real cartridge, decodes the 4bpp screen maps and OAM target-circle
objects for all seven hunter HUD sets, and the Win32 host uploads the selected
target circle plus helmet/visor data when a room is opened. This is a real
asset/parser and texture-upload path; the host also draws the ROM health/ammo
bar tiles and equipped weapon icon at the managed hunter-specific positions.
It is not a claim that the full managed HUD is already reproduced: animated
object instances, every meter/icon and mode overlay, the complete five-layer
state machine, page-accurate scan dialog rendering, and pause masking remain
migration work.
A native Nitro Composer reader now covers SDAT/SSEQ/SBNK/SWAR/SWAV. It
resolves INFO/SYMB/FAT relationships, decodes PCM8/PCM16/IMA-ADPCM samples,
and is exercised against the real `data/sound/sound_data.sdat` in the supplied
ROM. The native `sound::Catalog` also reads the custom MPH sound tables, and
`-soundinfo ROM_OR_DIR` reports their counts. A dependency-free
`sound::SoftwareMixer` now handles PCM8/PCM16 voice queues, loop points,
pitch/gain, distance attenuation, and stereo placement up to an interleaved
float block. `Mods/Sound/sfx_mixer.*` exposes the managed buffer/source
lifecycle and `pump()` converts that block to signed stereo PCM. The MinGW
Windows build now has a fixed-queue WinMM adapter; the host can keep it
no-device-safe while `Sound/sfx_runtime.*` connects lazy PCM sample playback,
encoded sample/script dispatch, 30 Hz script timing, source spatial updates,
gain, pitch, script pan, and DGN volume/pitch curves. `Sound/sseq_player.*` now
executes the core SSEQ track interpreter (allocation/opening, waits, jumps,
calls/loops, variables, tempo and note controls) and renders basic PCM/PSG/noise
voices through a portable float boundary. Full DS channel allocation/envelope/
interpolation, environment voice arbitration, per-frame track-volume mixing,
and OpenAL/Android output remain later parity work. `Sound/music_runtime.*`
now consumes the controller commands, renders a bounded selected-track SSEQ
buffer, and loops it through the native software/WinMM mixer; the Windows host
starts it for the room's first music track. `Sound/music.*` mirrors the managed
`Music` state machine for track selection, queued sequences, encounter
suspension, escape/event tempo ramps, volume fades, and per-track fades,
emitting the commands consumed by that runtime.
A native build passing only these checks is not evidence of gameplay parity;
the final retirement gate is protocol, save/ROM, renderer, launcher, Android,
and network-harness coverage for every supported target.
