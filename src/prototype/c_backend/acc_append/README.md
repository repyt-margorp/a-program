# Actual Acc QuickSort Callable Append

C38 emits the actual admitted append Fold and callable clauses used by QuickSort.
It composes with sealed C36 Acc and C37 partition code. The private `gs_sort`
entry's existing signature is declared in `module.h` for ordinary C modules.
No public native profile/ABI, producer/.a/schema/checker/source erasure changes.

The source Fold first selects a constructor and captures A, tag, head and the
original tail. Its native result is a callable over right. For cons, the source
IH force obtains the tail's Fold callable, which is applied to that same right;
the source constructor then prepends the captured head. Nil returns the borrowed
right directly. Source operation/binder order determines emitted statements;
the body is not copied from manual append. Actual branch fields/IH/motive types
are read from existing admitted views, with static factory A bound to Nat.

The callable stores fields at Fold time rather than rereading the original C
cell on application. Recursive tail Fold is deferred until the source force.
Right remains borrowed throughout application; the result shares that suffix.
No callable/data ownership or generalized closure policy is introduced. The
private original-whole-left capture variant explicitly refuses4; it requires
different base capture threading and is not the actual append body's shape.

`support.c` includes immutable C33 runtime/storage, generated partition/append,
then unchanged generated Acc. Function-like bindings redirect only the sealed
call sites. `gs_sort` uses generated Acc/partition/append; manual qs_apply_sort,
qs_partition and append remain present from C33 but are not this entry's path.
Remaining manual extent: Nat32/storage/Nat/LT primitives, accessibility/raw-down,
comparison, measure and outer array/copy-out orchestration.

Use an owned disk TMPDIR and qualified helpers serially:

```sh
make -j1 -f src/prototype/c_backend/acc_append/build.mk \
	OVERLAY=/path/to/qualified/overlay BUILD=/disk/build /disk/build/c_acc_append_emit
sh src/prototype/c_backend/acc_append/check.sh \
	/path/to/qualified/pointer-check /disk/build/c_acc_append_emit \
	/path/to/sealed/C36/c_acc_clause_emit /path/to/sealed/C37/c_acc_partition_emit \
	/path/to/sealed/C33/c_acc_mockup_oracle /disk/new-report
```

Actual source sort/append observations and341 finite existing-Core sorting
observations run through the composed entry. Sealed C33 resource tests retain
allocation faults/depth/capacity/count/rollback/lifetime and all proof/partition
branches. A noncandidate source mutation discards left heads, changing emitted
code and direct append result; it is never offered as an alternative sorter.
Other tests cover field capture snapshots, deferred malformed-tail refusal,
borrowed suffix identity, family mismatch, open A/whole-left/motive refusals,
prior-stream preservation, inert graph/count/machine and deterministic emission.

Target limits remain closed Nat32/depth256/node65536/borrowed source-valid data/
code/context lifetime. Private statuses1 pointer,2 metadata,3 allocation,4 depth,
5 overflow,6 capacity/count; overflow inspected, not exercised. Staged output
leaves streams untouched on unsupported shapes; final-copy I/O/publication is
caller-owned. Effect/Identity/dynamic/general indexed/callable capture contracts
and main native Acc refusal remain explicit. No full automatically generated
QuickSort/generalized lowering/accepted adoption/cost/full #61/Goal completion.
