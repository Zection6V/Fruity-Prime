# MphRead Native Implementation Order

## 1. Purpose

This document defines the implementation order for the strict C# to C++20 migration of:

- source: `src/MphRead/**/*.cs`
- target: `src/MphRead.Native/**/*.hpp` + `src/MphRead.Native/**/*.cpp`

The C# source is the sole behavioral specification.

The migration target is one C++ counterpart pair for every C# source file. Existing Native code is evidence only; it is not authoritative merely because a similarly named file already exists.

The final target is:

- 302 C# source files accounted for.
- 302 corresponding Native `hpp/cpp` pairs.
- Each pair colocated in the Native tree at the corresponding relative source location.
- No empty placeholder implementations.
- No Native-only gameplay, protocol, serialization, UI, update, rendering, timing, or error-handling policy.
- Only language-level translation machinery and the thinnest unavoidable platform/framework adapters.
- Every completed pair audited against the complete C# file, not against an earlier C++ implementation or a partial diff.

This plan is based on `develop2` at:

```text
9e9eb0e7267534faac044f0bdd36d17e00c97735
```

The dependency documents embedded in that commit say they were generated against its parent:

```text
2da573aa860507eab7150dfd3a69667528721063
```

The only change from `2da573aa` to `9e9eb0e` is the addition of the four dependency documents. There are no `src/MphRead` or `src/MphRead.Native` changes between those commits, so the dependency inventory describes the requested source snapshot.

The repository URL supplied for this review contains `/tree/codex/develop2`, but the repository branch API does not expose a branch named `codex/develop2`. The separately specified and verified `develop2@9e9eb0e` is therefore the snapshot used by this plan.

---

## 2. Current inventory

At this snapshot:

| Item | Count |
|---|---:|
| C# files under `src/MphRead` | 302 |
| Existing Native `hpp/cpp` pairs | 48 |
| Existing pairs at the matching relative path | 47 |
| Existing relocated pairs | 1 |
| C# files without a Native pair | 254 |

The one relocation is:

```text
C#:     Mods/Launcher/Portable/SetupProgress.cs
Native: Mods/Launcher/SetupProgress.hpp
        Mods/Launcher/SetupProgress.cpp
```

Because the migration target is one-for-one structural correspondence, this should be normalized in Wave 1 to:

```text
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp
src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.cpp
```

Temporary forwarding includes are acceptable only during migration if needed to avoid breaking concurrent branches. They are not the desired final structure.

---

## 3. Fundamental migration rules

### 3.1 C# remains the specification

For every pair:

1. Read the entire C# file at the pinned source commit.
2. Read the complete current Native `hpp/cpp` pair if it exists.
3. Inspect every directly required C# contract.
4. Establish exact type, value, initialization, lifetime, exception, conversion, formatting, threading, async, and side-effect semantics.
5. Correct or replace Native behavior where it differs.
6. Validate the resulting pair independently.
7. Only then mark the file complete.

A prior `PASS` is useful audit evidence, not permanent authority. A pair must continue to satisfy the current C# source and its current dependency contracts.

### 3.2 A file name is not a contract

Do not infer parity from:

- matching class names;
- matching method names;
- matching apparent constants;
- a Native include whose name resembles a C# `using`;
- a previous migration attempt;
- successful compilation alone.

The source-level behavior must be checked.

### 3.3 Do not use placeholder behavior to break dependency cycles

Allowed:

- C++ forward declarations;
- declaration-only contract passes;
- splitting declarations from definitions;
- moving full includes from headers to `.cpp` files;
- a behaviorless language-level aggregation header if unavoidable for C# `partial` types;
- test-only seams which do not change production behavior.

Not allowed:

- dummy implementations returning arbitrary defaults;
- fake framework objects with invented state;
- temporary no-op methods treated as migrated;
- synthetic game/network/render state not present in C#;
- changing visibility just to make a port easier;
- changing asynchronous operations into synchronous operations unless C# does so;
- swallowing failures that C# exposes;
- adding fallback behavior absent from C#.

### 3.4 Completion wave versus contract pass

The wave assigned to a C# file in Appendix A is its **completion wave**.

Some C# types must have their declaration contracts transcribed before the implementation bodies of all contributing files can be completed. A declaration-only pass does not make a file complete.

This distinction is necessary because C++ cannot reopen classes the way C# `partial` classes can.

### 3.5 Header dependency rule

Prefer:

```text
forward declaration in hpp
full include in cpp
```

when C++ completeness rules permit it.

A full definition must be included where the C# contract requires by-value layout, inheritance, inline/template behavior, or another operation that genuinely requires a complete type.

Header cycles must never be "solved" by replacing typed C# contracts with `void*`, integer handles, duplicated types, or altered ownership semantics.

---

## 4. Partial-type and strongly connected component handling

The direct-`using` dependency indices are useful but are not a complete C# dependency graph. They do not capture every same-namespace reference, enclosing-namespace lookup, member reference, static access, partial declaration, inheritance edge, or reflection dependency.

Several important cycles are visible directly in the source.

### 4.1 `Scene`

`Scene` is a C# partial type with contributions at least from:

```text
Scene.cs
Messaging.cs
Renderer.cs
Mods/Render/PreviewCamera.cs
Mods/Render/PreviewPass.cs
```

`Scene.hpp` should be the canonical C++ class declaration owner.

All members from all five C# partial declarations must be inventoried before that canonical class declaration is considered frozen.

The counterpart headers for the other partial files may include `Scene.hpp` and own auxiliary types declared by their C# files, while their `.cpp` files implement the `Scene` members belonging to that source file.

Do not define another C++ `class Scene` in those headers.

The declaration contract is frozen during the Wave 4 contract pass; implementation bodies finish in Waves 4, 8, and 12 as assigned.

### 4.2 `PlayerEntity`

`PlayerEntity` spans the core player files and later mod files. The complete contract includes the contributions from:

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

`Entities/Players/PlayerEntity.hpp` should own the one C++ `PlayerEntity` class definition.

The existing `Mods/Render/PlayerEntityIconBounds.hpp` currently declares its own standalone `PlayerEntity` class. That shape cannot survive integration with the real `PlayerEntity` port. It must be converted to the canonical partial-type strategy rather than preserved because it currently compiles in isolation.

The complete 21-file member contract must be frozen during the Wave 7 contract pass. Bodies belonging to later render/UI extensions can complete in Waves 8 and 12.

### 4.3 `Metadata`

The metadata files contribute to a shared C# partial static class:

```text
Metadata/Enemies.cs
Metadata/FrontendMeta.cs
Metadata/Metadata.cs
Metadata/Player.cs
Metadata/Rooms.cs
Metadata/SoundMeta.cs
Metadata/Weapons.cs
```

`Metadata/Metadata.hpp` should own the canonical C++ `Metadata` declaration.

Its whole declaration surface must be known by Wave 3 even though behavior depending on enemies and players completes in Waves 6 and 7.

### 4.4 `Repack` and `RepackCollision`

The source also uses partial static utility types across:

```text
Utility/RepackEntity.cs
Utility/RepackModel.cs
Mods/MapGen/RepackAccess.cs
```

for `Repack`, and:

```text
Utility/RepackCollision.cs
Mods/MapGen/RepackAccess.cs
```

for `RepackCollision`.

These files all complete in Wave 9, so the safest approach is to freeze their complete class declarations at the beginning of Wave 9 and then compile the individual implementation files against the single declaration.

If the project permits a declaration-only support header, a `Utility/Repack.Partials.hpp` style aggregation header is the least ambiguous choice. If one-for-one policy prohibits additional support headers, designate `Utility/RepackEntity.hpp` as the canonical `Repack` declaration owner and `Utility/RepackCollision.hpp` as the canonical `RepackCollision` owner. In either case there must be exactly one C++ class definition for each C# partial type.

### 4.5 Cyclic implementation groups

Three major SCC checkpoints are used:

**Core contract SCC:** Waves 4 through 7.

This contains the difficult bidirectional relationships among:

```text
Formats
Scene
EntityBase/concrete entities
Sound
Network
PlayerEntity
GameState/gameplay
```

Individual translation units must compile earlier, but full project-symbol link closure is required at the Wave 7 checkpoint.

**Render/partial tail SCC:** Waves 8 through 12.

This closes later `Scene` and `PlayerEntity` partial bodies, rendering, thumbnails, EndScreen, launcher, and GUI relationships.

**Entry SCC:** Waves 14 through 15.

`ModEntry` is accepted first; `Program` is accepted last.

---

## 5. Platform and framework boundaries

The C# projects establish multiple distinct environments. A single "desktop build passed" check is insufficient.

### 5.1 Desktop client

Relevant C# dependencies include:

- .NET 9 runtime semantics;
- OpenTK 4.9.4;
- Avalonia 11.3.11;
- ReFuel.StbImage 2.1.1;
- OpenAL Soft runtime in game builds;
- NcsfPlay;
- SoundFlow through the audio stack.

Normal game builds define the logical equivalent of `MPHREAD_AVALONIA`.

### 5.2 Dedicated server

The dedicated-server configuration:

- defines `MPHREAD_SERVER`;
- excludes `Mods/Launcher/Gui/**`;
- does not require the game launcher;
- does not depend on game assets merely to run the headless server path;
- has different audio/UI assumptions;
- must preserve command-line, console, exit-code, shutdown, update, socket, and cancellation behavior.

### 5.3 Android/shared-source contract

`src/MphRead.Android` recompiles the same `src/MphRead/**/*.cs` source set with Android conditions and aliases:

```text
GL  -> MphRead.Mods.Render.GlEs
AL  -> MphRead.Mods.Sound.AlEs
ALC -> MphRead.Mods.Sound.AlcEs
```

The Native plan must therefore preserve the semantics required by the Android-facing adapters even if Native Android executable packaging is implemented separately.

`GlEs` and `AlEs` are not optional approximations of desktop behavior; they are explicit C# platform substitutions and must be audited against what their callers observe.

### 5.4 External adapter rule

An adapter may translate an external contract such as:

```text
OpenTK mathematics
OpenGL/OpenGL ES
GLFW keyboard/mouse/gamepad
OpenAL/SoundFlow
Avalonia
filesystem
process creation
HTTP
sockets
threads/tasks/cancellation
timers/Stopwatch
CultureInfo
NCSF/NcsfPlay
STB image operations
```

It must not introduce application policy.

For every adapter, preserve observable C# behavior including:

- value widths and signedness;
- default values;
- nullability;
- ordering;
- exception type and timing where observable;
- cancellation timing;
- thread ownership;
- blocking versus non-blocking behavior;
- process exit status;
- path normalization;
- environment variable treatment;
- string encoding;
- current/invariant culture;
- floating-point constants and conversions;
- collection iteration order where the program observes it;
- socket framing and packet bytes.

---

## 6. Existing Native baseline

`paired` means only that an `hpp/cpp` pair exists. It does **not** mean parity.

### 6.1 Existing pairs with a recorded PASS result

These still re-enter the validation gate for their assigned wave:

```text
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
```

`Formats/Frontend.cs` appears in the old audit ledger under the stale path label `Mods/Launcher/Portable/Frontend.cs`; the actual tree path is authoritative.

### 6.2 Existing pairs with a recorded FAIL / NO-OP result

These must not be treated as completed:

```text
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
```

### 6.3 Existing pairs without a PASS/FAIL result in the current audit ledger

These remain suspect and require complete audits:

```text
Entities/Enemies/43_SlenchNest.cs
Entities/Enemies/51_CarnivorousPlant.cs
Entities/LightSourceEntity.cs
Entities/PointModuleEntity.cs
Formats/Culling.cs
Mods/Launcher/Gui/CrosshairPreview.cs
Mods/Launcher/Gui/HomeWindow.cs
Mods/Launcher/Gui/ProgressRow.cs
Mods/Launcher/Gui/TrackedText.cs
Mods/Launcher/Portable/LaunchPlan.cs
Mods/Launcher/Portable/SetupProgress.cs
Mods/Network/NetConnectCommand.cs
Mods/Network/NetLag.cs
Mods/Network/NetMatchSync.cs
Mods/Network/NetPlayerSetup.cs
Mods/Network/NetProbe.cs
Mods/Network/NetScoreboard.cs
Mods/Render/Crosshair.cs
Mods/Render/PlayerEntityIconBounds.cs
Mods/ShutdownSignals.cs
Mods/ThumbnailHost.cs
Mods/ThumbnailLog.cs
Mods/Update/BuildVersion.cs
Mods/Update/SyncHttp.cs
Mods/WorldEvents.cs
Utility/Rng.cs
```

### 6.4 Already audited missing targets

The previous ledger also records dependency-blocked or failed audits for files that do not yet have a pair. Canonical source paths are:

```text
Formats/FhSound.cs
Mods/Input/GamepadLayout.cs
Mods/Launcher/Gui/GuiLauncher.cs
Mods/Launcher/Portable/AdventureSave.cs
Mods/Launcher/Portable/TextLauncher.cs
Mods/LogShare.cs
Mods/MapGen/MapReport.cs
Mods/MapGen/RawStructs.cs
Mods/Network/DemoClip.cs
Mods/Network/NetHostSession.cs
Mods/Render/EsBindings.cs
Mods/RespawnChoice.cs
Mods/Update/UpdateDownload.cs
Mods/Update/UpdateInstall.cs
Mods/Update/Updater.cs
```

Several old ledger labels pointed at directories that do not contain the actual C# file. Future audit records must use the canonical tree path.

---

## 7. Best next two independent leaves

The best immediate parallel pair is:

### Worker A: `Shaders.cs`

Reasons:

- currently missing in Native;
- no MphRead dependency;
- no framework object ownership;
- no threading;
- no platform condition;
- essentially an exact static-string contract;
- independent of Worker B.

Validation:

- exact number of shader properties;
- exact property initialization semantics;
- exact string contents;
- exact line breaks and embedded characters;
- exact byte stream sent to GL after the same encoding used by the Native GL boundary;
- fixed hashes for every shader string against the C# reference.

### Worker B: `Mods/Chat/ChatFont.cs`

Reasons:

- currently missing in Native;
- no internal MphRead dependency;
- deterministic static initialization;
- no window/network dependency despite living under Chat;
- independent of `Shaders.cs`.

Validation:

- constants `Cell`, `First`, `Last`, and derived count;
- complete `Pixels` array after static initialization;
- complete `Widths` array;
- every glyph definition;
- `Index` lower/upper/out-of-range behavior;
- `Measure` behavior for empty, printable, repeated, unsupported, and mixed-character inputs;
- byte-for-byte/hash equality of the initialized arrays with C#.

`Features.cs` is **not** one of the next leaves even though its dependency-index row appears light: its `Load` and `Commit` directly use `Mods.Render.Crosshair`. It belongs after Crosshair is accepted.

---

## 8. Wave overview

The file counts below sum exactly to 302.

| Wave | Completion scope | Files | Required closure |
|---:|---|---:|---|
| W0 | Snapshot, ABI rules, partial-type contracts, build/test matrix | 0 | repository snapshot |
| W1 | Independent leaves and simple existing pairs | 17 | W0 |
| W2 | Runtime/value/platform primitives | 37 | W0, W1 contracts |
| W3 | Static catalogs, text, standalone serializers | 10 | W1-W2 |
| W4 | Core Formats/Scene/Entity contract SCC | 16 | W1-W3 + SCC declaration pass |
| W5 | Audio and foundational network | 17 | W2-W4 contracts |
| W6 | Non-player entities, CamSeq, enemies | 66 | W3-W5 contracts |
| W7 | Player/gameplay/network SCC | 58 | W1-W6; closes core SCC |
| W8 | Renderer/export/thumbnails/render partials | 13 | W1-W7 |
| W9 | MapGen/repack/memory tooling | 19 | W3-W8 |
| W10 | Update subsystem | 6 | W1 BuildVersion, W2 SyncHttp/platform adapters |
| W11 | Portable launcher, logging, EndScreen | 7 | W7-W10 |
| W12 | Avalonia GUI, pause UI, final Scene/Player render partials | 25 | W8, W10-W11; closes render/partial SCC |
| W13 | Remaining C# test/tool entry files | 9 | W1-W12 |
| W14 | `ModEntry.cs` | 1 | W1-W13 production dependencies |
| W15 | `Program.cs` | 1 | W14 + all process-entry dependencies |

The numbered order is the conservative merge order. Work from a later wave may run in parallel once **its stated prerequisites**, rather than every numerically earlier unrelated wave, are accepted. For example, the Update implementation can proceed once the W1/W2 runtime and HTTP/process contracts it requires are fixed; it does not semantically depend on completing all enemies.

---

# 9. Detailed implementation waves

## W0 - Freeze the migration contract

No C# file is marked complete in W0.

### Required work

Freeze:

- `develop2@9e9eb0e7267534faac044f0bdd36d17e00c97735`;
- the 302-file C# inventory;
- the 48-pair Native inventory;
- the one-for-one source-to-target path map;
- compiler-visible integer widths;
- enum representation rules;
- C# `struct` value/default/copy semantics;
- C# reference/null semantics;
- string/encoding policy;
- exception mapping;
- collection behavior policy;
- async/task/cancellation policy;
- OpenTK mathematics adapter behavior;
- GL/ES boundary;
- AL/ALC boundary;
- Avalonia boundary;
- process/filesystem/HTTP/socket boundary;
- build configurations.

Create a partial-type ownership manifest for:

```text
Scene
PlayerEntity
Metadata
Repack
RepackCollision
```

Also expose declaration-only contracts when later files own a type needed by earlier waves.

A concrete example is `ProgramException`: it is declared in `Program.cs`, but `SyntheticInput.cs`, `RepackModel.cs`, and other earlier files throw it. `Program.hpp` may expose the exact `ProgramException` type before `Program.Main` itself is completed. That is a declaration dependency, not permission to implement `Program.Main` early.

### W0 validation gate

W0 passes only when:

- C# inventory = 302;
- every path is unique;
- existing Native pair inventory = 48;
- every existing Native pair is mapped to exactly one C# file;
- `SetupProgress` relocation has an explicit normalization decision;
- all known partial types have one canonical C++ declaration owner;
- desktop client, dedicated server, and Android-facing adapter configurations are defined;
- the precise C# package/runtime versions used for differential tests are recorded;
- there is a reproducible way to compile an individual Native translation unit as C++20;
- there is a reproducible way to link subsystem parity tests;
- no audit status is inferred from a file name.

---

## W1 - Independent leaves and existing simple pairs

**17 files.**

See Appendix A for the exact set.

This wave intentionally begins with the two independent missing leaves:

```text
Shaders.cs
Mods/Chat/ChatFont.cs
```

It also re-audits simple existing pairs such as Enums, Frontend, Branding, Headless, TouchSettings, MapTexturePack, Crosshair, BuildVersion, Console, Output, and Rng.

Normalize `SetupProgress` to its source-relative Native directory in this wave.

### Prerequisites

- W0 complete.
- No gameplay/entity/network/renderer behavior is required.

### Validation gate

Every W1 file must satisfy all of the following:

1. Complete C# file read.
2. Complete Native pair read if a pair existed.
3. Exact public/internal-equivalent API inventory.
4. Exact constants and enum values.
5. Exact default/static state.
6. Exact construction/copy/value/reference behavior.
7. Exact parse/format/culture behavior.
8. Translation unit compiles independently with its legitimate prerequisites.
9. Deterministic C# versus Native tests pass for every observable operation.
10. No W1 header requires a missing project header except an explicitly frozen W0 declaration contract.
11. Existing `PASS` entries are revalidated rather than blindly inherited.

Special golden checks:

- `Shaders`: exact string hashes.
- `ChatFont`: `Pixels`/`Widths` hashes and `Index`/`Measure`.
- `Crosshair`: enum underlying values, non-Flags behavior, struct value semantics, floating constants, parsing.
- `Rng`: unsigned wrap/promotion/conversion behavior.
- `BuildVersion`/Branding: assembly/process-name/version fallback semantics.
- `Output`: ordering, queue, task/thread behavior.
- `SetupProgress`: source-relative target path.

---

## W2 - Runtime, value, and platform primitives

**37 files.**

This wave establishes contracts heavily reused by later work:

- raw memory/value wrappers;
- low-level model/culling/types;
- input data and desktop input boundary;
- frame timing;
- OpenGL ES bindings;
- OpenAL ES adapter;
- simple network latency/probe primitives;
- OS/logging/window mode primitives;
- compression/archive/parser utilities;
- HTTP synchronization primitive.

### Prerequisites

- W0 adapter rules.
- W1 value/utility contracts.
- exact declaration of `ProgramException` from the future Program counterpart where required.

### Validation gate

In addition to the common whole-file gate:

- every enum has exact C# width, signedness, values, aliases, Flags metadata, composite behavior, and unknown-value formatting where observable;
- every C# struct has exact zero/default construction behavior;
- readonly fields and assignment behavior are reproduced;
- floating constants are checked by bit pattern where relevant;
- OpenTK vector/matrix operations used by this wave are differential-tested against OpenTK 4.9.4;
- FrameTiming scenarios produce the same sequence/results;
- CurrentCulture versus InvariantCulture use is explicit and tested;
- `SyntheticInput` reproduces success/failure semantics of the non-public OpenTK constructors without adding synthetic key/mouse state;
- `PointerInput` preserves synchronization and failure timing;
- GL/ES and AL/ALC adapters are verified through call/state traces rather than visual or audible similarity;
- Windows/Linux/macOS-specific behavior is gated by the same logical OS conditions as C#;
- dedicated-server builds do not accidentally acquire client-only runtime requirements.

Files with previous FAIL results in this wave remain failed until these checks pass.

---

## W3 - Static catalogs, text, and standalone serialization

**10 files.**

This closes:

- `Features`;
- string/font/text tables;
- HUD structural metadata;
- core room/frontend/sound metadata;
- Collada/scripting support;
- extraction/NCSF boundary.

### Prerequisites

- W1 Crosshair accepted before `Features`.
- W2 model/types/math/utility contracts accepted.
- canonical `Metadata` class declaration includes all seven partial-file contributions even though enemy/player-dependent bodies finish later.

### Validation gate

- `Features` default values and getter overrides match C# exactly.
- Feature loading/commit serialization has exact keys, case, culture and omission behavior.
- String tables preserve encoding, indexing, lengths, replacement/error behavior and ordering.
- Metadata dictionaries contain identical keys, values, ordering where observable, normalized vectors, path strings and nullable values.
- HUD raw structures preserve exact numeric interpretation and field ordering.
- Extract operations reproduce file naming, binary parsing, exception behavior and external NCSF calls.
- deterministic exported text/binary results compare against C# fixtures.
- no player/enemy behavior is invented to make the `Metadata` partial declaration compile.

---

## W4 - Core Formats / Scene / Entity contract SCC

**16 files.**

This wave introduces the core object graph:

```text
Scene
Messaging
Read
EntityBase
DynamicLightEntityBase
LightSourceEntity
PointModuleEntity
PlayerSpawnEntity
AiPersonality
Collision
CollisionDetection
Effects
Entity
EntityClass
Formats
RawFormats
```

This is an SCC-oriented wave, not a claim that Formats can be finished independently from entities.

### Prerequisites

- W1-W3 complete.
- complete `Scene` declaration pass across its five source contributors.
- declaration contracts for later Sound/Network/Player types referenced by W4.
- no dummy implementations for those later contracts.

### Validation gate

W4 is a **compile-contract gate**, not yet the full gameplay link gate.

Required:

- all W4 headers parse together without duplicate types;
- all W4 `.cpp` translation units compile;
- `Scene` has exactly one C++ class definition;
- C# message queue timing/order is reproduced;
- EntityBase default state, transforms, model setup, collision setup, processing and destruction contracts are reproduced;
- collision structures and intersection tests match fixed C# vectors/fixtures;
- raw format parsing gives byte-identical field values;
- effect/model/entity readers produce equivalent object graphs for fixed fixtures;
- any unresolved project symbols are present in an explicit W5-W7 SCC symbol manifest and no others are tolerated.

Do not mark an individual W4 dependency as behaviorally implemented merely because a declaration was created for cycle-breaking.

---

## W5 - Audio and foundational network

**17 files.**

This wave completes:

- `Formats/Sound.cs`;
- `Sound/Music.cs`;
- `Sound/Sfx.cs`;
- `Mods/Sound/SfxMixer.cs`;
- network transport/protocol/master/probe/status/lag foundations;
- demo container/read/write/playback foundations;
- map rotation/vote primitives;
- dedicated-server networking foundation.

### Prerequisites

- W2 platform/audio/network primitives.
- W4 Entity/Scene declarations.
- W3 metadata/text as required.

### Validation gate

Network:

- `NetProtocol` packet discriminants and byte encodings are exact.
- Signedness, endian conversion, optional/null fields, string lengths and enum casts match C#.
- C# packet fixtures decode identically in Native.
- Native fixtures decode identically in C#.
- `NetTransport` loopback send/receive preserves packet boundaries, ordering and error handling.
- `NetProbe` behavior matches the accepted W2 pair.
- `NetLag` timing/loss state matches C# for deterministic inputs.
- Demo writer -> reader round trips every record type.
- `tools/nettest`'s three-source boundary (`NetProtocol`, `NetTransport`, `NetProbe`) has a Native equivalent validation target.

Audio:

- sound-format parsing is byte-for-byte equivalent;
- source/channel/instance defaults match C#;
- play/stop/loop state transitions match;
- mixer buffer output matches for fixed deterministic PCM inputs;
- failure to initialize an audio backend follows C# behavior;
- Android `AL`/`ALC` substitution has equivalent caller-visible behavior.

W5 does not yet close the full gameplay link because concrete entities and player/network simulation follow.

---

## W6 - Non-player entities, camera sequences, and enemies

**66 files.**

This wave contains:

- the remaining non-player entities;
- both CamSeq files;
- all 42 enemy implementation files;
- enemy metadata.

The large atomic group is intentional. Enemy spawners, enemy instances, collision, effects, sound and concrete enemy subclasses form a dense implementation cluster.

### Prerequisites

- W3 metadata.
- W4 core Formats/Scene/Entity contracts.
- W5 sound and foundational network contracts.
- exact future declarations for GameState/player members referenced through same/enclosing namespaces.

### Validation gate

For every entity/enemy:

- constructor defaults;
- source data conversions;
- node references;
- initial transforms;
- state-machine initial state;
- state transitions;
- timers/counters;
- signed/unsigned underflow behavior;
- RNG calls;
- collision volumes;
- message dispatch;
- sound/effect calls;
- death/destruction behavior;
- spawn ownership;
- culling visibility;
- serialization/network-observable state

must be compared against C# using fixed scenarios.

Additionally:

- existing SlenchNest and CarnivorousPlant Native pairs receive fresh full audits;
- LightSource and PointModule existing pairs are revalidated against the now-real EntityBase/Scene contracts;
- no enemy header includes a nonexistent placeholder `EnemyInstanceEntity.hpp`/`EnemySpawnEntity.hpp` contract;
- no enemy behavior is accepted merely because the class can be instantiated.

W6 may still have explicitly declared W7 player/game-state symbols. Full W4-W7 link closure is required at W7.

---

## W7 - Player, gameplay, and gameplay-network SCC

**58 files.**

This is the most important SCC wave.

It completes:

- `GameState`;
- `Menu`;
- `SceneSetup`;
- the main PlayerEntity implementation files;
- player metadata and weapons;
- gameplay settings/input settings;
- respawn/spectator/world-event behavior;
- Chat gameplay integration;
- `GamepadInput`;
- gameplay-aware network classes;
- server simulation;
- PlayerEntity network partials;
- core PlayerEntity HUD/render partials assigned to this wave.

### Partial contract prerequisite

Before implementing W7 bodies, freeze the complete `PlayerEntity` declaration across all 21 contributing C# files, including the later W8/W12 body files.

This is mandatory because the existing standalone `PlayerEntity` declaration in `PlayerEntityIconBounds.hpp` cannot coexist with the real class.

### Prerequisites

- W1-W6 contracts.
- input primitives from W2.
- HUD/text/catalogs from W3.
- entity/enemy implementations from W6.
- packet/transport/audio foundations from W5.

### Validation gate

W7 closes the **core gameplay SCC**.

Required:

- exactly one `PlayerEntity` C++ class definition;
- complete member inventory matches all 21 C# partial declarations;
- no extension header redefines `PlayerEntity`;
- all W4-W7 production objects link without unresolved MphRead symbols except explicitly later rendering/UI partial methods;
- player static state and slot arrays initialize identically;
- input snapshots and per-frame ordering match;
- movement/aim/weapon/alt-form calculations match on deterministic frame traces;
- GameState defaults, load/save and match transitions match;
- GamepadInput consumes W2 state exactly as C#;
- network intent/snapshot/host/client flows serialize identically;
- authoritative versus predicted damage behavior is preserved;
- hit prediction and unlagged paths preserve enable/default semantics;
- server simulation produces equivalent state for fixed intent streams;
- scoreboard/player color/room change/match-end behavior matches;
- chat integration adds no extra state outside the C# partial;
- RespawnChoice and spectator behavior match;
- SceneSetup loads and initializes the same logical object graph.

This is the first checkpoint where the central simulation should be linkable as a coherent Native library.

---

## W8 - Renderer, exports, thumbnails, and render partials

**13 files.**

Completes:

- `Renderer.cs`;
- `Selection.cs`;
- image export;
- movie path;
- screen capture;
- thumbnail generation/capture/mode;
- hunter preview;
- PlayerEntity Pro HUD partial;
- PreviewCamera Scene partial;
- SmoothHudIcon.

### Prerequisites

- W1 shader strings.
- W2 GL/ES/timing/window contracts.
- W3 HUD/text/metadata.
- W7 gameplay SCC.

### Validation gate

Prefer deterministic render-state comparison before relying on screenshot similarity.

Required:

- identical render-item collection and sorting;
- identical matrix/material/texture state;
- identical shader source supplied to GL;
- identical uniform values for fixed scenes;
- identical selection state;
- exact frame-timing interaction;
- identical movie state transitions;
- image encoders receive identical pixels for deterministic fixtures;
- screenshot/thumbnail dimensions and crop math match;
- ThumbnailMode correctly observes Music/Sfx contracts rather than carrying its previous dependency-blocked implementation;
- PreviewCamera operates on the canonical Scene type rather than a standalone substitute;
- PlayerEntity Pro HUD methods implement members declared in the canonical PlayerEntity contract.

GPU screenshot comparisons may be supplementary, but a small driver-dependent pixel difference must not be used either to excuse a known state mismatch or to invent compensating C++ behavior.

---

## W9 - MapGen, repacking, and memory tooling

**19 files.**

Completes:

- remaining `Mods/MapGen`;
- `MemoryClasses`;
- collision/entity/model repackers.

### Prerequisites

- W3 metadata and extract utilities.
- W4 formats/collision.
- W6 entities.
- W7 gameplay types where referenced.
- W8 model/render/export contracts where actually used.

### Validation gate

Map/pack operations:

- parse identical input structures;
- preserve integer/float conversions;
- emit identical room/entity/collision/model logical structures;
- preserve file entry names and ordering where C# observes them;
- compare exact bytes wherever C# output is deterministic;
- for container formats containing nondeterministic timestamps/metadata, compare every C#-observable entry, payload and metadata field rather than masking a semantic difference with a whole-file hash;
- Q3 BSP conversion fixtures produce equivalent maps;
- collision pack/unpack round trips;
- entity pack/unpack round trips;
- model repack round trips.

Partial types:

- one `Repack` declaration;
- one `RepackCollision` declaration;
- `RepackAccess` exposes only the access the C# partial adds;
- no duplicated private packer implementation.

Existing `BuiltMap` and `RepackAccess` pairs must be re-audited rather than carried forward from their FAIL result.

---

## W10 - Update subsystem

**6 files.**

`BuildVersion` completed in W1 and `SyncHttp` in W2. W10 finishes:

```text
DesktopUpdate
ServerUpdate
UpdateCheck
UpdateDownload
UpdateInstall
Updater
```

### Prerequisites

- W1 BuildVersion/Branding.
- W2 SyncHttp/filesystem/process/thread contracts.
- no launcher implementation is required to test core updater behavior.

### Validation gate

Use deterministic local HTTP/filesystem/process fixtures.

Required scenarios:

- no update available;
- valid newer version;
- malformed metadata;
- HTTP failure;
- cancellation before request;
- cancellation during transfer;
- partial file;
- invalid archive;
- desktop install staging;
- relaunch argument preservation;
- cleanup;
- server startup update decision;
- active-player deferral where specified;
- disabled update state;
- page-open success/failure;
- exact `LastReason`/available-state behavior;
- task completion and exception propagation timing.

Do not replace C# asynchronous behavior with detached threads or synchronous blocking merely because the end result appears the same.

---

## W11 - Portable launcher, DebugLog, and EndScreen

**7 files.**

Completes:

```text
AdventureSave
GameFiles
LauncherPrefs
MatchStart
TextLauncher
DebugLog
EndScreen
```

`LaunchPlan` and `SetupProgress` are already W1.

### Prerequisites

- W7 gameplay/network.
- W8 thumbnails/render prerequisites used by the launcher.
- W9 map support.
- W10 updater.
- W1 Branding/Credits.

### Validation gate

Portable launcher:

- preference defaults;
- preference load/commit formatting;
- game-file readiness/problem detection;
- setup path behavior;
- launch-plan construction;
- MatchStart side effects and ordering;
- text launcher menu transcripts for scripted input;
- launcher -> match -> launcher loop;
- NetSession/NetHostSession shutdown in `finally`;
- update check timing;
- no-display operation.

DebugLog:

- attach/force state;
- file naming/location;
- write/flush/exception output;
- lifecycle and repeated calls.

EndScreen:

- `Available`;
- ready-state transitions;
- next-room state;
- hunter/suit/respawn choice;
- input/pointer hit behavior;
- demo/spectator exclusions.

Previous failed audits for `AdventureSave` and `TextLauncher` are resolved only when this whole dependency closure is present.

---

## W12 - Avalonia GUI, pause UI, and final partial render tails

**25 files.**

Completes all 22 files under:

```text
Mods/Launcher/Gui
```

plus:

```text
Mods/PauseMenu.cs
Mods/Render/PlayerEntityEndScreen.cs
Mods/Render/PreviewPass.cs
```

### Prerequisites

- W8 renderer.
- W10 update.
- W11 portable launcher.
- canonical Scene and PlayerEntity class declarations.

### Validation gate

Desktop client:

- toolkit setup occurs at most once as in C#;
- setup occurs on the calling/main game thread where required;
- `GuiLauncher.TryRun()` returns false for the same probe/setup failures;
- Linux no-display detection matches C#;
- Windows console fallback behavior is preserved;
- launcher -> match -> launcher loop matches;
- each control's default value, binding, event, enabled/visible condition and update behavior matches;
- pause-window lifetime and dispatcher behavior match;
- update badge/UI state matches;
- settings serialize through the same underlying settings contracts;
- no GUI object is introduced into dedicated-server builds.

Android-facing compile:

- Android conditions compile;
- desktop toolkit initialization path is not invoked where C# excludes it;
- shared controls retain the same data behavior.

Partial-type closure:

- `PlayerEntityEndScreen.cpp` implements the already-declared PlayerEntity members;
- `PreviewPass.cpp` implements the already-declared Scene members;
- neither header defines a second class.

At the end of W12, all Scene and PlayerEntity partial-source implementations must be complete.

---

## W13 - Remaining test/tool C# files

**9 files.**

Completes:

```text
Test.cs
Testing/TestEffects.cs
Testing/TestLogic.cs
Testing/TestMisc.cs
Testing/TestOverlay.cs
Testing/TestParse.cs
Testing/TestPlayer.cs
Testing/TestPrint.cs
Testing/TestWeapons.cs
```

### Prerequisites

- W1-W12 production contracts.

### Validation gate

The Native versions must reproduce what these C# files test or print; they must not be rewritten into a different test suite merely because a different test would be easier in C++.

Required:

- same fixture construction;
- same test ordering where visible;
- same conversion/binary checks;
- same expected values;
- same failure criteria;
- same formatting where output is part of the behavior.

In addition, run the migration parity harness accumulated by Waves 1-W12.

A failing C#-parity test must be fixed in the corresponding production migration. Production behavior must not be changed away from C# merely to make a newly invented Native-only expectation pass.

---

## W14 - `ModEntry.cs`

**1 file.**

This file is deliberately almost last.

The runtime call chain is upstream-to-downstream:

```text
Program.Main
  -> ModEntry.TryHandleHeadless
       -> GuiLauncher.TryRun / TextLauncher.Run
       -> update/server/map/network headless paths
  -> Program.CheckSetup
  -> Program.ParseArguments
  -> ModEntry.TryHandle
  -> normal Program behavior
```

The implementation order must therefore be the reverse of the runtime dependency direction:

```text
Launcher/update/network/etc.
  -> ModEntry
  -> Program
```

### Prerequisites

Every production subsystem called directly or indirectly by `ModEntry` must be real, not stubbed.

That includes at least:

- InputSettings;
- LauncherPrefs;
- DebugLog;
- Render overrides;
- DesktopUpdate/ServerUpdate/Updater;
- NetLag/NetUnlagged/NetHitPrediction;
- Credits;
- custom maps/map bundles;
- GuiLauncher/TextLauncher;
- master server;
- dedicated/server paths;
- shutdown signals;
- network session/host session;
- gameplay launch paths.

### Validation gate

Build an exhaustive argv dispatch table from the C# source.

For every branch record and test:

- arguments;
- relevant build condition;
- OS condition;
- return value;
- console output where specified;
- environment/exit-code changes;
- files/process/network side effects;
- whether normal Program setup is bypassed.

Critical ordering assertions:

1. `InputSettings.Load` precedes gameplay/player creation.
2. launcher preferences/debug setup occurs before later launch paths.
3. desktop-update apply runs before ordinary file/window work.
4. headless commands work without `paths.txt`.
5. launcher dispatch happens before Program's normal setup check.
6. GUI is attempted before text fallback when enabled.
7. Windows GUI fallback shows a console before TextLauncher.
8. server build never opens a launcher.
9. dedicated/master server update setup occurs before bind/run.
10. a non-handled invocation returns false rather than consuming Program's normal path.

All preprocessor variants must be tested:

```text
normal client + MPHREAD_AVALONIA
MPHREAD_SERVER
relevant OS branches
```

Only after the matrix passes is `ModEntry` complete.

---

## W15 - `Program.cs`

**1 file. Final migration wave.**

`Program.cs` is the process entry and must be migrated only after its downstream graph exists.

Its `ProgramException` declaration may have been exposed earlier for dependency reasons; that does not make Program complete.

### Prerequisites

- W14 ModEntry accepted.
- ConsoleSetup accepted.
- ConsoleWindow accepted.
- Branding accepted.
- Paths/Extract/Read accepted.
- Export/Images accepted.
- SoundRead accepted.
- Metadata accepted.
- Renderer/Scene accepted.
- launcher/headless dispatch accepted.

### Validation gate

Run an end-to-end process-entry matrix against C#.

At minimum:

```text
no args on Windows client
no args on macOS client
no args on Linux client
launcher flag
text launcher flag
menu override
server build with no args
server/master/dedicated command
credits
update
mapbundle
valid paths.txt
missing paths.txt
incompatible paths.txt
ROM dragged/passed as sole path
extract
each export class
room by ID
room by name
model
First Hunt model
invalid room
multiple/empty room-model combinations
mode/player/boss/node/entity options
normal renderer launch
```

Verify:

- `ConsoleSetup.Run()` timing;
- Windows `ConsoleWindow.Prepare(args)` timing;
- `TryHandleHeadless` occurs before setup validation;
- early return behavior;
- CheckSetup order;
- argument parsing;
- `ModEntry.TryHandle`;
- export dispatch;
- room/model construction;
- renderer lifecycle;
- `Environment.Exit`/exit-code behavior;
- exception timing;
- exact Version/min-extract-version behavior.

### Final repository gate

After W15:

- 302/302 C# files have corresponding Native pairs.
- Every pair has a whole-file parity audit.
- No pair is accepted solely because it existed before the migration.
- `SetupProgress` is structurally normalized.
- No Native project include targets a nonexistent MphRead header.
- No C# partial type is represented by duplicate C++ class definitions.
- Desktop client Native build succeeds.
- Dedicated-server Native build succeeds.
- Android-facing adapter/shared-contract build succeeds if Native Android packaging is in scope; otherwise the GL/AL/input adapter differential suite still passes.
- all Native subsystem tests pass;
- all C# differential/golden fixtures pass;
- complete Native program links;
- process-entry matrix passes;
- no known unresolved C# behavior remains.

---

# 10. Thin-adapter rules

An unavoidable platform adapter is acceptable only when direct C++20 translation cannot call the same managed framework contract.

Each adapter must satisfy all of these rules.

1. Its API surface is no broader than the C# caller requires.
2. It owns no gameplay state absent from C#.
3. It adds no caching absent from C# unless provably unobservable.
4. It does not change exception timing.
5. It does not silently normalize invalid input that C# rejects.
6. It does not add retry behavior.
7. It does not change thread affinity.
8. It does not change cancellation semantics.
9. It does not change synchronous/asynchronous ordering.
10. It does not add "safe" clamping where C# permits unusual/unknown values.
11. It preserves IEEE float behavior as closely as the C++ implementation permits.
12. It preserves integer overflow/wrap behavior required by the C# expression.
13. It preserves enum unknown values instead of forcing them to named values.
14. It preserves path and culture behavior where visible.
15. It preserves the platform condition under which the C# path executes.
16. It is validated by C# versus Native differential tests at the boundary.

Particularly sensitive adapters are:

```text
OpenTK.Mathematics
GLFW KeyboardState / MouseState / Gamepad
OpenGL
GlEs
OpenAL
AlEs / AlcEs
SoundFlow
Avalonia
NcsfPlay
HTTP
Process
filesystem/path
socket APIs
Stopwatch/time
thread/task/cancellation
CultureInfo/string formatting
```

---

# 11. Semantic hazards to check on every file

The following must be part of every per-file audit when applicable:

- exact namespace/type visibility;
- static class versus instantiable class;
- sealed/non-sealed/final behavior;
- inheritance;
- interfaces;
- virtual/override dispatch;
- partial-type ownership;
- enum underlying type;
- `[Flags]`;
- enum aliases;
- implicit enum values;
- unknown enum values;
- nullable reference/value types;
- C# default construction;
- struct copying;
- readonly fields;
- mutable fields;
- field order where layout is observable;
- property getter/setter visibility;
- reference identity;
- collection aliasing;
- arrays versus spans versus lists;
- `ref`, `out`, `in`;
- unchecked integer wrap;
- checked conversion where present;
- signed/unsigned promotion;
- division semantics;
- float precision and constants;
- NaN/infinity behavior where reachable;
- string ordinal versus culture comparisons;
- lower/upper casing culture;
- interpolation formatting;
- UTF encoding;
- newline behavior;
- dictionary/frozen/immutable collection semantics;
- enumeration order where observed;
- static initialization;
- lazy initialization;
- explicit static constructors;
- exception types;
- exception timing;
- disposal/`using`;
- finalization only where C# actually exposes it;
- delegates/events;
- reflection;
- boxing/unboxing;
- `object` payload semantics;
- locking;
- atomics/volatile;
- task completion;
- cancellation;
- thread creation/join;
- Stopwatch/time units;
- file open/share/truncate behavior;
- filesystem case/path behavior;
- process start/exit;
- socket blocking/timeouts;
- binary endianness;
- packed structure layout;
- serialization field/order behavior;
- conditional compilation.

---

# 12. Unresolved questions and required resolution

These are not reasons to stop the migration. Each has a concrete resolution step.

### Q1. What is the authoritative Native build system?

The current dependency inventory does not show a tracked Native Makefile/CMake project covering all translation units.

**Resolution before W2:** establish or identify one C++20 build definition capable of:

```text
individual TU compile
desktop-client subsystem link
dedicated-server subsystem link
parity-test link
full final executable link
```

Build infrastructure must not contain application behavior.

### Q2. Are extra declaration-only support headers permitted?

C# partial classes make some centralized declaration mechanism unavoidable.

**Preferred resolution:**

- use the natural canonical file for `Scene`, `PlayerEntity`, and `Metadata`;
- use canonical existing Utility headers for Repack where practical;
- permit a declaration-only helper header only if C++ syntax makes the one-definition requirement otherwise unmaintainable.

No support header may contain alternative runtime behavior.

### Q3. How strict is target path identity?

The goal says one-for-one under `src/MphRead.Native`.

**Resolution:** use matching source-relative directories as the canonical final form. Normalize `SetupProgress` during W1.

### Q4. How is `ProgramException` made available before Program is complete?

**Resolution:** put its exact declaration in `Program.hpp` during the declaration pass. Do not implement or accept `Program.Main` until W15.

### Q5. What substitutes for OpenTK mathematics?

Choosing a C++ math library is not enough; matrix layout and method behavior are observable.

**Resolution:** whichever implementation is selected, differential-test every OpenTK operation actually used by the C# files against OpenTK 4.9.4, including matrix multiplication order, row/column access, normalization, cross products, inversion, rotations, scale extraction and equality behavior.

### Q6. How are managed collection/runtime semantics represented?

`Dictionary`, `FrozenDictionary`, immutable collections, `ReadOnlySpan`, nullable values, reflection and boxing do not have automatic one-to-one C++ semantics.

**Resolution:** define small language-level equivalents only for the exact observed contract and test ordering, aliasing, default and error behavior. Do not create a broad replacement .NET framework.

### Q7. What is the Native Android deliverable?

The C# Android project compiles all MphRead source and depends on `GlEs`/`AlEs` aliases.

**Resolution:** unless the migration scope explicitly excludes a Native Android executable, treat Android as a required final platform. If packaging is temporarily outside scope, Android-specific shared-source adapter semantics remain required differential-test targets.

### Q8. What is the NcsfPlay strategy?

`NcsfPlay` is outside `src/MphRead` but is a direct project dependency.

**Resolution:** treat it as an external dependency boundary for this 302-file migration. Reproduce exactly the subset of its public behavior used by MphRead through a thin Native adapter or separately approved port. Do not reimplement it opportunistically inside MphRead files.

### Q9. How are async `Task` methods translated?

Update, audio and related code contain task/cancellation behavior.

**Resolution:** first record each C# method's completion, blocking, cancellation and exception behavior; then select the smallest C++ future/thread/event representation reproducing that behavior. Do not standardize everything on one asynchronous architecture before auditing the source.

### Q10. How are old audit path labels handled?

The existing audit ledger contains stale labels, including examples such as:

```text
Mods/Launcher/Portable/Frontend.cs -> Formats/Frontend.cs
Mods/Input/EsBindings.cs            -> Mods/Render/EsBindings.cs
Utility/RawStructs.cs               -> Mods/MapGen/RawStructs.cs
Sound/FhSound.cs                    -> Formats/FhSound.cs
Mods/MapReport.cs                   -> Mods/MapGen/MapReport.cs
Mods/AdventureSave.cs               -> Mods/Launcher/Portable/AdventureSave.cs
Mods/Testing/FrameTimingCheck.cs    -> Mods/Render/FrameTimingCheck.cs
Mods/Launcher/Portable/ThumbnailMode.cs -> Mods/ThumbnailMode.cs
Mods/Testing/SyntheticInput.cs      -> Mods/Input/SyntheticInput.cs
Mods/Render/RenderOptions.cs        -> Mods/RenderOptions.cs
```

**Resolution:** all future audit records use the actual source-tree path. Historical result text may be retained only with its path mapped to the canonical file.

### Q11. What does an existing Native include prove?

Nothing about semantics. Several existing pairs include project headers whose counterparts are not yet present.

**Resolution:** regenerate the Native include-closure report after every wave. A pair cannot pass a wave if its include graph depends on nonexistent headers not covered by an explicit declaration-pass contract.

---

# Appendix A - File-level completion-wave assignment

This appendix accounts for every C# source file exactly once.

Wave tags specify **final behavior completion**, not an earlier declaration-only partial-class pass.

## `src/MphRead` root - 16 files

- **W1:** `Shaders.cs`
- **W2:** `Memory.cs`, `MemoryArrays.cs`
- **W3:** `Features.cs`, `Strings.cs`
- **W4:** `Messaging.cs`, `Read.cs`, `Scene.cs`
- **W7:** `GameState.cs`, `Menu.cs`, `SceneSetup.cs`
- **W8:** `Renderer.cs`, `Selection.cs`
- **W9:** `MemoryClasses.cs`
- **W13:** `Test.cs`
- **W15:** `Program.cs`

## `Entities` - 25 files

- **W4:** `EntityBase.cs`, `LightSourceEntity.cs`, `PlayerSpawnEntity.cs`, `PointModuleEntity.cs`
- **W6:** `AreaVolumeEntity.cs`, `ArtifactEntity.cs`, `BeamEffectEntity.cs`, `BeamProjectileEntity.cs`, `BombEntity.cs`, `DoorEntity.cs`, `EnemyInstanceEntity.cs`, `EnemySpawnEntity.cs`, `FlagBaseEntity.cs`, `ForceFieldEntity.cs`, `ItemInstanceEntity.cs`, `ItemSpawnEntity.cs`, `JumpPadEntity.cs`, `MorphCameraEntity.cs`, `NodeDefenseEntity.cs`, `ObjectEntity.cs`, `OctolithFlagEntity.cs`, `PlatformEntity.cs`, `RoomEntity.cs`, `TeleporterEntity.cs`, `TriggerVolumeEntity.cs`

## `Entities/CamSeq` - 2 files

- **W6:** `CameraSequence.cs`, `CamSeqEntity.cs`

## `Entities/Enemies` - 42 files

- **W6:** `00_WarWasp.cs`, `01_Zoomer.cs`, `02_Temroid.cs`, `03_Petrasyl1.cs`, `04_Petrasyl2.cs`, `05_Petrasyl3.cs`, `06_Petrasyl4.cs`, `10_BarbedWarWasp.cs`, `11_Shriekbat.cs`, `12_Geemer.cs`, `16_Blastcap.cs`, `18_AlimbicTurret.cs`, `19_Cretaphid.cs`, `20_CretaphidEye.cs`, `21_CretaphidCrystal.cs`, `23_PsychoBit.cs`, `24_Gorea1A.cs`, `25_GoreaHead.cs`, `26_GoreaArm.cs`, `27_GoreaLeg.cs`, `28_Gorea1B.cs`, `29_GoreaSealSphere1.cs`, `30_Trocra.cs`, `31_Gorea2.cs`, `32_GoreaSealSphere2.cs`, `33_GoreaMeteor.cs`, `35_Voldrum.cs`, `36_Voldrum.cs`, `37_Quadtroid.cs`, `38_CrashPillar.cs`, `39_FireSpawn.cs`, `40_EnemySpawner.cs`, `41_Slench.cs`, `42_SlenchShield.cs`, `43_SlenchNest.cs`, `44_SlenchSynapse.cs`, `45_SlenchTurret.cs`, `46_LesserIthrak.cs`, `47_GreaterIthrak.cs`, `49_ForceFieldLock.cs`, `50_HitZone.cs`, `51_CarnivorousPlant.cs`

## `Entities/Players` - 14 files

- **W4:** `DynamicLightEntity.cs`
- **W7:** `HalfturretEntity.cs`, `PlayerAi.cs`, `PlayerCamera.cs`, `PlayerCollision.cs`, `PlayerDialog.cs`, `PlayerDraw.cs`, `PlayerEntity.cs`, `PlayerHud.cs`, `PlayerInput.cs`, `PlayerPause.cs`, `PlayerProcess.cs`, `PlayerScan.cs`, `PlayerSound.cs`

## `Export` - 3 files

- **W3:** `Collada.cs`, `Scripting.cs`
- **W8:** `Images.cs`

## `Formats` - 18 files

- **W1:** `Enums.cs`, `Frontend.cs`
- **W2:** `Culling.cs`, `EntityEnemy.cs`, `FhSound.cs`, `Model.cs`, `NodeData.cs`, `Types.cs`
- **W4:** `AiPersonality.cs`, `Collision.cs`, `CollisionDetection.cs`, `Effects.cs`, `Entity.cs`, `EntityClass.cs`, `Formats.cs`, `RawFormats.cs`
- **W5:** `Sound.cs`
- **W8:** `Movie.cs`

## `HUD` - 1 file

- **W3:** `HudInfo.cs`

## `Metadata` - 7 files

- **W3:** `FrontendMeta.cs`, `Metadata.cs`, `Rooms.cs`, `SoundMeta.cs`
- **W6:** `Enemies.cs`
- **W7:** `Player.cs`, `Weapons.cs`

## `Mods` - 25 files

- **W1:** `Branding.cs`, `ConsoleWindow.cs`, `Credits.cs`, `Headless.cs`
- **W2:** `HunterSuits.cs`, `LogShare.cs`, `RenderOptions.cs`, `ShutdownSignals.cs`, `ThumbnailHost.cs`, `ThumbnailLog.cs`, `WindowMode.cs`
- **W7:** `GameSettings.cs`, `InputSettings.cs`, `RespawnChoice.cs`, `SpectatorMode.cs`, `WorldEvents.cs`
- **W8:** `ScreenCapture.cs`, `ThumbnailBatch.cs`, `ThumbnailCapture.cs`, `ThumbnailGenerator.cs`, `ThumbnailMode.cs`
- **W11:** `DebugLog.cs`, `EndScreen.cs`
- **W12:** `PauseMenu.cs`
- **W14:** `ModEntry.cs`

## `Mods/Chat` - 3 files

- **W1:** `ChatFont.cs`
- **W7:** `ChatBox.cs`, `PlayerEntityChatHud.cs`

## `Mods/Input` - 11 files

- **W1:** `TouchSettings.cs`
- **W2:** `GamepadDesktop.cs`, `GamepadLayout.cs`, `GamepadMappings.cs`, `GamepadProbe.cs`, `GamepadState.cs`, `PadBindings.cs`, `PointerInput.cs`, `StylusZone.cs`, `SyntheticInput.cs`
- **W7:** `GamepadInput.cs`

## `Mods/Launcher/Gui` - 22 files

- **W12:** `CrosshairPreview.cs`, `DemoPickerView.cs`, `GuiLauncher.cs`, `GuiTheme.cs`, `HomeView.cs`, `HomeWindow.cs`, `KeyRow.cs`, `MapPickerView.cs`, `MenuEntry.cs`, `PadRow.cs`, `PauseMenuView.cs`, `PauseMenuWindow.cs`, `ProgressRow.cs`, `Rows.cs`, `ServerRow.cs`, `SettingsView.cs`, `SettingsWindow.cs`, `SliderRow.cs`, `SplashView.cs`, `TrackedText.cs`, `UiCapture.cs`, `UpdateBadge.cs`

## `Mods/Launcher/Portable` - 7 files

- **W1:** `LaunchPlan.cs`, `SetupProgress.cs`
- **W11:** `AdventureSave.cs`, `GameFiles.cs`, `LauncherPrefs.cs`, `MatchStart.cs`, `TextLauncher.cs`

## `Mods/MapGen` - 16 files

- **W1:** `MapTexturePack.cs`
- **W9:** `BuiltMap.cs`, `CustomRooms.cs`, `MapBuilder.cs`, `MapBundle.cs`, `MapCollisionPacker.cs`, `MapDefinition.cs`, `MapNodePacker.cs`, `MapPacker.cs`, `MapReport.cs`, `MapTextureBake.cs`, `Q3Bsp.cs`, `Q3Convert.cs`, `Q3Import.cs`, `RawStructs.cs`, `RepackAccess.cs`

## `Mods/Network` - 43 files

- **W2:** `NetLag.cs`, `NetProbe.cs`
- **W5:** `DedicatedServer.cs`, `DemoClip.cs`, `DemoFile.cs`, `DemoInfo.cs`, `DemoLibrary.cs`, `DemoPlayback.cs`, `DemoRecorder.cs`, `MapRotation.cs`, `MapVote.cs`, `NetMaster.cs`, `NetProtocol.cs`, `NetStatus.cs`, `NetTransport.cs`
- **W7:** `MapAudit.cs`, `MechanicsDump.cs`, `NetCheckClient.cs`, `NetConnectCommand.cs`, `NetDamage.cs`, `NetDiagnostics.cs`, `NetFeatureCheck.cs`, `NetHitPrediction.cs`, `NetHooks.cs`, `NetHostSession.cs`, `NetLaunch.cs`, `NetLog.cs`, `NetMatchEnd.cs`, `NetMatchSync.cs`, `NetPlayerBridge.cs`, `NetPlayerSetup.cs`, `NetRoomChange.cs`, `NetScoreboard.cs`, `NetSession.cs`, `NetSlotManager.cs`, `NetTestScript.cs`, `NetUnlagged.cs`, `PlayerColors.cs`, `PlayerEntityNetAim.cs`, `PlayerEntityNetHud.cs`, `ServerSim.cs`, `ServerSimCheck.cs`, `WeaponDps.cs`

## `Mods/Render` - 16 files

- **W1:** `Crosshair.cs`
- **W2:** `EsBindings.cs`, `EsShaders.cs`, `FrameTiming.cs`, `FrameTimingCheck.cs`, `GlEs.cs`
- **W7:** `PlayerEntityAmmoClear.cs`, `PlayerEntityIconBounds.cs`, `PlayerEntityStylusHud.cs`, `PlayerEntityVoteHud.cs`
- **W8:** `HunterPreview.cs`, `PlayerEntityProHud.cs`, `PreviewCamera.cs`, `SmoothHudIcon.cs`
- **W12:** `PlayerEntityEndScreen.cs`, `PreviewPass.cs`

## `Mods/Sound` - 2 files

- **W2:** `AlEs.cs`
- **W5:** `SfxMixer.cs`

## `Mods/Update` - 8 files

- **W1:** `BuildVersion.cs`
- **W2:** `SyncHttp.cs`
- **W10:** `DesktopUpdate.cs`, `ServerUpdate.cs`, `UpdateCheck.cs`, `UpdateDownload.cs`, `UpdateInstall.cs`, `Updater.cs`

## `Sound` - 2 files

- **W5:** `Music.cs`, `Sfx.cs`

## `Testing` - 8 files

- **W13:** `TestEffects.cs`, `TestLogic.cs`, `TestMisc.cs`, `TestOverlay.cs`, `TestParse.cs`, `TestPlayer.cs`, `TestPrint.cs`, `TestWeapons.cs`

## `Utility` - 11 files

- **W1:** `Console.cs`, `Output.cs`, `Rng.cs`
- **W2:** `Analyzer.cs`, `Archive.cs`, `Compress.cs`, `Parser.cs`
- **W3:** `Extract.cs`
- **W9:** `RepackCollision.cs`, `RepackEntity.cs`, `RepackModel.cs`

---

# Appendix B - Wave count proof

```text
W1   17
W2   37
W3   10
W4   16
W5   17
W6   66
W7   58
W8   13
W9   19
W10   6
W11   7
W12  25
W13   9
W14   1
W15   1
------------
Total 302
```

No source file appears in more than one completion wave.

Declaration-only partial-type work performed earlier does not change these counts.

---

# Appendix C - Acceptance status vocabulary

Use exactly these states in future migration tracking:

```text
MISSING
    No Native pair yet.

PAIRED-UNAUDITED
    hpp/cpp exist, but whole-file C# parity has not been demonstrated.

AUDIT-FAIL
    Pair was inspected and a semantic or dependency-closure defect remains.

CONTRACT-ONLY
    Exact declarations needed for a dependency/SCC exist, but the C# file's
    implementation has not been completed.

PARITY-PASS
    Whole file audited, dependency contracts closed to the wave's required
    level, differential/golden tests pass, and no known C# behavior is absent.

FINAL-PASS
    PARITY-PASS plus all later SCC/link/platform gates complete.
```

`PAIRED` by itself is never a parity status.

---

# Final ordering rule

For normal per-file work, take files in this order:

```text
W1 -> W2 -> W3
        |
        +-> W10 may proceed once its actual W1/W2 prerequisites close

W4 -> W5 -> W6 -> W7
                 core/gameplay SCC closes here

W8 -> W9
 |
 +-> W10 if not already complete

W10 -> W11 -> W12
               render/launcher/partial SCC closes here

W13

W14 ModEntry

W15 Program
```

Within a wave, independent leaves may be assigned in parallel.

Before handing a file to a worker, check:

```text
1. Is every direct value/inheritance dependency accepted?
2. Is every reference-only cyclic dependency represented by an exact declaration?
3. Does this file contribute to a partial type?
4. Is its canonical class declaration already fixed?
5. Does it cross a platform boundary?
6. Is the relevant adapter contract already differential-tested?
7. Does an existing pair have an old PASS or FAIL that must be rechecked?
8. Does the Native include graph point at anything nonexistent?
```

If any answer blocks the file, move to another independent leaf rather than creating placeholder behavior.

The immediate recommended pair remains:

```text
Worker A: Shaders.cs
Worker B: Mods/Chat/ChatFont.cs
```

After those two, continue through the remaining W1 files, then open W2.
