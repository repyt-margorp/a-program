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

### Stream Lexical Addresses (after `c911f16`)

Every application previously copied enclosing binders into `application_state`,
even when it allocated no sequencing/index binder. Other lexical allocations
first copied the source scope and then copied it again into the canonical
source-binding record. Both application paths now use the existing lexical
registration helper. The two application snapshot fields and eager builders
are removed. A stack-local cursor reads either the immutable source scope or
an imported address array; only a missing address allocates/copies its record.
This is not a new persistent scope representation or a polarity/proof cache.

Array and source lookup share the same pointer-order key, validation and
conflict rules. The nested telescope regression checks that array lookup finds
the already registered lexical binder without adding a record. Existing
one/two-binder application, imported address, shadowing and inferred-index
tests exercise both allocation paths.

Same-input `-O0 -g` comparison against a `git archive c911f16` build, at
`main.c:395`, `--steps 1000000` (IF8 also uses `--legacy-intrinsic-dot`):

| Quantity | IF8 before / after | length property before / after |
|---|---:|---:|
| Arena used bytes | 19,799,584 / 19,762,560 | 4,216,512 / 4,211,040 |
| Arena capacity bytes | 19,955,712 / 19,922,944 | 4,259,840 / 4,259,840 |
| Solve transitions | 47,046 / 47,046 | 9,569 / 9,569 |
| Jobs / source bindings | 15,319 / 344 unchanged | 3,430 / 86 unchanged |
| Proofs / occurrences / Terms | 23,315 / 19,522 / 11,969 unchanged | 4,941 / 3,574 / 2,031 unchanged |

Arena totals include alignment and exclude index-table allocations. No
wall-time speedup is claimed. Implementation is +48/-38 (net +10), tests +5;
the local cursor removes allocation/state but does not reduce source LOC.
The cumulative reduction gate remains unmet; no exception is granted.

- [x] Remove eager application snapshots and the intermediate lexical array.
- [x] Debug synthesis, source/array address equivalence and same-input metrics.
- [x] Full optimized acceptance and affected ASan/UBSan tests.
- [x] Publish and verify both remote tips (`1760387`).

Strict `-O2 check-acceptance` exited 0, including compatibility 63/63 and
QuickSort universal proofs, images and invalid-claim checks. Normalized export
results match the preceding binding-origin epoch. ASan/UBSan synthesis,
`source_io.sh`, `handler-nesting` and `handler-boundaries` all exited 0 with
leak detection and halt-on-error enabled and no diagnostics. Logs:
`/tmp/a-program-authority-app-scope-acceptance.log` and
`/tmp/a-program-authority-app-scope-asan-{synthesis,source,handler,boundaries}.log`.
Sanitizer flags: `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`, with strict C11 warnings.
Implementation/header totals: R76 +2,269/-1,022 (net +1,247);
R0 +7,227/-3,564 (net +3,663). The broad R acceptance gates remain open.

The global source-origin scan remains open. A reverse index populated only
when a source producer finishes is insufficient: member allocations become
visible through a completed child before its source parent advances, and
restored allocations are visible without advancing at all. Any replacement
must cover those edges, not introduce a second mutable acceptance authority.

### One Lexical Binding Origin (after `abeedc6`)

`pg_synthesis_binding_at` previously skipped the immutable source-binding
registry when given an explicit binder. Consequently the source writer kept
a second origin path: enumerate every BINDING_JOB, recover its lexical scope,
and add it while retaining referenced objects. Automatic bindings were also
visited by that path, despite already having a source-binding record.

Explicit and automatic Lambda/Pi binding now use the same address registration
(`syntax`, slot, enclosing binders). An explicit binder must agree with that
address. The BINDING_JOB visitor branch, allocation-object case, and writer's
binding-origin fallback are deleted. Legitimately referenced lexical scopes
still travel through ordinary environment dependencies. Source annotations
and independently selected proof roots are still rechecked, not trusted.

Existing telescope tests now verify that explicit and automatic allocation
already registered the exact binder without adding a record on lookup, and
that Lambda/Pi-only inputs have no second job-based origin. Existing alias,
shadowing, alternate-proof, rejected-root and inert-resave checks pass.

The first full acceptance run stopped at `invalid_handler_binding`: its
malformed-image fixture expected an internal Handler scope to be retained
indirectly by the removed Lambda-origin path. The fixture now explicitly
selects an expression using the clause's request binder. All four corruption
checks remain; none was skipped or weakened. Focused nested-handler tests
then passed with exact source/evidence Core and classifier comparisons.

GDB callback counts for `source_io_test context-scopes` (70 writes):

| Callback | Before | After |
|---|---:|---:|
| `index_origin` | 294 | 28 |
| `index_binding` | 140 | 188 |
| `collect_origin` | 318 | 56 |
| `collect_binding` | 136 | 184 |

Candidate callbacks fall from 434 to 216; the extra binding records replace
the explicit-binder bypass. This does **not** eliminate the scan over all
jobs/bindings. Initial measurements used the previous ASan binary and the new
debug binary; the new ASan build then reproduced exactly these counts with
the same flags as the previous ASan binary. GDB disables leak detection only
for counting under ptrace; the separate sanitizer tests enable it.
The existing function-image fixture shrinks from 12,541 to 12,285 bytes.
Old-reader/new-image and new-reader/old-image checks both pass; no wire version
change was needed. These are fixture measurements, not a general speed claim.

- [x] Unify registration and remove the superseded origin path.
- [x] Debug synthesis/source-image suites and bidirectional image compatibility.
- [x] Full optimized acceptance and affected ASan/UBSan tests.
- [x] Verify publication to both remote tips (`54b12df`).

Final strict `-O2 check-acceptance` exited 0 (63/63 compatibility). Its export
results match the previous epoch after normalizing temporary paths/step counts
and execution order. Strict debug synthesis and source-image tests also pass.
ASan/UBSan synthesis, `source_io.sh`, `handler-nesting` and `handler-boundaries`
all exit 0 without diagnostics, with leak detection and halt-on-error enabled.
Logs: `/tmp/a-program-authority-bindings-acceptance-final.log` and
`/tmp/a-program-authority-bindings-asan-{synthesis,source,handler,boundaries}.log`.
The initial failed acceptance log is retained separately, not overwritten.

Implementation/header delta: `source_io.c` +5/-12, `synthesis.c` +8/-15,
`synthesis.h` +3/-2, total net -13. Tests: `tests/synthesis.c` +13/-0,
`tests/source_io.c` +10/-0.
The output-sensitive traversal and broader R completion gates remain open.
Implementation/header totals: R76 +2,256/-1,019 (net +1,237);
R0 +7,218/-3,565 (net +3,653). The cumulative reduction gate is still unmet.

### Source Value Kind from Accepted Judgements (after `9fc2ecd`)

`source_value_kind` called `body_rule_polarity`, which repeated the same
origin traversal and evidence judgement lookup. It now owns one traversal,
stopping at an accepted typed result instead of following completed recipes.
The redundant helpers are deleted. Pending adapters still expose their
prepared rule; this information is needed to close effect equations before
acceptance. No polarity cache, new job kind or proof authority was introduced.

This is descriptive classification, not acceptance: BODY still checks its
operand and Context, and application still checks its actual premises. An
unknown kind still waits for the producer; a namespace is not a value simply
because its preparation completed. Type-family preparation remains before
classification, including the existing pending `F n` regression.

`source_body_kinds` compares pending-name and accepted-name use for a universe,
a raw Lambda, a quoted Lambda and a computation block. Both schedules must
give the same judgement and the exact expected Lambda body up to alpha;
only values/types acquire RETURN. Existing pending-effect tests also pass
in the strict debug synthesis suite.

Debug comparison with the previous published binary used the same two inputs
and `main.c:395` counters as the projection audit below. All counts are
unchanged: IF8 Solve 47,046, actions 7,072, substitutions 5,663, proofs 23,315,
occurrences 19,522, Terms 11,969; length property 9,569 / 1,431 / 934 /
4,941 / 3,574 / 2,031 respectively. This removes repeated inspection within
steps, not the number of scheduled steps. No wall-time speedup is claimed.

- [x] Consolidate classification and add pending/accepted body regressions.
- [x] Strict debug synthesis and same-input work/node comparison.
- [x] Full optimized acceptance and affected ASan/UBSan tests.
- [x] Publish only after verification; confirm both remote tips (`50a7d1f`).

All commands exited 0: strict `-O0 -g` synthesis, strict `-O2`
`check-acceptance` (63/63 compatibility), and ASan/UBSan synthesis plus the
complete `source_io.sh`. Sanitizers used `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`, leak detection and halt-on-error.
Logs are `/tmp/a-program-authority-polarity-{synthesis,acceptance,
asan-synthesis,asan-source}.log`. No sanitizer diagnostics were reported.
Optimized export-result records match the preceding projection epoch after
normalizing temporary paths and step counts and sorting execution order.

Implementation: `synthesis.c` +13/-32 (net -19); tests: `tests/synthesis.c`
+31/-0. Documentation is separate. Broader pending-classifier construction,
source allocation traversal and the cumulative net-negative gate remain open.
Implementation/header totals: R76 +2,257/-1,007 (net +1,250);
R0 +7,221/-3,555 (net +3,666). The overall reduction requirement is not met.

### Shared Projection Action (after `8503fd6`)

Typed input traversal had a private projection shortcut, while callers of the
ordinary occurrence action substituted Core/classifier/annotation even for
that same projection. Projection now belongs to `occurrence_action_step`;
input traversal always uses the existing action request. The separate input
completion branch is deleted. No new job kind, cache, wire field or acceptance
rule is introduced. The direct projection API used by the kernel still shares
the same construction without requiring an action request.

This optimization requires an exact Context prefix and identity Core images,
as checked by the existing `pg_occurrence_projection`. It is not DefEq-based
interning. A general map, including one changing images without changing its
source/destination, still goes through substitution and ordinary proof checks.

The 10,000-level input test now checks one shared identity action, one action
transition, zero Core substitution requests and zero proofs, instead of zero
action requests. Its 20,002 input transitions and exact result are unchanged.
A separate weakening test checks zero-fuel suspension followed by one-step
completion, exact agreement with the kernel projection, and no new evidence
or Core substitution request. Existing non-identity/chunked-map checks remain.

Same-input debug comparison at `main.c:395`, `--steps 1000000`:

| Quantity | IF8 before / after | length property before / after |
|---|---:|---:|
| Solve transitions | 47,513 / 47,046 | 9,674 / 9,569 |
| Occurrence actions | 7,066 / 7,072 | 1,431 / 1,431 |
| Interned Core substitution jobs | 5,854 / 5,663 | 1,040 / 934 |
| Accepted proofs | 23,315 / 23,315 | 4,941 / 4,941 |
| Typed occurrences | 19,522 / 19,522 | 3,574 / 3,574 |
| Core Terms | 11,969 / 11,969 | 2,031 / 2,031 |

IF8 uses `--legacy-intrinsic-dot` and the unchanged
`if8_fuel_free_quicksort_check.p`; the other input is `length-output-proof.p`.
The extra six action records are shared projection requests, not extra proof
obligations. A preliminary `-pg` run was too short for a reliable time profile;
these counters establish less substitution work, not a wall-time speedup.

- [x] Shared action implementation and focused debug Core tests.
- [x] Full optimized and full ASan/UBSan acceptance; affected suites repeated
  with leak detection and halt-on-error enabled.
- [x] Publish after verification and confirm both remote tips (`fe62337b`).

Verification logs: `/tmp/a-program-authority-projection-core.log` (strict
`-O0 -g`), `-acceptance.log` (strict `-O2`) and `-asan.log` (full strict
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`).
All commands exited 0. The full sanitizer run reported no diagnostics; Core,
synthesis and `source_io.sh` were then repeated with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`, also exit 0, in `-asan-{core,synthesis,source}.log`.
All optimized `export results` records agree with the declared-Context baseline
after removing temporary paths and step counts and sorting execution order.

Implementation: `typing.c` +5/-9, net -4. Tests: `tests/core.c` +12/-1,
net +11. Documentation is separate. Pending classifier reconstruction, source
allocation traversal and the overall net-negative/final acceptance gates remain
open; this is not completion of R2-R5.
Current implementation/header totals remain +2,245/-976 (net +1,269) from R76,
and +7,208/-3,523 (net +3,685) from R0. This local reduction does not satisfy
the cumulative reduction requirement.

### Declared Types from Accepted Contexts (after `b4b674b`)

`declared_type_step` previously inspected source/derivation recipes even when
the Context was accepted. It now reads the declaration keyed by the exact
binder pointer in that Context first. The old late-result lookup is removed.
Existing symbolic snapshots remain immutable; a later query may use the closed
accepted declaration, as for the other structural views.

A pending PI_SCOPE now exposes its already-prepared CONTEXT_EXTEND through the
existing preparation notification and projection path. The dedicated Pi-domain
reconstruction branch is removed. Its binder allocation is still owned by the
scope request; no alternative allocation key or proof cache is introduced.
Source BINDING's family-parameter adaptation is retained: converting a surface
thunked family annotation into a family Context is not plain Context lookup.

Fresh debug IF8 original QuickSort measurements, before / after:

| Measurement | Before | After |
| --- | ---: | ---: |
| Declared-type step invocations | 163 | 101 |
| Invocations with an accepted Context | 126 | 64 |
| Solve steps | 47,619 | 47,513 |
| Requests | 15,378 | 15,318 |
| Proofs / typed occurrences / Core Terms | 23,315 / 19,522 / 11,969 | unchanged |

The `length-output-proof.p` control also drops from 9,698 to 9,674 steps and
3,445 to 3,430 requests, retaining 4,941 proofs, 3,574 occurrences and 2,031 Terms.

Counts use GDB at `declared_type_step` and `main.c:395`, with
`--legacy-intrinsic-dot --steps 1000000`. This measures less reconstruction,
not a demonstrated wall-time speedup. The new test constructs an accepted
dependent Context and a still-pending effectful Pi; its structure must read the
Context directly without requesting the original annotation's structure. The
Pi is subsequently checked after effect closure. Existing pending-effect tests
exercise source/Pi contexts at chunks 1/64 and invalid Context rejection.

- [x] Implement direct accepted lookup and shared pending Pi-scope projection.
- [x] Debug synthesis, focused no-reconstruction test, and work/node measurement.
- [x] Full optimized acceptance (63/63 compatibility) and ASan/UBSan synthesis.
- [x] Publish after the gates and verify both remote tips (`2cc3e5a`).

Per-file delta: `synthesis.c` +11/-13 (net -2); `tests/synthesis.c` +45/-0.
Remaining pending classifier reconstruction and the broader R gates stay open.

Verification: strict `-O0 -g` synthesis; `make -f
src/prototype/pointer/Makefile -j2 BUILD=/tmp/a-program-authority-declared-opt
check-acceptance` with strict `-O2`; synthesis with strict `-O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Logs:
`/tmp/a-program-authority-declared-{synthesis,acceptance,asan}.log`.

### Shared Type-Term Construction (after `b21dc46`)

Type-forming rules were interpreted separately by TYPE_STRUCTURE and
TERM_STRUCTURE. For Pi/F/U formation and their structural projections, the
ordinary Term path could wait for acceptance while the type path constructed
an early symbolic Core. Interning the eventual Core did not share this work.

Known type-forming derivations now use the **same TERM_STRUCTURE request** for
both APIs. The existing type-former construction is owned by that request;
Universe/host-type construction is no longer duplicated. The type-specific
wrapper remains for unresolved source and Context projection: projecting a
value must not make it usable as a type. No pending proof is trusted, and
invalid formation descriptions may still be constructed before their ordinary
typing rule rejects them. This does not merge erased computation with typing.

After `source_preparing` has released a handler, its selected
rule already specifies the result carrier. Zero clauses use effect subsumption
over FOLD; nonzero clauses use HANDLER_ELIM. Their ordinary classifier
projection now replaces the source-specific carrier lookup and dependency
polling branch. Clause/signature checking and effect equation ownership remain.

- [x] Share known formation requests between both views; retain type guards.
- [x] Test both request orders with unresolved effects and shared request identity.
- [x] Reject a value projection used as a type after effect closure.
- [x] Remove redundant source-handler carrier projection; debug synthesis passes.
- [x] Full optimized acceptance (including 63/63 compatibility) and ASan/UBSan synthesis on the final diff.
- [x] Record work, node and per-file counts.
- [x] Publish only after verification and check both remote tips (`1a08868`).

Pending classifier inference still has rule-specific computation (including
Pi application, effects and dependent substitution). This is not completion of
the entire pending-structure cleanup or the parent net-negative gate.

Fresh debug checker measurements at `main.c:395`, before (`b21dc46`) / after:

| Input | Solve steps | Requests | Proofs | Typed occurrences | Core Terms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original IF8 QuickSort | 47,619 / 47,619 | 15,378 / 15,378 | 23,315 / 23,315 | 19,522 / 19,522 | 11,969 / 11,969 |
| `inferred-index-effects.p` | 5,530 / 5,530 | 2,170 / 2,170 | 1,275 / 1,275 | 939 / 939 | 648 / 648 |

These source workloads do not show a work reduction; no speedup is claimed.
The pending-effect unit regression directly tests the eliminated duplication:
asking both views of each Universe/F/U/Pi producer returns the same request,
with no second allocation, in either request order. It also checks that the
symbolic snapshot survives effect closure and invalid formation still fails.

Per-file delta: `synthesis.c` +35/-35 (net 0); `synthesis.h` +4/-2 (net +2,
API comments); `tests/synthesis.c` +21/-0. Documentation is counted separately.

Verification: strict `-O0 -g` synthesis; `make -f
src/prototype/pointer/Makefile -j2 BUILD=/tmp/a-program-authority-type-core-opt
check-acceptance` with strict `-O2`; and synthesis with strict `-O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1`. Logs are
`/tmp/a-program-authority-type-core-{synthesis,final-acceptance,asan}.log`.

### Substitution Telescope Checking (after `7eeefb8`)

The composition audit did not find repeated source synthesis in each image:
`pg_occurrence_action_request` already shares that work. Adding a map-pair to
accepted-proof cache is not justified. Composition's image-premise array and
its synchronous wrapper remain; this change does not claim to remove them.

One removable copy was in the common `substitution_build` admission path.
It copied the source declaration chain into a reverse array on every request,
including already accepted requests, and allocated the typed-image array
before consulting the existing proof index. It now:

1. Assembles the actual source, destination and image premises, preserving
   projection of a lifted prefix, ownership and destination checks.
2. Looks up that exact proof in the existing immutable derivation index.
3. On a miss, forms the structural map and checks its new suffix by traversing
   the declaration chain directly, newest first. Each classifier is still
   substituted using only its preceding images (`retained + i - 1`).

The order of independent checks changes, not the dependent substitution scope.
All images exist before checking; none obtains acceptance from a later image.
A rejected request may leave an interned descriptive map, never an accepted
substitution. Alternate image evidence still yields a distinct derivation for
the same structural map. No cache, term tag, proof rule or wire change is added.

Debug GDB measurements on the same IF8 original QuickSort input, with
`--legacy-intrinsic-dot --steps 1000000`:

| Measurement | Published baseline | Current |
| --- | ---: | ---: |
| `substitution_build` scratch-array allocation calls | 16,791 | 9,544 |
| Requested scratch-array payload bytes | 609,896 | 455,784 |
| Declarations copied into a scratch array | 13,563 | 0 |
| Exact accepted-proof lookup hits | 1,650 | 1,650 |
| Solve steps / requests | 47,619 / 15,378 | 47,619 / 15,378 |
| Proofs / typed occurrences / Core Terms | 23,315 / 19,522 / 11,969 | 23,315 / 19,522 / 11,969 |

Counts come from breakpoints at the scratch allocations and exact-proof lookup,
not a new production counter. Payload bytes exclude allocator overhead. These
are local allocation savings, not a measured wall-time speedup. Logs:
`/tmp/a-program-scope-{before,after}-measure.log`.

Focused regression coverage repeats both exact and alternate substitutions,
checks that no proof/map/substitution work is added, and rejects incorrect
first and last dependent images without accepting a proof. Existing pairing,
composition, lifting, family-context and image round-trip tests remain gates.

- [x] Debug Core suite.
- [x] Full optimized acceptance and compatibility suite (63/63).
- [x] ASan/UBSan Core suite with leak detection.
- [x] Review delta and publish only after all gates pass (`5ec308a`, both remote tips verified).

Gate: `make -f src/prototype/pointer/Makefile -j2
BUILD=/tmp/a-program-authority-scope-opt check-acceptance`, default strict `-O2`.
Core also passed with `-O0 -g` and `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`. Sanitizers used
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Logs: `/tmp/a-program-authority-scope-{core,acceptance,asan}.log`.

Implementation delta: `evidence.c` +10/-11 (net -1); tests `core.c` +15/-0.
This is a limited scope-copy cleanup. Pending structural reconstruction and
the parent R net-negative gate remain open.
For `src/prototype/pointer` implementation/header files, excluding tests, the
cumulative diff is +2,193/-920 (net +1,273) from R76 `3a3bf550`, and
+7,166/-3,477 (net +3,689) from `4657cc6`. This does not satisfy that gate.

### Sequence Construction Choice (after `a37b283`)

Term projection independently decided whether a provisional FOLD could be
exposed, while classifier projection recovered from that FOLD's failure and
sequence acceptance separately selected pure APP or an effect-preserving
quoted result. These were three consumers of the same source adaptation choice.

Only `sequence_step` now publishes that choice through its existing `value_job`.
It may expose FOLD early when the descriptive domain agrees, the codomain is
independent, and either the result is F or the prefix is explicitly total/pure.
These are sufficient selection conditions, not a typing judgement. All original
premises and the requested Context still undergo checking. Cases needing
conversion, finite RETURN inversion or effectful quotation await that checking
before publishing a choice. Once published, a choice is not replaced.

Deleted: the term-projection FOLD eligibility branch and the classifier's
provisional-FOLD recovery branch. The checked sequence fallback itself remains;
this is not a claim that speculative candidate construction is eliminated.
No Core tag, proof rule, accepted-state store or image format is added.

The new unit test requests term/classifier structure before acceptance for both
constant and dependent raw-Pi continuations, verifies FOLD versus APP, and checks
both snapshots agree with the final evidence. Existing pending effect equations,
wrong Context and invalid FOLD tests also pass in debug and ASan/UBSan. Full
optimized `check-acceptance` passes, including 63/63 compatibility cases and
saved sort-property witnesses. No implementation/test edits followed the gates.
Local logs: `/tmp/a-program-authority-sequence-synthesis-final.log`,
`/tmp/a-program-authority-sequence-asan.log`,
`/tmp/a-program-authority-sequence-acceptance.log`. Flags and the affected-only
sanitizer scope match the preceding epochs.

Same-input debug counters, before/after:

| Input | Solve steps | Requests | Proofs | Occurrences | Terms |
|---|---:|---:|---:|---:|---:|
| `examples/06_pred.p` | 762 / 780 | 285 / 294 | 224 / 224 | 157 / 157 | 120 / 120 |
| `length-output-proof.p` | 9,369 / 9,698 | 3,301 / 3,445 | 4,941 / 4,941 | 3,574 / 3,574 | 2,031 / 2,031 |
| Original IF8 QuickSort | 46,766 / 47,619 | 14,998 / 15,378 | 23,315 / 23,315 | 19,522 / 19,522 | 11,958 / 11,969 |

Central selection requests shapes even when no external projection requested
them yet. Thus this is consolidation, not a speedup: QuickSort adds 853 steps,
380 requests and 11 descriptive Terms, without adding accepted evidence or
typed occurrences. Remaining shared pending-construction work must address that
cost without restoring independent consumer-side choice. `synthesis.c` is
+76/-76 (net zero, including removal of an unnecessary nested scope);
`tests/synthesis.c` is +27/-0. Documentation is counted separately.

### Preparation Notifications (after `7c7a58b`)

`await_source_preparation` followed the producer's changing dependency or
requeued its consumer. Only BODY and stored derivation adapters used direct
preparation subscriptions. All these paths now subscribe to the original
producer. `source_preparing` is the shared predicate for subscription and
publication at the existing scheduler step boundary. The two local publication
calls are removed. Completion still wakes all consumers; early preparation
wakes only preparation consumers, without accepting evidence or closing effects.
There is no extra queue, cache, completion flag or Core/typed-data change.

The new unit test fixes the queue order so the structural consumer encounters
an unprepared application, checks its direct producer dependency, checks the
final Core agrees, and checks the drained queue does no further work. Existing
pending-effect, handler, failed-input, deep stored-DAG and cycle tests pass in
debug and ASan/UBSan builds. Full optimized `check-acceptance` passes, including
63/63 compatibility cases, sort properties, resaves and invalid claims.
No implementation or test changes followed these runs. Local logs:
`/tmp/a-program-authority-preparation-synthesis-final.log`,
`/tmp/a-program-authority-preparation-asan.log`, and
`/tmp/a-program-authority-preparation-acceptance.log`. Flags match the body epoch
below; sanitizer coverage is the affected synthesis suite, not all acceptance.

Same-input debug counters, before/after; proof/occurrence/Term counts are unchanged:

| Input | Solve steps | Requests | Proofs | Occurrences | Terms |
|---|---:|---:|---:|---:|---:|
| `examples/06_pred.p` | 766 / 762 | 285 / 285 | 224 | 157 | 120 |
| `length-output-proof.p` | 9,813 / 9,369 | 3,308 / 3,301 | 4,941 | 3,574 | 2,031 |
| Original IF8 QuickSort | 48,875 / 46,766 | 15,017 / 14,998 | 23,315 | 19,522 | 11,958 |

Fewer requests reflect accepted projections becoming available at different
queue positions, not removal of proof checks. These are work counters, not
wall-clock claims. Implementation `synthesis.c`: +14/-17 (net -3); permanent
tests `tests/synthesis.c`: +28/-0. Other pending reconstruction remains open.

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

Historical audit-baseline implementation/header delta was **+891/-580, net +311** from R76, and
**+5883/-3156, net +2727** from `4657cc6`. These include the preceding worktree
refactor, not just #29. Tests/docs/build files are excluded. The parent
net-negative gate is not met. This audit does not declare the refactor complete
or authorize publication as a finished cleanup.
