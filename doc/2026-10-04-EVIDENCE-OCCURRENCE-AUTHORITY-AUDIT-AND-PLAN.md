# Evidence and Typed Occurrence Authority Audit

Date: 2026-10-04
Status: requested; initial structure inspection complete, worker audit pending.
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
| EOA1 | Determine whether Evidence duplicates Term/typed construction and eliminate unjustified authority or storage | Existing Job/Evidence worker; Merge integrates | Audit requested |

## EOA1. One Construction, No Duplicate Proof Tree

### Subjective (User)

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

### Plan

- [x] Record the human concern and initial committed/local structural baseline.
- [ ] Existing Job/Evidence worker traces constructors, interning, consumers and
  persistence/resume paths; include each rule family, not only Lambda/App.
- [ ] Produce a compact field/edge table: typed owner counterpart, physical
  duplication, independent information, readers, lifetime and proposed owner.
- [ ] Trace small concrete Lambda/App, Context/substitution and List/Acc cases;
  distinguish object-language witness Terms from checking receipts and frontier.
- [ ] Assess removing the separate Evidence DAG, integrating checked facts into
  Occurrence/Context/map, and localizing irreducible Oracle data. Explain adopted,
  rejected and deferred choices with code references and counterexamples.
- [ ] Propose implementable deletion epochs with existing positive/negative,
  fuel, inert-load, split-resume and scope tests. Keep inherited failures explicit;
  do not rewrite assertions only to make a candidate pass.
- [ ] Report likely removed structures and per-file code changes; separate
  physical counts, `.a` size and actual measured time/peak RSS. Reuse existing
  tools and coordinate heavy measurements with Performance/Merge.
- [ ] Submit the completed audit and concise findings to Merge and the inquiry
  desk. Any implementation remains prototype work until separately accepted.
- Completion: each stored Evidence component has an evidenced delete/integrate/
  retain disposition, with a concrete next refactor and verification scope.

## Progress

| Date | Problem | Result | Next Step |
| --- | --- | --- | --- |
| 2026-10-04 12:58 UTC | EOA1 | User request recorded; initial structure inspected, no new tests run | Route to the existing Job/Evidence worker and obtain acknowledgment |
