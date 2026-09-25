# Global Function-Witness Syntax Audit

Date: 2026-09-24
Compiler baseline: `8278c651aa6e5aed332b2aed69625ef75b6d7f22` (Main)
Issue: [#32](https://github.com/repyt-margorp/a-program/issues/32)
Status (2026-09-25): global `*f` removed, internal generation isolated, and
the general ordinary-result QuickSort Sorted theorem checked. Earlier sections
record the investigation at its dated baseline, not the final implementation.

Latest conclusion: no public witness accessor is needed for the explicit
general QuickSort proof now checked in `generic-quick-sorted-result.p`.
It proves `general_sorted A R (quickSort A (&le) xs)` by ordinary induction
over the actual algorithm, without assuming graph adequacy or using a packet
producer. Explicit adapters connect the local proof predicates to the same
nominal `general_sorted` used by the existing graph theorem.
This does not implement automatic adequacy generation for every function.
See [the completion plan](2026-09-25-FUNCTION-WITNESS-SURFACE-REMOVAL-COMPLETION-PLAN.md)
for consumer migration, verification and the archived-fixture boundary.

## Scope and Conclusion

The question concerns global `*f`, not recursive IH syntax such as `*tail`.
It is a proof-interface design question, not an established soundness bug.

The first investigation considered ordinary name/member access to replace
global `*f`. The user clarified that another public witness accessor does not
meet the goal: generation should be internal to proof elaboration. The
primary-source re-audit below supersedes that recommendation. No claim that
the proposed A Program syntax is already sound and complete has been established.

Removing internal evidence construction as well is a different change from
hiding its surface access. Existing graph-based proofs consume that evidence.
Automatic ordinary-result adequacy for arbitrary supported functions has not
been established.
No kernel rule, conversion policy or public syntax changes in this audit.

## What the Current Forms Mean

For an appropriate pure function `f`, conceptually:

- `@f x y` is the generated indexed graph relation, not an inhabitant of it.
- `*f x` produces a dependent packet containing `y` and evidence of `@f x y`.
- A stronger ordinary-result interface would provide evidence of `@f x (f x)`.

Do not conflate the last two. The packet's symbolic output cannot be identified
with the ordinary call merely by renaming its producer. `::` stays a
post-synthesis check, not an instruction to invent an inhabitant.

The implementation separates graph formation and witness production in
`src/prototype/pointer/synthesis.c` (`function_graph_step`,
`function_witness_step`, `graph_reference_step`). They share the checked source
branch plan; this is not evidence of two independent interpreters.
`src/prototype/pointer/function_graph.h` documents the shared plan and packet
contract. Internal helper dependencies also request witnesses.

## Accepted Example Without Global `*length`

The following is checked, current A Program syntax:

```text
Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
adequacy := \xs:List => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
adequacy :: (xs:List) -> @length xs (length xs);
```

The remaining `*tail` is IH, not the global function-witness operator. The
explicit motive is already supported. No global `*length` occurs anywhere
in the complete `length-direct.p` fixture.

That fixture also defines an ordinary graph-elimination property and checks:

```text
property_at_result := \xs:List => property xs (length xs) (adequacy xs);
property_at_result :: (xs:List) -> Unary (length xs);
```

It executes a proof consumer on a two-element list and returns two. This
establishes an ordinary-result theorem for this function, not a generic
compiler transformation for QuickSort or every total function.

## QuickSort and the Remaining Boundary

This current syntax checks and sorts the concrete fixture correctly:

```text
quick_witness := *quickSort;
copy_result := \xs:List Nat =>
	quick_witness Nat (&natLessOrEqual) xs @output => output;
```

The callers no longer use `*quickSort`, but its definition still does. Thus
this is an alias experiment, **not** removal of the operator from the grammar
or a proof of `Sorted (quickSort ... xs)`.

The following attempted shortcut is rejected:

```text
adequacy := \xs:List Nat =>
	*quickSort Nat (&natLessOrEqual) xs @output => @output;
adequacy :: (xs:List Nat) ->
	@quickSort Nat (&natLessOrEqual) xs (quickSort Nat (&natLessOrEqual) xs);
```

The same shortcut rejects for `length`. In both cases, removing only the
final `adequacy :: ...` expectation makes synthesis succeed. This isolates
the failed assertion from parsing or unsupported witness generation. No
claim that ordinary-result adequacy is impossible follows from this result.
Building its evidence, or a checked bridge, remains separate work.

## Verification

Fixtures: `src/prototype/pointer/tests/fixtures/graph_adequacy/`.

| Fixture under `tests/fixtures/graph_adequacy/` | Source result | Steps |
| --- | --- | ---: |
| `length-direct.p` | Accept; proof consumer equals two | 8,881 |
| `length-packet.p` | Reject the ordinary-result expectation | 2,362 |
| `quick-named.p` | Accept; consumer equals sorted `[0,1]` | 84,538 |
| `quick-packet.p` | Reject the ordinary-result expectation | 94,906 |

The optimized baseline compiler checked all four fixtures in ordinary and
retained-reduction images saved at 0, 100 and 1,000,000 steps. Inert resaves
were byte-identical; resumed Solve reproduced the expected accept/reject
results. Completed positive images computed the expected result with evaluator
chunks 1 and 64. This matrix passed 76 command invocations.

The harness initially expected a completed image to load with `done` at zero
steps. That expectation was corrected: both image modes resume through Solve,
so inert load is `pending`, not trusted acceptance. No compiler fix was made.

To check a fixture, use `pointer-check --steps 1000000 FILE.p`. For the two
QuickSort fixtures, also supply `--legacy-intrinsic-dot --imports
src/prototype/pointer/tests/fixtures/sorted-proof-provider.p`; the frozen
provider retains its historical intrinsic spelling. To execute the small
standalone positive test, use `program_test --equal FILE.p main two` on
`length-direct.p`. Negative fixtures document the current boundary, not a
requirement to reject every future checked adequacy implementation.

## Decision Checkpoint

- [x] Distinguish dedicated notation, internal witness generation, and
  ordinary-result adequacy; leave local IH `*arg` untouched.
- [x] Check actual positive and negative programs, including image resumption.
- [x] Record the limits without deleting accepted evidence or adding reflection.
- [ ] Specify a witness-free functional-elimination context, not a renamed
  public witness accessor. No replacement spelling is adopted.
- [ ] Implement the approved syntax and migrate callers before rejecting global
  `*f`. Keep `@f` graph types and IH expressions distinct.
- [ ] Separately establish automatic ordinary-result adequacy for QuickSort
  if that stronger interface is desired; do not infer it from the alias test.

Do not make ordinary `f xs` silently return a packet or make expected types
trigger proof search. #32 and the authority refactor stay open; the following
re-audit replaces the earlier recommendation to use ordinary witness lookup.

## Primary-Source Re-audit: Internal Generation, Not Another Accessor

Rechecked: 2026-09-24, repository `fc19ff5c0214f0277f02feee77ff9cf03b744524`.
Committed compiler C/H sources are unchanged from the tested `8278c65` baseline. Tests
below used its existing optimized binaries, not the unrelated, uncommitted
Context/IADT changes in the working tree. This is a documentation-only audit;
no parser, elaborator or kernel change was made.

### Corrected Confidence Statement

There is primary-source support for generating functional elimination
principles and hiding intermediate proof construction. There is **no cited
theorem certifying this proposed A Program extension**. The examples establish
specific accepted proof terms, not general soundness or preservation of every
current `*f` use. The previous unqualified answer was too strong.

In particular, removing a public witness accessor need not remove internal
witness generation. But neither renaming the accessor nor rechecking a term
alone proves that the new interface establishes the intended proposition
about the original function result.

### What the Primary Sources Actually Support

1. [Rocq 9.1.1, Functional induction](https://rocq-prover.org/doc/V9.1.1/refman/using/libraries/funind.html),
   sections "Tactics" and "Limitations": `functional induction` applies an
   induction principle registered for the function. The user need not supply
   a graph witness at that invocation, but registration and an appropriate
   principle are prerequisites. The documented `Function` mechanism restricts
   source shapes and excludes some dependent, non-structural cases. This is
   a precedent, not a guarantee for all A Program functions or its syntax.
2. [Equations official tutorial, section 1.2.2](https://rocq-prover.github.io/platform-docs/equations/tutorial_basics.html):
   the generated elimination principle follows function clauses and recursive
   calls; `funelim` applies it. User proofs still establish the branch
   properties. The tactic's goal-driven interface does not establish that
   A Program can always synthesize its motive without an expected type.
3. Sozeau and Mangin, [Equations Reloaded (ICFP 2019)](https://sozeau.gitlabpages.inria.fr/www/research/publications/Equations_Reloaded-ICFP19.pdf),
   DOI [10.1145/3341690](https://doi.org/10.1145/3341690), sections 1.7, 5.1
   and 5.2, printed pages 86:10 and 86:25-26: compilation targets Coq terms;
   elimination principles are generated from splitting trees. Crucially,
   section 5.1 uses functional extensionality to justify the well-founded
   unfolding lemmas through accessibility proof irrelevance. Section 1.7
   qualifies computational guarantees according to the equality assumptions
   used. Section 5.2 sketches the elimination construction; it is not a
   complete correctness proof for our CBPV/higher-observational system.
4. Sozeau, [Equations for HoTT (2019)](https://hott.github.io/HoTT-2019/conf-slides/Sozeau.pdf),
   slide labels 3-4 (PDF pages 5-10): dependent functional elimination can
   involve transport; the displayed `filter_elim` concludes a predicate at
   the ordinary `filter` result. This clarifies the intended interface, but
   the presentation is not a proof for A Program or grounds to assume UIP.

These observations neither require importing those assumptions wholesale nor
show that A Program needs them. One possible route is to retain the exact
`Acc` argument and checked helper results in its adequacy theorem, rather
than identify different accessibility proofs. That route still needs an
actual construction. Do not silently add proof irrelevance, equality
reflection or a termination oracle to make the proposed interface work.

### The Missing Construction, Precisely

The following is metatheoretic notation, not new surface syntax:

```text
G_f(x, y)                  generated graph type
W_f(x) : Sigma(y, G_f(x,y)) existing result/evidence packet
C_f(x) : G_f(x, f(x))       ordinary-result adequacy, still to be constructed
H(x,y,p) : P(x,y)           user's checked graph-induction proof
```

If `C_f` is constructed and checked, elaboration may insert
`H(x, f(x), C_f(x)) : P(x, f(x))`. No public witness name is needed. The
kernel validates the generated term using existing rules; it does not trust
an assertion that the elaborator succeeded. General proof-dependent motives
need the corresponding `P(x,y,p)`, not proof erasure.

Without `C_f`, elimination of `W_f(x)` gives the property at the packet's
output, not automatically at `f(x)`. An alternative bridge via equality and
transport, or a directly derived eliminator, must supply the missing
justification. Merely hiding the packet in lowering does not do so.

This is a conditional conservativity argument relative to the existing kernel:
if expansion produces a checked old-language term at the specified type, no
new logical rule is required. It is not a proof that expansion always succeeds,
that the kernel itself is verified, or that all current programs retain their
expressive power. A well-typed term at a different proposition is not enough.

### Rechecked Concrete Programs

All original four fixtures reproduced their recorded source statuses/steps.
Additional tests used `pointer-check --steps 1000000 -` for `length`, and
`--steps 10000000 --legacy-intrinsic-dot --imports
src/prototype/pointer/tests/fixtures/sorted-proof-provider.p -` for QuickSort.

| Additional case | Observed result | Steps |
| --- | --- | ---: |
| Literal proposed `@length xs @nil ... @cons ...` | Unsupported (exit 4) | 1,269 |
| Inline checked `length` adequacy below, with no global `*f` or named accessor | Accepted | 5,661 |
| Direct nested packet/graph Match, without expected type | Rejected | 2,098 |
| Packet expansion through generalized graph-elimination lambda | Accepted | 2,873 |
| Same generalized packet expansion asserted at `Unary (length xs)` | Rejected | 3,374 |
| Existing generic Sorted theorem plus Boolean consumers | Accepted | 615,047 |
| Packet proof asserted at `bool_sorted (quickSort ... xs)` for arbitrary `xs` | Rejected | 626,972 |
| Same assertion for the closed `duplicates` input | Accepted | 2,413,257 |
| Closed proof plus its executable proof consumer | Accepted | 3,003,157 |
| Existing Boolean wrong-output / wrong-comparator cases | Rejected | 616,137 / 615,787 |

Under the `Nat`, `List`, `length` and `Unary` definitions in `length-direct.p`,
this is the accepted inline term (a candidate *expansion target*, not an
implemented automatic translation):

```text
proof := \xs:List =>
	(\n:Nat => \graph:@length xs n => graph
		@nil => Unary.zero
		@cons head tail count rest => Unary.succ count *rest)
	(length xs)
	(xs @(self => @length self (length self))
		@nil => (@length).nil
		@cons head tail => (@length).cons head tail (length tail) *tail);
proof :: (xs:List) -> Unary (length xs);
```

The named `adequacy` function and global `*length` have both disappeared.
The proof still exists as an ordinary term; automatically building this
subterm is additional elaborator work. Local IH expressions remain unchanged.

For the rejected direct packet expansion the body was
`*length xs @output => (@output @nil => Unary.zero
@cons head tail count rest => Unary.succ count *rest)`. Abstracting the graph
elimination over `(input:List)`, `(n:Nat)` and `(graph:@length input n)` before
applying it to `xs`, `output`, `@output` makes synthesis succeed, but the
ordinary-result assertion still rejects. Thus even the proposed simple
nested-Match desugaring is not a verified general implementation strategy.
The exact reason for the direct form's rejection was not instrumented here.

For reproducible QuickSort cases, concatenate
`tests/acceptance/generic-quick-sorted.p` and
`tests/fixtures/generic_sorted/boolean-consumer.p`, then append either block:

```text
// Rejected: an arbitrary input, not an executed closed example.
ordinary_sorted := \xs:List Bool =>
	*quickSort Bool &bool_le xs @ys => bool_correct xs ys @ys;
ordinary_sorted :: (xs:List Bool) -> bool_sorted (quickSort Bool &bool_le xs);
```

```text
// Accepted: the closed input already defined by the consumer fixture.
concrete_sorted := *quickSort Bool &bool_le duplicates @ys =>
	bool_correct duplicates ys @ys;
concrete_sorted :: bool_sorted (quickSort Bool &bool_le duplicates);
concrete_count := read_bool_sorted (quickSort Bool &bool_le duplicates) concrete_sorted;
```

The latter was saved to `/tmp/a-program-witness-reaudit-concrete.a` and
`program_test --steps 10000000 --equal-image IMAGE concrete_count four`
returned `equal=1` for chunks 1 and 64. These tests still use global `*quickSort`
internally: they delimit the required bridge, not demonstrate its removal.
Loading the saved image and advancing ordinary Solve also accepted (3,003,354
steps); the different source/readback counts are not an equality claim.
No entire-compiler regression suite or performance comparison was rerun.

### Conditions Before Adopting Witness-Free Syntax

- [ ] Define where functional elimination is requested, using existing symbols
  if suitable, and distinguish it from ordinary value Match and a graph type.
  The proposed `@f args @case ...` is not today's ordinary Match semantics.
- [ ] Synthesize the motive from clauses or an explicit motive; retain `::`
  as postcheck. Do not copy Rocq's goal-directed tactic behavior implicitly.
- [ ] Generate checked adequacy or an equivalent eliminator from the same
  typed source/branch plan, with exact parameters, comparator, helper calls,
  nominal family and accessibility arguments. Do not add a second authority.
- [ ] Check graph-field/IH scopes and any proof-dependent result family;
  ordinary totality alone does not supply this construction automatically.
- [ ] Demonstrate symbolic `Sorted (quickSort ... xs)` and content preservation,
  not just closed executions or properties indexed by a packet output.
- [ ] Preserve wrong-index/comparator rejection, aliases/imports and pending
  image resumption. Investigate first-class witness uses before claiming no
  loss of expressiveness; do not extrapolate to effects or partial functions.

**Disposition:** hiding witness generation is a defensible design direction,
not yet a certified complete migration. `length` has a checked internal
expansion without a public accessor. General QuickSort ordinary-result
adequacy and the proposed surface elaboration remain unimplemented/unverified.
Keep the Goal paused and #32 open; no further implementation is authorized
by this re-audit alone.

## Follow-up: Closed QuickSort Without Function-Witness Access

Rechecked on the same compiler after the user asked why this alternative had
not been tested. The preceding packet-to-result tests did **not** establish
that global `*quickSort` is necessary. A direct constructor proof for a closed
result was missing from that investigation.

The complete program below uses neither `*quickSort` nor `@quickSort`, and
does not introduce a replacement witness accessor. It imports the unchanged
sorting implementation and defines the Boolean order and its standard
all-later-elements Sorted predicate directly. This is an independent closed
instance, not reuse or replacement of the universal graph-based theorem.

```text
import Nat;
import Bool;
import List;
import quickSort;
bool_order := @\left:Bool => @\right:Bool => {
	bottom:(b:Bool)->* Bool.false b;
	top:* Bool.true Bool.true;
};
bool_le := \x:Bool => x
	@false => (\y:Bool => Bool.true)
	@true => (\y:Bool => y);
AllFrom := \lo:Bool => @\xs:List Bool => {
	nil:* (List Bool).nil;
	cons:(head:Bool)->(tail:List Bool)->bool_order lo head->* tail->
		* ((List Bool).cons head tail);
};
Sorted := @\xs:List Bool => {
	nil:* (List Bool).nil;
	cons:(head:Bool)->(tail:List Bool)->AllFrom head tail->* tail->
		* ((List Bool).cons head tail);
};
nil := (List Bool).nil;
singleton := (List Bool).cons Bool.true nil;
input := (List Bool).cons Bool.true ((List Bool).cons Bool.false nil);
expected := (List Bool).cons Bool.false singleton;
singleton_proof := Sorted.cons Bool.true nil (AllFrom Bool.true).nil Sorted.nil;
proof := Sorted.cons Bool.false singleton
	((AllFrom Bool.false).cons Bool.true nil (bool_order.bottom Bool.true) (AllFrom Bool.false).nil)
	singleton_proof;
proof :: Sorted (quickSort Bool &bool_le input);
read_sorted := \xs:List Bool => \p:Sorted xs => p
	@nil => Nat.zero
	@cons head tail bound rest => Nat.succ *rest;
main := read_sorted (quickSort Bool &bool_le input) proof;
two := Nat.succ (Nat.succ Nat.zero);
direct := quickSort Bool &bool_le input;
```

Run with the frozen provider:

```text
pointer-check --steps 10000000 --legacy-intrinsic-dot \
  --imports src/prototype/pointer/tests/fixtures/sorted-proof-provider.p \
  --save IMAGE.a PROGRAM.p
program_test --steps 10000000 --equal-image IMAGE.a main two
program_test --steps 10000000 --equal-image IMAGE.a direct expected
```

Observed: accepted at 212,505 steps; the proof consumer returns two and the
ordinary call returns the expected sorted list, both at chunks 1 and 64.
Changing only the expectation to `proof :: Sorted input;` rejects at
137,320 steps: the constructed proof is not accepted at the unsorted input.

Here the output list is concrete. Constructor introduction builds its Sorted
proof and conversion checks its classifier against the ordinary QuickSort
call. There is no function-graph membership theorem to request. This is a
confirmed closed example without the public function-witness operation,
not a proof for every `xs` or an automatic functional-induction elaborator.

The remaining difficulty for an arbitrary input is constructing a uniform
proof without first evaluating that input: the existing implementation
passes through measurement, accessibility, partition, two recursive calls
and append. Their intermediate outputs must be connected to the original
call by checked evidence or an equivalent direct induction. A failed packet
cast does not prove that this construction is impossible. Its absence is
unfinished investigation/implementation, not a theorem that the `*f`
surface operator is indispensable.

## Clarification: Public Syntax Versus Internal Witness Generation

The user requires preservation of the **general** QuickSort Sorted theorem,
not correctness on closed Boolean inputs. The preceding closed example does
not establish that requirement. Also separate these two questions:

1. Must the proof infrastructure construct and consume graph evidence?
2. Must the programmer explicitly request it with global `*f`?

For the existing graph-based proof, the first answer is yes. The second is
no inherent requirement: the request can belong to functional-elimination
elaboration. This is an implementation assessment for the supported fragment,
not a claim that witness-free source elaboration has already been implemented.

### Fresh General Tests

Rebuilt the current working-tree compiler with:

```sh
make -f src/prototype/pointer/Makefile \
  BUILD=/tmp/a-program-general-witness-audit pointer-check
```

Base commit: `fc19ff5`; this build also includes the pre-existing uncommitted
`evidence.c/.h` and `iadt.c/.h` changes (61 additions, 3 deletions), without
editing them. Unlike the earlier tests, this is not the older cached binary.
Every test uses the frozen `sorted-proof-provider.p` via `--imports` and
`--legacy-intrinsic-dot`, with a 10,000,000-step bound. The source is
`tests/acceptance/generic-quick-sorted.p`, with each probe appended separately
in memory and passed on stdin. `A`, `le`, `R`, transitivity, reflexivity,
comparison correctness, and `xs : List A` remain bound variables throughout.
No concrete list or specialization of `A` is supplied.

| Probe | Result |
| --- | --- |
| Existing universal `quick_correct` | Accepted, 603,123 steps |
| Universal packet producer containing `ys` and `general_sorted A R ys` | Accepted, 607,498 steps |
| Extract packet proof and postcheck at `general_sorted A R (quickSort A (&le) xs)` | Rejected, 626,341 steps |
| Derive that ordinary-result theorem **assuming** general graph adequacy | Accepted, 769,876 steps |

For the second probe, `SortedPacket` is an ordinary, test-local ADT:

```text
SortedPacket := \A:@ => \R:A->A->@ => @{
	result:(ys:List A)->general_sorted A R ys->*;
};
```

Under the same leading binders as `quick_correct`, its producer body is:

```text
*quickSort A (&le) xs @ys =>
	(SortedPacket A R).result ys
		(quick_correct A le R trans le_refl decide xs ys @ys)
```

Its full postcheck retains all those binders and concludes
`(xs:List A)->SortedPacket A R`. This verifies the general proof-consuming
expansion that an elaborator could use internally, **not** source acceptance
after deleting the `*quickSort` token. The fourth probe assumes
`complete : (xs:List A)->@quickSort A (&le) xs (quickSort A (&le) xs)`;
it does not construct `complete` and must not be reported as doing so.

### Why the Request Can Be Internal

- `synthesis.c:function_graph_request` identifies the typed source and its
  source Match metadata. `graph_reference_step` implements global `*f` by
  requesting `FUNCTION_WITNESS_JOB` for that graph. No user-written witness
  body or additional correctness premise is supplied by the `*` token.
- `synthesis.c:function_graph_step` already requests that same witness job
  for helper dependencies without a user-written global `*helper`.
- `function_graph.c:pg_function_graph_witness_advance` constructs the producer
  through ordinary checked induction. `witness_calls` and `return_packet`
  connect recursive/helper results to graph constructors. This code does
  not require a surface witness operator.
- `synthesis.c:request_inputs_with_jobs` interns requests; graph and witness
  results have an existing owner. Hiding the request requires neither a
  second evidence authority nor a new public `.witness` name. The selected
  generated producer is determined by this mechanism; this does not assert
  uniqueness or irrelevance of all proofs inhabiting the graph relation.

The suitable trigger is a source operation that identifies the function and
arguments for functional induction/case analysis. Its elaborator can obtain
the canonical graph and checked producer, bind the output/evidence internally,
and apply the user's general branch proof. Exact surface notation still
needs approval. Do not infer arbitrary property proofs from `::`, generate
witnesses eagerly for every ordinary call, or accept unsupported/effectful
sources on the assumption that every function has such a producer.

### Separate Remaining Obligations

- [ ] Connect the approved functional-elimination syntax to this existing
  internal producer; retain the general Sorted regression without any public
  global witness accessor. Local recursive IH `*arg` remains unchanged.
- [ ] Separately construct the general ordinary-result connection through
  measurement, accessibility, partition, both recursive calls, and append,
  or give an equivalent direct proof. Test with symbolic inputs, not just
  evaluated lists. Do not silently identify different `Acc` evidence.

Failure of the ordinary-result shortcut is not evidence that public `*f` is
necessary: the shortcut fails **with `*f` present as well**. Conversely,
moving witness generation inside the compiler does not itself discharge that
connection. Preserve both distinctions in the migration report.
