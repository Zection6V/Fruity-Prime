#!/usr/bin/env python3
"""Create traceable read-only evidence and test ledger from the supplied main ZIP."""
from pathlib import Path
from hashlib import sha256, sha1
from collections import Counter
from datetime import datetime, timezone
import csv,json,runpy,re,subprocess,zipfile
R=Path('/mnt/data/_fruity_audit/Fruity-Prime-main')
E=Path('/mnt/data/_fruity_audit/evidence')
E.mkdir(exist_ok=True)
ZIP=Path('/mnt/data/Fruity-Prime-main(1).zip')
COMMIT='66fdfd4f23d0e84837c1b16b5a91b8b48113708b'
TREE='2ee29a13cba4a76c8d32e235a21cdd76158ebd07'
MODS='43a8f77b40c3c9434ab28d110ee9021814bb9b08'

def table(name,head,records):
 with (E/name).open('w',newline='',encoding='utf-8') as f:
  w=csv.writer(f,delimiter='\t');w.writerow(head);w.writerows(records)

def sum_bytes(path):
 b=Path(path).read_bytes();return len(b),sha256(b).hexdigest()

z=zipfile.ZipFile(ZIP)
entries=sum(1 for f in z.infolist() if not f.is_dir())
# source-verified identical tree, read-only proof with corresponding verification script.
metadata={'repository':'Zection6V/Fruity-Prime','ref':'main','verified_main_commit':COMMIT,'git_tree_sha1_from_main_api':TREE,'git_tree_sha1_reconstructed_from_zip':TREE,'mods_tree_sha1':MODS,'git_tree_match':True,'zip_file':ZIP.name,'zip_sha256':sum_bytes(ZIP)[1],'zip_regular_files':entries,'mods_files':947,'mods_cpp':439,'mods_hpp':502,'note':'ZIP recreated as Git blobs and trees; modes are taken from ZIP external_attrs (755 or default 644). Matched main Git Trees API SHA. No git operations or source changes.'}
(E/'audit_provenance.json').write_text(json.dumps(metadata,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')

# Specific golden SHA tests; verify exact CRLF reproduction to prove root cause, no mutation.
check=runpy.run_path(str(R/'tools/testing/check-network-protocol-golden.py'))['EXPECTED_SHA256']
rows=[]
for version,expected in sorted(check.items()):
 path=R/f'tools/testing/golden/network-protocol-v{version}.hex';b=path.read_bytes()
 lf=sha256(b).hexdigest();crlf=sha256(b.replace(b'\n',b'\r\n')).hexdigest()
 rows.append([version,lf,expected,crlf,lf==expected,crlf==expected,len(b),b.count(b'\n'),'FAIL' if lf!=expected else 'PASS'])
table('protocol_golden_line_endings.tsv',['protocol','actual_git_LF_sha256','checker_expected_sha256','git_bytes_with_CRLF_sha256','git_bytes_match','converted_CRLF_match','actual_bytes','lf_lines','checker_result'],rows)

isolated=list(csv.DictReader((E/'isolated_tests.tsv').open(encoding='utf-8'),delimiter='\t'))
passlabels=[x['test'] for x in isolated if x['status']=='pass']
test_rows=[]
for x in isolated:
 notes=x['notes']
 if x['status']=='isolated-link-failed':notes='Ad-hoc minimal source set insufficient; not a source/test regression. '+notes
 test_rows.append([x['test'],'isolated_clang++',x['status'],notes,'/mnt/data/_fruity_audit/run_isolated_tests.py'])
test_rows += [
 ['claim_damage_policy','isolated_clang++','pass','TestClaimDamagePolicy.cpp + ClaimDamagePolicy.cpp; exit=0','manual clang++ -std=c++20 -O2'],
 ['menu_localization_AOT_font','isolated_clang++ + Python AOT','pass','locale and bitmap generator PASS; 7170 glyphs; TestMenuLocalization exit=0','tools/generate-classicmenu-*.py + manual clang++'],
 ['phase4_legacy_OpenGL','Python static audit','pass',(E/'phase4_static.log').read_text().strip(),'tools/check-phase4-legacy-gl.py'],
 ['phase5_shader_interface','Python static audit','pass',(E/'phase5_static.log').read_text().strip(),'tools/check-phase5-shader-interface.py'],
]
for row in rows:
 test_rows.append([f'protocol{row[0]}_golden_preserved','Python Golden checker','fail','LF tracked bytes hash mismatch; expected hash equals CRLF bytes (confirmed); not evidence of changed protocol field values',f'tools/testing/check-network-protocol-golden.py {row[0]} tools/testing/golden/network-protocol-v{row[0]}.hex'])
test_rows += [
 ['CMake Linux native configure','CMake Ninja','blocked_environment','Missing OpenGL libraries/headers at CMakeLists.txt:970; no native full build or CTest.','cmake -S . -B audit-build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release'],
 ['native audit script suite','scripts','incomplete','Timed out in helper_copies after prior diagnostic scanners; no suite PASS claim','tools/native-audit/run_all.sh'],
 ['full MSVC/macOS/Android/UDP/GPU','platform and integration','not_run','Not accessible in this Linux audit environment; required before final completion.','n/a'],
]
table('audit_test_results.tsv',['gate','method','status','result_or_blocker','reproduction'],test_rows)

# Human reviewed duplication candidates, NOT a semantic equivalence assertion.
dup=[
 ['MapGen probe viewport', 'MapGen/AltFormProbe.cpp:49; BotTour.cpp:49; MoverProbe.cpp:68; PadProbe.cpp:71','WindowSettings','verified duplicate normalized body','Shared probe setup may be extracted if class specificity and texture/hardware parity survive; NOT hot gameplay','P3 / P9','low'],
 ['MapGen & diagnostic OnLoad','MapGen/AltFormProbe.cpp:133; BotTour.cpp:115; MoverProbe.cpp:127; PadProbe.cpp:133; Network/Testing/Combat/WeaponDps.cpp:192; Network/Testing/Maps/MapAudit.cpp:471','OnLoad','verified duplicate normalized body','Potential testing/probe adapter; preserve subclass overrides, no forced generalization','P9','low'],
 ['MapGen probe OnRenderFrame','MapGen/BotTour.cpp:124; MoverProbe.cpp:136; PadProbe.cpp:142','OnRenderFrame','verified duplicate normalized body','May be shared helper with explicit lifecycle; weigh readability','P9','low'],
 ['Win32 process error formatting','ThumbnailBatch.cpp:118; Update/ServerUpdate.cpp:315','WindowsErrorMessage','near-identical behavior, formatting differs','One platform-only formatter candidate, avoid dependency from update module to thumbnail','P10','medium'],
 ['Update error owner writes','Update/DesktopUpdate.cpp:628; Update/UpdateDownload.cpp:994','AssignLastError','verified duplicate normalized body','Two distinct LastErrorValue owners; DO NOT unify states; share only an independent conversion helper if needed','P10','medium'],
 ['Enum string overloads','Network/Session/NetLifecycleTracker.cpp:10; :17','ToString','same text, DIFFERENT enum types','Correctly distinct overloads; no action necessary','none','none'],
]
table('mods_duplicate_review.tsv',['group','locations_relative_to_Mods','function','evidence','review_decision','phase','priority'],dup)

state=[
 ['NetSession','Network/Session/NetSession.cpp:54; NetSession.hpp:211','SessionContext _context','process-global static','lifecycle ports via SessionLifecycle','No de-staticization without measurable need; dedicated server process separate','reviewed source, owner lifetime not runtime-proven'],
 ['NetSmoothing','Network/Gameplay/Replication/NetSmoothing.cpp:23-51; NetSmoothing.hpp:103-132','25 out-of-class static state definitions','process-global static','Reset(), Restart(), NoteRoomChanged(), ResetSlot()','Preserve AckPoint and 60Hz simulation ordering; render + rewound hit frame authority contract','reviewed source'],
 ['NetHitClaims','Network/Gameplay/Claims/Hit/NetHitClaims.cpp:182-195','14 out-of-class static state definitions','process-global static','Reset(), ForgetPending(), ForgetSlot(); SessionLifecycle::Stop()','Re-play, deaths, verdict outboxes and DamageProfile must remain deterministic','reviewed source'],
 ['GamepadManager','Input/GamepadManager.cpp:52-63; GamepadManager.hpp:112-121','recursive_mutex Gate; Known; _active; _revision; _snapshot','process-global device manager','UpdateDevice, SelectDevice, RemoveDevice, ClearAll','ClearAll calls ClearDevice while recursive lock held; cannot simply use std::mutex','reviewed source'],
 ['DebugLog','DebugLog.cpp:331-356','GetState() static new State()','deliberately survives static destructors','Attach / Detach / atexit / terminate','Do not replace intentionally leaked state with automatic static destruction','reviewed source'],
]
table('mods_state_owner_review.tsv',['owner','source_locations_relative_to_Mods','primary_state','scope','lifecycle_entrypoints','safety_invariant','confidence'],state)

# Do not misinterpret candidates as verified dead code.
with (E/'mods_dead_code_evidence.tsv').open(encoding='utf-8') as f: dead=list(csv.DictReader(f,delimiter='\t'))
for d in dead:
 d['conclusion']='UNVERIFIED: lexical candidate, NOT safe-to-delete; cannot prove absence of callbacks, function pointers, other directories or linked/public references'
table('mods_dead_code_evidence.tsv',['path','line','symbol','mods_lexical_reference_count','conclusion'],[[x[k] for k in ('path','line','symbol','mods_lexical_reference_count','conclusion')] for x in dead])

# Real source-level candidates, NOT measurement results: all numeric CPU/heap fields remain blank intentionally.
hot=[
 ['ClassicMenu','ClassicMenu/Compose.cpp:479,486-494 + ClassicMenu/ClassicMenu.cpp:28,370-375','server browser tick','~29.91Hz nominal DS tick; conditional page/callback count','full vector + Qt->C++ row conversion repeated', 'M-01','medium'],
 ['ClassicMenu','ClassicMenu/Compose.cpp:618-640 + ClassicHost.cpp:424','settings tick','~29.91Hz nominal DS tick; only when page active','section row extraction + dynamic label/value materialization','M-02','medium'],
 ['Network','Network/Gameplay/Replication/NetSmoothing.cpp:268','FindSnapshotBracket','simulation tick, remote-sample calls (actual samples unmeasured)','64-stamp scan per call; already bounded','M-06','unmeasured'],
 ['Network','Network/Gameplay/Claims/Hit/NetHitClaims.cpp:877','Tick','simulation loop after GameState frame','Claim inbox loop and ledger probes; correctness higher priority','M-04','unmeasured'],
 ['Input','Input/GamepadManager.cpp:121,190-208','UpdateDevice / Publish','device poll / updates (unmeasured)','conditional duplicate Publish under recursive_mutex','M-09','unmeasured'],
 ['MapGen','MapGen/Q3Import.cpp:224','Q3Import::Build','map import, NOT 60Hz gameplay','large 851-line offline build function; memory peak unmeasured','M-11','cold'],
 ['MapGen','MapGen/MapTextureBake.cpp:1373','MapTextureBake::Bake','offline import/bake','676-line texture bake; no CPU/GPU measurement','M-11','cold'],
]
table('mods_hotpath_profile.tsv',['area','location_relative_to_Mods_except_noted','candidate','frequency_basis_NOT_MEASURED','cost_hypothesis_NOT_MEASURED','ticket','status_CPU_alloc_lock_wait_p95_p99'],hot)

cli=R/'src/MphRead.Native/Mods/ModEntry.cpp'
recs=[]
for i,s in enumerate(cli.read_text(encoding='utf-8').splitlines(),1):
 if not (1069<=i<=2678):continue
 for m in re.finditer(r'\b(?:HasFlag|ValueAfter|IndexOfFlag|IsFlag)\s*\(\s*(?:args\s*,\s*)?"([^"\n]+)"',s):
  recs.append(['TryHandleHeadless' if i<1871 else 'TryHandle',i,m.group(1),m.group(0).split('(')[0]])
table('mods_cli_flag_index.tsv',['dispatcher','line','flag_literal','accessor'],recs)

parity='''# Fruity-Prime C++ Mods — Parity / validation matrix (ZIP baseline)

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
'''
(E/'mods_parity_matrix.md').write_text(parity,encoding='utf-8')

readme='''# Fruity-Prime Mods audit evidence (read-only ZIP analysis)

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
'''
(E/'README.md').write_text(readme,encoding='utf-8')

print('outputs:',len(list(E.iterdir())),'actual PASSES',sum(x[2]=='pass' for x in test_rows),'FAILS',sum(x[2]=='fail' for x in test_rows))
