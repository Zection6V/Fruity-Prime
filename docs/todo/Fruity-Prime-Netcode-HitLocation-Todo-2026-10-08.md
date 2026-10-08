# Netcode after PR #109: what is left (2026-10-08)

The goals the work is measured against: **no visible rewind**, **high precision
on the authority**, **a headshot is a headshot**, **the shooter's and the
victim's screens agree on every shot fired and taken** -- and the dedicated
server stays **cheap in RAM and CPU** (the bench boxes are 2 cores, 900 MB).

Where it stands (protocol 18, `.claude/multiplayer/NETWORK-SHOOTER-AUTHORITY.md`,
measured with `-hitlog`, `.claude/testing/HITRIG.md`): over the real Japan line
the victim sees the same shot hit them 94-95% of the time (31-75% before),
every damage taken comes with an impact (37-87% before), impacts drawn are real
95-99.5% of the time (72-82% before), 0 kills taken back in 369, head band
agreed 99-100%; 0.63% of hits would change band if judged against the
authority's history instead of the shooter's screen (left as is: shooter wins).

## In order

1. **Observers.** Two-player tests cannot see it: in a match of eight, C
   watching A shoot B still gets the old drawing -- nothing confirms A's shot
   on C's screen. The snapshot already carries B's damage events (impact,
   launch byte); confirm remote victims too. Needs a three-client run with a
   pure observer.
2. **Per weapon.** Battlehammer impacts are 72% real (shell and splash: two
   impacts for one damage); Volt Driver shots found on the victim's screen
   86%; the Shock Coil beam is drawn as aimed (align it on the shooter's point
   each tick). The 6-frame hold for an unconfirmed shot should follow the
   measured ping (3 frames are enough at 37 ms).
3. **Anti-cheat on the claim** (the claim is now the hit): the impact must lie
   on the ray the intent says was fired (`IntentPacket::ShotOrigin/Direction`),
   with line of sight in the authority's history; `AckFrame` bounded to the
   shooter's recent acks (no choosing the frame); fire rate per weapon. The
   claim radius (1.25, worst honest 0.95 locally / 0.44 Japan) can go lower.
4. **Server cost.** The authority still simulates and rewinds its own copy of
   every human-vs-human shot only to throw it away (`NetDamage::Suppress`,
   "server copies not applied"). Skip it, or keep it behind a flag as
   anti-cheat telemetry. Measure RSS and step time before/after
   (`-simcheck`, and the bench runs' server process).

## Also open

- Lead aim for remote shots turned onto the victim (`-nolead` to compare):
  more real projectiles arriving instead of impacts drawn on the spot.
- Latency spikes: `-netspike N:MIN-MAX` campaign on the Netherlands box.
- Charged Magmaul burn: untested -- the rig never lands a charged round, so
  burn-tick ownership and doubling (PR #109 review) are still unmeasured.
