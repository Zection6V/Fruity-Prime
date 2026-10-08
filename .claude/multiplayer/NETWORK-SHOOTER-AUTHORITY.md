# Shooter-authoritative hits, predicted kills, and the victim's view

Code: `src/MphRead.Native` -- `Mods/Network/NetHitClaims`, `NetHitPrediction`,
`NetDamage::Suppress`, `NetPlayerBridge::RetargetAtLocal` / `SteerIncoming`,
`NetHooks` (deferred intent, fired ray), `NetPlayerBridge::ConfirmIncoming`
(confirmed impacts). Protocol 18. Native only: the C# tree is still protocol
14 and has none of this.

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
- Validation: the victim the shooter drew must be within **1.25** units of
  the victim in the authority's own history at that frame (4.0 for no-beam
  hits) -- measured over 2581 claims at 250 ms +-40, the worst was 0.95 and
  p99.9 0.67, because the puppet is read off the very frame the authority
  rewinds to; it was 2.0 while the authority still checked every hit itself.
  Damage within the weapon's ceiling, lives current, and (protocol 18) the
  impact on the body: inside the capsule plus slack, and in the head band
  if the claim says headshot (`ImpactPlausible`, "landing off the body").
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

## Confirmed impacts (protocol 18)

What the victim's machine draws is the shooter's shot, fired from the
relayed trigger pull a round trip after the shooter saw it -- at a body that
has since moved. PR #109 turned it toward the victim on spec
(`RetargetAtLocal`): more shots were seen landing, and a third of the impacts
drawn on a victim were shots that had not hit anybody. Protocol 18 replaces
the guess with the authority's word:

- The claim carries **where on the body** the shooter's machine saw the shot
  land (`HitClaimPacket::Impact`, three signed bytes, 1/64 unit, relative to
  the victim's Position; `FlagSplash` when it was a blast). The projectile
  sets it around its `TakeDamage` (`NetHitPrediction::SetImpact`).
- The authority puts it, with the low byte of the shot's launch frame, into
  the damage event it publishes (`DamageEvent::Impact`, `LaunchLow`; 4 bytes
  per event, 16 per player state -- the snapshot still fits eight players).
- The victim's machine (`NetPlayerBridge::ConfirmIncoming`) finds **that**
  shot among the attacker's projectiles (the remote shot's `ModShooterAck`,
  closest launch frame within 3) and homes it onto that spot; if it has not
  appeared yet the confirmation waits 10 frames for it; if it has gone, or
  would have to turn more than 60 degrees, the weapon's impact is drawn on the
  spot (`SynthesizeImpact`) -- on a falling body too.
- A remote shot that meets this player **unconfirmed** is held where it met
  the body for 6 frames (the word follows the shot by about 2 frames at the
  median) and then let through: it is not drawn as a hit.
- The Shock Coil is left as drawn: one continuous beam, no projectile to
  bring in. `-noconfirmedimpacts` restores PR #109's behaviour.
- **Observers too.** Every player state carries its damage events, so a
  third machine watching A hit B confirms A's shot against B's puppet the
  same way (`ConfirmIncoming(attacker, victim, ...)`, `ModTargetSlot`). Before
  it, an observer saw 38% of A's hits as the same shot and 52% of B's damage
  with an impact; after, 93% and 99% (`HITLOC_OBSERVER=1`, a third headless
  client on `-hitrig observe`). `-noobservedimpacts` keeps it to this player.
- **A blast is an impact.** A shot that went out in an explosion reaching the
  victim needs nothing more drawn when its splash damage is confirmed (the
  Battlehammer drew two impacts for one damage: 72% precision).
- **The hold follows the line**: the 90th percentile of how long the word
  actually took for the last 64 held shots, plus one, within 2-8 frames (6
  until 16 are seen). A miss is held on a body no longer than it must be.
- Leading the turned shot onto where the victim will be (its speed and
  acceleration over the flight) was tried and taken out: on pads it made 17
  points fewer of the drawn shots arrive (49% -> 32% wells, 37% -> 20% lanes).

## What the authority checks, and what it no longer does

A claim is the hit, so the authority holds it to what the shooter's own
machine said it did (`NetHitClaims::ShotPlausible`, `RecordIntent`):

- **Its ack**: resolved against a world the shooter was still drawing -- no
  more than 20 frames before the newest ack its intents carried (honest
  claims travel with their intents: gap 0 at p99).
- **Its ray**: for weapons that fly straight (no gravity, homing or
  ricochet), the impact must lie within 0.75 of the ray the intent says that
  frame fired (honest: p99 0.11, worst 0.36), with **line of sight** from the
  muzzle (world geometry, `TestFlags::Beams`).
- **Its weapon's afflictions**: a burn, freeze or disrupt the weapon cannot
  inflict is dropped from the claim and counted, the damage kept.
- **No rewind for it.** A human's shot is no longer rewound on the authority
  unless a bot is in play (the only target its own copy can still hit): over a
  200 s two-player run, 1360 rewinds and 6667 projectile catch-up steps
  skipped, worst simulation step 6.7 ms -> 1.9 ms, mean unchanged (0.20 ms),
  RSS unchanged (~69 MB). History is still recorded: claims are checked
  against it.

## Measuring it

`-hitrig all` (see [the rig](../testing/HITRIG.md)): a pad-riding target that
shoots back, a shooter cycling all nine weapons. Each client reports
`hit prediction` (predicted / denied / unpredicted / kills / undone),
`hit claims`, and `hits taken that were seen landing` with a per-weapon line.
The authority reports `hit claims (as authority)`. `-hitlog FILE` on every
machine writes where each hit landed on the body, from all three points of
view, and `tools/hitrig/hitloc.py` joins them -- see
[the rig](../testing/HITRIG.md#where-a-hit-lands-hitlog). With `-debuglog` the
authority's `authority-hit` traces and the clients' `prediction` traces share a
shot key, which is what lets a run be joined shot by shot.
