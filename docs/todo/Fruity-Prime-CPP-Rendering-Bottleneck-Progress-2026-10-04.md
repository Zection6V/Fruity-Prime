# Rendering bottleneck plan — execution evidence

Plan: [audit and fix plan](Fruity-Prime-CPP-Rendering-Bottleneck-Audit-and-Fix-Plan-2026-10-04.md).
Starting production HEAD: `9ce34a391ad8bb2d08848e8189e413eccf28d2a4`.

Status: **CLOSED** (2026-10-04). All four must-close gates of the
[final audit](Fruity-Prime-CPP-Rendering-Final-Audit-and-Residual-Fix-Instructions-2026-10-04.md)
pass; see "Final closure" at the end. Remaining items are optional follow-ups.

| Requirement | Status / authoritative evidence |
|---|---|
| Phase 0, `dc1ffe` → `dd69cb` A/B before renderer edits | Passed matched active fixture, two sequential A/B pairs before renderer edits; see evidence below. |
| Remaining historical boundaries | `3d72fbb` and `cc0d2e90` measured below; `ec6e98b`, `f2597d92`, `e03978dc`, and clean `9ce34a39` are optional forensic work, not a blocker (final audit §16). |
| Phase 1, uniform state by program/block/slot generation | Delivered dense semantic slots and recording/version stamps in 4c9db2c; mutation/lifetime/RHI/resource/Golden gates pass. Warm diagnostic string lookups and uniform heap allocations are zero. |
| Phase 1, descriptor group dirty/version state | Delivered fixed program/group state and bounded texture-token table in 4c9db2c. Full semantic hash/tree lookup removed from steady DrawScene; measured heap allocations zero. |
| Phase 1, material owner identity/version state | Implemented stable RenderItem owner IDs, paged owner CPU data/version state, and two completion-safe GPU slices per owner. Full material content hashing removed; same-recording mutation uses immutable fallback. Unit tests and resource/lifetime gates pass. Remaining descriptor hot-path work pending. |
| Phase 1, unchanged update/bind suppression, push constants, mutation/lifetime correctness | Preserved; unit and RHI mutation/lifetime checks pass, including resumed draws and invalid borrowed bindings. |
| Phase 2, program-owned uniform locations and alpha values | Delivered d59b0b98; warm current-program and uniform-location queries zero. Other uniform-write redundancy remains measurable. |
| Phase 2, device-owned immutable limits | Delivered d59b0b98; queries gated by supported features and resolved at context initialization. |
| Phase 2, retained VAO and explicit Qt interop invalidation | Delivered d59b0b98; unchanged VAO native binds suppressed; external program/VAO/state disruption recovers without caller rebinds. |
| Generic OpenGL binding snapshot reuse without system heap churn | Delivered shared immutable snapshots in fixed group storage; warm Draw/DrawIndexed C++ allocations zero. |
| Phase 3, submit helper high-water storage and thread/reentrancy contract | Implemented retained signal storage, constructing-thread ownership and reentrancy rejection; native-array/failure/high-water/foreign-thread tests pass. |
| Phase 3, window-owned event pump | Delivered; Qt owns one event pump per accepted measured frame on both backends. Settings-driven active-match switches pass. |
| Phase 3, nonblocking acquire/image/present retirement; bounded frame-slot reuse | Delivered d10e33d9 deferred reacquisition proof with retirement ceiling; forced fallback releases six retired chains after 26 completed proofs. Frame-slot and cleanup waits separately zero in this fixture. |
| Phase 4, Off throughput / Generic On latency / native Reflex single authority | Passed; runtime failure matrix below. Intentional On waits retained. |
| Sampling profiler and allocation/API/wait/event telemetry | Windows sampling plus compile-time opt-in phase/new/API counters implemented; clocks sampled at 1/128 or 1/256 frames, totals emitted after shutdown. Application C++ new scope and foreign allocation exclusions are explicit; see latest evidence below. |
| Same room/hunter/bots/spawn/camera/size/render settings; warmup excluded | Historical and Qt diagnostic fixture uses first spawn, Sylux +3 bots, Alinos Perch, 2560x1439, scale 100, fog on, cel/FPS display off, Low Latency Off, Unlimited, requested/actual Immediate. |
| FPS-only Vulkan >=850 FPS, <=1.18 ms; goal >=950 FPS | Passed delivered 4c9db2c/d10e33d9 at 1552.60/1558.13 FPS; current follow-up 1642.20 FPS, loop 0.60456 ms. Profilers/validation/counters disabled in acceptance runs. |
| GPU scene and present CPU time, OpenGL CPU/GPU comparison | Vulkan 0.19844 ms and OpenGL 0.637106 ms in separate prior diagnostics. OpenGL CPU loop still exceeds GPU scene time; further frontend redundancy is measurable. |
| Windows MSVC, Linux GCC, macOS Clang, Android NDK, full CTest | d10e33d9 remote build_cpp 37201956163 passes all 12 jobs. Current telemetry follow-up MSVC/CTest 22/22 pass; final revision CI pending. |
| RHI conformance, resources, validation, GPU lifetime cycles | Current task source passes full RHI conformance, resource validation and 3 GPU lifetime cycles; zero live/retired objects, validation errors 0, steady lifetime host waits 0. |
| Immediate/FIFO/Mailbox, resize/fullscreen/minimize/restore, renderer switches | Passed on the final revision; see Final closure. |
| Off/On/Boost, native/fallback/toggles, no double pacing | Current source native Reflex check passes FIFO/Immediate/Mailbox, 5 cap choices, Off/On/Boost/Off transitions, resize/minimize/restore, 268 completed and 1 abandoned frame, validation 0 errors. Runtime failure/fallback matrix passes (extension, present-id, semaphore, set-mode, sleep, wait). |
| Golden Capture and GL/Vulkan parity: HUD, transparency, decals, particles, trails, fade, disruption, backdrop distortion | Latest source: GL/Vulkan Golden 7/7 pass, no pixels differ beyond 8 levels; owner-cache GL exactly matches previous GL pixels 7/7. Qt GL context ownership fixed to release the device before destroying its native context. Backdrop distortion parity passes two complete session cycles, animation/resize/fullscreen return, max difference 1 level, no errors or live resources. |
| No draw/effect omission, CPU rendering/copies, synchronous readback, hot-path queue/device idle | Passed: all CI static GL/RHI audits locally, Golden 7/7, host wait/device idle 0 in diagnostics. |

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

## OpenGL context ownership and memory-admission follow-up

The dense Vulkan work was delivered as
`4c9db2c37acd957a6785525dd78f85423943a62a`; post-commit FPS was 1552.60.
Remote `build_cpp` run 37199326759 passed all 12 jobs, including Windows,
Linux, macOS, both Android ABIs and APK. The tested executable and PDB were
copied to the user's `reflex-bin` (executable SHA-256
`55C5014C70E0BBF46836E810A8EEB935879F56A99ABA92B1BE2E687190534DD6`).

OpenGL now resolves `mat_alpha`/`alpha_test` locations when a program is linked,
retains their values per program, and tracks the bound program and VAO in the
context owner. Scene constants and small constants share the same alpha cache.
Ordinary draws keep their VAO; deletion invalidates it. Device initialization
queries immutable binding limits/alignment only for supported features. Generic
binding sets retain a shared immutable snapshot in a fixed group array rather
than copying a description and allocating nodes on each bind. Expired borrowed
resources are still rejected, while releasing public set/layout wrappers remains
valid. The fallback sampler scratch retains its capacity.

Qt Quick and the Android overlay use explicit external-GL boundaries. Entering
interop binds VAO 0 before external attribute/index writes and invalidates the
context state; leaving invalidates again. The next RHI draw restores its state.
The conformance check deliberately changes native program/VAO/viewport/scissor
state through the backend and verifies recovery without caller rebinds.

An initial FPS-only run improved OpenGL from 534.04 to 565.78, but CPU sampling
then isolated repeated NVX free-memory queries during transient buffer orphaning.
Memory admission now takes a fresh driver snapshot on the first allocation of
each render frame, conservatively charges every admitted allocation/orphan in
that frame, and refreshes on the next frame. Explicit snapshots and admission
outside a render frame still query live driver counters. EndFrame/WaitIdle/Close
end the admission scope. Native storage-error checks and budget safety reserves
remain enabled. Unit checks cover cumulative rejection, next-frame exhaustion,
cold live queries, forced ceiling invalidation and closed-owner rejection.

| Sequential FPS-only fixture | Mean FPS | Mean loop ms | Mean present ms |
|---|---:|---:|---:|
| Delivered dense Vulkan revision, OpenGL baseline | 534.04 | 1.960694 | 0.085528 |
| Program/VAO/binding ownership, before budget fix | 565.78 | 1.854803 | 0.085427 |
| Complete OpenGL candidate, run 1 | 826.35 | 1.227885 | 0.088368 |
| Complete OpenGL candidate, run 2 | 840.15 | 1.211939 | 0.084151 |
| Complete candidate, Vulkan regression check | 1550.44 | 0.642849 | 0.092804 |

Evidence: `bottleneck-opengl-before-fps.csv`, `bottleneck-opengl-owner-fps.csv`,
`bottleneck-opengl-budget-fps{,2}.csv` and
`bottleneck-opengl-budget-vulkan-fps.csv`. These use the unchanged active
Alinos Perch fixture, first seven complete warm windows, Immediate/Unlimited,
Low Latency Off, no metrics, sampling or GPU profiling. Separate diagnostic
GPU runs report OpenGL scene time 1.117615 ms before the budget change and
0.637106 ms afterward. These timings include driver submission bubbles and
do not establish a shader-work reduction. The geometry/effects are unchanged.
The CPU sampler's exclusive `NtGdiDdDDIEscape` count fell from 2469 to 56;
the latter diagnostic collected 6979 stacks. Inclusive samples overlap and
are not summed as percentages. CPU loop cost still exceeds scene GPU time,
so no claim of a completely GPU-bound OpenGL frontend is made.

MSVC Release and CTest 21/21 pass. Latest candidate Golden captures/parity
pass 7/7; all seven OpenGL PNG SHA-256 hashes are exactly equal to the delivered
dense revision's captures. Full RHI conformance, explicit interop disruption,
Vulkan resource validation (errors=0, live=0), shared presentation conformance,
Qt Settings-driven active-match renderer switching, and backdrop parity pass.
Alinos Perch OpenGL lifetime passes 3/3 cycles with zero live/retired resources
and zero waits in the final 15 frames of each cycle. Runtime logs use the
`bottleneck-opengl-budget-` prefix. Remote CI for this OpenGL follow-up is pending.
The full plan remains open, including remaining historical boundaries,
whole-DrawScene allocation/phase/API telemetry, native descriptor-group overflow,
deferred acquire-completion proof, final latency/failure matrix and exclusive
fullscreen (last, per the user's priority).

## Deferred fallback acquisition completion

OpenGL follow-up `d59b0b98b41e0a1d42196d1e2d9246adefac14b0` was pushed and
fetched with exact SHA equality. A fresh committed build measured 815.19 FPS
on OpenGL and was copied with its PDB to `reflex-bin`; executable SHA-256
`D43E3D9E619050F0886275AB3D7C7D2E4905654B4BA1BDEF8621C5B77BBC68F2`.
Remote `build_cpp` run 37201289117 is still running; Windows/static jobs passed.

Fallback presentation no longer creates or immediately waits/resets an acquire
fence. It uses the existing GPU wait on `imageAvailable`; completion of the
submission that consumed that semaphore proves the reacquired image's previous
present has finished. This follows the [Khronos semaphore-reuse guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
It does not treat a queue fence as proof of the *new* presentation finishing.
Image-indexed present semaphores and the maintenance present-fence ring remain.

Each frame captures the retirement serial ceiling at acquisition and becomes
pending only after successful queue submission. The ceiling is consumed only
after observing its actual submission fence. It releases only the older retired
prefix, so delayed/out-of-order completion cannot destroy a later replacement.
Current-image presentation history remains conservative. The normal reuse path
polls the frame fence first and waits only if that bounded command slot is busy;
teardown waits have a separate count. Resize/teardown still drain the necessary
work. The existing unextended shutdown limitation documented by Khronos is not
claimed to be solved by an unrelated queue fence.

Tests cover unsubmitted/failed-submit records, duplicate submission, still-pending
slot reuse, newer/older proof completion order, a never-presented image, and
fresh slot reuse. MSVC Release/CTest 21/21 pass. The validation-enabled forced
fallback diagnostic passes with six retired chains released, 26 completed
reacquisition proofs, zero frame-slot waits and zero cleanup-frame waits
(`bottleneck-deferred-acquire-fallback.log`). Native Reflex presentation also
passes Off/On/Boost toggles, resize, modes, fullscreen, minimize/restore and clean
shutdown with zero validation errors. Full RHI/resource/presentation conformance,
Alinos Perch lifetime 3/3, Settings-driven renderer switching, backdrop parity,
and Golden parity 7/7 pass (`bottleneck-deferred-acquire-*`).

An initial FPS-only candidate measured 1419.87 FPS. A controlled sequential
recheck of the unchanged delivered d59 build measured 1544.60 FPS
(loop 0.645176 ms, present 0.092593 ms); the deferred candidate immediately
afterward measured 1558.13 FPS (loop 0.639482 ms, present 0.093047 ms).
Maintenance is enabled in this fixture, so the fallback change is not credited
as a throughput gain. The full audit's remaining telemetry/allocation,
historical-boundary, failure-matrix and final fullscreen work is still open.

## Sampled phase telemetry and remaining native allocation

Deferred-proof revision `d10e33d97dbcbbe06720313796b6bfe2cd0ae700` is delivered
to `reflex-bin` (EXE SHA-256
`C6F463F4AEA18B2DAF5D708B03E81C2F00D620207D6F1F0B0FB04876CA3FA106`).
Remote `build_cpp` run 37201956163 passed all 12 jobs. The prior d59 run was
cancelled by that newer run; it is not counted as an all-platform success.

`FRUITY_PERF_TELEMETRY` and `FRUITY_NEW_TELEMETRY` are CMake options, both OFF
by default. NEW implies PERF and replaces application C++ new/delete only in
that diagnostic binary. It does not count malloc, foreign DLL or driver heap
operations. `FRUITY_PHASE_STATS=128|256` selects the timing cadence and writes
`<fps CSV>.phases.csv` after the session ends. Ordinary builds reject that opt-in
explicitly, and compile the hot scopes/counters out. Both native and Qt object
owners receive the diagnostic definition; a development run missing the Qt
definition had zero accepted frames and was rejected, not used as allocation
evidence. Tests exercise both cadences, eligibility changes, discarded warmup
and unpresented frames, nested inclusive new counts, alignment, void/result
wait operations and thread-owner release. Phase times/allocations are inclusive;
overlapping rows must not be summed.

The initial valid Vulkan diagnostic (`bottleneck-telemetry-vulkan`) accepted
11,462 warm active frames. DrawScene, uniform, material, descriptor and submit
scopes all had zero application new operations, but the entire scene-render
scope had 6,937,609. Native descriptor allocations were 140,611, all from the
32-set Frame-group reservation. Frame reservations are now bounded at 128
sets per completed command slot; Draw remains 1,024 and other groups 32.
Capacity/admission failure retains the existing ordinary fallback. Vulkan
transient geometry retains its index sequence/output scratch and copies the
result into submission-owned upload storage before returning. GPU work never
borrows those CPU vectors.

Final Vulkan diagnostics accepted 12,187 frames at 1/128 and 12,105 at 1/256
(`bottleneck-telemetry-final-vulkan` and `bottleneck-telemetry-256-vulkan`). Both
report zero DrawScene/uniform/material/descriptor/submit C++ allocations, zero
native descriptor allocation calls/sets and group overflows, zero vector growth,
zero host waits/device idle calls, no uniform string lookups, five queue submits
and one event pump per accepted frame. Scene-render allocations fell to exactly
two per frame (96 bytes); scene traversal still allocates about 81 times/frame.
Whole-frame heap freedom is not claimed.

Desktop OpenGL upload binding state now belongs to the current context, with
wrapper bind/delete notifications and explicit external-boundary invalidation.
The next upload after interop queries once, then preserves that caller's binding;
normal uploads reuse known state. GLES retains its existing live-query contract
because its fixed-function adapter also issues raw binds. RHI conformance checks
two uploads after external disruption and upload after deletion, verifies the
actual native binding, then resumes the original draw without caller rebinding.
The final OpenGL diagnostic accepted 6,422 frames: zero Draw/DrawIndexed C++
allocations, uniform-location/current-program queries, host waits and device idle;
one event pump per frame. Integer queries fell from approximately 459/frame to
22/frame (`bottleneck-telemetry-final-opengl` versus
`bottleneck-telemetry-query-opengl`), including two live NVX budget counters per
frame. This does not claim all GL query APIs or required storage-error checks
are gone. Uniform writes remain approximately 8,050/frame.

With both diagnostic options OFF, a sequential unchanged d10 OpenGL baseline
measured 842.83 FPS (loop 1.208281 ms, present 0.084222 ms), followed by candidate
833.01 (loop 1.223388 ms, present 0.084442 ms). This does not establish a separate
throughput gain from upload binding retention. Vulkan candidate measured
1,642.20 FPS (loop 0.604562 ms, present 0.092049 ms). Same active fixture and
first seven complete warm windows; no profiling, validation or counters in these
acceptance runs. Separate GPU diagnostics give scene time 0.199275 ms Vulkan and
0.647106 ms OpenGL (`bottleneck-telemetry-gpu-*`), preserving the prior workload.
OpenGL CPU loop still exceeds GPU time, so a fully GPU-bound frontend is not
claimed.

MSVC Release, CTest 22/22, all static GL/RHI audits, RHI/resource/presentation
conformance, forced-fallback retirement, native Reflex toggles, both backends'
Alinos Perch lifetime 3/3, active-match Settings renderer switches, backdrop
parity and Golden parity 7/7 pass (`bottleneck-telemetry-*`). OpenGL Golden PNGs
match the delivered dense revision's SHA-256 exactly, 7/7. Validation errors
and released live/retired GPU objects are zero. Remaining historical boundaries,
final runtime failure/fallback matrix, final revision CI and selectable exclusive
fullscreen remain open.

## Final closure

Source: `438624af` plus the OpenGL per-program uniform value state and a `wait`
Reflex failure injection (this commit). MSVC Release, CTest 23/23 (new
`FruityPrime.OpenGlUniformState`).

OpenGL uniform writes: every scene-shader uniform goes through
`OpenGlUniformState`, owned by the linked native program and indexed by the
generated `SceneShaderAbi` semantic. A write is suppressed only when program,
location, kind, byte size and exact bytes all match the last native write
(memcmp, no epsilon, no pointer identity). Any program mismatch or external-GL
boundary invalidates. GLES keeps every write, because its fixed-function
adapter also writes uniforms. Throughput is unchanged within noise, so no gain
is claimed: same-session A/B, first seven warm windows, diagnostics off, OpenGL
826.05 (438624af) vs 831.04 FPS, Vulkan 1644.11 vs 1632.99 FPS. An earlier
970 FPS Vulkan sample was taken immediately after a killed stuck process and is
discarded; the immediate rerun is the value above.

Must-close gates:

1. Reflex runtime failure matrix: `FRUITY_REFLEX_TEST_FAILURE` = extension,
   present-id, semaphore, set-mode, sleep, wait with `-reflexcheck`; 60/60 PASS
   each, exit 0. Every native failure reports requested On + Boost, effective
   Generic On, provider/authority Generic, Boost off, with the reason; FIFO,
   Immediate, Mailbox, five caps, resize/fullscreen/minimize/restore, clean
   shutdown, validation errors 0. Active-match Settings toggles and renderer
   switches with forced fallback were run on 438624af
   (`bottleneck-438624af-authority-*`).
2. `golden_parity_adapter` run 37205557611 on 438624af: success, including
   Phase 4 Windows runner-owned CMake build.
3. Presentation: `-presentconformance` OpenGL 2 modes / Vulkan 3 modes
   (Immediate, FIFO, Mailbox) kept, resize, minimised frame unblocked, restore;
   `-vulkanpresentcheck`, `-vulkanpresentfallbackcheck`, `-vulkancheck`,
   `-vulkanresourcecheck`, `-rhiconformance` all exit 0. Active-match renderer
   switch passes (`switchcheck PASS`).
4. Shipping hot path: Low Latency Off, Unlimited, Immediate, active match,
   validation/profile/telemetry off, numbers above. GPU lifetime Alinos Perch
   3/3 on both backends. Static audits (phase4 legacy GL, phase5 shader
   interface, phase9 frontend GL, phase11 boundaries, RHI isolation) pass.
   Diagnostic allocation/wait counts are those recorded under "Sampled phase
   telemetry"; no hot-path code other than the GL uniform filter changed since.

Optional follow-ups, deliberately not done here: Traversal ~81 allocations/frame
callsite histogram, SceneRender 2 allocations, Vulkan 5 submits/frame reason
split, OpenGL BindingSlot precompute, historical SHAs, selectable exclusive
fullscreen.
