# Current Code Structure and Duplication Audit

Date: 2026-08-26 13:16:55 JST

Status: complete static audit; no implementation refactoring is authorized by
this document

Baseline commit: `fd546e1` (`main`, equal to `origin/main` at audit start)

Follow-up implementation plan:
`doc/2026-08-26T13-55-23-CODE-STRUCTURE-CONSOLIDATION-REFACTOR-IMPLEMENTATION-PLAN.md`

## 1. Objective

This audit identifies code whose structure is unnecessarily duplicated,
semantically ambiguous, or physically disorganized after the PR22 checked-Core
implementation. It records both findings and rejected consolidation ideas.

The audit does not treat file size or repeated syntax as sufficient evidence.
A finding requires a concrete shared semantic fact, transform, invariant, or
algorithm whose ownership or implementation is duplicated without a necessary
boundary.

## 2. Project Invariants

The audit preserves these A Program design constraints:

- Core `TermDB` records untyped computation structure. Typing, proof context,
  and source occurrence are not folded into Core term identity.
- Surface `::` is a later expectation check, not bidirectional elaboration input.
- Compiler and runtime are distinguished by which computations are performed
  ahead of time, not by assigning unrelated meanings to the same Core node.
- Accepted evidence and an independent checker have distinct trust roles even
  when they validate the same calculus rule.
- `ContextDB`, `SubstitutionDB`, `JudgementDB`, `TypeDeclarationDB`, and artifact
  stores remain distinct when they own different mathematical objects.
- Migration size is not a reason to retain a second semantic authority.
- Separate encodings of `APP`, `MATCH`, induction-hypothesis elimination, and
  computation fold remain explicit kernel rules. Similar control flow alone is
  not grounds for merging their theorem checks.

## 3. Classification

Every candidate receives one of these verdicts:

| Verdict | Meaning |
| --- | --- |
| `SEMANTIC_AUTHORITY_DUPLICATION` | Two mutable records can independently decide the same accepted fact. |
| `ALGORITHM_DUPLICATION` | The same pure transform or invariant is reimplemented and may drift. |
| `PHYSICAL_PLUMBING_DUPLICATION` | Repeated allocation, traversal, transaction, or diagnostic machinery has no semantic need to differ. |
| `MIGRATION_DUPLICATION` | Old and new representations coexist temporarily with an explicit retirement condition. |
| `LEGITIMATE_BOUNDARY` | Similar data or code validates different objects or serves a required trust/serialization boundary. |
| `ORGANIZATION_ONLY` | Placement or file size obscures ownership, but no duplicate authority or algorithm is established. |
| `OBSOLETE_OR_DEAD` | A path is no longer required by any supported producer, checker, artifact, or test boundary. |

## 4. Evidence Standard

A confirmed finding must identify:

1. the semantic fact or algorithm;
2. every writer or implementation site;
3. every acceptance reader;
4. the synchronization or divergence path;
5. why the separation is not mathematically or operationally required;
6. the proposed owner or shared primitive;
7. permanent tests needed before consolidation.

No finding is confirmed from name similarity, line count, or a large `switch`
alone.

## 5. Baseline

- Audited implementation tree: `src/prototype/`.
- Audited C, header, include-fragment, lexer, and parser volume: 198,550 lines.
- Largest implementation units at baseline:

| File | Lines |
| --- | ---: |
| `src/frontend/lowering/graph_construction.inc` | 12,441 |
| `src/frontend/function_graph.c` | 9,046 |
| `src/kernel/typing/accepted_replay.inc` | 7,903 |
| `src/checker/session.c` | 7,271 |
| `src/identity/object_term_action.inc` | 6,159 |
| `src/frontend/lowering/context_and_type_lowering.inc` | 5,887 |
| `src/core/term/evaluation_and_conversion.inc` | 5,027 |
| `src/frontend/lowering/constraint/evidence_and_freeze.inc` | 4,368 |
| `src/driver/read_file.c` | 4,258 |
| `src/frontend/lowering/constraint/branch_refinement_and_motives.inc` | 4,254 |
| `src/artifact/wire_v86.c` | 3,898 |
| `src/kernel/typing/conversion.inc` | 3,885 |
| `src/identity/identity_computation.inc` | 3,565 |
| `src/checker/module.c` | 3,392 |

Large files are investigation leads, not findings.

## 6. Audit Plan and Progress

| Phase | Scope | Status |
| --- | --- | --- |
| D0 | Baseline, prior audits, and PR22 delta | Complete |
| D1 | Core Term, traversal, substitution, normalization, conversion | Complete |
| D2 | Context, substitution, binding identity, and resource usage | Complete |
| D3 | Lowering, typed occurrences, constraints, solvers, and freeze | Complete |
| D4 | Type declarations, indexed families, and function graphs | Complete |
| D5 | Proposition, Claim, Derivation, replay, and independent checking | Complete |
| D6 | CBPV, effects, host operations, folds, and residual computation | Complete |
| D7 | Identity, parametricity, dimensions, and HOTT actions | Complete |
| D8 | Artifact v86/v87, interface, publication, relocation, and import | Complete |
| D9 | Producer goals, effort, resume, merge, parallelism, and incrementality | Complete |
| D10 | Driver, diagnostics, tests, common C machinery, and documentation | Complete |
| D11 | Integrated owner ledger, priorities, and implementation sequence | Complete |

## 7. Investigation Method

For each phase:

- enumerate public stores and APIs;
- locate all writers before reviewing readers;
- trace acceptance decisions backward to their authority;
- compare recursive term/context/substitution traversals by semantics;
- distinguish producer evidence generation from checker validation;
- distinguish persistent wire duplication from in-memory semantic duplication;
- check whether caches carry source revision and phase identity;
- record negative decisions where apparent duplication must remain;
- record a test boundary and retirement condition for every proposed merge.

## 8. Semantic Owner Ledger

| Fact or object | Current authority | Other records | Current verdict | Evidence |
| --- | --- | --- | --- | --- |
| Core term identity | `prototype_term_db.terms` plus canonical intern index | typed occurrences, wire records | One authority; projections remain distinct | `core/term.h:677-737` |
| Typed occurrence classifier | frozen typed meta solution projected to `prototype_typed_occurrence` | constraints and diagnostic evidence | One accepted projection after freeze | 2026-08-24 M1 completion record |
| Effect-row solution | typed effect meta store before freeze | equations, frozen occurrence rows | One solver authority plus checked projection | 2026-08-24 M1/D1 completion record |
| Context extension | `ContextDB` | checked semantic context projection | `LEGITIMATE_BOUNDARY` | `checker/module.h:50-67` |
| Substitution morphism | `SubstitutionDB` | checked semantic substitution projection | `LEGITIMATE_BOUNDARY` | `checker/module.h:69-90` |
| Accepted proposition | immutable Proposition plus accepted Claim | checker-local reconstructed judgement | `LEGITIMATE_BOUNDARY` | accepted replay versus `checker/session.c` |
| Accepted derivation | accepted Derivation DAG | no checked-Core copy | One proof-evidence authority | PR22 authority schema section 2 |
| Type declaration schema | `prototype_type_semantic_schema_db` | readback, representation index, classifier cache | One semantic authority; capability split is mostly complete | `kernel/type_schema_view.h:35-52` |
| Universe closure | reconstructed producer closure certificate | checker-local reconstructed Universe solution | `LEGITIMATE_BOUNDARY` | `frontend/universe_collection.c:696-1030`, `checker/universe.c:463-524` |
| Checked module capability | opaque result of independent checker completion | serialized semantic content does not carry capability | One process-local authority | `checker/container.c:1386-1460` |
| Persistent artifact authority | v87 semantic sections for admitted checked fragment; v86 migration oracle for deferred roots | optional producer/debug sections | `MIGRATION_DUPLICATION` | PR22 EC6/EC8 |
| Producer resume state | typed producer session/capsule | checked module excludes it from authority | One producer-local authority per producer kind | PR22 EC9-EC14 |

## 9. Finding Ledger

| ID | Candidate | Verdict | Priority | Status |
| --- | --- | --- | --- | --- |
| D-001 | Term reference-field schema is repeated across traversal, validation, relocation, and wire code | `ALGORITHM_DUPLICATION` | P1 | Confirmed |
| D-002 | Core has no immutable structural graph views reusable by the independent checker | `ALGORITHM_DUPLICATION` | P1 | Confirmed |
| D-003 | Merge producer and checked module set both serialize, sort, and deduplicate the same modules | `PHYSICAL_PLUMBING_DUPLICATION` | P1 | Confirmed |
| D-004 | v87 has executable wire code but no standalone schema; docs do not clearly distinguish v86 proof artifacts from v87 checked containers | `ORGANIZATION_ONLY` | P1 | Resolved by CR1 on 2026-08-27 |
| D-005 | Canonical Term hashing and type representation fingerprinting duplicate alpha-aware traversal machinery | `ALGORITHM_DUPLICATION` | P2 | Confirmed with constrained remedy |
| D-006 | checked-Core semantic projection field inventory is repeated in projection, validation, closure, relocation, and serialization | `ALGORITHM_DUPLICATION` | P2 | Confirmed |
| D-007 | `checker/session.c` combines unrelated checker subsystems in one 7,271-line unit | `ORGANIZATION_ONLY` | P2 | Confirmed |
| D-008 | Core, lowering, kernel replay, and HOTT use large include forests as implicit internal APIs | `ORGANIZATION_ONLY` | P2 | Confirmed |
| D-009 | Type schema action classifier still requires the aggregate mutable TypeDeclarationDB | `ORGANIZATION_ONLY` | P2 | Confirmed capability leak, not duplicate authority |
| D-010 | Resource usage producer/checker implementations look similar | `LEGITIMATE_BOUNDARY` | None | Rejected consolidation of acceptance logic |
| D-011 | Universe producer/checker closure implementations look similar | `LEGITIMATE_BOUNDARY` | None | Rejected consolidation of acceptance logic |
| D-012 | Accepted replay and checked-Core checker validate overlapping calculus rules | `LEGITIMATE_BOUNDARY` | None | Required independent validation |
| D-013 | Function Graph package generation and ordinary type declaration construction are separate | `LEGITIMATE_BOUNDARY` | None | No duplicate authority found |
| D-014 | Hash helpers use repeated FNV-style plumbing | `PHYSICAL_PLUMBING_DUPLICATION` | P3 | Low-risk helper candidate only |
| D-015 | Driver command implementation remains a 4,258-line mixed-responsibility unit | `ORGANIZATION_ONLY` | P3 | Confirmed; no authority defect |

## 10. Required Non-Unifications

The following are presumed legitimate until contrary evidence is established:

- Core term versus typed occurrence;
- Context object versus substitution morphism;
- Proposition versus Claim versus Derivation;
- evidence-producing kernel versus independent checked-Core checker;
- live in-memory graph versus persistent wire representation;
- current wire reader versus explicitly supported legacy reader;
- value/computation polarity as typing information versus a duplicated Core
  application graph;
- logical relation action versus object-language Identity evidence;
- distinct eliminator rules with distinct premises and conclusions.

## 11. Findings

### D-001: Term reference-field schema is not centralized

The same `prototype_term` tag and payload inventory is manually encoded in all
of the following places:

- computational child enumeration in
  `src/prototype/src/core/term/declarations.inc:703-950`;
- checked semantic graph validation in
  `src/prototype/src/checker/module.c:246-525`;
- checked closure marking in
  `src/prototype/src/checker/module.c:2285-2437`;
- checked relocation in
  `src/prototype/src/checker/module.c:2439-2589`;
- v87 write/read in `src/prototype/src/checker/container.c:533-830`;
- v86 read/write in `src/prototype/src/artifact/wire_v86.c` and
  `src/prototype/src/artifact/publication/wire_primitives.inc`; and
- multiple free-binding, canonicalization, substitution, usage, and conversion
  traversals.

This is not merely repeated syntax. Adding one Term tag or one reference field
requires several switches to agree about which payload words are Term IDs,
binding IDs, owners, symbols, slices, or scalars. A missed site can create an
accepted in-memory graph that is serialized, relocated, or closed differently.

The current `prototype_term_child_count()`/`prototype_term_child_at()` API is
not sufficient as the single schema. It describes computational children.
Storage closure has additional references. For example, a constructor owner is
not a computational child, but it is a semantic reference that closure marking
and relocation must preserve. Replacing every switch with the current child API
would therefore be wrong.

Required direction:

1. define one immutable Term record schema or typed reference visitor;
2. classify each field as computational child, owner/type reference, binding,
   IH scope, case/clause slice, symbol, literal, or scalar;
3. retain separate algorithms for evaluation, validation, relocation, hashing,
   and serialization, but derive their field inventory from that schema;
4. make exhaustive tag coverage a compile-time or permanent-test gate; and
5. test every tag with nontrivial reference IDs through closure, relocation,
   v87 round-trip, and checker validation.

This is the highest-value structural consolidation because it reduces semantic
drift without merging any calculus rule.

### D-002: Pure graph algorithms are coupled to mutable DB owners

Core exposes semantic data through `struct prototype_term_db`, which also owns
normalization caches, canonical intern indices, allocation cursors, and mutable
capacities (`src/prototype/include/a_program/core/term.h:677-737`). PR22 had to
introduce a separate immutable `prototype_semantic_term_graph_view`
(`src/prototype/include/a_program/checker/module.h:21-35`) so the independent
checker cannot reach producer runtime state.

Pure Core helpers such as child traversal, free-binding analysis, shape
comparison, and substitution-oriented comparison still accept a full
`prototype_term_db`. The checker therefore reimplements substantial graph
machinery in `src/prototype/src/checker/session.c`:

- substitution-path comparison at lines 1281-1575;
- substitution binding images at lines 1671-1703;
- free-binding inspection at lines 1705-1781;
- Pi decomposition at lines 1783-1825;
- alpha/beta-aware comparison at lines 1827-2090;
- Context binding/path operations at lines 953-1034 and 2761-2843; and
- effect-row traversal and comparison around lines 4352-4560 and 6360-6430.

The checker must remain independent of producer Claims, solutions, caches, and
normalizer acceptance. It does not need an independently handwritten notion of
where fields are stored or how an immutable substitution path is followed.

Required direction:

1. add Core-level read-only Term, Context, and Substitution structural views;
2. implement pure, non-allocating structural visitors over those views;
3. make mutable DB APIs thin wrappers over the same views;
4. let the checker reuse representation traversal and reindex decoding only;
5. keep checker-local typing, DefEq admission, resource acceptance, and
   capability minting separate; and
6. retain differential tests between producer replay and checker results.

This change must not give the checker access to normalization caches or accepted
evidence. The target is shared mathematical representation plumbing, not shared
trust authority.

### D-003: Merge canonicalization serializes each module twice

`src/prototype/src/producer/merge.c:100-181` serializes each checked module into
a temporary file, owns the bytes, sorts them, and deduplicates equal images.
After pairwise conflict work, lines 268-282 call
`prototype_checked_module_set_create()`.

That function repeats the same operation in
`src/prototype/src/checker/module_set.c:28-80` and lines 189-252: it serializes
the same modules, sorts the bytes, deduplicates them, and checks conflicts.
The canonical bytes are then exposed at lines 275-284.

This is a confirmed physical duplicate and an avoidable serialization cost.
One canonical image collection should own each serialized module exactly once.
It must expose sorted/deduplicated bytes without eagerly performing all conflict
checks, because merge conflict checks are effort-accounted and resumable. The
merge producer should consume that collection incrementally, then transfer the
already checked images into the final module set without reserialization.

Permanent tests must count v87 serializations and require one serialization per
distinct input module while preserving order independence, deduplication,
conflict diagnostics, pause/resume, and canonical capsule bytes.

### D-004: The checked v87 format is implemented but not specified as a file

`PROTOTYPE_CHECKED_ARTIFACT_VERSION` is 87 in
`src/prototype/include/a_program/checker/container.h:9`, and
`src/prototype/src/checker/container.c` directly defined the wire order without
a corresponding v87 schema under `src/prototype/spec/` at the audit baseline.

Meanwhile, the main documentation describes v86 but does not clearly introduce
the separate checked v87 boundary:

- `README.md:40,461,483` describes the accepted artifact v86 path;
- `src/prototype/README.md:22,59,539,758` does the same;
- `src/prototype/spec/archive/README.md:6` calls v86 authoritative for that
  format; and
- `src/prototype/spec/artifact_v83.schema` remains outside `archive/` despite
  having no implementation reference.

The v86 statements are not inherently stale: v86 is the accepted
Claim/Derivation proof artifact, while v87 is a different independently checked
semantic container. This is not evidence that v86 can be deleted. PR22 retains
it for roots not yet admitted by checked-Core. The correction is:

1. add a standalone `checked_artifact_v87.schema` matching the executable
   reader/writer and capability rules;
2. document v86 and v87 by their distinct authority roles and give v86 a precise
   retirement condition rather than globally replacing its version number;
3. move the stale v83 schema into the archive; and
4. add a spec-consistency test covering version numbers, section order, hashes,
   optional producer/debug sections, and non-serialization of capabilities.

CR1 completed this correction on 2026-08-27. The active schema is now
`src/prototype/spec/checked_artifact_v87.schema`; the README files name both
authority boundaries, v83 is archived, and the permanent tests cover enum
agreement, malformed sections, optional-section erasure, and canonical
write/read/write bytes.

### D-005: Two alpha-aware graph folds duplicate binder machinery

Core canonical identity uses `canonical_hash_term_at_depth()` in
`src/prototype/src/core/term/canonicalization.inc:1767-2315`. Type
representation identity separately uses
`type_representation_fingerprint_term_at_depth()` and its Match helper in
`src/prototype/src/kernel/type_declaration.c:1596-2165`.

Both maintain binder environments and recursively handle nearly every Term tag,
including Match binders. Their outputs are intentionally different:

- canonical Term keys identify alpha-equivalent Core graph structure; and
- type representation fingerprints include declaration telescopes, recursive
  self/reference handling, parameter/index structure, and Universe-level
  alpha-renaming.

The two identities must not be collapsed into one hash or one equality. The
safe consolidation is an alpha-aware structural cursor/fold and shared binder
environment with separate hash algebras and explicit domain tags. Tests must
continue to demonstrate cases where the two keys intentionally differ.

### D-006: The checked semantic projection has a handwritten field inventory

PR22 correctly defines separate checked semantic records in
`src/prototype/include/a_program/checker/module.h`. They exclude producer
capacity, cache, proof-search, and provenance state. That separation is a trust
boundary and should remain.

However, the field inventory is repeated manually in:

- source-to-semantic projection in `checker/module.c`;
- structural validation in `checker/module.c`;
- reachability and compaction in `checker/module.c`;
- relocation in `checker/module.c`; and
- v87 write/read in `checker/container.c`.

D-001 addresses Term fields. The same risk also applies to Context,
Substitution, occurrence, contract, declaration, interface, and Function Graph
association records. A checked-Core record schema should declare field kinds
once and drive structural validation, closure, relocation, and wire coverage.
Projection remains explicit because it deliberately drops producer fields.

Do not generate checker typing rules from this schema. It owns record shape,
not semantic acceptance.

### D-007: The checked-Core session is a subsystem aggregate

`src/prototype/src/checker/session.c` is 7,271 lines and contains:

- Context lookup and ancestry;
- substitution interpretation and reindex comparison;
- local DefEq/beta reasoning;
- Pi, Match, constructor, and IH checks;
- CBPV request/fold and effect-row checks;
- type declarations and contracts;
- interface/export checks; and
- session effort/state transitions.

No duplicate authority follows from file size. The problem is that internal
ownership and dependency direction are invisible. Split only after D-002
provides stable read-only graph APIs. Suggested internal modules are
`context_and_substitution`, `conversion`, `dependent_match`, `cbpv_effects`,
`declarations`, `interface`, and `session_driver`. The opaque public checker
session API remains unchanged.

### D-008: Include forests act as undocumented internal linkage

Several public-looking `.c` units are actually large single translation units:

- `core/term.c` includes five semantic fragments;
- `frontend/lowering.c` includes context/type lowering, a 12,441-line graph
  construction fragment, six solver fragments, and finalization;
- `kernel/judgement.c` includes DB storage, conversion, solver, rule emission,
  candidate replay, and a 7,903-line accepted replay fragment; and
- `identity/hott.c` includes relation action, certificates, telescope action,
  identity computation, context bridge, object action, and execution.

This arrangement permits private helpers to cross nominal file boundaries
without explicit interfaces. Repeated forward declarations across included
fragments are not duplicate implementations; they often refer to one static
definition in the combined translation unit.

The safe sequence is to define narrow internal headers and state bundles first,
then split translation units. Splitting files first would create broad shared
headers and make ownership worse. Kernel elimination rules remain explicit even
when their allocation/transaction plumbing is shared.

### D-009: One TypeDeclaration aggregate capability leak remains

The previous T1 work correctly separated semantic schema, readback,
representation, constructor classifier cache, and specialization stores. Most
semantic queries now accept `prototype_type_semantic_schema_db` directly.

`prototype_constructor_schema_view_action_classifier()` still accepts the
aggregate mutable `prototype_type_declaration_db`
(`kernel/type_schema_view.h:54-62`). Its direct schema query uses only
`type_declarations->semantic_schema` (`kernel/type_schema_view.c:273-295`), but
it then invokes `prototype_term_graph_reindex_bindings()` with the broad DB at
lines 397-404. Substitution uses that aggregate only when rebuilding a changed
`TYPE_VIEW` (`core/term/substitution.inc:1287-1329`).

This is not a second semantic authority. It is capability leakage through a
Core graph-rewrite API. The remedy is an explicit TypeView rebuild capability
or callback containing exactly the semantic schema/cache operations required by
reindexing. Do not remove named TypeView rebuilding or silently fall back to an
erased structural view in typed action construction.

### D-010 through D-013: Apparent duplicates that must remain distinct

#### Resource usage

`src/prototype/src/graph/occurrence_usage.c` computes producer usage solutions
over typed occurrences. `src/prototype/src/checker/resources.c` independently
reconstructs usage from the checked semantic projection. Similar grade
accumulation is expected. The checker must not trust producer usage vectors.

Shared grade algebra and read-only structural visitors are appropriate;
consolidating the acceptance pass is not.

#### Universe closure

`src/prototype/src/frontend/universe_collection.c:696-1030` reconstructs and
certifies producer obligations from accepted evidence. The checker solver in
`src/prototype/src/checker/universe.c:463-524` reconstructs constraints from
checked semantic content without trusting that evidence. These answer different
trust questions. A common low-level graph solver is possible only if input
collection, certificate validation, and checked capability decisions remain
independent.

#### Accepted replay and checked-Core checking

Accepted replay validates a serialized Derivation DAG. The PR22 checker derives
admitted judgements from semantic content without reading Claims or
Derivations. Combining the two would destroy the authority-erasure property.
They may share Term record schema and pure structural helpers, but not the rule
acceptance path.

#### Function Graph and ordinary type declarations

Function Graph produces generated indexed declarations and selector metadata;
ordinary TypeDeclaration construction owns user declaration schemas. Generated
output is checked as ordinary content, but the package-generation invariant is
additional. No current evidence supports merging these stores or treating
Function Graph metadata as the type schema authority.

### D-014: Hash plumbing can be shared only below identity domains

FNV-style byte mixing is repeated in Core intrinsic fingerprints,
`checker/module.c`, `checker/container.c`, and `producer/capsule.c`. The checked
incremental path uses a separate four-lane content fingerprint. These hashes
belong to different persisted or semantic domains, so replacing them with one
undifferentiated `hash()` would be unsafe.

A small endian-stable byte encoder and named domain-separated hash context can
remove physical plumbing. Constants, versioning, hash width, and semantic field
order remain owned by each format. This is P3 because current duplication is
small and explicit.

### D-015: Driver organization remains broad but authority is no longer split

`src/prototype/src/driver/read_file.c` is 4,258 lines and mixes command parsing,
provider/import ordering, artifact inspection, normalization and type-shape
commands, diagnostics, and reader reentry audits. The previous S1 work removed
duplicate backing storage, so this is not a current authority defect.

Future organization should split command registration/parsing from program
session ownership, provider resolution, artifact commands, and diagnostic
rendering. Preserve one session-owned storage bundle and command ordering.

## 12. Cross-Cutting Conclusions

### 12.1 No current duplicate mutable semantic authority was confirmed

The prior F01-F10 authority defects are recorded complete in
`doc/2026-08-24T18-24-59-AUTHORITY-AND-SEMANTIC-DUPLICATION-CORRECTION-IMPLEMENTATION-PLAN.md`
at commit `64c7934`. Current source inspection found the expected replacements:

- typed classifier/effect/usage solution stores and a single freeze;
- reconstructed Universe closure;
- schema-revision-aware caches;
- explicit HOTT semantic freeze;
- TypeDeclaration capability stores;
- immutable accepted premise views;
- typed Judgement transactions; and
- session-owned driver storage.

The present findings are therefore mainly algorithm inventory drift and module
organization introduced or exposed by checked-Core, not a recurrence of the old
multiple-authority design.

### 12.2 The dominant root cause is missing immutable representation APIs

D-001, D-002, D-005, and D-006 share one root cause: algorithms are attached to
large mutable DB owner types or handwritten record switches instead of a small
immutable graph vocabulary. A Program should not merge Value/Computation,
typing/proof, or producer/checker semantics to fix this. It should expose the
same underlying graph structure through narrow views and typed folds.

### 12.3 File splitting is downstream work

`graph_construction.inc`, `accepted_replay.inc`, `object_term_action.inc`, and
`checker/session.c` should not be divided merely to reduce line counts. First
establish owners, immutable views, and internal APIs. Otherwise the same hidden
coupling will be spread across more files.

## 13. Recommended Implementation Sequence

### S1: Specify and test the dual v86/v87 boundaries

- add the v87 schema and document the distinct v86/v87 authority boundaries;
- archive v83; and
- add executable schema-consistency and round-trip gates.

This is first because later structural work must not accidentally change a
persistent format whose specification currently exists only in C code.

### S2: Introduce the Term record reference schema

- inventory every Term tag and payload field;
- introduce typed reference roles and slice descriptors;
- migrate structural validation, closure, and relocation first;
- migrate v87 read/write coverage second; and
- retain evaluation and typing switches as explicit semantic algorithms.

### S3: Add immutable graph views and pure structural algorithms

- move the semantic Term view to a Core-level header;
- add Context/Substitution structural views;
- provide non-allocating child/reference, binding, ancestry, substitution-image,
  and alpha-aware traversal primitives;
- make mutable DB wrappers delegate to them; and
- migrate checker representation plumbing without sharing checker acceptance.

### S4: Remove duplicate merge image ownership

- introduce one canonical image collection shared by module-set construction
  and the effort-accounted merge producer;
- adapt merge producer and capsules to consume its canonical bytes without
  bypassing incremental conflict checks; and
- add serialization-count and pause/resume regression tests.

### S5: Generalize checked semantic record schemas

- cover Context, Substitution, occurrence, contract, declaration, and interface
  reference fields;
- use the schema for closure, relocation, wire coverage, and malformed-field
  mutation generation; and
- keep projection and checker rules explicit.

### S6: Share alpha traversal and narrow TypeView rebuild capability

- factor binder/frame/Universe-level environments from the two fingerprints;
- keep domain-separated output algebras;
- replace broad TypeDeclarationDB access in typed graph reindexing with an
  explicit TypeView rebuild capability.

### S7: Reorganize translation units

- split checker session after S3;
- split checked projection/container after S5;
- define internal APIs before splitting lowering, judgement, and HOTT include
  forests; and
- split driver command responsibilities last.

## 14. Permanent Validation Matrix

Any implementation plan derived from this audit must include:

- exhaustive Term tag/reference-role coverage;
- all-tag closure and relocation tests;
- canonical v87 round trips and malformed-reference mutation tests;
- checker authority-erasure tests proving no Claim/Derivation/cache dependency;
- producer replay versus checker differential tests;
- resource and Universe independent-reconstruction tests;
- alpha-renaming tests for Term keys and type fingerprints;
- one-serialization-per-distinct-module instrumentation;
- merge order, deduplication, conflict, pause, resume, and capsule tests;
- ordinary examples 01-09, indexed family, IF8, Function Graph, effects, and
  HOTT admitted-fragment tests; and
- per-file added/deleted/net line counts and performance medians before and after
  each structural package.

## 15. Audit Limitations

- This was a static code and document audit. It did not mutate implementation or
  run the full integration suite.
- A large function or repeated switch was reported only when an owner or shared
  invariant could be identified.
- Unsupported future HOTT, effectful Identity, classifier-capsule, and source
  scheduler work was not classified as dead code.
- v86 was not classified obsolete because the current PR22 plan explicitly uses
  it as a migration oracle for unsupported roots.

## 16. Progress Log

- 2026-08-26 13:16 JST: fixed baseline at `fd546e1`; worktree and remote were
  synchronized; recorded implementation size and largest files.
- 2026-08-26 13:16 JST: started reconciling the 2026-08-24 authority audit with
  the PR22 checked-Core implementation rather than assuming old findings remain.
- 2026-08-26 13:24 JST: completed Term-tag switch inventory and separated
  computational children from storage references; confirmed D-001.
- 2026-08-26 13:29 JST: traced checker-local Context, Substitution, conversion,
  effect-row, resource, and Universe paths; confirmed D-002 and rejected unsafe
  producer/checker acceptance unification.
- 2026-08-26 13:33 JST: traced merge image ownership through
  `prototype_checked_module_set_create()`; confirmed duplicate serialization,
  sorting, and deduplication.
- 2026-08-26 13:37 JST: reconciled v86/v87 implementation, schema, README, and
  PR22 migration policy; confirmed a specification/documentation gap without
  incorrectly declaring v86 dead.
- 2026-08-26 13:41 JST: completed TypeDeclaration, fingerprint, include-forest,
  Function Graph, hash, driver, and test-boundary review; finalized priorities
  and negative decisions.
- 2026-08-26 13:55 JST: implementation-plan review corrected D-004: current v86
  documentation is not obsolete merely because the distinct checked v87
  container exists; the actual gap is missing dual-boundary documentation and a
  standalone v87 schema.
