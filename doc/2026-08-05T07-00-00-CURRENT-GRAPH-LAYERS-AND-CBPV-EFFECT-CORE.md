# Current Graph Layers and CBPV Effect Core

Date: 2026-08-05

Status: current-system design record. This document describes the implemented
prototype through `main` commit `586d16c`. It is descriptive where explicitly
labelled "current" and normative where explicitly labelled "invariant" or
"decision".

## 1. Purpose

The compiler must preserve two facts at once:

1. structurally identical computation fragments may share one canonical Core
   graph;
2. source occurrences with different classifiers, binders, polarity, or
   execution intent must remain distinguishable.

The implementation therefore does not consist of one graph with every concern
encoded into node identity. It consists of one shared TermDB plus
occurrence-level and proof-level graphs which refer to it.

This document fixes the responsibilities of those layers after direct effect
application replaced the former surface wrapper.

## 2. Principal Decision

A Program keeps one TermDB. It does not create independent ValueTermDB and
ComputationTermDB databases.

The same general `APP`, `LAMBDA`, binder, substitution, and canonicalization
machinery is reused across value construction, computation functions, type
families, and continuations where their rules permit it.

This does not mean that value and computation are interchangeable. CBPV
polarity is represented by explicit Core boundaries and occurrence-level
typing judgements:

```text
RETURN            A -> Comp({}, A)
THUNK              B -> Thunk(B)
FORCE              Thunk(B) -> B
COMPUTATION_FOLD   Comp(E, A) and continuation/fold algebra -> computation
```

An operation request is a computation-algebra constructor:

```text
OPERATION_REQUEST(operation, argument, continuation)
```

It is not an F/U boundary and is not reducible to a polarity annotation.

## 3. Current Compiler Layers

### 3.1 Surface AST

The AST records syntax before canonical Core sharing. It currently includes:

- names and variables;
- `APP`, `LAMBDA`, Match, constructor formation, and ascription;
- definition and computation blocks;
- quotation;
- computation-fold syntax.

The AST owns source spans and source binder identities. AST identity is not
semantic Core identity.

There is no perform-specific AST node. Direct application syntax is resolved
semantically during lowering.

### 3.2 TermDB: canonical static graph

`struct prototype_term_db` stores canonical program and classifier terms. Its
term vocabulary currently includes these broad groups.

Shared structural terms:

```text
VAR
APP
LAMBDA
PI
MATCH
CONSTRUCTOR
TYPE_FORMER
TYPE_VIEW
```

CBPV types and boundaries:

```text
COMPUTATION_TYPE
THUNK_TYPE
RETURN
THUNK
FORCE
```

Effect computation algebra:

```text
EFFECT_OPERATION
OPERATION_REQUEST
COMPUTATION_FOLD
EFFECT_LABEL
EFFECT_ROW_*
```

Host and linking terms:

```text
PURE_PRIMITIVE
PRIMITIVE_TEXT / TEXT_LITERAL
PRIMITIVE_INT* / INT_LITERAL
EXTERNAL_REF
```

TermDB nodes may be shared by alpha/canonical structure. For example, two
identity lambdas can have one Core `LAMBDA` even when one source occurrence is
typed `Bool -> Bool` and another is typed `Nat -> Nat`.

TermDB is a computation graph. Pure normalization may add canonical
intermediate and result terms. TermDB is not a runtime environment: live host
resources, handler stacks, mutable stores, and occurrence-local returned
values must not become canonical TermDB identity.

### 3.3 OperationGraph: typed source-occurrence graph

`struct prototype_operation_graph` records lowering occurrences. Every
operation node points to a `core_term`, but multiple operation nodes may point
to the same TermDB node.

An operation occurrence currently stores, among other fields:

- source operation tag;
- value/computation polarity;
- computation kind;
- APP role;
- typing context;
- canonical Core term;
- lowering-time known classifier;
- solver result classifier;
- source AST and symbol provenance;
- binder provenance;
- occurrence edges such as function, argument, body, and scrutinee;
- Match case and computation-fold clause ranges.

This is the layer in which the following distinction lives:

```text
same core lambda + Bool classifier + source occurrence X
same core lambda + Nat classifier  + source occurrence Y
```

OperationGraph identity must not be recovered by searching for a matching
TermDB node. Core sharing is deliberately many-to-one.

### 3.4 Constraint and solver state

OperationGraph construction emits classifier and effect-row constraints. A
lowering-time `known_classifier` is only an input fact. It is not a completed
kernel derivation.

The fixed-point/constraint machinery computes occurrence classifiers and
effect rows. Solver-local variables and unresolved constraints must not be
mistaken for ordinary source TermDB terms.

The current implementation still contains direct fixed-point behaviour in
addition to more explicit constraints. That is existing design debt; it does
not change the ownership boundary described here.

### 3.5 JudgementDB and proof graph

JudgementDB records accepted typing derivations. Relevant Core rules include:

```text
RETURN_INTRO
THUNK_INTRO
FORCE_ELIM
OPERATION_REQUEST_INTRO
COMPUTATION_FOLD_ELIM
```

OperationGraph inference is not proof authority. A classifier becomes an
accepted result only when the corresponding kernel proof can be reconstructed
and validated.

Definitional conversion remains a kernel decision procedure. It is not a
user-visible operation request and is not added to the effect algebra.

### 3.6 VerificationDB

VerificationDB stores residual obligations which could not be discharged by
the configured static work budget. A residual obligation is not a closed
has-type proof and must not be published as one.

This layer is relevant to dependent sequencing, where the result of a future
computation may determine a later classifier.

### 3.7 Runtime machine

The runtime machine owns:

- evaluation frames;
- binder environments;
- handler stacks;
- resumption identities and use state;
- host resource values;
- runtime instances of residual verification obligations.

These are not TermDB nodes. Runtime execution may use TermDB terms as static
program descriptions and may construct canonical pure result terms, but the
machine state itself remains separate.

## 4. Three Different Meanings of "Layer"

The code uses several classifications which must not be conflated.

### 4.1 `prototype_term_layer`

This classifies broad Core semantic families such as Lambda core,
eliminators, type formers, data, links, pure primitives, effect operations,
and induction. It is not the CBPV value/computation judgement.

### 4.2 Classifier category

`prototype_term_classifier_view` classifies a classifier as:

```text
VALUE
COMPUTATION
TYPE
```

For a computation it additionally exposes computation kind, effect row, and
result type. This is a view of a classifier graph, not a replacement for Core
syntax.

### 4.3 OperationGraph polarity

OperationGraph polarity records whether one elaborated source occurrence is
being used as a value or computation. It controls valid elaboration edges and
proof reconstruction.

It is occurrence-local. It is intentionally not part of TermDB canonical
identity.

## 5. Shared APP and LAMBDA

### 5.1 What is shared

There is one Core `APP` tag and one Core `LAMBDA` tag. The compiler does not
need separate `VALUE_APP`, `COMPUTATION_APP`, `VALUE_LAMBDA`, and
`COMPUTATION_LAMBDA` tags merely to duplicate substitution machinery.

OperationGraph `application_role` selects the typed rule for an APP
occurrence, including function elimination and constructor formation.

### 5.2 What is not erased

The following distinctions have operational content and cannot be recovered
from an arbitrary later TypeView alone:

- whether a computation was suspended by `THUNK`;
- whether a suspended computation is started by `FORCE`;
- whether a value was injected by `RETURN`;
- whether a continuation structurally sequences/interprets a computation;
- whether an effect operation has become a suspended request;
- which continuation belongs to that request.

These remain explicit Core structure.

### 5.3 Erased sharing versus executable occurrence

It is valid to think of TermDB sharing as an erased structural layer and the
OperationGraph as the typed executable occurrence layer. It is not valid to
execute an arbitrary shared Core node by selecting whichever classifier is
convenient. Execution begins from a selected typed occurrence/root and follows
its explicit CBPV boundaries.

## 6. CBPV Boundary Interpretation

Use the following conceptual rules.

```text
v : A
--------------------------
RETURN(v) : Comp({}, A)
```

```text
M : B
--------------------------
THUNK(M) : Thunk(B)
```

```text
u : Thunk(B)
--------------------------
FORCE(u) : B
```

```text
M : Comp(E, A)
k : Pi(A, Comp(F, B))
------------------------------------------------
COMPUTATION_FOLD(M, k, []) : Comp(E union F, B)
```

The last rule is the zero-operation-clause sequencing case. A fold with
operation clauses is a handler/algebra interpretation, not a second unrelated
node family.

## 7. Effect Operation Representation

### 7.1 Operation declaration

`EFFECT_OPERATION(op)` is a value-level operation identity with interface
metadata:

- nominal operation ID;
- classifier schema;
- language effect labels;
- required host capabilities;
- arity;
- inner-operation policy;
- resumption multiplicity.

Intrinsic namespace membership does not make every intrinsic an operation.
Host types, pure primitives, effect operations, and the contextual
`#.return` fold label are distinct binding kinds.

### 7.2 Applied signature versus request

The application graph

```text
APP(EFFECT_OPERATION(op), argument)
```

establishes that the operation signature has been supplied an argument. By
itself it does not contain the continuation required by the free computation
tree.

The Core request is:

```text
OPERATION_REQUEST(op, argument, THUNK(LAMBDA(result, continuation_body)))
```

For the smallest standalone request, `continuation_body` is
`RETURN(result)`.

### 7.3 Why OPERATION_REQUEST remains a Core node

The free effect computation has the conceptual constructors:

```text
RETURN(value)
REQUEST(operation, argument, continuation)
```

`COMPUTATION_FOLD` consumes that structure:

- `RETURN(value)` selects the return clause;
- a matching request selects its operation clause;
- an unmatched request is forwarded with a recursively folded continuation.

Removing the request tag and encoding it as nested APP is possible only by
moving the same distinction elsewhere. The evaluator would still have to
inspect an `EFFECT_OPERATION` head, detect saturation, recover the argument
and continuation, and distinguish that APP spine from ordinary beta
application.

The dedicated tag therefore remains the clearer canonical Core constructor.
This decision does not require a `perform` keyword in the surface language.

### 7.4 Higher-order operation payloads

The request argument is an arbitrary value TermDB term. It may therefore be a
`THUNK` containing another computation. No separate higher-order request tag is
required.

Whether a handler interprets operations inside the thunk is a scoped-handler
policy question. Merely storing a thunk argument does not force deep handling
inside it.

## 8. Current Reduction Behaviour

### 8.1 Pure normalization

Pure profiles may perform enabled beta, iota, CBPV cut, and transparent
reduction. They must not dispatch observable host effects.

An unhandled `OPERATION_REQUEST` is therefore a computation WHNF/blocking
point for pure normalization.

### 8.2 Computation fold

When a fold evaluates its input:

- `RETURN(v)` applies the return clause to `v`;
- `OPERATION_REQUEST(op, a, k)` applies a matching clause to `a` and the
  recursively folded `k`;
- an unmatched request is rebuilt with the recursively folded continuation.

### 8.3 Host dispatch

Only runtime execution with an enabled dispatcher may interpret an unhandled
request as a host action. The dispatcher returns a runtime result and the
machine resumes the request continuation with that result.

Kernel conversion and artifact validation must not perform this dispatch.

## 9. Direct Operation Application Invariant

The current surface uses ordinary application syntax:

```ap
#.print #"hello"
```

After name resolution, a saturated `EFFECT_OPERATION` head lowers to one
`OPERATION_REQUEST` occurrence with an explicit identity continuation. An
alias such as `output := #.print; output #"hello";` follows the same path.
Pure primitives remain ordinary APP spines.

OperationGraph validation enforces:

```text
Every accepted saturated EFFECT_OPERATION application denotes exactly one
operation request occurrence.
```

The internal APP used as the operation-signature typing premise may remain in
TermDB/JudgementDB, but it is not an executable source occurrence. Surface
`perform` has no special parser meaning and may be used as an ordinary name.

## 10. Naming and Identity Invariants

1. An operation alias resolves to the same nominal `EFFECT_OPERATION` identity.
2. Request recognition must happen after name resolution, not by checking for
   a `#.` spelling.
3. Pure primitives and effect operations remain different TermDB heads even
   when their surface application syntax is identical.
4. `#.return` is a computation-fold label, not an effect operation and not a
   request head.
5. Core TermDB sharing must not merge source occurrences or their classifiers.
6. Runtime handler/resumption identity must not become canonical TermDB
   identity.

## 11. Artifact Boundary

Artifacts must preserve reachable slices of:

- TermDB Core terms, including explicit requests and continuations;
- OperationGraph occurrences and occurrence edges;
- contexts and binders required by typed occurrences;
- JudgementDB proof roots;
- VerificationDB residual obligations;
- effect operation declarations/dependencies and backend requirements;
- selected definition/entry metadata.

Artifact v61 does not preserve parser-only `perform` syntax. Removing the
surface AST node did not remove the Core request tag. The version was bumped
because serialized OperationGraph occurrence tags are part of the format;
v60 is rejected explicitly.

## 12. Non-Goals

This design does not:

- split TermDB into separate value and computation databases;
- encode all ADTs and effects into Church-encoded APP/LAMBDA terms;
- make polarity an arbitrary runtime TypeView selection;
- put live environments or resources into TermDB;
- make every intrinsic namespace binding an effect operation;
- turn beta or iota reduction into effect requests;
- eliminate `OPERATION_REQUEST` merely to reduce the number of enum tags.

## 13. Review Checklist

- [x] One TermDB is authoritative for canonical static terms.
- [x] OperationGraph retains occurrence-local classifier and binder identity.
- [x] RETURN, THUNK, FORCE, OPERATION_REQUEST, and COMPUTATION_FOLD are explicit
      Core terms.
- [x] Runtime environments and host resources remain outside TermDB identity.
- [x] OPERATION_REQUEST is retained as the effect-algebra constructor.
- [x] Every saturated effect-operation application lowers to a request.
- [x] The `perform` surface keyword and AST node are removed.
- [x] OperationGraph uses request terminology instead of surface `PERFORM`
      terminology.

## 14. Related Documents

- `2026-07-14T01-00-00-RAW-DCBPV-MIGRATION-PLAN.md`
- `2026-07-16T01-00-00-RESIDUAL-DEPENDENT-CBPV-MIGRATION.md`
- `2026-07-17T02-00-00-CURRENT-SYSTEM-DESIGN-DEBT.md`
- `2026-07-18T00-00-00-EXECUTION-SUPPRESSION-CBPV-SURFACE-PLAN.md`
- `2026-08-05T00-00-00-MATCH-STYLE-COMPUTATION-FOLD-AND-INTRINSIC-RETURN-PLAN.md`
- `2026-08-05T03-00-00-HIGHER-ORDER-EFFECT-CAPABILITY-DECISIONS.md`
- `2026-08-05T06-00-00-DEFINITION-BLOCK-AND-IMPLICIT-THUNK-POLICY.md`
