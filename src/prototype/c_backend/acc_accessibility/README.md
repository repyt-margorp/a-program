# Actual Nat Accessibility Recurrence

C39 emits the admitted natAccessible Nat Fold recurrence used by actual Acc
QuickSort. The successor forces the predecessor's Acc IH before passing that
original predecessor and child proof to accessibleSucc. The zero clause emits
the source Acc constructor at zero with its exact supplied down closure.

The generated recurrence composes with sealed C36 Acc/C37 partition/C38 append
code. Its gs_sort entry retains the existing private declaration/signature.
Nat32 storage/Nat/LT primitives, accessibleSucc/raw-down/zero-down, comparison,
measure and outer array/copy-out orchestration remain explicitly manual C33.
In particular, the checked accessibleSucc LT Match has Identity transport-right
on step and transport-left on weaken/lift. This epoch does not lower or erase
those operations. probe.c reads/pins actual terms, fields and classifiers with
source-machine wrappers and unchanged source graph/evidence counts.

This source-specific helper accepts complete selected Core alpha identity with
the supplied admitted source reference, then reads its Fold/clause/type fields.
Provenance graphs can retain imported Folds next to a different executable;
matching only a context binder is insufficient. Different wrappers/callbacks
refuse4 until executable/Fold association is established. Actual clause syntax
determines Acc construction, recursive force, SEQ and helper arguments. Manual
zero/successor bodies are bound by exact source identity, not arbitrary closures.
This is not a generalized Nat/Acc compiler or a hash/label sorter template.

Serial use, with an owned disk TMPDIR and the qualified source overlay:

```sh
make -j1 -f src/prototype/c_backend/acc_accessibility/build.mk \
	OVERLAY=/path/to/qualified/overlay BUILD=/disk/build /disk/build/c_nat_accessibility_emit
sh src/prototype/c_backend/acc_accessibility/check.sh \
	/path/to/qualified/pointer-check /disk/build/c_nat_accessibility_emit \
	/path/to/sealed/C36/c_acc_clause_emit /path/to/sealed/C37/c_acc_partition_emit \
	/path/to/sealed/C38/c_acc_append_emit /path/to/sealed/C33/c_acc_mockup_oracle \
	/disk/new-report
```

Tests compare actual source sorting and Acc subject observations,341 existing-
Core finite sorting observations and the sealed C33 resource/rollback client.
Native subject checks0..31, every manual raw-down branch on emitted proof data,
and depth300 refusal are separate finite C controls. Changed zero callback,
unsupported accessibleSucc body, wrong family and prior-stream shape keep
explicit refusal4/no-partial output. Initial missing header/aliases, premature
gate and unsafe provenance-selection results are retained; the last negative
control keeps its original expected refusal.

Target limits remain closed Nat32/depth256/node65536, source-valid immutable
borrowed data/code/context lifetime and private statuses1-6. Overflow guard is
inspected, not exercised. Finite SAN covers executed paths; final destination
I/O/publication cleanup is caller-owned. Effect/Identity/dynamic/general indexed/
callable contracts and main-native Acc refusal remain explicit. No public ABI/
profile/.a/producer/schema/checker/source erasure change, automatically generated
whole QuickSort, accepted adoption/cost/full #61/Goal completion.
