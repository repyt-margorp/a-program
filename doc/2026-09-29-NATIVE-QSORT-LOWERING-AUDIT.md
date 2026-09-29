# Native QuickSort lowering: manifest-controlled experiment and audit

Date: 2026-09-29
Status: executable experiment; not a compiler integration or a certified lowering.
Baseline: A Program `02895e5ee62208f145d6c5dd916917b137a76236`.
Tracking: #49. Related: #44 (C backend), #46 (linkable components), #47 (relevance).

## Problem List

1. P1 — Request C-oriented function/data representation without confusing link configuration with semantic transformation.
2. P2 — Establish what the native QuickSort experiment demonstrates and which correctness obligations remain.
3. P3 — Introduce reusable lowering stages instead of a name-based QuickSort replacement.
4. P4 — Compile the selected observable boundary into a target-appropriate residual program.

## P1. Manifest policy versus implementation

### Subjective (User)

2026-09-29, English paraphrase: the user wants a prototype and audit proposal for generating more C-like QuickSort rather than curried A Program-style functions, potentially through the Link Manifest. They requested a local experiment and presentation, not an Issue/PR submission in this turn.

### Objective (Code)

The current prototype `src/prototype/c_backend/emit.c` emits shared Core nodes as C functions using `ap_function`, `ap_apply`, `ap_delay`, and descriptor-driven data/Match operations. The runtime implements the closure and Oracle behavior; generated C does not link the source parser or proof checker.

The earlier session experiment checked an ordinary general sortedness theorem with a Bool QuickSort client in the same module, then emitted its `main` in ordinary checked mode. It produced 10,873 lines / 1,046 shared Core nodes and ran to FFTT. Omitting the unreferenced general theorem produced byte-identical C. This is reachability selection, not a general proof-erasure theorem.

Current `link/plan.c` accepts product/export/entry and `abi isolated_v1`, but has no native-list lowering or uncurrying profile. Its strict parser rejects unknown directives. The syntax suggested below is NOT accepted A Program syntax today.

### Assessment

A manifest can select a lowering policy; it cannot itself perform or justify transformations. Keep three layers separate:

```text
semantic Artifact / selected typed roots
    -> checked applicability and representation lowering
    -> C-oriented calls/control/data
    -> ABI wrappers and native link products
```

Illustrative future policy, not an implementation commitment:

```text
lowering_profile native-first-order-v1
specialize selected_sort [Bool, false_before_true]
representation selected_list bool_slice
calls direct_saturated
recursion bounded_c_stack
require_semantics_preserving true
on_unsupported reject
```

Resolve selectors to nominal semantic objects, not arbitrary names. An alias named quickSort does not establish its behavior. A fallback to structural closures should be opt-in and recorded; a request for a native ABI must not silently return an incompatible boxed ABI.

Do not reuse `native_script` for this purpose: it is a native linker control input, not a semantic optimization profile.

### Plan

- [x] Inspect the current link parser and emitter; preserve upstream implementation unchanged.
- [x] Prototype an explicit experimental profile and readable native output locally.
- [ ] Design a versioned lowering-profile reference from the Link Manifest.
- [ ] Specify per-pass applicability, guarantees, diagnostics and fallback behavior.
- Completion: native policy selects actual checked transformation passes, while artifact semantics and evidence admission stay upstream.

## P2. Experiment, evidence and limitations

### Subjective (User)

The user asked to see a working prototype before choosing a design. The Bool specialization is the agent's narrow experimental choice, reusing the previously tested source algorithm; it is not a user requirement to restrict sorting to Bool.

### Objective (Code)

Files in this directory:

- `profile.json`: experimental selection/provenance manifest.
- `emit.py`: strict profile reader and fixed-template emitter.
- `native.c.in`: hand-derived algorithm candidate.
- `native.c`: generated 53-line C implementation.
- `native.receipt.json`: explicitly marks artifact_decoded=false and kernel_refinement_checked=false.
- `check.py`, `differential.p`, `reference.txt`, `results.json`: reproducible tests and generated comparison inputs/results.
- `sanitize.c`: standalone C boundary/exhaustive harness.

The public function is:

```c
int ap_qsort_bool(const uint8_t *input, size_t n, uint8_t *output);
```

It uses a contiguous output slice and one n-byte scratch allocation. It preserves the source's first pivot and partition predicate `le head pivot`. Each partition preserves traversal order, then the pivot is placed between them. It recursively processes the smaller segment and iterates over the larger segment to avoid linear C call-stack growth. There are no closure/environment objects, curried applications, unary Nat lengths, or Acc values in the candidate C.

Input/output overlap is supported; disjoint input is unchanged. The foreign caller must provide n readable/writable bytes as applicable. Values must be 0/1. Zero length accepts null pointers. Nonzero null or invalid Bool inputs return 1, allocation failure returns 2, success returns 0. Error paths leave output unchanged. Pointer extent is an ABI precondition, not generally checkable in C.

Fresh results:

| Check | Result |
| --- | --- |
| All Bool inputs length 0..6 against actual A Program evaluation | 127 matched |
| All Bool inputs length 0..10 against expected sorting | 2,047 passed |
| Same exhaustive inputs with in-place output aliasing | 2,047 passed |
| Deterministic random Bool inputs, lengths 0..512 | 100 passed |
| Empty/null/invalid Bool/partial overlap controls | Passed |
| Unsupported specialization, false hash, false exact status, bad C alias | 4 profiles rejected |
| ASan + UBSan + leak detection, exhaustive and boundary C harness | Passed |

The A Program differential run reported `done steps=237492`, `run: done steps=3466203`. Both use the same source provider as the baseline. Allocation failure was not injected. Foreign invalid-pointer extents were not tested: they violate the API contract. No speed benchmark was performed.

Reproduce from the book workspace, retaining the pinned clone and overlay built in this session:

```sh
python3 tools/native-qsort-prototype/check.py
cc -std=c11 -Wall -Wextra -Werror -g -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  tools/native-qsort-prototype/native.c \
  tools/native-qsort-prototype/sanitize.c \
  -o tools/native-qsort-prototype/sanitize
ASAN_OPTIONS=detect_leaks=1 tools/native-qsort-prototype/sanitize
```

The emitter verifies SHA-256 of the source provider and the earlier `quick-proved.a` for provenance. It does NOT decode that artifact, select its nominal quickSort object, verify its theorem, recognize its graph, or derive C from it. It selects a hand-derived fixed algorithm template. Hash checks are not an equivalence proof or authentication. This is deliberately not presented as an Artifact transpiler.

### Assessment

The experiment establishes a practical target shape and finite behavioral evidence. It does not establish that existing compiler passes can automatically produce this output.

The 53 versus 10,873 line comparison is illustrative, not like-for-like: the native file exposes a specialized Bool array API without the print client, polymorphism or Identity actions; the old generated file contains the closed list client and structural machinery, plus requires a separate runtime. It is not a measured code-size optimization of the same general interface, nor a speedup claim.

This is not counting sort: the candidate retains pivot partition and recursive subproblems. Its running time is still worst-case quadratic (including many equal values); eliminating closures does not improve that asymptotic bound. It uses O(n) scratch space and O(log n) recursive stack frames from smaller-side recursion. No sort stability is claimed: source <= partition puts equal tail elements before their pivot.

Comparator specialization is important. The candidate tests `<=` in two passes and changes subproblem evaluation order. This is safe to investigate for the chosen pure total Bool comparison and value output. It would not preserve arbitrary comparator effects, failures, or observations. No general comparator transformation is claimed.

Correctness should eventually be stated with a representation relation, not identical types:

```text
Rep_BoolList(xs, input[0:n])
and valid buffers and successful allocation
    => native_sort(input,n,output) succeeds
       and Rep_BoolList(quickSort Bool le xs, output[0:n])
```

Finite machine lengths/resources restrict this realization. This is an explicit foreign/runtime contract, not silent replacement of unbounded source naturals with wrapping machine integers. The target has status-returning resource failures; pure source equality alone does not specify them.

Once a checked refinement like this exists, source sortedness/permutation can transfer through Rep. Source sortedness by itself does NOT prove the compiler transformation correct. General tests and C sanitizers are not that missing proof.

### Plan

- [x] Emit readable non-curried C and test it against source evaluation.
- [x] Label the candidate and receipt as unproved, hand-derived and non-integrated.
- [ ] Specify representation relation, memory/resource behavior and target refinement obligations formally.
- [ ] Validate independent general refinement, not just source Sorted.
- [ ] Extend to fixed-width elements and a restricted pure comparator only after selection/representation contracts are defined.
- Completion: a checked or explicitly justified refinement connects selected source terms to native output; finite testing remains supplementary evidence.

## P3. Reusable compiler implementation path

### Subjective (User)

The requested direction is C-oriented output, not a built-in quickSort name or replacement by a host sorting library. The staged design below is an agent recommendation.

### Objective (Code)

The source provider uses generic `quickSort`, `measure`, `SizedList`, `Acc`, lower/upper bounds, partition, and append. Uncurrying alone does not erase these representations. Conversely, selecting a flat public ABI alone can merely marshal into the existing boxed runtime internally.

### Assessment

Separate at least these transformations:

1. Specialize the selected element type and comparator where justified; retain generic code for other uses.
2. Recognize saturated known calls and produce direct multi-argument calls; retain closure adapters for partial/higher-order uses. Preserve argument demand and effect order.
3. Introduce conventional control flow and explicit data layouts from constructors/elimination; do not identify unrelated nominal families by shape.
4. Establish runtime relevance of sizes, Acc and bound witnesses. Eliminating Acc-driven recursion requires a semantic argument relating the target recursion to the source; it is not simply deleting an unused proof argument.
5. Lower a particular finite List representation to slices under a checked representation contract. Preserve source persistence through copy-out or prove ownership before destructive update.
6. Only then apply scratch-buffer reuse and smaller-side tail iteration, with purity/resource conditions recorded.

A useful first reusable improvement is direct-call specialization/uncurrying while keeping existing data and totality structure. This yields a smaller risk than simultaneously changing function calling, recursion justification, container representation and memory ownership.

The current fixed template is only an executable specification candidate for the last stages. It must not become `if export_name == quickSort: emit_template()` in the production backend.

### Plan

- [ ] Add a C-oriented intermediate representation for direct functions, blocks, values and explicit calls.
- [ ] Implement direct saturated calls first, with negative tests for partial application, capture and effect/demand order.
- [ ] Add semantic representation selections and refuse unsupported profiles rather than change output silently.
- [ ] Connect relevance/Acc lowering to #47 independently of Linker packaging in #46.
- [ ] Use length, append and partition as smaller general transformation tests before native QuickSort.
- [ ] Compare the same public interface and input domain before making performance claims.
- Completion: a shared lowering pipeline handles multiple algorithms, with no sort-name special case and no unchecked reuse of source proof as target correctness evidence.

## Recommendation to the user

Yes: let a Link Manifest request a native lowering profile. No: do not make the manifest itself an unchecked assertion that a source List/Acc program is equivalent to arbitrary C.
The local experiment demonstrates the desired output shape. The next proposed implementation is reusable direct-call/representation lowering, with the current structural backend retained as a reference and explicit fallback.
At the experimental stage no upstream implementation was changed. A subsequent user request authorizes a documentation PR and a separate implementation/design Issue; publication links are recorded in the submission header.

## P4. Compile a selected observable boundary, not the entire Artifact

### Subjective (User)

2026-09-29, English paraphrase: only what is required by the selected link boundary needs transpilation, and the Link Manifest should request output suited to the target language/computation model. The user requested that this discussion and the experimental code be included in the Markdown, then submitted as an Issue and PR.

### Objective (Code)

At the pinned baseline the emitter already traverses selected roots rather than exporting every declaration. The prior experiment's unreferenced general Sortedness theorem did not change generated C. However, reachable Core is still emitted structurally. Root reachability alone does not provide specialization, representation lowering or a runtime-relevance theorem.

The link prototype now supports multiple selected exports. This proposal builds on that implementation rather than reporting multi-export selection as wholly absent.

### Assessment

The intended contract is:

```text
Artifact + selected public boundary + target/lowering policy
    -> semantic dependency closure and required checked facts
    -> justified precomputation, specialization, relevance and representation passes
    -> residual runtime dependency closure
    -> target functions/data/control + ABI wrappers
```

Unexported does not mean unnecessary: private callees, runtime support, effect handlers, layout dependencies and required foreign capabilities may remain. Conversely, a semantic dependency need not retain its original runtime representation. Proofs/types can be required to justify a transformation without appearing in the final binary.

Distinguish three entry cases:

1. Closed pure total result, with a completed justified normalization: emit only the represented result if the selected observation contract permits it. Do not force NF of arbitrary partial computation, or make exhaustive normalization a mandatory compilation prerequisite.
2. Open exported function such as sort(xs): external arguments remain unknown. Specialize only known parameters; emit residual code for all admissible runtime inputs.
3. Effectful entry: precompute justified pure portions, but preserve required effects, their order, failure and divergence behavior. Do not execute print at compilation time and mistake the emitted text for runtime behavior.

A manifest should describe public roots/imports, known specialization arguments, representation/ABI/ownership, target execution model, required observations, resource assumptions and unsupported-policy. It should not carry unchecked acceptance evidence. C, GPU and RTL have different resource/control constraints; unsupported realization may reject rather than redefine source semantics.

A finite-buffer native sort contract is not the same as an unbounded source List function. State the representation relation, memory validity and resource failure behavior explicitly. Changes to observable effects, order, failure or termination require a justified refinement or a separately labeled changed-semantics mode.

Recommended first implementation: saturated known calls become direct C calls while preserving data/recursion semantics; then establish independent representation and Acc/relevance rules. Do not make the manifest a special-case switch replacing any definition spelled quickSort.

### Plan

- [ ] Extend the existing link profile with a separately versioned lowering policy, not native linker-script commands.
- [ ] Define the selected observable contract and allowed input domain.
- [ ] Distinguish semantic/checking dependencies from residual executable dependencies.
- [ ] Add justified partial evaluation with an explicit budget/fallback; never force effectful or potentially diverging evaluation merely to emit C.
- [ ] Test closed pure result, open function and effectful entry independently.
- [ ] Test that unused exports are omitted, private required helpers remain, and callbacks/foreign dependencies are not pruned incorrectly.
- [ ] Reuse the native QuickSort candidate as a target-shape test, not a compiler correctness certificate.
- Completion: selected behavior is preserved under a recorded realization contract while unrelated artifact structure and justified compile-time-only structure need not survive in target code.

## Embedded experimental C candidate

This is the complete tested native.c. It is hand-derived, not automatically extracted from the Artifact. It is included as a reviewable target shape, not production compiler code.

```c
/* Experimental hand-derived lowering candidate, NOT certified artifact extraction. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Same first-pivot, <= partition recurrence as the source, for Bool only.
 * Recurse on the smaller segment; iterate on the larger to bound C stack.
 * Scratch preserves each partition's order, but sorting is not stable:
 * source <= partition places equal tail elements before the pivot. */
static void sort_range(uint8_t *values, uint8_t *scratch, size_t n)
{
	while (n > 1) {
		uint8_t pivot = values[0];
		size_t lower = 0;
		for (size_t i = 1; i < n; ++i)
			if (values[i] <= pivot) ++lower;
		size_t l = 0, u = lower + 1;
		for (size_t i = 1; i < n; ++i) {
			if (values[i] <= pivot) scratch[l++] = values[i];
			else scratch[u++] = values[i];
		}
		scratch[lower] = pivot;
		memcpy(values, scratch, n);
		size_t upper = n - lower - 1;
		if (lower < upper) {
			sort_range(values, scratch, lower);
			values += lower + 1;
			scratch += lower + 1;
			n = upper;
		} else {
			sort_range(values + lower + 1, scratch + lower + 1, upper);
			n = lower;
		}
	}
}

/* 0 success, 1 invalid Bool/null input, 2 allocation failure.
 * Nonempty pointers must refer to at least n valid bytes; out must be writable.
 * Overlap is allowed. Input is unchanged for disjoint buffers.
 * Failure leaves output unchanged; n==0 accepts NULL pointers. */
int ap_qsort_bool(const uint8_t *input, size_t n, uint8_t *output)
{
	if (!n) return 0;
	if (!input || !output) return 1;
	for (size_t i = 0; i < n; ++i) if (input[i] > 1) return 1;
	uint8_t *scratch = malloc(n);
	if (!scratch) return 2;
	memmove(output, input, n);
	sort_range(output, scratch, n);
	free(scratch);
	return 0;
}
```

## Embedded experimental manifest

This JSON is consumed only by the local experimental emitter, NOT the current A Program link parser. Paths refer to the book workspace. Hashes identify that experiment's inputs; they neither decode the Artifact nor establish correctness. The correctness label must remain experimental.

```json
{
  "format": "ap-lowering-experiment-1",
  "mode": "hand-derived-candidate",
  "artifact": "../qsort-c-20260929/quick-proved.a",
  "artifact_sha256": "64ac6a0b02b6e49074d3d2e0eea25f3ddc4944ff0d11d920a538376dfe875bc9",
  "provider": "../../a-program-qsort-c-20260929/tests/fixtures/sorted-proof-provider.p",
  "provider_sha256": "a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6",
  "source_commit": "02895e5ee62208f145d6c5dd916917b137a76236",
  "selected_algorithm": "quickSort",
  "specialization": "Bool:false-before-true",
  "representation": "bool-byte-slice",
  "abi": "copy-out-status-v1",
  "export": "ap_qsort_bool",
  "correctness": "experimental-unproved-refinement"
}
```

## Reproduction scope

The source provider and compiler revision are pinned above. The book-local test scripts and binary artifacts are not installed by this documentation PR, and the book-relative commands are not standalone upstream commands. The C candidate itself can be saved from the fence and compiled independently with any caller respecting its stated ABI. The numerical results above describe the actual local run, not an upstream CI job.

For an upstream reproduction, build the pinned C/artifact overlay, enumerate all Bool lists of length 0..6, evaluate quickSort Bool le for each using the repository provider, and compare with the candidate for the same inputs. Native-only tests enumerated lengths 0..10, exercised aliases and boundaries and ran under ASan/UBSan. A proper implementation PR must add durable upstream tests; this document does not substitute for them.
