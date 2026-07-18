# Current System Design Debt and Refactoring Boundaries

Date: 2026-07-17

Status: design audit and implementation plan

## 1. Scope

This document records the remaining structural problems found after the CBPV,
effect-row, typed-operation, and artifact work. It is not a request to preserve
the current artifact format. Prototype formats and APIs may be replaced when a
clean boundary requires it.

The immediate objective is to make the following pipeline coherent:

1. Lower surface syntax into a shared untyped computation graph.
2. Preserve each typed surface occurrence in the operation graph.
3. Generate classifier and effect constraints from those occurrences.
4. Solve as much as the selected deterministic step budget permits.
5. Store both solved facts and legitimate residual obligations in an artifact.
6. Resume solving after linking resolves external references.
7. Permit backend execution only when that backend's required obligations are
   discharged.

This document does not attempt to finish the following subjects:

- a complete predicative universe hierarchy;
- propositional equality, higher observational equality, or cubical equality;
- IADT index refinement;
- a complete dependent CBPV metatheory.

It does identify minimum invariants that must be preserved now so those features
can be added without replacing the compiler boundary again.

## 2. Architectural Invariants

The refactoring must preserve these project-level decisions.

### 2.1 TermDB is a computation graph, not a typing environment

TermDB may contain source terms and terms produced by normalization. It may grow
while pure evaluation is performed. It must not encode the mutable state of an
abstract machine, lexical environments, proof-search worklists, or unresolved
typing variables as if those were source computations.

### 2.2 Core sharing does not erase typed occurrence identity

Two surface terms may share one core node while selecting different classifiers
or names. For example, typed Boolean and Natural identity functions may share
the same core lambda. Classifier constraints, proof records, exports, and effect
rows therefore belong to typed operation occurrences, not exclusively to a core
term ID.

### 2.3 Constraints and solutions are different data

Elaboration generates equations. A solver derives solutions or leaves residual
obligations. A temporary guess must not be written as an established classifier.
An exhausted budget is not a contradiction, and an unresolved external symbol
is not proof of ill-typedness.

### 2.4 Static verification coverage may differ between artifacts

The compiler uses deterministic step budgets. Two machines or invocations may
close different numbers of optional obligations. The artifact must state what
was proved, what remains, and under which profile it was attempted. Linking may
resume the work. Backend admission must depend on the final facts required by
that backend, not on input artifacts having identical compile policies.

### 2.5 Pure reduction and effect execution remain distinct

Beta and iota reduction may be performed by the pure normalizer. Algebraic
operations are represented as computations and interpreted by handlers or a
backend. The fact that both are graph transformations does not make effectful
execution a definitional conversion rule.

## 3. Priority Summary

| Priority | Problem | Consequence |
| --- | --- | --- |
| Critical | Effect identity is conflated with backend capability | Handlers remove the wrong operations and pure operations disappear from rows |
| Critical | Backend admission ignores unresolved rows and dependencies | Incomplete artifacts are reported as executable |
| Critical | Effect constraints have two authoritative representations | Shared core nodes can acquire constraints from the wrong typed occurrence |
| High | Handler residual uses a stricter rule than set difference | Valid handlers can be rejected |
| High | Linker concatenates constraints but does not resume solving | Linking cannot improve knowledge after symbol resolution |
| High | General classifier constraints are compile-local | Partial typing cannot cross an artifact boundary |
| High | Artifact closure roots every operation occurrence | Trial and unreachable elaboration data leak into output |
| High | Effect-row proof scope is inferred indirectly | Free row variables can be accepted or rejected for accidental proof-graph reasons |
| Medium | An unknown effect row is exposed as an empty effect set | Callers can silently confuse unknown with pure |
| Medium | Universe conversion lacks explicit evidence at the point of use | Cumulativity can be accepted without a recorded solver obligation |
| Structural | Solver, artifact, and elaboration code are concentrated in large modules | Semantic duplication is difficult to detect and remove |

## 4. Effect Identity and Backend Capability Are Conflated

### Current state

`src/prototype/term.h` represents host effects as a small capability bitset. At
present the meaningful distinction is effectively `NONE` versus `TERMINAL`.
Intrinsic operation declarations in `src/prototype/term.c` assign one of those
bits. Effect rows then reuse the same bits as their semantic operation identity.

This combines two different questions:

1. Which algebraic operation may this computation request?
2. Which runtime facilities must a backend provide to execute it?

For example, terminal printing needs a terminal capability. Integer addition
does not need that capability, but it is still an intrinsic operation requiring
an evaluator or backend implementation. Conversely, two distinct terminal
operations may require the same backend capability without being the same
algebraic operation.

### Failure scenario

Assume `terminal.print` and `terminal.read` both use the `TERMINAL` bit. A handler
whose syntax selects `terminal.print` currently subtracts that shared bit from
the input row. It therefore also appears to handle `terminal.read`, even when no
read clause exists.

The reverse failure occurs for an intrinsic such as integer addition. Giving it
the `NONE` capability makes its effect row look empty. The type system can no
longer distinguish a returned value from a pending intrinsic operation solely
from that row.

### Required invariant

Semantic operation identity and backend capability must have separate data
types and separate combination rules.

- An effect row identifies operations or explicitly declared effect families.
- A handler removes exactly the identities covered by its clauses or declared
  family policy.
- Backend capabilities are derived from the remaining operations and execution
  strategy.
- An empty semantic row means pure, not merely “needs no terminal”.

### Refactoring direction

Introduce an opaque qualified operation/effect identity for rows. The identity
must survive artifact serialization and linking. Keep a separate capability set,
for example terminal I/O, intrinsic dispatch, handler execution, and dynamic row
verification. Add one explicit projection from operation declarations to backend
capabilities.

The language must then decide whether one handler clause handles one operation
or an entire declared effect family. Current syntax points at an operation, so
operation-level subtraction is the least surprising initial rule.

### Acceptance tests

- Two terminal operations share a backend capability but remain distinct in an
  effect row.
- Handling one of them leaves the other in the residual row.
- A pure intrinsic operation can be statically reduced when allowed, but an
  unreduced request remains identifiable as an operation.
- Artifact round trips preserve operation identity across namespaces.

## 5. Backend Admission Accepts Incomplete Artifacts

### Current state

`prototype_compile_metadata_validate_backend` in `src/prototype/ast.c` checks
selected solver status and capability metadata, but it does not establish that
all obligations required for execution are closed. In particular, unresolved
effect constraints, residual classifier obligations, unresolved external
dependencies, and relocation state are not uniformly part of the decision.

A reproduced artifact containing an unresolved external application and effect
copy constraints was reported as compatible with interpreter, C, and Verilog
backends.

### Why this is a design error

“Valid object input to the linker” and “executable by a backend” are different
states. A relocatable artifact may legitimately contain unresolved dependencies
and residual constraints. A final executable may not, unless the selected
backend explicitly implements the corresponding dynamic verifier or resolver.

### Required invariant

Backend admission must be a query over the complete linked artifact state, not a
query over compile metadata alone.

The query must account for:

- unresolved symbols and relocations;
- residual classifier and effect constraints;
- universe obligations required by accepted proof records;
- pending verification obligations;
- operation identities and derived runtime capabilities;
- the selected backend's explicitly supported dynamic checks.

### Refactoring direction

Separate at least these predicates:

- `artifact_is_relocatable_object`;
- `artifact_is_link_complete`;
- `artifact_is_backend_emittable(backend_profile)`.

An interpreter may support more dynamic obligations than Verilog, but this must
be declared in its backend profile rather than inferred from a permissive global
flag.

### Acceptance tests

- An unresolved external reference is accepted as a relocatable object and
  rejected as a final executable.
- A residual effect-row equation is rejected by a backend without a dynamic row
  verifier.
- The same artifact can become admissible after linking and solver resumption.
- C and Verilog checks cannot return compatible solely because capability bits
  are empty.

## 6. Handler Residual Is Not Implemented as Plain Set Difference

### Current state

The effect equation solvers in `src/prototype/typing.c` and
`src/prototype/ast.c` require the handled set to be a subset of the input row
before accepting a residual equation. They then compute the remaining effects by
set subtraction.

### Problem

For ordinary algebraic handlers, a handler may contain clauses for operations
that the handled computation never performs. The mathematical residual remains
well-defined:

```text
residual = input_effects - handled_effects
```

Requiring `handled_effects` to be a subset of `input_effects` adds an exactness
discipline. Such a discipline is possible, but it must be an intentional
language rule. It is not implied by set difference and conflicts with the
previously selected residual-row semantics.

### Required invariant

Unless an explicit exact-handler mode is introduced, absent clauses are benign.
Handling a superset of the operations actually used must leave the same residual
row as handling their intersection.

### Refactoring direction

First replace capability bits with semantic effect identities. Then remove the
subset precondition from residual solving. If exact handlers are later useful,
represent that as a separate checker or warning, not as the meaning of the
residual equation.

## 7. Effect Constraints Have Two Authoritative Systems

### Current state

Effect equations exist in both:

- `JudgementDelta.effect_row_equations` in `src/prototype/typing.c`;
- `OperationGraph.effect_constraints` in `src/prototype/ast.c`.

The operation graph imports equations by comparing an operation's core term with
the equation subject. Computation operations without a matched rule can receive
a tautological `COPY(row, row)` constraint so unresolved state is retained.

### Failure scenario

Typed Boolean and Natural identity expressions can share one core lambda. If an
effect equation is associated through only the core term ID, it can be attached
to more than one typed operation occurrence. The same risk applies to generic
instantiations and to trial occurrences created during fixed-point elaboration.

Core sharing is intentional. Therefore core identity cannot be the ownership
key for occurrence-specific typing constraints.

### Required invariant

There must be one authoritative occurrence-level constraint graph. Every
constraint has an operation occurrence ID and explicit operands. Judgement proof
records consume solver results; they do not provide a second solver input that
must be rediscovered by matching core terms.

### Refactoring direction

Generate effect equations directly while elaborating each typed operation:

- APP copies/unifies the callee computation row as required by its classifier;
- BIND unions the left computation and continuation rows;
- PERFORM introduces the selected operation identity;
- HANDLE subtracts handled identities and unions clause/return effects;
- MATCH joins branch computation effects under its motive constraints.

Remove the core-term matching bridge and tautological fallback. Unknown state
must be represented explicitly by unresolved row variables or constraints, not
by a self-equation added to satisfy bookkeeping.

## 8. The Linker Does Not Resume Constraint Solving

### Current state

Artifact linking in `src/prototype/read_file.c` relocates and appends effect
constraints. It does not rebuild a linked worklist and solve again after symbol
resolution. It also requires compatible compile policies and aggregates limits
and counters in ways that treat input attempts as one homogeneous compilation.

### Problem

External references are precisely why an object artifact may be unable to close
a classifier or effect equation. Linking supplies new information. Merely
concatenating the old residuals leaves the compiler unable to use it.

Requiring all input artifacts to have the same policy also conflicts with the
project decision that verification coverage may vary. A strict artifact and a
hybrid artifact can be linked safely if the final required obligations are
discharged under the linker's selected policy.

### Required invariant

Linking is another constraint-solving frontier. It must preserve provenance from
inputs while deriving a new linked verification result.

### Refactoring direction

The link pipeline should be:

1. merge and relocate core and operation graphs;
2. resolve qualified external identities;
3. canonicalize and deduplicate constraints;
4. resume classifier, effect, and universe solving with an explicit linker
   profile and step budget;
5. rematerialize proof records and verification coverage;
6. derive final backend capabilities and admission status.

Input step limits and consumed steps remain provenance. They must not be summed
and presented as if they were the budget of the new linked solver run.

## 9. General Classifier Constraints Do Not Survive Artifacts

### Current state

The operation classifier solver stores variables, motive applications,
equations, dependency edges, and worklists in compile-local arrays in
`src/prototype/ast.c`. Effect-row residuals now have artifact representation,
but general classifier constraints do not.

The final compiler still demands complete APP, LAMBDA, and MATCH classifiers in
many paths. Therefore partial artifact behavior is supported only for selected
dependent BIND, handler, and effect-row cases.

### Consequence

An unresolved dependent motive or induction-hypothesis classifier cannot be
resumed after linking. It must either be guessed, rejected early, or discarded.
This blocks the planned separation between constraint generation and solving,
especially for multiple base cases, dependent motives, Sigma constructors, and
future IADT index refinements.

### Required invariant

Every accepted residual classifier obligation must be serializable, relocatable,
and resumable. Its state must distinguish:

- solved;
- unresolved because information is absent;
- incomplete because a deterministic budget was exhausted;
- contradictory.

### Refactoring direction

Introduce an artifact-level classifier constraint graph, preferably sharing a
common occurrence and evidence model with effect constraints. It needs:

- artifact-scoped variable IDs;
- equation kinds and operands;
- owning operation occurrence IDs;
- motive applications and constructor-case scope;
- binder/telescope context where required;
- dependencies and solver state;
- relocatable external references;
- solver profile, version, and budget provenance.

This is the implementation counterpart of the constraint-generation/solver
separation already selected in the design discussions.

## 10. Artifact Closure Roots Every Operation

### Current state

Artifact closure in `src/prototype/ast.c` marks terms referenced by every
operation in compile metadata. Trial operations and unreachable elaboration
occurrences can therefore become serialization roots.

### Problem

The intended object format contains the graph reachable from exported names and
the interface data needed to type and link those names. It should not contain
every intermediate occurrence the compiler happened to construct.

Rooting all operations causes three classes of failure:

- output grows with elaboration strategy rather than program meaning;
- unreachable trial constraints can create residual obligations;
- strict completion checks may fail because of code that no export references.

### Required invariant

Artifact closure is based on semantic roots, not allocation history.

### Refactoring direction

Use these roots:

- exported typed name occurrences;
- exported type and constructor classifiers;
- proof premises required by those exports;
- residual constraints required by reachable occurrences;
- qualified dependencies and relocation records reachable from those roots.

Before serialization, prune unreachable operations and remap operation, case,
constraint, and debug-label IDs. Ignoring unreachable obligations during strict
checking while still serializing them is insufficient.

## 11. Effect-Row Scope Is Inferred from Proof Ancestry

### Current state

The proof validator searches premise ancestry for a binder assumption carrying
the same effect-row binder. This permits valid higher-order local derivations,
but the proof format itself does not contain an explicit effect-row context.

The existing free-row helper also focuses on the outer returned computation row.
It is not a general check that every row variable nested in a classifier is bound
or generalized.

### Problem

Whether a proof is accepted can depend on the accidental shape of its premise
graph. Nested and higher-rank row variables will make this increasingly fragile.
An artifact validator must not reconstruct scope from a recursive search whose
rules are not part of the proof object.

### Required invariant

Every free effect-row variable in a published classifier is either:

- bound in an explicit local proof context; or
- explicitly generalized in the exported classifier/interface.

### Refactoring direction

Add explicit row-context or row-generalization evidence to proof records, or
close/generalize classifiers before publishing them. Distinguish contextual
local judgements from closed exported judgements. Validate all nested effect-row
positions, not only the outer result row.

## 12. Unknown Effect Rows Are Exposed as Pure

### Current state

When a computation classifier contains an open effect row, a convenience view in
`src/prototype/term.c` exposes its bitset as `PROTOTYPE_HOST_EFFECT_NONE`. The
symbolic row is also retained, so careful callers can distinguish the states,
but the convenience field itself maps both “unknown” and “closed empty” to zero.

### Problem

This is the same ambiguity at an API boundary that previously existed in the
constraint model. A future backend or optimizer can read the bitset alone and
silently treat an unresolved computation as pure.

### Refactoring direction

Return an explicit status with the view, such as `CLOSED` versus `SYMBOLIC`.
Closed bits are meaningful only in the closed state. No API should encode
unknown as the empty set.

## 13. Minimum Universe Evidence Is Missing at Conversion Sites

### Current state

The typing conversion code currently accepts two universe variables without
comparing their level IDs. A comment states that inequalities are collected and
checked later by UniverseDB. However, the compatibility call itself does not
receive UniverseDB or return a constraint/evidence ID. Later reconstruction from
the judgement graph is expected to recover the necessary inequalities.

### Why this still matters without refined universes

The project can postpone max-level computation, cumulativity policy, and a full
predicative hierarchy. It cannot safely accept a universe conversion for which
no later phase can prove which directed inequality was required.

This section does not claim a concrete paradox in the present examples. It
identifies an evidence gap: accepted conversion and recorded universe obligation
are not structurally tied.

### Minimum required invariant

Every accepted conversion between `Universe(u)` and `Universe(v)` must do one of
the following:

- require identical level-variable identity; or
- produce an explicit directed `u <= v` constraint/evidence ID consumed by the
  conversion proof.

Artifact validation must ensure that every such proof points to a present and
successfully solved UniverseDB obligation. A comment or best-effort later graph
scan is not sufficient evidence.

## 14. Structural Refactoring Required After Semantic Repair

`src/prototype/ast.c` and `src/prototype/typing.c` currently contain elaboration,
constraint generation, solving, proof materialization, validation, artifact
closure, and portions of serialization policy. Large fixed-capacity arrays are
embedded in compile context structures. This concentration has allowed semantic
rules to be implemented twice without an obvious ownership violation.

After the authoritative models above are selected, split the implementation into
modules with narrow ownership:

- typed operation graph elaboration;
- classifier constraint generation and solving;
- effect-row constraint generation and solving;
- judgement proof materialization and validation;
- artifact reachability, remapping, and serialization;
- linker resolution and solver resumption;
- backend admission.

Replace unexplained fixed capacities with owned arenas or dynamic collections.
Deterministic resource limits should remain explicit policy and produce precise
diagnostics; they should not be accidental C array sizes.

Do not begin with cosmetic helper extraction. The first split must follow the
new authoritative data ownership, otherwise it will preserve the duplication in
smaller files.

## 15. Recommended Migration Order

### Stage A: Correct effect semantics

1. Separate operation/effect identity from backend capability.
2. Make unknown effect rows explicit in all views.
3. Define handler coverage at operation or family granularity.
4. Implement residual rows as set difference under that definition.

This stage will require an artifact format bump. No compatibility shim should be
kept in prototype code.

### Stage B: Establish one occurrence constraint authority

1. Generate effect constraints directly against operation occurrence IDs.
2. Remove JudgementDelta-to-OperationGraph matching by core term.
3. Remove tautological fallback constraints.
4. Materialize judgement proofs only from solver results and explicit premises.

### Stage C: Repair artifact boundaries

1. Compute artifact closure from exports and required obligations.
2. Prune and remap operation-level data before serialization.
3. Separate relocatable-object validity from backend executability.
4. Reject final backend emission with unsupported residual obligations.

### Stage D: Make linking a solver frontier

1. Resolve qualified external identities before final solving.
2. Merge and canonicalize constraints.
3. Run a new linked solver attempt under an explicit budget.
4. Preserve input verification provenance without requiring equal policies.
5. Recompute backend admission from the linked result.

### Stage E: Generalize residual constraints

1. Serialize classifier variables, motive equations, and dependencies.
2. Represent solved, unresolved, exhausted, and contradictory states separately.
3. Add explicit effect-row scope/generalization evidence.
4. Resume both classifier and effect solving after linking.

### Stage U: Add the minimum universe guard

Tie each nontrivial universe conversion to a directed UniverseDB constraint. Do
not attempt the complete refined-universe design as part of the effect refactor.

### Stage M: Split modules and remove capacity debt

Perform structural module extraction only after Stages A through E establish one
owner for each semantic fact.

## 16. Regression Gates

The migration is not complete until the permanent test suite covers all of the
following:

- the same core lambda with Boolean and Natural typed occurrences;
- generic `List Nat` instantiation followed by matching;
- one-level and multi-level `*rest` execution in append;
- two operations sharing one backend capability but distinct effect identities;
- a handler that covers an operation absent from its input row;
- a handler that leaves an unhandled operation in its residual row;
- an unresolved object rejected for backend execution and accepted for linking;
- a residual constraint closed after external symbol resolution;
- linking artifacts produced with different verification budgets;
- unreachable trial operations absent from serialized closure;
- open effect rows never reported as pure;
- every accepted universe conversion carrying solver evidence;
- artifact write/read/link/write preserving constraints and proof scope.

Run ordinary examples 01 through 09 as a baseline, but do not treat them as
sufficient evidence for artifact or dependent-effect correctness.

## 17. Decision Summary

The most urgent problem is not Universe refinement or a missing effect feature.
It is that semantic effect identity, occurrence-level constraints, proof data,
and backend capability are not yet owned by clearly separate layers.

The implementation should first make effect rows describe algebraic operations,
make the operation occurrence constraint graph authoritative, and make backend
admission inspect the final linked obligations. Only then should the linker gain
general classifier-solver resumption. The universe work needed during this
migration is limited to requiring explicit evidence for every accepted universe
conversion.
