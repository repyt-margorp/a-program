# Pure Primitive Node Migration

Date: 2026-07-18
Status: Complete

## Problem

The `#.` intrinsic namespace resolves names to different semantic objects. The
namespace itself does not establish an `intrinsic function` language category.
The current prototype nevertheless lowers pure host-backed primitives such as
`#.int_add` to `INTRINSIC_FUNCTION(id)` and names their declarations, proofs,
and evaluator paths after that category.

This is too strong. `#.int_add` only needs to be a distinct pure primitive node
with a declared type and a pure reduction rule:

```text
#.int_add -> PURE_PRIMITIVE(IntAdd)
#.print   -> EFFECT_OPERATION(Print)
```

In the current CBPV classifier representation:

```text
PurePrimitive(IntAdd)
  : Pi(Int, Pi(Int, Comp({}, Int)))
```

The empty computation effect is a property of the primitive's result, not a
reason to create an `intrinsic function` category.

## Target Boundaries

1. `intrinsic_namespace` owns `#.` source spellings and resolves each spelling
   to a semantic binding kind and target id.
2. `PURE_PRIMITIVE(id)` represents a pure primitive head with a declaration,
   classifier, and optional host reduction implementation.
3. `EFFECT_OPERATION(id)` represents an algebraic effect operation that may
   form an operation request and be intercepted by a handler.
4. Pure primitive nodes cannot be passed to `perform` or used as handler
   operation clauses.
5. TermDB identity contains semantic ids, not source spellings.

`PURE_PRIMITIVE` is deliberately not restricted to function vocabulary. The
initial declarations happen to have Pi classifiers, but the node category means
only "a primitive with a pure kernel/runtime reduction contract".

## Migration Plan

### Phase 1: Vocabulary and declarations

- [x] Replace `prototype_intrinsic_function_id` with
      `prototype_pure_primitive_id`.
- [x] Replace intrinsic-function declaration and implementation registries with
      pure-primitive registries.
- [x] Change the intrinsic namespace binding kind from `FUNCTION` to
      `PURE_PRIMITIVE`.

### Phase 2: AST, TermDB, and typing

- [x] Replace the AST system-name kind with `PURE_PRIMITIVE`.
- [x] Replace `PROTOTYPE_TERM_INTRINSIC_FUNCTION` with
      `PROTOTYPE_TERM_PURE_PRIMITIVE`.
- [x] Rename TermDB fields and constructors around `primitive_id`.
- [x] Replace classifier synthesis and proof kinds with pure-primitive names.
- [x] Keep the classifier rule equivalent: integer primitives end in
      `Comp({}, result_type)`.

### Phase 3: Reduction and effects

- [x] Restrict pure host reduction to `PURE_PRIMITIVE` application heads.
- [x] Keep operation requests, handlers, runtime capability detection, and
      effect dispatch restricted to `EFFECT_OPERATION`.
- [x] Verify no semantic branch infers purity from namespace membership.

### Phase 4: Artifact boundary

- [x] Serialize `PURE_PRIMITIVE` and `EFFECT_OPERATION` with different tags.
- [x] Resolve artifact spellings through `intrinsic_namespace` and reject a tag
      whose binding kind disagrees with the namespace entry.
- [x] Bump the artifact version and retain no compatibility path for the former
      `INTRINSIC_FUNCTION` representation.

### Phase 5: Verification

- [x] Verify direct integer primitive application and pure reduction.
- [x] Verify pure primitive proof readback.
- [x] Verify `perform` and handler clauses reject pure primitives.
- [x] Verify `print` remains an effect operation and can be handled/dispatched.
- [x] Verify corrupted artifact kind substitutions are rejected.
- [x] Run CBPV surface, CBPV boundary, artifact flow, shared-core, dependent Pi,
      existing examples 01-09 present in the repository, and training double.

## Compatibility Policy

This is a prototype representation correction. No source compatibility shim,
enum alias, old artifact reader, or old proof-kind alias will be retained.

## Progress Log

- 2026-07-18: Audited all `INTRINSIC_FUNCTION` references. Confirmed that the
  category crosses namespace resolution, AST lowering, TermDB identity,
  classifier synthesis, proof validation, pure reduction, debug output,
  artifacts, and tests. Selected `PURE_PRIMITIVE` as the replacement semantic
  node category while retaining `intrinsic_namespace` as the name of `#.`.
- 2026-07-18: Replaced the category end to end with `PURE_PRIMITIVE`, including
  ids, declarations, namespace binding kind, AST kind, TermDB fields, canonical
  identity and hashing, classifier synthesis, proof validation, evaluator
  dispatch, debug output, and tests. No compatibility aliases were retained.
- 2026-07-18: Kept effect semantics separate. `OPERATION_REQUEST`, handler
  clauses, effect dispatch, and runtime capability detection still require
  `EFFECT_OPERATION`; pure primitives are reduced only by the pure primitive
  evaluator path.
- 2026-07-18: Bumped the artifact format to version 49. Readback resolves source
  spellings through `intrinsic_namespace` and rejects both pure-primitive nodes
  encoded as effect operations and effect operations encoded as pure primitives.
- 2026-07-18: Verified `PURE_PRIMITIVE(int64_add)` synthesizes
  `Pi(Int64, Pi(Int64, Comp({}, Int64)))`, evaluates to the expected integer,
  and carries `pure-primitive-type-intro`. All planned regression suites pass.
