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
| Closed fold effect join | supported | closed return, clause, and residual rows form one monotone carrier |
| Symbolic fold effect join | not supported | occurrence-local latent rows are not retained in the request row |
| Effect weakening proof | supported for closed rows | symbolic inclusion remains a residual obligation |
| Modular higher-order forwarding | not supported | no typed policy transforms an unknown inner computation |
| Resource lifetime and multiplicity | not supported generally | direct resumption counting is insufficient |
| Artifact preservation | supported for the current form | complete semantic replay of carrier/row equations is pending |

The accurate feature statement is:

> A Program supports thunk-encoded higher-order requests, plural deep folds,
> explicit use of inner computations, and opaque default forwarding. It does
> not yet support a complete modular higher-order handler calculus or
> resource-safe scoped effects.

### 3.1 Three independent dimensions

The following questions must not be conflated:

1. **Clause cardinality:** can one fold dispatch several operation names?
2. **Request order:** can an operation argument contain a suspended
   computation?
3. **Static closure:** can the compiler prove the effects exposed by every
   discard, force, forwarding, and inner-fold choice?

The current answers are respectively **yes**, **yes**, and **only for closed
rows**. A surface AST which accepted one clause would be a syntax limitation;
it would not prove that thunk-encoded higher-order operations are impossible.
Conversely, accepting several clauses and `THUNK(M)` does not solve latent-row
typing.

### 3.2 Four compiler layers

The remaining work must also be separated by compiler layer:

| Layer | Established capability | Remaining problem |
|---|---|---|
| Surface/OperationGraph | plural clauses and thunk payloads are represented | no general occurrence-local latent-row atom |
| Runtime reduction | discard, force once, repeated force, explicit inner fold, and opaque forwarding are representable | no generic scoped transformation through an unknown thunk |
| Static solver | closed joins, residuals, and inclusions are solved | symbolic `scope<E>` elimination is residual |
| Proof/artifact | closed fold carrier and weakening are replayed | symbolic row equations cannot be accepted by strict policy |

Runtime success for a closed graph does not establish modular static typing.
Conversely, an unresolved symbolic row does not imply that the runtime
representation is missing.

### 3.3 Five questions for every higher-order operation

Every proposed operation must be reviewed along five independent axes:

1. **Representation:** can its argument contain `THUNK(M)` without executing
   `M`?
2. **Demand:** may a clause discard, force once, force repeatedly, or explicitly
   fold `M`?
3. **Static effects:** where is the latent row of `M` retained per request
   occurrence?
4. **Proof authority:** can the final classifier be replayed from proof premises
   after artifact readback?
5. **Resources:** is duplication or escape of the thunk legal?

The first two questions are already answered by the current Core and runtime.
The third now has a serializable TermDB vocabulary but is not yet connected to
request inference. The fourth blocks that connection. The fifth is a later
linear/resource discipline and must not be inferred merely from a one-shot
runtime counter.

### 3.4 Current implementation checkpoint

TermDB and artifact v58 can represent and preserve:

```text
EFFECT_ROW_OPERATION(operation_id, latent_row)
```

This is the concrete representation of the schematic `scope<E>` atom. It is
included in canonical hashing, graph traversal, binder substitution,
normalization comparison, relocation, printing, serialization, and artifact
reference validation.

It is intentionally **not yet generated automatically** by
`OPERATION_REQUEST_INTRO`. The current request proof has only the applied
operation and continuation as premises. It does not retain the selected
argument-occurrence classifier from which `E` must be derived. In addition,
the OperationGraph can still hold a provisional closed classifier while
JudgementDB later derives a more precise classifier. An enclosing fold may
therefore reify a proof against the provisional classifier.

The accepted next boundary is:

1. make the selected argument occurrence an explicit request-proof premise;
2. make the resulting JudgementDB derivation authoritative for the request
   operation occurrence;
3. prevent a fold from reifying until that authoritative derivation exists;
4. only then generate `EFFECT_ROW_OPERATION(op,E)` and implement handled-atom
   residual elimination.

Generating atoms before these authority changes is rejected because it makes
the same OperationGraph node carry incompatible provisional and proven
classifiers. A local normalization-equality workaround would hide, rather than
remove, that proof-source duplication.

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
runtime dispatch, proof tuple, and artifact v58. Clause cardinality is
independent of whether a request argument contains a thunk.

### 4.4 Forward an unknown request opaquely

When no clause matches, the fold preserves the request and its thunk argument.
This is coherent forwarding, but it is not recursive inner handling.

### 4.5 Execute closed examples under partial verification

The force-once and force-twice fixtures execute with the expected behavior.
Strict verification rejects them because latent-row constraints remain
undischarged. Runtime success and static verification are separate claims.

### 4.6 Join effects introduced by closed handler branches

When every relevant row is closed, the OperationGraph fixed point can now join
the return row, clause-body rows, and unhandled input residual. A pure return
clause and a `{print}` abortive clause therefore produce a
`Comp({print}, Text)` fold. Strict compilation and artifact readback accept
this case.

This does not yet solve `scope<E>`: a forced thunk whose `E` was hidden by the
first-order request result still leaves a symbolic obligation.

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

Lambda synthesis keeps the least closed effect row derivable from its body as
the principal classifier. A widened body judgement is admissible evidence for
a consumer, but it is not fed back into principal Pi synthesis. If two closed
candidate Pi classifiers have the same domain and result and their rows are
ordered by inclusion, the smaller row is selected. Incomparable or symbolic
candidates remain ambiguous. This prevents effect weakening from becoming a
second, order-dependent source of Lambda identity.

### 7.4 Artifact validation

Readback must verify parameterized atom identity and arguments, row inclusion,
join/residual equations, and correspondence between fold clauses and proof
premises. Serialization without semantic replay is not proof authority.

For partial-policy artifacts, a symbolic carrier comparison may remain a
residual obligation. The proof validator checks its structural domains and
result type but cannot claim row equality. Strict policy must reject the
artifact until all such rows are materialized. For closed rows, validator
acceptance requires exact carrier equality and row inclusion checks.

The artifact format may serialize a parameterized operation atom now, but
serialization alone is not evidence that a source request was assigned that
atom. Once occurrence generation is enabled, `OPERATION_REQUEST_INTRO` must
carry the request argument and its selected classifier as a proof premise so
readback can independently verify the latent row.

### 7.5 Two fixed points, one proof boundary

Closed fold carrier inference crosses two existing compiler layers:

1. OperationGraph monotonically approximates the carrier so continuation
   binders can be classified while their clause bodies are still being built.
2. JudgementDB replays the final carrier using `EFFECT_WEAKEN` and
   `COMPUTATION_FOLD_ELIM` premises.

OperationGraph bindings are inference state, not proof authority. JudgementDB
and artifact validation remain the authority boundary. Replacing either layer
with an unchecked classifier overwrite is rejected.

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
11. Treat clause cardinality, thunk-encoded request order, and static latent-row
    closure as separate capability axes.
12. Permit monotone closed-row carrier widening during OperationGraph inference,
    but require JudgementDB proof replay before strict export.
13. Allow symbolic carrier compatibility only as an explicit partial-policy
    residual; it is never strict proof evidence.
14. Keep principal Lambda synthesis separate from `EFFECT_WEAKEN`; select the
    least comparable closed effect row and reject incomparable candidates.
