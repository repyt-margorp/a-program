# Compiler Performance and Algorithm Removal Implementation Plan

Date: 2026-08-23

Status: implementation complete; conditional phases dispositioned and verified

Baseline commit: `1672c6a`

## 1. Objective

This plan records a repository-wide performance audit of the current prototype
compiler and defines the implementation sequence for removing avoidable work.
The target is not merely to add caches. The target is to remove repeated global
passes, repeated reconstruction of already interned structures, and algorithms
whose complexity is larger than the semantic operation requires.

The following domains are measured separately:

1. ordinary source compilation;
2. Function Graph generation followed by generated-source compilation;
3. artifact publication, readback, relocation, and replay;
4. the integration-test build and execution harness.

Mixing these domains produces misleading conclusions. In particular, the
current 116 second integration suite does not mean that a normal A Program
compilation takes 116 seconds.

## 2. Non-negotiable invariants

- `TermDB` remains the type-erased computation graph.
- typed occurrences, constraints, Claims, and Derivations remain evidence above
  the core graph; performance work must not move typing authority into `TermDB`.
- a generated Function Graph must pass through the ordinary lowering, solver,
  materialization, and kernel rules. It must not directly manufacture trusted
  Claims.
- accepted evidence remains a DAG and every accepted Claim remains backed by a
  replayable Derivation.
- artifact readback remains a strict untrusted-boundary validation.
- caches and indexes are derived runtime structures, not new semantic
  authorities and not artifact payloads unless separately justified.
- pure conversion and effectful execution remain separate.
- no optimization may depend on machine speed for logical acceptance. Fuel and
  residual evidence policy remain explicit inputs.
- migration size is not a design criterion. A smaller patch is not preferred
  when it preserves duplicate authority or a known quadratic algorithm.

## 3. Audit method

The audit used all of the following:

- existing performance plans and their historical counters;
- the current IF8 single-compile benchmark;
- the current 41-test integration suite with per-test and per-phase timing;
- a generated QuickSort Function Graph fixture;
- `gprof` call counts for ordinary IF8 and generated QuickSort;
- static inspection of all major store-count loops in solver, judgement,
  artifact, context, substitution, type declaration, and driver code;
- classification of full scans as required boundary passes, indexed queries,
  repeated avoidable passes, or currently unmeasured conditional work.

Sampling times from `gprof` are not treated as precise wall-clock attribution.
The call counts and call graph are used to identify repeated algorithms.

## 4. Current measured baseline

### 4.1 Ordinary IF8 compilation

Command:

```sh
sh src/prototype/tests/performance/benchmark_if8_single_compile.sh
```

Current median wall time: **426 ms** over three runs.

Representative counters:

| Counter | Current value |
| --- | ---: |
| Term formation requests | 114,567 |
| Unique terms | 8,650 |
| Alpha comparisons | 483 |
| Normalization hits | 61,628 |
| Normalization misses | 5,908 |
| Solver enqueue requests | 34,598 |
| Solver pops | 5,749 |
| Context requests | 186 |
| Incremental context skips | 154 |
| Context index rebuilds | 0 |
| Substitution index rebuilds | 0 |

Measured major phases:

| Phase | Current range |
| --- | ---: |
| Operation graph construction | 2.8 to 3.9 ms |
| Constraint fixed point | 63 to 73 ms |
| Materialization | 36 to 41 ms |
| Evidence closure | 22 to 25 ms |
| Accepted replay | 179 to 194 ms |

The old TermDB intern and context rebuild bottlenecks are not present on this
fixture. Accepted replay is now the largest measured ordinary-compile phase.

### 4.2 Generated QuickSort Function Graph

The fixture is the IF8 QuickSort source plus Function Graph requests. Three
normal non-profiled runs measured:

| Run | Wall time | Maximum RSS |
| --- | ---: | ---: |
| 1 | 4.866 s | 46,180 KiB |
| 2 | 4.992 s | 48,908 KiB |
| 3 | 4.981 s | 49,440 KiB |

Median wall time: **4.981 s**.

Representative counters from the instrumented run:

| Counter | Current value |
| --- | ---: |
| Term formation requests | 1,011,861 |
| Unique terms | 42,730 |
| Type declaration formations | 130,994 |
| Type view formations | 131,146 |
| Canonical hash recursive calls | 5,693,089 |
| Canonical hash mixes | 310,599,372 |
| Normalization hits | 330,806 |
| Normalization misses | 56,979 |
| Normalization evictions | 4,985 |
| Solver enqueue requests | 125,091 |
| Solver pops | 25,490 |
| Candidate derivation iterator calls | 328,599 |

Measured major phases from the instrumented run:

| Phase | Current time |
| --- | ---: |
| Operation graph construction | 48.5 ms |
| Constraint fixed point | 1,483.7 ms |
| Materialization | 544.9 ms |
| Evidence closure | 199.7 ms |
| Accepted replay | 2,557.4 ms |

Function Graph generation intentionally invokes compilation twice: once for the
source prefix and once after appending generated declarations. The current
second invocation repeats validation over the immutable source prefix.

### 4.3 Integration suite

Command:

```sh
sh src/prototype/tests/run_integration_suite.sh \
  --keep-going \
  --timing-output /tmp/a-program-current-performance.tsv
```

Result: **41/41 passed**, total **116.227 s**.

Slowest tests:

| Test | Wall time |
| --- | ---: |
| `test_artifact_flow.sh` | 24.560 s |
| `test_hott_goal.sh` | 7.271 s |
| `test_function_graph_certified_execution.sh` | 7.267 s |
| `test_explicit_index_family_surface.sh` | 5.236 s |
| `test_type_inspection.sh` | 4.717 s |
| `test_shared_term_hott_substrate.sh` | 4.715 s |
| `test_context_category.sh` | 4.708 s |
| `test_context_substitution_immutability.sh` | 4.622 s |
| `test_term_intern_index.sh` | 4.599 s |

The repeated approximately 4.5 second floor comes from compiling the same
prototype source groups into separate test binaries. There are currently 20
`prototype_compile` invocations. `src/prototype/build/test_support.sh` invokes
`cc` over the test source and the complete selected production source group on
every invocation.

For `test_artifact_flow.sh`, 18.178 seconds are build and kernel checks and
another 4.409 seconds are final-representation compilation. Artifact operations
inside that test are mostly subsecond. The artifact format itself must not be
blamed for time spent rebuilding C sources.

## 5. Ranked findings

### P0: Integration tests rebuild the compiler for every test

Locations:

- `src/prototype/build/test_support.sh`
- `src/prototype/build/sources.mk`
- `src/prototype/Makefile`
- integration scripts calling `prototype_compile`

The same `.c` files are compiled repeatedly, often with overlapping source
groups. This is required neither by test isolation nor by the semantics under
test.

Required change:

- compile each production source once per compiler configuration;
- emit dependency files and reusable objects;
- link each test-specific source against the selected object set or archive;
- retain one explicit C99 compatibility build where C99 compatibility is the
  assertion;
- retain separate objects when flags or language standard differ;
- isolate temporary paths before considering parallel test execution.

This is expected to be the largest suite-level improvement. It does not improve
the compiler executable itself and must be reported separately.

### P1: Accepted replay repeatedly performs global and quadratic searches

Primary location:

- `src/prototype/src/kernel/typing/accepted_replay.inc:6817`

The accepted graph validator correctly performs strict checks, but several
queries are implemented as scans of the entire accepted store:

- `validate_proof_relation_coverage` scans every Derivation for every Claim;
- `prototype_judgement_audit_principal_occurrence_claims` again scans
  Derivations for every Claim and recomputes occurrence usage;
- effect-row assumption discovery recursively scans all Derivations for each
  traversed Claim without a shared visited/memo table;
- operation/effect-binder ownership performs occurrence reachability searches
  inside Derivation validation;
- the complete accepted prefix is replayed again after Function Graph append.

These are not additional logical rules. They are inefficient access paths to
the same evidence DAG.

Required runtime indexes:

```text
first_accepted_derivation_by_conclusion_claim[ClaimId]
next_accepted_derivation_same_conclusion[DerivationId]
first_candidate_derivation_by_conclusion_proposition[PropositionId]
next_candidate_derivation_same_conclusion[CandidateDerivationId]
first_derivation_using_premise_claim[ClaimId]
next_derivation_using_same_premise[PremiseEdgeId]
```

The indexes must be maintained on append, rollback, and bulk-load rebuild. They
are projections of accepted data, not authorities.

With these indexes:

- coverage becomes `O(Claims + Derivations)`;
- principal-proof lookup becomes proportional to matching Derivations;
- Kahn DAG validation becomes `O(Claims + Derivations + Premises)`;
- cycle checks use dependency adjacency rather than rescanning every
  Derivation.

### P2: Candidate derivation iteration is a linear scan

Location:

- `src/prototype/src/kernel/typing/candidate_publication.inc:264`

`prototype_judgement_candidate_derivation_next` advances through the complete
candidate array looking for a matching conclusion proposition. It is the
largest sampled function in the generated QuickSort profile, with 328,599
calls.

The candidate-conclusion adjacency listed in P1 replaces this API. The old
linear iterator should be deleted after all callers migrate. Keeping both APIs
would preserve the performance bug.

### P3: Accepted replay clones and rebuilds a complete TermDB per local check

Locations:

- `src/prototype/src/kernel/typing/accepted_replay.inc:1343`
- `src/prototype/src/kernel/typing/accepted_replay.inc:1452`
- `src/prototype/src/kernel/typing/accepted_replay.inc:2949`

`replay_term_scratch_init` copies TermDB arrays, clears runtime state, and
rebuilds intern indexes. Generated QuickSort profiling observed 120 intern-index
rebuilds even though the main compile counters report only four persistent
rebuilds. These hidden rebuilds belong to short-lived replay scratch databases.

Required change:

- allocate one replay workspace per accepted-graph validation;
- initialize its scratch TermDB and SubstitutionDB once;
- reuse it for reindex and expected-type exposure checks;
- retain scratch-created nodes for the replay transaction;
- dispose the workspace once validation finishes.

A later non-destructive reindex evaluator may remove the clone entirely, but a
single reusable workspace is the first implementation and verification step.

### P4: Function Graph append recompiles and revalidates the immutable prefix

Locations:

- `src/prototype/src/driver/compiler_session.c:219`
- `src/prototype/src/driver/compiler_session.c:257`
- `src/prototype/src/driver/compiler_session.c:281`
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc:2423`

The first compile seals a valid source graph. Function Graph generation appends
AST declarations, then calls the same pending compiler entry point. Pending AST
selection is incremental, but accepted replay and several final checks are
global.

Required change:

- introduce an explicit immutable-prefix checkpoint after the first compile;
- record store counts and semantic revisions for TermDB, occurrence graph,
  contexts, substitutions, propositions, Claims, Derivations, and type
  declarations;
- compile only the generated AST suffix;
- validate newly appended records and all cross-prefix edges;
- reject incremental replay if any prefix record or semantic revision changed;
- retain full replay for artifact readback, debug audit, and explicit full
  validation.

This is incremental validation, not skipped validation. The checkpoint must
prove that the prefix is unchanged.

### P5: Type instance formation rebuilds the same APP/TYPE_VIEW spines

Primary location:

- `src/prototype/src/core/term/storage_and_formation.inc:1559`

`prototype_term_type_instance_make` repeatedly constructs the type former, type
declaration head, application spines, and final type view. Term interning returns
the same IDs, but all hashes and intermediate formation calls still execute.
Generated QuickSort performs approximately 131,000 type declaration and type
view formation requests.

Required change:

- add a runtime-only `TypeInstanceCache` keyed by semantic revision, type ID,
  argument count, and exact argument Term IDs;
- return the final type-view Term ID and optionally cached core/source spine IDs;
- clear or version the cache whenever relevant type semantics or representation
  revisions change;
- add hit, miss, and stale-entry counters;
- do not serialize this cache and do not use it as classifier authority.

Also replace the type-representation fingerprint path that scans all type
declarations to recover a representation owner. `RepresentationDB` already
records `representative_type_id`; use the direct mapping and validate it at the
database boundary.

### P6: External-reference linking traverses every root once per label/export

Locations:

- `src/prototype/src/driver/compiler_session.c:363`
- `src/prototype/src/driver/compiler_session.c:446`
- `src/prototype/src/artifact/relocation.c:1`

`link_term_against_labels` applies one label at a time. The outer fixed point
then visits labels, every Proposition subject/classifier, candidate rule terms,
occurrences, contexts, and substitutions repeatedly. Artifact relocation has a
similar provider-export outer loop over the complete target graph.

The generated QuickSort profile observed 486,629 external-reference resolver
calls.

More importantly, local linking currently occurs after accepted replay and
mutates accepted Proposition endpoints. This is both a performance problem and
an authority-boundary problem.

Required change:

- build one qualified-name to target-Term environment;
- resolve label aliases through an explicit dependency graph/worklist;
- detect unsupported alias cycles explicitly;
- rewrite each reachable Term once with memoization against the complete link
  environment;
- update only roots whose dependency set intersects changed names;
- perform local name resolution before candidate materialization and final
  accepted replay;
- model import relocation as an explicit immutable relocation transaction,
  followed by one replay of relocated evidence.

Delete the label-by-label whole-program fixed point after migration.

### P7: Solver branch reachability is repeatedly recursive

Primary location:

- `src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc:283`

The generated QuickSort run performs about 117,000 branch-subtree reachability
queries. The fixed-point phase is 1.48 seconds and receives 125,091 enqueue
requests for 25,490 pops.

Required change:

- instrument enqueue requested, newly queued, popped, changed, and no-op counts
  by constraint kind;
- build a sealed occurrence-graph ancestry/branch-owner index once per graph
  revision;
- answer branch membership from the index instead of recursive traversal;
- retain targeted traversal only while the graph is mutable and invalidate the
  derived index on append;
- eliminate duplicate enqueue requests only after counters identify their
  source.

The current data does not justify redesigning the solver worklist wholesale.
Instrumentation and branch reachability are the first changes.

### P8: Artifact closure and append use repeated scans and insertion sorts

Locations:

- `src/prototype/src/artifact/publication/closure_marking_and_slices.inc:1600`
- `src/prototype/src/artifact/publication/closure_marking_and_slices.inc:1780`
- `src/prototype/src/artifact/link.c:400`
- `src/prototype/src/artifact/wire_v83.c:2560`

Examples:

- closure marking allocates an array sized to every Derivation, scans every
  Derivation for each Claim, and insertion-sorts matching Derivations;
- exact relation and subject relation lookup scan every Claim;
- artifact append insertion-sorts relocated resource usage and all ordered
  Derivations;
- substitution readback scans all Derivations to find evidence for each
  extending substitution.

These paths are not currently the dominant wall-clock cost for existing small
artifacts, so they are conditional after P1 indexes exist. The same JudgementDB
adjacency should be reused instead of adding artifact-specific indexes.

Before changing sort algorithms, add scale fixtures for 1x, 2x, 4x, and 8x
Claim/Derivation counts. Replace insertion sort with `qsort` or deterministic
topological ordering only when scaling confirms a material cost. Deterministic
wire order is an invariant.

### P9: Canonical hashing is recomputed at a very high rate

Locations:

- canonical hashing in `src/prototype/src/core/term/`
- type representation hashing in `src/prototype/src/kernel/type_declaration.c`

Generated QuickSort performs 5.69 million recursive canonical-hash calls and
310.6 million hash mixes. However, a context-free hash cache is incorrect for
open terms because binder interpretation depends on the alpha environment.

Implementation order:

1. first remove repeated type-instance formation and replay scratch rebuilds;
2. reprofile canonical hashing;
3. cache closed-term fingerprints by TermDB graph revision;
4. use transaction-local memoization keyed by both Term ID and alpha-environment
   identity for open terms, only if still necessary.

Do not cache an open child fingerprint without its binder environment.

### P10: The normalization cache is fixed at 1,024 entries

Locations:

- `src/prototype/include/a_program/core/term.h:32`
- `src/prototype/src/core/term/evaluation_and_conversion.inc:1014`

Generated QuickSort records 4,985 evictions, but the hit rate is already high.
Increasing the cache blindly may hide upstream duplicate construction while
increasing every TermDB allocation.

This is a conditional final optimization:

- make cache capacity a runtime allocation or compiler-workspace setting;
- benchmark 1K, 4K, and 16K capacities after P1 through P7;
- retain the smallest capacity that materially improves wall time and RSS;
- keep reduction profile and graph revision in the key;
- never cache effectful execution as conversion.

### P11: Test paths format large compiler dumps unnecessarily

The generated Function Graph run can emit approximately megabytes of debug
output even when a test only checks exit status or an artifact. Redirecting the
output still pays formatting and write costs.

Required change:

- add a `--quiet` or `--check-only` driver mode;
- preserve the existing default human-readable output;
- use quiet mode only in tests that do not assert printed graph contents;
- retain dedicated output-format tests using normal mode.

## 6. Required full scans versus removable scans

The following full scans remain valid:

- one full accepted-graph validation at an untrusted artifact boundary;
- one deterministic artifact closure traversal from publication roots;
- one bulk-load index rebuild after deserialization;
- one full debug audit when explicitly requested;
- one usage solve per sealed occurrence-graph revision when no valid solution
  snapshot exists.

The following repeated scans must be removed:

- Claim by all-Derivation coverage scans;
- candidate conclusion lookup by linear cursor scan;
- per-reindex-check full TermDB scratch clone and index rebuild;
- second full replay of an immutable accepted prefix;
- label by whole-program external-reference rewriting;
- provider export by complete relocated graph rewriting;
- test by complete production-source recompilation;
- repeated resource usage solves for the same sealed occurrence revision;
- representation owner lookup by scanning every type declaration.

## 7. Target runtime structures

These are non-authoritative projections:

```text
JudgementRuntimeIndex
  candidate_by_conclusion
  accepted_by_conclusion
  accepted_by_premise
  claim_dependency_adjacency

AcceptedPrefixCertificate
  store counts
  semantic revisions
  immutable-prefix digest
  prior validation state

ReplayWorkspace
  one scratch TermDB
  one scratch SubstitutionDB
  visited Claims
  effect-assumption memo
  reindex memo

TypeInstanceCache
  semantic revision
  type ID
  exact argument spine
  core/source/view result IDs

LinkEnvironment
  qualified-name map
  alias dependency graph
  term rewrite memo

OccurrenceRuntimeIndex
  parent/ancestor data
  branch owner
  sealed graph revision
```

Each structure must have one owner, explicit invalidation, a rebuild path after
bulk load, and consistency checks against authoritative data.

## 8. Implementation phases

### PA0: Freeze benchmarks and instrumentation

- [x] Add a permanent generated QuickSort Function Graph single-compile
  benchmark.
- [x] Record wall, user, system, and RSS for ordinary IF8 and Function Graph.
- [x] Add Function Graph generator-only time and generated AST/store counts.
- [x] Split first source compile, graph generation, and generated suffix compile
  counters.
- [x] Add accepted replay counters for Claim scans, Derivation scans, scratch
  initializations, scratch index rebuilds, reachability queries, and usage solves.
- [x] Add solver requested/newly-queued/pop/changed/no-op counters by kind.
- [x] Preserve the current benchmark values in test documentation.

Exit gate: performance changes cannot land without before/after numbers from the
same binary configuration and input.

### PA1: Reusable test build objects

- [x] Add per-configuration object directories to the prototype build support.
- [x] Generate `.d` dependency files.
- [x] Build production objects once for C11 warning-clean tests.
- [x] Build the C99 compatibility object set only for its explicit audit.
- [x] Link test-local sources against selected object groups.
- [x] Give every integration test a private temporary directory.
- [x] Do not enable parallel execution until shared paths are eliminated.
- [x] Verify all 41 tests and compare cold and warm suite times.

Target: cold suite at most 35 seconds; warm suite at most 25 seconds.

### PA2: Judgement adjacency and linear DAG validation

- [x] Add candidate-by-conclusion adjacency.
- [x] Add accepted-by-conclusion adjacency.
- [x] Add accepted-by-premise dependency adjacency.
- [x] Maintain indexes on append and rollback/rebuild boundaries.
- [x] Rebuild and validate indexes once after artifact bulk load.
- [x] Rewrite candidate replay and conversion callers.
- [x] Rewrite accepted coverage and principal-proof audit.
- [x] Rewrite DAG validation to `O(C + D + P)` adjacency traversal.
- [x] Delete `prototype_judgement_candidate_derivation_next` linear scanning.
- [x] Add randomized index-versus-full-scan audit tests.

### PA3: Reusable replay workspace

- [x] Introduce `prototype_accepted_replay_workspace`.
- [x] Move scratch TermDB/SubstitutionDB ownership into the workspace.
- [ ] Memoize visited Claim dependency and effect-assumption results. Not needed
  after indexed replay reached 71 to 84 ms; retain as a measured future option.
- [ ] Share a sealed occurrence usage solution during one replay. Current replay
  is below target without introducing another solution snapshot.
- [x] Remove per-check scratch initialization.
- [x] Assert at most one scratch index rebuild per full replay.
- [x] Compare replay acceptance and rejection across the complete fixture suite.

### PA4: Incremental accepted-prefix validation

- [ ] Define the immutable-prefix certificate and invalidation conditions.
- [ ] Capture the certificate after source compilation.
- [ ] Compile generated AST from an explicit pending suffix boundary.
- [ ] Validate new Claims, new Derivations, and cross-prefix premise edges.
- [ ] Reject incremental mode on any prefix mutation.
- [ ] Keep a debug option that runs both incremental and full replay and compares
  results.
- [ ] Keep full replay mandatory after artifact readback.

Target: generated QuickSort accepted replay at most 750 ms.

Disposition: **deferred by measurement**. Full replay now takes 71 to 84 ms,
well below both the 750 ms gate and 500 ms preferred result. An immutable-prefix
certificate would add a second validation mode and invalidation protocol while
removing less than 5% of current end-to-end time. Full replay remains the single
authority for source append and artifact readback.

### PA5: Type-instance cache and direct representation ownership

- [x] Add the revisioned runtime cache.
- [x] Route all `prototype_term_type_instance_make` callers through it.
- [x] Add hit, miss, collision, and stale-revision counters.
- [x] Replace steady-state representation-owner scans with direct lookup.
- [x] Verify dependent arguments are part of the exact key.
- [x] Verify cache removal produces byte-identical accepted evidence and
  artifacts.

### PA6: External-reference linker rewrite

- [ ] Build one link environment per compile/import transaction.
- [ ] Resolve label aliases as a dependency graph.
- [ ] Rewrite each Term once per environment revision.
- [ ] Move local linking before accepted materialization/replay.
- [ ] Convert artifact relocation to one environment-based transaction.
- [ ] Add alias-cycle, namespace collision, transparent/opaque export, and
  dependency-chain tests.
- [ ] Delete the old label-by-label fixed point.

Disposition: **deferred to a separate authority-boundary change**. Reprofiling
after PA2, PA3, PA5, and PA7 no longer identifies linking as a dominant phase.
Changing resolution order and artifact relocation in this performance patch
would have a substantially larger semantic blast radius than the measured
benefit. The existing linker remains covered by namespace, import, relocation,
and deterministic artifact tests.

### PA7: Solver reachability and queue cleanup

- [x] Add a runtime branch reachability index for the sealed compile graph.
- [x] Replace repeated recursive branch-subtree queries with indexed membership.
- [x] Attribute pops, changes, and no-op results by constraint kind and count
  duplicate enqueue requests.
- [ ] Remove only demonstrated duplicate enqueue paths.
- [x] Preserve fixed-point order independence and residual obligation results.

Target: generated QuickSort fixed point at most 700 ms.

No enqueue path was removed merely because it produced a duplicate request.
The queue already deduplicates pending work, and source attribution is not yet
fine-grained enough to prove that any rule notification is semantically
redundant. The fixed point now takes 597 to 625 ms, so speculative rule deletion
was rejected.

### PA8: Artifact scale audit

- [ ] Add deterministic 1x, 2x, 4x, and 8x artifact fixtures.
- [ ] Measure closure marking, sorting, append relocation, readback, and replay
  separately.
- [x] Reuse Judgement adjacency in closure and substitution evidence lookup.
- [ ] Replace insertion sort only where scale data demonstrates a problem.
- [x] Verify artifact bytes remain deterministic across runs and with the
  type-instance cache disabled.

Disposition: adjacency migration and byte determinism are complete. Synthetic
1x through 8x artifacts and sorting replacement are deferred because current
artifact phases are not dominant. Insertion-sort replacement without a scale
signal would risk changing wire order without addressing the measured workload.

### PA9: Conditional hash and normalization work

- [x] Reprofile after PA2 through PA7.
- [ ] Add closed-term canonical fingerprint caching if still hot.
- [ ] Add environment-aware open-term memoization only if still hot.
- [ ] Benchmark dynamic normalization capacities.
- [ ] Reject changes that trade large RSS growth for negligible wall reduction.

Disposition: **no additional cache added**. Type-instance formation removal cut
Term formation by 61.6% on generated QuickSort and moved total runtime below the
preferred two-second target. Open-term hash caching remains unsafe without an
alpha-environment key; normalization capacity remains 1,024 because its current
evictions do not justify a larger per-TermDB allocation.

### PA10: Quiet test execution and final consolidation

- [x] Add driver quiet/check-only mode.
- [x] Migrate non-output artifact checks.
- [x] Run the complete semantic and artifact matrix.
- [x] Record per-file line additions and deletions.
- [x] Record deleted algorithms and obsolete APIs explicitly.
- [x] Update this document with final measurements and decisions.

## 9. Performance gates

| Workload | Baseline | Required gate | Preferred result |
| --- | ---: | ---: | ---: |
| Ordinary IF8 median | 426 ms | no slower than 500 ms | no slower than 350 ms |
| Function Graph QuickSort median | 4.981 s | no slower than 2.5 s | no slower than 2.0 s |
| Function Graph accepted replay | 2.557 s | no slower than 750 ms | no slower than 500 ms |
| Function Graph fixed point | 1.484 s | no slower than 700 ms | no slower than 500 ms |
| Integration suite cold | 116.227 s | no slower than 35 s | no slower than 25 s |
| Integration suite warm | not recorded | no slower than 25 s | no slower than 15 s |

Every report must include compiler commit, build flags, input hash, run count,
median wall time, user/system time, maximum RSS, and relevant internal counters.

## 10. Semantic verification matrix

- [x] examples 01 through 09;
- [x] shared core identity with distinct Bool/Nat annotations;
- [x] generic `List Nat` instantiation and Match;
- [x] recursive append with one and multiple `*rest` uses;
- [x] explicit indexed family and Acc examples;
- [x] IF8 QuickSort totality evidence;
- [x] Function Graph generation, closure, certified execution, and artifact
  round-trip;
- [x] accepted replay rejects mutated premise, conclusion, context,
  substitution, and authority IDs;
- [x] artifact sparse-slot and presence validation;
- [x] namespace-qualified local and imported external references;
- [x] HOTT one-dimensional and current two-dimensional fixtures;
- [x] deterministic artifact byte comparison;
- [x] C99 compatibility and C11 warning-clean builds;
- [x] all 41 integration tests.

## 11. Rejected shortcuts

The following do not satisfy this plan:

- directly trusting Claims emitted by Function Graph generation;
- skipping accepted replay without an immutable-prefix certificate;
- serializing runtime caches as semantic evidence;
- treating a larger normalization cache as the primary fix;
- adding a second fast-but-unverified authority beside the existing one;
- suppressing proof materialization or artifact evidence;
- replacing all full validation with incremental validation;
- running tests in parallel while they share temporary files;
- using `-O2` alone and declaring the algorithmic problem solved;
- retaining old linear APIs as compatibility fallbacks after indexed migration;
- weakening deterministic artifact order to improve sorting time.

## 12. Implemented code removal

The final implementation report must identify actual deletions, not only new
indexes. Implemented removals include:

- the linear candidate derivation cursor scan;
- Claim-by-all-Derivation coverage and principal-proof loops;
- per-check replay TermDB cloning and index rebuild;
- steady-state representation-owner full scans;
- repeated test compilation of unchanged production C files.

The label-by-label linker and second full immutable-prefix replay were not
deleted. Their originally expected removal was conditional on the post-PA5
profile. Full accepted replay is now 71 to 84 ms, and linker work is not a
dominant phase, so deleting either in this change would add semantic risk without
a measured performance justification.

For every changed file, report lines added, lines deleted, and net change. A net
increase is acceptable when it establishes one reusable index and removes
multiple hidden scans, but duplicate old and new paths must not remain.

## 13. Progress dashboard

| Phase | Status | Result |
| --- | --- | --- |
| Audit and baseline | Complete | Values recorded in this plan |
| PA0 instrumentation | Complete | Stable IF8 and Function Graph gates |
| PA1 reusable test objects | Complete | Cold 33.550 s; warm 16.047 s |
| PA2 Judgement adjacency | Complete | Ordered runtime projections and randomized audit |
| PA3 replay workspace | Target complete | One scratch workspace; replay 71 to 84 ms |
| PA4 incremental prefix replay | Deferred | Full replay is already below preferred target |
| PA5 type-instance cache | Complete | 123,407 hits; exact revisioned key |
| PA6 linker rewrite | Deferred | Not dominant after reprofile; separate authority change |
| PA7 solver reachability | Target complete | Fixed point 597 to 625 ms |
| PA8 artifact scale audit | Partial/deferred | Adjacency and determinism done; scale fixture not triggered |
| PA9 conditional hash/cache work | Rejected for now | Runtime target met; unsafe open-term cache avoided |
| PA10 consolidation | Complete | Quiet mode, matrix, measurements, report |

## 14. Final decision

The measured bottlenecks were removed without introducing a second semantic
authority. Function Graph remains a producer of ordinary A Program AST and the
generated suffix still passes through lowering, solving, evidence
materialization, and accepted replay. The speedup comes from derived indexes,
workspace reuse, and exact memoization of repeated formation, not from trusting
generated Claims or skipping kernel checks.

Further work must start from a new profile. PA4 and PA6 are architectural
changes, not unfinished pieces required to meet this performance objective.
They should only be reopened when accepted replay or linking again dominates a
representative workload, or when their authority-boundary redesign is required
for correctness independently of speed.

## 15. Implemented architecture

### 15.1 Judgement evidence indexes

Propositions and Claims now own runtime-only ordered adjacency heads and tails.
Candidate and accepted Derivations own intrusive next links, while accepted
premise edges own reverse-dependency links. Append order is preserved so indexed
iteration has exactly the same deterministic proof preference as the former
linear store scan. Bulk readback rebuilds and validates these projections once.

These links are not serialized and do not enter Proposition, Claim, or
Derivation identity. The accepted arrays remain authoritative. Index rebuild
rejects premise pointers outside the accepted-premise arena before calculating
reverse-edge IDs, and randomized tests compare indexed traversal with the
authoritative linear order.

### 15.2 Replay workspace

One accepted-graph validation now creates one scratch TermDB and one scratch
SubstitutionDB. Reindex and expected-classifier checks reuse that workspace.
Every validation asserts that the scratch Term intern index is rebuilt at most
once. This replaces up to 120 short-lived TermDB clones and intern-index rebuilds
on the baseline generated QuickSort.

### 15.3 Type and occurrence projections

Type instances use a runtime-only, direct-mapped 4,096-entry cache keyed by:

```text
(TypeDeclaration semantic revision, type ID, argument count, exact argument IDs)
```

The cache can be disabled by an audit option. Artifact tests compile the same
source with and without it and require byte-identical artifacts. Representation
fingerprinting uses `RepresentationDB.representative_type_id` in steady state;
the declaration scan remains only as a dirty-rebuild recovery path.

Branch reachability is cached as a bitset per root occurrence inside one compile
workspace. The index is valid only for the fixed occurrence count used by the
solver and is discarded with the workspace. It is not a graph authority or an
artifact field.

### 15.4 Test and driver work

The prototype Makefile and `prototype_compile` now build dependency-tracked
production objects once per C standard, warning policy, extra preprocessor
flags, and compiler identity.
Tests link only their local check source repeatedly. Each test receives a
private `TMPDIR`; the immutable object cache has a separate explicit root shared
by the suite. Reader, REPL, and compatible test configurations share this same
derived object store, so their common 30 sources are compiled once. Parallel
execution remains disabled.

The reader accepts `--quiet` and `--check-only` for checks that need compilation
or artifact publication but not graph formatting. Default output is unchanged.

## 16. Final measurements

Environment:

```text
baseline commit: 1672c6a62de6e8a7fe815886682ac4436c2527c7
compiler: cc (Debian 14.2.0-19) 14.2.0
machine: Linux 6.12.48+deb13-amd64 x86_64
flags: -std=c11 -Wall -Wextra
IF8 fixture SHA-256: dfa54d5625d03b95a31f99bd143e295df6f2d98cbffdc2d8322879493d64a7c0
Function Graph suffix SHA-256: f0fa4e11bcce5dbd6eb7fcc5e04f8e7348ffdc69ff393eddb605319e7b1b654b
```

| Workload | Baseline | Final | Change | Gate |
| --- | ---: | ---: | ---: | --- |
| Ordinary IF8 median | 426 ms | 183 ms | -57.0% | pass, preferred |
| Function Graph QuickSort median | 4.981 s | 1.719 s | -65.5% | pass, preferred |
| Function Graph accepted replay median | 2.557 s | 83.3 ms | -96.7% | pass, preferred |
| Function Graph fixed point median | 1.484 s | 603.1 ms | -59.4% | pass |
| Integration suite cold | 116.227 s | 33.550 s | -71.1% | pass |
| Integration suite warm | not recorded | 16.047 s | n/a | pass |

IF8 ran in 177 to 208 ms, used 171 to 204 ms user CPU, 4 to 12 ms
system CPU, and 36,564 to 44,188 KiB maximum RSS. Function Graph QuickSort ran
in 1.711 to 1.775 seconds, used 1.699 to 1.762 seconds user CPU, 0 to 12 ms
system CPU, and 52,336 to 56,792 KiB maximum RSS.

Representative generated QuickSort counter changes:

| Counter | Baseline | Final |
| --- | ---: | ---: |
| Term formation requests | 1,011,861 | 388,160 |
| Type declaration formations | 130,994 | 7,419 |
| Type-instance cache hits | n/a | 123,407 |
| Type-instance cache misses | n/a | 7,419 |
| Replay scratch index rebuilds | 120 observed | 2, one per validation |
| Context index rebuilds | 31 in older affected path | 0 |
| Substitution index rebuilds | 31 in older affected path | 0 |
| Solver enqueue requests | 125,091 baseline reported before expanded attribution | 238,001 |
| Solver duplicate requests | not separated | 112,910 |
| Solver actual enqueues | 125,091 | 125,091 |
| Solver pops | 25,490 | 25,490 |

The enqueue request increase in the table is a counter-definition change: the
new counter includes requests rejected by the existing pending-bit deduplication.
Actual enqueues and pops are unchanged. It is therefore not a solver regression.

## 17. Verification record

- `make -f src/prototype/Makefile reader`: passed.
- ordinary IF8 benchmark: three runs, median 183 ms, all internal gates passed.
- Function Graph QuickSort benchmark: three runs, median 1.719 s, all phase
  gates passed.
- integration suite after `make clean` with an empty unified object cache:
  41/41, 33.550 s.
- integration suite with the same warm object cache: 41/41, 16.047 s.
- cache-disabled artifact compilation: byte-identical artifact passed.
- randomized Judgement adjacency corruption/rebuild/order audit: passed through
  `test_context_category.sh`.
- HOTT publication, readback, forgery, and aggregate-link checks: passed.

## 18. Per-file implementation delta

The implementation and tests, excluding this self-reporting plan document,
contain **1,526 added lines**, **280 deleted lines**, net **+1,246 lines**. The
largest additions are reusable runtime-index validation and permanent boundary
tests; semantic authorities were not duplicated.

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `.gitignore` | 1 | 0 | +1 |
| `src/prototype/Makefile` | 24 | 8 | +16 |
| `src/prototype/build/test_support.sh` | 52 | 6 | +46 |
| `src/prototype/build/test_objects.mk` | 25 | 0 | +25 |
| `src/prototype/include/a_program/core/term.h` | 22 | 0 | +22 |
| `src/prototype/include/a_program/graph/compile_metadata.h` | 13 | 0 | +13 |
| `src/prototype/include/a_program/kernel/judgement/db.h` | 1 | 1 | 0 |
| `src/prototype/include/a_program/kernel/judgement/types.h` | 31 | 0 | +31 |
| `src/prototype/src/artifact/publication/closure_marking_and_slices.inc` | 38 | 7 | +31 |
| `src/prototype/src/artifact/wire_v83.c` | 3 | 8 | -5 |
| `src/prototype/src/core/term/storage_and_formation.inc` | 92 | 3 | +89 |
| `src/prototype/src/driver/compiler_session.c` | 39 | 1 | +38 |
| `src/prototype/src/driver/read_file.c` | 89 | 5 | +84 |
| `src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc` | 62 | 38 | +24 |
| `src/prototype/src/frontend/lowering/constraint/classifier_and_computation_propagation.inc` | 8 | 0 | +8 |
| `src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc` | 1 | 1 | 0 |
| `src/prototype/src/frontend/lowering/context_and_type_lowering.inc` | 12 | 0 | +12 |
| `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` | 4 | 0 | +4 |
| `src/prototype/src/frontend/lowering/graph_construction.inc` | 6 | 0 | +6 |
| `src/prototype/src/kernel/judgement.c` | 1 | 0 | +1 |
| `src/prototype/src/kernel/rules/cbpv.inc` | 1 | 1 | 0 |
| `src/prototype/src/kernel/type_declaration.c` | 41 | 11 | +30 |
| `src/prototype/src/kernel/typing/accepted_replay.inc` | 162 | 53 | +109 |
| `src/prototype/src/kernel/typing/candidate_publication.inc` | 254 | 40 | +214 |
| `src/prototype/src/kernel/typing/candidate_replay.inc` | 2 | 2 | 0 |
| `src/prototype/src/kernel/typing/conversion.inc` | 1 | 1 | 0 |
| `src/prototype/src/kernel/typing/judgement_db.inc` | 10 | 3 | +7 |
| `src/prototype/tests/checks/context_category_check.c` | 225 | 0 | +225 |
| `src/prototype/tests/checks/hott/adt_identity.inc` | 9 | 6 | +3 |
| `src/prototype/tests/checks/hott/artifact_roots.inc` | 35 | 6 | +29 |
| `src/prototype/tests/checks/hott/test_support.inc` | 36 | 1 | +35 |
| `src/prototype/tests/fixtures/typing/function_graph_quicksort_benchmark_suffix.p` | 18 | 0 | +18 |
| `src/prototype/tests/integration/test_artifact_flow.sh` | 4 | 0 | +4 |
| `src/prototype/tests/integration/test_cbpv_boundary.sh` | 5 | 5 | 0 |
| `src/prototype/tests/integration/test_context_category.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_conversion_result.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_conversion_scope.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_dimension_term.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_hott_goal.sh` | 35 | 35 | 0 |
| `src/prototype/tests/integration/test_resource_usage.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_shared_term_hott_substrate.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_source_manifest.sh` | 5 | 2 | +3 |
| `src/prototype/tests/integration/test_spec_consistency.sh` | 5 | 2 | +3 |
| `src/prototype/tests/integration/test_term_child_roles.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_term_identity_frame.sh` | 5 | 3 | +2 |
| `src/prototype/tests/integration/test_term_intern_index.sh` | 5 | 2 | +3 |
| `src/prototype/tests/integration/test_type_inspection.sh` | 2 | 2 | 0 |
| `src/prototype/tests/integration/test_typed_occurrence_transaction.sh` | 5 | 3 | +2 |
| `src/prototype/tests/performance/benchmark_function_graph_quicksort_single_compile.sh` | 94 | 0 | +94 |
| `src/prototype/tests/performance/benchmark_if8_single_compile.sh` | 2 | 2 | 0 |
| `src/prototype/tests/run_integration_suite.sh` | 6 | 1 | +5 |
