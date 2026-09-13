# A Program

A Program is a dependently typed programming language and proof-assistant
implementation under active development. Compilation and type checking perform
bounded computation ahead of execution, constructing explicit typing evidence.
Unfinished computation remains pending; it is not accepted as a proof.

The current implementation is the **pointer-core rewrite** in
[`src/prototype/pointer/`](src/prototype/pointer/), on the default branch
`rewrite/pointer-core-hott`. The directory name reflects the repository's
acceptance policy, not a promise to discard this implementation.

The previous implementation remains on `main` and at the tag
[`old-version/2026-09-14-main`](https://github.com/repyt-margorp/a-program/tree/old-version/2026-09-14-main).
The [previous top-level README](doc/2026-09-14-LEGACY-TOP-LEVEL-README.md) is
archived unchanged from `e9a131d`, a later legacy snapshot than the frozen
`main`. For that frozen version, use its
[own README](https://github.com/repyt-margorp/a-program/blob/old-version/2026-09-14-main/README.md).
Legacy build commands and artifact formats do not describe the pointer rewrite.

## Build and Try

From the repository root, with a C11 compiler and Make:

```sh
make -f src/prototype/pointer/Makefile pointer-check
src/prototype/pointer/.build/pointer-check examples/05_bool_to_nat.p
src/prototype/pointer/.build/pointer-check --nf main examples/05_bool_to_nat.p
src/prototype/pointer/.build/pointer-check --repl examples/07_add.p
```

Use this explicit Makefile: plain `make` still selects the legacy build.
`pointer-check` checks source and normalizes pure computations. It does **not**
dispatch host effects such as terminal printing. Normalization prints a Core
DAG, not a pretty-printed source value.

REPL commands include `:status`, `:solve 100000`, `:whnf main`, `:nf main`,
`:save session.a`, and `:quit`. Source definitions can also be entered.
The REPL and batch commands use the same Program and Solve mechanisms.

## Small Example

```ap
Nat := @{ zero : *; succ : * -> *; };

double := \n : Nat => n
	@zero => Nat.zero
	@succ k => Nat.succ (Nat.succ *k);
double :: Nat -> Nat;

main := double (Nat.succ (Nat.succ Nat.zero));
```

Here `*k` is the induction result for the recursive field `k`, not an
unrestricted recursive call. **`::` is a post-synthesis check**; it never
supplies an expected type to guide synthesis.

## Surface Language

- Typed Lambdas and curried application: `\x : A => body`, `f x`.
- Pi types: `A -> B` and `(x : A) -> B x`.
- Generative ADTs: `@{ ... }`; recursive schema occurrences use `*`.
- Indexed families: `@\index : A => { ... }`; parameters are outer Lambdas.
- Constructor selection: `Nat.zero`, `(List Nat).cons`; Match uses `@case`.
- Sequential blocks: `{ x := M; N; }`. A `.x` suffix selects execution
  through the binding of `x`; later statements are excluded.
- Definition blocks: `{{ name := expression; ... }}.name` name a graph root
  instead of executing sequential bindings.
- Quotation: `&M`. The default policy inserts supported CBPV boundaries;
  `--strict-thunks` requires explicit quotation at definition boundaries.
- Lambda exit: `!value`, with lexical restrictions across quotation.
- Effect handlers: `@#.return` and operation clauses, without a `perform`
  keyword. Requests, forwarding and resumptions have checked rules.
- Imports: `import name;`, supplied by `--imports provider.p`.
- Generated function graphs and witnesses: `@function` and `*function`
  for the supported fragment, distinct from schema Self and branch IH syntax.

Uniform parameters and indices are separate in a vector family:

```ap
Vec := \A : @ => @\n : Nat => {
	nil : * Nat.zero;
	cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
};
```

A declaration's own name is not an implicit recursive alias. See the
[parser](src/prototype/pointer/syntax.c),
[source tests](src/prototype/pointer/tests/acceptance/), and
[compatibility inventory](src/prototype/pointer/tests/compatibility.sh).
Some fixtures deliberately describe invalid or unsupported programs.

## Implementation Model

Core has only three node kinds: **Lambda, Application, Reference**. Binders
and semantic objects are referenced by pointers. Interning shares exact pointer
structures; it does not compare WHNF or merge alpha-equivalent terms. Alpha
comparison and conversion are explicit operations.

Typed occurrences, contexts, substitutions and derivations remain above Core.
A shared erased term alone does not determine its classifier. CBPV's
value/computation distinction belongs to typing. Constructors, Identity actions
and effect operations use semantic-object references and reduction protocols,
not new Core tags for every feature.

| Area | Sources under `src/prototype/pointer/` |
| --- | --- |
| Graph and evaluation | `graph.c`, `eval.c`, `computation.c` |
| Typing evidence and conversion | `typing.c`, `evidence.c`, `conversion.c` |
| Source synthesis and scheduling | `syntax.c`, `synthesis.c`, `program.c` |
| Inductive families and function graphs | `iadt.c`, `function_graph.c` |
| Higher Identity and dimensional action | `dimension.c`, `action.c`, `identity.c` |
| Program images and CLI | `source_io.c`, `graph_io.c`, `main.c` |

Pure normalization never performs observable host effects. Identity witnesses
do not extend global definitional equality or change interning.

## Program Images

Source can be saved as a `.a` Program image before Solve finishes:

```sh
# Exit status 3 means pending, not a completed check.
src/prototype/pointer/.build/pointer-check --steps 0 --save pending.a examples/07_add.p
src/prototype/pointer/.build/pointer-check --load --nf main pending.a
```

Loading uses ordinary Solve, not a separate proof Replay engine. Stored
completion claims confer no authority. Default saves retain inputs for
recomputation. `--retain-reductions` additionally preserves supported reduction
records; it is not a complete zero-recomputation solver checkpoint. These images
are not the legacy `.apo`/v90 formats.

Exit codes: `0` done, `1` rejected/syntax error, `2` input/internal error,
`3` pending, `4` unsupported. `--steps` bounds solver transitions, not
wall-clock time or the cost of an individual rule.
When a pending Program has no runnable synthesis work, a diagnostic is printed
on stderr. Increasing fuel alone cannot resolve that state; it is neither
successful verification nor evidence that the program is invalid.

## Status and Tests

September 14 verification includes examples 01-07 and 09, a 32-case legacy
compatibility gate (including intentional rejections), and six fuel-free
QuickSort output cases. Acc is source-defined using indexed induction, not a
special kernel primitive. A post-hoc specification of an existing `length`
is tested in [length-output-proof.p](src/prototype/pointer/tests/acceptance/length-output-proof.p).
Importing clients prove content preservation for the unchanged QuickSort
provider's [measure](src/prototype/pointer/tests/acceptance/legacy-measure-property.p)
and [partition](src/prototype/pointer/tests/acceptance/legacy-partition-property.p).
The partition specification places each input head into exactly one output
subsequence while preserving order within each subsequence.
[quickSortCorrect](src/prototype/pointer/tests/acceptance/legacy-quicksort-property.p)
composes these lemmas with append and recursive graph induction into a
`ContentsOf` certificate for the existing QuickSort. Its source-defined IADTs
certify content-preserving partition and reassembly, independently of the
comparator; they do not certify sortedness.
The outer `@quickSort` graph now retains the results and graph evidence of
`measure`, `natAccessible` and `quickSortAcc`, allowing an importing client to
inspect these intermediate steps rather than only extract the final value.
Selected captured indexed functions also generate checked graphs and witnesses,
including Vec copy. The unchanged legacy dependent Vec append now compiles,
and six result comparisons cover empty inputs, order and repeated recursion
through unfinished/completed images.
Typed one-step elimination supports direct and sequenced function fields, and
recovers constructor origins through return-producing sequences. Checked total,
effect-free result projection now has an explicit semantic reference, allowing
computed constructor indices to agree with source post-checks. Ordinary Fold and
Identity transport retain their strict behavior. See the
[result projection contract](doc/2026-09-14-TOTAL-PURE-RESULT-PROJECTION.md).

This is not complete legacy compatibility or complete Higher Observational
Type Theory. General higher/dependent/Universe Identity coherence, some indexed
graph and Vec cases, comparator-dependent sortedness proofs, and full host/backend
coverage remain open. Execution witnesses alone do not prove sorting
correctness. In particular, a standalone indexed Match whose every branch is
refuted can remain pending without a result-classifier constraint from an
application. Its trailing `::` deliberately does not supply that constraint.
See the [active plan](doc/2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md)
and [merge audit](doc/2026-09-14-MERGE-COMPOSITION-AND-IDENTITY-NORMALIZATION-AUDIT.md).

Run the current acceptance suite (Bash is required):

```sh
make -f src/prototype/pointer/Makefile check-acceptance
```

For a smaller compatibility/result check:

```sh
make -f src/prototype/pointer/Makefile check-source-compatibility
```

Tests cover Core/evidence rules, source behavior, negative cases, images/imports,
Identity, effects and selected results. They do not prove the entire proposed
theory.

## Repository Notes

- `src/prototype/pointer/`: current implementation and tests.
- Other `src/prototype/` directories: retained previous implementation.
- `src/handmade/`, other accepted `src/` code and `include/`: separate work,
  not silently replaced by the rewrite.
- `examples/`, `training/`: programs, including historical drafts.
- `doc/`: dated plans, audits and archives; older documents can be superseded.

Follow [AGENTS.md](AGENTS.md) and [CODING_STYLE.md](CODING_STYLE.md). Changing the
default branch does not promote AI-written code outside its permitted directory.
