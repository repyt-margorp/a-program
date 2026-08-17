# Post-Hoc Dependent Claims and Certified QuickSort Audit

Date: 2026-08-18

Status: static audit complete; implementation not started

Repository baseline: `0911896aed2e4dc1d002a10dad951f0a33b15481`

Artifact baseline: v78

Related Pull Request: [#15 Document general recursion and effectful iteration research](https://github.com/repyt-margorp/a-program/pull/15)

Related Issues:

- [#11 Add explicit index telescopes and general indexed families](https://github.com/repyt-margorp/a-program/issues/11)
- [#12 Dependent recursive proof production and indexed branch refinement](https://github.com/repyt-margorp/a-program/issues/12)
- [#13 Optional post-hoc dependent Claims cannot refer to pure computation results](https://github.com/repyt-margorp/a-program/issues/13)
- [#14 Specify Totality and Partiality before adding general recursion](https://github.com/repyt-margorp/a-program/issues/14)

## 1. Purpose

The current implementation can:

- declare ordinary and indexed ADTs;
- refine indexed Match branches;
- construct recursive dependent proofs;
- use `Acc` evidence to admit well-founded recursion;
- execute a generic fuel-free QuickSort; and
- serialize and replay the accepted typing graph.

It cannot yet attach an optional theorem such as sortedness to the value returned
by that existing QuickSort computation. The operational QuickSort must remain
one program. Sortedness, permutation, stability, and cost must be independent,
post-hoc Claims about that same program.

This document audits the current boundary before code is changed. It separates:

1. indexed-family formation and dependent Match;
2. termination evidence;
3. computation-result evidence;
4. optional functional-correctness Claims;
5. definitional equality; and
6. runtime verification obligations.

Conflating any two of these would make the immediate example pass by weakening
the kernel contract instead of implementing the requested theorem boundary.

## 2. Executive conclusion

The principal missing feature is not another Indexed Family constructor rule.
It is a replayable bridge from an existing typed computation occurrence to a
value binder that may occur in a dependent proposition.

The required logical shape is:

```text
Returns_A : Thunk(Comp(E, A)) -> A -> Type

quickSortSorted :
  (xs : List A) ->
  (ys : List A) ->
  Returns (&(quickSort A le xs)) ys ->
  SortedByIndex ys
```

`Returns` is a schematic name in this audit. Its witness must be produced only
from accepted operational evidence. It must not be an ordinary user-declared
IADT with a public constructor that can assert an arbitrary result.

For total correctness, the result relation is combined with independent
termination evidence:

```text
PartialEnsures(c, P) := (v : A) -> Returns c v -> P v

TotalEnsures(c, P) := Terminates(c) and PartialEnsures(c, P)
```

The current Acc-driven QuickSort can supply the termination side. The missing
work is the result relation, its elimination principle, and enough optional
order/partition lemmas to prove sortedness and permutation.

The immediate repair must have two paths:

- a closed pure-total path that may normalize to `RETURN(v)` and record static
  evidence; and
- an open path that reasons compositionally about an unknown result through a
  result binder, without running the unknown input during compilation.

Only the first path can be repaired by normalization alone.

## 3. Verified current state

### 3.1 PR #15

PR #15 was merged into `main` as:

```text
0911896 Merge pull request #15 from
        repyt-margorp/agent/review-general-recursion-design
```

The PR is documentation-only. It adds:

`doc/2026-08-18T00-00-00-GENERAL-RECURSION-PARTIALITY-AND-EFFECTFUL-ITERATION-RESEARCH-REVIEW.md`

It does not change compiler behavior.

### 3.2 Issue status matrix

| Issue | Current evidence | Audit disposition |
|---|---|---|
| #11 | Explicit indices, Vec/Matrix formation, Acc elimination, and IF8 QuickSort are implemented. The focused explicit-index-family suite passes. General indexed `Vec.append`, general IADT identity/transport, and every original broad acceptance criterion are not all evidenced. | Keep open or narrow/replace only after its remaining criteria are decided explicitly. |
| #12 | Both original minimal reproducers now compile. Permanent positive, negative, source-span, artifact-write, and artifact-replay checks exist in `test_dependent_match_refinement.sh`. | The reported two Bugs are fixed. The Issue can be closed after posting current commit/test evidence; post-hoc QuickSort properties belong to #13. |
| #13 | Reproduces on current `main`. A closed pure Match normalizes to the expected value, while the same computation in a dependent expected classifier fails lowering. | Active implementation Bug and the primary subject of this audit. |
| #14 | No totality proposition/status exists in the accepted judgement or artifact grammar. Current recursion remains structural, bounded, or Acc-based. | Required before general recursion; it constrains the durable #13 design but need not block a closed-total regression repair. |

### 3.3 Current #13 reproduction

The minimal source from Issue #13 still exits with status 1:

```text
compilation retained an earlier lowering error
/tmp/a-program-issue-13-repro.p:28:10: failed to compile AST graph
metadata resolve-error kind=compile name=proof ast#13 span=28:10
```

The control source, with the same closed Match exported as a computation,
passes normalization comparison:

```text
source-exports-normalization-equal chooseValue chooseExpected mode=default yes
conversion-status equal reason=none steps=10
```

Therefore the failure is not:

- a parser inability to represent `Sorted (choose ...)`;
- a failure of the pure evaluator to reduce the closed Match; or
- a general inability to form the indexed family `Sorted`.

It is a boundary failure between computation-result production and dependent
classifier construction.

A debugger trace further narrows the failure:

- all ten calls to `compile_extract_pure_return_value` return success;
- the expected classifier compiles successfully;
- `compile_ast_against_surface_classifier` successfully lowers both `choose`
  and `proof`; and
- the explicit block form later reports `ascription-mismatch`, with the bound
  `result` still represented as a symbolic fold-result variable.

The closed failure is therefore not an immediate return from the type-expression
compiler. The result is lost as evidence when the expected-type/constraint path
must justify that the symbolic continuation binder denotes the value obtained by
normalization.

### 3.4 Current #12 evidence

The following fixtures compile on the baseline:

- `src/prototype/tests/fixtures/typing/dependent_recursive_comparison_check.p`
- `src/prototype/tests/fixtures/typing/indexed_branch_rebuild_check.p`

`src/prototype/tests/integration/test_dependent_match_refinement.sh` checks:

- recursive dependent-result IH elimination;
- solved Match motives;
- exact branch refinement status;
- impossible branch evidence;
- source-located rejection of a wrong proof orientation;
- residual rather than false success for an unsupported index equation;
- rejection of forged artifact action/binder metadata; and
- artifact v78 write/read replay.

This is enough to distinguish #12's repaired branch-refinement foundation from
#13's still-missing computation-result boundary.

## 4. What IF8 proves and does not prove

The accepted fixture is:

`src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p`

### 4.1 Termination structure

The fixture defines:

```text
Acc A R subject
LT left right
SizedList A size
```

`quickSortAcc` eliminates `Acc Nat LT size`. Each recursive call consumes a
strictly smaller size witness obtained from `Partition`. This is real
well-founded recursion rather than a hidden fuel argument.

The construction follows the accessibility/domain-predicate account of Bove
and Capretta: termination is represented by an inductive accessibility proof,
and recursion is structural on that proof. See [Modelling General Recursion in
Type Theory](https://doi.org/10.1017/S0960129505004822).

### 4.2 Data retained by `Partition`

The current `Partition` constructor stores:

```text
lowerSize
lower
upperSize
upper
LT lowerSize (succ bound)
LT upperSize (succ bound)
```

It does not retain:

- that every lower element is `<= pivot`;
- that every upper element is `>= pivot`;
- that lower and upper contain exactly the input tail elements;
- a relation between Boolean comparator results and propositional order; or
- transitivity/antisymmetry laws for the selected order.

That omission is correct for a termination-only operational algorithm. These
facts should be optional theorem outputs, not mandatory runtime fields.

### 4.3 Result currently produced

`quickSortAcc` and `quickSort` return only `List A`. The fixture proves six
closed normalization examples, including ascending, descending, and duplicate
inputs. Finite examples are regression evidence, not a theorem for open `xs`.

### 4.4 Sortedness is not complete sorting correctness

A program that always returns `nil` is sorted. A complete sorting specification
must at least separate:

```text
SortedByIndex output
Permutation input output
```

Stability and cost are further independent Claims. None belongs in the
termination witness.

## 5. Current lowering boundary

### 5.1 Type-expression applications execute a special pure path

`context_and_type_lowering.inc:4159-4173` handles a type-expression application
by:

1. compiling the function to a Core Term;
2. compiling the argument to a Core Term;
3. constructing `APP(function, argument)`; and
4. calling `compile_extract_pure_return_value`.

`context_and_type_lowering.inc:3896-3972` normalizes with
`PURE_TYPE_WHNF`. It strips `RETURN(v)` and projects returns through a neutral
Match.

This code has four important limitations.

#### No accepted evidence is produced

The helper returns a Term ID. It does not construct a TypedOccurrence, a
Proposition, a Claim, or a Derivation saying that the source computation
returned that value.

Consequently, even successful compile-time evaluation cannot be cited later by
an open theorem or serialized as object evidence.

#### The helper mixes intermediate and final application states

For a normalized term that is neither `RETURN` nor `MATCH`, the helper returns
the WHNF unchanged. This is needed for intermediate curried lambdas, but it
also means the API named "extract pure return value" does not guarantee that
its result is a value returned by a completed computation.

The implementation recursively extracts after every application node rather
than flattening the application spine and checking the final result once.
Category and completion status are therefore implicit in tag-specific control
flow.

#### The API has no explicit outcome type

The helper returns only success/failure. It does not preserve:

- complete return;
- neutral/open computation;
- blocked effect;
- normalization budget exhaustion;
- unsupported reduction; or
- invalid input.

The public normalizer has structured outcomes elsewhere. Type-expression
lowering discards them and emits only the generic graph-compilation error seen
in #13.

#### The complete helper is unbudgeted

`prototype_term_normalize_complete_with_profile` installs no step budget. This
is tolerable only while every admitted pure computation is already known total.
It becomes unsound as a compiler architecture once unrestricted pure recursion
exists: an empty effect row is not a termination proof.

### 5.2 Ordinary strict value contexts already build sequencing folds

`graph_construction.inc:3144-3267` lowers a runtime strict value context into a
zero-clause `COMPUTATION_FOLD`. It allocates an occurrence-specific result
binder and passes that value to a continuation.

This is the right operational representation of:

```text
m : Comp(E, A)
k : (x : A) -> Comp(F, B x)
```

but the current static classifier rule intentionally declines to publish a
closed fold classifier when `B` depends on `x`.

### 5.3 Dependent folds become runtime obligations

`kernel/rules/cbpv.inc:2206-2213` states that no closed computation-fold
classifier exists until runtime supplies the result value. The solver returns
residual status.

`constraint_solver.inc:2027-2134` then:

- normalizes the handled computation using `PURE_TYPE_WHNF`;
- does nothing further in this pass when normalization is complete; and
- records `COMPUTATION_FOLD_RESULT` only for blocked-effect or exhausted cases.

Artifact v78 specifies that a pending fold-result obligation:

- is a runtime contract;
- is never a Claim or Derivation;
- may be discharged only for one execution; and
- cannot retroactively alter accepted static evidence.

This is sound for runtime type verification. It cannot express:

```text
for every xs and returned ys, SortedByIndex ys
```

Turning a discharged runtime obligation into a static Claim would violate the
current artifact contract and make compilation facts depend on one execution.

### 5.4 `::` is correctly post-synthesis

`finalization_and_entrypoints.inc:1377-1654` obtains the synthesized classifier
from the typed occurrence and checks it against the compiled expected
classifier. Expected-type exposure records evidence only after compatibility
has been established.

The repair must preserve this invariant:

```text
term synthesis first
expected classifier compilation/check second
```

`::` must not invent `Returns`, select a Match branch, or inject a desired
classifier into synthesis.

## 6. Current authority gaps

### 6.1 JudgementDB has typing propositions only

The accepted proposition kinds are:

```text
HAS_TYPE
IS_TYPE
```

There is no accepted proposition or object family for:

- evaluation/return;
- termination;
- partial correctness; or
- total correctness.

### 6.2 VerificationDB is conditional runtime authority

VerificationDB currently stores:

```text
COMPUTATION_FOLD_RESULT
EFFECT_ROW_EQUATION
```

Its contract is intentionally weaker than a Claim. Reusing it as theorem
authority is rejected.

### 6.3 Artifact v78 cannot publish the requested theorem boundary

Artifact v78 has no wire term/proof vocabulary for `Returns` or `Terminates`.
It also explicitly leaves generic transport and general IADT identity residual.

An implementation that works only in the compiler process but lacks accepted
artifact replay is incomplete.

### 6.4 Current Identity/HOTT work is not a computation-result relation

Identity evidence may relate values after its type-directed computation rules
have been established. It does not prove that a computation occurrence returns
a particular value. Registering an Identity witness must not add a DefEq edge
or substitute for operational evidence.

Generic transport will be useful later, but it is not the missing semantic
premise in #13.

## 7. Literature comparison

### 7.1 Dependent CBPV

Vakar's [An Effectful Treatment of Dependent
Types](https://arxiv.org/abs/1603.04298) distinguishes dCBPV- from dCBPV+.
dCBPV+ adds dependent Kleisli extension, but the paper also shows why this is
not a harmless ordinary bind rule: subject reduction and concrete effect
models can require the result type to become only more specified as effects
execute.

Implication for A Program:

- `m : Comp(E, A)` is not an `A` and must not be inserted directly into `P`;
- a result binder is the correct bridge;
- the bridge needs explicit conditions on effects and totality; and
- the runtime-dependent-fold verifier is not by itself an open static proof.

### 7.2 Accessibility and well-founded recursion

Bove and Capretta's [Modelling General Recursion in Type
Theory](https://doi.org/10.1017/S0960129505004822) supports the existing Acc
architecture: the logical accessibility proof controls recursive admissibility,
while the algorithm remains recognizably operational.

Implication for A Program:

- retain the existing QuickSort implementation;
- derive `Terminates (&quickSort xs)` from the Acc construction; and
- do not require sortedness evidence to execute QuickSort.

The paper does not by itself provide the CBPV result relation required by #13.

### 7.3 Dependent pattern matching and generated proof principles

McBride and McKinna's [The View from the
Left](https://doi.org/10.1017/S0956796803004829) emphasizes that dependent
pattern matching refines both values and their types, including case analysis
on intermediate results.

Sozeau and Mangin's [Equations
Reloaded](https://rocq-prover.org/papers/equations-reloaded-high-level-dependently-typed-functional-programming-and-proving-in-coq)
compiles dependent pattern matching and well-founded recursion while deriving
high-level proof principles that abstract from the compiled function.

Implication for A Program:

- #12's branch refinements are necessary and now present;
- post-hoc proofs should use a proof/elimination principle derived from the
  accepted operational graph; and
- the user should not manually duplicate QuickSort's recursive equations.

### 7.4 Predicate-transformer specifications

[Dijkstra Monads for Free](https://arxiv.org/abs/1608.06499) derives weakest
precondition semantics from computational monads and supports extrinsic proofs
about existing effectful programs.

Implication for A Program:

- `Ensures`/postcondition reasoning is a principled eventual interface;
- a postcondition takes the returned value explicitly;
- full weakest-precondition generation is larger than the immediate #13 fix;
  and
- the initial deterministic pure-total fragment can expose `Returns` and define
  `PartialEnsures` from it, leaving general effect WPs for a later stage.

### 7.5 Critical synthesis

No cited result licenses automatic promotion of every pure computation into a
type index. The defensible combined design is:

- Acc or another accepted certificate supplies totality;
- CBPV sequencing supplies the result binder;
- operational derivations supply `Returns` evidence;
- dependent families state postconditions about the binder; and
- artifact replay checks the same derivation without proof search.

## 8. Alternatives considered

### 8.1 Normalize every empty-effect computation in a type

Rejected as the general design.

It can fix the closed `choose` regression in the current total fragment, but:

- open `quickSort xs` has no concrete result;
- empty effects do not imply termination;
- budget exhaustion is not divergence or inequality;
- future general recursion could make compilation diverge; and
- raw normalization does not create reusable object evidence.

### 8.2 Make `::` guide synthesis

Rejected.

It violates the established post-synthesis expectation invariant and can hide
missing computation-result evidence behind bidirectional elaboration.

### 8.3 Reuse a pending VerificationDB obligation as a Claim

Rejected.

A pending obligation is conditional on a later execution/backend. A Claim is
accepted static evidence. Their lifecycle and artifact contracts differ.

### 8.4 Replace QuickSort with a proof-carrying implementation

Rejected as the required interface.

A proof-carrying variant may be useful as a library optimization, but it does
not satisfy the requirement that independent theorems attach to the same
operational QuickSort term.

### 8.5 Define `Returns` as a public ordinary IADT

Rejected unless its introduction is sealed.

If users can construct:

```text
returns : Returns c arbitraryValue
```

the proposition has no connection to evaluator semantics. The family may look
like an object type, but its witness introduction must be kernel-controlled and
backed by operational derivations.

### 8.6 Add only a non-object `RETURNS` judgement kind

Insufficient by itself.

The kernel could record an evaluation fact, but a user theorem must quantify
over its evidence and use it as a premise. A Program therefore needs an object
family/witness view or an equally expressive scoped semantic-premise binder.

### 8.7 Introduce a full Dijkstra-monad/WP layer immediately

Deferred, not rejected.

This is attractive for future effects, exceptions, nondeterminism, and state.
It is unnecessarily broad for the first deterministic pure-total result
boundary. The initial rules must be chosen so they can later derive or elaborate
to WPs without changing the meaning of accepted artifacts.

## 9. Recommended semantic boundary

### 9.1 Computations are named as values through thunking

Types depend on values. Therefore the object argument of result/termination
propositions should be a thunked computation:

```text
c : Thunk(Comp(E, A))
Returns A E c v : Type
Terminates A E c : Type
```

Surface syntax may hide `A` and `E`, but the elaborated family must retain them.
Using `&m` also identifies the computation without executing it and preserves
the CBPV value/computation boundary.

### 9.2 Evidence authority

The authoritative identity of a result fact must include at least:

```text
Context
TypedOccurrence of the computation/thunk
input computation classifier Comp(E, A)
result value Term/occurrence
result classifier A
operational derivation
```

Core Term ID alone is insufficient because one erased Core Term may occur under
different Contexts, classifiers, and source operations.

The object `Returns ...` type can remain an ordinary Term graph application,
but its accepted witness Claim must cite the TypedOccurrence authority.

### 9.3 Initial deterministic rules

The first fragment should contain replayable rules corresponding to:

```text
RETURNS_RETURN
RETURNS_BETA_APP
RETURNS_MATCH
RETURNS_ZERO_CLAUSE_FOLD
RETURNS_INDUCTION_HYPOTHESIS
RETURNS_ACC_RECURSION
```

They must mirror accepted operational graph edges rather than reconstructing a
new algorithm from source names.

An implementation may factor these through a checked one-step relation and its
closure, but the artifact must not serialize compiler search history.

### 9.4 Closed pure-total rule

For a closed occurrence:

```text
Pure(c)
accepted normalization(c) = COMPLETE(RETURN(v))
------------------------------------------------
Returns (&c) v
```

Complete normalization is evidence for this closed occurrence only. It does not
prove totality of an open function from a finite test set.

The rule must use a bounded/structured normalization API and retain enough
information for deterministic replay.

### 9.5 Open compositional rule

For open QuickSort, the compiler must not seek a concrete output. The theorem
quantifies over it:

```text
xs : List A
ys : List A
run : Returns (&(quickSort A le xs)) ys
------------------------------------------
... prove SortedByIndex ys ...
```

The proof eliminates `run` or uses a generated functional induction principle
for `quickSort`. Recursive premises refer to the actual recursive
TypedOccurrences and their returned values.

This is the point at which the Equations-style generated proof principle is
preferable to duplicating the operational source.

### 9.6 Partial and total correctness

Once `Returns` exists, partial correctness is definable in the object language:

```text
PartialEnsures c P := (v : A) -> Returns c v -> P v
```

`Terminates c` is separate. In the deterministic normal-return fragment it may
contain or imply existence of a returned value, but it must not be inferred
from `effects = {}`.

The current accepted recursion rules should generate totality evidence:

- structural Match/IH;
- indexed structural recursion;
- Acc/decrease recursion;
- finite fuel; and
- explicitly total host primitives.

General recursion must introduce `MAY_DIVERGE` or `UNKNOWN`, as specified by
Issue #14, before it is admitted.

## 10. QuickSort theorem decomposition

### 10.1 Order interface

The executable comparator remains:

```text
le : A -> A -> Bool
```

The theorem layer additionally receives independent laws, for example:

```text
leTrue  : le x y returns true  -> LE x y
leFalse : le x y returns false -> LE y x
leTrans : LE x y -> LE y z -> LE x z
```

The exact surface formulation depends on public Identity/Boolean reflection.
These laws must not be silently trusted from the host primitive.

### 10.2 Partition theorem

Keep the current operational `partition`. Add a theorem about its result:

```text
partitionCorrect :
  Returns (&(partition A le pivot size xs)) parts ->
  PartitionSpec pivot xs parts
```

`PartitionSpec` should state separately:

- lower values are below/equal to the pivot;
- upper values are above/equal to the pivot;
- lower, pivot, and upper preserve the input multiset; and
- the existing size decrease facts agree with the returned fields.

This proof may recurse over `xs`, but it refers to the existing `partition`
occurrence. It does not add proof fields to the runtime `Partition` value.

### 10.3 Sortedness theorem

The desired theorem is:

```text
quickSortSorted :
  (xs : List A) ->
  (ys : List A) ->
  Returns (&(quickSort A le xs)) ys ->
  SortedByIndex ys
```

Its recursive proof uses:

- Acc induction for the same decrease relation;
- `partitionCorrect`;
- recursive sortedness Claims for lower and upper results;
- comparator soundness; and
- order transitivity to connect lower, pivot, and upper regions.

### 10.4 Permutation theorem

Prove independently:

```text
quickSortPermutation :
  (xs : List A) ->
  (ys : List A) ->
  Returns (&(quickSort A le xs)) ys ->
  Permutation xs ys
```

Total sorting correctness combines termination, sortedness, and permutation.

## 11. Implementation plan

No implementation phase may weaken conversion or use expected types as
synthesis seeds merely to make the regression pass.

### R0: Freeze reproductions and diagnostics

- [ ] Add the closed `choose` failure as a permanent focused regression.
- [ ] Add its successful normalization control.
- [ ] Add the explicit computation-block variant from Issue #13.
- [ ] Preserve the beta-only dependent-family control.
- [ ] Emit a source-located diagnostic distinguishing incomplete, blocked,
      exhausted, unsupported, and invalid pure-result extraction.
- [ ] Post current evidence to #12 and close it if its scope remains the two
      original Bugs.

### R1: Close the static pure-result evidence path

Candidate files:

- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`
- `src/prototype/src/core/term/evaluation_and_conversion.inc`
- `src/prototype/include/a_program/core/term.h`

Work:

- [ ] When a zero-clause fold input normalizes completely to `RETURN(v)`,
      specialize its dependent continuation family at `v` and construct the
      ordinary static fold-elimination evidence instead of merely skipping
      residual-obligation creation.
- [ ] Preserve the runtime fold/execute-once semantics; classifier
      specialization must not replace or duplicate the operational term.
- [ ] Connect direct expected-classifier applications and explicit block-result
      binders to the same closed-result evidence path.
- [ ] Flatten a type-expression application spine before final result
      extraction.
- [ ] Separate intermediate negative function WHNF from a completed returned
      value.
- [ ] Return a structured normalization result, not success/failure alone.
- [ ] Require a final value category before installing a family argument.
- [ ] Account normalization steps against the compile policy budget.
- [ ] Keep effects blocked and never perform host effects in type lowering.
- [ ] Make the closed `choose` classifier reduce to `Sorted NatList.nil` and
      pass ordinary post-synthesis conversion.

R1 fixes the closed regression. It does not satisfy the open theorem criterion.

### R2: Define the result and totality object interfaces

Candidate files:

- `src/prototype/include/a_program/core/term.h`
- `src/prototype/src/core/term/`
- `src/prototype/include/a_program/kernel/judgement/types.h`
- `src/prototype/src/kernel/rules/`
- `src/prototype/spec/artifact_v79.schema` or the then-current successor

Work:

- [ ] Freeze the exact arity and CBPV classifier of `Returns`.
- [ ] Freeze the exact arity and evidence ownership of `Terminates`.
- [ ] Decide whether the surface names are intrinsic, reserved, or library
      aliases while keeping witness introduction kernel-controlled.
- [ ] Add ordinary formation Claims for the proposition families.
- [ ] Add witness Claims whose authority cites the typed computation
      occurrence.
- [ ] Add totality status `TOTAL`, `MAY_DIVERGE`, and `UNKNOWN` without
      conflating it with effect rows.
- [ ] Keep `PartialEnsures` and `TotalEnsures` derivable where possible rather
      than adding redundant Core tags.

### R3: Compositional operational derivations

- [ ] Add result derivations for RETURN, APP/beta, Match, zero-clause fold,
      guarded IH, and Acc recursion.
- [ ] Define deterministic result uniqueness for the initial fragment.
- [ ] Derive open-result premises from accepted occurrence edges.
- [ ] Never use a global Core-Term-to-result table without Context and
      occurrence authority.
- [ ] Keep blocked/effectful/exhausted computations residual.
- [ ] Do not publish runtime discharge as a static Claim.

### R4: Artifact authority and replay

- [ ] Version the new term/proof/status vocabulary.
- [ ] Serialize only rooted accepted result/termination evidence.
- [ ] Relocate Context, TypedOccurrence, result Term, and premise DAG IDs.
- [ ] Replay derivations without rerunning proof search.
- [ ] Reject forged result witnesses, wrong result classifiers, wrong
      occurrence IDs, and missing totality premises.
- [ ] Preserve runtime VerificationDB as a separate conditional layer.

### R5: Generated proof principle

- [ ] Derive a proof/elimination view from accepted recursive computation
      occurrences.
- [ ] Expose each actual recursive call and its result premise.
- [ ] Preserve exact indexed branch refinements from #12.
- [ ] Support a theorem body that follows QuickSort's Acc recursion without
      defining a second QuickSort.
- [ ] Keep proof generation optional and outside operational invocation.

### R6: Certified QuickSort library evidence

- [ ] Define `At` and `SortedByIndex` in permanent fixtures/library source.
- [ ] Define comparator reflection/order-law inputs.
- [ ] Prove `partitionCorrect` about the existing `partition` term.
- [ ] Prove `quickSortSorted` about the existing `quickSort` term.
- [ ] Prove `quickSortPermutation` separately.
- [ ] Combine Acc termination with both optional properties into total sorting
      correctness.

### R7: Acceptance matrix

Positive:

- [ ] beta-only closed result;
- [ ] closed pure Match result;
- [ ] closed recursive pure result;
- [ ] open result-binder theorem;
- [ ] Acc-driven QuickSort sortedness;
- [ ] QuickSort permutation;
- [ ] artifact write/read/replay; and
- [ ] deterministic artifact rewrite.

Negative/residual:

- [ ] arbitrary computation cannot be used directly as a value index;
- [ ] users cannot forge `Returns`;
- [ ] wrong returned value is rejected;
- [ ] wrong occurrence/context is rejected;
- [ ] empty effect row alone cannot forge `Terminates`;
- [ ] blocked effect remains residual;
- [ ] exhausted budget remains unknown/residual;
- [ ] pending runtime verification never becomes a Claim; and
- [ ] one closed successful run does not prove an open function total.

## 12. Required invariants

The implementation is complete only if all of these remain true:

1. `::` remains post-synthesis checking.
2. PropEq/Identity/Returns evidence never mutates global DefEq.
3. A computation is never used directly where a value index is required.
4. Empty effect row is not a termination certificate.
5. Budget exhaustion is not proof of divergence or inequality.
6. Runtime verification discharge is not static theorem publication.
7. Result evidence is occurrence- and Context-specific.
8. Accepted artifacts replay without proof search or hidden host execution.
9. Operational QuickSort is not duplicated by the sortedness theorem.
10. Sortedness and permutation remain separate optional Claims.

## 13. Completion definition

This audit is complete when the current failure, architectural cause, theory
boundary, rejected shortcuts, and staged repair are documented. The feature is
not complete until R0-R7 pass.

The first implementation milestone should be R0-R1. It repairs the concrete
closed-total Bug and improves diagnostics without prematurely freezing the
open theorem wire format. R2-R4 must then establish the semantic and artifact
authority before R5-R6 attempt the final QuickSort theorems.

## 14. Audit evidence log

| Date | Evidence | Result |
|---|---|---|
| 2026-08-18 | PR #15 state and merge commit | merged as `0911896` |
| 2026-08-18 | `git status` and `origin/main` | clean baseline before this document |
| 2026-08-18 | Issue #13 minimal closed Match | reproduces lowering failure |
| 2026-08-18 | Issue #13 normalization control | equal in 10 steps |
| 2026-08-18 | debugger trace of #13 lowering | extraction and expected-directed lowering succeed; evidence closure later fails |
| 2026-08-18 | Issue #13 explicit block form | result binder remains symbolic and dependent ascription mismatches |
| 2026-08-18 | #12 recursive comparison fixture | passes |
| 2026-08-18 | #12 indexed rebuild fixture | passes |
| 2026-08-18 | explicit Indexed Family integration suite | passes |
| 2026-08-18 | artifact v78 schema | no Returns/Terminates authority; pending fold result is runtime-only |
| 2026-08-18 | IF8 source inspection | termination/decrease retained; order/permutation evidence absent by design |
