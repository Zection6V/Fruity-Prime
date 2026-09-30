---

name: chatgpt-sol-review-general

description: Use a fresh ChatGPT Sol high conversation per work item to implement tracked-file changes directly in the designated GitHub repository, audit and correct them in the same conversation, and verify the resulting commit. Use when the user wants work carried out in ChatGPT rather than a Codex task.

---

# ChatGPT Sol Review

Use ChatGPT as the implementation worker for tracked-file work. Edit the requested files directly in the designated GitHub repository, perform source comparison, audit, and corrections in the same conversation, and commit/push according to the repository workflow. If GitHub access prevents completion, report the concrete blocker.

## Direct Git-backed implementation

- Direct GitHub editing is the default for requested tracked-file changes. Use the available GitHub tools on the designated repository and branch, and edit only the requested paths.
- Before editing, confirm the repository, target branch, authoritative source revision, and exact target paths. Derive the repository from the configured remote or explicit user designation; do not guess the canonical upstream from a project name. Verify that the selected repository contains the source/target paths and that the connected GitHub account can write there. Keep implementation, source comparison, audit, corrections, and final review in the same work-item conversation. Refresh the target branch immediately before writing, then commit/push and verify the resulting commit, changed paths, and relevant checks.
- Once the user has authorized a work item and specified its repository and target paths, treat that as standing authorization for the in-scope ChatGPT messages and GitHub edits, commits, and pushes needed to complete it. Do not pause for repeated conversational permission checks; ask again only if the next action expands the authorized scope or a higher-priority tool policy specifically requires fresh confirmation.
- Use local inspection only for independent, read-only verification of the committed GitHub result. If the audit finds a defect, have ChatGPT correct the GitHub files directly in the same conversation and re-verify the new commit.

## English ChatGPT communication

- Write every message to ChatGPT in English, including initial prompts, follow-ups, correction requests, and recovery messages. Require every natural-language response from ChatGPT, including its final audit and status report, to be in English. The requested translated file itself must, of course, be in the target language and read naturally.

## Keep translation, source audit, and correction in one chat

- For a per-file translation, use one fresh ChatGPT conversation for that file and keep every phase in that same conversation: translate the complete source into natural, idiomatic Japanese directly in the target GitHub file; compare it against the authoritative source; audit for omissions, meaning drift, and untranslated user-facing text (including code comments); correct any findings in that file; then commit/push and report the audit summary in English. Do not split translation, audit, and correction across separate chats, and do not treat the first translation draft as complete.
- Ask ChatGPT to preserve the full information content and account for each source heading, paragraph, list/table item, note, and example comment. Preserve executable code, identifiers, links, IDs, and document structure unless the task explicitly calls for changing them.
- Audit language metadata too: a Japanese HTML translation must declare the correct target language (for example, `<html lang="ja">`) rather than retaining `lang="en"`.
- The same-chat audit and correction are part of the worker's task. Independently compare the committed GitHub file with the local authoritative source in read-only checks; if any issue remains, request a direct correction in the same ChatGPT conversation. Mark the file complete only after the audit and commit verification pass.

## Start a fresh ChatGPT conversation

- Create a new ChatGPT conversation for every independent work item. Never reuse a conversation that handled a previous item. Follow-ups in one conversation may only clarify or continue that same item.

- Use Sol with reasoning effort `high`.

- This must be an ordinary ChatGPT conversation, not a Codex task or ChatGPT Work cloud task. Do not use a task-creation tool as a substitute. Create each new conversation in a real ChatGPT browser tab. If ChatGPT does not expose a conversation ID until the first message is sent, compose and send the complete English work-item prompt in that tab; do not send a placeholder. Keep the tab open while the item is active or unfinished.

- Use Computer Use in the visible ChatGPT tab for every message, reply inspection, and generation-status check. Do not use Codex app conversation tools (`send_message_to_thread`, `read_thread`, `list_threads`, or `wait_threads`) for this workflow; their status can disagree with the actual tab and can leave the real chat unopened. Identify/reopen conversations through the ChatGPT browser history/sidebar and verify the tab URL/title. If browser access fails, report that concrete blocker rather than switching to conversation tools, a Codex task, or ChatGPT Work cloud task.

## Recover from a stopped or stalled response

- Treat an incomplete response as unfinished, even if the conversation is no longer active. A stopped response is not a completed review.

- Let high-reasoning analysis and repository inspection run to completion while the actual ChatGPT tab shows generation in progress. Replies may take 20 minutes or more; do not interrupt just because the work is lengthy or progress text is repetitive. Check the visible tab periodically without sending duplicate messages.

- Determine status from the actual ChatGPT composer and latest visible exchange, not an app/API status flag. The rightmost composer control is decisive: a visible Stop/停止 control means generation is still active, so do not send. When Stop disappears and the rightmost control returns to the voice-mode waveform icon, treat that as the response having stopped and the composer being ready. If the latest requested work is unfinished or the answer stopped mid-task, send one concise continuation immediately in that same tab; do not wait 20 minutes and do not click Retry/再試行. A stale `active` report from another tool never overrides the current visible UI.

- When the UI indicates idle as above and the work is unfinished—because the answer visibly stopped mid-task or the latest accepted user message has no assistant response—send exactly one concise English continuation through the ChatGPT tab. Name the concrete remaining action; do not resend the original full prompt. Verify that the message appears in the conversation and that generation starts. Never send duplicates while the same idle composer state persists. If that continuation is accepted and generation later stops again (waveform returns), treat it as a new interruption and immediately continue only the remaining action in the same tab. Keep and reopen the same conversation; do not start another worker for that item unless replacement is authorized. If the browser is unavailable or an external blocker prevents progress, record the item as `parked` with its URL/ID, fixed source/target paths and blobs, last known branch/commit, last usable response, and next action. Continue independent in-scope work in another tab when possible; do not let an idle chat freeze the whole goal.

- Do not use elapsed time alone to decide whether to send: a response can legitimately take 20 minutes or more while Stop/停止 is visible. Once Stop is replaced by the voice-mode waveform and the work is unfinished, send the continuation immediately, even if a previous recovery was unanswered; the visible stopped state is the trigger.

- If the same conversation stops again, remains stalled, or its context is no longer reliable, keep the work item unfinished. Record the last usable response, fixed blob/path ledger, and next action; recover in that same conversation when it is idle. A separate ChatGPT chat is context-free and is not a handoff. Do not silently replace the conversation or assume another one inherited its investigation.

- If a replacement conversation is explicitly authorized and available, keep the original item unfinished until it has a terminal PASS/NO-OP or the user explicitly abandons it. The replacement prompt must repeat the full task, paths, authoritative source revision, fixed artifacts, and constraints. Verify the actual GitHub file and commit against the authoritative source; a chat message alone is not evidence that repository changes were completed.

### When a conversation reaches its length limit

- Treat ChatGPT's explicit “This conversation has reached the maximum length” notice as a hard limit on continuing that conversation, not as an ordinary stopped response. Do not keep sending continuations or use Retry there.
- Before acting on a previously observed length-limit, reopen the same conversation from ChatGPT's visible history/sidebar and re-check its latest exchange and composer. Earlier screenshots, summaries, and API status can be stale. If the same chat now has an accepted follow-up and shows Stop, keep working in that chat; do not create a duplicate replacement. Start the fresh-chat handoff only when the currently visible conversation still shows the explicit length-limit state and no generation is in progress.
- If the authorized work remains unfinished, start a new ordinary ChatGPT conversation and make its very first message a complete handoff. A new chat has no inherited context: include the repository/branch, exact work item and source/target paths, authoritative revisions or blobs, current remote commit, completed steps and their evidence, decisions and constraints, unresolved issues, and the concrete next action. Link the old chat for provenance only; never assume the new chat can read or inherit it.
- Recheck the live branch and any referenced repository documents before dispatch. Include concise local-only facts in the handoff when the new chat cannot read them; do not rely on uncommitted local paths as remote evidence.
- Keep the length-limited conversation's URL/ID and last usable result in the task ledger as `length-limited`; do not label unfinished work complete or forget its artifact state. Verify that the new handoff message was accepted and generation started before treating the transition as successful. Preserve the old tab until this is verified, and keep it open if the user is viewing it.
- After a verified handoff, continue implementation, audit, correction, and Git verification in the new conversation. Do not redo completed steps unless the handoff reveals a concrete verification gap.

## ChatGPT tab and GitHub invariants

- Before typing, confirm the tab URL/title and inspect the latest exchange. Use the same tab for all follow-ups, audits, and corrections. After typing a message, verify that it was actually sent (it appears as a user message, the composer is cleared, and generation begins); a ready-to-send composer alone is not proof of delivery.

- For an already-authorized work item, send in-scope progress and the single recovery continuation directly in its ChatGPT tab without asking for per-message confirmation. Do not use app conversation APIs as a shortcut. If the user rejects one proposed message, do not send that wording; treat the rejection as limited to that message unless the user explicitly withdraws or pauses the work item.

- Independently poll `git ls-remote origin refs/heads/<target-branch>`, then fetch/pull and verify the commit parent, exact changed paths, and blob IDs locally.

- If the response is incomplete after it has stopped, ask in the same chat to finish only the remaining GitHub file edit or audit; do not reconstruct or substitute a local file.

- For large generated files, keep the fixed source blob SHA and size in the working ledger. If analysis/tests finished but the GitHub write or commit timed out, send a short same-chat continuation that resumes from the finalized content, refreshes the target branch, and completes the direct write/commit/push; do not restart the broad investigation. A focused test pass without a verified commit/push is incomplete.

- If the first GitHub write, commit, or push fails after the target content is ready, do not leave the work uncommitted. When the ChatGPT tab shows the response stopped (voice-mode waveform instead of Stop/停止), send a concrete English continuation in the same chat: identify the failed GitHub operation, ask ChatGPT to resume from the finalized content, refresh the target branch, and complete the direct GitHub write/commit/push. Do not regenerate the target content or restart the broad investigation. If GitHub editing is unavailable, stop and report the concrete blocker for user direction.

- Prompts should include the current source revision/blob, exact target paths, the instruction to refresh the target branch immediately before committing, and the no-`git clone` constraint. Recheck the authoritative source blob immediately before generation: an earlier retrieval may be stale if the branch advanced.

- Distinguish implementation, correction, and final review. Review the actual commit against the authoritative source blob in the same conversation; if a defect is found, make the smallest correction in those target files, rerun focused checks, and re-review it. Once PASS/NO-OP and SHA evidence are terminal, mark the work item complete.

- Do not re-audit work items already marked audited or complete in the task ledger. Start the next genuinely missing item, while keeping two independent items live when possible; preserve concurrent non-overlapping commits by refreshing the target branch before every write.

## Keep at most two independent work items active

- Keep at most two ChatGPT work-item conversations with live responses or ongoing implementation/audit/correction activity, one per independent file or work item. Parked no-response conversations remain in the ledger but do not count against this cap. Never use a live slot for duplicate or overlapping work. Prefer two genuinely independent items in parallel when the account and available conversations permit it.

- Track each item by its ChatGPT URL/ID and keep its actual tab open while active or unfinished. Use browser tabs for messaging and status, not app conversation tools. Keep at most two active-work tabs, one per independent item; completed-tab cleanup is defined under Completion and cleanup.

- Never abandon or erase a conversation that is active, interrupted, stalled, or awaiting implementation/push/audit. For an idle unfinished conversation, send one same-chat continuation first; if still unanswered, park it rather than letting it freeze unrelated work. A new ChatGPT chat has no context and cannot inherit the old chat's reasoning or generated artifacts; keep the unfinished item in the ledger until its terminal response/SHA or explicit user abandonment.

- `parked` means deferred, not forgotten. Revisit parked items after each other work item reaches a terminal result and whenever no independent task remains to keep progressing. First inspect the same tab and remote target branch: a commit may have arrived without a final report. If the voice-mode waveform shows that the response stopped and the item is incomplete, send one concise continuation immediately in that same chat, subject to the two-live-item limit. Do not send duplicates while Stop/停止 is visible. After each stopped response, either record terminal evidence or resume only the concrete remaining action; never leave an idle tab waiting for the user to click Continue.

- Once PASS/NO-OP and SHA evidence are recorded, mark the item terminal and reuse its slot for the next genuinely missing item. Close that item's browser tab only after matching the tab to its conversation ID and verifying the terminal same-chat audit and exact commit/path/blob evidence. Do not leave a completed tab open between items or turns: close it immediately after verification to conserve browser memory. Closing a tab does not delete, archive, or otherwise alter the conversation; reopen it from chat history only if follow-up is needed. Keep tabs for unfinished, parked, recovery-required, or unrelated conversations.

- If the browser tab becomes unavailable, use ChatGPT's own history/sidebar to reopen that same URL/ID and preserve the last usable response, fixed blob/path ledger, and next action. Do not switch to app conversation tools or silently resend the original prompt.
- If Computer Use returns a stale tab/session-binding error (for example, a tab ID is not part of the current browser session), do not repeat the action with the stale handle or reuse its accessibility indexes. Reset the Computer Use JavaScript session once, make its required first call a fresh `cua.getState()` inventory, rebind the existing browser tab from that inventory, then verify the conversation URL/title, latest exchange, composer, and Stop/waveform state. Resetting the automation binding does not mean the conversation disappeared: preserve any draft and do not reload, recreate, or resend the conversation unless the fresh inventory proves the tab is absent; if absent, reopen it through the matching visible ChatGPT history row.
- For project conversations, prefer clicking the matching visible title row in the ChatGPT sidebar/history rather than navigating from a copied accessibility-link value. A serialized link value may omit a project-slug suffix and open a blank “New chat” even though the history row resolves to the correct conversation. Never reconstruct a `/g/.../c/<id>` URL from memory or combine a remembered project slug with the ID. After opening, verify the final URL/ID, title, loaded message history, latest exchange, and composer state; if the route is blank or mismatched, return to the sidebar and click the visible history row before concluding access is unavailable. A newly opened project route may briefly show a generic `ChatGPT` title, disabled composer, or loading skeleton; take one fresh state after the normal UI wait before deciding that it is blank. Reacquire the accessibility tree after each navigation or expansion and choose the next row from that fresh tree; do not reuse numeric element indexes from an earlier or diff-only snapshot.

## Run two independent work items in parallel

- Never leave one of the two work-item slots occupied by a completed item while another independent item is available. As soon as the terminal response/SHA or NO-OP is recorded, advance the task ledger and dispatch the next genuinely missing item in that slot; do not wait for the other item to finish. An apparently quiet conversation is not proof of completion: inspect its actual ChatGPT tab and latest response first.

- For long-running multi-item work, keep two genuinely independent work items active whenever the app and account permit it. Each item may be an implementation, a review, or a correction/re-review cycle, but never run two conversations that can edit the same files.

- Maintain a small task ledger in working notes or commentary with, for each item, the authoritative source path, target paths, phase (`implementation`, `waiting`, `parked`, `sync`, `review`, `correction`, or `done`), current commit SHA, and next action. Refresh the ledger whenever a response, commit, review result, or blocker changes the state.

- When one item reaches a terminal result, record its SHA or explicit NO-OP and start the next independent item immediately in the same control cycle so the active count does not unnecessarily drop to one. Keep a conversation active only while it has a live response or is needed for a follow-up; do not keep completed items active merely to maintain the count.

- Before starting a replacement item, verify that the remaining active item is still running or awaiting a concrete next step. Before committing, each worker must refresh the target branch and preserve any concurrent non-overlapping commits.

## Supply the task efficiently

- Give ChatGPT a directly accessible repository, document, page, or artifact link plus exact paths, sections, revisions, and desired output whenever possible.

- Avoid pasting a full large file when the link exposes the same content. Paste only unavailable material or the smallest excerpt needed to resolve ambiguity.

- State the authoritative source and the constraints that must not drift. Ask for concrete mismatches, omissions, edge cases, and actionable output rather than a generic summary.

- Do not include secrets, private credentials, or unrelated workspace content.

## Use the response

- Read ChatGPT's English audit/status report and inspect the actual committed GitHub files before marking the item complete. Verify claims against the authoritative source and local interfaces.

- Preserve the user's scope. Do not make unrelated changes outside the requested GitHub paths.

- If the response is truncated, request only the missing continuation. If a claim is unclear, ask a focused follow-up in the same conversation; do not restart the whole prompt.

- Inspect the actual ChatGPT tab for replies and status, allowing Sol high time to finish between checks. Do not use conversation APIs to poll or message; if the tab cannot expose the required reply/status, report that concrete limitation.

## Verify and close the task

- Verify the actual GitHub commit and changed paths independently. Keep any required implementation or correction in the same ChatGPT conversation and apply it directly to the designated GitHub files.

- Preserve unrelated work and respect all existing authorization boundaries.

- Run focused verification appropriate to the artifact, then broader checks only when the change affects shared integration.

- Report what ChatGPT materially influenced, what was verified locally, and what remains unverified. Do not present ChatGPT's unverified claims as completed work.

## Operational runbook learned from actual ChatGPT use

The following rules make the ChatGPT workflow reliable for long-running
tracked-file work. They supplement the repository-specific rules above. The
current visible ChatGPT UI is the source of truth for generation state; the
committed GitHub tree and exact blob/commit evidence are the source of truth
for repository changes.

### Session and prompt contract

- Use an ordinary ChatGPT conversation with the selected high-reasoning model,
  never a Codex task or ChatGPT Work cloud task. Do not spend prompt text saying
  “Sol High”; the model/reasoning selection is already the intended one.
- Create one fresh conversation for each independent file/work item. A fresh
  chat has no context from another chat: it does not inherit source inspection,
  fixed generated bytes, branch state, commit IDs, or audit conclusions. Keep
  all implementation, correction, and strict review for one item in that same
  conversation.
- Use English for every ChatGPT message, including implementation prompts,
  review requests, continuation messages, push-recovery requests, and the
  requested final report. Do not repeatedly ask the user for permission after a
  work item and repository paths have already been authorized.
- Give the exact repository, branch, authoritative source revision/blob, target
  paths, related callers, and the relevant order/document section. Push any
  supporting document before referring ChatGPT to it; a local `C:\...` path is
  not readable by the chat.
- State that ChatGPT cannot use `git clone`; it must use the GitHub app/tools.
  Require a branch refresh immediately before every write, normal commit/push,
  preservation of concurrent non-overlapping commits, and an exact SHA report.
- Prefer same-relative target paths and colocated headers. Do not create a
  generic include header, a project-only filename, or a native-only policy that
  has no source-file counterpart.

Use a prompt shape like this, replacing only the work-item data:

```text
Repository: <GitHub URL>, branch: <branch>
Authoritative source: <source path and blob/revision>
Target files: <exact tracked target paths>

Treat the authoritative source as the sole specification. Reproduce its full
API, defaults, state, initialization and side-effect order, timing, ownership,
null/exception/boundary behavior, and observable output exactly in the target
file. Existing target behavior is not authoritative. Do not add a redesign,
approximation, shim, fallback, or unrelated cleanup. You cannot use git clone;
use the GitHub app/tools. Refresh the branch immediately before writing,
preserve concurrent commits, commit and push normally, then report the exact
commit, parent, changed paths, blob IDs, diff, audit findings, and exact-SHA CI
status. Reply in English.
```

### Conversation creation and browser-tab discipline

- Create or reopen the ordinary ChatGPT conversation in a real browser tab, then
  use that same tab for all messages, reply inspection, and status checks. Do
  not call `mcp__codex_app__send_message_to_thread`, `read_thread`, `list_threads`,
  or `wait_threads` for this workflow. An API-reported active state can be stale
  while the visible composer is ready, and must not suppress a needed recovery.
- Confirm the intended conversation ID and current title before sending. After
  every send/read, inspect the latest visible exchange and composer. A no-change
  view is not proof that GitHub or the response is unchanged.
- Keep a compact ledger by conversation ID:

  ```text
  id | source | target | phase | SHA/blob ledger | next action
  ```

  Use phases `implementation`, `waiting`, `parked`, `sync`, `review`,
  `correction`, and `done`. Record the last usable response and concrete next
  action when a conversation stalls.

### Two-item parallel operation

- Keep at most two genuinely independent work-item conversations active. Never
  assign overlapping files or a shared owner that both chats may edit.
- Do not leave one slot occupied by a completed item while another independent
  item is available. Once a terminal SHA/PASS/NO-OP is recorded, close or mark
  the completed item as appropriate and dispatch the next missing item in that
  slot. A quiet conversation is not evidence of completion; inspect its status
  and latest response first.
- An unfinished/stalled conversation is not handed off by closing it. Keep its
  ID and fixed path/blob ledger. A parked no-response item does not consume a
  live execution slot, but it remains unfinished until the same conversation
  reaches a terminal result or the user explicitly abandons it.
- If the user asks to stop after the current two items, finish only their final
  reports, synchronize/verify the branch, and start no replacement item.

### Long waits, timeout, and continuation

High-reasoning repository work can legitimately take 20 minutes or more. Do
not send repeated nudges, duplicate prompts, or a new work item because the
latest text looks repetitive. Poll the specific conversation in bounded
intervals (normally 30–60 seconds; do not block for longer than 60 seconds) and
only report material state changes.

- While the composer shows Stop/停止, do not send another message. “Thinking”, a
  long tool phase, or a visible connection-wait message is not completion by
  itself.
- When Stop/停止 disappears and the composer's rightmost button returns to the
  voice-mode waveform icon, the visible UI indicates generation stopped and the
  input is ready. Inspect the latest exchange immediately. If the requested work
  is incomplete, send one concise continuation in that same tab right away; do
  not wait 20 minutes and do not click Retry/再試行. This explicit UI signal
  outranks any stale `active` status from a conversation API.
- If the waveform/voice-mode icon is present but the requested work has in fact
  completed, record the result and do not send a redundant message.
- If a visible ChatGPT UI shows `Message delivery timed out` /
  `メッセージ配信がタイムアウトしました` with `Retry` / `再試行`, never click
  Retry. Retry restarts the answer and can discard usable work. Wait only while
  Stop/停止 is still visible. As soon as the voice-mode waveform returns, send
  exactly one concise continuation in the same conversation if work remains.
- Treat a visible `ChatGPT stream recovery polling timed out` message the same
  way when the composer has returned to the voice-mode waveform: inspect the
  latest partial response, never click Retry, and continue only the unfinished
  action in the same conversation. Preserve any useful investigation already
  completed; do not assume the interrupted response made no progress.
- For an interrupted answer whose remaining action is only the final report,
  use a concrete continuation such as:

  ```text
  The previous answer delivery timed out while you were finishing the report.
  Do not press Retry and do not restart or redo the implementation. Continue
  from the existing completed state, provide only the final exact-SHA report and
  remaining CI status, and do not start another work item.
  ```

- A bare `Continue` is insufficient after a GitHub write/commit/push failure;
  it can make ChatGPT repeat the failed operation. State the failed operation
  and request recovery from the already-finalized content:

  ```text
  The finalized content is unchanged; the GitHub <write/commit/push> failed.
  Do not regenerate or restart the audit. Refresh the target branch, repair only
  that operation using the existing bytes, commit and push normally, and report
  the resulting SHA.
  ```

- If a response stops again after a recovery, resume only the remaining action
  immediately when the waveform returns; do not resend the full prompt. Never
  replace the original with a context-free chat. Keep a compact ledger of its
  URL/ID, fixed source/target blobs, last commit, last usable response, and next
  action. Never ask the user to send Continue or let an idle tab deadlock other
  independent work.

### Direct GitHub delivery and strict review loop

Use this lifecycle for every tracked-file item:

```text
fresh implementation chat
  -> direct GitHub edit
  -> normal commit/push
  -> same-chat strict audit of the committed blobs
  -> smallest correction in the target files if needed
  -> commit/push and re-audit
  -> terminal PASS/NO-OP with exact SHA evidence
```

- ChatGPT should perform the direct GitHub write and push. Local inspection is
  independent read-only verification unless the user explicitly asks for local
  delivery or attachment recovery.
- Before each write, refresh the remote branch and preserve concurrent commits;
  never force-push. Afterward independently verify `git ls-remote`, parent,
  exact changed paths, blob IDs, and the target branch's commit.
- The strict audit must compare the actual committed files with the source, not
  merely repeat the implementation answer. Check API shape, defaults/static
  state, initialization and side-effect ordering, timing units, ownership and
  identity, null/exception/message boundaries, collection order, encoding and
  culture, platform branches, and observable output.
- `STRICT SEMANTIC BLOCKED` is a recorded external-owner result, not permission
  to invent a local shim. Record the missing canonical framework/runtime/shared
  owner and remediation path, continue independent work, and re-audit only once
  that owner is available.
- Separate Git delivery from build/runtime/CI evidence. For every exact SHA,
  distinguish passed, failed, still-running, skipped/unrun, asset/setup failure,
  and a later unrelated compiler frontier. A clean file audit or pushed commit
  is not an all-platform green build. An alternate compiler or generator may be
  useful supplemental evidence, but it does not replace a required target-
  platform toolchain gate: do not reroute a failing Windows/MSVC validation to
  MinGW and then claim the Windows gate passed. Fix the canonical path or leave
  that gate explicitly failed/not established.

### Intermediate commits and evidence validity

- A long-running implementation can push a checkpoint while its ChatGPT tab
  still shows Stop/停止 and continues working. Poll the remote branch during
  long work, but treat each observed SHA as an immutable snapshot, not as the
  terminal result. Record its parent, changed paths, and checks; do not mark the
  item complete until the same-chat audit/correction is terminal and the final
  SHA is independently verified. An audit of an earlier SHA does not audit a
  later branch head.
- When the user requests a local build gate, it is useful to build the exact
  pushed snapshot even while the implementation chat is still active. Record
  the exact SHA, command, log path, and first relevant compiler errors. If the
  build fails, do not patch locally when ChatGPT owns direct GitHub edits; once
  the same implementation chat's composer is idle, send one specific
  continuation with those errors and ask it to correct and push. Rebuild the
  resulting exact SHA, not an assumed latest tree.
- Before a corrective push, inspect the workflow's branch concurrency policy.
  If `cancel-in-progress: true` means the push would cancel a live exact-SHA
  run, keep that branch snapshot unchanged until the run is terminal; diagnose
  the failure read-only in parallel. Then refresh the branch, preserve any
  concurrent commits, push the correction, and verify checks against the new
  exact SHA. Never treat the older run as evidence for the corrected commit.
- A generated screenshot, manifest, or successful capture command alone does
  not establish visual parity. Confirm that the intended fixture is actually
  visible and exercises the claimed behavior (including effective shader
  output such as alpha), and that baseline and target use equivalent scene,
  camera, resolution, settings, and capture timing. Identical/empty images may
  expose a fixture, visibility, or harness defect rather than renderer parity;
  fix the harness in the authorized work item and regenerate both sides under
  matching conditions before using the images as evidence.

### Push-failure attachment fallback

GitHub push is always preferred. If finalized content exists but the GitHub
write/commit/push cannot complete:

1. Send one concrete same-chat push-recovery message as above; do not regenerate
   the content.
2. If that recovery also fails or ChatGPT explicitly cannot write GitHub, ask for
   each completed target as a separate directly downloadable file attachment,
   with the exact repository-relative path, byte count, encoding/line-ending
   convention, and Git blob SHA-1.
3. Say “attach the file” explicitly. Do not ask for the full source inline, a
   fenced code block, an answer-copy, or a ZIP as the normal fallback. A ZIP
   supplied by the user may be installed locally, but it is not a substitute
   for the normal per-file attachment procedure.
4. Keep the source conversation open until every attachment is installed and
   independently verified. The “copy answer” action copies prose, not exact
   file bytes; use the individual file attachment/preview. If only one file is
   missing, request only that individual file.
5. When a preview is a virtualized CodeMirror editor, collect all `.cm-line`
   `textContent` values in two full bounded scans keyed by absolute position;
   require identical ordered line counts/text, preserve blank lines and line
   endings, and verify byte count plus `SHA-1("blob " + decimal_byte_count +
   NUL + bytes)` before installing.
6. After exact verification, perform the authorized local commit/push and ask
   the same conversation for the post-commit audit. Never treat a partial,
   normalized, reordered, or unverified attachment as complete.

### Completion and cleanup

- Mark an item complete only after the terminal response, exact commit/path/blob
  evidence, and required audit result are recorded. If CI is still running,
  report it as running; if it failed in a later unrelated file, report that
  caveat rather than claiming success or failure for the wrong item.
- Keep each active or unfinished conversation's browser tab open (mark it for
  handoff when the session may end) so its visible state can be monitored and
  continued. After terminal verification, close only completed/unneeded tabs to
  conserve browser memory, matching each tab to its conversation ID first.
  Closing a tab does not delete the conversation or its history. Never close an
  unfinished chat to “hand it off,” because a new chat cannot inherit its
  context.
- At each cleanup or work-slot transition, also close any tabs for work items
  already recorded as terminal from earlier steps. Do not keep completed tabs
  open while other items continue; if a tab's conversation ID cannot be matched
  confidently, resolve the identity before closing it.
- If local verification is performed, preserve unrelated dirty/untracked work
  and report it separately. Never use a broad reset, force-push, or cleanup as a
  shortcut for proving delivery.

### Phase-level completion and ordering

- When a task follows a multi-phase plan, use the exact current plan revision as
  the authority for phase order, scope, completion checkboxes, static audits,
  and runtime gates. A commit whose title names a phase proves only that a
  commit was made; it does not prove that the phase is complete.
- At dispatch and in every status report, name the active target phase
  explicitly. If another phase is present only as a comparison/baseline input,
  label it reference-only; never describe it as the current implementation
  target or count its checks as completion evidence for the active phase.
- Treat literal API/architecture lists in a phase as acceptance criteria;
  behavioral parity or a superficially present but unused API does not satisfy
  them. If they conflict with the existing backend, verify the concrete
  constraint and resolve it without silently weakening or rewriting the plan.
- Before advancing to the next phase, enumerate every applicable completion
  condition and runtime test from the plan, then attach current, exact-SHA
  evidence to each. Mark unrun or unavailable checks `NOT ESTABLISHED`; do not
  infer them from a successful build, a related commit, or another phase's
  results. Keep the phase unfinished until its required gates are verified.
- If the requested phase order differs from an older plan or a later section
  appears more convenient, follow the user's explicitly selected order and
  preserve dependencies; do not silently skip ahead or treat already-present
  implementation as newly completed work.

### Progress tracking in the plan

- When the user designates a plan document for ongoing progress, maintain a
  concise dated status section near its end. Keep it in the requested document
  rather than relying only on chat history or an ephemeral checklist.
- Update completion checkboxes per criterion only when evidence supports that
  criterion at a named source SHA. Record the exact evidence and leave other
  criteria unchecked; a partially checked phase is still incomplete until every
  required gate passes.
- Keep the last verified SHA separate from the latest remote SHA. State the
  local `HEAD`, remote branch tip, and any uncommitted paths when they differ;
  do not imply an audit or runtime result automatically covers a later commit.
- Record CI workflow run IDs/links, exact head SHAs, per-platform outcomes, and
  a timestamp for live status. Distinguish `PASS`, `FAIL`, `IN PROGRESS`,
  `CANCELLED`, and `NOT ESTABLISHED`; do not rerun a live run when a push would
  cancel it. Refresh the ledger after a commit, terminal job, or other material
  status change.
- End the status section with the concrete next actions and the condition for
  advancing phases. Preserve user-owned dirty work, and do not commit or push a
  local progress edit unless the user requested delivery.
