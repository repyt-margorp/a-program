# Namespace and Link-Layer References

Date: 2026-07-12

Status: implemented prototype direction

This note records the intended role of namespaces after the type-view and core
graph split.

## Problem

The core computation graph should not depend on source names. A lambda term,
application spine, match expression, constructor owner, or constructor ordinal
should keep its computational identity even if the source program exposes that
graph through different names.

For example, two source names may point to the same core graph:

```text
List.map  -> typed view A -> core node #123
Graph.map -> typed view B -> core node #123
```

This is the same layering principle used for typed lambdas:

```text
identityBool -> typed view Bool -> Bool -> core \x => x
identityNat  -> typed view Nat  -> Nat  -> core \x => x
```

However, unresolved external references are not yet core graph nodes. They are
linker obligations. If an object artifact records only the bare name `map`, the
linker cannot distinguish `List.map` from `Graph.map`. The current prototype has
this tension: artifact exports and dependencies carry namespace information,
while `EXTERNAL_REF` stores only a bare symbol id.

## Target Layering

Namespaces are not part of the core calculus. They belong to the name and link
layers.

```text
Core graph layer:
  APP / LAMBDA / MATCH / PI / CONSTRUCTOR(owner, ordinal)
  No source namespace is required here.

Typed/view layer:
  A source name points to a typed view over a core graph node.
  Multiple views may point to the same core node.

Name/link layer:
  Unresolved references are qualified link identities.
  They are resolved to typed views or graph nodes before ordinary computation.
```

Binders and namespaces should not be confused. A binder is local structure
inside a term. It can be represented by a pointer, frame id, or de Bruijn-like
mechanism, and the source spelling can disappear after lowering. A namespace is
an external address used to find a declaration across a compilation artifact,
library, or system-provided intrinsic set.

## Constructor Names

Constructor labels remain view-level data.

The computational constructor form is:

```text
CONSTRUCTOR(owner_type_graph, ordinal)
```

If `Bool` and `Two` share the same finite two-constructor core shape, then
`Bool.true` and `Two.one` may point to the same core constructor ordinal under
different views. The source view must still preserve the labels for typing,
readback, diagnostics, exports, and later explicit transport or relabeling.

The linker does not need constructor labels as computational identity once the
owner type graph is known. It needs labels only while resolving a source-level
member such as `Bool.true`.

## External References

Resolved external references should carry a qualified identity until they are
linked. A genuinely unresolved bare source name remains an explicitly
unqualified linker requirement; it must not be silently treated as a member of
the current namespace.

There are two acceptable implementation shapes:

```text
structured:
  EXTERNAL_REF(namespace_symbol_id, name_symbol_id)
  where namespace_symbol_id may be -1 only for an unresolved bare requirement

flattened:
  EXTERNAL_REF(qualified_symbol_id)
  where qualified_symbol_id interns the whole name, such as "Prelude.List.map"
```

Both are theoretically equivalent if applied consistently. The structured form
matches the existing artifact export and dependency data more directly. The
flattened form is simpler if the compiler treats qualified source names as
ordinary interned labels. In either case, `EXTERNAL_REF(map)` is too weak for
multi-artifact linking.

## Implementation Direction

The prototype should choose one qualified-reference representation and apply it
at every unresolved-name boundary.

Required changes:

1. Make `PROTOTYPE_TERM_EXTERNAL_REF` carry a qualified identity.
2. Make dependency collection preserve that same qualified identity.
3. Make imported transparent definition lookup key by qualified identity, not
   only by bare name.
4. Keep source namespace resolution outside the core graph after the reference
   has been resolved.
5. Keep constructor computational identity as `CONSTRUCTOR(owner, ordinal)`;
   only source member resolution needs constructor labels.

The prototype uses the structured form. It also follows these resolution
rules:

```text
local forward declaration:  name -> EXTERNAL_REF(current_namespace, name)
unique imported export:     name -> EXTERNAL_REF(provider_namespace, name)
unknown bare source name:   name -> EXTERNAL_REF(-, name)
```

The default current namespace is derived from the input source file, never from
the output artifact path. A multi-file compilation may explicitly pass one
`--namespace` value to place all of those files in the same namespace. This
keeps an artifact filename from changing the API identity of a module.

Link search must compare both namespace and name whenever a namespace is
present. A bare requirement is intentionally weaker and must eventually be
resolved by an explicit provider choice or rejected as ambiguous; it must not
select an arbitrary artifact merely because it exports the same bare name.

An imported type declaration is not the only unresolved reference that may
occur in a type expression. A transparent external term may itself compute to a
type, for example:

```text
NatAlias := Nat
idAlias := \x : NatAlias => x
```

Therefore `PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE` cannot be the only external
reference representation in the type-expression layer. It describes an
imported type declaration, while `NatAlias` is an imported term reference.
The implementation must introduce a qualified external type-expression term
reference, or lower such names directly to an `EXTERNAL_REF(namespace, name)`
graph term before artifact dependency collection. It must not fall back to an
unqualified `TYPE_EXPR_NAME` merely because the referenced export is a term.

The important invariant is:

```text
Before linking:
  locally declared and imported references are qualified names.
  unknown bare references are explicit unresolved linker requirements.

After linking:
  references point to typed views or core graph nodes.

During computation:
  source namespaces do not affect reduction or kernel equality.
```

This lets namespaces remain a practical artifact and library mechanism without
turning them into computational structure.
