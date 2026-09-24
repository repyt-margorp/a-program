# Global Function-Witness Syntax Audit

Date: 2026-09-24
Compiler baseline: `8278c651aa6e5aed332b2aed69625ef75b6d7f22` (Main)
Issue: [#32](https://github.com/repyt-margorp/a-program/issues/32)
Status: bounded investigation complete; syntax decision awaits the user.

## Scope and Conclusion

The question concerns global `*f`, not recursive IH syntax such as `*tail`.
It is a proof-interface design question, not an established soundness bug.

**The dedicated global `*f` spelling is not necessary if its checked producer
is available through ordinary name/member lookup.** This is a proposed
interface change, not implemented syntax. Keeping proof generation internally
does not require exposing a second meaning of `*` to the user.

Removing both the spelling and every way to obtain the evidence is a different
change. Existing graph-based proofs consume that evidence. Automatic ordinary-
result adequacy for arbitrary supported functions has not been established.
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
- [ ] User chooses how generated evidence is exposed through ordinary names
  or members, including collision rules. No replacement spelling is adopted.
- [ ] Implement the approved syntax and migrate callers before rejecting global
  `*f`. Keep `@f` graph types and IH expressions distinct.
- [ ] Separately establish automatic ordinary-result adequacy for QuickSort
  if that stronger interface is desired; do not infer it from the alias test.

Recommendation: remove the overloaded global spelling only after exposing the
same checked generator through ordinary lookup. Do not make ordinary `f xs`
silently return a packet or make expected types trigger proof search. Pause
the Goal at this report as requested; #32 and the authority refactor stay open.
