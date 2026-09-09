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
