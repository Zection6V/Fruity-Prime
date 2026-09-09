# C++ source tree

`src/MphRead.Native` is the native counterpart of `src/MphRead`.  Its
directory names deliberately match the current C# tree:

`Assets`, `Entities`, `Export`, `Formats`, `HUD`, `Metadata`, `Mods`, `Sound`,
`Testing`, and `Utility`.

The source-tree contract currently pairs all 272 managed `.cs` files with a
same-relative native `.cpp`/`.hpp` boundary. The 83-file `Entities` tree is
implemented by native translation units and contains no name-only boundary
stubs. The former non-entity boundary units now contain native format,
utility, and test adapters as well; remaining behavioral gaps are tracked by
their subsystem instead of being hidden behind name-only files.

The root-level files shown by the managed `src/MphRead` project are also
represented here. `Features`/`GameState`/`Messaging` carry the runtime
settings, public match-stat/mode rule state, global frame/trigger state, and
delayed message queue; `Memory`,
`MemoryArrays`, `MemoryClasses`, and `Read` provide the bounds-checked native
data boundary. `Scene`/`SceneSetup` wrap the decoded room and fixed-step
session, while `Renderer`/`Shaders` expose a graphics-API-independent frame
and verified GLSL sources. `Menu`, `Selection`, `Strings`, `Program`, and
`Test` are the corresponding portable frontend/tooling models. Their
implementations are the root-level `*.cpp` files, and are included in both
the direct MinGW archive and the source-facing CMake target.

`GameState` now also owns the native match-state boundary used by the scene:
it prepares the mode defaults, synchronizes authoritative player counters from
the network snapshot, rebuilds free-for-all/team result ordering, and evaluates
the scene-independent clock, point, Survival, Defender, and Prime Hunter end
conditions. Story `EnterShip` and escape-state reset mutations are also kept in
this boundary; the Win32 host forwards gameplay messages back through it before
updating the fixed world/save. `Scene` feeds that state on every fixed step,
while the Win32 host keeps the existing network `MatchState` handshake as the
authority boundary.
The camera/results presentation and the remaining per-entity message graph are
still separate layers.

The memory boundary now includes the managed `MemoryClass` primitive surface:
32-bit DS pointers, signed/unsigned integer and float reads/writes, BGR color
triples, 12.4 fixed-point Vector3/Vector4/Matrix4x3 values, and typed scalar
and enum array views. These views never expose host pointers and reject an
out-of-range access through the same checked `Buffer` used by the format
readers.

`Formats/model_format.*` now also decodes the managed `Read.LoadAnimation`
resource: all four group offset tables, node/material/texture-coordinate/
texture animation records, fixed-point and turn-based LUTs, and the authored
GuardBot1 UV correction are retained in `model::AnimationResults`. `Scene`
loads the room animation resource together with its model. `Formats/model_instance.*`
mirrors the managed playback state, group selection, frame stepping, node
hierarchy, matrix stack, material, texture, and UV updates. The Windows
OpenGL host now loads all eight hunter model sets (including Guardian's
SamusAlt reuse), their external `pal_01` recolor resources, and the managed
local/shared animation streams. Player primitives are keyed by hunter and
form, so matrix IDs, UV transforms, animated texture/palette rebinding, and
animated material color/alpha reach the corresponding model at draw time.
`Assets/model_catalog.*` and `Metadata/entity_metadata.*` now provide the
managed model/object/platform/door/jump-pad lookup boundary. The Windows
OpenGL host submits the cartridge models for static room entities (doors and
locks, platforms, objects, jump pads/beams, item/artifact bases, and
teleporters) and advances their native entity world. Shared
`*_TextureShare_img_Model.bin` resources and explicit `img_00` recolor models
are combined before texture decode, matching the managed resource boundary.
The supplied cartridge audit decodes all 106 unique static-entity model
dependencies with no missing or invalid model records. Full shader/material
parity and the remaining dynamic entity effects are separate renderer work.
`runtime::BeamProjectile` now retains the managed previous position and
ten-point trail history, weapon draw-function ID, and DS color. The Windows
OpenGL host resolves the cartridge `iceShard`, `energyBeam`, `trail`,
`electroTrail`, and `arcWelder` models and uses them for projectile sprites
and billboards, with the colored line retained as a missing-resource fallback.
Weapon visual metadata now also carries the managed draw/color/collision/muzzle
columns. Native projectile state emits local muzzle/impact effect events, the
complete 243-file metadata effect catalog is parsed from the supplied ROM
(including the common 33-resource weapon closure), and the Win32 host resolves
effect particle models and named nodes for mesh draws. Non-mesh draws now use
the cartridge B8/C4/CC/D0 geometry families, particle rotation, and material
repeat modes. It prefers the cartridge
`iceWave` model for effect 78. The
`BeamEffectEntity` type mapping covers `iceWave`, `sniperBeam`,
`cylBossLaserBurn`, and the two managed NoSplat substitutions. The native
Effects evaluator now executes the supported scalar/vector expression functions
used by those element actions. `Formats/effect_runtime.*` expands parsed
resources into element/particle lifetimes, spawn/update actions, acceleration,
collision callbacks, and child-effect requests; the dynamic entity graph remains
separate parity work. `Sound/sseq_player.*` now covers the core SSEQ timeline and
basic PCM/PSG/noise rendering. The shared gameplay session now
also mirrors the renderer's frame-local
Scene.AddSingleParticle queue: in-flight fuzzballs and the early death
particle burst use the cartridge particles/fuzzBall and
deathParticle/death materials, with a colored billboard only as an asset
fallback.
`Strings` now parses the ROM's normal, game-message, and eight-byte-header
`ScanLog` tables using the managed 12-byte entry layout. The Win32 scan visor
loads all 332 real-ROM scan categories and queues the matching lore, bioform,
object, equipment, and red icons for nearby visible items, enemies, doors,
platforms, artifacts, force fields, and teleporters; discovered entries use
the dim icon variants. Target selection, the animated target box, basic
progress/out-of-range presentation, and a basic ScanLog text panel are now
drawn by the Win32 HUD. The room collision line-of-sight check and visor
toggle are also wired. Scan completion now pauses the native scene and offers
local confirm/cancel handling before updating the story logbook; page-accurate
dialog rendering and visor audio flow remain separate parity work. The local scan
control is kept outside the network intent bitfield and is supplied by
keyboard `Q` or gamepad `X` while the scan visor is active.
Parsing and playback are covered by synthetic four-group tests and the
supplied real ROM.

`Formats/movie.*` now decodes the managed VXDS movie format used by
`Formats/Movie.cs`: its header tables and LPC audio metadata, MSB-first video
bitstream, CAVLC residuals, inter-frame prediction, DCT/YUV reconstruction,
and 128-sample audio frames. The real-ROM movie test exercises
`movies/01_bot.vx`. `movie::Player` adds the scene-independent decoded
timeline, pause/seek/end state, and the Windows host presents `-movie` frames
with aspect-preserving letterboxing; scene-triggered scheduling and platform
audio are separate layers.
The native utility also exposes `-movieinfo` and `-export movie`, producing
numbered PNG frames and a PCM16 `audio.wav` through the shared exporter.

The migration is staged, but the currently migrated C++ sources, headers,
and tests are now physically in this tree.  The C# project remains available
as a behavior reference.  The compatibility `native/` build entry compiles
these files directly, so there is no duplicate implementation directory.
New native modules should be added to the matching directory here.

The gameplay port is intentionally not a monolithic `gameplay.cpp`.  That file
now contains only session construction and fixed-step ordering.  Player
lifecycle/input, projectiles/effects, room environment/message dispatch,
objectives, story-save state, snapshots, combat, items, and enemy dispatch live
in their own `Entities/gameplay_*.cpp` modules.  This keeps the native tree
auditable against the managed `GameState`, `Scene`, and `Entities` boundaries
and prevents a new entity port from being hidden in a catch-all file.

The fixed room and dynamic entity implementations follow the same rule. Their
definitions live in class-oriented files under `Entities/` (`PlatformEntity`,
`ObjectEntity`, `RoomEntity`, `BeamProjectileEntity`, `EnemySpawnEntity`, and
the other matching classes), while shared declarations remain in the public
aggregate headers for existing consumers. `Entities/Players/PlayerEntity.cpp`,
`PlayerInput.cpp`, and `PlayerCollision.cpp` carry the corresponding session
boundaries. This aligns source ownership without pretending that the remaining
renderer, animation, and story-message behavior is already 1:1.

`Entities/Players/player_profile.*` now carries the managed eight-entry
`PlayerValues` table in its authored 12.4 fixed-point form and mirrors
`AvailableArray`. The fixed-step session uses each hunter's movement scale
and alt-form strafe rule. `Entities/Players/player_ai.*` now provides the
native bot decision boundary: nearest-opponent selection, team filtering,
distance-aware weapon selection, strafing, level-scaled firing cadence, and
hard-level jump timing all feed the same `gameplay::Input` path as a player.
Its state layer also mirrors the managed `AiFlags2/3/4`, button down/up
histories, 25-entry aggro table, 20-depth execution contexts, and weighted
personality path transitions. Visibility currently uses a conservative
active-player matrix until it can share the scene collision query.
Unsupported managed Func3 IDs are counted by the native state instead of being
silently claimed as implemented; the full Func1/2/3/4 action graph and all
gameplay side effects remain parity work.

`Mods/Render/render_mods.*` now mirrors the renderer-side preview camera and
the DS-tiled HUD icon ink-bound calculation. The preview camera can be
installed through `scene_runtime::Scene::set_preview_camera`, so map preview
and future native launcher surfaces use the same look-at fallback as the
managed mod.

`Mods/Launcher/Gui/gui_model.*` now provides the toolkit-neutral launcher page
state machine: splash/first-run game-file gating, home/map/demo/settings/pause
navigation, selection state, and Back history. The existing Win32 front end
uses the same `Selection` contract; Avalonia widgets and the Android head are
still separate frontend work.

`Mods/Launcher/Portable/text_launcher.*` adds the terminal front end for the
same launcher choices. It reads/writes `launcher.txt`, performs the first-run
`.nds` setup through `GameFiles`, lists story save slots and multiplayer rooms,
and returns a resolved `LaunchPlan`; Continue/New game also resolves the
managed save slot's checkpoint room and Story mode. `Mods/Launcher/Portable/
match_start.*` then turns offline, story, and network plans into shared match
rules, gameplay configuration, and a local/bot roster; the Windows OpenGL host
uses this boundary as well. Story inventory and packed room-state restoration
now reach both the static world and the gameplay spawners/volumes; AreaHunters
and S09 hunter selection are restored into the direct Adventure roster. The
native session now retains authored PlayerSpawn active/availability/orientation
records and uses the managed safe-distance/cooldown selection for ordinary
respawns. The first authored enemy controllers (`00_WarWasp`, `01_Zoomer`,
`02_Temroid`, the shared `12_Geemer` surface path, `10_BarbedWarWasp`,
`11_Shriekbat`, the S06 `18_AlimbicTurret`, and lethal-cloud `16_Blastcap`)
now decode/use their
cartridge behavior in the native fixed-step session. Prime Hunter's
twenty-frame periodic damage is also native. The remaining hunter
death/dialog graph and the actual Avalonia/Android widgets and platform
render-loop adapters remain separate work.

The Windows host also accepts `-adventure -rom FILE [-save-slot 1..3]
[-newgame]` for a direct native story launch. It loads the compatible save
slot, applies health/ammo/weapon state to the local session, and starts at the
checkpoint room; an empty or invalid slot follows the managed new-game default.
Its room-state records are applied to persistent platforms, doors, artifacts,
spawners, and environment volumes before the first tick. The command is
intentionally local-only. It also rebuilds the managed AreaHunters masks and
materializes selected S09 hunter spawners as native bots, but does not yet
claim the full story hunter death/respawn/message graph.

`Mods/Render/render_options.*` is the shared native render-settings port for
resolution scale, lighting, cel shading, fog, texture filtering, and the
associated clamped command-line values. It is independent of the OpenGL host,
so later window and renderer backends can consume the same settings.
The Windows host now consumes the same display boundary: `launcher.txt`'s
`window_mode` is applied at startup, `-fullscreen`/`-borderless` and
`-windowed` override it, and F11/Alt+Enter toggles borderless mode at runtime.
`-nohelmet` suppresses the helmet layers, while `-prohud on|off`,
`-cel on|off`, `-fog on|off`, `-fps on|off`, `-celbands N`, `-celedge N`, and
`-resolution-scale N` reach the native HUD/render options before the first
frame.

The `Formats` boundary now includes the managed `NodeData` reader for version
6 multiplayer navigation tables and First Hunt version 0 tables. It preserves
the three nested offset levels, shared value table, node colors/transforms,
and closest-node lookup; the small `Culling` value boundary mirrors the
managed room-node/frustum references. The supplied ROM test parses 116
supported node-data files and explicitly reports the one known version 4 file
as unsupported, matching the current C# behavior.

The native `Entities` runtime now also carries the `EnemySpawnEntity` lifecycle
boundary: initial/cooldown waits, player-distance suspension, batch and
concurrent/total limits, out-of-range destruction accounting, and
`Activate`/`SetActive` messages. `gameplay::Session` turns each accepted spawn
into an `EnemyState` and exposes damage/destruction hooks. Story completion now
records the room state, enemy-encounter bit, and Cretaphid/Slench/Gorea escape
boss state in the native save when a spawner is exhausted. `SceneSetup` now
also mirrors the managed AreaHunters update and S09 random-hunter selection,
including persisted encounter blocking, and the direct Adventure host
materializes the selected hunter bots. PlayerSpawn state and safe respawn
selection are native, and authored controllers for `00_WarWasp`, `01_Zoomer`,
`02_Temroid`, the shared `03_Petrasyl1`-`06_Petrasyl4` flying controller,
`11_Shriekbat`, `12_Geemer`, `10_BarbedWarWasp`, `35_Voldrum2`,
`36_Voldrum1`, `23_PsychoBit`, `39_FireSpawn`, `18_AlimbicTurret`,
`37_Quadtroid`, `38_CrashPillar`, both `46_LesserIthrak`/`47_GreaterIthrak`
variants, the static S07 `51_CarnivorousPlant` controller, and a staged S10
`41_Slench` shield/volley/roam controller are native. The Slench profile keeps
the room-specific phase and projectile data; `45_SlenchTurret` now retains the
authored S10.Index pairing and independent shot/salvo timers, and its native session now
creates the linked shield plus three synapse children with parent-following
transforms, shield damage forwarding, synapse health/heal/death/respawn
states, and child-prioritized beam collision. The original animation
choreography, nest presentation, and full story message graph remain separate
parity work.
Cross-room teleporter
requests now reload the destination room in the
native Scene and carry the player to its target entity. The other individual
enemy AI classes, story death/dialog, and the full parent
transform/message graph remain separate migration work.

`runtime::EntityPool` now exposes the same queued-message boundary for dynamic
objects. Raw cartridge `SetActive`, `Damage`, `Impact`, `Death`, `Destroyed`,
and turret activation messages reach item instances, bombs, enemies,
spawners, and half-turrets through `Scene`; their lifetime and damage state
is updated before inactive instances are reclaimed. The complete beam
collision/effect graph remains separate parity work.

`Entities/Enemies/enemy_catalog.*` now makes the managed `Entities/Enemies`
surface explicit for all 52 cartridge enemy IDs, while the 42 managed enemy
source files each have a same-named native `.cpp`/`.hpp` pair.  The session
dispatches every managed enemy type explicitly.  The catalogue marks each
file as an implemented controller, shared controller, partial Gorea
controller, or spawner boundary; those statuses are deliberately not reported
as 1:1 completion.  The remaining gaps are the original animation
choreography, full linked-boss/message graphs, and behavior details still
marked partial/shared in the per-file descriptors.

`Entities/CamSeq` is represented by the native camera sequence format and
playback boundary. It reads ROM-backed `cameraEditor/*.bin` data, reproduces
timing/easing/FOV/roll, resolves entity-relative poses through a live-scene
callback, and routes typed keyframe messages, fades, and form locks through
`scene_runtime::Scene`. `entities::cam_seq::CameraSequenceEntity` now adds
the entity-level activate/deactivate, delay, handoff timeout, per-entity form
flags, loop, and end-message lifecycle. Player death/dialog guards and
complete SFX/music orchestration remain separate parity work; the first native
ROM-backed SFX and room-music paths are now connected in the Windows host.

The fixed room records are also materialized by
`entities::static_entities::World`, an `EntityBase`-style polymorphic store
for platforms, objects, doors, volumes, jump pads, teleporters, flags,
artifacts, lights, force fields, point modules, and player/item spawns. It is
created and cleared with the native scene, receives targeted/broadcast
messages, runs each fixed step, and resolves authored parent links used by
moving platforms, objects, jump pads, artifacts, and item spawns. Trigger
relay volumes preserve the C# parent/child message boundary and enqueue their
forwarded cartridge message on the Scene queue. Player/enemy simulation
remains in `gameplay::Session`, while First Hunt records that do not yet have a
standard typed payload use a safe addressable fallback until their individual
behavior is migrated.

`Metadata`/`Entities` also contain the current C# room metadata boundary: all
66 story rooms (IDs 27-92) and all 27 multiplayer rooms are available through
`story_rooms()`, `multiplayer_rooms()`, and the combined `find_room()` lookup.
The supplied cartridge loads every story definition and reports 7,686 meshes,
2,895 entities, and 2,758 navigation nodes before loading the multiplayer
catalog. `Scene`,
`-roominfo`, and `-maptest` now use the combined lookup, so story definitions
are no longer limited to the multiplayer-only entry point.

`Export` now has the same tool-facing split as the managed tree: decoded
models can be written as OBJ/MTL or static COLLADA 1.4.1, and all material
referenced texture/palette pairs can be emitted as RGBA PNG files. The CLI
entry points are `-model-export-obj`, `-model-export-collada`, and the shared
model/texture export library remains independent of the game window. The
bidirectional `Formats/model` boundary also has a native writer: animation,
node/material/display-list tables, inline or separate FPTX/FPAL resources, and
none/capped/uncapped bounds are serialized by `RepackModel` and exposed as
`-model-repack`. Synthetic byte-round trips and a real-cartridge Samus model
round trip cover the writer.
The HUD category contains a renderer-independent player view model, a
ROM-backed DS 4bpp character-map decoder, and an OAM/object decoder for the
cartridge HUD assets. `assets::Store` also resolves the managed virtual
`_archives/...` paths by opening the corresponding ROM archive, so the Windows
host can load each hunter's target circle and helmet/visor layers from the
real NDS rather than from copied development files. The host uploads those
decoded layers through the Win32/OpenGL path and still keeps the native text,
score, weapon, and inventory readouts on top. The ROM-backed object path also
loads the hunter-specific health/ammo bar tiles and weapon icon frames, using
the same vertical/horizontal meter layouts as the managed HUD. Active native
story enemies now resolve their cartridge model through the enemy catalog and
are submitted with the same model-instance animation path; a family-coloured
marker remains the fallback when a model is unavailable. Standard item
instances likewise resolve all 23 managed item asset names and use the
spinning/floating model path, with colored markers as the fallback. Full animated
five-layer HUD composition, every HUD object and mode-specific overlay,
OpenAL/Android playback, and the complete launcher are still later parity
work. The Sound category also contains a bounds-checked native Nitro Composer
reader: SDAT INFO/SYMB/FAT records, SSEQ payloads, SBNK instrument ranges,
SWAR wave tables, and SWAV PCM8/PCM16/IMA-ADPCM decoding are covered by
synthetic tests and the supplied real cartridge. `Sound::SoftwareMixer` remains
the portable voice core, while `Sound::Win32AudioOutput` provides a fixed
WinMM PCM queue for MinGW/Windows and `Mods/Sound/sfx_mixer.*` can pump mixed
blocks into it. When a ROM-backed room is started, the native Windows host
initializes the catalog/runtime, dispatches fixed-step gameplay sound cues for
shots, impacts, damage/death, spawns, jumps, pickups, and enemy hits, and
pumps the WinMM queue once per rendered frame. The path remains safe when no
audio device exists. `Sound/sseq_player.*` now executes the core SSEQ track
interpreter (allocation/opening, waits, jumps, calls/loops, variables, tempo,
and note controls) and renders basic PCM/PSG/noise voices through a portable
float boundary. `Sound/music_runtime.*` now consumes those music commands,
renders a bounded SSEQ buffer per selected room track, and loops it through the
native software mixer; the Win32 host starts this path alongside gameplay SFX.
Full DS channel allocation/envelope/interpolation, streaming voice arbitration,
per-frame track-volume mixing, OpenAL/Android output, and complete renderer
integration remain later parity work. `Sound/music.*` mirrors the managed
`Music` state machine for track selection, queued sequences, encounter
suspension, escape/event tempo ramps, volume fades, and per-track fades, and
emits the commands consumed by the native runtime.
The native `sound::Catalog` also reads the complete MPH table set
(`SNDSAMPLES`, `WFSSNDSAMPLES`, both select lists, `SND3DLIST`, `SNDTBLS`,
`ASSIGNMUSIC`, `INTERMUSICINFO`, `SFXSCRIPTFILES`, and `DGNFILES`) and exposes
their counts through `FruityPrimeServer -soundinfo ROM_OR_DIR`.
`sound::SoftwareMixer` also implements the dependency-free SFX voice boundary
(PCM8/PCM16 conversion, buffer queues, looping, pitch, gain, distance
attenuation, and stereo placement) up to an interleaved float output block.
`Mods/Sound/sfx_mixer.*` now exposes the managed SfxMixer buffer/source
lifecycle and safe open/close boundary over that mixer. `pump()` converts one
mixed float block to signed stereo PCM and queues it through the Windows
adapter when a default WinMM device is available; it remains a no-device-safe
portable boundary elsewhere.
`Sound/sfx_runtime.*` now connects the catalog to that voice boundary: PCM
samples are decoded and cached on first play, encoded sample/script IDs are
dispatched, 30 Hz SFX script delays are scheduled, and source distance,
volume, pitch, and pan are refreshed each update. This covers the portable
sample/script path used by `SoundSource`; DGN volume/pitch curves are also
evaluated at runtime. `gameplay::Session::sound_events()` is the host-neutral
bridge from simulation to these runtime calls. Environment voice arbitration
and full live DS channel scheduling remain separate work; `Sound/sseq_player.*`
provides the portable core timeline/basic voice renderer, `Sound/music.*`
supplies the shared music state transitions, and `Sound/music_runtime.*`
connects the selected sequence to the native software/WinMM path with bounded
looping PCM.
The utility exposes that boundary as `FruityPrimeServer -sseqinfo ROM_OR_DIR`
with optional `-sequence N` and `-seconds N` arguments.

`Mods/MapGen` now reads the checked-in JSON recipe format used by
`maps/arena/arena.json`, including comments, trailing commas, brush geometry,
spawns, jump pads, and multiplayer items. The native brush generator emits a
valid model, MPH collision table, version-2 entity table, empty animation
resource, and version-6 navigation node table; `-mapgen` writes these into the
ignored `export/_mapgen` directory by default. The importer also reads Quake 3
version-46 `.bsp` files and deflated `.pk3` archives, converting polygon and
patch surfaces, clipped brush collision, spawn points, trigger-push pads, and
supported multiplayer pickups. When the recipe names an FPTX pack, its
palette-indexed images and BSP UVs are embedded in the generated model;
`-q3convert` also creates that pack from PK3 images on Windows through WIC.
Texture borrowing from a ROM remains separate asset work, while an explicit
FPTX pack is the portable/bundled path on other platforms.

Build the C++ tree from this directory's parent with MinGW:

```powershell
cmake -S src -B src/build-mingw -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe `
  -DCMAKE_BUILD_TYPE=Release
cmake --build src/build-mingw --parallel 4
$env:PATH = "C:\mingw64\bin;$env:PATH"
ctest --test-dir src/build-mingw --output-on-failure
```

The existing `native/build-mingw.ps1` remains the short entry point for the
same implementation and its real-ROM tests.  The ROM stays outside the
repository; generated extraction, export, and test output is ignored by the
root `.gitignore`.
