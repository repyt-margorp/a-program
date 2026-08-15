# A Program

A Program is an experimental dependently typed language and compiler
prototype. It compiles a compact source language into a shared canonical term
graph, preserves typed source occurrences in a separate operation graph,
synthesizes classifiers through constraints, records checkable typing proofs,
and can execute CBPV computations with algebraic effect requests.

The active implementation is under `src/prototype/`. Code under `src/` and
`include/` is accepted project code and is not automatically updated from the
prototype. See `AGENTS.md` for the promotion policy.

The former 2025 README described a different architecture. It is archived at
`doc/2026-08-06T00-00-00-LEGACY-README-2025-10-09.md`.

## Current Status

Implemented in the prototype:

- an AST and explicit AST-to-graph elaboration phase;
- context-indexed typed source occurrences over a shared TermDB;
- alpha-aware canonical sharing with pointer-like binder IDs;
- generative positive ADTs, parameterized type formers, constructors, Match,
  and guarded structural induction through `*field`;
- dependent Pi classifiers and computed Match motives for the supported
  guarded fragment;
- CBPV boundaries (`RETURN`, `THUNK`, `FORCE`) and computation blocks;
- explicit effect requests and multi-clause computation folds/handlers;
- bounded classifier/effect solving with strict, hybrid, and exploratory
  policies;
- JudgementDB typing derivations and VerificationDB residual obligations;
- profile-specific pure normalization with memoized WHNF results;
- a compiler-local logical-relation substrate, closed nondependent ADT object
  Identity, pure Return/Thunk Identity, nondependent pure Pi pointwise
  Identity, and selected higher square constructions;
- artifact v74, namespace-qualified interfaces, relocation, linking,
  aggregation, and backend capability checks;
- an interpreter/REPL and an inspection-oriented compiler CLI.

This remains a research prototype. The implemented Identity fragment is not a
complete Higher Observational Type Theory: surface equality and `refl`,
transport/J, general dependent lifting, and Universe coherence are not complete.
General IADT index refinement, general higher-order unification, linear
resources, user-defined operations, and production C or Verilog code generation
are also not complete language features.

## Build

The root `Makefile` builds the prototype directly:

```sh
make
make reader
```

This creates:

- `a.out`: interpreter and REPL;
- `read_file.out`: compiler, graph inspector, artifact, linker, and validation
  CLI.

Both require a C11 compiler. The current build uses only the C standard
library.

Run an example:

```sh
./a.out examples/05_bool_to_nat.p
./read_file.out examples/09_list_induction.p
```

At the REPL:

```text
:whnf main
:nf main
:q
```

`main` entered as an ordinary name executes under the runtime evaluator.
`:whnf` and `:nf` expose the explicit normalization commands.

## Small Example

```ap
Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

toNat := \b : Bool =>
	b @true  => Nat.succ Nat.zero
	  @false => Nat.zero;

negate := \b : Bool =>
	b @true  => Bool.false
	  @false => Bool.true;

main := toNat (negate Bool.false);
```

Applications are curried. Constructor names are selected through the type
view (`Nat.zero`, `Bool.true`, `(List Nat).cons`), while the shared Core graph
may erase distinctions that are irrelevant to computation.

## Surface Language

The source reader is implemented in `src/prototype/src/frontend/reader.c`.
Integration-test fixtures are the most reliable executable syntax reference;
examples 01-07 and 09 cover the stable introductory subset. Later examples may
be drafts for incomplete features.

### Definitions and declarations

An assignment publishes a named typed occurrence after synthesis:

```ap
identity := \A : @ => x : A => x;
```

An external declaration supplies a classifier without a body:

```ap
externalName : A -> B;
```

A standalone expectation checks a separately assigned name after inference:

```ap
main :: Nat;
```

Inline ascription uses the same `::` token:

```ap
(Nat.zero :: Nat)
```

Ascription is a post-synthesis check. It does not inject an arbitrary
classifier into TermDB or replace classifier synthesis.

`import Name;` declares an unresolved artifact dependency. Files can also be
compiled together under one namespace by the CLI; source paths are not the
semantic namespace identity.

### Types and constructors

`@` denotes a universe expression in the current surface. A positive ADT is:

```ap
List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};
```

An indexed family separates uniform parameters from explicit indices:

```ap
Vec := \A : @ => @\n : Nat => {
	nil  : * Nat.zero;
	cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
};
```

Recursive occurrences use only `*` plus index arguments. The declaration name
is not a recursive alias: `List A` inside `List`, or `Vec A n` inside `Vec`, is
rejected.

`*` in a constructor schema means the current instantiated owner type. Field
binders may be dependent:

```ap
Sigma := \A : @ => \B : A -> @ => @{
	mk : (a : A) -> B a -> *;
};
```

The authoritative constructor schema is graph-level and context-indexed.
Readback field metadata is derived diagnostic/interface data, not a second
typing authority.

Pi syntax includes non-dependent arrows and explicit dependent binders:

```ap
A -> B
(x : A) -> B x
```

### Lambda, application, Match, and induction

Lambda binders are typed:

```ap
\x : A => body
```

Nested binders may omit the second backslash:

```ap
\A : @ => x : A => x
```

Match uses elimination clauses:

```ap
n @zero => Nat.zero
  @succ predecessor => predecessor
```

The implemented indexed-family fragment includes ordinary source-defined
`Acc`, exact decrease witnesses, and a generic fuel-free QuickSort. Recursive
calls consume the lifted induction hypothesis supplied by `Acc`; there is no
primitive Acc or QuickSort kernel rule. Public Eq and transport remain future
work and are not used to justify this termination proof.

Inside a recursive constructor branch, `*field` denotes the guarded induction
hypothesis associated with a recursive field:

```ap
len := \A : @ =>
	\xs : List A =>
		xs @nil => Nat.zero
		   @cons x rest => Nat.succ *rest;
```

Lowering checks the direct guarded use discipline. The current motive solver
supports this recursive fragment; it is not a general IADT or higher-order
unification procedure.

### Intrinsic namespace

The `#.` namespace currently contains several different binding kinds:

- host value types: `#.Text`, `#.Int`/`#.Int32`, `#.Int64`;
- pure primitives: `#.int_add`, `#.int64_add`, `#.text_to_nat`, and related
  arithmetic/conversion functions;
- effect operations: `#.print`, `#.scope_text`, `#.scope_text_once`, and
  `#.abort_text`;
- the contextual computation-fold label `#.return`.

Namespace membership does not make every intrinsic effectful. Pure primitives
remain ordinary APP spines. A saturated effect operation application lowers
to an explicit Core request:

```ap
#.print #"hello"
```

There is no `perform` keyword or perform-specific AST node. Aliases are
resolved semantically, so this is equivalent:

```ap
output := #.print;
main := output #"hello";
```

Text literals use `#"..."`; integers use forms such as `#1` and `#-1`.

### Computation blocks

A single-brace block is a sequential computation expression:

```ap
main := {
	b : Bool := negate Bool.false;
	n := toNat b;
	toNat b;
};
```

Bindings sequence a computation once and expose its value to the remaining
continuation. An expression statement discards its result. The final selected
item is the block result.

A direct binding can be selected as a cutoff:

```ap
main := {
	x := #1;
	dead := missing;
}.x;
```

Only the prefix required for `x` is compiled as the selected computation.

Within the dynamic extent of a lambda body, `!value;` exits that nearest
lambda computation. It is not a generic unlabeled block jump and is rejected
outside a lambda or underneath a quoted computation.

### Quotation and implicit boundaries

`&term` quotes a computation as a thunk value. This is how a suspended
computation is passed to a higher-order operation:

```ap
main := #.scope_text &{ #.print #"inner"; };
```

The default surface policy inserts ordinary value-to-computation returns and
some use-site force/thunk boundaries. `--no-implicit-definition-thunks`
enables the stricter explicit definition policy. TermDB still records
`RETURN`, `THUNK`, and `FORCE` explicitly after elaboration.

### Computation folds and handlers

The same `@` clause syntax distinguishes ADT Match from computation folds. A
fold has exactly one `@#.return` clause and zero or more operation clauses:

```ap
main := (#.print #"x")
	@#.return value => value
	@#.print request resume => resume request;
```

The return clause maps the pure leaf. Each operation clause receives the
operation argument and a recursively folded continuation. Unmatched requests
are forwarded with that folded continuation. Multiple distinct operation
clauses are supported; duplicate nominal operations are rejected.

The current built-in operations also carry resumption multiplicity and inner
scope metadata. `scope_text_once` is one-shot and `abort_text` is abortive.
Higher-order payloads are represented by ordinary thunk values rather than a
separate request tag.

### Definition blocks

A double-brace definition block builds a set of named definitions and selects
one root:

```ap
{{
	id := \x : #.Int64 => x;
	main := { #1; };
}}.main
```

Definition blocks and computation blocks are distinct AST forms. The former
select a named program graph root; the latter construct sequential CBPV
computations.

## Compiler Architecture

The implemented pipeline is:

```text
source text
  -> surface AST and source binders
  -> name/type-declaration indexing
  -> ContextDB-indexed OperationGraph + shared TermDB
  -> classifier/effect constraint generation
  -> bounded fixed-point solving
  -> JudgementDB proof reconstruction and validation
  -> VerificationDB residual obligations
  -> artifact/interface or runtime execution
```

### TermDB

TermDB is the canonical static computation and classifier-expression graph.
It contains shared `VAR`, `APP`, `LAMBDA`, `MATCH`, `PI`, constructor,
type-view, CBPV, effect-row, request, fold, intrinsic, and linking terms.

TermDB is not the runtime machine state. Evaluation may intern pure
intermediate/result terms, but live environments, handler stacks, resumptions,
and host resources remain outside canonical term identity.

### ContextDB, SubstitutionDB, and OperationGraph

ContextDB entries are objects of the syntactic context category.
SubstitutionDB stores explicit identity, projection, extension, composition,
and reindexing morphisms.

OperationGraph stores typed source occurrences. Each occurrence points to a
TermDB node and retains its context, polarity, classifier variable, source and
binder provenance, and occurrence edges. Two typed identities can therefore
share one erased Core lambda without merging their classifiers or exported
names.

This many-to-one erasure is deliberate:

```text
typed/source operation occurrences  --many-to-one-->  canonical TermDB
```

Runtime execution follows validated operation roots; it does not choose an
arbitrary classifier attached to a shared Core node.

### Constraints, judgements, and verification

Lowering creates occurrence-local classifier and effect-row constraints. A
worklist/fixed-point solver attempts to solve them under configured step
budgets. Solved results are reconstructed as explicit JudgementDB proof nodes,
including lambda/app, Match motive, constructor, CBPV, request, and fold rules.

VerificationDB currently stores pending runtime checks for dependent
computation-fold results. Other residual state remains in the subsystem that
owns it: effect constraints, classifier-solver state, or Identity/parametricity
actions. A residual is not a negative proof and is not silently published as a
completed `HAS_TYPE` judgement.

Compile policies control admission:

- `strict`: requires the configured static obligations to close;
- `hybrid`: preserves supported residual verification for runtime discharge;
- `exploratory`: records experimental admission metadata and restricts backend
  admission to the interpreter; it does not turn an unresolved classifier into
  an accepted judgement.

The normalization and solver step limits are explicit artifact metadata, so
verification coverage is reproducible as data even when a build chooses a
different budget.

### Normalization and definitional equality

The normalizer has separate profiles:

- Core WHNF: beta-layer observation;
- computation WHNF: CBPV structural reduction;
- pure type WHNF: kernel conversion without host dispatch.

Reduction flags independently control definitions, beta, Match/iota,
induction, CBPV cuts, and pure intrinsics. Results are memoized by term,
profile, and graph revision. Outcomes distinguish `COMPLETE`,
`BLOCKED_EFFECT`, and `EXHAUSTED`; only complete normalization can establish
kernel conversion.

User proofs do not extend global definitional equality, and no propositional
equality type is currently part of the implemented surface language.

### Effects and runtime

The free effect computation is represented explicitly by:

```text
RETURN(value)
OPERATION_REQUEST(operation, argument, continuation)
```

`COMPUTATION_FOLD` interprets or forwards that structure. Pure normalization
never dispatches observable host effects. Only executable runtime mode may
send an unhandled request to an enabled host dispatcher.

Every accepted saturated effect operation source occurrence must lower to one
request. OperationGraph validation rejects a saturated effect APP that escapes
this elaboration boundary.

## Artifacts and Linking

Artifact format v74 serializes the dense reachable accepted object graph of:

- interfaces, qualified exports, dependencies, and transparency;
- TermDB and OperationGraph;
- contexts, substitutions, constructor schemas, and type views;
- JudgementDB proofs, effect constraints, pending runtime verification, and
  compile budgets;
- universe constraints and runtime/backend capabilities;
- relocation and debug/readback metadata.

Every serialized arena is renumbered densely to `0..count-1`; classifier-solver
candidates, work queues, HOTT actions and certificates, normalization fuel,
graph revisions, and unrooted graph nodes are absent. The reader validates
ranges, tags, relocation closure, the artifact calculus fingerprint, and
accepted proof replay. The linker resolves qualified external names, relocates
binders/contexts/terms, preserves typed export identity, and may share
alpha-equivalent Core representatives without merging the exports.

The exact current wire and semantic contract is
[`src/prototype/spec/artifact_v74.schema`](src/prototype/spec/artifact_v74.schema).
The implemented HOTT/Identity boundary is
[`src/prototype/spec/hott_fragment_v5.schema`](src/prototype/spec/hott_fragment_v5.schema).

Useful commands:

```sh
./read_file.out --write-artifact Example.apo --namespace Example example.p
./read_file.out --read-interface Example.apo
./read_file.out --read-graph Example.apo
./read_file.out --link-artifacts User.apo Library.apo --link-output Linked.apo
./read_file.out --aggregate-artifact Bundle.apo Base.apo Provider.apo
./read_file.out --check-backend interpreter Example.apo
```

The C and Verilog backend commands currently perform compatibility/admission
checks; this prototype is not yet a complete source-to-C or source-to-Verilog
compiler.

## Tests

Run the full prototype suite:

```sh
for test_script in src/prototype/tests/integration/test_*.sh; do
	sh "$test_script"
done
```

The permanent suite covers artifact/link flow, CBPV boundaries and surface
coercions, computation blocks and lambda exit, constructor polarity,
context/substitution laws, dependent Pi and Match, guarded IH, handler
resumption multiplicity, and shared Core occurrences with distinct typed
views.

Examples 01-07 and 09 are expected to compile. Later examples include drafts
for features that are not yet complete; do not treat every numbered file as a
positive conformance test.

## Repository Layout

- `src/prototype/`: active experimental implementation and tests;
- `examples/`: executable examples, negative fixtures, and future drafts;
- `training/`: small language exercises and dependent Match probes;
- `doc/`: dated design decisions, implementation plans, and reviews;
- `DESIGN-PHILOSOPHY.md`: historical 2025 design note; several implementation
  claims there have been superseded;
- `CODING_STYLE.md`: repository coding conventions.

## Design Documents

Start with these current documents:

- `doc/2026-08-05T07-00-00-CURRENT-GRAPH-LAYERS-AND-CBPV-EFFECT-CORE.md`
- `doc/2026-08-05T08-00-00-DIRECT-EFFECT-APPLICATION-AND-PERFORM-REMOVAL-PLAN.md`
- `doc/2026-08-05T06-00-00-DEFINITION-BLOCK-AND-IMPLICIT-THUNK-POLICY.md`
- `doc/2026-08-05T03-00-00-HIGHER-ORDER-EFFECT-CAPABILITY-DECISIONS.md`
- `doc/2026-07-27T00-00-00-CATEGORICAL-CWF-DCBPV-MIGRATION.md`
- `doc/2026-07-17T02-00-00-CURRENT-SYSTEM-DESIGN-DEBT.md`
- `doc/2026-07-12T02-00-00-THEORY-READING-MAP.md`

Earlier dated plans are historical records. Their unchecked items and
terminology are not automatically current requirements.

## Theory References

The detailed bibliography and project-specific reading notes are in
`doc/2026-07-12T02-00-00-THEORY-READING-MAP.md`. Core references include:

- Per Martin-Lof, *Intuitionistic Type Theory*;
- Peter Dybjer, *Internal Type Theory*;
- Castellan, Clairambault, and Dybjer, *Categories with Families*;
- Paul Blain Levy, *Call-By-Push-Value: A Functional/Imperative Synthesis*;
- Matthijs Vakar, *A Framework for Dependent Types and Effects*;
- Dybjer and Filinski, *Normalization by Evaluation for Martin-Lof Type
  Theory*;
- de Moura et al., *Elaboration in Dependent Type Theory*;
- Cockx, Devriese, and Piessens, *Unifiers as Equivalences*;
- Cohen, Coquand, Huber, and Mortberg, *Cubical Type Theory*;
- the Univalent Foundations Program, *Homotopy Type Theory*;
- Willsey et al., *egg: Fast and Extensible Equality Saturation*;
- Appel, *Foundational Proof-Carrying Code*.

These references inform design boundaries; they do not imply that all
associated calculi are implemented.
