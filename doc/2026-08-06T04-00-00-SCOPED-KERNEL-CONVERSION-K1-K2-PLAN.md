# Scoped Kernel Conversion K1/K2 Implementation Plan

Date: 2026-08-06

Status: implemented and validated

Parent plan:

- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

This plan combines V2-K1 and V2-K2 because both require kernel conversion to
compare terms under explicit lexical binder correspondence. It does not add
object equality, HOTT witnesses, transport, or new artifact records.

## 1. Baseline

This plan is pinned to:

- branch: `main`;
- commit: `474867ea31331bcf93821f9bf106184602715e58`;
- short commit: `474867e`;
- artifact format: `A_PROGRAM_ARTIFACT 61`;
- all twelve `src/prototype/test_*.sh` scripts passing;
- a clean worktree before the V2 planning documents were added.

The relevant completed migrations are:

- `056741f`: local alpha identity was separated from artifact-link shape;
- `474867e`: kernel conversion now reports structured outcomes.

## 2. Goal

Make definitional conversion respect lexical binder and recursive Match-frame
scope without using linker canonical keys or source-shape comparison as a
kernel equality rule.

The intended conversion judgements include:

```text
Gamma |- match_left == match_right
Gamma, left_frame <-> right_frame,
       left_case_binders <-> right_case_binders
    |- left_case_body == right_case_body
```

and:

```text
Gamma, e <-> f |- left_effect_row_body == right_effect_row_body
---------------------------------------------------------------
Gamma |- forall e. left_effect_row_body
      == forall f. right_effect_row_body
```

These are definitional conversion rules over alpha-bound syntax. They are not
observational equality rules and do not identify extensionally equivalent
Match programs.

## 3. Non-Goals

This migration must not:

- add `Eq`, path, dimension, transport, or coherence TermDB tags;
- add a `JUDGEMENT_EQ` relation;
- compare Match programs by testing their behavior on all inputs;
- insert object equality witnesses from successful conversion;
- use canonical hashes to complete a local equality decision;
- change TypeView identity or make Bool and Two definitionally equal;
- define equality for effectful computations;
- add a general unification solver;
- change the artifact field layout or numeric term tags;
- preserve an obsolete conversion helper for compatibility.

## 4. Current Implementation Audit

### 4.1 Local alpha comparison is already scoped

`src/prototype/term.c:603-613` defines `struct term_compare_env` with:

- a one-to-one term-binder correspondence;
- a one-to-one Match-frame correspondence;
- visited frame pairs for cross-link recursion.

The local shape comparator:

- pushes Lambda binders at `src/prototype/term.c:932-947`;
- pushes Match frames at `src/prototype/term.c:1040-1081`;
- pushes Match case binders at `src/prototype/term.c:835-855`;
- validates IH frames through the scoped map at
  `src/prototype/term.c:1115-1133`;
- pushes `EFFECT_ROW_FORALL` binders at
  `src/prototype/term.c:1172-1190`.

This proves that A Program can alpha-intern the existing Match syntax. It does
not mean that this shape comparator should become the conversion engine.

### 4.2 Conversion does not own a scope environment

`normalization_equal_at_depth` is declared at
`src/prototype/term.c:5669-5678` and implemented at
`src/prototype/term.c:6888-7375`. It recursively normalizes and compares terms,
but its parameters contain no binder or frame correspondence.

It currently handles binders through inconsistent mechanisms:

- Lambda substitutes the right binder with the left variable at
  `src/prototype/term.c:6998-7021`;
- Match case bodies are cloned through fresh common binders in
  `normalization_equal_match_case_bodies` at
  `src/prototype/term.c:5680-5740`;
- the computation/Match commuting conversion repeats the same substitution at
  `src/prototype/term.c:5842-5867`;
- `EFFECT_ROW_FORALL` creates a local shape-comparison environment and bypasses
  recursive conversion at `src/prototype/term.c:7218-7236`.

This duplication is the implementation reason K1 and K2 should be migrated
together.

### 4.3 K1 defect: linker key decides IH conversion

`match_frame_keys_equal` at `src/prototype/term.c:6863-6886` compares two frames
using canonical hash and summary fields. IH conversion calls it directly at
`src/prototype/term.c:7119-7136`.

A canonical key is a link lookup prefilter. It does not establish a scoped
one-to-one relation between recursive binders. A key collision or unrelated
same-shape Match can therefore influence kernel conversion.

### 4.4 K2 defect: forall conversion falls back to shape

The `EFFECT_ROW_FORALL` branch uses `shape_terms_equal_at_depth` rather than
`normalization_equal_at_depth`. It accepts alpha-identical bodies but does not
apply conversion recursively under the binder.

For example, the following bodies should be convertible under corresponding
binders when beta reduction is enabled:

```text
forall e. (lambda x. x) (effect-row-var e)
forall f. effect-row-var f
```

The current source-shape fallback cannot establish that conversion.

### 4.5 The public boundary is already suitable

`prototype_term_compare_for_conversion` at
`src/prototype/term.c:8121-8162` and its declaration at
`src/prototype/term.h:1009-1018` already expose a structured result. No public
signature change is required.

Typing and conversion-proof validation call this boundary indirectly. In
particular, `validate_conversion_proof` at
`src/prototype/typing.c:11490-11517` recomputes conversion and accepts only
`EQUAL`.

### 4.6 Recursive-field metadata is semantic in the current prototype

`struct prototype_case_binder` contains `is_recursive`, and artifact v61
serializes it. Typing and lowering consult it, while the local shape comparator
does not currently compare it directly. K1/K2 must not silently change this
policy while refactoring Match cases.

The re-audit found no artifact or TermDB validator that reconstructs this flag
from the authoritative constructor schema. Lowering computes it from classifier
conversion at `src/prototype/typing.c:3547-3555`, while the solver and IH proof
validation later trust the stored flag. It is therefore semantic data in the
current prototype, not a disposable cache.

K1 must preserve it by requiring corresponding case binders to have equal
`is_recursive` values in:

- local alpha identity;
- cross-database structural link validation;
- kernel Match conversion.

The v61 canonical hash is intentionally not changed in this migration. It may
select extra candidates, but full structural comparison must reject a recursive
flag mismatch. Do not let the answer depend on whichever alpha-interned Match
was inserted first.

## 5. Required Semantic Invariants

### 5.1 Binder correspondence

The conversion environment stores a lexical one-to-one relation:

```text
left binder <-> right binder
```

Rules:

1. a mapped left binder equals only its mapped right binder;
2. a mapped right binder cannot be paired with another left binder;
3. two free binders are equal only when their local binder IDs are equal;
4. mappings are pushed on binder entry and restored on scope exit;
5. capacity exhaustion is `EXHAUSTED`, never `NOT_EQUAL`.

### 5.2 Match-frame correspondence

Match frames are recursive binders scoped over their case bodies.

When comparing two Match terms:

1. compare scrutinees before entering frame scope;
2. require equal case counts;
3. if either Match has a frame, push a one-to-one frame pair, using
   `PROTOTYPE_INVALID_ID` for an absent partner;
4. an unreferenced one-sided frame is vacuous, while any IH reference to that
   frame fails against the absent partner;
5. compare constructor identity and owner before case-binder scope;
6. push corresponding case binders;
7. recursively convert the case bodies;
8. restore case-binder and frame scopes on every exit path.

When comparing two IH terms:

1. a mapped left frame equals only the mapped right frame;
2. a reverse-mapped right frame rejects any other left frame;
3. unmapped local frames require the same frame ID;
4. recursively convert IH arguments under the current scope;
5. never inspect `prototype_match_frame_key`.

Conversion does not follow `IH -> frame -> Match`, so ordinary lexical Match
comparison does not require link-style graph recursion. If implementation shows
otherwise, stop and document the cycle instead of adding canonical-key recovery.

### 5.3 Forall correspondence

For two `EFFECT_ROW_FORALL` terms:

1. save the binder scope depth;
2. push the forall binder pair;
3. recursively convert both bodies with the same normalization profile and
   remaining budget;
4. restore the scope on all paths.

`EFFECT_ROW_VAR` comparison must consult the binder correspondence. Free row
variables still require exact local binder identity.

### 5.4 Reduction and status propagation

The scope environment changes alpha handling only. Every recursive comparison
must continue to use:

- the selected normalization profile;
- the same definition/transparency environment;
- the shared remaining-step counter;
- the shared normalization status and reason;
- the existing depth limit.

Scope mismatch yields `NOT_EQUAL`. Malformed indices yield `INVALID`. Scope or
depth capacity exhaustion yields `EXHAUSTED`. Blocked effects remain
`BLOCKED_EFFECT`.

## 6. Internal Design

### 6.1 Conversion-owned scope state

Add one private structure in `src/prototype/term.c`, conceptually:

```c
struct conversion_scope_env {
	uint32_t binder_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t binder_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t binder_count;
	uint32_t frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_count;
};
```

This is compiler-local comparison state, not TermDB data, runtime state,
JudgementDB evidence, or an artifact record.

Do not copy the complete shape/link comparator policy into conversion. The
one-to-one mapping operations may be extracted into a small private shared
scope utility if that reduces duplication, but conversion and shape comparison
must retain separate recursive functions and separate semantic ownership.

### 6.2 Rename the recursive conversion helpers

The existing names hide that these functions decide conversion, not merely
normalization. During this migration rename:

- `normalization_equal_at_depth` to `conversion_equal_at_depth`;
- `normalization_equal_match_case_bodies` to
  `conversion_equal_match_case_bodies`;
- `normalization_equal_computation_match` to
  `conversion_equal_computation_match`.

Do not leave compatibility wrappers with the old private names.

The recursive entry receives `struct conversion_scope_env*`. The public
conversion APIs remain unchanged and initialize one empty environment per
comparison request.

### 6.3 Explicit variable cases

Add conversion branches for:

- `PROTOTYPE_TERM_VAR`;
- `PROTOTYPE_TERM_EFFECT_ROW_VAR`.

Both use the binder correspondence after WHNF. They must not rely on the
default `left_whnf == right_whnf` case.

### 6.4 Remove comparison-by-substitution

Replace fresh-binder substitution used only to compare bound bodies with scoped
correspondence:

- Lambda body comparison;
- Match case-body comparison;
- computation/Match commuting conversion case bodies.

This avoids manufacturing common-binder graph nodes solely to answer a kernel
comparison. Normalization may still add genuine reduction results to TermDB;
the prohibition here is narrower and concerns alpha alignment by substitution.

### 6.5 Match comparison implementation

Implement one conversion-owned Match case helper that:

- validates case and binder ranges before indexing arrays;
- compares constructor labels only for unresolved/non-authoritative cases;
- compares authoritative constructor owner and constructor ordinal otherwise;
- saves and restores binder count;
- recursively converts bodies under the enclosing frame pair.

The Match branch itself owns frame push/pop. Do not let each case independently
guess frame correspondence.

### 6.6 Computation/Match commuting conversion

`conversion_equal_computation_match` compares:

```text
Comp(E, match x { cases })
match x { cases -> Comp(E, body) }
```

It must receive the current conversion scope. It must compare the two
scrutinees before frame scope, pair the two result/outer Match frames, then
compare each branch under case-binder correspondence. It must not construct
fresh common binders.

This helper remains a definitional commuting conversion already selected by A
Program. K1/K2 do not broaden that rule.

### 6.7 Fast-path policy

The existing `prototype_term_view_shape_equal` fast path may remain because it
performs full local alpha-shape comparison and does not use link keys. It is
only a sufficient fast path.

Add a comment at the call site stating:

- local alpha shape may establish definitional reflexivity/congruence;
- artifact-link compatibility may not be substituted here;
- failure of the fast path must continue through recursive conversion.

## 7. Files and Expected Changes

### `src/prototype/term.c`

- add the private conversion scope environment;
- add binder/frame push, lookup, and restore handling;
- thread scope through every recursive conversion helper call;
- rename the three private conversion helpers;
- replace Lambda and Match comparison-by-substitution;
- implement scoped Match/IH comparison;
- preserve `case_binder.is_recursive` in local, link, and conversion comparison;
- implement recursive `EFFECT_ROW_FORALL` conversion;
- implement scoped `VAR` and `EFFECT_ROW_VAR` comparison;
- update the computation/Match commuting conversion;
- delete `match_frame_keys_equal`;
- retain link-key logic only in the named cross-database link comparator.

### `src/prototype/term.h`

No semantic or serialized type change is expected. Public conversion signatures
should remain unchanged. Edit only if a comment must clarify the boundary.

### `src/prototype/conversion_scope_check.c`

Add a focused test program for scoped kernel conversion. Do not overload the
term-identity test with conversion policy.

### `src/prototype/test_conversion_scope.sh`

Add a `-Wall -Wextra -Werror` driver. It should also fail if the private
`match_frame_keys_equal` helper reappears in `term.c`.

### Existing tests

- keep `term_identity_frame_check.c` focused on interning and link shape;
- keep `conversion_result_check.c` focused on status algebra and budgets;
- update them only where a renamed private behavior changes their stated
  expectations;
- extend `test_artifact_flow.sh` only if a focused recursive conversion proof
  can be exercised through artifact replay without adding a brittle numeric-ID
  fixture.

## 8. Required Characterization Tests

Add tests before changing the comparator.

### 8.1 K1 Match/IH tests

1. Identical IH node IDs are `EQUAL` by reflexivity.
2. Two isolated IH nodes with different unmapped frames are `NOT_EQUAL` even
   when their arguments and forged frame keys are identical.
3. Recursive Matches whose binders and frames differ but whose bodies are alpha
   equivalent are `EQUAL`.
4. Distinct recursive Match nodes whose corresponding bodies become equal only
   after beta reduction are `EQUAL`.
5. A Match body referring to a foreign frame is not equal to one referring to
   its enclosing frame.
6. A one-sided internal frame is ignored when vacuous, but yields `NOT_EQUAL`
   when a case body refers to it as an enclosing recursive frame.
7. Nested recursive Matches preserve the inner and outer frame pairings.
8. Reusing one right frame for two left frames is rejected.
9. Different constructors, constructor owners, case counts, recursive-frame
   reference structures, or recursive-binder flags remain `NOT_EQUAL`.
10. A deliberately forged canonical-key collision does not change any local
    conversion result.
11. Cross-artifact link shape still accepts valid relocated recursive Matches
    and rejects a forged key collision after full structural validation.

Because local interning may merge purely alpha-identical Matches, test 4 should
construct distinct nodes by giving one body a beta-redex and the other its
contractum. That exercises recursive conversion rather than only the alpha
shape fast path.

### 8.2 K2 forall tests

1. Alpha-renamed forall binders are `EQUAL`.
2. A beta-redex body and its contractum are `EQUAL` under corresponding forall
   binders.
3. Nested forall binders do not cross-map.
4. A bound row variable and an unrelated free row variable are `NOT_EQUAL`.
5. Two different free row variables are `NOT_EQUAL`.
6. Body comparison preserves `EXHAUSTED` under a zero or insufficient budget.
7. Body comparison preserves `BLOCKED_EFFECT` if an invalid effect dependency
   is deliberately presented to pure conversion.

### 8.3 General regression tests

1. Lambda alpha conversion still passes.
2. Pi conversion still compares domains and codomain families correctly.
3. TypeView/core sharing does not erase view identity.
4. Bool and Two are not made convertible by common representation shape.
5. Append normalization remains equal before and after artifact readback.
6. Conversion-proof replay accepts only structured `EQUAL`.

## 9. Artifact and Compatibility Decision

No artifact field, numeric tag, or serialized graph edge changes in K1/K2, so
the writer and reader remain v61.

The kernel meaning does change:

- K1 removes an invalid equality path based on linker metadata;
- K2 recognizes conversion under a binder that was previously limited to
  source-shape equality.

Therefore:

1. all v61 artifacts are revalidated by the new kernel on read/link;
2. no compatibility implementation for the old comparator is retained;
3. the semantic change is recorded in this plan and the V2 audit;
4. artifact round-trip, link, conversion-proof, and append-normalization tests
   are mandatory;
5. v62 remains reserved for the coordinated HOTT/proof-payload schema break.

If implementation reveals that a writer-produced v61 artifact requires a
canonical frame key to validate a legitimate conversion proof, stop. That means
the artifact lacks authoritative recursive scope information and the format
must be revised rather than restoring the shortcut.

## 10. Implementation Phases

### Phase A: Characterize current behavior

- [x] Add `conversion_scope_check.c`.
- [x] Add `test_conversion_scope.sh`.
- [x] Add forged frame-key collision coverage.
- [x] Add distinct beta-convertible recursive Match coverage.
- [x] Add beta-convertible `EFFECT_ROW_FORALL` coverage.
- [x] Classify `case_binder.is_recursive` as current semantic Match data.
- [x] Add recursive-binder flag mismatch characterization.
- [x] Confirm tests expose K1 and K2 before implementation.

Exit criterion: the focused test distinguishes the intended semantics from both
current defects.

### Phase B: Add conversion scope

- [x] Add the private conversion scope environment.
- [x] Add one-to-one binder mapping operations.
- [x] Add one-to-one Match-frame mapping operations.
- [x] Define capacity/status behavior.
- [x] Initialize one empty scope at the public conversion boundary.
- [x] Thread the scope through all recursive conversion calls.

Exit criterion: code compiles with no conversion branch silently creating a new
empty nested scope.

### Phase C: Implement K1

- [x] Compare Match scrutinees outside frame scope.
- [x] Push and restore the Match-frame pair.
- [x] Push and restore case-binder pairs.
- [x] Require corresponding case binders to preserve `is_recursive`.
- [x] Compare IH frames through the scope.
- [x] Convert IH arguments recursively.
- [x] Migrate computation/Match commuting conversion.
- [x] Delete comparison-by-substitution from Match conversion.
- [x] Delete `match_frame_keys_equal`.

Exit criterion: every K1 focused test passes and canonical frame keys are absent
from local conversion.

### Phase D: Implement K2

- [x] Add scoped `VAR` conversion.
- [x] Add scoped `EFFECT_ROW_VAR` conversion.
- [x] Push and restore `EFFECT_ROW_FORALL` binders.
- [x] Recursively convert forall bodies.
- [x] Remove the source-shape fallback from the forall conversion branch.
- [x] Migrate Lambda alpha comparison from substitution to the same scope.

Exit criterion: alpha and beta conversion under forall pass, while free/bound
row variables remain distinct.

### Phase E: Cleanup and ownership audit

- [x] Rename private normalization-equality helpers to conversion helpers.
- [x] Remove unused substitution-only comparison code paths.
- [x] Audit every call to `shape_terms_equal_at_depth` from conversion code.
- [x] Audit every canonical-key use in `term.c` and classify it as interning,
  diagnostics, or artifact linking.
- [x] Confirm no public API or artifact schema changed accidentally.
- [x] Update comments to distinguish alpha shape, link compatibility, and
  conversion.

Exit criterion: no helper name or comment implies that link shape is kernel
conversion.

### Phase F: Full validation

- [x] Run `make -j2`.
- [x] Run `test_conversion_scope.sh`.
- [x] Run `test_conversion_result.sh`.
- [x] Run `test_term_identity_frame.sh`.
- [x] Run every `src/prototype/test_*.sh` script.
- [x] Run examples 01-07 and 09 if not already covered by the scripts.
- [x] Run artifact write/read/link/revalidation tests.
- [x] Run `git diff --check`.

Exit criterion: all checks pass and the artifact header remains v61.

## 11. Stop Conditions

Stop implementation and revise this plan if any of the following occurs:

- a valid IH can refer to a Match frame that is not lexically enclosing it;
- recursive Match conversion needs to recover scope from a canonical key;
- normalization can replace a scoped Match with a new Match whose frame cannot
  be related to the original scope;
- effect-row binders are intentionally generative identities rather than alpha
  binders;
- the conversion scope must be serialized or stored in TermDB;
- K1/K2 require object equality or proof search;
- a writer-produced v61 artifact loses information required for scoped replay;
- the shape fast path proves equality using cross-artifact link policy.

Do not address a stop condition with a mode bit, a frame-ignore flag, a key-only
fallback, or a compatibility wrapper.

## 12. Progress Sheet

| ID | Work item | Status | Primary files | Evidence |
| --- | --- | --- | --- | --- |
| A1 | Characterize scoped IH conversion | complete | `conversion_scope_check.c` | nested, foreign, vacuous, and key-collision Match tests pass |
| A2 | Characterize forall conversion | complete | `conversion_scope_check.c` | alpha/beta, nested, budget, and blocked-effect tests pass |
| B1 | Add conversion binder scope | complete | `term.c` | scoped VAR tests pass |
| B2 | Add conversion frame scope | complete | `term.c` | nested Match tests pass |
| B3 | Thread scope through recursion | complete | `term.c` | `-Werror` focused build passes |
| C1 | Replace Match comparison-by-substitution | complete | `term.c` | no fresh alignment nodes remain in conversion |
| C2 | Replace IH key comparison | complete | `term.c` | forged collision rejected; static gate passes |
| C3 | Migrate computation/Match conversion | complete | `term.c` | dependent Pi and commuting conversion tests pass |
| C4 | Preserve recursive-binder metadata | complete | `term.c` | local/link/conversion mismatch tests pass |
| D1 | Make forall recursively convertible | complete | `term.c` | beta body accepted |
| D2 | Scope row variables | complete | `term.c` | nested and free/bound distinctions pass |
| E1 | Delete old helpers and key path | complete | `term.c` | static `rg` gate passes |
| E2 | Complete semantic ownership audit | complete | `term.c`, plan | conversion uses only local alpha fast path |
| F1 | Focused validation | complete | test scripts | all focused tests pass |
| F2 | Full regression matrix | complete | prototype tests | all 13 scripts pass |
| F3 | Artifact v61 revalidation | complete | artifact tests | round-trip/link pass; header remains v61 |

## 13. Decision Log

| Date | Decision | Reason | Revisit condition |
| --- | --- | --- | --- |
| 2026-08-06 | Implement K1 and K2 together | Both require lexical conversion scope and removal of comparison-by-substitution | Scope requirements diverge materially |
| 2026-08-06 | Preserve Match alpha-interning | Match case binders and IH frames are unobservable lexical binders | Frame identity becomes observable or generative |
| 2026-08-06 | Keep conversion separate from shape/link comparison | They answer different semantic questions | Never; this is a kernel boundary |
| 2026-08-06 | Keep artifact v61 | No serialized schema changes | Legitimate v61 replay lacks scoped information |
| 2026-08-06 | Do not add HOTT terms in this migration | K1/K2 are prerequisites, not object equality | K1/K2 complete and finite calculus frozen |
| 2026-08-06 | Treat `case_binder.is_recursive` as semantic in v61 | Solver and proof validation trust the serialized flag; no authoritative reconstruction validator exists | A later schema derives and validates it instead |
| 2026-08-06 | Treat a one-sided unreferenced Match frame as vacuous | Solver-generated motives retain an internal frame while the reduced surface family may not; only an IH reference observes frame ownership | Match frames become object-level generative binders |

## 14. Completion Definition

K1/K2 are complete only when:

- kernel conversion owns explicit binder and Match-frame scope;
- recursive Match alpha conversion succeeds without canonical keys;
- unrelated IH frames remain unequal despite identical or forged keys;
- recursive-binder flags survive interning, linking, and conversion;
- conversion under `EFFECT_ROW_FORALL` performs ordinary recursive conversion;
- alpha alignment no longer clones bodies through fresh common binders;
- structured status and budget propagation remain intact;
- shape and link comparison behavior remains unchanged;
- every focused and repository-wide regression test passes;
- artifact v61 round-trip and proof revalidation pass;
- the V2 audit progress table is updated with implementation evidence.

## 15. Implementation Result

The implementation reuses one private `term_scope_env` mapping utility across
local shape, cross-link shape, and conversion. The recursive algorithms remain
separate: conversion does not call the cross-link comparator or consume frame
keys.

The implementation audit found one necessary correction to the original plan.
`build_operation_uniform_motive` can produce a non-recursive Match with an
internal frame, while beta reduction of the corresponding surface family
produces the same Match without that frame. Rejecting this pair broke
`dependent_pi_surface_check.p`.

Conversion now pairs a missing frame with `PROTOTYPE_INVALID_ID`. Therefore:

- a vacuous internal frame does not change conversion;
- an IH referring to that frame cannot equal a free or foreign IH;
- two present recursive frames still require a lexical one-to-one pairing;
- canonical frame keys remain irrelevant to local conversion.

Validation on 2026-08-06 completed:

- `make -j2` and `make reader`;
- all 13 `src/prototype/test_*.sh` scripts;
- examples 01-07 and 09 with a freshly rebuilt reader;
- artifact v61 write/read/link/revalidation and append normalization;
- `git diff --check`.
