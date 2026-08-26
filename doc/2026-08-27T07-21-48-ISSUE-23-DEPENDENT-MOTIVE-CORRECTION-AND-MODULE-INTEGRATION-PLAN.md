# Issue 23 Dependent Motive Correction and Module Integration Plan

Date: 2026-08-27 JST

Status: planned; Issue 23 is reproduced and remains open. PR 24 is merged. No
semantic implementation change has been made by this plan.

Issue: #23, `Bug: generated Function Graph dependent motives become constant
and fail recursive IH replay`

Merged audit PR: #24, `Document generated Function Graph dependent-motive
replay failure`

Merged revision: `f71e61c04d8d159f076a1698eb301d8845918db5`

Source audit:

- `2026-08-27T00-40-19-GENERATED-FUNCTION-GRAPH-DEPENDENT-MOTIVE-IH-REPLAY-AUDIT.md`

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

Status: [ ] pending

- [ ] Copy the minimal generated `lengthOutputUnary` reproducer into a focused
      prototype integration fixture.
- [ ] Add controls for constant motive plus IH, dependent motive without IH,
      and hand-written IADT dependent motive plus IH.
- [ ] Record current expected failure separately from normal passing CI until
      the semantic correction lands.
- [ ] Capture phase timing and Term/Context/Substitution counts.
- [ ] Confirm no temporary debug output or Book checkout dependency remains.

Exit gate:

- [ ] one repository-local command reproduces the exact P0 failure;
- [ ] all controls have explicit expected outcomes; and
- [ ] the test does not depend on numeric Term or Derivation IDs.

## I23-1: Trace and Freeze the Index Correspondence

Status: [ ] pending

- [ ] For generated and hand-written indexed families, record the constructor
      result family, parameter spine, index spine, branch Context, and
      refinement substitution.
- [ ] Determine whether generated Function Graph production violates an
      existing canonical TypeView construction invariant.
- [ ] If two terms should be the same canonical TypeView by current authority,
      repair generation at the source and retain the typed abstraction checks.
- [ ] If source/core dual representation is intentional, define an explicit
      typed index-correspondence view rather than forcing physical interning.
- [ ] Assert matching nominal family identity and index ordinal.
- [ ] Add a negative control using distinct nominal families with equal erased
      shape.

Exit gate:

- [ ] the generated nil output occurrence maps to the output index binder;
- [ ] the recursive output maps to `tailLength`;
- [ ] Bool/Two-like equal Core shapes do not map across nominal identities; and
- [ ] no generic Core API acquires TypeDB or conversion authority.

## I23-2: Implement Typed Index Abstraction

Status: [ ] pending

- [ ] Introduce the private motive-solver module.
- [ ] Define a certified correspondence input containing family identity,
      index ordinal, source index term, refined occurrence, and Context morphism.
- [ ] Reconstruct candidate motive bodies through inverse refinement/reindexing.
- [ ] Return a dependency mask and explicit unavailable/conflict status.
- [ ] Validate every candidate by reindexing or instantiating it back into the
      originating branch Context and running ordinary conversion.
- [ ] Keep `prototype_term_graph_replace_exact` unchanged.
- [ ] Remove direct indexed motive abstraction from
      `operation_solver_seed_indexed_motive_from_branch` after parity tests.

Exit gate:

- [ ] `Unary Nat.zero` abstracts to `Unary output` for the generated nil case;
- [ ] a truly constant classifier retains an empty dependency mask and remains
      a valid candidate;
- [ ] a failed correspondence cannot silently create a vacuous final lambda;
- [ ] round-trip validation catches an incorrect occurrence mapping; and
- [ ] complexity is bounded by candidate classifier size plus mapped indices,
      not a database-wide conversion scan.

## I23-3: Make Motive Equations the Solver Authority

Status: [ ] pending

- [ ] Inventory every read/write of the current motive arrays.
- [ ] Introduce one `operation_motive_solution` record per owning Match.
- [ ] Link every `OPERATION_CONSTRAINT_MOTIVE_EQUATION` to that owner and its
      branch equation.
- [ ] Treat first-branch construction as `PROVISIONAL` only.
- [ ] Join candidates from every available branch.
- [ ] Accept convertible candidates; mark non-convertible multiple candidates
      residual/ambiguous with a precise reason.
- [ ] Finalize `READY` only after all required equations validate.
- [ ] Preserve budget interruption as `INCOMPLETE`, not logical rejection.
- [ ] Update transaction rollback to restore the consolidated solution record.

Exit gate:

- [ ] no branch-local helper directly writes a final motive state;
- [ ] no first-candidate-wins path remains;
- [ ] constant and dependent motives are selected by the same algorithm;
- [ ] ConstraintDB is the only equation lifecycle authority; and
- [ ] solver state has one selected motive/state owner.

## I23-4: Separate Guardedness from Equality

Status: [ ] pending

- [ ] Delete the path that assigns conversion `EQUAL` after only guarded-IH
      validation.
- [ ] Retain `operation_solver_validate_guarded_motive_occurrence` as a
      provenance check.
- [ ] Validate the recursive equation by instantiating the current motive at
      the recursive constructor indices.
- [ ] Use ordinary conversion for the instantiated expected classifier and the
      typed IH classifier.
- [ ] If direct unfolding would form a solver cycle, represent the recursive
      equation as guarded pending work and discharge it after candidate
      materialization; do not call it equal early.
- [ ] Record guardedness and conversion outcomes separately in diagnostics.

Exit gate:

- [ ] valid ownership plus incompatible classifier is rejected before replay;
- [ ] foreign or unguarded IH is rejected independently;
- [ ] valid recursive generated IH passes ordinary/replayable equation
      validation; and
- [ ] accepted replay requires no weakening or producer-only status.

## I23-5: Complete Producer Module Consolidation

Status: [ ] pending

- [ ] Move candidate joining, quoted motive construction, all-case validation,
      and materialization into `motive_solver.inc`.
- [ ] Leave branch Context/refinement construction in
      `branch_refinement_and_motives.inc` behind an immutable solved-refinement
      view.
- [ ] Leave generic occurrence graph construction in
      `graph_construction.inc`.
- [ ] Leave publication orchestration in `finalization_and_entrypoints.inc`.
- [ ] Keep accepted replay code physically and logically independent.
- [ ] Update `constraint_solver.inc` include order according to explicit
      dependencies.
- [ ] Record per-file line additions/deletions and explain any net growth.

Exit gate:

- [ ] each motive transition has one implementation owner;
- [ ] cross-fragment helper dependencies are documented;
- [ ] no duplicate candidate validation algorithm remains; and
- [ ] line reduction is measured but never achieved by hiding distinct kernel
      rules behind an untyped generic callback.

## I23-6: Diagnostics

Status: [ ] pending

- [ ] Preserve source span from the graph Match/IH selector into motive errors.
- [ ] Report graph owner, constructor selector, index ordinal, abstraction
      status, dependency mask, expected classifier, actual classifier,
      guardedness status, and conversion status.
- [ ] Replace the terminal `0:0 failed to generate accepted function graph`
      message with the earlier semantic cause while retaining IDs as debug data.
- [ ] Add assertions that diagnostics do not affect solver decisions.

Exit gate:

- [ ] the negative incompatible-motive fixture identifies the source branch;
- [ ] the message distinguishes missing correspondence from failed conversion;
      and
- [ ] no diagnostic snapshot becomes mutable solver input.

## I23-7: Artifact, Checker, and Performance Validation

Status: [ ] pending

- [ ] Pass frontend production and accepted P0 replay for the reproducer.
- [ ] Pass v86 serialization, readback, relocation, and accepted replay.
- [ ] Determine whether the proof root belongs to the admitted v87 fragment.
- [ ] If admitted, pass from-scratch checked-Core validation with authority
      erasure tests enabled.
- [ ] If not admitted, record the exact unsupported root and add a pending v87
      migration test without weakening the checker.
- [ ] Verify no artifact schema change occurred unless separately versioned.
- [ ] Compare focused compile time, solver steps, Term count, Context count, and
      Substitution count to the I23-0 baseline.
- [ ] Reject any implementation that performs database-wide DefEq scans per
      branch or regresses the official Function Graph suite materially.

Exit gate:

- [ ] all admitted artifact authorities agree;
- [ ] producer-only metadata is absent from replay decisions;
- [ ] no unexplained artifact version change exists; and
- [ ] performance results are recorded in this document.

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

## 12. Completion Record

Implementation revision: pending

Artifact version decision: pending

Issue 23 state: open

### File change accounting

| File | Before | Added | Deleted | After | Reason |
| --- | ---: | ---: | ---: | ---: | --- |
| pending | - | - | - | - | Filled after implementation |

### Performance accounting

| Scenario | Before | After | Delta | Result |
| --- | ---: | ---: | ---: | --- |
| Focused generated length proof | pending | pending | pending | pending |
| Official Function Graph suite | pending | pending | pending | pending |
| QuickSort dependent-property control | pending | pending | pending | pending |

### Final verification

- [ ] focused positive matrix passes;
- [ ] focused negative matrix rejects for the intended reason;
- [ ] accepted replay remains unchanged in strength;
- [ ] artifact/checker boundary is documented accurately;
- [ ] no broad Core semantic replacement was introduced;
- [ ] module authority audit passes;
- [ ] performance and line-count deltas are recorded;
- [ ] Issue 23 has a complete resolution comment; and
- [ ] implementation is committed and pushed to `main`.
