# Fruity-Prime ChatGPT planning workflow

Status: operational policy for the strict C# to C++ migration on develop2.

This workflow exists to keep dependency research fast while preserving the
one-file C#-as-specification contract. It is used before a broad implementation
order or a new migration slice is started.

## Fixed repository contract

- Repository: Zection6V/Fruity-Prime
- Branch: develop2
- Source link:
  https://github.com/Zection6V/Fruity-Prime/tree/develop2
- C# specification: src/MphRead/**/*.cs
- Native target: src/MphRead.Native
- Native layout: one hpp and one cpp beside the corresponding source-relative
  path
- No C++-only gameplay, protocol, UI, renderer, update, timing, fallback, or
  ownership policy

All ChatGPT messages in this workflow are written in English. Use the ChatGPT
High reasoning setting in the UI. Do not put the model name or reasoning label
in the prompt unless the UI requires it.

## Why this workflow is used

A namespace/using inventory is not a complete dependency graph. Same-namespace
references, partial classes, static initialization, inheritance, reflection,
conditional compilation, and platform aliases require source inspection.

Two independent investigations are therefore performed first. Their results are
committed as review artifacts. A fresh synthesis chat then reads those two
artifacts and writes the implementation-order document. Code implementation
does not begin while the order document is still being inferred.

## Phase 1 — two independent investigation chats

Open at most two ChatGPT Chat browser tabs for the two investigations. Run them
in parallel. Do not open duplicate tabs when one answer is still running.

Give each chat:

1. the develop2 GitHub tree link above;
2. the exact reviewed commit or branch;
3. the existing dependency-document links;
4. the strict C#-only/native-layout contract;
5. the instruction that git clone and shell cloning are unavailable;
6. the instruction to use the GitHub app/tools or repository browsing;
7. the instruction to inspect the complete source tree and not stop at examples;
8. the instruction to answer in English and continue until the investigation is
   fully complete.

Do not paste full source files into the message. Use source links and artifact
links instead.

The two investigations should be independent. At minimum, both must inspect
the full source tree and existing dependency documents, but each should
challenge the other on ordering, partial types, platform boundaries, and
validation. They must not silently assume that a matching Native file is
correct.

The required output of each investigation is a Markdown artifact, not a claim
that files were edited:

~~~text
docs/MphRead-Native-Dependency-Review-A.md
docs/MphRead-Native-Dependency-Review-B.md
~~~

Each artifact must contain:

- the reviewed commit and inventory counts;
- corrections to the existing dependency map;
- actual hidden same-namespace and member dependencies;
- partial-type and SCC membership;
- platform/runtime/external-library contracts;
- files that are safe, unsafe, or blocked to attempt;
- a complete proposed wave/order or a file-level dependency closure;
- prerequisites and validation gates;
- exact next parallel leaves;
- unresolved questions with a resolution method;
- no invented C++ behavior.

The first chat may emphasize exhaustive coverage and file-level assignment. The
second may emphasize adversarial review, cycles, platform boundaries, and
blocking assumptions. Both must still cover all 302 C# files or point to a
complete file-level appendix.

### Do not stop a high-reasoning answer early

Wait until the answer has completely finished. A long thinking indicator is not
a completed response. If a chat stops with an outline, send a short English
continuation such as:

~~~text
Continue the investigation to completion. Do not stop at an outline. Finish
the complete Markdown artifact, including all file coverage, prerequisites,
cycles, platform conditions, validation gates, blockers, and next leaves.
~~~

Do not start implementation or synthesis from a partial answer.

## Phase 2 — commit the two investigation artifacts

After each investigation is fully complete, ask that ChatGPT chat to commit
only its own Markdown artifact to develop2 using the GitHub app/tools.

The request must state:

~~~text
Create the requested Markdown artifact in docs/, review the diff, and commit
only that artifact to branch develop2. Do not use git clone or shell cloning.
Do not modify C# or C++ source files. Report the commit SHA and exact path.
~~~

Keep the two commits separate when possible. Do not ask ChatGPT to commit
unrelated source changes. If a chat cannot edit the repository, have it return
the complete artifact and record that it is not committed; the local agent may
then apply the artifact after checking it, but the missing commit must be
reported.

After both commits:

- close their completed browser tabs;
- fetch develop2 locally;
- verify each artifact exists at the expected path;
- inspect both diffs and ensure no source file changed;
- verify both artifacts refer to the same commit/inventory;
- preserve unrelated work;
- never use git add -A.

If either chat reports a stopped or incomplete investigation, do not treat its
artifact as authoritative. Continue that chat before Phase 3.

## Phase 3 — fresh synthesis ChatGPT chat

Start a new ChatGPT Chat for synthesis. Do not reuse a stale conversation whose
branch context or earlier assumptions may be wrong.

Give it:

~~~text
Repository:
https://github.com/Zection6V/Fruity-Prime/tree/develop2

Read these two committed investigation artifacts:
https://github.com/Zection6V/Fruity-Prime/blob/develop2/docs/MphRead-Native-Dependency-Review-A.md
https://github.com/Zection6V/Fruity-Prime/blob/develop2/docs/MphRead-Native-Dependency-Review-B.md

Use the GitHub app/tools or repository browsing. You cannot use git clone or
shell cloning. Do not request full source dumps. Work in English with high
reasoning effort. Treat src/MphRead/**/*.cs as the only specification and
src/MphRead.Native as a one-for-one C++20 target with colocated hpp/cpp files.
Do not invent C++ behavior.

Read both artifacts completely, reconcile disagreements against the current
source, and produce the complete implementation-order Markdown document:
docs/MphRead-Native-Implementation-Order.md
It must account for all 302 C# files exactly once, define prerequisites and
validation gates for every wave, identify partial classes/SCCs and platform
boundaries, preserve Program.Main -> ModEntry.TryHandleHeadless ->
GuiLauncher.TryRun/TextLauncher.Run ordering, list blocked files, select the
next two parallel leaves, and state unresolved questions. Do not stop at an
outline.
~~~

The synthesis chat should not be given full source dumps. The two committed
artifacts are the planning inputs; source browsing is used to resolve their
disagreements.

When the synthesis answer is complete, ask it to commit only the final order
document to develop2:

~~~text
Create docs/MphRead-Native-Implementation-Order.md from the completed synthesis,
review the diff, and commit only that Markdown file to branch develop2. Do not
modify C# or C++ source files. Report the commit SHA and exact path.
~~~

## Phase 4 — local verification before implementation

Fetch the synthesis commit and verify:

1. the order document is valid Markdown and contains the reviewed branch/commit;
2. all 302 source paths are present exactly once in its file-level assignment;
3. no assignment path is missing or extra;
4. wave counts total 302;
5. hpp/cpp target paths are source-relative;
6. SetupProgress relocation is explicitly normalized;
7. Scene, PlayerEntity, Metadata, Repack, and RepackCollision have one canonical
   declaration strategy;
8. Program.cs is last production entry integration;
9. no C# or C++ source file changed in the planning commits;
10. no unrelated work was staged or overwritten.

Use a deterministic check equivalent to:

~~~powershell
$root = (Resolve-Path 'src/MphRead').Path
$source = Get-ChildItem $root -Recurse -File -Filter *.cs |
  Where-Object { $_.FullName -notlike ($root + '\obj\*') }
$rows = Get-Content 'docs/MphRead-Native-Implementation-Order.md' |
  Where-Object { $_ -match '^W\d+ \| (.+\.cs)$' }
$source.Count
$rows.Count
~~~

Both counts must be 302. Also compare the normalized relative-path sets and
reject missing, extra, or duplicate records.

Only after this verification may implementation chats/workers start.

## Parallelism and tab hygiene

- Keep exactly two independent investigation chats in flight during Phase 1.
- Keep the synthesis chat separate and start it only after both artifacts are
  complete and committed.
- Do not repeatedly open replacement tabs for the same work.
- Close every completed or superseded tab immediately.
- If one chat is waiting for a response, the other may continue; do not stop
  either simply to make the browser look idle.
- If memory pressure appears, close finished tabs before opening new ones.
- A stopped response is a recoverable state: continue the same chat instead of
  discarding the investigation and starting many duplicates.

## Scope and commit safety

Planning artifacts may be committed; source implementation is a separate phase.
Each commit must contain only the intended Markdown path. Before pushing,
inspect staged paths and run a whitespace check. After pushing, compare local
HEAD with origin/develop2 and report both SHAs.

Never claim that a ChatGPT chat edited or committed a file unless the GitHub
history or returned commit SHA confirms it. Never treat an answer that only
contains an outline as a completed investigation.

## Transition to per-file implementation

The first implementation pair is taken from the final order document, not guessed
from the dependency index. The two workers receive separate fresh ChatGPT chats
and separate source-relative file scopes. For each file:

1. ask ChatGPT for a complete C#-faithful C++20 proposal/review;
2. inspect the answer only after it fully finishes;
3. implement the exact pair locally;
4. run the file-level C# oracle and relevant build checks;
5. commit only the intended source paths when authorized;
6. run the second ChatGPT review;
7. fix and repeat until the review is clean.

No Native-only behavior may be introduced to make a worker finish faster.
