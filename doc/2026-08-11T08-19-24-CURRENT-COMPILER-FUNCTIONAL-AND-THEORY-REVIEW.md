# Current Compiler Functional and Theory Review

Date: 2026-08-11 08:19:24 JST

Repository: `a-program`

Reviewed revision: `54200d144b95c37ecb0aab42c5bb7a81d30682c1`

Scope: `src/prototype/src`, `src/prototype/include/a_program`, and the normative HOTT fragment schema

Status: review baseline and refactoring decision record; no implementation semantics are changed by this document

## 1. Purpose

The prototype is now a production-sized compiler implementation despite its directory name. The source split completed before this review made the physical layout substantially easier to inspect, but a directory tree alone does not establish semantic ownership. This review records:

1. what every current compiler implementation file does;
2. which database is authoritative for each semantic fact;
3. which apparent duplications are intentional proof-theoretic distinctions;
4. which duplications or dependency directions are genuine design debt;
5. how the implementation compares with CBPV, CwF, algebraic effects, proof certificates, and the intended Higher Observational Type Theory direction;
6. which similar concepts should be unified, which distinctions must remain visible, and which layers need one checked connecting map;
7. a staged refactoring order that does not silently change accepted programs, normalization, certificates, or artifact bytes.

This document complements, rather than replaces, the physical organization plan in
`doc/2026-08-11T08-10-57-PROTOTYPE-SEMANTICS-PRESERVING-CODEBASE-ORGANIZATION-PLAN.md`.

## 2. Review Baseline

At the reviewed revision:

- implementation consists of 45 `.c`/`.inc` files and 100,043 lines;
- the installed-style prototype API consists of 27 headers and 6,829 lines;
- four additional root/internal compiler headers provide fingerprints, shared AST allocation, and artifact-private contracts;
- `kernel/judgement.c` and `identity/hott.c` deliberately assemble partitions as single translation units;
- the ordinary prototype build and artifact reader build succeed;
- `tests/integration/test_p0_certificate_boundary.sh` passes, including rejection of a forged weakening derivation with its substitution action removed;
- `tests/integration/test_hott_goal.sh` passes, including reversed Derivation storage order and artifact-byte stability;
- the working tree also contains an untracked organization-plan document, which is not part of the reviewed Git revision;
- no source file was changed as part of this review.

The review is based on code ownership, call direction, stored identifiers, validation paths, and artifact behavior. File size is evidence of physical concentration, not by itself evidence of an incorrect abstraction. Every current compiler `.c`, `.inc`, and `.h` file is named below. Test programs, shell fixtures, archived wire schemas, and example `.p` programs are reviewed as coverage groups rather than file by file because they are not compiler implementation units.

## 3. Architectural Summary

### 3.1 Data flow

The current compiler follows this broad path:

```text
surface source
    |
    v
Reader / ASTDB
    |
    v
Lowering
    |-------------------------------|
    v                               v
OperationGraph                 TermDB
typed source occurrences       shared computational terms
    |                               |
    +---------- constraints --------+
                    |
                    v
        JudgementDelta / solvers
                    |
                    v
     Proposition -> Claim -> Derivation DAG
                    |
             UniverseDB / CwF
                    |
                    v
   parametric relation action and object Identity action
                    |
                    v
       artifact closure, relocation, wire v70
```

The important architectural distinction is not “typed graph versus untyped graph” alone. It is:

- `TermDB`: canonical/shared term representation and reduction substrate;
- `OperationGraph`: source-occurrence identity, context, selected classifier, and lowering intent;
- `JudgementDB`: accepted logical claims and derivations;
- compiler-local deltas and metadata: unfinished inference and residual work;
- artifact graph: an explicitly rooted, closed, relocatable persistent image.

### 3.2 Authority table

| Fact | Authoritative store | Derived or transient views |
| --- | --- | --- |
| Computational term shape | `TermDB` | AST nodes, operation projections, artifact relocation copies |
| Surface occurrence and selected classifier | `OperationGraph` plus compile metadata | classifier work cells and diagnostics |
| Lexical binding identity | `TermDB` binding IDs and CwF context entries | parser names and readback labels |
| Context/substitution structure | `ContextDB` / `SubstitutionDB` | weakening derivations and endpoint projections |
| Accepted proposition | immutable `Proposition` reached by a `Claim` | candidate propositions in `JudgementDelta` |
| Why a claim holds | `Derivation` DAG | solver search history, candidate work queue |
| Type declaration schema | `TypeDeclarationDB`; constructor telescope is semantic source | derived curried classifier cache, diagnostic field metadata |
| Universe constraints and solved levels | `UniverseDB` with provenance | universe variables occurring in terms |
| Occurrence effect inference | operation effect constraints and solved operation classifiers | judgement rule constraints used to derive them |
| Language effect operation identity | currently split between operation ID/row atom and fixed-width operation-label bit | host capability and oracle mappings are separate and should remain so |
| Resource usage | no authoritative store yet; current one-shot check counts OperationGraph binder occurrences | future per-judgement usage vectors and quantitative substitution actions |
| Conditional runtime type check | `VerificationDB` obligation | never an accepted `JudgementDB` proof until discharged |
| Compiler parametric relation action | relation action plan/certificate | not object-language equality |
| Object Identity evidence | object Identity terms, claims, derivations, rooted artifact evidence | relation-action scaffolding used to construct it |
| Persistent module meaning | rooted artifact closure plus accepted certificates | work queues, fuel consumption, debug traces |

## 4. Compile Pipeline Review

`frontend/lowering.c` currently orchestrates the following effective phases:

1. diagnose duplicate definitions;
2. lower source nodes and type declarations into `OperationGraph` and `TermDB`;
3. install the operation store into the judgement delta;
4. validate constructor saturation;
5. materialize imported constructor classifiers;
6. canonicalize type-former references;
7. resolve pending Match constructors and typing requests;
8. canonicalize references after resolution;
9. run the classifier fixed point;
10. generate, solve, and commit effect-row constraints;
11. reject strict unresolved classifier/effect states;
12. check ascriptions and expected classifiers;
13. reject remaining pending classifier state;
14. canonicalize references after solved substitutions;
15. expand primitive facts;
16. add conversion premises justified by normalization;
17. publish candidates and replay the accepted derivation graph;
18. validate operation typing and normalization-cache invariants;
19. erase constructor view owners from the core projection;
20. publish source labels and exports.

Repeated canonicalization is not automatically redundant. Each occurrence follows a phase that may create or replace graph references. The debt is that these barriers are expressed as repeated calls inside a 16,883-line orchestrator rather than named phase contracts with explicit preconditions and postconditions.

The current pipeline is therefore not merely “fixed-point type synthesis.” It performs graph construction, constraint generation, constraint solving, proof candidate generation, proof publication, accepted-kernel replay, and artifact-facing cleanup. Those stages exist conceptually but are not yet clean module boundaries.

## 5. Implementation File Review

### 5.1 Support

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `support/symbol.c` | 109 | FNV hashing, symbol interning, lookup, storage usage | Cohesive. It is infrastructure, not semantic name resolution. Hierarchical/qualified naming must remain above this byte-string interner. |

### 5.2 Core term substrate

#### `core/term.c` (10,027 lines)

This file is the computational center of the compiler. It currently contains all of the following:

- host type, pure primitive, effect operation, and intrinsic namespace registries;
- term semantic/classifier views;
- alpha-aware local and cross-database shape comparison;
- canonical term keys;
- `TermDB` initialization, interning, graph mutation revision, and relocated append;
- binding and induction-hypothesis scope allocation;
- constructors for variables, APP, Lambda, Pi families, Match, type formers/views/instances, IH, Universe variables, literals, relation forms, effect rows, CBPV forms, host primitives, and external references;
- term substitution and binding reindexing;
- external-reference resolution;
- reduction, WHNF/NF, conversion, normalization profiles, fuel, and normalization cache;
- host integer primitive evaluation and effect dispatch;
- term printing and debug readback.

The single `TermDB` remains a sound authority. APP/Lambda/Match/CBPV/Identity nodes should not be copied into separate value and computation databases merely to reflect polarity. CBPV polarity belongs to classifiers and term former rules; sharing a graph does not identify values with computations.

One semantic duplication does remain inside the effect representation. General rows can contain variables and `EFFECT_ROW_OPERATION` atoms, but closed intrinsic operations are accumulated in an `unsigned` bit mask and handler residuals are subtracted through that mask. The operation atom/ID should become the only language-level operation identity. Host-effect capability bits can remain because they answer a different backend-permission question.

The file is nevertheless physically overloaded. The safe split is by implementation concern while preserving the one database and one public term-tag space:

- `term_db.c`: allocation, interning, import append, mutation revision;
- `term_shape.c`: alpha-aware equality and canonical keys;
- `term_build.c`: ordinary and type-level constructors;
- `term_cbpv.c`: effect rows and CBPV constructors;
- `term_substitution.c`: substitution and reindexing;
- `term_normalization.c`: evaluator, profiles, conversion, cache;
- `term_host.c`: host declarations and implementations;
- `term_print.c`: readback.

This is a physical refactor only if all tags, keys, interning order, reduction profiles, and mutation notifications remain unchanged. The split must not create “value APP” and “computation APP” tags.

### 5.3 Frontend

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `frontend/ast.c` | 1,125 | AST storage and constructors; type expressions; definitions/imports; definition index; computation blocks and folds | Cohesive mutable surface representation. Top-level binding and computation-block binding are intentionally different AST uses and should gain clearer node/API names before any semantic unification. |
| `frontend/ast_inspect.c` | 195 | Debug-only AST names and printer | Cohesive. Keep out of parser and kernel dependencies. |
| `frontend/reader.c` | 2,947 | Handwritten lexer/parser, surface syntax, system names, computation block/fold parsing; also compiler graph wrapper, external linking setup, and system Nat installation | Contains a real boundary problem. Parsing should produce AST and syntax diagnostics. Session/bootstrap and link preparation should move to a driver/compiler-session module. |
| `frontend/lowering.c` | 16,883 | Name/import resolution, graph construction, type declaration lowering, CBPV elaboration, computation-block lowering, classifier/effect fixed points, Match motives, guarded recursion/IH, judgement materialization, expectations, publication, and orchestration | Largest ownership concentration. It mixes constraint generation, solving, graph mutation, proof-candidate materialization, and phase control. It also contains duplicate polarity/kind enums and a syntactic one-shot resumption use counter. Split only after explicit phase records and authority contracts are introduced. |

`lowering.c` already distinguishes syntax-directed information from solved classifiers in several work cells. The next conceptual step is not a wholesale rewrite but a stable interface:

```text
constraint_generator(OperationGraph, TermDB)
    -> ConstraintSet

constraint_solver(ConstraintSet, budgets)
    -> Solution + Residuals

proof_materializer(Solution, OperationGraph)
    -> JudgementDelta candidates

accepted_replay(candidates)
    -> Claims + Derivations
```

Today these stages are callable only indirectly through private functions and shared mutable `compile_context` fields.

### 5.4 Graph and runtime

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `graph/compile_metadata.c` | 78 | Initializes aggregate compiler work storage | Cohesive but its header exposes too many concrete stores. It should remain allocation/wiring, not gain semantics. |
| `graph/operation_graph.c` | 2,324 | OperationGraph CRUD and validation; verification obligations and backend capabilities; runtime evaluator, environments, resumptions, and fold handling | Three distinct concerns share one file. Split storage/validation, residual verification, and runtime execution while preserving operation IDs and behavior. Runtime one-shot enforcement remains a useful guard but is not static resource evidence. |

`OperationGraph` is not a redundant copy of `TermDB`. Two source occurrences can project to the same core term while carrying distinct contexts, classifiers, spans, or selected nominal views. This occurrence identity is required for diagnostics, type evidence, and residual verification.

`VerificationDB` is also not a second proof database. It explicitly stores conditional runtime obligations, especially dependent computation-fold result checks. A pending obligation must never be accepted as a closed `Claim` merely because it was produced by the compiler.

### 5.5 Kernel context and declarations

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `kernel/context.c` | 1,629 | Context and substitution interning/validation; identity, empty, projection, extension, composition; term reindexing; comprehension actions and telescope reindexing | This is the coherent CwF substrate. Preserve binding IDs rather than introducing de Bruijn-level remapping as a second identity system. Future consumed usage belongs to per-judgement vectors indexed by these bindings, not one global multiplicity on a Context entry. |
| `kernel/cwf_certificate.c` | 264 | Context/substitution certificate storage and validation | Cohesive. Certificates validate persistent CwF structure; they do not replace the structure itself. |
| `kernel/kernel_view.c` | 58 | Aggregate kernel-store validation and builder/view conversion | Cohesive boundary object. It should become the narrow input to replay/validation APIs. |
| `kernel/type_declaration.c` | 3,449 | Type expressions/declarations, constructor telescopes, derived curried classifiers, structural shape keys, representations, generated Identity declaration construction and validation | Constructor telescope is the semantic schema; curried Pi classifier is a derived cache. HOTT-specific schema validation currently leaks into this generic module and should eventually move behind a generated-declaration verifier interface. |
| `kernel/universe.c` | 1,062 | Universe nodes, constraints, provenance collection, and solver | The solver is cohesive, but collection depends on `frontend/lowering.h` and `OperationGraph`. Move compiler traversal into an adapter above the kernel; keep constraint representation and solver here. |

The CwF reading is standard: contexts are objects, substitutions are morphisms, substitution acts on types and terms, and context extension provides comprehension. This matches the categories-with-families formulation described by [Castellan, Clairambault, and Dybjer](https://arxiv.org/abs/1904.00827).

`CONTEXT_WEAKEN` and a `PROJECTION` substitution are therefore related but not duplicates. Projection is the semantic morphism. A weakening derivation is evidence that a particular proposition is stable under that morphism. The correct consolidation is for the derivation premise edge to cite the exact substitution semantic action, not to delete either concept.

### 5.6 Judgement assembly and database

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `kernel/judgement.c` | 26 | Includes judgement/rule partitions into one translation unit | Intentional assembly file. Do not add semantics here. |
| `kernel/typing/judgement_db.inc` | 500 | Proposition interning, Claim and Derivation indexing, delta initialization, context/operation store installation | Coherent logical storage core. |
| `kernel/typing/candidate_publication.inc` | 1,324 | Candidate equality/interning, complete relation publication, premise edges, Match motive result publication | Compiler-to-kernel certificate boundary. Naming still uses “relation” for generic judgements and can be confused with HOTT relation terms. |
| `kernel/typing/candidate_replay.inc` | 940 | Candidate grounding, publishability, premise reconstruction, closure ranks, publication | Coherent transient replay stage, but must remain deterministic and independent of work-queue order. |
| `kernel/typing/accepted_replay.inc` | 4,959 | Rule-specific proof validators, accepted graph replay, operation typing validation, convenience Claim constructors, printing | Large but semantically appropriate to keep rule validators explicit. Extract common plumbing only; do not collapse distinct inference rules into a generic tag interpreter. |

The current logical storage split is correct:

- a `Proposition` identifies what is asserted;
- a `Claim` records acceptance of that proposition under authority/context;
- one or more `Derivation` records can conclude the same Claim;
- ordered premise edges carry Claim references or scoped propositions plus substitution actions.

Replacing this with “one relation record has one proof” would lose legitimate proof multiplicity and would make replay/history order semantic. The DAG is the correct shape.

#### 5.6.1 Can Claims and Derivations be reconstructed from OperationGraph?

The answer is different for four separate questions:

1. Can the principal typing proposition of an operation be reconstructed?
2. Can acceptance of that proposition be reconstructed?
3. Can one valid derivation be searched for again?
4. Can the exact accepted derivation DAG be reconstructed?

##### Audit trail for the external feedback

The feedback under review is:

> Claims and Derivations may, at least for OperationGraph-backed typing, be automatically recoverable from OperationGraph.

This review does not accept or reject that sentence as one indivisible claim. It decomposes it as follows:

| Interpretation | Decision | Reason |
| --- | --- | --- |
| The principal `HAS_TYPE` tuple of a solved operation is derivable from the operation node | Accepted | `context_id`, `core_term`, selected `classifier`, and operation authority are present. |
| The structural skeleton of APP/Lambda/Match/fold premises can often be found from operation child edges | Accepted with limits | Child occurrence edges are present, but their authoritative evidence owner and local scoped premises are not always operation-owned. |
| A compiler with the full TermDB, declaration environment, contexts, substitutions, normalization rules, and solver can search again for a proof | Accepted in principle | This is proof reconstruction by rerunning a substantial compiler phase, not extraction from OperationGraph alone. |
| Acceptedness of every proposition is determined by OperationGraph | Rejected | Grounding depends on a Derivation DAG and includes non-operation authorities. A solved classifier is not a proof that its premises close. |
| Every exact Derivation is a deterministic function of OperationGraph | Rejected | Alternate conversion/weakening proofs, scoped premises, rule payload, and multiple derivations are absent from the graph. |
| Claim records may be physically compressed | Open and plausible | The wrapper is small and closure rank is derived, but the accepted subset and stable references remain necessary. This is not evidence that Claims are semantically unnecessary. |
| Artifact proofs may be omitted and reconstructed on import | Possible alternative architecture, not adopted | It enlarges the trusted importer, makes reconstruction policy/version relevant, and can erase proof-relevant HOTT structure. |

Two concrete counterexamples reject the broad interpretation:

1. A `CONTEXT_WEAKEN` Derivation carries a particular `SubstitutionDB` projection path as `semantic_action_id`. The target operation node does not identify that morphism or prove that the source Claim weakens along it.
2. An `INDUCTION_HYPOTHESIS_ELIM` Derivation carries the exact Match, motive, case index, and field index. The source occurrence graph does not record that complete proof payload, and the same resulting classifier does not identify which IH evidence was used.

A third counterexample is proof multiplicity itself. Artifact closure marks every accepted Derivation concluding a reachable Claim. Reversing derivation storage order is an explicit HOTT regression test. OperationGraph has no field from which that accepted set or its distinct proof identities can be recovered.

This audit trail is retained even if a later implementation chooses Claim compaction. The reason for a future change must be “we introduced an equivalent accepted-subset representation under stated proof relevance,” not “OperationGraph already contained all proof information.”

For an ordinary solved operation, its principal proposition is nearly a projection:

```text
operation o = {
    context_id = Gamma,
    core_term = t,
    classifier = A
}

principal(o) =
    HAS_TYPE,
    authority = OPERATION(o),
    context = Gamma,
    operation = o,
    subject = t,
    classifier = A
```

This tuple should not be duplicated inconsistently. A helper that derives and checks this principal proposition from `OperationGraph` would be useful. It explains why operation-owned Claim headers often appear mechanically recoverable.

That observation does not extend to the whole proof database:

| Required proof information | In OperationGraph? | Consequence |
| --- | --- | --- |
| Principal context, core term, and selected classifier | Yes | Principal operation proposition can be projected. |
| Whether a proposition was accepted by the grounded closure | No | Presence of a solved node is not an acceptance certificate. |
| Alternate classifier exposed by conversion, expected-type exposure, literal admissibility, or effect weakening | No; only the selected/known operation classifiers are stored | One operation can support additional Claims not equal to its principal projection. |
| Claims owned by Context bindings, type declarations, intrinsics, type formation, Universes, core helpers, or exports | No | A large proof subgraph has no operation owner. |
| Exact proof rule | Sometimes suggested by the operation tag, but not uniquely | APP may be function elimination or constructor formation; wrappers and derived boundaries add other rules. |
| Ordered premise Claims | Structural child operations only partially determine them | The evidence owner may be a Context binding or declaration rather than the child operation. |
| Scoped local premises | No | Lambda, Match, fold, and effect-row assumptions need propositions that are deliberately not global Claims. |
| CwF substitution used by weakening/reindexing | Not in the operation node | It is an explicit `SubstitutionDB` semantic action and must be retained or reconstructed from that database. |
| Match-pattern and IH rule payload | Partial | Operation cases retain constructor selection, but exact owner view, motive, case index, field index, and proof use are Derivation data. |
| Normalization/conversion justification | No | It depends on reduction profile, definitions, and an actual conversion decision. |
| Multiple accepted derivations of the same Claim | No | OperationGraph has no proof multiplicity or proof identity. |
| Grounded closure rank | No | It is recomputable from the Derivation DAG, not from operation edges. |
| Exact derivation selected by object Identity action | No | HOTT action traverses accepted proof kinds and premise Claims. |

The distinction is visible in the current validators. `prototype_judgement_validate_operation_typing` checks that an APP occurrence projects to an APP core whose function and argument are the expected child occurrences. It does not prove that the function classifier is Pi, that the argument inhabits its domain, or that codomain substitution yields the result classifier. `validate_app_elim_proof` checks those facts from the explicit ordered premises. OperationGraph validation is therefore a structural cross-check, not a replacement typing derivation.

Match and dependent computation make the gap larger. A Match operation stores its scrutinee and branch occurrence graph, but the proof records the motive formation, branch substitutions, scoped pattern assumptions, Universe premises, and possible IH payload. A computation fold stores its structural clauses, but the accepted proof or residual verification record determines whether its dependent result was established statically.

##### Claim storage is partly physically compressible

A current `Claim` contains only a `proposition_id`, a recomputable closure rank, and hash-index fields. Conceptually it denotes membership in the accepted subset of propositions. It may eventually be represented more compactly as:

- an accepted bit/subset entry on an interned Proposition;
- a stable accepted-Proposition handle;
- or a dense artifact partition separating accepted conclusions from scoped-only propositions.

That is a physical representation choice, not reconstruction from OperationGraph. The accepted subset must still exist because the proposition arena also contains scoped premises that are not global Claims. Claim handles are also referenced by CwF certificates, Universe provenance, exports, Identity actions, and artifact roots. Any compaction must migrate those references directly; it must not add a compatibility/remap layer as permanent architecture.

##### Derivations are currently irreducible certificates

A Derivation stores the proof rule, conclusion Claim, ordered premise edges, scoped propositions, CwF semantic actions, and rule-specific payload. Artifact closure currently preserves every accepted Derivation concluding a reachable Claim and recursively includes all premise Claims. Accepted replay validates these records without rerunning solver search.

It is possible in principle to define a different artifact architecture that stores only a typed operation graph and reruns proof reconstruction at import. That would be a valid compiler design, but it would have different semantics:

- the importer would trust a larger elaboration/reconstruction engine rather than a small rule replay checker;
- acceptance could depend on compiler version, available definitions, reduction profile, and reconstruction budget;
- proof multiplicity would be discarded or replaced by a newly chosen canonical proof;
- HOTT object action could change because it currently consumes exact proof kinds and premises;
- artifacts would no longer be proof-carrying certificates in the present sense.

The current design is closer to proof-carrying code: the producer performs search, while the consumer validates explicit evidence. Necula's [Proof-Carrying Code](https://doi.org/10.1145/263699.263712) makes precisely this producer/checker trust distinction. The judgement-as-types account in [Harper, Honsell, and Plotkin's LF](https://publish.lfcs.inf.ed.ac.uk/reports/91/ECS-LFCS-91-162/) likewise treats derivations as proof objects checked by typing, rather than as dispensable execution traces.

For Higher Observational Type Theory, deleting proof multiplicity is especially unjustified. A Program has explicitly avoided proof irrelevance, and object Identity witnesses can themselves participate in higher Identity. Reconstructing one arbitrary derivation from an OperationGraph would silently impose a proof-canonicalization principle that has not been stated or justified.

##### Decision

The recommended design is:

1. retain Proposition, accepted-Claim status, and Derivation DAG as semantic certificate data;
2. derive the principal operation proposition through one checked projection API to remove header duplication;
3. allow Claim record compaction only as a separate physical change that preserves accepted-subset identity and all references;
4. retain all accepted derivations produced within the configured resource budget;
5. add a non-authoritative reconstruction audit that attempts to regenerate principal operation derivations and compares them with stored evidence;
6. never use successful reconstruction to delete non-operation, conversion, weakening, scoped, HOTT, or alternative derivations;
7. keep artifact import based on accepted replay unless a separately versioned “proof-reconstructing artifact” format is deliberately designed.

Thus the new observation identifies a useful normalization opportunity, but not a reason to remove the proof DAG.

### 5.7 Judgement conversion and classifier solving

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `kernel/typing/conversion.inc` | 2,912 | Kernel conversion and classifier views; Pi/family instantiation; effect constraints; expected-row solving; evidence selection | It combines three layers: kernel conversion, elaboration constraint solving, and proof-evidence selection. These need explicit internal interfaces. |
| `kernel/typing/classifier_solver.inc` | 2,029 | Type-expression and constructor classifier resolution; Match pattern/IH classifiers; type formation recognition | Primarily elaboration logic despite its kernel path. Kernel replay should validate its result, not rerun heuristic search. |

Definitional conversion and object Identity must remain separate. `prototype_judgement_classifier_conversion` is a kernel decision under a reduction profile; an object term witnessing Identity must not mutate that decision relation globally.

Effect-row constraints currently have two representations:

1. `prototype_judgement_effect_row_constraint` records rule-derived JOIN, RESIDUAL, or INCLUSION equations while computation rules are inferred;
2. `prototype_operation_effect_constraint` records occurrence-owned EXACT, COPY, UNION, or RESIDUAL constraints and their solved/residual state.

`lowering.c` explicitly imports relevant judgement constraints into occurrence constraints, then solves and substitutes operation-owned row variables. This can be a valid producer/consumer split, but it is not yet enforced as one. The required authority rule is:

```text
judgement rule constraint
    = declarative equation emitted by a typing rule

operation effect constraint
    = occurrence-indexed solver instance derived from those equations

solved OperationGraph classifier
    = authoritative compiler result

accepted Derivation
    = replayable evidence that validates the result
```

No second solver should independently decide a different row. Introduce a conversion function and invariants rather than sharing both mutable arrays through `compile_context`.

### 5.8 Rule implementations

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `kernel/rules/formation_early.inc` | 672 | Early commit; type/constructor formation; constructor spine; indexed branch refinement; binding and Match pattern assumptions | Coherent early-rule group. |
| `kernel/rules/formation_host.inc` | 128 | Text/int literal and host type formation expansion | Cohesive host-specific rules. |
| `kernel/rules/formation_recording.inc` | 475 | Records effect weakening, type/host formation, literal admissibility, declarations, and expected type exposure | Coherent recording API; audit naming because “expected exposure” is not object Identity. |
| `kernel/rules/introduction_lambda.inc` | 203 | Lambda expansion and delta inference | Cohesive. |
| `kernel/rules/elimination_app.inc` | 634 | APP expansion, binder assumptions, Lambda classifier inference, Pi candidate selection | Cohesive APP elimination and local bidirectional recovery. |
| `kernel/rules/match.inc` | 2,894 | Match motive construction, branch classifier selection, conversion/Universe helper generation, Match/IH expansion | Too broad. Motive synthesis is elaboration; rule recording and accepted validation are kernel concerns. Preserve explicit Match semantics while splitting search from rule construction. |
| `kernel/rules/cbpv.inc` | 2,475 | Host signatures, CBPV boundary inference, computation constraints, operation request and fold solving, effect-row rules | Correct semantic grouping, but solver and declarative rule emission are mixed. Dependent-output residual behavior requires an explicit contract. |
| `kernel/rules/introduction_identity.inc` | 1,769 | Lambda/APP recording, context weakening and reindex helpers, plus compiler relation type/witness rules | Misnamed and overloaded. Ordinary structural rules and compiler parametric-relation witness rules should be separate partitions. |

APP elimination, Match elimination, IH elimination, and computation-fold elimination contain similar storage plumbing but validate different theorems. They must remain distinct proof rules. A shorter generic validator that obscures each rule's premises would reduce auditability without reducing semantic complexity.

### 5.9 Identity and HOTT implementation

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `identity/hott.c` | 24 | Includes identity partitions into one translation unit | Intentional assembly file. |
| `parametricity/relation_action.inc` | 2,247 | Compiler-local logical-relation/parametricity action planning, work items, candidates, and residuals | Useful substrate, but it is not by itself object Identity or observational equivalence. T7 moved this authority out of the `identity` namespace. |
| `identity/action_certificate_init.inc` | 60 | Initializes action certificate storage | Cohesive. |
| `identity/action_certificate_validation.inc` | 1,734 | Validates action certificates and references | Large validator; keep separate from action search. |
| `identity/action_execution.inc` | 1,438 | Schedules object-Identity and parametricity actions under budgets through the public `parametricity/execution.h` contract | Compiler execution layer, not kernel equality reflection. |
| `identity/artifact_root_extraction.inc` | 92 | Converts eligible object Identity action results into artifact roots | Correct producer adapter; artifact policy must still validate roots independently. |
| `identity/telescope_action.inc` | 855 | Type-former descriptors and telescope capability analysis | Correct place for type-former-directed action capabilities. |
| `identity/identity_computation.inc` | 3,596 | Generates object Identity declarations/families for ADTs, Pi, Thunk/Return, and Universe scaffolding | Central object-Identity computation. Its capability limits must be normative and versioned. |
| `identity/context_bridge.inc` | 2,560 | Builds relation/Identity contexts, endpoint spans, projections, degeneracy, and Universe correspondence | Semantically central bridge from CwF substitutions to higher actions. |
| `identity/object_term_action.inc` | 5,585 | Constructs object Identity witnesses from accepted derivations for variables, constructors, APP, Lambda, Match, Thunk, and Return | Implements the object-level fundamental action for the supported fragment. It must consume accepted derivations, not solver guesses. |

The current identity subsystem has made a necessary distinction:

- `PARAMETRIC_ACTION` is a compiler logical relation over two endpoints;
- `OBJECT_IDENTITY` is an object-language type family and its inhabitants;
- bridge/certificate objects record how contexts and substitutions support those actions.

That distinction must become clearer in directory and identifier names. The present `relation_type`/`relation_witness` core tags are compiler relation forms and must not become the surface `Eq` merely by renaming them.

The several HOTT arenas are not currently proven redundant. A relation goal states a requested action, candidates retain alternatives and child/conversion premises, work items carry scheduling outcome, residuals explain non-closure, actions are stable requests, and certificates are checked results. They should be governed by one explicit state-transition contract, but collapsing them before that contract exists would confuse search state with object evidence. A future quantitative Context action must additionally specify the usage assigned to left endpoints, right endpoints, and relation/Identity witnesses; ordinary endpoint projection alone does not determine those grades.

The implementation status is:

| Capability | Current state |
| --- | --- |
| Compiler logical-relation action | Implemented |
| Closed homogeneous one-dimensional object Identity for supported formers | Implemented |
| Explicit witnesses for selected non-DefEq function identities | Implemented |
| Some identity-of-identity/two-dimensional cases | Implemented |
| General dependent Identity action | Partial |
| General transport/lifting | Not implemented |
| Recursive coherent Universe Identity | Not implemented |
| Arbitrary higher-dimensional coherence | Not implemented |
| Surface `Eq`, `refl`, `transport`, rewrite | Not implemented |
| Full Higher Observational Type Theory | Not implemented |

This is not a failure of the current implementation; it is the actual boundary that artifact and user-facing claims must state.

### 5.10 Artifact subsystem

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `artifact/interface.c` | 1,299 | Exports/dependencies/namespace; Identity roots; interface construction; dependency collection; Universe renumbering and export lookup | Semantic Identity root validation overlaps generated-Identity validation in the type-declaration module. Establish one shared object-Identity verifier. |
| `artifact/link.c` | 2,299 | Relocation, canonical reference repair, accepted judgement append, binding relocation, graph append, export operation alignment | Cohesive linker, though operation and proof relocation should have documented transaction invariants. |
| `artifact/publication.c` | 6,155 | Reachability marking, canonical publication order, graph slicing, relocation sections, serializer, republishing | Physically overloaded. Split closure discovery, canonical ordering, slicing/relocation, and text emission without changing wire order. |
| `artifact/relocation.c` | 630 | Applies term/type relocations, recomputes keys, attaches declaration support | Cohesive relocation pass. |
| `artifact/wire_v71.c` | 3,330 | Reads interface and graph sections; validates sparse present slots and all references; reads operation, Universe, debug, and relocation sections | Correct wire boundary. Semantic proof replay is delegated to kernel validators after structural readback. |

Artifact v71 stores object evidence and accepted derivations, not HOTT work queues or consumed fuel. That is the right boundary. Compiler search history is neither module meaning nor a stable certificate.

At the audit baseline, artifact v70 conflicted with executable policy for Universe correspondence roots. T0 resolved this by introducing `spec/artifact_v71.schema`, keeping finite Universe correspondence compiler-local, and aligning extraction, interface validation, tests, and fingerprints. Universe computation remains scaffolding rather than persistable recursive coherent Universe Identity.

The artifact layer currently performs structural recognition of Pi Identity families while `type_declaration.c` also validates generated Identity declarations. This is not acceptable as two independent semantic definitions. The target should be:

```text
wire reader
    validates presence, ranges, tags, and relocation closure

shared object Identity schema verifier
    validates generated declaration and root semantic shape

accepted replay
    validates Claims and Derivations

artifact interface
    applies publication/import policy only
```

### 5.11 Drivers

| File | Lines | Current responsibility | Review |
| --- | ---: | --- | --- |
| `driver/read_file.c` | 4,862 | CLI, module path resolution, provider dependency ordering, artifact read/link/republish, diagnostics, normalization/shape checks | Too broad for a driver. Extract module resolver, shared diagnostics, and artifact test/check helpers. |
| `driver/repl.c` | 1,208 | REPL input, named queries, WHNF/NF/value evaluation, state and diagnostics printing | UI-focused except for duplicated diagnostic/type/Universe printers. |

The two drivers contain exact or near-exact copies of resolution and Universe diagnostic helpers. This is genuine physical duplication with no semantic reason. Move it to a shared driver diagnostics module.

## 6. Public Header Review

### 6.1 Core, frontend, graph, and artifact headers

| Header | Public contract | Review |
| --- | --- | --- |
| `core/term.h` | Term tags, all term payloads, TermDB, builders, reduction, conversion, host operations | Too broad and conceptually depends on `kernel/type_declaration.h`. Move shared type/qualified-name IDs to a neutral schema header, then split API views without duplicating the term representation. |
| `frontend/ast.h` | Surface AST and type-expression structures/builders | Appropriate public frontend contract. |
| `frontend/ast_inspect.h` | AST debug printer | Cohesive. |
| `frontend/lowering.h` | compile entry point and broad compiler workspace dependencies | The 31-line header is small but overincludes artifact and judgement internals. Replace concrete dependencies with a workspace/builder facade. |
| `frontend/reader.h` | reader/session workspace | Includes lowering, term, all judgement APIs, declarations, and Universe. It exposes implementation assembly rather than parser input/output. |
| `graph/operation_graph.h` | operation occurrences, Match/fold edges, effect constraints, verification obligations, runtime APIs | Three contracts in one header. Split declaration headers while preserving shared IDs. |
| `graph/compile_metadata.h` | aggregate fixed-capacity compiler storage | Appropriate internal assembly type, but should not be included by semantic kernel headers. |
| `artifact/interface.h` | artifact interface, graph, relocation, wire, and publication APIs | Overbroad; it includes frontend, CwF, Universe, and all judgement APIs. Split interface model, publication, wire reader, and linker APIs. |

### 6.2 Kernel headers

| Header | Public contract | Review |
| --- | --- | --- |
| `kernel/context.h` | ContextDB, SubstitutionDB, comprehension, reindexing | Sound CwF contract. |
| `kernel/cwf_certificate.h` | persistent CwF certificates | Cohesive. |
| `kernel/kernel_view.h` | aggregate read-only/builder views | Good dependency-narrowing mechanism; use it more broadly. |
| `kernel/type_declaration.h` | type declarations, constructor telescopes, representations, generated origins | Constructor telescope authority is correctly stated. Generated-HOTT details should be abstracted behind verifier hooks. |
| `kernel/universe.h` | Universe graph, constraints, solver, collection | Split pure solver API from compiler collection adapter. |
| `kernel/judgement/types.h` | proposition, claim, derivation, candidates, constraints | Correct foundational declarations. Clarify which fields are persistent and which are solver-only in type names, not comments alone. |
| `kernel/judgement/db.h` | JudgementDB and delta storage/API | Cohesive storage interface. |
| `kernel/judgement/conversion.h` | conversion, classifier views, Pi specialization, evidence selection | Contains both kernel decision and elaborator helper APIs. Split public contracts. |
| `kernel/judgement/classifier_solver.h` | elaboration classifier resolver | Its path should reflect elaboration rather than trusted accepted replay. |
| `kernel/judgement/rules.h` | all rule emitters, builders, and helper APIs | 816-line umbrella header. Split by rule family and retain a compatibility-free internal aggregate include only inside `judgement.c`. |

### 6.3 Identity headers

| Header | Public contract | Review |
| --- | --- | --- |
| `identity/types.h` | action kinds, relation/bridge semantics, outcomes, residuals, certificates | Correctly records distinctions, but overincludes CwF, all judgement APIs, and kernel view. Split POD schemas from execution dependencies. |
| `parametricity/relation_action.h` | compiler parametricity action | T7 moved the compiler relation planner and its public types into the `parametricity` namespace. |
| `identity/identity_computation.h` | object Identity family computation | Correct object-level boundary. |
| `identity/context_bridge.h` | endpoint and projection bridge | Correct CwF/HOTT bridge boundary. |
| `identity/object_term_action.h` | witness action over accepted terms | Correct object-level action boundary. |
| `identity/telescope_action.h` | type-former capability/telescope action | Cohesive. |
| `identity/action_certificate.h` | certificate initialization/validation | Cohesive. |
| `parametricity/execution.h` | action scheduling/execution | Cohesive compiler service shared with the object-Identity owner. |

### 6.4 Support header

| Header | Public contract | Review |
| --- | --- | --- |
| `support/symbol.h` | symbol-table storage and API | Cohesive. |

### 6.5 Root and internal compiler headers

| Header | Internal contract | Review |
| --- | --- | --- |
| `calculus.h` | Accepted-calculus and HOTT-calculus fingerprints | Correct place for build-visible semantic identities. T0 reconciled its publication-root description with artifact v71 object Identity roots. |
| `src/artifact/artifact_graph_internal.h` | Artifact append order, sparse presence predicates, canonical-key linkability, internal append/read helpers | Appropriate private header. The repeated presence predicates are a valuable single structural authority and should be reused rather than recreated in each wire pass. |
| `src/artifact/artifact_internal.h` | Canonical-reference repair and optional runtime-capability scanner | The macro-controlled static helper is fragile ownership. Move runtime-capability scanning to the graph/verification implementation and call it through one function. |
| `src/internal/ast_common.h` | Shared fixed-capacity `reserve_slot` helper for AST-related translation units | Harmless but too generic a name for one static helper. Keep private or replace with a support capacity predicate during physical cleanup. |

### 6.6 Build, specifications, and test layout

| File/group | Role | Review |
| --- | --- | --- |
| `Makefile` | Builds REPL and artifact reader from manifest groups | Minimal and appropriate for the prototype boundary. |
| `build/sources.mk` | Defines overlapping AST, kernel, graph, compiler, HOTT, REPL, and reader source groups | It is the only source manifest, but the groups repeat direct members. Factor base groups so a new kernel component cannot be added to one aggregate and omitted from another. |
| `build/test_support.sh` | Shared shell-test setup | Correct common test infrastructure; keep paths derived from the source manifest where possible. |
| `spec/artifact_v71.schema` | Canonical wire semantic manifest and fingerprint input | Current persistent-boundary specification. It excludes compiler relation forms, work state, and unsupported Universe correspondence roots from object Identity roots. |
| `spec/hott_fragment_v5.schema` | Compiler relation and object Identity fragment contract | Current fragment-5 contract after T0 normalization. |
| `spec/archive/` | Historical artifact manifests | Correctly separated; never use these as compatibility parsers unless explicitly approved. |
| `tests/checks/*.c` | Focused structural/kernel executable checks | Covers CBPV boundary, CwF context category, conversion results/scopes, shared core representation/reindexing, frame identity, Universe DefEq, and profile cache. |
| `tests/integration/*.sh` | End-to-end surface, artifact, CBPV, HOTT, and certificate tests | Good breadth, but the regression matrix in Section 12 identifies missing authority and residual-lifecycle assertions. |

## 7. Theory Review

### 7.1 Higher Observational Type Theory

The current implementation should be described as an object-Identity fragment built on a compiler parametricity substrate, not as completed HOTT.

The [TYPES 2022 Higher Observational Type Theory abstract](https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf) motivates computing equality according to each type former and develops the required logical-relation/telescopic machinery over contexts and substitutions. This supports the current type-former-directed architecture, but it does not justify identifying every logical relation with equality.

[Internal parametricity, without an interval](https://arxiv.org/abs/2307.06448) demonstrates higher cubes and computational behavior from internal parametricity with operations and equations through finite dimensions. It supports relation action as a useful substrate, but does not make that substrate automatically a complete observational Identity type.

Narya's documentation explicitly distinguishes [parametricity mode](https://narya.readthedocs.io/en/latest/parametricity.html) from [HOTT mode](https://narya.readthedocs.io/en/latest/hott.html). Its [observational type theory discussion](https://narya.readthedocs.io/en/latest/observational.html) describes a type-indexed Identity family and the need for transport/lifting structure. This is the relevant distinction for A Program.

Required design conclusions:

1. `relation_action` must remain compiler parametricity infrastructure.
2. Surface equality must elaborate to the object Identity family selected by the exact type Claim and context bridge, not to `RELATION_TYPE_FORMER`.
3. Each supported type former needs a normative matrix covering Identity formation, degeneracy/reflexivity, object action, transport, lifting, higher closure, computation, replay, and artifact persistence.
4. Symmetry and composition should not be added as unrelated proof tags before transport/lifting and their coherence are specified. They may be derivable in the completed structure.
5. The finite Universe correspondence currently implemented is scaffolding, not recursive coherent Universe Identity and not univalence.
6. “No witness produced” must distinguish logical emptiness from unsupported/residual action.

### 7.2 Normative HOTT schema defect

The pre-T0 `src/prototype/spec/hott_fragment_v2.schema` was internally stale and
is normalized as `src/prototype/spec/hott_fragment_v5.schema` by T0:

- its header declares fragment version 5;
- later prose still calls it fragment version 4 and fragment version 2;
- sections said relation persistence/reflexivity was deferred until A1.T0 even though object Identity roots and artifact v70 support already existed;
- `object_action_proof` assigned `MATCH_ELIM=13`, while the C enum and artifact v70 assigned `MATCH_TYPE_FORMATION_INTRO=13` and `MATCH_ELIM=14`.

T0 resolved the persistent-manifest conflict by introducing artifact v71, excluding finite Universe correspondence from admitted roots, and updating `calculus.h`, interface policy, and tests together.

This is the highest-priority documentation/specification defect because implementation and artifact policy are being reviewed against a normative file that describes multiple historical states. Fixing it is not a code refactor, but it must precede the next wire-format or equality extension.

### 7.3 CBPV and dependent effects

The shared term graph with classifier polarity is compatible with CBPV. CBPV requires a semantic distinction between values and computations; it does not require two physically duplicated graph stores. Current Pi classification as a computation function and value-level function passing through Thunk is a coherent representation choice.

`OperationRequest` must remain a distinct core node. Encoding an operation name as ordinary APP would lose the request identity and continuation structure that a handler/fold must inspect. `ComputationFold` correctly unifies zero-clause sequencing and clause-bearing handling at the core algebra level.

Algebraic handlers are interpreted as homomorphisms from a free model of operations, as developed by [Plotkin and Pretnar](https://arxiv.org/abs/1312.1399). This supports keeping operation requests and folds explicit even when surface `perform` syntax is removed.

Dependent sequencing is the difficult boundary. Vákár's [Framework for Dependent Types and Effects](https://arxiv.org/abs/1512.08009) distinguishes dCBPV- from dCBPV+ with dependent Kleisli extension. In effectful dependent sequencing, running a computation can make the result type more specific, which complicates substitution and subject reduction.

A Program currently uses a hybrid:

- pure type normalization is attempted under a fixed profile and budget;
- a dependent fold closed by that process can receive static evidence;
- blocked-effect or exhausted cases become `COMPUTATION_FOLD_RESULT` verification obligations;
- those obligations remain outside accepted `JudgementDB` proofs until runtime discharge.

This is a viable research direction, but its soundness contract is not yet normative. The contract must define:

- the exact classifier family before runtime result substitution;
- which runtime value is substituted and under which binding identity;
- how runtime discharge produces or references evidence;
- how effect rows and handler residuals are related to the obligation;
- whether artifacts may export pending obligations and what backend capability is required;
- how a failed obligation affects execution and module validity.

### 7.4 Proof theory and CwF

The CwF substrate and the Proposition/Claim/Derivation split are among the strongest parts of the design.

The following distinctions must be preserved:

- context projection versus a proof that a proposition weakens along that projection;
- an immutable proposition versus acceptance of it;
- a Claim versus potentially many derivations of that Claim;
- candidate inference state versus accepted replayable evidence;
- definitional conversion evidence versus object Identity inhabitants;
- residual runtime verification versus a closed theorem.

The main debt is not the number of these structures. It is that some transitions between them are implicit shared-memory conventions. Every transition should be represented by one API that checks source authority, target authority, context/substitution action, and replayability.

### 7.5 Proof reconstruction is not proof identity

In proof-theoretic terms, OperationGraph can be viewed as annotated syntax from which some judgement derivability may be decidable. It is not itself the derivation tree. A syntax-directed rule can make proof reconstruction straightforward, but conversion, cumulativity, weakening, overloading, effect-row constraints, and local eliminator assumptions introduce choices and extra premises.

Three levels must be distinguished:

```text
derivable(Gamma |- t : A)
    an existence/property claim

reconstruct(Gamma, t, A)
    an algorithm that may search for one derivation

D : Gamma |- t : A
    a particular proof object with ordered premises
```

If proof irrelevance were an admitted metatheorem for typing certificates, different `D` values could be erased after checking. The intended HOTT direction does not permit assuming this globally: Identity inhabitants and higher identities are deliberately proof-relevant. Even if ordinary typing derivations were later placed in a proof-irrelevant stratum, that stratification would need to be explicit and must not include object Identity witnesses or their action certificates.

CwF does not collapse this distinction. A context/substitution category describes reindexing of types and terms. A derivation that a proposition is preserved along one substitution is evidence over that categorical structure, not the substitution morphism itself. Similarly, OperationGraph describes typed occurrences and structural dependencies, not acceptance closure in the proof category.

The safe optimization target is therefore proof reconstruction as an audit/cache mechanism. It can demonstrate that stored certificates are complete with respect to the graph, but it cannot define certificate identity unless the theory first chooses proof irrelevance or a canonical focused proof system for that fragment.

### 7.6 Resource-sensitive proof structure

The explicit Conversion, weakening, and substitution records are not accidental verbosity if A Program is to acquire a resource-sensitive theory. In ordinary intuitionistic dependent type theory, weakening and contraction are normally admissible without recording a resource cost. In a quantitative or linear dependent theory, the judgement must also explain how each Context binding is used, and a substitution must preserve that usage discipline. Atkey's [Quantitative Type Theory](https://doi.org/10.1145/3209108.3209189) records a usage annotation for each variable in a judgement and models the result with quantitative categories with families. Vákár's [Linear Dependent Type Theory](https://arxiv.org/abs/1405.0033) similarly separates the intuitionistic dependency base from a linear layer modelled by indexed symmetric monoidal categories with comprehension.

This yields an important correction to a tempting implementation plan: a single multiplicity field on a `ContextDB` entry is not enough. The Context says which binding and classifier are available. Different terms in the same Context can use that binding zero, one, or several times. The consumed usage therefore belongs to a judgement/operation derivation as a vector indexed by the Context; a declaration may additionally bound or qualify permitted usage.

A future resource judgement should have the conceptual form:

```text
Gamma ; rho |- t : A

Gamma    dependency Context/CwF object
rho      per-binding usage vector over a selected semiring or grade algebra
```

The corresponding substitution is not just `sigma : Delta -> Gamma`. It also carries or validates a usage transformation from resources in `Delta` to those required by `Gamma`. Composition combines those transformations. This is exactly why the existing `SubstitutionDB` path and the semantic action on `CONTEXT_WEAKEN`/`SUBSTITUTION_REINDEX` are useful seeds: they provide the morphism to which resource evidence can be attached.

The current one-shot resumption check is not such evidence. `frontend/lowering.c:11941` calls `operation_subtree_ast_binder_use_count`, whose implementation at `frontend/lowering.c:12889` counts syntactic `VAR` occurrences and stops after two. That is a conservative local check, but it is not stable under all semantically relevant transformations:

- shared operation nodes can represent one evaluated result used from several places;
- Match branches are alternatives rather than sequential consumption;
- a Lambda or Thunk can defer, duplicate, or prevent a use;
- a handler can resume zero, one, or several times;
- substitution can duplicate an argument even when the source variable appears once;
- future optimizer rewrites can change occurrence count while preserving program meaning.

The resource-sensitive replacement is rule evidence, not a more elaborate tree walk:

| Rule | Resource obligation |
| --- | --- |
| variable | consume the basis usage for its binding |
| Lambda | discharge the binder with the multiplicity declared/inferred for the function |
| APP | combine function and argument usages, scaling argument usage by the function multiplicity |
| Return/Thunk/Force | apply the selected CBPV modality's resource action; do not assume every Thunk is freely duplicable |
| Match | validate the theory's additive branch rule rather than summing mutually exclusive branches blindly |
| computation fold | combine input, return clause, operation clauses, and resumption multiplicity |
| substitution | transform usage by the substitution's quantitative action and compose those actions |
| weakening | require zero demand, an affine permission, or the grade-specific weakening law |
| contraction | require a duplicable grade/modality rather than inferring permission from graph sharing |
| conversion | preserve not only the classifier but also its resource/effect interpretation |

Effect rows and resource usage must remain separate. `Comp(E, A)` records which operations may occur; a usage vector records how values, thunks, or resumptions are consumed. The same operation can be permitted once or many times, and a pure computation can still consume a linear value. Conflating the two would reproduce the same mistake as treating an effect label as a continuation multiplicity.

CBPV also makes the required decision explicit. If `Thunk C` is an unrestricted value, duplicating it may run `C` more than once when forced. If `C` closes over a one-shot resumption or linear resource, unrestricted Thunk introduction is unsound. A Program must eventually choose a graded/linear modality or reject such captures with derivation evidence; the runtime `resumed` guard is a backstop, not a static proof.

HOTT action must be extended only after that choice. Relation contexts duplicate endpoints and add relation witnesses. In a quantitative theory, their endpoint and witness usages cannot be guessed from the ordinary Context bridge. The action must state how grades map to the left endpoint, right endpoint, and higher witness. This is another reason not to erase weakening, reindexing, or particular Derivations before the proof-relevance policy is settled.

### 7.7 Cross-layer unification audit

The codebase contains three materially different kinds of repetition:

1. **physical duplication**: two enums, fields, or validators encode the same fact and should have one authority;
2. **layered representation**: syntax, occurrence, semantic graph, proof, and wire forms intentionally differ and need a checked total map;
3. **proof-theoretic distinction**: two objects describe different propositions or evidence and must remain separate even when their C plumbing looks alike.

The correct response is therefore not a general reduction in tag count. The following table records the current decision for the main cross-cutting candidates.

| Concepts found in the code | Classification | Decision |
| --- | --- | --- |
| AST nodes, Operation nodes, and Term nodes | Layered representation | Keep all three. Define and test lowering/projection maps; do not require one-to-one tags because blocks, names, and ascriptions elaborate away while one core Term may have several typed occurrences. |
| lowering binder stack, `context_ids[n]`, and persistent `ContextDB` | Surface-to-semantic adapter plus authority | Keep the transient AST-binder lookup and persistent Context separately. Make ContextDB authoritative for scope/classifier semantics and check that each stack prefix maps to exactly its recorded Context; do not copy the Context list into another mutable list. |
| block `local_refs` and persistent Context bindings | Distinct lexical environments | Keep separate. A block local may alias a Value, Function computation, or Operation occurrence without introducing a dependent Context assumption; Lambda/Match binders do extend Context. Give both environments narrow lookup/push/pop APIs so their different scoping rules are explicit. |
| `compile_ref_polarity` and `prototype_operation_polarity` | Physical duplication | Remove the private alias enum and use one shared semantic-category vocabulary. |
| `compile_ref_computation_kind`, operation `computation_kind`, and `prototype_term_computation_kind` | Physical duplication plus phase state | Use the same kind enum for established values and a separate explicit `UNKNOWN`/unsolved state. Do not maintain numerically parallel enums. |
| `prototype_judgement_category`, `prototype_term_category`, operation polarity, and HOTT relation category | Shared semantic vocabulary over different subjects | Introduce one common Value/Computation/Type category enum, with APIs specifying whether it classifies a classifier, occurrence, or relation. This unifies scalars, not graphs or typing rules. |
| Term `application_role` and Operation `application_role` | Derived cache under current validator | The graph validator requires equality with `prototype_term_semantics`; therefore Term shape is currently authoritative. Either derive it on demand or label the Operation field as a checked cache. Do not claim occurrence-specific freedom while validation forbids divergence. |
| operation `known_classifier`, solver binding, and solved `classifier` | Distinct phase states | Keep the distinction, but move it into an explicit input/solution transition so no caller can publish `known_classifier` as a solved result. |
| Context classifier `TERM`, `VARIABLE`, and `PROVISIONAL` refs | Solver sum type | Keep. They distinguish a solved Term from a metavariable and a provisional pair. Prevent provisional refs from crossing accepted artifact/kernel boundaries. |
| projection substitution and weakening Derivation | Proof-theoretic distinction | Keep both and require the Derivation to cite the exact substitution semantic action. Extend that edge with usage evidence for resource sensitivity. |
| generic substitution reindex and context weakening | General rule versus restricted rule | Keep separate proof kinds: weakening requires a projection path, while reindex accepts any valid CwF substitution. Share the reindex implementation. |
| constructor telescope, result classifier, curried Pi cache, readback metadata | Authority plus derived views | Keep all data roles, but only the Context telescope/result classifier is semantic schema. Rebuild and validate caches/readback from it. |
| Type structural fingerprint, exact representation comparison, and nominal `TYPE_VIEW` | Filter, equivalence, and name/view | Keep distinct. A hash key narrows candidates, exact alpha-aware comparison establishes structural representation, and a TypeView preserves user-selected nominal meaning. |
| `TypeCodeShapeKey` name versus its implementation | Naming debt | Rename toward `type_representation_fingerprint`; it is neither a Tarski code nor typed equality evidence. |
| effect operation ID, operation-label bit, host-effect capability, and host oracle kind | One genuine duplication plus two necessary boundaries | Operation identity belongs to the language; host capability and backend oracle remain separate mappings. Remove the second operation identity encoded as `unsigned` label bits. |
| classifier-view `effect_row` and derived `unsigned effects` | Authority plus lossy legacy cache | Keep the effect-row Term as authority. Remove the fixed-width cache from general semantic APIs once all closed-row consumers use normalized operation atoms; a backend capability projection may still return a separate bitset. |
| intrinsic signature enum/schema and classifier Term | Derived builder versus semantic classifier | Make the full classifier Term (with a declaration/proof) authoritative. Keep compact host descriptors only as builders and backend ABI checks; do not let `TEXT_TO_TEXT`-style enums constrain future user/dependent operations. |
| effect-row Term algebra, judgement effect equations, and operation-owned solver constraints | Layered representation | Keep Term rows as language data and one solver constraint IR as compile state. Convert rule output through one checked adapter rather than maintaining two open-ended constraint languages. |
| pure primitive and effect operation | Proof-theoretic/operational distinction | Keep separate declarations and Terms. Machine dependence alone does not make a deterministic pure primitive an algebraic effect. Backend implementations remain separately selected. |
| APP and `OPERATION_REQUEST` | Computational distinction | Keep request as an inspectable free-algebra node. Surface syntax may elaborate ordinary operation application to it, but raw APP cannot carry handler-visible request/resumption structure. |
| zero-clause and clause-bearing `COMPUTATION_FOLD` | One generic eliminator with different rule cases | Keep one core constructor, with explicit sequencing and handler validation branches. Do not restore a parallel `BIND` node. |
| normalization, conversion, and runtime evaluation | Shared reduction engine with different contracts | Share reduction primitives and profile caches; keep conversion outcomes and host-effect execution separate. Runtime success is not definitional equality. |
| solver residual, HOTT residual, and runtime verification obligation | Different consumers and trust levels | Keep typed records. A common status vocabulary is harmless, but one generic residual record would erase who can discharge it and whether it is a theorem. |
| HOTT goal, candidate, work item, action, and action certificate | State-machine stages | Keep their semantic roles; replace loose cross-arena conventions with one documented transition API. Candidate/work storage may be physically co-located only after transition invariants are executable. |
| compiler relation action and object Identity | Proof-theoretic distinction | Keep names and modules separate. Parametricity can construct evidence used by Identity without itself being user equality. |
| generated Identity declaration and ordinary TypeDeclaration | Shared declaration representation with specialized validator | Keep one declaration DB; use one object-Identity schema verifier rather than a parallel Identity declaration database. |
| local semantic object and artifact/wire DTO | Layered representation | Keep wire records relocatable and versioned; centralize semantic validation so local construction and import cannot disagree. |
| Symbol ID, qualified name, binding ID, and operation ID | Different identities | Keep distinct. Symbols are spelling, qualified names are link identity, bindings are scope identity, and operations are typed occurrence identity. |
| source names, declaration qualified names, and artifact debug labels | Syntax/link identity versus diagnostics | Qualified declaration names are semantic linker identity; source symbols and debug labels are readback metadata. Preserve explicit projection and relocation, but never use a debug label to reconstruct a binding, declaration, or proof reference. |
| Universe Term variables and `UniverseDB` nodes/constraints | Syntax versus solver evidence | Keep both and add one compiler adapter mapping occurrence provenance to solver constraints. |

This audit changes two earlier instincts. First, “same mathematical phenomenon” does not imply “same record”: projection and weakening are the same structural phenomenon viewed as morphism and proof action, and both are required. Second, “different layer” does not justify copying the same enum or identity encoding: polarity aliases and operation-label bits create drift without adding semantics.

### 7.8 Normalization and graph mutation

Normalization may intern reduction results into `TermDB`. This is compatible with the project's computational-graph model and is not itself a defect. Correctness depends on:

- graph revision invalidating profile-specific normalization cache entries;
- conversion recording its reduction profile, fuel outcome, and reason;
- artifact publication tracing only named/rooted accepted content and its closure;
- host computations admitted to kernel conversion being deterministic, total under the admitted fragment, effect-free, environment-independent, and link-stable.

The current exclusion of host integer execution from pure type conversion is conservative and should remain until such a certified semantics exists.

## 8. Findings and Required Refactors

### Critical

#### F1. Normative HOTT fragment text describes incompatible historical versions

Impact: equality/HOTT implementation and artifact policy cannot be audited against one reliable specification. A reader of the manifest would believe Universe roots and proof tag 13 have meanings that the executable validator rejects or assigns differently.

Required action: rewrite the schema into a single current version, preserve a separate historical changelog, generate or mechanically compare all numeric vocabularies against C enums, decide whether Universe correspondence is compiler-local or persistable, and make extraction, interface validation, tests, fingerprints, and both manifests agree.

#### F1A. User-facing documentation advertised obsolete artifact versions (resolved by T0)

Audit-baseline impact: root `README.md` described artifact v61, while `src/prototype/README.md` said the reader accepted only v44. T0 updated both documents to v71 and added a specification-consistency test keyed from `src/prototype/spec/artifact_v71.schema`.

Resolution: both READMEs and their field descriptions now track v71. `tests/integration/test_spec_consistency.sh` rejects stale active version claims outside the archive and historical changelog.

#### F2. Object Identity and compiler relation terminology remain too easy to conflate

Impact: a future surface `Eq` could accidentally elaborate to compiler relation forms, reproducing the earlier parametricity/equality confusion.

Required action: physically namespace `identity/parametricity` and `identity/object_identity`; document core relation tags as compiler-internal; prohibit them in surface equality and object Identity artifact roots.

#### F3. Dependent CBPV residual verification lacks a complete normative soundness contract

Impact: static Claims, pending runtime checks, and backend execution could disagree about the type of a dependent fold result.

Required action: specify and test the obligation lifecycle before broadening dependent sequencing or artifact export.

### High

#### F4. Principal operation Claims are duplicated without a projection contract

Impact: the same operation/context/core/classifier tuple is manually materialized across lowering and rule producers, while broad statements that the whole proof graph is reconstructible encourage deletion of irreducible evidence.

Required action: add one principal-operation Proposition projection/check API; classify which Claims and rules are reconstructible; retain accepted-subset and Derivation semantics. Use reconstruction only as a differential audit until a proof-irrelevant or canonical proof fragment is formally specified.

#### F5. Effect operation identity is split between operation atoms and fixed-width label bits

Impact: `prototype_effect_operation_id` and `PROTOTYPE_TERM_EFFECT_ROW_OPERATION` can identify operation families structurally, but closed rows and handler subtraction return to `unsigned operation_labels`. This caps the intrinsic operation namespace at the machine bit width, prevents qualified user-defined operation identity from becoming the row authority, and makes higher-order operation atoms depend on a second identity mapping.

Required action: represent every closed operation row member by one stable operation identity/Term key; normalize rows as sorted unique atoms; make handler subtraction operate on those identities. Remove the lossy `unsigned effects` field from the general classifier view when its consumers have migrated. Preserve `required_host_effects` as a separate backend capability set and preserve oracle selection as a separate implementation mapping.

#### F5B. Intrinsic signatures have both full classifier Terms and closed schema enums

Impact: an `EFFECT_OPERATION` Term stores its classifier graph, but creation and several validators branch on `PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT` or `...THUNK_TEXT_TO_TEXT`. Pure primitive declarations similarly rebuild classifiers from host-type arrays. These are acceptable builtin constructors, but not independent semantic authorities. Extending the enum for every dependent/user operation would recreate constructor metadata/classifier-family divergence.

Required action: treat a validated classifier Term plus declaration Claim as the intrinsic's language signature. Host descriptors build/check that signature and map it to an ABI/oracle implementation. General user operations must use ordinary graph-level classifiers rather than new closed C enum cases.

#### F6. One-shot and abortive resumptions are checked by syntactic occurrence counting

Impact: the current check counts AST-binder uses in an Operation subtree. It cannot serve as a compositional proof under substitution, branch choice, graph sharing, Thunk/Force, optimizer rewrites, or nested handlers. It may conservatively reject valid terms and cannot be generalized into a full linear/resource-sensitive theory.

Required action: retain the current check only as a temporary syntactic guard. Design a per-judgement usage vector and quantitative substitution action before exposing general one-shot resources. Attach resource evidence to APP, Lambda, Match, fold, weakening, and reindex Derivations; keep effect rows orthogonal.

#### F7. CBPV category vocabularies are numerically duplicated

Impact: lowering refs, Operation nodes, classifier views, Judgement category queries, and HOTT relation goals can drift while describing the same Value/Computation/Type partition. The current private lowering enums already mirror public values manually.

Required action: introduce one scalar semantic-category and computation-kind vocabulary. Keep layer-specific nodes and rules, and represent unresolved elaboration state separately rather than as another near-identical enum.

#### F8. Classifier and effect constraint generation/solving have no single authority boundary

Impact: `lowering.c`, `classifier_solver.inc`, `conversion.inc`, and `cbpv.inc` can evolve into independent solvers over related mutable state.

Required action: define immutable constraint-set APIs, one solution object, explicit residuals, and proof materialization from that solution. Keep accepted replay search-free.

#### F9. Identity semantic validation is duplicated across generic declarations and artifacts

Impact: import and local compilation can accept different generated Identity shapes after one validator changes.

Required action: one shared object-Identity schema verifier; artifact code supplies relocated references and publication policy only.

#### F10. Universe collection creates a kernel-to-frontend dependency inversion

Impact: the Universe solver cannot be reused or audited independently of the lowering implementation.

Required action: move traversal from `kernel/universe.c` to a compiler adapter that emits provenance-bearing Universe constraints.

#### F11. Public headers expose implementation assembly rather than narrow contracts

Impact: physical file movement produces widespread recompilation and makes semantic dependency direction difficult to enforce.

Required action: split POD schema headers from builder/solver/runtime headers; use `kernel_view` and forward declarations where ownership permits.

### Medium

#### F12. `term.c`, `lowering.c`, `publication.c`, and `operation_graph.c` remain physical monoliths

Impact: reviews conflate unrelated changes and static helper ownership is unclear.

Required action: apply the physical splits described above with symbol/key/order equivalence tests. Do not introduce compatibility wrappers after callers are migrated.

#### F13. Match motive search and kernel rule construction are mixed

Impact: heuristic candidate selection is difficult to distinguish from trusted replay conditions.

Required action: move motive candidate discovery to elaboration; have kernel rule APIs consume an explicit selected motive plus premises.

#### F14. Driver diagnostics are duplicated

Impact: CLI and REPL report different states after one copy changes.

Required action: extract shared read-only diagnostic printers.

#### F15. Reader mixes parsing with compiler-session bootstrap

Impact: syntax testing requires semantic/link setup and parser ownership is obscured.

Required action: parser returns AST and syntax diagnostics only; driver/session code installs system declarations and external environments.

#### F16. Build source groups repeat physical members inside the manifest

Impact: a file split can update one aggregate group and silently omit another.

Required action: keep the one canonical manifest, but define non-overlapping base groups and compose target subsets from them while preserving deterministic order.

#### F17. Operation semantic fields mix authoritative occurrence facts with derived caches

Impact: operation polarity is intentionally occurrence-specific, while `application_role` is recomputed from Term shape and required to match it. `computation_kind` is initially a lowering approximation and later classifier-derived. Storing all three as undifferentiated integers makes callers assume the wrong authority.

Required action: document each field as source fact, solver state, or checked cache; derive application role from Term semantics or validate it through one projection helper; replace computation-kind approximation with an explicit unresolved/solved transition.

#### F18. HOTT search stages have typed records but an implicit aggregate state machine

Impact: relation goals, candidates, work items, residuals, actions, bridges, and certificates are individually justified, but legal transitions are distributed across several `.inc` files. Physical co-location or future compaction could accidentally publish a candidate as object Identity evidence or persist a residual as a closed action.

Required action: define one transition table and narrow APIs for goal -> candidates -> selected work outcome -> action request -> checked certificate/root. Retain the distinct records until that table proves two stages have identical lifetime and trust semantics.

## 9. Intentional Non-Unifications

The following pairs are related but must not be collapsed:

| Concepts | Why both remain |
| --- | --- |
| `TermDB` and `OperationGraph` | shared computation versus typed source occurrence |
| Context projection and weakening proof | semantic substitution versus evidence of proposition stability |
| Proposition, Claim, Derivation | statement, acceptance, and proof DAG are different proof-theoretic objects |
| Candidate and accepted derivation | solver frontier versus replayable certificate |
| Definitional conversion and object Identity | kernel computation judgement versus an object-language type and inhabitant |
| Parametric relation and object Identity | relation-preservation substrate versus equality structure selected by a type former |
| Verification obligation and Claim | conditional runtime check versus closed accepted theorem |
| Constructor telescope and curried Pi classifier | declaration schema versus derived callable classifier |
| APP, Match, IH, and fold elimination rules | similar plumbing but different theorems and replay conditions |
| Value and computation classifiers | CBPV semantic polarity; share storage but not typing rules |
| OperationRequest and ordinary APP | effect request/continuation must remain inspectable by handlers |
| Effect row and resource usage | permitted operations versus consumption/duplication of bindings and resumptions |
| Context dependency and per-judgement usage vector | available dependent variables versus how one derivation consumes them |
| Language operation identity and host capability | algebraic operation label versus permission to invoke a backend facility |
| HOTT residual and runtime verification obligation | unfinished compile-time Identity action versus conditional runtime type check |
| Structural type representation and nominal TypeView | alpha-aware shape equivalence versus user-selected semantic name/view |

## 10. Target Module Boundaries

The target remains a single semantic graph architecture with narrower implementation modules:

```text
support/
    symbol

core/
    term_schema
    term_db
    term_shape
    term_build
    term_substitution
    term_normalization
    host_semantics
    term_print

frontend/
    lexer_reader
    ast
    name_resolution
    graph_lowering
    constraint_generation
    compile_pipeline

elaboration/
    classifier_constraints
    effect_constraints
    match_motive_solver
    solution_materialization

graph/
    operation_graph
    verification_obligation
    runtime_execution

kernel/
    context_substitution
    cwf_certificate
    proposition_claim_derivation
    conversion
    rule_families
    accepted_replay
    universe_constraints
    universe_solver

identity/
    parametricity/
    object_identity/
    context_bridge
    capability_matrix
    action_certificate

artifact/
    interface_model
    closure
    canonical_order
    relocation
    wire_v71
    link

driver/
    compiler_session
    module_resolver
    diagnostics
    read_file
    repl
```

These are ownership boundaries, not a request for separate databases or separate calculi.

## 11. Refactoring Roadmap

### T0. Restore one normative truth

- [x] Update root `README.md` artifact v61 references and field descriptions to v71.
- [x] Update `src/prototype/README.md` artifact v44 header/reader claim and field descriptions to v71.
- [x] Add a stale-version documentation check keyed from `spec/artifact_v71.schema`.
- [x] Rename the stale HOTT manifest to `hott_fragment_v5.schema` and correct all active version/proof references.
- [x] Record the exact implemented Identity capability matrix and align closed ADT/Pi/Thunk artifact capability with the executable validator.
- [x] State that compiler relation forms are not surface/object Identity.
- [x] Specify dependent verification obligation lifecycle.
- [x] Add links from artifact v71 documentation to these contracts.

Implementation evidence: `tests/integration/test_spec_consistency.sh` checks the
README/schema version, both manifest fingerprints, the current HOTT fragment
number, the compiler-local Universe-root exclusion, and all manifest numeric
vocabularies through `tests/checks/spec_enum_check.c`. Artifact, HOTT, and CBPV
integration tests pass after a forced rebuild.

Exit criterion: every artifact-persisted HOTT/CBPV object has one current normative definition.

### T1. Narrow dependencies without behavior changes

- [x] Extract shared scalar IDs and qualified-name schemas from `term.h`/`type_declaration.h` dependency cycle.
- [x] Split aggregate public headers by model, builder, solver, runtime, and wire APIs.
- [x] Move Universe collection traversal above the kernel solver.
- [x] Extract shared driver diagnostics.
- [x] Move reader session/bootstrap logic to a compiler-session module.

Implementation evidence: shared scalar identities now live in
`support/schema.h`; the Universe DB/solver and frontend collector are separate
translation units; REPL and file compilation use `driver/diagnostics.c`; and
system declaration installation, graph compilation, and external-reference
linking live in `driver/compiler_session.c`. Operation records, graph builders,
verification, and runtime APIs now have separate headers, and artifact v71 wire
entry points no longer live in the interface-model header. Every public header
also passes a first-include `-Wall -Wextra -Werror` check.

Exit criterion: dependency graph follows support -> core -> kernel model -> elaboration/identity/artifact -> drivers, with explicitly documented callbacks for reverse services.

### T2. Normalize the OperationGraph-to-proof boundary

- [x] Add a single function that projects the principal operation Proposition.
- [x] Reject any operation-owned principal Claim whose header differs from that projection.
- [x] Inventory every accepted proof kind as principal-reconstructible, derived-operation, scoped, or non-operation.
- [x] Add a reconstruction audit that regenerates principal proof candidates without publishing them.
- [x] Compare regenerated proposition/rule/premise keys with the accepted DAG.
- [x] Measure potential Claim-wrapper compaction separately from Derivation retention.
- [x] Preserve every accepted Derivation and all current Claim references during this phase.

Implementation evidence: `prototype_judgement_project_principal_operation_proposition`
is the sole projection of an Operation-owned principal `HAS_TYPE` header.
Accepted replay runs a read-only principal audit which classifies every known
proof kind using both rule and Proposition authority, reconstructs the allowed
syntax-directed rule and structural premise owners from `OperationGraph`, and
compares them with replay views of the accepted DAG. The audit records total
Proposition/Claim counts and Claim-wrapper bytes but performs no publication,
interning, compaction, or Derivation selection. The focused context-category
check verifies all proof tags are inventoried, malformed principal headers are
rejected, and all DB counts and intern-hit counters remain unchanged. Artifact
v70 replay passes with all accepted Derivations retained.

Exit criterion: recoverable operation headers are generated once, while acceptedness, proof multiplicity, scoped premises, and non-operation evidence remain explicit and unchanged.

### T3. Normalize shared semantic vocabularies and effect identity

- [x] Replace private lowering polarity/computation-kind aliases with shared scalar enums.
- [x] Separate unresolved elaboration state from established CBPV category/kind.
- [x] Make Operation `application_role` a projection or explicitly checked cache.
- [x] Replace fixed-width operation-label membership with stable operation atoms.
- [x] Make validated classifier Terms the authority for intrinsic/operation signatures; demote closed schema enums to builtin builders.
- [x] Preserve host capability bits and oracle/backend selection as separate mappings.
- [x] Rename `TypeCodeShapeKey` to describe a structural representation fingerprint.

Implementation note (2026-08-11): language effect rows now normalize stable
`EFFECT_ROW_OPERATION(operation_id, latent_row)` atoms and no longer contain a
fixed-width membership mask.  Empty rows, operation atoms, row variables, and
unions share one normal-form implementation.  Host capability flags and host
oracle dispatch remain separate backend mappings.  OperationGraph validation
recomputes and checks `application_role`; unresolved classifier variables remain
solver state and are not published as established CBPV categories.  The T3
regression run includes the CBPV surface, CBPV boundary, resumption multiplicity,
and artifact-flow suites.  During that run, Context resolution was also corrected
so an unresolved Context classifier can never overwrite a previously established
pattern-assumption classifier with `INVALID_ID`.

Exit criterion: each semantic scalar and each operation identity has one definition, while occurrence, proof, host capability, and wire-layer distinctions remain intact.

### T4. Establish solver authority

- [x] Define immutable classifier and effect constraint sets.
- [x] Define one solution/residual object keyed by operation occurrence.
- [x] Convert judgement-rule effect equations to occurrence solver constraints through one checked API.
- [x] Materialize proof candidates only from the solution object.
- [x] Prove by tests that accepted replay does not perform search or choose a different classifier.

Implementation note (2026-08-11): classifier equations and their mutable
results are now separate arrays.  Effect equation identity is immutable during
solving; solver progress lives in `operation_effect_solver.constraint_states`
and is copied to compile metadata only as a diagnostic/artifact projection.  A
single `operation_solver_solution` cell owns the classifier, effect row, state,
and residual reason for each Operation occurrence.  Judgement effect equations
enter that solver only through the checked occurrence-import adapter.

Computation request and fold rules now synthesize `solved_classifier` before
proof closure.  If the rule-specific result refines an older OperationGraph
projection, the solver receives the refinement while proof publication is
withheld; a later fixed-point pass must project the exact same classifier before
the Derivation can be emitted.  This removed a stale higher-order request result
that previously lost the latent `print` effect from a thunked argument.  The
strict CBPV regression now requires the outer request and its accepted proof to
use `Comp({scope_text, print}, Text)`, and artifact readback replays that exact
Claim.  The principal-operation audit additionally rejects an accepted header
whose classifier differs from the Operation projection and verifies that its
read-only reconstruction leaves all DB counts and intern-hit counters unchanged.

Exit criterion: each solved classifier/effect row has one producer, and every accepted proof independently validates it.

### T5. Prepare resource-sensitive structural evidence

- [x] Select the usage algebra/semiring and state weakening, contraction, and substitution laws.
- [x] Define a per-judgement usage vector indexed by persistent Context bindings.
- [x] Define quantitative substitution actions and composition; do not store one consumed multiplicity on a Context entry.
- [x] Materialize resource premises for variable, Lambda, APP, Match, fold, weakening, and reindex rules.
- [x] Replace one-shot AST occurrence counting with checked usage evidence.
- [x] Specify whether Thunk is unrestricted, affine, linear, or grade-indexed when it captures resources/resumptions.
- [x] Specify how HOTT Context bridges distribute grades to endpoints and relation witnesses.

Implementation note (2026-08-11): resource evidence uses the saturated
`{ZERO, ONE, MANY}` usage semiring. Sequential composition is saturated
addition, mutually exclusive branches use maximum, and substitution scales the
usage vector of each source assignment by the target binding grade before
adding columns. Usage vectors are sorted by persistent `binding_id`, omit ZERO,
and are part of immutable Proposition/Claim identity. They are not mutable
consumption counters on Context entries.

`graph/operation_usage.c` is the shared occurrence analysis used by lowering
and read-only accepted replay. Lambda discharges its occurrence binding, APP
uses the callable binder grade, Match and handler clauses join alternatives,
and fold/request rules account for abortive, one-shot, and multi-shot
resumptions. Every operation-owned principal Claim is created with its complete
usage vector; Derivation premises copy the exact evidence vector. Projection
weakening preserves persistent IDs, while general CwF reindexing analyzes each
substitution term and applies the quantitative substitution law. Artifact v71
serializes this Proposition identity and rejects malformed grades, ordering, or
out-of-Context bindings during replay.

Thunk is grade-preserving: quotation does not confer contraction on captured
resources or resumptions. The current object-Identity bridge has no graded
modality for distributing one source use among its left endpoint, right
endpoint, and relation witness. Object-term HOTT actions with nonempty resource
usage are therefore residual instead of silently duplicating evidence.

Exit criterion: one-shot/abortive acceptance is compositional under substitution and graph sharing, and explicit weakening/contraction policy is visible in replayable evidence.

### T6. Specify and test dependent CBPV

- [x] Formalize zero-clause sequencing and clause-bearing fold typing separately.
- [x] Define dependent continuation substitution by binding ID and CwF substitution.
- [x] Define static closure, residual creation, runtime discharge, and failure.
- [x] Test effect-blocked and fuel-exhausted obligations across artifact round trips.
- [x] State whether the supported fragment is dCBPV-, dCBPV+, or an explicit gradual/residual extension.

Implementation note (2026-08-11): the core keeps one `COMPUTATION_FOLD` tag,
but typing and replay distinguish its zero-clause sequencing rule from its
return-plus-operation-clause handler rule. Continuations bind by persistent
binding identity and dependency is transported by the existing CwF
substitution/reindex action; no runtime result is substituted into a static
classifier during compilation.

The supported language is not full dCBPV+. It is the implemented value-
dependent dCBPV- fragment plus an explicit gradual/residual extension for
dependent sequencing. Deterministic pure normalization may close the result
family statically. Effect-blocked or fuel-exhausted closure creates a versioned
`COMPUTATION_FOLD_RESULT` runtime contract, never an accepted Claim. Strict
compilation rejects the contract; hybrid artifacts persist PENDING contracts;
runtime evaluation discharges or fails a frame-local copy without modifying
the static proof graph. CBPV surface tests cover static closure, residual
creation, strict rejection, artifact write/read/link, backend capability
checks, successful discharge, and failed/exhausted normalization behavior.

Exit criterion: dependent results cannot enter accepted Claims without either static evidence or a separately tracked runtime contract.

### T7. Separate parametricity from object Identity

- [x] Move compiler logical relation action under a parametricity namespace.
- [x] Keep object Identity family generation and term witnesses under object Identity.
- [x] Introduce the type-former capability matrix as executable validation data.
- [x] Add transport/lifting only after rules, computation behavior, and coherence obligations are specified.
- [x] Add non-DefEq positive and distinguishable negative observational test cases per supported type former.

Implementation note (2026-08-11): compiler relation planning now lives under
`include/a_program/parametricity` and `src/parametricity`; its planner structs
and public functions use the `prototype_parametricity_` prefix. Object Identity
family computation and witness construction remain under `identity`. The
executable type-former descriptor is the authority for supported/deferred
capabilities. General transport and lifting remain explicitly `DEFERRED`; this
checklist item records that gate, not an implementation of those operations.
The HOTT integration suite retains non-DefEq extensional Bool-function
identity, distinguishable negative cases, Identity-of-Identity, and residual
coverage for unsupported formers.

Exit criterion: documentation, APIs, terms, Claims, and artifact roots cannot confuse a relation-preservation certificate with an Identity inhabitant.

### T8. Consolidate semantic artifact validation

- [x] Build one object-Identity schema verifier shared by local generation and import.
- [x] Keep wire validation structural and accepted replay logical.
- [x] Preserve sparse-slot `present` checks and relocation closure.
- [x] Add corrupted-root tests that fail at the intended layer.

Implementation note (2026-08-11): both writer-side root publication and
reader-side import call
`prototype_artifact_interface_validate_identity_roots`. Text parsing remains a
wire/shape operation; imported Claims are accepted only after the normal
accepted-Derivation replay. Dense publication validates the relocated Identity
roots with the same schema verifier. Existing sparse-slot presence and
relocation-closure checks remain active. Forged source/rule/witness/context/
substitution/proof/term references and v71 resource-usage corruption are
rejected by the layer that owns each invariant.

Exit criterion: local and imported object Identity evidence is accepted by the same semantic rules.

### T9. Perform physical source compaction

- [x] Split `term.c` by concern without changing tags or public behavior.
- [x] Split `lowering.c` along the established phase APIs.
- [x] Split artifact publication into closure/order/slice/writer.
- [x] Split OperationGraph storage, verification, and runtime.
- [x] Split `introduction_identity.inc` into structural and relation-rule partitions.
- [x] Split Match search from Match rule emission.
- [x] Freeze per-file implementation/header LOC before the first move.
- [x] Record, for every old and new file, baseline/final physical LOC,
      `git diff --numstat` additions and deletions, net change, responsibility
      moved or deduplicated, and the reason for any positive net growth.
- [x] Report exact duplicate LOC removed separately from code merely moved or
      split, and report generated/test/specification/documentation changes in
      separate totals.
- [x] Produce both phase-local tables and one consolidated final per-file table,
      followed by repository totals.
- [x] Treat LOC reduction as a preferred consequence of genuine consolidation,
      not as authority to erase distinct proof rules or semantic checks.

T9 physical LOC report (implementation lines, `wc -l`, frozen immediately
before each move):

| Old owner | Baseline | New physical owners | Final total | Net | Exact duplicate removed |
| --- | ---: | --- | ---: | ---: | ---: |
| `core/term.c` | 10,044 | 5 concern `.inc` files + 8-line owner | 10,048 | +4 | 0 |
| `frontend/lowering.c` | 17,701 | 4 phase `.inc` files + 6-line owner | 17,281 | -420 | 424 |
| `artifact/publication.c` | 6,238 | 5 closure/slice/writer `.inc` files + 7-line owner | 6,242 | +4 | 0 |
| `graph/operation_graph.c` | 2,329 | 4 storage/verification/runtime `.inc` files + 6-line owner | 2,332 | +3 | 0 |
| `kernel/rules/introduction_identity.inc` | 1,867 | structural + relation witness partitions + 4-line owner | 1,870 | +3 | 0 |
| `kernel/rules/match.inc` | 2,893 | candidate search + 2 rule-emission partitions + 5-line owner | 2,897 | +4 | 0 |
| **Implementation total** | **41,072** | **29 partitions/owners** | **40,670** | **-402** | **424** |

Consolidated physical owner table:

| Original owner or new partition | Baseline LOC | Final LOC | Responsibility |
| --- | ---: | ---: | --- |
| `core/term.c` | 10,044 | 8 | One-TU owner |
| `core/term/declarations.inc` | 0 | 652 | declarations and private setup |
| `core/term/canonicalization.inc` | 0 | 2,021 | canonical keys and interning |
| `core/term/storage_and_formation.inc` | 0 | 2,255 | node storage and formation |
| `core/term/substitution.inc` | 0 | 1,210 | substitution and reindexing |
| `core/term/evaluation_and_conversion.inc` | 0 | 3,902 | reduction, normalization, and conversion |
| `frontend/lowering.c` | 17,701 | 6 | One-TU owner |
| `frontend/lowering/context_and_type_lowering.inc` | 0 | 3,851 | context and type lowering |
| `frontend/lowering/graph_construction.inc` | 0 | 6,353 | OperationGraph construction |
| `frontend/lowering/constraint_solver.inc` | 0 | 6,175 | constraint generation and solving |
| `frontend/lowering/finalization_and_entrypoints.inc` | 0 | 896 | finalization and public entrypoints |
| `artifact/publication.c` | 6,238 | 7 | One-TU owner |
| `artifact/publication/wire_primitives.inc` | 0 | 337 | textual wire primitives |
| `artifact/publication/closure_marking_and_slices.inc` | 0 | 1,683 | closure and graph slices |
| `artifact/publication/section_writers.inc` | 0 | 1,387 | section serialization |
| `artifact/publication/dense_publication.inc` | 0 | 2,142 | dense relocation/publication |
| `artifact/publication/writer.inc` | 0 | 686 | publication orchestration |
| `graph/operation_graph.c` | 2,329 | 6 | One-TU owner |
| `graph/operation/storage.inc` | 0 | 280 | operation storage |
| `graph/operation/graph_validation.inc` | 0 | 251 | graph invariants |
| `graph/operation/verification.inc` | 0 | 368 | verification obligations |
| `graph/operation/runtime.inc` | 0 | 1,427 | operation runtime |
| `kernel/rules/introduction_identity.inc` | 1,867 | 4 | One-TU owner |
| `kernel/rules/introduction/structural.inc` | 0 | 667 | ordinary structural introduction rules |
| `kernel/rules/introduction/relation_witness.inc` | 0 | 1,199 | relation-witness rules |
| `kernel/rules/match.inc` | 2,893 | 5 | One-TU owner |
| `kernel/rules/match/motive_rule_emission.inc` | 0 | 1,348 | motive and rule emission |
| `kernel/rules/match/candidate_search.inc` | 0 | 999 | Match candidate search |
| `kernel/rules/match/expansion_rule_emission.inc` | 0 | 545 | expansion rule emission |

Staged Git `numstat` against the pre-T0 repository commit is recorded
separately below. It includes semantic edits made during T0-T8, so its owner
deletion counts are intentionally not treated as the T9 physical baseline.

| Path | Added | Deleted |
| --- | ---: | ---: |
| `artifact/publication.c` | 7 | 6,155 |
| `artifact/publication/closure_marking_and_slices.inc` | 1,683 | 0 |
| `artifact/publication/dense_publication.inc` | 2,142 | 0 |
| `artifact/publication/section_writers.inc` | 1,387 | 0 |
| `artifact/publication/wire_primitives.inc` | 337 | 0 |
| `artifact/publication/writer.inc` | 686 | 0 |
| `core/term.c` | 8 | 10,027 |
| `core/term/canonicalization.inc` | 2,021 | 0 |
| `core/term/declarations.inc` | 652 | 0 |
| `core/term/evaluation_and_conversion.inc` | 3,902 | 0 |
| `core/term/storage_and_formation.inc` | 2,255 | 0 |
| `core/term/substitution.inc` | 1,210 | 0 |
| `frontend/lowering.c` | 6 | 16,883 |
| `frontend/lowering/constraint_solver.inc` | 6,175 | 0 |
| `frontend/lowering/context_and_type_lowering.inc` | 3,851 | 0 |
| `frontend/lowering/finalization_and_entrypoints.inc` | 896 | 0 |
| `frontend/lowering/graph_construction.inc` | 6,353 | 0 |
| `graph/operation_graph.c` | 6 | 2,324 |
| `graph/operation/graph_validation.inc` | 251 | 0 |
| `graph/operation/runtime.inc` | 1,427 | 0 |
| `graph/operation/storage.inc` | 280 | 0 |
| `graph/operation/verification.inc` | 368 | 0 |
| `kernel/rules/introduction_identity.inc` | 4 | 1,769 |
| `kernel/rules/introduction/relation_witness.inc` | 1,199 | 0 |
| `kernel/rules/introduction/structural.inc` | 667 | 0 |
| `kernel/rules/match.inc` | 5 | 2,894 |
| `kernel/rules/match/candidate_search.inc` | 999 | 0 |
| `kernel/rules/match/expansion_rule_emission.inc` | 545 | 0 |
| `kernel/rules/match/motive_rule_emission.inc` | 1,348 | 0 |

Each mechanical split was checked by comparing the SHA-256 of the original
file with the concatenation of its partitions before adding the owner wrapper.
Moved lines are not counted as deduplication. The only exact duplicate removal
is the disabled private lowering usage solver superseded by
`graph/operation_usage.c`. Wrapper comments explain positive growth. Public
headers, tests, specifications, and documentation are excluded from this
implementation table and remain visible in `git diff --numstat`.

Exit criterion: builds, examples, artifact bytes/round trips, canonical keys, Claim/Derivation counts, and normalization results match the pre-refactor baseline unless a separately approved semantic change says otherwise.

## 12. Required Regression Matrix

Every semantics-preserving phase must compare at least:

- [x] all supported examples through example 09;
- [x] same core Lambda under Bool and Nat operation classifiers;
- [x] `List Nat` type-argument instantiation followed by Match;
- [x] one-level and nested recursive `*rest` append/double reduction;
- [x] `:whnf` and `:nf` outputs;
- [x] normalization cache hit/miss and graph revision behavior;
- [x] OperationGraph validation and selected classifier IDs/keys;
- [x] projected principal operation Propositions exactly matching accepted principal Claims;
- [x] reconstruction audit matching every classified reconstructible proof;
- [x] non-operation and scoped Claims remaining absent from the reconstructed subset;
- [x] Proposition, Claim, Derivation, and premise-edge counts;
- [x] multiple derivations concluding one Claim;
- [x] derivation-order reversal preserving HOTT results without erasing derivation multiplicity;
- [x] CwF projection/weakening semantic action references;
- [x] shared Value/Computation/Type categories agreeing across lowering, Operation, Judgement, and HOTT views;
- [x] effect-row solved and residual states;
- [x] operation-atom union, inclusion, and handler subtraction without fixed-width identity assumptions;
- [x] one-shot/abortive resumption tests under substitution, branch alternatives, Thunk capture, and shared subgraphs;
- [x] quantitative weakening, contraction rejection, and substitution-composition tests once resource evidence is introduced;
- [x] dependent fold verification obligation creation/discharge/failure;
- [x] artifact v71 write/read/republish and sparse-slot corruption rejection;
- [x] README/current-schema artifact version consistency, excluding explicit archive/history documents;
- [x] non-DefEq object Identity witness examples;
- [x] identity-of-identity examples without proof irrelevance collapse;
- [x] unsupported Universe/HOTT cases remaining residual or rejected, never silently accepted.

## 13. Final Assessment

The repository is not architecturally unsalvageable and does not need a new compiler core. Its central choices are defensible:

- one shared computational `TermDB`;
- a distinct occurrence-level `OperationGraph`;
- CwF contexts and substitutions with binding identity;
- immutable propositions, accepted claims, and a multi-derivation DAG;
- explicit CBPV and effect nodes;
- separation of accepted proofs from runtime verification obligations;
- separation, now present in data structures, between parametric relation action and object Identity action;
- rooted artifact publication instead of serializing the entire compiler workspace.

The primary debt is boundary enforcement. Several distinctions exist in comments and data fields but are not enforced by narrow APIs or module dependencies. The next work should therefore begin with T0 through T6, not with indiscriminate file splitting.

The extended duplication audit does not recommend collapsing the proof architecture into TermDB or OperationGraph. It does recommend eliminating scalar aliases, fixed-width duplicate operation identity, and manually repeated principal-operation headers. Conversely, it strengthens the case for explicit substitutions, weakening/conversion evidence, operation requests, and proof Derivations. Those records are precisely where resource usage, CBPV modality behavior, and higher proof relevance must eventually be checked.

The HOTT direction also remains viable, but the implemented result must be stated precisely: a logical-relation substrate, a supported closed one-dimensional object Identity fragment, selected higher examples, and incomplete general transport/Universe/coherence. Progress toward full HOTT should be measured by the type-former capability matrix rather than by the presence of generic relation tags.

Finally, source reduction is not the main success metric. Removing duplicated driver printers or duplicated validators should reduce code. Keeping explicit proof validators may not. The stronger metric is that each semantic fact has one authority, each derived view names its source, every accepted Claim has replayable premises, and every unresolved computation remains visibly residual rather than becoming an accidental theorem.
