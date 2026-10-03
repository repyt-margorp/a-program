# Focused cross-checker qualification

These separate prototype helpers prepare the first two pairs of concrete tree
equalities from the unchanged, hash-pinned official sources. They retain each
system's helper declarations and proof encoding. False Boolean/tree controls
change one expected endpoint. They are corresponding computations, not identical
kernel workloads or universal tree proofs. The published performance prototype
and its original Core failure reports are unchanged.

Run from the repository root, with fresh output directories:

```bash
python3 -B src/prototype/performance/fetch_trees.py /tmp/checker-official
python3 -B src/prototype/performance_cross/prepare.py /tmp/checker-official /tmp/checker-controls
python3 -B src/prototype/performance_cross/check.py /tmp/checker-controls /tmp/checker-commands.json /tmp/checker-results
```

The commands JSON is keyed by `bend`, `lean`, `agda`, and `v`; each entry has an
`argv` array with `{source}` as the source argument, and optional `env` overrides.
Omitted systems are explicitly skipped. Supply absolute tool paths. For Bend,
use Bun plus the pinned CLI and `--check-only`, with `BEND_NO_TELEMETRY=1`; this
does not check the separate BendTT `--verdict` path. For Lean, pass the source
to `lean`. For Agda, use `--no-libraries --ignore-interfaces` and the release's
`Agda_datadir`. For Rocq, use `rocq compile` and the relocated library settings
required by the chosen distribution. Pin source, tool, runtime/library and
command hashes before relying on results.

`checks.json` records exact commands, source hashes, exits and qualification.
False controls require the checker's conversion-error text; an unavailable
tool, timeout or signal is a failure. This helper collects no wall/RSS metrics
and proves only these focused controls. Full workload checking, broad AP gates,
BendTT admission and comparative measurements have separate evidence.

Fresh qualification at task-branch implementation epoch `f3c3555`: Bend
2.0.34/Bun 1.4.2, Lean 4.34.0, Agda 2.7.0.1 and Rocq 9.0.1 with OCaml
4.14.2/Stdlib 9.0.0 pass all 12 controls. Private tool/source pins and recorded
results are in [results/qualification.json](results/qualification.json).
Rocq uses native binaries/libraries extracted from verified OCI layers, with
`ROCQLIB`, `OCAMLPATH` and `OCAMLFIND_CONF` relocation; no container daemon is
used. The incompatible Debian binary and intermediate relocation failures are
preserved as setup failures. They are distinct from the unchanged original AP
acceptance/observer failures in the published performance bundle.

The private BendTT kernel built from pinned `bendtt.lean` now accepts the emitted
two-pair positive source and rejects separately changed Boolean and tree endpoints
with proof errors. The pinned CLI `--verdict` also passes; its kernel path uses
the exact private `BENDTT` binary. See
[results/bendtt-prefix.json](results/bendtt-prefix.json). Frontend-only qualification
remains separately identified.

All four unchanged official full sources at revision
`7d24b8d0235cb9781140512c0f163c48ea84a719` pass their recorded checker commands:
400 pairs, 800 concrete equalities. Bend's `--verdict` uses the qualified BendTT
kernel. Full results are
[BendTT/Lean](results/full-bendtt-lean.json),
[Agda/Rocq](results/full-agda-rocq.json), and
[Bend frontend](results/full-bend-frontend.json).
The corresponding AP port also reaches DONE on the reviewed E1/current-producer
joint snapshot and ordinary completed-image reload; see
[AP full](results/ap-e1-full.json) and
[AP endpoint controls](results/ap-e1-controls.json).
Its source run charges 183,507,626 steps; ordinary reload charges 183,501,946.
These records are correctness and charged-step observations. Helper declarations
and proof encodings differ across systems. Later qualified E6 measurements use
the current Job epoch and an exclusive Merge grant; see the separate
[E6 measurement report](results/e6-exclusive/README.md). They do not compare
universal theorems or identical kernel work.

The next measurement baseline keeps the canonical family/Surface producer and
qualified Job epoch fixed, while comparing the ordinary evaluator with the
captured-head/direct-IADT evaluator plus cleanup. Source manifests will identify
the changed evaluator files and unchanged owner files. Run corresponding AP
endpoint controls on both binaries before requesting the slot. Retain charged
steps and stored-reference/Job-byte censuses separately from wall time and peak
RSS; the earlier four-variant E1 census is historical evidence, not an E6 timing.

Propose a bounded slot only after E4/E6 joint gates are terminal and baseline
alignment is verified. During the slot, use fresh processes and private output,
alternate AP variant order across repetitions, and record each exact command,
exit, tool/source hash, wall time and peak RSS. Native full-source measurements
use the already qualified pinned checker commands, with BendTT admission
separate from frontend-only checking. Report differing helper/proof encodings
and instrumentation boundaries alongside results. A timeout or failed check
remains a failed measurement; it is never a completed speed comparison.

The E6 grant is now terminal: all 30 samples pass, none censored. Matched AP tree
medians are 17.23s ordinary and 5.40s head, with 36.3% lower wrapped-command peak
RSS. Imported LocalSorted has identical 0.63s medians; List is below GNU time's
reported resolution. Exact sample values, pins, startup/proof/helper boundaries
and historical setup failures are retained in the separate report and bundle.
