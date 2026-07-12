# Core, View, and Representation-Shape Boundary

Date: 2026-07-12

Status: design decision; implementation debt remains

This note records four conclusions reached while separating A Program source
semantics from the erased computation graph. They are constraints on the
prototype implementation, not merely presentation conventions.

## 1. Core Evaluation Is Independent of Type Views

`TermDB` is the graph of pure computation. Its operational transitions inspect
only core graph structure and an explicitly selected reduction rule set. In
particular, they must not inspect a source type name, a `TYPE_VIEW` identity,
or a classifier in order to select a different computation.

For a fixed core term and fixed reduction rule set, evaluation must be unique
with respect to source views:

```text
evaluate(core_term, rules) = same core WHNF
```

regardless of the source name or typed binding through which the program
reached `core_term`.

The language remains type-directed at its boundary: a source identifier
resolves to a typed binding, and typing determines whether an application,
constructor selection, or match is permitted. It is not type-directed in the
evaluator: two type views must never cause different beta or iota transitions
for the same core graph.

## 2. Representation Shape Is Not Type Identity

Two declarations may erase to an equivalent finite representation shape:

```text
Bool := @{ true : *; false : *; }
Two  := @{ one  : *; zero  : *; }
```

They may share an erased representation with two constructor slots. This does
not make `Bool` and `Two` judgementally equal.

There is no canonical correspondence between their source constructors. At
least the following bijections are possible:

```text
true -> one,  false -> zero
true -> zero, false -> one
```

The representation shape cannot choose one. A conversion between the two
types must therefore be an explicitly supplied function/equivalence and, if a
later equality system permits it, an explicit transport. It must not arise
from shape comparison.

The same distinction applies to typed lambdas:

```text
identityBool : Bool -> Bool
identityNat  : Nat  -> Nat
```

may share the erased core `LAMBDA(x, x)`, while remaining distinct typed
bindings and not becoming judgementally equal merely because their cores are
the same.

## 3. Type Views Are Source and Typing Addresses

The current `TYPE_VIEW` node carries three pieces of data:

```text
TYPE_VIEW(view_type_id, core, source)
```

`view_type_id` is the nominal source type identity. `source` retains the
declaration-specific application spine for readback and typing. `core` is the
erased representation spine, potentially shared with another declaration.

This is the intended division of responsibility:

```text
RepresentationShape:
  establishes only whether a core representation may be shared.

TypeView:
  preserves the public type identity, source constructor labels, diagnostics,
  artifact export/import identity, and typed access boundary.

TermDB evaluator:
  reduces the core graph without consulting TypeView.
```

Consequently, all of the following must preserve `view_type_id` identity:

- type import and export resolution;
- ordinary classifier conversion and DefEq;
- constructor source-name resolution;
- declaration lookup across namespaces and artifacts.

The existing `TYPE_VIEW` normalization comparison does preserve
`view_type_id`. That is consistent with this decision. The core-shape equality
APIs are only representation evidence and must not be reused as typed
conversion evidence.

## 4. Reduction Rule Sets Are Not Type Views

The prototype currently has named normalization profiles:

```text
LAMBDA_WHNF:
  beta

INDUCTIVE_WHNF:
  beta + match iota + induction expansion

KERNEL_CONVERSION_WHNF:
  beta + match iota + induction expansion + temporary uniform-match rule
```

These are internal descriptions of which core reduction rules are allowed when
observing a weak-head result. They exist because beta-only and iota-enabled
observations can have different results and cannot safely share one cache
entry. They are not a mechanism by which `Bool`, `Two`, or any other
`TypeView` chooses a different evaluator.

The source execution semantics must ultimately select one explicit execution
strategy. Kernel conversion must likewise use one fixed conversion rule set.
Profiles are an implementation partition for evaluation and memoization, not a
new source-level typing feature.

## Required Refactoring

### Rename and narrow `TypeCodeShapeKey`

`TypeCodeShapeKey` reads like an identity key. Its intended role is a
representation-shape fingerprint. Rename it to a name such as
`RepresentationShapeFingerprint` or `CoreRepresentationShapeKey`, and state
at every API boundary that it is not declaration identity, DefEq evidence, or
an import-resolution key.

### Do not canonicalize from a fingerprint alone

`prototype_type_declaration_core_shape_representative(...)` currently selects
the first declaration with an equal `TypeCodeShapeKey`. The key is finite
metadata containing a hash and aggregate counts. Equal keys are not a proof of
structural representation equality: distinct declarations can collide.

Use the fingerprint only as an index or fast rejection. Before sharing a core
representative, perform an exact alpha-invariant comparison of the erased
constructor classifier-family graphs. The comparison must ignore source names
but preserve constructor ordinal, parameter arity, binder structure, recursive
positions, and referenced representation identities.

### Make representation sharing link-order independent

The current representative is the first matching local declaration. This
makes the chosen core type id depend on declaration/link order. The public
`TypeView` identity is protected, but artifact canonicalization should not
silently change a core representative solely because artifacts were linked in a
different order.

Use a stable exact representation key or a deterministic canonical graph
representative after exact comparison. The key may be serialized for artifact
deduplication, but it must never replace the qualified exported type identity.

### Keep shape out of all semantic lookup paths

Search for and remove any path equivalent to:

```text
IMPORTED_TYPE(representation_shape) -> local declaration with same shape
```

An imported type reference must resolve by its qualified exported
`TypeView` identity. Shape data can be carried beside that reference only for
optional validation or representation sharing after resolution.

## Non-Goals

This decision does not introduce equality types, univalence, structural type
equivalence, or automatic transports. It also does not require lowering
`PI` into `APP`/`LAMBDA`/`MATCH`. Those are separate design questions.

It fixes the boundary needed before those features can be considered: shared
core computation is not permission to identify source-level types.
