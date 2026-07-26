# Intrinsic Namespace and Effect Operation Registry Split

Date: 2026-07-18
Status: Complete

## Problem

The `#.` prefix is a namespace, not a semantic class. Its names may point to
different kinds of language objects:

- host-backed negative types such as `#.Int` and `#.Text`;
- pure eliminators such as `#.int_add` and `#.text_to_nat`;
- effect operations such as `#.print`;
- eventually, prelude definitions and ordinary graph roots.

The current prototype incorrectly uses `operation_declarations[]` as both the
`#.` namespace name table and the effect-operation registry. Consequently all
of the following become `PROTOTYPE_TERM_OPERATION` nodes:

```text
#.int_add
#.text_to_nat
#.print
```

The distinction is recovered later from an empty or non-empty effect mask.
This permits `perform (#.int_add ...)`, while a direct `#.print` application is
not itself an `OPERATION_REQUEST`. Namespace membership is therefore deciding
semantics indirectly, and pure eliminators and handleable requests do not have
separate invariants.

## Target Model

`#.` resolution is a namespace lookup which returns a typed binding descriptor.
The binding kind determines the target representation:

```text
#.Int       -> HOST_TYPE          -> PRIMITIVE(Int)
#.int_add   -> PURE_PRIMITIVE -> PURE_PRIMITIVE(int_add)
#.print     -> EFFECT_OPERATION   -> EFFECT_OPERATION(print)
```

The registries have distinct authority:

1. The intrinsic namespace registry owns source spelling and binding kind.
2. The pure-primitive registry owns pure host-backed primitive signatures.
3. The effect-operation registry owns effect signatures and operation identity.
4. The runtime implementation registry maps either function or operation ids to
   host code, but it does not define source names or typing semantics.

The `#.` namespace registry may later point to prelude graph exports without
turning those exports into intrinsics or effect operations.

## Semantic Invariants

### Pure primitives

- They are ordinary function heads used by `APP`.
- A saturated application has `Comp({}, A)` in the current CBPV model.
- Execution may reduce it under the pure-intrinsic reduction flag.
- Kernel conversion does not execute host intrinsics.
- They cannot be the operation argument of `perform`.
- Handler clauses cannot intercept them.

### Effect operations

- They have an explicit effect row and operation signature.
- `perform` forms `OPERATION_REQUEST` only from a saturated effect operation.
- Deep handlers match only effect-operation identities.
- Default host dispatch receives only effect-operation requests.
- Direct operation application remains an intermediate computation until the
  surface elaboration policy is finalized; it is not treated as a pure
  intrinsic merely because it has a host implementation.

## Implementation Plan

### Phase 1: Declaration and namespace split

- [x] Add an intrinsic namespace binding descriptor and lookup API.
- [x] Add `prototype_pure_primitive_id` and its declaration table.
- [x] Restrict `prototype_effect_operation_id` and the effect-operation table to
      actual algebraic operations (`print` initially).
- [x] Remove source-name lookup responsibility from both semantic registries.

### Phase 2: AST and TermDB representation

- [x] Split AST system-name kinds into host type, pure primitive, and effect
      operation.
- [x] Add distinct `PURE_PRIMITIVE` and `EFFECT_OPERATION` TermDB tags.
- [x] Lower namespace bindings to the corresponding TermDB node.
- [x] Preserve separate canonical identity, debug output, traversal, cloning,
      and artifact validation for both tags.

### Phase 3: Typing and execution

- [x] Type pure primitives from the pure-primitive signature registry.
- [x] Type effect operations from the effect-operation signature registry.
- [x] Restrict pure host reduction to `PURE_PRIMITIVE` applications.
- [x] Restrict `OPERATION_REQUEST`, handler clauses, and host effect dispatch to
      `EFFECT_OPERATION` nodes.
- [x] Reject `perform` applied to a pure primitive.

### Phase 4: Artifact boundary

- [x] Serialize and read back pure-primitive and effect-operation nodes with
      distinct tags.
- [x] Resolve artifact source spellings through the intrinsic namespace registry
      and verify that the stored tag agrees with the binding kind.
- [x] Remove the old ambiguous operation-node artifact path; no compatibility
      shim will be retained.

### Phase 5: Verification

- [x] Verify direct integer arithmetic still reduces and has an empty effect row.
- [x] Verify `perform (#.int_add ...)` is rejected.
- [x] Verify `perform (#.print ...)` forms an operation request and is handled.
- [x] Verify handlers cannot name `#.int_add` as an operation clause.
- [x] Verify intrinsic/effect nodes survive artifact write/read/link boundaries.
- [x] Run the CBPV surface, CBPV boundary, artifact flow, shared-core, dependent
      Pi, and existing example suites.

## Compatibility Policy

This is a prototype representation correction. Existing artifacts containing
the ambiguous operation tag are not supported after the change. Source syntax
for valid uses remains stable, except that applying `perform` or a handler clause
to a pure intrinsic becomes a compile error.

## Progress Log

- 2026-07-18: Confirmed that `operation_declarations[]` currently combines
  namespace lookup, pure intrinsic signatures, effect-operation signatures, and
  host implementation dispatch. Confirmed that explicit `perform` accepts
  `#.int_add` and that the artifact format records both categories with the same
  term tag.
- 2026-07-18: Added separate namespace, pure-primitive, effect-operation,
  and host implementation registries. The namespace owns source spellings;
  semantic registries own signatures; TermDB nodes own only semantic target IDs.
- 2026-07-18: Split AST kinds, TermDB tags, typing proofs, pure reduction, effect
  dispatch, handler validation, and artifact encoding. Artifact version 48 rejects
  the former ambiguous operation representation and rejects binding-kind/tag
  mismatches during readback.
- 2026-07-18: Verified direct integer reduction, `print` request handling,
  rejection of intrinsic `perform` and handler clauses, artifact round trips and
  corrupted-kind rejection, CBPV boundary/surface tests, shared-core tests,
  dependent Pi tests, examples 01-09 present in the repository, and
  `training/double.p`.
- 2026-07-18: Follow-up design review removed the overly strong
  `INTRINSIC_FUNCTION` category. The final representation is
  `PURE_PRIMITIVE(id)`; see
  `2026-07-18T23-23-27-PURE-PRIMITIVE-NODE-MIGRATION.md`.
