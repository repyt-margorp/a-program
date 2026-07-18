# Runtime Strict Value Context Migration Plan

Date: 2026-07-17

## Status

Implemented and verified in `src/prototype/`.

This document supersedes the blanket rule from
`2026-07-17T03-00-00-COMPUTATION-BLOCK-QUOTATION-PLAN.md` that every
computation in a value position is rejected. The raw CBPV core remains strict;
only the surface elaboration policy changes.

## Objective

A runtime consumer that requires a value of type `A` may receive either:

```text
v : A
M : Comp(E, A)
```

The first is passed directly. The second is executed exactly once and its
returned value is passed to the consumer:

```text
consumer M

==elaborate==>

BIND(M, \x : A => consumer x)
```

This makes ordinary A Program source use call-by-value evaluation while the
TermDB continues to use raw CBPV nodes. `APP` itself never accepts a
computation argument.

## Core Invariant

The migration must not add a rule of the form:

```text
APP(Pi(A, C), Comp(E, A))
```

The only core rule remains:

```text
f : Pi(A, C)    v : A
---------------------
APP(f, v) : C[v]
```

Surface sequencing introduces a separate `BIND` before that `APP`:

```text
M : Comp(E, A)    K : Pi(A, C)
--------------------------------
BIND(M, K) : sequence(E, C)
```

Existing BIND typing, effect-row union, dependent result residualization, and
runtime verification remain authoritative.

## Context Classification

### Runtime strict value contexts

The following runtime positions evaluate a returning computation before using
its result:

1. Ordinary function arguments.
2. Constructor fields, including partially applied constructor spines.
3. Match scrutinees.
4. Host-operation and `perform` value arguments.

Examples:

```text
toNat (negate Bool.false)

Nat.succ (computeNat x)

(computeBool x)
	@true  => trueBody
	@false => falseBody

perform (#.print (computeText x))
```

They elaborate respectively to the shapes:

```text
BIND(negate Bool.false, \b => toNat b)

BIND(computeNat x, \n => Nat.succ n)

BIND(computeBool x, \b => MATCH(b, ...))

BIND(computeText x, \text => perform (#.print text))
```

### Computation contexts

Lambda bodies, Match branch bodies, computation block tails, top-level
computation assignments, handler computation bodies, and explicit BIND bodies
remain computation contexts. A value in one of these positions is lifted with
`RETURN`.

### Quotation contexts

`&M` never executes `M`:

```text
M  : C
--------
&M : Thunk(C)
```

Quotation is the explicit way to pass a returning computation or raw function
computation as a value. Repeated FORCE may execute the quoted computation
repeatedly.

### Static type contexts

Type formation is not a runtime strict value context. It continues to use the
`PURE_TYPE_WHNF` normalization profile and must never dispatch an effect or
insert a runtime BIND merely to obtain a type.

The existing type-level pure extraction is retained in this migration. Its
termination budget, universe constraints, and eventual separation from graph
construction remain separate design work.

### Handler syntax

This migration does not redesign handler surface syntax. Existing handler
operation operands and handler bodies retain their current structural roles.
Runtime value operands reached inside those forms use the same strict-value
sequencing helper where they already pass through ordinary operation or
perform argument lowering.

## Classification Rule

Sequencing is selected by a runtime context and validated by classifier
constraints. It must not be selected only because a node has computation
polarity.

```text
expected A, actual A
	=> pass the value

expected A, actual Comp(E, A') and A' convertible to A
	=> BIND once, then pass its result

expected Thunk(C), actual C
	=> reject; explicit & is required

expected A, actual Pi(...)
	=> reject; a raw function is not a returning computation
```

During graph construction the expected and actual classifiers may not yet be
solved. The structural BIND may therefore be created provisionally, but its
input must generate the existing BIND classifier constraints. The solver must
prove that the input is a returning `Comp(E, A)` and that the continuation
domain is compatible with its result. An unresolved dependent result becomes
an existing dependent-BIND verification obligation; it is not silently
accepted as a value.

## Evaluation Order and Sharing

Runtime strict contexts use left-to-right call-by-value order. Every source
occurrence is executed once:

```text
f M N

==elaborate==>

BIND(M, \x =>
	BIND(N, \y =>
		APP(APP(f, x), y)))
```

Writing the same computation twice executes it twice:

```text
pair M M
```

A computation block remains necessary to execute once and share:

```text
{
	x := M;
	pair x x
}
```

Blocks also retain explicit sequencing, local result naming, and a stable
scope for effectful steps. This migration reduces boilerplate for nested
expressions; it does not make blocks obsolete.

## OperationGraph and TermDB Representation

The source occurrence remains represented in the typed OperationGraph. A
sequenced runtime value occurrence adds:

```text
input computation operation
continuation Lambda operation
BIND operation
consumer operation below the continuation
```

The TermDB receives the corresponding canonical `BIND`, `LAMBDA`, and consumer
nodes. Environment state is not stored in the TermDB. Evaluation substitutes
the value returned by the input computation into the continuation using the
existing BIND reduction.

The continuation binder must retain occurrence-local binder metadata. Shared
core VAR identity must not be used to infer source binder ownership.

## Implementation Plan

1. Keep `compile_ast_value_ref` as the strict value recognizer.
2. Keep a strict helper for contexts that must never execute a computation.
3. Add a runtime sequencing helper that:
   - first accepts an existing value;
   - otherwise lowers the source as a computation;
   - rejects a known raw function computation;
   - creates a continuation Lambda around the consumer;
   - emits BIND and binder assumptions through the existing operation graph;
   - lets BIND constraints validate unknown and returning computation kinds.
4. Route ordinary APP arguments through the runtime helper.
5. Route constructor fields through the runtime helper while preserving
   partially applied constructor values.
6. Route Match scrutinees through the runtime helper.
7. Route host-operation and perform arguments through the runtime helper.
8. Keep quotation, explicit FORCE, static type formation, and structural
   handler clauses on strict paths.
9. Remove or correct stale comments claiming implicit sequencing where the
   implementation rejects it.

The helper must not duplicate the BIND proof construction. It must construct
the same source operation shape as computation blocks and use the existing
fixed-point BIND solver.

## Verification Requirements

The migration is complete only when permanent tests prove all of the
following:

1. `toNat (negate Bool.false)` compiles to a BIND and evaluates correctly.
2. `Nat.succ (computeNat x)` compiles to a BIND around constructor formation.
3. `(computeBool x) @true ... @false ...` compiles to a BIND around Match.
4. A computed host-operation or perform argument is evaluated once before the
   request.
5. Multiple strict arguments execute left to right.
6. Repeated source occurrences execute repeatedly.
7. A computation block executes once and shares its bound result.
8. Runtime sequencing preserves distinct surface TypeViews.
9. Raw function computations still require explicit quotation with `&`.

## Implementation Record

The migration was completed on 2026-07-17.

### Lowering

`compile_ast_runtime_value_then` is the common runtime strict-value entry
point. It accepts an existing value or introduces a synthetic continuation and
`BIND` for a returning computation. Ordinary application spines are flattened
before lowering so their arguments are sequenced left to right around the
whole curried application.

Constructor spines use a dedicated continuation path. This is required for
two reasons:

1. A partially applied constructor is still a value.
2. The old recursive value probe emitted partial OperationGraph APP nodes
   before discovering a computed field. Falling back after that mutation could
   pass `RETURN(partial-constructor)` as the function of the next field APP.

The dedicated path now constructs:

```text
BIND(M1, \x =>
	BIND(M2, \y =>
		RETURN(Constructor x y)))
```

without a speculative lowering pass.

Match scrutinees, ordinary APP arguments, constructor fields, host-operation
arguments, and perform arguments all use the same strict-value policy.

### Runtime OperationGraph evaluation

The OperationGraph evaluator now evaluates the argument operation of APP and
PERFORM before reconstructing and reducing the corresponding TermDB node. This
is not environment storage in TermDB. The evaluator retains binder environment
state, while TermDB only receives the resulting canonical computation terms.

This distinction matters for synthetic BIND continuations: canonical TermDB
binders may be shared, but occurrence-local OperationGraph binders determine
which runtime value is supplied.

### Permanent tests

`src/prototype/test_cbpv_surface.sh` now covers:

- ordinary computed arguments;
- one-field and multiple-field computed constructor arguments;
- computed Match scrutinees;
- nested computed perform arguments;
- left-to-right evaluation;
- repeated evaluation versus block-local sharing;
- dependent BIND residual obligations across artifact write/read;
- rejection of a raw higher-order function without `&`;
- rejection of sequencing across distinct surface TypeViews.

The reusable source fixture is
`src/prototype/runtime_strict_value_check.p`.
7. A block binding executes once and shares its value.
8. `&M` remains a THUNK and does not execute `M` until FORCE.
9. A raw function passed as a higher-order value still requires `&f`.
10. A dependent result either closes under the compile budget or produces the
    existing residual dependent-BIND obligation.
11. Artifact write/read/link preserves the generated BIND occurrence graph.
12. Existing prototype build, CBPV, dependent-Pi, shared-core, and artifact
    regression suites pass.

## Progress Log

### 2026-07-17

* Design approved: runtime strict value contexts use implicit call-by-value
  sequencing; quotation and static type formation do not.
* Current implementation audit confirmed that `compile_ast_value_then`
  presently rejects every computation and that several comments still claim
  the removed sequencing behavior.
* Implementation and verification are pending.
