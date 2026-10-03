# Independent aize Audit Assignment

Date: 2026-10-03
Owner: `a-program:aize-audit`; one additional Codex session, GPT-6.1-Sol xhigh.

## Scope and Authority

The human asked Core to create one separate audit session for `../aize`, because
multi-goal development should belong there and stable operation is difficult.
This is not an A Program implementation task. Audit actual code and write only
`/home/repyt/workspace/aize/doc/2026-10-03-GOAL-BASED-MULTI-SESSION-WORKFLOW-AUDIT.md`.
Core seeded that file with the dated user requirement and a SOAP work list.
Read repository instructions first. Preserve existing dirty source/tests/docs.
Initially no implementation changes, Git commits/pushes, issue closure, live service launch,
database mutation or tool installation are authorized. Temporary isolated test
outputs are allowed; never use a user's running state/database for experiments.
Do not control A Program workers or access authentication secrets/session logs.

Superseding human clarification, 2026-10-03: publish the completed AIze audit and
finish this assignment, then prioritize A Program. Core will review and commit/push
only that document, preserving all unrelated local work; do not stage or push the
shared checkout yourself. Record this clarification immediately in the audit's
Subjective. No implementation/repair scope is added. End the bounded audit rather
than turning its recommendations into another development Goal.

## Goal and Completion

Use your own `/goal`: inspect aize's current implementation and tests, map the
observed A Program multi-goal development practices to it, and complete a concise,
code-grounded SOAP audit in the specified aize document, with prioritized
follow-up work and a hashed handoff. This is an audit Goal, not an implementation
Goal. Mark it complete only when the document and verification evidence satisfy
that scope. Do not extend into repairing identified problems.

Trace goal/task/session identity; dispatch ownership and in-flight work; who can
declare completion; notifications versus actual verification; cancellation,
pause/wait/resume; retries, stale/duplicate delivery and process restart; durable
requirements/provenance; concurrent source ownership and integration. Read the
current changes as well as HEAD, not only old documentation. Pin source findings
to HEAD plus hashes of relevant edited files. Do not infer bugs from missing
names/searches or the user's statement that stability is difficult. Prefer a
small number of substantial, reproducible findings over a long speculative list.

Use isolated existing tests where safe. Record exactly which revision/local edits
were tested and what was not tested. Reproducers may be temporary scripts in a
private directory, not implementation or permanent test changes. Keep evidence
small: code references and concise result summaries, not copied logs/programs.
Describe fit/gaps and a minimal recommended follow-up sequence without creating
another authority/cache/coordination graph. Recommendations remain agent proposals.

## Read-Only A Program Evidence

- `/home/repyt/workspace/a-program/doc/2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md`
- `/home/repyt/workspace/a-program/src/prototype/coordination/notify_core.cjs`
- `/home/repyt/workspace/a-program/src/prototype/coordination/watch_core.cjs`
- `/home/repyt/workspace/a-program/src/prototype/coordination/test_notify.cjs`

Observed practices: separate worker tmux windows/working directories and active
Goals; one Core Main integration owner; worker task-branch publication allowed;
prototype-only implementation; immutable epoch manifests/diffs; verified exact
code/test hashes; notifications are worker evidence, not human approval; Core
reviews terminal tests and retains failures; shared-source changes are jointly
tested, timing claims are separated from storage/fuel counts. User corrections
are immediately recorded in Subjective. Core waits on notification or a six-hour
timer while worker Goals continue. Active Wait interruption is verified, but
host/process restart recovery and actual six-hour elapsed wake are not verified.
Do not confuse a shell relay/tmux pane with a durable scheduler or invent evidence.

For current Codex-specific claims, consult primary official docs rather than
assuming local behavior is a general API guarantee:
https://developers.openai.com/cookbook/examples/codex/using_goals_in_codex
https://developers.openai.com/codex/app-server
Core fetched these on 2026-10-03; you must verify any details you cite. Do not
invent support for a particular goal lifecycle/provider from project names.

## Handoff

Keep SOAP sections and progress statuses in place, preserve the original dated
Subjective and label agent conclusions. Aim for a readable audit, not an unlimited
implementation log. When complete, compute the audit's SHA256 and create one
newline-terminated report of at most 8 KiB at
`/tmp/aize-goal-audit-20261003/outbox/ready.txt` using `apply_patch`. Include audit
path/hash, inspected revision/local-edit hashes, verified tests, top findings,
limits and a statement that no implementation changed. Use the prefix
`[worker-notification aize-audit/ready]`; this is evidence, not user approval.
For a material blocker use `blocker.txt` similarly, not routine status notices.
Core's relay handles notification; do not start/resume/fork another Core or change
socket permissions. Preserve the file if delivery is unavailable; Core has a
six-hour supervision fallback. Core alone reports conclusions to the human.
