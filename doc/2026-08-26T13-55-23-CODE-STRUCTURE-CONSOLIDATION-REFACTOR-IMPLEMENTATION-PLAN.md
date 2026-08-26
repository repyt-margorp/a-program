# Code Structure Consolidation Refactor Implementation Plan

Date: 2026-08-26 13:55:23 JST

Status: complete; CR1-CR7 implemented, CR8-CR10 resolved by explicit negative
decision, CR11 validated

Baseline commit: `fd546e1` (`main`)

Source audit:
`doc/2026-08-26T13-16-55-CURRENT-CODE-STRUCTURE-AND-DUPLICATION-AUDIT.md`

## 1. Objective

Refactor the current prototype implementation so that one structural fact is
described once, pure graph algorithms operate through narrow immutable views,
and physical modules expose explicit ownership boundaries.

The refactor must preserve A Program semantics, checked-Core independence,
artifact bytes unless an explicit format change is approved, and the current
surface language. It must remove old APIs when their replacement is complete;
compatibility wrappers are not retained merely to reduce migration work.

This plan is about implementation structure. It does not introduce a new
calculus, Equality rule, effect rule, surface syntax, or artifact acceptance
rule.

## 2. Critical Review of the Source Audit

The source audit is directionally correct, but several findings require narrower
implementation scopes than their first wording suggests.

### 2.1 Accepted findings

The following findings identify concrete duplication and are adopted:

| Finding | Decision |
| --- | --- |
| D-001 Term reference inventory | Adopt a shared semantic field/reference schema; do not merge evaluation rules |
| D-002 immutable structural views | Adopt for representation and Context/Substitution structure; keep checker conversion acceptance independent initially |
| D-003 duplicate merge images | Adopt one canonical image owner while preserving effort-accounted conflict checks |
| D-004 v87 specification gap | Adopt a checked-container schema and dual-format documentation; correct the claim that every v86 README reference is stale |
| D-005 alpha-aware traversal | Adopt shared binder/environment machinery first; full traversal unification requires a decision gate |
| D-006 checked projection inventory | Adopt only after the Term schema proves the approach; do not create one universal schema for unrelated records |
| D-007 checker session organization | Adopt after immutable structural APIs exist |
| D-008 include-forest organization | Adopt incrementally after internal interfaces are explicit |
| D-009 broad TypeDeclaration capability | Adopt an explicit TypeView rebuild capability |
| D-014 hash plumbing | Optional low-priority utility extraction with domain separation |
| D-015 driver organization | Adopt after semantic packages; no storage redesign |

### 2.2 Corrected v86/v87 interpretation

The current repository has two different persistent boundaries:

- artifact v86 is the accepted Claim/Derivation proof-graph artifact governed by
  `src/prototype/spec/artifact_v86.schema`; and
- checked artifact v87 is the independently checked semantic container governed
  by `src/prototype/src/checker/container.c`.

PR22 intends v86 eventually to become a migration oracle for roots not yet
admitted by checked-Core, but that target state does not make all current v86
documentation obsolete. The implementation must document both formats and
their authorities. It must not globally replace `v86` with `v87`.

The checked v87 format needs its own schema. Moving
`src/prototype/spec/artifact_v83.schema` to `spec/archive/` is valid because no
current implementation references it, but this move is independent of v86
retirement.

### 2.3 Narrowed checker sharing rule

The checker may share:

- immutable record access;
- Term reference-role enumeration;
- Context ancestry and structural path access;
- Substitution record decoding;
- grade algebra; and
- bounded, side-effect-free utility data structures.

The first migration must not share:

- producer Claims or Derivations;
- solver results or normalization caches;
- producer acceptance decisions;
- checker capability minting;
- resource-usage acceptance;
- Universe constraint collection; or
- the complete producer conversion algorithm.

Checker-local DefEq is deliberately retained through the first structural
migration. A later package may share a pure comparison kernel only after
differential tests prove that the checker remains non-circular.

### 2.4 Narrowed schema rule

The target is not one reflective runtime schema for every C structure. That
would hide calculus rules and make wire formats depend accidentally on memory
layout.

Use small schemas only where at least three consumers need the same field
inventory:

- Core Term fixed payload references and auxiliary slices;
- checked semantic root/reference closure; and
- checked record relocation coverage.

Persistent codecs keep explicit field order. They must assert complete schema
coverage, but they are not generated from `offsetof()` or native struct layout.

### 2.5 Negative-decision ledger

| Finding | Required disposition |
| --- | --- |
| D-010 resource usage | Keep producer solving and checker reconstruction separate; share grade algebra and structural readers only |
| D-011 Universe closure | Keep accepted-evidence closure and checker reconstruction separate; a low-level graph solver may be shared only with separate input collection and acceptance |
| D-012 accepted replay/checker | Preserve two acceptance paths; sharing is limited to record access and pure utilities |
| D-013 Function Graph/type declarations | Preserve generated-package validation and ordinary semantic schema ownership as distinct layers |
| D-014 hashes | Preserve domain identities; extract byte mechanics only if it reduces code without changing persisted output |

Permanent static tests or dependency checks must protect D-010 through D-013.
They are completed by proving the separation, not by deleting one side.

## 3. Invariants and Non-Goals

### 3.1 Semantic invariants

- Core Term identity remains independent of typed occurrence, Context, source
  AST identity, and proof evidence.
- Value/computation polarity remains typing information over one Core graph; no
  duplicate Value-Term and Computation-Term node families are introduced.
- `ContextDB` remains the Context-object store and `SubstitutionDB` remains the
  Context-morphism store.
- Proposition, Claim, and Derivation remain distinct proof objects.
- TypeDeclaration semantic schema, readback, representation, and classifier
  cache remain distinct capabilities.
- `APP`, `MATCH`, induction-hypothesis elimination, and computation fold retain
  explicit kernel rules.
- operation request remains distinct from `APP`.
- DefEq, parametricity relation action, and object Identity remain distinct.
- `::` remains post-synthesis expectation checking.
- compiler computation and runtime computation remain one computation model
  separated by permitted advance and residualization, not separate meanings for
  one Term.

### 3.2 Trust invariants

- The independent checker never consumes Claim, Derivation, solver, cache, or
  producer-success state.
- Checked capabilities remain process-local and cannot be deserialized.
- v87 optional producer/debug sections never influence checked authority.
- v86 accepted replay and checked-Core checking remain independent acceptance
  paths during migration.
- Resource and Universe reconstruction remain independently checked.

### 3.3 Non-goals

- no surface grammar change;
- no artifact semantic-format change in this refactor;
- no compatibility parser or compatibility wrapper added;
- no generalized reflection framework;
- no attempt to encode all eliminators as Lambda/APP;
- no merging of Function Graph metadata into TypeDeclaration authority;
- no HOTT fragment expansion;
- no source scheduler or classifier-capsule implementation; and
- no file split whose only justification is lower line count.

## 4. Progress Dashboard

| Package | Scope | Depends on | Status |
| --- | --- | --- | --- |
| CR0 | Baseline, byte fixtures, counters, and source inventory | none | Complete |
| CR1 | Dual artifact specification and documentation | CR0 | Complete |
| CR2 | Core Term semantic field/reference schema | CR0, CR1 | Complete |
| CR3 | Immutable Term/Context/Substitution structural readers | CR2 | Complete |
| CR4 | checked semantic closure and relocation consolidation | CR2, CR3 | Complete |
| CR5 | Canonical checked-module image ownership | CR1 | Complete |
| CR6 | Explicit TypeView rebuild capability | CR3 | Complete |
| CR7 | Alpha binder/environment toolkit | CR2, CR3 | Complete |
| CR8 | checker module decomposition | CR3, CR4, CR6 | Complete: negative decision |
| CR9 | lowering/kernel/HOTT translation-unit boundaries | CR2, CR3, CR7 | Complete: negative decision |
| CR10 | driver and low-level utility organization | CR5, CR8 | Complete: negative decision |
| CR11 | Integrated validation, LOC, performance, and documentation | CR1-CR10 | Complete |

## 5. Dependency Graph

```text
CR0
 |\
 | CR1 -------- CR5
 |  |
 CR2 --------- CR4
  | \           |
  |  CR7        CR8
  | /          / |
 CR3 ---------   |
  | \             |
 CR6 ------------ |
  |                |
  +------ CR9 -----+
           |
          CR10
           |
          CR11
```

CR1 is performed before code that can change v87 bytes. CR2 and CR3 establish
the vocabulary required by CR4 and CR8. File decomposition is deliberately
downstream of semantic/API consolidation.

## 6. CR0: Baseline and Change Control

### 6.1 Required records

- [x] Record `git rev-parse HEAD`, branch, and clean/dirty status.
- [x] Record all current untracked documentation separately from implementation
      changes.
- [x] Record C/header/include/parser line counts for every affected file.
- [x] Record current v86 and v87 canonical bytes for representative fixtures.
- [x] Record the current full integration-suite test count and wall time.
- [x] Record benchmark medians after one warm-up for IF8 and Function Graph
      QuickSort. The repository benchmark contract uses three measured runs,
      not the five runs originally proposed here; that executable contract was
      retained so baseline and final samples remain comparable.
- [x] Add or enable counters for v87 module serialization count, checker Term
      visits, Context ancestry queries, Substitution image queries, and semantic
      relocation visits.

### 6.2 Baseline commands

```sh
git status --short
git rev-parse HEAD

sh src/prototype/tests/run_integration_suite.sh \
  --timing-output /tmp/a-program-cr0-integration.timing

sh src/prototype/tests/performance/benchmark_if8_single_compile.sh
sh src/prototype/tests/performance/benchmark_function_graph_quicksort_single_compile.sh
```

The exact benchmark invocation and environment are copied into the progress
record. Medians from different compiler binaries or build flags are not mixed.

### 6.3 Change discipline

- [ ] Implement one CR package per reviewable commit.
- [x] Do not retain old and new internal APIs after all callers migrate.
- [x] Do not alter artifact bytes silently.
- [x] If a package changes v86 or v87 bytes, stop and create a separate format
      proposal before continuing.
- [x] Record per-file added, deleted, and net lines for the integrated change.
- [x] Record performance counters before and after the P1 implementation.

## 7. CR1: Dual Artifact Specification

### 7.1 Problem

v86 has a semantic manifest and fingerprint consistency test. v87 has an
implemented reader/writer, magic `APCHK087`, independently hashed sections, and
fresh checking on import, but no standalone schema. Documentation does not
consistently explain that v86 and v87 currently answer different authority
questions.

### 7.2 Files

Create or modify:

- `src/prototype/spec/checked_artifact_v87.schema`;
- `src/prototype/spec/archive/artifact_v83.schema` by moving the current file;
- `src/prototype/spec/archive/README.md`;
- `README.md`;
- `src/prototype/README.md`;
- `src/prototype/tests/integration/test_spec_consistency.sh`; and
- a small checked-v87 enum/schema consistency C test if shell checks cannot
  compare all persisted enum values reliably.

### 7.3 v87 schema content

The schema must state exactly:

- 8-byte magic and little-endian integer encoding;
- format version 87;
- section count and legal section kinds;
- required semantic and contract sections;
- optional producer and debug sections;
- duplicate-section rejection;
- section length and FNV-1a section hash;
- section size/count limits;
- exact semantic field order;
- exact contract field order;
- canonical ordering requirements;
- checked capabilities are never serialized;
- producer/debug sections have no authority;
- reading always reconstructs an untrusted module and runs the checker; and
- capsule extraction does not mint checked authority.

### 7.4 Documentation wording

Use explicit names:

```text
accepted proof artifact v86
checked semantic container v87
```

Do not call both simply “the current artifact”. State the current compiler path
for each and the condition under which v86 can later be retired: every published
root required by the language must have an admitted checked-Core representation
and rule, including currently deferred Identity/HOTT roots.

### 7.5 Tests

- [x] Schema version equals `PROTOTYPE_CHECKED_ARTIFACT_VERSION`.
- [x] Magic and section ordinals equal the implementation.
- [x] Every persisted Term tag and checked semantic enum appears in the schema.
- [x] Required-section omission is rejected.
- [x] duplicate, unknown, oversized, truncated, and hash-mismatched sections are
      rejected.
- [x] producer/debug erasure preserves checked exports.
- [x] canonical write/read/write bytes remain identical.
- [x] README checks permit documented v86 and v87 only in their named roles.

### 7.6 Exit gate

- [x] No executable v87 field lacks a schema entry.
- [x] No document describes v87 bytes as carrying checked authority.
- [x] v86 remains supported until its explicit retirement condition is met.
- [x] v83 is archived and has no current implementation reference.

## 8. CR2: Core Term Semantic Field and Reference Schema

### 8.1 Problem

Term tag payload knowledge is repeated in Core child enumeration, free-binding
walks, checked validation, checked closure, relocation, v86/v87 codecs,
canonicalization, substitution, and resource analysis. The existing child API
enumerates computational children, not all semantic references.

### 8.2 Selected abstraction

Introduce a small Core schema that describes fixed Term payload fields without
encoding evaluation rules or native struct serialization.

Proposed files:

- `src/prototype/include/a_program/core/term_schema.h`;
- `src/prototype/src/core/term_schema.c`; and
- optionally one private declarative `.def` file included only by
  `term_schema.c` and exhaustive tests.

Proposed concepts:

```c
enum prototype_term_field_kind {
    PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
    PROTOTYPE_TERM_FIELD_TERM_OPTIONAL,
    PROTOTYPE_TERM_FIELD_BINDING,
    PROTOTYPE_TERM_FIELD_IH_SCOPE,
    PROTOTYPE_TERM_FIELD_CASE_SLICE,
    PROTOTYPE_TERM_FIELD_FOLD_CLAUSE_SLICE,
    PROTOTYPE_TERM_FIELD_TYPE_DECLARATION,
    PROTOTYPE_TERM_FIELD_REPRESENTATION,
    PROTOTYPE_TERM_FIELD_SYMBOL,
    PROTOTYPE_TERM_FIELD_OPERATION,
    PROTOTYPE_TERM_FIELD_UNIVERSE_LEVEL,
    PROTOTYPE_TERM_FIELD_SCALAR
};
```

Field roles are more precise than field kinds. For example, constructor owner,
APP function, APP argument, TypeView core/source, effect label, and result type
remain distinguishable.

The API must support:

- exhaustive tag recognition;
- immutable enumeration of fixed payload fields;
- required versus optional references;
- rewriting reference fields through typed relocation maps;
- validation of auxiliary case/fold/IH ranges through caller-supplied graph
  extents; and
- filtering computational children without classifying every semantic reference
  as a reduction child.

### 8.3 Deliberate exclusions

The schema does not define:

- beta, iota, CBPV, or host reduction;
- typing premises;
- alpha-equivalence;
- wire byte order;
- resource grades;
- Universe constraints; or
- whether a checked capability is minted.

### 8.4 Migration order

- [ ] Inventory every tag from `PROTOTYPE_TERM_VAR` through
      `PROTOTYPE_TERM_TAG_MAX`, including reserved holes 35 and 36.
- [ ] Add schema coverage without changing any caller.
- [ ] Make `prototype_term_child_count()` and `prototype_term_child_at()` use
      schema roles for fixed children while retaining explicit Match/fold slice
      expansion.
- [ ] Migrate checked closure marking to the semantic reference visitor.
- [ ] Migrate checked Term relocation to typed reference rewriting.
- [ ] Migrate structural reference validation.
- [ ] Keep v86/v87 codecs explicit, but add assertions/tests that every schema
      tag has an exact codec case and vice versa.
- [ ] Remove the replaced closure/relocation tag switches in the same package.

### 8.5 Tests

- [ ] Extend `test_term_child_roles.sh` for every computational role.
- [ ] Add one all-tags schema test that fails for an unregistered enum value.
- [ ] Build nontrivial IDs so zero-valued fields cannot accidentally pass.
- [ ] Test constructor owner as a semantic reference but not a computational
      child.
- [ ] Test Match case slices, binders, constructor owners, and IH scope links.
- [ ] Test computation-fold operation/body slices.
- [ ] Test optional and required invalid IDs independently.
- [ ] Compact and relocate an all-tags graph, then compare semantic records.
- [ ] Round-trip the same graph through v87 and applicable v86 tags.

### 8.6 Exit gate

- [ ] Exactly one Core function owns fixed Term reference-field classification.
- [ ] Checked closure and relocation contain no second full Term-tag reference
      inventory.
- [ ] Evaluation and kernel rule switches remain explicit.
- [ ] v86/v87 bytes are unchanged.
- [ ] Term child performance does not regress by more than 5% in a focused
      traversal benchmark.

## 9. CR3: Immutable Structural Readers

### 9.1 Problem

Pure graph helpers accept broad mutable owner DBs. The checker therefore repeats
representation-level Context, Substitution, and Term access while correctly
avoiding producer caches and evidence.

### 9.2 Design

Introduce read-only reader interfaces, not mutable DB aliases. A reader exposes
semantic records and counts but no allocation, intern index, cache, solver,
source, or Claim state.

Required readers:

```text
TermStructuralReader
  term/case/case-binder/fold-clause access
  IH scope semantic access through an adapter

ContextStructuralReader
  count, parent, binding, frozen classifier, extension kind,
  producer computation

SubstitutionStructuralReader
  count, kind, source/target, first/second, term, term classifier
```

Use ordinary C structs and callbacks or explicit accessor tables. Do not cast
producer records to checked records. Producer Context classifier refs can be
provisional before freeze, whereas checked Context classifiers are concrete;
the producer adapter must reject a non-frozen classifier when a semantic reader
is requested.

### 9.3 Shared algorithms admitted in CR3

- [ ] Context ancestor/path length and binding lookup.
- [ ] Substitution shape and dependency traversal.
- [ ] Substitution binding image lookup without allocating Terms.
- [ ] Term fixed-reference traversal from CR2.
- [ ] basic Pi decomposition and pure-family binder extraction where no
      conversion decision is made.
- [ ] effect-row structural enumeration without equality/acceptance policy.

### 9.4 Algorithms retained separately

- [ ] Keep checker `checker_term_equal_after_bindings()` and producer conversion
      separate in this package.
- [ ] Keep producer substitution that interns rewritten Terms separate from
      checker non-allocating substitution interpretation.
- [ ] Keep resource usage reconstruction entry points separate.
- [ ] Keep Universe collection entry points separate.

### 9.5 Migration and deletion

- [ ] Add producer adapters over frozen Term/Context/Substitution stores.
- [ ] Add checked-module adapters over semantic records.
- [ ] Migrate checker Context ancestry and substitution image readers.
- [ ] Migrate producer read-only callers where they do not need mutation.
- [ ] Remove duplicate checker helpers only after differential tests pass.
- [ ] Delete broad legacy wrappers when no caller remains.

### 9.6 Tests and performance

- [ ] Run the same Context/Substitution corpus through producer and checked
      adapters and compare structural results.
- [ ] Include identity, empty, projection, extend, compose, and malformed cycles.
- [ ] Include sequence-result Contexts and producer computation references.
- [ ] Verify a provisional producer classifier cannot enter a semantic reader.
- [ ] Verify readers cannot name cache or proof fields at compile time.
- [ ] Record Context and Substitution query counts and medians.
- [ ] Reject more than 5% regression in IF8 or Function Graph QuickSort unless
      a measured reduction in repeated rebuilds offsets it.

## 10. CR4: Checked Semantic Closure and Relocation

### 10.1 Problem

`checker/module.c` separately owns projection, structural validation, root
marking, transitive closure, compaction, and relocation for many semantic record
families. Term internals are addressed by CR2, but root references across
Context, Substitution, declaration, occurrence, contract, and interface records
remain manually repeated.

### 10.2 Scope rule

Do not create one schema for every checked record immediately. Consolidate one
record family only when its reference inventory appears in at least three of:

- structural validation;
- closure marking;
- relocation;
- v87 codec;
- malformed-input mutation tests.

Start with Context, Substitution, semantic occurrence, Match case, fold clause,
contract, type/constructor declaration, and interface export/dependency records.

### 10.3 Implementation

- [ ] Add checked semantic reference visitors under
      `src/prototype/src/checker/` with a private internal header.
- [ ] Represent reference target kind explicitly: Term, Context, Substitution,
      occurrence, case, fold clause, contract, type, constructor, symbol, or
      checked-base index.
- [ ] Use the same visitor for range validation and closure marking.
- [ ] Use a typed mutable rewrite companion for relocation.
- [ ] Keep projection explicit because it intentionally drops producer fields.
- [ ] Keep v87 codec order explicit; add coverage assertions rather than native
      struct serialization.
- [ ] Remove replaced marking and relocation field switches.

### 10.4 Closure algorithm

Use one queue-based closure engine with typed mark sets. Avoid repeated
whole-arena fixed-point scans where dependency edges can enqueue their targets.
Substitution dependencies must remain backward/dense validated, but closure must
not rely on input ordering for correctness unless the schema explicitly requires
that ordering.

### 10.5 Tests

- [ ] Every semantic record family has all-reference and missing-reference cases.
- [ ] proof-only, cache-only, debug-only, and source-only records remain absent.
- [ ] dependencies reachable only through classifiers and contracts survive.
- [ ] unreachable stale producer records are removed.
- [ ] relocation preserves every cross-arena edge.
- [ ] malformed references reject before checker capability creation.
- [ ] canonical v87 bytes remain unchanged.

## 11. CR5: Canonical Checked-Module Image Ownership

### 11.1 Problem

`producer/merge.c` and `checker/module_set.c` both serialize checked modules to
temporary files, sort canonical bytes, and deduplicate equal modules. The merge
producer then calls module-set creation and serializes the surviving modules a
second time.

### 11.2 Selected ownership

Introduce one private canonical image collection owned by the checker/module-set
boundary:

```text
CanonicalCheckedImage
  borrowed checked capability
  owned canonical v87 bytes
  original input index

CanonicalCheckedImageSet
  sorted and byte-deduplicated images
```

Image preparation must not eagerly perform all export-conflict checks. Those
checks are effort-accounted and resumable in the merge producer. A completed
merge transfers or shares the already prepared image collection with the final
module set without serializing again.

### 11.3 API requirements

- [ ] One constructor serializes each input capability once.
- [ ] Sorting and byte deduplication happen once.
- [ ] Original input indices remain available for diagnostics.
- [ ] Merge producer can advance conflict-pair checking incrementally.
- [ ] Final module-set construction can adopt completed images.
- [ ] Capsule creation borrows canonical bytes without manufacturing authority.
- [ ] Ownership and destruction are explicit on pause, reject, complete, and
      resume paths.
- [ ] Remove `merge_module_image` and one of the duplicate image builders.

### 11.4 Tests

- [ ] Add a test-only serialization counter and require one serialization per
      distinct input capability.
- [ ] Permute input and worker completion order.
- [ ] Deduplicate byte-identical modules.
- [ ] Reject unequal same-name exports at the same effort step as before.
- [ ] Pause before and after each conflict pair and resume to identical bytes.
- [ ] Round-trip merge capsules in a fresh session.
- [ ] Verify failed merge frees each owned image exactly once.
- [ ] Compare canonical module-set bytes before and after refactor.

### 11.5 Performance gate

- [ ] v87 serialization count decreases from two passes to one for merged sets.
- [ ] merge wall time does not regress.
- [ ] ordinary checked import remains unchanged.

## 12. CR6: Explicit TypeView Rebuild Capability

### 12.1 Problem

`prototype_constructor_schema_view_action_classifier()` accepts the aggregate
mutable TypeDeclarationDB because graph reindexing may rebuild a changed
TypeView. Direct semantic schema access is otherwise narrow.

### 12.2 Design

Define an explicit typed capability, for example:

```text
TypeViewRebuildContext
  read-only semantic schema
  explicit constructor classifier cache capability if required
  no readback store
  no representation mutation
  no specialization statistics
```

The exact fields are determined by tracing
`prototype_term_type_view_rebuild_from_source()`. A callback is acceptable if
it names the rebuild operation and state explicitly; a `void*` escape hatch is
not.

### 12.3 Migration

- [ ] Trace every TypeView rebuild read and write.
- [ ] Add the narrow capability.
- [ ] Change Term substitution/reindex APIs to accept it.
- [ ] Change schema action-classifier construction to pass it.
- [ ] Migrate all callers.
- [ ] Delete broad overloads and wrappers.
- [ ] Add a static source test preventing aggregate TypeDeclarationDB parameters
      in pure semantic schema query APIs.

### 12.4 Tests

- [ ] Dependent constructor fields reindex correctly.
- [ ] named TypeViews preserve source and identity.
- [ ] clearing readback has no effect.
- [ ] representation IDs do not change.
- [ ] cache rebuild returns the same classifier.
- [ ] erased Core substitution remains independent of TypeDeclarationDB.

## 13. CR7: Alpha Binder and Environment Toolkit

### 13.1 Problem

Core canonical hashing and TypeDeclaration representation fingerprinting both
maintain alpha-renaming environments and traverse Lambda, Pi, Match binders, and
effect-row binders. Their semantic output domains differ.

### 13.2 First extraction

Extract only reusable environment mechanics initially:

- binding push/pop and lookup;
- Match case binder scope entry/exit;
- IH frame scope mapping where structurally identical;
- Universe-level alpha-slot mapping; and
- bounded depth/cycle accounting.

Keep canonical Term hash mixing and type representation fingerprint mixing in
their current modules with explicit domain tags.

### 13.3 Decision gate for a shared fold

After environment extraction:

- [ ] compare remaining tag-switch overlap;
- [ ] measure code removed versus callback/branch overhead;
- [ ] list tags intentionally treated differently, especially TypeFormer,
      TypeDeclaration, TypeView, recursive self references, and Universe vars;
- [ ] prove separate algebras can express both algorithms without exposing
      type representation authority to Core; and
- [ ] proceed with a shared structural cursor only if the distinction remains
      visible and the resulting code is smaller and faster enough to justify it.

If these conditions fail, retain separate traversals and record the negative
decision. Completing CR7 does not require forcing a generic fold.

### 13.4 Tests

- [ ] alpha-renamed Lambda/Pi/Match Terms retain canonical identity.
- [ ] free Binding identities remain distinct.
- [ ] IH frame-local references remain non-linkable where required.
- [ ] representation fingerprints handle parameter/index/field telescopes.
- [ ] recursive self references remain stable.
- [ ] Universe level alpha-renaming remains distinct from level equality.
- [ ] include examples where canonical Term key and representation fingerprint
      intentionally differ.

## 14. CR8: Checker Module Decomposition

### 14.1 Preconditions

CR3, CR4, and CR6 must be complete. Do not split `checker/session.c` while its
subsystems still communicate through private implicit state.

### 14.2 Target layout

Proposed private implementation layout:

```text
src/checker/
  internal.h
  session.c
  structure.c
  conversion.c
  declarations.c
  dependent_match.c
  cbpv_effects.c
  interface.c
  resources.c
  universe.c
  module.c
  container.c
  module_set.c
```

The exact filenames may change, but dependency direction must be:

```text
immutable readers
  -> structural checker
  -> rule-family checkers
  -> session driver
  -> opaque checked capability
```

### 14.3 Rules

- [ ] Define one private checker state in `internal.h` with documented ownership.
- [ ] Give each rule-family module a narrow entry point.
- [ ] Keep effort consumption in the session driver or explicit rule boundary.
- [ ] Preserve stop reason and subject reporting.
- [ ] Do not expose private modules through public headers.
- [ ] Do not merge rule acceptance with accepted replay.
- [ ] Do not move code solely to hit a line-count target.

### 14.4 Exit gate

- [ ] `checker/session.c` contains orchestration, not every rule implementation.
- [ ] no new cyclic include dependency exists;
- [ ] public checker headers still exclude producer and accepted-proof headers;
- [ ] checker authority-erasure tests pass; and
- [ ] checker per-rule-family effort counts remain identical.

## 15. CR9: Lowering, Kernel, and HOTT Internal Boundaries

### 15.1 Principle

Large `.inc` forests are not automatically wrong. They are changed only where
an internal API and owner can be stated. Similar theorem-rule plumbing may be
shared, but distinct rule premises and conclusions remain explicit.

### 15.2 Lowering

- [ ] Split the graph-construction state into explicit AST lowering, type
      declaration, block/effect, and occurrence-edge capabilities.
- [ ] Keep constraint generation separate from solving and freeze.
- [ ] Make the six constraint fragments consume one documented solver state.
- [ ] Do not reintroduce classifier/effect mutable authority into occurrence
      records.
- [ ] Convert `.inc` fragments to `.c` only after cross-fragment static helpers
      have explicit declarations and narrow parameters.

### 15.3 Kernel Judgement

- [ ] Keep Proposition/Claim/Derivation storage separate from conversion and
      rule emission.
- [ ] Extract common premise-view, transaction, and evidence-allocation plumbing.
- [ ] Keep `APP_ELIM`, `MATCH_ELIM`, IH elimination, and computation-fold
      elimination as named rule implementations.
- [ ] Keep candidate replay and accepted replay separate.
- [ ] Preserve immutable accepted premise views.

### 15.4 Identity/HOTT

- [ ] Define explicit internal interfaces for relation action, action
      certificate, telescope action, object Identity computation, and execution.
- [ ] Preserve logical relation versus object Identity separation.
- [ ] Preserve semantic freeze/epoch checks across modules.
- [ ] Do not expand the admitted HOTT fragment as part of organization work.

### 15.5 Decision gate

For each translation unit, record:

- static functions crossing current fragment boundaries;
- state fields used by each fragment;
- proposed owner;
- expected line movement and deletion;
- compile/link impact; and
- whether a real module split reduces coupling.

If no narrow interface exists, retain the include forest and document the
negative decision rather than creating a broad internal header.

## 16. CR10: Driver and Utility Organization

### 16.1 Driver

Preserve the S1 session-owned storage bundles. Separate only responsibilities:

- command parsing and dispatch;
- source/program session construction;
- provider/import resolution;
- artifact and checked-container inspection;
- normalization/type-inspection commands; and
- diagnostic rendering.

No static backing arrays may return to `read_file.c`.

### 16.2 Hash and byte utilities

Extract a utility only for repeated mechanics:

- endian-stable u32/u64 encoding;
- checked reserve/append operations where ownership contracts match; and
- domain-separated FNV-1a state initialization and byte mixing.

Each persisted format retains its own:

- domain identifier;
- version;
- hash width;
- field order;
- constants where the wire contract requires them; and
- final validation rule.

Do not replace the checked incremental four-lane fingerprint with a 64-bit FNV
helper.

### 16.3 Tests

- [ ] two driver sessions remain reentrant;
- [ ] failed read followed by valid read retains no state;
- [ ] provider/import ordering is unchanged;
- [ ] diagnostic command output is byte-identical where specified;
- [ ] v86, v87, and capsule bytes remain stable; and
- [ ] hash domain-crossing tests reject accidental reuse.

## 17. CR11: Integrated Validation

### 17.1 Required targeted tests

```sh
sh src/prototype/tests/integration/test_term_child_roles.sh
sh src/prototype/tests/integration/test_checked_core_projection.sh
sh src/prototype/tests/integration/test_checked_core_session.sh
sh src/prototype/tests/integration/test_checked_artifact_container.sh
sh src/prototype/tests/integration/test_work_capsule.sh
sh src/prototype/tests/integration/test_context_substitution_immutability.sh
sh src/prototype/tests/integration/test_context_resolution_incremental.sh
sh src/prototype/tests/integration/test_constraint_authority.sh
sh src/prototype/tests/integration/test_type_capability_authority.sh
sh src/prototype/tests/integration/test_resource_usage.sh
sh src/prototype/tests/integration/test_hott_goal.sh
sh src/prototype/tests/integration/test_spec_consistency.sh
```

The exact existing test name is used if one of these responsibilities is
currently covered under a differently named integration entry point.

### 17.2 Required semantic fixtures

- examples 01-09;
- same Core identity under Bool/Nat typed occurrences;
- List Nat Match and recursive append;
- indexed Vec and Acc;
- IF8 fuel-free QuickSort;
- Function Graph generated length, multiple recursion, and dependent spine;
- CBPV Return/Thunk/Force;
- higher-order operation and multi-clause computation fold;
- resource-sensitive one-shot and multi-shot resumptions;
- artifact read/write/link/import;
- checked capability erasure; and
- currently admitted Identity/HOTT dimensions.

### 17.3 Full suite

```sh
sh src/prototype/tests/run_integration_suite.sh \
  --timing-output /tmp/a-program-cr11-integration.timing
```

- [ ] Every current integration test executes or is explicitly classified as
      intentionally excluded with a reason.
- [ ] No pass count from an older suite size is used as completion evidence.
- [ ] Failed, skipped, and signaled tests are reported separately.

### 17.4 Performance rejection conditions

Reject the refactor when any condition holds without an explained and approved
tradeoff:

- IF8 or Function Graph QuickSort median regresses by more than 10%;
- Context/Substitution rebuild/query count increases;
- checker Term visits increase by more than 10% for identical input;
- merge serializes a distinct module more than once;
- artifact publication or checked import gains a new whole-graph scan;
- normalization cache hit rate decreases due to view migration; or
- peak memory grows by more than 10% on the stress fixtures.

### 17.5 Authority checks

- [ ] Search checker sources for Claim, Derivation, candidate, solver, and
      normalization-cache dependencies.
- [ ] Search semantic schema queries for readback/representation mutation.
- [ ] Search occurrence freeze paths for solved classifier/effect writers after
      freeze.
- [ ] Reconstruct resource and Universe results independently.
- [ ] Erase producer proof/search state and recheck identical exports.

## 18. Artifact and Compatibility Policy

This refactor targets byte-preserving implementation changes.

- v86 remains version 86 and retains its existing schema fingerprint.
- checked v87 remains version 87 and retains canonical bytes.
- moving v83 to archive changes documentation placement only.
- internal C APIs are replaced without compatibility wrappers.
- no artifact reader is retained merely for migration convenience beyond the
  explicitly supported v86 and v87 boundaries.

If exact bytes must change, stop this plan and create a separate versioned
format plan. Do not hide a format change inside code organization work.

## 19. LOC and Complexity Ledger

Record both movement and deletion. Moving code from `.inc` to `.c` is not a
reduction.

### 19.1 Per-package summary

| Package | Added | Deleted | Net | Files added | Files removed | Repeated switches removed | Notes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| CR0-CR7 | 5842 | 1558 | +4284 | 16 | 1 moved | 3 major inventories | Rename-aware integrated diff; includes docs/tests |
| CR8 | 0 | 0 | 0 | 0 | 0 | 0 | Negative decision: no narrow physical split |
| CR9 | 0 | 0 | 0 | 0 | 0 | 0 | Negative decision: retain explicit rule units |
| CR10 | 0 | 0 | 0 | 0 | 0 | 0 | Negative decision: no beneficial extraction |
| CR11 | 0 | 0 | 0 | 0 | 0 | 0 | Validation and ledger only |

### 19.2 Required file ledger

For every changed implementation or test file record:

| File | Baseline lines | Final lines | Added | Deleted | Net | Semantic owner after change |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| See Section 22.5 | Recorded | Recorded | Recorded | Recorded | Recorded | Recorded |

### 19.3 Structural metrics

Also record:

- full Term-tag reference switches before/after;
- Context ancestry implementations before/after;
- Substitution image implementations before/after;
- checked semantic root inventories before/after;
- canonical module serialization calls before/after;
- broad TypeDeclarationDB semantic query parameters before/after;
- `.inc` fragments and cross-fragment static declarations before/after; and
- largest implementation units before/after.

Code reduction is desirable but not the primary gate. One explicit schema plus
several small consumers may add lines while reducing semantic drift. Such an
increase requires a recorded explanation and measurable deletion of duplicate
inventories.

## 20. Commit and Review Sequence

Recommended commits:

1. `docs: specify accepted v86 and checked v87 boundaries`
2. `core: add exhaustive term semantic reference schema`
3. `core: add immutable structural readers`
4. `checker: consolidate semantic closure and relocation`
5. `checker: unify canonical checked module image ownership`
6. `kernel: narrow TypeView rebuild capability`
7. `core: share alpha binder environment mechanics`
8. `checker: split rule-family implementation modules`
9. `frontend: make lowering and solver internal boundaries explicit`
10. `kernel: make judgement and HOTT internal boundaries explicit`
11. `driver: separate command responsibilities and byte utilities`
12. `tests: complete structural, authority, performance, and LOC record`

Each commit must build and run its targeted tests. Do not accumulate all
deletions into one final cleanup commit; old paths are removed with the package
that replaces them.

## 21. Completion Gate

The plan is complete only when all of the following are proven from the current
worktree:

- [x] CR0-CR11 dashboard entries are complete or have an explicit negative
      decision accepted in this document.
- [x] every D-001 through D-015 finding has a final disposition;
- [x] one semantic Term reference inventory drives child/reference, checked
      closure, and relocation coverage;
- [x] immutable readers expose no producer cache/proof/solver state;
- [x] checker acceptance remains independent;
- [x] merged checked modules are serialized once per distinct input;
- [x] TypeView reindexing uses a narrow rebuild capability;
- [x] alpha identities retain separate output semantics;
- [x] file/module splits follow explicit internal APIs or have the explicit
      negative decisions in Section 22.2;
- [x] v86 and v87 authority and schemas are documented accurately;
- [x] canonical v86/v87 bytes are unchanged, or an independently approved
      format plan supersedes this condition;
- [x] full current integration suite passes;
- [x] performance gates are resolved as recorded in Section 22.4;
- [x] per-file LOC and structural metrics are recorded; and
- [x] no compatibility wrapper, duplicate mutable authority, or hidden fallback
      remains from the migration.

## 22. Final Implementation Record

This section is the authoritative completion record. Earlier package checklists
describe the planned search space; an unchecked item there is not an implicit
future commitment when this section records an explicit negative decision.

### 22.1 Implemented packages

- **CR1:** added an executable checked-container v87 schema, archived v83, and
  documented accepted proof artifact v86 separately from checked semantic
  container v87.
- **CR2:** added one exhaustive Term semantic-field schema. Computational child
  access, checked reference validation, checked closure, and Term relocation now
  consume that schema. Evaluation and typing switches remain explicit.
- **CR3:** added immutable producer and checked adapters for Term, Context, and
  Substitution structure. Shared readers cover ancestry, binding images, Pi,
  and pure-family decomposition without exposing mutable authority.
- **CR4:** added one typed semantic-reference inventory for checked Contexts,
  substitutions, declarations, occurrences, contracts, interfaces, and roots.
  Validation and closure use this inventory; Term relocation uses the Term
  schema. Substitution closure is queue-driven instead of a whole-arena scan.
- **CR5:** module-set and resumable merge now share one canonical checked-image
  owner. Each input capability is serialized once, and a completed merge adopts
  those bytes. Capsule restoration adopts already checked canonical images.
- **CR6:** graph substitution and reindexing accept
  `prototype_type_view_rebuild_context`, containing only semantic schema and
  representation data. Aggregate `TypeDeclarationDB` access was removed from
  these graph APIs. A structural-only experiment was rejected because generated
  QuickSort requires nominal TypeView reconstruction; the narrow capability
  preserves that semantic requirement without readback/cache authority.
- **CR7:** canonical Term hashing and type representation fingerprinting share
  alpha-slot environment mechanics while retaining separate domain-specific
  traversals and hash algebras.

### 22.2 Explicit negative decisions

- **CR8:** `checker/session.c` was not physically split. Its rule groups share
  conversion state, effort accounting, stop reason, and subject ownership. A
  split now would require a broad private state API and move lines without
  reducing authority. The reusable structural boundary was extracted in CR3;
  rule acceptance remains visibly checker-local.
- **CR9:** lowering, Judgement, and Identity include forests were not converted
  mechanically to translation units. Their distinct theorem rules must remain
  explicit, and no narrow ownership boundary was found beyond the Term schema,
  readers, and alpha environment already extracted. This avoids hiding
  `APP_ELIM`, `MATCH_ELIM`, IH elimination, or computation-fold premises behind
  generic callbacks.
- **CR10:** driver and hash code were left unchanged. Persisted hash domains and
  driver session ownership are already distinct; extracting byte mechanics did
  not remove mutable authority or repeated semantic inventories and would add
  churn to this refactor.

### 22.3 Finding dispositions

| Finding | Final disposition |
| --- | --- |
| D-001 | Implemented by the exhaustive Term field/role schema |
| D-002 | Implemented by immutable Term/Context/Substitution readers |
| D-003 | Implemented by canonical checked-image ownership |
| D-004 | Implemented by checked v87 schema and dual-format documentation |
| D-005 | Alpha environment extracted; generic traversal rejected because output algebras differ |
| D-006 | Implemented as typed checked semantic-reference descriptors, not native-struct reflection |
| D-007 | Structural state extracted; physical checker split rejected by CR8 decision |
| D-008 | Include-forest split rejected until a narrower rule-family API exists |
| D-009 | Implemented by `prototype_type_view_rebuild_context` |
| D-010 | Producer usage solving and checker reconstruction remain independent by design |
| D-011 | Producer Universe closure and checker reconstruction remain independent by design |
| D-012 | Accepted replay and checked-Core acceptance remain independent; only readers are shared |
| D-013 | Function Graph package validation and declaration semantic authority remain separate |
| D-014 | Hash domains preserved; common utility extraction rejected as non-beneficial here |
| D-015 | Driver storage preserved; organization-only movement rejected as non-beneficial here |

### 22.4 Validation and performance

- Targeted structural, checker, artifact, authority, resource, HOTT, and
  Function Graph tests all passed.
- The complete current suite discovered and executed 52 tests: 52 passed, zero
  failed, zero skipped, zero signalled, total wall time 36,168 ms.
- IF8 median after warm-up was 196 ms, versus the 200 ms baseline. Context and
  Substitution bulk index rebuilds remained zero.
- Function Graph QuickSort final medians were 2,501 ms and 2,612 ms in two
  samples. The unchanged baseline could not complete its benchmark because its
  fixed-point phase already exceeded the pre-existing phase limit; its first
  measured wall time was 2,387 ms. The final fixed-point phase remains about
  1.38-1.46 s and Context/Substitution rebuilds remain zero. This refactor does
  not establish a greater-than-10% regression, but the pre-existing 2,500 ms
  wall threshold remains a separate performance issue.
- Full-suite canonical round-trip, accepted replay, checked-container erasure,
  and specification consistency tests prove that v86/v87 persistent behavior
  remains stable. Representative baseline fixtures remain v86 485,567 bytes
  with SHA-256
  `39654eaee559dc6b17e8beb48171570d59b5525706de69708ecc0b16379846d5`
  and v87 32,520 bytes with FNV-1a-64 `26620790245307313`.
- `git diff --check` reports no whitespace errors. Static dependency checks find
  no Claim, Derivation, solver, or normalization-cache dependency in checker
  sources.

### 22.5 Structural and line metrics

Relative to `fd546e1`, Git's rename-aware diff adds 5,842 lines and deletes
1,558, net +4,284. The two new planning/audit documents alone account for 1,938
added lines, and the archived v83 schema is a move rather than
a semantic duplication. In implementation hotspots, `checker/module.c` is 257
lines smaller, Core Term declarations are 167 lines smaller,
`producer/merge.c` is 62 lines smaller, and `type_declaration.c` is 51 lines
smaller. New declarative schemas/readers add code deliberately so repeated
semantic inventories can be removed.

Key before/after structural counts:

- fixed Term semantic field inventories: multiple independent switches to one
  schema plus role-filtered consumers;
- checked semantic root inventories: validation/closure/relocation copies to
  one typed descriptor inventory;
- canonical merge serialization paths: two to one;
- graph reindex aggregate TypeDeclaration capabilities: one broad capability to
  one two-field read-only capability;
- Context/Substitution bulk rebuilds in IF8 and Function Graph: zero before and
  zero after;
- largest implementation unit remains
  `frontend/lowering/graph_construction.inc` at 12,440 lines; no artificial file
  split was made.

Per-file implementation and test ledger:

The path-level ledger below records the old v83 path as 297 deleted lines and
the archive path as 297 added lines. Git correctly reports those two entries as
a 100% rename, so its aggregate added/deleted totals are each 297 lines lower.

| File | Baseline | Final | Added | Deleted | Net |
| --- | ---: | ---: | ---: | ---: | ---: |
| `src/prototype/build/sources.mk` | 124 | 134 | 18 | 8 | +10 |
| `include/a_program/checker/module.h` | 467 | 483 | 16 | 0 | +16 |
| `include/a_program/core/term.h` | 1492 | 1504 | 15 | 3 | +12 |
| `spec/artifact_v83.schema` | 297 | 0 | 0 | 297 | -297 |
| `src/checker/internal.h` | 31 | 85 | 54 | 0 | +54 |
| `src/checker/module.c` | 3392 | 3135 | 498 | 755 | -257 |
| `src/checker/module_set.c` | 285 | 352 | 99 | 32 | +67 |
| `src/checker/session.c` | 7271 | 7245 | 56 | 82 | -26 |
| `src/core/term/canonicalization.inc` | 2574 | 2535 | 20 | 59 | -39 |
| `src/core/term/declarations.inc` | 1081 | 914 | 66 | 233 | -167 |
| `src/core/term/storage_and_formation.inc` | 3105 | 3140 | 63 | 28 | +35 |
| `src/kernel/type_declaration.c` | 2664 | 2613 | 26 | 77 | -51 |
| `src/kernel/typing/conversion.inc` | 3885 | 3872 | 13 | 26 | -13 |
| `src/producer/merge.c` | 436 | 374 | 46 | 108 | -62 |
| `src/core/term/evaluation_and_conversion.inc` | 5027 | 5029 | 5 | 3 | +2 |
| `src/core/term/substitution.inc` | 1414 | 1414 | 10 | 10 | 0 |
| `src/dimension/action.c` | 873 | 871 | 2 | 4 | -2 |
| `src/frontend/function_graph.c` | 9046 | 9044 | 2 | 4 | -2 |
| `src/frontend/lowering/constraint/branch_refinement_and_motives.inc` | 4254 | 4247 | 11 | 18 | -7 |
| `src/frontend/lowering/constraint/classifier_and_computation_propagation.inc` | 2103 | 2101 | 2 | 4 | -2 |
| `src/frontend/lowering/constraint/effect_propagation_and_residuals.inc` | 1712 | 1710 | 2 | 4 | -2 |
| `src/frontend/lowering/constraint/evidence_and_freeze.inc` | 4368 | 4367 | 1 | 2 | -1 |
| `src/frontend/lowering/context_and_type_lowering.inc` | 5887 | 5884 | 4 | 7 | -3 |
| `src/frontend/lowering/finalization_and_entrypoints.inc` | 2841 | 2837 | 9 | 13 | -4 |
| `src/frontend/lowering/graph_construction.inc` | 12441 | 12440 | 9 | 10 | -1 |
| `src/graph/typed_occurrence/runtime.inc` | 1614 | 1613 | 1 | 2 | -1 |
| `src/identity/action_certificate_validation.inc` | 1356 | 1354 | 2 | 4 | -2 |
| `src/identity/identity_computation.inc` | 3565 | 3561 | 4 | 8 | -4 |
| `src/identity/object_term_action.inc` | 6159 | 6156 | 3 | 6 | -3 |
| `src/kernel/context.c` | 2169 | 2168 | 1 | 2 | -1 |
| `src/kernel/rules/match/candidate_search.inc` | 1006 | 1005 | 1 | 2 | -1 |
| `src/kernel/type_schema_view.c` | 524 | 523 | 1 | 2 | -1 |
| `src/kernel/typing/accepted_replay.inc` | 7903 | 7902 | 2 | 3 | -1 |
| `src/kernel/typing/classifier_solver.inc` | 2772 | 2772 | 3 | 3 | 0 |
| `tests/checks/cbpv_boundary_check.c` | 638 | 637 | 2 | 3 | -1 |
| `tests/checks/checked_artifact_container_check.c` | 263 | 347 | 84 | 0 | +84 |
| `tests/checks/checked_core_session_check.c` | 1395 | 1408 | 14 | 1 | +13 |
| `tests/checks/core/dimension_term_check.c` | 206 | 206 | 1 | 1 | 0 |
| `tests/checks/hott/pi_identity.inc` | 4039 | 4036 | 3 | 6 | -3 |
| `tests/checks/hott/test_support.inc` | 2527 | 2522 | 5 | 10 | -5 |
| `tests/checks/shared_term_reindex_check.c` | 524 | 573 | 53 | 4 | +49 |
| `tests/checks/spec_enum_check.c` | 299 | 313 | 14 | 0 | +14 |
| `tests/integration/test_shared_term_hott_substrate.sh` | 35 | 48 | 13 | 0 | +13 |
| `tests/integration/test_spec_consistency.sh` | 84 | 236 | 153 | 1 | +152 |
| `tests/integration/test_term_child_roles.sh` | 13 | 17 | 4 | 0 | +4 |
| `include/a_program/core/alpha_slot_env.h` | 0 | 46 | 46 | 0 | +46 |
| `include/a_program/core/term_schema.h` | 0 | 128 | 128 | 0 | +128 |
| `include/a_program/kernel/structural_reader.h` | 0 | 214 | 214 | 0 | +214 |
| `spec/archive/artifact_v83.schema` | 0 | 297 | 297 | 0 | +297 |
| `spec/checked_artifact_v87.schema` | 0 | 178 | 178 | 0 | +178 |
| `src/checker/module_set_internal.h` | 0 | 37 | 37 | 0 | +37 |
| `src/checker/semantic_references.c` | 0 | 440 | 440 | 0 | +440 |
| `src/core/alpha_slot_env.c` | 0 | 60 | 60 | 0 | +60 |
| `src/core/term_schema.c` | 0 | 416 | 416 | 0 | +416 |
| `src/kernel/structural_reader.c` | 0 | 466 | 466 | 0 | +466 |
| `tests/checks/alpha_slot_env_check.c` | 0 | 35 | 35 | 0 | +35 |
| `tests/checks/structural_reader_check.c` | 0 | 262 | 262 | 0 | +262 |
| `tests/checks/term_schema_check.c` | 0 | 99 | 99 | 0 | +99 |
| `tests/integration/test_structural_readers.sh` | 0 | 13 | 13 | 0 | +13 |

### 22.6 Completion gate result

All semantic, trust, artifact, and test gates are satisfied. The Function Graph
absolute timing threshold remains a known pre-existing performance issue, not a
hidden compatibility path or correctness failure introduced by this refactor.
No old graph-reindex overload, duplicate checked-image builder, duplicate Term
reference inventory, or mutable-authority fallback remains.

## 23. Progress Log

- 2026-08-27: completed CR2 through CR7. Added the exhaustive Term schema,
  immutable structural readers, typed checked-reference inventory, canonical
  checked-image owner, narrow TypeView rebuild capability, and shared alpha-slot
  environment. Removed each superseded internal path as callers migrated.
- 2026-08-27: rejected a structural-only TypeView reindex experiment after the
  Function Graph QuickSort dependency-closure test demonstrated that nominal
  representation reconstruction is semantically required. Replaced it with the
  two-field read-only rebuild capability and reran the failing fixture.
- 2026-08-27: completed CR8 through CR10 by the negative decisions in Section
  22.2. No line-moving physical split or generic calculus-rule dispatcher was
  introduced.
- 2026-08-27: all 15 targeted tests passed. The complete current integration
  suite passed 52/52 in 36,168 ms. IF8 measured 196 ms median; Function Graph
  measured 2,501 ms median with zero Context/Substitution bulk rebuilds.
- 2026-08-26 14:18 JST: fixed the implementation baseline at commit
  `fd546e13784c0d7c59f984beeacdac199ddab321` on `main`. The tracked worktree was
  clean. The source audit and this plan were the only untracked files; no
  implementation file had changed.
- 2026-08-26 14:19 JST: ran the complete current integration suite with
  `--keep-going`. All 51 discovered tests passed in 24,248 ms; no test was
  skipped or signalled. Timing records are in
  `/tmp/a-program-cr0-integration.timing` for the active workspace session.
- 2026-08-26 14:20 JST: recorded the existing IF8 benchmark at 200 ms median
  wall time after one warm-up (three measured runs). The benchmark reported
  zero Context/Substitution bulk index rebuilds.
- 2026-08-26 14:21 JST: the existing Function Graph QuickSort benchmark failed
  before producing a median: its first measured compile used 2,387 ms and its
  fixed-point phase used 1,331,797,000 ns, exceeding the pre-existing
  700,000,000 ns phase limit. This is a baseline failure at the unchanged
  commit, not a refactor regression. The final record uses the benchmark's
  supported warm-up plus three-run contract and records the missing baseline
  median as an explicit limitation.
- 2026-08-27 00:06 JST: recorded representative byte baselines. Compiling the
  IF8 fixture produced accepted proof artifact v86 with 485,567 bytes and
  SHA-256 `39654eaee559dc6b17e8beb48171570d59b5525706de69708ecc0b16379846d5`.
  The checked-container fixture produced canonical v87 with 32,520 bytes and
  FNV-1a-64 `26620790245307313`.
- 2026-08-27 00:11 JST: completed CR1. Added the executable v87 wire/authority
  schema, archived v83, documented the distinct accepted-v86 and checked-v87
  boundaries, and added permanent version/enum plus malformed-container and
  canonical-erasure tests. Targeted specification and container tests pass.

- 2026-08-26 13:55 JST: reviewed the source audit against current v86/v87
  implementation and corrected the assumption that v87 simply supersedes every
  current v86 role.
- 2026-08-26 13:55 JST: narrowed shared checker work to immutable structural
  access; retained independent checker conversion, resource, Universe, and
  capability acceptance.
- 2026-08-26 13:55 JST: selected schema-first, immutable-reader-second, and
  module-decomposition-last sequencing.
- 2026-08-26 13:55 JST: added implementation packages, permanent validation,
  performance rejection conditions, and LOC/complexity ledgers.
