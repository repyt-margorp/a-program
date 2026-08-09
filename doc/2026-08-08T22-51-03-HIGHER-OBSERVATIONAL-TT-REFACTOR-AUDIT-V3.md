# Higher Observational Type Theory Refactor Audit V3

Date: 2026-08-08

Status: active; V2-P1, V3-G1 graph-consing, compiler-local V2-O1
type-directed observational action, and V3-SC1 semantic consolidation are
complete. V3-PC1 persistent Proposition-reference normalization is complete;
V2-A1 artifact publication is the next
stage, followed by V2-A1 artifact publication. The post-G1 O1 audit found one pullback-interning correction
and one Type-action endpoint-context correction; both were completed before
non-empty bridge construction.
Local identity, artifact-link comparison, kernel conversion, immutable
observation goals, and compiler-local candidate search have separate semantic
ownership. V3-G1 has repaired the former Context/Substitution/Judgement
interning and adjacency split. O1.1 replaced the former direct-mapped pullback
memo with the general interned comprehension action without reopening the whole
G1 migration.

This V3 document copies V2 in full as historical evidence and updates its active
plan against the implementation at commit `942bed7`. Requirements taken
from published work are distinguished from A Program-specific design
hypotheses. The latter must be validated as part of the project calculus rather
than treated as facts inherited from an external theory.

Current implementation direction (2026-08-08): V2-P0 is complete at commit
`4025532` and artifact v67. Solver-local obligations are separated from closed,
authority-complete Claims; one Claim may have multiple Derivations; and link and
Universe provenance name exact accepted Claims. The executed P0 plan is
recorded in
`doc/2026-08-08T17-10-35-V2-P0-REMAINING-IMPLEMENTATION-PLAN.md`.
The P1 re-audit found a mandatory entry refactor before observational payload
selection. V2-P1-R0 now grounds HOTT goals in exact accepted Claims, separates
substitution certification from structural substitutions, binds conversion
results to exact requests, and removes solver-local wire authority in artifact
v68. Its completed implementation and exit audit are tracked in
`doc/2026-08-08T20-00-00-V2-P1-ENTRY-REFACTOR-AUDIT-PLAN.md`.
V2-P1 then split immutable observation identity, typed rule candidates, mutable
work state, and residual diagnostics; froze exact action contracts and a
dedicated HOTT semantic fingerprint; and selected the replay-minimal future
accepted payload without changing artifact v68.

The post-P1 audit found a new pre-O1 requirement. Context extension is already
a persistent parent graph and Substitution is already an expression DAG, but
their interning is linear, reindex materializes dense binding views repeatedly,
generated motive contexts allocate fresh equivalent pullbacks, and accepted
Derivations retain copied scoped-premise tuples. V3-G1 applies one explicit
graph-consing discipline to Context, Substitution, and Judgement while retaining
their distinct categorical sorts and the one shared TermDB.

V3-G1 was completed at commit `575d6c0`. The detailed V2-O1 implementation
audit and progress plan is
`doc/2026-08-09T07-10-00-V2-O1-OBSERVATIONAL-ACTION-IMPLEMENTATION-PLAN.md`.
It corrects the Type-action contract to include the two-endpoint Context,
separates immutable action requests from generated results, and requires the
remaining direct-mapped pullback cache to become a true interned CwF action
before relational Contexts are generated.

## 1. Baseline

The inherited historical audit begins at the following repository state:

- branch: `main`;
- Git commit: `474867ea31331bcf93821f9bf106184602715e58`;
- short commit: `474867e`;
- commit date: `2026-08-06T23:04:09+09:00`;
- commit subject: `refactor: structure kernel conversion outcomes`;
- remote state at audit time: `HEAD`, `origin/main`, and `origin/HEAD` all
  point to `474867e`;
- artifact format: `A_PROGRAM_ARTIFACT 62`;
- verification-obligation schema: version `1` for
  `PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT`.

The active V3 delta is pinned to:

- branch: `main`;
- Git commit: `942bed77e6d825b9b9965b480ea1faf80afcc737`;
- artifact format at the pinned baseline: v68;
- current local artifact format after V3-G1.1: v69;
- prototype test scripts: 16.

The current O1 planning baseline is:

- branch: `main`;
- Git commit: `575d6c0`;
- artifact format: v69;
- prototype test scripts: 16.

The artifact writer emits version 61 at `src/prototype/ast.c:7368`, and the
reader accepts only version 61 at `src/prototype/ast.c:7694-7699`. The existing
test deliberately rejects version 60.

The worktree was clean before this V2 document was created. No HOTT object-term
implementation is present in artifact v61.

## 2. Baseline Verification

The following checks passed against the V2 baseline:

- `make`;
- `test_artifact_flow.sh`;
- `test_cbpv_boundary.sh`;
- `test_cbpv_surface.sh`;
- `test_computation_block_sequence.sh`;
- `test_constructor_polarity.sh`;
- `test_context_category.sh`;
- `test_conversion_result.sh`;
- `test_definition_block.sh`;
- `test_dependent_pi.sh`;
- `test_resumption_multiplicity.sh`;
- `test_shared_core_occurrences.sh`;
- `test_term_identity_frame.sh`.

All twelve `src/prototype/test_*.sh` scripts were run through `sh` and passed.

The conclusion is narrow: the existing regression suite passes. It does not
cover kernel conversion of unrelated or forged Match-frame IH references, and
it does not establish that v61 can represent or validate Higher Observational
equality.

## 3. What Artifact v61 Represents

Artifact v61 serializes the current boundaries:

1. exported typed-operation roots and classifiers;
2. sparse TermDB nodes;
3. TypeView identities and core representations;
4. ADT declarations and constructor telescopes;
5. OperationGraph source occurrences;
6. `HAS_TYPE` and `IS_TYPE` derivations;
7. conversion proof nodes whose conversion condition is recomputed by the
   current normalization kernel;
8. universe level constraints;
9. effect constraints and residual verification obligations.

It does not serialize any of the following HOTT commitments:

- the selected observational calculus or its version;
- operations on contexts, substitutions, types, and terms used by internal
  parametricity;
- a type-directed observational relation;
- higher equality witnesses or their levels;
- transport, symmetry, or coherence operations;
- observational rules for each type former;
- quotient or higher-constructor schemas;
- a semantic model and refinement contract for host primitives;
- a fingerprint identifying the kernel equality rules under which an artifact
  was checked.

Therefore HOTT support must not be encoded as optional v61 records. The first
accepted HOTT slice requires a breaking artifact migration. The provisional
next number is v62, but the writer must not emit v62 until the concrete calculus
and its serialized invariants are fixed.

## 4. Governing Separation

Three equalities must remain distinct.

### 4.1 Core representation equality

This is sharing or structural equality in TermDB. It supports graph reuse and
execution. It is not a typed proof that two source values are equal.

### 4.2 Definitional equality

This is the kernel conversion relation generated by the selected pure
reduction rules. It remains meta-level and must not be extended whenever a user
constructs an equality witness.

### 4.3 Higher observational equality

This is object-language, type-directed, proof-relevant equality. Its witnesses
are values and may themselves have non-trivial higher equality. It must not be
collapsed to proof-irrelevant legacy Observational Type Theory.

The current code has useful parts of the first two layers. It has no third
layer. Adding one generic `OBS_EQ` tag without the refactors below would mix all
three.

## 5. Critical Refactor 1: Freeze the Concrete Calculus First

### Current problem

`Higher Observational Type Theory` names a research direction, not one complete
drop-in kernel specification for A Program. The project still has to choose the
exact syntax and judgements for:

- observational equality at each dimension;
- heterogeneous equality over dependent families;
- context, substitution, type, and term actions;
- reflexivity, symmetry, transport, and composition;
- universe equality and univalence;
- quotient or higher-inductive formation and elimination;
- interaction with dependent CBPV and effects.

### Required change

Create a normative calculus document before adding C tags. It must provide:

1. grammar;
2. well-formedness judgements;
3. formation, introduction, elimination, and computation rules;
4. substitution laws;
5. observational equations for every admitted type former;
6. a clear boundary between judgemental conversion and object equality;
7. the finite initial fragment implemented by the first migration.

### Completion condition

Every proposed TermDB tag and proof rule must cite one rule in the calculus.
Prototype behavior must not become the source from which the theory is later
inferred.

## 6. Critical Refactor 2: Separate Hash-Consing from Shape Comparison

### Current implementation

Commit `056741f` separated local alpha interning, source/core/view projection
comparison, and cross-artifact link comparison. `add_term` now uses the private
`term_intern_alpha_equal_local` path. Cross-artifact comparison uses canonical
keys only as prefilters and validates the referenced Match graph.

The separation is incomplete at the kernel conversion boundary. The helper
`match_frame_keys_equal` at `src/prototype/term.c:6863-6886` treats two local
Match frames as equal from canonical hash and summary fields. IH conversion at
`src/prototype/term.c:7119-7136` calls this helper directly.

### HOTT risk

Higher witnesses can have the same endpoints while representing different
paths, squares, or higher cells. Interning must compare alpha-canonical syntax
and binding structure, including recursive Match-frame scope. A comparator
intended to erase local Match frames or link-local identities must never decide
TermDB node identity.

The remaining conversion path can establish equality between unrelated IH
references whose frame keys collide or happen to have the same summaries. A
linker lookup key is therefore still able to affect a kernel equality decision.
This already violates the intended kernel boundary before HOTT terms exist. A
future equality witness compared through this path could also be collapsed.

### Required change

Preserve the distinct APIs and prohibit cross-use:

- alpha-canonical TermDB interning equality under explicit binder and
  Match-frame environments;
- exact raw-record equality for diagnostics only;
- source-view equality;
- erased representation shape equality;
- cross-artifact canonical-key compatibility;
- kernel conversion;
- object observational equality.

`add_term` may continue to use only alpha-canonical interning equality. Kernel
conversion must gain its own explicit binder and Match-frame correspondence
environment. Comparing two Match terms establishes a scoped one-to-one frame
pair before comparing case bodies. Comparing IH terms consults that pair; an
unmapped local frame requires the same frame identity. It must never call
canonical-key construction or `match_frame_keys_equal`.

The conversion regression suite must cover isolated IH nodes, unrelated
enclosing Matches, nested recursive Matches, and a deliberately forged
canonical-key collision. The existing term-identity test does not exercise this
kernel conversion path.

### Artifact effect

Canonical keys may continue to support linking, but they must be marked as
representation keys. HOTT witnesses must be serialized by exact graph edges,
not reconstructed from canonical-key equivalence classes.

## 7. Critical Refactor 3: Add a Type-Directed Observational Kernel

### Current implementation

The current definitional conversion boundary has the effective shape:

```text
compare_for_conversion(profile, left, right, budget)
    -> structured conversion result
```

The public result in `src/prototype/term.h:277-308` distinguishes `EQUAL`,
`NOT_EQUAL`, `RESIDUAL`, `BLOCKED_EFFECT`, `EXHAUSTED`, and `INVALID`. The
classifier wrapper at `src/prototype/typing.c:673-726` invokes the pure-type
normalization profile. The recursive comparator still receives no typing
context and no carrier type. Constructor comparison ultimately uses
representation and ordinal data.

This structured API is a completed improvement to definitional conversion. It
does not make that comparator an observational equality engine. `RESIDUAL` also
has no producer yet and carries no resumable typed comparison problem.

### HOTT risk

Observational equality is intrinsically type-directed:

```text
Gamma |- x ~[A] y type
```

Equality at a Pi type is pointwise. Equality at a quotient is its specified
relation. Equality at an inductive type follows its observational interface.
Equality at a computation type cannot be inferred from an untyped comparison
of graph nodes.

The existing normalizer is appropriate for definitional conversion. It is not
an observational equality engine.

### Required change

Keep normalization conversion separate and add a type-directed HOTT service
whose inputs include:

- context;
- carrier type;
- left and right terms;
- observational dimension or level;
- the selected type-former semantics;
- a deterministic budget and residual result.

The service must construct or validate object terms. It must not merely return
`true` and it must not insert successful propositions into global DefEq.

## 8. Critical Refactor 4: Generalize Context and Substitution Operations

### Current implementation

`ContextDB` already models immutable value-context extension at
`src/prototype/context.h:13-29`. `SubstitutionDB` stores identity, empty,
projection, extension, and composition substitutions at
`src/prototype/context.h:31-59`.

An extension carries one term, its classifier, and one typing proof. V2-C1
moved reindexing into `src/prototype/context.c`; it implements substitution by
generating fresh binders and rebuilding shared TermDB graphs.

### Positive assessment

This is useful groundwork. It should not be removed, replaced by a HOTT-only
category, or embedded into ordinary TermDB nodes. It is the context CwF on
which both dependent typing and dependent CBPV are indexed. This categorical
indexing does not require separate value-side and computation-side copies of
TermDB constructors.

### Missing HOTT structure

The current API applies ordinary term substitution. Its implementation is now
owned by `src/prototype/context.c`, independently of AST lowering. V2-C1 fixed
that ownership issue; observational action and computation-specific naturality
laws are still absent.

HOTT's internal parametricity construction requires mutually coherent
operations on:

- contexts;
- substitutions;
- dependent types;
- terms;
- the observational witnesses generated for each of them.

Ordinary `prototype_term_reindex` does not state or validate those naturality
and coherence laws.

### Required change

Do not replace the existing CwF with one undifferentiated "HOTT category". The
A Program-specific working hypothesis is a four-part model:

1. a value CwF of contexts, substitutions, dependent value types, and values;
2. a family of computation categories indexed by value contexts and effect
   rows;
3. value/computation structure corresponding to `RETURN`/`Comp` and
   `THUNK`/`FORCE`, with explicit reindexing laws;
4. dimension-indexed observational actions on the admitted value and
   computation structures.

The first implementation fragment should define type-directed observational
equality over the one shared TermDB. It may admit ordinary values and a
precisely specified pure computation/function fragment through the existing
`Comp` and `Thunk` boundaries. Effectful computations, operation requests, and
handlers must initially return an explicit unsupported or residual result. It
must never fall back to raw graph comparison, and it must not introduce a
second value-side Pi, Lambda, APP, or Match syntax.

Add a separate observational/context-action layer over the value CwF:

- context observation or bridge construction;
- substitution action and composition law;
- dependent type action;
- dependent term action;
- reindexing of equality and higher witnesses;
- explicit proof that these operations respect identity and composition.

Before adding those actions, move the ContextDB/SubstitutionDB implementation
from `ast.c` into a dedicated prototype context implementation module. This is
an ownership refactor, not a semantic rewrite.

Do not overload `prototype_substitution` with an ambiguous mode bit. Ordinary
substitution and observational translation are related but distinct operations.

## 9. Critical Refactor 5: Establish One Type-Former Semantic Authority

### Current fragmentation

The implemented type formers are represented through different authorities:

- Pi is a primitive TermDB tag;
- ADTs use TypeDeclarationDB, ContextDB telescopes, and generated curried
  classifier caches;
- Universe variables use TermDB plus UniverseDB constraints;
- `Comp` and `Thunk` are primitive CBPV tags;
- host types and primitives use static C descriptors;
- effect operations use separate declaration tables.

The constructor telescope direction is already improved: semantic fields live
in ContextDB and source field expressions are documented as readback metadata
at `src/prototype/type_declaration.h:112-133`.

### HOTT risk

HOTT requires every admitted type former to define, at minimum:

1. equality between inhabitants;
2. equality between instances of the type former;
3. transport or cast computation;
4. action under substitution;
5. higher witness preservation.

Adding these rules to unrelated switches in `term.c`, `typing.c`, `ast.c`, and
`type_declaration.c` would duplicate the kernel semantics.

### Required change

Define a kernel-level type-former semantic interface. It need not be a runtime
function-pointer registry. A declarative tagged interface is sufficient, but it
must be authoritative for:

- formation;
- introduction and elimination;
- definitional computation;
- observational equality generation;
- transport;
- reindexing;
- artifact validation.

User ADTs should instantiate this interface from their authoritative telescope,
not from readback metadata or shape keys.

## 10. High Refactor 6: Complete Structured Conversion Outcomes

### Current implementation

The structured conversion migration is implemented. The result at
`src/prototype/term.h:277-308` retains status, reason, profile, observations,
budget use, and graph revision. Typing callers consume the structured result
and require `EQUAL` where conversion is mandatory.

The migration deliberately did not add residual conversion records to
JudgementDB or artifact v61. `RESIDUAL` currently has no producer. In addition,
the recursive implementation still threads incomplete status through internal
mutable outputs rather than a typed comparison stack.

### Problem

`not equal`, `budget exhausted`, `blocked`, and `invalid graph` must not become
the same kernel answer. This distinction becomes more important when HOTT
transport or observational reduction is residualized.

### Required change

Preserve the implemented status algebra:

```text
equal
not_equal
residual
exhausted
invalid
```

A future residual must carry the comparison problem, context, carrier, profile,
rule-set identity, transparency policy, steps used, and the boundary needed to
resume validation. Do not serialize the current transient result structure as a
residual obligation.

This does not require storing every successful DefEq comparison as a judgement
or object term.

## 11. High Refactor 7: Restore Typed-Occurrence Evidence Ownership

### Current implementation

JudgementDB has only `HAS_TYPE` and `IS_TYPE` conclusions at
`src/prototype/judgement.h:13-17`. This is not itself a defect: an equality
witness can be an ordinary TermDB term whose classifier is an equality type.

The immediate problem is not the physical proof record. Its conclusion subject
is a shared Core Term ID, while A Program assigns synthetic classifiers to
typed Operation occurrences. Distinct operations such as Bool and Nat identity
functions may intentionally share one erased core lambda.

The Operation classifier solver is already indexed by Operation ID. The loss of
identity occurs when solved Operation classifiers are materialized into
Term-indexed Judgement relations. Later code then recovers an occurrence by
searching Core Term, Context, classifier, and proof tuples.

JudgementDB also contains non-Operation facts: binder assumptions, declaration
facts, type formation, intrinsic typing, and universe constraint inputs. A
mechanical replacement of every Term subject by an Operation ID would therefore
be another category error.

The integer-literal implementation supplies a second constraint on the design.
The solver selects one classifier for an Operation, while an in-range literal
can have both Int64 and Int32 admissible typing relations. One synthetic solver
result must not be confused with one possible derived typing claim.

### HOTT risk

HOTT endpoints and witnesses must be checked at a typed occurrence or explicit
typed goal boundary. If operation typing remains Core-indexed, two distinct
surface occurrences can borrow each other's classifier before object equality
has even been introduced. That is the HOTT blocker.

HOTT proof relevance still belongs first to object witness terms. V2-P0 keeps
every validated derivation produced by the configured solver budget, but does
not commit failed, superseded, unfinished, or merely hypothetical search
candidates.

### Required change

V2-P0 makes OperationGraph authoritative for source/generated operation typing
and for structural child edges. Binder, declaration, type-formation, intrinsic,
and universe facts remain attached to their own authorities instead of being
forced into fake Operation IDs. Synthesis, derived admissibility, and explicit
exposure/conversion are represented as different concepts.

Do not preselect a tagged subject union, tagged proof payload, or premise arena.
After operation ownership is corrected, re-audit which evidence cannot be
derived from OperationGraph, ContextDB, TypeDeclarationDB, UniverseDB, or kernel
replay. Only that irreducible remainder may justify new storage in V2-P1.

The mandatory pre-P1 correction is V2-P0 and is tracked in
`doc/2026-08-07T06-00-00-OPERATION-INDEXED-TYPING-EVIDENCE-V2-P0-PLAN.md`.

Do not add a general `JUDGEMENT_EQ` merely to represent object equality. Meta
conversion remains a kernel service; object equality remains a type inhabited
by terms.

## 12. High Refactor 8: Replace Specialized Motive Equations with Typed Constraints

### Current implementation

The classifier solver now has an explicit constraint kind, state, worklist, and
dependency index at `src/prototype/ast.c:13154-13273`. It also retains
specialized arenas for:

- Match motive equations;
- constant motive candidates;
- IH motive applications;
- classifier bindings.

This is real constraint generation and solving, so the original audit's claim
that no constraint IR exists is obsolete. However, the generic record contains
only source IDs and untyped `target`, `left`, `right`, and `aux` fields. Its
comments use equations such as `M(_) == T`, but these remain solver-local
records, not typed equality judgements or object terms.

### HOTT risk

Higher observational synthesis will require constraints indexed by context,
carrier type, endpoints, and dimension. Reusing the current untyped equation
notation would confuse metavariable unification with object equality.

### Required change

Evolve the existing worklist solver into a typed constraint IR with distinct
kinds for:

- classifier metavariable assignment;
- conversion constraint;
- motive application constraint;
- observational witness goal;
- transport/coherence goal;
- effect-row constraint;
- universe constraint.

Each solved constraint must identify its validation rule. Solver equations must
not become object Eq proofs automatically.

Every HOTT-facing constraint must additionally carry or reference its context,
carrier, dimension, endpoints, boundary data, conversion profile, and stable
validation rule. Existing Match and effect constraints may be migrated
incrementally, but parallel untyped HOTT arrays must not be introduced.

## 13. High Refactor 9: Extend ADT Schemas Before Quotients or HITs

### Current implementation

An ADT constructor has a field telescope and a result classifier. Match has
ordinary constructor cases. There is no declaration form for path constructors,
quotient relations, or coherence clauses.

### Problem

The intended machine-word model cannot be represented merely by declaring
`2^32` ordinary point constructors. A cyclic model needs either:

- a quotient type former with a specified observational relation; or
- a higher-inductive schema with point and path constructors.

Those alternatives have different eliminators and computation laws.

### Required change

Choose one initial construction in the calculus. Then extend declarations with
an explicit distinction among:

- parameters;
- indices;
- point constructors;
- observational quotient relations or path constructors;
- elimination motives;
- coherence obligations.

Do not encode path constructors as ordinary constructor ordinals. Existing
constructor representation interning is a runtime/data-layout mechanism, not a
higher equality semantics.

## 14. High Refactor 10: Define the HOTT Boundary for CBPV and Effects

### Current implementation

TermDB distinguishes value, computation, and type categories. OperationGraph
also records source polarity. `Comp`, `Thunk`, `Return`, `Force`, operation
requests, and computation folds are explicit tags.

### Theoretical gap

It is not yet specified what observational equality means for:

- pure computations;
- suspended computations;
- effectful computations;
- operation requests and resumptions;
- handled computations;
- one-shot versus multi-shot resumptions.

Graph equality is too strong in some cases and too weak in others. Full
contextual equivalence is not an acceptable decidable kernel rule without a
separate decision procedure.

### Required initial boundary

The first HOTT slice should cover value types and pure dependent functions. It
should treat effectful `Comp` equality as unavailable or residual until a
specific observation semantics is approved.

`Thunk` equality may only be added together with a stated observation rule for
the suspended computation. It must not be inferred merely by comparing the
enclosed core graph.

## 15. High Refactor 11: Stabilize Universe Semantics

### Current implementation

UniverseDB stores nodes, parameter edges, level assignments, and lower/upper
constraints at `src/prototype/universe.h:10-71`. Definitional comparison of two
Universe variables currently compares their level-variable IDs.

### HOTT risk

Higher Observational equality at the universe is where equivalence, type
equality, transport, and univalence meet. A level solver alone is not a universe
semantics.

### Required change

Before universe-level observational equality is enabled, specify and implement:

- universe formation and cumulativity;
- Pi universe level computation;
- equality between universe inhabitants;
- how equivalences produce type equalities;
- transport computation across those equalities;
- resizing or its explicit absence;
- artifact-stable universe constraints.

The first HOTT slice may exclude universe equality, but that exclusion must be
enforced by the kernel rather than left as an unimplemented switch case.

## 16. High Refactor 12: Give Intrinsics a Logical Model Boundary

### Current implementation

Host types are static descriptors containing a type ID, debug name, TermDB tag,
type-expression tag, and bit width at `src/prototype/term.c:26-55`. Pure
primitive declarations contain only arity and host argument/result IDs at
`src/prototype/term.h:166-174`. Host implementations separately map primitive
IDs to oracle kinds.

### Problem

This interface can establish that `#.int_add` has a classifier. It cannot state
inside A Program that the host implementation refines a cyclic word model, nor
that an integer-to-text operation implements canonical encoding.

### Required change

Split each intrinsic into:

1. logical model type;
2. abstract operation over that model;
3. host representation;
4. refinement relation between representation and model;
5. proof or trusted certificate for each implementation;
6. backend capability and ABI identity.

HOTT equality can then express quotient-like machine-word semantics without
making the C oracle the source-language meaning.

## 17. Critical Artifact Migration

### Why v61 cannot remain valid

Artifact validation currently replays v61 typing and conversion rules. Once the
kernel admits new observational terms, transport rules, or type-former
semantics, the meaning of a proof graph depends on the selected calculus.

Two artifacts must not link merely because both say `A_PROGRAM_ARTIFACT 62` if
they were checked under different equality theories.

### Required v62 header contract

The first HOTT artifact should include at least:

- artifact format version;
- kernel calculus ID;
- HOTT fragment version;
- normalization-rule profile ID;
- universe-policy version;
- type-former semantic-schema version;
- intrinsic model/ABI fingerprints;
- proof-schema version;
- residual-obligation schema versions.

### Required graph additions

The graph serializer, marker, relocator, reader, and validator must understand:

- equality type/witness nodes selected by the calculus;
- transport and symmetry nodes;
- observational context/type/term actions;
- quotient or higher-constructor declarations;
- coherence proof edges;
- any new rule-specific JudgementDB payloads.

### Compatibility policy

Follow the existing repository policy: do not retain a v61 compatibility path
inside the new reader. Keep a v61-producing binary or fixture only for migration
tests. The new reader should reject v61 explicitly after the v62 transition.

## 18. Implementation Order

The order is semantic, not merely convenient.

### Phase 0: Theory freeze

- [x] Specify the concrete HOTT calculus.
- [x] Select the finite first implementation fragment.
- [x] Specify the context CwF and typed computation judgements over the shared
  TermDB.
- [x] Specify reindexing laws for `Comp`, `RETURN`, `Thunk`, and `FORCE`.
- [x] Define the CBPV/effect exclusion boundary for the first HOTT fragment.
- [x] Specify observational boundaries and the action on contexts,
  substitutions, types, terms, and admitted type formers.
- [x] Record quotient/HIT and machine-word observation as deferred beyond the
  first fragment rather than inventing an incomplete rule.

The initial working boundary is ordinary typed values, ordinary ADT point
constructors, and the subset of shared `PI/LAMBDA/APP` and F/U-boundary terms
whose computations are statically established as pure. This is not a
value-side Pi. Effectful computation, operation-request, and handler equality
remain unsupported or residual until their indexed observational semantics is
written.

### Phase 1: Existing-kernel cleanup without HOTT tags

- [x] Separate alpha-canonical TermDB interning from shape/link comparison.
- [x] Remove Match-frame canonical-key equality from kernel conversion.
- [x] Add scoped Match/IH conversion tests, including a forged key collision.
- [x] Introduce structured conversion outcomes.
- [x] Make `EFFECT_ROW_FORALL` conversion recursively use conversion under its
  binder instead of falling back to source-shape comparison.
- [x] Move ContextDB/SubstitutionDB implementation out of `ast.c`.
- [ ] Move source/generated operation typing from Core Term identity to typed
  Operation identity in V2-P0, while rehoming non-Operation facts.
- [ ] Re-audit whether a tagged proof payload remains necessary after V2-P0.
- [x] Extend the existing solver record into a typed constraint design.
- [x] Add tests proving TypeView/core sharing never establishes object equality.

The structured conversion item is planned and tracked in
`doc/2026-08-06T03-00-00-STRUCTURED-CONVERSION-RESULT-MIGRATION.md`.
The combined scoped-conversion implementation for V2-K1 and V2-K2 is tracked
in `doc/2026-08-06T04-00-00-SCOPED-KERNEL-CONVERSION-K1-K2-PLAN.md`.
The completed context/substitution extraction is tracked in
`doc/2026-08-07T00-00-00-CONTEXT-SUBSTITUTION-V2-C1-IMPLEMENTATION-PLAN.md`.

The Match/IH conversion and context ownership changes should remain artifact
v61 if no serialized field or meaning changes. Do not refactor serialized proof
payloads in isolation. Finalize the finite calculus first and perform one
intentional schema migration containing the new proof payload and first HOTT
records.

### Phase 2: Categorical and typed-solver infrastructure

- [x] Define a context-CwF API boundary independent of AST lowering.
- [x] Define context-indexed computation and effect-row reindexing interfaces.
- [x] Add law tests for identity, composition, extension, and computation
  reindexing; the current example-based category test is not a general law
  checker.
- [x] Add typed conversion/residual constraint records carrying context,
  carrier, profile, rule identity, and budget.
- [ ] Complete V2-P0: use OperationGraph edges for structural typing, attach
  derived conversion/exposure to explicit typed boundaries, and rehome
  non-Operation facts.
- [ ] Re-audit V2-P1 storage requirements after the ownership migration; do not
  assume tagged payloads or a premise arena in advance.

### Phase 3: Observational infrastructure

- [ ] Add context, substitution, type, and term observational actions.
- [ ] Add the type-former semantic authority.
- [ ] Add object equality terms for the selected fragment.
- [ ] Add transport/symmetry/coherence operations required by that fragment.
- [ ] Validate substitution and naturality laws.

### Phase 4: ADT and universe integration

- [ ] Generate observational rules from ADT telescopes.
- [ ] Add the chosen quotient or path-constructor schema.
- [ ] Add universe-level rules only after universe semantics is fixed.

### Phase 5: Intrinsic refinement

- [ ] Add logical models for machine words, Text, and encoding operations.
- [ ] Connect host implementations through refinement certificates.
- [ ] Keep host execution outside kernel DefEq unless explicitly justified by a
  deterministic, terminating, artifact-stable rule.

### Phase 6: Artifact v62

- [ ] Freeze all numeric tags and the evidence schema selected by the P1
  re-audit.
- [ ] Add semantic fingerprints to the header/interface.
- [ ] Serialize only the irreducible evidence selected after P0, together with
  typed residuals and the selected HOTT graph nodes, in one schema break.
- [ ] Serialize and relocate all new graph edges.
- [ ] Reject v61.
- [ ] Add source-to-artifact, readback, link, and revalidation tests.

## 19. Required Regression Tests

At minimum, the migration must test:

1. two distinct higher witnesses with identical endpoints remain distinct;
2. alpha-equivalent binders remain equal where the calculus says so;
3. core-shape sharing does not prove observational equality;
4. Bool and Two require an explicit equivalence/equality witness;
5. Pi equality is pointwise and preserves higher witnesses;
6. substitution reindexes endpoints, carrier types, and witnesses coherently;
7. transport computes according to the selected rules;
8. quotient equality uses its declared relation;
9. effectful computation equality is rejected or residualized according to the
   declared first-fragment boundary;
10. artifacts with different calculus fingerprints cannot link;
11. v61 is rejected by the v62 reader;
12. proof-only graph nodes remain reachable through artifact validation even if
   they are erased from runtime code generation.

## 20. Theory References Used by This Audit

The audit uses the following sources as constraints, not as a complete imported
specification for A Program:

1. Thorsten Altenkirch, Yorgo Chamoun, Ambrus Kaposi, and Michael Shulman,
   *Internal Parametricity, without an Interval*, POPL 2024.
   <https://doi.org/10.1145/3632920>
2. Ambrus Kaposi, *Towards Higher Observational Type Theory*, TYPES 2022
   paper and presentation.
   <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>
3. Loic Pujet and Nicolas Tabareau, *Observational Equality Meets CIC*, ESOP
   2024. <https://doi.org/10.1007/978-3-031-57262-3_12>
4. Matthew Sirman, Meven Lennon-Bertrand, and Neel Krishnaswami,
   *Implementing a Type Theory with Observational Equality, Using
   Normalisation by Evaluation*, TYPES 2024.
   <https://doi.org/10.4230/LIPIcs.TYPES.2024.5>
5. Matthijs Vakar, *A Framework for Dependent Types and Effects*, 2015.
   <https://arxiv.org/abs/1512.08009>

References 3 and 4 concern observational equality systems that are not the
selected higher theory. They are used only for implementation lessons about
type-former-specific equality, transport computation, inductive integration,
and normalization. Their proof-irrelevance assumptions must not be copied into
A Program's higher witness layer.

The proposed four-part value-CwF/indexed-computation/CBPV/observational model is
not claimed to be a theorem copied from one of these papers. It is the current
A Program-specific synthesis. Its equations, admissible effects, and artifact
meaning must be stated and tested in the normative calculus.

## 21. Immediate Decision

No equality TermDB tag should be added yet.

The former Phase 1 tasks for Match-frame conversion and context ownership and
the V2-T1/T2 shared-TermDB calculus checkpoint are complete. The frozen
calculus introduces no separate value/computation copies of Pi, Lambda, APP,
or Match. The completed plan is
`doc/2026-08-07T01-00-00-SHARED-TERM-HOTT-DCBPV-V2-T1-T2-PLAN.md`.

The next decision is V2-C2 direct binding-object reindexing. It preserves the
context, carrier, endpoints, bridge structure, and reindexing laws fixed by the
T1/T2 checkpoint while removing temporary fresh-binding substitution from the
substrate. V2-S1 typed constraint ownership follows C2. Proof schema and
artifact v62 still wait for the resulting exact payloads.

## 22. Detailed Implementation Plan: Term Identity and Match-Frame Scope

### 22.1 Goal and non-goals

The goal is to make each comparison API answer exactly one semantic question:

1. should two local terms occupy the same TermDB slot;
2. do two terms have the same selected TypeView projection;
3. are two terms compatible across an artifact boundary;
4. are two typed terms convertible in the kernel;
5. does an object-level equality witness exist.

This refactor implements only questions 1-3. It must not introduce PropEq,
HOTT witnesses, new conversion rules, or a general unification solver.

Alpha equality remains part of TermDB identity. The defect is that the current
implementation combines alpha comparison, TypeView projection, Match-frame
erasure, and linker compatibility in one traversal controlled by mode integers
and `ignore_match_frames`.

### 22.2 Current paths to split

The implementation must classify and migrate these paths:

- `add_term` in `src/prototype/term.c`: local alpha-canonical interning;
- `prototype_term_view_shape_equal`: source TypeView projection comparison;
- `prototype_term_core_shape_equal`: erased computation-core comparison;
- `prototype_term_source_shape_equal`: source projection comparison;
- cross-database shape functions used by `src/prototype/ast.c`: artifact link
  compatibility;
- structural fast paths used by normalization and typing: syntactic
  prefilters, never complete conversion decisions;
- canonical hashes and Match-frame keys: artifact lookup prefilters, never
  equality proofs.

Every call site must be recorded in a migration table before changing code.
The table must state the semantic question, selected TypeView projection,
whether the databases differ, and whether frame scope is local or linked.

### 22.2.1 Completed migration table

| Owner and call path | Semantic question | Projection | Database relation | Frame rule after migration |
| --- | --- | --- | --- | --- |
| `term.c:add_term -> term_intern_alpha_equal_local` | Reuse one local TermDB slot | View-preserving | Same DB | Enclosing Match establishes a scoped frame pair; free IH requires the same local frame |
| `prototype_term_view_shape_equal` callers in `typing.c`, `term.c`, and `ast.c` | Compare source-visible structural evidence | View-preserving | Same DB | Scoped local frame alpha comparison; never link-key recovery |
| `prototype_term_core_shape_equal` callers in `typing.c`, `term.c`, and `ast.c` | Compare erased representation shape | Core projection | Same DB | Scoped local frame alpha comparison; result is structural evidence, not conversion |
| `prototype_term_source_shape_equal` callers in `typing.c` | Compare the source projection used by constructor-owner validation | Source projection | Same DB | Scoped local frame alpha comparison; result is structural evidence |
| `normalization_equal_at_depth` effect-row-forall fast path | Compare already binder-aligned syntax before recursive normalization | Source projection | Same DB | Explicit term-binder environment and scoped local frames |
| `ast.c:find_existing_term_by_canonical_key` | Validate an append/readback candidate selected by a canonical key | View-preserving | Cross graph boundary in the merged DB | Full link comparison after the key prefilter |
| `ast.c:prototype_canonical_link_table_add_metadata` | Validate that two exported labels can share a link representative | View-preserving | Different artifact DBs | Full cross-DB Match/frame comparison after the key prefilter |
| `prototype_term_{view,core,source}_shape_equal_for_link` | Compare link candidates | Selected named projection | Cross DB | Bound frames use scoped pairs; free frames require key prefilter plus referenced-Match comparison |
| Canonical-key generation and Match-frame keys | Select lookup candidates | Not an equality projection | Local generation, cross use | Never returns term equality by itself |

The former `find_local_term_by_key` and
`prototype_canonical_link_table_find` paths had only a key and no source term
with which to perform structural validation. They were deleted rather than
retained as unsafe compatibility fallbacks.

### 22.3 Phase A: Characterization tests before implementation

Add a focused prototype test that constructs terms directly through TermDB
APIs. It must cover at least:

1. alpha-renamed Lambdas intern to one TermDB node;
2. different free binder references remain distinct;
3. alpha-renamed Match case binders intern to one node;
4. equivalent recursive Matches with different numeric frame IDs intern to one
   node when each IH refers to its own enclosing Match frame;
5. an IH referring to an unrelated frame does not intern with an IH referring
   to the enclosing frame, even if both frames have compatible keys;
6. two free IH references do not become equal merely because their frame keys
   match;
7. wrapping the same pair in APP, THUNK, or LAMBDA does not change the equality
   result;
8. cross-database link comparison accepts structurally corresponding recursive
   frames;
9. canonical-key equality or a hash collision never bypasses full structural
   comparison;
10. existing Bool/Two TypeView separation remains unchanged.

The test belongs under `src/prototype/`. It may use a shell driver that compiles
the focused test without modifying the accepted root `Makefile`.

### 22.4 Phase B: Introduce an explicit comparison environment

Replace the current implicit flag propagation with an internal environment:

```text
TermCompareEnv
  term binder pairs: left binder <-> right binder
  Match frame pairs: left frame <-> right frame
  visited cross-frame pairs for cycle prevention
```

Both correspondence tables must be one-to-one. A left identity cannot map to
two right identities, and a right identity cannot map to two left identities.
The implementation may use small arrays initially because comparison depth is
already bounded. It should not store this environment in TermDB.

TypeView projection is not part of binder scope. Pass projection through a
separate internal policy selected by a named entry point, rather than adding
more booleans to `TermCompareEnv`.

### 22.5 Phase C: Define exact Match-frame comparison rules

Treat a Match frame as a recursive binder whose scope is its case bodies.

When comparing two Match terms:

1. compare the scrutinees before extending frame scope;
2. if both Matches have no recursive frame, continue without a frame pair;
3. if exactly one Match has an internal frame, pair it with
   `PROTOTYPE_INVALID_ID`; the frame is observationally vacuous unless an IH
   refers to it;
4. if both have frames, push their IDs as a one-to-one pair;
5. compare constructor labels, case binders, and case bodies under that pair;
6. pop the frame pair on return.

When comparing two IH terms:

1. if the left frame is mapped, the right frame must be its mapped partner;
2. if the right frame is reverse-mapped, the left frame must be its mapped
   partner;
3. if neither frame is mapped during local interning, require the same local
   frame identity;
4. never establish local equality from `linkable`, hash, node count, or a
   canonical Match-frame key.

Cross-database comparison may resolve an otherwise free frame through its full
canonical key, but only as a prefilter. It must then recursively compare the
referenced Match structures. A visited frame-pair set must terminate the
`Match -> IH -> frame -> Match` cycle.

### 22.6 Phase D: Split and rename comparison entry points

Provide named internal entry points with no public `ignore_match_frames`
parameter:

```text
term_intern_alpha_equal_local
term_view_shape_equal_local
term_core_shape_equal_local
term_source_shape_equal_local
term_view_shape_equal_for_link
term_core_shape_equal_for_link
term_source_shape_equal_for_link
```

The exact names may follow the surrounding C naming convention, but the
semantic split is mandatory. Do not keep compatibility wrappers after all
prototype call sites are migrated.

`add_term` must call only `term_intern_alpha_equal_local`. Linker code in
`src/prototype/ast.c` must call only `*_for_link`. Typing and normalization must
call a named local projection comparison or kernel conversion; neither may call
a linker comparator.

### 22.7 Phase E: Separate canonical keys from equality

Canonical keys remain useful for hash-table lookup and artifact relocation.
Their role must be restricted to candidate selection:

```text
canonical key match
  -> candidate pair
  -> full cross-database structural comparison
  -> compatible or incompatible
```

Split terminology between:

- local alpha-canonical interning hash;
- artifact/link canonical key;
- Match-frame link key.

The same-database IH comparison must not call Match-frame key construction.
No successful key comparison may directly return semantic equality.

### 22.8 Phase F: Migrate call sites by semantic ownership

Migrate in this order:

1. `add_term` and all local interning paths;
2. local public TypeView/core/source shape APIs;
3. normalization's structural fast path;
4. typing's representation-shape checks;
5. artifact canonical lookup and link tables;
6. readback and diagnostic callers;
7. tests and debug utilities.

For normalization, structural equality is only an early success when it uses
the same scoped alpha semantics as TermDB. It must not inherit artifact frame
compatibility. For typing, each shape check must be documented as either a
representation check or replaced by kernel conversion if it is deciding a type
judgement.

### 22.9 Phase G: Artifact version decision

Do not bump v61 merely because internal comparator code changes. Keep v61 only
if all serialized fields, canonical-key algorithms, and link compatibility
semantics remain unchanged.

Bump directly to v62, with no backward-compatibility reader, if any of these
occur:

- serialized canonical keys change;
- Match-frame key semantics change;
- a new field is needed to preserve scoped frame identity;
- an artifact accepted by v61 must now be rejected for semantic correctness.

The decision must be made after Phase E tests, not guessed before them. Record
the chosen version and reason in the artifact format document and test fixture.

### 22.10 Phase H: Validation and deletion of old mechanisms

Completion requires:

1. all focused identity/frame tests pass;
2. all existing prototype checks pass;
3. examples 01-09 compile and evaluate as before;
4. artifact round-trip and cross-artifact recursive Match tests pass;
5. append normalization remains equal after artifact readback and linking;
6. `ignore_match_frames` no longer exists;
7. TermDB interning has no dependency on canonical link keys;
8. no public comparison API takes an untyped mode integer or frame-ignore
   boolean;
9. `git diff --check` passes;
10. comments and function names state whether a comparison is local identity,
    projection shape, link compatibility, or kernel conversion.

### 22.11 Progress checklist

- [x] Inventory every comparator and call site.
- [x] Add characterization and regression tests.
- [x] Add term-binder and Match-frame correspondence environments.
- [x] Implement scoped Match/IH comparison.
- [x] Split local interning, local shape, and link entry points.
- [x] Migrate TermDB interning.
- [x] Remove Match-frame canonical-key equality from kernel conversion.
- [x] Migrate artifact/link callers.
- [x] Separate local identity from link canonical keys.
- [x] Decide and document v61 versus v62.
- [x] Delete old shape/link flags, wrappers, duplicated frame-policy branches,
  and the conversion-specific key path.
- [x] Run the complete validation matrix.

### 22.12 Stop conditions

Stop implementation and revise the calculus or artifact design if testing shows
either of these assumptions is false:

- a Match frame is not lexically scoped by its enclosing Match;
- an IH may validly refer to a frame that cannot be represented by a scoped,
  one-to-one frame correspondence.

Do not repair either failure by restoring frame erasure. Such a result means
the recursive binding model itself needs an explicit redesign before HOTT
equality terms can be added safely.

### 22.13 Implementation result and V2 correction

Implemented on 2026-08-06.

The comparison environment now contains independent one-to-one maps for term
binders and Match frames. Local Match comparison pushes a frame pair before
comparing case bodies. An IH must refer to the mapped partner; an unbound local
IH frame is equal only to the same local frame ID.

Cross-database comparison uses the same scoped rule. For an unbound frame it
first checks the complete Match-frame canonical key, then compares the
referenced Match terms structurally. A visited frame-pair set terminates
recursive frame validation. Canonical keys therefore select candidates but do
not complete an equality decision.

The public frame-ignore boolean was removed. Link entry points are now named
`prototype_term_{view,core,source}_shape_equal_for_link`. TermDB interning uses
the private `term_intern_alpha_equal_local` entry point. The old key-only local
term fallback and unused key-only canonical-link lookup API were deleted.

V2 correction: this result is complete for TermDB interning, local projection
comparison, and cross-artifact link validation, but not for kernel conversion.
`normalization_equal_at_depth` still compares IH frames through
`match_frame_keys_equal`. The Phase 1 completion claim is therefore narrowed
until that conversion path uses an explicit scoped frame correspondence and its
focused tests pass.

The artifact remains v61. This refactor changes no serialized field, numeric
tag, canonical-key algorithm, or Match-frame key algorithm. It makes the
post-key structural validation stricter. A v62 bump remains required when HOTT
syntax or proof payloads alter the serialized schema.

The removed key-only recovery path was not part of the well-formed v61 writer
contract: it guessed a local term when an authoritative relocated term ID was
missing. Writer-produced, validated v61 artifacts retain their meaning and pass
the existing round-trip suite. Rejecting a malformed artifact that depended on
that guess is validation hardening, not a format migration.

Validation completed:

- the focused `test_term_identity_frame.sh` characterization test passes,
  including APP/LAMBDA/THUNK wrappers and a forged canonical-key collision;
- all prototype `test_*.sh` scripts pass;
- examples 01-07 and 09 compile with a freshly rebuilt reader;
- artifact recursive-Match readback and append normalization tests pass;
- `test_artifact_flow.sh` continues to assert the v61 header and v60 rejection;
- `ignore_match_frames` and key-only local term recovery are absent;
- `git diff --check` passes.

## 23. V2 Re-Audit Findings and Progress Sheet

### 23.1 Current semantic inventory

At commit `474867e`:

- TermDB has no equality, path, transport, interval, dimension, or coherence
  term tag;
- definitional conversion has structured outcomes but no typed residual
  producer;
- local alpha identity and cross-artifact link shape have named entry points;
- kernel conversion owns lexical binder and Match-frame correspondence and no
  longer consumes Match-frame canonical keys;
- Match case `is_recursive` metadata is preserved by local identity,
  cross-database structural comparison, and kernel conversion;
- `EFFECT_ROW_FORALL` conversion recursively normalizes under corresponding
  row binders and propagates budget and blocked-effect outcomes;
- ContextDB/SubstitutionDB provide ordinary context-CwF operations over the
  shared TermDB;
- OperationGraph records source occurrences, context IDs, and value/computation
  polarity;
- `Comp`, `Thunk`, effect rows, operation requests, and computation folds are
  present, but no categorical interface owns all of their typing, reindexing,
  conversion, and future observational rules;
- the classifier solver has constraints and a worklist, but not HOTT-indexed
  typed goals;
- JudgementDB proof payloads and artifact v61 are not extensible enough for the
  selected higher witness layer.

### 23.2 Revised priority table

| ID | Work item | Status | Artifact effect | Exit evidence |
| --- | --- | --- | --- | --- |
| V2-K1 | Replace key-based IH conversion with scoped frame correspondence and preserve recursive-binder metadata | complete | none | nested/foreign-frame, collision, local/link metadata, artifact tests pass |
| V2-K2 | Make binder-bearing conversion recursively semantic | complete | none | alpha/beta/nested forall, budget, and blocked-effect tests pass |
| V2-C1 | Extract context/substitution implementation from `ast.c` | complete | none | CwF laws, all prototype tests, examples 01-07/09, and byte-identical v61 artifact pass |
| V2-T1 | Freeze finite typed HOTT fragment over the shared TermDB | complete | none | frozen normative calculus and first-fragment matrix |
| V2-T2 | Freeze dependent CBPV boundary without duplicated graph syntax | complete | none | F/U reindex laws, purity trichotomy, and law tests pass |
| V2-C2 | Replace fresh-binder reindexing with direct binding-object graph action | complete | no schema change | simultaneous/capture/IH laws, depth-513 context, all 14 tests, examples 01-07/09, old/new v61 readback, and deterministic output pass |
| V2-B1 | Replace positional binder-assumption proof identity with direct binding-object identity | complete | v61 slot is reserved; physical removal is deferred to V2-P1/v62 | exact-binding validator, binding-aware context identity, relocation, forged-proof, artifact, all 14 test scripts, and examples 01-07/09 pass |
| V2-S1 | Extend solver constraints with typed HOTT indices | complete | v61 unchanged; residual serialization deferred to v62 | tagged classifier goals, deterministic conversion goals, Context/substitution-indexed HOTT goals, purity/residual tests, all 15 scripts, and byte-stable artifacts pass |
| V2-P0 | Make OperationGraph authoritative for operation typing and normalize accepted Claim/Derivation ownership | complete at `4025532` | v67 publishes accepted Claims/Derivations with exact link and Universe provenance | all 16 scripts, examples 01-07/09, forged artifacts, schema fingerprint, and exit audit pass |
| V2-P1-R0 | Repair the typed HOTT-goal entry substrate discovered by the P1 audit | complete at `564a2ee` | v68 removes untrusted solver wire state; structural substitutions replay typing; the first admitted bridge is terminal | adversarial typed-goal, conversion, bridge, substitution, polarity, artifact, all 16 scripts, and examples 01-07/09 pass |
| V2-P1 | Select irreducible HOTT certificate payload after P1-R0 | complete | compiler-local ownership split; no v68 change | immutable observation-family goals, multiple rule candidates, exact authority premises, dedicated HOTT manifest, all 16 scripts, and examples 01-07/09 pass |
| V3-G1 | Graph-cons Context, Substitution, and Judgement over direct binding identity | complete locally; general pullback interning corrected in O1.1 | v69 records exact weakening action; hash indices and caches are rebuilt, not serialized | all 16 scripts pass; collision-safe Context/Substitution/Judgement identity, provisional Context finalization, warm-cache pullback reuse, direct premise DAG edges, and resource boundary audited |
| V2-O1 | Implement type-directed observational action over shared terms | complete locally | compiler-local HOTT fragment v2; artifact remains v69 | non-empty bridge, substitution/naturality, typed ADT/Pi/CBPV/Match/IH family and witness tests |
| V3-SC1 | Consolidate proposition/premise, CwF certificate, reindex, and HOTT execution infrastructure before publication | complete | v69 wire retained through one expanded-tuple adapter | all old in-memory paths deleted; full suite, forgeries, sanitizers, and measured per-file report pass |
| V3-PC1 | Normalize persistent Claim and accepted-premise Proposition references before publication | complete | v69 retained through the existing adapter | pointer-free Claims, discriminated accepted premise IDs, full replay and artifact suite |
| V2-A1 | Add the object-HOTT artifact schema selected after O1 | next | breaking, version after v69 | compact proposition/premise wire plus HOTT witness/link matrix frozen by O1 |

### 23.3 Non-negotiable boundaries

1. Canonical keys and hashes select link candidates; they never establish
   local identity, definitional conversion, or object equality.
2. Definitional conversion remains a meta-level kernel service.
3. Object equality is represented by types and witness terms checked through
   ordinary `HAS_TYPE` judgements.
4. Solver equations do not become object equality witnesses automatically.
5. The context CwF remains the base of dependent syntax. Value/computation
   distinctions are typed occurrence data over one shared TermDB, with explicit
   CBPV boundary nodes where the adjunction changes interpretation.
6. The first HOTT fragment does not invent computation equality by comparing
   erased computation graphs.
7. No value-side Pi, Lambda, APP, or Match graph tag is introduced.
8. Published HOTT and dependent-CBPV work constrains the design but does not
   substitute for A Program's normative calculus.
9. Graph-consing unifies identity discipline and implementation substrate; it
   does not identify Context objects with Term objects or Substitutions with
   object-language functions.
10. Resource grades are indexed by the same binding objects but are not part of
    erased TermDB identity.

### 23.4 Next implementation checkpoint

V2-K1, V2-K2, V2-C1, V2-T1, V2-T2, V2-C2, V2-B1, and V2-S1 are complete.
V2-P0 is also complete. Commit `4025532` establishes the accepted certificate
boundary and artifact v67: source typing belongs to exact Operation/type-view
authority, structural premises replay through OperationGraph, one Claim may
have multiple Derivations, and link/Universe provenance names exact accepted
Claims. Solver candidates and residual obligations remain outside that closed
certificate.

The P1 entry audit and V2-P1-R0 repair are complete. HOTT observations now name
exact accepted carrier/endpoint Claims, conversion results are bound to exact
requests and cannot manufacture contradiction, ordinary ADTs dispatch through
nominal TypeView declarations, and artifact v68 removes solver/proof caches
from Context, Operation, and Substitution wire records. The first admitted
relational Context fragment is the terminal Context with its unique identity
projections; nonempty relational extension remains deferred until O1 defines
the relation field. The implementation and exit evidence are in
`doc/2026-08-08T20-00-00-V2-P1-ENTRY-REFACTOR-AUDIT-PLAN.md`. V2-P1 may now
select irreducible observational evidence. Object equality syntax and witness
construction remain deferred to V2-O1.
The completed V2-P1 implementation and exit record is in
`doc/2026-08-08T21-45-21-V2-P1-IRREDUCIBLE-HOTT-EVIDENCE-IMPLEMENTATION-PLAN.md`.
It separates immutable observation-family identity, candidate rule
applications, mutable work state, and future object witnesses; freezes exact
typed premise edges and a dedicated HOTT fingerprint; and corrects the
distinct-constructor family/contradiction conflation before O1. V3-G1 has now
removed the remaining Context/Substitution/Judgement graph identity split.
V2-O1 is complete locally. Its post-G1 implementation and verification record is
`doc/2026-08-09T07-10-00-V2-O1-OBSERVATIONAL-ACTION-IMPLEMENTATION-PLAN.md`.
V3-SC1 semantic consolidation is complete at `7cc6dc9`. It removed the
duplicated proposition/premise, base-CwF certificate, reindex wrapper, HOTT
store-bundle, and deterministic-outcome paths required before publication. Its
implementation and completion record is
`doc/2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`.
V3-PC1 persistent Proposition-reference normalization is complete. V2-A1 is now the next
implementation stage; V2-A1 follows its completion.

### 23.5 SC1 completion evidence

SC1 is complete relative to `31e5446`. The in-memory proof graph now uses one
interned proposition table, ordered candidate/accepted premise arenas, a
compact rule-data union, one typed CwF certificate arena, checked kernel
views/builders, shared proposition reindex replay, and one deterministic HOTT
outcome representation. The v69 seven-column rule payload and expanded scoped
tuple survive only in the artifact adapter and are removed by A1/v70.

All 16 prototype scripts, examples 01-07/09, optimized `-Werror`, ASan/UBSan,
deterministic v69 output, artifact append/link, CwF/reindex laws, HOTT actions,
and field-level scoped-premise forgeries pass. The exact file-by-file
`+4,195/-3,616` accounting and semantic-path deletion table are recorded in
the SC1 plan. V3-PC1 is therefore unblocked.
V2-A1 artifact publication follows V3-PC1.
Its codebase-aligned implementation and progress plan is
`doc/2026-08-09T13-11-17-V2-A1-OBJECT-HOTT-ARTIFACT-IMPLEMENTATION-PLAN.md`.
The completed G1 execution record remains in
`doc/2026-08-08T22-51-04-CONTEXT-SUBSTITUTION-JUDGEMENT-GRAPH-CONSING-V3-G1-PLAN.md`.
The completed P0
execution record is in
`doc/2026-08-08T17-10-35-V2-P0-REMAINING-IMPLEMENTATION-PLAN.md`. The
V2-B1 implementation and progress plan is in
`doc/2026-08-07T04-00-00-PROOF-BINDING-IDENTITY-V2-B1-PLAN.md`. The completed
V2-S1 implementation and evidence record is in
`doc/2026-08-07T05-00-00-TYPED-HOTT-GOALS-V2-S1-IMPLEMENTATION-PLAN.md`.
The completed V2-C2 implementation and evidence record is in
`doc/2026-08-07T03-00-00-BINDING-OBJECT-DIRECT-REINDEX-V2-C2-PLAN.md`. The
completed T1/T2 plan is in
`doc/2026-08-07T01-00-00-SHARED-TERM-HOTT-DCBPV-V2-T1-T2-PLAN.md`; the
normative calculus is in
`doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`. The
completed V2-C1 ownership, migration, and progress record is in
`doc/2026-08-07T00-00-00-CONTEXT-SUBSTITUTION-V2-C1-IMPLEMENTATION-PLAN.md`.

## 24. 2026-08-07 Binding-Object Reindex Correction

### 24.1 Design decision

A Program will not adopt De Bruijn indices or levels for stored TermDB bound
variables. A graph occurrence refers directly to an opaque binding object. The
current `uint32_t binding_id` is an arena handle, equivalent to a
stable pointer inside TermDB, and not as a source-level name.

Surface spelling remains in the parser, AST, diagnostic, and Name layers. A
TermDB `VAR` edge identifies its introducing graph binding independently of
that spelling. Local alpha interning continues to compare lexical binding
correspondence, so alpha-equivalent source programs can share one graph even
when lowering allocated different binding handles.

This decision follows the project's central graph principle: computation is
constructed by edges between semantic graph objects, while human names point
to typed occurrences above that graph. It also preserves the V2-T1/T2 decision
that Value and Computation do not receive duplicate Pi, Lambda, APP, or Match
graph constructors.

### 24.2 Why V2-C1 is not sufficient

V2-C1 correctly moved CwF ownership out of `ast.c`, but retained the old
algorithm. At the V2-C2 baseline, `prototype_term_reindex` protected
simultaneous substitution by replacing every target-context binding with a
temporary fresh binding and then replacing those fresh bindings with the final
terms. This was capture-safe, but performed repeated graph traversals, consumed
transient binding handles, interned staging-only graphs, and imposed a fixed
512-entry context limit.

The semantic operation remains necessary:

```text
sigma : Delta -> Gamma
Gamma |- t : A
----------------------
Delta |- t[sigma] : A[sigma]
```

The completed implementation builds a simultaneous map from target binding
handles to source-context TermDB nodes and applies it in one scope-aware graph
traversal. Replacement terms are final map images and are not recursively
captured by sibling entries.

### 24.3 IH ownership

A recursive Match frame is a hidden binding-like scope object, not runtime
machine state. When a changed Match is rebuilt, its IH owner and all enclosed
IH reference edges must be cloned coherently. This remains required, but it is
specified as scoped graph cloning rather than textual remapping or a separate
name-resolution mechanism.

An unchanged Match must retain its node and IH scope. A changed Match must not
leave any owner-local IH reference pointing to the old Match. Nested and
foreign frames remain distinct.

### 24.4 HOTT ordering consequence

Higher observational endpoints and witnesses must obey ordinary substitution
naturality. V2-S1 does not inherit the former fresh-renaming mechanism in its
typed goal records. V2-C2 was therefore completed before V2-S1:

```text
V2-C2 -> V2-B1 -> V2-S1 -> V2-P0/P0-R0A (including v63)
    -> V2-P1-R0 -> V2-P1 -> V2-O1 -> V3-SC1 -> V3-PC1 -> V2-A1
```

V2-C2 changes no artifact schema and adds no HOTT object term. Its complete
scope, implementation phases, progress sheet, and acceptance tests are defined
in
`doc/2026-08-07T03-00-00-BINDING-OBJECT-DIRECT-REINDEX-V2-C2-PLAN.md`.

### 24.5 Completion evidence

V2-C2 now uses one immutable binding-to-term view, one scope-aware TermDB
traversal, lexical binding blocking, IH-scope cloning, and traversal-local
memoization. Ordinary reindexing allocates no temporary binding. Graph APIs use
`binding_id`; recursive Match ownership uses `ih_scope_id`. Serialized v61
tokens remain `match_frames` and `match_frame` for compatibility.

The final run passed a clean build, all 14 prototype test scripts, examples
01-07 and 09, depth-513 reindexing, old and new v61 readback, normalization,
deterministic artifact output, and `git diff --check`. On
`09_list_induction.p`, transient TermDB slots fell from 1317 to 122 while the
artifact retained the same 99 present terms and one recursive scope. Detailed
numeric and test evidence is recorded in the V2-C2 plan.

## 25. 2026-08-07 Proof Binding-Identity Correction

### 25.1 Problem statement

V2-C2 made ordinary TermDB reindexing act directly on opaque `binding_id`
handles. Context entries also store `binding_id`, and a variable conclusion is
already represented by `VAR(binding_id)`. Binder-assumption proofs nevertheless
retain `assumption_index`, documented and validated as a De Bruijn level in the
conclusion Context.

The same binder is therefore identified in two incompatible ways:

```text
TermDB / ContextDB: binding object edge
Judgement proof:    lexical depth
```

This positional proof identity must not become part of the typed HOTT goal or
bridge-context contract. Reindexing a Context can preserve the binding graph
while changing arena placement or depth, and higher endpoint substitutions must
refer to the selected binding object rather than rediscover it by position.

### 25.2 Binding authority

The authoritative binder for a binder-assumption conclusion is the direct edge
already present in the conclusion subject:

```text
relation.subject = VAR(binding_id)
```

V2-B1 must not replace `assumption_index` with a second, independently writable
`assumption_binding_id` field. That would duplicate identity between the
conclusion subject and the proof payload. The validator must extract the
binding handle from the conclusion `VAR`, prove that the conclusion Context
contains that exact handle, locate the corresponding Context entry, and compare
that entry's classifier with the conclusion classifier under kernel conversion.

Context extension interning remains permitted only for an extension with the
same parent Context, the same binding object, and the same classifier state.
Two extensions with alpha-equal or convertible classifiers but different
binding objects are distinct Context objects. This is required because a
Context is not merely a classifier telescope: it is also the scope that owns
the exact binding edges referenced by terms and proofs.

### 25.3 Proof and artifact transition

During V2-B1, `assumption_index` becomes a reserved legacy slot and must be
`PROTOTYPE_INVALID_ID` in every newly created or accepted in-memory proof.
Artifact v61 keeps the numeric slot only to avoid an isolated schema migration
immediately before the P0/P1 evidence redesign. Readers must not use the slot to
reconstruct binder identity. The slot is removed physically in artifact v62
after the P1 re-audit selects the final evidence representation.

This is not backward-compatibility logic for positional proofs. A v61 proof is
accepted only when its relation conclusion and Context graph independently
establish the exact binding-object assumption; the positional value supplies no
authority.

### 25.4 Required implementation work

V2-B1 must:

1. replace every binder-assumption constructor assignment of
   `context->depth - 1` with the direct conclusion-binding invariant;
2. rewrite `validate_assumption_proof` to use the relation's `VAR(binding_id)`
   and exact Context membership;
3. remove positional checks from compiler-local assumption lookup;
4. ensure Context relocation and artifact append preserve/relocate binding
   handles independently of Context depth;
5. make proof parameter validation require the legacy slot to be invalid for
   every rule;
6. add forged-proof tests where the depth and classifier match but the binding
   object does not;
7. retain `HAS_TYPE` and `IS_TYPE` as the only Judgement conclusion forms.

V2-B1 does not add object equality, a HOTT witness tag, De Bruijn indices, a
second BindingDB, or separate Value/Computation graph syntax.

### 25.5 Ordering consequence

V2-S1 typed goals store Context and endpoint substitutions. They must be
created only after binder-assumption evidence has the same direct graph identity
as TermDB, ContextDB, and SubstitutionDB. V2-P0 next fixes occurrence ownership;
only its P1 re-audit may decide how the reserved proof slot and remaining rule
data are physically represented. The mandatory order is:

```text
V2-C2 -> V2-B1 -> V2-S1 -> V2-P0/P0-R0A -> V2-P1-R0 -> V2-P1 -> V2-O1
```

### 25.6 Completion evidence

V2-B1 is complete. The implementation:

1. derives binder-assumption identity solely from the conclusion
   `VAR(binding_id)`;
2. validates exact membership and retrieves the introducing Context through
   `prototype_context_find_binding`;
3. keeps distinct binding objects distinct during Context interning;
4. uses source-occurrence binders for Lambda-introduction premises while
   checking alpha shape only at the explicit typed-occurrence/erased-TermDB
   boundary;
5. writes the artifact v61 positional slot as invalid and rejects any artifact
   that attempts to give it authority;
6. preserves relocated binding identity across artifact append.

The final verification passed a warning-free clean build, all 14
`src/prototype/test_*.sh` scripts, examples 01-07 and 09, artifact readback and
append tests, forged-binding rejection, and `git diff --check`. The detailed
implementation record is
`doc/2026-08-07T04-00-00-PROOF-BINDING-IDENTITY-V2-B1-PLAN.md`.

## 26. 2026-08-08 V3 Graph-Consing Correction

### 26.1 Why V3 exists

V2 established correct semantic ownership in several local migrations, but it
did not give Context, Substitution, and Judgement one consistent graph identity
discipline. The remaining implementation is not a flat copied environment, but
it still combines persistent nodes, linear interning scans, dense temporary
views, fresh regenerated pullbacks, fixed premise arrays, and artifact
relocation loops.

Object-level HOTT action would amplify these differences. Every generated
relation Context, reindexed endpoint, naturality substitution, and accepted
observation derivation would pass through all three databases. O1 must not make
their current allocation history part of equality evidence.

V3 therefore inserts one mandatory stage:

```text
V2-P1 -> V3-G1 -> V2-O1 -> V3-SC1 -> V3-PC1 -> V2-A1
```

The detailed implementation and progress plan is:

`doc/2026-08-08T22-51-04-CONTEXT-SUBSTITUTION-JUDGEMENT-GRAPH-CONSING-V3-G1-PLAN.md`.

### 26.2 Meaning of unified graph-consing

Unified graph-consing does not mean one untyped universal node arena. The
categorical sorts remain distinct:

```text
Context                    object of the syntactic category
Substitution Delta -> Gamma  morphism
Term under Gamma           section/typed graph projection
Judgement proposition      typed occurrence claim
Derivation                 rule application edge graph
```

They share the following implementation laws:

1. every immutable semantic object has one explicit typed key;
2. construction goes through one interning API for that object sort;
3. identity is a stable arena handle, never a source name or insertion depth;
4. adjacency is stored as edges to existing graph handles;
5. mutable solver state is not part of immutable identity;
6. replayable data is derived from authoritative nodes rather than copied;
7. relocation re-interns typed keys and never proves equality from a hash; and
8. no object proof, conversion result, or resource permission is inferred from
   graph identity alone.

Graph-consing is therefore only half of V3-G1. The other half is **semantic
authority consolidation**. One semantic operation may have several typed
views, but it must have one authoritative graph edge:

```text
structural operation   Substitution projection p : Gamma.A -> Gamma
action                 reindex a term or proposition along p
certificate            CONTEXT_WEAKEN derived from the exact p and source Claim
```

These are not one node kind and must not be collapsed into TermDB. They also
must not independently rediscover the same relationship. A certificate points
to the operation which justifies it; a validator follows that edge and checks
the action. It does not repeat a second parent-chain interpretation of
weakening.

### 26.3 Current code findings

The V3 baseline has the following mixed representations:

| Area | Existing strength | Remaining debt |
| --- | --- | --- |
| Context | immutable parent extension; exact binding identity; repeated exact extension is reused | full linear scan on every extension; solved and unresolved classifiers use parallel fields; imported graphs are rebuilt entry by entry |
| Substitution | typed source/target; explicit identity, empty, projection, extension, and composition DAG | full linear scan on every insertion; no stable key index; repeated categorical actions are not cached |
| Reindex | one scope-aware TermDB traversal; unchanged paths retain Term IDs; traversal-local memo | each call walks the entire target Context and materializes a dense replacement array; composed lookup recursively repeats work |
| Context action | projections and extensions are explicit Substitution nodes | `prototype_context_fresh_reindex_extension()` allocates a new binding telescope for repeated equivalent pullback requests |
| Claim | accepted proposition identity is separate from Derivation | candidate and accepted proposition tuples are separate records and linearly deduplicated |
| Derivation | ordinary accepted premises use exact Claim IDs | local premises duplicate kind, Context, subject, and classifier in parallel fixed-size arrays |
| Artifact append | explicit relocation validates graph references | relocation depends on numeric offsets and reconstructs all Context/Substitution nodes rather than typed-key import |

The audit also found a more important semantic duplication:

| Phenomenon | Structural authority | Separate proof/check path | V3 decision |
| --- | --- | --- | --- |
| Context weakening | `PROTOTYPE_SUBSTITUTION_PROJECTION` and composition in `SubstitutionDB` | `CONTEXT_WEAKEN` stores only its source premise; `validate_context_weaken_proof()` walks Context parents and does not receive `SubstitutionDB` | retain the proof rule, but make it reference the exact projection/composition and validate through that substitution |
| Dependent family instantiation under a binder | projection passed to `instantiate_pure_family_in_context()` | Lambda/operation-request validators create the projection again while replaying a proof | use the same canonical substitution action key; do not reconstruct an unrecorded second authority |
| Typed substitution extension | `EXTEND` node plus its term/classifier fields | `prototype_substitution_certificate_db` separately pairs the substitution with the accepted typing Claim | keep certificate and morphism distinct, but make this pair the standard authority-edge pattern rather than a HOTT-only side database |
| Context formation | Context extension node | HOTT context-formation certificate separately pairs it with an `IS_TYPE` Claim | retain the distinction; integrate the pair into the common certificate-edge model |
| Conversion exposure | kernel conversion result | `CONVERSION` and `EXPECTED_TYPE_EXPOSURE` both replay compatibility, with the latter additionally allowed to discharge solver variables | keep the rules distinct only if their action/request records are distinct; neither may silently recompute a different conversion profile |
| Effect weakening | effect-row inclusion constraint | `EFFECT_WEAKEN` recomputes closed-bit inclusion from classifiers | audit after Context weakening; either reference an exact solved inclusion constraint or explicitly classify this as a deterministic derived check with no persistent structural object |

This table does not claim that every pair must become one tag. The defect is
two authorities, not two representations. The accepted forms are:

1. one structural node plus a proof edge to that node;
2. one canonical action-result node plus a proof edge to that action; or
3. a purely derived check with no competing persistent semantic object.

Storing two persistent objects which can disagree, or reconstructing an
unrecorded operation during certificate validation, is forbidden.

### 26.4 Binding and Context boundary

V3 retains direct opaque `binding_id` handles and does not adopt De Bruijn
indices. It also does not identify a Context extension with the erased Lambda
representative selected by TermDB alpha interning.

The exact Context key is:

```text
ContextEmpty
ContextExtend(parent_context, binding_id, classifier_reference)
```

`classifier_reference` is one explicit tagged value selecting either an exact
TermDB classifier or one unresolved solver variable. The current two parallel
fields must not survive the migration.

Different binding objects remain different Context extensions even when their
classifiers are alpha-equal. This preserves typed occurrence and resource
identity. Repeating the exact key returns the same Context ID.

### 26.5 Substitution and weakening boundary

The exact structural Substitution key remains typed by source and target:

```text
Identity(Gamma)
Empty(Delta)
Projection(Gamma.A, Gamma)
Extend(sigma : Delta -> Gamma, t : A[sigma])
Compose(delta, sigma)
```

Safe constructor simplifications such as identity composition are permitted.
General extensional equality of substitutions is not graph identity and is not
introduced by G1.

Weakening is not a second graph transformation. It is reindexing along an exact
projection Substitution. A weakening Derivation retains its source Claim and
projection/reindex authority; it must not store another independently writable
copy of that relationship. Concretely, the current API
`prototype_judgement_delta_record_context_weaken(delta, source_evidence)` is
insufficient because it cannot identify the morphism used. Its replacement
accepts an exact `substitution_id` (or an interned context-action ID which owns
that substitution), verifies the source and target Contexts, and records that
ID in the Derivation's rule-data edge.

`validate_context_weaken_proof()` must consequently receive `SubstitutionDB`.
The existing ancestor walk is retained only as a Context database integrity
check, not as the proof of weakening. Multi-level weakening is represented by a
canonical composition of projections, not by omitting the path from the proof.

### 26.6 Pullback Context action

Reindexing a dependent telescope requires the CwF comprehension pullback:

```text
sigma : Delta -> Gamma
Gamma.A
---------------------
Delta.A[sigma]
```

The current fresh-extension helper computes this structure operationally.
V3-G1 replaces it with an interned action keyed by the exact source extension
and Substitution. The result owns:

```text
pullback_context_id
lifted_substitution_id
new_binding_id, only when the key is first materialized
```

Repeated action on the same key returns the same result. A genuinely different
source binding or Substitution remains distinct.

### 26.7 Judgement proposition and premise graph

V3-G1 introduces one immutable proposition identity shared by solver
candidates and accepted Claims:

```text
JudgementProposition(
  kind,
  authority,
  context_id,
  operation_id_or_invalid,
  subject_term_id,
  classifier_term_id
)
```

Candidate state and accepted status remain separate. An accepted Claim points
to one proposition ID and closure metadata. One proposition may still have
multiple accepted Derivations.

Derivation premises become an edge arena. Every edge points to a proposition;
an independently accepted premise additionally points to its exact Claim. A
rule-local scoped premise has no accepted Claim but no longer copies its tuple
into every Derivation. Rule order and role remain explicit on the edge.

This preserves the P0/P1 boundary: compiler search does not become accepted
proof, and a local rule parameter does not become a publishable global Claim.

### 26.8 Resource-ready boundary

G1 does not implement a linear type system, but its identities must support one
without another Context rewrite.

The dependent telescope and usage demand remain separate:

```text
Gamma                         structural dependent Context
U : BindingID -> Grade        usage environment
Gamma ; U |- operation : A    resource-aware future Judgement
```

Usage is not part of erased TermDB identity. Declaration capability and actual
derivation demand are also distinct. A later usage graph may use the same
Binding IDs and semiring operations for zero, one, addition, and unrestricted
use. Projection exists structurally; whether weakening through it is admissible
for a resource-sensitive judgement is a separate checked premise.

### 26.9 Artifact policy

G1 first stabilizes in-memory typed keys and validators. If Context,
Substitution, Claim, or Derivation wire records change, V3 performs one explicit
artifact schema break after the in-memory migration. It does not keep v68
compatibility readers, duplicate legacy fields, offset-authority fallbacks, or
old/new interning paths.

Hashes accelerate candidate lookup. Artifact import must replay the full typed
key and validators before accepting identity.

### 26.10 V3 progress table

| ID | Item | Status | Required evidence |
| --- | --- | --- | --- |
| V3-G1.0 | Inventory duplicated semantic authorities and freeze ownership decisions | complete locally | 32-rule matrix; instrumentation counters move with G1.2 |
| V3-G1.1 | Connect `CONTEXT_WEAKEN` to exact projection/action authority | complete locally | one-/multi-level projection, Context relocation, v69 round trip, forged-path rejection, all 16 scripts |
| V3-G1.2 | Add typed hash-cons indices and explicit key APIs | complete locally | colliding distinct Context, Substitution, Claim, and Derivation keys remain distinct |
| V3-G1.3 | Migrate Context classifier references and extension interning | complete locally | provisional classifier provenance survives iteration and resolves before publication |
| V3-G1.4 | Migrate Substitution interning and constructor simplification | complete locally | category identity/composition and typed validation pass |
| V3-G1.5 | Replace repeated reindex work with memoized action | complete locally | exact reindex requests hit a non-authoritative cache; shared-term laws pass |
| V3-G1.6 | Memoize comprehension pullback actions | complete for an uncollided cache entry; general collision-stable interning is O1.1 | repeated exact warm-cache pullback allocates no fresh binding; collision-forcing identity test remains O1.1 |
| V3-G1.7 | Intern Judgement propositions, derivations, and semantic action edges | complete locally | multiple derivations retained; premise Claim IDs are direct DAG edges; duplicate source list removed |
| V3-G1.8 | Resolve conversion/effect/context certificate duplication audit | audit complete | deterministic derived checks documented; UniverseDB inequality witness remains independent kernel work |
| V3-G1.9 | Freeze resource-ready usage boundary | complete locally | usage remains outside TermDB and Context identity |
| V3-G1.10 | Migrate artifact schema | complete locally | v69 strict read/write/link/forgery matrix; no compatibility path |
| V3-G1.11 | Exit audit and O1 handoff | complete locally | strict builds, all 16 scripts, deterministic artifacts |

### 26.11 V3 stop conditions

Stop and revise G1 if an implementation requires:

- identifying typed Contexts by erased TermDB alpha shape;
- using a hash collision as identity evidence;
- putting source names or lexical depths back into graph identity;
- adding Context, Substitution, or proof tags to TermDB;
- adding separate value/computation APP, Lambda, Pi, or Match graphs;
- making solver state part of immutable proposition identity;
- treating resource grades as erased computation identity;
- treating projection as automatic permission to discard a linear resource; or
- validating a proof by recreating a semantic operation which has an existing
  authoritative graph node but is not referenced by that proof; or
- retaining the old implementation as a compatibility fallback.

## 27. 2026-08-09 V3-SC1 Pre-A1 Semantic Consolidation

### 27.1 Why this stage is mandatory

The post-O1 code audit found that G1 completed semantic Claim edges and exact
Substitution authority, but did not physically complete every compact graph
representation described in section 26. In particular, the current code still
contains:

- duplicated candidate and accepted proposition tuples;
- maximum-sized parallel premise arrays in candidate and accepted
  Derivations;
- Context-formation certificates owned by HOTT and Substitution certificates
  owned by Judgement;
- repeated Context/Substitution/Term/Judgement store bundles in HOTT APIs;
- repeated reindex proof wrappers over one structural Substitution action; and
- duplicated deterministic outcome fields in HOTT work and action results.

Publishing these layouts in A1 would turn implementation history into a new
artifact contract and make later deletion more expensive. V3 therefore inserts
one mandatory consolidation stage:

```text
V3-G1 -> V2-O1 -> V3-SC1 -> V3-PC1 -> V2-A1
```

### 27.2 Scope correction

V3-SC1 is not a universal graph rewrite. It retains separate Context,
Substitution, Term, Operation, proposition, Claim, Derivation, HOTT, and
runtime sorts. It consolidates only implementation paths with the same
mathematical authority.

The exact Substitution remains the authority for reindexing. No persistent
`ReindexAction` wrapper is added. Weakening and general substitution remain
different proof rules over the same structural replay operation.

Candidate state, accepted Claims, and Derivations also remain distinct. They
share one immutable proposition identity and ordered premise-edge discipline;
one Claim may retain multiple accepted Derivations.

### 27.3 Artifact and resource boundary

SC1 keeps v69 externally stable through one expanded-tuple adapter over the
new compact in-memory premise graph. A1 removes that adapter and performs the
single permanent v70 migration together with object-HOTT publication.

Runtime environment copying and resource-sensitive typing are deferred. Usage
grades remain outside Context, Term, Substitution, proposition, and reindex
cache identity. Structural projection remains distinct from permission to
discard a resource.

### 27.4 Required quantitative evidence

SC1 completion requires an actual per-file report of baseline lines, added
lines, deleted lines, final lines, and the semantic path removed. It also
measures fixed premise arrays, duplicated proposition layouts, certificate DB
implementations, repeated HOTT store parameters, and structural reindex paths.

Net LOC alone is insufficient. A phase passes only after its old semantic path
is deleted. Independent validators and malformed-input tests are retained even
when they increase line count.

### 27.5 Detailed plan

The implementation phases, progress table, validation matrix, stop
conditions, and mandatory deletion accounting are in:

`doc/2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`.

### 27.6 Encoding boundary for rule consolidation

SC1 does not collapse `APP_ELIM`, `MATCH_ELIM`,
`INDUCTION_HYPOTHESIS_ELIM`, or `COMPUTATION_FOLD_ELIM` into one generic
Lambda/App rule. Their computations may be encodable in a shared core, but
their typed premises and kernel theorems remain distinct. Section 3.9 of the
SC1 plan records the full decision and the infrastructure that may still be
shared.

### 27.7 Next implementation stage

The post-SC1 physical audit found one persistent representation issue which
must be resolved before v70: accepted scoped premises still copy Proposition
tuples, and Claims cache pointers in addition to Proposition IDs. V3-PC1
normalizes those persistent references while retaining v69 through the one
existing adapter.

Its implementation and progress plan is:

`doc/2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`.

V2-A1 follows PC1. Artifact v70 then publishes the compact
proposition/premise graph and O1 observation roots without collapsing
rule-specific eliminators into a generic Lambda/App derivation.
