# Actual Acc QuickSort Closed Comparator Candidate

This executable candidate selects a closed admitted source entry, such as
descending_sort := quickSort Nat &reverse_compare. The retained source lambda
calls the actual natLessOrEqual with right,left; emit.c reads that operand order,
ties the remaining lambda/type/comparator bindings to quickSort Nat, and emits a
tiny gclosed_compare adapter. Direct Nat comparison and the forward two-argument
wrapper select left,right. The actual comparison, partition, append, measure,
Acc Fold/down/indices/captured recursive parameters and creation recipe are the
unchanged source-derived bodies. No substitute sorting loop is added.

The ordinary public C declaration remains gs_sort in example/component.h.
The selected source determines direction; a client does not choose a direction
flag or supply a foreign comparator. client.c/resource_client.c receive direction
only as a test expectation for the selected product. Both actual recursive calls
retain the same source-selected comparator. No extra comparator depth frame or
foreign effect/interpretation boundary is introduced; the original source Nat
comparison's structural depth limit remains.

C59 also retains one closed ground Bool field, its actual source constructor
position, and both source matcher clauses. Each clause calls the known Nat
comparison with its admitted operand order. The generated private context is
passed through partition and both Acc recursive branches; the public ABI stays
unchanged. bool_example/component.c is the readable generated true-mode product.
The private capture_client.c tests both constructor positions against each
source clause map and rejects invalid/null contexts; no runtime public direction
flag or foreign comparator is added.

Use actual.aplink with the existing acc_creation_candidate_v1/c_acc_candidate_v1,
host-c11, fallback reject and source/object/archive. Export the closed source name
to gs_sort and the successor role to successor. Explicitly use this separate
c-acc-closed-plan/c-acc-closed-image command; older C56/C57 commands retain their
fixed roles and refusals. The shared LinkerScript grammar/producer/.a are unchanged.

Build with build.mk and an explicitly qualified producer tree, then run:

```
python3 compose.py SCRIPT.aplink NEW_DIRECTORY \
  --plan-driver BUILD/c-acc-closed-plan --driver BUILD/c-acc-closed-image
```

Repeated --cflag and --cc/--ar select ordinary native tool argv. Only gs_sort is
public; duplicate product definitions refuse. Every invocation ordinarily admits
the fixed roles, freshly emits eight parent inputs plus capture.inc, packages the
qualified bodies, pins script/image/helpers/outputs and publishes a new directory
with Linux no-replace semantics. Status0/1/2/3/4 and per-selector budget remain;
there is no trust-image mode. Atomic visibility/cleanup is not crash durability.

emit.c supports a direct known Nat comparator, its two bound-argument permutation,
or one ground Bool capture whose two actual matcher clauses have those operands.
Admitted constant/repeated-operand, unused-Bool/Nat captures, unsupported inactive
clauses and parameter/unary/wrong source shapes retain refusal4. More complex
closure environments, effects, indices, dynamic/callable main-native Acc and
generalized source functions remain open.
The source parameter body and pure-total classifier are separately validated by
the existing C57 emitter before capture reading. The new reader supplies no
private checker, synthesized expected classifier or erasure authority.

Manual target storage/roles/closures/array/Nat-LT interpretation, byte-qualified
bodies/recipe, full checked Scope/source equivalence and larger general lowering
remain. Borrowed input/output lifetime/nonoverlap, the whole private immutable
prior graph, Nat32/depth256/node65536/status1-6, transactional buffer/written and
finite sanitizer coverage persist. Source comparison of large Nat payloads still
refuses depth4; source literals/type policies do not change. Output count/byte
overflow guard, arbitrary pointer/full validation/net costs remain unproved.

Focused tests compare actual ascending/descending/forward source outputs, ordinary
source/object/archive products, source operands, recursive traces, permutation/
order, allocation/depth/capacity/rollback, parser/admission/refusal/I-O/prior/
late-output/script-freshness controls. The retained ascending Core341 oracle is
direct for forward products; reversing descending C output composes its existing
expectations, not a newly generated descending source oracle. Resource341
observations are finite C properties. Producer pointer/Core inputs remain O2;
affected target drivers/parser/generated products/clients carry O2 or ASan/UBSan/
leak instrumentation. No broad suite/timing/adoption/full #61 claim.

Initial strict build failed because the new header lacked the existing indexed
entry/occurrence declaration include; corrected the include and retained logs.
An initial descending product ran the old ascending-only client and asserted;
direction-aware tests corrected the fixture expectation, retaining the original
failure. These are target setup failures, not a producer/source-policy defect.

C59 initially refused the admitted Bool capture under the immutable C58 helper,
then added the bounded source-data reader above. Its first focused harness had
two malformed resource tuples and stopped before client testing; the corrected
harness retains those original logs. An existing-directory retry setup failure
is separately retained. Broader admitted captures and unsupported inactive
clauses remain explicit refusals rather than omitted coverage.
