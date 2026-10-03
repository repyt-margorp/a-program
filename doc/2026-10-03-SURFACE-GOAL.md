# Surface Binder Syntax Goal

Date: 2026-10-03
Status: ready for launch; session `surface`, branch `parallel/surface-20261003`.
Baseline: committed `eb0aad6` plus coordination documents and documentation PR #58.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md), #57.

## Problem List

1. Correct Function Graph named Binder polarity and migrate `.p` consumers
   without duplicating typed semantics or changing proof acceptance.

## 1. Named Binder Migration

### Subjective (User)

2026-10-03, English paraphrase: add a task-named surface session for function/
function-graph Binder notation and `.p` changes. Use `/goal`, `6.1 Sol`, `xhigh`
and separate work directories; launch once incoming issues/PRs clarify scope.
The supplied #57 / PR #58 audit proposes `local := source`, double-brace named
sets and ordered single-brace lists. Publication alone is not approval of every
grammar decision. Preserve synthesis-first typing and post-synthesis-only `::`.

### Objective (Code)

#57 / PR #58 reproduce current `source := local` orientation on accepted and
overlay builds at `eb0aad6`. Parser, `mark_index_names`, `case_field_scope` and
`syntax_io` all consume that orientation. Single-brace source order is ignored;
double-brace named lists are not parsed. Existing rejection controls still pass:
this is a surface inconsistency, not an established kernel-unsoundness bug.

### Assessment

Coordinator assignment: critically verify the audit, prototype consistent
`local := source` binding and migrate the active `.p` cases. Reuse the existing
complete canonical telescope and hidden fields; no second Context/graph/solver.
Prototype the documented brace alternatives with explicit agent decisions;
do not silently treat ordering/compatibility choices as user-approved policy.
Report those decisions before accepted promotion. Do not guess orientation from
name availability. Internal function witnesses remain separate; IH `*arg` remains.

### Plan

- [ ] Read AGENTS.md, CODING_STYLE.md, coordination and all three PR #58 documents.
- [ ] Inspect current parser, scope scan, elaboration, graph metadata and syntax I/O.
- [ ] Record exact proposed grammar and migration/compatibility decisions in SOAP.
- [ ] Assemble a private clean overlay using `ARTIFACT_SOURCE="$PWD/src"`.
- [ ] Implement the source-layer correction in `src/prototype/surface/`; keep
  patches and migrated `.p` fixtures there, not accepted parser/tests/examples.
- [ ] Coordinate parser/AST, scope scanner, `case_field_scope` and persisted
  syntax together. Do not reinterpret old saved syntax or add name heuristics.
- [ ] Verify distinct renaming, duplicate/unknown/both-valid selectors, local
  shadowing, omitted dependencies, canonical telescope and `@local`/`*local` roles.
- [ ] Test unordered permutations and any proposed ordered-mode reversals;
  preserve positional patterns and ordinary block/definition-brace parsing.
- [ ] Verify assertion-free synthesis, wrong-proof rejection and `.p`/image
  round-trips, then relevant combined regression and scope/sanitizer gates.
- [ ] Report per-file code/test/example/doc deltas and deliver verified epochs.
- Completion: #57's correction and migration gates pass under an explicit
  reviewed syntax policy; preserved canonical lowering and proof semantics
  are verified, not inferred from one closed result.

## Work Contract

- Write only `src/prototype/surface/`, this plan and lane-local documents.
  Parser/source-layer and `case_field_scope` changes are handed off as prototype
  patches; no accepted edits or modifications to shared solver_inputs patches.
- The coordinator owns Job/Evidence/query/admission/frontier and graph-generation
  semantics. Report needs crossing that boundary; do not create a private checker.
- Do not merge PRs, close issues, push Main, promote code or substitute model.
  Branch-only push is allowed after focused verification.
- Heavy regression needs the coordinator's machine slot; focused builds use
  `-j2` at most. Detach symlinks before edits; keep generated output untracked.
- Keep this SOAP work list concise and in place. Report actual tests, next action,
  cross-owner conflicts and unresolved syntax decisions before ending each turn.
