# Dimension Action Identity Migration Implementation Plan

Date: 2026-08-17

Status: complete

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
- `src/prototype/spec/artifact_v78.schema`.

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

Current correction target:

- an identity family is not a newly declared IADT;
- one-dimensional identity is `DIMENSION_ACTION(source, e0)`;
- its endpoints are ordinary APP arguments;
- identity of that family is
  `DIMENSION_ACTION(DIMENSION_ACTION(source, e0), e1)`;
- dimensions 2 and above must be obtained by repeating the same operation; and
- the old generated declaration path may be inspected only as a temporary
  differential oracle until DIA8 deletes it. It must never be consulted as
  semantic fallback by the active compiler path.

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

- formation of an acted type-family head as HAS_TYPE; ordinary boundary APP
  and IS_TYPE_FROM_HAS_TYPE produce the final type judgement;
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

Status: complete

- [x] Add checked immutable operator storage and interning.
- [x] Add variable-length face-key storage or caller-owned face values.
- [x] Add deterministic traversal of all required boundary faces.
- [x] Add checked cardinality/overflow helpers.
- [x] Add face restriction and operator composition.
- [x] Add transaction marks or rollback compatible with existing prototype
  arena conventions.
- [x] Add focused unit tests for dimensions 0 through 4.
- [x] Add malformed operator, duplicate-axis, invalid-face, and overflow tests.
- [x] Verify that no dimension-module consumer manually counts square faces.

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

Status: complete

- [x] Add one Core Term tag and payload containing source Term and semantic
  operator identity.
- [x] Add child roles for source traversal.
- [x] Add formation, interning, hashing, comparison, and debug readback.
- [x] Add clone, canonicalization, substitution, reindex, alpha-interning, and
  free-binding traversal.
- [x] Keep untyped normalization neutral until DIA5 supplies the typed
  dimension certificate required by the DIA0 identity/composition laws.
- [x] Preserve one common Term representation across value, computation, and
  type classifications.
- [x] Add malformed operator-reference validation at formation and relocation.
- [x] Add round-trip tests in TermDB before adding HOTT semantics.

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

Status: complete

- [x] Define a read-only `prototype_type_schema_view` or equivalently narrow
  query result.
- [x] Resolve source `TYPE_VIEW` Terms through `TypeDeclarationDB` as today.
- [x] Resolve one-dimensional `DIMENSION_ACTION` Terms from their source schema
  and act on constructor classifier telescopes.
- [x] Generalize the same query to already acted source families and arbitrary
  source/target dimensions without generated declaration lookup.
- [x] Keep source declaration and constructor IDs in every acted view.
- [x] Derive acted constructor classifier Terms without allocating declarations;
  DIA4 must expose their face-expanded telescope view.
- [x] Derive curried classifiers through existing Pi/telescope construction.
- [x] Ensure caches are rebuildable and are not part of schema identity.
- [x] Add an assertion/test that schema queries cannot mutate
  `TypeDeclarationDB`.
- [x] Add one-dimensional List parameter and indexed-family query tests. Bool,
  Nat, and Box coverage is active.
- [x] Add a dependent constructor field test.

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

Status: complete

- [x] Extend Context-action identity with a dimension operator.
- [x] Replace the fixed left/right/relation extension assumption with generic
  face traversal.
- [x] Preserve endpoint substitutions as named projections of the general face
  map.
- [x] Map every generated 1D face binding back to source `BindingId` plus face
  key; extend the same storage to dimensions 2 and 3.
- [x] Act on each dependent classifier under the complete acted prefix.
- [x] Retain CwF Context and substitution certificates.
- [x] Make Context materialization explicitly cache/workspace data.
- [x] Verify cache deletion and deterministic reconstruction.
- [x] Keep `PARAMETRIC_RELATION` and `OBJECT_IDENTITY` bridge semantics
  distinct.
- [x] Reject resource-sensitive Context action until the required modality is
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

Status: complete

- [x] Change ordinary ADT identity computation output from a generated declaration-backed
  type former to `DIMENSION_ACTION(source, e)` plus boundary APPs.
- [x] Replace ordinary ADT generated constructor Terms with action on source constructor
  Terms.
- [x] Add replayable Judgement formation and term-action proof rules from DIA0.
- [x] Update object term action for constructor, Lambda, APP, Match, IH,
  Return, and Thunk cases.
- [x] Replace `generated_type_declaration_id` in new certificates with operator,
  source schema, acted Term, and formation Claim references.
- [x] Update certificate validation to replay from source schema.
- [x] Update Match refinement to use the acted schema view rather than checking
  `origin_kind`.
- [x] Preserve residual outcomes for effects, unresolved rows, host primitives,
  and unsupported Universe cases.
- [x] Move all non-differential 1D tests to the new path.

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

Status: complete

Implementation sequence:

1. Treat the target dimension `d` as the only source of face arity. A source
   constructor field expands to `3^d` binders in canonical face-ordinal order.
2. For every face, derive the field classifier under the complete acted prefix:
   vertices reindex the source classifier to that face, while a positive-
   dimensional face applies the corresponding repeated action to all of that
   face's boundary values.
3. Construct each result boundary by applying the source or acted constructor
   for that face dimension to the same face-local field values.
4. End the telescope in the acted owner family applied to all non-centre result
   faces. Do not allocate a `TypeDeclaration` or infer dimension from an index
   count.
5. Port the square test to inspect `DIMENSION_ACTION` spines and add a dimension
   three structural test through the same schema query.

The face-in-face ordinal embedding belongs in the dimension module. It must not
be reimplemented with fixed square arrays in the schema, identity, or test
layers.

Discovered prerequisite and correction:

- a dependent source field classifier is scoped by its source telescope prefix;
- its acted classifier is scoped by a different prefix containing every face of
  each preceding field;
- therefore action formation must explicitly relate the source and target
  Contexts and validate the acted prefix, rather than requiring both Terms to
  inhabit one Context; and
- the 0-to-1 cross-Context formation boundary has been implemented in the
  working tree. DIA6 generalizes the same rule through face traversal.

- [x] Construct the existing square boundary through dimension traversal.
- [x] Replace fixed `bindings[8]`, `field_faces[64][9]`, and related arrays.
- [x] Construct square acted schemas by applying the same action to an acted
  source.
- [x] Recognize a fully applied `DIMENSION_ACTION` family by peeling its
  ordinary APP spine and validating every boundary argument against the source
  operator traversal.
- [x] Select the canonical extension operator `e_n : n -> n+1` from the acted
  source dimension; never identify dimensions by generated declaration index
  count.
- [x] Port the all-false Bool square witness.
- [x] Port field-bearing Box square witnesses.
- [x] Add dependent field square action instead of preserving the current
  residual restriction.
- [x] Add a dimension-three structural action test using the same APIs.
- [x] Preserve proof relevance for identity-of-identity witnesses.
- [x] Replace the `INDEXED_HIGHER_LIFT` computation rule with the generic action
  rule selected in DIA0.
- [x] Ensure no rule tests `index_count == 8` to identify a square.

Verification gates:

- [x] the existing dependent Box 1D test passes without declaration growth;
- [x] the all-false Box square has eight boundaries and one centre field, all
  derived from `3^2` traversal;
- [x] dimension three derives `3^3` field faces without a dimension-specific
  array or declaration generator;
- [x] repeated schema queries leave `TypeDeclarationDB` counts and semantic
  revision unchanged; and
- [x] kernel replay validates the same acted classifier independently of action
  request order.

Stop conditions:

- stop if a dependent field is compared by accidental Term ID equality instead
  of being acted under the prior telescope;
- stop if a generated declaration is proposed as fallback authority;
- stop if any implementation selects dimension two through `index_count == 8`
  or another shape-specific constant; and
- stop if schema query correctness depends on mutating `TypeDeclarationDB`.

Exit criteria:

- dimensions 1, 2, and 3 use one representation and traversal;
- no active code contains fixed square schema arrays;
- repeated action does not allocate a type or constructor declaration;
- dependent telescope action works at the square boundary.

### DIA7. Introduce artifact v78 semantic action publication

Status: complete

- [x] Archive `artifact_v77.schema` and write `artifact_v78.schema` first.
- [x] Archive `hott_fragment_v5.schema` and write `hott_fragment_v6.schema`.
- [x] Add dimension operator records or an equivalent canonical wire encoding.
- [x] Add `DIMENSION_ACTION` to Term wire grammar and child closure.
- [x] Serialize source/import declarations without origin fields.
- [x] Remove generated declaration closure marking and dense-publication cases.
- [x] Update identity roots to replay action from source Claim, operator, acted
  family Claim, and witness Claim.
- [x] Serialize only rooted Context/Claim/Derivation evidence.
- [x] Exclude action requests, work queues, cache views, and scratch Context
  identities not reachable as semantic proof evidence.
- [x] Update relocation and linking for semantic operators and action Terms.
- [x] Reject v77 artifacts.
- [x] Update README files, spec consistency checks, and artifact fixtures.

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

Status: complete

- [x] Delete `PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY`.
- [x] Delete `origin_kind` if no remaining source/import distinction requires
  it.
- [x] Delete `origin_source_carrier_term_id`.
- [x] Delete `prototype_type_declaration_add_generated_identity`.
- [x] Delete generated identity lookup and origin validation APIs.
- [x] Delete `generated_type_declaration_id` from certificates.
- [x] Delete `generated_schema_validation.c` after moving any still-valid
  action checks into semantic validators.
- [x] Delete `hott_initialize_generated_identity_declaration`.
- [x] Delete `hott_initialize_indexed_higher_identity_declaration`.
- [x] Delete generated constructor publication helpers used only by identity.
- [x] Delete old differential-oracle tests and compatibility wrappers.
- [x] Search the entire prototype for generated-origin and fixed-lift remnants.
- [x] Verify source TypeDeclaration APIs no longer accept anonymous generated
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

Status: complete

- [x] Run `make -f src/prototype/Makefile clean all reader`.
- [x] Run focused dimension tests.
- [x] Run focused HOTT identity and forgery tests.
- [x] Run `sh src/prototype/tests/integration/test_artifact_flow.sh`.
- [x] Run `make -f src/prototype/Makefile test-type-infer-and-check`.
- [x] Run `make -f src/prototype/Makefile test-integration`.
- [x] Run supported examples through the compiler.
- [x] Repeat artifact publication to test determinism.
- [x] Record before/after per-file added, deleted, and net lines.
- [x] Record subsystem line totals before and after.
- [x] Record clean-build and full-suite runtimes before and after.
- [x] Update architecture documentation and this dashboard.
- [x] Commit each completed gate separately and push `main` only after its exit
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
| `src/artifact/wire_v78.c` | 3,436 |
| `src/artifact/relocation.c` | 606 |
| `src/artifact/link.c` | 2,455 |
| `include/a_program/kernel/judgement/types.h` | 383 |
| `spec/hott_fragment_v5.schema` | 369; archived and replaced by v6 |

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
| DIA0 specification | complete | `82c728d` | generic calculus and invariants fixed |
| DIA1 dimension core | complete | `4a7ccf8` | generic operators, faces, traversal, and tests |
| DIA2 Core action Term | complete | `8c3c6c9`, `f68008f` | action Term and semantic operator ownership |
| DIA3 acted schema query | complete | `b03215b`, `7f3becd`, `291b06e` | recursive acted schemas and indexed/dependent tests pass |
| DIA4 Context/telescope action | complete | `3992084`, `b51df37`, `291b06e` | arbitrary-dimension face traversal and dependent prefixes pass |
| DIA5 1D cutover | complete | `5e9aaa5`, `291b06e` | all supported object actions use semantic dimension action |
| DIA6 generic higher action | complete | `291b06e` | dimensions 1-3 use one traversal; square/cube/dependent tests pass |
| DIA7 artifact v78 | complete | `291b06e` | v78 publishes operators/actions and rejects v77 |
| DIA8 old authority deletion | complete | `291b06e` | active legacy-authority search is empty |
| DIA9 verification/metrics | complete | `291b06e` | full suite passed and implementation was pushed to `main` |

## 11. Baseline and Final Metrics

Fill this table during DIA0 and DIA9. Line count is diagnostic, not an
acceptance criterion. Removing duplicate authority is more important than net
reduction.

| Metric | Baseline | Final |
| --- | ---: | ---: |
| active artifact version | 77 | 78 |
| generated identity search occurrences | 133 | 0 active |
| files containing generated identity references | 20 | 0 active |
| identity implementation/header lines | 17,895 | 14,145 |
| dimension module lines | 0 | 1,965 |
| total prototype implementation/header lines | 130,412 | 131,174 |
| clean build time | 5.550 s | 7.609 s |
| focused HOTT test time | 24.849 s | 32.489 s |
| artifact flow time | 13.008 s | 17.851 s |
| full integration time | 1312.058 s | 2133.264 s |

Per-file final report:

| File | Added | Deleted | Net | Reason |
| --- | ---: | ---: | ---: | --- |
| `include/a_program/core/term.h` | 26 | 3 | +23 | one generic action Term |
| `src/core/term/storage_and_formation.inc` | 60 | 19 | +41 | action formation/storage |
| `src/core/term/canonicalization.inc` | 39 | 0 | +39 | semantic action canonicalization |
| `src/core/term/evaluation_and_conversion.inc` | 82 | 0 | +82 | neutral action comparison |
| `src/core/term/declarations.inc` | 13 | 0 | +13 | action readback |
| `include/a_program/kernel/type_declaration.h` | 1 | 44 | -43 | generated authority removed |
| `src/kernel/type_declaration.c` | 2 | 103 | -101 | generated declaration APIs removed |
| `src/identity/identity_computation.inc` | 2,706 | 2,771 | -65 | generic action computation |
| `src/identity/context_bridge.inc` | 0 | 2,435 | -2,435 | fixed bridge deleted |
| `src/identity/telescope_action.inc` | 102 | 355 | -253 | generic face telescope |
| `src/identity/object_term_action.inc` | 1,387 | 998 | +389 | action witnesses and replay inputs |
| `src/identity/action_certificate_validation.inc` | 368 | 752 | -384 | semantic certificate replay |
| `src/identity/generated_schema_validation.c` | 0 | 1,104 | -1,104 | duplicate authority deleted |
| `src/kernel/rules/match/expansion_rule_emission.inc` | 31 | 15 | +16 | acted schema query |
| `src/artifact/publication/closure_marking_and_slices.inc` | 88 | 35 | +53 | semantic operator/action closure |
| `src/artifact/publication/dense_publication.inc` | 30 | 11 | +19 | v78 dense maps |
| `src/artifact/wire_v77.c` -> `wire_v78.c` | 75 | 32 | +43 | breaking wire migration |
| `src/artifact/relocation.c` | 0 | 0 | 0 | existing relocation abstraction retained |
| `src/artifact/link.c` | 141 | 17 | +124 | operator and action linking |
| `include/a_program/kernel/judgement/types.h` | 5 | 1 | +4 | proof kinds 45-48 |
| `spec/hott_fragment_v5.schema` -> `v6` | 203 | 369 | -166 | current semantic fragment |

Across the complete working diff before the final status-only commit: 9,658
lines were added, 11,478 were deleted, net -1,820. Restricting the diff to implementation `.c`, `.h`,
and `.inc` files gives 8,924 additions, 11,320 deletions, net -2,396. The
production implementation total grew by 762 lines relative to the DIA0
baseline because the generic dimension module and v78 replay support replace a
larger identity-specific authority implementation.

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

### 2026-08-17: migration implemented

- Replaced generated one- and two-dimensional identity declarations with the
  single semantic `DIMENSION_ACTION` Term and generic face traversal.
- Removed generated declaration origin fields, lookup APIs, certificate fields,
  validators, fixed lift rules, and their artifact authority.
- Added v78 operator/action publication and HOTT fragment v6; v77 and v5 are
  retained only in the specification archive.
- Fixed reader initialization so every compile metadata instance receives the
  dimension operator and image stores required to intern action Terms.
- Made artifact derivation closure traversal semantic and deterministic instead
  of depending on Derivation arena insertion order.
- Strengthened artifact identity-root validation so rule labels must agree with
  the source former (`TYPE_VIEW` for ordinary ADT and `THUNK_TYPE` over
  `COMPUTATION_TYPE` for Thunk/Return).
- Verified dimension 1-3 schema action, dependent constructor fields, square
  witnesses, proof relevance, read-only replay, forged-root rejection, and
  declaration-count invariance.
- Committed the complete cutover as `291b06e` and pushed it to `origin/main`.

## 14. Privacy Review

This plan contains repository-relative paths, public commit identifiers, and
public project terminology only. It contains no personal names, email
addresses, credentials, tokens, private URLs, or absolute home-directory
paths.
