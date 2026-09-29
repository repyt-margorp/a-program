# Solver and Evidence Duplication Audit

Date: 2026-09-30
Status: initial audit complete; refactoring not implemented.
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

Fresh verification: the List-09 and completed QuickSort censuses produced
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

This change adds only an audit utility and plan corrections. No kernel rule,
public `.a` format or accepted implementation has changed; no code-size reduction
or completed-refactoring claim is made yet.
