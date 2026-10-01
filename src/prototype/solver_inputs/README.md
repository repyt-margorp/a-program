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
Source-I/O fixtures also use direct checked inputs for retained Contexts,
substitutions, functions, constructor roots and Match theorems. Match allocation
inspection accepts the same input representation instead of requiring an
Evidence Job; inspection does not advance Solve or produce checked facts.
The adapter factory, role and recognition API are removed, including all test
callers. Suspension tests use real unfinished operations; known checked inputs
allocate no scheduler node. This does not complete the single-owner/frontier refactor.
Checked receipts now attach to their existing typed occurrence, Context or map;
the separate conclusion index and its prefix allocations are removed. Admission
links are ignored by descriptive interning and are never serialized. Only checking
publishes them, with typing-owner isolation. Match/induction borrow canonical
receipts through those typed inputs and retain only exceptional exact selections.
Alternatives and their order remain observable; no proof is replaced by the default.
The contiguous Evidence-premise API is deleted; consumers use the logical getter,
which remains valid after typing-index disposal. This is partial SE3 work, not
removal of all retained rule premises or completion of the resumption gate.
Constructor receipts also borrow their result-type receipt when its stable
first admission is the exact selected proof; other selections remain explicit.
One physical input count replaces the separate elimination-selection header;
logical arities come from existing rule/typed structure, without a new index.
Mapped constructor, Match/induction, Request, Fold and Handler receipts now use
one layout: independent inputs followed by sparse exact receipt selections.
Typed inputs supply the default receipts; no duplicate operand list is stored.
Handler retains its independent operation-signature proofs, not another copy
of its computation, return clause, carrier and clause bodies. Logical order,
alternative proof keys and access after typing-index disposal stay unchanged.
This supersedes the omitted-prefix/dense-suffix layout for Request and Fold.
Rule requests use the same hash lookup for checked and pending inputs, borrowing
the key during lookup and copying it only on a miss. Structural queries retain
only a result, child query and normalization pointer, not the broad source state.
The Job-only rule/plain-rule contracts and alternate producer-key representation
are removed, including source/import/preparation and test callers. Temporary
premise arrays borrow the canonical checked/pending inputs; they are not another
stored graph. No compatibility adapter recognition remains.
Job-only expect/reindex/normalization/evaluation aliases are removed. Rule export
also borrows checked/pending roots through its existing closure traversal; known
receipts need no completed scheduler node. Reading still creates unchecked work.
The obsolete fixed-prefix-plus-Job-array request API is removed; ordinary arrays
and borrowed key projections use the same interner. IADT and all checkpoint tests
now pass checked receipts directly rather than constructing Evidence Jobs.
Composition and lifting no longer allocate a `CHECKED_QUERY_JOB`: constructor
scope and Identity-family owners borrow the existing typed query directly.
The query keeps its sole progress/status/result. Any query advance consumes
the caller's dispatch, preventing two advances through a completion boundary.
Raw derivation preparation and binder-domain preparation now borrow the actual
checking/normalization owner. Preparation completion no longer copies its
receipt or terminal checking state. Name readiness, domain consumers and the
restricted derivation checkpoint distinguish prepared input from checked output.
These preparation records still retain genuine discovery cursors; this change
does not remove every Job or settle public same-fuel resumption.
Classifier formation and normalization also accept direct typed operands;
structural readers inspect those inputs without recreating Jobs. Normalization
borrows the existing classifier query instead of a classifier-formation Job.
Identity reflexivity and family action also borrow that canonical query after
validating their accepted operand. The query retains its sole progress/result;
an advance always yields, even when it completes. The action remains its own
checking obligation, and failed classifier discovery remains UNSUPPORTED.
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
export shares checked leaves directly through its existing DAG traversal,
without an adapter-recognition key projection.
No new persistence fields are added. Restricted source checkpoints still reject
direct checked reference capture; their existing pending-owner scope is unchanged.
Classifier normalization has one checked/pending input/result API. Checked
families with a checked matching Context return directly, without an Evidence
Job. Pending Contexts keep their scope-checking obligation and stable request
identity; ordinary value/computation inputs retain actual normalization work.
Constructor members, field scopes and IH scopes also borrow checked/pending
inputs through one API. A checked field map seeds the existing member worker
directly; declaration, parameters and map do not need completed Jobs. Source
transport reads these inputs from the owner, not its private key-array layout.
Dependent field lifting and IH construction retain their unfinished cursors.
Application callees and arguments borrow those same inputs. Match completion
starts from its checked elimination result and allocates a continuation only for
actual path/generalization applications or a computed scrutinee. Known implicit
constructor indices and branch Contexts are borrowed directly. The Job-only
classifier-formation entry point is removed. Known inputs now return the canonical
typed query directly. Unresolved inputs retain one discovery record whose resolved
query owns classification progress and the answer; discovery copies neither.
Existing Job/query identity prefixes share an embedded view, without allocating
another adapter or storing another result. Waiting yields after each query
transition, including completion. Export borrows the actual answer without Solve.
Direct-query admission checks the live typing interner, rejecting foreign and
reinitialized-owner inputs without adding another ownership record. This does
not yet unify the two dispatchers or close the public resumption gate.
IADT recovery similarly separates normalization preparation from its canonical
typed query. It borrows that query's recovered type and instance view, without
copying the parameter map or nominal-recovery completion into the preparation.
Family conversion now borrows its actual construction output through the same
checked/pending input representation. Discovery DONE is not output DONE; structural
consumers wait on that output without reinstating result copies. Pending and
resolved checked keys share one construction, while exact Contexts stay distinct.
This is a prototype ownership change, not completed frontier persistence.
Lexical binding now has one checked/pending Context API; the Job-only binding
entry point is removed. IH/graph associations borrow known Contexts directly.
Only unavailable inputs require a binding-validation request; an existing one
keeps its identity after completion. Source transport uses the same inputs.
Source/artifact writer roots and CLI selections borrow that same input directly;
there is no Job-only writer or completed Job for a known selected result.
Reading restores ordinary unchecked producers without Solve or acceptance.
Sequencing and result-Context inputs, constant-result extraction and handler
return/carrier/clause operands use the same borrowed representation. Their
structural readers preserve checked inputs; inferred effect owners and ordinary
kernel acceptance remain, without another Job-only input interface.

### Assessment

Index-result, index/constructor transport and constant-motive consumers also use
the same inputs. Checked endpoints, paths, values and targets need no completed
Job. Genuine transport retains its finite candidate search and suspension cursor.
Ordinary compiler consumers and tests borrow checked receipts directly; the
Evidence-adapter factory, test callers and source-root recognition are removed.
This does not remove the independent Job graph.
Reindex requests no longer forward to a second checked-input worker after their
operands resolve. Existing exact checked receipts are borrowed directly through
the same input/result API; real pending keys and the shared action remain stable.
The Job-only reindex API is deleted, without a new result cache or wire field.
Classifier normalization and post-synthesis assertion requests now publish a
resolved-key lookup pointing to their existing owner. Each entry contains only
an index header and owner pointer; its checked operands are borrowed through the
owner's immutable inputs. There is no copied key array, status, cursor, result or
acceptance store. Known checked requests allocate no lookup entry. Other pending
producers can reuse that owner without creating a full checked-input Job, and
later checked requests find it even before completion. Exact receipt pointers,
not erased Core or conclusion equality, determine sharing. The disposable index
is destroyed with the Solve invocation and is not serialized.
Removing forwarding alone lost completed-result reuse; that trial was rejected.
This lookup consolidation preserves the unchanged warm-sharing tests, not just
terminal Kernel answers. Other forwarding paths and the general classifier query
wrapper still remain. Keeping those indefinitely would not complete SE1. No
semantic boundary has been merged merely to reduce node count.

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
For semantic/metrics audit targets, use `ARTIFACT_TESTS=$overlay/artifact_tests/`
with the generated candidate's migrated fixtures. This changes test selection,
not the compiler or source-file protocol.
