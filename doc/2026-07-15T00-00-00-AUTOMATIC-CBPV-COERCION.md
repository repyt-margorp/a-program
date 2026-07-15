# Automatic CBPV Coercion

## Status

Implemented in the prototype on 2026-07-15.

## Implementation Plan

1. Preserve source polarity on an operation occurrence rather than inferring it
   from a shared erased TermDB node. Complete.
2. Make the canonical dCBPV boundary insertions explicit lowering operations.
   Complete.
3. Thread one compiler policy from the public program configuration through
   graph compilation. Its default must preserve the existing surface language.
   Complete.
4. Provide a strict mode that rejects an omitted boundary, and test both modes.
   Complete.

## Purpose

A Program keeps one source expression grammar while its raw dCBPV core has
separate value and computation terms. The compiler therefore elaborates a
source occurrence at a value/computation boundary. This is not a conversion
between arbitrary user types and it does not change DefEq, normalization, or
the typing kernel.

The only automatic graph completions are the canonical dCBPV boundaries:

| Actual operation node | Required context | Inserted core node |
| --- | --- | --- |
| value | computation | `RETURN(value)` |
| `THUNK(computation)` value | computation | `FORCE(value)` |
| computation | value | `THUNK(computation)` |
| computation whose returned value is required by a source continuation | value continuation | `BIND(computation, ...)` |

The source operation graph stores the actual polarity. A name resolves through
its defining operation occurrence, not merely to an erased TermDB node. For
example, a raw lambda and a thunked use of that lambda may share a core lambda
while remaining distinct operation nodes.

## Compiler Option

Automatic coercions are enabled by default.

```
./read_file.out file.p
./read_file.out --automatic-cbpv-coercions file.p
```

Use strict raw dCBPV checking to disable all implicit boundaries:

```
./read_file.out --no-automatic-cbpv-coercions file.p
```

The same options are accepted by `a.out`. Embedders set
`program.compile_options.disable_automatic_cbpv_coercions` before calling
`prototype_compile_graph`.

With the default option, this surface program elaborates its body as
`RETURN(x)`:

```
id := \x : #.Int => x;
```

With automatic coercions disabled, its raw equivalent is required:

```
id := \x : #.Int => return x;
```

Explicit thunk/force remains available in strict mode. A higher-order raw
function receives a thunked function value and opens it before application:

```
apply := \f : Nat -> Nat => (force f) Nat.zero;
main := apply (thunk id);
```

## Deliberate Boundary

The option controls lowering only. It does not:

- make proof equality into DefEq;
- rewrite arbitrary value classifiers;
- execute intrinsics during type checking;
- alter a normalization profile; or
- identify distinct operation/type views that share an erased core term.

Classifier constraints are still generated and solved after graph lowering.
The current implementation uses known operation classifiers and a provisional
function candidate for some unannotated nested applications. Replacing that
candidate with a dedicated polarity-constraint pass remains future work; it
is independent of the automatic coercion policy implemented here.
