# Incremental Solver: Duplication and Ownership Audit

Date: 2026-09-18. Baseline: `3a3bf550`, plus the uncommitted typed-data
refactor and issue #29 fixes. This is an audit and next-step plan, not a claim
that the cleanup or the parent acceptance gates are complete.

Related: [active authority plan](2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md),
[issue #29 findings](2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md).

## Conclusion

The principal duplication is **reconstructing pending term/type structure from
solver recipes**, not two independent App/Lambda evaluators. Remove reconstruction
paths and repeated orchestration, not logical checking rules. Core computation
must remain separate from typed occurrences and their checked derivations.

This review follows the active `src/prototype/pointer/Makefile` dependency graph.
It does not treat the archived integer-ID implementation as a second active
authority. Static inspection is not a proof that every execution path is covered.

### Resume Checkpoint: 2026-09-18

Inspected revision: `28e1837d6a06ae111d6b5166ab0216871298ccd4`. Both remote
Main and `rewrite/pointer-core-hott` point to it. The IADT surface and issue
#29 / PR #30 gates are complete; the broad authority refactor remains open.
The original line numbers and LOC deltas below are historical, not measurements
of this revision. No implementation change is included in this checkpoint.

Target 2 is still present: `pg_synthesis_restore_declaration` retains a
`DERIVATION_INPUT_JOB`; `data_schema_step` waits for that job and obtains its
checked field Contexts through `pg_evidence_inductive_schema`. Source checking
then constructs its own field results. This is a confirmed allocation/checking
dependency, not a newly demonstrated acceptance bug.

There is an important constraint on deleting it. `pg_data_declaration` already
owns the nominal family, parameter/index/field Contexts and result images.
However, a `pg_context` contains erased `declared_type` Terms, not the complete
typed construction of each field type. `schema_result_context` currently uses
the old checked Context to transport newly synthesized fields with fresh nested
binders. The existing `family_context_scopes` test requires both acceptance of
`y` -> `z` alpha renaming and rejection of `R y x` -> `R x y`, even when that
field is absent from the constructor result index.

Before replacing this path:

- [x] Specify how independently synthesized field evidence establishes the
  exact retained declaration Context, including nested binder renaming. Reuse
  existing typed construction/action where sufficient; identify a missing edge
  before adding storage. Raw Context existence is not formation evidence.
- [x] Preserve `pg_data_schema_check`'s validation of every field and result
  image. Do not relax it to result-index equality or silently replace the
  immutable nominal declaration with freshly synthesized annotations.
- [x] Transport the existing nominal reference through the declaration graph
  codec, without a formation theorem retained solely for allocation. Keep
  separately selected proof obligations and failed producers independent.
- [x] Extend the existing source-image tests for raw declaration restoration,
  alpha-renamed versus incompatible fields, two inert resaves and chunks 1/64.
  A loaded descriptor must create no accepted evidence before Solve.

Replacing the old formation dependency with another full declaration checker
solely to recover the same allocation would not meet this cleanup's purpose.
Conversely, trusting deserialized field annotations would remove a necessary
check. Neither shortcut is adopted. Publication remains per completed epoch,
after the existing suite and affected regressions pass, not after a partial
wire-format migration or a plan-only update.

### Declaration Epoch: Implementation and Measurements

The missing-edge investigation above found a smaller solution than retaining
another typed graph: independently synthesized field evidence already supplies
formation. `pg_prove_context_alpha` transports it to the retained Context using
ordinary variable substitution, Context extension and subject reduction. Only
bound names inside types may differ; free bindings, nominal identity, judgement
and every field remain checked. No Core tag, equality reflection, accepted-state
table or unchecked schema substitution is introduced.

A zero-step `PG_REDUCTION_PREFIX` supplies alpha-renamed subject reduction
without pretending the target is normal or executing it. The existing rule
checks the source modulo alpha. Imported zero-step prefixes require identical
stored endpoints and the same typed-source check through ordinary Solve. The
archive parser validates the empty trace; it never installs a WHNF/NF result.
APGRCP3 records this extension. Source images are APGSRC62/63, retaining only
the existing nominal declaration reference through the existing graph codec.

Deleted: `allocation_origin`, `pg_synthesis_allocation_origin`,
`pg_synthesis_restore_declaration`, declaration-origin completion waits and
formation-proof collection solely for source allocation. Explicitly selected
proof roots still undergo checking, including failed independent source roots.
`pg_data_schema_check` retains its exact Context/result-image checks.

Same-build-family comparison against `28e1837` (debug `-O0 -g`), using the
unchanged `if8_fuel_free_quicksort_check.p` and `--steps 1000000`:

| Workload / quantity | Before | After |
|---|---:|---:|
| Fresh compile Solve steps | 48,735 | 48,735 |
| Ordinary source-image load Solve steps | 48,999 | 48,999 |
| Retained `--whnf main` image load steps, root 2 | 139,084 | 138,924 |
| Retained image load requests | 17,526 | 17,460 |
| Retained image load evidence | 22,986 | 22,986 |
| Retained image load Contexts / maps | 1,369 / 4,832 | 1,369 / 4,832 |
| Retained image load occurrences / Terms | 19,341 / 36,366 | 19,341 / 36,366 |
| Retained image bytes | 442,130 | 437,130 |

Each revision writes its own image from the same source; old source-image
formats are intentionally not read by the new reader. GDB stops after Solve
at `main.c:395`. Counters are not a wall-clock bound or a global speedup claim.

Target 3 measurement at the same baseline: length calls composition 378 times
for 317 exact proof pairs (532 image visits, 57 repeat visits); the universal
QuickSort proof calls it 11,204 times for 8,504 pairs (54,160 image visits,
6,581 repeats). This confirms repeated assembly, not that it dominates runtime.
No new proof cache was added: map identity alone cannot select an alternative
requested derivation, and a second proof-pair index needs measured benefit.

Focused debug Core/source-image and ASan/UBSan Core, reduction-image and full
source-image tests pass. New coverage checks raw alpha-renamed Contexts,
family telescopes, wrong/free bindings, divergent zero-step Terms, forged
endpoints, explicit theorem roots and rejected declarations through two inert
resaves. The full optimized run initially stopped at an obsolete seed version
assertion. That test also restored version 46 before its negative checks;
it now restores the actual current version so truncation/policy tests exercise
their intended boundary. The final optimized `check-acceptance` passes, including
the universal QuickSort proof and invalid-claim regressions. The corrected seed
suite also passes under ASan/UBSan. Publication is recorded in the priority plan.

Verification logs under `/tmp/` (local execution evidence, not repository inputs):

- `a-program-authority-declaration-full-final.log`: complete optimized acceptance,
  `-std=c11 -Wall -Wextra -Werror -O2`, build `a-program-authority-declaration-opt`.
- `a-program-authority-declaration-core.log` and
  `a-program-authority-declaration-source.log`: affected debug suites, `-O0 -g`.
- `a-program-authority-declaration-asan-{core,identity,source,seed}.log`: affected
  sanitizer suites, `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`,
  non-PIE, leak detection and halt-on-error enabled. These are not a full
  sanitizer acceptance run.

Per-file source delta from `28e1837`, paths under `src/prototype/pointer/`:

| File | Added | Removed | Net |
|---|---:|---:|---:|
| `eval.c` | 7 | 0 | 7 |
| `eval.h` | 4 | 0 | 4 |
| `evidence.c` | 66 | 0 | 66 |
| `evidence.h` | 6 | 0 | 6 |
| `reduction_io.c` | 3 | 2 | 1 |
| `source_io.c` | 33 | 11 | 22 |
| `source_io.h` | 7 | 5 | 2 |
| `synthesis.c` | 11 | 48 | -37 |
| `synthesis.h` | 0 | 6 | -6 |
| `tests/core.c` | 52 | 0 | 52 |
| `tests/identity_io.c` | 51 | 0 | 51 |
| `tests/seed.c` | 6 | 5 | 1 |
| `tests/source_io.c` | 26 | 4 | 22 |

Implementation/headers: **+137/-72, net +65**. Tests: **+135/-9, net +126**.
Build files are unchanged; documentation is excluded. Removing the allocation
proof dependency reduced synthesis code but required a reusable checked Context
adaptation. This epoch does not satisfy the parent's net-negative gate. Target 1
and the remainder of targets 3-5 still require investigation and implementation.

## What Was Checked

### Body Adaptation: One Prepared Rule (after `cee33c3`)

`BODY_JOB` previously performed value-to-computation adaptation in three places:
an acceptance helper and independent term/classifier projection branches. It now
prepares one ordinary rule chain: VALUE_FROM_TYPE if needed, then TOTAL RETURN
for a value; a computation is passed through. Both structural queries follow
this same chain. Acceptance still checks the original input and optional Context.
The obsolete `computation`, `body_rule` and the two projection branches are
removed. No new Core tag, job role, stored acceptance flag or scheduler is added.

Preparation wakes the existing preparation subscribers without waiting for
effect closure. Thus a pending body can contribute its structure to the same
effect equation that acceptance awaits. A known polarity selects a checking
rule; it is not permission to accept its premises. Failed inputs and mismatched
Contexts remain failures even if their structural query completed earlier.

The extended `pending_effect_contexts` test verifies that an explicit RETURN
request finds the already prepared request without allocating another job,
then shares the same final proof. It also checks unchanged raw computations,
pre-closure term/type snapshots, and wrong-scope rejection at chunks 1/64.
The first full run found a real regression in the concrete Acc fixture:
unprepared source application syntax was classified as a computation before
resolution chose TYPE_FAMILY_APP. Body preparation now waits for the existing
source preparation boundary before choosing its ordinary rule. No expected
type or fixture-specific exception participates. A small family-application
lambda regression complements the original compatibility test. Its initial
test fixture used a definition block in expression position, then omitted the
explicit quotation required by this unit suite; these test authoring errors
are separate from the implementation regression. After correcting both, the
full optimized `check-acceptance` passes, including all 63 compatibility cases,
resaved/imported sort proofs and invalid-claim rejection. The final synthesis
suite passes in debug and ASan/UBSan builds (leak detection and halt-on-error).
This is affected sanitizer coverage, not the parent's full sanitizer gate.

Final local logs: `/tmp/a-program-authority-body-acceptance-final.log`,
`/tmp/a-program-authority-body-synthesis-final.log`, and
`/tmp/a-program-authority-body-asan-final.log`. Compiler flags are respectively
`-O2`, `-O0 -g`, and `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`, all with C11 and warnings as errors.

Same-input `-O0 -g` counts at `main.c:395`, before/after:

| Input | Solve steps | Requests | Proofs | Occurrences | Terms |
|---|---:|---:|---:|---:|---:|
| `examples/06_pred.p` | 752 / 766 | 282 / 285 | 224 / 224 | 157 / 157 | 120 / 120 |
| `length-output-proof.p` | 9,599 / 9,813 | 3,297 / 3,308 | 4,941 / 4,941 | 3,574 / 3,574 | 2,031 / 2,031 |
| Original IF8 QuickSort | 48,155 / 48,875 | 14,978 / 15,017 | 23,315 / 23,315 | 19,522 / 19,522 | 11,958 / 11,958 |

This is consolidation, not a speedup: ordinary rule scheduling adds small
amounts of work while eliminating duplicated adaptation. Do not restore the
separate acceptance implementation just to hide that cost. `synthesis.c` is
+46/-60 (net -14); `tests/synthesis.c` is +29/-0; documentation is separate.
QuickSort adds 39 requests and 720 outer Solve steps (about 1.5%), without
additional proofs, typed occurrences or Core Terms. Preparation currently
follows the producer's changing prerequisite; replacing unnecessary polling
with the existing preparation subscription requires its own scheduling tests.
Pending construction outside body adaptation and audit targets 3-5 remain open.

### Accepted Structural Projections (after `83249f1`)

Inspection found a concrete duplicate in target 1: all three structural queries
could follow source/derivation recipes even when their producer already retained
an accepted typed occurrence. They now share `accepted_structure`, reading its
Core or classifier directly. The three late-result extraction copies are removed.
No new job role, acceptance state, cache, Core tag or proof rule is introduced.
This removes completed-producer reconstruction; pending symbolic reconstruction
and the remaining target-1 work are not declared finished.

The old API comment promised symbolic rows even for a first query after
acceptance. That is not required by the consumers: contribution registration
already accepts closed rows, and formation still waits for the equation owner.
The revised contract explicitly freezes completed snapshots but lets later
queries read accepted data. Symbolic and closed snapshots agree after ordinary
substitution of solved parameters, not necessarily by pointer identity. This
changes an internal representation contract, not surface typing or execution.
Do not mutate old snapshots or register a closed row as independent acceptance.

Permanent tests cover Universe, Pi, Lambda, Thunk, application and a sequential
block. Three completed-producer queries take three steps/requests and allocate
no further Terms, occurrences, proofs or normalization jobs. Value/computation
terms are still rejected as type formations. The pending-effect suite checks a
cached symbolic Pi and a fresh accepted projection after closure, including
substitution agreement and unchanged snapshots at chunks 1/64. Existing rejected
producer tests remain unchanged; structural availability is not acceptance.

Same-input debug QuickSort compilation (`if8_fuel_free_quicksort_check.p`,
`--steps 1000000 --legacy-intrinsic-dot`, GDB at `main.c:395`):

| Quantity | Before (`83249f1`) | After |
|---|---:|---:|
| Solve steps | 48,735 | 48,155 |
| Requests | 15,203 | 14,978 |
| Accepted proofs | 23,315 | 23,315 |
| Typed occurrences | 19,522 | 19,522 |
| Core Terms | 11,990 | 11,958 |

Debug synthesis and ASan/UBSan synthesis pass, including the new tests; logs:
`/tmp/a-program-authority-structure-publish-debug.log` and
`/tmp/a-program-authority-structure-publish-asan.log`. Full optimized
`check-acceptance` also passes (`/tmp/a-program-authority-structure-publish.log`,
`-std=c11 -Wall -Wextra -Werror -O2`), including source-image and universal
QuickSort positive/negative checks. Sanitizers cover synthesis, not the whole
acceptance suite. Publication is recorded in the priority plan. No wall-clock
speedup is claimed.

Per-file delta: `synthesis.c` +24/-14; `synthesis.h` +6/-2;
`tests/synthesis.c` +58/-0. Implementation/headers total +30/-16 (net +14),
tests net +58, documentation separate. The parent's net-negative gate remains
unmet. Next inspect pending subject construction and declared-type lookup,
preserving effect-equation closure rather than waiting for full acceptance.

| Area | Files / main paths | Finding |
|---|---|---|
| Core and evaluation | `graph`, `eval`, `conversion`, `computation`, `execution` | Only Lambda/Application/Reference Core tags. Pure normalization shares work; actual external effects are not memoized results. |
| Typed structure | `typing`, `evidence`, `derivation` | Occurrence, context and map interning already exists. Checking and describing a structure are different responsibilities. |
| Incremental synthesis | `synthesis`, `effect_inference`, `program` | Pending structural projections duplicate construction logic; accepted classifier queries already use retained typed data. |
| IADT and graph properties | `iadt`, `function_graph`, source Match/graph paths | Graph formation and witness generation share one graph state. Source call-layout discovery still walks syntax separately. |
| Identity | `action`, `identity`, `symmetry`, typed boundary access | Core actions compute terms; checked family actions establish their types. Similar inputs alone do not make these duplicate acceptance authorities. |
| Persistence | source, occurrence, derivation, context/declaration, retained and reduction I/O | Match allocations are now descriptive; declaration allocation still depends on a saved derivation producer. Separate codecs are not automatically separate semantics. |
| Regression coverage | acceptance fixtures, `program_test`, source/image runners | Both accepted and rejected resumed inputs matter. A passing suite formerly included an expected graph-collision failure. |

## Confirmed Cleanup Targets

### 1. Pending structural reconstruction

`synthesis.c:7147` (`prepared_source_rule`) and the `term_structure_step`,
`declared_type_step`, `classifier_structure_step`, and `type_structure_step`
paths inspect job roles, source stages and derivation rules to rebuild term and
classifier spines. `evidence.c` later constructs the checked versions. Hash
interning can share the resulting Core nodes, but does not remove the duplicate
dispatch and preparation logic.

The reason for early structure is real: handler effect equations need the shape
of a computation before its acceptance is complete. Replacing these paths with
"wait until the producer is DONE" can introduce cyclic waits.

Required direction: let source elaboration retain its descriptive construction
once. Derive structural queries from that data and share pure spine-building
functions with checked rules. Start with Return/Thunk/Force/Pi/App, remove their
job-role reconstruction branches as each replacement lands. Do not create a
parallel trusted classifier table or treat the description as a proof.

`::` remains a post-check. In particular, the current structural projection of
an EXPECT job reads its target (`classifier_structure_step`); migration must
trace its consumers and preserve the non-feedback tests, not accidentally turn
this descriptive boundary into an inference oracle. No unsound acceptance from
this path has been demonstrated by this audit.

### 2. Declaration allocation through saved evidence (completed this epoch)

The following describes the pre-epoch dependency; its replacement and tests are
recorded above. It is no longer present in the current implementation.

`source_io.c:367`, `synthesis.c:911` and `synthesis.c:4208` still retain/import
an `allocation_origin` producer for declarations. Source checking waits for it
to recover the declaration schema, then checks the source declaration. This is
a real remaining source-to-proof-to-allocation dependency.

Finish the existing raw-allocation migration: retain the nominal declaration
and field/index binder allocation as descriptive inputs, recheck their types
through ordinary source synthesis. Delete this allocation-only derivation edge.
Do not delete independently selected theorem roots or silently trust loaded
field types. The analogous Match allocation change is already in this worktree.

### 3. Repeated scope-array assembly

`evidence.c:4466` (`pg_prove_substitution_compose`) walks all images and makes
a temporary proof array on each call. Individual occurrence actions are shared,
but assembling and checking the aggregate still happens before the final
substitution record can be reused. `typing.c:351` (`pg_context_map`) also walks
the telescope/images to find an already interned map.

First measure repeated identical compositions on imported QuickSort and indexed
transport. If material, share the composition construction at the existing
typed-map operation boundary, keeping proof premises distinct. Do not introduce
an unchecked `(map pair) -> accepted proof` cache or replace alternate derivations.
This is repeated traversal, not evidence of competing mutable type solutions.

### 4. Nested synchronous work

Several `evidence.c` wrappers drain existing resumable work with `1024` or
`UINT64_MAX` loops (for example `pg_prove_classifier` at 5235 and substitution
composition/lifting). Thus a single outer Solve step need not be a small amount
of work. This concerns budget granularity, not necessarily recomputation.

Incremental callers should depend on the existing request/advance/result
interfaces. Keep synchronous entry points as thin wrappers where needed; do not
add another scheduler or duplicate the checked rule implementations. Count inner
steps as well as outer steps before reporting a performance improvement.

### 5. Source call-layout rediscovery

`synthesis.c:3197` (`function_graph_order`) walks source markers and block groups;
`function_graph.c` separately discovers typed recursive/helper calls. They
currently answer different questions: source field order/names versus checked
call dependencies. Repeated calls of one IH in the same group remain restricted.

Retaining call-site provenance during elaboration could remove the extra source
walk. It must distinguish two uses of the same binder, shadowing, and block
cutoffs. Do not identify a call site merely by its erased Core or IH field.
This is a design candidate, not a proven drop-in deletion.

## Unifications Not Justified

- `pg_synthesis_job` is pending work, not an independent proof authority.
  `EVIDENCE_JOB` wraps an already accepted proof; it does not prove it again.
- `pg_occurrence` describes a typed use; `pg_evidence` records checked premises.
  Sharing erased `lambda x. x` does not identify its Bool and Nat typed uses.
- `pg_classifier_request` already keys on the exact typed subject and uses its
  retained type or scoped structural action. Do not describe it as fresh source
  type synthesis on every request.
- Typed input queries and structural occurrence actions already share requests.
  An additional checked boundary is not by itself redundant computation.
- Effect equation values have a solver owner; immutable closed effect rows are
  outputs. No competing effect-solution authority was established here.
- Conversion's WHNF-to-NF fallback uses the same normalization work store.
  Structural interning must not absorb conversion or observational equality.
- Match, App, IH and Fold prove different rules. Encoding them through Lambda
  is not a reason to erase those proof obligations.

## `@f` and `*f`: Fewer Spellings, Not Fewer Objects

`function_graph_request`, `function_graph_step`, `function_witness_step` and
`graph_reference_step` (`synthesis.c:3331-3440`) share the same graph owner:

```text
@f : input -> output -> Type              // relation family G_f
*f : input -> computation of (y, G_f x y) // generated result/witness packet
```

These are schematic, not literal current surface types. Totality does not make
the relation family and a term producing its witness interchangeable. Graph
elimination needs the former; connecting a property to a computed output needs
the latter. There is no second graph-extraction engine to delete here.

Surface proposal for discussion: keep `@f` and expose the existing generated
witness as a named member, e.g. `(@f).witness`, instead of global `*f`. This
changes elaboration/export naming only and must reserve or disambiguate that
member against graph constructor aliases. It is **not implemented or approved**.
Keep IH `*k` and recursive-type Self `*` unchanged.

Making ordinary `y := f x` automatically supply `@y` is a different, larger
proposal: preserve the particular call's provenance and pair its value with
its proof without executing effects twice. Do not infer that provenance merely
from the value or rename `*f` to `@f` while discarding one of the two types.

## Execution Order and Gates

Priority update: the [IADT / issue 29 / PR 30 plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md)
now precedes the unchecked broad refactoring items below. Keep the findings and
existing changes, but do not let this cleanup displace those two user priorities.

- [x] Reproduce and fix the two isolated #29 limitations; preserve remaining
  proof limitations in the linked report.
- [x] Audit the active Core/typed/Solver/persistence paths and identify existing
  sharing as well as actual reconstruction.
- [x] Finish declaration raw allocation (target 2, existing parent A1-A4 work).
- [ ] Consolidate pending construction (target 1), deleting replaced paths in
  the same change. No permanent old/new dual authority.
- [ ] Measure targets 3-5; adopt only changes with demonstrated simplification
  or bounded-work benefit. Record rejected proposals and why.
- [ ] Run pending/resumed handler effects, synthetic post-check, nominal identity,
  alternate derivation, indexed transport and imported QuickSort regressions.
- [ ] Report implementation/header, test and documentation LOC separately;
  compare allocations and work, not just wall time or outer Solve steps.

Current implementation/header delta is **+891/-580, net +311** from R76, and
**+5883/-3156, net +2727** from `4657cc6`. These include the preceding worktree
refactor, not just #29. Tests/docs/build files are excluded. The parent
net-negative gate is not met. This audit does not declare the refactor complete
or authorize publication as a finished cleanup.
