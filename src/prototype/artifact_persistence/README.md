# Artifact Persistence Prototype

## Problem List

1. Typed occurrences cannot use the existing nominal descriptor codec.
2. Temporary Ref wrappers create duplicate transport records for one object.
3. Source images omit materialized results; continuation/validation reuse is missing.
4. Whole-store reduction archives retain unused history across generations.

## Subjective (User)

2026-09-28, paraphrase: implement the
[semantic persistence plan](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md),
preserve computational/typed separation, remove redundant retained history and
test zero fuel and equivalent budget partitions. The initial instruction to
defer C transpilation was superseded on 2026-09-29 by the explicit request to
implement it; AP4 in the linked plan tracks the separate downstream backend.
Follow-up, reaffirmed 2026-09-29: backward compatibility is not required. Use only the
current format and remove migration paths/tests.

## Objective (Code)

Baseline implementation: `e716232`, unchanged in `a696276`. The prototype applies
small patches to existing owners, without modifying accepted files. The default
overlay uses the working tree; `ARTIFACT_SOURCE=/absolute/path/to/src` selects a
clean comparison source. Unrelated work in `evidence.*`/`iadt.*` is not included.

Occurrence/scoped occurrence transport now accepts `pg_graph_codec`, including
the enclosing shared-root table. The old name/resolve API delegates to it.
The dependency DAG optionally projects transport keys; only Ref wrappers are
mapped to a scratch-owned Ref with the identical object pointer. Lambda/App
nodes remain borrowed. No alpha comparison, reduction or nominal merging occurs.

Focused transport tests pass on the working tree and the clean baseline:
same Core/different classifiers, distinct nominal declarations, shared contexts,
scope formation, descriptor/Core aliases and three byte-identical inert cycles.
The focused test links evaluation/substitution entry points to aborting stubs;
loading/writing must not call them. Loaded proof/query/action stores remain empty.
Missing descriptor support and insufficient bounds reject without publishing
output roots or counts, through both scoped and unscoped descriptor entry points.
Existing graph, declaration and occurrence transport tests also pass. This is
not a complete source-image persistence or checkpoint implementation.
The prototype now writes APGSRC66/APGRET4 with available typed results connected
to existing producer ordinals. Its view borrows `pg_occurrence`; there is no new
type graph or accepted-result DB. Import preserves descriptive input only, and
ordinary Solve still checks source/annotations. Selected producer dependencies
are roots; allocation-only source origins are not additional semantic exports.
Program-owned archives and the all-store snapshot API are removed. CLI/REPL use
one semantic writer; `--retain-reductions` explicitly errors. Live memoization,
explicit reduction records and their checking API remain as a separate payload,
not a Program-format fallback. Older Program/occurrence formats reject; there
is no migration path or archive field in the semantic payload. Selected Core
supplies necessary source allocations; default
constructor fields reuse their existing lexical addresses instead of retaining
every incidental use-context proof. Structural identity rules are unchanged.

APGSRC66 adds one descriptive completion byte per producer, using the existing
producer associations and typed graph. It does not restore live `DONE` status,
accept Kernel evidence, or save private cursors. `artifact/file.c` owns explicit
trusted export: before any Solve, a completed source module and all its entries
(including imports and `::`) must have saved completion, and the selected local
definition must have a closed materialized result. Registration completion alone
is insufficient. Inputs-only images omit both results and completion. Trust is
an unauthenticated user choice, not a proof reconstructed from a file flag.
Ordinary checking ignores it. Inert resaves preserve completion; after any Solve,
only local completion is written. This conservative interim policy is not exact
checkpoint resumption or authenticated reuse. The C adapter exposes this choice
as `--trust-image`; the compiler's ordinary Solve path is unchanged.
Fresh O2 history/transport/semantic, seed and C differential tests pass on clean
`e716232` plus the prototypes. ASan/UBSan pass semantic/transport, generated-C
differentials, file policy and QuickSort's unsupported boundary. Source-format
tests now skip/check the completion byte before mutating their original payload
fields; they still exercise the intended invalid source relationships.

Fresh verification of the patches following `5eaf3b7`, applied to clean `e716232`:
`check-artifact-history`, `check-artifact-transport` and seed tests pass. They
cover source/normalization, explicit reduction checking, CLI/REPL roundtrips,
invalid inputs and Example 09 fuel curves. ASan/UBSan pass the focused semantic
and source suites. The added test checks that 32 unrelated evaluation jobs do
not change saved bytes. Saved Core is captured before rechecking, not compared
against a second lookup of the fresh result. Previous transport/derivation and
typed/scoped tests passed. The artifact-only acceptance run found a read failure
in assertion-free finite permutation views; the combined candidate below fixes
that case, but not the larger F3/F4 image failures.
The stale seed-header assertion was updated, retaining its old-header rejection
and truncation checks. The migration test and backward-compatible readers were
removed following the user's clarification.
Focused O2/ASan/UBSan tests also reject older/unknown occurrence and scoped-payload
headers without publishing roots or allocating Terms; no fallback reader is used.
The semantic payload likewise rejects APGRET versions 0-3 and 5 without changing
outputs or allocating Terms/proofs; version 4 succeeds (O2 and ASan/UBSan).

APGOCC8/APGSCP2 omit absent annotation/classifier slots, rather than filling them
with Core references. General Sorted's isolated failing image shrinks from
31,467,832 to 29,381,856 bytes and its ordinary-result comparison passes the
unchanged bound. This does not settle the remaining Fin failure. The focused
test verifies the sparse payload directly, with no reduction or trusted receipts.
The fresh current-format Fin image is 20,875,920 bytes. Its shared Core section
contains 214,216 objects, 814,001 Terms and 23,963 root references, exceeding the
existing 1,000,000-unit bound before typed-payload loading. This is not an old
format rejection. Persistence-root growth still needs investigation; no limit
increase or semantic omission has been adopted to make this test pass.

On 2026-09-29, `candidate.sh` combines these patches with the existing
`readback_support` and `conversion_head` prototypes, without duplicating them.
Full `check-acceptance` passes on clean `e716232` with this combination, before
the effect-order fix below. The same Fin case is 20,091,101 bytes and compiles
in 927,681 rather than 1,475,909 steps with readback sharing; default-bound import
passes. The separate QuickSort and MergeSort F3/F4 gates fail at image
loading before the byte-bound prototype below. The full QuickSort image is
32,372,583 bytes: 284,806 objects,
1,062,813 Terms and 46,350 root references exceed the unchanged 1M-unit bound.
Its selected Core closure has 30,716 Terms, while typed headers reach 1,061,082
Terms. Dropping typed evidence or merging distinct binders is not an acceptable
size fix. Large-case diagnostics use an explicitly supplied API bound, never a
silent CLI limit increase; they do not turn these failing gates into passes.

The new metrics test observes section bytes, shared Core, typed roots/nodes and
rule-premise edges. It forbids synthesis, reduction, substitution and effect
solving, checks exact resave bytes and unchanged source stores, and compares all
zero-step generations. O2 and ASan/UBSan pass. Pending continuation counts remain
unfinished. The sorting scripts test one semantic image path, without an
ordinary/archive compatibility matrix. Large fixtures use an explicit fixed
allowance, as recorded below; the default is not silently increased.

A separate effect-cycle test reproduced a zero-step byte change: definition
enumeration followed pointer-hash bucket order. The fix links existing equation
and edge nodes in registration order; solver adjacency and scheduling are
unchanged. Twelve relocated generations, duplicate contributions, and partial/
complete solving preserve definition bytes. Focused semantic ASan/UBSan, Core
and derivation-I/O tests pass after this fix.

The next address-reversal test also exposed pointer order in closed effect rows
and Fold clause descriptors. Rows now retain their first construction order;
handlers expose source clause order. Sorted membership/dispatch indexes remain
private and derived. No extra wire table, typing rule or nominal interning rule
is introduced. There is one extra in-memory pointer/index per label. O2 and
ASan/UBSan verify exact bytes, set operations, alias reuse and clause positions
with reversed external-label addresses; semantic/metrics and Core tests pass.
Full `check-acceptance` passes again on this combined candidate with all three
ordering fixes (clean `e716232` baseline, exit 0), before the reader-bound change
below. The separate fuel-partition gate remains failing.
The existing `source_io_test handler-boundaries` now also checks zero-step byte
identity: all 4,483 snapshots over its four valid/invalid/forwarding cases pass.

The user rejected the byte-derived allowance on 2026-09-29. It has been removed.
`artifact/file.c` now owns atomic publication and file/pipe adaptation; the
decoder receives a fixed allowance (CLI default 1,000,000, `--image-limit N`
override), or explicit `--image-limit none` for no policy quota (`SIZE_MAX`).
Both compiler and C adapter use one argument parser in `artifact/file.c`.
Representability, allocation failure and format checks still apply without a
quota. The limit is per-payload record/reference/name units, not a byte,
memory or fuel budget. Non-seekable inputs are staged only because relocation
requires seeks. There is no byte-derived allowance or automatic retry.
The explicit-unbounded follow-up separates actual array allocation checks from
policy quotas in Core, source, occurrence and derivation readers, using one
checked wire-array allocator. O2 history/transport/semantic and focused
ASan/UBSan tests pass. Fixed and unlimited quotas produce identical images,
including the 32,372,583-byte QuickSort image at step 0. This is a reader-policy
correction, not compaction or progress-preserving resumption.

`file_policy.sh` checks normal and piped step-0 roundtrips, explicit small-bound
failure, argument errors, and optionally a large image with a 10,000,000 override.
The semantic suite checks the exact acceptance threshold and atomic failed
replacement. `artifact_metrics IMAGE 10000000 --retention` attributes the typed
closure without altering retention policy. The 32,372,583-byte QuickSort image
contains 178,600 typed nodes; header dependencies reach 1,061,082 Terms. Omitting
origin edges in the diagnostic reduces these to 25,834 / 344,220. This is not a
valid reduced codec: origins still feed typed selection/context actions.

The opt-in `--save-inputs FILE.a` profile instead omits materialized results and
retains reconstruction inputs/obligations. It shares the existing writer and
format, not a parallel serializer. Re-solving is explicit recomputation; this
profile is not backend-ready typed output or a progress checkpoint. `--save`
and REPL `:save` keep result retention. Conversion of the QuickSort image at
zero steps produces 3,070,587 bytes (90.5% smaller), with no materialized typed
nodes. Remaining source/rule/module obligations must still pass ordinary Solve.
Small tests cover recursive Match/IH, arithmetic, root aliases and invalid
siblings at zero/partial/completed export; large sorting gates also exercise
input-only recomputation and invalid-proof rejection.

Fresh fixed-limit/input-profile checks pass: history/CLI, examples 01-09 and
results, focused ASan/UBSan, file/pipe policy tests and five sorting backends.
Input-only general QuickSort and negative proofs are covered; other large
backends use result retention. Full acceptance for this increment, production
promotion, progress checkpoints and trust/fuel integration remain unfinished.

Historical byte-bound candidate: O2 and ASan/UBSan semantic tests pass, including offset-written images, truncation,
trailing bytes and no input acceptance. CLI pipe tests and fuel metrics pass.
Piped ASan/UBSan loading/resaving of the 32,372,583-byte image is byte-identical
with zero Solve steps. Quick/Insertion `all` passes in 135 seconds, including a
large rejected image that remains rejected after inert load/save; MergeSort
`views` passes in 94 seconds, TreeSort in 138 seconds, BubbleSort in 191 seconds,
and concrete value tests pass. Full `check-acceptance` passes after this change
(exit 0), including derived LT, both partition orders and invalid inputs.
Fuel continuation preservation and production promotion remain unfinished.

The isolated materialized-validation test uses existing derivation inputs and
kernel/Solve rules after destroying the source Program. Universe, Lambda,
constructor and recursive Match/IH cases recover the exact imported typed
subject in 9/69/304/930 charged steps; an unrelated retained subject remains
unaccepted (O2 and ASan/UBSan pass). This is not yet connected to source-image
resumption and does not resolve the progress/validation fuel contract.

The separate read-only consumer now checks a suspended function's Pi, formation,
binder and scoped body without forcing it or including source/synthesis headers.
Inspection next to an unfinished invalid sibling preserves exact image bytes
and zero steps; ordinary checking still rejects the whole module/selection.
This prototype test passes O2 and ASan/UBSan; it is not an execution-readiness API.

Image loading now uses the shared empty-owner constructor, without installing
another standard namespace beside the restored one. The zero-root image test
fails on the prior reader's unrelated runnable prelude and passes on this
candidate. Restored intrinsics and source extensions, history/CLI gates and
focused ASan/UBSan and full `check-acceptance` pass (exit 0). Example 09 from a
zero-fuel image drops from 3,129 to 2,942 steps without changing the completed
50,472-byte image. The strict partition gate still fails (100+100: 25,830 versus
26,070 bytes); this is duplicate-initialization removal, not checkpointing.
The five-backend sorting gate also passes: Quick/Insertion 135s, MergeSort 98s,
TreeSort 139s, BubbleSort 192s, and concrete value transport.

| Example 09 state | Bytes | Three inert generations |
| --- | ---: | --- |
| Step 0 | 25,234 | byte-identical |
| Step 100 | 25,670 | byte-identical |
| Complete | 50,472 | byte-identical |

Three completed generations also remain exactly 50,472 bytes. The old archive's
+444 bytes/generation path is gone. More typed construction is retained than in
the former source-only image, so this is not a blanket image-size reduction.
The strict partition gate still fails at 100+100, 1000+1000 and 1600+1600 across
reload. For 100+100, bytes are 25,474 versus 25,830; at 1600+1600 the split stays
pending while the single run is done. All tested in-memory partitions and the
10+10 versus 20 reload case pass. Those partition figures precede the empty-owner
fix; the current 100+100 result is 25,830 versus 26,070 bytes. Private continuations
and validation reuse remain
unfinished, not hidden as successful checkpointing.

## Assessment

These changes belong to existing transport owners. A second serialized type graph
or C-specific representation would duplicate their work. Reference wrapper
sharing is structural identity, not observational equality. The wire grammar is
unchanged for structural Terms; only the current semantic format is accepted.
Production adoption still needs complete regression coverage.

The user now permits explicit trust/reuse with one total fuel budget and a
validation sublimit. `artifact/file.c:pg_artifact_revalidate` bounds ordinary
Solve by `min(total_budget, validation_limit)`, stops at a terminal target,
and reports fuel actually used. Subsequent useful work must subtract that cost
from the same budget. The C adapter uses this through `--revalidate-limit`;
exhaustion stays pending and never selects trust automatically. The target's
ordinary kernel result remains the only checked result. This does not restore
private continuations, reuse unchecked results inside Solve, or persist fuel
history. Those integrations remain open; loading itself stays descriptive.

## Plan

- [x] Add descriptor-aware occurrence/scoped occurrence adapters.
- [x] Share identical object references in the transport dependency collector.
- [x] Verify inert nominal/scoped cycles and existing focused transport tests.
- [x] Verify source/derivation and scoped transport, and focused sanitizers.
- [x] Prototype materialized source roots, inert views and shared relocation.
- [x] Prototype archive retirement, exact lexical-origin reuse and sparse typed payloads.
- [x] Remove backward compatibility and migration tests; verify old-header rejection.
- [x] Run history/transport gates and focused semantic/source sanitizers.
- [x] Add inert section metrics and address-independent effect-definition order.
- [x] Run full acceptance on the combined candidate, including all ordering fixes.
- [x] Historical byte-bound trial: five sorting backends and focused checks pass;
  **policy rejected by user**, not approved for promotion.
- [x] Replace it with explicit fixed limits, localize file I/O, and add threshold,
  publication, pipe, large-image and retention-attribution tests.
- [x] Check retained typed roots through existing rules without source rebuilding;
  exercise a read-only function consumer next to an unfinished invalid sibling.
- [x] Repeat full acceptance after the byte-derived reader change.
- [x] Remove duplicate intrinsic initialization on import; repeat full acceptance,
  five sorting backends and focused sanitizers.
- [ ] Complete semantic roots, residual inputs and progress integration in AP1-AP3.
- [ ] Run full acceptance/F3/F4, then promote approved pieces and register gates.

```sh
bash src/prototype/artifact_persistence/overlay.sh /tmp/a-program-artifact-persistence
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-transport
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-semantic
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-normalization-checkpoint
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-definition-checkpoint
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-history
```

Use `candidate.sh NEW_DIRECTORY` instead of `overlay.sh` to include readback
sharing and Fold comparison. `check-artifact-metrics` runs the history gate and
checks its section counts; `check-artifact-sorting` runs the separate large
sorting gates with `SORTING_IMAGE_LIMIT` (fixed default 10,000,000 for these
large fixtures). Their allowance is independent of the compiler default and is
not a claim that artifact compaction is complete.

The generated overlay must be new. It borrows unchanged files and applies the
tracked patches to copies; tests are copied and patched as well. Only prototype
files are edited during this phase. `HISTORY_REPORT` must not exist before a run.

The normalization checkpoint gate exercises the owner-local WHNF attachment at
300 intermediate boundaries, using the existing machine codec, input checks and
Solve path. It also restores shared WHNF consumers with ready/wait order and
preparation wakeups through `artifact/schedule.c`. Its inert wire records do not
import job status, evidence or private owner state. Attachment requires a closed
owner-supplied mapping and known-origin continuations; decoding alone does not
authorize attachment. Default source import does not use these APIs. NF and
most source-owner continuations remain unfinished; this is not a whole-source checkpoint gate.

The definition checkpoint gate covers pending registration: exact entry links,
name indexing and activation position. The owner rebuilds its disposable hash
index without replaying registration dispatches or importing acceptance. Fresh
definition bodies receive their lexical scope and activation gate; their later
private continuation, completed registration and whole-module checking position
are not restored by this hook. The enclosing owner must establish provenance,
restore other child owners and attach the saved schedule before advancing Solve.
The test-only envelope uses existing syntax and schedule codecs; it is not a new
public `.a` format. It checks 44 real source frontiers (including aliases, shared
syntax, ADT/function use, invalid siblings/checks and dormant cycles), destroys
the original Program, then compares remaining dispatches and final source-image
bytes. A 64-definition registration also checks 1+19/10+10/20+0 against 20.
Repeated import recipe sharing and duplicate-assignment rejection have separate
owner-link tests. Default source-image loading still does not use this hook.

The strict fuel-partition gate is separate from the currently passing transport
tests. It fails until saved progress survives reload, including cases where the
files have identical bytes but different completion states:

```sh
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build PARTITION_REPORT=/tmp/artifact-fuel-partitions check-artifact-partitions
```

`PARTITION_REPORT` must not exist; results remain there after a failing gate.
The target requires size, status, consumed-fuel and exact byte agreement, including
10+10 versus 20 and zero-fuel partitions. Default runs additionally find a terminal
boundary within `IMAGE_AUDIT_COMPLETION_BUDGET` (default 100000), then test
terminal+0 and 0+terminal. Byte/size, local status and fuel verdicts are separate:
the current 2942+0 case preserves bytes and cumulative fuel but correctly remains
locally unaccepted after import, so the strict gate fails on status. This differs
from losing materialized progress. Structural root/frontier comparison is still
pending; passing this diagnostic alone will not complete AP3.1b.

Candidate implementation delta (patch contents, excluding patch context):

| Owner | Added | Removed |
| --- | ---: | ---: |
| `classifier.c` | 51 | 13 |
| `classifier.h` | 2 | 0 |
| `computation.c` | 9 | 4 |
| `computation.h` | 2 | 2 |
| `computation_io.h` | 0 | 9 |
| `dag.c` | 9 | 2 |
| `dag.h` | 4 | 0 |
| `effect_inference.c` | 14 | 11 |
| `effect_inference.h` | 4 | 1 |
| `graph_io.c` | 12 | 0 |
| `main.c` | 17 | 22 |
| `occurrence_io.c` | 67 | 23 |
| `occurrence_io.h` | 20 | 4 |
| `program.c` | 11 | 1 |
| `program.h` | 4 | 4 |
| `reduction_io.c` | 0 | 47 |
| `retained_io.c` | 95 | 7 |
| `retained_io.h` | 15 | 0 |
| `source_io.c` | 112 | 24 |
| `source_io.h` | 19 | 9 |
| `synthesis.h` | 10 | 0 |
| `synthesis_work.c` | 28 | 0 |
| `synthesis_work.h` | 2 | 0 |
| Total | 507 | 183 |

Net +324 implementation lines from the accepted baseline. This table excludes
the existing readback/Fold patches. Test patches add 249 and remove 369 lines (net -120)
by removing duplicate ordinary/archive runs and testing the new semantic path.
The standalone prototype tests, overlay/build files and docs
are separate. `git apply --numstat *.patch test_patches/*.patch` reports each
patched file without counting diff context as implementation.
