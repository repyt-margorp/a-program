# Core, View, and Representation-Shape Boundary

Date: 2026-07-12

Status: implemented prototype boundary

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

## Implemented Refactoring

### Dedicated Representation Handles

`TYPE_FORMER` now stores an interned `representation_id`. A separate
`TYPE_DECLARATION(type_id)` graph node is used only in the source spine of a
`TYPE_VIEW`. A declaration id is therefore no longer used as both source
identity and core representation handle.

`prototype_type_declaration_db` owns the representation interning table. Its
numeric handles are implementation-local and never participate in source type
identity, qualified import lookup, or typed conversion.

### Exact Sharing Check

`prototype_type_declaration_rebuild_representations(...)` uses the existing
`TypeCodeShapeKey` only as a fingerprint for fast rejection. Candidate sharing
then requires an exact alpha-invariant comparison of parameters, constructor
ordinals, classifier-family graphs, binders, recursive declaration pairs, and
referenced type structure. A fingerprint match alone no longer shares a core
representation.

The old `prototype_type_declaration_find_by_code_shape_key(...)` lookup path
has been removed. Imported type references resolve through qualified exported
`TypeView` identity, not through a representation fingerprint.

### Finalization and Artifact Relocation

While recursive declarations are being assembled, their core type formers use
provisional declaration anchors. After graph construction, exact interning
rewrites those anchors to representation handles. Constructor and match owners
retain source views during typing/proof validation, then a final graph pass
erases them to core representation owners before publication.

Artifact format 27 serializes a type former through its representative
declaration anchor rather than an artifact-local representation id. Readback
rebuilds the intern table and rebinds the anchors. Artifact append/link remaps
both target and provider handles into the target table. Numeric allocation can
therefore vary with link order without changing any source or kernel equality
decision.

### Regression Coverage

`core_view_representation_check.c` verifies that same-shape `Bool` and `Two`
views share a core representation but are not normalization-equal, that their
function classifiers remain distinct, and that beta reduction and a fixed WHNF
profile operate on the shared core independently of the view.

`test_artifact_flow.sh` verifies that a `Bool` dependency remains unresolved
when only same-shape `Two` is linked, and resolves only when the qualified
`Bool` provider is linked.

## Remaining Naming Cleanup

`TypeCodeShapeKey` remains as a compatibility-oriented C identifier, but now
means only a representation fingerprint. Artifact/readback output calls it a
`representation_fingerprint`. A mechanical C identifier rename is separate
from the implemented semantic boundary.

## Non-Goals

This decision does not introduce equality types, univalence, structural type
equivalence, or automatic transports. It also does not require lowering
`PI` into `APP`/`LAMBDA`/`MATCH`. Those are separate design questions.

It fixes the boundary needed before those features can be considered: shared
core computation is not permission to identify source-level types.
