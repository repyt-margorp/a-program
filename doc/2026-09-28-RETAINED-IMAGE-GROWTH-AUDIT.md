# Image Persistence Growth Audit

Date: 2026-09-28
Status: measured baseline and prototype test; implementation planning moved to
[AP1-AP3](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md).
Production repair not started.
Planning baseline: `e716232` on Main; unrelated uncommitted implementation edits
are excluded. Earlier measurements below retain their original baseline.

## Problem List

1. **Primary:** remove unused reduction-history persistence from the source/CLI
   path; it accumulates data without providing Solve resumption.
2. **Separate follow-up:** investigate excessive contextual Term construction
   without conflating required binder freshness with redundant computation.
3. **Secondary physical cleanup:** eliminate duplicate Ref transport wrappers.
4. **Deferred:** compact verbose syntax metadata only after ownership is corrected.

## 1. Unused Persistence

### Subjective (User)

User paraphrase, 2026-09-28: inspect unnecessary duplication and data growth
before accepting a larger image-reader allowance. Readback promotion was
approved after regression; the reader-limit change was not approved.

Latest user paraphrase, same date: question whether these records should be
saved at all, and why so much data is kept for Solve resumption. The user then
requested a revised problem definition and Markdown plan. The removal scope
below is the agent's proposal, not approval to delete arbitrary proofs or change
image compatibility. No production implementation change is part of this update.

Further user requirement, 2026-09-28: quantify image growth against supplied
fuel, and always test `--steps 0`. This authorizes adding a prototype measurement
test; no production persistence change has been implemented.

### Objective (Code)

Baseline: `b85388caeb76dbbb6b2bf61a67150f4aafd93222`, clean detached worktree;
unrelated edits in the primary worktree are excluded. Large-case measurements
use that revision's readback, Fold-comparison and image-limit prototype overlay,
not the accepted compiler. No production changes are made by this audit.

The 102,817-byte input is `finite_sorting/provider.sh` applied to `quick.p`,
`tests/fixtures/generic_sorted/boolean-order.p` and `cases.p`. It contains the
Quick/Insertion proof library and observations, not just the sorting algorithm.
The diagnostic wraps existing codecs and reads their output; it does not
implement another serializer or invoke additional Solve during saving.

| State | Solve steps | RECOMPUTE bytes | Retained bytes | Saved Core Terms |
| --- | ---: | ---: | ---: | ---: |
| Parsed | 0 | 3,027,135 | 3,027,175 | 19 |
| Partial | 100,000 | 3,065,779 | 3,342,566 | 4,917 |
| Partial | 1,000,000 | 3,065,779 | 9,965,899 | 238,598 |
| Source Solve done | 5,811,051 | 3,065,779 | 20,893,258 | 923,298 |
| Load, Solve done, save | 5,943,393 | 3,065,779 | 37,551,302 | 1,817,582 |
| Repeat load/Solve/save | 6,059,972 | 3,065,779 | 54,229,765 | 2,712,340 |

Steps on loaded rows are per invocation, not cumulative. All three completed
large rows report `PG_SYNTHESIS_DONE`. The small accepted-build control,
`examples/09_list_induction.p`, also grows: 32,625 -> 33,069 bytes after one
load/Solve/save. Thus growth is not exclusive to the new compiler prototypes.
An accepted-build large run remained pending at 10M steps (29,498,845 retained
bytes); do not compare it to the completed prototype as equal progress.

`src/source_io.h:63` explicitly documents raw retention without installation.
`src/source_io.c:924` restores records to `program->retained_reductions`;
`src/source_io.c:990` schedules ordinary derivation synthesis. CLI/Solve never
consumes this reduction archive as a reusable cache. The reduction-check API
exists separately, but is not called on this CLI path.

`src/reduction_io.c:19` keeps previous roots unless an exact local request
matches; lines 30-45 then add the evaluator's completed requests and NF progress.
Fresh binding identities during reconstruction give distinct request keys.
There were no duplicate `(policy, source pointer, reduction kind)` root keys,
yet retained history grew. Pointer deduplication cannot remove that history.

At first completion, 11,808 of 14,818 reduction roots have identical source and
target. They record WHNF/normality, not failed computation. A diagnostic-only
snapshot omitting these roots shrinks to **8,850,456 bytes**; ordinary loaded
Solve still completes (5,917,118 steps). This does not establish general cache
equivalence or justify silently discarding records. Even this filtered trial
grows to 13,898,249 and 18,950,495 bytes in the later generations.

The first completed graph has 1,188,346 objects + Terms + root references,
before descriptor payload accounting: it already exceeds the default 1M bound.
The third has 3,478,345 and actually fails a 3M load. Raising the bound only
postpones the problem. These are record allowances, not bytes or Solve fuel.

Zero-step rewriting does not increase measured size. The first completed
image rewrites byte-identically; after one load/Solve/save, its next inert
rewrite has the same size but differs in bytes. Deterministic re-encoding at
later generations is therefore an additional open test, not a passed claim.

Fresh quantitative test, 2026-09-28: `fuel_curve.sh` tested a clean `e716232`
compiler, rebuilt with `-O2 -Wall -Wextra -Werror`, separately from the dirty
working tree. Fuel samples were 0, 1, 100 and 100000; each source state and each
of three completed generations received three consecutive zero-step rewrites.

| Input / mode | Bytes after source completion -> three generations | Result |
| --- | --- | --- |
| Example 09, ordinary | 26,374 -> 26,374 -> 26,374 -> 26,374 | Pass |
| Example 09, retained | 32,625 -> 33,069 -> 33,513 -> 33,957 | **Fail: +444 bytes per generation** |
| `image_audit/host.p`, ordinary / retained | 10,000 / 10,395, unchanged | Pass; no host marker printed |
| `image_limit/invalid.p`, ordinary / retained | 10,883 / 11,850, unchanged | Pass; explicit expected rejection preserved |

All zero-step rows in these small matrices report zero steps and preserve bytes;
this does not reproduce or invalidate the large-case inert drift above. Example
09 uses 2,824 source Solve steps and 3,120 ordinary / 3,136 retained loaded steps.
The retained test exits nonzero rather than accepting growth as its baseline.
The separate large combined-prototype ordinary matrix uses fuel 0, 1, 100 and
10M: completion is 5,811,051 source / 5,811,043 loaded steps, 3,065,779 bytes in
all completed generations, with every zero-step rewrite byte-identical.
The same large matrix in retained mode stops at its first completed-image
zero-step load: exit 2, default reader bound exceeded. It is a failed test, not
a successful inert rewrite or evidence of a zero-fuel Solve. No bound is raised
by the test. The earlier large same-size drift measurement used the separately
documented diagnostic reader allowance.
Logs, images and input/binary digests are under `/tmp/a-program-fuel-*`.
These are targeted measurements, not a fresh full compiler regression.

### Assessment

This is not an infinite evaluator loop or a demonstrated soundness failure.
It is a mismatch between raw-history retention and a useful resumable cache.
The earlier description "records prevent recomputation" was too strong for
the current CLI: records are kept, but are not installed for that purpose.
Their endpoints also retain lexical origins, derivations and entire syntax
subgraphs, so a small receipt can keep a large graph alive.

The initial R1-R6 proposal removed unused history and wrote ordinary APGSRC62.
After the user's #44/#45 follow-up, that is insufficient as the whole repair:
ordinary reconstruction alone does not preserve all materialized semantic exports.

The [Artifact Semantic Persistence Refactor](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md)
is now the single active implementation plan. It first specifies reachable
semantic content, typed structure and residual inputs, then removes unused
history without deleting necessary witnesses or live memoization. C-specific
data stays downstream; C emission and #43 recursion remain out of scope.

### Plan

- [x] Reproduce growth and distinguish it from same-size inert byte drift.
- [x] Establish the raw archive's ownership and absence of a CLI cache consumer.
- [x] Add the prototype fuel/image test, including repeated `--steps 0`.
- [x] Review #44/#45 and supersede the earlier removal-only R1-R6 ordering.
- [ ] Complete AP1-AP3 in the linked plan before claiming the artifact repaired.

The quantitative baseline above remains valid. Future completion additionally
requires preserving already materialized results without further synthesis merely
to inspect them. The existing CLI test alone does not establish that property.

## 2. Term Construction

### Subjective (User)

Inspect the cause of growth, not only the limit that reports it.

### Objective (Code)

The completed prototype contains 5,593,254 interned Terms in the main graph.
Externally invoked `pg_substitution_advance` accounts for 4,597,048 new Terms
in that graph. This is an allocation counter, not retained byte attribution.
The saved retained table contains 923,298 Terms, so the serializer is not
dumping the entire arena. `src/typing.c:765` substitutes the occurrence Core,
classifier and annotation; `src/eval.c:300` freshens a Lambda binder and then
rebuilds its body under a nonempty environment.

### Assessment

Repeated contextual reconstruction is a major construction-cost candidate.
It is not valid to call every such Term redundant: distinct scoped binders are
intentional, and the earlier closed-term substitution shortcut broke reindexing.
The approved readback optimization only reuses closed evaluator materialization.

### Plan

- [x] Separate main-graph allocation growth from exported reachability.
- [ ] Profile typed owner/context/substitution request keys and support overlap;
  identify avoidable re-traversals while preserving freshness and capture rules.
- [ ] Require Context reindex, dependent typing and image tests for any change;
  do not substitute alpha/WHNF-based interning for exact pointer structure.

## 3. Ref Wrapper Duplication

### Subjective (User)

Check whether unnecessary duplicate data is actually serialized.

### Objective (Code)

The first completed retained table has **4,772 identical Ref wire records**,
but no identical Lambda/Application records with the same literal child IDs.
This is about 43 KB of direct Ref-record bytes, not the 17.8 MB increase over
RECOMPUTE. It does not measure transitive structural or alpha equivalence.

`src/graph_io.c:19` replaces direct Ref roots with scratch-owned wrappers while
nested Lambda/Application children remain borrowed. A borrowed Ref child and a
new wrapper can therefore denote the same object but get different wire IDs.
The diagnostic self-test reproduces this with `APP(r,r)` and direct root `r`:
three records are written for two unique Terms; reading restores shared pointers.

### Assessment

This is real physical redundancy, not lost semantic sharing after loading.
It is worth fixing, but cannot explain the primary size increase.

### Plan

- [x] Count literal wire duplicates and construct a two-root reproduction.
- [ ] Track Ref-key cleanup and its accepted regression under AP2.2/AP3 of the
  linked plan; preserve scratch lifetime, nominal identity and binder distinctions.

## 4. Syntax Size

### Subjective (User)

Explain why a small source has a large image.

### Objective (Code)

The completed RECOMPUTE image is 3,065,779 bytes; syntax is 3,018,136 bytes for
26,973 nodes. `src/syntax_io.c:144` writes seven 64-bit token fields, and each
node also writes its tag, marker, links and item count. The syntax DAG is shared;
this is chiefly a verbose wire representation, not copying the whole AST per use.

### Assessment

Compact metadata can reduce the baseline, but will not fix generation growth.
Treat it as lower-priority format work, not a new inference mechanism.

### Plan

- [x] Measure the syntax contribution separately from Core and reduction data.
- [ ] After the retention contract is fixed, consider compact integer/token
  encoding with explicit versioning and existing roundtrip/diagnostic tests.

## Reproduction

Use `src/prototype/image_audit/build.mk` and its README. The diagnostic is
optional, GNU-linker-based, and not registered in the production regression
suite. Data/logs live under `/tmp/a-program-image-audit-*`; numeric results above
are the durable summary. No `.a` outputs or generated source copies are committed.

Fresh verification: diagnostic O2 build and large multi-generation runs passed;
the Ref reproduction and small source/load/save runs passed ASan/UBSan with
leak checking. Filtered large-image Solve passed only in O2. This audit does not
claim a fresh full compiler regression or promotion of any compiler prototype.
