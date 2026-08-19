# QuickSort Single-Compile Performance and Authority Consolidation Plan

Date: 2026-08-19

Status: Single-compile performance objective complete; non-critical authority
follow-ups remain

Baseline commit: `3413bf8`

## 1. Purpose

This plan addresses the cost of compiling the IF8 fuel-free QuickSort source once.
It does not treat repeated execution by the integration test as the primary defect.

At the baseline commit, this command succeeds but takes several minutes:

```sh
./read_file.out \
  src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
```

The measured wall time was 208,028 ms on the current development machine. A
compiler that needs more than three minutes for this 216-line fixture is not an
acceptable basis for further Equality, IADT, or solver work.

The implementation objective is therefore:

> Reduce the cost of one real QuickSort compilation by removing algorithmic
> full-store scans and repeated reconstruction, while preserving the accepted
> Term, Claim, Derivation, artifact, and runtime semantics.

Test-process deduplication may be done later, but it cannot satisfy this plan.

## 2. Scope

This plan covers:

- TermDB interning and canonical identity lookup;
- normalization and substitution caches;
- classifier, effect, and computation constraint generation;
- context resolution and fixed-point scheduling;
- conversion of solved constraints into Judgement candidates;
- the accepted replay premise API;
- TypeDeclarationDB authority boundaries;
- deterministic performance counters and QuickSort benchmarks;
- physical decomposition of oversized implementation units after semantic
  ownership has been corrected.

This plan does not:

- weaken or remove QuickSort proofs;
- replace dependent typing with unchecked annotations;
- cache a previous compiler result for this particular fixture;
- count repeated test-process elimination as a single-compile speedup;
- change accepted artifact meaning merely to reduce runtime;
- merge distinct kernel rules such as APP, MATCH, IH, and COMPUTATION_FOLD into
  an opaque generic rule.

## 3. Baseline Evidence

### 3.1 Source and result size

The baseline successful compilation reported approximately:

| Metric | Baseline |
|---|---:|
| Source lines | 216 |
| Type declarations | 9 |
| Constructors | 16 |
| Labels | 40 |
| Unique TermDB terms | 23,150 |
| Typed occurrences | 752 |
| Match cases | 29 |
| Solver constraints | 1,265 |
| Solved constraints | 1,257 |
| Solver worklist steps | 16,842 |
| Wall time | 208,028 ms |

The source size and final constraint count do not justify the wall time. The
problem is the amount of repeated internal work per source item and per
constraint.

### 3.2 Profiling method

A temporary `-pg` build was used without source modification:

```sh
make -B -f src/prototype/Makefile reader \
  CFLAGS='-std=c11 -Wall -Wextra -pg'
./read_file.out \
  src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
gprof ./read_file.out gmon.out
```

The instrumentation increases wall time, so its absolute time is not used as a
performance target. Its call counts identify the algorithmic amplification:

| Operation | Profiled calls |
|---|---:|
| `add_term` | 1,497,102 |
| `term_intern_alpha_equal_local` | 2,014,196,545 |
| direct `shape_terms_equal_at_depth` | 2,014,494,131 |
| `shape_term_compare_id` | 4,764,377,922 |
| normalization cache lookup | 516,600 |
| complete normalization entry | 493,686 |
| substitution internal entry | 509,089 |
| substitution extension | 84,642 |
| candidate authority lookup | 57,364,263 |
| proof candidate equality | 8,229,173 |
| proposition equality | 24,025,706 |

The compiler produced only 23,150 unique terms, but requested term formation
about 1.5 million times and performed about two billion candidate alpha
comparisons. This is the primary demonstrated cause of the single-compile cost.

### 3.3 Repeated global passes

The same profile observed:

| Pass | Calls |
|---|---:|
| constraint generation | 36 |
| constraint indexing | 36 |
| context resolution | 36 |
| ContextDB index rebuild | 36 |
| SubstitutionDB index rebuild | 36 |
| computation constraint generation | 8 |
| solver entry | 10 |

The solver has a worklist, but outer fixed-point processing repeatedly rebuilds
global derived state. The worklist is therefore not yet an incremental solver in
the strong sense required here.

### 3.4 Integration-test time is secondary

The timed integration suite spent 988,723 ms in the IF8 test because that test
performs several complete compilations for negative checks, source equality,
publication, determinism, artifact equality, and readback. This duplication is
real, but one ordinary compilation already costs 208,028 ms. The single compile
must be fixed before process reuse is considered sufficient.

## 4. Root-Cause Ranking

### RC1. Critical: TermDB interning is quadratic

`src/prototype/src/core/term/storage_and_formation.inc` temporarily appends each
candidate and compares it against every prior Term with
`term_intern_alpha_equal_local`.

The effective shape is:

```text
for every formation request:
    for every existing term:
        recursively compare alpha shape
```

This is worse than a simple `O(unique_terms^2)` cost because intermediate
normalization, substitution, reindexing, and proof reification repeatedly request
terms that already exist. Every duplicate request still pays the scan.

This must be fixed first. Optimizing the solver while retaining this lookup can
still leave every reconstructed term expensive.

### RC2. High: normalization cache lookup is linear

The current 1,024-entry normalization cache scans the full fixed array for
lookup and reservation. More than 500,000 lookups can therefore cause hundreds
of millions of entry probes.

The cache must use a keyed index over at least:

```text
(TermDB graph revision, reduction profile, input term ID)
```

The graph revision is required because normalization results must not survive a
semantic mutation of the graph.

### RC3. High: the fixed point regenerates global state

`finalization_and_entrypoints.inc` repeatedly performs pending-Match resolution,
context resolution, general classifier inference, CBPV binding, computation
constraint regeneration, operand refresh, solver passes, and effect generation.

The broad reconstruction is triggered by global count changes rather than
precise dirty dependencies. Context and substitution indexes are rebuilt along
the way. The solver worklist only controls work inside an individual solve; it
does not prevent the outer pipeline from recreating the graph.

### RC4. High: solved-state authority is represented in overlapping stores

The operation constraint store is intended to own mutable elaboration state, but
computation lifecycle and payload information is also rebuilt through
JudgementDelta records and ID links. This creates synchronization work and
encourages suffix retirement/regeneration.

The target must be:

- one persistent ConstraintGraph owns mutable solving state;
- JudgementDelta receives immutable kernel-candidate output only after the
  relevant constraint is terminal;
- accepted Proposition, Claim, and Derivation stores own persistent proof
  evidence after validation;
- no store is a mutable mirror of another store's constraints.

### RC5. Medium: proof and proposition searches are repeatedly linear

Tens of millions of candidate authority and proposition-equality calls indicate
that materialization and publication repeatedly rediscover evidence relations.
Stable keys and indexes are required for:

- proposition identity;
- candidate authority;
- candidate premise lists;
- solved-constraint to candidate mapping;
- observation/equality evidence lookup.

### RC6. Medium: substitution memoization is local and linear

Substitution uses a per-invocation linear memo table and discards it at the end.
This is semantically safe but fails to exploit repeated substitution of the same
term by the same immutable substitution. It should first become a hash-indexed
local memo. Cross-invocation reuse may be added only after substitution and graph
revision identity are explicit.

### RC7. Not the current hot path: accepted replay premise reconstruction

`accepted_replay.inc` rebuilds candidate-shaped premise arrays for accepted
derivations and casts away `const` from propositions. This is an API and
correctness-boundary defect, but accepted graph validation accounted for a small
fraction of the measured QuickSort time.

It must be corrected, but it must not be presented as the fix for the 208-second
compile.

### RC8. Not the current hot path: TypeDeclarationDB access breadth

TypeDeclarationDB physically contains semantic schema, readback state,
representation state, and classifier caches. The current QuickSort fixture has
only nine type declarations and representation rebuilding was not a dominant
profiled cost.

The broad API is nevertheless unsafe: solver and kernel code can accidentally
consult readback or cache state as semantic authority. The stores need explicit
views, but this cleanup follows the demonstrated algorithmic fixes.

### RC9. Large `.inc` files are a symptom, not a root cause

Current sizes include:

| File | Lines |
|---|---:|
| `constraint_solver.inc` | 11,936 |
| `graph_construction.inc` | 8,619 |
| `accepted_replay.inc` | 7,400 |

Splitting these files without changing ownership would only redistribute the
same complexity. They should be decomposed after the ConstraintGraph,
materialization, and replay boundaries are explicit.

## 5. Target Architecture

### 5.1 Term identity index

TermDB remains the append-only owner of canonical Core Terms. Add a runtime-only
intern index:

```text
canonical hash -> candidate Term IDs
```

Each canonical entry must include enough cheap discriminators to reject most
collisions before recursive alpha comparison, such as tag, arity, binder count,
case count, and canonical hash. Existing canonical-key code should be reused
rather than introducing a second definition of alpha identity.

Required behavior:

- hash and key lookup is the normal path;
- deep alpha comparison occurs only among true hash/key candidates;
- a collision never causes two unequal Terms to merge;
- alpha-equivalent binders still intern to one canonical Term;
- distinct nominal/TypeView semantics that are currently distinct remain so;
- the earliest existing Term ID remains the canonical result;
- index state is runtime-only and does not alter artifact numbering;
- any operation that mutates or bulk-relocates stored Terms must invalidate or
  rebuild the index explicitly.

### 5.2 Indexed normalization cache

Replace full-array scans with a hash table plus bounded replacement metadata.
Lookup must be expected `O(1)`. Replacement policy may remain bounded and simple;
semantic correctness depends on key and invalidation, not on LRU precision.

### 5.3 Persistent ConstraintGraph

Generate each source-derived constraint once and assign a stable ID. A constraint
record owns:

- kind and source occurrence;
- stable operands;
- current result or residual state;
- reverse dependencies;
- dirty and queued bits;
- semantic revision;
- diagnostics provenance.

When a result changes, enqueue only reverse dependents. Do not retire and rebuild
an entire classifier/effect/computation suffix.

Branch refinement must add or update explicit pullback/refinement constraints for
the affected branch. It must not request global constraint generation as a
normal restart path.

### 5.4 Context revisions instead of global rebuilding

Context and substitution records remain separate semantic stores. Add explicit
revision/dependency tracking so that:

- a context is resolved once per changed dependency set;
- ContextDB and SubstitutionDB indexes are rebuilt only after physical mutation,
  not once per solver round;
- unchanged occurrences retain their resolved context IDs;
- finalized resolution is a validation state transition, not another independent
  inference algorithm.

### 5.5 Projection boundaries

The ownership flow becomes:

```text
surface/operation occurrence
        |
        v
persistent ConstraintGraph       mutable elaboration authority
        |
        v
terminal solved constraint
        |
        v
Judgement candidate              immutable kernel input
        |
        v
accepted Proposition/Claim/Derivation
        |
        v
artifact roots and replay
```

TypedOccurrence classifier/effect fields are projections or references to solved
results. They must not become an independently writable authority.

### 5.6 Read-only accepted replay API

Candidate and accepted derivations may share a read-only rule-input interface,
but accepted replay must not masquerade as a mutable candidate.

Introduce a premise view/accessor that yields:

```text
const proposition *
```

for either source form. Remove candidate-shaped stack reconstruction and all
`const`-discarding casts. Keep rule-specific validators visible; this change is
data plumbing consolidation, not proof-rule erasure.

### 5.7 TypeDeclarationDB semantic views

Expose narrow interfaces:

- semantic schema view: formation, constructors, classifier families;
- readback view: labels and diagnostic reconstruction;
- representation view: computed structural representation;
- cache view: replaceable acceleration only.

Solver and kernel APIs receive the semantic view unless they explicitly perform
readback. No semantic decision may succeed only because a readback or cache entry
exists.

## 6. Implementation Phases

### SP0. Reproducible measurement

- [ ] Add a dedicated single-compile timing target for the IF8 source.
- [ ] Record wall, user, and system time separately.
- [ ] Add counters for term formation requests, unique insertions, intern bucket
      probes, deep alpha comparisons, normalization lookups/hits/probes,
      substitution memo probes, generated constraints, dirty transitions,
      worklist pops, context resolutions, and index rebuilds.
- [ ] Keep instrumentation disabled or cheap in normal builds.
- [ ] Record compiler flags, commit, fixture hash, and machine description.
- [ ] Run one warm-up and report the median of three measured runs.

Exit criteria:

- [ ] The 208,028 ms baseline can be reproduced within an explained variance.
- [ ] Counters distinguish single compilation from integration-test repetition.

### SP1. Replace quadratic TermDB interning

Primary files:

- `src/prototype/include/a_program/core/term.h`
- `src/prototype/src/core/term/storage_and_formation.inc`
- `src/prototype/src/core/term/canonicalization.inc`
- TermDB lifecycle and test files

Tasks:

- [ ] Define the canonical intern key from the existing alpha canonicalization
      semantics.
- [ ] Add hash buckets or open-addressed entries owned by TermDB.
- [ ] Cache canonical hashes for stored Terms where safe.
- [ ] Search only the matching bucket and verify collisions by alpha comparison.
- [ ] Preserve earliest-ID canonicalization.
- [ ] Implement index reserve, rebuild, invalidation, and disposal.
- [ ] Audit every direct Term mutation and relocation operation.
- [ ] Add a debug mode comparing indexed lookup with the old full scan on small
      inputs during migration.
- [ ] Delete the full-store production scan after equivalence tests pass.

Tests:

- [ ] repeated identical APP and constructor formation;
- [ ] alpha-equivalent Lambda formation with different binder IDs;
- [ ] alpha-equivalent Match branches with different pattern binders;
- [ ] non-equivalent nominal/TypeView cases remain distinct;
- [ ] forced hash collision does not merge unequal Terms;
- [ ] index rebuild after supported graph relocation;
- [ ] artifact output remains byte-deterministic.

Exit criteria:

- [ ] `add_term` contains no loop over all existing Terms.
- [ ] Deep alpha comparisons fall from billions to collision-bucket scale.
- [ ] One IF8 compile is at most 20,803 ms, or SP1 documents the newly dominant
      measured path before proceeding.

### SP2. Normalize and substitute by indexed keys

Primary files:

- `src/prototype/src/core/term/evaluation_and_conversion.inc`
- `src/prototype/src/core/term/substitution.inc`

Tasks:

- [ ] Replace normalization cache scans with expected-constant-time lookup.
- [ ] Include reduction profile and graph revision in the key.
- [ ] Make hit, miss, collision, eviction, and invalidation counters visible.
- [ ] Replace linear local substitution memo lookup with a hash index.
- [ ] Decide whether cross-invocation substitution reuse is valid only after
      substitution identity and graph revision are explicit.
- [ ] Do not cache effectful/runtime execution as kernel normalization.

Exit criteria:

- [ ] No normalization lookup scans all 1,024 entries.
- [ ] Pure conversion and normalization tests remain unchanged.
- [ ] IF8 timing and counters show the contribution of SP2 separately.

### SP3. Add persistent constraint indexes

Primary files:

- `src/prototype/src/frontend/lowering/graph_construction.inc`
- `src/prototype/src/frontend/lowering/constraint_solver.inc`
- relevant lowering headers

Tasks:

- [ ] Index constraints by source occurrence and kind.
- [ ] Index motive equations by `(match occurrence, case ordinal)`.
- [ ] Index computation constraints by occurrence.
- [ ] Build explicit reverse dependency adjacency once.
- [ ] Replace linked-list duplicate checks with keyed adjacency membership.
- [ ] Index clause-fold dependents instead of scanning all occurrences and
      classifier constraints.
- [ ] Give each result slot a monotonic revision.
- [ ] Enqueue dependents only when a semantic result changes.

Exit criteria:

- [ ] Motive and computation lookup do not scan the complete constraint store.
- [ ] Worklist queueing remains deterministic.
- [ ] Constraint IDs remain stable for the entire compilation.

### SP4. Make the fixed point genuinely incremental

Primary files:

- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`
- `src/prototype/src/frontend/lowering/constraint_solver.inc`
- context/substitution index code

Tasks:

- [ ] Replace global-count convergence with dirty-domain convergence.
- [ ] Generate source-derived constraints once.
- [ ] Represent branch refinements as affected constraint updates.
- [ ] Stop retiring and regenerating the computation/effect suffix.
- [ ] Resolve only contexts whose dependencies changed.
- [ ] Remove per-round ContextDB and SubstitutionDB index rebuilds.
- [ ] Keep a bounded diagnostic guard for internal non-convergence; it is not a
      semantic fuel limit.
- [ ] Assert that a clean fixed point performs no further mutations.

Exit criteria:

- [ ] Ordinary IF8 compilation performs one initial global generation pass.
- [ ] Later refinement is reported as targeted updates, not global regeneration.
- [ ] Context/index rebuild counts are tied to actual physical mutations and are
      not proportional to solver rounds.

### SP5. Materialize judgements once

Primary files:

- `src/prototype/src/frontend/lowering/constraint_solver.inc`
- Judgement candidate and publication modules

Tasks:

- [ ] Make the ConstraintGraph the only mutable owner of pending solve state.
- [ ] Remove mutable computation-constraint mirrors from JudgementDelta.
- [ ] Materialize a candidate only when its source constraint is terminal.
- [ ] Intern proposition and premise-list identities with explicit indexes.
- [ ] Memoize constraint-to-candidate and proof-reification results.
- [ ] Preserve every distinct valid derivation DAG; do not overwrite evidence by
      relation key.

Exit criteria:

- [ ] No solve iteration reconstructs already terminal candidates.
- [ ] Candidate authority and proposition lookup are indexed.
- [ ] Accepted Claim/Derivation results are unchanged.

### SP6. Correct accepted replay premise access

Primary file:

- `src/prototype/src/kernel/typing/accepted_replay.inc`

Tasks:

- [ ] Introduce a read-only rule-application/premise view.
- [ ] Remove stack reconstruction of candidate premise arrays for accepted data.
- [ ] Remove proposition `const` casts.
- [ ] Keep candidate validation and accepted replay entry points distinct.
- [ ] Keep APP, MATCH, IH, and COMPUTATION_FOLD validators explicit.

Exit criteria:

- [ ] Accepted replay traverses accepted structures directly.
- [ ] Compiler warnings and static checks find no discarded proposition constness.
- [ ] Artifact rejection tests continue to reject malformed premise graphs.

### SP7. Restrict TypeDeclarationDB authority

Primary files:

- `src/prototype/include/a_program/kernel/type_declaration.h`
- `src/prototype/src/kernel/type_declaration.c`
- solver, readback, artifact, and diagnostics consumers

Tasks:

- [ ] Define semantic-schema, readback, representation, and cache views.
- [ ] Convert solver/kernel consumers to semantic-schema access.
- [ ] Convert printers/diagnostics to explicit readback access.
- [ ] Mark representation and cache data as derived and rebuildable.
- [ ] Add tests proving semantic validation does not depend on readback/cache
      population.

Exit criteria:

- [ ] A semantic consumer cannot access readback/cache through its API type.
- [ ] Clearing and rebuilding derived stores does not change accepted semantics.

### SP8. Physical decomposition and secondary test reuse

Only after SP3-SP7 establish stable ownership:

- [ ] Split constraint model/indexing, propagation rules, context resolution, and
      candidate materialization into focused implementation units.
- [ ] Split accepted replay access plumbing from rule validators.
- [ ] Keep headers declarative and avoid creating catch-all utility modules.
- [ ] Reuse one compiled IF8 artifact inside integration scenarios where the
      scenario does not specifically test recompilation or determinism.
- [ ] Retain independent compiles for source determinism and publication
      boundary tests where fresh construction is the subject under test.

Exit criteria:

- [ ] File decomposition follows authority boundaries rather than line-count
      targets.
- [ ] Single-compile performance is unchanged or improved by the split.
- [ ] Integration IF8 time is reduced without weakening scenario coverage.

## 7. Performance Acceptance Gates

The same default, non-profiler build and the same fixture must be used before and
after each phase.

### Mandatory gates

- Single IF8 compile: no more than 20,803 ms, ten percent of the baseline.
- Preferred single IF8 target: no more than 10,000 ms.
- Full IF8 integration test: no more than 148,308 ms after secondary reuse work.
- Preferred full IF8 integration target: no more than 60,000 ms.
- No production full-TermDB scan in `add_term`.
- No production full normalization-cache scan.
- One initial global source-constraint generation pass.
- No per-solver-round ContextDB/SubstitutionDB index rebuild.

Wall-time gates belong to the dedicated benchmark and should not make ordinary
functional tests flaky. Functional CI should additionally enforce deterministic
algorithmic counters or generous upper bounds, such as intern candidate probes
and global generation counts.

An optimized compiler flag alone does not satisfy any algorithmic gate.

## 8. Semantic Verification Matrix

Every phase must retain:

- examples 01-09 and current accepted examples;
- IF8 positive compilation;
- IF8 negative decrease and transport checks;
- source normalization/equality checks;
- artifact publication and readback;
- artifact deterministic byte output;
- accepted replay rejection of malformed graphs;
- alpha identity for Lambda and Match binders;
- nominal TypeView distinctions;
- dependent classifier and branch refinement behavior;
- Equality/Observation Claim and Derivation preservation;
- runtime behavior of the compiled QuickSort main term.

Run focused tests after each phase. Run the full integration audit only after the
focused tests and single-compile benchmark pass.

## 9. Progress Dashboard

| Phase | Status | Required evidence |
|---|---|---|
| SP0 measurement | Complete | reproducible runner, process time, and algorithmic counters |
| SP1 TermDB intern index | Complete | collision/alpha tests and benchmark |
| SP2 normalization/substitution indexes | Complete | indexed caches and benchmark counters |
| SP3 persistent constraint indexes | Complete | occurrence, motive, computation, and reverse-dependency indexes |
| SP4 incremental fixed point | Partial | one global generation pass; branch updates preserve constraint identity |
| SP5 one-time judgement materialization | Partial | indexed candidate/proposition paths; final authority cleanup remains |
| SP6 accepted replay API | Deferred | runtime indexes are isolated; read-only premise API remains follow-up |
| SP7 TypeDeclaration views | Existing partial boundary | semantic view is used; broad API cleanup remains follow-up |
| SP8 physical split/test reuse | Deferred | full suite is 105 seconds; no single-compile benefit demonstrated |

## 10. Required Completion Report

The final report must contain:

- baseline and final median single-compile wall/user/system time;
- integration-test time reported separately;
- before/after values for every SP0 algorithmic counter;
- flame graph or profiler summary before and after;
- artifact determinism result;
- focused and full test results;
- remaining residual or unsupported constraints;
- files changed with added, deleted, and net lines per file;
- modules or duplicate authorities deleted, not only newly added indexes;
- any performance target not met and the measured remaining bottleneck.

## 11. Completion Report

### 11.1 Performance result

The final benchmark used one warm-up and three measured runs. The authoritative
post-change result is:

| Metric | Baseline | Final median | Change |
|---|---:|---:|---:|
| Wall time | 208,028 ms | 5,436 ms | 38.27x faster, 97.39% lower |
| Final measured wall range | - | 5,416-5,834 ms | three runs |
| Final user time | - | 5,415 ms | median run |
| Final system time | - | 20 ms | median run |

The mandatory 20,803 ms gate was exceeded by a wide margin. The fixture, proof
obligations, accepted Claims and Derivations, and artifact behavior were not
weakened.

### 11.2 Final algorithmic counters

The three final runs were stable. Each reported:

| Counter | Final value |
|---|---:|
| Term formation requests | 1,185,381 |
| Unique main TermDB terms | 12,272 |
| Intern bucket probes | 2,031,045 |
| Exact-identity probes | 1,452,318 |
| Deep alpha comparisons | 2,027,655 |
| Main TermDB intern rebuilds | 2 |
| Normalization hits | 436,155 |
| Normalization misses | 4,722 |
| Normalization probes | 452,903 |
| Normalization evictions | 0 |
| Normalization invalidations | 161 |
| Global constraint generations | 1 |
| Global constraint index builds | 1 |
| Computation constraint generations | 8 |
| Worklist enqueues | 35,556 |
| Worklist pops | 7,757 |
| Context resolutions | 31 |
| ContextDB index rebuilds | 31 |
| SubstitutionDB index rebuilds | 31 |

The baseline performed about 2.014 billion deep alpha comparisons and 36 global
constraint-generation passes. The final implementation performs about 2.028
million deep comparisons and one global generation pass. Context rebuild counts
remain an explicit SP4 follow-up, but they no longer dominate this compile.

### 11.3 Final profiler summary

The baseline hot path was dominated by approximately 2.014 billion calls to
`shape_terms_equal_at_depth` and 4.764 billion calls to
`shape_term_compare_id`. In the final profile, the corresponding counts were
35,563,375 and 278,062,468 across the profiled process, including replay scratch
stores. Candidate proof equality and effect-row whole-store scans disappeared
as visible hotspots after adding derived indexes.

The remaining largest sampled costs were structural term comparison, intern
index rebuilding for short-lived replay stores, Match case comparison,
proposition comparison, and candidate authority lookup. They are follow-up
optimization opportunities, not blockers for IF8 compilation.

### 11.4 Semantic verification

The permanent forced-collision/alpha-identity TermDB test passes. The full
integration suite passes all 37 tests in 105,102 ms. The IF8 integration test
passes in 22,102 ms and covers:

- positive source compilation and source equality;
- negative decrease and transport proofs;
- publication and artifact readback;
- artifact normalization equality;
- deterministic artifact bytes.

### 11.5 Implemented authority changes

- TermDB owns exact and alpha-canonical runtime indexes while preserving the
  earliest canonical Term ID.
- Normalization and substitution memoization use indexed keys.
- Motive and computation constraints have stable occurrence indexes and reverse
  dependency edges.
- Branch refinement refreshes persistent source constraints instead of retiring
  and regenerating the global suffix.
- Judgement candidate and computation occurrence lookup use derived indexes;
  distinct valid Derivation DAGs remain distinct.
- Effect-row variables use a TermDB-derived runtime index rather than repeated
  whole-store scans.
- Accepted replay scratch stores rebuild and dispose their own runtime indexes;
  runtime index state is not serialized.

### 11.6 Deliberately unfinished follow-ups

This change does not claim all original architectural phases are complete.
Dirty-domain-only context resolution, removal of the remaining mutable pending
state overlap, a fully read-only accepted replay premise API, stricter
TypeDeclarationDB view types, and physical file decomposition remain separate
work. None was used as a shortcut for the measured single-compile result.

### 11.7 Implementation and test line changes

The implementation and permanent tests add 2,180 lines, delete 324 lines, and
have a net change of +1,856 lines. The plan document itself is excluded from
these totals. No semantic module was deleted: the removed code consists of
full-store scans, suffix regeneration, and duplicated lookup plumbing replaced
in place by runtime-only derived indexes. Paths in the table are relative to
`src/prototype/`, except for the explicitly prefixed Makefile path.

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `src/prototype/Makefile` | 6 | 2 | +4 |
| `include/a_program/core/term.h` | 50 | 0 | +50 |
| `include/a_program/graph/compile_metadata.h` | 11 | 0 | +11 |
| `include/a_program/kernel/judgement/classifier_solver.h` | 8 | 0 | +8 |
| `include/a_program/kernel/judgement/db.h` | 10 | 0 | +10 |
| `include/a_program/kernel/judgement/types.h` | 3 | 0 | +3 |
| `src/core/term/canonicalization.inc` | 396 | 0 | +396 |
| `src/core/term/declarations.inc` | 3 | 0 | +3 |
| `src/core/term/evaluation_and_conversion.inc` | 48 | 17 | +31 |
| `src/core/term/storage_and_formation.inc` | 441 | 8 | +433 |
| `src/core/term/substitution.inc` | 98 | 16 | +82 |
| `src/driver/program_storage.c` | 1 | 0 | +1 |
| `src/driver/read_file.c` | 62 | 0 | +62 |
| `src/frontend/lowering/constraint_solver.inc` | 451 | 178 | +273 |
| `src/frontend/lowering/context_and_type_lowering.inc` | 20 | 4 | +16 |
| `src/frontend/lowering/finalization_and_entrypoints.inc` | 4 | 7 | -3 |
| `src/frontend/lowering/graph_construction.inc` | 92 | 77 | +15 |
| `src/kernel/rules/cbpv.inc` | 83 | 10 | +73 |
| `src/kernel/rules/formation_early.inc` | 2 | 0 | +2 |
| `src/kernel/typing/accepted_replay.inc` | 21 | 0 | +21 |
| `src/kernel/typing/candidate_publication.inc` | 69 | 4 | +65 |
| `src/kernel/typing/judgement_db.inc` | 7 | 0 | +7 |
| `tests/checks/term_intern_index_check.c` | 157 | 0 | +157 |
| `tests/integration/test_constraint_authority.sh` | 4 | 1 | +3 |
| `tests/integration/test_term_intern_index.sh` | 11 | 0 | +11 |
| `tests/performance/benchmark_if8_single_compile.sh` | 54 | 0 | +54 |
| `tests/support/process_metrics.c` | 68 | 0 | +68 |
