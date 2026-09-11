# MphRead.Native authoritative C# -> C++20 implementation order

## 1. Reviewed state and authority

**Reviewed `develop2` commit:** `de17012c4bc5cd80238dd040d3797c810271f392`  
**C# authority:** `src/MphRead/**/*.cs`  
**Native target:** `src/MphRead.Native/**/*.hpp` + `src/MphRead.Native/**/*.cpp`

This document is the authoritative implementation-order plan for the strict MphRead C# -> C++20 migration. It supersedes earlier ordering commentary where that commentary conflicts with the live source, the structural migration contract, or the dependency/closure distinctions defined here.

The current source inventory is **302 C# files**. The current Native inventory is **48 existing hpp/cpp pairs**: **47** already at the matching relative path and **1 relocated pair** (`Mods/Launcher/Portable/SetupProgress.cs` currently has Native files under `Mods/Launcher/SetupProgress.*`). Therefore **254 C# files currently have no Native pair**. These are inventory facts only; **an existing Native counterpart is never evidence that the file is parity-complete**.

The machine-generated C# dependency inventory was produced at `2da573aa860507eab7150dfd3a69667528721063`. GitHub comparison from that snapshot through the reviewed commit shows changes only under `docs/`; there are no changes under `src/MphRead`, `src/MphRead.Native`, or the project/build source. The source inventory used for the 302-file machine set is therefore source-identical to the reviewed commit.

The final assignment below contains **302 scheduled rows and 302 unique C# paths**. Set reconciliation is:

- source paths: **302**
- scheduled rows: **302**
- scheduled unique paths: **302**
- missing source paths: **0**
- extra scheduled paths: **0**
- duplicate scheduled paths: **0**
- authoring waves: **15**

The waves are an **authoring order**, not a claim that every earlier wave is behavior-complete before every later wave. Declaration readiness, compile/link readiness, and behavioral parity completion are separate states.

## 2. Non-negotiable migration rules

1. **C# is the only behavioral specification.** Existing Native code is untrusted migration evidence and must be re-audited from the full current C# file before it can pass.
2. **One C# source file maps to one Native `.hpp/.cpp` pair at the same relative path and stem.** The header stays beside its `.cpp`; do not move counterpart headers into a generic include directory.
3. **Do not introduce C#-unmatched policy/support source or aggregation headers.** In particular, do not create `*.Partials.hpp`, generic aggregate ownership headers, or other Native-only files merely to break cycles.
4. **Do not introduce Native-only application policy.** No extra retry, fallback, normalization, caching, logging, state ownership, validation, or error swallowing unless the C# source specifies it.
5. **Thin platform/runtime/library adapters are permitted only where unavoidable.** They implement mechanics, not program policy, and must preserve observable C# semantics.
6. **Partial C# types retain per-file counterpart ownership.** Every contributing `.cs` still gets its own colocated `.hpp/.cpp`, while the merged C++ type has one canonical declaration owner. Contributor headers must not define duplicate copies of the merged class/static type.
7. **No placeholder, dummy return, no-op body, stand-in class, `void*` escape hatch, or broadened template/overload contract may count as migrated behavior.**
8. **A compile success is not parity.** A file is complete only after its applicable structural, semantic, binary, initialization/lifetime, platform, and C# differential gates pass.
9. **Preserve C# initialization and lifetime timing.** Do not replace first-use static initialization, async/task ownership, cancellation, callbacks, socket/device/window lifetime, or exception timing with convenient C++ policy.
10. **Build-condition semantics are specification.** Desktop client, server, and Android share C# behavior but expose different mechanical host/library surfaces. Do not collapse those branches into one invented Native behavior.

## 3. Three dependency graphs must be tracked separately

The migration must maintain three related but non-equivalent graphs.

### 3.1 Declaration / forward-declaration graph

This graph answers what a C++ translation unit must know at declaration time.

- pointer/reference use may need only a forward declaration;
- by-value fields, inheritance, nested value types, templates requiring complete types, and exact ABI/layout require complete declarations;
- same-namespace and enclosing-namespace references must be inspected explicitly because a namespace-based index can miss them;
- a partial type must have one canonical class/static-type declaration owner before multiple contributor bodies are integrated.

A declaration seam may make an earlier file **authorable**. It does not make the dependency behavior-complete.

### 3.2 Compile / link / member graph

This graph includes actual called/accessed members, private cross-partial members, generated overload choices, templates/generic translations, and linked bodies. It must model:

- same/enclosing-namespace member references;
- overload resolution, optional/default values, `ref`/`out`, readonly/mutable struct behavior;
- generic constraints such as `struct`, `unmanaged`, inheritance, enum/type constraints;
- private-member access across C# partial contributors;
- conditional members compiled under `MPHREAD_AVALONIA`, `MPHREAD_SERVER`, OS symbols, or Android aliases.

### 3.3 Behavioral parity completion graph

This graph decides when a file or aggregate can be marked `PARITY-PASS`. It includes compile/link dependencies plus:

- static initialization and first-use timing;
- shared partial-type state;
- process/thread/task/cancellation lifetime;
- callbacks and event ordering;
- socket/device/window/resource ownership;
- culture/string/encoding behavior;
- file/path/process semantics;
- platform/build-condition branches;
- binary/protocol/layout compatibility;
- C# oracle/differential results.

A consumer may be authored against a frozen declaration while its provider remains behaviorally open. Do not translate that authoring convenience into an acceptance dependency.

## 4. Real process entry and launcher order

`src/MphRead/Program.cs` is the true desktop/server process specification. The launcher is downstream.

The observable startup sequence is:

```text
src/MphRead/Program.cs
  -> Program.Main(string[] args)
       -> ConsoleSetup.Run()
       -> ConsoleWindow.Prepare(args)                 [Windows]
       -> Mods.ModEntry.TryHandleHeadless(args)
            -> launcher/headless dispatch when requested
                 -> GuiLauncher.TryRun()              [MPHREAD_AVALONIA path]
                      -> true: GUI handled the launch
                      -> false: TextLauncher.Run()
                 -> TextLauncher.Run() directly       [when GUI path is unavailable/not selected]
       -> CheckSetup(args)                            [only if not already handled]
       -> ParseArguments(args)
       -> Mods.ModEntry.TryHandle(args)
       -> normal menu/read/render/export dispatch
```

Consequences:

- `Program.cs` is **W15**, the final integration unit.
- `Mods/ModEntry.cs` is **W14**, immediately before `Program.cs`.
- `GuiLauncher` and `TextLauncher` are late downstream subsystems; they are not alternate process specifications.
- `TryHandleHeadless(args)` must run before ordinary setup validation/argument dispatch exactly as in C#.
- `MPHREAD_SERVER` excludes the GUI subtree; this is a build condition, not permission to bypass `Program.Main`.
- `src/MphRead.Android/MainActivity.cs` is an Android lifecycle/host boundary. It is not an alternate desktop specification.
- Any Native-only `main`, `WinMain`, service wrapper, platform bootstrap, or Android host may mechanically enter the translated program, but may not reorder or replace the C# dispatch policy.

## 5. Canonical declaration seams and partial-type closure

### 5.1 `Scene`

Canonical C++ declaration owner: `src/MphRead.Native/Scene.hpp`.

The live C# `Scene` aggregate has **six** contributors:

1. `src/MphRead/Scene.cs`
2. `src/MphRead/Messaging.cs`
3. `src/MphRead/Renderer.cs`
4. `src/MphRead/Formats/Movie.cs`
5. `src/MphRead/Mods/Render/PreviewCamera.cs`
6. `src/MphRead/Mods/Render/PreviewPass.cs`

Freeze the complete `Scene` declaration surface before W4 bodies need it. Contributor pairs implement the members owned by their C# files without defining another `Scene` class. Authoring spans W4/W8/W12; aggregate behavioral completion is no earlier than **W12**.

`Scene` and concrete entities are mutually dependent. Break the C++ compile cycle with exact declarations/forward declarations and out-of-line definitions, not fake scene/entity stand-ins.

### 5.2 `PlayerEntity`

Canonical declaration owner: `src/MphRead.Native/Entities/Players/PlayerEntity.hpp`.

The aggregate has **21** contributors:

- `Entities/Players/PlayerAi.cs`
- `Entities/Players/PlayerCamera.cs`
- `Entities/Players/PlayerCollision.cs`
- `Entities/Players/PlayerDialog.cs`
- `Entities/Players/PlayerDraw.cs`
- `Entities/Players/PlayerEntity.cs`
- `Entities/Players/PlayerHud.cs`
- `Entities/Players/PlayerInput.cs`
- `Entities/Players/PlayerPause.cs`
- `Entities/Players/PlayerProcess.cs`
- `Entities/Players/PlayerScan.cs`
- `Entities/Players/PlayerSound.cs`
- `Mods/Chat/PlayerEntityChatHud.cs`
- `Mods/Network/PlayerEntityNetAim.cs`
- `Mods/Network/PlayerEntityNetHud.cs`
- `Mods/Render/PlayerEntityAmmoClear.cs`
- `Mods/Render/PlayerEntityEndScreen.cs`
- `Mods/Render/PlayerEntityIconBounds.cs`
- `Mods/Render/PlayerEntityProHud.cs`
- `Mods/Render/PlayerEntityStylusHud.cs`
- `Mods/Render/PlayerEntityVoteHud.cs`

Freeze one canonical declaration before the W7 player/gameplay body cluster. The late UI/render contributors keep the aggregate behaviorally open through **W12**.

### 5.3 `Metadata`

Canonical declaration owner: `src/MphRead.Native/Metadata/Metadata.hpp`.

Seven C# contributors form the static partial type:

- `Metadata/Enemies.cs`
- `Metadata/FrontendMeta.cs`
- `Metadata/Metadata.cs`
- `Metadata/Player.cs`
- `Metadata/Rooms.cs`
- `Metadata/SoundMeta.cs`
- `Metadata/Weapons.cs`

The declaration surface begins in W3. Static table/state parity cannot close until enemy/player/weapon contributors and their dependencies are complete, no earlier than **W7**.

### 5.4 `SoundRead`

Canonical declaration owner: `src/MphRead.Native/Formats/Sound.hpp`.

Two C# files contribute to the same partial static type:

- `Formats/FhSound.cs`
- `Formats/Sound.cs`

`FhSound.cs` calls the private `ExportSamples` supplied by `Sound.cs`, and the sound path also closes with `Sfx` behavior. Therefore `FhSound.cs` is placed in **W5**, not accepted as an early independent format leaf. `SoundRead` and its sound/Sfx closure must pass together.

### 5.5 `Repack` / `RepackCollision`

Canonical declaration owners:

- `Repack` -> `src/MphRead.Native/Utility/RepackEntity.hpp`
- `RepackCollision` -> `src/MphRead.Native/Utility/RepackCollision.hpp`

Contributors:

- `Repack`: `Utility/RepackEntity.cs`, `Utility/RepackModel.cs`, `Mods/MapGen/RepackAccess.cs`
- `RepackCollision`: `Utility/RepackCollision.cs`, `Mods/MapGen/RepackAccess.cs`

All contributor files still receive their own matching pair. The canonical owners hold the merged declarations. **Do not create `Repack.Partials.hpp` or any other unmatched aggregation header.** Both aggregates close in W9.

## 6. Platform, runtime, build, and external-library adapter boundaries

Adapters are permitted only for unavoidable mechanics required by the C# API surface.

### 6.1 .NET/runtime semantics

The Native runtime seam must reproduce, where used:

- .NET string/char/UTF-16, comparison, casing, interpolation, formatting, and culture behavior;
- arrays, spans/memory, collection semantics, nullable/default/reference behavior;
- signed/unsigned promotion, overflow/wrap, checked/unchecked conversions, floating-point edge behavior;
- enums, `[Flags]`, unknown enum values and observable formatting/conversion;
- exception type/timing when observable;
- `Task`/async/cancellation behavior and exception propagation;
- static initialization and first-use timing;
- filesystem/path/environment/process/console behavior;
- date/time/timer semantics;
- encoding and binary I/O.

These are semantic compatibility mechanisms, not places to simplify C# behavior.

### 6.2 Desktop libraries

The reviewed project references:

- OpenTK **4.9.4** for mathematics/GL/OpenAL-facing contracts;
- Avalonia **11.3.11** for the GUI launcher in `MPHREAD_AVALONIA` builds;
- ReFuel.StbImage **2.1.1**;
- Silk.NET.OpenAL.Soft.Native **1.23.1** for non-server OpenAL Soft runtime files;
- project reference `NcsfPlay`, whose current packages are CommunityToolkit.HighPerformance **8.4.2**, SoundFlow **1.4.1**, and System.IO.Hashing **10.0.9**.

Native translation may expose thin math, GL/GLES, OpenAL/ALC, audio-device, image-codec, UI-dispatcher/window, HTTP, socket/DNS, filesystem/process, and signal mechanics. Launcher, gameplay, update, network, audio, render, and error-handling policy remains in the translated per-file counterparts.

### 6.3 Server build

`MphReadServer=true` defines `MPHREAD_SERVER`; normal non-server builds define `MPHREAD_AVALONIA`. The project removes `Mods/Launcher/Gui/**` when Avalonia is not enabled. Server parity must therefore validate the server-specific compile surface and startup behavior without inventing a GUI dependency or bypassing `Program.Main`/`ModEntry`.

### 6.4 Android

`src/MphRead.Android` recompiles the shared MphRead sources for `net9.0-android35.0` with platform-specific removals and aliases:

- `GL` -> `Mods.Render.GlEs`
- `AL` -> `Mods.Sound.AlEs`
- `ALC` -> `Mods.Sound.AlcEs`

`MainActivity` supplies Android lifecycle hosting. These aliases and the activity are mechanical platform seams; they do not redefine shared C# gameplay/application semantics.

### 6.5 Network test project

`tools/nettest` directly compiles the network protocol/transport/probe source path and is a validation target for wire/layout/runtime compatibility. Test/build hosts may exercise translated code, but they may not become alternate production specifications.

## 7. Authoring waves, prerequisites, and closure barriers

| Wave | Files | Earliest safe authoring purpose | Completion note |
|---:|---:|---|---|
| W1 | 18 | Atomic deterministic leaves and tiny runtime/ABI contracts. | Per-file parity only after deterministic oracle checks. |
| W2 | 18 | Small data/runtime contracts that do not execute against later engine/launcher state. | Declaration availability is not behavior closure for later SCCs. |
| W3 | 9 | Export/HUD/metadata declaration surfaces and other small contracts. | `Metadata` remains open. |
| W4 | 22 | Core read/model/entity-layout/scene declaration work plus Read-consuming utilities. | Engine macro-SCC remains open. |
| W5 | 11 | SoundRead/Sfx-side closure, wire/demo primitives that are actually W5-safe, and server-independent network pieces. | `SoundRead` closes only with its audio/Sfx dependencies satisfied. |
| W6 | 66 | Bulk concrete entity/enemy bodies after core declarations are frozen. | Engine macro-SCC still open. |
| W7 | 67 | Player/game-state/gameplay-network/metadata body closure. | `Metadata` closes here; `PlayerEntity` remains open. |
| W8 | 14 | Renderer/movie/capture/preview bodies and thumbnail implementation. | `Scene` remains open through `PreviewPass`; `PlayerEntity` remains open. |
| W9 | 20 | Live-memory plus map/repack closure. | `Repack`/`RepackCollision` close here. |
| W10 | 6 | Update subsystem after HTTP/process adapter primitives exist. | Preserve C# async/cancellation/failure timing. |
| W11 | 14 | Portable launcher/logging/input-mapping/network-vote late closure. | Launcher/UI macro-SCC remains open. |
| W12 | 26 | Pause/window/GUI and late render partial closure. | `Scene` and `PlayerEntity` close here; launcher/UI closure must pass. |
| W13 | 9 | Test/oracle consumers and integration validation harnesses. | Tests never authorize production-only helpers. |
| W14 | 1 | `Mods/ModEntry.cs`. | Penultimate integration unit. |
| W15 | 1 | `Program.cs`. | True final process-entry integration unit. |
| **Total** | **302** |  |  |

### 7.1 Global micro-order inside a wave

When files in one wave are mutually connected, use this micro-order:

1. freeze canonical declaration owners and exact value/layout declarations;
2. add only necessary forward declarations;
3. implement independent contributor bodies;
4. implement bodies that require other contributor members;
5. link the complete local SCC with no placeholders;
6. run the applicable C# differential/oracle gates;
7. only then mark files/aggregates behaviorally complete.

A declaration-only stage is never `PARITY-PASS`.

### 7.2 Engine core macro-SCC

W4-W8 are authoring bands inside a larger engine closure, not a clean module DAG. The closure contains, among others:

```text
Read / model / formats
Scene / Messaging / Renderer / Movie / PreviewCamera / PreviewPass
EntityBase / concrete entities / enemies
PlayerEntity partials / GameState / SceneSetup / Menu
Metadata player/enemy/weapon contributors
gameplay network / NetSession / ServerSim
SoundRead / Sfx / Music
render/image/preview consumers
```

Do not expose an earlier band as an accepted behavioral prerequisite merely because its declarations compile. Required micro-order:

- freeze `Scene`, `Metadata`, and other layout-critical declarations before dependent bodies;
- author core Read/model/entity-layout/scene bodies;
- author concrete entities/enemies;
- freeze `PlayerEntity` declaration, then player/game-state/gameplay-network bodies;
- close metadata body/state;
- integrate renderer/movie/preview contributors;
- close `Scene` and `PlayerEntity` only after their W12 tails.

### 7.3 `SoundRead` / sound micro-order

- canonical `SoundRead` declaration comes from `Formats/Sound.hpp`;
- `Formats/FhSound.cs` and `Formats/Sound.cs` are one partial-type closure;
- private cross-partial member calls must bind to the canonical type;
- `Formats/Sound.cs` and `Sound/Sfx.cs` must close their mutual sound behavior;
- no side may be declared parity-complete against a placeholder on the other side.

### 7.4 Network micro-order

Wire declarations may be staged before high-level behavior, but behavior completion must respect real session/server edges:

- freeze protocol/config/packet declarations first;
- make transport/probe compile against exact wire declarations;
- keep demo playback/recording, `DedicatedServer`, `NetMaster`, `NetStatus`, and other `NetSession`/`GameState` consumers in the W7 gameplay closure;
- keep `MapVote` late because it reaches session plus end-screen/pointer UI contracts;
- validate protocol bytes/layout independently from high-level server/session behavior.

### 7.5 Live-memory / map / repack micro-order

`Memory.cs` is W9 with `MemoryClasses.cs`, not an early utility: it owns `Scene?` state and consumes AI/game memory classes. In W9:

- freeze exact memory wrapper/layout declarations;
- integrate `Memory.cs` with `MemoryClasses.cs`;
- freeze canonical `Repack`/`RepackCollision` declarations;
- implement utility/mapgen partial bodies including `Mods/MapGen/RepackAccess.cs`;
- require binary round-trip and layout parity before closure.

### 7.6 Launcher/UI macro-SCC

W11-W12 contain reverse edges and form a late closure:

```text
LauncherPrefs -> WindowMode.Parse
WindowMode -> PauseMenu.Open
DebugLog -> LauncherPrefs and runtime diagnostics
GamepadMappings -> LauncherPrefs
LogShare -> LauncherPrefs
ThumbnailLog -> GameFiles/build version
GuiLauncher/TextLauncher -> preferences/files/update/match/network state
MapVote -> EndScreen/pointer layout
```

Portable logic may be authored before GUI bodies where declarations permit it, but behavior closure waits for the reverse edges and applicable Avalonia/window/pause lifecycle.

### 7.7 Terminal integration

- W13 translates/tests oracle consumers only after production contracts exist.
- W14 is `Mods/ModEntry.cs`; its command/launcher/server routes must be differentially matched before acceptance.
- W15 is `Program.cs`; it is accepted only after the complete downstream startup and dispatch graph is parity-ready.

## 8. Exact 302-row file-to-wave assignment

Each current `src/MphRead/**/*.cs` source appears exactly once.

| Wave | C# source |
|---:|---|
| W1 | `src/MphRead/Formats/Enums.cs` |
| W1 | `src/MphRead/Formats/Frontend.cs` |
| W1 | `src/MphRead/Mods/Branding.cs` |
| W1 | `src/MphRead/Mods/Chat/ChatFont.cs` |
| W1 | `src/MphRead/Mods/ConsoleWindow.cs` |
| W1 | `src/MphRead/Mods/Credits.cs` |
| W1 | `src/MphRead/Mods/Headless.cs` |
| W1 | `src/MphRead/Mods/Input/StylusZone.cs` |
| W1 | `src/MphRead/Mods/Input/TouchSettings.cs` |
| W1 | `src/MphRead/Mods/Launcher/Portable/LaunchPlan.cs` |
| W1 | `src/MphRead/Mods/Launcher/Portable/SetupProgress.cs` |
| W1 | `src/MphRead/Mods/MapGen/MapTexturePack.cs` |
| W1 | `src/MphRead/Mods/Render/Crosshair.cs` |
| W1 | `src/MphRead/Mods/Update/BuildVersion.cs` |
| W1 | `src/MphRead/Shaders.cs` |
| W1 | `src/MphRead/Utility/Console.cs` |
| W1 | `src/MphRead/Utility/Output.cs` |
| W1 | `src/MphRead/Utility/Rng.cs` |
| W2 | `src/MphRead/Formats/Culling.cs` |
| W2 | `src/MphRead/Formats/Types.cs` |
| W2 | `src/MphRead/MemoryArrays.cs` |
| W2 | `src/MphRead/Mods/Input/GamepadLayout.cs` |
| W2 | `src/MphRead/Mods/Input/GamepadState.cs` |
| W2 | `src/MphRead/Mods/Input/PadBindings.cs` |
| W2 | `src/MphRead/Mods/Input/SyntheticInput.cs` |
| W2 | `src/MphRead/Mods/Network/NetLag.cs` |
| W2 | `src/MphRead/Mods/Render/EsBindings.cs` |
| W2 | `src/MphRead/Mods/Render/EsShaders.cs` |
| W2 | `src/MphRead/Mods/Render/FrameTiming.cs` |
| W2 | `src/MphRead/Mods/Render/FrameTimingCheck.cs` |
| W2 | `src/MphRead/Mods/Render/GlEs.cs` |
| W2 | `src/MphRead/Mods/RenderOptions.cs` |
| W2 | `src/MphRead/Mods/ShutdownSignals.cs` |
| W2 | `src/MphRead/Mods/Sound/AlEs.cs` |
| W2 | `src/MphRead/Mods/Update/SyncHttp.cs` |
| W2 | `src/MphRead/Utility/Compress.cs` |
| W3 | `src/MphRead/Export/Collada.cs` |
| W3 | `src/MphRead/Export/Scripting.cs` |
| W3 | `src/MphRead/Features.cs` |
| W3 | `src/MphRead/HUD/HudInfo.cs` |
| W3 | `src/MphRead/Metadata/FrontendMeta.cs` |
| W3 | `src/MphRead/Metadata/Metadata.cs` |
| W3 | `src/MphRead/Metadata/Rooms.cs` |
| W3 | `src/MphRead/Metadata/SoundMeta.cs` |
| W3 | `src/MphRead/Strings.cs` |
| W4 | `src/MphRead/Entities/EntityBase.cs` |
| W4 | `src/MphRead/Entities/LightSourceEntity.cs` |
| W4 | `src/MphRead/Entities/PlayerSpawnEntity.cs` |
| W4 | `src/MphRead/Entities/Players/DynamicLightEntity.cs` |
| W4 | `src/MphRead/Entities/PointModuleEntity.cs` |
| W4 | `src/MphRead/Formats/Collision.cs` |
| W4 | `src/MphRead/Formats/CollisionDetection.cs` |
| W4 | `src/MphRead/Formats/Effects.cs` |
| W4 | `src/MphRead/Formats/Entity.cs` |
| W4 | `src/MphRead/Formats/EntityClass.cs` |
| W4 | `src/MphRead/Formats/EntityEnemy.cs` |
| W4 | `src/MphRead/Formats/Formats.cs` |
| W4 | `src/MphRead/Formats/Model.cs` |
| W4 | `src/MphRead/Formats/NodeData.cs` |
| W4 | `src/MphRead/Formats/RawFormats.cs` |
| W4 | `src/MphRead/Messaging.cs` |
| W4 | `src/MphRead/Read.cs` |
| W4 | `src/MphRead/Scene.cs` |
| W4 | `src/MphRead/Utility/Analyzer.cs` |
| W4 | `src/MphRead/Utility/Archive.cs` |
| W4 | `src/MphRead/Utility/Extract.cs` |
| W4 | `src/MphRead/Utility/Parser.cs` |
| W5 | `src/MphRead/Formats/FhSound.cs` |
| W5 | `src/MphRead/Formats/Sound.cs` |
| W5 | `src/MphRead/Mods/Network/DemoFile.cs` |
| W5 | `src/MphRead/Mods/Network/DemoLibrary.cs` |
| W5 | `src/MphRead/Mods/Network/MapRotation.cs` |
| W5 | `src/MphRead/Mods/Network/NetProbe.cs` |
| W5 | `src/MphRead/Mods/Network/NetProtocol.cs` |
| W5 | `src/MphRead/Mods/Network/NetTransport.cs` |
| W5 | `src/MphRead/Mods/Sound/SfxMixer.cs` |
| W5 | `src/MphRead/Sound/Music.cs` |
| W5 | `src/MphRead/Sound/Sfx.cs` |
| W6 | `src/MphRead/Entities/AreaVolumeEntity.cs` |
| W6 | `src/MphRead/Entities/ArtifactEntity.cs` |
| W6 | `src/MphRead/Entities/BeamEffectEntity.cs` |
| W6 | `src/MphRead/Entities/BeamProjectileEntity.cs` |
| W6 | `src/MphRead/Entities/BombEntity.cs` |
| W6 | `src/MphRead/Entities/CamSeq/CamSeqEntity.cs` |
| W6 | `src/MphRead/Entities/CamSeq/CameraSequence.cs` |
| W6 | `src/MphRead/Entities/DoorEntity.cs` |
| W6 | `src/MphRead/Entities/Enemies/00_WarWasp.cs` |
| W6 | `src/MphRead/Entities/Enemies/01_Zoomer.cs` |
| W6 | `src/MphRead/Entities/Enemies/02_Temroid.cs` |
| W6 | `src/MphRead/Entities/Enemies/03_Petrasyl1.cs` |
| W6 | `src/MphRead/Entities/Enemies/04_Petrasyl2.cs` |
| W6 | `src/MphRead/Entities/Enemies/05_Petrasyl3.cs` |
| W6 | `src/MphRead/Entities/Enemies/06_Petrasyl4.cs` |
| W6 | `src/MphRead/Entities/Enemies/10_BarbedWarWasp.cs` |
| W6 | `src/MphRead/Entities/Enemies/11_Shriekbat.cs` |
| W6 | `src/MphRead/Entities/Enemies/12_Geemer.cs` |
| W6 | `src/MphRead/Entities/Enemies/16_Blastcap.cs` |
| W6 | `src/MphRead/Entities/Enemies/18_AlimbicTurret.cs` |
| W6 | `src/MphRead/Entities/Enemies/19_Cretaphid.cs` |
| W6 | `src/MphRead/Entities/Enemies/20_CretaphidEye.cs` |
| W6 | `src/MphRead/Entities/Enemies/21_CretaphidCrystal.cs` |
| W6 | `src/MphRead/Entities/Enemies/23_PsychoBit.cs` |
| W6 | `src/MphRead/Entities/Enemies/24_Gorea1A.cs` |
| W6 | `src/MphRead/Entities/Enemies/25_GoreaHead.cs` |
| W6 | `src/MphRead/Entities/Enemies/26_GoreaArm.cs` |
| W6 | `src/MphRead/Entities/Enemies/27_GoreaLeg.cs` |
| W6 | `src/MphRead/Entities/Enemies/28_Gorea1B.cs` |
| W6 | `src/MphRead/Entities/Enemies/29_GoreaSealSphere1.cs` |
| W6 | `src/MphRead/Entities/Enemies/30_Trocra.cs` |
| W6 | `src/MphRead/Entities/Enemies/31_Gorea2.cs` |
| W6 | `src/MphRead/Entities/Enemies/32_GoreaSealSphere2.cs` |
| W6 | `src/MphRead/Entities/Enemies/33_GoreaMeteor.cs` |
| W6 | `src/MphRead/Entities/Enemies/35_Voldrum.cs` |
| W6 | `src/MphRead/Entities/Enemies/36_Voldrum.cs` |
| W6 | `src/MphRead/Entities/Enemies/37_Quadtroid.cs` |
| W6 | `src/MphRead/Entities/Enemies/38_CrashPillar.cs` |
| W6 | `src/MphRead/Entities/Enemies/39_FireSpawn.cs` |
| W6 | `src/MphRead/Entities/Enemies/40_EnemySpawner.cs` |
| W6 | `src/MphRead/Entities/Enemies/41_Slench.cs` |
| W6 | `src/MphRead/Entities/Enemies/42_SlenchShield.cs` |
| W6 | `src/MphRead/Entities/Enemies/43_SlenchNest.cs` |
| W6 | `src/MphRead/Entities/Enemies/44_SlenchSynapse.cs` |
| W6 | `src/MphRead/Entities/Enemies/45_SlenchTurret.cs` |
| W6 | `src/MphRead/Entities/Enemies/46_LesserIthrak.cs` |
| W6 | `src/MphRead/Entities/Enemies/47_GreaterIthrak.cs` |
| W6 | `src/MphRead/Entities/Enemies/49_ForceFieldLock.cs` |
| W6 | `src/MphRead/Entities/Enemies/50_HitZone.cs` |
| W6 | `src/MphRead/Entities/Enemies/51_CarnivorousPlant.cs` |
| W6 | `src/MphRead/Entities/EnemyInstanceEntity.cs` |
| W6 | `src/MphRead/Entities/EnemySpawnEntity.cs` |
| W6 | `src/MphRead/Entities/FlagBaseEntity.cs` |
| W6 | `src/MphRead/Entities/ForceFieldEntity.cs` |
| W6 | `src/MphRead/Entities/ItemInstanceEntity.cs` |
| W6 | `src/MphRead/Entities/ItemSpawnEntity.cs` |
| W6 | `src/MphRead/Entities/JumpPadEntity.cs` |
| W6 | `src/MphRead/Entities/MorphCameraEntity.cs` |
| W6 | `src/MphRead/Entities/NodeDefenseEntity.cs` |
| W6 | `src/MphRead/Entities/ObjectEntity.cs` |
| W6 | `src/MphRead/Entities/OctolithFlagEntity.cs` |
| W6 | `src/MphRead/Entities/PlatformEntity.cs` |
| W6 | `src/MphRead/Entities/RoomEntity.cs` |
| W6 | `src/MphRead/Entities/TeleporterEntity.cs` |
| W6 | `src/MphRead/Entities/TriggerVolumeEntity.cs` |
| W6 | `src/MphRead/Metadata/Enemies.cs` |
| W7 | `src/MphRead/Entities/Players/HalfturretEntity.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerAi.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerCamera.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerCollision.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerDialog.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerDraw.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerEntity.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerHud.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerInput.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerPause.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerProcess.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerScan.cs` |
| W7 | `src/MphRead/Entities/Players/PlayerSound.cs` |
| W7 | `src/MphRead/Formats/AiPersonality.cs` |
| W7 | `src/MphRead/GameState.cs` |
| W7 | `src/MphRead/Menu.cs` |
| W7 | `src/MphRead/Metadata/Player.cs` |
| W7 | `src/MphRead/Metadata/Weapons.cs` |
| W7 | `src/MphRead/Mods/Chat/ChatBox.cs` |
| W7 | `src/MphRead/Mods/Chat/PlayerEntityChatHud.cs` |
| W7 | `src/MphRead/Mods/GameSettings.cs` |
| W7 | `src/MphRead/Mods/HunterSuits.cs` |
| W7 | `src/MphRead/Mods/Input/GamepadInput.cs` |
| W7 | `src/MphRead/Mods/InputSettings.cs` |
| W7 | `src/MphRead/Mods/Network/DedicatedServer.cs` |
| W7 | `src/MphRead/Mods/Network/DemoClip.cs` |
| W7 | `src/MphRead/Mods/Network/DemoInfo.cs` |
| W7 | `src/MphRead/Mods/Network/DemoPlayback.cs` |
| W7 | `src/MphRead/Mods/Network/DemoRecorder.cs` |
| W7 | `src/MphRead/Mods/Network/MapAudit.cs` |
| W7 | `src/MphRead/Mods/Network/MechanicsDump.cs` |
| W7 | `src/MphRead/Mods/Network/NetCheckClient.cs` |
| W7 | `src/MphRead/Mods/Network/NetConnectCommand.cs` |
| W7 | `src/MphRead/Mods/Network/NetDamage.cs` |
| W7 | `src/MphRead/Mods/Network/NetDiagnostics.cs` |
| W7 | `src/MphRead/Mods/Network/NetFeatureCheck.cs` |
| W7 | `src/MphRead/Mods/Network/NetHitPrediction.cs` |
| W7 | `src/MphRead/Mods/Network/NetHooks.cs` |
| W7 | `src/MphRead/Mods/Network/NetHostSession.cs` |
| W7 | `src/MphRead/Mods/Network/NetLaunch.cs` |
| W7 | `src/MphRead/Mods/Network/NetLog.cs` |
| W7 | `src/MphRead/Mods/Network/NetMaster.cs` |
| W7 | `src/MphRead/Mods/Network/NetMatchEnd.cs` |
| W7 | `src/MphRead/Mods/Network/NetMatchSync.cs` |
| W7 | `src/MphRead/Mods/Network/NetPlayerBridge.cs` |
| W7 | `src/MphRead/Mods/Network/NetPlayerSetup.cs` |
| W7 | `src/MphRead/Mods/Network/NetRoomChange.cs` |
| W7 | `src/MphRead/Mods/Network/NetScoreboard.cs` |
| W7 | `src/MphRead/Mods/Network/NetSession.cs` |
| W7 | `src/MphRead/Mods/Network/NetSlotManager.cs` |
| W7 | `src/MphRead/Mods/Network/NetStatus.cs` |
| W7 | `src/MphRead/Mods/Network/NetTestScript.cs` |
| W7 | `src/MphRead/Mods/Network/NetUnlagged.cs` |
| W7 | `src/MphRead/Mods/Network/PlayerColors.cs` |
| W7 | `src/MphRead/Mods/Network/PlayerEntityNetAim.cs` |
| W7 | `src/MphRead/Mods/Network/PlayerEntityNetHud.cs` |
| W7 | `src/MphRead/Mods/Network/ServerSim.cs` |
| W7 | `src/MphRead/Mods/Network/ServerSimCheck.cs` |
| W7 | `src/MphRead/Mods/Network/WeaponDps.cs` |
| W7 | `src/MphRead/Mods/Render/PlayerEntityAmmoClear.cs` |
| W7 | `src/MphRead/Mods/Render/PlayerEntityIconBounds.cs` |
| W7 | `src/MphRead/Mods/Render/PlayerEntityStylusHud.cs` |
| W7 | `src/MphRead/Mods/Render/PlayerEntityVoteHud.cs` |
| W7 | `src/MphRead/Mods/RespawnChoice.cs` |
| W7 | `src/MphRead/Mods/SpectatorMode.cs` |
| W7 | `src/MphRead/Mods/WorldEvents.cs` |
| W7 | `src/MphRead/SceneSetup.cs` |
| W8 | `src/MphRead/Export/Images.cs` |
| W8 | `src/MphRead/Formats/Movie.cs` |
| W8 | `src/MphRead/Mods/Render/HunterPreview.cs` |
| W8 | `src/MphRead/Mods/Render/PlayerEntityProHud.cs` |
| W8 | `src/MphRead/Mods/Render/PreviewCamera.cs` |
| W8 | `src/MphRead/Mods/Render/SmoothHudIcon.cs` |
| W8 | `src/MphRead/Mods/ScreenCapture.cs` |
| W8 | `src/MphRead/Mods/ThumbnailBatch.cs` |
| W8 | `src/MphRead/Mods/ThumbnailCapture.cs` |
| W8 | `src/MphRead/Mods/ThumbnailGenerator.cs` |
| W8 | `src/MphRead/Mods/ThumbnailHost.cs` |
| W8 | `src/MphRead/Mods/ThumbnailMode.cs` |
| W8 | `src/MphRead/Renderer.cs` |
| W8 | `src/MphRead/Selection.cs` |
| W9 | `src/MphRead/Memory.cs` |
| W9 | `src/MphRead/MemoryClasses.cs` |
| W9 | `src/MphRead/Mods/MapGen/BuiltMap.cs` |
| W9 | `src/MphRead/Mods/MapGen/CustomRooms.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapBuilder.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapBundle.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapCollisionPacker.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapDefinition.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapNodePacker.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapPacker.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapReport.cs` |
| W9 | `src/MphRead/Mods/MapGen/MapTextureBake.cs` |
| W9 | `src/MphRead/Mods/MapGen/Q3Bsp.cs` |
| W9 | `src/MphRead/Mods/MapGen/Q3Convert.cs` |
| W9 | `src/MphRead/Mods/MapGen/Q3Import.cs` |
| W9 | `src/MphRead/Mods/MapGen/RawStructs.cs` |
| W9 | `src/MphRead/Mods/MapGen/RepackAccess.cs` |
| W9 | `src/MphRead/Utility/RepackCollision.cs` |
| W9 | `src/MphRead/Utility/RepackEntity.cs` |
| W9 | `src/MphRead/Utility/RepackModel.cs` |
| W10 | `src/MphRead/Mods/Update/DesktopUpdate.cs` |
| W10 | `src/MphRead/Mods/Update/ServerUpdate.cs` |
| W10 | `src/MphRead/Mods/Update/UpdateCheck.cs` |
| W10 | `src/MphRead/Mods/Update/UpdateDownload.cs` |
| W10 | `src/MphRead/Mods/Update/UpdateInstall.cs` |
| W10 | `src/MphRead/Mods/Update/Updater.cs` |
| W11 | `src/MphRead/Mods/DebugLog.cs` |
| W11 | `src/MphRead/Mods/EndScreen.cs` |
| W11 | `src/MphRead/Mods/Input/GamepadDesktop.cs` |
| W11 | `src/MphRead/Mods/Input/GamepadMappings.cs` |
| W11 | `src/MphRead/Mods/Input/GamepadProbe.cs` |
| W11 | `src/MphRead/Mods/Input/PointerInput.cs` |
| W11 | `src/MphRead/Mods/Launcher/Portable/AdventureSave.cs` |
| W11 | `src/MphRead/Mods/Launcher/Portable/GameFiles.cs` |
| W11 | `src/MphRead/Mods/Launcher/Portable/LauncherPrefs.cs` |
| W11 | `src/MphRead/Mods/Launcher/Portable/MatchStart.cs` |
| W11 | `src/MphRead/Mods/Launcher/Portable/TextLauncher.cs` |
| W11 | `src/MphRead/Mods/LogShare.cs` |
| W11 | `src/MphRead/Mods/Network/MapVote.cs` |
| W11 | `src/MphRead/Mods/ThumbnailLog.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/CrosshairPreview.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/DemoPickerView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/GuiLauncher.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/GuiTheme.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/HomeView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/HomeWindow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/KeyRow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/MapPickerView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/MenuEntry.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/PadRow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/PauseMenuView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/PauseMenuWindow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/ProgressRow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/Rows.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/ServerRow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/SettingsView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/SettingsWindow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/SliderRow.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/SplashView.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/TrackedText.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/UiCapture.cs` |
| W12 | `src/MphRead/Mods/Launcher/Gui/UpdateBadge.cs` |
| W12 | `src/MphRead/Mods/PauseMenu.cs` |
| W12 | `src/MphRead/Mods/Render/PlayerEntityEndScreen.cs` |
| W12 | `src/MphRead/Mods/Render/PreviewPass.cs` |
| W12 | `src/MphRead/Mods/WindowMode.cs` |
| W13 | `src/MphRead/Test.cs` |
| W13 | `src/MphRead/Testing/TestEffects.cs` |
| W13 | `src/MphRead/Testing/TestLogic.cs` |
| W13 | `src/MphRead/Testing/TestMisc.cs` |
| W13 | `src/MphRead/Testing/TestOverlay.cs` |
| W13 | `src/MphRead/Testing/TestParse.cs` |
| W13 | `src/MphRead/Testing/TestPlayer.cs` |
| W13 | `src/MphRead/Testing/TestPrint.cs` |
| W13 | `src/MphRead/Testing/TestWeapons.cs` |
| W14 | `src/MphRead/Mods/ModEntry.cs` |
| W15 | `src/MphRead/Program.cs` |

## 9. Blocked and special-case Native files

These cases require explicit correction during their own future migration unit; this planning task does not edit C++.

### 9.1 `PlayerEntityIconBounds`

The existing `src/MphRead.Native/Mods/Render/PlayerEntityIconBounds.hpp` defines an isolated `PlayerEntity` shape. That cannot survive final integration because the C# specification has one 21-file partial `PlayerEntity`. Its future implementation must bind to the canonical `Entities/Players/PlayerEntity.hpp` declaration and must not introduce a second class or C++-only callable policy.

### 9.2 `SetupProgress`

C# source:

`src/MphRead/Mods/Launcher/Portable/SetupProgress.cs`

Current relocated Native pair:

`src/MphRead.Native/Mods/Launcher/SetupProgress.hpp/.cpp`

Strict final target:

`src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp/.cpp`

The relocated pair is inventory evidence, not structural completion.

### 9.3 Existing `Scene`/preview stand-ins

Any existing truncated/stand-in `Scene`, vector/math, or other type declaration inside a contributor counterpart is scaffolding only. Final integration must use the canonical declaration owner and exact adapter semantics; duplicate stand-in classes are prohibited.

### 9.4 Existing entity/enemy counterparts

An existing pair that includes missing/unimplemented base declarations or otherwise compiles only in isolation is not a satisfied prerequisite. Re-audit it against the full C# file and integrate it only after exact prerequisite declarations/bodies exist.

### 9.5 Existing `Program.cpp`

Native process-host code cannot make `Program.cs` complete early. `Program` stays W15 and is judged against the C# entry/dispatch order. In particular, C# culture-sensitive operations such as `exportValue.ToLower()` must not silently become invariant/native convenience semantics.

### 9.6 External/backend and conditional files

Files touching Avalonia, OpenGL/GLES, OpenAL/ALC, SoundFlow/NCSF, sockets, HTTP, process/filesystem APIs, Android lifecycle, OS signals, or server/client compile symbols are not blocked merely because the external API differs. They require the smallest mechanical adapter plus differential tests. The adapter may not absorb application policy.

## 10. Validation and acceptance gates

A counterpart is `PARITY-PASS` only after every applicable gate below passes. Existing Native code starts as **unreviewed**, regardless of age or apparent completeness.

### Gate 0 - inventory/assignment

- exact current `src/MphRead/**/*.cs` set is enumerated;
- exactly one scheduled row per C# source;
- no extra, missing, or duplicate rows;
- each source has exactly one intended same-relative-path `.hpp/.cpp` target.

Current plan result: **302 source / 302 rows / 302 unique / missing 0 / extra 0 / duplicate 0**.

### Gate 1 - structural/declaration

- correct namespace/type visibility;
- correct canonical owner for every partial type;
- no duplicate `Scene`, `PlayerEntity`, `Metadata`, `SoundRead`, `Repack`, or `RepackCollision`;
- no C#-unmatched aggregation/support header;
- exact by-value layout prerequisites available;
- same/enclosing-namespace references reviewed.

### Gate 2 - language semantics

Differential/oracle tests must cover as applicable:

- constants and static defaults;
- enum underlying values, `[Flags]`, aliases, unknown values and formatting;
- signed/unsigned arithmetic, overflow/wrap, conversion and division behavior;
- float bit preservation, NaN/infinity/signed-zero/boundary behavior;
- value/reference/copy/default-construction semantics;
- `ref`/`out`, readonly/mutable structs;
- overload selection, optional/default arguments;
- generic constraints and reflection/non-public construction behavior;
- string/char/UTF-16, culture, casing, comparison and interpolation;
- nullable/default behavior;
- exception type and timing.

### Gate 3 - binary/protocol/file oracle

Where applicable, compare C# and Native byte-for-byte for:

- struct size/alignment/packing/field order;
- serialization and deserialization;
- network packet encoding/decoding and round trips;
- ROM/file parser outputs;
- map/repack round trips;
- save/demo/audio/resource metadata formats.

### Gate 4 - initialization/lifetime/concurrency

Validate:

- static field/property initialization order and first-use timing;
- singleton/global reset behavior;
- task/thread/cancellation start, stop, failure and exception propagation;
- UDP/server/master/session receive/send lifetime;
- updater asynchronous/cancellation transitions;
- demo/movie/audio device/player/resource lifetime;
- Avalonia dispatcher/window ownership;
- shutdown/signal cleanup.

Do not replace C# lifetime with eager global construction or synchronous Native policy.

### Gate 5 - subsystem behavior

Use C# as the executable oracle for deterministic fixtures/traces covering applicable:

- engine/scene/entity/player state transitions;
- input and pointer/stylus/gamepad behavior;
- renderer/HUD calculations and image output where deterministic;
- audio pitch/state/resource transitions;
- network session/server/demo behavior;
- updater/install state machines;
- launcher preferences/game-file/match handoff behavior.

Use exact equality where C# behavior is exact. Use tolerance only where the specification/backend itself makes a measured floating/timing quantity non-exact; document the tolerance and why it exists.

### Gate 6 - platform/build matrix

Compile and run applicable differential paths for:

- desktop client with `MPHREAD_AVALONIA`;
- dedicated/headless server with `MPHREAD_SERVER`;
- network/protocol test path;
- Android shared-source build and GL/AL/ALC aliases;
- supported renderer/audio backends;
- OS-specific console/process/signal/filesystem branches.

A pass on one configuration does not imply another configuration passes.

### Gate 7 - terminal entry integration

Final end-to-end traces must prove:

- `Program.Main` remains the real process entry;
- console preparation ordering is preserved;
- `ModEntry.TryHandleHeadless(args)` precedes ordinary setup/argument dispatch;
- GUI launcher is attempted only on its C# build/path conditions;
- failed/unavailable GUI startup falls back to `TextLauncher.Run()` exactly where C# does;
- server and Android host boundaries do not redefine desktop policy;
- normal setup/parse/`TryHandle`/menu-read-render-export ordering matches C#;
- exit/exception/side-effect behavior matches.

## 11. Exact next two independent implementation leaves

The next two independent implementation units are:

1. **`src/MphRead/Mods/Chat/ChatFont.cs`** -> `src/MphRead.Native/Mods/Chat/ChatFont.hpp/.cpp`
2. **`src/MphRead/Mods/Input/StylusZone.cs`** -> `src/MphRead.Native/Mods/Input/StylusZone.hpp/.cpp`

They have no dependency edge between them, do not participate in the large partial-type/SCC ownership problems, and are small enough to receive complete deterministic C# parity tests independently. They can be implemented in parallel.

`src/MphRead/Shaders.cs` is the immediate comparable third leaf after those two.

## 12. Acceptance rule

No source file is considered complete because a Native file with the same name already exists, because it compiles alone, because an earlier review called it done, or because a downstream file can include it.

**Completion requires a fresh audit of the complete current C# source and passage of every applicable validation gate above.**

The strict structural end state remains:

```text
src/MphRead/<relative-path>/<Name>.cs
  ->
src/MphRead.Native/<relative-path>/<Name>.hpp
src/MphRead.Native/<relative-path>/<Name>.cpp
```

with colocated headers, canonical partial-type ownership, no duplicate partial definitions, no unmatched Native aggregation/policy files, and only thin behavior-preserving platform/runtime adapters.
