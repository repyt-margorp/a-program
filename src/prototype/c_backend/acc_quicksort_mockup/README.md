# Actual Acc QuickSort C Mockup

## Problem List

1. Make the admitted Acc QuickSort's C realization readable and executable
   without replacing its algorithm or claiming general automatic lowering.

## Subjective (User)

2026-10-04, conveyed human requirement/clarification via inquiry desk and Merge,
English paraphrase: within12 hours deliver realistic readable C CODE for actual
admitted Acc QuickSort. A clearly labeled hand-authored faithful candidate is
acceptable. Retain Acc/down recursion, indices, partition and captured parameters;
report executable extent/gaps. No alternate sorter, invented erasure, .a extension
or false generated-success claim. Deadline21:16:56Z; original Goal remains active.

## Objective (Code)

`mockup.c` is **hand-authored executable C**, not generated backend output. It
transcribes the closed `quickSort Nat &natLessOrEqual` instance from the actual
admitted `tests/fixtures/sorted-proof-provider.p` (SHA
`a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`).
The test fixture imports those definitions rather than providing a new sorter.
The candidate's C structures keep both indices of LT, its three constructors/
prior edges, SizedList size/tailSize, Partition sizes/bounds and Acc callable
fields. Read `qs_apply_sort` for the actual algorithm; its two `qs_force_down`
calls precede the original append/cons combination.

| Admitted definition | C correspondence |
| --- | --- |
| LT.step/weakenRight/lift,60-67 | qs_lt plus lt_step/lt_weaken/lt_lift; both indices and prior retained |
| Acc.acc,69-71 | qs_acc subject/current/domain/relation and original qs_acc_down |
| accessibleSucc,73-79 | successor_down captures proof: step returns proof; weaken calls original down; lift constructs accessibleSucc of its child |
| natAccessible,83-93 | qs_nat_accessible retains zero/succ recursion and constructed Acc values |
| SizedList/Measured,95-103 | explicit qs_sized_list index/tail_size; measure returns indexed values |
| Partition/lower/upper,105-148 | explicit bound/lower_size/upper_size and LT proofs; exact lift/weaken constructor choices |
| partition,150-161 | partition_capture retains type/le/pivot through the folded tail; decision before tail |
| append,175-180 | original List fold, applying folded tail to captured right |
| quickSortAcc,275-293 | qs_sort_closure captures type/le/access/current; qs_folded_down maps original child Acc to callable over matching SizedList |
| quickSort,295-299 | measure, then natAccessible, then apply the Acc Fold result |

`down y edge` obtains a smaller **Acc** from the original callable field.
`*down y edge` obtains the recursive Fold's **callable** result, which must still
receive its SizedList. `qs_force_down` keeps this distinction: obtain child Acc,
retain the same type/comparison capture, construct the recursive sort closure,
and apply it to the matching indexed partition. Neither recursive sort call is
chosen just from a subarray length; the actual LT edge and Acc down path are used.

Fresh pinned worker E8 source admission and existing-Core observations pass.
The entire hand-authored mockup and clients pass strict O2 and ASan/UBSan/leak
controls: three actual source/readback observations,341 finite Core fingerprint
comparisons, all LT down branches/both partition branches, allocation failure
exhaustion, atomic output and explicit depth limits. Existing backend native
representation refusal remains4; there is no automatic native success.

For a standalone ordinary client, compile `mockup.c` with a file including
`mockup.h` and call `qs_mockup_sort`. No compiler/runtime library is linked to
the resulting candidate executable. The retained gate uses:

```sh
cc -std=c11 -Wall -Wextra -Werror -O2 mockup.c client.c \
	-Wl,--wrap=malloc -o candidate
./candidate
```

`check.sh POINTER ORACLE OUTPUT` freshly admits the actual fixture and compares
the candidate to source/Core; `build.mk` builds the separate existing-Core test
helper from the chosen qualified source overlay. It is not a producer rebuild.

## Assessment

Executable extent: one closed Nat/LT instance, actual natural comparison and
source recurrence, scoped arena with all proof/value/closure contexts live until
return. Nat is target uint32 representation; type/relation identity are explicit
closed-instance tokens. Input arrays and buffer/written/trace must be separate
and remain valid. Status1 pointer,2 internal metadata mismatch,3 allocation/node
bound65536,4 execution depth256,5 Nat32 overflow,6 capacity/count. Failures preserve
buffer/written; optional trace reports the attempted path. SIZE_MAX count/depth/
allocation/capacity failures are exercised. Nat32 index overflow and node bound
guards are inspected, not reached by the bounded source comparisons.

Remaining concrete gaps: this is a manual source-specific transcription, with no
automatic extraction of these representations/motives from admitted views, no
general indexed constructor or callable recursive-field lowering, and no arbitrary
element/comparator closure conversion. Metadata guards are consistency checks on
the candidate's own constructed data, not source admission or proof checking.
Acc-at-zero has no valid LT y zero argument; the C down entry explicitly refuses
malformed foreign metadata rather than fabricating a proof. It does not model
the source's unreachable-branch return as a generally callable C function.
Allocation/sharing/cost correspondence is not measured or claimed. General effects,
ownership, Identity and accepted/native full #61 completion remain outside this
candidate. `.a`, source algorithms, producer/schema/checker and erasure authority
remain unchanged.

## Plan

- [x] Provide readable executable actual-Acc C candidate and source correspondence.
- [x] Compare against admitted source/Core and run focused O2/candidate SAN.
- [ ] Merge review/publication/current-producer qualification separately.
- [ ] Derive future automatic target support only after concrete boundary review;
  no further List conveniences under the superseding human priority.
