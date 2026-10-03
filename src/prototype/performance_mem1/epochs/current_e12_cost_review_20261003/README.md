# MEM1 v3 Cost Interpretation Addendum

Date: 2026-10-03. The original 109-file measurement epoch remains immutable at
`../current_e12_cost_20261003/manifest.sha256`, SHA256
`5497a58490ab8448f207efa9ad7ae8f04e9cd40674c4cb6d2409061103067d0c`.
All 36 raw samples, expected charged steps, 280 pins and output/metrics hashes
verify. Root independently confirms terminal0 at 15:49:08 UTC and releases the
exclusive slot. This addendum corrects interpretation only; it adds no sample.

The frozen `analysis.json` sentence that says the list launcher ranges overlap
is incorrect. Its raw launcher times are correct: baseline median 0.007159574s,
range 0.006873796–0.008052829s; v3 median 0.004687668s, range
0.004327305–0.005574897s. Those ranges are disjoint. GNU time rounds all six
list wall values to 0.00s, RSS ranges overlap, and process/collector startup
dominates. The observed launcher difference does not establish an evaluator
speed gain or a list memory gain.

The report table deliberately uses GNU time wall values, while the same raw
samples also record launcher wall values. Tree400 GNU medians are 5.33→4.92s;
launcher medians are 5.340210750→4.925981237s, with ranges
5.328517666–5.340467310s and 4.911348957–4.952229928s. Peak RSS medians are
1891928→1001764KiB, about 1847.6→978.3MiB. Both time measures and RSS have
disjoint three-sample ranges on this exact matched workload; charged steps are
183507626 in every sample. These process measurements include parsing,
synthesis, conversion, proof/helper/provider work and startup. They do not
attribute isolated evaluator time, semantic live bytes or accepted-only cost.

Imported LocalSorted launcher medians are 0.628923703→0.609386755s with disjoint
raw launcher ranges, while the GNU time ranges meet at 0.61s. Its RSS median
increases 492KiB, or 0.29%. Retain that small memory regression and the limited
three-repetition evidence; do not claim a general QuickSort speed or memory
improvement. No native systems were rerun and no universal ranking follows.

Original strict3, physical-frame observers, f3 seven sanitizer failures,
rejected v1 inline-state cleanup and the later v2 materialized-callback UAF
remain separate. Corrected E11/E12 and the v3 source/correctness freezes are
unchanged. This separate two-file addendum accompanies the original 109 files
for delegated task-branch publication; it is not accepted promotion, Main
integration or completion of the active performance Goal. Further measurements
need a new exclusive slot. Remaining memory/deletion work continues.
