# Computation-Fold, Block Sequencing, and Lambda Exit Migration Plan

Date: 2026-08-04

## 1. Status

Planning complete. Implementation has not started. The plan includes an
initial terminology migration from the current `DEEP_FOLD` spelling to
`COMPUTATION_FOLD`; later sections use the target spelling unless they quote a
current C identifier.

This document is the current authority for computation-block representation,
block result selection, discarded computation results, and lexical Lambda exit.
It supersedes the block grammar and block-lowering sections of:

- `2026-07-17T03-00-00-COMPUTATION-BLOCK-QUOTATION-PLAN.md`;
- `2026-07-18T00-00-00-EXECUTION-SUPPRESSION-CBPV-SURFACE-PLAN.md`.

Those documents remain useful as implementation history. Their descriptions of
an independent Core `BIND`, recursive `BLOCK_BIND(value, rest)`, and an erased
mandatory `!result;` terminator are not the target design.

The worktree on 2026-08-04 contains a transitional reader change that requires
`!result;` and erases `!` during parsing. That behavior is not accepted by this
plan. It must be replaced rather than retained as a compatibility path.

## 2. Purpose

The current computation-block AST stores source order as a right-recursive
linked structure:

```text
COMPUTATION_BLOCK(BLOCK_BIND(x, M, BLOCK_BIND(y, N, tail)))
```

That representation was sufficient for the first CBPV surface prototype, but
it makes the following extensions unnecessarily fragile:

1. selecting a named block result with `{ ... }.name`;
2. stopping semantic compilation at the selected binding;
3. executing an intermediate computation while discarding its result;
4. distinguishing normal block completion from lexical Lambda exit;
5. retaining stable source occurrences for diagnostics and OperationGraph
   provenance;
6. preserving ContextDB/SubstitutionDB indexing during sequencing.

The migration replaces the recursive source representation with an ordered AST
item slice. It continues to elaborate computation sequencing through generic
`COMPUTATION_FOLD`; it does not reintroduce a Core `BIND` node.

## 3. Existing Design Commitments

This migration must preserve the following decisions already implemented by the
current codebase.

### 3.1 Shared structural graph and typed occurrences

- `TermDB` is the shared, context-erased computation graph.
- `OperationGraph` records typed source occurrences, polarity, computation kind,
  application role, context, classifiers, and source provenance.
- Structurally equal Core terms may be shared while their typed occurrences
  remain distinct.
- Runtime environments are execution-machine state and are not stored in
  `TermDB`.

### 3.2 Context-indexed elaboration

- `ContextDB` represents immutable context extension.
- `SubstitutionDB` represents explicit context morphisms.
- A computation-result binder extends the Context before its continuation is
  compiled.
- An array representation does not justify compiling items backwards. Source
  items are visited from front to back; right-associated Core structure is
  assembled while recursive elaboration unwinds.

### 3.3 Generic computation fold

The current code calls this constructor `DEEP_FOLD`. The target name is
`COMPUTATION_FOLD`: folding a computation is the semantic operation, while
deep forwarding is its rule for omitted operation clauses. `deep` therefore
belongs in the reduction-law description, not in the constructor identity.

The authoritative effect-tree constructors are:

```text
RETURN(value)
OPERATION_REQUEST(operation, argument, continuation)
COMPUTATION_FOLD(computation, return_clause, clauses)
```

Sequencing is the zero-clause specialization:

```text
sequence(M, K) := COMPUTATION_FOLD(M, K, [])
```

The historical notation `BIND(M, K)` may be used in explanatory equations, but
there is no independent `PROTOTYPE_TERM_BIND`, `PROTOTYPE_OPERATION_BIND`, or
typing rule. A computation block must construct a zero-clause `COMPUTATION_FOLD`.

### 3.4 Reduction boundaries

- Beta reduction remains Lambda/App computation.
- Constructor iota remains Match computation.
- `THUNK` and `FORCE` preserve the CBPV value/computation boundary.
- `COMPUTATION_FOLD` interprets effect trees; it does not absorb beta or iota into an
  effect handler.
- Pure primitive evaluation and effect-operation dispatch remain distinct.

### 3.5 Effects and operation requests

- A pure primitive such as `#.int_add` is not an effect request merely because
  it is intrinsic.
- An effect operation such as `#.print` becomes an `OPERATION_REQUEST` only
  through the operation-request surface path.
- Writing an expression as a block statement must not silently convert an
  ordinary application into `perform`.

### 3.6 Artifact boundary

Artifacts serialize reachable Core and typed operation data, contexts,
substitutions, proofs, constraints, and residual verification obligations. They
do not serialize the source AST. An AST-only representation change therefore
does not require an artifact format bump. A new Core or OperationGraph tag does.

Artifact v54 serializes TermDB, OperationGraph, proof, constraint, obligation,
and capability tags as integer enum values. The terminology migration renames
the existing enum members in place and preserves every numeric value, field,
clause ordering rule, and payload. It therefore does not bump the artifact
version. Reordering an enum or inserting a value before the renamed members is
forbidden during that migration.

## 4. Target Surface Grammar

The target block grammar is:

```text
block             := "{" block_item+ "}" block_selector?
block_selector    := "." identifier
block_item        := block_binding ";"
                   | block_expression ";"
                   | lambda_exit ";"
block_binding     := identifier (":" type_expr)? ":=" term
block_expression  := term
lambda_exit       := "!" term
```

`lambda_exit` is introduced only in Stage 2. During Stage 1, `!` is rejected.

The selector is parsed as part of the computation block. It is not a general
member-selection operator and must not be represented as a namespace-qualified
name. This keeps `{ ... }.x` distinct from `Namespace.x`.

An empty block is rejected until the language has an accepted Unit policy.

## 5. Block Result Semantics

### 5.1 Implicit final result

Without a selector, the last item supplies the block result:

```text
{
	x := M;
	f x;
}
```

- If the final item is a value, it is lifted with `RETURN`.
- If the final item is a returning computation, it is the tail computation.
- If the final item is a raw function computation, it remains a function
  computation according to the existing nested-Lambda rule.
- If the final item is a binding, the bound result is returned as though the
  block ended in an expression referring to that binding.

### 5.2 Named result and semantic cutoff

For:

```text
{
	x := M;
	y := N;
	z := P;
}.y
```

the selected binding `y` is the result and the semantic block prefix ends
immediately after that binding. `z := P` is source text but is not part of the
compiled program.

The suffix after the cutoff:

- must still be lexically and syntactically valid;
- may participate in ordinary parser scope checks;
- is not lowered into `TermDB` or `OperationGraph`;
- emits no classifier, equality, effect-row, proof, or verification constraint;
- contributes no effect and is not serialized into an artifact.

The selector must resolve to a binding declared directly in the selected block.
It cannot select an outer binding, nested binding, or expression statement.
Duplicate bindings in one block remain errors.

### 5.3 Discarded expression statement

An intermediate expression statement executes a returning computation and
discards its result:

```text
{
	x := M;
	perform (#.print #"ready");
	N;
}
```

Conceptually, the print statement elaborates to:

```text
COMPUTATION_FOLD(print_request, LAMBDA(ignored, lower(N)), [])
```

The ignored result binder is a normal generated continuation binder. It is not
a new source binding and cannot be referenced by later source.

An intermediate pure Value statement is rejected as non-executable and
meaningless. A raw function computation statement is rejected unless it is the
selected or final result. A pure primitive application is still a computation
and may be sequenced and discarded.

## 6. Target AST Representation

### 6.1 Item nodes

Replace `PROTOTYPE_AST_BLOCK_BIND` with source-occurrence item nodes:

```text
PROTOTYPE_AST_BLOCK_BINDING
PROTOTYPE_AST_BLOCK_EXPRESSION
PROTOTYPE_AST_BLOCK_LAMBDA_EXIT
```

The binding node stores:

```text
ast_binder_id
binder_symbol_id
binder_type
value
```

It does not store `rest`.

The expression and exit nodes each store their operand AST ID. Keeping every
item as an AST node preserves a stable `source_ast` for diagnostics and
OperationGraph provenance.

### 6.2 Block slice

`PROTOTYPE_AST_COMPUTATION_BLOCK` stores:

```text
first_item
item_count
result_item_index
result_mode
```

`result_mode` initially distinguishes:

```text
PROTOTYPE_BLOCK_RESULT_FINAL_ITEM
PROTOTYPE_BLOCK_RESULT_SELECTED_BINDING
```

Lambda exit is an item control-flow form, not another normal result mode.

### 6.3 Arena ownership

Add a DB-owned `uint32_t block_item_ids[]` arena following the existing Match
case and constructor-array ownership model. Do not allocate one heap array per
block.

`prototype_ast_db_init` currently receives caller-owned arenas. Extending it
requires synchronized changes to its callers, including `repl.c` and
`read_file.c`. Capacity failure must be reported as an AST allocation error and
must not leave a partially published block slice.

## 7. Reader Migration

Replace recursive `parse_block_body` construction with iterative collection.

1. Save the outer binder scope.
2. Parse each item in source order.
3. Allocate an AST binder immediately for a binding and expose it only to later
   items.
4. Record item AST IDs in a temporary fixed-capacity parser array.
5. Parse the closing brace.
6. Parse an optional immediate `.identifier` selector.
7. Resolve the selector against direct bindings in that block and compute the
   result item index.
8. Copy the item ID slice into the AST DB arena and publish one block node.
9. Restore the outer binder scope.

The reader must distinguish binding starts with the existing identifier plus
`:`/`:=` lookahead. Every other term-start token begins an expression item,
except `!`, which is reserved for Stage 2 Lambda exit.

No compatibility parser is retained for mandatory erased `!result;`.

## 8. Stage 1 Lowering

### 8.1 Reachable prefix first

The block compiler computes the semantic cutoff before compiling any item. It
must never create OperationGraph nodes for suffix items and then attempt to
remove them. Constraints, effects, and proof records are derived from graph
reachability, so late deletion is too late.

### 8.2 Context-forward elaboration

Introduce an indexed helper conceptually equivalent to:

```text
compile_block_items(ctx, block, item_index, cutoff_index)
```

It visits the current item first and compiles the continuation under the
resulting Context.

For a returning computation binding:

1. compile the right-hand side in the current Context;
2. extend ContextDB with the source binding;
3. compile the remaining reachable prefix in the child Context;
4. construct the continuation `LAMBDA(binding, rest)`;
5. construct `COMPUTATION_FOLD(rhs, continuation, [])` while unwinding.

For a Value or raw-function alias:

1. add the existing local reference mapping;
2. compile the remainder without `COMPUTATION_FOLD`;
3. restore the previous local-reference scope.

For a discarded returning computation:

1. compile the expression;
2. extend a generated continuation Context with an inaccessible result binder;
3. compile the remainder;
4. construct the continuation Lambda and zero-clause `COMPUTATION_FOLD`.

Do not reverse-walk the array and compile right-hand sides in the wrong
Context. The Core result is right-associated, but the context construction is
source-order dependent.

### 8.3 Shared sequencing helper

The current code has overlapping execution-demand logic in
`compile_ast_block_body_ref` and `compile_ast_runtime_value_then`. Refactor the
common polarity decisions behind one sequencing API that accepts:

- the computation occurrence;
- an optional explicit source binder identity;
- an optional binder classifier annotation;
- a continuation callback;
- the source AST occurrence.

The helper creates a zero-clause `COMPUTATION_FOLD`. It must not expose a new BIND AST,
TermDB, OperationGraph, proof, or constraint kind.

### 8.4 Selected computation binding

If the selected result is a returning computation binding, retain the explicit
sequencing shape:

```text
COMPUTATION_FOLD(M, LAMBDA(x, RETURN(x)), [])
```

Do not optimize it directly to `M` during lowering. The continuation domain may
carry the binding annotation, and the typed occurrence/proof must show that the
returned result satisfies that annotation. A later normalization rule may prove
the equivalent form.

## 9. Typing, Effects, Proofs, and Runtime

### 9.1 Typing and constraints

The migration adds no block-specific typing rule. Existing operation rules
apply to the generated occurrences:

- `RETURN` introduces an empty effect row;
- zero-clause `COMPUTATION_FOLD` combines source and continuation rows;
- Lambda introduces the continuation binder in its child Context;
- final or selected result determines the block classifier;
- dependent fold results continue to use occurrence-local residual
  verification obligations when static solving is exhausted.

The block AST must not write a solved classifier directly. It contributes typed
OperationGraph occurrences and lets the existing fixed-point and constraint
pipeline solve them.

### 9.2 Effects

Discarding a result discards only the returned value, not the source effect
row. A cutoff suffix contributes no row at all. This distinction requires tests:

```text
executed-and-discarded operation  -> effect retained
item after `.selected` cutoff     -> effect absent
```

### 9.3 Proofs

Use `PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM` for all generated sequencing.
Do not recreate `BIND_INTRO`. Proof validation must reconstruct the same
zero-clause fold premises from the OperationGraph occurrence.

### 9.4 Runtime

No Stage 1 runtime rule is added. The runtime already distinguishes a
zero-clause `COMPUTATION_FOLD` from a clause-bearing fold. Internal frame names may use
`sequence`, but both execute the generic computation-fold representation.

## 10. Stage 2: Lexical Lambda Exit

### 10.1 Meaning

Inside the computation body of a source Lambda:

```text
f := \x : A => {
	condition @true => { !x; }
	          @false => { y := N; y; };
	M;
};
```

`!x` exits the nearest enclosing source Lambda. It is not normal completion of
the nested block and is not the effect-tree constructor `RETURN(x)` by itself.
Subsequent computation in the Lambda body is skipped on that path.

### 10.2 Why direct erasure is invalid

Compiling `!x` directly to `RETURN(x)` is incorrect because an enclosing
zero-clause `COMPUTATION_FOLD` applies its continuation to every normal return. A
clause-bearing `COMPUTATION_FOLD` also applies its return clause. The result would
resume code that lexical exit is required to skip.

Therefore the transitional implementation that parses `!result;` and erases
the marker must be removed before Stage 2.

### 10.3 Initial supported region

The first accepted lexical-exit implementation has these boundaries:

- `!` is valid only in a computation block enclosed by a source Lambda;
- nested ordinary blocks do not create a new exit target;
- a nested source Lambda creates a new target;
- quotation `&M` is a barrier because `M` may run after the outer Lambda has
  returned;
- `!` under a handler body or handler clause is initially rejected;
- `!` outside a Lambda is rejected;
- `!` is a block item, not a general prefix term operator;
- the initial elaborator propagates exit through Lambda bodies, block items,
  computation-binding right-hand sides, and Match branch bodies; an
  exit-bearing block is not accepted as an APP argument, Match scrutinee,
  constructor field, operation argument, or ascribed subterm.

Rejecting handler crossing is intentional. Normal handler return clauses and
lexical exit are different control effects. Treating them as the same return
would be unsound.

### 10.4 Elaboration strategy

Implement lexical exit as continuation-aware source elaboration, not as a new
serialized Core node.

The compiler carries two compile-time continuations while lowering the affected
Lambda body:

```text
normal continuation  - continue the surrounding block
exit continuation    - produce the Lambda result immediately
```

`!value` invokes the exit continuation. A normal item invokes the normal
continuation. Match lowering must pass both continuations into every branch so
one branch may exit while another continues. Shared TermDB interning may recover
identical structural tails; OperationGraph occurrences remain source-specific.

This stage must not begin until the continuation contract is written as tests
for nested blocks, Match branches, nested Lambdas, quotation barriers, and
handler rejection.

### 10.5 Deferred handler crossing

Supporting lexical exit through arbitrary handlers requires a separate design.
Candidates include:

1. a scoped, answer-type-indexed control operation handled at the Lambda
   boundary;
2. a full control-flow/CPS translation that propagates normal and exit outcomes
   through every computation-fold clause;
3. an explicit Core control node with corresponding typing, runtime, proof, and
   artifact rules.

This plan selects none of these implicitly. Adding a compiler-private global
effect operation is not acceptable without solving answer-type polymorphism,
scope identity, graph sharing, and artifact identity. Until a later plan is
approved, handler crossing remains a compile-time error.

## 11. Computation-Fold Terminology Migration

### 11.1 Scope confirmed by code audit

The 2026-08-04 audit found 323 `DEEP_FOLD`/`deep_fold` references across twelve
prototype files. The name is part of more than the Core constructor:

- TermDB tags, payload fields, clause arena, capacity, constructor API,
  equality, hashing, substitution, relocation, normalization, and printing;
- OperationGraph tags and operation constraints;
- judgement proof kinds, proof construction, solving, and proof validation;
- residual verification obligations and runtime capability bits;
- runtime frames, request forwarding, resumption, and result discharge;
- artifact reachability, writing, reading, validation, graph cloning, and
  linking;
- type-declaration shape comparison;
- diagnostics and representation-sensitive shell/C tests.

The affected implementation files are currently:

```text
src/prototype/term.h
src/prototype/term.c
src/prototype/ast.h
src/prototype/ast.c
src/prototype/judgement.h
src/prototype/typing.c
src/prototype/read_file.c
src/prototype/type_declaration.c
src/prototype/cbpv_boundary_check.c
src/prototype/whnf_profile_cache_check.c
src/prototype/test_cbpv_surface.sh
src/prototype/test_artifact_flow.sh
```

This must be one isolated mechanical-semantic rename before the block AST
migration. Mixing it with new block behavior would make representation failures
indistinguishable from terminology failures.

### 11.2 Authoritative rename map

Rename the constructor and every directly derived identifier consistently:

```text
PROTOTYPE_TERM_DEEP_FOLD
  -> PROTOTYPE_TERM_COMPUTATION_FOLD

PROTOTYPE_OPERATION_DEEP_FOLD
  -> PROTOTYPE_OPERATION_COMPUTATION_FOLD

PROTOTYPE_JUDGEMENT_PROOF_DEEP_FOLD_ELIM
  -> PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM

PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_DEEP_FOLD
  -> PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD

OPERATION_CONSTRAINT_DEEP_FOLD_RESULT
  -> OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT

PROTOTYPE_VERIFICATION_OBLIGATION_DEEP_FOLD_RESULT
  -> PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT

PROTOTYPE_RUNTIME_CAPABILITY_DEEP_FOLD_RESULT_VERIFIER
  -> PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER

OPERATION_RUNTIME_FRAME_DEEP_FOLD_RESULT
  -> OPERATION_RUNTIME_FRAME_COMPUTATION_FOLD_RESULT

PROTOTYPE_DEEP_FOLD_CLAUSE_CAPACITY
  -> PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY

struct prototype_deep_fold_clause
  -> struct prototype_computation_fold_clause

term.as.deep_fold
  -> term.as.computation_fold

term_db.deep_fold_clauses
  -> term_db.computation_fold_clauses

term_db.deep_fold_clause_count
  -> term_db.computation_fold_clause_count

prototype_term_deep_fold
  -> prototype_term_computation_fold

prototype_judgement_deep_fold_result_classifier
  -> prototype_judgement_computation_fold_result_classifier

prototype_judgement_delta_record_deep_fold_elim
  -> prototype_judgement_delta_record_computation_fold_elim

prototype_verification_db_discharge_deep_fold_result
  -> prototype_verification_db_discharge_computation_fold_result

deep_fold_rebuild / deep_fold_continuation / deep_fold_find_clause
  -> computation_fold_rebuild / computation_fold_continuation /
     computation_fold_find_clause
```

Static helpers, solver functions, runtime functions, local variables, comments,
and diagnostics follow the same spelling. Printed Core changes from
`DEEP_FOLD(...)` to `COMPUTATION_FOLD(...)`; proof diagnostics change from
`deep-fold-elim` to `computation-fold-elim`.

### 11.3 Derived sequencing terminology

Historical BIND-oriented helper names describe the zero-clause specialization,
not another graph constructor. Rename them separately:

```text
compile_ref_make_bind
  -> compile_ref_make_sequence_fold

OPERATION_RUNTIME_FRAME_BIND
  -> OPERATION_RUNTIME_FRAME_SEQUENCE_FOLD

operation_runtime_discharge_bind
  -> operation_runtime_discharge_sequence_fold
```

Related comments and diagnostics should say `zero-clause computation fold` or
`computation sequencing`. Mathematical discussion may state that monadic bind
is the derived zero-clause fold.

### 11.4 Artifact compatibility invariant

The artifact writer emits numeric `term->tag` values and reads them back into
the same enum positions. The rename must therefore:

1. replace each enum member in place;
2. preserve explicit values such as obligation kind `1` and capability bits;
3. preserve all struct payloads and serialized field order;
4. preserve clause order and clause-arena indexing;
5. leave artifact version 54 unchanged;
6. pass write/read/link tests across artifacts generated before and after the
   source-level rename when their numeric schema is identical.

No alias macros from old names to new names are retained. This project does not
need source compatibility for prototype C APIs. If the implementation cannot
preserve a serialized numeric value or payload, it must stop and treat that as
a real artifact schema migration rather than silently claiming a rename.

### 11.5 Documentation policy

Do not mechanically rewrite dated historical documents. Update `README.md`,
this plan, and later authoritative documents to use `COMPUTATION_FOLD`. The
2026-07-18 generic computation-fold migration remains a historical record of
the original `DEEP_FOLD` spelling.

## 12. File Migration Map

### `src/prototype/term.h` and `src/prototype/term.c`

- rename the Core tag, payload, clause arena, capacity, and public constructor;
- rename all structural traversal, equality, hashing, substitution, relocation,
  normalization, request-forwarding, and printing paths;
- preserve the enum position and all serialized payload fields;
- print `COMPUTATION_FOLD(...)` after the migration.

### `src/prototype/judgement.h` and `src/prototype/typing.c`

- rename computation-fold constraint and proof identifiers in place;
- rename result-classifier, solver, proof-recording, and validation helpers;
- preserve proof enum values and premise ordering;
- print `computation-fold-elim` in proof diagnostics.

### `src/prototype/ast.h`

- rename OperationGraph, obligation, and runtime-capability identifiers while
  preserving their numeric values;
- replace recursive block-binding payloads with item payloads;
- add block item slice fields and result mode;
- add the block item ID arena to `prototype_ast_db`;
- add constructors for binding, expression, block, and later Lambda-exit items.

### `src/prototype/ast.c`

- rename artifact, clone/link, runtime, solver, proof, and verification paths
  from deep-fold to computation-fold;
- initialize and validate the block item arena;
- update AST inspection and structural validation;
- replace `compile_ast_block_body_ref` with indexed prefix lowering;
- centralize zero-clause computation-fold sequencing;
- preserve ContextDB/SubstitutionDB extension order;
- add Stage 2 continuation-aware Lambda-body lowering only after Stage 1 is
  stable;
- rename stale BIND-oriented helper and runtime identifiers.

### `src/prototype/reader.c`

- parse ordered item arrays;
- parse immediate block selectors;
- parse expression statements;
- remove mandatory erased `!result;` behavior;
- reserve `!term;` for Stage 2 Lambda exit;
- preserve binding scope and duplicate-name diagnostics.

### `src/prototype/repl.c` and `src/prototype/read_file.c`

- provide storage for the new AST block-item arena;
- update AST DB initialization calls and capacity diagnostics.
- report `computation-fold` for OperationGraph diagnostics.

### Other semantic consumers

- rename computation-fold shape comparison in `type_declaration.c`;
- update direct C checks in `cbpv_boundary_check.c` and
  `whnf_profile_cache_check.c`;
- update representation-sensitive output assertions in prototype shell tests.

### Prototype fixtures and scripts

- migrate block syntax with no compatibility fixtures;
- add direct, REPL, artifact write/read, and linked-artifact coverage;
- assert OperationGraph and effect-row behavior, not only printed values.

No accepted implementation file outside `src/prototype/` is modified without a
separate promotion request.

## 13. Implementation Phases

### Phase 0: Freeze semantics and baseline

- [x] Record the current commit and dirty-worktree baseline.
- [x] Run `make` and every `src/prototype/test_*.sh` test.
- [x] Generate a pre-rename artifact v54 fixture for post-rename read/link
      verification.
- [x] Confirm the target parser cases in the Section 14 test matrix; executable
      target-grammar tests are added with the Phase 2 reader implementation.
- [x] Confirm that Stage 1 contains no Lambda-exit semantics.
- [x] Confirm that handler crossing is outside initial Stage 2.

### Phase 1: Rename deep fold to computation fold

- [x] Rename TermDB tags, payloads, clause arena, constructor API, and helpers.
- [x] Rename OperationGraph tags and computation constraints.
- [x] Rename judgement proof kinds, solver APIs, and proof validation.
- [x] Rename verification obligations and runtime capability identifiers.
- [x] Rename runtime frames, forwarding, and discharge functions.
- [x] Rename artifact traversal, readback, validation, clone, and link paths.
- [x] Preserve every serialized enum value and payload field order.
- [x] Read and link the pre-rename artifact v54 fixture with the renamed build.
- [x] Change Core and proof diagnostic output to the new spelling.
- [x] Update semantic C checks and representation-sensitive shell tests.
- [x] Confirm `rg 'DEEP_FOLD|deep_fold' src/prototype` returns no stale source
      identifier or active diagnostic spelling.
- [x] Run the complete prototype and artifact test suite before block changes.
- [x] Commit or otherwise review this rename independently from later phases.

### Phase 2: AST and reader representation

- [x] Add executable parser characterization tests for every target block form.
- [x] Add AST item tags and the block item arena.
- [x] Change AST DB initialization and callers.
- [x] Replace recursive block parsing with ordered item collection.
- [x] Add `.name` selector parsing and direct-binding resolution.
- [x] Remove the mandatory erased `!result;` parser path.
- [x] Update AST inspection and validation.
- [x] Keep compilation temporarily failing clearly for unsupported new item
      forms until Phase 3 lands in the same reviewable series.

### Phase 3: Prefix and sequencing lowering

- [x] Compute the result index and semantic cutoff before graph construction.
- [x] Implement Context-forward indexed block compilation.
- [x] Lower computation bindings through zero-clause `COMPUTATION_FOLD`.
- [x] Preserve Value and raw-function aliases without unnecessary folds.
- [x] Lower intermediate computation expressions with an ignored continuation
      binder.
- [x] Implement implicit final-item result.
- [x] Implement selected-binding result.
- [x] Ensure cutoff suffixes emit no OperationGraph or constraints.
- [x] Reject intermediate Value and raw-function statements.

### Phase 4: Consolidation

- [x] Merge block sequencing and strict-value sequencing policy behind one
      helper API.
- [x] Rename BIND-oriented internal helper/runtime identifiers.
- [x] Remove recursive `BLOCK_BIND.rest` code and dead compatibility branches.
- [x] Audit comments and current design docs for authoritative terminology.
- [x] Confirm no Core, proof-kind, or artifact-format addition was introduced.

### Phase 5: Stage 1 verification

- [x] Run all prototype tests.
- [x] Run examples 01-07 and 09.
- [x] Run `training/double.p` and `training/list_nat_match.p`.
- [x] Verify direct and artifact execution agree.
- [x] Inspect generated terms to confirm sequencing is `COMPUTATION_FOLD(M, K, [])`.
- [x] Inspect OperationGraph contexts for each computation binding.
- [x] Verify selected suffix effects and constraints are absent.

### Phase 6: Lambda-exit elaboration

- [x] Add `PROTOTYPE_AST_BLOCK_LAMBDA_EXIT`.
- [x] Track the nearest source Lambda exit target during lowering.
- [x] Introduce normal/exit continuation-aware block compilation.
- [x] Extend Match branch lowering to preserve both continuations.
- [x] Reject exit across quotation and handler boundaries.
- [x] Reject exit without an enclosing source Lambda.
- [x] Confirm no Core exit node or hidden operation is emitted.

### Phase 7: Stage 2 verification

- [x] Exit from the outer Lambda body.
- [x] Exit from a nested ordinary block.
- [x] Exit from one Match branch while another continues normally.
- [x] Nested Lambda exits only its own body.
- [x] Exit under quotation is rejected.
- [x] Exit under handler source or clause is rejected.
- [x] Code after an exit path has no runtime execution on that path.
- [x] Normal block completion remains ordinary `RETURN`/`COMPUTATION_FOLD` behavior.
- [x] Artifact round-trip preserves the already elaborated graph.

## 14. Test Matrix

| Case | Expected result |
| --- | --- |
| final Value expression | implicit `RETURN(value)` |
| final returning computation | used directly as tail |
| final binding | return its bound result |
| selected Value binding | prefix plus `RETURN(value)` |
| selected computation binding | `COMPUTATION_FOLD(M, lambda x. RETURN(x), [])` |
| operation statement before result | effect retained, value discarded |
| pure Value statement before result | compile error |
| raw function statement before result | compile error |
| effectful item after selected result | absent from effect row and artifact |
| unknown or outer selector | compile error |
| duplicate binding | compile error |
| empty block | compile error |
| nested block without exit | only inner block completes |
| `!value` in nested block under Lambda | exits nearest Lambda |
| `!value` under nested Lambda | exits nested Lambda only |
| `!value` under quote | compile error |
| `!value` under handler | compile error in initial Stage 2 |

Tests must inspect both behavior and representation. A value-only assertion is
insufficient when a dead suffix could still have polluted constraints or the
artifact.

## 15. Compatibility and Migration Policy

This is prototype code. No source compatibility layer is retained.

- The transitional mandatory `!result;` grammar is removed.
- Recursive `BLOCK_BIND(value, rest)` is deleted after migration.
- No separate BIND Core or OperationGraph node is added.
- No `DEEP_FOLD` compatibility macros or duplicate constructors are retained.
- Artifact v54 remains valid only because the rename preserves numeric schema.
- Old artifacts are rejected only if a serialized schema actually changes.
- If Stage 2 later requires a Core control node, that work receives a separate
  artifact version and migration plan.

## 16. Acceptance Criteria

The terminology migration is complete only when:

1. active prototype code uses `COMPUTATION_FOLD` consistently from TermDB
   through runtime and proof validation;
2. derived sequencing remains the zero-clause form rather than a new BIND node;
3. printed Core and proof diagnostics use the new spelling;
4. artifact numeric tags and payloads are unchanged;
5. all direct, artifact, link, normalization, and runtime tests pass;
6. no compatibility aliases preserve the old C identifiers.

Stage 1 is complete only when:

1. blocks are represented by ordered AST item slices;
2. final and selected results have the specified semantics;
3. selected suffixes create no semantic graph data;
4. intermediate computations can be executed and discarded;
5. ContextDB/SubstitutionDB indexing remains valid;
6. all sequencing is represented by zero-clause `COMPUTATION_FOLD`;
7. current prototype, example, training, and artifact tests pass;
8. no compatibility parser or recursive block AST remains.

Stage 2 is complete only when:

1. `!value` means nearest-source-Lambda exit rather than erased normal return;
2. nested blocks and Match branches preserve that meaning;
3. nested Lambdas, quotation, and handlers enforce explicit boundaries;
4. no implicit global effect operation or serialized control node was added;
5. direct and artifact execution agree.

## 17. Progress Record

Append dated entries here as implementation proceeds. Record design corrections,
test commands, artifact-version decisions, and rejected approaches. Do not edit
completed checkboxes to conceal a changed design; update the relevant section
and add an entry explaining the change.

- 2026-08-04: audited current Core, OperationGraph, runtime, reader, artifact,
  and latest migration documents. Confirmed that sequencing is the zero-clause
  specialization of `COMPUTATION_FOLD`; no independent BIND representation remains.
- 2026-08-04: identified the mandatory erased `!result;` reader change as a
  transitional worktree state, not the target lexical-exit semantics.
- 2026-08-04: selected ordered AST item slices with Context-forward lowering.
  Rejected naïve reverse compilation because dependent contexts must be
  extended in source order.
- 2026-08-04: separated ordinary block results from lexical Lambda exit.
  Handler-crossing exit remains a deliberate future design gate.
- 2026-08-04: selected `COMPUTATION_FOLD` as the target constructor name.
  Audited 323 current `DEEP_FOLD`/`deep_fold` references across twelve prototype
  files. Confirmed that artifact v54 stores the affected tags numerically, so an
  in-place enum rename can preserve the wire schema without a version bump.
- 2026-08-04: Phase 0 baseline uses commit `04a9653` plus the existing
  computation-block worktree migration. `make -j2` and all seven
  `src/prototype/test_*.sh` scripts pass when scripts are invoked through `sh`.
  A pre-rename artifact v54 was written to
  `/tmp/a-program-pre-computation-fold-v54.apo`, read back successfully, and has
  SHA-256 `70a81f054fd90e8c2629efc251b1813dec8a6e7ad81cc80f6781d3d4e1357e79`.
- 2026-08-04: corrected a plan-ordering conflict. Target-grammar executable
  tests cannot join the always-green `test_*.sh` set before the Phase 2 reader
  exists, so Phase 0 freezes the test matrix and Phase 2 adds those tests with
  the implementation.
- 2026-08-04: completed Phase 1. Renamed the Core, OperationGraph, judgement,
  constraint, verification, runtime, artifact, shape-comparison, diagnostic,
  and test identifiers to `COMPUTATION_FOLD`/`computation_fold`. The old-name
  search is empty in `src/prototype`. The renamed build reads the pre-rename v54
  artifact and aggregates it without a schema bump. `make -j2`, `make reader`,
  and all seven prototype test scripts pass. The rename was reviewed as an
  isolated semantic-no-op before block representation changes.
- 2026-08-04: completed the Phase 2 ordered block AST and reader migration and
  the Phase 3 Context-forward lowering. Blocks now accept binding and
  expression items, use the final item implicitly or a direct `.name`
  selector, and sequence computations through zero-clause
  `COMPUTATION_FOLD`. A selected result is a semantic cutoff: parsed suffixes
  emit no operation, unresolved-name, constraint, or effect metadata. Added
  `test_computation_block_sequence.sh`; all eight prototype test scripts,
  examples 01-07 and 09, `training/double.p`, and
  `training/list_nat_match.p` pass. At this Phase 3 checkpoint, Stage 2
  `!value` remained reserved and was rejected explicitly.
- 2026-08-04: completed Phase 4 consolidation. Block computation bindings and
  ordinary strict value contexts now use one continuation/sequence-fold
  builder. Source binders retain deterministic scope-slot binder identity,
  AST classifier-variable identity, and binder-assumption premises; synthetic
  strict contexts continue to allocate fresh binders. This distinction is
  required for artifact relocation and provider append stability.
- 2026-08-04: completed the initial lexical Lambda-exit elaboration. `!value;`
  is represented only by `PROTOTYPE_AST_BLOCK_LAMBDA_EXIT`. A Lambda body that
  contains an exit is compiled with separate normal and exit continuations;
  Match lowering distributes those continuations into each branch. Exit paths
  elaborate to existing `RETURN`, `MATCH`, and `COMPUTATION_FOLD` nodes, so
  artifact v54 needs no new serialized control tag. Quotation, handler, and
  no-enclosing-Lambda cases are rejected. Normalization tests distinguish the
  exiting and continuing Match branches, and artifact write/read preserves the
  elaborated graph.
- 2026-08-04: all eight prototype test scripts pass after Phases 2-7. Examples
  01-07 and 09 plus `training/double.p` and `training/list_nat_match.p` compile.
  The selected-result test proves that suffix names, operations, effects, and
  constraints are absent from both direct output and artifact output.
