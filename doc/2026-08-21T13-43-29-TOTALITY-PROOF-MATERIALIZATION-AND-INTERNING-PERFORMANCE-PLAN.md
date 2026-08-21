# Totality Proof Materialization and Interning Performance Plan

Date: 2026-08-21

Status: Complete

Baseline commit: `1471a53`

Implementation baseline: `b13f047`

Predecessor plan:
`doc/2026-08-19T00-45-00-QUICKSORT-SINGLE-COMPILE-PERFORMANCE-AND-AUTHORITY-CONSOLIDATION-PLAN.md`

## 1. Purpose

The earlier QuickSort work reduced one compile from about 208 seconds to about
5.4 seconds. Subsequent explicit computation-totality work did not materially
slow the common pre-totality input, but forming the open
`quickSortTerminates` theorem raises the current median to about 7.86 seconds.

This plan addresses that added proof-formation cost and the remaining dominant
compiler costs. It must improve the general implementation rather than special
case QuickSort or omit accepted proof evidence.

The target is:

> Construct each semantic proof fact once, intern alpha-equivalent Core Terms
> through a discriminating canonical index, and make later evidence publication
> close only the affected proof dependencies.

The work may remove implementation or proof-rule paths only after showing that
they are semantically redundant. Migration size and compatibility shims are not
reasons to retain a redundant path.

## 2. Design Invariants

The optimization must preserve the following A Program boundaries.

- Compilation and type checking are prior computation. Performance state may
  accelerate that computation but may not become semantic authority.
- TermDB remains the canonical untyped Core computation graph.
- TypedOccurrenceGraph identifies the source operation and its classifier,
  Context, and evidence ownership. A shared Core Term does not merge distinct
  typed occurrences.
- Constraint solutions are classifier/effect/usage authority before kernel
  materialization. A cache may project a solution but may not create one.
- Proposition, Claim, and Derivation remain explicit accepted evidence.
- Distinct valid Derivation DAGs are not overwritten merely because their
  conclusions coincide.
- Lambda and Match alpha identity, nominal TypeView identity, and IH frame
  identity retain their present semantics.
- A hash collision is never evidence of equality. Deep comparison remains the
  collision validator.
- Runtime indexes, memo tables, queues, and profiling counters are not
  serialized.
- APP, MATCH, IH, and COMPUTATION_FOLD proof rules remain explicit. Similar
  plumbing does not make their kernel rules interchangeable.
- Totality is not removed merely to make the benchmark faster.

## 3. Reproducible Measurements

Only adjacent implementations are compared. This plan does not require one
input to compile under every historical compiler generation.

### 3.1 Common-input comparison

The fixture at `c934be9`, before `quickSortTerminates` was added, was compiled
with three nearby revisions.

| Compiler | Median wall | Term formations | Unique Terms | Alpha compares | Solver pops |
|---|---:|---:|---:|---:|---:|
| `5050f11` | 5.426 s | 976,163 | 15,948 | 1,708,065 | 7,381 |
| `c934be9` | 5.752 s | 976,168 | 15,948 | 1,708,065 | 7,381 |
| `1471a53` | 5.516 s | 976,168 | 15,948 | 1,708,065 | 7,381 |

The `c934be9` replayable result-Context change and the later explicit totality
field do not show an algorithmic regression on the common input. The small wall
differences are within the resolution expected from three local runs.

### 3.2 Current totality theorem cost

The current fixture adds:

```text
quickSortTerminates := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	#.terminates (&(quickSort A &le xs));
```

| Metric | Common fixture | With theorem | Increase |
|---|---:|---:|---:|
| Median wall | 5.516 s | 7.862 s | 2.346 s / 42.5% |
| Term formations | 976,168 | 1,095,310 | 119,142 / 12.2% |
| Unique Terms | 15,948 | 19,258 | 3,310 / 20.8% |
| Alpha compares | 1,708,065 | 2,064,671 | 356,606 / 20.9% |
| Normalization misses | 7,047 | 11,584 | 4,537 / 64.4% |
| Solver pops | 7,381 | 7,493 | 112 / 1.5% |
| Context visits | 2,801 | 2,819 | 18 / 0.6% |

This is primarily proof construction and Term reconstruction, not classifier
equation solving, Context resolution, or Substitution index rebuilding.

### 3.3 Profile of the current full fixture

A temporary `-pg` build identified these dominant calls:

| Function or operation | Observed calls or share |
|---|---:|
| `shape_terms_equal_at_depth` | 35.54%; 52,935,073 recursive calls |
| `term_scope_env_push_binder` | 12.40%; 66,711,049 calls |
| `shape_term_compare_id` | 10.74%; 617,590,910 calls |
| `shape_match_cases_equal_at_depth` | 5.23%; 3,967,454 calls |
| `propositions_equal` | 2.20%; 21,860,569 calls |
| `operation_solver_reify_core_proof` | 3,330 roots and 35,299 recursive calls |
| `add_term` | about 1.10 million calls |

The dominant Term formations under `add_term` were approximately:

| Tag | Formation requests |
|---|---:|
| APP | 303,654 |
| VAR | 295,311 |
| TYPE_VIEW | 187,749 |
| TYPE_DECLARATION | 187,743 |
| LAMBDA | 34,732 |
| PI | 25,048 |
| RETURN | 24,457 |
| THUNK | 24,301 |

Only 19,258 Terms survive as unique nodes. Repeated proof reification therefore
creates most candidates only to rediscover an existing Term.

## 4. Root Causes

### RC1. Lambda intern keys are too coarse

`term_intern_canonical_key_local()` in
`src/prototype/src/core/term/canonicalization.inc` gives an ordinary Lambda a
key based mainly on its body root tag. It does not include a recursive
alpha-normal fingerprint of the body.

Consequently, most Lambdas whose bodies begin with the same tag enter the same
candidate bucket. `add_term()` then calls the full recursive alpha comparator.
About 2.054 million of the 2.065 million current alpha comparisons are Lambda
comparisons.

This is the largest demonstrated remaining bottleneck.

### RC2. Proof reification repeats successful subgraphs

`operation_solver_reify_core_proof()` first checks accepted JudgementDB
evidence, but evidence successfully built in the current JudgementDelta pass is
not a direct memoized terminal state for that occurrence. Multiple parent
occurrences can recursively reify the same child before publication.

The proof fixed point also scans every Match and every typed occurrence until
the proposition count stops changing. Successful facts are rediscovered rather
than scheduled once by dependency.

### RC3. Materialization is invoked twice as a full pass

`compile_pending_with_workspace()` calls
`operation_solver_materialize_judgements()` before result-evidence publication
and again afterwards. The second pass is semantically motivated: newly accepted
`Returns` evidence can discharge later premises. The implementation, however,
repeats full occurrence scans and checked computation-rule closure.

The required semantic closure is real; the second global traversal is not.

### RC4. Constructor proof reification repeatedly specializes schemas

Constructor-spine proof formation repeatedly derives expected domains,
reindexes parameter and field classifiers, forms TypeDeclaration and TypeView
Terms, and reconstructs curried Lambda/Pi classifiers. The declaration-level
classifier cache does not cover specialization by parameter spine, field
arguments, Context, and Substitution.

The high TYPE_DECLARATION and TYPE_VIEW formation counts are the visible result.

### RC5. Totality evidence has two introduction paths but only one compiler path

The kernel currently contains:

- `TERMINATES_TOTAL_COMPUTATION`, derived from a computation classifier whose
  totality is `TOTAL`; and
- `TERMINATES_FROM_RETURNS`, derived from explicit `Returns(M, v)` evidence.

The source compiler currently emits the first path. The second is used by a
focused kernel test and replay support but has no compiler producer.

These rules are not automatically duplicates: `Returns(M, v)` is a stronger
object-level witness that can imply termination even when totality was not
otherwise inferred. Nevertheless, retaining both without a declared proof
theory increases artifact and replay surface. Phase TP1 must decide whether the
second rule is an intentional public theorem or dead transitional machinery.

### RC6. Totality type formation creates evidence late and imperatively

`compile_phase_publish_result_evidence()` walks termination-witness occurrences,
creates a Universe variable, publishes `Terminates` type formation, and then
publishes the witness. This bypasses the ordinary dependency scheduling used
for structural occurrence evidence and is the reason a later full closure pass
is needed.

The type and witness remain normal object-language Terms and Claims. Their
publication should enter the same evidence dependency graph as other late
introduction rules.

### RC7. Normalization invalidations rose, but are secondary

The theorem increases normalization misses from 7,047 to 11,584 and current
invalidations from 169 to 833. Cache hits remain high, and profiling places
alpha interning and proof formation above normalization. Invalidation scope
must be audited after RC1-RC4 so that it is not optimized around work that will
disappear.

### RC8. Context and Substitution rebuilds are not the current issue

The current benchmark reports zero ContextDB and SubstitutionDB index rebuilds.
Context visits rise by only 18 with the theorem. The immutable incremental
Context work should remain intact, but it is not part of the critical path for
this plan.

## 5. Target Architecture

### 5.1 Alpha-canonical fingerprint index

Every intern candidate receives a strong alpha-canonical fingerprint, and the
root fingerprint is retained in the Term intern index.
Bound variables and Match binders are represented in the fingerprint by their
lexical binding correspondence, not by source names or raw Binding IDs. This is
an internal canonical key; it does not expose De Bruijn indices as the Core
representation.

The lookup becomes:

```text
tag + alpha fingerprint + cheap arity data
    -> collision bucket of canonical Term IDs
    -> deep alpha comparison only for a true fingerprint collision
```

Free Binding IDs, free IH frame identities, nominal TypeViews, operation IDs,
and other intentionally nominal components remain in the fingerprint.

The existing recursive canonical-hash implementation is the one authority.
Open child Terms do not receive a context-free cached fingerprint: their hash
depends on the enclosing binder and IH-frame environment. Caching those child
hashes independently would be unsound. The already-computed root alpha key is
reused by exact hashing.

### 5.2 Per-materialization evidence state

Maintain runtime-only state per typed occurrence:

```text
UNSEEN
IN_PROGRESS
SUCCEEDED
```

`SUCCEEDED` is reusable throughout the materialization transaction.
`IN_PROGRESS` detects recursive cycles. A residual result is deliberately reset
to `UNSEEN` and entered into the pending worklist; it is not cached as failure.

The key is occurrence identity plus the frozen classifier, Context, and evidence
generation. Core Term ID alone is insufficient because typed occurrences may
share Core while carrying distinct classifiers or Contexts.

### 5.3 Evidence dependency worklist

The initial structural pass records only residual occurrence IDs. Late result
evidence reruns that pending set, not the complete TypedOccurrenceGraph. This is
the minimal dependency worklist required by the measured pipeline; a second
general reverse index was not added because it would duplicate existing
constraint dependency indexes without reducing the residual set further.

The pipeline becomes:

```text
frozen solved occurrence graph
    -> seed structural proof worklist
    -> publish accepted candidates
    -> publish Returns/Terminates facts
    -> enqueue newly enabled consumers
    -> close worklist
```

There is no second full materialization pass.

### 5.4 Constructor specialization authority

The audit found an existing revisioned constructor classifier cache owned by
TypeDeclarationDB. All measured classifier requests hit that cache after the
initial declaration build. Solver and namespace callers now use that one API
instead of directly deriving the curried classifier.

The remaining parameter/field reindexing validates dependent constructor
domains in the current Context. A second transaction cache for these products
was rejected: the measured cache miss count is zero, and another mutable result
store would create a competing projection authority for little demonstrated
benefit.

Kernel candidate replay still validates the explicit premises. Dense artifact
relocation validates a persisted cache by nominal view-shape alpha equality,
not local Term-ID equality; the cache remains non-authoritative.

## 6. Implementation Phases

The checklists below preserve the original audit proposal. The phase `Status`
and `Implementation result` paragraphs are the completion authority: some
unchecked proposed mechanisms were intentionally replaced or rejected after
measurement, rather than implemented as redundant caches or indexes.

### TP0. Permanent attribution counters

Status: complete

Implementation result: permanent phase, Term-tag, alpha-comparison,
normalization-invalidation, constructor-specialization, proof-reification, and
post-result retry counters are emitted by the fixture-selectable benchmark.

- [ ] Extend the single-compile benchmark to accept an explicit fixture while
      retaining the current default fixture.
- [ ] Report separate timings for lowering/fixed point, proof materialization,
      result evidence publication, second closure, and artifact work.
- [ ] Count proof-reification root calls, recursive calls, successes, residuals,
      current-pass reuse, accepted-evidence reuse, and cycles.
- [ ] Count materialization rounds and typed occurrences visited per round.
- [ ] Count Terms formed and unique Terms by tag, not only intern probes by tag.
- [ ] Count alpha-fingerprint bucket lengths and actual deep-comparison node
      visits.
- [ ] Count constructor specialization attempts, cache hits, reindex calls, and
      produced TYPE_DECLARATION/TYPE_VIEW Terms.
- [ ] Count how many Claims late Returns/Terminates publication adds and how
      many consumers it actually enables.

Exit criteria:

- [ ] The common fixture and theorem fixture can be compared in one report.
- [ ] The 2.35-second theorem delta is attributed to named compiler phases.
- [ ] Instrumentation changes do not alter artifact bytes.

### TP1. Audit and trim totality-specific proof surface

Status: complete

Implementation result: both rules are retained intentionally.
`TERMINATES_TOTAL_COMPUTATION` requires a structurally `TOTAL` computation.
`TERMINATES_FROM_RETURNS` is the elimination of stronger object evidence and is
used when the classifier is not already total. Exact type-formation selection
prevents duplicate Universe allocation. Positive and negative kernel tests cover
both paths.

- [ ] Specify the object rules for `Returns(M, v)` and `Terminates(M)` and state
      whether `Returns` introduction is intended to imply `Terminates`.
- [ ] Decide whether `TERMINATES_FROM_RETURNS` is a supported theorem rule. If
      yes, add an actual elaboration/use boundary and retain replay tests. If no,
      remove the rule, API, enum/schema entry, replay branch, and isolated test;
      do not retain a compatibility alias.
- [ ] Verify that `TERMINATES_TOTAL_COMPUTATION` is not circular: the premise
      totality must be structurally derived before the witness Claim is added.
- [ ] Ensure `Terminates` type formation is interned/selected by exact
      Proposition before allocating another Universe variable.
- [ ] Remove any duplicate publication path that emits the same Proposition and
      proof rule more than once.
- [ ] Keep the Term former pair only if both are required to represent the type
      and its witness as ordinary object Terms.

Exit criteria:

- [ ] Every retained totality rule has a source or public kernel use case, an
      accepted replay rule, and a negative test.
- [ ] No dead proof kind remains solely because it already has an artifact tag.
- [ ] Totality semantics and current positive/negative behavior are unchanged.

### TP2. Memoize successful occurrence proof reification

Status: complete

Implementation result: occurrence/Context/classifier keyed transaction-local
states reuse successful reification, detect cycles, and never retain a residual
as failure.

Primary files:

- `src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc`
- compile workspace declarations in lowering modules

Tasks:

- [ ] Add the transaction-local reification state table.
- [ ] Consult accepted evidence and current JudgementDelta evidence through one
      read-only selection interface.
- [ ] Mark `IN_PROGRESS` before descending and `SUCCEEDED` only after a complete
      candidate is present.
- [ ] Associate residual retry with a premise/evidence generation instead of
      retrying on every outer scan.
- [ ] Preserve occurrence authority when alpha-equivalent occurrences share a
      Core Term.
- [ ] Remove repeated linear binder-assumption searches where the selected
      evidence index already provides the same answer.

Tests:

- [ ] shared identity Core under Bool and Nat annotations;
- [ ] shared constructor/Lambda subgraphs under distinct Contexts;
- [ ] recursive Match/IH proof cycle detection;
- [ ] residual premise becomes available and retries exactly once;
- [ ] distinct derivations for one Proposition remain accepted.

Exit criteria:

- [ ] A successful occurrence is structurally reified at most once per
      materialization transaction.
- [ ] Recursive reification calls fall by at least 50% on the theorem fixture.
- [ ] Term formation requests fall without changing accepted Claims.

### TP3. Replace full proof fixed points with a worklist

Status: complete with the Section 5.3 adaptation

Implementation result: proposition-count convergence and the second complete
occurrence traversal were removed. The post-result closure processes 61
recorded residual consumers on the full fixture, rather than all 755
occurrences. The function is invoked twice, but the second invocation is a
pending-only closure.

- [ ] Build reverse evidence dependencies for structural occurrence rules,
      Match motives, computation constraints, Returns, and Terminates.
- [ ] Seed only occurrences with solved classifiers and available premises.
- [ ] Make result-evidence publication return the evidence keys it introduced.
- [ ] Enqueue newly enabled consumers instead of invoking
      `operation_solver_materialize_judgements()` a second time.
- [ ] Run checked computation-rule closure only when a relevant premise or
      result changes.
- [ ] Retain a diagnostic non-convergence guard; it is not semantic fuel.
- [ ] Delete the all-occurrence second pass and proposition-count convergence
      loop after equivalence tests pass.

Exit criteria:

- [ ] Each occurrence visit is caused by initial seeding or a recorded changed
      dependency.
- [ ] The current theorem requires no second global occurrence scan.
- [ ] Late evidence still closes APP/Lambda/Match consumers before expectations.

### TP4. Cache constructor specialization products

Status: complete by audit; proposed second cache rejected

Implementation result: direct curried-classifier derivations now use the one
TypeDeclarationDB revision cache. The full fixture records 1,223 hits and zero
misses. Dependent field reindexing remains explicit kernel validation; no
parallel specialization authority was introduced.

- [ ] Define the immutable specialization key from semantic IDs already owned
      by TypeDeclarationDB, ContextDB, and SubstitutionDB.
- [ ] Cache `constructor_spine_expected_domains` results.
- [ ] Cache the saturated/unsaturated result classifier of a spine.
- [ ] Reuse one specialization in both expected-domain validation and proof
      recording during the same occurrence transaction.
- [ ] Remove duplicated TypeDeclaration/TypeView and curried classifier
      reconstruction after all callers use the shared result.
- [ ] Invalidate by immutable semantic revision or dispose the transaction-local
      cache; never mutate cached semantic nodes.

Exit criteria:

- [ ] Constructor specialization calls are proportional to distinct semantic
      spines, not proof parents or materialization rounds.
- [ ] TYPE_DECLARATION and TYPE_VIEW formation requests fall substantially.
- [ ] Dependent constructor, Acc, and IADT tests retain exact classifiers.

### TP5. Strengthen TermDB alpha interning

Status: complete with the Section 5.1 adaptation

Implementation result: Lambda, Match, and EFFECT_ROW_FORALL roots use the full
recursive alpha fingerprint. Exact hashing reuses that key. The collision
validator remains the deep comparator, and tests cover nested binders, free
Binding identity, and forced collisions. Context-free child fingerprint caching
was rejected for open Terms.

Primary files:

- `src/prototype/include/a_program/core/term.h`
- `src/prototype/src/core/term/canonicalization.inc`
- `src/prototype/src/core/term/storage_and_formation.inc`

Tasks:

- [ ] Replace the Lambda body-tag key with a full alpha-canonical fingerprint.
- [ ] Apply the same principle to Match and EFFECT_ROW_FORALL binders.
- [ ] Cache fingerprints for immutable stored Terms.
- [ ] Reuse child fingerprints when forming parent fingerprints.
- [ ] Keep a collision bucket and authoritative deep alpha comparison.
- [ ] Add a debug audit that compares indexed selection with the current
      comparator on bounded test stores.
- [ ] Preserve earliest-ID canonical selection and deterministic artifacts.
- [ ] Delete the coarse binder-key code after the strong index passes the audit;
      do not retain two production canonical-key algorithms.

Tests:

- [ ] many non-equivalent Lambdas with the same body root tag;
- [ ] alpha-equivalent nested Lambdas with fresh Binding IDs;
- [ ] Match pattern binders and recursive IH frames;
- [ ] free Binding IDs remain distinct;
- [ ] nominal TypeViews remain distinct;
- [ ] forced fingerprint collisions never merge unequal Terms;
- [ ] artifact bytes remain deterministic.

Exit criteria:

- [ ] Lambda deep alpha comparisons fall by at least 70% from 2,054,145.
- [ ] Deep comparison normally occurs only inside a matching strong-fingerprint
      bucket.
- [ ] No second alpha-identity authority is introduced.

### TP6. Audit normalization invalidation after structural fixes

Status: complete

Implementation result: all invalidations are attributed to graph mutation, IH
scope mutation, or TypeFormer completion. Empty-cache invalidation now advances
the graph revision without clearing empty arrays. No effectful normalization is
cached.

- [ ] Re-measure misses, evictions, and invalidations after TP2-TP5.
- [ ] Identify each invalidation reason and the semantic mutation that requires
      it.
- [ ] Narrow invalidation to graph revisions that affect the cached key.
- [ ] Remove duplicate invalidations caused only by temporary candidate
      append/rollback if fingerprint formation no longer needs such mutation.
- [ ] Do not cache effectful execution as conversion normalization.

Exit criteria:

- [ ] Every invalidation has a documented semantic cause.
- [ ] The theorem fixture no longer invalidates the complete useful cache for a
      local proof candidate.

### TP7. Secondary proposition/evidence indexes

Status: skipped after profiling

`propositions_equal` still has many calls, but no sampled self time in the final
profile. JudgementDB already owns proposition, Claim, and Derivation indexes.
Adding another key/index authority is not justified by the remaining cost.

Perform only after re-profiling TP2-TP6.

- [ ] If `propositions_equal` remains material, index full Proposition identity
      and candidate premise identity by one shared immutable key definition.
- [ ] Replace repeated candidate-authority scans with direct selected-evidence
      references where authority is already fixed.
- [ ] Do not collapse multiple Derivations into one proof merely to reduce
      lookup cost.
- [ ] Skip this phase if profiling no longer justifies its complexity.

## 7. Removal Policy

The implementation is expected to delete code, not merely add caches.

Candidates for deletion after replacement are:

- the second full `operation_solver_materialize_judgements()` invocation;
- proposition-count-based all-occurrence materialization convergence;
- coarse Lambda/Match binder intern keys;
- repeated constructor classifier/domain reconstruction paths;
- a totality proof rule with no intentional object-language role;
- duplicate linear scans superseded by authoritative indexes;
- counters or temporary comparison paths used only during migration.

The following are not deletion candidates for this performance work:

- explicit totality information in `COMPUTATION_TYPE`;
- `Terminates(M)` as an object proposition and its witness Term;
- accepted Claim and Derivation evidence;
- TypedOccurrence identity above shared Core Terms;
- ContextDB or SubstitutionDB;
- distinct kernel rules whose premises and conclusions differ.

## 8. Performance Gates

Use a default non-profiler build, one warm-up, and the median of three measured
runs. Record the commit and fixture hash.

| Gate | Required | Preferred |
|---|---:|---:|
| Common pre-theorem fixture | no regression over 5.75 s | at most 5.25 s |
| Current full IF8 fixture | at most 6.25 s | at most 5.50 s |
| Lambda alpha compares | at most 616,000 | at most 250,000 |
| Recursive proof reification | at most 17,650 | at most 10,000 |
| Full second materialization scans | 0 | 0 |

Intermediate phases must also report relative changes in Term formations,
unique Terms, constructor specialization attempts, normalization misses, and
accepted Claim/Derivation counts. A wall-time improvement alone does not permit
removing evidence.

## 9. Verification Matrix

After each phase run the narrow tests for the changed authority. Before final
completion run:

- [ ] Term interning collision and alpha-identity tests;
- [ ] result evidence and totality rule tests;
- [ ] IF8 fuel-free QuickSort source and artifact test;
- [ ] dependent Match, Acc, and indexed-family tests;
- [ ] shared-Core/different-annotation tests;
- [ ] artifact malformed-proof rejection;
- [ ] artifact deterministic byte comparison;
- [ ] examples 01-09;
- [ ] complete integration suite.

The current accepted artifact version is v82. Runtime-only optimization does
not require an artifact bump. Removing or changing a serialized proof kind does
require a new schema version, with no compatibility fallback unless separately
approved.

## 10. Progress Dashboard

| Phase | Status | Evidence |
|---|---|---|
| TP0 attribution counters | complete | permanent benchmark counters |
| TP1 totality surface audit | complete | both rules retained with distinct premises and negative tests |
| TP2 proof reification memo | complete | 227 recursive calls; 344 current-pass reuses |
| TP3 evidence worklist | complete | 61 residual retries; zero second full scans |
| TP4 constructor authority | complete by audit | 1,223 existing-cache hits; zero misses; no duplicate cache |
| TP5 alpha fingerprint index | complete | 665 deep comparisons on the full fixture |
| TP6 normalization invalidation | complete | reason counters and empty-cache no-op clearing |
| TP7 secondary evidence indexes | skipped | final profile does not justify another index |

Implementation followed TP0, TP2/TP3, TP5, TP4/TP1, TP6, then the conditional
TP7 audit. The ordering changed only after counters identified alpha interning
and repeated proof materialization as the dominant paths.

## 11. Completion Report Requirements

The final report must include:

- common-input and theorem-input median wall/user/system times;
- before/after phase timing and all TP0 counters;
- profiler summaries before and after;
- accepted Proposition, Claim, and Derivation counts before and after;
- exact functionality and code paths removed, with semantic justification;
- focused and full verification results;
- artifact version and deterministic-byte result;
- per-file added, deleted, and net source lines;
- total implementation/test/doc line changes reported separately;
- any performance gate not met and the measured remaining bottleneck.

## 12. Completion Report

### 12.1 Adjacent performance result

Environment: Debian GCC 14.2.0, Linux 6.12 amd64, default non-profiler build,
one warm-up and three measured runs. The full fixture hash is
`dfa54d5625d03b95a31f99bd143e295df6f2d98cbffdc2d8322879493d64a7c0`.
The pre-theorem fixture hash is
`4564af611d15b4b9f914fbe7fb9333e52ef91d9aba94a8609ff243b6febe0738`.

| Input/compiler | Median wall | Median user | Median system |
|---|---:|---:|---:|
| Full fixture at `b13f047` | 8.207 s | 8.162 s | 0.044 s |
| Full fixture after this plan | 0.462 s | 0.446 s | 0.016 s |
| Pre-theorem fixture after this plan | 0.323 s | 0.311 s | 0.016 s |

The adjacent full-fixture wall reduction is 94.4%. The theorem now adds about
139 ms rather than 2.35 seconds.

| Full-fixture metric | `b13f047` | Final | Change |
|---|---:|---:|---:|
| Term formations | 1,095,310 | 409,647 | -62.6% |
| Unique Terms | 19,258 | 9,068 | -52.9% |
| Alpha comparisons | 2,064,671 | 665 | -99.97% |
| Alpha comparison node visits | not attributed | 82,646 | measured |
| Recursive proof reification | 35,299 | 227 | -99.4% |
| Normalization misses | 11,584 | 7,788 | -32.8% |
| Normalization evictions | 2,880 | 0 | -100% |
| Normalization invalidations | 833 | 567 | -31.9% |
| Solver pops | 7,493 | 7,493 | unchanged |

The unchanged solver count confirms that this work optimized prior proof
construction and Term canonicalization rather than weakening type constraints.

### 12.2 Final phase attribution

Median compiler CPU timings on the final full fixture:

| Phase | Time |
|---|---:|
| Graph construction | 3.452 ms |
| Classifier/effect fixed point | 102.072 ms |
| Initial proof materialization | 30.066 ms |
| Result/totality evidence | 0.072 ms |
| Pending-only post-result closure | 19.465 ms |
| Accepted replay | 210.663 ms |

The full fixture visits 934 worklist entries across four rounds. The second
closure starts with 61 recorded residual consumers. Reification records 512
roots, 227 recursive calls, 491 successes, 248 residual observations, 344
current-pass reuses, and no failures or cycles. Totality publication adds two
Claims.

Constructor attribution records 2,556 specialization attempts, 1,223 curried
classifier cache hits, zero misses, and 5,242 dependent reindex requests.
Those data reject a second classifier cache; accepted replay is now the largest
named phase.

### 12.3 Evidence and artifact preservation

The full fixture publishes exactly the same dense artifact evidence before and
after implementation:

```text
propositions 675
claims       675
derivations  679
```

The pre-theorem fixture has 658 Propositions, 658 Claims, and 662 Derivations.
The additional theorem evidence is retained. Artifact version remains v82, and
two independent full-fixture publications are byte-identical.

### 12.4 Removed or consolidated paths

- Removed coarse Lambda/Match binder keys in favor of one recursive alpha
  fingerprint authority.
- Removed proposition-count-driven all-occurrence convergence scans and the
  second global materialization traversal.
- Consolidated direct constructor classifier derivation callers onto the
  TypeDeclarationDB revision cache.
- Reused exact IS_TYPE formation Claims before allocating fresh Universe
  variables.
- Avoided clearing normalization cache arrays when the cache is empty.
- Retained both totality proof rules because their premises express different
  object theorems; no proof evidence was deleted for speed.
- Rejected new constructor-specialization and Proposition indexes because the
  final counters/profile do not justify duplicate mutable state.

### 12.5 Profile and verification

The pre-implementation profile reported 52,935,073 recursive
`shape_terms_equal_at_depth` calls and 35.54% sampled time. The final profile
reports 18,417 calls with no sampled self time. The current sampled leaders are
`canonical_hash_term_at_depth` and accepted candidate traversal; canonical
hashing is now useful discriminating work rather than repeated failed deep
comparison. `propositions_equal` has 1,060,730 calls but no sampled self time,
which is why TP7 was skipped.

The final complete integration suite passes all 40 tests in 86.842 seconds. It covers
the Term collision audit, result/totality evidence, IF8 source/publication/
readback/determinism, dependent Match and indexed families, shared Core with
distinct annotations, malformed artifacts, examples 01-09, and incremental
REPL transactions.

Two latent boundary defects were exposed and fixed during full verification:

- cumulative profiling counters are observation only and cannot select a
  transaction's initial materialization phase;
- a relocated constructor cache is validated by nominal alpha shape, not local
  Term-ID equality.

All required and preferred performance gates are met.

### 12.6 Line accounting

The final diff from `b13f047` is:

| Area | Added | Deleted | Net |
|---|---:|---:|---:|
| Implementation | 860 | 402 | +458 |
| Tests and benchmark | 146 | 8 | +138 |
| This plan/documentation | 234 | 47 | +187 |
| Total | 1,240 | 457 | +783 |

The largest implementation changes are proof materialization/worklist handling
(`+373/-285`) and result-evidence publication (`+122/-19`). The added lines are
primarily permanent attribution counters, explicit state handling, and stronger
tests. No generated file or artifact schema was changed.

## 13. Decision Log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-21 | Compare adjacent compilers on a common fixture | historical compilers need not accept one evolving language input |
| 2026-08-21 | Treat totality theorem formation as a distinct measured cost | explicit totality propagation alone did not regress the common input |
| 2026-08-21 | Optimize proof materialization and interning before the classifier solver | solver pops rose only 1.5%, while proof Terms and alpha comparisons rose about 20% |
| 2026-08-21 | Audit unused totality proof paths for deletion | existing artifact tags do not justify retaining semantically undeclared machinery |
| 2026-08-21 | Preserve object totality evidence | performance cannot be obtained by erasing the theorem being checked |
| 2026-08-21 | Do not reopen Context/Substitution rebuilding work in this phase | both rebuild counts are currently zero and Context visits are not the hot path |
| 2026-08-21 | Keep `TERMINATES_FROM_RETURNS` | explicit Returns evidence proves termination independently of inferred TOTAL classifiers |
| 2026-08-21 | Do not cache open child fingerprints context-free | their alpha hash depends on enclosing binder and IH-frame environments |
| 2026-08-21 | Reuse the existing constructor cache | measured requests have zero misses; another cache would duplicate projection authority |
| 2026-08-21 | Skip a secondary Proposition index | the final profile reports no sampled `propositions_equal` self time |
| 2026-08-21 | Keep performance counters observational | cumulative counters must never control transaction-local compiler behavior |
| 2026-08-21 | Validate relocated caches by nominal alpha shape | local Term IDs are storage identities and may change under dense relocation |
