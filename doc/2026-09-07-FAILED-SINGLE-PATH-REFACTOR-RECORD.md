# Failed Single-Path Refactoring Record

Date: 2026-09-07
Status: stopped by user, incomplete; not accepted for main
Archive branch: `archive/2026-09-07-failed-single-path-refactor`
Parent commit: `63b00eba3a3cf87b8aa20434b7c8e513a71be92b`

## Decision

Preserve the current tracked changes and nonignored source/document additions in
a local snapshot commit. This includes the user's existing `src/handmade/main.c`
without editing it. Generated binaries and ignored build output are not source
snapshots. The independent `a-program-handmade` repository remains untouched.
Creating this archive does not accept the refactoring or update main.

The old implementation goal is stopped, not completed. Its outstanding work must
not silently resume. The new request is to plan a replacement from the handmade
pointer-graph direction.

## Evidence at Stop

The last saved diagnostic for
`src/prototype/tests/fixtures/artifact/artifact_list_induction_check.p` reads:

```text
classifier equation operand construction failed kind=17 projection=13
compile pending failed: open-image equation topology
compilation image rejected phase=0
```

The current `graph_construction.inc` adds a `SEQUENCE_PROVENANCE` operand by
searching for an IH equation during sequence-equation construction. Its producer
can be constructed later. This makes topology construction depend on traversal
order. The immediately preceding repair addressed another dependency problem:
an IH motive/provenance change could leave the classifier convertible to its
previous value, so consumers did not receive the needed wakeup.

These observations identify the immediate regression, not a proof that every
previous design decision was wrong. The deeper engineering failure is that
equation creation, mutable solution provenance, context projection, and
publication remain coupled enough that a local correction changes another
stage's assumptions.

Earlier session checks passed portions of examples, CompilationImage, IF8, and
artifact nominality/import checks. Those results predate the final dependency
edit and are not certification of this snapshot. No full suite was rerun for
archival, and the snapshot must not be described as passing.

The proposed `.a` zero/partial/solved image and RECOMPUTE/CHECKPOINT round trips
remain incomplete. The source currently declares artifact v90; a version number
does not establish completion of the proposed persistence model.

Before this record was added, tracked changes relative to the parent were
163 files, +44,089/-28,255 lines. This excludes untracked additions and therefore
is not the final snapshot size. Use the archive commit diff for full counts.

## Lessons for the Replacement

- Intern immutable structures using their actual inputs; record consumers of
  pending computations explicitly and independent of construction order.
- An answer includes the evidence/provenance needed by consumers. Comparing only
  a classifier can miss a meaningful answer change.
- Keep erased computation distinct from typed occurrences, without creating
  several mutable copies of one answer across handoff/projection/publication.
- Prove a small end-to-end path with a few semantic tests before extending it.
- Preserve failed experiments as evidence; do not turn unfinished architecture
  into a compatibility requirement for the replacement.

Successor: `2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md`.
