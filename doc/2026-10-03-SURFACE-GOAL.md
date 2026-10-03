# Surface Binder Syntax Goal

Date: 2026-10-03
Status: prototype verification and publication finished; integrated via `7a9a672`. Accepted language policy and promotion remain separate.
Session `surface`, branch `parallel/surface-20261003`.
Baseline: `parallel/surface-20261003` at `2d747ccfec844e8afc72d408385ceb79a7c01808`
(accepted implementation from `eb0aad6`, documentation PR #58 merged).
Local edits: this plan, lane handoff and `src/prototype/surface/` only.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md), #57.

## Problem List

1. Correct Function Graph named Binder polarity and migrate `.p` consumers
   without duplicating typed semantics or changing proof acceptance.

## 1. Named Binder Migration

### Subjective (User)

2026-10-03, English paraphrase: add a task-named surface session for function/
function-graph Binder notation and `.p` changes. Use `/goal`, `6.1 Sol`, `xhigh`
and separate work directories; launch once incoming issues/PRs clarify scope.
The supplied #57 / PR #58 audit proposes `local := source`, double-brace named
sets and ordered single-brace lists. Publication alone is not approval of every
grammar decision. Preserve synthesis-first typing and post-synthesis-only `::`.

2026-10-03, user paraphrase of the active Goal and continuation: implement and
verify #57 / PR #58 in this isolated worktree; critically read all three audits.
Prototype local `:=` source and explicit brace alternatives with scope, IH,
graph and persistence rejection gates. Preserve canonical telescopes and core
checking authority; distinguish agent syntax decisions from user approval.
Use the requested `gpt-6.1-sol` / `xhigh`, branch epochs and concise SOAP progress.
Do not push Main, merge or close issues. Completion requires current evidence
for the full correction and `.p` migration, rather than a passing subset.

2026-10-03, user authorization relayed by Core: workers may commit/push their own
task branches; Main merges are reserved to Core. This supersedes older uncertainty
about branch publication permission, not the prototype boundary.

2026-10-03, explicit user clarification: each worker may commit and push its own
task branch. Only Core merges results or updates Main; do not self-merge, promote
or close issues. Publish verified, reviewable epochs with test evidence and send
Core the branch/commit, changes, failures and integration needs. Continue within
the prototype write scope. The existing sandbox boundary still applies.

### Objective (Code)

2026-10-03 Merge status reconciliation at `5683755`: follow-up task `276f4c3`
and integration `7a9a672` are ancestors of Main. Both per-file delta records and
the verified harness/evidence follow-up are published. Earlier pending-publication
notes below are historical and superseded; no new compiler gate was run here.

Fresh inspection at `2d747ccfec844e8afc72d408385ceb79a7c01808`, with only this
plan edited: PR #58 is merged at this revision; #57 is open without comments.
All three published audit documents are read. Performance claims concern a
different revision plus unavailable experimental products; no speed claim or
checker change is adopted here. The supplied syntax report's full acceptance
matrix is a proposal; the current-head review correctly separates polarity
from optional order restrictions and records missing verification.

Fresh focused verification at that revision plus `src/prototype/surface/`:
baseline `graphMain` equals `expected` at chunks 1/64 in 10,048 steps. Corrected
renaming matches those steps. Brace reversal, duplicate/unknown selection,
duplicate locals, invalid graph/IH and shadowed role uses reject. Typed
tests prove unnormalized alpha equivalence across modes/positional/hidden fields,
four canonical fields plus IH, exact syntax transport, old-version rejection
and unsolved/solved source-image reload. The nested lexical scanner, pinned
migrator, migrated reader and syntax-I/O tests pass on the fresh overlay.
Actual baseline pending/completed images reject on the candidate, and wrong
`::` assertions reject. See the [epoch handoff](2026-10-03-SURFACE-EPOCH-1-HANDOFF.md)
for durable evidence, exact files and remaining combined/sanitizer gates.

Verified milestone at `90939fc45f3b9cbe8e6409d6142eeee783822041`: Core published
the frozen 48-file focused epoch to `origin/parallel/surface-20261003`; local and
remote-tracking refs agree. The immutable final overlay's full O2 acceptance
run exits 2 solely because a shell harness still searches for single braces in
a migrated fixture. The separate one-line harness patch restores that positive
control; the complete witness isolation gate then passes on both linked/plain
compilers. All other independently scheduled acceptance gates pass, including
63/63 compatibility cases. No second full run is claimed.

Semantic/transport/history persistence and all seven checkpoint targets pass.
ASan/UBSan `check-surface`, reader and syntax I/O pass. The strict public-image
partition gate fails at `1000:1000`, `1600:1600` and `1921:0` reloads. Fresh
baseline controls reproduce all three; all 40 report rows agree in every column
except absolute image bytes (candidate +1,056). These remain actual failures,
not expected-failure passes. A fresh follow-up overlay has identical C/header
and `.p` inputs and the byte-identical tested harness. See the
[follow-up handoff](2026-10-03-SURFACE-EPOCH-2-HANDOFF.md) for evidence and commands.

Local Git staging was attempted after focused verification and failed:
`index.lock: Read-only file system` in the shared worktree Git metadata. Source
and plan edits remain writable; no accepted file changed. Epoch commits require
coordinator staging; this does not block tests. The follow-up explicitly assigns
commit/push to Core. Only a read-only remote branch search was performed; no
remote branch or commit was created by Surface. Core subsequently performed the
authorized epoch publication above; the follow-up still needs delegated Git
publication. Do not work around the Git write boundary.

#57 / PR #58 reproduce current `source := local` orientation on accepted and
overlay builds at `eb0aad6`. Parser, `mark_index_names`, `case_field_scope` and
`syntax_io` all consume that orientation. Single-brace source order is ignored;
double-brace named lists are not parsed. Existing rejection controls still pass:
this is a surface inconsistency, not an established kernel-unsoundness bug.

### Assessment

2026-10-03, Core/agent provenance clarification: use `apply_patch` for manual
edits. Own task-branch push/Main-merge policy is user authorization; delegated
publication, slot and freeze procedures are Core/agent operational decisions.
This correction supersedes their earlier attribution in Subjective.

2026-10-03, Core operational directions delivered in the thread: do not bypass
the shared-Git `index.lock` failure or self-merge. Core can publish a coherent
epoch on Surface's task branch; hand over exact files, commit message, tests and
failures, and branch after checking tabs/English comments. Continue focused
verification while Core finishes its O2 full regression and short remaining
gates. Surface's full-regression slot is next but has not been released. Keep
broad gates open at the first handoff. Agent workflow decision: provide a frozen
hashed snapshot for delegated publication and keep one active work list. Core
published that snapshot; subsequent tests used the immutable final overlay and
private outputs. The original manifest is historical evidence pinned to that
published commit; the follow-up has its own exact file list and manifest.

2026-10-03, superseding Core operational status: Core's full O2 acceptance,
artifact/checkpoint/backend and sanitizer gates are finished; the full-regression
slot was released to Surface. All scheduled gates and fresh baseline controls
are finished; Surface has notified Core that the full-regression slot is released.
Core retains short final integration checks and Git work without timing.
Core's operational slot directions specified private outputs, at most two jobs
and notification when Surface released the slot; older queued status/permission
notes were resolved. Core's handoff steering requests exact follow-up files,
hashes and evidence for task-branch publication and combined prototype review;
no duplicate full run, and public-resume failures remain unwaived. These are
Core operational decisions, not new user requirements. Core assigns the next
broad correctness slot to Performance; ask Core before new CPU-heavy work.
Core reports its current
change is confined to `evidence_function.c` family-parameter scratch-frame
ownership, with no Surface file overlap. That report does not replace final
combined prototype integration checks.

Agent assessment of the broad results: migrate the harness's fixture-specific
`sed` expression, preserving its positive/negative expectations and witness-link
controls. The remaining three public-resume failures are inherited from the
committed baseline and cross the Core checkpoint boundary; hand them to Core
without changing checking authority or suppressing the gate. No further broad
run is needed for this one-line test migration: the failed gate is rerun fully,
and the fresh assembled overlay's implementation inputs are unchanged.

2026-10-03, agent prototype decision within the requested scope: use LHS local
and RHS source in the existing item AST; add an explicit clause binding mode.
Prototype nonempty unordered `{{...}}` sets and strictly increasing canonical
field subsequences in `{...}`; identity shorthand remains. Reject mixed forms
and legacy polarity without name-availability heuristics. Version the syntax
wire payload, rejecting old payloads rather than reinterpreting aliases. Reuse
the existing complete field expansion, source-owner metadata, role association
and ordinary checking. These are experimental decisions, not approved language
policy. Verify the actual lexical shadowing policy before changing it.

Fresh source inspection found another consumer: `branch_dependencies`' nested
clause shadow scan reads RHS aliases too. It must switch to LHS alongside
`mark_index_names`. Current scope binding permits lexical shadowing; the audit's
suggestion of an existing blanket prohibition is not established. Agent decision:
preserve that policy and test that shadowing severs invalid graph/IH access;
reject duplicates within one selector set. Do not invent a language-wide ban.

Coordinator assignment: critically verify the audit, prototype consistent
`local := source` binding and migrate the active `.p` cases. Reuse the existing
complete canonical telescope and hidden fields; no second Context/graph/solver.
Prototype the documented brace alternatives with explicit agent decisions;
do not silently treat ordering/compatibility choices as user-approved policy.
Report those decisions before accepted promotion. Do not guess orientation from
name availability. Internal function witnesses remain separate; IH `*arg` remains.

### Plan

- [x] Read AGENTS.md, CODING_STYLE.md, coordination and all three PR #58 documents.
- [x] Inspect current parser, scope scan, elaboration, graph metadata and syntax I/O.
- [x] Record exact proposed grammar and migration/compatibility decisions in SOAP.
- [x] Assemble a private clean overlay using `ARTIFACT_SOURCE="$PWD/src"`.
- [x] Implement the source-layer correction in `src/prototype/surface/`; keep
  patches and migrated `.p` fixtures there, not accepted parser/tests/examples.
- [x] Coordinate parser/AST, scope scanners, `case_field_scope` and persisted
  syntax together. Do not reinterpret old saved syntax or add name heuristics.
- [x] Verify distinct renaming, duplicate/unknown/both-valid selectors, local
  shadowing, omitted dependencies, canonical telescope and `@local`/`*local` roles.
- [x] Test unordered permutations and any proposed ordered-mode reversals;
  preserve positional patterns and ordinary block/definition-brace parsing.
- [x] Verify assertion-free synthesis, wrong-proof/`::` rejection and `.p`/image
  round-trips; focused header-index and nested-clause scope gates pass.
- [x] Run full relevant combined acceptance/compatibility/persistence and
  sanitizer gates at the released slot; original harness failure corrected and
  rerun; three baseline public-resume failures remain explicitly open with Core.
- [x] Report per-file code/test/example/doc deltas and deliver verified epochs:
  focused `90939fc` and follow-up `276f4c3` published, integrated via `7a9a672`.
- Completion: #57's correction and migration gates pass under an explicit
  reviewed syntax policy; preserved canonical lowering and proof semantics
  are verified, not inferred from one closed result.

## Progress

| Date | Problem | Material result | Evidence / next step |
| --- | --- | --- | --- |
| 2026-10-03 | 1 | E1 `90939fc` and follow-up `276f4c3` published and integrated via `7a9a672`; verified harness correction, sanitizers/checkpoints; three baseline public-resume failures remain open | [Follow-up](2026-10-03-SURFACE-EPOCH-2-HANDOFF.md); prototype delivery complete; #57 policy/diagnostics and accepted adoption remain separate |

## Work Contract

- Write only `src/prototype/surface/`, this plan and lane-local documents.
  Parser/source-layer and `case_field_scope` changes are handed off as prototype
  patches; no accepted edits or modifications to shared solver_inputs patches.
- The coordinator owns Job/Evidence/query/admission/frontier and graph-generation
  semantics. Report needs crossing that boundary; do not create a private checker.
- Do not merge PRs, close issues, push Main, promote code or substitute model.
  Branch-only push is allowed after focused verification.
- Heavy regression needs the coordinator's machine slot; focused builds use
  `-j2` at most. Detach symlinks before edits; keep generated output untracked.
- Keep this SOAP work list concise and in place. Report actual tests, next action,
  cross-owner conflicts and unresolved syntax decisions before ending each turn.
