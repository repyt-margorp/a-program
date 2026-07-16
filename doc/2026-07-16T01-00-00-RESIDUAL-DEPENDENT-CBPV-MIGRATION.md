# Residual Dependent CBPV Verification Migration Plan

Date: 2026-07-16

Status: living design and migration plan.  This document records an intended
architecture, not an implemented language guarantee.  Later work may revise
the plan, but a revision must state which invariant or decision changed.

## Purpose

A Program treats type checking, conversion, and equality checking as compiler
calculation over the program graph.  The compiler may perform pure reduction
before producing an artifact.  It must not perform terminal, stateful,
nondeterministic, or otherwise effectful operations merely to decide a
classifier.

The target system permits two outcomes for a typing problem:

```text
closed judgement
  The compiler has a complete kernel derivation under a stated pure reduction
  profile.

residual judgement
  The program is structurally well formed, but a classifier condition depends
  on a value produced by a future CBPV computation.  The artifact carries a
  runtime verification obligation for that computation occurrence.
```

Contradictory constraints, malformed terms, invalid handlers, and failed
runtime verification remain errors.  A residual judgement is not a proof and
must never be treated as a closed `HAS_TYPE` relation.

This makes artifact verification coverage a property of the produced artifact.
Two builds of the same source may contain different amounts of completed pure
normalization and different residual obligations.  Their program graph must
have the same meaning; only their recorded proof and verification frontier may
differ.

## Non-Goals

- Do not add Sigma as a kernel primitive.  Sigma is planned as an ordinary
  user-defined inductive type with a dependent constructor classifier.
- Do not add `RESULT_OF(m)` as an ordinary `A`-valued TermDB node.  For an
  effectful computation, that would conflate a static shared graph node with a
  particular dynamic execution occurrence.
- Do not add arbitrary user `Eq` proofs to kernel conversion.
- Do not execute host effects in the classifier kernel.
- Do not retain compatibility paths for the current closed-only compiler once
  the replacement is complete.

## Existing Architecture

The current prototype has useful foundations but is closed-only.

### TermDB and normalization

`src/prototype/term.h` and `src/prototype/term.c` already distinguish:

```text
CORE_WHNF
COMPUTATION_WHNF
PURE_TYPE_WHNF
```

and cache complete normal forms by `(term_id, profile, graph_revision)`.
`PURE_TYPE_WHNF` excludes host effect dispatch.  This is the intended kernel
conversion boundary.

The evaluator currently reports only success or failure.  A depth limit,
invalid graph, blocked effect, and unsatisfied runtime operation are not
represented as distinct outcomes.  The cache cannot represent a partial or
budget-exhausted result.

### Source operation graph

`src/prototype/ast.c` creates `prototype_operation_node` records while
lowering.  They represent typed source occurrences and prevent a shared core
TermDB node from being assigned one arbitrary classifier when different
TypeView occurrences select different classifiers.

However, `prototype_compile_metadata.operations` is compiler-local.  It is not
part of an artifact.  This is sufficient for closed compilation but not for a
runtime obligation, because a residual BIND condition belongs to a source
occurrence, not merely to a structurally shared core BIND node.

### JudgementDB and constraints

`src/prototype/judgement.h` stores closed `HAS_TYPE` and `IS_TYPE` relations
with proof DAGs.  The temporary `prototype_judgement_delta` stores CBPV and
effect-row constraints while compiling.

`src/prototype/ast.c` currently rejects a compilation if pending classifier
state remains after the fixed-point process.  `PROTOTYPE_JUDGEMENT_KIND_UNKNOWN`
means an absent relation slot; it is not an appropriate representation for a
runtime condition.

### Current BIND restriction

`src/prototype/typing.c:bind_result_classifier` accepts:

```text
M : Comp(E, A)
K : Pi(A, Comp(F, B))
----------------------
BIND(M, K) : Comp(E union F, B)
```

It rejects `B` and `F` when they contain the continuation Pi binder.  The
restriction prevents a binder from escaping after BIND, but it also prevents a
dependent continuation classifier from being represented as a residual
condition.

### Artifact

Artifact format v37 serializes interface data, sparse TermDB/type-declaration
graphs, JudgementDB proofs, universe constraints, debug data, and relocations.
It does not serialize source operation occurrences, solver constraints,
normalization evidence, or unresolved verification work.

The migration upgrades this boundary to artifact format v38.  It adds an
`operation_graph` section containing source operation occurrences, their child
edges, selected classifiers, and `VerificationDB` entries.  Sparse graph root
marking includes every TermDB reference from that section, so an obligation
cannot point at an in-range but absent slot.

## Semantic Target

### Closed pure BIND

For a BIND input that the pure kernel can reduce:

```text
m -->* RETURN(v)
f : Pi(A, M)
------------------------------
BIND(m, f) -->* APP(f, v) : M(v)
```

`M(v)` is an ordinary classifier TermDB graph.  It may contain Match and will
reduce by beta/iota only when its argument permits that reduction.

### Residual dependent BIND

For an effectful or budget-exhausted input:

```text
m : Comp(E, A)
f : Pi(A, M)
```

the compiler must not choose a concrete `M(v)` before a value `v` exists.
Instead it records a residual obligation associated with this BIND occurrence:

```text
when this occurrence evaluates m to RETURN(v):
  instantiate M(v)
  normalize M(v) using the permitted pure profile
  verify the continuation result against that classifier
```

The continuation binder is a normal Lambda binder.  It is used only while
constructing the family and while evaluating this BIND occurrence.  It does
not escape as a raw binder ID in a closed outer classifier.

This first migration deliberately uses a residual verification graph rather
than claiming a general global equation of the form:

```text
C(THUNK(RETURN(x))) == M(x)
```

The latter is the direction of standard dCBPV+ dependent Kleisli extension,
but requires a settled representation and synthesis policy for `C`.  It can be
introduced later as a static specialization of the residual representation.

## Core Invariants

1. TermDB is a static graph of program and classifier terms.  It is not an
   execution environment, handler stack, mutable store, or dynamic call frame.
2. Pure normalization may allocate canonical result nodes in TermDB.  Runtime
   state and occurrence-local returned values must remain outside TermDB.
3. A closed JudgementDB proof has only closed JudgementDB proofs as premises.
4. A residual obligation is explicit and serializable.  It is not encoded as a
   fake proof, an unknown classifier ID, or an arbitrary DefEq edge.
5. A runtime result may discharge an obligation only for the exact BIND or
   handler occurrence that created it.
6. Pure conversion and runtime effect dispatch use separate evaluators and
   separate APIs, even if both construct TermDB result nodes.
7. Link and artifact readback preserve the distinction between closed and
   residual verification data.

## Target Data Structures

### Normalization result

Replace `int` success/failure-only normalization APIs with a result object:

```text
status: COMPLETE | BLOCKED_EFFECT | EXHAUSTED | INVALID
term: resulting WHNF when available
profile: applied reduction profile
work: elapsed time and/or step/allocation statistics
```

The build policy may use wall-clock time.  The artifact records the actual
outcome and profile rather than treating the budget as semantic evidence.
Only `COMPLETE` may establish kernel conversion.

### VerificationDB

Introduce a serializable `prototype_verification_db`, separate from
`prototype_judgement_db`.

Initial obligation kinds:

```text
DEPENDENT_BIND
HANDLER_RESULT
RUNTIME_CONVERSION
```

An initial dependent-BIND obligation needs:

```text
operation occurrence ID
core BIND term ID
input computation occurrence and TermDB node
continuation occurrence and TermDB node
continuation binder ID
input result classifier A
continuation classifier family M
effect-row context
required pure normalization profile
state: pending | discharged | failed
```

The exact C representation for full dCBPV+ is intentionally not part of this
first schema.  The obligation holds the continuation family and performs the
ordinary substitution only after a runtime value exists.

### Persistent OperationGraph

Promote the currently compiler-local operation graph to an artifact-owned
structure.  It must preserve:

```text
operation ID
core TermDB node
source/annotation occurrence identity
child operation IDs
selected classifier or residual reference
polarity and computation kind
```

This graph is not a replacement TermDB.  It is the typed occurrence layer
needed because a shared core lambda/app/BIND may have multiple legitimate
TypeView derivations.

## Migration Phases

### Phase 0: Design fixtures and baseline

Before changing compiler behavior, add fixtures for:

- a pure dependent BIND whose input normalizes to `RETURN(true)`;
- an effectful Bool operation whose continuation returns Nat for true and Bool
  for false;
- two source occurrences sharing one core lambda but selecting different
  classifiers;
- artifact readback/link of a closed existing example;
- a fixture whose pure normalization budget is intentionally exhausted.

No fixture may rely on a kernel Sigma primitive.

### Phase 1: Outcome-aware pure normalizer

Files: `src/prototype/term.h`, `src/prototype/term.c`, normalization tests.

1. Add an explicit normalization result type and budget policy.
2. Preserve current profiles and ensure `PURE_TYPE_WHNF` cannot dispatch an
   operation or host effect.
3. Return `BLOCKED_EFFECT` when a profile encounters an effect boundary.
4. Return `EXHAUSTED` for budget/depth exhaustion without treating it as an
   invalid graph.
5. Cache only complete profile results initially.  Do not cache partial
   results as normal forms.
6. Keep the existing public convenience APIs temporarily as strict wrappers;
   remove them after all callers migrate.

Acceptance: all examples 01-09 retain closed compilation; tests distinguish
invalid input, blocked effect, and exhausted pure normalization.

### Phase 2: Closed proof graph versus residual verification graph

Files: `src/prototype/judgement.h`, `src/prototype/typing.c`, new
`src/prototype/verification.h` and `src/prototype/verification.c`.

1. Add VerificationDB and its lifecycle alongside JudgementDB.
2. Leave JudgementDB proof validation closed-only.
3. Replace pending `INVALID_ID` classifier outcomes with explicit solver
   states: solved, residual, contradiction.
4. Permit a compile fragment to commit residual data without publishing a
   closed `HAS_TYPE` conclusion for it.
5. Make ascriptions and exported interfaces policy-sensitive: strict contexts
   demand closed evidence; hybrid contexts may export residual requirements.

Acceptance: a deliberately exhausted pure classifier creates a residual record
instead of a false proof or generic compile failure.

### Phase 3: Persist the occurrence layer

Files: `src/prototype/ast.h`, `src/prototype/ast.c`, artifact reader/writer.

1. Extract the operation metadata types from ephemeral compile-only storage
   into a persistent OperationGraph representation.
2. Give residual obligations stable operation-occurrence references.
3. Update sparse artifact marking so operation nodes and all referenced family,
   proof, and TermDB nodes are roots.
4. Update import/link offsetting and validation for operation and verification
   IDs.
5. Bump the artifact version and reject prior layouts.

Acceptance: readback and link preserve an unresolved occurrence without
collapsing it onto an unrelated shared core TermDB node.

### Phase 4: Constraint generation and solver boundary

Files: primarily `src/prototype/ast.c` and `src/prototype/typing.c`.

1. Retain existing source lowering, but make it emit classifier equations and
   CBPV constraints rather than requiring immediate classifier bindings.
2. Replace `operation_classifier_solver.bindings` as the sole authority with
   a constraint/result graph that records provenance.
3. Keep Match motive equations as first-class constraints.
4. Add BIND-family constraints rather than rejecting continuation binder use
   at constraint generation time.
5. Run the solver to a fixed point.  Convert unresolved but well-formed
   equations into VerificationDB obligations; convert inconsistent equations
   into diagnostics.

Acceptance: ordinary Match and append examples still synthesize closed
classifiers; an unresolved dependent BIND becomes residual, not rejected.

### Phase 5: Dependent BIND static discharge

Files: `src/prototype/typing.c`, `src/prototype/ast.c`, `src/prototype/term.c`.

1. Replace the current free-binder rejection in `bind_result_classifier` with
   a branch that recognizes a dependent continuation family.
2. If the input computation has a complete pure reduction to `RETURN(v)`,
   substitute `v` into the continuation classifier family and produce a closed
   BIND derivation.
3. If normalization is blocked or exhausted, create a `DEPENDENT_BIND`
   obligation instead.
4. Keep the existing non-dependent BIND rule as the closed fast path, not as a
   compatibility implementation.
5. Do not add a global conversion rule from arbitrary residual families.

Acceptance: a pure Bool-dependent continuation is discharged to Nat or Bool;
the same shape behind an effectful operation is emitted as a residual artifact.

### Phase 6: Runtime verification machine

Files: `src/prototype/term.h`, `src/prototype/term.c`, a new runtime module,
REPL and target-runtime integration.

1. Separate the current recursive evaluator from a runtime machine that owns
   environment, handler stack, continuation frames, and obligation instances.
2. On `BIND(m, f)`, associate the dynamic frame with the serialized
   `DEPENDENT_BIND` occurrence.
3. When `m` returns `v`, instantiate the stored family with `v`, run only the
   permitted pure classifier normalizer, and check the continuation result.
4. Define failure behavior explicitly: trap, reported runtime type failure, or
   an application-defined recovery handler.  It must not silently succeed.
5. Keep runtime values and frame-local `v` out of TermDB.

Acceptance: a mock effect handler returning true and false discharges the same
artifact obligation along both paths and selects the corresponding classifier.

### Phase 7: Artifact, linker, and policy modes

Files: `src/prototype/ast.c`, `src/prototype/ast.h`, `read_file.c`, artifact
tests, future backend interfaces.

Define explicit policies:

```text
strict
  Reject any exported or entry-point-reachable residual obligation.

hybrid
  Emit residual obligations and require a compatible runtime verifier.

exploratory
  Permit residual execution for the REPL, while reporting verification state.
```

The linker must merge closed proofs and residual obligations without upgrading
the latter to DefEq.  A target such as C may emit runtime checks.  A target
without a compatible runtime verifier, such as a restricted hardware backend,
must reject or require discharge before code generation.

## Required Refactorings Before or During Migration

- Correct the stale TermDB semantic classification that reports BIND itself as
  binding a term variable.  The continuation Lambda owns that binder.
- Replace fixed-size compile/solver capacities where they would prevent
  artifact-sized residual graphs.  At minimum, make overflow a normal
  diagnostic rather than an indistinguishable internal failure.
- Ensure normalization cache entries are keyed by semantic profile and are
  never reused across runtime dispatch or artifact-linked definition contexts.
- Audit every caller of normalization equality so only `COMPLETE` results can
  create conversion proofs.
- Audit artifact graph marking, readback validation, and link offsetting for
  every newly persistent ID space.

## Test Matrix

The migration is incomplete until all of these are automated:

```text
closed pure beta/iota conversion
closed pure dependent BIND
blocked effect produces residual, not a closed proof
budget exhaustion produces residual, not a closed proof
runtime true branch verifies Nat result
runtime false branch verifies Bool result
runtime verification failure is observable
same core / different TypeView occurrences retain separate obligations
artifact round-trip preserves closed and residual graphs
link merges residual obligations without cross-namespace capture
examples 01-09 remain closed under strict policy
```

## Open Decisions

1. Whether a later static dCBPV+ layer should represent residual families using
   `C(THUNK(m))`, and what surface or synthesized representation constructs C.
2. Whether artifacts store replayable reduction traces, normalized result
   pointers plus profile metadata, or rely on a trusted compiler for closed
   normalization claims.  The first option is strongest for independent
   checking but substantially larger.
3. Which classifier forms can be verified at runtime for higher-order values.
   This requires a representation of runtime type evidence and cannot be
   settled by the current first-order intrinsic tests.
4. Which target backends support hybrid verification.  C can plausibly carry a
   verifier runtime; restricted Verilog generation likely requires strict
   discharge.

## Revision Policy

When this plan changes, append a dated decision entry identifying:

```text
changed assumption
reason
affected phases and artifact schema
new invariant or test
```

## Implementation Record

### 2026-07-16: Phase 1 and the first Phase 3/5 slice

Implemented:

- `prototype_term_whnf_with_profile_result` distinguishes `COMPLETE`,
  `BLOCKED_EFFECT`, `EXHAUSTED`, and `INVALID` without changing the strict
  existing WHNF API.
- `PURE_TYPE_WHNF` reports an operation boundary as `BLOCKED_EFFECT`; only a
  complete result is usable as kernel conversion evidence.
- `VerificationDB` is separate from `JudgementDB`.  Its first operation is an
  occurrence-local dependent-BIND discharge API: a runtime value is applied to
  the stored family and the resulting computation classifier is checked.
- Artifact v38 serializes the operation occurrence graph and verification
  records.  Reader validation checks every TermDB and operation reference, and
  sparse marking roots their referenced terms.
- Closed dependent BIND now evaluates a pure family application to `M(v)`.
  A pure family is represented in current CBPV TermDB as
  `THUNK(LAMBDA(x, RETURN(body)))`; the implementation deliberately opens
  that static family representation rather than treating a general `FORCE` of
  a function thunk as a type-level reduction rule.
- A dependent BIND blocked by an effect or the current normalization depth is
  recognized as a candidate `DEPENDENT_BIND` residual rather than as a closed
  BIND proof.  The compiler records it when the source occurrence supplies a
  dependent continuation family.

Current limitations, deliberately not hidden by this slice:

- This first residual runtime entry point handles a residual BIND at the
  selected operation occurrence.  It recursively evaluates a residual input
  occurrence, but it does not yet interpret arbitrary nested residual BIND
  occurrences introduced after continuation beta reduction.
- The frame checker specializes the serialized continuation classifier with
  the actual `RETURN(v)` and discharges the corresponding local obligation.
  It is not yet a general runtime type-evidence system for higher-order
  returned values.
- Artifact readback preserves and validates operation metadata.  Linking now
  offsets and merges target and provider operation IDs, match-case occurrence
  IDs, TermDB references, and continuation binder IDs, including residual
  obligations.  Target-runtime loading and handler-stack execution still
  remain later Phase 6 work.

Tests currently cover normalization outcomes, static pure dependent BIND,
effect-blocked source residual generation, operation-graph artifact
round-trip/linking, and direct REPL execution of an effectful residual BIND.

Do not preserve obsolete compatibility paths merely because a prior artifact
or prototype test used them.  Bump the artifact format and update fixtures
when a semantic representation changes.

### 2026-07-16: Effectful source residual and operation-entry runtime slice

Changed assumption:

The earlier implementation record assumed an effectful computation could not
produce a user ADT value for a residual fixture.  That was false.  A source
BIND can run an effectful `#.print`, discard its `Text` result, and return
`Bool.true`; a later BIND can therefore have a Bool-dependent Match classifier.

Implemented invariants:

- `compile_ast_ref` preserves a syntactic BIND before the value-first lowering
  path.  A top-level source BIND is therefore represented by an operation
  occurrence instead of being reduced to only its shared core term.
- BIND binder propagation is scoped to the exact continuation operation
  subtree.  Canonically shared erased core `VAR(_#n)` nodes from unrelated
  source lambdas cannot pollute another occurrence's classifier, while
  compiler-generated continuations remain supported.
- Handler constraint solving selects a Pi candidate by its required domain and
  checks the complete expected continuation/operation-clause Pi shape.  It no
  longer depends on provisional JudgementDelta insertion order.
- `prototype_operation_evaluate_with_verification` executes a direct residual
  BIND through its operation occurrence.  It evaluates the input exactly once,
  requires `RETURN(v)`, specializes the continuation family using the stored
  pure normalization profile, and discharges a one-record local
  VerificationDB frame before running the continuation.
- The serialized VerificationDB record is not mutated by evaluation.  A
  discharge belongs to one dynamic frame and cannot authorize another call of
  the same artifact occurrence with a different result.

Affected phases and schema:

- Phase 3's v38 operation graph is now used by the REPL as the source
  occurrence authority for direct residual BIND evaluation.
- Phase 6 is partially implemented.  It still requires a general runtime
  evaluator with handler frames, nested residual occurrence dispatch, and
  explicit target policy enforcement.

Regression fixture:

```text
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := #.bind (perform (#.print #"x")) (\x : #.Text => return Bool.true);
main := #.bind m (\b : Bool => b @true => Nat.zero @false => Bool.true);
```

Compilation emits one `DEPENDENT_BIND` residual for the `main` operation.
Artifact round-trip and link preserve it.  REPL execution prints `x` once,
reports `verification main := discharged`, and yields `Nat.zero` without
modifying the artifact obligation.
