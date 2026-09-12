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
  longer reliable, start a new ChatGPT conversation. Set Sol to `high` again
  and resend the repository link plus the exact task, paths, authoritative
  source, and constraints. Do not paste a large file merely to recover context.
- Mark the old conversation as interrupted in your working notes and continue
  from its last usable response. Apply the normal review rules below: the
  returned text is a proposal, and local authoritative sources decide the work.

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
- Prefer asking ChatGPT to attach each generated target as its own complete,
  directly downloadable file artifact (`.hpp` and `.cpp` separately), rather
  than bundling the pair into a ZIP. Require the exact repository-relative
  path, byte count, and Git blob SHA-1 for each artifact. If direct GitHub push
  remains blocked and the individual file attachments cannot be retrieved
  reliably, use a same-chat artifact fallback: ask ChatGPT to output each
  generated target file in full, in its own fenced code block, without ellipses
  or omitted sections, with clear start/end markers. The local agent must
  reconstruct the files, verify the byte count and Git blob SHA-1, and only
  then perform the two-file commit/push; never treat a partial code block, ZIP,
  or unverified attachment as complete.
- Artifact download buttons may open a virtualized CodeMirror preview without
  creating a file in the host Downloads directory. For a source artifact in
  that preview, reconstruct it in the page by scrolling the `.cm-scroller` in
  bounded increments, collecting each `.cm-line`'s `textContent` keyed by its
  absolute vertical position, sorting by position, and joining with one `\n`.
  Use `textContent`, not `innerText`: a blank CodeMirror line can make
  `innerText` contain an extra newline. Transfer the reconstructed bytes in
  chunks through the local bridge or another exact-text path, then verify the
  reported byte count and Git blob SHA-1 before installing; never trust the
  visible viewport or a partial clipboard selection.
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
