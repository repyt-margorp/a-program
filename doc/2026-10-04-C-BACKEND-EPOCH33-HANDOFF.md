# C Backend Epoch33 Actual Acc QuickSort CODE Handoff

Date: 2026-10-04. Branch: `parallel/c-backend-20261003`.
Chosen message: `prototype: add faithful Acc QuickSort C mockup`.
Parent publication dependency: sealed C32 exact19/archive `db6a663c`; its task
commit is pending. Current published task HEAD is C31 `cc3cba3495b81628e0a68ec8b45aee4f5447d188`.
Exact11 independent files; no C32 implementation/test/evidence edits.

## Problem List

1. Deliver realistic readable executable C for actual admitted Acc QuickSort,
   with retained proof/recursion/capture correspondence and concrete limits.

## 1. Actual Source Candidate

### Subjective (User)

2026-10-04T09:16:56Z, conveyed human requirement via inquiry desk/Merge, English
paraphrase: prioritize actual admitted Acc QuickSort C CODE within12 hours,
deadline21:16:56Z / Oct5 06:16:56 JST; preserve tested C32 and defer further List
conveniences. Subsequent human clarification: a clearly labeled hand-authored
faithful candidate is acceptable. No substitute means no alternate algorithm or
false generated-success claim. Retain Acc/down recursion, indices, partition and
captures; deliver executable extent/gaps, not only correspondence docs. Continue
the original Goal/model/prototype scope without .a extension or invented erasure.
Merge owns review/integration. Both requirements are recorded in the owning Goal
and [active mockup plan](2026-10-04-C-BACKEND-ACC-QUICKSORT-MOCKUP-PLAN.md).

### Objective (Code)

**Executable hand-authored C candidate:**
[mockup.c](../src/prototype/c_backend/acc_quicksort_mockup/mockup.c), SHA
`762f264ccad935d32a5fd2ad41958bc9cbd6882c98b829b8dae0ccacb7041cfe`.
[Declarative header](../src/prototype/c_backend/acc_quicksort_mockup/mockup.h),
SHA `0fdaeb6687be5b51ce865caf1698619f99d16c1c0cffc3f38a80fca0967ed0aa`.
The [source-to-C map and usage](../src/prototype/c_backend/acc_quicksort_mockup/README.md)
identify actual admitted definitions, captures and remaining gaps. This is not
generated backend output or a full generalized lowerer.

The code follows `quickSort Nat &natLessOrEqual` from actual admitted provider
`tests/fixtures/sorted-proof-provider.p` SHA
`a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`.
The owned fixture imports this implementation; it defines no alternate sorter.
It retains LT.step/weakenRight/lift and prior edges/both indices, SizedList
index/tailSize, Partition bound/sizes/both LT proofs and Acc subject/current/
domain/relation/raw callable down. accessibleSucc's closure captures original
proof; step returns it, weaken calls its original down, lift recursively builds
the successor accessibility result. natAccessible retains actual zero/succ
construction. Partition's Fold captures element type/comparison/pivot and its
branches construct exactly the source LT.lift/weakenRight bounds.

The central C path is `qs_apply_sort`: partition the indexed tail, obtain the
two folded-down callables using the actual LT edges, apply each to its indexed
SizedList, then append lowerResult to cons(pivot,upperResult). `qs_force_down`
first calls original down to obtain child Acc, then retains type/comparison
capture and constructs the recursive Fold's callable. Source `down y edge`
returns Acc; `*down y edge` returns a callable still requiring its input.
The recurrence is not replaced by direct recursive sorting of subarray lengths.
No in-place C partition, qsort or other target sorter is introduced.

Fresh worker verification uses qualified **E8**, not Root's newer producer.
Producer `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`; runtime128 manifest
`9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
Qualified pointer reused SHA `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`;
no producer rebuild. Serial j1 existing-Core helper build0; candidate and whole
candidate/client ASan/UBSan/leak gates each9 expected zero rows. The ordinary C
executable links only mockup/client/C library, without compiler/runtime modules.

Fresh source image SHA `79b9e40c7fd94d69b313a6e75c10284269cc32368d04a5c93b92db28f21f5c46`.
Three actual source/readback observations match:
`|0,1,2,2,3,|0,1,2,3,|`. Existing Core supplies341 finite comparisons for
length0..4/Nat0..3, using exact length and bounded base5 fingerprint/element
range. Candidate's ordinary client additionally checks histogram/order and
all raw-down constructor branches/both partition branches. It checks2n folded
down applications and2n+1 Acc clauses for n input values, demonstrating traversal
through retained accessibility. These are bounded observations, not a proof of
general source correspondence. Original Bool Acc fixture separately admits0 and
evaluatesFFTT0; this candidate specializes Nat only.

Each allocation failure in a five-element actual recurrence is injected, with
no output publication or leaked storage. Null/capacity/SIZE_MAX count and two
depth failure cases retain buffer/written; successful output remains valid after
the arena is destroyed. SAN instruments the entire manual candidate and ordinary/
Core-generated clients, while producer/helper remain O2. No wall/RSS cost run.

Native backend remains unsupported: initial existing fixture refuses its selected
declaration-head4. With a genuinely closed List classifier and explicit Bool
module alias, both checked/trusted controls reach
`unsupported native expression, argument or capture representation`4, with no
product. This isolates a target limitation beyond the old declaration selector;
fresh admission/source readback work, so no producer/checking gap is demonstrated.
No backend emitter/representation, schema or accepted implementation was changed.

Retained initial failures: strict C misleading-indentation build1 corrected;
helper linkage2 missing representation dependency then linkage2 missing emit
dependency corrected; initial deeper native probe1 selected imported Bool without
an explicit module definition alias. Established source_sort alias setup corrects
it to the real deeper target refusal4. Initial images/logs/code diagnostics remain
under `.evidence/epoch33-acc-quicksort-mockup-20261004/`; no shared cleanup.

| Pinned report/binary | SHA256 |
| --- | --- |
| verification.json | 4c91b3ff027081fee06843828dc0129e47c77219761869a7acdf910fd6c78a03 |
| qualified-inputs.json,327 inputs/runtime128 | 5f7752610448da8476052783a0d735e7108ba2a2b580ad9caa5729329a12b528 |
| existing-Core helper | 0b70125edc028492857b5cc69d463e7dd623df41c0d959322a917e1a40364bad |
| strict O2 candidate/client | 6439f1b4cd2e36a26bc4f1c609107df5d5ed80190a1c3fd260c86666b85b8bfd |
| candidate/client SAN | f2585837ce522430729395e66e1734e981ba8c2960486fe2392c753bf7de5e0b |

Exact11 files:

- `doc/2026-10-03-C-BACKEND-GOAL.md`
- `doc/2026-10-04-C-BACKEND-ACC-QUICKSORT-MOCKUP-PLAN.md`
- `doc/2026-10-04-C-BACKEND-EPOCH33-HANDOFF.md`
- `src/prototype/c_backend/acc_quicksort_mockup/README.md`
- `src/prototype/c_backend/acc_quicksort_mockup/mockup.h`
- `src/prototype/c_backend/acc_quicksort_mockup/mockup.c`
- `src/prototype/c_backend/acc_quicksort_mockup/fixture.p`
- `src/prototype/c_backend/acc_quicksort_mockup/client.c`
- `src/prototype/c_backend/acc_quicksort_mockup/oracle_test.c`
- `src/prototype/c_backend/acc_quicksort_mockup/build.mk`
- `src/prototype/c_backend/acc_quicksort_mockup/check.sh`

Exact manifest/submitted tar/raw manifest/freeze record are retained separately
in that evidence directory. C32 archive/manifest independently rechecked exact;
only live owning Goal advances independently. No C32 implementation/test/global
README/frozen evidence was altered. Tabs/English/C11/no typedef/declarative header/
no ML comments and diff checks pass; manual source/docs changes used apply_patch.
Publication/current-producer/Main review remains Merge-owned.

### Assessment

The requested readable C CODE now has a concrete executable extent: a manual
closed Nat/LT/natural-comparison realization of the original algorithm, with
proofs/indices/captures retained and scoped lifetime. It is not merely a sketch.
The explicit structures and raw/folded callable split provide a target blueprint
for later derivation from admitted views, without pretending that derivation is
implemented. Source/finite Core observations support only the pinned extent.

Remaining gaps: automatic indexed constructor/callable recursive field/motive
extraction; arbitrary A and comparator/captured-source closure conversion; general
sharing/Identity/effect/ownership correspondence. Nat32/depth256/node65536 and
readable immutable input/output lifetime/nonoverlap are target constraints.
Nat32 index overflow/node-bound guards are inspected, not reached by the finite
comparisons. Acc-at-zero has no valid LT y zero input; malformed foreign metadata
gets refusal2 instead of a fabricated proof. Target metadata consistency is not
source proof checking/admission or an erasure policy. Native compiler/full #61,
costs/adoption/accepted promotion and full Goal remain open.

Root operational publication dependency: use immutable C32/C33 snapshots in order
or resolve owning-doc integration explicitly. Unknown C32 commit is not an
implementation blocker. Shared Git remains read-only and was not mutated by this
worker. Source-owner needs, if later demonstrated, go through Merge to Job.

### Plan

- [x] Record conveyed human requirement/clarification and preserve sealed C32.
- [x] Deliver realistic executable actual-source C mockup with explicit proof/
  down/index/partition/capture mapping before21:16:56Z deadline.
- [x] Verify fresh source/Core/O2/full candidate SAN/resource/native-refusal
  extent, retain failures and prepare exact11 CODE/test/docs handoff/notice.
- [ ] Merge audit/task publication/current-producer/Main integration separately.
- [ ] Review target automatic indexed/callable gap after this code review;
  further List conveniences stay deferred. Original Goal remains active.
