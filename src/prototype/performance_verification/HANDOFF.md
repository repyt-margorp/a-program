# Corrective Captured-Head Handoff

Parent: `f3c35558ca8ce2b1c5fac4c7ce3eba63da75b858`.
Task branch: `parallel/performance-20261003`.
Chosen commit message: `Free decoded frame scratch after captured-head delivery`.
Status: terminal local verification; corrective publication set frozen for review.

The separate runtime patch changes only private overlay `eval.c` (+5/-1 lines).
Decoded empty materialization frames can own a readback arena even with no
output. Successful/error head delivery detaches those frames, so it must destroy
their materialization scratch. Return 2 keeps the scratch for charged ordinary
fallback. No codec, wire field, descriptor, fuel transition or owner authority
changes. The frozen published `performance/` files remain unchanged.

`eval_frame_cleanup.patch` SHA-256:
`e0e01ede03cc205b632509731f1ce6b155de318240f4bc8a7955d629796c167e`.
The separate Identity IO observer adapter recognizes the old/new descriptor;
every assertion and `machine_resave` remain unchanged. Its SHA-256 is
`672ab8140a9afb064418596ec2bd4d2552aed40fc4708ffb263b5fdb9833143f`.
The original Core observers, Identity IO assertion and seven sanitizer failures
remain explicit failed records of the published epoch.

| Gate | Result |
| --- | --- |
| Corrective full strict O2 acceptance | All 384 recorded recipes return zero, with explicit adapters |
| Corrective ASan/UBSan/leaks | All 45 commands pass |
| Head success/error/invalid return/return-2 fallback | Pass; scratch lifecycle checked |
| TotalResult raw cuts and fresh processes | 279 baseline / 70 head cuts pass |
| Corrective O2 raw images | All 70 TotalResult + 9 Force images unchanged |
| Charged TotalResult readback | Unchanged 3,014 transitions; budget 100 still pending |
| O2 transport/semantic/history and seven checkpoints | Pass with explicit test adapters |
| Strict public partitions | Fail: unchanged 1,000+1,000, 1,600+1,600 and 1,915+0 reloads |
| Two-pair trees | Unchanged 1,170,277 transitions and 109,937-byte completed image |

The original unadapted Identity IO acceptance/history gates remain failed;
adapter results are separate. The original sanitizer report remains 7/44 failed.
Baseline public partitions also fail at the two partial splits and completion+0
(1,921+0). These are integration needs, not expected-failure passes.

The publication set contains this `performance_verification/` subtree and
the owning Goal plan only. Private `performance_cross/` preparation is excluded.
The exact frozen files/hashes are in
`/tmp/ap-performance-corrective-f3c3555-20261003-publication.sha256`, with a
matching `.tar` archive. Shared Git metadata is read-only in this sandbox;
Core can perform the authorized task-branch publication without a sandbox bypass.
Core reviews combined correctness before any Main integration or promotion.

Align these pinned inputs/results with Main `341261d`'s disposable family/
Surface producer and Core's hash-verified frozen Job Epoch 1. The combined
[source alignment](results/combined-source-alignment.json) verifies all 128
records, with 12 owner/family/Surface differences and matching evaluator/head/
cleanup source hashes. Core reports joint Job/head focused gates terminal, 11/11
zero, including Source checkpoint and all 70 separate-process TotalResult cuts.
Its paired DONE imported QuickSort census is live; broader combined gates remain
open. This old producer does not verify
that combined implementation. Route the three public reload
failures and any owner/query/frontier findings through Core. Attribute removed
owner storage separately from traversal counts and eventual time/RSS. C is the
downstream `.a` plus LinkerScript bridge; this correction adds no C-driven A
Program features or competing Job/Evidence implementation.

The full performance Goal remains active: concentrated joint verification,
qualified full cross-system workloads/BendTT, cost attribution and actual
comparative timings remain open. No wall/RSS comparison or exclusive timing slot
has been requested or granted for this epoch.
