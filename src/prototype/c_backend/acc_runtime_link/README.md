# Actual Acc runtime-parameter LinkerScript candidate

This prototype connects the admitted C60 `runtime_sort` source wrapper to
ordinary C source, object and archive products. `actual.aplink` selects its
existing successor role and an explicit `acc_runtime_bool_candidate_v1`
lowering / `c_acc_runtime_bool_candidate_v1` target ABI. It does not authorize
general native lowering or source erasure. Main `a-to-c --link` and the public
publication API refuse this candidate with status4 before artifact/output access;
use the labeled separate command.

Build `c-acc-runtime-plan` with `build.mk` against a qualified producer overlay.
Reuse the matching C60 `c-acc-runtime-image` helper and an ordinarily admitted
materialized image containing `runtime_sort` and `succ_access`. Put the image
beside the script as `actual.a`, or change only that artifact path. Then run:

```sh
python3 src/prototype/c_backend/acc_runtime_link/compose.py actual.aplink product \
	--plan-driver /qualified/c-acc-runtime-plan \
	--driver /qualified/c-acc-runtime-image --cflag=-O2
```

Choose `product source`, `product object` or `product archive` in the script.
Compile another ordinary C translation unit against `component.h` and the
selected `component.c`, `component.o` or `library.a`. Its `gs_sort_mode` receives
`GS_BOOL_FIRST` / `GS_BOOL_SECOND`, the actual source Bool constructor positions.
One binary accepts both data values; there is no foreign comparison callback.
The unchanged C60 example shows the actual Acc/down recursion, indexed partition,
captured comparator context and manual array boundary. The opposite admitted
wrapper selects opposite source clauses through the same script role.

The composer pins the script, image and both helpers through compilation,
records nine fresh body inputs and output hashes, and publishes a new directory
atomically. Existing outputs are preserved. Status1 is a native-tool failure,
2 is syntax/I/O/publication failure, 3 is pending fuel and 4 is unsupported.
The parser is a target adapter; ordinary source admission remains in C60.

`check.py` exercises both source clause maps, all products, source output agreement,
retained finite Core cases, allocation/depth/rollback and both recursive captures.
It also checks parser/ABI/product/role refusals, early native/API refusals,
script/image/helper mutation, I/O faults and no-replace cleanup. Serial O2 and
ASan/UBSan/leak phases reuse exact C60 inputs. Finite checks do not establish
whole Scope/source equivalence or arbitrary graph/pointer safety.

The fixed roles, storage, closures and array adapter remain manual target code;
Nat32/depth256/node65536 and borrowed immutable graph/nonoverlap/lifetime limits
remain. General indexed/callable native Acc/QuickSort, full #61, accepted adoption
and actual net cost are open. No producer/schema/checker/erasure changes.
