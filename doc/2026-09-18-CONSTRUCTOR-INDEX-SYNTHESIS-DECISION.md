# Constructor Index Synthesis

Date: 2026-09-18. Status: agreed declaration-time rejection policy;
surface elaboration change not implemented yet.

Execution and progress tracking: [IADT / issue 29 / PR 30 priority plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md).

## Decision

Allow names declared by the family index telescope to be used in constructor
signatures. Elaborate each used name as a constructor-local implicit binder,
not as one fixed index shared by all constructors. Retain dependent telescope
ordering and fresh binder identities per constructor.

```text
Vec := \A : @ => @\n : Nat => {
	nil : * Nat.zero;
	cons : A -> * n -> * (Nat.succ n);
};
```

`nil` fixes its output index to zero and does not quantify an unused `n`.
`cons` obtains `n` from the synthesized type of its tail argument. This is a
symbolic index; it need not reduce to a closed numeral. The output index is
`succ n`, not `n`.

Reject a constructor declaration when a used implicit index cannot be
determined from its inputs by the supported synthesis rules:

```text
Tag := @\n : Nat => {
	mark : * n;
};
```

Reject this declaration, even if `mark` is never called. Do not defer the error
to a call, obtain `n` from a result expectation, or silently convert the missing
implicit into a new explicit argument. This is a language restriction for
synthesis, not a claim that such indexed families are logically inconsistent.

Explicit constructor fields such as `(k : Nat) -> * k` still supply their own
input and are not ruled out by this decision. Parameters before `@\` remain
explicit outer bindings. `::` remains a post-check and supplies no inference
information.

## Implementation Gates

- [ ] Resolve header index names in constructor signatures with independent
  constructor-local binders; preserve shadowing and dependent index scope.
- [ ] Validate recoverability at declaration time. Start with direct index
  projection from nominal typed arguments; do not assume arbitrary functions
  appearing in an index are invertible.
- [ ] Define lowering and partial-application behavior without using expected
  result types or inspecting erased Core as a source of typing information.
- [ ] Test Vec nil/cons, symbolic lengths, dependent indices, explicit fields,
  ambiguous indices, and unused invalid declarations through source/image Solve.
- [ ] Update the README example only when the new syntax actually passes.

This updates the direction of section 2.5 of the
[original indexed-family plan](2026-08-13T19-33-00-EXPLICIT-INDEX-FAMILY-SURFACE-AND-ACC-IMPLEMENTATION-PLAN.md).
It does not retroactively describe the existing explicit-constructor syntax as
incorrect or mark this surface change implemented.
