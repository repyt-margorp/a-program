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
uses the existing four-premise substitution rule, without a dedicated pair
worker. Reindexing and post-check conversion remain genuine obligations.
Neither creates an adapter
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
IADT formation also borrows parameter/index/constructor-result receipts from
its existing Schema, retaining no second premise array. Exact Schema receipt
selections and logical order remain distinct even for one nominal declaration;
the getter remains valid after typing-index disposal. This does not remove
Schema checking, generative identity or the remaining Evidence/Job owners.
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
Evidence adapters. Its private payload is `APGDRC\5`, without backward reading;
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
the same interface. Known polarity and a checked matching Context select the
actual input or ordinary RETURN rule directly, including pending computations.
Existing discovery identities remain stable; unknown polarity and unfinished
Context checks still need discovery. A Context input must actually be a checked
Context judgement, not just share its scope pointer. Block binders use the existing
lexical address interner; implicit constructor conventions follow the named source
statement instead of depending on retained classifier Jobs. This repairs the
earlier rejected pending-body trial without a new index, Job kind or wire field.
Against `8b38099`, QuickSort loses 114 Jobs and 215 dispatches with unchanged final
Term/Occurrence/Evidence counts. Its lexical addresses add 18 binding records and
178 enclosing-binder references; its image grows 1,704 bytes. No overall memory or
file-size reduction is claimed. See [census](sequence_origin_measurements.tsv),
[images](sequence_origin_images.tsv), [deltas](sequence_origin_delta.tsv) and
[public partitions](sequence_origin_partitions.tsv). Full regression, checkpoint,
C and focused sanitizer gates pass. The three public reload failures remain;
this is not complete Job removal or accepted-source promotion.
The obsolete result-Context reverse reader is deleted; callable provenance no
longer depends on inspecting a classifier-formation Job chain.
Context binding also borrows a completed producer without another validation Job.
Existing validation keys stay stable; exact parent/Binder checks remain for
unfinished or invalid inputs. Boundary, full regression and sanitizer gates pass.
The five-input [census](scope_inputs_ready_measurements.tsv) and [images](scope_inputs_ready_images.tsv)
are unchanged; this is not a measured runtime improvement or a completed resume gate.
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
Structural readers also borrow already checked Core/classifier Terms directly,
without a scheduler record. The by-value Term/pending-query view has no acceptance
or progress state. Existing provisional queries keep their identity and symbolic
snapshots. This removes read-only Jobs, not the broad source-work union or the
remaining centralized semantic dispatch; Oracle locality remains unfinished.
Lambda classifier, Handler carrier and Effect subsumption shapes now borrow
their actual input queries rather than allocating forwarding Jobs. Oracle-local
selection preserves provisional snapshots; shape readiness is still not checked
acceptance. See [deltas](structure_projection_delta.tsv) and
[measurements](structure_projection_measurements.tsv) against `9146893`.
Core-building queries and the four public reload failures remain unfinished.
Classifier-formation shapes likewise borrow the operand's classifier query;
Variable and Host classifier shapes borrow the declaration/type input directly.
The forwarding worker paths are deleted, not replaced by another stored graph.
Raw shape availability does not admit the formation or its Context. See
[applied deltas](classifier_projection_delta.tsv) and
[paired census](classifier_projection_measurements.tsv) against `755363a`.
The current public partition gate retains three failures; SE1-SE5 are unfinished.
Function graph synthesis now lives in the function owner: common source work
has no graph handle, case-layout slot, graph role, graph destruction or dispatch
branch. Source metadata retains only names/layouts, not a graph-worker backlink.
The graph owner lends its formation through the existing output view; no second
result is stored in the scheduler. Its source-interface readiness still exists,
so this is not complete Job removal or cursor persistence. See
[applied line changes](oracle_local_delta.tsv) and
[paired measurements](oracle_local_measurements.tsv) against `97825ec`.
Constructor values and data-case checking also live in the existing Schema
owner, not the shared source layout or dispatcher. Field inputs are borrowed
through the constructor owner; branch checking borrows its body from the request
instead of storing a second checked-term reference. Implicit-index conventions
and field checkpoints remain unchanged. This removes two broad-state roles,
not all Jobs; see [deltas](schema_owner_delta.tsv) and
[measurements](schema_owner_measurements.tsv) against `937daea`.
Scope validation now lives in Binding, with no private state or copied result;
it borrows its original Context after validating the lexical binder. Source and
binding post-checks retain two continuation pointers instead of the broad source
layout; their inputs and exports are borrowed. The real checks and pending Job
headers remain. See [deltas](source_check_owner_delta.tsv) and
[measurements](source_check_owner_measurements.tsv) against `19a166a`.
Match now owns its open result-type input; the separate result-slot Job and its
copied receipt are deleted. Constructor branches are allocated after schema
discovery; the existing conversion checks remain. See [deltas](match_owner_delta.tsv),
[measurements](match_owner_measurements.tsv) and [allocation](match_owner_allocation.tsv)
against `0c59383`. The next increment removes the propagation Job itself: the
existing application owns its interrupted source/type/check cursor and clears
it on completion. Ordinary no-work equations allocate no record. Argument
checking remains independent; no result cache, semantic tag or wire field is
added. See [deltas](application_owner_delta.tsv),
[measurements](application_owner_measurements.tsv) and
[allocation](application_owner_allocation.tsv) against `729d7d1`.
Body preparation now borrows its actual checking output instead of copying a
receipt/status; both await APIs follow that output. Repeated quotation uses the
existing Thunk recipe without waiting for its checked child, breaking the
double-quote/open-Match preparation cycle without premature acceptance. Against
`071742c`, QuickSort retains the same final typed graph and Job storage but
stores 1,447 fewer Job result references and uses 173 fewer dispatches. See
[measurements](quote_body_owner_measurements.tsv) and
[file deltas](quote_body_owner_delta.tsv). Four broad source roles and four
public reload failures remain; this is not complete Job/Evidence removal.
Application equations now follow existing Thunk/Lambda rule inputs instead of
reinterpreting those AST forms. Lambda scope/body come from the retained Pi
and body input; the actual Match alone keeps its open result equation. No new
Job, payload tag or wire field is added. See [file deltas](recipe_equation_delta.tsv)
and [final-count measurements](recipe_equation_measurements.tsv) against
`e58a1c8`; QuickSort uses three more dispatches, with unchanged graph counts.
Name registration, definitions and induction branches no longer allocate the
expression/Match/constructor state or pass through its central dispatcher.
Registration owns its existing index/frontier inline (88 bytes, previously a
184-byte state plus a separate 88-byte allocation). Definitions keep a body
input and activation (16 bytes instead of 184); branches keep four construction
references (32 instead of 184). Definition syntax/scope/exports are borrowed,
not copied. Only expression preparation still uses `source_work`; its remaining
central semantic paths and scheduler/Evidence cleanup are not complete. See
[measurements](definition_owner_measurements.tsv),
[wrapped allocation](definition_owner_allocation.tsv) and
[applied file deltas](definition_owner_delta.tsv) against `a8715d7`.
Lambda, Pi and surface quotation no longer use that central expression state.
Their Function/CBPV owners borrow lexical inputs from their request keys.
Lambda retains only its actual selected rule (8 bytes instead of 184), borrowing
Context/body from that rule's Pi/body inputs; Pi keeps 24 bytes, quotation 16.
Source inspection, Match demands and resume validation use the same lexical
inputs, not the old descriptor. No Core tag, rule graph, acceptance store or
wire field is added. See [measurements](source_oracle_measurements.tsv),
[allocation](source_oracle_allocation.tsv) and [applied deltas](source_oracle_delta.tsv)
against `2a0ba44`. This reduces retained state, not total source line count;
actual construction/checking consolidation and public resume remain unfinished.

Lambda/quotation, source/binding assertions and induction branches now borrow
their selected checking output instead of copying it into the preparation
header. Consumers read that owner; Lambda's genuine function/family choice and
post-synthesis checks remain. See [census](source_results_measurements.tsv) and
[applied deltas](source_results_delta.tsv) against `7d432d9`: QuickSort removes
533 result references, with unchanged graph/step counts and Job bytes. This
adds 30 implementation lines, not a source-size or speedup improvement. The
first IH-demand reader regression is fixed and covered by a small test;
full fixed-trial regression/acceptance, seven checkpoint and five C gates pass.
Focused ASan/UBSan/leaks and fresh/current assembly tests also pass. No new graph/tag/wire field is
added; full owner/frontier consolidation and public reload failures remain open.

Source blocks now own their sequence cursor in CBPV, without the expression's
second state allocation or completed receipt copy. Their syntax borrows the
existing source key, and Match demand discovery borrows visited scopes rather
than recreating them. See [census](block_owner_measurements.tsv),
[allocation](block_owner_allocation.tsv) and [applied deltas](block_owner_delta.tsv)
against `f459c0f`: QuickSort Job bytes fall 53,544, with unchanged graph/step counts.
Central synthesis loses 156 lines, but total implementation grows 51; this is
state consolidation, not total source reduction. Focused/fresh/current and
sanitizer tests, five C gates, full regression/acceptance, semantic persistence
and seven checkpoint gates pass, including general QuickSort Sorted/permutation.
Artifact controls are byte-identical; the four public reload failures stay open.

Fold construction now borrows clause inputs synchronously. The provisional
owner's clause array and the checked handler's temporary raw array are deleted;
only the unfinished cursor remains. Core retains no callback/owner pointer and
ordinary signature, carrier, effect and continuation checks still govern
acceptance. See [census](fold_inputs_measurements.tsv),
[allocation](fold_inputs_allocation.tsv) and [applied deltas](fold_inputs_delta.tsv)
against `33b06f1`. Graph/fuel counts and artifact controls are unchanged; source
grows by 27 implementation lines. This is not full Job/Evidence removal or
resolution of public reload failures.

IADT declaration preparation now lives with Schema, borrowing scope/syntax from
the existing request key. Central declaration dispatch/allocation slots are
deleted: declaration state shrinks 160->48 bytes and other expressions 160->144.
Conditional Universe, nominal Self and relocation checks remain authoritative.
See [census](declaration_owner_measurements.tsv),
[allocation](declaration_owner_allocation.tsv) and [applied deltas](declaration_owner_delta.tsv)
against `5e9187c`: QuickSort Job bytes fall 104,304; other graph/fuel counts and
artifact controls match. Full regression/acceptance, seven checkpoint and five
C gates, sanitizers and fresh/current checks pass. Central synthesis loses 155
lines, but total implementation grows 64. This is not full Job/Evidence removal;
the four public reload failures remain open.

Source App preparation now lives in Function, borrowing scope/syntax from its
exact key and its completed output from ordinary checking. Central App/callable
slots, separate application-state allocations and result copies are deleted.
Local progress is inline (120 bytes, formerly 144 + separately allocated 96);
other expressions shrink 144->128. Constructor indices, lexical IH, open Match
equations, CBPV sequencing and post-checks remain unchanged. See
[census](source_application_measurements.tsv), [allocation](source_application_allocation.tsv)
and [applied deltas](source_application_delta.tsv) against `0fb14e9`: QuickSort
loses 2425 result references and 122024 Job-layout bytes, not total RAM. Central
synthesis loses 542 lines, but total implementation grows 83. Full regression,
seven checkpoint/five C gates, sanitizers and fresh/current checks pass; artifact
controls match. Four public reload failures and SE1-SE5 remain open. No new task
graph, semantic tag, codec or production promotion is included.

Core-preserving Context projection and type/value readings now borrow their
input's structural query without a second Job/result. All three structural
APIs use one iterative discovery path; ordinary rule checking remains separate.
Fresh accepted inputs read typed data, not a stale symbolic snapshot; retained
raw views are unchanged. See [measurements](transparent_structure_measurements.tsv)
and [applied deltas](transparent_structure_delta.tsv) against `ba1e0d7`.
This removes redundant scheduled requests, not the remaining construction
owners, semantic checks or public-resumption failures.

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

2026-10-03, Context maps against `efc55b1`: delete the copied Core-value slots.
The existing typed images supply values through the same substitution input;
only the positional binder index remains, preserving constant-time reads.
Each map entry is 16 rather than 24 bytes on the tested 64-bit build.
[Census](map_images_measurements.tsv) and all 52 partition images are unchanged;
[arena samples](map_images_allocation.tsv) show smaller requested byte totals,
not measured peak RAM or a universal speedup. [Deltas](map_images_delta.tsv)
are net +13 implementation, +15 tests. Full regression/checkpoint/C/sanitizer
and fresh/current checks pass. The three public reload failures and full
Job/Evidence ownership work remain open; this is unpromoted prototype code.

2026-10-03, Effect substitution against `c1d93d3`: remove the intermediate
binding array and temporary flattened key. Existing substitution construction
accepts one stable synchronous array/reader view; only its immutable environment
is retained. The completed Term pointer survives lower work-store destruction,
without copying the Term. Array/reader reuse, identity prefixes, shadowing,
input lifetime and zero/split fuel are tested. [Census](effect_images_measurements.tsv)
is unchanged; [deltas](effect_images_delta.tsv) are net +19 implementation,
+48 tests, not source reduction. [Allocation samples](effect_images_allocation.tsv)
exclude the removed direct-malloc buffer, and establish no universal speedup.
Full regression/checkpoint/C/sanitizer and fresh/current gates pass; public
partition images match but the existing three reload failures remain open.

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

The 2026-10-02 borrowed-premise trial removes preparation/checking's unconditional
temporary input arrays. Indexed readers and array callers share the same named
rule checks; only variable-arity APIs materialize needed tail slices. Readers
are synchronous borrowed arguments, not stored Job/Evidence records. The paired
[census](borrowed_premises_measurements.tsv) is unchanged;
[direct malloc calls](borrowed_premises_allocation.tsv) decrease. This does not
delete the remaining owners or establish a general speedup. The
[source deltas](borrowed_premises_delta.tsv) show a net increase, not a line-count
reduction. Verification status remains in the parent plan.

Rule import also borrows its existing topological pointer map through that
reader, instead of scanning for maximum arity and reconstructing a premise
array. The temporary relocation map remains necessary; no new Job, retained
graph or image field is introduced. [Applied-file deltas](import_inputs_delta.tsv)
are +17/-11 implementation and +21/-1 tests against `b3aa9e6`, not a net source
reduction or a measured speedup. Status stays in the parent plan.

Mapped Context-projection receipts now borrow their typed Context/origin's
stable admissions. Alternate exact receipts remain sparse selections; ordinary
variable images without an origin still retain their real inputs. Against
`ca9a17c`, QuickSort retains 4,146 fewer premise references, with unchanged
steps and Term/Occurrence/Evidence/Job counts. See
[measurements](projection_receipts_measurements.tsv) and
[applied deltas](projection_receipts_delta.tsv). This removes duplicate references,
not object witness Terms, all Jobs or the existing public-resumption failures.

The remaining central expression state no longer has unused `tail`/`function`
slots or a preparation guard reading the always-NULL latter slot. Private state
shrinks from 184 to 168 bytes, without new state or a new owner. QuickSort's
measured Job layout shrinks by 102,864 bytes; other census fields are unchanged.
See [measurements](source_state_trim_measurements.tsv) and
[applied deltas](source_state_trim_delta.tsv), net two implementation lines removed.
This is stale-state deletion, not removal of the remaining source cursors.

2026-10-02, literal-input deletion against `be4a61e`: `@`/Int32/Text borrow
ordinary rule requests, with no source wrapper or LITERAL checkpoint record.
Exact typed Contexts remain distinct; lexical maps do not distinguish closed
literals. Annotation restoration compares the descriptive header, then ordinary
checking/projection validates its Context and type. This does not trust a saved
completion flag or use `::` for synthesis. [Census](literal_inputs_measurements.tsv)
preserves final typed/Evidence/query counts and removes 262 QuickSort Jobs;
[applied deltas](literal_inputs_delta.tsv) are -13 implementation, +72 tests.
[Inputs](literal_inputs_inputs.tsv) pin sample hashes, including the previously
recorded standalone QuickSort/LocalSorted source; the import-only C fixture is
not a census substitute. [Allocation](literal_inputs_allocation.tsv) measures
partial cumulative arena requests, not peak RAM or a universal speedup.
The repeated QuickSort sample varies between processes; both observations are
retained rather than claiming a deterministic allocation saving.
Verification and the four unresolved public reload failures stay in the parent
plan; this does not complete SE1-SE5 or promote the prototype.

2026-10-02, source-export projection against `41e5f48`: saving borrows the
existing checked/raw/pending rule inputs instead of copying their entire DAG.
The writer uses one synchronous header/child reader; no semantic state or wire
field is added. Detached-input export then remained for test/checkpoint
consumers; the borrowed-export increment below deletes it.
[Write measurements](export_inputs_measurements.tsv) preserve all
40 paired images and remove 30,078 external arena requests / 5,268,528 aligned
requested bytes in the checked general QuickSort LocalSorted export. This is
partial cumulative save allocation, not peak RAM or compilation speed.
[Census](export_inputs_state.tsv) is unchanged. [Applied deltas](export_inputs_delta.tsv)
are net +180 implementation and +151 tests: actual DAG-copy deletion does not
imply a line-count reduction. Verification and the same four unresolved public
reload failures are recorded in the parent plan. This is not full Job/Evidence
removal or production promotion.

2026-10-02, assertion-owner deletion against `1e707e6`: parsed `::` operands
request the existing post-check directly; the broad expression wrapper and its
preparation/dispatch are removed. Operation aliases follow the actual input,
without copied metadata. No annotation supplies missing synthesis and no wire
field is added. [Census](assert_inputs_measurements.tsv) removes five List Jobs,
1,200 Job-layout bytes and ten dispatches; final typed counts remain unchanged.
[Applied deltas](assert_inputs_delta.tsv): -12 implementation, +88 tests.
Full regression/acceptance, seven checkpoint/five C gates, sanitizers and clean/
current checks pass. Four public reload comparisons still fail; this is not
complete Job/Evidence removal or production promotion. Status stays in the plan.

2026-10-02, conversion receipts against `917b1bc`: normalization borrows its
typed origin; conversion/subsumption borrow origin/type admissions. The existing
sparse layout preserves exact alternate proofs and certificates; unchanged
reclassifications without typed edges retain real inputs. No graph/tag/field or
checking shortcut is added. [Census](conversion_receipts_measurements.tsv)
removes 4,452 QuickSort premise references, with other fields unchanged; this is
not a RAM/speedup measurement. [Deltas](conversion_receipts_delta.tsv): +7
implementation, +52 tests. Regression, checkpoint/C gates, sanitizers and fresh/
current tests pass; 52 List images match. Four public reload failures stay open.

2026-10-02, handler-wrapper deletion against `b307e8d`: multi-clause syntax
requests the existing Handler owner directly, without the broad expression
wrapper, another completion record or new wire fields. Source transport borrows
that owner's scope/syntax; return-only syntax keeps generalized sequencing and
explicit carriers keep ordinary rule export. [Census](handler_direct_measurements.tsv)
removes 10 Jobs, 10 result copies, 2,400 Job-layout bytes and 20 dispatches in
effect-application; final typed counts remain unchanged. [Deltas](handler_direct_delta.tsv):
implementation net +13, tests +65, not a net code reduction or peak-memory
claim. Regression, checkpoint/C gates, sanitizers and fresh/current checks pass,
including partial/complete source roundtrips and step 0. All 52 List images match
the parent; the same four public reload failures remain. Prototype only.

2026-10-02, upper-layout deletion against `30bf913`: `source_work`,
`EXPRESSION_JOB` and the shared semantic dispatcher are removed. Match,
lexical-reference, module, graph-reference and return-sequencing owners keep
private suspension data and borrow their key's scope/syntax. Projections replace
repeated central source dispatch; no new Term tag or artifact field is added.
[Census](source_owners_measurements.tsv) changes only Job-layout bytes, saving
327,656 bytes on completed QuickSort; counts and steps stay identical.
[Deltas](source_owners_delta.tsv): implementation net +63, tests net +51,
not code reduction or a peak-memory/speedup claim. Regression/acceptance,
checkpoint/C gates, sanitizers and fresh/current checks pass. All 52 List images
match; the same four public reload failures remain. SE1-SE5 stay open.

2026-10-02, borrowed export against `771b022`: the remaining detached rule-DAG
copy is deleted; test/checkpoint/semantic writers borrow existing inputs, without
a compatibility copier. [Deltas](borrowed_export_delta.tsv): implementation net
-15, tests net -1. [Census](borrowed_export_measurements.tsv) and all 52 public
partition images match the parent. Full regression, affected transport/C gates,
sanitizers and fresh/current checks pass; the same three public reload failures
remain. This is not complete Job/Evidence removal or production promotion.

2026-10-02, remaining receipt overlap against `e3b1216`: ten rules borrow their
existing typed Context/type/operand/map inputs; only independent premises and
different exact selections are stored. [Census](remaining_receipts_measurements.tsv)
preserves all fields except retained references, removing 30,594 from completed
QuickSort. The captured sample is `function-graph-captured-match.p` (not the
earlier captured-request sample). [Deltas](remaining_receipts_delta.tsv):
implementation +29, tests +151, not line reduction or a RAM/speedup claim.
Regression/acceptance, semantic/checkpoint/C gates, sanitizers and fresh/current
checks pass. All 52 images match; three public reload failures remain. No new
graph, tag or wire data is introduced. SE1-SE5 are unfinished; prototype only.

2026-10-02, Reindex rule unification against `9e9377b`: remove the dedicated
worker/action-pointer state; convenience and ordinary `PG_REINDEX` requests
share one rule owner and its Context checker. The existing Occurrence action
retains substitution progress, not another copied payload. Exact receipt
selections remain distinct; checked results allocate no adapter or header.
Interrupted actions are refused by the restricted premise-cursor checkpoint.
[Census](reindex_rule_measurements.tsv) preserves counts and steps except rule
classification/layout; completed QuickSort grows 7,680 Job-layout bytes.
[Applied deltas](reindex_rule_delta.tsv): implementation net +5, tests net +78.
Regression, checkpoint/C gates, sanitizers and fresh/current checks pass. All
52 public images match the parent, including the still-failing three reload
comparisons. This is neither full Job/Evidence deletion nor production promotion.

2026-10-02, borrowed Handler clauses against `e6dd1b3`: ordinary rule checking
and typed Fold rebuilding no longer reconstruct temporary clause arrays/arenas.
One checker reads each visited clause once; Core construction borrows its actual
typed inputs. The array convenience entry has no separate checker or allocation.
Exact receipts, signature checks and forwarding remain. [Census](handler_reader_measurements.tsv)
preserves all fields on five samples; [allocation](handler_reader_allocation.tsv)
saves four arena requests/64 aligned bytes on the Handler sample, not peak RAM
or a speedup. [Deltas](handler_reader_delta.tsv): implementation net +39, tests
net +48, not code reduction. Regression/acceptance, checkpoint/C gates, sanitizers
and fresh/current checks pass. The 52 List images and completed Handler image
match; the same three public reload failures remain. No new Job/tag/wire fields;
SE1-SE5 remain open, and this is an unpromoted prototype.

Evidence's eleven typed-query roles no longer use a shared Oracle-state union
or cross-role initializer. Each keeps only its actual cursor; phase checking
has zero private bytes and classifier lookup one input pointer. The existing
interner, exact chosen receipts and checking path remain. See the parent SE1
work list and [paired census](typed_owner_measurements.tsv): only query-layout
bytes change, with 949,336 fewer bytes on completed QuickSort. Actual code is
net +71 lines, not a source reduction. SE1-SE5 and three public reload failures
remain open; this is an unpromoted prototype.

Logical-family application now borrows its selected parameter query directly.
`FAMILY_DOMAIN_JOB`, its factory and copied result are removed; its caller's
existing phase avoids repeated argument discovery while suspended. Convertible
but non-alpha-equal domains retain the ordinary post-check, tested at chunks
1/7/64 alongside wrong-domain rejection. [Fixture counts](family_domain_census.tsv)
compare identical O0 tests against `fa6c916`; the one-off GDB probe uses test
line locations pinned to this increment, not a permanent timing gate. The five
existing census inputs are unchanged; [applied code](family_domain_delta.tsv)
grows one line. This removes a work owner, not all Jobs or Evidence. Full gates
pass; the same three public reload failures remain open.

### Borrowed Admission Inputs

Parent `ffc5cb1`, 2026-10-02: inductive formation, Match/induction, TypeCase,
substitution extension and family Identity no longer allocate flattened receipt
arrays just for lookup/admission. The existing interner borrows synchronous
readers and retains only required edges; exact alternative receipts remain.
The new caller-lifetime and invalid-input tests pass on parent and candidate.
Full regression, persistence/checkpoint, C and focused sanitizer gates pass.
This is unpromoted prototype work; SE1-SE5 and three public reload failures
remain open. All 52 public partition images and the report equal the parent.

[Applied source deltas](admission_inputs_delta.tsv) are implementation +113/-80
and tests +49/-0. [Five-input counts](admission_inputs_measurements.tsv) are
identical at each cumulative budget 0/100/1000/10000000. Inputs and measured
binaries are pinned in [the hash inventory](admission_inputs_inputs.tsv).
[Allocation rows](admission_inputs_allocation.tsv) count cumulative successful
requests: external `pg_alloc` uses aligned requested bytes; linked malloc/calloc
uses requested bytes. They are separate probes, not additive live-memory totals,
and do not include every libc allocation. QuickSort saves 1,103 external arena
requests and 53,280 aligned bytes; no general speedup or peak-RAM claim.

Reproduce counts with `artifact_state_audit` from the existing artifact
`build.mk`, selecting `ARTIFACT_TESTS=$overlay/artifact_tests/`, and pass the four
budgets above. Build `artifact_allocation_audit` for the arena probe. For the
malloc/calloc probe, build `artifact_state_audit` with the existing
`borrowed_input_audit.c` in `CFLAGS` and linker flags
`-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=pg_program_destroy`; pass only
10000000 to avoid different sampling I/O. Use the parent plan's
[Reproduction](../../../doc/2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md#reproduction)
to flatten the QuickSort fixtures. Both measurements use O2 frozen source;
unrelated accepted-source edits are excluded. Logs use
`/tmp/a-program-admission-inputs-`.

Operation-label discovery now reads existing lexical producers directly.
`OPERATION_REFERENCE_JOB` and both adapter APIs are deleted, not replaced by
another worker. Handler still awaits the source's own checking result; aliases,
quotes, modules, pending signatures, failed assertions and cycle refusal are
covered. Against `ffd2fcf`, [applied source](operation_origin_delta.tsv) shrinks
implementation by 74 lines; [paired counts](operation_origin_measurements.tsv)
remove 11/5 Jobs from completed Effect/Handler samples without changing their
Term/Occurrence/Evidence counts. Full regression/acceptance, persistence/checkpoint,
C and focused sanitizer gates pass; SE1-SE5 and the same three public reload
failures remain open. This is an unpromoted prototype increment.

Declared types now borrow checked Context fields or pending plain-extension
operands rather than allocate lookup Jobs per ancestor. Pending source telescope
discovery remains; visibility is not admission. [Paired counts](declared_type_measurements.tsv)
against `0b9d421` remove 189 Jobs from completed QuickSort and 131 from Effect,
without changing completed Term/Occurrence/Evidence counts. [Applied deltas](declared_type_delta.tsv)
are implementation net +1 and tests +43, not code shrinkage. Full regression,
examples/acceptance, persistence, checkpoint, C and focused sanitizer gates pass.
The 52 public images/report match; three reload failures and SE1-SE5 stay open.
This is unpromoted prototype work. See [pinned inputs](declared_type_inputs.tsv).

Variable/Universe/Host leaf descriptions no longer allocate structural Jobs.
Their Core shape does not grant admission: invalid premises/overflow still
reject. Against `464f6aa`, [paired counts](leaf_structure_measurements.tsv) remove
10 QuickSort Jobs/18 steps with unchanged completed typed/proof counts;
[applied implementation](leaf_structure_delta.tsv) is net -2 lines. Full O2
regression/examples/acceptance, focused sanitizer, persistence/checkpoint, C
and dirty-current gates pass. SE1-SE5 and three public reload failures stay open.
This is unpromoted prototype work; [hashes](leaf_structure_inputs.tsv) pin it.

Telescope traversal and Context checking now share one cursor owner. Delete the
second role/factory, Schema's duplicate pointer and the copied Context receipt;
existing preparation/completion notification preserves provisional discovery
without acceptance. Against `b54fdcb`, [census](telescope_owner_measurements.tsv)
removes 68 QuickSort Jobs/result references and 115 dispatches, with unchanged
completed typed/proof counts. [Implementation](telescope_owner_delta.tsv) is net
-5 lines; [hashes](telescope_owner_inputs.tsv) pin the comparison. Full regression,
persistence/checkpoints, C, sanitizer and fresh/current gates pass. The three
public reload failures remain; this is unpromoted, unfinished SE1-SE5 work.

Return-only handlers now share the existing sequencing owner directly; delete
`SOURCE_RETURN_HANDLER` state/dispatch. Eight preparations borrow checked output
instead of copying receipts; Context/carrier/index checks and genuine nullary
construction remain. Against `2bebfca`, [counts](return_owner_measurements.tsv)
remove 369 QuickSort result references, not Jobs or layout bytes. Handler loses
one dispatch but gains 16 layout bytes overall: [role counts](return_owner_roles.tsv)
identify changed discovery allocations. [Implementation delta](return_owner_delta.tsv)
is +39/-39, tests +70/-15; [hashes](return_owner_inputs.tsv) pin sources/binaries.
Focused O2/current-worktree, persistence/checkpoint, C and sanitizer gates pass;
full O2 regression/examples/acceptance exits 0. All 52 List images/report
match the parent; three public reload failures and SE1-SE5 remain open.
This is unpromoted prototype work, not complete Job/Evidence removal. Role counts
inspect a completed Handler run with GDB at `state_audit.c:census` on separate
`-O1 -g` builds; count/layout values agree with the O2 census, not total RAM.

Constant motive discovery now returns the existing constant-result request;
delete its wrapper Job. Telescope closing follows Scope parent inputs without a
scratch array, and borrows checked output. Against `486d64d`, [paired counts](constant_owner_measurements.tsv)
remove 65 QuickSort Jobs/130 dispatches/229 result references, with unchanged
completed Term/Occurrence/Evidence counts. [Applied implementation](constant_owner_delta.tsv)
shrinks by 23 lines; tests grow by 14. Full O2 regression/examples/acceptance,
persistence/checkpoints, C, sanitizers and fresh/current focused checks pass.
All 52 List images/report match; the same three reload failures remain.
[Hashes](constant_owner_inputs.tsv) pin the comparison. The [allocation/time probe](constant_owner_allocation.tsv)
varies by process and ran with other tests; it establishes no general speedup.
This is unpromoted prototype work; SE1-SE5 remain unfinished.

Named variable-arity checking now borrows the same input view as admission;
`premise_slice`, Match's copied branch list and IH's branch scratch arena are
deleted. No view escapes, and exact proof selections remain distinct. Against
`2f2e6d1`, [all paired rows](derivation_inputs_measurements.tsv) agree;
[external arena requests](derivation_inputs_allocation.tsv) save 1,075 calls /
51,600 aligned bytes on QuickSort, not a peak-RAM or speedup claim.
[Implementation](derivation_inputs_delta.tsv) is +152/-168, tests +204/-170;
[hashes](derivation_inputs_inventory.tsv) pin the tested sources and inputs.
Full O2 regression/examples/acceptance, persistence/checkpoints, C and focused
ASan/UBSan/leaks pass; the assembled candidate exactly matches the tested files.
The same three public reload failures remain; SE1-SE5 are unfinished.

Effect contribution no longer mirrors each join as child Jobs or forwards
shape discovery to a second contribution Job. One demand advances the existing
Term DAG; temporary traversal storage is lazy and freed at completion/failure.
Against `d1b8290`, the [shared-DAG probe](effect_frontier_sharing.tsv) removes
514 of 515 Jobs at depth 512, not 514 Terms. [Real inputs](effect_frontier_measurements.tsv)
preserve completed typed/proof counts; Effect/Handler remove 40/15 Jobs.
[Applied implementation](effect_frontier_delta.tsv) grows by 67 lines; regression
tests grow by 86, excluding the standalone probe and Make fragment. Traversal's
step grain changes; [allocation ranges](effect_frontier_allocation.tsv) establish
no general speedup. Full O2, persistence/checkpoints, C and focused sanitizer
gates pass; [hashes](effect_frontier_inventory.tsv) pin the tested candidate.
The same three public reload failures and SE1-SE5 remain open. Prototype only.

Dependent-field checking also borrows its parameter/Self prefix from the existing
Context map, and its suffix Core/binder from original Occurrences/declarations.
The binding copy, unchanged-prefix reduction slots and congruence target-image
scratch array are removed. Original versus reduced fields still require genuine
receipts; no acceptance rule or persistent graph is added. Against `7d72b62`,
[all census rows](field_inputs_measurements.tsv) and 52 List images/report agree,
including the same three unresolved public reload failures. Full O2 regression,
persistence/checkpoints, C and focused sanitizers pass; 64-parameter and split-
budget tests pass in fresh and current-user-edit trees. [Actual source deltas](field_inputs_delta.tsv)
are implementation +28 and tests +41, not a line-count reduction. [Hashes](field_inputs_inventory.tsv)
and [partial allocation samples](field_inputs_allocation.tsv) pin this unpromoted
prototype increment. The broad rule header is checking metadata, not a copied
Oracle Term; packing it and reconstructing consumers is not an adopted solution.

Typed WHNF/NF now reuses the existing resolved-input owner without rewriting
pending keys; mode, force, scope and exact receipts remain distinct. Handler
admission borrows its one-read clause snapshot, not another flattened premise
array. Source restoration discards the raw imported rule DAG after copying its
exact requests; referenced semantic objects remain in their real graph. Deep
import, invalid premises, recursive/effect I/O and focused sanitizers pass.
Against `0e3865d`, [paired census](checked_owners_measurements.tsv) is unchanged.
[Import requests](checked_owners_import_allocation.tsv) remove 7,552 externally
requested persistent-arena bytes on QuickSort, not RSS or artifact bytes.
[Deltas](checked_owners_delta.tsv) are implementation net +24, tests +107,
separate census/build +60; [hashes](checked_owners_inventory.tsv) pin verification.
Full O2 regression/acceptance, persistence/checkpoints, C and fresh/integration
checks pass. All 52 public List images match; the same three reload failures
remain. Prototype only; SE1-SE5 and complete Job/Evidence consolidation are open.
The optional `import_allocation.mk` census accepts an existing `.a`, performs
no Solve or admission and excludes initialization, graph.c internals and scratch.

Rule import now writes caller-owned root slots; source restoration discards that
array with its existing scratch. The shared image's whole Term lookup table is
also read-local, while actual Terms and public roots keep their semantic lifetime.
Against `149eca0`, [census](relocation_inputs_measurements.tsv) and all 52 List
images/report agree; the same three reload failures remain. [Read allocations](relocation_inputs_import_allocation.tsv)
decrease without smaller files; QuickSort samples have process variation.
[Deltas](relocation_inputs_delta.tsv) are implementation +7 and tests +23 net,
not a code-size reduction. [Hashes](relocation_inputs_inventory.tsv) pin inputs.
Focused, persistence/checkpoint, C, sanitizer and fresh/integration checks pass;
full O2 regression/examples/acceptance exits 0, including general QuickSort.
Prototype only; SE1-SE5 remain open.

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
