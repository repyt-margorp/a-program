# Evidence and Typed Occurrence Authority Audit

Date: 2026-10-04
Status: substantive static Markdown audit delivered and reviewed; prototype implementation and dynamic qualification remain open.
Code baseline: Main `c205d507a8568bfdda355e247a78b4e67464f294`.
Relevant local edits: `src/evidence.c` (+16/-3), `src/evidence.h` (+5/-0)
at initial inspection; these are unowned work and must be preserved.
Related: [Job/Evidence owner](2026-10-03-JOB-EVIDENCE-GOAL.md),
[SE audit](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md),
[coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md).
This is the single work list for the focused audit, not a replacement for
unfinished SE implementation or the running performance promotion.

## Problem List

| ID | Problem | Owner | Status |
| --- | --- | --- | --- |
| EOA1 | Determine whether Evidence duplicates Term/typed construction and eliminate unjustified authority or storage | Existing Job/Evidence worker; Merge integrates | Static findings reviewed; refactor pending |

## EOA1. One Construction, No Duplicate Proof Tree

### Subjective (User)

2026-10-04 13:53 UTC, English paraphrase of the human format permission
reaffirmed by inquiry desk019ebfae: old formats may be dropped. This does not
authorize accepted runtime changes from the audit request.

2026-10-04 12:58 UTC, English translation of the human request to the inquiry
desk: "I suspect Evidence rebuilds the same tree as Term, which seems pointless.
Have a worker investigate whether it can be reduced or integrated into typed
Occurrence, and summarize this in Markdown."

Earlier explicit user principles remain: proof structure should live in Term
and its typed construction through Curry-Howard, Core stays Lambda/Application/
Ref, Oracle-specific structures must not be expanded into another global union
or enumeration, and resume does not justify a duplicate program/proof graph.
These are requirements, not evidence that every existing receipt is redundant.

### Objective (Code)

Fresh initial inspection, not a complete audit or a new test run:

- `src/evidence.h:9`: the central `pg_evidence_rule` enumeration still includes
  Lambda/App, CBPV, Identity, IADT, host, Context and other rule families.
- `src/evidence.c:14`: `pg_evidence` retains a rule, conclusion reference,
  certificate, retained-input count and trailing Evidence references. Its
  conclusion union points to Occurrence, Context or Context map; it does not
  itself contain all Term payloads.
- `src/evidence.c:35`: sparse `receipt_selection` records preserve selected
  nondefault premise proofs. `borrowed_premise_count`, `borrowed_premise` and
  `pg_evidence_premise` reconstruct other logical inputs from typed owners.
  Logical premise count is therefore not the number of physically stored edges.
- `src/typing.h`: Occurrence already contains Core, classifier, type formation,
  operands, origin/map and admission links. Context and Context map also carry
  receipt links. Comments explicitly distinguish descriptive construction from
  checked acceptance; whether the current physical split is necessary remains
  the question, not an assumption to preserve it forever.
- The initial layout matches the committed baseline for these fields. Relevant
  uncommitted edits are separately noted above; worker findings must pin their
  own producer revision and distinguish accepted code from private trials.

2026-10-04, fresh Root static review: the [worker report](../src/prototype/solver_inputs/epochs/evidence_occurrence_audit_20261004/report.md)
and [coverage review](../src/prototype/coordination/reviews/20261004-evidence-occurrence-authority-audit-review.json)
verify all34 committed source pins at `c205d507`, also equal accepted
`fc52755b`, and all53 rules exactly once. Main dirty edits are excluded.
Evidence points to typed conclusions instead of copying every Term payload,
but separate headers/index/admission chains and repeated traversal remain,
even for zero-premise Lambda/APP. The report traces field/edge dispositions
and concrete invalid scope, Identity, IADT, substitution and conversion cases.
LP64 header64/alternate16 bytes is a static layout calculation, not peak RSS.

### Assessment

Inquiry-desk proposal, not a completed worker finding: audit the Evidence layer
itself, not only copied premise arrays. A shared DAG may still duplicate typed
construction even when it is interned and called a receipt. Conversely, a proof
selection, independently checked conversion, scope fact or suspended checking
cursor is not automatically recoverable from untyped Term shape.

Evaluate direct checking of typed Occurrence, checked-owner admission and
Oracle-local residual data against the current implementation. Do not create a
new wrapper graph, merely rename Evidence, merge distinct logical rules solely
to shorten an enum, or trust a raw typed-looking tuple as a checked theorem.
If retaining a datum is necessary, show the actual counterexample without it.
If existing provenance preservation is only accidental API behavior, identify
that separately rather than silently weakening checks or declaring it essential.

2026-10-04, Root adopts the agent proposal within the existing prototype scope:
coherently migrate structural checking into canonical Occurrence/Scope/map
constructors and their real Synthesis, typed-query and derivation consumers.
Delete receipt/index/chain allocations; preserve preconstruction sharing and
foreign/copied/raw-owner rejection. Do not embed the old Evidence header or
add a generic checked wrapper. Preserve selected Scope formation, checked
conversion/reduction endpoints, IADT positivity/nominal signatures and real
continuation on their existing owners. Constructor/type-case maps must migrate
with their checks. This is not user acceptance of a new runtime architecture.

Inquiry-desk review clarifies legacy Identity/substitution history selectors
and image/pointer/provenance assertions are migration debt, not a perpetual
compatibility requirement. The human permission in Subjective allows deliberate
retirement of old formats. They must not force preservation of a duplicate
receipt graph. Identify retired formats/assertions, retain historical evidence
and verify actual typing/frontier semantics; changed contracts are not an
unchanged historical pass or a blanket waiver of current strict failures.

The requested static audit checkpoint is complete. Implementation and current
composition, positive/negative/fuel/inert-load/split-resume qualification and
matched peak/time remain in the existing SE work list. No net memory/time gain
is inferred from static sizes. Original strict3 and typed-owner v2 Synthesis134
remain; SAN/public are unrun for that trial.

### Plan

- [x] Record the human concern and initial committed/local structural baseline.
- [x] Existing Job/Evidence worker traces constructors, interning, consumers and
  persistence/resume paths; include each rule family, not only Lambda/App.
- [x] Produce a compact field/edge table: typed owner counterpart, physical
  duplication, independent information, readers, lifetime and proposed owner.
- [x] Trace small concrete Lambda/App, Context/substitution and List/Acc cases;
  distinguish object-language witness Terms from checking receipts and frontier.
- [x] Assess removing the separate Evidence DAG, integrating checked facts into
  Occurrence/Context/map, and localizing irreducible Oracle data. Explain adopted,
  rejected and deferred choices with code references and counterexamples.
- [x] Propose implementable deletion epochs with existing positive/negative,
  fuel, inert-load, split-resume and scope tests. Keep inherited failures explicit;
  do not rewrite assertions only to make a candidate pass.
- [x] Report likely removed structures and per-file code changes; separate
  physical counts, `.a` size and actual measured time/peak RSS. Reuse existing
  tools and coordinate heavy measurements with Performance/Merge.
- [x] Submit the completed audit and concise findings to Merge and the inquiry
  desk. Any implementation remains prototype work until separately accepted.
- Completion: each stored Evidence component has an evidenced delete/integrate/
  retain disposition, with a concrete next refactor and verification scope.

## Progress

| Date | Problem | Result | Next Step |
| --- | --- | --- | --- |
| 2026-10-04 12:58 UTC | EOA1 | User request recorded; initial structure inspected, no new tests run | Route to the existing Job/Evidence worker and obtain acknowledgment |
| 2026-10-04 13:29 UTC | EOA1 | Original Job/Evidence worker actually consumed the request, recorded the human paraphrase in both active Subjective sections, and made this audit primary at the safe boundary; [exact receipt](../src/prototype/coordination/reviews/20261004-job-authority-audit-actual-receipt.json) | First substantive field/edge table and concrete Lambda/App/Context counterexamples around 14:00 UTC, a Root soft checkpoint, not a human deadline |
| 2026-10-04 13:51 UTC | EOA1 | Substantive report45109a85/all34 pins/53 dispositions reviewed; static audit checkpoint complete, actual refactor unimplemented | Continue coherent structural checked-owner prototype path in the existing SE list; retire format debt explicitly, retain semantic checks |

The preceding typed-owner v2 runtime126 `d192f8ce` is a separate prototype
trial: owner O2 focused checks report pass, full Synthesis abort134 retains
`tests/synthesis.c:853`, SAN/public are unrun. That trial does not establish the static audit conclusion
or accepted deletion. No owner, Goal, model, Git reference or index changed.
The 13:36 cost-drain request expired without a grant; light audit continues.
