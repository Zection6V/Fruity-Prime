# MphRead Native Dependency Review A

## Status, scope, and authority

**Repository:** `Zection6V/Fruity-Prime`  
**Branch:** `develop2`  
**Reviewed commit:** `b62fae787babaefaf6b8b2e3107cee1da7f1932c`  
**C# source authority:** `src/MphRead/**/*.cs`  
**Native target:** `src/MphRead.Native/**/*.hpp` + `src/MphRead.Native/**/*.cpp`

This is an independent dependency-closure review for the strict C# to C++20 migration. It is not an implementation patch. No C# or C++ source is changed by this review.

The C# source is the sole behavioral specification. Existing Native code is evidence of prior work, not an authority. A matching Native filename, a successful include, or a prior review label does not establish parity.

The structural target is exact one-file correspondence:

- every C# source file under `src/MphRead` receives exactly one colocated Native `.hpp/.cpp` pair at the matching relative path;
- the `.hpp` stays beside its `.cpp`;
- no generic include tree hides counterpart headers;
- no C#-unmatched support source/header is introduced merely to break cycles;
- unavoidable platform/runtime adapters are mechanical seams only and cannot add application policy.

### Snapshot verification

The generated dependency indexes were produced against `2da573aa860507eab7150dfd3a69667528721063`. The live branch head reviewed here is `b62fae787babaefaf6b8b2e3107cee1da7f1932c`. Comparing those commits shows only documentation additions; there are no changes under `src/MphRead` or `src/MphRead.Native`. Therefore the 302-source / 48-pair inventory remains exact for the reviewed head, while this document supersedes the older documents' semantic assumptions where called out below.

This review also reconciles the two additional implementation-order reviews that appeared on `develop2` while the investigation was running. They do not change the source snapshot.

---

## 1. Exact current inventory

| Item | Count |
|---|---:|
| C# files under `src/MphRead` | **302** |
| Native `.hpp` files | **48** |
| Native `.cpp` files | **48** |
| Native hpp/cpp pairs | **48** |
| Pairs at the matching relative path | **47** |
| Relocated pair | **1** |
| C# files with no Native pair | **254** |

The one relocated pair is `Mods/Launcher/Portable/SetupProgress.cs`, whose current Native files are under `Mods/Launcher/SetupProgress.*`. That placement is not valid as the final strict one-to-one target; its canonical target is `src/MphRead.Native/Mods/Launcher/Portable/SetupProgress.hpp/.cpp`.

### 1.1 Directory inventory

| Relative directory | C# | paired | relocated | missing |
|---|---:|---:|---:|---:|
| `.` | 16 | 1 | 0 | 15 |
| `Entities` | 25 | 2 | 0 | 23 |
| `Entities/CamSeq` | 2 | 0 | 0 | 2 |
| `Entities/Enemies` | 42 | 2 | 0 | 40 |
| `Entities/Players` | 14 | 0 | 0 | 14 |
| `Export` | 3 | 0 | 0 | 3 |
| `Formats` | 18 | 3 | 0 | 15 |
| `HUD` | 1 | 0 | 0 | 1 |
| `Metadata` | 7 | 0 | 0 | 7 |
| `Mods` | 25 | 11 | 0 | 14 |
| `Mods/Chat` | 3 | 0 | 0 | 3 |
| `Mods/Input` | 11 | 4 | 0 | 7 |
| `Mods/Launcher/Gui` | 22 | 4 | 0 | 18 |
| `Mods/Launcher/Portable` | 7 | 1 | 1 | 5 |
| `Mods/MapGen` | 16 | 3 | 0 | 13 |
| `Mods/Network` | 43 | 6 | 0 | 37 |
| `Mods/Render` | 16 | 5 | 0 | 11 |
| `Mods/Sound` | 2 | 0 | 0 | 2 |
| `Mods/Update` | 8 | 2 | 0 | 6 |
| `Sound` | 2 | 0 | 0 | 2 |
| `Testing` | 8 | 0 | 0 | 8 |
| `Utility` | 11 | 3 | 0 | 8 |

The directory totals sum to 302 C# files. Appendix A assigns every one of those files to exactly one implementation wave; there are no duplicate or unassigned entries.

### 1.2 Existing Native pairs

`paired` below means only that a pair exists. It does **not** mean dependency-closed, linkable, or C#-equivalent.

1. `Entities/Enemies/43_SlenchNest.cs`
2. `Entities/Enemies/51_CarnivorousPlant.cs`
3. `Entities/LightSourceEntity.cs`
4. `Entities/PointModuleEntity.cs`
5. `Formats/Culling.cs`
6. `Formats/Enums.cs`
7. `Formats/Frontend.cs`
8. `Mods/Branding.cs`
9. `Mods/ConsoleWindow.cs`
10. `Mods/Credits.cs`
11. `Mods/Headless.cs`
12. `Mods/Input/GamepadState.cs`
13. `Mods/Input/PointerInput.cs`
14. `Mods/Input/SyntheticInput.cs`
15. `Mods/Input/TouchSettings.cs`
16. `Mods/Launcher/Gui/CrosshairPreview.cs`
17. `Mods/Launcher/Gui/HomeWindow.cs`
18. `Mods/Launcher/Gui/ProgressRow.cs`
19. `Mods/Launcher/Gui/TrackedText.cs`
20. `Mods/Launcher/Portable/LaunchPlan.cs`
21. `Mods/Launcher/Portable/SetupProgress.cs` — current Native pair is relocated to `Mods/Launcher/SetupProgress.*`
22. `Mods/MapGen/BuiltMap.cs`
23. `Mods/MapGen/MapTexturePack.cs`
24. `Mods/MapGen/RepackAccess.cs`
25. `Mods/ModEntry.cs`
26. `Mods/Network/NetConnectCommand.cs`
27. `Mods/Network/NetLag.cs`
28. `Mods/Network/NetMatchSync.cs`
29. `Mods/Network/NetPlayerSetup.cs`
30. `Mods/Network/NetProbe.cs`
31. `Mods/Network/NetScoreboard.cs`
32. `Mods/Render/Crosshair.cs`
33. `Mods/Render/FrameTiming.cs`
34. `Mods/Render/FrameTimingCheck.cs`
35. `Mods/Render/PlayerEntityIconBounds.cs`
36. `Mods/Render/PreviewCamera.cs`
37. `Mods/RenderOptions.cs`
38. `Mods/ShutdownSignals.cs`
39. `Mods/ThumbnailHost.cs`
40. `Mods/ThumbnailLog.cs`
41. `Mods/ThumbnailMode.cs`
42. `Mods/Update/BuildVersion.cs`
43. `Mods/Update/SyncHttp.cs`
44. `Mods/WorldEvents.cs`
45. `Program.cs`
46. `Utility/Console.cs`
47. `Utility/Output.cs`
48. `Utility/Rng.cs`

The Native include inventory confirms that many of these existing pairs include headers whose counterpart units do not yet exist. Examples include the current enemy files reaching `EnemyInstanceEntity.hpp` / `EnemySpawnEntity.hpp`, `LightSourceEntity` reaching `EntityBase.hpp`, `Renderer.hpp`, and format headers, and `Program.cpp` reaching the still-missing Read/Renderer/Metadata/Sound/Menu/ModEntry closure. Pair existence therefore cannot be used as a topological-completion flag.

---

## 2. Real process entry and startup dependency closure

The desktop/server process entry is the C# `Program.Main(string[] args)`. Its order is observable and must not be rearranged:

```text
Program.Main
  -> ConsoleSetup.Run()
  -> ConsoleWindow.Prepare(args)             [Windows]
  -> ModEntry.TryHandleHeadless(args)
  -> CheckSetup(args)
  -> ParseArguments(args)
  -> ModEntry.TryHandle(args)
  -> normal menu/setup/export/read/render path
```

`TryHandleHeadless` deliberately executes **before** game-file setup validation. That ordering is what allows server/headless commands to run without requiring `paths.txt` or extracted game files.

The launcher branch is inside `ModEntry.TryHandleHeadless`:

```text
Program.Main
  -> ModEntry.TryHandleHeadless
       -> launcher requested / desktop double-click
            -> GuiLauncher.TryRun()           [MPHREAD_AVALONIA and not --text]
                 -> true: handled
                 -> false: Windows console exposure as required
            -> TextLauncher.Run()
```

The same headless dispatcher also reaches settings/preferences, debug logging, update/install policy already defined by C#, render overrides, network controls, map utilities, dedicated/master-server paths, and shutdown behavior. This is why `ModEntry.cs` is a terminal integration unit rather than a foundation unit.

After headless handling, the normal `Program` path reaches setup/path management, argument parsing, `ModEntry.TryHandle`, `Menu`, image/sound/movie export, archive extraction, `Metadata`, `Read`, and `RenderWindow`.

### 2.1 Android is a different host, not a second desktop specification

`src/MphRead.Android` recompiles the same `src/MphRead/**/*.cs` files under `net9.0-android35.0`. Android starts `MainActivity`, not `Program.Main`. Its project-level aliases replace:

```text
GL  -> MphRead.Mods.Render.GlEs
AL  -> MphRead.Mods.Sound.AlEs
ALC -> MphRead.Mods.Sound.AlcEs
```

Accordingly:

- desktop/server Native process startup must preserve `Program.Main`;
- Android/shared-source behavior constrains the callable contracts of shared files and their GL/AL adapters;
- an Android host must remain a host adapter and must not redefine shared game semantics;
- a Native `main`, `WinMain`, service wrapper, or future platform head may only marshal platform startup into the C#-specified entry behavior. It must not jump directly to a launcher or invent a second dispatch policy.

There is no tracked dedicated `Makefile` or `CMakeLists.txt` under the current `src/MphRead.Native` source tree, so the existing Native files alone do not establish a complete build/link contract.

---

## 3. Why `using` extraction is insufficient

The generated indexes are coverage indexes, not semantic dependency graphs. They contain lexical false positives in the `types` column and even source fragments in an apparent using column. More importantly, C# same-namespace and enclosing-namespace lookup creates real edges without a `using`.

Confirmed examples:

| Consumer | Actual dependency not established by its `using` list | Consequence |
|---|---|---|
| `Mods/Input/PadBindings.cs` | `GamepadButtons` from `Mods/Input/GamepadState.cs` | `GamepadState` contract precedes `PadBindings`. |
| `Mods/Launcher/Portable/AdventureSave.cs` | `StorySave`, `GameState`, `Menu`, `Metadata`, `RoomMetadata` | It is not an atomic launcher leaf despite having no internal `using`. |
| `Mods/Launcher/Portable/GameFiles.cs` | root `Paths` plus the process behavior of the upstream setup path | It requires filesystem/process/runtime and root setup contracts. |
| `Formats/FhSound.cs` | the other `SoundRead` partial, including `ExportSamples`, plus `SoundSample`, `Paths`, `Read` | It cannot be independently parity-accepted before the `SoundRead` aggregate closes. |
| `Messaging.cs` | private `Scene` state such as `_frameCount` from other partial contributors | The full `Scene` declaration must be frozen first. |
| `Mods/Render/PreviewCamera.cs` | private camera/input members of `Scene` | Its existing isolated Native `Scene` shape is not a valid integration strategy. |
| `Mods/MapGen/RepackAccess.cs` | private members of `Repack` / `RepackCollision` implemented in other partial files | Those are one C# type each, not independent utilities. |

The dependency scheduler must therefore resolve declared types, member references, base classes/interfaces, static member access, partial declarations, private cross-partial access, generic constraints, reflection targets, and runtime-loaded contracts before a file is called dependency-closed.

---

## 4. Namespace/type ownership and partial-type closure

C# namespace membership does not imply a C++ header boundary. Ownership must follow the C# type declaration that actually owns the observable state and members.

High-fan-in ownership anchors include:

- `MphRead`: `Program`, `Scene`, `GameState`, `Read`, `RenderWindow`, `Metadata`, `Menu`, format/model primitives and shared enums.
- `MphRead.Entities`: `EntityBase`, concrete entities, and the aggregate `PlayerEntity`.
- `MphRead.Entities.Enemies`: individual enemy implementations consumed by metadata and spawners.
- `MphRead.Formats`, `.Collision`, `.Culling`, `.Sound`, `MphRead.Effects`, `MphRead.Editor`: binary/layout, collision, culling, effect, audio-format and editor contracts.
- `MphRead.Mods.*`: input, render, network, launcher, update, chat, sound and map-generation extensions.
- `MphRead.Text`, `MphRead.Sound`, `MphRead.Memory`, `MphRead.Archive`, `MphRead.Testing`: text/audio/runtime-memory/archive/test surfaces.

### 4.1 `Scene` is a six-file partial type

The complete reviewed `Scene` contributor set is:

```text
Scene.cs
Messaging.cs
Renderer.cs
Formats/Movie.cs
Mods/Render/PreviewCamera.cs
Mods/Render/PreviewPass.cs
```

The existing planning documents list only five and omit `Formats/Movie.cs`. That omission is material: `Formats/Movie.cs` explicitly declares `public partial class Scene` and owns movie state, decoder cancellation, GL/AL resources and movie transition behavior.

Mechanical C++ representation:

- `Scene.hpp` (the counterpart of `Scene.cs`) owns the one C++ `Scene` class declaration.
- The five sibling counterpart headers may declare their own auxiliary C# types and include/forward-declare the canonical `Scene`, but may not define a second `Scene`.
- Member declarations originating from all six C# files must be inventoried before the class declaration is frozen.
- Implementation bodies remain traceable to the counterpart `.cpp` for the C# file that contains them.

No additional unmatched aggregation header is needed or allowed by this migration contract.

`Scene` is not aggregate-PARITY-PASS until the last contributor (`PreviewPass.cs` in the wave plan) and the dependencies used by those members have passed.

### 4.2 `PlayerEntity` is a 21-file partial type

Contributors:

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

`Entities/Players/PlayerEntity.hpp` must own the single C++ class declaration. The current `Mods/Render/PlayerEntityIconBounds.hpp` instead declares a standalone `class PlayerEntity`; that is structurally incompatible with the eventual aggregate and must not be treated as authoritative.

The 21-file declaration surface must be frozen before Player bodies are considered stable. Aggregate acceptance occurs only after the late render/UI contributors have closed.

### 4.3 `Metadata` is a seven-file partial static type

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

`Metadata/Metadata.hpp` is the natural canonical declaration owner because it is the direct counterpart of the primary declaration file. All seven declaration surfaces and C# static initialization dependencies must be known before downstream code treats `Metadata` as closed. Body work can be staged earlier, but aggregate validation cannot complete until the enemy and player/weapon contributors and their dependencies are available.

### 4.4 `SoundRead` is a two-file partial static type

This partial cluster is missing from the existing partial-type sections:

```text
Formats/FhSound.cs
Formats/Sound.cs
```

Both explicitly declare `public static partial class SoundRead`. `FhSound.cs` calls `ExportSamples`, whose implementation belongs to the `Sound.cs` part. A C++ translation therefore needs a single `SoundRead` class declaration containing the members contributed by both files.

`Formats/Sound.hpp` should own that canonical declaration; `Formats/FhSound.hpp` should own `FhSoundSampleHeader` and reference the canonical `SoundRead` declaration rather than defining another class. `FhSound.cpp` may be authored earlier, but the partial type cannot pass link/behavior validation until `Formats/Sound.cs` is complete.

### 4.5 `Repack` and `RepackCollision`

`Repack` contributors:

```text
Utility/RepackEntity.cs
Utility/RepackModel.cs
Mods/MapGen/RepackAccess.cs
```

`RepackCollision` contributors:

```text
Utility/RepackCollision.cs
Mods/MapGen/RepackAccess.cs
```

`RepackAccess.cs` exists specifically to expose private functionality from the other partials. No Native-only repack framework should be introduced.

Because unmatched files are prohibited, an extra `Repack.Partials.hpp` (suggested as an option in one newer review) is not acceptable. Use an existing counterpart header as the mechanical declaration owner:

- `Utility/RepackEntity.hpp` for `Repack`;
- `Utility/RepackCollision.hpp` for `RepackCollision`.

Other counterpart headers include the canonical declaration as needed while retaining their own one-file traceability.

---

## 5. Strongly connected components and closure checkpoints

File-level work is not a pure DAG. C# intentionally has cycles that C++ must represent without altering semantics.

| SCC / cluster | Important edges | Earliest useful body work | Aggregate acceptance |
|---|---|---:|---:|
| `SoundRead` | `FhSound` partial -> `Sound` private/member surface; `Sound` -> `MphRead.Sound` | W2 | **W5** |
| `Metadata` | core tables -> rooms/sound/frontend; enemy tables -> enemy types; player/weapon tables -> gameplay types | W3 | **W7** |
| core Scene/entity/formats | `Scene` -> `EntityBase`; `EntityBase` stores `Scene`; formats/collision/effects feed both | W4 | W7 core checkpoint, but `Scene` partial itself remains open |
| `PlayerEntity` gameplay/network | player private state crosses partials; GameState/Scene/Input/Network use PlayerEntity and vice versa | W7 | **W12** after late partials |
| `Scene` render/movie tail | Renderer/Movie/Preview contributors share private Scene state and GL/AL/runtime resources | W8 | **W12** |
| `Repack` / `RepackCollision` | access partial calls private packers across files | W9 | **W9** |
| entry/dispatch | `ModEntry` fans into launcher/update/network/map/render; `Program` fans into ModEntry and normal app paths | W14 | **W15** |

A declaration-only pass is not file completion. Conversely, a file body can be authored in its assigned wave while the aggregate partial/SCC remains BLOCKED until its closure checkpoint.

This distinction corrects an unsupported implication in the older W4/W7 text: neither `Scene` nor `PlayerEntity` can be declared aggregate-complete at those points when later partial contributors still exist.

---

## 6. Platform, runtime and external-library contracts

### 6.1 .NET 9 semantics that are observable

The Native compatibility surface must preserve only the semantics actually used, but those uses include:

- UTF-16 `string`/`char` semantics and `ReadOnlySpan<char>`;
- ordinal vs current-culture vs invariant comparisons/casing;
- `Version`;
- enum underlying widths, aliases, `[Flags]`, parsing and formatting;
- default struct values, value copying, `readonly struct`, nullable values/references;
- arrays, spans, list/dictionary/hash/frozen/immutable collections and observed ordering;
- LINQ selection/order/exception behavior where used;
- integer overflow/promotion, signed/unsigned conversion, `BitConverter`, binary primitives;
- `Marshal`, layout, unsafe/pointer behavior;
- `Math`/`MathF`, including NaN and boundary behavior;
- reflection, attributes and delegate/event invocation;
- filesystem/path/current-directory behavior;
- streams, compression, tar, JSON;
- console, environment and process semantics;
- current culture and formatting;
- `DateTime`, `Stopwatch`, timers;
- threads, tasks, locks, cancellation and exception propagation;
- sockets, endpoint/address conversion and packet byte order;
- OS tests and process exit behavior.

A concrete warning already exists in the Native baseline: C# `Program.cs` calls `exportValue.ToLower()` (current-culture semantics), while the current Native `Program.cpp` routes these comparisons through an explicitly invariant helper. This is exactly the kind of plausible-looking Native policy that cannot be accepted without reproducing the C# runtime behavior.

### 6.2 OpenTK 4.9.4

Contracts include:

- `OpenTK.Mathematics` vector/matrix layout, normalization, equality and transform conventions;
- desktop OpenGL constants/state/lifetime/error behavior;
- GLFW keyboard/mouse/gamepad enum and callback/polling semantics;
- OpenAL source/buffer/device ownership and failure behavior.

### 6.3 Avalonia 11.3.11

GUI files must preserve:

- UI-thread ownership and dispatcher behavior;
- application lifetime and shutdown semantics;
- control/event ordering;
- storage/dialog behavior;
- the compile-time exclusion of `Mods/Launcher/Gui/**` in server builds.

### 6.4 Audio/image/project dependencies

Live manifests pin:

```text
OpenTK                    4.9.4
Avalonia                  11.3.11
ReFuel.StbImage           2.1.1
Silk.NET.OpenAL.Soft.Native 1.23.1
NcsfPlay                  project reference
CommunityToolkit.HighPerformance 8.4.2  [through NcsfPlay]
SoundFlow                 1.4.1          [through NcsfPlay]
System.IO.Hashing         10.0.9         [through NcsfPlay]
```

These are external contracts, not permission to replace their observed C# behavior with a different Native policy.

### 6.5 Network standalone contract

`tools/nettest/nettest.csproj` directly compiles:

```text
Mods/Network/NetProtocol.cs
Mods/Network/NetTransport.cs
Mods/Network/NetProbe.cs
```

with OpenTK 4.9.4. This provides an independent network validation surface in addition to the full game/server closure.

---

## 7. Native-only hosts and existing Native caveats

### 7.1 Hosts must remain thin

A C++ `main`, Windows GUI entry, service wrapper, or platform bootstrap is outside the one-C#-file/one-pair inventory only to the extent that the language/platform requires a process hook.

Allowed host work:

- acquire native argv/environment in the form required to invoke the translated C# contract;
- perform unavoidable ABI/runtime bootstrap;
- invoke the translated entry exactly once;
- propagate the specified exit/error behavior.

Not allowed:

- bypass `Program.Main`;
- dispatch directly to `ModEntry`, GUI, text launcher or renderer;
- reinterpret arguments;
- add fallback launcher/server policy;
- swallow exceptions or rewrite exit status without a C# basis.

For Android, `MainActivity` is the C# platform head and its behavior is already specified outside `src/MphRead`; a future Native Android head must adapt to that platform contract rather than pretending Android calls desktop `Program.Main`.

### 7.2 Existing Native code remains suspect

Specific structural warnings:

1. `PlayerEntityIconBounds.hpp` currently defines an isolated `PlayerEntity`, which conflicts with the aggregate C# partial type.
2. `SetupProgress.*` is at the wrong relative path for the stated strict target.
3. `Program.cpp` includes many absent Native prerequisites; its existence is not evidence that the entry closure is buildable.
4. Existing enemy/entity pairs include not-yet-existing base/entity headers.
5. Native include edges are an implementation snapshot, not C# dependency authority.
6. Historical PASS/FAIL labels are review evidence only. Without a reproducible C# oracle and exact reviewed dependency state, they are not permanent parity certificates.

---

## 8. Prerequisites and validation gates

### Gate 0 - runtime oracle freeze

Before accepting dependent translations, establish tests for the actually used portions of:

```text
integer conversion/overflow
float conversion and Math/MathF boundaries
UTF-16/string/char/casing/culture
enum/[Flags] values and formatting
struct default/copy/layout
Version parsing/comparison
filesystem/path/current directory
exceptions and failure timing
thread/task/cancellation
OpenTK mathematics
```

### Per-file gate

For every C# file:

1. read the complete C# source at the pinned commit;
2. enumerate every declared type/member, visibility, modifiers, defaults and initialization;
3. resolve every direct symbol/member/base/interface/partial dependency;
4. inspect the exact contracts called by executable code, not just namespaces;
5. compare the complete existing Native pair if present;
6. preserve return values, exceptions, mutation, ordering, side effects, timing and lifetime;
7. compile the translation unit against real declarations;
8. run deterministic C# vs Native oracle cases where the file permits them;
9. do not mark PARITY-PASS if a partial/SCC aggregate is still open.

### SCC gate

Before exposing an SCC as a prerequisite to later waves:

- one-definition/partial declaration merge complete;
- all cross-partial private members represented once;
- full Native link succeeds without placeholders;
- static initialization behavior validated;
- serialization/network byte tests pass where applicable;
- cancellation/thread ownership tests pass where applicable.

### Platform gates

A migration checkpoint is not complete until the relevant configurations pass:

1. desktop client build/link and deterministic parity;
2. dedicated-server build/link and headless startup trace;
3. network `nettest` protocol/transport/probe tests;
4. Android/shared-adapter compile/behavior checks for GL/AL/ALC substitutions;
5. visual output tests for renderer/HUD/image paths;
6. audio output/state/lifetime tests for Sfx/Music/movie paths;
7. end-to-end startup/argument/process behavior for `Program -> ModEntry -> launcher/normal paths`.

---

## 9. Blocked and special-case files

The following are not safe independent leaves even if a Native pair already exists or their `using` column looks small:

- `Program.cs`: terminal process integration, culture/filesystem/export/render/launcher closure.
- `Mods/ModEntry.cs`: largest dispatch fan-out; headless/launcher/update/network/map/render integration.
- `Mods/Launcher/Gui/GuiLauncher.cs`: Avalonia lifetime plus launcher/match handoff.
- `Mods/Launcher/Portable/TextLauncher.cs`: portable launcher/game/network/update closure.
- `Mods/Launcher/Portable/MatchStart.cs`: gameplay/network launch bridge.
- all update files with HTTP/process/install/cancellation (`Updater`, `DesktopUpdate`, `ServerUpdate`, `UpdateCheck`, `UpdateDownload`, `UpdateInstall`) until runtime seams are frozen.
- `Entities/EntityBase.cs`, `Scene.cs`, `Renderer.cs`, `Formats/Formats.cs`, `Formats/Collision.cs`, `Formats/Effects.cs`, `Formats/Sound.cs`, `Sound/Music.cs`, `Sound/Sfx.cs`: core SCC.
- every `PlayerEntity` partial contributor when considered in isolation.
- every `Scene` partial contributor when considered in isolation.
- `Formats/FhSound.cs` as an aggregate-PASS target before `Formats/Sound.cs`, because `SoundRead` is one partial type.
- `Metadata/*` as independently closed singleton-like classes; the seven files form one static partial surface.
- gameplay network files consuming Scene/PlayerEntity before the player/scene declarations and core behavior are available.
- current `Mods/Render/PlayerEntityIconBounds.hpp` as an integration-ready PlayerEntity declaration.
- current relocated `SetupProgress` as final structural parity.

---

## 10. Stale or unsupported assumptions in existing planning documents

1. **Old snapshot hashes are stale as branch identifiers.** The live reviewed head is the commit at the top of this file. Source/native content is nevertheless unchanged from the dependency-index snapshot, which is why the counts remain valid.
2. **The generated `types` and `using` tables are not semantic graphs.** They contain lexical false positives and miss same/enclosing-namespace edges.
3. **The five-file `Scene` partial list is incomplete.** `Formats/Movie.cs` is a sixth explicit `Scene` partial contributor.
4. **The partial-type discussions omit `SoundRead`.** `Formats/FhSound.cs` and `Formats/Sound.cs` are one partial static class, and `FhSound` calls members supplied by the other part.
5. **W4 cannot aggregate-complete `Scene`.** Scene contributors remain in W8 and W12; only its declaration surface can be frozen earlier.
6. **W7 cannot aggregate-complete `PlayerEntity`.** `PlayerEntityProHud` is later and `PlayerEntityEndScreen` is in W12.
7. **W3 cannot aggregate-complete `Metadata`.** enemy/player/weapon partial bodies depend on later gameplay waves; declaration freezing and aggregate acceptance are separate events.
8. **A suggested unmatched `Repack.Partials.hpp` would violate the requested one-file/no-unmatched-files contract.** Existing counterpart headers must own the merged declaration mechanically.
9. **The old audit ledger contains stale path labels.** Examples include `Mods/Input/EsBindings.cs` (actual `Mods/Render/EsBindings.cs`), `Mods/MapTexturePack.cs` (actual `Mods/MapGen/MapTexturePack.cs`), `Utility/RawStructs.cs` (actual `Mods/MapGen/RawStructs.cs`), `Mods/Launcher/Portable/Frontend.cs` (actual `Formats/Frontend.cs`), `Sound/FhSound.cs` (actual `Formats/FhSound.cs`), and several similar launcher/render/test labels. Canonical tree paths must be used.
10. **A broad “leaf” wave based on empty internal `using` is unsafe.** `AdventureSave.cs` directly calls root `GameState`, `Menu`, `Metadata` and `StorySave`; `GameFiles.cs` directly calls root `Paths` and process/filesystem setup behavior. They are not atomic leaves.
11. **The two newer reviews disagree on the immediate pair.** One proposes `Shaders.cs` + `ChatFont.cs`; another proposes `ChatFont.cs` + `StylusZone.cs`. The source-level closure favors `ChatFont` + `StylusZone` as the smallest independent pair; `Shaders.cs` is the immediate third leaf.
12. **Recorded PASS labels are not current semantic proof.** They remain useful history but must re-enter their assigned validation gate against the live C# contracts.
13. **`paired` is not `buildable`.** The current Native include graph contains numerous edges to headers that do not yet exist.
14. **Existing Native runtime choices are not automatically valid.** The current-culture C# `Program.ToLower()` versus invariant Native helper is a concrete example requiring correction during the eventual Program audit.

---

## 11. Exact next two independent leaves

The exact next pair for parallel work at the reviewed commit is:

### Worker A - `Mods/Chat/ChatFont.cs`

Closure:

- only `System`;
- no MphRead type dependency;
- self-contained constants, arrays, `Index`, `Measure`, helper overloads and static glyph initialization;
- no platform framework, I/O, process, network, renderer or thread dependency.

Validation must include exact `char`/UTF-16 range behavior, every generated byte/width entry, static initialization, `ReadOnlySpan<char>` measurement behavior, unsupported-character handling and boundary characters.

### Worker B - `Mods/Input/StylusZone.cs`

Closure:

- only `System`;
- owns its enum, nested readonly value type, static state and all methods locally;
- no MphRead type dependency;
- no I/O, process, renderer object, network object or external framework type.

Validation must include enum values, array order, float constants, `Math.Clamp`/NaN/boundary behavior, placement state transitions, edge inclusivity in `RegionAt`, contact/pressed transition behavior, static defaults and reset semantics.

These two files have no edge between them and can be worked independently. `Shaders.cs` is the next leaf after them; it is also internally independent, but its very large exact multiline-string payload and the GL-boundary encoding/hash validation make it a less bounded first parallel partner than `StylusZone.cs`.

---

## 12. Actionable implementation waves

### Scheduling semantics

The file assignment below is an **authoring/implementation wave**: every source file appears exactly once. For partial/SCC files, authoring can occur before aggregate acceptance. The aggregate closure checkpoints in Section 5 control when later waves may rely on the type as PARITY-PASS.

Within a wave, resolve actual symbol edges first and use the listed order as the deterministic default. Independent files may be parallelized. Do not use placeholder bodies to satisfy same-wave dependencies.

### W0 - Runtime contract freeze

No source file is assigned to W0. Establish the Gate 0 oracles and the mechanical partial-type declaration strategy using only existing counterpart headers.

### W1 - atomic leaves / low-coupling existing pairs

Start with `ChatFont.cs` and `StylusZone.cs` in parallel, then `Shaders.cs`, then the remaining W1 units.

- `Mods/Chat/ChatFont.cs`
- `Mods/Input/StylusZone.cs`
- `Shaders.cs`
- `Formats/Enums.cs`
- `Formats/Frontend.cs`
- `Mods/Branding.cs`
- `Mods/ConsoleWindow.cs`
- `Mods/Credits.cs`
- `Mods/Headless.cs`
- `Mods/Input/TouchSettings.cs`
- `Mods/Launcher/Portable/LaunchPlan.cs`
- `Mods/Launcher/Portable/SetupProgress.cs`
- `Mods/MapGen/MapTexturePack.cs`
- `Mods/Render/Crosshair.cs`
- `Mods/Update/BuildVersion.cs`
- `Utility/Console.cs`
- `Utility/Output.cs`
- `Utility/Rng.cs`

Gate: deterministic leaf oracles pass; existing pairs are re-audited rather than grandfathered.

### W2 - runtime/data helpers and adapter foundations

- `Memory.cs`
- `MemoryArrays.cs`
- `Formats/Culling.cs`
- `Formats/EntityEnemy.cs`
- `Formats/FhSound.cs`
- `Formats/Model.cs`
- `Formats/NodeData.cs`
- `Formats/Types.cs`
- `Mods/HunterSuits.cs`
- `Mods/LogShare.cs`
- `Mods/RenderOptions.cs`
- `Mods/ShutdownSignals.cs`
- `Mods/ThumbnailHost.cs`
- `Mods/ThumbnailLog.cs`
- `Mods/WindowMode.cs`
- `Mods/Input/GamepadDesktop.cs`
- `Mods/Input/GamepadLayout.cs`
- `Mods/Input/GamepadMappings.cs`
- `Mods/Input/GamepadProbe.cs`
- `Mods/Input/GamepadState.cs`
- `Mods/Input/PadBindings.cs`
- `Mods/Input/PointerInput.cs`
- `Mods/Input/SyntheticInput.cs`
- `Mods/Network/NetLag.cs`
- `Mods/Network/NetProbe.cs`
- `Mods/Render/EsBindings.cs`
- `Mods/Render/EsShaders.cs`
- `Mods/Render/FrameTiming.cs`
- `Mods/Render/FrameTimingCheck.cs`
- `Mods/Render/GlEs.cs`
- `Mods/Sound/AlEs.cs`
- `Mods/Update/SyncHttp.cs`
- `Utility/Analyzer.cs`
- `Utility/Archive.cs`
- `Utility/Compress.cs`
- `Utility/Parser.cs`

Gate: runtime/data adapters used by later units have deterministic C# parity. `SoundRead` remains open because `FhSound.cs` is only one partial contributor.

### W3 - shared data/text/metadata foundation

- `Features.cs`
- `Strings.cs`
- `Export/Collada.cs`
- `Export/Scripting.cs`
- `HUD/HudInfo.cs`
- `Metadata/FrontendMeta.cs`
- `Metadata/Metadata.cs`
- `Metadata/Rooms.cs`
- `Metadata/SoundMeta.cs`
- `Utility/Extract.cs`

Gate: text/export/base metadata bodies pass. Freeze the complete seven-file `Metadata` declaration surface, but do not aggregate-accept Metadata yet.

### W4 - core formats/Scene/entity declaration SCC

- `Messaging.cs`
- `Read.cs`
- `Scene.cs`
- `Entities/EntityBase.cs`
- `Entities/LightSourceEntity.cs`
- `Entities/PlayerSpawnEntity.cs`
- `Entities/PointModuleEntity.cs`
- `Entities/Players/DynamicLightEntity.cs`
- `Formats/AiPersonality.cs`
- `Formats/Collision.cs`
- `Formats/CollisionDetection.cs`
- `Formats/Effects.cs`
- `Formats/Entity.cs`
- `Formats/EntityClass.cs`
- `Formats/Formats.cs`
- `Formats/RawFormats.cs`

Gate: core declarations and translation units compile/link as far as real definitions permit. Freeze all six `Scene` partial declarations now, including later Movie/Preview members. Scene remains aggregate-open.

### W5 - sound and transport closure

- `Formats/Sound.cs`
- `Mods/Network/DedicatedServer.cs`
- `Mods/Network/DemoClip.cs`
- `Mods/Network/DemoFile.cs`
- `Mods/Network/DemoInfo.cs`
- `Mods/Network/DemoLibrary.cs`
- `Mods/Network/DemoPlayback.cs`
- `Mods/Network/DemoRecorder.cs`
- `Mods/Network/MapRotation.cs`
- `Mods/Network/MapVote.cs`
- `Mods/Network/NetMaster.cs`
- `Mods/Network/NetProtocol.cs`
- `Mods/Network/NetStatus.cs`
- `Mods/Network/NetTransport.cs`
- `Mods/Sound/SfxMixer.cs`
- `Sound/Music.cs`
- `Sound/Sfx.cs`

Gate: `SoundRead` closes with `Formats/Sound.cs`; transport/audio-format byte and lifetime tests pass.

### W6 - concrete entity/enemy layer

- `Entities/AreaVolumeEntity.cs`
- `Entities/ArtifactEntity.cs`
- `Entities/BeamEffectEntity.cs`
- `Entities/BeamProjectileEntity.cs`
- `Entities/BombEntity.cs`
- `Entities/DoorEntity.cs`
- `Entities/EnemyInstanceEntity.cs`
- `Entities/EnemySpawnEntity.cs`
- `Entities/FlagBaseEntity.cs`
- `Entities/ForceFieldEntity.cs`
- `Entities/ItemInstanceEntity.cs`
- `Entities/ItemSpawnEntity.cs`
- `Entities/JumpPadEntity.cs`
- `Entities/MorphCameraEntity.cs`
- `Entities/NodeDefenseEntity.cs`
- `Entities/ObjectEntity.cs`
- `Entities/OctolithFlagEntity.cs`
- `Entities/PlatformEntity.cs`
- `Entities/RoomEntity.cs`
- `Entities/TeleporterEntity.cs`
- `Entities/TriggerVolumeEntity.cs`
- `Entities/CamSeq/CameraSequence.cs`
- `Entities/CamSeq/CamSeqEntity.cs`
- `Entities/Enemies/00_WarWasp.cs`
- `Entities/Enemies/01_Zoomer.cs`
- `Entities/Enemies/02_Temroid.cs`
- `Entities/Enemies/03_Petrasyl1.cs`
- `Entities/Enemies/04_Petrasyl2.cs`
- `Entities/Enemies/05_Petrasyl3.cs`
- `Entities/Enemies/06_Petrasyl4.cs`
- `Entities/Enemies/10_BarbedWarWasp.cs`
- `Entities/Enemies/11_Shriekbat.cs`
- `Entities/Enemies/12_Geemer.cs`
- `Entities/Enemies/16_Blastcap.cs`
- `Entities/Enemies/18_AlimbicTurret.cs`
- `Entities/Enemies/19_Cretaphid.cs`
- `Entities/Enemies/20_CretaphidEye.cs`
- `Entities/Enemies/21_CretaphidCrystal.cs`
- `Entities/Enemies/23_PsychoBit.cs`
- `Entities/Enemies/24_Gorea1A.cs`
- `Entities/Enemies/25_GoreaHead.cs`
- `Entities/Enemies/26_GoreaArm.cs`
- `Entities/Enemies/27_GoreaLeg.cs`
- `Entities/Enemies/28_Gorea1B.cs`
- `Entities/Enemies/29_GoreaSealSphere1.cs`
- `Entities/Enemies/30_Trocra.cs`
- `Entities/Enemies/31_Gorea2.cs`
- `Entities/Enemies/32_GoreaSealSphere2.cs`
- `Entities/Enemies/33_GoreaMeteor.cs`
- `Entities/Enemies/35_Voldrum.cs`
- `Entities/Enemies/36_Voldrum.cs`
- `Entities/Enemies/37_Quadtroid.cs`
- `Entities/Enemies/38_CrashPillar.cs`
- `Entities/Enemies/39_FireSpawn.cs`
- `Entities/Enemies/40_EnemySpawner.cs`
- `Entities/Enemies/41_Slench.cs`
- `Entities/Enemies/42_SlenchShield.cs`
- `Entities/Enemies/43_SlenchNest.cs`
- `Entities/Enemies/44_SlenchSynapse.cs`
- `Entities/Enemies/45_SlenchTurret.cs`
- `Entities/Enemies/46_LesserIthrak.cs`
- `Entities/Enemies/47_GreaterIthrak.cs`
- `Entities/Enemies/49_ForceFieldLock.cs`
- `Entities/Enemies/50_HitZone.cs`
- `Entities/Enemies/51_CarnivorousPlant.cs`
- `Metadata/Enemies.cs`

Gate: concrete entity/enemy behavior passes against the core. Enemy metadata contribution may now be validated.

### W7 - player/gameplay/network core

- `GameState.cs`
- `Menu.cs`
- `SceneSetup.cs`
- `Entities/Players/HalfturretEntity.cs`
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
- `Metadata/Player.cs`
- `Metadata/Weapons.cs`
- `Mods/GameSettings.cs`
- `Mods/InputSettings.cs`
- `Mods/RespawnChoice.cs`
- `Mods/SpectatorMode.cs`
- `Mods/WorldEvents.cs`
- `Mods/Chat/ChatBox.cs`
- `Mods/Chat/PlayerEntityChatHud.cs`
- `Mods/Input/GamepadInput.cs`
- `Mods/Network/MapAudit.cs`
- `Mods/Network/MechanicsDump.cs`
- `Mods/Network/NetCheckClient.cs`
- `Mods/Network/NetConnectCommand.cs`
- `Mods/Network/NetDamage.cs`
- `Mods/Network/NetDiagnostics.cs`
- `Mods/Network/NetFeatureCheck.cs`
- `Mods/Network/NetHitPrediction.cs`
- `Mods/Network/NetHooks.cs`
- `Mods/Network/NetHostSession.cs`
- `Mods/Network/NetLaunch.cs`
- `Mods/Network/NetLog.cs`
- `Mods/Network/NetMatchEnd.cs`
- `Mods/Network/NetMatchSync.cs`
- `Mods/Network/NetPlayerBridge.cs`
- `Mods/Network/NetPlayerSetup.cs`
- `Mods/Network/NetRoomChange.cs`
- `Mods/Network/NetScoreboard.cs`
- `Mods/Network/NetSession.cs`
- `Mods/Network/NetSlotManager.cs`
- `Mods/Network/NetTestScript.cs`
- `Mods/Network/NetUnlagged.cs`
- `Mods/Network/PlayerColors.cs`
- `Mods/Network/PlayerEntityNetAim.cs`
- `Mods/Network/PlayerEntityNetHud.cs`
- `Mods/Network/ServerSim.cs`
- `Mods/Network/ServerSimCheck.cs`
- `Mods/Network/WeaponDps.cs`
- `Mods/Render/PlayerEntityAmmoClear.cs`
- `Mods/Render/PlayerEntityIconBounds.cs`
- `Mods/Render/PlayerEntityStylusHud.cs`
- `Mods/Render/PlayerEntityVoteHud.cs`

Gate: freeze the complete 21-file `PlayerEntity` declaration. Core player/network/gameplay link tests pass. The seven-file `Metadata` aggregate can close here. PlayerEntity and Scene remain open because late render partials remain.

### W8 - renderer/movie/image/preview layer

- `Renderer.cs`
- `Selection.cs`
- `Export/Images.cs`
- `Formats/Movie.cs`
- `Mods/ScreenCapture.cs`
- `Mods/ThumbnailBatch.cs`
- `Mods/ThumbnailCapture.cs`
- `Mods/ThumbnailGenerator.cs`
- `Mods/ThumbnailMode.cs`
- `Mods/Render/HunterPreview.cs`
- `Mods/Render/PlayerEntityProHud.cs`
- `Mods/Render/PreviewCamera.cs`
- `Mods/Render/SmoothHudIcon.cs`

Gate: Renderer/Movie/image/preview bodies pass visual/resource/lifetime tests. `Formats/Movie.cs` is part of the Scene aggregate; Scene still remains open until W12.

### W9 - map generation, memory classes and repack closure

- `MemoryClasses.cs`
- `Mods/MapGen/BuiltMap.cs`
- `Mods/MapGen/CustomRooms.cs`
- `Mods/MapGen/MapBuilder.cs`
- `Mods/MapGen/MapBundle.cs`
- `Mods/MapGen/MapCollisionPacker.cs`
- `Mods/MapGen/MapDefinition.cs`
- `Mods/MapGen/MapNodePacker.cs`
- `Mods/MapGen/MapPacker.cs`
- `Mods/MapGen/MapReport.cs`
- `Mods/MapGen/MapTextureBake.cs`
- `Mods/MapGen/Q3Bsp.cs`
- `Mods/MapGen/Q3Convert.cs`
- `Mods/MapGen/Q3Import.cs`
- `Mods/MapGen/RawStructs.cs`
- `Mods/MapGen/RepackAccess.cs`
- `Utility/RepackCollision.cs`
- `Utility/RepackEntity.cs`
- `Utility/RepackModel.cs`

Gate: MapGen and Repack/RepackCollision aggregates close; binary round-trip/oracle tests pass.

### W10 - update subsystem

- `Mods/Update/DesktopUpdate.cs`
- `Mods/Update/ServerUpdate.cs`
- `Mods/Update/UpdateCheck.cs`
- `Mods/Update/UpdateDownload.cs`
- `Mods/Update/UpdateInstall.cs`
- `Mods/Update/Updater.cs`

Gate: HTTP/process/install/update state, cancellation and failure behavior pass on applicable platforms.

### W11 - portable launcher and end-screen/log integration

- `Mods/DebugLog.cs`
- `Mods/EndScreen.cs`
- `Mods/Launcher/Portable/AdventureSave.cs`
- `Mods/Launcher/Portable/GameFiles.cs`
- `Mods/Launcher/Portable/LauncherPrefs.cs`
- `Mods/Launcher/Portable/MatchStart.cs`
- `Mods/Launcher/Portable/TextLauncher.cs`

Gate: portable launcher setup/save/match-start/log/end-screen behavior passes against already closed gameplay/update contracts.

### W12 - GUI plus late Scene/PlayerEntity partial tail

- `Mods/Launcher/Gui/CrosshairPreview.cs`
- `Mods/Launcher/Gui/DemoPickerView.cs`
- `Mods/Launcher/Gui/GuiLauncher.cs`
- `Mods/Launcher/Gui/GuiTheme.cs`
- `Mods/Launcher/Gui/HomeView.cs`
- `Mods/Launcher/Gui/HomeWindow.cs`
- `Mods/Launcher/Gui/KeyRow.cs`
- `Mods/Launcher/Gui/MapPickerView.cs`
- `Mods/Launcher/Gui/MenuEntry.cs`
- `Mods/Launcher/Gui/PadRow.cs`
- `Mods/Launcher/Gui/PauseMenuView.cs`
- `Mods/Launcher/Gui/PauseMenuWindow.cs`
- `Mods/Launcher/Gui/ProgressRow.cs`
- `Mods/Launcher/Gui/Rows.cs`
- `Mods/Launcher/Gui/ServerRow.cs`
- `Mods/Launcher/Gui/SettingsView.cs`
- `Mods/Launcher/Gui/SettingsWindow.cs`
- `Mods/Launcher/Gui/SliderRow.cs`
- `Mods/Launcher/Gui/SplashView.cs`
- `Mods/Launcher/Gui/TrackedText.cs`
- `Mods/Launcher/Gui/UiCapture.cs`
- `Mods/Launcher/Gui/UpdateBadge.cs`
- `Mods/PauseMenu.cs`
- `Mods/Render/PlayerEntityEndScreen.cs`
- `Mods/Render/PreviewPass.cs`

Gate: Avalonia/UI behavior passes where enabled; final Scene and PlayerEntity partial contributors close. Aggregate `Scene` and `PlayerEntity` PARITY-PASS is first possible here.

### W13 - tests/oracle consumers

- `Test.cs`
- `Testing/TestEffects.cs`
- `Testing/TestLogic.cs`
- `Testing/TestMisc.cs`
- `Testing/TestOverlay.cs`
- `Testing/TestParse.cs`
- `Testing/TestPlayer.cs`
- `Testing/TestPrint.cs`
- `Testing/TestWeapons.cs`

Gate: test/oracle files themselves match C# behavior and do not introduce production semantics.

### W14 - ModEntry integration

- `Mods/ModEntry.cs`

Gate: `ModEntry.TryHandleHeadless` and `TryHandle` traces match C# across desktop/server flags and representative arguments.

### W15 - Program integration

- `Program.cs`

Gate: complete end-to-end `Program.Main` trace, setup/export/menu/render paths, culture/argument/error/exit behavior and launcher dispatch match C#.

---

## Appendix A - 302-file one-assignment proof

The W1-W15 lists above are the exact complete inventory. Count by assigned wave:

| Wave | Files |
|---:|---:|
| W1 | 18 |
| W2 | 36 |
| W3 | 10 |
| W4 | 16 |
| W5 | 17 |
| W6 | 66 |
| W7 | 58 |
| W8 | 13 |
| W9 | 19 |
| W10 | 6 |
| W11 | 7 |
| W12 | 25 |
| W13 | 9 |
| W14 | 1 |
| W15 | 1 |
| **Total** | **302** |

No path is duplicated across waves and no source path from the 302-file inventory is unassigned.

## Appendix B - Required review state vocabulary

Use only these semantic states for migration tracking:

- **UNREVIEWED** - pair may exist, but complete C# parity has not been established.
- **BLOCKED** - complete file audit identified a real prerequisite/adapter/SCC that is not yet closed; no placeholder is permitted.
- **PARITY-PASS** - complete file behavior is reproduced and all required dependency/SCC gates for relying on it are closed.

Structural labels such as `paired`, `missing`, and `relocated` remain useful inventory metadata but are not semantic states.

## Final verdict

The live tree is still a 302-file C# source surface with 48 existing Native pairs, 254 absent pairs, and one structurally relocated pair. The high-level W1-W15 inventory is usable as an authoring schedule once its acceptance semantics are corrected for real partial/SCC closure.

The most important corrections are:

- add `Formats/Movie.cs` to the `Scene` partial aggregate;
- add the two-file `SoundRead` partial aggregate;
- separate declaration freezing from aggregate PARITY-PASS for Scene, PlayerEntity, Metadata and SoundRead;
- reject any C#-unmatched aggregation header;
- continue resolving same/enclosing-namespace member dependencies rather than trusting `using`;
- keep `ModEntry` penultimate and `Program` final;
- begin the next independent parallel work with `Mods/Chat/ChatFont.cs` and `Mods/Input/StylusZone.cs`, with `Shaders.cs` next.

No C# or C++ implementation change is part of this review.
