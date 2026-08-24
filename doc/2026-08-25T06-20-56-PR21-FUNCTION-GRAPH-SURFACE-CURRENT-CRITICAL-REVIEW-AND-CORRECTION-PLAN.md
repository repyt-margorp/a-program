# PR21 Function Graph Surface: Current Critical Review and Correction Plan

Date: 2026-08-25 JST

Status: critical review complete; implementation not started

Repository revision: `be63360e72f7e324b7b263cb4fefbe6941e6c947`

Artifact format: v85

Reviewed proposal:

- `2026-08-24T19-13-51-FUNCTION-GRAPH-SOURCE-ORIGIN-SURFACE-AND-INSPECTION-PLAN.md`

## 1. Objective

PR #21 has the correct high-level goal: expose already generated function
graphs without requiring users to know `$graph.*`, `$result.*`, synthetic
field names, a positional 17-field QuickSort constructor, or the generated
`@returned` label.

The correction must preserve these A Program boundaries:

1. generated graphs remain ordinary IADTs;
2. named graph patterns remain elaboration into ordinary Match;
3. certified execution remains one computation producing a value and evidence
   for that exact value;
4. `:graph` remains static inspection rather than an object-language term;
5. `::` remains post-synthesis checking;
6. source names and sigils select elaboration roles, not new Core Term tags;
7. no local source Binder becomes a GlobalName; and
8. imported selector metadata cannot become an unchecked semantic authority.

## 2. Review Method

The proposal was checked against:

- `src/prototype/src/frontend/reader.c`;
- `src/prototype/include/a_program/frontend/ast.h`;
- `src/prototype/src/frontend/function_graph.c`;
- `src/prototype/src/frontend/lowering/function_graph_generation.inc`;
- `src/prototype/include/a_program/graph/compile_metadata.h`;
- `src/prototype/include/a_program/artifact/interface.h`;
- `src/prototype/src/artifact/wire_v85.c`;
- `src/prototype/spec/artifact_v85.schema`; and
- `test_function_graph_certified_execution.sh`.

The focused function-graph integration test passed every current phase. A
separate current-code probe reproduced the proposal's block-binding failure:

```sh
sh src/prototype/tests/integration/test_function_graph_certified_execution.sh
```

```a-program
@cons head tail => {
	tailLength := *tail;
	Nat.succ tailLength;
}
```

Requesting `@length` still fails with:

```text
function graph terminal IH collection failed body=12 tag=18
```

Thus the central addressability gap remains current at `be63360`.

## 3. Executive Verdict

| Proposal | Verdict | Required correction |
| --- | --- | --- |
| Generated graph remains an ordinary IADT | ACCEPT | No new kernel graph-elimination rule |
| `:graph f` is static inspection | ACCEPT | First implementation must respect compile staging |
| Owner-relative local selector metadata | ACCEPT | Key it by accepted owner/constructor/field identities |
| Source call bindings provide stable selectors | ACCEPT | Store one source-origin group, not one global name |
| `r`, `@r`, and `*r` are one Binding identity | REJECT | They are distinct Bindings sharing one origin-group ID |
| Named graph cases use braces | ACCEPT WITH CHANGE | One selected origin should expose its available roles |
| `*f args @ output graph => body` | SIMPLIFY | Use one source binder: `*f args @ result => body` |
| `@returned` remains public | REJECT | Keep it internal; direct elimination lowers to it |
| Positional graph Match remains available | ACCEPT | It is ordinary IADT Match, not a compatibility implementation |
| Higher-order `h`/`@h`/`*h` companions | DEFER | Define role-group and CBPV classifiers first |
| Artifact source selectors | ACCEPT | Requires v86, relocation, validation, and deterministic bytes |
| Branch path is selector authority | REJECT | Constructor/field identity is authority; paths are diagnostic |
| Imported selectors can be reconstructed from Core | REJECT | Read the exported validated interface only |

The proposal is therefore not discarded. Its surface goal is retained while
its Binding model, direct-elimination syntax, phase order, and persistent
authority are corrected.

## 4. Current Implementation Facts

### 4.1 Existing and sound

- Type position `@f` has a dedicated
  `PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE`.
- Global term position `*f` has
  `PROTOTYPE_AST_FUNCTION_GRAPH_WITNESS_REFERENCE` and requests both the graph
  family and certified execution.
- Local `*x` resolves to an ordinary Match induction hypothesis only when the
  parser's Binder has `induction_allowed`.
- Function-graph requests are owned by source assignment and source-entry IDs;
  equal erased Core Terms do not merge them.
- Generated graph constructors are ordinary TypeDeclaration/IADT
  constructors.
- Artifact v85 stores and validates owner, graph family, result family,
  interface, adapter, runner, and certified callback argument association.
- QuickSort's generated graph and its 17-field constructor pass source,
  publication, readback, link, and corruption tests.

### 4.2 Missing

- There is no `:graph` REPL/driver command.
- Term position `@localName` does not exist.
- Match cases accept positional identifier binders only.
- No persistent source-origin or selector-group DB exists.
- Artifact v85 has no selector-group section.
- Block-bound recursive results are not recognized by terminal IH collection.
- Generated recursive/helper fields still receive synthetic symbols.
- Higher-order callback graph and certified companions remain separate explicit
  parameters such as `LeGraph`.

### 4.3 Naming correction

`PROTOTYPE_AST_FUNCTION_GRAPH_WITNESS_REFERENCE` is misleading. Global `*f`
does not denote the graph witness for one execution; it denotes the certified
computation which returns a dependent package. Before adding more surface
forms, rename it conceptually and in code to a certified-function reference.
This is an AST/lowering rename, not a Core or Artifact semantic change.

## 5. Binding and Origin Model

### 5.1 Why one Binding ID is wrong

For a recursive call result `r`, the three roles have different classifiers:

```text
r  : B
@r : @f arguments r
*r : Motive arguments r @r
```

They can also coexist in one Context. A Binding ID identifies one Context
extension and therefore cannot identify all three. Reusing it would make
substitution, usage, and future resource constraints unsound.

The correct structure is:

```text
source_origin_group_id
  value_binding_id
  graph_binding_id
  ih_binding_id        // created only by recursive elimination
```

Each role has a distinct AST Binding and later Context Binding. The common
origin-group ID records that the roles came from the same accepted source call.
Surface spelling is presentation; Binding and field IDs remain authority.

### 5.2 Proposed compiler-local records

```c
struct prototype_function_graph_origin_group {
	uint32_t owner_association_id;
	uint32_t constructor_id;
	uint32_t source_binding_id;
	int display_symbol_id;
	uint32_t role_mask;
	uint32_t value_field_ordinal;
	uint32_t graph_field_ordinal;
	int recursive;
};
```

The exact layout may differ, but these invariants are required:

- one immutable record is frozen only after the graph constructor is accepted;
- field ordinals reference the complete constructor telescope;
- an unavailable role uses an invalid ordinal rather than a fabricated Term;
- `*r` is derived from the recursive graph-field IH and is never serialized as
  a constructor field;
- source AST/Binder IDs remain compiler-local; and
- branch paths and display strings are diagnostics, not identity.

This storage should be a typed function-graph interface DB, not another mutable
table embedded ad hoc in the 8,321-line generator.

## 6. Corrected Surface Design

### 6.1 Named graph cases

Retain the braced case form, but select an origin group once:

```a-program
graph
	@cons {
		pivot;
		lowerResult;
		upperResult;
	} => combine *lowerResult *upperResult;
```

Selecting `lowerResult` introduces every available local role for that origin:

```text
lowerResult   returned value
@lowerResult  paired graph witness
*lowerResult  IH, only when the graph field is recursive
```

This is simpler and less fragile than requiring both `lowerResult;` and
`@lowerResult;`. An alias renames the whole role group:

```a-program
lowerResult := lower;
```

and exposes `lower`, `@lower`, and, when valid, `*lower`.

For an ordinary pattern field with no graph origin, only the unsigiled value is
available. `@field` and `*field` are rejected with role-specific diagnostics.

### 6.2 Omitted fields

Omission is surface-only. The elaborator must create the complete constructor
telescope in field order, including hidden Binding IDs for omitted fields.
This is necessary because later field classifiers, result indices, and IHs may
depend on an omitted earlier field.

Therefore named cases do not lower to a shorter positional Match. They lower to
a full positional Match plus a user-visible alias map. Hidden fields participate
in Context, Substitution, and usage evidence. This is also the correct boundary
for future resource-sensitive weakening.

### 6.3 Role-qualified local references

`@name` in term position needs a surface AST role-reference node. The parser
cannot immediately emit a VAR because named graph selectors are resolved only
after synthesizing the scrutinee's graph-family classifier.

The role reference contains an explicit sigil and source spelling. Elaboration
resolves it through the active case's origin-group map and then emits an
ordinary VAR/IH occurrence. No new Core Term tag is introduced, and no expected
type is used to guess the role.

### 6.4 Direct certified elimination

The PR form still exposes two names for one logical execution result:

```a-program
*f arguments @ output graph => body
```

Use one origin binder instead:

```a-program
*f arguments @ result => body
```

Inside `body`:

```text
result   is the output value
@result  is the graph witness indexed by that value
*result  is unavailable because this is an actual run, not a recursive IH
```

It elaborates to the existing package exactly once:

```a-program
{
	hidden := *f arguments;
	hidden @returned result hiddenGraph => body;
}
```

with `result` and `hiddenGraph` registered as distinct Bindings in one origin
group. The generated `returned` label remains internal.

The `@` delimiter is syntactically feasible because it does not start an
ordinary term application atom. The parser must nevertheless create a
dedicated certified-elimination AST node rather than infer the construct from
the eventual classifier. Arguments containing elimination suffixes require
parentheses, as they already do in ambiguous nested Match cases.

Standalone `*f arguments` remains valid. Removing first-class certified
packages is unnecessary for surface simplification and would reduce
expressiveness.

## 7. `:graph` Inspection Boundary

The command remains outside the object language and does not reserve `graph`
as a source identifier.

The PR's proposed local behavior needs staging correction. A REPL command runs
after normal compilation, while a new graph request currently must be collected
before generated-closure compilation. The first implementation must not mutate
an already accepted program by appending a late graph request.

Implement two modes:

1. `--show-function-graph NAME` records an inspection request before compiling
   source and can therefore generate a missing local graph in the normal phase;
2. REPL `:graph NAME` initially inspects an already accepted association and
   reports that a non-generated graph must be requested in a new compilation
   session.

A later scratch-session recompile may make REPL inspection request generation,
but it must use independent program storage and must not mutate the current
accepted Claims.

The display reads the frozen function-graph interface DB. It does not walk all
Terms or reverse engineer synthetic symbols. Base display shows accepted raw
classifiers; optional normalization is a separate mode.

Owner lookup must ultimately use a qualified owner identity. Current
compiler-local lookup is primarily `owner_symbol_id`, while Artifact exports
already carry namespace identity. Selector work must not deepen this mismatch.

## 8. Artifact v86 Boundary

Artifact v85 cannot support stable imported source selectors. The first wire
change for this feature is v86.

The wire section should be relative to existing validated graph associations:

```text
function_graph_selector_groups COUNT
function_graph_selector_group ID ASSOCIATION CONSTRUCTOR_ORDINAL
	DISPLAY_SYMBOL ROLE_MASK VALUE_FIELD GRAPH_FIELD RECURSIVE
```

Exact spelling is provisional. Required validation is not:

> the stored name appears plausible.

It is:

1. association and constructor identities are in range;
2. field ordinals are in the complete constructor telescope;
3. role masks agree with present/absent ordinals;
4. value and graph fields belong to the same generated call group;
5. a recursive marker names a graph field that receives an ordinary IH;
6. selector groups are unique within one owner/case; and
7. publication, relocation, append, link, and readback preserve exact bytes.

For an imported module, the selector table is authoritative for source-name
elaboration in the same way an export name is authoritative. It is not proof
authority: the resulting full Match is still checked normally. Two same-typed
fields cannot be distinguished from provider source after compilation, so the
reader cannot recreate provider intent from classifiers alone.

Consequently the selector table must be included in deterministic interface
identity and corruption tests. Structural type checking alone is insufficient
to detect an accidental swap between same-typed fields.

## 9. Higher-Order Companion Roles

The `h`/`@h`/`*h` direction is useful but should not be implemented in the first
surface package.

It requires three separate Context Bindings linked by one role-group ID:

```text
h   raw callback value
@h  callback graph-family interface
*h  certified callback value/computation interface
```

The classifier of `*h` may be derived without expected-type search only after
the supported pure-total callback spine and CBPV polarity are explicit. The
surface `\*h =>` cannot be accepted merely because generation currently uses a
parameter printed as `LeGraph`.

First complete first-order origin groups, named cases, direct elimination, and
wire validation. Then specify companion-binder formation as a separate phase
and demonstrate it on QuickSort.

## 10. Revised Implementation Order

### FGSI0: Freeze corrected syntax and negative boundaries

- [ ] Add parser-negative tests for `:graph`, named cases, role references, and
      direct certified elimination before enabling each form.
- [ ] Add the reproduced block-bound recursive-result fixture permanently.
- [ ] Record deterministic current schemas for length, mirror, dependent spine,
      and QuickSort.
- [ ] Rename the misleading certified-reference AST concept.
- [ ] Freeze the rule that role projection never uses expected-type search.

Exit: current behavior is fixed by tests and the corrected grammar is accepted.

### FGSI1: Origin-group authority and block-bound recursive calls

- [ ] Add the typed compiler-local function-graph interface DB.
- [ ] Capture block Binding identity while source and recursive-call origin are
      both available.
- [ ] Fix terminal IH collection for `tailLength := *tail`.
- [ ] Generate and validate distinct value/graph role field ordinals.
- [ ] Mark the exact graph field that receives an IH.
- [ ] Freeze the interface only after graph constructors are accepted.
- [ ] Reject shadowed or duplicate short selectors deterministically.

Exit: lower and upper QuickSort calls have distinct origin groups without
public synthetic field names.

### FGSI2: Named graph cases and role references

- [ ] Add AST-only named case/selector/role-reference forms.
- [ ] Resolve the owner from the synthesized scrutinee classifier.
- [ ] Expand every case to the full constructor telescope.
- [ ] Create hidden Bindings for omitted fields.
- [ ] Create distinct value, graph, and IH Bindings linked by one origin group.
- [ ] Support whole-group aliases.
- [ ] Lower to existing MATCH and IH operations before kernel checking.

Exit: length and QuickSort proofs no longer require positional generated fields.

### FGSI3: Static inspection

- [ ] Add pre-compilation `--show-function-graph NAME` request handling.
- [ ] Add read-only REPL `:graph NAME` for accepted associations.
- [ ] Print owner, indices, constructors, origin groups, roles, and raw checked
      classifiers from the same frozen interface used by named-case lowering.
- [ ] Diagnose absent, residual, ambiguous, and unexported graphs separately.
- [ ] Add deterministic local and imported output tests.

Exit: inspection and elaboration cannot disagree about available selectors.

### FGSI4: Direct certified elimination

- [ ] Parse `*f arguments @ result => body` as a dedicated AST form.
- [ ] Introduce distinct hidden output/evidence Bindings under one origin group.
- [ ] Lower once to computation sequencing plus the current result-package
      Match.
- [ ] Keep standalone `*f arguments` valid.
- [ ] Remove `@returned` from ordinary examples after replacement tests pass.

Exit: one actual execution supplies `result` and `@result` without a generated
constructor name.

### FGSI5: Artifact v86 selectors

- [ ] Define selector-group wire grammar and update the calculus fingerprint.
- [ ] Publish only groups owned by exported graph associations.
- [ ] Relocate by association/constructor/field identity, never display name.
- [ ] Validate read, append, link, import, and republish.
- [ ] Add same-typed-field swap, wrong-owner, wrong-role, and false-recursive
      corruption fixtures.
- [ ] Make imported inspection and named elimination use the wire table.

Exit: a provider can hide its body while exporting one deterministic proof ABI.

### FGSI6: Higher-order companions and QuickSort proof

- [ ] Specify classifiers and CBPV categories for `h`, `@h`, and `*h`.
- [ ] Add distinct companion Bindings linked by one origin group.
- [ ] Replace user-visible `LeGraph` only after equivalent explicit typing is
      demonstrated.
- [ ] Implement one two-recursive-call QuickSort property using both IHs.
- [ ] Record compile time, generated graph size, Artifact bytes, and link time.

Exit: the syntax is validated on the motivating higher-order dependent example.

## 11. Permanent Verification Matrix

| Boundary | Positive test | Negative/corruption test |
| --- | --- | --- |
| Block recursive result | named `tailLength` graph generation | unrelated block result is not treated as recursion |
| Origin group | lower/upper calls remain distinct | duplicate/ambiguous selector rejected |
| Named case | omitted fields and aliases compile | unknown role and duplicate group rejected |
| Dependency | hidden earlier field supports later classifier | incomplete semantic telescope rejected |
| Role reference | `r`, `@r`, valid `*r` | `*r` on helper/actual execution rejected |
| Direct execution | exactly one run yields result/evidence | fabricated or mismatched graph rejected |
| Inspection | local/imported deterministic display | absent/unexported/ambiguous owner rejected |
| Artifact | write/read/link/republish equality | owner/field/role/recursive corruption rejected |
| Authority | selector resolves to full checked Match | no selector fallback from synthetic names/Core equality |
| Performance | no-request compilation unchanged | no whole-Term/Claim scan per selector |

The full integration suite runs after every phase. Function-graph timing must be
reported separately because QuickSort dependency closure currently dominates
the focused test.

## 12. Non-Goals

- No new function-graph kernel rule.
- No reflection of graph schemas into runtime Terms.
- No global Terms for `lowerResult`, `@lowerResult`, or `*lowerResult`.
- No reconstruction of source origins from normalized Core equality.
- No automatic proof of sortedness, permutation, or comparator laws.
- No effectful or partial function-graph semantics in this surface package.
- No removal of ordinary positional IADT Match.

## 13. Final Recommendation

Proceed with PR #21's source simplification, but use this corrected public
model:

```text
f / @f / *f
    executable / graph family / certified execution

named graph case { r; }
    introduces r, @r, and valid *r from one origin group

*f arguments @ result => body
    introduces result and @result from one execution

:graph f
    inspects the same frozen interface used by elaboration
```

Internally, these are not one Binding and not one new Core object. They are
distinct typed Bindings connected by immutable source-origin metadata and
lowered to the existing IADT, Match, IH, computation sequencing, Claim, and
Derivation infrastructure.

This preserves the proposal's no-new-reserved-identifier objective while
making the implementation compatible with dependent telescopes, CBPV,
separate compilation, future resource usage, and A Program's single-authority
design.
