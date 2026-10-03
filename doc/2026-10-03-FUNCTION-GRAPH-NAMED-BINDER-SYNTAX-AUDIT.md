# Problem List

## 1. Publication context: Function Graph named-binding design proposal

### Subjective (User)

2026-10-03, English paraphrase: the project owner supplied this document and
requested current-head verification, then explicitly authorized Issues and a
new documentation PR. Implementation changes are outside this submission.

### Objective (Code)

Verification revision: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
Fresh executions at eb0aad6 reproduce source := local on accepted source and
latest solver_inputs; local := source rejects and double-brace named sets do not
parse. Existing wrong-proof controls still reject. This is a confirmed surface
design inconsistency, not a demonstrated kernel-unsoundness defect.
Polarity correction and introduction of ordered/unordered brace modes are
separate design decisions; publication does not automatically approve every
proposed normative rule.

### Assessment

The supplied body below is preserved verbatim as author-supplied material.
Its assertions, historical reports and proposals must be read together with
[the revision-pinned current-head review](2026-10-03-NEW-AUDITS-CURRENT-HEAD-REVIEW.md).
That review controls what this submission independently verifies.

### Plan

- [x] Preserve the complete supplied document, without silently rewriting it.
- [x] Attach current-head qualifications and separate implementation boundaries.
- [ ] Complete the follow-up work in [Issue #57](https://github.com/repyt-margorp/a-program/issues/57).

---

# A Program Function Graph Named Binder Surface Syntax Audit

**Date:** 2026-10-03 JST  
**Target:** current `main` pointer-core implementation, with historical audit back to PR #21  
**Status:** design audit / corrective proposal; no implementation performed in this document  
**Primary current files:** `src/syntax.c`, `src/synthesis.c`  
**Historical introduction:** PR #21 design documents → commit `4fe5aee` (`Implement PR21 function graph surface`)  
**Recommended correction:** reverse the current `selector := local` spelling to the ordinary A Program binding direction `local := selector`, and split ordered/unordered graph-case binding forms into `{...}` and `{{...}}`.

---

## 0. Executive conclusion

The current Function Graph named-case notation contains a surface-language design error.

The problematic current form is conceptually:

```ap
graph
    @cons {
        head := h;
        tail := t;
    } => body;
```

The implementation interprets this as:

```text
source selector "head"  -> introduce local binder "h"
source selector "tail"  -> introduce local binder "t"
```

This is the reverse of the ordinary A Program meaning of `:=`.

Elsewhere in A Program:

```ap
x := expression;
```

means:

```text
introduce/bind x on the left
to the value/expression on the right
```

The Function Graph syntax instead made the left side a pre-existing lookup key and the right side the newly introduced local binder. That inversion is not merely a naming inconvenience. It changes the binding polarity of `:=` in one special surface construct.

The historical audit shows that this was **not introduced accidentally by the parser**. It entered the design itself in the PR #21 proposal:

```text
selector := localName
```

The following-day critical review retained the same orientation:

```ap
lowerResult := lower;
```

Commit `4fe5aee` then implemented exactly that design. The parser stored the first identifier as `source_symbol_id` and the identifier after `:=` as `local_symbol_id`. Its permanent fixture used:

```ap
tailLength := recursive;
```

and then referred to:

```ap
@recursive
```

in the branch body.

The current pointer-core rewrite still preserves the same logical direction. In current `src/syntax.c`, a named selector item is parsed as:

```text
item.name              = source selector
item.expression->token = local alias
```

and current `src/synthesis.c` resolves `item.name` against Function Graph source metadata, then introduces `item.expression->token` into lexical scope.

Therefore this audit recommends treating the problem as a **surface specification correction**, not as a compatibility-preserving spelling tweak.

The corrected direction should be:

```ap
graph
    @cons {{
        h := head;
        t := tail;
    }} => body;
```

Read literally:

```text
bind local h to the graph source field selected by head
bind local t to the graph source field selected by tail
```

For a recursive result origin group:

```ap
graph
    @cons {{
        recursive := tailLength;
    }} => @recursive;
```

The local binder is always on the left. The pre-existing Function Graph source selector is always on the right.

This document further recommends two braced forms:

```text
{{ ... }}   unordered / declarative named binding set
{  ...  }   ordered named binding list
```

with **`{{...}}` as the recommended/default style for generated Function Graph elimination**.

This fits an already-existing A Program visual distinction:

- ordinary `{ ... }` blocks preserve source-order computation;
- program-wide `{{ ... }}` definition blocks establish a name set rather than a sequential computation chain.

The Function Graph form should reuse that distinction rather than invent another one.

---

## 1. Scope

This audit addresses one narrow but important surface-language boundary:

> How should a proof bind selected fields/origin groups while eliminating a compiler-generated `@function` graph?

It does **not** propose changing:

- the semantic meaning of `@function`;
- the generated Function Graph IADT itself;
- constructor telescope order in Core;
- ordinary positional ADT/IADT patterns;
- ordinary recursive-field induction hypotheses such as `*tail`;
- graph adequacy;
- kernel Match semantics;
- definitional equality;
- graph evidence representation.

The proposed change belongs at the source parser/elaborator boundary.

The intended invariant is:

> Named Function Graph patterns are only sugar for selecting and locally renaming fields from a complete generated constructor telescope. The kernel continues to see the complete ordered Match structure.

---

## 2. Current semantic situation

Consider:

```ap
NatList := @{
    nil  : *;
    cons : Nat -> * -> *;
};

length := \xs : NatList =>
    xs
        @nil => Nat.zero
        @cons head tail => {
            tailLength := *tail;
            Nat.succ tailLength;
        };
```

In the source definition of `length`:

```ap
@cons head tail
```

is an ordinary positional pattern.

`head` and `tail` are local names introduced at the two constructor positions. They are not globally declared record-field labels.

When the compiler generates `@length`, it preserves selected source-origin metadata so that later proofs can address meaningful fields without remembering the entire generated positional telescope.

Historically, the named graph-case feature allowed:

```ap
graph
    @cons {
        tailLength := recursive;
    } => @recursive;
```

The intended semantic mapping was:

```text
source origin: tailLength
local name:    recursive
```

That is useful functionality. The error is the surface orientation of `:=`.

---

## 3. Why the present direction is inconsistent

### 3.1 Ordinary A Program assignment/binding polarity

The ordinary block syntax is:

```ap
{
    x := M;
    y := N;
    result;
}
```

Its lexical reading is conventional and stable:

```text
new binder := source expression
```

The left side is introduced.

The right side already denotes something.

Current `src/syntax.c` implements block bindings in this direction as well: it reads the identifier as the binding `name`, then parses the expression after `:=`.

### 3.2 Current graph-case polarity is reversed

Current named Function Graph parsing instead behaves as:

```text
existing source selector := new local binder
```

For example:

```ap
tailLength := recursive;
```

means:

```text
look up source-origin tailLength
and create local binder recursive
```

The notation therefore visually claims the opposite of what lexical binding actually occurs.

### 3.3 The problem is semantic readability, not merely taste

A programmer seeing:

```ap
head := h;
```

naturally asks:

> What value is `h`, and why am I assigning it into `head`?

But the implementation does the reverse:

> Find the already-known source field `head`, then introduce `h`.

This is exactly the kind of notation that becomes difficult to reason about later because the programmer must remember a construct-specific exception to the language's binding direction.

### 3.4 The implementation itself proves which side is the binder

The historical parser introduced in `4fe5aee` used explicit fields:

```c
selector->source_symbol_id = first_identifier;
selector->local_symbol_id  = first_identifier;

if (accept(TOKEN_ASSIGN)) {
    selector->local_symbol_id = identifier_after_assign;
}
```

It then pushed `local_symbol_id` into the lexical binder environment.

Thus the RHS was objectively the introduced binder.

The current pointer-core implementation represents the same relationship differently, but preserves the same polarity:

```text
syntax item .name              -> source selector
syntax item .expression->token -> local alias
```

The synthesizer resolves `.name` against the Function Graph case layout and binds the alias.

The current surface spelling is therefore inconsistent with its own binding behavior.

---

## 4. Historical audit: where the mistake entered

### 4.1 Before PR #21

Before the named Function Graph proof interface, generated graph elimination was positional.

For nontrivial functions such as QuickSort, this could expose a long generated telescope. The PR #21 design correctly identified the usability problem: users should be able to select source-relevant graph fields without memorizing generated field positions.

That motivation was sound.

### 4.2 2026-08-24 design proposal

The design document:

```text
doc/2026-08-24T19-13-51-FUNCTION-GRAPH-SOURCE-ORIGIN-SURFACE-AND-INSPECTION-PLAN.md
```

specified:

```text
name;
selector := localName;
```

and explicitly stated that selection order did not affect constructor field order.

This is the earliest audited point where the reversed `:=` convention is explicitly frozen as intended syntax.

The document correctly understood the semantic distinction:

- one source selector identifies an origin group;
- omitted fields remain in the constructor telescope;
- named selection is surface sugar;
- the Match still lowers through the complete generated telescope.

The direction of `:=` was the mistake.

### 4.3 2026-08-25 critical review

The next critical-review document did revisit the overall graph interface. It simplified origin-group selection and clarified that one selected recursive result should expose related roles such as:

```text
value
@graph
*IH
```

However it retained:

```ap
lowerResult := lower;
```

and described this as aliasing `lowerResult` to local `lower`.

Thus the review corrected several deeper graph-interface issues but did **not** catch the binding-direction inconsistency.

This is important for process auditing: the mistake was reviewed and normalized before implementation.

### 4.4 Implementation commit `4fe5aee`

Commit:

```text
4fe5aee  Implement PR21 function graph surface
```

has parent:

```text
e379c74  pre-implementation critical review state
```

and is the clear implementation boundary.

The commit changed 53 files and marked FGSI0–FGSI6 as implemented and verified.

Among the implemented pieces were:

- AST named selector structures;
- parser support;
- source-origin metadata;
- named graph case elaboration;
- role companions;
- artifact selector metadata in the legacy implementation;
- negative tests;
- named-selector fixtures;
- QuickSort proof-facing machinery.

The parser implementation exactly follows the design document's reversed direction.

### 4.5 Permanent fixture confirms intended old behavior

The implementation fixture:

```text
src/prototype/tests/fixtures/typing/function_graph_named_case_check.p
```

contained:

```ap
selectGraph := \input : NatList => \output : Nat =>
    \graph : @length input output =>
    graph
        @cons {
            tailLength := recursive;
        } => @recursive;
```

This is definitive evidence that the old spelling was deliberately tested as:

```text
source := local
```

rather than entering accidentally through an AST serialization detail.

### 4.6 Current `main`

The current implementation was rewritten/promoted to pointer core, but `src/syntax.c` still parses:

```ap
@case {
    source := alias;
}
```

as:

```text
name  = source
alias = alias
```

and `src/synthesis.c` still resolves `name` against Function Graph case metadata before introducing `alias`.

Thus the historical syntax bug survived architecture replacement.

---

## 5. What can and cannot be established about AI authorship

The repository history establishes with high confidence:

1. the reversed notation first appears as an explicit design rule;
2. it survives a critical review;
3. it is implemented in `4fe5aee`;
4. it is covered by permanent fixtures;
5. it remains semantically present in current `main`.

The repository alone does **not** provide a reliable forensic basis for determining which individual human or model invented the exact token orientation.

The historical branch/PR naming and the project-development context are compatible with an AI-assisted design process, and the project owner reports that this surface work was delegated to AI. That contextual conclusion can be recorded separately.

For repository audit purposes, the stronger and sufficient finding is:

> The error was introduced at the **design-specification stage**, not merely at implementation.

That is the relevant lesson for preventing recurrence.

---

## 6. Existing `{...}` / `{{...}}` semantics provide the right correction vocabulary

Current A Program already distinguishes single and double braces.

### 6.1 `{...}`

Current README describes:

```ap
{ x := M; N; }
```

as a sequential block.

Current parser comments explicitly say:

```text
ordinary blocks preserve source-order statements
```

The order is therefore semantically meaningful.

### 6.2 `{{...}}`

Current README describes:

```ap
{{ name := expression; ... }}.name
```

as a program-wide definition block that names a graph root rather than executing sequential bindings.

Current synthesis first registers definition names across the definition set and only afterwards activates their producers.

This is not identical to a Function Graph binder set, but it establishes a strong visual convention:

```text
{  ...  }  -> ordered/sequential source structure
{{ ... }}  -> name-oriented set/module structure
```

### 6.3 Design consequence

The Function Graph selector syntax should reuse this distinction.

Recommended:

```text
{{ ... }}  unordered declarative named selection
{  ...  }  ordered named selection
```

This avoids adding a fresh keyword and makes the punctuation carry the same broad language-level intuition it already has.

---

## 7. Proposed corrected surface syntax

### 7.1 Recommended unordered form

```ap
graph
    @cons {{
        h := head;
        t := tail;
    }} => body;
```

Meaning:

```text
local binder h := source selector head
local binder t := source selector tail
```

The order of the entries inside `{{...}}` has no semantic meaning.

Therefore this is equivalent:

```ap
graph
    @cons {{
        t := tail;
        h := head;
    }} => body;
```

### 7.2 Recursive-result origin-group example

Suppose the source function contains:

```ap
@cons head tail => {
    tailLength := *tail;
    Nat.succ tailLength;
}
```

Then a later graph proof should be able to write:

```ap
graph
    @cons {{
        recursive := tailLength;
    }} => @recursive;
```

The interpretation is:

```text
recursive  := value role of source origin tailLength
@recursive := paired graph role, when present
*recursive := IH for that paired graph role, when recursive
```

This preserves the useful "one origin group, multiple local roles" design from the 2026-08-25 review while fixing assignment polarity.

### 7.3 Identity binding

An identity mapping may remain available as shorthand:

```ap
@cons {{
    head;
    tail;
}} => ...
```

with exact desugaring:

```ap
@cons {{
    head := head;
    tail := tail;
}} => ...
```

This shorthand should be documented as such. It must not obscure the fundamental direction:

```text
local := source
```

### 7.4 Ordered form

The single-brace form should mean an ordered named binding list:

```ap
graph
    @cons {
        h := head;
        t := tail;
    } => body;
```

The bindings are still resolved by source selector names, but their written order is required to agree with the selected origin groups' canonical order in the generated constructor telescope.

For example, if the canonical source-related order is:

```text
head
tail
tailLength
...
```

then:

```ap
@cons {
    h := head;
    t := tail;
} => ...
```

is valid, while:

```ap
@cons {
    t := tail;
    h := head;
} => ...
```

should be rejected as an ordered-selector order violation.

The unordered `{{...}}` form accepts either order.

---

## 8. Important semantic qualification: `{{...}}` is not unordered Core

The generated constructor telescope remains ordered.

Dependent constructor fields may refer to earlier fields. Hidden omitted fields may also be needed to type later visible fields.

Therefore the implementation must **not** literally introduce Core/Context binders in the arbitrary textual order used inside `{{...}}`.

Instead:

1. parse the whole `{{...}}` set;
2. resolve every RHS source selector against the owner/case metadata;
3. reject unknown/duplicate selectors and duplicate local names;
4. recover each selected field/origin group's canonical telescope ordinal;
5. create the complete constructor telescope, including hidden omitted fields;
6. sort/expand visible bindings into canonical telescope order;
7. extend the dependent Context in that canonical order;
8. attach the requested local aliases to the selected visible roles;
9. lower to the ordinary complete Match.

Thus:

```text
unordered
```

means only:

> source listing order does not determine semantic field order.

It does **not** mean:

> dependent binders form an unordered mathematical context.

This distinction should be explicit in the specification.

---

## 9. Ordered `{...}` semantics

The ordered form should be defined as a stricter source contract.

For each entry:

```ap
local := source;
```

the elaborator resolves `source` to a canonical selected origin/field ordinal.

The resulting ordinals must be strictly increasing in written order.

This yields a clean interpretation:

```text
{ ... }  = "I am writing these selections in the graph telescope's semantic order."
```

Omitted fields remain legal and become hidden binders exactly as in the unordered form.

Therefore `{...}` is an ordered subsequence, not necessarily a complete enumeration.

This matters because a full generated graph may have many proof-internal fields the user intentionally does not name.

---

## 10. Why `{{...}}` should be recommended for Function Graph proofs

Generated Function Graphs are compiler-derived proof interfaces.

The exact constructor telescope may contain:

- source pattern fields;
- generalized/trailing arguments;
- recursive-call result values;
- paired graph witnesses;
- helper-call data;
- hidden fields required by dependency;
- future generated proof metadata.

A proof normally cares about semantic source origins, not their current generated physical order.

Therefore:

```ap
@case {{
    localA := semanticSourceA;
    localB := semanticSourceB;
}}
```

communicates the correct abstraction boundary:

> select these source-origin roles by identity; let the elaborator respect the generated telescope.

This is more stable under implementation changes than making field order part of ordinary proof source.

Recommended style rule:

> **Use `{{...}}` for generated Function Graph cases unless the proof intentionally wants to assert canonical field order.**

The single-brace ordered form remains useful as:

- a low-level/inspection-oriented form;
- a migration bridge from positional reasoning;
- an executable specification test for graph-interface ordering;
- a debugging aid.

---

## 11. Relationship to ordinary positional patterns

Ordinary ADT/IADT patterns should remain:

```ap
value
    @cons head tail => ...
```

No named-field semantics need to be added to every datatype.

Likewise the low-level positional Function Graph form can remain available where useful:

```ap
graph
    @cons head tail recursiveResult recursiveGraph => ...
```

The three forms then have distinct purposes:

```text
@case a b c
    raw positional binding

@case { local := source; ... }
    ordered source-named binding

@case {{ local := source; ... }}
    unordered/declarative source-named binding
```

For generated Function Graph user code, the third should be preferred.

---

## 12. Proposed grammar

Conceptually:

```text
elimination_clause
    ::= "@" case_label positional_binders "=>" term
     |  "@" case_label ordered_graph_bindings "=>" term
     |  "@" case_label unordered_graph_bindings "=>" term

ordered_graph_bindings
    ::= "{" graph_binding+ "}"

unordered_graph_bindings
    ::= "{{" graph_binding+ "}}"

graph_binding
    ::= local_name ":=" source_selector ";"
     |  identity_name ";"

source_selector
    ::= IDENT
```

At the current corrected origin-group abstraction, `source_selector` should remain the unsigiled source-origin name.

Role access belongs in the branch body:

```text
local
@local
*local
```

when those roles are valid.

Do not reintroduce separate `@sourceSelector` entries into the selector set unless a later design demonstrates a need. The 2026-08-25 review already improved this by selecting one origin group once.

---

## 13. Parser changes required in current `src/syntax.c`

Current code conceptually does:

```c
name = first_identifier;       /* source selector */
alias = name;
if (":=") alias = next_ident;  /* local binder */
```

It then stores:

```text
item.name       = source
item.expression = local
```

This should be reversed at the source AST boundary.

Preferred conceptual representation:

```text
item.name       = local binder
item.expression = source selector
```

For:

```ap
h := head;
```

the AST should mean:

```text
local  = h
source = head
```

not the reverse.

### 13.1 Do not rely on field names such as `alias`

The historical implementation terminology (`selector alias`) helped hide the polarity problem.

Use explicit names in parser/elaborator state:

```text
local_name
source_selector
```

Avoid generic `name`/`alias` where possible.

### 13.2 Represent ordered/unordered mode explicitly

Do not infer the mode later from incidental source spans.

The clause or binder-set AST should retain:

```text
GRAPH_BINDINGS_ORDERED
GRAPH_BINDINGS_UNORDERED
```

or equivalent.

The parser already recognizes doubled braces for program-root definitions through two consecutive `{` tokens. No fundamentally new lexical token is required.

---

## 14. Synthesis/elaboration changes required in current `src/synthesis.c`

Current named-case synthesis approximately does:

1. read `item->name` as source selector;
2. find a matching `source->fields[field].name`;
3. read `item->expression->token` as local alias;
4. bind that local alias;
5. preserve graph companion/IH relationships.

After the correction:

1. read RHS source selector from the explicit source field of the AST;
2. resolve it against the owner/case `source_case_layout`;
3. read LHS as the local binder;
4. reject duplicate local binders;
5. reject duplicate source-origin selection;
6. retain the existing complete-telescope expansion;
7. retain hidden omitted bindings;
8. retain `graph_value`/companion relationships;
9. bind local, `@local`, and valid `*local` roles consistently;
10. apply ordered/unordered validation before lowering.

The Function Graph semantic metadata itself need not change merely because the surface assignment direction changes.

---

## 15. Source metadata should remain source metadata

Current `function_graph_order()` correctly records source-origin names separately from generated field positions.

For ordinary source match fields it maps the source clause binder name into the generated case layout.

For recursive calls whose result is explicitly bound in a source block, it can preserve that result name as an origin name.

This is the right general architecture.

The correction should **not** turn source selectors into ordinary record fields or mutate the generated IADT's semantic identity.

The source selector remains:

```text
proof-facing metadata owned by one function graph + one case
```

The local binder remains:

```text
lexical scope introduced by this elimination branch
```

Keeping these concepts separate makes the corrected notation natural:

```text
local := source
```

---

## 16. Artifact and saved-image implications

The old prototype's artifact v86 serialized validated owner/case/field selector metadata.

The local alias was a surface-elaboration choice.

The current pointer-core program-image architecture is different from that legacy artifact format, but the same separation should be retained:

- persistent semantic identity: owner / case / field or origin group;
- source-facing selector spelling: metadata used to address that identity;
- branch-local alias: not semantic artifact identity.

Therefore the direction flip should not require changing kernel graph identity.

If a persistent source AST is serialized, the syntax version must of course distinguish old and new source forms. But the accepted graph relation itself should not change because:

```ap
old: source := local
new: local := source
```

select the same owner-relative semantic field after migration.

---

## 17. Backward compatibility: do not support both directions implicitly

This is one of the strongest conclusions of the audit.

Suppose both were accepted:

```ap
a := b;
```

Old interpretation:

```text
source a -> local b
```

New interpretation:

```text
local a <- source b
```

If both `a` and `b` are valid source selectors, the parser/elaborator cannot determine which interpretation the programmer intended.

A heuristic such as:

> If LHS resolves as a source field, assume legacy syntax.

is unsafe because a perfectly valid new local name may happen to equal another source selector.

Therefore the language should **not** silently support both directions.

Recommended migration policy:

1. make a hard source-syntax change;
2. update all repository fixtures/docs/examples atomically;
3. optionally provide a one-shot source migrator or diagnostic tool;
4. do not make normal compilation semantics depend on a guess.

A temporary explicit compatibility flag would be technically possible, but it should not become a permanent dual grammar.

---

## 18. Migration diagnostics

A useful targeted diagnostic may still be possible without changing semantics.

When compiling the new grammar:

```ap
foo := bar;
```

if:

- RHS `bar` is not a valid selector;
- LHS `foo` is a valid selector;

the compiler can report:

```text
unknown graph source selector 'bar';
'foo' is a valid selector.
This source may use the pre-2026-10 graph-binding direction.
Write: bar := foo;
```

This is only a diagnostic suggestion.

It must not auto-swap the program during normal compilation.

If both sides are valid selectors, the compiler must follow the new grammar exactly.

---

## 19. Required positive tests

At minimum, add current-pointer-core tests for the following.

### 19.1 Basic unordered rename

```ap
@cons {{
    h := head;
    t := tail;
}} => ...
```

passes.

### 19.2 Unordered permutation

```ap
@cons {{
    t := tail;
    h := head;
}} => ...
```

has the same typing/elaboration result as the previous test.

### 19.3 Identity shorthand

```ap
@cons {{
    head;
    tail;
}} => ...
```

is equivalent to:

```ap
@cons {{
    head := head;
    tail := tail;
}} => ...
```

### 19.4 Recursive origin-group rename

Given source:

```ap
tailLength := *tail;
```

proof:

```ap
@cons {{
    recursive := tailLength;
}} => @recursive
```

passes.

### 19.5 Recursive IH under renamed group

Where the graph role is recursive:

```ap
@cons {{
    recursive := tailLength;
}} => *recursive
```

resolves to the ordinary graph-field induction hypothesis.

### 19.6 Ordered valid subsequence

```ap
@cons {
    h := head;
    recursive := tailLength;
} => ...
```

passes when source ordinals are increasing.

### 19.7 Ordered omitted fields

The ordered form still works when hidden fields occur between selected visible origins.

### 19.8 Complete-telescope dependency

A selected later dependent field type-checks even when its dependency is omitted from user-visible bindings, proving that hidden canonical binders are inserted correctly.

---

## 20. Required negative tests

### 20.1 Ordered reversal

```ap
@cons {
    t := tail;
    h := head;
} => ...
```

fails if `head` canonically precedes `tail`.

### 20.2 Unknown RHS selector

```ap
@cons {{
    x := doesNotExist;
}} => ...
```

fails with a source-selector diagnostic.

### 20.3 Duplicate source selection

```ap
@cons {{
    x := head;
    y := head;
}} => ...
```

fails.

### 20.4 Duplicate local binder

```ap
@cons {{
    x := head;
    x := tail;
}} => ...
```

fails.

### 20.5 Shadowing active local

Retain the existing rule against silently shadowing an active incompatible branch binder, unless the language-wide shadowing policy changes separately.

### 20.6 Invalid graph companion role

`@local` fails when the selected origin has no graph role.

### 20.7 Invalid IH role

`*local` fails when the selected graph role is not recursive.

### 20.8 Legacy direction is not silently accepted

A fixture written in the old orientation must fail under the new grammar unless it also happens to be a valid new-orientation program.

A dedicated migration test should verify the diagnostic.

### 20.9 Ambiguous two-valid-selector case

If both sides are real selectors:

```ap
head := tail;
```

the compiler must interpret only:

```text
local head := source tail
```

under the new grammar.

It must never infer old syntax from source-name availability.

---

## 21. Equivalence tests between `{...}` and `{{...}}`

For any mapping whose ordered entries already follow canonical field order:

```ap
@cons {
    h := head;
    t := tail;
}
```

and:

```ap
@cons {{
    h := head;
    t := tail;
}}
```

should elaborate to alpha-equivalent complete Match structure.

The only intended difference is source acceptance:

- `{...}` checks order;
- `{{...}}` ignores written order and canonicalizes.

This should be tested at the elaborated evidence/term boundary, not only by final normalization output.

---

## 22. Why not use `as` instead?

A syntax such as:

```ap
head as h
```

would have been clearer than the current historical `head := h`.

However, once the project already has a stable and intuitive assignment convention:

```text
lhs := rhs
```

the smaller language is obtained by following it consistently:

```ap
h := head
```

No new keyword is needed.

The issue is not that aliasing requires a special notation. It is that the existing assignment notation was used backwards.

---

## 23. Why not call these record fields?

The named Function Graph source selectors are not ordinary record labels.

Their origin includes:

- positional source pattern binder names;
- source block-bound recursive result names;
- graph companion relationships generated from those origins.

A normal ADT constructor can remain positionally declared and positionally matched.

The Function Graph mechanism is better described as:

```text
owner-relative source-origin selector metadata
```

The corrected syntax does not change that architecture.

It only makes lexical binding read correctly.

---

## 24. Proof-ABI stability issue exposed by this audit

There is a related but separate concern.

If a source function changes:

```ap
@cons head tail => ...
```

to:

```ap
@cons x xs => ...
```

while preserving identical computation, the generated proof selector names may change.

Therefore source-origin selector spellings form part of the graph-facing proof ABI unless the compiler provides a more stable explicit naming mechanism.

This is not a reason to retain the backwards `:=`.

But documentation should distinguish:

```text
semantic graph field identity
```

from:

```text
human source selector spelling
```

and state whether source alpha-renaming is expected to be proof-API breaking.

For now, the simplest honest policy is:

> Function Graph proofs that address source-origin names are intentionally intensional and may require updates when proof-facing source binder names change.

A later explicit proof-label mechanism could stabilize selected names if needed.

---

## 25. Current documentation gap

Current README documents:

- positional Match;
- `{...}` sequential blocks;
- `{{...}}` definition blocks;
- generated `@function` graph types.

It does not currently document the named Function Graph selector syntax implemented in `src/syntax.c` / `src/synthesis.c`.

That gap likely contributed to the present confusion: the feature can survive internally without a current surface-language explanation.

The correction should therefore include a concise public README section, not only tests and an implementation patch.

Recommended README-level example:

```ap
property := \xs : List => \n : Nat => \graph : @length xs n =>
    graph
        @nil => ...
        @cons {{
            h := head;
            t := tail;
        }} => ...;
```

with:

```text
Double braces select Function Graph source-origin fields by name; entry order
is irrelevant. The left side is the local branch binder and the right side is
the source selector. Single braces require selector order to follow the generated
constructor telescope. Positional patterns remain available.
```

---

## 26. Recommended implementation order

### Phase 1 — freeze the corrected language contract

Before code changes, add a short design decision document containing:

```text
local := source
{{...}} unordered and recommended
{...} ordered
positional patterns unchanged
no implicit dual-direction compatibility
complete telescope preserved
```

### Phase 2 — parser/AST correction

- reverse local/source storage;
- add ordered/unordered binder-set mode;
- preserve identity shorthand;
- improve names in AST/parser structures.

### Phase 3 — elaborator correction

- resolve RHS source selectors;
- canonicalize unordered mappings;
- validate ordered mappings;
- preserve hidden complete telescope;
- preserve origin-group graph/IH roles.

### Phase 4 — tests

Add the positive, negative and equivalence matrix above before mass source migration.

### Phase 5 — repository migration

Update:

- current pointer-core fixtures;
- compatibility fixtures intended to stay supported;
- examples;
- active docs;
- training material if it describes this feature.

Historical archived documents should generally remain unchanged and be marked historical rather than rewritten, because they are evidence of the old design.

### Phase 6 — public documentation

Add current README grammar and examples.

---

## 27. Files/areas to inspect during implementation

Current pointer core:

```text
src/syntax.c
src/synthesis.c
src/function_graph.c
src/iadt.c
tests/
README.md
doc/
```

Historical evidence:

```text
doc/2026-08-24T19-13-51-FUNCTION-GRAPH-SOURCE-ORIGIN-SURFACE-AND-INSPECTION-PLAN.md
doc/2026-08-25T06-20-56-PR21-FUNCTION-GRAPH-SURFACE-CURRENT-CRITICAL-REVIEW-AND-CORRECTION-PLAN.md
```

Legacy implementation introduction:

```text
commit 4fe5aee  Implement PR21 function graph surface
src/prototype/src/frontend/reader.c
src/prototype/tests/fixtures/typing/function_graph_named_case_check.p
```

Do not treat the legacy prototype's artifact-v86 machinery as current pointer-core architecture. It is historical evidence for the syntax's origin.

---

## 28. Concrete old/new translation table

| Purpose | Historical/current direction | Corrected direction |
|---|---|---|
| Rename source `head` to local `h` | `head := h;` | `h := head;` |
| Rename source `tail` to local `t` | `tail := t;` | `t := tail;` |
| Rename recursive result `tailLength` to `recursive` | `tailLength := recursive;` | `recursive := tailLength;` |
| Same-name selection | `head;` | `head;` = `head := head;` |
| Recommended named set | `{ ... }` | `{{ ... }}` |
| Ordered named list | not distinguished | `{ ... }` |
| Raw positional | `@cons h t ...` | unchanged |

---

## 29. Canonical `length` example after correction

Source function:

```ap
Nat := @{
    zero : *;
    succ : * -> *;
};

List := @{
    nil : *;
    cons : Nat -> * -> *;
};

length := \xs : List =>
    xs
        @nil => Nat.zero
        @cons head tail => {
            tailLength := *tail;
            Nat.succ tailLength;
        };
```

Graph property:

```ap
lengthProperty :=
    \xs : List =>
    \n : Nat =>
    \graph : @length xs n =>
        graph
            @nil => ...
            @cons {{
                h := head;
                t := tail;
                recursive := tailLength;
            }} =>
                ... recursive ... @recursive ... *recursive ...;
```

The roles are immediately readable:

```text
h          local name for source pattern binder head
t          local name for source pattern binder tail
recursive  local value name for source recursive-result origin tailLength
@recursive paired graph witness when available
*recursive ordinary induction hypothesis when that graph field is recursive
```

No reader has to remember that `:=` reverses direction in this one grammar.

---

## 30. Canonical QuickSort-style example

Suppose source computation gives stable origin names:

```ap
{
    lowerResult := *down lowerSize lowerBound lower;
    upperResult := *down upperSize upperBound upper;
    ...
}
```

Recommended proof surface:

```ap
run
    @cons {{
        lo := lowerResult;
        hi := upperResult;
    }} =>
        combine *lo *hi;
```

This reads consistently with the source language:

```text
lo := lowerResult
hi := upperResult
```

and the role family follows the local alias:

```text
lo
@lo
*lo

hi
@hi
*hi
```

The generated physical field positions remain internal.

---

## 31. Process lesson from the history

The historical failure sequence is instructive:

```text
valid ergonomic problem
    ↓
reasonable source-origin abstraction
    ↓
surface token direction chosen inconsistently
    ↓
design document formalizes it
    ↓
critical review focuses on deeper semantics and misses notation polarity
    ↓
large implementation commit makes it real
    ↓
tests freeze behavior
    ↓
rewrite carries behavior forward
```

The lesson is not "do not delegate syntax work."

The useful lesson is:

> Surface syntax requires a small set of language-wide invariants that must be audited independently of feature-local correctness.

For A Program, one such invariant should be:

```text
In every binding-looking form written `lhs := rhs`,
the newly introduced lexical name belongs on the left.
```

Any future exception should require explicit design justification and probably a different token.

A second invariant can be:

```text
single braces preserve written order;
double braces denote a name-oriented set whose textual order is not semantic.
```

Freezing those two invariants would have caught this issue before implementation.

---

## 32. Final recommendation

Adopt the following as the corrected Function Graph case-binding contract:

### Normative rule A — assignment polarity

```text
localBinder := sourceSelector
```

The LHS is introduced into branch scope.

The RHS resolves against the selected Function Graph owner/case source-origin metadata.

### Normative rule B — recommended unordered form

```ap
@case {{
    local1 := source1;
    local2 := source2;
}} => body
```

Entry order is semantically irrelevant.

The elaborator canonicalizes to complete constructor telescope order.

### Normative rule C — ordered form

```ap
@case {
    local1 := source1;
    local2 := source2;
} => body
```

Selected source fields/origin groups must occur in canonical telescope order.

### Normative rule D — complete telescope

Omitted fields remain internally bound/hidden as required for dependent typing and IH construction.

### Normative rule E — role group

Selecting:

```ap
local := sourceOrigin;
```

may expose:

```text
local
@local
*local
```

according to the validated roles available for that origin.

### Normative rule F — positional syntax

Ordinary positional Match remains valid and unchanged.

### Normative rule G — no ambiguous legacy dual mode

Do not accept both `source := local` and `local := source` by heuristic.

Migrate source and fail clearly.

---

## 33. Audit verdict

**Finding:** confirmed surface-language design defect.

**Origin:** PR #21 design stage, explicitly present in the 2026-08-24 proposal.

**Review escape:** the 2026-08-25 critical review retained the same reversed orientation.

**Implementation introduction:** commit `4fe5aee` (`Implement PR21 function graph surface`), whose parent is the pre-implementation review revision `e379c74`.

**Historical implementation evidence:** `reader.c` stores first identifier as `source_symbol_id` and post-`:=` identifier as `local_symbol_id`; fixture uses `tailLength := recursive` and then `@recursive`.

**Current implementation evidence:** current `src/syntax.c` and `src/synthesis.c` preserve the same source→alias interpretation.

**Documentation finding:** current README describes `@function`, `{...}`, and `{{...}}` but does not document the named Function Graph selector surface still accepted by the implementation.

**Corrective action:** make a breaking syntax correction to `local := source`, introduce `{{...}}` as unordered/recommended named selection and `{...}` as ordered named selection, preserve complete telescope lowering, and update tests/docs atomically.

**Kernel impact:** none expected if implemented cleanly.

**Function Graph semantic impact:** none intended.

**Proof source impact:** deliberate source migration required.

---

## 34. Repository evidence reviewed

### Current `main`

- `README.md`
  - pointer-core is current implementation;
  - `{...}` documented as sequential;
  - `{{...}}` documented as program-wide definition block;
  - `@function` documented;
  - named graph selector syntax not publicly documented.
- `src/syntax.c`
  - ordinary block binding direction;
  - current named selector parsing;
  - program-root doubled-brace parsing.
- `src/synthesis.c`
  - source-case layout;
  - preservation of source pattern binder names into graph layout;
  - recursive-result origin naming;
  - named selector resolution;
  - definition-scope pre-registration behavior.
- `tests/fixtures/graph_adequacy/length-direct.p`
  - current ordinary-result proof path;
  - positional graph elimination remains available.

### Historical

- PR #21 design proposal:
  - `doc/2026-08-24T19-13-51-FUNCTION-GRAPH-SOURCE-ORIGIN-SURFACE-AND-INSPECTION-PLAN.md`
  - explicitly specifies `selector := localName`.
- PR #21 critical review:
  - `doc/2026-08-25T06-20-56-PR21-FUNCTION-GRAPH-SURFACE-CURRENT-CRITICAL-REVIEW-AND-CORRECTION-PLAN.md`
  - retains `lowerResult := lower`.
- `4fe5aee`:
  - `Implement PR21 function graph surface`;
  - first audited implementation boundary;
  - parent `e379c74`;
  - adds/updates parser, AST, function graph, artifact metadata and fixtures.
- historical fixture:
  - `src/prototype/tests/fixtures/typing/function_graph_named_case_check.p`
  - demonstrates `tailLength := recursive` → `@recursive`.
- Issue #23:
  - identifies `4fe5aee` as the earlier Function Graph implementation baseline.

---

## 35. Suggested follow-up implementation note

A future implementation commit should ideally be small enough that the syntax correction is reviewable independently of unrelated Function Graph work.

A good sequence would be:

```text
1. add failing tests for new polarity + brace modes
2. change parser AST polarity
3. change named-case resolution
4. add canonical ordering validation/canonicalization
5. migrate active source fixtures
6. update README + active design doc
7. run full acceptance/compatibility/image suite
```

Avoid combining this correction with changes to graph generation, adequacy, motive inference, or kernel checking. The whole point of the audit is that this is a surface/elaboration correction whose semantic graph boundary should remain stable.
