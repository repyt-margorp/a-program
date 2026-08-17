# Post-DA Semantic Authority Residual Consolidation Plan

Date: 2026-08-16

Last re-audited: 2026-08-17

Status: planned; Issue 12 gate satisfied; RC0 ready

Repository baseline:

- branch: `main`;
- committed baseline: `0bb1c99`;
- committed artifact format: v77;
- implementation root: `src/prototype/`; and
- Issue 12 is completed and pushed independently. RC0 starts from that clean
  semantic baseline and must not restore any deleted compatibility path.

Related documents:

- `2026-08-16T14-43-28-SEMANTIC-STORE-AUTHORITY-AND-PHYSICAL-CONSOLIDATION-PLAN.md`;
- `2026-08-16T02-36-41-CORE-TERM-TYPED-OCCURRENCE-SEPARATION-PLAN.md`;
- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`;
- `2026-08-11T08-19-24-CURRENT-COMPILER-FUNCTIONAL-AND-THEORY-REVIEW.md`; and
- `src/prototype/spec/artifact_v77.schema`.

## 1. Purpose

The previous DA0-DA6 migration established the intended semantic stores and
removed several large dual-authority paths. The current code nevertheless
retains transitional representations that make the completed ownership model
less strict than its documentation states.

This follow-up has three objectives:

1. make `ConstraintDB` the actual, not merely documented, mutable authority for
   classifier and effect solving;
2. complete the API and physical ownership split between type schema,
   structural representation, readback, and caches; and
3. retain the distinct meanings of Proposition, Claim, and Derivation while
   eliminating candidate-shaped copying and repeated proof-management plumbing.

The objective is not to minimize the number of structs. The objective is to
ensure that one semantic fact has one mutable owner and that duplicated C
machinery is shared only where doing so does not hide a kernel rule.

### 1.1 Re-audit conclusion

The 2026-08-17 source audit confirms that all three reported residuals still
exist after DA0-DA6:

1. `operation_constraint_db` is documented as classifier/effect authority,
   while `operation_solver_solution` and `operation_effect_row_meta` still
   retain overlapping mutable result and lifecycle fields;
2. schema, readback, representation, and classifier cache records are now
   logically distinguished, but remain reachable through the broad
   `prototype_type_declaration_db` capability and semantic declarations still
   contain the readback offset `first_parameter`; and
3. Proposition, Claim, and Derivation are correctly distinct, but accepted
   replay still reconstructs `prototype_judgement_candidate_premise` arrays
   and casts immutable Propositions into a candidate-shaped mutable view.

These are implementation-boundary defects, not grounds for collapsing every
database into one arena. The planned correction is respectively single-owner
constraint state, capability-separated type APIs, and immutable common proof
views.

### 1.2 Reported issue to implementation phase mapping

| Reported issue | Audit decision | Required correction | Phase |
| --- | --- | --- | --- |
| Classifier/effect authority remains in multiple records | Valid duplicate mutable authority | Make each `ConstraintDB` record own lifecycle, result, reason, and evidence; retain solver cells only as disposable algorithm workspace | RC1-RC2 |
| `TypeDeclarationDB` mixes semantic schema, readback, representation, and cache access | Valid API/capability defect; not a reason to erase the semantic DB | Split initializers and API capabilities, move readback offsets out of semantic records, and expose an explicit composed read-only view only where needed | RC3 |
| Proposition/Claim/Derivation management is large | The three meanings are not duplicates; candidate-shaped accepted replay is duplicate plumbing | Preserve all three persistent objects and replace copied mutable premise records with one immutable rule-application view | RC4 |
| Many DBs repeat append/intern/rollback mechanics | Valid physical duplication only after ownership is fixed | Share typed storage helpers without introducing an untyped arena or hiding rule-specific validators | RC5 |
| Derived lookup indexes can outlive graph rollback | Valid transaction-consistency defect; not a new semantic store | Rebuild or roll back every derived index from the same authoritative transaction mark as its source graph | RC0, RC5 |

### 1.3 Execution gate

This plan follows the Issue 12 dependent-Match migration. Its execution gate is
now satisfied:

- [x] Issue 12 was completed, verified, committed, and pushed as `0bb1c99`.
- [x] Artifact v77 is the clean baseline.
- [x] ConstraintDB, Context compaction, and accepted replay were re-audited
      after the Issue 12 changes.
- [x] No Issue 12 compatibility path is retained to reduce RC1-RC4 migration.

The gate separates commits and measurements. RC0 may now begin, but it still
must inventory fields before changing behavior.

### 1.4 Adopted consolidation boundary

This plan is the implementation and progress plan for the three residuals
reported after the Core Term/TypedOccurrence separation. The requested
"unification" is interpreted as eliminating duplicate authority and duplicate
physical plumbing, not as reducing every semantic concept to one record type.

| Reported area | Consolidate | Preserve separately |
| --- | --- | --- |
| Classifier and effect constraints | lifecycle, result, reason, and evidence ownership in `ConstraintDB`; checked transition/query APIs | typed classifier/effect payloads and their domain-specific solving algorithms |
| Type declarations | capability boundaries, initializers, and explicit cross-store views | nominal schema, structural representation identity, readback data, and rebuildable classifier caches |
| Proposition/Claim/Derivation | immutable premise/rule-application views and storage mechanics | statement identity, acceptance identity, and proof provenance |

The first implementation change after the Issue 12 gate is RC0, not a broad
struct merge. RC0 must produce a field-level owner inventory from the then
current code. RC1-RC4 may start only from that inventory, so fields introduced
by the v77 dependent-Match work are not accidentally copied into another
authority or removed as apparent duplication.

## 2. Decisions

### 2.1 What must remain distinct

The following are not duplicate representations and must not be merged:

```text
TermDB
    context-free computational terms

TypedOccurrenceGraph
    occurrences of those terms under a Context and source operation

ContextDB
    Context objects

SubstitutionDB
    Context morphisms

TypeDeclarationDB
    nominal and indexed constructor schema

JudgementDB
    accepted proof objects

ConstraintDB
    compiler-local unsolved and solved equations
```

Likewise, these proof objects remain distinct:

```text
Proposition  = a judgement statement
Claim        = acceptance of one Proposition
Derivation   = one rule application proving a Claim
```

Merging the three would erase the possibility that one Claim has multiple
Derivations and would confuse a statement with its acceptance and provenance.

Classifier, effect-row, Universe, branch-refinement, and future resource
constraints may share lifecycle storage, but their payloads and solvers remain
typed. No untyped mega-solver is introduced.

### 2.2 What is genuine duplicate authority

The current constraint implementation stores overlapping mutable facts in:

- `operation_constraint_db.constraints`;
- `operation_classifier_solver.solutions`;
- `operation_effect_solver.metas`;
- `prototype_compile_metadata.effect_constraints`; and
- invocation-local Judgement computation/effect outputs.

Some of these are valid algorithm workspaces or immutable projections. The
problem is that the C API does not enforce that distinction, and ordinary
compiler code directly reads and writes copied classifier, effect-row, state,
and reason fields.

The correction is:

```text
ConstraintDB record
    owns equation, lifecycle, solution, reason, and evidence

solver workspace
    owns only temporary algorithm state

occurrence solution index
    refers to Constraint IDs; it does not copy their results or states

TypedOccurrenceGraph
    receives one checked final projection after solving

compile diagnostics
    receive immutable rendered/snapshot data only when needed

artifact
    stores accepted evidence or an explicit residual obligation, never the
    mutable solver state
```

### 2.3 What is incomplete separation rather than wrong theory

`prototype_type_declaration_db` currently embeds:

- declaration and constructor schema arrays;
- `prototype_type_readback_db`;
- `prototype_type_representation_db`; and
- `prototype_constructor_classifier_cache`.

The nested records document separate authority, so this is no longer the old
semantic-schema reconstruction defect. However, the single initializer and the
large `prototype_type_declaration_db*` API still allow unrelated consumers to
reach readback and cache state. `first_parameter` also remains a readback index
inside the semantic declaration record.

The next correction is capability separation at the API boundary, not another
theory-layer split.

### 2.4 What should be consolidated in Judgement management

The existing `prototype_judgement_rule_application_view` is the correct
direction. The remaining problem is that its premise array still uses
`prototype_judgement_candidate_premise`, including a mutable Proposition
pointer. Accepted replay therefore constructs candidate-shaped premise arrays
and uses casts from `const` Proposition pointers.

The target is a genuinely immutable validator view:

```text
prototype_judgement_rule_application_view
    conclusion: const Proposition*
    premises: const prototype_judgement_premise_view*

prototype_judgement_premise_view
    proposition: const Proposition*
    semantic action reference
    optional source reference for diagnostics
```

Candidate and accepted adapters resolve IDs into this view. They do not copy a
Proposition and do not reuse a mutable candidate record as the common format.

### 2.5 CBPV graph primacy does not make every semantic store derivable

The CBPV-shaped TermDB remains the primary representation of computation. A
typed occurrence adds Context and source-operation authority to a shared Core
term; it must not introduce a second APP, Lambda, Match, or ComputationFold
semantics.

That principle does not imply that every other database is a cache derivable
from TermDB. In particular:

- a Core term deliberately erases the Context in which one occurrence is
  checked;
- a Context morphism records substitution, weakening, and later resource use
  that cannot be recovered from an erased term alone;
- nominal type declaration identity is not determined by structural Core
  shape; and
- a Derivation records why a Claim was accepted, not merely the proposition
  that happens to be true.

The target therefore has one computational graph vocabulary and several typed
semantic stores, rather than one universal arena:

```text
TermDB
    shared CBPV computation structure

TypedOccurrenceGraph
    Context- and source-authority-bearing occurrence of a Term

ConstraintDB
    transient elaboration equations over occurrences

JudgementDB
    persistent accepted propositions and proof DAGs
```

The consolidation rule is field-level: if two records can independently
change the answer to the same semantic question, one owner must be removed. If
two records answer different questions, their storage mechanics may be shared
but their typed identities remain separate.

### 2.6 No structure-count reduction target

The migration must not judge success by reducing the number of structs or
databases. It must instead reduce:

- duplicate mutable authorities;
- broad capabilities that expose unrelated stores;
- copied candidate/accepted proof views;
- repeated append, bounds-check, rollback, and traversal mechanics; and
- compatibility aliases retained only to reduce call-site migration.

Every retained store must have a one-sentence semantic ownership statement and
an API that prevents callers from writing facts owned elsewhere. The final
metrics report records line changes, but a net line reduction is not an exit
condition.

## 3. Current Concrete Findings

### 3.1 Constraint result cells still compete with ConstraintDB

`src/prototype/src/frontend/lowering/context_and_type_lowering.inc` defines
`operation_solver_solution` with:

- classifier and effect-row results;
- classifier/effect/usage lifecycle states;
- a shared reason field; and
- usage projection fields.

The same module defines `operation_constraint`, which also has lifecycle state,
reason, result Term, evidence, origin, and typed payload. The solver directly
reads `classifier_solver.solutions[...]` throughout
`constraint_solver.inc`. Thus the occurrence solution array remains a second
mutable result store rather than only an index into ConstraintDB.

### 3.2 Effect snapshots remain embedded in compile metadata

`prototype_compile_metadata` owns an array of
`prototype_occurrence_effect_constraint`. Its comment calls it an immutable
diagnostic snapshot, but `operation_solver_project_effect_constraints` rebuilds
it from ConstraintDB and later code reads it to create residual obligations.

This can be correct only if the snapshot is frozen once and never participates
in solving or acceptance. The current phase APIs do not make that timing
obvious. Residual publication should read the authoritative constraint or a
dedicated residual record, not a general diagnostic copy.

### 3.3 Effect meta cells mix algorithm state and semantic results

`operation_effect_solver.metas` stores a placeholder row, solution atoms,
solution row, and solved state. Union/equivalence-class work state is a valid
solver implementation detail. A final semantic result and externally observed
lifecycle state are not. The workspace must be private to the effect solver and
discardable without changing the projected program.

### 3.4 Judgement output and ConstraintDB need a transfer boundary

`prototype_judgement_computation_constraint` and
`prototype_judgement_effect_row_constraint` are documented as immutable kernel
input/output payloads. ConstraintDB then refers to or copies those records.

This is not automatically duplicate authority. It becomes duplicate authority
if both records can outlive the transfer and independently determine solving or
publication. The kernel should emit typed payloads through a caller-owned sink,
and the frontend should publish each emitted obligation exactly once into
ConstraintDB.

### 3.5 Type stores are logically split but physically over-capable

`prototype_type_declaration_db_init` receives every schema, readback,
representation, and cache backing array. Type-expression constructors such as
`prototype_type_expr_app` also take the complete declaration DB even though
they operate only on readback expressions.

Semantic APIs should receive a schema view. Readback APIs should receive a
readback DB. Representation queries should receive schema plus a representation
DB only where structural comparison genuinely requires both.

### 3.6 Semantic declarations still contain presentation indexing

`prototype_type_declaration.first_parameter` indexes the readback parameter
array. `parameter_count` is semantically meaningful, but the readback offset is
not. The offset must move into a declaration readback record keyed by
`TypeDeclarationId`.

### 3.7 The common proof view still has candidate representation leakage

Accepted replay fills `prototype_judgement_candidate_premise` arrays to call
shared validators. This introduces:

- candidate store-kind fields in accepted replay;
- mutable Proposition pointers in a read-only path;
- repeated zeroing and pointer-resolution loops; and
- casts away from `const`.

The proof meanings are already correctly separated. Only the adapter and
storage mechanics should be replaced.

### 3.8 Rule validators are not a consolidation target

APP elimination, Match elimination, induction-hypothesis elimination,
ComputationFold elimination, conversion, weakening, and Identity rules certify
different propositions. Similar loops and premise access do not make the rules
semantically identical.

Their premise access may use shared helpers. Their rule equations and explicit
validators remain separate.

### 3.9 Re-audit reference and size inventory

The 2026-08-17 worktree audit found the following lexical reference counts.
These are migration search baselines, not semantic object counts and not line
reduction targets:

| Search target | Current references | Interpretation |
| --- | ---: | --- |
| `classifier_solver.solutions` in frontend lowering | 57 | RC1 must classify every access as authority, temporary workspace, or final projection |
| `effect_solver.metas` in frontend lowering | 4 | RC2 must make all remaining accesses solver-private |
| `effect_constraints` across lowering, driver, and graph metadata | 23 | RC2 must remove solving/publication reads from the diagnostic snapshot path |
| `prototype_type_declaration_db` across prototype headers and sources | 762 | RC3 is an API-capability migration; this count includes legitimate composed-view consumers |
| `prototype_judgement_candidate_premise` in accepted replay | 19 | RC4 must reduce accepted replay usage to zero |
| mutable Proposition casts in accepted replay | 4 | RC4 must reduce these casts to zero |

The immediately affected files currently have these line counts:

| File | Lines |
| --- | ---: |
| `frontend/lowering/context_and_type_lowering.inc` | 4,819 |
| `frontend/lowering/constraint_solver.inc` | 11,936 |
| `kernel/type_declaration.h` | 512 |
| `kernel/type_declaration.c` | 2,588 |
| `kernel/judgement/types.h` | 383 |
| `kernel/typing/candidate_replay.inc` | 1,065 |
| `kernel/typing/accepted_replay.inc` | 7,294 |
| `kernel/typing/judgement_db.inc` | 629 |
| `graph/compile_metadata.h` | 295 |
| `graph/compile_metadata.c` | 268 |

These values were measured from clean post-Issue-12 commit `0bb1c99`. They
establish where duplicate access currently concentrates; they do not turn
lexical counts into line-reduction targets.

### 3.10 Derived indexes must follow source transactions

The Issue 12 audit exposed and fixed a related ownership failure in
`operation_compile_context.binder_owners`. The array is a derived lookup from
current Lambda occurrences and pending binder assumptions. It is now rebuilt
after rollback instead of surviving occurrence-slot reuse as apparent semantic
authority.

This does not justify another persistent binder database. It establishes a
general requirement for RC0 and RC5:

- every derived index names its authoritative source records;
- its transaction mark is advanced and rolled back with those sources, or the
  index is rebuilt before any semantic query;
- deleting and rebuilding the index cannot change acceptance; and
- a derived index is never serialized or used as evidence.

The permanent regression for `binder_owners` belongs to Issue 12. RC0 must audit
the same failure mode in reindex, hash-cons, normalization, representation, and
proof lookup caches before physical storage helpers are consolidated.

## 4. Target Data Flow

### 4.1 Elaboration and constraints

```text
surface AST
    |
    v
TypedOccurrenceGraph construction
    |
    +--> ConstraintDB: classifier equation
    +--> ConstraintDB: branch refinement
    +--> ConstraintDB: computation obligation
    +--> ConstraintDB: effect equation
    +--> later: Universe/resource constraints
                 |
                 v
        typed domain-specific solvers
                 |
                 v
       same ConstraintDB records updated
                 |
                 +--> solved projection -> TypedOccurrenceGraph
                 +--> residual contract -> VerificationDB
                 +--> accepted evidence -> JudgementDB
```

No compile-metadata equation is fed back into the solver. No per-occurrence
array owns a copied lifecycle state.

### 4.2 Type subsystem

```text
prototype_type_schema_db
    nominal declaration identity
    parameter and index Contexts
    constructor field Contexts
    constructor result classifiers

prototype_type_representation_db
    persistent structural representation identities

prototype_type_readback_db
    source type expressions and display metadata

prototype_constructor_classifier_cache
    rebuildable schema materialization cache

prototype_type_system_view
    read-only composition passed only to consumers that genuinely need more
    than one of the stores
```

`prototype_program_storage` may keep the backing arrays adjacent. Physical
adjacency does not grant semantic access.

### 4.3 Judgement validation

```text
candidate IDs --------------------+
                                  |
                                  v
                         immutable rule view --> explicit rule validator
                                  ^
                                  |
accepted Claim/Derivation IDs ----+
```

Publication remains:

```text
PropositionId -> ClaimId <- zero or more DerivationIds
```

## 5. Normative Invariants

### 5.1 Constraint authority

- One semantic elaboration equation has one `ConstraintId`.
- Only its ConstraintDB record owns lifecycle state, result, residual reason,
  and selected evidence.
- An occurrence may index relevant Constraint IDs but may not copy mutable
  states or results before final projection.
- Solver meta cells are inaccessible outside their solver module and may be
  destroyed and reconstructed without changing acceptance.
- Judgement rule emission creates a payload; ConstraintDB owns it after one
  explicit transfer.
- Diagnostics never become an input to solving, replay, or publication.
- VerificationDB owns only residual runtime contracts, not ordinary solved
  equations.

### 5.2 Type authority

- Semantic constructor typing reads only schema Contexts and classifiers.
- Structural representation identity is not nominal TypeView identity.
- Readback data can be deleted without changing compilation acceptance.
- Cache data can be deleted and rebuilt without changing any semantic result.
- No semantic declaration field indexes a readback arena.
- APIs expose only the capability needed by the caller.

### 5.3 Judgement authority

- A Proposition is immutable and interned once per statement identity.
- A Claim refers to one Proposition.
- Any number of valid Derivations may conclude one Claim.
- Rule validators consume immutable views.
- Candidate replay does not mutate accepted records.
- Accepted replay allocates no candidate object and runs no elaboration solver.
- Premise order remains part of Derivation identity where the rule is ordered.
- Rule-specific validation remains explicit.

### 5.4 Derived-index authority

- A derived index is never the only owner of a semantic fact.
- Every index declares the authoritative records from which it is rebuilt.
- Transaction rollback restores the index to the same logical snapshot as its
  source records, either by rollback or deterministic rebuild.
- Semantic acceptance is unchanged after discarding and rebuilding an index.
- Artifact publication excludes compiler-local indexes and cache generations.

## 6. Implementation Phases

### RC0. Freeze the residual authority inventory

Status: pending

- [x] Finish and push Issue 12 independently; use `0bb1c99` as the baseline.
- [ ] Record every read and write of `operation_solver_solution` fields.
- [ ] Record every read and write of effect meta cells and compile-metadata
  effect snapshots.
- [ ] Classify Judgement computation/effect records as producer payload,
  lifecycle state, result, evidence, or diagnostic data.
- [ ] Record every consumer of TypeDeclaration schema, readback,
  representation, and classifier cache fields.
- [ ] Record every accepted replay path that constructs a candidate premise or
  casts away `const`.
- [ ] Inventory derived indexes and caches, including binder-owner, reindex,
  normalization, representation, hash-cons, and proof lookup indexes; record
  each source authority and rollback/rebuild rule.
- [ ] Record baseline per-file line counts and full-suite runtime.

Exit condition:

- every duplicated-looking field has an explicit semantic owner;
- algorithm workspace is distinguished from semantic state;
- no code change has yet altered behavior.

### RC1. Replace copied occurrence solutions with Constraint references

Status: pending

- [ ] Introduce an occurrence-to-constraint index containing typed
  `ConstraintId` references for classifier, effect, branch, computation, and
  later usage constraints.
- [ ] Move classifier result, state, reason, and evidence authority to the
  classifier ConstraintDB payload/header.
- [ ] Replace direct reads of `classifier_solver.solutions[i].classifier` with
  checked ConstraintDB result queries.
- [ ] Replace effect result/state reads with checked effect Constraint queries.
- [ ] Keep usage data separate until it has an explicit resource constraint
  domain; do not force it into a classifier payload.
- [ ] Project solved results to TypedOccurrenceGraph once at the freeze phase.
- [ ] Delete classifier/effect fields and states from
  `operation_solver_solution`.
- [ ] Rename the remaining occurrence index so it cannot be mistaken for the
  solution authority.
- [ ] Remove comments describing the deleted pre-ConstraintDB authority.

Required tests:

- [ ] one classifier constraint used by multiple dependent occurrences;
- [ ] solver rollback leaves no copied solved occurrence state;
- [ ] repeated fixed-point passes cannot reopen a terminal constraint except
  through one checked transition API;
- [ ] frozen typed occurrences equal the authoritative final result; and
- [ ] deleting solver workspace after freeze leaves normalization and replay
  unchanged.

### RC2. Make effect and kernel-obligation transfer single-owner

Status: pending

- [ ] Encapsulate `operation_effect_solver` in the effect solver implementation.
- [ ] Restrict effect meta cells to union/equivalence algorithm work state.
- [ ] Remove externally observable semantic result and lifecycle state from
  effect meta cells.
- [ ] Replace JudgementDelta-to-ConstraintDB copying with a typed obligation
  sink or one move/append operation.
- [ ] Preserve rule-local immutable operands needed by candidate replay without
  retaining a second mutable equation.
- [ ] Make residual-obligation creation read the authoritative ConstraintDB
  record directly.
- [ ] Delete compile-metadata effect snapshots if they serve no remaining
  diagnostic consumer.
- [ ] If snapshots remain, place them in a diagnostics-owned structure, freeze
  them once, and add an assertion that no solver or publisher reads them.
- [ ] Delete dual count/state bookkeeping and redundant copy-back loops.

Required tests:

- [ ] nested APP and ComputationFold share one effect equation authority;
- [ ] residual effect inclusion preserves source occurrence and Context;
- [ ] failed effect solving cannot leave a solved diagnostic snapshot;
- [ ] rebuilding solver metas produces the same result;
- [ ] artifact closure contains residual contracts but no solver meta cells;
  and
- [ ] accepted replay never imports mutable effect solver state.

### RC3. Complete type-subsystem capability separation

Status: pending

- [ ] Rename the semantic owner to `prototype_type_schema_db` if the rename
  reduces ambiguity after call-site audit.
- [ ] Give schema, readback, representation, and cache stores independent
  initializers and validators.
- [ ] Move `first_parameter` from the semantic declaration to a readback record
  keyed by `TypeDeclarationId`.
- [ ] Change `prototype_type_expr_*` APIs to accept only
  `prototype_type_readback_db*`.
- [ ] Change schema registration and constructor typing APIs to accept only the
  schema DB plus ContextDB/TermDB capabilities they require.
- [ ] Change structural representation APIs to accept explicit schema and
  representation views.
- [ ] Keep constructor classifier materialization behind one checked cache API;
  no caller reads cache entries directly.
- [ ] Introduce a read-only `prototype_type_system_view` only for operations
  that genuinely cross these stores.
- [ ] Update program storage initialization without adding forwarding aliases.
- [ ] Delete the old all-capabilities initializer and broad accessors after
  migration.

Required tests:

- [ ] zeroing or omitting readback data leaves semantic validation unchanged;
- [ ] clearing constructor caches leaves classifier results unchanged;
- [ ] `List A`, Sigma-like dependent fields, `Acc`, and generated Identity use
  the same schema API;
- [ ] two nominal declarations may share a representation without becoming
  DefEq as TypeViews;
- [ ] corrupt readback is rejected as readback corruption and cannot affect
  semantic acceptance; and
- [ ] compile-time checks prevent a schema-only module from accessing readback
  fields directly.

### RC4. Replace candidate-shaped replay with an immutable premise view

Status: pending

- [ ] Add `prototype_judgement_premise_view` with only immutable Proposition
  access and semantic-action metadata.
- [ ] Change `prototype_judgement_rule_application_view.premises` to that type.
- [ ] Implement candidate and accepted premise iterators that resolve IDs
  without copying Propositions.
- [ ] Remove `proposition_store_kind` from validator-facing code.
- [ ] Remove all accepted replay casts from `const Proposition*` to mutable
  pointers.
- [ ] Replace repeated fixed-size premise-array initialization with one checked
  stack view builder or iterator.
- [ ] Preserve candidate-local unpublished Proposition storage behind the
  candidate adapter only.
- [ ] Preserve accepted Claim edges and scoped Proposition premise edges.
- [ ] Delete accepted-to-candidate reconstruction helpers after all validators
  consume the immutable view.

Required tests:

- [ ] candidate and accepted adapters produce identical rule views;
- [ ] accepted replay allocates no Proposition, candidate, Claim, or
  Derivation;
- [ ] two Derivations for one Claim remain distinct;
- [ ] scoped local premises replay without inventing Claims;
- [ ] malformed premise IDs fail before dereference; and
- [ ] premise order remains stable through artifact write/read/replay.

### RC5. Consolidate physical helpers without merging theories

Status: pending

- [ ] Reuse existing checked arena, append, mark, rollback, and intern helpers
  where the RC1-RC4 changes expose exact duplicates.
- [ ] Give every retained derived index either a typed transaction mark shared
  with its source store or a deterministic rebuild at the semantic query
  boundary.
- [ ] Keep typed IDs and typed accessors for each store.
- [ ] Remove per-module wrappers that perform only identical bounds checks and
  append bookkeeping.
- [ ] Retain module-specific validation, hashing keys, and artifact closure
  rules.
- [ ] Do not combine APP, Match, induction, ComputationFold, conversion,
  weakening, or Identity validators.
- [ ] Do not introduce `void*` payload casts or a global object-kind namespace.

Exit condition:

- shared code expresses storage mechanics only;
- reading the kernel still reveals every proof rule explicitly; and
- rollback followed by occurrence-slot reuse cannot leave a stale derived
  binding, reindex, cache, or proof lookup entry.

### RC6. Artifact and public boundary audit

Status: pending

- [ ] Determine whether RC1-RC4 alter the persistent wire schema or only
  compiler-local storage.
- [ ] Keep v77 if the serialized semantic closure is unchanged after the
  independent Issue 12 migration is committed.
- [ ] If the wire schema changes, write the next schema first and remove the
  obsolete reader rather than adding a compatibility remap.
- [ ] Verify that readback remains presentation data in publication and link
  validation.
- [ ] Verify that artifact replay reads accepted Claims/Derivations and
  residual obligations only.
- [ ] Verify that no ConstraintDB queue, solver meta, mutable cache, or
  diagnostic snapshot is serialized.
- [ ] Update architecture comments and documentation to match the implemented
  owner, not the transitional structure.

### RC7. Deletion audit, full verification, and metrics

Status: pending

- [ ] Search for obsolete solution-state, effect-snapshot, broad TypeDB, and
  candidate-premise symbols.
- [ ] Remove compatibility aliases, forwarding wrappers, dead branches, and
  stale comments.
- [ ] Run focused constraint, effect, type-schema, Judgement, and artifact
  tests.
- [ ] Run all examples through the supported boundary.
- [ ] Run the complete prototype suite and deterministic artifact replay.
- [ ] Record per-file additions, deletions, and net changes.
- [ ] Record subsystem line totals before and after.
- [ ] Record clean-build and full-suite runtime before and after.
- [ ] Update this dashboard and implementation log incrementally.

## 7. Implementation Order

The required order is:

```text
RC0
 |
 v
RC1 -> RC2
 |      |
 +---+--+
     v
    RC3
     |
     v
    RC4
     |
     v
    RC5
     |
     v
    RC6 -> RC7
```

RC1 precedes RC2 because effect metadata cannot be removed safely while the
occurrence solution array still acts as result authority. RC3 precedes the
final physical consolidation so broad TypeDB APIs are not generalized into a
shared abstraction. RC4 precedes proof storage cleanup because the immutable
view defines which candidate/accepted plumbing is truly common.

### 7.1 File-level migration map

The following map is a planning boundary, not an exhaustive list of incidental
call sites. RC0 must update it before implementation if Issue 12 changes one of
these owners.

| Area | Primary files | Planned change | Required deletion |
| --- | --- | --- | --- |
| Constraint record and occurrence index | `src/prototype/src/frontend/lowering/context_and_type_lowering.inc` | Replace `operation_solver_solution` classifier/effect authority with typed Constraint IDs and solver-only workspace | copied classifier/effect lifecycle, result, and reason fields |
| Constraint transitions and projection | `src/prototype/src/frontend/lowering/constraint_solver.inc` | Route every transition and final query through checked ConstraintDB APIs; freeze one final TypedOccurrence projection | direct `classifier_solver.solutions[...]` semantic reads and effect copy-back loops |
| Diagnostic effect snapshots | `src/prototype/include/a_program/graph/compile_metadata.h`, `src/prototype/src/graph/compile_metadata.c`, frontend lowering, driver diagnostics | Remove the snapshot or move it behind an explicitly frozen diagnostics-only API | any solver, residual publisher, or replay read of `metadata.effect_constraints` |
| Kernel obligation transfer | Judgement rule emitters under `src/prototype/src/kernel/rules/` and frontend imports in `constraint_solver.inc` | Transfer each immutable kernel obligation exactly once into a compiler-owned Constraint record | dual lifecycle/count ownership between Judgement delta output and ConstraintDB |
| Type store declarations | `src/prototype/include/a_program/kernel/type_declaration.h`, `src/prototype/src/kernel/type_declaration.c` | Expose schema, readback, representation, and cache capabilities separately | semantic `first_parameter`; broad readback constructors taking the composed DB |
| Type store ownership | `src/prototype/include/a_program/driver/compiler_session.h`, `src/prototype/src/driver/program_storage.c` | Keep physical backing storage together but initialize and pass typed capabilities independently | all-capabilities initializer as the only entry point |
| Type consumers | frontend lowering, kernel typing, Identity/parametricity, artifact publication, diagnostics | Narrow each parameter to the store or read-only composed view actually required | direct nested-store access from schema-only consumers |
| Common proof view | `src/prototype/include/a_program/kernel/judgement/types.h` | Add immutable `prototype_judgement_premise_view` and make the rule view consume it | validator dependency on `prototype_judgement_candidate_premise` |
| Candidate replay | `src/prototype/src/kernel/typing/candidate_replay.inc` | Adapt candidate-local IDs/pointers to immutable premise views | candidate storage-kind checks in rule validators |
| Accepted replay | `src/prototype/src/kernel/typing/accepted_replay.inc` | Resolve Claim/scoped Proposition edges directly into immutable views | accepted-to-candidate arrays, repeated full-array clearing, and casts away from `const` |
| Persistent proof storage | `src/prototype/src/kernel/typing/judgement_db.inc`, artifact relocation/publication/readback | Preserve Proposition, Claim, Derivation and their typed IDs; share only checked storage mechanics | forwarding helpers that duplicate identical bounds/append behavior |
| Boundary tests | `src/prototype/tests/checks/`, `src/prototype/tests/integration/` | Add authority, cache-erasure, replay-allocation, and solver-disposal checks | tests that validate only struct counts without semantic behavior |
| Derived index transactions | frontend lowering, reindex/cache implementations, focused checks | Prove rollback/rebuild equivalence and occurrence-slot reuse safety | independently advanced cache counts and stale lookup entries |

### 7.2 Transitional state policy

Each phase must migrate one owner in one change set. A temporary adapter may
exist inside that change set only when all of the following hold:

- it is file-local;
- it has no persistent ID or artifact representation;
- old and new stores are never both writable;
- it is removed before the phase is marked complete; and
- a boundary test would fail if the old authority were restored.

Permanent dual writes, fallback reads, compatibility remaps, and deprecated
aliases are prohibited. Migration size is not a reason to retain them.

## 8. Stop Conditions

Stop and revise the plan if any of the following occurs:

- a solver needs two mutable lifecycle records for one semantic equation;
- deleting a diagnostic snapshot changes solver or artifact acceptance;
- deleting readback or cache data changes constructor typing;
- a shared constraint API requires casting one theory payload to another;
- a common proof validator hides a rule-specific premise or computation law;
- accepted replay starts synthesis, normalization search, or constraint solving;
- physical consolidation permits IDs from distinct stores to be confused; or
- migration convenience requires a permanent remap or dual-write path.

## 9. Expected Deletion Areas

The plan does not promise a net reduction before implementation, but the
following areas should lose code:

- direct classifier/effect state access in
  `frontend/lowering/constraint_solver.inc`;
- copied result fields in
  `frontend/lowering/context_and_type_lowering.inc`;
- effect snapshot projection/copy-back and merge plumbing in compile metadata,
  driver, and artifact-adjacent code;
- broad TypeDeclarationDB initialization and forwarding parameters;
- readback offsets stored in semantic declaration records;
- accepted replay premise reconstruction and mutable casts; and
- repeated premise initialization and bounds-check wrappers.

Any net increase must be justified by a new checked invariant or permanent
boundary test, not by migration compatibility.

## 10. Progress Dashboard

| Phase | Status | Result |
| --- | --- | --- |
| RC0 | pending | Residual authority inventory and baseline |
| RC1 | pending | Constraint-referenced occurrence results |
| RC2 | pending | Single-owner effect and obligation transfer |
| RC3 | pending | Capability-separated type subsystem APIs |
| RC4 | pending | Immutable copy-free Judgement rule view |
| RC5 | pending | Shared physical helpers without theory merging |
| RC6 | pending | Artifact/public boundary decision and migration |
| RC7 | pending | Deletion audit, full verification, and metrics |

Current overall status: 0 of 8 phases complete.

Update phase status when its exit condition is met. Do not mark all phases
complete only after the final test run.

## 11. Required Final Report

The implementation report must include:

```text
baseline commit
final commit
artifact version before/after

per file:
    additions
    deletions
    net

per subsystem:
    ConstraintDB and classifier solving
    effect solving and residual verification
    type schema/representation/readback/cache
    Proposition/Claim/Derivation management
    artifact and driver
    tests
    documentation

behavior:
    accepted examples before/after
    normalized entry results before/after
    artifact replay before/after
    residual obligations before/after

performance:
    clean build time before/after
    focused suite time before/after
    full suite time before/after
```

## 12. Completion Definition

This migration is complete only when:

- ConstraintDB is the sole mutable authority for each classifier and effect
  equation;
- per-occurrence solver data contains references or final frozen projections,
  not duplicate mutable lifecycle state;
- effect solver metas are disposable algorithm workspace;
- residual verification does not depend on compile diagnostic snapshots;
- type schema, representation, readback, and caches have capability-separated
  APIs and no semantic record indexes readback storage;
- Proposition, Claim, and Derivation remain distinct;
- candidate and accepted replay share an immutable, copy-free rule view;
- rule-specific validators remain explicit;
- no permanent compatibility or dual-write path remains;
- artifact replay requires no compiler solver state; and
- focused and full regression suites pass with documented line and runtime
  changes.

## 13. Implementation Log

### 2026-08-17: planning audit completed

- Rechecked committed v77 baseline `0bb1c99` rather than relying only on the
  DA0-DA6 plan.
- Confirmed that classifier/effect lifecycle authority still overlaps between
  `operation_constraint_db`, occurrence solution cells, effect-solver meta
  cells, and compile-metadata snapshots.
- Confirmed that type schema, representation, readback, and classifier caches
  are logically distinguished but still exposed through one over-capable DB
  API.
- Confirmed that Proposition, Claim, and Derivation must remain separate; the
  removable duplication is accepted replay's candidate-shaped premise
  reconstruction and repeated storage plumbing.
- Retained the fixed Issue 12 binder-owner failure as a general derived-index
  transaction invariant for the remaining cache/index audit.
- Confirmed the execution boundary is satisfied: Issue 12 is verified,
  committed, and pushed separately; RC0 may begin from `0bb1c99`.
- No implementation work for RC0-RC7 has started.

Add dated entries as each phase is completed, including changed files, deleted
paths, test commands, and measured line-count changes.
