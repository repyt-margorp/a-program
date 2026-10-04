# Actual Acc Fold and Callable IH Adapter

## Problem List

1. Make the admitted QuickSort Acc Fold's original/callable-recursive distinction
   executable while retaining the separate manual full sorting candidate.

## Subjective (User)

Existing conveyed human requirement/clarification, 2026-10-04 via inquiry desk/
Merge, English paraphrase: prioritize actual admitted Acc QuickSort readable C,
preserve Acc/down, indices, partition and captures, and identify executable extent/
gaps. Faithful labeled manual code is allowed; no substitute algorithm, invented
source erasure or generated-success claim. Continue the same Goal and defer List
conveniences. The deadline applies to the separate executable mockup deliverable.

## Objective (Code)

C33 [mockup.c](../acc_quicksort_mockup/mockup.c) supplies the requested faithful
hand-authored full closed-Nat sorting candidate. C34 derives private indexed data
declarations from admitted descriptors. This C35 emitter reads actual
`quickSortAcc Nat &natLessOrEqual` occurrence allocations, admitted direct
elimination inputs and source scope/classifiers; it emits executable **Fold/IH
plumbing only** plus those declarative representations. Main native emission,
public ABI, producer, .a/schema and source algorithms remain unchanged.

The target reader requires the actual closed Nat/LT family instance, captures A
and le, one Acc clause with current/original-down/IH fields in that order,
`down: U((y:Nat)->LT y current->F{}Acc Nat LT y)` and
`IH: U((y:Nat)->LT y current->SizedList Nat y->F{}List Nat)`.
It checks those source binder/index associations descriptively; it neither
checks source typing nor substitutes/evaluates to obtain them. The source driver
provides admitted selected subjects. Inspection forbids source eval/substitution/
WHNF/typed-query advances and leaves source graph/evidence counts unchanged.

Generated `af_fold` constructs the callable IH for the source Acc branch;
`af_force_down` calls original down to obtain child Acc, then folds that child;
`af_apply` applies the returned callable to SizedList. The same element-type and
comparison capture travels through those phases. Original down and folded IH
are different C structures and operations. No array sorter or alternate algorithm
is emitted. The `af_program.clause` callback remains a supplied/manual body.

The ordinary client labels its handwritten clause code explicitly. It follows
the actual source at sizes0/1: nil returns List.nil; the singleton partition nil
case supplies both LT.step-zero bounds, uses both IH calls, then performs the
original append(empty,cons(pivot,upper)) result. It matches the admitted source's
empty/singleton length/head observations. Sizes beyond1 refuse4 in the manual
fixture. The comparison capture is retained but not invoked in those cases;
no comparison interpretation/execution coverage is claimed. C33's independent
full manual candidate/source comparisons remain the broader executable evidence.

Run `sh check.sh POINTER BUILD/c_acc_fold_emit OUTPUT` for fresh source admission,
deterministic C/header generation, ordinary C execution/source comparison and
wrong-IH C compiler refusal. `build.mk` builds the selected-source helper; it
reuses the qualified pointer binary. Regenerate all output C/headers; never
edit generated products. No A Program runtime library is linked to the client.

## Assessment

This lowers the actual callable IH traversal/sequence adapter, not the source
branch expression or a generalized Fold compiler. Nat uses the complete pointer
structure from C34; indices/proof fields/type/relation tokens remain represented.
The target reader rejects accessibleSucc's different motive, natAccessible and
partition selections instead of pretending their contracts are the same.

Source-valid indices/metadata, interpretation/purity of supplied callbacks and
data/code/context lifetime are caller preconditions. Runtime metadata guards do
not become source checking/proof admission. Returned closure contexts in this
fixture occupy explicit caller-owned storage; arbitrary escaping/owning closure
conversion is unfinished. Private statuses1 null,2 malformed local fields,
4 nested clause-construction depth; provider failures propagate. Staging preserves
outer result on failure. The depth guard covers clause construction, not execution
of an arbitrary supplied result callback. Provider side effects/allocation are
not rolled back by the adapter. The helper has no transactional publication or
public object/library profile. No cost/adoption/full #61/native QuickSort completion.

Remaining concrete work: automatically lower actual captured clause expressions,
partition operations, indexed constructor metadata and IH callable results,
including their allocation/lifetime handling. This candidate does not use the
manual clause slot as evidence those expression bodies have been generated.

## Plan

- [x] Read actual admitted Fold/motive/field/IH/capture associations.
- [x] Emit native original-down -> child-Fold -> callable-application machinery.
- [x] Terminal O2/full helper-client SAN/inert/source/refusal/resource verification.
- [x] Prepare exact code/test/docs freeze and honest partial automatic handoff.
- [ ] Actual clause/body/motive/capture/constructor lowering in a separate step.
