# Proof Binding-Object Identity V2-B1 Implementation Plan

Date: 2026-08-07

Status: complete

Parent plan:

- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Predecessors:

- `doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`
- `doc/2026-08-07T03-00-00-BINDING-OBJECT-DIRECT-REINDEX-V2-C2-PLAN.md`

Successors:

- V2-S1 typed HOTT goal records;
- V2-P1 tagged proof payloads and premise arena;
- V2-O1 type-directed observational action.

Baseline:

- branch: `main`;
- commit: `fbc3c99`;
- artifact format: `A_PROGRAM_ARTIFACT 62`;
- implementation boundary: `src/prototype/`.

## 1. Purpose

V2-C2 established direct binding-object graph action for ordinary substitution
and reindexing. TermDB variables refer to an opaque `binding_id`; ContextDB
entries store the same kind of handle; SubstitutionDB acts on those handles.

Judgement proofs still identify a binder assumption through
`assumption_index`, defined as a De Bruijn level in the conclusion Context. This
leaves proof validation dependent on lexical depth even though the graph and
context layers identify the binder directly.

V2-B1 removes this final positional binder authority before typed HOTT goals
are introduced.

## 2. Normative Decision

### 2.1 One binding identity

The authoritative identity of a binder-assumption conclusion is:

```text
relation.subject = VAR(binding_id)
```

The validator obtains `binding_id` from that `VAR`, checks that the exact
binding occurs in `relation.context_id`, obtains its Context entry, and checks
the declared classifier.

The proof must not independently store either:

- a De Bruijn index or level;
- a duplicate `assumption_binding_id` that can disagree with the conclusion.

When V2-P1 later separates relation conclusions from tagged proof payloads, the
binder-assumption rule payload therefore needs no binder identity field. Its
identity is part of the conclusion it justifies.

### 2.2 Context extension identity

Implementation disproved the initial proposal to intern `Gamma.A` by parent
Context and classifier alone. If two independently allocated bindings with the
same classifier were merged, the returned Context retained one binding while
TermDB variables and proof premises retained the other. Artifact append exposed
this directly: exact binder validation rejected the dangling premise.

The V2-B1 decision is therefore:

```text
Context extension identity = (parent, binding_id, classifier state)
```

Repeated requests for the same binding and classifier may intern. Distinct
binding objects never merge merely because their classifiers are convertible
or structurally equal. Avoiding this rule would require a whole-graph binding
rewrite during Context interning, which would restore the remap mechanism that
V2-C2 intentionally removed.

### 2.3 Artifact transition

Artifact v61 serializes `assumption_index` as an unlabelled numeric proof
field. V2-B1 must not give that slot a new binding-ID meaning. Reusing the same
slot for a different identity would make two incompatible v61 semantics
indistinguishable.

For V2-B1:

- the in-memory field remains temporarily for the fixed v61 record layout;
- it is renamed or documented as a reserved legacy field;
- all producers write `PROTOTYPE_INVALID_ID`;
- all validators require it to be invalid;
- binder identity is reconstructed only from the conclusion `VAR` and Context;
- V2-P1/v62 removes the slot physically.

An old artifact receives no authority from a positional value. It is valid only
if its conclusion and relocated Context graph establish the exact binding
assumption without using that value.

## 3. Current Implementation Inventory

### 3.1 Proof schema

`src/prototype/judgement.h` defines `prototype_judgement_proof` with:

```text
conclusion_context_id
conclusion_subject
conclusion_classifier
assumption_index
```

The comment explicitly defines `assumption_index` as a De Bruijn level. The
conclusion subject already points to the `VAR(binding_id)`, so the positional
field is redundant semantic authority.

### 3.2 Proof construction

`src/prototype/typing.c` assigns `context->depth - 1` in both binder-assumption
construction paths near the current lines 4611 and 4649. Proof copying near
line 10477 copies the positional value.

### 3.3 Proof validation

`validate_assumption_proof` in `src/prototype/typing.c` currently:

1. checks that the subject is a `VAR`;
2. walks backward through the Context;
3. selects an entry by `entry->depth == assumption_index + 1`;
4. compares the selected entry classifier with the conclusion classifier.

It does not currently require the selected Context entry's `binding_id` to
equal the binding referenced by the conclusion `VAR`.

`validate_proof_rule_parameters` separately requires `assumption_index` to be
present for binder-assumption proofs and absent for other rules.

### 3.4 Compiler-local lookup

The computation-fold continuation lookup in `src/prototype/ast.c` near the
current line 23583 checks `assumption_index == context->depth - 1`. This is a
second positional consumer and must instead compare the relation subject's
binding with the canonical binding stored by the required Context entry.

### 3.5 Artifact I/O and append

`src/prototype/ast.c` currently:

- writes the positional field near line 4588;
- reads it near line 8691;
- relocates Context IDs and TermDB IDs during append;
- does not relocate `assumption_index`, because it is a depth.

After V2-B1, the conclusion `VAR(binding_id)` is relocated as part of TermDB,
and the Context entry binding is relocated by `prototype_context_db_append_relocated`.
No proof-local binder relocation is required.

### 3.6 Historical documentation

`doc/2026-07-27T00-00-00-CATEGORICAL-CWF-DCBPV-MIGRATION.md` records the old
De Bruijn-level decision. It is historical and must not be edited to pretend
that decision was never made. V2-B1 and the V2 parent plan supersede it.

## 4. Target Invariants

V2-B1 is complete only when all of the following hold.

1. A binder-assumption relation has kind `HAS_TYPE`.
2. Its subject is exactly `VAR(binding_id)`.
3. Its conclusion Context contains exactly that `binding_id`.
4. The matching Context entry, not an entry selected by depth, supplies the
   declared classifier.
5. The Context classifier and conclusion classifier are kernel-convertible
   under the required normalization profile.
6. No proof constructor or validator derives binder identity from Context
   depth.
7. Context depth remains cached structural metadata only.
8. Artifact append relocates the conclusion term and Context binding to the
   same target binding identity.
9. A forged proof for a different same-typed binder is rejected.
10. Alpha-equivalent source spelling remains irrelevant.

## 5. Implementation Phases

### Phase B1.1: Add exact Context binding lookup

Add a ContextDB query with one authoritative behavior, for example:

```c
int prototype_context_find_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_entry_context_id
);
```

Requirements:

- walk immutable parent edges;
- compare exact `binding_id` handles;
- return the Context entry that introduced the binding;
- distinguish not-found from malformed Context;
- do not search by source symbol, depth, or classifier;
- use this helper in `prototype_context_contains_binding` to avoid two lookup
  algorithms.

Progress:

- [x] API declared in `src/prototype/context.h`.
- [x] implementation added in `src/prototype/context.c`.
- [x] `prototype_context_contains_binding` delegates to the exact lookup.
- [x] empty, nested, missing, and same-classifier/distinct-binding tests added.

### Phase B1.2: Remove positional proof production

Change binder-assumption proof construction so it no longer writes
`context->depth - 1`.

Requirements:

- validate that the relation subject is `VAR(binding_id)` before publishing;
- validate exact Context membership;
- initialize the legacy slot to `PROTOTYPE_INVALID_ID` for every proof kind;
- do not add `assumption_binding_id` as a duplicate proof field;
- keep Match-pattern assumptions and IH rule parameters unchanged.

Progress:

- [x] both binder-assumption creation paths updated.
- [x] proof reset/reuse path updated.
- [x] proof copy/commit path no longer copies positional identity.
- [x] no assignment of `context->depth - 1` remains in proof construction.

### Phase B1.3: Rewrite binder-assumption validation

Rewrite `validate_assumption_proof` around the conclusion binding edge.

Required algorithm:

```text
subject must be VAR(binding_id)
entry_context := find_binding(conclusion_context, binding_id)
entry := ContextDB[entry_context]
entry.classifier must be present or solved by the permitted classifier path
entry.classifier ~= relation.classifier by kernel conversion
```

Requirements:

- reject a subject binding absent from the conclusion Context;
- reject selection of another same-typed binder at the same or another depth;
- retain classifier formation checks presently performed after entry selection;
- require the legacy positional field to be invalid;
- do not turn successful conversion into an object equality witness.

Progress:

- [x] exact binding extraction implemented.
- [x] depth walk removed from assumption validation.
- [x] rule-parameter validator updated.
- [x] malformed/forged binding identity is rejected before classifier
  conversion can validate the assumption.

### Phase B1.4: Remove positional compiler consumers

Audit every `assumption_index` reference and replace semantic consumers.

The known non-I/O consumer is the computation-fold continuation lookup in
`src/prototype/ast.c`. It must identify the expected continuation binding from
the relevant canonical Context entry and compare it with the relation's
`VAR(binding_id)`.

Progress:

- [x] computation-fold lookup converted.
- [x] `rg "assumption_index" src/prototype` contains no implementation match;
  the field is named `reserved_legacy_assumption_level` only at the approved v61
  storage/read/write declarations approved by this plan.
- [x] no proof lookup compares against `context->depth`.

### Phase B1.5: Enforce Context binding-object identity

Audit callers of `prototype_context_extend` that allocate a candidate binding
before extension.

The audit found that a caller cannot safely obey the former classifier-only
canonicalization contract without rewriting every dependent TermDB and proof
edge. Context interning was therefore corrected to include `binding_id` in its
key. Source/AST provenance remains separate, but a candidate binding is never
discarded in favor of an unrelated same-typed binding.

Progress:

- [x] all `prototype_context_extend` callers classified.
- [x] fresh candidate users audited.
- [x] Context interning includes exact binding identity.
- [x] tests demonstrate that repeated identical extensions intern while
  same-parent/same-classifier extensions with distinct bindings remain distinct.

### Phase B1.6: Preserve artifact v61 safely

Keep the v61 proof record width stable until V2-P1, but remove positional
semantics.

Requirements:

- writer emits `PROTOTYPE_INVALID_ID` in the reserved slot;
- reader stores or discards the slot without using it as binder authority;
- validation requires exact conclusion/context binding identity;
- artifact slicing marks the conclusion term and Context as before;
- artifact append verifies that relocated conclusion `VAR` and Context entry
  still refer to the same relocated binding;
- no compatibility fallback reconstructs a binder by depth.

Progress:

- [x] writer updated.
- [x] reader updated.
- [x] artifact validation updated.
- [x] local write/read test passes.
- [x] provider/consumer append test passes.
- [x] tampered positional slot cannot redirect a valid conclusion.
- [x] physical slot removal recorded as a mandatory V2-P1/v62 task.

### Phase B1.7: Regression and acceptance testing

Required focused tests:

1. a one-binder Context assumption;
2. a nested Context assumption referring to the outer binder;
3. two distinct bindings with definitionally equal classifiers;
4. a forged proof whose depth would select a same-typed but different binder;
5. canonical Context extension returning an existing binder;
6. Context relocation with a nonzero binder offset;
7. artifact append preserving exact binder identity;
8. Lambda binder assumptions used by APP elimination;
9. Match case assumptions and IH proofs remain unaffected;
10. computation-fold continuation lookup uses exact binding identity.

Full regression:

- [x] clean prototype build passes.
- [x] all prototype test scripts pass.
- [x] examples 01-07 and 09 pass.
- [x] artifact flow and deterministic output pass.
- [x] `git diff --check` passes.
- [x] no generated parser/lexer file is edited manually.

## 6. Files Expected to Change During Implementation

Implementation changes remain under `src/prototype/`:

- `src/prototype/context.h`
- `src/prototype/context.c`
- `src/prototype/judgement.h`
- `src/prototype/typing.c`
- `src/prototype/ast.c`
- focused prototype check source and/or test scripts

Documentation changes:

- this progress plan;
- the V2 parent status table and completion evidence;
- the V2-P1 plan when the reserved v61 slot is physically removed.

No accepted implementation under `src/` or `include/` is in scope.

## 7. Explicit Non-Goals

V2-B1 does not:

- add object equality syntax or witness terms;
- add a general `JUDGEMENT_EQ`;
- implement V2-S1 typed HOTT goals;
- refactor all proof payloads into variants;
- remove duplicated relation/proof conclusions;
- change Match/IH ownership;
- add a BindingDB;
- add De Bruijn indices under another name;
- split Value and Computation TermDB syntax;
- change the runtime environment or evaluation strategy.

The relation/proof conclusion duplication and physical proof payload redesign
remain V2-P1 work. V2-B1 changes only binder identity semantics needed before
that larger migration.

## 8. Risks and Controls

### Risk 1: Context interning discards a distinct binding

Control: `binding_id` is part of Context extension identity. A focused
regression supplies a same-typed different binding and requires a distinct
Context, while an identical repeated extension must intern.

### Risk 2: Binding identity is duplicated in a new proof field

Control: use the conclusion `VAR(binding_id)` as the sole binder authority. Do
not introduce `assumption_binding_id`.

### Risk 3: Artifact v61 silently changes field meaning

Control: the slot becomes reserved and invalid; it is not reinterpreted.
Binding identity remains in already-relocated TermDB and Context records.

### Risk 4: Match-pattern assumptions are accidentally conflated

Control: Match-pattern assumptions retain constructor-owner/index/field rule
parameters. V2-B1 changes only `BINDER_ASSUMPTION` positional identity.

### Risk 5: Classifier equality is confused with binding identity

Control: first select the Context entry by exact binding handle, then compare
classifiers. Equal classifiers never select or merge assumption proofs.

## 9. Exit Criteria

V2-B1 is complete when:

1. no binder-assumption semantic path reads or writes a De Bruijn level;
2. exact `binding_id` membership determines the Context assumption;
3. canonical Context extension cannot leave a proof referring to a discarded
   candidate binder;
4. forged same-depth and same-classifier proofs are rejected;
5. local, relocated, and artifact-appended proofs validate by the same rule;
6. the v61 positional slot is reserved and has no semantic authority;
7. all focused and full regression tests pass;
8. the parent V2 plan records V2-B1 complete and makes V2-S1 the next task.

## 10. Progress Log

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-07 | Planning | complete | Code audit identified positional `assumption_index` consumers and selected conclusion `VAR(binding_id)` as sole authority. |
| 2026-08-07 | B1.1 exact Context lookup | complete | `prototype_context_find_binding` returns the exact introducing Context; focused Context tests pass. |
| 2026-08-07 | B1.2 proof production | complete | All proof constructors initialize the v61 slot to invalid; proof copy does not retain positional authority. |
| 2026-08-07 | B1.3 proof validation | complete | Binder assumptions select Context entries only by the conclusion `VAR(binding_id)` and then check classifier conversion. |
| 2026-08-07 | B1.4 compiler consumers | complete | Computation-fold lookup uses source-occurrence Context binding and exact introduction lookup; no proof lookup uses Context depth. |
| 2026-08-07 | B1.5 Context identity | complete | Classifier-only Context interning was rejected; distinct bindings now produce distinct Context objects. |
| 2026-08-07 | B1.6 artifact v61 transition | complete | Writer emits invalid, reader preserves width, validator rejects positional values, and append preserves relocated binding identity. |
| 2026-08-07 | B1.7 acceptance tests | complete | Clean build, all 14 prototype scripts, examples 01-07/09, artifact flow, and diff check pass. |

## 11. Implementation Findings

### 11.1 Typed occurrence binder versus erased Lambda binder

TermDB alpha interning can share one erased Lambda between typed source
occurrences whose binder objects differ. A binder proof for an occurrence must
therefore use the occurrence's Context binding, not the erased representative's
internal binder.

Lambda-introduction reification now supplies the source binder and source body
as premises. The rule validates that abstracting those premises has the same
core alpha shape as the conclusion Lambda. Exact identity is required for the
binder premise and Context membership; alpha shape is used only at the explicit
occurrence-to-erased-Lambda boundary.

Cross-artifact append can retain more than one alpha-equivalent representative,
so this check uses `prototype_term_core_shape_equal`, not numeric term-ID
equality.

### 11.2 Symbolic Context refinement

Computation-block lowering can create a Context whose classifier is initially
symbolic. Reification may add its exact binder-assumption proof after the last
ordinary solver pass. Context resolution now also reads an exact
same-Context/same-binding binder-assumption relation and runs once more before
commit. This is a monotone classifier refinement; it does not select a binding
by classifier.

### 11.3 Artifact result

Artifact format remains `A_PROGRAM_ARTIFACT 62`. The old numeric proof slot is
written as `4294967295`, parsed only to preserve record width, and required to
remain invalid. Physical removal remains mandatory in V2-P1/v62.
