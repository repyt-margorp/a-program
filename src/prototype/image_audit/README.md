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
`nonidentity-trial` is a measurement experiment that omits identity reduction
roots from an optional snapshot. It is not an adopted retention policy, proof
erasure, or a claim that normality records lack meaning. Production code is linked
unchanged. `--load` uses a diagnostic-only 3M reader allowance.

## Plan

- [x] Compile with warnings as errors and measure source/save/load cycles.
- [x] Reproduce temporary Ref duplication and check restored pointer sharing.
- [x] Pass O2 and ASan/UBSan self-test and small source/load/save controls.
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
