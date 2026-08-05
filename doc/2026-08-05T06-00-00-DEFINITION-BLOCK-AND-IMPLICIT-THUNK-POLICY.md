# Definition Blocks and Implicit Thunk Policy

Date: 2026-08-05

Status: partially implemented. Explicit root `{{ ... }}.name`, the definition
thunk policy, executable selection, and artifact v60 are implemented. Root
legacy syntax unification and the elaborated-block IR remain pending.

## 1. Decision

A Program will distinguish two source regions.

```ap
{{ ... }}  /* Definition region. */
{ ... }    /* Computation region. */
```

The target design gives the outer source region the same semantics as a
definition region. The current implementation deliberately keeps legacy outer
definitions on their existing path until executable selection with residual
verification obligations has a stable contract. An explicit definition region
builds an order-independent name graph and does not execute effects. A
computation region sequences computations and binds their returned values.

The two regions may retain the same `:=` spelling because their delimiters fix
the meaning before type inference:

```ap
{{ name := value; }}
{ result := computation; }
```

They must not share one AST binding tag or one lowering rule.

## 2. Definition-Region Invariant

Every definition-region name denotes a value after elaboration.

```text
DefinitionGraph : Name -> ValueOccurrence
```

Definitions are parallel in the language sense:

- source order does not define execution order;
- forward references are allowed;
- duplicate definitions are rejected;
- recursive name cycles remain rejected;
- dependency order may be computed for compilation;
- selecting `.name` projects a value and does not execute it.

An ordinary value RHS is stored directly:

```ap
{{ zero := Nat.zero; }}
```

An explicit computation value is written with quotation:

```ap
{{ main := &{ #.print #"hello"; }; }}
```

Its elaborated classifier is a thunk classifier, conceptually:

```text
main : Thunk(Comp({print}, Result))
```

## 3. Compatibility Elaboration

The default surface policy initially accepts a computation RHS in a definition
region and inserts exactly one `THUNK`:

```ap
{{ main := { #.print #"hello"; }; }}
```

elaborates as if the user had written:

```ap
{{ main := &{ #.print #"hello"; }; }}
```

This is not execution, DefEq, subtyping, or a general CBPV coercion. It is one
definition-region elaboration rule:

```text
M : Comp(E, A)
------------------------------------------ DEF-IMPLICIT-THUNK
name := M  elaborates to  name := THUNK(M)
```

If the RHS is already a value, no thunk is inserted. In particular, an
explicit `&M` must never become `THUNK(THUNK(M))`.

## 4. Strict Surface Mode

The compatibility rule must be independently switchable. It must not reuse
the artifact verification policies named `strict`, `hybrid`, and
`exploratory`; those policies govern unresolved proof obligations, not surface
elaboration.

Target CLI options:

```text
--implicit-definition-thunks       default during migration
--no-implicit-definition-thunks    require explicit `&`
```

Target embedding option:

```c
enum prototype_definition_thunk_policy {
	PROTOTYPE_DEFINITION_THUNK_IMPLICIT = 1,
	PROTOTYPE_DEFINITION_THUNK_EXPLICIT
};
```

With the explicit policy, this source is rejected:

```ap
{{ main := { #.print #"hello"; }; }}
```

The diagnostic must point at the RHS and state:

```text
definition requires a value; quote this computation with '&'
```

This is technically an elaboration error, not a parser error: `{ ... }` is a
valid computation expression and the parser cannot decide its polarity. It is
still a source-level compile failure from the user's perspective.

## 5. Execution Boundary

Projection does not execute the selected definition:

```ap
{{ ... }}.main
```

If `main` denotes `Thunk(Comp(E, A))`, an execution driver runs it by applying
one `FORCE`. The conceptual executable source is:

```ap
{ {{ ... }}.main; }
```

An executable entry contract should therefore require:

```text
main : Thunk(Comp(E, A))
```

This avoids an ambiguous runtime rule that sometimes returns a selected value
and sometimes forces it. Pure entry points can be written as quoted pure
computations.

## 6. Computation-Region Binding

The computation region remains distinct:

```ap
{
	x := M;
	N;
}
```

When `M : Comp(E, A)`, the binding elaborates to sequencing:

```text
COMPUTATION_FOLD(M, \x : A => N)
```

This is not a definition thunk and is unaffected by the definition-thunk
policy. The compiler must not use one global `automatic CBPV coercions` switch
for both features.

Whether a thunk value is forced is fixed by the consuming construct. A
computation-region binding is C-style execution syntax: when its RHS denotes a
returning thunk, it forces that thunk once and binds the result. Definition
regions preserve thunks as values. Delayed computations remain storable and
passable through explicit `&`; they are executed when consumed by a computation
binding, callable position, or another strict returned-value context.

## 7. Required Representation Split

The parser AST should distinguish at least:

```text
AST_DEFINITION_BLOCK
AST_DEFINITION_BINDING
AST_COMPUTATION_BLOCK
AST_COMPUTATION_RESULT_BINDING
AST_COMPUTATION_EXPRESSION
```

Polarity elaboration should produce an intermediate representation before
TermDB lowering:

```text
DefinitionValue(name, value)
DefinitionThunk(name, computation)       /* compatibility policy only */
Sequence(computation, continuation)
Discard(computation, continuation)
Result(value)
```

TermDB then receives explicit `THUNK`, `RETURN`, and `COMPUTATION_FOLD` nodes.
It must not inspect the original `:=` spelling to recover source semantics.

## 8. Artifact Requirements

Artifacts store the elaborated graph, not the convenience spelling. Therefore
the following sources may produce equivalent graph boundaries:

```ap
{{ main := { M; }; }}
{{ main := &{ M; }; }}
```

The artifact compile-policy record should include the definition-thunk surface
policy for reproducibility and diagnostics. Readback and linking do not
re-elaborate it and must not insert or remove thunks.

## 9. Implementation Order

1. Add definition-block and definition-binding AST tags.
2. Parse `{{ ... }}` separately from `{ ... }` and parse `.name` projection.
3. Keep legacy outer definitions stable while the explicit definition path is
   validated; unify them only after Phase 4's entry-contract issue is resolved.
4. Add a dedicated definition-RHS elaborator that requires a value.
5. Add the implicit/explicit definition-thunk policy to compile options and CLI.
6. Lower implicit acceptance to an explicit OperationGraph `THUNK` occurrence.
7. Require executable entry selection to produce `Thunk(Comp(E, A))`.
8. Split block local aliasing from computation-result sequencing in elaborated
   block IR.
9. Serialize the selected surface policy in the next artifact version.
10. Add default, explicit-mode rejection, artifact, import, and execution tests.

## 10. Non-goals

- Do not restore the removed global automatic-CBPV-coercion mode.
- Do not make computation and thunk classifiers definitionally equal.
- Do not execute definition RHS terms during graph construction.
- Do not infer runtime entry execution from an arbitrary selected value.
- Do not make the artifact reader depend on the source convenience policy.

## 11. Current Implementation Baseline

The migration starts from commit `a1774bb` and the following implementation
facts.

- Outer `name := term;` definitions are stored in
  `prototype_ast_term_assignment_def` and `asts->assignments`.
- `{ ... }` items are stored as `PROTOTYPE_AST_BLOCK_BINDING`,
  `PROTOTYPE_AST_BLOCK_EXPRESSION`, or `PROTOTYPE_AST_BLOCK_LAMBDA_EXIT`.
- `parse_computation_block()` in `src/prototype/reader.c` recognizes a local
  binding by looking ahead for `:=` or `:`.
- `compile_def()` in `src/prototype/ast.c` accepts either a value or a
  computation and records both the compiled TermDB root and OperationGraph
  occurrence.
- `compile_ast_block_items_ref()` and
  `compile_ast_block_items_control_ref()` decide during lowering whether a
  block binding is a local value alias or a computation fold.
- `compile_ast_block_execution_ref()` contains computation-region thunk opening
  behavior. Definition-region elaboration does not call this helper.
- `prototype_compile_options` carries a separate definition-thunk surface
  policy in addition to verification policy and step limits.
- Artifact v60 stores the definition-thunk policy and optional selected-entry
  term, classifier, and OperationGraph occurrence.

The migration must remove semantic branching on the shared source spelling. It
must not keep the old lowering as a compatibility path beside the new one.

## 12. Target Compilation Pipeline

The target pipeline is:

```text
source tokens
  -> parser AST with distinct definition/computation regions
  -> name and region resolution
  -> polarity elaboration
  -> explicit elaborated IR
  -> TermDB + OperationGraph construction
  -> constraint generation and solving
  -> Judgement proof validation
  -> artifact reachability slice and serialization
```

The elaborated IR is the semantic boundary. All implicit definition thunking
must be visible there. TermDB construction must not ask whether a source RHS
was written inside `{ ... }`, `{{ ... }}`, or the outer source region.

### 12.1 Definition elaboration

```text
elaborate_definition(name, rhs):
    rhs_ref = elaborate(rhs)
    if rhs_ref is Value:
        emit DefinitionValue(name, rhs_ref)
    else if rhs_ref is Computation and policy is IMPLICIT:
        quoted = emit explicit THUNK occurrence(rhs_ref)
        emit DefinitionValue(name, quoted)
    else:
        reject at rhs source span
```

### 12.2 Computation binding elaboration

```text
elaborate_computation_binding(x, rhs, tail):
    rhs_ref = elaborate(rhs)
    if rhs_ref is Value:
        source = RETURN(rhs_ref)
    else if rhs_ref is returning Computation:
        source = rhs_ref
    else:
        reject
    emit Sequence(source, lambda x => elaborate(tail))
```

A thunk is a value, but computation-region `:=` is a strict consuming construct
in the adopted C-style surface semantics. It inserts one FORCE before the
sequence. Definition-region `:=` is not strict and stores the thunk itself.

The Core simplifier may later reduce
`Sequence(RETURN(v), \x => tail)` to `tail[v/x]`; the source elaborator must not
replace the semantic rule with a local-reference special case.

## 13. Detailed Implementation Phases

### Phase 0: Freeze current behavior with characterization tests

Purpose: distinguish intended migration changes from accidental regressions.

Changes:

- extend `src/prototype/test_cbpv_surface.sh` with current outer-definition,
  block-value-binding, block-computation-binding, quoted computation, and
  artifact round-trip cases;
- add a test proving that a thunk can be bound locally without being forced;
- add a test proving that an effectful computation bound in `{ ... }` executes
  exactly once;
- retain all numbered examples currently expected to pass.

Exit criteria:

- [x] all characterization cases have explicit output assertions;
- [x] all prototype tests pass after the structural changes.

### Phase 1: Introduce separate parser AST forms

Files:

- `src/prototype/ast.h`
- `src/prototype/ast.c`
- `src/prototype/reader.c`
- `src/prototype/ast_inspect.c`

Add parser-level forms:

```text
PROTOTYPE_AST_DEFINITION_BLOCK
PROTOTYPE_AST_DEFINITION_BINDING
PROTOTYPE_AST_DEFINITION_SELECT
PROTOTYPE_AST_COMPUTATION_BINDING
```

Add dedicated storage arrays for definition-block entries and their source
spans. Do not put definition entries in `block_items`; they have different
scope and ordering invariants.

Parsing rules:

```ap
{{ name := rhs; other := rhs2; }}.selected
{ x := rhs; expression; }
```

- two consecutive opening braces begin a definition block;
- one opening brace begins a computation block;
- every definition block requires one `.name` projection in expression
  position for the initial implementation;
- the selected name must be a direct member of that block;
- definition names enter a block-local definition namespace, not the lexical
  Lambda binder stack;
- all definitions are collected before their references are resolved, which
  permits forward references;
- duplicate names are rejected during definition-scope construction;
- references outside the definition block do not see unselected private names.

Outer definitions continue to parse temporarily through their existing path.
An attempted immediate wrapping exposed the Phase 4 entry-contract problem, so
the implementation does not silently change their classifiers or runtime
behavior.

Exit criteria:

- [x] parser distinguishes `{` from `{{` without token ambiguity;
- [x] AST inspection shows distinct definition and computation nodes;
- [x] forward reference succeeds and duplicate definitions are rejected;
- [ ] nested definition blocks obey lexical definition-scope boundaries.

Implementation note: the current root-only form stores assignment IDs in a
dedicated definition-block item array. A separate definition-binding AST tag is
deferred to Phase 2 because assignments remain the compiler's authoritative
definition records.

### Phase 2: Add explicit elaborated-block IR

Files:

- `src/prototype/ast.h`
- `src/prototype/ast.c`

Add an internal, non-serialized elaboration representation. Suggested tags:

```text
PROTOTYPE_ELAB_DEFINITION_VALUE
PROTOTYPE_ELAB_DEFINITION_THUNK
PROTOTYPE_ELAB_SEQUENCE
PROTOTYPE_ELAB_DISCARD
PROTOTYPE_ELAB_RESULT
PROTOTYPE_ELAB_LAMBDA_EXIT
```

The representation owns source occurrence IDs and classifiers as they become
available. It does not own TermDB execution state.

Refactor these current paths:

- split `compile_def()` into definition resolution, definition RHS
  elaboration, and graph materialization;
- replace the polarity branch in `compile_ast_block_items_ref()` with
  elaboration to `SEQUENCE`;
- apply the same change to `compile_ast_block_items_control_ref()` so Lambda
  exit handling does not retain an alternate binding semantics;
- keep `compile_continue_runtime_computation()` as a Core construction helper,
  but call it from materialization of explicit `SEQUENCE`, not directly from
  parser AST interpretation.

Exit criteria:

- [ ] no TermDB lowering function decides the meaning of `:=`;
- [ ] value bindings and computation bindings share one sequencing rule;
- [ ] Lambda exit control uses the same elaborated sequence representation.

### Phase 3: Add the definition-thunk surface policy

Files:

- `src/prototype/reader.h`
- `src/prototype/reader.c`
- `src/prototype/read_file.c`
- `src/prototype/repl.c`
- `src/prototype/ast.h`
- `src/prototype/ast.c`

Add `definition_thunk_policy` to `prototype_compile_options`. Zero-initialized
embedders resolve zero to the migration default `IMPLICIT`; do not let zero
mean different things in different frontends.

Add CLI handling to both compiler and REPL entry paths:

```text
--implicit-definition-thunks
--no-implicit-definition-thunks
```

Do not connect this option to `compile_policy`. `--policy strict` may still use
implicit definition thunks, and `--policy exploratory` may require explicit
ones.

Implement one helper with one responsibility, conceptually:

```text
compile_definition_value_ref(ctx, rhs_ast, policy, out)
```

It first attempts Value elaboration. A Computation result is accepted only in
implicit mode and is converted through the existing explicit THUNK
OperationGraph constructor. It must not create a bare TermDB THUNK without its
source occurrence and proof path.

Exit criteria:

- [x] default mode accepts the convenience spelling;
- [x] explicit mode rejects it at the RHS span;
- [x] explicit `&{ ... }` passes in both modes;
- [x] both accepted spellings produce equivalent reachable Core boundaries;
- [x] neither mode executes the RHS during compilation.

### Phase 4: Unify the outer source region with definition blocks

Files:

- `src/prototype/reader.c`
- `src/prototype/ast.h`
- `src/prototype/ast.c`

Replace the semantic role of `asts->assignments` with a root definition scope.
The existing open-address name index may be reused internally, but it must be
owned by a definition scope rather than treated as a special global language
feature.

Required properties:

- compiling several files into one namespace merges root definition scopes;
- private nested definition blocks do not become artifact exports;
- outer names and `{{ ... }}` names use the same duplicate, dependency, cycle,
  classifier, and implicit-thunk rules;
- compilation order may follow a dependency topology but does not become part
  of language semantics;
- recursive name invocation remains rejected.

Remove the old direct `compile_phase_build_graph()` iteration once the root
definition graph owns compilation. Do not retain old assignment lowering as a
fallback.

This phase is intentionally deferred. The attempted migration wrapped every
legacy definition as a value and selected a conventional `main`. That changed
existing exported normalization behavior and failed for entries whose
classifier is valid only with a residual VerificationDB obligation. Before
retrying, the design must distinguish:

- definition/export identity;
- executable-entry selection;
- whether an entry classifier must be fully solved or may carry a serialized
  residual obligation;
- whether legacy source declares an executable entry at all.

Exit criteria:

- [ ] old outer syntax and an explicit root `{{ ... }}` produce equivalent
  definition graphs during migration;
- [ ] multi-file namespace tests still pass;
- [ ] definitions do not execute merely because they are compiled or imported.

### Phase 5: Define selection and executable entry behavior

Files:

- `src/prototype/reader.c`
- `src/prototype/ast.h`
- `src/prototype/ast.c`
- `src/prototype/repl.c`
- `src/prototype/read_file.c`

`PROTOTYPE_AST_DEFINITION_SELECT` returns the selected Value occurrence. It
does not FORCE it.

For executable mode, require the selected entry classifier to normalize to:

```text
Thunk(Comp(E, A))
```

The runtime entry adapter inserts exactly one FORCE. Compilation-only and
artifact-writing commands do not run the adapter.

Do not accept an arbitrary Value entry by guessing whether to RETURN or FORCE.
A pure entry is represented by a thunked pure computation. Diagnostics must
distinguish:

- selected name not found;
- selected definition is not a value after elaboration;
- executable entry is not `Thunk(Comp(E, A))`;
- selected entry has unresolved effect or classifier obligations.

Exit criteria:

- [x] selecting `main` while compiling produces no output effect;
- [x] running the same selected `main` forces it once;
- [x] importing an artifact does not run its selected entry;
- [ ] two explicit runtime invocations execute the thunk twice in one driver
  session.

### Phase 6: Remove current mixed block-binding behavior

Files:

- `src/prototype/ast.h`
- `src/prototype/ast.c`
- `src/prototype/reader.c`

After all callers use elaborated IR:

- remove `PROTOTYPE_AST_BLOCK_BINDING`;
- remove direct `push_local_ref()` handling as the semantic implementation of
  a block `:=`;
- retain local-reference maps only as an implementation of Lambda/continuation
  binder lookup;
- remove generic automatic FORCE from `compile_ast_block_source_ref()`;
- make callable auto-open and any expected-result FORCE separate, named
  elaboration rules;
- simplify `compile_ast_block_items_ref()` and the control variant or delete
  them after their responsibilities move to elaboration/materialization.

Exit criteria:

- [ ] every computation-region `:=` has a continuation binder in elaborated
  semantics, even if `BIND(RETURN(v), k)` later reduces away;
- [x] a computation-region binding executes a returning thunk exactly once;
- [ ] callable use of a thunked Pi still executes the function computation as
  specified;
- [ ] effect order remains left-to-right.

### Phase 7: Artifact migration

Files:

- `src/prototype/ast.c`
- `src/prototype/ast.h`
- `src/prototype/test_artifact_flow.sh`

Artifact v60 is the implemented format for this phase.

Serialize:

- the definition-thunk surface policy in compile metadata;
- root selected-entry identity, when an executable entry is declared;
- only elaborated reachable `THUNK`, `RETURN`, and `COMPUTATION_FOLD` graph
  nodes, never parser AST definition blocks.

Validate:

- enum value is known;
- an executable selected entry exists in the interface and has the serialized
  classifier;
- no readback path inserts an omitted THUNK;
- linking artifacts compiled under different surface policies is allowed when
  their elaborated interfaces are compatible;
- aggregate output records its own selected-entry decision rather than taking
  one silently from input order.

Exit criteria:

- [x] new artifacts round-trip and aggregate;
- [x] the immediately previous version is rejected cleanly;
- [x] forged surface-policy and selected-entry fields are rejected;
- [x] explicit and implicit source spellings have equivalent runtime behavior.

### Phase 8: Remove transitional outer syntax, if approved

This is a separate user-visible language decision after the migration is
stable.

Possible final source form:

```ap
{{
	definitions
}}.main
```

At that point, remove parsing of unwrapped outer `name := rhs;` entries. Do not
keep both forms indefinitely. Documentation and examples should migrate in one
reviewable change.

Exit criteria:

- [ ] all standard and training sources use the selected final form;
- [ ] parser rejects the removed form with a focused diagnostic;
- [ ] no compatibility branch remains in graph compilation.

## 14. Test Matrix

| Area | Default policy | Explicit policy | Required observation |
|---|---|---|---|
| Definition Value | accept | accept | no THUNK inserted |
| Explicit `&M` definition | accept | accept | one THUNK |
| Bare Computation definition | accept | reject | default inserts one THUNK |
| Bare effectful definition compile | accept | reject | no effect during compile |
| Definition forward reference | accept | accept | same graph as reordered source |
| Definition cycle | reject | reject | recursive-name diagnostic |
| Computation Value binding | accept | accept | `RETURN` then sequence |
| Computation binding | accept | accept | execute once and bind result |
| Computation-region thunk binding | accept | accept | FORCE once and bind result |
| Callable thunk use | accept | accept | FORCE only in callable position |
| Selected main compile | accept | accept | no execution |
| Selected main run | accept | accept | exactly one FORCE |
| Artifact read/link | accept | accept | no re-elaboration |

Permanent tests should be added to the existing focused scripts rather than a
single monolithic new script:

- parser and CBPV surface cases: `test_cbpv_surface.sh`;
- block sequencing and Lambda exit: `test_computation_block_sequence.sh`;
- source occurrence and shared Core boundaries: `test_shared_core_occurrences.sh`;
- artifact format, mutation, import, and aggregation: `test_artifact_flow.sh`.

The full acceptance run is:

```sh
make clean
make all reader
for test in src/prototype/test_*.sh; do
	sh "$test"
done
```

Numbered examples expected by the current project policy must also compile
under the default definition-thunk policy. Explicit-mode fixtures are separate
because old examples intentionally use migration convenience syntax.

## 15. Risks and Controls

### Shared Core identity

Explicit and implicit quotation may share a Core THUNK node. Their
OperationGraph occurrences must remain distinct so diagnostics and surface
policy provenance are not recovered from TermDB identity.

### Definition dependency versus execution order

A topological compilation order is an implementation detail. Never serialize
it as an execution order and never turn definition dependencies into
computation sequencing.

### Nested thunk collapse

Only a Computation RHS receives an implicit THUNK. A Value whose classifier is
already `Thunk(C)` remains unchanged. Test `Thunk(Thunk(C))` explicitly so a
shape-based convenience rule cannot collapse it.

### Artifact policy confusion

Verification policy controls unresolved obligations. Definition-thunk policy
controls accepted source sugar. Store separate enum fields and use separate
CLI parsing variables.

### Entrypoint side effects during tooling

AST inspection, artifact writing, readback, linking, backend capability checks,
and type checking must never invoke the runtime entry adapter. Only an explicit
run command does so.

### Scope leakage

Definition block names are namespace labels, not Lambda binders. Computation
block result names are continuation binders, not exports. Keep separate symbol
tables and separate source identifiers even when the displayed spelling is the
same.

## 16. Progress Tracking

| Phase | State | Notes |
|---|---|---|
| 0. Characterization tests | complete | Full prototype suite and examples 01-09 pass |
| 1. Parser AST split | partial | Root-only `{{ ... }}.name` implemented; nested scopes and a distinct binding IR remain |
| 2. Elaborated-block IR | pending | Current lowering still interprets block AST directly |
| 3. Definition-thunk policy | complete | Independent CLI policy and explicit OperationGraph THUNK are implemented |
| 4. Root definition graph | deferred | Legacy-root migration changes residual-entry and export semantics; see Phase 4 |
| 5. Selection and entry adapter | partial | Explicit root selection and one FORCE are implemented; repeated invocation contract remains |
| 6. Remove mixed block binding | partial | Strict contexts share thunk detection; explicit elaborated sequence IR remains absent |
| 7. Artifact migration | complete | Artifact v60 round-trips, validates, and aggregates selected-entry metadata |
| 8. Remove transitional syntax | pending | Requires explicit approval after migration |

Update this table and append a dated progress entry whenever implementation
changes a decision, reveals a blocker, or completes a phase. Do not mark a
phase complete until its exit criteria and the full regression suite pass.

## 17. Change Log

### 2026-08-05: Explicit root definition blocks implemented

- Added root-only `{{ ... }}.name` parsing, dedicated definition item storage,
  AST inspection, direct-member validation, and forward-reference coverage.
- Added independent implicit/explicit definition-thunk policies to compiler and
  REPL options. A bare computation RHS becomes one explicit THUNK only in
  implicit mode.
- Added a selected-entry OperationGraph FORCE. Compilation and artifact tooling
  do not execute the entry; executable REPL startup forces it once.
- Bumped artifacts to v60 and serialized the surface policy plus selected entry
  symbol, term, classifier, and operation. Readback and aggregation validate the
  new boundary.
- Fixed FORCE(THUNK(LAMBDA)) normalization, delayed dependent-Pi ascription
  solving, strict APP/Match consumption of returning thunks, and preservation
  of occurrence-specific classifiers for typed views sharing one Core node.
- Preserved computation-block execution semantics: each `x := delayed`
  executes the returning thunk once. An intermediate alias-only implementation
  incorrectly collapsed two executions and was removed.
- Tried and rejected automatic migration of every legacy outer source to an
  implicit definition block. It changed existing export normalization and
  could not represent a selected entry whose classifier remains a valid
  residual verification obligation. Phase 4 now records this as a design
  prerequisite rather than retaining a compatibility branch.
- Added focused definition-block fixtures for policy behavior, effects,
  forward references, typed views sharing one Core Lambda, duplicate names,
  selected-name validation, and artifact metadata.
- Verified every `src/prototype/test_*.sh` script and numbered examples 01-07
  and 09. There is no `examples/08_*.p` fixture.

### 2026-08-05: Detailed implementation plan added

- Anchored the plan to the current parser, AST, lowering, compile-option, and
  artifact implementation.
- Split the migration into characterization, syntax, elaboration, policy, root
  definition graph, entry execution, cleanup, artifact, and final syntax
  phases.
- Fixed the rule that computation-region assignment does not automatically
  FORCE a thunk value.
- Fixed the executable entry contract at `Thunk(Comp(E, A))` with one runtime
  FORCE.
- Added file-level change targets, phase exit criteria, a permanent test
  matrix, risks, and progress tracking.
