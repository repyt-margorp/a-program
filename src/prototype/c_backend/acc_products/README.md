# Actual Acc QuickSort Standalone C Candidate

`pack.py` packages exact qualified C44 products into readable `component.c`, a
declarative `component.h` and `provenance.json`. It copies the actual-source
Acc/down, successor/capture, partition, append, accessibility, comparison, measure
and outer bodies unchanged. Each section records its source or manual target
extent. The historical unused hand-authored sorter is excluded. No compiler or
A Program runtime library is required by the packaged module.

The checked-in generated [example/component.c](example/component.c) is the exact
708-line module verified as source/object/archive, not a correspondence-only
sketch. [example/component.h](example/component.h) is its ordinary client header.
Regenerate through the packager; do not edit generated files by hand.

This is a **fixed source-specific executable candidate**, not general native
backend success. Packaging verifies the exact selected input bytes rather than
admitting another source program. The manual target representation retains Acc
indices, original raw down versus folded IH, captured parameters, scoped nominal
LT actions and quote-before-force. It does not serialize source Identity maps or
prove general action/closure equivalence. Body comments inherited byte-exact from
earlier epochs describe their earlier stage; section labels state this composition.

```sh
python3 pack.py EXACT_C44_PRODUCTS product
cc -std=c11 -Wall -Wextra -Werror -O2 -Iproduct product/component.c client.c -o client
./client
cc -std=c11 -Wall -Wextra -Werror -O2 -c product/component.c -o product/component.o
ar rcs product/library.a product/component.o
cc -std=c11 -Wall -Wextra -Werror -O2 -Iproduct client.c product/library.a -o archive-client
```

For the supplied snapshot, use `example` as the product directory; compiling its
source directly needs only a C11 compiler and the standard C library.

The existing private `gs_sort` signature takes borrowed Nat32 arrays and separate
buffer/written/optional trace. Failure preserves buffer/written; all proof/value/
capture allocations are freed before return. Status1 pointer,2 metadata,3
allocation/node65536,4 depth256,5 Nat32 overflow,6 capacity/count. Source-valid
immutable internal values, private tokens and nonoverlap remain preconditions.
The foreign zero-down entry refuses2. Finite source/Core/SAN observations cannot
establish unrestricted equivalence, cost or safety for invalid foreign pointers.

`native.aplink` separately probes the existing main-native profile on the same
admitted source. A refusal is a remaining target boundary, not evidence that the
private candidate did not execute. No ABI/profile/.a/producer/schema/checker/
erasure change, accepted adoption or full #61/Goal completion is claimed. Detailed
evidence and publication state belong in the C45 handoff and owning Goal table.

Current fc52755b/runtime120 backend build fails before that probe: the target
capture code still includes removed `support.h` and uses `pg_support_contains`.
Passing `unavailable` as the backend argument to `check.sh` retains both native
expectations as explicit NOT_RUN records. It does not pass those checks or treat
historical refusal4 as fresh. The standalone composition uses no such dependency.
