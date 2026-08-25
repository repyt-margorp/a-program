# Effort Compilation, Parallel Elaboration, and From-Scratch Artifact Checking

Date: 2026-08-25

Status: proposed architecture; not implemented

Re-audit status: corrected against the same latest `origin/main` on
2026-08-25. The re-audit confirmed that the central direction is compatible
with the current architecture, but also identified two necessary corrections:

1. A Program should unify orchestration protocols, not collapse classifier,
   effect, Universe, HOTT, normalization, checking, and runtime semantics into
   one catch-all solver.
2. From-scratch artifact checking cannot discard the current proof graph until
   every semantic fact now available only through Proposition/Claim/Derivation
   storage has an explicit checked-Core source or a deterministic reconstruction
   rule.

Audited implementation baseline:

- branch: `main`
- commit: `4fe5aeea4da0262415cc24f101f45d43bb5b8e4b`
- subject: `Implement PR21 function graph surface`
- active artifact schema: version 86
- review input: `EffortCompile統合.eml`, dated 2026-08-25

## 1. Executive decision

A Program should recover its original progressive-compilation idea, but the
semantic center should be neither a single linear `advance` operation nor
replay of a serialized compiler derivation history.

The target architecture is:

```text
                         immutable input revision R
                                    |
                +-------------------+-------------------+
                |                   |                   |
          elaborate goal g1   elaborate goal g2   elaborate goal g3
             with effort e1      with effort e2      with effort e3
                |                   |                   |
          proposal delta d1   proposal delta d2   paused work capsule
                |                   |
                +--------- merge proposals -----------+
                                    |
                         from-scratch artifact check
                                    |
                     checked environment / diagnostics
```

Elaboration, inference, proof search, Function Graph generation, and other
compiler services may run:

- incrementally;
- in parallel;
- with different effort allocations;
- in any implementation-defined order;
- in separate processes; and
- at a later time than the declaration they analyze.

None of those production histories is semantic authority. A small independent
checker accepts the resulting, fully elaborated artifact content by type
checking it again under the exact calculus and intrinsic environment named by
the artifact.

The central trust statement is therefore:

```text
check_artifact(R, A) = Checked(E)
--------------------------------  checker soundness
well_formed_environment(R, E)
```

This is a metatheorem about the checker. Merely running a checker does not prove
the checker itself sound. A small trusted checker, a formal soundness proof, and
independent checker implementations are separate ways to strengthen this
boundary.

The current Claim/Derivation replay path may remain temporarily as an audit
facility and migration oracle. It must not be the only way to establish
artifact validity in the target architecture.

There are therefore three distinct mechanisms, and the document must not call
them all "replay":

| Mechanism | Input | Question answered | Target role |
| --- | --- | --- | --- |
| elaboration/solver execution | surface and open goals | can a complete explicit candidate be found? | untrusted producer |
| current Derivation replay | stored rule applications and premises | does this selected proof graph obey the current validators? | v86 authority and migration oracle |
| from-scratch artifact checking | fully elaborated checked-Core candidate | does the final content type-check without its producer history? | target final authority |

Current Derivation replay is not unsound or a blind log reader. It is an
explicit proof-producing authority mechanism. The proposed change is motivated
by construction independence and a smaller final artifact contract, not by an
identified soundness failure in accepted replay.

## 2. Corrected terminology

Several independent ideas have previously been called "incremental" or
"effort compilation." They must be separated.

### 2.1 Effort-resumable elaboration

The same goal can pause after consuming a deterministic amount of abstract
work and resume from its continuation.

```text
attempt(R, g, e1) = Paused(k)
resume(R, k, e2)  = Proposal(d)
```

This is not gradual typing. It does not weaken the type system, and effort
exhaustion does not create a runtime contract.

### 2.2 Incremental recompilation after edits

When revision `R` changes to `R'`, unaffected elaboration and checking results
may be reused after dependency validation.

```text
R --edit--> R'
dirty = reverse_dependency_closure(changed_semantic_keys)
```

This requires stable semantic keys, positive and negative dependencies,
provenance, and from-scratch consistency.

### 2.3 Parallel elaboration

Independent or speculative goals may be processed simultaneously. Parallel
workers produce proposals, not accepted facts.

### 2.4 Demand-directed completion

Completeness is relative to a requested root closure, not necessarily to every
record in a physical file.

Examples include:

```text
ImportInterface(module)
CheckDefinition(name)
GenerateFunctionGraph(name)
Evaluate(entry)
FullAudit
```

### 2.5 Semantic residuals

A residual is a calculus-defined obligation intentionally delegated to a later
phase with a specified verifier. It is not work that the compiler merely failed
to finish.

### 2.6 Runtime suspension

A suspended object computation is not a suspended compiler proof search. It has
a runtime continuation, handler state, and potentially committed host effects.
The two may share scheduling infrastructure, but not semantic state or
authority.

## 3. Current implementation audit

The current implementation already contains important parts of the desired
architecture, but it does not yet implement the target model.

### 3.1 Existing progressive core

The current pipeline is documented in `README.md` as:

```text
source text
  -> surface AST and source binders
  -> name/type-declaration indexing
  -> shared TermDB + ContextDB-indexed TypedOccurrenceGraph
  -> classifier/effect constraint generation
  -> bounded fixed-point solving
  -> JudgementDB proof reconstruction and validation
  -> VerificationDB residual obligations
  -> artifact/interface or runtime execution
```

The current front-end inference pipeline contains a classifier worklist
fixed-point engine and several cooperating, separately owned semantic engines.
Collectively, that pipeline covers substantially more than the early
classifier-only implementation:

- classifier synthesis;
- dependent Match motive reconstruction;
- index refinement;
- Context and Substitution actions;
- CBPV computation classifiers;
- effect rows through the effect constraint machinery;
- computation totality propagation plus later termination-evidence
  publication; and
- resource-usage solving and projections used by proof propositions.

These are not all one solver arena or one work queue. In particular, totality
evidence, proof materialization, Universe closure, accepted replay, HOTT action,
and Function Graph generation have separate phases or owners. The classifier
worklist remains the most direct implementation descendant of A Program's
original progressive type-synthesis idea, while the phase separation is part
of the current authority architecture and must not be erased accidentally.

### 3.2 The current solver budget is not a global compile effort

`prototype_compile_metadata` stores one compiler-session classifier-solver
limit and counter:

```text
normalization_step_limit
normalization_steps_used
solver_step_limit
solver_steps_used
solver_exhausted
```

In
`src/prototype/src/frontend/lowering/constraint/classifier_and_computation_propagation.inc`,
the classifier worklist consumes `solver_steps_used` for each popped classifier
constraint. Exhaustion sets `solver_exhausted` and returns status `1`.

The caller in
`src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` treats
any nonzero inference result as compile failure. The driver converts that into
`classifier solver step limit exhausted`.

The following command therefore still fails at the audited baseline:

```text
./read_file.out --check-only \
  --policy hybrid \
  --solver-steps 1 \
  examples/05_bool_to_nat.p
```

Observed result:

```text
typing fixed point: classifier inference failed status=1
compile pending failed: infer pending types
classifier solver step limit exhausted
```

The budget does not cover the whole compiler. Proof materialization, accepted
validation, Function Graph generation, artifact publication, linking, and
runtime execution do not consume one common global credit account.

Normalization is also distinct. `normalization_step_limit` is generally passed
to individual normalization or conversion requests; the aggregate
`normalization_steps_used` field is not a complete account of all normalization
performed by the compiler.

### 3.3 Pause and resume are not implemented

`prototype_ast_compile_pending_with_imports()` allocates a
`compile_judgement_workspace` for one invocation. If compilation returns a
nonzero status, the active TypedOccurrenceGraph transaction is rolled back and
the workspace is freed.

Consequently, the current compiler preserves neither:

- the classifier work queue;
- solver metavariable state;
- incomplete normalization machines;
- proof-materialization work;
- the current phase frontier; nor
- a resumable compile-session identity.

Append-only source compilation and fixed-point restarts inside one invocation
exist. Effort-sliced continuation across invocations does not.

### 3.4 Artifact v86 deliberately excludes compiler work state

`src/prototype/spec/artifact_v86.schema` defines one dense accepted object
graph. It explicitly excludes:

- solver variables and candidates;
- worklists;
- source AST identities;
- HOTT action work;
- normalization continuations and caches;
- runtime environments;
- graph revisions; and
- compiler effort counters.

This is a sound authority boundary for the current artifact. It also means that
renaming `.ao` or `.apo` to `.a` would not by itself produce a resumable
compiler state.

The current driver recognizes `.ao` and `.apo`, not `.a`, as artifact suffixes.

### 3.5 Current validation is Derivation replay

Compilation reconstructs JudgementDB candidates, publishes accepted Claims and
Derivations, seals the TypedOccurrenceGraph, and calls
`prototype_judgement_validate_accepted_graph()`.

Artifact v86 serializes the accepted Proposition, Claim, Derivation, and premise
edge closure. Readback checks those stored rule applications. Several authority
edges, including certified Substitution extension and HOTT/Identity roots,
currently refer to exact accepted Claim IDs.

This is stronger than trusting solver output, but it is not the proposed
from-scratch type-checking boundary. The current validator starts from a
serialized derivation selection. The target checker must be able to discard
that selection and establish typing from elaborated object content and the
kernel rules again.

### 3.6 Current architectural strengths to preserve

The redesign must preserve the following existing boundaries:

1. TermDB stores canonical static terms, not runtime environments.
2. TypedOccurrenceGraph retains source occurrence identity, Context, classifier,
   child roles, and transformation provenance over shared Core terms.
3. ContextDB and SubstitutionDB remain explicit CwF structure.
4. A raw Substitution ID is not automatically certified.
5. Solver equations are not object equality witnesses.
6. User proofs do not extend global DefEq.
7. VerificationDB residuals are not accepted Claims.
8. Pure normalization never dispatches host effects.
9. Artifact calculus and intrinsic-environment fingerprints are checked before
   semantic interpretation.
10. Function Graphs lower to ordinary generated IADTs and certified functions,
    rather than adding a privileged Core eliminator.

In particular, item 6 is essential for parallel proof introduction. If adding a
proof could change conversion for earlier terms through equality reflection or
global rewrite insertion, previously checked declarations could become order
dependent.

### 3.7 Facts currently available only through proof authority

The first document revision understated the amount of schema work required to
remove Derivation replay. At v86, the accepted proof graph is not a detachable
debug trace:

- term exports identify an exact accepted Claim or conditional base evidence;
- every `EXTEND` Substitution stores the exact accepted `HAS_TYPE` Claim for its
  assigned term;
- Identity roots store source-type, identity-family, and optional witness Claim
  IDs;
- Universe obligation provenance refers to Claims and Derivations;
- artifact dependency slicing follows accepted proof premises and semantic
  actions; and
- quantitative resource usage is part of Proposition identity and is not fully
  recoverable from the current serialized TypedOccurrence record alone.

Consequently, `erase_compiler_history(A)` is a target criterion, not an
operation that succeeds on an unchanged v86 file. The new checked-Core schema
must first make each required fact explicit or deterministically
reconstructible. Until then, accepted replay remains mandatory authority.

### 3.8 Compatibility matrix

| Current component | Target treatment | Required change |
| --- | --- | --- |
| TermDB | preserve | define the exact checked subset and canonical read rules |
| TypedOccurrenceGraph | preserve and strengthen | distinguish semantic checking fields from source/debug fields |
| ContextDB | preserve | reconstruct formation instead of accepting an arena record by presence |
| SubstitutionDB | preserve | replace serialized Claim-ID certification with direct checking and minted capabilities |
| classifier/motive solver | preserve as producer | make state resumable and proposal-producing |
| effect solver | preserve as separate producer | expose typed dependencies and resumable state without merging semantics |
| Universe solver | preserve as separate checker/producer | reconstruct obligations from checked content rather than proof provenance alone |
| HOTT action engine | preserve as producer | require ordinary object evidence or an explicit checked-Core rule at publication |
| JudgementDB | preserve during migration | use as v86 authority and checker-agreement oracle; later make derivation traces optional only after coverage |
| VerificationDB | preserve | continue to distinguish admitted conditional contracts from incomplete work |
| Function Graph generator | preserve as producer | check generated IADTs/functions and owner-relative metadata from scratch |
| runtime evaluator | preserve as a separate machine | share scheduling/container protocol only if useful |
| v86 artifact | preserve as current format | introduce a new `.a` schema rather than weakening v86 invariants in place |

## 4. The intended A Program philosophy

The following is the proposed restatement of the original philosophy.

### 4.1 Source is a request to construct meaning

A `.p` file is not itself the trusted semantic object. It is a surface-language
request from which elaborators may construct explicit Core terms, declarations,
Contexts, Substitutions, effects, totality information, and proof terms.

### 4.2 Construction order is operational, not semantic

The system may discover a classifier, proof, motive, function graph, or
normalization result in any order permitted by its dependencies. Two compilers
may use different search strategies and still construct the same valid module.

### 4.3 The final content must stand without its construction history

If a solver log, work queue, candidate order, and stored Derivation selection
are removed, a completed fragment must remain checkable.

This is the key construction-independence requirement:

```text
erase_compiler_history(A) = Acore
check_artifact(R, Acore) = Checked(E)
```

### 4.4 Proofs introduced later are ordinary declarations

Suppose `quickSort` has already been checked. A later fragment may add:

```text
quickSortSorted : (xs : List Nat) -> Sorted (quickSort xs)
quickSortSorted := ...;
```

The worker that constructs the body may use Function Graphs, generated IADTs,
automation, or manual induction. The authority boundary only asks whether the
final body checks against the stated classifier in the environment that
contains `quickSort`.

The previous compilation history of `quickSort` need not be reopened. Its
checked definition or opaque interface, plus the explicitly available Function
Graph interface when required by the proof, is the dependency.

### 4.5 Effort controls discovery, never truth

Effort determines how much work a producer attempts. It must not alter the
meaning of a checked term.

```text
more effort -> possibly more proposals
more effort -/-> weaker or stronger typing rules
```

The checker may itself be interruptible for engineering reasons, but a paused
check creates no authority for the unfinished root.

### 4.6 Unify orchestration, not semantic algorithms

The original mail correctly observed that classifier solving, normalization,
proof materialization, HOTT work, Function Graph generation, and eventually
runtime evaluation all need better scheduling, dependency, and pause/resume
support. It would be a mistake to infer that they must become one homogeneous
solver or one universal goal payload.

The shared layer should contain only operational protocol:

```text
GoalKey
base revision
typed dependency keys
positive and negative dependency observations
effort allocation and consumption
run status
pause/resume capsule
proposal output
diagnostic/provenance trace
```

The semantic engines remain typed and owner-specific:

```text
classifier and Match-motive engine
effect-row engine
Universe constraint engine
normalization/conversion machine
proof-term elaborator
HOTT action producer
Function Graph generator
artifact checker
CBPV runtime evaluator
```

This follows the current HOTT and authority design: typed references and
authority boundaries connect engines, while their algorithms and state spaces
remain separate. A common scheduler may dispatch them, but it must not erase the
difference between a classifier metavariable assignment, a kernel conversion,
an object witness, a runtime obligation, and a returned runtime value.

## 5. Existing theory and what it contributes

No single cited system supplies the complete A Program architecture proposed
here. The design combines several established ideas and adds an A
Program-specific authority model.

### 5.1 Elaborator and small kernel checker

Lean and Rocq translate rich surface syntax, inference, tactics, and recursion
mechanisms to smaller Core languages that trusted kernels type-check. Lean's
kernel performs no unification; elaboration must resolve metavariables and
recursive definitions must be reduced to kernel-recognized primitives. Rocq
describes the same separation as the de Bruijn criterion: tactics construct
proof terms and the kernel checks their types.

References:

- [Lean: Elaboration and Compilation](https://lean-lang.org/doc/reference/latest/Elaboration-and-Compilation/)
- [Rocq: Core language](https://rocq-prover.org/doc/V8.12.1/refman/language/core/index.html)
- [Lean4Lean: Towards a Verified Typechecker for Lean, in Lean](https://arxiv.org/abs/2403.14064)

This supports A Program's proposed final authority boundary. It does not by
itself provide effort-resumable or edit-incremental elaboration.

### 5.2 Incremental computation and stable identity

Nominal Adapton establishes from-scratch consistency for incremental
computation and shows why stable names matter when matching work across runs.

Reference:

- [Incremental Computation with Names](https://arxiv.org/abs/1503.07792)

A Program should adopt the consistency criterion and stable naming principle,
not copy the language wholesale. Arena indexes remain efficient session-local
handles but are unsuitable as cross-revision semantic identities.

### 5.3 Task, scheduler, rebuilder, and store separation

Build Systems à la Carte separates task meaning, scheduling order, rebuild
decisions, and persistent build information. This is directly relevant to
keeping A Program's goal semantics independent of sequential or parallel
schedulers.

Reference:

- [Build Systems à la Carte](https://simon.peytonjones.org/assets/pdfs/build-systems-original.pdf)

### 5.4 Incremental type checking and cyclic fixed points

Pacak, Erdweg, and Szabó derive incremental type checkers by compiling typing
rules into finite Datalog relations. Differential Dataflow and DBSP show how
iterative and recursive computations can be maintained under changes.

References:

- [A Systematic Approach to Deriving Incremental Type Checkers](https://doi.org/10.1145/3428195)
- [Differential Dataflow](https://www.cidrdb.org/cidr2013/Papers/CIDR13_Paper111.pdf)
- [DBSP: Automatic Incremental View Maintenance for Rich Query Languages](https://arxiv.org/abs/2203.16684)

A Program's dependent Contexts, normalization, higher-order motives, and
proof-producing rules do not reduce directly to finite Datalog. The usable
ideas are differential facts, dependency tracking, SCC-local fixed points,
alternative provenance, and retraction.

### 5.5 Bidirectional checking

A from-scratch checker should not repeat general elaboration. Bidirectional
typing supplies the relevant separation:

```text
Synth(Gamma, term) -> classifier
Check(Gamma, term, expected)
```

The artifact can store annotations at the boundaries where synthesis is not
syntax-directed, while the checker validates those annotations by checking and
kernel conversion. This reduces the trusted checker without requiring every
Core form to synthesize a principal classifier.

Reference:

- [Bidirectional Typing](https://research.cs.queensu.ca/home/jana/papers/bidir-survey/)

### 5.6 Parallel proof processing

Isabelle and Rocq demonstrate asynchronous and parallel proof-document
processing. They divide documents into tasks, prioritize demanded tasks, and
separate parallel proof production from a trusted logical boundary.

References:

- [Parallel Proof Checking in Isabelle/Isar](https://www21.in.tum.de/~wenzelm/papers/parallel-isabelle.pdf)
- [Asynchronous Processing of Coq Documents](https://arxiv.org/abs/1506.05605)

These systems support the claim that a sequential REPL history need not be the
fundamental proof-processing model.

### 5.7 Monotonicity and coordination

The CALM result relates coordination-free distributed computation to logical
monotonicity. A Program need not become a distributed database, but the design
lesson is useful:

- adding an independently checked positive theorem is monotonic;
- deciding that no overload, constructor, or proof candidate exists is
  non-monotonic and requires a sealed search space or coordination.

Reference:

- [Keeping CALM: When Distributed Consistency is Easy](https://arxiv.org/abs/1901.01930)

This distinction explains why positive checked declarations can merge freely,
while negative name-resolution and ambiguity decisions need an environment
revision fingerprint.

### 5.8 Confluence and alternative provenance

Parallel and incremental execution makes two previously implicit obligations
explicit:

1. fair processing order must not select a different semantic answer; and
2. retracting one support must not retract a fact that still has another valid
   support.

Constraint Handling Rules research treats confluence modulo an equivalence as
a central correctness criterion. Incremental Datalog materialization research
shows why deletion is difficult when several derivations support one fact.

References:

- [Confluence Modulo Equivalence with Invariants in CHR](https://arxiv.org/abs/1802.03381)
- [Incremental Update of Datalog Materialisation](https://ojs.aaai.org/index.php/AAAI/article/view/9409)

In the target A Program checker, a completed declaration is justified directly
by its checked content, so producer-side alternative derivations are not final
authority. They are still required for correct incremental maintenance of
producer caches and open goals.

### 5.9 Anytime algorithms are only a partial analogy

Anytime algorithms traditionally improve an approximate result as more
computation is allocated. A Program must not approximate typing truth. Its
analogue is an interruptible producer that discovers more checkable fragments,
not a checker that returns a lower-quality proof.

Reference:

- [Operational Rationality through Compilation of Anytime Algorithms](https://www2.eecs.berkeley.edu/Pubs/TechRpts/1993/6276.html)

The preferred term is therefore **effort-resumable elaboration**, not an
"anytime type checker."

## 6. Proposed semantic objects

### 6.1 Environment revision

Every elaboration or checking task runs against an immutable environment
revision:

```text
RevisionKey = hash(
    calculus_fingerprint,
    intrinsic_environment_fingerprint,
    imported_checked_interfaces,
    name_resolution_snapshot,
    source_or_elaborated_declaration_set
)
```

An edit creates another revision. It does not mutate the meaning of the old
revision in place.

### 6.2 Proposal fragment

A producer returns a fragment that is untrusted until checked:

```text
ProposalFragment = {
    base_revision,
    declared_global_names,
    local_term_graph,
    local_type_declarations,
    local_contexts,
    local_substitutions,
    typed_occurrence_annotations,
    explicit classifiers,
    explicit or reconstructible resource usage,
    explicit Match refinements and motives,
    explicit effect and totality classifiers,
    proof terms,
    dependencies,
    conditional obligations,
    optional diagnostics and provenance
}
```

The proposal does not contain an authoritative `accepted = true` bit.

The phrase "explicit or reconstructible" is intentionally strict. A field may
be omitted only when the checker can recover it deterministically from other
checked fields. A compiler cache or a current accepted Claim is not, by itself,
such a reconstruction rule.

### 6.3 Paused work capsule

A paused producer may preserve non-authoritative work:

```text
WorkCapsule = {
    producer_kind,
    base_revision,
    goal_key,
    producer_version,
    work_cost_model_version,
    solver_cells,
    work_queue,
    normalization_machines,
    dependencies_observed_so_far,
    partial_proposals,
    diagnostic_trace
}
```

A capsule can resume only when all compatibility fingerprints match. It is
never import authority.

### 6.4 Checked environment

The checker produces an in-memory checked environment:

```text
CheckedEnvironment = {
    checked_declarations,
    checked_types,
    checked_terms,
    checked_context_capabilities,
    checked_substitution_capabilities,
    checked_effect_and_totality_classifiers,
    checked_export_roots,
    explicit_conditional_contracts
}
```

Capabilities here are minted by the checker after validation. They are not
trusted merely because equivalent numeric IDs appeared in the file.

## 7. The `.a` container

The proposed `.a` format may contain both completed and incomplete work without
confusing their authority.

```text
A_PROGRAM_STATE version fingerprint

configuration
  calculus
  intrinsics
  namespaces
  imports

elaborated_content
  terms
  declarations
  contexts
  substitutions
  typed occurrences
  explicit classifiers
  proof terms
  exports

conditional_contracts
  supported semantic residuals

producer_state
  open goals
  candidates
  work queues
  normalization continuations
  dependency observations

runtime_state
  optional future execution continuations and effect protocol

debug
  source text or source map
  display names
  diagnostics
  optional derivation traces
```

The physical coexistence of these sections does not grant equal authority.

| Section | Authority |
| --- | --- |
| checked again `elaborated_content` | may enter the checked environment |
| well-formed `conditional_contracts` | conditional meaning only |
| `producer_state` | no semantic authority |
| unfinished `runtime_state` | operational state only |
| `debug` | no semantic authority |

The file does not need one global `complete` bit. Completeness is demand
relative:

```text
Complete(A, D)
  iff every semantic dependency reachable from demand D is checked
      or is an explicitly admitted conditional contract for D.
```

The loader should distinguish:

```text
ContainerValid(A)    structural and fingerprint validity
Checked(A, D)        demanded semantic roots type-check
Complete(A, D)       no non-admitted open work reaches those roots
```

## 8. From-scratch artifact checking

### 8.1 What "no replay" means

"No replay" means the checker does not require the serialized sequence of
solver choices or selected Derivation nodes.

The checker will still recursively apply typing rules. That is ordinary type
checking, not replay of compiler history.

### 8.2 Required elaboration boundary

Artifact checking can be small and deterministic only if elaboration has made
the following choices explicit:

- every resolved global and local name;
- every binder identity and Context extension;
- every implicit argument;
- every inferred classifier needed by checking;
- every dependent Match motive;
- every constructor owner and ordinal;
- every recursive or well-founded eliminator application;
- every effect row and totality classifier required at an interface;
- every object proof term;
- every Function Graph association used by a proof; and
- every transparency choice relevant to DefEq.

The checker must not redo open-ended proof search, overload search, or
higher-order motive synthesis. It may synthesize a syntax-directed principal
classifier or check a stored expected classifier and compare them by kernel
conversion.

### 8.3 No circular use of asserted classifiers

An occurrence's serialized classifier is an assertion to check, not evidence
for itself. For each rule, the checker must derive the conclusion from checked
premises, rule-specific structural data, and kernel conversion.

For example, an `APP` occurrence cannot be accepted merely because the
occurrence and its export both repeat classifier `B`. The checker must establish
the function's Pi classifier, check the argument against the domain, instantiate
the codomain through the exact Substitution, and compare that result with `B`.

The same non-circular rule applies to:

- a Context extension and its binder classifier;
- a Substitution extension and its assigned term;
- a Match motive and each branch;
- an effect row and its computation structure;
- a `TOTAL` annotation and its admitted totality construction; and
- an exported Function Graph association and its generated declarations.

### 8.4 TypedOccurrenceGraph is required

TermDB alone is context erased and intentionally many-to-one. It cannot be the
sole checking input. The checker needs the occurrence-indexed information that
distinguishes two typed uses of one shared Core term:

```text
TypedOccurrence = {
    core_term,
    context,
    child_role_edges,
    binding identity,
    explicit classifier,
    Context action when transformed
}
```

This means the current TermDB/TypedOccurrenceGraph separation is compatible
with from-scratch checking and should be retained.

### 8.5 Context and Substitution checking

The checker reconstructs Context formation in parent order. For each
Substitution it validates endpoints and constructor-specific rules:

- identity;
- empty;
- projection;
- extension;
- composition; and
- term reindexing.

For an extension, the assigned term is type-checked in the source Context
against the target telescope classifier after reindexing. The target design
does not need a serialized accepted Claim ID to make the extension valid.

After this check, the checker may mint an in-memory
`checked_substitution_ref` capability for downstream rules.

### 8.6 Definitional equality

DefEq is recomputed by the checker using a pure, deterministic, versioned
normalization relation. Compiler cache hits and previous normalization traces
are not authority.

Only `COMPLETE` conversion may establish equality. `EXHAUSTED` and
`BLOCKED_EFFECT` mean that the checker has not established the conversion.

If the checker is effort bounded, it returns `CHECK_PAUSED`; it must not turn
that status into an accepted residual.

### 8.7 Proof terms

Object-level proofs are checked like ordinary terms:

```text
Gamma |- proof : proposition
```

The method that found the proof is irrelevant. A proof may be added in a later
parallel fragment as long as its exact dependencies belong to the checked base
environment.

This statement is a target-language principle, not a claim that every desired
proposition is expressible in the current surface fragment. At the audited
baseline, ordinary typing terms, `Terminates`, Function Graph IADTs, and the
implemented Identity/HOTT fragment have different degrees of object-language
exposure. Each proof family must cross the checked-Core inventory before its
Derivation authority can be removed.

### 8.8 Strict and conditional checking

A strict demanded root is accepted only when all reachable typing and
conversion checks complete.

A hybrid root may depend on a versioned conditional contract only if:

1. the calculus explicitly defines that contract kind;
2. the backend supplies the required verifier capability;
3. the dependency edge is explicit; and
4. the static checker proves the conditional typing theorem associated with
   that contract form.

Budget exhaustion never satisfies these conditions.

## 9. Parallel production and merge

### 9.1 Producer interface

The semantic producer interface should be closer to:

```text
attempt(
    checked_base_view,
    goal,
    effort_credit,
    optional_work_capsule
) -> Proposal | Paused | Diagnostic
```

There is no requirement that all calls be threaded through one `advance`
state. A sequential `advance` function may exist as one scheduler adapter.

### 9.2 Merge is untrusted

Merging proposals performs structural relocation and conflict detection, but
does not establish typing:

```text
merge_unchecked(base, proposals) -> candidate_artifact
check_artifact(candidate_artifact) -> checked result
```

### 9.3 Local and global identity

Parallel workers must not allocate directly into one shared global arena.

- Local TermDB IDs are relocated and alpha-interned at merge.
- Local Context and Binding identities are namespaced by fragment identity.
- Generative Context identity is not collapsed merely because telescope shapes
  are alpha-equivalent.
- Global declarations use stable qualified names and declaration fingerprints.
- Function-local selectors remain owner-local metadata, as in PR21.

### 9.4 Conflict rules

Two proposals for the same global declaration are:

- duplicates if their checked canonical declaration content is identical;
- alternatives if the system explicitly admits multiple implementations under
  different identities; or
- a conflict otherwise.

Worklist arrival order must never choose one silently.

### 9.5 Dependency scheduling

Checked declarations form a dependency graph. Independent SCCs can be checked
in parallel. A mutually recursive declaration group is one checking unit, not a
cycle accidentally assembled from separately accepted declarations.

Positive proof additions are monotonic within one immutable revision. Negative
decisions require a sealed candidate-space fingerprint. Examples include:

- no overload candidate exists;
- this Match is exhaustive over the complete constructor set;
- this unqualified name is unambiguous; and
- no competing instance exists.

### 9.6 Independent, speculative, and mutually dependent proposals

Three parallel cases must be distinguished:

1. A proposal whose dependencies are all in the checked base can be checked
   independently and merged after fresh-name validation.
2. A speculative proposal may elaborate against another unchecked proposal,
   but neither becomes authority until their combined dependency closure checks.
3. Mutually recursive proposals must declare one explicit SCC and be checked as
   one rule-defined group.

The phrase "proofs can be introduced later and in parallel" primarily refers to
case 1. It does not permit a worker to assume an unchecked theorem produced by
another worker and then publish only the dependent result.

Adding a fresh qualified declaration can change future source name resolution,
but it does not reinterpret already elaborated exact references. Re-elaboration
of source and rechecking of elaborated artifacts are therefore separate
incremental dependencies.

## 10. Universal laws

These laws should be treated as architecture requirements, not optional tests.

### 10.1 Checker soundness

```text
check_artifact(R, A) = Checked(E)
=> well_formed_environment(R, E)
```

### 10.2 Construction independence

If two producer histories generate the same elaborated semantic content, they
must receive the same checking result.

```text
semantic_content(A1) = semantic_content(A2)
=> check_artifact(R, A1) = check_artifact(R, A2)
```

### 10.3 Schedule independence

For conflict-free proposal sets, parallel completion order must not change the
checked semantic environment.

```text
check(merge(S, permutation(P))) ~= check(merge(S, P))
```

Canonical publication should additionally produce identical bytes.

### 10.4 Effort irrelevance for completed content

Different effort partitions may change which proposals are available, but not
the meaning of a proposal that checks.

```text
check(proposal_from_schedule_1) = E
check(proposal_from_schedule_2) = E
```

when both proposals contain the same canonical declarations.

### 10.5 Resume equivalence

For an individual deterministic producer and unchanged revision:

```text
attempt(g, e1 + e2)
  ~= resume(attempt(g, e1), e2)
```

This is a producer-efficiency law, not the definition of semantic validity.

### 10.6 From-scratch incremental consistency

```text
check(incrementally_elaborated(R'))
  = check(cleanly_elaborated(R'))
```

### 10.7 No authority from incomplete work

```text
Paused(g)       -/-> Checked(g)
Unsupported(g)  -/-> Checked(g)
Candidate(g)    -/-> Checked(g)
```

### 10.8 Proof extension does not change prior DefEq

For a checked environment `E` and a new checked proof-only declaration `p`:

```text
DefEq_E(t, u) = DefEq_(E+p)(t, u)
```

unless a future language feature explicitly creates a new revision of the
calculus or transparency environment. Ordinary theorem introduction must not
do so.

### 10.9 Dependency-closed invalidation

For changed keys `Delta`:

```text
Dirty = reverse_reachable(positive_dependencies, Delta)
      union invalidated_negative_dependencies(Delta)
```

Under-invalidation is a soundness defect. Over-invalidation is a performance
defect.

### 10.10 Checked-root closure

Every imported or executed root must have a complete checked dependency closure
or explicitly expose the exact conditional contracts on which it depends.

### 10.11 Checked monotonicity within one compatible revision

For a fixed calculus, intrinsic environment, transparency environment, and
exact name-resolution snapshot, adding a fresh, independently checked fragment
must not invalidate an earlier checked root:

```text
Checked(E, root)
CompatibleFreshExtension(E, fragment)
Check(E + fragment) = E'
---------------------------------------
Checked(E', root)
```

An edit, conflicting declaration, changed import, changed transparency rule, or
changed candidate space creates a new revision and is outside this law.

### 10.12 Alternative-support preservation

Producer-side caches may record several ways to establish one candidate fact.
Deleting one support invalidates the fact only when no remaining support is
valid. This is necessary for correct retraction even though the final checker
does not trust those producer derivations.

### 10.13 Transactionality

A failed, rejected, or paused proposal must not damage the last checked view:

```text
commit(base, proposal) = Paused | Rejected
=> observable_checked_view = base
```

Current append transactions are a useful implementation basis. The target
requires versioned fragment transactions, deterministic relocation, and atomic
publication of a newly checked root closure.

### 10.14 Compile effort and object totality are orthogonal

The following implications are invalid:

```text
producer paused          => object computation may diverge
object computation TOTAL => elaboration finishes within a chosen effort
checker paused           => checked term is partial
```

Compile effort is a meta-level resource policy. Object totality is a classifier
property established by the calculus.

### 10.15 Authority and cache separation

Term interning tables, work queues, normalization continuations, dependency
revisions, green/red marks, stored checker results, and Derivation hints are
caches or control state. They may accelerate production or checking but cannot
replace checked semantic content.

### 10.16 Runtime effect exactly once

If a future `.a` version stores suspended runtime work, an observable host
effect must have a stable event identity and a commit record:

```text
dispatch_count(event_id) <= 1
```

After dispatch, the response must be persisted before the continuation can be
resumed from another process. This law is not needed for pure compile goals and
is one reason runtime state must not be merged into static checking state.

## 11. Claim and Derivation migration

### 11.1 Current roles

Current Claim and Derivation records serve several roles simultaneously:

- accepted typing authority;
- exact rule provenance;
- Context/Substitution certification;
- HOTT/Identity root support;
- Universe-obligation provenance;
- artifact slicing dependencies;
- audit printing; and
- replay input.

They cannot be deleted mechanically.

### 11.2 Target roles

The target separates those roles:

| Current role | Target authority |
| --- | --- |
| term typing | from-scratch occurrence checker |
| proof validity | ordinary proof-term type checking |
| Context formation | deterministic Context checker |
| Substitution certification | checked capability minted after direct checking |
| quantitative resource usage | recomputed occurrence usage or an explicit usage derivation checked from occurrence/Context structure |
| Universe constraints | reconstructed from checked declarations and terms |
| Function Graph validity | generated IADT/functions checked as ordinary declarations |
| dependency slicing | semantic dependency graph reconstructed or checked |
| audit provenance | optional non-authoritative trace |
| solver continuation | non-authoritative WorkCapsule |

### 11.3 Erasure criterion

A subsystem has completed this migration only when the following test passes:

```text
1. compile source to candidate .a
2. remove all optional Claim/Derivation and solver-history sections
3. run the independent artifact checker
4. obtain the same checked exports and conditional contracts
```

Until then, Claim/Derivation remains necessary for that subsystem.

This criterion is intentionally stronger than "the checker can regenerate the
same Claim IDs." Claim IDs are storage identities. The checker must regenerate
the same semantic judgements and checked export meaning; it may allocate a
different ephemeral proof graph or no persistent proof graph at all.

### 11.4 Optional certificates remain useful

Removing replay as the semantic foundation does not prohibit proof
certificates. Optional certificates can accelerate checking or provide better
diagnostics. Their contract must be:

```text
certificate accepted -> same result as ordinary checker
certificate absent   -> ordinary checker still works
certificate invalid  -> reject certificate or fall back; never accept content
```

## 12. A Program-specific rule-family consequences

### 12.1 Ordinary Pi, Lambda, and APP

These are the easiest first target. The artifact stores explicit binder
classifiers, Contexts, child occurrences, and result classifiers. The checker
reconstructs the syntax-directed rules and recomputes conversion.

### 12.2 Dependent Match and motives

The producer may continue to solve motives using the existing fixed-point
engine. The artifact must store the final motive explicitly. The checker then
verifies:

- scrutinee classifier;
- constructor ownership;
- branch telescope Context;
- refinement Substitution;
- branch body classifier under the refined Context; and
- agreement with the explicit motive application.

The checker does not rerun motive search.

### 12.3 Structural and well-founded recursion

The producer elaborates recursion to an explicit eliminator, induction
hypothesis occurrence, `Acc` eliminator, or another admitted totality form. The
checker checks that explicit form. It must not trust a producer's bare
`TOTAL` label.

General partial recursion may type-check under `MAY_DIVERGE` without being
executed by the checker.

### 12.4 Effects and CBPV

The checker validates the static `RETURN`, `REQUEST`, `THUNK`, `FORCE`, and
`FOLD` typing rules and their effect rows. It never dispatches a host request.

Runtime verification obligations remain explicit conditional contracts, not
proofs that static typing succeeded unconditionally.

### 12.5 Function Graphs

Function Graph generation remains producer work. Its generated IADT, result
package, runner, adapters, and selector metadata are proposal content.

The checker validates the generated declarations as ordinary IADT and function
content. Association metadata is then checked as an interface projection over
those declarations. A special trusted `GenerateFunctionGraph` rule is not
required.

### 12.6 HOTT and Identity

This is a major migration boundary. Current identity roots refer to accepted
Claims and checked action structure. To remove Derivation replay as authority,
each admitted HOTT result must be either:

1. an ordinary object term whose classifier can be checked; or
2. an explicit Core construct with a small deterministic kernel checking rule.

Compiler-local relation-action plans and rule labels cannot by themselves be
accepted object evidence.

### 12.7 Universes

The checker reconstructs Universe constraints from checked term and declaration
structure, solves them, and validates declared levels. A serialized closure
certificate may accelerate this process but must not replace reconstruction.

### 12.8 Resources

Future linear, affine, or graded usage must be checked from occurrence and
Context structure. A producer-supplied usage summary is a cache unless the
checker independently validates its derivation.

At v86, usage is already part of Proposition identity even though general
resource-sensitive surface semantics remain future work. A Claim-free checked
Core must therefore add a canonical occurrence-usage representation or define a
deterministic usage reconstruction. Simply deleting the Proposition section
would lose current authority-relevant information.

### 12.9 Exports, imports, and dependency closure

Current term exports point to exact source evidence, and dependency slicing may
discover imports reachable only through accepted proof premises. The target
checker must replace both roles:

- an export is admitted because its exact occurrence checks in the reconstructed
  environment, not because the file names an accepted Claim ID;
- its import dependency closure is recomputed from exact global references,
  Contexts, Substitutions, type schemas, checked rule payloads, and conditional
  contracts; and
- opaque exports expose their checked classifier and declared interface but not
  a body that downstream DefEq may unfold.

An imported `.a` may contain paused producer state, but the importer sees only
the freshly established checked view and explicit conditions.

## 13. Effort and scheduling design

### 13.1 Credits are producer-local allocations

There need not be one global scalar that all workers mutate. A scheduler owns a
total resource allowance and allocates deterministic credits to tasks:

```text
SchedulerBudget
  -> classifier goal credits
  -> normalization goal credits
  -> motive goal credits
  -> proof-search credits
  -> Function Graph generation credits
```

Each task reports consumed credits using a versioned cost model.

### 13.2 Semantic and operational status

Goal semantics and worker execution status remain separate:

```text
GoalResult = Solved | Refuted | Conditional | Unsupported
RunStatus  = Finished | Paused | FailedInternal
```

`Paused` is not a `GoalResult`.

### 13.3 Determinism

Parallel scheduling may be nondeterministic. Checked semantic output must not
be. Canonical naming, conflict reporting, deterministic normalization, and
canonical dense publication remove arrival-order dependence.

### 13.4 Fairness

Fairness affects whether open goals eventually receive effort, not whether an
already checked declaration is true. Demand priorities may intentionally delay
unrequested goals.

## 14. Proposed APIs

Names are illustrative.

```c
enum prototype_attempt_status {
	PROTOTYPE_ATTEMPT_PROPOSAL,
	PROTOTYPE_ATTEMPT_PAUSED,
	PROTOTYPE_ATTEMPT_DIAGNOSTIC
};

int prototype_elaboration_attempt(
	const struct prototype_checked_view* base,
	const struct prototype_goal_key* goal,
	uint64_t effort,
	const struct prototype_work_capsule* resume,
	struct prototype_attempt_result* result
);

int prototype_artifact_merge_unchecked(
	const struct prototype_a_container* base,
	const struct prototype_proposal_fragment* fragments,
	size_t fragment_count,
	struct prototype_a_container* candidate
);

enum prototype_check_status {
	PROTOTYPE_CHECK_COMPLETE,
	PROTOTYPE_CHECK_PAUSED,
	PROTOTYPE_CHECK_REJECTED
};

int prototype_artifact_check_from_scratch(
	const struct prototype_a_container* candidate,
	const struct prototype_check_demand* demand,
	struct prototype_check_session* session,
	struct prototype_checked_view* checked
);
```

The checker session may pause for bounded resource use, but only
`PROTOTYPE_CHECK_COMPLETE` produces a checked view for the demanded roots.

## 15. Implementation sequence

The migration should be staged so current v86 authority remains intact until a
replacement checker covers the same semantics.

### FC0. Freeze the target judgement

- Specify the elaborated Core artifact grammar independent of compiler history.
- State `check_artifact` and its soundness theorem for the implemented fragment.
- Inventory every current Derivation proof kind and classify it as
  syntax-directed, object-proof-driven, reconstructed auxiliary evidence, or
  not yet independently checkable.
- Inventory every non-Derivation consumer of Claim IDs: exports, Substitution
  extension, Identity roots, Universe provenance, artifact slicing, and any
  later schema addition.
- Specify where quantitative resource usage lives after Proposition erasure.
- Define a typed orchestration protocol without replacing owner-specific solver
  payloads with a positional catch-all record.

### FC1. Add a from-scratch source-result checker in memory

- Keep the current compiler unchanged.
- After compilation, discard access to its selected Derivations in a test path.
- Recheck ordinary Pi/Lambda/APP, constructors, and basic Match occurrences from
  TermDB, ContextDB, SubstitutionDB, and TypedOccurrenceGraph.
- Compare reconstructed propositions with current accepted Claims as an oracle.
- Do not claim full erasure until usage, export evidence, and dependency closure
  no longer require the removed graph.

### FC2. Cover CBPV, effects, totality, and dependent Match

- Recheck all static CBPV rules without host dispatch.
- Reconstruct explicit dependent motives and Context actions.
- Mint checked Context/Substitution capabilities.
- Preserve strict separation of residual contracts and incomplete checking.

### FC3. Cover declarations, imports, links, and universes

- Check qualified declaration identity and transparency.
- Reconstruct import dependencies from exact references.
- Rebuild Universe constraints from checked content.
- Require from-scratch and current-v86 validation to agree.

### FC4. Cover Function Graph and HOTT roots

- Check generated Function Graph declarations as ordinary content.
- Replace Claim-ID-only root authority with object terms or deterministic Core
  checking rules.
- Do not remove current replay until the new checker covers all admitted roots.

### FC5. Introduce the `.a` container

- Add elaborated, conditional, producer-state, runtime-state, and debug sections.
- Keep the checked semantic section readable without producer-state sections.
- Make `.p` and `.a` inputs produce the same internal proposal interface.
- Do not treat a stored checked bit as authority; re-establish a checked view.

### FC6. Persist resumable producer state

- Preserve classifier solver cells and work queues.
- Convert normalization to resumable machines.
- Version the effort cost model and producer state.
- Add compatibility rejection for changed calculus, intrinsics, imports, or
  producer implementation.

### FC7. Add parallel proposal production

- Give each worker fragment-local stores.
- Add deterministic relocation and conflict detection.
- Schedule declaration SCCs and independent proof goals in parallel.
- Preserve owner-local Function Graph selectors.

### FC8. Add edit incrementality

- Introduce stable GoalKeys and declaration keys.
- Track positive and negative dependencies.
- Invalidate reverse dependency closures.
- Retain a clean compile/check comparison mode permanently.

### FC9. Demote serialized Claim/Derivation authority

- Make Derivation sections optional audit caches.
- Prove by test that erasing them does not change checked exports.
- Remove current wire dependencies on Claim IDs only after every consumer uses
  checked capabilities or ordinary terms.

## 16. Required tests

### 16.1 Checker agreement

For every existing passing fixture:

```text
current v86 accepted replay result
  == new from-scratch checker result
```

During migration, any disagreement is a blocking audit result.

### 16.2 Derivation erasure

- Compile a candidate artifact.
- Remove optional Derivation and compiler-history data.
- Recheck successfully.
- Confirm identical checked export fingerprints.

### 16.3 Malicious artifacts

Reject at least:

- forged classifiers;
- wrong Context endpoints;
- forged Substitution extension assignments;
- wrong dependent Match motives;
- wrong constructor owner or ordinal;
- false totality labels;
- underreported effect rows;
- invalid Function Graph associations;
- forged HOTT roots;
- inconsistent Universe levels; and
- an unchecked declaration marked as accepted in the file.

### 16.4 Parallel schedule independence

Produce the same fragment set under many worker completion orders and require:

- identical check results;
- identical diagnostics after canonical sorting; and
- identical canonical checked artifact bytes.

### 16.5 Later proof introduction

1. Check a module defining a function.
2. Add a separate later proposal proving a property of that function.
3. Check only the new dependency closure.
4. Perform a clean full check.
5. Require identical checked environments.

### 16.6 Effort pause and resume

- Pause classifier synthesis.
- Pause motive synthesis.
- Pause normalization.
- Resume each under an unchanged revision.
- Compare with an uninterrupted producer.
- Confirm no paused goal is importable as checked authority.

### 16.7 Edit invalidation

Cover append, body replacement, signature replacement, deletion, rename,
reorder, new overload candidate, new constructor, changed transparency, and
changed intrinsic environment.

### 16.8 Independent checker

Maintain at least one checker path that does not share the elaborator's solver
state or candidate-publication code. Sharing TermDB data types is acceptable;
sharing the code that made the disputed semantic decision defeats the audit.

### 16.9 Remaining universal-law tests

- Add a fresh later proof and confirm prior checked roots remain checked.
- Remove one of two producer supports and preserve the candidate through the
  remaining support.
- Pause or reject a fragment and compare the checked base byte-for-byte.
- Exhaust compile effort for a known-total program and confirm no
  `MAY_DIVERGE` conclusion is created.
- Check a `MAY_DIVERGE` program without executing it.
- If runtime suspension enters `.a`, pause immediately before and after host
  dispatch and verify exactly-once event handling.
- Corrupt each cache/control section independently and require either cache
  rejection/recomputation or a check failure, never silent semantic acceptance.

## 17. Open theory and design questions

The following questions are not settled by the cited literature and require A
Program-specific decisions.

### 17.1 What exactly is the minimal checked Core?

TypedOccurrenceGraph currently contains both semantic occurrence data and
source-facing provenance. The checked subset must be specified field by field.

### 17.2 Does the checker synthesize or only check occurrence classifiers?

A strict checking-only kernel is smaller, but some term forms have natural
principal synthesis. The chosen bidirectional mode must be deterministic and
must not reopen surface inference.

### 17.3 How are mutual recursive declaration groups represented?

Parallel fragments cannot independently accept declarations whose validity
depends on a cyclic group unless the group is one explicit checking unit.

### 17.4 Which HOTT evidence is object syntax?

Current action certificates and identity roots mix compiler planning, object
terms, and accepted Claim references. A complete object/kernel boundary is
required before replay can be removed.

### 17.5 What is a cross-revision stable binder identity?

Context identity is generative, while edit incrementality needs stable matching.
Stable matching keys must never force two semantically distinct bindings to
become one Context binding.

### 17.6 How much source state belongs in `.a`?

Resuming elaboration may need source AST and spans, but importing checked content
does not. The container may store both while maintaining separate views.

### 17.7 Is runtime state part of the first `.a` version?

The unified container permits it, but compile elaboration and artifact checking
should be completed first. Runtime host effects require an exactly-once commit
protocol that static checking does not need.

### 17.8 What checker result is cached?

A digest-indexed checked-view cache is useful, but cache corruption must not
become semantic authority. Full from-scratch audit must remain available.

## 18. Traceability to the review input

The design originated in `EffortCompile統合.eml`, but two later decisions
intentionally supersede parts of the mail. This table records the relationship
so future implementation does not restore an obsolete intermediate proposal.

| Review-input concern or proposal | Final treatment in this document |
| --- | --- |
| current `solver_steps` is not whole-compiler effort | confirmed by source audit in Section 3.2 |
| budget exhaustion currently fails and loses the frontier | confirmed in Sections 3.2 and 3.3; replaced by paused WorkCapsules |
| residual and incomplete must remain distinct | retained in Sections 2.5, 8.8, and 10.7 |
| normalization fuel is per request rather than one complete global account | confirmed in Section 3.2 |
| `.p` and resumable `.a` should be compiler inputs | retained in Sections 7, 15, and 20 |
| completed and unfinished information may coexist in one `.a` | retained through authority-separated sections and demand-relative views |
| one `advance(A, effort, demand)` operation | superseded: it may be one sequential scheduler adapter, but parallel `attempt`, proposal, merge, and check are fundamental |
| one global goal frontier for every subsystem | narrowed: orchestration is shared, typed semantic engines remain separate |
| accepted Claim replay proves artifact authority | superseded as the target final boundary by from-scratch checked-Core type checking; retained as current v86 authority and migration oracle |
| proof materialization may occur after initial function compilation | retained as later ordinary proof declarations checked against an immutable checked base |
| source edits require positive/negative dependencies and retraction | retained in Sections 2.2, 9.5, 10.9, and 16.7 |
| accepted monotonicity, confluence, multiple supports, and transactionality | restored explicitly in Sections 10.3 and 10.11 through 10.13 |
| compile effort is distinct from object totality | restored explicitly in Section 10.14 |
| compile-time purity and runtime exactly-once effects | retained in Sections 3.6 and 10.16 |
| runtime and compile can share one solver | narrowed: they may share container and scheduling protocol, but their semantic states and machines remain separate |

The retained core of the mail's problem statement is therefore:

1. compilation work should be incremental, resumable, demand directed, and
   eventually edit incremental;
2. one physical `.a` may preserve both reusable work and checked semantic
   content without confusing their authority;
3. effort must control computation, never logical truth;
4. completed meaning must be independent of machine speed and producer
   schedule; and
5. a later proof or generated structure should be addable without rebuilding an
   earlier function's construction history.

The two corrections are:

1. no single linear state transition is semantically fundamental; and
2. no serialized compiler proof history is semantically fundamental once a
   complete from-scratch checker exists.

## 19. Explicit non-goals

This proposal does not require:

- gradual typing;
- accepting approximate typing results;
- converting effort exhaustion into runtime verification;
- one global mutable solver shared by all workers;
- making every compiler algorithm Datalog;
- executing effects during type checking;
- equality reflection from arbitrary user proofs;
- trusting serialized `accepted` flags;
- retaining compiler work queues in imported interfaces; or
- deleting current Claim/Derivation infrastructure before replacement coverage
  exists.

## 20. Final architectural summary

The proposed A Program compilation model is:

```text
.p source or existing .a state
            |
            v
parallel, effort-bounded, resumable producers
            |
            v
untrusted elaborated proposals + paused work capsules
            |
            v
deterministic structural merge
            |
            v
independent from-scratch type checking
            |
            v
checked views for import, proof extension, linking, or execution
```

The deepest invariant is:

> A checked A Program artifact is valid because its elaborated semantic content
> type-checks under the named calculus and intrinsic environment, not because a
> particular compiler reports that it once followed a particular sequence of
> solver or Derivation steps.

This restores the original progressive-compilation philosophy without forcing
the compiler into one sequential `advance` history. It also permits proofs and
other properties to be introduced later and in parallel, while preserving a
small, auditable final authority boundary.
