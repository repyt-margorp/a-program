# Current Authority and Semantic Duplication Audit Report

Date: 2026-08-24

Status: complete; audit only; no implementation files changed

Repository baseline:

- branch: `main`;
- commit: `14b51b16019f049ec54c0dced1320012f1de3d1d`;
- artifact format: v84;
- implementation root: `src/prototype/`;
- implementation LOC, excluding specs and tests: 154,253; and
- full integration suite: 42/42 passed in 18.685 seconds of shell wall time.

This report completes the audit defined by
`2026-08-24T17-57-04-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-COMPREHENSIVE-AUDIT-PLAN.md`.

## 1. Executive Conclusion

The current system is not suffering from one general problem that can be fixed
by merging all databases. Most of its major layers represent different
semantic questions and must remain separate.

The following separations are sound and should be preserved:

- context-free `TermDB` and context-bearing `TypedOccurrenceGraph`;
- Context objects and Substitution morphisms;
- Proposition identity, accepted Claim, and Derivation provenance;
- constructor telescope and Pi type;
- DefEq, parametricity relation, and object Identity;
- operation request and ordinary APP;
- Constraint, accepted Claim, and residual Verification Obligation; and
- semantic schema, structural representation identity, and diagnostic
  readback.

The serious remaining problems occur at phase transitions, not at those
theoretical boundaries:

1. Constraint lifecycle is centralized, but classifier/effect solutions are
   still materialized into several mutable projections.
2. Universe Claims are accepted before global constraints are solved, while
   artifact replay trusts serialized Universe solutions without recomputing
   them.
3. Pure-type normalization depends on type-schema state, but its cache key does
   not contain the type-schema revision.
4. TypeDeclaration capability boundaries are documented but not enforced by
   narrow APIs.
5. Accepted replay and the driver retain substantial physical duplication even
   where semantic authority is already correct.

No evidence supports replacing the current typed stores with one untyped arena,
splitting APP/LAMBDA into Value and Computation tags, or encoding MATCH/fold
away. Those changes would hide the relevant rules rather than remove duplicate
meaning.

## 2. Reproduction Baseline

The full suite was executed with:

```sh
TIMEFORMAT='elapsed=%3R user=%3U sys=%3S'
time make -f src/prototype/Makefile test-integration
```

Result:

```text
42/42 passed
suite-reported elapsed: 17.939 s
shell elapsed: 18.685 s
user: 12.830 s
sys: 6.070 s
```

The slowest reported groups were:

| Test group | Time |
| --- | ---: |
| Function graph | 3.647 s |
| Artifact flow | 2.682 s |
| HOTT | 1.780 s |
| IF8 | 1.400 s |
| CBPV surface | 1.307 s |

A green suite establishes the baseline. It does not establish single
authority, because several current tests intentionally inspect source shape or
validate only one direction of a projection.

## 3. Semantic Owner Ledger

| Fact | Current authority | Other records | Verdict |
| --- | --- | --- | --- |
| Context-free computation identity | `TermDB` | AST syntax, typed occurrence | Sound separation |
| Source occurrence classifier | solver result during elaboration; frozen occurrence after finalization | hints, effect materializations | Authority transition is not explicit enough |
| Classifier/effect equation lifecycle | `operation_constraint_db` | solution cells and metadata | Lifecycle is centralized |
| Classifier solution | `operation_solver_solution.classifier` in practice | hints, occurrence classifier, rewritten classifier Terms | Multiple mutable projections remain |
| Effect meta solution | `operation_effect_solver.metas[]` in practice | solution-cell row, hints, binder classifier | Multiple mutable projections remain |
| Resource usage during solving | per-occurrence solution and usage arena | frozen Proposition usage | Legitimate phase transition, but needs an explicit freeze contract |
| Context object | `ContextDB` | Context certificates | Sound separation |
| Context morphism | `SubstitutionDB` | weakening/reindex Derivations | Sound separation |
| Type semantic schema | `type_declarations.semantic_schema` | operational type-former projection | Sound only while projection is synchronized |
| Structural type representation | `representation_db` | representation IDs in Terms/artifacts | Persistent semantic identity, not a cache |
| Constructor readback | `type_declarations.readback` | artifact readback fields | Presentation only |
| Constructor callable classifier cache | constructor classifier cache | exported classifier evidence | Rebuildable in memory; serialized field is not a cache |
| Proposition identity | `JudgementDB.propositions` | premise views | Sound separation |
| Accepted fact | `JudgementDB.claims` | artifact roots | Sound separation |
| Proof provenance | `JudgementDB.derivations` | accepted replay views | Sound separation |
| Definitional equality | reduction/conversion rules | object Identity witnesses | Sound separation |
| Universe inequalities | `UniverseDB.constraints` after collection | Derivation kind and serialized constraints | Missing accepted phase certificate |
| Universe numerical solution | `UniverseDB.levels` produced by solver | serialized levels and solved flag | Artifact replay does not verify authority |
| Object Identity | Identity Terms, Claims, Derivations, and action certificates | parametricity action substrate | Sound separation, incomplete higher fragment |
| Residual effect/computation condition | `VerificationDB` after explicit transfer | compile diagnostic snapshot | Sound if snapshot stays non-authoritative |
| Qualified external identity | `prototype_qualified_name` | export/dependency records | Current namespace retention is sound |

## 4. Confirmed Findings

### F01: Universe artifact replay accepts an unsatisfiable solution

Priority: **P1**

Classification: `WIRE_EVIDENCE` accepted without rebuilding its authority

Relevant code:

- `src/prototype/src/kernel/typing/accepted_replay.inc:7518`;
- `src/prototype/src/frontend/universe_collection.c:718`;
- `src/prototype/src/frontend/universe_collection.c:729`;
- `src/prototype/src/artifact/wire_v84.c:3138`;
- `src/prototype/src/driver/read_file.c:714`; and
- `src/prototype/src/driver/read_file.c:4383`.

Source compilation collects constraints from accepted Claims, solves them, and
then validates provenance. Artifact replay only reads the serialized levels,
constraints, and `solved` flag and validates that each provenance Claim still
exists. It does not rerun the Universe solver and does not reconstruct the
inequality from the cited Claim.

The problem was reproduced without modifying source code:

1. Compile `examples/01_bool.p` to a v84 artifact.
2. Change the first Universe constraint from `u + 1 <= v` to the positive
   self-cycle `u + 1 <= u`.
3. Read the modified artifact with `--read-graph`.

Observed result:

```text
universe_constraint 0 1 1 1 ...
read result: accepted
```

Changing a serialized level value by 100 and changing `solved 1` to `solved 0`
were also accepted.

The numerical level values are currently used mainly for diagnostics, so the
tampered number alone is not a direct kernel exploit. The unsatisfiable
constraint is more important: replay accepts a persistence image whose claimed
Universe closure cannot be produced by the authoritative solver.

Required correction:

- rebuild Universe constraints from accepted Claims and schema during artifact
  replay;
- solve that rebuilt graph;
- compare serialized constraints/levels only if they remain wire evidence; and
- make cumulativity/formation Derivations reference the exact solved constraint
  or a Universe certificate.

Do not add cumulativity to DefEq and do not trust the serialized `solved` bit.

Required tests:

- reject a positive Universe self-cycle;
- reject a wrong serialized level assignment;
- reject `solved 0` for an artifact whose roots require solved Universe
  evidence;
- reject a constraint whose endpoints/offset do not follow from its provenance
  Claim; and
- accept an artifact after independently rebuilding the same solution.

### F02: Universe cumulativity is an unconditional Claim before solver closure

Priority: **P1**

Classification: missing phase-transition certificate

`UNIVERSE_CUMULATIVITY` introduction and accepted replay validate that the
subject and classifier are Universe variables. The numerical inequality is
collected later from the accepted Claim and solved globally. Thus the Claim is
stored as accepted before the condition that justifies it is closed.

This differs from a normal accepted Derivation premise. Its real premise lives
in a later database and is linked only in the reverse direction: the Universe
constraint cites the Claim.

Required correction options are:

1. keep the relation provisional until Universe solving succeeds, then publish
   the Claim with a Universe certificate; or
2. publish it as explicitly conditional evidence whose condition is a stable
   Universe constraint/certificate ID.

The first option is simpler for the current closed compile pipeline. The second
is appropriate only if unresolved Universe conditions are intentionally allowed
across artifacts.

### F03: Classifier/effect solved values have no single explicit authority

Priority: **P1**

Classification: `DUPLICATE_AUTHORITY` at solution materialization

Relevant code:

- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc:347`;
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc:398`;
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc:445`;
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc:547`; and
- `src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc:442`.

`operation_constraint_db` correctly owns equation lifecycle, reason, evidence,
dependencies, and scheduling. It should not automatically own one result Term
per equation, because several equations can constrain one metavariable.

The remaining problem is that solved values are held or substituted into all of
the following:

- `operation_solver_solution.classifier`;
- `operation_solver_solution.effect_row`;
- `operation_effect_row_meta.solution_row`;
- `classifier_hints[owner]`;
- the occurrence binder classifier; and
- classifier Terms rewritten by bound-variable substitution.

`operation_effect_materialize_solution()` mutates four of these locations in
one pass. This keeps them synchronized procedurally, but it does not establish
one owner. A missed writer or retry path can leave different answers to the
same semantic question.

The correct unification is not “put `result_term` into every Constraint.” The
correct model is:

```text
ConstraintDB
    owns equations, lifecycle, dependencies, and evidence

TypedMetaSolutionDB
    owns one solution per classifier/effect/usage metavariable

Freeze/Reification
    reads both authorities once and creates immutable TypedOccurrence and
    Proposition projections
```

Classifier metas and effect-row metas may use different typed payloads and
solvers while sharing lifecycle and lookup machinery. No compatibility copy of
the old solution cell should remain.

The current `test_constraint_authority.sh` proves that lifecycle fields were
removed from solution cells. It explicitly allows classifier/effect results in
those cells, so it does not prove single result authority.

Required tests:

- fail when a meta solution and frozen occurrence disagree;
- fail when an effect equation is solved but its meta is unresolved;
- fail when a meta has two non-convertible solutions;
- prove that deleting all hints/projections before freeze does not change the
  accepted artifact; and
- compare counters before/after so authority centralization does not introduce
  whole-graph scans.

### F04: Pure-type normalization cache omits type-schema revision

Priority: **P1**

Classification: incomplete `REBUILDABLE_CACHE` validity key

Relevant code:

- `src/prototype/include/a_program/core/term.h:353`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc:1014`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc:1102`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc:1396`;
- `src/prototype/src/kernel/type_declaration.c:16`; and
- `src/prototype/src/kernel/context.c:1202`.

The normalization cache key contains:

- Term ID;
- reduction profile; and
- `TermDB.normalization_graph_revision`.

Pure-type evaluation also reads:

- constructor counts;
- parameter counts;
- index counts; and
- type-instance/schema information.

`TypeDeclarationDB` has a separate `semantic_revision`, and the reindex cache
already includes that revision. The normalization cache does not.

Some type-former mutations happen to update a Term projection and invalidate
the Term cache. That procedural coupling is not a complete validity rule for
all schema reads. In particular, a cache must remain correct even when a
semantic-schema mutation produces no otherwise relevant Term mutation.

Required correction:

- add the semantic-schema revision to cache entries used by profiles that read
  the schema; or
- keep separate caches for schema-independent and schema-dependent profiles.

The latter avoids reducing hit rates for `CORE_WHNF` and ordinary computation
WHNF.

Required test:

- normalize a schema-dependent neutral Match or partial type application;
- extend/finalize the relevant schema without changing the source Term;
- normalize again; and
- require the second result to be recomputed under the new schema revision.

### F05: TypeDeclaration roles are separated in storage but not in capability

Priority: **P2**

Classification: broad mutable capability and residual physical mixing

Relevant code:

- `src/prototype/include/a_program/kernel/type_declaration.h:134`;
- `src/prototype/include/a_program/kernel/type_declaration.h:161`;
- `src/prototype/src/kernel/type_declaration.c:552`;
- `src/prototype/src/kernel/type_declaration.c:610`; and
- `src/prototype/src/kernel/type_declaration.c:666`.

Current storage already distinguishes:

- semantic schema;
- readback;
- structural representation;
- constructor classifier cache; and
- statistics.

The old claim that flat field metadata remains constructor semantic authority
is not supported by current kernel code. Constructor semantics use the
`parameter_context -> field_context` telescope and `result_classifier`.

Residual problems are:

- many functions receive a broad mutable `prototype_type_declaration_db*`;
- `first_parameter`, a readback-array offset, is stored inside the semantic
  declaration record;
- readback parameter count is used to maintain semantic declaration layout;
- linker/publication code directly reaches across all sub-databases; and
- the artifact field `curried_classifier_cache` names persistent interface
  evidence as if it were an optional cache.

Required correction:

- move readback offsets to `prototype_type_readback_db`;
- introduce narrow semantic-schema, representation, readback, and classifier
  cache views/builders;
- rename serialized constructor classifier evidence; and
- retain the aggregate only as explicit program storage composition, not as the
  default kernel capability.

This is an API and ownership correction. Do not replace the constructor
telescope with a Pi type and do not derive dependent schema from readback.

### F06: Accepted replay still reconstructs candidate-shaped adapters

Priority: **P2**

Classification: `PHYSICAL_DUPLICATION`

Relevant code:

- `src/prototype/src/kernel/typing/accepted_replay.inc:1960`;
- local candidate premise arrays at lines 2046, 2088, 2127, 2233, 2335, 2752,
  4004, 4047, and 5274; and
- `src/prototype/include/a_program/kernel/judgement/types.h`.

The accepted replay boundary is substantially improved: immutable rule and
premise views exist, and no current unsafe const cast was found in the audited
path. Several validators still allocate and populate
`prototype_judgement_candidate_premise` arrays as adapters.

Required correction:

- make shared theorem-checking helpers consume immutable premise views;
- keep candidate builders confined to proof search/publication; and
- preserve separate validators for APP, MATCH, IH, and computation fold.

The proof rules must not be collapsed merely to reduce lines.

### F07: Driver storage duplicates program/provider/artifact bundles

Priority: **P2**

Classification: `PHYSICAL_DUPLICATION`

Relevant code:

- `src/prototype/src/driver/read_file.c:92`;
- `src/prototype/src/driver/read_file.c:129`;
- `src/prototype/src/driver/read_file.c:160`;
- `src/prototype/src/driver/read_file.c:246`;
- `src/prototype/src/driver/program_storage.c`; and
- `src/prototype/src/support/storage.c`.

`program_storage` provides an explicit reusable owner bundle, but
`read_file.c` still declares parallel global static arrays for local, provider,
and artifact graphs. This duplicates initialization and ownership code and
makes the command driver non-reentrant.

Required correction:

- instantiate explicit storage bundles for local, provider, linked, and
  artifact roles;
- retain typed stores inside each bundle;
- introduce role-specific initialization profiles where capacities differ; and
- remove the parallel global arrays in one migration, without a compatibility
  facade.

Do not replace typed storage with an untyped mega-arena.

### F08: HOTT action reuse relies on an unstated freeze invariant

Priority: **P2**

Classification: cache/phase contract is implicit

Relevant code:

- `src/prototype/include/a_program/identity/types.h:51`;
- `src/prototype/include/a_program/identity/types.h:321`;
- `src/prototype/src/identity/action_certificate_validation.inc:195`;
- `src/prototype/src/identity/action_certificate_validation.inc:1341`; and
- `src/prototype/src/identity/action_execution.inc:25`.

An action result records a Term graph revision and calculus fingerprint, but
result lookup is by request ID only. Existing results are returned without an
epoch check. The action and certificate validators can read type declarations,
Contexts, substitutions, occurrences, and Claims.

Append-only Terms and immutable Claims make ordinary Term growth harmless.
Schema or Context mutation would not be harmless. Current pipeline ordering
appears to execute these actions after the relevant source evidence has been
accepted, but that freeze invariant is not represented in the action key or
kernel view.

Required correction:

- formally freeze the semantic stores before HOTT action execution and assert
  their epochs; or
- include the relevant semantic epochs in the action request/result validity
  contract.

Do not merge parametricity goals with object Identity requests. Their shared
traversal substrate may be factored, but their certificates establish different
theorems.

### F09: Diagnostic effect snapshots outlive their useful boundary

Priority: **P3**

Classification: oversized `DIAGNOSTIC_SNAPSHOT`

`compile_metadata.effect_constraints[]` is copied from `ConstraintDB`. Current
code no longer reads it to create Verification obligations and does not
serialize it. That is semantically safe.

It is still copied, relocated, stored in driver bundles, and carried through
metadata append paths. If diagnostics need only counts and selected residuals,
the full copied table should be removed or moved to an optional diagnostics
object.

The existing comment still says artifact v82 although the current format is
v84. This is documentation debt, not semantic authority.

### F10: Index rebuild failures are inconsistently handled

Priority: **P2**

Classification: transaction/error-handling duplication

`prototype_judgement_db_rebuild_index()` is checked at artifact, link, replay,
and publication boundaries, but rollback paths in
`formation_early.inc:32` and parts of `candidate_publication.inc` explicitly
discard its return value.

Rollback should either be infallible by construction or return an error through
one shared transaction API. Repeated manual count restoration followed by a
full rebuild is both a performance and correctness risk.

Required correction:

- define one typed Judgement transaction mark/rollback operation;
- make rollback restore all arena counts and rebuild indexes exactly once; and
- make failure propagation uniform.

This helper can be shared without combining Proposition, Claim, and Derivation.

## 5. Resolved or Unsupported Earlier Suspicions

### 5.1 Weakening and projection are now connected correctly

`CONTEXT_WEAKEN` Derivations carry
`PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION` and the exact Substitution ID.
Accepted replay checks that it is a projection path and reindexes the premise.

Verdict: `LEGITIMATE_DISTINCTION`.

The projection is a Context morphism. Weakening is a proof that a proposition
has been transported along that morphism. They should not be one record.

### 5.2 Constructor schema authority is graph-level

Current semantic constructors store parameter Context, field Context, and
result classifier. Readback field/result expressions are attached separately.

Verdict: the old semantic-authority split is `RESOLVED`; broad capability is
still an issue under F05.

### 5.3 Multi-clause computation folds are implemented end to end

The reader accepts a return clause and multiple operation clauses. Core Term,
runtime reduction, kernel constraints, premise generation, and accepted replay
all iterate over `clause_count`.

Verdict: the old “Core supports multiple clauses but typing accepts one” finding
is `RESOLVED`.

### 5.4 External Term references retain namespace identity

`PROTOTYPE_TERM_EXTERNAL_REF` carries `prototype_qualified_name`, and
canonicalization, reduction, publication, relocation, and dependency lookup
compare both namespace and name.

Verdict: the old namespace-loss finding is `RESOLVED`.

### 5.5 Sparse artifact slots use presence checks

Current v84 graph validation uses typed `artifact_read_*_present` helpers for
Terms, types, frames, and cases rather than range checks alone.

Verdict: the old sparse-hole finding is `RESOLVED` for audited v84 paths.

### 5.6 Universe variable DefEq compares level identity

The old rule that treated all Universe variables as convertible is no longer
present. Cumulativity remains a separate proof/constraint issue under F01 and
F02.

Verdict: the DefEq part is `RESOLVED`.

## 6. Required Non-Unifications

| Concepts | Decision | Reason |
| --- | --- | --- |
| Term and TypedOccurrence | Preserve | Computation identity and source/context occurrence differ |
| Value and Computation APP/LAMBDA tags | Do not split | Polarity belongs to typing/CBPV classifiers over common graph structure |
| Context and Substitution | Preserve | CwF objects and morphisms differ |
| Weakening and projection | Preserve with reference | Derivation and morphism establish different facts |
| Proposition, Claim, Derivation | Preserve | Statement, acceptance, and provenance differ |
| Constraint and Claim | Preserve | Search equation and accepted theorem differ |
| Constraint and Verification Obligation | Preserve with transfer | Compile-time solving and exported residual differ |
| Pi and telescope | Preserve | Callable type former and schema Context differ |
| Structural representation and TypeView | Preserve | Shared shape and nominal operation identity differ |
| DefEq, relation, Identity | Preserve | Conversion, relation preservation, and object evidence differ |
| APP, MATCH, IH, fold proof rules | Preserve | They validate different introduction/elimination theorems |
| OP_REQUEST and APP | Preserve | Handler free-algebra operation identity must remain observable |
| Pure primitive and effect operation | Preserve | Host implementation does not imply effect semantics |
| Wire parsing and semantic replay | Preserve | Bounds safety and theorem validation differ |
| TermDB and all semantic stores | Do not merge into one arena | Typed stores have different identity and rollback rules |

## 7. Physical Consolidation Candidates

These changes can reduce duplication without changing theory.

| Candidate | Common mechanism | Must remain explicit |
| --- | --- | --- |
| Typed meta solution store | meta lookup, state transition, freeze | classifier/effect/usage payload algorithms |
| Judgement transaction | mark, rollback, index restoration | Proposition/Claim/Derivation identities |
| Accepted premise views | immutable premise traversal | each proof-rule validator |
| Program storage profiles | typed allocation and initialization | local/provider/artifact roles |
| TypeDeclaration capabilities | narrow read/build views | schema/readback/representation/cache roles |
| Graph action traversal | child enumeration and memo protocol | identity/parametricity certificate meaning |
| Generated function package builder | schema/package assembly | first-order/higher-order validation rules |
| Diagnostic collection | counters and optional snapshots | solver and acceptance inputs |

The large files should be split only after these boundaries exist. Moving code
between `.inc` files without changing capabilities or ownership does not solve
duplication.

## 8. Audit by Planned Section

| Phase | Result |
| --- | --- |
| AU0 | Baseline, stores, roots, LOC, and test timing recorded |
| AU1 | Term/occurrence split is sound; no Value/Computation graph duplication found |
| AU2 | Lifecycle centralized; meta-result authority remains split (F03) |
| AU3 | Context/Substitution and weakening/projection are correctly linked |
| AU4 | Semantic schema is authoritative; capability split remains (F05) |
| AU5 | Proof-layer distinctions are sound; accepted adapters remain (F06) |
| AU6 | Shared evaluator/profile design is sound; cache key incomplete (F04) |
| AU7 | Request/fold distinctions are sound; multi-clause fold is complete; snapshot remains (F09) |
| AU8 | DefEq is improved; Universe phase and replay certificates are incomplete (F01/F02) |
| AU9 | Relation and Identity are separated; action freeze contract is implicit (F08) |
| AU10 | Generated graph/package roles are distinct; helper consolidation is physical only |
| AU11 | v84 boundaries are generally strong; Universe replay is the major exception |
| AU12 | Qualified names and TypeViews preserve intended distinctions |
| AU13 | Shared reserve helper exists; driver storage and rollback remain duplicated (F07/F10) |
| AU14 | Full suite is green; focused authority and cache tests are missing |

## 9. Recommended Implementation Order

### Stage A: Universe evidence closure

Fix F01 and F02 together.

1. Define the exact Universe constraint/certificate represented by each
   formation and cumulativity Derivation.
2. Delay or condition Claim publication until solver closure.
3. Rebuild and solve Universe constraints during artifact replay.
4. Compare or remove serialized numerical solutions.
5. Bump the artifact version because accepted evidence semantics change.

### Stage B: Typed metavariable solution authority

Fix F03 without putting one result on every equation.

1. Define stable classifier, effect-row, and usage meta IDs.
2. Move all solved values into typed meta solution stores.
3. Make ConstraintDB refer to metas and own equation lifecycle only.
4. Remove mutable solution copies and classifier hints.
5. Perform one checked freeze into TypedOccurrence and Proposition.
6. Measure IF8, totality, and function-graph compile time throughout.

### Stage C: Cache and phase epochs

Fix F04 and formalize F08.

1. Add schema revision to schema-dependent normalization entries.
2. Keep schema-independent profiles on the cheaper key.
3. Freeze semantic stores before HOTT action execution or key actions by their
   semantic epochs.
4. Add stale-cache and stale-action tests.

### Stage D: Capability and physical consolidation

Fix F05-F10 after semantic authorities are stable.

1. Split TypeDeclaration capabilities and move readback offsets.
2. Convert accepted replay helpers to immutable premise views.
3. Introduce typed Judgement transactions.
4. Migrate `read_file.c` to explicit storage bundles.
5. Remove or isolate full diagnostic effect snapshots.

Each stage should receive its own implementation/measurement plan before code
changes begin. No compatibility fields or remap layer should be retained merely
to reduce migration size.

## 10. Artifact Impact

Stage A changes what it means to accept Universe evidence from an artifact and
therefore requires an artifact version bump.

Stage B requires a version bump only if serialized TypedOccurrence, Claim, or
conditional evidence changes. Compiler-local meta stores must not be
serialized.

Stage C does not require a wire change unless semantic epochs become persistent
certificate fields.

Stage D should preserve wire meaning. Renaming
`curried_classifier_cache` in the wire schema is still a format change even if
the underlying Term is unchanged.

## 11. Completion Statement

The audit found no justification for collapsing the current theory layers into
fewer semantic databases. The useful unifications are narrower:

- one authority for each metavariable solution;
- one explicit closure certificate at each solver-to-Claim boundary;
- complete epoch keys for derived computation;
- narrow capabilities over already separated stores; and
- shared typed transaction/storage/view machinery.

The most urgent work is Universe artifact closure, followed by classifier and
effect meta authority. Physical code reduction should follow those semantic
corrections, not lead them.
