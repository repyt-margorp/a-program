# Generic QuickSort First: Implementation and Refactor Resume

Date: 2026-09-24
Status: Q0-Q3 complete and published; Q4 authority refactor remains open
Planning baseline local revision: `0446d4eef364c78b41e07b03e53179ee4f999b18`
Remote Main at review: `a72cda371109fdbf84d747456ed0aeb09af2391e`
Published Q3 Main/rewrite revision: `c2ed4a75064792975f2f6637b207c1801b848e8c`

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
- `make -C /tmp/a-program-q3-clean-tree/src/prototype/pointer BUILD=/tmp/a-program-q3-committed-build check-acceptance`: passed from a detached, clean worktree at `c2ed4a7`.
- Clean-tree ASan/UBSan with leak detection: `generic_sorted.sh` (retained mode), `retained_quicksort.sh`, and `source_io_test constructor-inputs` all passed.
- `git diff --cached --check`: passed before commit. Remote Main and rewrite heads were verified at `c2ed4a7` after an atomic fast-forward push.

#### Q3 change size

The following is the per-file delta of Q3 code commit `c2ed4a7` against its
parent, excluding later documentation status updates. Paths without `doc/`
are relative to `src/prototype/pointer/`.

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `doc/2026-09-24-GENERIC-QUICKSORT-FIRST-IMPLEMENTATION-PLAN.md` | 224 | 0 | 224 |
| `Makefile` | 10 | 2 | 8 |
| `evidence.c` | 14 | 6 | 8 |
| `syntax.c` | 21 | 3 | 18 |
| `syntax.h` | 2 | 1 | 1 |
| `syntax_io.c` | 11 | 3 | 8 |
| `synthesis.c` | 79 | 11 | 68 |
| `tests/acceptance/decision-explicit-effectful.p` | 11 | 0 | 11 |
| `tests/acceptance/decision-explicit-recomputed.p` | 13 | 0 | 13 |
| `tests/acceptance/decision-explicit-result.p` | 13 | 0 | 13 |
| `tests/acceptance/decision-explicit-wrong-arity.p` | 13 | 0 | 13 |
| `tests/acceptance/decision-explicit-wrong-comparator.p` | 13 | 0 | 13 |
| `tests/acceptance/decision-explicit-wrong-index.p` | 13 | 0 | 13 |
| `tests/acceptance/decision-explicit-wrong-scope.p` | 13 | 0 | 13 |
| `tests/acceptance/generic-quick-sorted.p` | 247 | 0 | 247 |
| `tests/fixtures/generic_sorted/box-fixed-cast.p` | 4 | 0 | 4 |
| `tests/fixtures/generic_sorted/box-fixed.p` | 5 | 0 | 5 |
| `tests/fixtures/generic_sorted/comparator-bridge-computed.p` | 100 | 0 | 100 |
| `tests/fixtures/generic_sorted/comparator-bridge-rejected.p` | 100 | 0 | 100 |
| `tests/fixtures/generic_sorted/decision-computed.p` | 12 | 0 | 12 |
| `tests/fixtures/generic_sorted/decision-graph-general.p` | 12 | 0 | 12 |
| `tests/fixtures/generic_sorted/decision-no-check.p` | 10 | 0 | 10 |
| `tests/fixtures/generic_sorted/generic-conditional.p` | 224 | 0 | 224 |
| `tests/fixtures/generic_sorted/generic-quick-original.p` | 245 | 0 | 245 |
| `tests/fixtures/generic_sorted/generic-quick-projection-outside.p` | 245 | 0 | 245 |
| `tests/generic_sorted.sh` | 101 | 0 | 101 |
| `tests/retained_quicksort.sh` | 20 | 0 | 20 |
| `tests/source_io.c` | 49 | 1 | 48 |
| `typing.c` | 11 | 0 | 11 |
| `typing.h` | 3 | 0 | 3 |

Totals: implementation +151/-26 (net +125), tests +1463/-1 (net +1462),
documentation +224/-0 (net +224). The large test delta includes the exact
audit fixtures and the complete generic proof, not only harness code.

### Q4. Publish and resume the authority refactor

- [x] Once Q0-Q3 pass, make a coherent reviewed commit, fast-forward Main and
  the active rewrite branch, and verify both remote tips. The user authorized
  Main publication at completed milestones; no push follows a passing minimal
  fixture alone. Record per-file added/deleted/net implementation, test and
  documentation lines. Explain #31's result on the Issue; close only when its
  full acceptance criteria hold. Published `c2ed4a7` to both branches; #31
  was documented and closed after the clean-tree and sanitizer gates passed.
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

#### First A3 continuation

The saved-allocation comparison introduced for Q3 exposed two older,
binder-only variants in `synthesis.c`. The induction-scope prefix used
`same_context_binders`; qualified constructor references used a hand-written
field loop. Both now use the same allocation-shape comparison as the other
retained scopes. This checks binder, judgement and index allocation, while
deliberately excluding saved declared types; the ordinary checked scope still
supplies typing. The nominal constructor owner remains selected by source
resolution. No new accepted proof or equality rule is introduced.

The regression extends `induction_scope_inputs` with a same-binder/wrong-sort
prefix, and `member_prefix_recheck` with a same-binder/wrong-sort field in a
source image. Correct-binder/different-saved-type cases still recheck and pass.
`synthesis.c` is +2/-15 (net -13); `tests/source_io.c` is +20/-7 (net +13).
The other A3-A5/R2-R5 authority and performance gates remain open.

Verification on this continuation: optimized `check-acceptance`, focused
`check-generic-retained` and `check-image-origins` passed. With `-O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer` and leak detection,
`source_io.sh`, `generic_sorted.sh` in retained mode, and
`retained_quicksort.sh` passed. No ignored failures.

#### R5 baseline checkpoint after Q3/A3

This is a measurement checkpoint, not an R2-R5 completion claim. A clean
`4657cc6` worktree and a clean `4aa8073` worktree were built with the same
`-std=c11 -Wall -Wextra -Werror -O2` flags. The three inputs below are
byte-identical at both revisions. Source and zero-step `.a` loads each used
`--steps 1000000`; the zero-step images were created by the respective
revision. The QuickSort compatibility input also used
`--legacy-intrinsic-dot` and the same `if8_fuel_free_quicksort_check.p`
import. All completed checks succeeded; zero-step saves were pending as
expected. Times are medians of fresh processes, alternated by revision.

| Input | R0/current Solve steps | R0/current source time | R0/current zero-image load time |
| --- | ---: | ---: | ---: |
| `length-output-proof.p` | 10,950 / 8,213 | 6.652 / 7.051 ms | 6.956 / 7.402 ms |
| `function-graph-function-field.p` | 12,906 / 10,948 | 9.937 / 10.654 ms | 9.845 / 11.085 ms |
| `legacy-quicksort-property.p` | 149,501 / 132,330 | 544.107 / 165.117 ms | 549.588 / 159.525 ms |

The small-input times used five alternating batches of 40 fresh invocations
per revision/mode; the QuickSort times used seven alternating fresh-process
samples. The QuickSort source peak RSS medians were 225,396 / 75,092 KiB and
the image-load medians 225,860 / 75,532 KiB. These observations establish a
large QuickSort improvement and small-input regressions, not their cause.
Solve-step reduction is not a time metric. The small cases' roughly 13 MiB
high-water marks do not resolve allocation differences. Zero-step images grew
only 24 bytes and completed images 56 bytes at the current revision for each
input; image size alone does not explain the timing.

Debug builds stopped at `pg_program_destroy` after the same successful
checks. Counts are live interned graph records, not accepted derivation
counts or an assertion that every Occurrence is redundant.

| Input | Revision | Core Terms | Typed Occurrences | Proofs |
| --- | --- | ---: | ---: | ---: |
| length | `4657cc6` | 1,829 | 2,249 | 3,856 |
| length | `4aa8073` | 1,691 | 2,894 | 4,131 |
| function-field | `4657cc6` | 4,472 | 4,524 | 7,927 |
| function-field | `4aa8073` | 4,670 | 5,662 | 8,155 |
| QuickSort | `4657cc6` | 168,628 | 435,774 | 588,033 |
| QuickSort | `4aa8073` | 131,016 | 74,584 | 88,020 |

From `4657cc6` to committed `4aa8073`, implementation/header files under
`src/prototype/pointer` (excluding tests, docs and build files) changed
`+9,338/-4,821`, net **+4,517** lines. Largest net growth is in
`evidence.c` (+1,056), `synthesis.c` (+978), `typing.c` (+921),
`source_io.c` (+373), and `function_graph.c` (+223). The R5 net-negative
gate is therefore open. The earlier named recovery walks are largely gone;
deleting a live proof rule, synchronous test-facing entry or separate Core/
typed store simply to improve this count would violate the authority and
soundness contracts.

Next R2-R4 slice: trace the additional small-input Occurrence/Proof creations
to their exact construction requests, then audit whether any are repeated
typed actions or genuinely different checked conclusions. Attribute
`evidence.c`, `synthesis.c` and `typing.c` growth to retained rule checks,
new features, and superseded construction separately. Delete only a proven
duplicate construction together with its old owner; preserve one interned
Core computation and distinct typed uses/derivations. Recheck source and
image behavior plus small and QuickSort performance after each coherent
change. If no such deletion materially addresses the +4,517 lines, reopen
the representation/ownership design as R5 requires rather than relabeling
the growth as cleanup.

The first allocation-site trace narrows that slice. A nonmutating debug
breakpoint at the unique `pg_occurrence_intern` insertion, grouped by caller,
found the following new Occurrences (not request counts):

| Insertion path | length | old QuickSort |
| --- | ---: | ---: |
| `context_variable` from `pg_occurrence_weaken` in `context_map_extend` | 327 | 32,084 |
| `context_variable` from `pg_context_map_projection` in `pg_occurrence_weaken` | 288 | 12,746 |
| `pg_occurrence_mapped` from `occurrence_action_step` | 670 | 10,655 |

For length, the current typing store also retains 992 Context maps, 269
projection lookups, 203 lift requests, 1,254 occurrence-action requests and
891 typed queries. QuickSort retains 12,541 maps, 2,771 projection lookups,
3,551 lift requests, 27,134 occurrence-action requests and 9,994 typed
queries. These categories overlap in their construction chains; summing them
would double-count work. None of these counts proves an unnecessary
certificate. In particular, `context_map_extend` currently makes every prefix
image valid in the destination Context, and kernel checks and substitution
consume those exact images.

The candidate ownership change is therefore **not** to skip a weakening or
pretend a source-Context occurrence is valid in a destination Context. Audit
a persistent Context-map extension that stores a checked prefix map plus its
new image, and derives destination images only when a consumer needs them.
That requires a simultaneous contract change for direct `map->images[]` and
`pg_context_map_bindings()` consumers in `evidence.c`, `occurrence_io.c` and
the substitution path, including proof of the extension and image replay.
Retaining a flat eager map behind a new wrapper would only add another layer.
Before implementing, measure how many prefix images are actually demanded
by those consumers; if almost all are demanded, this representation will not
improve allocation and should be rejected. Core Term interning and typed
Context identity remain distinct throughout.

That first demand check rules out a *lazy accessor alone*: 900 of 992 length
maps and 11,312 of 12,541 QuickSort maps already have a
`PG_CONTEXT_SUBSTITUTION` proof (902 and 11,520 proof records respectively;
alternate derivations account for the difference). The current rule checks
every image. A persistent extension is worth pursuing only if its proof rule
can derive validity from a checked prefix map and the new image while still
supporting arbitrary map composition and image readback. Otherwise all images
will be forced before acceptance, and the extra representation would worsen
complexity. This is a representation/proof-ownership design task, not a
permission to weaken validation or classify all 32,084 projections as waste.

The physical proof cost is measurable: length has 902 substitution proofs
holding 2,537 image-premise slots (largest map 12); old QuickSort has 11,520
such proofs holding 96,834 slots (largest map 42). Each proof also has its two
Context premises. Those slots are **not** duplicate logical conclusions: the
current flat rule records individually checked images and distinct derivations
remain valid. They do show where a checked prefix-plus-new-image rule could
remove repeated physical expansion. Any replacement must make composition,
projection, lookup and image replay derive the same images and preserve
alternative derivations; changing only the storage layout is insufficient.

A debug-build size check gives 112 bytes per typed Occurrence and 8 bytes per
premise pointer. Even attributing all 32,084 QuickSort extension Occurrences
and all 96,834 image-premise slots to this path gives roughly 4.4 MiB before
allocator overhead, versus roughly 75 MiB observed peak RSS. This is not a
complete allocation profile and does not rule out indirect costs, but it does
not yet justify introducing a second persistent map representation. Defer that
rewrite until a checked prefix-map proof rule and a measured end-to-end benefit
are both available; first trace the small-input construction ownership called
out above.

#### Context-map lookup boundary (`fd89842`, not a completed Q4 epoch)

`pg_context_map_lookup` no longer returns an address inside `map->images[]`.
It returns the selected typed image and optionally its oldest-source binder
position. `pg_substitution_image` uses that position to select the exact
supplied proof, so two distinct binders with the same image still retain
different derivations. Variable-frame and occurrence-action consumers use the
same lookup without requesting a position. No map, evidence, Core or image
representation was added; no proof rule or acceptance condition changed.
This removes one dependence on the flat array's pointer layout, but does
**not** remove the eager maps or their premise slots counted above.

#### #33 termination-boundary checkpoint

`Acc` remains an ordinary source-defined indexed family. The permanent
`acc-family-parameter.p` fixture now checks induction after renaming the
family and constructor to `AccessibleFoo`/`accessible_node`; the renamed
`acc-motive-wrong-field.p` still rejects. A distinct wrong-IH-index fixture,
`acc-renamed-wrong-ih.p`, is **unsupported**, not rejected, even with a closed
`Nat.zero` index. Its acceptance test guards against accidental acceptance but
does not establish the intended negative theorem. Keep #33 open and classify
this as a solver/elaboration gap until the exact kernel premise path is tested.

`PG_TERMINATION_INTRO` checks a suspended computation's declared `TOTAL`
classifier; it does not execute the computation. `pg_prove_total_pure_value`
additionally requires an empty effect row. General Acc induction proves
properties of an indexed relation but does not itself connect that relation
to the operational computation classifier. Replacing termination evidence
therefore requires a checked adequacy bridge, including retained-artifact
verification; removing the named rule alone would erase an obligation.

Optimized `check-acceptance` passed on the current worktree, including these
three Acc fixtures, generic Sorted, old QuickSort and retained-WHNF checks.
This is a test/audit checkpoint, not closure of #33 or Q4; an isolated kernel
forgery check and a negative IADT/erased-partial adequacy analysis remain.

Focused strict-debug Core and source IO passed on the working tree. An
independent clean detached worktree at `fd89842` passed optimized
`check-acceptance`, including generic Sorted and retained QuickSort; its
ASan/UBSan build with leak detection passed Core, source IO and retained
QuickSort. The pre-existing same-image/two-binder Core test checks both
positions and both exact `pg_substitution_image` derivations, including reverse
proof order. A sequential, fresh-process O2 timing check (60 length and 12 old
QuickSort runs per revision) measured medians of 7.048/7.095 ms and
164.747/161.399 ms at committed `4aa8073` versus the earlier worktree respectively;
this small, nonalternating check is only a regression screen, not a speedup
claim. The lookup commit changed implementation/header lines +22/-20 and
tests +14/-13. The main worktree still contains a separately uncommitted
telescope relocation experiment; none of that experiment entered `fd89842`.

## Change Log

| Date | Stage | Revision and evidence | Status |
| --- | --- | --- | --- |
| 2026-09-24 | Plan | Inspected #31-#34, PR #35 audit, current `match_candidate_step`, `pg_prove_pattern_type`, source parser, index transport, test runners and local diff. | Plan only; Q0-Q4 open. |
| 2026-09-24 | Q0-Q2 | Exact audit fixtures, explicit dependent Match motive, complete no-`partition_spec` generic Sorted proof, positive and negative motive tests. | Implemented; source proof and recompute images pass. |
| 2026-09-24 | Q3 | Optimized full acceptance passed. Retained-image rejection traced to constructor prefix identity; checked alpha adaptation proved insufficient and was removed. | Open; no Main publication. |
| 2026-09-24 | Q3 | Temporary call-site tracing identified the saved binder as image readback and the regenerated binder as evaluator readback under a nonempty substitution environment. Tracing removed; focused ordinary gate re-passed and retained gate remains red. | Open; checked materialization/allocation bridge needed. |
| 2026-09-24 | Q3 | Saved binder allocation separated from checked declared types; retained generic and old WHNF images, optimized acceptance and generic sanitizer gate pass. | Implementation complete; publication audit pending. |
| 2026-09-24 | Q4 | Clean-tree acceptance and ASan/UBSan passed at `c2ed4a7`; atomic Main/rewrite fast-forward verified; #31 closed with acceptance evidence. | Generic QuickSort milestone published; authority refactor still open. |
| 2026-09-24 | A3 | Removed two binder-only allocation checks in favor of the shared saved-allocation shape check; added same-binder/wrong-sort negatives. Optimized acceptance and focused ASan/UBSan gates passed. | First authority cleanup, not A3-A5 completion. |
| 2026-09-24 | R5 audit | Clean R0/current source and zero-image comparisons, interned graph counts and implementation LOC measured above. QuickSort improves sharply; small inputs regress and cumulative implementation grows. | R2-R5 remain open; isolate construction ownership before further deletion. |
| 2026-09-24 | #33 audit | Renamed Acc positive and wrong-field rejection made permanent; wrong-IH index remains unsupported, not rejected. Full optimized acceptance passed. | Retain termination classifier check and keep #33 open for kernel/adequacy audit. |
