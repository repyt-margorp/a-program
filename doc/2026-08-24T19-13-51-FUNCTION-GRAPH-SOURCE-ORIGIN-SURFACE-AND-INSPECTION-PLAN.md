# Function Graph Source Origins, Surface Elimination, and `:graph` Inspection

Date: 2026-08-24 JST

Status: design proposal; no language implementation change

Repository: `repyt-margorp/a-program`

Audited revision: `14b51b16019f049ec54c0dced1320012f1de3d1d`

Audited revision subject: `Enforce PR20 semantic authority boundaries`

Audited artifact format: v84

Related documents:

- `doc/2026-08-22T00-00-00-FUNCTION-GRAPH-RETURNS-BRIDGES-AND-PROOF-INTERFACE-RESEARCH.md`
- `doc/2026-08-22T18-06-00-FUNCTION-GRAPH-CERTIFIED-EXECUTION-AND-RETURNS-REMOVAL-PLAN.md`
- `doc/2026-08-23T23-40-38-PR20-MEANING-BOUNDARY-CRITICAL-REVIEW-AND-IMPLEMENTATION-PLAN.md`

## 1. Purpose

A Program can now generate an ordinary indexed function graph for a named,
supported, pure-total function. It can also generate a certified execution
which returns an output together with evidence that the output belongs to that
function graph.

The remaining problem is primarily a source-language proof-interface problem:

> How can a programmer discover and use the branch structure of an already
> defined function without reverse engineering generated internals, depending
> on unstable positional fields, or introducing a trusted function-induction
> tactic?

This document proposes a narrow answer:

1. keep the generated function graph as an ordinary IADT;
2. add one static inspection command, `:graph f`;
3. preserve source origins for generated graph constructors and fields;
4. allow graph cases to select fields by those source origins;
5. hide the generated result package behind direct certified elimination; and
6. continue to use ordinary IADT elimination and ordinary local induction
   hypotheses as the proof mechanism.

This is not a proposal to reflect compiler types or ASTs into the object
language. It is not a proposal for a general tactic language. It does not add a
function-graph rule to the kernel.

## 2. Executive decision

The recommended public model is:

```text
f
    the already accepted executable definition

@f
    the generated graph family of that exact definition

*f arguments @ output graph => body
    execute the certified definition once and bind its dependent result

:graph f
    display the checked, public proof interface generated for f
```

Within a generated graph case, one source call-result binding supplies a
three-role name family:

```text
resultName
    the value returned by the named call

@resultName
    the graph witness paired with that exact value

*resultName
    the induction hypothesis derived from @resultName when and only when that
    graph field is recursive for the graph currently being eliminated
```

These are not three generated identifier strings. They are three syntactic
projections of one source Binding identity. This distinction is required if the
surface is to avoid reserved names such as `returned`, `recursiveOutput`,
`recursiveGraph`, `helperOutput`, or `helperGraph`.

The graph remains an ordinary indexed family. A property theorem therefore has
the existing shape:

```text
propertyOfF :
    (input : A) ->
    (output : B) ->
    @f input output ->
    Property input output
```

The theorem eliminates the supplied graph witness by ordinary Match. Recursive
graph fields receive ordinary local induction hypotheses through the existing
`*field` mechanism.

The initial surface change should not add:

- `f.Result`;
- `f.results`;
- a public `returned` constructor;
- `f.induct` or `quickSort.induct`;
- unrestricted `@x` reflection for every arbitrary local value;
- unrestricted `*x` for a value without a recursive graph origin; or
- object-language reflection of inferred types, motives, ASTs, or graphs.

Scoped `@resultName` and `*resultName` are accepted by this proposal only inside
the generated proof interface which records their exact source origin. General
introspection of an unrelated local value remains postponed. This keeps the
name/sigil design without claiming that every runtime value automatically owns
a function graph or an induction hypothesis.

## 3. Classification of claims

This document uses the following labels conceptually:

| Class | Meaning |
| --- | --- |
| Current fact | Verified against revision `14b51b1`. |
| Design requirement | A property the proposed interface must satisfy. |
| Proposed syntax | Not accepted by the current parser. |
| Possible follow-up | Deliberately excluded from the first implementation. |

Code blocks under a section titled "Proposed" are not claimed to compile at
the audited revision.

## 4. Current implementation facts

### 4.1 Graph generation is requested, not universal

The current front end collects function-graph requests from two source forms:

- `@f` in type position requests the graph family; and
- global `*f` requests the family and certified execution.

`compile_phase_collect_function_graph_requests()` records these requests by the
unique source definition identity. The compiler does not generate a public
graph for every function unconditionally.

This requested-only policy should remain. Function graphs can be materially
larger than the erased executable term, especially for QuickSort and
higher-order callback interfaces.

### 4.2 The generated graph is an ordinary IADT

For the current `length` example:

```a-program
length := \xs : NatList =>
	xs
		@nil => Nat.zero
		@cons head tail => Nat.succ *tail;

length :: NatList -> Nat;
```

the generated family is schematically:

```text
@length : NatList -> Nat -> @
```

with constructors corresponding to the source Match cases:

```text
nil : @length NatList.nil Nat.zero

cons :
    (head : Nat) ->
    (tail : NatList) ->
    (tailOutput : Nat) ->
    @length tail tailOutput ->
    @length (NatList.cons head tail) (Nat.succ tailOutput)
```

The generated family and its constructors are checked through ordinary type
formation, constructor introduction, Match, Context, Substitution, Claim, and
Derivation paths. No special function-induction kernel rule is required.

### 4.3 Constructor labels are copied from source cases

The current generator passes `source_case->constructor_symbol_id` when it adds
a graph constructor. Thus a source `@nil` case becomes the graph's `@nil` case,
and a source `@cons` case becomes its `@cons` case.

This is better than generating arbitrary names such as `equation_1` and
`equation_2`. It already preserves one important source origin.

It is not enough for large generated graphs because constructor fields remain
the difficult part of the interface.

### 4.4 Some field names are source names and some are synthetic

Pattern fields are generally copied from source binder symbols. Recursive and
helper computation evidence currently uses synthetic symbols including:

```text
recursiveOutput
recursiveGraph
helperOutput
helperGraph
output
graph
```

The exact recursive call occurrence that produced one `recursiveOutput` is
held internally by structures such as `function_graph_recursive_site` and
`function_graph_recursive_call`. That origin is not yet a public selector.

QuickSort exposes the practical consequence. Its checked graph constructor has
17 fields. The current integration test consumes it positionally:

```a-program
graph
	@nil current down => output
	@cons tailSize pivot tail current down lowerSize lower upperSize upper
		lowerBound upperBound lowerOutput lowerGraph upperOutput upperGraph
		result appendGraph => output;
```

This proves that the graph is eliminable. It does not provide a satisfactory
human-scale proof interface.

### 4.5 The certified result package still leaks one constructor label

The current source connects execution and graph evidence as follows:

```a-program
packet := *length input;
packet @returned output graph => body;
```

The generated result-family name is internal, but the user must still know the
generated `@returned` constructor. The internal family has the conceptual
shape:

```text
ResultOfLength input :=
    returned :
        (output : Nat) ->
        @length input output ->
        ResultOfLength input
```

The dependency is necessary. The public family name and constructor label are
not.

### 4.6 Artifact v84 stores the semantic association, not source selectors

Artifact v84 records, for each exported graph-enabled owner:

- the owner term export;
- graph type export;
- result type export;
- graph interface term export;
- certified adapter export when present;
- certified runner export; and
- the higher-order certified argument index.

The reader validates the owner/graph/result/runner relationship and does not
reconstruct ownership from Core Term equality.

Artifact v84 does not define a public source-origin selector table for graph
constructors and fields. A stable imported `:graph f` display and named field
selection therefore require an Artifact revision if they are intended to work
across separate compilation.

### 4.7 The lexer currently has no ordinary identifier keywords

The audited reader tokenizes every alphabetic spelling as `TOKEN_IDENT`.
Ordinary words such as `graph`, `output`, `returned`, `case`, and `result` are
not globally reserved lexer keywords. Special meanings are selected by
punctuation, parser position, namespaces such as `#.`, and later name
resolution.

This makes a no-new-reserved-identifier design realistic. A driver or REPL may
recognize `graph` after the command prefix in `:graph f` without making
`graph := ...;` illegal in A Program source.

### 4.8 A source-named recursive result is not yet graph-generatable

The no-reserved-name design requires a programmer to be able to write:

```a-program
@cons head tail => {
	tailLength := *tail;
	Nat.succ tailLength;
};
```

At revision `14b51b1`, this ordinary function body compiles when no function
graph is requested. When a later type mentions `@length`, graph generation
fails with:

```text
function graph terminal IH collection failed body=12 tag=18
failed to generate accepted function graph
```

Therefore source block binding is already expressive enough for the executable
function, but the current function-graph generator does not yet preserve this
named recursive-result shape. The name/sigil proposal is not a description of
current behavior. Supporting a block-bound recursive result is a mandatory
implementation item, not optional syntax polish.

## 5. The actual missing feature

The missing feature is not the ability to construct an IADT eliminator. A
generated graph is already an IADT, so its generic elimination rule already
exists.

The missing feature is a usable mapping:

```text
accepted source definition
    -> generated graph constructor
    -> generated graph field
    -> source branch, binding, or call occurrence
```

Without this mapping, a programmer must learn a generated graph by reading
developer-level compiler output or a long positional pattern.

The distinction is important:

```text
proof power
    ordinary @f IADT elimination already supplies it

proof discoverability
    the programmer cannot reliably see the generated schema

proof addressability
    the programmer cannot select a particular recursive call by its source name

proof stability
    insertion of an unrelated generated field can shift positional patterns
```

A tactic could hide these problems temporarily, but it would still need the
same source-origin map. The source-origin map is therefore the foundational
feature; a tactic is optional presentation built on top of it.

## 6. Why the function's simple type is insufficient

From only:

```text
f : List Nat -> List Nat
```

one cannot prove a nontrivial property of this exact implementation. A sorting
function, the identity function, and a function returning the empty list can
share that simple type while satisfying different properties.

A later proof needs at least one exported proof interface:

- transparent definition equations;
- a function-specific graph;
- a function-specific induction principle; or
- an already proved specification theorem.

A Program has selected the function-graph route. `:graph f` must therefore
inspect the exported graph interface. It must never pretend to derive a graph
or a property from the simple classifier alone.

## 7. Design requirements

### 7.1 Semantic requirements

1. A graph belongs to one accepted definition identity, not to every definition
   with a DefEq Core body.
2. Graph evidence remains proof-relevant ordinary IADT evidence.
3. The graph generator remains untrusted. Existing type and proof checking
   validates its generated objects.
4. A certified execution runs the accepted computation once and binds one
   output together with evidence indexed by that exact output.
5. Named field selection elaborates to ordinary positional IADT elimination
   before kernel checking.
6. No surface convenience adds a new DefEq rule.
7. Imported graphs are available only through an explicit exported
   owner/graph association.
8. Unsupported partial or effectful graphs remain rejected or residual. A
   display command must not silently broaden the accepted graph fragment.

### 7.2 Usability requirements

1. A programmer can ask for the graph schema using only `:graph f`.
2. The output distinguishes inputs, output index, source pattern fields,
   recursive call results, recursive graph premises, helper results, and helper
   graph premises.
3. Two recursive calls in one branch have distinct stable selectors.
4. A proof may bind only fields it uses.
5. The programmer does not need to mention `$graph.f`, `$result.f`,
   `$certified.f`, or `returned`.
6. A function-body refactor that changes the exported proof interface is
   observable as an interface change rather than silently reassigning selectors.

### 7.3 Performance requirements

1. A compilation with no graph request performs no graph generation.
2. `:graph f` requests only `f` and the dependency closure required by `f`.
3. Inspection does not execute `f` on a concrete input.
4. Source-origin lookup is indexed. It must not scan all Terms or Claims for
   every field.
5. Import inspection reads the serialized proof interface. It does not rerun
   source elaboration or decompile TermDB.

### 7.4 No-new-reserved-identifier requirement

The object-language proof interface must be expressible using only:

- identifiers already written by the programmer in the function definition;
- existing constructor labels;
- the existing punctuation vocabulary, especially `@`, `*`, `:=`, braces,
  and arrows; and
- user-chosen binders in a later theorem.

The normative role table is:

| Binding origin | `name` | `@name` | `*name` |
| --- | --- | --- | --- |
| Top-level function | executable definition | generated graph family | certified execution |
| Higher-order function parameter | raw callback | callback graph interface | certified callback interface |
| Named call result in a graph case | returned value | graph witness for that exact call | property IH only when that graph premise is recursive |
| Ordinary pattern field | field value | unavailable unless the field also has an explicit graph origin | unavailable unless an exact recursive graph premise exists |

No implementation-generated word in this table becomes a source identifier.
Synthetic internal symbols may remain in compiler storage and developer dumps,
but `:graph f` and named elimination must present the source-origin roles.

Resolution must use syntactic position, lexical Binding identity, and the
validated source-origin role. It must not use an expected type to guess which
meaning was intended. Type synthesis and ordinary checking validate the
already resolved projection afterward.

## 8. Proposed static command: `:graph f`

### 8.1 Surface contract

The only initial inspection syntax should be:

```text
:graph f
```

Do not initially add parallel spellings such as:

```text
:graph @f
:graph *f
f.graph
f.results
:result f
```

The argument is a global definition name. Name resolution must select exactly
one local definition or one imported owner export. Ambiguity is an error.

### 8.2 `:graph` is not reflection

`:graph f` is a static front-end or REPL command. It does not produce an object
language term. In particular, it does not allow:

```text
g := :graph f;
```

It does not make the following objects available as runtime data:

- compiler AST nodes;
- inferred classifier IDs;
- Match motives;
- Context IDs;
- Substitution IDs;
- Claim IDs; or
- graph schemas.

This boundary is the same reason that a future `:type x` can display the
synthesized type without adding a `^x` type-reflection operator to ordinary A
Program terms.

### 8.3 Local and imported behavior

For a definition available with source:

1. resolve the accepted owner definition;
2. request the same graph family that `@f` would request;
3. generate and check the graph through the normal pipeline in an inspection
   session; and
4. print its public schema.

Inspection alone need not force publication of the generated graph. Publication
still follows the module's ordinary export policy.

For an imported definition:

1. locate the explicit Artifact function-graph association;
2. validate it through the normal reader path; and
3. print only its serialized public schema.

If the provider did not export a graph association, report:

```text
graph interface is not exported for f
```

The importer must not reconstruct the graph from the imported executable Core
Term.

### 8.4 Proposed display for `length`

The exact pretty-printing may evolve, but the information categories should be
normative. For example:

```text
:graph length

owner length
family @length (input : NatList) (output : Nat) : @

case @nil
  result @length NatList.nil Nat.zero

case @cons
  pattern head : Nat
  pattern tail : NatList
  call tailLength.output : Nat
  call tailLength.graph : @length tail tailLength.output [recursive]
  result @length
    (NatList.cons head tail)
    (Nat.succ tailLength.output)
```

This display answers the questions a later proof author actually has:

- which branch constructors exist;
- which values are available in each branch;
- which graph fields represent recursive calls;
- what each recursive call returned; and
- how the final output index is constructed.

### 8.5 Raw type versus normalization

`:graph f` should display the checked graph schema as exported or generated. It
should not silently normalize every displayed type to WHNF or NF.

Optional display flags may later request normalized views, but the base command
must expose the actual synthesized/exported proof interface. This keeps
inspection separate from conversion and avoids implying that the graph is
identified only by normalized Core shape.

## 9. Source-origin model

### 9.1 The compiler needs identities, not only strings

A source symbol such as `lowerResult` is useful for a person. It is not enough
as compiler authority because:

- names can be shadowed;
- two branches can reuse the same name;
- two nested Matches can reuse one constructor label;
- alpha-renaming changes the spelling without changing the Core term; and
- separately compiled modules can use the same spelling.

The compiler should retain a source-origin record conceptually equivalent to:

```text
SourceOrigin {
    owner_definition_id
    branch_path
    source_binding_id
    source_symbol
    occurrence_kind
    occurrence_ordinal
}
```

The exact in-memory representation should reuse frozen accepted structures. It
must not duplicate Contexts, Substitutions, or Core Terms into another mutable
semantic authority.

Suggested occurrence kinds are:

```text
FUNCTION_PARAMETER
PATTERN_FIELD
BLOCK_BINDING_RESULT
RECURSIVE_CALL_OUTPUT
RECURSIVE_CALL_GRAPH
HELPER_CALL_OUTPUT
HELPER_CALL_GRAPH
FINAL_OUTPUT
```

### 9.2 Source origins are reference metadata, not proof axioms

The logical authority remains:

- the generated IADT declaration;
- its checked constructor classifiers;
- the ordinary graph witness term; and
- the accepted Match derivation.

The source-origin table only maps a surface selector to an exact constructor
field ordinal. After this resolution, ordinary IADT checking validates the
proof.

The source-origin table must not authorize a field whose classifier disagrees
with the actual graph constructor. Reader validation must reject such a table.

### 9.3 Artifact identity

Compile-session AST or Binding IDs are not suitable as wire identities by
themselves. A future Artifact version should serialize a canonical selector
table relative to already validated exports:

```text
graph owner export
graph constructor export or constructor ordinal
graph field ordinal
occurrence kind
canonical branch path
display symbol
same-origin output/graph pairing
recursive marker
```

The reader must validate:

1. every selector targets the associated graph family;
2. constructor and field ordinals are in range;
3. no selector is duplicated within one case;
4. output/graph pairs agree on their source call origin;
5. a recursive marker targets an actual recursive graph premise; and
6. the selected field classifier equals the classifier in the checked
   constructor telescope.

Generated strings remain display names. The validated owner, constructor, and
field identities are the resolution authority.

### 9.4 Alpha-renaming policy

Core alpha-equivalence and source proof-interface compatibility are different
questions.

If a source binding name is exported as a graph selector, renaming it changes
the human-facing proof ABI even if the executable function remains
alpha-equivalent. The first implementation should report this as a proof
interface change.

A later explicit stable label may decouple the public selector from the local
variable spelling. The first implementation should not guess such stability
from TermDB equality.

## 10. Proof-addressable source bindings

### 10.1 Anonymous repeated calls are the hard case

Consider the current QuickSort branch:

```a-program
append A
	(*down lowerSize lowerBound lower)
	((List A).cons pivot
		(*down upperSize upperBound upper));
```

There are two recursive calls to the same worker. A display can call them
`recursive call 0` and `recursive call 1`, but those ordinals are poor public
selectors. Inserting another call can renumber them.

The source already has the correct naming mechanism: block binding.

### 10.2 Proposed proof-addressable function body

The programmer should name any call site that must become a stable proof
interface:

```a-program
{
	lowerResult := *down lowerSize lowerBound lower;
	upperResult := *down upperSize upperBound upper;
	result := append A lowerResult
		((List A).cons pivot upperResult);
	result;
}
```

This does not ask the programmer to construct graph evidence manually. It gives
the graph generator stable source occurrences from which to derive selectors:

```text
lowerResult
@lowerResult
*lowerResult
upperResult
@upperResult
*upperResult
result
@result
```

In this list, the leading `@` is selector notation in a named graph pattern:

- `lowerResult` selects the returned value field;
- `@lowerResult` selects the graph field paired with that returned value.
- `*lowerResult` selects no constructor field; in a proof body it denotes the
  existing IADT induction hypothesis derived from the recursive
  `@lowerResult` field.

These forms are available only because `lowerResult` names an exact call origin
recorded in the generated graph interface. They are not general expression
forms for asking for the inferred type, graph, or proof of every local value.

### 10.3 Unnamed call policy

The generator may continue to represent unnamed recursive calls internally.
`:graph f` may display diagnostic ordinals for them.

The initial public named-pattern contract should require an explicit source
binding for stable selection. It must not invent lasting public names such as
`call1` and `call2`.

This yields a simple rule:

> If a call site must be addressed by a later proof, bind its result in the
> original definition.

### 10.4 Higher-order parameter companions

QuickSort also exposes a second naming problem. Its comparator parameter is
written as `le`, while the current generated graph interface uses an additional
explicit binder commonly printed as `LeGraph` and a certified callback package.
`LeGraph` is not a name from the original function definition.

To satisfy the name/sigil requirement, the public proof interface should derive
the companions from the original parameter Binding:

```text
le
    raw callback

@le
    graph family interface for that callback

*le
    certified callback interface pairing each callback result with @le evidence
```

A generic theorem may need to quantify all three roles, but it should not have
to invent a second identifier such as `LeGraph`. A concrete global callback
uses the same rule with its global name.

The graph companion's classifier can be written explicitly. The certified
companion's classifier is canonically determined by the raw callback spine and
the preceding graph companion, so a sigil binder may synthesize it:

```a-program
\le : A -> A -> Bool =>
\@le : (left : A) -> (right : A) -> Bool -> @ =>
\*le =>
body
```

The exact binder grammar remains to be implemented. It must create distinct AST
roles tied to the same source Binding; it must not decide between them by
expected-type search. Without this higher-order companion rule, the name/sigil
design is complete for first-order functions but not for generic QuickSort.

## 11. Proposed named graph-case patterns

### 11.1 Motivation

Current Match patterns bind every constructor field positionally. This is
acceptable for a small two-field datatype. It is not acceptable for a
generated 17-field QuickSort proof interface.

Named graph-case patterns should be surface sugar over the exact constructor
telescope. They do not change IADT elimination.

### 11.2 Proposed syntax

For a graph witness named `graph`:

```a-program
graph
	@cons {
		pivot;
		lowerResult;
		@lowerResult;
		upperResult;
		@upperResult;
	} => body;
```

Rules:

1. `name;` selects a field and binds it under the same local name.
2. `selector := localName;` selects a field and binds a chosen local alias.
3. `@sourceBinding` selects the graph field paired with that call-result origin.
4. Omitted fields are still present in the constructor telescope but are not
   introduced as user-visible local names.
5. Selection order does not affect constructor field order.
6. Duplicate selectors and unknown selectors are errors.
7. Positional patterns may remain for ordinary IADTs and backward
   compatibility.
8. `*sourceBinding` is not a stored constructor field. In the case body it
   resolves to the eliminator's ordinary IH for recursive `@sourceBinding`.
9. `*sourceBinding` is rejected for a helper or other nonrecursive graph field.

After `@sourceBinding` has been selected, the same spelling is also a scoped
term reference to that graph witness in the case body. Parser position
distinguishes a prefix `@sourceBinding` term from the `@constructor` which
introduces an elimination clause.

The braces distinguish this form from the current positional Match grammar.

### 11.3 Ordinary induction hypotheses remain ordinary

Suppose `@lowerResult` selects a recursive graph premise. In a theorem whose
motive proves `Sorted output`, the source-origin form:

```a-program
*lowerResult
```

denotes the induction hypothesis for that recursive IADT field. It elaborates
to the same ordinary IH that the current implementation attaches to the
corresponding positional recursive graph field.

This does not define a second function-induction theory. The proof remains
ordinary graph-IADT elimination:

```text
@lowerResult : @quickSortAcc ... lowerResult
*lowerResult : Sorted lowerResult
```

This preserves the semantic meaning of local `*name`: apply the induction
hypothesis attached to one exact recursive IADT graph field. The surface anchor
is the source call-result Binding rather than a synthetic generated graph-field
name.

## 12. Proposed direct certified elimination

### 12.1 Surface syntax

Replace the visible package Match:

```a-program
{
	packet := *length input;
	packet @returned output graph => body;
}
```

with:

```a-program
*length input @ output graph => body
```

The grammar is conceptually:

```text
certified_elimination
    ::= "*" global_definition application_argument*
        "@" output_binder graph_binder "=>" term
```

The exact parser integration must avoid ambiguity with application arguments
and existing Match syntax. Parenthesized arguments remain available where the
end of the application spine is otherwise unclear.

### 12.2 Elaboration

The first implementation can elaborate mechanically to the current package:

```text
*f arguments @ output graph => body
```

becomes:

```text
{
    hiddenPacket := *f arguments;
    hiddenPacket @returned output graph => body;
}
```

The generated `$result.f` family and `returned` constructor may remain internal
in Artifact v84 during this phase.

This translation preserves the dependent Context extension:

```text
output : ResultType f arguments
graph  : @f arguments output
```

The graph's final index is the exact output binder from the same certified
execution.

### 12.3 Exactly-once rule

The elaborator must not implement the surface form by separately evaluating:

```text
output := f arguments
graph  := *f arguments
```

That would duplicate computation and would become observably wrong once
effectful graphs exist. Even in the current pure-total fragment, it would make
the intended execution/graph connection an external compiler assumption.

One certified execution must construct both the value and its graph evidence.

## 13. Complete `length` workflow

### 13.1 Definition with a proof-addressable recursive result

Proposed source style:

```a-program
length := \xs : NatList =>
	xs
		@nil => Nat.zero
		@cons head tail => {
			tailLength := *tail;
			Nat.succ tailLength;
		};
```

The graph generator associates both generated recursive fields with the source
binding origin `tailLength`.

### 13.2 Inspecting the proof interface

```text
:graph length
```

shows, among other details:

```text
case @cons
  pattern head : Nat
  pattern tail : NatList
  call tailLength.output : Nat
  call tailLength.graph : @length tail tailLength.output [recursive]
```

### 13.3 Later property theorem

```a-program
lengthOutputUnary := \input : NatList => \output : Nat =>
	\graph : @length input output =>
		graph
			@nil => Unary.zero
			@cons {
				tailLength;
				@tailLength;
			} => Unary.succ tailLength *tailLength;
```

This theorem is defined after `length`. It does not reconstruct the recursion
of `length`. It eliminates the generated graph and receives the ordinary
induction hypothesis for `tailGraph`.

### 13.4 Applying the theorem to one actual execution

```a-program
executionProperty :=
	*length oneElement @ output graph =>
		lengthOutputUnary oneElement output graph;
```

No public result-family name and no `@returned` label are required.

## 14. Complete QuickSort proof skeleton

### 14.1 What the graph provides

For the recursive QuickSort worker, a correct function graph can expose:

- the pivot and tail;
- the partition result and its bounds;
- the lower recursive result and graph;
- the upper recursive result and graph;
- the append result and helper graph; and
- the final output index.

It does not by itself prove:

- comparator total-order laws;
- partition membership;
- partition ordering around the pivot;
- permutation preservation;
- append sortedness; or
- final sortedness.

Those remain ordinary lemmas and property IADTs.

### 14.2 Why source names matter here

QuickSort has two recursive calls of the same worker in one branch. A generic
`recursiveOutput` name cannot tell the proof author which one is lower and
which one is upper. Position alone is fragile.

With explicit original bindings:

```a-program
lowerResult := *down lowerSize lowerBound lower;
upperResult := *down upperSize upperBound upper;
result := append A lowerResult ((List A).cons pivot upperResult);
```

the generated proof interface can preserve the intended roles without
inventing algorithm-specific knowledge in the compiler.

### 14.3 Proposed inspection

The relevant part of:

```text
:graph quickSortAcc
```

would display schematically:

```text
case @cons
  pattern pivot : A
  pattern lower : SizedList A lowerSize
  pattern upper : SizedList A upperSize

  call lowerResult.output : List A
  call lowerResult.graph  : @quickSortAcc ... lower lowerResult.output
                            [recursive]

  call upperResult.output : List A
  call upperResult.graph  : @quickSortAcc ... upper upperResult.output
                            [recursive]

  call result.output : List A
  call result.graph  : @append A lowerResult.output
                         ((List A).cons pivot upperResult.output)
                         result.output

  result @quickSortAcc ... input result.output
```

The actual dependent indices must be printed in full or made available in a
verbose mode. Ellipses are acceptable in this document only; they are not an
acceptable compiler display if they hide a binder needed by the proof.

### 14.4 Proposed sortedness proof shape

```a-program
quickSortAccSorted :=
	\A : @ =>
	\le : A -> A -> Bool =>
	\@le : (left : A) -> (right : A) -> Bool -> @ =>
	\*le =>
	... =>
	\graph : @quickSortAcc A @le size access input output =>
		graph
			@nil => Sorted.nil
			@cons {
				pivot;
				lowerResult;
				@lowerResult;
				upperResult;
				@upperResult;
				@result;
			} => {
				lowerSorted := *lowerResult;
				upperSorted := *upperResult;
				combineSorted
					pivot
					lowerResult lowerSorted
					upperResult upperSorted
					partitionOrders
					@result;
			};
```

This is a skeleton, not currently accepted QuickSort proof source. It shows the
division of responsibility:

```text
generated graph
    supplies exact recursive calls and their induction hypotheses

partition lemmas
    supply lower <= pivot <= upper facts

append/sorted lemmas
    combine the mathematical facts

ordinary IADT checking
    validates the final proof term
```

No `quickSort.induct` primitive is required to obtain the two recursive
induction hypotheses.

## 15. Why `:graph f` is necessary but not sufficient

`:graph f` solves discoverability. It does not construct a proof.

The actual proof power comes from:

```text
graph : @f input output
```

and ordinary elimination of that graph.

Named source-origin selectors solve addressability and positional stability.
Direct certified elimination solves the `returned` package leak.

The four pieces therefore have separate jobs:

| Feature | Job |
| --- | --- |
| `@f` | State the function-specific input/output derivation family. |
| ordinary Match | Eliminate that IADT and obtain branch premises. |
| source-origin selectors | Select generated premises by stable source role. |
| `:graph f` | Show the interface a proof author may select. |
| direct `*f` elimination | Connect one actual output to one graph witness. |

Removing any one row changes a different usability or semantic boundary.

## 16. Why not begin with a tactic?

A tactic such as:

```text
fun_induction quickSort
```

could eventually be useful. It would still need to know:

- which generated graph belongs to `quickSort`;
- which constructor corresponds to each source branch;
- which fields are the two recursive premises;
- which returned values index those premises; and
- which imported interface exported the schema.

That is exactly the source-origin proof interface proposed here.

Implementing the mapping first has three advantages:

1. the non-tactic term language remains sufficient;
2. the mapping can be inspected and audited with `:graph f`; and
3. a later tactic becomes untrusted syntax construction over ordinary terms.

The tactic must not become the only way to discover an undocumented generated
schema.

## 17. Why not expose the evaluator's global relation?

A language-wide evaluation relation exposes the semantics of every computation
step. It can be mathematically useful, especially for effects and partiality,
but it is not the minimum interface for a property of one named total function.

For the present goal, the function-specific graph already provides:

```text
@f input output
```

and the certified runner already provides one witness of that relation.

The proposed syntax therefore does not reintroduce public `Returns`. If a
future language-wide computation logic is needed, it should be designed for
effects, divergence, handlers, and may/must observations on its own merits. It
should not be smuggled back as an implementation detail of `:graph f`.

## 18. Opacity and separate compilation

### 18.1 Three useful export policies

An implementation may export:

| Policy | Executable body | Graph interface | Stable theorems |
| --- | ---: | ---: | ---: |
| Transparent implementation | visible | optional | optional |
| Abstract implementation with proof ABI | hidden | visible | visible |
| Type-only opaque | hidden | hidden | separately chosen only |

If both the body and graph are hidden, no command can recover a nontrivial
property from the function's simple type. `:graph f` must report that the graph
is unavailable.

### 18.2 Graphs are implementation proof ABIs

A generated function graph mirrors branch and recursion structure. It is
therefore more sensitive to refactoring than a high-level theorem such as:

```text
quickSortSorted : ...
```

Library authors should be able to use the graph internally and export only a
stable theorem downstream. Exporting the graph itself deliberately exposes an
implementation proof ABI.

`:graph f` should identify whether the displayed schema is:

- a local generated preview;
- an exported graph ABI; or
- an imported graph ABI.

## 19. Name resolution and ambiguity

### 19.1 Global owner resolution

`:graph f`, `@f`, and global `*f` must use the same unique owner resolution.
They must not disagree about which definition `f` denotes.

Aliases do not acquire another definition's graph merely because their Core
Terms are equal. If alias graph forwarding is desired, it requires an explicit
checked association.

### 19.2 Local selector resolution

Within one graph constructor case, a selector resolves by:

```text
owner graph identity
constructor identity
source-origin selector
occurrence role
```

It does not search all local variables by spelling.

If two fields would print the same selector, `:graph f` must qualify them using
the canonical branch path or require an explicit stable source binding. The
compiler must never silently choose the first match.

### 19.3 Interaction with existing `@` and `*`

The proposed uses remain syntactically scoped:

- `@f` in type position is a global function-graph family reference;
- `@constructor` after a Match scrutinee is a constructor case;
- `@sourceBinding` inside a named graph-case pattern is a graph-field selector;
- global `*f` begins certified execution;
- local `*sourceBinding` in a graph case applies the ordinary IADT induction
  hypothesis associated with that source binding's recursive graph field; and
- a higher-order parameter may acquire scoped `@parameter` and `*parameter`
  companion roles in a generated proof interface.

The parser must represent these as distinct AST forms. Semantic resolution
must not infer the role from TermDB shape after parsing.

For a higher-order parameter, the proposed binder roles are likewise
punctuation-based:

```a-program
\le  : A -> A -> Bool =>
\@le : (left : A) -> (right : A) -> Bool -> @ =>
\*le =>
body
```

The classifier of the `*le` companion is synthesized from the already resolved
raw callback `le` and graph family `@le`. Internally it is the ordinary
dependent package shape generated by the existing graph implementation. No
fixed type name such as `CertifiedCallback` is required at the surface.

## 20. Internal elaboration architecture

### 20.1 Reuse accepted authorities

The implementation already retains:

- source AST assignment and Match structure;
- TypedOccurrence identities;
- accepted classifier/effect/totality data;
- Match Contexts and refinement Substitutions;
- exact IH owner/scope/case/field identities; and
- function-graph owner associations.

The source-origin feature should be an immutable view and a derived selector
table over these authorities. It must not create an independently mutable
`DefinitionTree` containing copied Contexts or Terms.

### 20.2 Generation sequence

A suitable staged sequence is:

1. accept and freeze the executable source definition;
2. collect graph requests from `@f`, global `*f`, and inspection sessions;
3. construct the accepted-definition view;
4. discover source branches, block bindings, helper calls, and recursive sites;
5. generate the ordinary graph IADT and certified runner;
6. record selector-to-constructor-field mappings while generation still has
   exact source origins;
7. type-check all generated objects through ordinary paths;
8. validate the selector table against the accepted generated constructors;
9. publish the semantic graph association and public selector table if export
   policy permits; and
10. let `:graph f` print only the validated interface.

### 20.3 Do not decompile later

After source occurrences have been erased, TermDB alone cannot reliably recover:

- which identical recursive call came from which source occurrence;
- the original branch path;
- the source binding selected as a public name; or
- the intended proof-interface ownership of alpha-equivalent terms.

Source-origin capture must occur during definition/graph finalization, not when
a later proof or importer asks for `:graph f`.

## 21. Surface grammar sketch

This is a design sketch, not a parser patch.

```text
inspection_command
    ::= ":graph" global_name

function_graph_type
    ::= "@" global_name type_argument*

certified_elimination
    ::= "*" global_name application_argument*
        "@" binder binder "=>" term

graph_named_case
    ::= "@" constructor_name "{" graph_field_binding* "}" "=>" term

graph_field_binding
    ::= selector ";"
     |  selector ":=" local_name ";"

selector
    ::= local_name
     |  "@" local_name
     |  qualified_origin_selector

role_binder
    ::= local_name
     |  "@" local_name ":" type_expr
     |  "*" local_name

source_origin_term
    ::= "@" local_name
     |  "*" local_name
```

`qualified_origin_selector` is required only when a short selector is
ambiguous. Its final spelling should be chosen after the first selector-table
prototype demonstrates actual ambiguous cases. The implementation must not
freeze an invented textual path before testing nested Match and shadowing.

## 22. Error behavior

The implementation should provide dedicated diagnostics for at least:

```text
unknown function graph owner
ambiguous function graph owner
function graph not requested or unavailable
imported function graph association missing
function graph generation residual for unsupported source shape
graph selector metadata missing
unknown graph field selector
ambiguous graph field selector
duplicate graph field selector
selector role does not match constructor field
recursive selector targets a nonrecursive graph premise
certified elimination requires a supported pure-total owner
```

Errors should include the owner and constructor names and should suggest
`:graph f` for valid selectors where appropriate.

Do not collapse these failures into generic "failed to compile AST graph" in
the final user-facing interface.

## 23. Implementation phases

### FGSI0: Freeze the contract

- [ ] Add parser-negative fixtures for `:graph`, named graph cases, and direct
  certified elimination before enabling them.
- [ ] Record current `length`, two-recursive-call `mirror`, dependent-spine, and
  QuickSort graph outputs.
- [ ] Confirm no graph request remains a no-generation path.
- [ ] Freeze the distinction between source-origin metadata and proof authority.

Exit condition:

- the surface and authority boundaries in this document are accepted or
  explicitly revised.

### FGSI1: Static graph inspection

- [ ] Add a driver/REPL request representing `:graph f`.
- [ ] Resolve one local or imported owner using existing graph ownership rules.
- [ ] Print family indices, constructor cases, field classifiers, recursive
  markers, and current diagnostic names.
- [ ] Keep inspection outside the object language.
- [ ] Add a CLI equivalent for noninteractive tests, for example
  `--show-function-graph f`.

Exit condition:

- `:graph length`, `:graph mirror`, and `:graph quickSortAcc` deterministically
  display their checked schemas;
- an imported owner without an exported graph is rejected.

### FGSI2: Source-origin capture

- [ ] Add immutable source-origin records to the accepted graph-generation
  view.
- [ ] Pair each generated output and graph field with one exact call origin.
- [ ] Preserve source block-binding names when available.
- [ ] Accept block-bound recursive results during function-graph generation;
  the audited `tailLength := *tail` probe currently fails at terminal IH
  collection.
- [ ] Mark exact recursive graph premises.
- [ ] Detect shadowing and short-name ambiguity.
- [ ] Update `:graph` to print source-oriented selectors.

Exit condition:

- QuickSort's lower and upper recursive calls are distinguishable without
  field ordinals or synthetic `recursiveOutput` names.

### FGSI3: Named graph-case patterns

- [ ] Parse the braced named-case form separately from positional Match.
- [ ] Resolve selectors through the validated origin table.
- [ ] Lower named patterns to the existing positional constructor telescope.
- [ ] Preserve the current ordinary local `*graphField` IH behavior.
- [ ] Resolve scoped `name`, `@name`, and `*name` from one source call-result
  Binding without introducing generated source identifiers.
- [ ] Support omission and aliasing of unused fields.

Exit condition:

- the `lengthOutputUnary` example compiles without positional recursive fields;
- a QuickSort proof skeleton can select lower and upper recursive graph premises
  independently;
- swapping the two selector mappings is detected by type/index checking or
  produces the correspondingly swapped explicit proof term, never silent field
  rebinding.

### FGSI4: Direct certified elimination

- [ ] Parse `*f arguments @ output graph => body`.
- [ ] Elaborate it to the current checked result package.
- [ ] Ensure one execution constructs both output and graph.
- [ ] Remove `@returned` from ordinary user documentation and examples.
- [ ] Decide explicitly whether standalone first-class storage of `*f arguments`
  remains supported.

Exit condition:

- post-hoc property application needs only `@f`, direct `*f` elimination, and
  ordinary graph Match at the source level.

### FGSI5: Artifact proof-interface metadata

- [ ] Define a new Artifact schema version for public selector metadata.
- [ ] Publish selectors only for exported graph associations.
- [ ] Relocate constructor and field identities without name-based inference.
- [ ] Validate all selector invariants on read, link, and replay.
- [ ] Make imported `:graph f` print the serialized interface.
- [ ] Add corruption fixtures for wrong owners, fields, roles, call pairs, and
  recursive markers.

Exit condition:

- a provider can hide its body, export a graph proof ABI, and let a consumer
  inspect and eliminate that graph by the same selectors after linking.

### FGSI6: QuickSort property case study

- [ ] Refactor the test QuickSort worker to bind lower, upper, and append results
  explicitly where they form the public proof interface.
- [ ] Replace a separately invented `LeGraph` surface binder with the
  `le`/`@le`/`*le` higher-order companion roles.
- [ ] Display the resulting schema through `:graph quickSortAcc`.
- [ ] Implement at least one nontrivial post-hoc property using named graph
  fields and both recursive IHs.
- [ ] Keep comparator laws and partition lemmas explicit.
- [ ] Measure source compile time, generated Terms, typed occurrences, Artifact
  bytes, and read/link time.

Exit condition:

- the feature is demonstrated on a two-recursive-call dependent worker, not only
  on unary `length`.

## 24. Verification matrix

### 24.1 Positive source tests

- `length` graph display and unary post-hoc property;
- `mirror` with two recursive calls in reversed order;
- a dependent argument-spine graph;
- QuickSort lower/upper recursive selectors;
- helper-call output/graph selectors;
- omitted named fields;
- local selector aliases;
- direct certified elimination;
- local source and imported Artifact inspection.

### 24.2 Negative source tests

- unknown `:graph` owner;
- ambiguous owner;
- nonfunction owner;
- unsupported effectful owner;
- unsupported partial owner;
- anonymous repeated call selected by an invented source name;
- duplicate selector;
- selector from the wrong constructor;
- selector from another function's graph;
- `@callName` used where no call graph field exists;
- `*field` used on a nonrecursive field;
- attempt to use `:graph f` as an object-language term;
- imported owner without graph export.

### 24.3 Artifact corruption tests

- selector owner changed;
- constructor ordinal out of range;
- field ordinal out of range;
- output and graph selectors paired from different calls;
- recursive marker placed on a nonrecursive field;
- selector classifier inconsistent with the constructor telescope;
- duplicate short selector accepted after read;
- selector table attached to the wrong graph association.

### 24.4 Authority tests

- equal Core Terms from two source definitions retain separate graph selectors;
- alpha-equivalent definitions do not share selector tables accidentally;
- named selection lowers to the same accepted ordinary Match as positional
  selection;
- no graph-specific Claim or Derivation kind is added;
- no new DefEq conversion is added;
- no source-origin record becomes a substitute for accepted typing evidence.

### 24.5 Performance tests

- no-request baseline remains unchanged;
- `:graph length` remains small and deterministic;
- `:graph quickSortAcc` generates only the required dependency closure;
- named selector lookup is constant or logarithmic per field after owner/case
  resolution;
- Artifact selector metadata growth is measured separately from graph term
  growth;
- imported display does not invoke source graph generation.

## 25. Rejected or postponed alternatives

### 25.1 Reverse compile an arbitrary finished Term

Rejected. TermDB does not retain the complete source occurrence, Context,
Substitution, branch, and public naming information required for a stable proof
interface.

### 25.2 Generate only `f.induct`

Rejected as the first interface. It hides the ordinary IADT foundation and does
not solve graph discoverability or imported source-origin naming by itself.

### 25.3 Add a general tactic language first

Postponed. Tactics need the same checked mapping and can be added later as
untrusted term construction.

### 25.4 Expose generated result families and `returned`

Rejected at the public surface. They are an internal encoding of a dependent
output/graph package, not a separate concept required by property authors.

### 25.5 Add unrestricted `@x` and `*x` introspection

Rejected. An arbitrary local value does not automatically own graph evidence or
an induction hypothesis.

The accepted narrower form is source-origin projection inside a validated
generated proof interface:

```text
resultName   exact named call output
@resultName  graph field paired with that output
*resultName  IH only if that graph field is recursive
```

Outside that scope, `@x` and `*x` do not become Java-style reflection or a way
to ask the compiler to manufacture a proof for an arbitrary value.

### 25.6 Invent field names from call order

Rejected as a stable public interface. `call0`, `call1`, or repeated
`recursiveOutput` labels are diagnostic fallbacks, not proof ABI selectors.

### 25.7 Treat normalized Core equality as graph ownership

Rejected. It would merge distinct source definitions and violate the explicit
nominal association already enforced by Artifact v84.

### 25.8 Make `:graph` return runtime data

Rejected. That would turn a static proof-interface inspection feature into
compiler reflection and raise unrelated staging, serialization, and safety
questions.

## 26. Open questions that do not block the first prototype

1. Should direct certified execution remain first-class storable, or become
   elimination-only at the surface?
2. What explicit stable-label syntax should decouple a public proof selector
   from alpha-renamable local binder text?
3. What qualified selector spelling best handles deeply nested Matches?
4. Should positional graph patterns eventually be discouraged for generated
   graphs while remaining valid for ordinary hand-written IADTs?
5. Should `:graph f` display generated helper graphs inline or as references
   expanded by a separate flag?
6. Should the internal result family be replaced by a CPS encoding in a later
   Artifact version?
7. How should partial and effectful function graphs expose traces, handlers,
   divergence, and may/must observations?

These questions should not delay the source-origin display prototype. FGSI1 and
FGSI2 can answer several of them using actual generated schemas.

## 27. Acceptance criteria

The proposal is successful when a programmer can perform this workflow:

```text
1. define f normally;
2. give stable block-binding names to proof-relevant call sites;
3. run :graph f and see the checked proof interface;
4. state a later theorem over @f inputs output;
5. eliminate its graph using named source-origin fields;
6. use ordinary *sourceBinding induction hypotheses;
7. apply the theorem to one actual run through direct *f elimination;
8. publish and consume the same proof interface across Artifacts.
```

For QuickSort, success specifically requires that the lower and upper recursive
calls are independently visible and independently usable without a positional
17-field pattern.

## 28. Final recommendation

A Program should keep the logical core of the current function-graph design:

```text
generated ordinary graph IADT
+ certified execution
+ ordinary dependent Match
+ ordinary local IH
```

The next implementation should improve the source boundary, not add another
proof theory:

```text
:graph f
    makes the checked interface visible

source-origin selectors
    make branches and call premises addressable

named graph cases
    remove positional proof fragility

*f arguments @ output graph => body
    hides the internal dependent result package
```

This gives later proofs access to the exact recursive structure of an already
defined function while preserving A Program's existing separation between:

- erased executable computation;
- occurrence-sensitive elaboration data;
- generated proof-relevant graph evidence;
- accepted Claims and Derivations; and
- optional source-level inspection.

It also leaves a clean boundary for future tactics: they may consume the same
validated graph interface, but they are not required for expressiveness and do
not become a new semantic authority.

## 29. Reserved-identifier sufficiency verdict

### 29.1 Result

Within the currently intended pure-total function-graph fragment, no new
object-language reserved identifier is theoretically necessary for post-hoc
properties.

The following name/sigil algebra is sufficient:

```text
f / @f / *f
    executable / graph family / certified execution

h / @h / *h
    higher-order callback / callback graph / certified callback

r / @r / *r
    named call output / its graph witness / its recursive property IH
```

Constructor cases continue to use the constructor names already present in the
source definition. Ordinary theorem parameters and local aliases remain chosen
by the theorem author. No fixed words such as `returned`, `output`, `graph`,
`recursiveOutput`, `recursiveGraph`, `LeGraph`, `Result`, or `induct` need to
enter the object-language namespace.

### 29.2 Necessary conditions

This conclusion holds only if all of the following are implemented:

1. Every proof-addressable call result has an explicit unique source Binding,
   normally created with existing `:=` block syntax.
2. Graph generation preserves that Binding as the common origin of `r` and
   `@r`.
3. Ordinary graph elimination exposes `*r` only when `@r` is an actual
   recursive graph premise under the current motive.
4. Higher-order parameters receive scoped `h`/`@h`/`*h` companion roles.
5. The origin-role table is serialized and validated for imported proof
   interfaces.
6. Resolution is based on syntax, Binding identity, and source-origin role,
   not guessed from an expected type.

### 29.3 Current implementation verdict

Revision `14b51b1` does not yet satisfy these conditions:

- graph constructors are ordinary IADTs and are already eliminable;
- global `@f` and `*f` already exist;
- constructor labels already preserve source case symbols;
- block-bound recursive results compile as executable functions;
- requesting a graph for the tested block-bound recursive result currently
  fails during function-graph generation;
- recursive/helper graph fields still use synthetic internal names;
- generic higher-order graphs currently expose a separately named graph-family
  parameter such as `LeGraph`; and
- Artifact v84 has no source-origin selector table.

Therefore the answer is:

> The name/sigil design can eliminate new reserved identifiers without losing
> the ordinary function-graph proof method, but the current compiler has not
> implemented the source-origin and higher-order companion machinery needed to
> make that statement true today.

### 29.4 Boundary of the claim

This interface makes additional properties expressible. It does not make every
property true or automatically provable. QuickSort sortedness still requires
comparator laws, partition lemmas, and list-combination lemmas. A partial or
effectful definition still requires a future graph semantics for divergence and
effects. Those are missing mathematical or semantic inputs, not missing
reserved words.

Finally, `:graph f` uses `graph` in a command namespace. It does not reserve the
identifier `graph` in A Program source. If even command names must be
symbol-only, a spelling such as `:@f` could be considered, but it provides no
object-language expressiveness benefit and is not required by this proposal.

## 30. Local Binding and Owner-Relative Namespace Audit

### 30.1 Current locality result

Source binders such as the QuickSort worker's:

```text
down
lowerSize
lower
upperSize
upper
lowerBound
upperBound
partitioned
```

are not current A Program GlobalNames.

The reader maintains a lexical `local_binder` chain. A local identifier resolves
to its AST Binder while that Binder is in scope. After the Match case, Lambda,
or computation block closes, the local binding is removed from the resolution
environment.

Two concrete checks at revision `14b51b1` established:

1. two separate functions can both use Match binders named `lowerSize` and
   `down` without collision; and
2. a top-level `leaked := down; leaked :: Nat;` after the defining Match fails
   because no local or accepted global definition supplies `down`.

The QuickSort source readback exports top-level terms such as `partition`,
`quickSortAcc`, and `quickSort`. It does not export terms named `down`,
`lowerSize`, `lower`, `upper`, `lowerBound`, or `upperBound`.

### 30.2 A global symbol ID is not a GlobalName

The lexer interns identifier spellings in one symbol table. Therefore two local
binders spelled `down` may share one integer symbol ID used for display and
lookup acceleration.

This does not make `down` a global semantic binding. The semantic identity of a
local occurrence includes its lexical AST Binder and later its accepted
Binding/occurrence identity. A top-level definition has a separate assignment
and export identity.

The source-origin implementation must preserve this distinction. It must never
publish a local merely because its display spelling has a global symbol-table
entry.

### 30.3 Generated graph fields are also local telescope binders

The function-graph generator currently copies some source symbols into
generated constructor field symbols. Those fields belong to one generated
constructor telescope. They are introduced only while eliminating that
constructor.

They are not top-level term assignments. The proposed `resultName`,
`@resultName`, and `*resultName` roles must remain local to:

```text
one owner graph
one graph constructor case
one source-origin Binding
one elimination scope
```

Thus defining QuickSort must not create global terms named:

```text
lowerResult
@lowerResult
*lowerResult
```

Those spellings become meaningful only inside a case which eliminates an
instance of the associated QuickSort graph.

### 30.4 Recommended use of the `qsort` namespace

It is reasonable to say that the proof interface is owned by `qsort`, but the
owned entries should be selector metadata rather than Global Terms.

Conceptually the metadata is nested as follows:

```text
qsort
  graph association
    case @cons
      lowerSize          -> constructor field identity
      lowerResult        -> recursive output field identity
      @lowerResult       -> paired recursive graph field identity
      *lowerResult       -> IH projection derived during elimination
      upperResult        -> recursive output field identity
      @upperResult       -> paired recursive graph field identity
      *upperResult       -> IH projection derived during elimination
```

This is an owner-relative table. It need not create the source names:

```text
qsort.lowerSize
qsort.lowerResult
qsort.upperResult
```

as object-language terms.

The graph scrutinee already determines the owner:

```a-program
proof := \input : List A => \output : List A =>
	\run : @qsort A @le input output =>
		run
			@cons {
				lowerSize;
				lowerResult;
				@lowerResult;
				upperResult;
				@upperResult;
			} => combine *lowerResult *upperResult;
```

When resolving `lowerResult`, the compiler already knows:

```text
owner     = qsort, from the scrutinee classifier
case      = @cons, from the active clause
selector  = lowerResult, from the source-origin table
role      = value, graph, or IH, from the sigil and syntax position
```

No global lookup for `lowerResult` is needed.

### 30.5 Where qualification may still be useful

Owner qualification is useful for static inspection, diagnostics,
serialization, and IDE navigation:

```text
:graph qsort
qsort / @cons / lowerResult
qsort / @cons / @lowerResult
```

These paths identify metadata. They are not necessarily legal A Program terms.
They can disambiguate nested branches or two source Bindings with the same
display name.

If a future surface form needs explicit qualification, it should resolve an
owner-relative selector, not promote the selected local to a Global Term. For
example, a diagnostic path conceptually equivalent to:

```text
qsort/@cons/lowerResult
```

is safer than defining a global value `qsort.lowerResult`, because the latter
would be ill-scoped: its type depends on `pivot`, `lowerSize`, `lower`, the
partition bounds, and other branch-local values.

### 30.6 Artifact representation

Artifact publication should keep:

1. the existing global owner export for `qsort`;
2. the existing explicit association to the generated graph family and
   certified runner; and
3. one nested selector table keyed by owner, constructor, and field identity.

The selector table serializes local roles relative to the owner association.
It does not append every source Binder to the global term-export table.

An importer first resolves global `qsort`, validates its graph association, and
only then resolves case-local selectors while checking an `@qsort` graph
elimination. The imported selector spelling remains display metadata; the
validated owner/constructor/field identity is authority.

### 30.7 Namespace verdict

The recommended answer is therefore:

> Keep `qsort` and explicitly exported theorems global. Keep `down`,
> `lowerSize`, `lowerResult`, and their sigil projections local. Let `qsort` own
> a nested proof-interface selector table, but do not materialize its selectors
> as GlobalNames.

This provides the organizational benefit of a `qsort` namespace without
polluting the global term namespace or attempting to turn dependent
branch-local data into closed global terms.
