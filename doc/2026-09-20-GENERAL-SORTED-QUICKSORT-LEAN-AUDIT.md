# Verification addendum — 2026-09-24

Tracking issue: https://github.com/repyt-margorp/a-program/issues/31

## Classification and latest-revision verification

Confirmed source-synthesis limitation, not a soundness report and not a claim that all Sorted proofs fail.

Fresh GitHub clone on 2026-09-24: `a72cda371109fdbf84d747456ed0aeb09af2391e` on `rewrite/pointer-core-hott`. This is exactly the revision pinned in the original audit. The original ten A Program cases were extracted verbatim, rebuilt and rerun with a 10,000,000-step ceiling:

| Case | Result |
|---|---|
| box-fixed-cast.p | rejected, 1,287 |
| box-fixed.p | done, 1,896 |
| decision-no-check.p | done, 2,302 |
| decision-computed.p | done, 3,525 |
| decision-graph-general.p | rejected, 11,479 |
| comparator-bridge-rejected.p | rejected, 25,570 |
| comparator-bridge-computed.p | done, 26,591 |
| generic-conditional.p | done, 598,624 |
| generic-quick-original.p | rejected, 587,424 |
| generic-quick-projection-outside.p | rejected, 582,624 |

All ten reproduce the original findings. Expected rejection is not a successful proof.
The existing **Nat-specific** `sort-quick-property.p` additionally passes at 456,336 steps, using the complete order/append provider assembly from `tests/sort_insertion.sh`. Supplying only the algorithm provider misses imports and is not a valid regression test.

Lean was **not rerun** in this submission pass. Its execution results are historical evidence recorded in the attached original audit, not a fresh measurement. No logging-instrumented checker was rerun; the original trace is preserved as historical diagnosis.

## Problem

For `g : Graph A le x y b` and a certificate of type
`(x:A) -> (y:A) -> Decision A R x y (le x y)`, the same elimination body can fit both:

- `Decision A R x y b`, the result-index motive needed by the caller;
- `Decision A R x y (le x y)`, a recomputed-index motive.

Both specialize to the same branch type at the constructor image. Current synthesis chooses the latter. The final `::` correctly rejects the desired former type because it is a post-synthesis assertion, not expected-type input.

The full generic theorem with `partition_spec` as an extra parameter passes, but that leaves the essential partition proof assumed. This is not completion of arbitrary-type/relation Quicksort sortedness.

## Requested resolution / acceptance criteria

1. Provide a reviewed explicit motive or scoped transport encoding, or improve deterministic candidate selection with ordinary evidence validation.
2. Keep `::` post-synthesis; do not silently turn it into Lean-style expected-type guidance.
3. Do not globally identify an opaque comparison result with a graph index by conversion.
4. Test both candidate motives, true/false outputs, wrong comparator, wrong index and scope escape; include source/image rechecking.
5. Complete and rerun the generic proof **without** a `partition_spec` assumption.
6. Preserve the already working Nat theorem.

Relevant current files: `synthesis.c` (`match_candidate_type`, `match_candidate_step`), `evidence.c` (`pg_prove_pattern_type`), and the existing checked index-transport path. The full audit includes exact fixtures and pinned source links.

This is a follow-up beyond the completed Nat scope of #29/#30, related to the broader concern in #13. It does not reopen the repaired graph-name-collision or LE-inversion bugs.

## Historical audit follows

The supplied audit below is preserved as a dated record. Its proposals are not implemented by this documentation PR. Statements about prior execution or publication refer to the original audit date; the scope of fresh verification is given above.

---

# General Sorted for Quicksort: A Program / Lean execution audit

Date: 2026-09-20 (UTC)  
Status: investigation complete; implementation changes not proposed for acceptance  
Suggested repository location: `doc/2026-09-20-GENERAL-SORTED-QUICKSORT-LEAN-AUDIT.md`

## Executive conclusion

**Lean 4.34.0 checks a complete generic Quicksort sortedness theorem, and the relevant generic comparator-evidence bridge. The tested A Program proof does not check at the audited revision. Its isolated blocker is source-level dependent-match motive selection: the inferred result retains the comparator expression instead of depending on the graph's result index.**

The A Program result is a reproducible limitation of the tested proof spelling and inference path. It is not a proof that the theorem is inexpressible in every spelling, and it is not evidence of an unsound kernel. A Program deliberately treats `::` as a post-synthesis check; it cannot serve as the expected-type guidance used by the successful Lean proof.

An earlier explanation that attributed the original rejection simply to missing equality transport was too broad. A naive fixed-output `cases` proof also fails in Lean. A Program already contains checked Identity transport, and a smaller `Box` example succeeds after removing a premature branch-local assertion. The faithful generic `Decision` example is what establishes the remaining difference.

All 15 final reproducibility cases matched their expected acceptance or rejection. No compiler acceptance rule was changed. The full sources and raw results are embedded below, so this Markdown can be registered as a self-contained audit record.

## 1. Scope and versions

| Component | Audited version |
| --- | --- |
| A Program repository | <https://github.com/repyt-margorp/a-program> |
| Branch cloned | `rewrite/pointer-core-hott` |
| A Program commit | `a72cda371109fdbf84d747456ed0aeb09af2391e` |
| A Program executable | `src/prototype/pointer/.build/pointer-check` |
| Build | `make -f src/prototype/pointer/Makefile pointer-check` |
| Host | Linux x86_64; GCC 13.3.0 |
| Lean | Official Linux release binary, `4.34.0` |
| Lean commit reported by binary | `293d5d0c0c3f3dded4688b3ccd6a33939ac5102b` |
| Lean libraries | Bundled `Std`; no mathlib dependency |

Lean was actually executed; this is not a prediction that a proof should elaborate. Its release archive was obtained from:

<https://github.com/leanprover/lean4/releases/download/v4.34.0/lean-4.34.0-linux.tar.zst>

Archive size: `580367391` bytes. SHA-256:

```text
caaa98356098c85dc0fcbbd28e1ec66f39eb6551829972b752ff20e1286b646b
```

“Current” in this audit means the pinned A Program commit and Lean binary above, not future branch heads. The A Program checker was rebuilt from the clone. A separate diagnostic executable was compiled from a logging-only copy of `synthesis.c`; ordinary acceptance results use the original checker.

## 2. What was proved in Lean

`QuickSort.lean` defines an executable, fuel-free quicksort on arbitrary `A : Type u`. Recursion is justified by decreasing list length. The lower partition filters for `cmp x pivot = true`; the upper partition filters for `false`.

The checked conclusion is:

```lean
List.Pairwise R (quicksort cmp xs)
```

It is universally quantified over the element type, relation, comparator and input list, with these assumptions:

- `R : A → A → Prop` is transitive.
- A true comparator result provides `R x y`.
- A false comparator result provides `R y x`.

The additional theorem `quicksort_sorted_from_decision` obtains both comparator laws from a certificate of type `OrderDecision R x y (cmp x y)`. There is no assumed partition-sortedness theorem: the partition bounds follow from filter membership and the comparator certificate. No reflexivity assumption is needed by this Lean proof; the audited A Program conditional proof also takes reflexivity, owing to its concatenation-helper structure.

`List.Pairwise R` requires each earlier element to relate to each later element. This corresponds to the all-tail-element meaning of the A Program `general_sorted`, rather than merely checking adjacent elements. The auxiliary membership theorem only proves that output elements came from the input; this audit does not establish permutation, multiplicity preservation, stability or complexity as formal theorems.

Actual output:

```text
'quicksort_sorted' depends on axioms: [propext, Quot.sound]
[0, 1, 1, 2]
'quicksort_sorted_from_decision' depends on axioms: [propext, Quot.sound]
```

There is no `sorry`, `admit` or `sorryAx` in the successful proof. It would nevertheless be incorrect to describe the full theorem as axiom-free: Lean reports `propext` and `Quot.sound`. The isolated graph/comparator bridges report no axioms. The full sortedness theorem uses a `Prop`-valued relation; the isolated `DecisionBridge.lean` additionally verifies the bridge with an arbitrary `Type v`-valued relation, which is closer to A Program's proof-data encoding.

**Cross-language scope:** this is a corresponding mathematical Quicksort implementation and proof strategy, not a literal translation of the A Program CBPV program, generated function graphs, `Acc` evidence or `SizedList` representation. Lean uses two filters; the A Program source partitions in one recursive traversal. No formal equivalence between those two implementations was proved. Thus the correct answer is “the strategy and corresponding generic theorem pass in Lean,” not “Lean checked the original `.p` artifact unchanged.”

## 3. Findings

### F1 — Premature fixed-index checking is a proof-script problem

The original failed comparator branch asserts a certificate indexed by `true`, although its immediate type is indexed by the open comparator application. These are not definitionally equal just because a graph witness exists nearby.

A tiny analogue in A Program reproduces the distinction:

- `box-fixed-cast.p`: branch-local `cert a :: Box Bool.true` is rejected.
- `box-fixed.p`: removing that local assertion and checking the whole elimination succeeds.

Lean's naive `cases g` with `g : Graph f x true` also fails:

```text
Dependent elimination failed: Failed to solve equation
  true = f x
```

Lean succeeds after either generalizing the output index, explicitly transporting along `graph_sound`, or using an explicit recursor motive. Therefore the first rejected branch alone is insufficient to diagnose a missing logical equality rule in A Program.

### F2 — Generic Decision elimination retains a recomputed index

The faithful minimal reproducer uses:

```text
Decision A R x y b
Graph A le x y b
cert : (x:A) -> (y:A) -> Decision A R x y (le x y)
```

`Graph` has one constructor whose output indices are `a` and `le x a`. Its elimination body is simply `cert x a`.

For the same body, the original A Program checker gives:

| Requested or inferred result | Result |
| --- | --- |
| No final assertion | Accepted, 2,302 steps |
| Final assertion `Decision A R x y (le x y)` | Accepted, 3,525 steps |
| Final assertion `Decision A R x y b` | Rejected, 11,479 steps |

Two valid motives explain the ambiguity. Suppressing parameters and the graph-witness argument:

```text
M_result(y,b)    = Decision A R x y b
M_recomputed(y,b)= Decision A R x y (le x y)
```

At the constructor image `(a, le x a)`, both instantiate to the branch type `Decision A R x a (le x a)`. But at a general graph index `(y,b)`, they are different types. The current synthesis path selects the recomputed-index result. Merely accepting the body does not supply the result-index certificate needed by the caller.

The Lean counterpart with a declared result type passes:

```lean
(x y : A) (b : Bool) (g : ComparisonGraph cmp x y b) : Decision R x y b := by
  cases g
  exact cert x y
```

An explicit recursor motive also passes. Both report no axioms. The explicit recursor example is marked `noncomputable` to avoid Lean's executable-code generation restriction on that recursor form; this does not add a logical axiom. The ordinary `cases` version is an ordinary `def`.

For a fair comparison, the unannotated Lean `match` attempt was also tested and rejected. Lean is not automatically inferring the desired theorem from every spelling. Its successful proof has expected-type or explicit-motive information that A Program's post-check assertion intentionally does not provide.

**Classification:** a demonstrated source-synthesis/motive-selection limitation for this proof interface, not a demonstrated failure of the logical theorem or kernel soundness. No claim is made that every possible explicit encoding in A Program must fail.

### F3 — The full A Program proof remains conditional or rejected

The full original generic proof attempt rejects at 587,424 steps. Moving the true/false proof projection outside the comparison match still rejects at 582,624 steps. This shows that the simple “remove the premature cast” repair that works for `Box` is not sufficient for this tested generic proof.

The generic proof with an explicit `partition_spec` parameter succeeds at 598,624 steps. That is a real checked implication, but the essential partition obligation remains a hypothesis in `quick_correct`; it must not be presented as an unconditional generic correctness proof.

The earlier Nat-specific universal proof was reported successful in the prior verification. It is not counted among the 15 rerun cases in this audit and does not discharge the arbitrary-type/relation obligation.

## 4. Source-level diagnosis and trace

All source locations below refer to A Program commit `a72cda371109fdbf84d747456ed0aeb09af2391e`.

| Location | Relevant behavior |
| --- | --- |
| `README.md`, lines 59–61 | Documents `::` as a post-synthesis check, never expected-type guidance. |
| `src/prototype/pointer/synthesis.c:5805`, `match_candidate_type` | Derives a candidate motive from a branch type via `pg_prove_pattern_type`. |
| `src/prototype/pointer/evidence.c:4750`, `pg_prove_pattern_type` | Reindexes/inverts a constructor pattern and checks the candidate by substituting back into the branch. |
| `src/prototype/pointer/evidence.c:4634`, `pattern_index_type` | Participates in checked abstraction of constructor index images. |
| `src/prototype/pointer/synthesis.c:5953`, `match_candidate_step` | Validates and commits a branch-derived candidate motive. |
| `src/prototype/pointer/synthesis.c:3740`, `expect_step`; `:3715`, `compare` | Checks an already synthesized classifier against the assertion. |
| `src/prototype/pointer/synthesis.c:3694`, `conversion_step` | Rejects when those classifiers are not convertible. |
| `src/prototype/pointer/synthesis.c:6402`, `match_index_step`; `:8092`, `index_transport_candidate`; `:8339`, `index_transport_step` | Existing checked index/Identity transport machinery. Its presence rules out the blanket diagnosis “transport is absent.” |

Pinned source links:

- [synthesis.c](https://github.com/repyt-margorp/a-program/blob/a72cda371109fdbf84d747456ed0aeb09af2391e/src/prototype/pointer/synthesis.c)
- [evidence.c](https://github.com/repyt-margorp/a-program/blob/a72cda371109fdbf84d747456ed0aeb09af2391e/src/prototype/pointer/evidence.c)
- [README](https://github.com/repyt-margorp/a-program/blob/a72cda371109fdbf84d747456ed0aeb09af2391e/README.md)
- [Existing graph-export/index-transport audit](https://github.com/repyt-margorp/a-program/blob/a72cda371109fdbf84d747456ed0aeb09af2391e/doc/2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md)

The existing transport audit also states that the source search is deliberately incomplete and that `::` must remain a post-check. The present finding is consistent with those constraints.

A logging-only checker copy records candidate dependence and the conversion failure for `decision-graph-general.p`:

```text
candidate term=g result=accepted binder_from_end[0]_independent=1 binder_from_end[1]_independent=1 binder_from_end[2]_independent=0
conversion DIFFERENT step=11476
rejected steps=11479
```

In this motive context the binders counted backwards are the graph witness, output `b`, and input `y`. The selected candidate depends on `y`, but not on `b` or the witness. Combined with the successful recomputed-index post-check, this identifies the local failure mechanism: a valid candidate was selected, but it is not the result-dependent motive needed by the proof. The post-check then correctly rejects the incompatible result type.

This trace establishes the local mechanism of the isolated reproducer. It does not prove that fixing this one synthesis case would automatically make every later step of the full Quicksort proof succeed. The full proof must be rerun after any repair.

The diagnostic generator is embedded in the appendix. It adds printing to a separate copy of `synthesis.c`, preserves the returned candidate, and preserves the rejection branch. No trusted evidence rule or acceptance condition is relaxed.

## 5. Recommended correction and acceptance criteria

Prefer a reviewed, explicit way to provide the dependent eliminator motive, or a checked source-level encoding of transport that produces the required result-index certificate. An alternative is an improved, deterministic candidate-selection rule whose result still passes ordinary evidence validation. Choosing between these is a language-design decision; it is not implemented by this audit.

Preserve the following boundaries:

1. Keep `::` as the documented post-synthesis check unless the project explicitly changes that policy. A Lean-style expected type cannot silently be substituted for the current A Program contract.
2. Do not make an opaque `le x y` definitionally equal to `b` globally. A graph/Identity witness must justify any transport, with its scope preserved.
3. Keep substitution-back validation and the normal kernel/evidence checking path. Do not accept a candidate merely because its type has the desired printed shape.
4. Add regression coverage for both motives, true and false results, a contradictory result index, and a certificate for a different comparator. Ensure the transport evidence cannot escape its scope. If restored artifacts are supported by the eventual change, verify the same evidence on that path.
5. Rerun the full generic proof without `partition_spec`. The positive acceptance criterion is a theorem that constructs that obligation from comparator evidence, not one that adds it as a new hypothesis.

The concrete current limitation is diagnosed; a general language repair and an unconditional A Program proof are outstanding work. No implementation fix, commit, push or issue publication was performed.

## 6. Reproduce the audit

Clone and pin A Program, install the pinned Lean release, and place this Markdown anywhere accessible. Run the extraction snippet below from the A Program repository root. It writes only beneath `src/prototype/`, in accordance with `AGENTS.md`.

```sh
git clone --branch rewrite/pointer-core-hott https://github.com/repyt-margorp/a-program.git
cd a-program
git checkout a72cda371109fdbf84d747456ed0aeb09af2391e
make -f src/prototype/pointer/Makefile pointer-check
```

Download the Lean archive from the URL in section 1, check its SHA-256, and extract it with `tar --no-same-owner --zstd -xf lean-4.34.0-linux.tar.zst`. Set `LEAN` to the absolute path of its `bin/lean` executable.

Extract the embedded files (replace the report path with the actual location):

```sh
python3 - /absolute/path/to/2026-09-20-GENERAL-SORTED-QUICKSORT-LEAN-AUDIT.md <<'PY'
from pathlib import Path
import re
import sys

report = Path(sys.argv[1]).read_text()
root = Path('src/prototype/pointer/experiments/lean-comparison-20260920')
root.mkdir(parents=True, exist_ok=True)
pattern = r'^### Embedded file: ([\w.-]+)\n\n```[^\n]*\n(.*?)^```$'
for name, body in re.findall(pattern, report, re.MULTILINE | re.DOTALL):
    (root / name).write_text(body)
    print(root / name)
PY

export LEAN=/absolute/path/to/lean-4.34.0-linux/bin/lean
python3 src/prototype/pointer/experiments/lean-comparison-20260920/run_audit.py
```

The runner uses a 10,000,000-transition ceiling, captures exit codes and output, and checks the expected accept/reject outcomes. The three full A Program proofs require the pinned repository's `sorted-proof-provider.p` import and `--legacy-intrinsic-dot`; the runner supplies both. Omitting these compatibility inputs can produce `unsupported` and is not the proof rejection reported here. Exit 0 means `done`; exit 1 means rejection in these fixtures; `pending`, `unsupported` and internal errors are not counted as success.

Reproduce the optional diagnostic trace from the repository root:

```sh
python3 src/prototype/pointer/experiments/lean-comparison-20260920/build_trace.py
AP_TRACE_MOTIVE=1 src/prototype/pointer/experiments/lean-comparison-20260920/pointer-check-trace --steps 10000000 src/prototype/pointer/experiments/lean-comparison-20260920/decision-graph-general.p
```

Expected diagnostic exit: 1. The trace executable is not used to establish the positive acceptance results.

## 7. Final execution results

The step counts below are solver-transition counts, not timings or cross-system performance measurements. Expected rejection is a passing audit observation, not a successful proof.

| Fixture | Expected exit | Actual exit | Observation |
| --- | ---: | ---: | --- |
| `box-fixed-cast.p` | 1 | 1 | rejected steps=1287 |
| `box-fixed.p` | 0 | 0 | done steps=1896 |
| `decision-no-check.p` | 0 | 0 | done steps=2302 |
| `decision-computed.p` | 0 | 0 | done steps=3525 |
| `decision-graph-general.p` | 1 | 1 | rejected steps=11479 |
| `comparator-bridge-rejected.p` | 1 | 1 | rejected steps=25570 |
| `comparator-bridge-computed.p` | 0 | 0 | done steps=26591 |
| `generic-conditional.p` | 0 | 0 | done steps=598624 |
| `generic-quick-original.p` | 1 | 1 | rejected steps=587424 |
| `generic-quick-projection-outside.p` | 1 | 1 | rejected steps=582624 |
| `GraphFixedCasesRejected.lean` | 1 | 1 | Dependent elimination/type mismatch rejected |
| `GraphBridgeAccepted.lean` | 0 | 0 | Checked; printed bridge axioms: none |
| `DecisionBridgeRejected.lean` | 1 | 1 | Dependent elimination/type mismatch rejected |
| `DecisionBridge.lean` | 0 | 0 | Checked; printed bridge axioms: none |
| `QuickSort.lean` | 0 | 0 | Checked; full theorem uses propext, Quot.sound |

## Appendix A. Raw execution output

Absolute sandbox prefixes have been removed from Lean diagnostic paths; messages, exit codes and step counts are retained.

```json
[
  {
    "file": "box-fixed-cast.p",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "rejected steps=1287\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "box-fixed.p",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "done steps=1896\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "decision-no-check.p",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "done steps=2302\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "decision-computed.p",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "done steps=3525\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "decision-graph-general.p",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "rejected steps=11479\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "comparator-bridge-rejected.p",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "rejected steps=25570\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "comparator-bridge-computed.p",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "done steps=26591\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "generic-conditional.p",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "done steps=598624\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "generic-quick-original.p",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "rejected steps=587424\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "generic-quick-projection-outside.p",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "rejected steps=582624\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "GraphFixedCasesRejected.lean",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "GraphFixedCasesRejected.lean:8:2: error: Dependent elimination failed: Failed to solve equation\n  true = f x\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "GraphBridgeAccepted.lean",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "'graph_sound' does not depend on any axioms\n'bridge_transport' does not depend on any axioms\n'bridge_general' does not depend on any axioms\n'bridge_specialized' does not depend on any axioms\n'bridge_rec' does not depend on any axioms\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "DecisionBridgeRejected.lean",
    "expected_exit": 1,
    "actual_exit": 1,
    "stdout": "DecisionBridgeRejected.lean:14:4: error: Type mismatch\n  ComparisonGraph.run a\nhas type\n  ComparisonGraph ?m.8 ?m.9 a (?m.8 ?m.9 a)\nbut is expected to have type\n  ComparisonGraph cmp x y b\nDecisionBridgeRejected.lean:16:8: error(lean.unknownIdentifier): Unknown identifier `comparison_inferred`\nDecisionBridgeRejected.lean:17:14: error(lean.unknownIdentifier): Unknown constant `comparison_inferred`\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "DecisionBridge.lean",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "'comparison_certificate' does not depend on any axioms\n'comparison_true' does not depend on any axioms\n'comparison_explicit' does not depend on any axioms\n",
    "stderr": "",
    "matched": true
  },
  {
    "file": "QuickSort.lean",
    "expected_exit": 0,
    "actual_exit": 0,
    "stdout": "'quicksort_sorted' depends on axioms: [propext, Quot.sound]\n[0, 1, 1, 2]\n'quicksort_sorted_from_decision' depends on axioms: [propext, Quot.sound]\n",
    "stderr": "",
    "matched": true
  }
]
```

## Appendix B. Embedded reproducibility files

These are audit fixtures, including deliberately rejected programs. They are not proposed production source. The extraction script writes the file bodies verbatim. Original proof-fixture names are retained for direct comparison with the logs.

| File | SHA-256 |
| --- | --- |
| `box-fixed-cast.p` | `9ee8f728f1495b621c8f03af7ffbf22093357698048c80603058ac5b549c50dc` |
| `box-fixed.p` | `e8f0ccd8b7114597dfaffda68478ec9bf51b97565341d24280a4ef860e456527` |
| `decision-no-check.p` | `74b866f8370602200413fb63c3fac0f7976b12f15f9390d8c2b75f8c112c8bfc` |
| `decision-computed.p` | `17918a9671110ffa1f002a448a9425c5ba7e56d47a415942f94c07b32b1f85ed` |
| `decision-graph-general.p` | `f6ece00ec1be9a4c904948f571c121286ca4df069e8cf278bda8b94e25493dde` |
| `comparator-bridge-rejected.p` | `ea89f11ddce161cd3b198ea65fdc3f8f16648ee9d0b910b5eddc4448d94b273f` |
| `comparator-bridge-computed.p` | `8ecef467e7112cadb474f11af0f15a698f52350de26a426ec7774af825ae52a6` |
| `generic-conditional.p` | `fece890be290c1bf766d10338f1de7a6aeed5e5584e29fa5f7864bfff17115b7` |
| `generic-quick-original.p` | `ae08a0fa3efeea9a91130b4c06dd6b89902b152578525f93504fd496ecec0951` |
| `generic-quick-projection-outside.p` | `07ea6315e2aa8f704c0c888d5f5c8450f21a7b32f070e4ff38abb93ae0d975bd` |
| `GraphFixedCasesRejected.lean` | `beb47304733c1b01a94a3573af6310c06c185549f56d90083f66da102ae93f7a` |
| `GraphBridgeAccepted.lean` | `31350c38a931d486e314b495cf19a54f4822d513c38b91f95ebc8a675d6b9e8c` |
| `DecisionBridgeRejected.lean` | `8bb7f366395601fb584a3988cdc1da41398e66566a49b91fbaa3b21783665dd6` |
| `DecisionBridge.lean` | `b63db10862ac192d755b2c2439e084e42bd9b5784f1b32b48318d834a0bfe2ef` |
| `QuickSort.lean` | `74744bec5aaaa0b7cf4e941649cf7f8f4addf31a74905609a5cf68440fe3af9f` |
| `run_audit.py` | `04152ccfc3b5b1a67a493bd78b3569cceeed7de1d8f2de3187be5fb5187825e2` |
| `build_trace.py` | `92328b01c0a249e1a7414e1a173b210698cc97e286ae2e505e930bdf65cab0b7` |

### Embedded file: box-fixed-cast.p

```text
Bool := @{ true:*; false:*; };
Box := @\b:Bool => { yes:* Bool.true; no:* Bool.false; };
Graph := \f:Bool->Bool => @\x:Bool => @\y:Bool => { run:(a:Bool)->* a (f a); };
bridge := \f:Bool->Bool => \cert:(x:Bool)->Box (f x) => \x:Bool => \g:Graph f x Bool.true => g @run a => (cert a :: Box Bool.true);
```

### Embedded file: box-fixed.p

```text
Bool := @{ true:*; false:*; };
Box := @\b:Bool => { yes:* Bool.true; no:* Bool.false; };
Graph := \f:Bool->Bool => @\x:Bool => @\y:Bool => { run:(a:Bool)->* a (f a); };
bridge := \f:Bool->Bool => \cert:(x:Bool)->Box (f x) => \x:Bool => \g:Graph f x Bool.true => g @run a => cert a;
bridge :: (f:Bool->Bool)->((x:Bool)->Box (f x))->(x:Bool)->Graph f x Bool.true->Box Bool.true;
```

### Embedded file: decision-no-check.p

```text
Bool := @{ true:*; false:*; };
Decision := \A:@ => \R:A->A->@ => \x:A => \y:A => @\b:Bool => {
	yes:R x y->* Bool.true;
	no:R y x->* Bool.false;
};
Graph := \A:@ => \le:A->A->Bool => \x:A => @\y:A => @\b:Bool => {
	run:(a:A)->* a (le x a);
};
bridge := \A:@ => \le:A->A->Bool => \R:A->A->@ => \cert:(x:A)->(y:A)->Decision A R x y (le x y) =>
	\x:A => \y:A => \b:Bool => \g:Graph A le x y b => g @run a => cert x a;
```

### Embedded file: decision-computed.p

```text
Bool := @{ true:*; false:*; };
Decision := \A:@ => \R:A->A->@ => \x:A => \y:A => @\b:Bool => {
	yes:R x y->* Bool.true;
	no:R y x->* Bool.false;
};
Graph := \A:@ => \le:A->A->Bool => \x:A => @\y:A => @\b:Bool => {
	run:(a:A)->* a (le x a);
};
bridge := \A:@ => \le:A->A->Bool => \R:A->A->@ => \cert:(x:A)->(y:A)->Decision A R x y (le x y) =>
	\x:A => \y:A => \b:Bool => \g:Graph A le x y b => g @run a => cert x a;
bridge :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->((x:A)->(y:A)->Decision A R x y (le x y))->
	(x:A)->(y:A)->(b:Bool)->Graph A le x y b->Decision A R x y (le x y);
```

### Embedded file: decision-graph-general.p

```text
Bool := @{ true:*; false:*; };
Decision := \A:@ => \R:A->A->@ => \x:A => \y:A => @\b:Bool => {
	yes:R x y->* Bool.true;
	no:R y x->* Bool.false;
};
Graph := \A:@ => \le:A->A->Bool => \x:A => @\y:A => @\b:Bool => {
	run:(a:A)->* a (le x a);
};
bridge := \A:@ => \le:A->A->Bool => \R:A->A->@ => \cert:(x:A)->(y:A)->Decision A R x y (le x y) =>
	\x:A => \y:A => \b:Bool => \g:Graph A le x y b => g @run a => cert x a;
bridge :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->((x:A)->(y:A)->Decision A R x y (le x y))->
	(x:A)->(y:A)->(b:Bool)->Graph A le x y b->Decision A R x y b;
```

### Embedded file: comparator-bridge-rejected.p

```text
Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
		lift : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};

SizedList := \A : @ => @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> A -> * n -> * (Nat.succ n);
};

Partition := \A : @ => \bound : Nat => @{
	parts :
		(lowerSize : Nat) ->
		(lower : SizedList A lowerSize) ->
		(upperSize : Nat) ->
		(upper : SizedList A upperSize) ->
		LT lowerSize (Nat.succ bound) ->
		LT upperSize (Nat.succ bound) -> *;
};

partitionLower := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					(Nat.succ lowerSize)
					((SizedList A).cons lowerSize head lower)
					upperSize upper
					(LT.lift lowerSize (Nat.succ size) lowerBound)
					(LT.weakenRight upperSize (Nat.succ size) upperBound);

partitionLower :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionUpper := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					lowerSize lower
					(Nat.succ upperSize)
					((SizedList A).cons upperSize head upper)
					(LT.weakenRight lowerSize (Nat.succ size) lowerBound)
					(LT.lift upperSize (Nat.succ size) upperBound);

partitionUpper :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionByDecision := \A : @ => \head : A => \size : Nat =>
	\decision : Bool => \partitioned : Partition A size =>
		decision
			@true => partitionLower A head size partitioned
			@false => partitionUpper A head size partitioned;

partitionByDecision :: (A : @) -> A -> (size : Nat) -> Bool ->
	Partition A size -> Partition A (Nat.succ size);

partition := \A : @ => \le : A -> A -> Bool =>
	\pivot : A => \size : Nat => \xs : SizedList A size =>
		xs @nil =>
			(Partition A Nat.zero).parts Nat.zero (SizedList A).nil
					Nat.zero (SizedList A).nil
					(LT.step Nat.zero) (LT.step Nat.zero)
		@cons tailSize head tail => {
			decision := le head pivot;
			partitionByDecision A head tailSize decision *tail;
		};

partition :: (A : @) -> (A -> A -> Bool) -> (pivot : A) ->
	(size : Nat) -> SizedList A size -> Partition A size;
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};
probe := \A:@ => \le:A->A->Bool => \R:A->A->@ => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => Nat.zero
	@case1 k h t comparison l left r right lb rb rest => { ignored := comparison @case0 bound => (decide h bound :: general_decision A R h bound Bool.true); Nat.zero; }
	@case2 k h t comparison l left r right lb rb rest => Nat.zero;
```

### Embedded file: comparator-bridge-computed.p

```text
Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
		lift : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};

SizedList := \A : @ => @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> A -> * n -> * (Nat.succ n);
};

Partition := \A : @ => \bound : Nat => @{
	parts :
		(lowerSize : Nat) ->
		(lower : SizedList A lowerSize) ->
		(upperSize : Nat) ->
		(upper : SizedList A upperSize) ->
		LT lowerSize (Nat.succ bound) ->
		LT upperSize (Nat.succ bound) -> *;
};

partitionLower := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					(Nat.succ lowerSize)
					((SizedList A).cons lowerSize head lower)
					upperSize upper
					(LT.lift lowerSize (Nat.succ size) lowerBound)
					(LT.weakenRight upperSize (Nat.succ size) upperBound);

partitionLower :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionUpper := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					lowerSize lower
					(Nat.succ upperSize)
					((SizedList A).cons upperSize head upper)
					(LT.weakenRight lowerSize (Nat.succ size) lowerBound)
					(LT.lift upperSize (Nat.succ size) upperBound);

partitionUpper :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionByDecision := \A : @ => \head : A => \size : Nat =>
	\decision : Bool => \partitioned : Partition A size =>
		decision
			@true => partitionLower A head size partitioned
			@false => partitionUpper A head size partitioned;

partitionByDecision :: (A : @) -> A -> (size : Nat) -> Bool ->
	Partition A size -> Partition A (Nat.succ size);

partition := \A : @ => \le : A -> A -> Bool =>
	\pivot : A => \size : Nat => \xs : SizedList A size =>
		xs @nil =>
			(Partition A Nat.zero).parts Nat.zero (SizedList A).nil
					Nat.zero (SizedList A).nil
					(LT.step Nat.zero) (LT.step Nat.zero)
		@cons tailSize head tail => {
			decision := le head pivot;
			partitionByDecision A head tailSize decision *tail;
		};

partition :: (A : @) -> (A -> A -> Bool) -> (pivot : A) ->
	(size : Nat) -> SizedList A size -> Partition A size;
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};
probe := \A:@ => \le:A->A->Bool => \R:A->A->@ => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => Nat.zero
	@case1 k h t comparison l left r right lb rb rest => { ignored := comparison @case0 bound => (decide h bound :: general_decision A R h bound (le h bound)); Nat.zero; }
	@case2 k h t comparison l left r right lb rb rest => Nat.zero;
```

### Embedded file: generic-conditional.p

```text
import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Acc;
import LT;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
general_all_from := \A:@ => \r:A->A->@ => \head:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(next:A)->(tail:List A)->r head next->* tail->* ((List A).cons next tail);
};
general_sorted := \A:@ => \r:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->general_all_from A r head tail->* tail->* ((List A).cons head tail);
};
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};
yes_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.true => d @yes p => p;
no_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.false => d @no p => p;
all_trans := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \y:A => \xs:List A => \bound:(general_all_from A R) y xs => bound
	@nil => (\x:A => \xy:R x y => ((general_all_from A R) x).nil)
	@cons head tail yh yt => (\x:A => \xy:R x y =>
		((general_all_from A R) x).cons head tail (trans x y xy head yh) (*yt x xy));
all_trans :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(y:A)->(xs:List A)->(general_all_from A R) y xs->(x:A)->R x y->(general_all_from A R) x xs;
tail_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => rest;
tail_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_sorted A R) tail;
tail_bound := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => bound;
tail_bound :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_all_from A R) head tail;
all_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => bound;
all_head :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->R lo head;
all_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => rest;
all_tail :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->(general_all_from A R) lo tail;
AllTo := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->R head hi->* tail->* ((List A).cons head tail);
};
to_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => bound;
to_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => rest;
append_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\lo:A => \left:(general_all_from A R) lo (List A).nil => \right:(general_all_from A R) lo following => right)
	@case1 head tail following output rest => (\lo:A =>
		\left:(general_all_from A R) lo ((List A).cons head tail) => \right:(general_all_from A R) lo following =>
		((general_all_from A R) lo).cons head output ((all_head A le R trans le_refl decide) lo head tail left) (*rest lo ((all_tail A le R trans le_refl decide) lo head tail left) right));
append_lower :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(lo:A)->(general_all_from A R) lo xs->(general_all_from A R) lo ys->(general_all_from A R) lo zs;
append_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\hi:A => \left:(AllTo A le R trans le_refl decide) hi (List A).nil => \right:(AllTo A le R trans le_refl decide) hi following => right)
	@case1 head tail following output rest => (\hi:A =>
		\left:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) => \right:(AllTo A le R trans le_refl decide) hi following =>
		((AllTo A le R trans le_refl decide) hi).cons head output ((to_head A le R trans le_refl decide) hi head tail left) (*rest hi ((to_tail A le R trans le_refl decide) hi head tail left) right));
append_upper :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(hi:A)->(AllTo A le R trans le_refl decide) hi xs->(AllTo A le R trans le_refl decide) hi ys->(AllTo A le R trans le_refl decide) hi zs;
append_sorted_step := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \following:List A => \output:List A =>
	\rest:@append A tail following output =>
	\pivot:A => \ih:(general_sorted A R) tail->(AllTo A le R trans le_refl decide) pivot tail->(general_sorted A R) following->(general_all_from A R) pivot following->(general_sorted A R) output =>
	\sl:(general_sorted A R) ((List A).cons head tail) =>
		\ul:(AllTo A le R trans le_refl decide) pivot ((List A).cons head tail) => \sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following =>
		(general_sorted A R).cons head output
			((append_lower A le R trans le_refl decide) tail following output rest head ((tail_bound A le R trans le_refl decide) head tail sl)
				((all_trans A le R trans le_refl decide) pivot following lr head ((to_head A le R trans le_refl decide) pivot head tail ul)))
			(ih ((tail_sorted A le R trans le_refl decide) head tail sl) ((to_tail A le R trans le_refl decide) pivot head tail ul) sr lr);
append_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\sl:(general_sorted A R) (List A).nil => \ul:(AllTo A le R trans le_refl decide) pivot (List A).nil =>
		\sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following => sr)
	@case1 head tail following output rest => (append_sorted_step A le R trans le_refl decide) head tail following output rest pivot &*rest;
append_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(general_sorted A R) xs->(AllTo A le R trans le_refl decide) pivot xs->(general_sorted A R) ys->(general_all_from A R) pivot ys->(general_sorted A R) zs;
All := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->P h->* t->* ((List A).cons h t);
};
SizedAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(n:Nat)->(h:A)->(t:SizedList A n)->P h->* n t->
		* (Nat.succ n) ((SizedList A).cons n h t);
};
PartAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) P l left->(SizedAll A le R trans le_refl decide) P r right->* ((Partition A n).parts l left r right lb rb);
};
all_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => head;
all_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => tail;
sized_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => head;
sized_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => tail;
part_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
part_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
append_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 right => (\pl:(All A le R trans le_refl decide) P (List A).nil => \pr:(All A le R trans le_refl decide) P right => pr)
	@case1 h t right output rest => (\pl:(All A le R trans le_refl decide) P ((List A).cons h t) => \pr:(All A le R trans le_refl decide) P right =>
		((All A le R trans le_refl decide) P).cons h output ((all_first A le R trans le_refl decide) P h t pl) (*rest ((all_rest A le R trans le_refl decide) P h t pl) pr));
append_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(xs:List A)->(ys:List A)->(zs:List A)->
	@append A xs ys zs->(All A le R trans le_refl decide) P xs->(All A le R trans le_refl decide) P ys->(All A le R trans le_refl decide) P zs;
lower_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionLower A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			(((SizedAll A le R trans le_refl decide) P).cons l h left ph ((part_left A le R trans le_refl decide) P n l left r right lb rb prior))
			((part_right A le R trans le_refl decide) P n l left r right lb rb prior));
lower_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionLower A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
upper_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionUpper A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			((part_left A le R trans le_refl decide) P n l left r right lb rb prior)
			(((SizedAll A le R trans le_refl decide) P).cons r h right ph ((part_right A le R trans le_refl decide) P n l left r right lb rb prior)));
upper_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionUpper A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
decision_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \d:Bool =>
	\input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionByDecision A h n d input output => g
	@case0 result trace => (lower_all A le R trans le_refl decide) P h n input result trace
	@case1 result trace => (upper_all A le R trans le_refl decide) P h n input result trace;
decision_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(d:Bool)->
	(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionByDecision A h n d input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
partition_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil =>
		((PartAll A le R trans le_refl decide) P Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
			(LT.step Nat.zero) (LT.step Nat.zero) ((SizedAll A le R trans le_refl decide) P).nil ((SizedAll A le R trans le_refl decide) P).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
				(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
				(((SizedAll A le R trans le_refl decide) P).cons l h left ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
				((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
	@case2 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
				(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
				((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))
				(((SizedAll A le R trans le_refl decide) P).cons r h right ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))));
partition_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(SizedAll A le R trans le_refl decide) P n xs->(PartAll A le R trans le_refl decide) P n output;
PartOrdered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot)) l left->(SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h)) r right->
		* ((Partition A n).parts l left r right lb rb);
};
ordered_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
ordered_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
quick_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil => ((All A le R trans le_refl decide) P).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k pivot tail) =>
			(append_all A le R trans le_refl decide) P left ((List A).cons pivot right) result appending
				(*leftGraph ((part_left A le R trans le_refl decide) P k l lower r upper lb rb
					((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
						partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))
				(((All A le R trans le_refl decide) P).cons pivot right ((sized_first A le R trans le_refl decide) P k pivot tail prior)
					(*rightGraph ((part_right A le R trans le_refl decide) P k l lower r upper lb rb
						((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
							partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))));
quick_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(SizedAll A le R trans le_refl decide) P n input->(All A le R trans le_refl decide) P output;
to_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R x hi)) xs => p
	@nil => ((AllTo A le R trans le_refl decide) hi).nil
	@cons h t bound rest => ((AllTo A le R trans le_refl decide) hi).cons h t bound *rest;
to_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R lo x)) xs => p
	@nil => ((general_all_from A R) lo).nil
	@cons h t bound rest => ((general_all_from A R) lo).cons h t bound *rest;
join_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \left:List A => \right:List A => \output:List A =>
	\appending:@append A left ((List A).cons pivot right) output =>
	\sl:(general_sorted A R) left => \ul:(AllTo A le R trans le_refl decide) pivot left => \sr:(general_sorted A R) right => \lr:(general_all_from A R) pivot right =>
	(append_sorted A le R trans le_refl decide) pivot left ((List A).cons pivot right) output appending sl ul
		((general_sorted A R).cons pivot right lr sr) (((general_all_from A R) pivot).cons pivot right (le_refl pivot) lr);
quick_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \partition_spec:(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output => \n:Nat => \access:Acc Nat LT n => \input:SizedList A n => \output:List A =>
	\g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (general_sorted A R).nil
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(join_sorted A le R trans le_refl decide) pivot left right result appending *leftGraph
			((to_upper A le R trans le_refl decide) pivot left ((quick_all A le R trans le_refl decide) (&(\x:A => R x pivot)) l (down l lb) lower left leftGraph
				((ordered_left A le R trans le_refl decide) pivot k l lower r upper lb rb
					(partition_spec pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))))
			*rightGraph
			((to_lower A le R trans le_refl decide) pivot right ((quick_all A le R trans le_refl decide) (&(\x:A => R pivot x)) r (down r rb) upper right rightGraph
				((ordered_right A le R trans le_refl decide) pivot k l lower r upper lb rb
					(partition_spec pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))));
quick_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(partition_spec:(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(general_sorted A R) output;
quick_correct := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \partition_spec:(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output => \xs:List A => \output:List A => \g:@quickSort A (&le) xs output => g
	@case0 original n values measurement access accessibility sorted sorting => (quick_sorted A le R trans le_refl decide partition_spec) n access values sorted sorting;
quick_correct :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(partition_spec:(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output)->(xs:List A)->(output:List A)->@quickSort A (&le) xs output->(general_sorted A R) output;
```

### Embedded file: generic-quick-original.p

```text
import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Acc;
import LT;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
general_all_from := \A:@ => \r:A->A->@ => \head:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(next:A)->(tail:List A)->r head next->* tail->* ((List A).cons next tail);
};
general_sorted := \A:@ => \r:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->general_all_from A r head tail->* tail->* ((List A).cons head tail);
};
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};
yes_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.true => d @yes p => p;
no_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.false => d @no p => p;
all_trans := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \y:A => \xs:List A => \bound:(general_all_from A R) y xs => bound
	@nil => (\x:A => \xy:R x y => ((general_all_from A R) x).nil)
	@cons head tail yh yt => (\x:A => \xy:R x y =>
		((general_all_from A R) x).cons head tail (trans x y xy head yh) (*yt x xy));
all_trans :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(y:A)->(xs:List A)->(general_all_from A R) y xs->(x:A)->R x y->(general_all_from A R) x xs;
tail_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => rest;
tail_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_sorted A R) tail;
tail_bound := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => bound;
tail_bound :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_all_from A R) head tail;
all_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => bound;
all_head :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->R lo head;
all_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => rest;
all_tail :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->(general_all_from A R) lo tail;
AllTo := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->R head hi->* tail->* ((List A).cons head tail);
};
to_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => bound;
to_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => rest;
append_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\lo:A => \left:(general_all_from A R) lo (List A).nil => \right:(general_all_from A R) lo following => right)
	@case1 head tail following output rest => (\lo:A =>
		\left:(general_all_from A R) lo ((List A).cons head tail) => \right:(general_all_from A R) lo following =>
		((general_all_from A R) lo).cons head output ((all_head A le R trans le_refl decide) lo head tail left) (*rest lo ((all_tail A le R trans le_refl decide) lo head tail left) right));
append_lower :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(lo:A)->(general_all_from A R) lo xs->(general_all_from A R) lo ys->(general_all_from A R) lo zs;
append_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\hi:A => \left:(AllTo A le R trans le_refl decide) hi (List A).nil => \right:(AllTo A le R trans le_refl decide) hi following => right)
	@case1 head tail following output rest => (\hi:A =>
		\left:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) => \right:(AllTo A le R trans le_refl decide) hi following =>
		((AllTo A le R trans le_refl decide) hi).cons head output ((to_head A le R trans le_refl decide) hi head tail left) (*rest hi ((to_tail A le R trans le_refl decide) hi head tail left) right));
append_upper :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(hi:A)->(AllTo A le R trans le_refl decide) hi xs->(AllTo A le R trans le_refl decide) hi ys->(AllTo A le R trans le_refl decide) hi zs;
append_sorted_step := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \following:List A => \output:List A =>
	\rest:@append A tail following output =>
	\pivot:A => \ih:(general_sorted A R) tail->(AllTo A le R trans le_refl decide) pivot tail->(general_sorted A R) following->(general_all_from A R) pivot following->(general_sorted A R) output =>
	\sl:(general_sorted A R) ((List A).cons head tail) =>
		\ul:(AllTo A le R trans le_refl decide) pivot ((List A).cons head tail) => \sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following =>
		(general_sorted A R).cons head output
			((append_lower A le R trans le_refl decide) tail following output rest head ((tail_bound A le R trans le_refl decide) head tail sl)
				((all_trans A le R trans le_refl decide) pivot following lr head ((to_head A le R trans le_refl decide) pivot head tail ul)))
			(ih ((tail_sorted A le R trans le_refl decide) head tail sl) ((to_tail A le R trans le_refl decide) pivot head tail ul) sr lr);
append_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\sl:(general_sorted A R) (List A).nil => \ul:(AllTo A le R trans le_refl decide) pivot (List A).nil =>
		\sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following => sr)
	@case1 head tail following output rest => (append_sorted_step A le R trans le_refl decide) head tail following output rest pivot &*rest;
append_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(general_sorted A R) xs->(AllTo A le R trans le_refl decide) pivot xs->(general_sorted A R) ys->(general_all_from A R) pivot ys->(general_sorted A R) zs;
All := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->P h->* t->* ((List A).cons h t);
};
SizedAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(n:Nat)->(h:A)->(t:SizedList A n)->P h->* n t->
		* (Nat.succ n) ((SizedList A).cons n h t);
};
PartAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) P l left->(SizedAll A le R trans le_refl decide) P r right->* ((Partition A n).parts l left r right lb rb);
};
all_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => head;
all_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => tail;
sized_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => head;
sized_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => tail;
part_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
part_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
append_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 right => (\pl:(All A le R trans le_refl decide) P (List A).nil => \pr:(All A le R trans le_refl decide) P right => pr)
	@case1 h t right output rest => (\pl:(All A le R trans le_refl decide) P ((List A).cons h t) => \pr:(All A le R trans le_refl decide) P right =>
		((All A le R trans le_refl decide) P).cons h output ((all_first A le R trans le_refl decide) P h t pl) (*rest ((all_rest A le R trans le_refl decide) P h t pl) pr));
append_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(xs:List A)->(ys:List A)->(zs:List A)->
	@append A xs ys zs->(All A le R trans le_refl decide) P xs->(All A le R trans le_refl decide) P ys->(All A le R trans le_refl decide) P zs;
lower_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionLower A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			(((SizedAll A le R trans le_refl decide) P).cons l h left ph ((part_left A le R trans le_refl decide) P n l left r right lb rb prior))
			((part_right A le R trans le_refl decide) P n l left r right lb rb prior));
lower_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionLower A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
upper_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionUpper A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			((part_left A le R trans le_refl decide) P n l left r right lb rb prior)
			(((SizedAll A le R trans le_refl decide) P).cons r h right ph ((part_right A le R trans le_refl decide) P n l left r right lb rb prior)));
upper_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionUpper A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
decision_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \d:Bool =>
	\input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionByDecision A h n d input output => g
	@case0 result trace => (lower_all A le R trans le_refl decide) P h n input result trace
	@case1 result trace => (upper_all A le R trans le_refl decide) P h n input result trace;
decision_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(d:Bool)->
	(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionByDecision A h n d input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
partition_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil =>
		((PartAll A le R trans le_refl decide) P Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
			(LT.step Nat.zero) (LT.step Nat.zero) ((SizedAll A le R trans le_refl decide) P).nil ((SizedAll A le R trans le_refl decide) P).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
				(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
				(((SizedAll A le R trans le_refl decide) P).cons l h left ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
				((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
	@case2 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
				(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
				((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))
				(((SizedAll A le R trans le_refl decide) P).cons r h right ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))));
partition_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(SizedAll A le R trans le_refl decide) P n xs->(PartAll A le R trans le_refl decide) P n output;
PartOrdered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot)) l left->(SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h)) r right->
		* ((Partition A n).parts l left r right lb rb);
};
ordered_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
ordered_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
partition_ordered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => ((PartOrdered A le R trans le_refl decide) pivot Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
		(LT.step Nat.zero) (LT.step Nat.zero)
		((SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot))).nil ((SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h))).nil
	@case1 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R x pivot))).cons l h left
				(comparison @case0 bound => (yes_order A le R trans le_refl decide) h bound (decide h bound))
				((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest))
			((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest)
	@case2 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
			((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R pivot x))).cons r h right
				(comparison @case0 bound => (no_order A le R trans le_refl decide) h bound (decide h bound))
				((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest));
partition_ordered :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output;
quick_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil => ((All A le R trans le_refl decide) P).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k pivot tail) =>
			(append_all A le R trans le_refl decide) P left ((List A).cons pivot right) result appending
				(*leftGraph ((part_left A le R trans le_refl decide) P k l lower r upper lb rb
					((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
						partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))
				(((All A le R trans le_refl decide) P).cons pivot right ((sized_first A le R trans le_refl decide) P k pivot tail prior)
					(*rightGraph ((part_right A le R trans le_refl decide) P k l lower r upper lb rb
						((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
							partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))));
quick_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(SizedAll A le R trans le_refl decide) P n input->(All A le R trans le_refl decide) P output;
to_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R x hi)) xs => p
	@nil => ((AllTo A le R trans le_refl decide) hi).nil
	@cons h t bound rest => ((AllTo A le R trans le_refl decide) hi).cons h t bound *rest;
to_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R lo x)) xs => p
	@nil => ((general_all_from A R) lo).nil
	@cons h t bound rest => ((general_all_from A R) lo).cons h t bound *rest;
join_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \left:List A => \right:List A => \output:List A =>
	\appending:@append A left ((List A).cons pivot right) output =>
	\sl:(general_sorted A R) left => \ul:(AllTo A le R trans le_refl decide) pivot left => \sr:(general_sorted A R) right => \lr:(general_all_from A R) pivot right =>
	(append_sorted A le R trans le_refl decide) pivot left ((List A).cons pivot right) output appending sl ul
		((general_sorted A R).cons pivot right lr sr) (((general_all_from A R) pivot).cons pivot right (le_refl pivot) lr);
quick_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat => \access:Acc Nat LT n => \input:SizedList A n => \output:List A =>
	\g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (general_sorted A R).nil
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(join_sorted A le R trans le_refl decide) pivot left right result appending *leftGraph
			((to_upper A le R trans le_refl decide) pivot left ((quick_all A le R trans le_refl decide) (&(\x:A => R x pivot)) l (down l lb) lower left leftGraph
				((ordered_left A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))))
			*rightGraph
			((to_lower A le R trans le_refl decide) pivot right ((quick_all A le R trans le_refl decide) (&(\x:A => R pivot x)) r (down r rb) upper right rightGraph
				((ordered_right A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))));
quick_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(general_sorted A R) output;
quick_correct := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \output:List A => \g:@quickSort A (&le) xs output => g
	@case0 original n values measurement access accessibility sorted sorting => (quick_sorted A le R trans le_refl decide) n access values sorted sorting;
quick_correct :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(output:List A)->@quickSort A (&le) xs output->(general_sorted A R) output;
```

### Embedded file: generic-quick-projection-outside.p

```text
import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Acc;
import LT;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
general_all_from := \A:@ => \r:A->A->@ => \head:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(next:A)->(tail:List A)->r head next->* tail->* ((List A).cons next tail);
};
general_sorted := \A:@ => \r:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->general_all_from A r head tail->* tail->* ((List A).cons head tail);
};
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};
yes_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.true => d @yes p => p;
no_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.false => d @no p => p;
all_trans := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \y:A => \xs:List A => \bound:(general_all_from A R) y xs => bound
	@nil => (\x:A => \xy:R x y => ((general_all_from A R) x).nil)
	@cons head tail yh yt => (\x:A => \xy:R x y =>
		((general_all_from A R) x).cons head tail (trans x y xy head yh) (*yt x xy));
all_trans :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(y:A)->(xs:List A)->(general_all_from A R) y xs->(x:A)->R x y->(general_all_from A R) x xs;
tail_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => rest;
tail_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_sorted A R) tail;
tail_bound := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => bound;
tail_bound :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_all_from A R) head tail;
all_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => bound;
all_head :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->R lo head;
all_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => rest;
all_tail :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->(general_all_from A R) lo tail;
AllTo := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->R head hi->* tail->* ((List A).cons head tail);
};
to_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => bound;
to_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => rest;
append_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\lo:A => \left:(general_all_from A R) lo (List A).nil => \right:(general_all_from A R) lo following => right)
	@case1 head tail following output rest => (\lo:A =>
		\left:(general_all_from A R) lo ((List A).cons head tail) => \right:(general_all_from A R) lo following =>
		((general_all_from A R) lo).cons head output ((all_head A le R trans le_refl decide) lo head tail left) (*rest lo ((all_tail A le R trans le_refl decide) lo head tail left) right));
append_lower :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(lo:A)->(general_all_from A R) lo xs->(general_all_from A R) lo ys->(general_all_from A R) lo zs;
append_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\hi:A => \left:(AllTo A le R trans le_refl decide) hi (List A).nil => \right:(AllTo A le R trans le_refl decide) hi following => right)
	@case1 head tail following output rest => (\hi:A =>
		\left:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) => \right:(AllTo A le R trans le_refl decide) hi following =>
		((AllTo A le R trans le_refl decide) hi).cons head output ((to_head A le R trans le_refl decide) hi head tail left) (*rest hi ((to_tail A le R trans le_refl decide) hi head tail left) right));
append_upper :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(hi:A)->(AllTo A le R trans le_refl decide) hi xs->(AllTo A le R trans le_refl decide) hi ys->(AllTo A le R trans le_refl decide) hi zs;
append_sorted_step := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \following:List A => \output:List A =>
	\rest:@append A tail following output =>
	\pivot:A => \ih:(general_sorted A R) tail->(AllTo A le R trans le_refl decide) pivot tail->(general_sorted A R) following->(general_all_from A R) pivot following->(general_sorted A R) output =>
	\sl:(general_sorted A R) ((List A).cons head tail) =>
		\ul:(AllTo A le R trans le_refl decide) pivot ((List A).cons head tail) => \sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following =>
		(general_sorted A R).cons head output
			((append_lower A le R trans le_refl decide) tail following output rest head ((tail_bound A le R trans le_refl decide) head tail sl)
				((all_trans A le R trans le_refl decide) pivot following lr head ((to_head A le R trans le_refl decide) pivot head tail ul)))
			(ih ((tail_sorted A le R trans le_refl decide) head tail sl) ((to_tail A le R trans le_refl decide) pivot head tail ul) sr lr);
append_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\sl:(general_sorted A R) (List A).nil => \ul:(AllTo A le R trans le_refl decide) pivot (List A).nil =>
		\sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following => sr)
	@case1 head tail following output rest => (append_sorted_step A le R trans le_refl decide) head tail following output rest pivot &*rest;
append_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(general_sorted A R) xs->(AllTo A le R trans le_refl decide) pivot xs->(general_sorted A R) ys->(general_all_from A R) pivot ys->(general_sorted A R) zs;
All := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->P h->* t->* ((List A).cons h t);
};
SizedAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(n:Nat)->(h:A)->(t:SizedList A n)->P h->* n t->
		* (Nat.succ n) ((SizedList A).cons n h t);
};
PartAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) P l left->(SizedAll A le R trans le_refl decide) P r right->* ((Partition A n).parts l left r right lb rb);
};
all_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => head;
all_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => tail;
sized_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => head;
sized_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => tail;
part_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
part_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
append_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 right => (\pl:(All A le R trans le_refl decide) P (List A).nil => \pr:(All A le R trans le_refl decide) P right => pr)
	@case1 h t right output rest => (\pl:(All A le R trans le_refl decide) P ((List A).cons h t) => \pr:(All A le R trans le_refl decide) P right =>
		((All A le R trans le_refl decide) P).cons h output ((all_first A le R trans le_refl decide) P h t pl) (*rest ((all_rest A le R trans le_refl decide) P h t pl) pr));
append_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(xs:List A)->(ys:List A)->(zs:List A)->
	@append A xs ys zs->(All A le R trans le_refl decide) P xs->(All A le R trans le_refl decide) P ys->(All A le R trans le_refl decide) P zs;
lower_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionLower A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			(((SizedAll A le R trans le_refl decide) P).cons l h left ph ((part_left A le R trans le_refl decide) P n l left r right lb rb prior))
			((part_right A le R trans le_refl decide) P n l left r right lb rb prior));
lower_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionLower A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
upper_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionUpper A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			((part_left A le R trans le_refl decide) P n l left r right lb rb prior)
			(((SizedAll A le R trans le_refl decide) P).cons r h right ph ((part_right A le R trans le_refl decide) P n l left r right lb rb prior)));
upper_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionUpper A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
decision_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \d:Bool =>
	\input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionByDecision A h n d input output => g
	@case0 result trace => (lower_all A le R trans le_refl decide) P h n input result trace
	@case1 result trace => (upper_all A le R trans le_refl decide) P h n input result trace;
decision_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(d:Bool)->
	(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionByDecision A h n d input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
partition_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil =>
		((PartAll A le R trans le_refl decide) P Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
			(LT.step Nat.zero) (LT.step Nat.zero) ((SizedAll A le R trans le_refl decide) P).nil ((SizedAll A le R trans le_refl decide) P).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
				(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
				(((SizedAll A le R trans le_refl decide) P).cons l h left ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
				((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
	@case2 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
				(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
				((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))
				(((SizedAll A le R trans le_refl decide) P).cons r h right ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))));
partition_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(SizedAll A le R trans le_refl decide) P n xs->(PartAll A le R trans le_refl decide) P n output;
PartOrdered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot)) l left->(SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h)) r right->
		* ((Partition A n).parts l left r right lb rb);
};
ordered_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
ordered_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
partition_ordered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => ((PartOrdered A le R trans le_refl decide) pivot Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
		(LT.step Nat.zero) (LT.step Nat.zero)
		((SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot))).nil ((SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h))).nil
	@case1 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R x pivot))).cons l h left
				((yes_order A le R trans le_refl decide) h pivot (comparison @case0 bound => decide h bound))
				((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest))
			((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest)
	@case2 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
			((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R pivot x))).cons r h right
				((no_order A le R trans le_refl decide) h pivot (comparison @case0 bound => decide h bound))
				((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest));
partition_ordered :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output;
quick_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil => ((All A le R trans le_refl decide) P).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k pivot tail) =>
			(append_all A le R trans le_refl decide) P left ((List A).cons pivot right) result appending
				(*leftGraph ((part_left A le R trans le_refl decide) P k l lower r upper lb rb
					((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
						partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))
				(((All A le R trans le_refl decide) P).cons pivot right ((sized_first A le R trans le_refl decide) P k pivot tail prior)
					(*rightGraph ((part_right A le R trans le_refl decide) P k l lower r upper lb rb
						((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
							partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))));
quick_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(SizedAll A le R trans le_refl decide) P n input->(All A le R trans le_refl decide) P output;
to_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R x hi)) xs => p
	@nil => ((AllTo A le R trans le_refl decide) hi).nil
	@cons h t bound rest => ((AllTo A le R trans le_refl decide) hi).cons h t bound *rest;
to_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R lo x)) xs => p
	@nil => ((general_all_from A R) lo).nil
	@cons h t bound rest => ((general_all_from A R) lo).cons h t bound *rest;
join_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \left:List A => \right:List A => \output:List A =>
	\appending:@append A left ((List A).cons pivot right) output =>
	\sl:(general_sorted A R) left => \ul:(AllTo A le R trans le_refl decide) pivot left => \sr:(general_sorted A R) right => \lr:(general_all_from A R) pivot right =>
	(append_sorted A le R trans le_refl decide) pivot left ((List A).cons pivot right) output appending sl ul
		((general_sorted A R).cons pivot right lr sr) (((general_all_from A R) pivot).cons pivot right (le_refl pivot) lr);
quick_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat => \access:Acc Nat LT n => \input:SizedList A n => \output:List A =>
	\g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (general_sorted A R).nil
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(join_sorted A le R trans le_refl decide) pivot left right result appending *leftGraph
			((to_upper A le R trans le_refl decide) pivot left ((quick_all A le R trans le_refl decide) (&(\x:A => R x pivot)) l (down l lb) lower left leftGraph
				((ordered_left A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))))
			*rightGraph
			((to_lower A le R trans le_refl decide) pivot right ((quick_all A le R trans le_refl decide) (&(\x:A => R pivot x)) r (down r rb) upper right rightGraph
				((ordered_right A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))));
quick_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(general_sorted A R) output;
quick_correct := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \output:List A => \g:@quickSort A (&le) xs output => g
	@case0 original n values measurement access accessibility sorted sorting => (quick_sorted A le R trans le_refl decide) n access values sorted sorting;
quick_correct :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(output:List A)->@quickSort A (&le) xs output->(general_sorted A R) output;
```

### Embedded file: GraphFixedCasesRejected.lean

```lean
inductive Box : Bool → Type where
  | yes : Box true
  | no : Box false
inductive Graph (f : Bool → Bool) : Bool → Bool → Type where
  | run (a : Bool) : Graph f a (f a)
def bridge_cases (f : Bool → Bool) (cert : (x : Bool) → Box (f x))
    (x : Bool) (g : Graph f x true) : Box true := by
  cases g
  exact cert x
```

### Embedded file: GraphBridgeAccepted.lean

```lean
inductive Box : Bool → Type where
  | yes : Box true
  | no : Box false
inductive Graph (f : Bool → Bool) : Bool → Bool → Type where
  | run (a : Bool) : Graph f a (f a)
theorem graph_sound (f : Bool → Bool) (x y : Bool) (g : Graph f x y) : f x = y := by
  cases g
  rfl
def bridge_transport (f : Bool → Bool) (cert : (x : Bool) → Box (f x))
    (x : Bool) (g : Graph f x true) : Box true := by
  have h := graph_sound f x true g
  exact h ▸ cert x
def bridge_general (f : Bool → Bool) (cert : (x : Bool) → Box (f x))
    (x y : Bool) (g : Graph f x y) : Box y := by
  cases g
  exact cert x
def bridge_specialized (f : Bool → Bool) (cert : (x : Bool) → Box (f x))
    (x : Bool) (g : Graph f x true) : Box true :=
  bridge_general f cert x true g
noncomputable def bridge_rec (f : Bool → Bool) (cert : (x : Bool) → Box (f x))
    (x : Bool) (g : Graph f x true) : Box true :=
  Graph.rec (motive := fun y _ => Box y) (cert x) g
#print axioms graph_sound
#print axioms bridge_transport
#print axioms bridge_general
#print axioms bridge_specialized
#print axioms bridge_rec
```

### Embedded file: DecisionBridgeRejected.lean

```lean
universe u v
inductive Decision {A : Type u} (R : A → A → Type v) (x y : A) : Bool → Type v where
  | yes : R x y → Decision R x y true
  | no : R y x → Decision R x y false

inductive ComparisonGraph {A : Type u} (cmp : A → A → Bool) (x : A) : A → Bool → Type u where
  | run (y : A) : ComparisonGraph cmp x y (cmp x y)


def comparison_inferred {A : Type u} (cmp : A → A → Bool) (R : A → A → Type v)
    (cert : (x y : A) → Decision R x y (cmp x y))
    (x y : A) (b : Bool) (g : ComparisonGraph cmp x y b) :=
  match g with
  | .run a => cert x a

#check @comparison_inferred
#print axioms comparison_inferred
```

### Embedded file: DecisionBridge.lean

```lean
universe u v
inductive Decision {A : Type u} (R : A → A → Type v) (x y : A) : Bool → Type v where
  | yes : R x y → Decision R x y true
  | no : R y x → Decision R x y false

inductive ComparisonGraph {A : Type u} (cmp : A → A → Bool) (x : A) : A → Bool → Type u where
  | run (y : A) : ComparisonGraph cmp x y (cmp x y)

def comparison_certificate {A : Type u} (cmp : A → A → Bool) (R : A → A → Type v)
    (cert : (x y : A) → Decision R x y (cmp x y))
    (x y : A) (b : Bool) (g : ComparisonGraph cmp x y b) : Decision R x y b := by
  cases g
  exact cert x y

def yes_order {A : Type u} {R : A → A → Type v} {x y : A}
    (d : Decision R x y true) : R x y := by
  cases d with
  | yes p => exact p

def comparison_true {A : Type u} (cmp : A → A → Bool) (R : A → A → Type v)
    (cert : (x y : A) → Decision R x y (cmp x y))
    (x y : A) (g : ComparisonGraph cmp x y true) : R x y :=
  yes_order (comparison_certificate cmp R cert x y true g)

#print axioms comparison_certificate
#print axioms comparison_true

noncomputable def comparison_explicit {A : Type u} (cmp : A → A → Bool) (R : A → A → Type v)
    (cert : (x y : A) → Decision R x y (cmp x y))
    (x y : A) (b : Bool) (g : ComparisonGraph cmp x y b) : Decision R x y b :=
  ComparisonGraph.rec (motive := fun b _ => Decision R x y b) (cert x y) g
#print axioms comparison_explicit
```

### Embedded file: QuickSort.lean

```lean
import Std
universe u

def lower {A : Type u} (cmp : A → A → Bool) (pivot : A) (tail : List A) :=
  tail.filter (fun x => cmp x pivot)
def upper {A : Type u} (cmp : A → A → Bool) (pivot : A) (tail : List A) :=
  tail.filter (fun x => !(cmp x pivot))

def quicksort {A : Type u} (cmp : A → A → Bool) (xs : List A) : List A :=
  match xs with
  | [] => []
  | pivot :: tail =>
    quicksort cmp (lower cmp pivot tail) ++
      pivot :: quicksort cmp (upper cmp pivot tail)
termination_by xs.length
decreasing_by
  all_goals
    simp_wf
    exact Nat.lt_succ_of_le (List.length_filter_le ..)


theorem mem_quicksort {A : Type u} (cmp : A → A → Bool) (xs : List A) :
    ∀ x ∈ quicksort cmp xs, x ∈ xs := by
  induction xs using quicksort.induct cmp with
  | case1 => simp [quicksort]
  | case2 pivot tail ih_lower ih_upper =>
    intro x hx
    rw [quicksort] at hx
    rcases List.mem_append.mp hx with hl | hr
    · exact List.mem_cons_of_mem pivot (List.mem_filter.mp (ih_lower x hl)).1
    · rcases List.mem_cons.mp hr with heq | hu
      · exact List.mem_cons.mpr (Or.inl heq)
      · exact List.mem_cons_of_mem pivot (List.mem_filter.mp (ih_upper x hu)).1

theorem quicksort_sorted {A : Type u} (cmp : A → A → Bool) (R : A → A → Prop)
    (trans : ∀ x y z, R x y → R y z → R x z)
    (yes_law : ∀ x y, cmp x y = true → R x y)
    (no_law : ∀ x y, cmp x y = false → R y x)
    (xs : List A) : List.Pairwise R (quicksort cmp xs) := by
  induction xs using quicksort.induct cmp with
  | case1 => simp [quicksort]
  | case2 pivot tail ih_lower ih_upper =>
    rw [quicksort, List.pairwise_append]
    have low_bound : ∀ x ∈ quicksort cmp (lower cmp pivot tail), R x pivot := by
      intro x hx
      exact yes_law x pivot (List.mem_filter.mp (mem_quicksort cmp _ x hx)).2
    have up_bound : ∀ y ∈ quicksort cmp (upper cmp pivot tail), R pivot y := by
      intro y hy
      have h := (List.mem_filter.mp (mem_quicksort cmp _ y hy)).2
      exact no_law y pivot (by cases e : cmp y pivot <;> simp_all)
    refine ⟨ih_lower, List.pairwise_cons.mpr ⟨up_bound, ih_upper⟩, ?_⟩
    intro x hx y hy
    rcases List.mem_cons.mp hy with heq | hr
    · subst y
      exact low_bound x hx
    · exact trans x pivot y (low_bound x hx) (up_bound y hr)

#print axioms quicksort_sorted
#eval quicksort (fun x y : Nat => x ≤ y) [2, 0, 1, 1]

inductive OrderDecision {A : Type u} (R : A → A → Prop) (x y : A) : Bool → Type where
  | yes : R x y → OrderDecision R x y true
  | no : R y x → OrderDecision R x y false

theorem quicksort_sorted_from_decision {A : Type u} (cmp : A → A → Bool) (R : A → A → Prop)
    (trans : ∀ x y z, R x y → R y z → R x z)
    (cert : (x y : A) → OrderDecision R x y (cmp x y))
    (xs : List A) : List.Pairwise R (quicksort cmp xs) := by
  apply quicksort_sorted cmp R trans
  · intro x y h
    have d : OrderDecision R x y true := h ▸ cert x y
    cases d with
    | yes p => exact p
  · intro x y h
    have d : OrderDecision R x y false := h ▸ cert x y
    cases d with
    | no p => exact p

#print axioms quicksort_sorted_from_decision
```

### Embedded file: run_audit.py

```python
import json
import os
from pathlib import Path
import subprocess
import sys

fixture_dir = Path(__file__).resolve().parent
repo = fixture_dir.parents[4]
lean = Path(os.environ.get('LEAN', 'lean'))
checker = repo / 'src/prototype/pointer/.build/pointer-check'
cases = [
	('box-fixed-cast.p', 1),
	('box-fixed.p', 0),
	('decision-no-check.p', 0),
	('decision-computed.p', 0),
	('decision-graph-general.p', 1),
	('comparator-bridge-rejected.p', 1),
	('comparator-bridge-computed.p', 0),
	('generic-conditional.p', 0),
	('generic-quick-original.p', 1),
	('generic-quick-projection-outside.p', 1),
	('GraphFixedCasesRejected.lean', 1),
	('GraphBridgeAccepted.lean', 0),
	('DecisionBridgeRejected.lean', 1),
	('DecisionBridge.lean', 0),
	('QuickSort.lean', 0),
]
results = []
for filename, expected in cases:
	path = fixture_dir / filename
	cmd = [str(lean), str(path)] if path.suffix == '.lean' else [str(checker), '--steps', '10000000', str(path)]
	if filename in ('generic-conditional.p', 'generic-quick-original.p', 'generic-quick-projection-outside.p'):
		cmd[1:1] = ['--legacy-intrinsic-dot', '--imports', str(repo / 'src/prototype/pointer/tests/fixtures/sorted-proof-provider.p')]
	result = subprocess.run(cmd, text=True, capture_output=True, timeout=180)
	ok = result.returncode == expected
	if expected == 0 and path.suffix == '.lean':
		ok = ok and 'sorryAx' not in result.stdout + result.stderr
	results.append(dict(file=filename, expected_exit=expected, actual_exit=result.returncode,
		stdout=result.stdout, stderr=result.stderr, matched=ok))
	print(filename, 'PASS' if ok else 'UNEXPECTED', result.returncode, flush=True)
(fixture_dir / 'audit-results.json').write_text(json.dumps(results, indent=2) + '\n')
sys.exit(0 if all(result['matched'] for result in results) else 1)
```

### Embedded file: build_trace.py

```python
from pathlib import Path
import subprocess,shlex
root=Path('src/prototype/pointer').resolve();p=root/'experiments/lean-comparison-20260920'
s=(root/'synthesis.c').read_text()
old='return pg_prove_pattern_type(typing, source_context(job->inner), pattern, type);'
new='''const struct pg_evidence *candidate = pg_prove_pattern_type(typing, source_context(job->inner), pattern, type);
	if (getenv("AP_TRACE_MOTIVE")) {
		fprintf(stderr, "candidate term=%.*s result=%s", (int)job->syntax->left->token.length,
			job->syntax->left->token.text, candidate ? "accepted" : "none");
		const struct pg_context *mc = pg_evidence_context(match_motive_context(synthesis, job));
		const struct pg_context *end = pg_evidence_context(source_context(job->inner));
		for (size_t i = 0; candidate && mc && mc != end; ++i, mc = mc->parent)
			fprintf(stderr, " binder_from_end[%zu]_independent=%d", i,
				pg_term_independent(pg_evidence_subject(candidate)->core, mc->binder));
		fputc('\\n', stderr);
	}
	return candidate;'''
assert s.count(old)==1;s=s.replace(old,new)
old='if (status == PG_CONVERSION_DIFFERENT) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }'
new='if (status == PG_CONVERSION_DIFFERENT) { if (getenv("AP_TRACE_MOTIVE")) fprintf(stderr, "conversion DIFFERENT step=%llu\\n", (unsigned long long)synthesis->steps); finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }'
assert s.count(old)==1;s=s.replace(old,new)
(p/'synthesis_trace.c').write_text(s)
r=subprocess.run(['make','-n','-B','-f','src/prototype/pointer/Makefile','pointer-check'],capture_output=True,text=True,check=True)
cmd=next(shlex.split(l) for l in r.stdout.splitlines() if l.startswith('cc '));cmd=[str(p/'synthesis_trace.c') if x==str(root/'synthesis.c') else x for x in cmd];cmd[-1]=str(p/'pointer-check-trace');subprocess.run(cmd,check=True)
```
