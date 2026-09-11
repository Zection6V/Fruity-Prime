# MphRead.Native strict implementation order

## Status and authority

**Repository:** `Zection6V/Fruity-Prime`  
**Branch:** `develop2`  
**Reviewed commit:** `9e9eb0e7267534faac044f0bdd36d17e00c97735`

This document defines the dependency-closure order for the strict C# to C++20 migration.

The only behavioral specification is:

```text
src/MphRead/**/*.cs
```

The Native tree must reproduce those files one-for-one under:

```text
src/MphRead.Native/
```

Every translated C# file must have a colocated `.hpp` and `.cpp`. A Native implementation is not allowed to introduce policy, fallback behavior, validation, normalization, ownership rules, threading rules, exception handling, or defaults that are not observable in the C# source. Platform and language adapters are permitted only where C++ cannot directly express the C# mechanism.

At this snapshot:

- C# files: **302**
- existing Native headers: **48**
- existing Native implementations: **48**
- existing Native pairs: **48**
- files without any Native pair: **254**

The 48 existing pairs are **not 48 completed dependencies**. Their existence means only that files with those names exist. The Native include inventory already shows existing pairs including headers for Native units that do not yet exist, notably `Program`, `ModEntry`, entity files, and launcher-related units. Therefore `paired` must never be interpreted as `dependency-closed`, `buildable`, or `C#-equivalent`.

---

# 1. Corrections to the existing dependency map

## 1.1 `using` directives are not a file dependency graph

The existing C# indexes record:

- namespace
- apparent declarations
- internal `using`
- external `using`

That is an inventory, not a resolvable graph.

A file can depend on another file without any `using` at all when both declarations are in the same namespace.

Concrete example:

```text
Mods/Input/PadBindings.cs
```

has no internal `using`, but its executable code uses `GamepadButtons`, which is declared by:

```text
Mods/Input/GamepadState.cs
```

Therefore:

```text
GamepadState.cs -> PadBindings.cs
```

is a real file-level prerequisite that is absent from the namespace index.

The same problem is pervasive in the root `MphRead` namespace and in:

```text
MphRead.Entities
MphRead.Mods
MphRead.Mods.Network
MphRead.Mods.Launcher
MphRead.Formats.Sound
```

**Rule:** no file may be declared dependency-free merely because its `internal using` column is empty.

---

## 1.2 The declaration extractor is not a semantic parser

The current indexes contain entries such as:

```text
private
struct
is
makes
so
after
with
for
rather
read
```

as alleged declared types.

`ServerRow.cs` also has executable source text in its external/alias-using column.

Those are lexical extraction artifacts.

Consequences:

1. the `types` column cannot be used as a symbol table;
2. repeated declarations cannot be inferred reliably from that column alone;
3. partial types must be resolved from actual C# declarations;
4. dependency closure must use actual symbol references, not the generated table as an oracle.

The indexes remain useful for coverage only.

---

## 1.3 `paired` is not a dependency state

For example, the Native include index shows existing Native units referring to absent units:

```text
Entities/Enemies/43_SlenchNest
    -> EntityBase.hpp
    -> EnemyInstanceEntity.hpp
    -> EnemySpawnEntity.hpp

Entities/LightSourceEntity
    -> EntityBase.hpp
    -> Formats/Entity.hpp
    -> Formats/Types.hpp
    -> Renderer.hpp

Program
    -> Formats/Formats.hpp
    -> Formats/Movie.hpp
    -> Formats/Sound.hpp
    -> Menu.hpp
    -> Metadata/Metadata.hpp
    -> Metadata/Rooms.hpp
    -> Mods/ModEntry.hpp
    -> Read.hpp
    -> Renderer.hpp
    -> Utility/Extract.hpp
    ...
```

Most of those prerequisites do not yet have Native counterparts.

Accordingly every existing Native pair must retain one of these states:

```text
UNREVIEWED
BLOCKED
PARITY-PASS
```

Existence is not a fourth state.

---

## 1.4 `SetupProgress` is not correctly placed for the strict contract

The dependency inventory treats this as an acceptable relocation:

```text
C#:
Mods/Launcher/Portable/SetupProgress.cs

Native:
Mods/Launcher/SetupProgress.hpp
Mods/Launcher/SetupProgress.cpp
```

For a strict one-C#-file/one-Native-pair tree, the canonical counterpart is:

```text
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.cpp
```

The present relocation must not be used as precedent.

This is a structural correction, not permission to change it during this planning review.

---

## 1.5 The old layer sequence hides real cycles

The old high-level order approximately says:

```text
Utility
 -> Formats
 -> Entities
 -> Network/MapGen
 -> Render/Input
 -> Launcher
 -> Program
```

That is too linear.

Actual C# relationships include, among others:

```text
Formats <-> Entities
Entities <-> Network
Scene <-> Entities
Formats.Sound <-> Sound
PlayerEntity base file <-> PlayerEntity partial extensions
Scene base file <-> Scene partial extensions
Metadata base file <-> Metadata partial files
Repack partial implementation files <-> Formats/Editor/Entities
```

These are strongly connected implementation clusters, not mistakes that should be “fixed” by inventing a new C++ abstraction.

---

# 2. True process entry and startup closure

The real process entry remains:

```text
Program.Main(string[] args)
```

Its initial order is significant:

```text
ConsoleSetup.Run()
ConsoleWindow.Prepare(args)            // Windows
ModEntry.TryHandleHeadless(args)
CheckSetup(args)
ParseArguments(args)
ModEntry.TryHandle(args)
normal upstream command/menu/render path
```

Therefore `TryHandleHeadless()` is not merely another command handler. It executes **before game-file setup validation**.

Launcher dispatch occurs inside `TryHandleHeadless()` for launcher invocations:

```text
GuiLauncher.TryRun()
    -> true: launcher handled
    -> false: show console if required, then TextLauncher.Run()

or directly:

TextLauncher.Run()
```

The GUI call is conditional on `MPHREAD_AVALONIA` and runtime/argument state.

The server build changes this graph with `MPHREAD_SERVER`.

So the dependency closure for startup is:

```text
Program
  -> ConsoleSetup / ConsoleWindow
  -> ModEntry.TryHandleHeadless
       -> input settings
       -> launcher preferences
       -> debug logging
       -> update subsystem
       -> render overrides
       -> network controls
       -> map tools
       -> launcher
       -> dedicated/master server paths
  -> setup/read/export path
  -> ModEntry.TryHandle
  -> Menu / Renderer / Read / Metadata / Sound
```

`Program` and `ModEntry` are consequently **terminal integration units**, not foundation units.

---

# 3. Language-level clusters that C++ cannot translate one file at a time

## 3.1 Partial `PlayerEntity`

Multiple source files contribute to the same C# class.

The cluster includes the core player files:

```text
Entities/Players/PlayerEntity.cs
Entities/Players/PlayerAi.cs
Entities/Players/PlayerCamera.cs
Entities/Players/PlayerCollision.cs
Entities/Players/PlayerDialog.cs
Entities/Players/PlayerDraw.cs
Entities/Players/PlayerHud.cs
Entities/Players/PlayerInput.cs
Entities/Players/PlayerPause.cs
Entities/Players/PlayerProcess.cs
Entities/Players/PlayerScan.cs
Entities/Players/PlayerSound.cs
```

and mod extensions including:

```text
Mods/Chat/PlayerEntityChatHud.cs
Mods/Network/PlayerEntityNetAim.cs
Mods/Network/PlayerEntityNetHud.cs
Mods/Render/PlayerEntityAmmoClear.cs
Mods/Render/PlayerEntityEndScreen.cs
Mods/Render/PlayerEntityIconBounds.cs
Mods/Render/PlayerEntityProHud.cs
Mods/Render/PlayerEntityStylusHud.cs
Mods/Render/PlayerEntityVoteHud.cs
```

C++ cannot reopen a completed class declaration the way C# `partial class` does.

### Required adapter rule

Before the Player wave begins, define one mechanical partial-class translation convention:

1. one Native header owns the complete C++ `PlayerEntity` class declaration;
2. declarations originating in every C# partial file remain traceable to that C# file;
3. each C# file still receives its own colocated hpp/cpp pair;
4. implementation bodies remain in the corresponding `.cpp`;
5. no additional state, methods, ordering policy, or ownership may be added;
6. this adapter is language-level plumbing only.

No individual `PlayerEntity` partial file should be treated as completed before the aggregate declaration gate passes.

---

## 3.2 Partial `Scene`

At minimum, these source files contribute to `Scene` behavior:

```text
Scene.cs
Messaging.cs
Renderer.cs
Mods/Render/PreviewCamera.cs
Mods/Render/PreviewPass.cs
```

`PreviewCamera.cs`, for example, explicitly declares:

```csharp
public partial class Scene
```

and directly accesses Scene private state.

Therefore `PreviewCamera.cs` cannot truthfully be considered an isolated Native pair just because a Native pair already exists.

Use the same partial-class adapter rule as `PlayerEntity`.

---

## 3.3 Metadata cluster

The metadata files collectively supply the metadata API:

```text
Metadata/Metadata.cs
Metadata/Enemies.cs
Metadata/FrontendMeta.cs
Metadata/Player.cs
Metadata/Rooms.cs
Metadata/SoundMeta.cs
Metadata/Weapons.cs
```

Treat this as one declaration/initalization cluster until actual partial declarations and static initialization order have been reproduced.

Do not independently invent Native singleton registries for each file.

---

## 3.4 Repack cluster

These files contribute closely coupled repacking behavior:

```text
Utility/RepackCollision.cs
Utility/RepackEntity.cs
Utility/RepackModel.cs
Mods/MapGen/RepackAccess.cs
```

The Native layer must follow the C# organization and visibility. Do not introduce a Native-only “repack framework” merely to break dependencies.

---

# 4. External/runtime contracts that must be frozen first

These are not translated C# files, but every wave depends on them.

## 4.1 .NET 9 language/runtime behavior

The Native compatibility layer must have explicit decisions/tests for observable uses of:

```text
System.String / UTF-16 semantics
StringComparison
StringBuilder
char
Version
Enum / [Flags] / Enum.GetValues / Enum.TryParse / ToString
Array
Span<T> / ReadOnlySpan<T>
List<T>
Dictionary<T>
HashSet<T>
Frozen collections
Immutable collections
LINQ ordering and exception behavior
nullable references/value types
struct copying
readonly struct
record/value equality where used
delegates/events
reflection
attributes
Math / MathF
checked/unchecked integer arithmetic
floating-point conversions
BitConverter / binary primitives
Marshal / StructLayout / unsafe
DateTime / Stopwatch / timers
CurrentCulture / InvariantCulture
Console
Environment
Path / File / Directory
Stream/BinaryReader/BinaryWriter
Compression
Tar
JSON
HttpClient
Socket/UdpClient/Tcp*
Thread / Task
CancellationToken
lock/Monitor-equivalent synchronization
Process
OperatingSystem platform tests
```

Do not create one universal approximation when the C# file depends on narrower semantics.

---

## 4.2 OpenTK 4.9.4

Observed areas include:

```text
OpenTK.Mathematics
OpenTK.Graphics.OpenGL
OpenTK.Audio.OpenAL
OpenTK.Windowing.Common
OpenTK.Windowing.Desktop
OpenTK.Windowing.GraphicsLibraryFramework
```

Matrix convention, vector normalization, equality, GL constants, input enums, callback ordering, and OpenAL ownership must be treated as externally observable contracts.

---

## 4.3 Avalonia 11.3.11

Relevant only in builds where:

```text
MPHREAD_AVALONIA
```

is defined.

Server builds explicitly remove:

```text
Mods/Launcher/Gui/**
```

from compilation.

Native GUI work must therefore distinguish:

```text
game build
server build
desktop platform runtime
Android runtime
```

without inventing a universal GUI startup path.

---

## 4.4 Android aliases

Android compiles the shared `src/MphRead/**/*.cs` sources and substitutes aliases for:

```text
GL  -> MphRead.Mods.Render.GlEs
AL  -> MphRead.Mods.Sound.AlEs
ALC -> MphRead.Mods.Sound.AlcEs
```

Therefore OpenGL/OpenAL translation cannot be validated against desktop alone.

---

## 4.5 Image/audio/runtime packages

Contracts also exist for:

```text
ReFuel.StbImage 2.1.1
Silk.NET.OpenAL.Soft.Native 1.23.1
NcsfPlay
CommunityToolkit.HighPerformance
SoundFlow
System.IO.Hashing
```

`NcsfPlay` is a project dependency rather than an incidental namespace.

---

# 5. Scheduling rule

The following is a **dependency-wave order**, not an instruction to implement all files in a wave simultaneously.

Within a non-SCC wave:

1. resolve actual referenced source symbols;
2. implement files whose predecessors in the same wave are already closed;
3. when two files have no edge between them, they may run in parallel;
4. alphabetical order below is only the deterministic tie-breaker.

Within an SCC/partial wave:

1. inspect every member of the SCC first;
2. freeze the shared C++ declaration/adapter;
3. implement bodies file by file;
4. no file becomes a prerequisite for later waves until the SCC-level oracle passes.

---

# Wave 0 - Runtime contract freeze

**No C# translation work dependent on an unresolved semantic boundary is considered complete in this wave.**

Prerequisites: none.

Gate:

- integer and float conversion oracle;
- enum/Flags oracle;
- string/culture oracle;
- exception oracle;
- struct/layout oracle;
- filesystem/path oracle;
- threading/task/cancellation oracle;
- OpenTK math oracle.

---

# Wave 1 - Verified atomic leaves

These are the first scheduling pool. They have no required gameplay/entity/renderer/network object graph.

## Root

```text
Features.cs
Shaders.cs
```

## Formats

```text
Formats/Enums.cs
Formats/Culling.cs
Formats/Frontend.cs
Formats/FhSound.cs
Formats/NodeData.cs
Formats/Types.cs
```

## HUD

```text
HUD/HudInfo.cs
```

## Metadata leaf data

```text
Metadata/FrontendMeta.cs
Metadata/Metadata.cs
Metadata/Rooms.cs
Metadata/SoundMeta.cs
```

The entire Metadata aggregate must still pass its later cluster gate before downstream use.

## Mods

```text
Mods/Branding.cs
Mods/ConsoleWindow.cs
Mods/Credits.cs
Mods/Headless.cs
Mods/HunterSuits.cs
Mods/RenderOptions.cs
Mods/ShutdownSignals.cs
```

## Chat

```text
Mods/Chat/ChatFont.cs
```

## Input

```text
Mods/Input/GamepadState.cs
Mods/Input/StylusZone.cs
Mods/Input/TouchSettings.cs
```

## Portable launcher value/data units

```text
Mods/Launcher/Portable/AdventureSave.cs
Mods/Launcher/Portable/GameFiles.cs
Mods/Launcher/Portable/LaunchPlan.cs
Mods/Launcher/Portable/SetupProgress.cs
```

`SetupProgress` must ultimately use the mirrored `Portable` Native path.

## MapGen data/file leaves

```text
Mods/MapGen/MapDefinition.cs
Mods/MapGen/MapReport.cs
Mods/MapGen/MapTexturePack.cs
Mods/MapGen/Q3Bsp.cs
Mods/MapGen/RawStructs.cs
```

## Network transport/value leaves

```text
Mods/Network/DemoClip.cs
Mods/Network/DemoFile.cs
Mods/Network/DemoInfo.cs
Mods/Network/MapRotation.cs
Mods/Network/MapVote.cs
Mods/Network/NetLag.cs
Mods/Network/NetMaster.cs
Mods/Network/NetProbe.cs
Mods/Network/NetStatus.cs
Mods/Network/NetTransport.cs
```

## Render leaf units

```text
Mods/Render/Crosshair.cs
Mods/Render/EsBindings.cs
Mods/Render/EsShaders.cs
Mods/Render/FrameTiming.cs
Mods/Render/FrameTimingCheck.cs
```

`FrameTiming.cs` precedes `FrameTimingCheck.cs`.

## Update leaf units

```text
Mods/Update/BuildVersion.cs
Mods/Update/SyncHttp.cs
```

## Utility leaves

```text
Utility/Archive.cs
Utility/Compress.cs
Utility/Console.cs
Utility/Output.cs
Utility/Parser.cs
Utility/Rng.cs
```

### Wave 1 oracle

For each file:

- exact namespace and accessibility;
- exact type kind (`class`, static class, struct, readonly struct, enum);
- enum underlying types and values;
- field/property initialization order;
- copy/value/reference semantics;
- exact constants and strings;
- boundary and invalid inputs;
- exact exception type/timing;
- float bit results where arithmetic is observable;
- static initialization behavior.

For data-heavy files, compare complete serialized/output tables rather than samples.

---

# Wave 2 - Foundation data and serialization

Prerequisite: Wave 1 contracts used by the file are closed.

## Root

```text
Memory.cs
MemoryArrays.cs
Strings.cs
```

## Formats

```text
Formats/EntityEnemy.cs
Formats/Model.cs
Formats/RawFormats.cs
```

## Metadata continuation

```text
Metadata/Player.cs
Metadata/Weapons.cs
Metadata/Enemies.cs
```

Do not expose Metadata as a completed prerequisite until all seven Metadata files have been validated together.

## Input foundations

```text
Mods/Input/GamepadDesktop.cs
Mods/Input/GamepadLayout.cs
Mods/Input/GamepadMappings.cs
Mods/Input/GamepadProbe.cs
Mods/Input/PadBindings.cs
```

Explicit hidden edge:

```text
GamepadState.cs -> PadBindings.cs
```

## Launcher portable settings

```text
Mods/Launcher/Portable/LauncherPrefs.cs
```

## Update data/check layer

```text
Mods/Update/UpdateCheck.cs
Mods/Update/UpdateDownload.cs
```

## MapGen leaf transforms

```text
Mods/MapGen/CustomRooms.cs
Mods/MapGen/MapBundle.cs
Mods/MapGen/MapTextureBake.cs
Mods/MapGen/Q3Convert.cs
```

## Network demo/value continuation

```text
Mods/Network/DemoLibrary.cs
Mods/Network/DemoPlayback.cs
Mods/Network/DemoRecorder.cs
```

## Utility

```text
Utility/Analyzer.cs
Utility/Extract.cs
Utility/RepackModel.cs
```

### Wave 2 oracle

Add:

- byte-for-byte serialization round trips;
- exact path normalization;
- exact enum formatting/parsing;
- collection iteration order where output observes it;
- JSON field/default/null behavior;
- stream truncation/EOF/error behavior;
- static-table completeness checks.

---

# Wave 3 - Core format/entity/scene SCC

**BLOCKED until Waves 0-2 relevant dependencies are closed.**

This is the first major strongly connected component.

## Formats

```text
Formats/AiPersonality.cs
Formats/Collision.cs
Formats/CollisionDetection.cs
Formats/Effects.cs
Formats/Entity.cs
Formats/EntityClass.cs
Formats/Formats.cs
```

## Root core

```text
Messaging.cs
Scene.cs
SceneSetup.cs
```

## Basic entity runtime

```text
Entities/AreaVolumeEntity.cs
Entities/ArtifactEntity.cs
Entities/BeamEffectEntity.cs
Entities/DoorEntity.cs
Entities/EnemyInstanceEntity.cs
Entities/EnemySpawnEntity.cs
Entities/EntityBase.cs
Entities/FlagBaseEntity.cs
Entities/ForceFieldEntity.cs
Entities/ItemInstanceEntity.cs
Entities/ItemSpawnEntity.cs
Entities/JumpPadEntity.cs
Entities/LightSourceEntity.cs
Entities/MorphCameraEntity.cs
Entities/NodeDefenseEntity.cs
Entities/ObjectEntity.cs
Entities/OctolithFlagEntity.cs
Entities/PlatformEntity.cs
Entities/PlayerSpawnEntity.cs
Entities/PointModuleEntity.cs
Entities/RoomEntity.cs
Entities/TeleporterEntity.cs
Entities/TriggerVolumeEntity.cs
```

## Camera-sequence extensions

```text
Entities/CamSeq/CameraSequence.cs
Entities/CamSeq/CamSeqEntity.cs
```

## Scene partial extension

```text
Mods/Render/PreviewCamera.cs
```

### Why this is an SCC

Representative edges include:

```text
EntityBase -> Collision/Culling/Network/Sound
Collision -> Entities/Culling
Effects -> Entities/Collision
Formats -> Effects/Entities/Culling
Scene -> Entities
entities -> Formats/Scene services
```

Do not “solve” this by moving behavior into a new Native service.

Use C++ forward declarations to break header inclusion only where a C# reference does not require a complete C++ type. That is a compile-time language mechanism, not a semantic dependency deletion.

### Wave 3 oracle

In addition to per-file parity:

- entity construction/destruction order;
- Scene registration/removal;
- collision-volume bit/layout tests;
- virtual/override dispatch;
- message dispatch;
- reference identity;
- null behavior;
- entity update ordering;
- room/node ownership;
- partial `Scene` declaration completeness.

No enemy/player/network behavior may be used to declare this gate passed unless explicitly part of the C# core dependency.

---

# Wave 4 - Enemy runtime cluster

**BLOCKED until Wave 3.**

File-level rule:

1. shared enemy base/data contracts first;
2. enemy implementations with only Culling/Formats next;
3. Effects/Collision/Sound users next;
4. bosses with cross-enemy references last.

Files:

```text
Entities/Enemies/00_WarWasp.cs
Entities/Enemies/01_Zoomer.cs
Entities/Enemies/02_Temroid.cs
Entities/Enemies/03_Petrasyl1.cs
Entities/Enemies/04_Petrasyl2.cs
Entities/Enemies/05_Petrasyl3.cs
Entities/Enemies/06_Petrasyl4.cs
Entities/Enemies/10_BarbedWarWasp.cs
Entities/Enemies/11_Shriekbat.cs
Entities/Enemies/12_Geemer.cs
Entities/Enemies/16_Blastcap.cs
Entities/Enemies/18_AlimbicTurret.cs
Entities/Enemies/19_Cretaphid.cs
Entities/Enemies/20_CretaphidEye.cs
Entities/Enemies/21_CretaphidCrystal.cs
Entities/Enemies/23_PsychoBit.cs
Entities/Enemies/24_Gorea1A.cs
Entities/Enemies/25_GoreaHead.cs
Entities/Enemies/26_GoreaArm.cs
Entities/Enemies/27_GoreaLeg.cs
Entities/Enemies/28_Gorea1B.cs
Entities/Enemies/29_GoreaSealSphere1.cs
Entities/Enemies/30_Trocra.cs
Entities/Enemies/31_Gorea2.cs
Entities/Enemies/32_GoreaSealSphere2.cs
Entities/Enemies/33_GoreaMeteor.cs
Entities/Enemies/35_Voldrum.cs
Entities/Enemies/36_Voldrum.cs
Entities/Enemies/37_Quadtroid.cs
Entities/Enemies/38_CrashPillar.cs
Entities/Enemies/39_FireSpawn.cs
Entities/Enemies/40_EnemySpawner.cs
Entities/Enemies/41_Slench.cs
Entities/Enemies/42_SlenchShield.cs
Entities/Enemies/43_SlenchNest.cs
Entities/Enemies/44_SlenchSynapse.cs
Entities/Enemies/45_SlenchTurret.cs
Entities/Enemies/46_LesserIthrak.cs
Entities/Enemies/47_GreaterIthrak.cs
Entities/Enemies/49_ForceFieldLock.cs
Entities/Enemies/50_HitZone.cs
Entities/Enemies/51_CarnivorousPlant.cs
```

### Wave 4 oracle

Use deterministic C# enemy-state scripts:

```text
initial state
N fixed updates
collisions/messages/damage injected at fixed frames
capture all observable state after every frame
```

Compare:

- state transition frame;
- RNG consumption count/order;
- spawned entities/effects;
- animation indices;
- health/damage;
- message sends;
- collision flags;
- float values/bit patterns where practical.

Do not validate only final state.

---

# Wave 5 - Player/partial gameplay SCC

**BLOCKED until Waves 3-4 and partial-class adapter are closed.**

## Player support

```text
Entities/Players/DynamicLightEntity.cs
Entities/Players/HalfturretEntity.cs
```

## Core `PlayerEntity` partial cluster

```text
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
```

## Player extensions outside the Players directory

```text
Mods/Chat/PlayerEntityChatHud.cs
Mods/Network/PlayerEntityNetAim.cs
Mods/Network/PlayerEntityNetHud.cs
Mods/Render/PlayerEntityAmmoClear.cs
Mods/Render/PlayerEntityEndScreen.cs
Mods/Render/PlayerEntityIconBounds.cs
Mods/Render/PlayerEntityProHud.cs
Mods/Render/PlayerEntityStylusHud.cs
Mods/Render/PlayerEntityVoteHud.cs
```

## Player metadata/runtime helpers

```text
MemoryClasses.cs
```

### Wave 5 oracle

One aggregate Player oracle must check:

- complete field/default state;
- construction;
- every partial method's access to shared private state;
- input;
- movement;
- collision;
- damage;
- weapon/ammo state;
- morph/boost;
- camera;
- HUD;
- sound;
- AI;
- scan;
- pause/dialog;
- network extension fields;
- mod HUD extension behavior.

A PASS on one partial `.cpp` is not a PlayerEntity PASS.

---

# Wave 6 - Network SCC

**BLOCKED until core Entity and Player contracts exist.**

The transport/value leaves from Waves 1-2 are prerequisites.

Remaining files:

```text
Mods/Network/DedicatedServer.cs
Mods/Network/MapAudit.cs
Mods/Network/MechanicsDump.cs
Mods/Network/NetCheckClient.cs
Mods/Network/NetConnectCommand.cs
Mods/Network/NetDamage.cs
Mods/Network/NetDiagnostics.cs
Mods/Network/NetFeatureCheck.cs
Mods/Network/NetHitPrediction.cs
Mods/Network/NetHooks.cs
Mods/Network/NetHostSession.cs
Mods/Network/NetLaunch.cs
Mods/Network/NetLog.cs
Mods/Network/NetMatchEnd.cs
Mods/Network/NetMatchSync.cs
Mods/Network/NetPlayerBridge.cs
Mods/Network/NetPlayerSetup.cs
Mods/Network/NetProtocol.cs
Mods/Network/NetRoomChange.cs
Mods/Network/NetScoreboard.cs
Mods/Network/NetSession.cs
Mods/Network/NetSlotManager.cs
Mods/Network/NetTestScript.cs
Mods/Network/NetUnlagged.cs
Mods/Network/PlayerColors.cs
Mods/Network/ServerSim.cs
Mods/Network/ServerSimCheck.cs
Mods/Network/WeaponDps.cs
```

`PlayerEntityNetAim.cs` and `PlayerEntityNetHud.cs` remain owned by the Wave 5 partial declaration cluster even though their runtime behavior is validated again here.

### EntityBase/Network cycle

`EntityBase.cs` references networking while much of networking references entities.

Do not invent an `INetworkEntity` or new event bus unless such an abstraction exists in C#.

The cycle should be handled through:

- C++ declaration ordering;
- forward declarations;
- pointers/references where the C# object relationship already implies indirection;
- joint integration testing.

### Wave 6 oracle

Mandatory protocol tests:

- byte-for-byte packet serialization;
- exact signedness and endianness;
- unknown packet values;
- short/truncated packet behavior;
- connection state transitions;
- sequence/ack behavior;
- timeout behavior;
- snapshot application order;
- host/client authority;
- lag/loss simulation;
- server slot lifecycle.

Run the C# `tools/nettest`-equivalent oracle because the C# project intentionally compiles `NetProtocol.cs`, `NetTransport.cs`, and `NetProbe.cs` independently.

---

# Wave 7 - Map generation, export and repacking

**BLOCKED until Format/Entity contracts are closed.**

## Map generation

```text
Mods/MapGen/BuiltMap.cs
Mods/MapGen/MapBuilder.cs
Mods/MapGen/MapCollisionPacker.cs
Mods/MapGen/MapNodePacker.cs
Mods/MapGen/MapPacker.cs
Mods/MapGen/Q3Import.cs
Mods/MapGen/RepackAccess.cs
```

The leaf MapGen files from Waves 1-2 join this integration gate:

```text
CustomRooms.cs
MapBundle.cs
MapDefinition.cs
MapReport.cs
MapTextureBake.cs
MapTexturePack.cs
Q3Bsp.cs
Q3Convert.cs
RawStructs.cs
```

## Utility repacking

```text
Utility/RepackCollision.cs
Utility/RepackEntity.cs
Utility/RepackModel.cs
```

## Export/read

```text
Export/Collada.cs
Export/Images.cs
Export/Scripting.cs
Read.cs
```

### Wave 7 oracle

Use fixture inputs and compare:

```text
exact generated bytes
file count
file names
ordering
alignment
offsets
padding
compression result where deterministic
reported diagnostics
exception types
```

For textual exports, compare exact UTF encoding and culture-sensitive formatting.

---

# Wave 8 - Renderer, input and HUD integration

**BLOCKED until Scene, Entity and Player declaration clusters are closed.**

## Render

```text
Mods/Render/GlEs.cs
Mods/Render/HunterPreview.cs
Mods/Render/PreviewPass.cs
Mods/Render/SmoothHudIcon.cs
```

plus earlier render leaves:

```text
Crosshair.cs
EsBindings.cs
EsShaders.cs
FrameTiming.cs
FrameTimingCheck.cs
PreviewCamera.cs
PlayerEntityIconBounds.cs
```

and the Player partial files already owned by Wave 5:

```text
PlayerEntityAmmoClear.cs
PlayerEntityEndScreen.cs
PlayerEntityProHud.cs
PlayerEntityStylusHud.cs
PlayerEntityVoteHud.cs
```

## Input

```text
Mods/Input/GamepadInput.cs
Mods/Input/PointerInput.cs
Mods/Input/SyntheticInput.cs
```

plus the Wave 1-2 Input foundations:

```text
GamepadDesktop.cs
GamepadLayout.cs
GamepadMappings.cs
GamepadProbe.cs
GamepadState.cs
PadBindings.cs
StylusZone.cs
TouchSettings.cs
```

## Root renderer-facing source

```text
Renderer.cs
Selection.cs
```

### Wave 8 oracle

Separate:

1. deterministic CPU-state parity;
2. GL/OpenTK adapter parity;
3. visual output parity.

For frame/input tests compare:

```text
input sampling order
pressed/released transitions
mouse/stylus coordinates
dead zones
frame delta
clamping
camera state
draw-list ordering
uniform values
viewport/scissor
blend/depth state
resource lifetime
```

Visual screenshots alone are insufficient.

---

# Wave 9 - Audio SCC

**BLOCKED until external SoundFlow/OpenAL/NcsfPlay contracts and required Entity/Format types are fixed.**

Files:

```text
Formats/Sound.cs
Sound/Music.cs
Sound/Sfx.cs
Mods/Sound/AlEs.cs
Mods/Sound/SfxMixer.cs
```

`Formats/FhSound.cs` is a lower-level prerequisite from Wave 1.

### Why it is an SCC

The format layer describes audio structures consumed by runtime sound code, while portions of format/audio parsing reference runtime sound facilities. Music additionally crosses into `NcsfPlay` and SoundFlow.

Do not replace this with one Native audio subsystem that merely “sounds right.”

### Wave 9 oracle

Compare:

```text
sample decoding
sample counts
loop points
channel ownership
source/buffer lifecycle
gain/pitch
music transitions
fades
stop/pause/resume
missing-device behavior
failure timing
async completion
```

---

# Wave 10 - Mod orchestration and update/platform services

**BLOCKED until the subsystems each file invokes are available.**

## Mods

```text
Mods/DebugLog.cs
Mods/EndScreen.cs
Mods/GameSettings.cs
Mods/InputSettings.cs
Mods/LogShare.cs
Mods/PauseMenu.cs
Mods/RespawnChoice.cs
Mods/ScreenCapture.cs
Mods/SpectatorMode.cs
Mods/ThumbnailBatch.cs
Mods/ThumbnailCapture.cs
Mods/ThumbnailGenerator.cs
Mods/ThumbnailHost.cs
Mods/ThumbnailLog.cs
Mods/ThumbnailMode.cs
Mods/WindowMode.cs
Mods/WorldEvents.cs
```

Earlier Mod leaves participating in integration:

```text
Branding.cs
ConsoleWindow.cs
Credits.cs
Headless.cs
HunterSuits.cs
RenderOptions.cs
ShutdownSignals.cs
```

## Chat runtime

```text
Mods/Chat/ChatBox.cs
```

`ChatFont.cs` was Wave 1; `PlayerEntityChatHud.cs` belongs to the Player partial cluster.

## Update

```text
Mods/Update/DesktopUpdate.cs
Mods/Update/ServerUpdate.cs
Mods/Update/UpdateInstall.cs
Mods/Update/Updater.cs
```

plus:

```text
BuildVersion.cs
SyncHttp.cs
UpdateCheck.cs
UpdateDownload.cs
```

## Root game state/menu

```text
GameState.cs
Menu.cs
```

### Wave 10 oracle

Use temporary directories/process fixtures and compare:

```text
preference defaults
load/save round trip
logging attachment timing
update state transitions
HTTP cancellation
download temp-file behavior
install/apply sequencing
process argument quoting
cleanup
window mode defaults
thumbnail lifecycle
headless behavior
```

No Native “safer” fallback is allowed unless C# has it.

---

# Wave 11 - Launcher closure

**DO NOT ATTEMPT YET as an integrated launcher.**

Portable leaf/value files may be handled in earlier waves, but launcher behavior requires the gameplay, update, map, thumbnail, networking and platform contracts.

## Portable launcher

```text
Mods/Launcher/Portable/AdventureSave.cs
Mods/Launcher/Portable/GameFiles.cs
Mods/Launcher/Portable/LauncherPrefs.cs
Mods/Launcher/Portable/LaunchPlan.cs
Mods/Launcher/Portable/MatchStart.cs
Mods/Launcher/Portable/SetupProgress.cs
Mods/Launcher/Portable/TextLauncher.cs
```

File-level order:

```text
AdventureSave / GameFiles / LaunchPlan / SetupProgress
    -> LauncherPrefs
    -> MatchStart
    -> TextLauncher
```

## GUI launcher

```text
Mods/Launcher/Gui/CrosshairPreview.cs
Mods/Launcher/Gui/DemoPickerView.cs
Mods/Launcher/Gui/GuiLauncher.cs
Mods/Launcher/Gui/GuiTheme.cs
Mods/Launcher/Gui/HomeView.cs
Mods/Launcher/Gui/HomeWindow.cs
Mods/Launcher/Gui/KeyRow.cs
Mods/Launcher/Gui/MapPickerView.cs
Mods/Launcher/Gui/MenuEntry.cs
Mods/Launcher/Gui/PadRow.cs
Mods/Launcher/Gui/PauseMenuView.cs
Mods/Launcher/Gui/PauseMenuWindow.cs
Mods/Launcher/Gui/ProgressRow.cs
Mods/Launcher/Gui/Rows.cs
Mods/Launcher/Gui/ServerRow.cs
Mods/Launcher/Gui/SettingsView.cs
Mods/Launcher/Gui/SettingsWindow.cs
Mods/Launcher/Gui/SliderRow.cs
Mods/Launcher/Gui/SplashView.cs
Mods/Launcher/Gui/TrackedText.cs
Mods/Launcher/Gui/UiCapture.cs
Mods/Launcher/Gui/UpdateBadge.cs
```

File-level rule:

1. visual/value controls with no project dependencies;
2. controls consuming Input/Render/Network contracts;
3. individual views/windows;
4. `HomeView`;
5. `HomeWindow`;
6. `GuiLauncher` last.

`GuiLauncher` is not the process entry point.

### Wave 11 oracle

Run C# and Native launcher scenarios with identical synthetic selections:

```text
fresh install/no paths
existing install
offline match
online match
server browser
settings changes
update available/unavailable
GUI available
GUI probe failure
forced text mode
return from match
second launcher visit
```

Compare resulting `LaunchPlan`, persistent files, state, and dispatch path.

A screenshot match does not prove launcher parity.

---

# Wave 12 - Final root/runtime integration

**DO NOT ATTEMPT YET.**

Files:

```text
BeamProjectileEntity.cs
BombEntity.cs
Program.cs
Test.cs
```

The two projectile/bomb files are intentionally delayed because they cross Effects, Entity, Enemy, Culling and Network boundaries and are poor foundation candidates despite residing in `Entities`.

Remaining root integration files already owned by prior waves are now jointly validated:

```text
Features.cs
GameState.cs
Memory.cs
MemoryArrays.cs
MemoryClasses.cs
Menu.cs
Messaging.cs
Read.cs
Renderer.cs
Scene.cs
SceneSetup.cs
Selection.cs
Shaders.cs
Strings.cs
```

The central Mod integration unit is now eligible:

```text
Mods/ModEntry.cs
```

Final file-level order:

```text
all lower subsystem gates
    -> ModEntry.cs
    -> Program.cs
```

`Program.cs` must be the final production unit.

### Program oracle

Exercise every top-level route against C#:

```text
empty argv on Windows
empty argv on macOS
empty argv on Linux
server empty argv
-launcher
-text
-menu
-debuglog
-noupdate
update-apply invocation
-masterserver
-server
-dedicated
-servers
map bundle tooling
normal setup check
setup command
export variants
extract
room/model execution
invalid arguments
```

Record:

```text
called methods and order
stdout/stderr
exit code
files touched
process spawning
exceptions
whether setup validation occurred
whether GUI/text launcher ran
```

The Native host `main`/`WinMain` must remain a thin call adapter to the translated `Program.Main` behavior.

---

# Wave 13 - Test/diagnostic sources

Production dependencies must already be closed.

Files:

```text
Testing/TestEffects.cs
Testing/TestLogic.cs
Testing/TestMisc.cs
Testing/TestOverlay.cs
Testing/TestParse.cs
Testing/TestPlayer.cs
Testing/TestPrint.cs
Testing/TestWeapons.cs
```

`Test.cs` is validated with the root integration wave because it is in the main namespace and may expose production-facing dispatch.

These files are themselves part of the 302-file one-for-one requirement. They are not optional merely because they are tests.

### Wave 13 oracle

Where a C# test encodes expected values, Native must reproduce the test logic itself, not just independently produce the expected result.

---

# 6. Files that must not be attempted yet

As of this dependency review, the following are specifically unsuitable for isolated implementation:

```text
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
all PlayerEntity partial files as independent units
all Scene partial files as independent units
network gameplay files that consume PlayerEntity/Scene
```

Existing Native counterparts for any of these do not change that status.

They may be **read and audited**, but an implementer must not fill missing dependencies with C++-only substitutes to obtain a local build.

---

# 7. Next two parallel leaves

The safest next two independent jobs are:

## Leaf A - `Mods/Chat/ChatFont.cs`

Why:

- no project runtime dependency;
- only `System`;
- no OpenTK/Avalonia/network/entity object contract;
- deterministic static initialization;
- self-contained observable API;
- currently no Native pair.

Required oracle:

1. `Cell`, `First`, `Last`, derived count;
2. exact `Pixels.Length` and `Widths.Length`;
3. static-constructor initialization ordering;
4. complete byte-for-byte `Pixels` array after type initialization;
5. complete `Widths` array;
6. `Index()` for:
   - below `' '`
   - `' '`
   - printable range
   - `'~'`
   - above `'~'`;
7. `Measure()`:
   - empty;
   - unsupported characters;
   - all 95 supported glyphs;
   - mixed strings;
8. every authored glyph, not a representative sample.

The C++ pair must reproduce the generated tables, not substitute another font.

---

## Leaf B - `Mods/Input/StylusZone.cs`

Why:

- no project-internal runtime dependency;
- only `System.Math` operations and local types/state;
- deterministic;
- no OS/input API despite being an Input file;
- currently no Native pair.

Required oracle:

1. exact `StylusRegion` ordinals;
2. nested `Button` field types/order/value semantics;
3. exact `Buttons` array values/order;
4. all initial static property values;
5. `Height` calculation;
6. `SetRect()` clamp boundaries;
7. placement state machine:
   - Begin
   - Cancel
   - Down
   - Drag
   - Up;
8. `Update()` transition behavior;
9. sliding contact from one button to another;
10. `RegionAt()`:
    - every boundary;
    - exact right/bottom exclusion;
    - circle-edge equality;
    - outside zone;
    - Aim region;
11. `OnButton`;
12. `Reset`;
13. float special cases where .NET `Math.Clamp/Abs/Min/Max` behavior is observable.

These two leaves share no C# runtime state and can be reviewed in parallel.

---

# 8. Standard C#-oracle gate for every file

Every file-level review must answer all of these before PASS.

## API

```text
namespace
type visibility
type kind
inheritance/interfaces
partial/static/sealed/abstract status
generic parameters
field accessibility
field mutability
property getter/setter visibility
method overloads
parameter types
ref/out/in semantics
return types
nested types
enum underlying type/value
attributes whose effects are observable
```

## Initialization

```text
zero/default state
field initializer order
static initializer order
lazy initialization
singleton state
first-use behavior
repeated initialization
```

## Values

```text
integer overflow/wrap
signed/unsigned conversions
floating-point constants
NaN/infinity/signed-zero cases where relevant
string encoding
culture
enum unknown values
Flags formatting/parsing
```

## Control flow

```text
branch order
short-circuiting
loop order
early returns
null branches
exception timing
finally/disposal behavior
```

## Object semantics

```text
class reference identity
struct copying
readonly behavior
array aliasing
collection mutability
delegate capture
event registration lifetime
virtual dispatch
partial-class private-state access
```

## Side effects

```text
console
files
network
GL/audio
processes
environment variables
threading
timers
random state
global/static state
```

A Native implementation that reaches the same common-case output through different observable side effects is not a parity PASS.

---

# 9. Wave-level closure rule

A wave closes only when:

```text
1. every scheduled C# file has exactly one canonical Native pair;
2. its pair is in the mirrored functional location;
3. all file-level C# oracles pass;
4. no pair depends on a fabricated Native-only behavioral stub;
5. all SCC aggregate tests pass;
6. all external adapter calls have documented C# source justification;
7. public/type/layout differences are accounted for;
8. release/server/Android conditional paths affected by the wave are checked.
```

Only then may the wave be used as a prerequisite for the next wave.

---

# 10. Complete 302-file coverage inventory

The waves above cover the complete C# inventory:

```text
.                                      16
Entities                               25
Entities/CamSeq                         2
Entities/Enemies                       42
Entities/Players                       14
Export                                  3
Formats                                18
HUD                                     1
Metadata                                7
Mods                                   25
Mods/Chat                               3
Mods/Input                             11
Mods/Launcher/Gui                      22
Mods/Launcher/Portable                  7
Mods/MapGen                            16
Mods/Network                           43
Mods/Render                            16
Mods/Sound                              2
Mods/Update                             8
Sound                                   2
Testing                                 8
Utility                                11
-----------------------------------------
TOTAL                                 302
```

No `src/MphRead/**/*.cs` file is excluded from the strict migration.

---

# 11. Architecture verdict

The migration should **not** continue by taking arbitrary `missing` rows from the current dependency index.

The existing documents correctly establish coverage, external packages, and the fact that most Native counterparts do not yet exist, but their namespace/regex extraction is insufficient to determine implementation order.

The governing architecture should instead be:

```text
runtime semantics
    ↓
verified atomic leaves
    ↓
data/serialization foundations
    ↓
Format + Entity + Scene SCC
    ↓
Enemies
    ↓
Player partial SCC
    ↓
Network
    ↓
MapGen / export / repack
    ↓
Renderer / Input / HUD
    ↓
Audio
    ↓
Mod/update orchestration
    ↓
Launcher
    ↓
ModEntry
    ↓
Program.Main
    ↓
remaining test/diagnostic closure
```

with partial classes and true cyclic subsystems treated as clusters rather than artificially broken by C++-only architecture.

**Immediate next parallel work:**

```text
A. Mods/Chat/ChatFont.cs
B. Mods/Input/StylusZone.cs
```

Neither requires waiting for the gameplay, network, renderer, launcher, or audio closure.
