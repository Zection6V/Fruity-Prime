# Fruity-Prime Mods audit evidence (read-only ZIP analysis)

Source ZIP: `Fruity-Prime-main(1).zip`; Git main SHA `66fdfd4f23d0e84837c1b16b5a91b8b48113708b`; Git tree reconstructed exactly as `2ee29a13cba4a76c8d32e235a21cdd76158ebd07`.

## Verified inventories
- `mods_file_inventory.tsv`: every one of 947 Mods paths, bytes, LOC, SHA256.
- `mods_dependency_edges.tsv`: 4,219 resolved direct **quoted** include relations, which is not a full compiled/transitive include graph. `mods_unresolved_includes.tsv`: 4 CMake-generated files, not broken source imports.
- `mods_function_index.tsv`: **lexical heuristic** 4,699 candidate function definitions, 3,801 qualified. Not AST, does not guarantee correctness or full enumeration.
- `mods_mutable_state.tsv`: **lexical heuristic** 1,097 candidate declarations (includes headers and definitions, potential duplicates and false positives). `mods_state_owner_review.tsv` is manually reviewed key owners.
- `mods_duplicate_candidates.tsv`: 26 unreviewed lexical rows including false matches; use `mods_duplicate_review.tsv` for reviewed groups. **Do not automatically consolidate.**
- `mods_dead_code_evidence.tsv`: 75 low lexical reference candidates; **none is approved for deletion**; searches did not establish program-wide reachability.
- `mods_hotpath_profile.tsv`: instrumentation candidates only; NO CPU/alloc/latency values were measured.
- `mods_parity_matrix.md`, `audit_test_results.tsv`: tests actually executed or blocked; baseline, not after-change results.
- `protocol_golden_line_endings.tsv`: root-cause proof for 3 baseline red tests: hash of source with CRLF equals expected; hash of Git LF does not.

## Reproducibility
`audit_index.py` extracts direct includes and lexical indexes, `verify_git_tree.py` reconstructs Git tree SHA, `run_isolated_tests.py` compiles selected no-Qt units. Scripts contain paths specific to this analysis sandbox and may need path updates on another workstation. They do not change source files, commit, push, or write to GitHub.

### Scope caveats
- CMake configuration blocked by missing OpenGL development headers/libs in this Linux container; no whole-project compile or full CTest.
- Python `check-phase4-legacy-gl.py` and `check-phase5-shader-interface.py` passed; native-audit helper suite timed out and is NOT marked pass.
- Windows, macOS, Android, GPU capture, UDP latency/fault matrix and true profiler runs remain mandatory.
- Function/category labels are proposals, not proof of SRP violations. All static data must be reviewed by human owners.
