# Incremental Solver: Duplication and Ownership Audit

Date: 2026-09-18. Baseline: `3a3bf550`, plus the uncommitted typed-data
refactor and issue #29 fixes. This is an audit and next-step plan, not a claim
that the cleanup or the parent acceptance gates are complete.

Related: [active authority plan](2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md),
[issue #29 findings](2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md).

## Conclusion

The principal duplication is **reconstructing pending term/type structure from
solver recipes**, not two independent App/Lambda evaluators. Remove reconstruction
paths and repeated orchestration, not logical checking rules. Core computation
must remain separate from typed occurrences and their checked derivations.

This review follows the active `src/prototype/pointer/Makefile` dependency graph.
It does not treat the archived integer-ID implementation as a second active
authority. Static inspection is not a proof that every execution path is covered.

## What Was Checked

| Area | Files / main paths | Finding |
|---|---|---|
| Core and evaluation | `graph`, `eval`, `conversion`, `computation`, `execution` | Only Lambda/Application/Reference Core tags. Pure normalization shares work; actual external effects are not memoized results. |
| Typed structure | `typing`, `evidence`, `derivation` | Occurrence, context and map interning already exists. Checking and describing a structure are different responsibilities. |
| Incremental synthesis | `synthesis`, `effect_inference`, `program` | Pending structural projections duplicate construction logic; accepted classifier queries already use retained typed data. |
| IADT and graph properties | `iadt`, `function_graph`, source Match/graph paths | Graph formation and witness generation share one graph state. Source call-layout discovery still walks syntax separately. |
| Identity | `action`, `identity`, `symmetry`, typed boundary access | Core actions compute terms; checked family actions establish their types. Similar inputs alone do not make these duplicate acceptance authorities. |
| Persistence | source, occurrence, derivation, context/declaration, retained and reduction I/O | Match allocations are now descriptive; declaration allocation still depends on a saved derivation producer. Separate codecs are not automatically separate semantics. |
| Regression coverage | acceptance fixtures, `program_test`, source/image runners | Both accepted and rejected resumed inputs matter. A passing suite formerly included an expected graph-collision failure. |

## Confirmed Cleanup Targets

### 1. Pending structural reconstruction

`synthesis.c:7147` (`prepared_source_rule`) and the `term_structure_step`,
`declared_type_step`, `classifier_structure_step`, and `type_structure_step`
paths inspect job roles, source stages and derivation rules to rebuild term and
classifier spines. `evidence.c` later constructs the checked versions. Hash
interning can share the resulting Core nodes, but does not remove the duplicate
dispatch and preparation logic.

The reason for early structure is real: handler effect equations need the shape
of a computation before its acceptance is complete. Replacing these paths with
"wait until the producer is DONE" can introduce cyclic waits.

Required direction: let source elaboration retain its descriptive construction
once. Derive structural queries from that data and share pure spine-building
functions with checked rules. Start with Return/Thunk/Force/Pi/App, remove their
job-role reconstruction branches as each replacement lands. Do not create a
parallel trusted classifier table or treat the description as a proof.

`::` remains a post-check. In particular, the current structural projection of
an EXPECT job reads its target (`classifier_structure_step`); migration must
trace its consumers and preserve the non-feedback tests, not accidentally turn
this descriptive boundary into an inference oracle. No unsound acceptance from
this path has been demonstrated by this audit.

### 2. Declaration allocation through saved evidence

`source_io.c:367`, `synthesis.c:911` and `synthesis.c:4208` still retain/import
an `allocation_origin` producer for declarations. Source checking waits for it
to recover the declaration schema, then checks the source declaration. This is
a real remaining source-to-proof-to-allocation dependency.

Finish the existing raw-allocation migration: retain the nominal declaration
and field/index binder allocation as descriptive inputs, recheck their types
through ordinary source synthesis. Delete this allocation-only derivation edge.
Do not delete independently selected theorem roots or silently trust loaded
field types. The analogous Match allocation change is already in this worktree.

### 3. Repeated scope-array assembly

`evidence.c:4466` (`pg_prove_substitution_compose`) walks all images and makes
a temporary proof array on each call. Individual occurrence actions are shared,
but assembling and checking the aggregate still happens before the final
substitution record can be reused. `typing.c:351` (`pg_context_map`) also walks
the telescope/images to find an already interned map.

First measure repeated identical compositions on imported QuickSort and indexed
transport. If material, share the composition construction at the existing
typed-map operation boundary, keeping proof premises distinct. Do not introduce
an unchecked `(map pair) -> accepted proof` cache or replace alternate derivations.
This is repeated traversal, not evidence of competing mutable type solutions.

### 4. Nested synchronous work

Several `evidence.c` wrappers drain existing resumable work with `1024` or
`UINT64_MAX` loops (for example `pg_prove_classifier` at 5235 and substitution
composition/lifting). Thus a single outer Solve step need not be a small amount
of work. This concerns budget granularity, not necessarily recomputation.

Incremental callers should depend on the existing request/advance/result
interfaces. Keep synchronous entry points as thin wrappers where needed; do not
add another scheduler or duplicate the checked rule implementations. Count inner
steps as well as outer steps before reporting a performance improvement.

### 5. Source call-layout rediscovery

`synthesis.c:3197` (`function_graph_order`) walks source markers and block groups;
`function_graph.c` separately discovers typed recursive/helper calls. They
currently answer different questions: source field order/names versus checked
call dependencies. Repeated calls of one IH in the same group remain restricted.

Retaining call-site provenance during elaboration could remove the extra source
walk. It must distinguish two uses of the same binder, shadowing, and block
cutoffs. Do not identify a call site merely by its erased Core or IH field.
This is a design candidate, not a proven drop-in deletion.

## Unifications Not Justified

- `pg_synthesis_job` is pending work, not an independent proof authority.
  `EVIDENCE_JOB` wraps an already accepted proof; it does not prove it again.
- `pg_occurrence` describes a typed use; `pg_evidence` records checked premises.
  Sharing erased `lambda x. x` does not identify its Bool and Nat typed uses.
- `pg_classifier_request` already keys on the exact typed subject and uses its
  retained type or scoped structural action. Do not describe it as fresh source
  type synthesis on every request.
- Typed input queries and structural occurrence actions already share requests.
  An additional checked boundary is not by itself redundant computation.
- Effect equation values have a solver owner; immutable closed effect rows are
  outputs. No competing effect-solution authority was established here.
- Conversion's WHNF-to-NF fallback uses the same normalization work store.
  Structural interning must not absorb conversion or observational equality.
- Match, App, IH and Fold prove different rules. Encoding them through Lambda
  is not a reason to erase those proof obligations.

## `@f` and `*f`: Fewer Spellings, Not Fewer Objects

`function_graph_request`, `function_graph_step`, `function_witness_step` and
`graph_reference_step` (`synthesis.c:3331-3440`) share the same graph owner:

```text
@f : input -> output -> Type              // relation family G_f
*f : input -> computation of (y, G_f x y) // generated result/witness packet
```

These are schematic, not literal current surface types. Totality does not make
the relation family and a term producing its witness interchangeable. Graph
elimination needs the former; connecting a property to a computed output needs
the latter. There is no second graph-extraction engine to delete here.

Surface proposal for discussion: keep `@f` and expose the existing generated
witness as a named member, e.g. `(@f).witness`, instead of global `*f`. This
changes elaboration/export naming only and must reserve or disambiguate that
member against graph constructor aliases. It is **not implemented or approved**.
Keep IH `*k` and recursive-type Self `*` unchanged.

Making ordinary `y := f x` automatically supply `@y` is a different, larger
proposal: preserve the particular call's provenance and pair its value with
its proof without executing effects twice. Do not infer that provenance merely
from the value or rename `*f` to `@f` while discarding one of the two types.

## Execution Order and Gates

Priority update: the [IADT / issue 29 / PR 30 plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md)
now precedes the unchecked broad refactoring items below. Keep the findings and
existing changes, but do not let this cleanup displace those two user priorities.

- [x] Reproduce and fix the two isolated #29 limitations; preserve remaining
  proof limitations in the linked report.
- [x] Audit the active Core/typed/Solver/persistence paths and identify existing
  sharing as well as actual reconstruction.
- [ ] Finish declaration raw allocation (target 2, existing parent A1-A4 work).
- [ ] Consolidate pending construction (target 1), deleting replaced paths in
  the same change. No permanent old/new dual authority.
- [ ] Measure targets 3-5; adopt only changes with demonstrated simplification
  or bounded-work benefit. Record rejected proposals and why.
- [ ] Run pending/resumed handler effects, synthetic post-check, nominal identity,
  alternate derivation, indexed transport and imported QuickSort regressions.
- [ ] Report implementation/header, test and documentation LOC separately;
  compare allocations and work, not just wall time or outer Solve steps.

Current implementation/header delta is **+891/-580, net +311** from R76, and
**+5883/-3156, net +2727** from `4657cc6`. These include the preceding worktree
refactor, not just #29. Tests/docs/build files are excluded. The parent
net-negative gate is not met. This audit does not declare the refactor complete
or authorize publication as a finished cleanup.
