# MEM1 v3: Matched Current E12 Costs

Exact current prototype producer, baseline `12026914` and v3 `0deb36a7`; only
`eval.c`/`eval.h` differ. All36 samples pass with exact expected charged steps.
Three fresh processes per variant/case, sequential; repetition2 reverses paired
order. Existing GNU time collector, warm filesystem, no cache flush.

| Case | Charged steps | Baseline wall median [range], s | V3 wall median [range], s | Baseline peak RSS median [range], KiB | V3 peak RSS median [range], KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| list | 1915 | 0.00 [0.00–0.00] | 0.00 [0.00–0.00] | 2524 [2484–2660] | 2580 [2500–2748] |
| imported-local-sorted | 761848 | 0.62 [0.61–0.64] | 0.60 [0.60–0.61] | 169612 [169608–169920] | 170104 [169944–170320] |
| trees4 | 2318248 | 0.08 [0.08–0.08] | 0.07 [0.07–0.08] | 34740 [34368–34776] | 21216 [21056–21360] |
| trees16 | 7608511 | 0.24 [0.23–0.24] | 0.23 [0.21–0.23] | 91592 [91168–91620] | 50900 [50520–50948] |
| trees64 | 29780510 | 0.83 [0.82–0.83] | 0.74 [0.73–0.75] | 316944 [316472–317052] | 169984 [169928–170280] |
| trees400 | 183507626 | 5.33 [5.32–5.33] | 4.92 [4.90–4.95] | 1891928 [1891756–1892132] | 1001764 [1001732–1001824] |

Tree400 wall median5.33→4.92s (−7.69%); peak RSS1891928→1001764KiB
(−47.05%, about1847.6→978.3MiB). Both three-sample ranges are disjoint.
Tree64 also has disjoint time/RSS ranges. Tree4/16 time ranges overlap and
are close to GNU time's0.01s resolution; their RSS reductions are observed.
QuickSort timing overlaps at0.61s and its RSS median increases492KiB (+0.29%).
List GNU wall is0.00s for all runs; launcher/startup dominates and RSS overlaps.
Full three-repetition values, including launcher wall, are in `analysis.json`.

This is actual process peak RSS/time for these concrete AP source/proof/helper
workloads. Includes parsing, synthesis, conversion, source-provider import and
process/wrapper startup. RSS is GNU time's wrapped-command maximum, not a sum
of simultaneously resident processes or semantic live bytes. Allocator/kernel
effects are included. No native systems were rerun; their different proof/helper
work prevents a universal ranking. These prototype costs do not establish
accepted-only values. Selected-cut capacity/cumulative allocation findings
remain separate from these actual measurements; new WHNF aligned overhead
and the small-workload cost remain visible.

Grant `MEM1V3-E12-20261003T154600Z-600`:15:46–15:56UTC. Actual launch
15:48:26.754134UTC; remaining453.245866s, derived maximum451s. The original
configuration `a3335c5e` differs from `f51097b9` only in maximum_seconds.
Collector terminates0 at15:49:08.050690UTC. All280 pins and every output/metrics
hash verify. All36 expected counts agree; zero failed, timeout, incomplete or
censored samples. Terminal stopped-child notice precedes this analysis.

Original strict3, Core/Identity observers, f3 seven sanitizer failures, rejected
MEM1v1 inline-state and later v2 materialized-callback UAF remain separately
preserved. E11/E12 corrected qualification remains frozen without MEM1. V3
and its current E12 pair pass callback, fuel, raw cuts, step0 and split-image
controls; public52 images/full verdict-fuel TSV equal the matched parent.
No new full-suite, accepted promotion, Main merge or Goal-closure claim.

Tree peak remains about978MiB. Remaining owned memory/deletion work is active;
generic argument/environment recycling is not justified by reachability guesses.
Further profiling/heavy correctness and any new measurements follow the
coordinator release/slot rules. Published epoch bytes remain immutable.
