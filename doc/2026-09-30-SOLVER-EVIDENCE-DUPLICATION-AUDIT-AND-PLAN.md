# Solver and Evidence Duplication Audit

Date: 2026-09-30
Status: audit complete; SE1 direct-input migration underway, full refactor unfinished.
Parent: [artifact plan, AP0](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md#ap0-simplify-before-extending-persistence).
This is the active prerequisite work list, not another artifact format proposal.

## Problem List

1. **SE1:** an independent Job graph is assumed necessary; even accepted
   Evidence is allocated again as a completed Job.
2. **SE2:** source/rule structure queries and checked construction overlap.
3. **SE3:** some Evidence retains premise edges already present in typed data.
4. **SE4:** persistence reconstructs another rule-input graph from these layers.

## Subjective (User)

2026-09-30, English paraphrases of the latest instructions:

- Audit wrapping and Job construction, including Evidence, before changing the
  plan. Being free of recomputation does not justify duplicated data structures.
- Complete this refactoring before resuming work that would otherwise keep
  adding code around the current structures.
- Question whether Job itself is needed. Do not assume that preserving the Job
  graph and making its nodes smaller is the desired outcome.
- Keep Core computation separate from typed construction. Do not extend `.a`
  for Transpiler/Linker responsibilities.

The user has not approved a new dependency representation or a new proof format.
The design directions below are agent proposals grounded in the inspected code.

## Objective (Code)

### Revision and Method

Audited `37b66f750c6f5c8728cb1d0e837ff35335263518`: accepted `src/` plus the
artifact/readback/conversion prototypes. Fresh measurements use clean `e716232`
accepted sources plus those overlays, as previous artifact gates did.
The concurrent working-tree changes to `evidence.*`/`iadt.*` were also inspected:
they add explicit context renaming and schema relocation, not a changed Evidence
or Job representation. They are excluded from measurements and are not reverted.

The inspection covers request allocation/scheduling, source and function/CBPV
structure queries, binding/domain, conversion, context/IADT queries, Evidence
allocation/structural views, derivation import/export and source persistence.
It is not a completed proof that every premise or worker is removable.

### Findings

| ID | Inspected construction and consumer | Finding and limit |
| --- | --- | --- |
| SE1 | `synthesis_derivation.c:evidence_ready`, `pg_synthesis_evidence`; `synthesis_work.h:pg_synthesis_job` | One interned full Job per used accepted Evidence pointer. `inputs[0]` and `result` point to the same Evidence; status is immediately DONE, with no pending work. This duplicates a scheduling representation, not the witness Term. |
| SE1 | `synthesis_conversion.c:classifier_step`, `synthesis_iadt.c:inductive_instance_step`, `synthesis_context.c:reindex_step`, `synthesis_function.c:family_function_step` | A producer-keyed request can forward to another request keyed through completed Evidence adapters. Checked-input sharing is real, but the forwarding layer is not thereby proven necessary. |
| SE2 | `synthesis_function.c:lambda_structure_step`, `synthesis_cbpv.c:computation_structure_step`; `evidence.c:pg_prove_lambda`, `unary_term` | Both provisional synthesis and checked construction assemble Lambda/App/Oracle spines. Core interning shares equal pointers, so this is duplicated construction logic/dependency traversal, not necessarily duplicate Core allocations. |
| SE2 | `synthesis.c:accepted_structure`, `term_structure_step`, `classifier_structure_step`, `type_structure_step` | Checked data already supplies a direct structural path. Pending rules supply a separate path needed, among other things, to close effect equations before full acceptance. Deleting that path without replacing its information flow can deadlock inference. |
| SE2 | `synthesis.c:source_work`, `SOURCE_WORK`; `synthesis_context.c:checked_query_step`; `typed_query.[ch]` | Many roles still allocate the broad transitional source state. A checked query can also have an outer Job copying its status/result. These are state/lifetime issues to audit with the construction change, not reasons to merge Core evaluation and typing. |
| SE3 | `evidence.c:accept_record`; `typing.h:pg_occurrence`; `evidence_function.c`, `evidence_cbpv.c`, `evidence_identity.c` | Evidence already borrows its conclusion; classifier/context are not copied per term proof. Several direct rules have zero stored premises and use typed inputs. The old claim that all Evidence is a second full Term graph is incorrect. |
| SE3 | `evidence.c:prove_data_elimination` | Match/induction stores scrutinee, branches, motive and formation in typed operands, then also stores a `count + 6` Evidence-premise array. This is a concrete overlap. Scope, substitution and reduction requirements must be mapped before deleting it. |
| SE4 | `synthesis_derivation.c:pg_synthesis_export_rule_closure`, `export_header`; `source_io.c:pg_sources_write_with` | Export traverses workers, accepted proofs and raw inputs, then allocates fresh rule headers/premise arrays in scratch storage. These are temporary copies, not another live acceptance authority; their traversal/allocation and any duplicated wire content still require review. |
| SE4 | `artifact_persistence/artifact/source.c`, `derivation.c`, `source_checkpoint_test.c` | The restricted checkpoint carries owner/rule links and a shared schedule. Extending it owner by owner before simplifying the producers would preserve incidental implementation structure. Stop that expansion; existing tests remain useful evidence, not a permanent schema commitment. |

### Measurements

Fresh O2 census, on the revision above. Budget is cumulative; status refers to
the module root. These are whole live-store counts, not reachable `.a` counts.

| Input | Actual steps / status | All Jobs | Evidence-only Jobs | Adapter allocation bytes | All Job allocation bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| List-09, step 0 | 0 / pending | 40 | 1 | 104 | 5,992 |
| List-09, step 200 | 200 / pending | 67 | 2 | 208 | 11,560 |
| List-09, completed | 2,824 / done | 1,061 | 152 | 15,808 | 187,424 |
| Nat addition, completed | 1,552 / done | 583 | 91 | 9,464 | 97,560 |
| General QuickSort Local Sorted, completed | 815,075 / done | 69,221 | 12,360 | 1,285,440 | 11,878,608 |
| Fin/sorting-provider aggregate, partial | 2,000,000 / pending | 233,741 | 41,697 | 4,336,488 | 40,252,520 |

Each adapter is 104 bytes on this build. Bytes include Job headers, immutable
input slots and inline private state; exclude hash buckets, arena padding and
separately allocated state. This is not a total-memory or speedup estimate.
Evidence adapters alone account for about 11% of Job allocation bytes in the
completed QuickSort case; removing them alone does not solve graph growth.

List-09 retains 857 Evidence-premise edges; 218 have a child subject already
present as a direct operand, type or origin of the parent subject. The QuickSort
case has 200,407 / 27,387 such edges. This pointer-level overlap is a lower-bound
inventory, **not** proof that all these premises are redundant: a receipt may
carry a required justification not implied by its subject's existence.

List-09 has 862 Job result references to 329 distinct Evidence records. Aliases,
post-checks and projections can legitimately share a result; this count alone
does not authorize merging their requests or dropping their checks.

### Reproduction

The diagnostic is `src/prototype/artifact_persistence/state_audit.c`, built as
`artifact_state_audit` by the existing prototype `build.mk`. It inspects stores
without changing their counts or advancing Solve during the census. Explicit
budget arguments advance the ordinary solver between samples.

```sh
make -f src/prototype/artifact_persistence/build.mk \
  OVERLAY=/tmp/a-program-domain-frontier BUILD=/tmp/a-program-domain-frontier/build \
  /tmp/a-program-domain-frontier/build/artifact_state_audit
/tmp/a-program-domain-frontier/build/artifact_state_audit examples/09_list_induction.p 0 200 3000 3000
sed '/^import /d;s/#\.terminates/#terminates/g' \
  tests/fixtures/sorted-proof-provider.p tests/fixtures/local-strong-sorted.p \
  tests/fixtures/quick-sort-proof-common.p tests/acceptance/generic-quick-local-sorted-result.p \
  > /tmp/a-program-state-audit-quick-local.p
/tmp/a-program-domain-frontier/build/artifact_state_audit /tmp/a-program-state-audit-quick-local.p 0 100000 5000000 5000000
```

The generated QuickSort input flattens those fixtures; it does not measure the
separate import boundary. Only the deprecated intrinsic spelling is normalized.
The larger aggregate uses `finite_sorting/provider.sh quick.p boolean-order.p`
with the same spelling replacement. It remains pending at the measured budget;
this audit makes no completion or regression claim about that partial sample.

At the audit-only milestone, the List-09 and completed QuickSort censuses produced
identical TSVs in O2 and ASan/UBSan builds, including repeated samples without
additional fuel. No full acceptance suite was rerun for this audit-only change.

## Assessment

**Job is not a semantic primitive.** Current Job inputs usually borrow existing
objects; they do not necessarily copy Term nodes. Nevertheless, an independently
interned request with its own result, status and dependency edges can duplicate
an existing owner's representation. API signatures requiring a Job are not a
justification for keeping that representation.

Resumable Solve still needs the position of unfinished work, unresolved
dependencies and their notification. These can belong to the existing operation
owner, with the ready queue referencing that owner directly. For example,
`pg_typed_query` already owns progress, status and result; a second outer Job
does not need to own them again. This is a candidate replacement, not a claim
that deleting all Jobs from the present implementation already works.

Follow-up inspection at `ee2fefa` (2026-09-30), separating accepted sources
from the committed direct-input prototype. The unfinished local operation-input
trial and unrelated accepted-source edits are excluded from these conclusions:

- **No unfinished work:** `synthesis_derivation.c:evidence_ready` only assigns
  `result = inputs[0]` and DONE. This role still exists in both implementations;
  direct Evidence references can replace it. It is not a second witness Term.
- **Already-owned work:** accepted `synthesis_context.c:checked_query_step`
  wraps a `pg_typed_query` that already owns progress and result. The committed
  prototype has removed that wrapper. This is evidence for removing this
  particular allocation, not for treating every enclosing operation as an alias.
- **Genuinely unfinished work:** `substitution_step` retains its current map,
  next image and a pending conversion. `source_work` also precedes the finished
  typed result. These cursors cannot simply be replaced with that future result.
  Their representation should belong to the operation, without another graph
  copying its inputs, progress and result.

A common queue header embedded in an operation is not inherently redundant;
two independently owned records for the same operation are. Conversely,
`reindex_step` waits for a structural occurrence action and then constructs
checked reindexing evidence: the action result alone is not that evidence.
Audit distinct obligations before deleting such a wrapper. No new runtime tests
were run for this follow-up static inspection; verified Identity migration is
recorded separately below.

### Does Job Have to Exist?

**Subjective (User):** 2026-09-30, English paraphrase: question the need for Job
itself, not only the size of its adapters.
Follow-up paraphrase: resume from the Solve constraint frontier; investigate
whether Job/Evidence add another near-identical structure above Term, typing and
Solve, with unnecessary competing authority. This requests investigation, not
approval of a particular replacement layout.

**Objective (Code):** `synthesis_work.h:pg_synthesis_job` retains an interned
request key, result reference, status, queue link and dependency notifications.
`synthesis_work.c:pg_synthesis_work_request_key` allocates owner-private state
and this header together on a hash miss: it does not recursively copy a Term.
However, `synthesis_derivation.c:evidence_ready` allocates this representation
even when no work remains. `typed_query.h:pg_typed_query` already owns a query's
key, progress and result; the prototype now borrows that query without its old
wrapper. `classifier_step` still forwards producer-keyed requests to another
checked-input-keyed request. `substitution_step` genuinely needs its next-image
cursor and pending conversion; source block synthesis also needs its position
before the final typed occurrence exists.
`effect_inference.h:pg_effect_inference` already owns its equation queue and
dependency cursor, while `synthesis_effect.c` places a scheduling Job around it.
`derivation_step` retains the next premise and conversion/normalization phase.
These are distinct cases: the former already has a progress owner; the latter
also has unfinished checking, not simply a missing result reference.

**Assessment:** an independent Job graph is neither a CBPV requirement nor an
object-language proof. Its necessity has not been established. Retain the
capability to suspend, share and resume unfinished operations, not this layout
by default. A queue can reference the existing operation owner. Where there is
no owner yet, a single unfinished-construction record may be necessary; attaching
it to a nonexistent future occurrence, a mutable shared Term, or a renamed Task
graph does not solve ownership. Completed results may remain memoized for sharing,
but this does not require retaining every scheduler frame and forwarding edge.
Removing forwarding alone previously lost checked-result sharing, so the change
must preserve that sharing through one actual owner rather than abandon it.

Clarification from the 2026-09-30 code inspection at `910d515`: there is not
already a separate canonical constraint record underneath every Job. Some Jobs
are the sole unfinished-operation record. Those require ownership consolidation,
not deletion followed by a renamed copy. A borrowed result pointer is not itself
a competing acceptance authority; duplicated progress/validation decisions are
the concern. Term identity must remain independent of typed-use identity.

The user's frontier model is a viable architectural target: authoritative
constraints borrow original syntax/typed inputs and solved results; a ready
frontier references those constraints, not another shadow graph of their inputs
and answers. Reconstructing readiness on load is not proof replay. Reconstructing
only readiness does not preserve work already spent inside a suspended reduction
or conversion: exact continuation additionally needs that owner's cursor.
Without it, restarting partial work may preserve meaning but not equal-fuel
progress. Disposable wake indexes/queues can be rebuilt from canonical constraints;
record their deterministic ordering contract where partition tests require it.

Evidence is not uniformly another Term graph: its conclusion borrows the typed
occurrence/context/map, with a receipt and sometimes premise references. An
object-language witness remains a Term. The fact that a descriptive graph exists
does not establish that it passed Kernel checking. Removing redundant receipts
or premise arrays must therefore preserve one checked-admission authority,
without promoting provisional classifier/effect approximations to accepted facts.
SE3 already covers concrete duplicate Match/induction premise edges; it must not
be replaced by an unsupported claim that every Evidence record is removable.

2026-10-01 static recheck at `a78240f42fb3e3bfac5a35435326c1a3c89fdb8f`:
accepted `src/` and the committed direct-input overlay were inspected separately.
The overlay workbench uses accepted-source baseline `e716232`, not promotion
into `src/`. Concurrent uncommitted Context-renaming/IADT-relocation edits were
read; they do not replace the Job, occurrence or Evidence layouts considered
here. This recheck makes no new runtime or performance verification claim.

- `synthesis_derivation.c:evidence_ready` still retains identical Evidence
  pointers in the input and result of an immediately DONE Job. Ordinary source
  callers no longer require it, but the factory and test callers remain.
- `synthesis_function.c:classifier_step` still advances `pg_classifier_request`
  and copies its completion/result into a Job. The typed query is the existing
  progress owner. The pending operand's classifier projection must survive
  removal of this wrapper; it supports effect discovery before acceptance.
  In the overlay, `synthesis_identity.c:action_input` still creates that wrapper
  after receiving an accepted input. This consumer can instead borrow the
  canonical typed query; the Identity action itself remains a distinct obligation.
- `synthesis_context.c:substitution_step` holds the partial map, next image and
  pending pairing. No underlying complete constraint record currently replaces
  this owner. Preserve this unfinished obligation, not a second copy of it.
- Match/induction still retains typed operands and `count + 6` receipt premises.
  This concrete overlap is separate from scheduling and remains SE3 work.

The agent's recommendation is **frontier plus owner-local cursors**, not an
independent Job graph. Here a constraint includes an unfinished construction or
checking obligation, not only a numeric/effect equation. The ready queue and
reverse dependency index may be disposable references to those owners; neither
owns a second result or acceptance state. Merely scanning Terms cannot recover
the frontier: a Term does not identify its scoped typed use or its unfinished
checking phase. Rebuilding readiness must not restart saved partial work or
silently alter scheduling order when equal-fuel partition behavior is required.
This sharpens the existing SE1 completion criterion, not a new work track.

The relevant ownership test is not whether two structures look alike. A partial
operation, its completed typed description and its checked-admission receipt
have different responsibilities. Conversely, two records tracking the same
query's progress/completion are a concrete consolidation candidate. A Job result
pointer borrowing accepted Evidence is not, by itself, a second acceptance
authority. The target keeps each obligation's inputs, cursor and result in one
owner; the frontier only references that owner. It must not introduce a new
Constraint record underneath an unchanged, independently stateful Job record.

Latest static recheck, 2026-10-01, `85f4ccd` plus the separately inspected
uncommitted Context/IADT edits: the committed prototype has now deleted the
completed Evidence adapter factory and all callers. Accepted `src/` still has
that adapter; prototype deletion is not production promotion. The general
classifier Job still forwards the canonical typed query's result/status; partial
substitution and derivation workers still own genuinely unfinished cursors.
These observations supersede the adapter status in the historical tables below.
No fresh runtime verification was performed for this recheck.

Agent assessment: adopt the user's frontier model as the architectural target,
not a claim that every existing Job already has an underlying constraint record.
Retain each unfinished obligation once, with its own cursor; reconstruct only
queue/wakeup references on load. Admission indexes are lookup paths, not extra
authorities merely because they index Evidence. The uncommitted
`solver_inputs/elimination_receipts_work` trial moves receipt lookup onto typed
owners and stores only non-default Match/induction selections. It remains an
agent experiment, not an approved representation: owner isolation, exact premise
identity after index disposal, memory cost and full resumption gates are still
required. Neither that trial nor adapter deletion completes SE1-SE5.
The SE3 milestone below supersedes this trial status; it does not supersede the
remaining single-owner construction and public resumption gates.

**Plan:** use the existing SE1 work list, not a second migration track. Its gate
requires accounting for each remaining owner and deleting needless wrappers,
not just accepting direct inputs. For each role, record its canonical key, input
owner, unfinished cursor, completion owner and actual resumption consumer.
Latest user follow-up, 2026-10-01 (English paraphrase): resume the Solve
constraint frontier rather than maintaining a near-identical Job/Evidence graph
above Term, typing and Solve. The concern is duplicated authority, not merely
adapter allocation size. This is a request for critical examination, not approval
of an additional constraint table or of moving mutable solver state into Core.

Static recheck at `55d77817d67abab5f695c70e91cca939538ecf9a` compares accepted
`src/` (with concurrent Context/IADT edits) with the verified prototype assembled
on `e716232`. The uncommitted `adapter_removal_work` trial is not verified evidence.
No tests were rerun for this recheck; milestone test results below are historical.

| Inspected operation | Accepted implementation | Committed prototype / remaining obligation |
| --- | --- | --- |
| `synthesis_derivation.c:evidence_ready` | Immediately DONE Job repeats the same checked Evidence in input/result. | Non-test prototype callers use direct inputs; the adapter factory still exists for tests. Factory deletion alone does not finish SE1. |
| `synthesis_context.c:checked_query_step`; `synthesis_effect.c:inference_step` | Outer scheduling records mirror an already-owned query/effect completion. | These wrappers are removed; consumers borrow the existing progress owners. |
| `synthesis_function.c:classifier_step` | Job advances `pg_classifier_request` and copies its result/completion. | This general wrapper remains, although Identity and normalization consumers now borrow queries directly. Consolidate ownership without losing pre-acceptance classifier discovery. |
| `synthesis_context.c:substitution_step`; `synthesis_derivation.c:derivation_step` | Partial map/next image or premise/comparison phase is held by the unfinished operation. | These are real cursors, not checked Term copies. No separate underlying constraint currently owns all this work. Consolidate the operation itself instead of deleting it or adding a parallel owner. |
| `evidence.c:prove_data_elimination` | Typed operands/maps/type retain structural inputs; the receipt also retains `count + 6` premise references. | This overlap remains SE3 work. Map exact receipts, Contexts and conversions before removing duplicated edges. |

The intended resume path is: load canonical unfinished obligations and their
local cursors, rebuild disposable readiness/wakeup references, then run ordinary
Solve on the same owners. A frontier listing alone cannot recover a half-finished
conversion or map construction. Keeping the cursor does not justify keeping a
second graph with copied inputs/results. Core pointers alone also cannot key a
typed obligation: the same Core may occur under different Contexts/classifiers.
Evidence is a checked-admission receipt borrowing a conclusion, not the
object-language witness Term itself. Preserve that admission distinction while
removing reconstructible premise edges; do not maintain a second witness program.

2026-10-01 assessment clarification (agent): preserve required information, not
the current containers. Neither an independent Job database nor a separate full
Evidence DAG follows from the theory. Checked admission can belong to the
canonical scoped typed-use owner; conversion certificates and genuinely
independent premises must remain reachable wherever that admission is stored.
Do not claim the present Evidence representation is mandatory merely because
checking is necessary. A descriptive Term or classifier is not itself checked
admission. Likewise, a frontier can be rebuilt from unfinished constraints only
if those constraints retain the interrupted operation's actual cursor. Several
current Jobs are that sole owner, not wrappers over another constraint record.
The refactor must consolidate them with the construction/checking operation,
not create a parallel ConstraintDB and keep the same stateful Jobs above it.

Do not declare SE1 complete merely because completed Evidence adapters disappear:
the remaining ownership inventory and frontier/resumption gates below still apply.
Use canonical constraints and their owner-local continuation as the frontier
target; do not allocate a second constraint-shaped Task graph. Audit existing
typed/effect query owners first, then source construction and rule checking.
Delete completed Evidence adapters; borrow already-owned queries; unify duplicate
operation ownership and construction paths before expanding persistence. Verify
shared and distinct typed uses, every changed suspension boundary, and zero-fuel
and split-fuel behavior. Do not claim all Job state removable until that gate passes.
For the accepted-input Identity classifier consumer, verify direct query sharing,
one transition per dispatch, failure propagation and no extra classifier Job.
Removing that consumer's wrapper is not removal of every classifier-formation
operation, and is not completion of SE1.

Some synthesis requests precede the existence of a typed conclusion. Their
identity includes the operation, scope and inputs, not just a Term pointer.
Inventory these cases before deciding the remaining state layout. Do not move
typing state into Core, encode all requests as Terms, or rename the same Job
graph to a new Task/Result database and call that removal.

**Revise the earlier conclusion:** preserving checked-input sharing does not
require retaining a full `EVIDENCE_JOB`. "It does not recompute" was insufficient.
Conversely, replacing each adapter with its original source producer would lose
sharing between distinct producers yielding the same checked input. Reject both
that shortcut and merging requests merely because their erased Core is equal.

The target is one structural owner for each construction and sparse work state
for unfinished operations on it. Known inputs should reference existing checked
data directly, without allocating a completed scheduler node. Pending inputs
still need stable identity, dependency notification and failure propagation.
A dependency representation must support both without silently changing interned
keys after completion or building another persistent result/Claim database.
Choose its concrete C layout from the owner/dependency inventory before caller
migration, then verify it with allocation measurements. Do not replace the
wrapper with another separately allocated wrapper.

Typed construction and its checking must use the same owner-local structural
operations. Keep provisional structure unaccepted until the ordinary rules
validate it. Preserve pending effect discovery, independent synthesis and `::`
as post-check only. Do not interpret a stored occurrence or a DONE byte as proof.

Object-language equality witnesses remain Terms. Removing duplicated metalevel
premises must not erase witness distinctions, conversion/reduction certificates,
nominal identity, scoped substitutions or the difference between checking and
executing an effect. Type/value judgement changes and `Thunk` formation are not
the same as a no-work Evidence adapter and cannot be deleted on that analogy.

## Plan

No new binding/domain checkpoint fields or backend features before this gate.
Implement prototypes, verify, and push reviewable deletion-oriented milestones.
Do not mark a milestone complete just because a view hides the old representation.

- [x] **SE0 audit:** trace the four producer/consumer paths above; add a read-only
  census and pin the baseline. Audit measurements do not complete the refactor.
- [ ] **SE1 ownership before adapters:** enumerate Job roles and record their
  existing input/result owner, actual suspension state and consumers. Classify
  no-work adapters, forwarding aliases, queries with an existing progress owner,
  and genuinely unfinished construction. Try direct owner references plus
  owner-local suspension state; document any residual allocation that cannot be
  eliminated and why. An independent Job graph is not an acceptance criterion.
  For each retained record, identify the unfinished obligation that has no
  other owner. Direct-input migration alone does not discharge this requirement;
  a renamed task graph or one mutable status per Core Term also fails it.
  Then migrate the dependency interface and its callers
  so accepted Evidence needs no `EVIDENCE_JOB`, scheduler status or duplicate
  result slot. Remove the adapter factory/role, then redundant producer-to-
  checked forwarding where the same operation is otherwise duplicated. Retain
  exact sharing, ownership and failure contracts. First cover binding/domain,
  normalization and rule premises together, not three incompatible adapters;
  then migrate source/context/IADT/Identity/operation consumers before closure.
- [ ] **SE2 single construction path:** enumerate provisional structure inputs
  for Lambda/App/Pi and CBPV, extract their actual construction once under the
  existing semantic owners, and have checking consume that same construction.
  Remove the corresponding duplicated structural walkers and overbroad private
  state. Keep genuinely unfinished queries and effect dependencies; do not
  replace all of them with an unconditional wait for accepted Evidence.
- [ ] **SE3 Evidence inputs:** map each retained premise to typed operands,
  context/map, receipt or other real logical input. Remove duplicate premise
  arrays and history-based access for the mapped rules, starting with
  Match/induction/constructor. Migrate interning keys, consumers, export and
  ordinary checking in the same milestone. Recheck concurrent relocation work;
  do not overwrite it or silently discard required scope/formation evidence.
  Match/induction sharing and conclusion-index removal are implemented in the
  SE3 milestone below; constructor/other-rule input consolidation remains open.
- [ ] **SE4 persistence projection:** consume those canonical structures through
  borrowed views, removing exported copies of reconstructible rule-input trees
  and obsolete owner codecs. Keep only unfinished state needed by actual Solve
  consumers. Wire ordinals are transport references, not a new semantic layer.
  No C layout, ABI, LinkerScript or target-native representation enters `.a`.
- [ ] **SE5 completion gate:** run existing acceptance, effect/handler, IADT,
  Identity, source/derivation I/O and backend boundary tests. Add focused tests
  for shared checked inputs from distinct producers, same Core with different
  typed uses, foreign/unaccepted input rejection, and every affected suspension
  boundary. Require unchanged object-language results and canonical sharing.
  Report per-file implementation/test/doc deltas separately and repeat the
  census. A growing compatibility layer retaining both implementations fails
  this gate. Exact old dispatch counts are not a semantic invariant when useless
  dispatches are removed; equivalent split budgets on the new path still are.
- [ ] After SE1-SE5, resume AP1-AP3 using the simplified owners. Keep the current
  failing public partition gate visible; passing owner fixtures does not settle
  it. Full resumption must pass step-0 inertness and equal-fuel partition tests
  under the selected trust/revalidation policy, without an independent replay
  engine. Return to AP6 only after the prerequisite refactor is complete.

### SE1 Direct-Input Migration

The [direct-input prototype](../src/prototype/solver_inputs/README.md) applies
after the artifact candidate. Fresh rule-premise results below refer to parent
`661d4a0` plus the prototype patches in this milestone, on the isolated accepted
baseline described above. Concurrent accepted-source edits remain excluded.

- [x] WHNF/NF requests borrow checked inputs without creating Evidence Jobs.
  Pending inputs refer to their existing producer, without a new input node.
- [x] Migrate binding/domain normalization, CLI demands and IADT endpoint
  requests through that same input representation. Validate ownership once at
  request construction; do not change interned keys after a producer finishes.
- [x] Export checked leaves directly, without creating Jobs during writing.
  The membership index is temporary transport bookkeeping; no wire fields,
  Core tags, acceptance table or target-language data were added.
- [x] Extend the same input representation to Context reindexing, substitution
  source/destination inputs and post-synthesis expectations. Migrate known
  Match/Identity-family operands and share legacy/direct keys. Actual conversion
  still runs and creates its checked receipt; a direct pointer is not a bypass.
- [x] Migrate rule premises and every reader together: checking, Lambda/Pi/CBPV
  structure queries, binder/scope discovery, export and checkpoint fixtures.
  Remove the Job-only premise getter instead of recreating adapters inside it.
  Direct inputs and legacy adapters have the same key; pending producers retain
  stable identity after completion. Context-dependent callers reuse this input
  representation rather than converting a checked Context back into a Job.
- [x] Remove broad source state from generic term/type/classifier queries.
  Their private state contains three borrowed pointers, not source/Match fields.
  This physical cleanup does not complete SE2's single-construction-path work.
- [x] Remove `CHECKED_QUERY_JOB` and its composition/lift wrapper APIs. These
  queries already own progress, status and results in `typing->typed_queries`.
  Constructor-scope and Identity-family consumers should borrow them directly,
  without interning a second scheduling/result node. One query advance consumes
  the consumer's dispatch, even when it finishes; no second query may advance
  in that dispatch. Test zero fuel, sharing across owners, cancellation,
  completed reuse and failures, then rerun source/checkpoint regressions.
- [x] Migrate substitution images through the same direct-input representation;
  replace known/pending substitution APIs with one request and one validation
  path. Return checked family-pair results without a completed Job; retain
  dependent value conversion and migrate Match generalization consumers.
- [x] Migrate classifier operands and structural readers to direct inputs.
  Normalization borrows the existing classifier query rather than allocating a
  classifier-formation Job. Keep completed-input sharing and pending identities.
- [x] Migrate nominal IADT lookup to direct inputs; retain its existing query
  across suspension instead of repeatedly obtaining normalization evidence.
  Preserve nominal formation, canonical checked sharing and stable pending keys.
- [x] Migrate logical-family requests and Lambda-body/Pi-scope contexts together;
  remove their checked-input adapters, including abstraction and operation
  signature consumers. Keep family/CBPV conversion and context checking.
- [x] Unify application, sequencing and result-context APIs around direct
  Context inputs; migrate constant-result and handler consumers, deleting
  separate checked/Job entry points rather than adding another API variant.
- [x] Migrate source-scope Context storage, environment export and name lookup
  together. Scope identity must retain checked/pending input identity, not change
  when a producer finishes. Removed the source checkpoint fixture's root Context
  Job and the derivation codec's requirement for an external checked input to
  have a Job. This prerequisite is distinct from further owner-codec expansion.
- [x] Unify Identity formation, faces, reflexivity, instances and family action/
  transport inputs. Migrate source and IADT consumers; remove the separate
  checked/pending APIs and the temporary path-to-Job array.
- [x] Migrate operation signatures, handler signature premises and startup
  Contexts to direct inputs; source transport borrows the same checked leaves.
- [x] Share the bounded query-borrowing operation with family-origin/body
  consumers. A query completing must not permit a second query advance in
  that dispatch; preserve nominal fallback when a body query has no result.
- [x] Migrate source and block-binding annotations to the same direct inputs,
  including immutable-input readers, source transport and checkpoint fixtures.
  This is a prerequisite to direct lexical-name inputs, not expected-guided
  inference or removal of the annotation's actual post-check obligation.
- [x] Remove the Effect-inference Job, its factory/getter and mirrored status.
  Consumers borrow the existing equation owner; seal/failure notifies them
  directly. Share disposable notification edges with ordinary dependencies,
  retaining one advance per dispatch and detaching borrowed edges on cancel.
  This does not close the remaining owner/persistence work in SE1 or SE4.
- [x] Migrate body, abstraction and Lambda-body inputs through the same direct
  representation. Checked constructor bodies no longer need an Evidence Job;
  actual polarity lifting, Context validation and Lambda construction remain.
- [x] Migrate lexical-name inputs and their lookup/environment/source-transport
  consumers together. Preserve typed-use identity and stable pending keys;
  do not recreate a completed producer just to retain lexical provenance.
- [x] Unify classifier-normalization entry points and return known family
  results directly. Preserve pending Context obligations, typed-use identity,
  canonical sharing and rejection instead of introducing completed workers.
- [x] Migrate constructor member/field/IH inputs, Match consumers and source
  transport together. Borrow a checked field scope instead of allocating three
  completed input Jobs; retain one unfinished scope cursor and checked admission.
- [x] Migrate application callees/arguments and known Match results directly.
  Retain actual path/generalization applications and computed-scrutinee closure;
  remove the Job-only classifier-formation API and known-index input adapters.
- [x] Finish IH-binding Context inputs through one binding API, including known
  associations, actual pending parent checks and all environment/I/O consumers.
  Focused, checkpoint, sanitizer and full acceptance verification pass.
- [x] Verify index-result/transport and constant-motive direct inputs together
  with checked-target structural projection and source consumers. See the
  transport milestone below; full acceptance of the pending-Context fix passes.
- [x] Finish direct source/artifact-writer roots, CLI selections and their I/O,
  checkpoint and audit-test consumers. See the root-input milestone below;
  full acceptance, focused, semantic and checkpoint gates pass.
- [x] Finish sequencing/result-Context and handler carrier/continuation inputs
  through the same borrowed representation, including structural readers and
  source restoration. See the CBPV-input milestone below.
- [x] Remove the Job-only rule/premise contract and every caller, including
  import/preparation, source synthesis, tests and checkpoint consumers. Only
  the checked/pending API remains; no compatibility conversion helper is kept.
- [x] Remove Job-only expect/reindex/normalization/evaluation aliases and their
  callers. Rule export accepts the same borrowed checked/pending roots; ordinary
  program and normalization/constructor checkpoint consumers need no adapters.
  See the direct export milestone below for verification status.
- [x] Remove `EVIDENCE_JOB`, remaining test callers and adapter recognition.
  Checked roots must use the same borrowed inputs, not a replacement completed
  worker. Tests use real pending owners where suspension is the property under
  test. Reading still returns real unchecked producers. See the deletion
  milestone below; this completes only the adapter-removal prerequisite.
- [x] Remove reindex producer-to-checked forwarding while retaining warm reuse
  through exact checked receipts, stable pending keys and the shared typed
  action. See the reindex-frontier milestone below for its verification gates.
- [ ] Consolidate remaining duplicate query scheduling and forwarding under
  canonical unfinished-operation owners. Inventory keys, inputs, cursors,
  results and wakeup consumers; preserve provisional classifier discovery.
  No second Constraint/Task graph underneath retained stateful Jobs. SE1 remains
  open until the ownership and resumption gates, not just adapter removal, pass.

### SE1 Completed-Input Adapter Deletion (2026-10-01)

#### Subjective (User)

English paraphrase of the latest follow-up: resume the Solve constraint frontier,
not another nearly identical Job/Evidence graph above Term and typing. Separate
required information from the current containers; no production promotion is
authorized by this inspection request.

#### Objective (Code)

Parent `f589e40`; implementation comparison is the verified `55d7781` overlay,
assembled on accepted-source snapshot `e716232`. The prototype removes the factory,
role, recognition API and remaining input-unwrapping helper. All 65 synthesis-test
factory calls and the program/definition-checkpoint recognition paths are gone.
The read-only census also drops the obsolete adapter API and its two columns.
The source/test/checkpoint/artifact-test trees match fresh patch assembly exactly.

#### Assessment

Checked inputs remain borrowed receipts, not replacement completed workers.
Pending-owner tests now use actual unfinished projection/query operations,
retaining cross-store, scoped-use, wrong-role and zero-fuel rejection checks.
List and QuickSort census values match the parent in every remaining column:
875/53885 Jobs and 2725/809426 steps. Compiler callers already stopped creating
adapters in earlier milestones; deleting the remaining API does not reduce these
runtime counts and is not completion of the frontier refactor. General classifier
wrapping, producer forwarding and SE3 premise overlap still require consolidation.

#### Plan

- [x] Remove the adapter implementation and every candidate caller/recognizer.
- [x] O2 synthesis/program tests; ASan/UBSan synthesis/program/source-I/O and
  normalization tests; semantic gate and all seven checkpoint targets pass.
- [x] Acceptance commands all pass across two runs of the frozen candidate.
  The first aggregate exited 2 on the obsolete census API and stopped scheduling
  `check`/`check-examples`. After correcting the census, both remaining targets
  exit 0; all 342 acceptance commands are accounted for across the logs. The
  first aggregate is not relabeled as a successful run.
- [x] Current-worktree overlay applies; O2 Core/IADT/synthesis tests pass with
  the unrelated local Context/IADT edits. Those edits are not in this milestone.
- [x] Public split-fuel gate rerun: still exits 2, with the same four failing
  partitions (`100+100`, `1000+1000`, `1600+1600`, `2725+0`). The report exactly
  matches the parent's; zero-fuel and early partitions pass. Overall gate is open.

Evidence: `/tmp/a-program-adapter-removal-{acceptance,acceptance-remainder,checkpoints}.log`,
`/tmp/a-program-adapter-removal-asan-*.log`, List/QuickSort census TSVs and
`/tmp/a-program-adapter-removal-partitions/partitions.tsv` (ephemeral work files).
[Per-file source/test deltas](../src/prototype/solver_inputs/adapter-removal-lines.tsv)
are +163/-341, net -178: implementation -97, audit -6, tests -75; patch-file
churn and documentation are excluded. SE1-SE5 are not marked complete.

### SE1 Source-I/O Receipt Inputs (2026-10-01)

#### Subjective (User)

English paraphrase of the latest requirement: resume canonical Solve obligations,
not a duplicate Job/Evidence graph. This prerequisite changes no proof rules or
object-language syntax and is not approval to promote the prototype.

#### Objective (Code)

Parent `d4f33a1`, isolated accepted baseline `e716232`, plus the prototype patches.
All 27 Evidence-adapter factory calls in `tests/source_io.c` are removed. Checked
Contexts, maps, functions and retained roots use existing direct inputs. On read,
unchecked roots still have actual Solve producers; repeated unsolved resaves
update the selected inputs to those restored producers. Synthesis tests still
contain 65 factory calls, so the factory/role/recognition have not been removed.

#### Assessment

The Match-allocation inspection API now accepts the same checked/pending inputs;
it does not add an alternate Job API. Checked extraction still materializes a
temporary allocation view, not a scheduler record or acceptance fact. This is
not completion of SE3's premise or SE4's persistence cleanup. Boundary tests
reject empty, unaccepted and mixed inputs and check that normal extraction leaves
Jobs, Evidence, typed occurrences/queries and Solve steps unchanged. Source-origin,
scope, nominal-family, negative-control and read-before-Solve checks are retained.
Static review corrected a stale selected-root reference in the unverified trial
and the read-only export handle's const mismatch before final verification.

#### Plan

- [x] Migrate every source-I/O fixture adapter to direct checked inputs.
- [x] Add Match inspection rejection and no-progress/no-Job boundary checks.
- [x] Focused O2 source-I/O script, synthesis and IADT tests pass.
- [x] ASan/UBSan source-I/O script and synthesis tests pass.
- [x] Fresh assembly exactly matches all four source/test/fixture directories.
- [x] Default worktree overlay assembles; core/IADT/synthesis tests pass with
  concurrent user Context/IADT edits, excluded from this milestone.
- [x] Full O2 acceptance, semantic audit and all seven checkpoint gates finish
  with exit 0, including both general QuickSort providers/partition orders,
  ordinary-result proofs, semantic partial images and invalid evidence.
- [x] Repeat ordinary census and public partition comparison: List/QuickSort
  TSVs equal the parent; public partition target exits 2 with the same four
  reload failures and an identical TSV. This is not a passing gate.

Evidence logs use `/tmp/a-program-source-receipt-`. Full SE1-SE5 remain open;
the verified workbench is `/tmp/a-program-source-receipt-tested-work`.
Next remove the remaining synthesis-test adapters without replacing
pending-owner tests with checked
inputs, then delete the factory/role/recognition. Ownership consolidation beyond
adapters and the public split-fuel failures remain mandatory completion work.

Applied-file delta from the parent: `synthesis.c` +6/-5, `synthesis.h` +1/-1,
`source_io.c` +2/-1 (implementation +9/-7, net +2); `tests/source_io.c`
+124/-108 and `tests/synthesis.c` +1/-1 (tests +125/-109, net +16).
Patch-context churn and documentation lines are excluded from these C counts.
No ordinary-compile speedup or Job-count reduction is claimed by this fixture
migration; the unchanged ordinary census is expected.

### Effect Owner Result (2026-09-30)

**Objective (Code):** parent `a6d8750` plus this prototype, on the same isolated
accepted baseline. The equation queue/cursor/results in `pg_effect_inference`
are now the only Effect-solving progress owner. Two direct rule consumers create
two Jobs, not three; unsealed work parks them, seal/failure wakes them, and a
dispatch advances the equation owner once, including the completing transition.
Cancellation removes borrowed subscriptions. Released notification edges are
reused; the focused two-consumer test retains two edges, not wait history.

**Assessment:** notification edges are disposable references, not another
constraint/result graph. This supersedes the unresolved Effect-wrapper finding
in the annotation milestone; it does not remove all Jobs or Evidence adapters.
The restricted schedule codec still rejects waits on external Effect owners;
owner persistence remains unfinished, not hidden by an added transport graph.

**Plan / Results:** O2 `check` and seven checkpoint targets pass. ASan/UBSan
synthesis, the full source-I/O script and source/derivation/definition/namespace
checkpoints pass. Fresh patch application exactly matches tested source, tests
and checkpoint fixtures. Full O2 `check-acceptance` also passes, including
Effect/handler origins, nesting, general sorting, witness and image boundaries.
List/QuickSort retain 890/54,750 Jobs, 14/771 adapters, 154,608/9,481,408 Job
bytes and 2,726/809,533 steps: these cases do not use the removed wrapper.
Job bytes exclude notification allocations; no total-memory/speedup is claimed.
Public partitions retain the four failures documented above. SE1-SE5 stay open.

| Applied-code delta from `a6d8750` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `effect_inference.c` / `.h` | 21 | 9 | +12 |
| `subscription.c` / `.h` (new notification module) | 45 | 0 | +45 |
| `synthesis.h` | 2 | 8 | -6 |
| `synthesis_work.c` / `.h` | 101 | 16 | +85 |
| `synthesis_effect.c` / `.h` | 4 | 40 | -36 |
| `synthesis_derivation.c` | 14 | 17 | -3 |
| `synthesis_cbpv.c` | 2 | 2 | 0 |
| `synthesis_handler.c` | 2 | 3 | -1 |
| `artifact/schedule.c` | 9 | 6 | +3 |
| Implementation total | 200 | 101 | +99 |
| Prototype `Makefile` | 1 | 0 | +1 |
| `tests/synthesis.c` | 82 | 21 | +61 |
| `tests/derivation_io.c` | 0 | 2 | -2 |
| `normalization_checkpoint_test.c` | 3 | 2 | +1 |

Applied-source counts follow symlink targets; patch context/docs are excluded.
The intermediate increase adds shared notification/lifetime handling, not a
new progress owner, and is not presented as final code-size simplification.

### Body Input Result (2026-09-30)

**Objective (Code):** parent `db5d694` plus this prototype, using the same
isolated accepted baseline. Body/abstraction inputs borrow checked results or
pending producers through one API. Checked constructor bodies do not allocate
input adapters. Context inputs must be checked Context judgements, not merely
owned receipts with the same scope pointer. Focused tests cover wrong Context
receipts, foreign/mixed inputs, scope mismatch, exact sharing, zero fuel and
ordinary lifting/abstraction. No Core tags or wire fields were added.

**Assessment:** the remaining BODY worker performs polarity/context work; this
input migration does not establish that every enclosing worker is necessary.
List/QuickSort adapters fall from 14/771 to 11/652 and all Jobs from 890/54,750
to 887/54,631. Job bytes rise from 154,608/9,481,408 to 154,648/9,492,808 because
direct-input keys/state are larger. Steps remain 2,726/809,533; typed/Evidence
counts and premise edges are unchanged, including repeated terminal samples.
This is not a memory/speedup claim or completion of the ownership refactor.

**Plan / Results:** final O2 `check`, ASan/UBSan synthesis/IADT and the full
source-I/O script pass, as do sanitizer source/derivation/definition/namespace
checkpoints. Full O2 acceptance and all seven checkpoint targets also pass.
Fresh patch application exactly matches the tested source/tests/fixtures.
Public partition testing still fails on reload at 100+100, 1,000+1,000,
1,600+1,600 and 2,726+0; step 0 and 10+10 pass. SE1-SE5 remain open.

| Applied-code delta from `db5d694` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` / `.h` | 47 | 42 | +5 |
| `synthesis_source.h` | 3 | 3 | 0 |
| `synthesis_cbpv.c` | 44 | 33 | +11 |
| `synthesis_function.c` | 3 | 4 | -1 |
| `synthesis_handler.c` | 6 | 6 | 0 |
| `synthesis_operation.c` | 2 | 2 | 0 |
| `synthesis_binding.c` | 1 | 1 | 0 |
| Implementation total | 106 | 91 | +15 |
| `tests/synthesis.c` | 97 | 25 | +72 |
| `definition_checkpoint_test.c` | 21 | 14 | +7 |

Counts follow applied source, excluding patch context and documentation.

### Lexical Name Input Result (2026-09-30)

**Objective (Code):** parent `910d515` plus this prototype, with accepted-source
snapshot `e716232` excluding concurrent edits. Names, lookup, environments and
source-reference inputs now borrow checked receipts or pending producers through
one API. The Job-only name API is removed. Typed-use identity, scope ownership
and pending keys are preserved. A pending parent Context remains a real
projection obligation, not grounds to reject an otherwise checked leaf early.
Source export canonicalizes legacy adapter/direct references through its existing
DAG key projection. An initial acceptance run exposed duplicate wire producers;
the correction retains byte-identical step-0 resave, rather than relaxing it.
No new Core tags, wire fields or owner codecs were introduced.

**Assessment:** QuickSort adapters fall from 652 to 489, total Jobs from 54,631
to 54,468 and Job allocation bytes from 9,492,808 to 9,475,856. List remains at
11 adapters, 887 Jobs and 154,648 Job bytes. Steps remain 809,533 / 2,726 and
typed/Evidence/premise counts are unchanged. Job bytes exclude scopes, indexes
and other arenas: this is not a total-memory or speedup claim. Restricted source
checkpoint capture still rejects direct checked references; SE1-SE5 remain open.

**Plan / Results:** focused O2 synthesis/program/source-I/O and seven checkpoint
targets pass. ASan/UBSan synthesis, IADT, full source-I/O and five source/
derivation/definition/namespace checkpoint targets pass. Fresh cumulative patch
application matches all tested source, tests and checkpoint fixtures exactly.
Final full O2 `check-acceptance` (including `check`) passes, including both LT
providers, both partition orders and invalid-evidence refusal after reload.
Public partitions freshly retain four reload failures at 100+100,
1,000+1,000, 1,600+1,600 and 2,726+0; 0+0 and 10+10 pass.
Evidence: `/tmp/a-program-name-input-final-acceptance.log`,
`/tmp/a-program-name-input-checkpoints-final.log`,
`/tmp/a-program-name-input-asan-checkpoints.log`,
`/tmp/a-program-name-input-partitions/partitions.tsv` and the List/Quick census
files `/tmp/a-program-name-input-{list,quick}-census.tsv`.

| Applied-code delta from `910d515` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `artifact/source.c` | 5 | 3 | +2 |
| `program.c` | 8 | 8 | 0 |
| `source_io.c` | 29 | 10 | +19 |
| `synthesis.c` | 153 | 132 | +21 |
| `synthesis.h` | 14 | 15 | -1 |
| `synthesis_source.h` | 6 | 4 | +2 |
| Implementation total | 215 | 172 | +43 |
| `tests/derivation_io.c` / `execution.c` / `program.c` | 11 | 11 | 0 |
| `tests/source_io.c` | 107 | 40 | +67 |
| `tests/synthesis.c` | 217 | 131 | +86 |
| `source_checkpoint_test.c` | 12 | 12 | 0 |

Counts exclude cumulative patch context and documentation; this intermediate
adapter migration is not yet a source-size reduction or ownership completion.

### Annotation Input Result (2026-09-30)

**Objective (Code):** `b4b633a` plus this prototype milestone, tested on the same
isolated accepted baseline. Source `::` and block annotations borrow checked or
pending endpoints through one API. Context readiness is awaited explicitly;
both endpoints are projected into that Context before the ordinary post-check.
The new nested-context regression exposed the missing target projection during
development; it now passes. Checked input presence does not bypass validation.
Source writing borrows checked leaves without allocating Jobs or changing fuel;
reading restores ordinary unchecked producers, not acceptance. No wire fields,
Core tags or separate verification engine were added.

**Assessment:** lexical-name migration needs this consumer to accept direct
inputs. Pending keys remain unchanged after completion; exact checked inputs
share independently of pending producers. This does not remove all adapters,
annotation conversion, forwarding, or the independent Job graph. The unresolved
Effect scheduling owner remains part of SE1, not replaced by another task graph.

**Plan / Results:** O2 `check` and all seven checkpoint/namespace targets pass.
ASan/UBSan synthesis, full source-I/O script and source/definition/namespace
checkpoints pass. Tests cover one annotation Job with no input adapters, ownership
and malformed-input rejection, context suspension, invalid context projection,
ordinary rejection of a wrong target, checked-leaf sharing and byte-identical
step-zero resave. Fresh patch application exactly matches the tested candidate.
List/QuickSort retain 890/54,750 Jobs, 14/771 adapters and 2,726/809,533 steps;
annotation input slots add 144/688 Job-allocation bytes. No performance or net
allocation reduction is claimed for this prerequisite. The public partition gate
still fails the same four cases (100+100, 1000+1000, 1600+1600, completed+0).
Actual deltas versus `b4b633a`: `synthesis.c` +33/-24, `synthesis.h` +3/-3,
`source_io.c` +3/-7 (implementation net +5); tests +144/-15 (net +129);
definition checkpoint fixture +9/-6 (net +3). SE1-SE5 remain incomplete.

### Operation Input Result (2026-09-30)

**Objective (Code):** `2e95665` plus the `solver_inputs` operation milestone,
verified against the isolated accepted baseline. Operation payload/response,
handler signature premises and startup Contexts use the existing direct input;
the Job-only operation request API is removed. Source transport borrows checked
leaves without new wire fields. Invalid allocation shape is rejected before
request allocation. No checking rule, witness representation or Core tag changes.

**Assessment:** removes adapters, not the operation constructor or Job graph.
The source builder still has genuine pending Lambda/request construction. The
next ownership investigation targets the Effect worker's already-owned queue,
not another expansion of adapters or checkpoint codecs.

**Plan / Results:** O2 `check` plus all seven checkpoint/namespace targets pass;
ASan/UBSan synthesis, program, execution, handler boundaries and source/derivation
checkpoints pass. Fresh patch application exactly matches the tested sources and
tests. New assertions cover direct signatures, malformed pairs, checked/pending
handler inputs, zero startup adapters and invalid-allocation non-growth.
The public partition gate still fails at 100+100, 1000+1000, 1600+1600 and
completed+0. SE1-SE5 are not complete.

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after | Solve steps |
| --- | ---: | ---: | ---: | ---: |
| List-09 | 891 / 890 | 15 / 14 | 154,552 / 154,464 | 2,726 unchanged |
| QuickSort Local Sorted | 54,751 / 54,750 | 772 / 771 | 9,480,808 / 9,480,720 | 809,533 unchanged |

Terms, occurrences, Evidence and premise edges are unchanged; repeated completed
samples do not grow. These are live-store counts, not `.a` size or timing claims.
Applied implementation changes: `synthesis.h` +6/-6, `synthesis_operation.c`
+25/-17, `synthesis_handler.c` +13/-14, `program.c` +15/-12, `source_io.c` +15/-13:
total +74/-62, net +12. Tests: synthesis +50/-20, program +4/-0, source I/O +8/-1,
net +41. Patch-context and documentation lines are excluded.

O2 measurements at `7b0bf99`, after rule-premise migration and state cleanup:

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after | Solve steps |
| --- | ---: | ---: | ---: | ---: |
| List-09 | 1,061 / 975 | 152 / 66 | 187,424 / 162,312 | 2,824 unchanged |
| General QuickSort Local Sorted | 69,221 / 61,778 | 12,360 / 4,917 | 11,878,608 / 10,152,032 | 815,075 unchanged |

Relative to `661d4a0`, this milestone removes 515 QuickSort Jobs and 1,130,784
Job-allocation bytes. Before the state cleanup, the new two-pointer premise keys
increased Job bytes by 205,240 despite removing Jobs; that intermediate result
was not a memory reduction. Generic queries no longer allocate unused source
state. At step 0 List-09 now allocates 6,376 Job bytes versus 5,992 at baseline;
the larger premise slots remain a tradeoff, not a universal size improvement.
These are live-store measurements, not `.a` size, total-memory or speedup claims.
The `structure_capable_jobs` census is now narrower: ordinary source Jobs no
longer advertise a structure callback that always returns NULL.

Term, typed-occurrence and Evidence counts are unchanged. An intermediate
version split direct inputs from legacy Evidence-adapter inputs, adding 18
normalization requests and 27 dispatches in QuickSort. Reject that split:
both denote the same checked input, so the final factory keys them identically.
Distinct unfinished producers keep their identities; distinct typed uses of
the same Core are not merged.

Implementation deltas against the artifact candidate (not patch-file line counts):

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `program.c` | 6 | 6 | 0 |
| `source_io.c` | 52 | 18 | +34 |
| `synthesis.c` | 143 | 81 | +62 |
| `synthesis.h` | 32 | 5 | +27 |
| `synthesis_context.c` | 49 | 29 | +20 |
| `synthesis_conversion.c` | 83 | 61 | +22 |
| `synthesis_conversion.h` | 2 | 2 | 0 |
| `synthesis_derivation.c` | 89 | 27 | +62 |
| `synthesis_iadt.c` | 10 | 9 | +1 |
| `synthesis_identity.c` | 4 | 4 | 0 |
| `synthesis_work.c` | 55 | 14 | +41 |
| `synthesis_work.h` | 11 | 0 | +11 |
| `synthesis_source.h` | 20 | 1 | +19 |
| `synthesis_function.c` | 59 | 45 | +14 |
| `synthesis_binding.c` | 19 | 10 | +9 |
| `synthesis_cbpv.c` | 52 | 36 | +16 |
| `synthesis_handler.c` | 1 | 1 | 0 |
| `artifact/derivation.c` | 34 | 10 | +24 |
| Implementation total | 721 | 359 | +362 |
| `tests/program.c` | 34 | 4 | +30 |
| `tests/source_io.c` | 58 | 5 | +53 |
| `tests/synthesis.c` | 132 | 8 | +124 |
| `definition_checkpoint_test.c` | 10 | 8 | +2 |
| `derivation_checkpoint_test.c` | 4 | 2 | +2 |

The rule-premise increment alone is implementation +429/-230 (net +199), tests
+79/-13 (net +66), prototype build/overlay scripts +21/-10 (net +11).
These exclude documentation and patch-file context lines. This intermediate
migration still increases source size; SE1 is not a completed deletion milestone.

Fresh verification: O2 `make check` passes, including the general QuickSort
ordinary-result theorem and negative/image controls. Normalization, source,
derivation, definition, namespace/body and constructor checkpoint targets pass;
source covers 204 lifecycle cuts and derivation covers 281 cuts (91 direct).
ASan/UBSan `synthesis_test` and the derivation checkpoint pass on the final
revision. New tests cover mixed checked/pending premises, invalid/foreign input,
stable request identity, export without semantic/Solve allocation, ordinary
import checking and small structural-query state. Earlier normalization/Context
tests retain their exact-sharing and byte-identical step-0 assertions.
These results exclude concurrent accepted-source edits and do not promote code.

Implementation decisions: rule lookup borrows the caller's key and uses the
existing shared interning algorithm, avoiding an initially tried temporary key
allocation. Checkpoint handling finds checked leaves among explicitly supplied
external owners using a temporary DAG index; it neither allocates Evidence Jobs
nor extends the wire format. Unsupported ownership still fails. Test assertions
now read checked/pending references instead of old physical operand offsets.
The standalone artifact candidate keeps its own tests through an optional
checkpoint-test path, not a duplicated test suite.

This is a staged migration, not a code-size reduction or completion of SE1.
The public partition gate was rerun on the final candidate and still fails at
100+100, 1000+1000, 1600+1600 and 2824+0. A passing checked-input step-0 roundtrip
does not close that gate.

### SE1 Checked-Query Ownership

2026-09-30, parent `7b0bf99` plus the current prototype changes. The isolated
accepted baseline and exclusion of concurrent source edits are unchanged.

The audit found that composition and lifting already intern their queries in
`typing->typed_queries`. `CHECKED_QUERY_JOB` only advanced that query and copied
its status/result. Remove that class and both public synthesis wrapper APIs.
Constructor-scope and Identity-family work now retain the original query, not a
new Task or result table. The constructor owner retains its initial pending
substitution separately because that input must still be checked.

`pg_synthesis_await_query` is an internal borrowing operation, not a request
factory. It allocates no node and never accepts Evidence. An advance consumes
the current dispatch, even if it finishes; the caller continues on a later
dispatch. Already completed queries require no further query steps. This keeps
one fuel budget without duplicate scheduling/result authority. Core, typed
rules and the artifact format are unchanged.

Fresh O2 census relative to `7b0bf99`:

| Completed input | Jobs before / after | Job bytes before / after | Solve steps before / after |
| --- | ---: | ---: | ---: |
| List-09 | 975 / 968 | 162,312 / 161,664 | 2,824 / 2,824 |
| General QuickSort Local Sorted | 61,778 / 60,991 | 10,152,032 / 10,072,920 | 815,075 / 815,008 |

Final Term, occurrence, Evidence and premise-edge counts are unchanged; repeated
completed requests do not grow stores. This removes 787 QuickSort Jobs and
79,112 Job bytes, not all remaining Evidence adapters (4,917 remain). Dispatch
order changes, so equal partial fuel need not reproduce the old revision's
intermediate counts. No `.a` size or elapsed-time improvement is claimed.

Applied-code delta against `7b0bf99`:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis_context.c` | 0 | 28 | -28 |
| `synthesis.h` | 0 | 11 | -11 |
| `synthesis_work.c` | 16 | 0 | +16 |
| `synthesis_work.h` | 3 | 0 | +3 |
| `synthesis_iadt.c` | 8 | 3 | +5 |
| `synthesis_identity.c` | 4 | 4 | 0 |
| Implementation total | 31 | 46 | -15 |
| `tests/synthesis.c` | 66 | 17 | +49 |
| `tests/iadt.c` | 5 | 11 | -6 |

Fresh `make check` and all seven checkpoint/namespace targets in the prototype
README pass. ASan/UBSan synthesis, IADT and constructor-checkpoint tests pass.
Test-only consumers exercise shared query borrowing, zero fuel, cancellation,
reuse, foreign/failed/null queries, and the one-advance limit across two distinct
queries. Actual source consumers are covered by constructor/Identity tests and
the general QuickSort theorem with negative/image controls. No production
wrapper is retained merely to keep the old test API.

The public partition gate was rerun: 100+100 and 1000+1000 still differ in bytes,
1600+1600 also differs in progress, and completed+0 still loses accepted status.
Step-0 pending resave and 10+10 pass. These remain AP1-AP3 failures; SE1 and the
overall goal are not complete. The next SE1 work is remaining no-work Evidence
adapters and producer-to-checked forwarding, not another persistence codec.

#### Family Query Boundary (2026-09-30)

**Objective (Code):** parent `f93ae9b` plus this prototype, with the same isolated
accepted baseline. `family_function_step` could advance its origin query and
application-body query in one dispatch when origin completed. The added cold
partial-family test fails on the parent implementation at the two-query budget
assertion and passes on the candidate. Both queries remain canonical typing
owners; no additional cursor, result slot, Job role or wire field was added.

**Assessment:** share an allocation-free yielding operation with the existing
checked-query borrower. Yield even when that advance completes; terminal queries
consume no query steps. Keep failure policy at the consumer: checked lifting
fails on a failed query, while family reification retains its nominal fallback.
A broader trial delayed completion of unrelated single-query consumers and
failed the advanced-namespace exact-resumption gate. Reject that unnecessary
delay; it is not required to prevent two query advances. This fix does not
remove the independent Job graph or close the ownership audit.

**Plan / verification:** the candidate passes full O2 `check` and all seven
checkpoint/namespace targets. ASan/UBSan synthesis, List-09 program, handler
boundaries and source/derivation checkpoints pass. Fresh patch application matches
the tested source, tests and checkpoint tests. Logs: `/tmp/a-program-family-frontier-*`.
List-09 and general QuickSort Local Sorted census rows are unchanged from the
parent, including repeated completed samples. The public partition gate still
fails at 100+100, 1000+1000, 1600+1600 and completed+0; do not close SE1-SE5.
Applied deltas against the parent: `synthesis_function.c` +2/-2, `synthesis_work.c`
+10/-4, `synthesis_work.h` +3/-0; implementation net +9. `tests/synthesis.c`
+19/-1, net +18. Patch-file context churn is not implementation growth.

### SE1 Substitution Inputs (2026-09-30)

**Objective (Code):** parent `93b3bea` plus this prototype, using the same isolated
accepted baseline. Substitution images formerly required Job pointers even when
already checked. Family pairing returned an ordinary checked substitution and
then allocated a DONE Job solely to expose it to consumers.

**Assessment:** use existing by-value inputs, not another allocated wrapper.
The single substitution API validates ownership on request and checks context
shape/arity in its worker for both input forms. Previously the known-input API
rejected arity at request time while the pending API deferred it; now both report
ordinary rejection when advanced. No expected type flows into image synthesis.
Family pairing retains its existing ordinary checking rule; value pairing still
requires the reindexed type and conversion receipt. Match generalization keeps
its distinction between failed speculation and a hard error.

**Plan / Results:** direct/legacy-input keys share one request; unfinished input
identity stays stable after completion. Fresh `make check` and all seven
checkpoint/namespace targets pass, including general QuickSort ordinary-result
proofs and their negative/image controls. Focused tests cover mixed images,
dependent cube rejection, zero fuel, foreign/malformed inputs, and family pairing
without any Job allocation. ASan/UBSan synthesis, IADT and constructor-checkpoint
tests also pass. The public partition gate still fails at 100+100,
1000+1000, 1600+1600 and completed+0; no persistence completion is claimed.

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 968 / 961 | 66 / 59 | 161,664 / 161,032 |
| General QuickSort Local Sorted | 60,991 / 60,838 | 4,917 / 4,764 | 10,072,920 / 10,058,232 |

Solve steps (2,824 / 815,008), Terms, occurrences, Evidence and premise-edge
counts are unchanged. Repeated completed requests do not grow stores. This is
live Job allocation, not `.a` size or an elapsed-time improvement. The two-slot
image keys cost space; the measured reduction includes that cost.

| Applied-code delta from `93b3bea` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 12 | 10 | +2 |
| `synthesis.h` | 10 | 11 | -1 |
| `synthesis_context.c` | 43 | 52 | -9 |
| `synthesis_iadt.c` | 6 | 5 | +1 |
| Implementation total | 71 | 78 | -7 |
| `tests/synthesis.c` | 105 | 46 | +59 |

### SE1 Classifier Inputs (2026-09-30)

**Objective (Code):** parent `232ffbc` plus this prototype on the same isolated
baseline. Classifier operands and their provisional readers now borrow direct
inputs. Normalization no longer allocates a classifier-formation Job solely to
advance an existing `pg_classifier_request`. A family discovered after waiting
returns its checked input without another adapter; the known-family convenience
API still returns an Evidence Job at that revision; the result migration below
supersedes this particular remaining task, not the rest of SE1.

**Assessment:** a trial also deleted producer-to-checked forwarding. The existing
`named_transport` warm-reuse assertion then failed: later requests for the same
checked input allocated fresh work instead of reusing the completed operation.
Reject that trial, without weakening the test or adding a result cache. The final
implementation retains canonical checked-input sharing. This does not establish
that the forwarding graph is necessary; removing it requires moving the complete
operation's ownership, not just deleting its connecting edge.

**Plan / Results:** fresh `make check` and seven checkpoint/namespace targets
pass, including general QuickSort ordinary-result proofs and negative/image
controls. Tests cover allocation-free checked inputs, shared classifier query
progress, zero fuel, stable pending keys, distinct producers of one result,
foreign/malformed inputs, and post-completion reuse. The public partition gate
still fails at 100+100, 1000+1000, 1600+1600 and completed+0. SE1 remains open.
ASan/UBSan synthesis, IADT, definition-checkpoint and both namespace-frontier
variants also pass.

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after | Steps before / after |
| --- | ---: | ---: | ---: | ---: |
| List-09 | 961 / 911 | 59 / 35 | 161,032 / 156,376 | 2,824 / 2,726 |
| General QuickSort Local Sorted | 60,838 / 56,712 | 4,764 / 2,733 | 10,058,232 / 9,655,672 | 815,008 / 809,533 |

Occurrences, Evidence and premise-edge counts are unchanged. QuickSort retains
1,037,913 Core nodes versus 1,037,914 before; whole-arena temporary construction
is not asserted identical across schedules. List-09 retains 259. Repeated final
samples are unchanged. These are live-store counts, not artifact bytes or timing.

| Applied-code delta from `232ffbc` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 14 | 9 | +5 |
| `synthesis_source.h` | 3 | 3 | 0 |
| `synthesis_conversion.c` | 44 | 22 | +22 |
| `synthesis_conversion.h` | 1 | 1 | 0 |
| `synthesis_function.c` | 20 | 15 | +5 |
| `synthesis_cbpv.c` | 5 | 3 | +2 |
| `synthesis_identity.c` | 2 | 1 | +1 |
| Implementation total | 89 | 54 | +35 |
| `tests/synthesis.c` | 72 | 2 | +70 |
| `checkpoint_tests/definition_checkpoint_test.c` | 1 | 1 | 0 |

No wire fields, Core tags, acceptance table or backend metadata were added.

### Classifier Result Migration (2026-09-30)

**Objective (Code):** parent `40cc48a` plus this prototype, using frozen accepted
baseline `e716232`. One checked/pending API replaces the three classifier-
normalization entry points. A checked family under a checked matching Context
returns its original receipt directly. Pending Contexts retain the actual scope
obligation and stable request key. Application-domain and motive-demand consumers
borrow the same result; no adapter factory call remains in the conversion owner.

**Assessment:** ordinary value/computation requests still check their Context
inside Solve. An initial early-rejection trial changed sequencing rejection into
an internal error; it was corrected without weakening the rejection gate.
Checked family direct return must validate its Context before granting that
result. Core/typing remain separate, with no new tags, tables or wire fields.
Constructor/Match adapters and producer-to-checked forwarding remain SE1 work.

**Plan / Results:** focused O2 synthesis/program/IADT and seven checkpoint
targets pass. ASan/UBSan synthesis, IADT, full source-I/O and normalization/
source/derivation checkpoints pass. Fresh patch application exactly matches the
tested source/tests/fixtures. Full O2 acceptance passes, including both LT
providers, both partition orders and invalid evidence after image reload. Tests cover
unchanged allocation counts for generic, partial and projected families, step 0,
pending Context rejection and key reuse, mixed/foreign inputs and legacy adapter
unwrapping. List/Quick census counts are unchanged from the lexical milestone,
including terminal repeats. Public reload partitions still fail at 100+100,
1,000+1,000, 1,600+1,600 and 2,726+0; 0+0 and 10+10 pass. SE1-SE5 remain open.
Evidence: `/tmp/a-program-family-result-{acceptance,checkpoints,asan-checkpoints}.log`
and `/tmp/a-program-family-result-{list,quick}-census.tsv`.

| Applied-code delta from `40cc48a` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 27 | 23 | +4 |
| `synthesis.h` | 5 | 5 | 0 |
| `synthesis_source.h` | 1 | 3 | -2 |
| `synthesis_conversion.c` | 17 | 28 | -11 |
| `synthesis_cbpv.c` | 3 | 3 | 0 |
| `synthesis_function.c` | 8 | 10 | -2 |
| `synthesis_identity.c` | 3 | 3 | 0 |
| Implementation total | 64 | 75 | -11 |
| `tests/synthesis.c` | 61 | 24 | +37 |

Counts exclude cumulative patch context and documentation.

### Constructor Input Migration (2026-09-30)

**Objective (Code):** parent `289fca4` plus this prototype, frozen accepted
baseline `e716232`. Constructor member, field-scope and IH-scope requests use
the existing checked/pending inputs. The separate member `_jobs` API is removed.
`constructor_from_scope` borrows the checked field map instead of creating
declaration, parameter and map Evidence Jobs. Match, constructor transport,
source I/O and checkpoint fixtures consume the same owner inputs. No Core tag,
acceptance table or public wire field is added.

**Assessment:** the field/IH cursor still owns genuinely unfinished checking;
direct inputs do not accept saved field types. An early trial missed a direct
constructor-key-array reader and failed Indexed ADT tests. Namespace/Match
readers now use the owner's input projection rather than its private layout.
Match completion and classifier-formation adapters, producer forwarding and
public resumption remain unfinished SE1/AP work; this is not removal of Job.

**Plan / Results:** focused O2 synthesis/IADT/source-I/O and all seven O2
checkpoint targets pass. ASan/UBSan synthesis, IADT, source-I/O, constructor and
both namespace-boundary targets pass. Fresh patch application exactly matches
the tested source/tests/checkpoint fixtures. Full O2 acceptance passes, including
both LT providers, both partition orders and invalid evidence after reload.
Tests check zero-fuel inactivity, one worker for a checked field-map input,
no completed input adapters, repeated reuse, pending/foreign/mixed inputs and
retained allocation rejection. Existing tests cover dependent fields, IH scopes,
invalid constructor labels and saved constant images. Public reload partitions
still fail at 100+100, 1000+1000, 1600+1600 and 2726+0; 0+0 and 10+10 pass.
Evidence: `/tmp/a-program-constructor-input-{acceptance,checkpoints,asan-checkpoints}.log`,
`/tmp/a-program-constructor-input-{list,quick}-census.tsv` and
`/tmp/a-program-constructor-input-partitions/partitions.tsv`.

| Completed census | Jobs before / after | Adapters before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 887 / 878 | 11 / 2 | 154,648 / 154,064 |
| General QuickSort Local Sorted | 54,468 / 54,158 | 489 / 179 | 9,475,856 / 9,453,344 |

Steps (2726 / 809533), Core Terms, typed occurrences, Evidence and premise edges
are unchanged; repeated terminal samples do not grow. No total-memory, artifact
size or runtime speedup is claimed.

| Applied-code delta from `289fca4` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 92 | 84 | +8 |
| `synthesis.h` | 15 | 20 | -5 |
| `synthesis_iadt.c` | 44 | 36 | +8 |
| `source_io.c` | 4 | 4 | 0 |
| Implementation total | 155 | 144 | +11 |
| `tests/synthesis.c` | 72 | 35 | +37 |
| `tests/iadt.c` | 2 | 2 | 0 |
| `tests/source_io.c` | 14 | 13 | +1 |
| `constructor_checkpoint_test.c` | 3 | 2 | +1 |
| `definition_checkpoint_test.c` | 5 | 3 | +2 |

Counts follow symlink contents and exclude patch context/documentation.

### SE1 IADT Lookup Inputs (2026-09-30)

**Objective (Code):** parent `6b9cdaa` plus this prototype, with the same isolated
baseline. Nominal lookup required an Evidence adapter even for completed types.
While its typed query was pending, each dispatch repeated normalization-proof
construction and query lookup. These were interned lookups, not necessarily new
proof allocations. Its stored instance was a borrowed pointer, not a copied ADT.

**Assessment:** pass checked data directly, retain the existing typed query,
and read the nominal instance from its owner. This removes no-work adapters but
does not remove the outer normalization/lookup worker or producer forwarding.
The pending-key API now retains the original producer identity after completion;
checked-input calls still share the completed canonical operation. Constructor,
Match and family callers already holding checked inputs use them directly.
No new owner, compatibility API or artifact field is introduced.

**Plan / Results:** fresh O2 `make check` and all seven checkpoint/namespace
targets pass, including the general QuickSort ordinary-result theorem and
negative/image controls. Focused tests cover one worker and zero adapters for a
checked nominal input, shared queries across consumers, at most one query advance
per dispatch, zero fuel, completed reuse, stable pending aliases and malformed/
foreign inputs. Fresh overlay source/tests equal the tested work tree exactly.
ASan/UBSan synthesis, IADT and constructor-checkpoint tests also pass.
The public partition gate still fails at 100+100, 1000+1000, 1600+1600 and
completed+0. This is not SE1 or artifact-plan completion.

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 911 / 908 | 35 / 32 | 156,376 / 156,104 |
| General QuickSort Local Sorted | 56,712 / 56,428 | 2,733 / 2,449 | 9,655,672 / 9,627,896 |

Steps (2,726 / 809,533), Term, occurrence, Evidence and premise-edge counts are
unchanged; repeated completed samples do not grow. No artifact-size or timing
improvement is claimed. The remaining 2,449 adapters and independent workers
are still SE1 work, not justified by this smaller count.

| Applied-code delta from `6b9cdaa` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis_iadt.c` | 34 | 29 | +5 |
| `synthesis.h` | 3 | 2 | +1 |
| `synthesis.c` | 10 | 6 | +4 |
| `synthesis_function.c` | 2 | 1 | +1 |
| Implementation total | 49 | 38 | +11 |
| `tests/synthesis.c` | 73 | 8 | +65 |
| `tests/source_io.c` | 2 | 1 | +1 |

### SE1 Family and Lambda Inputs (2026-09-30)

**Objective (Code):** parent `ebe238c` plus this prototype on the same isolated
baseline. Family conversion, family-domain lookup and Lambda-body contexts still
required Jobs. Fixing only the family interface would have recreated adapters
when abstracting its body. These interfaces and Pi-scope construction now use
the existing by-value checked/pending input. Operation-signature premises also
pass checked data directly. No new API variant, owner class or wire field.

**Assessment:** no-work wrapping is distinct from converting a checked logical
family into a CBPV callable. Preserve the latter, context equality checks and
pending failure propagation. Pending family request identities are now stable
after completion; they still share the canonical checked-input computation.
Family query lookups and the outer workers remain, so this does not close SE1.

**Plan / Results:** fresh O2 `make check` and seven checkpoint/namespace targets
pass, including the general QuickSort ordinary-result theorem and negative/image
controls. Tests retain family application/projection, cancellation at every
cut, zero fuel and warm reuse; they now also assert zero Evidence adapters in
the family conversion test, direct/legacy key sharing, stable pending contexts,
malformed/foreign rejection and rejection of the wrong body Context. The fresh
overlay exactly matches tested source/tests. Public partition failures remain
at 100+100, 1000+1000, 1600+1600 and completed+0.
ASan/UBSan synthesis, IADT, definition-checkpoint and both namespace-frontier
variants also pass.

| Completed input | Jobs before / after | Evidence Jobs before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 908 / 901 | 32 / 25 | 156,104 / 155,552 |
| General QuickSort Local Sorted | 56,428 / 56,030 | 2,449 / 2,051 | 9,627,896 / 9,606,168 |

Steps (2,726 / 809,533), Terms, occurrences, Evidence and premise edges are
unchanged. Repeated final samples do not grow. Larger direct-input slots are
included in these live Job bytes; this is not an artifact-size or timing claim.

| Applied-code delta from `ebe238c` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 21 | 14 | +7 |
| `synthesis.h` | 2 | 2 | 0 |
| `synthesis_source.h` | 4 | 4 | 0 |
| `synthesis_function.c` | 74 | 58 | +16 |
| `synthesis_cbpv.c` | 13 | 12 | +1 |
| `synthesis_handler.c` | 7 | 6 | +1 |
| `synthesis_operation.c` | 11 | 10 | +1 |
| Implementation total | 132 | 106 | +26 |
| `tests/synthesis.c` | 56 | 28 | +28 |
| `definition_checkpoint_test.c` | 1 | 1 | 0 |

### SE1 Context Consumers (2026-09-30)

**Objective (Code):** parent `455ad02` plus this prototype, on the same isolated
baseline. Scope migration encountered Job-only Context consumers. Application,
sequencing and result-context construction already had direct implementations
behind duplicate APIs. These are now the sole entry points; constant-result
extraction and handler Context/carrier construction borrow the same inputs.
The completed destination in Match motive construction no longer needs a Job.

**Assessment:** this removes a prerequisite to scope migration, not the scope's
Job owner itself. No new worker, acceptance state, Core tag or wire field was
added. Conversion, effect discovery and dependent-codomain checks remain.

**Plan / Results:** O2 `check` and all seven checkpoint/namespace targets pass,
including the general QuickSort ordinary-result theorem and negative/image
controls. A new direct-context test covers zero fuel, no Evidence adapters,
legacy/direct sharing, stable pending identity and malformed/foreign rejection.
ASan/UBSan synthesis, IADT and definition/both namespace variants pass. Fresh
overlay source/tests/checkpoints exactly match the tested work. The public
partition gate still fails at 100+100, 1000+1000, 1600+1600 and completed+0.

QuickSort Jobs: 56,030 -> 56,010; adapters: 2,051 -> 2,031; live Job bytes:
9,606,168 -> 9,604,088. List-09 is unchanged. Steps, Terms, occurrences, Evidence
and premise edges are unchanged for both; repeated final samples do not grow.
This is not a claim of reduced `.a` size or improved runtime.

| Applied-code delta from `455ad02` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 17 | 17 | 0 |
| `synthesis.h` | 9 | 12 | -3 |
| `synthesis_source.h` | 1 | 7 | -6 |
| `synthesis_function.c` | 12 | 24 | -12 |
| `synthesis_cbpv.c` | 4 | 18 | -14 |
| `synthesis_handler.c` | 18 | 16 | +2 |
| Implementation total | 61 | 94 | -33 |
| `tests/synthesis.c` | 124 | 55 | +69 |
| `tests/iadt.c` | 2 | 2 | 0 |

### SE1 Source Scopes (2026-09-30)

**Objective (Code):** parent `5acaae3` plus this prototype, same isolated
baseline. Source scopes, environment export, lookup, Match index transport and
constructor application now borrow direct Context inputs. Tests check no Job
allocation for root/checked bindings, stable pending identities after completion,
exact binder validation and ordinary source-image rechecking.

**Assessment:** the checkpoint failure exposed a real Job-only transport
assumption, not a need to restore root adapters. The derivation codec now takes
borrowed spans of checked inputs and pending owners. Checked references have no
queue slots or duplicated completion state; missing/foreign references fail.
Its experimental `APGDRC4` payload changes reference ordinals, not the public
source format. No new semantic graph, Core tag, trust grant or owner codec was
introduced. This does not finish SE1 or establish that all Jobs are removable.

**Plan / Results:** O2 `check` and all seven checkpoint targets pass, including
general QuickSort ordinary-result and negative/image controls. ASan/UBSan
synthesis, IADT, source-checkpoint (204 cuts) and derivation-checkpoint (281 cuts,
15 mixed cuts) pass. Regenerated overlay source/tests/checkpoints exactly match
tested files. Public partitions still fail at 100+100, 1000+1000, 1600+1600 and
2726+0; step 0 and 10+10 pass. Do not close AP0/AP1-AP3 from these local gates.

QuickSort Jobs: 56,010 -> 55,718; Evidence adapters: 2,031 -> 1,739; live Job
bytes: 9,604,088 -> 9,574,080. List-09: 901 -> 896 Jobs, 25 -> 20 adapters,
155,552 -> 155,032 bytes. Steps, Terms, occurrences, Evidence and premise edges
are unchanged, and repeated final samples do not grow. Standalone root creation
allocates no Job; the initialized CLI still has one adapter elsewhere. This is
not a speedup or `.a`-size claim.

| Applied-code delta from `5acaae3` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `artifact/derivation.c` | 76 | 48 | +28 |
| `artifact/derivation.h` | 17 | 6 | +11 |
| `source_io.c` | 8 | 5 | +3 |
| `synthesis.c` | 133 | 130 | +3 |
| `synthesis.h` | 3 | 3 | 0 |
| `synthesis_binding.c` | 4 | 4 | 0 |
| `synthesis_handler.c` | 8 | 8 | 0 |
| `synthesis_iadt.c` | 50 | 39 | +11 |
| `synthesis_schema.c` | 2 | 2 | 0 |
| `synthesis_source.h` | 3 | 3 | 0 |
| Implementation total | 304 | 248 | +56 |
| `tests/synthesis.c` | 52 | 2 | +50 |
| `tests/iadt.c` | 17 | 17 | 0 |
| `tests/source_io.c` | 2 | 2 | 0 |
| `source_checkpoint_test.c` | 29 | 19 | +10 |
| `derivation_checkpoint_test.c` | 50 | 48 | +2 |

These are applied-source changes, not patch context or documentation lines.
The intermediate source increase is not presented as the final simplification.

### SE1 Identity Inputs (2026-09-30)

**Objective (Code):** parent `d0dd69d` plus the Identity prototype, same isolated
baseline. Formation, faces, reflexivity, instances and family action/transport
now borrow checked or pending inputs through one API. Source and IADT transport
consumers use it directly. The separate face/action APIs and temporary array
wrapping checked paths into Jobs are removed. Tests retain exact request sharing,
stable pending keys, prefix-dependent path checking and endpoint rejection.

**Assessment:** the remaining producer-to-checked forwarding still shares
completed requests. This milestone removes input adapters, not that ownership
problem or the whole Job graph. Witnesses and conversion/transport obligations
are unchanged. Foreign reflexivity contexts now fail at the request boundary.

**Plan / Results:** O2 `check` and seven checkpoint targets pass, including
general QuickSort ordinary-result Sorted and negative/image controls. ASan/UBSan
synthesis, IADT, source-checkpoint (204 cuts) and derivation-checkpoint (281 cuts,
15 mixed cuts) pass. LeakSanitizer could not run under sandbox ptrace; the same
binaries passed outside that sandbox. Freshly regenerated source/tests/checkpoint
files match the tested overlay. Public partitions still fail at 100+100,
1000+1000, 1600+1600 and completed+0; step 0 and 10+10 pass.

QuickSort Jobs: 55,718 -> 54,751; adapters: 1,739 -> 772; live Job bytes:
9,574,080 -> 9,480,808. List-09: 896 -> 891 Jobs, 20 -> 15 adapters,
155,032 -> 154,552 bytes. Steps, Terms, occurrences, Evidence and premise edges
are unchanged; repeated completed samples do not grow. These are live-store
measurements, not `.a` size or runtime claims.

| Applied-code delta from `d0dd69d` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 1 | 1 | 0 |
| `synthesis.h` | 14 | 25 | -11 |
| `synthesis_iadt.c` | 22 | 18 | +4 |
| `synthesis_identity.c` | 98 | 100 | -2 |
| Implementation total | 135 | 144 | -9 |
| `tests/synthesis.c` | 104 | 93 | +11 |
| `tests/iadt.c` | 19 | 18 | +1 |

### SE1 Application and Match Inputs (2026-10-01)

#### Subjective (User)

2026-09-30 paraphrase: resume the authoritative Solve frontier rather than
retain another Job/Evidence-shaped graph above Term and typing. The existing
ownership gate remains the requirement, not merely fewer adapters.

#### Objective (Code)

Parent `aa6ffc8` plus this prototype, frozen accepted baseline `e716232`.
Application borrows checked/pending callees and arguments through its existing
API. Post-checking still uses ordinary classifier/domain/conversion rules.
Match completion borrows its checked result; a pending chain is retained only
for actual path/generalization applications or computed-scrutinee sequencing.
Known constructor indices and branch Contexts no longer need adapters. The
Job-only classifier-formation API is deleted, not retained as an alias. Source
and test callers use the existing direct-input API. No Core tag, mutable result
store, acceptance table or public wire field is added.

#### Assessment

This removes completed input wrappers, not the remaining independent Job graph.
Classifier-formation still mirrors its typed Query's completion; IH binding and
refuted-branch transport still need input migration. SE1-SE5 and AP1-AP3 remain
open. Evidence receipt/premise ownership is unchanged by this milestone.

#### Plan

Focused O2 synthesis/IADT/source-I/O, all seven O2 checkpoint targets and
ASan/UBSan synthesis/IADT/source-I/O plus constructor and both namespace-boundary
targets pass. Full O2 acceptance passes, including both LT providers, both
partition orders, general ordinary-result Sorted/permutation evidence and
negative checks after reload. Fresh patch application matches the tested
source/tests/checkpoints. The prototype milestone is ready to push; this does
not authorize production promotion or close the remaining ownership gate.
New tests cover zero-fuel inactivity, no checked-input adapters, distinct typed
uses of the same Core, stable pending identity, checked/pending sharing, invalid
scope/polarity and foreign/mixed inputs. Existing tests exercise dependent
path application, computed scrutinees, effects, nested Match and resumed rules.
Public partitions still fail at 100+100, 1000+1000, 1600+1600 and 2726+0;
0+0 and 10+10 pass. No failed gate is converted to expected success.
Evidence: `/tmp/a-program-application-input-{acceptance,checkpoints,asan-checkpoints}.log`,
`/tmp/a-program-application-input-{list,quick}-census.tsv` and
`/tmp/a-program-application-input-partitions/partitions.tsv`.

| Completed census | Jobs before / after | Adapters before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 878 / 877 | 2 / 1 | 154,064 / 153,960 |
| General QuickSort Local Sorted | 54,158 / 54,059 | 179 / 80 | 9,453,344 / 9,443,048 |

Steps (2726 / 809533), Terms, occurrences, Evidence and premise edges are
unchanged; repeated terminal samples do not grow. This is a live-store count,
not an artifact-size, total-memory or elapsed-speedup claim.

| Applied-code delta from `aa6ffc8` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 24 | 23 | +1 |
| `synthesis.h` | 4 | 3 | +1 |
| `synthesis_function.c` | 6 | 13 | -7 |
| `synthesis_cbpv.c` | 3 | 4 | -1 |
| `synthesis_source.h` | 0 | 2 | -2 |
| Implementation total | 37 | 45 | -8 |
| `tests/synthesis.c` | 132 | 26 | +106 |
| `tests/iadt.c` | 2 | 3 | -1 |

Counts follow applied source contents, excluding patch context and documentation.

### SE1 Binding Context Inputs (2026-10-01)

#### Subjective (User)

2026-10-01, English paraphrase: resume the canonical Solve frontier rather than
manage another near-identical Job/Evidence graph above Term and typing. Preserve
only the unfinished operation's own continuation, not a renamed shadow graph.

#### Objective (Code)

Parent `ddf007c` plus these prototype edits; frozen accepted baseline `e716232`.
`pg_synthesis_bind` now handles checked/pending inputs. Its separate Job-only
entry point is removed. IH and graph bindings use the same representation and
exact parent/binder/associated-field validation. Known Contexts are borrowed
without an adapter or binding worker. Environment export, declared-type queries,
source restoration and checkpoint fixtures migrate together; no wire field,
Core tag or acceptance store is added. A raw decoded rule is still unchecked.

#### Assessment

The first trial unnecessarily suspended checked extensions whose parent producer
had already finished. Indexed Match then could not obtain the branch Context and
became unsupported. That trial is rejected, not covered by a relaxed test.
The corrected factory borrows ready Contexts immediately, while preserving an
existing request created when its parent really was pending. The hash lookup
references that same obligation; it does not add a scope-validation result table.
The original pending transport test remains unchanged and passes. Pending
binding validation still uses broad source state; its removal/localization and
remaining query/result ownership are not completed by this input migration.
SE1-SE5 and public `.a` partition/resumption remain open.

#### Plan

- [x] Focused O2 synthesis/IADT/source-I/O and seven O2 checkpoint targets pass.
- [x] ASan/UBSan synthesis/IADT/source-I/O and source/definition/namespace/
  namespace-body/constructor checkpoint targets pass.
- [x] Fresh patch application exactly matches all ten modified source/test files.
- [x] Full O2 acceptance passes, including both LT providers/partition orders,
  universal ordinary-result Sorted/permutation proofs, images and negative controls.

This verified prototype milestone is ready to push; it does not authorize
production promotion or complete the remaining ownership/resumption gate.

Added tests cover direct IH/graph association without Job allocation, exact
environment export, absent associations, mixed/foreign input rejection,
zero-fuel inactivity and stable validation identity before/after a pending
parent completes. Existing tests retain conditional typing, transport waiting,
effectful bodies and source restoration. Public partitions freshly fail at
100+100, 1000+1000, 1600+1600 and completed+0 (2725+0); 0+0 and 10+10 pass.
No failed gate is made expected success. Logs use the prefix
`/tmp/a-program-scope-input-`; detailed partition evidence is `partitions/partitions.tsv`.

| Completed census | Jobs before / after | Adapters before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 877 / 875 | 1 / 0 | 153,960 / 153,528 |
| General QuickSort Local Sorted | 54,059 / 54,005 | 80 / 53 | 9,443,048 / 9,431,528 |

Steps decrease 2726 -> 2725 and 809533 -> 809493. Terms, occurrences, Evidence
and premise edges are unchanged; repeated terminal samples do not grow. These
are live-store measurements, not artifact-size or elapsed-speedup claims.

| Applied-code delta from `ddf007c` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 43 | 43 | 0 |
| `synthesis.h` | 7 | 11 | -4 |
| `synthesis_source.h` | 2 | 2 | 0 |
| `synthesis_binding.c` | 3 | 2 | +1 |
| `synthesis_handler.c` | 4 | 4 | 0 |
| `source_io.c` | 4 | 4 | 0 |
| Implementation total | 63 | 66 | -3 |
| `tests/synthesis.c` | 110 | 51 | +59 |
| `tests/iadt.c` | 2 | 2 | 0 |
| `tests/source_io.c` | 12 | 12 | 0 |
| `source_checkpoint_test.c` | 1 | 1 | 0 |

Counts are applied-source changes, excluding patch context and documentation.

### SE1 Transport Inputs (2026-10-01)

#### Subjective (User)

2026-10-01, English paraphrase: resume authoritative Solve constraints, rather
than add Job/Evidence structures duplicating Term and typing. Preserve one
owner for each genuinely unfinished operation.

#### Objective (Code)

Parent `4454d5e`, frozen accepted baseline `e716232`, plus these prototype edits.
Index-result, index transport, constructor disjointness/field transport and
constant-motive inputs now borrow the existing checked/pending representation.
Result extraction awaits its source/destination Context owners before reading
their checked results. A cold pending-input test failed before this fix and
passes with it; each of its three cases actually waits on the original producer.
The source classifier projection reads the same transport target; it does not
recreate a target Job. All public call sites and suspension-test readers migrate
together. The source compiler no longer calls `pg_synthesis_evidence`; its
factory, test callers and source-root adapter recognition remain temporarily.
No Core tag, acceptance table, target-specific field or wire extension is added.

#### Assessment

Endpoint/path/context validation remains ordinary checking. A direct target
from the wrong Context is rejected, not reinterpreted as a synthesis hint.
The genuinely unfinished transport retains its finite dependency-decrease
measure, candidate scopes/maps and comparison cursor. Removing that cursor
would restart computation, not simplify duplicated authority.
The allocation-only adapter milestone does not complete SE1-SE5. In particular,
classifier query completion is still mirrored by an outer Job, and the public
image does not yet resume the complete owner frontier.

#### Plan

- [x] O2 synthesis passes, including cold source/destination Context waiting.
- [x] Reverify IADT/source-I/O and seven O2 checkpoint targets after the waiting fix.
- [x] Reverify ASan/UBSan synthesis/IADT/source-I/O after the waiting fix.
- [x] Fresh cumulative patches reproduce all source/test/checkpoint files exactly.
- [x] Reverify ASan/UBSan source/definition/namespace/namespace-body/constructor checkpoints.
- [x] Full O2 acceptance, including general ordinary-result Sorted/permutation.

New tests cover direct checked targets/results, checked versus adapter request
sharing, zero-fuel inactivity, missing/mixed/foreign input rejection and exact
Context requirements. Existing pending transport tests retain their candidate
scope/map identity assertions and cancellation coverage. The initial direct
test supplied an unprojected target in the outer Context; it was correctly
rejected. The test now asserts both the valid projected target and that rejection.
The first full run exercised all four general QuickSort provider/order variants,
but returned exit 2 because a syntax-inventory subprocess changed directory and
could not resolve the relative BUILD path. That run is not a passing gate.
The fixed-code run uses an absolute BUILD path and exits 0, including all four
QuickSort provider/order variants; no test is removed or relaxed. This verifies
the prototype milestone, not production promotion or completion of SE1-SE5.

| Completed census | Jobs before / after | Adapters before / after | Job bytes before / after |
| --- | ---: | ---: | ---: |
| List-09 | 875 / 875 | 0 / 0 | 153,528 / 153,528 |
| General QuickSort Local Sorted | 54,005 / 53,952 | 53 / 0 | 9,431,528 / 9,427,472 |

Steps (2725 / 809493), Terms, occurrences, Evidence and premise edges are
unchanged; repeated terminal samples do not grow. These are live-store counts,
not artifact-size, total-memory or elapsed-speedup claims. Public partition
checks still fail at 100+100, 1000+1000, 1600+1600 and completed+0 (2725+0);
0+0 and 10+10 pass. No failed gate is changed into expected success.

| Applied-code delta from `4454d5e` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis_iadt.c` | 70 | 58 | +12 |
| `synthesis.c` | 21 | 19 | +2 |
| `synthesis.h` | 8 | 8 | 0 |
| `synthesis_source.h` | 3 | 3 | 0 |
| Implementation total | 102 | 88 | +14 |
| `tests/iadt.c` | 48 | 26 | +22 |
| `tests/synthesis.c` | 70 | 5 | +65 |

Counts exclude patch context/documentation. Logs and census files use the prefix
`/tmp/a-program-transport-input-`; partition details are `partitions/partitions.tsv`.

### SE1 Root Inputs (2026-10-01)

#### Subjective (User)

2026-10-01, English paraphrase: reconstruct the authoritative Solve frontier,
not another Job/Evidence graph above Term and typed construction. A no-work
completed wrapper is not justified by persistence or an API requiring a Job.

#### Objective (Code)

Parent `8933f72`, frozen accepted baseline `e716232`, plus these prototype edits.
The existing source writers and atomic artifact-file writer now receive the
same checked/pending inputs as Solve. CLI root selections store that input
directly. Seed writing and all I/O/checkpoint/audit callers migrate together;
there is no parallel Job-only writer. Known namespace formation/map/body roots
and explicit accepted exports need no Evidence adapter. Reading still returns
ordinary unchecked producers, with zero Solve and no evidence admission.
The source wire format, trust policy and shared relocation remain unchanged.

#### Assessment

This removes a concrete API requirement for completed Jobs, not the entire Job
graph or its remaining adapter factory/test callers. Export membership remains
scratch bookkeeping, not an acceptance authority. The temporary selection
arrays in reader tests borrow restored producer pointers; they do not schedule
work or persist a second result graph. No backend data or new wire field enters
`.a`. The ordinary compilation census is unchanged, as this milestone changes
root transport rather than computation ownership.

Two old audit assumptions required correction: an obsolete Job-only binding
API, and a total Job count including the removed startup Evidence wrapper.
The negative Context test now uses an ordinary pending Universe-formation
request; registration still must not bypass its invalid Context result. The
import test still requires exactly 2048 projection requests plus Context,
Universe and invalid Return, and no duplicate requests on repeated import.
The new round-trip test compares relocated results within their own graph,
not nominal Oracle pointers from two independently owned Programs.

#### Plan

- [x] Direct checked/pending root tests: missing/mixed/foreign rejection,
  unchanged live stores, exact repeated/step-0 bytes and shared decoded roots.
- [x] O2 source-I/O/seed gates and seven checkpoint targets pass.
- [x] O2 and ASan/UBSan semantic artifact audit passes.
- [x] ASan/UBSan source-I/O/seed and source/definition/namespace/body checkpoints pass.
- [x] Final cumulative patches reproduce all source/test/audit files exactly.
- [x] Full O2 acceptance, including ordinary-result Sorted/permutation.

The final full acceptance run exits 0, including all four general QuickSort
provider/order variants and invalid-evidence boundaries. This is prototype
verification, not production promotion or completion of SE1-SE5.

Fresh List-09 and general QuickSort census TSVs exactly match the previous
milestone. The public partition gate still exits 1 with the same four failed
reload cases (100+100, 1000+1000, 1600+1600 and 2725+0). Additional 0+20,
20+0, 1+19 and 0+2725 cases pass; shared baseline rows are unchanged.
No failure is reclassified as expected success. SE1-SE5 remain open.

| Applied-code delta from `8933f72` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `source_io.c` | 13 | 5 | +8 |
| `source_io.h` | 5 | 3 | +2 |
| `seed.c` | 1 | 1 | 0 |
| `main.c` | 4 | 4 | 0 |
| `artifact/file.c` | 2 | 2 | 0 |
| `artifact/file.h` | 1 | 1 | 0 |
| Implementation total | 26 | 16 | +10 |
| `tests/source_io.c` | 216 | 65 | +151 |
| `tests/seed.c` | 1 | 1 | 0 |
| `source_checkpoint_test.c` | 12 | 6 | +6 |
| `definition_checkpoint_test.c` | 6 | 6 | 0 |
| `semantic_test.c` | 16 | 11 | +5 |
| `metrics.c` | 3 | 1 | +2 |

These count applied source, resolving symlinks, not patch context. Test-build
selection adds 5/removes 4 lines; overlay assembly adds 13. Neither adds an
execution/acceptance path. Logs and TSVs use `/tmp/a-program-root-input-`.

### SE1 CBPV Inputs (2026-10-01)

#### Subjective (User)

2026-10-01, English paraphrase: a resumed Solve frontier must refer to the
unfinished computation, not wrap Term/typed construction in a second Job graph.
This reiterates the existing requirement; it does not authorize production edits.

#### Objective (Code)

Parent `18a54ff`, frozen accepted baseline `e716232`, plus the prototype.
Sequencing, result-Context extraction, constant-result extraction and handler
return/carrier/clause inputs now borrow the same checked/pending representation.
Their existing interfaces change together with source callers, structural
readers and handler-scope restoration; no Job-only compatibility API is added.
The sequence's normalization dependency also permits a direct checked result.
The result-Context reader preserves a checked operand instead of assuming a Job.
The source wire format and ordinary kernel rules do not change.

#### Assessment

Known inputs need no completed adapter. Actual Fold/Pi/context/conversion work
and handler effect-equation ownership remain. An explicit carrier is still a
post-synthesis bound, not an expected type used to infer a clause body.
The remaining Evidence factory and rule/expect/reindex/normalization aliases are
not removed by this milestone.
Those convenience aliases already route to the same interned request; removing
them eliminates a duplicate input contract, not a second solving authority.
The classifier-formation Job still delegates to a separately owned typed query
and copies its completion/result. That ownership issue remains SE1/SE2 work;
provisional classifier projection must survive its consolidation.

Initial verification caught one missed restore call after the signature change.
The new test then incorrectly used a value-subject comparator for a Context,
and compared an unspecified-totality Return against a total Return. These
fixture mistakes were corrected using Context judgements and an explicitly
total ordinary Return rule; kernel acceptance/comparison was not weakened.

#### Plan

- [x] Direct checked inputs, repeated request sharing, missing/mixed/foreign
  rejection, zero-fuel inactivity and stable real pending-input identity.
- [x] O2 `all check`, semantic audit and seven checkpoint targets pass.
- [x] Fresh cumulative patches reproduce source/test/audit files exactly.
- [x] ASan/UBSan synthesis, source-I/O, semantic and source/definition/body checkpoints.
- [x] Full O2 acceptance, including all general QuickSort provider/order variants.

The final full acceptance run exits 0, including all four provider/order
variants, ordinary-result Sorted/permutation, exact outputs, partial images
and invalid evidence. This verifies the prototype, not production promotion.

Fresh List-09/general QuickSort retain 2725/809493 steps, 875/53952 Jobs and
zero Evidence adapters. Terms, occurrences, proofs and premise edges are
unchanged. Job allocation bytes increase 153528 -> 153696 and 9427472 -> 9431888
because direct input slots and the normalization input occupy more inline space.
This is an input-contract change, not a memory/speedup claim. Repeated terminal
samples do not grow. Public partition TSVs exactly match the previous milestone:
the same four reload failures remain; no failure becomes an expected pass.

| Applied-code delta from `18a54ff` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis_cbpv.c` | 42 | 42 | 0 |
| `synthesis_function.c` | 3 | 4 | -1 |
| `synthesis_handler.c` | 60 | 51 | +9 |
| `synthesis.h` / `synthesis_source.h` | 13 | 13 | 0 |
| `synthesis.c` | 11 | 11 | 0 |
| Implementation total | 129 | 121 | +8 |
| `tests/synthesis.c` | 133 | 88 | +45 |
| `tests/source_io.c` | 3 | 3 | 0 |

Counts resolve applied-file symlinks and exclude patch context/documentation.
Logs, census and partition files use `/tmp/a-program-cbpv-input-`. SE1-SE5 stay open.

### SE1 Single Rule Input Contract (2026-10-01)

#### Subjective (User)

English paraphrase of the latest instruction: resume the canonical Solve
frontier rather than retaining another Job/Evidence graph over typed structure.
This milestone removes an input-contract duplication, not all unfinished owners.

#### Objective (Code)

Parent `8d3ac3f` plus the cumulative prototype patches. Rule requests now have
only checked/pending premises. The Job-only rule/plain-rule APIs, alternate
producer-key branch and test-local Job-array rule helper are deleted. Import
and preparation use direct-input scratch arrays; their actual unfinished rule
owners remain. Existing checking, structural projection and wire format do not
change. Tests borrow known receipts directly, rather than creating adapters.
Additional gates cover a missing array, oversized arity, mixed/foreign operands,
and mutation of the caller's temporary array without mutation of the interned key.

#### Assessment

This deletes a duplicate contract, not an independent authority by itself.
The Evidence factory, other Job-only contracts, classifier-formation/query
ownership, duplicated elimination premises and public exact resumption remain.

**Verification provenance correction:** default overlay assembly copies the
current accepted worktree. Its IADT/evidence edits were present in the initial
tests, despite earlier milestone descriptions calling that environment isolated.
Patch reproduction caught an unrelated IADT test addition in the generated diff;
it was removed from packaging, not from the user's files. The current recheck
also builds from a fresh `git archive e7162320712f1acdc9420b6ffae993cd97035663`.
Treat prior exclusion claims as superseded where only default assembly was used.
The default-worktree and archived-baseline results are distinguished below.

#### Plan

- [x] O2 `all check` and all seven checkpoint gates on both assemblies.
- [x] Default-worktree and archived-baseline ASan/UBSan synthesis, IADT, source/derivation I/O,
  semantic audit and derivation-checkpoint gates.
- [x] Fresh patch reproduction exactly matches source/tests/audit fixtures.
- [x] Archived-baseline List/QuickSort census and public partition TSVs exactly
  match the preceding milestone at the same budgets.
- [x] Full O2 acceptance on both assemblies.

Both acceptance runs exited 0, including all four general QuickSort
provider/order variants, ordinary-result Sorted/permutation, invalid evidence,
finite views and witness packet/isolation checks. This milestone is verified
prototype work; it is not promotion into accepted `src/`.

The derivation-I/O binary was initially invoked without its required arguments;
that invocation failed its CLI assertion. Its ordinary test script then passed;
no implementation or checking rule was changed to bypass the assertion.
The public partition script still fails the same four reload cases; `make`
reports exit 2. This is not a passing gate or an expected-success reclassification.
Completed List/QuickSort remain at 2725/809493 steps, 875/53952 Jobs and zero
Evidence adapters; graph counts and Job bytes are unchanged. SE1-SE5 stay open.

| Applied-file delta from `8d3ac3f` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/synthesis_derivation.c` | 13 | 33 | -20 |
| `src/synthesis_source.h` | 0 | 2 | -2 |
| `src/synthesis.h` | 2 | 5 | -3 |
| `src/synthesis_cbpv.c` | 3 | 3 | 0 |
| `src/synthesis_iadt.c` | 2 | 2 | 0 |
| `src/synthesis_handler.c` | 19 | 11 | +8 |
| `src/synthesis_function.c` | 18 | 18 | 0 |
| `src/synthesis_operation.c` | 8 | 6 | +2 |
| `src/synthesis.c` | 38 | 33 | +5 |
| `src/program.c` | 2 | 2 | 0 |
| `src/synthesis_identity.c` | 5 | 5 | 0 |
| Implementation total | 110 | 120 | -10 |
| `tests/iadt.c` | 7 | 7 | 0 |
| `tests/derivation_io.c` | 3 | 3 | 0 |
| `tests/synthesis.c` | 312 | 196 | +116 |
| `tests/source_io.c` | 10 | 11 | -1 |
| `artifact_tests/semantic_test.c` | 2 | 2 | 0 |
| `checkpoint_tests/derivation_checkpoint_test.c` | 7 | 7 | 0 |

Applied-code counts resolve symlinks and exclude unrelated changes, patch context
and documentation. Logs and TSVs use `/tmp/a-program-rule-input-`.

### SE1 Direct Reduction and Export Inputs (2026-10-01)

#### Subjective (User)

English paraphrase of the latest requirement: resume the Solve frontier without
retaining Job/Evidence structures that duplicate known typed inputs and results.

#### Objective (Code)

Parent `dd430c0`, prototype assembled from the archived accepted baseline
`e7162320712f1acdc9420b6ffae993cd97035663`, excluding concurrent accepted edits.
Removed four Job-only forwarding APIs and migrated all 57 calls. Rule export now
accepts checked/pending roots through the existing closure traversal; its
temporary membership indexes do not allocate Jobs, advance Solve or retain a
second acceptance table. Output ordering and duplicate roots are preserved.
The source reader selects reduction/force through the same request, without a
duplicate conditional call. No Core, Kernel rule or wire field is changed.

#### Assessment

The direct comparison test now needs three Jobs instead of six: two genuine
post-checks and one shared comparison. Its failure-sharing test needs two checks
and one comparison, with no accepted-input wrapper. Program and both migrated
checkpoint consumers contain no Evidence-adapter calls. The factory and other
test callers still exist; mirrored classifier/query ownership and SE3-SE5 remain
open. API removal alone is not completion of the frontier refactor.

#### Plan

- [x] Fresh patch assembly exactly reproduces source/tests/checkpoint/audit inputs.
- [x] Focused O2 synthesis, program and derivation-I/O tests.
- [x] O2 normalization and constructor checkpoints, including exact remaining fuel.
- [x] Final ASan/UBSan synthesis, program, source/derivation I/O and both migrated checkpoints.
- [x] Full O2 acceptance, semantic audit and remaining checkpoint gates;
  integrated successor revalidation is recorded below.
- [x] Same-budget List/QuickSort census and public partition comparison.

Export boundaries cover missing/oversized input arrays, mixed inputs, foreign
checked/pending owners, repeated mixed-root export, no source allocation/advance,
and unchanged output plus poisoned transport state on failure. Initial focused
commands were invoked before those test binaries were built (exit 127); explicit
target builds and subsequent correct invocations passed.
Static recheck then corrected the export count bound to `sizeof(*roots)`, since
the borrowed-input element contains two pointers. The oversized test targets
that exact boundary. The first full acceptance run was deliberately stopped
(exit 143) before editing frozen sources; final gates are rerun, not inherited.

Applied-file delta from `dd430c0`: implementation +31/-62 (net -31);
tests/checkpoint/audit fixtures +207/-107 (net +100). These counts exclude patch
context, documentation and unrelated accepted edits. Per-file evidence and
verification logs use `/tmp/a-program-direct-alias-`. The final full-regression
log reaches the last derived-LT success marker, including both providers and
partition orders. On continuation its process handle is no longer available;
the aggregate exit code was not retained. The successor's integrated full
regression below supplies the observed terminal result; no historical exit
code is inferred from that earlier log.

The ordinary List/QuickSort census is byte-identical to the preceding milestone
at matching budgets. Public partition TSVs also match: the same four reload
failures remain and the partition target exits 2. This is not a passing gate.

| Applied-file delta from `dd430c0` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/synthesis_context.c` | 0 | 7 | -7 |
| `src/synthesis_conversion.c` | 0 | 21 | -21 |
| `src/synthesis.h` | 4 | 19 | -15 |
| `src/synthesis.c` | 6 | 4 | +2 |
| `src/synthesis_iadt.c` | 2 | 1 | +1 |
| `src/source_io.c` | 3 | 3 | 0 |
| `src/synthesis_derivation.c` | 16 | 7 | +9 |
| `tests/synthesis.c` | 115 | 49 | +66 |
| `tests/iadt.c` | 8 | 5 | +3 |
| `tests/program.c` | 16 | 12 | +4 |
| `tests/source_io.c` | 19 | 8 | +11 |
| `tests/derivation_io.c` | 24 | 9 | +15 |
| `checkpoint_tests/normalization_checkpoint_test.c` | 21 | 21 | 0 |
| `checkpoint_tests/constructor_checkpoint_test.c` | 1 | 2 | -1 |
| `artifact_tests/semantic_test.c` | 3 | 1 | +2 |

### SE1 Remaining Checked-IADT Consumers (2026-10-01)

#### Subjective (User)

English paraphrase of the latest requirement: the Solve frontier should refer to
canonical obligations, not duplicate Term/typed inputs in a separate Job graph.

#### Objective (Code)

Parent `da162ec`, same archived accepted baseline `e716232`. Removed 23 adapter
calls: 21 in IADT tests and the final two in checkpoint tests. Checked Match,
index-path and constructor-field premises are borrowed directly. There remain
65 factory calls in synthesis tests and 27 in source-I/O tests; the Evidence
factory is not yet deleted. Ordinary compiler call sites do not require it.
`pg_synthesis_work_request_inputs` had only one test caller and the ordinary
request's internal call. Remove this obsolete prefix/Job-array key interface and
its branching key object; retain the single existing interner and borrowed-key
projection. No owner, acceptance store, cursor, Core or wire field is added.

#### Assessment

Do not discard the negative controls with the old adapters. Mixed checked/pending
input rejection uses a genuine producer; the wrong-kind body-resume guard uses
an unfinished ordinary Universe rule. Sharing, zero-fuel behavior, invalid
transport endpoints, field selection and scope checks retain their assertions.
This prerequisite cleanup does not discharge the SE1 ownership criterion.
Static ownership tracing identifies `synthesis_identity.c:action_input` as the
next query-wrapper removal: reflexivity/family action have already awaited and
validated their inputs before requesting classifier formation. They can borrow
`pg_classifier_request` directly. Do not apply this reasoning to pending
result-Context or application recipes, whose structural readers need a
classifier projection before full acceptance.
The Identity classifier milestone below supersedes this next-task selection.

#### Plan

- [x] Fresh patch assembly matches source/tests and checkpoint/audit fixtures.
- [x] Focused O2 synthesis/IADT and four migrated checkpoint/namespace gates.
- [x] Final ASan/UBSan synthesis/IADT and migrated checkpoint gates.
- [x] Full O2 acceptance, semantic audit and all seven checkpoint gates.
- [x] Repeat ordinary census/public partition comparison before committing.

Evidence logs use `/tmp/a-program-accepted-operands-`. The unchanged accepted
worktree edits are excluded, not reverted. Full SE1-SE5 remain open.
The final `make` invocation completed with exit 0, including both general
QuickSort providers/partition orders, ordinary-result proofs and invalid controls.
List/QuickSort census rows match the parent byte-for-byte at the same budgets.
Public partition rows also match; the same four reload failures remain and that
target exits 2. No test is weakened or marked as an expected-success failure.
The default worktree overlay also assembles with the concurrent Context/IADT
relocation edits. Its focused core/IADT tests pass; this is not a second full
regression run or inclusion of those edits in the commit. Differences from the
isolated candidate are only the four edited source files, two edited test files
and two new fixtures listed by the working-tree comparison.

| Applied-file delta from `da162ec` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/synthesis_work.c` | 5 | 23 | -18 |
| `src/synthesis_work.h` | 0 | 3 | -3 |
| `tests/synthesis.c` | 7 | 2 | +5 |
| `tests/iadt.c` | 50 | 52 | -2 |
| `checkpoint_tests/definition_checkpoint_test.c` | 6 | 4 | +2 |
| `checkpoint_tests/derivation_checkpoint_test.c` | 2 | 2 | 0 |

Implementation +5/-26 (net -21); tests +65/-60 (net +5). These are applied-code
counts, not patch-context or documentation lines; no speedup is asserted.

### SE1 Identity Classifier Ownership (2026-10-01)

#### Subjective (User)

English paraphrase of the latest requirement: resume the canonical Solve
frontier, without placing another near-identical Job/Evidence structure above
Term and typed data. This milestone adopts no new object-language proof format.

#### Objective (Code)

Parent `a78240f`, accepted baseline `e716232`. The prototype's
`synthesis_identity.c:action_input` now borrows `pg_classifier_request` directly
after awaiting and validating its operand. Reflexivity/family owners retain only
the query pointer, not a classifier-formation Job. Query progress/status/result
remain in the existing typing-owned query. The ordinary formation factory and
its pending structural projections are not deleted in this milestone.

#### Assessment

This removes a concrete forwarding owner, not the distinct Identity checking
obligation. Each query advance consumes the action's dispatch and yields even
on completion; a failed or unavailable classifier remains UNSUPPORTED, matching
the former formation worker. No Core, admission, queue or wire field is added.
The new boundary test checks fresh shared queries, step 0, completion yielding,
borrower cancellation, stable completed reuse, wrong scope and unsupported kind.
Existing source-action tests retain pending-input, dependent, cycle and higher
Identity controls. The test fixture's initial invalid Pi construction was fixed
to supply a computation codomain; no production acceptance rule was relaxed.

#### Plan

- [x] Replace both accepted-input action classifier wrappers with direct queries.
- [x] Fresh assembly reproduces source, tests and checkpoint/audit fixtures exactly.
- [x] Focused O2 synthesis and ASan/UBSan synthesis/IADT pass.
- [x] Default worktree overlay assembles; focused core/IADT/synthesis pass with
  concurrent user Context/IADT edits, which are not included in this change.
- [x] Full O2 acceptance, semantic audit and all seven checkpoint gates finish.
- [x] Compare ordinary List/QuickSort census to the parent.
- [x] Public partition gate rerun: exit 2, identical parent TSV, the same four
  reload failures; this is not a passing or expected-success gate.

Evidence logs use `/tmp/a-program-identity-query-`. Full SE1-SE5 remain open.
The full `make` invocation finished with exit 0, including both general
QuickSort providers/partition orders, ordinary-result proofs, semantic partial
images and invalid controls. The separate public partition target remains exit 2.
List census rows are unchanged. QuickSort completes in 809,426 transitions
(parent 809,493), with 53,885 Jobs (parent 53,952) and 9,423,312 Job allocation
bytes (parent 9,431,888). Terms, occurrences, Evidence and premise-edge counts
are unchanged; zero-fuel and 1,000-step rows match the parent. Removing 67
classifier wrappers saves 8,576 Job bytes; this is not a total-memory or wall-time
speedup measurement. Repeated terminal samples do not grow these stores.
The source delta is +11/-9 (net +2); the boundary test is +69/-0.
These are applied C-file differences from the parent, not cumulative patch
context. Job allocation elimination, not a source-line reduction, is the
verified ownership change in this milestone.

### SE3 Shared Admission and Elimination Inputs (2026-10-01)

#### Subjective (User)

English paraphrase, 2026-10-01: resume the Solve frontier without a second
near-identical Job/Evidence structure above Term and typed data. Remove duplicated
ownership, not merely rename its containers. Core computation stays untyped.

#### Objective (Code)

Parent `85f4ccd`, prototype on accepted baseline `e716232`; concurrent user
Context/IADT edits are excluded from this milestone. Typed occurrences, Contexts
and maps now anchor their checked receipts directly. The conclusion index and
its first-proof prefix allocation are deleted. Descriptive constructors ignore
incoming admission links; only checking publishes them, with owner isolation.
Match/induction no longer retain a complete second premise array: typed inputs
supply stable first receipts; sparse selections preserve other exact receipts.
The contiguous-premise API is removed and all callers use the logical getter.
No Core tag, pending-state store, acceptance table or wire field is added.

| Completed census | Logical edges, unchanged | Retained edges, before / after | Wrapped aligned arena bytes, before / after |
| --- | ---: | ---: | ---: |
| List-09 | 857 | 857 / 849 | 527,088 / 517,680 |
| General QuickSort Local Sorted | 200,407 | 200,407 / 199,746 | 238,173,696 / 237,598,448 |

All previous census columns match at identical requested budgets; QuickSort still
uses 809,426 transitions. The allocation wrapper measures cumulative `pg_alloc`
requests originating outside `graph.c`, including temporary work, not total live
memory. Its output is unchanged by the diagnostic interposition. QuickSort saves
575,248 such bytes and 140 requests; this is not a demonstrated wall-time speedup.
Applied-file deltas are recorded in [the line-count sheet](../src/prototype/solver_inputs/elimination-receipts-lines.tsv):
implementation +137/-49 (net +88), tests +67/-11 (net +56), audit/build +40/-3
(net +37). Physical duplication decreased; source lines did not decrease.

#### Assessment

Agent implementation decision: preserve exact receipt choices and alternatives,
not proof irrelevance. A circular tail link keeps the first receipt stable while
adding alternatives in order. Admission links do not participate in descriptive
identity or I/O. Reject the earlier index-dependent compression trial: logical
premise reads must survive index disposal and not depend on a live typing store.
Ordinary checking, canonical reuse and foreign-store rejection remain required.
Constructor receipts still retain family/formation/parameter/instance inputs;
the general classifier wrapper still duplicates a query result/status. These
remain work under the main SE1-SE3 list, not new migration tracks.

#### Plan

Fresh assembly reproduces source/tests/checkpoint/audit fixtures exactly. Focused
O2 tests, the full `check`/examples run, semantic audit and seven checkpoint gates
pass. ASan/UBSan Core/IADT/Identity/synthesis and source/derivation I/O pass.
New controls cover exact alternate Match receipts, stable logical inputs after
index disposal, copied-header admission rejection and no-work reads. The default
overlay also assembles with user edits; its Core/IADT/synthesis pass separately.
Full `check-acceptance` finished with exit 0, including both general QuickSort
providers/partition orders, ordinary-result proofs and invalid controls.
Public partitions remain exit 2 with the same four parent
failures and identical TSV; this gate is not waived. Evidence logs and comparison
files use `/tmp/a-program-elimination-receipts-`. Full SE1-SE5 remain unfinished.

### SE1 Reindex Frontier Ownership (2026-10-01)

#### Subjective (User)

English paraphrase, 2026-10-01: resume the Solve frontier rather than a second
Job/Evidence graph; consolidate ownership instead of renaming duplicated state.
This prototype remains separate from accepted sources and user Context/IADT edits.

#### Objective (Code)

Parent `9eb94cf`, assembled on isolated accepted baseline `e716232` plus recorded
prototype patches. `synthesis_context.c:reindex_step` created another checked-input
Job after its pending operands resolved. Both borrowed the same occurrence action;
the outer worker then copied the other Job's admission result and completion.
The checked `PG_REINDEX` result already has an exact-premise key in `typing.proofs`.

#### Assessment

Remove that forwarding owner, not just its edge. One checked/pending result API
now returns an existing exact checked receipt directly or the actual unfinished
obligation. Existing pending keys remain stable, including originally checked
requests that needed work. Pending requests no longer manufacture checked-input
Jobs on completion. The ordinary occurrence action owns structural progress;
the reindex obligation checks admission afterward. An action result alone is
not a checked fact. The new read-only receipt lookup uses the existing proof
index, not a result cache, acceptance table or replay engine.

Pairing, constructor scopes, family continuations, source name transport and data
cases borrow that same result interface. The Job-only reindex API is deleted.
Two distinct producers retain their own unresolved-input obligations, but share
the action and exact admission result once inputs agree. No Core/wire field is
added. General classifier forwarding and the broader SE1-SE5 gates remain open.

#### Plan

- [x] Delete automatic checked-input reindex forwarding and migrate all callers.
- [x] Preserve warm admission reuse and pending request identity without a cache.
- [x] Add alias sharing, one-action-step-per-dispatch, zero-fuel, scheduler-lifetime
  and action-without-admission boundary tests; retain existing negative controls.
- [x] Full O2 `check`, examples, semantic audit and seven checkpoint gates pass.
- [x] ASan/UBSan synthesis passes, with leak detection enabled.
- [x] Repeat ordinary List/QuickSort census and public partition comparison.
- [x] Complete full acceptance, including both general QuickSort providers/orders.
- [x] Reassemble recorded patches exactly; check coexistence with user edits.
- [x] Record applied per-file deltas; prepare the verified prototype milestone.
- [x] Push the verified prototype milestone after full acceptance finishes:
  `dc33ef4` published to `origin/main` on 2026-10-01.

Fresh census: List-09 completes in 2,720 steps (parent 2,725), with 870 Jobs
(875) and 153,016 Job bytes (153,696). General QuickSort completes in 809,172
steps (809,426), with 53,643 Jobs (53,885) and 9,392,688 Job bytes (9,423,312).
Completed Terms, occurrences, Evidence and logical/retained premise counts are
unchanged; repeated terminal censuses do not grow them. Zero-fuel rows match.
These are live Job allocations, not total memory, artifact size or a wall-time
speedup claim. Family continuation state includes the existing checked/pending
input pair; its cost is included, not hidden from the net reduction.

Public partition comparison still exits 2: reload failures at 100+100,
1000+1000, 1600+1600 and completion+0. The completion budget falls from 2,725
to 2,720; image byte counts and failure classes match the parent. This is not
a passing gate. Fresh evidence files use `/tmp/a-program-reindex-frontier-`.

Recorded patches reproduce `src`, tests, checkpoint fixtures and artifact audit
fixtures exactly. The default worktree overlay also assembles, and its Core,
IADT and synthesis tests pass with the concurrent user edits. These edits are
not included or promoted. Applied deltas from `9eb94cf` are recorded in
[the per-file sheet](../src/prototype/solver_inputs/reindex-frontier-lines.tsv):
implementation +84/-53 (net +31), tests +105/-25 (net +80). Stored patch context
and documentation are separate from those applied C-file counts.
Full `check-acceptance` exits 0, including both LT providers/partition orders,
universal Sorted/permutation witnesses, ordinary results, semantic partial images
and invalid controls. Full SE1-SE5 remain unfinished; public partitions are not
waived by this milestone's successful acceptance run.
