# Generic QuickSort First: Implementation and Refactor Resume

Date: 2026-09-24
Status: Q0-Q3 complete and published; Q4 authority refactor and #34 provider experiment remain open
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

#### #34 derived-LT checkpoint

`lt-derived-lift.p` defines `LT` with only `step` and `weakenRight`, then
derives `ltLift` by ordinary indexed induction. Both the `step` case and a
recursive `weakenRight` case normalize to the corresponding expected proof
trees. This confirms that the third constructor is unnecessary for the
standalone order proposition, not that it can be removed from the frozen
QuickSort provider: partition and accessibility currently inspect its
constructor history. The parallel-provider rewrite, source/image checks and
A/B resource comparison requested by #34 remain open.
Optimized `check-acceptance` passed after adding both fixture cases.

The first parallel-provider trial removes primitive `LT.lift`, uses that
derived `ltLift` in `partitionLower`/`partitionUpper`, and removes the old
constructor branches from accessibility. This provider alone checks (56,173
steps), but its generic Sorted consumer is `unsupported` (410,636 steps).
The smaller `@partitionLower` request is also `unsupported` (59,415 steps),
while `@ltLift` itself checks. The isolated distinction is a helper call at
`ltLift m n p` versus `ltLift m (Nat.succ n) p` inside a Match branch: only
the latter fails graph construction. At `function_graph.c:1240-1245`, graph
input preparation requires each index image to be the immediately preceding
source binder. The failing image is the *left* index `m`: the computed second
argument has already caused the first application to expose a Lambda with
`m` in its ambient Context. The right index still matches its binder. This is
an open helper-graph specialization boundary, not a counterexample to derived LT or
permission to drop the indexed-input check. A trial retaining an original
callee through substitution composition did not fix it: the typed beta result
had already become a local Lambda with no `pg_occurrence.origin` edge. That
trial was withdrawn. Before the full
#34 comparison, retain the generic helper's checked graph and instantiate it
at the call's index expressions without solving those expressions backwards
or treating a specialized Lambda as a new graph authority. The parallel
fixtures remain experimental until this route passes the old graph checks.

At the initial audit, two minimized imported fixtures ran in `tests/generic_sorted.sh`:
`lt-derived-helper-graph-direct.p` checks, while the otherwise equivalent
`lt-derived-helper-graph-shifted.p` is expected `unsupported`. The latter
passes an `LT m (Nat.succ n)` proof to the ordinary `ltLift` function. The
standalone `@ltLift` graph checks. In the failing request, `helper_call`
publishes a two-argument Lambda and its checked environment; the direct case
publishes the full three-argument source. Subsequent source advancement
abstracts a specialized Lambda, and `GRAPH_INPUT` rejects its captured `m`
because that index is no longer an immediately preceding source binder.
The expected-unsupported assertion is a regression marker, not acceptance of
that limitation. Reusing the generic source graph requires retaining the
source function and its argument substitution through partial application;
alternatively, graph construction must support an open parameter Context with
checked instantiation. The current local Lambda cannot be treated as the
closed generic source by term shape alone.
Closing the entire four-binder ambient Context over that Lambda was tested and
withdrawn: the three intervening binders still leave captured `m` outside the
immediately preceding index telescope, and the minimized case remains
`unsupported`. Blind closure conversion or loosening the binder check is not
the repair; any exchange must preserve dependent Context order and proof maps.
At the first `helper_call` in the shifted fixture, the checked computation is
`PG_REINDEX` of an application whose source proof is a local `PG_APP_ELIM`;
its application head is already a Lambda. The complete three-argument
`ltLift` source is not recoverable from that local proof's construction origin.
If source reuse is chosen, retain the checked callee and substitution at the
earlier partial-application step, outside Core; do not reconstruct them from
the reduced Lambda's shape. The alternative is a graph rule for an open
parameter Context, with index instantiation checked against that Context.

#### Checked specialization of a partially applied helper

The repair reuses `capture_eliminator`, which already abstracts a checked
eliminator over its complete generic index/input telescope. When `GRAPH_INPUT`
cannot reuse the source's immediately preceding binders, it takes that same
path instead of rejecting the request. The captured ambient Context remains
fixed. `pg_prove_inductive_motive_substitution` supplies the actual index
expressions and input when publishing the graph and its witness.

This is the open-parameter-Context alternative above, not recovery of the
original named function. No source-name lookup, backwards index solving,
new evidence rule, graph authority or equality coercion is added. The generated
generic telescope still passes the existing indexed-input and parameter checks.
It does not imply that every specialized source Match can be synthesized:
`function-graph-fixed-index.p` still reports `unsupported`.

- [x] Reuse checked eliminator abstraction for a non-generic input telescope.
- [x] Change the shifted-helper regression from expected `unsupported` to
  success. Exercise both the base and recursive `ltLift` cases through actual
  graph packets and a graph eliminator, comparing their returned proof trees.
- [x] Reject a consumer claiming the unshifted result type; keep `::` a
  post-synthesis check. Resume ordinary images saved at 0 and 7,500 steps;
  compare a retained recursive witness and byte-stable inert resave.
- [x] Complete clean optimized acceptance and sanitizer verification.
- [x] Publish the verified epoch without the unrelated relocation experiment.
  `3da9d1b` was atomically pushed to Main and the rewrite branch; both remote
  tips were verified at that revision.

The parallel two-constructor provider and complete generic Sorted client now
check at 623,500 steps in the clean build. The client must bind the new
helper-result and helper-graph fields and use that result in `PartAll` and
`PartOrdered`; substituting `ltLift` textually for `LT.lift` while retaining the
old graph patterns is invalid. There is no additional theorem assumption.
Their delta is now permanent as `tests/fixtures/generic_sorted/derived-lt.patch`,
applied without fuzzy matching by `tests/derived_lt.sh` (requires `patch`).
This avoids maintaining another 554-line provider/client copy. The two earlier
untracked working copies remain outside the commit. Ordinary and retained
complete images recheck, and inert resaves are byte-stable. The #34 comparison
still needs its full output/content-preservation and wrong-evidence matrix,
partial parallel-provider images and A/B resource measurements. The frozen
provider is unchanged and #34 remains open. Do not claim a speedup from fewer
LT constructors or identify the old and new nominal declarations.

Clean verification excluded the pre-existing `evidence`/`iadt` relocation
experiment. Strict `-O2` full `check-acceptance` passed after adding the
parallel-provider gate, including 63/63 compatibility. ASan/UBSan with leak
detection passed `generic_sorted.sh` in retained mode and `derived_lt.sh`,
built with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-pie -no-pie`. Logs are `/tmp/a-program-captured-acceptance-final.log`,
`/tmp/a-program-captured-sanitize.log` and
`/tmp/a-program-captured-derived-sanitize.log`.

The frozen generic theorem still takes 619,092 Solve steps; the parallel
two-constructor version takes 623,500. These are transition counts, not timing
or memory improvements. Source/image witness comparisons cover the specialized
helper at chunk sizes 1 and 64, including its recursive branch. The new negative
test rejects a graph consumer claiming the unshifted fiber.

Per-file epoch delta, relative to `src/prototype/pointer`:

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `function_graph.c` | 7 | 4 | +3 |
| `Makefile` | 3 | 2 | +1 |
| `tests/acceptance/lt-derived-helper-graph-shifted.p` | 23 | 0 | +23 |
| `tests/acceptance/lt-derived-helper-graph-shifted-wrong.p` | 7 | 0 | +7 |
| `tests/generic_sorted.sh` | 19 | 2 | +17 |
| `tests/derived_lt.sh` | 26 | 0 | +26 |
| `tests/fixtures/generic_sorted/derived-lt.patch` | 147 | 0 | +147 |

Implementation is net +3, test/build data net +221. No new Term tag, checking
rule, accepted-state owner or persistent format is introduced. This functional
repair is not completion of A3-A5/R2-R5 or the cumulative LOC-reduction gate.
The publication follow-up adds a two-line fixture-local `.gitattributes` entry:
unified-diff context prefixes are patch data, so `derived-lt.patch` does not use
source whitespace lint. Other source/test files retain the normal checks.

#### Typed-conclusion multiplicity audit

The `length-output-proof.p` source fixture is byte-identical in clean R0
`4657cc6` and clean current `fd89842` worktrees. At `pg_program_destroy`,
`-O0 -g` builds show 2,249 Occurrences and
3,856 proofs in R0, versus 2,894 Occurrences and 4,131 proofs now. Grouping
accepted evidence by `(judgement, Context, Core, classifier)` finds 2,116
keys with 135 additional distinct Occurrence pointers in R0, versus 2,314
keys with 390 additional pointers now. This is not a direct allocation delta:
R0 kept the classifier in Evidence and could reuse one Occurrence under more
than one classifier; the current Occurrence includes its classifier.

For the completed generic QuickSort input, the current build has 434,502
Occurrences and 459,006 proofs. Of 395,984 Occurrences referenced by accepted
proofs, 12,483 are additional pointers in groups with an identical four-field
key (8,389 such groups). These are upper bounds on candidates for physical
coalescing, not counts of redundant checks or proofs. A representative length
group has two accepted `PG_PI_FORM` subjects with the same Application Core
and classifier but different typed argument Occurrences; those arguments in
turn have the same four-field key and different structural inputs. Interning
only by the four-field key would discard an accepted construction recipe and
its exact premises. The 390 length candidates occupy at most about 44 KiB of
112-byte base Occurrence records; the generic candidates at most about 1.4
MiB before variable tails and allocator overhead. Blind coalescing therefore
does not address R5's source-size gate and has an unproved semantic contract.

Next R2/R3 action: identify an actual repeated *source construction* and its
checked typed-input owner, then move only a proven duplicate recipe or walk to
that owner. Keep alternate accepted derivations and source allocation identity
distinct. Do not add a tuple-keyed accepted-evidence cache or silently select
one recipe. The contextual map and current typed-conclusion counts alone do
not establish which structural consumers can be deleted; R2-R5 remain open.

The first representative is **not** that deletion. In the clean length run,
two `PG_PI_FORM` proofs with one Core/context/classifier come from different
codomain proof inputs. One enters through ordinary derivation checking; the
other enters through Match branch typing. The latter exact
`(extended_context, codomain)` request occurs again at elimination admission:
the source-side expected-type check and the trusted rule both ask for it, and
`find_record` reuses the same proof. `request_role` already interns source jobs
by `(role, scope, syntax)`. Neither collapsing the two PI subjects nor deleting
the kernel's branch check is justified. Subsequent R2/R3 work should target a
structural reconstruction that actually publishes new persistent records, not
repeat calls that return an already accepted proof.

The remaining synchronous typed-query wrappers in `evidence.c` are likewise
not separate result authorities: inductive-instance, classifier, input and
Context-lift requests are interned by the existing typing stores. The
`schema_result_context` path in `synthesis.c` reconstructs a saved constructor
telescope through checked alpha transport; it has one production caller and
must retain its proof when the saved annotation differs. An uncommitted
renaming/relocation experiment currently has no production caller and is not
part of this checkpoint. Do not promote it merely to replace a passing path.
The next implementation slice must show a concrete duplicated persistent
record or redundant owner before deleting a reconstruction path; otherwise
R5 requires revisiting the representation/ownership target itself.

The clean `4657cc6`/`bdfdeff` debug comparison of the same
`length-output-proof.p` input locates the extra records more narrowly. R0
finishes in 10,950 Solve steps and current in 8,213. Current creation sites
include 1,101 mapped Occurrences in `action_result`, 647 Context variables,
and 184 variables from Context lifting. Current Context creation includes
184 lifts; R0 creates 119 through substitution lifting. An inspection of all
1,241 current mapped-Occurrence calls found 929 with unchanged Core/type but
a *different* destination Context, 265 with changed Core, 44 with changed Core
and type, two with changed type, and one with changed Core in the same Context.
There were no calls with identical Context, Core, classifier, annotation and
judgement. These are request counts, not a proof of indispensable allocation,
but they rule out a bulk no-op-map deletion as the next R2/R3 change. Trace
why the additional Context lifts arise before changing the map or Occurrence
representation; keep the accepted reindex premise even if a physical record
can later be shared.
At `bdfdeff`, 111 newly interned lift requests originate in
`occurrence_input_step`, 17 in structural map dependency checking, and 75
through the ordinary proof-level lift callers. These are distinct request
keys; `input_request` already shares exact `(source, index, map, child)` work.
The typed-input readback path and proof-level lift path both need a scoped
child, but their checked outputs differ. Any consolidation must prove a shared
typed-input owner with the same destination Context and accepted premises;
merging requests merely by their resulting Core would be unsound.
Proof allocation confirms the same boundary: R0/current length has 603/783
new `REINDEX`, 549/638 `CONTEXT_PROJECTION`, and 225/281 `CONTEXT_EXTEND`
proofs. Of the 783 current new `REINDEX` proofs, only 11 have the exact source
Occurrence as their result; 771 change Context and one changes the subject in
the same Context. Thus a global identity-reindex shortcut cannot account for
the growth and would erase explicit map premises from requested derivations.

For #33, the renamed wrong-IH source fixture exits `unsupported` at 1,143
steps, before elimination admission: its one recursive branch offers no
independent result from which `match_recursive_motive_step` can synthesize a
motive. This status is not a kernel rejection proof. The Acc IADT test now
constructs an explicit alternative motive at the *outer* subject index. Its
induction scope and IH are well typed, but the constructor step requires the
IH at the field index; typed Application rejects that mismatched IH. The
ordinary field-index motive and branch still check. Optimized full
`check-acceptance` and focused ASan/UBSan `iadt_test` pass. Keep #33 open:
source-level rejection and the general totality/graph adequacy bridge remain
separate obligations.

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

### Q4: Canonical projection evidence

The concrete redundant check is in `pg_prove_substitution_projection`: the
structural projection already selects typed variables from an ancestor
Context, but the proof builder eagerly materialized every variable proof and
then invoked general substitution checking over the same declarations.

The replacement keeps the existing map, occurrence and evidence stores:

- Canonical prefix projection is the two-premise form of
  `PG_CONTEXT_SUBSTITUTION`: checked source and destination Contexts. The
  structural map must establish that the source is an ancestor of the
  destination. Identity and the empty prefix are included.
- Arbitrary substitution still retains source, destination and every supplied
  image proof. Alternative derivations are not discarded, even when their
  structural maps coincide with a canonical projection.
- Map arity and image terms come from `pg_evidence_context_map`, not physical
  proof-premise positions. `pg_substitution_image_at` obtains the exact supplied
  image proof, or derives the canonical variable proof on demand using the
  destination Context premise. Contiguous proof consumers use the same image
  accessor, borrowing explicit arrays and using temporary storage otherwise.
- Core, conversion, declaration identity, Context scope and solver acceptance
  remain unchanged. No additional Core tag, mutable result cache or solver
  state is introduced. A derived variable proof is interned normally.

This is a proof representation change, not permission to accept arbitrary
maps without checking their images. Both Context premises remain explicit;
sibling Contexts and reverse projections are rejected. General and canonical
proofs share a map but remain different derivations. A 64-variable canonical
projection now creates one proof before any image is demanded, rather than
eagerly creating its variable proofs.

The nested derivation format changes from APGDRV14 to APGDRV15. Old derivation
headers are rejected instead of interpreting previously invalid two-premise
requests as projections. Enclosing source/retained formats retain their own
headers; images containing old nested derivations must be regenerated. Loading
still creates unaccepted requests for ordinary Solve, not trusted acceptance.

- [x] Convert all production image consumers to map arity and indexed images.
- [x] Preserve alternate Context/image receipts and repeated-request interning.
- [x] Test canonical/general map agreement, delayed image construction,
  invalid scopes and index bounds, and omitted-binder rejection.
- [x] Round-trip both derivations in fresh processes with one-step and bulk
  Solve; reject the previous derivation header.
- [x] Optimized full `check-acceptance` on the working tree.
- [x] Focused ASan/UBSan with leak detection: Core, derivation IO, and generic
  Sorted including retained reductions.
- [x] Repeat full acceptance with only this change, excluding the unrelated
  uncommitted telescope-renaming experiment.
- [x] Compare clean baseline/current timing and record per-file changes.
- [x] Publish this completed epoch after the final gates pass. Implementation
  `ec6a47b` was atomically pushed to Main and the rewrite branch; both remote
  tips were checked. The parent refactor remains open.

Debug source runs at baseline `fb5109a` versus this implementation show:

| Input | Proofs before/after | Core terms | Typed occurrences | Maps | Solve steps |
| --- | ---: | ---: | ---: | ---: | ---: |
| `length-output-proof.p` | 4,131 / 4,128 | 1,691 | 2,894 | 992 | 8,213 |
| Generic QuickSort Sorted with the existing provider | 459,006 / 456,949 | 593,486 | 434,502 | 40,600 | 619,092 |

The last four columns are unchanged. This removes 2,057 proofs on the generic
case, not the general typed-structure growth recorded above. It does not
complete R2-R5 or the parent's net-negative source gate. The unrelated Context
renaming experiment and derived-LT whole-provider fixtures remain outside this
publication; #32-#34 are not closed by this change.

Clean verification used a detached `fb5109a` worktree with only the staged
projection patch applied, `-std=c11 -Wall -Wextra -Werror -O2`, and
`check-acceptance`; it passed without exclusions. The working-tree optimized
suite also passed. An initial failure in `substitution_prefix_rebase` asserted
the old physical premise layout; its replacement still checks every image's
judgement and rejects loss of an in-use binder. Initial mistakes in the new IO
fixture (a reused C identifier and a sibling rather than extended Context)
were corrected before the successful whole-suite runs.

Fresh-process timings below used that clean implementation and a separate
clean `fb5109a` build, with the same inputs and flags. Seven alternating samples
per revision were measured after warm-up; the small cases used batches of 40
processes per sample. No build/test job ran concurrently. Values are medians;
the observed ranges overlap for every case, so these are regression screens,
not established speedups.

| Source input | Before | After | Before range | After range |
| --- | ---: | ---: | ---: | ---: |
| length | 7.370 ms | 7.101 ms | 6.866-7.594 | 6.817-7.622 |
| function-field graph | 11.021 ms | 10.734 ms | 10.388-11.493 | 10.300-11.123 |
| old QuickSort property | 168.930 ms | 170.956 ms | 159.605-176.445 | 153.641-175.636 |
| generic QuickSort Sorted | 1009.298 ms | 997.973 ms | 967.898-1017.002 | 976.732-1022.118 |

Per-file delta for this epoch only, excluding the unrelated dirty files and
earlier branch commits; paths are relative to `src/prototype/pointer`:

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `action.c` | 11 | 8 | +3 |
| `derivation.c` | 9 | 5 | +4 |
| `derivation_io.c` | 1 | 1 | 0 |
| `evidence.c` | 128 | 72 | +56 |
| `evidence.h` | 9 | 1 | +8 |
| `function_graph.c` | 34 | 26 | +8 |
| `iadt.c` | 5 | 5 | 0 |
| `synthesis.c` | 49 | 39 | +10 |
| **Implementation subtotal** | **246** | **157** | **+89** |
| `tests/core.c` | 22 | 6 | +16 |
| `tests/derivation_io.c` | 27 | 5 | +22 |
| `tests/synthesis.c` | 17 | 16 | +1 |
| **Test subtotal** | **66** | **27** | **+39** |

The source delta is positive: lazy proof access and representation-independent
consumers cost code. This epoch is not evidence that the overall refactor has
met its source-reduction target. Continue R2-R5 separately; do not relax the
final gate or claim that a small timing difference pays for arbitrary growth.

### Q4: Direct effect-definition dependencies

`retain_dependencies` and `pg_derivation_inputs_collect_objects` still build a
wire-root array with `pg_effect_inference_pack` merely to discover references.
Use the existing definition visitor to collect parameter/seed references and
edge masks directly into the caller's DAG. Endpoint references are already
visited with equations. Keep actual wire packing, order and format unchanged;
do not export solver approximations or create another persistent index.

- [x] Replace the two dependency-only packing paths with the shared collector.
- [x] Compare ordered object sets against the packed representation,
  including cyclic masked equations, constant-source aliases, empty workers,
  failed inputs and repeated collection before/after Solve.
- [x] Run optimized acceptance and focused sanitizer tests; verify inert image
  resave and record allocation/code deltas without claiming a general speedup.
- [x] Publish the verified epoch separately from the unaccepted relocation
  experiment. `ef9dce0` was atomically pushed to Main and the rewrite branch;
  both remote tips were verified. R2-R5 and the cumulative source-reduction
  gate remain open.

Verification used a clean detached `46019c9` worktree with only these five
implementation/test files changed. Full optimized `check-acceptance` passed,
including 63/63 compatibility and generic Sorted with retained images.
ASan/UBSan with leak detection passed Derivation IO, Source IO and generic
Sorted with retained images. The initial new test omitted initialization of
its packing scratch graph and crashed in `pack_equation`; that fixture error
was fixed and the full suite rebuilt/rerun before the successful gate. The
collector also rejects a DAG without initialized term storage.

The direct/packed object sequence is identical for empty, unsealed, partially
solved and solved workers. The masked cyclic test includes a disposable
constant-source alias. Repeating collection 128 times neither grows the
reference/object sets nor changes the worker's queue/state. Invalid inputs
are rejected without advancing Solve.

Clean debug profiling against the preceding implementation, whose pointer
sources were diff-checked against `46019c9`, records:

| Workload | Dependency-only pack calls before/after | Wire pack calls before/after | Removed aligned root-array bytes |
| --- | ---: | ---: | ---: |
| `source_io_test write` | 3 / 0 | 12 / 12 | 96 |
| retained old QuickSort property | 0 / 0 | 1 / 1 | 0 |
| inferred-index effects, save at 1,000 steps | 0 / 0 | 1 / 1 | 0 |

These bytes count only the removed `effect_pack.roots` allocations rounded
to the arena's 32-byte unit, not peak RSS. The last two CLI cases did not
export pending equation workers through this path: do not claim a QuickSort
or general compiler speedup. All three saved images are byte-identical across
the two builds. No wire version, typing rule or Core representation changes.
Logs use `/tmp/a-program-effect-*`; the successful full-suite log ends in
`clean-acceptance-final.log`.

Per-file implementation delta: `effect_inference.c` +25/-0,
`effect_inference.h` +5/-0, `derivation_io.c` +1/-6, `source_io.c` +1/-7:
**+32/-13, net +19**. Tests are `tests/derivation_io.c` +63/-0. This removes
disposable payload construction, not the parent's outstanding source growth.

### Q4: Retain substitution prefixes as proof dependencies

`substitution_build` checked only a new telescope suffix, but still projected
and copied every old image proof into every extension. The replacement retains
the accepted prefix plus the new image proofs.
The structural Context map remains the authority for typed images; this does
not add a second map, a Core tag, or a new acceptance mechanism.

- [x] Use one checked extension constructor for explicit maps, pairing and
  lifting. Preserve exact source/destination and supplied proof provenance.
- [x] Derive old image proofs on demand through the retained prefix DAG,
  without recursive C traversal or a persistent image-proof cache.
- [x] Update ordinary derivation checking and the nested wire version together;
  loaded premises remain unaccepted requests for the same Solve path.
- [x] Test dependent suffixes, alternate receipts, invalid scopes, long prefix
  chains, exact repeated-request reuse and fresh-process image continuation.
- [x] Run clean optimized/debug/sanitizer acceptance and compare proof count,
  persistent premise edges, image size and time against `4476d7b`.
- [x] Publish only if the full suite passes and measurements justify retaining
  the representation change; otherwise record why and remove the experiment.
  `1762fa0` was atomically pushed to Main and the rewrite branch; both remote
  tips were verified. Parent A3-A5/R2-R5 and #32-#34 remain open.

An explicit all-images derivation and an extension derivation may establish
the same Context map by different premises. Do not merge their evidence merely
by map identity or relax retained-premise checks to make deserialization pass.
This is a verified physical proof-sharing epoch, not completion of R2-R5 or the
cumulative source-reduction gate.

The rule is ordinary weakening followed by telescope extension: a checked
map `Delta -> Gamma`, projected into `Theta` extending `Delta`, plus checked
new images in `Theta` gives a map into the extended source telescope. Each new
image's classifier is still checked against substitution of preceding images.
Neither a structural map nor a matching erased Core supplies acceptance.

Canonical projections retain `[source, destination]`. Extensions retain
`[source, destination, prefix, new_images...]`; the prefix is itself checked
evidence. `pg_prove_substitution_extension` is the common constructor, not a
new proof tag. Explicit maps start from the empty-source projection. A no-op
extension reuses its prefix only when both exact Context receipts agree.
The derivation reader/checker retains exact premises, with nested format
APGDRV16 replacing APGDRV15; images with the old nested header require
regeneration. There is no old-format adapter or separate replay authority.

For a chain of single-field extensions, proof-premise storage is now linear
instead of quadratic. Structural maps still retain flat typed image arrays;
this does **not** make the entire substitution representation linear. Demand
for an old image traverses the prefix DAG and builds only the required checked
projections, using a temporary iterative stack. It does not cache a second
persistent image-proof array. Tests cover 128 pairings followed by 64 lifts,
exact alternative receipts and unchanged maps, repeated requests, dependent
field rejection, image continuation and fresh-process derivation checking.

Clean debug profiling of the generic theorem against `4476d7b`:

| Quantity | Before | After |
| --- | ---: | ---: |
| Accepted proofs | 456,949 | 218,967 |
| Persistent proof-premise edges | 1,359,398 | 578,678 |
| Context-projection proofs | 282,330 | 12,082 |
| Context-substitution proofs | 35,089 | 39,159 |
| Variable proofs | 42,242 | 66,980 |
| Main graph aligned used bytes | 249,990,336 | 218,070,944 |
| Core terms | 593,486 | 593,540 |
| Typed occurrences | 434,502 | 440,476 |
| Context maps | 40,600 | 44,012 |
| Source Solve steps | 619,092 | 619,094 |

The counts are not identical outside the removed proof copies: explicit and
extension derivations retain different dependencies, and typed reconstruction
may select different valid receipts. Do not claim that all typed construction
has shrunk, or that fewer proofs alone establishes a speedup. Main-graph bytes
exclude other arenas, index tables and temporary allocations; peak RSS and
wall time must be measured separately. Profiling logs/scripts are under
`/tmp/a-program-prefix-proof-*`.

Full `check-acceptance` passed on the exact staged source in a clean detached
worktree: strict O0/g, O2, and O1 ASan+UBSan with leak detection. Sanitizer
binaries were force-rebuilt after the final edit. All three include 63/63
compatibility, higher Identity, source/derivation continuation, inert resave,
ordinary/retained generic Sorted and the parallel derived-LT provider. Logs
end in `debug-verified.log`, `optimized-verified.log`, and
`sanitize-verified.log`. There are no test exclusions or sanitizer diagnostics.
The main working tree's Core/IADT tests also pass; its separate unaccepted
relocation experiment remains unstaged, with its test image accessor adapted.

Initial fixture failures exposed direct reads of the old image-premise
offsets and assertions that extension and flat introduction had the same
proof pointer. Tests now check map/image agreement **and** exact prefix
provenance instead. The ordinary checker still rejects changed/missing
premises; no acceptance check was removed to make a saved proof pass.

Generic complete source image sizes are 1,368,472 bytes before and after in
ordinary mode, and 3,219,484 -> 3,209,476 bytes with retained reductions.
On length, accepted proofs are 4,128 -> 3,832 and main-graph aligned bytes
3,081,088 -> 3,050,624; Solve stays at 8,213 steps. Neither byte measure is a
complete process-memory figure.

Per-file changes for this epoch, relative to `4476d7b`:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `derivation.c` | 2 | 1 | +1 |
| `derivation_io.c` | 1 | 1 | 0 |
| `evidence.c` | 63 | 44 | +19 |
| `evidence.h` | 7 | 1 | +6 |
| **Implementation** | **73** | **47** | **+26** |
| `tests/core.c` | 58 | 11 | +47 |
| `tests/derivation_io.c` | 3 | 3 | 0 |
| `tests/iadt.c` | 40 | 35 | +5 |
| `tests/identity.c` | 5 | 5 | 0 |
| `tests/synthesis.c` | 18 | 18 | 0 |
| **Tests** | **124** | **72** | **+52** |

Cumulative implementation/header delta from R0 `4657cc6`, excluding tests and
the unstaged experiment, is +9,608/-4,952, net **+4,656**. The parent's
net-negative source gate remains unmet. This epoch reduces persistent proof
copying, not source size or all typed-map duplication.

Timing compares clean strict-O2 executables with seven alternating-order
samples after warm-up, with no acceptance build/test running concurrently.
Each sample runs 40 fresh processes for small inputs, three for legacy
QuickSort, and one for generic QuickSort. Both QuickSort inputs use their
complete providers; all timed processes exit 0. The corrected harness uses
`perf_counter_ns` and per-child `wait4`; incomplete earlier harness attempts
(missing system `time` command, then missing legacy provider) are discarded.

| Source compile | Before median (range), ms | After median (range), ms |
| --- | ---: | ---: |
| length | 7.216 (6.813-7.376) | 7.252 (6.784-7.628) |
| function-field | 10.957 (10.602-11.647) | 11.077 (10.808-11.319) |
| legacy QuickSort property | 173.670 (169.884-178.747) | 160.742 (157.245-166.080) |
| generic QuickSort Sorted | 1017.619 (1007.682-1040.357) | 913.501 (884.567-934.839) |

Generic compile median improves about 10.2% on this machine, with peak RSS
median 311,316 -> 276,064 KiB (ranges 311,152-311,536 and 275,828-276,516).
Legacy QuickSort RSS median is 75,144 -> 70,464 KiB. Small-case ranges overlap
and their RSS is dominated by inherited harness high-water marks: no speedup
or memory conclusion is established there. These measure compilation/Solve,
not sorting execution, and do not discharge the older R0 small-case regression
gate. Verified samples: `/tmp/a-program-prefix-proof-benchmark-verified.log`.

### Q4: Share Exact Substitution Input Environments

Baseline: `c94e39c`. Core substitution requests already share exact work, but
different input terms copy identical ordered binding environments. A read-only
debug trace of the generic Sorted source records 94,347 requests, 404,784
allocated environment nodes and only 20,089 distinct `(parent, binder, value)`
nodes (19,604 environment roots). Each old environment node is 32 bytes.
These are physical input copies, not duplicate typing derivations.

- [x] Measure input duplication and add a regression that fails before sharing.
- [x] Intern immutable environment prefixes inside the existing Core
  substitution store. Keep typed Contexts/evidence separate and exact-pointer
  request keys; no WHNF/alpha interning or new checking authority.
- [x] Check simultaneous substitution, ordered shadowing, mutable caller-array
  snapshots, independent store ownership, cancellation and every-cut images.
- [x] Run full optimized/debug/sanitizer acceptance; compare measured storage,
  source/image behavior and alternating performance samples before publication.
- [x] Record per-file LOC and publish only after the gates pass. This slice
  does not close A3-A5/R2-R5 or its original cumulative reduction requirement.

Request lookup still examines the supplied binding sequence: environment
interning happens only on a new request, not on each hit. The new index owns
references to immutable input nodes, never results or an alternative proof.
Standalone and deserialized substitutions retain their existing private input
ownership; the image format describes environments, not this lookup index.

The new Core regression fails on the baseline (exit 134, distinct environment
pointers for the same binding sequence). A 64-prefix/two-term test now allocates
exactly 64 shared environment nodes. The image regression finishes a sibling
request sharing that environment before saving the other request at every
cut; loading resumes independently after destruction of the original store.

Final verification: full optimized, debug and ASan/UBSan `check-acceptance`
all exit zero, each including 63/63 compatibility. No source edit occurred
after these final builds started. The normalized result/step multisets in the
optimized export logs match the previous epoch (ignore paths and concurrent
line ordering). The dirty worktree's separate Core/IADT tests also pass; its
unaccepted telescope-relocation experiment is not part of this publication.
The generic input still takes 619,094 Solve steps with 593,540 Core terms,
440,476 typed occurrences, 218,967 proofs, 44,012 maps and 94,347 substitution
requests. Main-graph used bytes remain 218,070,944. Substitution-arena aligned
used bytes decrease from 37,105,920 to 25,438,528; the new environment index
adds 32,768 pointer buckets (262,144 bytes on this machine). Counts exclude
transient allocation and are not process RSS. An interned node occupies 48
bytes before arena alignment, rather than the old unindexed 32-byte node.

Seven alternating O2 sample pairs after warmup, fresh processes, no concurrent
build/test runs: 25 invocations per small-input sample, three for compatibility
QuickSort and one for generic Sorted. Times below are median milliseconds per
process; zero-step and retained images were created by the respective compiler.
This comparison is against the immediately preceding implementation, not R0.

| Input | Source old/new | Zero-step image old/new | Retained image old/new |
| --- | ---: | ---: | ---: |
| length | 7.435 / 7.362 | 7.492 / 7.371 | 7.297 / 7.807 |
| function-field | 11.018 / 11.250 | 11.118 / 11.392 | 11.647 / 11.391 |
| Vec append | 9.180 / 9.059 | 9.147 / 8.903 | 9.389 / 9.710 |
| compatibility QuickSort | 161.697 / 162.138 | 161.628 / 160.961 | 172.138 / 172.920 |
| generic Sorted | 922.177 / 915.394 | 890.796 / 915.604 | 923.863 / 925.547 |

Timing is mixed: do not claim a general speedup. Generic source ranges are
906.767-936.522 / 909.165-929.756 ms; zero-image ranges are
858.379-920.322 / 864.944-948.619 ms. Keep the small-input regressions and the
original R5 comparison gate visible. Generic source peak RSS medians decrease
276,140 -> 264,632 KiB; compatibility QuickSort decreases 70,448 -> 66,796 KiB.
Small-process RSS inherits the Python runner's high-water mark and does not
resolve the compiler's small allocation differences.

All five zero-step images are byte-identical across revisions. Independently
written retained images have unchanged sizes but differ in bytes (separate
baseline runs also differ). Both compilers successfully read the other one's
retained images and reproduce them byte-for-byte on a zero-step resave.
Generic retained images remain 3,209,476 bytes. No codec/version change or
trusted acceptance of saved environments was introduced.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `eval.c` | 39 | 11 | +28 |
| `eval.h` | 3 | 0 | +3 |
| `tests/core.c` | 33 | 0 | +33 |
| `tests/eval_io.c` | 5 | 0 | +5 |

Implementation/header net is **+31**, tests **+38**, documentation separate.
Cumulative implementation/header delta from R0 `4657cc6` is +9,647/-4,960,
net **+4,687**. Input storage is smaller; source code is not. This does not
satisfy the cumulative net-negative gate or finish the broader authority work.

Evidence: `/tmp/a-program-environment-{optimized,debug,sanitize}.log`,
`/tmp/a-program-environment-regression-before.log`,
`/tmp/a-program-substitution-environment-{before,after}.log`,
`/tmp/a-program-environment-storage-{before,after}.log`,
`/tmp/a-program-environment-benchmark.{py,log}` and
`/tmp/a-program-environment-image-cross.{sh,log}`.

Published implementation `37e5b610b5a399cc1af73868480daabd65e42891` atomically
to Main and `rewrite/pointer-core-hott`; both remote heads were verified.
No issue was closed. The unrelated dirty work remains outside the commit.

## Q4: One Structural Context-Extension Builder

Baseline: `d800ad8` (2026-09-24). This addresses R2's construction ownership,
not the remaining general R2-R5 completion criteria.

The structural layer's private `context_map_extend` preserved an existing
image when its destination did not change. In contrast,
`pg_prove_substitution_extension` independently rebuilt every prefix image
through `pg_occurrence_weaken`, even in the same destination. For a variable
with an explicit classifier-conversion occurrence, that reconstruction could
replace it with the bare declared variable. The retained image proof still
concluded the converted occurrence: the proof and its map then disagreed.
The new regression fails on the baseline at exactly that pointer agreement,
not at a timing threshold or an unsupported surface program.

Use one `pg_context_map_extend` for structural lifting, scoped instantiation
and checked substitution extension. It handles the source suffix and optional
destination weakening; unchanged destinations retain exact typed images.
The evidence layer supplies its suffix subjects and separately verifies their
ownership, scope, sort and dependent classifiers. No structural descriptor is
accepted merely because the builder returned it. Prefix proof DAGs and their
alternative receipts remain distinct. Ordinary weakening/explicit substitution
semantics are not globally redefined to obtain this fix.

- [x] Reproduce converted-prefix image/proof disagreement on the old code.
- [x] Remove the evidence-owned prefix reconstruction loop and use the shared
  builder; no additional index, cache or proof authority.
- [x] Check flat/extended agreement, exact image retention, invalid source and
  destination scopes, zero-suffix identity, and ordinary derivation checking.
  Structural construction alone publishes no evidence. Existing Core tests pass.
- [x] Finish full debug, optimized and ASan/UBSan acceptance, including retained
  generic QuickSort, effect equations and source compatibility.
- [x] Record behavior/performance comparison and per-file delta.
- [x] Publish this verified increment, leaving the broader refactor open.

Audit disposition: do not merge `typed_selection_step` with ordinary occurrence
input traversal solely because both follow maps. Constant Pi codomain selection
can remove a binder for which no total substitution image exists; checked
restriction is not ordinary context action. Likewise, normalized inputs need
their reduction receipts rather than the original construction's stale children.
These are required semantic distinctions, not evidence that both traversals
must retain their present implementation forever.

Implementation/header delta for this patch is +35/-31, net **+4**; tests add
29 lines, documentation counted separately. This removes a duplicate builder
and fixes a concrete inconsistency, but does not meet the cumulative reduction
gate (still net +4,691 from R0). The broader source/typed-query representation
review and original R0 performance matrix remain open.

All three clean-tree acceptance runs exit zero, each with 63/63 compatibility;
no sanitizer diagnostic. The 2,481 normalized exported result records agree
with the preceding optimized run when comparing endpoint names, equality
outcomes and chunks (paths/order excluded). Some step counts change; they are
not claimed identical. Separate working-tree Core/IADT tests also pass, without
promoting the unaccepted telescope-relocation experiment.

The same generic Sorted source on old/new debug binaries gives:

| Measurement | Before | After |
| --- | ---: | ---: |
| Solve steps | 619,094 | 619,092 |
| Core terms | 593,540 | 593,486 |
| Typed occurrences | 440,476 | 434,502 |
| Proofs | 218,967 | 216,679 |
| Context maps | 44,012 | 43,378 |
| Substitution requests | 94,347 | 94,301 |
| Main arena used bytes | 218,070,944 | 216,497,088 |
| Substitution arena used bytes | 25,438,528 | 25,426,752 |

These are GDB counters/aligned arena usage, not timings or total RSS. Evidence:
`/tmp/a-program-context-extension-{debug,opt,sanitize}.log`,
`/tmp/a-program-context-extension-counts{,-before}.log` and the working-tree
`/tmp/a-program-context-extension-working-{core,iadt}.log`.

Seven alternating O2 sample pairs after warmup, fresh processes and no concurrent
build/test run; reuse the previous epoch's benchmark protocol (25 invocations per
small-input sample, three for compatibility QuickSort, one for generic Sorted).
Median milliseconds per invocation, previous implementation versus this patch:

| Input | Source | Zero-step image | Retained image |
| --- | ---: | ---: | ---: |
| length | 7.300 / 6.951 | 7.550 / 7.311 | 7.576 / 7.283 |
| function-field | 11.294 / 11.203 | 11.458 / 11.355 | 11.468 / 11.055 |
| Vec append | 9.033 / 8.944 | 8.837 / 9.056 | 9.714 / 9.561 |
| compatibility QuickSort | 159.022 / 157.654 | 160.887 / 160.033 | 170.587 / 165.327 |
| generic Sorted | 895.142 / 886.377 | 898.351 / 888.128 | 910.771 / 889.598 |

Most medians decrease slightly, but sample ranges overlap; append's zero-image
median increases. No broad speedup claim or waiver of the original R0 gate.
Generic source peak RSS medians are 265,148 / 263,472 KiB. Small-input RSS is
dominated by inherited runner high-water marks. Log:
`/tmp/a-program-context-extension-benchmark.log`.

All five zero-step images match byte-for-byte. Retained images have unchanged
sizes; both compilers read the other's images and reproduce them byte-for-byte
in an inert resave. Independent retained saves are not canonical byte equality.
No wire version change. Log: `/tmp/a-program-context-extension-image-cross.log`.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 9 | 16 | -7 |
| `typing.c` | 19 | 15 | +4 |
| `typing.h` | 7 | 0 | +7 |
| `tests/core.c` | 29 | 0 | +29 |

Published `cb704f89af510e4697cf29262ce4d7d96b52dded` atomically to Main and
`rewrite/pointer-core-hott`; both remote heads verified. No issue was closed.
Unrelated working-tree experiments remain outside the commit.

## Q4: One Pass over Substitution Images

Baseline: `633bbcd` (2026-09-24). R2-R5 remain open.

The proposed consolidation of the three pending structural-query dispatchers
was withdrawn: it passed synthesis tests but mostly moved their different
selectors into a larger dispatcher. It did not remove reconstruction. Shared
Core builders and the distinction between provisional shape and accepted typed
data already exist; a second descriptor or another job kind is not justified.

A concrete repeated traversal remains after prefix-proof sharing:
`pg_substitution_images` calls point lookup for every image, restarting at the
same prefix root. A chain of n single-image extensions in one destination
therefore needs quadratic prefix visits to obtain its n already supplied proofs.
Use one private range traversal for both point and bulk access. Each relevant
prefix is visited once; required destination projections keep their original
order and exact proof premises. Their cost is not claimed to disappear.
Pattern-type inversion uses the same traversal to fill its mutable work array;
function-graph return packets reuse their already requested complete image view.
No persistent image cache, new proof rule, wire change or acceptance shortcut.

- [x] Preserve exact alternative receipts through unchanged destinations and
  64 nested destination lifts; check every bulk image against point lookup.
  Repeated point reads after bulk access must create no new evidence.
- [x] Check out-of-range access, including `SIZE_MAX`, and existing Core tests.
- [x] Finish full debug, optimized and ASan/UBSan acceptance on frozen sources.
- [x] Compare source, zero-step and retained-image behavior and timings.
- [x] Record final source delta and publish the verified increment.

Baseline GDB inspection of the generic Sorted provider/client found 1,415 bulk
calls, of which 1,195 materialize 9,838 images. Walking their retained chains
counts 30,754 node visits for old point-by-point access versus 4,849 for one
range pass. These are structural visit counts, not wall-clock measurements or
the total cost of projection/evidence checking. Log:
`/tmp/a-program-image-range-profile-before.log`.

The new debug run confirms exactly 4,849 bulk range-node visits (plus 25,168
point-access visits and 434 pattern-inversion visits, outside that comparison).
Generic source compilation is unchanged at 619,092 Solve steps, 593,486 Core
Terms, 434,502 typed occurrences, 216,679 proofs, 43,378 maps and 94,301 Core
substitution requests. Main/substitution arena used bytes are unchanged at
216,497,088 / 25,426,752. These retained-arena figures exclude temporary storage.
Logs: `/tmp/a-program-image-range-{profile-after,counts}.log`.

The complete debug, optimized and ASan/UBSan acceptance runs passed, including
63/63 compatibility cases, without sanitizer diagnostics. All 2,481 normalized
export outcomes match the previous epoch. One parallel-test log line is prefixed by another process's `whnf:`;
normalization extracts the export record rather than dropping that line.
Logs: `/tmp/a-program-image-range-debug.log`,
`/tmp/a-program-image-range-opt-final.log`,
`/tmp/a-program-image-range-sanitize.log`. All use strict C11 warnings; the
sanitizer build uses `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`.

Isolated O2 timing used seven alternating old/new sample pairs after warmup,
with fresh processes (25 per small-input sample, three for compatibility
QuickSort, one for generic Sorted). Median milliseconds, old/new:

| Input | Source | Zero-step image | Retained image |
| --- | ---: | ---: | ---: |
| Length | 6.810 / 7.325 | 7.420 / 7.355 | 7.540 / 7.569 |
| Function field | 10.705 / 11.456 | 11.324 / 11.392 | 11.210 / 11.537 |
| Vec append | 8.920 / 8.796 | 9.103 / 8.945 | 9.481 / 9.435 |
| Compatibility QuickSort | 157.129 / 161.356 | 161.341 / 158.317 | 166.361 / 171.259 |
| Generic Sorted | 905.028 / 898.598 | 902.226 / 891.499 | 899.702 / 910.740 |

Because the small source cases appeared slower, repeat source measurements
with **31** alternating pairs. Old/new medians: Length 7.204/7.045,
function field 11.174/11.038, Vec append 8.900/8.915, compatibility QuickSort
154.429/154.790, generic Sorted 872.469/873.821 ms. Ranges overlap and the
small-case direction reverses. No general wall-time improvement or stable
regression is established. Generic source peak RSS medians are both 263,444
KiB in the repeat; small-process RSS includes the runner's inherited high-water.
Logs: `/tmp/a-program-image-range-benchmark{,-repeat}.log`.

Both revisions read the other revision's five retained images and resave them
inertly byte-for-byte (`/tmp/a-program-image-range-cross.log`). Zero-step saves
are identical across revisions. Independently produced retained saves differ
in bytes, including two saves by the old binary alone, but retain the same
sizes across revisions. No source/derivation format version is changed.

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 33 | 26 | +7 |
| `function_graph.c` | 2 | 3 | -1 |
| `tests/core.c` | 13 | 2 | +11 |

Implementation/header net is **+6**, tests **+11**. Cumulative implementation
from R0 `4657cc6` is +9,677/-4,980, net **+4,697**; docs/tests are excluded.

The experiment lives in a clean detached worktree; unrelated context-relocation
experiments remain untouched. Cumulative source reduction is still a required,
unmet parent gate, not waived by this algorithmic improvement.

Published `b314646811cbf44c4a272c72d40aa53cb53ce24b` atomically to Main and
`rewrite/pointer-core-hott`; both remote heads verified. The actual working
tree, including the excluded experiments, also passed Core and IADT tests
(`/tmp/a-program-image-range-working-{core,iadt}.log`). No issue was closed.

### Q4: Derived LT Provider Consumers

Baseline: `7b43a43`. This continues #34's library experiment, not a kernel
replacement or completion of A3-A5/R2-R5. The frozen provider stays unchanged;
the existing exact patch produces the parallel two-constructor provider and
adapts its helper graph consumers through explicit checked fields.

- [x] Add one shared Bool-order consumer to both complete generic theorems.
  Its order has `false <= b` and `true <= true`, not an always-inhabited
  relation. Reflexivity, transitivity and comparator decisions are ordinary
  checked terms. Explicit Match motives in `bool_decide` select the dependent
  result family; the trailing `::` remains a post-check.
- [x] Verify source and image outputs for empty, singleton, ordered, reversed
  and duplicate-containing inputs. Execute the resulting Sorted proofs by
  induction to obtain their lengths; do not merely typecheck unused witnesses.
- [x] Exercise zero-step, partial and completed ordinary/retained images,
  inert resave, and resumed consumers at Solve chunks 1 and 64.
- [x] Reject unrelated output claims, wrong-comparator graph evidence and an
  invalid accessibility descent in source and resumed images.
- [x] Measure both providers with the same compiler, inputs and budgets;
  distinguish theorem construction from execution of result/proof consumers.
- [x] Run optimized full acceptance and focused debug/ASan+UBSan gates;
  publish only this verified epoch and report the remaining #34 obligations.

Exact expected lists check order and multiplicity for these concrete inputs.
They are **not** a universal permutation proof. `quick_all` preserves arbitrary
element predicates, which also does not establish multiplicity. Keep #34 open
for general content preservation, partition-change sensitivity and the final
library decision, even if all tests in this subsection pass.

Performance: GCC 14.2.0, strict C11 `-O2`, compiler at `b314646` (unchanged
through `7b43a43`). Seven alternating fresh-process pairs after one warmup;
no other agent build/test was running during measurement. Each measurement
includes source/import Solve and, except `compile`, the requested NF consumer.
Wall time uses `perf_counter`; per-child maximum RSS uses `wait4`, not the
cumulative high-water mark of the benchmark runner. Budget: 10,000,000.

| Consumer | Frozen/derived steps | Frozen/derived median ms | Frozen/derived median RSS KiB |
| --- | ---: | ---: | ---: |
| Compile | 631,361 / 635,769 | 881.460 / 907.745 | 267,396 / 275,304 |
| Empty Sorted length | 797,776 / 805,416 | 897.483 / 931.892 | 270,844 / 278,416 |
| Singleton Sorted length | 1,172,708 / 1,168,668 | 927.000 / 955.946 | 275,776 / 282,640 |
| Ordered Sorted length | 1,527,469 / 1,443,867 | 951.457 / 972.471 | 281,756 / 287,076 |
| Reversed Sorted length | 1,547,029 / 1,463,423 | 946.937 / 971.020 | 280,772 / 286,032 |
| Duplicate Sorted length | 3,853,678 / 2,548,526 | 1,222.361 / 1,073.573 | 336,672 / 307,496 |
| Duplicate packet output | 1,843,119 / 829,806 | 1,029.502 / 960.159 | 302,456 / 280,364 |
| Duplicate direct output | 1,221,256 / 771,340 | 945.263 / 945.555 | 282,356 / 278,432 |

The derived provider improves duplicate Sorted execution by 12.2% in this
end-to-end sample, but compile/small-case medians regress by about 2-4%.
Duplicate Sorted timing ranges were 1,175.509-1,229.818 versus
1,055.871-1,097.003 ms. Direct-output timing ranges overlap substantially,
despite its step reduction. Steps are not uniform-cost machine instructions.
These results do not justify silently replacing the frozen library.

At compile completion, frozen/derived Core nodes are 601,178/664,767;
typed occurrences 437,831/447,952; proofs 220,754/225,056. After duplicate
Sorted NF, Core nodes are 1,387,651/1,041,433 and proofs 220,783/225,085.
The reduction concerns runtime Core construction, not a deletion of typed
evidence or identification of the two nominal LT families. Ordinary image
sizes are 1,431,682/1,428,810 bytes; retained sizes 3,310,750/3,306,023 bytes.
No format, Core representation, typing rule or default provider changed.

Evidence files: `/tmp/a-program-derived-provider-measure/timing.jsonl`,
`/tmp/a-program-derived-provider-counts.log` and
`/tmp/a-program-derived-provider-focused.log`. Focused debug and ASan+UBSan
checks passed for both providers: retained 300,000-step images, inert resaves,
resumed duplicate Sorted proof execution at chunks 1/64, and all three source
negatives. Leak detection and sanitizer halt-on-error were enabled. Compiler
and C test sources were byte-compared against the preceding verified builds;
this test-only epoch reuses those binaries rather than rebuilding identical C.
Optimized full `check-acceptance` also passed, including 63/63 source
compatibility cases and the new ordinary/retained 0/300,000/completed matrix
(`/tmp/a-program-derived-provider-acceptance.log`). The tested files were
frozen in `/tmp/a-program-derived-provider-clean` and byte-compared against
the staged versions. Existing unpublished relocation experiments remain
excluded. Published test commit `54d48b642d9a98bcbc7521c51af7f2b554ce9359`
atomically to Main/rewrite and verified both remote tips. #34 remains open.

Per-file delta: `tests/derived_lt.sh` +65/-17; `boolean-consumer.p` +52/-0;
wrong comparator/descent/output fixtures +4/+3/+3 (no deletions). Total tests
+127/-17, net +110. Production C/H delta is zero; the parent's cumulative
implementation reduction gate remains unmet at +4,697 lines from R0.

### Q4: Cross-request Substitution Audit

Baseline: `37a130d`, 2026-09-24. No production change in this audit.
`eval.c:substitution_environment` interns immutable input environments, but
`substitution_init` creates a private `readback_context.results` for each root.
Consequently, two different roots can reconstruct the same exact subclosure.
This is duplicate computational work, not two authorities for typed evidence.

Diagnostic-only tracing in a detached worktree recorded new `reify_request`
entries whose nonempty environment is the request's original shared input.
Keys are exact `(Term pointer, environment pointer)` pairs within one process.
Semantic references, empty environments and fresh Lambda-local environments
are excluded; the figures below are **not all readback work**. A second event
at `substitution_finish` identifies entries for which a previously completed
top-level request was already available.

| Source input | Recorded entries | Unique pairs | Repeated entries | Previously completed root |
| --- | ---: | ---: | ---: | ---: |
| Length output proof | 2,332 | 1,738 | 594 | 117 |
| Function-field graph | 4,293 | 3,402 | 891 | 147 |
| Generic Sorted, frozen provider | 474,613 | 300,578 | 174,035 | 72,366 |

The generic repeated entries comprise 108,757 References, 64,508 Applications
and 770 Lambdas. The existing-root candidates comprise 71,513 References,
840 Applications and 13 Lambdas. These are entry counts, not saved transitions
or speedups; neither a global memo table nor a root-result shortcut has been
implemented or benchmarked. The diagnostic source checks all succeeded.

Two tempting implementations are not justified:

- Retain every private readback entry in a global table. On this build an
  entry is 88 bytes: even the 300,578 unique pairs in the restricted sample
  require 26,450,864 bytes before buckets, environments, other subclosures or
  root progress. The existing complete substitution arena uses 25,426,752
  bytes on this input. This is a storage lower bound, not an RSS prediction;
  deleting root bookkeeping could offset some costs, but sharing alone does
  not establish a memory improvement.
- Reuse an entire request's intrusive `pending`/`next` chain. Those links
  describe one traversal, not the dependency DAG. A second demand must not
  overwrite them or execute unrelated pending roots. Copying that chain into
  another cache merely introduces another reconstruction mechanism.

The next substitution experiment must replace ownership machinery, not add
another accepted-result authority or another typed graph:

- [x] Establish exact duplicate subclosures on small and large source inputs.
- [ ] Separate reusable structural results from a demand's traversal cursor;
  reuse the existing substitution store rather than adding a parallel cache.
  Compare retaining only already requested roots with sharing subproblems.
  Choose by measured work, retained bytes and source delta, not cache-hit count.
- [ ] Preserve ordered simultaneous substitution, shadowing, capture avoidance
  and distinct semantic-object/binder identities. Never use WHNF/alpha equality
  as a structural interning key. Keep typed occurrences and evidence separate.
- [ ] Check interleaved overlapping roots, cancellation, zero/chunked budgets,
  saved fresh binders and restore after destroying the original owner. Saving
  remains inert. Existing `shared_substitution_images` also fixes historical
  private traversal counts; distinguish those counts from necessary semantics
  before changing them, and never silently weaken result/scope checks.
- [ ] Compare source, ordinary/retained images and small-input timings against
  this baseline, with full debug/O2/sanitizer gates before publication.
  Reject a candidate that only moves work into persistence or adds a second
  solver path. The original cumulative source-reduction gate remains open.

Reproduction: strict C11 O2 build, `pointer-check --steps 10000000`, the two
named acceptance fixtures, and `--legacy-intrinsic-dot --imports
src/prototype/pointer/tests/fixtures/sorted-proof-provider.p` for
`tests/acceptance/generic-quick-sorted.p`. Logs are
`/tmp/a-program-readback-trace-{length,field,generic}.{out,log}`; the tracing-only
tree is `/tmp/a-program-authority-profile-clean`. Raw pointer traces are not
portable artifacts and are not committed. No new regression or speedup is
claimed, no issue is closed, and A3-A5/R2-R5 remain unfinished.

#### Completed-root reuse trials: withdrawn

Three executable trials at `d3a6563` tested the smaller reuse option before
changing the shared work representation. Each passed Core and evaluation-image
tests; every benchmark source check succeeded. None was adopted:

| Trial | Change | Generic compile median, baseline/trial (ms) |
| --- | --- | ---: |
| Canonical request key | Intern the input environment before lookup; replace the ordered-array hash/comparison and stored length with `(Term, environment)` | 864.007 / 883.473 |
| Completed-root lookup | Let private readback borrow a DONE root from that same request index; no second cache or retained subproblem table | 870.745 / 960.586 |
| Input-environment-only lookup | Search only the shared input environment, not fresh Lambda-local environments; keep the reuse view on the call stack | 872.196 / 903.104 |

Measurements use seven alternating O2 pairs after warmup, fresh processes,
25 repeats for length/function-field/append, three for compatibility QuickSort
and one for generic Sorted. The last trial's small-input medians are
7.079/7.067, 11.310/10.931 and 8.723/8.875 ms; compatibility QuickSort is
151.564/155.696 ms. Ranges overlap, so the small differences do not establish
a stable regression or gain. The unrestricted lookup's generic slowdown is
clear in this run; the scoped variant still does not justify its extra path.

The scoped trial lowers generic Solve steps 619,092 -> 618,589 but adds 23
implementation lines. Readback cache probes and much synchronous checking are
not individual Solve transitions: fewer reported steps are not a speedup.
Request state stays 128 bytes and request storage remains arena-aligned;
removing its length field alone does not reduce the allocated slot size.

The trial patch is retained only at
`/tmp/a-program-substitution-reuse-withdrawn.patch`; the detached worktree was
returned to its clean baseline. Timing logs are
`/tmp/a-program-substitution-{key,reuse,bounded}-timing.log`. Focused Core and
evaluation-image logs use the same prefixes. Full acceptance/sanitizer/image
consumer gates were not run for these rejected candidates. No production or
test change is retained, and no Main implementation push is warranted.

Next work must not stack this lookup onto private traversal. The unresolved
question is ownership of shared subproblems versus per-demand progress and
discardable materialization. A replacement must remove existing work/storage,
not merely add a lookup to each child. Retained-input reuse and the parent's
net-negative source/performance gates remain open.

### Q4: Context-map Representation and Projection Composition

At `53ddaa5`, the generic Sorted input retains 43,378 maps and 631,749
image slots. An exact-pointer prefix-sharing simulation needs 422,023 prefix
nodes and 17,353 empty destinations. Current aligned flat allocations total
17,456,448 bytes; even an optimistic 64-byte prefix / 32-byte empty layout
needs 27,564,768 bytes, before its erased environments and lookup overhead.
These are representation counts, not RSS or timings. Do not replace every
map array with linked prefixes solely on the assumption that sharing saves
space. The diagnostic is `/tmp/a-program-map-prefix-profile.gdb` and its
successful input log is `/tmp/a-program-map-prefix-generic.log`.

One independent redundant path is narrower: composition of two accepted
canonical prefix projections currently rebuilds individual image receipts.
Their composite is the existing projection between the endpoint Contexts.
After validating both inputs and their shared boundary, use
`pg_prove_substitution_projection` directly. This adds no map/evidence tag,
cache, acceptance rule or artifact format. General substitutions, including
explicit image derivations with the same structural map, retain their old path.
This is canonical construction reuse, not observational/definitional equality.

- [x] Add a regression that fails before the change: two nonempty projections
  must reuse the endpoint projection without adding occurrence actions/proofs.
  Check its Context premises, derivation reconstruction, reversed-boundary
  rejection and the distinct explicit-image derivation.
- [x] Apply and test the isolated implementation in
  `/tmp/a-program-map-compose-clean`, excluding the unrelated local relocation
  experiment. Core and IADT tests pass; optimized full acceptance passes.
- [x] Complete debug and ASan/UBSan full acceptance. All three configurations
  pass, including 63/63 compatibility cases and both generic Sorted providers.
  Logs: `/tmp/a-program-map-compose-{debug,opt,sanitize}-acceptance.log`.
  Sanitizer flags are `-O1 -g -fsanitize=address,undefined
  -fno-omit-frame-pointer -fno-pie -no-pie`; all builds use C11 and
  `-Wall -Wextra -Werror`.
- [x] Compare clean O2 source/image timings without competing builds; all
  source checks and both directions of old/new seed/retained image loads pass.
- [x] Transfer only the isolated patch, preserving unrelated local work.
- [x] Publish the verified patch and audit: `6868ef9` was atomically pushed to
  Main and the rewrite branch; both remote tips were verified. The staged C
  files exactly match the isolated, tested files. Unrelated local experiments
  remain outside the publication.

The source measurements limit the benefit: length/function-field/generic
inputs encounter 54/132/259 projection-pair compositions, all with zero image
slots; compatibility QuickSort encounters 179 pairs and only three image
slots. Compatibility QuickSort proofs fall 58,115 -> 58,114 and used main-arena
space falls 52,641,856 -> 52,641,728 bytes, with other recorded counts unchanged.
Generic Solve steps, Core/Occurrence/proof/map counts and measured arena usage
are unchanged. Do not present this as the main compile-time repair or
extrapolate the regression's saving to ordinary workloads. Count logs use
`/tmp/a-program-map-compose-count*.log`; storage logs use
`/tmp/a-program-map-compose-storage*.log`.

Clean O2 comparison uses seven alternating samples after warmup, 25 fresh
processes per small-input sample, three per compatibility QuickSort sample
and one per generic sample. Medians below are before/after in milliseconds;
all sample ranges overlap, so no speedup is established. The baseline source
is `d3a6563` (identical implementation to `53ddaa5`); logs and the reproducible
driver are `/tmp/a-program-map-compose-timing.log` and
`/tmp/a-program-map-compose-benchmark.py`.

| Input | Source | Zero-step image | Retained image |
| --- | ---: | ---: | ---: |
| length | 7.159 / 6.992 | 7.407 / 7.281 | 7.823 / 7.551 |
| function-field | 11.301 / 11.125 | 11.206 / 11.191 | 11.618 / 11.744 |
| append | 8.860 / 9.039 | 9.137 / 9.088 | 9.563 / 9.329 |
| compatibility QuickSort | 156.595 / 158.200 | 158.103 / 159.068 | 168.588 / 171.924 |
| generic Sorted | 893.253 / 888.303 | 874.313 / 895.659 | 910.364 / 907.394 |

Accepted patch delta: `evidence.c` +3/-0; `tests/core.c` +19/-0.
The cumulative implementation/header delta from R0 becomes +9,680/-4,980,
net +4,700. The parent's net-negative source gate, shared-subproblem ownership
audit and A3-A5/R2-R5 remain open; no Issue is closed by this change.

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
| 2026-09-24 | #34 audit | Derived two-constructor LT lift as an ordinary recursive program; both constructor cases compute. | Standalone lemma only; retain primitive provider lift pending whole-provider comparison. |
| 2026-09-24 | R2/R3 audit | Clean R0/current debug runs measured accepted typed-conclusion multiplicity on identical length input; clean current generic QuickSort was also measured. PI formation has distinct typed child recipes under the same four-field conclusion key. | Do not collapse by conclusion key; trace source construction ownership before a semantic refactor. R2-R5 remain open. |
| 2026-09-24 | R2/R3 trace | A clean length run traced the representative PI proofs to derivation checking and Match branch typing. Source validation and elimination admission repeat one exact branch-type request, but the existing proof interner reuses it. | Preserve both validation points; look for persistent reconstruction rather than adding a cache or dropping a kernel check. |
| 2026-09-24 | #33 kernel boundary | The wrong-IH source fixture stops at motive synthesis, so an explicit wrong-index motive was tested directly in the Acc IADT kernel fixture. The ill-indexed IH cannot be applied to the constructor step; full optimized acceptance and focused sanitizer pass. | Kernel negative established for this case; surface elaboration and adequacy remain open. |
| 2026-09-24 | #34 whole-provider trial | The two-constructor provider checks, but its derived lifting under `@partitionLower` reaches the graph generator's direct-binder-only index gate. The first attempted callee-origin change did not solve it and was removed. | Generalize helper graph instantiation with checked substitution before claiming a whole-provider A/B result; #34 stays open. |
| 2026-09-24 | Q4 projection evidence | `ec6a47b`: canonical Context projections retain two Context premises and derive variable proofs on demand. Clean optimized acceptance and focused sanitizer gates passed; generic proof count fell by 2,057, with unchanged Core/occurrences/Solve steps. | Published Main/rewrite; timing differences are inconclusive and R2-R5 remain open. |
| 2026-09-24 | Q4 effect dependency collection | `ef9dce0`: shared direct collection removes dependency-only wire arrays; ordered object comparisons, clean optimized acceptance and focused sanitizer tests pass. Existing Source IO test removes three small arrays; measured QuickSort path is unchanged. | Published Main/rewrite; net implementation +19, not overall refactor completion. |
| 2026-09-24 | #34 helper specialization | `3da9d1b`: reuse checked eliminator abstraction and index substitution for captured helper indices. The shifted helper's graph/witness, negative consumer and images pass; the parallel derived-LT provider now admits the complete generic Sorted proof. Clean optimized acceptance and affected ASan/UBSan pass. | Published Main/rewrite; library/performance decision and A3-A5/R2-R5 remain open. |
| 2026-09-24 | Q4 substitution proof sharing | `1762fa0`: retain checked prefix dependencies instead of eagerly projecting/copying every prior image. Full debug/O2/ASan+UBSan acceptance passed. Generic proof edges fall from 1,359,398 to 578,678; measured compile median improves about 10.2%. | Published Main/rewrite; APGDRV16 replaces APGDRV15. Net source +26, cumulative reduction gate and remaining authority work stay open. |
