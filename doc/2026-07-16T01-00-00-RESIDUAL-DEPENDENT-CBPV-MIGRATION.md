# Residual Dependent CBPV Verification Migration Plan

Date: 2026-07-16

Status: implemented migration baseline plus remaining runtime/solver work.
Later work may revise the plan, but a revision must state which invariant or
decision changed.

Revision baseline: 2026-07-16, artifact v44 prototype.  The current baseline
includes deterministic normalization/solver budgets, explicit operation
constraint outcomes, qualified artifact proof relocation, nested residual
dependent BIND execution, compile policies, runtime capability declarations,
and occurrence-level HANDLE clause scope.  The rebased plan below remains
authoritative for the incomplete work stated explicitly in each phase.

## Purpose

A Program treats type checking, conversion, and equality checking as compiler
calculation over the program graph.  The compiler may perform pure reduction
before producing an artifact.  It must not perform terminal, stateful,
nondeterministic, or otherwise effectful operations merely to decide a
classifier.

The target system permits two externally visible outcomes for a typing
problem:

```text
closed judgement
  The compiler has a complete kernel derivation under a stated pure reduction
  profile.

residual judgement
  The program is structurally well formed, but a classifier condition depends
  on a value produced by a future CBPV computation.  The artifact carries a
  runtime verification obligation for that computation occurrence.
```

Contradictory constraints, malformed terms, invalid handlers, unsupported
incomplete constraints, and failed runtime verification remain errors.  A
residual judgement is not a proof and must never be treated as a closed
`HAS_TYPE` relation.

This makes artifact verification coverage a property of the produced artifact.
Two compilations of the same source with different explicit step budgets may
contain different amounts of completed pure normalization and different
residual obligations.  With the same inputs, linked definitions, profile, and
budget, the result must be reproducible.  The program graph has the same
meaning; only the recorded proof and verification frontier may differ across
different budget configurations.

## Non-Goals

- Do not add Sigma as a kernel primitive.  Sigma is planned as an ordinary
  user-defined inductive type with a dependent constructor classifier.
- Do not add `RESULT_OF(m)` as an ordinary `A`-valued TermDB node.  For an
  effectful computation, that would conflate a static shared graph node with a
  particular dynamic execution occurrence.
- Do not add arbitrary user `Eq` proofs to kernel conversion.
- Do not execute host effects in the classifier kernel.
- Do not retain obsolete closed-only compatibility paths once their producers
  and consumers have migrated.

## Executive Decision

The migration will not turn the whole TermDB into a dynamic typing machine.
It will preserve four different kinds of state:

```text
TermDB
  Shared static program and classifier expressions.  Pure reduction may add
  canonical intermediate expressions.

OperationGraph
  Occurrence-local typed source operations.  This is where a surface name or
  source occurrence selects one classifier view of a shared core term.

JudgementDB
  Closed kernel derivations only.  Every conclusion is justified without
  running a future effectful computation.

VerificationDB plus runtime frames
  Serializable residual requirements plus dynamic, invocation-local evidence.
  A runtime frame may discharge one occurrence of one obligation; it does not
  mutate the artifact into a globally proved program.
```

Compilation is therefore staged, but not split into unrelated static and
dynamic languages.  The same classifier TermDB expressions and the same pure
normalization profile are used on both sides.  What changes is when the value
needed to instantiate a dependent family becomes available and which evidence
store is allowed to record the result.

The compiler exposes a deterministic pure-reduction step budget.  A build with
a larger configured budget may close more constraints and emit fewer residual
obligations.  Machine speed and elapsed wall-clock time do not affect the
verification frontier.  Budget-dependent variability is permitted only under
these conditions:

1. The executable operation graph has the same semantics.
2. Every closed judgement is valid under a named semantic reduction profile;
   the step budget is a search limit, not part of the truth of the judgement.
3. Every condition not closed is represented explicitly as a reachable
   residual obligation.
4. The artifact records its verification coverage and runtime requirements.
5. A backend that cannot implement those requirements rejects the artifact.

For the same input graph, linked definitions, reduction profile, and step
budget, solving must produce the same closed/residual frontier.  Implementations
may offer an external cancellation mechanism, but a cancelled build produces no
artifact rather than a machine-dependent partial artifact.

### Deterministic budget parameters

Use two explicit counters rather than elapsed time:

```text
normalization_step_limit
  Maximum deterministic evaluator transitions for one normalization request.
  The current recursive prototype charges a step when the normalization
  machine enters a term, including the traversal needed to expose the next
  beta, iota, CBPV cut, or enabled unfold.  This is a reproducible work unit,
  not elapsed time and not a claim that every charged step is a semantic
  rewrite.

solver_step_limit
  Maximum deterministic worklist transitions for one compilation unit after
  linking its declared interfaces.  Processing one queued constraint consumes
  one solver step; normalization called by it has its own limit above.
```

The prototype compile options should expose both as `uint64_t` values and the
CLI should expose stable forms such as:

```text
--normalization-steps N
--solver-steps N
```

The default values are named constants in one policy module.  Zero is not an
implicit synonym for unlimited work; it means a literal zero-step budget.
Unlimited work, if offered for diagnostics, is an explicit option and is not
the default artifact mode.  Constraint scheduling, tie breaking, and graph
iteration order must be stable so these counters are reproducible.

The artifact records the configured limits, reduction-profile identity, solver
outcome counts, and reachable residual obligations.  It need not treat the
number of unused steps as semantic evidence.

## Current Baseline and Gaps

The current code has already moved beyond the original closed-only baseline
described by the historical sections below.

| Area | Current state | Remaining gap |
| --- | --- | --- |
| Core BIND | `BIND(computation, continuation)`; direct and nested dependent results can become residual obligations | dynamically generated continuations still need a first-class runtime frame stack |
| Normalization | explicit `COMPLETE`, `BLOCKED_EFFECT`, `EXHAUSTED`, `INVALID`; deterministic step limit and complete-only cache | the evaluator is still recursive and retains a structural recursion guard in legacy APIs |
| Occurrence layer | operation occurrences, Match cases with source binder identities, HANDLE clause bodies, and clause binder identities are serialized in artifact v44 through OperationGraph APIs | API implementation remains physically concentrated in `ast.c` until the build boundary is approved for splitting |
| Closed evidence | JudgementDB proof DAGs reconnect imports to provider evidence by qualified export identity | no known proof-boundary defect remains in the covered artifact tests |
| Residual evidence | VerificationDB serializes dependent-BIND obligations and nested occurrences discharge per invocation | handler-result and general runtime-conversion obligation kinds are not materialized yet |
| Constraints | immutable operation constraint blueprint, stable IDs, explicit kinds and four outcome states; deterministic dependency-indexed FIFO worklist | effect-row and universe constraints still use separate propagation protocols |
| Runtime | occurrence evaluator executes APP, RETURN, BIND and HANDLE clause entry; TermDB evaluator handles dynamic continuations | no explicit continuation/handler/obligation stack API yet |
| Policy | strict/hybrid/exploratory are serialized and checked at compile/read/link; capability bits are validated | exploratory incomplete verifiers and backend entry contracts are not implemented |

The former blocking artifact-proof defect has been fixed.  Link finalization
now uses qualified provider identity and provider evidence rather than
retaining a premise-free declaration proof after replacing an external term.
The current blocking architectural gap is dynamic execution: a continuation
created while handling an operation has no static OperationGraph occurrence.
It therefore needs an explicit runtime continuation/handler frame carrying
residual verification instances instead of falling back indefinitely to the
recursive TermDB evaluator.

## Target Compilation Pipeline

The target compiler runs the following explicit stages.  A stage may allocate
ordinary TermDB expressions, but only the listed stage may publish each class
of evidence.

### 1. Parse and resolve surface names

Input is AST plus a namespace/import environment.  Output retains source
occurrence identity and resolved declaration identities.  It does not publish
typing judgements.

### 2. Lower the two graph views

Lower each expression once into:

```text
OperationGraph occurrence -> erased/static TermDB root
```

Automatic CBPV boundaries are elaboration decisions on the occurrence graph.
They may create `RETURN`, `THUNK`, `FORCE`, or continuation Lambda/BIND nodes in
TermDB, but they must not infer a classifier by collecting every judgement
ever attached to the shared root.

### 3. Generate constraints

Generate immutable constraints with provenance.  Initial constraint forms are:

```text
HAS_CLASSIFIER(operation, classifier-meta)
CONVERTIBLE(left, right, profile)
APP_RESULT(function-operation, argument-operation, result-meta)
MATCH_MOTIVE(match-operation, family-meta, case equations...)
BIND_RESULT(bind-operation, input-operation, continuation-operation,
            result-meta)
EFFECT_ROW_UNION(left-row, right-row, result-row)
UNIVERSE_LE(left-level, right-level)
```

Constraint generation may refer to scoped family binders.  It must not create a
fake closed classifier merely to let another branch continue.

### 4. Solve under an explicit compile policy

The solver repeatedly propagates constructor, Lambda, APP, Match, Pi, effect
row, and BIND equations.  Every constraint ends in one of four states:

```text
SOLVED          closed evidence can be materialized
RESIDUAL        structurally valid, but requires a future computation value
CONTRADICTION   equations are incompatible
INCOMPLETE      budget ended before SOLVED/RESIDUAL/CONTRADICTION was proved
```

`RESIDUAL` and `INCOMPLETE` are distinct.  A residual condition has a known
runtime discharge rule.  An incomplete arbitrary unification problem does not
automatically become runtime-checkable.  Policy may preserve an `INCOMPLETE`
item only after a specific runtime verifier kind has been selected for it.

### 5. Materialize evidence

For `SOLVED`, publish a closed JudgementDB proof DAG.  For `RESIDUAL`, publish a
VerificationDB obligation tied to an OperationGraph occurrence.  For
`CONTRADICTION`, emit a source-provenance diagnostic.  For unsupported
`INCOMPLETE`, fail compilation.

There is no edge from VerificationDB into a closed JudgementDB premise.

### 6. Slice and serialize

Start artifact reachability from named exports, selected entry points, closed
proof roots, and residual obligation roots.  Serialize the TermDB slice,
OperationGraph occurrence slice, JudgementDB proofs, VerificationDB records,
namespace interface, universe constraints, reduction-profile identities, and
verification coverage summary.

Intermediate terms produced only by speculative normalization are serialized
only if a proof or residual record references them.

### 7. Link and revalidate

The linker resolves qualified exports, relocates all graph ID spaces, and then
rebuilds evidence edges affected by resolution.  It must not merely overwrite
proof conclusions after changing their terms.  Final linked proof validation
and residual-reference validation are mandatory before writing the linked
artifact.

### 8. Execute and verify

The runtime machine owns environments, continuations, handlers, and dynamic
obligation instances.  When an effectful computation returns the value needed
by a residual family, the machine instantiates that family in TermDB, runs the
recorded pure profile, and verifies the continuation result for that one frame.
The serialized obligation remains immutable.

## Rebased Implementation Plan

The following work packages supersede using the historical Phase numbering as
a simple linear checklist.  Each package has an entry condition, concrete code
scope, and completion gate.  Later revisions may split a package, but must not
skip its completion gate.

### Phase A: Restore the artifact proof invariant

Status: complete for the current artifact and regression surface.

Primary files:

- `src/prototype/ast.c`: term relocation and artifact graph validation;
- `src/prototype/read_file.c`: link finalization order;
- `src/prototype/typing.c`: declaration, conversion, and APP proof validation;
- `src/prototype/test_artifact_flow.sh`: provider/consumer proof regressions.

Tasks:

1. Record which declaration relations refer to each external export before
   replacing external TermDB nodes.
2. After provider JudgementDB append and term relocation, select provider
   evidence by resolved export identity, not by a global scan for an equal core
   subject.
3. If consumer and provider classifiers are identical after relocation, point
   the declaration boundary at the provider derivation.  If they are only
   kernel-convertible, add an explicit conversion proof whose premise is the
   provider derivation.
4. Refresh APP premises from those reconstructed relations.
5. Resolve proof IDs once, after all evidence reconstruction.
6. Validate the complete linked JudgementDB.  Do not repair a failed graph by
   copying relation conclusions into proof records.
7. Add a regression where a consumer calls an imported typed identity and a
   regression where two exports share a core Lambda but retain distinct
   TypeViews.

Completion gate:

- `IdUser.apo + IdProvider.apo` links with a valid proof DAG;
- malformed premise-free declarations of resolved provider Lambdas are
  rejected;
- artifact round-trip and Append normalization tests pass;
- no debug-only proof-validation path remains.

Rollback rule: do not retain broad proof-repair scans.  If provider evidence
cannot be selected by export identity, stop and revise the artifact relocation
record instead of guessing by term shape.

### Phase B: Make normalization attempts policy-complete

Status: complete for deterministic compile budgets and normalization outcomes.

Primary files: `term.h`, `term.c`, normalization tests, compile option plumbing.

Tasks:

1. Extend normalization results with a deterministic reduction-step counter,
   configured step limit, and the graph revision observed.  Do not use a
   wall-clock deadline to select artifact verification coverage.
2. Ensure every kernel conversion caller consumes the outcome object and uses
   only `COMPLETE` as evidence.
3. Cache only complete results by `(term, profile, graph revision,
   transparency environment)`.  Never cache `EXHAUSTED` as a normal form.
4. Separate semantic profile identity from step budget.  Two configured limits
   may produce different coverage, but a completed `PURE_TYPE_WHNF` result has
   the same meaning.
5. Add counters to diagnostics and artifact coverage metadata without treating
   them as proof premises.

Completion gate: blocked effects, resource exhaustion, malformed graphs, and
complete WHNF are distinguishable at every typing call site.

### Phase C: Separate constraint generation from solving

Status: complete for the current operation-classifier solver.  Constraint
identity, immutable generation, explicit outcomes, and dependency-indexed FIFO
worklist scheduling are implemented.  Effect-row and universe constraints
remain separate solver domains and are not claimed to use this worklist.

Primary files: `judgement.h`, `typing.c`, `ast.c`; split modules only after the
data contract is stable.

Tasks:

1. Give every constraint a stable ID, source operation ID, kind, operands,
   state, and provenance.
2. Move Match motive equations, APP equations, BIND equations, effect rows, and
   universe inequalities into the same explicit result protocol.
3. Keep scoped metavariables solver-local.  A metavariable is not a TermDB term
   and is never serialized as a classifier.
4. Replace APIs whose return values conflate `no progress`, `residual`, and
   `error` with the four solver outcomes.
5. Make fixed-point iteration a worklist over changed constraints rather than
   repeated whole-TermDB scans.
6. Materialize TermDB motive/family graphs only when a solution or a defined
   residual verifier requires them.

Implementation note:

- Each classifier binding enqueues only constraints indexed as depending on
  that operation.
- Processing one queued constraint consumes one `solver_step_limit` unit.
- CBPV boundary propagation and BIND input propagation are ordinary constraint
  kinds rather than post-solver whole-graph passes.
- Re-entering the solver may seed an already classified Match from an earlier
  phase.  In that case an IH recovers the authoritative motive from the
  classifier `APP(motive, scrutinee)`; no temporary motive metavariable is
  emitted into TermDB.

Completion gate: the solver can report why each unresolved operation is
residual or unsupported, and existing Match/append/IH examples remain closed.

### Phase D: Generalize dependent BIND classification

Status: direct, nested, repeated, true-branch, and false-branch residual BIND
slices implemented.

Primary files: `typing.c`, `ast.c`, VerificationDB API and tests.

Tasks:

1. Represent the continuation result as a scoped classifier family `M(x)`.
2. For pure `m ->* RETURN(v)`, solve the result as `M(v)` and publish ordinary
   BIND evidence.
3. For an effect boundary, emit `DEPENDENT_BIND` with the exact input,
   continuation, family, binder, effect row, profile, and source occurrence.
4. For exhaustion, emit a residual only if the same verifier can soundly
   discharge it later.  Otherwise retain `INCOMPLETE` and apply policy.
5. Permit multiple dynamic invocations by creating runtime obligation
   instances; never change the artifact record from pending to globally
   discharged.
6. Extend this through nested BINDs introduced after beta/iota reduction.

Completion gate: pure, effect-blocked, nested, repeated, true-branch,
false-branch, and failing-result fixtures all have defined outcomes.

### Phase E: Make OperationGraph and VerificationDB owned subsystems

Status: ownership APIs implemented at the persistence and link boundaries.
Physical source-file separation remains pending because the accepted build file
is outside the default prototype write boundary.

Primary files: currently `ast.h` and `ast.c`; planned
`operation_graph.[ch]` and `verification.[ch]` under `src/prototype/`.

Tasks:

1. Move lifecycle, validation, relocation, and slicing behind subsystem APIs.
2. Keep OperationGraph node identity occurrence-local and TermDB identity
   structural.  Never use a core term ID as an obligation occurrence ID.
3. Add verification-record schema versions per obligation kind.
4. Validate all sparse references for presence, not merely ID range.
5. Compute a coverage summary: closed exports, residual exports, reachable
   obligation kinds, and required runtime capabilities.

Completion gate: artifact code does not access VerificationDB storage fields
directly outside its API, and linked obligations retain exact occurrence
identity across provider offsets.

Current implementation:

- Artifact v44 stores a schema version on every verification obligation and
  source binder identities on every Match case.
- VerificationDB owns count, lookup, insertion, validation, coverage, and
  operation-identity search.
- OperationGraph owns count, lookup, append, case append, and graph validation
  at artifact read/write/link boundaries.
- Artifact and linker code no longer reads VerificationDB storage fields
  directly.  Provider operation, source binder, graph binder, and TermDB
  relocation spaces remain distinct.

### Phase F: Introduce the general runtime machine

Status: complete for the current operation set.  The REPL and reusable runtime
API execute one explicit occurrence machine; the former recursive
`depth=100000` evaluator has been removed.

Primary files: new `runtime.[ch]`, with `term.c` retaining pure graph
normalization and REPL delegating execution.

Runtime state:

```text
current computation
value environment
continuation stack
handler stack
effect capabilities
dynamic obligation-instance stack
failure/trace sink
```

Tasks:

1. Implement RETURN, BIND, FORCE/THUNK, APP, Match, and operation request as
   machine transitions.
2. Keep pure beta/iota/CBPV cut normalization callable independently.
3. Attach a dynamic verification instance when entering a residual occurrence.
4. On the relevant RETURN, instantiate and normalize its classifier family,
   then check the continuation output before completing the frame.
5. Dispatch nested residual occurrences by OperationGraph edge, not by searching
   for a structurally equal BIND term.
6. Define observable runtime verification failures and stack traces.

Completion gate: the REPL and a reusable runtime API execute the same machine;
the recursive evaluator is no longer the authority for effectful execution.

Current implementation:

- The machine owns the current occurrence, source-binder environment,
  continuation stack, handler stack, runtime resumptions, dynamic verification
  instances, and failure trace.
- APP, RETURN, BIND, Match, static FORCE/THUNK, HANDLE, and operation requests
  are machine transitions.  Pure TermDB normalization remains independent.
- Operation requests capture intervening machine frames until a handler.
  Resumption invocation restores those frames and the original environment, so
  BIND and Match occurrence identities survive suspension.
- Deep handling is covered by a resumed BIND whose continuation performs the
  handled operation again.
- `prototype_operation_evaluate_with_trace` reports the failed occurrence,
  continuation frames, and dynamic obligation states.

### Phase G: Complete algebraic handlers without absorbing pure reduction

Status: handler/effect-row foundations and occurrence-level clause scope exist.
Dynamic continuation frames and handler-result residuals remain incomplete.

Tasks:

1. Treat operation requests and handlers as computation constructors and
   eliminators.  Do not encode beta or iota reduction as effect operations.
2. Give handler return and operation clauses explicit typed OperationGraph
   occurrences.
3. Propagate residual effect rows through BIND and HANDLE constraints.
4. Emit `HANDLER_RESULT` obligations when a dependent handler result genuinely
   requires a runtime operation result.
5. Keep deterministic host intrinsics outside kernel conversion even when their
   effect row is empty.

Completion gate: pure normalization is confluent under its profile regardless
of installed runtime handlers, and handler execution cannot alter DefEq.

### Phase H: Enforce artifact and backend policies

Status: compile/read/link policy and capability metadata implemented; backend
entry enforcement remains incomplete.

Tasks:

1. Add explicit `strict`, `hybrid`, and `exploratory` compile/link policies.
2. Compute reachability of obligations from each export and entry point.
3. Record required verifier version, normalization profiles, and runtime effect
   capabilities in the artifact interface.
4. Reject incompatible provider/consumer policy combinations during link.
5. Require strict discharge for backends without a verifier.  Permit hybrid C
   output only after its runtime contract exists.  Do not assume a Verilog
   backend can implement dynamic verification.

Completion gate: policy is enforced at compile, link, load, and backend entry;
it is not a REPL-only option.

### Phase I: Remove transitional paths and establish the new baseline

Status: final cleanup.

Tasks:

1. Delete old success/failure normalization wrappers after all callers migrate.
2. Delete provisional classifier paths superseded by explicit constraints.
3. Remove direct effectful evaluation from TermDB normalization APIs.
4. Reject old artifact layouts rather than maintaining dual readers.
5. Update `src/prototype/README.md`, examples, artifact fixtures, and design
   documents to describe one architecture.
6. Run the full prototype suite and examples 01-09 from clean builds.

Completion gate: there is one path for constraint solving, one path for closed
proof publication, one path for residual publication, and one effectful runtime
machine.

## Dependency Order and Checkpoints

```text
A artifact proof invariant
  -> B normalization outcomes
  -> C explicit solver results
  -> D dependent BIND
  -> E persistent occurrence/evidence ownership
  -> F runtime machine
  -> G handlers
  -> H backend policies
  -> I cleanup
```

Phases B and E may be developed in parallel after A, but D must consume B/C
contracts, and F must consume D/E artifact contracts.  Artifact format changes
should occur only at the end of A, E, or H, followed immediately by round-trip
and link tests.  Do not make an artifact bump for an in-memory-only refactor.

Each checkpoint must include:

```text
documented invariant change
artifact schema impact
new positive and negative regression
examples 01-09 result
clean-build result
known residual limitations
```

## Planned Test Gates

Every phase retains the earlier tests and adds focused regressions.  The final
gate is:

```text
make
make reader
all src/prototype/test_*.sh tests applicable to the prototype
examples/01 through examples/09
shared core identityBool / identityNat
generic List Nat construction and Match
single- and multi-step *rest append
artifact provider/consumer typed APP
artifact round-trip and linked normalization equality
pure dependent BIND static discharge
effect-blocked dependent BIND residual generation
nested and repeated runtime residual discharge
runtime verification failure
strict backend rejection of reachable residual work
```

Tests must identify exports and operations by names or serialized stable IDs,
not fixed numeric TermDB or proof slots.

## Revision and Course-Correction Rules

This is a living plan.  A course correction is valid when it updates this file
with all of the following:

1. the failed assumption or newly discovered invariant;
2. code and artifact schemas affected;
3. migration phases that move, split, or become unnecessary;
4. replacement positive and negative tests;
5. whether existing artifacts are rejected;
6. whether the change affects program semantics or only verification coverage.

Do not add a compatibility helper solely to keep an obsolete prototype path
alive.  If a representation is replaced, migrate its current producers and
consumers in the same phase and reject the old representation at the artifact
boundary.

## Historical Pre-v38 Baseline

This section records the implementation state from which the first v38 slice
started.  It is retained to explain the implementation record; it is not the
current execution plan.

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

The evaluator reported only success or failure.  A depth limit,
invalid graph, blocked effect, and unsatisfied runtime operation are not
represented as distinct outcomes.  The cache cannot represent a partial or
budget-exhausted result.

### Source operation graph

`src/prototype/ast.c` creates `prototype_operation_node` records while
lowering.  They represent typed source occurrences and prevent a shared core
TermDB node from being assigned one arbitrary classifier when different
TypeView occurrences select different classifiers.

At that baseline, `prototype_compile_metadata.operations` was compiler-local
and was not part of an artifact.  That was sufficient for closed compilation
but not for a runtime obligation, because a residual BIND condition belongs to
a source occurrence, not merely to a structurally shared core BIND node.

### JudgementDB and constraints

`src/prototype/judgement.h` stores closed `HAS_TYPE` and `IS_TYPE` relations
with proof DAGs.  The temporary `prototype_judgement_delta` stores CBPV and
effect-row constraints while compiling.

At that baseline, `src/prototype/ast.c` rejected a compilation if pending classifier
state remains after the fixed-point process.  `PROTOTYPE_JUDGEMENT_KIND_UNKNOWN`
means an absent relation slot; it is not an appropriate representation for a
runtime condition.

### Original BIND restriction

The original `src/prototype/typing.c:bind_result_classifier` accepted:

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

Artifact format v37 serialized interface data, sparse TermDB/type-declaration
graphs, JudgementDB proofs, universe constraints, debug data, and relocations.
It did not serialize source operation occurrences, solver constraints,
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

## Original Target Data Structures

This section describes the first v38 target.  The current implementation has
partially realized it; the rebased phases above define the remaining work.

### Normalization result

Replace `int` success/failure-only normalization APIs with a result object:

```text
status: COMPLETE | BLOCKED_EFFECT | EXHAUSTED | INVALID
term: resulting WHNF when available
profile: applied reduction profile
work: deterministic reduction steps consumed and configured step limit
```

The artifact records the actual outcome, profile, configured step limit, and
steps consumed rather than treating the budget as semantic evidence.  Only
`COMPLETE` may establish kernel conversion.  Wall-clock cancellation aborts the
build and does not emit a partially verified artifact.

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

## Historical Phase Decomposition

These phases are retained to map the implementation record to the original
plan.  Use the rebased Phase A-I plan above for scheduling and completion.

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

### 2026-07-16: Deterministic budgets, v42 policy boundary, and nested runtime slice

Changed assumptions:

- Verification coverage is controlled by explicit deterministic work counts,
  never by machine speed or elapsed time.
- Source binder identity is a separate relocation space from canonical TermDB
  binder identity.  Local source binder numbers cannot be concatenated across
  artifacts unchanged.
- A HANDLE occurrence must retain operation-clause and return-clause operation
  bodies plus their source/graph binder correspondence.  The TermDB handler
  term alone is insufficient for occurrence-local execution.

Implemented:

- `--normalization-steps N` and `--solver-steps N` configure deterministic
  compile limits.  Zero is a literal zero-step limit.  Diagnostics and artifact
  metadata record configured limits, consumed work, exhaustion, and
  solved/residual/incomplete constraint counts.
- Normalization attempts report `COMPLETE`, `BLOCKED_EFFECT`, `EXHAUSTED`, or
  `INVALID`.  Only complete results enter the WHNF profile cache or establish
  conversion evidence.
- Operation classifier constraints have stable IDs, source operation and AST
  provenance, explicit APP/Match/BIND/perform/HANDLE kinds, and
  solved/residual/contradiction/incomplete states.  The immutable constraint
  blueprint is generated once; solver state is rebuilt from it while evolving
  lowering facts are materialized.
- Residual dependent BIND evaluation follows occurrence edges through nested
  BINDs.  Runtime environments bind source occurrence identity to returned
  values while TermDB remains a static computation graph.
- Artifact v42 stores strict/hybrid/exploratory policy, required runtime
  capability bits, deterministic budget coverage, HANDLE clause scope, and
  residual obligations.  The reader recomputes capability requirements and
  rejects understated metadata.  The linker rejects mixed policies and
  relocates source binder identities independently from graph binders.
- HANDLE execution enters the serialized operation and return clause bodies
  with occurrence-local environments.  Dynamically generated resumptions are
  still represented as TermDB thunks and are the boundary to the remaining
  runtime-machine work.
- Fixed magic pass limits in operation name unwrapping and artifact link
  convergence were replaced by finite graph-size bounds.  These bounds are
  structural cycle guards, not verification budgets.

Runtime capability bits in v42 are:

```text
1  dependent-BIND verifier
2  operation dispatch
4  algebraic handler execution
8  terminal capability
```

Policy behavior:

```text
strict
  Reject every artifact with reachable residual obligations.

hybrid
  Permit defined residual verifier kinds and record required runtime
  capabilities.

exploratory
  Reserved for future explicitly named runtime verifiers.  Arbitrary
  INCOMPLETE constraints are not silently accepted today.
```

Known remaining work:

- Move OperationGraph and VerificationDB lifecycle/relocation code out of
  `ast.c` behind owned subsystem APIs.
- Introduce an explicit runtime continuation, handler, and dynamic obligation
  stack.  A resumption generated by HANDLE currently crosses into the TermDB
  computation evaluator, so residual obligations inside such a dynamic
  continuation are not yet generally occurrence-addressable.
- Materialize and execute `HANDLER_RESULT` and general runtime-conversion
  obligations.
- Enforce policy and capability contracts at future C/Verilog backend entry
  points.

Completed after the initial v42 baseline:

- The operation-classifier solver now uses a stable dependency-indexed FIFO
  worklist.  The configured solver budget counts dequeued constraints, not
  elapsed time or machine-dependent throughput.
- `RETURN`, `THUNK`, and `FORCE` classifier propagation is represented by an
  explicit CBPV-boundary constraint.  BIND input propagation is likewise
  triggered through the worklist.
- Recursive dependent Match remains closed across repeated classifier phases:
  an already materialized `APP(motive, scrutinee)` classifier restores its
  motive pointer for IH propagation without serializing solver-local unknowns.

Regression coverage includes the four prototype test suites, examples 01-09,
shared-core typed identities, generic `List Nat` Match, recursive `*rest`,
nested/repeated residual BIND, policy mismatch, capability tampering, HANDLE
execution, provider proof relocation, and artifact normalization equality.
