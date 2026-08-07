# Context and Substitution Extraction V2-C1 Implementation Plan

Date: 2026-08-07

Status: implemented and validated

Parent plan:

- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Related completed work:

- `doc/2026-08-06T04-00-00-SCOPED-KERNEL-CONVERSION-K1-K2-PLAN.md`
- commit `8fe515d83360cfbc1aa4078653bba40c41da735c`

## 1. Objective

V2-C1 makes `ContextDB`, `SubstitutionDB`, and syntactic reindexing an explicit
compiler-semantic module instead of an accidental part of the AST elaborator.

The target dependency direction is:

```text
surface parser and AST
        |
        v
AST resolution and elaboration
        |
        v
typed OperationGraph and JudgementDB
        |
        +---- uses ----> ContextDB / SubstitutionDB
        |                       |
        |                       v
        +--------------------> TermDB
```

`ast.c` may orchestrate these services, but it must not implement the context
category. This boundary is required before HOTT equality and dependent-CBPV
reindexing are added because both must state their laws over contexts and
substitutions without depending on parser or AST state.

This migration is primarily an ownership extraction. It must preserve the
current language, term tags, artifact format, context IDs, substitution IDs,
normalization behavior, and accepted/rejected examples.

## 2. Baseline

The plan is pinned to:

- branch: `main`;
- commit: `8fe515d83360cfbc1aa4078653bba40c41da735c`;
- artifact header: `A_PROGRAM_ARTIFACT 61`;
- `src/prototype/context.h` already contains the public data structures and
  most declarations;
- implementations are split incorrectly between `ast.c` and `typing.c`;
- no `src/prototype/context.c` exists;
- the root `Makefile` and several test scripts enumerate source files
  explicitly.

Record a fresh baseline immediately before implementation:

- [x] `git status --short --branch` is recorded.
- [x] `git rev-parse HEAD` still identifies the intended starting point, or
  the new starting commit is written into this section.
- [x] `make clean && make && make reader` passes.
- [x] every `src/prototype/test_*.sh` script passes.
- [x] examples 01-07 and 09 compile with the freshly built reader.
- [x] one representative artifact is saved for byte-for-byte comparison after
  the extraction.

## 3. Semantic Boundary

### 3.1 ContextDB

A context is an object of the current syntactic CwF. Context zero is empty and
every other context is an immutable extension:

```text
Gamma.A --p_A--> Gamma
```

The stored entry contains:

- the parent context;
- the canonical binder ID for the extension;
- either a resolved classifier term or an unresolved classifier variable;
- the cached depth.

`prototype_context_extend` currently interns extensions by parent and
classifier, not by the caller's proposed binder ID. Therefore two alpha-renamed
extensions can share one context entry. This is intentional behavior for this
migration and must be covered by a characterization test.

ContextDB is not:

- a runtime environment;
- a namespace table;
- an AST lexical-scope stack;
- an object-equality proof store;
- a container for effectful runtime results.

### 3.2 SubstitutionDB

The orientation remains:

```text
sigma : Delta -> Gamma
```

It assigns a term in `Delta` to every variable declared by `Gamma`. The
constructors represent:

```text
id_Gamma                    : Gamma -> Gamma
empty_Delta                 : Delta -> empty
p                           : Gamma.A -> Gamma
<sigma, t>                  : Delta -> Gamma.A
sigma o tau                 : Theta -> Gamma
```

`prototype_term_reindex(term, sigma)` is contravariant substitution over the
shared TermDB. It may intern fresh and substituted TermDB nodes. That is static
graph construction, not insertion of a runtime environment into TermDB.

### 3.3 Typed validation dependency

`prototype_substitution_extend` currently checks that the supplied term
classifier is convertible to the reindexed target classifier. It therefore
uses the typing/kernel conversion service.

V2-C1 will preserve this behavior. The initial `context.c` may consequently
depend on the narrow judgement conversion API. This is an acknowledged
dependency, not evidence that AST owns substitution. Splitting structural
substitution construction from typed proof validation is deferred to V2-P1,
where proof premises and typed residuals can be represented correctly.

### 3.4 Higher-equality dependency

Future observational equality must be stable under reindexing:

```text
Gamma |- p : Eq_A(a, b)    sigma : Delta -> Gamma
--------------------------------------------------
Delta |- p[sigma] : Eq_{A[sigma]}(a[sigma], b[sigma])
```

V2-C1 does not add this rule. It establishes the module and tests on which
V2-T1, V2-T2, V2-S1, and V2-O1 can state and implement it.

## 4. Current Ownership Audit

### 4.1 Functions implemented in `ast.c` that belong in `context.c`

The following groups are generic CwF/substitution operations and have no
semantic ownership in AST elaboration:

| Current area | Functions | Target owner |
| --- | --- | --- |
| `ast.c:17-129` | ContextDB init, empty, get, extend, binder containment, validation | `context.c` |
| `ast.c:254-302` | context artifact relocation | `context.c` |
| `ast.c:303-650` | SubstitutionDB storage, constructors, composition, validation | `context.c` |
| `ast.c:651-819` | substitution lookup and `prototype_term_reindex` | `context.c` |
| `ast.c:21379-21411` | context extension path | `context.c` |
| `ast.c:21412-21519` | fresh reindexed extension | `context.c` |
| `ast.c:21520-21583` | substitution from argument terms | `context.c` |
| `ast.c:21584-21667` | telescope-entry classifier reindexing | `context.c` |

The line numbers describe the baseline and will move during implementation.
Function names, not old line numbers, are the authoritative migration list.

### 4.2 Constructor functions currently misplaced in `ast.c`

These functions inspect `TypeDeclarationDB` constructor records and derived
curried-classifier caches:

- `prototype_constructor_telescopes_validate`;
- `prototype_constructor_curried_caches_validate`;
- `prototype_constructor_curried_caches_rebuild`.

They are not ContextDB primitives. Move their implementations to
`type_declaration.c` and their declarations from `context.h` to
`type_declaration.h`. They may consume ContextDB through the existing forward
declaration in `type_declaration.h`.

Putting these routines into `context.c` would merely move the existing
ownership error to a new file.

### 4.3 Typing-owned helper declared as a context primitive

`prototype_context_instantiate_pure_family` is implemented in `typing.c` and is
used only there. It performs a typing operation: create the family context,
check/extend a typed substitution, and instantiate a pure family.

During V2-C1:

- remove it from `context.h`;
- make the implementation private to `typing.c`;
- rename it to `instantiate_pure_family_in_context` if that name remains
  accurate at implementation time;
- do not leave a compatibility wrapper.

This keeps `context.h` declarative and prevents a typing rule from appearing to
be a primitive structural CwF operation.

### 4.4 Artifact relocation asymmetry

Context relocation is public, but substitution relocation is currently a
`static` helper in `ast.c`. Artifact append needs both operations.

Add this compiler-internal API to `context.h` and implement it in `context.c`:

```c
int prototype_substitution_db_append_relocated(
	struct prototype_substitution_db* target,
	const struct prototype_substitution_db* source,
	const uint32_t* context_relocation,
	size_t context_relocation_count,
	uint32_t term_offset,
	uint32_t proof_offset
);
```

The artifact orchestrator in `ast.c` remains responsible for computing all
offsets and ordering the complete artifact append. `context.c` is responsible
only for relocating the internal references of context and substitution
records.

### 4.5 AST-specific code that must remain in `ast.c`

Do not move:

- parser AST allocation and source spans;
- name and namespace resolution;
- `compile_context` and pending-resolution worklists;
- OperationGraph construction;
- Match-case lowering;
- artifact section ordering and whole-artifact append orchestration;
- compile phase scheduling;
- diagnostics and source-level readback.

The extraction is complete only when these routines call ContextDB and
SubstitutionDB APIs without reimplementing their invariants.

## 5. Target File and Dependency Layout

### 5.1 `context.h`

Owns only:

- ContextDB and SubstitutionDB data declarations;
- structural and typed substitution APIs currently required by clients;
- reindexing APIs;
- context/substitution relocation APIs;
- context telescope traversal helpers.

It must continue to use forward declarations for TermDB and
TypeDeclarationDB. It must not include `ast.h` or expose AST types.

### 5.2 `context.c`

Owns:

- ContextDB storage and validation;
- SubstitutionDB storage and validation;
- substitution constructors and composition;
- syntactic term reindexing and private substitution lookup;
- context/substitution relocation;
- generic extension-path and telescope reindexing helpers.

Expected includes are limited to:

```text
context.h
term.h
type_declaration.h
judgement.h     temporary narrow dependency for typed extension validation
stdlib/string standard headers as actually required
```

It must not include `ast.h`, `reader.h`, or parser/lexer headers.

### 5.3 `type_declaration.c/.h`

Own constructor telescope and curried-cache validation/rebuild. This module
already owns the classifier derivation used by those routines.

### 5.4 `typing.c`

Own the private pure-family instantiation helper and all rules that create or
consume JudgementDB proof evidence.

### 5.5 `ast.c`

After extraction, an ownership check must find no definitions matching:

```text
^prototype_context_
^prototype_substitution_
^prototype_term_reindex$
^prototype_constructor_telescopes_validate$
^prototype_constructor_curried_caches_
```

Calls are expected; definitions are not.

## 6. Implementation Phases

### C1-0: Freeze behavior and dependency inventory

Status: [x] complete

Tasks:

- [x] Record the baseline commit and test results.
- [x] Enumerate every definition and call site of the migration API.
- [x] Record all explicit compile/link source lists.
- [x] Save one artifact produced by the baseline compiler.
- [x] Add characterization assertions for context interning by
  parent/classifier and canonical binder reuse.
- [x] Add characterization assertions for identity, projection, extension,
  composition, and dependent classifier reindexing.

Exit criteria:

- [x] Existing behavior is represented by tests before functions move.
- [x] No known caller depends on an undeclared `static` implementation detail.

### C1-1: Create `context.c` and move storage primitives

Status: [x] complete

Tasks:

- [x] Create `src/prototype/context.c`.
- [x] Move ContextDB init/get/extend/contains/validate without behavior change.
- [x] Move SubstitutionDB init/get/private-add/constructors/compose/validate.
- [x] Keep private helpers `static`.
- [x] Remove the old definitions from `ast.c` in the same change.
- [x] Preserve all public names that remain semantically valid.
- [x] Remove accidental duplicate statements encountered at the extraction
  boundary, including duplicate `return` or repeated no-op expression lines;
  do not combine this with semantic rewrites.

Exit criteria:

- [x] No duplicate symbol or unresolved symbol occurs.
- [x] Context category checks pass.
- [x] `context.c` has no AST dependency.

### C1-2: Move reindexing and generic context traversal

Status: [x] complete

Tasks:

- [x] Move private substitution lookup and `prototype_term_reindex`.
- [x] Move context extension-path traversal.
- [x] Move fresh reindexed extension construction.
- [x] Move substitution construction from term argument lists.
- [x] Move telescope-entry classifier reindexing.
- [x] Preserve the current capture-avoidance sequence: temporary fresh binders
  first, substitution lookup second, final replacement third.
- [x] Preserve TermDB interning of static reindexing results.
- [x] Document that reindexing may grow TermDB but never stores runtime state.

Exit criteria:

- [x] Reindexing under nested dependent binders produces the same alpha-local
  term as at baseline.
- [x] Identity reindexing returns the original term ID.
- [x] Composition reindexing is definitionally convertible to sequential
  reindexing under the same conversion profile.
- [x] No AST object is required to call reindexing.

### C1-3: Correct neighboring ownership boundaries

Status: [x] complete

Tasks:

- [x] Move constructor telescope validation to `type_declaration.c/.h`.
- [x] Move constructor curried-cache validate/rebuild to
  `type_declaration.c/.h`.
- [x] Remove the public pure-family instantiation declaration from
  `context.h`.
- [x] Make the pure-family instantiation helper private to `typing.c` and
  rename it for its actual typing role.
- [x] Add the substitution relocation API to `context.h`.
- [x] Move substitution relocation into `context.c`.
- [x] Keep whole-artifact append orchestration in `ast.c`.

Exit criteria:

- [x] `context.h` contains no constructor-cache or typing-rule declarations.
- [x] `type_declaration.h` owns constructor validation declarations.
- [x] Artifact append calls both relocation APIs through declared interfaces.

### C1-4: Update build and test linkage

Status: [x] complete; the implementation request approved the planned build edit

The repository policy permits prototype implementation changes under
`src/prototype/`, but editing the accepted root `Makefile` requires explicit
user approval. Obtain that approval during implementation before changing it.

Tasks after approval:

- [x] Add `src/prototype/context.c` to the `all` target.
- [x] Add `src/prototype/context.c` to the `reader` target.
- [x] Add it to every compile command in `test_artifact_flow.sh`.
- [x] Add it to `test_cbpv_boundary.sh`.
- [x] Add it to `test_context_category.sh`.
- [x] Add it to `test_conversion_result.sh`.
- [x] Add it to `test_conversion_scope.sh`.
- [x] Add it to `test_term_identity_frame.sh`.
- [x] Re-run `rg -l 'src/prototype/ast\.c' src/prototype/test_*.sh` and verify
  every standalone link requiring AST semantics also links `context.c`.
- [x] Do not edit generated parser or lexer output.

Exit criteria:

- [x] `make` and `make reader` link without relying on accidental inclusion.
- [x] Every standalone check links the new implementation exactly once.

### C1-5: Strengthen module-level tests

Status: [x] complete

Extend `context_category_check.c` or add narrowly named checks for:

- [x] empty-context uniqueness and malformed-empty rejection;
- [x] extension parent/depth invariants;
- [x] alpha-renamed extension interning and canonical binder recovery;
- [x] resolved classifier versus unresolved classifier-variable distinction;
- [x] identity left and right laws, tested through reindexing;
- [x] substitution composition associativity, tested through reindexing;
- [x] projection/extension behavior;
- [x] rejection of source/target context mismatch;
- [x] dependent classifier reindexing;
- [x] fresh extension without binder capture;
- [x] context relocation of binder and term references;
- [x] substitution relocation of context, term, classifier, and proof IDs;
- [x] malformed forward/cyclic substitution references rejected by validation;
- [x] artifact write/read/link preserving context-indexed judgements.

The laws are tested by typed/reindexed behavior where raw substitution IDs are
not expected to canonicalize to the same node.

Exit criteria:

- [x] Tests fail if ContextDB/SubstitutionDB functions are removed from the
  build.
- [x] Tests distinguish structural validity from typed proof validity.
- [x] No test identifies substitutions solely because integer IDs happen to
  match.

### C1-6: Full regression and artifact parity

Status: [x] complete

Tasks:

- [x] Run `make clean && make && make reader`.
- [x] Run every `src/prototype/test_*.sh` script.
- [x] Compile examples 01-07 and 09.
- [x] Run negative recursive-motive and context-mismatch examples.
- [x] Compare the saved artifact with the post-extraction artifact.
- [x] Confirm the artifact header remains v61.
- [x] Run `git diff --check`.
- [x] Run ownership searches listed in Section 5.5.
- [x] Inspect `git diff --stat` and ensure no accepted implementation or
  generated file changed unexpectedly.

Exit criteria:

- [x] All tests pass.
- [x] Artifact bytes are unchanged, or every byte difference is explained and
  proven semantically neutral. An unexplained difference blocks completion.
- [x] No artifact schema or tag number changed.
- [x] `ast.c` no longer defines context/substitution semantics.

### C1-7: Documentation and completion record

Status: [x] complete

Tasks:

- [x] Update this phase sheet incrementally as each phase completes.
- [x] Record final commit IDs and exact test commands.
- [x] Update V2-C1 in the parent plan from `planned` to `complete` only after
  C1-6 passes.
- [x] Record any newly discovered semantic defect as a separate follow-up;
  do not silently broaden V2-C1.

Exit criteria:

- [x] The parent plan and this detailed plan agree on status.
- [x] V2-T1/T2 can cite `context.h` as the stable substitution boundary.

## 7. Test Matrix

| Area | Primary check | Required result |
| --- | --- | --- |
| Context object laws | `test_context_category.sh` | pass |
| Substitution/reindex laws | expanded context category check | pass |
| Typed CBPV indexing | `test_cbpv_boundary.sh` | pass |
| Kernel conversion consumers | `test_conversion_result.sh`, `test_conversion_scope.sh` | pass |
| Match/IH alpha scope | `test_term_identity_frame.sh` | pass |
| Artifact relocation and link | `test_artifact_flow.sh` | pass, v61 unchanged |
| Full prototype | every `test_*.sh` | pass |
| Surface regression | examples 01-07 and 09 | compile |

## 8. Known Risks and Decisions

### 8.1 Hidden fixed-size scratch limits

The baseline contains local path/reindex arrays with limits such as 128 and
512, while database capacities are larger. These are real scalability debts.
They are not to be changed during the initial move because doing so would mix
algorithmic behavior with ownership extraction.

After C1 parity is established, create a separate follow-up to derive scratch
space from context depth or an explicit caller-owned arena. Capacity exhaustion
must remain a reported failure, not memory corruption or silent truncation.

### 8.2 Context interning ignores proposed binder identity

This is deliberate alpha-canonical behavior in the current model. Callers must
read the canonical binder from the returned context entry. Changing the
interning key to include binder ID would multiply alpha-equivalent contexts and
is outside V2-C1.

### 8.3 Validation is not a complete proof checker

`prototype_substitution_db_validate` checks structural references but does not
fully replay `term_proof_id` or every classifier judgement. V2-C1 must document
this limit and preserve the existing typed construction checks. Full proof
payload replay belongs to V2-P1.

### 8.4 Context classifier variables are solver references

An unresolved `classifier_variable` is not a TermDB ID. Context structural
validation cannot validate it against `term_count`. Its ownership must remain
with the classifier solver until V2-S1 defines typed HOTT constraints.

### 8.5 Relocation order matters

Contexts are relocated before substitutions because substitutions refer to
context IDs. Both are relocated before judgement proofs that consume their
IDs. V2-C1 must preserve this order in artifact append.

### 8.6 No compatibility shims

The repository does not require prototype backward compatibility. Remove moved
definitions and obsolete declarations. Do not leave forwarding wrappers in
`ast.c` or duplicate declarations in unrelated headers.

## 9. Explicit Non-Goals

V2-C1 must not:

- add `Eq`, path, transport, interval, dimension, or coherence terms;
- define Higher Observational Type Theory rules;
- change Match alpha identity or conversion;
- change normalization profiles;
- add solver goal kinds;
- redesign `term_proof_id`;
- implement dynamic dependent Bind;
- change effect-row semantics;
- change artifact v61;
- move runtime environments into TermDB;
- promote prototype code into accepted `src/` or `include/`.

## 10. Completion Checklist

- [x] C1-0 baseline frozen.
- [x] C1-1 storage primitives extracted.
- [x] C1-2 reindexing and traversal extracted.
- [x] C1-3 neighboring ownership corrected.
- [x] C1-4 build linkage updated with required approval.
- [x] C1-5 law-level tests expanded.
- [x] C1-6 full regression and artifact parity passed.
- [x] C1-7 documentation finalized.
- [x] Parent V2-C1 status marked complete.
- [x] Final implementation commit recorded: `77083ea`.

V2-C1 is complete only when every item above is checked. Creating `context.c`
without the ownership, relocation, law, and artifact checks is not completion.

## 11. Follow-Up Order

After V2-C1:

1. finish the normative V2-T1 finite typed HOTT fragment over the shared
   TermDB;
2. finish the V2-T2 dependent-CBPV boundary and substitution laws without
   duplicating Pi/Lambda/APP/Match graph constructors;
3. implement V2-S1 typed HOTT solver indices;
4. implement V2-P1 extensible proof records and premise arena;
5. implement V2-O1 type-directed observational action over shared terms;
6. perform the coordinated V2-A1 artifact v62 migration.

Do not add equality tags before V2-T1/T2 can state how those tags reindex over
the ContextDB/SubstitutionDB boundary established here.

The combined V2-T1/T2 plan is
`doc/2026-08-07T01-00-00-SHARED-TERM-HOTT-DCBPV-V2-T1-T2-PLAN.md`.

## 12. Implementation Evidence

Implemented ownership boundary:

- `src/prototype/context.c` now owns ContextDB, SubstitutionDB, relocation,
  term reindexing, and generic telescope traversal;
- `src/prototype/type_declaration.c/.h` own constructor telescope and curried
  classifier cache validation;
- `src/prototype/typing.c` owns the private
  `instantiate_pure_family_in_context` typing helper;
- `src/prototype/ast.c` retains artifact orchestration and elaboration but no
  longer defines context/substitution semantics.

Implementation commit: `77083ea` (`refactor: extract context and substitution
semantics`).

Validation commands:

```text
make clean && make && make reader
for test_script in src/prototype/test_*.sh; do sh "$test_script"; done
sh src/prototype/test_context_category.sh
sh src/prototype/test_term_identity_frame.sh
git diff --check
```

All 13 prototype test scripts passed. Examples 01-07 and 09 compiled with the
fresh reader.

Artifact parity evidence:

```text
baseline SHA-256: 1211bf6bac81a4fcf27f0db409b9f329ea6072d1da34bdac8c48ec85163fe4ad
post-C1 SHA-256:  1211bf6bac81a4fcf27f0db409b9f329ea6072d1da34bdac8c48ec85163fe4ad
size:             7880 bytes
artifact header:  A_PROGRAM_ARTIFACT 61
```

The extraction exposed two existing implementation details without changing
their semantics:

1. reindexing has fixed local scratch limits of 128/512 entries;
2. typed substitution extension still calls kernel classifier conversion.

The first is recorded as a bounded-scratch follow-up. The second remains until
V2-P1 can separate structural substitution data from replayable typing proof
premises without weakening the current check.
