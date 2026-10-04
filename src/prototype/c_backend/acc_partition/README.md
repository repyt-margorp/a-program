# Actual Acc QuickSort Partition Expressions

This separate C37 experiment emits the admitted partition Fold and its inline
partitionByDecision/partitionLower/partitionUpper expressions, then composes them
with sealed C36's generated Acc clause. It changes no main backend profile,
producer/.a/schema/checker/source erasure policy or public ABI.

The generated capture retains A, comparison/code/context and pivot. The source
cons clause calls comparison(head,pivot) before forcing the tail thunk; the thunk
retains the same captures, tail size and original SizedList tail. Bool Match then
selects the actual lower/upper expression, retaining all six Partition fields,
SizedList sizes and LT.step/weakenRight/lift proof fields. Static lambda/SEQ
pending operands are bound in source order; helpers are inlined from source
terms, not replaced with the manual partition implementation.

Constructor parameters come from the retained classifier of the **exact** source
constructor term. The target reads actual family arguments and static lexical
bindings, including Partition's bound parameter and SizedList's result index.
Missing, unsupported or conflicting instances refuse; expected types are not a
fallback. No source checking/evaluation/substitution or new evidence/term producer
runs during emission. The source driver supplies ordinary admission. This is
bounded syntax/representation applicability, not another checker or generalized
dependent lowering. Refusal leaves the stream untouched; final-copy destination
I/O and product publication remain caller-owned.

`support.c` includes immutable C33 code/storage, generated `partition.inc`, then
unchanged C36 `clause.inc`. A function-like private macro binds only the latter's
partition call to `gp_partition`; struct tags and frozen bytes stay unchanged.
The entry `gs_sort` calls generated Acc and partition code. Manual qs_apply_sort
and qs_partition may be present from C33 but are never the gs entry's path.

Remaining manual extent is closed Nat32 representation/storage/Nat/LT constructor
primitives, accessibility/raw-down bodies, comparison, append, measure and outer
array/copy-out orchestration. C34/C35 full-Nat pointer experiments remain separate;
no automatic full/generalized QuickSort or public indexed/callable ABI claim.

Focused serial verification accepts qualified helpers:

```sh
make -j1 -f src/prototype/c_backend/acc_partition/build.mk \
	OVERLAY=/path/to/qualified/overlay BUILD=/disk/build /disk/build/c_acc_partition_emit
sh src/prototype/c_backend/acc_partition/check.sh \
	/path/to/qualified/pointer-check /disk/build/c_acc_partition_emit \
	/path/to/sealed/C36/c_acc_clause_emit /path/to/sealed/C33/c_acc_mockup_oracle \
	/disk/new-report
```

The actual original source algorithm is the candidate. A labeled noncandidate
partition mutation swaps comparison arguments solely to test that source changes
affect emitted code and direct partition execution; it is never offered as an
alternative sorter. Actual source outputs and341 finite Core sorting observations
are compared with the composed generated entry. Resource tests reuse sealed C33's
client with only its translation-unit entry renamed; support has no such rename.
Additional provider probes check captured context/pivot, front-to-tail comparison
order, early failure before tail allocation and malformed Bool refusal2.

Unsupported generic/open A, wrong families/motives, missing or ambiguous static
constructor metadata, unknown syntax/calls, arbitrary captures and effect/Identity
contracts remain explicit. The main backend's native indexed/callable Acc refusal
is unchanged. Target limits: closed Nat32, depth256,65536 nodes, borrowed source-
valid data/code/context lifetime, staged finite copy-out; callback side effects
are not rolled back. Private statuses1 pointer,2 metadata,3 allocation,4 depth,
5 Nat32 overflow,6 capacity; overflow inspected, not exercised. Finite observations
and affected sanitizers cover executed paths, not all values/providers/lifetimes.
No cost/adoption/full #61/Goal completion follows.
