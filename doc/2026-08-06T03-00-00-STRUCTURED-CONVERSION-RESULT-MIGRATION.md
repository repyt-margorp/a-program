# Structured Conversion Result Migration

Date: 2026-08-06

Status: Implemented

Baseline:

- branch: `main`
- commit: `056741f`
- artifact format: v61
- preceding audit:
  `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT.md`

## 1. Purpose

The next refactoring must make kernel conversion a structured decision instead
of a boolean predicate.

The current implementation can report whether an individual normalization
attempt completed, was blocked by an effect, exhausted its budget, or observed
an invalid graph. However, the recursive conversion comparator and all typing
clients collapse that information to:

```text
equal
not equal
```

That collapse is not sufficient for the planned Higher Observational Type
Theory work. A term pair that is decisively not definitionally equal is
different from a pair whose comparison has not finished. The latter may become
a residual verification problem; it must not silently become a type mismatch.

This migration does not add object equality, equality witnesses, transport,
quotients, or higher constructors. It cleans the existing meta-level
definitional conversion boundary so those features can be added without
reusing an ambiguous boolean API.

## 2. Current Implementation

### 2.1 Normalization already has structured status

`src/prototype/term.h` defines:

```c
enum prototype_term_normalization_status {
	PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE = 1,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID
};
```

The associated result records the observed term, step limit, consumed steps,
and graph revision. This is a suitable low-level source of evidence, but it is
not propagated through conversion.

### 2.2 Conversion discards normalization status

The authoritative recursive comparator is
`normalization_equal_at_depth(...)` in `src/prototype/term.c`. It currently
returns a C success/failure code and writes an `int equal` result.

Consequently, these situations are not represented as distinct outcomes:

- both sides reached decisive weak-head forms and differ;
- an effect request blocked pure type normalization;
- the configured reduction budget was exhausted;
- an opaque or unsupported neutral comparison remains unresolved;
- the graph or comparison request is invalid.

The four public `prototype_term_normalization_equal...` entry points preserve
the same boolean contract.

### 2.3 Typing collapses the result again

`classifier_kernel_normalization_equal(...)` in `src/prototype/typing.c`
special-cases reflexivity and universe variables, then calls the boolean term
API. It returns false both for decisive inequality and for an unsuccessful
normalization call.

The current call-site inventory is:

| Area | Direct classifier boolean calls |
| --- | ---: |
| `src/prototype/typing.c` | 65 |
| `src/prototype/ast.c` | 29 |
| focused check programs | 6 |

There are additional direct term-level boolean calls in
`src/prototype/read_file.c` and
`src/prototype/core_view_representation_check.c`.

These call sites do not all mean the same thing. Some validate a required
conversion, some search among candidates, some detect recursive fields, and
some only print an interactive normalization comparison. They must be
classified before migration rather than mechanically replacing every boolean
test.

### 2.4 Conversion proofs contain no conversion result

`PROTOTYPE_JUDGEMENT_PROOF_CONVERSION` records one original `HAS_TYPE`
premise. Artifact validation recomputes classifier compatibility through the
same boolean kernel predicate. A completed conversion proof therefore does not
currently serialize an equality trace, a reduction budget, or a residual
problem.

That is acceptable for completed definitional conversion in v61. It is not an
acceptable representation of an unfinished comparison.

## 3. Semantic Boundary

The migration must preserve these distinctions.

### 3.1 Definitional conversion

Definitional conversion is a meta-level kernel decision under a declared
normalization profile, definition environment, reduction policy, and budget.
It may justify the conversion typing rule, but it is not an object-language
equality witness.

### 3.2 Object equality

Higher observational equality will later introduce typed terms and rules in
the object language. No conversion result from this migration may be inserted
into TermDB as an equality witness.

### 3.3 Shape and link comparison

The alpha-canonical interning and link-shape relations completed at baseline
commit `056741f` remain separate from conversion. Canonical keys and graph
shape may reject or select candidates, but they must not establish object
equality or bypass the conversion profile.

## 4. Required Result Algebra

Add a structured conversion result in `src/prototype/term.h`.

The names below are the intended contract. Exact field layout may change while
implementing Phase 1, but no status may be removed by replacing it with a
boolean.

```c
enum prototype_term_conversion_status {
	PROTOTYPE_TERM_CONVERSION_EQUAL = 1,
	PROTOTYPE_TERM_CONVERSION_NOT_EQUAL,
	PROTOTYPE_TERM_CONVERSION_RESIDUAL,
	PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT,
	PROTOTYPE_TERM_CONVERSION_EXHAUSTED,
	PROTOTYPE_TERM_CONVERSION_INVALID
};

enum prototype_term_conversion_reason {
	PROTOTYPE_TERM_CONVERSION_REASON_NONE = 0,
	PROTOTYPE_TERM_CONVERSION_REASON_NEUTRAL,
	PROTOTYPE_TERM_CONVERSION_REASON_OPAQUE_DEFINITION,
	PROTOTYPE_TERM_CONVERSION_REASON_UNSUPPORTED_RULE,
	PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST,
	PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH
};

struct prototype_term_conversion_result {
	int status;
	int reason;
	int profile;
	uint32_t left;
	uint32_t right;
	uint32_t left_observation;
	uint32_t right_observation;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};
```

The public operation should follow this contract:

```c
int prototype_term_compare_for_conversion(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* result
);
```

The implementation also provides `prototype_term_compare_with_options(...)`
for interactive comparisons under an explicitly selected reduction rule set.
Both operations return the same structured result. The explicit `step_limit`
was added during implementation because exhaustion cannot be tested or replayed
correctly when the budget is hidden in an implicit global policy.

The C return value reports failure to execute the API itself, such as a null
output pointer or allocation failure. A well-formed request returns zero and
places its semantic outcome in `result->status`. A malformed graph observed by
the comparator should normally produce `INVALID`, not masquerade as
`NOT_EQUAL`.

## 5. Status Meaning

| Status | Meaning | Kernel typing action in this migration |
| --- | --- | --- |
| `EQUAL` | The selected conversion rules established conversion | Accept conversion |
| `NOT_EQUAL` | Decisive observed forms conflict | Report type mismatch or reject candidate |
| `RESIDUAL` | No rule decides the pair under the current policy | Reject with a distinct unresolved diagnostic |
| `BLOCKED_EFFECT` | Pure conversion reached an effectful computation | Reject as an invalid conversion dependency |
| `EXHAUSTED` | The declared step/depth budget ended first | Reject with budget diagnostic; never call it unequal |
| `INVALID` | Malformed graph, unsupported invariant, or invalid request | Report an internal/artifact validation error |

The first implementation must not broaden acceptance. Residual conversion
obligations become an artifact feature only after their schema and replay
policy are specified. Until then, non-`EQUAL` results are rejected, but their
reason remains observable and testable.

## 6. Recursive Propagation Rules

Changing only the public wrapper is insufficient. Every recursive comparison
in `normalization_equal_at_depth(...)` and its Match/computation helpers must
propagate structured outcomes.

The following rules are required:

1. Identical term IDs return `EQUAL` without normalization.
2. Alpha/shape equality may return `EQUAL` only where the existing conversion
   calculus already authorizes that comparison.
3. A valid constructor/tag/field conflict may return `NOT_EQUAL` immediately.
4. `EXHAUSTED`, `BLOCKED_EFFECT`, and `RESIDUAL` must never be rewritten to
   `NOT_EQUAL` by a caller.
5. `INVALID` dominates every ordinary comparison result.
6. For product-like recursive comparison, a decisive valid mismatch may return
   `NOT_EQUAL`. Otherwise the first unresolved result in deterministic
   left-to-right traversal is preserved.
7. Depth exhaustion must become `EXHAUSTED` with reason `DEPTH_LIMIT`, not a C
   error and not inequality.
8. Both observations and total consumed steps must be retained even when the
   result is incomplete.

Do not introduce a global conversion union-find in this migration. Conversion
depends on profile, definitions, transparency, budget, and graph revision.

## 7. API Migration Policy

Backward-compatible boolean wrappers must not remain after the migration.

The implementation may temporarily add the structured function while
converting callers inside one working branch, but the completed change must
delete:

- `prototype_term_normalization_equal(...)`;
- `prototype_term_normalization_equal_with_definitions(...)`;
- `prototype_term_normalization_equal_with_options(...)`;
- `prototype_term_normalization_equal_with_profile(...)`;
- `prototype_judgement_classifier_normalization_equal(...)`;
- `prototype_judgement_classifier_normalization_equal_with_definitions(...)`.

Do not replace them with a generic `is_equal(result)` helper that recreates the
same information loss. Sites that require equality must explicitly require
`status == PROTOTYPE_TERM_CONVERSION_EQUAL`. Sites where unresolved comparison
has different control flow must use a `switch`.

## 8. Call-Site Classification

Before editing each caller, assign it one of these classes.

| Class | Typical current use | Required behavior |
| --- | --- | --- |
| Required conversion | APP domain check, Lambda classifier check, conversion proof | Only `EQUAL` succeeds |
| Candidate search | Find a compatible classifier or declaration | `NOT_EQUAL` tries the next candidate; unresolved statuses stop or defer the search |
| Semantic classification | Recursive-field detection, Nat-shape recognition | Unresolved is not equivalent to false; propagate it |
| Artifact validation | Revalidate serialized proof or interface relation | Require `EQUAL`; distinguish malformed artifact from mismatch |
| Interactive inspection | `read_file.c` normalization commands | Print the status, reason, observations, and step use |
| Focused test | representation, universe, CBPV checks | Assert the exact expected status |

This classification is mandatory because a mechanical conversion such as
`old_equal(...)` to `result.status == EQUAL` would preserve the bug at search
and semantic-classification sites.

## 9. Conversion Proof and Artifact Policy

### 9.1 Completed v61 conversion proofs

The existing `PROTOTYPE_JUDGEMENT_PROOF_CONVERSION` payload may remain unchanged
while this migration only accepts `EQUAL` results and recomputes them during
validation. That keeps the artifact format at v61.

Validation must use the structured result and require `EQUAL`. It must report:

- `NOT_EQUAL` as an invalid claimed conversion;
- `EXHAUSTED` as insufficient replay budget;
- `BLOCKED_EFFECT` as an illegal effect dependency in kernel conversion;
- `RESIDUAL` as an unsupported incomplete conversion claim;
- `INVALID` as malformed artifact or internal inconsistency.

### 9.2 Residual conversion obligations

Do not serialize `struct prototype_term_conversion_result` directly. It
contains observations of one run, not a stable proof obligation.

A future residual record must instead contain a stable conversion problem:

- relocated left and right term IDs;
- normalization profile and rule-set fingerprint;
- definition/transparency policy identity;
- declared step budget;
- required graph and declaration dependencies;
- replay policy and expected terminal status.

Adding that record changes artifact semantics and requires v62. It belongs with
the later typed constraint and residual verification migration, not this v61
cleanup.

## 10. Cache Policy

Do not add a global conversion-result cache in the first implementation.

The existing normalization cache is keyed by term, profile, and graph revision
for completed WHNF observations. A correct conversion cache would additionally
need to account for:

- both terms and their orientation or a canonical pair order;
- definition environment and transparency policy;
- normalization profile and enabled rules;
- step/depth budget;
- graph revision;
- future universe and observational constraints.

Incomplete results must not be cached as permanent inequality. Cache design is
a separate optimization after the status contract is covered by tests.

## 11. Implementation Phases

### Phase A: Freeze the contract

- [x] Confirm the status and reason enums.
- [x] Confirm that C API failure and semantic `INVALID` remain distinct.
- [x] Specify deterministic recursive status propagation.
- [x] Specify first-migration diagnostics for every status.
- [x] Confirm that v61 stores only completed conversion proofs.

Exit criterion: the result algebra has no boolean or ambiguous zero state.

### Phase B: Refactor the term-level comparator

- [x] Add conversion result types to `src/prototype/term.h`.
- [x] Add structured profile and explicit-options conversion entry points.
- [x] Refactor `normalization_equal_at_depth(...)` to propagate the result.
- [x] Refactor Match-case binder comparison.
- [x] Refactor computation/Match commuting conversion.
- [x] Propagate normalization status and consumed steps from both sides.
- [x] Convert depth overflow to structured exhaustion.
- [x] Add focused term-level status tests before migrating typing.

Exit criterion: term-level tests distinguish all six statuses without using a
boolean equality output.

### Phase C: Refactor the typing kernel boundary

- [x] Replace `classifier_kernel_normalization_equal(...)` with a structured
  classifier conversion operation.
- [x] Preserve same-ID reflexivity in the structured result.
- [x] Preserve current universe-level identity behavior without adding
  cumulativity to DefEq.
- [x] Inventory and classify all 65 `typing.c` call sites.
- [x] Migrate required-conversion sites.
- [x] Migrate candidate-search sites with explicit unresolved propagation.
- [x] Migrate semantic-classification sites without treating unresolved as
  false.
- [x] Add diagnostics that identify status, reason, and observed terms.

Exit criterion: no typing decision obtains conversion through a boolean API.

### Phase D: Refactor lowering and artifact validation

- [x] Inventory and classify all 29 `ast.c` call sites.
- [x] Migrate source-lowering compatibility checks.
- [x] Migrate recursive-field and declaration-selection decisions.
- [x] Make conversion-proof validation require structured `EQUAL`.
- [x] Verify that conversion proof serialization remains byte-compatible v61.
- [x] Document any discovered payload change before implementing it; if a
  payload changes, stop and bump to v62 rather than adding compatibility code.

Exit criterion: artifact replay cannot confuse exhaustion or blocked effects
with a claimed type mismatch.

### Phase E: Migrate tools and tests

- [x] Update `src/prototype/read_file.c` to display structured status.
- [x] Update `core_view_representation_check.c`.
- [x] Update `context_category_check.c`.
- [x] Update `universe_defeq_check.c`.
- [x] Update `cbpv_boundary_check.c`.
- [x] Add a dedicated `conversion_result_check.c` and shell driver.
- [x] Delete all boolean conversion APIs and declarations.
- [x] Add an `rg` regression gate proving old API names are absent.

Exit criterion: every in-tree conversion caller consumes the structured API.

### Phase F: Full validation

- [x] Run the focused conversion status test.
- [x] Run the term identity/frame test.
- [x] Run every `src/prototype/test_*.sh` script.
- [x] Rebuild the compiler and reader from clean objects.
- [x] Compile examples 01-07 and 09.
- [x] Run artifact write/read/link/revalidation tests.
- [x] Confirm v61 emission and v60 rejection remain unchanged.
- [x] Record commands and results in Section 15.

Exit criterion: all validation passes and the worktree contains no legacy
boolean compatibility path.

## 12. Required Focused Tests

The dedicated conversion test must cover at least:

1. identical node IDs produce `EQUAL` without reduction;
2. alpha-equivalent Lambda terms produce `EQUAL` under the authorized rule;
3. beta reduction produces `EQUAL`;
4. constructor Iota reduction produces `EQUAL`;
5. guarded induction reduction produces `EQUAL`;
6. distinct constructor heads produce `NOT_EQUAL`;
7. distinct universe level variables produce `NOT_EQUAL` under the current
   universe policy;
8. zero or deliberately insufficient fuel produces `EXHAUSTED`;
9. a pure-type comparison that reaches an effect request produces
   `BLOCKED_EFFECT`;
10. an unsupported or intentionally opaque comparison produces `RESIDUAL`;
11. an invalid term ID or malformed edge produces `INVALID` rather than
    `NOT_EQUAL`;
12. a recursive child exhaustion propagates to the parent comparison;
13. a decisive child mismatch remains `NOT_EQUAL`;
14. TypeView/core sharing alone does not produce `EQUAL` for distinct
    classifiers;
15. conversion-proof validation accepts only `EQUAL`.

The current calculus has no honest producer for `RESIDUAL`. The enum, reason
names, and propagation contract are retained, while test 10 remains deferred
until opaque conversion or typed residual constraints supply a semantic
producer. No fake production path was added.

## 13. Stop Conditions

Stop implementation and revise this plan if any of the following occurs:

- a typing caller needs to accept `EXHAUSTED` or `BLOCKED_EFFECT` as equality;
- conversion invokes host effect dispatch or machine-dependent pure intrinsic
  execution;
- a residual problem can only be represented using process-local pointers;
- conversion proof replay requires a different rule set than source typing but
  the artifact does not identify that rule set;
- a v61 serialized field or numeric tag must change;
- TypeView selection or source names become implicit evidence of DefEq;
- the only way to migrate a call site is to restore a global boolean wrapper.

## 14. Progress Sheet

Allowed status values are `not started`, `in progress`, `blocked`, and
`complete`.

| ID | Work item | Status | Primary files | Evidence / notes |
| --- | --- | --- | --- | --- |
| A1 | Freeze conversion status algebra | complete | this document | Six statuses and eight reasons |
| A2 | Freeze status propagation order | complete | this document | Shared status/fuel survives recursive comparison |
| A3 | Confirm v61 boundary | complete | `ast.c`, audit document | Artifact suite asserts v61 and rejects v60 |
| B1 | Add structured term API | complete | `term.h`, `term.c` | Profile and explicit-options APIs |
| B2 | Refactor recursive comparator | complete | `term.c` | Decisive mismatch remains distinct from interruption |
| B3 | Propagate normalizer status and fuel | complete | `term.c` | Step and depth reasons recorded |
| B4 | Add focused term status tests | complete | `conversion_result_check.c` | Residual producer intentionally deferred |
| C1 | Add structured classifier conversion | complete | `judgement.h`, `typing.c` | Struct returned by value |
| C2 | Classify 65 typing call sites | complete | `typing.c` | Required, search, and semantic sites migrated |
| C3 | Migrate required checks | complete | `typing.c` | Explicit `EQUAL` requirement |
| C4 | Migrate searches and semantic classifiers | complete | `typing.c` | Non-decisive results abort candidate collection |
| D1 | Classify 29 AST call sites | complete | `ast.c` | Lowering and solver sites migrated |
| D2 | Migrate AST/lowering decisions | complete | `ast.c` | Recursive classification rejects interruption |
| D3 | Migrate conversion-proof validation | complete | `typing.c`, `ast.c` | Recomputed result must be `EQUAL` |
| D4 | Confirm artifact remains v61 | complete | artifact tests | No serialized schema or numeric tag changed |
| E1 | Migrate REPL/read-file comparison | complete | `read_file.c` | Status, reason, and steps printed |
| E2 | Migrate focused check programs | complete | `*_check.c` | All focused checks compile with `-Werror` |
| E3 | Delete boolean APIs | complete | `term.h`, `term.c`, `judgement.h`, `typing.c` | Legacy declarations and definitions absent |
| E4 | Add legacy-name absence gate | complete | `test_conversion_result.sh` | `rg` gate runs before compilation |
| F1 | Run focused tests | complete | test logs | `test_conversion_result.sh` passes |
| F2 | Run all prototype tests | complete | test logs | Every `test_*.sh` passes |
| F3 | Run example matrix | complete | test logs | Examples 01-07 and 09 pass |
| F4 | Run artifact matrix | complete | test logs | Write/read/link/revalidation passes |
| F5 | Record final result and commit | complete | Section 15 | Commit and push follow this recorded validation |

## 15. Validation Log

Update this section during implementation. Do not mark a phase complete without
recording its evidence.

| Date | Commit | Command / test | Result | Notes |
| --- | --- | --- | --- | --- |
| 2026-08-06 | `056741f` | planning baseline | complete | Existing term identity/frame refactor; no structured conversion result yet |
| 2026-08-06 | working tree | `sh src/prototype/test_conversion_result.sh` | complete | Equal, not-equal, beta, recursive exhaustion, blocked effect, invalid, and universe identity |
| 2026-08-06 | working tree | every `src/prototype/test_*.sh` | complete | Includes term identity, CBPV, dependent Pi, and artifact flow |
| 2026-08-06 | working tree | clean `make` and examples 01-07, 09 | complete | All selected examples compile and run |
| 2026-08-06 | working tree | `test_artifact_flow.sh` | complete | v61 emitted, v60 rejected, conversion proofs revalidated |

## 16. Decisions and Revisions

Record plan changes here so implementation does not silently drift.

| Date | Decision | Reason | Consequence |
| --- | --- | --- | --- |
| 2026-08-06 | Structured conversion is the next kernel cleanup | Boolean conversion loses incomplete-normalization state | HOTT object equality remains deferred |
| 2026-08-06 | Keep completed proof payloads at v61 initially | No serialized schema change is required for recomputed `EQUAL` | Residual obligations require a later v62 design |
| 2026-08-06 | Remove old boolean APIs after migration | Repository policy rejects compatibility clutter | All prototype callers migrate in one change |
| 2026-08-06 | Do not add conversion caching yet | A sound cache key is wider than the current normalization key | Optimize only after semantic tests pass |
| 2026-08-06 | Make the step limit an explicit API argument | Exhaustion is part of the semantic result | Typing uses `UINT64_MAX`; focused tests use finite fuel |
| 2026-08-06 | Keep `RESIDUAL` without an artificial producer | No current conversion rule honestly yields it | Typed residual constraints will introduce the first producer |

## 17. Completion Definition

This migration is complete only when:

- the recursive comparator exposes all six outcomes;
- normalization budget and blocked-effect status survive recursive comparison;
- typing, lowering, validation, REPL, and tests use the structured API;
- no boolean conversion wrapper remains;
- conversion proofs are accepted only after an `EQUAL` result;
- no object-language equality term has been introduced accidentally;
- artifact v61 compatibility is either demonstrated or intentionally replaced
  by a separately documented v62 migration;
- the full validation matrix passes and is recorded above.

The next HOTT refactoring after this plan is JudgementDB rule-specific proof
payloads and typed residual constraint records. Those should consume the
structured conversion result rather than reopening the boolean API.
