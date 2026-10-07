# Shooter-authoritative hits, predicted kills, and the victim's view

Code: `src/MphRead.Native` -- `Mods/Network/NetHitClaims`, `NetHitPrediction`,
`NetDamage::Suppress`, `NetPlayerBridge::RetargetAtLocal` / `SteerIncoming`,
`NetHooks` (deferred intent, fired ray). Protocol 17. Native only: the C#
tree is still protocol 14 and has none of this.

This replaces the model [hit claims](NETWORK-HITCLAIMS.md) describes, where the
authority resolved every shot itself a round trip late and claims only rescued
what it missed. The complaint that model could not answer: *you kill somebody on
your screen and the server stands them back up*.

## The rule

**What the shooter's machine resolved is the hit.** The authority validates it
and applies it; it does not hold a second opinion.

- A remote human's hit on another human arrives as a claim and is applied on the
  next tick, in fire-frame order (grace 0). The authority's own simulated copy of
  that shot is not applied (`NetDamage::Suppress`, counted as "server copies not
  applied"). Self-damage, bots (either end) and the world stay the authority's.
- The claim carries what the authority's copy used to provide: the knockback
  impulse the shooter's machine applied (`HitClaimPacket::Impulse`,
  `FlagImpulse`) and the afflictions (freeze, burn, disrupt).
- Validation is unchanged: the victim's body in the authority's own history at
  the frame the shooter was drawing must be within 2.0 units (4.0 for no-beam
  hits) of the claimed hit, damage within the weapon's ceiling, lives current.
- `-servershots` on a server restores the authority resolving them itself.

Why the authority's copy cannot be trusted as a second opinion: it fires the
shot a round trip after the shooter did, rewinds only the first ~20 frames and
flies the rest in its own present. For a slow projectile (an Omega Cannon round
flies 1.7 s) the two simulations meet different worlds and disagree no matter
how good the rewind is.

## Kill arbitration

A shot is void only if it was **fired after its owner was already displaying
their own death** (`FiredAfterOwnDeath`, compared against the authority frame
the shooter went down on). A client cannot fire after its own death snapshot,
so a trade inside a round trip is two kills and neither screen is taken back.

## Predicted kills

`NetHitPrediction::DeathEnabled()` is on: a lethal hit kills the puppet the
frame it lands. The snapshot cannot stand it back up -- `ApplyState` ignores a
same-life "alive" state while the puppet is dead here -- unless the kill was
genuinely refused (resync on the verdict) or never reported within
`HoldFrames + 30` (`ShowingKill`). The scoreboard keeps a predicted kill until
the authority's own count catches up (`KillsShown`). `-nodeathprediction` opts out.

## The fired ray

The intent used to be captured and sent before the frame's simulation, so it
carried the aim and body position of the *previous* frame, and the authority
rebuilt the shot from a camera it could only guess (the client's camera is
smoothed and lags the body on a jump pad). Now the intent is captured before
the simulation and **sent after it**, carrying the exact origin and direction
the frame fired (`IntentPacket::ShotSize`, optional tail block).

## The victim's view

The shooter wins -- including against a victim who has already reached cover on
the authority (a decision taken on purpose). What the victim gets instead is to
*see* the shot that hits them:

- A remote shot drawn here that was aimed at this player in the world its
  shooter was drawing is turned onto where this player is now, from the gun
  drawn here (`RetargetAtLocal`, `DrawnRemoteShot`). Falling projectiles
  (Battlehammer) are rotated so the arc keeps its elevation.
- When the authority's damage arrives and a shot from that attacker is still in
  the air here, it is steered onto this player (`SteerIncoming`).
- `-noretarget` draws remote shots with their shooter's own aim.

## Measuring it

`-hitrig all` (see [the rig](../testing/HITRIG.md)): a pad-riding target that
shoots back, a shooter cycling all nine weapons. Each client reports
`hit prediction` (predicted / denied / unpredicted / kills / undone),
`hit claims`, and `hits taken that were seen landing` with a per-weapon line.
The authority reports `hit claims (as authority)`. With `-debuglog` the
authority's `authority-hit` traces and the clients' `prediction` traces share a
shot key, which is what lets a run be joined shot by shot.
