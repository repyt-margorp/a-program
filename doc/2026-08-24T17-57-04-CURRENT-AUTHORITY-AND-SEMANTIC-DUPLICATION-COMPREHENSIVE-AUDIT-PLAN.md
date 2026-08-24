# Current Authority and Semantic Duplication Comprehensive Audit Plan

Date: 2026-08-24

Status: complete; audit only; no semantic refactoring was performed

Repository baseline:

- branch: `main`;
- commit: `14b51b1`;
- artifact format: v84;
- implementation root: `src/prototype/`;
- implementation LOC, excluding specs and tests: 154,253; and
- implementation, specs, and test LOC: 186,454.

Related documents:

- `2026-08-16T14-43-28-SEMANTIC-STORE-AUTHORITY-AND-PHYSICAL-CONSOLIDATION-PLAN.md`;
- `2026-08-16T20-42-43-POST-DA-SEMANTIC-AUTHORITY-RESIDUAL-CONSOLIDATION-PLAN.md`;
- `2026-08-19T23-28-49-OPEN-ISSUES-11-13-14-16-17-18-INTEGRATED-AUTHORITY-IMPLEMENTATION-PLAN.md`;
- `2026-08-23T20-08-44-COMPILER-PERFORMANCE-ALGORITHM-REMOVAL-IMPLEMENTATION-PLAN.md`;
- `2026-08-23T23-40-38-PR20-MEANING-BOUNDARY-CRITICAL-REVIEW-AND-IMPLEMENTATION-PLAN.md`; and
- `src/prototype/spec/artifact_v84.schema`.

Completed audit report:

- `2026-08-24T18-14-17-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-AUDIT-REPORT.md`.

## 1. Purpose

This plan defines a repository-wide audit of semantic authority, duplicated
representation, duplicated algorithms, and abstractions that may allow several
individual mechanisms to be implemented from one common concept.

The audit must answer four different questions without conflating them:

1. Does one semantic fact have more than one mutable owner?
2. Is one immutable fact copied into several derived views with an explicit
   source and validation boundary?
3. Is equivalent C plumbing repeated even though the semantic rules must remain
   distinct?
4. Are superficially similar concepts being incorrectly proposed for
   unification?

The final output of this work is not an immediate patch. It is an evidence-based
inventory and a prioritized implementation plan produced only after every
section in this document has been audited.

### 1.1 Why a new audit is required

The older authority plans cannot be resumed mechanically:

- their baselines range from artifact v76 to v83;
- later work implemented some planned changes through different migrations;
- artifact v84 introduced exact conditional evidence and new validation
  boundaries;
- some old findings are now resolved, while their checklist remains open; and
- current code contains new function-graph, identity, totality, and performance
  machinery that those plans did not cover.

Every old finding is therefore only a lead. Current source code and current
tests are authoritative for this audit.

### 1.2 Non-goal

This audit does not seek the smallest possible number of structs, enums, tags,
or source files. A smaller implementation is useful only when it preserves the
meaning of each layer and leaves one explicit authority for every semantic
fact.

During the audit:

- do not repair a finding as soon as it is discovered;
- do not add compatibility adapters to reduce migration size;
- do not merge databases merely because their append/intern code looks alike;
- do not convert a semantic distinction into a mode flag without proving that
  the data and invariants are actually identical; and
- do not count generated files or documentation as implementation reduction.

## 2. Project Invariants

The audit starts from the following A Program invariants.

1. Compilation and type checking are bounded earlier computation. Runtime is a
   later computation phase, not a different language semantics.
2. `TermDB` is the canonical context-free computation graph.
3. Typed occurrences, source provenance, Context, and proof provenance belong
   above the context-free Term graph.
4. Value/computation polarity must be visible to typing and CBPV rules without
   duplicating the common Term vocabulary into Value-side and
   Computation-side graph tags.
5. One semantic fact has one mutable authority. Other copies must be immutable
   projections, caches, wire records, diagnostics, or algorithm-local state.
6. A derived view must name its source, construction point, invalidation rule,
   and disagreement behavior.
7. A cache must be removable and reconstructible without changing accepted
   meaning.
8. An accepted Claim is not an unsolved Constraint or a residual Verification
   Obligation.
9. `::` is post-synthesis expectation checking. It must not become a source of
   bidirectional elaboration authority.
10. Namespace, TypeView, and labels may distinguish operation-level meaning but
    must not become hidden computation identities.
11. No migration-size argument justifies retaining an obsolete authority or a
    remap facade.

## 3. Classification Vocabulary

Every audited field, table, or API must receive exactly one primary role.

| Role | Meaning |
| --- | --- |
| `SEMANTIC_AUTHORITY` | The unique mutable or persistent owner of a fact |
| `DERIVED_PROJECTION` | An immutable view computed from named authority |
| `ALGORITHM_WORKSPACE` | Disposable state used only while solving or traversing |
| `REBUILDABLE_CACHE` | Performance state whose deletion changes no meaning |
| `DIAGNOSTIC_SNAPSHOT` | Immutable reporting data that cannot affect acceptance |
| `WIRE_EVIDENCE` | Serialized evidence checked against in-memory authority |
| `PHASE_RESIDUAL` | Explicit pending condition owned across a phase boundary |
| `PRESENTATION_DATA` | Names and readback data with no kernel authority |
| `LEGITIMATE_DISTINCTION` | Similar representation with a different theorem or role |
| `DUPLICATE_AUTHORITY` | Multiple mutable owners for the same semantic question |
| `PHYSICAL_DUPLICATION` | Repeated storage or traversal code without duplicate meaning |
| `UNSUPPORTED_CLAIM` | A suspected duplication not supported by current code |

### 3.1 Required authority test

For every suspected duplicate, record answers to all of these questions:

1. What exact semantic question does each record answer?
2. Which function creates it?
3. Which functions mutate it?
4. Which functions can make an acceptance or rejection decision from it?
5. Can the records disagree, and what happens when they do?
6. Is one record reconstructed from another?
7. Is either record serialized or exported?
8. What invalidates each record?
9. Can either record be removed without changing accepted artifacts?
10. Does unification reduce repeated meaning, or merely hide different rules?

No finding may be classified as `DUPLICATE_AUTHORITY` from matching field names
or similar struct shapes alone.

## 4. Audit Artifacts

The audit will maintain these tables in this document or in dated companion
reports linked from it.

### 4.1 Semantic owner ledger

| Fact ID | Semantic fact | Authority | Writers | Derived consumers | Persistence | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending | pending | pending |

### 4.2 Duplication finding ledger

| Finding ID | Candidate duplication | Classification | Evidence | Proposed response |
| --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending |

### 4.3 Physical consolidation ledger

| Mechanism | Current implementations | Shared invariant | Rule-specific behavior | Consolidation candidate |
| --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending |

### 4.4 Negative decision ledger

This ledger is mandatory. It records tempting unifications that were rejected
and why, so a later audit does not reopen them without new evidence.

| Concepts | Why they look similar | Required distinction | Decision |
| --- | --- | --- | --- |
| pending | pending | pending | pending |

## 5. Progress Dashboard

| Phase | Scope | Status | Output |
| --- | --- | --- | --- |
| AU0 | Freeze baseline and establish measurement method | complete | report sections 2 and 8 |
| AU1 | Term, AST lowering, and TypedOccurrence ownership | complete | report sections 3, 6, and 8 |
| AU2 | Constraint generation, solving, and solution projection | complete | report F03 |
| AU3 | Context, substitution, binding, and resource usage | complete | report sections 3 and 5.1 |
| AU4 | Type declaration schema, representation, readback, and caches | complete | report F05 and 5.2 |
| AU5 | Proposition, Claim, Derivation, and accepted replay | complete | report F06 |
| AU6 | Evaluation, conversion, reduction profiles, and memoization | complete | report F04 |
| AU7 | CBPV, effects, folds, residuals, and host operations | complete | report F09 and 5.3 |
| AU8 | Universe formation and constraint solving | complete | report F01 and F02 |
| AU9 | Identity, parametricity, dimensions, and HOTT actions | complete | report F08 |
| AU10 | Generated function graphs, totality, and indexed families | complete | report sections 7 and 8 |
| AU11 | Artifact, interface, publication, relocation, and replay | complete | report F01 and section 10 |
| AU12 | Namespaces, external names, TypeViews, and labels | complete | report 5.4 and section 6 |
| AU13 | Driver storage, indexes, transactions, and common C machinery | complete | report F07 and F10 |
| AU14 | Tests, diagnostics, counters, and documentation | complete | report F09 and section 2 |
| AU15 | Integrated synthesis and implementation-plan extraction | complete | report sections 9 and 10 |

## 6. AU0: Baseline and Method

### 6.1 Tasks

- [x] Record commit, artifact version, compiler build options, and dirty-tree
      state.
- [x] Count source LOC by module, excluding generated parser/lexer output.
- [x] Capture the full test-suite result and wall-clock time.
- [x] Capture focused IF8, function-graph QuickSort, artifact, and HOTT timings.
- [x] Inventory every database, arena, store, index, cache, snapshot, and
      externally visible ID type.
- [x] Inventory every serialization root and publication root.
- [x] Produce a static writer/reader list with `rg`, call-site inspection, and
      targeted instrumentation where static evidence is insufficient.
- [x] Mark earlier plan findings as `CURRENT`, `RESOLVED`, `SUPERSEDED`, or
      `UNSUPPORTED`; do not inherit their status unchecked.

### 6.2 Baseline performance references

Current known references, to be remeasured rather than assumed:

- complete suite: 42/42 in approximately 17.569 seconds;
- IF8 focused compile: approximately 181 ms; and
- function-graph QuickSort: approximately 1.709 seconds.

### 6.3 Exit gate

- [x] Every store has an initial stated purpose.
- [x] Every mutable store has at least one identified writer.
- [x] Measurements can be reproduced from recorded commands.
- [x] No implementation file has changed.

## 7. AU1: Term, Lowering, and Typed Occurrences

### 7.1 Scope

- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/src/core/term/`;
- `src/prototype/src/frontend/lowering/`;
- `src/prototype/include/a_program/graph/typed_occurrence_graph.h`;
- `src/prototype/include/a_program/graph/typed_occurrence_model.h`; and
- `src/prototype/src/graph/typed_occurrence_graph.c`.

### 7.2 Questions

- [x] Confirm that `TermDB` owns erased computation identity only.
- [x] Confirm that source location, Context, classifier, effect, usage, and
      proof provenance occur only in occurrence-level structures.
- [x] Find remaining OperationGraph-era records that duplicate a Term node
      instead of annotating a Term occurrence.
- [x] Compare APP, LAMBDA, MATCH, constructor, induction-hypothesis,
      RETURN, THUNK, FORCE, operation-request, and computation-fold payloads
      across AST, lowering, Term, and typed occurrence records.
- [x] Distinguish temporary AST syntax from duplicated semantic graph state.
- [x] Verify that alpha-interning and binding identity do not depend on surface
      names or De Bruijn reindexing.
- [x] Check whether the same lowering traversal is reimplemented for typing,
      function graphs, identity actions, and artifact replay.
- [x] Identify common graph-walk mechanics that can be shared without merging
      the semantic actions performed by each walk.

### 7.3 Required non-unifications

- `TermDB` must not absorb occurrence Context or proof provenance.
- The typed occurrence graph must not become a second computational Term graph.
- Value-side and Computation-side copies of APP or LAMBDA must not be
  introduced merely to encode polarity.
- MATCH and computation-fold must not be erased into APP merely because an
  encoding exists.

### 7.4 Deliverable and exit gate

- [x] A node-by-node ownership matrix exists.
- [x] Every duplicate payload is classified as syntax, authority, projection,
      or workspace.
- [x] Any proposed shared traversal names its callback contract and preserves
      rule-specific validation.

## 8. AU2: Constraints, Solvers, and Solution Projection

### 8.1 Scope

- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`;
- `src/prototype/src/frontend/lowering/constraint/`;
- `src/prototype/src/frontend/lowering/constraint_solver.inc`;
- classifier, effect-row, computation, motive, and usage solver modules; and
- TypedOccurrence final classifier/effect/usage projections.

### 8.2 Known audit leads

Current code still exposes all of the following:

- `operation_constraint_db` with lifecycle, reason, evidence, result, and
  domain-specific payloads;
- `operation_solver_solution[]` with classifier, effect-row, usage, state, and
  reason fields;
- `operation_effect_solver.metas[]` with effect solution state;
- invocation-local computation result tables; and
- final TypedOccurrence projections checked against solver results.

Their coexistence is not automatically wrong. It becomes wrong if two mutable
records answer the same final semantic question.

### 8.3 Questions

- [x] Map classifier generation, worklist scheduling, solving, and final
      projection as separate phases.
- [x] Determine whether `ConstraintDB` or solution cells own final classifier,
      effect, usage, and computation results.
- [x] Classify every solution-cell field as authority or disposable workspace.
- [x] Trace every copy from ConstraintDB to solution cells and back.
- [x] Identify any acceptance path that reads a copied result without checking
      its source constraint.
- [x] Determine whether effect metavariables own solutions while effect
      equation constraints own only lifecycle, or whether both own results.
- [x] Check whether motive candidate tables and branch-refinement payloads are
      genuine search state or alternative classifier authority.
- [x] Audit computation constraints across JudgementDelta payload,
      invocation-local results, ConstraintDB, and VerificationDB transfer.
- [x] Confirm that TypedOccurrenceGraph is a checked final projection, not a
      solver authority.
- [x] Measure repeated scans, index rebuilds, and full-array solution copying.
- [x] Evaluate whether domain solvers can share one lifecycle API while keeping
      distinct typed equation payloads and algorithms.

### 8.4 Deliverable and exit gate

- [x] One field-level authority table exists for classifier, effect, usage,
      motive, branch refinement, and computation equations.
- [x] Every copy has a one-way transition and disagreement check.
- [x] The final report can state exactly which mutable fields should be removed,
      retained as workspace, or retained as authority.
- [x] Performance consequences of each consolidation are measured or bounded.

## 9. AU3: Context, Substitution, Binding, and Resource Usage

### 9.1 Scope

- `src/prototype/include/a_program/kernel/context.h`;
- `src/prototype/src/kernel/context.c`;
- Context compaction, comprehension, and reindex modules;
- Context-related Judgement rules; and
- occurrence and proposition resource-usage records.

### 9.2 Questions

- [x] Verify that Context objects and substitutions remain distinct CwF
      objects and morphisms.
- [x] Trace `CONTEXT_WEAKEN` evidence to the exact projection substitution that
      gives it meaning.
- [x] Determine whether weakening and projection duplicate authority or are a
      proof of a morphism and the morphism itself.
- [x] Audit persistent `BindingId` identity across Lambda, Match binders,
      Context extension, proof Context, and reindex operations.
- [x] Find remaining name-, slot-, or positional remaps that should instead
      reference stable binding identity.
- [x] Classify pullback, composition, reindex, and operation-local lookup tables
      as semantic substitutions or rebuildable caches.
- [x] Verify transaction/rollback coverage for every Context and substitution
      index.
- [x] Determine whether classifier references stored on Context bindings are
      immutable schema references or copied solver state.
- [x] Compare solver usage state, occurrence usage, Context resource entries,
      and proposition resource usage.
- [x] Establish a future resource-sensitive invariant without prematurely
      merging usage evidence into Context identity.

### 9.3 Required non-unifications

- ContextDB and SubstitutionDB remain separate.
- A weakening proof is not replaced by a bare projection ID; the proof may
  reference and validate that projection.
- Binding identity must not be replaced with De Bruijn indices.
- Resource usage is not part of context-free Term identity.

### 9.4 Deliverable and exit gate

- [x] Every Context/substitution cache has one authoritative source and an
      invalidation rule.
- [x] Weakening, projection, reindex, and substitution composition have a
      single semantic dependency graph.
- [x] Resource usage ownership is explicit at generation, solution, accepted
      judgement, and artifact boundaries.

## 10. AU4: Type Declaration Boundaries

### 10.1 Scope

- `src/prototype/include/a_program/kernel/type_declaration.h`;
- `src/prototype/src/kernel/type_declaration.c`;
- constructor classifier construction;
- type expression and readback APIs;
- artifact type publication and relocation; and
- representation and classifier caches.

### 10.2 Questions

- [x] Confirm graph-level `classifier_family` as constructor semantic authority.
- [x] Find semantic decisions still made from `field_types`, `result_type`, or
      readback metadata.
- [x] Separate nominal/indexed schema, structural representation identity,
      readback data, classifier cache, and statistics at the API capability
      level.
- [x] Determine whether `first_parameter` and similar offsets are semantic or
      presentation data.
- [x] Inventory functions that accept the broad mutable
      `prototype_type_declaration_db` when a narrow read-only view is enough.
- [x] Verify that dependent constructor telescopes preserve earlier field
      binders and are not reconstructed from flat metadata.
- [x] Check whether representation keys derive from the authoritative
      classifier family or from lossy metadata.
- [x] Distinguish persistent structural representation identity from a
      rebuildable classifier cache.
- [x] Measure duplicate schema traversal in typing, readback, artifact, and
      function-graph generation.

### 10.3 Required non-unifications

- Constructor telescopes and Pi types must not be collapsed. A telescope is
  schema/context structure; Pi is a callable type former.
- Nominal TypeView identity must not be collapsed into structural core shape.
- Structural representation identity must not be dismissed as a cache.
- Readback metadata must not become kernel classifier authority.

### 10.4 Deliverable and exit gate

- [x] A capability diagram names every read-only and mutable API boundary.
- [x] Every stored field is assigned to schema, representation, readback,
      cache, or statistics.
- [x] Any proposed struct split includes initialization, transaction, artifact,
      and performance consequences.

## 11. AU5: Proposition, Claim, Derivation, and Replay

### 11.1 Scope

- `src/prototype/include/a_program/kernel/judgement/`;
- `src/prototype/src/kernel/judgement.c` and
  `src/prototype/src/kernel/typing/judgement_db.inc`;
- `src/prototype/src/kernel/typing/accepted_replay.inc`;
- candidate proof construction; and
- accepted artifact replay.

### 11.2 Questions

- [x] Confirm distinct authority for Proposition identity, Claim acceptance,
      and Derivation provenance.
- [x] Audit whether accepted replay still reconstructs mutable
      candidate-premise arrays from accepted derivations.
- [x] Determine whether immutable premise and rule-application views cover all
      replay paths.
- [x] Find const removal, accepted-to-candidate copying, or mutable temporary
      records that can affect replay meaning.
- [x] Separate rule-generic storage/validation plumbing from theorem-specific
      APP, MATCH, induction-hypothesis, computation-fold, identity, and
      totality checks.
- [x] Verify that a Term ID alone never identifies a typing proof; the
      occurrence/operation-level proposition must be preserved.
- [x] Check derivation DAG interning, premise order, multiple derivations for one
      proposition, and accepted root ownership.
- [x] Measure stack reconstruction and repeated proposition lookup costs.

### 11.3 Required non-unifications

- Proposition, Claim, and Derivation remain separate.
- Multiple valid derivations may prove one proposition; they must not overwrite
  each other merely to maintain one proof slot.
- APP_ELIM, MATCH_ELIM, INDUCTION_HYPOTHESIS_ELIM, and
  COMPUTATION_FOLD_ELIM must remain explicit kernel rules even if their C
  plumbing is shared.

### 11.4 Deliverable and exit gate

- [x] Every accepted proof read path uses an immutable accepted view.
- [x] Candidate construction and accepted replay have a precise shared boundary.
- [x] Proposed helper extraction cannot bypass rule-specific validation.

## 12. AU6: Evaluation, Conversion, and Reduction Profiles

### 12.1 Scope

- `src/prototype/src/core/term/evaluation_and_conversion.inc`;
- kernel conversion and classifier WHNF code;
- runtime evaluation;
- pure intrinsic evaluation; and
- normalization caches and reduction profiles.

### 12.2 Questions

- [x] Enumerate beta, iota, force/return, fold, pure intrinsic, effect request,
      and function-graph reduction rules.
- [x] Identify the unique implementation of each reduction step and every
      wrapper that duplicates it.
- [x] Distinguish Lambda-only WHNF, inductive WHNF, pure compile-time
      normalization, and runtime execution as reduction profiles over common
      Terms.
- [x] Verify that classifier conversion does not own a private Match reduction
      rule inconsistent with the normalizer.
- [x] Audit memoization keys for profile, Context/substitution, effect policy,
      budget, and database epoch.
- [x] Determine whether cache entries are reusable across type and value
      calculations when their reduction profile is identical.
- [x] Verify that graph growth caused by evaluation does not cause unnamed
      temporary Terms to become artifact roots.
- [x] Separate DefEq from object Identity and optimizer rewrite evidence.
- [x] Measure repeated WHNF/normalization calls in IF8 and totality formation.

### 12.3 Required non-unifications

- Compile-time and runtime phases may share reduction machinery but must retain
  different allowed profiles and effect authority.
- DefEq, object Identity, and optimization equivalence remain distinct.
- An effect request is not reducible by the pure normalizer merely because a
  host implementation exists.

### 12.4 Deliverable and exit gate

- [x] A reduction-rule ownership table exists.
- [x] Every profile difference is explicit data, not a forked evaluator.
- [x] Every memoized result has a complete validity key and invalidation rule.

## 13. AU7: CBPV, Effects, Folds, Residuals, and Host Operations

### 13.1 Scope

- RETURN, THUNK, FORCE, operation-request, and computation-fold Terms;
- effect rows and operation declarations;
- intrinsic names and host evaluators;
- VerificationDB;
- computation blocks and surface lowering; and
- artifact conditional evidence.

### 13.2 Questions

- [x] Verify that the Intrinsic namespace is only a naming domain and does not
      force every intrinsic to share one semantic kind.
- [x] Classify pure primitives, machine-dependent pure operations, effect
      operations, types, and Prelude-like names separately.
- [x] Confirm operation-request as the explicit free-algebra node required by
      handlers, independent of whether surface `perform` syntax remains.
- [x] Audit computation-fold as the shared semantic basis for sequencing and
      handlers while retaining clause-specific typing.
- [x] Check multi-clause and return-clause representation across surface AST,
      Term, typing, runtime, and artifact.
- [x] Trace effect equation ownership from generation through solution,
      residualization, publication, import, link, and discharge.
- [x] Confirm VerificationDB contains phase residuals, not accepted Claims or a
      second solver.
- [x] Audit `compile_metadata.effect_constraints[]` as diagnostics only; no
      acceptance or obligation generation may depend on the snapshot.
- [x] Distinguish higher-order operation payloads expressed with THUNK from the
      separate semantics of scoped higher-order handling.
- [x] Verify that implicit surface coercions lower to explicit CBPV Terms and do
      not create a second dynamic polarity authority.

### 13.3 Required non-unifications

- Pure primitive evaluation and effect handling remain distinct.
- Operation requests must not be lowered away into ordinary APP or BIND.
- A THUNK payload alone does not define recursive handling scope.
- Constraint residuals, runtime requests, and accepted Claims remain distinct.

### 13.4 Deliverable and exit gate

- [x] Every effect fact has one owner at each explicit phase transition.
- [x] Surface sugar has a unique raw-CBPV lowering.
- [x] The report identifies exactly which folds can share implementation and
      which typing rules remain explicit.

## 14. AU8: Universe Authority

### 14.1 Scope

- Universe Terms and variables;
- UniverseDB nodes, edges, levels, constraints, and solved state;
- Pi and type-formation rules;
- conversion and cumulativity; and
- artifact persistence.

### 14.2 Questions

- [x] Separate universe variable identity, level expressions, inequalities,
      solver lifecycle, and accepted evidence.
- [x] Check whether levels, edges, and constraints duplicate solutions or form
      one graph with derived indexes.
- [x] Verify that DefEq compares appropriate level identity and does not treat
      all universe variables as equal.
- [x] Verify that cumulativity reads solved inequality evidence rather than
      contaminating DefEq.
- [x] Verify Pi formation records the intended max constraints or the current A
      Program alternative explicitly.
- [x] Audit wire/readback copies and rebuildability.

### 14.3 Deliverable and exit gate

- [x] Universe formation, conversion, cumulativity, and solving have separate
      authorities and explicit interfaces.
- [x] Any current inconsistency is demonstrated by a focused accepted/rejected
      example before an implementation plan is proposed.

## 15. AU9: Identity, Parametricity, Dimensions, and HOTT

### 15.1 Scope

- `src/prototype/src/identity/`;
- `src/prototype/src/parametricity/`;
- dimension contexts, substitutions, actions, and artifact roots;
- object Identity Terms and witnesses; and
- HOTT action caches and generated evidence.

### 15.2 Questions

- [x] Keep logical-relation/parametricity action distinct from object Identity.
- [x] Identify shared endpoint/context traversal that can be implemented once.
- [x] Determine whether dimension operators, actions, bridges, and telescope
      actions duplicate semantic identity or represent different certificates.
- [x] Audit one-dimensional, two-dimensional, and higher action caches for
      authority, canonical roots, and invalidation.
- [x] Verify that object Identity inhabitance is not inferred merely from
      parametricity preservation.
- [x] Trace canonical action roots through artifact v84 publication and replay.
- [x] Audit duplicate constructor/Lambda/APP/Match action implementations while
      preserving their separate proof rules.
- [x] Record unsupported higher coherence explicitly rather than representing
      absence of implementation as an empty Identity type.

### 15.3 Required non-unifications

- Logical relation and Identity are not aliases.
- DefEq and object Identity are not aliases.
- Higher-dimensional evidence must not be flattened into proof irrelevance.
- Similar graph traversals do not imply identical introduction rules.

### 15.4 Deliverable and exit gate

- [x] A table separates relation generation, Identity formation, witness
      construction, action preservation, and artifact rooting.
- [x] Shared action infrastructure is proposed only where certificate meaning
      remains explicit.

## 16. AU10: Generated Function Graphs, Totality, and Indexed Families

### 16.1 Scope

- function-graph generation and validation;
- generated indexed declarations, runners, interfaces, and adapters;
- Acc and well-founded recursion;
- totality/termination witnesses; and
- generated artifact package records.

### 16.2 Questions

- [x] Distinguish the source function, generated graph relation, totality
      witness, runner, adapter, and package association.
- [x] Verify that association metadata certifies a relation and does not assert
      extensional equality without proof.
- [x] Find repeated generated schema construction across first-order and
      higher-order modes.
- [x] Compare semantic validation in graph construction, type checking,
      artifact readback, and interface linking.
- [x] Audit totality predicates and generated IADT evidence for duplicate
      reachability or termination authority.
- [x] Measure repeated Context resolution, substitution indexing, normalization,
      and declaration specialization in QuickSort.
- [x] Keep future QuickSort property proofs outside this audit unless their
      current scaffolding creates duplicate authority.

### 16.3 Deliverable and exit gate

- [x] Every generated object names its source fact and theorem.
- [x] Generation, validation, certification, and execution are not represented
      as interchangeable records.
- [x] Common schema-building helpers preserve mode-specific contracts.

## 17. AU11: Artifact and Persistence Boundaries

### 17.1 Scope

- artifact writer and reader;
- v84 schema and wire structs;
- interface publication;
- dependency collection;
- relocation and linking;
- accepted replay; and
- artifact roots.

### 17.2 Questions

- [x] For every serialized table, name the in-memory authority and prove that
      the wire record is immutable evidence, not a second mutable owner.
- [x] Distinguish parser bounds checking, semantic interface validation,
      relocation, and accepted proof replay.
- [x] Identify repeated semantic validation across these stages and determine
      whether it is defense in depth or conflicting authority.
- [x] Verify sparse-slot presence checks for every referenced ID.
- [x] Confirm caches, search queues, fuel, diagnostics, and solver snapshots are
      not serialized as meaning.
- [x] Audit exact conditional dependencies for residual obligations through
      publication, import, linking, and discharge.
- [x] Check version constants and documentation for v84 consistency.
- [x] Find compatibility readers, remap tables, or legacy fallback
      reconstruction that should not survive a meaning change.

### 17.3 Required non-unifications

- Wire parsing and semantic validation remain separate.
- Relocation is not semantic normalization.
- Accepted replay is not candidate proof search.
- Dense publication IDs and internal sparse IDs may differ, but the mapping must
  be explicit and one-way.

### 17.4 Deliverable and exit gate

- [x] Every artifact field has an authority/projection classification.
- [x] Every repeated validator has a stated layer-specific contract.
- [x] No readback fallback silently reconstructs authoritative dependent schema
      from diagnostic metadata.

## 18. AU12: Names, TypeViews, and Labels

### 18.1 Scope

- surface and qualified name resolution;
- external references and import/export IDs;
- constructor namespaces;
- TypeView and structural representation keys;
- labels, readback names, and diagnostics; and
- top-level assignment names.

### 18.2 Questions

- [x] Separate qualified operation identity from display labels.
- [x] Verify that unresolved external references retain enough namespace/import
      identity to avoid collisions.
- [x] Confirm that structurally identical Bool and Two core graphs can retain
      distinct nominal TypeViews and constructor meanings.
- [x] Determine whether alias resolution, export identity, and linker identity
      are implemented from one qualified-name abstraction.
- [x] Find symbol IDs that are compared as semantic identity after their
      namespace has been discarded.
- [x] Verify that labels and readback names cannot affect DefEq, interning, or
      accepted proof identity.

### 18.3 Deliverable and exit gate

- [x] Every name-bearing ID is classified as semantic qualified identity,
      nominal TypeView identity, binding identity, or presentation label.
- [x] No flattening/remap workaround is proposed where a stable qualified ID is
      the correct representation.

## 19. AU13: Storage, Indexes, Transactions, and Common C Machinery

### 19.1 Scope

- reserve/append/intern helpers;
- hash and index rebuilds;
- transaction marks and rollback;
- driver/program/provider/reader/publication storage bundles;
- static workspaces; and
- large include-file modules.

### 19.2 Questions

- [x] Inventory repeated reserve, append, intern, hash, rebuild, and rollback
      implementations.
- [x] Determine which repetitions share identical ownership and failure
      semantics.
- [x] Verify every derived index rolls back or rebuilds with its authoritative
      graph.
- [x] Measure Context and Substitution index rebuild count and cost.
- [x] Identify broad mutable database pointers that can become narrow capability
      views without adding adapters that preserve duplicate APIs.
- [x] Evaluate typed storage helpers, not an untyped mega-arena.
- [x] Audit repeated initialization and cleanup of compiler storage bundles.
- [x] Find large physical modules that can be split by responsibility without
      splitting authority.
- [x] Record per-file LOC before and after any later implementation phase.

### 19.3 Current size leads

The audit must inspect, but not condemn by size alone, these large modules:

- `graph_construction.inc`: approximately 11,153 lines;
- `function_graph.c`: approximately 8,321 lines;
- `accepted_replay.inc`: approximately 8,020 lines;
- `identity/object_term_action.inc`: approximately 6,156 lines;
- `context_and_type_lowering.inc`: approximately 5,803 lines; and
- `read_file.c`: approximately 5,472 lines.

### 19.4 Deliverable and exit gate

- [x] Physical helper candidates include exact call sites and error semantics.
- [x] No proposed helper hides semantic validation behind an untyped callback.
- [x] Every later implementation plan includes per-file added, deleted, and net
      LOC reporting.

## 20. AU14: Tests, Diagnostics, Counters, and Documentation

### 20.1 Scope

- shell and C test harnesses;
- artifact mutation fixtures;
- compile timing and counter output;
- diagnostic snapshots;
- README and design documents; and
- obsolete plans and version references.

### 20.2 Questions

- [x] Identify tests that duplicate setup and parsing machinery but need
      independent assertions.
- [x] Separate semantic regression tests, boundary mutation tests, performance
      benchmarks, and source-text audits.
- [x] Verify all tests report elapsed time under one policy.
- [x] Ensure counters observe algorithms without becoming acceptance inputs.
- [x] Confirm diagnostic snapshots cannot feed solver, publication, or replay
      decisions.
- [x] Find stale artifact-version and architecture claims.
- [x] Determine which old plans should be marked superseded rather than edited
      into false historical accuracy.
- [x] Add a future regression-test requirement to every confirmed authority
      correction.

### 20.3 Deliverable and exit gate

- [x] Each test names the invariant and authority boundary it protects.
- [x] Shared fixture helpers do not cause writer and reader tests to share the
      same semantic bug.
- [x] Documentation distinguishes current contract, historical decision, and
      pending proposal.

## 21. AU15: Integrated Synthesis

AU15 begins only after AU0-AU14 are complete. It combines findings; it does not
discover new facts by intuition.

### 21.1 Required final classifications

Every finding must end in one of these responses:

1. remove a duplicate mutable authority;
2. make a derived projection immutable and checked;
3. narrow a capability/API boundary;
4. share physical storage or traversal machinery;
5. preserve a legitimate semantic distinction and document it;
6. add a missing phase-transition certificate;
7. delete obsolete compatibility or remap behavior;
8. add a focused regression or performance test; or
9. reject the original suspicion as unsupported.

### 21.2 Prioritization

| Priority | Meaning |
| --- | --- |
| P0 | Two mutable authorities can disagree and alter accepted meaning |
| P1 | Persistence, replay, or phase transition can lose or invent evidence |
| P2 | Broad capabilities or repeated algorithms make future semantic changes unsafe |
| P3 | Repeated physical code, diagnostics, or documentation increase maintenance cost |

Performance is not a lower-priority afterthought. A correction that centralizes
authority but introduces repeated whole-graph scans must be redesigned before
implementation.

### 21.3 Implementation-plan output

The synthesis must produce separate, reviewable implementation plans. Each plan
must include:

- exact fields and APIs to remove, retain, or introduce;
- old and new authority diagrams;
- migration order without compatibility paths;
- semantic and performance invariants;
- focused tests and full-suite gates;
- artifact version impact;
- before/after benchmark commands;
- before/after per-file LOC; and
- related GitHub Issue disposition.

Plans should be divided by semantic boundary, not by whichever files are easiest
to edit together.

### 21.4 Final exit gate

- [x] AU0-AU14 are complete with code references.
- [x] Every suspected duplicate has a classification and evidence.
- [x] The negative decision ledger is complete.
- [x] P0/P1 findings have focused reproductions or disagreement tests.
- [x] Performance baselines exist for affected paths.
- [x] Implementation plans preserve all required non-unifications.
- [x] No implementation change was mixed into the audit commits.

## 22. Initial Negative Decisions

These decisions are provisional audit guardrails. They may change only with a
concrete counterexample and an explicit update to this document.

| Concepts | Decision |
| --- | --- |
| Term and typed occurrence | Preserve separately; computation identity and occurrence evidence differ |
| Context and substitution | Preserve separately; objects and morphisms differ |
| Proposition, Claim, and Derivation | Preserve separately; statement, acceptance, and provenance differ |
| Constraint and accepted Claim | Preserve separately; equation search is not proof acceptance |
| Constraint and Verification Obligation | Preserve separately with an explicit phase transfer |
| Pi and constructor telescope | Preserve separately; callable type and schema Context differ |
| DefEq and object Identity | Preserve separately; kernel conversion and object evidence differ |
| Parametricity relation and Identity | Preserve separately; relation preservation does not itself establish equality |
| Pure primitive and effect operation | Preserve separately even when both are under `#.` |
| OP_REQUEST and APP | Preserve separately; operation identity is required by the free algebra |
| APP/MATCH/IH/FOLD proof rules | Preserve explicit rules; share only validated plumbing |
| TypeView and structural shape | Preserve nominal operation identity above shared core structure |
| Wire parser and semantic validator | Preserve layered validation responsibilities |
| Compile-time and runtime reduction | Share rules through explicit profiles; do not fork Term semantics |

## 23. Audit Log Template

Each AU section must append entries in this form:

```text
Finding ID:
Date and commit:
Files and lines:
Semantic question:
Candidate records/APIs:
Writers:
Readers that affect acceptance:
Serialization/publication:
Reconstruction/invalidation:
Disagreement behavior:
Performance evidence:
Classification:
Recommended response:
Required regression test:
Open uncertainty:
```

This template is intentionally stricter than a code-review finding. The audit
must explain not only that two structures look duplicated, but which one owns
meaning and why the other structure exists.
