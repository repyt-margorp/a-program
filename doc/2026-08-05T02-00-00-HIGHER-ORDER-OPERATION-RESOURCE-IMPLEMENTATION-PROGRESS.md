# Higher-Order Operation and Resource Implementation Progress

Date: 2026-08-05

## 1. Purpose

This is the implementation and progress record for the decisions in:

- `2026-08-05T01-00-00-THUNK-ENCODED-HIGHER-ORDER-OPERATIONS.md`

The referenced document is the semantic decision record.  This document owns:

- concrete code changes;
- migration order;
- completion gates;
- test evidence;
- deviations discovered during implementation;
- decisions revised after contact with the implementation.

It must be updated while the migration proceeds.  A checked item requires
direct code or test evidence recorded in this file.

## 2. Baseline

Implementation baseline inspected on 2026-08-05:

```text
branch: main
commit: 8afe93c generalize computation folds to match-style clauses
```

Uncommitted baseline documentation:

```text
doc/2026-08-05T01-00-00-THUNK-ENCODED-HIGHER-ORDER-OPERATIONS.md
```

Current relevant implementation facts:

- `&M` parses as `PROTOTYPE_AST_QUOTE` and lowers to `THUNK(M)`;
- `OPERATION_REQUEST` already stores an arbitrary Term argument and a thunked
  outer continuation;
- `COMPUTATION_FOLD` and its source occurrence carry plural operation clauses;
- deep folding rebuilds the outer continuation but treats thunks in request
  arguments as opaque values;
- operation signatures are static host-type metadata with maximum arity one;
- only `#.print` is currently declared as an effect operation;
- runtime environments bind source binders to TermDB IDs or resumption IDs;
- the runtime has no resource-value domain, resource table, finalizer frame,
  region binder, or usage multiplicity checker.

## 3. Non-Negotiable Invariants

1. Do not add a second higher-order operation Core node.  Quoted computations
   remain ordinary operation arguments.
2. Do not restore singular handler syntax.  Match-style plural
   `COMPUTATION_FOLD` remains authoritative.
3. Do not put a live host resource into TermDB or an artifact.
4. Do not infer ownership or one-shot use from an effect-row label.
5. Do not perform host operations during normalization or kernel conversion.
6. Keep operation identity, language effect labels, and backend capabilities
   distinct.
7. Keep implementation changes under `src/prototype/` until separately
   promoted.
8. Do not retain compatibility paths for replaced prototype representations.

## 4. Migration Overview

| Stage | Goal | Status |
|---|---|---|
| 0 | Freeze implementation plan and baseline evidence | complete |
| 1 | Make graph-level classifiers authoritative for operations | complete |
| 2 | Validate thunk-encoded higher-order operation requests | partial, opaque handling proven |
| 3 | Add inner-policy and resumption-multiplicity contracts | complete for direct use |
| 4 | Separate runtime values from TermDB and add resource scopes | partial: tagged values introduced |
| 5 | Add File scoped borrowing and finalization | pending |
| 6 | Add region non-escape and required multiplicity checks | pending |
| 7 | Validate direct, runtime, artifact, and backend boundaries | pending |
| 8 | Commit and push the completed main branch | pending |

## 5. Stage 1: Authoritative Graph-Level Operation Classifiers

### 5.1 Problem

`prototype_effect_operation_declaration` currently stores:

```text
arity
argument_types[]
result_type
operation_labels
required_host_effects
```

`prototype_judgement_effect_operation_classifier` reconstructs a Pi/Comp graph
from the host-type IDs.  This prevents operation domains from containing user
ADTs, thunks, dependent families, open effect rows, regions, or multiplicity
information.

### 5.2 Target representation

The static declaration keeps only stable schema and dispatch metadata:

```text
operation identity
classifier-schema kind
logical arity
language operation labels
required runtime capabilities
inner-computation policy
resumption multiplicity
host implementation identity
```

The classifier schema is materialized into each TermDB as an ordinary graph.
Each `PROTOTYPE_TERM_EFFECT_OPERATION` node directly points to that classifier.
There is no side cache: the edge is part of graph identity, artifact
reachability, validation, and relocation.  Typing reads this edge and does not
independently reconstruct a second classifier.

Initial schema kinds may be builtin constructors implemented in C.  They are
not the final public operation declaration language.  Their output is still an
authoritative Term graph and exercises the same typing path required by future
source or artifact declarations.

### 5.3 Work items

- [x] Replace host argument/result arrays as typing authority.
- [x] Add classifier schema kinds for current `#.print` and a thunk-operation
      test declaration.
- [x] Store the authoritative classifier as an operation-node graph edge.
- [x] Make all typing callers read the graph classifier from one API.
- [x] Store only a dispatch arity; typing does not use it.
- [ ] Validate the dispatch arity against the classifier Pi spine when the
      classifier schema is materialized.
- [ ] Validate that a declaration's final codomain is a returning computation.
- [x] Preserve nominal operation labels and host capability mapping.
- [x] Include graph-level operation classifiers in artifact reachability and
      validation where they become interface dependencies.
- [x] Remove the old host-signature classifier constructor for operations.

### 5.4 Completion evidence

- `#.print` retains its existing classifier and behavior.
- a declared test operation accepts `Thunk(Comp(E,A))` through the ordinary APP
  and operation-request path;
- direct and artifact readback expose the same operation classifier graph;
- malformed classifier schemas fail before runtime dispatch.

## 6. Stage 2: Thunk-Encoded Higher-Order Operation Tests

Introduce a non-resource operation before File handling.  Its purpose is to
separate higher-order representation from resource safety.

The operation now supports a request containing a quoted computation.  The
remaining handler tests depend on effect-row-forall specialization described
below:

- [x] discard without forcing;
- [ ] force exactly once;
- [ ] force more than once for an unrestricted thunk;
- [ ] explicitly place a computation fold around the inner computation;
- [ ] forward an unknown request while preserving the thunk unchanged;
- [ ] keep the thunk's effect row latent before force;
- [ ] include the inner row when a clause forces the thunk.

Completed request-level evidence:

- [x] `#.scope_text` accepts `&(perform (#.print ...))` as an ordinary argument;
- [x] the resulting graph contains `THUNK(OPERATION_REQUEST(...))`;
- [x] the inner print row remains latent in the thunk classifier;
- [x] artifact v57 preserves operation classifiers and fold wrapper edges.
- [x] a `#.scope_text` clause receives the quoted computation as an ordinary
      argument and can discard it;
- [x] discarding the argument leaves the nested `#.print` unexecuted;
- [x] the handler result is `RETURN(TEXT_LITERAL("handled"))`;
- [x] handler typing strips classifier-only `EFFECT_ROW_FORALL` binders before
      reading the operation Pi domain;
- [x] an unused operation-clause argument still receives the operation domain;
      variable occurrence is usage evidence, not typing authority;
- [x] the handler and its proof graph survive artifact write/read.
- [x] rebuild clause-bearing fold premises after dependency-closed removal of
      provisional handler derivations;

No special higher-order surface syntax or Core node may appear in this stage.

### 6.1 Precisely established capability

The implementation now establishes the following, and no stronger claim:

```text
op : Thunk(Comp(E, A)) -> Comp({op}, B)
M  : Comp(E, A)
---------------------------------------
perform (op &M) : Comp({op}, B)
```

The handler receives `&M` as an opaque value.  It may discard that value,
retain it, or use the language's ordinary force path where its clause type
permits that use.  Deep folding the outer request does not inspect the thunk.

This representation is sufficient for a higher-order operation request.  It
is not by itself a complete higher-order handler semantics.  In particular,
the following remain separate obligations:

- whether a scoped clause handles effects inside `M`;
- whether forwarding an unknown request recursively transforms `M`;
- how latent and handled effect rows are related after force;
- whether the resumption or the inner thunk is one-shot;
- how resource ownership survives capture, forwarding, and scope exit.

Treating these as one boolean `is_higher_order` flag would lose the semantic
distinctions.  They remain explicit policy and typing work.

### 6.2 Implementation defects found while adding the handler

Two defects were exposed:

1. Handler propagation attempted to read an operation classifier directly as
   Pi.  A polymorphic operation classifier begins with `EFFECT_ROW_FORALL`, so
   classifier-only row binders must be exposed before Pi decomposition.
2. Clause argument typing was inferred only from variable occurrences.  An
   unused argument consequently had no classifier.  Its type is determined by
   the operation domain regardless of use count.

The proof cleanup path also exposed a broader debt.  Temporary lambda/app/IH
derivations are removed before handler reconstruction.  A fold proof that uses
those lambdas must be removed and rebuilt in the same unit; retaining it leaves
dangling premise IDs.  The current implementation therefore drops clause-bearing
`COMPUTATION_FOLD_ELIM` proofs together with those structural derivations.
Zero-clause sequencing folds remain intact; they do not contain clause lambda
premises and are not part of handler reconstruction.

This is not yet a general proof-DAG garbage collector.  A fully transitive
proof removal exposed that `operation_solver_reify_core_proof` cannot recreate
every context-local proof from scratch.  General dependency-closed compaction
is deferred until the reifier contract is changed to:

```text
0 = the exact proof exists in the requested context
1 = deferred until another fixed-point round
-1 = invalid
```

That work must be completed before temporary-proof cleanup is generalized.

## 7. Stage 3: Handling Contracts

### 7.1 Inner-computation policy

Add an operation declaration policy with an explicit initial set:

```text
OPAQUE
SCOPED_EXPLICIT
```

`OPAQUE` means generic forwarding preserves all thunk request fields unchanged
and folds only the outer continuation.  `SCOPED_EXPLICIT` means a clause is
required to interpret the inner computation; it still does not authorize the
runtime to search arbitrary ADT fields for thunks.

Automatic recursive descent into operation arguments is intentionally excluded
until a typed request-field traversal contract exists.

### 7.2 Resumption multiplicity

Add declaration metadata and runtime enforcement for:

```text
ONE_SHOT
MULTI_SHOT
ABORTIVE
```

Work items:

- [x] type each clause continuation independently of whether it is used;
- [x] reject direct continuation use for `ABORTIVE` operations;
- [x] reject more than one direct source use for a one-shot continuation;
- [x] add a consumed flag to runtime resumptions as a dynamic backstop;
- [x] retain unrestricted direct-use behavior only for `MULTI_SHOT`;
- [x] test zero, one, and two direct uses under the three policies.

The static check counts source OperationGraph edges, not shared Core TermDB
nodes.  It saturates at two because the current contracts distinguish zero,
one, and more-than-one use.  This is intentionally not claimed as a complete
linear type system: capturing a continuation in another thunk and duplicating
that thunk requires usage-aware value typing.  The runtime consumed flag
remains the defensive one-shot check for paths the direct counter cannot prove.

## 8. Stage 4: Runtime Value and Resource Separation

### 8.1 Runtime values

Replace term-only runtime bindings with a tagged value domain:

```text
TERM(term_id)
RESOURCE(resource_slot, generation)
RESUMPTION(resumption_id)
```

Resumptions may remain a distinct binding kind internally, but a generic
runtime value must not overload all three as an untagged `uint32_t`.

Implementation status:

- [x] introduce one tagged runtime-value representation for terms, resource
      references, and resumptions;
- [x] store the tagged value in runtime environments;
- [x] carry tagged values as the operation-machine result and request argument;
- [x] restrict TermDB binder substitution to `TERM` values;
- [x] allow lambda and continuation entry to bind a generic runtime value;
- [x] retain the public TermID evaluation API as an adapter which succeeds only
      when the final runtime result is `TERM`;
- [ ] permit `RESOURCE` arguments to cross a perform boundary without building
      a TermDB `OPERATION_REQUEST`;
- [ ] expose a resource-valued evaluation API to runtime backends.

The first five completed items are a representation migration, not a claim
that resources are executable.  The existing operation request reducer and
host dispatcher still consume TermDB arguments.  A resource must not be
encoded as a synthetic Term merely to satisfy those interfaces; Stage 4.2 and
the runtime dispatch boundary must be completed instead.

### 8.2 Resource table

Add runtime-only entries containing at least:

```text
kind
generation
state
host payload
finalizer
```

Term substitution remains available only for `TERM`.  Applying a continuation
to a `RESOURCE` value must bind the runtime value in the occurrence machine; it
must not materialize a resource term in TermDB.

### 8.3 Resource scopes

Add a finalizer/scope frame which records acquired resources.  Unwind it on:

- normal `RETURN`;
- lambda exit lowered from `!`;
- abortive handled operation;
- runtime failure after acquisition;
- future cancellation entry points.

Finalization must be idempotent in the runtime as a defensive check, while the
type system proves the stronger exactly-once protocol where possible.

## 9. Stage 5: File Scoped Borrowing

The first public File interface is scoped.  Raw ownership-returning `open` and
manual `close` are not exposed in this stage.

Conceptual interface:

```text
with_file : forall E A.
	Path ->
	Mode ->
	(forall r. Thunk(Pi(FileBorrow(r), Comp(E, A)))) ->
	Comp(E union {Filesystem}, Result(OpenError, A))
```

Required operations and runtime capabilities are separate:

```text
language operations: Filesystem.with_file, Filesystem.read
runtime capability:  FILESYSTEM
```

Work items:

- [ ] introduce nominal Path/Mode/request/result classifiers;
- [ ] introduce an opaque runtime `FileBorrow(r)` capability;
- [ ] add filesystem operation identities and effect labels;
- [ ] add filesystem runtime capability checks;
- [ ] install a File finalizer only after successful acquisition;
- [ ] close on every supported exit path;
- [ ] reject File operations in pure normalization and conversion;
- [ ] reject filesystem artifacts on the Verilog backend;
- [ ] permit C/interpreter backends only when filesystem capability is present;
- [ ] verify no resource payload is written into an artifact.

## 10. Stage 6: Region and Multiplicity Checks

Only the usage discipline needed by the scoped File interface is introduced
here.  It must nevertheless use a general occurrence-level representation so
that it can later extend to other resources.

### 10.1 Region non-escape

- [ ] allocate an occurrence-scoped region binder for each scoped resource;
- [ ] record the region in the borrowed resource classifier;
- [ ] reject a fold result, returned ADT, thunk, or exported classifier which
      contains the region binder;
- [ ] allow ordinary repeated reads within the region when the borrow policy is
      unrestricted;
- [ ] prevent retention of the borrow by an unrestricted closure or thunk that
      can outlive the region.

### 10.2 Ownership and continuation usage

- [ ] represent at least zero, one, and unrestricted usage in Context/Judgement
      constraints;
- [ ] keep usage out of TermDB canonical identity;
- [ ] check APP, computation fold, Match branches, and thunk capture;
- [ ] require one-shot continuation behavior for resource scopes;
- [ ] record residual usage obligations under hybrid compile policy rather than
      inventing a successful proof.

## 11. Stage 7: Test and Artifact Matrix

Every row requires direct and artifact-backed evidence where applicable.

| Area | Required tests | Status |
|---|---|---|
| Existing examples 01-09 | compile as before | passed 2026-08-05 |
| Existing prototype suite | every `test_*.sh` passes | passed 2026-08-05 |
| Thunk operation | discard, once, repeated, explicit inner fold | pending |
| Forwarding | unknown thunk request preserves argument | pending |
| Effect rows | latent before force, union after force | pending |
| Resumptions | direct one-shot replay rejected; multi-shot replay returns to clause | passed |
| File success | read and close exactly once | pending |
| File acquisition failure | no finalizer on failed acquisition | pending |
| Lambda exit | File finalizer runs before exit reaches caller | pending |
| Handler abort | File finalizer runs before abort leaves scope | pending |
| Region escape | returned/stored/thunk-captured borrow rejected | pending |
| Artifact | operation classifiers and fold wrapper edges survive readback | passed for v57 |
| Backend | unsupported filesystem capability rejected | pending |

## 12. Progress Log

### 2026-08-05: Plan created

- Inspected `main` at `8afe93c`.
- Confirmed plural operation clauses are implemented through reader, AST,
  OperationGraph, TermDB, typing, runtime, and artifact paths.
- Confirmed `&M` and arbitrary operation request arguments already provide the
  representation needed for computation parameters.
- Identified the first concrete blocker as the host-only operation signature in
  `term.h` and `typing.c`, not operation syntax.
- Identified runtime term-only bindings as the first concrete blocker for live
  resources.
- No implementation stage is marked complete yet.

### 2026-08-05: Tagged runtime-value boundary introduced

- Replaced the runtime environment's independent `kind` and untyped `uint32_t`
  payload with a single tagged value used by bindings, machine results, lambda
  entry, resumptions, and pending request arguments.
- Term instantiation now substitutes only `TERM(term_id)` bindings.  Resource
  and resumption identities cannot be inserted into TermDB by that path.
- Kept `prototype_operation_evaluate_with_trace` as a Term-result adapter so
  existing callers retain their contract while the internal machine is no
  longer term-only.
- Confirmed the change with clean builds, the resumption multiplicity runtime
  tests, and the artifact flow tests.
- Discovered the next precise boundary: `PERFORM_ARGUMENT` still materializes
  a TermDB request and the host dispatch callback accepts only Term IDs.
  Resource-bearing requests must bypass that representation without weakening
  the invariant that artifacts contain no live resource.

### 2026-08-05: Graph classifier milestone

- Replaced effect-operation host argument/result arrays with classifier schema,
  policy, and checked-arity metadata.
- Added the nominal `#.scope_text` operation with classifier
  `forall E. Thunk(Comp(E, Text)) -> Comp({scope_text}, Text)`.
- Made the classifier an authoritative child edge of each effect-operation
  TermDB node; structural comparison and canonical hashing include the edge.
- Changed artifact format from v55 to v56.  Writer, reader, sparse-slot
  validation, graph marking, and append relocation preserve the edge.
- Changed effect-operation typing and proof validation to use the stored graph
  classifier rather than rebuilding a host signature.
- Added `higher_order_operation_check.p` and direct/artifact regression checks.
- Added runtime one-shot/abortive resumption checks as a dynamic backstop.
- Existing CBPV boundary tests passed after the graph change.  The complete
  prototype script suite, artifact flow, and examples 01-09 also pass.

### 2026-08-05: Direct resumption multiplicity milestone

- Corrected `#.scope_text` from `INNER_SCOPED` to `INNER_OPAQUE`; the current
  deep fold treats its thunk argument opaquely and must not claim scoped inner
  interpretation.
- Added nominal one-shot and abortive test operations with separate effect
  labels and the same thunk-argument classifier schema.
- Derived the clause continuation classifier from the operation result and
  handler return computation even when the continuation binder is unused.
- Added a source OperationGraph continuation-use counter.  Multi-shot accepts
  two direct uses, one-shot accepts one and rejects two, and abortive accepts
  zero and rejects one.
- Added a runtime resumption-return state.  Invoking a captured continuation
  now restores the handler-clause environment, handler stack, and reduction
  options after the captured continuation returns.  This makes sequential
  multi-shot resumption return to the clause before the second use.
- Added `test_resumption_multiplicity.sh`; it covers direct compilation,
  negative static checks, runtime results, and artifact readback.

## 13. Deviations and Revised Decisions

Record every implementation-driven design change here before marking the
affected stage complete.

### 2026-08-05: Operation node edge replaces the proposed cache

The plan initially proposed a TermDB-side classifier cache.  Implementation
review showed that a cache would create another authority and would not
naturally participate in artifact reachability.  The operation node now owns a
classifier edge instead.

### 2026-08-05: Higher-order handling blocker refined and resolved

The request `perform (#.scope_text &M)` originally built and type-checked while
its fold clause failed before clause solving.  The implementation now exposes
classifier-only effect-row quantifiers before Pi decomposition, types unused
operation and continuation binders from the handler algebra, and validates the
resulting handler and artifact.

This confirmed that the failure was not a representation failure and did not
justify a new higher-order operation node.  Discard, one direct resumption, and
repeated multi-shot resumption now pass.  Explicit scoped interpretation and
unknown-operation forwarding remain Stage 2 work.

### 2026-08-05: Inner-force probe exposed proof lifecycle debt

A clause which merely receives and discards `&M` is not sufficient evidence
that the clause can execute `M`.  A dedicated force probe exposed two general
proof-DAG defects:

1. RETURN/THUNK/FORCE proof construction omitted context reindexing for a child
   proof inherited from the handler argument context.
2. Reindex lookup accepted provisional APP/Lambda/IH derivations which are
   deliberately removed during handler reconstruction, leaving durable CBPV
   proofs with missing premises.

A local lookup repair made the simple ancestor-context probe reach proof
validation, but regressed ordinary computation blocks because a reindex proof
could itself cite a provisional proof.  That lookup repair was reverted.
Temporary-proof cleanup now removes every derivation whose premise tuple is no
longer provided by a surviving relation, and clears stale proof IDs before
reification.  Examples 01-09, artifact flow, and resumption tests pass with
this dependency-closed cleanup.

The full CBPV surface script still fails where it requires a
`computation-fold-elim` proof for a clause-bearing handler.  This is the known
gap in clause-fold reification, not a runtime failure.  Stage 2's force-once,
force-repeatedly, and explicit-inner-fold boxes remain unchecked.  The source
probe also established that `inner := delayed` merely binds the thunk, while
the removed `#.force` syntax has no complete replacement for this clause use.
The remaining issue is therefore proof reification plus surface elaboration
and classifier solving, not Core representation.

### 2026-08-05: Higher-order capability reassessed by independent contracts

The implementation and the primary scoped-effect literature were reviewed
again after the claim that a thunk argument might already provide the required
higher-order structure.  The claim is partly correct and changes the problem
statement:

- `&M` already supplies the representation of an internal computation;
- the operation request already keeps that computation distinct from its outer
  continuation;
- plural operation clauses are already implemented and are unrelated to
  whether an individual request is higher-order;
- no separate higher-order operation Core tag is justified by the current
  evidence.

What remains is not "make operations accept computations".  It is to complete
and validate the contracts for inner scheduling, recursive inner handling,
unknown-operation forwarding, usage, and resource lifetime.  The current
runtime's generic deep fold handles the outer continuation and preserves a
thunk argument opaquely.  That is a coherent default, but not a complete scoped
forwarding semantics.

The semantic decision document now records a nine-axis capability matrix and
the following implementation order:

1. repair clause-bearing fold proof reification;
2. validate plural proofs and artifact readback;
3. provide an unambiguous surface computation-demand path for inner force;
4. separately test discard, once, repeated, and explicit inner fold;
5. decide modular forwarding only after those paths are sound;
6. add resource lifetime machinery after the runtime perform boundary supports
   non-Term values.

This supersedes any earlier diagnosis that the present surface syntax accepts
only one operation clause.  It also prevents the opposite overstatement that
successful `&M` request construction proves full higher-order handling.

Code inspection refined item 1 further.  The Core proof requires the generated
return-clause lambda and each generated outer operation-clause lambda as proof
premises.  OperationGraph currently retains only their bodies as direct fold
edges, despite creating wrapper-lambda occurrence nodes during lowering.
Searching those nodes again by binder identity would be fragile under shared
Core representatives and artifact relocation.  The planned repair therefore
adds authoritative wrapper-lambda occurrence edges and reifies the complete
`2 + 2*n` premise tuple from them.  This representation repair precedes the
inner-force surface work.

### 2026-08-05: Clause-bearing fold proof milestone

- Added authoritative OperationGraph edges for the generated return-clause
  lambda and every generated outer operation-clause lambda.
- Rebuilt `COMPUTATION_FOLD_ELIM` with input, return lambda, and each
  `(operation identity, clause lambda)` pair, for `2 + 2*n` premises.
- Preserved the old zero-clause sequencing path.  Applying handler-specific
  context reindexing to sequencing had admitted a provisional lambda
  derivation into the durable proof graph and was reverted.
- Corrected the post-cleanup fixed-point order to materialize structural
  proofs, solve operation requests, and only then materialize enclosing folds.
- Changed the artifact format from v56 to v57 and relocated, serialized, and
  validated the new edges.  Validation checks correspondence with Core fold
  children, not only operation-ID bounds.
- Corrected the higher-order runtime test to request evaluation of `main`;
  defining a name in the REPL is not execution.
- Passed every `src/prototype/test_*.sh` script and examples 01-09.

The next implementation item is the explicit surface computation-demand path
for a thunk received by a clause.  This milestone does not make generic folds
descend through thunks and does not establish modular higher-order forwarding.

The positive-fold artifact validator still checks structural premises more
strongly than semantic carrier/effect-row equations.  Source compilation has
already solved those equations, but independent replay by the artifact kernel
remains a separate hardening task and must not be confused with the completed
proof-lifecycle repair.

## 14. Completion Rule

This migration is complete only when:

1. all stages above are complete or explicitly superseded with a justified
   replacement recorded in Section 13;
2. all validation rows have direct evidence;
3. the full prototype test suite passes;
4. the artifact and backend boundaries have been exercised;
5. both this progress record and the referenced decision record describe the
   implemented system rather than a future intention;
6. the resulting main commit is pushed to `origin/main`.
