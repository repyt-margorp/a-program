# Authority and Semantic Duplication Correction Implementation Plan

Date: 2026-08-24

Status: complete; implementation `64c7934` is published on `origin/main`

Repository baseline:

- branch: `main`;
- commit: `14b51b16019f049ec54c0dced1320012f1de3d1d`;
- artifact format: v84;
- implementation root: `src/prototype/`; and
- integration baseline: 42/42 passed, 18.685 seconds shell wall time.

This plan turns the findings in the following documents into reviewable
implementation work:

- `2026-08-24T17-57-04-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-COMPREHENSIVE-AUDIT-PLAN.md`; and
- `2026-08-24T18-14-17-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-AUDIT-REPORT.md`.

The audit documents remain the record of investigation. This document is the
implementation and progress authority. It copies only the conclusions needed
to perform the correction and revalidates them against the code at the
baseline above.

## 1. Objective

For every semantic fact, the compiler must have:

1. one mutable authority while the fact is being computed;
2. one explicit transition that freezes or certifies the result;
3. immutable projections only after that transition;
4. complete cache validity keys for every dependency read by a computation;
5. replay that reconstructs or independently validates accepted evidence; and
6. no compatibility copy retained merely to reduce migration work.

The work is not an attempt to merge every database. It removes duplicate
answers to the same question while preserving records that answer different
questions.

## 2. Required Distinctions

The following distinctions are invariants and are not refactoring targets:

| Concepts | Required decision |
| --- | --- |
| `TermDB` and `TypedOccurrenceGraph` | Preserve context-free Core identity separately from typed source occurrence |
| Context and Substitution | Preserve Context objects separately from Context morphisms |
| Proposition, Claim, and Derivation | Preserve statement, accepted fact, and proof provenance |
| Constraint and Claim | Preserve solver equation separately from accepted theorem evidence |
| Constraint and Verification Obligation | Preserve compile-time search separately from residual runtime contract |
| Constructor telescope and Pi | Preserve schema Context separately from callable classifier |
| DefEq and object Identity | Preserve kernel conversion separately from object-language evidence |
| Parametricity relation and Identity | Preserve relation preservation separately from equality evidence |
| APP and OP_REQUEST | Preserve ordinary elimination separately from free-algebra operation identity |
| APP, MATCH, IH, and FOLD proof rules | Preserve visible kernel rules; share only typed plumbing |
| Pure primitive and effect operation | Preserve their different computation laws despite the shared `#.` namespace |
| Semantic schema, representation, and readback | Preserve meaning, persistent structural identity, and presentation |

No phase in this plan may:

- split APP or LAMBDA into Value-side and Computation-side Core tags;
- encode MATCH, IH, or computation fold away to reduce source lines;
- move accepted typing authority into `TermDB` or `TypedOccurrenceGraph`;
- reflect object Identity into global DefEq;
- derive dependent constructor schema from readback metadata; or
- introduce a remap or compatibility facade between old and new authorities.

## 3. Confirmed Work Packages

| Package | Findings | Priority | Result |
| --- | --- | ---: | --- |
| U1 | F01, F02 | P1 | Universe obligations and closure become independently reconstructible evidence |
| M1 | F03 | P1 | Typed metavariables have one solution authority |
| C1 | F04 | P1 | Schema-dependent normalization caches use complete keys |
| H1 | F08 | P2 | HOTT actions execute against an explicit frozen semantic epoch |
| T1 | F05 | P2 | TypeDeclaration capability boundaries match existing storage boundaries |
| R1 | F06 | P2 | Accepted replay uses immutable storage-neutral premise views throughout |
| J1 | F10 | P2 | Judgement rollback uses one typed transaction API |
| S1 | F07 | P2 | Driver roles use explicit typed storage bundles |
| D1 | F09 | P3 | Full diagnostic effect-constraint copies are removed |

Dependency order:

```text
baseline
   |
   v
U1 Universe closure and artifact v85
   |
   v
M1 typed meta authority ----> D1 diagnostic snapshot removal
   |
   v
C1 cache revision ----------> H1 semantic freeze
   |
   v
T1 TypeDeclaration capabilities
   |
   +----------> R1 accepted replay views
   +----------> J1 Judgement transactions
   +----------> S1 driver storage bundles
   |
   v
integrated replay, determinism, performance, and LOC audit
```

U1 and M1 must not be combined in one implementation commit. U1 changes
accepted evidence and the wire format. M1 changes compiler-local solving.

## 4. Global Completion Gates

- [x] Every removed field has no remaining writer or semantic reader.
- [x] Every derived projection has one validator against its authority.
- [x] No new whole-graph scan is added to a fixed-point inner loop.
- [x] Ordinary source compilation retains
      `context_index_rebuilds=0 substitution_index_rebuilds=0`.
- [x] `::` remains a post-synthesis expectation check.
- [x] Artifact replay rejects evidence that source compilation could not
      construct.
- [x] Artifact publication and readback are deterministic.
- [x] The full integration suite passes after every work package.
- [x] Per-file added, deleted, and net lines are recorded for every package.
- [x] Related GitHub Issues are updated with evidence before close or reopen.

## 5. P0: Baseline and Change Control

### 5.1 Baseline records

- [x] Record commit, artifact version, suite count, and suite wall time.
- [x] Record the ten confirmed findings and the negative decision ledger.
- [x] Record current key-file line counts in section 15.
- [x] Save deterministic artifacts for representative fixtures before U1.
- [x] Save compiler counters for IF8, totality, function graph, effects, and
      HOTT fixtures.
- [x] Record five-run medians for the performance commands in section 14.

### 5.2 Change discipline

Each package must be implemented as:

1. focused failing boundary test;
2. authority model and API change;
3. removal of the old path in the same package;
4. focused tests;
5. full suite and performance comparison;
6. LOC report; and
7. Issue update.

An old field may not remain writable while a new authority is introduced. A
temporary migration branch is permitted only inside an uncommitted edit and
must not be the reviewed end state.

## 6. U1: Universe Obligation and Closure Authority

### 6.1 Problem

Source compilation reconstructs Universe inequalities from Claims and solves
them. Artifact replay currently trusts serialized constraints, levels, and the
`solved` flag, then checks only that the cited Claim exists. It accepts a
positive self-cycle such as `u + 1 <= u`.

The same boundary is incomplete during source compilation. A cumulativity
Claim is accepted before the later global Universe solve closes the inequality
that justifies it.

### 6.2 Semantic model

Universe constraints are a global condition context:

```text
Delta ; Gamma |- t : T
```

A Claim may emit obligations into `Delta`. The accepted compilation image is
not closed until one deterministic Universe certificate closes every required
obligation. This avoids treating cumulativity as DefEq and avoids pretending
that each inequality is an independent term-level premise.

Old authority:

```text
Claim/Derivation
      |
      v
late collector -> UniverseDB.constraints -> levels + solved
                                      \
artifact constraints/levels/solved ---+--> replay trusts wire
```

New authority:

```text
Claim/Derivation + semantic schema
      |
      v
deterministic Universe obligation reconstruction
      |
      +--> Claim-to-obligation coverage
      +--> canonical constraint set
      |
      v
authoritative solver
      |
      v
Universe closure certificate
      |
      +--> source program acceptance
      +--> artifact publication fingerprint
      +--> replay rebuild + solve + exact comparison
```

### 6.3 Data changes

Retain:

- `prototype_universe_node` and `prototype_universe_edge` as source/schema
  projections;
- `prototype_universe_constraint` as the canonical inequality with exact
  provenance;
- `prototype_universe_level` as a solver result used for diagnostics and
  publication validation; and
- `prototype_universe_solve()` as the authoritative solving algorithm.

Introduce in `src/prototype/include/a_program/kernel/universe.h`:

```c
struct prototype_universe_claim_obligation_span {
	uint32_t source_claim_id;
	uint32_t source_derivation_id;
	uint32_t first_constraint;
	uint32_t constraint_count;
};

struct prototype_universe_solution_certificate {
	uint64_t constraint_fingerprint;
	uint64_t solution_fingerprint;
	uint32_t constraint_count;
	uint32_t level_count;
	int state;
};
```

Add typed borrowed storage for obligation spans to `prototype_universe_db` and
replace the ambiguous `int solved` with the certificate state. The final names
may follow local style, but the two roles must remain distinct:

- Claim coverage: which accepted derivation emitted which inequalities; and
- closure certificate: which canonical global constraint set was solved.

Do not add a generic semantic-action tag for Universe closure. Existing
`PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION` identifies an exact graph
action; Universe closure is a global condition certificate and belongs in
`UniverseDB`.

### 6.4 API changes

Replace the monolithic collection contract with explicit phases:

```c
int prototype_universe_reconstruct_obligations(...);
int prototype_universe_close(...);
int prototype_universe_validate_closed_program(...);
int prototype_universe_compare_certificate(...);
```

`prototype_universe_collect()` and
`prototype_universe_validate_provenance()` are removed after their callers
migrate. No wrapper retaining the old acceptance behavior remains.

`prototype_universe_reconstruct_obligations()` must:

- clear only compiler-derived Universe state;
- derive every endpoint, offset, and reason from Claim/Derivation and semantic
  schema;
- produce constraints in a deterministic order;
- create exact Claim-to-obligation coverage;
- reject a provenance record that cannot be derived; and
- not read serialized Universe constraints as input.

`prototype_universe_close()` must:

- run the current solver over the reconstructed constraints;
- reject positive cycles and capacity exhaustion;
- canonicalize level results;
- compute both fingerprints; and
- publish the certificate only after all checks succeed.

`prototype_universe_validate_closed_program()` must ensure that every proof
kind requiring Universe evidence is covered and that no serialized/public root
is accepted without a current certificate.

### 6.5 Source pipeline migration

- [x] Add obligation-span and certificate storage to
      `prototype_program_storage_backing`.
- [x] Make compiler session finalization reconstruct and close Universe data
      after the Judgement graph is stable.
- [x] Gate successful `prototype_compile_graph*()` return on a valid closure
      certificate.
- [x] Make artifact publication require the same certificate.
- [x] Remove all acceptance decisions based only on `universe.solved`.
- [x] Add a debug validator that reconstructs a second UniverseDB and compares
      fingerprints without sharing the first builder state.

Claims remain immutable. The program-level certificate closes their global
Universe condition. This avoids rewriting the Claim DAG after solving while
making it impossible to treat a bare JudgementDB as a closed compilation
image.

### 6.6 Artifact v85

U1 opens artifact v85. There will be no current-build v84 compatibility reader.

The v85 wire format must not serialize mutable solver authority. Replace the
v84 raw `solved` acceptance input with one of these exact policies:

- preferred: serialize only the canonical certificate fingerprints and
  counts, then always reconstruct constraints and levels during replay; or
- if levels remain useful wire diagnostics, serialize them as checked
  projections and compare every entry against the rebuilt solution.

The first policy is preferred because constraints and levels are derivable
from accepted Claims and schema. It reduces the wire surface and prevents a
second solution authority.

Required implementation files include:

- new `src/prototype/spec/artifact_v85.schema`;
- new `src/prototype/include/a_program/artifact/wire_v85.h`;
- new `src/prototype/src/artifact/wire_v85.c`;
- publication section writers and dense publication;
- `src/prototype/src/driver/read_file.c`;
- artifact relocation/link validation where certificate identity is carried;
  and
- README references to the current version.

Archive v84 documentation as historical. Do not keep v84 parser code in the
current reader merely for backward compatibility.

### 6.7 U1 tests

- [x] Add a source fixture whose Universe inequalities form a positive cycle
      and require compile rejection.
- [x] Mutate a v85 certificate fingerprint and require replay rejection.
- [x] Mutate a v85 constraint count and require replay rejection.
- [x] If numerical levels remain serialized, mutate one and require rejection.
- [x] Remove Claim coverage for one cumulativity derivation and require
      rejection.
- [x] Change an endpoint or offset while retaining a valid source Claim and
      require reconstruction disagreement.
- [x] Publish, read, republish, and compare deterministic artifact bytes.
- [x] Prove source compilation and replay produce the same two fingerprints.

### 6.8 U1 exit gate

- [x] No artifact field named `solved` controls Universe acceptance.
- [x] No replay path accepts wire Universe data without reconstruction.
- [x] Every Universe-dependent root is covered by the current certificate.
- [x] DefEq remains level-identity comparison; cumulativity remains separate.
- [x] Artifact v85 schema and implementation agree.
- [x] Full integration and artifact mutation suites pass.

## 7. M1: Typed Metavariable Solution Authority

### 7.1 Problem

`operation_constraint_db` now correctly owns equation lifecycle, dependencies,
reason, and evidence. Solved classifier/effect values are still written into:

- `operation_solver_solution.classifier`;
- `operation_solver_solution.effect_row`;
- `operation_effect_row_meta.solution_row`;
- `compile_context.classifier_hints[]`;
- occurrence `binder_classifier`; and
- classifier Terms rewritten during effect materialization.

The current `test_constraint_authority.sh` checks that lifecycle fields are not
duplicated. It does not check that the solution itself has one owner.

### 7.2 New authority model

```text
Generation facts
  declared/expected classifier seeds
             |
             v
ConstraintDB ------------------------+
  equations/lifecycle/dependencies   |
                                      v
TypedMetaSolutionDB <---------- domain solvers
  one solution per classifier/effect/usage meta
             |
             v
checked freeze, exactly once
             |
             +--> immutable TypedOccurrence classifier/status
             +--> Proposition/Claim evidence
             +--> Verification residual
```

Constraints do not own one result Term because several constraints may solve
one meta. Typed occurrences do not own mutable solver answers because they are
the frozen source graph.

### 7.3 Exact data migration

Replace `operation_solver_solution` with typed stores:

```c
struct operation_classifier_meta_solution {
	uint32_t owner_occurrence;
	uint32_t classifier;
	uint32_t evidence_constraint_id;
	int state;
};

struct operation_effect_meta_solution {
	uint32_t owner_occurrence;
	uint32_t placeholder_row;
	uint32_t first_solution_atom;
	uint32_t solution_atom_count;
	uint32_t materialized_row;
	uint32_t evidence_constraint_id;
	int state;
};
```

Keep usage analysis typed and separate. Move
`first_usage_entry`, `usage_entry_count`, `binder_usage`, and `usage_state`
into the existing per-occurrence usage solution structure rather than forcing
resource analysis into classifier meta storage.

Remove:

- `struct operation_solver_solution`;
- `operation_classifier_solver.solutions[]`;
- every solved-value write to `classifier_hints[]`;
- `operation_effect_materialize_solution()` writes to hints, occurrences, or
  classifier solution cells; and
- mutable fallback reads from `TypedOccurrence.classifier` during solving.

`operation_effect_row_meta` becomes the effect solution authority or is
replaced by `operation_effect_meta_solution`; do not retain both records.

### 7.4 Separate seeds from solutions

`classifier_hints[]` is currently overloaded. Some writes are legitimate
generation facts from annotations, binders, and already declared references;
others mirror solved classifiers.

Replace it with a typed, phase-limited seed store:

```c
struct operation_classifier_seed {
	uint32_t owner_occurrence;
	uint32_t classifier;
	int reason;
};
```

Rules:

- graph construction may append or unify seeds;
- the seed store freezes before fixed-point solving;
- solvers read seeds but never write them;
- solved classifiers exist only in classifier meta solutions; and
- freeze consumes both seeds and solutions, then validates agreement.

If a call-site currently uses a hint as speculative backtracking state, move
that state into the relevant constraint transaction. It must not remain in the
seed API.

### 7.5 Constraint references

Rename `operation_constraint.payload.classifier.classifier_variable` to an
explicit classifier meta ID. Effect constraints similarly refer to effect meta
IDs rather than using an occurrence or result Term as implicit identity.

Introduce O(1) lookup APIs:

```c
const struct operation_classifier_meta_solution*
operation_classifier_meta_get(...);

int operation_classifier_meta_solve(...);

const struct operation_effect_meta_solution*
operation_effect_meta_get(...);

int operation_effect_meta_solve(...);
```

Each solve API must:

- accept the first solution;
- accept a later convertible solution without replacing authority;
- reject a later non-convertible solution;
- record the constraint that supplied evidence; and
- enqueue dependents through `ConstraintDB`, not scan all constraints.

### 7.6 Freeze boundary

Create one checked operation, conceptually:

```c
int operation_solver_freeze_occurrences(struct compile_context* ctx);
```

It must be the only solver-to-graph writer for:

- occurrence classifier;
- classifier status;
- classifier Verification obligation;
- binder classifier after effect substitution; and
- immutable resource usage attached to published Propositions.

The freeze validates:

- every solved classifier equation points to a solved classifier meta;
- every solved effect equation points to a solved effect meta;
- every materialized effect row equals the meta normal form;
- residual metas have one exact Verification obligation; and
- a frozen occurrence never disagrees with its meta solution.

After freeze, all meta stores become read-only and are compiler-local. They are
not serialized.

### 7.7 M1 migration order

- [x] Inventory and classify every `classifier_hints[]` writer as seed,
      speculative transaction state, solved mirror, or frozen projection.
- [x] Introduce typed seed and meta stores with no consumers yet.
- [x] Migrate classifier constraint payloads and O(1) lookups.
- [x] Migrate classifier solver writers and readers.
- [x] Migrate effect meta solving without changing effect equations.
- [x] Move usage fields to the usage solution owner.
- [x] Introduce the single freeze operation.
- [x] Delete `operation_solver_solution` and `classifier_hints[]`.
- [x] Delete direct solver writes to occurrence classifier fields.
- [x] Add static source checks that prevent these paths from returning.

### 7.8 M1 tests

- [x] Extend `test_constraint_authority.sh` to reject solved classifier/effect
      values in generic solution cells or hint arrays.
- [x] Add a C test that perturbs a frozen occurrence after freeze and requires
      authority validation failure.
- [x] Add a C test that submits two non-convertible solutions for one meta.
- [x] Add a C test for one meta constrained by multiple convertible equations.
- [x] Add a C test for a solved effect equation with an unresolved effect meta.
- [x] Verify deleting seed workspace after solver initialization does not
      change artifact bytes.
- [x] Run List/append, indexed family, IF8, effect handler, and totality
      fixtures.

### 7.9 M1 performance gate

- no per-lookup scan across `ConstraintDB`;
- no extra full fixed-point pass;
- one initial constraint-generation pass;
- zero ordinary Context/Substitution rebuilds;
- IF8 median must remain within 10% of baseline unless a measured correctness
  cost is reviewed; and
- constraint enqueue/pop counters must not increase without an explained new
  equation.

## 8. D1: Diagnostic Effect Snapshot Removal

D1 follows M1 because M1 establishes the final effect authority.

Remove from `prototype_compile_metadata`:

- `effect_constraints`;
- `effect_constraint_count`; and
- `effect_constraint_capacity`.

Remove corresponding backing arrays and initialization parameters from:

- `src/prototype/src/graph/compile_metadata.c`;
- `src/prototype/src/driver/program_storage.c`;
- `src/prototype/src/driver/read_file.c`;
- artifact publication/append initialization; and
- provider/local/artifact role storage.

Diagnostics that need totals receive a compact immutable summary:

```c
struct prototype_effect_constraint_summary {
	uint32_t solved_count;
	uint32_t residual_count;
	uint32_t failed_count;
};
```

This summary is not an equation table and cannot generate Verification
obligations. Residual obligations continue to be produced directly from
`ConstraintDB` at freeze.

- [x] Remove the stale v82 comment.
- [x] Prove no artifact field changes because the snapshot was not serialized.
- [x] Require source checks that Verification creation never reads diagnostics.

## 9. C1: Schema-Complete Normalization Cache Keys

### 9.1 Problem and model

`PROTOTYPE_TERM_REDUCTION_PROFILE_PURE_TYPE_WHNF` reads type semantic schema,
but normalization cache entries contain only Term ID, profile, and Term graph
revision.

Old key:

```text
(term_id, profile, term_graph_revision)
```

New key:

```text
CORE profiles:
  (term_id, profile, term_graph_revision)

schema-dependent profiles:
  (term_id, profile, term_graph_revision, type_schema_revision)
```

### 9.2 Data and APIs

Add `uint64_t semantic_revision` to
`prototype_term_normalization_cache_entry`.

Change internal lookup/reserve/bucket helpers to receive one computed semantic
revision. The value is:

- `0` for schema-independent profiles; and
- `type_declarations->semantic_schema.semantic_revision` for profiles that
  inspect type-former/schema data.

Add one helper that classifies profile dependencies. Do not duplicate the
profile switch across lookup, reserve, and invalidation.

No wire change is required. This is a runtime cache validity correction.

### 9.3 C1 tests

- [x] Extend `whnf_profile_cache_check.c` with a schema-dependent neutral Match
      or partial type application.
- [x] Populate the cache, mutate/finalize the schema without changing the
      source Term, and require a miss and recomputation.
- [x] Prove Core WHNF remains a cache hit across a schema-only revision.
- [x] Check stale-revision and hit/miss counters.
- [x] Run artifact flow and conversion tests.

## 10. H1: Explicit HOTT Semantic Freeze

### 10.1 Problem

HOTT action results are interned and retrieved by request ID. Certificates can
read Contexts, Substitutions, typed occurrences, Claims, and semantic schema,
but the result validity contract records no complete semantic epoch.

Append-only Term growth and immutable Claims do not by themselves invalidate a
completed action. Mutation of an already referenced semantic schema or Context
would.

### 10.2 Selected policy

Use an explicit freeze contract rather than adding every arena count to every
action key.

Introduce a kernel semantic epoch:

```c
struct prototype_kernel_semantic_epoch {
	uint64_t type_schema_revision;
	uint64_t context_revision;
	uint64_t substitution_revision;
	uint64_t judgement_revision;
};
```

Only revisions for mutation of existing meaning belong here. Pure append-only
growth that cannot change an existing ID does not invalidate old results.

Add to `prototype_hott_action_db`:

- the epoch captured when action execution begins; and
- a frozen flag/state.

Action request publication, result lookup, and certificate validation must
reject an epoch mismatch. Type schema builders and Context/Substitution bulk
loaders must explicitly seal before HOTT execution. A new compiler session or
artifact load creates a new action DB; stale results are never remapped.

Do not merge relation-action and object-Identity request kinds. The epoch is
shared infrastructure, not shared theorem identity.

### 10.3 H1 tests

- [x] Reuse an action under the same epoch and require the same result ID.
- [x] Mutate semantic schema after capture and require lookup rejection.
- [x] Mutate a referenced Context/Substitution through a test-only bulk-load
      path and require rejection.
- [x] Append an unrelated immutable Term and prove a valid action remains
      reusable if the epoch contract permits it.
- [x] Replay HOTT artifact roots only after the loaded kernel view is sealed.

No wire change is required unless the epoch becomes part of persistent HOTT
certificate identity. The preferred design keeps epochs runtime-local and
revalidates persistent certificates against the freshly loaded stores.

## 11. T1: TypeDeclaration Capability Separation

### 11.1 Problem

Storage is already split into semantic schema, readback, representation, and
constructor-classifier cache. Many APIs still accept a broad mutable
`prototype_type_declaration_db*`; `prototype_type_declaration.first_parameter`
is a readback offset inside a semantic record.

### 11.2 Authority model

```text
prototype_type_declaration_storage
  |
  +-- semantic_schema_builder/view   authoritative schema
  +-- readback_builder/view          source reconstruction only
  +-- representation_builder/view    persistent structural identity
  +-- classifier_cache               rebuildable materialization
  +-- specialization_stats           diagnostics only
```

The aggregate remains a session storage composition. Kernel consumers receive
only the capabilities they need.

### 11.3 Data changes

- Move `first_parameter` out of `prototype_type_declaration` into a readback
  type-entry table keyed by type ID.
- Keep semantic `parameter_count`, `parameter_context`, `index_context`, and
  constructor Context telescopes in semantic schema.
- Keep representation ID in semantic declaration because it is persistent
  structural identity, while fingerprint lookup remains in representation DB.
- Keep constructor classifier cache rebuildable and indexed by semantic
  constructor ID.

Introduce explicit narrow types, using local naming conventions:

```text
prototype_type_semantic_schema_view
prototype_type_semantic_schema_builder
prototype_type_readback_view
prototype_type_readback_builder
prototype_type_representation_view
prototype_type_representation_builder
prototype_constructor_classifier_cache_view/builder
```

### 11.4 API migration

- [x] Classify all broad DB parameters by actual fields read/written.
- [x] Convert pure kernel validators and conversion to semantic read-only view.
- [x] Convert schema construction to semantic builder plus explicit readback
      builder where source reconstruction is also written.
- [x] Convert interface/link/publication code to an explicit capability set.
- [x] Remove broad DB parameters from semantic query APIs.
- [x] Remove any const cast needed only to call a cache-writing query; make
      cache mutation explicit in the capability.
- [x] Delete old broad semantic wrappers after all callers migrate.

The baseline broad-parameter search had 543 matches across prototype headers,
kernel, artifact, and identity code. The aggregate remains as the typed
lifetime owner used by orchestration. It is not a semantic capability: leaf
queries and validators now receive semantic schema, readback, representation,
or classifier-cache capabilities explicitly.

### 11.5 Wire naming

In v85, rename persistent interface field `curried_classifier_cache` to
`constructor_classifier`. It is accepted interface evidence, not an optional
runtime cache. This rename is included while v85 is open; it must not trigger a
second version bump.

### 11.6 T1 tests

- [x] Clear readback after semantic construction and prove kernel validation is
      unchanged.
- [x] Clear constructor classifier cache and prove it rebuilds to the same
      classifier.
- [x] Rebuild representation lookup index and prove representation IDs remain
      unchanged.
- [x] Reject a dependent constructor whose semantic Context telescope is
      malformed even when readback appears valid.
- [x] Static check that semantic validators cannot name readback fields.

Permanent coverage is split between
`core_view_representation_check.c`, artifact constructor-cache mutation tests,
and `test_type_capability_authority.sh`.

## 12. R1: Storage-Neutral Accepted Premise Views

Accepted replay already has `prototype_judgement_premise_view` and
`prototype_judgement_rule_application_view`. Nine replay paths still allocate
`prototype_judgement_candidate_premise[]` adapters.

### 12.1 Required change

- Change shared rule-checking helpers to consume only immutable
  `prototype_judgement_premise_view` arrays.
- Keep `prototype_judgement_candidate_premise` confined to candidate proof
  construction/publication.
- Build accepted views directly from Claim/Derivation edges.
- Remove replay-local candidate arrays at the currently identified call sites
  near lines 2046, 2088, 2127, 2233, 2335, 2752, 4004, 4047, and 5274.
- Preserve separate APP, MATCH, IH, and computation-fold validators.
- Add a source audit that fails if accepted replay declares candidate premise
  storage again.

This is physical consolidation only. Proposition/Claim/Derivation identities
and premise DAG semantics do not change. No wire version change is required.

## 13. J1: Typed Judgement Transactions

### 13.1 Problem

Candidate publication and early formation manually restore counts and rebuild
indexes. Some rollback paths discard the return value of
`prototype_judgement_db_rebuild_index()`.

### 13.2 API

Introduce in the Judgement DB API:

```c
struct prototype_judgement_transaction_mark {
	/* Every authoritative arena count and derived-index generation. */
};

int prototype_judgement_transaction_begin(...);
int prototype_judgement_transaction_commit(...);
int prototype_judgement_transaction_rollback(...);
```

The mark must cover:

- Proposition count;
- candidate Derivation count;
- Claim count;
- accepted Derivation count;
- candidate and accepted premise counts;
- resource usage count; and
- any additional arena count discovered by the implementation inventory.

Rollback restores authoritative counts first, then repairs every derived index
exactly once. It must either be allocation-free and infallible or propagate one
error; ignored rebuild results are forbidden.

Replace local transaction structures and manual rollback in
`candidate_publication.inc`, `candidate_replay.inc`, and formation paths. Do not
combine Proposition, Claim, or Derivation storage.

### 13.3 J1 tests

- [x] Force rollback after each authoritative arena extent and compare all
      counts to the
      pre-transaction mark.
- [x] Verify rollback repairs the lookup index from restored authoritative
      extents.
- [x] Assert exactly one rebuild per rollback, and zero rebuilds on commit.
- [x] Run artifact replay mutation tests through rollback paths.

The transaction boundary deliberately restores arena extents before rebuilding
the derived index. It does not snapshot or duplicate Proposition, Claim, or
Derivation contents.

## 14. S1: Explicit Driver Storage Bundles

### 14.1 Problem

`program_storage.c` already owns a typed backing bundle. `read_file.c` still
declares large parallel static arrays for local, provider, artifact, appended,
and imported roles, with repeated initialization calls.

### 14.2 Required model

```text
driver command/session
  |
  +-- local_program_storage
  +-- provider_program_storage
  +-- linked_program_storage
  +-- artifact_interface_storage
  +-- imported_interface_storage[]
```

These are lifetime owners, not semantic mega-databases. Their members remain
typed DBs.

Introduce role-specific capacity profiles only where measured sizes differ.
The common initialization and destruction algorithm belongs in driver storage
support. Artifact interface arrays that are not a `prototype_program` receive
a separate typed interface-storage bundle rather than being forced into
program storage.

### 14.3 Migration

- [x] Inventory every static array in `read_file.c` by owner role.
- [x] Extend `prototype_program_storage` with resettable typed ownership and
      add named constructors for existing fixed profiles.
- [x] Add `prototype_artifact_interface_storage` for interface-only arrays.
- [x] Replace local role globals and initialization first.
- [x] Replace provider and linked role globals.
- [x] Replace appended/imported interface arrays.
- [x] Make command execution reentrant with all storage reachable from a local
      session owner.
- [x] Delete the old static arrays and repeated initialization blocks in one
      reviewed package.

Do not use an untyped byte arena. Do not hide semantic DB pointers behind
`void*` callbacks.

### 14.4 S1 tests

- [x] Execute two independent reader sessions sequentially in one process.
- [x] Execute a failed read followed by a valid read and prove no stale counts.
- [x] Link local/provider/imported artifacts and compare bytes with baseline.
- [x] Run memory and capacity-boundary checks already used by artifact flow.
- [x] Report `read_file.c` and storage-module LOC before and after.

`read_file.c` changed by +1,009/-2,302 lines after U1. The typed storage owner
grew by +212/-15 lines and its public driver header by +29/-0 lines. The net
driver-storage change is -1,067 lines while removing file-static backing
arrays.

## 15. Test and Performance Matrix

### 15.1 Commands

Full suite:

```sh
TIMEFORMAT='elapsed=%3R user=%3U sys=%3S'
time make -f src/prototype/Makefile test-integration
```

IF8 median and counters:

```sh
make -f src/prototype/Makefile benchmark-if8-single-compile
```

Focused authority boundaries:

```sh
sh src/prototype/tests/integration/test_constraint_authority.sh
sh src/prototype/tests/integration/test_artifact_flow.sh
sh src/prototype/tests/integration/test_context_resolution_incremental.sh
sh src/prototype/tests/integration/test_hott_goal.sh
```

Each package records five measured runs after one warm-up. Use median wall time
for comparison and retain semantic counters from deterministic output.

### 15.2 Required fixture coverage

| Boundary | Fixtures/tests |
| --- | --- |
| Universe closure | Bool, Pi, expected-type cumulativity, malformed artifact |
| Meta authority | identity Bool/Nat, List append, effects, indexed family, IF8 |
| Cache key | schema-dependent neutral Match and schema-independent Core WHNF |
| HOTT epoch | relation action, object Identity, artifact identity roots |
| Type capability | dependent constructor, cache/readback clearing |
| Replay view | APP, MATCH, IH, fold accepted replay |
| Transaction | forced failure after each Judgement arena append |
| Storage | local/provider/import/link/read/republish flows |

### 15.3 Performance rejection conditions

Redesign before merge if any package causes:

- repeated whole-constraint scans in a lookup path;
- more than one initial constraint-generation pass;
- ordinary Context/Substitution index rebuilds;
- loss of artifact determinism;
- more than 10% IF8 median regression without identified semantic work; or
- an unexplained increase in proof reification recursion, alpha comparison, or
  constraint pop counters.

## 16. LOC Ledger

Baseline line counts:

| File | Baseline lines | Added | Deleted | Net | Package |
| --- | ---: | ---: | ---: | ---: | --- |
| `include/a_program/kernel/universe.h` | 159 | 50 | 2 | +48 | U1 |
| `src/kernel/universe.c` | 274 | 108 | 3 | +105 | U1 |
| `src/frontend/universe_collection.c` | 801 | 297 | 51 | +246 | U1 |
| `src/artifact/wire_v84.c` | 3,750 | 85 | 26 | +59 | U1/v85 rename |
| `src/driver/read_file.c` | 5,472 | 1,020 | 2,287 | -1,267 | U1/S1 |
| `src/frontend/lowering/context_and_type_lowering.inc` | 5,803 | 127 | 67 | +60 | M1 |
| `src/frontend/lowering/constraint/effect_propagation_and_residuals.inc` | 1,612 | 177 | 78 | +99 | M1/D1 |
| `src/frontend/lowering/constraint/evidence_and_freeze.inc` | 4,256 | 160 | 64 | +96 | M1 |
| `include/a_program/kernel/type_declaration.h` | 502 | 51 | 30 | +21 | T1 |
| `src/kernel/type_declaration.c` | 2,617 | 181 | 134 | +47 | T1 |
| `src/kernel/typing/accepted_replay.inc` | 8,020 | 79 | 196 | -117 | R1 |
| `src/kernel/typing/candidate_publication.inc` | 2,202 | 238 | 107 | +131 | J1 |
| `src/driver/program_storage.c` | 246 | 216 | 16 | +200 | S1 |
| `include/a_program/graph/compile_metadata.h` | 487 | 10 | 6 | +4 | D1 |
| `src/graph/compile_metadata.c` | 426 | 0 | 4 | -4 | D1 |
| `include/a_program/identity/types.h` | 362 | 2 | 0 | +2 | H1 |
| `src/identity/action_certificate_validation.inc` | 1,356 | 3 | 3 | 0 | H1 |
| `src/identity/action_execution.inc` | 1,551 | 6 | 2 | +4 | H1 |

Final reporting must use `git diff --numstat <package-base>..<package-tip>` and
also report total implementation, test, and documentation deltas separately.
Generated files and artifact fixtures are not counted as implementation LOC.

Baseline-to-final category totals are: implementation 65 files,
`+4,872/-3,590` (net +1,282); tests 25 files, `+679/-129` (net +550);
documentation/specification 9 files, `+3,891/-15` (net +3,876).

Code reduction is desirable in R1, S1, and D1. It is not a gate for U1, M1,
C1, H1, or T1, where explicit evidence and narrow APIs may add lines. The gate
is removal of duplicate meaning, not a lower raw line count.

## 17. Artifact Version Plan

- U1 creates v85 because accepted Universe evidence semantics change.
- T1 includes the `constructor_classifier` wire rename before v85 is frozen.
- M1, C1, R1, J1, S1, and D1 are compiler-local or physical and must not add
  wire fields.
- H1 remains runtime-local unless persistent certificate validation proves an
  epoch field is semantically necessary.
- v84 schema moves to `spec/archive/` after v85 acceptance tests pass.
- README and prototype README are updated to v85 in the same package.
- No v84 compatibility parser remains in the current build.

## 18. GitHub Issue Disposition

Existing Issues #17 and #18 are closed. The re-audit found residual physical
and authority gaps, but the implementation removes them without invalidating
the original closure decisions. Reopening only to close them again would lose
the distinction between the historical baseline and this follow-up.

Final handling:

- [x] Post the M1/D1 follow-up evidence and final commit to #18; leave it
      closed after confirming no acceptance criterion regressed.
- [x] Post the T1/R1 follow-up evidence and final commit to #17; leave it
      closed after confirming accepted replay and capability boundaries.
- [x] Keep U1 recorded in this implementation plan and artifact v85 history;
      do not create and immediately close an administrative Issue.
- [x] Keep C1/H1/J1/S1 in this integrated correction record because no
      unresolved design decision remains after their focused tests.
- [x] Do not attach unrelated open Issue #13 to this work.

An Issue closes only after its focused tests, full suite, performance record,
artifact impact, and LOC delta are posted. A finding rejected during
implementation closes with code evidence explaining why no authority is
duplicated.

## 19. Integrated Validation

After all packages:

- [x] Re-run the complete writer/reader authority matrix from the audit.
- [x] Search for removed symbols and old artifact version references.
- [x] Re-run all artifact mutation tests with independent writer and reader
      reconstruction.
- [x] Compare source compile and replay Claims, Derivations, Universe
      certificate, and public roots.
- [x] Verify one mutable writer for every classifier/effect meta solution.
- [x] Verify all schema-dependent caches contain semantic revision.
- [x] Verify HOTT action lookup cannot cross a semantic epoch.
- [x] Verify accepted replay contains no candidate premise arrays.
- [x] Verify rollback contains no ignored index rebuild result.
- [x] Verify `read_file.c` owns no parallel program DB backing arrays.
- [x] Run 42/42 or the then-current larger full suite.
- [x] Record final five-run performance medians and counters.
- [x] Record per-file and total LOC deltas after staging all new tests.
- [x] Update closed Issues #17 and #18 with final follow-up evidence.

## 20. Progress Summary

| Phase | Status | Completion evidence |
| --- | --- | --- |
| Audit and baseline | complete | Two 2026-08-24 audit documents; 42/42 baseline |
| P0 deterministic fixtures/counters | complete | Baseline artifacts and IF8 counters stored under `/tmp/a-program-authority-baseline-14b51b1.jeDAVd`; performance medians remain a final cross-package gate. |
| U1 Universe closure and v85 | complete | Artifact v85 reconstructs and solves Universe obligations from accepted evidence; 42/42 integration tests pass. |
| M1 typed meta authority | complete | Typed classifier, effect-row, and usage stores; one freeze to checked occurrence projections. |
| D1 diagnostic snapshot removal | complete | Full effect-equation copies removed from compile metadata; residuals originate at ConstraintDB freeze. |
| C1 normalization cache revision | complete | Schema revision participates only in schema-dependent reduction profiles. |
| H1 HOTT semantic freeze | complete | Action reuse validates Context, Substitution, Judgement, and type-schema epoch. |
| T1 TypeDeclaration capabilities | complete | Semantic schema, readback, representation, and classifier cache APIs are explicit; aggregate remains lifetime owner only. |
| R1 accepted premise views | complete | Accepted replay consumes immutable premise views and owns no candidate premise adapter arrays. |
| J1 Judgement transactions | complete | One begin/commit/rollback API restores all authoritative extents and repairs the index once. |
| S1 driver storage bundles | complete | Local/provider/interface/import roles use local typed owners; `read_file.c` has no backing arrays. |
| Integrated validation | complete | 45/45 passed in 31.148 s suite wall time; staged LOC is recorded below. |
| GitHub disposition | complete | Follow-up evidence posted to closed Issues #17 and #18; unrelated #13 unchanged. |

### 20.1 Implementation-time correction

The HOTT regression run exposed a cross-layer ambiguity not listed in F01-F10.
Because context-free Core Terms are shared, one Term can have accepted typing
evidence in several Contexts. Candidate synthesis must not scan and merge all
of those derivations. It now accepts evidence only from the current Context or
an ancestor Context, preferring non-conversion evidence and consulting
conversion evidence only when no direct candidate exists. This preserves Core
sharing without treating Judgement evidence as a global property of a Term ID.

The permanent boundary is covered by `context_category_check.c` and the full
HOTT integration suite.

## 21. Final Performance Record

Five-run medians for the same IF8-style compiler invocation:

| Revision | Median | Change from previous |
| --- | ---: | ---: |
| baseline `14b51b1` | 183 ms | - |
| U1 `c2e9aca` | 197 ms | +7.7% |
| all current semantic packages | 211 ms | +7.1% |

The total baseline-to-current change is +15.3%. It is attributable to
independent Universe closure/replay evidence and Context-visible typing
evidence checks, not a repeated constraint-generation pass or index rebuild.
The result remains below the 500 ms hard gate for this invocation.

Ordinary compilation counters remain:

```text
context_index_rebuilds=0
substitution_index_rebuilds=0
constraint_generations=1
constraint_indexes=1
constraint_pops=8974
```

## 22. Baseline-to-Final Per-File LOC

The following ledger is generated from
`git diff --numstat 14b51b1 --`. Rename rows are counted once. Generated
build outputs are excluded.

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `README.md` | 7 | 5 | 2 |
| `doc/2026-08-24T17-57-04-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-COMPREHENSIVE-AUDIT-PLAN.md` | 934 | 0 | 934 |
| `doc/2026-08-24T18-14-17-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-AUDIT-REPORT.md` | 697 | 0 | 697 |
| `doc/2026-08-24T18-24-59-AUTHORITY-AND-SEMANTIC-DUPLICATION-CORRECTION-IMPLEMENTATION-PLAN.md` | 1246 | 0 | 1246 |
| `doc/2026-08-24T21-31-21-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-RE-AUDIT-PLAN-V2.md` | 609 | 0 | 609 |
| `src/prototype/README.md` | 11 | 9 | 2 |
| `src/prototype/build/sources.mk` | 1 | 1 | 0 |
| `src/prototype/calculus.h` | 2 | 2 | 0 |
| `src/prototype/include/a_program/artifact/interface.h` | 2 | 2 | 0 |
| `src/prototype/include/a_program/artifact/{wire_v84.h => wire_v85.h}` | 2 | 2 | 0 |
| `src/prototype/include/a_program/core/term.h` | 3 | 0 | 3 |
| `src/prototype/include/a_program/driver/compiler_session.h` | 29 | 0 | 29 |
| `src/prototype/include/a_program/frontend/universe_collection.h` | 18 | 2 | 16 |
| `src/prototype/include/a_program/graph/compile_metadata.h` | 10 | 6 | 4 |
| `src/prototype/include/a_program/graph/verification.h` | 1 | 1 | 0 |
| `src/prototype/include/a_program/identity/action_certificate.h` | 8 | 0 | 8 |
| `src/prototype/include/a_program/identity/types.h` | 2 | 0 | 2 |
| `src/prototype/include/a_program/kernel/context.h` | 10 | 0 | 10 |
| `src/prototype/include/a_program/kernel/judgement/db.h` | 12 | 0 | 12 |
| `src/prototype/include/a_program/kernel/judgement/types.h` | 17 | 0 | 17 |
| `src/prototype/include/a_program/kernel/kernel_view.h` | 23 | 0 | 23 |
| `src/prototype/include/a_program/kernel/type_declaration.h` | 51 | 30 | 21 |
| `src/prototype/include/a_program/kernel/universe.h` | 50 | 2 | 48 |
| `src/prototype/spec/archive/README.md` | 1 | 1 | 0 |
| `src/prototype/spec/{ => archive}/artifact_v84.schema` | 0 | 0 | 0 |
| `src/prototype/spec/artifact_v85.schema` | 386 | 0 | 386 |
| `src/prototype/src/artifact/interface.c` | 41 | 5 | 36 |
| `src/prototype/src/artifact/link.c` | 32 | 15 | 17 |
| `src/prototype/src/artifact/publication/closure_marking_and_slices.inc` | 18 | 7 | 11 |
| `src/prototype/src/artifact/publication/dense_publication.inc` | 43 | 16 | 27 |
| `src/prototype/src/artifact/publication/section_writers.inc` | 45 | 9 | 36 |
| `src/prototype/src/artifact/publication/wire_primitives.inc` | 1 | 1 | 0 |
| `src/prototype/src/artifact/publication/writer.inc` | 1 | 1 | 0 |
| `src/prototype/src/artifact/relocation.c` | 5 | 3 | 2 |
| `src/prototype/src/artifact/{wire_v84.c => wire_v85.c}` | 85 | 26 | 59 |
| `src/prototype/src/core/term/evaluation_and_conversion.inc` | 47 | 7 | 40 |
| `src/prototype/src/core/term/storage_and_formation.inc` | 37 | 13 | 24 |
| `src/prototype/src/driver/compiler_session.c` | 25 | 11 | 14 |
| `src/prototype/src/driver/diagnostics.c` | 16 | 2 | 14 |
| `src/prototype/src/driver/program_storage.c` | 216 | 16 | 200 |
| `src/prototype/src/driver/read_file.c` | 1020 | 2287 | -1267 |
| `src/prototype/src/driver/repl.c` | 6 | 3 | 3 |
| `src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc` | 178 | 73 | 105 |
| `src/prototype/src/frontend/lowering/constraint/classifier_and_computation_propagation.inc` | 103 | 83 | 20 |
| `src/prototype/src/frontend/lowering/constraint/context_computation_and_fixed_point.inc` | 12 | 4 | 8 |
| `src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc` | 177 | 78 | 99 |
| `src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc` | 160 | 64 | 96 |
| `src/prototype/src/frontend/lowering/constraint/model_generation_and_index.inc` | 82 | 20 | 62 |
| `src/prototype/src/frontend/lowering/context_and_type_lowering.inc` | 127 | 67 | 60 |
| `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` | 76 | 24 | 52 |
| `src/prototype/src/frontend/lowering/graph_construction.inc` | 717 | 150 | 567 |
| `src/prototype/src/frontend/universe_collection.c` | 297 | 51 | 246 |
| `src/prototype/src/graph/compile_metadata.c` | 0 | 4 | -4 |
| `src/prototype/src/graph/typed_occurrence/verification.inc` | 31 | 8 | 23 |
| `src/prototype/src/identity/action_certificate_init.inc` | 39 | 0 | 39 |
| `src/prototype/src/identity/action_certificate_validation.inc` | 3 | 3 | 0 |
| `src/prototype/src/identity/action_execution.inc` | 6 | 2 | 4 |
| `src/prototype/src/identity/identity_computation.inc` | 5 | 2 | 3 |
| `src/prototype/src/identity/object_term_action.inc` | 5 | 2 | 3 |
| `src/prototype/src/kernel/context.c` | 38 | 0 | 38 |
| `src/prototype/src/kernel/kernel_view.c` | 105 | 0 | 105 |
| `src/prototype/src/kernel/rules/cbpv.inc` | 25 | 2 | 23 |
| `src/prototype/src/kernel/rules/elimination_app.inc` | 14 | 2 | 12 |
| `src/prototype/src/kernel/rules/formation_early.inc` | 15 | 6 | 9 |
| `src/prototype/src/kernel/rules/formation_recording.inc` | 1 | 1 | 0 |
| `src/prototype/src/kernel/rules/match/motive_rule_emission.inc` | 59 | 9 | 50 |
| `src/prototype/src/kernel/type_declaration.c` | 181 | 134 | 47 |
| `src/prototype/src/kernel/typing/accepted_replay.inc` | 79 | 196 | -117 |
| `src/prototype/src/kernel/typing/candidate_publication.inc` | 238 | 107 | 131 |
| `src/prototype/src/kernel/typing/candidate_replay.inc` | 17 | 2 | 15 |
| `src/prototype/src/kernel/typing/classifier_solver.inc` | 59 | 10 | 49 |
| `src/prototype/src/kernel/typing/conversion.inc` | 35 | 13 | 22 |
| `src/prototype/src/kernel/typing/judgement_db.inc` | 1 | 0 | 1 |
| `src/prototype/src/kernel/universe.c` | 108 | 3 | 105 |
| `src/prototype/tests/checks/cbpv_boundary_check.c` | 3 | 0 | 3 |
| `src/prototype/tests/checks/context_category_check.c` | 19 | 0 | 19 |
| `src/prototype/tests/checks/conversion_result_check.c` | 3 | 0 | 3 |
| `src/prototype/tests/checks/conversion_scope_check.c` | 3 | 0 | 3 |
| `src/prototype/tests/checks/core_view_representation_check.c` | 71 | 9 | 62 |
| `src/prototype/tests/checks/hott/adt_identity.inc` | 2 | 1 | 1 |
| `src/prototype/tests/checks/hott/forgery.inc` | 50 | 0 | 50 |
| `src/prototype/tests/checks/hott/test_support.inc` | 28 | 10 | 18 |
| `src/prototype/tests/checks/judgement_transaction_check.c` | 106 | 0 | 106 |
| `src/prototype/tests/checks/lifted_ih_runtime_check.c` | 3 | 0 | 3 |
| `src/prototype/tests/checks/shared_term_reindex_check.c` | 3 | 0 | 3 |
| `src/prototype/tests/checks/spec_enum_check.c` | 1 | 1 | 0 |
| `src/prototype/tests/checks/universe_defeq_check.c` | 43 | 0 | 43 |
| `src/prototype/tests/checks/whnf_profile_cache_check.c` | 61 | 1 | 60 |
| `src/prototype/tests/integration/test_artifact_flow.sh` | 105 | 19 | 86 |
| `src/prototype/tests/integration/test_computation_block_sequence.sh` | 2 | 2 | 0 |
| `src/prototype/tests/integration/test_constraint_authority.sh` | 50 | 7 | 43 |
| `src/prototype/tests/integration/test_definition_block.sh` | 1 | 1 | 0 |
| `src/prototype/tests/integration/test_dependent_match_refinement.sh` | 2 | 2 | 0 |
| `src/prototype/tests/integration/test_explicit_index_family_surface.sh` | 2 | 2 | 0 |
| `src/prototype/tests/integration/test_judgement_transaction.sh` | 27 | 0 | 27 |
| `src/prototype/tests/integration/test_p0_certificate_boundary.sh` | 24 | 73 | -49 |
| `src/prototype/tests/integration/test_reader_session_storage.sh` | 26 | 0 | 26 |
| `src/prototype/tests/integration/test_spec_consistency.sh` | 3 | 1 | 2 |
| `src/prototype/tests/integration/test_type_capability_authority.sh` | 41 | 0 | 41 |
