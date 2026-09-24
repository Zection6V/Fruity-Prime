---
name: chatgpt-sol-review
description: Start a fresh ChatGPT conversation with Sol high for each independent work item, give it source links or artifact references, and inspect its answer before continuing the local task. Use when the user wants ChatGPT Sol to analyze, translate, review, or propose work without delegating to a Codex task.
---

# ChatGPT Sol Review

Use ChatGPT as a separate reviewer or worker, then make the final decisions and
perform the requested local work yourself.

## Start a fresh ChatGPT conversation

- Create a new ChatGPT conversation for every independent work item. Never reuse
  a conversation that handled a previous item. Follow-ups in one conversation may
  only clarify or continue that same item.
- Use Sol with reasoning effort `high`.
- This must be a ChatGPT conversation, not a Codex task. Do not call a Codex task
  creation tool as a substitute. If available controls cannot create a new
  ChatGPT conversation, ask the user to create one and provide or open it.

## Recover from a stopped or stalled response

- Treat a visible stopped state (for example, the UI says that thinking was
  stopped) as an interrupted response, not as a completed review.
- Let high-reasoning analysis and repository inspection run to completion
  whenever the UI is making progress. Do not stop a response merely because it
  is taking several minutes or because intermediate progress is repetitive;
  use bounded polling and continue waiting. Stop or recover only for a clearly
  stalled response, an explicit user request, or an actual UI/tool failure.
- Before deciding that a response stopped, inspect the composer button again.
  A blue circular button containing a white square and labelled `回答を停止`
  means the response is still running; do not send a continuation while that
  button is present. A blue circular button containing the white waveform
  icon, with the stop-square action gone, is the fully stopped state and may
  be recovered. Prefer the accessible label/state over the icon when the UI
  exposes both.
- Re-observe the ChatGPT tab before acting and verify that it is the intended
  conversation. Send one short continuation message such as `続けて` to the
  same conversation, then wait for the response. Do not repeat the message in a
  loop.
- If the same conversation stops again, remains stalled, or its context is no
  longer reliable, keep that conversation tab open: a separate ChatGPT chat is
  completely context-free and closing the old tab is not a handoff. Record the
  last usable response, fixed blob/path ledger, and next action, then recover in
  that same conversation when its composer is live. Do not silently replace it
  with a fresh chat or assume the new chat inherited any investigation.
- Use a fresh ChatGPT conversation only as an explicit last-resort replacement
  when the original is kept open and the new prompt repeats the full task,
  paths, authoritative source revision, fixed artifacts, and constraints. Mark
  the old conversation as still unfinished until its work has a terminal
  PASS/NO-OP or the user explicitly abandons it. Apply the normal review rules:
  the returned text is a proposal, and local authoritative sources decide the work.

## Computer Use and GitHub invariants

- Re-observe a newly opened tab after the page settles. Accessibility-tree
  indices can reflow, so never reuse an initial textbox or submit-button index.
  Locate the live `prompt-textarea`, set the prompt, then locate the live
  `composer-submit-button` and click it. Pressing Enter alone is not sufficient;
  confirm the prompt is visible in the conversation and `回答を停止` appears
  before treating the request as submitted and running.
- Rebind a known tab with `cua.getTab` when a diff is ambiguous and inspect its
  full current accessibility state. A no-change result from `getAXState` is not
  proof that the response or remote repository is unchanged; independently
  inspect the live composer/stop button and, when needed, use a small DOM
  check. Independently poll `git ls-remote origin refs/heads/develop2`, then
  fetch/pull and verify the commit parent, exact changed paths, and blob IDs
  locally.
- Treat `回答を停止` (the blue stop-square action) as the running-state
  authority even when the visible progress text mentions a connection
  interruption or waiting for completion. Do not send another prompt while it
  is present. Detect an explicit delivery failure such as
  `メッセージ配信がタイムアウトしました` / `Message delivery timed out`
  together with a `再試行` / `Retry` control as a terminal timeout state. Do
  not click `再試行` / `Retry`: it restarts the answer and can discard the
  usable partial work. After the stop-square is gone and the composer is live,
  send exactly one short same-chat continuation, `Continue.`, then wait for
  the response. Do not send that continuation while the stop-square is still
  present. If the resumed answer is still incomplete, record the usable
  revision/byte ledger and switch to local reconstruction or an exact
  missing-fragment fallback instead of repeatedly restarting the investigation.
- For large generated files, keep the fixed blob SHA and size in the working
  ledger. If analysis/tests finished but upload or commit timed out, send a
  short same-chat continuation that resumes from those fixed blobs and finishes
  the tree/commit/push; do not restart the broad investigation. A focused test
  pass without a verified commit/push is incomplete.
- Prefer a verified GitHub write, commit, and push. If the first GitHub write,
  commit, or push fails after the generated pair is ready, do not leave the
  result as an uncommitted report or switch to attachment recovery yet. After
  the stop-square is gone, send exactly one concrete
  same-chat continuation rather than a bare `Continue.`: state which GitHub
  operation failed and ask ChatGPT to resume from the already-finalized bytes,
  repair only the transfer/commit/push, refresh `develop2`, and finish the
  GitHub commit/push. Do not click the UI Retry control, regenerate the pair,
  or restart the broad investigation. A focused test pass without a verified
  commit/push is incomplete. If that concrete continuation also fails, or
  ChatGPT explicitly cannot write GitHub, use attachment recovery as the final
  last resort: ask for each completed target as a separate directly downloadable
  file attachment (one `.hpp` and one `.cpp`), never inline source, a fenced
  code block, or a ZIP. Require the exact repository-relative path, byte count,
  and Git blob SHA-1 for every attachment, and preserve the existing finalized
  bytes rather than regenerating them. Install an attachment only after its
  bytes and blob ID are independently verified, then perform the local
  commit/push; if only one file arrived, request only the missing individual
  file and do not restart or regenerate the pair.
- Attachment recovery is a per-file, byte-exact procedure. Keep the source chat
  tab open until every attachment is installed and independently verified. Open
  each artifact through its own file preview (or use the direct downloaded file
  when one was actually created); never copy the visible answer, a partial
  selection, or a generated code block. Confirm the artifact's repository path,
  encoding/newline convention, declared byte count, and Git blob SHA-1 before
  transfer. If the host download is missing, the preview may be a virtualized
  CodeMirror editor: scroll `.cm-scroller` in bounded increments, collect every
  `.cm-line`'s `textContent` keyed by absolute vertical position plus a stable
  tie-breaker, scan the whole editor twice, sort by position, and require both
  scans to have identical ordered line counts and text. Use `textContent`, not
  `innerText`, because a blank CodeMirror line can add an extra newline. Preserve
  blank lines, encoding, and line endings; do not guess a line ending that is not
  stated or independently derivable. Transfer the exact text/bytes in bounded
  chunks through the local bridge, without text-mode normalization or shell
  redirection. Reconstruct only the individual missing file, then compute its
  byte count and Git blob SHA-1 (`SHA-1("blob " + decimal_byte_count + NUL +
  bytes)`) locally. Install it only when both values match the artifact report;
  otherwise discard the candidate and request that same individual attachment
  again. Verify the pair and the exact two-path diff before the local
  commit/push; never treat a partial, reordered, normalized, or unverified
  recovery as complete.
- Two recovery details are mandatory. A CodeMirror document ending in an empty
  line already represents one terminating `\n`; when converting it to an
  `apply_patch` Add File payload, omit the patch-only synthetic empty line so a
  second newline is not introduced. Verify the installed file immediately and
  correct any one-byte terminator mismatch before staging. Also do not assume
  the browser download or browser clipboard reaches the host: the in-app
  browser's clipboard/download context can be separate. Prefer a localhost-only
  exact-text bridge (or the actual downloaded file when its path is visible),
  and never use an external paste service or text-mode shell redirection.
- Prompts should include the current source revision/blob, exact native paths,
  the instruction to refresh `develop2` immediately before committing, and the
  no-`git clone` constraint. Recheck the C# blob immediately before generation:
  an earlier retrieval may be stale if the branch advanced.
- Distinguish implementation, correction, and final review. Review the actual
  commit against the C# source blob in the same conversation; if a defect is
  found, make the smallest correction in that pair, rerun focused checks, and
  re-review it. Once PASS/NO-OP and SHA evidence are terminal, close the tab.
- Do not re-audit pairs already marked audited or complete in the migration
  ledger. Start the next genuinely missing pair, while keeping two independent
  items live when possible; preserve concurrent non-overlapping commits by
  refreshing `develop2` before every write.

## Keep the browser footprint small

- Keep at most two ChatGPT tabs for active work: the current implementation or
  recovery conversation and one independent review conversation when both are
  genuinely running. Do not open a new tab merely to duplicate a stalled or
  completed chat.
- Never close a tab whose response is running, interrupted, stalled, or still
  awaiting implementation/push/audit. A new ChatGPT chat has no context and
  cannot inherit the old chat's reasoning or generated artifacts; closing it
  loses the recovery path rather than handing it off. Keep the unfinished tab
  until its terminal response/SHA or an explicit user abandonment, even when
  a fresh replacement is opened.
- After recording the final response, commit SHA, or explicit NO-OP for an item,
  close its browser tab with the browser tab close control. Close obsolete,
  duplicated, rate-limited, and completed migration tabs promptly so long runs do
  not exhaust desktop memory.
- Preserve a tab the user is actively viewing or explicitly asked to keep, even
  when it is otherwise complete. Before closing, verify that it is not the
  current user-designated tab and that no response is still running.
- If the browser session drops tabs, reopen only the active conversation by its
  known URL, inspect its last state, and do not resend the original prompt unless
  the conversation proves that it was never submitted.

## Run two migration items in parallel

- Never leave a completed ChatGPT tab idle while another independent migration
  item is available. As soon as the terminal response/SHA or NO-OP is recorded,
  use that tab slot for the next genuinely missing pair (or close it if no
  replacement is ready); do not wait for the other tab to finish. An apparently
  quiet tab is not a reason to assume completion: recheck the stop/retry/composer
  state first, and if it is terminal, advance the task ledger and dispatch the
  replacement in the same control cycle.

- For the long-running C#-to-C++ migration, keep two genuinely independent work
  items active whenever the browser and account permit it. Each item may be an
  implementation, a review, or a correction/re-review cycle, but never run two
  conversations that can edit the same files.
- Maintain a small task ledger in working notes or commentary with, for each
  item, the C# source path, Native paths, phase (`implementation`, `waiting`,
  `sync`, `review`, `correction`, or `done`), current commit SHA, and next action.
  Refresh the ledger whenever a response, commit, review result, or blocker
  changes the state.
- When one item reaches a terminal result, record its SHA or explicit NO-OP,
  close its finished tab, and start the next independent item immediately in the
  same control cycle so the active count does not unnecessarily drop to one.
  Keep a tab only while it has a live response or is needed for a follow-up;
  do not keep completed tabs open merely to maintain the count.
- Before starting a replacement item, verify that the remaining active item is
  still running or awaiting a concrete next step. Before committing, each worker
  must refresh `develop2` and preserve any concurrent non-overlapping commits.

## Supply the task efficiently

- Give ChatGPT a directly accessible repository, document, page, or artifact link
  plus exact paths, sections, revisions, and desired output whenever possible.
- Avoid pasting a full large file when the link exposes the same content. Paste
  only unavailable material or the smallest excerpt needed to resolve ambiguity.
- State the authoritative source and the constraints that must not drift. Ask for
  concrete mismatches, omissions, edge cases, and actionable output rather than a
  generic summary.
- Do not include secrets, private credentials, or unrelated workspace content.

## Use the response

- Read the response before acting. Treat it as a review or proposal, not as
  authority: verify claims against the authoritative source and the actual local
  interfaces.
- Preserve the user's scope. Do not adopt invented behavior, unrelated redesigns,
  or broader changes merely because ChatGPT suggested them.
- If the response is truncated, request only the missing continuation. If a claim
  is unclear, ask a focused follow-up in the same conversation; do not restart the
  whole prompt.
- ChatGPT conversations may not support task-wait APIs. Poll sparingly with the
  available chat-reading control, or inspect the conversation in ChatGPT after
  allowing time for Sol high to finish.

## Complete and verify the local task

- Apply only verified conclusions using the normal tools and conventions of the
  current workspace.
- Preserve unrelated work and respect all existing authorization boundaries.
- Run focused verification appropriate to the artifact, then broader checks only
  when the change affects shared integration.
- Report what ChatGPT materially influenced, what was verified locally, and what
  remains unverified. Do not present ChatGPT's unverified claims as completed work.

## Learned operational runbook

This section records the concrete ChatGPT Chat workflow that has proved reliable
for the Fruity-Prime migration. It supplements the rules above; when a UI state
is ambiguous, the live accessibility state and the Git repository are the
authority, not a stale screenshot or an earlier chat message.

### 1. Scope and session setup

- Use the ChatGPT web chat through Computer Use. Do not replace it with a Codex
  task, a delegated agent, or a direct local implementation when the user asked
  for ChatGPT Chat to do the analysis/work.
- Start one fresh ChatGPT conversation per independent migration item. A new
  conversation is context-free: it does not inherit source inspection, fixed
  bytes, commit state, or conclusions from another tab. Follow-ups and recovery
  messages stay in the same conversation as the item they belong to.
- Use the repository link
  `https://github.com/Zection6V/Fruity-Prime/tree/develop2` and state the
  branch explicitly. Do not paste an entire source file when the GitHub link is
  sufficient. If a document or source is needed by ChatGPT, it must already be
  committed and pushed; a local Windows path is not visible to that chat.
- Say explicitly that ChatGPT cannot use `git clone` and must use the GitHub
  app/tools. Never ask ChatGPT to infer a local checkout from a `C:\...` path.
- The messages sent to ChatGPT are English. Do not waste a prompt saying “Sol
  High”; the skill/UI selection already supplies the high-reasoning ChatGPT
  model. If the UI exposes a reasoning selector, keep it at High.
- Do not ask the user for routine permission to send the already-authorized
  migration prompt. Ask only when a genuinely new external side effect or an
  out-of-scope choice is required.
- The native counterpart is normally a colocated `.hpp/.cpp` pair with the
  same feature-directory shape as the C# source. Do not put a new header in a
  generic `include` directory and do not invent a filename that has no C# basis.

### 2. Select and dispatch work

Before sending a prompt, inspect the current local branch, the current remote
head, the authoritative implementation-order document, and the migration
ledger. A historical build-fix document can identify the next frontier, but it
cannot prove that the frontier is still current. Do not re-audit a pair already
marked terminal PASS/NO-OP unless a later shared-owner change explicitly
invalidated it.

Keep a small live ledger (commentary or working state is enough; do not create a
repository file just for this) with:

```text
item | C# oracle | native hpp/cpp | phase | current SHA | next action
```

Use phases `implementation`, `waiting`, `sync`, `review`, `correction`, and
`done`. Record every pushed SHA, parent, changed path, blob ID when available,
CI run IDs, and any `STRICT SEMANTIC BLOCKED` external owner. This prevents a
completed tab from becoming an idle tab and prevents an old plan from sending
the same file again.

Dispatch two genuinely independent items at once whenever the browser/account
permits it. Never put two workers on overlapping files or a shared owner that
one worker may edit. Typical safe pairings are two different C# file pairs or a
pair implementation and a read-only build-frontier investigation. Do not wait
for the first chat to finish before using the second slot.

The implementation prompt should include, in this order:

1. the repository link and `develop2` branch;
2. the exact implementation-order document/section and current source revision;
3. the authoritative C# path;
4. the exact native `.hpp/.cpp` paths and relevant existing call sites;
5. the C#-only specification and colocated-header constraints;
6. the instruction to inspect the live branch immediately before writing,
   refresh `develop2`, preserve concurrent non-overlapping commits, commit and
   push normally, and report the exact SHA; and
7. an English-only response requirement.

Use language equivalent to:

```text
Treat the C# file as the sole specification. Reproduce its public and
internal-equivalent API, defaults, static state, initialization timing, side
effects, ordering, exception/null/boundary behavior, ownership/reference
identity, and platform branches exactly in C++20. Existing native behavior is
not authoritative. Do not add a native-only policy, redesign, approximation,
shim, or unrelated cleanup. Keep the HPP beside the CPP at the matching
feature path. You cannot use git clone; use the GitHub app/tools. Refresh
develop2 immediately before writing, preserve concurrent commits, commit and
push normally, and report the exact commit/parent, paths, blobs, diff, parity
findings, and exact-SHA CI status in English.
```

For build-frontier work, give the exact compiler diagnostics and order document
but still require a file-slice audit; fixing one compiler line is not a parity
completion. For a GitHub Actions task, explicitly request separate Windows,
macOS, Linux, and Android workflows, each independently runnable, plus one
aggregate workflow, and dependency caching in every platform workflow. Do not
let a workflow convenience become a substitute for C# behavior.

### 3. Submit and observe without stale UI indices

- After opening or rebinding a tab, obtain a fresh full accessibility tree. Do
  not reuse textbox or submit-button indices from an earlier tree; the DOM
  reflows after every message.
- Locate the live `prompt-textarea`, paste the message, then locate the live
  `composer-submit-button` and click it. Pressing Enter is not sufficient proof
  of delivery: verify that the prompt appears in the conversation and that the
  blue `回答を停止` stop-square is present.
- After every click, paste, scroll, or navigation, obtain a fresh AX state before
  deciding the next action. A no-change diff is not proof that the remote branch
  or response is unchanged.
- Keep no more than two ChatGPT tabs for active work. When one item reaches a
  terminal SHA/PASS/NO-OP, record it, close that completed tab unless the user is
  actively viewing it or a follow-up is still required, and dispatch the next
  genuinely missing item in that slot. Never leave a completed tab idle while
  another independent item is available.
- If tabs disappear, reopen the exact known conversation URL, inspect its last
  message, and resend nothing unless the original prompt is demonstrably absent.
  Closing a stalled or unfinished chat is not handoff; keep it open for recovery.

### 4. Wait, timeout, and continuation state machine

High-reasoning ChatGPT answers can legitimately take 20 minutes or more. Do not
send progress nudges or repeated `continue` messages merely because the visible
text has not changed. Poll the specific tab in bounded intervals (normally
30–60 seconds, never a blocking wait longer than 60 seconds) and keep the user
informed only when the state materially changes.

Use the composer control as the state authority:

| Live state | Required action |
|---|---|
| Blue `回答を停止` stop-square exists | The answer is still running, even if the text says `思考中`, `接続が中断されました`, or “waiting”. Wait; do not send another prompt or click stop. |
| Stop-square disappears and a normal composer/submit control is live | The answer is stopped or terminal. Inspect the full response before deciding whether one continuation is needed. |
| `メッセージ配信がタイムアウトしました` / “Message delivery timed out” plus `再試行` / `Retry` | This is a delivery timeout, not permission to restart. Never click Retry. Preserve the partial response, wait until the stop-square is gone, then send exactly one same-chat continuation. |
| Final answer has response actions and no stop-square | Record the result/SHA and advance the ledger; do not send a redundant follow-up. |

For a delivery timeout after analysis/implementation is already complete, send a
concrete English continuation such as:

```text
The previous answer delivery timed out while you were finishing the report.
Do not press Retry and do not restart or redo the implementation. Continue from
the existing completed state and provide only the final exact-SHA report and
remaining CI status. Do not begin another work item.
```

For an interrupted response that was still working, use one same-chat
`Continue from the current state.` only after the stop-square has disappeared.
Do not loop this message. If a resumed response fails again, preserve the old
tab and ledger and switch to the exact-artifact fallback below instead of
restarting the broad investigation.

A bare `Continue` is not enough after a GitHub operation failure: it may cause
ChatGPT to repeat the failed push. State the exact failed operation and request
recovery from the already-finalized bytes, for example:

```text
The finalized files are unchanged; the GitHub commit/push failed at <operation>.
Do not regenerate or restart the audit. Repair only that transfer, refresh
develop2, commit and push the existing bytes, and report the resulting SHA.
```

### 5. Commit, audit, and CI closure

The normal lifecycle for each item is:

```text
fresh implementation chat
  → exact C#-to-C++ implementation
  → normal commit/push to develop2
  → same-chat strict post-commit audit
  → smallest pair-local correction if needed
  → commit/push and repeat the audit
  → terminal PASS/NO-OP report
```

The audit prompt must name the exact committed SHA and ask ChatGPT to compare
the actual committed blobs against the C# oracle across API shape, static and
default state, initialization/side-effect order, timing units, locking/thread
behavior, ownership/reference identity, null/exception/message boundaries,
collection order, encoding/culture, platform branches, and all observable
outputs. Existing C++ convenience behavior is not a valid answer.

When an audit reports `STRICT SEMANTIC BLOCKED`, do not force an invented bridge
inside the pair. Record the precise missing canonical owner (framework,
runtime, shared input, Android/JNI, renderer, collection, or exception owner),
its dependency, and the remediation path. Continue the implementation order
with independent work while leaving that pair's status blocked. Re-audit only
after the canonical owner exists; a focused compile pass alone is not PASS.

Before accepting a ChatGPT completion, verify locally and remotely:

- source C# blob and final HPP/CPP blob IDs;
- exact commit SHA, parent, branch, changed paths, and no unrelated edits;
- local `HEAD == origin/develop2` after fetch, without deleting unrelated
  untracked/dirty work;
- focused compile/tests appropriate to the pair; and
- exact-SHA CI results for Windows, macOS, Linux, and Android, separating
  success, still-running, skipped/unrun, asset/setup failure, and an unrelated
  later compiler frontier.

A CI failure in `TeleporterEntity.cpp`, an asset gate, or another later file is
not a failure of a pair that compiled cleanly, but it is also not an all-green
build. Never call the whole migration complete from a static audit, a pushed
commit, or one platform's result.

### 6. Last-resort file recovery, not source-text recovery

Prefer GitHub commit/push. If the finalized bytes exist but ChatGPT cannot push:

1. Send the concrete same-chat push-recovery prompt once.
2. If that also fails, ask ChatGPT to attach each completed target as a separate
   directly downloadable file: one `.hpp` and one `.cpp`, with exact
   repository-relative path, byte count, line-ending/encoding convention, and
   Git blob SHA-1.
3. Say “attach the file” explicitly. Do not say only “continue”, because that
   invites another failed Git operation. Do not request a full source dump,
   fenced code block, answer-copy, or ZIP as the normal fallback. ZIP is not the
   per-file fallback; it is usable only when the user has already supplied one.
4. Keep the source chat open until every individual attachment is installed and
   verified. The `回答をコピーする` button copies the answer prose; it is not a
   byte-exact file transfer. Open the individual file preview/download instead.
5. If no host download exists and the preview is a virtualized CodeMirror
   editor, perform the two-pass `.cm-scroller`/`.cm-line` collection described
   above, keyed by absolute position and using `textContent`. Require identical
   ordered line counts and text in both scans, preserve blank lines, encoding,
   and line endings, then compute the local byte count and Git blob SHA-1 before
   installation.
6. Reconstruct only the missing individual file with exact bytes, verify the
   pair and two-path diff, then perform the local normal commit/push and ask
   the same ChatGPT chat for the strict audit. If only one file is missing,
   request only that file; never regenerate the pair.

If the user has already downloaded a ZIP or file pair, do not ask ChatGPT to
commit it again. Inspect the archive/file bytes locally, verify paths and blob
IDs, install the exact files, commit/push locally, and use ChatGPT for the
post-commit audit.

### 7. Finish, pause, and handoff

When the user says to stop after the current two items, finish only their
terminal reports, synchronize and verify the branch, and start no replacement
item. Report unfinished CI as unfinished rather than silently waiting forever.
When the user has not requested a pause, keep two independent items moving;
one completed item should immediately free a slot for the next order entry.

The final status must identify the two item paths, phase, SHA, audit result,
CI caveat, and any preserved dirty/untracked path. Do not claim that a tab is
finished merely because it looks quiet, and do not claim that a task is
complete merely because a response timed out or a commit was attempted.
