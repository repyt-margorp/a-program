# Effect-Row Constraint Migration

Date: 2026-07-17

Status: implemented and verified.

This document is the live implementation plan for removing the overloaded
meaning of the empty effect row. Update the status and findings here whenever
the implementation changes the plan.

## Problem

The current TermDB representation uses:

```text
Comp({}, A)
```

for two different meanings:

1. an exact pure computation returning `A`; and
2. a surface function annotation whose latent effect row was omitted.

`classifier_kernel_compatible_at_depth()` compensates for this by treating an
empty expected row as a wildcard. That makes the meaning of one closed
classifier depend on whether it appears on the expected or actual side of a
compatibility check.

The target invariant is:

```text
Comp({}, A)   means exactly pure
Comp(?E, A)   means an unsolved effect-row constraint
Comp(E, A)    means the solved row E
```

An unsolved `?E` is compiler constraint state. It must not be published as a
closed JudgementDB conclusion.

## Existing Facilities

The prototype already contains most of the required representation:

- `EFFECT_ROW_VAR` represents symbolic rows.
- `EFFECT_ROW_FORALL` represents generalized latent rows.
- `EFFECT_ROW_UNION` represents row composition.
- `prototype_judgement_effect_row_equation` records `UNION` and `RESIDUAL`
  equations.
- BIND, operation request, and HANDLE already generate some row equations.
- closed rows are solved as capability bitsets.

The migration therefore does not introduce another effect type system. It
connects these existing pieces into the source-operation classifier pipeline.

## Target Pipeline

### 1. Surface elaboration

When a surface arrow omits its latent effect row, allocate a fresh row
metavariable:

```text
A -> B
  elaborates as
Pi(A, Comp(?E, B))
```

An explicit `RETURN` still synthesizes `Comp({}, A)`. Empty rows are never
used as omitted-row markers.

The row metavariable is scoped by the source operation or expectation that
introduced it. Higher-order binder annotations may generalize it with the
existing `EFFECT_ROW_FORALL` mechanism.

### 2. Constraint generation

Each computation-producing OperationGraph node contributes row equations:

```text
RETURN(V)             Eresult = {}
APP(F, V)             Eresult = Ecodomain
BIND(M, K)            Eresult = Em union Ek
PERFORM(OP, K)         Eresult = Eop union Ek
MATCH(S, cases...)     Eresult = union(Ecase...)
HANDLE(H, M)           Eres = Ein residual Ehandled
                       Eresult = Ehandler_out union Eres
```

Lambda does not add an effect by itself. Its codomain row is the solved row of
its body computation.

### 3. Propagation

Classifier shape and result type may become known before the row. The solver
must retain:

```text
Comp(?E, A)
```

and propagate equations until `?E` has a closed or generalized solution.
Classifier and row propagation are mutually staged, but row solving remains a
finite-set dataflow problem for closed rows.

This is not a pipeline in which the entire ordinary classifier is solved and
effects are attached afterward. `Comp(E, A)` is itself a classifier, so APP,
ascription, higher-order arguments, and handler typing can expose more
classifier shape after a row equation is solved. The implementation therefore
runs classifier propagation and row propagation in the same fixed-point
phase, while keeping their constraint domains separate.

### 4. Row solving

Closed rows use:

```text
UNION:     result = left | right
RESIDUAL:  handled subset of input
           result = input & ~handled
```

Symbolic equations remain explicit. They must not be approximated by `{}`.
Row variables with a legitimate function scope may be generalized. Other
unresolved rows become incomplete/residual compiler outcomes according to the
compile policy.

### 5. Policy and artifact

- strict policy rejects every unresolved effect-row equation;
- hybrid/exploratory policy may emit an unresolved row only when the artifact
  carries the equation and enough qualified graph references to resume or
  validate it after linking;
- an unresolved row is not a runtime effect execution obligation;
- artifact loading and linking must preserve and relocate row equations;
- backend compatibility uses solved rows plus explicitly preserved residual
  rows, never an empty-row approximation.

The artifact format will be revised in this migration. No backward-compatible
reader for the previous prototype format will be retained.

### 6. Kernel compatibility

Remove the rule:

```text
expected Comp({}, A) accepts Comp(E, A)
```

Compatibility of computation classifiers requires compatible result
classifiers and normalization-equal effect rows. Solver metavariables are
resolved before this kernel check; they are not wildcard kernel terms.

### 7. Judgement materialization

JudgementDB receives only classifiers whose rows are:

- closed;
- explicitly generalized by `EFFECT_ROW_FORALL`; or
- represented by a supported residual judgement outside JudgementDB.

The proof materializer must reject a `HAS_TYPE` conclusion containing a free,
unsolved row metavariable.

## Implementation Sequence

1. Add row-variable ownership and row-equation storage to compile metadata.
2. Change surface arrow/expectation elaboration to allocate row variables.
3. Generate occurrence-level equations for all computation OperationGraph
   nodes.
4. Replace the current closed-row checker with worklist propagation that can
   bind row metavariables and retain symbolic equations.
5. Remove empty-row wildcard compatibility.
6. Gate proof materialization on solved/generalized rows.
7. Serialize, read, validate, relocate, and link residual row equations.
8. Add positive and negative tests for pure, effectful, higher-order,
   BIND/MATCH/HANDLE, strict policy, and artifact round trips.

## Progress Log

### 2026-07-17

- Inspected the current source annotation, classifier solver, computation
  constraint, effect equation, and artifact paths.
- Confirmed that BIND, operation request, and HANDLE already construct row
  unions/residuals.
- Confirmed that the current effect equation solver only validates fully
  closed equations; it does not bind symbolic row variables.
- Confirmed that ordinary surface arrows still create exact empty rows in
  several elaboration paths, while higher-order lambda binder arrows already
  use scoped row variables.
- Confirmed that artifact v44 stores only aggregate solver counts, not the
  actual unresolved effect-row equations.
- Selected a no-backward-compatibility artifact migration.
- Removed the kernel rule that treated an expected empty row as a wildcard.
  Kernel compatibility now compares computation rows exactly after
  normalization.
- Surface function results and computation-position value annotations now
  elaborate omitted effects as fresh `EFFECT_ROW_VAR` terms.
- Added an elaboration solver that instantiates free expected row variables
  from the corresponding rows of an inferred classifier before kernel
  compatibility.
- Moved ascription row solving into the OperationGraph fixed point. Solving it
  only in the final ascription-check phase left APP proof premises referring to
  the old unsolved classifier.
- Removed independent classifier seeding for `NAME` operations. A `NAME`
  classifier now follows its referenced source operation through the existing
  equality constraint; otherwise the name and its ascription were competing
  authoritative sources.
- Added the first JudgementDB materialization gate for effect rows. A row
  variable is acceptable when it is owned by an enclosing Lambda operation's
  `implicit_effect_row_binders`, or when it has already been generalized by
  `EFFECT_ROW_FORALL`. An unowned row is not materialized as a proof
  conclusion.
- Restricted the unowned-row completeness check to operations reachable from
  exported `NAME` roots. Lowering currently leaves some unreachable trial
  occurrences in compile metadata; they must not become artifact or proof
  obligations.
- Distinguished externally declared function rows from ordinary unsolved
  annotation rows. A declaration such as `idNat : Nat -> Nat` owns a residual
  latent row because no implementation is available locally. It must be
  preserved for artifact/link validation rather than solved to `{}` or
  incorrectly generalized through the higher-order argument specialization
  path.
- Verified `examples/01` through `examples/09`, CBPV surface and boundary
  tests, and artifact flow after this stage.
- Added occurrence-level `EXACT`, `COPY`, `UNION`, and `RESIDUAL` effect
  constraints to compile metadata. Every operation whose solved classifier is
  a returning computation receives a constraint record.
- `RETURN` produces an exact empty row. BIND and operation requests produce
  set union equations. HANDLE imports the existing residual equation and
  solves it as set difference after checking that the handled row is a subset
  of the input row. Match combines branch rows by set union.
- The row worklist substitutes a closed solution for an `EFFECT_ROW_VAR`
  through OperationGraph classifiers, classifier-solver bindings, and pending
  row equations. Symbolic equations remain residual instead of becoming an
  empty row.
- Added artifact v45 serialization, validation, graph marking, relocation, and
  linking for occurrence-level effect constraints. The previous artifact
  version is intentionally rejected.
- strict policy rejects every non-solved effect constraint. Hybrid policy
  preserves unresolved constraints in the artifact.
- Externally declared functions with omitted latent effects no longer create
  closed declaration or APP proofs. Their row state remains in OperationGraph
  constraints; strict compilation rejects it.
- Judgement proof validation rejects an unresolved result row unless the proof
  depends on a binder assumption that scopes that row. This preserves valid
  higher-order local derivations while preventing a residual top-level
  classifier from being published as a closed proof.
- Added regression coverage for exact-pure versus Terminal compatibility,
  expected-row solving, exact RETURN constraints, operation-request UNION,
  handler RESIDUAL, strict rejection, and hybrid artifact round trips.
- Reverified examples `01` through `09`, all current training programs, shared
  core occurrences, CBPV surface/boundary tests, and artifact flow.

### Surface-arrow boundary

The source uses arrow syntax in two semantically distinct contexts:

- a term-level computation arrow with omitted effects elaborates to a fresh
  row variable;
- a type-family or constructor-schema arrow used by kernel-level type
  computation remains exact pure.

For example, the `B : A -> @` parameter of the user-defined Sigma type is a
pure type family, not a runtime function with an omitted effect annotation.
Changing every type-expression arrow to a fresh row breaks this distinction
and changes imported Sigma formation. The implementation therefore allocates
fresh rows at term-level function-result and higher-order binder elaboration
boundaries, while preserving exact-pure arrows in the type-family/schema
compiler.

### Remaining cleanup

Operation-level constraints currently import BIND/PERFORM/HANDLE row equations
from `JudgementDelta` by matching their core subject. This is correct for the
current terms and covered by shared-core tests, but it leaves two generation
paths. A later cleanup should pass the OperationGraph occurrence id directly
when those equations are created so that `JudgementDelta` is not used as an
intermediate occurrence lookup.

OperationGraph also retains some unreachable trial occurrences after
elaboration succeeds. They are excluded from proof materialization, but a
future graph-construction cleanup should avoid retaining them.

## Completion Gates

- `Comp({}, A)` fails compatibility with `Comp({Terminal}, A)`.
- An omitted source arrow accepts an effectful implementation by solving
  `?E = {Terminal}`, not by wildcard compatibility.
- An explicit or internally exact pure classifier remains exact.
- BIND, Match, operation request, and HANDLE produce reproducible row results.
- strict mode rejects unsupported unresolved rows.
- hybrid/exploratory artifacts preserve supported residual row equations.
- artifact read/link/write round trips preserve row equation identity.
- JudgementDB validation rejects free unsolved row variables.
- examples `01` through `09`, `training/double.p`, CBPV tests, and artifact
  flow tests pass.
