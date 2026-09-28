# Image Growth Diagnostic

## Problem List

1. Attribute `.a` growth without changing production serialization or acceptance.

## Subjective (User)

Inspect duplicate retained data before raising the reader allowance (2026-09-28).

## Objective (Code)

`audit.c` uses GNU linker wrapping around the actual codecs, records byte ranges,
and inspects the emitted Core table. It counts literal `(tag, child IDs)` duplicate
records, not alpha equivalence. External substitution calls are counted only when
their destination is the main program graph. Internal calls in `eval.c` are not
intercepted. The driver intentionally uses assertions on its own generated input;
it is a local diagnostic, not a general image-inspection CLI.

## Assessment

See [the audit](../../../doc/2026-09-28-RETAINED-IMAGE-GROWTH-AUDIT.md).
The active [AP1-AP3 plan](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md)
also requires preserving materialized semantic content for downstream consumers;
size stability alone is not completion of the artifact refactor.
`nonidentity-trial` is a measurement experiment that omits identity reduction
roots from an optional snapshot. It is not an adopted retention policy, proof
erasure, or a claim that normality records lack meaning. Production code is linked
unchanged. `--load` uses a diagnostic-only 3M reader allowance.

## Plan

- [x] Compile with warnings as errors and measure source/save/load cycles.
- [x] Reproduce temporary Ref duplication and check restored pointer sharing.
- [x] Pass O2 and ASan/UBSan self-test and small source/load/save controls.
- [x] Add the CLI fuel/byte regression, including mandatory zero-step rewrites.
- [ ] Repair owners separately, after choosing the retention contract.

From the repository root, build against accepted code:

```sh
make -f src/prototype/image_audit/build.mk BUILD=/tmp/image-audit-build
/tmp/image-audit-build/image_audit --self-test
mkdir -p /tmp/image-audit-first /tmp/image-audit-second
/tmp/image-audit-build/image_audit examples/09_list_induction.p /tmp/image-audit-first 1000000
/tmp/image-audit-build/image_audit --load /tmp/image-audit-first/1000000-retained.a /tmp/image-audit-second 0 1000000
```

For the previously verified combined prototype, build with
`COMPILER=/tmp/a-program-image-limit/src` and a separate `BUILD` directory.
Generate the large input using the existing provider, not a modified algorithm:

```sh
bash src/prototype/finite_sorting/provider.sh src/prototype/finite_sorting/quick.p tests/fixtures/generic_sorted/boolean-order.p src/prototype/finite_sorting/cases.p > /tmp/sorting-image-audit.p
/tmp/image-audit-build/image_audit /tmp/sorting-image-audit.p /tmp/image-audit-first 0 1000 100000 1000000 10000000
```

Each step argument is a cumulative per-invocation budget. Inspect reported status:
10M was sufficient for the combined prototype, but not the accepted large case.
Do not compile with `-DNDEBUG`; the diagnostic relies on active assertions.

## Fuel Regression

`fuel_curve.sh` exercises the actual CLI and always includes fuel zero. It writes
TSV size/digest/work measurements, checks three inert rewrites after each source
budget and each completed load/Solve/save generation, and returns nonzero on
inert drift or accumulated history. It adds no reader-limit option. For example:

```sh
bash src/prototype/image_audit/fuel_curve.sh build/pointer/pointer-check examples/09_list_induction.p /tmp/fuel-ordinary ordinary
bash src/prototype/image_audit/fuel_curve.sh build/pointer/pointer-check examples/09_list_induction.p /tmp/fuel-retained retained
bash src/prototype/image_audit/fuel_curve.sh build/pointer/pointer-check src/prototype/image_audit/host.p /tmp/fuel-host ordinary
IMAGE_AUDIT_EXPECT=1 bash src/prototype/image_audit/fuel_curve.sh build/pointer/pointer-check src/prototype/image_limit/invalid.p /tmp/fuel-rejected ordinary
```

The output directory must not already exist. Optional trailing arguments choose
source fuel samples (default: 0, 1, 100, 100000; zero is always included). The last
must finish successfully, or reject when `IMAGE_AUDIT_EXPECT=1` is explicitly set.
Pending, unsupported or unexpectedly rejected final inputs do not pass. Source
and load invocations must print only their status, never execute the host marker.
`IMAGE_AUDIT_TIMEOUT` sets the per-process deadline in seconds (default: 180).
Use the same-format mode throughout a run. Cross-format migration is a separate
test, not an exception that suppresses zero-step failures. Recorded steps count
Solve transitions, not file decoding, allocation, elapsed time or total CPU work.
The test never uses `--nf`, `--whnf` or `--run` to add hidden evaluation demand.
It tests image stability and status, not semantic equality of two completed roots;
the existing typed result comparator remains necessary for that separate check.

Fresh `e716232` control: ordinary List, host and rejected-input matrices pass.
The retained List matrix correctly **fails**: each completed load/Solve/save adds
444 bytes; its zero-step rows stay byte-identical. Both host/rejection matrices
also pass with retention enabled. See the linked audit for the larger prototype
case and the distinction between size growth and same-size byte drift. These
prototype tests are not yet registered in the accepted regression suite.
