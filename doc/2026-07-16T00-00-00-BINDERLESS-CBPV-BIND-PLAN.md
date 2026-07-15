# Binderless CBPV Bind Refactoring Plan

Date: 2026-07-16

## Goal

Make the Core TermDB use the two CBPV eliminators directly:

```text
APP  : Pi(A, B) x A -> B
BIND : Comp(E, A) x Pi(A, Comp(F, B)) -> Comp(E union F, B)
```

`BIND` must not bind a name itself.  All local names in the Core TermDB are
bound by existing `LAMBDA`, `PI`, or `MATCH` nodes.  The continuation supplied
to `BIND` is a `LAMBDA` computation for the initial implementation.

This removes the current generalized rule that permits a bind body to be a
raw function computation and makes the bind result itself a `Pi`.

## Required Semantic Boundary

The initial rule is intentionally non-dependent in its result:

```text
M : Comp(E, A)
K : Pi(A, Comp(F, B))
----------------------
BIND(M, K) : Comp(E union F, B)
```

`B` must not contain the continuation argument.  Dependent Kleisli extension
is not part of this refactoring.

The reduction rules are:

```text
BIND(RETURN(v), LAMBDA(x, N))
  -> N[v/x]

BIND(REQUEST(op, arg, k1), k2)
  -> REQUEST(op, arg,
       THUNK(LAMBDA(r, BIND(APP(FORCE(k1), r), k2))))
```

There is deliberately no rule that pushes `BIND` below a returned `LAMBDA`.

## Initial-State Findings

The current implementation is not only surface syntax sugar.  It has a
three-field Core node:

```text
BIND(computation, binder_id, body)
```

and each layer relies on that binder:

- `src/prototype/term.h` and `src/prototype/term.c` store and construct it.
- `src/prototype/term.c` compares, hashes, substitutes, serializes, imports,
  prints, and evaluates it.  Evaluation currently pushes a bind below a
  lambda.
- `src/prototype/typing.c:bind_result_classifier` accepts both returning and
  raw-function body classifiers; the raw-function branch recursively sequences
  below the Pi codomain.
- `src/prototype/typing.c:solve_bind_constraint` publishes a BIND-specific
  local binder assumption.
- `src/prototype/ast.c` uses that local assumption during fixed-point
  classifier solving and records a BIND proof context.
- `src/prototype/reader.c` exposes the old `let x <- M in N` syntax through
  `PROTOTYPE_AST_BIND`.
- artifacts serialize three BIND fields and must change format.

The existing `bind-function.p` regression deliberately tests the behavior to
be removed:

```text
let x <- (return #1) in \y : #.Int => x
```

It currently produces a BIND classified as Pi.  Under this plan it is not a
valid BIND result.  A source expression that wants to return a function value
must explicitly be a returning computation of a suspended function once
closures are introduced.

## Implementation Sequence

### 1. Core TermDB

Change `PROTOTYPE_TERM_BIND` to:

```text
as.bind.computation
as.bind.continuation
```

Change the constructor to:

```text
prototype_term_bind(db, computation, continuation, out)
```

Update structural equality, canonical hashing, free-binder traversal,
substitution, external-reference resolution, import rewriting, debug printing,
and artifact read/write/validation.  BIND itself no longer changes a binder
environment; its continuation lambda does.

### 2. Evaluator

Remove the current BIND-under-LAMBDA rule.  Evaluate the computation operand.
On `RETURN(v)`, apply the continuation with ordinary `APP`.  On an operation
request, compose the request continuation with the BIND continuation using a
fresh lambda binder belonging to that newly constructed lambda only.

### 3. Typing and JudgementDB

Replace `bind_result_classifier` with a rule that:

1. reads `M` as `Comp(E,A)`;
2. reads `K` as `Pi(A, Comp(F,B))` after classifier WHNF;
3. checks the Pi domain against `A` by normalization equality;
4. rejects a codomain result or effect row that captures the Pi binder;
5. produces `Comp(E union F,B)`.

Delete BIND-specific binder assumptions and the `BIND_RESULT` proof context.
The normal lambda-introduction derivation for the continuation supplies its
own scoped binder assumption.

### 4. Lowering and Surface Syntax

Introduce application-shaped intrinsic syntax:

```text
#.bind M (\x : A => N)
```

The intrinsic lowers to `BIND(M, LAMBDA(x,N))`.  It introduces no AST binder
outside the lambda.

Remove `let`, `in`, `TOKEN_BIND_ARROW`, `PROTOTYPE_AST_BIND`, and their old
lowering path.  Do not retain a compatibility lowering.

Automatic value-demand lowering may still need a synthetic continuation.  It
must construct `LAMBDA(fresh_binder, body)` and then use that lambda as the
second BIND operand; it must not create a BIND-owned binder.

### 5. Tests and Artifact Version

- Replace old `let` tests with `#.bind` tests.
- Add a test proving `BIND(RETURN(v), lambda)` reduces through APP.
- Add a test proving a BIND classifier is always `Comp`, never `Pi`.
- Add an operation-request forwarding test for the new continuation shape.
- Bump the artifact format version because BIND payload arity changes; reject
  old artifacts rather than accepting both layouts.
- Run the CBPV tests, artifact tests, shared-core tests, and examples 01-09.

## Decision Log

- `BIND` is a fold/Kleisli extension over returning computations, not a second
  binder form.
- `APP` consumes `Pi x Value`; `BIND` consumes `Comp x Pi`.
- Passing a named continuation as an ordinary value remains a later closure
  problem.  This phase supports inline lambda continuations and raw Pi
  continuations already available in computation position.
- No dependent bind result is introduced in this phase.

## Implemented Result

The refactoring now uses the following Core TermDB shape:

```text
BIND(computation, continuation)
```

The continuation is a normal `LAMBDA` TermDB node. No BIND-owned binder,
`BIND_RESULT` proof context, or `let` AST node remains.

`#.bind` is recognized only as a two-argument application spine:

```text
#.bind M (\x : A => N)
```

It lowers directly to `BIND(M, LAMBDA(x, N))`. A bare `#.bind` has no runtime
TermDB node and cannot be used as an ordinary function value.

`PERFORM` now preserves the already-instantiated operation application
classifier as an occurrence fact. This is required so a BIND continuation
whose body performs an operation obtains its `Comp(E, A)` classifier from the
operation application, rather than from the operation head.

The artifact format is version 37 because BIND payloads now have two term
references instead of a computation, binder id, and body.

## Verification

The following pass after the refactoring:

- `make` and `make reader`
- `src/prototype/test_cbpv_surface.sh`
- `src/prototype/test_cbpv_boundary.sh`
- `src/prototype/test_artifact_flow.sh`
- `src/prototype/test_shared_core_occurrences.sh`
- `examples/01_bool.p` through `examples/07_add.p`, plus
  `examples/09_list_induction.p`

The old dependent-bind test was removed deliberately. The implemented rule
requires the continuation result classifier and effect row to be independent
of the continuation binder. Dependent Kleisli extension belongs to a later
dependent-CBPV design step, not to this binary BIND rule.
