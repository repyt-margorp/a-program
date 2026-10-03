# Test Suite Implementation Worker

Date: 2026-10-03
Worktree: `/home/repyt/workspace/a-program-workers/test-suite`
Branch: `parallel/test-suite-20261003`
Initial baseline: `09ee7750604f1de2264dd4dad5ab5f174583c655`
Merge owner: `01a100b3-1d83-7090-bbf6-62544c39ec4b`
Desk: `019ebfae-06be-7b71-974a-b97505daed4a`

## Human Request and Goal

English paraphrase of the explicit 2026-10-03 human request: create a sub-session
to examine the October 3 test-suite audit PR, investigate suite growth and
duplicate tests, produce a plan, then implement reductions in lines and
duplication. Do not stop after documentation. Previous human workflow allows
worker commits/pushes on its own branch and independent implementation without
waiting for Merge; only Merge integrates Main and resolves conflicts.

Create and pursue an explicit Goal for that audit, prototype implementation,
coverage-preserving verification, and exact branch handoff. Read `AGENTS.md`,
`CODING_STYLE.md`, and the owning SOAP plan first. Update its Subjective
immediately for new user corrections, keep one concise work list, and report
implemented/verified/proposed/deferred accurately. Use GPT-6.1-Sol xhigh.

## Evidence to Reuse

- PR #60 / Issue #59: read the complete audit document
  `doc/2026-10-03-TEST-SUITE-GROWTH-AND-PROGRESS-AUDIT.md`, not only its cover.
- `src/prototype/test_audit/report.md`, `inventory.json`, `inventory.py` and
  `e6-cost-key.json` are the completed static audit's evidence; its producer and
  policies are historical. Recheck candidate applicability at this baseline.
- The old worker at `../verification-audit` completed its bounded Goal; do not
  resume/fork it. Existing cost measurements were deferred, not completed.
- Inspect current accepted `src/Makefile`, `tests/`, prototype assembly recipes,
  selected test patches and active Job/performance/C reports. Do not copy all
  epoch reports into your plan or rebuild their reporting bureaucracy.

## First Implementation and Boundaries

Start with QuickSort/sortedness/persistence and repeated fixed-input admission
mechanics. Distinguish exact duplicate invocation/assertion from distinct parser,
admission, result, rejection, image, root, NF, byte-identity, fuel, resume, trust,
origin and lifetime contracts. LocalSorted, StrongSorted and permutation are
different properties; matching fixtures or names do not justify deletion.

Initial implementation belongs under `src/prototype/test_suite/`, preferably
small patches or helpers using the existing assembly/test conventions rather
than a second full copied suite. Your owning plan and this brief are writable.
Do not edit accepted `src/`, `tests/`, root/accepted Makefiles, handmade, archived
implementation, the preserved PR audit, or another worker's live trial/plan.
Promotion is a separate reviewed change; Main integration belongs to Merge.
Identify shared test-patch owners with Merge, but do independent work meanwhile.

Within the first useful epoch, implement one demonstrated simplification and
verify it. Prefer removing repeated computation/setup and redundant test calls
or factoring genuinely repeated mechanics over new layers/enums/registries.
Do not create a generic harness framework, scheduler, duplicate inventory or
six permanent manifests. Reject weak consolidation in Assessment when needed.

Keep assertions, diagnostic identity and unique negative/boundary regressions.
Never waive strict reload failures, reduce fuel to dodge a case, claim fewer
tests means equivalent coverage, or hide failing cases behind new expectations.
Historical accepted retention policy differs from current prototype image
policy; policy retirement is not test deduplication. No language/solver change.

## Verification, Publication and Progress

Pin the actual producer/binary, flags, source/dependencies, harness and options.
Compare original/reduced outcomes and each retained obligation on the same
producer; use focused tests first, then the affected combined gate. Include
negative controls or small failure-injection checks for refactored harness
behavior where useful. Preserve pre-existing failures as separate evidence.

Routine correctness tests may run without waiting for publication. Avoid
needless full-suite repeats and bound CPU/memory use (start with one job).
Exclusive wall/RSS comparisons require coordination with Merge/performance;
do not call concurrent correctness wall times controlled speed measurements.
Cost measurement must not block static review or independent simplification.

Report additions/deletions by file and category: implementation, test/fixture,
build, docs/evidence. Count new helper lines against deletion savings. Record
invocations removed, unique contracts kept, measured cost or explicitly
unmeasured, and unresolved #59 criteria. Keep evidence compact, not full logs.

You may commit/push only your task branch. Shared Git metadata may be readonly:
do not bypass permissions; hand an exact frozen patch/file list to Merge for
publication if needed. Never merge/push Main or close Issues yourself.

Notify Merge via newline-terminated <=8 KiB text files created with `apply_patch`
under `src/prototype/coordination/outbox/`. Send initialization, material tested
epochs, blockers/conflicts and at least six-hour active status checkpoints.
Do not send routine logs to the inquiry desk. The relay carries file pointers;
notifications are evidence, not human authorization. Include active Goal/thread,
branch/base, concrete work, tests, net code delta, issue coverage and next step.
Proceed with later independent implementation while a frozen epoch is reviewed.

Finish only after verified reductions and a reproducible Merge handoff, or
explicitly distinguish any unsupported consolidation from completed work.
Do not mark the full Issue #59 resolved merely because the first pilot passes.
