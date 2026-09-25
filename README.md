# A Program

A Program is a dependently typed programming language and proof-assistant
implementation under active development. Compilation and type checking perform
bounded computation ahead of execution, constructing explicit typing evidence.
Unfinished computation remains pending; it is not accepted as a proof.

The current implementation is the **pointer core** in
[`src/prototype/pointer/`](src/prototype/pointer/), on the default branch
`main`. The directory name reflects the repository's
acceptance policy, not a promise to discard this implementation.

The previous implementation is preserved at the tag
[`old-version/2026-09-14-main`](https://github.com/repyt-margorp/a-program/tree/old-version/2026-09-14-main).
The [previous top-level README](doc/2026-09-14-LEGACY-TOP-LEVEL-README.md) is
archived unchanged from `e9a131d`, a later legacy snapshot than the frozen
tagged revision. For that frozen version, use its
[own README](https://github.com/repyt-margorp/a-program/blob/old-version/2026-09-14-main/README.md).
Legacy build commands and artifact formats do not describe the pointer rewrite.

## Build and Try

From the repository root, with a C11 compiler and Make:

```sh
make -f src/prototype/pointer/Makefile pointer-check
src/prototype/pointer/.build/pointer-check examples/05_bool_to_nat.p
src/prototype/pointer/.build/pointer-check --nf main examples/05_bool_to_nat.p
src/prototype/pointer/.build/pointer-check --repl examples/07_add.p
src/prototype/pointer/.build/pointer-check --run raw src/prototype/pointer/tests/acceptance/host-print.p
```

Use this explicit Makefile: plain `make` still selects the legacy build.
`pointer-check` checks source and normalizes pure computations. Only explicit
`--run NAME` executes unhandled `#print` requests, after module checking.
Print writes exact Text bytes with no added newline; run diagnostics use stderr.
Normalization prints a Core DAG, not a pretty-printed source value, and never
executes terminal output. `--run-steps N` separately bounds execution transitions
(default 100000), not I/O time. A repeated run starts a fresh invocation; saved
images do not record which effects have already happened.

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
  `x : A := M` checks the synthesized result type against `A`, preserving
  the effects and totality of `M`; the annotation does not make `M` pure.
- Program-wide definition blocks: `{{ name := expression; ... }}.name` name
  a graph root instead of executing sequential bindings.
- Quotation: `&M`. The default policy inserts supported CBPV boundaries;
  `--strict-thunks` requires explicit quotation at definition boundaries.
- Lambda exit: `!value`, with lexical restrictions across quotation.
- Intrinsic names: `#Int`, `#Text`, `#return`. Dotted `#.Name` is rejected
  unless `--legacy-intrinsic-dot` is supplied, including in imports and REPL
  input. Ordinary `Namespace.name` selection is unchanged.
- Effect handlers: `@#return` and operation clauses, without a `perform`
  keyword. Requests, forwarding and resumptions have checked rules.
- Imports: `import name;`, supplied by `--imports provider.p`.
- Generated function graph types: `@function`. Global `*function` witness
  access is removed; local recursive IH `*arg` and declaration Self `*` remain.
  Witness construction is an optional internal module, not a surface accessor.

Uniform parameters and indices are separate in a vector family:

```ap
Vec := \A : @ => @\n : Nat => {
	nil : * Nat.zero;
	cons : A -> * n -> * (Nat.succ n);
};
```

`(Vec Nat).cons value tail` recovers `n` from `tail`'s synthesized type.
Its pattern is `@cons head tail`; `*tail` is the induction result. Header
indices used in a constructor are fresh constructor-local binders, not a
fixed result index. Unused indices add no hidden argument to `nil`.

Recovery currently uses direct nominal-family index projections from written
arguments, never a trailing `::` or an assumed inverse of an arbitrary function.
For example, `@\n:Nat => { mark:* n; }` is rejected at declaration time.
The explicit form `cons : (k:Nat) -> A -> * k -> * (Nat.succ k)` remains valid;
its calls and patterns still include `k`. Direct aliases and partial applications
such as `(Vec Nat).cons value` retain the source convention; an ordinary
higher-order parameter keeps its explicitly declared Pi signature.

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
| Host contracts and explicit execution | `host.c`, `execution.c` |
| Typing evidence and conversion | `typing.c`, `evidence.c`, `conversion.c` |
| Source synthesis and scheduling | `syntax.c`, `synthesis.c`, `program.c` |
| Inductive families and function graphs | `iadt.c`, `function_graph.c` |
| Machine types and literal descriptors | `host.c` |
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

Exit codes: `0` done, `1` rejected/syntax error, `2` input/output/internal error,
`3` pending, `4` unsupported. `--steps` bounds solver transitions, not
wall-clock time or the cost of an individual rule.
When a pending Program has no runnable synthesis work, a diagnostic is printed
on stderr. Increasing fuel alone cannot resolve that state; it is neither
successful verification nor evidence that the program is invalid.

## Status and Tests

September 18 verification includes examples 01-07 and 09, a 60-case legacy
compatibility gate (including intentional rejections), and six fuel-free
QuickSort output cases. Post-hoc universal Sorted proofs now cover the unchanged
reported insertion, tree, merge and QuickSort implementations using the proved
natural-number comparator. These are supported fragments, not complete legacy
compatibility or complete Higher Observational Type Theory.
The frozen compatibility inputs opt into `--legacy-intrinsic-dot`; current
source fixtures and default CLI tests use `#Name`.

| Area | Verified scope |
| --- | --- |
| Indexed induction | Source-defined Acc, recursive/function fields, dependent Vec append and selected captured indexed functions |
| Dependent synthesis | Constructor-index refinement and branch-proposed motives checked against every induction branch; unchanged `lengthCertified` |
| Function properties | [Length at its ordinary result](src/prototype/pointer/tests/fixtures/graph_adequacy/length-direct.p), [general QuickSort Sorted](src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p) and [permutation](src/prototype/pointer/tests/fixtures/generic_sorted/content-result-proof.p) at its ordinary result, without global function-witness syntax |
| Higher Identity | Selected typed action, transport and higher-dimensional examples; general coherence remains unfinished |
| Effects | `#print` requests, multi-clause handlers, forwarding and resumptions; ordered partial applications; explicit `--run` terminal output with split-budget and source/image tests |
| Host values | `#Int` aliases `#Int32`; distinct `#Int64`; `#Text` stores exact bytes. Literal typing and image round trips, including recursive Text fields |
| Arithmetic | `#int_add`, `#int_sub`, `#int_mul`, `#int_neg` and corresponding `#int64_*` functions: fixed-width wraparound; partial and higher-order application |
| Formatting | `#int_to_text` / `#int64_to_text`: signed decimal ASCII, no leading zeros or added newline; `#print (#int_to_text (#int_add #20 #22))` prints `42` with `--run` |
| Images | Unfinished/completed source inputs, imports and selected retained reductions through ordinary Solve |

Important limitations:

- The execution backend handles unhandled `#print` only. Other unhandled
  operations report unsupported; runtime sessions are not checkpointed.
  General character/encoding conversions remain open; integer decimal ASCII
  formatting is available. Pure checking/normalization never prints.
  Integer literals synthesize
  `#Int32` and reject out-of-range values; `:: #Int64` does not change that
  choice. Text literals do not impose Unicode normalization or decode an
  encoding. Host types do not yet have general Higher Identity rules.
- Pure evaluation now uses profile `evaluation/pure/v7`. Old v5/v6 retained
  evaluation records are rejected, not silently upgraded. Regenerate those
  images from source or save description-only inputs with the older compiler.
- General dependent motive inference, indexed graph coverage and
  higher/dependent/Universe Identity coherence remain open.
- A standalone indexed Match whose every branch is refuted can remain pending
  without an application result constraint. Its trailing `::` does not supply
  that constraint.
- Nested definition-block expressions and general dependent handlers are not
  supported. Tested dependent captures cover ordinary sequential aliases and
  return-only handlers.
- Execution witnesses alone do not establish sorting correctness. The ordinary
  QuickSort result theorem quantifies over the element type and list, assuming
  a reflexive/transitive relation and a comparator decision proof. It does not
  establish sortedness for an unchecked Boolean comparator, stability, or
  complexity. Current correctness consumers use explicit ordinary proof terms;
  archived `*f` examples are not supported syntax or passing current tests.
  The reported merge uses repeated insertion rather than a linear two-front merge.

Detailed contracts and implementation history are in the
[active plan](doc/2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md),
[result projection contract](doc/2026-09-14-TOTAL-PURE-RESULT-PROJECTION.md), and
[merge audit](doc/2026-09-14-MERGE-COMPOSITION-AND-IDENTITY-NORMALIZATION-AUDIT.md).

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
