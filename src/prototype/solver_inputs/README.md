# Direct Solver Inputs

Prototype prerequisite to the [Solver/Evidence plan, SE1](../../../doc/2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md).
Applies after the artifact/readback/conversion candidate, not to accepted code.

## Problem List

1. Completed typed inputs should not require scheduler allocations.

### Subjective (User)

2026-09-30 paraphrase: question Job itself, remove duplicated construction, and
finish the ownership refactor before extending persistence or the C backend.

### Objective (Code)

The first migration covers typed WHNF/NF and closed evaluation demands, including
binding/domain normalization, CLI demands and IADT endpoint normalization.
An input directly borrows checked Evidence or its pending producer. The two
pointers are passed/stored by value; there is no allocated wrapper, result table
or new Core tag. Pending request identity does not change when its input finishes.

Source transport visits checked leaves without allocating Jobs. Its temporary
pointer-membership index is destroyed after export; the existing rule payload
and file layout are unchanged. Read still creates ordinary unchecked premises.

### Assessment

This removes some adapters, not the independent Job graph. Rule premises,
source scopes, classifier/expectation, context/IADT/Identity constructors and
checked-query scheduling still use it. Keeping those indefinitely would not
complete SE1. No semantic boundary has been merged merely to reduce node count.

### Plan

Use the parent plan for progress. Create and verify a disposable candidate:

```sh
bash src/prototype/solver_inputs/overlay.sh /tmp/a-program-direct-inputs
make -f /tmp/a-program-direct-inputs/src/Makefile BUILD=/tmp/a-program-direct-inputs/build check
make -f src/prototype/artifact_persistence/build.mk \
  OVERLAY=/tmp/a-program-direct-inputs BUILD=/tmp/a-program-direct-inputs/build \
  check-artifact-normalization-checkpoint check-artifact-source-checkpoint
```

`ARTIFACT_SOURCE` selects an isolated accepted-source snapshot when the worktree
contains unrelated edits. Keep that baseline explicit in verification reports.
