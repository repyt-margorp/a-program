# Actual Acc QuickSort Clause Experiment

This separate C36 candidate emits the actual admitted `quickSortAcc Nat
&natLessOrEqual` clause expressions. It advances C33's faithful hand-authored
executable mockup and C35's generated Fold adapter. It changes no public backend
profile, producer, `.a` format, schema, checker or source erasure policy.

The emitted `gs_apply` body follows the saved source terms: SizedList Match,
partition call, Partition field binding, lower `*down`, upper `*down`, pivot
constructor and append. Static lambda applications bind their actual arguments;
source SEQ controls statement order. Dependent Match's extra original-down/IH
arguments remain scoped. Capture references select `f->element_type` and
`f->comparison`; child closures preserve both. The raw field returns Acc, whose
recursive Fold produces the callable subsequently applied to SizedList.

`emit.c` reads existing admitted occurrence/context/classifier/layout views and
reuses C35's applicability reader. It neither executes source computations nor
constructs source terms/evidence. Exact alpha comparison identifies the two
audited source-selected helper bodies; it does not evaluate them. Other source
helper bodies, dynamic calls, effects, arbitrary motives/captures/data layouts,
unbound references and unsupported syntax refuse. Private status4 means target
shape refusal. Staging keeps the output untouched on refusal; final destination
I/O errors can leave a partial stream, so a publishing caller owns cleanup.

The private C33 representation and implementations of accessibility, LT proofs,
partition, append, comparison, measure, allocation and array/copy-out entry
remain **manual**. `support.c` includes immutable C33 code, then generated
`clause.inc`; its entry calls `gs_apply`, never manual `qs_apply_sort`. C34/C35's
full-Nat pointer declarations are a separate component experiment; this composed
candidate retains C33's closed Nat32 fields and indices. The admitted helper
mapping is audited for this pinned source, not a general plugin registration ABI.

Run serially with a qualified producer/helper and owned disk-backed TMPDIR:

```sh
make -j1 -f src/prototype/c_backend/acc_clause/build.mk \
	OVERLAY=/path/to/qualified/overlay BUILD=/disk/build /disk/build/c_acc_clause_emit
sh src/prototype/c_backend/acc_clause/check.sh \
	/path/to/qualified/pointer-check /disk/build/c_acc_clause_emit \
	/path/to/C33/c_acc_mockup_oracle /disk/new-report
```

The fixture imports the actual provider. Its separately labeled `drop_acc`
mutation removes pivot insertion solely to verify source dependence; it is not
a proposed sorter. Its emitted C differs and execution produces the source's
empty result. Swapped helper and wrong motive selections refuse without partial
code. Deterministic emission, actual source observations,341 finite existing-Core
observations and the immutable C33 resource client run against the generated
clause composition. Resource/oracle translation units alone rename the old entry
to `gs_sort`; the generated/support translation unit uses no such macro.

Target limits remain explicit: closed Nat32; depth256;65536 allocated nodes;
borrowed input/output lifetime; finite staged copy-out. Private runtime statuses
1 pointer,2 metadata,3 allocation,4 depth,5 Nat32 overflow,6 capacity. Finite
observations and sanitizers cover executed paths, not all inputs or callbacks.
Nat32 overflow is inspected but not exercised. This is executable source-driven
clause emission with manual helper/runtime composition, not full automatically
generated QuickSort, generalized indexed/callable lowering, accepted adoption,
actual cost qualification or full #61/Goal completion. Main backend's historical
native Acc refusal and effect/Identity/dynamic/indexed contracts remain explicit.
