# Broad prototype verification

`make_shell.py` records each Make recipe's original exit code while a diagnostic
`make -k -i` run continues after failures. The aggregate Make exit is not an
acceptance result. Audit every record's `exit`, the original log and the requested
target set before reporting a gate result. Commands are run by `/bin/bash` with
the original Make shell arguments; the recorder does not change their outcome.

For example, from the repository root, with a private overlay and output:

```bash
PERFORMANCE_GATE_LOG=/tmp/performance-gates/recipes.jsonl \
make --no-print-directory -s -k -i -j2 -f /tmp/performance-overlay/src/Makefile \
SHELL="$PWD/src/prototype/performance_verification/make_shell.py" \
BUILD=/tmp/performance-overlay/build check-acceptance \
> /tmp/performance-gates/acceptance.log 2>&1
```

Use a new log path for each gate/variant and pin the overlay source hashes,
compiler flags, targets and any explicit test adapters. Original head-policy
observer failures must remain separate from the published Core adapter's results.
This diagnostic helper collects no comparative wall/RSS measurements.

The published `f3c3555` epoch's unadapted Identity IO failure is retained in
[the original failure log](results/head-identity-io-original-failure.log).
Independent TotalResult tests verify captured results, trailing applications,
neutral-Force Fold fallback, neutral projection and overapplied Return, with two
inert reloads at every raw cut and separate writer/reader processes. A depth-1,000
case checks charged final readback and absence of premature result/certificate.
The descriptor-only adapter retains `machine_resave` and every original assertion;
its separate O2 results are pinned in [the diagnosis](results/identity-observer.json).

The original broad sanitizer batch also fails: successful head delivery discards
decoded empty-frame scratch. `eval_frame_cleanup.patch` frees that scratch on
accepted/error returns and retains it on return 2 for charged ordinary fallback.
This is a separate corrective prototype; the published files and original seven
failed commands remain unchanged. `head_cleanup_test.c` checks success, error,
invalid return and fallback without adding a codec descriptor or schema.
The [original report](results/sanitizers-original.json) retains all seven exits
and their individual original logs; the separate
[corrective report](results/sanitizers-corrective.json) passes all 45 commands
with ASan, UBSan and leak detection enabled. The
[O2 artifact report](results/artifacts-corrective.json) passes transport, semantic,
history and all seven checkpoints, while strict public partitions still fail.
Their [corrective table](results/corrective-partitions.tsv) is byte-identical to
the published-head table, preserving all three reload failures.

Prepare a fresh private corrective overlay and build its independent tests:

```bash
bash src/prototype/performance_verification/overlay.sh /tmp/performance-corrective
make -j2 -f src/prototype/performance_verification/build.mk \
OVERLAY=/tmp/performance-corrective BUILD=/tmp/performance-corrective/build \
/tmp/performance-corrective/build/performance_total_result_test \
/tmp/performance-corrective/build/performance_head_cleanup_test
python3 -B src/prototype/performance_verification/total_result_cuts.py \
/tmp/performance-corrective/build/performance_total_result_test /tmp/total-result-cuts
/tmp/performance-corrective/build/performance_head_cleanup_test
```

The five fixtures have 279 baseline and 70 published-head cuts. The runner pins
binary/test hashes and validates every writer/reader exit and exact raw total;
these are correctness/count records, not wall/RSS measurements. Full O2,
artifact/checkpoint and sanitizer results qualify each implementation separately.
Known public split-fuel reload failures remain failed integration needs.

The corrective O2 tests reproduce all 70 published-head TotalResult machine
images and all nine Force images byte for byte, with identical totals and
readback output. The depth-1,000 TotalResult budget remains 3,014 transitions.
The [two-pair check](results/tree-corrective.json) remains 1,170,277 transitions;
its completed 109,937-byte ordinary image matches baseline/direct/published head.
These results establish cleanup correctness within this pinned epoch, not timing,
current-Main integration or completion of the full performance Goal.
The [terminal O2 acceptance report](results/acceptance-corrective.json) audits
all 384 recipe exits, with both explicit adapters. Publication scope, remaining
failures and integration needs are recorded in [the frozen handoff](HANDOFF.md).
