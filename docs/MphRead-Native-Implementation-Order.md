# Fruity-Prime authoritative C# -> C++20 implementation order

## 1. Reviewed state, authority, and scope

**Reviewed `develop2` parent commit:** `17f4216d59be3cbdead0e8cbf6ffac169872df8c`  
**Reviewed parent tree:** `59488f2506427a06574a5f2ad334c20d42984e05`

This document is the authoritative implementation-order and dependency-closure plan for the strict C# -> C++20 migration of three separate source-to-Native targets:

1. `src/MphRead/**/*.cs` -> `src/MphRead.Native/**/*.hpp` + `src/MphRead.Native/**/*.cpp`
2. `src/NcsfPlay/**/*.cs` -> `src/NcsfPlay.Native/**/*.hpp` + `src/NcsfPlay.Native/**/*.cpp`
3. local `src/MphRead.Android/**/*.cs` -> `src/MphRead.Native.Android/**/*.hpp` + `src/MphRead.Native.Android/**/*.cpp`

The C# source in the corresponding source project is the sole behavioral specification. Project/build files define target boundaries, compilation conditions, package/runtime mechanics, and platform ownership; they do not authorize Native-only application behavior.

### 1.1 Live inventory reconciliation

The live source trees contain **356 unique physical C# migration units**:

| Source project | Physical C# files | Native owner | Live same-path Native pairs | Physical C# files without a live pair |
|---|---:|---|---:|---:|
| `src/MphRead` | 302 | `src/MphRead.Native` | 83 | 219 |
| `src/NcsfPlay` | 36 | `src/NcsfPlay.Native` | 0 | 36 |
| local `src/MphRead.Android` | 18 | `src/MphRead.Native.Android` | 0 | 18 |
| **Total unique physical C# files** | **356** |  | **83** | **273** |

Inventory facts are not parity claims:

- The 302-file `src/MphRead` source census and W1-W15 assignment remain unchanged from the earlier reviewed plan. The C# tree did not change between that reviewed source snapshot and this reviewed parent.
- The old Native count of 48 pairs is stale. The live `src/MphRead.Native` tree now has **83** same-relative-path `.hpp/.cpp` pairs.
- The former `SetupProgress` relocation mismatch is resolved: the live pair is now correctly colocated at `src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp/.cpp`.
- `src/NcsfPlay.Native` does **not** exist at the reviewed parent. Its 36 one-file counterparts are therefore all still structurally absent.
- `src/MphRead.Native.Android` does **not** exist at the reviewed parent. Its 18 local Android one-file counterparts are therefore all still structurally absent.
- `MphRead.Android.csproj` disables default compile items and explicitly includes `*.cs`, so all 18 local C# files in `src/MphRead.Android` are in scope, including `TouchControls.cs` and `TouchOverlayView.cs`.
- Android also recompiles all 302 `src/MphRead/**/*.cs` files in its C# project. Those are **shared source inputs, not 302 additional physical migration units**. Their Native ownership remains `src/MphRead.Native`; the Android Native target must consume that translated shared core under Android build/platform semantics instead of creating a second fork of those 302 implementations.
- A live Native pair means only that implementation artifacts exist. It does not, by itself, establish a strict parity audit, focused synthetic checks, repository build/link, runtime/device validation, C# differential validation, or CI validation.

The exact assignment sections below reconcile to:

- MphRead: 302 scheduled rows, 302 unique paths, 0 missing, 0 extra, 0 duplicate.
- NcsfPlay: 36 scheduled rows, 36 unique paths, 0 missing, 0 extra, 0 duplicate.
- MphRead.Android local: 18 scheduled rows, 18 unique paths, 0 missing, 0 extra, 0 duplicate.
- Combined physical source set: **356 scheduled rows and 356 unique paths**.

The three order sequences are dependency-aware authoring bands, not one flattened project graph and not a claim that an earlier band is automatically behavior-complete before a later band.

## 2. Non-negotiable migration rules

1. **C# is the only behavioral specification.** Existing Native code is not specification. A live counterpart may be reused only after its behavior is checked against the complete authoritative C# file and the relevant contracts.
2. **Preserve accepted review evidence.** A file already completed and audited against an unchanged authoritative C# source and unchanged relevant contract does not need to be moved or gratuitously re-audited. Pair-only files without accepted review evidence remain open.
3. **One physical C# source file maps to one Native `.hpp/.cpp` pair at the same relative path and stem inside its owning Native project.** Keep every `.hpp` beside its `.cpp`. Never move counterpart headers into a generic `include` directory.
4. **Do not invent cross-project ownership.** `MphRead.Native`, `NcsfPlay.Native`, and `MphRead.Native.Android` remain separate owners. Shared linkage is allowed where the C# projects share/reference code; collapsing them into one invented Native project is not.
5. **Do not introduce C#-unmatched policy/support source or aggregation headers.** In particular, do not add `*.Partials.hpp`, policy helpers, or aggregate ownership headers merely to break C++ cycles.
6. **No-C++-独自仕様.** Do not add Native-only retry, fallback, normalization, caching, validation, logging policy, state ownership, exception swallowing, command routing, launcher policy, or gameplay policy unless the C# source specifies it.
7. **Thin platform/runtime/library adapters are permitted only where unavoidable.** They implement mechanics, not application policy, and must preserve observable C# semantics.
8. **A Native-only `main`, `WinMain`, service wrapper, process host, or Android bootstrap is mechanical only.** It may enter the translated C# policy at the correct seam; it may not replace `Program.Main`, `ModEntry`, launcher routing, Android activity/view policy, or other C# dispatch decisions.
9. **Partial C# types retain per-file counterpart ownership.** Every contributing `.cs` still gets its own colocated pair while the merged C++ type has one canonical declaration owner. Contributor headers must not define duplicate copies of the same merged class/static type.
10. **No placeholder, dummy return, no-op body, stand-in type, `void*` escape hatch, broadened overload/template contract, or silent behavior deletion may count as migrated behavior.**
11. **Compile success is not parity.** Implementation, focused synthetic checks, repository build/link, runtime/device checks, C# differential checks, and CI are distinct evidence dimensions.
12. **Preserve C# initialization and lifetime timing.** Do not replace first-use static initialization, async/task ownership, cancellation, callbacks, stream/socket/device/window lifetime, or exception timing with convenient C++ policy.
13. **Build-condition semantics are specification.** Desktop client, dedicated server, Android, and library targets expose different host/library mechanics. Do not collapse them into one behavior or allow a host adapter to bypass shared C# policy.
14. **Binary and numeric compatibility is behavioral compatibility.** Preserve byte layout, signedness, endianness, overflow/wrap, floating-point edge behavior, enum values, flags, string/culture/encoding rules, stream positions, and protocol/file serialization exactly where observable.

## 3. Dependency graphs that must be tracked separately

The migration must maintain four related but non-equivalent graphs.

### 3.1 Declaration / forward-declaration graph

This graph answers what a C++ translation unit must know at declaration time.

- pointer/reference use may need only a forward declaration;
- by-value fields, inheritance, nested value types, templates requiring complete types, and exact ABI/layout require complete declarations;
- same-namespace and enclosing-namespace references must be inspected explicitly;
- a partial type must have one canonical declaration owner before multiple contributor bodies are integrated.

A declaration seam can make a file authorable. It does not make the dependency behavior-complete.

### 3.2 Compile / link / member graph

This graph includes actual called/accessed members, private cross-partial members, generated overload choices, templates/generic translations, and linked bodies. It must model:

- same/enclosing-namespace member references;
- overload resolution, optional/default values, `ref`/`out`, readonly/mutable struct behavior;
- generic constraints, enum/type constraints, nullability, reference identity, and default construction;
- private-member access across C# partial contributors;
- conditional members compiled under `MPHREAD_AVALONIA`, `MPHREAD_SERVER`, OS/platform symbols, and the Android compile surface;
- cross-project links from MphRead to NcsfPlay and from the Android head to both shared projects.

### 3.3 Behavioral parity completion graph

This graph decides when a file or aggregate can be marked parity-complete. It includes compile/link dependencies plus:

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

### 3.4 Cross-project ownership graph

The C# project boundaries produce this Native ownership graph:

```text
src/NcsfPlay (36)
    -> src/NcsfPlay.Native
           ^
           | library dependency
           |
src/MphRead (302)
    -> src/MphRead.Native
           ^
           | shared core reused under Android platform semantics
           |
src/MphRead.Android local adapters (18)
    -> src/MphRead.Native.Android
```

`MphRead.csproj` has a project reference to `NcsfPlay.csproj`. `MphRead.Android.csproj` also references `NcsfPlay.csproj` and directly compiles all `../MphRead/**/*.cs`. Therefore the eventual Android Native target must link/consume **both** the translated NcsfPlay library and the translated MphRead core. It must not duplicate either project into `MphRead.Native.Android`.

## 4. MphRead process entry and terminal integration order

`src/MphRead/Program.cs` remains the true desktop/server process specification. The launcher is downstream.

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

- `Program.cs` is **W15**, the final MphRead integration unit.
- `Mods/ModEntry.cs` is **W14**, immediately before `Program.cs`.
- The existence of live `Program.hpp/.cpp` or `ModEntry.hpp/.cpp` does not move either file earlier or establish terminal parity.
- `GuiLauncher` and `TextLauncher` are late downstream subsystems; they are not alternate process specifications.
- `TryHandleHeadless(args)` must run before ordinary setup validation/argument dispatch exactly as in C#.
- `MPHREAD_SERVER` excludes the GUI subtree; this is a build condition, not permission to bypass `Program.Main`.
- `src/MphRead.Android/MainActivity.cs` is a different Android lifecycle host. It is not an alternate desktop process specification and must not be used to rewrite W14/W15 desktop/server policy.

## 5. MphRead canonical declaration seams and partial-type closure

### 5.1 `Scene`

Canonical C++ declaration owner: `src/MphRead.Native/Scene.hpp`.

The live C# `Scene` aggregate has six contributors:

1. `src/MphRead/Scene.cs`
2. `src/MphRead/Messaging.cs`
3. `src/MphRead/Renderer.cs`
4. `src/MphRead/Formats/Movie.cs`
5. `src/MphRead/Mods/Render/PreviewCamera.cs`
6. `src/MphRead/Mods/Render/PreviewPass.cs`

Freeze the complete `Scene` declaration surface before W4 bodies need it. Contributor pairs implement only the members owned by their C# files and must not define another `Scene` class. Authoring spans W4/W8/W12; aggregate behavioral completion is no earlier than W12.

`Scene` and concrete entities are mutually dependent. Break the C++ compile cycle with exact declarations/forward declarations and out-of-line definitions, not fake scene/entity stand-ins.

### 5.2 `PlayerEntity`

Canonical declaration owner: `src/MphRead.Native/Entities/Players/PlayerEntity.hpp`.

The aggregate has 21 contributors:

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

Freeze one canonical declaration before the W7 player/gameplay body cluster. The late UI/render contributors keep the aggregate behaviorally open through W12.

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

The declaration surface begins in W3. Static table/state parity cannot close until enemy/player/weapon contributors and their dependencies are complete, no earlier than W7.

### 5.4 `SoundRead`

Canonical declaration owner: `src/MphRead.Native/Formats/Sound.hpp`.

Two C# files contribute to the same partial static type:

- `Formats/FhSound.cs`
- `Formats/Sound.cs`

`FhSound.cs` calls the private `ExportSamples` supplied by `Sound.cs`, and the sound path also closes with `Sfx` behavior. `FhSound.cs` therefore remains in W5. `SoundRead` and its sound/Sfx closure must pass together.

### 5.5 `Repack` / `RepackCollision`

Canonical declaration owners:

- `Repack` -> `src/MphRead.Native/Utility/RepackEntity.hpp`
- `RepackCollision` -> `src/MphRead.Native/Utility/RepackCollision.hpp`

Contributors:

- `Repack`: `Utility/RepackEntity.cs`, `Utility/RepackModel.cs`, `Mods/MapGen/RepackAccess.cs`
- `RepackCollision`: `Utility/RepackCollision.cs`, `Mods/MapGen/RepackAccess.cs`

All contributor files still receive their own matching pairs. The canonical owners hold the merged declarations. Do not create `Repack.Partials.hpp` or another unmatched aggregation header. Both aggregates close in W9.

## 6. Project, platform, runtime, and library boundaries

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

### 6.2 MphRead desktop/server external seams

The live MphRead project references OpenTK 4.9.4, Avalonia 11.3.11 in GUI builds, ReFuel.StbImage 2.1.1, and OpenAL Soft native packaging for non-server audio. Native translation may expose thin math, GL/GLES, OpenAL/ALC, image-codec, UI-dispatcher/window, HTTP, socket/DNS, filesystem/process, and signal mechanics. Launcher, gameplay, update, network, audio, render, and error-handling policy remains in the translated per-file counterparts.

`MphReadServer=true` defines `MPHREAD_SERVER`; normal non-server builds define `MPHREAD_AVALONIA`. The project removes `Mods/Launcher/Gui/**` when Avalonia is not enabled. Server parity therefore requires a server-specific compile/link and startup gate without inventing a GUI dependency or bypassing `Program.Main`/`ModEntry`.

### 6.3 NcsfPlay library boundary

`NcsfPlay.csproj` is a separate `net9.0` library project and currently depends on:

- CommunityToolkit.HighPerformance 8.4.2;
- SoundFlow 1.4.1;
- System.IO.Hashing 10.0.9.

The Native equivalent belongs under `src/NcsfPlay.Native`, not under `MphRead.Native`. Runtime/library adapters for spans/memory ownership, audio streaming, CRC/hash mechanics, compression, and stream I/O must be thin equivalents of the C# contracts. They must not change NCSF parsing, SDAT reconstruction, sequencer timing, channel behavior, sample generation, tag parsing, ReplayGain mathematics, error behavior, or stream semantics.

The NcsfPlay order intentionally builds low-level Nitro Composer structures before runtime playback layers:

```text
NDS header / FAT / INFO / SYMB primitives
    -> section/record structures and SWAV
    -> SWAR / SBNK / SSEQ
    -> SDAT aggregate
    -> TagList / NCSF PSF container
    -> core Channel / Track / Player
    -> NCSF player wrappers / Stream
    -> ReplayGain layer
```

### 6.4 Android source reuse and platform boundary

`MphRead.Android.csproj` targets `net9.0-android35.0`, requires the Android workload/SDK, uses a minimum supported Android version of 24, and deliberately is not the normal desktop solution head. It has default compile items disabled and explicitly compiles:

- all 18 local `src/MphRead.Android/*.cs` files through `<Compile Include="*.cs" />`;
- all `../MphRead/**/*.cs` shared sources directly;
- a project reference to `../NcsfPlay/NcsfPlay.csproj`.

It aliases shared graphics/audio calls mechanically:

- `GL` -> `MphRead.Mods.Render.GlEs`
- `AL` -> `MphRead.Mods.Sound.AlEs`
- `ALC` -> `MphRead.Mods.Sound.AlcEs`

Therefore:

- the 302 shared MphRead translations remain owned by `MphRead.Native`;
- the 36 NcsfPlay translations remain owned by `NcsfPlay.Native`;
- only the 18 Android-local C# files map to `MphRead.Native.Android` pairs;
- `MphRead.Native.Android` must consume the shared native libraries under Android-compatible compile/link settings rather than copy/fork them;
- Android resources/manifest/build metadata are platform assets, not extra C# parity rows;
- any JNI/activity/native loader/bootstrap necessary to enter the translated Android head is a mechanical adapter only and cannot replace the C# lifecycle/GUI/match policy.

The 18 local Android files divide by role:

- lifecycle/GUI composition: `AndroidApp.cs`, `MainActivity.cs`;
- GL/render host: `GameView.cs`, `OffscreenGl.cs`;
- input/touch controls: `AndroidInput.cs`, `GamepadBridge.cs`, `TouchControls.cs`, `TouchOverlayView.cs`;
- match/room integration: `AndroidMatch.cs`, `AndroidMaps.cs`;
- preview/resource/image integration: `AndroidPng.cs`, `AndroidThumbnails.cs`, `PreviewRun.cs`, `PreviewService.cs`;
- update/install/log/platform utility adapters: `AndroidConsole.cs`, `AndroidLogShare.cs`, `ApkInstaller.cs`, `AndroidUpdateInstaller.cs`.

These are adapters around shared MphRead policy where the C# says they are adapters; they are not permission to reimplement shared gameplay/UI decisions independently.

## 7. Authoring bands and closure barriers

### 7.1 MphRead W1-W15

| Wave | Files | Earliest safe authoring purpose | Completion note |
|---:|---:|---|---|
| W1 | 18 | Atomic deterministic leaves and tiny runtime/ABI contracts. | Per-file parity only after applicable exact checks. |
| W2 | 18 | Small data/runtime contracts that do not execute against later engine/launcher state. | Declaration availability is not behavior closure for later SCCs. |
| W3 | 9 | Export/HUD/metadata declaration surfaces and other small contracts. | `Metadata` remains open. |
| W4 | 22 | Core read/model/entity-layout/scene declaration work plus Read-consuming utilities. | Engine macro-SCC remains open. |
| W5 | 11 | SoundRead/Sfx-side closure, wire/demo primitives, server-independent network pieces. | `SoundRead` closes only with its audio/Sfx dependencies. |
| W6 | 66 | Bulk concrete entity/enemy bodies after core declarations are frozen. | Engine macro-SCC remains open. |
| W7 | 67 | Player/game-state/gameplay-network/metadata body closure. | `Metadata` closes here; `PlayerEntity` remains open. |
| W8 | 14 | Renderer/movie/capture/preview bodies and thumbnail implementation. | `Scene` and `PlayerEntity` remain open. |
| W9 | 20 | Live-memory plus map/repack closure. | `Repack`/`RepackCollision` close here. |
| W10 | 6 | Update subsystem after HTTP/process adapter primitives exist. | Preserve C# async/cancellation/failure timing. |
| W11 | 14 | Portable launcher/logging/input-mapping/network-vote late closure. | Launcher/UI macro-SCC remains open. |
| W12 | 26 | Pause/window/GUI and late render partial closure. | `Scene` and `PlayerEntity` close here; launcher/UI closure must pass. |
| W13 | 9 | Test/oracle consumers and integration validation harnesses. | Tests never authorize production-only helpers. |
| W14 | 1 | `Mods/ModEntry.cs`. | Penultimate integration unit. |
| W15 | 1 | `Program.cs`. | True final process-entry integration unit. |
| **Total** | **302** |  |  |

Global micro-order inside a wave:

1. freeze canonical declaration owners and exact value/layout declarations;
2. add only necessary forward declarations;
3. implement independent contributor bodies;
4. implement bodies that require other contributor members;
5. link the complete local SCC with no placeholders;
6. run applicable exact/synthetic/differential checks;
7. only then record the evidence level actually achieved.

Engine core W4-W8 remains a macro-SCC containing Read/model/formats, Scene partials, entities/enemies, PlayerEntity partials, GameState/Menu/SceneSetup, Metadata contributors, gameplay network/session/server simulation, sound, renderer/movie/preview, and image/capture consumers. Earlier declaration readiness must not be confused with closure.

Network micro-order remains: exact protocol/config/packet declarations -> transport/probe -> demo/session/server/game-state consumers -> late vote/end-screen/pointer integration. Wire bytes/layout are validated separately from session behavior.

Live-memory/map/repack W9 remains a closure: exact memory wrapper/layout declarations -> Memory/MemoryClasses -> canonical Repack declarations -> mapgen/repack contributor bodies -> binary round-trip/layout validation.

W11-W12 launcher/UI remains a late closure because of reverse edges among LauncherPrefs, WindowMode, PauseMenu, DebugLog, GamepadMappings, LogShare, ThumbnailLog, GUI/Text launcher state, GameFiles, update, match/network state, and MapVote/end-screen/pointer layout.

W14/W15 are terminal integration only after downstream production contracts are ready.

### 7.2 NcsfPlay N1-N7

| Band | Files | Prerequisites / closure purpose |
|---:|---:|---|
| N1 | 6 | Foundational common helpers plus header/record/value primitives with no player/stream ownership. |
| N2 | 10 | FAT/INFO/SYMB section records, instrument structure, and SWAV after N1 declarations. |
| N3 | 4 | SWAR/SBNK/SSEQ and finally SDAT, after their section/file primitives exist. |
| N4 | 2 | Tag collection then NCSF PSF/container helpers; `NCSF.cs` directly consumes `TagList`. |
| N5 | 3 | Core sequencer `Channel`, `Track`, `Player` after the full low-level NC model is available. |
| N6 | 6 | NCSF player wrapper/file/stream layer after N3-N5; stream behavior consumes SDAT and the sequencer. |
| N7 | 5 | ReplayGain data/filter/gain layer, last because it is playback-analysis policy rather than a low-level NC prerequisite. |
| **Total** | **36** |  |

NcsfPlay closure notes:

- `SDAT.cs` owns the aggregate that reads/holds `SYMBSection`, `INFOSection`, `FATSection`, `SSEQ`, `SBNK`, `SWAR`, and player-info state. It therefore comes after all of those low-level definitions.
- `SWAV`, `SWAR`, `SBNK`, `SBNKInstrument`, `SSEQ`, INFO/FAT/SYMB structures are file-format behavior and must preserve exact binary offsets, counts, signedness, endian rules, exception/assert behavior, and object relationships.
- `NCSF.cs` uses zlib/CRC32/tag encoding and `TagList`; a Native compression/hash wrapper is a mechanical library seam, not an alternate file format.
- `Player/NCSFPlayerStream.cs` consumes `NCSFFile`, `SDAT`, and the sequencer `Player`; its seek/read/length/fade/silence/sample-generation behavior closes only after those providers are exact.
- ReplayGain constants, coefficient tables, sample-rate support, histogram/filter arithmetic, gain/peak calculations, and exception behavior require numeric differential checks; do not substitute a different replay-gain implementation simply because it is standards-compatible.
- `NcsfPlay.Native` remains a standalone library owner. MphRead and Android link it; they do not absorb it.

### 7.3 MphRead.Android A1-A6

| Band | Files | Prerequisites / closure purpose |
|---:|---:|---|
| A1 | 5 | Leaf platform mechanics: console, PNG, maps/resource staging, log sharing, APK install. Shared MphRead declarations already available. |
| A2 | 4 | Android keyboard/mouse/gamepad/touch-control state plus update installer after shared input/update contracts and A1 installer mechanics. `TouchControls` is available before its Android view/GL-thread consumers. |
| A3 | 2 | Offscreen GL then preview rendering core after shared GLES/capture/thumbnail contracts and Android input/PNG. |
| A4 | 2 | Preview service workers then thumbnail host/worker coordinator; service owns worker types consumed by the coordinator. |
| A5 | 3 | Android match builder, touch overlay view, and GameView render/input host after shared Scene/network/match contracts plus A2 touch/input state and GL platform seams. |
| A6 | 2 | AndroidApp front-screen lifetime then MainActivity lifecycle composition; `MainActivity` is the final Android-local integration unit. |
| **Total** | **18** |  |

Android closure notes:

- `AndroidInput.cs` creates/mechanically drives the OpenTK keyboard/mouse state that shared input code consumes; it must not fork `PlayerEntity.ProcessAllInput` policy.
- `GamepadBridge.cs` translates Android key/motion events into shared `GamepadInput.State`; shared mapping/dead-zone/action policy remains in MphRead.
- `TouchControls.cs` is deliberately free of Android framework types. It owns the synchronized touch-control state, layout, held actions, aim deltas, taps, swipe boost/double-tap behavior, and visibility rules consumed by the Android head, and it consults shared `MphRead.Mods.Input.TouchSettings`. It therefore belongs in A2 before its `TouchOverlayView` and `GameView` consumers; its behavior must be translated from C# rather than replaced by a Native-only input policy.
- `TouchOverlayView.cs` is an Android `View` adapter over `TouchControls`: it lays out/draws the controls and forwards Android `MotionEvent` down/move/up/cancel events into the A2 state. It belongs in A5 with the Android match/render/input view host and does not own shared gameplay policy.
- `OffscreenGl.cs` is GL context mechanics. `PreviewRun.cs` assumes a current context and uses shared `EsBindings`, `GlEs`, Scene, capture, thumbnail, and audio-silencing behavior.
- `PreviewService.cs` depends on `AndroidPng`, `OffscreenGl`, and `PreviewRun` and defines the worker service types. `AndroidThumbnails.cs` depends on those worker types and coordinates them; therefore service precedes coordinator.
- `AndroidMatch.cs` translates a `LaunchPlan` into a shared Scene without inventing a second gameplay policy; it reuses shared network/demo/adventure/match contracts and calls `AndroidMaps.EnsureBuilt`.
- `GameView.cs` owns Android surface/EGL/render-thread/input delivery mechanics and consumes `TouchControls`, `AndroidInput`, `GamepadBridge`, shared Scene/render/chat/end-screen behavior, and match callbacks.
- `AndroidApp.cs` uses the shared `HomeView`, launcher preferences, game settings, files, thumbnail contracts, and delegates actual match launch to `MainActivity`.
- `MainActivity.cs` composes console, writable-root selection, maps, thumbnails, update installer, log sharing, PNG writer, launcher view, `TouchControls`, `TouchOverlayView`, match view, preview workers, lifecycle and display/input hooks. It is intentionally last among local Android files.
- Android runtime/device parity cannot be inferred from desktop compilation. EGL/surface lifecycle, activity/service behavior, touch overlay layout/rendering, touch/gamepad delivery, installer/share intents, storage roots, orientation/display changes, preview worker processes, and package resources require Android-specific build and device/emulator gates.

## 8. Exact per-file assignment

Every physical C# source file in the three reviewed source trees appears exactly once in the assignment tables below.

### 8.1 `src/MphRead` -> `src/MphRead.Native` (302)

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

### 8.2 `src/NcsfPlay` -> `src/NcsfPlay.Native` (36)

| Band | C# source |
|---:|---|
| N1 | `src/NcsfPlay/Common.cs` |
| N1 | `src/NcsfPlay/NC/NDSStandardHeader.cs` |
| N1 | `src/NcsfPlay/NC/FATRecord.cs` |
| N1 | `src/NcsfPlay/NC/INFOEntry.cs` |
| N1 | `src/NcsfPlay/NC/SBNKInstrumentEntry.cs` |
| N1 | `src/NcsfPlay/NC/SYMBRecord.cs` |
| N2 | `src/NcsfPlay/NC/FATSection.cs` |
| N2 | `src/NcsfPlay/NC/INFOEntryBANK.cs` |
| N2 | `src/NcsfPlay/NC/INFOEntryPLAYER.cs` |
| N2 | `src/NcsfPlay/NC/INFOEntrySEQ.cs` |
| N2 | `src/NcsfPlay/NC/INFOEntryWAVEARC.cs` |
| N2 | `src/NcsfPlay/NC/INFORecord.cs` |
| N2 | `src/NcsfPlay/NC/INFOSection.cs` |
| N2 | `src/NcsfPlay/NC/SBNKInstrument.cs` |
| N2 | `src/NcsfPlay/NC/SYMBSection.cs` |
| N2 | `src/NcsfPlay/NC/SWAV.cs` |
| N3 | `src/NcsfPlay/NC/SWAR.cs` |
| N3 | `src/NcsfPlay/NC/SBNK.cs` |
| N3 | `src/NcsfPlay/NC/SSEQ.cs` |
| N3 | `src/NcsfPlay/NC/SDAT.cs` |
| N4 | `src/NcsfPlay/TagList.cs` |
| N4 | `src/NcsfPlay/NCSF.cs` |
| N5 | `src/NcsfPlay/Channel.cs` |
| N5 | `src/NcsfPlay/Track.cs` |
| N5 | `src/NcsfPlay/Player.cs` |
| N6 | `src/NcsfPlay/Player/SWAVWrapper.cs` |
| N6 | `src/NcsfPlay/Player/Channel.cs` |
| N6 | `src/NcsfPlay/Player/Track.cs` |
| N6 | `src/NcsfPlay/Player/Player.cs` |
| N6 | `src/NcsfPlay/Player/NCSFFile.cs` |
| N6 | `src/NcsfPlay/Player/NCSFPlayerStream.cs` |
| N7 | `src/NcsfPlay/ReplayGain/FrequencyInfo.cs` |
| N7 | `src/NcsfPlay/ReplayGain/GainData.cs` |
| N7 | `src/NcsfPlay/ReplayGain/AlbumGain.cs` |
| N7 | `src/NcsfPlay/ReplayGain/TrackGain.cs` |
| N7 | `src/NcsfPlay/ReplayGain/ReplayGain.cs` |

### 8.3 local `src/MphRead.Android` -> `src/MphRead.Native.Android` (18)

| Band | C# source |
|---:|---|
| A1 | `src/MphRead.Android/AndroidConsole.cs` |
| A1 | `src/MphRead.Android/AndroidPng.cs` |
| A1 | `src/MphRead.Android/AndroidMaps.cs` |
| A1 | `src/MphRead.Android/AndroidLogShare.cs` |
| A1 | `src/MphRead.Android/ApkInstaller.cs` |
| A2 | `src/MphRead.Android/AndroidInput.cs` |
| A2 | `src/MphRead.Android/GamepadBridge.cs` |
| A2 | `src/MphRead.Android/TouchControls.cs` |
| A2 | `src/MphRead.Android/AndroidUpdateInstaller.cs` |
| A3 | `src/MphRead.Android/OffscreenGl.cs` |
| A3 | `src/MphRead.Android/PreviewRun.cs` |
| A4 | `src/MphRead.Android/PreviewService.cs` |
| A4 | `src/MphRead.Android/AndroidThumbnails.cs` |
| A5 | `src/MphRead.Android/AndroidMatch.cs` |
| A5 | `src/MphRead.Android/TouchOverlayView.cs` |
| A5 | `src/MphRead.Android/GameView.cs` |
| A6 | `src/MphRead.Android/AndroidApp.cs` |
| A6 | `src/MphRead.Android/MainActivity.cs` |

## 9. Live status and special-case handling

Status terminology in this section is intentionally narrower than “done”. A file may be implemented and audited while project/device/CI gates remain external.

| C# source | Live structural state | Preserved parity/review status | Remaining evidence notes |
|---|---|---|---|
| `src/MphRead/Mods/Chat/ChatFont.cs` | matching Native pair exists | **Completed/audited file-level parity evidence retained**; implementation commit `36b70de7cfc95e0d7f7c744b4a9e25f245df2134` | Do not move or re-audit merely because this document was expanded. Repository-wide build/runtime/differential/CI are separate gates. |
| `src/MphRead/Mods/Input/StylusZone.cs` | matching Native pair exists | **Completed/audited file-level parity evidence retained**; implementation commit `f8ec61db336916027ae6234d7f4e18337e846694` | Do not move or re-audit merely because this document was expanded. Repository-wide build/runtime/differential/CI are separate gates. |
| `src/MphRead/Formats/RawFormats.cs` | matching Native pair exists; reviewed parent contains its strict-parity-fix history | **Not marked complete by this plan** | Pair presence and a fix commit do not substitute for a final accepted strict parity review plus applicable build/differential evidence. Remains W4. |
| `src/MphRead/Messaging.cs` | matching Native pair exists; contributes to `Scene` | **Not marked complete by this plan** | Its own audit remains distinct from aggregate `Scene` closure, which remains open through W12 contributors. Remains W4. |
| `src/MphRead/Mods/Launcher/Portable/SetupProgress.cs` | matching pair now at the correct relative path | prior relocation blocker **resolved structurally** | Behavioral parity status is whatever accepted file review evidence establishes; path correction alone is not parity. |
| `src/MphRead/Mods/ModEntry.cs` | matching Native pair exists | terminal integration remains W14 | Pair existence does not move it ahead of downstream closures. |
| `src/MphRead/Program.cs` | matching Native pair exists | terminal integration remains W15 and final | Pair existence does not make startup/dispatch parity complete early. |
| all NcsfPlay C# files | `src/NcsfPlay.Native` absent | not implemented in the target tree | Create only the per-file pairs/build mechanics required by the library boundary; no invented policy. |
| all 18 Android-local C# files | `src/MphRead.Native.Android` absent | not implemented in the target tree | Create local pairs only after shared MphRead/NcsfPlay contracts needed by each band are available. |

Additional special cases retained from the original plan:

- `PlayerEntityIconBounds` is a separate C# partial contributor and must remain a separate Native pair even though it contributes to canonical `PlayerEntity`.
- Existing Scene/preview/entity/enemy counterparts are not automatically complete simply because a class declaration or body exists.
- Partial-type contributor acceptance must distinguish the contributor’s own file parity from aggregate closure.
- External/backend conditional paths that cannot run on the current machine remain explicit external gates rather than being silently treated as passed.

## 10. Validation and evidence gates

### 10.1 Evidence dimensions

Record these dimensions independently for each file/aggregate. Do not compress them into a single unchecked “done” flag.

| Code | Evidence dimension | What it proves | What it does not prove |
|---|---|---|---|
| I | Implementation | The exact target pair exists and implements the C# surface/body. | Semantic parity, build, runtime, differential, CI. |
| S | Focused synthetic/oracle checks | Deterministic targeted cases for the file/algorithm pass. | Repository linkage, real platform/device lifecycle, broad integration. |
| B | Repository build/link | The relevant Native project configuration compiles and links with real dependencies. | Runtime correctness or C# equivalence. |
| R | Runtime/device | Real executable/library/platform path behaves correctly on applicable host/device. | Exhaustive C# equivalence by itself. |
| D | Differential | Same inputs/state compared against the authoritative C# implementation produce equivalent observable results. | Every untested platform branch. |
| C | CI | Required automated configurations/checks pass at the reviewed commit. | Uncovered runtime/device behavior. |

A strict file audit may establish file-level parity reasoning even when B/R/D/C evidence is unavailable; that distinction must be written down rather than overstated.

### 10.2 Gate 0: inventory and ownership

Before implementing a file:

- refresh `develop2`;
- verify the authoritative C# path still exists and has not changed since any preserved audit evidence;
- verify which Native project owns it;
- verify the same-relative-path `.hpp/.cpp` destination;
- inspect only direct callers/contracts/dependencies needed for exact behavior;
- for Android shared MphRead sources, do not create a second Android-local copy of the shared implementation.

### 10.3 Gate 1: structural/API parity

Verify every relevant public/internal/private member, nested type, enum/constant value, overload, default, nullability/reference identity, constructor, static field/property, accessibility requirement, collection order, declaration order when observable, and conditional compilation member.

### 10.4 Gate 2: language/runtime semantic parity

Verify C#-specific string/culture/numeric/collection/span/default/nullable/reference/exception/static-initialization semantics. Use thin compatibility mechanisms only where the language/runtime differs.

### 10.5 Gate 3: binary/protocol/file parity

For parsers, formats, network, compression, NCSF/SDAT, map/repack, serialization, and stream code verify exact byte layout, endian rules, offsets/counts, checksums/hashes, stream positions, truncation/boundary behavior, deterministic ordering, and exception behavior.

NcsfPlay requires focused binary fixtures for NDS headers, FAT/INFO/SYMB records, SWAV/SWAR/SBNK/SSEQ/SDAT and NCSF PSF/tag/zlib/CRC behavior before playback layers are accepted.

### 10.6 Gate 4: lifetime/concurrency/resource parity

Verify static initialization, ownership, callbacks, threads/tasks, cancellation, service/process lifetime, stream/device/window/GL/audio/socket resources and teardown ordering.

### 10.7 Gate 5: subsystem behavior and differential checks

Where practical, run C# and Native against the same fixture/input/state and compare observable outputs. Examples include:

- parsers/serializers and exact bytes;
- math/geometry/collision values;
- metadata/static table order and identities;
- network packets and state transitions;
- NcsfPlay sample/sequence/stream outputs and ReplayGain values;
- launcher plans/settings and command routing;
- Android adapter inputs, including touch-control state and MotionEvent translation, producing the same shared state/observable behavior as C#.

### 10.8 Gate 6: repository build/link matrix

At minimum distinguish these configurations instead of treating one compile as universal:

- MphRead Native desktop client;
- MphRead Native dedicated server;
- platform-specific desktop configurations required by translated code;
- standalone `NcsfPlay.Native` library plus its MphRead link consumer;
- `MphRead.Native.Android` with Android-compatible shared MphRead/NcsfPlay libraries and required NDK/SDK/toolchain mechanics.

The existing C# Android head is intentionally outside the ordinary desktop solution because it requires the Android workload/SDK; the Native Android gate is likewise external to a desktop-only build.

### 10.9 Gate 7: runtime/platform/device matrix

Exercise behavior that compilation cannot prove:

- desktop window/input/GL/audio/process paths on applicable OSes;
- dedicated-server startup/shutdown/network/update/process behavior;
- NcsfPlay real file playback/stream seeking/timing/audio output where applicable;
- Android activity/service lifecycle, surface/EGL context loss/recreation, pause/resume, orientation/display rotation, soft keyboard/chat, touch-control layout/overlay drawing, multi-touch input, gamepad, storage root selection, PNG capture, preview workers, install/share intents, update flow, and resources on emulator/device.

### 10.10 Gate 8: terminal entry/integration

MphRead terminal acceptance order is fixed:

1. downstream W1-W13 production contracts and required closures are ready;
2. W14 `Mods/ModEntry.cs` is differentially matched;
3. W15 `Program.cs` is integrated last;
4. mechanical Native process hosts enter that policy without reordering it.

Android terminal acceptance is separate: A6 `MainActivity.cs` closes the Android-local lifecycle head only after shared core/library and A1-A5 adapter paths are ready. It does not replace W15.

### 10.11 Gate 9: CI

CI must be reported separately from local/focused validation. A commit is not “repository-wide complete” merely because source review or one build passed. Record which workflows/configurations actually ran and which did not.

## 11. One-file-at-a-time execution contract

For each scheduled C# row:

1. refresh live `develop2` and inspect the complete authoritative C# file;
2. inspect only the direct C# contracts/callers and existing Native interfaces necessary for exact behavior;
3. confirm the file’s band prerequisites and partial-type declaration owner;
4. create or correct only its colocated `.hpp/.cpp` pair in the correct Native project, except for unavoidable build/platform mechanical adapters;
5. reproduce every observable member, state, initialization, default, ordering, algorithm, format/layout, exception, lifetime and conditional behavior from C#;
6. run focused checks appropriate to that file;
7. run the narrowest real repository build/link configuration that exercises it when available;
8. run runtime/device and C# differential checks where applicable/available;
9. record CI separately;
10. do not promote the file or aggregate past the strongest evidence actually obtained;
11. do not re-open an already accepted unchanged file merely to make unrelated progress unless a dependency/contract change invalidates its evidence.

Parallel work is safe only for independent leaves within the same currently-open dependency frontier. Do not have two workers edit the same canonical partial-type declaration at once, and do not let a consumer invent a temporary provider API that differs from C#.

## 12. Completion criteria

The migration is not complete until all of the following are true:

- all **356** physical C# files have exactly one correctly owned colocated Native pair;
- the 302 MphRead rows preserve W1-W15 order with `Mods/ModEntry.cs` W14 and `Program.cs` W15 final;
- all 36 NcsfPlay rows close from low-level NC structures through SDAT/NCSF, sequencer/player/stream, and ReplayGain without library-policy invention;
- all 18 Android-local rows close through the separate `MphRead.Native.Android` owner while consuming, not duplicating, shared `MphRead.Native` and `NcsfPlay.Native` code;
- `TouchControls.cs` and `TouchOverlayView.cs` each retain their own `MphRead.Native.Android` pair and remain ordered by their actual C# dependency, with `TouchControls` preceding the overlay/view host that consumes it;
- all partial C# aggregates have one canonical Native declaration owner and every contributing file retains its own pair/body ownership;
- no generic include-directory relocation or unmatched aggregation/policy source has been introduced;
- no Native-only main/WinMain/service/Android bootstrap has replaced C# dispatch/lifecycle policy;
- applicable structural, semantic, binary, lifetime, build/link, runtime/device, differential, and CI gates are individually recorded and satisfied to the required project release standard;
- no file is marked complete merely because its pair exists or a synthetic check passes;
- unresolved external platform/device/CI gates remain visible until actually exercised.

A documentation commit updating this plan does **not** establish implementation completion for any source file beyond preserved accepted review evidence already recorded above.
