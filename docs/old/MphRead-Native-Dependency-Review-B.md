# MphRead Native Dependency Review B

## Status, scope, and authority

**Repository:** `Zection6V/Fruity-Prime`  
**Branch:** `develop2`  
**Source snapshot reviewed:** `a93424d264ba228d3526e089d3a23daf03b2b9fc`  
**C# authority:** `src/MphRead/**/*.cs`  
**Native target:** `src/MphRead.Native/**/*.hpp` + `src/MphRead.Native/**/*.cpp`

This is an independent adversarial dependency and ordering review. It does not modify C# or C++ source. The C# source is the sole behavioral specification; existing Native code is untrusted evidence and cannot justify custom behavior, alternate ownership, fallback policy, or different initialization/lifetime rules.

The strict target remains:

- one colocated Native `.hpp/.cpp` pair for every C# source file at the matching relative path;
- no generic include-directory counterpart headers;
- no C#-unmatched support source/header introduced merely to break cycles;
- no placeholder/no-op behavior counted as migrated;
- only the thinnest unavoidable platform/runtime adapters, with no application policy inside them;
- every existing Native pair re-audited against the complete live C# contract before it can be marked parity-complete.

At the reviewed snapshot the inventory is still **302 C# files**, **48 existing Native hpp/cpp pairs**, **47 pairs at the matching relative path**, **one relocated pair**, and **254 C# files without a Native pair**. The relocated pair is `Mods/Launcher/Portable/SetupProgress.cs`, whose existing Native files are under `Mods/Launcher/SetupProgress.*`; the final strict target is `src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp/.cpp`.

`develop2` moved during the investigation only by adding review Markdown. No `src/MphRead`, `src/MphRead.Native`, project, or build source changed, so the source-level findings below remain applicable to this snapshot.

---

## 1. Executive verdict

The current dependency maps are useful inventories but are not safe topological schedules. Three separate graph layers must be modeled:

1. **declaration ownership edges** — the C++ compiler needs a declaration or complete by-value layout;
2. **member/compile/link edges** — a body calls or accesses a member implemented by another source file;
3. **behavior-completion edges** — static initialization, thread/process lifetime, callbacks, partial-type state and platform behavior must already match C# before a consumer may rely on the dependency as `PARITY-PASS`.

Treating all three as one `using`-based graph produces false leaves and false wave boundaries. In particular, the middle of the program is not a clean `Formats -> Scene -> Entities -> Player -> Network -> Renderer` DAG. It is a set of semantic SCCs that must be broken in C++ with exact declarations, forward declarations and out-of-line definitions — never with placeholders or invented abstractions.

The two smallest truly independent next leaves remain:

1. `Mods/Chat/ChatFont.cs`
2. `Mods/Input/StylusZone.cs`

`Shaders.cs` is the next comparable independent unit after them.

---

## 2. Real entry chain and terminal integration order

The desktop/server process chain is behaviorally significant:

```text
Program.Main(string[] args)
  -> ConsoleSetup.Run()
  -> ConsoleWindow.Prepare(args)              [Windows]
  -> ModEntry.TryHandleHeadless(args)
  -> CheckSetup(args)
  -> ParseArguments(args)
  -> ModEntry.TryHandle(args)
  -> normal menu/read/render/export path
```

`ModEntry.TryHandleHeadless` executes before normal game-file setup validation. That is required for launcher, dedicated-server and other headless flows which must be able to start without passing through the ordinary setup path first.

Launcher dispatch is downstream of `ModEntry.TryHandleHeadless`:

```text
Program.Main
  -> ModEntry.TryHandleHeadless
       -> GuiLauncher.TryRun()                 [when the Avalonia path is enabled]
            -> true: launcher handled
            -> false: TextLauncher.Run()
       -> or TextLauncher.Run() directly
```

Therefore:

- `Program.cs` is the final integration unit, not a foundation;
- `Mods/ModEntry.cs` is immediately before it;
- `GuiLauncher.cs` and `TextLauncher.cs` are late integration units because they close over preferences, game-file setup, update, thumbnails, match start, network cleanup and game state;
- no Native `main`, `WinMain`, service wrapper or platform head may bypass or reorder the C# dispatch semantics.

---

## 3. Confirmed defects in the previous dependency/order model

### 3.1 `SoundRead` is a real partial-type SCC, not two independent format files

`Formats/FhSound.cs` and `Formats/Sound.cs` both contribute to `partial SoundRead`. `FhSound.cs` calls `ExportSamples`, whose private implementation is supplied by `Sound.cs`.

There is also a direct cross-subsystem cycle:

```text
Formats/Sound.cs -> Sfx.CalculatePitchDiv(...)
Sound/Sfx.cs     -> SoundRead.ReadSoundSamples()/ReadSoundTables()/...
```

Consequences:

- `FhSound.cs` cannot be parity-accepted in an earlier wave than the rest of `SoundRead`;
- `SoundRead` and `Sfx` form a semantic closure;
- declaration staging is allowed, but neither side is a completed prerequisite while the other side is still a placeholder or absent.

### 3.2 `Scene` has six known partial contributors, not five

The complete reviewed contributor set is:

```text
Scene.cs
Messaging.cs
Renderer.cs
Formats/Movie.cs
Mods/Render/PreviewCamera.cs
Mods/Render/PreviewPass.cs
```

`Formats/Movie.cs` explicitly contributes to `partial Scene` and owns movie state/lifetime behavior. Any plan that freezes `Scene` without it is incomplete.

`Scene.hpp` must be the single C++ class-declaration owner. The sibling counterpart headers may declare auxiliary C# types and may implement `Scene` members in their colocated `.cpp` files, but they must not create a second `Scene` class.

### 3.3 `Scene` and concrete entities are mutually dependent

`Scene.cs` names concrete entity types and has typed maps/iterators over them. Entity implementations retain/use `Scene`, call its rendering, node, timing and visibility services, and in turn are created/managed by scene setup.

This is a semantic SCC. C++ forward declarations and out-of-line definitions are the correct language-level seam; claiming `Scene.cs` behavior-complete before the entity family is not.

### 3.4 `PlayerEntity` is one 21-file class

The aggregate includes the twelve core player files plus chat/network/render partial contributors:

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

`Entities/Players/PlayerEntity.hpp` must own the single C++ `PlayerEntity` declaration. The existing Native `Mods/Render/PlayerEntityIconBounds.hpp` instead creates another `PlayerEntity` definition and adds Native-only concept/template overload machinery; that isolated shape cannot be retained as the integration architecture merely because it currently compiles by itself.

Aggregate `PlayerEntity` cannot be marked `PARITY-PASS` before the late render/UI contributors close.

### 3.5 `Metadata` is one seven-file static partial type

Contributors:

```text
Metadata/Enemies.cs
Metadata/FrontendMeta.cs
Metadata/Metadata.cs
Metadata/Player.cs
Metadata/Rooms.cs
Metadata/SoundMeta.cs
Metadata/Weapons.cs
```

Its declaration surface can be frozen before all bodies are complete, but its static initialization and table contents cannot be treated as complete until enemy/player/weapon dependencies are closed.

### 3.6 `Repack` and `RepackCollision` are partial-type ownership problems

`Utility/RepackEntity.cs`, `Utility/RepackModel.cs` and `Mods/MapGen/RepackAccess.cs` contribute to the same `Repack` type; `Utility/RepackCollision.cs` and `Mods/MapGen/RepackAccess.cs` contribute to `RepackCollision`.

A C#-unmatched generic aggregation header is not required and would violate the requested one-file structural contract. One existing counterpart header must own the canonical merged declaration for each partial type, while each source file keeps its own colocated counterpart pair for the members it owns.

### 3.7 `Read` is not an isolated format leaf

Confirmed reverse/member edges include:

- `Formats/Model.cs` uses results/contracts supplied through `Read`;
- `Read.cs` constructs and caches model/model-instance state;
- `Read.cs` emits through `DebugLog`;
- `Utility/Extract.cs`, `Utility/Analyzer.cs`, `Utility/Archive.cs`, `Utility/Parser.cs` and `Formats/NodeData.cs` call `Read` members directly.

`Read -> DebugLog` must not drag the full launcher subsystem into the format core. The exact member declaration needed by `Read` can exist early, but `DebugLog` behavior remains late because it also reaches launcher preferences, branding/version, network/render diagnostics and OS/runtime information.

### 3.8 `Formats/AiPersonality.cs` is gameplay/player code for ordering purposes

It reaches player AI/player/game-state contracts and is consumed by the player AI implementation. Its earlier placement in a generic format wave is a false dependency direction. It belongs in the player/gameplay closure.

### 3.9 Network “foundation” files are more entangled than the namespace index shows

Confirmed edges include:

- `DemoPlayback.cs`, `DemoRecorder.cs`, `DemoClip.cs` and the replay path in `DemoInfo.cs` -> `NetSession`;
- `MapVote.cs` -> `NetSession` and `EndScreen.Hit`/pointer state;
- `DedicatedServer.cs` -> `ServerSim` and `GameState`;
- `NetMaster.cs` owns master-reporter behavior and creates/controls dedicated servers;
- `DedicatedServer <-> NetMaster` is therefore a real operational SCC;
- `NetStatus.cs` reaches metadata to turn room keys into display names;
- `NetProtocol.cs` uses `PlayerEntity.SlotCapacity` for roster sizing.

The wire-format declarations may be staged early, but the higher-level session/server/demo behavior cannot be completed before the engine/player/game-state contracts it actually executes against.

### 3.10 Several apparent “utility” files are not utilities in the topological sense

Confirmed examples:

- `Memory.cs` owns `Scene?` state and consumes AI memory classes; `MemoryClasses.cs` retains `Memory` and entity/math contracts;
- `Mods/HunterSuits.cs` calls `Metadata`, `Read`, `Model/Recolor` and `PlayerColors`;
- `Mods/WindowMode.cs` directly reads `PauseMenu.Open`;
- `Mods/ThumbnailHost.cs` calls `ThumbnailBatch` and `ThumbnailGenerator`;
- `Mods/ThumbnailLog.cs` reaches launcher `GameFiles`, branding and build version;
- `Mods/LogShare.cs` uses `LauncherPrefs.Directory`;
- `Mods/Input/GamepadDesktop.cs` consumes `GamepadInput` and mappings;
- `Mods/Input/GamepadMappings.cs` consumes `LauncherPrefs`;
- `Mods/Input/GamepadProbe.cs` consumes `InputSettings`, `PadBindings`, `GamepadDesktop` and `GamepadInput`;
- `Mods/Input/PointerInput.cs` emits through `DebugLog`.

An empty or small `using` list does not make these leaves.

### 3.11 Existing Native partial stand-ins must not become architecture

The existing `Mods/Render/PreviewCamera.hpp` defines a truncated `Scene` and a local OpenTK-like `Vector3` declaration surface. This is a migration scaffold, not C# authority. It must ultimately bind to the canonical `Scene` and the exact math adapter contract instead of becoming a second scene/math implementation.

The same rule applies to every existing pair: existence means `UNREVIEWED`, not “dependency satisfied”.

---

## 4. Same-namespace, layout, overload and generic-constraint rules

The generated dependency index misses same-namespace references. `Mods/Input/PadBindings.cs -> GamepadButtons` from `GamepadState.cs` is a simple example; the root `MphRead` namespace contains many more consequential cases.

Before calling a file dependency-closed, inspect:

- base classes/interfaces;
- by-value fields and nested value types;
- same/enclosing-namespace type/member references;
- overload resolution, default arguments and optional values;
- C# enum underlying types, `[Flags]`, unknown values and formatting behavior;
- `ref`/`out` semantics and mutable versus readonly struct behavior;
- generic constraints (`struct`, `unmanaged`, class inheritance constraints and enum/type constraints);
- partial-class private-member access;
- reflection targets and non-public construction;
- static field/property initialization and first-use timing;
- `async`/`Task`, cancellation and exception propagation;
- platform/build-condition branches.

C++ may use templates or overloads to express the C# mechanism, but must not broaden the public contract into new callable behavior.

By-value binary layout is a hard declaration edge. Files such as `Formats/EntityEnemy.cs` embed entity headers, messages, collision volumes, fixed-point vectors and enums by value; they therefore require those exact declaration/layout owners, not merely forward declarations.

---

## 5. Static initialization and lifetime gates

C# static initialization order must not be replaced with arbitrary C++ translation-unit initialization. High-risk groups include:

- `Metadata` tables and dictionaries;
- player/static gameplay state;
- network singleton/session/server state;
- sound/music singleton state;
- launcher preferences/update state;
- partial-type static members split across source files.

Where C# relies on first-use type initialization, use a C++ mechanism with equivalent observable timing (for example, function-local statics or an explicit exact initialization call already present in C#). Do not add an eager Native bootstrap simply to make construction convenient.

Thread/process ownership is likewise behavioral. UDP receive loops, updater tasks, demo recording/playback, renderer/movie cancellation, Avalonia dispatcher ownership, SFX/music devices and dedicated/master servers must preserve start/stop/failure timing, not merely compile.

---

## 6. Smallest allowed platform/runtime seams

The platform seams are mechanical boundaries only. The smallest exact seams identified by the review are:

- **OpenTK mathematics:** vector/matrix/quaternion operations and exact numeric semantics;
- **GL/GLES:** desktop GL versus Android `GlEs`, preserving the shared C# call contract;
- **OpenAL/ALC:** desktop OpenAL versus Android `AlEs`/`AlcEs`/`SfxMixer`;
- **music/audio backend:** NCSF/SoundFlow/MiniAudio device/player operations required by C#;
- **Avalonia:** application setup, dispatcher/event-loop/window mechanics, with launcher policy staying in translated C# counterparts;
- **network runtime:** UDP sockets, DNS, endpoints, timeouts and datagram behavior;
- **HTTP runtime:** request/download primitives only; updater policy remains in C# counterparts;
- **filesystem/process/console:** path, environment, executable name, file locking/sharing, child process, console/fd and signal mechanics;
- **image/graphics decoding:** only the codec/resource mechanics actually called by C#;
- **OS signal integration:** Ctrl+C/POSIX registration mechanics used by `ShutdownSignals`.

Adapters must not add retries, normalization, fallback policy, caching, validation, ownership, logging or error swallowing unless the C# code itself specifies it.

---

## 7. SCC model and completion barriers

### 7.1 Engine core macro-SCC

The authoring bands corresponding to the old W4-W8 region must not be interpreted as a topological chain of completed modules. Together they contain the mutually dependent closure of:

```text
Read / model / formats
Scene / Messaging / Renderer / Movie
EntityBase / concrete entities / enemies
PlayerEntity partials / GameState / SceneSetup / Menu
Metadata player/enemy/weapon tables
gameplay network / NetSession / ServerSim
SoundRead / Sfx / Music
renderer/image/preview consumers
```

Forward declarations and a frozen complete partial-type declaration surface allow C++ translation units to be authored in an order. They do **not** make earlier bands behavior-complete prerequisites.

The macro-SCC gate requires:

- one canonical definition for every partial C# type;
- all cross-partial private members represented once;
- no duplicate stand-in Scene/PlayerEntity/Metadata types;
- full link with no placeholder bodies;
- static initialization parity;
- network/binary byte-oracle tests where applicable;
- audio/resource lifetime tests where applicable;
- engine/player/scene deterministic behavior tests.

### 7.2 Launcher/UI macro-SCC

The late launcher/UI region also contains reverse edges:

```text
LauncherPrefs -> WindowMode.Parse
WindowMode    -> PauseMenu.Open
DebugLog      -> LauncherPrefs and runtime diagnostics
GamepadMappings -> LauncherPrefs
LogShare      -> LauncherPrefs
ThumbnailLog  -> GameFiles/build version
GuiLauncher/TextLauncher -> all of the above + match/network/update
MapVote       -> EndScreen layout/pointer contract
```

Declaration/member staging is allowed, but do not claim the portable launcher/UI group behavior-complete until these reverse edges and the Avalonia/pause/window lifecycle close.

### 7.3 Partial aggregate acceptance

Earliest aggregate `PARITY-PASS` points are later than the first source-file body:

- `SoundRead`: after both `FhSound.cs` and `Sound.cs` plus the Sfx/audio closure;
- `Metadata`: after enemy/player/weapon contributors and their data dependencies;
- `Scene`: after all six contributors, including `PreviewPass.cs`;
- `PlayerEntity`: after all 21 contributors, including end-screen/pro-HUD/render tails;
- `Repack`/`RepackCollision`: after all contributing utility/mapgen files.

---

## 8. Corrected file-level assignment

The exact 302-path assignment in `docs/MphRead-Native-Dependency-Review-A.md` is incorporated as the coverage baseline. Review B changes only the earliest safe authoring wave for the 28 files below. Every unlisted path retains its Review-A assignment. Applying each relocation exactly once preserves complete one-file coverage with no duplicate or unassigned C# source.

| File | Review A | Review B | Reason |
|---|---:|---:|---|
| `Memory.cs` | W2 | W9 | `Scene`/AI memory-class dependency; pair with `MemoryClasses`. |
| `Formats/EntityEnemy.cs` | W2 | W4 | by-value entity/message/collision/fixed layout prerequisites. |
| `Formats/FhSound.cs` | W2 | W5 | same `SoundRead` partial; calls private member supplied by `Sound.cs`. |
| `Formats/Model.cs` | W2 | W4 | direct `Read`/model construction closure. |
| `Formats/NodeData.cs` | W2 | W4 | direct `Read` calls. |
| `Mods/HunterSuits.cs` | W2 | W7 | `Metadata`, `Read`, `Model/Recolor`, `PlayerColors`. |
| `Mods/LogShare.cs` | W2 | W11 | `LauncherPrefs.Directory` and branding. |
| `Mods/ThumbnailHost.cs` | W2 | W8 | `ThumbnailBatch`/`ThumbnailGenerator`. |
| `Mods/ThumbnailLog.cs` | W2 | W11 | `GameFiles`, branding/build-version launcher state. |
| `Mods/WindowMode.cs` | W2 | W12 | direct `PauseMenu.Open` behavior; earlier enum/parse declarations may be exposed mechanically. |
| `Mods/Input/GamepadDesktop.cs` | W2 | W11 | `GamepadInput` plus mappings/launcher-backed mapping path. |
| `Mods/Input/GamepadMappings.cs` | W2 | W11 | `LauncherPrefs.Directory`. |
| `Mods/Input/GamepadProbe.cs` | W2 | W11 | `InputSettings`, `PadBindings`, `GamepadDesktop`, `GamepadInput`. |
| `Mods/Input/PointerInput.cs` | W2 | W11 | direct `DebugLog` emission. |
| `Mods/Network/NetProbe.cs` | W2 | W5 | wire `PacketType`/`NetConfig` prerequisite. |
| `Utility/Analyzer.cs` | W2 | W4 | direct `Read` and effect-layout access. |
| `Utility/Archive.cs` | W2 | W4 | direct `Read` generic struct/offset operations. |
| `Utility/Parser.cs` | W2 | W4 | direct `Read.ReadStruct<T>`. |
| `Utility/Extract.cs` | W3 | W4 | extensive `Read` calls; only exact `Program.Version` declaration is needed early. |
| `Formats/AiPersonality.cs` | W4 | W7 | player AI / `PlayerEntity` / `GameState` closure. |
| `Mods/Network/DedicatedServer.cs` | W5 | W7 | `ServerSim`/`GameState`; SCC with master-server control. |
| `Mods/Network/DemoClip.cs` | W5 | W7 | direct `NetSession` state/frame dependency. |
| `Mods/Network/DemoInfo.cs` | W5 | W7 | replay drives `DemoPlayback` + `NetSession`. |
| `Mods/Network/DemoPlayback.cs` | W5 | W7 | direct `NetSession` session/pump behavior. |
| `Mods/Network/DemoRecorder.cs` | W5 | W7 | direct session/network state behavior. |
| `Mods/Network/MapVote.cs` | W5 | W11 | `NetSession` plus `EndScreen.Hit`/pointer layout contract. |
| `Mods/Network/NetMaster.cs` | W5 | W7 | operational SCC with `DedicatedServer`; server/game-state closure. |
| `Mods/Network/NetStatus.cs` | W5 | W7 | room metadata plus wire/network behavior. |

Corrected counts after those 28 relocations:

| Wave | Files |
|---:|---:|
| W1 | 18 |
| W2 | 18 |
| W3 | 9 |
| W4 | 22 |
| W5 | 11 |
| W6 | 66 |
| W7 | 67 |
| W8 | 14 |
| W9 | 20 |
| W10 | 6 |
| W11 | 14 |
| W12 | 26 |
| W13 | 9 |
| W14 | 1 |
| W15 | 1 |
| **Total** | **302** |

These are authoring waves, not blanket completion barriers. The SCC rules in Section 7 override any interpretation that W4 can be globally accepted before W5-W8, or that the launcher W11 region can be globally accepted before W12 reverse edges close.

---

## 9. Wave gates and what must not be attempted early

### W1 — atomic deterministic leaves

Start with `ChatFont.cs` and `StylusZone.cs` independently; `Shaders.cs` follows. Re-audit existing small pairs rather than grandfathering them.

**Gate:** exact constants, arrays, struct/enum semantics, static defaults and edge cases have deterministic parity oracles.

### W2-W3 — small runtime/data contracts only

Only files that do not execute against later engine/launcher state may complete here.

**Do not** treat `Memory`, WindowMode, launcher-backed input mappings, `FhSound`, `Model`, `NodeData`, Read-consuming utilities or HunterSuits as completed foundations.

### W4-W8 — engine core macro-SCC

Use exact declarations/forward declarations to make translation units compile, but do not expose earlier subwaves as `PARITY-PASS` modules to later work until the relevant SCC gate closes.

**Do not:** invent duplicate `Scene`/`PlayerEntity`; add no-op entity/network/audio methods; independently freeze static metadata; or use a Native stand-in type as specification.

### W9 — live-memory/map/repack closure

`Memory.cs` belongs with `MemoryClasses.cs` here. Repack partial declarations must already be canonical and one-definition-safe.

**Gate:** map/repack binary round-trip and memory-wrapper semantics match C# exactly.

### W10 — updater

HTTP/process/install adapters are mechanical. All state transitions, retries actually present in C#, cancellation and failure timing stay in translated updater code.

### W11-W12 — portable launcher, logging, window/pause/GUI closure

These bands form a late macro-SCC because of LauncherPrefs/WindowMode/PauseMenu/DebugLog/gamepad/log/thumb dependencies.

**Gate:** launcher loops, preference serialization, update display, game-file setup, match handoff, window/pause behavior and network cleanup match C# on the applicable build configurations.

### W13 — test/oracle consumers

Tests are translated after production contracts; they cannot be used to introduce production-only helpers or policy.

### W14 — `Mods/ModEntry.cs`

Do not attempt final acceptance before launcher/update/network/map/render/server command dependencies close.

**Gate:** `TryHandleHeadless` and `TryHandle` argument/exit/side-effect traces match C# across desktop/server configurations.

### W15 — `Program.cs`

Final unit only.

**Gate:** end-to-end startup trace, culture/argument behavior, setup ordering, export/menu/render routing, exception/exit behavior and launcher dispatch match C#.

---

## 10. Validation gates

A file may be marked `PARITY-PASS` only after its relevant gates are satisfied.

### Structural gate

- matching colocated `.hpp/.cpp` path;
- no C#-unmatched policy/support source;
- correct namespace/type visibility and one-definition ownership;
- partial declarations merged into one canonical C++ class/static-type declaration.

### Language-semantic gate

- enum underlying types/flags/unknown values;
- signed/unsigned overflow and conversion;
- default construction/value semantics/copy behavior;
- overload/default-argument resolution;
- exact string/char/UTF-16 behavior;
- nullable/default/reference semantics;
- generic constraint behavior;
- exception timing/type where observable.

### Binary/protocol gate

- struct sizes/alignment/packing and field order;
- serialization byte-for-byte tests;
- protocol packet sizes/constants/read-write round trips;
- ROM/file parsing and repack round trips.

### Initialization/lifetime gate

- static initialization timing;
- singleton/reset state;
- task/thread/cancellation ownership;
- socket/process/device/window/resource start-stop order;
- failure cleanup.

### Platform gate

Validate each applicable configuration rather than assuming desktop parity implies server/Android parity:

- desktop client;
- dedicated/headless server;
- network/protocol test paths;
- Android shared GL/AL/ALC substitution contracts;
- renderer/HUD/image visual paths;
- SFX/music/movie audio/resource paths;
- full `Program -> ModEntry -> launcher/normal path` integration.

---

## 11. Blockers and prohibited shortcuts

The following must not be used to move a file earlier or mark it complete:

- an existing Native pair with no complete parity audit;
- an include compiling against a fake/minimal duplicate type;
- dummy/default-return/no-op behavior;
- `void*`/integer handles replacing typed C# ownership merely to break a header cycle;
- a new unmatched aggregation header standing in for C# partial types;
- broad Native-only templates/overloads not callable in C#;
- eager global initialization where C# is first-use initialized;
- synchronizing async work that is asynchronous in C#;
- swallowing exceptions/failures C# exposes;
- adding retry/fallback/normalization policy absent from C#;
- moving the process entry directly to launcher/server code and bypassing `Program.Main`/`ModEntry` order.

Existing Native code may be reused only after its behavior is independently shown to reproduce the current C# source.

---

## 12. Next two independent leaves

### `Mods/Chat/ChatFont.cs`

Independent closure:

- no MphRead type dependency;
- constants/arrays and static glyph initialization are local;
- no filesystem, renderer, network, process, task or platform ownership.

Validation must cover exact static initialization, arrays/constants, `Index`, `Measure`, character/encoding behavior, unsupported-character behavior and all boundaries.

### `Mods/Input/StylusZone.cs`

Independent closure:

- owns its enum, readonly nested value type, arrays/state and methods locally;
- no engine, launcher, network, renderer or external framework dependency.

Validation must cover enum values/order, static defaults, float constants, NaN/clamp/boundary behavior, region inclusivity and placement/contact transitions.

These files have no edge between them and are safe parallel work items. `Shaders.cs` is the immediate third leaf.

---

## Final verdict

The 302-file migration is schedulable, but not as a naïve namespace or `using` topological sort. The exact safe model is:

- keep one-file counterpart ownership and exact paths;
- distinguish declarations from member/link dependencies and from behavior completion;
- treat the Scene/entity/player/GameState/gameplay-network/audio/render middle as an SCC-controlled macro region;
- treat late launcher/window/pause/logging/input preferences as another reverse-edge closure;
- use only thin mechanical platform adapters;
- preserve the real `Program.Main -> ModEntry.TryHandleHeadless -> GuiLauncher.TryRun/TextLauncher.Run` chain;
- do not promote an existing Native pair to authority;
- keep `ModEntry` penultimate and `Program` final;
- begin parallel implementation with `ChatFont.cs` and `StylusZone.cs`.

Review B preserves exact 302-file coverage by taking the complete Review-A assignment and applying the 28 explicit relocations in Section 8; the corrected wave counts still sum to 302 exactly.