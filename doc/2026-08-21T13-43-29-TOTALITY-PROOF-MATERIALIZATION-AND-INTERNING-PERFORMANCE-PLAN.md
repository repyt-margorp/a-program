# Totality Proof Materialization and Interning Performance Plan

Date: 2026-08-21

Status: Audit complete; implementation not started

Baseline commit: `1471a53`

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

Every immutable Term receives a cached, strong alpha-canonical fingerprint.
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

The existing recursive canonical-hash implementation should be reused or
factored into one authority. Do not add another independent definition of
alpha identity.

### 5.2 Per-materialization evidence state

Maintain runtime-only state per typed occurrence:

```text
UNSEEN
IN_PROGRESS
SUCCEEDED(candidate proposition/authority reference)
RESIDUAL(current evidence generation)
```

`SUCCEEDED` is reusable throughout the materialization transaction.
`IN_PROGRESS` detects recursive cycles. A residual result is retried only when
one of its missing premise generations changes; it is not cached permanently.

The key is occurrence identity plus the frozen classifier, Context, and evidence
generation. Core Term ID alone is insufficient because typed occurrences may
share Core while carrying distinct classifiers or Contexts.

### 5.3 Evidence dependency worklist

Materialization must enqueue occurrences whose premises become available. Late
result evidence returns the accepted Claim IDs or evidence keys it introduced.
Reverse dependency indexes enqueue only consumers of those keys.

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

### 5.4 Immutable specialization cache

Constructor specialization receives a runtime-only cache keyed by the exact
semantic inputs:

```text
constructor declaration
parameter argument spine
field argument prefix and their classifiers
source Context
parameter/field Substitution
TypeDeclaration semantic revision
```

The cache may retain expected domain Term IDs and the resulting classifier.
Kernel candidate replay still validates the explicit premises. The cache is not
proof authority and is never serialized.

## 6. Implementation Phases

### TP0. Permanent attribution counters

Status: not started

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

Status: not started

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

Status: not started

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

Status: not started

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

Status: not started

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

Status: not started

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

Status: not started

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

Status: not started

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
| TP0 attribution counters | not started | phase and reification counters |
| TP1 totality surface audit | not started | retained-rule rationale or deletion |
| TP2 proof reification memo | not started | once-per-occurrence proof construction |
| TP3 evidence worklist | not started | no second global pass |
| TP4 constructor specialization cache | not started | reduced schema reconstruction |
| TP5 alpha fingerprint index | not started | reduced Lambda deep comparisons |
| TP6 normalization invalidation | not started | measured post-structural audit |
| TP7 secondary evidence indexes | conditional | new profile justifies work |

Implementation order is TP0, TP1, TP2, TP3, TP4, TP5, TP6, then conditional
TP7. TP2 and TP5 may be benchmarked on separate branches, but must be integrated
only after each preserves the same accepted proof graph.

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

## 12. Decision Log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-21 | Compare adjacent compilers on a common fixture | historical compilers need not accept one evolving language input |
| 2026-08-21 | Treat totality theorem formation as a distinct measured cost | explicit totality propagation alone did not regress the common input |
| 2026-08-21 | Optimize proof materialization and interning before the classifier solver | solver pops rose only 1.5%, while proof Terms and alpha comparisons rose about 20% |
| 2026-08-21 | Audit unused totality proof paths for deletion | existing artifact tags do not justify retaining semantically undeclared machinery |
| 2026-08-21 | Preserve object totality evidence | performance cannot be obtained by erasing the theorem being checked |
| 2026-08-21 | Do not reopen Context/Substitution rebuilding work in this phase | both rebuild counts are currently zero and Context visits are not the hot path |
