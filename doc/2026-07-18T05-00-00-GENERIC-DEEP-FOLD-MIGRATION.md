# Generic Deep-Fold Migration Plan

Date: 2026-07-18

## Status

Implemented and verified for the current source language. Core terms and the
evaluator support clause collections; the current surface `handle` syntax and
occurrence-local classifier solver still introduce one operation clause per
fold. Extending that source boundary is recorded below rather than hidden as
part of this migration.

## Decision

Replace the separate Core `BIND`, `HANDLER`, `HANDLE`, and `HANDLER_TYPE`
families with one generic deep fold over computations:

```text
DEEP_FOLD(
	computation,
	return_clause,
	op_clause(operation_0, clause_0),
	...,
	op_clause(operation_n, clause_n)
)
```

The fold always has one return clause. It may override zero or more operations.
Operations without an explicit clause are forwarded with a continuation that
deeply re-enters the same fold.

No compatibility representation is retained. Artifact version 46 is rejected
after the migration, old Core tags are deleted, and all accepted prototype
tests are rewritten to assert the new representation.

## Semantic Model

For a computation tree

```text
T_Sigma(X) = mu Y. X + sum_i A_i * (B_i -> Y)
```

the fold supplies:

```text
return_clause : X -> C
op_clause_i   : A_i -> Thunk(B_i -> C) -> C
```

where `C` is a computation classifier. Omitted operation clauses use the
derived forwarding algebra:

```text
forward_i(a, k) = OPERATION_REQUEST(
	op_i,
	a,
	THUNK(LAMBDA(b, DEEP_FOLD(APP(FORCE(k), b), clauses)))
)
```

The reduction equations are:

```text
DEEP_FOLD(RETURN(v), r, clauses)
  --> APP(r, v)

DEEP_FOLD(OPERATION_REQUEST(op_i, a, k), r, clauses)
  --> APP(APP(clause_i, a), deep_continuation(k))
      when clause_i is present

DEEP_FOLD(OPERATION_REQUEST(op_j, a, k), r, clauses)
  --> OPERATION_REQUEST(op_j, a, deep_continuation(k))
      when clause_j is absent
```

`BIND(M, K)` becomes exactly:

```text
DEEP_FOLD(M, K)
```

The current one-operation handler becomes:

```text
DEEP_FOLD(M, return_clause, op_clause(operation, operation_clause))
```

`RETURN`, `OPERATION_REQUEST`, and `DEEP_FOLD` are the effect-tree Core. Lambda,
App, Match, Thunk, and Force remain independent term constructors; this change
does not classify beta or iota reduction as effect handling.

## Core Representation

### TermDB

Add one term tag:

```text
PROTOTYPE_TERM_DEEP_FOLD
```

Its fixed fields are:

```text
computation
return_clause
first_clause
clause_count
```

Add a TermDB-owned clause arena:

```text
struct prototype_deep_fold_clause {
	uint32_t operation;
	uint32_t body;
};
```

`body` is the existing curried clause value
`LAMBDA(argument, LAMBDA(continuation, computation))`. Keeping binders in
ordinary Lambda nodes preserves the existing tagless binder representation and
avoids a second scope mechanism.

Construction rejects duplicate operation identities. Clause order is preserved
in the current representation. Evaluation is order-independent because
duplicates are forbidden, but independently constructed folds with differently
ordered equivalent clause sets are not yet hash-consed together. Canonical
clause-set ordering remains follow-up work before a multi-clause surface syntax
is introduced.

Delete:

```text
PROTOTYPE_TERM_BIND
PROTOTYPE_TERM_HANDLER
PROTOTYPE_TERM_HANDLE
PROTOTYPE_TERM_HANDLER_TYPE
```

Do not introduce aliases or constructors that recreate these tags.

### OperationGraph

Replace `PROTOTYPE_OPERATION_BIND` and `PROTOTYPE_OPERATION_HANDLE` with
`PROTOTYPE_OPERATION_DEEP_FOLD`.

An operation occurrence records:

```text
computation operation
return-clause operation
zero or more operation-clause occurrences
```

Core clauses use a TermDB-owned fixed-capacity arena. OperationGraph retains the
existing one-clause occurrence fields because the current source grammar can
introduce only one handler clause. It does not reuse the Match case arena.

The current source `handle` syntax contributes one clause. Computation block
sequencing contributes zero clauses. Core construction, equality, substitution,
relocation, printing, artifact readback, and evaluation support a clause
collection. The classifier solver currently rejects collections larger than one;
adding multi-clause source syntax therefore requires a solver extension, but no
further Core representation migration.

## Typing and Proofs

Replace specialized bind/handler/handle constraints with one fold-result
constraint. Its inputs are occurrence-local classifiers for:

1. the source computation;
2. the return clause;
3. each selected operation declaration;
4. each operation clause.

For source classifier `Comp(E, A)` and return clause
`Pi(A, Comp(F, B))`, the provisional output is `Comp(R union F, B)`, where
`R` is the source effect row after removing every handled operation. Each
operation clause must accept:

```text
operation argument A_i
Thunk(Pi(B_i, output_computation))
```

and return the same output computation classifier. Unhandled effects remain in
`R`; clause effects are unioned into the output row through the existing
effect-row equation machinery.

The zero-clause specialization yields the existing bind equation `E union F`.
It is not implemented by a separate typing helper or proof rule.

Replace proof kinds:

```text
BIND_INTRO
HANDLER_INTRO
HANDLE_ELIM
```

with one `DEEP_FOLD_ELIM` proof. Its premises are the source, return clause, and
operation clauses selected by the typed OperationGraph occurrence. The proof
validator reconstructs the same generic rule; it must not infer a hidden
first-class handler type.

Rename `HANDLER_RESULT` verification obligations to `DEEP_FOLD_RESULT` and let
them cover dependent results for both the zero-clause and clause-bearing forms.
The schema records the source occurrence, return-clause occurrence, clause
range, result family, effect row, normalization profile, and solver budget.

## Evaluator

Create one evaluator branch for `PROTOTYPE_TERM_DEEP_FOLD`.

1. Normalize the source computation without host operation dispatch.
2. On `RETURN(v)`, apply the return clause.
3. On a request with a matching clause, construct the deep continuation and
   invoke that clause.
4. On a request without a matching clause, rebuild the request with that same
   deep continuation.
5. If the source takes an ordinary reduction step, rebuild the fold around the
   reduced source.

The current BIND and HANDLE evaluator implementations contain two versions of
steps 1, 4, and 5. They must be deleted rather than retained as special fast
paths. A later optimization may recognize zero clauses, but it must preserve
the generic representation and equations.

## Artifact Migration

Increment the artifact format to version 47.

The graph section serializes clauses inline with each reachable fold node:

```text
term_node <term-id> DEEP_FOLD <computation> <return-clause> <clause-count>
  (<operation> <body>)*
```

Sparse graph reachability marks both terms referenced by every reachable
clause. Inline serialization removes sparse clause holes; readback validation
still requires every referenced term slot to be present, not merely within
capacity.

The OperationGraph artifact section must likewise serialize generic fold
occurrences and their clause occurrence metadata. Version 46 is intentionally
not accepted. No read-time conversion from BIND/HANDLER/HANDLE is added.

## Surface Lowering

The source syntax does not gain a `fold` keyword in this migration.

1. A computation block binding lowers to a zero-clause `DEEP_FOLD` whose return
   clause is the existing continuation Lambda.
2. `handle (...) with (...)` lowers directly to one `DEEP_FOLD` occurrence.
   It no longer creates an intermediate HANDLER term.
3. `perform` continues to lower to `OPERATION_REQUEST`.
4. Removed `#.bind` and `#.force` syntax stays removed.

This preserves the source policy while making the Core representation generic.

## File Migration Map

### `src/prototype/term.h` and `term.c`

- replace tags and union fields;
- add the clause arena and constructor API;
- update allocation and initialization;
- update term semantics/category checks;
- update interning, structural/cross-DB equality, canonical hashing;
- update binder occurrence checks and substitution;
- update clone/remap and external reference resolution;
- replace evaluator branches with generic deep fold;
- update normalization equality and printing.

### `src/prototype/ast.h` and `ast.c`

- add the OperationGraph fold clause arena;
- replace operation tags and constraints;
- lower blocks and handlers to the same operation form;
- merge propagation, materialization, validation, and runtime frames;
- rename residual verification records;
- update sparse artifact traversal, serialization, readback, relocation, and
  imported-term cloning;
- bump artifact format to 47.

### `src/prototype/judgement.h` and `typing.c`

- replace computation constraint and proof kinds;
- replace specialized solvers with one fold solver;
- use occurrence-selected classifiers when Core nodes are shared;
- validate one generic proof rule;
- retain operation-request typing as the free-algebra introduction rule.

### `src/prototype/type_declaration.c`

- update structural shape traversal for `DEEP_FOLD` and its clause arena;
- remove handler-type shape logic.

### Tests

- update Core shape assertions from `BIND` and `HANDLE(HANDLER(...))` to
  `DEEP_FOLD`;
- update proof labels to `deep-fold-elim`;
- preserve block sequencing, nested sequencing, and shared-Core regressions;
- preserve handled return, handled operation, unhandled deep forwarding,
  nested handlers, dependent handler result, and artifact round-trip tests;
- add a direct equivalence regression showing block bind and a handler with no
  operation override use the same Core constructor;
- assert that old artifact version 46 is rejected.

## Implementation Progress

- [x] Audit existing TermDB, OperationGraph, typing, evaluator, and artifact
  ownership of BIND and handler data.
- [x] Record the no-compatibility migration design.
- [x] Add TermDB deep-fold clauses and remove old term tags.
- [x] Migrate evaluator reduction to one deep-fold implementation.
- [x] Migrate JudgementDB constraints, proof construction, and proof validation.
- [x] Migrate OperationGraph constraints and residual verification.
- [x] Lower computation blocks and source handlers to generic deep folds.
- [x] Migrate artifact graph and operation sections to version 47.
- [x] Update structural type keys, cloning, relocation, and readback.
- [x] Update permanent tests and expected printed forms.
- [x] Run examples 01 through 09 and all prototype test scripts.
- [x] Record final implementation deviations and verification evidence.
- [x] Commit and push `main`.

## Implementation Findings

1. The fold clause arena must be copied into sparse artifact slices. Copying
   only the reachable term node left a valid `first_clause` pointing at a
   zero-initialized arena. The v47 artifact round-trip regression now covers a
   clause-bearing fold.
2. OperationGraph branch-body occurrences are not the same Core nodes as the
   curried clause Lambdas stored in TermDB. Proof reification now validates
   clause premises against the authoritative Core fold instead of assuming
   those occurrence IDs coincide.
3. TermDB uses a fixed clause arena, matching its existing fixed-capacity
   storage model. Artifact clauses are inline rather than a second sparse slot
   section.
4. The generic Core/evaluator accepts multiple clauses. The current source and
   classifier solver support zero or one clause. Multi-clause typing must fold
   residual effect rows across all selected operations and validate every
   clause against one output computation classifier.

## Verification Evidence

Verified on 2026-07-18:

- `make -j2` and `make reader`;
- examples `01_bool.p` through `07_add.p` and `09_list_induction.p`;
- `test_cbpv_boundary.sh`;
- `test_cbpv_surface.sh`;
- `test_shared_core_occurrences.sh`;
- `test_dependent_pi.sh`;
- `test_artifact_flow.sh`, including v46 rejection and clause-bearing v47
  artifact readback.

## Verification Gate

The migration is complete only when:

1. `rg` finds no old Core or OperationGraph bind/handler tags or constructors;
2. computation blocks and handlers both print `DEEP_FOLD`;
3. handled operations are deep, and omitted operations remain forwarded;
4. proof validation succeeds after artifact round-trip;
5. examples 01 through 09 pass;
6. `test_cbpv_surface.sh`, `test_cbpv_boundary.sh`,
   `test_shared_core_occurrences.sh`, `test_dependent_pi.sh`, and
   `test_artifact_flow.sh` pass;
7. all additional executable `src/prototype/test_*.sh` scripts pass or any
   unrelated pre-existing failure is documented with reproduction evidence.
