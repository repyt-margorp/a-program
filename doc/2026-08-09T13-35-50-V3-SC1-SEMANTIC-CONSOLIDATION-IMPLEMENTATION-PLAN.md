# V3-SC1 Semantic Consolidation Before Object-HOTT Publication

Date: 2026-08-09

Status: complete; verified against baseline `31e5446`

Repository baseline:

- branch: `main`;
- commit: `31e5446` (`Implement first observational action fragment`);
- artifact format: v69;
- compiler-local HOTT manifest: `src/prototype/hott_fragment_v2.schema`;
- V2-O1: complete locally;
- V2-A1: blocked on this consolidation gate.

Source audit:

- `2026-08-09T13-23-19-SEMANTIC-CONSOLIDATION-AND-RESOURCE-READY-REINDEX-PLAN.md`;
- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`;
- `2026-08-08T22-51-04-CONTEXT-SUBSTITUTION-JUDGEMENT-GRAPH-CONSING-V3-G1-PLAN.md`;
- `2026-08-09T07-10-00-V2-O1-OBSERVATIONAL-ACTION-IMPLEMENTATION-PLAN.md`; and
- the current `src/prototype/` implementation at the baseline above.

This plan is the required implementation stage between V2-O1 and V2-A1. It
does not seek a cosmetic line-count reduction. It removes duplicate semantic
authorities and repeated storage/validation paths before the artifact boundary
makes them permanent.

## 1. Objective

Reduce the implementation and proof-state surface while preserving the sorts
which are mathematically different:

```text
Context                      CwF object
Substitution Delta -> Gamma  CwF morphism
Term/Operation occurrence    syntax and typed occurrence
Judgement proposition        proposition identity
Claim                        accepted proposition
Derivation                   rule application proving a Claim
HOTT request/result          compiler action state
Runtime environment          dynamic realization, not a Context
```

The consolidation target is not one universal node arena. It is one
authoritative implementation path for each mathematical operation:

```text
typed substitution action
  -> structural reindex
  -> proposition reindex
  -> weakening specialization
  -> HOTT endpoint/naturality construction

typed proposition identity
  -> solver candidate
  -> accepted Claim
  -> ordered Derivation premise edge

CwF object/morphism formation evidence
  -> one typed certificate store
  -> feature-specific validators over that store
```

V2-A1 starts only after these paths are stable and measured.

## 2. Baseline Size and Duplication Evidence

The baseline production and principal test files contain:

| File | Lines | Relevant responsibility |
| --- | ---: | --- |
| `src/prototype/ast.c` | 30,067 | AST lowering, runtime, artifact wire, linking |
| `src/prototype/typing.c` | 19,274 | solver, proof construction, proof replay |
| `src/prototype/term.c` | 9,494 | shared Term graph, equality views, reduction |
| `src/prototype/hott.c` | 5,454 | planning, action execution, certificates, replay |
| `src/prototype/context.c` | 1,604 | Context/Substitution graph and reindex |
| `src/prototype/judgement.h` | 1,382 | solver and accepted proof representations |
| `src/prototype/hott.h` | 729 | HOTT records and long store-bundle APIs |
| `src/prototype/context.h` | 335 | CwF graph declarations |
| `src/prototype/hott_goal_check.c` | 3,721 | HOTT fixtures and adversarial tests |

These numbers are measurements, not proof that a file is wrong. The concrete
duplication is below.

### 2.1 Premise identity is still copied into parallel arrays

`src/prototype/judgement.h:92-131` gives every solver-local Derivation
candidate seven arrays of the maximum computation-fold arity. The accepted
Derivation at `src/prototype/judgement.h:178-202` gives every proof five more
arrays. Most rules have zero, one, or two premises.

The same fields are copied, hashed, replayed, written, read, relocated, and
validated separately. The current implementation contains 726 references to
the maximum-premise constant or its parallel premise fields in `typing.c`.

G1 did correctly make accepted Claim IDs direct DAG edges and retained
multiple Derivations for one Claim. It did not physically implement the
interned proposition/premise-edge representation described by V3 section
26.7. V3-SC1 completes that migration.

### 2.2 Candidate and accepted proposition tuples are separate layouts

`prototype_judgement_claim_candidate` and `prototype_judgement_claim` repeat:

```text
kind
authority kind/id
Context id
Operation id
subject
classifier
```

Candidate status and acceptance must remain distinct, but proposition identity
is the same mathematical object. Repeating its physical layout forces every
solver-to-accepted transition to copy and compare the tuple again.

### 2.3 Base-CwF formation certificates are feature-owned

`prototype_substitution_certificate_db` is declared in `judgement.h` and
implemented in `typing.c`. The structurally identical Context-formation pair
is declared in `hott.h` and implemented in `hott.c`.

Both express a typed edge from a CwF structural object to an accepted Claim:

```text
ContextFormation(context_id, classifier_is_type_claim_id)
SubstitutionFormation(substitution_id, assigned_term_has_type_claim_id)
```

They must retain different certificate kinds and validators, but allocation,
storage, lookup, bounds checks, and store ownership need not be duplicated or
owned by HOTT.

### 2.4 Reindex has one structural implementation but repeated proof wrappers

`prototype_term_reindex()` is already the shared CwF structural action and is
memoized by `(term, substitution)`. `CONTEXT_WEAKEN` already names the exact
projection/composition Substitution. This part of G1 is correct.

The remaining duplication is in wrappers such as:

- `prototype_judgement_add_context_weakened_claim()`;
- `prototype_judgement_add_reindexed_claim()`;
- `validate_context_weaken_proof()`;
- `validate_substitution_reindex_proof()`; and
- HOTT endpoint and naturality setup.

Each path independently assembles or checks source/target Context, premise
tuple, result subject/classifier, and semantic-action fields.

V3-SC1 does **not** add a persistent `ReindexAction` node merely to wrap the
existing Substitution. The exact Substitution plus the deterministic Term
reindex operation is already sufficient structural authority. Consolidation
uses one typed replay function and compact rule-specific checks.

### 2.5 HOTT APIs repeatedly pass the same store bundle

Public and internal HOTT calls repeatedly pass Context, Substitution, Context
certificate, Substitution certificate, bridge, Term, type-declaration,
Operation, and Judgement stores. The pair of certificate-store names alone
appears 222 times in `hott.h` and `hott.c`.

This is not only syntactic noise. A caller can accidentally combine stores
from different compile sessions. A checked view should bundle one coherent
read-only kernel world; a separate builder should expose mutable stores.

### 2.6 Deterministic outcome metadata is repeated

`prototype_hott_work_item` and `prototype_hott_action_result` both carry
state, residual reason, normalization profile, step limit, graph revision, and
calculus fingerprint. Work additionally owns used steps and source location;
an action result owns request/certificate identity.

The identities must remain distinct. Their deterministic execution outcome can
be one embedded typed record with shared state validation.

### 2.7 Runtime copying is real but outside this gate

`operation_runtime_extend_environment()` copies a 512-entry environment on
every extension, and captured resumptions copy environments again. This should
be replaced by persistent frames or stack save/restore, but it is dynamic
runtime work and has no bearing on object-HOTT artifact authority.

It is explicitly deferred. Folding it into SC1 would increase semantic risk
without simplifying V2-A1.

## 3. Non-Negotiable Semantic Decisions

### 3.1 No universal graph and no tag merger by resemblance

Context, Substitution, proposition, Claim, Derivation, Term, Operation, and
runtime values remain separate sorts. Shared allocation mechanics do not imply
shared identity.

### 3.2 One proposition identity, separate lifecycle state

Introduce an immutable interned proposition:

```text
JudgementProposition(
  kind,
  authority_kind,
  authority_id,
  context_id,
  operation_id,
  subject,
  classifier
)
```

A solver candidate refers to a proposition ID. An accepted Claim also refers
to that proposition ID and adds closure/publication state. Acceptance does not
mutate the proposition and does not choose one privileged Derivation.

### 3.3 Ordered premise edges, not maximum-sized embedded arrays

Use an edge arena:

```text
DerivationPremiseEdge(
  owner_derivation_or_candidate,
  ordinal,
  proposition_id,
  accepted_claim_id_or_invalid
)
```

An accepted Claim ID means an accepted DAG dependency. An invalid Claim ID is
a scoped rule-local proposition and must not be promoted to an accepted Claim.
The ordinal remains part of rule identity.

Candidate and accepted Derivations may use separate edge arenas if rollback
semantics require it, but both use the same proposition identity and iterator
contract. Mutable candidate edges never become accepted by pointer aliasing;
commit copies/interns the ordered edge slice.

### 3.4 One CwF certificate store, typed payloads

Use a narrowly scoped CwF certificate arena, not a generic all-feature
`AuthorityEdge` database:

```text
CwfCertificate
  kind = CONTEXT_FORMATION | SUBSTITUTION_FORMATION
  structural_id
  claim_id
```

Kind-specific validators remain separate and check the appropriate structural
store and Claim shape. HOTT bridge/action certificates remain HOTT records;
they refer to CwF certificate IDs rather than owning parallel stores.

### 3.5 Reindex authority remains Substitution-based

For `sigma : Delta -> Gamma`, the authoritative structural data is the exact
Substitution DAG. Reindexing a proposition computes both subject and
classifier through `prototype_term_reindex()` and constructs the target
proposition under `Delta`.

`CONTEXT_WEAKEN` is a specialization requiring that `sigma` is a projection
path. `SUBSTITUTION_REINDEX` is the general rule. Both use the same replay
primitive; only the projection predicate differs.

### 3.6 Checked views are capability bundles, not global state

Introduce:

```c
struct prototype_kernel_view;
struct prototype_kernel_builder;
```

The view groups coherent read-only stores. The builder provides explicitly
mutable stores required by constructors. Neither is serialized or used as
semantic identity. Low-level Context/Term tests may continue to call narrow
APIs directly.

### 3.7 Constructor and validator remain independently reviewable

They may share declarative descriptors, exact graph lookup, and deterministic
structural actions. They must not share an unchecked function which both
constructs and accepts a certificate. A validator follows stored IDs and
replays the rule from immutable inputs.

### 3.8 Resource readiness does not add resource typing

Usage remains a future judgement component:

```text
Gamma ; U |- operation : A
```

`U` is not part of Context, Term, Substitution, proposition, or reindex-cache
identity. A projection is a structural morphism, not permission to discard a
linear value. SC1 only preserves the extension point.

## 4. Artifact Boundary Strategy

SC1 must not cause two permanent artifact migrations immediately before A1.

The in-memory proposition and premise-edge representation is serialized
through the existing v69 expanded tuple format during SC1. This is an ordinary
wire adapter, not a parallel in-memory compatibility path:

```text
v69 read tuple -> intern proposition -> append premise edge
premise edge -> expand proposition tuple -> v69 write tuple
```

Only one in-memory representation exists after migration. V2-A1 then performs
the planned single v70 break and writes the compact proposition/premise graph
directly together with object-HOTT Terms, proof kinds, and observation roots.

The temporary v69 adapter is deleted in A1, not retained as a compatibility
reader. The A1 plan owns that deletion gate.

## 5. Implementation Phases

### SC1.0 Baseline, counters, and scoped-premise audit

Status: complete

- [x] Record the exact baseline commit, compiler, build flags, and line counts.
- [x] Record `git diff --numstat` separately for production, tests, and docs.
- [x] Add counters for proposition, premise-edge, CwF-certificate, and HOTT
      outcome allocation/reuse.
- [x] Run all 16 prototype scripts.
- [x] Run examples 01-07 and 09.
- [x] Run optimized `-Werror` and ASan/UBSan baselines. The optimized build
      exposed and fixed a parser binder-stack restoration bug in `reader.c`.
- [x] Add a focused v69 scoped-premise tuple round-trip fixture.
- [x] Audit the alleged derivation scoped-premise readback destination bug.
      The current baseline already reads context, subject, and classifier into
      distinct fields; no such code defect is present at `31e5446`.
- [x] Add field-by-field forged scoped-premise tests. Kind, Context, subject,
      and classifier are independently forged and rejected.

Exit gate: the baseline is reproducible and every premise tuple field is known
to round-trip correctly.

### SC1.1 Introduce checked kernel views

Status: complete

- [x] Define a read-only `prototype_kernel_view` in a dependency-neutral
      header.
- [x] Define a separate mutable `prototype_kernel_builder`.
- [x] Validate non-null stores and cross-store CwF/Claim coherence when
      constructing a view.
- [x] Convert HOTT validation entry points first.
- [x] Convert HOTT action construction entry points second.
- [x] Keep narrow low-level APIs for Context, Term, and isolated unit tests.
- [x] Delete repeated store-bundle null checks only when equivalent validation
      is owned by view construction.
- [x] Reject a deliberately mixed-store view in tests.

Exit gate: HOTT public APIs no longer expose the repeated eight-to-ten pointer
bundle and mixed stores cannot reach a validator.

### SC1.2 Consolidate base-CwF certificates

Status: complete

- [x] Add `cwf_certificate.h` and `cwf_certificate.c` under
      `src/prototype/`.
- [x] Move Context-formation certificate declarations out of `hott.h`.
- [x] Move Substitution-formation certificate declarations out of
      `judgement.h`.
- [x] Store both in one tagged CwF certificate arena.
- [x] Retain separate Context and Substitution validators.
- [x] Update HOTT bridge and action payloads to refer to typed CwF certificate
      IDs.
- [x] Add cross-kind, duplicate, and wrong-Claim tests. CwF certificates use a
      linear exact-key arena rather than a hash index, so a hash-collision test
      is not applicable to this representation; mixed-store rejection remains
      part of SC1.1 checked-view testing.
- [x] Delete the two old DB initializers, allocation loops, and lookup paths.

Exit gate: no HOTT-owned Context certificate store and no separate
Substitution certificate allocation infrastructure remain.

### SC1.3 Consolidate typed proposition reindex replay

Status: complete

- [x] Add one helper that derives a target proposition identity from a source
      proposition and exact Substitution.
- [x] Make it reindex both subject and classifier and validate Context
      orientation.
- [x] Route general `SUBSTITUTION_REINDEX` construction and replay through the
      helper.
- [x] Route `CONTEXT_WEAKEN` through the same helper plus the projection-path
      predicate.
- [x] Route HOTT endpoint proposition construction through the same API.
- [x] Keep naturality conversion as a separate equality check over the
      canonically constructed square sides.
- [x] Delete duplicate accepted-premise/result tuple assembly from the two
      public construction paths.
- [x] Do not add a persistent `ReindexAction` wrapper node.

Exit gate: source/target orientation and result reconstruction have one
structural implementation path; weakening remains a distinct proof rule.

### SC1.4 Intern proposition identity and migrate premise edges

Status: complete

- [x] Introduce the immutable `prototype_judgement_proposition` table and
      collision-safe index.
- [x] Replace duplicated candidate/accepted proposition tuples with
      proposition IDs.
- [x] Preserve accepted Claim identity and allow multiple Derivations per
      Claim.
- [x] Add candidate and accepted ordered premise-edge arenas.
- [x] Replace parallel fixed arrays in Derivation candidates.
- [x] Replace parallel fixed arrays in accepted Derivations.
- [x] Move constructor/IH parameters into a compact tagged rule-data payload
      or union; do not make ordinary rules reserve HOTT/Match storage.
- [x] Update derivation hash keys, collision handling, closure-rank calculation,
      rollback, and validation.
- [x] Update all proof constructors and validators to use premise iterators.
- [x] Keep scoped propositions unaccepted unless an independent Claim is
      explicitly published.
- [x] Implement the v69 expanded-tuple read/write adapter over the edge graph.
- [x] Add zero-, one-, two-, maximum-arity, shared-proposition,
      multiple-Derivation, reordering, cycle, and forged-edge tests.
- [x] Delete all accepted and candidate maximum-premise arrays.

Exit gate: one proposition identity feeds both solver and accepted proof
graphs; no fixed maximum-premise array remains in Derivation identity.

### SC1.5 Consolidate deterministic HOTT outcome plumbing

Status: complete

- [x] Add a typed `prototype_hott_deterministic_outcome` embedded by work and
      action results.
- [x] Centralize state, residual reason, normalization profile, budget,
      revision, and fingerprint validation.
- [x] Preserve work-only source/steps metadata and action-only
      request/certificate identity outside the common outcome.
- [x] Share residual and READY publication checks without merging Goal, Work,
      Request, Result, or Certificate identity.
- [x] Verify exhaustion remains residual and never publishes equality.
- [x] Delete repeated outcome validation/publication branches.

Exit gate: execution metadata has one representation while semantic request
and result sorts remain separate.

### SC1.6 Descriptor-directed HOTT scaffolding

Status: complete

- [x] Extend the existing type-former descriptor only with immutable action
      capabilities and child-role metadata actually shared by admitted forms.
- [x] Share child request interning, result propagation, exact Claim lookup,
      premise-edge construction, and endpoint startup.
- [x] Keep ADT, Pi, pure `Comp`, `Thunk`, Match, and IH formation/witness rules
      as independently replayed typed handlers.
- [x] Require every admitted type former to state Type-action, Term-action,
      ordinary-reindex, purity, resource-hook, and artifact status.
- [x] Audit repeated switch scaffolding after coverage tests prove every
      admitted former is represented. Rule-specific typed handlers remain
      intentionally separate; only shared recursion plumbing was consolidated.
- [x] Keep the descriptor declarative; it contains capability status and child
      roles, not imperative callbacks that construct and validate the same
      evidence.

Exit gate: adding one admitted former cannot omit a semantic action silently,
and shared recursion mechanics occur once.

### SC1.7 Consolidate successful test fixtures

Status: complete

- [x] Use checked builders for Contexts, Substitutions, propositions, Claims,
      premise edges, CwF certificates, bridges, and HOTT action requests.
- [x] Convert repeated successful setup in `hott_goal_check.c`.
- [x] Keep malformed raw-record tests explicit and separate.
- [x] Ensure builders cannot be used by artifact forgery tests.
- [x] Report fixture LOC reduction separately from production LOC reduction.

Exit gate: successful fixtures are shorter without reducing malformed-record
coverage or making constructor and validator share unchecked setup.

### SC1.8 Exit audit and A1 handoff

Status: complete

- [x] Run all prototype scripts and examples 01-07/09.
- [x] Run optimized `-Werror`, ASan, and UBSan builds.
- [x] Verify deterministic v69 bytes through the temporary edge adapter.
- [x] Verify HOTT fragment fingerprint remains unchanged unless the admitted
      calculus itself changed.
- [x] Re-run CwF identity/composition/pullback and reindex laws.
- [x] Re-run all O1 Type/Term action and forged-certificate tests.
- [x] Produce the final file-by-file deletion report defined in section 8.
- [x] Update V3 and the A1 plan with actual completion evidence.
- [x] Freeze the compact in-memory schema consumed by A1/v70.

Exit gate: no old in-memory path remains, all tests pass, actual code-size and
authority-path reductions are recorded, and V2-A1 is unblocked.

## 6. Required File Changes

| File | Planned responsibility change |
| --- | --- |
| `src/prototype/context.[ch]` | retain Context/Substitution graph and structural Term reindex; expose narrow proposition-reindex support inputs |
| `src/prototype/cwf_certificate.[ch]` | new typed Context/Substitution formation certificate arena and validators |
| `src/prototype/judgement.h` | proposition IDs, compact Claim/Derivation records, premise-edge and rule-data declarations |
| `src/prototype/typing.c` | proposition interning, premise-edge commit/replay, shared reindex rule implementation; delete parallel-array paths |
| `src/prototype/hott.h` | checked views/outcome records; remove base-CwF certificate ownership and long store bundles |
| `src/prototype/hott.c` | use checked views, CwF certificates, shared reindex and outcome helpers, descriptor scaffolding |
| `src/prototype/ast.c` | v69 proposition/premise adapter only; remove direct fixed-array assumptions; fix scoped tuple readback |
| `src/prototype/hott_goal_check.c` | checked successful fixture builders; retain explicit raw forgeries |
| `src/prototype/artifact_v69.schema` | no semantic change during SC1 |
| `doc/...V2-A1...md` | consume compact graph and delete v69 adapter during v70 migration |

Do not move accepted implementation files outside `src/prototype/` in this
stage.

## 7. Deferred Work

The following findings from the source audit are real but are not SC1 entry
requirements for object-HOTT publication:

- persistent/stack runtime environments instead of 512-entry copies;
- resource grades, usage vectors, linear weakening, contraction, and
  resumption multiplicity proofs;
- splitting `ast.c`, `typing.c`, or `hott.c` merely by line count;
- object equality surface syntax, transport, rewrite, or equality reflection;
- Universe observational action and univalence; and
- effect-handler contextual or observational equality.

Each requires a separate semantic plan. SC1 only ensures that future usage
evidence can refer to Binding IDs, Substitutions, Claims, and Derivations
without changing their identity.

## 8. Mandatory Code-Size Accounting

Code-size reduction is a deliverable, but raw net LOC is not the sole
acceptance criterion. Every phase records both physical and semantic surface
changes.

### 8.1 Commands and stored evidence

At SC1.0 and after every phase, record:

```sh
wc -l src/prototype/*.[ch]
git diff --numstat <SC1-baseline> -- src/prototype
git diff --stat <SC1-baseline> -- src/prototype
rg -n 'PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES' src/prototype
rg -n 'context_certificates|substitution_certificates' src/prototype/hott.[ch]
```

The final report must not count documentation deletion as implementation
reduction. Generated files and build outputs are excluded.

Keep one immutable measurement snapshot after every completed phase. Each
snapshot records the phase-end commit or worktree label and the output of:

```sh
git diff --numstat 31e5446 -- src/prototype
wc -l src/prototype/*.[ch]
```

The final report derives per-file `Added` and `Deleted` from `numstat`; it must
not infer them from `Final lines - Baseline lines`. Files created to replace
duplicated implementations are listed explicitly, so moving code into a new
module cannot be reported as deletion. Test/fixture reductions and production
reductions are subtotalled separately. For every phase, also record which old
authoritative functions, structs, arrays, or call signatures ceased to exist.
This gives both physical deletion and semantic-path deletion evidence.

### 8.2 Final per-file table

SC1.8 must replace `pending` with measured values:

| File | Baseline lines | Added | Deleted | Final lines | Net | Deleted semantic path |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `ast.c` | 30,067 | 565 | 338 | 30,294 | +227 | internal fixed-array wire assumptions; v69 adapter remains only at boundary |
| `typing.c` | 19,274 | 1,447 | 1,294 | 19,427 | +153 | tuple-column copy/hash/replay and duplicate reindex wrappers |
| `term.c` | 9,494 | 0 | 0 | 9,494 | 0 | no semantic path moved |
| `context.c` | 1,604 | 0 | 0 | 1,604 | 0 | no semantic path moved |
| `judgement.h` | 1,382 | 87 | 97 | 1,372 | -10 | two proposition layouts and twelve embedded premise arrays |
| `hott.c` | 5,454 | 757 | 931 | 5,280 | -174 | feature-owned CwF stores, repeated outcome/view/reindex plumbing |
| `hott.h` | 729 | 66 | 169 | 626 | -103 | feature-owned CwF declarations and repeated store-bundle parameters |
| `read_file.c` | 4,755 | 86 | 45 | 4,796 | +41 | Claim tuple assumptions at artifact grounding |
| `reader.c` | 2,935 | 45 | 35 | 2,945 | +10 | rule-data column relocation |
| `repl.c` | 1,195 | 12 | 2 | 1,205 | +10 | old JudgementDB storage initialization |
| `universe.c` | 1,037 | 60 | 60 | 1,037 | 0 | direct accepted-Claim tuple access |
| `cwf_certificate.c` | 0 | 261 | 0 | 261 | +261 | one typed replacement for two certificate stores |
| `cwf_certificate.h` | 0 | 90 | 0 | 90 | +90 | typed CwF certificate contract |
| `kernel_view.c` | 0 | 58 | 0 | 58 | +58 | centralized checked kernel view construction |
| `kernel_view.h` | 0 | 46 | 0 | 46 | +46 | read-only view / mutable builder split |
| **production subtotal** | **77,926** | **3,580** | **2,971** | **78,535** | **+609** | four explicit replacement modules included |

Test and fixture accounting is separate:

| File | Baseline lines | Added | Deleted | Final lines | Net | Coverage change |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cbpv_boundary_check.c` | 358 | 20 | 3 | 375 | +17 | compact premise arenas |
| `context_category_check.c` | 1,038 | 99 | 4 | 1,133 | +95 | collision, ordering, multi-Derivation and 0/1/2/max arity |
| `hott_goal_check.c` | 3,721 | 460 | 638 | 3,543 | -178 | checked successful fixture construction; raw forgeries retained |
| `test_artifact_flow.sh` | 2,863 | 34 | 0 | 2,897 | +34 | scoped tuple round-trip and four field forgeries |
| `test_hott_goal.sh` | 41 | 2 | 0 | 43 | +2 | new replacement modules linked |
| **test/fixture subtotal** | **8,021** | **615** | **645** | **7,991** | **-30** | malformed coverage increased |

The total measured source change is `+4,195/-3,616`, net `+579` lines. The
production net increase is intentional replacement infrastructure, not hidden
movement: all 455 lines in the four new modules are shown explicitly.

### 8.3 Semantic surface table

SC1.8 must also report:

| Metric | Baseline | Final | Required direction |
| --- | ---: | ---: | --- |
| fixed premise arrays in candidate/accepted Derivations | 12 parallel arrays | 0 | 0 |
| separate proposition tuple layouts | 2 | 1 | 1 |
| base-CwF certificate DB implementations | 2 | 1 typed arena | 1 typed arena |
| HOTT Context/Substitution certificate parameter mentions | 407 | 128 | materially lower |
| persistent reindex authority node kinds | 0 | 0 | remain 0 |
| proof rules for weakening/general reindex | 2 | 2 | remain 2 |
| structural reindex result implementations | 1 plus wrappers | 1 shared proposition helper | 1 |
| accepted Derivations permitted per Claim | multiple | multiple | unchanged |
| runtime environment implementations touched | 0 | 0 | unchanged |

### 8.4 Interpretation rule

A phase does not pass merely because net LOC decreases. It must delete the old
authoritative path after callers migrate. Conversely, independent validator
logic and explicit malformed-input tests are not accidental duplication and
must not be removed to improve the number.

Expected ranges may be recorded during implementation, but only actual
`numstat` and final line counts are completion evidence.

## 9. Validation Matrix

### 9.1 Proposition and proof graph

- [x] proposition hash collision does not merge distinct tuples;
- [x] candidate and accepted state share identity but not mutability;
- [x] one Claim retains multiple accepted Derivations;
- [x] scoped premise remains local;
- [x] ordered edge reordering changes Derivation identity;
- [x] cycle and forward accepted-Claim edges are rejected;
- [x] zero-, one-, two-, and maximum-arity rules replay; and
- [x] v69 expanded tuples round-trip to the same proposition/edge graph.

### 9.2 CwF authority and reindex

- [x] Context and Substitution certificate kinds cannot be confused;
- [x] one- and multi-level weakening name exact projection paths;
- [x] general reindex checks source/target orientation;
- [x] reindex identity and composition laws pass;
- [x] unchanged Terms retain Term IDs;
- [x] HOTT endpoints use the same structural action; and
- [x] projection existence creates no resource-discard certificate.

### 9.3 HOTT action

- [x] empty and non-empty bridges;
- [x] Context, Substitution, Type, and Term actions;
- [x] ADT, Pi, pure `Comp`, `Thunk`, Match, and IH cases;
- [x] host primitive and Universe unsupported results;
- [x] effectful and conversion-budget residuals;
- [x] forged certificate/action cross-links rejected; and
- [x] successful observation never extends DefEq.

### 9.4 Build and artifact

- [x] all 16 prototype scripts;
- [x] examples 01-07 and 09;
- [x] optimized `-Werror`;
- [x] ASan/UBSan;
- [x] deterministic v69 bytes;
- [x] artifact append/link relocation;
- [x] schema and calculus fingerprints; and
- [x] `git diff --check`.

## 10. Progress Sheet

| Phase | Status | Dependency | Completion evidence |
| --- | --- | --- | --- |
| SC1.0 Baseline/scoped premise audit | complete | O1 | 16 scripts, examples, optimized `-Werror`, sanitizers, scoped tuple round-trip/forgeries |
| SC1.1 Checked views | complete | SC1.0 | HOTT APIs use checked view/builder capabilities; mixed Judgement store rejected |
| SC1.2 CwF certificates | complete | SC1.1 | one typed arena; duplicate, cross-kind, and wrong-Claim rejection |
| SC1.3 Reindex replay | complete | SC1.2 | one accepted reconstruction/replay path; exact projection specialization |
| SC1.4 Proposition/premise graph | complete | SC1.3 | no embedded fixed arrays, v69 adapter, DAG/arity/forgery tests |
| SC1.5 HOTT outcome | complete | SC1.1 | one outcome representation and validator; semantic identities remain distinct |
| SC1.6 Descriptor scaffolding | complete | SC1.3, SC1.5 | declarative capabilities/child roles; admitted-form coverage and replay |
| SC1.7 Fixture builders | complete | SC1.2-SC1.6 | fixture file -178 net lines; raw forgeries retained |
| SC1.8 Exit/A1 handoff | complete | SC1.7 | full suite and measured file-by-file report |

At most one phase is marked `in progress`. Update this table and the per-file
measurement table after every completed phase, not only at the end.

## 11. Recommended Execution Order

```text
V2-O1
  -> SC1.0
  -> SC1.1
  -> SC1.2
  -> SC1.3
  -> SC1.4
  -> SC1.5
  -> SC1.6
  -> SC1.7
  -> SC1.8
  -> V2-A1/v70
```

SC1.5 may begin after SC1.1, but it should merge only after SC1.4 has frozen
the proposition/premise APIs used by action certificates. A1 must not begin
writing observation roots against the old embedded premise layout.

## 12. Stop Conditions

Stop and revise SC1 if implementation requires:

- a universal untyped graph;
- identifying Context, Substitution, Term, Operation, proposition, or Claim
  IDs;
- one privileged proof per Claim;
- a persistent wrapper node around every deterministic reindex request;
- treating a projection as linear/affine discard permission;
- putting usage grades in Context or Term identity;
- serializing HOTT work/action history in v69;
- retaining old and new in-memory proof layouts in parallel;
- using a hash match as identity proof;
- making constructors and validators share unchecked acceptance code;
- deleting independent replay or forgery tests for LOC reduction; or
- starting runtime-environment or resource semantics changes inside SC1.

## 13. A1 Handoff Contract

V2-A1 receives:

1. one interned proposition table;
2. accepted Claims referring to proposition IDs;
3. ordered premise-edge Derivations with no fixed arrays;
4. one typed CwF certificate arena;
5. one shared proposition-reindex path;
6. checked HOTT kernel/builder views;
7. shared deterministic HOTT outcome validation; and
8. a complete actual code-size/deletion report.

A1 then:

- removes the temporary v69 expanded-tuple adapter;
- defines the compact v70 proposition and premise-edge wire records;
- adds observation former tags and O1 proof kinds;
- adds object-HOTT publication roots;
- relocates and validates the compact accepted proof closure; and
- preserves no v69 compatibility reader.

This keeps semantic consolidation before publication while performing only one
permanent artifact schema break.
