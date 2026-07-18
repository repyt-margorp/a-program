# Execution-Suppression CBPV Surface Plan

Date: 2026-07-18

## Status

Implemented and verified for the current raw CBPV surface. The implementation
changed one part of the original plan: Core boundary construction is
syntax-directed and happens once during OperationGraph lowering; classifier
solving validates that boundary afterward. It is not deferred until classifier
solving, because the classifier does not select between execute, suppress, and
call.

## Approval and Progress

The following decisions were approved on 2026-07-18:

1. Use surface type `&A` provisionally for a suspended computation returning
   `A`. Its initial elaboration is `Thunk(Comp(e, A))`, where `e` is a fresh
   latent effect-row variable. The spelling may be revised later, but the Core
   classifier and ownership rule must not depend on that spelling.
2. Treat `THUNK`, `FORCE`, and synthetic `BIND` selection as a constraint on a
   typed OperationGraph occurrence. Do not repeatedly construct and remap
   partial Core graphs while classifier information is still incomplete.
3. Remove ordinary surface `#.bind`; computation blocks become the source
   spelling of sequencing. Keep Core `BIND`.
4. Track implementation progress in this document and revise the plan when
   evidence from the implementation requires it.

Current implementation audit:

- [x] Confirmed that `&` currently parses only as `PROTOTYPE_AST_QUOTE` and
  rejects an operand already lowered as a Value.
- [x] Confirmed that block lowering currently aliases a Value RHS and creates
  `BIND` immediately for a Computation RHS.
- [x] Confirmed that callable application currently inserts `FORCE`
  immediately when the callee occurrence has Value polarity.
- [x] Confirmed that `#.bind` and `#.force` are special APP-spine forms rather
  than ordinary TermDB operations at the source-name layer.
- [x] Add the `&A` type AST, parser rule, latent-row ownership, and classifier
  lowering.
- [x] Preserve execute/suppress/call as occurrence-local OperationGraph
  structure, with classifier constraints validating the selected boundary.
- [x] Confirm that boundary selection is syntax-directed and does not require
  classifier-directed search or post-lowering Core remapping.
- [x] Migrate computation blocks, ordinary strict arguments, and callable
  application to the common occurrence-sensitive boundary path.
- [x] Remove surface `#.bind` without retaining a compatibility path.
- [x] Remove ordinary surface `#.force` after every raw FORCE use has a tested
  surface elaboration.
- [x] Add permanent direct and nested `&A`, shared-Core, strict-effect,
  dependent-handler, computed-Match, and removed-intrinsic regressions.
- [x] Rerun the Phase 6 matrix without `#.bind` or `#.force` compatibility
  parsing.

### Implementation Findings

The implementation established the following points:

1. A direct binder `m : &A` lowers to
   `Thunk(Comp(effect-row-variable, A))`. Every free latent row in a binder
   classifier is owned and generalized by the enclosing Lambda occurrence.
2. Nested references are representable. `&&A` owns two independent latent
   rows, and `&{ &m }` lowers to the raw shape
   `THUNK(RETURN(THUNK(...)))` rather than collapsing the nested thunk.
3. Effect-row specialization must follow the expected and actual classifier
   structure. The old one-level rule could specialize `&A` but not `&&A`.
4. A bare suspended non-function computation in a block or computation body
   is an execution demand. `&m` is suppression and preserves an existing
   thunk without wrapping it again.
5. Surface `#.bind` and `#.force` are no longer recognized. Computation blocks,
   strict contexts, and callable application generate Core `BIND` and `FORCE`
   nodes.
6. `&{ M }` deliberately suppresses execution. Therefore replacing
   `#.force &{ M }` with `x := &{ M }` is incorrect: the equivalent block form
   is `x := { M }`, or a separate `saved := &{ M }; x := saved` execution
   demand.

The implementation also exposed three pre-existing occurrence/core leaks:

1. The global BIND solver selected arbitrary classifiers for a shared Core
   Lambda. It now derives opportunistically only; the OperationGraph validates
   BIND from the classifiers of its actual child occurrences.
2. BIND result synthesis previously depended on a JudgementDB proof that could
   only be created after the BIND result classifier was known. The kernel BIND
   classifier rule is now shared directly by OperationGraph solving and proof
   materialization.
3. Context-only handler binder facts were reflected into unrelated VAR
   occurrences sharing the same Core node. Generic reflection now requires a
   source Lambda operation identity; synthetic BIND and handler binders use
   their construct-specific OperationGraph propagation.

The implementation invalidated the original deferred-materialization premise.
Execute, suppress, and call are source-context decisions, while the classifier
solver proves that the selected occurrence is well typed. Deferring the Core
choice until classifier solving would make constraint generation depend on a
graph that does not yet exist, requiring a third symbolic graph in addition to
OperationGraph and TermDB. No lowering path repeatedly patches or remaps a
provisional boundary. Consequently, eager one-time Core materialization is not
an architectural debt by itself; classifier borrowing across occurrences is,
and the fixes above address that issue.

### Verification Run

Verified on 2026-07-18:

- examples `01_bool.p` through `07_add.p` and `09_list_induction.p`;
- `test_cbpv_surface.sh`;
- `test_cbpv_boundary.sh`;
- `test_shared_core_occurrences.sh`;
- `test_dependent_pi.sh`;
- `test_artifact_flow.sh`.

The surface suite additionally verifies that `#.bind` and `#.force` are
rejected, that a computed Match scrutinee is sequenced before constructor
resolution, and that direct and nested `&A` binders preserve independent
latent effect rows.

This document supersedes the expression-level quotation policy in
`2026-07-17T03-00-00-COMPUTATION-BLOCK-QUOTATION-PLAN.md`. In particular,
surface `&` is no longer specified as a direct spelling of a raw `THUNK` node.
It is a type-directed request to preserve a computation as a value rather than
execute it in the surrounding execution context.

The raw CBPV TermDB remains unchanged:

```text
RETURN(value)
THUNK(computation)
FORCE(thunk_value)
APP(function_computation, value)
BIND(returning_computation, continuation)
```

The change belongs to surface elaboration and the typed OperationGraph. It does
not add a reduction rule and does not identify `THUNK(FORCE(v))` with `v` in
the kernel.

## Objective

Keep the source language close to C's distinction between using an executable
entity and taking a non-executing reference to it:

```text
x := module;       execute module and bind its result
x := &module;      preserve module as a value without executing it
f argument         call f, opening a suspended callable when necessary
```

Unlike a C address, `&module` does not expose storage or pointer identity. It
elaborates to a CBPV thunk when its operand is a computation. Its purpose is to
make execution suppression explicit while allowing `FORCE` and `BIND` to
remain Core-only operations in ordinary source.

## C Analogy and Its Limit

C collapses a function designator and a function pointer at a call site:

```text
fp(arg) == (*fp)(arg)
```

That implicit dereference is operationally inert. A CBPV `FORCE` is not inert
for an arbitrary `Thunk(Comp(E, A))`: it may start a computation and perform
effects. The surface language may therefore copy C's callable auto-open rule,
but only as one instance of a larger, syntax-directed execution-context rule.

The compiler must never add `FORCE` merely because a thunk value occurs. It
adds `FORCE` only where the source context explicitly demands execution. `&`
selects the non-executing interpretation.

## Surface Contexts

Every expression occurrence is elaborated under one of three demands.

### Value demand

The consumer requires a value and does not execute a suspended computation.

Examples include constructor fields whose expected classifier is already a
thunk type, a function argument whose domain is a thunk type, and an ordinary
value alias.

```text
actual A, expected A
    => pass the value

actual Thunk(C), expected Thunk(C)
    => pass the thunk value
```

### Execution demand

The source position requests that a computation be run. The principal
execution-demand positions are:

1. the right-hand side of a computation-block binding;
2. a computation-block tail;
3. a Lambda body;
4. a Match branch body;
5. a selected program entry point;
6. the callee of an application.

The context selects the operation first, and the classifier solver validates
its admissibility:

```text
M : Comp(E, A)
    => execute M directly

v : Thunk(Comp(E, A))
    => execute FORCE(v)

f : Pi(A, C)
    => call f directly

f : Thunk(Pi(A, C))
    => call FORCE(f)
```

An ordinary value in a computation body is still lifted with `RETURN`.

### Suppressed-execution demand

`&M` requests a value that preserves the selected computation:

```text
M : C, where C is a computation classifier
--------------------------------------------
&M : Thunk(C)
```

If the operand is already a thunk value, `&v` is surface-level execution
suppression and returns the same value occurrence:

```text
v : Thunk(C)
-------------
&v : Thunk(C)
```

This second rule is not a kernel equation and must not construct
`THUNK(RETURN(v))`. It is an elaborator decision saying that an already
suspended value needs no additional boundary.

Applying `&` to an unrelated ordinary value remains an error. There is no
address-of operation in this design.

## Block Semantics

A top-level assignment names a graph and never executes its right-hand side:

```text
module := {
    x := operation;
    x
};
```

If the block has classifier `Comp(E, A)`, then `module` names that computation.
The top-level assignment is a Naming-layer operation, not a CBPV `BIND`.

Inside a computation block:

```text
{
    x := module;
    body
}
```

elaborates as:

```text
BIND(module, LAMBDA(x, body))
```

If `module` is already a thunk value, the same execution-demand position opens
it first:

```text
BIND(FORCE(module), LAMBDA(x, body))
```

To preserve the computation or thunk instead:

```text
{
    saved := &module;
    consumer saved
}
```

`saved` is a value alias and introduces no `BIND` for `module`.

Each bare execution-demand occurrence runs once. Bind once and use the result
more than once to share it. Quotation is not memoization.

## Higher-Order Functions

A surface function argument remains a value-level suspended callable:

```text
high_f := \f : A -> B => body;
main := high_f &some_f;
```

The binder classifier is conceptually:

```text
f : Thunk(Pi(A, Comp(E, B)))
```

An application in `body`:

```text
f x
```

elaborates to:

```text
APP(FORCE(f), x)
```

The auto-open is restricted to the callee occurrence. Passing `f` to another
consumer that expects the same thunk type does not execute it.

For a dependent callable:

```text
f : Thunk(Pi(x : A, Comp(E, B(x))))
v : A
```

the result remains:

```text
APP(FORCE(f), v) : Comp(E, B(v))
```

No special dependent-FORCE rule is required. The existing Pi application
substitution is applied after the callable has been opened.

## Raw CBPV Expressiveness Audit

The proposed surface can encode every existing raw CBPV constructor if all of
the context rules above are implemented.

| Raw Core form | Surface source |
| --- | --- |
| `RETURN(v)` | value `v` in a computation body or block tail |
| `THUNK(M)` | `&M` |
| `FORCE(v)`, returning computation | bare `v` in an execution-demand block binding or tail |
| `FORCE(f)`, function computation | use `f` as an application callee |
| `APP(f, v)` | ordinary application `f v` |
| `BIND(M, LAMBDA(x, N))` | `{ x := M; N }` |
| nested `BIND` | multiple block bindings |
| `OPERATION_REQUEST` | `perform` form, subject to the separate operation-surface plan |
| `HANDLE` | `handle` form, subject to the separate handler-surface plan |

Nested suspension is not lost by making `&` idempotent on an existing thunk
occurrence. A computation that returns a thunk can still be quoted explicitly:

```text
&{
    &module
}
```

This represents a thunk of a returning computation whose returned value is
itself a thunk. It is not collapsed to `&module`.

Therefore no Core CBPV computation is inherently lost by removing ordinary
surface `#.force` and `#.bind`. Both design gates below are now closed.

## Design Gate 1: General Thunk Types

Expression-level `&` is insufficient for full higher-order expressiveness.
Lambda binders require annotations, so source must be able to state the type
of a suspended non-function computation:

```text
consume := \m : <thunked computation returning A> => {
    x := m;
    use x
};
```

Current arrow sugar covers suspended Pi computations but there is no surface
spelling for general `Thunk(Comp(E, A))`.

The implementation selected the first of these policies:

1. introduce a value-type form such as `&A`, elaborating to
   `Thunk(Comp(e, A))` with a latent effect-row variable;
2. expose a more explicit computation-reference type that can include its
   effect row;
3. allow omitted binder annotations and infer this classifier through the
   constraint solver.

`&A` now lowers to `Thunk(Comp(e, A))`. The fresh row `e` is owned and
generalized by the enclosing Lambda occurrence, including every level of a
nested `&&A`. The artifact retains only the explicit Core classifier. An
omitted row is never interpreted as the empty row.

## Design Gate 2: Occurrence Boundary Selection

The original plan assumed that an occurrence's unresolved classifier had to
select among:

```text
A
Thunk(Comp(E, A))
Thunk(Pi(A, C))
```

Implementation showed that this is the wrong ownership boundary. Source syntax
and source context select one of:

```text
EXECUTE(occurrence, context_kind)
SUPPRESS(occurrence)
CALL(occurrence, argument)
```

`SUPPRESS` comes only from `&`; `CALL` comes from the callee position of APP;
and `EXECUTE` comes from strict runtime contexts such as a block binding. These
decisions are deterministic before full classifier synthesis. Lowering creates
the corresponding OperationGraph occurrence and Core boundary exactly once.
`OPERATION_CONSTRAINT_CBPV_BOUNDARY`, `PI_EXPECTED`, and `BIND_RESULT` then
validate and classify that selected occurrence.

This separation is required for shared Core terms with different TypeViews.
Boundary selection belongs to the typed occurrence, never to the canonical
TermDB node. The solver may leave a boundary residual under the configured
compile policy, but it does not search an alternative executing or suppressed
interpretation.

## Coherence Rules

Automatic elaboration remains deterministic only if these rules are enforced:

1. Syntax determines the context kind before classifier solving.
2. The context determines the unique boundary; the solved classifier validates
   it.
3. `&` always suppresses execution; it never means address-of or memoize.
4. A bare thunk is opened only in an execution-demand context.
5. A value-demand context that expects a thunk passes it unchanged.
6. A returning computation supplied where `A` is demanded sequences exactly
   once with `BIND`, preserving left-to-right source order.
7. No boundary insertion participates in normalization equality.
8. Static type normalization never uses runtime execution demand and never
   dispatches effects.

These rules prevent the same source occurrence from being elaborated by
searching both an executing and a non-executing interpretation.

## Surface Removal

The completed surface migration has these properties:

1. remove recognition of `#.bind M K` from ordinary source;
2. remove recognition of `#.force v` from ordinary source;
3. retain `BIND` and `FORCE` as Core TermDB and OperationGraph operations;
4. retain computation blocks as the source introduction of sequencing;
5. retain callable auto-open and execution-demand auto-open as elaboration;
6. do not keep compatibility parsing paths.

Internal debug/readback syntax may still print `BIND` and `FORCE`. That syntax
is not A Program source.

## Implementation Sequence

### Phase 1: Context model (complete)

The implementation uses polarity-aware lowering entry points and
`compile_ast_runtime_value_then` rather than one public context enum. This is
intentional: polarity is structural lowering information, while context demand
is represented by which continuation API is called. Lambda bodies, Match
bodies, block bindings/tails, APP arguments, constructors, and operation
arguments now use those paths.

### Phase 2: Boundary constraints (revised and complete)

1. Execute/suppress/call are represented by occurrence-local OperationGraph
   structure, not by classifier metavariables.
2. Core boundaries are materialized once from source context.
3. Classifier constraints solve and validate those occurrences afterward.
4. Residual reachable boundaries follow the artifact verification-obligation
   policy; strict compilation rejects them.
5. No provisional boundary is remapped into another boundary kind.

### Phase 3: General computation-reference type (complete)

1. Add the selected surface type form for `Thunk(Comp(e, A))`.
2. Allocate a fresh latent effect-row variable when the source omits the row.
3. Generalize or solve that row at the same ownership boundary as function
   latent rows.
4. Add artifact/readback support for the resolved Core classifier only; surface
   sugar does not enter the artifact format.

### Phase 4: Block and application elaboration (complete)

1. Make bare returning computations and returning thunks execute once in block
   bindings.
2. Make `&` suppress both direct computation execution and opening of an
   existing thunk occurrence.
3. Make `Thunk(Pi(...))` callees elaborate through `FORCE` followed by `APP`.
4. Preserve direct raw-Pi application.
5. Preserve left-to-right strict argument sequencing through synthetic BINDs.
6. Keep dependent-BIND residual obligations explicit; do not claim that this
   surface refactoring solves dependent Kleisli typing.

### Phase 5: Remove explicit source intrinsics (complete)

1. Delete `PROTOTYPE_AST_SYSTEM_NAME_BIND` source recognition and its special
   APP-spine lowering.
2. Delete `PROTOTYPE_AST_SYSTEM_NAME_FORCE` source recognition and its special
   APP-spine lowering.
3. Keep Core constructors, evaluator rules, artifact tags, and debug printers.
4. Update prior design documents to point to this plan rather than describing
   `#.bind` or `#.force` as stable source forms.

### Phase 6: Verification (complete for the current surface)

Permanent tests must cover:

1. top-level computation naming performs no execution;
2. a bare computation in a block binding executes once;
3. `&computation` passes a thunk without execution;
4. a bare thunk in a block execution position forces once;
5. `&thunk` passes the existing thunk without adding a wrapper;
6. a thunked callable is forced only at each call occurrence;
7. passing a thunked callable onward does not force it;
8. a computation returning a thunk can be quoted without collapsing nested
   `Comp`/`Thunk` structure;
9. repeated bare occurrences execute repeatedly, while one block binding
   shares one result;
10. effect order remains left-to-right;
11. static type normalization cannot trigger an execution-demand boundary;
12. shared Core nodes may have distinct execute/suppress OperationGraph
   occurrences without leaking one decision into the other;
13. general suspended non-function computations can be accepted as Lambda
   arguments using the new type form;
14. examples 01-09, CBPV boundary tests, surface tests, effect tests, shared-core
   occurrence tests, and artifact flow tests pass.

## Non-Goals

This migration does not by itself complete:

- dependent Kleisli extension;
- effect-row unification;
- operation identity in effect rows;
- multi-clause handlers;
- handler proof completeness;
- memoized or linear thunks;
- concurrency or cancellation semantics.

Those features must use the same execution-context boundary once introduced,
but they remain independent typing and runtime work.

## Decision Summary

The execution-suppression surface is expressively adequate for the existing raw
CBPV Core. It can remove ordinary source `#.bind` and `#.force` without removing
Core `BIND` or `FORCE`.

The migration first added a surface type for general suspended computations,
then made boundary typing occurrence-local, and only then removed the source
intrinsics. Boundary construction remains one-time and syntax-directed; its
classifier is solved afterward. This preserves first-class delayed
non-function computations without making type synthesis choose operational
behavior.
