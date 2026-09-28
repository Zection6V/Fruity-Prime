---

name: chatgpt-sol-review

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

- This must be an ordinary ChatGPT conversation, not a Codex task or ChatGPT Work cloud task. Do not use a task-creation tool as a substitute. Create each new conversation in the browser. If ChatGPT does not assign or expose a conversation ID until its first user message is sent, submit the complete English work-item prompt as the creation action; do not send a placeholder. If an empty conversation already exposes an ID, send the initial prompt with the app conversation tool instead. After the conversation exists, never use the browser to compose or send follow-up messages, read replies, or check status.

- Once the conversation exists, use the Codex app's conversation tools for all communication and inspection: `mcp__codex_app__send_message_to_thread` for messages and `mcp__codex_app__read_thread` (or `list_threads` to locate/confirm its ID) for replies and status. If browser creation fails, report the concrete blocker rather than switching to a Codex task or ChatGPT Work cloud task.

## Recover from a stopped or stalled response

- Treat an incomplete response as unfinished, even if the conversation is no longer active. A stopped response is not a completed review.

- Let high-reasoning analysis and repository inspection run to completion whenever the app conversation tool reports that the response is active. Do not interrupt it merely because it is taking several minutes or because intermediate progress is repetitive; wait and recheck with bounded polling. Recover only for a clearly stalled response, an explicit user request, or an actual tool failure.

- Use `mcp__codex_app__read_thread` as the status and response source of truth. While the thread is active, do not send another prompt. When it becomes idle, inspect the latest exchange to determine whether the work completed or needs a continuation.

- If the work is unfinished and the thread is idle—whether the assistant response visibly stopped mid-answer or the latest accepted user message has no assistant response—Codex must send exactly one concise English `Continue`/continuation to the same conversation ID with `mcp__codex_app__send_message_to_thread`; never ask the user to press Continue or authorize this normal recovery. State the concrete remaining action; do not resend the original full prompt. Then recheck with `read_thread` and allow time for the response to run. Never send repeated continuations for the same idle period. If that continuation also gets no response or stops again, record the item as `parked` with its conversation ID, fixed source/target paths and blobs, last known branch/commit, last usable response, and next action. Keep it in the ledger and resume in the same conversation; do not start another worker for that file unless the user authorizes replacement. A parked no-response chat does not consume one of the two live execution slots, so continue a different, non-overlapping page in a fresh conversation. Do not report the whole goal blocked solely because a parked chat is unanswered while independent in-scope work remains.

- If the same conversation stops again, remains stalled, or its context is no longer reliable, keep the work item unfinished. Record the last usable response, fixed blob/path ledger, and next action; recover in that same conversation when it is idle. A separate ChatGPT chat is context-free and is not a handoff. Do not silently replace the conversation or assume another one inherited its investigation.

- If a replacement conversation is explicitly authorized and available, keep the original item unfinished until it has a terminal PASS/NO-OP or the user explicitly abandons it. The replacement prompt must repeat the full task, paths, authoritative source revision, fixed artifacts, and constraints. Verify the actual GitHub file and commit against the authoritative source; a chat message alone is not evidence that repository changes were completed.

## Conversation tools and GitHub invariants

- Use `list_threads` when needed to confirm the intended conversation ID before sending; after creation, all message and status operations must use the app conversation tools, not the browser.

- Independently poll `git ls-remote origin refs/heads/<target-branch>`, then fetch/pull and verify the commit parent, exact changed paths, and blob IDs locally.

- If the response is incomplete after it has stopped, ask in the same chat to finish only the remaining GitHub file edit or audit; do not reconstruct or substitute a local file.

- For large generated files, keep the fixed source blob SHA and size in the working ledger. If analysis/tests finished but the GitHub write or commit timed out, send a short same-chat continuation that resumes from the finalized content, refreshes the target branch, and completes the direct write/commit/push; do not restart the broad investigation. A focused test pass without a verified commit/push is incomplete.

- If the first GitHub write, commit, or push fails after the target content is ready, do not leave the work uncommitted. After `read_thread` reports the response is idle, send exactly one concrete English continuation in the same chat: identify the failed GitHub operation, ask ChatGPT to resume from the finalized content, refresh the target branch, and complete the direct GitHub write/commit/push. Do not regenerate the target content or restart the broad investigation. If that recovery also fails or GitHub editing is unavailable, stop and report the blocker for user direction.

- Prompts should include the current source revision/blob, exact target paths, the instruction to refresh the target branch immediately before committing, and the no-`git clone` constraint. Recheck the authoritative source blob immediately before generation: an earlier retrieval may be stale if the branch advanced.

- Distinguish implementation, correction, and final review. Review the actual commit against the authoritative source blob in the same conversation; if a defect is found, make the smallest correction in those target files, rerun focused checks, and re-review it. Once PASS/NO-OP and SHA evidence are terminal, mark the work item complete.

- Do not re-audit work items already marked audited or complete in the task ledger. Start the next genuinely missing item, while keeping two independent items live when possible; preserve concurrent non-overlapping commits by refreshing the target branch before every write.

## Keep at most two independent work items active

- Keep at most two ChatGPT work-item conversations with live responses or ongoing implementation/audit/correction activity, one per independent file or work item. Parked no-response conversations remain in the ledger but do not count against this cap. Never use a live slot for duplicate or overlapping work. Prefer two genuinely independent items in parallel when the account and available conversations permit it.

- Track each conversation by its ChatGPT conversation ID and use the app conversation tools to read/send. Keep at most two active-work browser tabs, one per independent item, and only as needed for unfinished work; do not rely on browser operations for messaging or status checks. Completed-tab cleanup is defined under Completion and cleanup.

- Never abandon or erase a conversation that is active, interrupted, stalled, or awaiting implementation/push/audit. For an idle unfinished conversation, send one same-chat continuation first; if still unanswered, park it rather than letting it freeze unrelated work. A new ChatGPT chat has no context and cannot inherit the old chat's reasoning or generated artifacts; keep the unfinished item in the ledger until its terminal response/SHA or explicit user abandonment.

- `parked` means deferred, not forgotten. Revisit parked items after each other work item reaches a terminal result and whenever no independent task remains to keep progressing. First inspect the same conversation and the remote target branch: a commit may have arrived without a final report. If the item is still idle and incomplete, send one concise continuation in that same chat, subject to the two-live-item limit. Do not resend within 20 minutes of the last continuation unless the user explicitly asks. After each retry, either record the terminal evidence or park it again and continue productive work; never leave the overall task waiting for the user to click Continue.

- Once PASS/NO-OP and SHA evidence are recorded, mark the item terminal and reuse its slot for the next genuinely missing item. Close that item's browser tab only after matching the tab to its conversation ID and verifying the terminal same-chat audit and exact commit/path/blob evidence. Do not leave a completed tab open between items or turns: close it immediately after verification to conserve browser memory. Closing a tab does not delete, archive, or otherwise alter the conversation; reopen it from chat history only if follow-up is needed. Keep tabs for unfinished, parked, recovery-required, or unrelated conversations.

- If conversation state becomes unavailable through the app tools, report the concrete limitation and preserve the last usable conversation ID, response, fixed blob/path ledger, and next action. Do not switch to browser messaging or silently resend the original prompt.

## Run two independent work items in parallel

- Never leave one of the two work-item slots occupied by a completed item while another independent item is available. As soon as the terminal response/SHA or NO-OP is recorded, advance the task ledger and dispatch the next genuinely missing item in that slot; do not wait for the other item to finish. An apparently quiet conversation is not proof of completion: check its status and latest response with `read_thread` first.

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

- ChatGPT conversations may not support task-wait APIs. Poll sparingly with `mcp__codex_app__read_thread`, allowing time for Sol high to finish between checks. If the app conversation tool cannot expose the required status or reply, report that limitation instead of switching to browser UI.

## Verify and close the task

- Verify the actual GitHub commit and changed paths independently. Keep any required implementation or correction in the same ChatGPT conversation and apply it directly to the designated GitHub files.

- Preserve unrelated work and respect all existing authorization boundaries.

- Run focused verification appropriate to the artifact, then broader checks only when the change affects shared integration.

- Report what ChatGPT materially influenced, what was verified locally, and what remains unverified. Do not present ChatGPT's unverified claims as completed work.

## Operational runbook learned from actual ChatGPT use

The following rules make the ChatGPT workflow reliable for long-running
tracked-file work. They supplement the repository-specific rules above. The
live conversation status, the committed GitHub tree, and exact blob/commit
evidence outrank a stale browser view or an earlier status message.

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

### Conversation creation and app-tool discipline

- Use the browser only to create/rebind the ordinary ChatGPT conversation or to
  inspect a user-visible artifact when necessary. Once the conversation exists,
  use `mcp__codex_app__send_message_to_thread` for all messages and
  `mcp__codex_app__read_thread`/`list_threads` for all replies and status, as
  required by this skill. Do not mix browser messaging into an app-tool chat.
- Confirm the intended conversation ID and current title before sending. After
  every send/read, re-evaluate the latest status; a no-change result is not
  proof that GitHub or the response is unchanged.
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

- While the app conversation reports an active response, do not send another
  message. “Thinking”, a long tool phase, or a visible connection-wait message
  is not completion by itself.
- When the response becomes idle, read the latest exchange. If it is a complete
  report with response text/actions, record it; do not send a redundant prompt.
- If a visible ChatGPT UI shows `Message delivery timed out` /
  `メッセージ配信がタイムアウトしました` with `Retry` / `再試行`, never click
  Retry. Retry restarts the answer and can discard usable work. Wait until the
  response is no longer active, then send exactly one concise continuation in
  the same conversation. Do not repeatedly retry the same idle period.
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

- If one continuation also fails or the conversation remains unreliable, park
  it with its ID, fixed source/target blobs, last commit, last usable response,
  and next action. Do not loop continuations and do not silently replace it with
  a context-free chat. Parking is temporary: recheck it after another work item
  reaches a terminal result or when the independent backlog is exhausted, first
  verifying whether a commit landed. If it remains idle and incomplete, resume
  it in the same conversation, respecting the two-live-item limit and a
  20-minute minimum between continuations. Never ask the user to send Continue
  or let unanswered chats deadlock the remaining work.

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
  is not an all-platform green build.

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
- After terminal verification, immediately close the browser tab for each
  completed work-item conversation to conserve browser memory, even if it is
  selected, unless the user explicitly asks to keep it open. Match the tab to
  its conversation ID first; do not ask for separate confirmation because this
  is routine memory cleanup within the workflow. This closes only the tab, not
  the conversation or its history. Preserve tabs for active, stalled, parked,
  recovery-required, or unrelated conversations; never close an unfinished chat
  to “hand it off,” because a new chat cannot inherit its context.
- At each cleanup or work-slot transition, also close any tabs for work items
  already recorded as terminal from earlier steps. Do not keep completed tabs
  open while other items continue; if a tab's conversation ID cannot be matched
  confidently, resolve the identity before closing it.
- If local verification is performed, preserve unrelated dirty/untracked work
  and report it separately. Never use a broad reset, force-push, or cleanup as a
  shortcut for proving delivery.
