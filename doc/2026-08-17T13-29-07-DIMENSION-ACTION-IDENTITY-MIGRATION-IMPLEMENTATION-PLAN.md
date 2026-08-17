# Dimension Action Identity Migration Implementation Plan

Date: 2026-08-17

Status: active; DIA0 complete, DIA1 implemented and under gate verification

Repository baseline:

- branch: `main`;
- implementation-start commit: `7f76d82f97605b3abb75e09b99dc8418df4f7da3`;
- implementation root: `src/prototype/`;
- artifact format: `v77`; and
- generated identity references at baseline: 133 occurrences across 20
  prototype files.

Related documents:

- `2026-08-17T13-19-08-NARYA-DIMENSION-ACTION-AND-GENERATED-IDENTITY-AUDIT.md`;
- `2026-08-09T19-46-59-A1-T0-RELATION-TO-HIGHER-OBSERVATIONAL-IDENTITY-AUDIT.md`;
- `2026-08-09T13-11-17-V2-A1-OBJECT-HOTT-ARTIFACT-IMPLEMENTATION-PLAN.md`;
- `2026-08-16T14-43-28-SEMANTIC-STORE-AUTHORITY-AND-PHYSICAL-CONSOLIDATION-PLAN.md`;
- `2026-08-16T20-42-43-POST-DA-SEMANTIC-AUTHORITY-RESIDUAL-CONSOLIDATION-PLAN.md`; and
- `src/prototype/spec/artifact_v77.schema`.

## 1. Objective

Replace the current representation:

```text
source ADT
  -> generated two-index identity TypeDeclaration
  -> generated eight-index square TypeDeclaration
  -> further generated declarations for further dimensions
```

with one dimension-generic action over the existing Core Term graph and source
type schema:

```text
dimension_action(source_term, operator)
  -> acted Term

acted Term applied to boundary Terms through ordinary APP
  -> instantiated identity family or witness
```

The migration must remove generated identity declarations from semantic type
authority and artifact publication. It must preserve:

- source and imported ADT/IADT declarations;
- ordinary Core Terms, Claims, and Derivations as proof evidence;
- stable `BindingId` identity;
- concrete Context and Substitution evidence where dependent checking needs it;
- the distinction between compiler-local parametricity relations and object
  identity; and
- the current admitted HOTT fragment unless a phase explicitly strengthens it.

The migration is complete only when
`PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY`,
`generated_type_declaration_id`, the fixed eight-index generator, and artifact
support for generated declarations have been deleted.

## 2. Non-goals

This plan does not:

- collapse source IADTs into HOTT identity machinery;
- implement all of Higher Observational Type Theory;
- claim full abstraction or completeness for contextual equivalence;
- replace `BindingId` with De Bruijn indices;
- eliminate `ContextDB`, `SubstitutionDB`, `JudgementDB`, or
  `TypeDeclarationDB`;
- merge parametricity relation evidence with object identity evidence;
- add surface `Eq`, `refl`, `transport`, or rewrite syntax;
- make object identity proofs into DefEq rules;
- preserve artifact `v77` compatibility; or
- retain the old generated-declaration path as a fallback after cutover.

## 3. Normative Decisions

### 3.1 TypeDeclarationDB becomes source/import authority only

After this migration, every `prototype_type_declaration` denotes one
source-defined or imported declaration. Identity computation cannot allocate a
type declaration.

The following invariant is mandatory:

```text
type_count before identity action == type_count after identity action
```

This must hold for one-dimensional identity, squares, repeated action, object
term action, artifact publication, and readback replay.

### 3.2 Add one generic Core Term action, not one tag per dimension

Add a single Term form conceptually equivalent to:

```c
DIMENSION_ACTION {
	uint32_t source;
	uint32_t operator_id;
}
```

The final spelling is fixed during DIA0, but the semantic constraints are:

- it is one tag for types, constructors, functions, and other Terms;
- it is not duplicated into value-side and computation-side variants;
- its category and classifier are derived from the source Claim and operator;
- repeated action nests or composes this same representation;
- Term interning compares the semantic operator, not an action-work ID; and
- it is serializable because it is part of object evidence, not merely solver
  state.

This is the justified new Core distinction. Encoding the action only through a
generated type declaration hides the operation. Adding separate
`IDENTITY_TYPE_1`, `IDENTITY_TYPE_2`, and similar tags recreates the present
problem.

### 3.3 Keep boundary application in ordinary APP spines

No permanent `BoundaryDB` is introduced in the first migration.

The acted Term synthesizes a dependent classifier whose arguments are the
boundary faces required by its operator. Explicit boundary Terms are applied
using ordinary `APP` nodes in the canonical traversal order defined by the
dimension module.

Conceptually:

```text
Id A x0 x1
  = APP(APP(DIMENSION_ACTION(A, e), x0), x1)

Id (Id A) boundary...
  = APP(...APP(DIMENSION_ACTION(DIMENSION_ACTION(A, e), e), face0)..., faceN)
```

The exact higher boundary is not reduced to four informal edge names. The
dimension module defines the complete typed face traversal. APP is only the
existing graph application mechanism.

If partial higher instantiation later proves impossible to validate from an APP
spine, a separate instantiation Term may be proposed in a new audited plan. It
must not be added speculatively during this migration.

### 3.4 Add one dimension module with no fixed maximum dimension

Create a prototype module under:

```text
src/prototype/include/a_program/dimension/
src/prototype/src/dimension/
```

It owns:

- immutable dimension operators;
- operator identity, hashing, validation, and composition;
- face keys whose digits distinguish endpoint 0, endpoint 1, and varying
  dimensions;
- deterministic face traversal;
- boundary cardinality checks with overflow detection; and
- mapping a face through an operator.

The representation must use checked variable-length storage. It must not encode
the implementation limit in arrays such as `[8]`, `[9]`, or `[576]`.

Resource limits may reject a request deterministically. Such limits are runtime
policy, not the mathematical dimension representation.

### 3.5 Do not add a permanent acted-schema authority store

An acted ADT needs constructor and telescope information during checking, but
it is derivable from:

```text
source TypeDeclaration
source constructor ordinal
dimension operator
acted Context prefix
```

Implement a read-only schema query/view that can resolve either:

- an ordinary source `TYPE_VIEW`; or
- a `DIMENSION_ACTION` whose source ultimately resolves to a source
  declaration.

The view may be caller-owned or certificate-owned temporary data. It must not:

- receive a nominal `TypeDeclarationId`;
- be added to `TypeDeclarationDB`;
- become a second mutable constructor database; or
- be serialized as a type declaration.

Cached acted classifier Terms and Context paths remain rebuildable cache data.

### 3.6 Concrete acted Contexts remain allowed

Dependent checking can materialize a Context containing endpoint values,
identity faces, and higher fillers. This is not prohibited.

The Context action must be keyed semantically by:

```text
source Context
dimension operator
object-identity semantics
```

and produce:

- a concrete Context view;
- face-to-`BindingId` lookup;
- endpoint/face substitutions;
- CwF and ordinary typing certificates; and
- the action certificate connecting it to the source Context.

The view is disposable. Deleting the action/cache databases and replaying the
same request must reconstruct equivalent Terms, Claims, and substitutions
without changing source declaration identity.

### 3.7 Object proof evidence remains ordinary and replayable

`DIMENSION_ACTION` is not accepted merely because the compiler produced it.
Judgement replay must validate explicit rules for:

- formation of an acted type;
- action on a typed term;
- action on a constructor telescope;
- application to boundary Terms; and
- the existing Lambda, APP, Match, IH, Return, and Thunk action cases.

The exact proof-kind split is fixed in DIA0. Do not hide all type-former cases
behind one unchecked action certificate. Shared dimension plumbing may be
common, while distinct kernel theorems retain distinct validators.

### 3.8 Artifact migration is intentionally breaking

Artifact `v77` states that generated declarations and their Context telescopes
are wire authority. It cannot be reinterpreted as the new model.

The implementation must create artifact `v78` unless another artifact version
has already been committed before this plan starts. The chosen version must:

- archive the preceding schema;
- serialize `DIMENSION_ACTION` Terms and semantic operators;
- serialize ordinary rooted boundary Terms, Claims, Derivations, and Contexts;
- serialize source/import type declarations only;
- omit scratch bridge/action cache identities;
- replay acted schema queries from source declarations; and
- reject old artifacts at the version boundary.

No compatibility reader or generated-declaration fallback is retained.

## 4. Target Data Flow

### 4.1 Source declaration

```text
surface ADT/IADT
  -> TypeDeclarationDB source schema
  -> source TYPE_VIEW / TYPE_FORMER graph
```

This path remains unchanged except that `origin_kind` and
`origin_source_carrier_term_id` disappear after cutover.

### 4.2 Identity family computation

```text
source type Claim
dimension operator
source/acted Context view
  -> DIMENSION_ACTION(source type Term, operator)
  -> dependent classifier over required boundary faces
  -> ordinary formation Claim and Derivation
```

No TypeDeclaration is allocated.

### 4.3 Constructor action

```text
source constructor schema
acted parameter/index Context prefix
dimension operator
  -> act each field classifier under the already acted prefix
  -> acted constructor classifier
  -> DIMENSION_ACTION(source constructor Term, operator)
  -> ordinary witness Claim
```

Dependent fields are handled incrementally. A field mentioning an earlier
field is not a reason to switch algorithms or return residual.

### 4.4 Repeated action

```text
DIMENSION_ACTION(A, d1)
  acted again by d2
  -> DIMENSION_ACTION(DIMENSION_ACTION(A, d1), d2)
```

Normalization may compose operators when the dimension laws prove the
composition valid. Structural construction must work before this optimization;
operator composition is not allowed to erase a semantically relevant boundary.

### 4.5 Artifact publication

```text
identity root
  -> source type Claim
  -> acted family Term/Claim
  -> dimension operator
  -> rooted boundary and witness proof DAG
```

Publication follows semantic Term children and proof premises. It does not mark
generated declarations because none exist.

## 5. Implementation Phases

### DIA0. Freeze the dimension calculus and wire-independent invariants

Status: complete

- [x] Add `src/prototype/spec/dimension_action_v1.schema` defining operator,
  face, traversal, composition, and malformed-input rules.
- [x] Specify the binary HOTT direction used by the first implementation.
- [x] Specify how a dimension operator acts on an already acted Term.
- [x] Specify the canonical boundary APP order without placing that order in
  every caller.
- [x] Specify classifier formation for `DIMENSION_ACTION` on Universe, source
  ADT/IADT, Pi, Return/Thunk, constructor, Lambda, APP, Match, and IH.
- [x] Classify every case as executable, residual, unsupported, or outside this
  migration.
- [x] Define action identity and composition laws used by normalization.
- [x] Define the exact Judgement proof kinds and their ordered premises.
- [x] Define how resource usage and effects are preserved or made residual.
- [x] State that no action rule implies equality reflection.
- [x] Record baseline build, focused HOTT, artifact, and full-suite runtimes.
- [x] Record baseline line counts for every file in the final migration map.

Exit criteria:

- no C structure is added before the dimension/face laws are reviewable;
- dimensions 1, 2, and 3 can be written using the same specification;
- malformed boundaries and composition failures have deterministic outcomes;
- the specification does not mention generated TypeDeclaration identity.

### DIA1. Implement dimension operators and generic face traversal

Status: blocked on DIA0

- [ ] Add checked immutable operator storage and interning.
- [ ] Add variable-length face-key storage or caller-owned face values.
- [ ] Add deterministic traversal of all required boundary faces.
- [ ] Add checked cardinality/overflow helpers.
- [ ] Add face restriction and operator composition.
- [ ] Add transaction marks or rollback compatible with existing prototype
  arena conventions.
- [ ] Add focused unit tests for dimensions 0 through 4.
- [ ] Add malformed operator, duplicate-axis, invalid-face, and overflow tests.
- [ ] Verify that no consumer manually counts square faces.

Expected files:

- `src/prototype/include/a_program/dimension/types.h`;
- `src/prototype/include/a_program/dimension/operator.h`;
- `src/prototype/include/a_program/dimension/face.h`;
- `src/prototype/src/dimension/operator.c`;
- `src/prototype/src/dimension/face.c`;
- `src/prototype/build/sources.mk`; and
- focused checks under `src/prototype/tests/checks/dimension/`.

Exit criteria:

- the same traversal API produces 1D, 2D, and 3D boundaries;
- no fixed face array appears outside focused test fixtures;
- operator interning is independent of request order.

### DIA2. Add the generic DIMENSION_ACTION Core Term

Status: blocked on DIA1

- [ ] Add one Core Term tag and payload containing source Term and semantic
  operator identity.
- [ ] Add child roles for source traversal.
- [ ] Add formation, interning, hashing, comparison, and debug readback.
- [ ] Add clone, canonicalization, substitution, reindex, alpha-interning, and
  free-binding traversal.
- [ ] Add normalization rules only for laws specified in DIA0.
- [ ] Preserve one common Term representation across value, computation, and
  type classifications.
- [ ] Add malformed operator-reference validation.
- [ ] Add round-trip tests in TermDB before adding HOTT semantics.

Expected files include:

- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/src/core/term/storage_and_formation.inc`;
- `src/prototype/src/core/term/canonicalization.inc`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc`;
- `src/prototype/src/core/term/declarations.inc`;
- Term graph traversal/relocation helpers; and
- `src/prototype/tests/checks/core/` fixtures.

Exit criteria:

- nested action Terms intern deterministically;
- reindexing changes source bindings but not operator identity;
- conversion does not equate unrelated action Terms;
- no generated declaration is needed to construct the Term.

### DIA3. Introduce source-or-acted schema queries without a new authority DB

Status: blocked on DIA2

- [ ] Define a read-only `prototype_type_schema_view` or equivalently narrow
  query result.
- [ ] Resolve source `TYPE_VIEW` Terms through `TypeDeclarationDB` as today.
- [ ] Resolve `DIMENSION_ACTION` Terms by recursively resolving their source
  schema and acting on its parameter/index/constructor telescopes.
- [ ] Keep source declaration and constructor IDs in every acted view.
- [ ] Derive acted constructor classifiers without allocating declarations.
- [ ] Derive curried classifiers through existing Pi/telescope construction.
- [ ] Ensure caches are rebuildable and are not part of schema identity.
- [ ] Add an assertion/test that schema queries cannot mutate
  `TypeDeclarationDB`.
- [ ] Add one-dimensional Bool, Nat, List, Box, and indexed-family query tests.
- [ ] Add a dependent constructor field test.

Primary migration points:

- `src/prototype/include/a_program/kernel/type_declaration.h`;
- `src/prototype/src/kernel/type_declaration.c`;
- new identity acted-schema query implementation;
- `src/prototype/src/kernel/rules/match/expansion_rule_emission.inc`;
- `src/prototype/src/kernel/typing/classifier_solver.inc`; and
- constructor classifier consumers.

Temporary policy:

The old generated declaration may remain callable only from focused differential
tests during DIA3-DIA4. It must not become fallback authority, artifact input,
or a compatibility API. The temporary oracle is deleted in DIA8.

Exit criteria:

- all selected 1D schema queries succeed from source schema plus action;
- dependent fields are handled by the same telescope traversal;
- `type_count` remains unchanged;
- query results survive cache deletion and reconstruction.

### DIA4. Generalize Context bridge and telescope action

Status: blocked on DIA3

- [ ] Extend Context-action identity with a dimension operator.
- [ ] Replace the fixed left/right/relation extension assumption with generic
  face traversal.
- [ ] Preserve endpoint substitutions as named projections of the general face
  map.
- [ ] Map every generated face binding back to source `BindingId` plus face key.
- [ ] Act on each dependent classifier under the complete acted prefix.
- [ ] Retain CwF Context and substitution certificates.
- [ ] Make Context materialization explicitly cache/workspace data.
- [ ] Verify cache deletion and deterministic reconstruction.
- [ ] Keep `PARAMETRIC_RELATION` and `OBJECT_IDENTITY` bridge semantics
  distinct.
- [ ] Reject resource-sensitive Context action until the required modality is
  explicitly represented.

Primary files:

- `src/prototype/include/a_program/identity/types.h`;
- `src/prototype/src/identity/context_bridge.inc`;
- `src/prototype/src/identity/telescope_action.inc`;
- `src/prototype/src/identity/action_execution.inc`;
- `src/prototype/src/identity/action_certificate_validation.inc`; and
- Context/Substitution focused tests.

Exit criteria:

- one Context action API constructs dimensions 1 and 2;
- `A : Universe, b : A, c : A` retains exact dependent fibers;
- no Context view requires a generated TypeDeclaration;
- rebuilding the view produces equivalent accepted Claims.

### DIA5. Cut over one-dimensional identity computation and object action

Status: blocked on DIA4

- [ ] Change identity computation output from a generated declaration-backed
  type former to `DIMENSION_ACTION(source, e)` plus boundary APPs.
- [ ] Replace generated constructor Terms with action on source constructor
  Terms.
- [ ] Add/adjust Judgement formation and term-action proof rules from DIA0.
- [ ] Update object term action for constructor, Lambda, APP, Match, IH,
  Return, and Thunk cases.
- [ ] Replace `generated_type_declaration_id` in new certificates with operator,
  source schema, acted Term, and formation Claim references.
- [ ] Update certificate validation to replay from source schema.
- [ ] Update Match refinement to use the acted schema view rather than checking
  `origin_kind`.
- [ ] Preserve residual outcomes for effects, unresolved rows, host primitives,
  and unsupported Universe cases.
- [ ] Move all non-differential 1D tests to the new path.

Primary files:

- `src/prototype/src/identity/identity_computation.inc`;
- `src/prototype/src/identity/object_term_action.inc`;
- `src/prototype/src/identity/action_certificate_validation.inc`;
- `src/prototype/src/identity/context_bridge.inc`;
- `src/prototype/src/kernel/rules/match/expansion_rule_emission.inc`;
- `src/prototype/include/a_program/kernel/judgement/types.h`; and
- kernel rule validators/replay dispatch.

Exit criteria:

- Bool, Nat, List, dependent Box, Pi, Return/Thunk, and Match identity tests use
  no generated declaration;
- non-DefEq admitted function identity witnesses remain ordinary Terms;
- forged action certificates fail read-only replay;
- all 1D identity requests preserve declaration count.

### DIA6. Replace INDEXED_HIGHER_LIFT with repeated generic action

Status: blocked on DIA5

- [ ] Construct the existing square boundary through dimension traversal.
- [ ] Replace fixed `bindings[8]`, `field_faces[64][9]`, and related arrays.
- [ ] Construct square acted schemas by applying the same action to an acted
  source.
- [ ] Port the all-false Bool square witness.
- [ ] Port field-bearing Box square witnesses.
- [ ] Add dependent field square action instead of preserving the current
  residual restriction.
- [ ] Add a dimension-three structural action test using the same APIs.
- [ ] Preserve proof relevance for identity-of-identity witnesses.
- [ ] Replace the `INDEXED_HIGHER_LIFT` computation rule with the generic action
  rule selected in DIA0.
- [ ] Ensure no rule tests `index_count == 8` to identify a square.

Exit criteria:

- dimensions 1, 2, and 3 use one representation and traversal;
- no active code contains fixed square schema arrays;
- repeated action does not allocate a type or constructor declaration;
- dependent telescope action works at the square boundary.

### DIA7. Introduce artifact v78 semantic action publication

Status: blocked on DIA6

- [ ] Archive `artifact_v77.schema` and write `artifact_v78.schema` first.
- [ ] Add dimension operator records or an equivalent canonical wire encoding.
- [ ] Add `DIMENSION_ACTION` to Term wire grammar and child closure.
- [ ] Serialize source/import declarations without origin fields.
- [ ] Remove generated declaration closure marking and dense-publication cases.
- [ ] Update identity roots to replay action from source Claim, operator, acted
  family Claim, and witness Claim.
- [ ] Serialize only rooted Context/Claim/Derivation evidence.
- [ ] Exclude action requests, work queues, cache views, and scratch Context
  identities not reachable as semantic proof evidence.
- [ ] Update relocation and linking for semantic operators and action Terms.
- [ ] Reject v77 artifacts.
- [ ] Update README files, spec consistency checks, and artifact fixtures.

Primary files:

- `src/prototype/spec/artifact_v78.schema`;
- `src/prototype/include/a_program/artifact/wire_v78.h`;
- `src/prototype/src/artifact/wire_v78.c`;
- `src/prototype/src/artifact/interface.c`;
- `src/prototype/src/artifact/link.c`;
- `src/prototype/src/artifact/publication/closure_marking_and_slices.inc`;
- `src/prototype/src/artifact/publication/dense_publication.inc`;
- driver/reader includes;
- `README.md`;
- `src/prototype/README.md`; and
- artifact integration tests.

Exit criteria:

- source declarations and acted identity evidence round-trip read-only;
- no generated type origin appears in the wire schema;
- request order and scratch cache state do not change semantic publication;
- obsolete v77 input is rejected cleanly.

### DIA8. Delete generated identity declaration authority

Status: blocked on DIA7

- [ ] Delete `PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY`.
- [ ] Delete `origin_kind` if no remaining source/import distinction requires
  it.
- [ ] Delete `origin_source_carrier_term_id`.
- [ ] Delete `prototype_type_declaration_add_generated_identity`.
- [ ] Delete generated identity lookup and origin validation APIs.
- [ ] Delete `generated_type_declaration_id` from certificates.
- [ ] Delete `generated_schema_validation.c` after moving any still-valid
  action checks into semantic validators.
- [ ] Delete `hott_initialize_generated_identity_declaration`.
- [ ] Delete `hott_initialize_indexed_higher_identity_declaration`.
- [ ] Delete generated constructor publication helpers used only by identity.
- [ ] Delete old differential-oracle tests and compatibility wrappers.
- [ ] Search the entire prototype for generated-origin and fixed-lift remnants.
- [ ] Verify source TypeDeclaration APIs no longer accept anonymous generated
  declarations.

Required zero-result searches:

```text
ORIGIN_GENERATED_IDENTITY
generated_type_declaration_id
add_generated_identity
find_generated_identity
validate_generated_identity
INDEXED_HIGHER_LIFT
```

Exit criteria:

- all searches above return zero active implementation references;
- `TypeDeclarationDB` is source/import authority only;
- no forwarding alias preserves the deleted model;
- clean build and tests pass after a fresh rebuild.

### DIA9. Full verification, metrics, and plan closure

Status: blocked on DIA8

- [ ] Run `make -f src/prototype/Makefile clean all reader`.
- [ ] Run focused dimension tests.
- [ ] Run focused HOTT identity and forgery tests.
- [ ] Run `sh src/prototype/tests/integration/test_artifact_flow.sh`.
- [ ] Run `make -f src/prototype/Makefile test-type-infer-and-check`.
- [ ] Run `make -f src/prototype/Makefile test-integration`.
- [ ] Run supported examples through the compiler.
- [ ] Repeat artifact publication to test determinism.
- [ ] Record before/after per-file added, deleted, and net lines.
- [ ] Record subsystem line totals before and after.
- [ ] Record clean-build and full-suite runtimes before and after.
- [ ] Update architecture documentation and this dashboard.
- [ ] Commit each completed gate separately and push `main` only after its exit
  criteria pass.

Completion criteria:

- all DIA0-DIA9 checkboxes are complete;
- no generated identity declaration authority remains;
- dimensions 1-3 use the same action model;
- v78 replay verifies ordinary object evidence independently;
- source and imported IADT behavior remains unchanged;
- no known test is skipped or weakened to complete the migration.

## 6. Permanent Test Matrix

| Test | Required property |
| --- | --- |
| `dimension_operator_identity` | identity operator preserves source Term |
| `dimension_operator_compose` | composition is deterministic and validated |
| `dimension_faces_1_2_3` | one traversal handles all tested dimensions |
| `dimension_face_overflow` | oversized requests fail without partial state |
| `identity_bool_no_declaration_growth` | 1D Bool identity leaves `type_count` unchanged |
| `identity_nat_recursive_action` | recursive constructor action remains guarded and typed |
| `identity_list_parameter_action` | parameterized ADT action selects exact parameter identity |
| `identity_dependent_constructor` | later field may depend on earlier acted field |
| `identity_indexed_adt` | result indices are refined from source constructor action |
| `identity_pi_pointwise` | function identity remains pointwise and proof relevant |
| `identity_non_defeq_function` | admitted observational witness does not alter DefEq |
| `identity_match_action` | Match witness uses acted constructor schema view |
| `identity_square_generic` | square uses repeated generic action, not index count 8 |
| `identity_cube_dimension_three` | dimension 3 has no special declaration generator |
| `identity_cache_rebuild` | deleting acted Context caches preserves evidence |
| `identity_request_order` | request order preserves semantic publication |
| `identity_relation_separation` | parametric relation cannot certify object identity |
| `identity_resource_residual` | unsupported graded Context remains residual |
| `artifact_v78_identity_roundtrip` | acted family and witness replay read-only |
| `artifact_v77_rejected` | no backward-compatibility path remains |
| `artifact_forged_action_rejected` | malformed operator/boundary/certificate fails |

## 7. File-level Migration Map

| Area | Current responsibility | Target change |
| --- | --- | --- |
| `core/term.h` and Term implementation | no object dimension action Term | add one generic action node and all graph operations |
| `kernel/type_declaration.*` | source and generated identity declarations | source/import schema only; add read-only acted schema query boundary |
| `identity/identity_computation.inc` | builds generated 1D and fixed 2D declarations | build action Terms and classifiers from source schema |
| `identity/context_bridge.inc` | fixed endpoint/relation Context chains | operator-indexed acted Context view |
| `identity/telescope_action.inc` | dependent action coupled to generated identity | generic acted-prefix telescope traversal |
| `identity/object_term_action.inc` | witnesses target generated families | witnesses target action Terms |
| `identity/action_certificate_validation.inc` | validates generated declaration IDs | validates source/operator/output proof relation |
| `identity/generated_schema_validation.c` | validates generated declaration hierarchy | delete after semantic checks move to action validators |
| Match expansion | recognizes generated 2-index declaration | query acted schema from action Term |
| artifact publication | marks generated declaration closure | mark action Term/operator/proof closure |
| artifact wire/link | serializes and relocates generated declarations | v78 source declarations plus dimension actions |
| HOTT tests | assert generated IDs and index counts | assert semantic boundaries and no declaration growth |

### 7.1 DIA0 baseline measurements

Measured at implementation-start commit
`7f76d82f97605b3abb75e09b99dc8418df4f7da3` on 2026-08-17:

| Command or metric | Baseline |
| --- | ---: |
| clean build plus reader | 5.550 s |
| focused HOTT goal | 24.849 s |
| artifact flow | 13.008 s |
| full integration suite | 1312.058 s |
| prototype C/header implementation | 130,412 lines |
| identity C/header implementation | 17,895 lines |
| generated-identity/fixed-lift search | 145 occurrences in 25 active/archive files |

The earlier 133/20 count used a narrower search. The 145/25 baseline is retained
for final deletion accounting and must be split into active and archived
references when DIA8 closes.

| Migration file | Baseline lines |
| --- | ---: |
| `include/a_program/core/term.h` | 1,288 |
| `src/core/term/storage_and_formation.inc` | 2,315 |
| `src/core/term/canonicalization.inc` | 2,057 |
| `src/core/term/evaluation_and_conversion.inc` | 4,106 |
| `src/core/term/declarations.inc` | 1,055 |
| `include/a_program/kernel/type_declaration.h` | 512 |
| `src/kernel/type_declaration.c` | 2,588 |
| `src/identity/identity_computation.inc` | 3,627 |
| `src/identity/context_bridge.inc` | 2,560 |
| `src/identity/telescope_action.inc` | 866 |
| `src/identity/object_term_action.inc` | 5,767 |
| `src/identity/action_certificate_validation.inc` | 1,740 |
| `src/identity/generated_schema_validation.c` | 1,104 |
| `src/kernel/rules/match/expansion_rule_emission.inc` | 632 |
| `src/artifact/publication/closure_marking_and_slices.inc` | 1,844 |
| `src/artifact/publication/dense_publication.inc` | 2,415 |
| `src/artifact/wire_v77.c` | 3,436 |
| `src/artifact/relocation.c` | 606 |
| `src/artifact/link.c` | 2,455 |
| `include/a_program/kernel/judgement/types.h` | 383 |
| `spec/hott_fragment_v5.schema` | 369 |

## 8. Stop Conditions

Stop the current phase and revise this plan if any of the following occurs:

- `DIMENSION_ACTION` requires a second nominal declaration identity to type a
  constructor;
- APP boundary spines cannot express required partial instantiation without
  ambiguous typing;
- an acted Context cannot be reconstructed from source Context, operator, and
  semantic boundary evidence;
- dependent fields require comparing classifier Term IDs rather than acting
  under the prior telescope;
- proof replay requires mutable HOTT action state;
- v78 publication depends on scratch Context allocation order;
- the new path silently accepts a case the old kernel rejected without a new
  rule and test;
- resource-sensitive assumptions are copied or contracted without an explicit
  modality;
- parametric relation action is used as object identity authority;
- generated declaration compatibility code is proposed as a shortcut; or
- fixed dimension-specific arrays reappear outside tests.

Discovering a stop condition is not permission to restore the old path. Record
the counterexample, update DIA0 and this plan, and resume only after the
semantic representation is settled.

## 9. Sequencing with the Post-DA Consolidation Plan

The Post-DA plan's RC0-RC2 constraint/effect work is independent and may run
before this migration. RC3 currently proposes capability separation while
explicitly testing generated Identity as a TypeDeclaration consumer. Performing
that part first would formalize the authority this plan removes.

Therefore:

```text
RC0-RC2 (optional independent work)
  -> DIA0-DIA9
  -> revised RC3 source-only type capability separation
  -> RC4-RC7
```

RC3 must be revised after DIA8 so its schema tests cover source/import ADTs,
IADTs, `Acc`, and acted schema queries without treating generated Identity as a
declaration.

## 10. Progress Dashboard

| Phase | Status | Commit | Notes |
| --- | --- | --- | --- |
| DIA0 specification | planned | - | next gate |
| DIA1 dimension core | blocked | - | waits for DIA0 |
| DIA2 Core action Term | blocked | - | waits for DIA1 |
| DIA3 acted schema query | blocked | - | waits for DIA2 |
| DIA4 Context/telescope action | blocked | - | waits for DIA3 |
| DIA5 1D cutover | blocked | - | waits for DIA4 |
| DIA6 generic higher action | blocked | - | waits for DIA5 |
| DIA7 artifact v78 | blocked | - | waits for DIA6 |
| DIA8 old authority deletion | blocked | - | waits for DIA7 |
| DIA9 verification/metrics | blocked | - | waits for DIA8 |

## 11. Baseline and Final Metrics

Fill this table during DIA0 and DIA9. Line count is diagnostic, not an
acceptance criterion. Removing duplicate authority is more important than net
reduction.

| Metric | Baseline | Final |
| --- | ---: | ---: |
| active artifact version | 77 | pending |
| generated identity search occurrences | 133 | target 0 |
| files containing generated identity references | 20 | target 0 |
| identity implementation/header lines | 17,895 | pending |
| dimension module lines | 0 | pending |
| total prototype implementation/header lines | pending DIA0 | pending |
| clean build time | pending DIA0 | pending |
| focused HOTT test time | pending DIA0 | pending |
| full integration time | pending DIA0 | pending |

Per-file final report:

| File | Added | Deleted | Net | Reason |
| --- | ---: | ---: | ---: | --- |
| pending DIA0 inventory | - | - | - | - |

## 12. Completion Definition

This plan is complete only when all of the following are true:

1. object identity and higher action are represented by one generic Core action;
2. TypeDeclarationDB contains no generated identity declarations;
3. requesting identity never changes declaration or constructor count;
4. Context materialization is explicitly derived and rebuildable;
5. dependent constructor telescopes use the same action algorithm as
   nondependent ones;
6. dimensions 1, 2, and 3 use one operator and face traversal;
7. identity proofs remain ordinary Terms, Claims, and Derivations;
8. parametricity and object identity authority remain distinct;
9. artifact v78 persists semantic action evidence but no generated schemas;
10. v77 is rejected without a compatibility path;
11. all obsolete generated-identity symbols are deleted;
12. full tests and deterministic replay pass; and
13. the final line/runtime report is recorded here.

## 13. Implementation Log

### 2026-08-17: plan created

- Compared the current generated-IADT hierarchy with Narya's generic
  dimension/action separation.
- Selected one generic Core `DIMENSION_ACTION` representation.
- Selected ordinary APP spines for explicit boundary application.
- Rejected a permanent acted-schema database.
- Retained concrete Context materialization as derived checking workspace.
- Required a breaking artifact migration and final deletion of the old path.

## 14. Privacy Review

This plan contains repository-relative paths, public commit identifiers, and
public project terminology only. It contains no personal names, email
addresses, credentials, tokens, private URLs, or absolute home-directory
paths.
