# Merge Composition and Identity Normalization

Date: 2026-09-14
Branch: `rewrite/pointer-core-hott`
Starting revision: `eb007ed`
Issue: <https://github.com/repyt-margorp/a-program/issues/28>
Status: ordinary merge covered; curried and captured merge graph/witness restored.

## What Reproduces

The posted Issue #28 source rejects both with and without `repeatWithFuel`
on the starting revision and this worktree. The reported declaration-order
contamination is therefore not reproduced here. This does not establish what
happened in the reporter's older `843c57c` binary or other source variants.
The reporter subsequently confirmed in Issue #28 that the corrected library
program runs with both `843c57c` and `3c5ab2a`. That is external confirmation,
not a local run of the full library; its source location is still needed here.

The first failing source application is `*tail right` in `structuralMerge`.
Its `right` lambda surrounds the Match, rather than appearing inside each
branch. In this example the recursive motive returns `List A`, with `right`
captured in the environment. Thus `*tail` already produces the list; applying
it to `right` tries to call a list as a function. No Function Graph declaration
is requested by the posted source, which contains neither `@function` nor
`*function` at a named function definition.

Two valid forms are tested:

```text
// Captured argument; recursive result is a list.
\left : List A => \right : List A =>
  left @nil => right
       @cons head tail => insertBy A le head *tail

// Branch-local argument; recursive result is a function.
\left : List A =>
  left @nil => (\right : List A => right)
       @cons head tail => (\right : List A => insertBy A le head (*tail right))
```

`src/prototype/pointer/tests/merge_composition.sh` is part of `check`:

- [x] Check both valid forms, alone and alongside the independent fuel recursion.
- [x] Move the fuel definition before the merge definitions and recheck results.
- [x] Check nonempty merge, empty-left merge and fuel composition results.
- [x] Reject the original overapplication, with and without the fuel definition.
- [x] Check pending and completed `.a` inputs through ordinary Solve, with
  evaluation chunks of 1 and 64. No special merge or replay rule is added.

This is insertion-based merging, not a claim of linear-time merge or a complete
MergeSort implementation. The full user MergeSort source has been requested.

## Separate Function Graph Failure and Fix

After correcting the recursive call, ordinary merge execution succeeds.
At revision `3c5ab2a`, requesting `@curriedMerge` yields `unsupported`. The checked reproduction
is `merge-function-graph-request.p`, importing `merge-function-graph-composition.p`
from `src/prototype/pointer/tests/acceptance/`.

Tracing reaches `pg_function_graph_advance`: both branch plans are constructed,
but `pg_prove_inductive_type` does not admit the generated schema. This is a
different failure from the posted application's wrong classifier. Neither a
global metadata collision nor a termination failure has been established.

The failed premise is the universe bound, not positivity. On this fixture,
`graph_universe` chooses level 0 from the selected input/output telescope,
whereas `pg_data_schema_field_level` finds a field formation at level 1.
That field's retained evidence passes through `PG_PI_DOMAIN` and `PG_REINDEX`.
Pi component inversion deliberately retains the whole Pi's universe bound;
it does not recover a minimal component bound. The kernel therefore correctly
refuses to place this evidence in the generated level-0 declaration.

- [x] Identify the failed formation premise: field bound 1 exceeds declaration 0.
- [x] Recover the retained Pi domain formation through the existing component
  traversal and typed context maps. Require the resulting context, judgement
  and domain to agree before using that proof; otherwise keep the conservative
  inversion rule. No equality reflection or universe lowering axiom is added.
- [x] Reuse the same component traversal for inductive recovery, with its
  temporary arena and context frames passed explicitly. No Merge-specific
  schema rule, new proof tag, or parallel type authority is introduced.
- [x] Check a small domain of a large Pi, projection/reindex/Thunk wrappers,
  repeated requests, and a genuinely large domain that must stay large.
- [x] Restore graph formation and witness generation, and compare the witness's
  output with the expected merge through a saved imported source image.
- [ ] Prove an actual post-hoc property separately from obtaining that witness.
- [ ] Inspect the full MergeSort source when its location is available.

### Captured Eliminator Follow-up

At `3b9bdae`, `import structuralMerge; graph := @structuralMerge;` was still
unsupported although ordinary execution passed. Graph input selection chose
the last outer Lambda, whereas this function matches an earlier argument.

The graph generator now retains the complete source argument environment and
abstracts the eliminator's scrutinee in that environment. It reuses checked
elimination reindexing and ordinary Match/induction, Pi, Lambda and application
rules. The local graph varies only its fresh input; captured arguments stay
fixed. Publication applies that graph and witness to the original scrutinee,
then abstracts the original argument telescope. Public application order and
the List-valued IH are unchanged. No Core node, evaluator rule or proof axiom
is added, and source arguments are not permuted across dependent binders.

Helper result-type substitution must also insert the specialized local input;
otherwise the helper's private result context has one more binder than its
public argument list. This is an ordinary typed context map, not a second
source or solver authority.

- [x] Generate `@structuralMerge` and `*structuralMerge` without rewriting it.
- [x] Check nonempty and empty outputs through saved source images, chunks 1/64.
- [x] Repeat with standalone and reordered provider declarations.
- [x] Check a captured non-recursive Match and branches returning raw functions.
- [x] Check `\P : Nat -> @ => \n : Nat => \v : P n => ...`, where
  moving `n` past the captured `v` would invalidate its domain. The witness
  returns the expected value for a nonconstant `Family`.
- [x] Check a recursive caller of a captured list-copy helper, including its
  specialized result context. This isolates helper mapping from comparison.
- [x] Full debug `check-acceptance`, including compatibility 28/28 and QuickSort.
- [x] Full sanitizer `check-acceptance` for this follow-up.
  The final dependent-capture fixture was added afterward and the complete
  merge-composition script was rerun separately on debug and sanitizer builds.

Limits recorded before the indexed-capture follow-up below:

- Captured indexed inputs still require generalizing the index telescope along
  with the new local input. The existing generic-index check remains in force;
  the single-input construction does not solve fixed indices backwards.
- `import lessEqual; graph := @lessEqual; witness := *lessEqual;` is unsupported
  on both the prior test binary and this implementation. The backend forms
  its three-leaf graph, but `function_graph_exports` rejects repeated source
  constructor names (`zero` from different split paths). An earlier debugger
  stop at unresolved `*k` was a speculative branch, not this final cause.
- These cases and full MergeSort remain follow-up work.

### Partial-call Follow-up

At `e9a131d`, a recursive caller `repeatMerge le fuel` remained unsupported:
its zero branch returns `\xs : List Nat => xs`, and its successor branch is
`\xs : List Nat => { previous := *k xs; structuralMerge Nat le left previous; }`.
Graph planning failed before schema construction, after recording the first IH
call. Partial application/sequencing exposed a callable with arguments on the
continuation stack. Eager beta-body extraction then lost the helper boundary,
leaving a neutral nested induction which cannot normalize to RETURN.

Before this beta exposure, the planner now reconstructs the pending typed
application spine and asks the existing helper-graph owner to handle it. It
consumes the argument frames only when that request is accepted. Otherwise the
ordinary symbolic evaluation path is unchanged. No new evaluation protocol,
proof rule, effect dispatch or equality reflection is introduced.

- [x] Reproduce the failure independently of graph export-name collisions.
- [x] Generate the recursive merge caller's graph and witness.
- [x] Add zero-, one- and two-iteration output comparisons to the permanent
  captured-request fixture and image tests, including provider reordering.
- [x] Full debug and ASan/UBSan `check-acceptance` for this follow-up,
  including compatibility 28/28, QuickSort and the new repeated merge checks.

The root README now describes the pointer rewrite rather than the legacy
TermDB/alpha-interning/replay architecture. Its predecessor is preserved in
`2026-09-14-LEGACY-TOP-LEVEL-README.md`. The documented build, NF, REPL and
pending-image commands were checked with the current pointer binary.

Keep Issue #28 open while reporting this distinction, rather than describing
all graph-composition concerns as fixed.

### Indexed Capture Follow-up at `7fb27a3`

The next probe separates source synthesis from graph extraction. For
`count := \A : @ => \n : Nat => \xs : Vec A n => \start : Nat => xs ...`,
the existing indexed Match elaborator generalizes the ambient telescope after
the index. Its IH therefore accepts `start`: the cons branch
`Nat.succ (*tail start)` checks and computes; `Nat.succ *tail` rejects.
This differs from the non-indexed captured merge above. It is existing surface
behavior, not a proposed new implicit-application rule. Whether independent
captures should be generalized needs a separate compatibility decision.

At `9185428`, even the non-recursive `select`, returning `start` in both Vec
branches, could not publish `@select`. GDB showed `function_graph_order` called with
`ready = 0`, `cases = 0`, body rule `PG_APP_ELIM`, and source Match
`generalized_count = 1`. It rejects before `prepare_graph` runs. The application
specializes the generalized Match; it is not an invalid source argument.
Thus the earlier generic-index restriction is a later boundary, not the first
observed cause. Do not claim that modifying `capture_eliminator` alone fixes it.

- [x] Retain ordinary count/select comparisons in
  `tests/acceptance/indexed-captured-induction.p`, including empty/nonempty
  inputs and unfinished/completed `.a` loads at Solve chunks 1/64.
- [x] Preserve the typed specialization spine while exposing the eliminator;
  do not discard its arguments or normalize a neutral Match into RETURN.
- [x] Derive graph/source clause layouts after the selected eliminator and its
  internal generalized arguments are known. Internal Pi arguments are not
  necessarily explicit source branch Lambdas.
- [x] Generalize the captured index telescope together with its input using
  existing checked context maps; specialize the public graph and witness with
  the same map. Keep dependent captures in their original order.
- [x] Verify count/select graphs and witnesses, dependent outputs, helper calls,
  wrong-index rejection, source/image agreement and unchanged QuickSort.

The implementation keeps a typed application stack during preparation, then
constructs the local eliminator over generic indices and input in the unchanged
captured environment. One checked specialization maps that telescope and its
already supplied Pi arguments to the source call. Publication of the graph,
witness and helper result classifier uses that same map. Remaining branch-local
Pi arguments stay callable. Source clause layout is registered only after
preparation, distinguishing generalized arguments from explicit branch Lambdas.

The recursive `repeatCount` caller exposed an additional Universe bug. Its
generated constructor contains a helper graph of level 1, while the caller's
Nat input/output had prematurely fixed Self to level 0. Positivity passed;
the existing field-level check correctly rejected the declaration. The planner
now finishes the call/split plan before building Self and constructor schemas,
joining the interface, helper-graph and split-type bounds. The result packet
uses the same final level. Each plan is built once in parent-before-child order;
no failed schema is repeatedly rebuilt with guessed levels, and no bound check
or conversion rule is weakened.

New permanent fixtures cover count/select graphs, empty and nonempty Vecs,
dependent Vec copy results, recursive helper composition, dependent indices
`(A : Type, x : A)`, and a captured Match returning a raw Pi. Wrong input indices
still reject from source and unfinished images. Comparison runs use chunks
1/64 and unfinished/completed images. The pre-existing kernel test rejecting a
positive schema whose field exceeds its universe remains unchanged.

- [x] Full debug `check-acceptance`, including compatibility 28/28 and QuickSort.
- [x] Expanded focused debug checks after adding the two-index/raw-Pi cases.
- [x] Full ASan/UBSan acceptance and the final expanded focused checks.

No Core tag, kernel rule, evaluator policy, artifact format or separate Replay
engine changes. The surface IH generalization policy is unchanged. These
examples do not establish arbitrary indexed graph generation, resolve graph
export-name collisions, or prove general QuickSort/MergeSort properties.
Relative to `9185428`, implementation C/headers add 122 and remove 37 lines
(net +85); regression fixtures/script add 89 lines. Documentation is separate.

The first implementation searched every Pi origin and increased a 20-run debug
QuickSort compile sample from 0.611 s to 1.030 s. Level 0 is already the minimum
bound, so its inversion needs no origin search. Limit bound recovery accordingly;
this is an optimization of proof selection, not a type-specific admission rule.

The minimum-bound shortcut brought the same 20-run sample to 0.738 s (about
37 ms per compilation versus 31 ms baseline). This is a small debug timing
sample, not a throughput guarantee; residual proof traversal cost remains.

## Independent Identity Fix

An attempted Vec compatibility change exposed a genuine WHNF problem in
`identity.c:action_body_resume`. When an open action body did not reduce, the
evaluator stopped without checking whether the supplied boundary path reduced
to reflexivity. Re-evaluating the materialized result could then make progress.
QuickSort ascending input exposed this through a stuck transport and Match.

The fix invokes the existing `diagonal_fallback` in that unchanged-body case.
It adds no equality reflection, type-specific axiom, or new term constructor.
The existing fallback checks the supplied path and both endpoints; an arbitrary
loop proof is not declared reflexive. Scope pruning still precedes this check.

The direct regression uses `Act(\x. Act(q x))` applied to a computed reflexive
boundary. One WHNF pass must produce the iterated reflexivity result. It also
checks neutral non-reflexive input and suspended evaluator boundaries. Forcing
the old unchanged-body early return under GDB makes this test fail.

The portable pure policy changes from `evaluation/pure/v2` to `v3`, and the
action-body work operation from `identity/action_body/v1` to `v2`. Old normality
receipts must not silently inherit the new reduction behavior. The action-body
continuation retains its own unchanged `v1` name. Raw source inputs still use
the same Solve path; no new replay engine is introduced.

## Withdrawn Vec Trial and Verification

Lowering every total, empty-effect sequence as application of an extracted value
made the legacy Vec append check terminate, but regressed
`indexed-recursive-result.p`. Restricting the trial to syntactically dependent
codomains did not fix that regression and lost the Vec improvement. Neither
trial is retained. No failing test was removed to claim compatibility.

- [x] Final debug `check-acceptance`, including Pi recovery: passed.
- [x] Existing source compatibility gate: 28/28; all six QuickSort outputs passed.
- [x] Final ASan/UBSan `check-acceptance` with the minimum-bound shortcut:
  passed, including the new graph/witness image and universe-bound tests.
- [ ] Legacy Vec append remains open, as do general QuickSort post-hoc properties.

The overall rewrite goal remains active. These local tests do not establish
complete Higher Observational TT, general termination soundness or MergeSort
correctness for all inputs. No Main promotion is justified by this audit alone.

## Compatibility Follow-up

At `b0ee0a3`, the unchanged legacy Vec append still exhausts 100,000 Solve
steps. Removing the complete multiline `append :: ...;` statement does not
change that outcome. This is an internal result-classifier comparison, not
only the final post-check. The weak comparison's first mismatching normalized
pair is a semantic-object reference versus an application containing neutral
Fold and recursive Lambda encoding. Strong normalization then starts again
from the comparison endpoints. No new equality rule is justified by this
debugger observation; in particular it does not justify discarding a Fold's
demanded computation. The total-pure result/conversion design remains open.

Three previously verified sources are now permanent compatibility cases:

- `indexed_branch_rebuild_check.p`: rebuild a refined indexed constructor.
- `residual_index_equation_negative.p`: despite the historical filename, its
  constant Nat motive asserts no false index equality and should compile.
- `examples/type-infer-and-check/level2/02_tree.p`: two recursive fields and
  nested arithmetic; the size of the existing tree must evaluate to three.

An importing client additionally checks empty and two-constructor rebuilds,
including LE evidence, through unfinished/completed `.a` files and Solve chunks
1/64. The original sources are unchanged. The compatibility inventory is now
31 cases; the pending Vec append is not counted as a passing case.

- [x] Expanded debug compatibility suite: 31/31, image/results checks passed.
- [x] Expanded ASan/UBSan compatibility suite: 31/31 and image/results passed.

This increment changes tests and documentation only; runtime, synthesis,
conversion, and artifact semantics are unchanged.

## Typed Elimination Follow-up

Historical diagnosis below: the later
[total-result projection](2026-09-14-TOTAL-PURE-RESULT-PROJECTION.md) restores these
cases. The constructor client now lives in `tests/acceptance/`.

The Vec problem is reproducible without an append or IH. The positive control
`tests/acceptance/computed-index-arithmetic.p` checks the explicit index types
`Vec Nat (add (succ m) n)` and `Vec Nat (succ (add m n))`, then executes a
concrete use. That comparison succeeds. In contrast,
`tests/known-limitations/computed-constructor-index.p` constructs a cons with
index argument `add m n` and post-checks it at the former type; this remains
pending after 100,000 steps. Run it with the positive control as `--imports`.
It is deliberately outside the passing acceptance suite.

Constructor signature substitution builds `succ(q(add m n))`, whereas an
explicit source application may retain a sequence under `q`, where
`q(M) = Fold(M, lambda x.x)`. Thus this is not simply failure to reduce add or
infer recursive motives. It concerns the interaction of symbolic pure-result
extraction with value substitution. A globally valid raw-Core equation must
not be inferred from a typed, total input; the existing ignored-operation and
ignored-divergence counterexamples still apply.

The proof-preserving groundwork now unifies the former `pg_prove_match_body`
API as `pg_prove_elimination_body`. At a retained constructor introduction it
applies actual fields and, for direct recursive fields, suspended instances of
the original admitted induction. It uses the same formation, parameters,
generic motive, branches, ordinary substitution and Thunk introduction. It
does not evaluate the recursive calls or invent evidence for neutral inputs.
The function-graph planner uses this before its existing normalization fallback.
No Core tag, derivation rule, conversion equation or artifact codec is added.

- [x] Nonrecursive Match and renamed/substituted origins remain covered.
- [x] Direct recursion: base, two successors, and an open `succ n` unfold once.
- [x] Two recursive fields preserve IH order (asymmetric Tree result).
- [x] Neutral scrutinees, foreign evidence and missing classifiers decline.
- [x] Repeated requests reuse their result; ordinary derivation checking works.
- [x] Full debug acceptance, plus final expanded IADT/arithmetic checks.
- [x] Full ASan/UBSan acceptance and final expanded IADT check, including
  pure conversion of the open one-step body against its original elimination.
- [ ] Function-valued recursive fields need typed sequencing of their result;
  they still use the pre-existing normalization fallback, not a guessed IH.
- [ ] Specify and implement the total-pure result/substitution equations with
  typed premises, including reindexing and Higher Identity action compatibility.
- [ ] Restore unchanged Vec append and its result/image regressions.

### Literature and Scope

[Chan, Gudin, Levy and Weirich, *Commuting Conversions and Join Points for
Call-by-Push-Value*](https://ionathan.ch/assets/pdfs/ccnf.pdf), section 4, expresses
commuting conversions as moving evaluation contexts into continuation positions.
Its typing and preservation account is useful background, not a justification
for moving a constructor out of this implementation's total-value extraction.
Its language does not establish our dependent index or Higher Identity laws.

[Harper, *Effects in Call-by-Push-Value*](https://www.cs.cmu.edu/~rwh/courses/atpl/pdfs/effects.pdf)
separates values/computations and develops typed semantic equations for effects
and partiality. This reinforces the need to state the typing assumptions of an
equation. It does not supply a proof of A Program's `q` naturality or prescribe
its representation. These references were reviewed on September 14; the
implementation above derives only ordinary one-step elimination, not either
paper's full equational theory.

Relative to `e1e97cb`, implementation C/headers add 51 and remove 12 lines
(net +39). Tests, their Makefile entry and documentation are counted separately.

## Return-producing Sequence Origins

Date: September 14, 2026. Follow-up to `7af65dd`.

A separate missing path was reproduced in `tests/iadt.c`: normalize a sequence
which returns a Nat constructor, then request its typed Match body. Evaluation
succeeded, but `pg_prove_elimination_body` returned NULL because
`return_value_origin` did not traverse `PG_FOLD_ELIM`. The new assertion failed
against the previous implementation before the fix.

Origin recovery now keeps pending Fold continuations and their context maps.
It resumes a continuation only after recovering an actual Return introduction,
using the existing application-body/substitution API. Nested sequences use an
explicit work stack rather than recursive calls to the origin finder. Effect
subsumption is transparent to this provenance lookup, not permission to execute
an operation. Unknown or merely total prefixes do not become Return evidence.

This does not introduce the equation `q(Fold(M,K)) = q(K(q(M)))`, change Core
evaluation, or solve the symbolic constructor-index problem. No new proof rule,
artifact format, or evaluation-policy version is required.

- [x] Reproduce missing constructor origin after a returned sequence.
- [x] Check one/two sequential constructor steps, with a weakened contract.
- [x] Check surrounding context projection, body conversion and ordinary
  derivation reconstruction, including split Solve budgets.
- [x] Run full debug acceptance after the fix, plus the final expanded IADT
  test with projection inside a pending continuation.
- [x] Run ASan/UBSan acceptance after the fix, including the expanded IADT
  test, 31/31 compatibility cases and all six QuickSort outputs.

Verification logs: `/tmp/a-program-return-origin-debug.log` and
`/tmp/a-program-return-origin-sanitize.log`. The optimized README build/check/NF
commands also succeeded. The computed-constructor-index reproduction remains
pending at 100,000 steps with its imported arithmetic control; do not mark the
Vec milestone complete. The old README archive was compared byte-for-byte with
`440f516^:README.md`; the old Main and its tag still identify `63b00eba`.

Relative to `7af65dd`, implementation C adds 25 and removes 1 line (net +24),
and IADT regression tests add 46 lines. Documentation is counted separately.

## Sequenced Function-field Induction

Date: September 14, 2026. Follow-up to `65b5a31`.

The remaining pure-result conversion cannot be justified by an untyped Fold
rewrite. The related typed IH construction does have an ordinary-rule path
when its result does not depend on the returned child value:

```text
field : U(Pi(args, Comp(E, D(index(args)))))
IH    = thunk(lambda args.
          Fold(force(field) args, lambda child. Induction(child)))
```

The admitted field schema already restricts E to empty. The existing Fold
rule retains the field's totality contract; the helper does not strengthen it.
The output motive can depend on `args` and `index(args)`. If it depends on
`child` itself, the ordinary Fold rule rejects this construction. That case
remains unsupported by the typed-body helper, even when the field is total;
its caller retains the pre-existing normalization fallback.

`induction_field_body` constructs the arguments and child binder using ordinary
context extension, application, induction, abstraction and Fold evidence. It
never recursively unfolds the child. A shared internal `elimination_instance`
maps the generic motive and branches for both public elimination reindexing
and child specialization, avoiding an unused intermediate induction proof.
No Core, conversion, transport, effect, proof-rule or image-format change is
made. The separate total-pure result/substitution problem stays open.

- [x] Reproduce failure of typed one-step elimination on an Acc constructor
  carrying a two-argument recursive function field.
- [x] Construct the body under both unspecified and total field contracts.
- [x] Check the dependent index result classifier and repeated construction
  up to explicit alpha comparison, without alpha interning.
- [x] Check original/body raw-Core conversion and ordinary derivations.
- [x] Run the final expanded IADT checks with budgets 1 and 64, including the
  final missing-child guard, in Debug and ASan/UBSan builds.
- [x] Run full Debug and ASan/UBSan acceptance, including existing recursive
  field, Identity transport, unknown-totality and QuickSort regressions.
- [ ] Support motives depending on the returned child through justified typed
  pure-result laws; do not bypass strict Fold to make this case pass.

Full-suite logs: `/tmp/a-program-function-ih-debug.log` and
`/tmp/a-program-function-ih-sanitize.log`; both completed successfully, including
31/31 compatibility cases and six QuickSort output checks. Final focused IADT
runs followed the last failure-path guard. The optimized checker also accepts
`indexed-recursive-result.p` (3,584 steps); the minimal computed-constructor-index
case still reports pending at 100,000 steps. No claim of Vec compatibility is
made from these results.

Relative to `65b5a31`, implementation C/headers add 51 and remove 13 lines
(net +38); focused regression tests add 27 lines. Documentation is separate.
