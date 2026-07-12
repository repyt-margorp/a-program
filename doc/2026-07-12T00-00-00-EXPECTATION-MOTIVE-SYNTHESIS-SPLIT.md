# Expectation and Match Motive Synthesis Split

Date: 2026-07-12

Status: implemented in prototype

This note records the design for separating type expectations from
classifier synthesis, and for unifying match motive construction around generated
motive applications.

## Problem

The prototype had already removed `EXPECTS_TYPE` as a judgement relation, but
still had expected classifiers flowing through ordinary classifier candidate
paths. That made source annotations act like synthesized evidence.

That is the wrong boundary. An expected classifier is a pending check request.
It is not a synthesized classifier and it is not evidence that a graph node has
that classifier. If expected classifiers participate in constructor resolution,
APP typing, match branch classifier selection, or lambda inference, then
source-level annotations can change what the graph appears to synthesize.

The match motive path has the same issue in another form. The intended rule is:

```text
match_classifier = APP(generated_motive, scrutinee)
```

Even when every branch currently appears to have the same classifier, that
classifier should be observed by reducing the generated motive application, not
by installing a special branch-unification result as the match classifier.

## Target Invariants

Classifier synthesis may read only proven judgement facts:

- committed `JudgementDB` `HAS_TYPE` relations
- current `JudgementDelta` `HAS_TYPE` relations
- generated match-motive result records

Classifier synthesis must not read:

- pending ascription checks
- top-level expectation checks
- expected-classifier side-table entries

Ascription and top-level expectation checks are post-inference checks. They may
select an already synthesized classifier and record a conversion proof when the
selected classifier is normalization-compatible with the requested classifier.
They may not seed IH typing, constructor resolution, APP typing, or match motive
synthesis.

Match motive synthesis has one semantic shape:

```text
M := \x : scrutinee_classifier =>
       x @case1 => classifier_of_branch1
         ...
         @caseN => classifier_of_branchN

match_classifier := M scrutinee
```

The compiler may build a constant motive when branch classifiers are all
normalization-equal, but that is still a generated motive case list. It is not an
expected-classifier shortcut.

## Implemented Shape

1. Generic classifier collection and lookup APIs expose only judgement facts and
   generated match-motive results. There is no expected-classifier side table.
2. `compile_ref_with_proven_classifier(...)` may carry only an already proven or
   synthesized classifier. Ascriptions and top-level expectations do not fill
   this field while lowering.
3. Pending ascription checks remain explicit obligations. They are consumed
   only after inference has produced real classifiers.
4. Match scrutinee constructor resolution may use binder assumptions or already
   proven classifiers, but it must not use the type expression inside
   `term :: Type` as resolution evidence.
5. The old `build_match_motive_from_expected_classifier(...)` shortcut is
   replaced by a helper that builds a generated motive from a branch classifier
   vector.
6. The "known common branch classifier" path calls the same branch-derived
   motive builder with the selected branch classifier repeated for each case.

This keeps the graph construction path pure with respect to expectations while
still allowing source-level annotation information to exist as diagnostics and
later checks.
