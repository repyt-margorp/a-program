# Qualified E6 checker measurements

All 30 pinned fresh-process samples pass under Merge grant
`e6-shared-cost-gnu-time-20261003-0833`. The runner terminates at
2026-10-03 08:36:55.938837 UTC, before the 08:52:05 absolute stop. No sample is
incomplete, timed out or censored. No additional workload or build ran.

The matched AP pair holds canonical producer `898463e`, qualified Job E6
`b2d66682be75c21a3c2a8c5138db9d3f0c5e0eb2`, family and Surface fixed. Only
`computation.c`, `eval.c`, `eval.h` and `iadt.c` differ between the ordinary
evaluator and published head/direct-IADT/cleanup implementation `0539051`.
Both variants passed independent original/adapted observers, endpoint controls
and corresponding List/imported LocalSorted inputs before measurement.
E6 joint runtime evidence remains separately frozen in its 1,939-entry manifest.

Each row is three fresh processes. Values are GNU time medians, with the
observed range in parentheses; RSS is KiB. The machine has 22 logical CPUs,
Intel Core Ultra 7 155H. AP uses strict C11 O2. AP order alternates between
repetitions. Native source copies are fresh; filesystem caches remain warm
after correctness qualification. Exact commands, environment, metrics and
launcher times remain in `measurements.json` and individual private sample logs.

| AP input | Variant | Wall seconds | Peak RSS KiB | Charged steps |
| --- | --- | --- | --- | --- |
| List | Ordinary | 0.00 | 2,680 (2,632–2,752) | 1,921 |
| List | Head | 0.00 | 2,516 (2,512–2,560) | 1,915 |
| Imported LocalSorted | Ordinary | 0.63 (0.61–0.63) | 169,816 (169,680–169,964) | 766,477 |
| Imported LocalSorted | Head | 0.63 (0.61–0.63) | 170,080 (169,912–170,176) | 761,848 |
| Tree, 400 pairs | Ordinary | 17.23 (17.20–17.25) | 2,971,260 (2,971,092–2,971,852) | 227,277,774 |
| Tree, 400 pairs | Head | 5.40 (5.38–5.44) | 1,891,724 (1,891,684–1,892,004) | 183,507,626 |

On this matched tree run the combined evaluator change reduces median wall
time by 68.7%, peak RSS by 36.3% and charged steps by 19.3%. The measured
ordinary/head wall ratio is 3.19. Imported LocalSorted shows no median wall-time
improvement. Its RSS ranges overlap; the median increase is 264 KiB. Its
4,629-step reduction is a count observation. List rounds to GNU time's
hundredth-second resolution; no List speed ratio is supported. Launcher times
are retained but include wrapper startup.

The same qualified concrete tree obligations have these native results:

| Checker command | Wall seconds | Peak RSS KiB |
| --- | --- | --- |
| AP ordinary source checker | 17.23 (17.20–17.25) | 2,971,260 (2,971,092–2,971,852) |
| AP head source checker | 5.40 (5.38–5.44) | 1,891,724 (1,891,684–1,892,004) |
| Bend CLI with BendTT verdict | 2.78 (2.77–2.80) | 99,280 (97,120–99,668) |
| Lean | 1.74 (1.73–1.74) | 541,632 (539,956–542,360) |
| Agda | 6.86 (6.82–6.90) | 121,012 (120,416–121,144) |
| Rocq | 1.73 (1.73–1.74) | 397,132 (395,312–397,316) |

Native sources are unchanged official revision
`7d24b8d0235cb9781140512c0f163c48ea84a719`: 400 pairs/800 concrete equalities.
Bend 2.0.34/Bun 1.4.2 uses the independently qualified private BendTT kernel;
Lean 4.34.0, Agda 2.7.0.1 and Rocq 9.0.1 use the previously qualified pinned
commands and libraries. Each checker has positive and false-endpoint controls.
The AP port has corresponding obligations, with different helper declarations
and proof encodings. These results compare the recorded checker invocations;
they do not establish identical kernel work, a universal tree theorem or native
sorting/execution speed. Compiler distributions and optimization differ.

GNU time `%e/%U/%S/%M/%x` wraps timeout plus the exact checker command. Wall
includes process startup and each command's parsing/checking work; fresh source
copying precedes the timed invocation. `%M` is wrapped-command peak RSS, not a
sum of simultaneously resident processes or an owner/arena byte counter. AP
source checking includes parsing, synthesis and conversion. Bend includes its
CLI and verdict path. This pair measures head/direct-IADT/cleanup together;
it does not isolate individual time/RSS contributions or a Job deletion.
The earlier four-variant owner-reference/Job-byte census remains separate.

Applied code changes against the matched ordinary evaluator are:

| Applied file | Added lines | Deleted lines | Purpose |
| --- | --- | --- | --- |
| `src/computation.c` | 50 | 4 | Deliver owner-local captured heads through existing continuations |
| `src/eval.c` | 31 | 1 | Consume captured closure/spine and clean detached readback scratch |
| `src/eval.h` | 13 | 0 | Declare the head-delivery interface |
| `src/iadt.c` | 43 | 7 | Apply branch fields directly through existing closures |

The four-file runtime delta is +137/-12, net +125 lines. Bounded protocol code
replaces repeated runtime prefix copying/readback and administrative branch
construction; this is a work reduction, with more source code. Core/Identity
observer migrations remain explicit test adapters; independent semantic/fuel/
persistence controls justify them without erasing their original failures.
Per-file implementation/test/tool/documentation payload deltas are retained in
`applied-runtime-and-test-delta.tsv` and the original frozen publication records.

The initial attempt collected zero samples because `/usr/bin/time` was absent.
Its partial report/trace are retained under
`/tmp/ap-performance-exclusive-e6-shared-cost-20261003-0812`. An unexecuted
wait4 proposal remains historical; it supplies no measurement. The revised
collector changes only the original runner's GNU time path, using Merge's
private Debian trixie `time` 1.9-0.2 amd64 package. The tool's version banner
says `GNU Time UNKNOWN`; verified package metadata supplies its distro version.
Tool/package/libc/loader/timeout/Python pins and the trivial smoke are recorded
in `gnu-time-instrumentation-proposal.json`.

Original Core/Identity observer failures, all seven original sanitizer failures
and the three strict reload failures remain failed historical records in their
separate frozen evidence. This report grants no test waiver, accepted promotion,
Main integration or full Goal completion. E7/E8 qualification waits for Merge's
heavy-work release. Merge subsequently releases correctness for distinct E7
then E8 qualification, while deferring the audit cost phase without launching it.
No further wall/RSS run is authorized during correctness overlap.
