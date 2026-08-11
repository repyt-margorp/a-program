# Prototype Semantics-Preserving Codebase Organization Plan

Date: 2026-08-11

Status: proposed; implementation not started

Repository baseline:

- branch: `main`;
- commit: `54200d1` (`Complete prototype layout equivalence audit`);
- implementation root: `src/prototype/`;
- active artifact format: v71;
- active implementation and public-header size: 106,872 lines
  (100,043 implementation lines plus 6,829 public-header lines);
- active implementation files: 45 `.c`/`.inc` files; and
- active public headers: 27 files below `src/prototype/include/a_program/`.

Related documents:

- `2026-08-11T04-38-38-PROTOTYPE-PHYSICAL-SOURCE-LAYOUT-REORGANIZATION-PLAN.md`;
- `2026-08-11T05-45-00-PROTOTYPE-SOURCE-LAYOUT-LOC-INVENTORY.md`;
- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`;
- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`;
- `2026-08-09T19-46-59-A1-T0-RELATION-TO-HIGHER-OBSERVATIONAL-IDENTITY-AUDIT.md`.

## 1. Objective

Make the production-intent prototype easier to navigate, review, and modify
without changing any language semantics or implementation state.

The first physical reorganization has already separated support, core, graph,
kernel, identity, artifact, frontend, driver, tests, and specifications. This
second stage addresses concentration and dependency problems inside that
layout:

1. `frontend/lowering.c` and `core/term.c` remain very large ownership units;
2. artifact publication and command-driver responsibilities remain
   concentrated;
3. public headers include whole subsystems where only a few data types are
   required;
4. `judgement.c` and `identity/hott.c` are physically partitioned but still rely
   on translation-unit include order and undeclared private dependencies; and
5. reusable diagnostics and inspection routines are duplicated by the REPL and
   file driver.

This is a code-organization plan, not a semantic refactoring plan. A semantic
defect discovered while executing it must be recorded in a separate issue or
plan and must not be fixed in the same commit as a move or extraction.

## 2. Non-Goals

This plan does not:

- promote prototype code into accepted `src/` or `include/`;
- alter the surface grammar or reserve new words or symbols;
- add `Eq`, `refl`, transport, equality reflection, or another identity rule;
- expand or reduce the implemented Higher Observational Type Theory fragment;
- alter CBPV polarity, implicit coercion, effect-row, handler, or runtime rules;
- alter normalization, conversion, reduction profiles, fuel, or cache keys;
- change Context, Substitution, Binding, Operation, Term, Proposition, Claim,
  Derivation, Universe, TypeDeclaration, or HOTT record identity;
- split the value and computation fragments into separate Term databases;
- combine distinct kernel rules merely because their C control flow is similar;
- change artifact v71 fields, ordering, fingerprints, or validation;
- rename public APIs, enum members, proof kinds, or Term tags;
- introduce compatibility wrappers, forwarding headers, duplicate source files,
  path remaps, or symlinks; or
- optimize, simplify, or repair algorithms while moving them.

## 3. Meaning of “No Semantic or Implementation-State Change”

The required equivalence is stronger than “the examples still compile.” Every
phase must preserve all of the following.

### 3.1 Source and typing behavior

- the same positive fixtures are accepted;
- the same negative fixtures are rejected at the same compiler stage;
- source Operation nodes retain their IDs, contexts, polarity, classifier, and
  shared core Term references;
- generated Propositions, Claims, Derivations, and ordered premise edges remain
  identical;
- classifier solver residuals and reasons remain identical; and
- diagnostics remain byte-identical unless a later diagnostics-only plan
  explicitly changes their contract.

### 3.2 Core and runtime behavior

- Term tags, canonical keys, binding identities, and interned graph shape remain
  identical;
- WHNF/NF result, completion state, step count, and cache behavior remain
  identical for the same normalization profile;
- runtime operation requests, resumptions, folds, and host dispatch remain
  identical; and
- no source occurrence annotation is moved into context-erased Term identity.

### 3.3 Proof and identity behavior

- conversion remains a kernel/meta-level judgement and is not merged with
  object Identity;
- compiler-local parametric relation action remains distinct from object
  Identity evidence;
- Context/Substitution morphisms remain distinct from the Derivations that cite
  them;
- `APP_ELIM`, `MATCH_ELIM`, `INDUCTION_HYPOTHESIS_ELIM`, and
  `COMPUTATION_FOLD_ELIM` remain named, separate rules;
- HOTT requests, deterministic outcomes, residuals, certificates, and artifact
  roots remain identical; and
- unsupported higher, dependent, effectful, host, and Universe cases remain
  unsupported or residual rather than being silently admitted.

### 3.4 Artifact and build behavior

- artifact format stays at v71;
- schema and calculus fingerprints remain unchanged;
- deterministic artifact fixtures remain byte-identical;
- read, validate, append, relocate, link, and republish behavior remains
  identical;
- public symbol inventory remains unchanged during move-only phases;
- public struct size, alignment, and field offsets remain unchanged; and
- compiler source manifests produce the same executable roles.

## 4. Current Architecture and Concentration

### 4.1 Intended dependency direction

```text
support
  -> core Term graph
  -> kernel Context / Substitution / declarations / judgement
  -> graph typed Operation occurrences and compile metadata
  -> identity actions and object Identity certificates
  -> artifact persistence and linking
  -> frontend orchestration
  -> drivers
```

This diagram is an ownership direction, not a statement that every C include
already follows it. The current graph contains broad umbrella includes and a
few reverse dependencies that this plan will make explicit before changing.

### 4.2 Current large files

| Current file | Lines | Responsibilities currently co-located |
| --- | ---: | --- |
| `src/frontend/lowering.c` | 16,883 | name resolution, AST lowering, typed Operation construction, CBPV elaboration, Match/IH handling, classifier/effect constraints, fixed point, proof publication orchestration |
| `src/core/term.c` | 10,027 | TermDB storage, interning, alpha identity, constructors, substitution/reindex, normalization, conversion support, host primitive reduction, runtime dispatch, printing |
| `src/artifact/publication.c` | 6,155 | publication closure, canonical ordering, graph slicing, validation support, textual serialization |
| `src/identity/object_term_action.inc` | 5,585 | object action for variables, constructors, APP, Lambda, Match, CBPV terms, and higher witnesses |
| `src/kernel/typing/accepted_replay.inc` | 4,959 | accepted proof replay for all rule families, helper Claim construction, and proof printing |
| `src/driver/read_file.c` | 4,862 | module search/import, link orchestration, compilation commands, diagnostics, graph inspection, normalization comparisons |
| `src/identity/identity_computation.inc` | 3,596 | generated Identity families, rules for supported type formers, result/certificate publication |
| `src/kernel/type_declaration.c` | 3,449 | declarations, constructor telescopes, curried classifiers, structural keys, generated Identity declarations |
| `src/artifact/wire_v71.c` | 3,309 | complete v71 reader and sparse-reference validation |
| `src/frontend/reader.c` | 2,947 | lexer/parser and source diagnostics |
| `src/kernel/typing/conversion.inc` | 2,912 | conversion, classifier views, Pi instantiation, effect-row constraints, evidence selection |
| `src/kernel/rules/match.inc` | 2,894 | Match typing, motives, conversion obligations, recursion and some Universe support |
| `src/identity/context_bridge.inc` | 2,560 | relation Context bridges, projections, degeneracy, and Universe correspondence |
| `src/kernel/rules/cbpv.inc` | 2,475 | CBPV rules, host signatures, fold constraints, and effect-row processing |
| `src/graph/operation_graph.c` | 2,324 | OperationGraph storage/validation, verification obligations, and runtime evaluator |
| `src/artifact/link.c` | 2,299 | append/link, relocation application, canonical references, and accepted proof graph import |

### 4.3 Confirmed dependency and duplication issues

The following are physical organization problems. They are not, by themselves,
proofs of semantic bugs.

1. `graph/operation_graph.h` includes frontend AST, Context, type declarations,
   and all judgement API headers. Its public data structures should depend on
   minimal shared type declarations, not every operation available on those
   types.
2. `artifact/interface.h` includes frontend AST and the complete judgement API
   surface. Artifact records use many shared IDs and structures, but most API
   declarations can be forward-declared or moved behind operation-specific
   headers.
3. `frontend/lowering.h` exports two orchestration functions but includes the
   artifact API and every judgement API header.
4. `identity/types.h` is a 506-line umbrella that includes CwF certificates,
   kernel view, and all judgement APIs. Every identity header therefore imports
   the entire identity and judgement type universe.
5. `kernel/universe.c` and `kernel/judgement.c` include
   `frontend/lowering.h`. The immediate reason is shared Operation/compile
   structures, but the include makes a kernel implementation appear to depend
   on frontend orchestration.
6. `driver/read_file.c` and `driver/repl.c` duplicate resolution error names,
   resolution trace printers, type-expression printers, declaration printers,
   and Universe graph printers.
7. `judgement.c` and `identity/hott.c` preserve one translation unit by textual
   inclusion. This correctly minimized semantic risk in the first move, but
   private dependencies are still implicit in include order.
8. `core/term.h` exposes the complete TermDB and over 500 lines of operations in
   one header. A caller cannot depend only on Term tags, construction, reduction,
   or inspection.
9. `build/sources.mk` repeats overlapping source lists manually. The current
   lists are correct, but their set relationships are not mechanically checked.

## 5. Ownership Rules for the Reorganization

The following authorities must remain singular.

| Concept | Sole semantic authority | Other modules may hold |
| --- | --- | --- |
| context-erased computation identity | `core/TermDB` | Term IDs and views |
| typed source occurrence | `graph/OperationGraph` | Operation IDs and immutable references |
| named ADT/type declaration | `kernel/TypeDeclarationDB` | declaration IDs and readback data |
| Context and substitution morphism | `kernel/ContextDB` and `SubstitutionDB` | IDs and certificates |
| accepted typing fact | immutable Proposition/Claim in `JudgementDB` | Claim references |
| proof reason | append-only Derivation DAG | Derivation references and roots |
| meta-level conversion | kernel conversion implementation | structured conversion result/evidence |
| compiler-local relation action | identity relation planner/action DB | deterministic request/result IDs |
| object Identity | ordinary Term, Claim, and Derivation | validated artifact roots |
| persistent module boundary | artifact v71 interface | dense relocation maps and external refs |

Physical extraction must not create a second cache, registry, or reconstructed
metadata representation that can become authoritative.

## 6. Target Source Organization

The directory roots remain unchanged. New files divide existing owners by
operation while retaining each database and semantic authority.

```text
src/prototype/
  include/a_program/
    core/
      term_types.h
      term_db.h
      term_build.h
      term_substitution.h
      term_reduction.h
      term_inspect.h
      term.h                 # temporary aggregate during header migration only
    graph/
      operation_types.h
      operation_graph.h
      operation_runtime.h
      verification.h
      compile_metadata.h
    kernel/
      context.h
      type_declaration.h
      universe.h
      judgement/
        types.h
        db.h
        conversion.h
        classifier_solver.h
        rules.h
    identity/
      types/
        outcome.h
        relation.h
        action.h
        certificate.h
      ... existing operation headers ...
    artifact/
      types.h
      interface.h
      publication.h
      wire_v71.h
      link.h
    frontend/
      ast.h
      reader.h
      lowering.h
      diagnostics.h

  src/
    core/
      term.c                 # one-TU owner during the first split
      term/
        db.inc
        identity.inc
        build.inc
        substitution.inc
        effects.inc
        reduction.inc
        host.inc
        inspect.inc
    frontend/
      lowering.c             # one-TU phase orchestrator during the first split
      lowering/
        context.inc
        names.inc
        types.inc
        operations.inc
        cbpv.inc
        match.inc
        constraints.inc
        effects.inc
        proof_materialization.inc
        phases.inc
    graph/
      operation_graph.c
      operation_runtime.c
      verification.c
      compile_metadata.c
    kernel/
      judgement.c
      judgement_internal.h
      typing/...
      rules/...
    identity/
      hott.c
      hott_internal.h
      ... existing partitions ...
    artifact/
      interface.c
      publication.c
      publication/
        closure.inc
        ordering.inc
        slice.inc
        writer.inc
      wire_v71.c
      relocation.c
      link.c
    frontend/
      diagnostics.c
    driver/
      read_file.c
      repl.c
```

The exact filenames may be adjusted after dependency extraction. The ownership
boundaries and invariance rules may not be adjusted merely to make a move easier.

## 7. File-by-File Disposition

| Current file | Planned action | Semantic constraint |
| --- | --- | --- |
| `support/symbol.c` | retain | no reason to split a cohesive 109-line owner |
| `core/term.c` | partition inside one TU first | one TermDB, one intern table, unchanged cache and mutation order |
| `frontend/ast.c` | retain | AST allocation and index ownership is cohesive |
| `frontend/reader.c` | retain initially | parser changes are explicitly out of scope |
| `frontend/ast_inspect.c` | retain; consume shared diagnostics only where output is identical | no formatting changes |
| `frontend/lowering.c` | partition inside one TU first | preserve exact phase order and all static state lifetimes |
| `graph/compile_metadata.c` | retain | small cohesive storage owner |
| `graph/operation_graph.c` | separate runtime and verification after characterization tests | OperationGraph remains the typed occurrence authority |
| `kernel/context.c` | retain initially | Context and substitution form one CwF substrate; do not collapse proof certificates into morphisms |
| `kernel/cwf_certificate.c` | retain | certificates cite Context/Substitution facts; they do not become the morphisms |
| `kernel/kernel_view.c` | retain | small aggregate validation boundary |
| `kernel/type_declaration.c` | partition only after authority tests | constructor telescope/classifier graph remains authoritative; readback metadata cannot regain authority |
| `kernel/universe.c` | extract compiler collection adapter later | Universe constraint solver must not own frontend phase policy |
| `kernel/judgement.c` | keep one TU until private interfaces are explicit | no proof rule merge and no replay-order change |
| `kernel/typing/*.inc` | retain named responsibilities; add private declarations | conversion, solving, publication, and replay remain distinguishable |
| `kernel/rules/*.inc` | retain theorem-oriented files | similar plumbing is not sufficient reason to unify rules |
| `identity/hott.c` | keep one TU until private interfaces are explicit | preserve deterministic action ordering and certificate fingerprints |
| `identity/*.inc` | retain operation-oriented partitions | relation action and object Identity remain separate concepts |
| `artifact/interface.c` | retain, narrow to interface records and structural validation | do not duplicate HOTT semantic replay here |
| `artifact/publication.c` | partition inside one TU first | closure and canonical ordering remain byte-identical |
| `artifact/wire_v71.c` | retain initially | wire reader is format-specific and sparse validation must remain centralized |
| `artifact/relocation.c` | retain | one relocation authority |
| `artifact/link.c` | retain initially; expose internal stages only after tests | no remap compatibility layer and no alternate proof authority |
| `driver/read_file.c` | extract reusable diagnostics and session orchestration | CLI output and exit codes remain identical |
| `driver/repl.c` | consume reusable diagnostics | interactive command behavior remains identical |

## 8. Implementation Phases and Progress

Only one phase may be in progress at a time. Each phase is independently
reviewable and revertible.

### R0: Freeze an equivalence baseline

Status: `[x] complete`

- [x] Record commit, compiler version, C compiler, and build flags.
- [x] Record sorted source and public symbol inventories.
- [x] Record physical line counts for every implementation and public-header
      file, using one checked command and one fixed inclusion policy.
- [x] Record sizes and offsets of public structures used by artifacts/tests.
- [x] Run every integration script and preserve logs and exit statuses outside
      the repository.
- [x] Save deterministic artifact hashes for the normal and perturbation runs.
- [x] Save representative `:whnf`, `:nf`, proof-DAG, residual, and diagnostics
      output.
- [x] Add one manifest-consistency check that verifies source-list set
      relationships without changing executable contents.
- [x] Verify that current README artifact-version claims agree with the first
      line of `spec/artifact_v71.schema`; repair the known root v61 and
      prototype v44 descriptions in a documentation-only commit before using
      them as baseline instructions.

Exit gate:

- [x] baseline evidence can detect Term/Operation/proof/artifact/output drift.

Evidence for baseline commit `54200d144b95c37ecb0aab42c5bb7a81d30682c1`
is stored outside the repository in
`/tmp/a-program-r0-54200d1-20260811`. It contains per-test logs, source and
public-symbol inventories, fixed LOC inventories, ABI size/offset records,
deterministic artifact hashes, and representative driver/normalization/proof
output. `tests/integration/test_spec_consistency.sh` and
`tests/integration/test_public_headers.sh` provide the new executable manifest
and header-boundary checks.

### R1: Narrow public header dependencies

Status: `[ ] blocked by R0`

- [ ] Measure direct and transitive includes for each public header.
- [ ] Introduce minimal type-only headers or forward declarations.
- [ ] Remove unused judgement operation headers from
      `graph/operation_graph.h`, `artifact/interface.h`,
      `frontend/lowering.h`, and `identity/types.h`.
- [ ] Break the apparent `kernel -> frontend/lowering.h` dependency by placing
      shared Operation structures in graph-owned headers.
- [ ] Split declarations from operation APIs without changing any struct field,
      signature, enum, or symbol name.
- [ ] Compile every public header as the first include in an empty C file.
- [ ] Compare pre/post public symbols and struct layout records.

Exit gate:

- [ ] the include graph exposes ownership direction and all ABI records match.

### R2: Make one-TU private contracts explicit

Status: `[ ] blocked by R1`

- [ ] Create `kernel/judgement_internal.h` for declarations shared only among
      judgement partitions.
- [ ] Create `identity/hott_internal.h` for declarations shared only among HOTT
      partitions.
- [ ] Remove accidental dependence on textual include order.
- [ ] Keep `judgement.c` and `hott.c` as one translation unit each during this
      phase.
- [ ] Add compile checks that permute independent partition ordering where
      possible; document genuinely ordered groups.
- [ ] Do not export private helpers through prototype-public headers.

Exit gate:

- [ ] every cross-partition call has a declared private contract, while generated
      code and runtime behavior remain unchanged.

### R3: Partition `core/term.c` without creating another Term model

Status: `[ ] blocked by R2`

- [ ] Produce a function-to-owner inventory before moving code.
- [ ] Move storage/init/import functions to `term/db.inc`.
- [ ] Move alpha-aware shape identity and canonical-key logic to
      `term/identity.inc`.
- [ ] Move constructors and application-spine helpers to `term/build.inc`.
- [ ] Move substitution, reindex, binding, and IH-frame logic to
      `term/substitution.inc`.
- [ ] Move effect-row and CBPV Term helpers to `term/effects.inc`.
- [ ] Move WHNF/NF, cache, and structured conversion support to
      `term/reduction.inc`.
- [ ] Move host primitive evaluation/dispatch to `term/host.inc`.
- [ ] Move readback/debug printing to `term/inspect.inc`.
- [ ] Include these files from `term.c` in the original definition order first.
- [ ] Verify that no extracted file introduces a second registry or cache.

Exit gate:

- [ ] Term IDs, canonical keys, cache traces, reductions, and runtime dispatch
      match the R0 baseline exactly.

### R4: Partition `frontend/lowering.c` around explicit compiler phases

Status: `[ ] blocked by R3`

- [ ] Freeze the current orchestration order as a phase table and test it.
- [ ] Extract compile-context and workspace helpers.
- [ ] Extract source-name/import resolution.
- [ ] Extract type expression and declaration lowering.
- [ ] Extract typed Operation construction.
- [ ] Extract CBPV elaboration and computation-block lowering.
- [ ] Extract Match, motive, recursion, and IH handling.
- [ ] Extract classifier constraints and fixed-point scheduling.
- [ ] Extract effect-row constraint generation and residual recording.
- [ ] Extract ascription, expectation, and proof materialization.
- [ ] Leave `lowering.c` as the orchestration owner and one-TU include point.
- [ ] Name the repeated type-view/constructor canonicalization barriers and
      document their pre/postconditions; do not remove or combine them in this
      plan.

The preserved phase order is:

1. definition-index diagnostics;
2. AST/Operation/Term graph construction;
3. operation-store installation;
4. constructor saturation validation;
5. imported constructor classifier inference;
6. reference canonicalization barrier;
7. pending Match resolution;
8. reference canonicalization barrier;
9. pending classifier fixed point;
10. effect constraint generation, commit, and residual recording;
11. strict-policy validation;
12. ascription and expectation validation;
13. pending classifier-state rejection;
14. final reference canonicalization barrier;
15. primitive proof expansion;
16. normalization-premise conversion insertion;
17. candidate publication and accepted-DAG replay;
18. OperationGraph and constructor-cache validation;
19. constructor-view owner erasure; and
20. label publication.

Exit gate:

- [ ] phase event traces, solver steps, accepted proof DAG, and published labels
      match the R0 baseline exactly.

### R5: Separate OperationGraph storage, verification, and runtime execution

Status: `[ ] blocked by R4`

- [ ] Add focused characterization tests for OperationGraph validation,
      verification obligations, runtime requests, folds, and resumptions.
- [ ] Move verification-obligation storage/validation to `graph/verification.c`.
- [ ] Move runtime machine/environment/resumption logic to
      `graph/operation_runtime.c`.
- [ ] Keep OperationGraph node storage and graph invariants in
      `graph/operation_graph.c`.
- [ ] Pass explicit immutable graph views instead of reconstructing typed
      meaning from TermDB.
- [ ] Confirm that this is a module split only; it must not split the underlying
      graph or add value/computation graph variants.

Exit gate:

- [ ] runtime traces and verification records match R0, and OperationGraph has
      one owner.

### R6: Partition artifact publication without changing v71

Status: `[ ] blocked by R5`

- [ ] Characterize publication closure, canonical order, sparse validation,
      relocation, append, link, and republish independently.
- [ ] Partition `publication.c` into closure, ordering, slicing, and writer code
      while retaining one TU first.
- [ ] Keep `wire_v71.c` format-specific and do not introduce a generic wire
      abstraction without a second active format.
- [ ] Make identity-root semantic validation call one identity-owned verifier;
      artifact code should validate persistent references, closure, and schema.
- [ ] Do not alter artifact records or move compiler-local HOTT work state into
      artifacts.
- [ ] Compare artifact bytes after every extraction.

Exit gate:

- [ ] every deterministic artifact hash and malformed-artifact result matches R0.

### R7: Consolidate driver diagnostics and compiler-session orchestration

Status: `[ ] blocked by R6`

- [ ] Extract duplicated resolution/type/Universe printers into
      `frontend/diagnostics.c` or a driver-shared inspection module.
- [ ] Extract reusable module search/import/link orchestration from
      `driver/read_file.c` into a compiler-session module.
- [ ] Leave command parsing, REPL state, and process exit policy in the drivers.
- [ ] Preserve every emitted byte and exit status with golden comparisons.
- [ ] Do not make diagnostics functions semantic authorities.

Exit gate:

- [ ] REPL and file-driver output is byte-identical and duplicate helper bodies
      are removed.

### R8: Optional translation-unit separation

Status: `[ ] deferred; requires R2-R7`

This phase is optional. Physical `.inc` partitions already provide navigability
without changing linkage. Separate compilation is justified only if it improves
build isolation and dependency checking enough to offset the additional private
API surface.

- [ ] Measure incremental-build and dependency benefits.
- [ ] Convert only partitions with explicit, narrow private interfaces.
- [ ] Preserve initialization and mutation order.
- [ ] Compare symbol visibility and ensure no formerly static helper becomes
      public accidentally.
- [ ] Keep tightly coupled rule replay in one TU if separation obscures theorem
      ownership or requires a broad internal API.

Exit gate:

- [ ] separate compilation has measured value and complete equivalence evidence.

### R9: Final audit and documentation

Status: `[ ] blocked by R7; R8 is optional`

- [ ] Update `src/prototype/README.md` with the final ownership map.
- [ ] Regenerate the LOC inventory and record per-file additions/deletions.
- [ ] Record which reductions are physical movement, header cleanup, or actual
      duplicate removal.
- [ ] Record public include fan-out before and after.
- [ ] Record build-time changes separately from LOC changes.
- [ ] Run the full R0 evidence bundle again.
- [ ] Confirm `git diff --check` and a clean generated-file audit.
- [ ] Mark this document complete only after every mandatory gate passes.

## 9. Required Verification Matrix

| Area | Required evidence |
| --- | --- |
| build | root `make`, reader build, focused `-Wall -Wextra -Werror` builds |
| examples | supported examples through the current expected boundary |
| typing | dependent Pi, shared-core typed occurrences, Match/IH, constructor polarity |
| CBPV | boundary, surface, block sequence, strict value/effect/dependent checks |
| effects | higher-order operation, handler, latent rows, resumption multiplicity |
| conversion | scoped conversion, structured results, profile-aware WHNF cache |
| Context/CwF | category laws, shared reindex, binding identity |
| proof DAG | candidate publication, accepted replay, premise order, operation-indexed evidence |
| identity | ADT, Pi, dependent fragment, non-DefEq function witness, selected higher witness, residual cases |
| artifact | publish/read/append/link/republish, malformed sparse references, deterministic hashes |
| drivers | REPL and file-driver golden output and exit status |
| memory safety | focused ASan/UBSan checks used by the existing integration suite |

The exact commands are captured in R0 from the current integration scripts.
This avoids duplicating a command list that can drift from the executable test
manifest.

## 10. Commit Discipline

Each commit must have exactly one primary purpose:

1. baseline or characterization test;
2. move-only partition;
3. private declaration extraction;
4. public include narrowing;
5. exact duplicate removal; or
6. documentation/manifest update.

A commit must not combine a file move with a typing, reduction, identity,
artifact, or diagnostics behavior change. `git diff --find-renames` and a
whitespace-insensitive diff are required for move-only review.

If a semantic failure appears after a move:

1. stop the current phase;
2. identify whether the move exposed an existing undefined dependency or
   introduced behavior drift;
3. revert or correct only the physical extraction;
4. record any pre-existing semantic defect separately; and
5. rerun the phase gate before continuing.

## 11. LOC and Complexity Accounting

LOC reduction is not a success criterion, but the final report must explain the
effect of the work.

For each phase record:

| Phase | Files before | Files after | LOC before | LOC after | Duplicated LOC removed | Header fan-out before/after | Notes |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| R0 | TBD | TBD | TBD | TBD | 0 | TBD | baseline |
| R1 | TBD | TBD | TBD | TBD | TBD | TBD | header cleanup |
| R2 | TBD | TBD | TBD | TBD | TBD | TBD | private contracts |
| R3 | TBD | TBD | TBD | TBD | TBD | TBD | Term partitions |
| R4 | TBD | TBD | TBD | TBD | TBD | TBD | lowering partitions |
| R5 | TBD | TBD | TBD | TBD | TBD | TBD | graph/runtime split |
| R6 | TBD | TBD | TBD | TBD | TBD | TBD | artifact partitions |
| R7 | TBD | TBD | TBD | TBD | TBD | TBD | driver deduplication |
| R8 | TBD | TBD | TBD | TBD | TBD | TBD | optional separate compilation |
| R9 | TBD | TBD | TBD | TBD | TBD | TBD | final state |

The phase summary is not sufficient by itself. R0 must create a file-level
baseline, and every later phase must append a file-level delta table with this
schema:

| File before | File after | Baseline LOC | Final LOC | Added | Deleted | Net | Responsibility moved or deduplicated | Reason when net growth is positive |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD |

Rules for this accounting:

- use `git diff --numstat` for additions and deletions and the R0 command for
  physical LOC; do not infer one from the other;
- record moves and splits by mapping every old owner to all new owners, rather
  than reporting only repository totals;
- report deleted duplicate implementations separately from lines merely moved;
- report generated files, tests, specifications, and documentation separately
  from implementation and public headers;
- explain positive net growth caused by explicit private contracts, invariants,
  or characterization tests;
- attach the table to each phase report and produce one consolidated table in
  the final report; and
- never delete proof rules, checks, or semantic distinctions merely to improve
  the LOC result.

Expected outcomes:

- move-only partition phases may slightly increase LOC because private contracts
  and module comments become explicit;
- driver diagnostic extraction should remove real duplicate LOC;
- header narrowing should reduce preprocessed input and rebuild fan-out more than
  source LOC; and
- no semantic code should be deleted merely to improve the metric.

The preferred outcome is a reduction in implementation LOC where exact
duplicate plumbing can be replaced by one owned operation. The controlling
criterion remains clearer authority and preserved semantics; a smaller number
without those properties is a regression.

### 11.1 Execution reconciliation (2026-08-11)

The later functional and theory review in
`2026-08-11T08-19-24-CURRENT-COMPILER-FUNCTIONAL-AND-THEORY-REVIEW.md` became
the implementation authority for this work. Consequently, unchecked bullets in
the original fine-grained R3/R4 sketch do not necessarily denote missing work:
the broader ownership concerns were implemented using partitions selected after
the semantic audit.

The executed organization decisions were:

- keep large static-helper families in one translation unit through `.inc`
  partitions, preserving declaration visibility, allocation order, and IDs;
- partition Match motive/rule emission from candidate search and expansion;
- partition ordinary introduction rules from relation-witness rules without
  collapsing distinct proof rules into one generic validator;
- move compiler relation planning and its public POD types to the
  `parametricity` namespace while retaining object Identity ownership under
  `identity`;
- move the artifact boundary to v71 and keep compiler search state out of the
  persistent format; and
- reject R8 separate compilation as an objective: it is only justified later
  when a stable private contract exists and exact behavior remains preserved.

The physical T9 result was:

| Original owner | Baseline LOC | Final owner plus partitions LOC | Net |
| --- | ---: | ---: | ---: |
| `core/term.c` | 10,044 | 10,048 | +4 |
| `frontend/lowering.c` | 17,701 | 17,281 | -420 |
| `artifact/publication.c` | 6,238 | 6,242 | +4 |
| `graph/operation_graph.c` | 2,329 | 2,332 | +3 |
| `kernel/rules/introduction_identity.inc` | 1,867 | 1,870 | +3 |
| `kernel/rules/match.inc` | 2,893 | 2,897 | +4 |
| **Total** | **41,072** | **40,670** | **-402** |

Before wrapper includes and ownership comments were added, concatenating each
partition set reproduced its original owner byte-for-byte by SHA-256. The only
semantic-source deletion was 424 lines of disabled duplicate usage-solver code
in `lowering.c`; no proof rule or semantic distinction was deleted for LOC
reduction.

## 12. Risks and Controls

### 12.1 Static helper and include-order drift

Moving code out of one TU can alter declaration order, static state, and symbol
visibility. R2 and the one-TU-first rule control this risk.

### 12.2 Initialization and ID-order drift

Term, Operation, Claim, Derivation, Context, substitution, and HOTT IDs depend on
mutation order. The phase trace and artifact/proof comparisons must detect even
behaviorally equivalent reordering.

### 12.3 Accidental authority duplication

Extracted modules may be tempted to cache or reconstruct data locally. Every new
record must be classified as authority, immutable reference, derived cache, or
diagnostic snapshot before introduction. This plan permits no new authority.

### 12.4 False abstraction across proof rules

Shared C plumbing does not imply the same theorem. Rule files remain separate,
and only storage iteration or error formatting may use common helpers when their
contracts are identical.

### 12.5 Semantic work hidden as cleanup

Known concerns in conversion, dependent CBPV, effect rows, Universe handling,
object Identity, and artifacts must remain in their existing plans. They are not
opportunistically resolved here.

## 13. Decision Summary

The safe next organization step is not another broad directory move. It is:

1. freeze a stronger equivalence baseline;
2. narrow header dependencies;
3. make private one-TU contracts explicit;
4. partition `term.c`, `lowering.c`, and `publication.c` inside their existing
   translation units;
5. only then separate cohesive runtime, verification, diagnostics, and session
   modules where the ownership boundary is already clear; and
6. treat separate compilation as optional rather than as the objective.

This order improves navigability while preserving the current compiler exactly,
including its intentionally incomplete Higher Observational Type Theory fragment
and its current CBPV, effect, proof, and artifact behavior.
