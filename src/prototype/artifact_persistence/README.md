# Artifact Persistence Prototype

## Problem List

1. Typed occurrences cannot use the existing nominal descriptor codec.
2. Temporary Ref wrappers create duplicate transport records for one object.
3. Source images omit materialized results; continuation/validation reuse is missing.

## Subjective (User)

2026-09-28, paraphrase: implement the
[semantic persistence plan](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md),
preserve computational/typed separation, remove redundant retained history and
test zero fuel and equivalent budget partitions. Do not start C transpilation.

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

Fresh O2 focused tests pass on the working tree and the clean baseline:
same Core/different classifiers, distinct nominal declarations, shared contexts,
scope formation, descriptor/Core aliases and three byte-identical inert cycles.
The focused test links evaluation/substitution entry points to aborting stubs;
loading/writing must not call them. Loaded proof/query/action stores remain empty.
Missing descriptor support and insufficient bounds reject without publishing
output roots or counts, through both scoped and unscoped descriptor entry points.
Existing graph, declaration and occurrence transport tests also pass. This is
not a complete source-image persistence or checkpoint implementation.
Clean-baseline source/derivation and typed/scoped structure suites also pass;
the new transport test passes ASan/UBSan. The budget-partition test still fails
at 100+100 and 1600+1600 across reload, as expected before progress integration.
On completed Example 09, the retained image is 32,463 bytes with zero duplicate
Ref records (prior baseline: 32,625 bytes); ordinary output remains 26,374 bytes.
This removes duplicate encoding, not the unused archive or its generation growth.

The next prototype writes APGSRC64/APGRET3 with available typed results connected
to existing producer ordinals. Its view borrows `pg_occurrence`; there is no new
type graph or accepted-result DB. Import preserves descriptive input only, and
ordinary Solve still checks source/annotations. Selected producer dependencies
are roots; allocation-only source origins are not additional semantic exports.
The ordinary Example 09 image is 49,830 bytes, including typed structure, and
remains byte-identical through three inert and three completed generations.
Source/derivation suites and focused ASan/UBSan pass on the clean baseline.
The new test also checks forged inputs and whole-module acceptance. Split-fuel
resumption remains failing; private continuations are not yet transported.

## Assessment

Both changes belong to existing transport owners. A second serialized type graph
or C-specific representation would duplicate their work. Reference wrapper
sharing is structural identity, not observational equality. The wire grammar is
unchanged, but canonical emitted bytes can change: final Program migration still
needs the explicit version decision in AP1.4 before production adoption.

Do not fix fuel-partition failures by importing trusted status bits or by silently
charging zero for recomputation. Necessary unfinished computation and its validation
must be represented by their existing owners. The active plan tracks this work.

## Plan

- [x] Add descriptor-aware occurrence/scoped occurrence adapters.
- [x] Share identical object references in the transport dependency collector.
- [x] Verify inert nominal/scoped cycles and existing focused transport tests.
- [x] Verify source/derivation and scoped transport, and focused sanitizers.
- [x] Prototype materialized source roots, inert views and shared relocation.
- [ ] Complete semantic roots, residual inputs and progress integration in AP1-AP3.
- [ ] Run full acceptance/F3/F4, then promote approved pieces and register gates.

```sh
bash src/prototype/artifact_persistence/overlay.sh /tmp/a-program-artifact-persistence
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-transport
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build check-artifact-semantic
```

The generated overlay must be new. It borrows unchanged files and applies the
tracked patches to copies; only prototype files are edited during this phase.

The strict fuel-partition gate is separate from the currently passing transport
tests. It fails until saved progress survives reload, including cases where the
files have identical bytes but different completion states:

```sh
make -f src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/a-program-artifact-persistence BUILD=/tmp/a-program-artifact-persistence/build PARTITION_REPORT=/tmp/artifact-fuel-partitions check-artifact-partitions
```

`PARTITION_REPORT` must not exist; results remain there after a failing gate.
The target requires size, status, consumed-fuel and exact byte agreement, including
10+10 versus 20 and zero-fuel partitions. Structural root/frontier comparison is
still pending; passing this diagnostic alone will not complete AP3.1b.

Candidate implementation delta (patch contents, excluding patch context):

| Owner | Added | Removed |
| --- | ---: | ---: |
| `dag.c` | 9 | 2 |
| `dag.h` | 4 | 0 |
| `graph_io.c` | 12 | 0 |
| `occurrence_io.c` | 39 | 8 |
| `occurrence_io.h` | 14 | 0 |
| `retained_io.c` | 94 | 4 |
| `retained_io.h` | 15 | 0 |
| `source_io.c` | 26 | 6 |
| `source_io.h` | 10 | 3 |
| `synthesis.h` | 10 | 0 |
| `synthesis_work.c` | 28 | 0 |
| `synthesis_work.h` | 2 | 0 |
| Total | 263 | 23 |

Net +240 implementation lines; focused tests are separate. Overlay/build
scripts (including the mechanical link additions) and documentation are separate.
This enables shared transport and descriptive materialized roots; the
larger retention deletion is not yet done and is not counted as a reduction.
