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

This table is ordered for **Native authoring**, not alphabetically. `Order` is the body-implementation order. `Predeclare by` records the earliest wave at which the C++ header surface from that source file must already exist because an earlier body refers to one of its types. This prevents false reordering caused by C# cycles such as `Scene`/entities, `PlayerEntity`/entities, renderer enums used by gameplay, and `ProgramException` used long before `Program.Main` is implemented. A shared `SCC` tag means those rows are mutually dependent at body level; do not pretend that a strict linear order exists inside that component. Freeze declarations first, then implement the listed bodies in the displayed order.

| Order | Predeclare by | SCC | C# source |
|---:|---:|---|---|
| W1.01 | — |  | `src/MphRead/Formats/Enums.cs` |
| W1.02 | — |  | `src/MphRead/Formats/Frontend.cs` |
| W1.03 | — |  | `src/MphRead/Mods/Branding.cs` |
| W1.04 | — |  | `src/MphRead/Mods/Chat/ChatFont.cs` |
| W1.05 | — |  | `src/MphRead/Mods/ConsoleWindow.cs` |
| W1.06 | — |  | `src/MphRead/Mods/Credits.cs` |
| W1.07 | — |  | `src/MphRead/Mods/Headless.cs` |
| W1.08 | — |  | `src/MphRead/Mods/Input/StylusZone.cs` |
| W1.09 | — |  | `src/MphRead/Mods/Input/TouchSettings.cs` |
| W1.10 | — |  | `src/MphRead/Mods/Launcher/Portable/LaunchPlan.cs` |
| W1.11 | — |  | `src/MphRead/Mods/Launcher/Portable/SetupProgress.cs` |
| W1.12 | — |  | `src/MphRead/Mods/MapGen/MapTexturePack.cs` |
| W1.13 | — |  | `src/MphRead/Mods/Render/Crosshair.cs` |
| W1.14 | — |  | `src/MphRead/Mods/Update/BuildVersion.cs` |
| W1.15 | — |  | `src/MphRead/Shaders.cs` |
| W1.16 | — |  | `src/MphRead/Utility/Console.cs` |
| W1.17 | — |  | `src/MphRead/Utility/Output.cs` |
| W1.18 | — |  | `src/MphRead/Utility/Rng.cs` |
| W2.01 | — |  | `src/MphRead/Formats/Culling.cs` |
| W2.02 | — |  | `src/MphRead/Formats/Types.cs` |
| W2.03 | — |  | `src/MphRead/MemoryArrays.cs` |
| W2.04 | — |  | `src/MphRead/Mods/Input/GamepadLayout.cs` |
| W2.05 | — |  | `src/MphRead/Mods/Input/GamepadState.cs` |
| W2.06 | — |  | `src/MphRead/Mods/Input/PadBindings.cs` |
| W2.07 | — |  | `src/MphRead/Mods/Input/SyntheticInput.cs` |
| W2.08 | — |  | `src/MphRead/Mods/Network/NetLag.cs` |
| W2.09 | — |  | `src/MphRead/Mods/Render/EsBindings.cs` |
| W2.10 | — |  | `src/MphRead/Mods/Render/EsShaders.cs` |
| W2.11 | — |  | `src/MphRead/Mods/Render/FrameTiming.cs` |
| W2.12 | — |  | `src/MphRead/Mods/Render/FrameTimingCheck.cs` |
| W2.13 | — |  | `src/MphRead/Mods/Render/GlEs.cs` |
| W2.14 | — |  | `src/MphRead/Mods/RenderOptions.cs` |
| W2.15 | — |  | `src/MphRead/Mods/ShutdownSignals.cs` |
| W2.16 | — |  | `src/MphRead/Mods/Sound/AlEs.cs` |
| W2.17 | — |  | `src/MphRead/Mods/Update/SyncHttp.cs` |
| W2.18 | — |  | `src/MphRead/Utility/Compress.cs` |
| W3.01 | — |  | `src/MphRead/Features.cs` |
| W3.02 | — |  | `src/MphRead/HUD/HudInfo.cs` |
| W3.03 | — | W3-C1 | `src/MphRead/Metadata/Metadata.cs` |
| W3.04 | — | W3-C1 | `src/MphRead/Metadata/Rooms.cs` |
| W3.05 | — | W3-C2 | `src/MphRead/Export/Collada.cs` |
| W3.06 | — | W3-C2 | `src/MphRead/Export/Scripting.cs` |
| W3.07 | — |  | `src/MphRead/Metadata/FrontendMeta.cs` |
| W3.08 | — |  | `src/MphRead/Metadata/SoundMeta.cs` |
| W3.09 | — |  | `src/MphRead/Strings.cs` |
| W4.01 | — | W4-C1 | `src/MphRead/Entities/EntityBase.cs` |
| W4.02 | — | W4-C1 | `src/MphRead/Entities/LightSourceEntity.cs` |
| W4.03 | — | W4-C1 | `src/MphRead/Entities/PlayerSpawnEntity.cs` |
| W4.04 | — | W4-C1 | `src/MphRead/Entities/PointModuleEntity.cs` |
| W4.05 | — | W4-C1 | `src/MphRead/Formats/Collision.cs` |
| W4.06 | — | W4-C1 | `src/MphRead/Formats/Effects.cs` |
| W4.07 | — | W4-C1 | `src/MphRead/Formats/Entity.cs` |
| W4.08 | — | W4-C1 | `src/MphRead/Formats/EntityEnemy.cs` |
| W4.09 | W1 | W4-C1 | `src/MphRead/Formats/Formats.cs` |
| W4.10 | W3 | W4-C1 | `src/MphRead/Formats/Model.cs` |
| W4.11 | W3 | W4-C1 | `src/MphRead/Formats/RawFormats.cs` |
| W4.12 | — | W4-C1 | `src/MphRead/Messaging.cs` |
| W4.13 | W1 | W4-C1 | `src/MphRead/Read.cs` |
| W4.14 | W3 | W4-C1 | `src/MphRead/Scene.cs` |
| W4.15 | — | W4-C1 | `src/MphRead/Utility/Archive.cs` |
| W4.16 | — |  | `src/MphRead/Entities/Players/DynamicLightEntity.cs` |
| W4.17 | — |  | `src/MphRead/Formats/CollisionDetection.cs` |
| W4.18 | — |  | `src/MphRead/Formats/EntityClass.cs` |
| W4.19 | — |  | `src/MphRead/Formats/NodeData.cs` |
| W4.20 | — |  | `src/MphRead/Utility/Analyzer.cs` |
| W4.21 | — |  | `src/MphRead/Utility/Extract.cs` |
| W4.22 | — |  | `src/MphRead/Utility/Parser.cs` |
| W5.01 | — | W5-C1 | `src/MphRead/Formats/FhSound.cs` |
| W5.02 | — | W5-C1 | `src/MphRead/Formats/Sound.cs` |
| W5.03 | — | W5-C1 | `src/MphRead/Sound/Music.cs` |
| W5.04 | W4 | W5-C1 | `src/MphRead/Sound/Sfx.cs` |
| W5.05 | — |  | `src/MphRead/Mods/Network/MapRotation.cs` |
| W5.06 | — |  | `src/MphRead/Mods/Network/NetProtocol.cs` |
| W5.07 | — |  | `src/MphRead/Mods/Network/DemoFile.cs` |
| W5.08 | — |  | `src/MphRead/Mods/Network/DemoLibrary.cs` |
| W5.09 | — |  | `src/MphRead/Mods/Network/NetProbe.cs` |
| W5.10 | — |  | `src/MphRead/Mods/Network/NetTransport.cs` |
| W5.11 | W2 |  | `src/MphRead/Mods/Sound/SfxMixer.cs` |
| W6.01 | W4 |  | `src/MphRead/Entities/BeamEffectEntity.cs` |
| W6.02 | — |  | `src/MphRead/Entities/CamSeq/CameraSequence.cs` |
| W6.03 | W4 |  | `src/MphRead/Entities/CamSeq/CamSeqEntity.cs` |
| W6.04 | W4 |  | `src/MphRead/Entities/DoorEntity.cs` |
| W6.05 | W4 | W6-C1 | `src/MphRead/Entities/BeamProjectileEntity.cs` |
| W6.06 | W4 | W6-C1 | `src/MphRead/Entities/BombEntity.cs` |
| W6.07 | — | W6-C1 | `src/MphRead/Entities/Enemies/00_WarWasp.cs` |
| W6.08 | — | W6-C1 | `src/MphRead/Entities/Enemies/01_Zoomer.cs` |
| W6.09 | — | W6-C1 | `src/MphRead/Entities/Enemies/02_Temroid.cs` |
| W6.10 | — | W6-C1 | `src/MphRead/Entities/Enemies/03_Petrasyl1.cs` |
| W6.11 | — | W6-C1 | `src/MphRead/Entities/Enemies/04_Petrasyl2.cs` |
| W6.12 | — | W6-C1 | `src/MphRead/Entities/Enemies/05_Petrasyl3.cs` |
| W6.13 | — | W6-C1 | `src/MphRead/Entities/Enemies/06_Petrasyl4.cs` |
| W6.14 | — | W6-C1 | `src/MphRead/Entities/Enemies/10_BarbedWarWasp.cs` |
| W6.15 | — | W6-C1 | `src/MphRead/Entities/Enemies/11_Shriekbat.cs` |
| W6.16 | — | W6-C1 | `src/MphRead/Entities/Enemies/12_Geemer.cs` |
| W6.17 | — | W6-C1 | `src/MphRead/Entities/Enemies/16_Blastcap.cs` |
| W6.18 | — | W6-C1 | `src/MphRead/Entities/Enemies/18_AlimbicTurret.cs` |
| W6.19 | — | W6-C1 | `src/MphRead/Entities/Enemies/19_Cretaphid.cs` |
| W6.20 | — | W6-C1 | `src/MphRead/Entities/Enemies/20_CretaphidEye.cs` |
| W6.21 | — | W6-C1 | `src/MphRead/Entities/Enemies/21_CretaphidCrystal.cs` |
| W6.22 | — | W6-C1 | `src/MphRead/Entities/Enemies/23_PsychoBit.cs` |
| W6.23 | — | W6-C1 | `src/MphRead/Entities/Enemies/24_Gorea1A.cs` |
| W6.24 | — | W6-C1 | `src/MphRead/Entities/Enemies/25_GoreaHead.cs` |
| W6.25 | — | W6-C1 | `src/MphRead/Entities/Enemies/26_GoreaArm.cs` |
| W6.26 | — | W6-C1 | `src/MphRead/Entities/Enemies/27_GoreaLeg.cs` |
| W6.27 | — | W6-C1 | `src/MphRead/Entities/Enemies/28_Gorea1B.cs` |
| W6.28 | — | W6-C1 | `src/MphRead/Entities/Enemies/29_GoreaSealSphere1.cs` |
| W6.29 | — | W6-C1 | `src/MphRead/Entities/Enemies/30_Trocra.cs` |
| W6.30 | — | W6-C1 | `src/MphRead/Entities/Enemies/31_Gorea2.cs` |
| W6.31 | — | W6-C1 | `src/MphRead/Entities/Enemies/32_GoreaSealSphere2.cs` |
| W6.32 | — | W6-C1 | `src/MphRead/Entities/Enemies/33_GoreaMeteor.cs` |
| W6.33 | — | W6-C1 | `src/MphRead/Entities/Enemies/35_Voldrum.cs` |
| W6.34 | — | W6-C1 | `src/MphRead/Entities/Enemies/36_Voldrum.cs` |
| W6.35 | — | W6-C1 | `src/MphRead/Entities/Enemies/37_Quadtroid.cs` |
| W6.36 | — | W6-C1 | `src/MphRead/Entities/Enemies/38_CrashPillar.cs` |
| W6.37 | — | W6-C1 | `src/MphRead/Entities/Enemies/39_FireSpawn.cs` |
| W6.38 | — | W6-C1 | `src/MphRead/Entities/Enemies/40_EnemySpawner.cs` |
| W6.39 | — | W6-C1 | `src/MphRead/Entities/Enemies/41_Slench.cs` |
| W6.40 | — | W6-C1 | `src/MphRead/Entities/Enemies/42_SlenchShield.cs` |
| W6.41 | — | W6-C1 | `src/MphRead/Entities/Enemies/43_SlenchNest.cs` |
| W6.42 | — | W6-C1 | `src/MphRead/Entities/Enemies/44_SlenchSynapse.cs` |
| W6.43 | — | W6-C1 | `src/MphRead/Entities/Enemies/45_SlenchTurret.cs` |
| W6.44 | — | W6-C1 | `src/MphRead/Entities/Enemies/46_LesserIthrak.cs` |
| W6.45 | — | W6-C1 | `src/MphRead/Entities/Enemies/47_GreaterIthrak.cs` |
| W6.46 | — | W6-C1 | `src/MphRead/Entities/Enemies/49_ForceFieldLock.cs` |
| W6.47 | — | W6-C1 | `src/MphRead/Entities/Enemies/50_HitZone.cs` |
| W6.48 | — | W6-C1 | `src/MphRead/Entities/Enemies/51_CarnivorousPlant.cs` |
| W6.49 | W4 | W6-C1 | `src/MphRead/Entities/EnemyInstanceEntity.cs` |
| W6.50 | W4 | W6-C1 | `src/MphRead/Entities/EnemySpawnEntity.cs` |
| W6.51 | W4 | W6-C1 | `src/MphRead/Entities/ForceFieldEntity.cs` |
| W6.52 | W4 | W6-C1 | `src/MphRead/Entities/ItemInstanceEntity.cs` |
| W6.53 | W4 | W6-C1 | `src/MphRead/Entities/ItemSpawnEntity.cs` |
| W6.54 | W4 | W6-C1 | `src/MphRead/Entities/PlatformEntity.cs` |
| W6.55 | W4 | W6-C1 | `src/MphRead/Entities/TriggerVolumeEntity.cs` |
| W6.56 | W4 |  | `src/MphRead/Entities/AreaVolumeEntity.cs` |
| W6.57 | W4 |  | `src/MphRead/Entities/ArtifactEntity.cs` |
| W6.58 | W4 |  | `src/MphRead/Entities/JumpPadEntity.cs` |
| W6.59 | W4 |  | `src/MphRead/Entities/MorphCameraEntity.cs` |
| W6.60 | W4 |  | `src/MphRead/Entities/NodeDefenseEntity.cs` |
| W6.61 | W4 |  | `src/MphRead/Entities/ObjectEntity.cs` |
| W6.62 | W4 |  | `src/MphRead/Entities/OctolithFlagEntity.cs` |
| W6.63 | W4 |  | `src/MphRead/Entities/FlagBaseEntity.cs` |
| W6.64 | — |  | `src/MphRead/Entities/RoomEntity.cs` |
| W6.65 | W4 |  | `src/MphRead/Entities/TeleporterEntity.cs` |
| W6.66 | — |  | `src/MphRead/Metadata/Enemies.cs` |
| W7.01 | W6 |  | `src/MphRead/Metadata/Weapons.cs` |
| W7.02 | W4 | W7-C1 | `src/MphRead/Entities/Players/HalfturretEntity.cs` |
| W7.03 | — | W7-C1 | `src/MphRead/Entities/Players/PlayerAi.cs` |
| W7.04 | W6 | W7-C1 | `src/MphRead/Entities/Players/PlayerCamera.cs` |
| W7.05 | W6 | W7-C1 | `src/MphRead/Entities/Players/PlayerDialog.cs` |
| W7.06 | W4 | W7-C1 | `src/MphRead/Entities/Players/PlayerEntity.cs` |
| W7.07 | — | W7-C1 | `src/MphRead/Entities/Players/PlayerInput.cs` |
| W7.08 | W6 | W7-C1 | `src/MphRead/Formats/AiPersonality.cs` |
| W7.09 | W4 | W7-C1 | `src/MphRead/GameState.cs` |
| W7.10 | W5 | W7-C1 | `src/MphRead/Menu.cs` |
| W7.11 | W6 | W7-C1 | `src/MphRead/SceneSetup.cs` |
| W7.12 | W4 |  | `src/MphRead/Entities/Players/PlayerCollision.cs` |
| W7.13 | — |  | `src/MphRead/Entities/Players/PlayerDraw.cs` |
| W7.14 | — |  | `src/MphRead/Entities/Players/PlayerPause.cs` |
| W7.15 | — |  | `src/MphRead/Entities/Players/PlayerProcess.cs` |
| W7.16 | — |  | `src/MphRead/Entities/Players/PlayerScan.cs` |
| W7.17 | — |  | `src/MphRead/Entities/Players/PlayerSound.cs` |
| W7.18 | — |  | `src/MphRead/Metadata/Player.cs` |
| W7.19 | — |  | `src/MphRead/Mods/GameSettings.cs` |
| W7.20 | — |  | `src/MphRead/Mods/HunterSuits.cs` |
| W7.21 | — |  | `src/MphRead/Mods/InputSettings.cs` |
| W7.22 | — |  | `src/MphRead/Mods/Input/GamepadInput.cs` |
| W7.23 | — |  | `src/MphRead/Mods/Network/MechanicsDump.cs` |
| W7.24 | — |  | `src/MphRead/Mods/Network/NetScoreboard.cs` |
| W7.25 | — |  | `src/MphRead/Mods/Network/NetStatus.cs` |
| W7.26 | — |  | `src/MphRead/Mods/Render/PlayerEntityAmmoClear.cs` |
| W7.27 | — |  | `src/MphRead/Mods/Render/PlayerEntityIconBounds.cs` |
| W7.28 | — |  | `src/MphRead/Entities/Players/PlayerHud.cs` |
| W7.29 | — |  | `src/MphRead/Mods/Render/PlayerEntityStylusHud.cs` |
| W7.30 | — |  | `src/MphRead/Mods/Render/PlayerEntityVoteHud.cs` |
| W7.31 | — |  | `src/MphRead/Mods/SpectatorMode.cs` |
| W7.32 | — | W7-C2 | `src/MphRead/Mods/Network/DemoClip.cs` |
| W7.33 | W4 | W7-C2 | `src/MphRead/Mods/Network/DemoPlayback.cs` |
| W7.34 | — | W7-C2 | `src/MphRead/Mods/Network/DemoRecorder.cs` |
| W7.35 | — | W7-C2 | `src/MphRead/Mods/Network/MapAudit.cs` |
| W7.36 | W6 | W7-C2 | `src/MphRead/Mods/Network/NetDamage.cs` |
| W7.37 | — | W7-C2 | `src/MphRead/Mods/Network/NetDiagnostics.cs` |
| W7.38 | — | W7-C2 | `src/MphRead/Mods/Network/NetHitPrediction.cs` |
| W7.39 | — | W7-C2 | `src/MphRead/Mods/Network/NetHooks.cs` |
| W7.40 | — | W7-C2 | `src/MphRead/Mods/Network/NetLaunch.cs` |
| W7.41 | W6 | W7-C2 | `src/MphRead/Mods/Network/NetLog.cs` |
| W7.42 | — | W7-C2 | `src/MphRead/Mods/Network/NetMatchEnd.cs` |
| W7.43 | — | W7-C2 | `src/MphRead/Mods/Network/NetMatchSync.cs` |
| W7.44 | — | W7-C2 | `src/MphRead/Mods/Network/NetPlayerBridge.cs` |
| W7.45 | — | W7-C2 | `src/MphRead/Mods/Network/NetPlayerSetup.cs` |
| W7.46 | — | W7-C2 | `src/MphRead/Mods/Network/NetRoomChange.cs` |
| W7.47 | — | W7-C2 | `src/MphRead/Mods/Network/NetSession.cs` |
| W7.48 | — | W7-C2 | `src/MphRead/Mods/Network/NetSlotManager.cs` |
| W7.49 | — | W7-C2 | `src/MphRead/Mods/Network/NetTestScript.cs` |
| W7.50 | — | W7-C2 | `src/MphRead/Mods/Network/NetUnlagged.cs` |
| W7.51 | — | W7-C2 | `src/MphRead/Mods/Network/PlayerColors.cs` |
| W7.52 | — | W7-C2 | `src/MphRead/Mods/RespawnChoice.cs` |
| W7.53 | — |  | `src/MphRead/Mods/Chat/ChatBox.cs` |
| W7.54 | — |  | `src/MphRead/Mods/Chat/PlayerEntityChatHud.cs` |
| W7.55 | — |  | `src/MphRead/Mods/Network/DemoInfo.cs` |
| W7.56 | — |  | `src/MphRead/Mods/Network/NetConnectCommand.cs` |
| W7.57 | — |  | `src/MphRead/Mods/Network/NetFeatureCheck.cs` |
| W7.58 | — |  | `src/MphRead/Mods/Network/NetCheckClient.cs` |
| W7.59 | — |  | `src/MphRead/Mods/Network/PlayerEntityNetAim.cs` |
| W7.60 | — |  | `src/MphRead/Mods/Network/PlayerEntityNetHud.cs` |
| W7.61 | — |  | `src/MphRead/Mods/Network/ServerSim.cs` |
| W7.62 | — | W7-C3 | `src/MphRead/Mods/Network/DedicatedServer.cs` |
| W7.63 | — | W7-C3 | `src/MphRead/Mods/Network/NetMaster.cs` |
| W7.64 | — |  | `src/MphRead/Mods/Network/NetHostSession.cs` |
| W7.65 | — |  | `src/MphRead/Mods/Network/ServerSimCheck.cs` |
| W7.66 | — |  | `src/MphRead/Mods/Network/WeaponDps.cs` |
| W7.67 | — |  | `src/MphRead/Mods/WorldEvents.cs` |
| W8.01 | W4 |  | `src/MphRead/Export/Images.cs` |
| W8.02 | W6 | W8-C1 | `src/MphRead/Formats/Movie.cs` |
| W8.03 | W4 | W8-C1 | `src/MphRead/Renderer.cs` |
| W8.04 | W4 | W8-C1 | `src/MphRead/Selection.cs` |
| W8.05 | — |  | `src/MphRead/Mods/Render/HunterPreview.cs` |
| W8.06 | — |  | `src/MphRead/Mods/Render/PreviewCamera.cs` |
| W8.07 | — |  | `src/MphRead/Mods/Render/SmoothHudIcon.cs` |
| W8.08 | — |  | `src/MphRead/Mods/Render/PlayerEntityProHud.cs` |
| W8.09 | W2 |  | `src/MphRead/Mods/ScreenCapture.cs` |
| W8.10 | — |  | `src/MphRead/Mods/ThumbnailMode.cs` |
| W8.11 | — | W8-C2 | `src/MphRead/Mods/ThumbnailBatch.cs` |
| W8.12 | — | W8-C2 | `src/MphRead/Mods/ThumbnailCapture.cs` |
| W8.13 | — | W8-C2 | `src/MphRead/Mods/ThumbnailGenerator.cs` |
| W8.14 | — |  | `src/MphRead/Mods/ThumbnailHost.cs` |
| W9.01 | W2 | W9-C1 | `src/MphRead/Memory.cs` |
| W9.02 | W2 | W9-C1 | `src/MphRead/MemoryClasses.cs` |
| W9.03 | — |  | `src/MphRead/Mods/MapGen/Q3Bsp.cs` |
| W9.04 | — |  | `src/MphRead/Mods/MapGen/MapReport.cs` |
| W9.05 | — |  | `src/MphRead/Mods/MapGen/MapTextureBake.cs` |
| W9.06 | W4 | W9-C2 | `src/MphRead/Utility/RepackEntity.cs` |
| W9.07 | — | W9-C2 | `src/MphRead/Utility/RepackModel.cs` |
| W9.08 | — |  | `src/MphRead/Utility/RepackCollision.cs` |
| W9.09 | — |  | `src/MphRead/Mods/MapGen/MapCollisionPacker.cs` |
| W9.10 | — | W9-C3 | `src/MphRead/Mods/MapGen/BuiltMap.cs` |
| W9.11 | — | W9-C3 | `src/MphRead/Mods/MapGen/CustomRooms.cs` |
| W9.12 | — | W9-C3 | `src/MphRead/Mods/MapGen/MapBuilder.cs` |
| W9.13 | — | W9-C3 | `src/MphRead/Mods/MapGen/MapBundle.cs` |
| W9.14 | — | W9-C3 | `src/MphRead/Mods/MapGen/MapDefinition.cs` |
| W9.15 | — | W9-C3 | `src/MphRead/Mods/MapGen/MapNodePacker.cs` |
| W9.16 | — | W9-C3 | `src/MphRead/Mods/MapGen/MapPacker.cs` |
| W9.17 | — | W9-C3 | `src/MphRead/Mods/MapGen/Q3Import.cs` |
| W9.18 | — | W9-C3 | `src/MphRead/Mods/MapGen/RawStructs.cs` |
| W9.19 | — |  | `src/MphRead/Mods/MapGen/Q3Convert.cs` |
| W9.20 | — |  | `src/MphRead/Mods/MapGen/RepackAccess.cs` |
| W10.01 | — |  | `src/MphRead/Mods/Update/UpdateCheck.cs` |
| W10.02 | — |  | `src/MphRead/Mods/Update/UpdateDownload.cs` |
| W10.03 | — |  | `src/MphRead/Mods/Update/DesktopUpdate.cs` |
| W10.04 | — |  | `src/MphRead/Mods/Update/UpdateInstall.cs` |
| W10.05 | — |  | `src/MphRead/Mods/Update/Updater.cs` |
| W10.06 | — |  | `src/MphRead/Mods/Update/ServerUpdate.cs` |
| W11.01 | — |  | `src/MphRead/Mods/Input/GamepadMappings.cs` |
| W11.02 | — |  | `src/MphRead/Mods/Input/GamepadDesktop.cs` |
| W11.03 | — |  | `src/MphRead/Mods/Input/GamepadProbe.cs` |
| W11.04 | — |  | `src/MphRead/Mods/Launcher/Portable/AdventureSave.cs` |
| W11.05 | — |  | `src/MphRead/Mods/Launcher/Portable/GameFiles.cs` |
| W11.06 | — |  | `src/MphRead/Mods/Launcher/Portable/LauncherPrefs.cs` |
| W11.07 | W2 |  | `src/MphRead/Mods/DebugLog.cs` |
| W11.08 | W7 |  | `src/MphRead/Mods/EndScreen.cs` |
| W11.09 | — |  | `src/MphRead/Mods/Input/PointerInput.cs` |
| W11.10 | — |  | `src/MphRead/Mods/Launcher/Portable/MatchStart.cs` |
| W11.11 | — |  | `src/MphRead/Mods/Launcher/Portable/TextLauncher.cs` |
| W11.12 | — |  | `src/MphRead/Mods/LogShare.cs` |
| W11.13 | W7 |  | `src/MphRead/Mods/Network/MapVote.cs` |
| W11.14 | W8 |  | `src/MphRead/Mods/ThumbnailLog.cs` |
| W12.01 | — |  | `src/MphRead/Mods/Launcher/Gui/GuiTheme.cs` |
| W12.02 | — |  | `src/MphRead/Mods/Launcher/Gui/CrosshairPreview.cs` |
| W12.03 | — |  | `src/MphRead/Mods/Launcher/Gui/ProgressRow.cs` |
| W12.04 | — |  | `src/MphRead/Mods/Launcher/Gui/Rows.cs` |
| W12.05 | — |  | `src/MphRead/Mods/Launcher/Gui/ServerRow.cs` |
| W12.06 | — |  | `src/MphRead/Mods/Launcher/Gui/SplashView.cs` |
| W12.07 | — |  | `src/MphRead/Mods/Launcher/Gui/TrackedText.cs` |
| W12.08 | — |  | `src/MphRead/Mods/Launcher/Gui/KeyRow.cs` |
| W12.09 | — |  | `src/MphRead/Mods/Launcher/Gui/MenuEntry.cs` |
| W12.10 | — |  | `src/MphRead/Mods/Launcher/Gui/DemoPickerView.cs` |
| W12.11 | — |  | `src/MphRead/Mods/Launcher/Gui/MapPickerView.cs` |
| W12.12 | — |  | `src/MphRead/Mods/Launcher/Gui/PadRow.cs` |
| W12.13 | — |  | `src/MphRead/Mods/Launcher/Gui/SliderRow.cs` |
| W12.14 | — |  | `src/MphRead/Mods/Launcher/Gui/UpdateBadge.cs` |
| W12.15 | — | W12-C1 | `src/MphRead/Mods/PauseMenu.cs` |
| W12.16 | W11 | W12-C1 | `src/MphRead/Mods/WindowMode.cs` |
| W12.17 | — |  | `src/MphRead/Mods/Launcher/Gui/PauseMenuView.cs` |
| W12.18 | — |  | `src/MphRead/Mods/Launcher/Gui/SettingsView.cs` |
| W12.19 | — |  | `src/MphRead/Mods/Launcher/Gui/HomeView.cs` |
| W12.20 | — |  | `src/MphRead/Mods/Launcher/Gui/HomeWindow.cs` |
| W12.21 | — | W12-C2 | `src/MphRead/Mods/Launcher/Gui/PauseMenuWindow.cs` |
| W12.22 | — | W12-C2 | `src/MphRead/Mods/Launcher/Gui/SettingsWindow.cs` |
| W12.23 | — |  | `src/MphRead/Mods/Launcher/Gui/GuiLauncher.cs` |
| W12.24 | — |  | `src/MphRead/Mods/Launcher/Gui/UiCapture.cs` |
| W12.25 | — |  | `src/MphRead/Mods/Render/PlayerEntityEndScreen.cs` |
| W12.26 | — |  | `src/MphRead/Mods/Render/PreviewPass.cs` |
| W13.01 | — |  | `src/MphRead/Test.cs` |
| W13.02 | — |  | `src/MphRead/Testing/TestEffects.cs` |
| W13.03 | — |  | `src/MphRead/Testing/TestLogic.cs` |
| W13.04 | — |  | `src/MphRead/Testing/TestMisc.cs` |
| W13.05 | — |  | `src/MphRead/Testing/TestOverlay.cs` |
| W13.06 | — |  | `src/MphRead/Testing/TestParse.cs` |
| W13.07 | — |  | `src/MphRead/Testing/TestPlayer.cs` |
| W13.08 | — |  | `src/MphRead/Testing/TestPrint.cs` |
| W13.09 | — |  | `src/MphRead/Testing/TestWeapons.cs` |
| W14.01 | — |  | `src/MphRead/Mods/ModEntry.cs` |
| W15.01 | W1 |  | `src/MphRead/Program.cs` |

**Cross-wave declaration seams.** The following implementation files stay in their later body wave, but their named declaration surface must be frozen by the earlier wave shown. This is a compile/declaration prerequisite, not permission to move the C# behavior earlier.

| Provider source | Body wave | Header surface needed by | Referenced project types |
|---|---:|---:|---|
| `src/MphRead/Formats/Formats.cs` | W4 | W1 | `Frozen`, `GameMode`, `Paths`, `TexcoordAnimationGroup` |
| `src/MphRead/Formats/Model.cs` | W4 | W3 | `Model` |
| `src/MphRead/Formats/RawFormats.cs` | W4 | W3 | `RawStringTableEntry`, `TexcoordAnimation`, `Texture` |
| `src/MphRead/Read.cs` | W4 | W1 | `Read` |
| `src/MphRead/Scene.cs` | W4 | W3 | `Scene` |
| `src/MphRead/Mods/Sound/SfxMixer.cs` | W5 | W2 | `SfxMixer` |
| `src/MphRead/Sound/Sfx.cs` | W5 | W4 | `SoundSource` |
| `src/MphRead/Entities/AreaVolumeEntity.cs` | W6 | W4 | `AreaVolumeEntity` |
| `src/MphRead/Entities/ArtifactEntity.cs` | W6 | W4 | `ArtifactEntity` |
| `src/MphRead/Entities/BeamEffectEntity.cs` | W6 | W4 | `BeamEffectEntity` |
| `src/MphRead/Entities/BeamProjectileEntity.cs` | W6 | W4 | `BeamProjectileEntity` |
| `src/MphRead/Entities/BombEntity.cs` | W6 | W4 | `BombEntity` |
| `src/MphRead/Entities/CamSeq/CamSeqEntity.cs` | W6 | W4 | `CamSeqEntity` |
| `src/MphRead/Entities/DoorEntity.cs` | W6 | W4 | `DoorEntity` |
| `src/MphRead/Entities/EnemyInstanceEntity.cs` | W6 | W4 | `Effectiveness`, `EnemyInstanceEntity` |
| `src/MphRead/Entities/EnemySpawnEntity.cs` | W6 | W4 | `EnemySpawnEntity` |
| `src/MphRead/Entities/FlagBaseEntity.cs` | W6 | W4 | `FlagBaseEntity` |
| `src/MphRead/Entities/ForceFieldEntity.cs` | W6 | W4 | `ForceFieldEntity` |
| `src/MphRead/Entities/ItemInstanceEntity.cs` | W6 | W4 | `ItemInstanceEntity` |
| `src/MphRead/Entities/ItemSpawnEntity.cs` | W6 | W4 | `ItemSpawnEntity` |
| `src/MphRead/Entities/JumpPadEntity.cs` | W6 | W4 | `JumpPadEntity` |
| `src/MphRead/Entities/MorphCameraEntity.cs` | W6 | W4 | `MorphCameraEntity` |
| `src/MphRead/Entities/NodeDefenseEntity.cs` | W6 | W4 | `NodeDefenseEntity` |
| `src/MphRead/Entities/ObjectEntity.cs` | W6 | W4 | `ObjectEntity`, `ObjectFlags` |
| `src/MphRead/Entities/OctolithFlagEntity.cs` | W6 | W4 | `OctolithFlagEntity` |
| `src/MphRead/Entities/PlatformEntity.cs` | W6 | W4 | `PlatformEntity`, `PlatformFlags` |
| `src/MphRead/Entities/TeleporterEntity.cs` | W6 | W4 | `TeleporterEntity` |
| `src/MphRead/Entities/TriggerVolumeEntity.cs` | W6 | W4 | `TriggerFlags`, `TriggerVolumeEntity` |
| `src/MphRead/Entities/Players/HalfturretEntity.cs` | W7 | W4 | `HalfturretEntity` |
| `src/MphRead/Entities/Players/PlayerCamera.cs` | W7 | W6 | `CameraInfo` |
| `src/MphRead/Entities/Players/PlayerCollision.cs` | W7 | W4 | `DamageResult` |
| `src/MphRead/Entities/Players/PlayerDialog.cs` | W7 | W6 | `DialogType` |
| `src/MphRead/Entities/Players/PlayerEntity.cs` | W7 | W4 | `PlayerEntity` |
| `src/MphRead/Formats/AiPersonality.cs` | W7 | W6 | `AiPersonality` |
| `src/MphRead/GameState.cs` | W7 | W4 | `GameState` |
| `src/MphRead/Menu.cs` | W7 | W5 | `SoundCapability` |
| `src/MphRead/Metadata/Weapons.cs` | W7 | W6 | `EquipInfo`, `WeaponInfo` |
| `src/MphRead/Mods/Network/DemoPlayback.cs` | W7 | W4 | `DemoPlayback` |
| `src/MphRead/Mods/Network/NetDamage.cs` | W7 | W6 | `NetDamage` |
| `src/MphRead/Mods/Network/NetLog.cs` | W7 | W6 | `NetLog` |
| `src/MphRead/SceneSetup.cs` | W7 | W6 | `SceneSetup` |
| `src/MphRead/Export/Images.cs` | W8 | W4 | `Images` |
| `src/MphRead/Formats/Movie.cs` | W8 | W6 | `AfterMovie` |
| `src/MphRead/Mods/ScreenCapture.cs` | W8 | W2 | `ReadPixels`, `ScreenCapture` |
| `src/MphRead/Renderer.cs` | W8 | W4 | `AfterFade`, `CameraMode`, `CollisionType`, `RenderWindow`, `VolumeDisplay` |
| `src/MphRead/Selection.cs` | W8 | W4 | `Selection` |
| `src/MphRead/Memory.cs` | W9 | W2 | `Memory` |
| `src/MphRead/MemoryClasses.cs` | W9 | W2 | `MemoryClass` |
| `src/MphRead/Utility/RepackEntity.cs` | W9 | W4 | `Repack` |
| `src/MphRead/Mods/DebugLog.cs` | W11 | W2 | `DebugLog` |
| `src/MphRead/Mods/EndScreen.cs` | W11 | W7 | `EndScreen` |
| `src/MphRead/Mods/Network/MapVote.cs` | W11 | W7 | `MapVote` |
| `src/MphRead/Mods/ThumbnailLog.cs` | W11 | W8 | `ThumbnailLog` |
| `src/MphRead/Mods/WindowMode.cs` | W12 | W11 | `WindowStartMode` |
| `src/MphRead/Program.cs` | W15 | W1 | `Program`, `ProgramException` |

### 8.2 `src/NcsfPlay` -> `src/NcsfPlay.Native` (36)

This replaces the former coarse N1-N7 grouping with a stricter dependency order. In particular, `SBNKInstrument` now precedes `SBNKInstrumentEntry`; `SWAR` precedes `INFOEntryWAVEARC`; the mutually-referential `INFOEntryBANK`/`SBNK` and `INFOEntrySEQ`/`SSEQ` pairs are explicit SCCs; `SDAT` comes only after its FAT/INFO/SYMB/SBNK/SSEQ/SWAR closure; the core `Channel`/`Track`/`Player` runtime is treated as one SCC; and ReplayGain is ordered `FrequencyInfo -> ReplayGain -> GainData -> TrackGain -> AlbumGain`.

| Order | SCC | C# source |
|---:|---|---|
| N1.01 |  | `src/NcsfPlay/Common.cs` |
| N1.02 |  | `src/NcsfPlay/NC/FATRecord.cs` |
| N1.03 |  | `src/NcsfPlay/NC/INFOEntry.cs` |
| N1.04 |  | `src/NcsfPlay/NC/NDSStandardHeader.cs` |
| N1.05 |  | `src/NcsfPlay/NC/SBNKInstrument.cs` |
| N1.06 |  | `src/NcsfPlay/NC/SYMBRecord.cs` |
| N1.07 |  | `src/NcsfPlay/NC/SWAV.cs` |
| N2.01 |  | `src/NcsfPlay/NC/FATSection.cs` |
| N2.02 |  | `src/NcsfPlay/NC/INFOEntryPLAYER.cs` |
| N2.03 |  | `src/NcsfPlay/NC/INFORecord.cs` |
| N2.04 |  | `src/NcsfPlay/NC/SBNKInstrumentEntry.cs` |
| N2.05 |  | `src/NcsfPlay/NC/SYMBSection.cs` |
| N2.06 |  | `src/NcsfPlay/NC/SWAR.cs` |
| N3.01 |  | `src/NcsfPlay/NC/INFOEntryWAVEARC.cs` |
| N3.02 | N3-C1 | `src/NcsfPlay/NC/INFOEntryBANK.cs` |
| N3.03 | N3-C1 | `src/NcsfPlay/NC/SBNK.cs` |
| N3.04 | N3-C2 | `src/NcsfPlay/NC/INFOEntrySEQ.cs` |
| N3.05 | N3-C2 | `src/NcsfPlay/NC/SSEQ.cs` |
| N3.06 |  | `src/NcsfPlay/NC/INFOSection.cs` |
| N4.01 |  | `src/NcsfPlay/NC/SDAT.cs` |
| N5.01 |  | `src/NcsfPlay/TagList.cs` |
| N5.02 |  | `src/NcsfPlay/NCSF.cs` |
| N6.01 | N6-C1 | `src/NcsfPlay/Channel.cs` |
| N6.02 | N6-C1 | `src/NcsfPlay/Track.cs` |
| N6.03 | N6-C1 | `src/NcsfPlay/Player.cs` |
| N7.01 |  | `src/NcsfPlay/Player/Track.cs` |
| N7.02 |  | `src/NcsfPlay/Player/Player.cs` |
| N7.03 |  | `src/NcsfPlay/Player/NCSFFile.cs` |
| N7.04 | N7-C1 | `src/NcsfPlay/Player/SWAVWrapper.cs` |
| N7.05 | N7-C1 | `src/NcsfPlay/Player/Channel.cs` |
| N7.06 |  | `src/NcsfPlay/Player/NCSFPlayerStream.cs` |
| N8.01 |  | `src/NcsfPlay/ReplayGain/FrequencyInfo.cs` |
| N8.02 |  | `src/NcsfPlay/ReplayGain/ReplayGain.cs` |
| N8.03 |  | `src/NcsfPlay/ReplayGain/GainData.cs` |
| N8.04 |  | `src/NcsfPlay/ReplayGain/TrackGain.cs` |
| N8.05 |  | `src/NcsfPlay/ReplayGain/AlbumGain.cs` |

### 8.3 local `src/MphRead.Android` -> `src/MphRead.Native.Android` (18)

The Android order is rebuilt around the real local lifecycle graph. Before A4 bodies, freeze the `MainActivity` declaration surface needed by `ApkInstaller` and `AndroidThumbnails`; **do not** implement `MainActivity` early. `ApkInstaller -> AndroidUpdateInstaller`, `PreviewRun -> PreviewService`, `AndroidMatch -> GameView`, and `TouchControls -> TouchOverlayView/GameView` are then linearized. `AndroidApp` and `MainActivity` remain a terminal lifecycle SCC because each refers to the other.

| Order | Predeclare by | SCC | C# source |
|---:|---:|---|---|
| A1.01 | — |  | `src/MphRead.Android/AndroidConsole.cs` |
| A1.02 | — |  | `src/MphRead.Android/AndroidPng.cs` |
| A1.03 | — |  | `src/MphRead.Android/AndroidMaps.cs` |
| A1.04 | — |  | `src/MphRead.Android/AndroidLogShare.cs` |
| A1.05 | — |  | `src/MphRead.Android/AndroidInput.cs` |
| A1.06 | — |  | `src/MphRead.Android/GamepadBridge.cs` |
| A1.07 | — |  | `src/MphRead.Android/TouchControls.cs` |
| A1.08 | — |  | `src/MphRead.Android/OffscreenGl.cs` |
| A2.01 | — |  | `src/MphRead.Android/PreviewRun.cs` |
| A2.02 | — |  | `src/MphRead.Android/AndroidMatch.cs` |
| A2.03 | — |  | `src/MphRead.Android/TouchOverlayView.cs` |
| A3.01 | — |  | `src/MphRead.Android/PreviewService.cs` |
| A3.02 | — |  | `src/MphRead.Android/GameView.cs` |
| A4.01 | — |  | `src/MphRead.Android/ApkInstaller.cs` |
| A4.02 | — |  | `src/MphRead.Android/AndroidUpdateInstaller.cs` |
| A4.03 | — |  | `src/MphRead.Android/AndroidThumbnails.cs` |
| A5.01 | — | A5-C1 | `src/MphRead.Android/AndroidApp.cs` |
| A5.02 | A4 | A5-C1 | `src/MphRead.Android/MainActivity.cs` |
