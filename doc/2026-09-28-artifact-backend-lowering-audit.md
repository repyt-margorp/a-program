# Implementation request: Artifact-to-C transpilation behind a semantic boundary

Date: 2026-09-28
Fresh baseline: `main` at `e7162320712f1acdc9420b6ffae993cd97035663` (independent clone under a-book; no implementation edits).
Status: proposal/review, not implemented by this submission.

## Problem List

1. P1 — Introduce Artifact-based transpilation with C as the first target, keeping semantic identity, trust and unfinished obligations independent of backend details.

## P1. Semantic snapshot and first C realization

### Subjective (User)

English paraphrase of the 2026-09-28 request: open an implementation issue for transpilation from Artifact; implement C output first. The supplied backend audit contains the detailed design motivation.
Historical user attributions in the supplied document are preserved as reported provenance; individual API/IR names are proposals, not newly approved choices.

### Objective (Code)

- `src/source_io.h` defines APGSRC62 source reconstruction and APGSRC63 optional retained reductions. Loading itself does not advance Solve; source synthesis normally runs again afterward.
- The system is not replay-only in the sense of preserving no computation progress: `--retain-reductions` preserves completed reductions and partial NF records. `src/eval.h` exposes reduction receipts/checked imports. These do not amount to a complete solver checkpoint or a universal semantic-export API.
- `src/occurrence_io.h` transports APGOCC7 and scoped APGSCP1 descriptive inputs without granting evidence. It exposes name/resolve callbacks, not the full descriptor-codec variants proposed in the report.
- `src/retained_io.h` likewise distinguishes raw/unaccepted records from admission.
- `src/main.c` exposes checking, image save/load, normalization and host execution; the inspected CLI has no C emission option.
- `src/classifier.h`, `src/iadt.h` and `src/host.h` already separate computation contracts, nominal declaration/runtime layout, and fixed machine types.

Fresh checks:
- `make -s -j3 check-execution`: pass, including source/image runs and effect behavior.
- `bash tests/occurrence_io.sh build/pointer/occurrence_io_test`: pass.
- `bash tests/typed_structure.sh build/pointer/typed_structure_test`: pass for typed-only and scoped image rechecking.
The two test binaries were built via their absolute Make targets under build/pointer.
Initial bare test-binary invocations lacked required arguments and asserted; the supported shell runners passed. An initial relative Make target was also invalid; these were invocation errors, not language defects.
No full acceptance suite or generated-C test was run; no emitter was implemented.

### Assessment

Implement a narrow backend-independent snapshot/export boundary before coupling an emitter to compiler internals. Preserve materialized structure, available checked facts and explicit residual obligations. Backends must not traverse syntax/synthesis worklists as their ABI.

Lowering eligibility is per selected export/profile, not equivalent to whole-Program Solve completion. However, partial source structure is not accepted typing evidence: missing representation/effect/semantic information requires NOT_READY, an explicitly defined residual-runtime mode, or an explicit conditional contract. No silent admission of unfinished/rejected input.

Rechecking serialized evidence is not the same as rerunning open-ended search. Reuse existing typed transport and retained-reduction mechanisms before inventing another certificate hierarchy. Preserve their trust boundaries.

C is the first implementation target. A minimal exact path for selected supported exports should precede aggressive Nat compression, CUDA or hardware lowering. Those later routes must not block a useful first C emitter.

Record separately:
- source semantic verification status;
- target support/materialization frontier;
- lowering correctness (exact, conditional, intentionally approximate);
- assumptions and their discharge state.

A runtime check that traps when a Nat bound is exceeded is not an exact realization of unbounded Nat on those inputs. Exact fallback may preserve full semantics; defined failure alone does not. C overflow must never be introduced accidentally, even in approximate modes.

Resolve profile selections to nominal identities, not merely a declaration named Nat. Preserve generativity when two types share runtime representation.
Memory ownership, closure/call representation, effect ordering, fixed-width arithmetic and unsupported operations need explicit runtime/ABI contracts.
Future recurrence lowering must preserve its chosen divergence/trace semantics, but the first C subset need not wait for the recursion issue.

Prior-art spot-check: MLIR separates legality, rewrite patterns and type conversion, and distinguishes full/partial conversion. Partial conversion is NOT a theorem about compiling unverified A Program input; the analogy is architectural only. [MLIR dialect conversion](https://mlir.llvm.org/docs/DialectConversion/).
The rest of the supplied literature survey is historical and not exhaustively reverified here.

### Plan

- [ ] Specify a minimal semantic snapshot API and selected-export lowerability query, with explicit residual obligations and no mandatory further Solve.
- [ ] Reuse/extend typed transport and descriptor codecs to cover nominal semantic exports; validate imports rather than trust serialized status flags.
- [ ] Keep canonical semantics/backend dependencies one-way; target ABI names/layout/configuration belong downstream.
- [ ] Define the initial supported subset and runtime contract, then emit compilable C using exact structural representations.
- [ ] Add a profile/manifest recording verification status, lowering conditions, unsupported constructs and target contract.
- [ ] Differential-test constants, integer arithmetic, print/effect ordering, closures/calls and inductive match/recursion against the existing evaluator.
- [ ] Test completed and pending images; a sufficiently materialized export must lower without further Solve, while insufficient or rejected material fails explicitly.
- [ ] Test descriptor round-trips, distinct nominal families, overflow boundaries, unsupported effects and runtime failure behavior.
- [ ] Only afterward prototype bounded Nat representation with whole-reachable-value invariants and explicit prove/check-fallback/assume policy; approximation must be opt-in.
- [ ] Defer CUDA/Verilog to separate downstream legalization work.

Completion: Artifact input produces buildable C for a documented subset, behavior agrees with the evaluator under its stated contract, residual obligations are not concealed, and backends do not become owners of canonical artifact semantics.

## Supplied research record and precedence

Tracking issues: #43 (recursion) and #44 (Artifact-to-C).
The current-review addendum above takes precedence over unpinned or broader historical statements below. Original text is preserved; its proposed names, pseudocode and phase ordering are not accepted implementation. Historical user attributions remain attributed to the supplied record. Original SHA-256: `bf9c69528648acd8f021721e677aca2f37e2e150cda64a791a5296f77bd1f7c4`.

## Original supplied document

# Artifact and Backend Lowering Refactoring Audit

Date: 2026-09-28  
Status: in progress  
Code baseline: `main` as served by GitHub on 2026-09-28. The exact commit SHA could not be pinned in this environment because local `git clone` failed at DNS resolution; replace this note with `git rev-parse HEAD` before implementation or committing this audit.  
Related: current Program-image implementation in `src/source_io.*`; typed transport in `src/occurrence_io.*` and `src/context_io.*`; nominal data representation in `src/iadt.*`; graph descriptors in `src/graph_io.*`, `src/descriptor_io.*`, and `src/declaration_io.*`.

## Problem List

| ID | Problem | Issue / PR | Status |
| --- | --- | --- | --- |
| P1 | The current `.a` Program image is a source/synthesis replay image, not a clean backend-facing semantic artifact. | none | audited |
| P2 | A verified, backend-independent semantic boundary is missing. | none | planned |
| P3 | Backend lowering must remain subordinate to artifact semantics and must not feed target constraints back into the canonical artifact. | none | design proposed |
| P4 | A Program needs an explicit mechanism for aggressive representation lowering, including conditional mappings such as an unbounded `Nat`-shaped type to a bounded machine integer. | none | design proposed |
| P5 | Exact, conditional, and intentionally approximate lowering need separate correctness contracts and provenance. | none | design proposed |
| P6 | C, CUDA C, and Verilog need a common target-independent lowering boundary, followed by target-specific legalization. | none | design proposed |
| P7 | Nominal declarations need a stable way to be selected by lowering profiles without making source names or backend names part of semantic identity. | none | planned |
| P8 | Solve completion must not be a prerequisite for lowering: a partially solved `.a` must preserve settled semantic progress and explicit residual obligations so it can be lowered without advancing Solve further. | none | design proposed |

---

## P1. Separate the Program replay image from the semantic artifact

### Subjective (User)

- User requirement, translated/paraphrased from the 2026-09-28 discussion: the artifact should be designed first, and backend implementations must follow that design. Backend implementation constraints must not force changes to the semantic artifact.
- User requirement, translated/paraphrased from the 2026-09-28 discussion: future compilation/transpilation to C, CUDA C, and Verilog should be considered now, before a backend accidentally becomes the de facto owner of the artifact representation.
- User requirement, translated/paraphrased from the 2026-09-28 discussion: the system should preserve the ability to represent the full A Program semantics even when a particular backend chooses a much smaller or more conventional computation model.

### Objective (Code)

- `src/source_io.h` describes `APGSRC62`/`APGSRC63` as **reconstructible source environments**. It retains syntax, scopes, producer dependencies, prepared annotations, source declaration allocations, context-binding records, Match/Handler source state, and derivation inputs.
- `src/source_io.h` explicitly states that stored contexts and derivation inputs are not accepted typing evidence, that source types and branches are synthesized again, that no search state or acceptance flag is retained, and that ordinary `Solve` recomputes solutions after loading.
- `src/program.h` shows that `struct pg_program` owns graph, typing, evaluation, synthesis, imported effect inference, retained reductions, parser state, exports, source scope, and a synthesis root. This is intentionally a live compiler/program owner, not a minimal executable IR.
- The current README states that a `.a` image may be saved before `Solve` finishes and that loading uses ordinary `Solve`; stored completion claims confer no authority.
- Therefore the current `.a` format is correctly described as a **Program image / replay image**, not as a backend ABI.

### Assessment

The current replay model is valuable and should not be deleted. It supports pending programs, rechecking, source reconstruction, and the current trust model.

The problem is only its position in the architecture. A backend should not consume `pg_program`, `pg_synthesis_job`, source scopes, syntax nodes, or scheduler state directly. If the first C backend is implemented by walking these structures, they will become accidental backend ABI. Later artifact refactors would then be constrained by "the C backend already depends on this field", which is exactly the dependency direction the user wants to prevent.

The first architectural change should therefore be a **one-way semantic snapshot/export boundary**. Importantly, this boundary must exist **before Solve completion**:

```text
source / Program image
        |
        v
parse / synthesis / zero or more Solve steps
        |
        v
Partial Semantic View
(settled facts + materialized structure + residual obligations)
        |
        +------> continue Solve
        |
        +------> target-independent lowering
                     |
                     +------> C
                     +------> CUDA C
                     +------> hardware lowering ----> Verilog
```

A backend must never traverse upward from the Partial Semantic View into `source_io`, syntax, solver queues, or synthesis internals.

A fully verified semantic view remains a useful **stronger state** of the same interface, but it must not be the only state accepted by lowering.

### Plan

- [ ] Introduce a read-only in-memory semantic snapshot API, tentatively `pg_semantic_snapshot` / `pg_semantic_module`.
- [ ] Allow this API to be constructed after any finite number of Solve steps, including zero, provided the selected roots have enough materialized structure for the requested lowering profile.
- [ ] Represent verified/accepted facts as a stronger subset of the snapshot, not as a prerequisite for snapshot construction.
- [ ] Do not expose `struct pg_program`, `struct pg_synthesis_job`, parser objects, syntax nodes, or source scopes through the backend-facing API.
- [ ] Add an include/dependency test: backend modules must not include `source_io.h`, `syntax.h`, `synthesis.h`, or `program.h`.
- [ ] Keep Program-image loading and pending replay as a separate responsibility.
- Completion: a backend can enumerate selected semantic roots, all materialized executable structure, settled classifiers/types/contexts where available, referenced semantic objects, and explicit residual obligations without observing frontend scheduling or source-replay structures.

---

## P2. Promote typed semantic transport instead of inventing a backend-specific artifact

### Subjective (User)

- User requirement, translated/paraphrased from the 2026-09-28 discussion: do not redesign the artifact around what C, CUDA, or Verilog happens to find convenient.
- User preference from the same discussion: preserve the term/semantic graph as the primary representation; source names and contexts may exist around it, but should not become target-specific compiler metadata.

### Objective (Code)

- `src/typing.h` defines `struct pg_occurrence` as a **descriptive typed structure, not acceptance evidence**. It carries:
  - judgement,
  - context,
  - Core,
  - classifier,
  - retained type occurrence,
  - annotation,
  - origin/selection,
  - context map,
  - induction allocation,
  - ordered operands and selected construction maps.
- `src/occurrence_io.h` already serializes `APGOCC7`, retaining contexts, Core, classifiers, annotations, structural maps, selected construction inputs, and recursive-elimination allocation.
- `src/context_io.h` already transports contexts and extra Core roots, and has both name/resolve and full `pg_graph_codec` descriptor variants.
- `src/declaration_io.c` supplies `pg_declaration_graph_codec`, adding `"data-declaration/v3"` transport on top of the built-in graph descriptor codec.
- `src/occurrence_io.h`, unlike `context_io.h`, currently exposes only name/resolve callbacks rather than a full descriptor-codec form.

### Assessment

The repository already contains most of the substrate needed for a backend-independent semantic artifact. The correct first move is not to create a `c_ir` and then retrofit the artifact around it. It is to **promote the existing typed occurrence/context transport into the semantic boundary**, while also allowing that boundary to carry partially settled structure and residual obligations.

One concrete blocker is descriptor completeness. A semantic artifact containing arbitrary nominal data declarations must transport the same owner-specific descriptor payloads that `source_io` can already carry. The occurrence transport should therefore support the same full graph codec path as context/declaration transport.

This refactor is upstream of all backends and is justified by semantic completeness, not by C convenience.

### Plan

- [ ] Add full-descriptor variants to occurrence transport, analogous to `pg_contexts_write_descriptors` / `pg_contexts_read_descriptors`.
- [ ] Consolidate graph/declaration descriptor ownership so a semantic export can carry `"data-declaration/v3"` and future semantic owners without backend code knowing those payload formats.
- [ ] Define a small `semantic_export` record that points to a materialized semantic root, any settled typed occurrence information currently available, explicit residual obligations, and a logical export identity.
- [ ] Keep acceptance evidence distinct from descriptive transport; loading a semantic artifact must not create authority merely because a record exists.
- [ ] Reuse ordinary verification/checking to obtain the backend-facing verified view.
- Completion: a verified semantic export round-trips through artifact transport, including nominal declarations, without source/synthesis reconstruction being required by the backend.

---

## P3. Make backend subordination an explicit architectural invariant

### Subjective (User)

- User requirement, translated from 2026-09-28: **"The backend implementation must remain subordinate to the artifact design."**
- User requirement, translated/paraphrased: avoid the future dilemma where artifact design cannot be changed because existing backend implementation details depend on it.
- User requirement, translated/paraphrased: C, CUDA C, and Verilog are realizations of A Program semantics, not co-authors of those semantics.

### Objective (Code)

- Core currently has only Lambda, Application, and Reference node kinds; typed occurrences, contexts, substitutions, and derivations remain above Core.
- `src/graph_io.h` uses graph-local relocation identities; these are transport identities, not C symbol names, CUDA symbols, or Verilog module names.
- `src/host.h` already makes an important backend-independent distinction: machine contracts such as `#Int32`/`#Int64` are fixed semantic contracts, **not C's implementation-dependent `int`/`long` widths**.
- `src/iadt.h` explicitly separates an erased `pg_data_layout` from a full nominal `pg_data_declaration`. The erased layout records runtime constructor arity; it is not the positivity/index/type schema.

### Assessment

These existing separations should become hard invariants.

The canonical artifact may contain A Program concepts such as:
- Core terms,
- nominal declarations,
- contexts,
- typed occurrences,
- semantic object descriptors,
- effect information,
- verification/recheck material,
- optional source/replay metadata.

It must not acquire fields merely because a target wants them, such as:
- C ABI struct layout,
- a C identifier spelling,
- CUDA grid/block sizes,
- CUDA address spaces solely as a device implementation choice,
- Verilog clock frequency,
- pipeline stage count,
- register/wire choice,
- BRAM/DSP mapping,
- target calling convention.

If A Program itself later gains a semantic concept of parallelism, memory region, timing, or resource, that concept may belong in the canonical artifact because it is part of the language semantics. A CUDA or Verilog representation of that concept still belongs downstream.

A backend may motivate a canonical-artifact change only by demonstrating a **semantic incompleteness**: information required to recover or verify the A Program meaning is missing. "The backend would be easier if this field existed" is insufficient.

### Plan

- [ ] Document the invariant in the artifact module headers and backend design document.
- [ ] Make the canonical-artifact module buildable and testable with every backend disabled.
- [ ] Require canonical artifact bytes/round trips to be independent of whether C, CUDA, or Verilog support is compiled into the tool.
- [ ] Store target-specific configuration in a separate lowering/realization profile or derived artifact.
- [ ] Add dependency checks so canonical artifact code does not include target backend headers.
- Completion: removing every backend from the build leaves artifact read/write/verify behavior unchanged.

---

## P4. Add conditional representation lowering for aggressive compilation

### Subjective (User)

- User requirement, translated/paraphrased from 2026-09-28: under an explicit option, a `Nat`-like value should be allowed to become an ordinary C `int` rather than preserving the literal inductive representation.
- User requirement, translated/paraphrased: A Program semantics can be redundant for execution in another computation model. It should be possible to discard selected "branches and leaves" of the full semantic structure when that enables a dramatically simpler representation.
- User requirement, translated/paraphrased: this aggressive lowering must not redefine the canonical artifact; it is a deliberate realization choice.

### Objective (Code)

- The current README's `Nat` example is an ordinary generative inductive declaration:
  `Nat := @{ zero : *; succ : * -> *; };`
  It is not described as a built-in primitive natural-number type.
- `src/iadt.h` makes nominal declarations generative. Two declarations with the same erased constructor arities are still distinct nominal families.
- `src/iadt.h` already exposes an erased runtime layout separately from the complete declaration schema.
- `src/host.h` provides target-independent fixed machine contracts, demonstrating that the codebase already distinguishes mathematical/source structure from compact machine representation.
- A direct `Nat -> C int` mapping is not generally semantics-preserving: mathematical naturals are unbounded, while C `int` is bounded and its width is implementation-dependent; signed overflow is undefined behavior in C.

### Assessment

This feature should be implemented, but it should **not** be a C-backend special case.

The correct abstraction is a target-independent **representation lowering profile**. For example:

```text
nominal Nat-like family
        |
        | representation lowering
        | invariant: 0 <= n <= LIMIT
        | correctness: exact / conditional / approximate
        v
bounded machine natural/integer
        |
        +---- C backend ------> int / int32_t / uint32_t
        +---- CUDA backend ---> device integer
        +---- RTL lowering ---> logic [W-1:0]
```

The profile selects a specific nominal declaration and states how it may be represented. The backend merely materializes that already-chosen representation.

For a unary `Nat` representation:

```text
zero       -> 0
succ n     -> n + 1
match n
  zero     -> test n == 0
  succ k   -> nonzero branch with k = n - 1
```

Recursive eliminations can then become loops or lower-level control flow. Further rewrites may turn a loop into arithmetic, but that is a separate optimization and needs its own correctness argument.

#### Do not recognize `Nat` by spelling alone

Because data declarations are generative, a lowering profile must identify the exact nominal family selected from an artifact/export. A source-level name may be used as a user-facing selector, but it must resolve to a particular nominal declaration before lowering. Two structurally identical `zero/succ` declarations remain semantically distinct even if both are represented by the same machine integer type.

#### Do not use C `int` as the target-independent representation

The portable lowered form should describe a machine integer contract such as:

```text
machine_integer {
    width = 32;
    signed = true;
    invariant = 0 <= value <= 2147483647;
}
```

The C backend may choose literal `int` only when its target ABI contract establishes the required width/range. Otherwise it may use `int32_t`, `uint32_t`, or reject that realization. CUDA C can make the analogous choice. Verilog can use a fixed-width bit vector.

This keeps C's ABI out of the artifact and out of the target-independent lowering semantics.

#### The range condition must cover intermediate values

A profile that says only "inputs fit in `int`" is insufficient. Every reachable value that is represented by the bounded carrier must remain within the invariant, including:
- constructor results,
- recursive intermediates,
- arithmetic intermediates,
- values stored in data structures,
- values crossing function boundaries.

For an unchecked C signed `int` realization, the contract must additionally establish that no generated signed operation overflows. Otherwise the generated program may enter C undefined behavior.

### Plan

- [ ] Introduce a target-independent representation-lowering layer after the Verified Semantic View.
- [ ] Define a generic bounded-integer representation in the lowered IR; do not encode it as "C int".
- [ ] Add a profile mechanism that selects a nominal declaration and verifies that the requested representation rule applies to that declaration.
- [ ] Implement a `Nat`-shape rule as a first prototype: no parameters/indices, one nullary constructor, one constructor with one direct recursive field, plus the required checked semantic conditions.
- [ ] Lower constructors and elimination structurally to integer operations/control flow.
- [ ] Make profile assumptions explicit and separately versioned from the canonical artifact.
- [ ] Add overflow/range negative tests that intentionally violate the profile.
- Completion: the same canonical artifact can be lowered with either the full inductive runtime representation or the bounded-integer representation without changing or regenerating the canonical artifact.

---

## P5. Distinguish exact, conditional, and approximate lowering

### Subjective (User)

- User requirement, translated/paraphrased from 2026-09-28: it should be legal to deliberately discard some of the full semantic structure when another computation model can execute the useful computation much more simply.
- The request explicitly allows bold assumptions under appropriate options; therefore the compiler must represent the status of those assumptions instead of silently presenting every output as fully semantics-preserving.

### Objective (Code)

- Current A Program artifact loading is deliberately conservative: stored descriptions do not become accepted evidence merely because they were serialized.
- `src/typing.h`, `src/context_io.h`, and `src/occurrence_io.h` repeatedly distinguish descriptive structure from acceptance evidence.
- That existing discipline would be undermined if a backend silently converted an unbounded semantic object into a bounded target representation without recording the lost guarantee.

### Assessment

Lowering should use three correctness classes.

#### 1. `exact`

No additional user assumption is required for the observable A Program behavior represented by the compilation unit.

Examples:
- ordinary control-flow lowering,
- closure conversion with a semantics-preservation argument,
- an optimized Nat representation with arbitrary-precision fallback,
- erasure of typing-only data after an established erasure criterion,
- elimination of a constructor branch proven unreachable by accepted typing/index information.

`exact` does not mean "the representation looks the same". Lean's runtime treatment of `Nat` is a useful precedent: the logical type is inductive while compiled execution uses an efficient integer representation and arbitrary-precision fallback, preserving unbounded semantics.

#### 2. `conditional`

The generated target program refines the A Program program **if a declared assumption holds**.

Subclasses should be visible:
- `conditional:proved`: the compiler discharged the condition from accepted program facts or a checked analysis certificate.
- `conditional:checked`: the generated program enforces the condition at runtime and uses an exact fallback or another defined recovery path when it fails.
- `conditional:assumed`: the user explicitly authorizes an undischarged condition.

Example:

```text
representation:
    Nat-family-X -> signed machine integer

assumption:
    every reachable represented value is in [0, INT_MAX]

status:
    assumed
```

A C backend may then use an ordinary `int` realization when the ABI is compatible.

This model is deliberately stronger than silently copying the classic extraction pattern where an unbounded natural is mapped onto a bounded host integer.

#### 3. `approximate`

The user explicitly allows changed results or changed behavior even when all stated input preconditions hold.

Examples:
- wrap an unbounded natural modulo `2^32`,
- saturate at `INT_MAX`,
- truncate precision,
- deliberately discard an observable effect,
- replace exact arithmetic with approximate floating point.

Approximate lowering is useful, but it must never be reported as proof-preserving or as an exact executable interpretation of the canonical artifact.

### Plan

- [ ] Add a lowering-result manifest with `exact`, `conditional`, or `approximate` status.
- [ ] For `conditional`, record every obligation and its state: proved, runtime-checked, or user-assumed.
- [ ] Require an explicit CLI/profile opt-in for `conditional:assumed`.
- [ ] Require a stronger explicit opt-in for `approximate`.
- [ ] Never write an unproved lowering assumption back into the canonical artifact as if it were semantic evidence.
- [ ] Include the lowering profile and assumption manifest in derived-output reproducibility hashes.
- Completion: inspecting a generated C/CUDA/Verilog output is sufficient to determine whether it claims exact semantics, conditional semantics, or intentional approximation, and why.

---

## P6. Use a target-independent lowered IR before C/CUDA/Verilog legalization

### Subjective (User)

- User requirement, translated/paraphrased from 2026-09-28: C, CUDA C, and Verilog are all intended future outputs.
- User requirement: the artifact should remain upstream of all of them.
- User requirement: aggressive simplification should make it possible to execute A Program computations in foreign computation models without forcing those models to reproduce all high-level structure literally.

### Objective (Code)

- Current Core is intentionally extremely small and semantic.
- Current typed occurrences contain richer type/context information than Core alone.
- C/CUDA execution and Verilog synthesis have radically different realization constraints:
  - C tolerates general dynamic control flow and memory.
  - CUDA adds host/device placement and kernel-launch constraints.
  - synthesizable Verilog requires static hardware structure, explicit widths, and hardware scheduling/resource decisions.
- Therefore no single backend-specific IR should become the canonical artifact.

### Assessment

Use at least two downstream layers:

```text
Verified Semantic View
        |
        | semantic/representation lowering
        v
Portable Lowered IR
        |
        +-----------------------> C legalization/emission
        |
        +-----------------------> CUDA legalization/emission
        |
        +--> hardware staticization/scheduling
                    |
                    v
                  RTL IR
                    |
                    v
                  Verilog
```

The Portable Lowered IR should contain only target-independent execution concepts that are already justified by the selected lowering profile, for example:
- machine-width integers and explicit arithmetic semantics,
- tuples/records/sums after representation selection,
- local variables and explicit control flow,
- calls/returns,
- explicit memory objects where required,
- explicit effect/runtime calls after effect lowering,
- no dependent typing machinery that has already been proven runtime-irrelevant.

The RTL IR may add:
- clocks/resets,
- registers/wires,
- cycle scheduling,
- pipeline stages,
- handshake protocols,
- resource sharing.

Those are realization facts, not canonical A Program facts.

For CUDA, kernel decomposition and launch geometry should similarly live after portable lowering unless A Program itself gains a semantic parallel-execution construct.

### Plan

- [ ] Define a minimal Portable Lowered IR only after P1-P5 boundaries are in place.
- [ ] Keep representation types explicit and target-independent.
- [ ] Require each backend to declare which lowered operations/types it legalizes.
- [ ] Allow backends to reject unsupported programs without changing the canonical artifact.
- [ ] Add a separate RTL lowering for hardware-specific staticization and scheduling.
- [ ] Differential-test exact/conditional-in-domain C and CUDA outputs against the A Program evaluator.
- [ ] For Verilog, test finite examples against both the lowered reference model and RTL simulation.
- Completion: one Verified Semantic View and one representation profile can be consumed by multiple backends; backend-specific constraints appear only after the portable lowering boundary.

---

## P7. Give lowering profiles stable nominal targets without contaminating semantic identity

### Subjective (User)

- User requirement, translated/paraphrased from prior artifact discussions and reaffirmed by the current request: names are presentation/cultural choices and should not unnecessarily define the identity of a mathematical/semantic object.
- User requirement in the present discussion: nevertheless, a user needs to be able to say "lower this Nat-like type to `int`" under an explicit option.

### Objective (Code)

- `pg_data_declaration` is generative; declaration identity is semantically relevant.
- Graph serialization uses relocation tables, but relocation IDs are transport-local.
- Source names exist and are useful for user interaction, but backend symbol restrictions should not become semantic identity.
- A lowering profile therefore needs a way to select a declaration without making a C name or a textual spelling the canonical identity.

### Assessment

Use two levels of identity:

1. **Artifact-local semantic entity identity**
   - deterministic within a serialized artifact,
   - scoped by the artifact/container identity or digest,
   - distinguishes two generative declarations even when structurally identical.

2. **Human selector metadata**
   - source/export path such as `Nat`,
   - used to resolve a profile entry to the semantic entity,
   - not trusted as the semantic identity after resolution.

Illustrative profile:

```text
representation "small-nat" {
    select export "Nat";
    require nat_like_unary_declaration;
    lower_to machine_integer(signed = true, width = 32);
    invariant all_reachable_values <= 2147483647;
    obligation_policy assume;
}
```

After resolution, the lowering manifest should record the resolved artifact-local entity, not repeatedly re-resolve by spelling.

Do not add `c_name`, `cuda_name`, or `verilog_name` to that semantic entity. Target names are assigned later.

### Plan

- [ ] Define deterministic artifact-local IDs for exported semantic entities, or define an equivalent `(artifact digest, local ID)` scheme.
- [ ] Resolve user profile selectors to nominal entities before lowering.
- [ ] Reject ambiguous selectors.
- [ ] Preserve nominal distinction even if several entities lower to the same physical representation.
- [ ] Allocate target symbol names only in backend-specific emission.
- Completion: representation profiles can target exact nominal families reproducibly, while renaming a generated C identifier cannot change artifact semantics.

---


## P8. Make partial semantic snapshots lowerable without advancing Solve

### Subjective (User)

- User requirement, translated/paraphrased from the 2026-09-28 discussion: after A Program has executed some finite number of Solve steps, the resulting `.a` should be usable as input to C, CUDA C, or Verilog lowering **whether or not additional Solve work remains**.
- User requirement, translated/paraphrased: the backend must not force the compiler to continue solving merely because the backend is being invoked.
- User requirement, translated/paraphrased: additional Solve steps may improve or certify the generated result, but the already-materialized semantic state should remain a valid compilation/transpilation source.
- User requirement, translated/paraphrased: this must remain subordinate to the artifact model; solver queues or backend implementation details must not become canonical artifact semantics.

### Objective (Code)

- The current Program-image design already permits saving incomplete work. The repository documentation includes pending images and `--steps`-bounded solving.
- `src/source_io.h` explicitly states that:
  - stored contexts and derivation inputs are reconstructed as unaccepted inputs;
  - source types/branches are synthesized again;
  - ordinary `Solve` recomputes solutions after loading;
  - **no search state or acceptance flag is retained**;
  - the format is **not a complete checkpoint codec**.
- Consequently, a current pending `.a` records enough source/synthesis material to resume ordinary solving, but it does **not** preserve all solver progress as an authoritative semantic snapshot.
- `src/synthesis.h` already contains an important separation between acceptance and structure. Helpers such as `pg_synthesis_type_structure`, `pg_synthesis_classifier_structure`, and `pg_synthesis_term_structure` expose structural subjects for supported producers even when they are not accepted results.
- This means the codebase already has the beginnings of a distinction between:
  1. **materialized semantic structure**, and
  2. **accepted/verified evidence**.

### Assessment

The artifact needs a new invariant:

> **Lowerability is independent of proof completion.**

The compiler should distinguish at least three axes:

```text
materialization        acceptance / proof             lowering
---------------        ------------------             --------
not available          pending                        not ready
structural             pending                        potentially lowerable
typed/classified       partial or accepted            lowerable
fully verified         accepted                       lowerable
```

These axes must not be collapsed into a single `DONE` bit.

A backend does not fundamentally need every proof obligation to be solved. It needs enough **materialized executable structure** to select and justify a lowering.

For example, if an exported function's lambda/application/match/constructor structure is already materialized, a backend may be able to lower it even while unrelated or stronger obligations remain pending:
- termination obligations,
- equalities or conversions,
- theorem-only obligations,
- additional normalization,
- proof terms that are irrelevant to execution under the selected lowering profile.

Therefore the desired architecture is:

```text
                    .a semantic snapshot
                    /                  \
                   /                    \
          continue Solve               lower now
             |                           |
             v                           v
   more settled facts          Portable Lowered IR
             |                    /      |      \
             +---------------->  C    CUDA C    RTL
```

**Solve and lowering are siblings, not a mandatory pipeline `Solve -> backend`.**

#### The canonical snapshot should contain semantic progress, not solver implementation state

The new snapshot should separate:

```text
Semantic Snapshot
├── Materialized Semantic Structure
│   ├── Core / subjects
│   ├── declarations
│   ├── contexts
│   ├── materialized eliminations/branches
│   └── semantic object dependencies
│
├── Settled Facts
│   ├── accepted classifiers/types
│   ├── accepted conversions/equalities
│   ├── completed reductions
│   └── checked certificates
│
├── Residual Obligations
│   ├── pending typing
│   ├── pending conversion
│   ├── pending normalization
│   ├── pending effect inference
│   └── pending proof obligations
│
├── Optional Source / Replay Metadata
│
└── Optional Solver Checkpoint
    ├── ready queues
    ├── search cursors
    └── implementation-specific work state
```

Backends may consume the first three categories. They must **not** depend on the optional solver checkpoint.

The checkpoint may exist for fast resume, but it is not semantic ABI.

#### Settled work must survive save/load

The current replay-oriented design can discard useful Solve progress and reconstruct derivation recipes to be solved again. That is appropriate for conservative replay, but it does not satisfy the user's compilation requirement.

Once a fact has been accepted, the artifact should be able to retain enough evidence to re-establish that fact **without rerunning the original search**.

Conceptually:

```text
expensive Solve/search
        |
        v
accepted fact + compact certificate/evidence
        |
      save
        |
      load
        |
small deterministic checker
        |
        v
settled fact restored
```

This does not mean trusting bytes from disk. The loader still verifies the certificate.

The critical distinction is:

- **checking a certificate** is allowed;
- **rerunning open-ended solver search merely to invoke a backend** is not required.

#### Residual obligations must remain explicit

A partially solved snapshot should not pretend to be fully verified.

A selected export might carry:

```text
export f
status:
    structure = materialized
    typing = partial
    termination = pending
    effects = settled
residual:
    obligation O17
    obligation O31
```

Lowering then decides, according to its profile, whether those obligations are:
- irrelevant to the requested runtime semantics,
- already covered by another settled invariant,
- required before this lowering can proceed,
- allowed as explicit assumptions,
- or require residualization/runtime support.

This is where the `exact / conditional / approximate` distinction from P5 becomes essential.

#### Define a lowerability frontier, not a Solve-completion frontier

The compiler needs an explicit query roughly of the form:

```text
lowerability(snapshot, export, profile)
    -> NOT_READY
     | STRUCTURAL
     | TYPED
     | VERIFIED
```

Exact naming can change, but the semantic distinction should remain.

`STRUCTURAL` means the target-independent executable structure needed by the profile is materialized.

`TYPED` means the representation-relevant classifier/type information required by the profile is also settled.

`VERIFIED` means all obligations required by the full semantic claim are accepted.

A profile may accept `STRUCTURAL`, `TYPED`, or require `VERIFIED`.

For example:
- a conservative proof-carrying backend mode may require `VERIFIED`;
- a `Nat -> machine_integer` mode may require the selected nominal declaration and eliminator structure plus an explicit boundedness assumption;
- a debugging/interpreter-style backend may residualize more unresolved work.

#### Extremely early snapshots may require residual execution

The requirement should not be misread as "every snapshot after one Solve step must become ordinary straight-line C or static RTL".

If the executable structure itself is not yet materialized, a backend has two choices:

1. reject the selected direct lowering as `NOT_READY`; or
2. use a more general residual/runtime realization.

A C backend could theoretically lower unresolved semantic machinery into an A Program runtime machine and continue residual computation at runtime. That is a valid fallback architecture, but it should be recognized as a different lowering mode.

For Verilog, the same idea can only be realized under explicit finite-resource assumptions (bounded heap/stack/state/width), making it conditional or approximate as appropriate.

### Interaction with aggressive representation lowering

This requirement combines naturally with P4.

Suppose a snapshot has already materialized a unary Nat-like declaration and its executable eliminator structure, but still has unrelated proof obligations.

The lowering profile may still select:

```text
Nat-family-X
    -> machine_integer(width = 32, signed = true)
```

and lower:

```text
zero       -> 0
succ n     -> n + 1
match n    -> zero/nonzero test with predecessor
```

without continuing Solve, provided every obligation required by that transformation is either:
- already settled,
- discharged by a local checker/analysis,
- explicitly runtime-checked,
- or explicitly assumed by the selected `conditional` profile.

The lowering manifest could then state:

```text
materialization: structural
verification: partial
representation:
    Nat-family-X -> s32
correctness:
    conditional:assumed
residual obligations:
    O17
    O31
```

Further Solve steps may later convert the exact same artifact state lineage into:

```text
correctness:
    conditional:proved
```

or permit additional erasure/specialization, but those steps are an optimization/refinement of the lowering, not a prerequisite imposed by the backend.

### Plan

- [ ] Replace the "backend accepts only a fully verified artifact" assumption with a `pg_semantic_snapshot` / Partial Semantic View abstraction.
- [ ] Separate materialized structure, settled facts, and residual obligations in the snapshot API.
- [ ] Add stable IDs for residual obligations so they can survive serialization and appear in lowering manifests.
- [ ] Preserve accepted results/certificates across save/load without requiring the original search to be replayed.
- [ ] Add a small deterministic checker for serialized evidence where feasible; do not equate certificate checking with Solve/search.
- [ ] Keep solver ready queues, cursors, and worklist implementation details out of the canonical semantic snapshot.
- [ ] Add a `lowerability` query parameterized by export and lowering profile.
- [ ] Define profile-specific minimum frontiers (`STRUCTURAL`, `TYPED`, `VERIFIED` or equivalent).
- [ ] Permit C/CUDA/RTL lowering from partial snapshots when the profile's frontier is met.
- [ ] Record unresolved obligations and assumption policy in the lowering manifest.
- [ ] Make further Solve steps monotonic refinements when possible: they may settle residuals and enable stronger lowering, but must not invalidate previously materialized semantic structure without an explicit semantic reason.
- [ ] Add tests:
  - save after 0 steps, N steps, and full Solve;
  - load and lower without invoking additional Solve;
  - verify that already-settled facts are not recomputed by search;
  - compare outputs at different Solve frontiers;
  - confirm that target backends never inspect solver worklists/checkpoint internals.
- Completion: for any snapshot whose selected export meets a lowering profile's materialization frontier, `load -> lower` succeeds without invoking further Solve, regardless of whether unrelated residual obligations remain.

### Architectural invariant

The following should be written into the artifact/backend contract:

> **Solve is an optional refinement of an artifact, not a prerequisite imposed by a backend.**
>
> Once the semantic/executable frontier required by a selected lowering profile has been materialized, the artifact may be lowered without advancing Solve further.
>
> Additional Solve steps may certify residual obligations, expose stronger types/classifiers, specialize representations, erase additional structure, or improve generated code. They must not be required merely to make already-materialized semantic content visible to a backend.


# Proposed Architecture

```text
                           CANONICAL / AUTHORITATIVE MEANING
                           ================================

        source text
            |
            v
    parser / synthesis
            |
            v
    +-----------------------------+
    | A Program Program state     |
    | graph + typing + synthesis  |
    +-----------------------------+
            |
            | zero or more Solve steps
            v
    +--------------------------------------+
    | Partial Semantic View / Snapshot     |
    |--------------------------------------|
    | materialized Core/semantic structure |
    | nominal declarations                 |
    | contexts                             |
    | settled typed occurrences/facts      |
    | semantic descriptors                 |
    | effects known so far                 |
    | residual obligations                 |
    +--------------------------------------+
          |                         |
          | continue Solve          | lower now
          v                         v
    stronger snapshot       representation/lowering profile
                                  |
                                  | assumptions + proved obligations
                                  v
                         +-----------------------------+
                         | Lowering Manifest           |
                         | exact / conditional /       |
                         | approximate                 |
                         +-----------------------------+
                                  |
                                  v
                         +-----------------------------+
                         | Portable Lowered IR         |
                         +-----------------------------+
                            |             |             |
                            v             v             v
                           C IR        CUDA IR      Hardware lowering
                            |             |             |
                            v             v             v
                            C          CUDA C          RTL IR
                                                        |
                                                        v
                                                     Verilog

    The snapshot itself may also be serialized as the canonical semantic
    artifact, with optional source/replay metadata and optional solver
    checkpoint/cache sections.
```

The key rules are:

1. no arrow points back upward from a target realization into the canonical artifact schema;
2. lowering and further Solve are sibling consumers of a semantic snapshot;
3. full verification is a stronger snapshot state, not the universal entry requirement for lowering.

---

# Worked Example: `Nat` to Native Integer

Assume the canonical artifact contains a verified nominal declaration corresponding to:

```text
Nat := @{
    zero : *;
    succ : * -> *;
};
```

The full semantic artifact retains the declaration, its nominal identity, typing structure, and any evidence required by A Program. It does **not** rewrite this declaration to `int`.

A separate profile selects that exact nominal family:

```text
Nat-family-17
    => machine_integer(width = 32, signed = true)

invariant:
    0 <= every reachable Nat-family-17 value <= 2147483647

policy:
    conditional:assumed
```

The representation-lowering pass may then derive:

```text
zero            => 0
succ x          => x + 1
zero/succ match => x == 0 ? zero_case : succ_case(x - 1)
```

A C target with a compatible ABI may emit:

```c
int ...
```

A CUDA C target may emit the same physical scalar or a fixed-width device scalar.

A Verilog lowering may emit:

```text
logic signed [31:0] ...
```

subject to the chosen invariant and to later RTL scheduling.

The **artifact does not change** between these realizations.

If the compiler later proves that all reachable values satisfy the bound, the manifest can be upgraded from:

```text
conditional:assumed
```

to:

```text
conditional:proved
```

without changing the source-language meaning.

If an exact arbitrary-precision fallback is added, the same conceptual optimization can become:

```text
exact
```

because the machine integer is only a fast representation for the bounded common case.

If instead the user chooses:

```text
overflow = wrap_mod_2_32
```

for a source `Nat`, this must be classified as `approximate`, unless a separate proof establishes that wrapping is unreachable.

---

# Important distinction: erasure is not the same as approximation

The user's "discard branches and leaves" requirement covers several fundamentally different transformations.

## Semantically exact erasure

Examples:
- remove descriptive typing structures that have no runtime observation after verification,
- erase an impossible indexed-constructor branch when accepted facts prove it unreachable,
- erase a proof/evidence payload when a specific erasure theorem says no runtime observation depends on it,
- choose an unboxed representation while retaining an exact fallback.

These should remain `exact`.

## Conditional semantic compression

Examples:
- unbounded `Nat` -> fixed machine integer under a whole-program range invariant,
- recursive container length -> fixed-width counter under a capacity invariant,
- arbitrary index -> finite bit-vector under a proved or assumed bound.

These are `conditional` unless the compiler discharges the condition.

## Intentional approximation

Examples:
- overflow wraps even though source `Nat` would continue growing,
- saturation,
- truncation,
- approximate floating-point replacement,
- dropping an observable effect.

These are `approximate`.

This distinction should be a first-class compiler concept, not an informal backend convention.

---

# Why this is safer than making `Nat -> int` a C backend trick

A direct C-only shortcut would create four long-term problems.

1. **The C backend would need to recognize A Program semantic objects itself.**  
   CUDA and Verilog would duplicate that logic and could drift.

2. **The assumption would be hidden.**  
   Users could mistake bounded target behavior for an exact consequence of the A Program proof.

3. **C ABI details would leak upward.**  
   `int` width and signed-overflow behavior could begin influencing artifact fields.

4. **The same optimization could not be shared cleanly.**  
   The mathematical statement is "this unbounded carrier is representable by a bounded carrier under invariant X", not "C likes `int`".

A target-independent representation-lowering rule solves all four.

---

# Relationship to existing implementation choices

## `pg_data_layout` vs `pg_data_declaration`

This is the strongest existing precedent for the proposed architecture.

`src/iadt.h` explicitly says that `pg_data_layout` is an **erased, immutable constructor layout** and that arity is runtime layout information rather than a positivity/index/type schema. The full nominal `pg_data_declaration` separately retains parameters, indices, fields, constructor result images, and generative identity.

The artifact/lowering architecture should extend this distinction rather than collapse it:
- canonical semantic artifact retains the complete declaration;
- lowering derives an erased/optimized representation;
- backends consume the derived representation.

## `#Int32` / `#Int64`

`src/host.h` states that these are fixed machine contracts and not C `int`/`long`. This is exactly the right direction.

A future lower IR can use explicit machine-width arithmetic without making the canonical artifact dependent on a C ABI. However, source `#Int32` semantics and "Nat represented by 32 bits" are not automatically the same thing: the latter carries a range/refinement obligation and must not be silently conflated with wraparound source semantics.

## Typed occurrences

`struct pg_occurrence` is a good candidate input to lowering because it carries the Core subject together with classifier/context/origin/operand structure. A pure Core term is intentionally too erased to decide all representation transformations safely.

The backend should therefore receive a verified semantic view derived from typed structure, not rediscover types by analyzing erased Core.

---

# Prior-art sanity check

These comparisons are informative but do not define A Program's design.

## Lean

Lean presents `Nat` logically as the usual `zero`/`succ` inductive type while its compiler/runtime gives natural numbers a radically different efficient representation. Small naturals use immediate machine-word representations and larger values fall back to arbitrary precision. This demonstrates that **logical representation and runtime representation can differ dramatically without weakening the semantics**.

Relevant documentation:
- https://lean-lang.org/doc/reference/latest/Basic-Types/Natural-Numbers/
- https://lean-lang.org/functional_programming_in_lean/Programming___-Proving___-and-Performance/Special-Types/

The A Program design proposed here should permit this exact style as an `exact` realization.

## Rocq/Coq extraction

Rocq's extraction libraries have historically allowed mappings from logical naturals/integers to host integer representations. The standard-library documentation explicitly warns that extracting an unbounded integer-like type into a bounded host `Int` is not in itself a sound certified realization.

This is close to the user's requested aggressive mode and is a useful warning: A Program should support the optimization, but should make the extra assumption explicit and machine-readable rather than silently adding it to the trusted computing base.

Relevant documentation:
- https://rocq-prover.org/doc/v9.0/stdlib/Stdlib.extraction.ExtrHaskellNatInt.html
- https://rocq-prover.org/doc/master/stdlib/Stdlib.extraction.ExtrHaskellZInt.html

## MLIR

MLIR's dialect conversion separates a conversion target, legalization rules, and type conversion, and distinguishes full from partial conversion. The useful lesson for A Program is architectural: target legality belongs in a downstream conversion framework rather than in the upstream semantic representation.

Relevant documentation:
- https://mlir.llvm.org/docs/DialectConversion/

A Program should go further by attaching an explicit semantic-correctness class and assumption ledger to aggressive conversions.

---

# Minimal refactoring required before a real C backend

A complete artifact redesign is **not** required before any backend experiment. The minimum safe boundary is smaller:

1. **Create the Partial Semantic View / semantic snapshot.**
   - Backends see materialized semantic exports, settled facts, and residual obligations.
   - Backends cannot see source/synthesis worklists or solver checkpoint internals.
   - Fully verified exports are represented as a stronger snapshot state, not as the only acceptable input.

2. **Make occurrence transport descriptor-complete.**
   - Nominal declarations and future semantic owners must round-trip through the same codec discipline.

3. **Introduce a lowering profile + manifest.**
   - Keep it separate from the canonical artifact.
   - Record exact/conditional/approximate status.

4. **Introduce a minimal Portable Lowered IR.**
   - Start only with what the first C examples need.
   - Do not let C syntax define the IR.

After these four steps, a C backend can begin without freezing `source_io` or `pg_program` into a backend ABI.

The larger source/replay-section cleanup can proceed incrementally afterward because the dependency direction is already protected.

---

# Recommended implementation order

## Phase 0 — Pin and test the current baseline

- [ ] Record exact `main` commit SHA.
- [ ] Run the full current acceptance suite.
- [ ] Save representative pending and completed `.a` images for compatibility tests.

## Phase 1 — Semantic snapshot boundary

- [ ] Add `semantic_snapshot.[ch]` / `semantic_export.[ch]` or equivalent.
- [ ] Export materialized roots, settled typed facts, nominal dependencies, and residual obligations after any finite number of Solve steps.
- [ ] Represent full verification as a stronger status of the same snapshot model.
- [ ] Add backend include-boundary tests.

## Phase 2 — Semantic transport and progress preservation

- [ ] Extend occurrence I/O to full `pg_graph_codec`.
- [ ] Round-trip data declarations, materialized exports, settled facts, and residual obligations independently of source replay.
- [ ] Preserve accepted results/certificates so load/lower does not rerun search.
- [ ] Define artifact-local semantic entity IDs and residual-obligation IDs.

## Phase 3 — Lowering contracts

- [ ] Add lowering profile parsing/building.
- [ ] Add manifest records for representation choices and obligations.
- [ ] Implement correctness classes.

## Phase 4 — Portable lowered IR

- [ ] Add explicit scalar widths and arithmetic contracts.
- [ ] Add basic control flow, calls, sum/data representation, and effect boundary representation.
- [ ] Implement exact structural lowering first.

## Phase 5 — Aggressive Nat prototype

- [ ] Select one verified nominal Nat-like family.
- [ ] Verify declaration shape.
- [ ] Lower `zero`, `succ`, and elimination to bounded integer operations.
- [ ] Support `prove`, `check/fallback`, and `assume` obligation policies.
- [ ] Add overflow negative controls.

## Phase 6 — C reference backend

- [ ] Emit C from Portable Lowered IR only.
- [ ] Differential-test against A Program evaluation.
- [ ] Do not optimize by peeking at source syntax or synthesis jobs.

## Phase 7 — CUDA C

- [ ] Reuse the same Portable Lowered IR and representation profile.
- [ ] Add device-specific legalization only after the portable boundary.

## Phase 8 — Verilog

- [ ] Add a separate hardware staticization/scheduling pass.
- [ ] Emit RTL IR, then Verilog.
- [ ] Keep clocking, pipelines, and resource mapping outside the canonical artifact.

---

# Acceptance criteria

The refactor is successful when all of the following hold.

- [ ] A canonical artifact can be loaded and verified with all backends disabled.
- [ ] A partially solved canonical artifact whose selected export meets a profile's materialization frontier can be loaded and lowered without invoking further Solve.
- [ ] Save/load preserves already-settled semantic progress as checkable facts/certificates rather than reducing all progress back to replay-only recipes.
- [ ] Residual obligations remain explicit and machine-identifiable after serialization.
- [ ] Backends never depend on solver ready queues, search cursors, or checkpoint implementation details.
- [ ] C/CUDA/Verilog modules do not include source/synthesis Program-image headers.
- [ ] The same canonical artifact is accepted by different lowering profiles without regeneration.
- [ ] Adding or deleting a backend cannot change canonical artifact serialization.
- [ ] A `Nat`-like nominal family can be represented either as its ordinary erased inductive runtime layout or as a bounded machine integer selected downstream.
- [ ] The bounded lowering records a whole-program range obligation.
- [ ] `exact`, `conditional`, and `approximate` outputs are mechanically distinguishable.
- [ ] An undischarged assumption requires explicit user opt-in.
- [ ] Approximate lowering can never be mislabeled as proof-preserving.
- [ ] C signed overflow is either proven unreachable, dynamically avoided, or replaced by a defined operation; it is never accidentally introduced.
- [ ] CUDA-specific launch/address-space information does not appear in canonical artifact schemas unless A Program independently gains an equivalent semantic feature.
- [ ] Verilog timing/scheduling/resource information is confined to derived hardware/RTL layers.
- [ ] Nominal generativity survives representation lowering: two equal-shaped declarations may share representation but do not become the same semantic type.
- [ ] Differential tests compare exact and in-contract conditional generated code with the accepted A Program behavior.

---

# Non-goals

This audit does not propose:

- making `Nat` a built-in primitive in A Program;
- changing A Program's mathematical semantics to fixed-width arithmetic;
- defining C `int` as an artifact type;
- guaranteeing that every A Program program is synthesizable to Verilog;
- trusting serialized typed occurrences as evidence merely because they were loaded;
- forcing the current Program-image replay functionality to disappear;
- choosing the final textual syntax of lowering profiles before the semantic boundary is implemented.

---

# Final recommendation

The highest-priority refactor is **not** "write a C backend" and is also **not** "redesign every byte of `.a` first".

The highest-priority refactor is to create a narrow **Partial Semantic View / semantic snapshot** and make it the only legal semantic input to downstream compilation. This view must expose materialized structure, settled facts, and residual obligations after any finite number of Solve steps. A fully verified view is a stronger state of the same model, not a prerequisite for every backend invocation. Existing typed occurrences, structural synthesis accessors, contexts, nominal data declarations, and descriptor codecs already provide much of the required substrate.

Immediately after that boundary exists, introduce a separate **representation-lowering contract**. This is where aggressive choices belong:

```text
full A Program semantics
        |
        | exact / conditional / approximate lowering
        v
simpler computation model
        |
        v
C / CUDA C / RTL
```

`Nat -> int` should be a first-class demonstration of the design, not an exception hidden in the C emitter.

The canonical artifact/snapshot continues to say what is currently materialized about what the A Program program **means**, which facts are already settled, and which obligations remain unresolved.  
Further Solve steps refine that snapshot; they are not a mandatory gateway to a backend.  
The lowering profile says what semantic frontier, assumptions, and invariants the compiler may **exploit**.  
The backend says how the resulting lowered computation is **realized** on a target.

Keeping those three statements separate is the main architectural condition required to obtain aggressive transpilation without making A Program's artifact subordinate to any one backend.

---

## Progress

| Date | Problem | Material result or decision | Evidence / next step |
| --- | --- | --- | --- |
| 2026-09-28 | P1-P3 | Current `.a` path classified as Program/source replay rather than a clean backend ABI; proposed Verified Semantic View boundary. | Inspect `src/source_io.h`, `src/program.h`, `src/typing.h`. Pin exact commit SHA before implementation. |
| 2026-09-28 | P2 | Existing occurrence/context/declaration codecs identified as the preferred semantic-transport substrate. | Extend occurrence I/O to full descriptor codec and add semantic export round-trip tests. |
| 2026-09-28 | P4-P5 | Proposed target-independent conditional representation lowering, including `Nat`-like nominal families to bounded machine integers, with exact/conditional/approximate correctness classes. | Prototype only after semantic boundary exists. |
| 2026-09-28 | P6 | Proposed Portable Lowered IR shared by C/CUDA, with separate hardware lowering before Verilog. | Define only the minimum operations required by initial C examples. |
| 2026-09-28 | P8 | Revised backend entry model: lowering may consume a partial semantic snapshot without advancing Solve, provided the selected profile's materialization frontier is met. Settled facts must survive save/load; unresolved obligations remain explicit. | Add semantic snapshot/progress transport before production backends. |
