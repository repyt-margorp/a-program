# Problem List

1. Preserve the supplied Distributed Solve / Local Proof Delivery document as
   a record of the author's long-term design motivation, not an immediate
   implementation request or a new current-bug report.

## P1. Documentation-only design-intent submission

### Subjective (User)

2026-10-03, English paraphrase of the author's clarification: this Markdown
was written as an expression of their design intent. Submit a PR only; do not
create an Issue or turn its contents into an implementation work order.

### Objective (Code)

Submission base: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
The supplied document is dated 2026-09-30 and reports observations of
`db5d694`; these are historical observations, not fresh verification of
the submission base. Original SHA-256:
`fc9b11caa4a9eef513e2f823a8c794e1c517ec8f9f5dea99f8d35a20acc9bbfe`.
Its original text is preserved verbatim below. No compiler tests or independent
literature revalidation are claimed by this documentation submission.

### Assessment

This cover note records submission intent only. The supplied recommendations,
sketches and staged tests are discussion material, not newly approved
architecture, implementation commitments, current defect verdicts or changes
to proof admission. Its floating `main`/`HEAD` links are preserved as supplied,
not evidence that its snapshot descriptions apply to today's implementation.
Existing ownership, persistence and backend plans retain their own scope.

### Plan

- Preserve the supplied text and identify its historical snapshot.
- Limit the PR to this Markdown file under `doc/`.
- Create no Issue; introduce no implementation or test changes.

---

## Supplied document (verbatim)

# A Program: Distributed Solve / Semantic Persistence / Local Proof Delivery Audit

**Date:** 2026-09-30  
**Status:** design audit; future-scale requirements recorded, **not** an implementation order to build a distributed system now  
**Repository:** `repyt-margorp/a-program`  
**Observed repository HEAD:** `db5d694` — *Borrow canonical Effect inference without a Job wrapper*  
**Current accepted implementation:** pointer core under `src/`; several persistence / solver-owner / C-backend changes remain prototypes under `src/prototype/`  

---

## 0. Executive conclusion

This audit records a long-term requirement that is easy to misstate.

A Program is **not** being developed merely so that one workstation can checkpoint a proof assistant session. The intended limiting case is much larger: an arbitrarily large computation network continuously performs proof, synthesis, reduction, checking, and related computation; the global system may contain vastly more formal knowledge and unfinished work than any physical node can hold; each finite node should receive only the theorem/proof/semantic material required for its local task.

That long-term requirement does **not** imply that the current codebase should immediately acquire a distributed scheduler, a global object store, a consensus protocol, a Merkle proof database, or a second universal graph. In fact, the current refactor points in the opposite direction: before persistence is expanded, duplicate `Job`/`Evidence` ownership is being removed so that each semantic or computational operation has one authoritative owner.

The most important architectural consequence is therefore:

> **Preserve the distinction between durable semantic knowledge, durable open obligations, owner-local resumable progress, disposable scheduling/index state, and downstream deployment policy. Do not serialize today’s worker layout merely because tomorrow’s system must resume distributed work.**

The future distributed problem should constrain today’s **boundaries**, not force today’s **scale**.

A second conclusion is equally important. Earlier discussion sometimes reduced persistence to “store certified knowledge and throw search state away.” That is insufficient for A Program’s stated goal. If years of computation are invested in an unfinished obligation, losing the useful frontier and restarting the same search is a real failure even when no new theorem was completed. A Program therefore eventually needs to preserve enough **abstract continuation/progress state** to avoid unjustified recomputation. But that continuation must belong to the operation that owns the unfinished work; it must not be confused with every cache, ready-queue pointer, hash bucket, or solver implementation detail.

A third conclusion concerns locality. The eventual problem is not just “distributed proving.” It is:

> **global formal knowledge + global unfinished computation → finite, task-specific local semantic/proof bundle**

The extraction of that bundle is a distinct problem from proving a theorem. Logical dependency, local revalidation dependency, executable/lowering dependency, and unfinished-solve continuation dependency are related but not identical closures. A future system that conflates them will either ship far too much data or silently weaken its trust model.

---

## 1. User intent — recorded without novelty inflation

The following points are requirements expressed in the 2026-09-30 discussion. They should be treated as design intent, not as claims that the current implementation already realizes them.

### 1.1 Motivation is persistence, not novelty

The project was not started to claim discovery of a new type theory, theorem-proving primitive, or effect system. The user explicitly stated:

> 「そもそも新しいことを発見しにきたんではなく、既存のあらゆる言葉では永続性が低いからこれを作り始めた」

and later:

> 「定理証明支援系で実現されていたり普通のエフェクト付き言語で作られているものを一緒くたにここにいれているだけ」

Accordingly, “novel”, “interesting”, or “unprecedented” is **not** an acceptance criterion for the architecture. If an existing theorem prover, proof-certificate system, effect calculus, incremental engine, or compiler already solved a subproblem, that is useful prior art rather than a threat to the project.

The relevant question is whether the pieces can be integrated under a sufficiently durable semantic and persistence model without duplicating authorities or binding `.a` to temporary implementation policy.

### 1.2 A Program itself is intended to be long-lived

A Program is not conceived as a disposable interchange experiment. The earlier formulation “A Program should remain readable even if A Program dies” is too strong as a project goal. A more accurate requirement is:

- A Program itself should remain a maintained, long-lived substrate.
- Its semantics should not become hostage to one backend, one solver version, one machine architecture, or one transient internal representation.
- Reimplementation or independent checking is still valuable resilience, but not because project death is assumed.

### 1.3 Workstation checkpointing is only the small case

The user explicitly reframed the goal:

> 「問題は無限にでかい計算機ネットワーク内で定理証明されるような莫大なシステムを計算で統制すること」

and then:

> 「無限にでかいシステムを統制しにいくけど、結局はそのノードサイズ物理限界で必要な定理だけを現地に提供できるようなシステムにする」

For engineering purposes, “無限にでかい” should be read as **arbitrarily large relative to any individual node**, not as a requirement to model a literal mathematical infinity in the implementation.

This produces two simultaneous requirements:

1. Global proof/synthesis work must be decomposable and resumable beyond one process or one machine.
2. Local execution/checking must remain bounded by the finite memory, storage, bandwidth, compute, and trust budget of an individual physical node.

### 1.4 The current code is not ordered to implement that distributed system now

The user explicitly clarified that the present codebase is not to be immediately transformed into the giant architecture. This audit therefore records **constraints and future pressure points**, not a request to insert a distributed runtime into `src/` now.

The correct near-term interpretation is:

> simplify ownership now; preserve future separability; prove local persistence properties now; postpone global orchestration until the semantic owners and artifact boundary are sufficiently stable.

---

## 2. Audit snapshot and scope

### 2.1 Repository state inspected

On 2026-09-30 the GitHub repository page reported 1,297 commits. GitHub `/commit/HEAD` resolved to:

- short commit: `db5d694`
- message: `Borrow canonical Effect inference without a Job wrapper`
- change set: 19 files, `+1058/-142`
- the change is primarily the current `src/prototype/solver_inputs` work plus updates to the solver/evidence audit; it does **not** mean that the accepted top-level `src/` has already absorbed that prototype.

This distinction matters. The latest repository contains both:

- the accepted pointer core under `src/`, and
- newer candidate/refactor work under `src/prototype/` and `doc/`.

A “latest codebase” audit must therefore not report prototype behavior as already-promoted compiler behavior.

### 2.2 Files/documents examined for this audit

The main evidence used here is:

- `README.md`
- `src/program.h`, `src/program.c`
- `src/synthesis.h`, `src/synthesis.c`, `src/synthesis_work.c`
- `src/main.c`
- `src/source_io.h`, `src/source_io.c`
- `src/retained_io.h`, `src/retained_io.c`
- `src/Makefile`
- `doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md`
- `doc/2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md`
- `src/prototype/artifact_persistence/README.md`
- `src/prototype/artifact_persistence/artifact/schedule.h`
- `src/prototype/c_backend/README.md`

The local container could not resolve `github.com` for `git clone`, so current files and HEAD metadata were inspected through GitHub/raw GitHub rather than a local checkout/build. Consequently, this document does not claim a fresh local test run. It does compare the current public code and the test results already recorded by the repository’s active audit documents.

---

## 3. What the current implementation already gets right

### 3.1 `pg_program` explicitly avoids becoming a second state machine

`src/program.h` states:

> `One owner, not another compilation state machine.`

and says synthesis `root/status/evidence remain authoritative`.

This is highly aligned with the long-term requirement. A distributed future will fail if every convenience layer introduces its own copy of “current state”, “accepted result”, and “pending dependency”. The current comment is not cosmetic; it should remain an architectural invariant.

`pg_program` currently aggregates graph, typing, evaluation, synthesis, imported effect inference, source parser/scope/export state, and roots. It is an ownership/composition boundary, not supposed to become another solver authority.

**Audit judgment:** preserve this direction.

### 3.2 Solve is already explicitly budgeted and resumable in-memory

Current `src/synthesis.h` defines:

```c
enum pg_synthesis_status {
    PG_SYNTHESIS_PENDING,
    PG_SYNTHESIS_DONE,
    PG_SYNTHESIS_REJECTED,
    PG_SYNTHESIS_UNSUPPORTED,
    PG_SYNTHESIS_ERROR
};
```

and exposes:

```c
void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget);
```

The scheduler comment states that ready jobs are processed FIFO and each ready job gets one transition before returning to the tail. The budget is explicitly **not** wall-clock time.

The CLI/REPL already exposes:

```text
:solve [N]
:status
:save FILE.a
```

and the batch CLI uses `--steps N`.

This means A Program already treats proof/type/synthesis work as something that can remain `PENDING` after a bounded amount of computation instead of interpreting resource exhaustion as logical failure.

That property is central to the long-term vision and should not be lost during simplification.

### 3.3 Pending is not proof and unsupported is not refutation

Current comments repeatedly distinguish:

- structural information available before acceptance,
- accepted evidence,
- pending producers,
- unsupported operations,
- negative logical results.

For example, `pg_synthesis_return` explicitly says unsupported heads are not negative proofs. Many structural query APIs say that provisional structure is not an accepted type certificate.

This distinction is essential in a global compute network. Otherwise a temporary resource or implementation limit would silently become part of the logic.

### 3.4 The persistence layer already refuses to equate a stored structure with accepted proof

Current `src/source_io.h` says that stored contexts/binders/annotations are reconstructible inputs, **not accepted typing evidence**. It also says:

- no search state or acceptance flag is retained;
- source roots are recomputed;
- the current format is not a complete checkpoint codec.

`src/retained_io.h` similarly says raw terms carry no typing or reduction evidence and reading performs no Solve/evidence admission.

This is the correct trust boundary for the present implementation.

### 3.5 Save publication is atomic at the file boundary

`src/main.c:save_image` writes to a temporary file in the destination directory and renames only after a successful complete write. A failed save removes the temporary file.

For a future long-running compute system, “partial checkpoint accidentally becomes authoritative artifact” is unacceptable. The current atomic-publication habit should be preserved when persistence becomes more sophisticated.

### 3.6 Effects are correctly separated from saved proof/computation images

The README and CLI explicitly state that a saved image is **not an effect receipt**; rerunning a saved executable computation can repeat output. Host effects execute only through `--run`, and pure normalization does not execute terminal output.

This is important for the eventual global system. A theorem/proof checkpoint, an evaluation checkpoint, and an external-world transaction receipt are different things. Conflating them would make replay and migration unsafe.

### 3.7 The C backend remains downstream of `.a`

The C backend prototype states:

- it reads checked artifact views;
- it does not call parser, Solve, conversion, or normalization during emission;
- generated executables do not link the A Program parser/kernel/solver/artifact reader;
- LinkerScript and C ABI metadata remain downstream and are not written into canonical `.a`.

This boundary should remain. The eventual distributed proof network must not become “whatever the current C lowering needs.”

---

## 4. What the current implementation deliberately does **not** yet solve

### 4.1 Current accepted save/load is reconstruction, not exact continuation

The CLI help is unusually clear:

```text
--save defaults to RECOMPUTE inputs, including pending/rejected inputs.
--retain-reductions ...
Retained records are not trusted on load; ordinary source Solve still recomputes.
This is not full CHECKPOINT.
```

The README also says exact saved-Solve resumption remains unsupported/unfinished.

Therefore the existence of `:save` + `:solve` must not be described as “A Program already checkpoints computation effort.” It currently preserves enough input/structure for reconstruction and selected retained information, but not the complete live frontier required for exact split-budget equivalence.

### 4.2 The artifact prototype confirms that recipe-only persistence loses progress

The active artifact plan inventories unfinished owners such as:

- source registration/index cursors,
- substitution maps and next images,
- derivation premise positions,
- normalization/conversion owners,
- effect equation progress,
- Match/IADT/Identity field/index/endpoint cursors,
- typed-query progress,
- ready/wait relationships.

The prototype explicitly records that a continuation codec must preserve the **owner’s current continuation**, not merely the total step counter.

This is directly relevant to the user’s long-term goal: if a billion node-hours have advanced partial owners, saving only propositions plus a number saying “we spent a billion node-hours” is useless.

### 4.3 The current strict split-fuel gate still fails

The persistence prototype documents failures such as `100+100` after reload not matching a fresh `200`-step run and larger partitions where the split run remains pending while the unsplit run finishes.

This is exactly the kind of failure A Program must eventually eliminate if “compute effort is accumulative across restarts/nodes” is an invariant.

The key point is not byte identity by itself. The required equality is a bundle of properties:

- equivalent semantic state,
- equivalent unfinished frontier,
- equivalent cumulative charged progress under the chosen logical fuel model,
- no silent imported acceptance,
- deterministic enough scheduling/ownership reconstruction that partitioning the same budget does not discard useful work.

### 4.4 The schedule prototype is intentionally insufficient on its own

`artifact/schedule.h` explicitly says its scheduler record does **not** encode:

- job identities,
- private continuation state,
- evidence.

Those must already be restored by the enclosing owner/provenance policy before the schedule can be attached.

This is an excellent design constraint. It prevents an attractive but wrong implementation:

> serialize the ready queue and call it a checkpoint.

The queue is only an ordering/reference layer. The semantic operation and its cursor remain elsewhere.

### 4.5 The direct-input refactor is still incomplete

The current HEAD `db5d694` updates the prototype so Effect inference itself is the sole Effect-solving progress owner; the separate inference Job wrapper is removed in the prototype and consumers subscribe directly.

However the repository audit explicitly keeps SE1–SE5 open. Remaining work includes:

- other adapters,
- `EVIDENCE_JOB`,
- duplicate query scheduling,
- single construction path work,
- premise overlap,
- persistence projection.

**Audit judgment:** do not freeze the current Job graph into a distributed wire protocol while this refactor is explicitly questioning whether that graph should exist.

---

## 5. The long-term system should be stated as a locality problem, not merely a cluster problem

A naive description would be:

> “Run theorem proving across many computers.”

That is incomplete.

The stronger user requirement is:

> **The global knowledge/solve space may be arbitrarily larger than any physical node. A node should receive only what is required to justify and perform its local task.**

A useful abstract picture is:

```text
               Global A Program knowledge / open obligations
               =============================================
                 accepted facts / proofs / typed structure
                 unresolved constraints / goals
                 resumable owner-local continuations
                 semantic dependency relations
                 reusable computations
                          |
                          | task-specific projection
                          v
            +--------------------------------------+
            | finite physical node                 |
            |--------------------------------------|
            | requested computation/function       |
            | required definitions                 |
            | required theorems/contracts          |
            | required proof/certificate material  |
            | required local assumptions           |
            | optional local unresolved frontier   |
            +--------------------------------------+
```

The global store is not useful if every node must load it before doing anything. **Locality is therefore part of correctness-at-scale, not merely a performance optimization.**

---

## 6. A persistence taxonomy that fits both today and the future

The current code would benefit from naming several different kinds of retained state explicitly. This is mostly a documentation/specification proposal; it is not a request for five new C object hierarchies.

### 6.1 K — accepted semantic knowledge

Examples:

- checked theorem/proof terms,
- accepted typed occurrences,
- checked definitions,
- nominal declarations,
- equalities/certificates that A Program accepts under its kernel rules.

Properties:

- durable;
- may be shared broadly;
- should not regress merely because a worker crashes;
- must retain assumptions/provenance necessary to interpret the judgement;
- is not equivalent to an arbitrary `DONE` byte.

### 6.2 O — open obligations

Examples:

- an unresolved goal,
- a pending type/synthesis obligation,
- an unresolved effect equation,
- a remaining conversion/equality obligation,
- a theorem whose proof is not yet completed.

Properties:

- durable if it is part of the intended global work;
- not a failure;
- should have stable identity sufficient for multiple compute nodes to refer to the same obligation without merging unrelated nominal/scoped problems.

### 6.3 F — useful resumable frontier

Examples:

- next unprocessed derivation premise,
- partially built substitution map,
- current Match/IADT field cursor,
- a normalization machine state whose discarded prefix would require large recomputation,
- effect-inference queue/cursor owned by that inference operation.

Properties:

- operational rather than a theorem;
- still worth persisting because it represents invested compute effort;
- should be owned by the semantic/solver operation it advances;
- must not be represented by duplicating all of that owner’s inputs/results into a second generic Task graph.

This category corrects an overly simplistic “only certified knowledge matters” position. For A Program’s stated scale target, unfinished but expensive progress can matter enormously.

### 6.4 Q — disposable scheduling/index/cache state

Examples:

- hash buckets,
- pointer-order indexes,
- free lists,
- derived ready indexes that can be reconstructed,
- memoization with no unique consumed progress,
- historical wait edges that no live obligation needs.

Properties:

- normally reconstructible/discardable;
- may be checkpointed as an optimization only when explicitly justified;
- must not become semantic identity or acceptance authority.

The recent Effect-owner prototype is a good example: notification edges are disposable and reusable; the equation owner retains the actual progress.

### 6.5 D — deployment/local-delivery policy

Examples:

- which theorem bundle a physical node receives,
- trust/revalidation depth,
- target ABI/layout,
- C/CUDA/RTL lowering decisions,
- machine-specific optimization,
- network placement and replication policy.

Properties:

- downstream policy;
- must not redefine theorem meaning;
- should generally not be embedded into canonical semantic `.a` unless a future artifact type is explicitly defined for deployment manifests.

This is where the “necessary theorem only at the local node” problem eventually belongs.

---

## 7. Four different closures must not be collapsed into one

When a node asks for one local computation, “send its dependencies” sounds simple. It is not. There are at least four closures.

### 7.1 Logical dependency closure

What facts/definitions are used by the theorem or typed object itself?

This answers:

> What does the proposition/result *mean* and what was it proved from?

### 7.2 Verification closure

What must this particular node possess in order to **recheck** the result to its chosen trust depth?

A node that trusts an authenticated upstream kernel result may need less than a node that insists on locally reconstructing the entire proof.

This answers:

> What evidence must be local before I accept this result under my trust policy?

### 7.3 Execution/lowering closure

What semantic/runtime material is required to execute or compile the selected program fragment on the target?

Proof terms that are required for type checking may be irrelevant after a valid erasure/lowering boundary; conversely a runtime handler or Oracle operation may matter even when no theorem checker needs its implementation.

This answers:

> What must physically ship for this target to run the selected computation?

### 7.4 Continuation closure

If the physical node is going to continue an unfinished Solve operation, what owner-local frontier and prerequisite checked/open inputs must accompany it?

This answers:

> What must move so the node continues useful work rather than restarting it?

These four sets can overlap but are not identical. A future distributed A Program should treat their relationship as an explicit planning problem rather than trying to invent one universal “dependency DAG” whose edges secretly mean all four things.

---

## 8. The local theorem delivery problem

The future local-node request can be described abstractly as:

```text
Request:
    semantic target(s)
    local operation to perform
    accepted assumptions / trust policy
    available checker/runtime capabilities
    resource limits

Result:
    minimal-enough self-consistent bundle
    + explicit external assumptions
    + validation/deployment metadata
```

A conceptual function might be written:

```text
bundle = project(global_state,
                 requested_roots,
                 verification_policy,
                 execution_policy,
                 resource_budget)
```

This is **not** a proposed C API. Its purpose is to make the design problem explicit.

The bundle should not pretend to be globally self-contained if it relies on an upstream trusted theorem service. Conversely, requiring every tiny node to receive the transitive proof term of all mathematics would defeat the locality goal.

### 8.1 Proof depth should eventually be a policy, not hidden behavior

At least conceptually, a node might operate under different validation contracts:

1. **Statement/result trust:** accept a result from a declared trusted authority.
2. **Certificate validation:** receive a compact certificate/proof object and run a local checker.
3. **Dependency revalidation:** recursively obtain/check dependencies to a deeper closure.

This should not be prematurely hard-coded as exactly three levels. The point is that **proof distribution and trust depth are separate from theorem semantics**.

The Foundational Proof Certificate literature is directly relevant here: certificate formats can trade certificate size against the amount of reconstruction/search performed by the checker. That is a useful precedent for A Program’s eventual “small node vs large global proof” problem, without implying that A Program should adopt FPC as its internal proof format.

---

## 9. Critical problem 1: cross-node identity is eventually unavoidable

The current pointer core deliberately uses exact object identity, scoped binders, nominal declarations, and image-local relocation. The active persistence plan explicitly rejects casually merging nominal families/binders by alpha or WHNF equality.

That is correct today.

However, a distributed system eventually needs one node to say:

> “I mean *this* theorem/obligation/definition,”

and another node to resolve that reference without sharing a process pointer.

### 9.1 Do **not** solve this by naive content hashing now

A tempting future answer is “hash every term.” That is dangerous because:

- generative/nominal identities may intentionally distinguish structurally equal objects;
- scoped binders and local allocation identity matter;
- the same erased Core can have different typed uses;
- current audits explicitly reject merging by erased Core equality.

A hash can authenticate bytes once the canonical identity representation is known. It does **not** by itself define the semantic identity relation.

### 9.2 A safe future direction is to separate naming from semantic equality

Possible future schemes include artifact identity + internal stable ordinal, immutable declaration identities, or canonical semantic IDs that explicitly encode generativity/scope. This audit does not select one.

The near-term rule is only:

> Do not make raw machine pointers the future public identity, but also do not prematurely replace them with an equality/hash scheme that erases nominal distinctions.

---

## 10. Critical problem 2: accumulated compute effort needs two meanings

The current `steps` counter is a logical scheduler dispatch count. The source comments explicitly say it is not wall time or individual rule cost.

For one deterministic implementation, this is useful for reproducibility and split-fuel tests.

For a heterogeneous global network, however, “compute effort” has at least two meanings:

### 10.1 Logical Solve effort

A deterministic/protocol-level budget such as A Program transitions. This is useful for:

- reproducible tests,
- proving that save/load did not lose frontier progress,
- bounding a local solver invocation independently of CPU speed.

### 10.2 Physical resource effort

Examples:

- CPU/GPU time,
- energy,
- memory residency,
- network transfer,
- storage,
- specialized accelerator capacity.

This is needed for global scheduling/economics/resource governance but should not become theorem truth.

**Recommendation:** keep these separate. Do not try to redefine `pg_synthesis_advance(..., steps)` as a universal physical-cost metric. Future orchestration can account for real resources while the core retains a logical progress protocol.

---

## 11. Critical problem 3: exact resumption and semantic monotonicity are different

Two desirable properties must not be confused.

### 11.1 Semantic monotonicity

Completed valid knowledge should remain available:

```text
K0 ⊆ K1 ⊆ K2 ...
```

subject to immutable assumptions/identities.

### 11.2 Operational non-regression

If unfinished work has advanced from frontier `F0` to `F1`, a save/migrate/load operation should not silently return to `F0` unless that recomputation is explicitly accepted by policy.

The current A Program artifact work is precisely discovering that semantic roots alone are insufficient for this second property.

The global system needs both:

- a durable knowledge graph, and
- resumable unfinished owner state.

But they should remain distinguishable, because only the first is a logical result.

---

## 12. Critical problem 4: distributed merge cannot mean “merge arbitrary worker memory”

In the future, multiple nodes may work independently on the same or related obligations.

Easy cases:

- node A completes theorem `T1`;
- node B completes independent theorem `T2`;
- both accepted results can be added to global knowledge if their identities/assumptions are compatible.

Hard cases:

- two nodes advance different branches of one search owner;
- one node refines a constraint representation the other still references;
- heuristics/restarts create non-comparable internal states;
- one partial frontier subsumes another only under solver-specific reasoning.

Therefore, a future A Program should not assume every `F` state has a simple CRDT-like join.

A safer separation is:

- **accepted semantic results:** merge under explicit semantic identity/admission rules;
- **open obligation decomposition:** merge when the decomposition protocol says the subobligations are jointly sufficient;
- **owner-local continuation:** normally has one owner/version lineage unless that solver explicitly defines a sound merge operation;
- **scheduling hints/caches:** may be dropped or opportunistically combined without semantic significance.

This means “distributed Solve” does not require a universal `merge(all_state)` primitive.

---

## 13. Critical problem 5: global knowledge extraction can become the real scalability bottleneck

Suppose proving is massively parallel but extracting one node’s required bundle requires traversing the entire global graph. Then the system has simply moved the bottleneck.

Eventually A Program will need locality-oriented indexes or summaries that answer questions such as:

- which accepted theorem supplies this requested contract?
- what is the verification closure under trust policy P?
- what is the executable closure for target T?
- which artifact shards contain those nodes?
- which dependencies are already cached at the destination?

But these are **derived indexes**. The current refactor correctly teaches that indexes should not become a second semantic authority.

Near-term implication: preserve deterministic, explicit dependency ownership and avoid hidden global scans in APIs that will later need to project local bundles.

---

## 14. Critical problem 6: invalidation/versioning must be exact

A global proof network cannot safely say “theorem X was proven once” if the meaning of its dependencies later changes.

The safest long-term model is strongly biased toward immutable semantic objects:

- a proved judgement refers to exact definitions/assumptions;
- editing a definition creates a new semantic identity/version rather than mutating the proposition underneath an existing proof;
- human-readable names can be movable aliases;
- local bundles record exact dependencies, not only names.

This is broadly compatible with the current pointer-core instinct that identity is not inferred from pretty names or alpha/WHNF coincidence.

No immediate global version database is recommended. This is simply an invariant to remember when stable cross-node identities are eventually designed.

---

## 15. Critical problem 7: theorem locality must not silently become security/trust locality

A small physical node may not be capable of checking the entire global proof closure. That is fine if the system explicitly says what it trusts.

It is **not** fine to silently transform:

```text
“proof exists globally”
```

into:

```text
“this local node has verified it”
```

The current artifact work is already careful about this distinction: stored completion/trusted-image modes are separate from ordinary kernel acceptance, and the C prototype calls `--trust-image` an explicit unauthenticated user choice.

That discipline should scale forward.

Potential future authentication/signature mechanisms are a separate systems/security problem. A digest inside the same untrusted file does not create trust; the current artifact plan correctly notes this.

---

## 16. Why the current Job/Evidence refactor matters to the giant future

The current SE1–SE5 work can look like a local memory/code-size refactor. It is more important than that.

A distributed architecture needs to know **what the thing being distributed actually is**.

If A Program currently has:

```text
Term
  + typed occurrence
  + Evidence receipt
  + completed Evidence Job
  + forwarding Job
  + query owner
  + persistence rule copy
```

for one logical/computational operation, then choosing one of those layers as the network identity prematurely would preserve accidental duplication for decades.

The current audit’s principle is therefore exactly right:

> **one structural owner for each construction; sparse work state for unfinished operations on it.**

This is a better precursor to distribution than adding a network layer now.

### 16.1 Effect inference is a useful concrete example

The latest prototype removes the separate Effect-inference Job and leaves the equation queue/cursor/results in `pg_effect_inference` as the one progress owner. Consumers subscribe with disposable notification edges.

This gives a useful pattern:

```text
semantic/solver operation owner
    ├── owns progress/cursor/result
    ├── may be referenced by multiple consumers
    └── consumers have disposable notification links
```

rather than:

```text
operation owner
    -> mirrored Job
        -> mirrored status/result
            -> consumer Job
```

A future checkpoint can then ask, “what progress does this owner need to resume?” instead of “which arbitrary wrapper stack happened to exist in this compiler version?”

---

## 17. Current C backend/LinkerScript boundary is compatible with local-node delivery

The prototype C backend already establishes a useful downstream pattern:

- `.a` remains the semantic source;
- C names/layout/ABI are produced later;
- multiple exports can share emitted Core;
- generated products do not carry the theorem prover runtime;
- LinkerScript chooses exports/products without changing artifact semantics.

For the eventual local-node system, this suggests a clean sequence:

```text
global semantic store
    ↓ project requested semantic/proof closure
local semantic bundle
    ↓ target-specific lowering policy
local native artifact
```

not:

```text
global store
    ↓ encode C/Verilog deployment choices into semantic identity
canonical .a
```

The current plan’s insistence that C/Linker data stay out of `.a` should therefore remain.

---

## 18. Related existing work: useful precedents, not novelty claims

### 18.1 Rocq State Transaction Machine / asynchronous proofs

Rocq’s STM represents proof documents as states/tasks and supports asynchronous proof processing. Its documentation explicitly distinguishes document types and whether unfinished proofs are allowed, and its parallel proof processing can decouple statement checking from proof construction/checking.

**Relevance to A Program:**

- proof work can be taskified and processed asynchronously;
- interactive state and proof workers need not be a single synchronous REPL chain;
- unfinished proof/document states are operationally useful.

**Limit:** this is not the same as A Program’s proposed durable, arbitrarily large, cross-node global Solve frontier with local theorem projection.

Sources:

- https://rocq-prover.org/doc/V9.1.0/api/rocq-runtime/Stm/index.html
- https://staging.rocq-prover.org/doc/V9.0.0/refman/addendum/parallel-proof-processing.html

### 18.2 Isabelle/PIDE persistent document model

Isabelle/PIDE treats the prover/editor interaction using multiple document versions and background processing rather than one mutable linear REPL state.

**Relevance:** persistent/versioned document thinking and asynchronous dependency-driven processing are strong prior art for separating user-visible source state from prover processing state.

**Limit:** again, this does not by itself define A Program’s future artifact-level continuation and local theorem distribution model.

Source:

- https://isabelle.in.tum.de/library/Doc/JEdit/JEdit.html

### 18.3 Foundational Proof Certificates / distributed web of formal proofs

Dale Miller and collaborators explicitly study persistent proof evidence that can be communicated between different computational logic systems and checked by independent checkers. The FPC approach also makes the amount of omitted proof detail a deliberate tradeoff: smaller certificates can require more reconstruction by the checker; more explicit certificates can be checked by simpler machinery.

This is highly relevant to the future question:

> how much proof material must a finite local node receive?

It does **not** answer A Program’s separate problem of preserving arbitrary unfinished Solve computation, but it is strong prior art for producer/checker separation and proof-bundle granularity.

Sources:

- https://www.lix.polytechnique.fr/~dale/ProofCert/
- https://www.lix.polytechnique.fr/Labo/Dale.Miller/papers/icdcit-2020.pdf

### 18.4 Proof-carrying code as a local-consumer analogy

Proof-carrying code established the broad pattern that a producer can do expensive reasoning and a consumer can receive code plus formal evidence and validate a declared safety policy locally.

**Relevance:** this is close to the “local node receives what it needs” direction.

**Limit:** A Program’s target is broader: the local node may receive program semantics, theorem evidence, and possibly unfinished proof/synthesis continuation; it is not only a mobile-code safety certificate problem.

---

## 19. Near-term recommendations for the actual current codebase

These are intentionally conservative. They are meant to keep the future open without turning the present compiler into a distributed platform.

### R1. Finish the current ownership simplification before expanding checkpoint format

Continue SE1–SE5. In particular:

- remove no-work completed wrappers where direct checked inputs suffice;
- keep owner-local progress where work is genuinely unfinished;
- eliminate duplicated query scheduling/forwarding where there is already one canonical owner;
- do not add persistence codecs merely to preserve representations already targeted for deletion.

**Reason:** distributed persistence built before ownership simplification would fossilize accidental layering.

### R2. Keep “frontier” as a requirement, not a generic new graph type

Document that full checkpointing must preserve:

- open obligation identity,
- exact owner-local continuation required to avoid recomputation,
- dependency/wakeup relations needed to resume,
- deterministic scheduling constraints where split-fuel equivalence depends on them.

Do **not** introduce a new global `FrontierNode`/`TaskNode` database until the existing owner inventory demonstrates that such a separate semantic object is necessary.

### R3. Retain and strengthen split-fuel tests

Tests such as:

```text
solve(a + b)
```

versus:

```text
solve(a) -> save -> load -> solve(b)
```

are one of the best present-day proxies for the far-future requirement that computation can move between physical nodes without losing useful effort.

Keep separate verdicts for:

- semantic result,
- local acceptance status,
- remaining logical fuel,
- serialized bytes/structure.

Byte equality is useful but not sufficient by itself.

### R4. Do not make `synthesis.steps` physical resource accounting

Preserve it as the current deterministic logical transition budget unless/until its semantics are deliberately revised. Future cluster accounting belongs above this layer.

### R5. Preserve atomic/inert load-save behavior

A zero-fuel load/save must remain unable to:

- prove new theorems,
- execute effects,
- advance synthesis,
- mutate acceptance status accidentally.

The existing tests around inert serialization are directly useful for a future network transport boundary.

### R6. Keep C/CUDA/RTL/deployment outside the semantic persistence owner

The existing C prototype’s downstream-only boundary is correct. The future “which theorem goes to which node” policy should likewise not leak target-native ABI/layout into theorem identity.

### R7. Add the future locality vocabulary to docs now, but not implementation

It is useful to reserve/document the conceptual distinction:

- semantic dependency closure,
- verification closure,
- execution/lowering closure,
- continuation closure.

Doing this now can prevent later APIs from using one vague `dependencies` relation for four incompatible purposes.

### R8. Treat stable cross-node identity as a future explicit design gate

Do not solve it opportunistically inside artifact serialization. Before any distributed artifact protocol is promoted, require an audit answering:

- what is globally referenceable?
- what remains artifact-local?
- how are nominal/generative identities preserved?
- how does a renamed human symbol relate to semantic identity?
- does any hashing canonicalize more than the type theory permits?

### R9. Do not require every local node to possess the full proof closure by default

Record proof/checking depth as a future deployment/trust policy question. Otherwise the “finite physical node” requirement will be defeated before implementation starts.

### R10. Do not call the global system “append-only knowledge” without qualification

Accepted immutable judgements can be monotonic, but:

- open obligations can be decomposed/replaced,
- continuations can advance and obsolete older frontier states,
- deployment views can expire,
- aliases can move,
- rejected speculative search branches can be garbage-collected.

The design should distinguish monotonic **accepted semantic facts** from mutable/advancing **operational state**.

---

## 20. Tests that would eventually make the large-system vision falsifiable

These are staged design tests, not a request to implement all of them now.

### Stage A — present single-process invariants

1. **Split fuel:** `a + b` equals `a/save/load/b` in semantic result and useful remaining progress.
2. **Zero-fuel inertness:** load/save adds no proof, execution, reduction, or Solve transition.
3. **Owner uniqueness:** removing a wrapper does not create a second hidden status/result owner.
4. **Pointer-order independence:** serialized semantic bytes do not depend on allocator/hash iteration order where order is not semantic.
5. **Effect separation:** replaying a proof image does not claim external I/O already occurred.

### Stage B — process migration

6. Destroy the original process, load the checkpoint into a new process, and show equal remaining logical transitions for representative owners.
7. Ensure the new process does not need machine pointers or hidden global singleton state from the old process.
8. Revalidate/provenance costs are explicit and do not masquerade as preserved useful progress.

### Stage C — local bundle extraction

9. Select one closed export/theorem and derive its semantic closure without serializing unrelated library roots.
10. Demonstrate a second node can check/use the bundle without the original global Program.
11. Add an unrelated theorem/library and require unchanged bundle content except for intentionally global metadata.
12. Reject a bundle missing a semantically required assumption even if the target code happens not to execute that path.

### Stage D — distributed open work

13. Move one unfinished owner frontier to another process/node and preserve remaining logical work.
14. Split a decomposable obligation into independent subobligations; merge completed results through the ordinary kernel/admission path.
15. Demonstrate that two non-mergeable solver frontiers are not silently conflated.
16. Crash the worker holding disposable queue/index state; reconstruct it from durable owners without loss of semantic result or required frontier progress.

### Stage E — finite-node proof delivery

17. Run the same local target under two trust depths and demonstrate different proof bundle size/checking work but identical interpreted theorem meaning.
18. Enforce a node resource bound and produce an explicit “cannot construct requested verification closure under this policy” result rather than silently weakening verification.
19. Cache a dependency locally and demonstrate that subsequent bundles need not retransmit it while exact semantic identity remains checked.

---

## 21. Things **not** to implement yet merely because of this audit

This section is important because the future vision is large enough to justify almost any accidental complexity if left unconstrained.

Do **not** add the following to the accepted core solely on the basis of this document:

- a distributed scheduler;
- consensus/Raft/Paxos;
- blockchain or signed proof marketplace machinery;
- a universal content-addressed theorem database;
- a new CRDT layer;
- a second proof IR;
- a generic `Task` graph duplicating synthesis owners;
- C/CUDA/Verilog deployment metadata inside canonical `.a`;
- automatic proof erasure for local delivery;
- a global stable-ID format before nominal/scoped identity requirements are audited;
- physical CPU/energy cost fields in the type theory or kernel judgement;
- a requirement that every local node recursively rechecks every global dependency.

The current job is smaller: **make the single-process semantic/solver ownership clean enough that these decisions can later be made without reverse-engineering duplicated state.**

---

## 22. Proposed long-term invariants

The following are strong enough to guide future work but do not prescribe current C structures.

### I1. One semantic/progress owner per operation

No layer should mirror an operation’s inputs, progress, status, and result merely for scheduling convenience.

### I2. Accepted knowledge is distinct from unfinished progress

A checkpoint may contain both, but only the accepted part is a theorem/result.

### I3. Unfinished useful work may be durable

Do not discard a continuation merely because it is not itself mathematical evidence. At large scale, recomputation can be the dominant failure.

### I4. Scheduling structures are not semantic authority

Ready queues, subscriber lists, indexes, and caches may be reconstructed unless a particular ordering is required for the defined logical-fuel protocol.

### I5. Locality is explicit

A finite node receives a projection of global state. The projection states what it includes and what it assumes externally.

### I6. Proof/checking depth is policy

A theorem’s meaning does not change because a particular node chooses upstream trust, compact certificate checking, or deeper local revalidation.

### I7. Target lowering is downstream

C/CUDA/RTL/linker decisions do not redefine canonical A Program semantics.

### I8. Nominal/scoped identity is preserved

No network deduplication scheme may merge objects merely because erased Core, alpha form, or normalized surface syntax looks equal.

### I9. Resource exhaustion is not logical refutation

Pending/unsupported/resource-bound outcomes remain distinct from a checked negative theorem.

### I10. Global scale must not require global residency

Any architecture that requires every worker to load the whole global theorem/proof database before making local progress violates the stated physical-node goal.

---

## 23. Suggested wording for A Program’s design philosophy

The following is a concise candidate statement for future documentation. It is deliberately not a novelty claim:

> A Program treats proof, typing, computation and unresolved obligations as long-lived formal objects whose useful state may outlive one process or one execution budget. The long-term system is intended to scale to a formal knowledge and Solve space much larger than any physical node. Global work may therefore be accumulated across computation resources, while each node receives only the semantic, proof, and continuation closure required by its local task and trust policy. Accepted mathematical/program facts, unfinished Solve progress, scheduling metadata, and target-specific deployment policy remain distinct. A Program does not require these component ideas to be novel; the goal is a durable integration in which temporary implementation mechanisms do not become the owner of semantics.

A shorter version:

> **Global knowledge and Solve may be arbitrarily large; local machines remain finite. Preserve formal meaning and useful computation globally, project only the required closure locally.**

---

## 24. Relation to the active 2026-09-30 work

This future audit should **not** supersede the current solver/evidence duplication audit. The dependency is the other way around:

```text
SE1–SE5 ownership simplification
        ↓
AP1–AP3 semantic persistence / exact resumption
        ↓
well-defined single-machine/process migration boundary
        ↓
local semantic/proof bundle extraction
        ↓
only then: distributed orchestration at larger scale
```

The C backend can continue as a downstream prototype where already planned, but it should not pull the persistence model toward C-specific representations.

The immediate success criterion remains boring and local:

- fewer duplicate authorities;
- exact owners identified;
- pending work remains resumable;
- saved artifacts are inert and deterministic where specified;
- split fuel stops losing progress;
- checked and unaccepted data remain distinguishable.

If those properties are wrong on one process, adding a network will amplify the error rather than solve it.

---

## 25. Final assessment

The user’s large-system idea is not invalid because the current compiler is small, and it is not validated merely because the idea can be described abstractly.

The current repository provides unusually concrete evidence for both sides:

- **supporting evidence:** bounded `Solve`, explicit pending states, owner-oriented synthesis, inert artifact reading, strict separation of acceptance from stored structure, atomic save, downstream backend boundaries;
- **counterevidence / unfinished work:** saved source images are not exact checkpoints, strict split-fuel resumption still fails, many private owner continuations are not transported, cross-node stable identity is undefined, local theorem/proof closure extraction is not implemented, and the current Job/Evidence graph is still being simplified.

Therefore the correct conclusion is not “A Program is already a distributed proof substrate,” and not “the future idea is too abstract to matter.”

The useful conclusion is:

> **The future scale requirement gives a reason to keep present ownership and persistence boundaries clean. The current refactor is prerequisite work, not a detour. Exact owner-local resumption is the bridge between today’s bounded single-process Solve and tomorrow’s large compute network; local proof/theorem projection is a later, separate bridge between global knowledge and finite physical nodes.**

The most serious future design questions are now identifiable:

1. What is the stable cross-process/cross-node identity of a theorem, nominal declaration, open obligation, and resumable owner?
2. Which owner-local continuation state represents invested computation rather than disposable scheduling history?
3. How is useful progress compared across save/load/migration when the solver is decomposed or heterogeneous?
4. How are logical, verification, execution, and continuation closures represented without creating four duplicated semantic databases?
5. How does a finite node state its trust depth and receive only the corresponding proof/certificate closure?
6. Which parts of global state are monotonic accepted knowledge, which are advancing/replaceable frontier state, and which are garbage-collectable policy/index state?
7. How can all of the above be added **after** the current duplicate Job/Evidence ownership is simplified, rather than preserving that duplication forever?

Those questions are substantial enough for future dedicated audits. They are **not** reasons to interrupt the current simplification and implement the global network now.

---

## 26. Source map

### A Program repository / current implementation

- Repository: https://github.com/repyt-margorp/a-program
- Observed HEAD (`db5d694`): https://github.com/repyt-margorp/a-program/commit/HEAD
- README: https://github.com/repyt-margorp/a-program/blob/main/README.md
- `src/program.h`: https://github.com/repyt-margorp/a-program/blob/main/src/program.h
- `src/synthesis.h`: https://github.com/repyt-margorp/a-program/blob/main/src/synthesis.h
- `src/main.c`: https://github.com/repyt-margorp/a-program/blob/main/src/main.c
- `src/source_io.h`: https://github.com/repyt-margorp/a-program/blob/main/src/source_io.h
- Artifact plan: https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md
- Solver/Evidence audit: https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md
- Artifact persistence prototype: https://github.com/repyt-margorp/a-program/blob/main/src/prototype/artifact_persistence/README.md
- Schedule prototype: https://github.com/repyt-margorp/a-program/blob/main/src/prototype/artifact_persistence/artifact/schedule.h
- C backend prototype: https://github.com/repyt-margorp/a-program/blob/main/src/prototype/c_backend/README.md

### Related systems / research

- Rocq STM API: https://rocq-prover.org/doc/V9.1.0/api/rocq-runtime/Stm/index.html
- Rocq asynchronous/parallel proof processing: https://staging.rocq-prover.org/doc/V9.0.0/refman/addendum/parallel-proof-processing.html
- Isabelle/PIDE document model: https://isabelle.in.tum.de/library/Doc/JEdit/JEdit.html
- ProofCert project: https://www.lix.polytechnique.fr/~dale/ProofCert/
- Dale Miller, *A distributed and trusted web of formal proofs*: https://www.lix.polytechnique.fr/Labo/Dale.Miller/papers/icdcit-2020.pdf

