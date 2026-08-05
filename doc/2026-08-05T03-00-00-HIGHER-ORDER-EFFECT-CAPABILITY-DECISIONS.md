# Higher-Order Effect Capability Decisions

Date: 2026-08-05

Status: accepted design boundary for the prototype

Related documents:

- `2026-08-05T01-00-00-THUNK-ENCODED-HIGHER-ORDER-OPERATIONS.md`
- `2026-08-05T02-00-00-HIGHER-ORDER-OPERATION-RESOURCE-IMPLEMENTATION-PROGRESS.md`

## 1. Purpose

This document restates the current problem after reviewing representation,
typing, operational behavior, modularity, artifacts, and resources separately.

The central correction is:

> Passing `THUNK(M)` to an operation is sufficient to represent a
> higher-order request, but it is not sufficient to derive a modular static
> semantics for handling every such request.

The missing work is not one syntax form or Core node. It is a set of contracts
around effect-row abstraction, fold typing, forwarding, and resource use.

## 2. Current Semantic Objects

The prototype has the following distinct objects:

1. `THUNK(M)` is a value containing a computation graph without executing it.
2. `OPERATION_REQUEST(op, argument, continuation)` is a suspended operation
   request with an outer continuation.
3. `COMPUTATION_FOLD` interprets `RETURN` and selected operation requests.
4. An effect row records which operation families may remain observable from a
   computation.
5. `JudgementDB` records successful typing derivations. Compiler-local
   constraints are not runtime state and are not TermDB nodes.

A request argument may itself be `THUNK(M)`. The representation therefore
already distinguishes the inner computation `M` from the outer continuation.
No additional `HIGHER_ORDER_OPERATION` Core tag is required for that purpose.

## 3. Multi-Axis Capability Result

The implementation must not be described by one higher-order yes/no flag.

| Axis | Current capability | Boundary |
|---|---|---|
| Request representation | supported | an operation can receive `THUNK(Comp(E,A))` |
| Direct row instantiation | supported | application can specialize a quantified row from a concrete thunk |
| Plural dispatch | supported | one fold can contain several nominal clauses |
| Clause demand | supported operationally | direct-style binding can force a thunk once or repeatedly |
| Outer continuation handling | supported | deep fold recursively handles the outer continuation |
| Inner computation handling | explicit only | a selected clause may force or explicitly fold the thunk |
| Unknown forwarding | opaque forwarding supported | generic fold does not cross `THUNK` |
| General static latent effects | not supported | the first-order request result does not preserve the latent row |
| Fold effect join and weakening | not supported | clauses currently require an exact common computation carrier |
| Modular higher-order forwarding | not supported | no typed policy transforms an unknown inner computation |
| Resource lifetime and multiplicity | not supported generally | direct resumption counting is insufficient |
| Artifact preservation | supported for the current form | complete semantic replay of carrier/row equations is pending |

The accurate feature statement is:

> A Program supports thunk-encoded higher-order requests, plural deep folds,
> explicit use of inner computations, and opaque default forwarding. It does
> not yet support a complete modular higher-order handler calculus or
> resource-safe scoped effects.

## 4. What Is Already Possible

### 4.1 Store a computation in an operation argument

An operation domain may be:

```text
forall E. Thunk(Comp(E, A))
```

A request may receive `&M` without executing `M`. Direct operation application
can recover `E` from a concrete `Thunk(Comp(E,A))` argument.

### 4.2 Select inner scheduling explicitly

A clause may discard the inner thunk, force it once, force an unrestricted
thunk repeatedly, or place an explicit fold around it before forcing it. These
are observably different programs; generic fold must not silently choose one.

### 4.3 Handle several operation names in one fold

Plural clauses are represented in the surface AST, OperationGraph, TermDB,
runtime dispatch, proof tuple, and artifact v57. Clause cardinality is
independent of whether a request argument contains a thunk.

### 4.4 Forward an unknown request opaquely

When no clause matches, the fold preserves the request and its thunk argument.
This is coherent forwarding, but it is not recursive inner handling.

### 4.5 Execute closed examples under partial verification

The force-once and force-twice fixtures execute with the expected behavior.
Strict verification rejects them because latent-row constraints remain
undischarged. Runtime success and static verification are separate claims.

## 5. Precise Static Problem

Consider:

```text
scope : forall E. Thunk(Comp(E, A)) -> Comp({scope}, B)
```

A concrete argument may establish `E = {print}`. The result intentionally
contains only `{scope}` because constructing the request does not execute its
thunk. A handler therefore cannot recover `E` from the first-order result row.

This creates two separate static problems.

### 5.1 Occurrence-local latent rows

One input computation may contain requests whose thunks have different rows:

```text
scope<&M1>  where M1 : Comp(E1, A)
scope<&M2>  where M2 : Comp(E2, A)
```

One fold-wide substitution for declaration binder `E` cannot represent both
occurrences. The latent row must be retained per request occurrence. The
intended static representation is a parameterized effect atom, written
schematically as `scope<E>`. This is an effect-row object, not another runtime
request node.

### 5.2 Fold carrier fixed point

Suppose the return clause is pure while one clause forces its thunk:

```text
return clause : Comp({}, C)
scope clause  : Comp(E, C)
```

The fold result row must be solved as:

```text
F = join(return_row, clause_rows..., residual_input_row)
```

Every branch then needs evidence `branch_row <= F`. This is a fixed-point
constraint because continuation classifiers refer to the fold carrier while
clause bodies determine effects contributing to that carrier.

The current solver only closes bitset-compatible union/residual equations and
the current fold rule chooses an exact carrier too early. A parameterized atom
alone cannot repair this second problem.

## 6. Rejected Shortcuts

### 6.1 Do not add every latent row to the request result

`Comp(E union {scope}, B)` would report `E` even when a handler discards the
thunk. It destroys useful opacity.

### 6.2 Do not use closed-graph inspection as the general rule

Inspecting known request occurrences can be an optimization. It is not a
compositional type for a function receiving an unknown computation.

### 6.3 Do not make generic fold transparent through `THUNK`

Crossing `THUNK` would commit to execution or recursive transformation. The
default remains an opacity boundary.

### 6.4 Do not silently widen effects

Using `Comp(E1,A)` as `Comp(E2,A)` requires explicit evidence of `E1 <= E2`.
This must not become hidden subtyping or definitional equality.

### 6.5 Do not use binder variables as solver metavariables

`EFFECT_ROW_VAR` is graph-level syntax. A fold output row is a compiler-local
unknown constrained by several branches. Solver metavariables belong in the
constraint arena and should be materialized into TermDB only after solving.

## 7. Accepted Static Architecture

### 7.1 Durable graph-level rows

TermDB represents serializable row expressions: closed labels, quantified row
variables, unions, and a parameterized operation atom carrying latent rows.
They are program/type data, not mutable solver cells.

### 7.2 Compiler-local constraints

The row solver must support equality, n-ary join, residual, inclusion,
parameterized-atom elimination, and unresolved residuals under partial policy.
Solutions are occurrence-local; declaration binders are not globally rewritten.

### 7.3 Proof-producing materialization

After solving, the compiler materializes the fold classifier and explicit
inclusion proofs for branches. The runtime-erased rule is:

```text
M : Comp(E1, A)    E1 <= E2
--------------------------------
M : Comp(E2, A)
```

This `EFFECT_WEAKEN` rule is restricted to computation effects. It is not
value-type subtyping and does not alter definitional equality.

### 7.4 Artifact validation

Readback must verify parameterized atom identity and arguments, row inclusion,
join/residual equations, and correspondence between fold clauses and proof
premises. Serialization without semantic replay is not proof authority.

## 8. Operational Decisions

1. `COMPUTATION_FOLD` remains the common Core operation for sequencing and
   handling.
2. Matching a clause folds the outer continuation deeply, as today.
3. Generic forwarding preserves request arguments, including thunks, opaquely.
4. Executing or recursively handling an inner thunk is explicit in a clause.
5. A future scoped policy may transform inner and outer computations, but it
   requires a separate typed contract.
6. Parameterized effect atoms are static evidence and do not alter runtime
   dispatch identity.

## 9. Resource Boundary

Higher-order representation does not imply resource safety. A thunk may capture
a file handle or region token and then be stored, discarded, or replayed.
Resource operations additionally require non-Term runtime values, a resource
table, finalization scopes, region/non-escape constraints, multiplicity,
branch-consistent ownership, and backend capability validation.

Effects describe observable operations. Ownership describes legal value use.
They must not be collapsed into one label set.

## 10. Implementation Order

1. Separate solver-local row metavariables from graph-level row binders.
2. Add row inclusion and n-ary join constraints, initially exact for closed
   rows and residual for unsupported symbolic cases.
3. Add and artifact-validate the runtime-erased `EFFECT_WEAKEN` proof.
4. Add the graph-level parameterized operation atom and artifact support.
5. Generate one parameterized atom per higher-order request occurrence and its
   handler-elimination constraint.
6. Solve the fold carrier from return, clause, and residual rows before
   reifying the fold proof.
7. Make force-once and force-twice pass strict verification and artifact tests.
8. Add explicit-inner-fold and opaque-forwarding tests.
9. Only then add resource values, regions, finalization, and multiplicity.

Adding the parameterized atom before steps 1-3 is rejected: it would add
durable syntax whose equations the current solver and proof kernel cannot
justify.

## 11. Acceptance Tests

The static higher-order milestone is complete only when:

1. one computation may contain two requests with distinct latent rows;
2. discarding both thunks exposes neither latent row;
3. forcing one exposes only its row;
4. forcing both exposes their join;
5. a pure return clause is accepted through explicit inclusion evidence;
6. strict compilation rejects unsolved symbolic inclusion;
7. partial compilation records residuals without a valid export claim;
8. artifact readback rejects forged atom, inclusion, join, or fold premises;
9. opaque forwarding preserves an unknown thunk without executing it; and
10. examples 01-09 and all existing prototype tests remain green.

## 12. Final Decisions

1. Keep thunk encoding for higher-order request arguments.
2. Do not add a separate higher-order request TermDB tag.
3. Keep plural `COMPUTATION_FOLD` and opaque default forwarding.
4. Treat direct force and explicit inner fold as operational choices.
5. Describe strict static higher-order support as incomplete.
6. Use occurrence-local parameterized effect atoms, not one fold-wide row
   substitution.
7. Solve fold output effects with compiler-local constraints and n-ary join.
8. Require explicit, runtime-erased effect inclusion proofs.
9. Keep effect inclusion out of definitional equality and value subtyping.
10. Delay resource claims until ownership and lifetime machinery exists.
