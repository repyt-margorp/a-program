# Type View and Constructor Classifier Reform

Date: 2026-07-04

Status: current implementation target

This document records the current implementation goal after reviewing the
interaction between type former application, `TELESCOPE`, `PI`, constructor
classifiers, and named type views.

The design target has changed from treating every type declaration as a
generative identity in the core graph. The new target is to separate:

- core computation graph identity
- source-level type views and constructor names
- classifier facts and proof obligations

This matches the existing lambda design goal: the core lambda term `\x => x`
can be shared by `identityBool : Bool -> Bool` and `identityNat : Nat -> Nat`,
while the named typed entries remain distinct annotations over the same core
computation.

## Current Implementation State

The prototype currently has several related but separate representations.

`APP` is the ordinary unary application node:

```text
APP(f, x)
APP(APP(f, x), y)
```

Type former application used to be special-cased through `TELESCOPE`:

```text
Bool
  = APP(TYPE_FORMER(Bool), TELESCOPE_NIL)

List Nat
  = APP(TYPE_FORMER(List),
        TELESCOPE_CONS(Nat, TELESCOPE_NIL))
```

New lowering now builds source type views through
`prototype_term_type_instance_make(...)`. A source type view contains both a
structural core spine and a source spine:

```text
Bool
  = TYPE_VIEW(Bool,
      core   = TYPE_FORMER(BoolShape),
      source = TYPE_FORMER(Bool))

List Nat
  = TYPE_VIEW(List,
      core   = APP(TYPE_FORMER(ListShape), TYPE_FORMER(NatShape)),
      source = APP(TYPE_FORMER(List), TYPE_VIEW(Nat, ...)))
```

`prototype_term_type_instance_info(...)` still understands the old
`TELESCOPE` form so older graph paths can be inspected while the tag remains in
the enum and artifact reader. New semantic graph generation no longer uses it.

Function types are represented separately by `PI`:

```text
A -> B
  = PI(A, \_ => B)

(x : A) -> B x
  = PI(A, \x => B x)
```

Constructor field schemas are also separate. They live in `TypeDeclarationDB`
as `type_expr` field lists and are materialized later into a `PI` classifier
when a constructor is used. This means constructor schemas are not currently
first-class graph terms.

The current result is mostly view/core split, with a few compatibility paths:

- value/function application uses `APP`
- type former application uses ordinary `APP` spines inside `TYPE_VIEW`
- function classifiers use `PI`
- constructor classifier families are graph terms
- constructor field lists remain as compatibility metadata
- constructor owner label lookup uses the source view, not only the core shape

## Target Model

The target is to make the core graph more uniform while keeping enough metadata
for names, namespaces, artifact interfaces, and diagnostics.

Type former application should eventually be represented as an ordinary `APP`
spine:

```text
Bool
  = TYPE_FORMER(BoolShape)

List Nat
  = APP(TYPE_FORMER(ListShape), Nat)

Sigma A B
  = APP(APP(TYPE_FORMER(SigmaShape), A), B)
```

In this model, `TELESCOPE_NIL` and `TELESCOPE_CONS` are no longer semantic
nodes. The information they carry can be recovered by reading the `APP` spine.

Names such as `Bool`, `Two`, `List`, and `Sigma` should not necessarily be core
graph identities. They should point to typed source-level views over core type
shapes. For example:

```text
core shape:
  finite type with two nullary constructors

view Bool:
  true  -> ordinal 0
  false -> ordinal 1

view Two:
  one  -> ordinal 0
  zero -> ordinal 1
```

Under this view, `Bool` and `Two` may share the same core type shape, while the
source-level names and constructor labels remain separate annotations. This is
the same layering principle used for typed names over shared lambda terms.

This distinction is intentional. Structural sharing belongs to the core graph;
source names should point to view entries over that graph. In the same way that
`identityBool` and `identityNat` can both point to the same core lambda while
carrying different classifiers, `Bool` and `Two` can point to the same finite
two-constructor core shape while carrying different constructor label tables.
`Bool.true` and `Two.one` may therefore resolve to the same core constructor
ordinal under different views, but the view must remain available for source
typing, diagnostics, exports, and any later explicit transport or relabeling.

The typechecker must still decide when views may be erased. The intended
default is view-preserving at the source boundary:

- evaluation and normalization operate on the core graph
- source names resolve through a view
- diagnostics and exports preserve the view used by the source
- explicit cast, transport, or relabeling may later move between views

## Constructor Classifier Goal

The current `field_types[]` constructor schema should be replaced by constructor
classifier families represented as ordinary graph terms.

For `List`:

```text
ListShape : @ -> @

nil_classifier_family
  = \A : @ =>
      List A

cons_classifier_family
  = \A : @ =>
      PI(A, \x =>
        PI(List A, \rest =>
          List A))
```

For `Sigma`:

```text
SigmaShape : (A : @) -> (A -> @) -> @

mk_classifier_family
  = \A : @ =>
    \B : A -> @ =>
      PI(A, \a =>
        PI(B a, \b =>
          Sigma A B))
```

This is the main reason the existing field-list schema is insufficient. A
constructor such as `mk` needs the second field type `B a` to refer to the first
field binder `a`. A plain field list cannot express that dependency cleanly.

The metadata that remains outside the graph should be minimal:

- type view name
- constructor label
- constructor ordinal
- pointer to the constructor classifier family graph
- export and source span information

The classifier itself should be graph data, not an ad hoc schema.

## Required Implementation Work

1. Hide `TELESCOPE` behind a type-application API.

   Before changing the representation, all direct users should go through a
   narrow API:

   ```text
   type_instance_make(former, args)
   type_instance_info(term, former, args)
   type_instance_extend(term, arg)
   type_instance_is_saturated(term)
   ```

   This keeps the current behavior working while preparing the implementation
   to switch from `TELESCOPE` to ordinary `APP` spine.

2. Replace constructor field-list materialization.

   `materialize_constructor_classifier(...)` currently reads field `type_expr`
   metadata and constructs a `PI` chain at use time. It should instead retrieve
   the constructor classifier family graph, apply it to the owner arguments, and
   normalize the result to a `PI` spine.

   Conceptually:

   ```text
   owner = Sigma A B
   constructor = mk

   classifier =
     WHNF(APP(APP(mk_classifier_family, A), B))
   ```

3. Make match pattern binder types come from constructor classifiers.

   Match branch binders should be classified by walking the resulting `PI`
   spine, not by looking up field schemas. This is what allows dependencies
   between fields.

   For:

   ```text
   value @mk a b => body
   ```

   the checker should derive:

   ```text
   a : A
   b : B a
   ```

4. Separate core type shape from source type view.

   `TYPE_FORMER(type_id)` used to behave like a generative type identity. The
   implementation now distinguishes a structural core shape from a named source
   view by wrapping type instances in `TYPE_VIEW(view_type_id, core, source)`.
   The compiler no longer has to keep structurally identical declarations apart
   at the core graph layer.

   The current split is:

   ```text
   core graph:
     TYPE_FORMER(shape_id)

   source graph/view:
     TYPE_VIEW(view_type_id, core, source)
   ```

   `TYPE_VIEW` is not a lambda-core computation node. It is a source annotation
   layer in the term graph. It is visible to name resolution, artifact exports,
   diagnostics, and source-level classifier annotations, while `core` preserves
   the structural sharing target.

5. Keep `PI` as the kernel function-type representation for now.

   `PI` should not be removed in this reform. Function-type recognition should
   remain centralized behind helpers such as `classifier_as_pi(...)`, so that
   later representations can be added without spreading special cases.

6. Remove `TELESCOPE` from newly generated semantic type applications.

   New type former application should use ordinary `APP` spines. The old
   `TELESCOPE_NIL` and `TELESCOPE_CONS` tags may remain temporarily as legacy
   artifact/debug tags, but `prototype_term_type_instance_make(...)` must not
   generate them.

### Implementation Status as of 2026-07-04 23:50 JST

- Done: type instance construction and inspection are routed through
  `prototype_term_type_instance_make/info/extend/is_saturated`.
- Done: newly generated type instances use ordinary `APP` spines and
  `TYPE_VIEW`; `TELESCOPE` is only a legacy reader/debug surface.
- Done: constructor classifier families are graph terms and are exported in
  artifacts.
- Done: match pattern binder typing walks constructor classifier `PI` spines,
  including dependent fields such as `mk : (a : A) -> B a -> *`.
- Done: `Bool` and `Two` can share a type code shape key and `core_type_id`
  while preserving source constructor labels.
- Done: artifact format `A_PROGRAM_ARTIFACT 21` preserves type view metadata,
  constructor classifier family ids, and source/core term graph data.
- Deferred: source-level views are not made implicitly interchangeable. That
  remains a future explicit equality/transport/cast problem.
- Deferred: constructor field-list metadata remains for compatibility,
  type code shape keys, and interface-only fallback paths.

## Issues That Motivated This Reform

### `TELESCOPE` Is Not a Real Dependent Telescope

The name suggests a binder context, but the current nodes only represent a list
of type former arguments. This makes the code harder to reason about when
discussing actual dependent constructor fields such as `B a`.

### `PI` and Type Former Application Use Different Spines

Function application uses nested `APP`, while type former application packages
arguments into `TELESCOPE`. This forces many helpers to know about a special
type-application shape.

### Constructor Schemas Are Not Graph Terms

Constructor field types are stored as `type_expr` metadata. They are converted
to `PI` terms only when needed. This blocks a clean treatment of dependent
constructor fields and makes `Sigma` awkward.

### Bool/Two Generativity Was Too Coarse

The previous implementation direction intentionally kept independently declared
`Bool` and `Two` apart by declaration identity. The revised model allows their
core shape to coincide while preserving different source-level views and
constructor labels.

This is not the same as erasing all type information. The distinction moves from
core graph identity to the annotation/view layer.

### Source Names Need a View Layer

If names point directly at core type graph nodes, then structurally identical
types lose their source-level distinction. If names point at views, the compiler
can share the computation graph while still remembering that the source wrote
`Bool.true` rather than `Two.one`.

### Match and Induction Depend on Constructor Classifiers

The work on match motive synthesis and induction hypotheses exposed that branch
binder classifiers should be derived from constructor classifier graphs. This
becomes especially important when a later field depends on an earlier field.

## Non-Goals for This Step

- Do not introduce object-level equality, `refl`, `transport`, or rewrite as
  part of this refactor.
- Do not remove `PI` yet.
- Do not solve universe precision here.
- Do not make arbitrary source-level views interchangeable without an explicit
  cast, transport, or later equality mechanism.
- Do not add a separate high-level `ConstructorSchema` term if the same
  information can be represented as `PI/LAMBDA/APP` graph terms plus metadata.

## Completion Criteria

The refactor is complete when:

- type former applications can be read and built through one abstraction
- constructor classifier families are graph terms
- constructor field binders are typed by walking classifier `PI` spines
- `Sigma A B` can express `mk : (a : A) -> B a -> Sigma A B`
- `Bool` and `Two` can share a core shape without losing their distinct source
  views
- artifact export/import preserves view metadata and constructor label mapping
- normalization continues to operate on core graph structure
- source-level diagnostics still report the names used by the source program

For this document, "complete" does not mean source-level views become
definitionally interchangeable. `Bool` and `Two` may share a core shape, but
moving values between those views still requires a later equality, transport,
cast, or relabeling design. It also does not require deleting every legacy
field-list or `TELESCOPE` reader path in the same step; those paths are allowed
to remain when they are explicitly compatibility-only and are not used for new
semantic graph generation.

## Remaining Implementation Gaps

As of 2026-07-04 23:47 JST, the following gaps remain.

### Type Views Exist, But Equality Modes Are Still Coarse

The term graph now has an explicit source view wrapper:

```text
TYPE_VIEW(view_type_id, core, source)
```

`core` is the structural type-instance spine, using the representative
`TYPE_FORMER(core_type_id)` and core arguments. `source` is the source-level
spine that preserves view arguments and constructor-label lookup. This lets
`Bool` and `Two` share a structural shape while still keeping `Bool.true` and
`Two.one` distinct at the source view layer.

Normalization equality currently treats `TYPE_VIEW` as view-sensitive and
source-sensitive. That is the conservative rule for type checking. A separate
core-shape equality mode may still be needed later for explicit transport,
casts, or user-provided equality proofs. It should not be silently substituted
for the current typed-view equality.

### Constructor Field Lists Remain as Compatibility Metadata

Constructor classifier families are now graph terms and the main constructor
classifier path uses them when available. Match binder typing and recursive IH
field detection also read constructor classifier `PI` spines.

The old field-list metadata still remains for:

- artifact compatibility
- built-in declarations that do not yet have classifier family graphs
- interface-only imports where provider graph terms are not available in the
  current `TermDB`
- type code shape keys, which still hash field-list type expressions

This metadata should shrink further once constructor classifier families become
the canonical exported constructor schema.

### Dependent Constructor Field Syntax Is Partially Implemented

The graph side can represent a classifier family such as:

```text
\A => \B => PI(A, \a => PI(B a, \b => Sigma A B))
```

The source type-definition syntax now accepts named constructor fields in
constructor types:

```text
Sigma := \A : @ => \B : A -> @ => @{
  mk : (a : A) -> B a -> *;
};
```

Lowering records the field binder metadata and uses it when constructing the
constructor classifier family graph. Match pattern binder typing can then derive
the second binder type as `B a`.

The remaining limitation is that constructor result syntax still ends in `*`.
Indexed result families such as `Vec A n` are still outside this step.

### `TELESCOPE` Tags Remain as Legacy Reader Surface

New type instance construction no longer emits `TELESCOPE_NIL` or
`TELESCOPE_CONS`; generated examples and artifacts now use ordinary `APP`
spines. The enum tags, old parser paths, and printers still exist so legacy
graph data can be inspected while the prototype settles.

## Progress Log

### 2026-07-04 22:44 JST

- Added this implementation target document.
- Renamed the public prototype term API from `prototype_term_type_formation`
  and `prototype_term_type_formation_info` to
  `prototype_term_type_instance_make` and
  `prototype_term_type_instance_info`.
- Added `prototype_term_type_instance_extend` so callers do not manually append
  to the hidden `TELESCOPE` argument list.
- Added `prototype_term_type_instance_is_saturated` so callers do not directly
  compare type former argument counts against declaration arity.
- Renamed internal `type_formation_*` helpers that referred to type instances
  rather than AST type formation.
- Updated `ast.c`, `typing.c`, `reader.c`, and `term.c` to use the new
  `type_instance` API names.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 22:51 JST

- Added `classifier_family` to `prototype_type_constructor_declaration`.
- Source-compiled constructors now build a constructor classifier family graph
  from the current field types and owner type instance.
- `materialize_constructor_classifier(...)` now prefers the classifier family
  graph when it is available, applies it to the owner arguments, normalizes it,
  and falls back to the old field-list materialization path for imported or
  built-in declarations that do not yet have a family graph.
- Artifact graph serialization now records the constructor classifier family
  term id on each `type_constructor` line.
- Graph append relocation now offsets `classifier_family` term references.
- Added an artifact-flow regression check that the source-defined `cons`
  constructor exports a non-empty classifier family id.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 22:55 JST

- Added `classifier_family` to compile and artifact constructor exports.
- Updated artifact text format to `A_PROGRAM_ARTIFACT 19`.
- Interface constructor export lines now preserve the constructor classifier
  family term id in addition to legacy field-list refs.
- Artifact append relocation now offsets interface constructor classifier
  family term refs when provider graphs are aggregated.
- Updated `read_file.out` interface diagnostics to print constructor
  classifier family ids.
- Added an artifact-flow regression check that `List.cons` interface export
  preserves a non-empty classifier family id.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.

### 2026-07-04 23:01 JST

- Changed induction-recursive-field detection to read the field classifier from
  the constructor classifier `PI` spine and compare it with the constructor
  owner by normalization equality.
- Removed the old local helpers in `typing.c` that classified recursive IH
  fields by directly reading `type_expr SELF` from constructor field metadata.
- This moves IH validity closer to the same classifier graph used for match
  pattern binder typing.
- Kept the field-list fallback in `materialize_constructor_classifier(...)`
  for built-in and interface-only imported constructors that do not yet have a
  usable classifier family graph in the current term database.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 23:03 JST

- Added a regression case for structurally equal but separately named type
  views.
- The new check verifies that `idBool := \x : Bool => x` and
  `idTwo := \y : Two => y` share the same core lambda term.
- The same check verifies that `Bool` and `Two` export the same structural type
  key while preserving distinct constructor labels and classifier family ids in
  the interface.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.

### 2026-07-04 23:11 JST

- Added constructor-field binder metadata to the prototype AST.
- Updated the prototype reader so constructor types can contain named fields,
  such as `(a : A) -> B a -> *`.
- Updated constructor classifier family lowering to emit dependent `PI` nodes
  with the field binder as the codomain-family lambda binder.
- Added a regression case for `Sigma`-style dependent constructor fields and a
  `first` eliminator that verifies match pattern binder typing derives
  `b : B a`.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/List.p` and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 23:15 JST

- Clarified the intended `Bool`/`Two` layering decision.
- Structural sharing of equal type declarations is allowed at the core graph
  shape layer.
- Source names such as `Bool` and `Two` should point to source-level type view
  entries over the shared shape, not directly to an unqualified raw shape.
- Constructor names such as `Bool.true` and `Two.one` should likewise preserve
  their source view and label table even if they share the same core
  constructor ordinal.
- This means the next implementation step must introduce or expose a type-view
  layer before changing local `TYPE_FORMER(type_id)` equality/hash-consing to
  structural shape equality.

### 2026-07-04 23:21 JST

- Added `prototype_type_declaration_core_shape_representative(...)`, which computes
  the local representative declaration for a structural type shape.
- Added `core_type_id` to artifact type exports. `local_type_id` now denotes
  the source-level type view, while `core_type_id` denotes the local core shape
  representative.
- Updated artifact text format to `A_PROGRAM_ARTIFACT 20`.
- Updated artifact append/key recomputation so appended type exports recompute
  `core_type_id` in the target type declaration database.
- Updated `read_file.out` interface diagnostics to print both `local_type#...`
  and `core_type#...`.
- Extended the `Bool`/`Two` regression to assert that the two source views keep
  distinct constructor labels while sharing the same `core_type_id`.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 23:26 JST

- Changed `prototype_term_type_instance_make(...)` to build ordinary `APP`
  spines instead of `APP(TYPE_FORMER, TELESCOPE)`.
- Changed `prototype_term_type_instance_info(...)` to read both the new ordinary
  `APP` spine and the old legacy `TELESCOPE` representation.
- Removed the now-unused `TELESCOPE_NIL` construction helper.
- Updated artifact-flow expectations that depended on zero-argument type
  instances printing as `APP(TYPE_FORMER(...), TELESCOPE_NIL)`.
- Verified that generated standard example output and a generated `List`
  artifact no longer contain `TELESCOPE`.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/09_list_induction.p` with
  `read_file.out`.

### 2026-07-04 23:47 JST

- Added `PROTOTYPE_TERM_TYPE_VIEW` as an explicit wrapper over a structural
  core type-instance spine and a source type-instance spine:
  `TYPE_VIEW(view_type_id, core, source)`.
- Changed `prototype_term_type_instance_make(...)` so the core spine uses the
  structural `core_type_id` and core-erased type arguments, while the source
  spine preserves the source view and its arguments.
- Changed `prototype_term_type_instance_info(...)` to read source arguments
  from `TYPE_VIEW.source`. This keeps constructor classifier materialization
  source-typed, so `List Nat.cons` expects `Nat` rather than the raw core
  `#.Nat` shape.
- Updated substitution so substituted `TYPE_VIEW` nodes are rebuilt from their
  source spine. This prevents beta/match reduction from leaking source views
  into the core spine.
- Updated printing, artifact serialization, relocation, dependency traversal,
  and external-ref reachability for the new `source` field.
- Updated artifact text format to `A_PROGRAM_ARTIFACT 21`.
- Updated artifact-flow checks for the new term counts and dependent-match
  debug output.
- Verified `make reader`.
- Verified `sh src/prototype/test_artifact_flow.sh`.
- Verified examples `01_bool`, `03_main`, `04_match`, `05_bool_to_nat`,
  `06_pred`, `07_add`, and `09_list_induction` with `read_file.out`.
- Verified `examples/standard/Bool.p`, `examples/standard/Nat.p`,
  `examples/standard/List.p`, and `examples/11_maybe.p` with `read_file.out`.

### 2026-07-04 23:50 JST

- Audited this document against the current `TYPE_VIEW` implementation.
- Updated the current-state section so it describes
  `TYPE_VIEW(view_type_id, core, source)` rather than raw `TYPE_FORMER(...)`
  type instances.
- Added an implementation status checklist for the six required work items.
- Clarified that this refactor's completion does not include implicit
  source-view interchangeability. Equality, transport, casts, and relabeling
  remain separate future work.
- Clarified that `TYPE_VIEW` is a source annotation/view layer in the term
  graph, not a lambda-core computation node.

### 2026-07-04 23:52 JST

- Added an artifact-flow regression check that newly generated
  `examples/09_list_induction.p` output does not contain `TELESCOPE`.
- Re-verified `make reader`.
- Re-verified `sh src/prototype/test_artifact_flow.sh`.
- Re-verified examples `01_bool`, `03_main`, `04_match`, `05_bool_to_nat`,
  `06_pred`, `07_add`, `09_list_induction`, `standard/Bool`,
  `standard/Nat`, `standard/List`, and `11_maybe` with `read_file.out`.
