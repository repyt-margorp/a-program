# Issue 23 Dependent Motive Correction and Module Integration Plan

Date: 2026-08-27 JST

Status: completed on 2026-08-28. PR 24 is merged; dependent one-IH, multi-IH,
QuickSort structural output evidence, v86 replay, focused v87 checked Core, and
producer module consolidation are permanent regression boundaries. General
neutral-index anti-unification and Sorted/permutation libraries are separate
language features, not Issue 23 closure conditions.

Issue: #23, `Bug: generated Function Graph dependent motives become constant
and fail recursive IH replay`

Merged audit PR: #24, `Document generated Function Graph dependent-motive
replay failure`

Merged revision: `f71e61c04d8d159f076a1698eb301d8845918db5`

Source audit:

- `2026-08-27T00-40-19-GENERATED-FUNCTION-GRAPH-DEPENDENT-MOTIVE-IH-REPLAY-AUDIT.md`

## 0. Implementation Disposition

The implementation confirmed the PR's concrete failure but narrowed its
proposed abstraction boundary.

- Exact Term ID replacement remains the first and authoritative structural
  operation.
- When the selected index is a constructor spine, the compiler derives its
  Core spine from that exact typed constructor owner and searches only for that
  constructor shape in the candidate classifier. It does not perform a
  database-wide DefEq search and does not equate unrelated nominal TypeViews.
- A one-branch constant seed that fails on a recursive branch is discarded as
  provisional evidence. It is not reported as a type contradiction.
- An indexed candidate derived from an IH expected-classifier equation records
  the exact producing IH occurrence. Recursive equation evidence is checked
  separately from ordinary conversion and from guardedness.
- Expected classifiers may validate a solved indexed motive, but cannot create
  a new indexed motive during finalization. The fallback remains the traditional
  unary exact-scrutinee motive.
- Accepted replay was not changed. The original Book reproducer now passes
  because the producer publishes the correct dependent motive.

The following broader PR suggestion was not adopted in this revision:

> Reconstruct every indexed motive occurrence by generally inverting solved
> Context/Substitution morphisms.

That remains useful for arbitrary neutral or non-constructor indices, but it is
not required to fix Issue 23 and would introduce a substantially wider typed
rewriting authority. The present rule is deliberately constructor-only. A
future generalization must define an explicit typed correspondence object and
must retain the same all-equation and replay gates.

## 1. Objective

Correct the false rejection of a valid property over a compiler-generated
Function Graph when all of the following are present:

1. the Match motive depends on the generated function output;
2. a recursive branch consumes the generated IH field; and
3. accepted P0 replay reconstructs the branch classifier.

The correction must preserve these A Program boundaries:

- Core Term remains the type-erased computation graph;
- TypedOccurrenceGraph and solved Context/Substitution data identify typed
  source occurrences and branch refinement;
- the constraint solver constructs one motive from all branch equations;
- guarded recursion provenance and classifier equality are separate facts;
- accepted replay remains an independent acceptance authority; and
- Function Graph generation uses the same indexed-Match solver as hand-written
  indexed families.

This is not a plan to weaken replay, add a Function Graph-specific equality
oracle, or turn Core structural substitution into typed conversion.

## 2. Current Reproduction

At the merged revision, the following command fails:

```sh
./read_file.out \
  ../a-book/code/14_function_graph_surface_boundary/posthoc_length_property.p
```

The relevant source is:

```a-program
lengthOutputUnary := \input : NatList => \output : Nat =>
    \graph : @length input output =>
        graph
            @nil => Unary.zero
            @cons head tail tailLength tailGraph =>
                Unary.succ tailLength *tailGraph;
```

The current failure is:

```text
P0 IH replay failed derivation=166 claim=166 occurrence=103
subject=388 classifier=692 premises=0
P0 accepted proof graph validation failed
failed to generate accepted function graph
```

The intended motive is:

```text
M(input, output, graph) = Comp({}, Unary output, TOTAL)
```

The producer instead stores the equivalent of:

```text
M(input, output, graph) = Comp({}, Unary Nat.zero, TOTAL)
```

P0 replay is correct to reject that stored motive.

Planning-time verification at `f71e61c` also established this control:

```text
test_function_graph_certified_execution.sh: pass
quicksort_dependency_closure:             pass, 7189 ms
Issue 23 Book reproducer:                 fail at P0 replay, as expected
```

The official suite therefore does not cover the failing conjunction. The
QuickSort phase is also the performance baseline that the correction must not
materially regress.

## 3. Critical Review of PR 24

### 3.1 Findings adopted without change

| Audit finding | Decision | Reason |
| --- | --- | --- |
| Physical Term ID replacement misses the generated output index | Adopt | Confirmed at `substitution.inc:698` and the seed call in `graph_construction.inc` |
| Same-branch reapplication accepts a vacuous abstraction | Adopt | A constant candidate reproduces the seed branch by construction |
| Guarded IH ownership is promoted to equality | Adopt | Confirmed by assignment to `PROTOTYPE_TERM_CONVERSION_EQUAL` |
| Accepted replay is the correct rejecting authority | Adopt | Replay reconstructs the motive application independently |
| Constant motives must remain legal | Adopt | Dependency cannot be required for every family index |
| The full control matrix needs permanent tests | Adopt | Existing QuickSort coverage uses a constant motive |
| No example-specific or `Returns` compatibility fix | Adopt | Such changes duplicate authority or hide the defect |

### 3.2 Findings accepted with a narrower implementation boundary

The audit lists a "TypeView-aware structural abstraction operation" as one
possible fix. That phrase is too broad to implement directly.

`TYPE_VIEW` preserves nominal declaration identity. Two declarations may have
the same erased Core shape while remaining distinct typed views. Therefore the
following rule is forbidden:

```text
replace every subterm convertible to index_term
```

General conversion does not identify which occurrence in a classifier was
introduced by a specific family index. It can also cross a nominal boundary
that Core substitution is deliberately unable to cross.

The accepted narrower rule is:

> Abstract only an occurrence connected to a constructor result index by the
> solved branch-refinement morphism and the same nominal family identity.

Conversion is then used to validate the reconstructed candidate, not to search
arbitrarily for replacement locations.

### 3.3 Claims that remain unproven

PR 24 correctly requires all-branch evidence, but all-branch validation alone
does not make motive synthesis unique. The equations

```text
M(indices_c, value_c) = classifier(branch_c)
```

are a restricted higher-order anti-unification problem. Multiple candidates
may satisfy the observed constructors, especially for incomplete or neutral
families.

The first implementation must therefore be intentionally restricted:

- candidates are produced only by certified branch refinement and inverse
  reindexing already available to the solver;
- no general higher-order unifier is introduced;
- zero candidates means residual or contradiction according to existing
  constraint state;
- multiple non-convertible candidates means residual ambiguity, never
  first-candidate selection; and
- a candidate becomes `READY` only after every currently required motive
  equation validates.

### 3.4 Checked-Core qualification

PR 24 asks the positive case to pass from-scratch checked-Core validation. That
is the correct target, but current v87 admits only a subset of v86 roots. This
plan does not silently equate v86 replay with v87 checking.

The implementation gate is:

1. the producer must construct a valid motive;
2. accepted P0 replay and v86 artifact round-trip must pass;
3. if the exact proof is admitted by current v87, checked-Core must pass;
4. otherwise the test records the precise unsupported checked-Core root and
   remains an explicit checked-fragment migration task.

No checker rule may be added merely to make this single test green.

## 4. Root Cause in the Current Code

### 4.1 Core exact replacement is doing its documented job

`prototype_term_graph_replace_exact` in
`src/prototype/src/core/term/substitution.inc` replaces only when:

```c
term_id == ctx->exact_term
```

Its TypeView rebuild context rebuilds parent wrappers after a child changes. It
does not and should not broaden the matching rule.

The Core API must remain exact and structural.

### 4.2 Motive seed loses dependency information

`operation_solver_seed_indexed_motive_from_branch` in
`src/prototype/src/frontend/lowering/graph_construction.inc`:

1. reconstructs the saturated constructor;
2. reads its family indices;
3. allocates one binder per index;
4. attempts exact replacement in the branch classifier;
5. wraps the result in lambdas; and
6. validates only by applying it back to the same constructor.

The routine does not record whether each intended replacement occurred. A
generated TypeView/core/source representation mismatch therefore yields an
unused output binder and a constant motive that passes the same-case check.

### 4.3 Existing branch refinement contains the missing authority

Each `prototype_typed_occurrence_match_case` already stores:

- its solved branch Context;
- `refinement_status`;
- `refinement_substitution` from refined Context to source branch Context;
- nominal constructor owner and ordinal; and
- exact case Binding identities.

This is the correct source of index correspondence. The mapping must be
projected into motive constraints. It must not be rediscovered by scanning
TermDB for convertible subterms.

### 4.4 Guardedness and equality are conflated

`branch_refinement_and_motives.inc` first runs ordinary conversion. On failure,
it validates a unique typed IH edge, owner, recursive argument, and guarded
occurrence, then overwrites the failed result with `EQUAL`.

The ownership check proves:

```text
this IH is legal evidence for this recursive edge
```

It does not prove:

```text
instantiate(candidate motive, recursive indices) == IH classifier
```

The overwrite must be deleted. If an equation requires recursive handling,
the motive solver must retain that equation explicitly and later validate its
instantiation; it may not relabel provenance as conversion.

## 5. Target Invariants

### I23-I1: One motive authority

For each Match occurrence, the classifier solver owns one motive solution
record. ConstraintDB owns equation lifecycle and result state. Typed occurrence
and Function Graph metadata are immutable inputs, not parallel motive stores.

### I23-I2: Equation-first synthesis

Every branch contributes an explicit equation:

```text
classifier(branch body)
  == M(constructor indices, constructor value)
```

A seed is only a candidate. It is not a solved motive.

### I23-I3: Certified index correspondence

An index occurrence is abstractable only when its correspondence is derived
from:

- the constructor result telescope;
- the solved branch Context and refinement substitution;
- exact Binding identities; and
- the same nominal type-family identity.

Erased Core shape equality and unrestricted conversion are insufficient.

### I23-I4: Observable dependency

Candidate construction returns a dependency mask for family indices and the
matched value occurrence. The mask distinguishes:

- dependency found;
- deliberately absent dependency;
- mapping unavailable; and
- conflicting correspondence.

An unused binder is not itself an error. An unavailable mapping prevents the
candidate from becoming final.

### I23-I5: Joint validation

A candidate becomes `READY` only when all required branch equations are
validated. A constant motive is accepted exactly when it satisfies all those
equations.

### I23-I6: Orthogonal recursive evidence

Guardedness validation and classifier equation validation produce separate
statuses and diagnostics. Both are required for a recursive IH branch.

### I23-I7: Replay remains independent

Accepted replay reconstructs the expected IH classifier from the stored motive
and compares it normally. It consumes neither candidate dependency masks nor
producer-only shortcuts.

### I23-I8: No new persistent format by default

Dependency masks and candidate correspondence are solver-local construction
data. If a valid ordinary motive Term and existing Derivation are sufficient
for replay, artifact v86 and checked v87 formats remain unchanged.

If implementation discovers that new evidence must be trusted after
publication, work stops for an explicit artifact schema/version decision.

## 6. Module Integration Decision

### 6.1 Concepts that should be integrated

Motive handling is currently split across:

- goal and solver storage in `context_and_type_lowering.inc`;
- seed construction in `graph_construction.inc`;
- refinement quoting and branch validation in
  `constraint/branch_refinement_and_motives.inc`;
- final materialization in `finalization_and_entrypoints.inc`; and
- authoritative validation in `accepted_replay.inc`.

The first four producer responsibilities form one coherent domain. They should
share one private motive-solver API and one solution record. Replay remains a
separate consumer and authority.

### 6.2 Proposed private module

Add:

```text
src/prototype/src/frontend/lowering/constraint/motive_solver.inc
```

It remains in the existing `lowering.c` translation unit because
`compile_context` is private. This is an ownership split, not a line-count-only
file split.

The module owns:

- motive equation collection views;
- certified index-correspondence extraction;
- candidate abstraction and inverse-refinement quoting;
- dependency-mask calculation;
- candidate joining and ambiguity detection;
- all-case validation;
- recursive motive-equation validation; and
- final motive materialization.

The module receives narrow immutable views of:

- TermDB structural readers;
- type schema;
- TypedOccurrenceGraph;
- ContextDB;
- SubstitutionDB; and
- the relevant ConstraintDB equation slice.

It mutates only its solver-owned solution records and ConstraintDB through the
existing lifecycle API.

### 6.3 Concepts that must remain separate

Do not merge:

- Core substitution with typed motive abstraction;
- Function Graph generation with indexed-Match motive solving;
- guarded-IH ownership with classifier conversion;
- producer motive solving with accepted replay;
- hand-written IADT rules with generated-graph special cases; or
- v86 replay with v87 checked-Core acceptance.

Function Graph generation may expose immutable origin correspondence where
necessary, but it must consume the same motive solver as every other indexed
family.

### 6.4 Data model correction

Replace the parallel motive arrays in `operation_classifier_solver` with one
record per Match occurrence, conceptually:

```c
struct operation_motive_solution {
    uint32_t constant_candidate;
    uint32_t motive_term;
    uint32_t first_equation;
    uint32_t equation_count;
    uint32_t dependency_mask;
    uint8_t state;
    uint8_t ambiguity;
};
```

The exact fields must be chosen after usage enumeration. The invariant is more
important than this provisional C layout:

- state and selected motive are changed together;
- equation lifecycle remains in ConstraintDB;
- symbolic IH applications refer to the owning Match solution;
- no second mutable solved classifier is added to TypedOccurrenceGraph; and
- diagnostic snapshots cannot feed the solver.

This consolidation is required because the Issue 23 fix otherwise adds more
side arrays to an already fragmented motive authority.

## 7. Implementation Phases

## I23-0: Freeze the Reproducer and Baseline

Status: [x] completed for the Issue 23 regression boundary

- [x] Copy the minimal generated `lengthOutputUnary` reproducer into a focused
      prototype integration fixture.
- [x] Add controls for constant motive plus IH, dependent motive without IH,
      and hand-written IADT dependent motive plus IH.
- [x] Record current expected failure separately from normal passing CI until
      the semantic correction lands.
- [x] Capture phase timing and Term/Context/Substitution counts.
- [x] Confirm no temporary debug output or Book checkout dependency remains.

Exit gate:

- [x] one repository-local command reproduces the exact P0 failure;
- [x] all controls have explicit expected outcomes; and
- [x] the test does not depend on numeric Term or Derivation IDs.

## I23-1: Trace and Freeze the Index Correspondence

Status: [x] completed with constructor-only typed/Core correspondence

- [x] For generated and hand-written indexed families, record the constructor
      result family, parameter spine, index spine, branch Context, and
      refinement substitution.
- [x] Determine whether generated Function Graph production violates an
      existing canonical TypeView construction invariant.
- [x] If two terms should be the same canonical TypeView by current authority,
      repair generation at the source and retain the typed abstraction checks.
- [x] If source/core dual representation is intentional, define an explicit
      typed index-correspondence view rather than forcing physical interning.
- [x] Assert matching nominal family identity and index ordinal.
- [x] Add a negative control using distinct nominal families with equal erased
      shape.

Exit gate:

- [x] the generated nil output occurrence maps to the output index binder;
- [x] the recursive output maps to `tailLength`;
- [x] Bool/Two-like equal Core shapes do not map across nominal identities; and
- [x] no generic Core API acquires TypeDB or conversion authority.

## I23-2: Implement Typed Index Abstraction

Status: [x] completed for constructor indices; general inverse reindexing deferred

- [x] Introduce the private motive-solver module.
- [x] Define a certified correspondence input containing family identity,
      index ordinal, source index term, refined occurrence, and Context morphism.
- [x] Reconstruct candidate motive bodies through inverse refinement/reindexing.
- [x] Return a dependency mask and explicit unavailable/conflict status.
- [x] Validate every candidate by reindexing or instantiating it back into the
      originating branch Context and running ordinary conversion.
- [x] Keep `prototype_term_graph_replace_exact` unchanged.
- [x] Remove direct indexed motive abstraction from
      `operation_solver_seed_indexed_motive_from_branch` after parity tests.

Exit gate:

- [x] `Unary Nat.zero` abstracts to `Unary output` for the generated nil case;
- [x] a truly constant classifier retains an empty dependency mask and remains
      a valid candidate;
- [x] a failed correspondence cannot silently create a vacuous final lambda;
- [x] round-trip validation catches an incorrect occurrence mapping; and
- [x] complexity is bounded by candidate classifier size plus mapped indices,
      not a database-wide conversion scan.

## I23-3: Make Motive Equations the Solver Authority

Status: [x] completed for the existing branch-equation solver

- [x] Inventory every read/write of the current motive arrays.
- [x] Introduce one `operation_motive_solution` record per owning Match.
- [x] Link every `OPERATION_CONSTRAINT_MOTIVE_EQUATION` to that owner and its
      branch equation.
- [x] Treat first-branch construction as `PROVISIONAL` only.
- [x] Join candidates from every available branch.
- [x] Accept convertible candidates; mark non-convertible multiple candidates
      residual/ambiguous with a precise reason.
- [x] Finalize `READY` only after all required equations validate.
- [x] Preserve budget interruption as `INCOMPLETE`, not logical rejection.
- [x] Update transaction rollback to restore the consolidated solution record.

Exit gate:

- [x] no branch-local helper directly writes a final motive state;
- [x] no first-candidate-wins path remains;
- [x] constant and dependent motives are selected by the same algorithm;
- [x] ConstraintDB is the only equation lifecycle authority; and
- [x] solver state has one selected motive/state owner.

## I23-4: Separate Guardedness from Equality

Status: [x] completed with separate recursive-equation evidence

- [x] Delete the path that assigns conversion `EQUAL` after only guarded-IH
      validation.
- [x] Retain `operation_solver_validate_guarded_motive_occurrence` as a
      provenance check.
- [x] Validate the recursive equation by instantiating the current motive at
      the recursive constructor indices.
- [x] Use ordinary conversion for the instantiated expected classifier and the
      typed IH classifier.
- [x] If direct unfolding would form a solver cycle, represent the recursive
      equation as guarded pending work and discharge it after candidate
      materialization; do not call it equal early.
- [x] Record guardedness and conversion outcomes separately in diagnostics.

Exit gate:

- [x] valid ownership plus incompatible classifier is rejected before replay;
- [x] foreign or unguarded IH is rejected independently;
- [x] valid recursive generated IH passes ordinary/replayable equation
      validation; and
- [x] accepted replay requires no weakening or producer-only status.

## I23-5: Complete Producer Module Consolidation

Status: [x] completed

- [x] Move candidate joining, quoted motive construction, all-case validation,
      and materialization into `motive_solver.inc`.
- [x] Leave branch Context/refinement construction in
      `branch_refinement_and_motives.inc` behind an immutable solved-refinement
      view.
- [x] Leave generic occurrence graph construction in
      `graph_construction.inc`.
- [x] Leave publication orchestration in `finalization_and_entrypoints.inc`.
- [x] Keep accepted replay code physically and logically independent.
- [x] Update `constraint_solver.inc` include order according to explicit
      dependencies.
- [x] Record per-file line additions/deletions and explain any net growth.

Exit gate:

- [x] each motive transition has one implementation owner;
- [x] cross-fragment helper dependencies are documented;
- [x] no duplicate candidate validation algorithm remains; and
- [x] line reduction is measured but never achieved by hiding distinct kernel
      rules behind an untyped generic callback.

## I23-6: Diagnostics

Status: [x] completed without making diagnostics a solver authority

- [x] Preserve source span from the graph Match/IH selector into motive errors.
- [x] Report graph owner, constructor selector, index ordinal, abstraction
      status, dependency mask, expected classifier, actual classifier,
      guardedness status, and conversion status.
- [x] Replace the terminal `0:0 failed to generate accepted function graph`
      message with the earlier semantic cause while retaining IDs as debug data.
- [x] Add assertions that diagnostics do not affect solver decisions.

Exit gate:

- [x] the negative incompatible-motive fixture identifies the source branch;
- [x] the message distinguishes missing correspondence from failed conversion;
      and
- [x] no diagnostic snapshot becomes mutable solver input.

## I23-7: Artifact, Checker, and Performance Validation

Status: [x] completed for frontend, P0 replay, and v86 artifacts

- [x] Pass frontend production and accepted P0 replay for the reproducer.
- [x] Pass v86 serialization, readback, relocation, and accepted replay.
- [x] Determine whether the proof root belongs to the admitted v87 fragment.
- [x] If admitted, pass from-scratch checked-Core validation with authority
      erasure tests enabled.
- [x] If not admitted, record the exact unsupported root and add a pending v87
      migration test without weakening the checker.
- [x] Verify no artifact schema change occurred unless separately versioned.
- [x] Compare focused compile time, solver steps, Term count, Context count, and
      Substitution count to the I23-0 baseline.
- [x] Reject any implementation that performs database-wide DefEq scans per
      branch or regresses the official Function Graph suite materially.

Exit gate:

- [x] all admitted artifact authorities agree;
- [x] producer-only metadata is absent from replay decisions;
- [x] no unexplained artifact version change exists; and
- [x] performance results are recorded in this document.

## 8. Permanent Test Matrix

| Case | Frontend | P0 replay | v86 round-trip | v87 checked Core |
| --- | --- | --- | --- | --- |
| Generated length, dependent output motive, recursive IH | pass | pass | pass | pass if admitted; otherwise explicit migration gate |
| Generated graph, dependent motive, no IH | pass | pass | pass | preserve current status |
| Generated graph, constant motive, IH | pass | pass | pass | preserve current status |
| Hand-written IADT, dependent motive, IH | pass | pass | pass | preserve current status |
| QuickSort, two output-dependent IHs | pass | pass | pass | pass if admitted |
| Incompatible recursive property | reject | not reached | not published | not published |
| Foreign IH ownership | reject | not reached | not published | not published |
| Equal Core shape, unrelated nominal TypeView | reject mapping | not reached | not published | not published |
| Vacuous seed contradicted by later branch | reject/residual before READY | not reached | not published | not published |

The QuickSort positive test should first use a small structural output-indexed
property. It must not wait for a complete `Sorted` theorem library.

## 9. Required Commands

Focused commands may be adjusted to final fixture names, but the completed
implementation must run at least:

```sh
make -f src/prototype/Makefile reader
sh src/prototype/tests/integration/test_function_graph_certified_execution.sh
sh src/prototype/tests/integration/test_explicit_index_family_surface.sh
sh src/prototype/tests/integration/test_constraint_authority.sh
sh src/prototype/tests/integration/test_artifact_flow.sh
```

Add a dedicated Issue 23 integration command rather than depending on the Book
checkout. Run broader tests only after focused semantic and replay gates pass.

## 10. Explicit Non-Fixes

- Do not change exact Core replacement into conversion-based replacement.
- Do not erase TypeView nominal identity to expose matching Core terms.
- Do not force every motive index binder to occur.
- Do not infer equality from IH ownership or guardedness.
- Do not disable or special-case accepted replay.
- Do not add generated global constructor/induction names.
- Do not duplicate Function Graph motive rules outside the indexed-Match solver.
- Do not reintroduce `Returns` or another user-forgeable bridge.
- Do not use `::` as bidirectional motive synthesis input.
- Do not publish candidate masks or solver history unless an independent
  artifact-schema decision proves they are semantic evidence.

## 11. Commit and Issue Workflow

Suggested implementation commits:

1. `test prototype generated graph dependent motive boundary`
2. `add typed indexed motive correspondence`
3. `make motive equations the solver authority`
4. `separate guarded IH validation from classifier equality`
5. `consolidate prototype motive solver module`
6. `validate generated dependent motives across artifacts`

After each phase:

- update its checklist and measured results in this document;
- comment on Issue 23 with the exact revision and tests;
- keep Issue 23 open while any required admitted boundary fails; and
- close Issue 23 only after the positive and negative matrix is complete or an
  explicitly excluded v87 migration item is separately tracked and justified.

## 12. First Implementation Record

Implementation revision: `e2ca3a5`

Artifact version decision: unchanged; accepted artifacts remain v86 and no
producer-only motive metadata is serialized

Issue 23 state at this revision: left open after the implementation commit. The concrete Book
reproducer is fixed, but the issue's broader v87, output-dependent QuickSort,
negative-provenance, and diagnostic acceptance items are not all complete.

Implementation status comment:
`https://github.com/repyt-margorp/a-program/issues/23#issuecomment-5438768732`

### Critical implementation result

The implementation does not claim a general higher-order anti-unifier. It
solves the observed constructor-index mismatch, retains ordinary constant
motives, and leaves neutral/non-constructor inverse reindexing unsupported.
The private `motive_solver.inc` currently owns the typed/Core constructor-shape
abstraction. Candidate joining and materialization remain in their existing
lowering fragments, so I23-5 is intentionally partial rather than a physical
move made only to satisfy the original module sketch.

The old semantic conflation was removed: a recursive equation may be accepted
from the exact IH equation that produced the candidate plus guardedness and
successful reapplication, but this fact is no longer stored in or reported as
`PROTOTYPE_TERM_CONVERSION_EQUAL`. Accepted replay still reconstructs the
published classifier independently.

### File change accounting

| File | Before | Added | Deleted | After | Reason |
| --- | ---: | ---: | ---: | ---: | --- |
| `constraint/motive_solver.inc` | 0 | 226 | 0 | 226 | Constructor-only certified index abstraction |
| `constraint/branch_refinement_and_motives.inc` | 4247 | 59 | 72 | 4234 | Joint validation and separate recursive evidence |
| `constraint/effect_propagation_and_residuals.inc` | 1710 | 2 | 2 | 1710 | Consolidated solution access |
| `constraint/model_generation_and_index.inc` | 1078 | 6 | 4 | 1080 | Consolidated solution reset |
| `constraint_solver.inc` | 8 | 1 | 0 | 9 | Private module inclusion |
| `context_and_type_lowering.inc` | 5884 | 25 | 4 | 5905 | One motive solution record per Match |
| `finalization_and_entrypoints.inc` | 2837 | 55 | 38 | 2854 | Reuse solved indexed motive; unary fallback only |
| `graph_construction.inc` | 12440 | 112 | 52 | 12500 | Candidate lifecycle and exact IH provenance |
| `test_function_graph_certified_execution.sh` | 550 | 27 | 0 | 577 | Permanent Issue 23 and nominal controls |
| `function_graph_dependent_output_ih_check.p` | 0 | 40 | 0 | 40 | Focused positive regression fixture |
| `function_graph_nominal_index_constant_motive_check.p` | 0 | 63 | 0 | 63 | Equal-shape nominal/constant control |

### Performance accounting

| Scenario | Before | After | Delta | Result |
| --- | ---: | ---: | ---: | --- |
| Book generated length proof | rejected at P0 | 30 ms | now accepted | pass |
| Official Function Graph suite | not recorded | 7.8 s | no comparable total | pass |
| QuickSort dependency closure | 7189 ms | 6677 ms | -7.1% | pass; no regression |

### Final verification

- [x] focused positive matrix passes;
- [x] nominal/constant control passes without cross-TypeView abstraction;
- [x] accepted replay remains unchanged in strength;
- [x] accepted v86 artifact boundary is verified and unchanged;
- [x] no broad Core semantic replacement was introduced;
- [x] motive storage has one solver-owned solution record per Match;
- [x] performance and line-count deltas are recorded;
- [x] Issue 23 has an implementation-status comment with remaining closure gates; and
- [x] implementation is committed and pushed to `main`.

## 13. 2026-08-28 Follow-up Implementation Record

The first implementation fixed the minimal single-IH failure, but the wider
matrix exposed three remaining producer defects:

1. one Match stored only one `recursive_equation_operation`, so a second IH
   could replace or hide the first recursive equation;
2. replacing an IH classifier from the selected motive did not invalidate all
   downstream classifier consequences; and
3. an incompatible recursive property could survive producer solving and fail
   only at accepted replay.

The follow-up corrects those defects without weakening Core conversion or
accepted replay.

### Authority and module result

- `operation_motive_solution` remains the single selected-motive authority per
  Match.
- `recursive_equation_owners[ih]` records independent provenance for every
  exact IH occurrence. It is not a conversion result and supports multiple IHs
  for one Match.
- `ih_projected_motives[ih]` is a derived cache only. A cache hit is valid only
  while the IH classifier is present and was projected from the same motive.
- ConstraintDB remains the lifecycle authority for each branch motive
  equation. Cached validation is keyed by motive, actual classifier, Context,
  refinement substitution, and recursive-equation revision.
- `motive_solver.inc` owns branch-index correspondence, refinement quotation,
  candidate construction and joining, all-equation validation, recursive
  provenance, materialization, IH projection, rebuild transactions, and
  terminal motive diagnostics.
- branch Context construction and fixed-point scheduling remain in
  `branch_refinement_and_motives.inc`; occurrence construction and motive
  equation registration remain in `graph_construction.inc`. These are inputs
  to motive solving, not parallel motive implementations.
- use-site effect-row instantiation is shared by producer propagation and the
  kernel computation-fold rule. The generalized premise classifier is not
  overwritten.

The key rule is now:

```text
guarded recursive ownership
  + projection of the selected motive to every exact IH
  + recomputation of each downstream branch classifier
  + validation of every branch motive equation
  ------------------------------------------------------
  accepted recursive motive equation
```

The conclusion is a recursive equation premise, not
`PROTOTYPE_TERM_CONVERSION_EQUAL`. Ordinary DefEq remains unchanged.

### Permanent coverage added

- generated `length` graph with output-dependent motive and one recursive IH;
- generated `mirror` graph with an output-dependent structural family and two
  recursive IHs;
- generated `quickSortAcc` graph with an output-indexed structural derivation
  consuming both recursive IHs;
- incompatible recursive output property rejected by the frontend with
  `motive-equation-mismatch` before P0 replay;
- constant and nominally distinct equal-Core-shape controls;
- v86 write/read for both one-IH and two-IH positive fixtures; and
- a dedicated `test_issue_23_dependent_motive.sh` authority-boundary audit.

`QuickSortOutputTree output` is deliberately a small graph-structural property:
its recursive constructor requires witnesses for both recursive outputs and
produces a witness indexed by the final output. It tests the compiler rule
without pretending to be `Sorted` or permutation. Those theorems require an
object-level property library and must not become QuickSort-only compiler rules.

### Checked-Core boundary

The proof functions themselves pass producer elaboration, accepted P0 replay,
v86 artifact round-trip, and from-scratch v87 checked Core. The earlier v87
failure was misdiagnosed as an unsupported Function Graph APP role. It was an
outer test wrapper whose Match result leaked a branch-local `output` binder
without existential packaging; the checker correctly rejected it. Focused
fixtures now validate the admitted proof roots without weakening the checker.

The repository-wide checked-Core example command still stops earlier at the
pre-existing `explicit_index_family_acc_concrete_check.p` root
(`status=3 reason=4`). That independent checked-fragment issue is not used as
an exception for Issue 23.

### Follow-up performance

| Scenario | Planning baseline | First implementation | Follow-up | Result |
| --- | ---: | ---: | ---: | --- |
| QuickSort dependency closure | 7189 ms | 6677 ms | 6480 ms | no regression |
| Dedicated checked-Core/one/two-IH/QuickSort/negative matrix | absent | absent | 1.9 s | pass |

Timings are wall-clock observations from one local run and are regression
guards, not deterministic artifact data.

### Follow-up line accounting

Relative to `9d685b0`, tracked implementation and existing-test files add 1406
lines and delete 750 lines. Two new fixtures/scripts add 112 lines, for a total
of 1518 additions, 750 deletions, and a net increase of 768 lines, excluding
this document. Most growth is the explicit typed correspondence, validation
cache keys, multi-IH provenance, diagnostics, and permanent tests; 318 lines of
the old branch-local motive path and 63 lines of finalization fallback were
removed.

### Follow-up verification

- [x] `make -f src/prototype/Makefile reader`;
- [x] `test_issue_23_dependent_motive.sh`;
- [x] `test_function_graph_certified_execution.sh`;
- [x] `test_explicit_index_family_surface.sh`;
- [x] `test_constraint_authority.sh`;
- [x] `test_context_substitution_immutability.sh`;
- [x] `test_artifact_flow.sh`;
- [x] no Issue 23 temporary motive/replay trace was added;
- [x] accepted replay remains independent and unchanged in strength;
- [x] focused generated Function Graph proof roots pass v87 checked Core;
- [x] QuickSort consumes two output-dependent IHs through a structural
      output-indexed family; Sorted/permutation remain object-library work.

## 14. Final Closure Record

The final module pass moved candidate abstraction/joining, all-case motive
construction, recursive validation, materialization, rebuild transactions, and
IH projection into `constraint/motive_solver.inc`. Context refinement remains
in `branch_refinement_and_motives.inc`; graph construction only registers the
motive equations. This is a physical ownership correction with no algorithmic
or artifact-format change.

### Physical module accounting

Relative to `952a073`:

| File | Before | After | Delta |
| --- | ---: | ---: | ---: |
| `graph_construction.inc` | 12535 | 11312 | -1223 |
| `branch_refinement_and_motives.inc` | 4067 | 1971 | -2096 |
| `motive_solver.inc` | 743 | 4063 | +3320 |
| `constraint_solver.inc` | 9 | 9 | 0 |
| `context_and_type_lowering.inc` | 6030 | 6044 | +14 |

The five implementation modules therefore grow by only 15 physical lines; the
large add/delete count is code movement into the correct authority module.

| File | Git additions | Git deletions |
| --- | ---: | ---: |
| `branch_refinement_and_motives.inc` | 13 | 2109 |
| `motive_solver.inc` | 3886 | 566 |
| `constraint_solver.inc` | 1 | 1 |
| `context_and_type_lowering.inc` | 14 | 0 |
| `graph_construction.inc` | 0 | 1223 |
| `checked_core_examples_check.c` | 61 | 3 |
| `test_issue_23_dependent_motive.sh` | 59 | 0 |
| two focused checked-Core fixtures | 54 | 0 |
| this plan document | 186 | 112 |

Including documentation and tests, the closure change has 4274 additions and
4014 deletions, net +260 lines. The two focused checked-Core fixtures account
for 54 of those added lines.

### Focused resource record

For `function_graph_dependent_output_ih_check.p`:

| Resource | Final value |
| --- | ---: |
| TermDB terms | 1026 |
| Typed occurrences | 129 |
| Contexts | 100 |
| Substitutions | 181 |
| Constraints solved | 200 / 200 |
| Total effort | 12224 |
| Motive effort | 13 |

The final QuickSort dependency-closure phase completed in 6447 ms in the final
local run, compared with the 7189 ms planning baseline. The dedicated Issue 23
matrix completed in about 1.9 seconds including compilation of the focused v87
checker.

### Final boundary result

- producer elaboration, P0 replay, v86 round-trip, and focused v87 checking pass;
- dependent one-IH, two-IH, nominal, constant, incompatible, and QuickSort
  structural cases are permanent tests;
- artifact schema remains v86 and checked module format remains v87;
- no diagnostic or producer-only motive metadata participates in replay; and
- the broader checked-Core suite still stops at the pre-existing
  `explicit_index_family_acc_concrete_check.p` root (`reason=4`), independently
  of Issue 23.
