# Compiler Control-Flow and Branch Architecture Audit

Date: 2026-08-29 JST

Status: static architecture audit complete; no implementation change is
authorized by this document

Audited baseline: `63b00eb`

Follow-up implementation plan:
`doc/2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md`

## 1. Objective

This audit examines whether the current compiler has accumulated conditional
paths because its underlying data flow is unnecessarily complicated.

The objective is not to minimize the number of C `if` statements mechanically.
The objective is a smaller and more direct A Program implementation in which:

1. canonical objects are found or created through one path;
2. dependencies are recorded once;
3. one agenda advances the affected constraints;
4. solved semantic facts are never rediscovered by whole-store scans;
5. proof production reads sealed solutions and does not repair them;
6. validation checks the result without becoming another producer;
7. the same mathematical object has one representation and one mutable
   authority;
8. Core calculation and typed static semantics remain separate layers;
9. code volume decreases as a consequence of removing repeated discovery,
   refresh, relocation, and closure machinery.

This is the intended meaning of a simpler path. It does not mean replacing
distinct typing rules with one opaque generic branch.

## 2. Evidence Policy

The current source is the authority for this audit. Previous design documents
are used for:

- design intent;
- historical decisions;
- regression requirements;
- identifying implementation claims that must be revalidated.

They are not treated as proof that the current implementation has the stated
property. A checked box in an older plan is historical evidence only.

This distinction matters because the latest canonical-constraint plan states
that one generation pass creates an immutable hypergraph and that edge inputs
do not change during solving:

- `doc/2026-08-28T23-21-14-FAILING-ACC-AND-IF8-REGRESSION-REPAIR-PLAN.md:22-28`
- `doc/2026-08-28T23-21-14-FAILING-ACC-AND-IF8-REGRESSION-REPAIR-PLAN.md:198-212`

The current classifier topology approximates that design, but current
classifier payloads are refreshed, computation constraints are retired and
reopened, and effect constraints are deleted and regenerated. The document is
therefore a target architecture, not an accurate description of the complete
current solver.

## 3. Baseline Shape

The audited `.c` and `.inc` implementation under `src/prototype/src/` contains
164,930 lines.

| Subsystem | Lines |
| --- | ---: |
| Frontend | 52,804 |
| Kernel | 37,157 |
| Artifact | 17,214 |
| Checker | 14,360 |
| Identity | 13,585 |
| Core | 13,146 |
| Driver | 6,642 |
| Graph | 4,626 |
| Parametricity | 2,253 |
| Dimension | 1,625 |

The largest control-flow concentration is in frontend lowering. Important
units include:

| Unit | Approximate lines | Current role |
| --- | ---: | --- |
| `frontend/lowering/graph_construction.inc` | 10,867 | Surface lowering, occurrence construction, graph lookup |
| `frontend/function_graph.c` | 9,054 | Generated Function Graph declarations and evidence |
| `kernel/typing/accepted_replay.inc` | 7,833 | Independent replay of accepted derivations |
| `checker/session.c` | 7,531 | Independent checked-Core session |
| `identity/object_term_action.inc` | 6,156 | Identity action over object Terms |
| `frontend/lowering/context_and_type_lowering.inc` | 6,091 | Context and type lowering plus aggregate compile state |
| `frontend/lowering/constraint/motive_solver.inc` | 4,874 | Motive equations and solutions |
| `frontend/lowering/constraint/evidence_and_freeze.inc` | 4,385 | Proof production and solution freezing |

Large files and branch counts are investigation signals, not defects by
themselves. The defects below are established from repeated ownership,
rediscovery, regeneration, or phase feedback.

## 4. Current Compiler Path

The intended semantic layers remain sound and useful:

```text
Surface AST
    |
    v
Typed Occurrence Graph + Context/Substitution
    |
    +--> typed constraints and solutions
    |
    +-- checked erasure/materialization --> Core Term Graph
                                              |
                                              v
                                      normalization/reduction

typed constraints and solutions
    |
    v
Proposition/Claim/Derivation production
    |
    v
accepted replay + checked-Core + Universe closure
    |
    v
artifact publication / execution
```

The problem is the actual control path inside the middle of this diagram:

```text
pending Match resolution
    -> Context relocation
    -> classifier worklist
    -> Context relocation
    -> CBPV classifier binding
    -> computation constraint regeneration
    -> computation operand refresh
    -> request solve and solution copy
    -> computation operand refresh
    -> computation solve and solution copy
    -> effect constraint regeneration and solve
    -> compare global counters
    -> repeat the complete sequence

then:

final Match resolution
    -> final classifier solve
    -> Context relocation
    -> refined motive rebuild
    -> classifier solve again
    -> Context relocation again
    -> proof materialization
    -> proof closure loop
    -> normalization-conversion expansion
    -> second proof closure loop
    -> final Match motive repair
    -> seal and replay
```

The outer fixed point is visible in
`frontend/lowering/finalization_and_entrypoints.inc:1-213`. It surrounds a
classifier solver that already has its own dependency worklist at
`frontend/lowering/constraint/classifier_and_computation_propagation.inc:1395-1490`.
Proof materialization then has another queue at
`frontend/lowering/constraint/evidence_and_freeze.inc:4174-4270`, followed by
two more closure loops at
`frontend/lowering/finalization_and_entrypoints.inc:2422-2517`.

The compiler therefore does not currently have one dependency-driven path. It
has several partially overlapping convergence mechanisms.

## 5. Branch Classification

Conditional code must be classified before removal.

### 5.1 Irreducible theorem and syntax dispatch

These branches should remain explicit:

- `APP`, `MATCH`, induction-hypothesis, and computation-fold elimination rules;
- Term-tag and AST-tag dispatch;
- effect operation versus return clauses;
- artifact record-tag validation;
- parser grammar alternatives;
- positive and negative acceptance checks in the independent checker.

These alternatives state different rules. Encoding them all as a generic
handler may reduce source lines while hiding the kernel theorem being checked.

### 5.2 Canonical construction branches

These are also legitimate when they have one form:

```text
key = complete semantic key(input)
id = index.find(key)
if id exists:
    return id
id = arena.append(immutable_object)
index.insert(key, id)
return id
```

The `if` is not the problem. Multiple partial-key searches, fallback scans, and
phase-specific creation policies are the problem.

Hash interning is appropriate only when several producers can request the same
semantic object. A direct occurrence-indexed table is simpler when topology
guarantees exactly one object per occurrence. Hashing every relation would add
cost without improving identity.

### 5.3 Dependency-state branches

Branches such as `UNSEEN`, `PENDING`, `SOLVED`, `RESIDUAL`, and `REJECTED` are
valid if they form one explicit state machine and are advanced by dependency
events. They become debt when a phase infers the state again from payload shape,
old snapshots, or missing IDs.

### 5.4 History, repair, and synchronization branches

These are the main removal target:

- determine whether a stale record still matches the current occurrence;
- retire it and create or reopen another record;
- compare whole-store counters to discover whether work happened;
- re-run a global pass because an unrelated phase may have refined a fact;
- search Core-equal occurrences to recover lost typed identity;
- synthesize a missing solution during finalization;
- copy a solution between stores and branch on snapshot revisions;
- maintain global modes that make ordinary lowering behave differently.

These branches encode the history of how the compiler arrived at a state,
rather than the semantic rule being applied.

## 6. Findings

### F0. Critical: the Core calculation / typing boundary is incomplete

The intended two-layer design is correct:

- TermDB is the canonical static calculation graph, including terms which
  denote types;
- TypedOccurrenceGraph, Context, constraints, and judgements state where a Core
  Term occurred and how it is typed;
- Core reduction must not inspect the typing layer.

The current implementation satisfies important parts of this boundary.
`prototype_typed_occurrence` stores a `core_term`, Context, category, and
classifier separately, and explicitly permits several typed occurrences to
share one Core Term (`graph/typed_occurrence_model.h:44-103`). Runtime operation
dispatch uses the operation ID (`core/term/evaluation_and_conversion.inc:4212-4219`).

However, the source re-audit found concrete violations:

- `PROTOTYPE_TERM_EFFECT_OPERATION` contains a classifier Term
  (`core/term.h:582-585`), and Core canonicalization includes that classifier
  (`core/term/canonicalization.inc:544-548,2366-2368`);
- Core `term.h` owns category, computation-kind, and classifier-view APIs whose
  consumers are typing and replay modules (`core/term.h:95-115,797-800`);
- TermDB owns a type-instance cache keyed by TypeDeclaration semantic revision
  (`core/term.h:726-729`);
- Core normalization accepts a mutable TypeDeclarationDB and resolves
  type-directed type instances and unresolved Match labels from semantic schema
  (`core/term/evaluation_and_conversion.inc:1203-1247,1520-1542,2225-2269`);
- the Core definition-environment record includes a classifier even though
  unfolding needs the Term and transparency (`core/term.h:751-762`).

This does not justify separate value, computation, and type Term databases.
Type expressions are programs at the static stage and should continue to use
the same TermDB and normalizer. The defect is the reverse dependency from Core
syntax/reduction into typing authority.

Required correction:

- retain one TermDB for program and type-denoting terms;
- remove classifier, Context, proof, effect-solution, and occurrence identity
  from Core records and canonical keys;
- move classifier views and category/computation typing vocabulary to the
  graph/kernel typing layer;
- represent type-directed Core construction as a one-way checked projection
  from a typed occurrence after the required typing inputs are solved;
- do not insert unresolved or classifier-bearing provisional Core nodes and
  repair them later;
- make Core normalization depend only on TermDB, reduction profile, Core
  definitions, and narrowly defined immutable operational reduction data;
- remove TypeDeclaration semantic schema and all Layer T stores from Core
  runtime stepping APIs;
- update artifact and checker boundaries so Core operation records no longer
  serialize a classifier edge.

The one constraint graph and one semantic agenda proposed later in this audit
belong entirely to the typed Layer T. They do not replace or absorb TermDB.

### F1. Critical: finalization constructs missing Match motives

Evidence:

- `frontend/lowering/finalization_and_entrypoints.inc:1053-1177`
- `frontend/lowering/finalization_and_entrypoints.inc:2522-2595`

There are two non-motive-solver producers. During expected-classifier
refinement, a Match with no reusable motive allocates a binder, abstracts the
expected classifier over the scrutinee, and writes the result directly into
`motive_solutions`. Later, after inference, proof closure, normalization
conversion expansion, and classifier freezing, finalization walks every Match.
If no acceptable motive solution exists, it either extracts a Lambda from the
final classifier or allocates another binder and abstracts the classifier over
the scrutinee. It also writes this value back into
`classifier_solver.motive_solutions`.

This is not validation. It is a late semantic solver hidden in the freeze path.
It creates three bad properties:

1. the motive solver is not the sole producer of motive solutions;
2. expected propagation and proof finalization can each invent a solution;
3. an unresolved motive is not necessarily reported at the phase where it
   became unresolved.

The Issue 23 plan records removal of 63 lines of finalization fallback, but the
current general fallback remains. The historical completion statement is
therefore incomplete for the current source.

Required correction:

- the motive constraint graph must produce the motive or a typed residual;
- expected classifier information may create an explicit motive equation, but
  may not write the selected motive cell directly;
- finalization must only read a sealed motive solution;
- absence, ambiguity, or unsupported abstraction must fail before proof
  materialization;
- no finalization function may write any classifier, motive, Context, effect,
  or usage solution.

This is the first change because every later architecture invariant depends on
a genuine solver/finalizer boundary.

### F2. High: the constraint graph is not globally immutable

Classifier topology is kept stable after generation, but its semantic payload
is refreshed after branch pullback:

- `frontend/lowering/constraint/model_generation_and_index.inc:952-1055`
- `frontend/lowering/constraint/classifier_and_computation_propagation.inc:1395-1443`

The refresh resets solver state and overwrites Context-derived payload fields.
Stable edge count is weaker than immutable semantic identity.

The edge record itself also mixes immutable key data with mutable lifecycle and
cache fields: `state`, `reason`, `evidence_id`, `activation_revision`,
`projected_motive`, and `conversion_goal` are stored beside rule inputs in
`context_and_type_lowering.inc:355-400`. Effect construction assigns a
`PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_*` state to this shared field,
relying on its numeric correspondence with `OPERATION_CONSTRAINT_STATE_*`.
That accidental enum coupling is another symptom of an unsplit edge/state
record.

Computation constraints are more explicitly history-dependent:

- `frontend/lowering/constraint/context_computation_and_fixed_point.inc:1023-1240`

Stale JudgementDelta slots are marked by setting their occurrence to the
invalid ID. The index is rebuilt, all typed occurrences are rescanned, lifecycle
entries are created or reopened, and current premise snapshots are repopulated.
This function is called during each outer fixed-point round.

Effect constraints are deleted and regenerated from current classifier views:

- `frontend/lowering/constraint/effect_propagation_and_residuals.inc:989-1167`

The implementation explicitly truncates the effect suffix before every
generation. It then scans the existing effect constraints for every operation
to determine whether a fallback self-copy edge is needed
(`effect_propagation_and_residuals.inc:1109-1133`).

Required correction:

- distinguish immutable edge identity from mutable solution cells;
- edge inputs must refer to meta IDs, Context-expression IDs, effect-row meta
  IDs, and refinement-result IDs, rather than copies of their current Terms;
- computation and effect edge templates must be generated with occurrence
  topology, not regenerated from solved classifier shape;
- replacing or refining a meta must enqueue its dependents, not mutate edge
  identity or rebuild a suffix;
- remove stale-record retirement once one occurrence has one permanent
  computation edge.

### F3. High: multiple schedulers approximate one dependency graph

Current schedulers and convergence detectors include:

1. the outer inference counter loop;
2. the classifier ConstraintDB worklist;
3. branch-pullback restart via `goto`;
4. the proof-materialization occurrence queue;
5. the post-evidence 16-round closure loop;
6. the normalization-evidence 16-round closure loop;
7. repeated Context-resolution revision checks.

Each mechanism is locally understandable. Together they obscure which event
causes which work and force broad rescans.

Required correction:

- the existing ConstraintDB worklist is generalized into the one semantic
  agenda for classifier, Context, refinement, motive, computation, effect, and
  usage domains; a second graph or queue is not introduced beside it;
- each work item has explicit input dependencies and output meta/claim IDs;
- solving a meta or accepting a premise enqueues reverse dependents;
- completion means that the agenda is empty and all required roots are sealed;
- effort exhaustion records the remaining agenda, rather than relying on a
  partially advanced set of nested loops;
- no arbitrary 16-round closure bound remains. Cycle detection, residual
  classification, and effort limits are explicit alternatives.

Proof closure remains a distinct downstream monotone agenda because Claims and
Derivations are different mathematical objects. It may reuse the same queue
primitive, but it runs only after semantic solutions are sealed and cannot
enqueue semantic solver work.

This does not merge the typing rules. It merges only scheduling and dependency
notification.

### F4. High: unresolved Context extensions are materialized too early

`operation_solver_resolve_contexts` is a large relocation transaction:

- `frontend/lowering/constraint/context_computation_and_fixed_point.inc:99-1020`

It computes dirty Contexts, resolves current binder classifiers, interns new
immutable Context extensions, finds live substitutions, rebases morphisms, and
rewrites Context IDs in occurrences, constraints, cases, fold clauses, pending
facts, computation constraints, and derivation candidates.

The source re-audit found two deeper causes inside this path:

- `prototype_context_extend_occurrence` bypasses full-key lookup and always
  appends a Context (`kernel/context.c:198-301`), even though a globally fresh
  `BindingId` already distinguishes genuinely different lexical binders;
- provisional `classifier_ref.variable_id` stores an AST binder ID
  (`context_and_type_lowering.inc:4244-4257`), then Context resolution uses it
  as both source provenance and an unresolved classifier selector
  (`context_computation_and_fixed_point.inc:364-416`).

Consequently, the existing provisional representation is a useful storage
shape but not yet a correct semantic key. Keeping its API unchanged would
preserve the duplicate-state problem.

ContextDB and SubstitutionDB are mathematically necessary. Their immutable
interned representation is also appropriate. The architectural defect is that
an extension whose classifier is still being solved is assigned an apparently
final Context ID. Later refinement then requires global root relocation.

Required correction:

- use the existing Context candidate representation with
  `prototype_context_classifier_ref` variable/provisional references as the
  stable construction node; do not add a parallel Context-expression arena;
- replace AST-binder-based `variable_id` with an explicit typed classifier
  solution reference; keep source provenance outside the semantic key;
- remove occurrence-specific interning bypasses and route every Context
  extension through one full-key find-or-create operation;
- use `BindingId` for lexical binder identity and TypedOccurrence ID for typed
  source occurrence identity; do not encode either distinction by duplicating
  otherwise identical Context nodes;
- use that stable node ID in constraint topology;
- add one solution sidecar which projects a candidate Context ID to its
  materialized concrete Context ID when required classifier and substitution
  inputs are sealed;
- cache that one-way candidate-to-concrete projection;
- never rewrite every root merely because the solution behind a meta changed;
- retain ContextDB, SubstitutionDB, weakening, and projection as separate
  mathematical structures and proof rules.

The target is not mutable Context records. It is stable symbolic construction
followed by one immutable semantic materialization.

### F5. High: proof materialization feeds back into inference

Evidence:

- `frontend/lowering/constraint/evidence_and_freeze.inc:4067-4147`
- `frontend/lowering/constraint/evidence_and_freeze.inc:4227-4269`

On its initial pass, proof materialization propagates constructor domains. If
they change, it runs the classifier solver again. During each proof queue round,
it refreshes and solves computation constraints again.

This means “produce evidence for a solved graph” and “improve the solution” are
not separate phases. It also explains why finalization needs repeated closure
loops: proof discovery can expose information that ordinary inference did not
record as a dependency.

Required correction:

- constructor-domain propagation is a classifier constraint rule;
- computation replayability is a computation constraint rule;
- normalization-conversion premises are either pre-generated proof obligations
  or ordinary proof-agenda items;
- proof materialization may append Propositions, Claims, and Derivations, but it
  may not alter type/effect/usage/Context solutions;
- proof closure uses the same premise-to-dependent index as other agenda work,
  while preserving the distinct proof graph and trust boundary.

### F6. High: typed occurrence identity is reconstructed from Core Terms

`compile_prior_value_ref_at_depth` receives a Core Term and expected classifier,
then scans typed occurrences in reverse to find a compatible occurrence:

- `frontend/lowering/graph_construction.inc:3627-3712`

It checks ancestry, occurrence category, Context, and classifier, and retains a
unique fallback candidate. The checks reduce accidental aliasing, but the need
for the scan shows that a typed operation edge was lost earlier.

Core Term sharing is intentional: `\x : Bool => x` and `\x : Nat => x` may
share the same untyped Core Lambda. A later compiler stage must never infer
which typed occurrence was intended from that shared Core identity.

Required correction:

- lowering functions pass a typed `compile_ref` containing occurrence identity
  and Core identity;
- child construction records direct occurrence edges at creation time;
- APIs that require typing receive occurrence IDs, not only Term IDs;
- Core-only APIs remain limited to evaluation, structural transformation, and
  conversion under an explicit reduction environment;
- remove the reverse occurrence fallback after all callers carry direct
  identity.

### F7. Medium: Function Graph staging is represented as a global mode

The driver compiles the source prefix, generates Function Graph declarations,
then invokes pending compilation again:

- `driver/compiler_session.c:220-366`

Graph construction branches on global `function_graph_preflight` state:

- `frontend/lowering/graph_construction.inc:6721-6840`

Two stages are legitimate because generated declarations depend on accepted
source declarations. The global mode is not. It causes ordinary lowering to
change entry selection, request handling, and accepted roots according to
ambient state.

Required correction:

- represent source and generated work as explicit `CompileUnit` records;
- give each unit declared inputs, generated roots, and an immutable accepted
  prefix certificate;
- schedule units in a dependency DAG;
- pass unit policy explicitly to construction APIs;
- remove global preflight branches from ordinary graph construction.

### F8. Medium: accepted replay and Universe closure have no aggregate state

The driver validates accepted derivations and only later constructs the closed
Universe solution:

- `driver/compiler_session.c:367-395`

Universe level identity comparison in conversion is no longer the old
“all variables are equal” defect. Producer and checker Universe solvers are
legitimate independent components. The remaining architecture issue is that
the name “accepted graph” can describe a graph whose full module certificate is
not yet closed.

Required correction:

- introduce an aggregate `ModuleCertification` state;
- record separate completion capabilities for typing replay, Universe closure,
  occurrence validation, totality obligations, and checked-Core validation;
- artifact publication and execution require the appropriate aggregate state;
- do not merge independent producer and checker implementations.

### F9. Medium: missing direct indexes cause repeated scans

Confirmed examples include:

- effect generation scans all current effect constraints for every operation;
- constructor saturation validation scans occurrences and their potential
  parents rather than consuming a reverse occurrence index;
- `operation_binder_owner_find` linearly scans all binder-owner entries for
  repeated BindingId queries during lowering and Context resolution;
- several identity, publication, and finalization helpers scan all Claims or
  Derivations for exact semantic keys.

Not all complete scans are wrong. Serialization, deterministic publication,
closure marking, and an independent audit often require them. The criterion is
whether the query is repeated inside construction or solving.

Required correction:

- inventory hot query shapes from performance counters;
- add typed indexes owned by the immutable arena or graph;
- use direct arrays for one-per-occurrence facts;
- use full-key hash indexes for interned composite facts;
- use reverse adjacency for dependents and parents;
- keep full scans in explicit validation/publication passes only.

### F10. Medium: `compile_context` is an untyped phase aggregate

`frontend/lowering/context_and_type_lowering.inc:874-971` stores nearly every
mutable frontend structure, cache, pending array, imported interface, effort
state, and an integer `producer_phase`. Any helper accepting this pointer can
read or mutate almost every compiler subsystem.

The aggregate does not itself duplicate semantics, but it makes phase boundary
violations such as F1 and F5 easy.

Required correction:

- split narrow stage capabilities such as `GraphBuildContext`,
  `ConstraintSolveContext`, `ProofBuildContext`, and `FreezeContext`;
- expose immutable views where a phase is read-only;
- represent resumable stage state with a tagged stage record rather than a bare
  phase integer;
- make illegal writes unrepresentable in normal C APIs.

### F11. Medium: fixed capacities and magic convergence bounds mix policy with logic

Examples include 4,096-entry proof queues, 8,192 Context/Substitution limits,
and two 16-round proof closure limits.

Hard capacity checks are valid implementation guards, but they must not define
the accepted logical fragment accidentally.

Required correction:

- replace stack-sized semantic work queues with reusable growable arenas;
- charge explicit effort for growth and work-item processing;
- classify capacity exhaustion separately from an invalid program;
- replace round limits with agenda completion and explicit residual/cycle
  results.

## 7. Findings That Are Not Confirmed Defects

The following apparent duplications should not be collapsed merely to reduce
lines:

### 7.1 Core Term versus typed occurrence

Core Term records computation without typing occurrence identity. Typed
occurrence records where and under which rule that Term appears. F6 requires a
stronger direct link, not elimination of either layer.

### 7.2 Context versus Substitution

Context is an object/telescope. Substitution is a morphism between Contexts.
Weakening evidence and projection morphisms should reference one another where
appropriate, but they are not the same record.

### 7.3 Proposition versus Claim versus Derivation

These represent a judgement, its acceptance, and why it follows. Management and
indexing plumbing may be shared; the objects must remain distinct.

### 7.4 Producer replay versus checked-Core

Independent checking intentionally repeats theorem validation across a trust
boundary. Sharing declarative schemas and immutable views is valid; sharing the
acceptance decision is not.

### 7.5 Distinct eliminators

`APP_ELIM`, `MATCH_ELIM`, induction-hypothesis elimination, and
`COMPUTATION_FOLD_ELIM` prove different statements. Common transaction,
premise-view, and diagnostic helpers are appropriate, but one generic theorem
switch is not a target.

### 7.6 TypeDeclaration semantic and derived stores

The current TypeDeclaration wrapper separates semantic schema, readback,
representation cache, and constructor-classifier cache more clearly than old
audits describe. The remaining broad mutable pointer APIs are capability
hygiene, not proof of a second semantic authority.

### 7.7 Core normalization profiles

The Core normalizer already keys cached results by reduction profile and graph
and semantic revisions. Beta, iota, and permitted intrinsic reduction should
remain rules in one evaluator rather than separate evaluators.

## 8. Target Architecture

### 8.1 Stores

The target uses distinct stores for distinct mathematical objects, with one
authority each:

| Store | Contents | Mutability |
| --- | --- | --- |
| `TermStore` | Hash-consed Core Terms | Append-only |
| `OccurrenceGraph` | Source/generated typed operation topology | Append then seal |
| `ContextDB` candidate arena | Context extensions, including existing variable/provisional classifier references | Interned append-only |
| Context solution sidecar | Candidate-to-concrete Context projection | Mutable monotone cells |
| `SubstitutionDB` | Materialized Context morphisms | Interned append-only |
| `ConstraintGraph` | Typed immutable dependency edges | Append during graph build, then seal |
| `SolutionStore` | Classifier/effect/usage/motive/refinement meta states | Mutable monotone cells |
| `ProofGraph` | Propositions, Claims, Derivations | Append-only monotone closure |
| `ModuleCertification` | Completion capabilities from independent checks | Monotone stage record |

`ConstraintGraph` is the evolved existing `operation_constraint_db`, not a
second graph. It may contain several edge kinds. This is one scheduling graph,
not one theorem. Each edge kind retains a narrow rule handler.

### 8.2 One direct compilation path

```text
1. Parse and resolve source names.
2. Build explicit source CompileUnit dependencies.
3. Lower once to Core Terms and typed occurrence topology.
4. Generate stable Context candidates and all typed constraint templates once.
5. Seal topology.
6. Seed one agenda with unsolved roots.
7. Process dependency-triggered work until empty or effort is exhausted.
8. Seal required solutions or emit typed residuals.
9. Materialize the proof DAG with one downstream proof agenda.
10. Close module certificates, including Universe and checked-Core results.
11. Freeze projections and publish the artifact.
```

There is no “try a pass, scan everything, compare counts, and try again” step.
There is no final semantic repair. A later generated CompileUnit may append a
new graph transaction, but it does not reinterpret the accepted prefix.

### 8.3 Work-item model

The existing ConstraintDB worklist is generalized to process work items with:

```text
WorkItem {
    rule_kind;
    immutable_edge_id;
    required_input_ids[];
    output_id;
}
```

Processing is:

```text
item = agenda.pop()
inputs = solution_store.read(item.required_input_ids)
result = rule[item.rule_kind](edge, inputs)
transition = solution_store.join(item.output_id, result)
if transition changed the output:
    agenda.enqueue(reverse_dependents[item.output_id])
```

The `join` operation must be defined per solution domain. It may reject
incompatible solved values, produce a residual, or advance a monotone state. It
must not silently choose a newer historical payload.

### 8.4 Find-or-create discipline

Use the smallest correct identity mechanism:

- Term: structural hash-consing with binding identity rules;
- one classifier/computation result per occurrence: direct indexed cell;
- candidate Context: full semantic key using BindingId and typed solution
  references;
- concrete Context/Substitution: existing full semantic interning after the
  occurrence-specific bypass is removed;
- composite constraint requested by several rules: full-key constraint
  interning;
- proof proposition: immutable proposition interning;
- multiple derivations for one proposition: append distinct derivation nodes,
  never collapse by conclusion alone.

No fallback may use only a shared Core Term as the key for typed identity.

## 9. Migration Plan

The migration must not preserve old and new mutable authorities in parallel to
reduce implementation effort.

### A0. Instrument and freeze invariants

- [ ] Add debug assertions that finalization does not write solver cells.
- [ ] Record topology counts and semantic edge digests at graph seal.
- [ ] Assert that solving cannot change edge digests.
- [ ] Record agenda enqueue causes and full-scan counts.
- [ ] Add a branch-purpose annotation convention for theorem, state,
  validation, fallback, and compatibility branches.
- [ ] Capture per-file and total lines before migration.

### A-C. Re-establish the Core calculation / typing boundary

- [ ] Remove classifier fields from Core Term records and canonical keys.
- [ ] Move typing-only category and classifier-view APIs out of Core ownership.
- [ ] Split Core and typed definition environments.
- [ ] Move TypeDeclaration-dependent caches out of TermDB.
- [ ] Replace unresolved type-directed Core nodes with checked occurrence-to-Core
  projection after typed inputs are solved.
- [ ] Make ordinary Core reduction independent of TypeDeclarationDB and all
  Layer T stores.
- [ ] Update artifact/checker formats and delete classifier-bearing Core wire
  compatibility.

Gate: a sealed Core graph can reduce without TypedOccurrence, Context,
Constraint, Solution, Judgement, or mutable TypeDeclaration stores.

### A1. Remove final Match motive repair

- [ ] Move every valid motive construction into motive constraint handlers.
- [ ] Represent unsupported/ambiguous motives as explicit residuals.
- [ ] Make finalization reject an unsealed required motive.
- [ ] Delete `finalization_and_entrypoints.inc:2522-2595` semantic repair.
- [ ] Add constant, unary, indexed, neutral-scrutinee, one-IH, and two-IH tests.

Gate: finalization is read-only with respect to all solver stores.

### A2. Preserve occurrence identity end to end

- [ ] Introduce a typed compile reference used by lowering APIs.
- [ ] Record direct parent/child occurrence edges during construction.
- [ ] Convert callers of `compile_prior_value_ref_at_depth`.
- [ ] Delete Core-to-occurrence reverse fallback scans.
- [ ] Add shared-Core Bool/Nat Lambda and repeated Match regressions.

Gate: no typing path selects an occurrence by Core Term alone.

### A3. Make all constraint topology immutable

- [ ] Split immutable computation payload from current operand solutions.
- [ ] Generate one computation edge per request/fold occurrence.
- [ ] Generate effect equations over row metas during occurrence lowering.
- [ ] Replace payload refresh with meta dependencies.
- [ ] Remove stale computation slot retirement and suffix regeneration.
- [ ] Add direct effect-edge indexes.

Gate: topology and edge digest remain unchanged from seal through freeze.

### A4. Stabilize candidate Context construction

- [ ] Treat existing provisional/variable Context classifier references as the
  migration seed instead of introducing another arena.
- [ ] Replace AST binder IDs in classifier references with explicit typed
  classifier solution references.
- [ ] Remove occurrence-specific Context append APIs and use one complete-key
  find-or-create path.
- [ ] Use candidate Context IDs in constraint edges.
- [ ] Materialize final Context/Substitution objects once inputs are sealed.
- [ ] Delete global Context root relocation from ordinary solving.

Gate: classifier refinement does not rewrite occurrence or proof roots.

### A5. Replace nested semantic convergence loops with the ConstraintDB agenda

- [ ] Generalize the existing worklist with typed solution dependencies.
- [ ] Move branch refinement, motives, computation, effects, usage, and Context
  materialization onto the agenda.
- [ ] Replace global count comparison with event enqueueing.
- [ ] Persist the remaining agenda in effort-paused sessions.
- [ ] Delete classifier restart and broad fixed-point loops.

Gate: one scheduler reaches a deterministic quiescent or residual state.

### A6. Make proof production one-way with one downstream proof agenda

- [ ] Move constructor-domain refinement into A3 classifier edges.
- [ ] Move computation premise readiness into A3 computation edges.
- [ ] Generate normalization-conversion proof obligations before proof closure.
- [ ] Use premise-to-derivation reverse dependencies.
- [ ] Delete both 16-round loops and proof-time solver invocations.

Gate: proof production can append evidence but cannot change semantic
solutions.

### A7. Replace Function Graph global modes with CompileUnits

- [ ] Define source and generated CompileUnit records.
- [ ] Record explicit dependency and accepted-prefix capability.
- [ ] Pass unit policy explicitly.
- [ ] Remove ambient `function_graph_preflight` branches.
- [ ] Verify source-only, generated-only, and appended artifact equality.

### A8. Add aggregate module certification

- [ ] Define certification components and transition rules.
- [ ] Require Universe closure explicitly before publication.
- [ ] Preserve independent checked-Core authority.
- [ ] Make publication requirements visible in artifact diagnostics.

### A9. Narrow phase capabilities

- [ ] Split the aggregate `compile_context` into narrow views.
- [ ] Replace the integer producer phase with a tagged stage state.
- [ ] Make proof/freeze APIs accept const solution views.
- [ ] Replace fixed work queues with reusable growable arenas.

### A10. Remove repeated scans and measure source reduction

- [ ] Add indexes only for measured repeated query shapes.
- [ ] Replace parent discovery with reverse adjacency.
- [ ] Replace repeated exact Claim/Derivation scans where they occur in hot
  construction paths.
- [ ] Retain explicit full scans for validation, closure marking, deterministic
  serialization, and independent checking.
- [ ] Run unused-code and unreachable-branch analysis after old paths are gone.

## 10. Required Verification

Every stage must preserve or add these permanent boundaries:

- all current integration tests;
- shared Core with distinct Bool and Nat annotations;
- generic indexed family instantiation and dependent Match;
- Acc general eliminator and concrete checked-Core validation;
- IF8 and fuel-free QuickSort positive and negative decrease cases;
- one-IH and two-IH Function Graph cases;
- computation request and multi-clause fold cases;
- empty, inferred, and non-empty effect rows;
- artifact write/read/replay and deterministic equality;
- independent checked-Core rejection tests;
- effort pause/resume equivalence with from-scratch compilation;
- unresolved motive rejection before finalization;
- graph topology and edge digest stability during solving;
- proof production cannot change classifier/effect/usage/motive revisions.

Performance counters must report at least:

- occurrence visits by phase;
- full-store scans by query kind;
- constraint edges generated by domain;
- agenda pushes, duplicate pushes, and pops;
- candidate Context and concrete Context materialization counts;
- solution transitions and rejected incompatible joins;
- proof work items and deferred items;
- accepted replay and checked-Core time separately.

## 11. Code-Volume Accounting

Code reduction is an explicit secondary acceptance criterion. It must be
reported per file and by subsystem after each migration stage:

```text
path | baseline lines | added | deleted | final lines | reason
```

Expected deletion sources are:

- final motive repair;
- stale computation-record retirement;
- effect suffix regeneration;
- global counter convergence checks;
- Context root relocation and repeated rewriting;
- duplicate proof closure loops;
- Core-to-occurrence fallback scans;
- global Function Graph mode branches;
- compatibility helpers retired after direct callers migrate.

New graph types and indexes will add some lines. A stage that only adds a new
layer while retaining old scheduling, refresh, and repair paths is incomplete.
The target is a net reduction in frontend lowering and finalization, even if a
small reusable graph module grows.

No line-count target justifies merging theorem rules, removing independent
validation, or weakening diagnostics.

## 12. Priority and Decision

The user's concern is substantially confirmed. The primary problem is not the
raw number of conditional statements. It is that current lowering combines a
useful immutable graph core with several mutable, partially overlapping phase
systems. Those systems repeatedly rediscover current state and compensate for
lost dependencies with refresh, relocation, rescan, fallback, and late repair.

The immediate sequence is:

1. A0 instrumentation;
2. A-C Core calculation / typing boundary;
3. A1 final-motive authority repair;
4. A2 occurrence identity preservation;
5. A3 immutable computation/effect/classifier dependency topology;
6. A4 candidate Context identity and materialization;
7. A5 one agenda;
8. A6 one-way proof production;
9. A7-A10 boundary cleanup, certification, capability narrowing, and measured
   deletion.

The target A Program compiler remains layered, because Terms, occurrences,
Contexts, substitutions, constraints, proofs, and checker certificates are
different objects. It becomes simpler because every object is constructed once,
every dependency is explicit, and every stage has one forward path.
