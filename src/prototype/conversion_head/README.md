# Conversion Head Prototype

## Problem List

1. F3's same-domain wrong-function check remains pending while recursive code
   is unnecessarily normalized under a blocked Fold.

## Subjective (User)

Continue F3/F4 verification. Preserve ordinary sorting functions, Core pointer
interning and post-synthesis-only `::`. Do not substitute a simpler negative
example for the same-domain case. Experimental compiler changes remain here
until explicitly approved for promotion.

## Objective (Code)

Baseline `ac7d906` plus the closed-readback prototype. In the failing source,
identity is supplied as `sort` together with QuickSort's Local/permutation
proofs. At 12 million Solve transitions only one comparison remains ready;
its request count is stable, but strong NF has taken 6,184,671 transitions and
its stack has reached 158,580 entries. At 80 million, the source is still
pending. This is not repeated classifier synthesis or an accepted bad proof.

The endpoint's WHNF is `Fold(Match(xs, ...), lambda p. Match(p, ...))`.
`conversion.c:rigid_head` does not recognize this as blocked, so strong NF
descends into the recursively encoded branch code. The raw Core regression
also stays pending with the previous converter; the candidate finishes it.

## Assessment

The patch adds a conservative head-stability test, not a new equation:

- `Fold M` is under-applied: it cannot reduce without its continuation.
- `Fold M (lambda x. N)` stays a Fold when both `M` and `N` are syntactically
  neutral under the existing predicate. `M` cannot become Return or Request;
  `N` cannot become Return(x), so the right-unit rule cannot remove the Fold.

The existing comparison then compares children with its existing binder scope
and pure policy. Partial applications matter because the comparison traverses
the application spine. Recognizing only the complete Fold did not fix the case.
Unknown shapes still use the existing NF fallback. No retained field, new
semantic tag, source syntax, alpha interning or sort-specific rule is added.
Unconditionally treating Fold as rigid is not sound: beta inside a continuation
can expose its right unit. Both such eta examples are positive regressions.

## Plan

- [x] Preserve a diagnostic using the actual owner layouts, not copied structs.
- [x] Core tests: blocked recursive input, partial Fold, both comparison
  directions, congruent/different branch bodies, beta/iota, delayed eta, wrong
  binder and pending divergence.
- [x] Same-domain source rejection and saved/rejected input checking, ordinary
  and retained 100-step resume and byte-identical zero-step rewrite.
- [x] Focused Core and source checks with ASan/UBSan and leak detection.
- [x] Quick/Insertion retained List/Vec/Fin reports and concrete value transport.
- [x] Bubble's full prototype gate on this candidate (O2, 212 s), including its
  internal nontransitive counterexample and ordinary/retained resume.
- [x] Legacy Merge's full `views` gate (O2, 93 s), including both Vec/Fin reports.
- [x] Tree's full prototype gate (O2, 195 s), including ordinary/retained resume.
- [x] Rerun Quick/Insertion `all`: reaches the retained CLI load, then fails at
  its default reader limit. All earlier source, observation and rejection
  checks pass; the complete `all` gate is **not** passing.
- [x] Full `check-acceptance` on this candidate, including 63/63 source
  compatibility, all four LT/partition variants, ordinary-result theorems,
  Local/Strong, Fin/Vec, images, optional witnesses and both prototype suites.
- [ ] Resolve the separate CLI image-limit policy and obtain promotion approval.

Source rejection now takes **5,816,830** transitions; resumed rejection takes
**5,817,131**. The source/image gate passes in **18 s** (O2) and **53 s**
(ASan/UBSan). The retained Quick/Insertion gate passes in **43 s** with the
explicit 3-million-record API bound. These are local measurements, not sorting
complexity bounds. The default CLI reader limit is unchanged.
ASan/UBSan coverage here is the focused Core/source suite, not a claim of a
sanitized rerun of the entire compiler acceptance gate.

```sh
bash src/prototype/conversion_head/overlay.sh /tmp/a-program-conversion-head
make -f src/prototype/conversion_head/build.mk BUILD=/tmp/a-program-conversion-head/build check-conversion-head
bash src/prototype/conversion_head/source-check.sh /tmp/a-program-conversion-head/build/pointer-check
make -f src/prototype/conversion_head/build.mk BUILD=/tmp/a-program-conversion-head/build check-acceptance
```

`overlay.sh` composes the existing readback candidate and this patch without
editing accepted code. `probe.c` is a cost diagnostic, not an acceptance test;
its `conversion_probe` target observes up to 12 million transitions from an
assembled source file. The [sorting plan](../../../doc/2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md)
tracks the remaining scope.
