# Prototype test-suite reduction

Base: `09ee7750604f1de2264dd4dad5ab5f174583c655`, branch
`parallel/test-suite-20261003`. The sole work list is the
[worker SOAP plan](../../../doc/2026-10-03-TEST-SUITE-DEDUPLICATION-WORKER-PLAN.md).
These patches apply after the inherited AP image policy and committed Surface
migration. Accepted tests, compiler code and build rules stay untouched.

- `compatibility.sh.patch`: admit each fixed source once, then check every
  explicit result pair. Keep all 43 sources, 37 CASES result pairs, five
  rejection witnesses and the entire later image/import/proof section.
- `quick_result.sh.patch`: remove a second ordinary complete-image save/load.
  Both original images have identical bytes. Completed/pending/rejected,
  normalized-index, wrong-motive and optional-module linkage checks survive.
  Accepted retention policy and inherited AP policy retirement earn no savings.

Create a fresh private assembly with the existing producer conventions:

```sh
bash src/prototype/test_suite/overlay.sh /tmp/ap-suite-review original
make -j1 -f src/prototype/performance_verification/build.mk \
	OVERLAY=/tmp/ap-suite-review BUILD=/tmp/ap-suite-review/build \
	CHECKPOINT_TESTS=/tmp/ap-suite-review/checkpoint_tests/ \
	ARTIFACT_TESTS=/tmp/ap-suite-review/artifact_tests/ \
	check-quick-result check-source-compatibility
git apply --check --unsafe-paths --directory=/tmp/ap-suite-review/tests \
	src/prototype/test_suite/quick_result.sh.patch \
	src/prototype/test_suite/compatibility.sh.patch
git apply --unsafe-paths --directory=/tmp/ap-suite-review/tests \
	src/prototype/test_suite/quick_result.sh.patch \
	src/prototype/test_suite/compatibility.sh.patch
# Repeat the same Make command on the same binary and unchanged inputs.
```

The default overlay mode applies the reductions directly. Never apply these
patches to accepted `tests/`. The overlay creates a new directory and copies
source symlinks before patching. Surface migration checks both input/output
hashes; patch context fails closed on producer drift.

See [verification.json](verification.json) for scoped results and the frozen
raw-evidence pointer. The comparison records binary invocations, options,
actual input/output hashes, exits, status/fuel and diagnostics; temporary paths
and the proved duplicate image alias are normalized. Full suite cost and
dynamic inventory remain separate Issue59 work. Wall/RSS is unmeasured.
