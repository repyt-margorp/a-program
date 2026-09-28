# Closed Readback Prototype

## Problem List

1. F3/F4 observations spend most normalization steps materializing closures.
2. Context substitution also uses readback, but requires fresh lexical binders.

## Subjective (User)

Continue F3/F4 verification. Preserve pointer-structural interning, ordinary sort
results and the post-synthesis-only meaning of `::`. New experimental compiler
work stays under `src/prototype/` until explicitly promoted.

## Objective (Code)

Baseline `aaba32b`, 2026-09-28. `eval.c` traverses a closed Lambda/Application
DAG again whenever it has a nonempty closure environment. A 26-node example
creates 38 terms and takes 63 readback steps. This is not exponential in that
small example; shared requests already avoid revisiting identical input keys.

`graph.c` interns exact pointer structure. The prototype derives immutable free
binder sets on an intern miss; support sets themselves are hash-consed. No WHNF,
alpha conversion, type information or proof participates in this metadata.
Unknown hand-built terms conservatively have unknown support. Semantic Ref
payloads stay opaque, as in the existing evaluator's environment rule.

## Assessment

The cache computes syntactic free binders:
`FV(Ref b) = {b}`, `FV(App f a) = FV(f) union FV(a)`, and
`FV(Lambda b t) = FV(t) minus {b}`. Oracle references have empty support under
the existing Core rule that does not substitute inside their payload. When
support is empty, capture-avoiding substitution can only change bound names;
this justifies materialization reuse, not alpha-based Term interning.

The first trial skipped closed terms in *all* readback. Reject that trial:
`tests/core.c`'s normalization/reindex test fails when a closed Lambda's binder
already occurs in the destination Context. Its child scope is freshened but the
unchanged parent still refers to the original binder. The earlier attempt to
erase the request's input environment also broke exact substitution lookup;
do not conflate a reusable result with the identity of its request.

The revised trial permits reuse only for evaluator materialization. Context
substitution keeps its existing freshening behavior. Both use the same executor;
the caller supplies the contract on every step, including resumed steps. There
is no new saved state or wire-format flag, and the input key stays unchanged.
On the small test, substitution still uses 63 steps; materialization uses zero
readback steps and creates no terms. This does not avoid normalization itself
or memoize effect results: it reuses syntax, not a prior execution.

Reject the initial flat-set representation too. A spine of 6,000 distinct free
binders allocated about 230 MiB RSS versus 3 MiB in the baseline: every prefix
copied the growing set. The current representation is an interned compressed
pointer-key trie. Each branch splits at a lower bit than its parent; removing
one binder copies only its path and shares off-path nodes. It does not intern
Terms modulo alpha equality. `construction_bench.c` is a Linux-only diagnostic:

| Binders | Baseline RSS | Flat sets RSS | Trie RSS |
| --- | ---: | ---: | ---: |
| 3,000 | 2,332 KiB | 61,308 KiB | 3,696 KiB |
| 6,000 | 3,004 KiB | 235,520 KiB | 5,640 KiB |

Fresh measurements on this machine, not portable performance bounds. The final
trie candidate passes full `check-acceptance` (including 63/63 source compatibility)
and focused sanitizer gates. There remains construction/storage overhead;
no accepted code is changed.

## Plan

- [x] Prototype the derived support cache and materialization-only shortcut.
- [x] Keep exact/alpha interning separate; test capture, shadowing, shared input
  and unknown support. Add union/removal checks against explicit bit-mask sets.
- [x] Keep Core normalization/reindex and existing evaluator I/O tests passing.
- [x] Save/inertly rewrite/resume a mixed closed/open DAG at every step.
- [x] Complete Quick and legacy Merge Vec/Fin observations on this candidate.
- [x] Complete concrete value transport, assertion-free synthesis and rejection
  of wrong Quick/Insertion labels on this candidate.
- [x] Pass focused Core, evaluator I/O, Quick and Merge ASan/UBSan gates.
- [x] Pass the value-test sanitizer gate, including incorrect labels.
- [x] Complete the general acceptance gate.
- [x] Verify full and partial retained sorting images with explicit reader bounds.
- [x] Read baseline retained output with the candidate and candidate retained
  output with baseline; use the existing bounded typed comparator in both.
- [ ] Same-domain comparison is still pending at 80M with this patch alone;
  the [conversion-head companion](../conversion_head/README.md) addresses it.
- [x] Measure construction/storage overhead and reject quadratic prefix copying.
- [x] Review the final representation and complete its regression gates.
- [ ] Obtain explicit approval before promoting changes into accepted code.

Build a generated overlay, leaving accepted source/build files untouched:

```sh
bash src/prototype/readback_support/overlay.sh /tmp/a-program-readback-support
make -f src/prototype/readback_support/build.mk BUILD=/tmp/a-program-readback-support/build check-acceptance
make -f src/prototype/readback_support/build.mk BUILD=/tmp/a-program-readback-support/build check-support
```

Do not treat a prototype result as a Main compiler result. Detailed F3/F4 status
and known image-limit failures remain in the [active plan](../../../doc/2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md).
