# Solver and Evidence Duplication Audit

Date: 2026-09-30
Status: audit complete; SE1 ownership consolidation underway, full refactor unfinished.
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

2026-10-01, English paraphrase of the latest user follow-up: restarting from
the Solve constraint frontier should suffice; critically examine Job/Evidence
as possible duplicate graphs above Term, typed construction and Solve. This is
a request to examine the architecture, not approval of a replacement wrapper.

2026-10-01, English paraphrase of the user's clarification: the original concern
was that Job enumerated and re-expanded the data structures already separated
into Oracles, collapsing their implementation boundaries into one upper layer.
The target is not merely fewer Jobs or a different spelling of the enumeration.

2026-10-01, English paraphrase of the user's documentation instruction: record
new user input immediately in each affected active plan's `Subjective (User)`,
before context compaction can discard it. This requirement is now in `AGENTS.md`.

2026-10-01, English paraphrase of the user's further clarification: the reviewed
Job representation appeared to flatten the Lambda/Ref-based Core and its
Oracle-local IADT/CBPV structures into a second upper-layer representation;
remove that duplication rather than preserve Job as an architectural premise.
Audit Evidence for the same flattening and remove duplicated structures
thoroughly. The user's intended resume model is to retain the already verified
portion through witness Terms and their typed Occurrences, and continue Solve
from unfinished obligations on those owners. The Curry-Howard-based expectation
that this can suffice is a user design requirement to investigate, not an agent
claim that resume correctness has already been established.

2026-10-01, English paraphrase of the user's follow-up: balance continued audit
with concrete implementation; do not postpone all deletions until an exhaustive
audit ends, or implement deletions without checking their actual ownership.

The user has not approved a new dependency representation or a new proof format.
The design directions below are agent proposals grounded in the inspected code.

2026-10-01, English paraphrase of the latest correction: documentation-only
updates are not sufficient progress; proceed with concrete code deletion and
verification alongside the ownership audit. The user also reports GitHub
connectivity restored; this is operational information, not a design change.

2026-10-02, English paraphrase of the user's reiterated instruction: continue
actual deletions, not documentation-only work; audit and implementation must
advance together without rebuilding Oracle-local Term structure in Job/Evidence.

2026-10-03, English paraphrase of the continuation: keep that balance and remove
duplicated stored structure; documentation-only activity is not implementation.

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

Current clarification check (`f0ff363` plus the uncommitted structural-reader
trial, 2026-10-01): `synthesis_work.h` uses private class descriptors rather than
a global semantic Job enum, but `synthesis.c:source_work` still contains broad
state, including a union of function-graph, constructor, block/application/Match
and definition work. Its `SOURCE_WORK` roles still share central dispatch.
Descriptor replacement alone therefore does not satisfy Oracle locality.
The same inspected trial's `evidence.c:pg_evidence` retains a rule identifier,
conclusion reference, certificate and sparse receipt inputs, not a complete
copy of each Oracle payload. However, `derivation.h:pg_derivation_parameters`
and `synthesis.c:step` still centralize many domain-specific choices. Distinguish
this actual centralization from the stronger, unproven claim that every Evidence
is a duplicated witness Term.

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

2026-10-03, agent follow-up on `efc55b1`:
`pg_context_map` stores both typed images and their Core values in a second
binding array. Project values from the existing images through the same borrowed
input contract; retain only the positional binder index. Removing that index
would replace constant-time indexed reads with repeated Context-list walks.
Keep ordinary scope/dependent checks, exact image identity and wire layout;
this is not permission to infer acceptance from descriptive maps.

2026-10-02, agent decision on `c1d93d3`: unary CBPV, Lambda/APP and Context
introduction already retain no premise array; do not invent new receipt storage
there. Effect substitution still copies equation results into `bindings[]`
before the Core substitution owner consumes them.
Use one synchronous array/reader contract for the existing substitution API;
borrow solved equations, preserve ordered images/identity-prefix normalization,
checking and unfinished cursors. Remove the staging array and temporary flattened
request key, not the actual substitution work. Retain the borrowed completed
Term pointer: the lower substitution store frees its work on destruction while
the Core result remains graph-owned; this pointer is not a copied Term graph.

2026-10-02, agent implementation decision against `2f2e6d1`: receipt admission
already borrows typed inputs, but `derivation.c:premise_slice` still allocates
full premise tails before ordinary Match/induction, TypeCase, Schema, substitution
and family Identity checking. Use one synchronous input view for those existing
constructors and their callers, not parallel array/borrowed APIs or another graph.
Preserve exact receipt selection, dependent checks, nominal allocation and
post-check-only expectations. The view is a call argument, never retained work
or evidence; verify null/foreign inputs, repeated lookup, source images and fuel.

2026-10-02, agent implementation decision against `486d64d`:
`CONSTANT_MOTIVE_JOB` has no independent reduction cursor. It stores an existing
branch abstraction and constant-result request, translates rejection and copies
the checked receipt. Remove that owner; the Function factory returns the
canonical constant-result request directly. Match retains strategy fallback at
its actual decision site, not in another scheduling object. Preserve scoped
independence checking, independent branch synthesis and post-check-only `::`.
Verify dependent/invalid branches, zero/split fuel, images and allocation/runtime
effects before adoption; repeated factory lookup must not recreate checked work.
The temporary constant-result Scope array also repeats the immutable parent
edges already on those Scope requests. Traverse those edges in reverse and
borrow each Scope's checking output; no second telescope or receipt is needed.

2026-10-02, agent implementation decision against `2bebfca`: Handler return,
operation clause and fold, sequencing, operation-function preparation,
constructor-result preparation, field-bearing constructor abstraction and IADT result-map discovery
still copy an existing checking owner's receipt into their Job header. Borrow
that owner's output through the existing DONE-gated input view instead. Keep
carrier/context/index checks and genuine nullary constructor admission; raw
structure readiness must not expose a checked result. Delete the separate
`SOURCE_RETURN_HANDLER` role/state/dispatch: return-only surface handling shares
the existing sequencing owner directly. Roles whose output is their projected
rule opt into one accessor; no Oracle switch, graph or progress state is added.
This removes aliases and duplicate receipt references, not every unfinished
discovery cursor or the resume gate.

2026-10-02, agent decision against `b54fdcb`: Telescope discovery owns the
Scope/tail cursor; a second `TELESCOPE_JOB` only waits for that cursor and copies
its Context receipt. Consolidate discovery and checking on the existing owner,
using the scheduler's preparation notification for provisional readers and
ordinary completion for admitted Contexts. Borrow the Scope's checked output;
delete the second factory/role and Schema's duplicate pointer. Preparation is
not acceptance. Verify cyclic/unresolved domains, both kinds of waiter, nominal
allocation, rejection, zero fuel and persistence before publishing the trial.

2026-10-02, agent decision against `464f6aa`: pending Variable/Universe/Host
leaf descriptions still allocate structural Jobs whose sole step invokes an
existing Core constructor. Construct that syntax directly, without a new tag,
worker or proof; keep ordinary admission and genuinely pending source discovery.
Do not merge `DOMAIN_STRUCTURE_JOB` into domain acceptance merely to delete it:
its symbolic shape can close an effect cycle before the Context/type is checked.
Descriptive readiness and checked admission must remain distinct outcomes.

2026-10-02, agent decision against `0b9d421`: declared-type lookup still allocates
`DECLARED_TYPE_JOB` for an already checked Context. Return the existing borrowed
structure view for that case through one checked/pending API; delete the Job-only
entry point. Plain Context extensions also lend their actual type operand;
their immutable parent inputs form an acyclic DAG, not another discovery graph.
Keep pending source telescope discovery and its effect-cycle information,
ownership/judgement checks and binder identity. This is descriptive lookup, not
admission, new inference or permission to use `::` as synthesis input.

2026-10-02, agent decision against `ffd2fcf`: `OPERATION_REFERENCE_JOB` owns
no object witness or independent check. It waits for the existing lexical
producer, recursively allocates more reference Jobs and copies the operation
origin/status. Remove this role and its factory; use source-origin lookup for
descriptive signature discovery and await the original lexical producer before
accepting a Handler clause. Keep that producer's failed expectation/ownership
checks, nominal operation identity and cycle refusal. Test alias/quote/module
paths, pending signature discovery, zero fuel and invalid labels; no replacement
Task graph or Core-based recognition of operations.

2026-10-02, agent decision against `ffc5cb1`: admission lookup for inductive
formation, Match/induction, TypeCase, family Identity and substitution extension
still flattens already-owned receipts into temporary full premise arrays. Replace
those copies with a private synchronous borrowed-input reader in the existing
receipt interner; keep its exact receipt identity, sparse selections and all
checks. Typed operands and independent certificates are not interchangeable
with accepted receipts. This deletes copying, not the pending solver frontier
or every Evidence record. Verify exact alternate receipts, I/O and graph/fuel
censuses before recording completion; no new authority or wire format.

2026-10-02, agent implementation decision at `33b06f1`: do not add a second
Lambda/App constructor API merely to wrap existing interned Core builders.
Their provisional descriptions are not checking authority. Instead remove the
actual Fold clause copies: `fold_structure_state.clauses[]` and the checked
handler's temporary raw clauses. Both can borrow existing immutable inputs
through one synchronous Core constructor; retain only the unfinished cursor.
No new proof format, request graph, or acceptance bypass is authorized by this
decision. Fresh verification is required before recording this trial as done.

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

Latest static recheck, 2026-10-01, `4583ddd`: accepted `src/` and the
`solver_inputs` overlay on baseline `e716232` were inspected separately. The
uncommitted Context/IADT edits remain outside this refactor. The committed
prototype has deleted the completed Evidence adapter factory and all callers;
accepted `src/` still has that adapter. Prototype deletion is not production
promotion. The general classifier now borrows the canonical typed query directly;
unresolved operands retain discovery only, without copying its answer/completion.
Partial substitution and derivation workers still own genuinely unfinished
cursors. These observations supersede the adapter status in historical tables.
The classifier-ownership milestone below records fresh verification, including
the corrected export path and per-dispatch query fuel. Job and typed-query
dispatchers still remain separate, so this does not establish one frontier.
Full frontier/persistence and partition gates remain open; no promotion or SE1
completion follows.

2026-10-01 recheck at `7846f59`, including the uncommitted family-owner trial:
accepted `src/synthesis_function.c:classifier_step` still copies a typed query's
answer/status into its Job; the recorded prototype already removed that copy.
`src/evidence.c:pg_evidence` borrows its typed conclusion: it is not a recursive
copy of the Core Term. Concurrent accepted-source edits are excluded from trial
verification. The trial exposed a concrete remaining consumer defect:
`term_structure_step` read a family preparation's raw DONE status while its
actual output was pending, making the historical QuickSort provider unsupported.
Waiting on the actual output, rather than restoring answer copies, fixes that
reproduction; the family milestone below tracks its gates.

Agent assessment: the frontier references unfinished obligations, not another
Term/derivation graph. Store each obligation's inputs, result and actual cursor
once. Where a Job is currently the sole unfinished record, consolidate that
record rather than adding a Constraint beneath it. Where a Query already owns
the work, remove mirrored Job progress. Ready/wake indexes borrow these owners
and may be reconstructed once on load; do not scan the entire graph each step.
Saving only the frontier loses interrupted reduction/conversion progress.
Evidence admission is distinct from object witness Terms; premise edges already
recoverable from typed inputs should not be retained twice. A shared pointer or
lookup index alone is not a competing authority. This sharpens SE1-SE4, not a new
replacement graph, work list, or authorization to expand the wire format.

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

Latest static recheck, 2026-10-01, at `808c217f73a691b79d13aa05cba5662ddb157e7e`:
accepted `src/` and the recorded prototype assembled on `e716232` were inspected
separately, including concurrent Context/IADT edits without modifying them.
This supersedes the ownership table at `55d7781`. No tests were rerun for this
recheck; the verified reindex milestone below remains historical test evidence.

| Inspected operation | Accepted implementation | Committed prototype / remaining obligation |
| --- | --- | --- |
| `synthesis_derivation.c:evidence_ready` | Immediately DONE Job repeats the same checked Evidence in input/result. | Adapter factory and all callers are deleted. This does not finish SE1. |
| `synthesis_context.c:checked_query_step`; `synthesis_effect.c:inference_step` | Outer scheduling records mirror an already-owned query/effect completion. | These wrappers are removed; consumers borrow the existing progress owners. |
| `synthesis_function.c:classifier_step` | Job advances `pg_classifier_request` and copies its result/completion. | At `4583ddd`, known inputs borrow the canonical query; unresolved inputs retain discovery only. Single dispatch/frontier restoration remains open. |
| `synthesis_context.c:substitution_step`; `synthesis_derivation.c:derivation_step` | Partial map/next image or premise/comparison phase is held by the unfinished operation. | These are real cursors, not checked Term copies. No separate underlying constraint currently owns all this work. Consolidate the operation itself instead of deleting it or adding a parallel owner. |
| `synthesis_context.c:reindex_step` | Resolved inputs create a second checked-input Job; the outer Job copies its completion/result. | Forwarding Job is removed; existing receipts are borrowed directly and real unfinished admission follows the shared occurrence action. General classifier wrappers are removed at `4583ddd`; the complete frontier remains unresolved. |
| `evidence.c:prove_data_elimination` | Typed operands/maps/type retain structural inputs; the receipt also retains `count + 6` premise references. | Match/induction, constructor, request and Fold milestones below borrow existing typed inputs and retain differing exact receipt selections. This is not removal of every receipt or closure of SE3. |

2026-10-01 follow-up recheck at `17ff239` plus the IADT recovery trial below:
accepted `src/synthesis_work.c:pg_synthesis_forward` and the assembled prototype
still copy a child's result reference and completion into a forwarding Job.
`family_function_step` still forwards pending inputs to a checked-input worker.
This duplicates answer bookkeeping, not the pointed-to Term or proof itself;
no competing admission authority or current incorrect answer follows merely
from that pointer copy. The IADT trial removes recovery answer mirroring but
retains genuine normalization preparation. Do not treat wrapper deletion as
proof that the whole construction/frontier is unified.

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

**Frontier contract (agent assessment, 2026-10-01):** rebuilding the ready set is
enough to find which obligations can run, but is not the whole continuation.
Each existing construction/checking owner must retain its input references,
unresolved dependencies, interrupted cursor and solved result once. The frontier
and reverse wake index contain references only; they do not own copied results
or acceptance decisions. For example, a partially built substitution needs its
current map and next-image position even when readiness can be reconstructed.
Do not attach scoped typing progress to a shared untyped Core Term.

Clarification after inspecting `f4c4218` and the local classifier trial:
"constraint" here means an actual construction/checking obligation, not a new
table beneath the same Job. A query already owned by `typed_queries` is used
directly; a partial substitution has one record for its map, next image and
waiting dependency. The ready frontier borrows those records. Completed work
may retain its canonical answer for sharing, but an independently completed Job
above that answer is not justified by resumption. Do not assume a scan of Terms
can recover scoped obligations, or require a full scan on every Solve step.
Evidence must preserve checked admission and independent certificates; its
current separate container and recoverable premise arrays are not required by
that distinction. This is an agent architectural assessment, not a claim of
conflicting accepted answers or a completed implementation.

The general classifier is the next concrete ownership case: its Job waits for
producer inputs, then advances a canonical `pg_typed_query` and mirrors that
query's result/status. Consolidate the actual query after input resolution,
while retaining the unresolved construction and its provisional classifier
projection before acceptance. The latter feeds effect inference; making every
consumer wait for checked formation is not an equivalent replacement. A borrowed
result pointer alone is not a competing authority, and this static finding does
not demonstrate contradictory accepted answers. No additional Constraint/Job
database or full Evidence DAG is required by this ownership contract.

2026-10-01 recheck at `e9731a0` (agent observation): accepted
`synthesis_function.c:classifier_step` still mirrors the canonical typed query;
`synthesis_effect.c:inference_step` schedules an independently owned effect
solver. Neither observation establishes conflicting accepted results. The
uncommitted `solver_inputs/conversion_frontier_work` trial removes checked-input
forwarding from classifier normalization and post-synthesis assertion checking.
Its existing focused test log reports failure at `square_template_jobs`: after
checking against a pending type, requesting the same check against its resolved
type no longer finds the completed result immediately. This is a sharing
regression, not evidence that a second Job owner is necessary. Preserve that
test; consolidate request discovery as well as progress ownership. This
deletion-only trial is rejected; the resolved-request milestone below supersedes
it. This finding alone approves no new constraint table or lookup-alias layout,
and this initial recheck does not claim a fresh test run.

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

Clarified completion criterion: common frontier code borrows owners and invokes
their local operations; it does not enumerate or unpack every Oracle's semantic
payload. Remove the broad `source_work` union and corresponding central semantic
dispatch by consolidating construction/checking in their actual owning modules,
not by moving the same union into another file. Local rule distinctions remain
where they validate different theorems. This is an agent implementation direction
under the user's Oracle-locality requirement, not a claim that all continuation
state or all checking provenance can be deleted.

The next audit/implementation loop must test the user's Term/Occurrence resume
model against actual import and unfinished checking boundaries. Curry-Howard
supports keeping object proofs as Terms; it does not by itself identify a stored
typing annotation with successful checking or recover a suspended cursor. Keep
that necessary distinction on the existing typed/Oracle owners, without using
it to justify a second full derivation program or permanent generic Job graph.

Typed construction and its checking must use the same owner-local structural
operations. Keep provisional structure unaccepted until the ordinary rules
validate it. Preserve pending effect discovery, independent synthesis and `::`
as post-check only. Do not interpret a stored occurrence or a DONE byte as proof.

2026-10-01, next prototype trial (agent decision, parent `729d7d1`): application
result-type propagation has no independent consumer or semantic result. Keep
its interrupted source/type/check cursor on the existing application owner,
and delete the separately interned propagation Job, including no-work requests.
Expose the application's ordinary recipe before awaiting that cursor, to retain
pending effect discovery. Existing argument checking still decides acceptance;
concurrent equations must agree without replacing the Match's first input.
Verify the boundary and full regressions before publishing this deletion.
Historical observation before the next trial: the
[double-quote/open-Match probe](../src/prototype/solver_inputs/fixtures/nested-open-equation.p)
also stops with no runnable work on the parent binary (707 steps, requested
10,000) and candidate (706 steps). The outer quote's preparation waits
for a checked classifier before the application can deliver its equation.
This is an existing pending-structure cycle for SE2, not a demonstrated regression
or evidence that the separate propagation Job is necessary. Do not claim that
probe completes, supply its type through `::`, or restore a wrapper to hide it.

2026-10-01, agent decision on parent `071742c`: reuse the actual construction
found by polarity discovery. A Thunk introduction guarantees a U classifier
only if its ordinary checking succeeds; this suffices to preserve surface `&`
without checking its unfinished child twice. This is not early admission.
Body preparation validates its requested Context, then borrows its adapted
rule's output, not another result/completion copy. Both await entry points must
follow that existing output; preparation DONE does not mean checking DONE.
Focused tests pass, including rejected outputs and zero fuel. The default-policy
CLI probe is freshly pending at 706 steps on the parent and done at 991 on the
trial. This is progress past a cycle, not a speed comparison. O2 regression,
examples/acceptance, semantic persistence, seven checkpoint gates, C backend
and focused ASan/UBSan checks pass. The first namespace-body checkpoint run
failed because its test read the removed adapter result after checking the
public getter; correcting that stale test dereference and rerunning all seven
gates passed. Broad source state and public frontier restoration remain open.

2026-10-01, agent trial on `e58a1c8`: application equations now follow existing
Thunk/Lambda rule inputs, not a second AST interpretation or scope recovery.
Lambda Context/body come from the retained Pi/body input. Match alone keeps
an open result equation; ordinary checking and post-synthesis `::` still decide
acceptance. Focused tests pass, including two compatible callers plus a
conflicting caller on one Match, dependent codomain propagation, invalid
domains/quotes, zero fuel and split budgets. No Job, payload tag, result slot or
wire field is added. Full O2 regression/examples/acceptance, semantic persistence,
all seven checkpoints, five C-backend gates and focused ASan/UBSan/leak checks
pass. Fresh assembly matches 156 C/header files and passes synthesis/derivation
I/O; current-worktree Core/IADT/synthesis checks pass without staging user edits.
The default-policy CLI probe completes at 991 steps. This is a verified,
unpromoted prototype increment, not SE1-SE5 completion.
[Final-count measurements](../src/prototype/solver_inputs/recipe_equation_measurements.tsv)
retain the final graph and Job counts; List/effect/captured-block steps are
unchanged, QuickSort uses three more dispatches, not a speed improvement.
The QuickSort sample follows the source assembly command in Revision and Method;
the captured sample here is `tests/acceptance/captured-block-match.p`.
[File deltas](../src/prototype/solver_inputs/recipe_equation_delta.tsv): implementation
+22/-15 (net +7), tests +15/-1 (net +14), excluding patch context/documents.
The public partition gate still fails the same four reload cases at 100:100,
1000:1000, 1600:1600 and 2597:0; step 0 and in-memory/save-only partitions pass.
Broad source state, provisional construction and general frontier restoration
remain under the existing open work list. Logs use `/tmp/a-program-recipe-equation-`.

Object-language equality witnesses remain Terms. Removing duplicated metalevel
premises must not erase witness distinctions, conversion/reduction certificates,
nominal identity, scoped substitutions or the difference between checking and
executing an effect. Type/value judgement changes and `Thunk` formation are not
the same as a no-work Evidence adapter and cannot be deleted on that analogy.

2026-10-02, agent assessment on `2a0ba44` plus the source-Oracle prototype:
Lambda source preparation needs only its selected rule; its Context and body
already live on that rule's Pi/body inputs. Pi type-position normalization and
surface quotation still have genuine unfinished dependencies. Their private
states replace the broad central expression state, not the typing authority.
Source inspection and resume validation must compare the actual lexical key,
not require a particular implementation descriptor. A source-key capability on
static descriptors is an inspection contract, not a new Core/Oracle tag or
serialized acceptance flag. Plain structural-building queries still remain.

2026-10-02, agent implementation decision on `ba1e0d7`: Core-preserving
Context projection and type/value readings should borrow their input's
structural request, not retain a second structural result/Job. This does not
skip the ordinary rule's scope or judgement checks. Test exact sharing before
acceptance, long input chains, split/zero fuel and invalid rule rejection.
The first focused run exposed a stale symbolic snapshot on a fresh accepted
projection. Accepted inputs now read their typed data before borrowing raw
inputs; retained pre-closure views remain immutable. No alias graph/cache is added.

2026-10-02, agent decision on `7c3a1a1`: preparation and ordinary rule checking
still materialize temporary premise arrays from immutable inputs. Use borrowed
indexed readers at these call boundaries, following the existing request-key
reader pattern. Keep one rule dispatcher and the named checking rules; retain
temporary slices only where variable-arity rule APIs genuinely require them.
This removes copies, not the remaining preparation/checking owners or witness
Terms. Verify exact receipt selection, foreign/missing input rejection, large
arities, zero/split fuel and persistence before claiming the increment complete.
The first trial passed regression but used a fixed-size scratch Arena for
variable slices: the combined allocator census exposed 4,021,920 additional
QuickSort `calloc` bytes. Reject that allocation choice and use exact-size
temporary slices (none for an empty tail); rerun verification on the corrected
trial. A malloc-only decrease was insufficient evidence of an improvement.

2026-10-02, agent decision on `b3aa9e6`: rule import still scans the entire
dependency order for its maximum arity and copies each node's inputs into a
temporary array. Borrow the existing DAG/job map through the same synchronous
request-key reader instead. Delete that scan, array and copy loop; retain the
temporary topological map needed to relocate file-local pointers. No new
frontier, retained input graph or wire field is justified by this change.

2026-10-02, agent follow-up on `ca9a17c`:
mapped Context-projection receipts still retain a Context/source pair already
available on the typed occurrence. Test borrowing these exact stable admissions
with the existing sparse-selection layout. Retain ordinary variable-image
projections without an origin; preserve alternate source receipts. Do not extend
this automatically to reindexing: an identity action can return an occurrence
whose map/origin describe a different earlier action. Its current checking
inputs are not recoverable from that result alone.

2026-10-02, agent follow-up on `a5c617d`: the remaining central expression
state still declares unused `tail` and `function` pointers. The latter is read
only in a preparation guard and is never written; arena initialization makes
it always NULL. Delete both slots and simplify that guard without changing
discovery, checking, fuel or persistence. This is removal of stale state, not
proof that the remaining expression/Match/application cursors are unnecessary.

2026-10-02, agent decision on `7d432d9`: source Lambda/quotation, type assertions
and induction branches already select a lower checking owner. Borrow that
owner through the existing output contract instead of copying its receipt into
the preparation header. Retain preparation until its actual choice is stable:
a Lambda may still switch to logical-family abstraction after body discovery.
Migrate consumers to the existing result reader, preserve exact receipt/failure
selection and verify zero/split fuel, pending destruction and image invariance.
This does not remove those genuine discovery cursors or the remaining aliases.
The first trial exposed a direct `demand->callee->result` read in IH motive
discovery (`induction-index-environment.p`, fresh UBSan stack). Migrate that
consumer to the owner reader, not back to copied results or a nullable-proof
fallback. Add a small Lambda/IH demand regression with the borrowed-output tests.

2026-10-02, agent decision on `f459c0f`: source blocks still allocate both the
universal expression state and a separate block cursor, then copy the completed
tail receipt. Move the existing cursor/sequence construction into the CBPV owner
and borrow its final checking rule; remove the expression's block slot and
dispatch branch. Match demand scanning borrows visited lexical scopes through
that owner, without recreating them. Keep actual binding frames, ordered effects,
named-result truncation and post-synthesis assertions. This is prototype work,
not authorization to promote code or a claim that general resumption is solved.

## Plan

No new binding/domain checkpoint fields or backend features before this gate.
Implement prototypes, verify, and push reviewable deletion-oriented milestones.

- [x] **SE1 Context-map images (2026-10-03, parent `efc55b1`):** delete
  copied Core-value slots; borrow typed images, retaining the binder index.
  Dependent maps, raw duplicate-binder ordering, array/reader request reuse,
  zero/split fuel, owner lifetime and persistence tests pass. On the tested
  64-bit build, each map entry shrinks from 24 to 16 bytes; read access remains
  constant-time without another stored Core-value array or Context-list walk.
  [Census](../src/prototype/solver_inputs/map_images_measurements.tsv) is identical
  for five inputs at fuel 0/100/1000/completion.
  [Arena samples](../src/prototype/solver_inputs/map_images_allocation.tsv)
  decrease aligned requested bytes by 1,968/12,320/15,824/1,283,200/1,216 for
  List/Effect/captured/QuickSort/Handler. Capture/QuickSort call counts vary;
  these are partial cumulative samples, not peak RAM or a universal speedup.
  [Applied delta](../src/prototype/solver_inputs/map_images_delta.tsv):
  implementation +41/-28 (net +13), tests +28/-13 (net +15), excluding patch
  context/docs/reports. Full O2 regression/examples/acceptance, semantic persistence,
  seven checkpoint/five C gates and Core/Synthesis/Eval-I/O/Source-I/O
  ASan/UBSan/leaks exit 0. Fresh assembly matches all 156 C/header files;
  dirty-current Core/IADT/Synthesis also pass. All 52 List images/report equal
  the parent; the same three strict reload failures remain. Prototype only;
  SE1-SE5 stay open. [Hashes](../src/prototype/solver_inputs/map_images_inventory.tsv)
  pin tested files/inputs; logs use `/tmp/a-program-map-images-*.log`.

- [x] **SE1 Effect substitution inputs (2026-10-03, parent `c1d93d3`):**
  unify existing Core substitution construction on stable synchronous inputs;
  delete Effect's intermediate binding array and key scratch; retain the
  graph-owned completed result across lower work-store destruction.
  Array/reader sharing, shadowing, identity-prefix handling, null/invalid
  inputs, reader lifetime, zero/split fuel and destruction tests pass.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint,
  five C gates and focused Core/Synthesis/Eval-I/O/Source-I/O ASan/UBSan/leaks
  exit 0. Fresh patch assembly matches all 156 C/header files; dirty-current
  Core/IADT/Synthesis pass without including the user's edits in this increment.
  [Paired census](../src/prototype/solver_inputs/effect_images_measurements.tsv)
  is identical at fuel 0/100/1000/completion for five inputs. The deleted staging
  allocation used direct malloc, excluded from the
  [arena samples](../src/prototype/solver_inputs/effect_images_allocation.tsv);
  those samples establish no general speedup or total-memory saving.
  [Applied delta](../src/prototype/solver_inputs/effect_images_delta.tsv):
  implementation +68/-49 (net +19), tests +72/-24 (net +48), excluding patch
  context/docs/reports. [Hashes](../src/prototype/solver_inputs/effect_images_inventory.tsv)
  pin tested files/inputs. All 52 List images/report match the parent; the same
  three strict reload failures remain, not waived. Prototype only; SE1-SE5 open.
  Logs: `/tmp/a-program-effect-images-*.log`.

- [x] **SE1 Effect-row traversal increment (2026-10-02, parent `d1b8290`):**
  delete per-join child Jobs and the shape-discovery forwarding Job in
  `synthesis_effect.c`. One contribution demand walks the existing Term DAG
  through bounded `pg_dag_advance`; it allocates traversal storage only for
  joins and frees it on completion/failure/destruction. The synchronous DAG API
  uses the same traversal loop. No Core tag, proof authority or wire field added.
  [Shared-DAG probe](../src/prototype/solver_inputs/effect_frontier_sharing.tsv):
  depth 512 removes 514 of 515 Jobs (74,160 to 144 retained Job-layout bytes).
  Temporary DAG storage is excluded; this is not total-memory reduction.
  [Paired census](../src/prototype/solver_inputs/effect_frontier_measurements.tsv):
  completed Effect/Handler remove 40/15 Jobs, preserving all five inputs'
  final Term/Occurrence/Evidence/premise counts. The deep probe uses more steps
  (1,028 to 1,541): one transition now visits a child slot or closes a node.
  [Allocation samples](../src/prototype/solver_inputs/effect_frontier_allocation.tsv)
  have overlapping QuickSort ranges; no general speedup/regression established.
  [Applied delta](../src/prototype/solver_inputs/effect_frontier_delta.tsv):
  implementation +101/-34 (net +67), tests +86/-0, excluding the standalone
  probe, Make fragment, reports, docs and cumulative patch context.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint,
  five C and focused ASan/UBSan/leaks gates exit 0. Shared/deep joins, zero/split
  fuel, masks, malformed rows, sealing and pending destruction are covered.
  Fresh assembly matches all 156 source/test C/header files; dirty-current
  Core/IADT/Synthesis also pass. [Hashes](../src/prototype/solver_inputs/effect_frontier_inventory.tsv)
  pin the candidate and inputs. All 52 public List images/report match the parent;
  the same three strict reload failures remain (exit 1), not waived. SE1-SE5
  remain open. Prototype only; user edits excluded. Agent decision: lexical
  definition indices are actual source lookup state, not deleted merely because
  they reference pending producers. Logs use `/tmp/a-program-effect-frontier-`.
Do not mark a milestone complete just because a view hides the old representation.

2026-10-02, verified agent prototype on `2d802de`: ordinary rule checking now
consumes already checked premises through its existing monotonic cursor, yielding
only for unfinished inputs. Delete the empty per-premise self-requeue; the named
kernel rule still checks the conclusion. No owner, cache, trust policy or public
wire field is added. [Applied delta](../src/prototype/solver_inputs/rule_frontier_delta.tsv):
implementation +1/-3 (net -2), verification +52/-10 (net +42), excluding docs/data
and cumulative patch-file context. [Paired census](../src/prototype/solver_inputs/rule_frontier_measurements.tsv):
List/effect/captured graph/QuickSort remove 15/92/36/1,490 Jobs and
647/3,673/1,520/31,702 dispatches. QuickSort Job-layout storage falls by 231,088
bytes; this is not a total-memory or wall-time measurement. Typed Occurrence,
Evidence, query and logical/retained-premise counts are unchanged. Zero-fuel
counts match in all four samples; the initial and completed List images are
byte-identical to the parent.

The first regression exposed two timing-dependent fixtures: pending-Context
waiting used formerly empty dispatches, and competing Match equations assumed
request creation ordered their independently checked carriers. Keep actual
waiting via an unsealed effect input; establish the first Match carrier before
checking later agreeing/conflicting callers. Rejection assertions remain.
A checkpoint fixture also assumed a selected namespace had already started;
preserve its unstarted state and queued first dispatch instead. No production
codec is extended. Initial combined regression exited 2 at that fixture;
the corrected seven checkpoint/semantic gates exit 0 with exact remaining fuel
and bytes, including the newly exposed unstarted boundary. The other full O2
regression/examples/acceptance targets completed successfully, including both LT
providers/orders, general Sorted/permutation and ordinary-result proofs. Five C
gates and ASan/UBSan/leaks Core/synthesis/source/derivation/checkpoint pass.
Fresh patch assembly matches all tested source/test/checkpoint files and passes
focused checks. Current-worktree Core/IADT/synthesis checks pass; user edits are
excluded from the commit. Logs use `/tmp/a-program-rule-frontier-`.

Public strict List reload still fails at 1,000+1,000, 1,600+1,600 and complete+0
(now 1,955+0). The prior 100+100 mismatch disappears at this changed progress
boundary, not because general resumption was fixed. In-memory splits, inert
save/load and initial/completed byte equality pass; do not promote this to a
resume acceptance gate. Saved descriptions/completion flags remain untrusted.
SE1-SE5 remain open; prototype only.

2026-10-02, verified agent prototype on `112646d`: delete imported derivation
preparation Jobs, their cursor API, raw export branch and checkpoint format
branch. Single-root/batch imports share ordinary checking requests; temporary
transport relocation is not retained Solve state. Invalid transport dependencies
fail during import; invalid typing claims still reach ordinary checking.
Checkpoint format is single-owner `APGDRC\5`, without compatibility adapters.
[Applied delta](../src/prototype/solver_inputs/direct_import_delta.tsv):
implementation +94/-353 (net -259), verification +63/-98 (net -35), excluding
docs/data and cumulative patch-file context. [Fresh paired checks](../src/prototype/solver_inputs/direct_import_measurements.tsv)
reduce materialized validation 7/58/268/822 -> 3/33/160/505 and ordinary derivation
import 1,670 -> 1,326 transitions. These are dispatch counts, not wall time.
Four source census samples are unchanged at four budgets; all 52 public List
images and the four-failure resume report equal the parent. Full O2 regression,
examples/acceptance, semantic images, seven checkpoint and five C gates pass,
including both LT providers/orders and universal Sorted/permutation witnesses.
ASan/UBSan/leaks Core/synthesis/source/derivation/checkpoint pass. Fresh patch
assembly matches the tested code and passes Core/synthesis/source/checkpoint.
Its first assembly used the wrong layered patch location; that packaging error
was corrected without changing the tested implementation. The separate
current-worktree assembly passes Core/IADT/synthesis; user edits are neither
staged nor reverted.
SE1-SE5 remain open; prototype only. Logs use `/tmp/a-program-direct-import-`.

2026-10-02, verified agent prototype on `708c01d`: Identity formation/instance/
face discovery no longer allocates a second full request for its resolved exact
receipts. The existing resolved-key index shares the original work. Keys remain
immutable: a face's recovered formation is a different input, not a mutation of
the original key. Genuine formation/endpoint cursors remain; no additional
typed query, Oracle enumeration, witness representation or wire field is added.
Pending-first/checked-first sharing, zero fuel, higher faces and cancellation
pass. The first new fixture accidentally reused an already completed formation;
it was corrected to use a distinct, genuinely unprocessed checked input.
[Paired census](../src/prototype/solver_inputs/identity_requests_measurements.tsv):
QuickSort removes 18 Jobs and 36 transitions (798,331 -> 798,295); raw Job-layout
bytes decrease by 2,160, lookup-key aligned bytes increase by 576, with unchanged
bucket bytes and typed graph/query counts. Other three samples are unchanged.
This is not a peak-memory or wall-time claim. [Applied delta](../src/prototype/solver_inputs/identity_requests_delta.tsv):
implementation +27/-13, tests +35/-2; no net source-line reduction.
Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
and five C gates pass, including general Sorted/result/permutation and both LT
providers/orders. ASan/UBSan/leaks Core/synthesis/source/derivation I/O pass.
Fresh patch assembly matches the tested sources and passes Core/synthesis/
source I/O; the separate current-worktree assembly passes Core/IADT/synthesis.
All 52 List images and the public four-failure resume report equal the parent.
SE1-SE5 remain open; this is not promotion. Logs use `/tmp/a-program-identity-requests-`.

2026-10-02, agent trial on `6506828`, rejected before adoption: moving reindex
admission into an additional typed query reduces completed List Jobs by 7 but
adds 98 completed queries. Job/query layout bytes change 162,184 -> 171,424
at the same 2,602 steps; semantic counts are unchanged. That preserves another
completed input/result owner rather than removing it. Do not adopt the trial
or its API. Instead remove reindex receipt inputs already present in the typed
map/origin, retaining exact alternative selections and inputs absent from that
structure. Verify logical premises, ordinary checking and persistence unchanged.
Function-graph Jobs are not removed blindly: helper waits and source-interface
publication remain genuine work. Prototype scope; SE1-SE5 remain unfinished.

2026-10-02, verified agent prototype on `4b4d62e`: Context-substitution receipts
borrow source/destination/image receipts from the typed map. Exact alternative
selections and empty-Context inputs without a descriptive owner remain explicit.
The independent prefix proof is retained once in the existing rule-parameter
slot, included in the physical-reference census; no map history/query/tag/wire
field is added. All raw premise consumers now use the logical getter. Eight
boundary cases preserve logical order, sharing and index-disposal access.
[Paired census](../src/prototype/solver_inputs/context_receipts_measurements.tsv)
differs only in retained premise references: List/effect/captured/QuickSort remove
292/2,073/2,064/98,316 at completion; graph counts and steps are equal. No overall
memory/speedup claim. [Applied delta](../src/prototype/solver_inputs/context_receipts_delta.tsv):
implementation +89/-69 (net +20), tests +36/-0, excluding docs and patch context.
Fresh full O2 regression/examples/acceptance, semantic persistence, seven
checkpoint and five C gates pass, including both LT providers/orders and invalid
evidence. ASan/UBSan/leaks Core/synthesis/source/derivation, exact fresh assembly
and current-worktree checks pass. All 52 List images and the four-failure public
resume report equal the parent; SE1-SE5 remain open. Prototype only; user edits
are excluded. Logs use `/tmp/a-program-context-receipts-`.

- [x] **SE0 audit:** trace the four producer/consumer paths above; add a read-only
  census and pin the baseline. Audit measurements do not complete the refactor.
- [ ] **SE1 ownership before adapters:** enumerate Job roles and record their
  existing input/result owner, actual suspension state and consumers. Classify
  no-work adapters, forwarding aliases, queries with an existing progress owner,
  and genuinely unfinished construction. Try direct owner references plus
  owner-local suspension state; document any residual allocation that cannot be
  eliminated and why. An independent Job graph is not an acceptance criterion.
  Direct-body increment (2026-10-03, parent `4b46660`; agent decision): body and
  abstraction return checked/pending inputs through one interface. Checked
  operands and a checked matching Context select the ordinary RETURN rule
  directly; checked computations need no preparation Job. Pending operands and
  unfinished Context checks retain stable discovery identity and input edges.
  Lambda, block and Handler consumers borrow that output; no mirrored Term,
  semantic tag, checking authority, wire field or trust bypass is added.
  Focused O2, semantic/seven checkpoint, five C gates and ASan/UBSan/leaks pass;
  freshly assembled C/headers match. Current-user-edit Core/IADT/Synthesis pass
  without changing the user's working-tree files.
  The namespace checkpoint now walks the actual RETURN premise rather than
  assuming a body wrapper, preserving exact remaining dispatches and bytes.
  [Paired census](../src/prototype/solver_inputs/body_inputs_measurements.tsv)
  preserves all five final Term/Occurrence/Evidence and logical-premise counts.
  QuickSort removes 133 Jobs, 19,096 Job bytes and 371 dispatches; List removes
  3 Jobs and 9 dispatches. This is not a measured wall-clock speedup.
  [Applied deltas](../src/prototype/solver_inputs/body_inputs_delta.tsv):
  implementation +66/-54 (net +12), tests +81/-54 (net +27), excluding patches/docs.
  [Public partitions](../src/prototype/solver_inputs/body_inputs_partitions.tsv)
  still fail the same three reload cases; the gate exits 1, not waived.
  [Hashes](../src/prototype/solver_inputs/body_inputs_inventory.tsv) pin the
  prototype; logs use `/tmp/a-program-body-inputs-`. Full O2 regression/examples/
  acceptance exits 0, including both LT providers/orders, exact outputs and
  invalid-evidence rejection. The final boundary test also passes O2 and
  ASan/UBSan/leaks. SE1-SE5 remain open; no accepted-source promotion.
  Rejected trial: direct selection for pending operands broke the unchanged
  indexed-constructor-sequencing test. Completed classifier formation returns
  a checked operand, but callable-origin recovery reads only its pending source
  edge, exposing an existing scheduling-dependent provenance assumption. The
  accepted scope of this increment is checked operands only, not a repair of
  that broader issue. A focused test now retains a completed pending input's
  exact origin; the existing effectful partial-constructor gate passes again.
  Next SE1 prerequisite: obtain implicit-constructor calling conventions from
  their actual lexical/typed owner, independent of classifier-completion timing,
  before removing pending-body discovery. Do not add another Job/Term graph or
  reconstruct accepted Evidence solely to recover that source edge.
  Verified prototype increment (2026-10-03, parent `8b38099`; agent decision): address each named
  block binder by its existing lexical binding key (block syntax, statement
  ordinal, enclosing binders). Callable-origin lookup uses that source address,
  not classifier completion or reconstructed Evidence. No new Job kind, index
  or wire field is introduced. Known polarity selects the actual computation or
  ordinary RETURN rule, including pending operands; existing discovery keys stay
  stable. Unknown polarity and unfinished Context validation still need discovery.
  This supersedes the previous checked-only restriction, not SE1's remaining work.
  Full O2 regression/examples/acceptance, semantic/seven checkpoint and five C
  gates pass, including unchanged dependent/partial/effectful constructors and
  both LT providers/orders. ASan/UBSan/leaks Synthesis/source/derivation, fresh
  assembly and current-user-edit Core/IADT/Synthesis pass. Lexical-address tests
  cover inert registration, exact reuse, invalid slots and nested capture isolation.
  [Census](../src/prototype/solver_inputs/sequence_origin_measurements.tsv) preserves
  all five final Term/Occurrence/Evidence and logical-premise counts. QuickSort
  removes 114 Jobs, 16,416 Job-layout bytes and 215 dispatches, but adds 18 lexical
  binding records and 178 enclosing-binder references. [Images](../src/prototype/solver_inputs/sequence_origin_images.tsv)
  grow 1,704 bytes for QuickSort and 168 for effects; three other images are equal.
  These are source allocation addresses, not copied types/proofs. No total-memory,
  file-size reduction or wall-clock speedup is claimed. [Applied deltas](../src/prototype/solver_inputs/sequence_origin_delta.tsv):
  implementation +36/-12 (net +24), tests +52/-11 (net +41), audit +14/-3 (net +11).
  [Public partitions](../src/prototype/solver_inputs/sequence_origin_partitions.tsv)
  still fail reload 1000+1000, 1600+1600 and completed+0; exit 1 is not waived.
  [Hashes](../src/prototype/solver_inputs/sequence_origin_inventory.tsv) pin inputs;
  logs use `/tmp/a-program-sequence-origin-`. SE1-SE5 stay open; prototype only.
  Follow-up (2026-10-03, parent `e8e4c4d`): delete the now-unused
  `pg_synthesis_result_context_input` reverse reader and its declaration. Only
  inspection assertions remained; actual Context/type/effect checks stay covered.
  Implementation -13 lines, tests -5. Fresh patch assembly, O2 check/examples,
  constructor sequencing, semantic/source/namespace-body/constructor checkpoints
  and ASan/UBSan/leaks Synthesis pass. Full acceptance is the preceding increment's
  verified result, not a fresh run for this dead-API deletion. Prototype only.
  Verified SE1 increment (2026-10-03, parent `bc08ac0`): Context binding borrows
  an already checked producer directly, keeping its pending key. Existing
  validation owners remain stable; unresolved or invalid pending inputs still
  require exact parent/Binder validation. No cached Context or new graph is added.
  First-use/reuse, invalid input and zero-fuel tests pass; full O2 regression/
  examples/acceptance, semantic/seven checkpoint and five C gates, ASan/UBSan/leaks,
  fresh assembly and current-user-edit Core/IADT/Synthesis pass. Unrelated edits
  remain untouched. [Counts](../src/prototype/solver_inputs/scope_inputs_ready_measurements.tsv)
  agree at every sampled fuel on five inputs; [images](../src/prototype/solver_inputs/scope_inputs_ready_images.tsv)
  are byte-identical. No benchmark improvement is claimed. [Applied delta](../src/prototype/solver_inputs/scope_inputs_ready_delta.tsv):
  implementation -2, tests +17. [Public partitions](../src/prototype/solver_inputs/scope_inputs_ready_partitions.tsv)
  still fail the same three reload cases (exit 1). [Hashes](../src/prototype/solver_inputs/scope_inputs_ready_inventory.tsv)
  pin the trial; logs use `/tmp/a-program-scope-inputs-ready-`. SE1-SE5 stay open;
  prototype only, not accepted-source promotion or completion of Job removal.

- [x] **SE1 Context cursor consolidation (2026-10-03, parent `5786038`;
  agent implementation decision):** remove Constructor partial-application
  `scopes[]` and its duplicate count; abstract the same pending Context parent
  chain, bounded by the existing callable's remaining-index count. Substitution
  retains only its current checked/pending map input, not a copied pair result
  and completed header result. Its forward declaration order is temporary
  scratch, released on completion/failure/destruction; repeated reverse scans
  would instead make dependent checking quadratic. No new semantic owner, tag,
  checking rule or wire field is introduced. Focused Synthesis/source-I/O tests
  pass, including one/two omitted indices, partial-call reuse, split dispatch,
  retained-map lifetime and abandoned pending work. Full O2 regression/examples/
  acceptance exits 0, including general Sorted/permutation/result connection and
  both LT providers/orders. Semantic/seven checkpoint and five C gates pass;
  ASan/UBSan/leaks Synthesis/source-I/O, fresh assembly and current-user-edit
  Core/IADT/Synthesis pass. The unrelated user files remain unchanged.
  [Paired census](../src/prototype/solver_inputs/context_cursor_measurements.tsv)
  preserves Term/Occurrence/Evidence, Jobs, steps, premise and query counts on
  five inputs at all 20 sampled budgets. QuickSort loses 33 copied header result
  references and 264 Job-layout bytes. Those bytes exclude the removed Constructor
  scope arrays and temporary Substitution scratch; no total-memory or speedup
  claim is made. [All five images](../src/prototype/solver_inputs/context_cursor_images.tsv)
  are byte-identical to the parent. [Applied deltas](../src/prototype/solver_inputs/context_cursor_delta.tsv):
  implementation +46/-26 (net +20), tests +50. Stored duplication is removed,
  but resource-lifetime callbacks mean this increment does not reduce source lines.
  [Public partitions](../src/prototype/solver_inputs/context_cursor_partitions.tsv)
  still fail reload 1000+1000, 1600+1600 and completed+0 (exit 1), not waived.
  [Hashes](../src/prototype/solver_inputs/context_cursor_inventory.tsv) pin the
  frozen trial; logs use `/tmp/a-program-context-cursor-`. SE1-SE5 remain open;
  prototype only, not accepted-source promotion or complete Job/Evidence removal.
- [x] **SE1 lexical Context contracts (2026-10-03, parent `7a6d0f8`;
  agent implementation decision):** an exact ordinary/family Context-extension
  rule already checks its parent and binder. A source binding can borrow that
  pending input without a second `SCOPE_CONTEXT_JOB`, provided its parent input
  and binder match exactly and no separate IH association is required. This is
  not early acceptance: failed Context formation still rejects the consumer.
  Opaque producers and association checks retain their actual obligation;
  existing validation keys stay stable. Inertness, exact reuse, invalid
  domain/parent/binder, family extension and delayed-parent tests pass, with
  chunks 1/64. Full O2 regression/examples/acceptance exits 0, including general
  Sorted/permutation/result connection and both LT providers/orders. Semantic
  persistence/seven checkpoint and five C gates, ASan/UBSan/leaks, fresh assembly
  and current-user-edit Core/IADT/Synthesis pass; unrelated files are unchanged.
  [Paired census](../src/prototype/solver_inputs/scope_contract_measurements.tsv):
  completed Term/Occurrence/Evidence, rule, logical/retained premise and query
  counts agree on all five inputs. QuickSort loses 19 Jobs, 27 dispatches and
  2,592 Job-layout bytes; effects lose 48/93/6,632, handler 18/33/2,488.
  Partial-fuel progress can differ after removing dispatches; step 0 agrees.
  These are not total-memory or wall-clock speedup measurements.
  [Five images](../src/prototype/solver_inputs/scope_contract_images.tsv) are
  byte-identical to the parent. [Applied deltas](../src/prototype/solver_inputs/scope_contract_delta.tsv):
  implementation +24/-7 (net +17), tests +85/-6 (net +79), not patch/doc lines.
  Stored duplicate validation is reduced, not total source size.
  [Public partitions](../src/prototype/solver_inputs/scope_contract_partitions.tsv)
  still fail reload 1000+1000, 1600+1600 and completed+0 (exit 1), not waived.
  [Hashes](../src/prototype/solver_inputs/scope_contract_inventory.tsv) pin the
  candidate; logs use `/tmp/a-program-scope-contract-`. No replacement scope
  state, pending class, Term graph, wire field or acceptance authority is added.
  SE1-SE5 remain open; prototype only, not accepted-source promotion or complete
  Job/Evidence removal. Static recheck also confirms basic Lambda/App/unary
  introduction receipts already borrow typed children without another premise
  array; they are not a newly discovered duplicate graph.
  Verified prototype increment (2026-10-03, parent `0e3865d`; agent decision):
  normalization now uses the existing resolved-input interner after operand
  validation. Equivalent checked inputs share the checking owner, without
  changing pending keys, WHNF/NF mode, force demand or exact receipt/scope keys.
  Handler checking borrows one validated clause snapshot rather than allocating
  a flattened premise array; exact selections and one-read callbacks remain.
  SE4 deletion in the same increment: source import formerly retained the raw
  derivation DAG after copying its requests into ordinary checking owners.
  The existing restore scratch now owns those raw headers/edges; restoration
  discards them before Solve. Referenced Terms/Contexts/induction allocations
  retain their actual semantic lifetime. No new graph, Job role, proof store,
  wire field, accepted-source promotion or trust policy is added.
  Deep 2,048-link import survives scratch destruction before Solve, preserving
  shared roots and invalid-input rejection; recursive/effect I/O also passes.
  Full O2 regression/examples/acceptance exits 0, including general QuickSort
  Sorted/permutation/result connection and both LT providers/orders. Semantic
  persistence, seven checkpoint and five C gates, focused ASan/UBSan/leaks and
  fresh/integration Core/IADT/Synthesis pass. Integration preserves unrelated
  user edits; only its test copy adapts overlapping Schema API call sites.
  [Paired census](../src/prototype/solver_inputs/checked_owners_measurements.tsv)
  agrees on all five inputs at every sampled fuel. [Import allocation](../src/prototype/solver_inputs/checked_owners_import_allocation.tsv)
  removes 72 external persistent-arena requests / 7,552 requested bytes on the
  same QuickSort image, 39 / 5,968 on List including step 0. These exclude
  initialization, graph.c internals and discarded scratch; not RSS/peak memory
  or smaller files. All 52 List images/report equal the parent byte-for-byte;
  the same three public reload failures remain (exit 1), not waived.
  [Actual deltas](../src/prototype/solver_inputs/checked_owners_delta.tsv):
  implementation +61/-37 (net +24), tests +116/-9 (net +107), separate audit
  harness/build +60. No general speedup or total code-size reduction is claimed.
  [Hashes](../src/prototype/solver_inputs/checked_owners_inventory.tsv) pin the
  candidate and inputs; logs use `/tmp/a-program-checked-owners-`.
  SE1-SE5 remain open. The selected import-root array still has permanent
  lifetime although source restoration needs only a temporary relocation map;
  inspect/remove that residual under SE4, not a new parallel work list.
  Resolved by SE4's subsequent 2026-10-03 relocation-lifetime increment below.
  Prototype increment (2026-10-02, parent `486d64d`; agent decision): delete
  `CONSTANT_MOTIVE_JOB` and its receipt/status wrapper. Function's stateless
  helper returns the canonical constant-result request; Match owns its actual
  strategy fallback. Dependent classifiers reject that candidate, not the term.
  Constant-result telescope closing follows existing Scope parent edges instead
  of allocating a second array, and borrows the Scope's checked output.
  [Paired census](../src/prototype/solver_inputs/constant_owner_measurements.tsv)
  preserves completed Term/Occurrence/Evidence and premise counts on five inputs.
  QuickSort removes 65 Jobs, 130 dispatches, 9,360 Job-layout bytes and 229 result
  references; captured Match removes 6 Jobs and 11 dispatches. This is not a
  total-memory or speedup claim: the [allocation/time probe](../src/prototype/solver_inputs/constant_owner_allocation.tsv)
  has process variance and ran alongside regression tests.
  [Applied deltas](../src/prototype/solver_inputs/constant_owner_delta.tsv):
  implementation +42/-65 (net -23), tests +20/-6 (net +14), excluding patch/docs.
  Focused O2, ASan/UBSan/leaks, semantic persistence, seven checkpoint and five C
  gates pass. Full O2 regression/examples/acceptance exits 0, including general
  Sorted/permutation/result connection, both LT providers/orders and invalid
  proof rejection. Fresh assembly matches 128
  C/header files and Synthesis tests; fresh/current-worktree focused checks pass.
  [Hashes](../src/prototype/solver_inputs/constant_owner_inputs.tsv) pin sources/binaries.
  All 52 public List images/report match the parent; the same three reload
  failures remain (exit 1), not waived. SE1-SE5 remain open; prototype only,
  unrelated user edits excluded. Logs use `/tmp/a-program-constant-owner-final-`.
  Verified prototype increment (2026-10-02, parent `2bebfca`; agent decision): delete
  `SOURCE_RETURN_HANDLER` state/dispatch and use the existing sequencing owner.
  Eight preparation roles borrow their checking owner's output rather than copy
  its receipt. Keep actual Context/carrier/index checks and genuine nullary
  constructor admission; preparation alone exposes no accepted output.
  [Paired census](../src/prototype/solver_inputs/return_owner_measurements.tsv)
  preserves completed Term/Occurrence/Evidence counts. QuickSort loses 369
  copied receipt references, not Jobs or Job-layout bytes. Handler loses one
  dispatch; its total Job count is unchanged and layout bytes increase by 16.
  [Role census](../src/prototype/solver_inputs/return_owner_roles.tsv) shows the
  deleted alias and changed discovery allocations; no overall memory/speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/return_owner_delta.tsv):
  implementation +39/-39 (net 0), tests +70/-15 (net +55), excluding patch/docs.
  Focused O2, ASan/UBSan/leaks, semantic persistence, seven checkpoint and five C
  gates pass. Full O2 regression/examples/acceptance exits 0, including ordinary
  QuickSort-result/Sorted/permutation and both LT providers/partition orders.
  Fresh assembly matches 128 C/header files and Synthesis tests; dirty-current
  Core/IADT/Synthesis pass. [Hashes](../src/prototype/solver_inputs/return_owner_inputs.tsv).
  All 52 public List images/report match the parent; the same three reload
  failures remain (exit 1), not waived. SE1-SE5 remain open; prototype only,
  unrelated user edits excluded. Logs use `/tmp/a-program-return-owner-`.
  Verified prototype increment (2026-10-02, parent `b54fdcb`; agent decision):
  collapse structural/checked Telescope work into one cursor owner; delete the
  second role/factory, Schema's duplicate pointer and copied Context receipt.
  Existing preparation notification exposes lexical Scope/tail; full completion
  alone borrows checked Context output. No replacement graph, tag or wire field.
  Reuse, step-0 inertness, cyclic domains, preparation/completion waiters, rejected
  domains and reserved-binder arity/conflict tests pass. Full O2 regression,
  examples/acceptance, semantic persistence, seven checkpoint and five C gates,
  focused ASan/UBSan/leaks and fresh/dirty-current checks pass. Fresh assembly
  matches 128 C/header files and Synthesis tests; unrelated user edits excluded.
  [Paired counts](../src/prototype/solver_inputs/telescope_owner_measurements.tsv)
  retain completed Term/Occurrence/Evidence counts on five inputs. QuickSort
  removes 68 Jobs/result references, 7,224 Job-layout bytes and 115 dispatches.
  [Applied deltas](../src/prototype/solver_inputs/telescope_owner_delta.tsv):
  implementation +54/-59 (net -5), tests +81/-24 (net +57), excluding patch/doc
  lines. [Input/binary hashes](../src/prototype/solver_inputs/telescope_owner_inputs.tsv).
  All 52 public partition images match the parent after mapping the completion
  budget 1950->1939. The same three reload failures remain (exit 1), not waived;
  the report differs only in that completion budget/used fuel. SE1-SE5 remain
  open; prototype only. Logs use `/tmp/a-program-telescope-owner-`.
  Verified prototype increment (2026-10-02, parent `464f6aa`; agent decision): Variable,
  Universe and Host leaf descriptions use existing Core constructors directly;
  delete their no-work structural dispatches. Ordinary admission still rejects
  invalid premises and Universe overflow; descriptive visibility grants no proof.
  Repeated queries allocate no Job/Occurrence/receipt or fuel. Known non-Comp
  Effect inputs are refused immediately, not wrapped in a delayed rejection Job.
  O2 Synthesis/source-I/O, semantic persistence, seven checkpoint and five C
  gates, focused ASan/UBSan/leaks and dirty-current Core/IADT/Synthesis pass.
  Full O2 regression/examples/acceptance exits 0, including universal Sorted,
  permutation/result connection, both LT providers/orders and invalid proofs.
  Fresh assembly matches 128 active C/header files and Synthesis tests.
  [Paired counts](../src/prototype/solver_inputs/leaf_structure_measurements.tsv)
  preserve completed Term/Occurrence/Evidence counts on five inputs; QuickSort
  loses 10 Jobs/1,440 layout bytes/18 steps, Handler loses 1 Job/144 bytes/2 steps.
  [Applied deltas](../src/prototype/solver_inputs/leaf_structure_delta.tsv):
  implementation +16/-18 (net -2), tests +48/-2 (net +46), excluding patches/docs.
  [Pinned hashes](../src/prototype/solver_inputs/leaf_structure_inputs.tsv).
  All 52 public partition images/report equal the parent; the same three reload
  failures remain (exit 1), not waived. SE1-SE5 stay open; unpromoted prototype.
  Logs use `/tmp/a-program-leaf-structure-`. Keep genuinely pending discovery
  and Domain's pre-admission shape needed to close effect cycles; do not replace
  these with another acceptance authority merely to remove a class name.
  Verified prototype increment (2026-10-02, parent `0b9d421`; agent decision): checked
  declaration lookup borrows its Context field; pending plain extensions borrow
  their actual type operand, walking immutable parents without per-ancestor Jobs.
  One checked/pending API replaces both Job-only entries; Variable/Pi readers
  consume that view. Pending source telescope discovery remains, not a replacement
  worker or acceptance authority. Repeated checked/128-deep pending lookup,
  zero-fuel inertness, foreign/mixed/wrong-judgement inputs and wrong-scope Context
  rejection pass. Synthesis/source-I/O, semantic persistence, seven checkpoint
  and five C gates, focused ASan/UBSan/leaks and dirty-current Core/IADT/Synthesis
  pass. Full O2 regression/examples/acceptance exits 0, including general
  Sorted/permutation/result connection, both LT providers/orders and invalid proofs.
  Fresh assembly matches 128 active C/header files and the Synthesis test.
  [Paired counts](../src/prototype/solver_inputs/declared_type_measurements.tsv)
  preserve completed Term/Occurrence/Evidence counts on five inputs. QuickSort
  loses 189 Jobs/27,016 layout bytes/87 steps; Effect loses 131/18,600/168.
  [Applied deltas](../src/prototype/solver_inputs/declared_type_delta.tsv):
  implementation +27/-26 (net +1), tests +53/-10 (net +43), excluding patches/docs.
  [Input/binary hashes](../src/prototype/solver_inputs/declared_type_inputs.tsv)
  pin the comparison. All 52 public partition images/report equal the parent;
  the same three reload failures remain (exit 1), not waived. SE1-SE5 remain
  open; prototype only. Logs use `/tmp/a-program-declared-type-`.
  Verified prototype deletion (2026-10-02, parent `ffd2fcf`; agent decision): remove
  `OPERATION_REFERENCE_JOB`, its private state, factory and input adapter.
  Handler discovers the nominal signature through existing source-origin links
  and awaits the original lexical producer; lookup creates no Job, Term or
  receipt and spends no fuel. Failed assertions and non-label functions/apps
  reject; foreign signature owners and cyclic origin chains are not recognized.
  O2 Synthesis/source-I/O, semantic persistence, seven checkpoint and five C
  gates, focused ASan/UBSan/leaks and dirty-current Core/IADT/Synthesis pass.
  Full O2 regression/examples/acceptance also exits 0, including general
  Sorted/permutation/result connection and invalid-proof rejection. A fresh patch
  assembly matches the frozen trial's 128 active C/header files and Synthesis tests.
  [Paired counts](../src/prototype/solver_inputs/operation_origin_measurements.tsv)
  retain completed Term/Occurrence/Evidence counts on all five inputs. Effect
  loses 11 Jobs/1,320 layout bytes and 31 steps; Handler loses 5 Jobs/600 bytes
  and 13 steps. Mid-budget scheduling changes are recorded, not assumed equal.
  [Applied deltas](../src/prototype/solver_inputs/operation_origin_delta.tsv):
  implementation +7/-81 (net -74), tests +46/-41 (net +5), excluding patches/docs.
  [Input/binary hashes](../src/prototype/solver_inputs/operation_origin_inputs.tsv)
  pin the comparison. All 52 List partition images/report equal the parent;
  the same three public reload failures remain (exit 1), not waived. The first C
  invocation used nonexistent `check-c-linker`; the corrected five-target batch
  uses `check-c-link` and exits 0. Logs use `/tmp/a-program-operation-origin-`.
  SE1-SE5 remain open; no production promotion or new Task graph/format.
  Verified prototype (2026-10-02, parent `fa6c916`, agent decision): delete
  `FAMILY_DOMAIN_JOB`, its factory and result copy. Logical-family application
  yields the existing parameter query and passes its exact declaration to the
  ordinary post-check. Its existing local phase avoids repeating argument
  discovery/alpha scans while the query is suspended; no private pointer,
  query class, acceptance owner or wire field is added. A non-alpha but
  convertible domain succeeds at chunks 1/7/64; incorrect domains reject,
  selected/partial/projected families and zero fuel pass. Full O2 regression,
  examples/acceptance, semantic persistence, seven checkpoint and five C gates
  pass. Additional conversion boundary tests pass separately on the same
  implementation, ASan/UBSan/leaks and fresh/current-worktree Core/IADT/synthesis
  pass. All 388 reassembled C/header files match the final tested assembly.
  [Paired fixture counts](../src/prototype/solver_inputs/family_domain_census.tsv),
  using O0 GDB on identical tests, show 1/2/2 fewer new Jobs after requesting
  three sequential calls; steps are 37/15/15 versus 37/12/12. This is not a
  wall-time or general-memory claim. All fields in the five-input census equal
  the preceding `typed_owner` candidate rows, including zero fuel; input hashes
  are unchanged. All 52 public partition images/report equal the parent; the
  same three reload failures remain, not waived. The first added fixture used
  bare `@` in argument position and failed parsing; `D (@)` is the corrected
  syntax, not an implementation change. [Applied deltas](../src/prototype/solver_inputs/family_domain_delta.tsv):
  implementation +38/-37 (net +1), tests +49/-5 (net +44). Do not merge telescope
  discovery with acceptance: scope/tail consumers run before Context checking.
  Logs: `/tmp/a-program-family-domain-`. SE1-SE5 remain open; prototype only,
  unrelated user changes excluded.
  Verified prototype increment (2026-10-02, parent `9e9377b`; agent decision):
  delete the dedicated Reindex worker and action-pointer cache. Convenience and
  ordinary `PG_REINDEX` requests now share one rule owner and checker, borrowing
  the canonical Occurrence action with at most one transition per dispatch.
  Exact alternative receipts remain distinct; action completion is not proof
  admission. Cold/warm sharing, zero fuel, cancellation, invalid scopes/arity
  and endpoints pass. The restricted rule codec captures unstarted/completed
  rules but refuses an interrupted action whose cursor it cannot transport.
  A new test's initial assumption that failed capture clears its output pointer
  was corrected to the actual unchanged-output behavior, without changing the
  implementation. O2 full regression/examples/acceptance, semantic persistence,
  seven checkpoint and five C gates pass, including general Sorted/permutation
  and both LT providers/orders. Focused ASan/UBSan/leaks and fresh/current
  synthesis/IADT tests pass; all 156 final assembled C/header files match the
  tested prototype. [Census](../src/prototype/solver_inputs/reindex_rule_measurements.tsv)
  preserves every field except rule classification and Job-layout bytes:
  List/effect/captured-Match/QuickSort increase by 224/160/352/7,680 bytes.
  The removed private pointer does not outweigh the ordinary rule's key/state;
  this is path unification, not memory saving or a speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/reindex_rule_delta.tsv):
  implementation +50/-45 (net +5), tests +83/-5 (net +78), excluding patch
  context and docs. All 52 public partition images and the three-failure report
  equal the parent; the strict reload gate still fails, not waived. No new
  graph, tag, format or trust policy is added. SE1-SE5 remain open; prototype
  only, user edits excluded. Logs use `/tmp/a-program-reindex-rule-`.
  Verified prototype (2026-10-02, agent decision, parent `134735d`): replace the
  dedicated substitution `PAIR_JOB` and its checking cursor with the existing
  `PG_CONTEXT_SUBSTITUTION` rule request. Its four premises are the extension,
  destination, prefix and post-checked image. Reindexing and post-synthesis
  conversion remain actual obligations, not duplicated pair progress. This
  removes an upper-layer constructor without adding an owner, rule or format.
  Repeated requests, dependent images, rejection and zero/split fuel pass.
  [Applied deltas](../src/prototype/solver_inputs/substitution_rule_delta.tsv):
  implementation +12/-23 (net -11), tests +12/-1 (net +11), excluding patch
  context and docs. [Paired census](../src/prototype/solver_inputs/substitution_rule_measurements.tsv):
  completed List/effect/captured-block/QuickSort retain the same typed graphs
  and logical/physical proof inputs. Job-layout bytes increase by
  504/544/648/15,720; steps change by -5/+2/-3/-100. No memory/speedup claim:
  adoption removes an independent constructor/checking cursor, not every Job.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
  and five C gates pass, including general Sorted/result/permutation and both
  LT providers/orders. ASan/UBSan/leaks Core/synthesis/source/derivation I/O and
  checkpoint, exact fresh assembly and current-worktree Core/IADT/synthesis
  checks pass. Public reload still has three failures (terminal cut now 1,950);
  seed and completed images equal the parent. No format or trust policy changes.
  Source/typed-query frontier and public resume remain open; SE1-SE5 incomplete.
  Prototype only; user edits excluded. Logs use `/tmp/a-program-substitution-rule-`.
  For each retained record, identify the unfinished obligation that has no
  other owner. Direct-input migration alone does not discharge this requirement;
  a renamed task graph or one mutable status per Core Term also fails it.
  A common descriptor interface alone also fails it if a shared upper-layer
  union or dispatcher still re-expands the Oracle payloads. Verify owner-local
  semantic handling and removal of the broad `source_work` layout, not just
  disappearance of an enum or reduction of the Job count.
  Verified prototype increment (2026-10-02, agent decision, parent `076a35f`):
  remove Evidence's `typed_recipe_query` union and its cross-role initializer.
  The eleven query roles retain only actual suspension fields: classifier has
  one input pointer, phase checking has no private state. Composition borrows
  the calling owner's cursor instead of casting unrelated states to a common
  layout. Request keys, chosen receipts, dependency and transition order remain;
  no object witness, acceptance owner, semantic tag or wire field is added.
  [Private layouts](../src/prototype/solver_inputs/typed_owner_layout.tsv) are
  verified on the 64-bit sanitizer build (formerly 104 bytes for every role).
  Zero/split fuel, interrupted scope/selection/inductive queries, zero-state
  phase reuse and compact classifier/origin/rebase tests pass. Full O2
  regression/examples/acceptance, semantic persistence, seven checkpoint and
  five C gates, ASan/UBSan/leaks Core/synthesis/source/derivation I/O and
  fresh/current-worktree checks pass. All 388 reassembled C/header files match.
  [Paired census](../src/prototype/solver_inputs/typed_owner_measurements.tsv)
  changes only query-layout bytes on five inputs at fuel 0/100/1000/completion;
  completed QuickSort saves 949,336 bytes, not a peak-RAM or speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/typed_owner_delta.tsv):
  implementation +153/-82 (net +71), tests +23/-0; no source-line reduction.
  All 52 List images/report equal the parent; the same three public reload
  failures remain, not waived. [Inputs](../src/prototype/solver_inputs/typed_owner_inputs.tsv)
  pin hashes. Logs: `/tmp/a-program-typed-owner-`. SE1-SE5 remain open;
  prototype only, unrelated user edits excluded.
  Verified upper-layout deletion (2026-10-02, agent decision, parent `30bf913`):
  the broad `source_work`, `EXPRESSION_JOB` and shared semantic `step` are
  removed. Private Match/reference/module/graph-reference/return-sequencing
  owners keep only their actual suspension data (64/40/40/16/8 bytes on this
  build, excluding the common header and separately owned queries/cases).
  Scope/syntax borrow the canonical key; source-rule inspection delegates to
  owner projections instead of repeating central semantic dispatch. Module
  selections cannot receive constructor member allocations. Exact registration,
  preparation-before-effect-checking and `::` post-check contracts remain.
  [Paired census](../src/prototype/solver_inputs/source_owners_measurements.tsv):
  only Job-layout bytes change at fuel 0/100/1000/completion. Completed
  List/effect/captured-graph/QuickSort save 4,728/13,728/11,336/327,656 bytes;
  Job counts, typed counts, receipts, queries and steps are unchanged. This is
  not peak RAM or a speedup measurement. [Applied deltas](../src/prototype/solver_inputs/source_owners_delta.tsv):
  implementation +289/-226 (net +63), tests +52/-1 (net +51), excluding docs
  and patch context; this is not net code reduction. O2 regression/examples/
  acceptance, semantic persistence, seven checkpoint and five C gates exit 0,
  including general Sorted/result/permutation and both LT providers/orders.
  ASan/UBSan/leaks synthesis/source/derivation I/O, fresh-assembly checks and
  current-worktree Core/IADT/synthesis/source I/O pass. An initial missing
  startup enqueue was fixed; the first full run was stopped after an invalid
  new parser fixture was identified, then rerun successfully with the corrected
  fixture. All 52 strict List images and the four-failure partition TSV equal
  the parent. The public reload gate still fails, not waived. Evidence admission,
  remaining stateful forwarding and resumable query/frontier ownership remain
  open: this does not complete SE1-SE5 or remove every Job/Evidence record.
  Prototype only; user edits are excluded. Logs use `/tmp/a-program-source-owners-`.
  The frontier borrows canonical unfinished obligations; only those obligations
  own inputs, interrupted cursors and results. Save the necessary cursor and
  deterministic scheduling information, not a second dependency program.
  Evidence admission may move onto typed owners: retain independent checking
  information, not a mandatory separate Evidence graph. Merely adding a common
  header while retaining both stateful wrappers fails this ownership gate.
  Check completed-frame retention as well as pending ownership: preserve exact
  answer sharing without keeping a second completed Job for that answer. Queue
  membership and reverse wake indexes borrow owners; they do not replicate the
  typed/Term graph or own a parallel acceptance state. A query reference resolving
  an input-discovery record is not closure of this gate until persistence and
  all consumers distinguish discovery completion from query completion.
  Then migrate the dependency interface and its callers
  so accepted Evidence needs no `EVIDENCE_JOB`, scheduler status or duplicate
  result slot. Remove the adapter factory/role, then redundant producer-to-
  checked forwarding where the same operation is otherwise duplicated. Retain
  exact sharing, ownership and failure contracts. In particular, a request made
  through pending producers and a later request through their exact checked
  results must reuse the same operation without allocating another full owner;
  equal erased Core alone must not merge distinct scoped/proof inputs.
  Verified source output increment (2026-10-02, parent `7d432d9`): Lambda/quotation,
  source/binding assertions and induction branches borrow their selected
  checking owner through the existing output contract. Forwarding no longer
  fills their second result slot. Retain actual preparation and failure checks;
  no new output graph, tag, wire field or acceptance authority is added.
  [Paired census](../src/prototype/solver_inputs/source_results_measurements.tsv)
  at fuel 0/100/1000/completion differs only in retained Job result references:
  completed List/effect/captured/QuickSort remove 13/6/5/533 references. Job layout
  bytes and all graph/step counts stay equal; this is not a memory/speedup claim.
  [Applied-file delta](../src/prototype/solver_inputs/source_results_delta.tsv):
  implementation +47/-17 (net +30), tests +21/-3 (net +18), excluding patch-context
  churn and docs. This milestone does not reduce total source size.
  Focused/fresh/current Core/IADT/synthesis, focused ASan/UBSan/leaks and the
  IH-demand reproducer pass; all five C gates exit 0. Full O2 regression/examples/
  acceptance, semantic persistence and all seven checkpoint gates exit 0 on the
  fixed trial, including general QuickSort result/Sorted/permutation and both LT
  providers/partition orders. Fresh assembly matches the tested C/header files.
  The failed first regression run was stopped after its IH-demand failure;
  the owning result reader fixes that regression without restoring the copy.
  All 52 strict List images and the four-failure partition TSV match the parent;
  the public gate still exits 1. SE1-SE5 stay open; this is unpromoted prototype
  work. Logs use `/tmp/a-program-source-results-fixed-`; user edits are excluded.
  First cover binding/domain,
  normalization and rule premises together, not three incompatible adapters;
  then migrate source/context/IADT/Identity/operation consumers before closure.
  Verified prototype prerequisite (2026-10-01; `e6e7d2f` plus the preparation
  ownership patches): raw derivation and binder-domain preparation borrow their
  actual checking/normalization output instead of copying its receipt and terminal
  state. Agent implementation decision: preparation DONE is not checked DONE;
  name readiness, binding/domain consumers and restricted checkpoint attachment
  follow the actual output. Genuine input cursors and annotation conversion remain.
  Fresh regression/examples/semantic and checkpoint gates pass; acceptance and
  C-backend gates pass on these same implementation sources. The initial combined
  run failed an obsolete constructor-test raw-result read; migrating that fixture
  to the public getter passes separately and in the final regression. Latest
  synthesis/derivation/constructor checks also pass ASan/UBSan with leak detection.
  Clean patch assembly matches all 156 tested C/header files; its focused checks
  pass. Logs use `/tmp/a-program-preparation-owner-final-`.
  Applied source delta: +45/-28 (net +17); tests: +96/-2 (net +94), excluding docs
  and patch context; see [file deltas](../src/prototype/solver_inputs/preparation_owner_delta.tsv).
  The effect-application sample loses 11 result references and five dispatches,
  but Jobs stay at 5,129 and wrapped aligned allocation grows by 1,024 bytes;
  see [measurements](../src/prototype/solver_inputs/preparation_owner_measurements.tsv).
  This is not a memory-saving or completed Job-removal milestone. Public reload
  partitions still fail at 100:100, 1000:1000, 1600:1600 and 2617:0, unchanged
  from the parent. Preserve and restore the actual checking frontier/cursor;
  do not trust the saved completion flag or add a second task graph. SE1-SE5
  remain open. SE2's provisional walkers still need their effect dependency cycle
  resolved before consolidation. The previous connectivity failure is resolved:
  commits through `97825ec` were pushed to Main on 2026-10-01. Accepted `src/`
  and user edits are not included.
  Verified prototype increment (2026-10-01, parent `19a166a`): scope
  validation belongs to Binding and borrows its checked input after validation,
  with zero private state and no copied result. Source/binding post-checks borrow
  scope, operands and exports; only type-computation/check continuation pointers
  remain (16 bytes, formerly 192). Three shared roles/dispatch paths and the
  source layout's duplicate checked-type field are removed. Do not remove lexical
  validation: an admitted Context does not justify associating a different binder
  with its name. The new rejection tests exercise that distinction and prevent
  pre-validation acceptance; `::` remains an independent post-check. Focused O2,
  C backend, ASan/UBSan with leak detection, clean-assembly synthesis/constructor/
  derivation I/O and current-worktree Core/IADT/synthesis checks pass. Full O2
  regression/examples/acceptance, semantic persistence and all seven checkpoint
  gates exit 0, including general Sorted/result connection and all LT-provider/
  partition orders with negative controls. The first census build used the old
  helper path and failed; the corrected build uses the recorded patched helper.
  [Paired measurements](../src/prototype/solver_inputs/source_check_owner_measurements.tsv)
  preserve Solve steps, graph counts and logical premises in four samples. Job
  state storage falls by 2,640/11,048/2,664/82,040 bytes for List/effect/graph/
  QuickSort; this is not total memory or a speedup result. Job result references
  fall by 35/18 in effect/QuickSort. Actual implementation is +136/-128 (net +8),
  tests +21/-0; [per-file deltas](../src/prototype/solver_inputs/source_check_owner_delta.tsv).
  Fresh assembly matches all 156 tested C/header files. All 52 List partition
  images and the four-failure public reload report match the parent exactly.
  Pending validation/checking headers, post-check result forwarding and six broad
  source roles remain: this does not complete SE1-SE5 or promote prototype code.
  Logs use `/tmp/a-program-source-check-owner-`.
  Verified prototype increment (2026-10-01, parent `0c59383`): Match retains its
  open result-type input on its existing owner. The separate `RESULT_TYPE_JOB`,
  dispatch path and copied terminal receipt are deleted. Constructor branches
  are allocated once after schema discovery; equations can arrive earlier.
  Classifier propagation borrows its immutable operands and actual output,
  retaining one continuation pointer (8 bytes, formerly 184), not broad source
  state. Additional equations still use existing conversion and cannot overwrite
  the first input. Concurrent application wakeup, one shared Match/two equations,
  zero fuel and independent `::` post-check tests pass. Full O2 regression,
  examples, acceptance, semantic persistence and all seven checkpoint gates exit
  0, including general Sorted/result connection and both LT providers/partition
  orders. Separate C backend and focused ASan/UBSan synthesis/constructor/
  derivation checks pass with leak detection. Fresh assembly matches all 156
  tested C/header files; its focused checks and current-worktree Core/IADT/
  synthesis checks pass. User edits remain excluded. Paired
  [measurements](../src/prototype/solver_inputs/match_owner_measurements.tsv)
  preserve Term/Occurrence/Evidence/query counts and logical premises. QuickSort
  loses two Jobs/result references and 244,000 Job-storage bytes; steps fall by
  one. Whole wrapped cumulative
  [allocation](../src/prototype/solver_inputs/match_owner_allocation.tsv) falls
  234,832 aligned bytes but grows 227 calls, not a general speedup result.
  Actual implementation is +54/-30 (net +24), tests +90/-0;
  [per-file deltas](../src/prototype/solver_inputs/match_owner_delta.tsv).
  All 52 List partition images and the four-failure public reload report match
  the parent exactly. Four broad source roles and public reload restoration
  remain unfinished; SE1-SE5 stay open. Logs use `/tmp/a-program-match-owner-`.
- [ ] **SE2 single construction path:** enumerate provisional structure inputs
  for Lambda/App/Pi and CBPV, extract their actual construction once under the
  existing semantic owners, and have checking consume that same construction.
  Remove the corresponding duplicated structural walkers and overbroad private
  state. Keep genuinely unfinished queries and effect dependencies; do not
  replace all of them with an unconditional wait for accepted Evidence.
  Owner-locality audit (2026-10-03, `7d72b62`, agent assessment): the broad
  `rule_key/rule_header` input carries checking parameters, not copied Oracle
  Terms. Replacing it with a packed record and reconstructing headers for
  consumers would add a boundary without removing a semantic duplicate;
  reject that shortcut. Localizing the actual rule contracts remains open.
  Verified prototype increment (2026-10-02, agent decision; parent `755363a`):
  classifier-formation, Variable and Host classifier shapes borrow their actual
  input queries. Delete the forwarding type-query path/helper and Variable/Host
  dispatch branches, without a replacement Job, tag or authority. Accepted views
  still read typed data; existing symbolic views stay immutable. Shape readiness
  does not discharge formation's Context/judgement checks. Direct sharing,
  unaccepted Effect-dependent inputs, zero/split fuel and wrong-Context rejection
  pass. Full O2 regression/examples/acceptance (including general Sorted and both
  LT providers/orders), semantic persistence, seven checkpoint and five C gates,
  focused ASan/UBSan/leaks and fresh/current-worktree Core/synthesis/IADT pass.
  All 388 assembled C/header files match. [Paired census](../src/prototype/solver_inputs/classifier_projection_measurements.tsv)
  preserves all zero-fuel fields and final typed/Evidence/query/premise counts;
  QuickSort loses 230 Jobs/33,120 Job-layout bytes and 216 dispatches, Handler
  loses 42 Jobs/6,048 bytes and 58 dispatches. These are layout/fuel measurements,
  not total RAM or wall-time speedups. [Applied deltas](../src/prototype/solver_inputs/classifier_projection_delta.tsv):
  implementation +19/-25 (net -6), tests +21/-1 (net +20). All 52 List partition
  images/report equal the parent; the same three public reload failures remain,
  not waived. [Inputs](../src/prototype/solver_inputs/classifier_projection_inputs.tsv)
  pin the samples and compiler/source hashes. Logs: `/tmp/a-program-classifier-projection-`.
  SE1-SE5 remain open; prototype only, unrelated user edits excluded.
  Verified prototype (2026-10-02, agent decision, parent `e6dd1b3`): Handler checking
  borrows clauses from ordinary rule inputs or the existing typed Fold owner.
  The two temporary clause-array reconstruction paths are deleted; each visited
  clause is read once into the actual typed constructor's inputs. Signature
  checking, exact receipt selections and provisional Effect dependencies remain.
  Repeated hash-consed Core constructor calls do not by themselves establish
  duplicated graph allocation; do not remove necessary provisional queries on
  that assumption. Borrowed/array equivalence, reader lifetime, invalid inputs,
  ordering and typed Fold rebuilding pass. Full O2 regression/examples/acceptance,
  semantic persistence, seven checkpoint and five C gates pass, including general
  Sorted/permutation and both LT providers/orders. Focused ASan/UBSan/leaks and
  fresh/current-worktree Core/synthesis/IADT checks pass. All 388 assembled C/header
  files match the tested candidate. [Paired census](../src/prototype/solver_inputs/handler_reader_measurements.tsv)
  preserves all fields at fuel 0/100/1000/completion on five samples. Handler
  [wrapped allocation](../src/prototype/solver_inputs/handler_reader_allocation.tsv)
  saves four arena requests/64 aligned bytes; this is partial cumulative allocation,
  not live RAM or a speedup. [Applied deltas](../src/prototype/solver_inputs/handler_reader_delta.tsv):
  implementation +71/-32 (net +39), tests +49/-1 (net +48); no net code reduction.
  All 52 List partition images and the completed Handler image equal the parent.
  The same three strict public reload failures remain, not waived. No Job, tag,
  format, trust policy or acceptance store is added. SE1-SE5 remain open; prototype
  only, user edits excluded. Logs use `/tmp/a-program-handler-reader-`.
  Verified literal-input deletion (2026-10-02, parent `be4a61e`): `@`, Int32 and
  Text requests return their existing ordinary rules directly. Delete the source
  wrapper, literal preparation/frontier APIs, stale structural-reader branches
  and restricted source checkpoint's LITERAL record; use the existing rule codec.
  A closed literal shares the exact typed Context, not unrelated lexical scopes.
  Saved annotation headers describe a recipe, not acceptance: ordinary checking
  and post-check projection still validate its premises/Context. Invalid Int32,
  foreign ownership, sharing, zero fuel and literal checkpoints are tested.
  [Census](../src/prototype/solver_inputs/literal_inputs_measurements.tsv): QuickSort
  loses 262 Jobs, 162 result references and 51,680 Job-layout bytes; 524 fewer
  dispatches. Final typed graph, Evidence and query counts are unchanged.
  [Deltas](../src/prototype/solver_inputs/literal_inputs_delta.tsv): implementation
  +69/-82 (net -13), tests net +72, excluding patch context/data/documentation.
  Full O2 acceptance and C/sanitizer gates pass; after three unreachable reader
  branches were deleted, regression/examples, semantic persistence, all seven
  checkpoint gates, five C gates, sanitizers and fresh/current focused checks
  were repeated successfully. The final census is identical to the earlier one.
  [Inputs](../src/prototype/solver_inputs/literal_inputs_inputs.tsv) pin sample
  hashes; wrapped allocation is partial cumulative requests, not total live RAM.
  The repeated QuickSort allocation sample varies between processes; retain both
  observations without inferring a deterministic saving or speedup.
  Public reload still fails four comparisons (100:100, 1000:1000, 1600:1600,
  completed+0), not an expected-pass exemption. SE1-SE5 remain open. This is
  an unpromoted prototype; user edits are excluded. Logs: `/tmp/a-program-literal-inputs-`.
  Verified assertion-owner deletion (2026-10-02, parent `1e707e6`): parsed
  `PG_SYNTAX_EXPECT` operands go directly to the existing assertion owner.
  The broad expression wrapper, preparation stage and dispatch case are deleted;
  Operation aliases borrow that owner's original input. Synthesis still precedes
  post-checking, with no new state/tag/codec or annotation-directed inference.
  [Census](../src/prototype/solver_inputs/assert_inputs_measurements.tsv): List
  completion removes five Jobs/result references, 1,200 Job-layout bytes and ten
  dispatches (2612->2602); final typed/Evidence/query counts are unchanged.
  Other sampled final censuses match; this is not a general memory/speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/assert_inputs_delta.tsv):
  implementation +11/-23 (net -12), tests +89/-1 (net +88), excluding patch context.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
  and five C gates pass, including general QuickSort Sorted/permutation. Focused
  ASan/UBSan/leaks and fresh/current-worktree checks pass; fresh assembly matches
  tested source/tests. Parsed assertions roundtrip byte-identically at step zero
  and still reject missing inputs/wrong types. An initial test wrongly expected
  fresh parses to share AST identity; the corrected test repeats the same AST.
  Existing Operation-alias coverage exposed an old wrapper-specific origin path;
  reading the direct assertion input fixes it without copying alias metadata.
  All 42 common-budget List images match the parent's recorded images. The public
  partition gate still fails four reload comparisons (100:100, 1000:1000,
  1600:1600, completed+0), with completion now at 2602; no failure is waived.
  SE1-SE5 remain open. Prototype only; user edits excluded.
  Logs use `/tmp/a-program-assert-inputs-`.
  Verified application-owner increment (2026-10-02, parent `0fb14e9`): Function
  owns source App preparation and borrows scope/syntax from its exact key.
  Delete central application/callable slots, the separate application-state
  allocation and completed receipt copies. Actual progress is inline (120 bytes,
  formerly 144 + separately allocated 96); other expressions shrink 144->128.
  Preserve constructor indices, lexical IH, open Match equations, CBPV sequencing
  and independent post-checks. No task graph, tag or persistence codec is added.
  [Census](../src/prototype/solver_inputs/source_application_measurements.tsv)
  preserves graph/step counts; QuickSort loses 2425 copied result references and
  122024 Job-layout bytes, not total RAM. [Allocation](../src/prototype/solver_inputs/source_application_allocation.tsv)
  is partial cumulative arena requests, not live memory or a speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/source_application_delta.tsv):
  central synthesis -542 lines; implementation net +83, tests +23.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
  and five C gates, focused ASan/UBSan/leaks and fresh/current checks pass.
  Fresh assembly matches tested source/tests. All 52 List and eight effect/block
  images match the parent; loaded block step 0 is byte-invariant. Four public
  reload failures remain unchanged (exit 1); SE1-SE5 stay open. This is an
  unpromoted prototype; user edits are excluded. Logs use `/tmp/a-program-source-application-`.
  Verified declaration-owner increment (2026-10-02, parent `5e9187c`): Schema
  owns declaration preparation and borrows scope/syntax from its exact key.
  Central dispatch/allocation slots are deleted, without a new tag or codec:
  declaration state 160->48 bytes, other expressions 160->144. Universe/nominal
  Self/relocation checks remain. Full regression, seven checkpoint/five C gates,
  sanitizers and fresh/current checks pass; see [verification and limits](../src/prototype/solver_inputs/README.md).
  [Census](../src/prototype/solver_inputs/declaration_owner_measurements.tsv)
  differs only in Job bytes (QuickSort -104304); wrapped allocation is not live RAM.
  [Applied deltas](../src/prototype/solver_inputs/declaration_owner_delta.tsv):
  central synthesis -155 lines, implementation net +64, tests +30.
  Artifact controls match; four public reload failures and SE1-SE5 stay open.
  Verified Fold-input increment (2026-10-02, parent `33b06f1`): delete the provisional
  `fold_structure_state.clauses[]` allocation and the checked handler's temporary
  raw clause array. Both borrow existing inputs through the same synchronous
  Core builder; the structural owner keeps only its unfinished clause cursor.
  No reader is retained in Core and no shape is promoted to accepted Evidence.
  Focused/fresh/current Core/IADT/Solver and ASan/UBSan/leak checks and all five
  C gates pass. Full O2 regression/examples/acceptance, semantic persistence
  and all seven checkpoint gates exit 0, including general QuickSort
  result/Sorted/permutation and both LT providers/partition orders.
  [All 26 census fields](../src/prototype/solver_inputs/fold_inputs_measurements.tsv)
  match the parent at fuel 0/100/1000/completion in four samples. The effect
  sample removes ten temporary allocation calls (160 aligned requested bytes);
  [allocation samples](../src/prototype/solver_inputs/fold_inputs_allocation.tsv)
  are partial cumulative requests, not live RAM. QuickSort varies between
  processes; no speedup or memory saving is inferred from that noise.
  [Applied deltas](../src/prototype/solver_inputs/fold_inputs_delta.tsv):
  implementation +65/-38 (net +27), tests +54/-0. All 52 List images, the strict
  partition TSV and eight effect/captured images are byte-identical to the parent;
  loaded captured-block step 0 is invariant. The four public reload failures
  remain (gate exit 1), as do SE1-SE5 and general owner/frontier consolidation.
  Verified block-owner increment (2026-10-02, parent `f459c0f`): source block construction,
  its visited scopes, cursor and name-index cleanup now live in the CBPV owner.
  Delete the expression's block slot, extra state allocation, dispatch branch
  and receipt copy. Its immutable syntax is borrowed from the existing key;
  shared sequencing frames are not a new Term representation. Central state
  shrinks 168->160 bytes; the block retains only its 72-byte local state.
  Fresh/frozen/current Core/IADT/synthesis and ASan/UBSan/leak tests pass,
  including zero/split fuel, cancellation, truncation, duplicate names and wrong
  post-checks. Five C gates, full O2 regression/examples/acceptance, semantic
  persistence and all seven checkpoint gates exit 0, including general
  QuickSort result/Sorted/permutation and both LT providers/partition orders.
  Fresh recorded C/header/test files match the tested trial exactly.
  [Paired census](../src/prototype/solver_inputs/block_owner_measurements.tsv)
  differs only in Job layout bytes and result references. Completed
  List/effect/captured/QuickSort save 728/3464/512/53544 Job bytes and remove
  0/7/1/12 result references; all graph/step counts match. Four QuickSort
  [arena samples](../src/prototype/solver_inputs/block_owner_allocation.tsv)
  vary between processes: aligned requests fall 75216..134736 bytes, but calls
  range from -608 to +632. These are cumulative partial allocation counts,
  not live RAM or a general speedup. [Applied deltas](../src/prototype/solver_inputs/block_owner_delta.tsv):
  central `synthesis.c` -156 lines; all implementation +245/-194 (net +51), tests
  +38/-0. Source lines do not decrease overall. All 52 strict List images, the
  four-failure partition TSV, and eight effect/captured-block images match the
  parent byte-for-byte; loaded captured-block step 0 is also byte-invariant.
  Public resumption still fails those four cases (exit 1); SE1-SE5 stay open.
  Logs use `/tmp/a-program-block-owner-`; no production promotion or user edits.
  Verified source-state deletion (2026-10-02, parent `a5c617d`): remove the unused
  `tail`/`function` slots and simplify the latter's always-NULL preparation
  guard. Central private state shrinks 184->168 bytes; no new structure replaces
  it. [Paired censuses](../src/prototype/solver_inputs/source_state_trim_measurements.tsv)
  at fuel 0/100/1000/completion differ only in Job layout bytes. Completed
  List/effect/captured/QuickSort save 1456/5696/848/102864 bytes; this excludes
  arena padding/other allocations and is not a live-memory or speedup estimate.
  [Applied-file deltas](../src/prototype/solver_inputs/source_state_trim_delta.tsv)
  are +1/-3, net two implementation lines removed; tests are unchanged.
  Focused ASan/UBSan/leaks, five C gates and fresh/current assembly
  Core/IADT/synthesis pass. Fresh assembly matches the tested C/header files;
  all 52 List images and the four-failure strict partition TSV match the parent.
  O2 regression/examples/full acceptance, semantic persistence and all seven
  checkpoint gates exit 0, including general QuickSort result/Sorted/permutation
  and both LT providers/partition orders. The public gate still exits 1. No Core tag,
  checking authority or wire field changes. SE1-SE5 remain open and this trial
  unpromoted; logs use `/tmp/a-program-source-state-trim-`. User edits are excluded.
  Verified prototype increment (2026-10-02, parent `2a0ba44`): Lambda/Pi/Quote preparation no longer uses
  the broad central expression state or its dispatcher. Function/CBPV borrow
  syntax/scope from the exact request key. Lambda borrows Context/body from
  the existing Pi/body rule inputs and retains only one rule pointer (8 bytes,
  formerly 184); Pi retains 24 bytes and quotation 16. Type-position normalization
  uses the existing CBPV query, not expected-type inference. Match demands,
  definition body attachment and registration resume validate the same lexical
  inputs, not the old central descriptor. Focused zero/split-fuel, sharing,
  scope/ownership and ordinary-acceptance tests pass. The first regression run
  exposed the old descriptor restriction on Pi assertions during image loading;
  after replacing it with exact scope/syntax checks, the `07_add` image gate passes.
  [Measurements](../src/prototype/solver_inputs/source_oracle_measurements.tsv)
  preserve final Term/Occurrence/Evidence counts and premises in four samples.
  QuickSort Job bytes fall 159,168, with nine additional structural requests and
  dispatches: this is not a speedup claim. [Allocation](../src/prototype/solver_inputs/source_oracle_allocation.tsv)
  is cumulative wrapped allocation, not live/total memory; pointer/hash order
  varies between runs. [Applied deltas](../src/prototype/solver_inputs/source_oracle_delta.tsv):
  implementation +250/-118 (net +132), tests +73/-1 (net +72). Code size has not
  fallen; central semantic handling and retained state have been reduced.
  Fresh assembly matches all 156 C/header files and its synthesis/derivation
  checks pass. Current-worktree Core/IADT/synthesis, five C gates and focused
  ASan/UBSan/leak checks pass without staging user changes. Full acceptance
  passed on 2026-10-02, including general Sorted/permutation/result connection,
  both LT providers/partition orders, partial images and invalid controls.
  Semantic persistence and all seven checkpoint gates also pass. All 52 public
  List partition images and the four-failure reload
  report match the parent exactly; no failure is waived. Logs use
  `/tmp/a-program-source-oracle-`. Core-building/checking-owner consolidation,
  general frontier restoration and SE1-SE5 remain open; no production promotion.
  Verified prototype increment (2026-10-02, parent `ba1e0d7`): Core-preserving Context
  projection and type/value readings borrow the actual structural query instead
  of allocating another Job/result. Three structural APIs use one iterative
  discovery path; their duplicate rule dispatch is removed. Fresh accepted
  inputs read typed data, while retained symbolic views stay unchanged. Tests
  cover exact pre-acceptance sharing, 4,096 input links, split/zero fuel and
  invalid rule rejection; the ordinary checking requests remain authoritative.
  [Measurements](../src/prototype/solver_inputs/transparent_structure_measurements.tsv):
  QuickSort Jobs 46,932 -> 46,381; Job bytes 7,965,984 -> 7,886,872; steps
  799,129 -> 798,855. Typed Occurrences/Evidence/query counts are unchanged in
  all four samples; QuickSort has two additional raw Terms. List steps increase
  2,597 -> 2,618, so this is not a general speedup claim. [Applied deltas](../src/prototype/solver_inputs/transparent_structure_delta.tsv):
  implementation +38/-36 (net +2), tests +74/-0. Source line count has not fallen.
  Fresh assembly matches all 156 C/header files; recorded synthesis/derivation
  and current-worktree Core/IADT/synthesis pass. Focused ASan/UBSan/leak and
  all five C gates pass. Full regression, examples, acceptance, semantic
  persistence and all seven checkpoint gates pass, including both LT providers,
  both partition orders, general Sorted/permutation proofs and invalid controls.
  The public partition
  gate retains the same four failing reload cuts; zero fuel, 10+10=20 and all
  in-memory/save-only comparisons pass. No failure is waived. Logs use
  `/tmp/a-program-transparent-structure-`. SE1-SE5 and production promotion
  remain unfinished.
  Verified prototype increment (2026-10-01, parent `9146893`): Lambda classifier,
  Handler carrier and Effect subsumption structural requests now borrow their
  actual input queries. Function/CBPV selection stays Oracle-local; their
  forwarding walks are deleted, without an alias index, replacement task,
  additional acceptance or wire field. Zero/split-fuel, exact query sharing,
  symbolic effect snapshots and invalid-body/clause rejection tests pass.
  Full O2 regression/examples/acceptance, semantic and seven checkpoint gates,
  five C gates, ASan/UBSan/leak checks, fresh assembly checks and current-worktree
  Core/IADT/synthesis checks pass; all 156 assembled C/header files match.
  Final Term/Occurrence/Evidence counts match the parent in four samples;
  QuickSort loses 178 Jobs, 31,280 Job bytes and 111 dispatches. See
  [measurements](../src/prototype/solver_inputs/structure_projection_measurements.tsv),
  [allocation](../src/prototype/solver_inputs/structure_projection_allocation.tsv)
  and [deltas](../src/prototype/solver_inputs/structure_projection_delta.tsv).
  Allocation is a cumulative wrapped sample, not total memory or a speed claim;
  pointer/hash ordering can change call counts between invocations. Applied
  implementation +53/-29 (net +24), tests +20/-4 (net +16), excluding patch
  context/docs. The public partition report exactly matches the parent's four
  reload failures. Remaining Core-building queries, checking-owner consolidation
  and SE1-SE5 stay open. Logs use `/tmp/a-program-structure-projection-`;
  accepted code/user edits are excluded, and no test exemption was introduced.
  Prototype increment (2026-10-01, parent `a8715d7`): name registration,
  definitions and induction branches no longer allocate `source_work` or use its
  central dispatch. The union and multi-role macro are removed. Registration
  owns its existing index/frontier inline (88 bytes instead of 184 + a separate
  88); definitions retain activation/body only (16 instead of 184), borrowing
  syntax/scope/exports; branches retain four references (32 instead of 184).
  This is an agent implementation decision, not a replacement Job graph or full
  Oracle-module extraction. Full O2 regression/examples/acceptance, focused
  ownership/zero-fuel tests, semantic and seven checkpoint gates, C-backend gates,
  ASan/UBSan/leak checks and fresh/current assemblies pass. Fresh assembly matches 156
  C/header files. [Measurements](../src/prototype/solver_inputs/definition_owner_measurements.tsv)
  preserve all sampled semantic counts/steps; QuickSort Job storage falls 16,664
  bytes. [Wrapped allocation](../src/prototype/solver_inputs/definition_owner_allocation.tsv)
  falls 46,800 aligned bytes/610 calls for QuickSort, not a general memory/speed
  claim. [Deltas](../src/prototype/solver_inputs/definition_owner_delta.tsv):
  implementation +137/-114 (net +23), tests +63/-0, excluding patch context/docs.
  The public partition report is identical to the parent's four reload failures;
  no wire expansion or test exemption was added. Only expression preparation
  uses the broad layout now; central semantic paths and SE1-SE5 remain open.
  Logs use `/tmp/a-program-definition-owner-`; user edits are excluded.
  Verified prototype prerequisite (2026-10-01; implementation parent `f0ff363`,
  accepted baseline `e716232`): structural readers now borrow known Core/classifier
  Terms directly, without creating a Job. All consumers use the same by-value
  Term/pending-query view; it owns no state, acceptance or persistent graph.
  Existing pending-query identity, symbolic snapshots and effect discovery stay
  intact. No-allocation, wrong-owner, zero-fuel and independent post-check tests
  pass. O2 regression/examples/acceptance/semantic and all seven checkpoint gates
  pass; C backend gates and ASan/UBSan synthesis/derivation-I/O pass. Fresh assembly
  matches all 156 tested C/header files; its synthesis/derivation checks pass.
  Current-worktree assembly also passes Core/IADT/synthesis checks with the user's
  unrelated local edits; those edits are not included in the prototype milestone.
  Logs use `/tmp/a-program-structure-owner-`; the raw legacy QuickSort census
  input was rejected by both binaries' parser, so measurements use the previously
  recorded `quick-local` input rather than treating that rejection as a regression.
  [Measurements](../src/prototype/solver_inputs/structure_owner_measurements.tsv)
  show 6/41/459 fewer Jobs for List/effect-application/QuickSort, with unchanged
  Term/Occurrence/Evidence counts. List uses nine more steps; borrowed views
  enlarge pending owner state, so Job bytes grow in all three cases and wrapped
  allocation grows by 6,048 bytes for effect-application. This is not a universal
  speed/memory improvement or full Job/Evidence removal. Applied implementation
  is +146/-96, tests +284/-254; see [deltas](../src/prototype/solver_inputs/structure_owner_delta.tsv).
  Public reload still fails at 100:100, 1000:1000, 1600:1600 and completed+0
  (now 2626:0). Source loading reconstructs checking from initial state, so
  queue-only restoration is insufficient. Do not extend every Job's codec just
  to turn the gate green. Next consolidate the broad source/Oracle state and
  actual checking owners; keep SE1-SE5 open rather than pursue adapter counts
  as a substitute for the user's clarified locality requirement.
  Current prototype increment (2026-10-01, parent `97825ec`): function graph
  checking is removed from the broad source state/dispatcher and remains local
  to `synthesis_function.c`. Source metadata retains names/layout only; its
  graph-worker backlink and shared case-layout slot are deleted. The graph's
  formation is borrowed through the existing output view, not copied into a
  second result slot. Source-interface readiness is still scheduled, so the
  independent graph owner and scheduler are not yet completely unified.
  Full regression, examples, acceptance, semantic persistence and all seven
  checkpoint gates pass, including general Sorted/result connection and both
  derived-LT providers/partition orders. Focused/clean-assembly synthesis and
  derivation I/O, C backend and ASan/UBSan checks also pass. The current-worktree
  assembly passes Core/IADT/synthesis with user edits excluded from this change.
  All 156 assembled code/test files
  match the tested trial. Paired [measurements](../src/prototype/solver_inputs/oracle_local_measurements.tsv)
  preserve steps, Jobs and Term/Occurrence/Evidence counts in four samples;
  QuickSort Job storage drops 72,864 bytes. The graph example loses two result
  copies. Wrapped QuickSort allocation calls increase by 311 despite fewer
  bytes, so do not claim universal allocation reduction. Applied implementation
  is +122/-65, tests +55/-0; [per-file deltas](../src/prototype/solver_inputs/oracle_local_delta.tsv).
  Public reload has the same four failing partitions; do not waive that gate
  or mark SE1-SE5 complete. Logs use `/tmp/a-program-oracle-local-`.
  Verified prototype increment (2026-10-01, parent `937daea`): constructor value
  construction and data-case checking now belong to the existing Schema module.
  Their shared source roles/dispatch branches and constructor-scope union member
  are deleted. Source consumers borrow field/callable inputs from that owner;
  case checking borrows its body from the immutable request instead of retaining
  another body/checked-term reference. Private state is 32/24 bytes respectively;
  the remaining broad source layout and completed result forwarding are not
  eliminated. No second owner, semantic tag, acceptance policy or wire field is
  introduced. O2 regression/examples/full acceptance, semantic persistence and
  all seven checkpoint gates exit 0, including general Sorted/result connection
  and both LT providers/partition orders. Separate C backend gates and focused
  ASan/UBSan synthesis/constructor/derivation checks pass with leak detection.
  Fresh assembly matches all 156 tested C/header files and its focused checks
  pass. Current-worktree Core/IADT/synthesis checks also pass; user edits remain
  excluded. [Paired measurements](../src/prototype/solver_inputs/schema_owner_measurements.tsv)
  preserve steps, Job/Term/Occurrence/Evidence/query counts in all four samples;
  Job state storage falls by 1,280/1,120/1,760/27,200 bytes in
  List/effect/captured-graph/QuickSort, not the total live-memory measurement.
  Actual implementation is +300/-270 (net +30), tests +13/-0; central
  `synthesis.c` loses 244 lines. [Per-file deltas](../src/prototype/solver_inputs/schema_owner_delta.tsv)
  exclude patch context, documentation and symlink representation. All 52 List
  partition images are byte-identical to the parent, with the same four failing
  public reload partitions. SE1-SE5 remain open; this is unpromoted prototype
  work. Logs use `/tmp/a-program-schema-owner-`.
  Verified handler-wrapper deletion (2026-10-02, parent `b307e8d`): multi-clause
  source handlers request their existing Handler owner directly; the second
  broad expression allocation and forwarded completion are removed. Source
  transport borrows syntax/scope from that owner, without another record or wire
  field. Return-only syntax retains generalized sequencing, not the Handler
  carrier restriction; explicit carrier requests remain ordinary rule exports.
  [Paired census](../src/prototype/solver_inputs/handler_direct_measurements.tsv)
  removes 10 Jobs, 10 result references, 2,400 Job-layout bytes and 20 dispatches
  from effect-application. Final Term/Occurrence/Evidence/query counts and all
  other samples' final censuses are unchanged. This is not a peak-memory or
  general speedup claim. [Applied deltas](../src/prototype/solver_inputs/handler_direct_delta.tsv):
  implementation +17/-4 (net +13), tests +65/-0, excluding docs, patch context
  and symlink representation. O2 regression/examples/full acceptance, semantic
  persistence, seven checkpoint and five C gates exit 0, including general
  Sorted/result/permutation and both LT providers/partition orders. ASan/UBSan/
  leaks, fresh assembly synthesis/source I/O and current-worktree Core/IADT/
  synthesis/source I/O pass. Added tests check direct owner sharing, exact scope,
  step 0 and roundtrips at 0/1/32/completed fuel, including invalid clauses.
  Fresh source/tests/checkpoint/inspection files match the tested candidate.
  All 52 List images and the strict four-failure partition TSV equal the parent;
  the public reload gate remains failing, not waived. This is unpromoted
  prototype work; user edits are excluded and SE1-SE5 remain open.
  Logs use `/tmp/a-program-handler-direct-`.
- [ ] **SE3 Evidence inputs:** map each retained premise to typed operands,
  context/map, receipt or other real logical input. Remove duplicate premise
  arrays and history-based access for the mapped rules, starting with
  Match/induction/constructor. Migrate interning keys, consumers, export and
  ordinary checking in the same milestone. Recheck concurrent relocation work;
  do not overwrite it or silently discard required scope/formation evidence.
  - [x] **Dependent-field inputs (2026-10-03, parent `7d72b62`):** remove
    `typed_field.bindings` and the prefix reduction slots; borrow unchanged
    prefix images from the existing Context map and suffix Core/binder from
    retained original Occurrences/declarations. Preserve original versus
    reduced fields and their exact pure reduction receipts. Replace conversion
    congruence's copied target-image array with a synchronous input reader,
    shared by both substitutions. Verify dependent-field exposure, receipt
    rejection, zero/split fuel, serialization and sorting regressions before
    publishing. This is an agent implementation decision within the existing
    duplicate-removal scope, not a new equality or acceptance rule.
    Verified prototype: prefix images are borrowed, suffix inputs retain only
    original Occurrences and their genuine reduction receipts. Both endpoint
    substitutions use the same Core algorithm through stack-only readers;
    no callback, extra Job, Term tag or acceptance state is retained. On this
    64-bit build the two old field arrays used `24*(prefix+ordinal)` requested
    bytes, versus `16*ordinal` now; congruence's temporary target array is gone.
    These are layout facts, not peak-RAM or speedup claims. Full O2 regression,
    examples/acceptance, persistence/seven checkpoints, five C gates and focused
    ASan/UBSan/leaks pass. Added checks cover 64 parameters, last-field-first,
    chunks 1/64, unchanged/shadowed inputs, invalid receipts and caller mutation.
    Fresh assembly matches all 156 C/header files; fresh and concurrent-user-edit
    Core/IADT checks pass, as does current-tree synthesis. [All paired census
    rows](../src/prototype/solver_inputs/field_inputs_measurements.tsv) and all
    52 public List images/partition TSV equal the parent. The three public
    reload failures remain, not waived. [Allocation samples](../src/prototype/solver_inputs/field_inputs_allocation.tsv)
    are cumulative external arena requests with process variation. [Applied
    deltas](../src/prototype/solver_inputs/field_inputs_delta.tsv): implementation
    +60/-32 (net +28), tests +63/-22 (net +41), excluding docs and patch context.
    [Hashes](../src/prototype/solver_inputs/field_inputs_inventory.tsv) pin inputs,
    sources and binaries; logs use `/tmp/a-program-field-inputs-`. SE1-SE5 remain
    open; accepted code and unrelated user edits are not promoted by this change.
  Increment against `2f2e6d1` (2026-10-02, agent decision):
  - [x] Delete `premise_slice`; existing variable-arity constructors accept one
    synchronous array/reader view, also used by their admission interner.
  - [x] Delete Match's checked-branch copy and IH's branch scratch arena;
    borrow existing checked inputs without rebuilding their proofs.
  - [x] Check exact alternate receipts, offsets, null/foreign inputs, caller
    mutation, repeated lookup and ordinary reconstruction. O2 focused tests,
    persistence/seven checkpoints, five C gates and ASan/UBSan/leaks pass.
    The clean assembled tree matches all 148 C/header files of the trial.
    Concurrent relocation changes were merged and call arguments adapted only
    in a disposable verification tree; its Core/IADT/synthesis tests pass.
  - [x] Full O2 regression/examples/acceptance exits 0, including general
    QuickSort Sorted/permutation/result, both LT providers and partition orders,
    invalid-proof refusal, source/image consumers and optional witness isolation.
  [Census](../src/prototype/solver_inputs/derivation_inputs_measurements.tsv):
  all 20 paired rows agree, including step 0, partial fuel and completion.
  [External arena requests](../src/prototype/solver_inputs/derivation_inputs_allocation.tsv)
  decrease by 1,075 / 51,600 aligned bytes on QuickSort; these are cumulative,
  not peak RAM, and exclude `malloc` tails removed by `premise_slice`.
  [Applied deltas](../src/prototype/solver_inputs/derivation_inputs_delta.tsv):
  implementation +152/-168 (net -16); tests +204/-170 (net +34).
  [Hashes](../src/prototype/solver_inputs/derivation_inputs_inventory.tsv) pin
  the comparison; logs use `/tmp/a-program-derivation-inputs-`.
  All 52 List images/report match the parent. The same three public reload
  failures remain unwaived; no wire fields or trust policy changed. Prototype
  only; unrelated user edits excluded and SE1-SE5 remain open.
  Verified prototype increment (2026-10-02, parent `ffc5cb1`; agent decision):
  remove five transient flattened receipt arrays from inductive formation,
  Match/induction, TypeCase, substitution extension and family Identity.
  The existing interner reads their immutable inputs synchronously; no reader
  escapes and no new authority, tag or wire field is introduced. Exact alternate
  receipt selections and independent certificates remain. Caller-array mutation,
  null/oversized inputs and logical prefix/argument/suffix order are tested;
  the new tests also pass on the parent implementation. Full O2 regression,
  examples/acceptance, semantic persistence, seven checkpoint and five C gates
  pass, including both LT providers/orders and invalid evidence. Focused
  ASan/UBSan/leaks, fresh assembly and dirty-worktree Core/IADT/synthesis pass;
  all 156 active C/header files match the frozen tested candidate.
  [Paired census](../src/prototype/solver_inputs/admission_inputs_measurements.tsv)
  has identical fields at 0/100/1000/completion for all five inputs. Cumulative
  external arena requests on QuickSort decrease by 1,103 (53,280 aligned bytes);
  [allocation measurements](../src/prototype/solver_inputs/admission_inputs_allocation.tsv)
  also record linked malloc/calloc requests, not live/peak RAM or timing claims.
  [Applied deltas](../src/prototype/solver_inputs/admission_inputs_delta.tsv):
  implementation +113/-80 (net +33), tests +49/-0; not net line reduction.
  All 52 public partition images/report equal the parent; the same three reload
  failures remain, not waived. Handler admission still collects its read-once
  clause inputs and independent signature receipts; this increment does not
  infer those receipts from their subjects or reread callbacks. SE1-SE5 remain
  open, prototype only, unrelated user edits excluded. Logs use
  `/tmp/a-program-admission-inputs-`; reproduction and hashes are in the
  [prototype README](../src/prototype/solver_inputs/README.md#borrowed-admission-inputs).
  Verified prototype increment (2026-10-02, parent `e3b1216`; agent decision):
  ten Universe/Host/Variable/Termination/TypeCase/family-Identity rules borrow
  their existing typed Context/type/operand/map receipts instead of storing
  duplicate premise pointers. Empty-Context receipts, TypeCase's independent
  formation/parameter inputs and different exact proof selections remain.
  Ordinary reconstruction, repeat lookup, foreign/invalid rejection and access
  after typing-index disposal pass. An initial Identity fixture incorrectly
  assumed reindexing a derived family preserves its typed-node pointer; the
  corrected fixture uses genuinely identical variable endpoints, not a kernel
  change to make that assumption pass. O2 full regression/examples/acceptance,
  semantic persistence, seven checkpoint and five C gates exit 0. Focused
  ASan/UBSan/leaks and fresh/current-worktree Core/Host/IADT/Identity checks pass;
  all 156 assembled C/header files match the frozen tested candidate. User edits
  are excluded. [Paired censuses](../src/prototype/solver_inputs/remaining_receipts_measurements.tsv)
  at 0/100/1000/completion change only retained premise counts: completed
  List/effect/captured-Match/QuickSort save 76/446/400/30,594 references.
  This is not a peak-memory or speedup measurement. [Applied deltas](../src/prototype/solver_inputs/remaining_receipts_delta.tsv)
  are implementation +29/-0, tests +151/-0, not source-line reduction. All 52
  public partition images and the strict three-failure TSV equal the parent;
  that gate still fails and is not waived. No new tag, owner, wire field or
  acceptance policy is added. SE1-SE5 stay open; prototype only. Logs use
  `/tmp/a-program-remaining-receipts-`.
  Verified conversion-receipt deletion (2026-10-02, parent `917b1bc`): pure
  normalization borrows its typed origin's stable receipt; type conversion and
  Effect subsumption borrow origin/type receipts through the existing sparse
  selection layout. Unchanged reclassifications lacking those typed edges retain
  their actual inputs. Exact alternate proofs and conversion certificates remain;
  this adds no graph, tag, field or acceptance authority and bypasses no checking.
  [Census](../src/prototype/solver_inputs/conversion_receipts_measurements.tsv):
  final List/effect/captured/QuickSort retain 55/230/141/4452 fewer premise
  references, with every other field unchanged, including logical premises and
  Solve fuel. These are reference counts, not peak RAM or a speedup claim.
  [Applied deltas](../src/prototype/solver_inputs/conversion_receipts_delta.tsv):
  implementation +7/-0, tests +52/-0, excluding docs and patch-context churn.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
  and five C gates pass. Focused ASan/UBSan/leaks and fresh/current-worktree
  checking/I/O pass; fresh assembly matches tested source/tests. Tests retain
  alternate proofs, reconstruct ordinary derivations, cover unchanged typing
  boundaries and read logical inputs after typing-index disposal. All 52 List
  images and the strict four-failure partition TSV equal the parent; the public
  reload gate remains failing, not waived. SE1-SE5 stay open; prototype only,
  user edits excluded. Logs use `/tmp/a-program-conversion-receipts-`.
  Verified prototype increment (2026-10-02, parent `b3aa9e6`): rule import
  borrows its existing DAG/job relocation map; the maximum-arity scan, scratch
  premise array and copy loop are deleted. Readers are synchronous, not stored.
  Tests cover repeated/shared imports, exact alternate scopes, missing/cyclic
  children and zero fuel. O2 regression/examples/full acceptance, semantic
  persistence and all seven checkpoint gates exit 0, including general ordinary
  QuickSort result/Sorted/permutation and both LT providers/partition orders.
  Five C gates, focused ASan/UBSan/leaks and fresh/current assembly synthesis,
  source-I/O and derivation-I/O checks pass. All 156 tested C/header files match
  fresh assembly. All 52 List images and the strict four-failure TSV match the
  parent; that public gate remains failing, not waived. Implementation is +17/-11
  (net +6), tests +21/-1 (net +20), not a measured speedup or net line reduction;
  [applied-file deltas](../src/prototype/solver_inputs/import_inputs_delta.tsv).
  No Core tag, checking owner or wire field is added; SE1-SE5 remain open.
  Logs use `/tmp/a-program-import-inputs-`. User edits are excluded.
  Projection-receipt trial (2026-10-02, parent `ca9a17c`): mapped Context
  projections borrow the typed Context/origin's stable receipts; differing exact
  source selections remain sparse edges. Variable images without an origin
  retain their real inputs. Ordinary reconstruction, access after index disposal,
  focused ASan/UBSan/leaks and fresh/current assembly Core/IADT/synthesis pass.
  [Paired censuses](../src/prototype/solver_inputs/projection_receipts_measurements.tsv)
  at fuel 0/100/1000/completion differ only in retained premise references:
  completed List/effect/captured/QuickSort change 816->776, 5262->4892,
  1380->1310 and 198977->194831. Logical arities, steps and graph counts are
  unchanged; these are reference counts, not byte or speed estimates.
  All 52 List images and the four-failure strict partition TSV match the parent;
  the public gate still exits 1. O2 regression/examples/full acceptance, semantic
  persistence, all seven checkpoint gates and five C gates exit 0. This includes
  general QuickSort result/Sorted/permutation and both LT providers/partition
  orders. All 156 tested C/header files match fresh assembly; user edits are
  excluded. [Applied-file deltas](../src/prototype/solver_inputs/projection_receipts_delta.tsv)
  are +6/-0 implementation and +23/-0 tests, not a net line reduction.
  No owner, tag, wire field or trust bypass is added. Keep SE1-SE5 open and this
  increment unpromoted. Logs use `/tmp/a-program-projection-receipts-`.
  Verified prototype increment (2026-10-02, parent `7c3a1a1`):
  preparation reads its existing input owners through the request-key reader;
  checking reads the immutable checked/pending operand slots directly. The two
  unconditional `malloc`/copy/free paths in `synthesis_derivation.c` are deleted.
  Array callers and indexed readers use one ordinary rule dispatcher. The
  reader/owner are synchronous borrowed call arguments, never retained state;
  variable-arity named rule APIs still require temporary tail slices. This is
  not removal of those APIs, the remaining preparation/checking owners or object
  witness Terms. New Core/IADT tests reconstruct directly from accepted inputs,
  reject missing readers/children and preserve exact selections. The nominal
  image test also borrows alternate formation selections without a premise copy.
  [Paired censuses](../src/prototype/solver_inputs/borrowed_premises_measurements.tsv)
  are identical in all four samples at fuel 0/100/1000/completion. Wrapped direct
  [allocation measurements](../src/prototype/solver_inputs/borrowed_premises_allocation.tsv)
  for the corrected trial remove 223/1,363/234/12,907 malloc calls for
  List/effect/captured/QuickSort and 2,888/18,072/3,064/171,640
  malloc bytes. Arena calloc requests are measured too: QuickSort combined
  requested bytes decrease 155,224-188,056 in two paired runs; hash/pointer order
  changes scratch allocation. These exclude libc internals and are not live-memory
  or speedup estimates. Implementation is +169/-94 (net +75), tests +48/-7
  (net +41), diagnostic +33/-0;
  [applied-file deltas](../src/prototype/solver_inputs/borrowed_premises_delta.tsv).
  Source line count has not decreased. Corrected-trial ASan/UBSan/leaks, five C gates, fresh
  recorded-assembly checks and current-worktree Core/IADT/synthesis checks pass;
  all 156 tested C/header files match the recorded assembly. User edits are
  excluded. O2 regression/examples/full acceptance, semantic persistence and all
  seven checkpoint gates exit 0, including universal Sorted/permutation/result
  connection and both LT providers/partition orders. All 52 public List images
  and the four-failure partition TSV match the parent exactly; that strict gate
  still exits 1. SE1-SE5 remain open; this is unpromoted prototype work. Logs use
  `/tmp/a-program-borrowed-premises-`.
  Verified prototype increment (2026-10-01, parent `85c5549`): IADT formation
  borrows its exact parameter/index/constructor-result receipts from the existing
  Schema and retains no second premise array. All raw formation-premise readers
  now use the logical getter. Tests cover empty/indexed families, distinct exact
  result receipts for one nominal declaration, foreign-owner rejection, ordinary
  reconstruction and access after typing-index disposal. This removes duplicate
  edges, not Schema checking, generative identity or the remaining Evidence/Job
  owners. Clean assembly matches all 156 tested C/header files; focused checks,
  the current-worktree Core/IADT/synthesis checks and ASan/UBSan with leak detection
  pass. User edits are not included. Regression, examples, semantic persistence,
  all seven checkpoint gates and the separate C backend targets pass.
  The first batch failed because it incorrectly requested C backend targets from
  the artifact Makefile, not because a code test failed. The completed retained
  QuickSort/derived-LT tests are reused in the corrected split acceptance run,
  which exits 0 (`-o check-generic-retained` skips only that completed target);
  do not report the failed batch as a successful aggregate run. An initial test
  fixture wrongly expected identity Projection to produce a distinct receipt;
  the corrected Reindex fixture tests a genuinely distinct exact proof selection.
  [Measurements](../src/prototype/solver_inputs/schema_receipt_measurements.tsv)
  retain all logical inputs and graph counts while removing 9/5/15/65 stored
  premise pointers from List/effect/captured-graph/QuickSort. Steps are unchanged.
  Whole-arena QuickSort allocation ranges overlap across repeated paired runs;
  no overall allocation or speed improvement is established. Actual implementation
  is +29/-17 (net +12), tests +46/-2 (net +44), excluding documentation and patch
  context; see [per-file deltas](../src/prototype/solver_inputs/schema_receipt_delta.tsv).
  All 52 List partition images are byte-identical to the parent. The strict public
  gate still has the same four failures (100:100, 1000:1000, 1600:1600 and completed
  2626:0); no wire field or resume acceptance shortcut is added. Logs use
  `/tmp/a-program-schema-receipt-`. SE1-SE5 remain open; this is prototype work.
  Match/induction sharing and conclusion-index removal are implemented in the
  SE3 milestone below; constructor/other-rule input consolidation remains open.
  Mapped-receipt milestone (2026-10-01, agent implementation decision; parent
  `c9e0ce8` plus the recorded prototype changes): constructor, Match/induction,
  Request, Fold and Handler now share one storage path for independent inputs
  and sparse exact receipt selections. Handler borrows its computation, return
  clause, carrier and clause bodies from typed structure; independent operation
  signature proofs remain. Logical premise order and exact proof keys are
  unchanged. The remaining raw Handler reader now uses the logical getter.
  No semantic graph, acceptance flag or persistence field was added.
  Fresh verification: O2 `check`, `check-examples`, full `check-acceptance`,
  `check-artifact-semantic` and all seven checkpoint targets passed; C backend,
  sorting boundary, Linker, scalar and enum targets passed. ASan/UBSan with
  leak detection passed Core, IADT, Identity and synthesis. Tests cover seven
  two-clause receipt selections, foreign-owner rejection, reconstruction and
  logical access after typing-index disposal. A fresh recorded assembly matches
  every source/test C/header and passes Core, IADT and source-I/O tests.
  Effect-application's parent/candidate `.a` files are byte-identical. Its
  retained premise references fall by 40 and instrumented aligned arena
  allocation by 320 bytes, with unchanged graph counts and 17,924 Solve steps;
  see [measurements](../src/prototype/solver_inputs/receipt_owner_measurements.tsv).
  This is not an overall memory/speed claim: a sparse alternative selection
  includes an ordinal and can cost more than the old one-pointer dense suffix.
  Implementation delta: +55/-44 (net +11); tests: +36/-1 (net +35), excluding
  patch context and documentation; see [per-file delta](../src/prototype/solver_inputs/receipt_owner_delta.tsv).
  Evidence logs use `/tmp/a-program-receipt-owner-`. The fresh strict public
  partition report is identical to the parent's four-failure report. Current
  `source_io.c` imports completion descriptively and reconstructs pending
  checking, without interrupted checking cursors. Promoting its completion flag
  to accepted evidence would not establish exact or checked resumption. SE1-SE5
  remain open; this milestone is unpromoted prototype code, not the full gate.
  Publication: GitHub HTTPS initially failed on 2026-10-01; after connectivity
  was restored, `d6640ef` and later verified commits through `97825ec` were
  pushed to Main that day. No production promotion is implied.
  Verified reindex increment (2026-10-02, agent decision; parent `6506828`):
  mapped receipts borrow the map/origin inputs already retained by the typed
  action. Exact alternative receipts and inputs absent from that action remain
  independent; no new query, tag, API or wire field is added. Seven selection
  cases preserve logical arity/order, exact sharing and access after index
  disposal. [Paired census](../src/prototype/solver_inputs/reindex_receipts_measurements.tsv)
  differs only in retained premise references: completed List/effect/captured/
  QuickSort remove 188/1,346/1,114/36,963 references; steps and graph counts stay
  equal. This is not a peak-memory/speedup claim. [Applied delta](../src/prototype/solver_inputs/reindex_receipts_delta.tsv):
  implementation +6/-0, tests +32/-0, excluding docs and patch context.
  Fresh full O2 regression/examples/acceptance, semantic persistence, all seven
  checkpoint and five C gates pass, including both LT providers/orders and
  invalid-evidence controls. ASan/UBSan/leaks Core/synthesis/source/derivation,
  fresh-assembly checks and current-worktree Core/IADT/synthesis pass. All 52
  strict List images and the four-failure partition report match the parent;
  the public resume gate still fails. SE1-SE5 remain open; prototype only.
  Logs use `/tmp/a-program-reindex-receipts-`; user edits are excluded.
- [ ] **SE4 persistence projection:** consume those canonical structures through
  borrowed views, removing exported copies of reconstructible rule-input trees
  and obsolete owner codecs. Keep only unfinished state needed by actual Solve
  consumers. Wire ordinals are transport references, not a new semantic layer.
  No C layout, ABI, LinkerScript or target-native representation enters `.a`.
  Verified prototype increment (2026-10-03, parent `149eca0`; agent decision): delete the
  allocating rule-import wrapper. Import now writes caller-owned output slots;
  source restoration keeps them in its existing scratch, not the Program arena.
  The shared image's complete Term lookup table also becomes read-local. Its
  actual Terms and selected public roots keep their original semantic lifetime;
  one descriptor decoder serves both paths. No Job, semantic tag, acceptance
  authority, wire field, trust policy or accepted-source promotion is added.
  Deep 2,048-link rule import destroys its raw DAG and output array before Solve;
  deep shared Term images survive lookup-table destruction. Failure leaves
  caller output slots unchanged; zero roots/fuel do no checking. Focused O2,
  persistence/seven checkpoints, five C gates, ASan/UBSan/leaks, fresh assembly
  and current-user-edit Core/IADT/Synthesis pass. Full O2 regression, examples
  and acceptance exit 0, including general QuickSort Sorted/permutation/result
  connection, both LT providers/orders and invalid-evidence rejection.
  [All five census inputs](../src/prototype/solver_inputs/relocation_inputs_measurements.tsv)
  agree at 0/100/1000/completion. All 52 List images and the strict partition
  report equal the parent: the same three public reload failures remain, exit 1.
  [Read allocation samples](../src/prototype/solver_inputs/relocation_inputs_import_allocation.tsv)
  remove 1,288 externally requested persistent-arena bytes on List; four paired
  QuickSort reads remove 104,984-120,056 bytes with process variation. These
  exclude initialization, graph.c internals and discarded scratch, not RSS,
  peak memory, file-size reduction or a speedup. [Applied source delta](../src/prototype/solver_inputs/relocation_inputs_delta.tsv):
  implementation +30/-23 (net +7), tests +55/-32 (net +23), excluding patch/docs.
  [Hashes](../src/prototype/solver_inputs/relocation_inputs_inventory.tsv) pin
  the candidate and inputs; logs use `/tmp/a-program-import-inputs-`.
  SE1-SE5 remain open; duplicate structural construction and public frontier
  persistence are not settled by these transport-lifetime deletions.
  Verified increment (2026-10-02, agent decision, parent `771b022`): the detached
  rule-header/premise exporter is deleted. Test, checkpoint and semantic-image
  callers use synchronous borrowed views or existing checked inputs; no copy is
  moved into a compatibility helper. Exact sharing/root order, invalid/foreign
  inputs, callback failure, step-0 inertness and no exported-DAG allocation pass.
  Full O2 regression/examples/acceptance, semantic persistence, seven checkpoint
  and five C gates pass; ASan/UBSan/leaks Core/synthesis/source/derivation and
  affected normalization/constructor/semantic tests pass. Fresh assembly matches
  the tested candidate; fresh I/O/synthesis/semantic/checkpoints and dirty-current
  Core/IADT/synthesis pass. [Census](../src/prototype/solver_inputs/borrowed_export_measurements.tsv)
  is unchanged. All 52 public partition images and its three-failure report
  match the parent: resumption is not complete or waived. [Applied delta](../src/prototype/solver_inputs/borrowed_export_delta.tsv):
  implementation +45/-60 (net -15), tests +173/-174 (net -1), excluding docs and
  patch context. Logs use `/tmp/a-program-borrowed-export-`; prototype only.
  Verified increment (2026-10-02, parent `41e5f48` plus export-input patches):
  source saving no longer allocates the second rule-header/premise DAG. The
  writer synchronously borrows original checked, raw and pending inputs through
  one header/child projection. Membership/relocation maps remain temporary; no
  new semantic tag, acceptance authority or wire field is introduced. The
  explicit detached-input export API then still copied a requested transport DAG
  for test/checkpoint callers; the increment above deletes it. Import checking
  and the unfinished source owners remain, so SE1-SE5 are not complete.
  All 40 paired images (four inputs, step 0/100/1000/completed, two repeats,
  plus checked exports) equal the parent's bytes. Save does not advance Solve or
  add Terms, Occurrences, Evidence or Jobs. [Write measurements](../src/prototype/solver_inputs/export_inputs_measurements.tsv)
  show 30,078 fewer external arena requests / 5,268,528 fewer aligned requested
  bytes for the checked general QuickSort `quick_locally_sorted` theorem; the
  ordinary source-root case removes only 41 / 6,400. These are partial cumulative
  allocation requests, not peak RAM or a speedup claim. [Census](../src/prototype/solver_inputs/export_inputs_state.tsv)
  is unchanged; [sample hashes](../src/prototype/solver_inputs/literal_inputs_inputs.tsv)
  retain the standalone QuickSort input, not an import-only replacement.
  Fresh O2 `check`, examples, full acceptance and semantic/normalization/source/
  derivation/definition/namespace/constructor checkpoint gates passed. After the
  final NULL-root guard, rebuilt Core/IADT/synthesis, source/derivation I/O,
  semantic and affected checkpoints passed again. ASan/UBSan passed Core/IADT/
  synthesis and I/O; the final guard's I/O was rechecked under both sanitizers.
  Fresh isolated assembly matches the tested candidate; dirty-current assembly
  passes I/O without staging the user's edits. All five C backend gates passed.
  The strict public partition gate still fails at the same four reload cases
  (100+100, 1000+1000, 1600+1600, completed+0); its report and all 52 images match
  the parent, not a waived pass. Evidence logs: `/tmp/a-program-export-inputs-*`.
  [Applied deltas](../src/prototype/solver_inputs/export_inputs_delta.tsv):
  implementation +408/-228 (net +180), tests +151, diagnostics net +70, build net
  +8; documentation and patch context excluded. Actual copy deletion is not a
  source-line reduction. This remains unpromoted prototype work.
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
  Verified increment (2026-10-01, parent `729d7d1`): the independent classifier-
  propagation Job, request interning, output forwarding and no-work dispatches
  are deleted. The existing application owns the interrupted source/type/check
  cursor and clears it when done; ordinary argument checking still decides
  acceptance. No duplicate result, semantic enum or persistence field is added.
  Concurrent shared-Match calls, nested Lambda propagation, closed repeated
  quotation, zero fuel, 1/7/64-step partitions and independent `::` tests pass.
  Full O2 regression/examples/acceptance, semantic persistence, all seven
  checkpoints, C backend and focused ASan/UBSan with leak detection pass.
  Fresh patch assembly matches all 156 C/header files and passes focused checks;
  current-worktree Core/IADT/synthesis checks pass without including user edits.
  [Paired measurements](../src/prototype/solver_inputs/application_owner_measurements.tsv)
  preserve final Term/Occurrence/Evidence/query counts and logical premises.
  QuickSort loses 1,383 Jobs, 165,960 Job-storage bytes and 1,390 dispatches.
  Four [wrapped allocation pairs](../src/prototype/solver_inputs/application_owner_allocation.tsv)
  have fewer calls/bytes, with run-to-run variation; these are cumulative arena
  allocation, not live memory or a timing result. Implementation +37/-50 (net
  -13); tests +43/-13 (net +30), excluding documentation, patch context and the
  five-line diagnostic fixture; [file deltas](../src/prototype/solver_inputs/application_owner_delta.tsv).
  Public in-memory/save-without-reload partitions and step 0 pass; the four
  reload failures remain at 100:100, 1000:1000, 1600:1600 and completed 2600:0.
  At this milestone the double-quote/open-Match probe remained pending; the
  next verified increment below supersedes that result. Four broad source roles
  and SE1-SE5 remain open. This is unpromoted
  prototype work; logs use `/tmp/a-program-application-owner-`.
  Verified increment (2026-10-01, parent `071742c`): BODY discovery validates
  its Context and borrows the actual rule output, retaining neither copied
  result nor completed acceptance state. Job and pending await share one
  algorithm; Match consumers use the actual output too. The same polarity walk
  exposes the existing Thunk recipe to preserve repeated quotation. Rejected
  children still reject; zero fuel performs no checking. The open double/triple
  quote, concurrent equations, independent `::` and 1/7/64-step tests pass.
  Full regression/examples/acceptance, semantic persistence, all seven
  checkpoints, five C-backend gates and focused ASan/UBSan/leak checks pass.
  Fresh recorded assembly matches all 156 C/header files and passes synthesis,
  advanced namespace and derivation I/O; current-worktree Core/IADT/synthesis
  tests pass without staging the user's production changes.
  [Paired measurements](../src/prototype/solver_inputs/quote_body_owner_measurements.tsv)
  keep final Terms, Occurrences, Evidence, queries and premise counts unchanged;
  QuickSort loses 1,447 copied Job result references and 173 dispatches, not
  fixed Job storage. Implementation +50/-33 (net +17), tests +38/-5 (net +33);
  [file deltas](../src/prototype/solver_inputs/quote_body_owner_delta.tsv) exclude
  documentation and patch context. Public reload still fails at 100:100,
  1000:1000, 1600:1600 and completed 2597:0; step 0 and in-memory/save-only
  partitions pass. This remains unpromoted prototype work, not SE1-SE5 completion.
  Logs use `/tmp/a-program-quote-body-owner-`.

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

### SE3 Constructor Receipt Inputs (2026-10-01)

#### Subjective (User)

English paraphrase of the 2026-10-01 follow-up: Job/Evidence must not duplicate
the structures or authority already belonging to Term, typing and Solve.
Preserve the untyped Core boundary and each real unfinished obligation once.

#### Objective (Code)

Parent `808c217`, isolated accepted baseline `e716232` plus recorded prototypes.
`constructor_instance` already retains the result-type occurrence in `subject->type`,
but its receipt also stores that type's checking receipt as the first of four
premises. The other three premises retain declaration, parameter and instance
admission not recoverable from the constructor's current operands alone.
Match/induction also store the exceptional-selection count in an extra header,
alongside a separately retained logical arity derivable from the typed operands.
No accepted-source or concurrent user edit is changed.

#### Assessment

Agent prototype decision: store physical input count once in the receipt header.
Constructor and elimination logical arities come from their rule/typed structure;
other layouts keep their existing dense inputs. Constructor stores only the
three independent premises when its type's stable first receipt is exactly the
chosen premise; otherwise it retains all four. Do not replace a selected proof
by another proof of the same subject. Match/induction keep the existing sparse
ordinal/receipt selections, without another count header. Ordinary checking,
exact proof keys and logical getters are unchanged; no new lookup table, Core
node, typed edge, wire field or acceptance path is introduced.

Initial focused verification disproved the test assumption that every constructor
can omit the type receipt: the Nat fixture already has another first receipt.
The implementation preserves that selection; the corrected test explicitly
checks four retained inputs there. Indexed Acc supplies the three-input case.
This distinction is evidence-driven, not a reason to identify alternate proofs.

Further inspected SE3 candidates, not implemented here: `PG_REQUEST_INTRO`
repeats payload/response type receipts already held by its operation declaration;
`PG_FOLD_ELIM` repeats its two typed operands. Verify exact receipt selections
and all consumers before removing those references. This does not replace the
open SE1 general-classifier/frontier consolidation.

#### Plan

- [x] Implement in the isolated prototype; preserve both receipt-selection cases.
- [x] Verify indexed Acc, alternate instance receipts, logical arity, allocation-free
  reads and getters after typing-index disposal, plus existing rejection tests.
- [x] Complete regression, acceptance, sanitizer, census and public partition
  comparisons; reproduce recorded patches and report applied deltas.
- [x] Publish the verified prototype milestone; do not mark full SE1-SE5 complete.

Published to `origin/main` as `1ab416f` on 2026-10-01. Accepted source and
concurrent user edits are excluded; this publishes the prototype, not promotion.

General classifier ownership, remaining constructor inputs and full SE1-SE5
completion are still open. Full O2 `check`, examples, semantic audit and seven
checkpoint gates pass. ASan/UBSan Core/IADT/Identity/synthesis pass with leak
detection. Recorded patches reproduce source, tests and checkpoint/audit fixtures
exactly; the default overlay also assembles, and Core/IADT/synthesis pass with
the excluded user edits. Full `check-acceptance` exits 0, including both LT
providers and partition orders, universal Sorted/permutation witnesses, ordinary
results, semantic partial images and invalid controls. The four pre-existing
public partition failures below remain open, not waived by this run.

Fresh completed census: List-09 retained premise edges 849 -> 843; general
QuickSort 199,746 -> 199,280. Logical edge counts, Terms, occurrences, Evidence,
Jobs and step counts are unchanged, including zero-fuel and repeated terminal
rows. The 466 omitted pointers and 90 removed count headers reduce QuickSort
receipt payload by 4,448 bytes on this build, before arena alignment. This is
not total live memory, artifact size or a measured speedup. The cumulative
allocation wrapper varies across repeated parent/current runs and is not used
as a net-memory comparison here. Public partitions still exit 2; their TSV is
byte-identical to the parent's four failures. Evidence logs use
`/tmp/a-program-constructor-receipts-`.

Applied deltas from `808c217`, excluding stored patch context and documentation:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/evidence.c` | 36 | 26 | +10 |
| `tests/iadt.c` | 33 | 0 | +33 |

Physical duplication decreases; implementation source lines increase by 10.

### SE3 Request and Fold Receipt Inputs (2026-10-01)

#### Subjective (User)

English paraphrase of the 2026-10-01 follow-up: keep the actual Solve frontier
and unfinished cursors rather than another graph duplicating Term and typing.
Evidence must not reconstruct proof structure already retained by typed inputs.

#### Objective (Code)

Parent `76f67ad`, frozen accepted baseline `e716232` plus recorded prototypes.
`PG_REQUEST_INTRO` stores four premises although its existing operation
declaration retains the first two receipts and its typed operands identify the
remaining subjects. `PG_FOLD_ELIM` stores two premises for its two typed operands.
Both use the ordinary logical premise getter for interning and transport.

General classifier audit: `synthesis_function.c:classifier_step` still wraps
`pg_classifier_request`; `synthesis.c:type_structure_step` can inspect its
operand's provisional classifier before the producer is checked. CBPV result
contexts also inspect that pending formation's source operand. The wrapper is
not removed by this change. `synthesis_conversion.c:classifier_step` additionally
forwards to a checked-input classifier-normalization Job; its normalization and
conversion are real work, but that extra scheduling owner remains to consolidate.
No contradictory accepted classifier was demonstrated by these static findings.

#### Assessment

Agent prototype decision: extend the constructor's omitted-prefix/dense-suffix
layout to Request and Fold. Borrow the largest exact prefix from existing
declaration/typed-input receipts; retain the suffix beginning at the first
different selection. This preserves every exact proof key without an exception
index, copied operand array, new Core tag, typed edge or wire field. No checker
or reduction rule is bypassed. Receipt lookup does not compute or allocate.

Do not implement classifier consolidation as another query wrapper or a third
independent result table. Migrate the dependency interface and provisional
projection together; source readiness and a canonical typed query's cursor are
not interchangeable. This remains the existing SE1/SE2 obligation, not an excuse
to declare the frontier refactor complete after receipt compaction.

#### Plan

- [x] Consolidate the three mapped dense layouts under one prefix/suffix path.
- [x] Test canonical and alternative Request/Fold inputs, exact sharing,
  allocation-free logical reads and read access after typing-index disposal.
- [x] Run full O2 regression, examples, semantic audit, seven checkpoint gates,
  and ASan/UBSan Core/IADT/Identity/synthesis with leak detection.
- [x] Reassemble recorded patches exactly and compare census/public partitions.
- [x] Complete full acceptance and verify the default overlay: assembly and
  Core/IADT/synthesis pass with the excluded concurrent user edits.
- [x] Publish the prototype milestone. Keep full SE1-SE5 and the public
  partition failures open.

Published to `origin/main` as `82f23b5` on 2026-10-01. This is prototype
publication only; accepted source and concurrent user edits are excluded.

Fresh census: List-09 retained edges 843 -> 825; general QuickSort 199,280 ->
199,042. Other census columns and terminal repeated/zero-fuel controls are
unchanged: QuickSort still uses 809,172 steps and 53,643 Jobs. The 238 omitted
pointers save 1,904 raw receipt bytes on this build, before arena alignment;
this is not total live memory, artifact size or a speed claim. Public partitions
exit 2 with a byte-identical TSV to the parent's four failures. Full
`check-acceptance` exits 0, including both LT providers/partition orders,
universal Sorted/permutation witnesses, ordinary results, semantic partial
images and invalid controls. Logs and comparisons use
`/tmp/a-program-request-fold-`.

Applied deltas from `76f67ad`, excluding stored patch context and documentation:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/evidence.c` | 55 | 13 | +42 |
| `tests/core.c` | 39 | 0 | +39 |

Physical duplication decreases; implementation source increases by 42 lines.

### SE1 Resolved Conversion Requests (2026-10-01)

#### Subjective (User)

English paraphrase of the latest follow-up: resume the actual Solve frontier;
do not put a second near-identical Job/Evidence graph above typed construction.

#### Objective (Code)

Parent `e9731a0`; frozen accepted baseline `e716232`; unrelated Context/IADT
worktree edits are excluded from the full candidate and preserved. Classifier
normalization and assertion checking now publish a resolved-key lookup to their
existing worker, instead of allocating another checked-input worker. Its
24-byte entry holds only an index header and owner pointer; the key borrows
checked receipts through the owner's immutable inputs. No copied input array,
cursor, status, result, acceptance decision or wire field is added. Known
checked requests do not allocate a lookup entry. `::` remains post-synthesis.

| Completed census | Parent | Candidate |
| --- | ---: | ---: |
| List-09 steps / Jobs | 2,720 / 870 | 2,594 / 828 |
| List-09 raw Job bytes | 153,016 | 147,120 |
| List-09 aligned lookup entries + buckets | 0 | 1,344 + 512 |
| General QuickSort steps / Jobs | 809,172 / 53,643 | 801,376 / 49,993 |
| General QuickSort raw Job bytes | 9,392,688 | 8,880,936 |
| General QuickSort aligned lookup entries + buckets | 0 | 115,936 + 32,768 |

Occurrences, Evidence and logical/retained premise counts are unchanged; QuickSort
has one fewer Core term. These are allocation/dispatch measurements, not total
memory or wall-time measurements. Census includes the new lookup's cost.

#### Assessment

Agent implementation decision within prototype scope: retain one result owner
and a disposable discovery index, not another constraint/result store. Exact
receipt pointers prevent merging distinct Contexts or proof selections. The
unchanged warm-sharing tests and new pending/checked tests pass. Several pending
discovery requests can still forward to the same owner; removing all retained
forwarding frames and the general classifier wrapper remains SE1 work. The
lookup is not serialized; full owner-frontier restoration remains unfinished.

#### Plan

- [x] Remove checked-input worker recreation for these two roles; preserve warm
  sharing, rejection, exact scopes and zero-fuel inactivity.
- [x] Run O2 `check`, examples, semantic and all seven checkpoint targets;
  run ASan/UBSan Core, IADT, Identity and synthesis with leak detection.
- [x] Reassemble the recorded patches and compare source/test/checkpoint/artifact
  directories exactly. Current-default Core/IADT/synthesis also pass; this is
  not an independent full acceptance run on the unrelated local edits.
- [x] Finish full `check-acceptance`, including both LT providers/partition
  orders, universal Sorted/permutation witnesses, exact outputs, semantic
  partial images and invalid controls.
- [x] Publish the verified prototype milestone (`6fbf092`, pushed to Main),
  without production promotion.
- [ ] Resolve the public partition gate under the existing SE1-SE5 work list.

Fresh public partition target exits 2: the same four failing cases remain
(100+100, 1000+1000, 1600+1600 and terminal+0); terminal fuel is now 2,594.
No failure is waived. Evidence uses `/tmp/a-program-conversion-frontier-`.
Applied per-file deltas are in
[the line sheet](../src/prototype/solver_inputs/conversion-frontier-lines.tsv):
implementation +56/-12 (net +44), tests +73/-0. Audit code is separately +10/-3.
Full SE1-SE5 remain open; this is not production promotion.

### SE1 General Classifier Ownership (2026-10-01)

#### Subjective (User)

English paraphrase of the latest follow-up: resume the actual constraint frontier
instead of maintaining a second Job graph duplicating Terms, typing and Solve.
This requirement does not approve a particular replacement representation.

#### Objective (Code)

Parent `f4c4218`, frozen accepted baseline `e716232` plus recorded prototypes;
concurrent accepted-source/Context/IADT edits are excluded and preserved.
Known-input classifier requests now return the canonical typed query directly.
Unresolved-input discovery retains only a reference to that query, not its
answer. Its completion means discovery completed, not classification completed.
The existing index/owner/role prefix is shared without allocating an adapter;
Job/query header sizes remain 96/80 bytes on this build.

Dependency/export consumers now borrow the actual owner's result. Query
admission checks current interner membership, rejecting stale and foreign
typing inputs without adding another ownership table. Every query transition,
including completion, yields before a parent can advance another query.

| Completed census | Parent | Candidate |
| --- | ---: | ---: |
| List-09 steps / Jobs | 2,594 / 828 | 2,612 / 820 |
| List-09 raw Job bytes | 147,120 | 146,472 |
| General QuickSort steps / Jobs | 801,376 / 49,993 | 801,698 / 48,986 |
| General QuickSort raw Job bytes | 8,880,936 | 8,757,184 |

Typed-query counts/bytes, Terms, occurrences and Evidence counts are unchanged.
Query bytes remain 45,104 for List-09 and 5,345,744 for QuickSort. These are raw
allocation/dispatch measurements, not total memory or a wall-time speedup.
Applied per-file deltas are in
[the line sheet](../src/prototype/solver_inputs/classifier_owner_delta.tsv):
implementation +1,020/-698 (net +322); unit/checkpoint/artifact audit tests
+903/-735 (net +168). Stored patch context and documentation are excluded.

#### Assessment

Agent prototype decision: remove the classifier answer wrapper and preserve
real source discovery separately. This does not yet give a single dispatcher:
typed-query cursors and remaining construction Jobs still have separate queues.
Private namespace checkpoint fixtures restore only answers already complete at
capture, charge validation separately, and retain exact remaining-step/image
checks. Their supported one-transition classifier queries are not a general
public query-checkpoint codec. No wire fields or production code are added.

Four public reload partition failures remain: 100+100 and 1,000+1,000 image-size
differences; 1,600+1,600 progress/status differences; terminal+0 status difference.
In-memory partitions and seed step-0 controls remain exact. Terminal fuel is now
2,612. These failures remain completion blockers, not waived exceptions.

#### Plan

- [x] Migrate classifier consumers/export to the actual result owner; test
  exact sharing, pending export, stale/foreign ownership and zero-fuel inactivity.
- [x] Reproduce and fix multiple query advances in one parent dispatch.
- [x] Run full O2 regression, examples, semantic and all seven checkpoint gates
  from a clean recorded assembly; verify exact reassembly against trial inputs.
- [x] Run ASan/UBSan Core/IADT/Identity/synthesis with leak detection.
- [x] Verify both LT providers/partition orders, universal Sorted/permutation,
  exact outputs, semantic partial images and invalid evidence. The earlier
  combined invocation exited 2 only at the old namespace fixture; its corrected
  fixture and the clean full regression/checkpoint invocation subsequently pass.
- [x] Measure census and rerun strict public partitions; retain all four failures.
- [x] Publish this prototype milestone without production promotion (`4583ddd`,
  pushed to `origin/main` on 2026-10-01).

Evidence logs use `/tmp/a-program-classifier-owner-`; clean publication checks
use `/tmp/a-program-classifier-owner-publish-check.log`. Full SE1-SE5 remain open
under the existing active work list; remaining IADT forwarding, construction
duplication, receipt overlap and public owner-frontier restoration are not
completed by this milestone.

### SE1 IADT Recovery Ownership (2026-10-01)

#### Subjective (User)

English paraphrase of the latest follow-up: the resumed frontier should not
duplicate Terms, typing or Solve with another authoritative Job/Evidence graph.
Source: user message on 2026-10-01; this asks for critical examination, not
approval of a new container or of deleting required checking information.

#### Objective (Code)

Parent `17ff239`, accepted baseline `e716232` plus recorded prototypes; concurrent
accepted-source edits remain excluded. IADT recovery previously allocated a
checked-input worker after resolving pending inputs and copied its query/result/
completion back. The trial keeps one resolved-key normalization preparation and
borrows the canonical nominal-recovery query. Preparation finishes when its
query reference exists, not when the query succeeds; its raw result remains NULL.
Constructor/Match/index-path/family consumers and qualified-name dependency
resolution now await the actual owner through the existing pending interface.

Fresh census: List-09 keeps 820 Jobs and 146,472 raw Job bytes; steps 2,612 ->
2,617, raw result references 576 -> 571. General QuickSort changes 48,986 ->
48,985 Jobs, 8,757,184 -> 8,757,048 raw Job bytes, 801,698 -> 801,912 steps,
and 34,560 -> 34,340 raw result references. Completed Terms/occurrences/Evidence,
typed-query counts/bytes and resolved-index storage are unchanged. Empty result
slots still occupy header bytes; fewer references are not a memory saving by
themselves. These measurements are not a wall-time speedup.

#### Assessment

Agent prototype decision: preserve real normalization evidence, not a second
checked-input preparation or nominal-recovery result owner. Already-resolved
inputs reuse the checked key before allocating. Earlier distinct discovery
references can converge on one query; removing all retained discovery frames
and consolidating dispatch/persistence remain SE1/SE4 work. No new queue, Core
node, wire field, acceptance rule or expected-type inference is introduced.
The user's frontier proposal does not require an independent Job graph. It
does require actual interrupted reduction/substitution cursors if resumption
must avoid recomputation. Keep those with the canonical unfinished operation;
rebuild only disposable ready/wakeup references. Object witnesses remain Terms;
checked admission and independent certificates need not form a second witness
program, but cannot be inferred from the mere existence of a typed description.

#### Plan

- [x] Migrate all recovery consumers; test discovery/query completion separately,
  zero fuel, converging producers, stable pending keys, and scoped/foreign inputs.
- [x] Run synthesis tests and ASan/UBSan Core/IADT/Identity/synthesis with leaks.
- [x] Reassemble recorded patches exactly and measure census/public partitions.
- [x] Finish full O2 regression, examples, semantic, seven checkpoint gates and
  acceptance; verify clean recorded Core/IADT/synthesis/source-I/O binaries.
- [x] Publish the prototype milestone without production promotion (`50e1f6f`,
  pushed to `origin/main` on 2026-10-01).

The same four public reload failures remain at 100+100, 1,000+1,000,
1,600+1,600 and terminal+0 (terminal fuel 2,617); no failure is waived.
Evidence uses `/tmp/a-program-instance-owner-`. Per-file deltas are in
[the line sheet](../src/prototype/solver_inputs/instance_owner_delta.tsv):
implementation +75/-67 (net +8), tests +96/-26 (net +70), excluding patch context
and docs. The full trial invocation exits 0; the clean recorded source-I/O suite
and normalization case also pass. An earlier no-argument source-I/O invocation
failed its CLI assertion, not a test case. Full SE1-SE5 remain open under the
existing active work list.

### SE1 Family Construction Ownership (2026-10-01)

#### Subjective (User)

English paraphrase, 2026-10-01: resume the actual Solve frontier, not another
Job/Evidence graph duplicating Term, typing and Solve. No new result database
or replacement Task graph is authorized by this requirement.

#### Objective (Code)

At `7846f59`, the recorded prototype's `family_function_step` still requests a
checked-input worker after discovering its pending input and copies the result
back. `family_continue` also copies an already-owned continuation answer.
Classifier/IADT discovery already borrows a canonical query, but its descriptor
interface can express only that particular output owner. Concurrent accepted
source edits remain excluded; this observation concerns the frozen-baseline
prototype assembly, not a fresh test result.

Fresh trial verification: synthesis tests and ASan/UBSan synthesis with leak
checking pass, including nested borrowed-output structural reads, failure
propagation, exact scoped sharing and zero fuel. The first full regression run
failed QuickSort at 732,804 steps: structural fallback mistook preparation DONE
for output completion. All three structural fallbacks now await the actual input
owner. The same historical provider completes at 1,493,904 steps; the published
parent completes at 1,493,976. No new proof axiom, expected-type inference or
answer-copy compatibility path was added. The corrected full regression exits
0: existing checks/examples/acceptance, semantic validation and all seven
restricted checkpoint modes pass. This does not include a passing public
partition gate; its failures are recorded below.

The recorded patches assemble cleanly and match the trial's C/header files.
Clean-assembly Core, IADT, synthesis, source-I/O and normalization tests pass.
Evidence logs use `/tmp/a-program-family-owner-`; they are local verification
records, not additional artifact data.

Whole-store census versus the published IADT prototype: List-09 is unchanged
(2,617 steps, 820 Jobs, 146,472 Job allocation bytes). General QuickSort Local
Sorted changes from 801,912 to 801,877 steps, 48,985 to 48,958 Jobs, 8,757,048 to
8,752,944 Job allocation bytes, and 34,340 to 34,054 raw Job result references.
Typed-node, Evidence and Query counts remain unchanged. These are not total
memory or wall-time estimates; the resolved-key index also gains 54 entries.

Strict public partition verification still fails the same four reload cases:
100:100 (-178 bytes), 1000:1000 (-1,123 bytes), 1600:1600 (pending instead of
done), and 2617:0 (same image bytes but pending instead of done). In-memory and
save-without-reload paths match exactly, including split 10+10 versus 20.
This remains a frontier/cursor persistence blocker, not an accepted exception.

#### Assessment

Agent trial decision: use the existing checked/pending input representation for
borrowed outputs as well as dependencies, replacing the query-only descriptor
hook. Discovery completion must not mirror output completion. Family conversion
should reuse its resolved input key and publish its actual continuation, without
constructing another full checked-input worker. Already checked non-family inputs
need no conversion worker. Retain genuine family construction and nominal
normalization work. This is an SE1 ownership change, not the full frontier gate.
The discovered raw-status consumer defect demonstrates why hiding mirrored state
behind an interface is insufficient: consumers must follow the actual owner.
The limited allocation reduction does not justify claiming consolidation is done.
Published prototype milestone: `13fdf8f5d119f9e981b549e0359aedbc14365c75`,
pushed to `origin/main` on 2026-10-01. Accepted `src/` changes and user-authored
uncommitted tests/documents were not included. Overall SE1-SE5 remain open.

#### Plan

- [x] Implement borrowed outputs and migrate classifier/IADT/family consumers;
  remove the query-only hook and redundant family forwarding/result copies.
- [x] Test converging inputs, exact scoped distinctions, failures, ownership,
  zero fuel and interrupted-output advancement; run regression and sanitizers.
- [x] Measure allocations/applied code deltas and rerun public partitions;
  record the failures without waiving them. Applied implementation: +117/-68
  (net +49); tests: +176/-19 (net +157), excluding patch context and docs.
- [x] Publish only verified prototype changes; keep SE1-SE5 incomplete until
  the existing frontier/persistence gates actually pass.
