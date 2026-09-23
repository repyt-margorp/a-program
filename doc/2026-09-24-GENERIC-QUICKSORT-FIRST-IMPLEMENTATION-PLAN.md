# Generic QuickSort First: Implementation and Refactor Resume

Date: 2026-09-24
Status: Q0-Q3 implementation, optimized acceptance and sanitizer gates complete; Q4 publication pending clean-tree audit
Current local revision: `0446d4eef364c78b41e07b03e53179ee4f999b18`
Remote Main at review: `a72cda371109fdbf84d747456ed0aeb09af2391e`

This is the active execution order. The [September 18 priority plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md)
records completed Surface and #29/#30 milestones and the detailed refactor
history. Its earlier priority order is historical. The open authority work in
the [parent plan](2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md) resumes only
after the generic QuickSort gate below.

## Issue Triage

| Issue | What the evidence establishes | Action |
| --- | --- | --- |
| [#31](https://github.com/repyt-margorp/a-program/issues/31) | Reproducible synthesis limitation: a dependent Match accepts the recomputed comparator index but fails the result index needed for a general Sorted proof. The Nat theorem still passes. This is the immediate correctness target for the intended proof interface; no kernel soundness failure has been shown. | Repair first and prove the complete generic theorem. |
| [#32](https://github.com/repyt-margorp/a-program/issues/32) | Proposal to simplify global `*f` syntax; no checked replacement for graph adequacy yet. | Keep the current witness mechanism while fixing #31. Investigate after the refactor resumes. |
| [#33](https://github.com/repyt-margorp/a-program/issues/33) | Trust-boundary audit of totality rules; a renamed Acc provider checks, but no unsoundness or replacement is established. | Audit with the resumed authority work; retain termination checks pending proof of a replacement. |
| [#34](https://github.com/repyt-margorp/a-program/issues/34) | A derived LT lifting lemma checks. The whole provider and performance comparison have not been tested. | Library experiment after #31; do not change the frozen provider during the repair. |

[PR #35](https://github.com/repyt-margorp/a-program/pull/35) contains the
documentation-only [generic Sorted audit](https://github.com/repyt-margorp/a-program/blob/docs/audit-generic-sorted-20260924/doc/2026-09-20-GENERAL-SORTED-QUICKSORT-LEAN-AUDIT.md)
with ten embedded A Program fixtures and an extraction runner. PRs #36-#38
likewise carry audit documents for #32-#34. A documentation merge is not an
implementation result and does not close an Issue. Recheck their status before
changing the GitHub records.

## Current Code and Failure

In `src/prototype/pointer/synthesis.c`, `match_candidate_type` builds a motive
from each branch classifier via `pg_prove_pattern_type`. The candidate loop
checks all branches and commits the first complete candidate. Its checks prove
that the chosen family fits the constructor images; they do not make the family
unique away from those images.

The audit's minimal `Graph A le x y b` has a constructor at index
`(a, le x a)`. A branch returning `cert x a` fits both families:

```text
M_result(y,b)     = Decision A R x y b
M_recomputed(y,b) = Decision A R x y (le x y)
```

Both specialize to `Decision A R x a (le x a)` at the constructor. The current
selection yields `M_recomputed`. A trailing `:: Decision A R x y b` is correctly
rejected: `::` checks the synthesized result afterward and must not steer
synthesis. `pg_prove_pattern_type` and the typed index-transport path already
exist; their presence alone cannot choose the desired motive.

The pinned audit reports `decision-no-check.p` done at 2,302 steps,
`decision-computed.p` done at 3,525, and `decision-graph-general.p` rejected at
11,479. The generic theorem with `partition_spec` as an extra hypothesis passes
at 598,624; `generic-quick-original.p` without it rejects at 587,424. Reproduce
these outcomes on the candidate build before attributing any later change to
#31. Use the audit's exact source bodies, complete
`tests/fixtures/sorted-proof-provider.p` imports, `--legacy-intrinsic-dot` and
a 10,000,000-step ceiling. A missing import or `unsupported` result is not the
same failure. The existing Nat-specific
`tests/acceptance/sort-quick-property.p` passes with the complete
`tests/sort_insertion.sh` assembly; preserve it.

## Repair Contract

- Core stays Lambda/Application/Reference with pointer-owned semantic objects.
  Typed Context, substitution and derivation evidence certify any new source
  operation. No graph-index or comparator equation is installed in DefEq.
- Synthesis remains independent of a later `::` assertion. All candidate
  validation goes through ordinary typing, checked substitution and Match
  branch validation. A graph witness may justify transport only within its
  checked scope.
- Prefer an explicit, synthesized eliminator motive or a source-level checked
  transport encoding. The choice belongs to the source program and is checked
  against the synthesized scrutinee family and every branch. Do not choose by
  output annotation, proof search order, or a comparator-specific heuristic.
- A deterministic candidate policy is also admissible if the same ordinary
  evidence checks both valid motives, its choice is specified independently of
  traversal order, and the negative cases below still reject. Compare that
  design with explicit motive/transport before choosing the implementation.
- Use the existing typed index-transport and Identity rules if the source
  encoding suffices. Add a general surface motive form only if a faithful
  encoding cannot express the minimal result-index bridge. The unoccupied
  `@(` branch-head shape in `syntax.c:elimination` is one candidate syntax;
  settle its exact telescope, scope and serialization contract with a parser
  and minimal proof test before extending the grammar.
- An explicit motive may denote a computation result type; it must preserve
  totality/effect information. It cannot force an effectful computation while
  checking a type or identify an open result with a recomputation.
- The complete generic theorem must construct the partition ordering
  obligation from its comparator certificate. Keeping `partition_spec` as a
  parameter is a useful intermediate test, not completion.

## Execution Stages

### Q0. Freeze and reproduce

- [x] Record local/remote revisions, current diff and toolchain flags. The
  worktree already contains an unpublished, separately tested telescope
  relocation experiment in `evidence.[ch]`, `iadt.[ch]` and Core/IADT tests.
  Keep its proof obligations separate from #31. Focused debug Core/IADT tests
  passed; full acceptance and integration have not been run for that change.
- [x] Extract the ten exact fixtures from PR #35 into
  `src/prototype/pointer/tests/` with the existing fixture conventions. Check
  each exit status and result kind; keep failing examples as expected failures
  until the implementation changes them. Preserve the frozen provider.
- [x] Reproduce the minimal three motive outcomes and the two full generic
  outcomes on the current candidate; record steps and the selected candidate's
  dependence on the graph result binder. A tracing-only diagnostic build may
  observe the choice, but must not alter it.

### Q1. Establish a checked source expression for the intended motive

- [x] Try a small source-level proof that explicitly transports the
  constructor result to `Decision A R x y b` through the existing typed
  graph/index evidence. Confirm it works for both Boolean outcomes and an open
  `b`, without a trailing `::` influencing synthesis.
- [x] If the encoding cannot supply the motive while preserving the existing
  surface contracts, specify an explicit Match motive. Its expression must
  synthesize independently in the correct index/scrutinee Context. Define how
  its binders correspond to the generated motive Context and how the selected
  constructor substitutes into it. Check all branches before accepting it.
  Add only the necessary syntax/AST and source-image codec fields; leave
  unannotated Matches on their current path.
- [x] Put the validation in the shared Match typing path in `synthesis.c`,
  using `evidence.c:pg_prove_pattern_type`, checked substitution and existing
  transport as appropriate. Keep accepted derivations in the normal evidence
  graph. Do not add a special `Decision` or QuickSort rule.

### Q2. Complete the property proof

- [x] Add `decision-explicit-result.p` as a positive test and retain the
  recomputed-index version as a separate positive test. The original
  unannotated `decision-graph-general.p` remains an expected rejection:
  a post-synthesis assertion does not select a motive.
- [x] Construct `PartOrdered` from comparator evidence and the checked
  partition graph. Then complete generic QuickSort Sorted for arbitrary
  `A`, `R` and comparator satisfying the theorem's written hypotheses, with
  **no `partition_spec` parameter**. Keep the operational `quickSort` term and
  the Nat-specific theorem unchanged.
- [x] Ensure consumers use the proved result of the existing graph/witness
  mechanism. Do not merely prove a relation about a separately constructed
  list or add the target theorem as a new assumption.

### Q3. Soundness and persistence gates

- [x] Test true/false outputs, both valid motives, contradictory result index,
  wrong comparator/certificate, out-of-scope transport, malformed explicit
  motive and effectful/partial terms. Wrong cases must reject; pending work
  must stay pending at small budgets.
- [x] Check source, zero/partial/completed `.a` images, inert resave, imports
  and chunked resume through the same Solve path. A restored typed certificate
  must be checked, not accepted from the presence of a serialized object.
  The generic proof passes from source and ordinary/retained completed images;
  zero/partial ordinary and retained images resume, and inert resaves are
  byte-stable. Invalid ordinary and retained images still reject.
- [x] Resolve the existing retained-WHNF graph-family mismatch documented in
  the old plan's A3 section. Saved allocations retain binder identity, while
  re-synthesis supplies declared types. `tests/retained_quicksort.sh` now
  passes, and `check-generic-retained` is part of ordinary acceptance. No
  nominal, DefEq or Core interning rule was weakened.
- [x] Run focused debug tests, optimized full `check-acceptance`, the affected
  ASan/UBSan checks, and the complete generic proof at the same revision.
  Record commands, flags and outcomes below. No ignored or reclassified failures.

#### Retained-image resolution and verification

The first divergent checked use was `(List A).cons`. The saved constructor-use
Context and freshly synthesized parameter Context had the same binder allocation
but different declared-type pointers: evaluator readback had created a fresh
bound Lambda under substitution. A saved Context is therefore an allocation
record, not typing authority. `pg_context_same_allocation_shape` checks its
binders, judgements and index allocations against the freshly checked prefix;
the checked scope result supplies the declared types. Field binders and nominal
constructor identity still have to match. The saved Match branch context is
paired with its saved parent, then the field and induction scopes are checked
against the reconstructed parent allocation. This does not add an equality or
an alpha-only conversion rule to the kernel. A source-image regression changes
the saved declared type while preserving its binder and checks successful
retyping; changing the binder is rejected.

Earlier trials excluding reduction receipts from origin discovery, or applying
context-alpha proof at the first mismatch, did not solve the whole image path
and were removed. Diagnostic tracing has also been removed.

Verification on the Q3 worktree (zero unexpected failures):

- `make -C src/prototype/pointer BUILD=/tmp/a-program-q3clean check-acceptance`: passed, including the generic retained and old retained-WHNF gates.
- `make -C src/prototype/pointer BUILD=/tmp/a-program-q3clean check-generic-retained`: passed. Old QuickSort steps: solved 132,563; retained 147,025; WHNF 632,065; retained-WHNF 646,630.
- `ASAN_OPTIONS=detect_leaks=1 bash src/prototype/pointer/tests/generic_sorted.sh /tmp/a-program-q3san/pointer-check 1`: passed, including partial retained resume and invalid retained rejection, with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`.
- `ASAN_OPTIONS=detect_leaks=1 bash src/prototype/pointer/tests/retained_quicksort.sh /tmp/a-program-q3san/pointer-check`: passed.
- `ASAN_OPTIONS=detect_leaks=1 /tmp/a-program-q3san/source_io_test constructor-inputs`: passed, including the binder/type-authority negative test.
- `git diff --cached --check`: passed. Clean-tree acceptance remains the publication gate.

### Q4. Publish and resume the authority refactor

- [ ] Once Q0-Q3 pass, make a coherent reviewed commit, fast-forward Main and
  the active rewrite branch, and verify both remote tips. The user authorized
  Main publication at completed milestones; no push follows a passing minimal
  fixture alone. Record per-file added/deleted/net implementation, test and
  documentation lines. Explain #31's result on the Issue; close only when its
  full acceptance criteria hold.
- [ ] Resume A3-A5 and R2-R5 in the parent authority plan. First re-audit the
  current source and remove duplicate synthesis/temporary work where one
  checked construction can serve both consumers. Keep Core computation and
  typed occurrence/proof information separate. Verify each substantial epoch
  with the old and new regressions, then push Main for that completed epoch.
- [ ] Evaluate #32, #33 and #34 in their own audit/experiment stages after the
  generic proof gate. Do not delete global `*f`, termination rules or primitive
  `LT.lift` while #31 depends on their present contracts. Retain open Issues
  until their individual evidence and acceptance criteria justify closure.
- [ ] Keep the parent's final acceptance, performance and net-negative source
  delta gates open until measured on the finished refactor. A successful #31
  milestone is not completion of that separate work.

## Change Log

| Date | Stage | Revision and evidence | Status |
| --- | --- | --- | --- |
| 2026-09-24 | Plan | Inspected #31-#34, PR #35 audit, current `match_candidate_step`, `pg_prove_pattern_type`, source parser, index transport, test runners and local diff. | Plan only; Q0-Q4 open. |
| 2026-09-24 | Q0-Q2 | Exact audit fixtures, explicit dependent Match motive, complete no-`partition_spec` generic Sorted proof, positive and negative motive tests. | Implemented; source proof and recompute images pass. |
| 2026-09-24 | Q3 | Optimized full acceptance passed. Retained-image rejection traced to constructor prefix identity; checked alpha adaptation proved insufficient and was removed. | Open; no Main publication. |
| 2026-09-24 | Q3 | Temporary call-site tracing identified the saved binder as image readback and the regenerated binder as evaluator readback under a nonempty substitution environment. Tracing removed; focused ordinary gate re-passed and retained gate remains red. | Open; checked materialization/allocation bridge needed. |
| 2026-09-24 | Q3 | Saved binder allocation separated from checked declared types; retained generic and old WHNF images, optimized acceptance and generic sanitizer gate pass. | Implementation complete; publication audit pending. |
