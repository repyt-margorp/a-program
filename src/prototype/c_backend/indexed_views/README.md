# Actual Acc Declaration Extraction

## Problem List

1. Obtain actual indexed/callable C data declarations from the existing admitted
   Acc QuickSort views, preserving the separate executable hand-authored mockup.

## Subjective (User)

Conveyed human requirement/clarification, 2026-10-04 via inquiry desk/Merge,
English paraphrase: prioritize readable executable C for actual admitted Acc
QuickSort, retaining Acc/down, indices, partition and captures. A faithful labeled
manual candidate is allowed; no substitute sorter, invented erasure or false
generated-success claim. The owning Goal remains active; defer List conveniences.

## Objective (Code)

The requested executable code is [the separate C33 mockup](../acc_quicksort_mockup/mockup.c).
This C34 candidate generates **declarative private C headers only**, directly from
admitted Nat/LT/Acc/SizedList/Measured/Partition/List descriptors imported from
`tests/fixtures/sorted-proof-provider.p`. `view.c` follows exact known factory
lambda/application and pure carrier/Force-Thunk syntax with lexical operands.
It does not run source evaluation, substitution, WHNF, typed queries or a checker.
Target-local bindings and packed descriptor arrays live in a separate arena.

`emit.c` derives constructor counts/fields, separate parameters/indices and
constructor result-image comments. Acc's `U(Pi y:A, Pi edge:R y x, F{}Self y)`
becomes a private synchronous C closure with typed Nat/LT arguments, Acc result,
context and status. The source relation/current/result-index correspondence is
printed explicitly; it is not enforced by C's pointer types. All family/Self/type
tokens, value parameters, indices and proof fields remain represented. Nat uses
its complete zero/succ structure here; this header does not introduce Nat32 or
erase indices. Partition's bound remains a value parameter, not an index.

An ordinary client builds one valid step edge, invokes one supplied down callback,
and accesses the generated indexed/parameter fields without compiler/runtime
libraries. It exercises representation, not a generated source algorithm. A wrong
Nat argument in the LT position refuses compilation. Missing LT and a Measured
Bool/SizedList Nat mismatch refuse extraction instead of becoming opaque proofs.

Run the focused gate with a qualified producer and existing-source helper:

```sh
sh check.sh POINTER BUILD/c_indexed_view_emit OUTPUT
```

`build.mk` builds the private probe/emitter helper at the selected source overlay;
it does not rebuild the pointer compiler. The header is a generated product;
regenerate it, never edit it manually.

## Assessment

This establishes that the existing views contain the concrete representations;
no missing producer field is demonstrated. The main backend/Linker/public ABI
remain unchanged and still refuse the indexed/callable expression boundary.
The C33 sorting body remains hand-authored. Automatic Fold motives/IH callable
adaptation, captured expression bodies, constructor-index calculation/validation,
arbitrary instances/effects/ownership and general source-to-C lowering remain gaps.
No source erasure/admission/checking authority, accepted promotion, cost or full
#61/Goal completion follows from a compilable header.

The bounded emitter supports one selected closed type/family parameter tuple per
nominal declaration, known registered family field heads, up to16 entries/factory
operands/constructors/callable arguments,32 telescope fields,64 symbolic levels
and1024 static-spine steps. Unknown types/signatures/heads refuse; it neither
normalizes arbitrary computations nor infers types from expected results. Output
is a caller-owned stream; this experimental helper provides no transactional
publication promise. All pointed-to nodes and callback context/code are borrowed
and must outlive use. Index consistency and valid callback interpretation remain
source/client obligations; the tiny client is not a proof checker.

## Plan

- [x] Extract actual indexed/callable declaration shapes without producer changes.
- [x] Preserve source constructor-image and callable argument/result associations.
- [x] Terminal strict O2/helper-client SAN, inert/refusal/source-pin verification.
- [x] Prepare exact code/test/docs freeze and separate representation handoff.
- [ ] Lower actual source expression/Fold/capture bodies in a later bounded step;
  no generated native QuickSort completion at this declaration milestone.
