# Prototype Physical Source Layout Reorganization Plan

Date: 2026-08-11

Status: in progress; L0-L2 complete, L3 next

Repository baseline:

- branch: `main`;
- commit: `97302c9` (`Record A1 object identity verification`);
- active implementation root: `src/prototype/`;
- artifact format: v70;
- prototype line count: 130,375 lines, including tests, fixtures, schemas, and
  the inactive `current/` snapshot;
- prototype test scripts: 16, all passing at the baseline; and
- deterministic HOTT artifact hash:
  `2d64defe38600f40e73bcb6bfa5b92e925f771783b8ea14de2641daba0496514`.

Related documents:

- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`;
- `2026-08-09T13-11-17-V2-A1-OBJECT-HOTT-ARTIFACT-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T19-46-59-A1-T0-RELATION-TO-HIGHER-OBSERVATIONAL-IDENTITY-AUDIT.md`;
- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
  and
- `2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`.

## 1. Objective

Reorganize the active prototype as a production-intent compiler tree without
changing its language, calculus, graph identities, typing behavior, evaluation,
artifact format, diagnostics contract, or accepted proof authority.

This stage addresses two distinct physical problems:

1. implementation, audit programs, fixtures, integration scripts, and schemas
   currently share one flat directory; and
2. several translation units contain unrelated ownership domains merely because
   the implementation grew incrementally.

The reorganization must make future work easier to locate and review. It must
not use directory movement as an opportunity to redesign the compiler.

## 2. Non-Goals

This plan does not:

- promote files from `src/prototype/` into accepted `src/` or `include/`;
- add surface `Eq`, `refl`, transport, effects, or new HOTT rules;
- change any Term, Operation, Context, Substitution, Proposition, Claim,
  Derivation, TypeDeclaration, Universe, HOTT, or artifact data model;
- renumber an enum, Term tag, proof kind, artifact field, or schema rule;
- change source grammar, parser behavior, normalization profiles, solver policy,
  fuel, residual handling, or runtime strategy;
- merge distinct proof rules because they share implementation plumbing;
- rename public APIs while moving their implementation;
- retain old paths through forwarding headers, symlinks, wrapper functions, or
  duplicate source files;
- edit generated parser or lexer output; or
- reduce LOC as a correctness objective.

Any semantic defect discovered during this work is recorded separately. It is
not fixed in the same commit as a physical move.

## 3. Baseline Audit

### 3.1 Current physical concentration

The largest active files at the baseline are:

| File | Lines | Current mixed responsibilities |
| --- | ---: | --- |
| `ast.c` | 34,187 | AST/lowering, OperationGraph, compile metadata, artifact publication, wire read/write, append/link, relocation |
| `typing.c` | 21,923 | JudgementDB storage, candidate publication, replay, classifier synthesis, conversion, Match and elimination rules |
| `hott.c` | 18,167 | relation action, object identity computation, Context bridges, object action, certificates, root extraction |
| `hott_goal_check.c` | 11,464 | all HOTT unit, integration, negative, determinism, and artifact fixtures |
| `term.c` | 10,027 | shared Term graph, interning, substitution, normalization, evaluation support |
| `read_file.c` | 4,859 | compiler inspection CLI and artifact commands |
| `type_declaration.c` | 3,449 | declaration schema, generated identity declarations, structural validation |
| `test_artifact_flow.sh` | 3,019 | build, fixtures, wire tests, linking, malformed artifact matrix |

The flat directory also contains:

- 16 `test_*.sh` integration drivers;
- focused `*_check.c` audit executables;
- surface `.p` fixtures;
- active and historical artifact schemas;
- the HOTT calculus schema; and
- `current/`, an inactive copy of an older accepted compiler snapshot.

### 3.2 Current module dependency shape

The intended semantic dependency is approximately:

```text
support
  -> core Term graph
  -> Context / Substitution / declaration structures
  -> typed Operation and Judgement kernel
  -> identity and HOTT actions
  -> artifact publication and linking
  -> frontend and command drivers
```

The current include graph does not make this visible. In particular:

- `ast.h` is consumed as a broad graph-and-artifact umbrella;
- `hott.c` includes `ast.h` because identity action and artifact root extraction
  are physically coupled;
- `typing.c` includes `ast.h` because typed Operations and kernel rules are
  physically coupled; and
- every test script repeats a raw compiler source list.

This plan exposes those boundaries physically. It does not invert or otherwise
change a dependency unless a later semantic plan explicitly approves it.

## 4. Target Layout

The target remains entirely below `src/prototype/`:

```text
src/prototype/
  Makefile
  README.md

  include/
    a_program/
      support/
      core/
      graph/
      kernel/
      identity/
      artifact/
      frontend/

  src/
    support/
    core/
    graph/
    kernel/
      rules/
      internal/
    identity/
      internal/
    artifact/
      internal/
    frontend/
    driver/

  spec/
    artifact_v70.schema
    hott_fragment_v2.schema
    archive/
      artifact_v67.schema
      artifact_v68.schema
      artifact_v69.schema
      README.md

  tests/
    integration/
    checks/
    fixtures/
      artifact/
      cbpv/
      conversion/
      effects/
      identity/
      typing/
      negative/
    support/

  build/
    sources.mk
    tests.mk
```

Headers and implementations are separated to follow `CODING_STYLE.md`.
Prototype-public declarations live under `include/a_program/`. Declarations
shared only by translation units within one module live in that module's
`internal/` directory and are not installed or included by other modules.

### 4.1 Module ownership

| Target module | Owns | Must not own |
| --- | --- | --- |
| `support` | symbols and small allocation-independent utilities | Term or proof semantics |
| `core` | shared context-erased TermDB, bindings, alpha-aware interning, normalization primitives | typed occurrence authority |
| `graph` | typed OperationGraph and compile metadata structures | kernel proof acceptance or artifact wire |
| `kernel` | ContextDB, SubstitutionDB, JudgementDB, classifier solving, conversion, type declarations, Universe constraints, CwF certificates | frontend parsing or artifact serialization |
| `identity` | compiler-local relation action, object identity computation, telescope/object action, bridges and certificates | artifact wire encoding |
| `artifact` | interface, publication closure, dense relocation, v70 read/write, append/link, identity-root persistence | proof search or HOTT action execution |
| `frontend` | `.p` reader, AST, name resolution, lowering into typed Operations and shared Terms | artifact linking or kernel rule implementation |
| `driver` | REPL and file compiler/inspection commands | reusable compiler semantics |

`APP_ELIM`, `MATCH_ELIM`, `INDUCTION_HYPOTHESIS_ELIM`, and
`COMPUTATION_FOLD_ELIM` remain distinct kernel rules. Moving them under one
`kernel/rules/` directory does not merge their theorem statements.

### 4.2 Schema ownership

`spec/artifact_v70.schema` and `spec/hott_fragment_v2.schema` are authoritative
semantic inputs. Historical artifact schemas move under `spec/archive/` and
receive an explicit non-authoritative README. They are not accepted parser
fallbacks.

`calculus.h` remains a compiler header during this stage. A later generated-file
plan may derive fingerprints from schemas, but this physical move does not add a
generator or change fingerprint computation.

### 4.3 Test ownership

Tests are divided by execution role, not by file extension alone:

- `tests/integration/`: shell entry points representing supported workflows;
- `tests/checks/`: focused C audit executables linked against compiler modules;
- `tests/fixtures/`: `.p` source and malformed-input fixture data; and
- `tests/support/`: shared shell and C helpers with no compiler authority.

No test may locate a semantic object by a fixed Term, Operation, Claim,
Derivation, Binding, or artifact slot number. Tests select objects by root role,
record kind, declaration name, or structural predicate.

## 5. Invariance Contract

Every phase must preserve all of the following.

### 5.1 Language and kernel invariants

- all existing positive sources compile;
- all existing negative sources remain rejected;
- every accepted Claim and Derivation used by a permanent fixture has the same
  proposition and ordered premises;
- no proof kind or rule payload changes;
- normalization results and completion states remain unchanged;
- residual reasons remain unchanged;
- OperationGraph remains the typed occurrence authority;
- TermDB remains shared and context-erased;
- compiler-local `RELATION_*` remains distinct from object identity; and
- no new equality, transport, or reflection rule appears.

### 5.2 ABI and representation invariants

- enum values and frozen Term/proof tags are unchanged;
- public struct size, alignment, and field offsets are unchanged during move-only
  phases;
- public function signatures are unchanged;
- public symbol inventory is unchanged, except for an explicitly recorded
  internal-linkage extraction approved in a split phase;
- artifact format remains v70;
- schema and calculus fingerprints remain unchanged; and
- no compatibility include path or duplicate implementation remains after a
  move.

### 5.3 Artifact invariants

- baseline HOTT artifact bytes remain identical;
- perturbation runs remain byte-identical;
- write/read/append/link retain the same exact root counts and proof closure;
- malformed artifact fixtures remain rejected;
- v67-v69 remain rejected by the active reader; and
- no HOTT action, bridge, work, fuel, or compiler-local relation state leaks
  into v70.

### 5.4 Build invariants

- C11 remains the language standard;
- `-Wall -Wextra -Werror` focused builds remain clean;
- ASan/UBSan focused checks remain clean;
- no generated output is edited manually; and
- the root build continues to produce the same two executable roles.

## 6. Baseline Evidence Bundle

Before the first move, capture one ignored temporary evidence directory outside
the repository containing:

1. `git rev-parse HEAD`;
2. sorted active source and test file inventory;
3. `wc -l` per file;
4. hashes of both active schemas;
5. extracted enum values for Term tags, Operation tags, judgement kinds, proof
   kinds, identity computation rules, and artifact version;
6. `sizeof`, `_Alignof`, and `offsetof` output for persistent public structs;
7. sorted global symbol inventories for `a.out`, `read_file.out`, and focused
   check binaries;
8. outputs and exit statuses of all 16 test scripts;
9. outputs and exit statuses of examples 01-07 and 09 and permanent training
   fixtures;
10. baseline v70 artifacts and their hashes; and
11. focused ASan/UBSan results.

The evidence collector itself may be added under `tests/support/`, but it must
inspect existing behavior only. It must not normalize IDs, diagnostics, or
artifact data to conceal a change.

## 7. Implementation Phases

### L0: Centralize prototype build manifests

Status: complete in the working tree

- [x] Add `src/prototype/build/sources.mk` with module-neutral source groups.
- [x] Keep focused test source groups in the same authoritative source manifest;
      a second `tests.mk` would duplicate ownership rather than centralize it.
- [x] Add a prototype-local Makefile that consumes those manifests.
- [x] Change every integration script that links compiler modules to consume the
      shared source groups rather
      than repeating compiler source lists.
- [x] Update the root Makefile after the user explicitly approved execution of
      this complete plan. It now delegates to the prototype-local Makefile.
- [x] Preserve compiler and linker flags exactly.
- [x] Capture the complete baseline evidence bundle at
      `/tmp/a-program-layout-baseline-97302c9.dm6BTK`.
- [x] Run all verification gates before committing. All 16 scripts pass, the
      baseline artifact is byte-identical, and the REPL/reader global symbol
      inventories are unchanged.

Exit gate: one authoritative prototype source manifest exists, while binaries,
tests, symbols, and artifact bytes are unchanged.

### L1: Separate tests, fixtures, and schemas

Status: complete in the working tree

- [x] Move all 16 `test_*.sh` files to `tests/integration/` with `git mv`.
- [x] Move 10 focused `*_check.c` programs to `tests/checks/`.
- [x] Classify all 36 `.p` fixtures by semantic owner and move them to the matching
      `tests/fixtures/` directory.
- [x] Move the two active schemas to `spec/`.
- [x] Move v67-v69 schemas to `spec/archive/` and add the archive README.
- [x] Update all manifest, script, fingerprint, and fixture paths atomically.
- [x] Do not rename fixture contents or test functions in this phase.
- [x] Remove every old path; do not add symlinks or forwarding scripts.
- [x] Verify the inventory contains no active test/schema file at the old root.
- [x] Run all 16 integration scripts from their new paths. All pass.
- [x] Compare the generated HOTT v70 artifact against the L0 baseline. Its hash
      remains `2d64defe38600f40e73bcb6bfa5b92e925f771783b8ea14de2641daba0496514`
      and `cmp` reports byte identity.

Exit gate: implementation files are physically separate from tests and specs,
and the baseline evidence remains identical.

### L2: Remove the inactive compiler snapshot

Status: complete in the working tree

- [x] Compare `src/prototype/current/` against Git history and the accepted
      implementation to confirm that it contains no unique active source.
- [x] Confirm no build, test, documentation command, or include references it as
      executable input.
- [x] Delete `src/prototype/current/`; do not move it to another source archive.
      Git history is the archive.
- [x] Update README references that describe it as an active prototype area.
- [x] Record removed file and line counts: 13 files and 989 physical lines.
- [x] Verify that the 11 implementation/header files were byte-identical to the
      accepted tree at parent commit `7d117e5`; the remaining two files only
      built and described that frozen snapshot.

Exit gate: there is one active prototype compiler tree and no stale source
snapshot that can be mistaken for an implementation choice.

### L3: Move cohesive implementation modules

Status: pending; blocked on L2

Move already cohesive translation units without splitting function bodies:

```text
symbol.*              -> support/
term.*                -> core/
context.*             -> kernel/
type_declaration.*    -> kernel/
universe.*            -> kernel/
cwf_certificate.*     -> kernel/
kernel_view.*         -> kernel/
reader.*              -> frontend/
ast_inspect.*         -> frontend/
repl.c, read_file.c   -> driver/
```

- [ ] Move declarations to `include/a_program/<owner>/`.
- [ ] Move implementations to `src/<owner>/`.
- [ ] Change all includes and manifests in the same commit as each module move.
- [ ] Preserve include guard values unless a separate mechanical guard-only
      commit is approved.
- [ ] Do not rename functions, structs, fields, or enum constants.
- [ ] Do not retain old headers as forwarding adapters.
- [ ] Run the complete invariant matrix after each owner group, not only after
      all files have moved.

Exit gate: cohesive modules live under explicit owners with no old-path
compatibility layer.

### L4: Decompose `ast.c` and `ast.h`

Status: pending; blocked on L3

This is the highest-priority physical split. Use mechanical extraction commits
with unchanged function bodies and call order.

Target ownership:

| Extracted area | Target |
| --- | --- |
| AST nodes, parser-facing structures | `frontend/ast.*` |
| AST lowering and source-name resolution | `frontend/lowering.*` |
| typed Operations and compile metadata | `graph/operation_graph.*`, `graph/compile_metadata.*` |
| artifact interface records | `artifact/interface.*` |
| root closure and dense publication | `artifact/publication.*` |
| v70 parser/writer | `artifact/wire_v70.*` |
| append/link/dependency resolution | `artifact/link.*` |
| relocation and canonical publication ordering | `artifact/relocation.*` |

- [ ] Inventory every function and assign exactly one owner before extraction.
- [ ] Record the pre-split call graph and public declaration inventory.
- [ ] Move one ownership group per commit.
- [ ] Introduce private headers only for declarations genuinely shared across
      extracted translation units.
- [ ] Prefix newly externalized private helpers with `prototype_internal_` or
      otherwise keep them out of prototype-public headers.
- [ ] Do not change loop order, traversal order, allocation order, error order,
      or hash-table behavior.
- [ ] Compare baseline artifact bytes after every extraction commit.
- [ ] Delete `ast.c` and the umbrella `ast.h` only after all consumers include
      their actual owner interfaces.

Exit gate: no artifact implementation remains under a frontend AST filename,
and no umbrella header is required to access unrelated graph sorts.

### L5: Decompose `typing.c` and `judgement.h`

Status: pending; blocked on L4

Target ownership:

```text
kernel/judgement_db.*
kernel/candidate_publication.*
kernel/accepted_replay.*
kernel/classifier_solver.*
kernel/conversion.*
kernel/rules/formation.*
kernel/rules/introduction.*
kernel/rules/elimination.*
kernel/rules/match.*
kernel/rules/cbpv.*
```

- [ ] Preserve the Proposition/Claim/Derivation authority model exactly.
- [ ] Preserve multiple Derivations per Claim.
- [ ] Preserve premise order and rule-specific payloads.
- [ ] Keep candidate publication and accepted replay separate.
- [ ] Keep conversion result states and normalization profiles unchanged.
- [ ] Keep each proof rule visible as a named validator.
- [ ] Extract storage/index code before rule code so rule commits remain
      reviewable.
- [ ] Do not introduce a generic eliminator rule to reduce file count.
- [ ] Delete the old translation unit only after direct includes replace the
      umbrella declaration path.

Exit gate: the kernel's storage, solver, conversion, and rule ownership are
visible without changing any accepted judgement graph.

### L6: Decompose `hott.c` and `hott.h`

Status: pending; blocked on L5

Target ownership:

```text
identity/relation_action.*
identity/identity_computation.*
identity/context_bridge.*
identity/telescope_action.*
identity/object_term_action.*
identity/action_certificate.*
identity/artifact_root_extraction.*
```

- [ ] Keep compiler-local parametric relation action physically distinct from
      object identity computation.
- [ ] Keep action/work/certificate state outside persistent artifact ownership.
- [ ] Preserve every residual boundary for unsupported dependent, higher,
      Universe, and effectful cases.
- [ ] Preserve exact request/result/certificate keys.
- [ ] Preserve the distinction between family computation and ordinary proof
      inhabitation.
- [ ] Do not add symmetry, composition, transport, or higher coherence while
      moving code.
- [ ] Move artifact root extraction under `identity/` only as the producer-side
      adapter; wire validation remains owned by `artifact/`.

Exit gate: relation infrastructure, object identity, and persistent root
adaptation have explicit physical owners and unchanged theory boundaries.

### L7: Split oversized audit executables

Status: pending; blocked on L6

Split `hott_goal_check.c` by fixture ownership without weakening the integrated
test:

```text
tests/checks/hott/test_support.*
tests/checks/hott/adt_identity.*
tests/checks/hott/pi_identity.*
tests/checks/hott/universe_scaffold.*
tests/checks/hott/higher_identity.*
tests/checks/hott/artifact_roots.*
tests/checks/hott/forgery.*
tests/checks/hott/main.c
```

Similarly divide the artifact integration script into sourced test libraries or
separate executables only if one top-level command still runs the complete
matrix.

- [ ] Keep one canonical `test_hott_goal` entry point.
- [ ] Keep one canonical `test_artifact_flow` entry point.
- [ ] Select fixtures structurally, never by fixed numeric IDs.
- [ ] Do not duplicate compiler construction helpers in each fixture file.
- [ ] Keep negative tests adjacent to the rule boundary they attack.
- [ ] Confirm the split test executes the same named checks and failure cases.

Exit gate: no individual audit source owns unrelated theory areas, while the
same complete regression matrix remains available through stable entry points.

### L8: Documentation, include, and dead-path audit

Status: pending; blocked on L7

- [ ] Update `src/prototype/README.md` to describe the production-intent module
      graph and promotion boundary.
- [ ] Update documentation links and commands to new paths.
- [ ] Search the repository for every removed path.
- [ ] Search for duplicate source files and forwarding headers.
- [ ] Verify historical schemas are not compiled or accepted.
- [ ] Verify implementation modules do not include test headers.
- [ ] Verify kernel modules do not include driver headers.
- [ ] Verify artifact modules do not execute HOTT action search.
- [ ] Record final per-directory and per-file LOC.
- [ ] Record each deleted umbrella, adapter, duplicate source list, and stale
      snapshot.

Exit gate: documentation and includes describe exactly one active module graph.

### L9: Final equivalence audit and handoff

Status: pending; blocked on L8

- [ ] Run both standard builds.
- [ ] Compile all production-intent prototype sources with
      `-Wall -Wextra -Werror`.
- [ ] Run all 16 baseline integration entry points.
- [ ] Run examples 01-07 and 09 and permanent training fixtures.
- [ ] Run focused ASan/UBSan checks.
- [ ] Compare schema hashes and calculus fingerprints with the baseline.
- [ ] Compare frozen enum/tag values with the baseline.
- [ ] Compare public layouts and symbol inventories with the baseline.
- [ ] Compare v70 artifacts byte-for-byte with the baseline.
- [ ] Run `git diff --check`.
- [ ] Review the diff with rename detection disabled and enabled: the former
      catches hidden edits, while the latter confirms intended moves.
- [ ] Record commits and push only after every gate passes.

Exit gate: the physical tree is reorganized and all recorded semantic,
representational, proof, and artifact observations match the baseline.

## 8. Commit Discipline

Each commit has one physical purpose:

1. build manifest only;
2. tests/spec move only;
3. stale snapshot removal only;
4. cohesive module move only;
5. one giant-file ownership extraction only; or
6. documentation/progress update only.

For move commits:

- use `git mv`;
- update all call sites atomically;
- do not format moved code;
- do not rename identifiers;
- do not reorder declarations;
- do not modify comments except path references;
- inspect `git diff --no-renames` to detect accidental edits; and
- run the phase gate before starting the next commit.

If a required extraction exposes a real bug, stop the layout phase, document
the bug, and fix it in a separate semantic commit with its own tests. Resume
from that new explicit baseline.

## 9. Stop Conditions

Stop and revise this plan if any phase requires:

- changing an artifact byte to make a directory move easier;
- renumbering an enum or rewriting a schema;
- retaining old paths for compatibility;
- using a fixed numeric graph ID in a relocated test;
- rebuilding Claims or Derivations from Term shape;
- merging typed Operations because their Core Terms are shared;
- moving compiler-local relation evidence into object identity;
- changing a static helper's behavior while externalizing it;
- changing allocation or traversal order without an independently approved
  determinism change;
- modifying accepted `src/`, `include/`, parser inputs, or the root Makefile
  without explicit user approval; or
- combining this work with a new language or type-theory feature.

## 10. Progress Sheet

| Phase | Status | Blocked by | Completion evidence |
| --- | --- | --- | --- |
| L0 build manifests and baseline | complete in working tree | none | centralized source list; baseline bundle; 16 scripts; symbol and artifact equality |
| L1 tests/spec separation | complete | L0 | 16 tests pass; no root-level tests/fixtures/schemas; identical artifact |
| L2 stale snapshot removal | complete | L1 | removed 13 files/989 lines; one active implementation tree |
| L3 cohesive module moves | pending | L2 | owner directories; no compatibility paths; full suite |
| L4 `ast` decomposition | pending | L3 | frontend/graph/artifact ownership; identical v70 bytes |
| L5 kernel decomposition | pending | L4 | storage/solver/rules separated; identical proof DAGs |
| L6 identity decomposition | pending | L5 | relation/object/artifact boundaries explicit |
| L7 audit decomposition | pending | L6 | stable complete test entry points |
| L8 documentation/dead-path audit | pending | L7 | no stale paths or umbrella ownership |
| L9 final equivalence audit | pending | L8 | full suite, sanitizers, ABI/schema/artifact comparison, commits |

## 11. Measurement Record

Fill this table after every completed phase. Record additions and deletions, not
only net lines.

| Phase | Commit | Added | Deleted | Moved files | Old paths removed | Test result | Artifact hash |
| --- | --- | ---: | ---: | ---: | --- | --- | --- |
| baseline | `97302c9` | 0 | 0 | 0 | none | 16 scripts pass | `2d64defe38600f40e73bcb6bfa5b92e925f771783b8ea14de2641daba0496514` |
| L0 | pending commit | 165 implementation/build | 146 | 0 | repeated compiler source lists | 16 scripts pass; symbols unchanged | `2d64defe38600f40e73bcb6bfa5b92e925f771783b8ea14de2641daba0496514` |
| L1 | pending | | | | | | |
| L2 | pending | | | | | | |
| L3 | pending | | | | | | |
| L4 | pending | | | | | | |
| L5 | pending | | | | | | |
| L6 | pending | | | | | | |
| L7 | pending | | | | | | |
| L8 | pending | | | | | | |
| L9 | pending | | | | | | |

## 12. Recommended Execution Order

```text
97302c9
  -> L0 central build manifest and baseline evidence
  -> L1 tests/spec physical separation
  -> L2 inactive snapshot deletion
  -> L3 cohesive module moves
  -> L4 AST/Operation/artifact decomposition
  -> L5 kernel decomposition
  -> L6 identity decomposition
  -> L7 audit decomposition
  -> L8 path and documentation audit
  -> L9 final equivalence audit
```

L0 through L3 provide the immediate navigational improvement. L4 through L7
address the deeper ownership problem. The latter phases must not begin merely
because directories exist; they begin only after the previous phase has a
committed equivalence record.
