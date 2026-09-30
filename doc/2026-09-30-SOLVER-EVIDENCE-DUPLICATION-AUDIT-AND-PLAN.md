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

**Plan:** use the existing SE1 work list, not a second migration track. Its gate
requires accounting for each remaining owner and deleting needless wrappers,
not just accepting direct inputs. For each role, record its canonical key, input
owner, unfinished cursor, completion owner and actual resumption consumer.
Use canonical constraints and their owner-local continuation as the frontier
target; do not allocate a second constraint-shaped Task graph. Audit existing
typed/effect query owners first, then source construction and rule checking.
Delete completed Evidence adapters; borrow already-owned queries; unify duplicate
operation ownership and construction paths before expanding persistence. Verify
shared and distinct typed uses, every changed suspension boundary, and zero-fuel
and split-fuel behavior. Do not claim all Job state removable until that gate passes.

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
- [ ] Finish IH-binding, index-result/transport and remaining Context input
  adapters and consumers, remove `EVIDENCE_JOB`, then remove duplicated query scheduling.
  The remaining adapter recognition is temporary, not the final architecture.

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
