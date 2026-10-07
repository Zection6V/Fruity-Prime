# Morph Ball gap audit (EU1.1) — closure

Answers `Fruity-Prime-develop5_morphBall-Implementation-Gap-Audit-EU1_1`
(audited HEAD `57174384`), finding by finding.

| Priority | Finding | Outcome |
| --- | --- | --- |
| P1 | `FruityPrime.NativeTouchState` not run by CI | **Fixed.** `build_cpp.yml` builds and runs it on the Windows, Linux and macOS host jobs, beside the new `FruityPrime.IntentTouchPayload`. The step is renamed *Reflex lifecycle, raw mouse and Morph Ball input regression*. |
| P1 | 4-sample window advanced at 60 Hz | **Fixed (option A).** `NativeTouchClock` ticks the producer every other simulation step; both 60 Hz substeps read the same sample, the way MphRead's doubled counters read one native tick as two frames. Mouse motion is collected every step and handed over each tick (`MouseMotionTouchSource::AddMotion`/`Tick`). Threshold and history length are unchanged. |
| P1/P2 | Intent carries no touch state | **Fixed (state option).** `IntentPacket` carries `TouchFlags` in the state block's spare byte and `TouchDelta4X/Y` in four bytes after it; `NetPlayerBridge` sends the owner's `NativeTouchState::Reported` and applies it to remote players before `ProcessAlt`. `ProtocolVersion` 14 → 15, because an older server drops an intent longer than its `FullSize`. |
| P2 | Test used a toy state machine | **Fixed.** `MorphBallBoostStateMachine` is the code `PlayerEntity::ProcessBoost` runs; the tests drive it directly. `PlayerMorphBall.cpp` only applies what it returns (impulse, cap, cooldown, damage, effect 136). |
| P3 | `player+0x364 & 0x10` represented implicitly | **Kept, documented.** No control-mode byte exists in the port; the touch adapter raising `Down` stands in for it (`PlayerMorphBall.cpp`, CLAUDE.md). |
| — | `02021D40` field35C / Message 24 | **Not ported, on purpose.** MphRead documents Message 24 as never checked; there is no consumer. |

## Acceptance tests

`TestNativeTouchState.cpp`:

- **Cadence:** a steady 0.5 s stroke at 600, 700 and 1350 DS units/s, run
  through a 30 Hz reference producer and through Fruity's clock at 60 Hz.
  The rolling SUM matches the reference tick for tick, the boost fires
  within one 60 Hz step of the native time (and not at all below 675 units/s),
  and each frame's touch roll equals its native tick's.
- **State machine:** small continued touch freezes the charge (held and
  released); touch boost preserves it; the next frame charges; the release
  fires the shoulder boost and spends it; a repeated contact does not boost
  twice; min is strict; FullBoostCharge spends the max; touch strength is
  full and shoulder strength proportional.
- **Reported state:** the authority's copy takes the owner's branch; no
  contact or a first contact carries no delta.

`TestIntentTouchPayload.cpp`: the touch block round-trips signed; a
state-only payload (an older demo) reads as no contact.

## Verification

- Windows MSVC Release, clean build: green. CTest: 27 tests, all pass except
  `VulkanDescriptorAllocator`, which fails without output and touches none
  of these files.
- `-maptest "AD2 ALINOS PERCH" -players 8 -bots -seconds 20`: exit 0.
- Loopback server (protocol 15) with two `-netcheck` clients, 60 s: both
  connect, alt form crosses both ways (509/531 frames). The run reports
  "never took a single hit" — the tour landed no shots on MP1 SANCTORUS
  with two players; nothing about touch.

## Not done

- No recorded EU1.1 hardware trace exists, so parity is against the ROM
  disassembly and a 30 Hz reference model, not a captured trace.
- No two-client run drives an actual swipe: the scripted tour has no stylus.
  The intent path is covered by the payload test and the reported-state test.
- Linux, macOS and Android were not built locally; CI covers them.
