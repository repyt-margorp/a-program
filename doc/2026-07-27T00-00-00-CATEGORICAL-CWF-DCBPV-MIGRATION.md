# Categorical CwF and dCBPV Migration

Date: 2026-07-27

Status: completed for the current language fragment

This document is the authoritative progress record for the migration. Update
the checklists and implementation evidence whenever a phase changes. A phase is
complete only when its acceptance tests pass and its obsolete implementation
has been removed. Prototype compatibility is not a goal.

## 1. Objective

Make the categorical semantics of A Program explicit in both the compiler
architecture and its data structures while preserving the existing shared
computation graph.

The target architecture combines:

- a category with families (CwF) for local contexts, substitutions, dependent
  value types, and terms;
- an indexed dependent CBPV layer for value and computation judgements;
- an erased shared computation graph for canonical program structure and pure
  normalization;
- graph-level telescopes shared by dependent constructor fields and dependent
  function families;
- distinct semantic structures for constructor introduction and Pi
  abstraction/application;
- an artifact format that preserves every context-indexed exported judgement
  and schema.

The migration must not make runtime environments part of TermDB. Pure
normalization and static reindexing may add canonical TermDB nodes.

## 2. Semantic Contract

### 2.1 Signature and local context are different

Judgements have the conceptual form:

```text
Sigma ; Gamma |-v V : A
Sigma ; Gamma |-c M : C
```

`Sigma` contains namespaces, global definitions, type declarations, intrinsic
declarations, and imported interfaces. It is a compiler/linker signature.

`Gamma` contains local value variables. It is an object in the syntactic
category of contexts.

An imported or global name is not a local context entry. Namespace resolution
must not be implemented as local substitution.

### 2.2 Contexts are objects and substitutions are morphisms

A substitution has the orientation:

```text
sigma : Delta -> Gamma
```

It assigns a term in `Delta` to every variable declared by `Gamma`. Reindexing
is contravariant:

```text
Gamma |- A type       => Delta |- A[sigma] type
Gamma |- t : A        => Delta |- t[sigma] : A[sigma]
```

The implementation must expose:

```text
id_Gamma                     : Gamma -> Gamma
empty_Delta                  : Delta -> empty
p                            : Gamma.A -> Gamma
<sigma, t>                   : Delta -> Gamma.A
sigma o tau                  : Theta -> Gamma
```

where `sigma : Delta -> Gamma`, `tau : Theta -> Delta`, and
`Delta |- t : A[sigma]`.

Single-binder substitution is a derived operation:

```text
t[u/x] = t[<id_Gamma, u>]
```

It is not the authoritative substitution representation.

### 2.3 Types and terms are indexed by context

A type is not merely an object in the context category. A type
`A in Ty(Gamma)` determines a context extension and display map:

```text
p_A : Gamma.A -> Gamma
```

A term `Gamma |- a : A` determines a section:

```text
<id_Gamma, a> : Gamma -> Gamma.A
p_A o <id_Gamma, a> = id_Gamma
```

JudgementDB relations and solver constraints must therefore carry an explicit
context ID. Context must not be reconstructed from a lambda or match node after
the fact.

### 2.4 Core graph sharing is an erasure, not context equality

The shared TermDB remains context-erased:

```text
erase : TypedOperation -> CoreTerm
```

For example, the typed occurrences:

```text
identityBool : Bool -> Bool
identityNat  : Nat -> Nat
```

may erase to one core identity lambda. Their operation occurrences, contexts,
classifiers, TypeViews, proofs, and names remain distinct.

Consequences:

- `context_id` must not be part of the TermDB structural interning key;
- a core Term ID is not sufficient to select a classifier;
- normalization may share results across typed occurrences only under an
  appropriate reduction profile;
- the typed operation graph is the source of context-sensitive meaning.

### 2.4.1 Graph construction is indexed syntax

Computation-graph construction is part of the categorical architecture, not a
pre-categorical preprocessing step:

```text
surface AST plus resolved signature
  -> OperationGraph occurrence in Gamma
  -> classifier constraints in Gamma
  -> erase occurrence
  -> canonical TermDB node
```

The AST itself is not a context-category object: it still contains unresolved
names, source spelling, and parser scope. After name resolution, every lowered
operation occurrence is constructed with an existing `ContextDB` object ID.
Lambda and Match lowering extend that object; APP keeps the outer object; case
bodies use the instantiated constructor-field telescope.

`prototype_operation_graph_add` and
`prototype_operation_graph_add_case` reject nonexistent contexts. The same
constructors are used by source lowering, artifact readback, and linking.
Consequently there is no context-free operation insertion path.

Erasure into TermDB is many-to-one. It is valid for two operation occurrences
in different contexts to point to the same core node. This is how graph
construction preserves both the categorical indexing and the intended
tagless computation sharing.

### 2.5 Graph edges and categorical morphisms are not identical

TermDB child references are graph implementation edges. They are not all
categorical morphisms.

- substitutions are morphisms of the context category;
- APP is the elimination/evaluation structure of Pi;
- LAMBDA is Pi introduction;
- MATCH is inductive elimination;
- BIND is computation sequencing/Kleisli extension;
- beta, iota, and CBPV cut reduction are equations between parallel typed
  terms, implemented as graph rewriting and normalization.

The compiler must not describe APP, MATCH, or every TermDB edge as generic
categorical composition.

### 2.6 Indexed dCBPV

Local contexts contain value variables. Over each context `Gamma`, the compiler
tracks at least:

```text
TyV(Gamma)       value classifiers
TyC(Gamma)       computation classifiers
TmV(Gamma, A)    value terms
TmC(Gamma, C)    computations
```

`RETURN/F`, `THUNK/U`, Pi, BIND, effects, and handlers are structure indexed by
the same value context. Effectful execution is not a context substitution.

Residual dependent computations remain occurrence-local verification
obligations. A dynamic computation result must not be inserted into a static
context as though it were already a value term.

## 3. Telescope Contract

### 3.1 Shared structural concept

A telescope is an iterated dependent context extension:

```text
Gamma
Gamma.A
Gamma.A.B
Gamma.A.B.C
```

Constructor fields, type former parameters, Pi binders, match case binders, and
handler clause binders all require the same structural operations:

- extend a context by a classifier;
- project to a parent context;
- substitute/reindex every later classifier;
- instantiate the whole telescope with a dependent tuple;
- validate that every free local variable belongs to the telescope prefix.

These operations must be implemented once by ContextDB and SubstitutionDB.

### 3.2 Constructor semantics

A constructor declaration has:

```text
owner type former parameter_context
field_context over parameter_context
result_classifier over field_context
constructor ordinal and source view
```

For a dependent constructor such as:

```text
mk : (a : A) -> B(a) -> Sigma(A, B)
```

the second field classifier is stored in the context extended by the first
field. It must not be reconstructed from flat readback metadata.

A fully applied constructor is a positive value introduction. Constructor
application and inductive iota reduction remain constructor-specific semantic
operations.

### 3.3 Pi semantics

Pi uses the same one-step telescope/context extension:

```text
Gamma |- A type
Gamma.A |- C computation-type
--------------------------------
Gamma |- Pi(A, C) computation-function-type
```

Lambda and APP remain Pi-specific introduction and elimination. Pi is not
replaced by a constructor telescope, and a constructor is not reclassified as
a runtime Pi function merely because its surface syntax is curried.

The shared implementation is context extension and reindexing. The distinct
implementation is formation/introduction/elimination/computation rules.

### 3.4 Match semantics

Each case owns a context extending the outer match context by that
constructor's instantiated field telescope. A synthesized motive is a family
over the scrutinee context. Case equations and induction-hypothesis constraints
must carry these context IDs.

The classifier of a Match remains motive application. A uniform result may
normalize to a constant classifier, but uniformity is not a separate typing
rule.

## 4. Target Data Structures

The following structures are implemented; their roles are fixed.

### 4.1 ContextDB

```c
struct prototype_context {
	uint32_t parent;
	uint32_t binder_id;
	uint32_t classifier;
	uint32_t classifier_variable;
	uint32_t depth;
};

struct prototype_context_db {
	struct prototype_context* contexts;
	size_t context_count;
	size_t context_capacity;
};
```

The empty context has a stable ID. Extensions are immutable and interned.
`binder_id` is the core handle for the variable projection introduced by this
context extension, not source spelling or global alpha identity.

Source symbols and spans remain occurrence metadata and do not participate in
semantic ContextDB interning.

### 4.2 SubstitutionDB

Use an inductive representation rather than a flat array whose dependent
well-formedness is implicit:

```text
IDENTITY(context)
EMPTY(source_context)
PROJECTION(extended_context)
EXTEND(prefix_substitution, term, term_judgement)
COMPOSE(outer, inner)
```

Every substitution stores explicit source and target context IDs.

The first implementation may normalize substitutions eagerly to extension
chains, but identity and composition remain visible API operations and law
tests.

### 4.3 Context-indexed judgements

Every relation and proof conclusion carries:

```text
signature identity or artifact unit
context_id
judgement sort: value or computation
subject
classifier
proof
```

Proof premises refer to relation IDs or full context-indexed conclusions.
Remove `context_subject`, `context_index`, and `context_aux` after all producers
and validators use explicit contexts.

### 4.4 Context-indexed operations and constraints

Each operation occurrence carries `context_id`. A Lambda body, Match case body,
deep-fold clause, and handler clause has a derived extended context.

Every classifier/effect/motive/IH constraint carries the context in which its
terms are meaningful. A solver binding is not valid outside that context unless
explicitly reindexed.

### 4.5 Graph-level constructor schema

Replace semantic use of flat `field_type_exprs` and `result_type_expr` with:

```text
parameter_context
field_context
result_classifier
```

Readback source expressions may remain only as diagnostics until graph-level
pretty-printing can replace them. They must never be used for typing, shape
identity, linking, or reconstruction.

## 5. Current Implementation Gaps

The following observations are from the 2026-07-27 worktree.

### 5.1 Context migration status

ContextDB is now the semantic representation of `Gamma`. The mutable binder
stack remains only as a lowering cursor and source-name lookup mechanism.
Operation occurrences, operation cases, judgement relations, proof conclusions,
and proof premises carry Context IDs.

Binder-assumption proofs use `assumption_index`, a De Bruijn level in their
conclusion Context. They no longer identify a local variable through Lambda or
Match source provenance. Match-pattern and IH proofs retain only their
constructor/match-specific semantic coordinates.

### 5.2 Core binder identity remains erased

`push_graph_binder` still obtains a tagless core binder by scope slot. This is
intentional erasure. Context coordinates, not core binder IDs, distinguish
typed local variables.

### 5.3 Typed substitution and erased graph rewriting are separated

`SubstitutionDB` implements identity, empty, projection, extension, and
composition with explicit source and target Context IDs.
`prototype_term_reindex` is the typed reindexing operation.

`prototype_term_graph_substitute_bound_var` remains as an erased TermDB
rewriting primitive used by beta reduction and alpha transport. It does not
represent a context morphism and is named accordingly.

### 5.4 Pi application uses context substitution

Pi codomain instantiation constructs `<id_Gamma, argument>` and reindexes the
codomain family. APP proof construction and validation use the same operation.
CBPV F/U wrappers remain only where they are part of the classifier.

### 5.5 Judgements now include Gamma

`prototype_judgement_relation` and every proof conclusion/premise now carry
Context IDs. Proof validation checks binder assumptions by Context coordinate.
Artifact format 52 introduced this proof representation; format 53 additionally
stores Match operation-case Context IDs.

### 5.6 Constructor schema has one authority

Constructor declarations expose `parameter_context`, `field_context`, and
`result_classifier` as their semantic schema. Shape keys, exact representation
comparison, field typing, Match resolution, and Nat-shape recognition traverse
that telescope.

`curried_classifier_cache` is rebuilt from the telescope after source
elaboration, artifact readback, relocation, and TypeView canonicalization. It
exists only to feed current Pi consumers and is validated against its source
schema. The stored type-former `parameter_count` is likewise a readback/index
cache whose equality with `parameter_context.depth` is validated.

### 5.7 Artifact context support

Artifact format 53 serializes and relocates ContextDB and SubstitutionDB.
Context classifier external references are resolved during linking.
Operation-case Context IDs are relocated together with operation occurrences.
Constructor caches are rebuilt only after those contexts and term references
have reached their final linked identities.

## 6. Migration Phases

### Phase 0: Baseline and semantic law harness

Status: completed

- [x] Record the current architecture and target semantic contract.
- [x] Confirm `make` succeeds on the starting worktree.
- [x] Confirm artifact flow tests pass on the starting worktree.
- [x] Build `read_file.out` and run every prototype regression.
- [x] Add direct ContextDB law tests before replacing existing consumers.
- [x] Record all starting failures as baseline defects (none after
      `read_file.out` was built).

Evidence:

```text
2026-07-27: make passed with -Wall -Wextra.
2026-07-27: test_artifact_flow.sh passed.
2026-07-27: test_cbpv_surface.sh was not run in the first command because
            read_file.out had not yet been built.
2026-07-27: built read_file.out and passed test_artifact_flow.sh,
            test_cbpv_boundary.sh, test_cbpv_surface.sh,
            test_dependent_pi.sh, and test_shared_core_occurrences.sh.
2026-07-27: added test_context_category.sh for empty/extend interning,
            classifier separation, unresolved-meta separation, ancestry, and
            structural validation.
```

### Phase 1: Introduce persistent ContextDB

Status: completed for proof and operation scope

- [x] Add `context.h` under `src/prototype`.
- [x] Keep the implementation in `ast.c` under the current build boundary.
      Moving it to `context.c` is source-layout-only and would require an
      accepted-build `Makefile` change; it is not a semantic migration item.
- [x] Add immutable empty/extend/query operations.
- [x] Add stable context interning.
- [x] Add `context_id` to operation occurrences.
- [x] Add `context_id` to judgement relations and proofs.
- [x] Move JudgementDelta proof storage out of `compile_context` before adding
      premise context IDs; the current 4096 by 65 premise arena is already
      close to the stack budget.
- [x] Add `context_id` to operation classifier and computation constraints.
- [x] Derive child contexts for Lambda, Match, deep fold, and handler clauses.
- [x] Validate local variable assumptions through context-indexed proofs.
- [x] Keep Context IDs out of TermDB canonical keys.

Removal gate:

- [x] Remove proof inference that reconstructs scope solely from
      `context_subject/context_index/context_aux`.

### Phase 2: Introduce first-class substitutions and reindexing

Status: completed

- [x] Add immutable SubstitutionDB with source and target contexts.
- [x] Implement identity, empty, projection, extension, and composition.
- [x] Implement simultaneous `prototype_term_reindex`.
- [x] Reindex constructor field classifiers under explicit contexts.
- [x] Preserve Match/IH typed scope through Context and Substitution IDs.
- [x] Replace typed single-binder substitution with simultaneous reindexing.
- [x] Rename the remaining erased rewrite primitive so it cannot be confused
      with categorical substitution.

Required laws:

- [x] `t[id] == t`.
- [x] `t[sigma][tau] == t[sigma o tau]` for the implemented extension fragment.
- [x] `p o <sigma,t> == sigma` structurally.
- [x] `q[<sigma,t>] == t`.
- [x] Dependent classifier reindexing validates `t : A[sigma]`.

### Phase 3: Migrate Pi, Lambda, and APP

Status: completed for the current pure-family fragment

- [x] Represent Pi domain in `Gamma` and codomain in `Gamma.A`.
- [x] Replace `pi_codomain_after_argument` with reindexing by
      `<id_Gamma, argument>`.
- [x] Context-index Lambda introduction premises.
- [x] Context-index APP elimination premises and conversion checks.
- [x] Make Pi substitution stability an explicit test.
- [x] Remove `PROTOTYPE_PI_UNUSED_BINDER_ID`.
- [x] Keep CBPV family wrappers only where they represent real F/U structure,
      not as a substitute for missing context data.

### Phase 4: Migrate constructor schemas to telescopes

Status: completed

- [x] Store type parameters as a context telescope.
- [x] Store the parameter telescope on the type former itself, including
      constructor-free types.
- [x] Store every constructor field as a context extension.
- [x] Store constructor result classifier over the full field context.
- [x] Instantiate Match case fields through a substitution into the field
      context.
- [x] Generate a derived curried classifier cache from the context telescope
      for existing Pi consumers.
- [x] Make constructor ordinal ownership remain TypeView-sensitive.
- [x] Resolve Match field counts from the telescope rather than by decoding
      the curried cache.
- [x] Validate the Nat-shaped primitive boundary from constructor telescopes.
- [x] Update structural representation keys from graph-level schema.
- [x] Remove semantic reads of field/result readback metadata.
- [x] Rebuild and validate the derived curried classifier cache at every graph
      relocation boundary.

Required examples:

- [x] Nat and Bool nullary/unary constructors.
- [x] `List A` with parameter reindexing.
- [x] Sigma-like `mk : (a:A) -> B(a) -> *`.
- [x] Two declarations sharing erased shape but retaining distinct TypeViews.

### Phase 5: Migrate Match, motive, and IH contexts

Status: completed for the current guarded structural-recursion fragment

- [x] Instantiate each case field telescope in a case context.
- [x] Generate motive equations under explicit case contexts.
- [x] Store the semantic case telescope as an OperationGraph Context ID.
- [x] Remove TermDB/AST binder-range copies from solver motive equations.
- [x] Reindex branch classifiers from source case contexts into fresh motive
      case contexts.
- [x] Represent IH classifier applications as solver equations owned by an
      explicit Match operation and case Context.
- [x] Remove copied binder arrays and duplicate case Context fields from motive
      equations.
- [x] Preserve current direct guarded IH behavior.
- [x] Add dependent-field and recursive dependent-Match regressions.

### Phase 6: Align indexed dCBPV

Status: completed for implemented CBPV forms

- [x] Index value/computation operation occurrences by the same value Context.
- [x] Derive judgement sort from the classifier view rather than storing a
      second mutable sort field.
- [x] State F/RETURN and U/THUNK rules over the same value context.
- [x] State deep-fold/BIND premises using context-indexed continuations.
- [x] Keep effect rows separate from context morphisms.
- [x] Keep residual dependent execution evidence occurrence-local.
- [x] Verify Pi/APP normalization and conversion under value substitution.

### Phase 7: Artifact and linker migration

Status: completed

- [x] Increment the prototype artifact format without a compatibility reader.
- [x] Serialize reachable ContextDB entries.
- [x] Serialize substitutions referenced by exported schemas, proofs, or
      residual obligations.
- [x] Relocate context and substitution IDs during linking.
- [x] Resolve external references occurring in Context classifiers.
- [x] Serialize and relocate Match operation-case Context IDs (artifact v53).
- [x] Serialize and relocate type-former parameter Contexts.
- [x] Validate source/target context closure after relocation.
- [x] Resume proof and constraint validation only after context-correct imported
      facts and constructor caches are rebuilt.
- [x] Retain only binder/frame relocation required by the erased core graph;
      typed scope relocation uses ContextDB/SubstitutionDB.

### Phase 8: Cleanup and completion audit

Status: completed

- [x] Remove obsolete context provenance fields.
- [x] Separate erased bound-variable graph rewriting from typed substitution.
- [x] Remove flat constructor metadata from semantic decisions.
- [x] Remove duplicate constructor/Pi schema reconstruction helpers.
- [x] Update prototype README and theory reading map.
- [x] Run all prototype tests and examples 01 through 09.
- [x] Run artifact round-trip and linking tests.
- [x] Run a clean warning-free build.
- [x] Audit every checklist item against code and test evidence.
- [x] Commit and push the completed migration to `main`.

## 7. Test Matrix

### Core sharing

- typed Bool and Nat identities share the erased lambda;
- their operation contexts and classifiers remain distinct;
- reindexing does not add context identity to the core structural key.

### CwF laws

- identity and composition for substitutions;
- context projection and extension beta laws;
- context extension eta law where represented;
- type/term substitution stability.

### Pi

- dependent identity;
- a codomain motive reduced after APP;
- nested Pi reindexing;
- artifact round-trip of an exported dependent function.

### ADT and telescope

- List parameter instantiation;
- dependent second constructor field;
- Match case context instantiation;
- recursive IH under a field context;
- distinct TypeViews over one erased representation.

### CBPV

- RETURN, THUNK, FORCE, APP, BIND, and deep fold preserve contexts;
- pure normalization commutes with reindexing;
- effectful residual obligations do not become static substitutions;
- handler clause binders use explicit contexts.

### Artifact

- every exported judgement references a serialized context;
- every referenced substitution has valid source and target contexts;
- linking preserves substitution composition and constructor schemas;
- sparse graph slicing preserves all context/schema dependencies.

## 8. Deletion Policy

No compatibility layer is required. An old API may remain temporarily only
inside one migration phase and only when this document identifies its removal
gate.

Do not keep:

- old and new context provenance as permanent parallel authorities;
- flat constructor metadata and graph telescope as parallel semantic sources;
- single-binder and simultaneous substitution as parallel public APIs;
- context-indexed and context-free judgement insertion paths;
- old artifact readers after the new writer is authoritative.

## 9. References

- Peter Dybjer, *Internal Type Theory*.
- Simon Castellan, Pierre Clairambault, and Peter Dybjer,
  *Categories with Families: Unityped, Simply Typed, and Dependently Typed*.
- Paul Blain Levy, *Call-By-Push-Value: A Functional/Imperative Synthesis*.
- Matthijs Vakar, *A Framework for Dependent Types and Effects*.

The implementation target is a strict syntactic CwF because compiler
substitution must obey chosen laws on the nose. External categorical models may
preserve this structure only up to coherent isomorphism; that coherence problem
is not represented by silently weakening compiler invariants.

## 10. 2026-07-27 Implementation Checkpoint

The current implementation preserves the original graph-first architecture:

```text
surface AST
  -> typed OperationGraph indexed by ContextDB
  -> shared context-erased TermDB
  -> context-indexed constraints and JudgementDB proofs
  -> artifact slices containing reachable terms, contexts, substitutions,
     schemas, operations, and proofs
```

TermDB remains the canonical graph of how computation proceeds. ContextDB and
JudgementDB do not replace it; they make explicit which typed occurrence in
which local context selects a shared core node. Runtime environments remain
outside every serialized graph.

Implemented in this checkpoint:

- context extension is canonical by `(parent, classifier)` after classifier
  solving, rather than by source binder identity;
- AST lowering inserts every operation and Match case through the public
  Context-validating OperationGraph API;
- distinct operation occurrences may share one erased TermDB node while
  retaining different Context IDs and classifiers;
- unresolved context extensions retain a solver variable until resolution;
- APP, Lambda, Match, CBPV boundaries, operation requests, deep folds, and
  handler clauses materialize context-indexed derivations;
- computation constraints retain the context of their source operation;
- proof reification distinguishes deferred premises from invalid derivations;
- substitutions validate extension terms against `A[sigma]`, not raw `A`;
- Match field typing uses constructor parameter/field contexts and explicit
  substitutions, including dependent second fields;
- Match motive branch classifiers are reindexed from operation-case Contexts;
- artifact version 53 serializes and relocates context and substitution
  closure, including operation-case Context IDs;
- constructor structural keys and exact comparison traverse semantic
  telescopes; `curried_classifier_cache` is a checked derived projection;
- context, artifact, dependent Pi, shared-core, CBPV surface, and examples
  01-09 regressions pass.

Deliberate implementation boundaries:

- `ContextDB` and `SubstitutionDB` declarations now live in `context.h`, but
  their implementation remains in `ast.c` until the accepted build may add
  `context.c`;
- TermDB beta reduction and proof readback still need erased binder/frame graph
  rewriting. This is exposed under the explicitly non-categorical name
  `prototype_term_graph_substitute_bound_var`;
- readback field/result expressions remain serialized only for diagnostics and
  source-oriented printing; semantic consumers use Context telescopes;
- generic TermDB proof validation may alpha-transport already-erased Match
  cases. Source motive synthesis itself uses ContextDB/SubstitutionDB;
- the current IH solver covers direct guarded structural recursion. General
  IADT index refinement and unrestricted higher-order unification are future
  language features, not silently accepted by this migration.

Checkpoint verification:

```text
2026-07-27: make and make reader passed with -Wall -Wextra.
2026-07-27: every src/prototype/test_*.sh script passed sequentially.
2026-07-27: examples 01-07 and 09, the numbered examples currently present
            through 09, compiled successfully.
2026-07-27: dependent constructor field and artifact round-trip checks passed
            after Match field instantiation moved to telescope substitutions.
2026-07-27: artifact version 53 preserves the constructor owner TypeView used
            by Match-pattern proof rules even though the shared Match core
            erases its owner to the canonical representation.
2026-07-27: APP candidate synthesis, APP proof refresh, Lambda checking, and
            proof validation all instantiate Pi codomains by context
            substitution rather than a context-free binder replacement.
2026-07-27: constructor field count, field typing, and Nat-shape validation
            read the constructor telescope. The curried classifier cache is
            generated from that telescope rather than independently from AST
            field arrays.
2026-07-27: source lowering, artifact readback, and linking all reject
            OperationGraph entries whose Context IDs are absent.
2026-07-27: motive synthesis clones the source case telescope by a CwF
            substitution and reindexes each branch classifier before building
            the motive Match.
2026-07-27: all prototype shell regressions, examples 01-07/09, dependent
            Match fixtures, artifact round-trip/link tests, and both `-Werror`
            CLI builds passed before publication to `main`.
```
