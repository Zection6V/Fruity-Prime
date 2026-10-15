# Fruity-Prime C++ Mods — Parity / validation matrix (ZIP baseline)

Fixed source: main `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`. These are **baseline tests on the uploaded ZIP**, NOT after-refactor results. **PASS is only claimed for tests executed here**.

| Area | Test or invariant | Baseline result | Before/after requirement |
|---|---|---|---|
| Provenance | ZIP Git tree SHA matches GitHub main | **PASS**: `2ee29a13...` | Reject if SHA differs |
| Localization | AOT en/ja/fr generator, full k8x12L 7170 glyphs, standalone C++ MenuLocalization | **PASS** | Same string/layout/glyph code parity |
| Combat | Standalone `ClaimDamagePolicy` | **PASS** | Preserve charged, headshot, Double Damage, turret split |
| Combat | `HitClaimRateLimiter` / `HitClaimMetrics` / `HitClaimDeathState` / `RescuedHitLedger` | **PASS** (4 isolated targets) | Same verdicts/order/lifecycle |
| Network | `SessionAuthorityState`, `ClientConnectionHealth`, `SessionTrafficMetrics` | **PASS** (3 isolated targets) | Server/client role and reset invariants |
| Network | Protocol 30/31/32 historical Golden check | **FAIL 3/3**: LF tracked blobs vs CRLF expected SHA | Correct line-ending baseline *without loosening packet data constraints*; rerun all |
| Network | Protocol 33 compiled binary Golden, UDP matrix | **NOT RUN** | Requires full native build, socket scenarios |
| NetSmoothing | C++ TestNetSmoothing timeline | **NOT RUN** | Protect shared display/AckFrame/NetUnlagged timestamps |
| Render | Phase 4 legacy GL API static gate | **PASS** | No forbidden legacy GL submissions |
| Render | Phase 5 shader interface static gate | **PASS** | Preserve location bindings and shader hashes |
| Build | Linux CMake configure | **BLOCKED**: OpenGL development package absent | Complete native configure, compile and full CTest |
| CI | `.github/workflows/build_cpp.yml` filtered CTest commands | **CONFIRMED static**: selected `-R` excludes historical Golden tests | CI must execute the previously red preservation tests |
| Input/UI | Gamepad + server selection + setting mutation live UI | **NOT RUN** | Preserve device notifications and selected endpoint across list reorder |
| MapGen | Q3 map import, bake, hash, error recovery | **NOT RUN** | Preserve assets and semantic outputs |
| Shutdown | Asynchronous audio / debug logger termination | **NOT RUN** | No lifetime regression across cancellation and crash hooks |
| Platforms | Windows/MSVC, macOS, Android, GPU hardware | **NOT RUN** | Explicit independent gates before declaring final completion |

**Interpretation:** Isolated tests compile selected sources directly rather than through the CMake target and cannot replace complete CTest/CI. A failed manual link from an intentionally incomplete object set is a test-harness limitation, not a product failure. No performance baselines measured; see `mods_hotpath_profile.tsv` for instrumentation candidates only.
