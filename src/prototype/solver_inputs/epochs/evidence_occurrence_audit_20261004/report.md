# Evidence can lose its structural receipt graph; checked facts must move with it

Date: 2026-10-04 UTC. Read-only authority audit, first substantive checkpoint.
Producer: committed accepted Main `c205d507a8568bfdda355e247a78b4e67464f294`.
All source references below refer to that revision, not the worker's old accepted
files or Main's dirty files. The audited Evidence/typing/derivation files also
match `fc52755b` exactly. Source pins and the exhaustive 53-rule disposition map
are adjacent. No new build, execution, cost measurement or accepted edit was made
for this audit.

Evidence does **not** contain a second copy of every Term payload. It nevertheless
allocates a separate receipt node, proof-index entry and admission-chain link for
each accepted rule use, including rules whose entire construction already lives
in Occurrence. Those nodes and their repeated input traversal are a real deletion
target. Borrowing premises reduced edges; it did not remove this graph.

The supported direction is checked Occurrence/Scope/map owners and owner-local
remaining facts. A copied descriptive tuple cannot become a theorem. A generic
replacement receipt wrapper would preserve the same problem. The sections below
identify actual independent information and distinguish it from proof-history
identity that the current APIs happen to preserve.

## Problem List

1. EOA1 / existing SE2-SE4: remove unnecessary Evidence authority, graph storage
   and repeated construction while preserving checked facts and genuine frontier.
   The focused work list remains Main's
   `doc/2026-10-04-EVIDENCE-OCCURRENCE-AUTHORITY-AUDIT-AND-PLAN.md`; the single SE
   implementation/status list is not replaced by this report.

## Subjective (User)

2026-10-04 12:58 UTC, English paraphrase of the explicit human request through
inquiry desk019ebfae: Evidence may rebuild the Term tree pointlessly; investigate
deletion or integration into typed Occurrence and summarize in Markdown.
The earlier human priority correction supersedes minor epochs. Speed and peak
memory must improve by removing unnecessary mechanisms and work while retaining
indispensable checked facts, reusable results and resume progress.

## Objective (Code)

### Physical ownership and disposition

| Current component | Existing typed/semantic counterpart | Independent information and actual reader | Disposition |
| --- | --- | --- | --- |
| `pg_evidence.conclusion` union | Exact Occurrence, Context or map pointer | Back-reference for Evidence APIs; no copied Core, classifier or Context payload (`evidence.c:14`, `5831`) | Delete with the separate receipt object; return the existing typed owner directly in migrated APIs. |
| `rule`, global 53-rule enumeration | Core operation owner, existing judgement, typed origin/selection/maps and local construction checks | `derivation.c:73`, named rule checking and legacy transport select a request; a tag alone does not certify it | Remove from live structural admission. Retain legacy transport dispatch during migration, then move residual dispatch to existing semantic owners; no second global Oracle enumeration. |
| `index`, `typing.proofs` | Existing occurrence/scope/map interners | Exact receipt identity and derivation-input cache, including preconstruction lookup (`evidence.c:371`) | Remove structural receipts from this index, then delete the index after remaining owner migrations. Retain preconstruction sharing on existing owner/query keys; do not rebuild fresh binders just to deduplicate later. |
| `owner` | Canonical typed-owner membership and `typing.owner_key` | Reject foreign admission (`evidence.c:71`, `5827`) | Integrate admission provenance in canonical typed owners. Membership and validation remain distinct; a copied link/key must not pass. |
| `next_conclusion`, typed `receipts`, `empty_receipts` | Owner-linked circular alternative chain | Enumerates exact alternate receipts and stabilizes the first choice (`evidence.c:51-98`) | Delete structural alternative chains when callers consume a checked typed subject. The NULL Context needs one local empty-context admission fact, not a rule node per use. |
| `retained_count`, direct structural premises | `type`, `operands`, `origin`, `selection`, scoped inputs | Many direct rules already store zero Evidence premises; exporters still traverse typed dependencies (`derivation.c:7-49`) | Delete the structural receipt itself, not another premise wrapper. |
| Sparse `(ordinal, proof)` selections | Exact same typed premise subject can have several receipts | `find_record_inputs`, `pg_evidence_premise`, transport equality (`evidence.c:35`, `425`, `5868`) | Current contract, not demonstrated extra typing meaning. Eliminate from semantic construction after migrating exact-history consumers; preserve old image/request behavior explicitly until qualified. |
| Scope certificate for Context/Pi/family abstraction | Existing `pg_scope`: Context plus selected typed declaration and index telescope | Same raw Context can have different selected formation bounds; `evidence_scope.c:8-29`, `derivation.c:35` need that selection | Use the existing Scope as checked declaration owner. Do not replace it with raw Context acceptance alone or add another telescope graph. |
| Substitution certificate is an exact prefix receipt | Full typed map stores source/destination/images; it does not store the requested prefix decomposition | Logical arity and old receipt identity differ (`evidence.c:4645`, `4757`) | Check full map once through ordinary substitution; migrate semantic callers to checked map. Keep prefix request/frontier while checking and old transport provenance until compatibility controls pass. |
| Conversion certificate | Reclassified Occurrence retains source origin and target formation | Successful ordinary pure comparison of these exact endpoints (`conversion.c:8`, `evidence.c:4548`) | Retain the checked relation, attached to its existing local comparison/result owner or validated typed construction; delete global receipt packaging. No raw classifier retagging. |
| Reduction certificate | Derived Occurrence retains origin and target Core | Pure policy, reduction kind and endpoint relation (`evidence.c:4586`, `derivation.c:53`) | Retain needed endpoint/kind facts on the existing evaluator/checking owner. NF completion cannot replace a requested intermediate WHNF/prefix endpoint. Performance owns evaluator lifetime/readback. |
| Inductive schema certificate | Existing Oracle-local schema/signature, nominal declaration and checked field/result maps (`iadt.c:324-346`) | Positivity, Universe bound and declaration-specific formation, absent from a bare Core Ref (`evidence.c:706`) | Keep on the existing IADT owner; remove global Evidence node once APIs take the checked schema/fiber owner. No Core-to-global-schema search or new proof graph. |
| Constructor certificate and three independent premises | Core names exact constructor; Occurrence retains fields and result formation but lacks parameter/instance maps | `constructor_instance` checks parameter prefix and Self before construction (`evidence.c:2192`) | Constructor identity can come from the existing Oracle view. Preserve checked parameter/instance maps as existing typed inputs before deleting those edges; do not infer Self from layout/arity. |
| Request declaration certificate | Existing nominal operation owner already retains payload/response formations | Exact signatures belong to label/declaration, not only effect-row membership (`evidence.c:120`, `5384`) | Borrow the checked operation owner; remove global receipt packaging. Signature authority stays local. |
| Handler signature and clause signature proofs | Core's handler owner already stores ordered nominal labels/positions; Occurrence stores bodies and carrier | Clause signature checks and forwarding/subset equations (`computation.c:23`, `93`, `130`; `evidence.c:5535`) | Investigate deleting duplicate label signature via that owner view. Retain checked operation signature inputs locally; do not regard a label list as clause acceptance. |
| Waiting frames, typed queries and selected-input cursors | Existing ordinary query/substitution/WHNF/NF owners | Some are real charged dependencies and preserve partial checking or escaped pointers (`typed_query.c:119`) | Keep indispensable progress. Delete copied completion/status wrappers and dead state; never replace live frontier with replay or a second checker. |
| Exported `pg_derivation_input` closure | Temporary headers/premises rebuilt from checked typed inputs (`synthesis_derivation.c:385-425`) | Untrusted portable checking requests, not loaded Evidence (`derivation_io.c:310`) | Migrate the writer to direct typed-owner traversal and one existing input view. Loading remains inert/untrusted; resume information and legacy transport cannot be silently discarded. |

On this LP64 declaration layout, a receipt header occupies 64 bytes before
trailing storage; an alternative selection normally adds 16 bytes. This is a
static layout calculation from `graph.h:9` and `evidence.c:14`, not a new compiler
or RSS measurement. The receipt index's bucket capacity is additional overhead.
Zero retained premises therefore does not mean zero Evidence storage. Final
counts and requested bytes do not explain the reported peak scratch memory.

### All 53 rule families

Each enumerator is covered exactly once in `rule-dispositions.tsv`. The table
groups them to expose common ownership rather than reproduce a new live enum.
Logical premises can be borrowed even when this table says a receipt remains.

| Rules (names omit `PG_`) | Current storage / checked meaning | Proposed existing owner |
| --- | --- | --- |
| CONTEXT_EMPTY, CONTEXT_EXTEND, CONTEXT_FAMILY_EXTEND | Zero trailing inputs; selected Scope is the certificate for nonempty contexts | Empty typing fact / checked Scope. Context remains descriptive. |
| UNIVERSE_FORM, VARIABLE | One logical Context input, borrowed except NULL empty Context | Checked Occurrence and accepted scope; retain level/fresh binder checks. |
| TYPE_FROM_VALUE, VALUE_FROM_TYPE | Zero inputs; typed origin records judgement boundary | Checked boundary Occurrence; keep Universe/sort checks. |
| RETURN_TYPE_FORM, THUNK_TYPE_FORM | Zero receipt inputs; typed child and local F/U shape | Checked Occurrence through existing CBPV checker. |
| PI_FORM, TYPE_FAMILY_ABSTRACT | Zero receipt inputs plus selected binding Scope | Checked scoped Occurrence and existing Scope. Family sort remains distinct. |
| LAMBDA_INTRO, APP_ELIM, TYPE_FAMILY_APP | Zero receipt inputs; type/body or function/argument retained | Checked Occurrence; scope and dependent result equations checked once. |
| RETURN_INTRO, THUNK_INTRO, FORCE_ELIM, TOTAL_PURE_VALUE | Zero receipt inputs; operand/type and grade in typed structure | Existing CBPV owner; TOTAL and empty effects remain different facts. |
| THUNK_CONTENT, RETURN_CONTENT, PI_DOMAIN, PI_CODOMAIN, PI_CONSTANT_CODOMAIN | Zero inputs; origin/selection and optional argument identify inversion | Existing selected-input/typed-query result, with ordinary inversion admission. |
| RETURN_VALUE, THUNK_COMPUTATION | Often origin-borrowed; converted same-Core boundaries can retain the inverted producer instead | Checked CBPV boundary; preserve the conversion relation until integrated. |
| IDENTITY_FORM, IDENTITY_INSTANCE, REFLEXIVITY, IDENTITY_LEFT_TYPE, IDENTITY_RIGHT_TYPE, IDENTITY_TRANSPORT, IDENTITY_LIFT | Zero receipt inputs; typed operands/type/Core owner encode construction | Existing Identity typed owner/checker; export's introduction selection is separate legacy plumbing. |
| FAMILY_IDENTITY_FORM, FAMILY_ACTION | Typed maps/operands and sparse alternate selections | Existing Identity boundary/maps with checked path inputs. Alternate receipt identity is not a second path theorem. |
| TYPE_CONVERSION, PURE_NORMALIZATION, EFFECT_SUBSUMPTION | Origin/type inputs borrowed; first two have independent certificates | Checked relation on existing conversion/reduction/CBPV owner. No strengthening of effects/totality. |
| CONTEXT_PROJECTION, REINDEX | Origin/map/scope inputs borrowed, alternative receipts may differ | Checked Occurrence/map; keep scoped action and canonical sharing. |
| CONTEXT_SUBSTITUTION | Typed map/images plus independent empty Context and exact-prefix certificate | Checked map; prefix remains transient request/frontier or explicit legacy transport. |
| INDUCTIVE_FORM | Schema certificate; all logical schema premises borrowed | Existing checked IADT schema and fiber Occurrence. |
| CONSTRUCTOR_INTRO | Result type borrowed; formation/parameter/instance edges retained | Existing constructor/schema plus typed maps/fields; missing map ownership must be addressed first. |
| MATCH_ELIM, INDUCTION_ELIM | Typed scrutinee/branches/motive/formation/map/type/lexical allocation; sparse selections | Existing elimination Occurrence and checked IADT input owners. Retain motive transport, field scope and IH checking. |
| TYPE_CASE | Formation/parameter receipt edges independent; scrutinee/branch typed operands | Existing IADT type-case owner with explicit checked formation/parameter map; no computation-to-type inference. |
| FOLD_ELIM | Two logical operands borrowed; no certificate | Existing CBPV sequencing Occurrence, preserving dependent/result/effect checks. |
| REQUEST_INTRO | Four logical inputs borrowed through operation declaration and typed payload/continuation | Checked nominal operation and CBPV Occurrence. |
| HANDLER_ELIM | Carrier/computation/return/body inputs borrowed; two signature edges per clause independent | Existing handler/operation owners and checked clause Occurrences. |
| TERMINATION_FORM, TERMINATION_INTRO | Typed operands/type borrowed | Existing termination Oracle owner and checked Occurrence; intro requires TOTAL for this suspended term. |
| HOST_TYPE_FORM, HOST_VALUE_INTRO, HOST_FUNCTION_INTRO | Context/type input borrowed; closed host descriptor/signature checked | Existing closed host contract and Occurrence; arbitrary Ref/signature is not accepted. |

### Concrete constructions and counterexamples

**Lambda and application.** `pg_prove_lambda` (`evidence.c:4056`) checks the
selected Pi body scope and classifier, calls Core interning for one Lambda, and
creates an Occurrence containing that Pi formation and body. Its Evidence then
stores only the separate header, with zero premises. APP (`4483`) similarly
retains two operands and dependent result formation; its receipt adds no child
information. `pg_function_proof_inputs` (`evidence_function.c:201`) reads those
same fields again for export. Delete these receipt allocations by returning a
checked canonical Occurrence, with no replacement graph. A raw Lambda with an
unrelated body Context or wrong domain remains invalid; `typing.c:216` only
describes tuples and does not perform these checks. Interning equal Core alone
cannot establish them.

**Context.** `pg_prove_context_extension` (`evidence.c:3562`) checks the parent,
declared type, Universe and fresh binder; Context holds raw declaration and
parent, while Scope holds its selected typed formation. Scope is more than
another raw telescope copy: different selected Universe bounds can share the
same raw declaration. The correct merge target is this existing Scope with
canonical checked admission, not a bare Context boolean that forgets the
formation selection. `pg_context_parent_input`/`declared_input` currently go
Scope -> typed child -> separate Evidence lookup. That round trip can disappear.

**Substitution.** `pg_prove_substitution_extension` (`4645`) constructs one map,
checks every dependent image against the earlier image prefix and then allocates
another receipt. Projection and explicit extension can have the **same map**
and different receipts (`tests/derivation_io.c:567-579`); the semantic images
agree. Exact prefix/history is a real current transport/API contract, but this
counterexample does not prove a distinct logical substitution is necessary.
Delete its semantic receipt graph only after old request/byte contracts are
migrated explicitly. A merely constructed map or occurrence action still lacks
admission (`tests/synthesis.c:6097-6112`).

**Identity.** Existing tests build an identical typed boundary with default and
identity-reindexed endpoint proofs (`tests/identity.c:126-166`). They assert
different receipts for the **same subject**, with sparse retained selections.
The alternatives do not introduce different Identity Terms. However, replacing
the checked path with a value, changing the classifier, or using a foreign
typing owner is rejected (`59-81`, `189-201`). Keep those checked facts; migrate
receipt identity separately. `pg_identity_export_input` (`evidence_identity.c:34`)
searches historical introduction-rule receipts because the derivation grammar
expects those names. This is a concrete blocker to simply deleting the chain,
not evidence that a global history graph is mathematically indispensable.

**List and Sorted.** A constructor Occurrence contains checked fields and the
result family but not its full parameter/instance maps. The constructor checker
(`evidence.c:2192`) rejects a well-typed field substitution choosing the wrong
Self or parameter prefix. Layout/arity alone is insufficient. Match/induction
already keeps branches, motive, formation and parameter map in the typed owner
(`2849`); its remaining receipt header/selections can be integrated after
readers stop choosing construction from proof history. Sorted proofs must still
be ordinary function results, not trusted source flags. No new QuickSort census
was run: historical concatenated `#.terminates` rejection2/61489 is preserved;
the earlier genuine imported DONE provider measurements are distinct evidence.

**Acc.** `tests/iadt.c:598-645` uses actual logical family assumptions and an
Acc-shaped recursive function field. `pg_data_field_positive` and schema
formation establish positivity, checked telescope sort and Universe bounds;
the bare nominal Ref does not contain those facts. The typed IH field and its
application are ordinary checked constructions, not another proof AST. Removing
the schema fact without its local IADT owner admits negative occurrences or a
wrong motive (`tests/iadt.c:142-200`, `856-861`). Preserve the schema's checked
meaning while deleting global Evidence packaging and any duplicate field arrays
only when existing Scope links provide an exact, stable view.

**Conversion, effects and host locality.** A reclassified tuple can look correct
without a successful pure comparison; normalization additionally needs its
actual endpoint/kind. Existing import controls reject forged NF endpoints,
wrong reduction mode and overshoot (`tests/derivation_io.c:711-744`, `1971`).
An effect row is a set of nominal labels, not signature acceptance or TOTAL.
Host function admission checks a closed descriptor, each Pi domain and TOTAL
result (`evidence.c:3625`), not an arbitrary semantic Ref. These are irreducible
checks, but their independent facts can belong to existing local owners rather
than the 53-way global Evidence node.

## Assessment

Adopt **integration of structural admission into canonical typed construction**
as the main next SE2/SE3 refactor. The immediate coherent scope is direct
Universe/variable, ordinary Lambda/Pi/APP and F/U/RETURN/THUNK/FORCE/boundary
construction, including the corresponding checked inputs and consumers. Delete
their receipt allocation, proof-index insertion, admission-ring edge and repeat
receipt dependency traversal. Returning an existing Occurrence directly is a
real deletion; embedding the old Evidence header inside it is not.

Use ordinary owner checks to publish admission once. Prototype APIs should take
and return the existing Occurrence, Scope or map explicitly instead of allocating
a generic checked-handle wrapper. Constructors and decoders must ignore incoming
admission data. Exact local interner membership, foreign/copy rejection, lifetime
and immutable descriptive identity remain requirements. Account for any owner
field bytes/index overhead against deleted storage; do not promise a net gain
from this static report.

Defer global deletion of every receipt until the concrete independent facts and
legacy selectors above have migrated. Preserve conversion/reduction relations,
schema positivity, nominal signatures, scope selections and live checking
cursors on their **existing** owners. This finding rejects blanket deletion from
Term shape; it does not justify keeping the full separate Evidence DAG forever.
Alternative receipt pointer identity is an API/transport issue to remove
deliberately, not a new logical authority requirement. The old global rule enum
may remain a temporary reader adapter, never the new accepted representation.

The main resume dependency is unchanged: reusable evaluator results, ordering
and escaped argument/spine pointers stay Performance-owned, routed through
Merge. Job owns the genuine Source/rule/query continuation and admission. Neither
lane may solve the other's constraints by dropping caches, charging replay as
useful continuation, executing Solve during load, or adding a second codec.

## Plan / current verification boundary

This is a proposal within the existing SE list, not a new accepted architecture
or a completed implementation. Change `typing.[ch]`, structural constructors in
`evidence.c` and the existing function/CBPV owners, then their real Synthesis/
typed-query/derivation consumers. Migrate one coherent structural path through
all consumers rather than adding an unused checked-owner API. Scope and map
migration must retain the exact typed inputs described above. Legacy reading
must use ordinary checking, with its transport selection contract explicit.

Verify positive Lambda/APP/CBPV and dependent scopes, invalid raw/copied/foreign
owners, Identity and IADT counterexamples, actual ordinary Sorted/Acc results,
inert load/resave/advance0, exact fuel/B/R and split frontier. Paired parent and
candidate runtime/input hashes and current-producer composition precede any
memory/time claim. Cost gates remain Merge/Performance scheduled.

Preserved private frontier review18: manifest `789a78c9`, accepted94a+six named
checkpoint modules, runtime126 `293d9d12`. Its local O2/SAN evidence covers full
Synthesis **unit** and affected Source/normalization **checkpoint** groups;
it did not execute full `tests/source_io.sh`/`source_io_test`. The Lambda fixture
change is an explicit support migration, not an unchanged assertion pass.
56 destroyed-owner cuts and three wrong-child/forged-query controls pass locally.
Actual compact public recipe remains1, parent/candidate all52 images/full TSV
equal: 1000+1000,1600+1600,completed1915+0 fail; original completed1921+0 evidence
remains unwaived. No joint/broad/cost/accepted adoption is inferred.

Separate typed-owner v2 removes three tiny private states using the existing
Job union slot (runtime126 `d192f8ce`, inputs `565cc07c`); O2 owner/frontier and
Source/normalization checkpoints pass. Full Synthesis aborts at the retained
old layout assertion853, raw `2692bef8`; SAN/public are unrun. Preserve this
trial and all exact inputs/failure evidence while the requested main authority
audit/refactor takes priority. E25/E26/E27 remain parked, not closed or waived.

Audit coverage is static: all53 rule dispositions and all stored receipt
components are classified. Independent implementation/qualification, full
frontier/strict3 resolution and exclusive matched peak/time are still pending.
