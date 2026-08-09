# Post-SC1 Physical Compaction Audit and Plan

Date: 2026-08-09

Status: codebase-reviewed; narrowed V3-PC1 gate completed before V2-A1

Repository baseline:

- branch: `main`;
- commit: `7cc6dc9` (`Consolidate semantic proof infrastructure`);
- ordinary artifact format: v69;
- V3-SC1 semantic consolidation: complete; and
- local documentation edits are outside the source audit and are not modified
  by this plan.

Related documents:

- `2026-08-09T13-23-19-SEMANTIC-CONSOLIDATION-AND-RESOURCE-READY-REINDEX-PLAN.md`;
- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T13-11-17-V2-A1-OBJECT-HOTT-ARTIFACT-IMPLEMENTATION-PLAN.md`; and
- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`.

## 1. Executive Summary

SC1 succeeded as a semantic consolidation even though total production line
count increased. It deleted duplicated authority paths and introduced shared
infrastructure plus stronger validation. The measured production change was:

```text
added      3,580 lines
deleted    2,971 lines
net          609 lines
```

The next compaction work should not repeat SC1 at a more generic level. The
remaining high-value findings are physical normalization of references and
variable-arity data:

1. finish Proposition identity normalization by replacing copied Proposition
   tuples with typed IDs;
2. move solver constraint operands out of maximum-sized embedded arrays;
3. remove the temporary v69 expanded-tuple adapter during the v70 break;
4. replace copied runtime environments with persistent or stack frames;
5. centralize declarative proof-kind metadata while keeping validators
   independent; and
6. share only structural Term child enumeration, not binder-sensitive
   algorithms.

Codebase review narrows the pre-A1 requirement. Only the persistent accepted
part of item 1 must precede A1: accepted scoped premises must become
Proposition-ID edges, and Claims must stop caching Proposition pointers. Item 3
is V2-A1 itself. Solver operands, runtime frames, metadata descriptors, Term
child views, and session storage remain valid later work, but they do not shape
the v70 accepted-proof wire contract and therefore do not block A1.

## 2. What SC1 Actually Reduced

The raw net line count obscures real deletion. SC1 removed or consolidated:

- two physical layouts for candidate and accepted conclusion propositions;
- twelve maximum-sized premise arrays embedded in candidate and accepted
  Derivation identity;
- separate Context-formation and Substitution-formation certificate database
  implementations;
- repeated HOTT Context/Substitution certificate store parameters;
- duplicated weakening and general-reindex proposition reconstruction;
- repeated HOTT deterministic-outcome fields and validation paths; and
- part of the successful HOTT test fixture setup.

SC1 also retained distinctions which must not be removed for compactness:

```text
Context != Substitution
Proposition != accepted Claim
Claim != Derivation
Term != typed Operation occurrence
structural reindex != resource admissibility
constructor != independent validator
static Context != runtime environment
```

Therefore the correct interpretation is:

```text
physical code size: slightly larger
number of semantic authority paths: smaller
validation and forgery surface: stronger
remaining copied record data: still substantial
```

## 3. Audit Method and Measurements

The audit inspected the current in-memory records, capacity declarations,
artifact adapters, runtime environment operations, large functions, and
repeated tag dispatches.

Measured record sizes on the baseline C11 build are:

| Record | Size |
| --- | ---: |
| `prototype_judgement_proposition` | 48 bytes |
| `prototype_judgement_candidate_premise` | 48 bytes |
| `prototype_judgement_premise_edge` | 56 bytes |
| `prototype_judgement_selected_evidence` | 28 bytes |
| `prototype_judgement_claim` | 40 bytes |
| `prototype_judgement_derivation_candidate` | 64 bytes |
| `prototype_judgement_derivation` | 64 bytes |
| `prototype_judgement_computation_constraint` | 2,688 bytes |
| `prototype_judgement_effect_row_constraint` | 156 bytes |

Additional source measurements are:

- `PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES` occurs 85 times in `typing.c`;
- `typing.c` contains at least 32 maximum-sized temporary premise arrays;
- `term.c` contains approximately 399 `PROTOTYPE_TERM_*` case labels across
  its separate algorithms;
- `prototype_hott_execute_term_action()` spans approximately 1,000 lines;
- `operation_solver_reify_core_proof()` spans approximately 870 lines;
- the v69 artifact graph reader spans approximately 768 lines; and
- the main accepted-proof validator spans approximately 518 lines.

Function length is an orientation metric, not an automatic refactoring order.
Some long validators are explicit because the accepted calculus has many
distinct rules.

## 4. Finding P1: Proposition Identity Is Not Yet Physically Canonical

### 4.1 Current representation

`prototype_judgement_proposition` is interned and has an immutable ID. However,
several adjacent records still copy the complete tuple.

The current candidate premise is:

```c
struct prototype_judgement_candidate_premise {
	struct prototype_judgement_proposition proposition;
};
```

The current accepted premise edge is:

```c
struct prototype_judgement_premise_edge {
	uint32_t claim_id;
	struct prototype_judgement_proposition scoped_proposition;
};
```

`prototype_judgement_selected_evidence` separately repeats kind, authority,
Context, Operation, subject, and classifier. A Derivation candidate retains a
conclusion Proposition ID and also retains the conclusion tuple fields.
`prototype_judgement_claim` retains both a Proposition ID and a direct pointer.

These representations are understandable migration intermediates, but they
leave multiple writable or relocatable copies of one mathematical
Proposition.

### 4.2 Scoped does not mean uninterned

A scoped premise must not automatically become an accepted Claim. That does
not require it to remain an uninterned tuple.

The three states are distinct:

```text
interned Proposition
  immutable proposition identity exists

scoped premise
  a Derivation refers to the Proposition locally
  no accepted Claim is implied

accepted Claim
  publication/closure state refers to the Proposition
```

Consequently, scoped premises can safely use Proposition IDs while preserving
the Claim boundary.

### 4.3 Reviewed target representation

The accepted graph does not need a persistent arena tag. It uses one accepted
Proposition arena and a discriminated premise reference:

```c
struct prototype_judgement_premise_edge {
	uint32_t claim_id;
	uint32_t scoped_proposition_id;
};
```

Exactly one field is valid. A valid Claim already fixes its Proposition, while
a scoped Proposition ID deliberately does not create a Claim.

Normalize the persistent records before A1:

```text
AcceptedPremiseEdge
  accepted Claim id or scoped Proposition id

Claim
  proposition_id
  closure/publication fields
```

Candidate premises, candidate conclusion caches, selected evidence, and delta
references are compiler-local and are not part of the pre-A1 gate. A later
solver compaction may introduce owner-typed local references, but delta arena
tags must not leak into accepted identity or the artifact.

### 4.4 Remove the cached Claim pointer

`prototype_judgement_claim.proposition` duplicates
`prototype_judgement_claim.proposition_id` and must be repaired after arena
movement, readback, linking, or index rebuild.

Replace direct pointer storage with a checked accessor:

```c
const struct prototype_judgement_proposition*
prototype_judgement_claim_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
);
```

If profiling later proves the lookup material, cache a stable arena base in a
read-only view. Do not store a second identity-bearing pointer in every Claim.

### 4.5 Expected effect

The pre-A1 accepted edge with two discriminated IDs is approximately 8 bytes
rather than 56 bytes. Removing the Claim pointer also removes one repairable
machine address per Claim. Exact padding is recorded by the PC1 baseline and
exit measurements.

The larger benefit is deletion of:

- scoped tuple hashing and comparison;
- pointer repair after Proposition movement;
- tuple expansion during internal relocation;
- accepted-edge field-by-field relocation; and
- a second Claim-to-Proposition identity representation.

Candidate premise copying, candidate conclusion synchronization, and selected
evidence snapshots remain possible later solver-compaction targets. They are
not claimed as PC1 deletions.

The current artifact slice additionally assumes that Proposition slots and
Claim slots have the same count and corresponding IDs. Scoped Proposition IDs
invalidate that shortcut. PC1 must introduce independent Proposition and Claim
reachability/remaps while retaining tuple expansion only at the v69 wire
boundary.

### 4.6 Safety requirements

- Proposition ID equality is valid only within the exact arena/session.
- Hash equality remains insufficient; interning must compare complete keys.
- A Proposition ID must never imply an accepted Claim.
- Accepted Claim DAG ordering remains separate from Proposition allocation
  order.
- Constructor and validator continue to resolve IDs independently.

## 5. Finding P2: Solver Constraints Still Reserve Maximum Arity

### 5.1 Current cost

Accepted and candidate Derivations no longer embed maximum premise arrays, but
`prototype_judgement_computation_constraint` still contains arrays for the
maximum computation-fold arity:

```text
premise_operations[64]
premise_contexts[64]
premise_states[64]
premise_classifiers[64]
premise_evidence[64]
```

The record is 2,688 bytes. The compile workspace reserves 4,096 records, which
is approximately 11 MB for this one record class even when programs contain
few or small constraints.

The workspace also reserves:

```text
4,096 * 64 candidate premises * 48 bytes  ~= 12.0 MiB
4,096 computation constraints * 2,688     ~= 10.5 MiB
```

The read-file and REPL entry points separately reserve candidate and accepted
premise arrays sized as `JUDGEMENT_CAPACITY * MAX_PREMISES`. The accepted edge
pool alone is approximately 14 MiB at the current record size.

### 5.2 Target operand arena

Move variable operands into a solver-local arena:

```c
struct prototype_judgement_constraint_operand {
	uint32_t operation_id;
	uint32_t context_id;
	struct prototype_judgement_proposition_ref evidence;
	uint32_t classifier;
	unsigned char state;
};

struct prototype_judgement_computation_constraint {
	int kind;
	uint32_t context_id;
	uint32_t operation_id;
	uint32_t first_operand;
	uint32_t operand_count;
	/* scalar rule state only */
};
```

The maximum clause count may remain a language or implementation limit. It
should be checked when appending a slice, not paid by every constraint record.

### 5.3 Builders replace parallel temporary arrays

Many proof constructors currently create separate local arrays for subject,
classifier, ID, Context, or evidence. Introduce one checked slice builder:

```text
premise_builder_begin(mark)
premise_builder_add(proposition_ref, accepted_claim_or_invalid, role)
premise_builder_finish(first, count)
premise_builder_rewind(mark)
```

The builder must be transactional because failed solver paths currently rewind
their delta. It must not allocate one heap object per premise.

### 5.4 Do not unify accepted and mutable storage by aliasing

Candidate/constraint operands are mutable solver state. Accepted Derivation
edges are immutable proof graph state. They may share record shape and iterator
APIs, but commit must copy or intern an immutable accepted slice. An accepted
Derivation must never point into rewindable delta storage.

## 6. Finding P3: v69 Re-expands the New Graph

### 6.1 Temporary adapter

SC1 deliberately retained v69 and translates between:

```text
in-memory Proposition/edge graph
        <->
expanded v69 Claim and premise tuples
```

The adapter is currently responsible for write expansion, read interning,
candidate reconstruction, append relocation, and pointer repair. It is valid
as a short-lived migration layer, but it now represents a major remaining
source of physical duplication.

### 6.2 v70 target

The v70 artifact should directly serialize:

```text
PropositionRecord
  kind
  authority
  Context
  Operation
  subject
  classifier

ClaimRecord
  proposition_id
  closure/publication data

PremiseEdgeRecord
  proposition_id
  accepted_claim_id_or_invalid

DerivationRecord
  conclusion_claim_id
  proof kind
  rule data
  semantic action
  first premise
  premise count
```

Relocation then maps each ID-bearing field exactly once. It does not reconstruct
the same Proposition tuple in Claim, candidate, and scoped-premise storage.

### 6.3 Migration rule

Perform one strict schema break:

- write only v70;
- read only v70 in the kernel reader;
- delete the v69 tuple adapter from the accepted path;
- use an external one-shot converter if old artifacts must be retained; and
- preserve deterministic bytes and collision-safe index rebuild.

Do not retain both v69 and v70 readers in the prototype kernel merely to make
the transition appear smaller.

## 7. Finding P4: Runtime Environment Copies Are Independent Waste

### 7.1 Current behavior

`operation_runtime_environment` contains 512 bindings. Every extension copies
the entire source environment before appending one binding:

```c
*p_ret = *source;
```

Captured requests and resumption returns embed environments again. Term
instantiation then traverses the Term once per Term-valued binding by repeatedly
calling bound-variable substitution.

This is unrelated to static Context sharing. It must not be addressed by
merging Runtime Environment with ContextDB or SubstitutionDB.

### 7.2 Target runtime design

Use one of:

1. parent-linked immutable environment frames;
2. a stack with explicit save/restore marks; or
3. a stack for normal evaluation plus immutable captured frame slices for
   resumptions.

Recommended prototype direction:

```text
RuntimeBindingArena
  binding records

RuntimeEnvironment
  parent frame ID
  first binding
  binding count

RuntimeCapture
  environment frame ID
  handler slice
  continuation slice
  multiplicity state
```

Build one simultaneous runtime substitution or indexed lookup view when a Term
must be instantiated. Do not perform one whole-Term traversal per binding.

### 7.3 Resource-sensitive compatibility

Future linear or affine runtime semantics can attach consumption state and
resumption multiplicity to runtime frames. Static usage evidence remains in
resource-sensitive Judgements. Neither belongs in static Context identity.

## 8. Finding P5: Proof-Kind Metadata Is Repeated

`typing.c` has several exhaustive proof-kind switches for:

- structural premise-to-Operation mapping;
- validator dispatch;
- proof name rendering;
- conclusion/category classification;
- premise arity checks;
- semantic action requirements; and
- artifact-visible status.

Introduce declarative metadata for only the facts which are genuinely common:

```c
struct prototype_judgement_rule_descriptor {
	int proof_kind;
	const char* name;
	int conclusion_kind;
	int arity_kind;
	uint32_t fixed_arity;
	int semantic_action_kind;
	int artifact_status;
};
```

Variable structural child mappings may use a separate typed helper indexed by
rule kind.

Do not place one unchecked constructor/validator callback in the descriptor.
Rule-specific replay must remain explicit and independent. The descriptor can
reject impossible metadata before dispatch, after which the validator proves
the actual rule.

Benefits include:

- one authoritative proof name;
- one arity declaration;
- coverage checks for every admitted rule;
- smaller display and pre-validation switches; and
- less risk that a new rule is serializable but not replayable, or replayable
  but omitted from classification.

## 9. Finding P6: Structural Term Traversal Is Repeated

`term.c` necessarily contains separate algorithms for:

- alpha/cross-shape equality;
- canonical hashing;
- free-variable and binder analysis;
- substitution and reindexing;
- evaluation and normalization; and
- conversion.

These algorithms must not be collapsed merely because each switches on the
same Term tag. In particular, binder-sensitive equality and substitution have
different state and proof obligations.

However, passive structural operations can share a child-role view:

```c
enum prototype_term_child_role {
	PROTOTYPE_TERM_CHILD_ORDINARY,
	PROTOTYPE_TERM_CHILD_BINDER_BODY,
	PROTOTYPE_TERM_CHILD_TYPE_FAMILY,
	PROTOTYPE_TERM_CHILD_MATCH_CASE,
	PROTOTYPE_TERM_CHILD_EFFECT_ROW
};

struct prototype_term_child {
	int role;
	uint32_t term_id;
	uint32_t binder_id_or_invalid;
};
```

Suitable consumers are:

- reachability marking;
- artifact relocation discovery;
- generic graph statistics;
- debug graph output;
- non-binding child bounds checks; and
- capability coverage checks.

Unsuitable consumers include full alpha equality, dependent substitution,
normalization, and conversion. Those remain typed algorithms even if they use
the common child view for simple cases.

## 10. Finding P7: Large Entrypoint Storage Is Duplicated

`read_file.c` and `repl.c` each declare many fixed-capacity global arrays and
repeat database initialization calls. This duplicates capacity policy and
makes the cost of a record-layout change appear in several entry points.

Introduce an owning session allocation layer:

```text
PrototypeSessionStorage
  capacities
  allocated arenas
  initialized DBs
  release function

PrototypeKernelView
  checked non-owning view of initialized stores
```

The storage owner and kernel view must remain different concepts. A view is a
capability bundle; it does not allocate or free. Tests may continue to provide
small caller-owned arenas through narrow initialization APIs.

Prefer capacity from requested program size or a growth policy over reserving
`JUDGEMENT_CAPACITY * MAX_PREMISES` in every frontend process.

## 11. File Splitting Is a Consequence, Not the Primary Reduction

Current large files are:

```text
ast.c       approximately 30,294 lines
typing.c    approximately 19,427 lines
term.c       approximately 9,494 lines
hott.c       approximately 5,280 lines
```

Splitting these files first changes navigation but not semantic surface or line
count. Split only after the ID and arena boundaries above are stable.

Suggested responsibility extraction:

```text
ast.c
  Operation graph and source lowering

operation_runtime.c
  runtime frames, requests, resumptions

artifact_graph.c
  mark, write, read, append, relocate

typing_graph.c
  Proposition, Claim, Derivation arenas

typing_solver.c
  constraints and fixed point

typing_validate.c
  independent accepted-proof replay

hott_plan.c
  goals, work, candidates, residuals

hott_action.c
  Context/Substitution/Type/Term action execution

hott_validate.c
  independent certificate replay
```

A split is complete only when dependency direction is clear and old helpers are
deleted from the source file.

## 12. Lower-Priority Generic Infrastructure

Context, Substitution, Proposition, Claim, Derivation, and HOTT requests each
implement collision-safe hash-cons or index rebuild mechanics. A small typed
index utility could share:

```text
bucket clearing
chain insertion
probe accounting
bounds-checked iteration
```

It must not own semantic equality or use a generic byte comparison. Padding,
pointers, cached fields, and mutable state make `memcmp` invalid as semantic
identity. This change is lower priority because macro/callback machinery can
easily become longer and less reviewable than the repeated loops.

Adopt it only after measurements show a net reduction and each store retains a
typed exact-key comparator.

## 13. Reviewed Migration Order

### V3-PC1. Normalize persistent Proposition references

- Intern accepted scoped premises without accepting Claims.
- Replace accepted scoped tuples with discriminated Proposition/Claim ID
  edges.
- Delete cached Claim Proposition pointers.
- Preserve the v69 tuple only through one adapter.

Exit condition: one physical Proposition tuple for every persistent accepted
identity; accepted edges and Claims use checked IDs.

The executable plan is
`2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`.

### V2-A1. Complete the v70 artifact break

- Freeze the PC1 accepted graph.
- Serialize Proposition, Claim, Derivation, and premise edges directly.
- Relocate IDs exactly once.
- Delete v69 tuple expansion/reconstruction.
- Retain deterministic round-trip and forgery tests.

Exit condition: no expanded Proposition tuple compatibility path remains in
the kernel reader or writer.

### Post-A1 PC2. Move solver operands to arenas

- Add constraint operand arena and transactional builder.
- Migrate computation-fold and operation-request constraints.
- Migrate temporary proof premise arrays to slice builders.
- Retain maximum arity as an append-time limit only.
- Measure workspace size and allocation counts.

Exit condition: no solver constraint reserves maximum premise payload in every
record.

### Post-A1 PC3. Refactor runtime environments

- Extract runtime implementation from `ast.c`.
- Add frame/slice environment storage.
- Replace whole-environment copies.
- Add simultaneous instantiation.
- Preserve resumption multiplicity and capture behavior.

Exit condition: environment extension is constant-size and static Context
semantics are unchanged.

### Post-A1 PC4. Centralize proof metadata

- Add declarative proof rule descriptors.
- Centralize name, arity, conclusion kind, and semantic-action metadata.
- Add exhaustive coverage tests.
- Keep rule-specific validators and construction paths independent.

Exit condition: adding a proof kind cannot silently omit metadata, while
validator replay remains explicit.

### Post-A1 PC5. Introduce passive Term child views

- Define child roles for admitted Term tags.
- Migrate artifact marking, debug traversal, and structural statistics.
- Compare switch and line counts before expanding usage.
- Do not migrate binder-sensitive conversion merely for consistency.

Exit condition: passive graph traversals share one structural enumeration and
all binder-sensitive tests remain unchanged.

### Post-A1 PC6. Consolidate session storage and split files

- Add one owning session-storage initializer.
- Migrate read-file and REPL fixed arrays.
- Preserve small caller-owned test stores.
- Split runtime, artifact, solver, graph, and validator modules along stable
  dependency boundaries.

Exit condition: capacity policy has one owner and file splits remove old code
rather than duplicate it.

## 14. Validation Requirements

### 14.1 Proposition and proof graph

```text
local and accepted Proposition namespaces cannot alias
scoped Proposition does not create a Claim
one Claim retains multiple Derivations
ordered premise identity is preserved
candidate rollback leaves no accepted edge
commit remaps every local Proposition reference
hash collision does not merge distinct Propositions
artifact relocation does not preserve stale pointers
```

### 14.2 Constraint arenas

```text
zero-, one-, two-, and maximum-arity constraints
failed solver path rewinds operand slice
accepted proof never points into mutable constraint storage
fold clause ordering is preserved
capacity exhaustion returns residual/error deterministically
workspace bytes are materially lower
```

### 14.3 Artifact v70

```text
deterministic bytes
strict schema and fingerprint
direct Proposition/Claim/Derivation round-trip
append and link relocation
scoped premise round-trip without Claim publication
forged cross-arena and forward Claim IDs rejected
no v69 reader fallback
```

### 14.4 Runtime

```text
shadowing lookup
closure capture
handler capture
single- and multi-shot resumption behavior
simultaneous instantiation agrees with sequential reference implementation
no static Context mutation
future consumption-state extension remains possible
```

### 14.5 Regression

- all prototype scripts;
- examples 01-07 and 09;
- optimized `-Werror`;
- ASan and UBSan;
- CwF identity, composition, projection, pullback, and reindex laws;
- HOTT action and forged-certificate tests;
- deterministic artifact round-trip; and
- `git diff --check`.

## 15. Compaction Metrics

Do not use net line count alone. Record after every phase:

```text
production added/deleted/net lines
test added/deleted/net lines
bytes per Proposition-related record
bytes per solver constraint
reserved workspace bytes
number of copied Proposition tuple layouts
number of maximum-premise embedded arrays
number of maximum-premise temporary arrays
number of artifact compatibility paths
number of proof-kind exhaustive metadata switches
largest function spans
runtime environment bytes copied per extension
```

Every phase includes a deletion manifest. A new abstraction is not complete
while the old path remains available.

Independent validators and malformed-input tests are not accidental
duplication. Count them separately from constructor and fixture code.

## 16. Stop Conditions

Stop and revise the compaction if it requires:

- identifying Proposition existence with Claim acceptance;
- sharing mutable candidate edges with accepted Derivations;
- merging delta and accepted IDs without an explicit namespace/remap;
- retaining a cached pointer as a second source of identity;
- reconstructing proof authority from insertion order;
- serializing raw process pointers;
- keeping v69 and v70 accepted readers in parallel;
- replacing typed equality with `memcmp`;
- merging Runtime Environment with static Context;
- treating structural projection as resource discard permission;
- making constructor and validator share unchecked acceptance code;
- replacing binder-sensitive Term algorithms with an untyped generic walker;
  or
- deleting forgery, replay, sanitizer, or deterministic artifact tests to
  improve line-count measurements.

## 17. Codebase Adjudication

| Finding | Validity | Pre-A1 decision |
| --- | --- | --- |
| P1 Proposition copies | valid, but original scope was too broad | accepted scoped tuples and Claim pointers form V3-PC1; candidate/delta snapshots are deferred |
| P2 fixed solver operands | valid memory and maintenance debt | defer; solver constraints are compiler-local and absent from v70 |
| P3 v69 expansion | valid temporary adapter | implement as V2-A1 after PC1 |
| P4 runtime copies | valid runtime debt | defer; independent of static accepted proof publication |
| P5 proof metadata | valid only for declarative common facts | defer; A1 adds coverage tests while rule-specific validators remain explicit |
| P6 Term traversal | valid only for passive child enumeration | defer; no binder-sensitive algorithm is generalized |
| P7 entrypoint storage | valid ownership/capacity debt | defer; it does not alter accepted graph semantics |

The resulting dependency order is:

```text
V3-SC1 -> V3-PC1 -> V2-A1 -> post-A1 physical compaction
```

## 18. Expected Result

After V3-PC1 and V2-A1, the persistent accepted proof system has one physical
Proposition identity, compact ID edges from Claims and Derivations, and a direct
v70 artifact representation. Compiler-local candidate and solver snapshots
remain outside that wire contract.

After the post-A1 stages, solver memory should scale with actual premise count,
runtime environment extension should no longer copy 512 binding slots, passive
Term graph traversal should have one structural child view, and entry points
should obtain initialized stores from one owning session layer.

The intended final shape is:

```text
immutable semantic identities
  Context ID
  Substitution ID
  Term ID
  Proposition ID
  Claim ID
  Derivation ID

compact edges
  typed IDs and arena slices

mutable compiler state
  delta references and constraint operand slices

runtime state
  persistent/stack frames and explicit capture state

independent evidence replay
  typed validators over immutable IDs
```

This reduces physical duplication without erasing the categorical,
proof-theoretic, observational, or future resource-sensitive distinctions on
which the current architecture depends.
