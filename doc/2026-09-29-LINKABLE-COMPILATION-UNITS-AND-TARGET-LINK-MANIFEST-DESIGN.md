# Linkable compilation units: current review and submission plan

Date: 2026-09-29
Baseline: main at `ea287c2e45d33c6d19f34faca6ab9fd61fc50be5`.
Status: documentation/design submission; no implementation changes.
Related: #44 (Artifact-to-C), #46 (components), #47 (relevance/admissibility).

## Problem List

1. P1 — Linkable components and target manifests: #46.
2. P2 — Runtime relevance and partial-artifact compilation admissibility: #47.

## P1. Linkable components and target link manifests

### Subjective (User)

2026-09-29, English paraphrase: after reviewing the supplied Linkable Compilation Units document against current code, the user approved filing two separate issues and one documentation PR. This issue covers packaging/linkage; runtime relevance and pending-proof compilation are separate. This is approval of the submission/scope, not every proposed API name or ABI rule.

### Objective (Code)

Baseline: main at `ea287c2e45d33c6d19f34faca6ab9fd61fc50be5`, unchanged at publication recheck.

- `src/prototype/c_backend/emit.h`: pg_c_emit consumes one borrowed closed occurrence; admission is the caller's responsibility.
- `emit.c:197` and `:257`: private static tN functions and an unconditional main wrapper.
- `main.c`: one named selection; ordinary whole-module checking or explicit trusted saved completion. Trust is unauthenticated and does not complete pending work.
- `runtime.h`: internal ABI 2; invocation-wide allocation, status and setjmp failure boundary. This is not a public library ABI.
- `emit.c:29`: object identities become emission-local ordinals; runtime.c compares these for nominal families and effect labels.
- The prototype has no general multi-export/header/library product interface. This is an extension request, not a reproduced current single-executable bug.

Fresh review-session verification: candidate.sh created an isolated overlay and check-c-backend passed, including standalone C differential tests for effects, closures, arithmetic, recursive ADTs, failure gates and Oracle/Identity boundaries. The full acceptance suite and check-c-sorting-boundary were not run for this review.

### Assessment

Follow up #44 with a downstream Link Plan/Component Manifest:
semantic roots -> joint dependency closure -> private generated code + public wrappers -> object/library/executable.
A name is a logical selection unit, not necessarily one object file. Executable main is a product wrapper, not a semantic property of the selected name.

Keep canonical artifact identity/type/evidence independent of target symbol names, ABI layout, visibility and native linker scripts.
A GNU ld memory-layout script is a possible generated target product, not the common interface model.

First implement multiple already-supported closed exports under existing admission policy. General proof erasure and relaxed pending-proof admission must not block this packaging milestone.
Do not expose tN functions or simply declare the current runtime struct a stable foreign ABI.

Additional boundary requiring specification:
- boxed handles need owning runtime/component provenance and defined lifetime;
- independently emitted components have local ordinal spaces: equal numbers must not equate unrelated nominal declarations or operations;
- shared semantic declarations also must not accidentally become incompatible through repeated loading;
- either prohibit cross-component handle exchange initially or provide a checked namespace/relocation contract;
- define failure recovery, repeated calls, reentrancy/thread policy and allocation lifetime before persistent contexts;
- do not expose a stale setjmp target across foreign calls.

### Plan

- [ ] Split executable startup/main generation from private DAG emission.
- [ ] Select multiple closed roots and collect their union once; preserve current check/trust policy.
- [ ] Generate explicit public wrappers, a header and a link receipt; reject duplicate/invalid/reserved aliases.
- [ ] Support generated source, object/static-library and executable products; separate artifact .a from native archive .a by explicit product metadata.
- [ ] Specify runtime ownership, error status and repeated-call behavior; keep public ABI version separate from internal runtime ABI.
- [ ] Test two exports called from a C client, private symbol visibility, unchanged input artifact, deterministic emission, failures and sanitizer lifetime checks.
- [ ] Define component-qualified nominal/effect identity before cross-component boxed calls; add collision and shared-identity controls.
- [ ] Stage callable Pi wrappers, restricted flat ABI, shared-library visibility and foreign imports afterward.
- [ ] Defer CUDA/Wasm/RTL realization and full memory-layout scripts; preserve their architectural room.

Completion: a C client links a generated library and calls at least two supported exports, while executable mode wraps the same machinery; no proof-admission relaxation, hidden identity collision or runtime lifetime ambiguity is introduced.

## P2. Erasure and compilation from partially verified artifacts

### Subjective (User)

2026-09-29, English paraphrase: the user approved separating this topic from the linkable-component implementation issue and submitting both with the supplied document.
The proposed default pending-proof policy is a design question to resolve, not an implemented or automatically approved acceptance rule.

### Objective (Code)

Baseline: main at `ea287c2e45d33c6d19f34faca6ab9fd61fc50be5`.

- The C prototype adapter currently requires ordinary whole-module checking or explicitly trusted saved whole-module completion.
- The emitter itself does not check imported occurrences or establish relevance.
- Prototype README/runtime implement selected Identity actions and scoped transports; arbitrary paths are not blanket-erased.
- Existing unused/lazy operands and supported Identity reductions do not establish a general relevance judgment for pending artifacts.
- Review-session check-c-backend passed. No proposed erasure pass, pending-proof acceptance mode or cross-language erasure theorem was tested.

### Assessment

Distinguish three obligations:
1. Is an argument/field computationally irrelevant?
2. Are the runtime types, effects, representations and boundary contracts established without the pending obligation?
3. Is the requested target realization supported?

Absence of a proof object from generated code does not establish (2). A proof may justify an impossible branch, a cast/layout, a bounds check removal or an index equality even if its runtime payload disappears.
Require a dependency/accounting rule identifying which unresolved obligations are irrelevant to the selected compilation claim. Never classify an unknown typing obligation as proof-only by naming or syntax.

A Program must not adopt Lean Prop-style erasure by analogy alone. The existing higher/path/Identity behavior may be relevant; decide per permitted eliminator/occurrence what can be erased and prove or check the criterion.
The supplied document already warns against unsafe optimization from pending evidence; this issue makes that warning an admission prerequisite, not only an optimizer switch.

Keep verification status, runtime completeness and lowering correctness separate. Metadata records assumptions but does not make them sound.
Pending semantic claims can remain recorded while a separately justified conservative runtime graph is emitted; insufficient typing/layout information stays NOT_READY, unless an explicit, separately specified assumption or runtime-hole mode applies.
Recheckable relevance facts must not require trusting all completion bytes.
No general theorem prover need be embedded in generated C.

Existing link packaging can proceed using current admission rules. This broader feature follows #44's partial-artifact direction but is not required to split main or generate a library.

### Plan

- [ ] Specify a conservative relevance judgment, its trusted base, and dependency relation to typing/conversion/layout/effect obligations.
- [ ] Distinguish runtime-relevant data, compile-time representation inputs and erasable evidence; do not assume proof irrelevance/UIP.
- [ ] Specify Identity/transport cases whose erasure is justified; retain or reject unsupported relevant cases.
- [ ] Define selected-export admissibility from partial artifacts independently of whole-program completion, with explicit residual obligations.
- [ ] Decide development/release defaults only after the criterion and negative tests exist.
- [ ] Prohibit pending evidence from authorizing unreachable, unchecked casts, narrowing or check removal.
- [ ] Separate optional computational-hole traps from erased proof obligations; give traps an explicit changed-behavior contract.
- [ ] Test harmless pending theorem versus pending representation/type constraint; same erased witness behavior; computational witness preservation; invalid siblings/dependency closure; false bounds and equality assumptions.
- [ ] Record verification, assumptions, relevance status and runtime holes in receipts without granting Kernel evidence.

Completion: a precisely specified class of partially verified exports compiles under checked admissibility conditions, unsafe dependencies reject or require explicit policy, and the generated program is not misreported as fully verified.

## Verification details and precedence

The latest remote main was rechecked before publication and remained the pinned revision.
The earlier review in this session built and tested the repository's unpromoted prototype through its documented overlay:

```sh
bash src/prototype/artifact_persistence/candidate.sh /home/repyt/workspace/a-book/a-program-linker-overlay-20260929
make -s -j3 -f src/prototype/c_backend/build.mk \
  OVERLAY=/home/repyt/workspace/a-book/a-program-linker-overlay-20260929 \
  BUILD=/home/repyt/workspace/a-book/a-program-linker-overlay-20260929/build \
  check-c-backend
```

Result: pass, including C differential and Oracle tests. This validates the existing baseline, not the proposed component ABI or erasure policy.
Use a fresh overlay path when reproducing. No accepted implementation was changed.

This addendum is the active submission plan. In particular, it qualifies original sections 24/33: general relevance analysis and pending-proof admission are not prerequisites for the first closed-export library milestone.
The proposed default compilation policy in sections 3.11/32H is tracked separately in #47 and must not be silently enabled by the packaging work in #46.
Sections that describe ABI/CLI names remain illustrative proposals.

The original report follows unchanged as a supplied research record. Its historical recommendations and user attributions remain attributed to that document; not every recommendation is an approved implementation decision.
Original SHA-256: `d9f34ddf05068c65a71caae43cb1ffde9d17aef8ce324335c6e7db6828df0815`.
The broad language survey was not exhaustively revalidated. Review spot-checks confirmed the distinction between [GNU ld layout scripts](https://sourceware.org/binutils/docs/ld/Scripts.html) and [WIT import/export contracts](https://component-model.bytecodealliance.org/design/worlds.html), and Lean's specific [Prop irrelevance rules](https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/); none supplies an A Program erasure theorem.

## Original supplied document

# Linkable Compilation Units and Target Link Manifest Design

Date: 2026-09-29  
Status: research/design proposal; no implementation is claimed  
Scope: compilation-unit granularity, named exports/imports, executable entry selection, foreign ABI boundaries, and target-specific link products for A Program

## 1. Problem statement

The first C backend now demonstrates that a checked `.a` export can be lowered to standalone C, but the current prototype hard-wires an executable-shaped result: one selected closed returning entry is lowered, all generated Core-node functions are `static`, and the emitter writes an `int main(void)` wrapper. The generated program links only the backend runtime and libc. This is a useful first execution backend, but it is not yet a general link model.

The next design problem is therefore not merely “add a C FFI”. A Program needs a target-independent answer to three separate questions:

1. **What semantic names form the public boundary of a compiled unit?**
2. **Is the requested product an executable with one entry, or a reusable linkable component with multiple exported names?**
3. **How are those semantic names represented in the ABI and linker conventions of C, CUDA C, WebAssembly, Verilog/SystemVerilog, and future targets?**

These questions should not be answered by changing the canonical `.a` format to carry C symbol names, ELF sections, CUDA linkage rules, or RTL port layouts. The existing artifact plan already states that C names, ABI, calling conventions, target profiles, and generated code are downstream concerns and should be absent from canonical `.a`.

The proposed direction is therefore:

> **Keep `.a` as the target-independent semantic artifact. Add a separate link-plan layer that selects named semantic roots and describes the desired external boundary. Let each backend lower that plan into target-specific objects, libraries, modules, wrappers, symbol-visibility files, and, where appropriate, native linker scripts.**

The central architectural rule is:

> **A Program name is a logical link/export unit; it is not necessarily a physical object-file unit.**

A backend may place several exported names and their shared dependency closure in one `.o`, one C translation unit, one Wasm module, one CUDA relocatable device object, or one RTL package. The physical partition should remain a backend and optimization decision.

---

## 2. Current A Program backend boundary

The current prototype is deliberately narrow and should be treated as the baseline to generalize rather than discarded.

Relevant current behavior:

- `a-to-c INPUT.a ENTRY OUTPUT.c` selects exactly one named entry from a `.a` image.
- The adapter obtains a whole-module-checked selection through ordinary Solve, or an explicitly trusted completed export through the artifact trust path.
- `pg_c_emit` accepts a borrowed closed typed occurrence and emits from the existing Core DAG and Oracle views; it does not parse, Solve, convert, or normalize.
- The emitter validates the reachable subset before publishing C.
- Every generated Core node is currently emitted as a `static struct ap_value *tN(...)` function.
- The emitter currently terminates by generating `int main(void)`, allocating a runtime, running the selected root, destroying the runtime, and returning status.
- The current accepted entry shape is a closed returning computation (or supported thunked value), not an unapplied Pi/function.
- C ABI details are backend-local; `AP_C_RUNTIME_ABI` versions only the generated-code/runtime internal contract, not a public foreign-language ABI.

Sources:

- A Program README: <https://github.com/repyt-margorp/a-program>
- Prototype C backend README: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/src/prototype/c_backend/README.md>
- Current emitter: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/src/prototype/c_backend/emit.c>
- Current adapter: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/src/prototype/c_backend/main.c>
- Artifact persistence plan, especially AP1 and AP4: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md>

### 2.1 What is missing

The current prototype has no general representation for:

- multiple exported A Program names in one compiled product;
- imported target symbols supplied at link/load time;
- public symbol naming and visibility;
- library vs object vs executable product selection;
- a callable ABI for unapplied functions;
- runtime lifetime when code is used as a library;
- generated C headers or equivalent target interface descriptions;
- shared-library export filtering/versioning;
- CUDA device-link requirements;
- Wasm imports/exports/start distinction;
- Verilog/SystemVerilog top-module/port/DPI boundaries;
- cross-target declaration of “this semantic root is the entry” versus “this semantic root is an externally callable API”.

These omissions are normal for the first backend, but they should now be addressed by one common abstraction instead of independently growing target-specific flags.

---

## 3. Comparative study

## 3.1 C, ELF, GNU `ld`: translation units, symbols, entry, and layout are distinct

C itself gives translation units internal/external linkage, while the system linker resolves external symbols across object files. GNU `ld` adds a separate linker-script language. Its documentation is explicit that linker scripts primarily map input sections into output sections and control memory layout. `ENTRY(symbol)` selects the program entry point; `VERSION`/version scripts can constrain exported symbol visibility and define symbol versions.

This is an important separation:

- **C source/API declaration** decides which functions or variables are intended to exist.
- **Object symbol linkage/visibility** decides whether they can participate in linking.
- **The linker script** decides final layout, entry, and other output rules.
- **A version/export script** can narrow which symbols a shared library exposes.

A Program should preserve the same separation. An A Program “link script” should not primarily be an ELF memory-layout script. It should describe the semantic boundary first; an ELF linker script or version script can be generated later as a target-specific derivative.

References:

- GNU ld manual, linker scripts and `ENTRY`: <https://sourceware.org/binutils/docs/ld.html>
- GNU ld `SECTIONS`: <https://sourceware.org/binutils/docs/ld/SECTIONS.html>

### Lesson for A Program

Do not make `main` the only compiled shape. Do not make a GNU `.ld` file the canonical description of the A Program interface. Instead, generate `.ld`/version-script/DEF/export-list files from the target-neutral export set when a concrete platform needs them.

---

## 3.2 LLVM: semantic linkage is per global name, not per physical file

LLVM IR makes linkage a property of individual global values/functions: `private`, `internal`, `external`, `linkonce`, `weak`, and related forms. In particular, `internal` corresponds roughly to C `static`, while `external` values participate in linkage. LLVM therefore cleanly separates:

- module membership;
- symbol linkage;
- visibility;
- eventual object-file partition.

This is a useful model for A Program because a named A Program root can have a logical external status without requiring one object file per name.

Reference:

- LLVM Language Reference, Linkage Types: <https://llvm.org/docs/LangRef.html#linkage-types>

### Lesson for A Program

The link plan should classify names at least as:

- `private` / implementation-only;
- `export` / externally provided;
- `import` / externally required.

Weak/ODR-like policies can be deferred. The first design should reject duplicate external names rather than introducing weak resolution prematurely.

---

## 3.3 Rust: output kind is an explicit compiler product choice

Rust explicitly distinguishes multiple crate output kinds. A `bin` has a `main` and produces a runnable executable. `rlib` is compiler-consumable Rust library metadata/code. `staticlib` produces a system static library suitable for linking into non-Rust programs. `cdylib` produces a dynamic system library intended for foreign-language use. The Rust reference also notes that static libraries can unintentionally expose many dependency symbols and may require platform-specific export filtering such as ELF version scripts, macOS exported-symbol lists, or Windows module-definition files.

Reference:

- Rust Reference, Linkage: <https://doc.rust-lang.org/reference/linkage.html>

### Lesson for A Program

A single semantic module should be able to produce multiple product kinds without rewriting the source:

- executable;
- relocatable object/component;
- static library;
- shared library;
- backend-specific module.

“Executable vs library” should be a link-plan/build decision, not a property baked into the semantic definition called `main`.

---

## 3.4 Zig: external symbol exposure is explicit and object/library/executable production is separate

Zig has an unusually direct model for systems interoperation. `export` makes a declaration externally visible and usable under the relevant ABI; `@extern` creates a reference to an external symbol. Separately, the toolchain can build an object, static library, shared library, or executable. Zig’s documentation explicitly demonstrates exporting a C library.

References:

- Zig language documentation, exporting and `@extern`: <https://ziglang.org/documentation/master/>

### Lesson for A Program

The distinction between semantic declaration and external symbol binding should be explicit. A Program should not derive public target symbols merely because a definition happens to be globally named in the source. The link plan should select which global names cross the compilation boundary.

---

## 3.5 Go: package role and build mode are orthogonal

Go’s build system distinguishes ordinary package archives, executables, C archives, C shared libraries, plugins, and shared Go libraries. `-buildmode=c-archive` and `-buildmode=c-shared` create C-consumable products but only explicitly exported functions are callable externally. This shows a useful pattern: the compiler/build driver owns product construction, while the package’s internal name space is larger than its foreign ABI surface.

Reference:

- Go build modes: <https://pkg.go.dev/cmd/go/internal/help>

### Lesson for A Program

Do not equate “named in `.a`” with “foreign-exported”. The link plan should be a filter and adapter over the semantic name space.

---

## 3.6 OCaml and GHC: native object code is accompanied by language metadata/runtime contracts

OCaml native compilation is especially instructive. A compilation unit can produce native `.o` code plus `.cmx` metadata used for language-level linking and optimization, and `.cmi` interface information. OCaml can also produce a C object/shared object via `-output-obj`, while normal module interfaces remain distinct from the C object interface.

GHC similarly supports shared libraries and C-facing APIs, but Haskell runtime initialization and FFI requirements remain separate concerns. A shared library can be treated like a foreign library only after the Haskell/C boundary has been explicitly constructed.

References:

- OCaml native compiler: <https://ocaml.org/manual/5.5/native.html>
- GHC shared libraries and C API: <https://ghc.gitlab.haskell.org/ghc/doc/users_guide/shared_libs.html>

### Lesson for A Program

A Program has even stronger reasons than OCaml/Haskell to keep semantic metadata separate from machine code, because checked occurrences, classifiers, nominal identities, and proof/evidence structure must not become accidental target ABI. The `.a` artifact and the foreign binary should remain different products linked by a controlled lowering contract.

---

## 3.7 WebAssembly: module imports/exports and start are first-class and separate

Core WebAssembly modules are deployment/compilation units containing definitions plus explicit imports and exports, with an optional start function. A module can export many functions without having a start function, or can have a start function in addition to callable exports.

The Component Model/WIT goes further. A WIT `world` describes the complete set of imports and exports of a component; interfaces group functions/types, and the world is used as the basis for bindings generation. This is very close to the abstraction A Program needs.

References:

- WebAssembly Core modules: <https://webassembly.github.io/spec/core/syntax/modules.html>
- WIT worlds/interfaces: <https://component-model.bytecodealliance.org/design/wit.html>
- WIT design: <https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md>

### Lesson for A Program

The strongest precedent for the proposed A Program link plan is not GNU `ld`; it is closer to a small WIT `world` combined with backend-specific linker generation:

- imports are requirements;
- exports are provided capabilities;
- entry/start is optional and separate;
- the implementation may contain many more private names;
- bindings are generated from the declared boundary.

---

## 3.8 CUDA: one target may require more than one link stage

CUDA separate compilation demonstrates why the A Program abstraction must not assume “compile each source file, then call one linker”. CUDA can emit relocatable device code, use `nvlink` to combine device code, then embed/link the result with host objects using the host linker. `extern`/`static` matter across device compilation units, but the physical linking pipeline differs from ordinary C.

References:

- NVIDIA nvcc separate compilation: <https://docs.nvidia.com/cuda/cuda-compiler-driver-nvcc/#using-separate-compilation-in-cuda>
- CUDA Programming Guide, separate compilation: <https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/nvcc.html>

### Lesson for A Program

The target-neutral plan should specify **what must be linked**, not hard-code **how many native linker phases exist**. A CUDA backend may lower one A Program component into host object + relocatable device code + device link + final host link.

---

## 3.9 Verilog/SystemVerilog/Verilator: “top module” is not the same thing as `main`

Hardware targets make the executable/library distinction even clearer. A SystemVerilog design has modules and a selected top. Verilator can emit a reusable C++/SystemC model without `main`, or can generate/build a standalone binary. It supports DPI-C imports and exports and emits headers/wrappers for the C boundary. The wrapper that owns `main()` is separate from the model itself.

References:

- Verilator overview: <https://verilator.org/guide/latest/overview.html>
- Connecting to generated models and DPI: <https://verilator.org/guide/latest/connecting.html>
- Verilator command modes: <https://verilator.org/guide/latest/verilating.html>

### Lesson for A Program

For RTL lowering, “entry point” should generalize to **top/root selection**, not necessarily a function named `main`. An A Program link plan should allow the backend to interpret the selected root according to target profile:

- C executable: process entry wrapper;
- Wasm: optional start function;
- CUDA: callable host function and/or kernel export;
- Verilog: top module/ports, not process entry.

---


## 3.10 Lean, Rocq, Agda, Idris2, and F*: proof erasure is really a runtime-relevance discipline

The initial link-design draft was deliberately conservative about proof erasure. A closer comparison with established dependently typed languages sharpens the rule considerably:

> **Proof/evidence objects should normally disappear before native C ABI/code generation, but only after the type system has established that they are computationally irrelevant.**

This is stronger and more precise than either of the two naive rules:

- “keep every dependent/proof argument at runtime”; or
- “erase anything that looks like a proof”.

The mature systems do not use the second rule syntactically. They make erasure sound by constraining how erased evidence may be used.

### Lean 4: `Prop` is definitionally proof-irrelevant and runtime-irrelevant

Lean gives `Prop` a very strong semantic status. The current reference explicitly states that propositions have **run-time irrelevance: propositions are erased from compiled code**. Lean also restricts elimination from `Prop` into computational data, with limited subsingleton-style exceptions. The Lean 4 implementation paper describes the compiler pipeline as erasing proof terms before converting expressions to compiler IR and ultimately C.

This means a function such as conceptually:

```text
f : (x : A) -> ProofOf P x -> B
```

can compile to a runtime function whose ordinary calling convention contains only the computationally relevant arguments, provided the second argument is genuinely in `Prop` and no permitted computation depends on the identity of that proof.

The key distinction is visible in Lean's treatment of booleans versus propositions. `Bool` is computational and remains at runtime; `Prop` is logical and erased. If a proposition must drive computation, Lean requires computational evidence such as a `Decidable p`, which lives outside `Prop` and therefore supplies runtime information.

References:

- Lean Reference, Propositions: <https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/>
- Lean Reference, Booleans and Propositions: <https://lean-lang.org/doc/reference/latest/Basic-Types/Booleans/>
- de Moura and Ullrich, *The Lean 4 Theorem Prover and Programming Language*: <https://lean-lang.org/papers/lean4.pdf>

### Rocq/Coq: `Prop` proofs are non-informative for extraction

Rocq follows the same broad separation. Proofs in `Prop` are treated as non-computational and are ignored by extraction. The elimination rules prevent arbitrary case analysis on a `Prop` proof to manufacture data in `Set`/`Type`, precisely because such a branch would require proof structure that extraction intends to remove.

The equality recursor is an important precedent for A Program. Rocq's reference shows extraction of `eq_rec` to a function whose equality-proof argument has vanished and whose computational behavior is essentially the identity transport. In other words, the logical equality witness can justify a typed transport during checking while leaving no proof object in the extracted runtime program.

At the same time Rocq deliberately distinguishes logical disjunction in `Prop` from computational alternatives such as `sumbool`/data in `Set`. The latter must remain because programs may branch on which constructor is present.

References:

- Rocq 9.1, inductive types and elimination from `Prop`: <https://rocq-prover.org/doc/V9.1.0/refman/language/core/inductive.html>
- Rocq extraction manual: <https://docs.rocq-prover.org/master/refman/addendum/extraction.html>
- MetaRocq erasure description: <https://rocq-prover.org/p/rocq-metarocq-erasure/latest>

### Agda: erase what is marked runtime-irrelevant, not merely what looks like a proof

Agda exposes the more general principle directly. With erasure enabled, an argument annotated `@0`/`@erased` is absent at runtime. The type checker then enforces that ordinary runtime computation cannot depend on that erased value. Constructor fields and record fields marked runtime-irrelevant are removed from compiled representations; erased higher-order/function arguments may be represented only by placeholders where required by the intermediate calling convention.

Agda is especially instructive because a value need not be a proposition in order to be erased. A vector length can be `@0 Nat`: it is ordinary data at the type level, but the program promises not to inspect it at runtime. Conversely, something that looks proof-like but is used computationally cannot simply be erased.

References:

- Agda, Run-time Irrelevance: <https://agda.readthedocs.io/en/latest/language/runtime-irrelevance.html>
- Agda command-line erasure options: <https://agda.readthedocs.io/en/latest/tools/command-line-options.html#erasure>

### Idris2: quantitative types make the runtime boundary explicit

Idris2's Quantitative Type Theory gives each binder a multiplicity. Quantity `0` means the variable has zero runtime occurrences and is guaranteed to be erased; quantity `1` is linear runtime use; unrestricted binders are runtime-relevant unless further optimized. The implementation notes state that `0`-multiplicity constructor arguments are erased completely and `0`-multiplicity function arguments are replaced by an erased placeholder in the compiler IR where necessary.

This yields a particularly useful lesson for A Program: **types and values do not determine relevance by themselves**. A type index such as a vector length may be erased, while a type argument may intentionally remain runtime-relevant if the program performs type-directed computation. Idris2 therefore treats “is this needed at runtime?” as an independently tracked property.

References:

- Idris2, Multiplicities: <https://idris2.readthedocs.io/en/latest/tutorial/multiplicities.html>
- Idris2, Implementation Overview / Erasure: <https://idris2.readthedocs.io/en/latest/implementation/overview.html>
- Idris2, Custom backend cookbook / `Erased`: <https://idris2.readthedocs.io/en/latest/backends/backend-cookbook.html>

### F*: ghost computations establish the same phase separation

F* and Pulse use ghost/erased computations for values needed by verification but unavailable to executable code. A non-ghost computation is rejected if it tries to compute a runtime value from an erased value. Ghost functions may inspect such values because the entire ghost computation itself disappears at runtime.

Reference:

- F*/Pulse, Ghost Computations: <https://fstar-lang.org/tutorial/book/pulse/pulse_ghost.html>

### Comparative summary

| System | Main erasure boundary | Why erasure is sound | Runtime-relevant counterexample |
|---|---|---|---|
| Lean 4 | terms in `Prop` | proof irrelevance + restricted elimination; compiler erases proofs before IR/C | `Bool`, `Decidable p`, ordinary `Type` data |
| Rocq/Coq | proofs in `Prop` during extraction | `Prop` is non-informative; elimination into computational sorts is restricted | `Set`/`Type` data such as computational sums |
| Agda | `@0` / runtime-irrelevant positions | checker forbids runtime dependence on erased values | un-erased indices/types/values used in computation |
| Idris2 | QTT quantity `0` | quantitative usage checking guarantees zero runtime occurrences | unrestricted type/value arguments may be inspected |
| F* | erased/ghost values and computations | non-ghost code cannot obtain runtime data from erased inputs | ordinary `Tot`/stateful runtime computation |

### Direct consequence for A Program

A Program should not make its C backend carry proof terms merely because the semantic artifact contains them. The desirable pipeline is:

```text
checked semantic graph
        |
        | establish runtime relevance / erasure legality
        v
runtime-relevant graph
        |
        | ABI lowering
        v
C / CUDA / Wasm / RTL
```

The erasure pass belongs **after semantic checking and before target ABI lowering**. It should not mutate the canonical `.a` artifact and it should not be performed by heuristic recognition of names such as `proof`, `witness`, `LT`, or `Identity`.

The key invariant should be:

> If a term is erased, no observable runtime computation, control-flow choice, data-layout choice, foreign call argument, or exported ABI result may depend on which inhabitant of that erased position was supplied.

This naturally suggests a target-independent relevance classification on binders/fields/occurrences, analogous to Agda `@0` or Idris2 quantity `0`, even if A Program does not expose such an annotation in surface syntax initially.

A first implementation can infer/mark three classes internally:

```text
runtime      must survive into executable lowering
compiletime  may affect checking/lowering but has no runtime representation
erased       proof/evidence with no runtime observation
```

`compiletime` and `erased` may eventually collapse in a simpler core, but keeping the distinction during implementation is useful: a type/index may be consulted by the compiler to choose a representation and then disappear, whereas a proof witness should normally contribute no target-level value at all.

### Equality/Identity requires special care, but not necessarily runtime proof objects

The current C backend's conservative handling of Identity should **not** be read as evidence that equality proofs ought to remain as C values. Lean and Rocq demonstrate the opposite: equality evidence can justify typed transport during checking and then disappear from runtime code.

However, this is sound only if A Program's Identity eliminator satisfies an erasure-compatible discipline. There are two broad implementation strategies:

1. **Proof-irrelevant/erased Identity.** Identity witnesses are runtime-irrelevant, and any permitted transport lowers to identity/no-op representation changes after type checking. This is closest to Lean/Rocq extraction.
2. **Relevant identity/path data.** If A Program intentionally allows programs to inspect identity/path structure to compute ordinary data, those identities are not proofs in the Lean/Rocq extraction sense and cannot be generally erased.

Therefore the backend should not decide this locally. The language semantics should establish whether a given Identity occurrence is in an erasable proof fragment. Once that is established, C lowering should eliminate the witness aggressively rather than box it.

### Recommended A Program rule

For native compilation, adopt the following rule:

> **Erase all semantically certified runtime-irrelevant evidence, including ordinary proof terms and erasable equality transports, before constructing the public target ABI. Preserve only computational witnesses whose constructors/contents may affect runtime behavior.**

This means a theorem-carrying function can have a rich A Program type while exposing a small C signature.

Conceptually:

```text
A Program:
    sort : (xs : Vec Int n) -> Sorted (sort xs) -> Result n

native ABI after relevance analysis:
    ap_result ap_sort(ap_context *, ap_vec_i32 xs);
```

The exact example depends on A Program's eventual relevance rules; the important part is that `Sorted ...` is absent only when the checker has certified it as non-computational evidence.

---

## 3.11 Verification completeness and compilation admissibility must be separate

The comparison above suggests a further distinction that is especially important for A Program's incremental `Solve` model:

> **A target backend should not require proof completion merely because proof checking has not finished. It should require only that the runtime-relevant program is sufficiently determined to lower.**

This separates two independent questions:

1. **Verification status:** have all propositions, typing obligations, termination arguments, identities, refinements, and other proof obligations been discharged?
2. **Compilation admissibility:** after erasing semantically runtime-irrelevant material, is there a closed target-level computation with a known representation and realizable foreign dependencies?

A Program currently tends to couple these questions because ordinary backend selection is reached through ordinary `Solve` or through an explicit trusted completed image. That is a reasonable first prototype policy, but it should not become the long-term compilation semantics.

### Evidence from other systems

Lean is the clearest precedent. `sorry` provisionally closes an unfinished proof by introducing `sorryAx`; Lean warns about it, but the declaration remains usable. Proofs in `Prop` are runtime-irrelevant and erased from compiled code. Thus an unfinished *logical* proof need not itself become a runtime object. Lean separately rejects genuinely non-computational definitions that try to manufacture runtime data from principles such as choice by requiring them to be marked `noncomputable` and omitting executable code. Lean's own programming documentation also warns that an incorrect provisional proof can justify an unsafe high-level assumption such as an array bound and thereby cause a runtime failure. The lesson is therefore not “unfinished proofs are harmless”, but rather “proof completeness and executable-code existence are different predicates”.

References:

- Lean `sorry`: <https://lean-lang.org/doc/reference/latest/Tactic-Proofs/Tactic-Reference/#Lean.Parser.Tactic.tacticSorry>
- Lean axioms / `sorryAx`: <https://lean-lang.org/doc/reference/latest/Axioms/>
- Lean `Prop` runtime erasure: <https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/>
- Lean axioms and executable computation: <https://lean-lang.org/theorem_proving_in_lean4/Axioms-and-Computation/>
- Lean warning about provisional proofs and runtime array safety: <https://lean-lang.org/functional_programming_in_lean/Programming___-Proving___-and-Performance/Summary/>

Rocq makes almost exactly the same logical/informative distinction during extraction. A logical axiom in `Prop` has no extracted computational content and can disappear with a warning, whereas an informative axiom in `Set`/`Type` needs a realization; the extractor cannot invent the runtime value. This is a very useful model for A Program's distinction between an unfinished proof obligation and an unfinished computation.

Reference:

- Rocq program extraction, realizing axioms: <https://rocq-prover.org/doc/V8.20.0/refman/addendum/extraction.html>

Idris2's backend interface gives another useful operational precedent: top-level error definitions can represent source holes, `idris_crash`, or unreachable case-tree branches. Its backend documentation explicitly notes that incomplete programs may be executed for testing as long as execution never demands the hole. Thus even a computational hole can be represented as a runtime trap rather than making all code generation impossible, although this is a much stronger and less safe mode than simply erasing unfinished proofs.

Reference:

- Idris2 custom backend cookbook: <https://idris2.readthedocs.io/en/latest/backends/backend-cookbook.html>

Agda similarly separates ordinary checking policy from a stricter trusted subset: `--allow-unsolved-metas` permits interface generation despite unsolved metas, while `--safe` forbids that option. This is further evidence that “trusted/verified artifact” and “artifact usable for continued compilation work” need not be identical statuses.

References:

- Agda `--allow-unsolved-metas`: <https://agda.readthedocs.io/en/latest/tools/command-line-options.html>
- Safe Agda: <https://agda.readthedocs.io/en/latest/language/safe-agda.html>

### Recommended two-axis state model

Do not encode compilation state as one linear enum such as:

```text
unchecked -> pending -> checked -> compiled
```

Instead keep at least two axes.

Verification axis:

```text
verified
pending
assumed/trusted
rejected
```

Runtime/codegen axis:

```text
lowerable
lowerable-with-runtime-stubs
not-lowerable
```

For example, all of the following should be representable:

```text
verified + lowerable
pending  + lowerable
assumed  + lowerable
pending  + lowerable-with-runtime-stubs
pending  + not-lowerable
rejected + not-lowerable
```

The important new state is:

```text
pending verification + fully lowerable runtime graph
```

That state should normally be compilable.

### Default native-compilation policy

For C/CUDA/Wasm/native compilation, the recommended default is:

> **Compile even when proof obligations remain unfinished, provided every unresolved item is absent from the runtime-relevant graph after sound relevance analysis and all runtime-relevant terms/imports/representations are lowerable.**

In that case:

- erase the unfinished proof/evidence positions exactly as solved proof positions would be erased;
- do **not** emit a proof interpreter or a general dynamic type checker into C;
- produce the requested C/object/library/executable normally;
- record that the product is not fully verified in the generated link receipt / metadata;
- retain provenance identifying which obligations were pending or assumed when the product was built.

This should be the ordinary development-oriented behavior rather than an exceptional `--trust-image`-style escape hatch.

A stricter build mode should remain available:

```text
--require-verified
```

for release/certification pipelines that want code generation to fail unless all required proof obligations have been discharged.

### Runtime holes are a different case

A proof hole and a computational hole must not be conflated.

Suppose an unresolved item is needed to produce a runtime `Int32`, choose a constructor, provide a foreign pointer, determine an effect operation, or otherwise affect observable target execution. Erasing it would change the program. Then one of three things must happen:

1. reject ordinary compilation of that export;
2. under an explicit development mode, lower the hole to a deterministic runtime trap/stub;
3. if the missing fact is decidable from runtime data, emit a concrete runtime check and execute the safe branch/failure path.

A possible explicit mode is:

```text
--allow-runtime-holes=trap
```

This should be distinct from ordinary pending-proof compilation.

### Unproved proofs must not silently authorize unsafe lowering

There is one crucial caveat to “compile pending proofs by default”. A backend must not treat an unproved theorem as permission to perform target-level transformations whose safety depends on that theorem.

Examples include:

- deleting a bounds check because an unfinished proof claims the index is in range;
- narrowing `Nat` to `uint32_t` because an unfinished range proof claims the value fits;
- using `__builtin_unreachable` because an unfinished exhaustiveness/impossibility proof says a branch cannot occur;
- emitting unchecked pointer casts or aliasing assumptions justified only by pending evidence;
- selecting an ABI layout that is valid only if an unfinished equality/refinement is true.

For pending verification, the backend should use the **conservative runtime semantics**:

```text
proof-complete
    -> proof-directed optimization / check elimination may be permitted

proof-pending
    -> proof term is erased,
       but proof-dependent unsafe optimization is disabled;
       preserve runtime checks or a representation that is valid independently
       of the unfinished theorem
```

This is the right interpretation of “type checking is prior computation”. The *proof object* need not survive, and the *type checker* need not run at runtime, but information learned from successful checking may legitimately enable specialization or check elimination. If the checking result is unavailable, code generation should fall back to a representation/operation that does not assume it.

### Dynamic checking should be narrow and explicit

An unfinished `.a` does not imply that the generated program should dynamically re-run A Program's type system. General proof obligations are not necessarily decidable at runtime, and carrying the full checker into every executable would destroy the phase separation that native compilation is supposed to achieve.

Dynamic checks make sense only for reifiable executable predicates such as:

- array/index bounds;
- integer-range checks;
- tag/layout checks at an FFI boundary;
- null/alignment checks;
- explicitly compiled decidable predicates.

Thus a later option such as:

```text
--pending-obligations=runtime-check
```

should mean “retain/reify supported executable guards”, **not** “embed the theorem prover”. Unsupported logical obligations remain metadata/assumptions, not runtime checker state.

### Consequence for `.a` and link manifests

The link layer should therefore carry or derive a compilation receipt such as:

```text
verification:
    status: pending
    solved: 1842
    pending: 3
    assumed: 0

runtime:
    holes: 0
    unresolved_imports: 0
    lowering: complete

proof_dependent_optimizations:
    enabled: false
```

The exact wire syntax is not important yet. The architectural point is that **verification completeness is metadata/provenance, not necessarily a prerequisite for native code generation**.

This also fits A Program's own partial-artifact direction: the current artifact prototype already distinguishes descriptive loading, completion bytes, ordinary revalidation, explicit trust, and unfinished continuation/progress state. The future compiler should consume that richer state without reducing it to a single “may/may not emit C” bit.

---

## 4. Synthesis: separate four kinds of granularity

A Program should explicitly separate four granularities that are often conflated.

### 4.1 Semantic module/artifact granularity

A `.a` image is the target-independent semantic container. It retains named roots, dependencies, typed occurrences, nominal structure, and unfinished obligations according to the artifact contract.

This layer knows nothing about ELF symbol tables, C headers, CUDA kernels, or RTL ports.

### 4.2 Logical external-name granularity

The link plan selects **A Program semantic names** as exports/import requirements. This is the user-facing API granularity.

Example logical exports:

- `sort`
- `lookup`
- `main`

Each is a semantic root, not a promise of one `.o` file.

### 4.3 Physical code-generation unit granularity

The backend decides how to partition the joint reachable graph into generated units:

- one C translation unit;
- multiple C translation units;
- one `.o` with many global symbols;
- several `.o` files;
- one Wasm module;
- several CUDA device objects;
- one RTL module hierarchy.

This is an optimization/build concern. It may change without changing the link plan or `.a` semantics.

### 4.4 Final product granularity

The link driver selects the final product:

- executable;
- object/component;
- static library;
- shared library;
- Wasm module/component;
- CUDA object/fatbin/executable;
- RTL source/module/simulation library.

This separation avoids prematurely encoding one target’s file model into the language.

---

## 5. Proposed architecture

```text
                   canonical semantic layer

       source .p
          |
          v
   checked / partial .a
          |
          |  select named roots
          v
 +--------------------------+
 | A Program Link Plan      |
 |                          |
 | product kind             |
 | semantic exports         |
 | semantic imports         |
 | optional entry/top       |
 | target + ABI profile     |
 | runtime policy           |
 +--------------------------+
          |
          v
 +--------------------------+
 | target-neutral selection |
 | + dependency closure     |
 +--------------------------+
          |
          v
 +--------------------------+
 | backend lowering         |
 +--------------------------+
          |
   +------+------+------------------+
   |             |                  |
   v             v                  v
 C/C++         CUDA              RTL/Wasm/...
   |             |                  |
 .c/.h/.o     host.o + rdc       .sv/.wasm/...
 export map     nvlink plan       interface data
   |             |                  |
   +-------------+------------------+
                 |
                 v
       target-native linker/build
```

The link plan is downstream from `.a`. It must never be accepted as proof or typing evidence.

---

## 6. Recommended semantic model: `component` first, executable as a wrapper

The reusable unit should be a **component with named exports/imports**. An executable should be a special product that adds an entry wrapper around one component export.

This is preferable to making `main` fundamental.

### 6.1 Component

A component has:

- zero or more exported semantic names;
- zero or more external requirements/import bindings;
- one shared internal dependency closure;
- no requirement for a process entry.

### 6.2 Executable

An executable adds:

- exactly one process/runtime entry selection;
- target startup policy;
- runtime allocation/initialization;
- exit-status conversion;
- optional argument/environment bridge in future.

Thus the current prototype becomes conceptually:

```text
component export: main
        +
C executable wrapper(main)
        =
current generated executable
```

rather than “the definition `main` intrinsically means C `main()`”.

### 6.3 Why this is better for future targets

The same semantic component can be wrapped as:

- C executable;
- C static library;
- C shared library;
- Wasm module with no start;
- Wasm module with a start wrapper;
- CUDA host-callable library;
- Verilog top module.

No semantic source rewrite is required.

---

## 7. Proposed link-plan file

Working extension: **`.aplink`**.

The exact syntax is not important yet; the semantic fields are. A simple declarative text format is preferable to putting this information in `.p` or `.a`.

Example:

```text
aplink 1

artifact "build/program.a"

target c {
    product static-library "libsort.a" {
        export sort as "ap_sort" abi c-flat-v1
        export sort_generic as "ap_sort_generic" abi ap-boxed-v1
    }
}

target c {
    product executable "sort-demo" {
        entry main
    }
}
```

A different target can select the same semantic names:

```text
target wasm {
    product module "sort.wasm" {
        export sort as "sort"
        export version as "version"
    }
}
```

And an RTL target might use:

```text
target systemverilog {
    product rtl "sort_accel" {
        top sortKernel
        profile ready-valid-v1
    }
}
```

The important point is that `entry`, `export`, and `top` are product/link concepts, not new proof rules.

---

## 8. Export model

An export record should minimally contain:

```text
semantic-name
external-name
ABI profile
target visibility
```

For example:

```text
export sort as "ap_sort" abi c-flat-v1
```

### 8.1 Semantic name

This must resolve through the existing A Program module/name-selection mechanism, preserving nominal identities and current whole-module acceptance rules.

### 8.2 External name

This is target-facing and must never modify the semantic identity of the A Program definition. Two target plans may export the same semantic definition under different external names.

### 8.3 ABI profile

The ABI profile states **how an accepted A Program type crosses the foreign boundary**. This must be validated by a target-specific type-lowering rule.

### 8.4 Visibility

Initial policy should be simple:

- not selected => private/internal to generated component;
- selected export => globally visible target symbol/interface item;
- target support symbols/runtime => hidden unless explicitly part of the ABI.

Do not initially add weak linkage or symbol interposition semantics.

---

## 9. Import model

There are two different forms of “import” and they should not be conflated.

### 9.1 A Program semantic/source imports

These are current language/module dependencies such as `import name;` supplied by another A Program provider. They belong to the semantic checking layer and may be resolved before target lowering.

### 9.2 Foreign target imports

These are target ABI requirements: C symbols, host functions, Wasm imports, CUDA runtime hooks, DPI functions, etc.

A target import mapping should bind a **previously declared semantic/Oracle contract** to a concrete target symbol or intrinsic. The link plan should not invent an untyped foreign function merely by spelling a symbol name.

Conceptually:

```text
bind host.log -> c.symbol "app_log"
bind host.alloc -> c.symbol "app_alloc"
```

The semantic type/effect contract must come from A Program’s existing or future host/Oracle declaration mechanism; the link plan supplies the target realization.

### 9.3 Why this matters

This keeps the dependency direction:

```text
A Program semantic contract
        |
        v
foreign binding choice
        |
        v
target symbol/intrinsic
```

and avoids:

```text
C prototype text
        |
        v
pretend A Program semantic contract
```

---

## 10. ABI design: do not require one representation strategy

A Program should support at least two ABI families for C-like targets.

## 10.1 `ap-boxed-v1`: semantically broad opaque runtime ABI

This ABI exposes A Program values through opaque runtime handles. It is the natural first general library ABI because the current C backend already represents runtime values as `struct ap_value *` internally.

Conceptual C header:

```c
struct ap_runtime;
struct ap_value;

enum ap_status ap_module_create(struct ap_runtime **out);
void ap_module_destroy(struct ap_runtime *r);

enum ap_status ap_export_sort(
    struct ap_runtime *r,
    struct ap_value *arg,
    struct ap_value **result);
```

Advantages:

- can represent higher-order/runtime-rich values without immediately freezing C struct layouts;
- preserves nominal distinction inside the runtime;
- gives the linker work a usable public boundary before aggressive representation lowering exists.

Disadvantages:

- foreign callers need runtime helpers;
- not idiomatic C;
- ownership/lifetime rules must be explicit;
- not appropriate as the final GPU/RTL ABI.

## 10.2 `c-flat-v1`: restricted native C ABI

This profile lowers only a proved supported first-order subset to direct C representations, for example initially:

- `#Int32` -> `int32_t`/specified bit-preserving representation;
- `#Int64` -> `int64_t`/specified bit-preserving representation;
- selected byte strings -> `{ptr,len}` or caller-provided buffer contract;
- explicitly stabilized POD ADTs in a later phase.

Anything higher-order, dependent, effectful in an unsupported way, or representation-unstable must reject rather than silently box or erase unless the ABI profile explicitly permits boxing.

### 10.3 Proof and dependent data

The comparative evidence from Lean, Rocq, Agda, Idris2, and F* supports a stronger design than the original conservative wording here.

**Certified runtime-irrelevant proof/evidence parameters should not cross the native ABI at all.** They should be erased before wrapper-signature construction. A rich dependent source type may therefore lower to a much smaller C signature.

The important qualifier is **certified runtime-irrelevant**. Erasure must come from language semantics/relevance analysis, not from a backend heuristic that guesses that an IADT or argument “looks like a proof”.

Recommended rule:

- erase a binder/field when the semantic checker or a verified relevance pass establishes zero runtime use;
- erase ordinary proof terms in a proof-irrelevant fragment;
- erase equality/Identity witnesses when their permitted elimination/transport is representation-erasing;
- preserve computational witnesses whose constructor or value may influence ordinary computation;
- allow type/index information to be used at compile time for specialization/layout and then disappear from the runtime ABI when its runtime multiplicity is zero;
- if relevance cannot be established, `c-flat-v1` must reject rather than silently keep an accidental proof ABI; `ap-boxed-v1` may keep genuinely computational values, but should not box evidence already certified erased.

Thus the intended ordering is:

```text
checked typed occurrence
   -> relevance / proof erasure
   -> representation lowering
   -> foreign ABI wrapper
   -> C symbol
```

This mirrors Lean/Rocq extraction while retaining the more general Agda/Idris2 insight that runtime relevance is not identical to “is syntactically a proof”.

---

## 11. Callable functions require a new wrapper layer

The current emitter only accepts a closed returning entry and rejects an unapplied Pi. A linkable library needs wrappers for functions.

Do **not** expose internal generated Core functions such as `t17` directly. They are backend implementation details with environment/runtime calling conventions.

Instead generate one stable wrapper per selected export.

Conceptually:

```c
/* private generated Core graph */
static struct ap_value *t17(struct ap_runtime *, const struct ap_env *);

/* stable foreign wrapper */
int32_t ap_sort_i32(/* lowered args */) {
    /* create/reuse runtime */
    /* obtain closure/value for semantic export */
    /* marshal C args to A Program runtime values */
    /* apply */
    /* execute required computation boundary */
    /* marshal result back */
}
```

This wrapper is where the target ABI lives. Core-node functions remain private.

---

## 12. Runtime lifetime for libraries

The current executable allocates one runtime in `main` and frees it on exit. Libraries need an explicit lifetime model.

Two modes are useful.

### 12.1 Per-call runtime

Each exported flat function creates and destroys a runtime for one call.

Pros:

- simplest initial implementation;
- no cross-call ownership state;
- easy failure isolation.

Cons:

- allocation overhead;
- cannot naturally keep A Program values/closures across calls.

### 12.2 Explicit context runtime

Expose a runtime/module context:

```c
ap_context *ap_context_create(void);
void ap_context_destroy(ap_context *);
```

Export wrappers accept `ap_context *`.

Pros:

- persistent handles and closures;
- amortized allocation;
- better fit for embedding and plugins.

Cons:

- lifetime and thread-safety contract must be specified.

### Recommendation

Implement the **explicit context ABI as the fundamental boxed ABI**, and allow generated flat wrappers to use a private one-shot context as syntactic convenience. This avoids later changing ownership semantics when reusable values are introduced.

---

## 13. Product kinds

The link plan should expose semantic product kinds while allowing each backend to map them to native formats.

Recommended initial set:

```text
object       relocatable/linkable unit, no process entry
library      reusable collection of exports
executable   entry-bearing runnable product
module       backend-native module/component where “library” is misleading
source       generated target source for inspection/debug/build integration
```

Backends refine these with target options.

### C family

```text
source -> .c + .h
object -> .o/.obj
library(static) -> .a/.lib
library(shared) -> .so/.dylib/.dll
executable -> native executable
```

### WebAssembly

```text
module -> .wasm with imports/exports
executable-like command -> module/component + start/command convention
```

### CUDA

```text
object -> host object and/or relocatable device code
library -> static/shared host library plus device-link metadata
executable -> device link + host link
```

### Verilog/SystemVerilog

```text
source/module -> .sv hierarchy with selected top
simulation-library -> generated simulator/C++ integration product
executable -> optional simulator wrapper, not the default meaning of compilation
```

---

## 14. Entry semantics

`entry` must be a property of a **product**, not of a global A Program name.

Example:

```text
product executable "demo" {
    entry main
}
```

The same `main` definition can also be exported from a library if desired:

```text
product shared-library "libdemo.so" {
    export main as "demo_run"
}
```

Target interpretation:

- C: generate C `main`/platform startup wrapper around the chosen root;
- Wasm: optionally map to start/command convention;
- CUDA: host entry wrapper, not a device kernel by default;
- RTL: `entry` is generally invalid; use `top`.

This prevents C’s process model from contaminating all targets.

---

## 15. One export is not one object file

This point should be explicit in the specification.

Suppose semantic exports `f` and `g` share a large internal dependency graph `D`.

A bad invariant is:

```text
f -> f.o containing D
g -> g.o containing D
```

This duplicates code or requires difficult cross-object nominal/runtime coordination.

The correct logical model is:

```text
component {
    export f
    export g
    private shared D
}
```

The backend may initially emit:

```text
component.o
  global ap_f
  global ap_g
  local/private D
```

Later it may partition by strongly connected components, hot/cold regions, COMDAT/linkonce-like groups, or target section GC without changing the public contract.

### Recommendation

For the first implementation:

> **One link plan product -> one jointly collected semantic dependency closure -> one target compilation unit by default -> multiple generated public wrappers.**

Add physical partitioning only as an optimization phase.

---

## 16. Generated companion metadata

Each successful lowering should produce a machine-readable **link receipt** in addition to target code/binaries.

This is not proof evidence. It records what the backend emitted.

Suggested fields:

```json
{
  "format": "ap-link-receipt-v1",
  "target": "c",
  "product": "static-library",
  "runtime_abi": "ap-c-runtime-v1",
  "exports": [
    {
      "semantic": "sort",
      "external": "ap_sort",
      "abi": "c-flat-v1"
    }
  ],
  "imports": [],
  "generated": ["sort.o", "sort.h"],
  "native_link_requirements": ["ap_runtime"]
}
```

Uses:

- reproducible build integration;
- debugging symbol mappings;
- target linker command generation;
- auditing that only intended symbols are public;
- future package managers/build systems.

The receipt should contain a digest/reference to the input artifact and link plan for reproducibility, but the digest must not be treated as proof authenticity.

---

## 17. Target-specific generated linker/control files

The target-neutral link plan should generate native control files where useful.

### ELF/Linux

Generate an ELF version/export script from selected public symbols, e.g. conceptually:

```text
APROGRAM_1 {
    global:
        ap_sort;
        ap_lookup;
    local:
        *;
};
```

A full memory-layout `.ld` script should only be generated when the target profile actually needs section placement/embedded layout.

### macOS

Generate an exported-symbols list or equivalent linker arguments.

### Windows

Generate a `.def` file or the appropriate explicit DLL exports.

### Embedded/bare metal

Here a real GNU/LLD linker script may become first-class output because memory regions, sections, startup entry, and symbols are part of the deployment contract.

### Key rule

> The `.aplink` file is the source of truth for semantic exports. Platform linker files are generated projections.

---

## 18. Target profiles

A target profile packages lowering decisions that should not be repeated on every export.

Example:

```text
target c profile hosted-c11-v1 {
    runtime dynamic
    visibility hidden-by-default
    integer #Int32 native-i32
    integer #Int64 native-i64
    text counted-bytes
}
```

Possible profiles:

- `c-hosted-v1`
- `c-freestanding-v1`
- `cuda-device-v1`
- `wasm-component-v1`
- `systemverilog-ready-valid-v1`

Profiles should be versioned. A profile version is a backend contract, not an A Program theorem.

---

## 19. CUDA-specific implications

CUDA demonstrates that “linkable name” needs target annotations beyond a plain C symbol.

Potential export roles:

```text
export foo role host-function
export kernel role kernel
```

The CUDA backend may then emit:

```text
host wrapper object
relocatable device code
nvlink input set
optional registration/fatbin data
```

The semantic definition remains one A Program root. The role determines the target calling boundary.

A first CUDA implementation should reject arbitrary closures/effects at kernel boundaries unless a specific lowering exists. Buffer/scalar ABI profiles should be explicit.

---

## 20. Verilog/SystemVerilog-specific implications

RTL cannot inherit the C function ABI model.

A target plan must map a semantic root to a hardware interface contract, for example:

```text
product rtl "adder" {
    top add
    profile ready-valid-v1
}
```

A profile could map a total first-order function to ports:

```text
clk
reset
in_valid
in_ready
arg0[N-1:0]
arg1[M-1:0]
out_valid
out_ready
result[K-1:0]
```

A pure combinational profile could omit clock/handshake where proven safe.

The important architectural analogy to Verilator is that the generated **model/module** should exist independently from a generated simulation `main`. A testbench/simulator wrapper is a separate product.

For mixed C/RTL use, DPI-C is a precedent for generating both target declarations and C headers/wrappers from one interface contract.

---

## 21. WebAssembly-specific implications

For Wasm, the A Program link plan can map unusually directly to imports/exports:

```text
product module "m.wasm" {
    export sort as "sort"
    import log bind wasm "env" "log"
}
```

A later Component Model backend could translate an A Program interface subset into WIT-like interface/world information. The key conceptual import is WIT’s separation of:

- implementation;
- named interface;
- world/import-export contract;
- bindings generation.

A Program should emulate that separation rather than copy WIT syntax wholesale.

---

## 22. Proposed validation and compilation-admissibility pipeline

Compilation of a link plan should proceed in this order:

1. **Read artifact inertly.** Loading confers no proof authority.
2. **Resolve every requested semantic export/entry/top name.**
3. **Read verification/progress state without requiring completion by default.** Distinguish verified, pending, assumed/trusted, and rejected obligations.
4. **Compute the union of reachable semantic dependencies for all selected roots.**
5. **Run target-independent relevance analysis/erasure.** Determine which unresolved items are proof-only and which remain runtime-relevant.
6. **Check compilation admissibility.** Every runtime-relevant term must be determined, representable, and backed by a realizable runtime operation/import. Pending erased proof obligations do not fail this step.
7. **Apply the requested verification policy.** `--require-verified` rejects pending/assumed obligations; the default development policy may continue with pending erased proofs.
8. **Select conservative lowering rules.** Do not use pending proofs to authorize unsafe representation narrowing, check elimination, unreachable assumptions, or other proof-dependent optimizations.
9. **Classify every reachable Oracle/operation under the chosen target profile.**
10. **Validate each external export against its ABI profile.**
11. **Resolve every required foreign import binding.**
12. **Choose backend physical partitioning.**
13. **Emit private implementation code and public wrappers.**
14. **Emit target interface files (`.h`, WIT-like data, DPI header, etc.).**
15. **Emit target-native linker/export control files if required.**
16. **Optionally invoke the native compiler/linker for the requested product.**
17. **Emit a link receipt containing verification status, assumptions/pending obligations, runtime-hole status, target/ABI versions, and whether proof-dependent optimizations were enabled.**
18. **Run differential/ABI tests where the backend provides them.**

No stage may reinterpret successful target compilation/linking as proof that the A Program source was verified. Conversely, unfinished proof work should not by itself prevent target code generation when the erased runtime graph is complete.

---

## 23. Suggested API refactor of the current C prototype

The current function:

```c
int pg_c_emit(FILE *output, const struct pg_occurrence *root, const char **error);
```

encodes “one closed entry -> one executable C file”.

Refactor toward two layers.

### 23.1 Target-neutral selected component

Conceptually:

```c
struct pg_link_export {
    const char *semantic_name;
    const struct pg_occurrence *root;
    const char *external_name;
    enum pg_abi_profile abi;
};

struct pg_link_component {
    size_t export_count;
    const struct pg_link_export *exports;
    /* resolved imports and shared selected module identity */
};
```

This structure should borrow existing checked semantic roots and should not copy a second type graph.

### 23.2 C emitter

Conceptually:

```c
int pg_c_emit_component(
    FILE *source,
    FILE *header,
    const struct pg_link_component *component,
    const struct pg_c_profile *profile,
    struct pg_link_receipt *receipt,
    const char **error);
```

Then executable generation becomes another layer:

```c
int pg_c_emit_executable_wrapper(
    FILE *source,
    const struct pg_link_export *entry,
    const struct pg_c_profile *profile,
    const char **error);
```

The Core DAG emitter can remain largely private underneath this.

---

## 24. C backend migration sequence

A low-risk sequence from the current prototype is:

### L1. Separate `main` from Core emission

Replace unconditional generated `int main(void)` with a private callable root function.

Current shape:

```text
Core DAG + main wrapper
```

New shape:

```text
Core DAG + private root constructor
```

Then add `main` only for executable products.

### L2. Multiple selected closed exports

Allow multiple already-supported closed roots to share one emitted source/runtime component. Generate one public no-argument wrapper per export.

This tests multiple named symbols without yet solving Pi argument marshalling.

### L3. Generate header + visibility map

Generate stable declarations and hide all unselected support symbols.

### L4. Emit object/static library

Add product driver support for `.o` and `.a` using the host C compiler/ar tool. Keep generated C as an inspectable mode.

### L5. Introduce target-independent runtime-relevance erasure and compilation admissibility

Before freezing a public ABI, classify reachable binders/fields/occurrences as runtime-relevant or erasable. Remove certified proof/evidence arguments from the backend IR and assert that generated target control flow cannot inspect them.

At the same milestone, separate **verification completeness** from **runtime lowerability**. A selected export with pending proof-only obligations should remain compilable when its runtime-relevant graph is complete. Identity/equality cases should initially be accepted when their runtime transport/representation is explicitly supported; an unfinished equality proof may be erased, but it must not authorize a proof-dependent unsafe lowering.

### L6. Add `ap-boxed-v1` callable Pi wrappers

Marshal remaining **runtime-relevant** arguments/results as opaque A Program runtime values and introduce explicit runtime context lifetime. Erased proofs must not become boxed handles.

### L7. Add restricted `c-flat-v1`

Implement type-directed direct ABI lowering for fixed machine types and carefully specified byte/string structures.

### L8. Shared library + platform export control

Generate ELF version scripts, macOS export lists, or Windows `.def` files from the same export set.

### L9. Foreign imports

Bind declared host/Oracle contracts to external symbols/intrinsics.

### L10. Native linker-script generation where needed

Only after component/export semantics are stable, add full memory-layout/linker-script generation for freestanding/embedded profiles.

---

## 25. CLI proposal

Keep single-entry `a-to-c` as a compatibility prototype if desired, but introduce a general driver.

Example:

```sh
ap-compile --artifact program.a --link program.aplink --product libsort
```

Direct command-line shorthand can exist for simple cases:

```sh
ap-compile program.a \
  --target c \
  --kind staticlib \
  --export sort=ap_sort:c-flat-v1 \
  -o libsort.a
```

Executable shorthand:

```sh
ap-compile program.a \
  --target c \
  --kind executable \
  --entry main \
  -o app
```

Verification policy should be orthogonal to the product kind. Suggested flags:

```sh
# Default development behavior: compile a complete runtime graph even if
# proof-only obligations are still pending; record them in the receipt.
ap-compile program.a --target c --kind object -o program.o

# Certification/release behavior: require all relevant verification work.
ap-compile program.a --target c --kind object --require-verified -o program.o

# Explicitly permit runtime computational holes to become traps.
ap-compile program.a --target c --kind object --allow-runtime-holes=trap -o program.o
```

The file form should be preferred once imports, multiple exports, ABI profiles, and platform settings grow.

---

## 26. Naming and symbol stability

Do not export raw source names or pointer-derived ordinals as stable ABI symbols.

Rules:

1. Internal Core-node names (`t1`, `t2`, etc.) are private and unstable.
2. External symbols come only from explicit link-plan export names or a deterministic target mangling scheme.
3. If no external alias is supplied, a backend may derive one from the semantic name under a versioned mangling rule.
4. ABI stability requires both symbol-name stability and ABI-profile stability.
5. Changing the internal DAG, proof structure, or code partition must not change an explicitly declared external symbol.

A versioned mangling scheme can be added later for overloaded/namespaced semantic names; the first version may simply require explicit unique aliases for foreign-facing APIs.

---

## 27. Interaction with optimization and proof-preserving lowering

The link plan must not force an early commitment to a representation.

For example, a later target profile may allow a proved Nat range to lower to `uint32_t`, while another preserves structural Nat. Both can export the same semantic function under different ABI profiles.

Thus:

```text
semantic root
    |
    +--> structural/boxed ABI
    |
    +--> proven flat C ABI
    |
    +--> CUDA scalar/buffer ABI
    |
    +--> RTL bitvector/handshake ABI
```

The representation choice belongs to target lowering and must be justified by its profile/rules. It should not rewrite canonical `.a` semantics.

---

## 28. Failure policy

Link planning should fail closed with respect to **runtime soundness and target realizability**, but not with respect to proof completion by default.

Reject before publishing ordinary target output when:

- a requested export does not exist;
- the selected semantic/runtime graph is structurally rejected or malformed;
- an unresolved hole is runtime-relevant and no explicit trap/check policy handles it;
- an export type is unsupported by the requested ABI;
- a reachable Oracle has no target realization;
- foreign imports remain unresolved for a final executable/product that requires closure;
- two exports map to the same external symbol;
- runtime ABI/profile versions are incompatible;
- a target-specific role is invalid (for example a higher-order effectful closure requested as a flat RTL combinational port);
- the backend would need an erasure not justified by semantic relevance rules;
- the backend would need a representation conversion, bounds-check deletion, unreachable assumption, or other unsafe lowering justified only by a pending/unproved obligation.

Do **not** reject ordinary development compilation merely because erased proof obligations are pending. Instead record that status in the link receipt. Under `--require-verified`, pending or assumed obligations become a policy failure.

As in the current C backend, `unsupported`, `verification-pending`, `runtime-incomplete`, and `rejected/invalid` should remain distinguishable states.

---

## 29. Testing requirements

The current backend’s differential testing style should be retained and extended.

### 29.1 Link-surface tests

- exactly selected symbols are public;
- unselected semantic names do not leak;
- internal Core-node functions remain local/hidden;
- duplicate aliases reject;
- deterministic repeated emission.

### 29.2 Library tests

- C caller links against generated static library and calls two exported names;
- exports share internal dependencies without duplicated semantic identity;
- multiple calls with one runtime context preserve ownership rules;
- destroy/failure paths leak no memory under sanitizer tests.

### 29.3 Executable wrapper tests

- executable wrapper behavior equals calling the corresponding library export through the runner;
- changing product from library to executable does not change the selected root semantics.

### 29.4 ABI tests

- boundary values for Int32/Int64;
- embedded NUL in counted Text;
- unsupported higher-order/dependent exports reject in flat ABI;
- boxed ABI round-trips nominally distinct runtime-relevant values without collapsing identity;
- semantically erased proof arguments do not appear in generated headers or public symbol signatures;
- changing only an erased proof witness does not change generated runtime behavior or ABI;
- computational witnesses that are permitted to drive runtime case analysis are not erased;
- equality/Identity transport cases certified as representation-erasing compile without allocating or passing a proof object.

### 29.5 Native symbol-control tests

- ELF `nm`/`readelf` or platform equivalent sees only intended exports;
- shared-library version/export script matches receipt;
- Windows/macOS equivalents tested where available.

### 29.6 Cross-target contract tests

The same semantic export set should be selectable by multiple targets without mutating `.a`.

---

## 30. What should remain out of `.a`

Do not add the following to canonical semantic artifacts merely to support linking:

- C symbol spellings;
- ELF/Mach-O/PE visibility;
- GNU ld section placement;
- Windows `.def` information;
- CUDA device-link flags;
- C headers;
- RTL port names/layouts;
- Wasm module/import string names;
- target mangling output;
- generated object paths;
- final linker command lines.

`.a` may expose enough semantic structure for downstream lowering, but target packaging remains external.

---

## 31. What may deserve a target-independent interface concept later

There is one possible future refinement: a **semantic interface/world** independent of any target.

For example:

```text
interface Sorting {
    export sort
    export isSorted
    require comparator
}
```

This would be analogous to WIT interfaces/worlds but expressed in A Program semantic terms. Such an interface could then be instantiated by several link plans.

However, this should be deferred until real multi-module/package needs appear. The immediate implementation can use existing named definitions plus an external `.aplink` selection file. Avoid adding source syntax before the backend contract is proven useful.

---

## 32. Recommended decision

Adopt the following architecture.

### Decision A — executable is not the fundamental compilation unit

The fundamental target output abstraction is a **component with selected named exports/imports**. An executable is a component plus one target-specific entry wrapper.

### Decision B — name-level linkage is logical, not physical

A Program names are the public selection granularity. Do not require one file/object per name. Compile the union of selected roots and shared dependencies together by default.

### Decision C — add a downstream link plan, not C data to `.a`

Introduce a versioned `.aplink` (working name) file or equivalent driver structure containing:

- artifact reference;
- product kind;
- selected exports;
- optional entry/top;
- foreign import bindings;
- target;
- ABI/lowering profile;
- runtime policy.

### Decision D — separate semantic interface from target ABI

Each export has:

```text
A Program semantic root
        -> target ABI adapter
        -> external symbol/port/interface
```

No target symbol is itself semantic evidence.

### Decision E — generate native linker/control files

For C-like system targets, generate target-native artifacts from the link plan:

- header;
- object/library;
- ELF version/export script;
- macOS exported-symbol list;
- Windows `.def`;
- full linker script only where memory/layout requirements justify it.

### Decision F — start with boxed ABI, then prove flat ABIs

Use a runtime-handle ABI to establish general reusable linking without prematurely freezing representations. Add native flat ABIs as explicitly restricted/proved profiles.

### Decision G — keep private generated DAG functions private

Stable foreign wrappers are the only exported code symbols. `tN` and similar generated functions are implementation details.

### Decision H — proof completion is not the default code-generation gate

Define compilation admissibility independently from verification completion. By default, allow code generation from a pending `.a` when all pending material is certified runtime-irrelevant and the erased runtime graph is complete. Emit verification status/assumptions in the receipt, and reserve `--require-verified` for builds that require proof completion.

Never let a pending proof silently authorize unsafe target-level optimizations. If a theorem is needed only to remove a runtime check or narrow a representation, keep the conservative check/representation until that theorem is verified (or until an explicitly unsafe/trusted policy is selected).

---

## 33. Concrete next implementation milestone

A minimal but architecturally correct next milestone can be much smaller than a complete FFI:

1. Modify the C emitter so it no longer unconditionally emits `main`.
2. Accept a list of **multiple closed returning named exports**.
3. Collect the union of their reachable DAGs once.
4. Keep all DAG node functions `static`.
5. Generate one externally visible wrapper per selected export.
6. Generate a `.h` file.
7. Support:
   - `--kind source`
   - `--kind object`
   - `--kind staticlib`
   - `--kind executable --entry NAME`
8. Generate an export/link receipt that distinguishes verification status from runtime/codegen completeness.
9. Permit a selected export with pending **erased proof-only** obligations to reach this path without requiring a full Solve completion; keep `--require-verified` as a stricter policy.
10. For executable mode, generate a tiny separate `main` wrapper that calls the same exported-root machinery.
11. Differential-test library calls against interpreter execution and against executable mode.

This milestone deliberately avoids function-argument marshalling at first. It establishes the correct unit structure before extending the public ABI to unapplied Pi values.

The next milestone can then implement `ap-boxed-v1` argument/result wrappers, followed by `c-flat-v1`.

---

## 34. Final assessment

The closest existing analogue to the desired design is not one language or tool in isolation:

- **GNU ld** contributes entry/layout/export-control ideas;
- **LLVM** contributes per-symbol logical linkage independent of physical file layout;
- **Rust/Go/Zig** show that executable/library/object are explicit product modes and that foreign exports are a subset of language-visible names;
- **OCaml/GHC** show why language semantic metadata/runtime contracts must remain separate from C-facing machine objects;
- **WebAssembly/WIT** provides the strongest model for explicit imports/exports plus optional start and interface-driven binding generation;
- **CUDA** demonstrates that target linking can be multi-stage;
- **SystemVerilog/Verilator DPI** demonstrates that a reusable module/top and a generated `main` wrapper are separate concerns.

For A Program, the resulting design should therefore be:

```text
.a semantic artifact
      |
      v
.aplink: select semantic boundary
      |
      +-- exports by A Program name
      +-- imports/bindings
      +-- optional entry/top
      +-- target + ABI profile
      v
joint dependency closure
      v
backend-private generated graph
      |
      +-- stable public wrappers
      +-- generated interface/header
      +-- target linker/control files
      v
object / library / module / executable
```

The most important invariant is:

> **Compilation packaging may change how accepted semantic roots are represented and connected, but it must not become an alternate owner of A Program acceptance, nominal identity, proof relevance, or canonical artifact semantics.**

That invariant is consistent with the current artifact-persistence and C-backend direction and provides a path to C, CUDA C, WebAssembly, and Verilog/SystemVerilog without forcing any one target’s notion of `main`, object file, or linker script into the language itself.

---

### Additional references for proof erasure and incomplete verification

- Lean Reference, Propositions: <https://lean-lang.org/doc/reference/latest/The-Type-System/Propositions/>
- Lean Reference, Axioms (`sorryAx`): <https://lean-lang.org/doc/reference/latest/Axioms/>
- Lean Reference, `sorry`: <https://lean-lang.org/doc/reference/latest/Tactic-Proofs/Tactic-Reference/>
- Lean, *Axioms and Computation*: <https://lean-lang.org/theorem_proving_in_lean4/Axioms-and-Computation/>
- Lean, provisional proofs and runtime safety: <https://lean-lang.org/functional_programming_in_lean/Programming___-Proving___-and-Performance/Summary/>
- Rocq 8.20, Program extraction / realizing axioms: <https://rocq-prover.org/doc/V8.20.0/refman/addendum/extraction.html>
- Agda, Run-time Irrelevance: <https://agda.readthedocs.io/en/latest/language/runtime-irrelevance.html>
- Agda, command-line options (`--allow-unsolved-metas`): <https://agda.readthedocs.io/en/latest/tools/command-line-options.html>
- Agda, Safe Agda: <https://agda.readthedocs.io/en/latest/language/safe-agda.html>
- Idris2, Multiplicities: <https://idris2.readthedocs.io/en/latest/tutorial/multiplicities.html>
- Idris2, Custom backend cookbook: <https://idris2.readthedocs.io/en/latest/backends/backend-cookbook.html>
- F*/Pulse, Erasure and the Ghost Effect: <https://fstar-lang.org/tutorial/book/part4/part4_ghost.html>

## References

Accessed 2026-09-29.

1. A Program repository: <https://github.com/repyt-margorp/a-program>
2. A Program prototype C backend: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/src/prototype/c_backend/README.md>
3. A Program C emitter: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/src/prototype/c_backend/emit.c>
4. A Program artifact persistence plan: <https://raw.githubusercontent.com/repyt-margorp/a-program/main/doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md>
5. GNU Binutils `ld` manual: <https://sourceware.org/binutils/docs/ld.html>
6. LLVM Language Reference, linkage types: <https://llvm.org/docs/LangRef.html#linkage-types>
7. Rust Reference, linkage: <https://doc.rust-lang.org/reference/linkage.html>
8. Zig documentation: <https://ziglang.org/documentation/master/>
9. Go build modes: <https://pkg.go.dev/cmd/go/internal/help>
10. OCaml native compiler: <https://ocaml.org/manual/5.5/native.html>
11. GHC shared libraries: <https://ghc.gitlab.haskell.org/ghc/doc/users_guide/shared_libs.html>
12. WebAssembly Core modules: <https://webassembly.github.io/spec/core/syntax/modules.html>
13. WebAssembly Component Model WIT: <https://component-model.bytecodealliance.org/design/wit.html>
14. WebAssembly Component Model WIT design: <https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md>
15. NVIDIA CUDA Compiler Driver, separate compilation: <https://docs.nvidia.com/cuda/cuda-compiler-driver-nvcc/#using-separate-compilation-in-cuda>
16. CUDA Programming Guide, separate compilation: <https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/nvcc.html>
17. Verilator overview: <https://verilator.org/guide/latest/overview.html>
18. Verilator connecting/DPI: <https://verilator.org/guide/latest/connecting.html>
19. Verilator compilation modes: <https://verilator.org/guide/latest/verilating.html>
