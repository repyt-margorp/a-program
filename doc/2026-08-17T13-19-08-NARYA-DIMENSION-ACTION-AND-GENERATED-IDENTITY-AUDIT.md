# Narya Dimension Action and Generated Identity Audit

Date: 2026-08-17

Status: problem audit and migration boundary, not an implementation plan

## Baselines

- A Program commit:
  `ebe72297f7ce664be85dcfefaf698f50dcebaeb7`
- A Program artifact schema: `v77`
- Narya repository: <https://github.com/gwaithimirdain/narya>
- Narya commit inspected:
  `7bf7fb88d626b432dcfa1519985a1a604c1b477d`
- Narya documentation:
  - <https://narya.readthedocs.io/en/latest/observational.html>
  - <https://narya.readthedocs.io/en/latest/hott.html>

The Narya checkout used for this audit is under the locally ignored `temp/`
directory. It is not part of the A Program repository and must not be
committed.

## Question Under Review

The current A Program implementation computes the identity of an ADT by
creating another indexed type declaration. Applying identity again creates a
further indexed type declaration whose indices encode a square boundary. The
main question is whether this on-demand hierarchy should remain the semantic
representation of higher identity, or whether a Narya-like dimension action
should replace it.

The requested direction is conservative:

- keep source IADTs and their existing constructor semantics;
- do not attempt to identify all IADT machinery with HOTT machinery at once;
- stop promoting every demanded identity dimension into a newly authoritative
  type declaration and constructor schema;
- preserve A Program's stable `BindingId` design rather than importing
  Narya's De Bruijn representation;
- keep generated Context data when a checker genuinely needs concrete scoped
  assumptions, but do not confuse that workspace with type-declaration
  authority.

## Executive Conclusion

The concern is valid.

The defect is **not** merely that A Program creates Term or Context nodes while
computing identity. A graph-based compiler may memoize derived Terms, and a
dependent checker may need a concrete Context containing endpoints, faces, and
witness variables.

The structural problem is more specific:

> A Program currently represents a derived dimension action by allocating a
> full `GENERATED_IDENTITY` entry in `TypeDeclarationDB`, publishing generated
> constructor schemas, storing that declaration ID in identity certificates,
> and exposing the representation in artifact `v77`.

This turns a computation over a source type into another nominal semantic type
declaration. Iteration then recursively creates a hierarchy of declarations:

```text
source ADT
  -> generated two-index identity IADT
  -> generated eight-index square IADT
  -> further dimensions require further generated declarations
```

The first two levels are not represented by one generic operation. They are
separate algorithms with fixed arrays and fixed face layouts. This is already
visible in the `2` and `8` index counts and the `9` generated field entries per
source field.

Narya makes a different semantic separation:

- a canonical datatype keeps its identity;
- terms and types are acted on by dimension operators;
- boundaries are represented by cube/tube structures;
- a higher Context may still be concretely generated for checking;
- that generated Context is not a new global datatype declaration.

Therefore the appropriate A Program target is not "never materialize
anything". It is:

> Make dimension action and boundary data first-class derived structure, while
> keeping source/import type declarations as the only declaration authority.
> Concrete acted Contexts may be cached checking workspaces, but generated
> identity declarations must cease to be the semantic representation.

## Current A Program Representation

### 1. TypeDeclarationDB has two incompatible roles

[`type_declaration.h`](../src/prototype/include/a_program/kernel/type_declaration.h)
states that type declarations are source-derived formation metadata providing
the named type view. The same structure nevertheless has two origins:

```c
PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE
PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY
```

This means one database is currently authoritative for both:

1. source/import declarations with names and constructor identity; and
2. compiler-derived results of applying identity to a carrier.

Those are different semantic categories. A generated identity family is a
computation from a source schema plus a dimension boundary. It is not another
source declaration.

### 2. Generated identity allocation mutates semantic declaration state

[`type_declaration.c`](../src/prototype/src/kernel/type_declaration.c) function
`prototype_type_declaration_add_generated_identity` interns by:

```text
(source_carrier_term_id, parameter_context_id)
```

If no entry exists, it calls the ordinary declaration allocator, changes the
origin to `GENERATED_IDENTITY`, records the source carrier, and marks a semantic
database revision.

Therefore this does not allocate one declaration for every textual occurrence
of Equality. Equal requests under the same interning key reuse an entry. The
problem remains because the cache key includes a concrete Context and a cache
miss promotes a derived result into semantic declaration state.

Consequences:

- requesting identity changes `type_count`;
- the result depends on a concrete `ContextId`;
- derived checking work enters the same authority domain as source types;
- request order and allocation order can affect physical declaration IDs;
- artifact code must understand and replay compiler-generated declarations.

There are also two related identities in play. An action request is keyed by a
source Claim and source bridge, while a generated declaration is interned by a
source carrier Term and parameter Context. The implementation must maintain the
correspondence between these two key spaces even though both describe aspects
of the same derived action.

### 3. One-dimensional identity is a generated two-index IADT

[`identity_computation.inc`](../src/prototype/src/identity/identity_computation.inc)
function `hott_initialize_generated_identity_declaration`:

1. creates fresh left and right bindings;
2. extends `ContextDB` twice;
3. constructs a classifier equivalent to
   `source_carrier -> source_carrier -> Universe`;
4. writes `index_count = 2` into the generated declaration.

For an ordinary ADT constructor, its identity constructor is then published as
another ordinary constructor declaration. For every source field, the simple
path allocates:

```text
left field value
right field value
identity witness between those values
```

as three successive Context extensions.

This is a reasonable *view* of a one-dimensional acted constructor telescope.
The problem is that the view is installed as a new authoritative declaration.

### 4. Two-dimensional identity is a separate fixed square algorithm

The same file's `hott_initialize_indexed_higher_identity_declaration` only
accepts a source that is already a generated two-index identity declaration.
It then creates a new declaration with exactly eight boundary indices.

Its implementation contains fixed representation sizes:

```text
bindings[8]
variables[8]
index_contexts[8]
classifiers[8]
field_faces[64][9]
field_bindings[9]
field_classifiers[9]
readback_fields[576]
```

The eight indices represent the boundary of a square. Each ordinary source
field is expanded to nine entries: four corner values, four edge identities,
and one square witness.

This proves that the current implementation does not have a dimension-generic
action. It has:

- a dedicated one-dimensional declaration generator; and
- a dedicated two-dimensional declaration generator.

Adding dimension three by the same technique would require another fixed
boundary layout rather than instantiating one general operator.

### 5. Dependent fields expose the limit of schema regeneration

The simple ordinary generated-constructor path rejects a field classifier that
contains a previous field binding. A newer contextual one-dimensional path can
handle dependent fields by first constructing an object-identity bridge for
the complete source constructor Context and then publishing that bridge Context
as the generated constructor telescope. This is real progress, but it remains
coupled to a generated declaration. The square path still rejects original
fields that depend on preceding fields.

Thus the generated-schema approach handles nondependent field products more
directly, needs a second physical-Context path for dependent fields in the
first dimension, and still cannot carry that dependency uniformly into the
next dimension. This blocks the needed direction for Sigma-like types, IADTs,
and higher dependent boundaries.

A generic telescope action should act on each field classifier under the
already acted prefix. Reconstructing a flat generated schema from the outside
instead forces every dimension to rediscover and republish the dependency
structure.

### 6. Context bridges recursively materialize endpoint/witness Contexts

`prototype_hott_bridge_db_ensure_identity_context` recursively walks the
source Context parent chain. For every extension, it requests identity type
computation and constructs an object-identity bridge containing:

- the generated bridge Context;
- a left endpoint substitution;
- a right endpoint substitution;
- the computed identity fiber and witness Claim;
- Context and substitution certificates.

This part is not automatically wrong. A checker needs scoped variables and
ordinary Claims in order to verify dependent actions. The problem is the
coupling:

```text
Context action
  -> identity computation certificate
  -> generated TypeDeclarationId
  -> generated constructor schema
```

The Context bridge cannot currently be understood independently as a derived
dimensional view of the original Context.

### 7. Certificates retain the generated declaration as identity

`prototype_hott_identity_type_computation_certificate` contains:

```c
uint32_t generated_type_declaration_id;
uint32_t backing_type_former_term_id;
uint32_t identity_type_term_id;
```

The certificate therefore identifies a successful action partly by the
generated declaration. It does not merely certify:

```text
source type + dimension operator + boundary -> acted family
```

This makes it difficult to remove generated schemas later, because proof
replay, validators, tests, and artifact publication all follow that ID.

### 8. Artifact v77 exposes the physical strategy

[`artifact_v77.schema`](../src/prototype/spec/artifact_v77.schema) requires an
identity root computation to replay from the source carrier and generated
declaration. Type declarations serialize their origin and generated source
carrier. The schema explicitly discusses the generated eight-index square
family.

This is no longer a private cache representation. It is part of the wire
contract.

Consequences:

- replacing the generated hierarchy requires an artifact version change;
- declaration allocation details can leak into reproducibility;
- a consumer must implement A Program's current physical generation strategy,
  not just its identity computation rules;
- dimensions beyond the hard-coded fragment have no representation-independent
  wire form.

### 9. Tests currently lock in the physical representation

The HOTT test `pi_identity.inc` requires the square result to have:

```text
computation_rule == INDEXED_HIGHER_LIFT
generated declaration index_count == 8
generated declaration constructor_count == 2
```

That is a representation assertion, not merely a semantic assertion. A future
generic dimension action could compute the same square and fail this test
solely because it correctly stopped creating a generated declaration.

### 10. The dependency is broad enough to require a planned migration

At the audited baseline, generated identity origin/declaration APIs and fields
occur 133 times across 20 prototype files. The identity implementation and
headers contain about 17,895 lines.

The main concentration is in:

- identity computation;
- object term action;
- action certificate validation;
- Context bridge construction;
- generated schema validation;
- artifact closure, wire read/write, and interface validation;
- permanent HOTT tests.

This is not evidence that all those lines are wrong. It shows that deleting
one enum case is not a sufficient migration.

## What Narya Actually Does

Narya must not be treated as a finished or directly portable specification.
Its documentation explicitly says that the combined multi-direction theory is
not fully realized and that HOTT transport/lifting computation is incomplete
for several canonical forms.

Nevertheless, its representation provides a useful separation missing in A
Program.

### 1. Dimension theory is centralized

`lib/dim/README.md` states that the category of dimensions, faces, and
degeneracy operators is centralized and abstract outside `lib/dim`.

Its cube and tube structures are indexed by faces. The implementation avoids
hard-coding a face count or external face ordering into each consumer.

This contrasts with A Program's local arrays of sizes `8` and `9` in the
identity declaration generator.

### 2. Action is a generic internal operation

Narya's checked internal term has an `Act` form carrying:

```text
term
degeneracy operator
term classification information
```

`act.mli` exposes generic action over values, types, normal forms, evaluations,
and cubes. The action is parameterized by the operator's domain and codomain
dimensions.

The important abstraction is not the literal OCaml constructor. It is that
dimension change is represented once as an operation, rather than as a new
datatype declaration generator for each dimension.

### 3. Canonical datatype identity is stable under action

Narya's canonical `Data` value keeps:

- its dimension;
- the original type family;
- its indices as cubes;
- its constructors as closures;
- recursion and discreteness information.

`act_canonical` acts on the type family, index cubes, and constructor closures,
then returns `Data` again. Its implementation comment explicitly distinguishes
acting on arguments from changing canonical type identity.

In semantic terms:

```text
act(Data D, operator)
```

is an acted view of `D`, not a newly registered declaration `D_identity_2`.

### 4. Higher boundaries are tube data

Narya represents instantiation of a higher-dimensional type by a boundary
tube. A higher type can be partially instantiated only at a valid boundary
shape. Dimension and boundary consistency are carried by the data structures.

A Program currently obtains comparable information by constructing a sequence
of endpoint/relation Context entries and then baking that sequence into a
generated IADT schema. The first operation may still be needed; the second is
not.

### 5. Higher Contexts are still concretely generated

This is a necessary correction to an overly strong comparison.

Narya's checked Context contains cubes of variables. `degctx.ml` explicitly
says that constructing a degenerated Context requires an evaluation/readback
cycle because the result contains more level variables. It then builds a new
Context and environment.

Therefore Narya does **not** show that all Context materialization should be
abolished.

It shows a narrower and more relevant invariant:

> Context materialization is an application of a general dimension/cotensor
> operation. It does not register a new semantic datatype declaration for each
> identity occurrence.

### 6. Higher identity is iteration of one structure

Narya documents:

```text
Id A
Id (Id A)
Id (Id (Id A))
```

as iterated higher-dimensional types. Cube boundaries and arbitrary
degeneracies emerge from the common dimensional structure. Its generic
degeneracy syntax combines reflexivity and symmetry without adding a new
type-declaration origin for each result.

For datatypes, `Id` computes to a higher version of the same datatype whose
endpoints behave as indices. This confirms the useful intuition behind A
Program's generated constructor telescopes. It does **not** imply that those
telescopes must become global declarations.

## Comparison

| Concern | Current A Program | Narya lesson | Conservative A Program target |
| --- | --- | --- | --- |
| Source datatype identity | `SOURCE` declaration | canonical `Data` remains stable | retain one source/import schema |
| Identity of datatype | new `GENERATED_IDENTITY` declaration | action on canonical data | derived acted-type view |
| Higher dimensions | separate fixed one/square generators | generic dimension operators | dimension-generic action |
| Boundary representation | flattened Context chain plus fixed arrays | cube/tube indexed by faces | abstract boundary object using `BindingId` references |
| Context generation | persistent bridge Context tied to generated type | degenerated Context is still built | allow rebuildable checking Context view |
| Constructor action | generated constructor declaration | act on constructor closure/telescope | acted constructor view over source constructor ID |
| Proof certificate | stores generated declaration ID | action carries operator and acted data | certify source, operator, boundary, output Claim |
| Artifact | serializes generated declaration origin | not copied directly | serialize semantic action/evidence, not cache schema |
| Binding representation | stable `BindingId` | intrinsically scoped De Bruijn indices | retain `BindingId`; copy only semantic separation |
| IADT implementation | TypeDeclarationDB schema | canonical data/index structures | retain source IADT; share telescope action only |

## Precise Problems to Fix

### P0. Derived action is promoted into declaration authority

`TypeDeclarationDB` should answer which source/import type and constructors a
program denotes. It should not grow because the checker asks for another
identity dimension.

Required invariant:

```text
requesting identity or a higher action does not change semantic type_count
```

### P0. Dimension is encoded by recursion through generated declarations

The second-dimensional algorithm recognizes the first generated declaration
and creates another. This is a representation-level recursion, not a generic
dimension operation.

Required invariant:

```text
the same action API handles dimension n -> m without a case named for each n
```

The dimension category need not initially be as general as Narya's. A Program
may begin with its required binary HOTT direction. It must still avoid encoding
dimension in fixed declaration layouts.

### P0. Artifact authority contains the cache strategy

Artifact replay should validate ordinary Terms, Claims, Derivations, and the
semantic identity action. It should not require the producer's generated
declaration allocation.

Required invariant:

```text
two executions that derive the same acted family serialize the same semantic
evidence even if their scratch Context and cache IDs differ
```

### P1. Acted Context and generated schema are inseparable

The current bridge is useful proof infrastructure, but it is coupled to an
identity certificate whose output is a generated declaration.

Required split:

```text
ContextAction(source_context, dimension_operator)
  -> boundary/context view + endpoint substitutions

TypeAction(source_type, dimension_operator, boundary)
  -> acted family Term + Claims
```

The first may allocate ContextDB entries. Neither operation should allocate a
source-level type declaration.

### P1. Constructor action does not scale to dependent telescopes

Flat expansion works for products of independent fields. A dependent field
must be acted on under the complete acted prefix.

Required rule:

```text
act((Gamma, x : A), d)
  = extend(act(Gamma, d), acted cube of x classified by act(A, d))
```

The exact boundary variables depend on the selected dimension operator. The
operation must consume the original telescope incrementally rather than first
requiring a generated declaration schema.

### P1. Certificates use physical generated IDs

The identity certificate must eventually replace
`generated_type_declaration_id` with semantic inputs and outputs such as:

- source type/schema identity;
- dimension operator identity;
- boundary/tube identity;
- acted family Term and classifier Claims;
- dependencies on Context-action and telescope-action certificates.

The final field set must follow the chosen operator model. These are required
semantic roles, not a demand for these exact C structs.

### P1. Tests assert the obsolete representation

Tests should assert:

- the acted family has the expected boundary;
- constructor action yields the expected witness;
- replay succeeds without mutable action state;
- applying action repeatedly reaches dimension two and higher;
- source declaration count remains unchanged;
- allocation/request order does not alter publication identity.

They should not require a generated declaration with exactly eight indices.

### P2. Terminology hides three different mechanisms

Current code and documents use nearby words for:

1. compiler-local parametricity relation materialization;
2. object identity type computation;
3. Context bridge construction.

These should remain semantically distinct even if they share a dimension-action
library. In particular, a parametricity relation result must not become an
object identity Claim merely because both use endpoint Contexts.

## What Must Not Be Removed

### Source IADT schemas

`TypeDeclarationDB` remains necessary for generative indexed ADTs, source type
views, constructor ordinals, and source/import identity. This audit does not
propose encoding those declarations away.

### ContextDB and SubstitutionDB

Dependent checking needs scoped assumptions and substitutions. A generic
dimension action may still generate a concrete Context path. The generated
path should be treated as derived, replayable workspace rather than a type
declaration.

### Ordinary object proof evidence

Identity inhabitants must remain ordinary Terms with Claims and Derivation
DAGs. Removing generated declarations must not replace proof objects with a C
boolean or an opaque HOTT action result.

### Term graph memoization

Derived acted Terms may be interned in `TermDB`. A Program intentionally uses
the graph as its computation substrate. The prohibited promotion is from
derived computation to nominal declaration authority, not from computation to
memoized Term nodes.

### Separation of parametricity and object identity

The existing distinction between compiler-local relation action and object
identity is conceptually correct. A shared dimension engine must preserve the
different proof rules and authority boundaries.

## Conservative Target Model

The following model is sufficient to remove the immediate defect without
committing A Program to Narya's full implementation architecture.

### 1. Stable source schema

One source/import declaration remains authoritative:

```text
SourceTypeSchema {
  declaration identity
  parameter telescope
  index telescope
  constructor telescopes
}
```

No identity request creates another `SourceTypeSchema`.

### 2. Dimension operator

Introduce one semantic representation for a dimension action. It must at least
support:

- identity operator;
- one degeneracy/reflexivity direction;
- iteration to squares and higher dimensions;
- face selection required by endpoint substitutions;
- composition sufficient for repeated action.

The representation may initially be restricted to the one binary HOTT
direction. It must not use an enum case or a fixed array layout per dimension.

Whether this becomes a generic Term tag, an identity-action arena referenced by
Terms, or a canonical APP encoding is a later implementation decision. The
semantic operation must be explicit somewhere; reconstructing it from a
generated declaration shape is not acceptable.

### 3. Boundary/tube object

A boundary object should map abstract faces to existing Term/Binding/Claim
identities. It should:

- use stable `BindingId` references;
- validate shared subfaces once;
- avoid embedding one global numeric face ordering in every caller;
- support partial instantiation where A Program needs it;
- be usable by type, constructor, Context, and term action.

This can be a C data structure without duplicating Narya's OCaml GADTs.

### 4. Derived Context action

```text
ContextAction(Gamma, d)
  -> acted_context_view
```

The view contains concrete ContextDB entries and substitutions when required by
the current kernel. Its identity is semantic in `(Gamma, d, boundary)`, not the
allocation IDs of the generated entries.

It must be possible to discard and reconstruct the view without changing the
source schema or artifact meaning.

### 5. Derived telescope/constructor action

```text
ConstructorAction(source_constructor_id, d, boundary)
  -> acted telescope view + result family + Claims
```

It traverses the original constructor telescope under the acted prefix. The
source constructor ID remains stable. No generated constructor declaration is
published.

### 6. Type action result

```text
TypeAction(source_type_term, d, boundary)
  -> acted family Term
```

The result is a normal object-language Term with formation Claims. It may refer
to a derived acted-type view needed by constructor and Match checking, but that
view is not a source declaration.

### 7. Semantic certificate

An action certificate should connect:

```text
source Claim
dimension operator
boundary/context action
acted output Term
output formation Claim
ordinary proof dependencies
```

It must remain replayable without the mutable action work queue and without a
generated `TypeDeclarationId`.

### 8. Artifact vNext

This change is not compatible with artifact `v77`'s generated-declaration
contract. A new artifact version should:

- serialize source/import declarations only as type declarations;
- serialize the dimension operator and boundary/action evidence needed by
  identity roots;
- serialize ordinary result Terms, Claims, and Derivations;
- omit scratch Context/action cache IDs;
- replay the action against the original source schema;
- reject roots that depend only on unpersisted compiler work.

Backward compatibility should not be retained by keeping both authority paths.
Old artifacts should be rejected by version when the migration lands.

## Required Invariants Before Implementation

1. `TypeDeclarationDB` contains only source/import declaration authority.
2. Computing any identity dimension does not increment semantic declaration
   count.
3. Repeated action uses one operator model rather than recursively recognizing
   generated declaration origins.
4. An acted constructor is identified by source constructor identity plus its
   action inputs, not by a generated constructor ID.
5. Concrete acted Contexts are disposable and replayable caches/workspaces.
6. Artifact identity is independent of Context, Term, Claim, and action request
   allocation order except where those IDs are explicitly dense wire-local
   references.
7. Source `BindingId` identity remains the basis of binder correspondence; no
   De Bruijn migration is introduced.
8. No maximum identity dimension is encoded by a fixed enum case, index count,
   or face array.
9. Dependent constructor fields are acted under the transformed preceding
   telescope, not rejected merely for mentioning a prior field.
10. Compiler-local parametricity relations and object identity Claims remain
    distinct authorities.
11. Definitional equality is not extended from arbitrary object identity
    proofs.
12. Ordinary proof Terms and accepted Derivation DAGs remain the final evidence
    for identity inhabitants.

## Migration Hazards

### Ordinary typing currently expects generated type views

Generated identity Terms are deliberately represented through ordinary
`TYPE_FORMER`, `TYPE_VIEW`, `TYPE_INSTANCE`, constructor, APP, and Match rules.
Removing the declaration cannot leave these rules without a classifier source.
The acted-type view must provide equivalent read-only schema queries.

### Match and constructor validation query generated origins

Generated identity recognition is spread through identity validation and
artifact replay. Every query must be classified as either:

- source declaration identity;
- acted schema/view lookup;
- ordinary Term/Claim validation; or
- obsolete generated-origin logic.

Blindly deleting the origin enum before this classification would weaken the
kernel.

### Artifact v77 cannot be silently reinterpreted

Its documented authority is the generated declaration and Context telescope.
The new representation needs a new wire version rather than a fallback reader
that reconstructs either model.

### Existing permanent tests include useful semantics

The tests for Bool, recursive constructors, pointwise Pi identity, object term
action, and identity-of-identity contain valuable proof boundaries. They should
be rewritten to use semantic action queries, not discarded with the generated
schema assertions.

### Narya's incompleteness must remain visible

Narya documents incomplete transport/lifting computation and an unrealized
multi-direction architecture. A Program must define its own admitted fragment,
residual outcomes, and artifact claims. Adopting its representation lesson does
not justify claiming complete HOTT.

## Proposed Order for a Separate Implementation Plan

This audit does not authorize code changes. A follow-up implementation plan
should use the following gates.

### Gate D0: semantic specification

- define A Program's initial dimension/operator algebra;
- define face and boundary identity without fixed dimension-specific arrays;
- define action composition and identity laws;
- state which HOTT computations are admitted and which remain residual.

### Gate D1: derived views beside the old path

- implement dimension and boundary data structures;
- implement Context and telescope action as derived views;
- validate first-dimensional ordinary ADT action without allocating a generated
  declaration;
- compare proof Terms and Claims against the old path in tests only.

This temporary comparison is not backward-compatible production authority. It
is a migration oracle and must be deleted after equivalence tests pass.

### Gate D2: dimension-generic iteration

- produce the current square examples through repeated generic action;
- remove fixed `8`-index and `9`-field construction from the active path;
- add a dimension-three structural test that does not require a complete HOTT
  feature at that dimension, only correct boundary/action composition.

### Gate D3: certificate and kernel query migration

- replace generated declaration IDs in action certificates;
- migrate constructor, Match, Context bridge, and validator queries to acted
  views;
- make source declaration count invariant under HOTT actions;
- remove generated-origin interning.

### Gate D4: artifact vNext

- specify and implement semantic action publication;
- replay without action work queues or scratch Context identity;
- migrate permanent artifact tests;
- reject v77 rather than retaining dual identity authority.

### Gate D5: deletion and audit

- delete `GENERATED_IDENTITY` declaration origin and its validators;
- delete fixed `INDEXED_HIGHER_LIFT` schema generation;
- remove tests asserting generated declaration counts/layouts;
- report per-file additions and deletions;
- run the full prototype and artifact suites;
- perform an authority audit proving that no generated dimensional schema can
  enter `TypeDeclarationDB`.

## Acceptance Tests for the Future Plan

At minimum, the migration should permanently test:

1. Bool identity and constructor witnesses without declaration growth.
2. Recursive Nat/List identity through source constructor action.
3. A dependent constructor telescope whose later field mentions an earlier
   field.
4. An indexed ADT whose result indices differ by constructor.
5. Pointwise Pi identity and non-DefEq observational witness examples already
   admitted by the project.
6. Identity-of-identity square construction by repeated generic action.
7. A structural dimension-three boundary construction with no fixed face
   arrays in the caller.
8. Context action cache deletion followed by deterministic reconstruction.
9. Different request orders yielding equivalent artifact publication.
10. Rejection of a parametricity relation certificate where object identity is
    required.
11. Rejection of an identity proof as a new DefEq rule.
12. Artifact readback that reconstructs source declarations and validates
    acted identity evidence without generated declarations.

## Decision Record

### Accepted

- The current generated-identity hierarchy is a design debt.
- Source IADTs remain first-class and are not collapsed into HOTT.
- Dimension action should become generic and independent of generated type
  declarations.
- Concrete Context materialization may remain where dependent checking needs
  it.
- Such Contexts are derived checking views, not declaration or artifact
  authority.
- A Program keeps stable `BindingId` references and does not copy Narya's De
  Bruijn implementation.
- Artifact migration is intentionally breaking; dual authority is not retained.

### Rejected

- "Narya never materializes higher Contexts."
  This is false; its `degctx` path explicitly builds them.
- "Delete ContextDB and represent all faces only as Terms."
  This loses the scoped dependent-checking structure currently certified by
  Context and substitution evidence.
- "Keep generated identity declarations as an artifact compatibility cache."
  This preserves the authority ambiguity the migration is intended to remove.
- "Encode every HOTT rule into APP/LAMBDA to avoid a dimension abstraction."
  This hides, rather than removes, the boundary and operator semantics.
- "Copy Narya's implementation wholesale."
  Its language, binding representation, supported fragment, and unfinished
  areas differ from A Program.

## Final Assessment

The current generated IADT representation was a productive scaffold. It made
one-dimensional ADT identity and a selected square fragment checkable through
ordinary Terms, Contexts, Claims, and constructor rules. That evidence should
be retained.

It has now reached its architectural limit. The source declaration database,
derived dimension action, concrete checking Context, and artifact evidence are
four distinct responsibilities. The current implementation collapses all four
around `generated_type_declaration_id`.

The next refactoring should separate those responsibilities before extending
general dependent identity or dimensions above two. Otherwise each theoretical
advance will add another generated schema layer, another fixed boundary
encoding, and another artifact exception.

## Privacy Review

This document contains repository-relative paths, public upstream URLs, and
public Git commit identifiers only. It contains no personal names, email
addresses, credentials, tokens, private URLs, or absolute home-directory paths.
