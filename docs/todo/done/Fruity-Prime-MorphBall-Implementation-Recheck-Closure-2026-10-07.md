# Morph Ball implementation recheck — EU1.1 closure

Implements the three documents supplied in
`E:\##Users\Admin\Downloads\Fruity-Prime-develop5_morphBall-Implementation-Recheck-EU1_1`,
against `develop5_morphBall` baseline `48090b474cc77d4c48ed811729da6894c074a330`:

- `Fruity-Prime-develop5_morphBall-Implementation-Recheck-EU1_1.md`
- `Fruity-Prime-Intent-Touch-Payload-Recheck-EU1_1.md`
- `Fruity-Prime-NativeTouchClock-Double-Consumption-Fix-Instructions-EU1_1.md`

## Findings addressed

| Finding | Change |
| --- | --- |
| Same native sample adds full Touch Roll twice | `PlayerEntity::ApplyTouchRoll` uses `MorphBallTouchRules::TouchRollStep`. `NativeTouchSample` allocates half of the native impulse to each 60 Hz substep. `5/4096` and the `8100` threshold are unchanged. |
| TouchBoost falls into Shoulder on its sibling | `ProcessBoost` calls production `MorphBallBoostStateMachine::AdvanceSample`. `SampleLatch` holds the touch decision by sample identity. TouchBoost fires once; TouchBoost and SkipShoulder cannot charge or release Shoulder until the next native sample. Shoulder-only decisions retain their 60 Hz charge/cooldown adaptation. |
| Cadence test compares individual impulses instead of their integral | `TestNativeTouchState` compares the sum of both simulation contributions against one native impulse, including X/Y/diagonal strokes, start/stop/lift, clamped edges, reversal and jump-pad factors 0/0.35/1. |
| Remote cannot distinguish identical samples or siblings | Protocol **16** transports a 32-bit sample sequence and a sibling flag. The adapter rejects duplicate, stale and reordered substeps. The identity is scoped by the intent's existing slot generation and life ID; those are validated by the network lifecycle before applying the report. |
| Online sample-boundary parity lacks a regression | `TestIntentTouchPayload` drives the real `TouchInputAdapter` using `HostTouch`, then uses the same report conversion as `NetPlayerBridge`, actual wire Write/Read, the remote adapter and production boost state machine. Owner/authority branches, events, charge and integrated roll match. Fault cases cover duplicates, held intents, dropped first/sibling packets, reordered samples, sequence wrap, re-arm, respawn and slot reuse. |
| Network declarations occupy one physical macro line | `ModSetShotState`, `ModTouchReport` and `ModSetReportedTouch` have separate macro lines. |

`TouchInputAdapter::Sample()` exposes the native boundary, sequence, identity
and this step's roll allocation. The input collector clears the allocation
at the start of every simulation step, including steps with no remote intent.
The producer still samples touch at 30 Hz and collects mouse motion at 60 Hz.
Suspend clears contact/motion and resumes with a fresh sequence.

## Wire and loss behavior

The state block remains four bytes. The original signed Delta4X/Y remain
four bytes; another four bytes carry the sequence. TouchFlags bits 3 and 4
indicate a sample identity and the second substep. The dedicated server's
existing `Size..FullSize` acceptance and length-preserving relay cover the
expanded payload. The native protocol assertion now expects version 16.

Bare/state-only payloads still read as no contact. Protocol 15 touch payloads
retain their signed deltas, with no invented identity from a truncated tail.
Legacy reports derive a fallback boundary from `IntentPacket::Frame`.
This preserves payload decoding; live protocol 15 peers cannot join a
protocol 16 session.

A delivered substep contributes at most half an impulse. Holding/repeating
the last intent never manufactures another contribution. A lost substep's
half is not reconstructed, and touch decisions remain latched until a newer
sample arrives. Therefore packet loss may reduce roll or delay the next
Shoulder decision; these tests establish deduplication and boundary recovery,
not identical trajectories or timing under arbitrary packet loss/jitter.

## Verification

- **Windows MSVC Release:** game and both touch test targets build successfully.
- **Focused CTest:** 5/5 PASS: NativeTouchState, IntentTouchPayload,
  RawMouseMotion, WindowsRawMouseInput and VulkanNvidiaReflex.
- **GCC:** the same CPU touch/boost regression builds with C++20 and
  `-ffp-contract=off`, then passes.
- **Regression sensitivity:** isolated source mutations restoring full roll
  on each substep and removing the sample latch each fail the corresponding
  integrated-roll or sibling-charge assertion. Tracked production sources
  are not modified by these mutation checks.
- **Android NDK 27.2 / arm64-v8a / API 28:** compile succeeds for the nine
  changed production translation units: MorphBallBoostStateMachine,
  MorphBallTouchRules, PlayerInput, PlayerMorphBall, TouchInputAdapter,
  NetProtocol, NetPlayerBridge, PlayerEntityNetAim and NetLobbyTest.
- **Game runtime:** `-maptest "AD2 ALINOS PERCH" -players 8 -bots -seconds 20`
  exits 0: 1200 frames, 8/8 spawned, 7/8 moved, 3/8 used alt form.
- **Real UDP session:** private protocol 16 loopback server with two Samus
  `-netcheck` clients for 30 seconds, `MPHREAD_PHASE_SECONDS=1`. Both exit 0
  and report PASS; each runs 1800 frames and spends 138 in alt form. Clients
  take 12/9 hits and each dies once. The server is stopped after the check.
- `git diff --check`: clean.

Logs and temporary check programs are under ignored `tools/build/out/`,
named `morph-recheck-*`. The real two-client tour verifies gameplay transport
and the new protocol's compatibility within this build; it does not produce
a physical stylus swipe. Sample-boundary swipe/boost coverage is the
deterministic production-adapter/wire/state-machine test described above.

No EU1.1 hardware trace was captured. Linux/macOS full builds, a new remote
CI run, Android APK packaging and Android device execution were not run.
The existing CI host jobs already build/run both expanded regression targets.
The previous closure's individual-frame roll assertion and immediate
post-TouchBoost charge behavior are superseded by this sample-pair contract.
