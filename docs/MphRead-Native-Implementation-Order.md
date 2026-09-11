# MphRead.Native strict C# implementation order

Status: planning baseline for develop2.

This document is the dependency-closure plan for:

~~~text
src/MphRead/**/*.cs -> src/MphRead.Native/**/*.hpp + src/MphRead.Native/**/*.cpp
~~~

The C# source is the only behavioral specification. Existing Native code is
evidence to audit, not an authority. A matching filename, a successful
translation-unit compile, or a previous audit result does not prove parity.

This plan is the synthesis of two independent complete architecture reviews of
the same repository snapshot. They agree that the existing dependency material
is a coverage inventory, not an implementation order; that same-namespace
references and C# partial types must be resolved from source; and that
Program.Main is the real process entry. The selected first parallel leaves are
ChatFont.cs and StylusZone.cs because they have the smallest deterministic
runtime surfaces. Shaders.cs is the next independent W1 leaf.

## 1. Frozen scope and snapshot

- Repository: Zection6V/Fruity-Prime
- Branch: develop2
- Reviewed commit: 9e9eb0e7267534faac044f0bdd36d17e00c97735
- Source: src/MphRead
- Native target: src/MphRead.Native
- C# inventory: 302 files, excluding generated src/MphRead/obj/**
- Existing Native pairs: 48 hpp/cpp pairs
- Missing Native pairs: 254

The four dependency documents were added on top of source commit
2da573aa860507eab7150dfd3a69667528721063; no source-file change occurred
between that parent and the reviewed commit.

The final structural invariant is:

~~~text
src/MphRead/<relative>.cs
  <-> src/MphRead.Native/<relative>.hpp
      src/MphRead.Native/<relative>.cpp
~~~

Known relocation:

~~~text
C#:     Mods/Launcher/Portable/SetupProgress.cs
Native: Mods/Launcher/SetupProgress.hpp
        Mods/Launcher/SetupProgress.cpp
~~~

The final location must be:

~~~text
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.cpp
~~~

Temporary forwarding includes are migration aids only and are not the final
one-for-one layout.

## 2. Non-negotiable migration contract

For every C# file:

1. Read the entire C# file at the pinned revision.
2. Read the complete existing Native hpp/cpp pair, if any.
3. Resolve every directly used C# type, member, partial declaration, static
   initialization dependency, and platform condition.
4. Translate behavior, not merely names or signatures.
5. Compare Native results against a C# oracle before PARITY-PASS.

The Native side must not add gameplay, protocol, rendering, UI, update, timing,
or error policy; fallback, validation, normalization, clamping, caching,
retry, ownership, state, or defaults absent from C#; a replacement event bus,
network abstraction, service framework, or broad .NET substitute; a synchronous
implementation of an asynchronous C# operation; or arbitrary no-op bodies used
only to make a build pass.

Allowed plumbing is limited to language/platform translation: forward
declarations and include placement; one canonical declaration for a C# partial
type; exact value/reference/nullable/enum/struct representation; and thin
OpenTK, GL/GL ES, OpenAL, Avalonia, filesystem, process, HTTP, socket, task,
timer, and external-library adapters.

Every adapter must preserve observable C# widths, defaults, ordering, exception
type/timing, cancellation, thread affinity, blocking behavior, path and
culture rules, encoding, packet bytes, and exit status.

WinMain.cpp and any Native main.cpp are host adapters only. They contain no
application policy and do not count as extra C# specifications.

## 3. Why the current dependency indexes are insufficient

The dependency map and the two C# indexes are useful coverage inventories, but
not a semantic graph:

- same-namespace symbols can be used without an internal using;
- inheritance, member/static access, partial declarations, reflection, and
  conditional compilation are not captured by namespace extraction;
- the indexes contain lexical artifacts such as private, struct, is, makes,
  so, and source text in a using column;
- a Native include can point to an absent Native counterpart;
- paired means only that hpp/cpp files exist.

Verified hidden edge:

~~~text
Mods/Input/GamepadState.cs -> Mods/Input/PadBindings.cs
~~~

PadBindings.cs uses GamepadButtons declared by GamepadState.cs without a useful
internal-using edge. Future scheduling must inspect actual declarations and
references; generated tables are never the sole reason to schedule or accept a
file.

## 4. Partial types and SCCs

Scene is one C# partial type contributed to by:

~~~text
Scene.cs
Messaging.cs
Renderer.cs
Mods/Render/PreviewCamera.cs
Mods/Render/PreviewPass.cs
~~~

One canonical C++ Scene declaration must own the complete member surface.
Each source file still gets its own colocated pair, and each cpp retains the
bodies attributable to that C# file. No partial header defines a second Scene.

PlayerEntity is one C# type contributed to by these 21 files:

~~~text
Entities/Players/PlayerAi.cs
Entities/Players/PlayerCamera.cs
Entities/Players/PlayerCollision.cs
Entities/Players/PlayerDialog.cs
Entities/Players/PlayerDraw.cs
Entities/Players/PlayerEntity.cs
Entities/Players/PlayerHud.cs
Entities/Players/PlayerInput.cs
Entities/Players/PlayerPause.cs
Entities/Players/PlayerProcess.cs
Entities/Players/PlayerScan.cs
Entities/Players/PlayerSound.cs
Mods/Chat/PlayerEntityChatHud.cs
Mods/Network/PlayerEntityNetAim.cs
Mods/Network/PlayerEntityNetHud.cs
Mods/Render/PlayerEntityAmmoClear.cs
Mods/Render/PlayerEntityEndScreen.cs
Mods/Render/PlayerEntityIconBounds.cs
Mods/Render/PlayerEntityProHud.cs
Mods/Render/PlayerEntityStylusHud.cs
Mods/Render/PlayerEntityVoteHud.cs
~~~

PlayerEntity.hpp is the canonical declaration owner. The existing standalone
declaration in Mods/Render/PlayerEntityIconBounds.hpp must be folded into this
convention; isolated compilability is not a valid reason for duplicate classes.

The Metadata partial surface is:

~~~text
Metadata/Enemies.cs
Metadata/FrontendMeta.cs
Metadata/Metadata.cs
Metadata/Player.cs
Metadata/Rooms.cs
Metadata/SoundMeta.cs
Metadata/Weapons.cs
~~~

It needs one declaration and one exact static-initialization model. The Repack
and RepackCollision partials are:

~~~text
Utility/RepackEntity.cs
Utility/RepackModel.cs
Mods/MapGen/RepackAccess.cs

Utility/RepackCollision.cs
Mods/MapGen/RepackAccess.cs
~~~

Freeze one declaration for each partial type before Wave 9. A declaration-only
aggregation header is allowed only when necessary for C++ one-definition
ownership and must contain no alternate behavior.

For every SCC: inspect all members, freeze the declaration/adapter, implement
each source body, run aggregate C#-oracle tests, and only then expose the SCC
as a prerequisite. Forward declarations may break include cycles only where a
complete type is not required. void pointers, integer handles, dummy returns,
duplicate classes, and invented services are not cycle solutions.

## 5. Real process entry and startup closure

The actual process entry is not inside the launcher:

~~~text
Program.Main(args)
  -> ConsoleSetup.Run()
  -> Windows ConsoleWindow.Prepare(args)
  -> ModEntry.TryHandleHeadless(args)
       -> GuiLauncher.TryRun() when the GUI path is enabled
       -> TextLauncher.Run() on direct text path or GUI fallback
  -> setup validation / CheckSetup
  -> argument parsing
  -> ModEntry.TryHandle(args)
  -> normal menu, read, renderer, export, or game path
~~~

On Windows/macOS, ordinary no-argument launch is a headless launcher decision.
GuiLauncher.TryRun is the GUI launcher's logical entry, not process Main. A GUI
probe failure falls back to TextLauncher.Run according to C# conditions.
MPHREAD_SERVER removes the GUI path and retains dedicated-server command,
shutdown, socket, update, and exit behavior.

All launcher, update, network, map, settings, and headless dependencies must
be real before ModEntry.cs. ModEntry.cs must be accepted before Program.cs.
ProgramException may be declared early when an exact throwing contract needs
it, but that does not complete Program.Main.

## 6. Runtime and platform contracts

The dependency closure must explicitly test the used subset of .NET 9 strings
and UTF-16, culture, collections, nullable values, structs, enums, checked
arithmetic, reflection, tasks, cancellation, processes, filesystem,
compression, JSON, sockets, and timers; OpenTK 4.9.4 mathematics, GL,
OpenAL, GLFW input and callbacks; Avalonia 11.3.11 setup/dispatcher/lifetime;
ReFuel.StbImage, NcsfPlay, SoundFlow, OpenAL Soft, and direct packages; and the
Android aliases GL -> Mods.Render.GlEs, AL -> Mods.Sound.AlEs, and
ALC -> Mods.Sound.AlcEs.

Keep these gates separate:

1. deterministic C#/Native value and state parity;
2. adapter call/state traces;
3. visual/audio output;
4. desktop client build;
5. dedicated-server build;
6. Android/shared-adapter compile and behavior;
7. end-to-end process behavior.

A desktop build alone is never a complete gate.

## 7. Existing Native baseline

The 48 pairs are provisional. Recorded PASS examples, all of which re-enter
the appropriate wave gate, are:

~~~text
Formats/Enums.cs
Formats/Frontend.cs
Mods/Branding.cs
Mods/ConsoleWindow.cs
Mods/Credits.cs
Mods/Headless.cs
Mods/Input/TouchSettings.cs
Mods/MapGen/MapTexturePack.cs
Utility/Console.cs
Utility/Output.cs
~~~

Recorded FAIL/NO-OP entries include:

~~~text
Mods/Input/GamepadState.cs
Mods/Input/PointerInput.cs
Mods/Input/SyntheticInput.cs
Mods/MapGen/BuiltMap.cs
Mods/MapGen/RepackAccess.cs
Mods/ModEntry.cs
Mods/Render/FrameTiming.cs
Mods/Render/FrameTimingCheck.cs
Mods/Render/PreviewCamera.cs
Mods/RenderOptions.cs
Mods/ThumbnailMode.cs
Program.cs
~~~

All other pairs remain PAIRED-UNAUDITED unless a whole-file audit proves
otherwise. Old ledger path labels must not be reused as current source paths.

## 8. Wave order and gates

Wave numbers are final behavior-completion tags. Declaration-only work for a
later partial type may happen earlier, but does not complete that C# file.

| Wave | Scope | Count | Prerequisite |
|---|---|---:|---|
| W0 | snapshot, ABI/runtime rules, partial ownership, test matrix | 0 | snapshot |
| W1 | deterministic leaves and simple pairs | 18 | W0 |
| W2 | runtime/value/platform primitives and low-level utilities | 36 | W0 + W1 contracts |
| W3 | catalogs, text, metadata, standalone exports/extraction | 10 | W1-W2 |
| W4 | Formats/Scene/Entity contract SCC | 16 | W1-W3 + declarations |
| W5 | audio and foundational network | 17 | W2-W4 |
| W6 | non-player entities, CamSeq, enemies | 66 | W3-W5 |
| W7 | Player/gameplay/network SCC | 58 | W1-W6 |
| W8 | renderer, exports, thumbnails, render partials | 13 | W1-W7 |
| W9 | MapGen, repacking, memory tooling | 19 | W3-W8 as used |
| W10 | update subsystem | 6 | W1/W2 platform contracts |
| W11 | portable launcher, logging, EndScreen | 7 | W7-W10 |
| W12 | Avalonia GUI, pause UI, final partial tails | 25 | W8 + W10-W11 |
| W13 | test/diagnostic C# files | 9 | W1-W12 |
| W14 | ModEntry.cs | 1 | all production dispatch dependencies |
| W15 | Program.cs | 1 | W14 + process-entry dependencies |

The counts total exactly 302. Within a wave, start only after the actual
predecessors are closed. Independent files may run in parallel.

### W0 — contract freeze

Record the pinned inventory, mirrored path map, integer widths, enum rules,
struct/reference/null rules, encoding/culture, exception mapping, collection
ordering, async/cancellation behavior, OpenTK math convention, GL/ES and
AL/ALC boundaries, Avalonia conditions, OS conditions, and desktop/server/
Android build matrix.

Gate: 302 unique C# paths; 48 existing pairs; one owner for every partial
declaration; reproducible C++20 translation-unit and parity-test builds; no
status inferred from names.

### W1 — deterministic leaves

Complete the W1 assignment in Appendix A. The first two workers are ChatFont
and StylusZone; Shaders is the next independent leaf. Re-audit simple existing
pairs, and normalize SetupProgress's path.

Gate: exact API/type/enum/default/static-init inventory, complete constants and
tables, invalid-input behavior, exceptions, culture/formatting, and
deterministic C# versus Native results.

Special oracles:

- ChatFont: every glyph pixel/width, Index, and Measure; never samples only.
- StylusZone: ordinals, Button value semantics, all regions, clamps,
  transitions, boundaries, and .NET Math edge cases.
- Shaders: exact properties, strings, line endings, encoding, and hashes.
- Rng: unsigned wrap and promotion behavior.

### W2 — primitives and platform adapters

Complete memory/value/model/culling/types, desktop input data, frame timing,
GL ES/OpenAL ES adapter traces, simple network probes/lag, window/logging,
archive/compression/parser, HTTP synchronization, and remaining low-level
data.

Gate: enum widths/unknown values, zero/default structs, float bits, OpenTK
differential operations, culture behavior, input synchronization/failure
timing, adapter call/state traces, and server exclusion of client-only needs.

### W3 — catalogs and standalone data

Complete Features, Strings, Collada/Scripting, HudInfo, the full Metadata
cluster, and Extract.

Gate: exact keys/order/serialization/encoding/path/null behavior and
deterministic binary/text fixtures. Metadata is not exposed downstream until
all seven files pass aggregate initialization tests.

### W4 — core Formats/Scene/Entity SCC

Complete core Formats, Messaging/Read/Scene contracts, EntityBase/basic
entities, CamSeq, and the Scene declaration pass.

Gate: one Scene class; all units compile; message/registration/removal/update/
destruction order; collision/raw-format fixtures; and an explicit manifest for
only the later SCC symbols still unresolved.

### W5 — audio and foundational network

Complete Formats/Sound, Music/Sfx, SfxMixer, transport/protocol/master/status/
probe/lag, demo containers, map rotation/vote, and dedicated network
foundation.

Gate: packet bytes/endian/signedness/truncation/unknown values, loopback
ordering/errors, demo round trips, sound bytes/defaults/lifecycle, mixer
fixtures, backend failure timing, and Android AL/ALC behavior.

### W6 — entities and enemies

Complete remaining entities, CamSeq, all 42 enemy files, and enemy metadata.
Use shared base/data first, low-coupling enemies next, effects/collision/sound
users next, and bosses last.

Gate: per-frame state traces for transitions, RNG order, spawns/effects,
animation, health/damage, messages, collision, culling, serialization, and
destruction. Freshly audit SlenchNest, CarnivorousPlant, LightSourceEntity,
and PointModuleEntity against the real graph.

### W7 — Player/gameplay/network SCC

Freeze the complete 21-file PlayerEntity declaration before any body. Complete
GameState, Menu, SceneSetup, core PlayerEntity files, player metadata/weapons,
settings, respawn/spectator/world events, chat HUD, GamepadInput, gameplay
network, server simulation, and the assigned PlayerEntity tails.

Gate: one PlayerEntity class; all contributions traceable; player static state,
input ordering, movement/aim/weapon/morph/camera/HUD/sound/AI/scan/pause,
network authority/prediction, scoreboard/room/match transitions, chat, and
SceneSetup match C# frame by frame. This closes the core simulation SCC.

### W8 — renderer/export/thumbnail

Complete Renderer, Selection, Images/Movie, screen/thumbnail tools,
HunterPreview, PreviewCamera, SmoothHudIcon, and assigned Pro HUD work.

Gate: render collection/sorting, matrices/materials/textures/shaders/uniforms,
selection, frame timing, image pixels/crops, thumbnail dimensions, canonical
Scene use, and canonical PlayerEntity ownership. Screenshots supplement state
traces; they do not replace them.

### W9 — MapGen/repack/memory

Complete MemoryClasses, remaining MapGen, and Repack/RepackCollision partials.

Gate: parsed structures, names/order/alignment/offsets/padding, deterministic
bytes, diagnostics, exceptions, Q3 fixtures, collision/entity/model
round-trips, and one declaration per repack partial type.

### W10 — update

Complete DesktopUpdate, ServerUpdate, UpdateCheck, UpdateDownload,
UpdateInstall, and Updater after BuildVersion/SyncHttp and runtime contracts.

Gate: local HTTP/filesystem/process fixtures for no-update, newer version,
malformed metadata, errors, cancellation before/during transfer, partial files,
invalid archives, staging, relaunch args, cleanup, server decisions, disabled
state, page-open behavior, LastReason, task completion, and exceptions.

### W11 — portable launcher/logging/EndScreen

Complete DebugLog, EndScreen, AdventureSave, GameFiles, LauncherPrefs,
MatchStart, and TextLauncher. Recheck W1 LaunchPlan and SetupProgress in the
integration gate.

Gate: preference defaults/formatting, game-file readiness, setup paths,
LaunchPlan side effects/order, scripted text transcripts, launcher/match loop,
shutdown in finally, update timing, no-display behavior, logging lifecycle,
EndScreen availability/choices/hitboxes/exclusions.

### W12 — GUI/pause/final partial tails

Complete all 22 GUI files, PauseMenu, PlayerEntityEndScreen, and PreviewPass.
GuiLauncher.TryRun is the last GUI unit and is not process entry.

Gate: one-time setup/thread affinity, GUI probe/text fallback, Linux no-display
behavior, launcher loop, controls/defaults/events/visibility, pause lifetime,
update/settings state, server exclusion, Android conditions, and final
Scene/PlayerEntity partial closure.

### W13 — tests and diagnostics

Complete Test.cs and the eight Testing files after production contracts close.
Reproduce the C# test/print behavior rather than inventing a Native-only suite.

Gate: fixture construction/order, expected values, binary checks, failure
criteria, output formatting, and the accumulated differential harness.

### W14 — ModEntry.cs

Build an exhaustive argument/build/OS dispatch table. Verify headless dispatch
before setup validation; GUI before text fallback; InputSettings and launcher
preference timing; update-before-work; headless paths without paths.txt;
server exclusion; shutdown; network/map dispatch; return values; side effects;
and all preprocessor variants.

### W15 — Program.cs

Final production file. Exercise Windows/macOS/Linux no-argument behavior,
launcher/text/menu/server/master/update/mapbundle/export/extract/room/model/
invalid routes, ConsoleSetup and ConsoleWindow timing, early headless
dispatch, setup checking, argument parsing, ModEntry dispatch, renderer
lifecycle, exit codes, exception timing, and version/minimum-extract behavior.

Final gate: 302/302 mirrored pairs, no nonexistent includes, no duplicate
partial types, client/server/Android gates, all fixtures, complete link, and
the process-entry matrix.

## 9. Immediate parallel work

Run exactly these two independent files first.

### Worker A — Mods/Chat/ChatFont.cs

Self-contained deterministic static data and logic. Check Cell/First/Last,
derived count, complete Pixels and Widths lengths/bytes, initialization order,
Index below-space through above-tilde, and Measure for empty, unsupported,
mixed, repeated, and all authored glyphs.

### Worker B — Mods/Input/StylusZone.cs

Self-contained enum/struct/static state and System.Math behavior. Check exact
StylusRegion ordinals, nested Button field order/value semantics, all Buttons
and initial static values, Height, SetRect clamps, Begin/Cancel/Down/Drag/Up/
Update transitions, sliding contact, RegionAt boundaries/right-bottom
exclusion/circle edge/outside/Aim, OnButton, Reset, and Math special cases.

Shaders.cs is the next independent W1 leaf and can follow immediately.

## 10. Standard per-file and wave gates

Every audit records:

~~~text
API: namespace, visibility, type kind, inheritance, interfaces, overloads,
     ref/out/in, nested types, enum width/value, observable attributes
Init: zero/default state, field order, static/lazy initialization, first use
Value: overflow, signedness, float bits, NaN/infinity, strings, culture, flags
Flow: branch/loop/short-circuit/early-return order, nulls, finally/disposal
Object: identity, struct copying, aliasing, mutability, virtual dispatch,
        partial private-state access, event/delegate lifetime
Effects: console, files, network, GL/audio, process, environment, threads,
         timers, random/global state
~~~

A wave closes only when every assigned file has one mirrored pair, every file
oracle passes, all SCC tests pass, no fabricated behavior is needed, adapter
calls have C# justification, and affected client/server/Android conditions
have been checked.

## 11. Files blocked from isolated implementation

Do not implement these as standalone units merely because a pair exists:

~~~text
Program.cs
Mods/ModEntry.cs
Mods/Launcher/Gui/GuiLauncher.cs
Mods/Launcher/Portable/TextLauncher.cs
Mods/Launcher/Portable/MatchStart.cs
Mods/Update/Updater.cs
Mods/Update/DesktopUpdate.cs
Mods/Update/ServerUpdate.cs
Entities/EntityBase.cs
Scene.cs
Renderer.cs
Formats/Formats.cs
Formats/Collision.cs
Formats/Effects.cs
Formats/Sound.cs
Sound/Music.cs
Sound/Sfx.cs
all PlayerEntity partial files as independent classes
all Scene partial files as independent classes
gameplay network files that consume PlayerEntity/Scene
~~~

They may be audited and declaration-planned, but missing dependencies must not
be replaced with C++-only substitutes.

## 12. Open questions and resolution

1. Native build authority: identify or establish a C++20 definition for
   individual TUs, parity tests, desktop client, dedicated server, and final
   host. Keep application behavior out of build files.
2. Partial declaration helpers: prefer canonical existing headers; allow a
   declaration-only helper only for one-definition ownership.
3. Target path identity: use source-relative paths everywhere and normalize
   SetupProgress in W1.
4. ProgramException: expose only its exact declaration early if required;
   accept Program.Main only in W15.
5. OpenTK mathematics: differential-test every used operation against 4.9.4.
6. Managed runtime semantics: implement only narrow observed contracts for
   collections, Span, nullable, reflection, boxing, and test order/aliasing.
7. Native Android: if packaging is out of scope, GL/AL/input shared-adapter
   semantics remain mandatory differential targets.
8. NcsfPlay/SoundFlow: treat direct use as an external boundary and port only
   the observed contract through a thin adapter.
9. Async Task translation: record completion/blocking/cancellation/error
   behavior per method before selecting a future/thread/event mechanism.
10. Historical paths: use current source-tree paths in every new audit record.
11. Include closure: regenerate the Native include report after every wave;
    missing headers are allowed only with an explicit declaration contract.

## Appendix A — exact completion-wave assignment

The following records account for every C# source file exactly once. The wave
tag is the final behavior-completion wave.

~~~text

W1 | Shaders.cs
W1 | Formats/Enums.cs
W1 | Formats/Frontend.cs
W1 | Mods/Branding.cs
W1 | Mods/ConsoleWindow.cs
W1 | Mods/Credits.cs
W1 | Mods/Headless.cs
W1 | Mods/Chat/ChatFont.cs
W1 | Mods/Input/StylusZone.cs
W1 | Mods/Input/TouchSettings.cs
W1 | Mods/Launcher/Portable/LaunchPlan.cs
W1 | Mods/Launcher/Portable/SetupProgress.cs
W1 | Mods/MapGen/MapTexturePack.cs
W1 | Mods/Render/Crosshair.cs
W1 | Mods/Update/BuildVersion.cs
W1 | Utility/Console.cs
W1 | Utility/Output.cs
W1 | Utility/Rng.cs
W2 | Memory.cs
W2 | MemoryArrays.cs
W2 | Formats/Culling.cs
W2 | Formats/EntityEnemy.cs
W2 | Formats/FhSound.cs
W2 | Formats/Model.cs
W2 | Formats/NodeData.cs
W2 | Formats/Types.cs
W2 | Mods/HunterSuits.cs
W2 | Mods/LogShare.cs
W2 | Mods/RenderOptions.cs
W2 | Mods/ShutdownSignals.cs
W2 | Mods/ThumbnailHost.cs
W2 | Mods/ThumbnailLog.cs
W2 | Mods/WindowMode.cs
W2 | Mods/Input/GamepadDesktop.cs
W2 | Mods/Input/GamepadLayout.cs
W2 | Mods/Input/GamepadMappings.cs
W2 | Mods/Input/GamepadProbe.cs
W2 | Mods/Input/GamepadState.cs
W2 | Mods/Input/PadBindings.cs
W2 | Mods/Input/PointerInput.cs
W2 | Mods/Input/SyntheticInput.cs
W2 | Mods/Network/NetLag.cs
W2 | Mods/Network/NetProbe.cs
W2 | Mods/Render/EsBindings.cs
W2 | Mods/Render/EsShaders.cs
W2 | Mods/Render/FrameTiming.cs
W2 | Mods/Render/FrameTimingCheck.cs
W2 | Mods/Render/GlEs.cs
W2 | Mods/Sound/AlEs.cs
W2 | Mods/Update/SyncHttp.cs
W2 | Utility/Analyzer.cs
W2 | Utility/Archive.cs
W2 | Utility/Compress.cs
W2 | Utility/Parser.cs
W3 | Features.cs
W3 | Strings.cs
W3 | Export/Collada.cs
W3 | Export/Scripting.cs
W3 | HUD/HudInfo.cs
W3 | Metadata/FrontendMeta.cs
W3 | Metadata/Metadata.cs
W3 | Metadata/Rooms.cs
W3 | Metadata/SoundMeta.cs
W3 | Utility/Extract.cs
W4 | Messaging.cs
W4 | Read.cs
W4 | Scene.cs
W4 | Entities/EntityBase.cs
W4 | Entities/LightSourceEntity.cs
W4 | Entities/PlayerSpawnEntity.cs
W4 | Entities/PointModuleEntity.cs
W4 | Entities/Players/DynamicLightEntity.cs
W4 | Formats/AiPersonality.cs
W4 | Formats/Collision.cs
W4 | Formats/CollisionDetection.cs
W4 | Formats/Effects.cs
W4 | Formats/Entity.cs
W4 | Formats/EntityClass.cs
W4 | Formats/Formats.cs
W4 | Formats/RawFormats.cs
W5 | Formats/Sound.cs
W5 | Mods/Network/DedicatedServer.cs
W5 | Mods/Network/DemoClip.cs
W5 | Mods/Network/DemoFile.cs
W5 | Mods/Network/DemoInfo.cs
W5 | Mods/Network/DemoLibrary.cs
W5 | Mods/Network/DemoPlayback.cs
W5 | Mods/Network/DemoRecorder.cs
W5 | Mods/Network/MapRotation.cs
W5 | Mods/Network/MapVote.cs
W5 | Mods/Network/NetMaster.cs
W5 | Mods/Network/NetProtocol.cs
W5 | Mods/Network/NetStatus.cs
W5 | Mods/Network/NetTransport.cs
W5 | Mods/Sound/SfxMixer.cs
W5 | Sound/Music.cs
W5 | Sound/Sfx.cs
W6 | Entities/AreaVolumeEntity.cs
W6 | Entities/ArtifactEntity.cs
W6 | Entities/BeamEffectEntity.cs
W6 | Entities/BeamProjectileEntity.cs
W6 | Entities/BombEntity.cs
W6 | Entities/DoorEntity.cs
W6 | Entities/EnemyInstanceEntity.cs
W6 | Entities/EnemySpawnEntity.cs
W6 | Entities/FlagBaseEntity.cs
W6 | Entities/ForceFieldEntity.cs
W6 | Entities/ItemInstanceEntity.cs
W6 | Entities/ItemSpawnEntity.cs
W6 | Entities/JumpPadEntity.cs
W6 | Entities/MorphCameraEntity.cs
W6 | Entities/NodeDefenseEntity.cs
W6 | Entities/ObjectEntity.cs
W6 | Entities/OctolithFlagEntity.cs
W6 | Entities/PlatformEntity.cs
W6 | Entities/RoomEntity.cs
W6 | Entities/TeleporterEntity.cs
W6 | Entities/TriggerVolumeEntity.cs
W6 | Entities/CamSeq/CameraSequence.cs
W6 | Entities/CamSeq/CamSeqEntity.cs
W6 | Entities/Enemies/00_WarWasp.cs
W6 | Entities/Enemies/01_Zoomer.cs
W6 | Entities/Enemies/02_Temroid.cs
W6 | Entities/Enemies/03_Petrasyl1.cs
W6 | Entities/Enemies/04_Petrasyl2.cs
W6 | Entities/Enemies/05_Petrasyl3.cs
W6 | Entities/Enemies/06_Petrasyl4.cs
W6 | Entities/Enemies/10_BarbedWarWasp.cs
W6 | Entities/Enemies/11_Shriekbat.cs
W6 | Entities/Enemies/12_Geemer.cs
W6 | Entities/Enemies/16_Blastcap.cs
W6 | Entities/Enemies/18_AlimbicTurret.cs
W6 | Entities/Enemies/19_Cretaphid.cs
W6 | Entities/Enemies/20_CretaphidEye.cs
W6 | Entities/Enemies/21_CretaphidCrystal.cs
W6 | Entities/Enemies/23_PsychoBit.cs
W6 | Entities/Enemies/24_Gorea1A.cs
W6 | Entities/Enemies/25_GoreaHead.cs
W6 | Entities/Enemies/26_GoreaArm.cs
W6 | Entities/Enemies/27_GoreaLeg.cs
W6 | Entities/Enemies/28_Gorea1B.cs
W6 | Entities/Enemies/29_GoreaSealSphere1.cs
W6 | Entities/Enemies/30_Trocra.cs
W6 | Entities/Enemies/31_Gorea2.cs
W6 | Entities/Enemies/32_GoreaSealSphere2.cs
W6 | Entities/Enemies/33_GoreaMeteor.cs
W6 | Entities/Enemies/35_Voldrum.cs
W6 | Entities/Enemies/36_Voldrum.cs
W6 | Entities/Enemies/37_Quadtroid.cs
W6 | Entities/Enemies/38_CrashPillar.cs
W6 | Entities/Enemies/39_FireSpawn.cs
W6 | Entities/Enemies/40_EnemySpawner.cs
W6 | Entities/Enemies/41_Slench.cs
W6 | Entities/Enemies/42_SlenchShield.cs
W6 | Entities/Enemies/43_SlenchNest.cs
W6 | Entities/Enemies/44_SlenchSynapse.cs
W6 | Entities/Enemies/45_SlenchTurret.cs
W6 | Entities/Enemies/46_LesserIthrak.cs
W6 | Entities/Enemies/47_GreaterIthrak.cs
W6 | Entities/Enemies/49_ForceFieldLock.cs
W6 | Entities/Enemies/50_HitZone.cs
W6 | Entities/Enemies/51_CarnivorousPlant.cs
W6 | Metadata/Enemies.cs
W7 | GameState.cs
W7 | Menu.cs
W7 | SceneSetup.cs
W7 | Entities/Players/HalfturretEntity.cs
W7 | Entities/Players/PlayerAi.cs
W7 | Entities/Players/PlayerCamera.cs
W7 | Entities/Players/PlayerCollision.cs
W7 | Entities/Players/PlayerDialog.cs
W7 | Entities/Players/PlayerDraw.cs
W7 | Entities/Players/PlayerEntity.cs
W7 | Entities/Players/PlayerHud.cs
W7 | Entities/Players/PlayerInput.cs
W7 | Entities/Players/PlayerPause.cs
W7 | Entities/Players/PlayerProcess.cs
W7 | Entities/Players/PlayerScan.cs
W7 | Entities/Players/PlayerSound.cs
W7 | Metadata/Player.cs
W7 | Metadata/Weapons.cs
W7 | Mods/GameSettings.cs
W7 | Mods/InputSettings.cs
W7 | Mods/RespawnChoice.cs
W7 | Mods/SpectatorMode.cs
W7 | Mods/WorldEvents.cs
W7 | Mods/Chat/ChatBox.cs
W7 | Mods/Chat/PlayerEntityChatHud.cs
W7 | Mods/Input/GamepadInput.cs
W7 | Mods/Network/MapAudit.cs
W7 | Mods/Network/MechanicsDump.cs
W7 | Mods/Network/NetCheckClient.cs
W7 | Mods/Network/NetConnectCommand.cs
W7 | Mods/Network/NetDamage.cs
W7 | Mods/Network/NetDiagnostics.cs
W7 | Mods/Network/NetFeatureCheck.cs
W7 | Mods/Network/NetHitPrediction.cs
W7 | Mods/Network/NetHooks.cs
W7 | Mods/Network/NetHostSession.cs
W7 | Mods/Network/NetLaunch.cs
W7 | Mods/Network/NetLog.cs
W7 | Mods/Network/NetMatchEnd.cs
W7 | Mods/Network/NetMatchSync.cs
W7 | Mods/Network/NetPlayerBridge.cs
W7 | Mods/Network/NetPlayerSetup.cs
W7 | Mods/Network/NetRoomChange.cs
W7 | Mods/Network/NetScoreboard.cs
W7 | Mods/Network/NetSession.cs
W7 | Mods/Network/NetSlotManager.cs
W7 | Mods/Network/NetTestScript.cs
W7 | Mods/Network/NetUnlagged.cs
W7 | Mods/Network/PlayerColors.cs
W7 | Mods/Network/PlayerEntityNetAim.cs
W7 | Mods/Network/PlayerEntityNetHud.cs
W7 | Mods/Network/ServerSim.cs
W7 | Mods/Network/ServerSimCheck.cs
W7 | Mods/Network/WeaponDps.cs
W7 | Mods/Render/PlayerEntityAmmoClear.cs
W7 | Mods/Render/PlayerEntityIconBounds.cs
W7 | Mods/Render/PlayerEntityStylusHud.cs
W7 | Mods/Render/PlayerEntityVoteHud.cs
W8 | Renderer.cs
W8 | Selection.cs
W8 | Export/Images.cs
W8 | Formats/Movie.cs
W8 | Mods/ScreenCapture.cs
W8 | Mods/ThumbnailBatch.cs
W8 | Mods/ThumbnailCapture.cs
W8 | Mods/ThumbnailGenerator.cs
W8 | Mods/ThumbnailMode.cs
W8 | Mods/Render/HunterPreview.cs
W8 | Mods/Render/PlayerEntityProHud.cs
W8 | Mods/Render/PreviewCamera.cs
W8 | Mods/Render/SmoothHudIcon.cs
W9 | MemoryClasses.cs
W9 | Mods/MapGen/BuiltMap.cs
W9 | Mods/MapGen/CustomRooms.cs
W9 | Mods/MapGen/MapBuilder.cs
W9 | Mods/MapGen/MapBundle.cs
W9 | Mods/MapGen/MapCollisionPacker.cs
W9 | Mods/MapGen/MapDefinition.cs
W9 | Mods/MapGen/MapNodePacker.cs
W9 | Mods/MapGen/MapPacker.cs
W9 | Mods/MapGen/MapReport.cs
W9 | Mods/MapGen/MapTextureBake.cs
W9 | Mods/MapGen/Q3Bsp.cs
W9 | Mods/MapGen/Q3Convert.cs
W9 | Mods/MapGen/Q3Import.cs
W9 | Mods/MapGen/RawStructs.cs
W9 | Mods/MapGen/RepackAccess.cs
W9 | Utility/RepackCollision.cs
W9 | Utility/RepackEntity.cs
W9 | Utility/RepackModel.cs
W10 | Mods/Update/DesktopUpdate.cs
W10 | Mods/Update/ServerUpdate.cs
W10 | Mods/Update/UpdateCheck.cs
W10 | Mods/Update/UpdateDownload.cs
W10 | Mods/Update/UpdateInstall.cs
W10 | Mods/Update/Updater.cs
W11 | Mods/DebugLog.cs
W11 | Mods/EndScreen.cs
W11 | Mods/Launcher/Portable/AdventureSave.cs
W11 | Mods/Launcher/Portable/GameFiles.cs
W11 | Mods/Launcher/Portable/LauncherPrefs.cs
W11 | Mods/Launcher/Portable/MatchStart.cs
W11 | Mods/Launcher/Portable/TextLauncher.cs
W12 | Mods/Launcher/Gui/CrosshairPreview.cs
W12 | Mods/Launcher/Gui/DemoPickerView.cs
W12 | Mods/Launcher/Gui/GuiLauncher.cs
W12 | Mods/Launcher/Gui/GuiTheme.cs
W12 | Mods/Launcher/Gui/HomeView.cs
W12 | Mods/Launcher/Gui/HomeWindow.cs
W12 | Mods/Launcher/Gui/KeyRow.cs
W12 | Mods/Launcher/Gui/MapPickerView.cs
W12 | Mods/Launcher/Gui/MenuEntry.cs
W12 | Mods/Launcher/Gui/PadRow.cs
W12 | Mods/Launcher/Gui/PauseMenuView.cs
W12 | Mods/Launcher/Gui/PauseMenuWindow.cs
W12 | Mods/Launcher/Gui/ProgressRow.cs
W12 | Mods/Launcher/Gui/Rows.cs
W12 | Mods/Launcher/Gui/ServerRow.cs
W12 | Mods/Launcher/Gui/SettingsView.cs
W12 | Mods/Launcher/Gui/SettingsWindow.cs
W12 | Mods/Launcher/Gui/SliderRow.cs
W12 | Mods/Launcher/Gui/SplashView.cs
W12 | Mods/Launcher/Gui/TrackedText.cs
W12 | Mods/Launcher/Gui/UiCapture.cs
W12 | Mods/Launcher/Gui/UpdateBadge.cs
W12 | Mods/PauseMenu.cs
W12 | Mods/Render/PlayerEntityEndScreen.cs
W12 | Mods/Render/PreviewPass.cs
W13 | Test.cs
W13 | Testing/TestEffects.cs
W13 | Testing/TestLogic.cs
W13 | Testing/TestMisc.cs
W13 | Testing/TestOverlay.cs
W13 | Testing/TestParse.cs
W13 | Testing/TestPlayer.cs
W13 | Testing/TestPrint.cs
W13 | Testing/TestWeapons.cs
W14 | Mods/ModEntry.cs
W15 | Program.cs
~~~

## Appendix B — count and acceptance proof

The assignment must be checked against the source tree, excluding generated obj files:

~~~powershell
$source = Get-ChildItem src/MphRead -Recurse -File -Filter *.cs |
  Where-Object { $_.FullName -notlike '*\\src\\MphRead\\obj\\*' }
$rows = Select-String -Path docs/MphRead-Native-Implementation-Order.md -Pattern '^W\\d+ \\| (.+\\.cs)$'
$source.Count  # must be 302
$rows.Count    # must be 302
~~~

No path may appear twice, and the set of row paths must equal the set of source-relative paths.
The wave counts are W1=18, W2=36, W3=10, W4=16, W5=17, W6=66, W7=58,
W8=13, W9=19, W10=6, W11=7, W12=25, W13=9, W14=1, W15=1; total 302.

Use these status labels in future tracking:

~~~text
MISSING          no Native pair
PAIRED-UNAUDITED hpp/cpp exist; whole-file parity is unproven
AUDIT-FAIL       a semantic or dependency-closure defect remains
CONTRACT-ONLY    exact declarations exist; implementation is incomplete
PARITY-PASS      whole-file oracle and required wave gates pass
FINAL-PASS       PARITY-PASS plus final link/platform/entry gates pass
~~~

PARITY-PASS is never inferred from paired alone.
