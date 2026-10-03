# Fruity Prime `3d72fbb2` — NVIDIA Reflex + Audit Fixes: Independent Audit

## Resolution status (2026-10-04)

The review below describes the historical audited SHA. On checking
`131544595f7d483d2ef778a66286fefe217ecf71`, items 1–3 were still open;
the macOS narrowing and Android incomplete-type corrections were already present.

Implemented corrections:

- Measurement availability and native pacing are separate. Off opens a measurement
  frame and emits all seven markers with its submission/present ID, without native
  sleep or generic GPU pacing. The desktop frame gate opens measurement before
  returning for Off; native runtime failures still fall back in the same frame.
- Timing queries use completed measurement frames, once per 120 completions.
  Duplicate closure and abandoned frames do not advance the cadence.
- `AbandonLowLatencyFrame()` clears readiness, pending admission, the marker mask
  and current identity without fabricated Present markers or completion. Final
  acquire failure, unavailable direct acquisition/presentation and zero-sized
  desktop frames abandon the logical frame. Swapchain recreation within a frame
  continues to preserve the admitted identity.
- The controller regression covers Off markers/attribution, cadence before and
  after exactly 120 sleeps, duplicate closure, abandonment in every mode,
  pending-sleep mode changes, resize and optional native failures. Desktop CI
  now builds and runs this regression on Windows, Linux and macOS.

Local verification (Windows, MinGW Release, Qt Quick, RTX 5070 Ti):

- Full native build passed; CTest **21/21 passed**.
- `-reflexcheck -noupdate`, `FRUITY_RENDER_METRICS=1`: **passed**, validation
  enabled, **0 errors**; 268 completed measurement frames, 1 abandoned frame,
  2 timing queries, 149 native sleeps, 1879 markers. FIFO/Immediate/Mailbox,
  Off/On/Boost, frame caps, resize/fullscreen and minimize/restore were exercised.
  Both sampled driver timing reports contain all seven nonzero timestamps in
  input/simulation/submit/present order.
- RHI isolation, frontend OpenGL, dependency classification, shader-interface
  and legacy OpenGL static audits passed; `git diff --check` passed.
- `-vulkanresourcecheck -noupdate`: **passed**, validation enabled, **0 errors**,
  no live/retired resources left at shutdown.
- `FRUITY_REFLEX_TEST_FAILURE=sleep -reflexcheck -noupdate`: **passed**;
  native failure preserved requested Boost and selected Generic On, with **0
  validation errors**.

Cross-platform CI for the implementation commit is pending. Nsight/Reflex
verification tooling is not installed here; API counters and Vulkan validation
verify the lifecycle contract, without claiming external display-latency measurements.

---

## Scope

Repository: `Zection6V/Fruity-Prime`  
Branch: `develop3_rendering`  
Exact audited commit: `3d72fbb275a197ae90d2442c41a7d0a57f0afb32`  
Parent: `ff753f9b643e86c936632e32b7521c4b1c25d8ee`

This review is read-only and audits the exact pushed source.

## Result

The core native Reflex pacing path and all three prior audit corrections are implemented coherently.

Verified source-level properties:

- `VK_NV_low_latency2` and `VK_KHR_present_id` are probed/enabled independently of the user's current mode.
- revision >= 2 is required for the current low-latency2 API contract; explicit queue-submission attribution is separately gated to revision >= 3.
- the Reflex sleep semaphore is dedicated and is not reused as the queue-completion timeline.
- native sleep completes before the fresh desktop input sample.
- healthy native Reflex owns pacing; the generic `WaitForLatestSubmission()` path does not run in parallel.
- a runtime native failure is re-resolved to Generic On in the same frame.
- `VkLatencySubmissionPresentIdNV` preserves any pre-existing `VkSubmitInfo2::pNext` chain.
- revision >= 3 submission attribution is explicitly cleared with present ID 0 when there is no current Reflex frame.
- present acceptance is independent from swapchain-rebuild status.
- `VK_SUBOPTIMAL_KHR` is represented as `ResizeRequired + accepted=true`.
- Settings Cancel/Escape restores the live low-latency setting.
- the descriptor fixed-page cache is current-command-slot, first-use, 32-set, bounded, completion-recycled, and fail-soft for native capacity/allocation failures.
- program destruction retires fixed descriptor identities.
- the old device-global eager scene-layout broadcast is gone.

No source-level stale descriptor, use-after-free, duplicate pacing-authority, or push-constant regression was found.

Three lifecycle/telemetry corrections remain, and the exact SHA currently has cross-platform CI build blockers.

---

# 1. P1 — Reflex markers disappear when Low Latency is Off

## Proven source path

`VulkanNvidiaReflex.hpp`:

```cpp
bool Active() const noexcept
{
    return _available && _swapchain
        && _mode != LowLatencyMode::Off;
}

std::uint64_t FrameId() const noexcept
{
    return Active() && _ready ? _frameId : 0;
}
```

`VulkanNvidiaReflex.cpp`:

```cpp
bool VulkanNvidiaReflex::BeginFrame()
{
    if (!Active()) return true;
    ...
}
```

and:

```cpp
void VulkanNvidiaReflex::Mark(LowLatencyMarker marker)
{
    if (!FrameId()) return;
    ...
}
```

`Renderer.cpp::BeforeFrame()` additionally returns before `BeginLowLatencyFrame()` when the effective mode is Off:

```cpp
auto state = ResolveLowLatency(...);
if (state.effective == LowLatencyMode::Off)
    return true;
```

Therefore, with a usable NVIDIA controller but UI mode `Off`:

```text
no application frame ID
no InputSample marker
no SimulationStart/End markers
no RenderSubmitStart/End markers
no PresentStart/End markers
```

This does not break On/Boost pacing, but it makes Reflex latency telemetry incomplete. NVIDIA's current Reflex verification guidance expects markers to continue while Low Latency mode is Off so baseline PC-latency measurements can be compared against On / On + Boost.

## Required correction

Separate:

```text
measurement availability
```

from:

```text
pacing active
```

For example:

```cpp
MeasurementAvailable()
PacingActive()
```

A frame identity should open whenever native Reflex measurement is available for the live swapchain, including Off. Off must not call `vkLatencySleepNV`.

Conceptually:

```cpp
bool BeginFrame()
{
    if (!MeasurementAvailable())
        return true;

    if (_ready)
        return true;

    _frameId = ++_sequence;
    _markers = 0;

    if (!PacingActive())
    {
        _ready = true;
        return true;
    }

    // existing sleep + bounded wait
}
```

`FrameId()` should depend on an open measurement frame, not on `_mode != Off`.

The desktop frame gate must invoke the native frame-begin hook before returning for `effective == Off`. Default/non-native swapchains can remain no-ops.

Do not introduce generic GPU pacing in Off mode.

## Acceptance

For usable NVIDIA/Vulkan:

```text
Off:
  sleepCalls unchanged
  lowLatencyMode = false
  expected frame markers emitted
  present uses the same frame ID

On:
  same marker contract
  native sleep occurs

On + Boost:
  same marker contract
  native sleep occurs
  lowLatencyBoost = true
```

---

# 2. P2 — latency timing polling can run every frame in Off mode

`VulkanNvidiaReflex::PollTimings()` currently uses:

```cpp
if (!Available()
    || !_swapchain
    || !std::getenv("FRUITY_RENDER_METRICS")
    || _stats.sleepCalls % 120)
{
    return;
}
```

This opens the timing-query gate when `sleepCalls` is `0`, `120`, `240`, etc.

Because `FinishFrame()` runs independently of whether the sleep counter changed:

- starting in Off with `FRUITY_RENDER_METRICS=1` can query timings every presented frame because `0 % 120 == 0`;
- switching Off immediately after exactly 120/240/... native sleeps leaves the counter at a multiple of 120, again allowing a query every presented frame.

This is diagnostics-only overhead, but it violates the intended 1/120 cadence.

## Required correction

Use a dedicated completed measurement-frame counter, not `sleepCalls`.

For example:

```cpp
std::uint64_t _completedMeasurementFrames = 0;
```

Increment exactly once when a logical measurement frame closes and poll at the desired cadence.

Also prevent the same counter value from triggering more than once.

After fixing Off-mode markers, timing polling should remain meaningful in Off even though no native sleep occurs.

---

# 3. P2 — admitted Reflex frame is not explicitly abandoned when acquire never reaches Present

Normal native admission:

```text
Renderer.cpp::BeforeFrame()
-> BeginLowLatencyFrame()
-> active frame ID
```

Later `VulkanGraphicsDevice.cpp::PresentWindow()` does:

```cpp
const auto acquired = swapchain.TryAcquireTexture();

if (!acquired.texture)
    return {acquired.status, acquired.failure};
```

This exits before `swapchain.TryPresent()`, while `VulkanNvidiaReflex::FinishFrame()` is tied to the actual present path.

A concrete race is:

```text
BeginLowLatencyFrame succeeds
-> input/simulation/rendering
-> window becomes iconified or drawable becomes unavailable
-> TryAcquireTexture returns TemporarilyUnavailable
-> PresentWindow returns early
-> Reflex _ready/frame ID survive
```

The existing `-reflexcheck` minimizes only after active draw/present frames have completed, so it does not exercise this race.

If simulation continues while unavailable, later CPU frames can reuse the old frame ID and marker de-duplication suppresses fresh per-frame markers until a later successful present.

## Required correction

Add an explicit abandon/end-without-present operation at the swapchain/RHI boundary.

For example:

```cpp
virtual void AbandonLowLatencyFrame() {}
```

Vulkan should clear:

```text
current readiness
marker mask
current logical frame identity
```

without manufacturing PresentStart/PresentEnd or treating the frame as accepted.

Call it when a frame was admitted but final acquire cannot reach an actual present.

Prefer a distinct `AbandonFrame()` over blindly calling `FinishFrame()` if `FinishFrame()` semantically means a completed presented frame.

## Acceptance

Test:

```text
BeginFrame succeeds
Input + Simulation markers emitted
final acquire -> TemporarilyUnavailable
frame abandoned
next admitted frame gets a newer ID
fresh markers are emitted
no fake Present markers for the abandoned frame
```

---

# 4. Exact-SHA CI is currently not green

Exact SHA `3d72fbb275a197ae90d2442c41a7d0a57f0afb32` has terminal PR `build_cpp` run:

`37108531152`

Overall result:

```text
failure
```

Successful jobs include:

```text
Windows / MSVC
Linux / GCC
frontend OpenGL static audit
RHI backend isolation
OpenGL dependency classification
shader interface static audit
legacy OpenGL static audit
Android native build contract
```

Failures:

```text
macOS / Clang
Android NDK / arm64-v8a
Android NDK / x86_64
```

Android APK was skipped because the ABI builds failed.

At audit time:
- later exact-SHA `build_cpp` run `37109373686` is in progress;
- Golden Parity adapter run `37108528709` is in progress;
- both shared-adapter compile jobs in the Golden Parity run have passed.

Do not describe this SHA as cross-platform green yet.

---

# 5. macOS blocker — narrowing conversion

Failed job:

`111161707466` — `macOS / Clang`

Error:

```text
OpenGlGeometry.cpp:144:49:
non-constant-expression cannot be narrowed
from unsigned long to std::uint32_t
```

Current source:

```cpp
const auto slot =
    static_cast<std::uint32_t>(bufferCount);

buffers[bufferCount++] =
{
    slot,
    components * sizeof(float)
};
```

Narrow explicitly:

```cpp
const auto stride =
    static_cast<std::uint32_t>(
        components * sizeof(float));

buffers[bufferCount++] =
{
    slot,
    stride
};
```

`components` is the fixed small attribute component count in this path.

`OpenGlGeometry.cpp` is byte-identical at the parent and audited SHA:

`d77185c894f08cdaeb19dbf1cc69070906e63a1a`

So this is not introduced by the Reflex commit, but it blocks exact-HEAD macOS verification.

---

# 6. Android blocker — incomplete `FramePerformance` type

Failed jobs:

```text
111161825459 — Android NDK / arm64-v8a
111161825440 — Android NDK / x86_64
```

Error:

```text
invalid application of 'sizeof' to an incomplete type
MphRead::Mods::Diagnostics::FramePerformance
```

`Renderer.hpp` forward-declares `FramePerformance`, while `RenderWindow` owns:

```cpp
std::unique_ptr<FramePerformance> _performance;
```

`RendererAndroid.cpp` defines `RenderWindow` construction/destruction without including the complete `FramePerformance` definition.

Narrow correction:

```cpp
#include "Mods/Diagnostics/FramePerformance.hpp"
```

in `RendererAndroid.cpp`.

Keep the forward declaration in `Renderer.hpp`.

`RendererAndroid.cpp` is byte-identical at the parent and audited SHA:

`f3609a8177224e6cc9f2698e5694caa1d260dc8f`

The Reflex commit's `Renderer.hpp` diff only adds the new input-sampling virtual hooks in the relevant area; it did not introduce the incomplete-type ownership pattern.

After fixing this first Android compile blocker, rerun both ABIs because later failures may currently be masked.

---

# 7. Accepted items

## Native/generic pacing authority

Accepted. Healthy native Reflex and Generic `WaitForLatestSubmission()` are mutually exclusive. A native runtime failure re-resolves the same frame to Generic.

## Revision handling

Accepted. Revision >= 2 is required for the current low-latency2 API surface and explicit submission attribution is gated to revision >= 3. Existing submit `pNext` chains are preserved.

## Dedicated sleep semaphore

Accepted. Reflex sleep has its own timeline semaphore and bounded waits do not repeat `vkLatencySleepNV` for the same pending frame.

## Present acceptance split

Accepted. `ResizeRequired` and `accepted` are independent. `VK_SUBOPTIMAL_KHR` can be accepted while requesting rebuild; rejected out-of-date presents are not treated as accepted.

## Settings rollback

Accepted. The settings view snapshots the original live value and restores it from the shared Cancel/Escape `Close()` path. Commit/Save persists the selected value.

## Descriptor lazy reservation

Accepted. Production now reserves 32 sets on first use of a program/group in the current command slot, recycles only after completion, and falls back to the ordinary allocator instead of making the optional fast path renderer-fatal.

---

# 8. Recommended close order

```text
1. Keep the current native pacing implementation.
2. Decouple Reflex measurement frame identity/markers from Off/On pacing state.
3. Replace sleepCalls timing cadence with completed measurement-frame cadence.
4. Add explicit abandoned-frame handling for pre-present acquire-unavailable paths.
5. Fix the macOS narrowing compile error.
6. Include FramePerformance.hpp in RendererAndroid.cpp.
7. Re-run exact-SHA Windows/Linux/macOS/Android CI to terminal state.
8. Re-run -reflexcheck and Vulkan validation after lifecycle changes.
9. Validate marker quality with NVIDIA Reflex verification tooling / NSight if available.
```

## Final assessment

The difficult part — native pacing ownership, same-frame generic fallback, present attribution, and the descriptor fixes — is in good shape. A rewrite is not indicated.

The remaining Reflex work is concentrated at the frame-lifecycle boundary:

```text
measure even when pacing is Off
+
close/abandon every admitted logical frame exactly once
+
sample timing reports on a real frame counter
```

The previous three audit fixes are substantively resolved. The branch is not yet cross-platform verified because macOS and both Android ABI jobs have concrete compile failures.
