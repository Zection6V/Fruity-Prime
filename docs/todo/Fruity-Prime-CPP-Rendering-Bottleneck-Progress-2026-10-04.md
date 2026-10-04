# Rendering bottleneck plan — execution evidence

Plan: [audit and fix plan](Fruity-Prime-CPP-Rendering-Bottleneck-Audit-and-Fix-Plan-2026-10-04.md).
Starting production HEAD: `9ce34a391ad8bb2d08848e8189e413eccf28d2a4`.

This is an incomplete execution record. The goal remains open until all mandatory
requirements, including Vulkan mean >=850 FPS and image correctness, have evidence.

| Requirement | Status / authoritative evidence |
|---|---|
| Phase 0, `dc1ffe` → `dd69cb` A/B before renderer edits | Passed matched active fixture, two sequential A/B pairs before renderer edits; see evidence below. |
| Remaining historical boundaries: `3d72fbb`, `cc0d2e90`, `ec6e98b`, `f2597d92`, `e03978dc`, `9ce34a39` | Pending; no inferred timings from old summaries. |
| Phase 1, uniform state by program/block/slot generation | Implemented fixed program/block arrays stamped by recording generation; MSVC Release / CTest 21/21 passed. Runtime/mutation/lifetime verification in progress. |
| Phase 1, descriptor group dirty/version state | Pending. Current full semantic key construction/hash confirmed. |
| Phase 1, material owner identity/version state | Implemented stable RenderItem owner IDs, paged owner CPU data/version state, and two completion-safe GPU slices per owner. Full material content hashing removed; same-recording mutation uses immutable fallback. Unit tests and resource/lifetime gates pass. Remaining descriptor hot-path work pending. |
| Phase 1, unchanged update/bind suppression, push constants, mutation/lifetime correctness | Pending. Must preserve while replacing cache. |
| Phase 2, program-owned uniform locations and alpha values | Pending. Current draw-time program/location queries confirmed. |
| Phase 2, device-owned immutable limits | Pending. Current binding-time driver queries confirmed. |
| Phase 2, retained VAO and explicit Qt interop invalidation | Pending. Current post-draw VAO zero bind confirmed. |
| Generic OpenGL binding snapshot reuse without system heap churn | Pending. |
| Phase 3, submit helper high-water storage and thread/reentrancy contract | Implemented retained signal storage, constructing-thread ownership and reentrancy rejection; native-array/failure/high-water/foreign-thread tests pass. |
| Phase 3, window-owned event pump | Removed the swapchain event pump; window loop retains event delivery. Native/fallback presentation and RHI conformance pass; final switch gate pending. |
| Phase 3, nonblocking acquire/image/present retirement; bounded frame-slot reuse | Removed image.lastFrame host waits and acquire-time present waits. Four present completion tickets per image, polled before reuse; bounded exhaustion wait and teardown proof remain. Native/fallback presentation gates pass. Deferred acquire-fence proof and separate steady/teardown telemetry pending. |
| Phase 4, Off throughput / Generic On latency / native Reflex single authority | Pending final matrix; do not remove intentional On waits. |
| Sampling profiler and allocation/API/wait/event telemetry | Opt-in Windows main-thread 1 ms stack sampling implemented, heap preallocated and symbols resolved after capture. WPR unavailable (policy permission 0xc5585011); built-in sample recorded 6,919 stacks. Allocation/API/wait/event counters still pending. |
| Same room/hunter/bots/spawn/camera/size/render settings; warmup excluded | Historical and Qt diagnostic fixture uses first spawn, Sylux +3 bots, Alinos Perch, 2560x1439, scale 100, fog on, cel/FPS display off, Low Latency Off, Unlimited, requested/actual Immediate. |
| FPS-only Vulkan >=850 FPS, <=1.18 ms; goal >=950 FPS | Passed current task source: native Reflex available Off, 1182.49 / 1189.08 FPS, 0.85030 / 0.84573 ms loop, same fixture, no profiling or validation. Final source delivery/CI still pending. |
| GPU scene and present CPU time, OpenGL CPU/GPU comparison | Pending separate profiling runs. |
| Windows MSVC, Linux GCC, macOS Clang, Android NDK, full CTest | Pending final revision. Starting revision CI run 37146220993 completed successfully. |
| RHI conformance, resources, validation, GPU lifetime cycles | Current task source passes full RHI conformance, resource validation and 3 GPU lifetime cycles; zero live/retired objects, validation errors 0, steady lifetime host waits 0. |
| Immediate/FIFO/Mailbox, resize/fullscreen/minimize/restore, renderer switches | Pending final revision. |
| Off/On/Boost, native/fallback/toggles, no double pacing | Current source native Reflex check passes FIFO/Immediate/Mailbox, 5 cap choices, Off/On/Boost/Off transitions, resize/minimize/restore, 268 completed and 1 abandoned frame, validation 0 errors. Runtime failure/fallback matrix still pending. |
| Golden Capture and GL/Vulkan parity: HUD, transparency, decals, particles, trails, fade, disruption, backdrop distortion | Latest source: GL/Vulkan Golden 7/7 pass, no pixels differ beyond 8 levels; owner-cache GL exactly matches previous GL pixels 7/7. Qt GL context ownership fixed to release the device before destroying its native context. Backdrop distortion parity passes two complete session cycles, animation/resize/fullscreen return, max difference 1 level, no errors or live resources. |
| No draw/effect omission, CPU rendering/copies, synchronous readback, hot-path queue/device idle | Pending final static and runtime audit. |

Historical worktree `render-bottleneck-baseline` stays detached. Original game files
are referenced by copied `paths.txt`; preferences/saves are isolated copies. The
production tree's pre-existing Reflex-document move is preserved independently.

Historical build-only adjustment: old Visual Studio environment overrides
`VCPKG_ROOT`; pin the isolated old build script to installed `C:/vcpkg` after
vcvars. No renderer logic changes are included in this adjustment. Both sides use
the same compiler, dependencies, driver, and diagnostic-only CSV/hold patch.

## Matched historical A/B before renderer changes

All four checks exited 0 and photographed an active player. Use the first seven
complete CSV windows after the built-in two-second warmup; the last window includes
the exit screenshot/readback and is excluded. No build or other GPU test ran during
these measurements. MSVC 19.44 Release; NVIDIA RTX 5070 Ti, driver 617.14;
validation and GPU profiling disabled.

| Revision | Run | Mean FPS | Mean CPU loop ms | Mean present CPU ms |
|---|---|---:|---:|---:|
| `dc1ffe004dadeba8f938d5519eb154206b667f06` | A1 | 1160.05 | 0.85949 | 0.09343 |
| `dd69cb2467e6fdc322f4f7ad73f52ac7ba31df31` | B1 | 819.07 | 1.21585 | 0.09101 |
| `dc1ffe004dadeba8f938d5519eb154206b667f06` | A2 | 1178.24 | 0.84820 | 0.08630 |
| `dd69cb2467e6fdc322f4f7ad73f52ac7ba31df31` | B2 | 810.62 | 1.22884 | 0.09064 |

Artifacts live under ignored `tools/build/out/bottleneck-{dc1ffe,dd69cb}-ab{1,2}`
(`.csv`, `.log`, `-captures/fps-active.png`). Original switch-script windows could
include a dead/unspawned main player, so their earlier FPS values are not this gate.

Old `dc1ffe` initializes the applied cap sentinel to -1, which collides with the
measurement-only uncapped override and leaves initial FIFO untouched. The isolated
historical header changes only that sentinel to -2, matching the later source,
to establish requested/actual Immediate. Rendering, simulation, and effects are
otherwise untouched. The historical include-prefix encoding was corrected before
comparison; the resulting header edit rebuilt 232 dependent targets, confirming
Ninja tracked it rather than reusing incompatible objects.

The current Qt production baseline under this fixture measured 598.52 / 586.23 FPS
(loop 1.73015 / 1.75852 ms; present 0.59591 / 0.63215 ms). These establish the local
comparison only; remaining historical boundaries, GPU timing, sampled CPU profiles,
all-platform CI, allocation telemetry, and the final >=850 FPS/visual gates remain
mandatory. Removing duplicate event pumping alone has not established the target.

## Current source evidence and remaining Reflex delay

These binaries are `9ce34a39` plus the uncommitted task patch, not untouched
`9ce34a39`. The latest compiler output is `tools/build/out/qt-final-bin`, despite
the build script printing `msvc-Release`; delivery must use the actual linked file.

MSVC Release and CTest 21/21 pass. Material-owner GPU lifetime: three Alinos Perch
cycles, all native counts/retired resources return to zero, no validation errors;
resource check passes. Native present, fallback present, shared present conformance
and RHI conformance pass with the present ring. Golden evidence is under
`bottleneck-owner-golden-{opengl,vulkan}` and `bottleneck-owner-golden-parity.log`;
pixel comparison to historical GL is `bottleneck-owner-prior-golden-pixels.log`.

| Diagnostic condition, same active fixture | Mean FPS | Mean loop ms | Mean present ms |
|---|---:|---:|---:|
| Native Reflex available, Off; owner cache + present ring | 467.19 | 2.14 | 1.35 |
| Extension failure injection, Generic Off | 1203.99 | 0.84 | 0.09 |
| Native Off, implicit Vulkan layers disabled | 467.59 | 2.14 | 1.34 |
| Native Off, diagnostic latencyModeEnable false | 468.69 | 2.13362 | 1.34378 |

The fallback result is isolation evidence, **not** the required native Off FPS
gate. Off measurement markers and matching submission/present IDs must survive the
fix. The swapchain enable flag alone does not explain the delay. Passing a null
sleep-mode pointer (allowed by the Vulkan specification) instead of an explicit
zero Off structure caused this NVIDIA 617.14 driver to fault in nvoglv64.dll at
startup; that experiment was reverted and will not be delivered. Read-only NVAPI
inspection found global FPS limiter disabled, prerender limit 1 and forced VSync
off; no driver settings were changed. Evidence logs are under ignored
`tools/build/out/bottleneck-reflex-*` and `bottleneck-driver-settings.log`.

Historical `3d72fbb` matched active fixture: 804.59 FPS / 1.23756 ms loop /
0.09299 ms present. Historical Qt `cc0d2e90`: 709.74 FPS / 1.44425 ms loop /
0.09961 ms present. Its isolated build additionally requires NOMINMAX and a
fail-loud link stub for an unused, disabled Avalonia photo decoder. The stub was
never invoked in the successful active fixture; no scene renderer code was
changed. Historical source include-prefix encoding and dependency rebuilds were
verified, avoiding stale ABI comparisons. Remaining historical boundaries and
repeat runs remain open.

The user requested selectable exclusive fullscreen, then explicitly deferred it
until after FPS improvements. No exclusive-fullscreen implementation has begun.

## Native Off throughput fix

The delay is inside `vkQueuePresentKHR`, not either Present marker:
diagnostic sampled clock means were 1.30617 ms queue-present versus 0.00011 /
0.00056 ms for PresentStart/PresentEnd. Disabling the startup Off mode call did
not help (471.13 FPS); suppressing KHR presentation IDs alone did not help
(495.26 FPS); suppressing the frame-establishing empty submit did not help
(470.20 FPS). Clearing explicit submission attribution did: 1185.46 FPS with
all measurement markers and KHR presentation IDs kept.

The delivered policy therefore uses **implicit NV frame attribution in Off**:
revision 3 submits chain an ID of zero to clear a previous On/Boost explicit
queue identity. PresentStart supplies the application frame identity by the
specified implicit rules. On/Boost keep explicit application IDs for Qt/RHI
work. This preserves measurement availability, seven markers with matching
nonzero frame IDs, monotonic KHR presentation IDs, and no sleep or generic wait
in Off. It follows the [submission attribution specification](https://docs.vulkan.org/refpages/latest/refpages/source/VkLatencySubmissionPresentIdNV.html),
which explicitly defines returning to implicit mode after all queue IDs clear.
Temporary diagnostic switches and per-present clocks were removed from source.

Final FPS-only fixture checks with native availability confirmed and no failure
injection: `bottleneck-reflex-off-fixed-fps{1,2}.csv`:

| Run | Mean FPS | Mean loop ms | Mean present ms |
|---|---:|---:|---:|
| 1 | 1182.49 | 0.850303 | 0.094508 |
| 2 | 1189.08 | 0.845726 | 0.093549 |

Separate GPU/metrics run (`bottleneck-fps-delivery-gpu.csv`, not FPS acceptance):
mean GPU scene 0.19749 ms. Native timing reports in Off contain nonzero input,
simulation, submit and present timestamps and advancing nonzero application frame
IDs. For example report frame 9234 includes all seven timestamps in order.

Latest source verification: MSVC Release; CTest 21/21; Reflex/presentation,
fallback present, shared present conformance, full RHI conformance and resource
checks; Alinos Perch GPU lifetime 3/3 cycles; Qt Settings-driven active-match
renderer switching 3 times; all exit 0. Golden GL/Vulkan captures and parity 7/7
also rerun on this source (`bottleneck-fps-delivery-golden-*`). The full plan
remains open: remaining historical boundaries, OpenGL work, Vulkan descriptor
group cache/telemetry, deferred acquire proof, cross-platform CI, and the user's
later exclusive-fullscreen request still require work.

Backdrop gate initially captured 1264x681 after requesting 1280x720 on return:
Qt's pending native frame-margin event overrode the immediate resize. The harness
now pumps that transition before restoring client geometry, matching production
WindowMode and the presentation check, and asserts the exact requested size.
The fixed gate (`bottleneck-fps-delivery-backdrop-fixed.log`) passes both backends,
two session cycles, animation, resize/fullscreen/return, maximum difference 1
channel level, and zero live/retired resources. Pixel tolerances were unchanged.

Post-fix CPU sampling (`bottleneck-fps-delivery-cpu.csv.cpu.csv`) captured 6,962
stacks. The previous NtDelayExecution/SleepEx present-delay hotspot is gone from
the leading exclusive samples; remaining work includes VulkanSceneUniforms::Write
and native Windows queue submissions. CSV symbol fields are quoted so demangled
template names remain parseable. This diagnostic run is not FPS acceptance.

## Delivery and dense binding follow-up

Commit `785dfb57bba0343ecf96fd45e76c0514cb4ccdca` was pushed normally to
`develop3_rendering` and fetched back with exact local/remote SHA equality. The
user's `tools/build/out/reflex-bin/FruityPrime.exe` was updated to that tested
build (SHA-256 `CBD297C427F34DCEB2F5B2F78FB3AB726F5172F3D09417FBF62C5EF197583701`).
Remote `build_cpp` run 37187688132 passed all 12 jobs, including MSVC, GCC,
Clang, both Android NDK ABIs and APK packaging. Golden adapter run 37187685513
passed all three jobs. The full audit is still open.

The follow-up removes the 64-word semantic descriptor hash and node map,
replacing it with per-program/group binding versions and a bounded direct
texture-token table. Program identity, recording, resource invalidation epoch,
uploaded uniform version, and texture view/generation/sampler/layout all guard
reuse. A collision replaces CPU lookup data only; it never overwrites a GPU set
already referenced by a recording. Construction allocates the bounded table;
the 10,000-operation alternating-texture test detects zero subsequent CPU heap
allocations. Native Draw-group slots admit 1,024 immutable sets per completed
command slot; other groups admit 32, with the existing ordinary allocation path
preserved on admission failure or exhaustion.

Early descriptor-only FPS runs were 810.71 / 808.13 / 811.33, below the gate.
A same-period re-run of the unchanged delivered executable was also only 730.62
FPS, so those results do not isolate a code regression. They were not accepted
or delivered. Separate diagnostic logs show that fixed slots reduce ordinary
descriptor allocation, but descriptor writes remain frequent when draw constants
change. Profiling still identified uniform string lookup as a major CPU cost.

Uniform writes now use compile-time semantic slots into each program's dense
metadata array. The generated packing, validation, inactive-uniform behavior,
material ownership and change detection are unchanged; string lookup remains for
cold/dynamic callers. Tests exercise all 55 generated constants through both
paths, repeated unchanged writes, and missing semantics without allocation.

| Sequential FPS-only fixture | Mean FPS | Mean loop ms | Mean present ms |
|---|---:|---:|---:|
| Dense uniform/binding candidate, run 1 | 1555.44 | 0.64 | 0.09 |
| Delivered 785dfb57 re-run immediately afterward | 1183.28 | 0.85 | 0.09 |
| Dense uniform/binding candidate, run 2 | 1564.61 | 0.64 | 0.09 |

Evidence: `bottleneck-dense-uniform{,2}-fps.csv` and
`bottleneck-dense-baseline2-fps.csv`. First seven complete warm windows, unchanged
active Sylux/three-bot Alinos Perch fixture, native Reflex available Off,
Immediate/Unlimited, 2560x1439, no sampler/GPU profiling/metrics. MSVC Release
and CTest 21/21 pass. Latest candidate Golden GL/Vulkan captures and parity pass
7/7 with unchanged tolerances (`bottleneck-dense-golden-*`). Resource check
passes with validation enabled, zero errors and zero live native objects.
All follow-up runtime gates pass: full RHI conformance (including four async
readback shutdown/recreate cycles), Reflex Off/On/Boost toggles, fallback present,
shared present conformance, Alinos Perch lifetime 3/3 cycles with zero live or
retired native objects and zero steady host waits, Settings-driven active-match
renderer switching, and backdrop parity. Logs are `bottleneck-dense-{check}.log`,
`bottleneck-dense-lifetime.log`, `bottleneck-dense-switch.log` and
`bottleneck-dense-backdrop.log`. Separate GPU profiling reports mean scene time
0.19844 ms (`bottleneck-dense-gpu.csv`); this diagnostic is excluded from FPS
acceptance. Its allocator counters still show ordinary overflow for groups
admitted at 32 sets, so the full steady-allocation/telemetry goal is not complete.
