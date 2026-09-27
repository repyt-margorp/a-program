# Local and Strong Sortedness

Date: 2026-09-27
Baseline: `b9249ba54d51c2115522cc1b3eaaa12280482e15` (clean publication tree).
Priority: execute before resuming P4/P5 of the
[authority plan](2026-09-25-POST-SURFACE-ISSUE-AND-AUTHORITY-SOAP-PLAN.md).
Sources: [#39](https://github.com/repyt-margorp/a-program/issues/39),
[PR #40](https://github.com/repyt-margorp/a-program/pull/40), documentation head
`f4b0e532d33dafe23a563f61667599380d0dd3a6`.

## Problem List

| ID | Problem | Status |
| --- | --- | --- |
| S1 | Distinguish adjacent and all-pairs certificates | Complete |
| S2 | Prove Local QuickSort at the ordinary result, then derive Strong | Complete within the stated QuickSort scope |
| S3 | Verify compatibility, separation, images and cost | Verified; publication pending |

## S1. Predicate and Conversion Library

### Subjective (User)

English paraphrase, this conversation, 2026-09-27: add the newly submitted
Local/Strong Sorted Issue and PR to the implementation plan and work on them
before continuing the refactoring. Existing requirements remain: proofs are
ordinary typed terms, `::` is a post-check, and no global function-witness
syntax or redundant solver authority is introduced.
Follow-up, 2026-09-27 (English paraphrase): leave QuickSort, MergeSort and the
other algorithms unchanged; prove Local/Strong afterwards under the appropriate
relation hypotheses. This supersedes any reading of "migration" as an algorithm
change. Under transitivity both certificates are available, not exclusive modes;
a particular list can also be Strong without global transitivity.

### Objective (Code)

`tests/fixtures/sorted-proof-provider.p` defines `general_sorted` with an
`All (R head)` certificate for the entire tail, recursively. It is strong,
not local. `tests/acceptance/generic-quick-sorted-result.p` proves that predicate
for `quickSort A (&le) xs`, using transitivity and reflexivity. The published
baseline has a passing full acceptance run; the PR separately reports targeted
checks at `8cc9752`. Neither report verifies the new theorems below.

### Assessment

Adopt the distinction, not a claim that the existing theorem is unsound.
Agent implementation decision: add `general_locally_sorted` (nil, singleton,
adjacent cons), and alias `general_strongly_sorted := general_sorted` so existing
strong constructors and nominal identity remain authoritative. Do not create a
third nominal copy or silently weaken `general_sorted`. Retiring that name is
deferred, not authorized by this implementation.

Strong implies Local without assumptions. Local implies Strong with transitivity.
These are proof transformations, not inverse laws, DefEq or proof irrelevance.
The primary [Agda v2.4 Linked.Properties source](https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.Properties.html)
was re-read on 2026-09-27: `AllPairs=>Linked`, `Linked=>AllPairs` under
transitivity, and boundary-based append support this distinction. This is a
mathematical guide, not an assurance that A Program accepts a particular proof.
The referenced Rocq page was unavailable; no fresh Rocq verification is claimed.

### Plan

- [x] Inspect the PR, current nominal predicates and ordinary-result proof.
- [x] Add explicitly named predicates without changing the old strong contract.
- [x] Check open Strong-to-Local and transitive Local-to-Strong functions.
- [x] Check diagonal extraction from `general_decision`. Its false branch is
  `R y x`, not `Not (R x y)`; both diagonal branches give reflexivity.
- [x] Reject a wrong endpoint in a conversion; do not use expected types to
  infer missing motives.

## S2. Ordinary-Result QuickSort

### Subjective (User)

The latest priority is Local/Strong Sorted. Earlier user requirement: prove
properties of ordinary `quickSort ... xs`, not just of a separately generated
output, without reinstating global `*quickSort`.

### Objective (Code)

The existing proof's `qsr_partition_ordered` and `qsr_quick_all` supply lower-to-
pivot and pivot-to-upper bounds without transitivity. `qsr_join_sorted` expands
cross-partition pairs and inserts `refl pivot`. These latter steps belong to
the strong proof, not to adjacency. Three test runners consume the existing
proof: `quick_result.sh`, `sort_insertion.sh`, and `derived_lt.sh`.

### Assessment

Extract unchanged partition/All-preservation helpers into one fixture, shared
by the old direct strong theorem and the new local theorem. Update all three
consumers together. Prove local concatenation by induction on local lower
evidence, using only its adjacent edges and the existing pivot bounds.

The base theorem takes `A`, `R`, `le`, directional comparator evidence and `xs`.
It must not take transitivity, reflexivity, a partition specification or an
assumed correctness theorem. Derive strong correctness by the generic
Local-to-Strong function and transitivity; retain the old direct strong proof
as an independently checked compatibility control.

### Plan

- [x] Share existing helpers without copying their implementation or adding
  compiler import behavior.
- [x] Prove the local pivot join, sized/Acc induction, and ordinary-result
  `quick_locally_sorted` for arbitrary inputs.
- [x] Derive `quick_strongly_sorted` at the identical result index; no separate
  reflexivity parameter.
- [x] Preserve old direct-strong and derived-LT consumers.
- [x] Inspect insertion/tree/merge assumptions separately; record disposition
  without promising an unverified no-transitivity theorem for those algorithms.

2026-09-27 targeted results: `quick_locally_sorted` checks in 1,048,022 Solve
transitions; the derived strong theorem in 1,081,052. Both indices are literally
the imported ordinary `quickSort A (&le) xs`. Cyclic comparator correctness
quantifies over both inputs, and its QuickSort corollary quantifies over lists.
Execution checks reconstruct the output list from certificates for empty,
singleton, duplicate and three-cycle inputs. Wrong edge, strong bound,
conversion endpoint and output indices reject. Counts are this candidate's
checks, not a speed comparison against the old compiler.

Other-sort disposition: `sort-insertion-property.p` defines its own nominal
Nat/LE `Sorted`/`AllFrom`; insertion/tree/merge proofs consume that contract
and its order lemmas. `mergeSort` is Nat-specific and its `mergeBy` folds
insertion over the left list, rather than a textbook two-head merge. Keep
those proofs and algorithms unchanged in this epoch. The reusable conversion
applies after explicitly relating these nominal predicates; do not silently
identify them or claim arbitrary-relation Local correctness without its proof.
This narrower implementation scope is an agent decision, not a theorem that
generic Local MergeSort is impossible.

## S3. Permanent Verification and Publication

### Subjective (User)

Earlier user requirements: permanent boundary tests, Main push at verified
milestones, concise provenance-aware progress, and report code additions and
deletions separately from documentation.

### Objective (Code)

`tests/quick_result.sh` already covers the old theorem, incorrect output index,
pending resume and retained images. The PR's cyclic example checks only a
finite local certificate, not a cyclic comparator or generic QuickSort theorem.
Inherited uncommitted Context/IADT trials remain outside this change.

### Assessment

Use a reflexive directed three-cycle to separate contracts. Compile its
comparator certificate for all inputs and instantiate the open local theorem;
reject an attempt to supply the missing nonadjacent edge. A rejected candidate
is not by itself a general theorem of uninhabitance. Permutation remains a
separate existing property. The unfolded relation-position counts are
`max(n-1,0)` and `n(n-1)/2`, not DAG memory or running-time estimates.

### Plan

- [x] Add one permanent runner to `check-acceptance` for conversions, cyclic
  comparator, open local/strong theorems, and precise negative controls.
- [x] Verify saved, pending/resumed and retained images, including rejection
  after loading; check source parses and sanitizer runs of affected paths.
- [x] Run the existing full acceptance suite before publication.
- [x] Record comparable Solve steps, wall time/RSS, serialized size and available
  evidence counts for local, derived-strong and direct-strong proofs. Separate
  shared-provider overhead; make no unmeasured speedup claim.
- [x] Update README with actual theorem assumptions and test entry point.
- [ ] Record per-file diff totals, publish the verified epoch to Main, and
  report #39/#40 disposition. Documentation submission alone does not close #39.

Completion: checked general theorems at ordinary QuickSort output, explicit
contract separation, preserved old consumers, permanent image/negative tests,
and measured results. Resume P4/P5 only after this priority is handled.

## Verification Record

2026-09-27, baseline plus this candidate, clean publication worktree:

- Full `make check-acceptance`: passed, wall 1371.081 s, user 1271.771 s,
  system 98.805 s. This includes unchanged insertion/tree/merge/QuickSort,
  all four derived-LT/partition variants, and the new sortedness runner.
- Debug (`-O0 -g`) and ASan/UBSan (`-O1 -g`, leak checking and halt on errors):
  both `local_strong_sorted.sh` and `quick_result.sh` passed.
- New runner checks ordinary/retained images, inert byte-identical resaves,
  pending budgets 0/100, resumed wrong-result rejection, cyclic nonadjacent
  evidence rejection, and executed certificates at chunk sizes 1/64.
- `sorted-proof-provider.p` remains byte-identical, SHA256
  `a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`.
  No compiler C/header, reduction rule, algorithm, or image format changed.

### Measurements

Same release checker, same provider (frozen algorithms + shared QuickSort
helpers + Local/Strong library), fresh processes, one warm-up then five samples
per case in alternating order. Source checking only, without serialization in
the timed interval. Derived source combines Local and the strong corollary;
direct source is the retained independent strong theorem. This common-provider
setup differs from individual runner inputs, explaining their different steps.

| Proof | Solve transitions | Median wall s (range) | Median peak RSS KiB | Ordinary / retained image bytes |
| --- | ---: | --- | ---: | --- |
| Local | 1,048,022 | 0.628 (0.613-0.666) | 153,176 | 941,551 / 5,639,341 |
| Derived Strong | 1,083,340 | 0.659 (0.643-0.664) | 153,604 | 962,143 / 5,810,745 |
| Direct Strong | 1,162,033 | 0.775 (0.774-0.789) | 176,108 | 1,041,876 / 6,413,482 |

Read-only debugger inspection immediately before `pg_program_destroy` in the
debug build counted whole-session acceptance receipts / typed occurrences /
Core terms: Local 94,885 / 114,368 / 1,118,688; derived Strong
95,851 / 115,426 / 1,162,074; direct Strong 108,176 / 131,853 / 1,441,940.
These are not final normalized witness sizes or adjacent/all-pairs slot counts.
This sample shows cheaper checking of these proof programs, not a compiler
optimization or faster sorting execution. No general asymptotic bound is inferred.

### Diff Accounting

Relative to `b9249ba`, compiler implementation C/headers: +0/-0; build entry:
+5/-0; proof/library fixtures and test runners: +610/-199, net +411.
The moved common helpers and Boolean order are counted on both sides.
Documentation totals are reported separately in the publication receipt.

| File (under tests/) | Added | Deleted |
| --- | ---: | ---: |
| acceptance/generic-quick-local-sorted-result.p | 92 | 0 |
| acceptance/generic-quick-sorted-result.p | 11 | 175 |
| acceptance/generic-quick-strong-sorted-result.p | 17 | 0 |
| derived_lt.sh | 5 | 2 |
| fixtures/generic_sorted/boolean-consumer.p | 0 | 21 |
| fixtures/generic_sorted/boolean-order.p | 21 | 0 |
| fixtures/local-strong-sorted.p | 69 | 0 |
| fixtures/quick-sort-proof-common.p | 199 | 0 |
| fixtures/sortedness-cycle-wrong-edge.p | 3 | 0 |
| fixtures/sortedness-cycle-wrong-strong.p | 5 | 0 |
| fixtures/sortedness-cycle.p | 58 | 0 |
| fixtures/sortedness-strong-consumer.p | 21 | 0 |
| fixtures/sortedness-wrong-conversion.p | 12 | 0 |
| local_strong_sorted.sh | 92 | 0 |
| quick_result.sh | 3 | 0 |
| sort_insertion.sh | 2 | 1 |

PR disposition: incorporate #40's central mathematical distinction and tested
construction into this implementation. Preserve its original research at the
linked PR; this shorter active SOAP plan supersedes its implementation checklist.
Do not adopt silent weakening, proof irrelevance, automatic nominal identity,
mandatory legacy-name retirement, or an unmeasured quadratic-memory claim.
