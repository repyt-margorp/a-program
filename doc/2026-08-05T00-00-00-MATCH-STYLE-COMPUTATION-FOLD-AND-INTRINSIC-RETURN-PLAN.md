# Match-Style Computation-Fold and Intrinsic Return Plan

Date: 2026-08-05

## 1. Status

Implemented for the currently declared operation set. Stages 1 through 5 are
complete in code. Validation with a second distinct operation identity remains
an explicit follow-up because the language currently declares only `#.print`;
this migration does not introduce a fake operation solely for testing.

Baseline verified on 2026-08-05:

- `make clean && make && make reader` passed;
- all `src/prototype/test_*.sh` scripts passed;
- implementation started from `main` commit `3f6cd54`.

Implementation progress on 2026-08-05:

- the old handler parser and singular handler AST were removed;
- `@#.return` and Match-style operation clauses lower to plural
  `COMPUTATION_FOLD` Core data;
- TermDB, OperationGraph, typing premises, runtime dispatch, graph linking,
  and artifact v55 now carry clause arrays;
- aliases dispatch and duplicate detection use nominal operation identity;
- return-only folds reuse the existing zero-clause sequencing representation;
- artifact clause contexts participate in context relocation;
- language operation-label rows and required host capabilities are separate
  declaration fields;
- the full prototype test suite passes after fixture migration.

The implementation exposed three pre-existing representation assumptions:

1. zero-clause sequencing stores its continuation in the OperationGraph
   `argument` edge, while a positive-clause fold stores its return-body
   occurrence in `scrutinee`; generic traversal must branch on clause count;
2. a clause context cannot merely be serialized: it must be included in both
   solver context relocation and artifact context slicing;
3. a host capability cannot serve as a language operation identity, because
   several nominal operations may require the same backend capability.

This document defines the migration from the current single-operation
`handle ... with ... return ...` surface form to a Match-like, multi-clause
surface form for `COMPUTATION_FOLD`.

The target spelling for the return branch is exactly `#.return`, in the
intrinsic namespace:

```text
M
	@#.return x => return_body
	@operation request continuation => operation_body;
```

`#.return` is a structural computation-fold label. It is not an effect
operation, is not performed, and must never be added to an effect row.

This plan is additive to the representation and sequencing decisions in
`2026-08-04T00-00-00-COMPUTATION-BLOCK-SEQUENCE-AND-LAMBDA-EXIT-MIGRATION.md`.
It supersedes that document only where it describes the old handler surface
syntax or a single operation clause.

No compatibility parser for the old `handle`, `with`, or handler-local
`return` keywords will remain after this migration.

## 2. Target Semantics

### 2.1 Surface form

The target grammar is conceptually:

```text
fold_term          := term fold_clause+ ";"
fold_clause        := "@" fold_label binder+ "=>" term
fold_label         := intrinsic_return_label
	                  | term_name
	intrinsic_return_label := "#.return"
```

A valid computation fold has:

- exactly one `@#.return value => body` clause;
- zero or more operation clauses;
- exactly two binders in each operation clause: request and continuation;
- no duplicate operation identity after name resolution.

For example:

```text
M
	@#.return value => value
	@#.get request continuation => continuation default_value
	@#.print text continuation => continuation text;
```

The example uses `#.get` illustratively. The current prototype defines only
`#.print` as an effect operation.

### 2.2 Alias behavior

An operation clause label is resolved as a term name before validation:

```text
output := #.print;

M
	@#.return value => value
	@output text continuation => continuation text;
```

The direct spelling and the alias must select the same language-level
operation identity. Duplicate detection therefore happens after resolution:

```text
M
	@#.return value => value
	@#.print text continuation => continuation text
	@output text continuation => continuation text;
```

must be rejected as a duplicate clause.

This migration does not make `#.return` an ordinary callable or performable
term. It is a context-sensitive intrinsic label accepted in a computation-fold
clause head. Consequently, `ret := #.return` is not part of this stage. Making
the return label aliasable would require either a first-class label type or a
separate name-binding category; silently pretending it is an effect operation
would be incorrect.

### 2.3 Core meaning

The existing Core representation is already the correct target:

```text
COMPUTATION_FOLD(
	computation,
	return_clause,
	[
		(operation_0, operation_clause_0),
		...,
		(operation_n, operation_clause_n)
	]
)
```

The return branch remains the dedicated `return_clause` field. It must not be
inserted into `computation_fold_clauses` as a fake operation.

Each operation clause body remains encoded as:

```text
LAMBDA(request, LAMBDA(continuation, operation_body))
```

The surface syntax changes how this Core graph is constructed; it does not
introduce another handler or fold node.

### 2.4 Deep forwarding

When the folded computation produces an operation request:

- a matching clause interprets the request;
- an unmatched request is forwarded with a recursively folded continuation;
- a `RETURN(value)` selects the dedicated return clause.

Clause source order is preserved for diagnostics and artifact readback, but it
must not affect operation dispatch or effect-row subtraction.

## 3. Current Codebase Findings

### 3.1 Reader syntax is singular and keyword-based

`src/prototype/reader.c:1738` implements:

```text
handle (M) with (operation) request continuation => operation_body;
return value => return_body
```

The parser requires exactly one operation and one return clause. It recognizes
`handle`, `with`, and `return` by keyword text rather than through the intrinsic
namespace.

The existing Match suffix parser at `src/prototype/reader.c:1537` accepts only
a bare identifier after `@`. It cannot currently parse either `@#.return` or
`@#.print`.

This is not fixed safely by adding a second special-case parser. Both ADT Match
and computation fold use `@`, and an operation alias such as `@output` is
syntactically indistinguishable from a constructor label until the full clause
list is known.

### 3.2 AST handler payload is singular

`src/prototype/ast.h:191` stores one:

- operation AST;
- request binder;
- continuation binder;
- operation body;
- return binder;
- return body.

`prototype_ast_handle` at `src/prototype/ast.c:12196` has the same singular API.
There is no AST arena for computation-fold clauses.

### 3.3 Intrinsic namespace has no return-label category

`src/prototype/term.h:155` and `src/prototype/term.c:133` distinguish:

- host types;
- pure primitives;
- effect operations.

The AST mirror at `src/prototype/ast.h:42` has the same three categories.
There is no binding kind for a contextual computation-fold label, and the
intrinsic table has no `return` entry.

Adding `return` as `PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION`
would be a semantic bug: the compiler would assign it an operation signature,
put it in effect rows, permit `perform #.return`, and expose it to runtime
dispatch.

### 3.4 TermDB already supports multiple clauses

`src/prototype/term.h:398` stores `first_clause` and `clause_count`, and
`prototype_term_computation_fold` at `src/prototype/term.c:3587` accepts an
arbitrary clause array.

The TermDB normalizer searches all clauses at `src/prototype/term.c:5279`.
Structural comparison, cloning, substitution, graph traversal, serialization,
and readback also iterate `clause_count`.

Therefore the migration must not replace the TermDB representation. The Core
work is limited to tightening operation-identity validation and preserving
canonical clause behavior.

### 3.5 Lowering constructs exactly one Core clause

`compile_handle_ast` at `src/prototype/ast.c:12804`:

1. compiles one operation term;
2. creates two Lambda binders for one clause body;
3. creates one `prototype_computation_fold_clause` local;
4. calls `prototype_term_computation_fold(..., &clause, 1, ...)`;
5. records one set of occurrence-local handler binders.

The Core constructor can already accept the target array, but the AST lowering
and occurrence metadata cannot produce it.

### 3.6 OperationGraph occurrence metadata is singular

`prototype_operation_node` at `src/prototype/ast.h:349` stores one set of:

- `body` and operation-label occurrence edges;
- request binder IDs;
- continuation binder IDs;
- return binder IDs.

Match already has a separate `prototype_operation_match_case` arena. A
computation fold needs an analogous, independent clause arena because each
operation clause has its own context and binder identities.

Putting all clause binders on the parent node, or recovering them later from
TermDB Lambda IDs, would lose occurrence identity when structurally equal Core
Lambdas are shared. That would repeat the same typed-occurrence/core-identity
mistake previously fixed for `identityBool` and `identityNat`.

### 3.7 OperationGraph runtime rejects multiple clauses

The operation runtime at `src/prototype/ast.c:2376` explicitly requires
`clause_count == 1`. It then compares only the first operation, binds the one
stored request/continuation pair, and enters the one stored body occurrence.

This is separate from TermDB normalization, which already searches all clauses.
The current system can therefore represent and normalize a plural fold in Core
while failing to execute the corresponding typed occurrence graph.

### 3.8 Operation solver propagates one clause

`operation_solver_propagate_clause_computation_fold_input` at
`src/prototype/ast.c:23130` reads only the first TermDB clause and the singular
OperationGraph binder fields. It supplies classifier facts only to that clause.

Without changing this path, later clauses would either remain untyped or could
accidentally reuse the first clause's context and binders.

### 3.9 Kernel constraint solver rejects multiple clauses

`solve_clause_computation_fold_constraint` at `src/prototype/typing.c:5917`
rejects any `clause_count != 1`.

For the single clause it derives:

- the input computation classifier;
- the handled operation signature;
- the residual effect row;
- the return-clause output carrier;
- request and continuation binder classifiers;
- one operation-clause result compatible with the carrier.

The algorithm must be lifted from one clause to a set of clauses sharing one
output carrier.

### 3.10 Proof verification is ahead of proof construction

`validate_computation_fold_elim_proof` at `src/prototype/typing.c:10696`
already expects:

```text
2 + 2 * clause_count
```

premises: input computation, return clause, then operation and body for every
clause.

By contrast, the solver currently constructs a fixed four-premise proof, and
the OperationGraph proof-coverage check at `src/prototype/ast.c:26150` still
requires one clause and four premises.

This is an implementation split, not a missing kernel rule. Proof construction
and occurrence coverage must be brought up to the verifier's plural model.

`PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES` is 65. With the current flat proof
shape, the explicit maximum is 31 operation clauses. The reader must enforce
that bound with a diagnostic unless proof premises are migrated to an arena.
This plan does not silently truncate clauses.

### 3.11 Effect rows conflate operation identity and host capability

The current effect operation declaration stores an `unsigned effects` bitset.
`PROTOTYPE_HOST_EFFECT_TERMINAL` is presently the only bit. The fold solver
subtracts the selected operation's effect bitset from the input row.

This works accidentally while `#.print` is the only operation. It is not a
general handler model. Two distinct operations may require the same host
capability but have different handler clauses. Dispatch must distinguish their
operation identities even if both require terminal access.

The migration must distinguish:

1. language-level operation labels used by effect rows and handlers;
2. backend/runtime capability flags used to decide whether an artifact can run.

`#.return` belongs to neither set. It is the fold's structural return branch.

### 3.12 Artifact v54 serializes singular occurrence fields

TermDB artifact data already serializes all Core fold clauses. OperationGraph
artifact data at `src/prototype/ast.c:6839` serializes only one set of handler
binders and one operation body edge. Readback at `src/prototype/ast.c:9273` and
validation at `src/prototype/ast.c:9501` enforce the same shape.

Adding a computation-fold occurrence-clause arena changes the artifact schema.
The artifact version must move from 54 to 55. Per project policy, v55 should
replace v54 readback rather than retain a compatibility branch.

## 4. Target Representation

### 4.1 Reader-local generic elimination clauses

The reader should parse an `@` chain into a temporary generic clause array:

```c
struct parsed_elimination_clause {
	int head_kind;
	uint32_t head_ast;
	int head_symbol_id;
	uint32_t first_binder;
	uint32_t binder_count;
	uint32_t body_ast;
};
```

The exact C name may differ, but the parser must retain:

- a bare label or qualified intrinsic head;
- all source binders;
- the body AST and source span;
- source order.

After the full chain is parsed:

- exactly one literal `#.return` means computation-fold syntax;
- no `#.return` means existing ADT Match syntax;
- more than one `#.return` is an error.

This avoids using type synthesis inside the reader and still permits ordinary
operation aliases in the other fold clauses.

The `#.return` marker must be literal in this stage. This makes parsing
deterministic without giving an untyped, first-class meaning to a return label.

### 4.2 Plural computation-fold AST

Rename `PROTOTYPE_AST_HANDLE` to `PROTOTYPE_AST_COMPUTATION_FOLD` and replace its
singular operation payload with:

```c
struct prototype_ast_computation_fold_clause {
	uint32_t operation;
	uint32_t operation_argument_binder_id;
	int operation_argument_symbol_id;
	uint32_t operation_continuation_binder_id;
	int operation_continuation_symbol_id;
	uint32_t body;
	struct prototype_source_span span;
};
```

The parent AST node stores:

- computation AST;
- `first_clause` and `clause_count`;
- one return binder and return body.

Add a dedicated clause arena to `prototype_ast_db`. Do not reuse Match cases:
Match cases carry constructor ordinals and recursive-field binder semantics,
while computation-fold clauses carry operation identities and resumptions.

### 4.3 Intrinsic namespace marker

Add a lookup category such as:

```text
PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN
```

and register the source spelling `return` in `intrinsic_namespace`.

The reader consumes that binding only in an elimination-clause head. Ordinary
term lowering must reject it. No `PROTOTYPE_TERM_EFFECT_OPERATION` is created,
and no host oracle or effect declaration is assigned.

This is a namespace entry with contextual syntax meaning, not a new Core
computation constructor. The existing `PROTOTYPE_TERM_RETURN(value)` remains
the actual effect-tree injection.

### 4.4 OperationGraph clause arena

Add a structure such as:

```c
struct prototype_operation_computation_fold_clause {
	uint32_t operation_operation;
	uint32_t body_operation;
	uint32_t context_id;
	uint32_t argument_ast_binder_id;
	uint32_t argument_binder_id;
	uint32_t continuation_ast_binder_id;
	uint32_t continuation_binder_id;
};
```

Add its arena and capacity/count to `prototype_operation_graph`. The parent
fold occurrence stores `first_fold_clause` and `fold_clause_count`, plus the
existing return-body occurrence and return binder.

The old singular request/continuation fields are removed. They must not remain
as a compatibility cache because two authoritative representations would then
exist.

### 4.5 Operation identity

Runtime dispatch and duplicate detection need one explicit query that extracts
the nominal language-level operation identity from a resolved clause-head term.
They must not use arbitrary structural equality of operation signatures.

Two aliases of one operation have one identity. Two separately declared
operations with identical request/result classifiers have different identities.

The identity may initially be the existing effect-operation declaration ID,
but the API must not expose host capability bits as operation identity.

## 5. Typing Rule

Assume:

```text
M       : Comp(E, A)
return  : Pi(A, Comp(F, C))
op_i    : Pi(Request_i, Comp(H_i, Result_i))
clause_i:
	Pi(Request_i,
		Pi(Thunk(Pi(Result_i, Comp(F union R, C))),
			Comp(F, C)))
```

where `R` is the unhandled residual row and the handled operation-label set is
`H = {op_0, ..., op_n}`. Then:

```text
E = H union R
-----------------------------------------------
COMPUTATION_FOLD(M, return, clauses)
	: Comp(F union R, C)
```

The implementation sequence is:

1. synthesize the input computation classifier;
2. synthesize the return clause and establish the common output carrier;
3. resolve and validate every operation identity;
4. form the handled label set without regard to source order;
5. solve one residual-row equation for the whole set;
6. type every request binder from its operation domain;
7. type every continuation from its operation result and common carrier;
8. require every clause body to normalize to the common output computation;
9. publish one fold-elimination proof with all clause premises.

Dependent handler-result obligations remain subject to the existing staged
verification policy. Plural clauses do not justify closing an output family
whose index is available only after an effectful computation runs.

## 6. Effect Representation Prerequisite

Before adding a second operation that shares a host capability, introduce an
operation-label row distinct from runtime capabilities.

Minimum target split:

```text
operation effects: set/row of nominal operation IDs
runtime capabilities: bitset of backend facilities
```

For example, `Terminal.write` and `Terminal.read` may be separate operation
labels while both require `PROTOTYPE_HOST_EFFECT_TERMINAL` at runtime.

The fold removes handled operation labels. Artifact capability collection scans
the residual executable graph and computes required backend facilities from the
remaining operation declarations. It must not infer handler identity from the
capability bitset.

This split affects:

- `prototype_effect_operation_declaration` in `term.h`;
- effect-label and row constructors in `term.c`;
- residual/union equations in `typing.c`;
- `compile_metadata_refresh_runtime_capabilities` in `ast.c`;
- artifact interface and operation declaration validation;
- runtime dispatch and capability checks.

## 7. File-by-File Change Plan

### 7.1 `src/prototype/reader.c`

- replace `parse_handle_term` with generic `@`-clause parsing;
- accept qualified intrinsic heads after `@`;
- buffer clause binders and bodies before selecting Match or fold syntax;
- detect exactly one literal `#.return` for a computation fold;
- validate one return binder and two operation binders;
- remove `handle`, `with`, and handler-local `return` parsing;
- preserve the separate `perform` migration until operation invocation syntax
  is decided; do not conflate this work with request construction;
- report duplicate return, missing return, malformed binder count, and excess
  clause-count errors at source spans.

### 7.2 `src/prototype/ast.h` and `src/prototype/ast.c`

- rename the handler AST tag and constructor to computation-fold terminology;
- add the plural AST clause arena;
- compile each clause under its own nested request/continuation context;
- build one Core clause array and call `prototype_term_computation_fold` once;
- retain one return clause and return occurrence;
- update AST validation, traversal, source diagnostics, capacity checks, and
  compile dispatch;
- remove all singular AST operation-clause fields.

### 7.3 `src/prototype/term.h` and `src/prototype/term.c`

- add the contextual intrinsic namespace binding kind for `#.return`;
- do not add an effect-operation ID for return;
- expose a nominal operation-identity query;
- use that query for duplicate clause detection;
- preserve the existing plural `COMPUTATION_FOLD` payload and normalizer;
- separate operation-label effects from host capability flags before multiple
  operations can share one capability.

### 7.4 OperationGraph in `src/prototype/ast.h` and `src/prototype/ast.c`

- add the computation-fold occurrence-clause arena;
- add graph allocation, getters, append, validation, traversal, slicing,
  copying, relocation, merge, and capacity handling;
- update the fixed-point solver to propagate request and continuation facts for
  every clause occurrence;
- update proof-coverage validation to compare all Core clauses with all
  occurrence clauses;
- remove singular handler binder fields from `prototype_operation_node`.

### 7.5 `src/prototype/typing.c`

- remove the `clause_count != 1` rejection;
- derive the common output classifier from the return clause once;
- iterate every operation clause and construct `2 + 2*n` proof premises;
- solve residual effects from the set of handled operation identities;
- reject duplicate identities before solving;
- enforce the current 31-operation proof limit explicitly, or migrate proof
  premise storage before raising it;
- update dependent-result obligation generation for plural clause occurrences;
- retain normalization equality as the compatibility test for clause result
  classifiers.

### 7.6 Operation runtime in `src/prototype/ast.c`

- remove the `clause_count == 1` requirement;
- locate the matching Core clause by nominal operation identity;
- locate the corresponding occurrence-clause metadata by the same index;
- extend the environment with that clause's request and resumption binders;
- evaluate that clause's body occurrence;
- forward unmatched requests unchanged except for recursively folding their
  continuation;
- retain the dedicated return path and return binder.

### 7.7 Artifact read/write in `src/prototype/ast.c`

- bump `A_PROGRAM_ARTIFACT` from 54 to 55;
- add an `operation_fold_clauses` section containing occurrence edges, context,
  and binder identities;
- serialize parent `first_fold_clause` and `fold_clause_count`;
- remove singular handler request/continuation fields from operation records;
- validate one-to-one correspondence with Core fold clauses;
- relocate clause operation IDs and contexts during artifact merge/link;
- reject v54 instead of retaining compatibility readback;
- retain the existing plural TermDB fold-clause section.

### 7.8 Tests and fixtures

Migrate old handler syntax in:

- `src/prototype/test_cbpv_surface.sh`;
- dependent handler-result fixtures;
- artifact round-trip and runtime fixtures.

Add permanent tests for:

1. one operation clause, equivalent to the old syntax;
2. zero operation clauses plus `@#.return`;
3. two distinct operation clauses;
4. an alias such as `output := #.print` used as a clause head;
5. duplicate direct/alias operation rejection;
6. duplicate `@#.return` rejection;
7. missing `@#.return` interpreted as ADT Match or rejected by later typing;
8. `perform #.return` rejection;
9. `#.return` exclusion from effect rows and runtime capabilities;
10. unmatched operation deep forwarding;
11. source-order independence of handled-effect calculation;
12. correct request/continuation binder selection for the second clause;
13. plural proof verification and proof-coverage checking;
14. artifact v55 write/read/link round-trip;
15. v54 rejection;
16. nested folds with different handled operation sets;
17. dependent result residual-obligation preservation.

The plural-runtime test requires at least two real operation identities. Add a
second prototype operation only with an explicit signature and runtime oracle;
do not model it by reusing the `print` identity with a different name.

## 8. Implementation Stages

### Stage 1: Surface and AST

- [x] Add reader-local generic elimination clauses.
- [x] Add contextual intrinsic lookup for `#.return`.
- [x] Add plural computation-fold AST storage.
- [x] Lower the new syntax to existing plural TermDB Core.
- [x] Remove old handler keywords and AST payload.
- [x] Add parser and lowering tests.

Exit condition: a plural fold reaches TermDB with the expected return clause and
operation array, without requiring typing or execution success.

### Stage 2: Typed occurrence graph

- [x] Add OperationGraph fold-clause arena.
- [x] Preserve each clause context and binder identity.
- [x] Update graph traversal, validation, and fixed-point propagation.
- [x] Remove singular operation-clause occurrence fields.

Exit condition: each plural Core clause has exactly one typed source occurrence
with independent request and continuation binders.

### Stage 3: Kernel typing and effects

- [x] Generalize fold constraint solving to all clauses.
- [x] Construct plural proof premises.
- [x] Generalize proof-coverage validation.
- [x] Split nominal operation rows from host capabilities at the declaration boundary.
- [x] Preserve dependent-result residual obligations.

Exit condition: the kernel accepts valid plural folds, rejects duplicate or
incompatible clauses, and computes residual effects independent of clause order.

### Stage 4: Runtime

- [x] Dispatch over all operation clauses.
- [x] Bind the selected clause's occurrence-local environment.
- [x] Forward unmatched requests deeply.
- [ ] Verify second-clause and nested multi-operation resumptions after a second
  real operation is specified.

Exit condition: the second and later clauses execute correctly, and unmatched
requests remain observable by an enclosing fold or host handler.

### Stage 5: Artifact v55

- [x] Serialize and read the occurrence-clause arena.
- [x] Relocate and merge plural clauses.
- [x] Validate Core/OperationGraph correspondence.
- [x] Remove v54 compatibility.
- [x] Add direct and readback execution tests; linked plural execution awaits a
  second real operation identity.

Exit condition: source compilation and artifact readback produce equivalent
normalization, typing, and runtime results for plural folds.

## 9. Non-Goals

This migration does not:

- make `#.return` a user-performable operation;
- make the return label aliasable;
- replace `RETURN(value)` with an effect operation;
- merge ADT Match and computation fold in Core;
- add higher-order operations or scoped handlers;
- settle the final surface spelling for operation requests;
- promote user proofs into global kernel conversion;
- make runtime capability bits the identity of language operations.

## 10. Acceptance Invariants

The implementation is complete only when all of the following hold:

1. `@#.return` is resolved through the intrinsic namespace and remains outside
   effect rows.
2. Operation aliases dispatch by resolved nominal identity.
3. TermDB has one authoritative plural fold representation.
4. OperationGraph has one occurrence record per clause and no singular cache.
5. Typing, proof construction, proof validation, normalization, runtime, and
   artifacts all accept the same clause count.
6. Clause ordering changes diagnostics/readback order only, not semantics.
7. Distinct operations with equal signatures remain distinct.
8. Distinct operations sharing a host capability remain separately handleable.
9. Unhandled requests are forwarded with their continuation under the fold.
10. v55 artifacts preserve every clause context and binder identity.
11. No old handler syntax or v54 compatibility path remains.

## 11. Principal Risks

The largest risk is not the TermDB constructor. It is keeping four identities
separate while lowering and linking:

- source label spelling;
- resolved language operation identity;
- typed occurrence and binder identity;
- backend capability requirement.

Collapsing any two of these recreates known failures: aliases stop matching,
equal signatures become the same operation, shared Core Lambdas capture the
wrong typed binder, or handling one terminal operation incorrectly removes all
terminal effects.

The second risk is implementing the new parser as an isolated `@#.` special
case. That would work for direct intrinsic names but fail for aliases and would
leave two competing `@` grammars. The reader-local generic clause pass is the
required boundary: syntax is collected first, then elaborated as ADT Match or
computation fold without asking the parser to solve types.

## 12. Final Verification

Verified on 2026-08-05:

- `make clean && make && make reader`;
- reader build with `-Wall -Wextra -Werror`;
- every `src/prototype/test_*.sh` script;
- `examples/01_bool.p` through `examples/07_add.p`;
- `examples/09_list_induction.p`;
- direct alias dispatch through `output := #.print`;
- duplicate direct/alias operation rejection;
- duplicate `@#.return` and `perform (#.return ...)` rejection;
- return-only fold lowering;
- v55 computation-fold artifact write/read.

No `examples/08*.p` exists in the repository. A true two-operation runtime and
linked-artifact test must be added together with the next real operation
declaration or with user-defined operation declarations. The plural Core,
typed occurrence, proof, runtime, and artifact paths are implemented now; the
missing test reflects the operation vocabulary, not a retained single-clause
code path.
