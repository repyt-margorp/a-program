# Actual Acc QuickSort Nat Comparison

C40 is a separate source-specific prototype. It emits the actual admitted
`natLessOrEqual` callable Nat Fold used by Acc QuickSort, rather than replacing
the source algorithm with a C comparison operator. The hand-authored executable
[C33 candidate](../acc_quicksort_mockup/mockup.c) remains explicitly labeled.

The generated `gc_fold` snapshots the left constructor and predecessor.
`gc_apply` follows the actual source clauses: zero returns true; successor first
matches right, returning false for zero and forcing the left predecessor IH
only for successor. Both captured predecessors remain explicit. Bool constructor
positions bind to the existing private callback convention, 1 true/0 false,
with arena status separate. `gc_compare` composes that Fold and application.

`emit.c` checks the actual selected Core/reference, Nat/Bool declarations,
callable motive, context binder, original constructor field, IH and clause terms.
An unrelated admitted comparator refuses even when supplied as its own reference.
This is a bounded experiment, not generalized executable/Fold association.
Emission does not advance source machines or change owner graph/store counts.

`support.c` composes unchanged C36 Acc clauses, C37 partition, C38 append and C39
Nat accessibility with this comparator. Existing manual sorting/partition/append/
comparison/Nat-accessibility definitions remain included but are not the composed
entry's path. Manual extent still includes C33 storage/Nat/LT representation,
accessibility successor/raw-down/zero-down checked transport bodies, measure and
outer array/copy-out orchestration. Source Identity operations are not erased.
There is no main backend, public ABI, producer, schema or checker change.

Focused serial O2 and new/reused emitter-runtime/composed-client ASan/UBSan/leak
checks each have 33 matching expected rows: 29 zero and four refusal4. Actual
source sort and direct comparisons match; 341 existing-Core sorting and 256
comparison observations pass. Core oracles/pointer/read-only probe are O2 tools,
not sanitized. Native small comparisons, lazy extreme opposite operands,
deferred IH, malformed private tag, parent/depth refusal, allocation/capacity/
rollback and sealed C33 resources are separate C controls. Finite SAN execution
does not establish the unchecked caller preconditions or general completion.

Limits remain closed Nat32, depth256/node65536, source-valid immutable borrowed
code/context/data lifetime, private statuses1-6 and staged nonoverlapping copy-out.
Overflow guard is inspected, not exercised; destination stream/publication cleanup
is caller-owned. Main-native Acc, general indexed/callable/dynamic/effect/Identity
refusals and full #61/Goal remain open. No cost, accepted promotion or generalized
native QuickSort claim.

Evidence: `../.evidence/epoch40-acc-comparison-20261004/`, especially executable
`terminal-O2/main/comparison.inc`, source/product/binary hashes and exact freeze.
The [handoff](../../../../doc/2026-10-04-C-BACKEND-EPOCH40-HANDOFF.md) names every
publication path and distinguishes worker E8 from pending Root qualification.
