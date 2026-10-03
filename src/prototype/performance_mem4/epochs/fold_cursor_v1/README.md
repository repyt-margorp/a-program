# Captured Fold Caller-Prefix Traversal Deletion

Source128 `ff851fe9` changes only `computation.c` over frozen MEM3 `81c3ad27`,
on the same E12/family/Surface/head/cleanup/MEM1 v3/direct-IADT producer.
The checked cursor obtains the continuation and validates every consumed
caller argument once. An empty caller tail then enters that same closure and
reuses the immutable Return spine. Nonempty tails retain the original apply
path. Unary Return, neutral Force, handler polarity and return2 handling remain.

For a plain empty caller, source inspection proves five prefix-link advances
become two. This is deletion of repeated work, not a measured speed claim.
No node is mutated or freed; no allocation, owner/index, interface/schema,
authority or charged transition is added. Existing spine lifetimes remain.

Focused684 records pass: five equal results/fuels11/15/12/13/11, all67 cuts,
268 O2 and268 ASan/UBSan separate-process cross-reads, inert load/resave,
zero-budget readback, null-continuation/incomplete-handler error boundaries
and head cleanup. Targeted22 records pass Core, raw codec, TotalResult,
normalization checkpoint, three callback-lifetime/cleanup units, full SourceIO,
full IdentityIO and list DONE1915 in O2 and sanitizers. Leaks and
stack-after-return checks are enabled.

The unadapted public partition command exits1 on the original strict3.
All52 images and the full verdict/fuel table equal the frozen parent exactly.
The other six owner checkpoints and28 history images retain parent
qualification; they were not freshly rerun here. No new whole-suite acceptance,
actual wall/RSS, native comparison or Goal-completion claim is made.

`evidence.json` pins1125 private source/log/image/helper records. `test-pins.json`
pins the exact reused tests, including the already published MEM3 Fold unit.
Keep historical observer, sanitizer, strict and setup failures separate as
recorded in `known-failures.json`. Existing freezes and cost pins stay immutable.
This exact task-branch handoff is a prototype; Merge owns integration and
accepted promotion. Chosen message: `prototype: eliminate repeated Fold caller-prefix walks`.
