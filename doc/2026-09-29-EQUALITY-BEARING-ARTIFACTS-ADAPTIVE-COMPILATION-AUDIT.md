# Equality-Bearing Artifacts and Adaptive Compilation — Fresh Re-audit

Date: supplied design 2026-09-29; fresh re-audit 2026-10-01
Status: specification/prototype proposal; current boundaries verified, proposed assets/JIT not implemented
Code baseline: 97825ec28506f8a055db18af9e2577cba8cb1da7, clean main plus explicitly identified committed prototype overlays
Related: [artifact plan](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md), [ownership plan](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md), Issues [#44](https://github.com/repyt-margorp/a-program/issues/44), [#47](https://github.com/repyt-margorp/a-program/issues/47), [#49](https://github.com/repyt-margorp/a-program/issues/49)

## Problem List

| ID | Problem | Fresh disposition |
| --- | --- | --- |
| R1 | Separate existing artifacts/C realization from future equality assets and adaptive policy | Current/prototype capabilities corrected |
| R2 | Specify which relation licenses which transformation | Taxonomy supported; original union prohibition needs qualification |
| R3 | Reuse one checked equality without duplicating current semantic authority | Narrow experiment proposed; no new wire/TCB mandated |
| R4 | Separate compilation economics from acceptance, fuel, erasure and checkpoint claims | Cost model refined; trust and progress gaps explicit |

This active re-audit supersedes the implementation-status and unconditional
claims in the supplied design, preserved below. Its SHA-256 is
b37ec41c3f6a253b0bc46dc0f193b21ef3381eb33fff7c94eaaf50acadbe7c88.
The embedded copy preserves wording; Markdown space-based hard breaks are
normalized to explicit HTML breaks. The original temp file is unchanged.
Original Subjective sections report historical user intent; they do not grant
fresh approval for a wire format, e-graph, trusted checker or all proposed APIs.
The current request is re-audit and documentation/Issue submission only.
HOTT here means Higher Observational Type Theory.

## R1. What is actually implemented

### Subjective (User)

English paraphrase of the 2026-10-01 request: check the supplied design against
the latest A Program and open appropriate Issues. The supplied philosophy
treats compilation effort as optional investment and keeps target policy
downstream; implementation details remain proposals.

### Objective (Code)

| Layer | Baseline observation | What it does not establish |
| --- | --- | --- |
| Accepted Core | exact-pointer Lambda/App/Ref graph; alpha/conversion separate | one classifier per erased term |
| Accepted conversion | pg_conversion_certificate contains left/right; fixed pure reduction and implemented eta | portable explanation DAG or equality reflection |
| Accepted derivation export | pg_derivation_input_header turns local receipts into endpoint obligations; IO rejects raw receipt pointers | imported proof acceptance |
| Accepted images | APGSRC62/63, ordinary Solve on load, optional reduction support | complete zero-recomputation checkpoint |
| Artifact prototype | APGSRC68/APGRET4 typed views; --save-inputs reconstruction profile | promotion, full frontier resumption or automatic trust |
| C prototype | structural runtime, LinkerScript, scalar and enum profiles | general optimizer, equality saturation or adaptive JIT |
| Explicit trust option | --trust-image borrows saved completion under user trust without Solve | authenticated evidence or Kernel proof |
| Solver-input prototype | borrowed checked/pending inputs and actual query/family result owners | complete single-dispatcher/frontier consolidation |

The emitter borrows typed roots and performs no parsing, Solve or conversion.
Its adapter still selects/checks via ordinary whole-module Solve by default.
Selected-root emission is not selected-dependency-only acceptance of an
otherwise unchecked module.

Native scalar emission is already more than a dumb structural emitter:
lower/scalar.c supports a limited checked Pi subset as C parameters/results,
direct calls/captures and fixed-width wraparound. lower/representation.c defines
a downstream nullary nominal-enum contract, not source type equality.
General Nat compression/native list QuickSort lowering are not thereby
implemented. CUDA/RTL remain future directions.

Fresh tests used solver_inputs/overlay.sh NEW_DIR, composing its committed
ownership changes with artifact/readback/conversion patches over clean accepted
source at this revision. No patch was installed into accepted src/.

| Fresh command | Result / scope |
| --- | --- |
| make -j2 check, accepted code | pass; not full check-acceptance |
| make -f NEW_DIR/src/Makefile BUILD=NEW_DIR/build -j2 check | pass |
| artifact check-artifact-semantic, with adapted artifact tests | pass; inert views, aliases, untrusted inputs, validation |
| C check-c-backend check-c-scalar, on the same overlay | pass; interpreter/C differentials, Identity/Oracle controls, native scalar products |
| strict public fuel partition, List-09, 100:100 and 1600:1600 | fail; known unfinished frontier gate |

The successful C targets do not equal a fresh run of all Linker/enum/large
sorting/sanitizer suites or a backend-correctness theorem. This audit does not
claim them. Local evidence lives in the task's disposable test directory.

Reproduction from the repository root (the last command is expected to expose
the still-failing gate, not to turn it into an expected-failure pass):

```sh
audit_parent=$(mktemp -d)
audit_trial="$audit_parent/candidate"
bash src/prototype/solver_inputs/overlay.sh "$audit_trial"
make -f "$audit_trial/src/Makefile" BUILD="$audit_trial/build" -j2 check
make -f src/prototype/artifact_persistence/build.mk \
    OVERLAY="$audit_trial" BUILD="$audit_trial/build" \
    ARTIFACT_TESTS="$audit_trial/artifact_tests/" check-artifact-semantic
make -f src/prototype/c_backend/build.mk \
    OVERLAY="$audit_trial" BUILD="$audit_trial/build" -j2 \
    check-c-backend check-c-scalar
IMAGE_AUDIT_STRICT_BYTES=1 bash src/prototype/image_audit/partition_fuel.sh \
    "$audit_trial/build/pointer-check" examples/09_list_induction.p \
    "$audit_parent/partitions" ordinary 100:100 1600:1600
```

### Assessment

The missing work is not “introduce C transpilation” again. It is a selected
proof/DefEq reuse contract and, later, measurable adaptive policy. Existing
transport, receipts, reductions, representations and Linker policy should be
reused rather than copied into another authority.

### Plan

- [x] Inspect accepted and unpromoted capabilities and run relevant tests.
- [ ] Reference #44/#47/#49 instead of cloning their backend/erasure work lists.
- [ ] Focus the new Issue on selected semantic relation reuse and optional cost
  policy, starting without an e-graph.
- Completion: every implementation statement identifies its layer and test
  scope; proposals remain proposals.

## R2. Equality taxonomy and the precise union boundary

### Subjective (User)

The supplied design wants reusable equality without losing computational higher
witnesses. Relation permissions below are agent analysis, not newly approved
kernel rules.

### Objective (Code)

src/conversion.h explicitly excludes equality reflection and runtime handler
overrides. Its endpoint-oriented relation is over Core; evidence checks its
typed use. src/identity.h constructs action/transport expressions without
establishing their typing; checked evidence must authorize the family contract.
Interning remains exact structural sharing.

External primary-source spot-checks, accessed 2026-10-01:

- [Narya HOTT](https://narya.readthedocs.io/en/latest/hott.html) describes
  computational transport/lifting and explicitly incomplete implementation.
- [TYPES 2022 abstract](https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf)
  presents work-in-progress telescopic equality. The cited PDF is a three-page
  abstract, not a completed LIPIcs implementation.
- [egg Explanation API](https://docs.rs/egg/0.11.0/egg/struct.Explanation.html)
  has tree/flat explanations and shared subproof printing. Its rewrite checker
  is not an A Program dependent-type/higher-Identity checker.
- [Persistent Compiler Abstraction, v2](https://arxiv.org/abs/2602.16707v2)
  keeps e-graphs across MLIR passes; it does not prescribe a portable
  source-artifact format.
- [Lean propositions](https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/),
  [Agda runtime irrelevance](https://agda.readthedocs.io/en/latest/language/runtime-irrelevance.html)
  and [Idris multiplicities](https://idris2.readthedocs.io/en/latest/tutorial/multiplicities.html)
  specify their own irrelevance disciplines, not A Program's.
- [CompCert manual](https://compcert.org/man/manual.pdf) concerns observable
  behavior preservation. A Program's differential tests do not inherit that theorem.

### Assessment

Keep structural identity, alpha, Kernel DefEq, object-language Identity/type
equivalence, target representation correspondence and target behavior relations
separate. A typed e-node needs sufficient scope/classifier, nominal identity,
polarity and observer information; it does not require a duplicate typed DB.

Correct an overstatement in original P3: maintaining an optimizer quotient of
endpoints does **not by itself** erase higher witnesses or truncate source
theory. The problem is using that quotient as source conversion, discarding
computational witnesses, or replacing terms without a checked observer/relation
contract.

A justified target-observational or explicitly irrelevant fragment may admit
Identity-derived rewrites. That is not Kernel equality reflection. Banning all
proof-guided target rewrites would defeat the intended proof-aware optimizer.
Start with checked DefEq to avoid prematurely designing this richer bridge.

Type preservation is necessary but insufficient: two functions of the same type
need not return the same results. Source DefEq also does not validate C emission,
memory behavior, overflow or effect traces.

### Plan

- [ ] Specify scope, endpoints, relation, observers, conditions and validator
  for every transformation.
- [ ] Start with closed well-typed pure **total** supported roots. Pure alone
  does not establish termination; matching returned values does not preserve
  effect order or divergence.
- [ ] Test same Core/different classifier, nominal families, chosen loops and
  witnesses actually consumed computationally.
- [ ] Retain/reject unsupported relevant Identity instead of erasing by naming.
- Completion: optimizer union never silently extends Kernel conversion, merges
  nominal identity or authorizes proof erasure.

## R3. One reusable equality experiment, not a parallel relation store

### Subjective (User)

The original plan favors useful solved knowledge over search-history dumps.
No relation-fact table or wire extension is approved by this submission.

### Objective (Code)

src/derivation.c records receipt endpoints and clears local certificate pointers;
derivation_io.c rejects such pointers. synthesis_derivation.c re-establishes
local conversion/reduction authority. Therefore “all equality is thrown away”
is wrong: language witnesses and checking obligations already survive, and
reductions have supported retention APIs. There is no general downstream
checked-DefEq asset with a separate portable validation/reuse contract.

The artifact prototype removes Program-wide reduction-history retention from
its semantic image. Explicit reduction records remain separately checked.
Saving every conversion search frontier would reverse that boundary.

### Assessment

First ask whether existing derivation/reduction protocols suffice. Prefer a
reference/projection and small checker extension to a new relation DB, broad
explanation hierarchy or mutable answer lifecycle. A format extension needs
an independent A Program consumer; a C optimizer's missing information alone
does not justify it.

An imported endpoint pair or explanation is an obligation, not evidence.
A checker may re-evaluate endpoints; reusable explanations need measured value.
“No work during decoding” and “less validation than original search” are distinct.

The accepted source/kernel and emitter/runtime/native toolchain remain part of
end-to-end correctness. Declaring search untrusted does not remove unchecked
code generation from that boundary.

### Plan

- [ ] Select one closed root and existing DefEq-preserving rewrite.
- [ ] Reuse its actual typed owner and selected derivation/reduction inputs;
  identify the minimal missing immutable information.
- [ ] Export/import only reachable obligations, not pointer identity,
  union-find, queues, failed search or a new equality answer lifecycle.
- [ ] Validate with existing checking where possible. Corrupt endpoints,
  classifier, scope, nominal identity, profile and premises must fail.
- [ ] Reuse before selected C lowering; retain a no-optimization route and
  unchanged input-image digest.
- [ ] Measure baseline against round-trip validation/search/lowering costs.
  Keep cheap local conversion unless evidence favors replacement.
- Completion: a reusable selected relation rejects corrupt inputs and supports
  bounded reuse without a parallel semantic authority. No general e-graph needed.

## R4. Adaptive economics is policy, not semantic acceptance

### Subjective (User)

The supplied model treats .a as the semantic program and native code as optional
realization. Retain that design proposal without claiming every partial
artifact is already executable.

### Objective (Code)

--steps counts Solve transitions, not time or all allocation work. Execution
has a distinct limit. C revalidation stays within its total checking budget;
unused fuel is not secretly augmented. Explicit unauthenticated trust cannot
complete pending work or prove a transformation.

A continuous 3200-step List-09 run finishes in 2626 steps; saving after 1600 and
reloading with 1600 remains pending after cumulative 3200. Exact resumption is
already open in SE/AP work. No hotness monitor, adaptive policy or native cache
was tested or claimed implemented. The earlier c9e0ce81 candidate needed 2617
continuous steps; the updated prototype was regenerated and the listed checks
repeated at 97825ec2. Accepted source/test blobs are unchanged between these revisions.

### Assessment

Let I include additional search, validation, lowering, native build and
cache/profile I/O. Let d and n be direct/native costs per invocation under the
same observation contract. Invest only if d > n and
expected_remaining_runs * (d - n) > I. Do not divide by a nonpositive saving.

This is a heuristic/expectation, not a universal speedup theorem. Include
uncertain reuse, failures/fallback, profiling, invalidation and resource limits.

If validation is too costly, skip the optional transformation; do not skip
validation. Execute an already admissible realization instead. Partial
artifacts still need an explicit typing/effect/runtime contract for each
selected execution path.

“No optimization” is legitimate. Profile/native cache deletion must not change
source truth. A same-file digest is not authentication; user trust is not
checked proof reuse. Semantic reconstruction and exact progress checkpoints
are different products.

### Plan

- [ ] Put policy in downstream Transpiler/LinkerScript configuration or external
  profiles, not canonical semantic identity.
- [ ] Benchmark interpretation, cheap lowering and optimization with matching
  roots, observation/termination contracts and validation policy.
- [ ] Report Solve fuel separately from time, bytes and memory.
- [ ] Keep #47 relevance/admissibility and #43 partiality work separate.
- [ ] Introduce policy only after measured relation reuse; never weaken checking
  to fit the budget.
- Completion: policy changes cost/realization without changing source truth,
  assuming unfinished proofs or overclaiming resumption.

## Submission scope and literature limits

Tracking: [Issue #52](https://github.com/repyt-margorp/a-program/issues/52).

Create one focused design/prototype Issue for selected equality reuse followed
by optional adaptive policy. Cross-link #44, #47, #49 and the active ownership
plan; do not resubmit their entire work lists. The companion legacy audit
supplies anti-duplication constraints.

The broader bibliography is background, not an exhaustive fresh proof review.
Primary-source spot-checks support these boundaries, not soundness of every
proposed HOTT/effect/backend combination. Narya/OptiSat existence does not
prove A Program correct.

Additional checked references:
[Internal parametricity](https://arxiv.org/abs/2307.06448),
[Two-level staging](https://arxiv.org/abs/2209.09729),
[HotSpot tiering](https://docs.oracle.com/en/java/javase/17/vm/java-hotspot-virtual-machine-performance-enhancements.html),
[Jalapeño optimization](https://research.ibm.com/publications/adaptive-optimization-in-the-jalapeno-jvm--2),
[PLDI 2025 partial evaluation](https://pldi25.sigplan.org/details/pldi-2025-papers/14/Partial-Evaluation-Whole-Program-Compilation).
These are analogies/research evidence, not adopted syntax or schemas.
Illustrative a run/a compile commands, relation structs and phases below are
proposed pseudocode, not currently supported CLI/API.

<details>
<summary>Preserved 2026-09-29 design — historical requirements and unimplemented proposals</summary>

# Equality-Bearing Artifacts, Higher Observational Equality, and Adaptive Compilation

**Date:** 2026-09-29<br>
**Status:** Design audit / implementation proposal<br>
**Intended location:** `doc/2026-09-29-EQUALITY-BEARING-ARTIFACTS-ADAPTIVE-COMPILATION-AUDIT.md`

Related A Program material:

- `README.md`
- `src/conversion.c`
- `src/derivation.h`
- `src/derivation_io.c`
- `src/synthesis_derivation.c`
- `src/dimension.c`
- `src/action.c`
- `src/identity.c`
- `src/iadt.c`
- `src/prototype/c_backend/README.md`
- `doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md`

This document is an architectural audit and implementation plan. It is not an
approval of a concrete wire format, e-graph implementation, or trusted-kernel
extension.

In this document **HOTT means Higher Observational Type Theory**, not HoTT in the
generic sense.

---

# Problem List

| ID | Problem | Status |
| --- | --- | --- |
| P0 | The intended execution model is adaptive, but this design principle is not yet stated explicitly: `.a` should be directly executable, partially optimized, or fully transpiled according to an amortized cost decision. | planned |
| P1 | A Program already has several semantically different notions of equality, but they are not yet specified as one cross-stage taxonomy. | planned |
| P2 | Solve can establish conversion/normalization knowledge, but reusable solved equality is not yet a first-class semantic artifact asset. | planned |
| P3 | An ordinary e-graph would be unsound if every Higher Identity witness caused endpoint union. | planned |
| P4 | Equality saturation over raw erased Core terms is insufficient for A Program's typed occurrences, dependent classifiers, contexts, and nominal families. | planned |
| P5 | Optimization needs a proof-producing or translation-validating boundary so search can remain outside the TCB. | planned |
| P6 | Higher Identity, proof irrelevance, and runtime erasure must remain distinct. | planned |
| P7 | Source equality and backend representation correspondence must remain distinct. | planned |
| P8 | `.a` should persist selected semantic equality knowledge, not entire Solve/e-graph/evaluator histories. | planned |
| P9 | C/CUDA C/RTL backends should consume checked optimized semantic roots instead of rediscovering equality. | planned |
| P10 | Execution profiling and "hotness" should guide how much additional Solve/optimization/compilation work is worth paying for, without contaminating canonical semantics. | planned |

---

# P0. State the root design principle: compilation is an optional investment, not a mandatory ritual

## Subjective (User)

The intended architecture has long included a JIT-like cost model.

A program that is going to execute once should not automatically pay an
expensive compilation cost when

```text
compile_time + native_execution_time > direct_execution_time.
```

Conversely, a function or artifact that will execute repeatedly can justify
increasingly expensive Solve, optimization, specialization, and native
compilation work.

The intended user model is therefore:

```text
.a can be executed as an A Program artifact
        OR
.a can be incrementally solved / optimized
        OR
.a can be transpiled to a target
        OR
frequently reused parts can be compiled more aggressively than cold parts.
```

Full compilation is not the definition of successful execution.

## Objective

This principle fits the rest of A Program unusually well.

A Program already treats Solve as computational work rather than merely a
single yes/no front-end check. The artifact work is moving toward persistence of
meaningful semantic state rather than persistence of incidental process state.
The C prototype also deliberately keeps emission downstream from ordinary
checking/Solve rather than making the emitter itself the semantic authority.

The missing architectural statement is that **the amount of work spent before
execution should itself be adaptive**.

## Assessment

The most useful cost model is not "interpreted versus compiled" as a binary
choice. It is a ladder:

```text
Tier 0: execute the semantic artifact directly
Tier 1: execute after cheap Solve / normalization already present in .a
Tier 2: perform local equality-driven specialization
Tier 3: lower selected hot regions to a target representation
Tier 4: perform expensive equality saturation / specialization / native compile
Tier 5: reuse the resulting optimized artifact/native object many times
```

The exact tier count is not normative. The architectural point is that these are
points on one continuum.

For an execution region `r`, a simple break-even model is:

```text
C_compile(r) + N * C_compiled(r) < N * C_direct(r)
```

or

```text
N > C_compile(r) / (C_direct(r) - C_compiled(r)).
```

For A Program, `C_compile` should be decomposed further:

```text
C_compile
  = C_solve
  + C_equality_search
  + C_extraction
  + C_validation
  + C_lowering
  + C_target_codegen.
```

This matters because A Program can choose to purchase these components
independently.

For example, the system may discover that an additional 20 ms of Solve produces
a range fact that permits a 50x faster machine-integer representation for a
loop that will run a million times. That is a rational optimization investment.

The inverse case is equally important. If a command-line expression will be
executed once, a 500 ms equality-saturation pass that saves 2 ms of execution is
a regression even if the resulting C code is "better".

## Comparison with JIT and adaptive optimization

HotSpot historically starts by interpreting bytecode and compiles methods after
runtime evidence identifies them as hot. Its tiered compilation combines
different compilation costs and optimization qualities: a cheaper compiler can
improve startup while a more expensive optimizing compiler is reserved for code
whose expected future execution can amortize the work.

The Jalapeño/Jikes RVM adaptive optimization work makes the same principle
explicit as an online feedback-directed optimization problem: profiling and
statistical sampling choose where multi-level optimization effort is worth
spending.

A Program should adopt the **economic principle** while not copying the JVM
implementation.

The major difference is persistence:

```text
conventional JIT:
    runtime observation -> ephemeral optimized code

A Program:
    runtime/static observation
        -> Solve / equality knowledge
        -> persistent .a semantic assets
        -> later interpretation OR later optimization OR later transpilation
```

A `.a` file can therefore act as a **persistent compilation state** without
being merely a compiler checkpoint.

## The stronger A Program interpretation

The strongest architectural reading is:

> `.a` is the program representation. Native code is one possible cached
> realization of that program under a target/profile contract.

This removes the false requirement that every usable program must cross a
one-way "compile" boundary.

The artifact may remain perfectly legitimate even when:

- some expensive proof search has not been performed;
- some optional equalities have not been discovered;
- no target representation has been chosen;
- no native object exists;
- the expected execution count is too low to justify further work.

The relevant distinction is not:

```text
uncompiled / compiled
```

but:

```text
semantic artifact
    + amount of solved knowledge
    + amount of optimization knowledge
    + available target realizations.
```

## Compilation debt and optimization credit

This suggests two useful informal concepts.

### Compilation debt

Work that must still be done before a requested execution mode is legal.

Examples:

- unresolved type/semantic obligations required by the chosen execution path;
- unresolved representation preconditions;
- target constraints not yet checked.

### Optimization credit

Solved information that is not necessary for correctness but can make future
execution cheaper.

Examples:

- a proven bound;
- a solved constructor alternative;
- a DefEq result;
- a proof of runtime irrelevance;
- a stronger algebraic equality;
- an effect-freedom fact;
- an equality-saturation explanation permitting a cheaper extracted term.

`.a` should be able to accumulate optimization credit.

## Plan

- [ ] Add a normative statement that `.a` is directly executable in principle
  and native compilation is an optimization/realization choice, not the
  semantic definition of execution.
- [ ] Keep compilation policy out of canonical `.a` semantics.
- [ ] Define execution tiers only after measuring current direct execution,
  Solve, and transpilation costs.
- [ ] Put cost models, hotness, optimization budgets, and target realization
  policy in the Transpiler / LinkerManuscript layer.
- [ ] Keep canonical semantics independent of invocation counters and wall-clock
  measurements.
- [ ] Permit explicit policy to select `interpret`, `quick`, `optimize`, `full`,
  or future equivalents without changing the meaning of the `.a` artifact.
- [ ] Completion criterion: a one-shot program can execute without paying full
  transpilation cost, while the same `.a` can later be consumed by a more
  expensive transpilation strategy when reuse justifies it.

---

# P0.5. `.a` is optimization-neutral; proof use belongs to Transpiler and LinkerManuscript

## Subjective (User)

The fact that A Program has proved an equality or extensionality theorem does
not imply that any particular execution or transpilation must exploit it.

A user should be able to execute a deliberately unoptimized realization even
when the artifact contains enough proof information to justify stronger
optimization.

The time spent *finding, selecting, combining, and validating optimizations* is
itself computation time. Therefore optimization search cannot be treated as a
free or semantically mandatory consequence of possessing a proof.

The intended boundary is:

```text
A Program / .a
    answers: what terms, types, proofs, equalities, and solved facts exist?

Transpiler
    answers: which of those facts are useful for this target realization?

LinkerManuscript
    answers: which roots are linked, which target/profile is selected,
             how aggressive specialization/optimization may be,
             and how much compilation work is worth paying for?

e-graph or other optimizer
    is an optional implementation technique inside the Transpiler.
```

## Assessment

This changes an important emphasis of the earlier audit.

The canonical artifact should not be described as storing "the optimizer's
chosen equality closure" or "the e-graph state". It should instead expose the
semantic material that a future compiler *may* use.

In particular:

```text
proof exists in .a
        does not imply
proof is used by transpilation

proof is usable for optimization
        does not imply
optimization search must be performed

optimization is discovered
        does not imply
the cost model must select it

an optimized realization exists
        does not imply
it is the only valid realization
```

This permits all of the following to be valid consumers of the same `.a`:

```text
1. Direct A Program execution with no target compilation.

2. A minimal Transpiler that lowers syntax/semantics almost structurally and
   ignores most available extensionality proofs.

3. A proof-aware Transpiler that uses selected equalities for specialization.

4. An equality-saturation Transpiler that spends substantial time searching a
   large equivalence space.

5. A LinkerManuscript requesting aggressive optimization only for selected
   exported roots.

6. A LinkerManuscript deliberately selecting a simple/debuggable realization
   even when a faster realization is known.
```

The artifact therefore provides **optimization opportunities**, not an
optimization mandate.

## Consequence for e-graphs

An e-graph should not appear in the semantic definition of `.a`.

It is better understood as:

```text
.a semantic facts
      |
      v
Transpiler imports a chosen subset
      |
      v
optional e-graph / rewrite search / superoptimizer
      |
      v
candidate target realization
      |
      v
checker / translation validation
```

The e-graph may be created, saturated, partially saturated, abandoned early, or
not created at all.

Its union-find structure, rewrite frontier, extraction cost model, and
saturation budget are compiler implementation state.

None of them should be required in order to load or execute `.a`.

## Consequence for equality persistence

The phrase "equality-bearing artifact" must be interpreted semantically.

The artifact should carry equality **when equality is itself part of the
persistent A Program semantic state**:

- an object-language Higher Identity witness;
- a proved theorem;
- an explicitly materialized result of Solve that the artifact model defines as
  persistent;
- checked semantic facts required by retained declarations/results;
- explicit equivalences or extensionality witnesses represented in the
  language/artifact.

By contrast, the artifact need not contain every equality that a future
Transpiler could rediscover from those facts.

For example, if `.a` contains:

```text
comm : Π a b. Id Nat (a + b) (b + a)
```

then the theorem/witness belongs to `.a` if it is part of the semantic program.

Whether a Transpiler creates millions of derived rewrite instances of `comm`,
inserts them into an e-graph, and uses one of them to optimize a hot loop is a
compiler decision.

Those instantiated search edges do not become `.a` semantics merely because
they were explored.

## LinkerManuscript as the policy boundary

The LinkerManuscript is the natural place to express choices such as:

```text
target = c
root = main
optimization_budget = low
use_extensional_rewrites = false
integer_representation = checked_machine_int
specialize = ["foo", "bar"]
```

or, in another build:

```text
target = cuda-c
optimization_budget = exhaustive
use_extensional_rewrites = true
specialize_hot_roots = true
```

The exact syntax is not proposed here.

The architectural point is that the LinkerManuscript describes a **requested
realization**, not program truth.

The same `.a` remains valid under different manuscripts.

## Radical cost interpretation

Compilation work must be charged to the same global computation budget as
execution work.

For a candidate optimization search `S`:

```text
TotalCost(S)
  = search_time
  + proof/equality instantiation time
  + extraction time
  + validation time
  + lowering/codegen time
  + resulting_execution_time.
```

Therefore a compiler may rationally choose:

```text
"I know stronger equalities exist, but it is not worth searching them now."
```

This is not a missed semantic obligation. It is a cost decision.

Likewise, a future run may make a different decision because expected reuse has
changed.

## Plan

- [ ] Remove e-graph state from any normative description of canonical `.a`.
- [ ] Describe `.a` as carrying semantic proofs/facts, not optimizer closure.
- [ ] Put equality-search strategy in the Transpiler.
- [ ] Put optimization budget, target, linkage, and realization policy in the
  LinkerManuscript.
- [ ] Permit proof-aware and proof-ignorant transpilers to consume the same `.a`
  as long as both preserve required semantics.
- [ ] Make "do not optimize" a first-class valid policy.
- [ ] Charge optimization discovery/validation time to total execution cost.
- [ ] Completion criterion: deleting every e-graph implementation from the
  compiler must not change the `.a` specification or prevent direct `.a`
  execution.

---

# P1. Define an explicit equality taxonomy

## Subjective (User)

A Program is now at the point where equality must be discussed precisely in
Higher Observational Type Theory terms, while also supporting e-graph-like
optimization and transpilation.

## Objective (Code)

The current architecture already distinguishes several concepts.

- Core pointer interning is structural sharing, not alpha/WHNF equality.
- Typed occurrences and classifiers sit above erased Core.
- `src/conversion.c` supplies a Kernel conversion relation.
- Identity/higher-dimensional machinery is implemented separately in
  `dimension.c`, `action.c`, `identity.c`, and `iadt.c`.
- Existing design text explicitly does not make an arbitrary Identity witness
  extend global definitional equality.
- The C backend's treatment of Identity already avoids assuming that every loop
  is reflexivity.

This is evidence that A Program already has a *plural* equality architecture,
even if the taxonomy is not yet written as one normative table.

## Assessment

At least the following relations must remain distinct:

| Relation | Typical form | Proof relevance | Ordinary e-class union? | Artifact role | Backend role |
| --- | --- | ---: | ---: | --- | --- |
| Structural identity | same canonical Core node | no | already shared | graph sharing | code sharing |
| Alpha equivalence | `t ≡α u` | meta-level | only in alpha-aware domain | optional fact | naming/scoping |
| Definitional equality | `Γ ⊢ t ≡ u : A` | meta-level certificate | **yes, inside a well-defined typed congruence domain** | reusable equality asset | source optimization |
| Higher Identity | `p : Id A a b` | **yes** | **no, in general** | semantic witness | transport/action/higher computation |
| Type equivalence | `e : A ≃ B` or equivalent structure | yes | no, by default | semantic witness | possible representation bridge |
| Representation relation | `s ~rep[T] t` under conditions | compiler evidence | only inside the representation domain | optimization asset | lowering |
| Target observational equivalence | `P ≈obs,target Q` | compiler evidence | only for target semantics | target-scoped asset | late optimization |

The central invariant is:

```text
p : Id A a b
```

does **not** imply:

```text
Γ ⊢ a ≡ b : A
```

for Kernel conversion.

Likewise:

```text
A ≃ B
```

does not imply that `A` and `B` are definitionally interchangeable unless a
specific theory rule says so.

And:

```text
Nat ~rep[C] uint64_t
```

under a range proof does not imply source-level `Nat ≡ uint64_t`.

## Comparison: Observational Type Theory

Observational Type Theory is historically relevant because it separates a
decidable/intensional conversion discipline from a richer substitutive
extensional equality. That separation is directly useful here: richer equality
does not have to be implemented by making Kernel conversion omniscient.

## Comparison: Higher Observational Type Theory / Narya

Higher Observational Type Theory makes Identity computational by defining its
behavior according to the observed structure of the base type.

Narya's HOTT mode is the most direct implementation reference currently
available. Its documentation describes:

- function identities as higher functions;
- record identities as records of identities;
- datatype identities as higher datatypes;
- codata identities as bisimulations;
- transport and path lifting as higher fields/operations.

This is much closer to A Program's existing action/dimension direction than a
design where equality is only a proposition attached to terms.

However, HOTT is an active research program. Narya's documentation itself notes
unfinished areas. A Program should say exactly which equations/rules it
implements rather than using "HOTT" as an undifferentiated compatibility label.

## Plan

- [ ] Add a normative equality glossary.
- [ ] For each relation, document:
  reflexivity, symmetry, transitivity, congruence, substitution, transport,
  e-class union, erasure, and lowering permissions.
- [ ] Keep Higher Identity out of global `conversion.c` unless a specific HOTT
  computation rule reduces it.
- [ ] Require every optimizer rule to state which relation it consumes and
  which relation it produces.
- [ ] Completion criterion: no source file or artifact field uses the generic
  word "equal" where two distinct relations could be meant.

---

# P2. Persist semantic equality results without persisting compiler search policy

## Subjective (User)

Solve should not throw away a semantic equality result when that result is part
of the persistent A Program state. `.a` should retain the proof/witness/fact in a
form that future consumers can use.

This does **not** require `.a` to precompute or persist the closure of equalities
that a Transpiler might derive for optimization.

## Objective (Code)

The current conversion certificate is intentionally small. At present it is
effectively endpoint-oriented: once conversion establishes equality, the local
certificate identifies the terms accepted by that conversion run.

The derivation serialization design is more conservative:

- process-local certificate pointers are not portable semantic identities;
- serialized endpoints/inputs are obligations rather than an assertion that a
  previous process should be blindly trusted;
- load/reconstruction uses ordinary semantic work to re-establish local
  authority.

The artifact persistence plan already contains the right general principle:
retain semantic roots and dependencies, not every evaluator/cache/history
structure.

The missing consumer is a future optimizer/transpiler.

## Assessment

Three things must be separated:

```text
1. equality fact
2. explanation/checking evidence
3. search history that discovered it
```

Only (1) and the reachable subset of (2) belong in a canonical semantic
artifact.

Item (3) is normally disposable.

A useful first meta-level explanation language could be:

```text
DefEqExplanation ::=
    Refl(j)
  | Alpha(j_left, j_right)
  | Computation(rule, before, after, premises...)
  | Congruence(operator, child_explanations...)
  | CheckedNormalization(receipt)
  | Sym(explanation)
  | Trans(explanation, explanation)
```

This is **not** an A Program object-language Identity proof. It is a checker
certificate for a meta-level Kernel relation.

A Program should avoid forcing every ordinary conversion query to allocate a
large proof object. Explanations can be materialized lazily only when a
reachable downstream consumer requests a persistent asset.

## Comparison: egg explanations

`egg` represents a congruence relation over many expressions and can maintain
explanations for why expressions became equivalent. Its explanation subsystem
demonstrates an important engineering point: equality-search state and
equality-explanation state can be separated, and explanations can preserve
sharing rather than becoming flat, duplicated rewrite logs.

A Program should borrow this principle, not the exact data format.

## Plan

- [ ] Prototype immutable DAG-shared DefEq explanation nodes.
- [ ] Build a small explanation checker.
- [ ] Do not modify the TCB until the checker contract is written.
- [ ] Materialize explanations only for selected exported/optimized facts.
- [ ] Keep current cheap local conversion certificates for ordinary Kernel use
  unless benchmarks justify replacement.
- [ ] Completion criterion: an optimized export can reload an equality asset and
  validate it without reconstructing the original equality-search frontier.

---

# P3. Never model arbitrary HOTT Identity by ordinary e-class union

## Objective (Code)

The existing function action logic has the characteristic HOTT shape: function
Identity observes two endpoint functions at related endpoint arguments and
relates their results.

The Identity implementation also contains explicit diagonal/refl handling rather
than treating all loops as reflexive.

The dimension subsystem includes maps, faces, inversions, composition, and
higher-dimensional structure. This is inherently richer than a setoid quotient
of terms.

The current C prototype similarly avoids reducing arbitrary chosen loops merely
because their endpoints happen to coincide.

## Assessment

A conventional e-graph does:

```text
prove/rewrite a = b
    -> union(eclass(a), eclass(b))
```

That is valid only when the relation being represented is intended as a
quotientable congruence.

For HOTT:

```text
p : Id A a b
q : Id A a b
r : Id (Id A a b) p q
```

the witness itself is data with higher structure.

Doing:

```text
union(a, b)
```

solely because `p` exists discards the distinction between endpoints and the
path connecting them. Repeating the process at higher levels performs an
unwanted truncation.

Therefore A Program should begin with three logically separate graph roles:

```text
Q: Quotient/equality graph
   - DefEq
   - selected compiler equations whose relation is explicitly quotientable

H: Higher witness graph
   - p : Id A a b
   - q : Id (Id A a b) p p'
   - transport/action/lifting structure

R: Representation relation graph
   - source semantic object related to a target realization
   - side conditions and target profiles
```

`Q` can be implemented with an e-graph.

`H` should initially remain ordinary typed semantic structure plus indexed
relation edges.

`R` should remain explicitly target/representation scoped.

A true "higher e-graph" may eventually be useful, but it should be introduced
only when a concrete algorithm needs higher cells to participate directly in
saturation.

## Comparison: egglog / relational extensions

Modern equality-saturation systems often combine congruence classes with
analyses or relational facts. This supports the important engineering idea that
not every useful compiler fact must become the e-graph's union relation.

## Plan

- [ ] Define exactly one initial quotientable relation for the prototype.
- [ ] Do not call union solely from an `Id` witness.
- [ ] Add a regression test with nontrivial loops.
- [ ] Add a regression where function Identity/action is consumed after
  optimization.
- [ ] Add a test that `Id` witnesses survive artifact save/load even when their
  endpoint terms happen to be DefEq separately.
- [ ] Completion criterion: equality saturation cannot silently collapse higher
  witness structure.

---

# P4. E-graph nodes must represent typed judgements, not merely erased Core

## Objective (Code)

A Program intentionally keeps typed occurrences, classifiers, contexts, maps,
and derivations above the small erased Core DAG.

The same erased Core can participate in different typed occurrences. Nominal
families also require identity beyond structural similarity.

Therefore:

```c
const struct pg_term *
```

is insufficient as the semantic identity of an e-node.

## Assessment

Conceptually, the unit should be a judgement:

```text
Γ ⊢ t : A
```

not just `t`.

A safe first prototype should restrict equality saturation to closed exports:

```text
∅ ⊢ t : A.
```

That avoids prematurely solving the full problem of contextual equivalence,
substitution-map equivalence, and open-term congruence.

A stable typed e-node needs enough information to preserve:

- classifier/family;
- value/computation distinction where relevant;
- nominal declaration identity;
- typed child classes;
- target-independent semantic head/operator;
- context identity or a closed marker.

Process-local pointer values may accelerate hashing in-memory but must not define
serialized semantic identity.

## Plan

- [ ] Start with closed checked export roots.
- [ ] Key the e-graph by typed semantic occurrence.
- [ ] Include nominal family identity.
- [ ] Add a negative test: same erased Core, distinct classifier.
- [ ] Add a negative test: structurally identical, nominally distinct families.
- [ ] Extend to open terms only after context/map equivalence has a written
  contract.
- [ ] Completion criterion: no union is justified solely by equality of erased
  Core pointers.

---

# P5. Keep equality search outside the TCB; validate the extracted result

## Objective (Code)

The C backend is currently designed as a relatively dumb emitter downstream of
checked semantic input. It does not need to become a theorem prover.

That separation should be preserved.

## Assessment

Preferred architecture:

```text
source
  |
  v
elaboration / Solve
  |
  +--> typed roots
  +--> HOTT witnesses
  +--> DefEq facts
  +--> range/effect/totality facts
  |
  v
.a semantic artifact
  |
  v
optimizer / e-graph / search                 [untrusted]
  |
  v
candidate optimized root + explanation
  |
  v
small checker / translation validator        [trusted boundary]
  |
  v
checked optimized semantic root
  |
  v
C / CUDA C / RTL / interpreter realization
```

If the optimizer is wrong, the expected failure mode should be:

```text
candidate rejected
```

not:

```text
miscompiled program accepted.
```

This is especially important because equality saturation, extraction cost
models, profiling heuristics, and target-specific optimization are exactly the
parts most likely to evolve rapidly.

## Comparison: OptiSat

OptiSat explores verified equality saturation in Lean, including verified
e-graph components and extraction/certificate-checking approaches.

A Program need not start by verifying the entire search implementation.
Translation validation is a smaller and more modular first milestone.

## Comparison: CompCert

CompCert states compiler correctness in terms of preservation/refinement of
observable program behavior and composes pass-level simulations into a
whole-compiler theorem.

A Program is not currently proposing a CompCert-scale proof for every backend,
but the comparison clarifies the endpoint:

```text
backend correctness concerns program behavior,
not merely syntactic similarity.
```

Source DefEq is only one possible sufficient relation.

## Plan

- [ ] Keep optimizer/e-graph implementation outside the TCB.
- [ ] Require an explanation/certificate for the selected optimized root.
- [ ] Make validation failure an optimization failure.
- [ ] Keep emitters free from proof search.
- [ ] Later prove/check larger pass composition once local relations stabilize.
- [ ] Completion criterion: corrupt optimizer output cannot bypass the checker.

---

# P6. Do not equate "proof" with "erasable"

## Objective

A Program's current Identity direction is proof-relevant.

The C prototype already behaves more cautiously than a blanket proof-erasing
compiler: it requires actual reflexive structure for diagonal simplification and
does not erase arbitrary loops.

## Assessment

The rule:

```text
proof => erase
```

is invalid for general HOTT Identity.

Other proof assistants erase proofs because their source theories provide
specific irrelevance guarantees.

### Lean

Lean's `Prop` has definitional proof irrelevance and runtime irrelevance.
Compiled programs erase propositions/proofs under that contract.

This is not evidence that arbitrary A Program `Id` inhabitants are erasable.

### Agda

Agda exposes compile-time irrelevance and explicit runtime irrelevance. Values
marked `@0` / `@erased` are absent at runtime, and the type checker prevents
runtime computation from depending on them.

The lesson is explicit relevance discipline.

### Idris 2

Idris 2's Quantitative Type Theory assigns quantity `0` to variables that are
erased at runtime. Its backend IR explicitly contains `Erased` for compile-time
only values.

Again, erasure follows a static usage contract.

### Rocq / Coq extraction

Rocq extraction separates computational and logical content and can erase large
logical fragments. That architecture is valuable as a comparison, but its
erasability assumptions are not automatically compatible with proof-relevant
higher Identity.

## Proposed A Program direction

Introduce a separate property, eventually something like:

```text
RuntimeIrrelevant Γ x
```

or a target-observational theorem that states runtime behavior does not depend
on a value.

Potential future sufficient conditions include:

- explicit source irrelevance;
- restricted singleton/contractible structure;
- target-specific observational irrelevance;
- a checked dependency analysis under a relevance discipline.

None should be inferred only from an English classification as "proof".

## Plan

- [ ] Keep Higher Identity runtime-relevant by default.
- [ ] Design runtime irrelevance independently.
- [ ] Require erasure explanations.
- [ ] Add negative tests where a higher witness is consumed computationally.
- [ ] Completion criterion: no backend contains a generic "drop proof-typed
  field" rule.

---

# P7. Backend representation is a relation with conditions, not source equality

## Subjective (User)

A Program should be able to make aggressive target simplifications, including
using ordinary C integers for richer source naturals when the conditions make it
safe.

## Assessment

Consider:

```text
Nat -> uint64_t
```

This is not a source equality.

A source `Nat` is unbounded; a machine integer is finite. Overflow behavior,
pattern matching, representation, and target operations differ.

The correct judgement is closer to:

```text
Γ ⊢ n : Nat
Γ ⊢ n < 2^64
---------------------------------
Γ ⊢ n ~rep[C,uint64] encode(n)
```

and each operation has its own preservation condition.

For addition:

```text
a < 2^64
b < 2^64
a + b < 2^64
---------------------------------
encode(a + b)
  ≈target
uint64_add(encode(a), encode(b))
```

This architecture turns Solve results directly into optimization assets:

```text
Solve proves range bound
        |
        v
artifact stores bound
        |
        v
C profile chooses uint64 representation
```

Another backend can use the same source fact differently:

```text
CUDA C -> uint64_t / vectorized integer
RTL    -> exact-width logic [k-1:0]
```

## Comparison: Rocq extraction

Rocq permits custom extraction mappings for inductive types, including mappings
from source naturals to host integers, but this introduces the familiar
range/overflow responsibility.

A Program can improve on that engineering boundary by retaining the relevant
range facts and making the mapping conditional and checkable.

## Comparison: Idris 2 runtime representations

Idris 2 also demonstrates that a dependently typed source representation does
not force a naive runtime representation. Compiler representations can be more
efficient when the compiler has a static contract supporting them.

## Plan

- [ ] Define a representation-relation prototype separate from DefEq.
- [ ] Make target assumptions explicit.
- [ ] Let solved range/shape/irrelevance/effect facts feed representation
  selection.
- [ ] Never inject a representation relation into Kernel conversion.
- [ ] Completion criterion: aggressive target lowering is accepted only under
  checked conditions.

---

# P8. Persist semantic proof/equality assets; keep e-graphs entirely compiler-local

## Objective

The artifact semantic-persistence work already argues for semantic reachability
rather than preservation of all historical implementation state.

The equality design should follow the same rule.

## Assessment

Canonical `.a` should **not** serialize compiler search structures such as:

```text
union-find rank arrays
hash-cons buckets
all rewrite matches
all failed candidates
saturation iteration history
scheduler queues
profiling counters
wall-clock timings
every intermediate normalization state
```

Instead it should retain the semantic closure required by selected results:

```text
source semantic root
optimized semantic root
relation between them
explanation DAG
side-condition facts
HOTT witnesses referenced by the explanation
typed/context/nominal dependencies
representation profile if the relation is target-scoped
```

This gives an important anti-bloat invariant:

> Additional search effort does not enlarge the canonical artifact unless it
> produces additional retained semantic knowledge or changes the selected
> result/justification.

This is exactly the safeguard needed to avoid repeating earlier architecture
mistakes where process/history structure grew much larger than semantic content.

## Comparison: persistent compiler e-graphs

Merckx et al. (2026) identify a real compiler problem: equalities discovered at
one abstraction layer are often discarded as compiler passes move to another
representation. They propose keeping e-graph state in the compiler IR.

A Program should solve the same *knowledge-loss problem* but can choose a
different persistence boundary:

```text
transient e-graph implementation state  -> rebuildable
selected checked equality knowledge     -> persistent
```

This is more appropriate for a long-lived portable `.a` format.

## Suggested artifact separation

```text
Canonical semantic sections
    Core / semantic object graph
    typed occurrences
    contexts/maps/nominal declarations
    HOTT witness roots
    equality/relation facts
    selected explanation DAGs
    solved side conditions
    semantic declarations/results intentionally retained by the artifact

Compiler-local or external profile/cache state
    execution counters
    hotness
    equality-saturation cache
    search frontier
    profiling traces
    cost model measurements
```

The optional sections may be discarded without changing program meaning.

## Relation-fact sketch

Illustrative only:

```c
enum pg_relation_kind {
	PG_REL_DEFINITIONAL,
	PG_REL_IDENTITY,
	PG_REL_EQUIVALENCE,
	PG_REL_REPRESENTATION,
	PG_REL_TARGET_OBSERVATIONAL
};

struct pg_relation_fact {
	enum pg_relation_kind kind;

	uint64_t left_occurrence;
	uint64_t right_occurrence;

	uint64_t relation_descriptor;
	uint64_t witness_term;
	uint64_t explanation;
	uint64_t condition_set;
	uint64_t target_profile;
};
```

The real wire format should be derived from artifact identity requirements, not
copied from this sketch.

## Plan

- [ ] Add relation/equality assets to the artifact prototype.
- [ ] Use stable IDs, never serialized process pointers.
- [ ] Reachability-collect equality assets from selected semantic roots.
- [ ] Keep e-graph caches noncanonical.
- [ ] Add section-size metrics.
- [ ] Add zero-step load/resave stability tests.
- [ ] Add "large search, small selected explanation" tests.
- [ ] Completion criterion: search work and artifact size are not linearly
  coupled.

---

# P9. Make optimized semantic roots the interface to C/CUDA C/RTL

## Objective

The current C emitter benefits from being downstream of semantic checking rather
than embedding Solve.

## Assessment

Prefer a boundary conceptually like:

```c
int pg_optimize_export(
	const struct pg_artifact_view *artifact,
	const struct pg_backend_profile *profile,
	const struct pg_occurrence *source,
	struct pg_optimized_export *out);

int pg_check_optimized_export(
	struct pg_typing *typing,
	const struct pg_optimized_export *candidate);

int pg_c_emit_export(
	const struct pg_optimized_export *checked);
```

Names are illustrative.

`pg_optimized_export` should identify:

- source root;
- optimized root;
- classifier;
- relation used to justify replacement;
- explanation;
- side conditions;
- target/profile, if target-specific.

## Target-independent optimization

Examples:

- DefEq normalization;
- solved pure branch elimination;
- source-level specialization;
- equality-driven common subexpression choices;
- source-theory algebraic rewrites with checked premises.

These can be shared across C, CUDA C, and RTL.

## Target-specific representation optimization

Examples:

- fixed-width integer lowering;
- bit-width narrowing;
- proof-field erasure;
- closure layout;
- data flattening;
- target intrinsics.

These should carry a backend profile.

## Plan

- [ ] Add optimizer adapter before the C emitter.
- [ ] Do not teach the C emitter to perform equality search.
- [ ] Version representation profiles separately from source equality.
- [ ] Demonstrate one `.a` producing multiple legal target realizations.
- [ ] Completion criterion: source semantics remain unchanged when selecting a
  different backend.

---

# P10. Use hotness to allocate Solve and optimization budgets, but keep profiles nonsemantic

## Subjective (User)

The intended model is analogous to JIT compilers choosing frequently executed
regions first. Work that is not reused should not automatically receive the
same expensive compilation treatment as hot code.

## Assessment

This is where A Program can go beyond a conventional ahead-of-time compiler.

A profile can estimate:

```text
expected_future_invocations(root)
expected_direct_cost(root)
expected_compiled_cost(root)
expected_solve_gain(root)
expected_compile_cost(root)
```

The policy engine can then decide whether the next unit of computation should be:

- execute now;
- continue Solve;
- run equality saturation;
- specialize with known arguments;
- prove a range/shape/effect fact;
- extract a cheaper term;
- compile a target realization.

A generic decision rule is:

```text
expected future savings > cost of additional optimization.
```

For a candidate action `a`:

```text
Benefit(a)
  ≈ P(success | known facts)
    * expected_remaining_runs
    * expected_per_run_saving

Perform a when:

Benefit(a) > Cost(a) + RiskPenalty(a).
```

The exact heuristic can remain approximate. Soundness does not depend on its
quality because the resulting transformation is checked separately.

## Why profile data should not be canonical semantics

Invocation count is historical/environmental information. It changes across
machines and workloads.

Therefore:

```text
semantic equality fact          canonical candidate
range theorem                   canonical candidate
selected optimization proof     canonical candidate

"function ran 10,482 times"     noncanonical profile
"optimization took 7.3 ms"      noncanonical profile
"CPU model X was faster"        profile/target metadata
```

A `.a` file may carry optional profile sections, but deleting them must not
change meaning or proof status.

## Persistent JIT-like optimization

The distinctive A Program opportunity is:

```text
run 1:
    interpret .a
    collect hotness
    perform small Solve
    save useful semantic facts

run 2:
    reuse facts
    specialize hot root
    save checked optimized root

run 3:
    threshold crossed
    generate C/native realization
    cache/link realization

later:
    reuse .a equality assets even if native cache is invalidated
```

Native code can be architecture-specific and disposable while the semantic
optimization credit remains portable.

That is a much stronger persistence model than a traditional in-process JIT
code cache.

## Plan

- [ ] Design a noncanonical profile overlay keyed by stable semantic root IDs.
- [ ] Record invocation counts and cost samples outside core semantic identity.
- [ ] Allow profile-guided Solve/equality-saturation budgets.
- [ ] Make native realization caches invalidatable independently of equality
  assets.
- [ ] Add a benchmark comparing:
  direct `.a` execution, eager full compile, and adaptive compilation.
- [ ] Completion criterion: the adaptive policy wins or ties on total
  wall-clock cost over mixed one-shot/hot-loop workloads without affecting
  semantic acceptance.

---

# Critical regression cases

These cases should be considered architecture tests.

| Case | Knowledge obtained | Allowed optimizer action | Forbidden shortcut |
| --- | --- | --- | --- |
| beta redex | DefEq | same quotient class / reduce | recompute forever after persistence |
| eta-equivalent function, if Kernel DefEq | DefEq | union inside typed DefEq e-graph | treat arbitrary extensional theorem as Kernel DefEq |
| `p : Id A a b` | Higher Identity | retain witness edge/data | union endpoints solely due to `p` |
| nontrivial `p : Id A a a` | loop | preserve loop | replace by `refl` without computation |
| function Identity | higher function/path action | transport/action | identify function code pointers globally |
| same Core, different classifier | distinct judgements | keep distinct | raw-Core union |
| distinct nominal families | distinct semantic declarations | keep distinct | structural family merge |
| `StrongSorted -> LocalSorted` | transformation | retain map | call types definitionally equal |
| both-direction transformations + coherence | possible equivalence | record equivalence | silently upgrade to DefEq |
| `Nat -> uint64_t` with bound | representation relation | target lowering | source-type equality |
| proof field with explicit irrelevance | relevance fact | erase if checked | erase because "proof" |
| effectful rewrite | effect-sensitive relation | preserve order/effect conditions | use pure algebraic rewrite |
| cold one-shot root | profile fact | execute directly | mandatory expensive full compile |
| hot repeated root | profile fact | spend larger optimization budget | repeatedly interpret despite profitable cached compile |
| large saturation search, short selected proof | selected explanation | persist short explanation | serialize whole search history |

---

# Proposed execution and compilation architecture

The `.a` boundary is deliberately above optimization policy. The Transpiler may
consume proofs from `.a`, but `.a` does not prescribe how aggressively they are
used.

```text
                          ┌─────────────────────────┐
                          │      source program      │
                          └────────────┬────────────┘
                                       │
                                       v
                          ┌─────────────────────────┐
                          │ elaboration / initial   │
                          │ Solve                   │
                          └────────────┬────────────┘
                                       │
                   ┌───────────────────┼───────────────────┐
                   │                   │                   │
                   v                   v                   v
             typed roots         HOTT witnesses      solved facts
                                                        / DefEq
                   └───────────────────┼───────────────────┘
                                       v
                          ┌─────────────────────────┐
                          │     canonical .a        │
                          │ semantic artifact       │
                          └───────┬─────────┬───────┘
                                  │         │
                     direct run   │         │ optimization request
                                  │         v
                                  │  ┌───────────────────────┐
                                  │  │ e-graph/search/profile│
                                  │  │      untrusted        │
                                  │  └──────────┬────────────┘
                                  │             │
                                  │             v
                                  │  candidate + explanation
                                  │             │
                                  │             v
                                  │  ┌───────────────────────┐
                                  │  │ certificate / relation│
                                  │  │ checker               │
                                  │  └──────────┬────────────┘
                                  │             │
                                  │             v
                                  │  optimized semantic root
                                  │             │
                                  │   ┌─────────┼──────────┐
                                  │   │         │          │
                                  v   v         v          v
                             interpreter      C       CUDA C      RTL
```

The direct interpreter and backends consume the **same semantic program** at
different optimization/realization levels.

---

# Equality saturation policy

## Initial safe domain

The first prototype should be intentionally narrow:

```text
closed checked occurrences
+ DefEq-preserving equations
+ pure computations
+ explicit side-condition facts
```

Do not begin by saturating every object-language theorem.

## Rewrite classification

Every rewrite should declare:

```text
name
input judgement pattern
output judgement pattern
relation kind
required premises
effect assumptions
target scope
proof/explanation constructor
cost-model metadata
```

Example:

```text
rule beta
relation: DefEq
scope: source
premises: typing
proof: Kernel computation rule
```

versus:

```text
rule nat-add-u64
relation: Representation(C,uint64)
scope: C profile
premises:
    0 <= a
    0 <= b
    a + b < 2^64
proof:
    representation-preservation rule
```

These must never be stored in the same undifferentiated rewrite table.

---

# What Solve should persist

Solve may discover many facts. Persistence should be consumer-driven.

## Strong candidates for canonical semantic persistence

- exported object-language witnesses;
- solved classifier/formation facts needed to reconstruct checked roots;
- DefEq facts selected by optimizer/extractor;
- range/shape facts that justify retained representation relations;
- runtime-irrelevance facts used by retained optimized roots;
- effect/totality facts needed by retained transformations;
- selected relation explanations;
- pending semantic obligations intentionally exposed by the artifact model.

## Normally noncanonical

- work queues;
- unsuccessful candidates;
- raw memo tables;
- union-find parent/rank;
- evaluator stack snapshots;
- wall-clock timings;
- instruction counters;
- hotness counters;
- target-native machine code.

Native code may be separately cached, but its cache validity is not source
semantic validity.

---

# Artifact monotonicity: define carefully

A tempting slogan is:

```text
more Solve => better artifact
```

but this should **not** mean bytewise monotonic growth.

A better semantic statement is:

> Spending additional sound Solve/optimization work may add reusable facts or
> replace a selected realization with one justified to be no worse under the
> active cost model; it must not invalidate previously accepted source
> semantics.

Canonicalization is allowed to delete redundant explanations, subsumed facts,
or obsolete optimized roots.

Therefore monotonicity is semantic, not append-only storage.

---

# Relationship to staged computation and partial evaluation

A Program's model overlaps with staging and partial evaluation but is not
identical to either.

Kovács's two-level type theory work is relevant because it treats compile-time
evaluation as a semantics-preserving staging operation in a dependently typed
setting.

For A Program, the important analogy is:

```text
static/Solve knowledge
        |
        v
residual executable program
```

The difference is persistence and adaptivity:

- A Program may stop staging early and execute the artifact.
- More staging/Solve can occur later.
- Equality assets can survive across executions.
- Target lowering can be selected after source-level semantic work.
- Profiling can influence where more static work is spent.

Recent work on deriving compilation by partial evaluation of interpreters is
also conceptually relevant: it reduces semantic duplication between an
interpreter and compiler. A Program should likewise resist having an
interpreter, optimizer, and each backend independently encode different versions
of source equality semantics.

The semantic artifact and its checked relation system should remain the shared
source of truth.

---

# TCB proposal

## TCB candidates

- Core semantic constructors/invariants;
- type/classifier checking required by accepted judgements;
- conversion checker;
- HOTT computation/action rules that are part of the source theory;
- equality explanation checker;
- representation-relation checker definitions for admitted backend profiles;
- artifact integrity/identity checks required for semantic reconstruction.

## Explicitly outside the TCB

- e-graph scheduling;
- rewrite-search strategy;
- extraction heuristics;
- cost models;
- profiling;
- hotness thresholds;
- compiler tier policy;
- optional SMT/solver used only to propose certificates;
- target code selection before validation.

This separation should be documented before optimization grows large.

---

# Implementation phases

## Phase A — Specification only

- [ ] Merge equality taxonomy.
- [ ] Merge adaptive-compilation/JIT cost model.
- [ ] Define canonical versus profile/cache artifact state.
- [ ] Write critical regression tests as expected semantics, even if some are
  initially skipped.

## Phase B — Equality assets without e-graph

- [ ] Add stable relation-fact IDs.
- [ ] Persist selected DefEq facts/explanations.
- [ ] Reload and validate them.
- [ ] Use one retained equality fact to eliminate redundant recomputation before
  C emission.

Success condition: Solve knowledge survives artifact round-trip and changes
later compile cost without changing semantics.

## Phase C — Closed typed e-graph prototype

- [ ] Closed checked occurrences only.
- [ ] DefEq/pure rewrite subset only.
- [ ] Proof-producing extraction.
- [ ] No HOTT endpoint union.
- [ ] No target representation relation in the same congruence.

Success condition: a nontrivial source term is optimized, the extracted root is
validated, and an intentionally corrupted explanation is rejected.

## Phase D — Representation relations

- [ ] Introduce target profile descriptors.
- [ ] Implement one range-conditioned integer lowering.
- [ ] Keep source artifact semantics unchanged.
- [ ] Emit at least two target representations from one source artifact.

## Phase E — Adaptive execution

- [ ] Add optional execution profile overlay.
- [ ] Implement a simple hotness threshold.
- [ ] Compare:
  - interpret always;
  - compile always;
  - adaptive policy.
- [ ] Persist semantic optimization credit independently of profile/native cache.

## Phase F — Wider equality

Only after the previous boundaries are stable:

- [ ] algebraic laws with premises;
- [ ] effect-sensitive equations;
- [ ] open terms/contexts;
- [ ] equivalence-aware transformations;
- [ ] richer higher-dimensional optimization if a concrete use demands it.

---

# Non-goals

This proposal does **not** require:

- converting all HOTT Identity to DefEq;
- serializing complete e-graphs;
- proving the e-graph implementation correct before experimentation;
- requiring full Solve before execution;
- requiring full native compilation before execution;
- erasing every proof;
- forcing all backends to share one runtime representation;
- making profile data part of program meaning;
- selecting one globally optimal optimization strategy.

---

# Architectural invariants

These should become review rules.

1. **Higher Identity is not global DefEq.**
2. **Typed judgement identity is not raw Core pointer identity.**
3. **Representation equality is not source equality.**
4. **Proof is not synonymous with runtime irrelevant.**
5. **Optimization search is not trusted merely because it is expensive.**
6. **Selected optimization results require a checkable relation.**
7. **Canonical artifact size is driven by reachable semantics, not elapsed
   search history.**
8. **Profiling affects policy, not source truth.**
9. **Backends consume semantic facts; they do not invent source equality.**
10. **Compilation effort should be amortized against expected future execution.**
11. **`.a` remains a legitimate executable artifact even without a native
    realization.**
12. **Native code is a target-scoped realization/cache of semantics, not the
    canonical definition of the program.**
13. **The existence of an equality proof does not mandate its use in
    transpilation.**
14. **An e-graph is a Transpiler implementation technique, not an `.a`
    semantic concept.**
15. **Optimization-search time is part of the total computation cost and may
    rationally be skipped.**
16. **LinkerManuscript selects a realization policy; it does not alter source
    truth.**

---

# Consequence for the identity of `.a`

The design converges on a useful definition:

> A `.a` artifact is a persistent semantic program state consisting of checked
> semantic roots plus reachable semantic knowledge that may justify future
> computation, optimization, specialization, or target realization.

It is **not** merely:

- a source archive;
- a fully linked executable;
- a dump of the current evaluator heap;
- a theorem prover checkpoint;
- an e-graph snapshot;
- a conventional object file.

This explains why both of the following should be normal:

```text
a run foo.a
```

and:

```text
a compile --target=c --opt=full foo.a
```

The first spends little up-front compilation work.

The second deliberately spends more computation because the user or adaptive
policy expects the cost to be amortized.

A future hybrid runtime can make the same choice per exported root or even per
hot subregion.

---

# Why the equality artifact is central to adaptive compilation

A conventional JIT usually accumulates runtime profile information and emits
optimized machine code.

A Program can accumulate something more reusable:

```text
profile evidence             ephemeral / workload-specific
        |
        v
Solve / proof search
        |
        v
semantic facts               portable
        |
        v
equality/representation asset portable or target-scoped
        |
        v
native realization           architecture-specific / disposable
```

This ordering matters.

If machine code is invalidated because the CPU, ABI, backend version, or target
changes, a solved theorem such as

```text
n < 2^32
```

does not necessarily disappear.

The next backend can reuse that theorem.

Thus the unit of long-term optimization investment should be **semantic
knowledge**, not only generated code.

This is the central reason equality belongs in `.a`.

---

# Research and implementation references

## Higher Observational Type Theory

1. Thorsten Altenkirch, Ambrus Kaposi, Michael Shulman.
   **Towards Higher Observational Type Theory.**
   TYPES 2022 / LIPIcs.
   https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf

2. Thorsten Altenkirch, Yorgo Chamoun, Ambrus Kaposi, Michael Shulman.
   **Internal Parametricity, without an Interval.**
   POPL 2024. DOI: 10.1145/3632920.
   https://arxiv.org/abs/2307.06448

3. Narya documentation.
   **Observational higher dimensions.**
   https://narya.readthedocs.io/en/latest/observational.html

4. Narya documentation.
   **Higher Observational Type Theory.**
   https://narya.readthedocs.io/en/latest/hott.html

## Observational equality

5. Thorsten Altenkirch, Conor McBride.
   **Towards Observational Type Theory.**
   Historical precursor for separating conversion and observational equality.

## Equality saturation

6. Max Willsey, Chandrakana Nandi, Yisu Remy Wang, Oliver Flatt,
   Zachary Tatlock, Pavel Panchekha.
   **egg: Fast and Extensible Equality Saturation.**
   POPL 2021.
   https://arxiv.org/abs/2004.03082

7. Jules Merckx, Alexandre Lopoukhine, Samuel Coward, Jianyi Cheng,
   Bjorn De Sutter, Tobias Grosser.
   **E-Graphs as a Persistent Compiler Abstraction.**
   2026.
   https://arxiv.org/abs/2602.16707

8. OptiSat project.
   Verified equality-saturation work in Lean.
   https://github.com/lambdaclass/truth_research

## Verified compilation

9. Xavier Leroy et al.
   **CompCert.**
   Current commented development and semantic-preservation documentation:
   https://compcert.org/doc/
   https://compcert.org/

## Runtime irrelevance and erasure comparisons

10. Lean reference manual.
    **Propositions.**
    `Prop` has definitional proof irrelevance and runtime irrelevance.
    https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/

11. Agda documentation.
    **Run-time Irrelevance.**
    https://agda.readthedocs.io/en/latest/language/runtime-irrelevance.html

12. Agda documentation.
    **Irrelevance.**
    https://agda.readthedocs.io/en/latest/language/irrelevance.html

13. Idris 2 documentation.
    **Multiplicities.**
    https://idris2.readthedocs.io/en/latest/tutorial/multiplicities.html

14. Idris 2 documentation.
    **Custom backend cookbook / `Erased`.**
    https://idris2.readthedocs.io/en/latest/backends/backend-cookbook.html

15. Rocq / Coq extraction documentation.
    Program extraction and custom runtime representations are relevant comparative
    material. The current manual documents user-supplied target realizations, while
    `ExtrOcamlNatInt` explicitly warns that bounded host integers are not a certified
    drop-in representation for unbounded `nat`. A Program should independently
    prove/check its representation preconditions rather than importing Rocq's
    logical/computational boundary.
    https://docs.rocq-prover.org/master/refman/addendum/extraction.html
    https://rocq-prover.org/doc/master/stdlib/Stdlib.extraction.ExtrOcamlNatInt.html

## Adaptive/JIT compilation

16. Oracle HotSpot documentation.
    **Compilation Optimization / Tiered Compilation.**
    HotSpot interprets initially and JIT-compiles hot methods; compilation tiers
    trade startup cost against stronger steady-state optimization.
    https://docs.oracle.com/javacomponents/jrockit-hotspot/migration-guide/comp-opt.htm
    https://docs.oracle.com/en/java/javase/17/vm/java-hotspot-virtual-machine-performance-enhancements.html

17. Matthew Arnold, Stephen Fink, David Grove, Michael Hind, Peter F. Sweeney.
    **Adaptive Optimization in the Jalapeño JVM.**
    OOPSLA 2000.
    IBM Research publication page:
    https://research.ibm.com/publications/adaptive-optimization-in-the-jalapeno-jvm--2

## Staging / partial evaluation

18. András Kovács.
    **Staged Compilation with Two-Level Type Theory.**
    https://arxiv.org/abs/2209.09729

19. Chris Fallin, Maxwell Bernstein.
    **Partial Evaluation, Whole-Program Compilation.**
    PLDI 2025.
    Relevant as a modern example of deriving compilation behavior from an
    interpreter/semantic source of truth rather than duplicating semantics
    across independent tiers.

---

# Recommended PR framing

Suggested PR title:

```text
doc: define equality-bearing artifacts and adaptive compilation model
```

Suggested summary:

```text
This PR documents the intended equality architecture before adding equality
saturation or stronger transpiler optimizations.

It distinguishes Kernel DefEq, Higher Observational Identity, semantic/type
equivalence, backend representation relations, and target observational
equivalence. It proposes persisting selected checked equality knowledge in `.a`
without serializing full Solve/e-graph history.

The document also makes explicit the older execution-model goal behind `.a`:
A Program artifacts should be directly executable, while additional Solve,
optimization, and native compilation are adaptive investments whose cost should
be amortized over expected repeated execution, analogous to tiered/JIT
compilation.

No Kernel or wire-format change is made by this documentation PR.
```

Suggested first implementation issue after the documentation PR:

```text
prototype: persist one checked DefEq asset and reuse it before C emission
```

The first implementation should deliberately avoid a general e-graph. Proving
that one solved equality can survive `.a` round-trip and eliminate later work
will validate the most important architectural boundary with minimal new code.

</details>
