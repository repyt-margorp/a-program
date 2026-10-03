# Captured Fold Empty-Tail Copy Deletion

Private source128 `81c3ad27` layers only `computation.c` over qualified MEM2
`2e87fd84`, using the same E12/family/Surface/head/cleanup/MEM1 v3 producer.
No live Job epoch, owner implementation, accepted code or interface is changed.

After the existing unary Return guard, captured Fold reuses the Return's
immutable argument spine when its restored caller has no trailing arguments.
The spine lives in the machine arena or decoded output graph, as in qualified
direct IADT delivery. Nonempty callers keep the original apply path; neutral,
overapplied and return2 fallback handling are unchanged. No node is mutated,
freed or recycled, and no owner/index/ABI/schema mechanism is added.

Parent/candidate O2 and ASan/UBSan builds pass the new captured, trailing,
handler, neutral and overapplied cases with identical11/15/12/13/11 steps.
All67 cuts pass readback, inert load/resave and zero-budget controls. Separate
processes pass268 O2 cross-reads plus268 sanitizer cross-reads. Affected18
records pass Core, raw codec, TotalResult, callback cleanup and normalization.
Additional transport/semantic/SourceIO/Identity and six owner-checkpoint
sanitizer checks pass, completing all seven checkpoint boundaries. Leaks and
stack-after-return checks are enabled.

The O2 persistence batch has44 recipes with only the original strict target
failing. All52 public and28 history images and both verdict/fuel tables are
byte-identical to the parent. Keep the three reload failures and historical
observer/sanitizer failures separate; no new full384 acceptance claim is made.

The unchanged external allocation audit reaches all six exact DONE/fuel counts.
Tree400 removes1725722 requests/55223104 cumulative aligned bytes; list is
unchanged. LocalSorted totals increase and vary in repeat controls. Both binaries
call `fold_head`179 times there: at most179 direct argument copies can disappear,
so the whole-program allocation delta is not fully attributed to this deletion.
The audit omits internal graph.c calls and measures cumulative requests, not
actual peak/live memory or time. No new wall/RSS comparison has run.

Preserve the initial malformed patch hunk, invalid binder-label parent fixture
and absent guessed computation-test setup failures. The corrected fixture uses
an existing semantic Request label and asserts construction; no runtime outcome
or authority check was weakened. Previous source9/cost88, MEM1 source18/cost111
and every earlier freeze remain immutable. This is a prototype readiness handoff,
with performance Goal active and Main/accepted decisions owned by Merge.
