# Direct Solver Inputs

Prototype prerequisite to the [Solver/Evidence plan, SE1](../../../doc/2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md).
Applies after the artifact/readback/conversion candidate, not to accepted code.

## Problem List

1. Completed typed inputs should not require scheduler allocations.

### Subjective (User)

2026-09-30 paraphrase: question Job itself, remove duplicated construction, and
finish the ownership refactor before extending persistence or the C backend.

### Objective (Code)

The migration covers typed WHNF/NF and closed evaluation demands, including
binding/domain normalization, CLI demands and IADT endpoint normalization;
Context reindexing, substitution source/destination inputs; post-synthesis
type expectations; and rule premises, including their structural queries,
scope readers, checking, export and checkpoint consumers.
Substitution images use the same direct inputs. One API accepts checked or
pending contexts/images, with validity and arity checked by the ordinary worker.
Family pairing returns its checked result directly; dependent value pairing
still awaits reindexing and post-check conversion. Neither creates an adapter
merely to pass an existing result to a consumer.
An input directly borrows checked Evidence or its pending producer. The two
pointers are passed/stored by value; there is no allocated wrapper, result table
or new Core tag. Pending request identity does not change when its input finishes.

Source transport visits checked leaves without allocating Jobs. Its temporary
pointer-membership index is destroyed after export; the existing rule payload
and file layout are unchanged. Read still creates ordinary unchecked premises.
Rule requests use the same hash lookup for direct and legacy inputs, borrowing
the key during lookup and copying it only on a miss. Structural queries retain
only a result, child query and normalization pointer, not the broad source state.
Composition and lifting no longer allocate a `CHECKED_QUERY_JOB`: constructor
scope and Identity-family owners borrow the existing typed query directly.
The query keeps its sole progress/status/result. Any query advance consumes
the caller's dispatch, preventing two advances through a completion boundary.
Classifier formation and normalization also accept direct typed operands;
structural readers inspect those inputs without recreating Jobs. Normalization
borrows the existing classifier query instead of a classifier-formation Job.
Nominal IADT recovery accepts direct typed inputs and retains the existing
inductive query. It no longer wraps the input in an Evidence Job or repeatedly
reconstructs its normalization proof while that query is suspended.
Logical-family conversion/domain requests and Lambda-body/Pi-scope contexts use
the same inputs; abstraction and operation-signature consumers no longer need
Evidence adapters. The actual family/CBPV conversion and context checks remain.
Application, sequencing and result-context construction each have one direct
Context API, not separate checked/Job entry points. Constant-result extraction
and handler-context/carrier construction use the same input representation.
Source scopes now retain the same checked/pending Context input. Name lookup,
block/application/Match contexts and environment export no longer require a
completed Context Job. Pending scope keys do not change after completion.
The derivation-checkpoint prototype now borrows checked external inputs
separately from scheduled workers; neither capture nor restoration recreates
Evidence adapters. Its private payload is `APGDRC4`, without backward reading;
this is not adoption of a new public `.a` format or additional owner codecs.
Identity formation, faces, reflexivity, instances and family action/transport
now use direct checked/pending inputs through one API. Source and IADT transport
callers no longer wrap the known family, endpoints or paths into Evidence Jobs.
The separate face/action entry points and temporary path-to-Job array are removed.
Operation signatures and handler premises borrow the same checked/pending input.
Startup Contexts and source export no longer wrap those known inputs in Jobs.
The Job-only operation request API is removed, not retained as another variant.
Family reification uses the same allocation-free query-yield operation as
checked lifting. Completing an origin query cannot advance the application-body
query in the same dispatch. Query failure policy remains with each consumer;
the family owner's nominal fallback is unchanged. No new progress owner is added.
Source and block-binding annotations also use direct inputs through one API.
They await their scope Context and project both endpoints before post-checking;
the expected type never supplies missing synthesis. Source export borrows checked
leaves, while reading restores unchecked ordinary producers without acceptance.
Effect consumers borrow the existing equation owner directly. The inference
Job, its mirrored completion and factory-recall wakeup are removed. Job and
Effect owners share disposable subscriber-owned notification edges; these
contain no inputs, results, progress or acceptance. Released edges return to a
Solve-local pool instead of retaining one allocation per historical wait.
The restricted schedule codec still handles Job-to-Job waits only; this change
does not introduce a new Effect checkpoint or establish public resumption.
Body, abstraction and Lambda-body requests borrow checked/pending inputs through
the same interface. Checked constructor bodies no longer need completed input
Jobs. The ordinary polarity lifting and Context checks remain; a Context input
must actually be a checked Context judgement, not just share its scope pointer.
Selected checked bodies do not require a pending-rule projection or a new codec.
Lexical names, lookup and environments use the same direct inputs. Checked leaves
retain typed-use identity; pending keys remain stable after completion. Source
export uses its existing DAG key projection to share a checked leaf reached
directly or through a legacy adapter, avoiding duplicate transport records.
No new persistence fields are added. Restricted source checkpoints still reject
direct checked reference capture; their existing pending-owner scope is unchanged.

### Assessment

This removes some adapters, not the independent Job graph. Known-family
results and other context/IADT/operation consumers still use
adapters. Producer-to-checked forwarding paths also remain. Removing classifier
forwarding alone lost completed-result reuse, so that trial was rejected;
removing the query wrapper does not settle the pending/checked request ownership.
Keeping those indefinitely would not
complete SE1. No semantic boundary has been merged merely to reduce node count.

### Plan

Use the parent plan for progress. Create and verify a disposable candidate:

```sh
bash src/prototype/solver_inputs/overlay.sh /tmp/a-program-direct-inputs
make -f /tmp/a-program-direct-inputs/src/Makefile BUILD=/tmp/a-program-direct-inputs/build check
make -f src/prototype/artifact_persistence/build.mk \
  OVERLAY=/tmp/a-program-direct-inputs BUILD=/tmp/a-program-direct-inputs/build \
  CHECKPOINT_TESTS=/tmp/a-program-direct-inputs/checkpoint_tests/ \
  check-artifact-normalization-checkpoint check-artifact-source-checkpoint \
  check-artifact-derivation-checkpoint check-artifact-definition-checkpoint \
  check-artifact-namespace-frontier check-artifact-namespace-body-frontier \
  check-artifact-constructor-checkpoint
```

`ARTIFACT_SOURCE` selects an isolated accepted-source snapshot when the worktree
contains unrelated edits. Keep that baseline explicit in verification reports.
The optional checkpoint-test directory adapts assertions to the new premise
representation without changing the artifact-only candidate's tests.
