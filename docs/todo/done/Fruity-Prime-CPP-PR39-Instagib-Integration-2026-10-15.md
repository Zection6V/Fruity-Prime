# PR #39: Instagib integration with the ModEntry refactor

Date: 2026-10-15. PR: <https://github.com/Zection6V/Fruity-Prime/pull/39>.

## Integration

The PR merges `liveteklol/master` at `3bb76d809d42ef2e2644fb4a14d6b44a7489eecb`
into `develop13`, starting at `55ea45e7a0fa2def30b926e14a8e243ed4d39bd5`.
The integration commit is `9eb803418587bb8466cc8729d6c82dcec0005695`.
Follow-up `5c84d70ae82e9a2418ff39ee472ddab02b549a39` fixes the claim validation
and lethal-hit test observation issues exposed by the integration test.

The conflict was in `Mods/ModEntry.cpp`: Instagib had been added to the original
dispatcher before its responsibilities moved into `Mods/Entry/`. Keep the
refactored dispatcher and initialization order. Its local `-instagib` and
`-instagibdemo` settings remain after setup/map-generation startup; the server
flag is applied in `Mods/Entry/ServerCommands.cpp`. The flag inventory gains
only these two flag names (three lookup sites); the previous 217 names remain.
The background-map shutdown fix remains intact.

The PR's launcher, Android launch-plan propagation, network rule bit and CI
changes are preserved. The Qt installer already supports the `--cache` option
used by the incoming workflow. HomingTargetHistory's existing out-of-class
array definition is preserved alongside the incoming Frame initialization fix.

## Responsibility boundaries

- `Mods/Combat/InstagibRules`: multiplayer-only policy, damage, pickup policy,
  AI weapon choice and the AI pickup/spawn target range.
- `Mods/Combat/InstagibPlayers.cpp`: loadout and AI private-state adapters,
  following the existing PlayerEntity extension pattern. Their `Mod`-prefixed
  declarations/calls are the hooks in the ported player sources.
- `Mods/Render/InstagibEffects`: muzzle, impact, gun-material tint and replacement
  sniper-rail policy. The existing `InstagibRails` owns rail rendering.
- `Mods/Render/EffectTint`: effective tint lookup, brightness-preserving colour
  calculation and effect spawning. Gun materials and particles share this
  calculation. Child effects inherit entry-owned tints as well as element-owned
  tints; the original PR only propagated the latter.

`Entities/` contains calls to these modules and the required method/argument
declarations. `Formats/Effects.cpp` has four calls into EffectTint; its new
`TintedColor` implementation has been removed. The only format data additions
are two optional `ModTint` fields, reset when the renderer reuses pooled entries.
No C# files or game/protocol namespace names change.

## Online damage integration

An Imperialist duel exposed a compatibility issue with the current authority's
hit-claim validation. The client produced Instagib's 9,999 damage, but
`NetHitClaims::MaxDamageFor` still used the ordinary weapon row and rejected all
six sampled claims. Merely resolving the ModEntry conflict would have left
online hits predicted and then undone.

`InstagibRules::DirectDamage` is now the common source for projectile damage,
the authority's preliminary claim bound and its final damage profile. The final
profile also treats the rule as overriding the Prime Hunter bonus, just as the
projectile does. Weapon/shot eligibility, geometry, life/stream identity and
line-of-sight validation retain their existing checks.

The duel then confirmed the claimed kills, but `NetFeatureCheck` reported
"never took a single hit": it excluded HP zero and sampled health below the
alive-only parity guard. Sample health transitions before that guard, keep the
parity guard intact, and share the health-drop predicate with NetCheckClient's
local/remote counters. This counts a lethal hit once and does not count a
respawn as damage. The final duel passed both reports and confirmed every
declared claim.

## Validation

| Check | Result |
|---|---|
| Windows MSVC Release | Build and link passed. Final binary SHA-256: `e5c2eb1d2e5b07a136937dad8cecd474a0e89483e7ce6da934ca5e32f75287a8` |
| CTest | 123/123 passed, including Instagib, the three ModEntry tests, NetworkLobby, NetworkCombat and seven game-file checks |
| CLI baseline comparison | All 45 existing cases matched stdout/stderr, exit codes and named artifact hashes |
| GCC 16.2 / MSYS2 | All 38 changed native C++ translation units compiled using the configured Ninja commands; NetFeatureCheck was compiled again after its final sampling fix. This is compile validation, not a full GCC link |
| Android x86_64 | Native shared library compiled and linked; no device run |
| Normal map test | TEST ARENA, 8 players/bots, 1,200 simulation frames, exit 0; 8/8 spawned, 7/8 moved, 7/8 fired |
| Instagib map test | Same map/player count/frame count, exit 0; 8/8 spawned, 8/8 moved, 7/8 fired |
| Instagib rendering | Hidden OpenGL map test and 11 offscreen captures, exit 0; inspected the coloured rail fan and gun presentation |
| UDP loopback | Instagib server plus two clients without the `-instagib` CLI flag, `-hitrig duel`, no bots; both clients completed 18 seconds with exit 0 and RESULT: PASS. Each declared two claims, both applied, zero refused/unanswered, two predicted kills confirmed with zero undone. Each died twice. Status packet flags were `0xa1`, including Instagib bit 7; server exited 0 after Ctrl+C |
| Remote CI | All 16 jobs passed: <https://github.com/Zection6V/Fruity-Prime/actions/runs/38736477180>, testing `5c84d70ae82e9a2418ff39ee472ddab02b549a39` before updating the PR's base branch. Includes Windows/MSVC, Linux/GCC, macOS/Clang, Linux arm64 server, Android builds/APK and API 28/30/35 startup smoke. The subsequent delivery commit changes only this validation record |

The offline map/render checks ran on the initial integration commit. The later
changes concern online claim settlement and diagnostic health observation.
The ordinary 18-second tour was also tried with two bots: it passed once, but
another run lacked a hit on one bot and failed that tour's coverage requirement.
Use the focused Imperialist duel above for reproducible Instagib acceptance,
including positive authority confirmations rather than only a generic PASS.

To reproduce the focused case on a build with game files and TEST ARENA, write
`TEST ARENA | Battle | 7 | 1000` to a rotation file, start the server, then run
the two clients concurrently in separate processes:

```text
FruityPrime.exe -server -port 27982 -players 2 -bots 0 -rotation rotation.txt -instagib -nomaster -noupdate -noautoupdate -console -debuglog
FruityPrime.exe -netcheck 127.0.0.1 -port 27982 -name IG1 -hunter Samus -seconds 18 -headless -hitrig duel -noupdate
FruityPrime.exe -netcheck 127.0.0.1 -port 27982 -name IG2 -hunter Samus -seconds 18 -headless -hitrig duel -noupdate
```

Check each client's `hit claims` and `hit prediction` lines as well as its exit
code: there must be accepted/confirmed hits, with no damage-bound refusals or
undone predicted kills. Stop the server with Ctrl+C after the clients finish.

The Instagib test now covers normal-match preservation, lethal Imperialist
damage, unchanged other-weapon damage, AI pickup-range boundaries, story-mode
isolation, offline rule reset, online authority, untinted particles, tint
precedence, inherited child tint and muzzle pool reuse. These checks do not
stand in for manual input/device testing.

Local raw logs and captures are under `tools/build/out/pr39-*` (ignored build
output). They are retained locally; map payloads and game-derived images are not
committed. The unrelated `docs/todo/refactoring/pending/` work is untouched.
