# Total Pure Result Projection

Date: 2026-09-14
Baseline: `28d0823`, branch `rewrite/pointer-core-hott`
Parent: [pointer rewrite plan](2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md)
Preceding evidence: [withdrawn extraction trial and finite counterexample](2026-09-14-PURE-RESULT-CONVERSION-EXPERIMENT.md)

## Decision

Give the existing `PG_TOTAL_PURE_VALUE` rule an explicit Core semantic reference,
`kernel/total-result/v1`, written `q` here. This is not a new Core node kind,
value-side Lambda implementation, or second normalization engine. Its application
is interned by the ordinary pointer tuple and evaluated by the existing machine.
It is not exposed as a general-purpose surface function.

Previously the rule encoded `q(M)` as `Fold(M, lambda x.x)`. That represented
its unit computation but hid its stronger typed domain from the computational
syntax. Consequently these inferred and written indices differed:

```text
succ(q(choose b))
q(Fold(choose b, lambda x.return(succ x)))
```

The prior attempt to recover selected typed bodies could not give a uniform
answer for all producers. The new reference represents the result projection
itself, rather than assigning a second meaning to an ordinary Fold.

## Typed Contract and Laws

The admission rule remains:

```text
Gamma |- M : Comp(TOTAL, {}, A)
---------------------------------
Gamma |- q(M) : A                 [PG_TOTAL_PURE_VALUE]
```

The proof retains M's owned, context-scoped computation evidence. Empty effects
alone are insufficient; unknown totality, nonempty rows, raw Pi computations,
and values cannot supply this premise. A Core reference alone confers no typing
authority. No classifier lookup by erased Term pointer is introduced.

The result projection computes by:

```text
q(return v)       --> v
q(Fold(M, K))     --> q(App(K, q(M)))
```

The second law's typed premises are:

```text
M : Comp(TOTAL, {}, A)
K : Pi(x:A, Comp(TOTAL, {}, B))     x not free in B
```

These are the premises of an ordinary Fold whose result satisfies q's domain.
Handlers with operation clauses are not matched by this rule. General dependent
sequencing is not added. The evaluator first exposes the input through the same
WHNF/Demand machinery, then recognizes Return or a zero-clause Fold. Unhandled
neutral heads remain symbolic; no result is guessed for an unknown computation.

Local justification, conditional on sound TOTAL/purity typing: under a closing
well-typed valuation, M returns some a without an observable operation and K a
returns b. Both sides denote b. The result can contain suspended computations;
neither totality of those computations nor evaluation inside the returned value
is inferred. This justifies the law on the admitted fragment, not on arbitrary
untyped inputs or computations with effects/divergence.

In particular, this does **not** equate ordinary `Fold(M,K)` with `K(q(M))`.
Ordinary Fold and Identity transport retain their existing strict behavior.
Arbitrary raw applications of the q reference are outside its typed contract;
the new equations must not be used as an optimizer for arbitrary raw Fold.

Substitution into q is ordinary substitution into APP: q is a closed reference
and its argument retains all free binders. A well-typed substitution preserves
the admission premises. No synthetic identity binder needs to be allocated or
serialized now. This is why that obsolete argument is removed from the evidence
API and derivation parameters, not replaced by an unused compatibility field.

`::` still checks an already synthesized classifier. Pure conversion now sees
the explicit q computation; user Identity witnesses are not inserted into DefEq.
The comparison/certificate algorithm is unchanged. The fixed pure policy becomes
`evaluation/pure/v4`, so old retained normalization receipts cannot claim these
new computations. Source images continue to use ordinary Solve.

## Verification Gates

- [x] Finite open Bool index post-check compiles, without case-specific elaboration.
- [x] Arithmetic control and computed constructor index both compile independently.
- [x] Unchanged legacy Vec append compiles, including its original post-check.
- [x] Previously regressed `indexed-recursive-result.p` still compiles.
- [x] Initial full debug acceptance passes with the new reference and unchanged
  Fold/Identity transport implementations.
- [x] Expanded typed unit/composition, unknown-grade/effect rejection and ordinary
  Fold strictness tests pass after final registration.
- [x] Open-index concrete results and unchanged append results pass through source
  and unfinished/completed images, including order and multiple recursive steps.
- [x] Final compiler sources pass full debug and ASan/UBSan acceptance, including
  32/32 compatibility cases and all six QuickSort outputs. The last added
  `nonrecursive-open-index-wrong.p` rejection was also run separately in both
  builds after its registration. Pending q machine frames survive repeated
  save/read at single-step boundaries using the existing machine codec.
- [x] Update current README and parent milestones. Publish after these checks.

Local logs: `/tmp/a-program-total-result-final-debug.log` and
`/tmp/a-program-total-result-final-sanitize.log`. Vec append completes source
checking in 16,038 Solve steps on this revision; the earlier 1,000,000-step
bound did not complete. This is completion evidence, not a wall-clock benchmark.
The separate all-refuted `impossible_index_branch_check.p` still stops pending
at 515 steps; it is not fixed or counted as a passing compatibility case here.

General Higher Identity coherence is not established by these tests. The desired
dimensional law is that projection of a related total computation yields related
results. Existing substitution tests and higher/transport regression tests remain
mandatory, but are not a proof of that full law for all dimensions and type
formers. No unsupported higher case is silently admitted by this change.

## September 14 Follow-up: Case Naturality

After `89dce1e`, unchanged `dependent_pi_surface_check.p` synthesized `choose`
but rejected its post-check `(q:Bool) -> Result q`. The synthesized motive was
a type-valued Match; the written family projected a Match returning types.
The unit and Fold laws alone did not connect them.

Extend the fixed pure policy to `evaluation/pure/v5` with:

```text
q(Match s { C_i fields => M_i })
  --> Match s { C_i fields => q(M_i) }
```

The surrounding q still requires an admitted TOTAL, empty-row computation.
The existing Match premises type each `M_i` at the substituted result
classifier. Its total pure result has that classifier. Under a closing
valuation, both sides select the same constructor and fields and return the
same result. This is a conditional preservation argument for q, not equality
reflection or an arbitrary handler commutation rule. Ordinary Fold/Match and
the `::` post-check are unchanged.

The reducer recognizes a saturated ordinary erased matcher, binds fresh fields
using its retained arities, and places q around each branch application. It
does not execute unselected branches or merge Core terms. Captures remain
Lambda/Application edges; substitution and image relocation need no new
representation. A known constructor takes the existing iota path first.

Oversaturated matchers returning raw Pi are not directly matched by this rule.
General higher Identity coherence and normalization-cost bounds remain separate
obligations. The local law is not a proof of the full HOTT model.

- [x] Original dependent-Pi post-check compiles (1,204 transitions).
- [x] Separate `total-result-type-case.p` covers zero-, one- and two-field
  branches with Nat/Bool result families; a wrong constant annotation rejects.
- [x] Complete source/result/image regressions and full debug/sanitizer tests.
  Both full `check-acceptance` runs exited 0, including the 56-case legacy
  gate, negative annotations, and unfinished/completed images at split budgets.
  Optimized CLI checks also pass; unchanged QuickSort content certification
  remains at 148,372 Solve transitions.
- [x] Reject old `evaluation/pure/v4` retained work. Graph/continuation payload
  layouts have not changed; the policy identifies the changed reduction rules.

## Research Boundary

[Vakar, *A Framework for Dependent Types and Effects*](https://arxiv.org/abs/1512.08009)
distinguishes dependent CBPV variants and their interaction with effects;
[Torczon et al., *Effects and Coeffects in Call-By-Push-Value*](https://arxiv.org/abs/2311.11795)
studies effect/coeffect tracking and an alternative evaluation treatment of pure
computations. These motivate making the purity assumptions explicit. Neither is
being cited as a ready-made proof of A Program's q rules or Higher Identity.
The local contract and conditional semantic argument above are this design's
obligations, not an imported theorem about the complete implementation.
