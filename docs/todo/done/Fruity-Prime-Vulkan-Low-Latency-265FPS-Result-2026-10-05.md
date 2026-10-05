# Vulkan Low Latency `On` ~265–280 FPS ceiling: result

- Instruction: `Fruity-Prime-Vulkan-Low-Latency-265FPS-Investigation-Fix-Instructions-2026-10-05.md` (this folder)
- Machine: Windows 11, NVIDIA GeForce RTX 5070 Ti, driver 617.14, Vulkan 1.4.351, 2560x1440 at **540 Hz**
- Scene: in-match fixture (`FP_QT_DEMO` + `FRUITY_FPSCHECK_ONLY=1`, `FRUITY_SHOT_ROOM="MP2 HARVESTER"`, Sylux + 3 bots),
  `-fpsmeasure` CSV, rows with `main_active=1`, median of 1 s windows, window focused, exclusive fullscreen
- Status: **fixed**

## Root cause

**Explicit queue attribution.** In On/On+Boost every RHI queue submit carried a nonzero
`VkLatencySubmissionPresentIdNV`. On this driver that makes the Reflex sleep's semaphore signal about a frame
late: native admission took **2.7 ms** (p50; p95 2.87 ms), nearly all of it in `vkWaitSemaphores` after
`vkLatencySleepNV` returned in ~9 us. That is the ~280 FPS ceiling (≈ half the 540 Hz refresh; the reported 265
is the same ceiling on a different scene). It is not a Fruity cap: `minimum_interval_us=0`, present mode
`Immediate`, generic present wait 0.

The code already carried the same finding for Off ("a nonzero explicit ID in Off makes NVIDIA 617.14 serialize
immediate presentation"). The Vulkan spec makes the structure optional and calls the implicit frame attribution
rules sufficient for most applications.

**Fix:** implicit attribution in every mode (zero explicit ID, the markers carry the frame). `vkLatencySleepNV` is
still called once per frame before the input sample, the wait still happens, markers and
`vkGetLatencyTimingsNV` reports stay consistent; nothing about the Reflex contract moved (instruction §19 NGs
respected).

## A/B (Phase 3), VSync off / Immediate, unlimited

| Run | Arm | before (FPS) | after (FPS) | admission p50 |
|---|---|---:|---:|---:|
| A0 | Off | 1152 | 1155 | — |
| A1 | On | 279 | **1137** (-1.6%) | 2715 us → 11 us |
| A2 | On + Boost | 279 | **1118** (-3.3%) | 2655 us → ~11 us |
| B1 | On, swapchain latency opt-in disabled | 279 | 1134 | opt-in is not the cause |
| B2 | On, explicit submission ID (before: disabled / after: re-enabled) | 1132 | 279 | the cause |
| B3 | On, sleep bypassed (diagnostic) | 507 | 1154 | the sleep is not the cause |
| C1 / C2 | On, cap 240 / 144 | 240 / 144 | 240 / 144 | caps exact |

Gate 5 (On ≤ 4%, Boost ≤ 7% vs Off) passes.

## VSync as its own setting

VSync used to exist only as the FPS limit's "Display (VSync)" stop. It is now a separate **VSync** toggle
(`settings.json` `VSync`, `-vsync on|off`); the FPS limit is a number or Unlimited. A saved `display` cap is read
once as VSync on + Unlimited.

With Reflex pacing, FIFO alone did not hold presentation to the display (1136 FPS reported on FIFO), so VSync on
also hands Reflex the refresh period as `minimumIntervalUs` (a lower cap still wins):

| VSync | Low Latency | FPS |
|---|---|---:|
| On | Off | 540.2 |
| On | On / Boost | 461 / 469 |
| On | On, cap 240 | 240 |
| Off | Off / On | 1120 / 1103 |

461 at 540 Hz matches NVIDIA's documented Reflex+VSync driver cap (≈ refresh − refresh²/3600 = 459): instruction
Case D, driver policy, not a Fruity cap.

## Telemetry and arms (Phase 1)

`FRUITY_RENDER_METRICS=1` adds one `[reflex-pacing]` line per 120 frames: mode/effective/provider/authority, cap,
VSync, `minimum_interval_us`, requested/actual present mode, `screen_hz`, image count, revision, frame,
completed/abandoned, and p50/p95/p99/max for the sleep call, the semaphore wait and the whole admission, plus wait
timeouts. Fixed 256-sample arrays; no per-frame allocation or formatting; normal builds read no clock.

Developer-only arms: `FRUITY_REFLEX_EXPLICIT_SUBMISSION_ID=1` (the old behaviour),
`FRUITY_REFLEX_DISABLE_SWAPCHAIN_LATENCY_MODE=1`, `FRUITY_REFLEX_DIAGNOSTIC_BYPASS_SLEEP=1` (announced as "not
Reflex On"). Defaults unchanged apart from the attribution fix.

## Gates

| Gate | Result |
|---|---|
| 1 no hidden cap | `minimum_interval_us=0` for Unlimited, logged |
| 2 no double pacing | `present_wait_count=0`, Native authority skips generic waits |
| 3 cap accuracy | 240 → 240.0, 144 → 144.0 |
| 4 Off baseline | 1152 → 1155 |
| 5 Reflex FPS impact | On -1.6%, Boost -3.3% |
| 6 latency correctness | one sleep per frame, 7 markers per frame, timing reports ordered input → sim → submit → present |
| 7 validation | `-reflexcheck -vkvalidation` PASS errors=0; in-match On/Boost under validation: 0 VUID / SYNC-HAZARD / DEVICE_LOST |
| renderer switch | `FRUITY_SWITCHCHECK=1 -shellshot` PASS |
| CTest | 25/25 (`FruityPrime.VulkanNvidiaReflex`: interval mapping incl. VSync floor, implicit attribution, explicit A/B monotonic IDs, bypass, timeout poll = one sleep, pacing summary) |
| `-frametimingcheck` | exit 0 |

## Open

- Photon (display-side) latency was not measured; the CPU-side markers are consistent.
- One driver and one GPU. Other drivers may not serialize on explicit IDs; the arm exists to check.
- A GPU-bound scene could not be produced (render scale caps at 100%), so Reflex's queue reduction under GPU
  saturation with implicit attribution is covered by the spec, not by a measurement here.
- The provider-preference UI (§14.2) was not needed: the loss was a Fruity-side attribution fault, not native
  Reflex pacing.
