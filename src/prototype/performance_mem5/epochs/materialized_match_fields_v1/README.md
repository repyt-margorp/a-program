# Materialized IADT Match Field-Array Deletion

Source128 `2344ad41` changes only `iadt.c` over qualified MEM4 `ff851fe9`, using
the frozen E12/family/Surface/head/cleanup/MEM1 v3/MEM2/MEM3 producer.
The existing materialized match callback reads its application spine from
last field to first while prepending arguments. This deletes the temporary
field-pointer array and its population pass. The shared apply helper remains
for the separate action path.

Constructor membership, exact arity, overflow and branch guards remain.
Field order, captured branch and trailing caller arguments remain. Existing
named continuation, codec, direct-head path and charged transitions remain.
No new state, allocation, owner/index, interface/schema or authority is added;
no existing argument/environment node is mutated, freed or recycled.

Focused964 records pass in O2 and ASan/UBSan. Seven materialized cases retain
25/26/12/8/10/14/12 total steps, all79 remaining continuation cuts, two inert
resaves per local cut, zero-budget readback,316 O2 and316 sanitizer fresh
cross-reads. Captures, trailing arguments, zero/two fields, neutral, under/over
arity and foreign constructors pass. Preparation seeds the already registered
materialized callback and restores the ordinary portable pure policy before
every save; these cuts start at that continuation boundary. No new policy or
descriptor is registered. The unchanged ordinary direct-head55-cut and cleanup
units also pass. Leaks and stack-after-return checks are enabled.

Affected12 build/execution records pass Core, full IADT, raw codec,
normalization checkpoint and list1915 in O2 and sanitizers. The unadapted
public partition command exits1 on original strict3; all52 images and the
full verdict/fuel table equal MEM4. Other six checkpoints/history28 and full
SourceIO/IdentityIO retain unchanged parent qualification, without a fresh claim.

Preserve the initial candidate build2 from an omitted inherited Makefile and
the initial affected driver1 from expecting uppercase DONE: its six actual
commands exit0 and the unchanged producer emits `done steps=1915`. Both original
drivers/logs are frozen separately from corrected results. Earlier observer,
sanitizer, strict and rejected-candidate failures remain referenced unchanged.

`evidence.json` pins the private1586-record source/log/image/helper freeze.
This rare materialized path was absent from the bounded100K allocation census;
no actual cost, main-peak improvement or new whole-suite claim is made.
Earlier freezes and Root's exact MEM3 cost88 remain immutable. Task publication
depends on MEM4 exact11 `ddba9930`; Merge resolves integration and owns any
accepted promotion. Full performance Goal remains active.
Chosen message: `prototype: remove materialized match field array`.
