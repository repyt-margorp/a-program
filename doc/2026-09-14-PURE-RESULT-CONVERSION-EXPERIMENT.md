# Pure Result Conversion Experiment

Date: 2026-09-14
Baseline: `aa21f98` on `rewrite/pointer-core-hott`
Status: rejected experiment; compiler changes reverted
Parent: [pointer rewrite plan](2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md)

## Question

Can result extraction alone reconcile constructor index substitution with
source computations, without changing raw Core conversion or runtime Fold?
The unchanged Vec append still requires this boundary to be resolved.

Write `q(M)` for the existing `PG_TOTAL_PURE_VALUE` encoding
`Fold(M, lambda x.x)`. The problematic shapes include:

```text
succ(q(add m n))
q(Fold(add m n, lambda x.return(succ x)))
```

An unconditional Core equation between these forms is not justified: the
second demands its prefix; the first can expose the outer constructor without
doing so. Existing effect/divergence counterexamples must remain tests.

## Tested Change

The archived [patch](../src/prototype/pointer/experiments/2026-09-14-pure-result-extraction.patch)
is against the baseline above. It is not applied and is not a build input.

Only the typed `RETURN_JOB` path was changed. After checking TOTAL and an empty
effect row, it attempted these operations:

1. Recover executable premises through checked context substitution, sharing
   the function-graph planner's existing view rather than copying it.
2. For Fold, obtain the prefix's typed result, apply its continuation through
   ordinary application evidence, and extract that application's result.
3. For application or known Match/induction, expose ordinary typed bodies.
4. Handle a callee which is a Fold when its prefix has actual Return evidence.
5. Keep the original `q` rule for an unexposed neutral computation.

There was no change to `sequence_step`, Core reduction, raw conversion,
Identity transport, or the artifact codec. This differs from the earlier
withdrawn trial which changed all pure source sequences to application.

## Results

All checks below use a 100,000-step bound, with separate baseline/trial builds.

| Input | Baseline | Final trial |
| --- | --- | --- |
| `computed-index-arithmetic.p` | done, 3,836 steps | pending, 100,000 steps |
| `computed-constructor-index.p`, importing the arithmetic control | pending | pending |
| legacy `explicit_index_family_append_check.p` | pending | pending |
| `indexed-recursive-result.p` | done, 3,584 steps | done, 3,584 steps |
| `indexed-computed-type.p` | existing passing gate | done, 3,274 steps |

The first row is decisive: the provider itself regresses. Consequently the
second row's final-trial failure is not evidence that the client reached its
old failure point. Checking imports only through the client would conceal this
distinction. No full acceptance claim is made for the trial.

Debugger inspection of the trial's first weak-comparison failure found an
application versus a semantic reference, followed by the existing strong-NF
fallback. Remaining `q` inputs included application of a reindexed callee and
curried application through a projected callee. Exposing one returned callee
does not provide a uniform representation for all of these terms.

Local experiment artifacts were `/tmp/a-program-pure-result-trial/pointer-check`,
`/tmp/a-program-pure-compare.log` and `/tmp/a-program-pure-origin.gdb`. The patch,
baseline commit and input files above retain the portable reproduction inputs.

## Decision and Next Implementation Gate

Do not merge the extraction rewrite or its otherwise unused public helper
APIs. Compiler implementation files were restored exactly to the baseline;
the baseline positive control was rerun successfully after withdrawal.

The result rules out this partial result-only implementation, not every typed
pure-result calculus. The precise remaining mismatch still needs proof-level
tracing; do not assert that more fuel or a single additional beta case fixes it.

The next design must account for both occurrences and their classifiers,
including function-returning sequences and substitution. Two candidate routes
remain, neither admitted by this experiment:

- A uniform typed elaboration with a checked correspondence to the original
  computation, including raw Pi results and recursive neutral calls.
- Scoped built-in pure-result conversion justified by typed premises. Its
  evidence must not masquerade as today's untyped conversion certificate or
  promote arbitrary user Identity proofs into global DefEq.

A minimal candidate obligation, not an implemented rule, is:

```text
Gamma |- M : Comp(TOTAL, {}, A)
Gamma |- K : Pi(x:A, Comp(TOTAL, {}, B))   x not free in B
---------------------------------------------------------
Gamma |- q(Fold(M,K)) ~ q(App(K,q(M))) : B
```

Here `~` needs its own precise typed meaning and checked substitution/action
laws. It is not the current raw conversion certificate and is not an arbitrary
Identity witness. The corresponding unit obligation is `q(return v) ~ v`.
Extending this to a B depending on the returned value is a further problem,
not implicitly licensed by the nondependent rule above.

Before connecting either route to source synthesis:

- [ ] Specify the equality/correspondence judgment and its typed premises.
- [ ] Establish its behavior under context substitution and nominal TypeViews.
- [ ] Keep `::` a post-synthesis obligation, never synthesis guidance.
- [ ] Test function-returning Fold, ignored prefixes and recursive neutrals.
- [ ] Test Identity action/transport against the unchanged strict Core rules.
- [ ] Pass the arithmetic control independently, then the constructor client.
- [ ] Restore unchanged Vec append and concrete results through source/images.

This gate is not a new goal or a replacement for full compatibility and HOTT.
It prevents repeatedly installing a local rewrite with no coherent typing law.

## Finite Control After Withdrawal

On unchanged compiler implementation `aa21f98`,
[`nonrecursive-computed-index.p`](../src/prototype/pointer/tests/known-limitations/nonrecursive-computed-index.p)
replaces recursive addition with `choose : Bool -> Nat`, implemented by two
constant branches. The following post-check rejects after 2,539 Solve steps:

```text
prepend := \b:Bool => \xs:Vec Nat (choose b) =>
  ((Vec Nat).cons (choose b) Nat.zero xs
    :: Vec Nat (Nat.succ (choose b)));
```

Debugger inspection confirms that the failing conversion completes strong NF.
After removing common type wrappers and beta-redexes, the index shapes are:

```text
succ(Fold(choose b, lambda x.x))
Fold(choose b, lambda x.succ x)
```

Here `choose b` remains a neutral Match. There is no recursive unfolding or
infinite normalization in this reproduction. This confirms a missing source
conversion correspondence, not merely a fuel/performance problem. It does not
by itself establish kernel inconsistency, nor justify equating these terms
under the untyped Core policy.

The [passing control](../src/prototype/pointer/tests/acceptance/nonrecursive-computed-index.p)
keeps the same inferred open `prepend` without that post-check, then applies it
to both concrete Bool constructors. Both post-checks succeed, and the resulting
vectors equal independently constructed one- and two-element vectors. Source
result comparison passes with chunks 1 and 64 (4,172 and 4,412 Solve steps for
the respective comparisons). The acceptance gate also checks images saved
before, during, and after Solve and then resaved without Solve.
After registration, the full debug `check-acceptance` passed, including the
31/31 compatibility gate and six QuickSort outputs. The two new direct result
checks also passed under ASan/UBSan with chunks 1 and 64. Compiler C/header
implementation is unchanged by this control; the open rejection was rerun.

Do not register the open reproduction as an expected rejection: accepting that
program remains the compatibility objective. The next conversion design must
pass this finite case before the recursive arithmetic/Vec gate, while keeping
the existing unknown-totality, effect and Identity transport checks unchanged.

## Research Scope

[Koronkevich, Rakow, Ahmed and Bowman, *ANF preserves dependent types up to
extensional equality* (2022)](https://doi.org/10.1017/S0956796822000090),
sections 7.3-7.4 and 8, discusses dependence disrupted by sequencing translations,
the role of extensional equality, and the consequences for type checking. Its
related-work discussion connects thunkability with computations usable in
dependencies. This is useful evidence that the obligation is substantive, not
a ready-made justification for A Program's `q` equation. In particular, this
experiment does not adopt that paper's equality-reflection rule or establish
its translation theorem for A Program's indexed recursion and Higher Identity.

[Vakar, *A Framework for Dependent Types and Effects*](https://arxiv.org/abs/1512.08009)
was also located as background for dependent CBPV. Its existence does not
establish the needed total-pure extraction or higher coherence laws here.

Reviewed on September 14, 2026. Paper claims and the local experimental
observations above are deliberately separate.
