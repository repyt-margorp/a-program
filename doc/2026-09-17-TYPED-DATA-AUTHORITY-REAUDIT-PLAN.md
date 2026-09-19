# Typed Data Authority Reaudit and Revised Plan

Date: 2026-09-17
Status: A0 complete; source allocation regressions repaired; A1-A4 partial; final acceptance open
Inspected HEAD: `3a3bf550b3e882ab650395fd612e0fa2526b45bc` (R76)
Branch: `rewrite/pointer-core-hott`
Parent: [Typed Structure and Evidence Refactoring](2026-09-16-TYPED-STRUCTURE-AND-EVIDENCE-REFACTOR-PLAN.md)

Priority update, 2026-09-18: execute the
[IADT surface and issue 29 / PR 30 plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md)
first, with both workstreams advancing together. Resume this plan's broad
reconstruction/authority cleanup after that combined feature gate. Preserve
existing work; only directly necessary prerequisite refactoring runs meanwhile.
This changes execution order, not this plan's final completion criteria.

Publication update, 2026-09-18: the user authorizes separate Main pushes for
the completed IADT surface, verified issue #29 / PR #30 improvements, and each
substantial completed refactoring epoch, after existing tests and affected
regressions pass. Follow the priority plan's publication policy. This supersedes
the earlier requirement to wait for the entire cleanup before publishing;
overall completion and the net-negative goal remain distinct from epoch pushes.

Resume checkpoint, 2026-09-18 (`28e1837`): both priority milestones are now
published, and issue #29 is closed. Resume the unchecked cleanup below; do not
interpret its historical failing-test checkpoints as current failures. The
[Solver audit resume checkpoint](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#resume-checkpoint-2026-09-18)
records the remaining declaration-origin dependency and its exact-field
validation gate. Resolve that gate before changing the source image format.

This is the executable remainder of R2-R5, not an additional feature project.
It supersedes R76's proposed alias-specific repair. Historical measurements
and checkpoints stay in the parent document; they do not imply acceptance.

## 1. Scope and Evidence

The user stopped a patch that introduced `refinement` source metadata and a
`REFINED_SCOPE_JOB`. Inspect existing graph data before inventing more state.
Keep erased computation separate from typed structure. A witness is a Term;
its acceptance derivation is not a second program AST.

This audit read `typing.[ch]`, relevant construction/checking/query paths in
`evidence.c`, source scope/Match/application/allocation paths in `synthesis.c`,
`source_io.[ch]`, `occurrence_io.c`, `context_payload.c`, `derivation.h`,
structural consumers in `action.c` and `function_graph.c`, and source image
tests. It is not a proof that every kernel rule or every module is correct.
No external theory claim or new equality rule is needed for this repair.

The stopped patch in `synthesis.c`, `synthesis.h`, `source_io.c` (+66/-9)
was removed with a reviewed reverse patch before this audit's changes.
At A0, APGSRC46/47 and existing IADT index refinement remained unchanged.
The later A4 application-allocation transport uses APGSRC48/49, described below.
No replacement alias subsystem has been introduced.

### Current allocation checkpoint

APGSRC60/61 now transports immutable binder addresses through the existing
Core/Context relocation. Applications and default Lambda/Pi bindings use
`(syntax, lexical binder prefix, slot)`; Lambda/Pi require slot zero.
Default constructor field scopes use `(constructor, destination
binder prefix, slot)`. These are addresses for bound symbols only: they do
not share classifiers, substitution images, checking requests or acceptance.
Distinct parameter maps still undergo their own substitution/type checks.
Nested scopes have different prefixes; nominal constructors remain distinct.
Explicit constructor `_at` allocations remain independent checked inputs,
not declarations that replace the default allocation for all other callers.
Handler return clauses use slot zero; operation clauses use slots 0/1/2 for
payload/resume/response. Handler proofs are not allocation inputs.
Recursive source Match now retains its existing prefix/motive Contexts, source
branch scopes and recursive-erasure allocation in that same payload. It does
not retain a theorem solely to recover these references. Independently selected
proof roots still use ordinary Solve. Source declarations still have an origin
theorem dependency; this change does not complete the origin audit.

The additional `index-alias` trace located one missing nonrecursive Match
field binder: saved and reconstructed application prefixes agreed on `i`
and `v`, but differed on `k`. Preserving the application's own binder alone
could not repair that prefix. Constructor scope synthesis now obtains its
default field symbols from the same allocation index. No Match proof is
reconstructed merely to recover those symbols, and no new job role is added.

The earlier occurrence-codec split was removed from the patch: the repair
does not need to serialize another typed graph, so that unused API would
only enlarge the implementation. Standalone APGOCC7 remains unchanged.

Current debug evidence in `/tmp/a-program-authority-bindings`:

- `synthesis_test` passes, including application arity/prefix/conflict cases.
- The complete `tests/source_io.sh` passes. All 11 retained fixtures, including
  `function-field` and `index-alias`, pass source-only and typed-root modes,
  two inert resaves, retained-check and recompute (44 final checks).
- Full debug `check-acceptance` exits zero, including 63/63 compatibility and
  imported QuickSort source/image results at chunks 1/64. Log:
  `/tmp/a-program-authority-bindings-debug.log`.
- Added boundary tests then passed in `synthesis_test` and `source_io.sh`:
  independent parameter producers share default field symbols; a nested
  constructor scope has distinct symbols; malformed address syntax/prefix/slot
  records reject without publishing roots. Explicit alternative field
  allocations still pass the preexisting constructor-input tests.
- Full ASan/UBSan `check-acceptance` also exits zero, including the added
  boundary tests, 63/63 compatibility, Handler snapshots and final retained
  QuickSort results at chunks 1/64. Log:
  `/tmp/a-program-authority-bindings-sanitize.log` (O0, address/undefined
  sanitizers, frame pointers, non-PIE). No sanitizer diagnostic was reported.
- These full runs precede the application-interface and recursive-branch
  cleanup below. They are not acceptance evidence for later edits.
  Optimized/performance/LOC/publication gates remain open.

### Current structural cleanup

Removed `pg_synthesis_application_at`, its allocation getter, the application
case in `pg_synthesis_allocation_object`, `allocation_prefix`, and the saved
application Context count/prefix branches. Only tests still called these APIs;
source images already used immutable symbol addresses. Their tests now check
registration before/after request creation, immutable conflicts, capture
rejection and inferred arity without recreating a typed Context as an address.

Removed recursive-branch origin attachment in `match_induction_source` and
the Lambda/Pi derivation-input walk in `induction_branch_step`. Branch fields
already have checked Context extensions from the induction substitution.
`case_field_scope` now uses those extensions for both ordinary and recursive
Match; the separate job-pointer array and its dispatch are gone. Imported
Match evidence is still checked by `match_step`, and restored field/IH symbols
still come from `source_match_branch_context` / `match_induction_scope`.

Deleting that walk alone failed four append retained-image checks: nested
Lambda allocation still depended on the Context-checker identity. The fix uses
the same symbol-address index for default Lambda/Pi bindings. Explicit `_at`
inputs stay independent. No new job, Core node or alpha-interning rule was
introduced. Source-only and typed-root append now retain exact Core identity.

Focused current evidence in `/tmp/a-program-authority-cleanup` (O0, debug):
`synthesis_test` and the complete `source_io.sh` exit zero. The latter covers
all 44 retained-fixture checks. `source_alias_targets` now tests both explicit
and default Lambda symbols in 16 scenarios: alternate proofs, distinct images,
pending/failed producers, both request orders, chunk sizes and two inert
resaves. Nested same-syntax binders remain distinct; invalid address kinds and
nonzero Lambda slots reject.
Full debug `check-acceptance` also exits zero, including 63/63 compatibility,
Handler boundaries and imported/retained QuickSort results at chunks 1/64.
Log: `/tmp/a-program-authority-lexical-debug.log`.
Full ASan/UBSan `check-acceptance` exits zero on the same implementation and
tests, including 63/63 compatibility and final retained QuickSort cases.
Log: `/tmp/a-program-authority-lexical-sanitize.log`; build directory
`/tmp/a-program-authority-bindings-sanitize`, with the parent's O0/non-PIE
sanitizer flags. No sanitizer diagnostic was reported. Current optimized
acceptance and the complete baseline performance matrix remain outstanding.

This deletion is **not a measured speedup**. GDB at `pg_program_destroy`, on
independently generated append images with `retained-check`, reports:

| Build | Solve steps | Jobs | Accepted proofs |
|---|---:|---:|---:|
| `/tmp/a-program-authority-bindings` | 6081 | 1799 | 911 |
| `/tmp/a-program-authority-cleanup` | 6203 | 1846 | 911 |

The remaining source-scope keys still distinguish imported checking requests
from accepted Context wrappers. Removing the proof-shaped identity bridge
therefore exposes additional requests, despite preserving allocation and
accepted results. A1/A2 must address structural-work sharing without dropping
the independent imported checks. Do not reintroduce the deleted proof walk as
a performance shortcut, or merge jobs by keeping only the first obligation.
The 47 extra jobs break down as expression +7, binding/domain +1 each,
classifier/formation/constraint/structure +22, derivation +13, expectation +2
and scope-context +1. This is not a count of repeated evaluator reductions:
it locates the remaining source/checking identity duplication. Both binaries
were debug builds; these are work counters, not wall-time comparisons.
The corresponding retained append images grow from 75443 to 75811 bytes
because default Lambda/Pi symbol addresses are now explicit. This cost must
be included in the final image-size report, not hidden by a source-only test.

### Lambda/Pi source environments

APGSRC54/55 removes another proof-shaped allocation input: a source `BINDING`
environment now stores its syntax and binder reference, not a Context theorem.
The existing shared Core payload relocates the symbol. Reading calls
`pg_synthesis_binding_at`, which synthesizes the original domain annotation
through ordinary Solve. Removed `pg_synthesis_restore_binding`, its declaration,
the binding-job origin attachment, and the completed-expression indirection in
allocation discovery. Pending bindings can be retained without first proving
their Context. Explicitly selected proof roots and genuine Context/IH/graph
binding inputs retain their checking obligations; they are not downgraded to
unchecked symbol references.

`source_binding_annotations` covers valid and missing-name annotations, zero,
partial and completed/rejected work, two inert resaves and chunks 1/64. Writing
does not advance Solve or add proofs. Loading publishes no result; the valid
case later yields the relocated bound variable, and the invalid case rejects.
`invalid_binding_rule` rejects the obsolete theorem field on the new wire
record without publishing roots. Source/header/seed magic changes together;
the superseded format has no compatibility replay path.

Focused debug `synthesis_test`, `seed_test` and `source_io.sh` pass in
`/tmp/a-program-authority-binding-refs` before the last malformed-record test;
`context-scopes` also passes with the new annotation cases. The full acceptance
run, including the final malformed-record check, is tracked separately below.

This is a structural deletion, not a performance win: independently generated
append retained-check now uses 6233 steps, 1858 jobs and 911 accepted proofs;
the image is 76251 bytes. The previous checkpoint was 6203/1846/911 and 75811
bytes. Recording pending binder allocations and reconstructing source contexts
without imported receipt identity does not itself merge the source requests.
Do not infer duplicated evaluator reductions solely from these job counters.

The older failure reports below are historical evidence, not the current
result of these two retained fixtures.

### Handler allocation without proof reconstruction

APGSRC56/57 reuses the same source-binding addresses for Handler/Fold symbols.
It removes `handler_clause_origin`, the Handler cases of
`pg_synthesis_restore_elimination`, their waits and Context comparisons in
return/clause checking, and the Handler proof pointer in source binding inputs.
The source codec rejects the now-unused Handler binding theorem field.
Explicit proof roots still undergo ordinary checking; operation resolution,
carrier synthesis, effect inference and final Handler rules are unchanged.
Explicit clause `_at` allocation inputs remain independently checked.

Fresh debug tests in `/tmp/a-program-authority-handler` pass `synthesis_test`,
`seed_test`, all `source_io.sh` fixtures, Fold/Handler/nested-Handler selectors
and all 4446 Handler save boundaries. Repeated inert resaves retain exact Core
and classifier pointers after relocation, with no alpha-equality relaxation.
Tests reject conflicting/invalid binder slots, obsolete theorem fields and
invalid return-body type expectations. Obsolete tests of the removed
proof-shaped Fold allocation API were replaced with these actual input checks.
Full debug and optimized acceptance subsequently exited zero (logs
`/tmp/a-program-authority-handler-debug.log` and
`/tmp/a-program-authority-handler-o2.log`). Focused ASan/UBSan synthesis,
source images and all 4446 Handler boundaries passed. These results precede
the qualified-member change below; no completed Main publication is claimed.

### Qualified member allocation without substitution proofs

APGSRC58/59 replaces the qualified-use origin theorem with its existing field
Context references in the shared allocation payload. `pg_synthesis_member_at`
supplies only the prefix and field binders; ordinary constructor resolution
still synthesizes parameters and field types. `pg_synthesis_restore_member`
and `allocation_context_binder` are removed. No new job role or acceptance
state is introduced. Selected pending source requests retain these inputs
directly, not only when a completed Core makes their symbols reachable.

The constructor-input tests pass with three inert resaves of independent
valid/invalid requests: wrong arity remains rejected, a stored field annotation
cannot override the synthesized type, and accepted results retain exact typed
subjects. Registration creates neither evidence nor Solve steps. Full current
debug, optimized and ASan/UBSan acceptance exit zero in
`/tmp/a-program-authority-member-{debug,o2,sanitize}.log`, including 63/63
compatibility, all 4446 Handler save boundaries and retained QuickSort at
chunks 1/64. Sanitizers use O0, frame pointers and non-PIE. This closes the
verification of this change, not the remaining A1-A5 or net-negative gate.

Same source fixtures, independently saved by the before/after debug binaries
`/tmp/a-program-authority-{handler,member}/source_io_test`, then loaded with
`retained-check`. GDB reads counters at `pg_program_destroy`; all runs exit
zero. These are work/storage counts, not elapsed-time speedups:

| Fixture | Image bytes before -> after | Solve steps | Jobs | Accepted proofs |
|---|---:|---:|---:|---:|
| constructor | 18921 -> 17273 | 1042 -> 961 | 343 -> 321 | 181 -> 181 |
| append | 76251 -> 72356 | 6233 -> 6032 | 1858 -> 1801 | 911 -> 911 |
| function-field | 69722 -> 67890 | 4574 -> 4468 | 1271 -> 1247 | 836 -> 836 |

Commands: `<binary> retained-write /tmp/<version>-<fixture>.a <fixture>`,
then `gdb -batch -ex 'break pg_program_destroy' -ex run
-ex 'print program->synthesis.steps' -ex 'print program->synthesis.jobs.count'
-ex 'print program->typing.proofs.count' -ex continue --args <binary>
retained-check /tmp/<version>-<fixture>.a`. No premise deletion or proof merge
is inferred from these measurements; accepted proof counts stay unchanged.

### Reproduction during this audit

Using the existing `/tmp/a-program-typed-structure-r76/source_io_test` binary
(not a rebuild of the dirty worktree), write fresh `function-field` and
`append` images, then run `retained-check` and `retained-recompute`.

| Fixture | Checked receipt reuse | Recompute |
|---|---|---|
| function-field, source root | Fails exact source identity, exit 134 | Same failure, exit 134 |
| append, source root | Pass | Pass |

The function-field diagnostic reports `alpha_equal=1` and the first exact
difference is a binder. The assertion is `check_retained_program` in
`tests/source_io.c:1264`. This rerun corroborates R76, not the unbuilt patch.
The R76 log additionally reports failures with typed roots and passing imported
QuickSort. Those larger checks were not rerun in this planning pass.

A0 rebuilt the restored implementation with `-O0 -g -Wall -Wextra -Werror`
in `/tmp/a-program-authority-a0`. Fresh function-field retained-check and
retained-recompute both fail with the same exact-binder mismatch; append
passes both. Thus the failure does not depend on an old binary or the removed
experiment.

Reproduction command pattern, with an isolated temporary image path:

```sh
/tmp/a-program-typed-structure-r76/source_io_test retained-write /tmp/a-program-authority-reaudit-function-field.a function-field
prlimit --core=0 -- /tmp/a-program-typed-structure-r76/source_io_test retained-check /tmp/a-program-authority-reaudit-function-field.a
prlimit --core=0 -- /tmp/a-program-typed-structure-r76/source_io_test retained-recompute /tmp/a-program-authority-reaudit-function-field.a
```

## 2. Findings and Decisions

Paths are relative to `src/prototype/pointer/`. Symbol names are authoritative;
line numbers describe the inspected worktree and may move.

| Finding | Code evidence | Decision |
|---|---|---|
| Binder images already exist independently of derivations | `typing.h:66`, `pg_context_map`; `typing.c:341`, `pg_context_map_image` | Reuse source/destination/images. Do not create a second refinement map |
| Context action already retains typed provenance | `typing.h:34`, `pg_occurrence.origin/map`; `typing.c:649`, `pg_occurrence_action_request` | Reuse exact `(subject, map)` work; classifier/annotation transport belongs here |
| Source scope identity includes checking machinery | `synthesis.c`, `intern_scope`, `bind_context`, `pg_synthesis_name_job`, `request_role` | Source expression jobs inherit producer/context-job distinctions. Audit semantic identity separately from pending dependency identity |
| Match converts existing mapped data back into producer-keyed names | `generalized_scope`: compose map, select image, wrap in `pg_synthesis_evidence`, call `pg_synthesis_name_job` | Fix this handoff and its image counterpart, not the algebra of index substitution |
| Image restoration has two coupled obstacles | `source_io.c`, `collect_origin`; `synthesis.c`, `pg_synthesis_restore_application`, `prepare_application` | Preserve selected-root allocation reachability AND shared source identity. Widening collection alone is known to fail |
| Allocation inspection still depends on proof/job shape | `pg_synthesis_allocation_object` walks handler rule premises and distinguishes loaded versus live producers | Redirect available structure to retained binders/contexts/induction allocations; preserve rule validation separately |
| Structural consumers still have fallback paths | `function_graph.c`, `computation_view`; `action.c`, `identity_structure` | Audit each against existing typed inputs. A fallback or evidence lookup is not automatically a duplicate authority |

The minimal R76 failure was previously traced to a saved `DERIVATION_INPUT_JOB`
and a fresh `EVIDENCE_JOB` yielding the same exact Evidence but different scope
keys. Static code confirms those keys feed source request identity; the rerun
confirms the resulting binder mismatch. Add a small permanent test that records
that intermediate relationship rather than relying only on this diagnosis.

### A1 trace on the rebuilt baseline

GDB on `/tmp/a-program-authority-a1/source_io_test`, using the existing
`retained-write ... function-field` and `retained-check ...` entry points,
confirmed the following chain without changing implementation behavior:

1. At `generalized_scope`'s name creation, `v` maps to a constructor occurrence
   with no `origin` or `map`. The composed map still exists separately and
   contains this occurrence among its images. The image therefore cannot
   recover the old binding or the complete substitution. The mapped `i`
   retains a different map, belonging to its own construction, not the complete
   source-environment substitution. Following `occurrence.map` is not a
   replacement for retaining the original source-to-map edge.
2. At `collect_origin`'s lexical filter, the application at source byte 156
   (`Nat.succ (*down Nat.zero)`) is skipped. Its scope's last name is the
   generated `i`, backed by an `EVIDENCE_JOB`, with neither a context binding
   nor a syntax site. Two qualified-constructor origins at byte 150 are
   skipped by the same filter. The containing source syntax was selected.
3. After load/Solve, the application at byte 156 has no `allocation_origin`.
   Its application's context binder is exactly the fresh binder found at
   the first difference in the failed retained-Core comparison. The retained
   reduction source contains the other binder. Compare these pointers only
   within this loaded process, not between write and read processes.

This proves a missing allocation association on the unmodified format. It
does not show that merely collecting more scopes repairs identity: the prior
withdrawn collection experiment exposed a second, producer-keyed mismatch.
The new alias test independently confirms distinct producer jobs can check to
the same exact Evidence/occurrence. Both problems must be addressed together.

The permanent retained-image runner now includes `index-alias`, a smaller
counterexample without recursive fields, IH calls or Identity operations:

```text
Nat := @{zero:*; succ:*->*;};
D := @\i:Nat=>{mk:(k:Nat)->* k;};
f := &(\i:Nat=>\v:D i=>v @mk k=>Nat.succ (Nat.succ i));
r := &{f Nat.zero (D.mk Nat.zero);};
```

With `/tmp/a-program-authority-minimal/source_io_test`, `retained-write PATH
index-alias` succeeds; both `retained-check PATH` and `retained-recompute PATH`
fail exact source identity, with first difference `aaabffaabbabinder` and
`alpha_equal=1`. `retained-write-typed` also reproduces the source failure:
checking the separately retained typed root succeeds, but the final independent
source comparison fails. That is not permission to replace source checking
with the retained root. The runner includes both root modes and inert resaves.

GDB on this rebuilt executable finds the application at source byte 100
excluded by `collect_origin`'s lexical filter. Its final scope entry is `i`, an
`EVIDENCE_JOB` alias with no context-binding or syntax-site description. This
narrows the missing association to ordinary indexed source context action;
changing induction, effect or higher Identity rules is unwarranted. Pruning
unused generated aliases would not repair this example: `i` is actually used.

Consequences for A2/A3: retain the source environment's action by its existing
typed map and original lexical bindings, rather than trying to infer that
action from image values or canonicalizing their proofs. The exact handling
of an unaccepted map remains part of A1's key/obligation design gate. A map
description is not permission to bypass its checking obligation.

`pg_prove_structural_subject` is not a general checker for imported typed
nodes. `structural_dependency` reuses accepted subjects, checks binder uses,
and certifies mapped origins; it does not construct arbitrary constructor or
Lambda evidence from operands. The `v` image above is a concrete counterexample
to replacing a name's producer with this call alone. Keep the actual checking
dependency until its replacement can certify that input. Neither a NULL result
nor an existing occurrence is an acceptance decision.

### Why the stopped repair is rejected

- It introduces a special identity rule for generated names, extra work state
  and a wire-format exception before using existing map/subject structure.
- It skips the producer in one class of interning key, then validates differing
  producers afterwards. The first producer remains in the stored scope. This
  requires a new lifecycle argument that the current implementation lacks.
- Original binding plus destination alone does not in general specify a
  substitution: different maps can share source and destination contexts.
  Rejecting the later interpretation does not make an incomplete key correct.
- It publishes an interned record before resolving the repeated interpretation.
  Pending/failed registration and later lookup would need explicit tests.
- Renaming `refinement` would not remove any of these problems.

Do not delete existing IADT refinement/factorization rules because they contain
the same word. They express checked index substitutions, not this new alias fix.

## 3. Authority Contract

"One authority" means one owner for each fact, not one record for all facts.

| Fact | Existing owner | Other structures may do |
|---|---|---|
| Erased executable shape and reference identity | `pg_term` / graph objects | Refer to it; never merge by WHNF/alpha during interning |
| Declared scope and binder identity | `pg_context`, binder objects | Reference the same allocation; prove formation separately |
| Typed use, classifier, semantic operands | `pg_occurrence` | Read or request context action; do not infer from arbitrary proof history |
| Substitution action | `pg_context_map` and shared occurrence action | Select `images`, cache work, prove well-formedness |
| Acceptance of a specified conclusion | `pg_evidence` and ordinary kernel constructors | Retain alternative derivations; none changes the structural referent |
| Source spelling, lexical visibility and pending definition | Existing source syntax/scope/definition inputs | Point to declarations/typed inputs and wait for checking |
| Progress and failure of a requested calculation | Existing synthesis/query jobs | Cache state/results; must not define new lexical allocations by discovery order |
| Persistent references | Existing image graph/relocation tables | Reconstruct shared references and unaccepted obligations, not certify them |

Evidence already delegates term classifier/context access to its conclusion
(`pg_evidence_classifier`, `pg_evidence_context`). Do not undo this completed
single-owner arrangement. A pointer to a conclusion is not a competing answer.

`pg_context_map_bindings` is an immutable erased projection of typed images;
it is not a second mutable substitution solution. Likewise, shared typed-query
work and its result do not become another authority merely by being cached.
Inspect keys and consumers before declaring these structures redundant.

Important limits:

- Erased Core does not recover source spelling, nominal typing, or acceptance.
- Context maps do not recover lexical shadowing or an unresolved definition.
- A checked image of a binder need not exist before Solve. Preserve its existing
  source/derivation input and dependency, not a guessed answer.
- Exact typed subjects can share structural work even when proofs differ.
  Distinct source declarations must not merge merely because their values match.
- Keep reduction policy in computational cache identity. This is not an excuse
  to merge pending/failed derivations or to trust deserialized typed structure.

Structural synthesis needs a further distinction: `term_structure_step` and
`type_structure_step` can construct terms with unresolved effect parameters
before acceptance. `stored_effect_derivation` in `tests/synthesis.c` requires
that completed symbolic snapshots remain stable even after the accepted type
has a solved effect row. The later accepted-projection epoch (`607058c`)
preserves these snapshots but lets a not-yet-completed query read accepted
typed data directly. Its contract compares the symbolic and closed forms after
effect substitution, rather than requiring the same unsolved representation
regardless of scheduling. The audit records the consumer checks and regression.
Remove redundant reconstruction, not necessary initial symbolic construction;
do not claim every structural Job is redundant.

## 4. Replacement Design

Use the existing data flow:

```text
source binding/declaration -> descriptive typed input / binder / existing map
                                            |
                                   ordinary Solve checks
                                            |
                                Evidence of that same target
```

For a generated Match alias, the semantic target comes from
`pg_context_map_image(map, original_binder)`. For a captured named expression,
use its typed subject under the existing occurrence action. Its source spelling
and lexical owner remain source metadata. Do not replace either with a proof ID.

Shared structural work is keyed by complete immutable typed inputs. A source
binding keeps its lexical identity; its producer is a checking dependency, not
the authority for a different semantic binding. Two different substitutions
must remain distinguishable even in the same destination context.

Do not interpret this as requiring identical source request pointers. An
allocation reference and a checking request have different keys. The former
must survive relocation; the latter must retain the particular unaccepted
input being checked. Sharing the former does not require merging the latter.
In particular, deleting producer identity from `intern_scope` and retaining
the first producer is not a repair. Nor must every ordinary rule request be
removed merely because two accepted results share a typed subject.

The existing `_at` allocation inputs already implement this distinction;
reuse them rather than adding another alias worker or acceptance table.
The source image must supply those inputs to the independently reconstructed
source request. Its origin association must be descriptive, not an assumption
that an imported checking job equals a fresh accepted-evidence wrapper.

For implicit application binders, the allocation address can be smaller than
the semantic environment: `(syntax, ordered enclosing lexical binders, slot)`.
Names, classifiers, substitution images and checker identities are not part of
this address. This shares only bound symbols, not source jobs, typed results or
nominal datatype declarations. Different images still produce different bodies;
their checking dependencies remain independent. A nested use has another
enclosing binder sequence. Separate applications under the same sequence may
need different numbers of implicit binders, so each slot is allocated separately
rather than treating a saved count as a typing theorem.

Implement this as the owner of source binder allocations, with existing jobs
referring to those objects. Do not use it to intern Core by alpha/WHNF, merge
named environments, or allocate nominal IADT identities. Images must transport
the address and its existing binder through the shared graph relocation, not
reconstruct a named scope merely to find the address. This supersedes A1's
assumption that the complete alias/map key must be solved before repairing the
application allocation association; that stronger key is required for sharing
semantic work, not for sharing these bound symbols. Explicit constructor,
telescope and operation `_at` inputs retain their own allocation checks;
the superseded application Context-pair interface has been deleted.

Apply this to the existing scope/name representation, replacing its conflicting
identity policy rather than wrapping it in a new registry or translation layer.
No special `REFINED_SCOPE_JOB`, third typed AST, or alias acceptance store.
Pending dependencies use the existing scheduler. Do not mutate hash keys after
Solve or globally redirect one proof-producing job to another.

The exact pending/source image encoding is an A1 design gate, not decided here:
some descriptions exist in memory but are not referenced by the current source
codec, which transports unaccepted rule inputs. Inspect the failure's actual
descriptive graph before choosing fields. Reuse the existing context/occurrence
codecs and source-origin associations where sufficient. If a source-to-structure
edge is genuinely absent, document the lost fact and extend its existing owner;
do not claim that a Context map can supply missing syntax or vice versa.

Imported data follows the same Solve checks. Retaining allocation identity must
not accept a forged classifier/context. Keep every caller's proof obligations;
sharing structural access is not selecting a canonical derivation.

## 5. Implementation Checklist

Keep A1-A3 in order. Independent A4 checks and changes may finish earlier when
they do not depend on or alter source identity; they must not bypass A1's
design gate. A phase closes only with the stated evidence and deletions.

### A0. Remove the stopped experiment

- [x] Remove only this agent's +66/-9 uncommitted refinement patch, using a
  reviewed reverse patch; preserve unrelated changes and all committed R76 work.
- [x] Restore APGSRC46/47 consistently before building the baseline. Remove
  `refined_scope_state`, `REFINED_SCOPE_JOB`, API/field and interning exceptions.
- [x] Rebuild the focused baseline in an isolated build directory. Reproduce
  function-field failure and append success; record commands and revision.

### A1. Trace identity end to end

The failures below are historical checkpoints, not current test failures.
The allocation-address changes in sections 1/4 now pass the retained index-alias
and function-field cases. The complete remaining descriptor/consumer audit is
still open; green images alone do not close that audit.

Current contract audit at `58c9295` (2026-09-18): the following table supersedes
the historical failure/status descriptions below. Re-running the complete
`tests/source_io.sh` with `/tmp/a-program-authority-declared-opt/source_io_test`
exited 0; log: `/tmp/a-program-authority-current-contract.log`. This includes
44 retained source/image comparisons, not a new full acceptance run.

| Reference | Existing authority/key | Checked boundary |
|---|---|---|
| Accepted name | `source_name_target`: immutable typed occurrence | Alternate proofs share a target, not their premise DAGs |
| Pending name | Exact producer plus lexical scope | Completion cannot mutate the intern key or hide a rejected producer |
| Source binder | `pg_source_binding`: syntax/constructor, slot, enclosing binder sequence | Both checking orders share allocation; shadowed scopes remain distinct |
| Transported alias | Existing typed subject and Context map | Equal source/destination with different images must stay distinct |
| Application/constructor use | Existing source-binding addresses and prefix/field Contexts | Relocated binders reused; saved annotations are independently checked |
| Match | `pg_match_allocation` for imported inputs; typed elimination for completed fresh inputs | Source and erasure scopes retained without trusting the saved motive |
| Declaration | Nominal declaration reference and existing schema/member allocation | Same shape does not merge distinct declarations |

`source_alias_targets` now has 16 scenarios, including automatic allocation as
well as explicit allocation. `member_use_origins`, `application_origins`,
`fold_origins` and `match_motive_authority` cover the corresponding retained
allocation and invalid-input boundaries. No new persistent alias schema is
needed. Sharing allocation is not permission to merge independent checking
obligations.

`source_alias_targets` in `tests/source_io.c` now checks two maps with identical
source/destination but different images, alternate map-image proofs, direct
versus unaccepted producers resolving to the same exact occurrence, and an
invalid producer alongside valid aliases. Eight scenarios cover chunks 1/64,
reversed producer/use request order, pending versus independently completed
producers, and nested same-name shadowing without changing the outer binding.
They pass in `context-scopes` with a fresh debug build in
`/tmp/a-program-authority-a1`. They do not assert that producer-keyed scopes
are already unified. The end-to-end source image regression still fails.

- [x] Extend existing source image tests with a minimal index-alias case. Observe
  original binder, map, image occurrence, producer, scope and application job
  before save and after load/Solve. Compare relocated identities inside each
  process, never numerical addresses across processes.
- [x] For each differing reference, distinguish missing serialization from
  reconstruction keyed by a checker. Include the hidden Fold binder allocation.
- [x] Specify concrete scope/name/request keys for resolved and pending input;
  verify source shadowing, distinct maps and nominal declarations remain distinct.
- [x] Record which existing field/edge supplies each key. Any proposed extra
  field needs a counterexample showing why existing data cannot supply it.
  No implementation of a new persistent schema before this gate closes.

2026-09-18 gate completion (working tree based on `7c6b8fe`):
`mapped_alias_producers` now uses `Nat.succ alias` under the indexed Match.
The trace follows the actual branch scope's alias producer to its `PG_REINDEX`
premises. It checks the source binder, map source/destination and exact image
occurrence, then looks up the existing application/argument requests without
allocating another job. The application, map and original variable premise are
selected independently, saved and resaved without Solve, and checked again.
The relocated source scope reaches the same application job, exact map proof
and original premise as the independent roots. Eight schedules cover the
existing direct/imported/delayed/invalid producers and insertion orders.
Debug `context-scopes` and the full `source_io.sh` pass in
`/tmp/a-program-authority-index-trace`; full publication gates are recorded in
the priority plan. This closes the identity-trace gate, not all of A3-A5.

Reference classification from current consumers:

| Reference difference | Meaning and retained owner | Required action |
|---|---|---|
| Accepted alias versus pending producer | Typed subject versus independent checking obligation | Keep both keys; never suppress the invalid producer |
| Original index versus branch alias | The checked map's source variable versus its destination image | Retain the same map/subject edges; no alias-specific schema |
| Fresh versus restored Lambda/application/Fold binder | Syntax/slot/enclosing-binder address in `pg_source_binding` | One registry, including hidden application sequencing binders |
| Fresh constructor fields versus explicit restored fields | Checked map destination versus unaccepted allocation Context | Read through `pg_synthesis_constructor_input`; recheck arity, prefix and types |
| Fresh versus restored Match allocation | Typed elimination operands versus an unaccepted prefix/motive/branch tuple | Keep the read-only projection and recheck source; do not store a second accepted tuple |
| Declaration versus another declaration of the same shape | Distinct nominal identity | Never merge by structural shape or checker completion |

`fold_origins` checks exact hidden continuation identity, inert resaves and
conflicting-symbol rejection; `handler_save_boundaries` additionally connects
the accepted return Lambda directly to its lexical registry entry. These are
allocation inputs, not accepted classifiers. For constructor uses the former
generic getter duplicated the child-job traversal; it now delegates to the
existing member/constructor input view. Completed constructor scopes read the
accepted map's destination, not nested proof-premise positions. Explicit field
allocations are not globally forced to use the automatic address: they may be
independent, well-typed callable scopes. Restoring one source use still checks
its agreement with that use's constructor allocation.

Additional boundary verified in `source_alias_targets`: four scopes use one
Lambda syntax and one explicitly supplied binder, but independent producers.
Two valid producers reach the same typed target, a third reaches a different
map image, and a fourth has an invalid variable premise. The first two keep
distinct checking requests and produce the exact same Core Lambda; the third
keeps the supplied binder but a different body; the fourth remains rejected.
After retaining a typed reference to that binder and two inert source-image
resaves, the same properties hold. The restored Lambda binder must equal the
relocated binder in the retained Context, not merely match the other Lambda.
Eight scenarios exercise chunks 1/64, both insertion orders and producer
completion before/after source request creation.

This test deliberately retains the allocation through a Context root. A
source-only image that retains neither a typed result nor an external
allocation reference may allocate afresh on recomputation; equality between
fresh process addresses is not required. Conversely, retaining a typed Context
must preserve the existing pointer-sharing relation inside the loaded graph.
No alpha-equality relaxation, new node, source key or acceptance rule was added.

This narrows A2: preserve the alias-to-typed-input association and feed existing
allocation APIs; do not try to solve the failure by globally merging checker
jobs. It does not close A1's complete alias descriptor/serialization design
gate. A fresh `retained-check` still fails for `index-alias` at
`aaabffaabbabinder`, with `alpha_equal=1`.

Verification: `context-scopes` passes with fresh debug and ASan/UBSan builds
in `/tmp/a-program-authority-scope-key` and
`/tmp/a-program-authority-scope-key-sanitize`; `git diff --check` passes.
This continuation changes the boundary regression and design constraint only,
not production source identity or the source format. The full acceptance
matrix and Main publication remain open.

Narrow A4 exception, established by code inspection: application sequencing
allocations already have an explicit `(prefix, end)` Context input in
`pg_synthesis_application_at`. Its tests require inferred types, exact binder
reuse, prefix/count checking and rejection of late conflicts. Move only this
existing input into the shared context payload, replacing the application's
saved derivation origin. This does not change source scope/request keys or
resolve A1's missing alias association. Version this transport change; do not
retain the old reconstruction path alongside it. An inert resave must preserve
the descriptor without invoking Solve. Other proof obligations remain intact.

The first transport test exposed an overstrong prefix check: append's checked
prefix retains the same lexical binder sequence but reconstructs an IH type
with different bound type-level Lambda objects. Its type Terms are alpha-equal,
not pointer-equal. Allocation inputs never certify their saved annotations
(the existing application test deliberately uses dummy Universe annotations).
Validate the exact prefix binder sequence, not equality of the old typed
Context. Independently synthesize/check all types as before. This is neither
an alpha-interning change nor a relaxation of the retained-Core equality test.

### A2. Repair source-to-typed references

Implemented subset: `source_name_target` uses the immutable typed subject of
an `EVIDENCE_JOB` in source-name interning and alias visibility comparisons.
Other producers keep their exact job identity, even after completion. The key
does not read mutable status/result, select a canonical proof, or introduce a
new stored field. Alternative accepted proofs remain in the typing graph.
This removes receipt identity from already-checked names only. It does not
unify imported unaccepted producers with those names or restore missing origins.

The alias test now also checks both accepted-proof insertion orders, distinct
typed targets, rejection of a failed producer as a checked name, and unchanged
pending-name keys after Solve. All eight scenarios pass. Pending producer
identity cannot simply be omitted: an independently invalid input must still
be rejected when another input establishes the same proposed result.

Further repair: `scope_value` and its `match_generalize` guard are removed.
The old guard enabled index generalization only for an `EVIDENCE_JOB` containing
a value. Merely changing it to read a completed result was insufficient: a
delayed producer still selected a different elaboration. `generalized_scope`
now uses ordinary projection and reindex requests for captured named terms,
independently of their checking history or current completion state. Both
requests operate on the eventual typed subject; no new job role, scope field,
acceptance rule or wire format was added. Closed names need no such transport.
Local definition environments and Handler scopes remain outside this change.

- [x] Remove the producer-role/completion gate and the synchronous named-value
  reindex path. Retain the existing map and the producer's checking dependency.
- [x] Add `mapped_alias_producers` to `context-scopes`: direct and imported
  references, 512 delayed projection checks, chunks 1/64, reversed request
  order, two inert resaves, and an independently invalid binder reference.
  All eight scenarios pass; the invalid branch remains rejected.
- [x] Add a nested reuse of the same Lambda syntax object to `source_telescopes`.
  Distinct enclosing bindings require distinct binders; a syntax-pointer-only
  allocation key would be wrong. Repeated requests in one scope still share.

The direct/imported Match test failed before repair, even though both results
were accepted. A completion-only patch passed the completed-producer case but
failed the deliberately delayed case. The final test compares independently
fresh Match graphs up to alpha equivalence; the retained-source exact-pointer
assertion is unchanged. This fixes order-dependent elaboration, not the remaining
source-allocation identity problem, and does not close A1-A3.

Fresh debug builds in `/tmp/a-program-authority-map` pass `synthesis_test`, all
12 focused source selectors and all 228 Makefile `program_test` commands.
Program outputs and Solve step counts match the metadata-change baseline
(ignoring the absolute versus relative fixture path spelling). `synthesis_test`
and all 12 source selectors also pass ASan/UBSan in
`/tmp/a-program-authority-map-sanitize`. Fresh function-field retained-check and
retained-recompute still fail at the same exact binder comparison, exit 134.

- [x] Repair alias transport using the existing typed target/map and lexical
  allocation key. Do not merge pending checking requests merely because their
  proposed results coincide. The original requirement to remove all producer
  identity was too broad; `source_alias_targets` demonstrates why it is needed
  for independent rejection while Core allocation is shared.
- [x] Reuse existing context allocation references when preparing applications;
  no fresh binder merely because a different checker established the same input.
- [x] Retain failed and pending proof obligations independently. Neither an
  earlier successful producer nor a cached structural result may suppress them.
- [x] Cover both producer completion orders, alternate proofs of the same
  target, distinct targets, nested shadowing and two maps into one context.
- [x] Delete the superseded `scope_value`/`match_generalize` producer gate and
  synchronous alias reindex path. Pending-producer identity is retained for
  checking, not as a second authority for binder allocation.

### A3. Transport the same structure

- [x] Current epoch: dependency discovery must not construct disposable wire
  payloads. Share the fixed derivation parameter projection between encoding
  and collection, and collect Context binder/type edges through one temporary
  DAG across source allocations and derivations. Remove intermediate payload
  arrays and the second root-array traversal in derivation object collection.
  Keep wire numbering/encoding separate; preserve exact saved bytes where the
  format is unchanged. Verify direct collection against packed dependencies,
  shared Context prefixes, invalid inputs, inert resave, existing acceptance
  and sanitizer tests. This does not finish scope-sensitive origin selection
  or waive the parent's cumulative source-reduction gate.

  Implemented with a shared fixed-parameter projection, `pg_context_collect`
  and `pg_derivation_input_collect`. The source writer and derivation object
  collector use the same direct edges. The latter no longer allocates a second
  array of roots for another graph traversal. Context binder/type validation
  is shared with the encoder; no proof acceptance, new persistent cache or
  wire-format change is introduced. Tests compare packed/direct object sets,
  128 repeated collections, family index dependencies and malformed inputs.

  Against `8d77ce0`, retained IF8 QuickSort saving calls
  `pg_derivation_input_terms <- retain_dependencies` 697 -> 0; the 697 actual
  writer calls remain. Context edge callbacks stay 689. Saved files are
  byte-identical. Immediately before `pg_retained_write`, used bytes in the
  rules/term-dependency/context-dependency arenas total 500,352 -> 446,240;
  reserved bytes total 524,288 -> 491,520. These are selected arena snapshots,
  not peak RSS. Seven alternating debug runs give median source+save wall
  times 0.0821/0.0771 seconds; another run gave 0.0737/0.0755. Do not infer a
  reliable speedup from this small noisy fixture.

  Independently compiling the function-field fixture produced different wire
  ordering even with the same old binary. That is not a regression test for
  stable nominal allocation. Loading the same retained image and resaving
  with `--load --steps 0 --retain-reductions` is byte-identical to the original
  with both binaries (exit 3: pending, no Solve). Without `--retain-reductions`,
  both correctly select the other save mode; do not compare its bytes to the
  retained-mode input.

  Strict debug Source/Derivation/Graph scripts, full optimized acceptance
  (63/63 compatibility and all universal sort proofs), and the same three
  ASan/UBSan scripts pass. Normalized `export results:` records including
  step counts match the preceding full run. Evidence logs use
  `/tmp/a-program-authority-direct-dependencies-*`; debug binaries use the
  same prefix, optimized/sanitizer builds reuse `authority-source-sites-*`.
  This change adds 106/deletes 64 implementation/header lines (net +42),
  tests +51/-2. Cumulative source is still +1,361 versus R76 and +3,777 versus
  R0. Output-sensitive lexical discovery and the net-negative gate remain
  open. This epoch left duplicate Match allocation projection during discovery
  and encoding; the later ordered-origin change below removes it without a
  permanent cache.

- [x] 2026-09-19: collect each selected Match allocation once. Delete the
  separate Match DAG and the second reconstruction pass/array. Match wire IDs
  are the ordered subset of origin IDs; a writer-local list retains the first
  projection until encoding. No new solver state, semantic authority or format.
  Retained IF8/main: allocation queries 14 -> 7; output is byte-identical to
  `5a853e0`. Rules arena used bytes 180,352 -> 179,904; the removed Match arena
  used 448/reserved 16,384 bytes, plus a 512-byte bucket table. These are
  selected stores, not peak RSS or elapsed-time measurements.
  The nested-Match regression checks three inert resaves, restored allocation
  identity after Solve, alpha-equivalence and equal normal forms. Its initial
  exact-Core assumption also failed on `5a853e0`: nested context actions can
  freshen generated binders. Likewise two retained origins become three
  construction candidates during synthesis. The new case states these separate
  counts; the original one-Match exact-pointer gate is unchanged. This does not
  prove optimal retained-reduction reuse for freshly generated nested binders.
  Strict debug source tests and optimized full acceptance pass; normalized
  exports/steps are unchanged. Final affected sanitizer results and publication
  status are recorded in the priority plan. Implementation +33/-27 (net +6),
  tests +23/-7; overall reduction and broad A3-A5 gates remain open.
- [x] Register explicit Lambda/Pi binders in the same immutable address store
  as automatic bindings; delete their duplicate job-origin enumeration and
  writer scope-recovery path. Exact binder/conflict and inert-resave checks
  remain. This is not completion of the output-sensitive traversal item below.
- [x] Update `source_io.c` environment/producer/origin handling using A1's
  reference contract. Loaded inputs and fresh source reference the same relocated
  lexical allocations; source rechecking remains necessary.
- [x] Preserve selected-root reachability without scanning unrelated source
  scopes or collecting all syntax-free aliases. Do not repeat the withdrawn
  broad `collect_origin` experiment.
  Published in `df53645`: transparent definition/handler scopes use their
  container syntax as the reference key, visited by the writer's existing
  syntax frontier. Other scopes use their own binder/address. This removes the
  parent walk without losing mapped-alias or recursive append origins; 128
  unrelated definition scopes leave each tested candidate set/image unchanged.
  Same-syntax/multiple-environment candidate bounds remain open. The priority
  plan records validation and publication status; this is not A3 completion.
  Current follow-up: definition blocks use their exact environment address,
  borrowed from selected prepared producers, rather than their shared syntax.
  The writer visits references once without retaining an empty environment.
  This excludes 128 same-syntax foreign environments (6 candidate visits rather
  than 134) and preserves inert saves without Solve. Handler syntax keys and
  shared binder/allocation bounds remain open. See the priority plan for final
  gates, the rejected eager-retention draft and publication status.
  Verified local follow-up: remove handler syntax keys by borrowing the existing
  prepared handler environment through the same producer view. The permanent
  same-handler/128-environment regression fails before this change and passes
  afterwards without saving foreign scopes or advancing Solve. All source
  reference registrations then use scope/binder/allocation pointers, so remove
  the obsolete syntax-key lookup (but retain syntax-dependent origin ordering).
  Final acceptance passes as recorded in the priority plan. This remains local
  until a substantive epoch; shared binder/allocation bounds remain open.
  The latter now has an executable failing gate: `source_io_test
  binder-environment-bound` grows 2 -> 130 candidates for one selected Lambda
  environment, including a real shared constructor-wrapper allocation.
  Exact-scope-only and added member-origin trials were withdrawn:
  the first drops 80 retained QuickSort origins; the latter still fails inert
  byte equality and the Match alias-isolation test. Do not repeat those key-only
  substitutions or waive the retained-origin tests. See the priority plan for
  the next joint lexical/allocation reachability audit and reproducer status.
  Reaudit on `ef5e53d`: replacing declaration/Match address keys with the
  existing nearest-binder/member keys failed `member_use_origins`. After
  specialization and inert resaves, a constructor reference changed nominal
  identity (alpha comparison also failed). The trial is withdrawn. The next
  design must join selected lexical provenance with retained allocation
  dependencies even when specialization erases the original binder; nearest
  binder reachability alone is insufficient. The priority plan records the
  reproducer. Do not weaken the exact retained-reduction/source identity test.
  Current resolution (2026-09-19): exact environment allocation keys plus
  `(parent, binder)` lookup preserve retained origins without enumerating
  foreign-parent uses or unrelated sibling binders. Both 128-environment
  bounds now pass, with unchanged image bytes and no Save-time Solve.
  This supersedes the historical open-bound statements above. Selected
  environment/binder pairs are still joined; do not claim linear complexity
  or completion of the broader A3-A5 work. Final epoch gates and costs are
  recorded in the priority plan's paired environment lookup section.
  Previous stage (2026-09-19): declaration/Match references use their
  actual family/matcher/induction addresses, registered idempotently when the
  allocation becomes available, including unaccepted imported inputs. Remove
  their opaque-root grouping and redundant writer object-wait phase; keep
  exact syntax and lexical ancestry checks. An additional 128 pending sibling
  declarations no longer enlarge the selected lexical candidate set. Another
  128 unselected names explicitly borrowing the same declaration are excluded
  from saved origins. Such shared-address uses are still inspected; this is
  not a strict retained-output bound. Full checks, unchanged image resaves and
  publication status are recorded in the priority plan. Other A3-A5 gates stay
  open; no extra semantic owner or allocation cache was introduced.
  Verified follow-up: declarations and Match now use the member lexical key.
  The existing reference index separately maps family/matcher/Self to the
  allocation's existing dependency Context. Registration distinguishes exact
  object/Context pairs; it does not equate allocations, motives or proofs.
  Context binders recover only candidates for that exact allocation, including
  after specialization erases the original binder. Do not discover unrelated
  members early: that draft reordered a retained QuickSort image. Final old-image
  resaves remain byte-identical. Each of the declaration and Match regressions
  adds 128 unrelated shared-allocation aliases without changing selected bytes
  or lexical candidate count; saving does not Solve or create proofs.
  Parameter-prefix traversal and distinct families sharing one matcher remain
  part of the open bound; this is not A3 completion. The priority plan's
  `21faa54` measurement records 52 prefix steps over 14 allocation/Context
  pairs in each of fresh and inert retained QuickSort saves. This is repeated
  work, not evidence of a major runtime bottleneck. Surface parameter absence
  does not imply an empty allocation Context: a Universe binder can remain.
  No new index or empty-Context pruning follows from that failed assumption.
  The priority plan's
  allocation-context epoch groups this with `e5d1f28`/`6fb1db6`, records full
  optimized acceptance, affected sanitizers, costs and publication preparation.
  Registration follow-up: input owners now publish each reverse-reference edge
  once. Imported declaration/Match inputs publish at attachment; fresh inputs
  publish at their first finish. Qualified requests and binding interning
  already have one-shot construction paths. Delete the per-object duplicate
  scan rather than add another index or registered-state field. A source-image
  fixture previously performed 16,589 comparisons for 433 registrations, of
  which only two were redundant imported-input finish calls. The new path
  performs 431 registrations and no duplicate scan, preserving identical
  image bytes. Full gate/publication status is in the priority plan. This
  removes quadratic insertion work, not the remaining lexical selection bound.
  Remaining bound: transparent lexical descendants within one selected root
  remain candidates. Do not claim cost strictly proportional to retained output.
  Reaudit at `8ee3826`: matcher reverse discovery and matcher waiters are both
  necessary for the current representation. Deleting either changes the
  semantic constructor pointer after inert saves of `&(Box Nat).mk` (not merely
  alpha renaming). The priority plan records the reduced counterexample and
  new cross-process `specialized-constructor` matrix. Replace this discovery
  only after tracing a precise allocation dependency; deleting it or interning
  nominal layouts by shape is not a valid optimization.
  Subsequent direct-origin implementation preserves matcher discovery without
  requiring a prepared constructor proof. At source allocation registration,
  resolve the owning input's lexical binder against its immutable allocation
  Context once. Replace Context descriptors with direct references to those
  eligible inputs in the existing index. Delete the writer's Context/candidate
  join, not its matcher waiters or lexical/syntax checks. Imported inputs use
  the same path before Solve. Specialized-constructor retention, 128 independent
  same-matcher families and modified Match roots pass; full O2, affected debug
  and sanitizer gates pass. The priority plan records measurements/publication.
  This removes repeated joins, not the remaining same-binder candidate bound.
  The member-only binder-frontier trial reduced callbacks (132 -> 4), but
  failed retained QuickSort inert resave byte equality and was withdrawn.
  Preserving per-key registration order did not repair the failure. Nominal
  origins also need erased-layout reachability, not only retained binders.
  See the priority plan for the counterexamples; this gate remains open.
  Follow-up on 2026-09-19: the member-only binder boundary now passes the
  retained-image gate when each reached reference batch is dispatched in
  selected syntax order, rather than hash/registration order. The previous
  images differed at exactly four bytes, exchanging syntax IDs 68/74 in two
  producer and two origin records. A selected member save performs four
  callbacks both before and after 128 unused binder scopes (previously 132
  afterward). A temporary sorted reference array holds no result or acceptance
  flag. Ordinary/retained selection and exact lexical ancestry checks remain.
  Declaration/Match candidates still cross transparent binders because the
  selected erased layout may omit them. This does not close the broad A3 bound
  or claim canonical ordering for every possible externally constructed lexical
  graph. See the priority plan for regression, sanitizer and publication gates.
  Follow-up: reference visitation now yields source candidates without probing
  allocation availability. The writer selects syntax, reads the allocation,
  then checks lexical ancestry only for reached objects. Existing temporary
  waiters retain their resumption callback instead of recovering a phase from
  the wakeup key and inspecting source inputs again. Global enumeration of
  available allocations retains its old contract. This removes repeated reads,
  not the remaining descendant candidate scan; see the priority-plan measurements.
  Follow-up audit: indexing solely by the erased allocation object is not a
  substitute for lexical reachability. `member_use_origins` has 128 unselected
  named uses sharing the exact same allocation binder. A scope-by-syntax join
  would instead introduce a selected-scope/selected-syntax cross product; it
  has not been adopted as an output-sensitive solution. Keep this item open.
  On 2026-09-19, allocation references became keyed by their nearest opaque
  lexical scope, not globally by syntax. Binder/definition/handler extensions
  are transparent only for grouping; exact syntax and ancestry checks remain.
  There is one borrowed producer edge, no new persistent cache or completion
  hook. Selected scopes drain once; syntax/site/scope dependencies use the
  writer's existing temporary frontier. With 128 unrelated named scopes sharing
  syntax and binders, selected lookup callback count is unchanged, as is the
  selected image. This closes that regression, not all lexical complexity.

  Two unpublished drafts were rejected. Immediate scope-order dispatch changed
  inert-resave ordering; staging through the syntax frontier fixes it. Iterating
  a hash bucket while callbacks inserted dependencies let rehash rewrite its
  links and skip append's recursive `(List A).cons` origin. The event index now
  owns detachable waiter lists, so callback insertion cannot change iteration.
  The permanent retained-append-origin regression fails on the deficient image
  and passes on the repaired one, including two byte-identical zero-Solve
  resaves. It identifies the IH scope, not the preliminary no-IH candidate.

  Retained IF8/main versus `a0cbc2e`: allocation-origin dispatches 85 -> 76,
  collection callbacks 93 -> 84, environment reads 750 -> 731. All 76 selected
  origins and the 420,556-byte image are preserved; Solve remains 128,565 steps.
  Do not claim fresh-image byte identity across versions. Selected arena usage
  increases 501,344 -> 504,096 bytes (+2,752), capacity stays 557,056; persistent
  references stay 514. This is a work/isolation improvement, not a memory win.
  Strict debug source tests, full optimized acceptance and source/image
  ASan/UBSan tests pass. Logs: `/tmp/a-program-authority-lexical-roots-final-*`;
  debug source log: `...-waiters-source.log`. Implementation/headers +80/-14
  (net +66); tests +65/-1. Cumulative implementation remains +1,427 from R76,
  +3,843 from R0. Publication waits for a substantial epoch; A3-A5 and the
  parent's net-negative requirement remain open.
  The whole-job/whole-binding searches described below are now removed.
  At `8d4c731`, ordinary IF8 saving inspects 15,319 jobs, indexes 85 source
  origins and 344 binding addresses, but selects none of those candidates.
  The initial object closure still contains two prelude binders and two host
  objects. A trial deferring index creation until that closure became nonempty
  did not reduce any count and was withdrawn. Retained `--whnf main` saving
  inspects 15,402 jobs and the same candidate counts, with 93 origin and 282
  binding callbacks. A correct direct lookup must cover pre-completion child
  allocations and zero-step restored descriptors, not just completed parents.
- [x] 2026-09-19: replace unconditional source-origin discovery with an index
  of borrowed input references, registered at source request/address creation.
  Syntax keys find existing allocation-producing jobs; binder keys find their
  existing lexical addresses. No result, allocation tuple, acceptance flag or
  completion callback is stored in the index. Read availability through the
  original job/view, including completed children and inert restored inputs.
  `source_io.c` advances its selected syntax/object frontiers and no longer
  enumerates all jobs or all source bindings. The existing temporary candidate
  table also retains binder-first/syntax-later edges; no extra solver work runs.

  A rejected first draft indexed constructor bindings by their constructor.
  `member_use_origins` now retains only a field Context whose type mentions Nat,
  not Box's constructor. The draft lost its binding address (exit 134); binder
  reverse lookup preserves the one address and zero syntax through two inert
  resaves. Constructor-site discovery alone is insufficient. This failure was
  in the unpublished draft, not evidence of a bug in the preceding release.

  Same-input GDB counters (`f8fb837` archive versus this epoch, `-O0 -g`,
  IF8 fixture, `--steps 1000000 --legacy-intrinsic-dot`):

  | Save mode | Old job entries inspected | New index bucket candidates inspected | Old/new binding callbacks | Selected origin / binding callbacks (unchanged) |
  |---|---:|---:|---:|---|
  | Ordinary `--save` | 15,319 | 1,425 | 344 / 0 | 0 / 0 |
  | `--retain-reductions --whnf main --save` | 15,402 | 1,584 | 344 / 282 | 93 / 282 |

  Ordinary output is byte-identical; retained files are both 420,556 bytes.
  Before saving, Solve work is unchanged: 47,046 steps, 15,319 jobs, 23,315
  proofs, 19,522 occurrences, 11,969 Terms and 344 binding addresses.
  The 473 reference entries increase graph arena used bytes from 19,755,840
  to 19,786,112 (+30,272); capacity grows by 32,768 bytes/two blocks. The new
  512-bucket index additionally uses 4,096 bytes. This is an explicit space/work
  tradeoff, not a claim of reduced compiler memory or measured elapsed speedup.
  Logs/builds: `/tmp/a-program-authority-source-sites`, with `-baseline`/`-new`
  prefixes for `-counts`, `-retained-counts` and `-arena` logs. Full optimized
  acceptance and affected sanitizers pass; details are in the priority plan.
- [x] 2026-09-19: exercise 128 unselected sibling scopes sharing one qualified
  member syntax and constructor-field binder. Saving the original selected
  source before/after those requests produces byte-identical images; saving
  neither advances Solve nor adds proofs. This establishes isolation for that
  case, not output-sensitive discovery for every lexical scope shape.
  The test exposed quadratic candidate-table insertion: each new edge scanned
  all earlier edges sharing its object. Delete that duplicate search. Source
  reference registration is unique per job/address, and selected syntax/object
  frontiers visit each key once. Declaration family/layout edges have distinct
  object keys. Already processed objects are collected immediately without
  retaining a useless future candidate. No new index or acceptance state.

  Strict-debug GDB counts on `source_io_test constructor-inputs`, with the same
  added test before/after the production change: 348 candidate registrations
  and 334 origin callbacks in both runs; the old insertion loop performs
  16,785 candidate comparisons, and is now absent. Candidate registrations in
  this case all precede their object frontier, so its immediate-collection
  optimization does not reduce the 348 allocated entries. These are work
  counts, not wall-clock speedup. The linear same-syntax candidate search and
  lexical ancestry checks remain, so the broader A3 bound stays open.
  Logs: `/tmp/a-program-authority-siblings-{baseline,new}-counts.log`.
- [x] Share Context dependency discovery across selected allocations. Remove
  `collect_allocation`'s temporary serialization payloads; a writer-local DAG
  visits each parent/index Context once and adds its binder/type dependencies.
  The packer uses the same edge iterator. Final serialization still assigns
  wire IDs in the existing root order and validates the complete payload.
  This is not formation evidence or a new persistent Context authority.
  Producer reachability and lexical origin records remain distinct: the latter
  are a selected allocation subset, not duplicate definitions to discard.

  On the retained IF8 QuickSort/main image, intermediate packing calls fall
  from 95 to zero, Context edge-iterator calls from 929 to 689; final packing
  and eight descriptor payload packs remain. Output is byte-identical.
  At final retained writing, rules-arena used bytes fall 199,392 -> 180,352.
  The discovery DAG adds 10,176 used arena bytes and 2,048 bucket bytes:
  these measured stores use 6,816 fewer bytes overall, but reserved capacity
  increases by 2,048 bytes and the collection stack record by 136 bytes.
  Do not call this a peak-RSS reduction. Logs use
  `/tmp/a-program-authority-context-discovery-{before,after}-{counts,memory}.log`.
  The graph regression checks repeated shared parents and family index edges
  without allocating Core or evidence. Full verification is in the priority
  plan. The separate scope-sensitive source-reference bound remains open.
- [x] Read and resave zero/partial/completed inputs without executing requests
  or promoting saved results into acceptance. Reuse context/occurrence payloads
  where appropriate; do not duplicate their maps in a new alias wire record.
- [x] Evaluate and remove the unused occurrence-codec split. Its experimental
  shared-section tests passed, but source allocation transport needs only
  existing binder references, not an additional typed-graph section. The
  uncommitted split and its API-only tests were withdrawn together; existing
  occurrence transport tests remain intact.
- [x] If the format must change, update magic, headers and seed tests together.
  Explain the actual newly transported reference, not just the version number.
- [x] Remove obsolete proof-shape allocation recovery only after its pending
  cases have descriptive replacements. A source-only image must still work.
  `pg_synthesis_match_allocation` still obtains a fresh completed elimination's
  operand Contexts from its typed occurrence; restored inputs use the explicit
  allocation tuple. This is read-only, not proof replay or source synthesis.
  Both are needed: a source-only fresh compilation has no imported tuple,
  whereas a zero-step resave has no accepted elimination. The fresh path is a
  temporary projection of typed operands, not a second stored authority or
  proof-constructor traversal. Keeping it avoids adding an accepted allocation
  cache. The separate member getter traversal has been removed as noted in A1.
- [x] Remove the completed Handler return-binder fallback from
  `pg_synthesis_allocation_object`: its only clients were test assertions;
  Handler bindings already use the single lexical source-binding registry.
  The boundary test now checks that the accepted return Lambda uses that
  registry's binder, rather than checking a reader against the same operand
  it reads. Normalization, effects, rejection and two inert resaves remain
  covered. Verification/publication is tracked in the priority plan below.
- [x] Replace source Lambda/Pi binding-origin theorems with relocated binder
  references plus their existing source annotation. Remove the restore adapter;
  retain real Context/proof roots and test unchecked/invalid annotations.
- [x] Use the same source-binding address transport for Handler/Fold clauses;
  remove proof-shaped Handler origins and their reconstruction, not the
  operation/carrier/continuation checks or independently selected proof roots.
- [x] Replace qualified-constructor-use substitution origins with existing
  field Context allocation inputs, including explicitly selected pending and
  rejected requests. Recheck types normally; do not import their old theorem.
- [x] Replace recursive source Match origin theorems with descriptive allocation
  inputs. APGSRC60/61 transports prefix/motive/source-branch/erasure Contexts
  through `pg_contexts_pack` and the same Core relocation. Delete the origin
  completion wait in `match_step`; `pg_synthesis_restore_elimination` accepts
  this immutable tuple, not a derivation job. Re-form motive index/scrutinee
  types using the existing telescope lifting and source formation/parameters.
  No saved motive or classifier is treated as an expected type.
  Source branch scopes and recursive-erasure scopes remain separate because
  their binders represent different Lambda constructions, not duplicate proofs.
  `match_motive_authority` retains independent weaker and rejected proof roots;
  the source still synthesizes TOTAL, through chunks 1/64 and two inert resaves.
  `match-origins` checks immutable conflicts, invalid counts, unchanged jobs and
  proof counts during writing, and exact source/erasure symbols after loading.
  All 44 retained source checks pass in
  `/tmp/a-program-authority-match-description-source.log` (debug).
  The first run exposed an IH scope prefix check comparing complete Context
  pointers. The stored/new prefixes had identical binders and alpha-equal types
  with fresh internal Lambda symbols. Reuse `same_context_binders` there and in
  the existing schema adapter: saved annotations are not acceptance evidence;
  source field types and each IH are still constructed by ordinary rules.
  Full debug acceptance is running; optimized/sanitizer acceptance is pending.

### A4. Audit remaining structural consumers and remove duplication

- [x] 2026-09-19: private Solver storage is selected by its existing immutable
  role, with role-directed cleanup. Derivation checking retains simultaneous
  normalization state; expression slots remain separate within their owner.
  Headers shrink 368 -> 336 bytes, saving 1,097,152 retained bytes on QuickSort
  without changing work or proof counts. Full debug/O2 and affected sanitizer,
  cancellation and image gates pass. The integrated three-change epoch is
  implementation net +1, not LOC reduction; see the priority plan for timing.
  Pending construction, original R0 comparisons and cumulative reduction remain.

- [x] 2026-09-19: rule/substitution/family-action request factories no longer
  allocate a concatenated key before interning. One interner reads their fixed
  prefix and borrowed dependency array and stores new requests in the original
  flat format. Owner checks, exact ordered dependencies and rule-header keys
  remain unchanged. QuickSort drops 7,492 temporary key arenas with identical
  persistent counts and Solve steps. Full O2 and affected debug/sanitizer/image
  checks pass; implementation net -8. This is local progress for a subsequent
  publication epoch, not completion of pending construction or A5.

- [x] 2026-09-19: constructor occurrence assembly borrows the checked instance
  map's field suffix instead of projecting proofs into a temporary array.
  Dependent field checks and exact alternative derivations remain intact.
  Debug IADT, full O2 acceptance and targeted sanitizer/image checks pass;
  mixed paired timings are recorded in the priority plan. Implementation -7
  lines; no new authority or schema. This is local A4 progress, not completion.

- [x] 2026-09-19: structural queries resume their selected dependencies instead
  of rediscovering source preparation. Classifier WHNF, accepted-subject
  priority and symbolic effect snapshots are preserved. QuickSort repeat
  inspections after selection fall by 4,879 (classifier, term, declared type),
  with identical Solve/retained-data counts. Request/Fold also share the
  existing checked constant-codomain operation without duplicate prechecks.
  Full debug/O2 acceptance and affected ASan/UBSan/image/handler-boundary tests
  pass; the priority plan records measurements and epoch publication. This
  does not close the remaining pending-construction or original A5 gates.

- [x] 2026-09-19: IADT recursive-field classification inspects the application
  head before checking independence. Non-Self heads no longer scan arguments
  and then scan the whole type again. QuickSort comparison transitions fall
  36,013 -> 20,608 with identical Solve and persistent structural counts.
  Full O2 acceptance and affected debug/ASan/UBSan/image gates pass. See the
  priority plan for coverage, mixed paired timings and +3 source lines.
  No cache or new authority; pending construction and A4/A5 remain open.

- [x] 2026-09-19: measure repeated substitution composition before introducing
  a cache. On universal QuickSort, only 1,595 of 18,673 image visits repeat;
  a new persistent pair cache is not justified by this measurement. Instead,
  replace fixed-array temporary arenas in substitution building/composition
  with sized arrays, matching existing projection ownership. Full O2 and
  affected sanitizer/image tests pass; semantic counts/steps are unchanged.
  The priority plan records allocation traffic, paired timings and +1 source
  line. Published in epoch `21cf3ff`; remaining A4/A5 gates stay open.

- [x] 2026-09-19: Handler assembly resolves its clauses into the existing array
  once, then shares it between effect collection and rule assembly. Remove the
  second scan/cursor and count reset. Retain operation descriptors separately
  from the body's checked name reference; invalid references still reject.
  Full O2 acceptance and affected debug/sanitizer/source/image tests pass.
  The priority plan records mixed costs and implementation -18 lines. This is
  published epoch `21cf3ff`, not A4/A5 completion.

- [x] 2026-09-19: type-case checks closed dependent signatures without discarded
  field scopes (`bc3745c`); source Match/IH validation reuses its existing scope
  producer instead of constructing a fresh one. Final kernel elimination
  checks remain. Debug, full O2 and focused sanitizer/image gates pass; the
  priority plan records capture/dependency tests, exact-premise reuse tests,
  reduced QuickSort work/storage and LOC. Final source induction also supplies
  those scopes through the existing checked `_at` API, preserving retained
  allocations. Published together as epoch `0137156` after full gates;
  overall A4/A5 and the reduction gates stay open.

- [x] 2026-09-19: both transport branch builders use the existing scheduled
  constructor-scope producer. A retained cursor subscribes to completion before
  constructing branch bodies; it does not create another scope authority.
  Regression checks exact shared binders at chunks 1/64 and fails beforehand.
  Full O2 acceptance and focused debug/ASan/UBSan source/image gates pass.
  The priority plan records the integrated epoch, increased scheduling/storage
  costs, mixed timings and unmet reduction gates. Match/type-case kernel scope
  validation and overall A4/A5 remain unfinished.

- [x] 2026-09-19: distinguish repeated pattern reindex lookups from fresh scope
  construction. On QuickSort, all 37 repeated reindex requests reuse accepted
  evidence without new proof/occurrence/map/query/action records. No extra cache
  is justified. Four constructor-scope requests repeat across distinct index
  transport consumers and allocate fresh binders; examine existing scheduled
  scope reuse next, preserving explicit allocation and proof identity. See the
  priority plan's branch/pattern audit. The transport epoch remains unpublished.

- [x] 2026-09-19: share resumable boundary telescope preparation between
  constructor disjointness/injectivity and constructor-field index transport.
  Delete `index_rebase` and the synchronous `constructor_transport_context`;
  reuse existing rebase/lift/occurrence-action work and stable fresh binders.
  The parameter-map consumer also advances its existing rebase query. There
  are no `pg_prove_substitution_rebase` calls left in `synthesis.c`. Full O2
  acceptance and focused debug/sanitizer gates pass. The priority plan records
  costs and the separate, still-open branch/pattern audit; this is not A4/A5
  completion or authorization for a standalone Main push.

- [x] 2026-09-19: index-transport scope preparation retains its accepted map,
  field cursor and existing rebase query across Solve turns. Each turn advances
  that query with budget 1; direct and normalized attempts share the prepared
  scope. Constructor-field attempts retain their selected field values too.
  The oldest-first dependency checks and candidate rejection rules remain.
  Full O2 acceptance and focused debug/ASan/UBSan tests pass, including 45
  transport image cuts. See the priority plan's scope-preparation entry for
  time/space costs. This is local progress, not the complete transport epoch:
  `constructor_transport_context`, branch preparation and pattern factoring
  still contain synchronous work. Keep the broader deletion and A5 gates open.

- [x] 2026-09-19: accepted name registration checks the immutable Context prefix
  without constructing/discarding a weakening proof. Actual references still
  use ordinary projection. Classifier normalization and reflexivity/family
  action share the existing read-only accepted-input check. The regression
  fails before the change and confirms no proof/occurrence/map/Core allocation
  during registration, while escaping and sibling scopes reject. Full O2
  acceptance and affected debug/sanitizer suites pass; export/step records are
  unchanged. See the priority plan's read-only validation entry for counts and
  local-only publication status. This does not close A4 or the synchronous
  transport-scope audit below.

- [ ] Follow the priority plan's corrected 2026-09-19 retained-input audit:
  Match sequencing uses the common source-binding address. Independently
  generated proof binders are not subject to an exact source-identity contract;
  retained-input reuse remains a separate measurement. The shared-binder
  lexical bound now passes using exact environments and parent/binder edges;
  the subsequent paired-key lookup removes sibling enumeration too. See the
  priority plan's measured costs and publication gates. The whole A4 audit is
  not finished, and this does not claim linear complexity in selected inputs.

- [x] 2026-09-19: Identity boundary validation reads the immutable declaration
  Context rather than assuming every Context receipt has an ordinary type in
  premise 1. A family extension puts its index Context there. The new negative
  `dependent_families` case crashes before this correction (exit 139); after
  it, the existing boundary check rejects the invalid witness without accepting
  a proof. Common family parameters remain allowed. `family_action_core` takes
  the existing Core and Context directly; no new proof tag, fallback, semantic
  authority or equality rule is added. Forward dependency-check order remains
  unchanged. See the priority plan for the final verification checkpoint;
  this correction does not complete the remaining A4 consumer audit.
- [x] 2026-09-19: an accepted map rebased to its exact destination Context
  proof returns that same map proof. Previously image rebase could choose
  another proof of the same typed image and rebuild a different map receipt.
  Do not generalize this identity case to merely equal Contexts: a distinct
  destination proof remains an explicit premise of the constructed result.
  The priority plan records the failing-before provenance regression, work
  counts and publication gates. No new query/cache/normalization rule is added.
- [x] 2026-09-19: substitution introduction validates against the immutable
  Context declaration chain, not the Context proof's premise chain. Exact
  source/destination/image proofs remain premises. Remove the unused
  `pg_prove_induction_case` synchronous adapter: source branches have used
  ordinary scheduled projection/application/abstraction since September 9.
  No induction rule is removed. The priority plan records verification and
  the remaining synchronous-consumer audit; this does not close A4.
- [x] 2026-09-19: source application uses the callee whose classifier it already
  normalized for Pi inspection. Remove the second call through the generic
  application builder; share the existing domain job between the application
  constraint and argument check, then build the ordinary APP rule. The public
  API still normalizes unchecked inputs. Identity transport keeps its separate
  checking policy, not a second application construction. A dependent two-call
  regression checks exact DAG reuse through the public API, at chunks 1/64;
  it fails on the old implementation. Full O2 acceptance and focused debug and
  sanitizer tests pass. See the priority plan for counts and local publication
  status. This is not a global classifier-job idempotence rule or A4 completion.
- [x] 2026-09-19: unify source rule selection and preparation readiness in
  `source_rule`. Notification and structure consumers inspect the same existing
  producer, with no retained view or new scheduler state. Waiting consumers
  reuse that rule instead of reading it again. Function-field inspection calls
  fall 9,385 -> 8,997 with all measured structural counts and Solve steps
  unchanged. Full O2 acceptance and affected debug/sanitizer gates pass; see the
  priority plan for logs, the corrected test fixture and publication status.
  This deletes two independent dispatch paths, not the pending/accepted
  distinction. A3-A5 and the cumulative reduction gate remain open.
- [x] Remove `context_binders` and `substitution_state`'s copied scope array
  and length. Constructor binding registration streams the original parameter
  Context through the same address cursor as source scopes and imported arrays.
  Existing explicit allocation checks are unchanged; this does not merge
  Context evidence, Core terms or distinct lexical addresses. A two-level
  Context/array lookup regression checks exact binder identity without adding
  another registry entry. Verification is tracked in the priority plan.
- [x] 2026-09-18: remove variable-proof construction from `binding_context`. A source name
  refers to an already accepted Context extension; checking its existing owner,
  sort, parent and binder does not require constructing the bound variable.
  `accepted_inputs` now asserts a fresh unused binding creates no evidence,
  occurrence or Core term, and rejects foreign Context evidence, a mismatched
  binder and the wrong lexical parent. The no-allocation assertion fails before
  the change (exit 134) and passes afterward; the complete `synthesis_test` and
  `source_io.sh` pass in `/tmp/a-program-authority-binding-read` (debug).
  On the same retained append image, GDB reports unchanged 6032 Solve steps,
  1801 jobs and 658 typed occurrences, with accepted proofs reduced 911 -> 910.
  This deletes an unrequested variable receipt, not an alternative requested
  derivation. No scheduler state or structural owner is added.
  Full debug acceptance exits zero afterward, including 63/63 compatibility
  and 4446 Handler snapshots (`/tmp/a-program-authority-binding-read-debug.log`).
  All exported results agree with the telescope baseline. The retained
  QuickSort property cases use one extra Solve step at both chunk sizes; other
  `export results:` records agree exactly after temporary-path normalization.
  Removing eager proof creation is not claimed as a global speedup; A5's
  complete work/timing audit remains open.
  ASan/UBSan `synthesis_test` and complete `source_io.sh` also exit zero in
  `/tmp/a-program-authority-binding-read-sanitize`; logs end in
  `-sanitize-synthesis.log` and `-sanitize-source.log`. The final ownership
  change had not yet had full optimized/full sanitizer acceptance runs at that
  checkpoint; the subsequent read-only-view checkpoint below includes it.
- [x] Share the kernel's existing elimination input view with source Match.
  `match_motive_context` selects its Context through the typed construction,
  not receipt slots 0/4. The retained-motive check has since been removed below.
  Export the existing view and remove its private forwarding adapter; do not
  introduce a second view algorithm, job, cache or persistent record.
  `match-origins` verifies repeated reads of saved/fresh accepted eliminations
  and an alternative motive derivation create no jobs, evidence or typed
  occurrences. Both derivations remain present with their own premises.
  It rejects foreign evidence, non-eliminators and null inputs.
  The initial focused test and complete `source_io.sh`
  pass in `/tmp/a-program-authority-match-view` (debug).
- [x] Make `pg_elimination_view` genuinely read-only (`const pg_typing *`).
  Direct Match/induction acceptance registers every operand, map and motive
  Context premise. Read those exact conclusion indices; remove calls to
  `pg_prove_structural_subject` and `pg_prove_context_map` from inspection.
  No alternate derivation is replaced. Extend `match-origins` to assert that
  reads allocate no typed queries, actions, Contexts or maps, in addition to
  the existing proof/occurrence/job checks. Debug IADT, synthesis and the
  complete source-image suite pass in `/tmp/a-program-authority-readonly`.
  Full optimized `check-acceptance` passes in
  `/tmp/a-program-authority-readonly-o2` (log with the same prefix), including
  63/63 compatibility, 4446 Handler snapshots and final retained QuickSort.
  Compared with `binding-read-debug`, normalized `export results:` records
  differ only by two fewer steps in each retained QuickSort property case.
  This cross-build observation is not a measured speedup or final A5 evidence.
  ASan/UBSan IADT and complete source-image tests also exit zero in
  `/tmp/a-program-authority-readonly-sanitize` (O0, frame pointers, non-PIE),
  without reported sanitizer diagnostics. Full sanitizer acceptance is pending.
- [x] Remove saved-motive comparison/replacement from `match_step` (16 lines).
  An origin supplies lexical allocation, not the result type of newly checked
  source. `match_motive_authority` attaches an independently valid induction
  with UNSPECIFIED totality to a source Match which synthesizes TOTAL. Both
  remain valid with their own classifiers; the source does not adopt the saved
  motive. Cover chunks 1/64, direct checking and two inert source resaves.
  Restoring the old block makes the direct regression fail (exit 134); deleting
  it passes all four cases. Full debug `check-acceptance` exits zero in
  `/tmp/a-program-authority-source-motive-debug.log`. A preceding O2 writer's
  retained QuickSort image also passes the current debug reader at chunks 1/64
  (727037 steps, equal=1). No wire-format change or equality relaxation.
- [x] Keep the accepted generic motive Context directly in Match state instead
  of an `EVIDENCE_JOB` used solely as a container. When projection is actually
  requested, use the existing `projected_image` helper. The selected motive's
  Context remains separate because it also marks completed motive synthesis.
  This removes an unnecessary wrapper, not pending computation or proof checks.
  Full optimized `check-acceptance` exits zero in
  `/tmp/a-program-authority-direct-context-o2.log`, including 63/63 compatibility
  and retained QuickSort. The same previously saved QuickSort image still takes
  727037 steps at chunks 1/64; separate freshly written images have different
  scheduling counts and are not a controlled performance comparison.
  ASan/UBSan IADT and the complete source-image script also exit zero, without
  reported diagnostics (`/tmp/a-program-authority-direct-context-sanitize-`
  `iadt.log` and `source.log`). The last full debug run precedes this wrapper
  removal. Full final sanitizer/performance/LOC/publication gates remain open.
- [x] Read constructor fields through the existing accepted data schema.
  Replace the narrow `pg_evidence_inductive_declaration` getter with
  `pg_evidence_inductive_schema`; callers needing the declaration/layout use
  the existing schema accessors. `data_schema_step` no longer guesses the
  constructor-premise offset from VALUE_TYPE versus TYPE_FAMILY. This is a
  borrowed immutable pointer, not an inductive-instance query or recomputation.
  At this checkpoint both declaration and Match origins still required checking
  and APGSRC58/59 was unchanged. The subsequent A3 Match migration removes its
  origin requirement; declaration restoration remains unfinished.
  GDB on the same retained function-field image reports unchanged work before
  and after these reads: 4468 Solve steps, 1247 jobs, 836 accepted proofs
  (`authority-telescope` versus `authority-direct-views`, both debug builds).
  Full debug acceptance after the view/getter migration exits zero, but
  comparison with the preceding log exposed three additional Solve steps in
  each retained QuickSort property case. `match_step` was still comparing
  motive receipt pointers. At that checkpoint it compared exact typed subjects and kept the
  independently synthesized evidence when the subject already agrees; distinct
  subjects still require conversion. The expanded alternative-proof regression
  passes after this correction. Full optimized acceptance exits zero in
  `/tmp/a-program-authority-direct-views-o2`; its complete `export results:`
  stream matches the preceding telescope-debug run after temporary paths are
  normalized, including QuickSort Solve counts. Log:
  `/tmp/a-program-authority-direct-views-o2.log`. This precedes the separate
  binding-ownership cleanup below, not final A5 acceptance. The comparison and
  replacement themselves are now removed by `match_motive_authority` above.
- [x] Share the checked positional telescope substitution already used by
  refined Match with `schema_result_context`; delete its separate image-array
  construction. Preserve the source restoration guard requiring identical
  binder order. No new rule, job, saved state or equality admission is added.
  `core_test` verifies exact proof reuse, incompatible fields, different
  lengths, non-Context inputs and foreign ownership. `context-scopes` accepts
  an alpha-renamed Acc field (`y` -> `z`) but rejects `R y x` -> `R x y`,
  at chunks 1/64. Both pass in `/tmp/a-program-authority-telescope` (debug).
  GDB records 18 schema adapters, four with distinct source/target Contexts:
  the two valid renamed cases and the two invalid relation edits. Dropping
  adaptation rejects valid renamed fields; checking only result-index images
  can admit incompatible ones because the recursive field is unused in that
  index. Saved declarations remain immutable.
  The implementation/header change for this consolidation is net -4 lines;
  it does not eliminate the remaining declaration/Match origin dependency.
  Full debug `check-acceptance` exits zero after this change, including 63/63
  source compatibility, 4446 Handler snapshots and final retained QuickSort
  results at chunks 1/64. Log: `/tmp/a-program-authority-telescope-debug.log`.
  ASan/UBSan `core_test` and `source_io_test context-scopes` also exit zero
  in `/tmp/a-program-authority-telescope-sanitize`. The current consolidation
  has not yet had full optimized or full sanitizer acceptance runs.
- [x] For `pg_synthesis_allocation_object` and restoration helpers, classify
  each field as allocation description, checking input or acceptance. Read
  available binders/context/induction layout from structural owners, not nested
  theorem shapes. Keep constructor/handler/IADT checks explicit.
  On 2026-09-18, A3 removed the Match origin wait using descriptive transport.
  The later declaration epoch also removed the formation-theorem wait, using
  descriptive transport and ordinary rechecking while retaining independently
  selected proof obligations. Do not treat this historical dependency as an
  outstanding implementation path. The A1 reference table above classifies
  fresh Match/member reads separately from the removed Handler fallback.
- [x] Audit `computation_view`'s structural/origin fallback in `function_graph.c`.
  Remove repeated discovery when querying the same effective input; preserve
  the distinction between current reduced head and historical construction.
- [x] Audit `identity_structure` and evidence-conclusion lookups. Looking up a
  valid proof of one fixed subject is not itself wrong. Verify boundary maps
  and direction come from that subject, not whichever proof was found first.
- [x] Check `typed_query_request` sharing with alternate derivations: same
  semantic inputs share computation without overwriting or dropping explicitly
  requested premise DAGs. No blanket merge of theorem-specific jobs.
- [x] Remove `SOURCE_ORIGIN_JOB`, which never executes: store its existing
  source naming/layout links in a small typed-subject index, not a work item.
  Remove its two dedicated fields from every synthesis job. Reads do not create
  empty registrations. This is not a new checking or alias mechanism.
- [x] Replace application origin restoration with the existing raw allocation
  input. Remove `pg_synthesis_restore_application` and its proof-completion
  wait/prefix reconstruction in `prepare_application`. APGSRC48/49 transports
  the `(prefix, end)` Context pair in the shared context payload. No acceptance
  result or additional semantic allocation store is introduced.
- [ ] Report concrete deleted paths. Move remaining synchronous query loops to
  existing scheduling only where necessary for the same work contract; do not
  turn this repair into another scheduler framework.
- [x] 2026-09-19: index transport's decreasing-dependency test resumes the
  existing structural comparison one transition per Solve turn, retaining its
  field/phase/counts in the existing private transport state. Cancellation frees
  comparisons; no extra job kind or accepted evidence store is introduced.
  Constructor boundary prefixes borrow accepted map images and use the shared
  map-rebase query instead of an allocated per-image reconstruction loop.
  The priority plan records cut/resume coverage and final publication gates.
  Context membership lookup and the remaining transport/scope builders are still
  synchronous; this is neither a wall-clock bound nor completion of A4.
- [x] Remove path-dependent scope expansion from structural independence
  checks: comparing the same Core under `x -> absent` does not need fresh scope
  entries for unrelated lambdas. Preserve the shared query key and discharge
  the constraint at a lambda binding x. A 97-node diamond DAG failed the old
  1,000-transition gate and now visits 97 tasks; shadowing and image-resumption
  checks pass. No new tag/store or alpha interning. Final regression and epoch
  publication evidence belongs to the priority plan; this does not finish A4.
- [x] Local Lambda structure correction after `f3da3c3`: read the existing Pi
  binding address and body, without reconstructing the whole classifier just
  to find its binder. Share Context/input binding extraction with Pi structure.
  Opaque producers still wait when they actually determine that address; ordinary
  Pi/body acceptance remains independent. Pending normalized-codomain and invalid
  raw-body regressions pass at effect closure. The priority plan records final
  gates and local publication status. This is not completion of pending
  classifier reconstruction, A4 or A5.
- [x] 2026-09-19 local cleanup: seven constructor/family/function-graph consumers
  borrow existing immutable accepted premise slices instead of allocating and
  copying them. Transformation arrays and negative-test copies remain owned.
  No new accepted state or reconstruction rule. The priority plan records the
  exact validation/publication status; this does not close A4 or A5.
- [x] 2026-09-19 local work: remove synchronous helper-schema application from
  `function_graph.c`. The existing shared query/wait slot now resumes beta;
  one graph-private case cursor retains the current map, call and argument.
  Helper discovery and schema construction reuse a role-exclusive state pointer;
  no proof/cache authority is added. Two dependent helper calls exercise every
  cancellation boundary, zero budget and late dependency rejection. Initial
  full O2 and affected debug/sanitizer/image tests pass; final publication gates,
  the extra 64 aligned cursor bytes per graph and unchanged 47 query-work steps
  are recorded in the priority plan. Other synchronous subrules and A3-A5 remain
  open; this does not assert an overall LOC reduction.
- [x] 2026-09-19: function-graph Match planning uses the existing typed
  elimination query and shared wait slot. Splitting stores each checked
  reindexed elimination in its existing child computation; ordinary child
  planning advances iota later. Remove both `pg_prove_elimination_body` calls
  from `function_graph.c`, without another phase, field or answer cache.
  `suspended_match_body` reproduces multi-step draining before the change and
  checks one-step progress plus independent resumption after each cancellation.
  Other checked kernel subrules and helper-schema application remain outside
  this local bound. Full optimized acceptance and affected debug/sanitizer/image
  checks pass; exact results and timing limitations are in the priority plan's
  shared-Match epoch. Existing wrappers already shared the query: this removes
  synchronous draining, not duplicate proof authority. A4/A5 stay open.
- [x] 2026-09-19 local work: function-graph preparation no longer drains
  inductive/parameter-map queries in initialization or one graph turn. Existing
  graph scheduling and its shared-query wait slot own this work; a phase replaces
  the ready flag. The priority plan records the failing old-code boundary test,
  removed redundant head discovery, unchanged query-work/graph/allocation counts,
  passing debug/O2/ASan-UBSan acceptance, timing comparisons and the verified
  publication boundary. Other synchronous consumers remain open.
- [x] Audit weakening composition together with explicit map admission and
  dependent elimination transport before normalizing typed scope paths.
  The 2026-09-19 trial was withdrawn: normalizing map application broke
  admission of an explicit nested projection recipe; normalizing only the
  ordinary projection producer broke dependent Match reindexing. See the
  priority plan for reproductions. Keep the new boundary test, not either
  partial normalization or its altered scheduling assertions. Review
  `pg_occurrence_projection`, `context_map_extend`, `map_lift_work`,
  `substitution_build` and elimination reindexing as one contract. Do not repair
  exact-map disagreements with a new acceptance cache or erased-Core lookup.
  Reaudit on `a8aa05f`: retain explicit action recipes under the current
  representation. `context_map_extend` projects the existing typed images;
  `substitution_build` checks their dependent declarations and retains supplied
  image proofs; structural admission must produce the requested exact subject.
  Elimination reindexing obtains its generic motive through the same lifted
  input action. Merely equal Core/classifier results cannot replace any of
  those map edges. This closes the joint audit, not a normalization migration.
  The extended `indexed_match` test opens motives after both direct and
  two-stage weakening, checks fresh dependent binders and an unchanged nominal
  declaration, and verifies the ordinary derivations. Final tests and retained
  dispatch reasons are tracked in the priority plan. A4/A5 remain incomplete.
  Follow-up on `ccd0f7b`: ordinary weakening and map extension construct the
  selected declared variable without first materializing every variable of
  a projection map. This is the same exact output as explicit projection,
  not normalization of recipes. Converted/non-variable inputs retain maps;
  caller-supplied maps and alternate proof premises are unchanged. Fresh
  QuickSort saves 3,252 occurrences and 2,130 maps, with identical proof,
  query and Solve counts. Final gates and mixed timing results are recorded
  in the priority plan; original performance/LOC requirements stay open.
- [x] 2026-09-19 local work: map admission reuses a checked destination and
  checked prefix through ordinary pairing, without reconstructing a structural
  lift. Unknown destinations still require the existing lifting check. Keep
  exact explicit-lift premise sharing: the trial which skipped prefix reuse
  failed the permanent test and was withdrawn. Full publication gates are
  tracked in the priority plan; this does not normalize projection recipes.
  The combined epoch also reuses the existing private-work union for six
  role-exclusive pointers, retaining concurrent derivation/normalization state.
  Final debug/O2/ASan-UBSan acceptance passes; original A3-A5 gates stay open.
- [x] Share retained function-source inspection between the synchronous API,
  budgeted graph preparation, surface `@f`/`*f`, and concrete-helper lookup.
  One borrowed cursor advances existing origin/body queries; it is neither
  another result authority nor a new semantic query. Initialization performs
  no query work. Preserve checked specialization and pending-helper rejection.
  The priority plan records old-caller regression failures, final acceptance,
  scheduling/storage costs and publication. This does not bound all checked
  kernel subrules, normalize projection recipes or close A3-A5.
- [x] Resume branch field/IH/remaining-argument telescopes through the existing
  application-body query. Keep each checked extension in its existing scope
  slot while pending; reuse the later leaf-number storage for the next slot.
  Symbolic APP and Fold continuation application use the same wait slot.
  The existing branch output holds a returned value until its continuation
  completes; no synthetic Core node, result cache or new proof rule is added.
  The priority plan records failing-before cold tests, full gates and costs.
  `helper_application` in schema construction and checked kernel subrules
  remain synchronous. Pending-versus-accepted classifier-structure requests
  also remain an audit item; do not omit projection obligations merely to
  reduce request counts. A3-A5 are not complete.
- [x] Audit the proposed classifier-structure projection collapse on `ef5e53d`.
  A pre-closure symbolic query and a post-acceptance projection need not return
  the same classifier Term. `pending_effect_contexts` now verifies this for a
  Lambda at chunks 1/64; unconditional redirection to its operand fails the
  accepted-classifier assertion. Keep the test, not the trial. This distinguishes
  immutable snapshots from duplicate acceptance authorities; it does not close
  all structural sharing or the remaining A3-A5 gates.
- [x] 2026-09-19: share IH scope-map relocation through the existing map-rebase
  query. Remove Solver's per-image projection jobs and substitution-prefix
  reconstruction, and the direct API's extra projection/composition map.
  Keep constructor binder allocation and IH formation as distinct operations.
  Full optimized acceptance and focused debug tests pass; the priority plan
  records sanitizer status, semantic comparison, allocation costs and LOC.
  This is local work for the next publication epoch, not A3-A5 completion.
- [x] 2026-09-19: remove the synchronous map-image loop and `rebase_image` /
  `scope_map_step` adapters. Map restriction retains its image prefix in the
  existing rebase machine; nominal queries wait on that shared query. Explicit
  map/target-Context proof premises remain distinct, while value-image work
  stays shared by typed subject. A regression also exposes and fixes resuming
  a shared consumer before its pending dependency completes, using only APIs
  present in the previous Main. The priority plan records old-code failure,
  passing gates and costs. This does not make all kernel subchecks resumable
  or close A4/A5; implementation/header LOC grows by 20 for this epoch.
- [x] 2026-09-19: replace `scope_image`'s synchronous frame loop with one shared
  step used by selected-input and nominal-index queries. Restriction waits on
  the existing rebase query; the owning query retains its cursor and checked
  application prefix. Delete Identity constructor transport's duplicate
  parameter-rebase loop in favor of `pg_prove_substitution_rebase`. Full O2
  acceptance and focused debug/sanitizer tests pass; the priority plan records
  the old-code failure, timing and LOC. This is local work for the next epoch,
  not A4 completion: map restriction and other synchronous kernel checks remain.
- [x] 2026-09-19: helper Match inspection no longer finishes its argument-spine
  beta queries synchronously. Reuse `application_body` and the graph owner's
  shared-query wait slot; retain the next argument in the existing helper
  cursor. Cancellation releases only that cursor. The new test fails on the
  previous implementation and passes with this change. Other synchronous
  subchecks remain; this does not bound every graph turn's total kernel work.
  Verification and unchanged QuickSort work counts are in the priority plan.
- [x] 2026-09-19: `function_graph.c:prepare_head` no longer completes typed
  beta queries through the synchronous application-body adapter. Its three
  call sites use the existing shared query and `s->view` resumption slot;
  input/origin/head queries share the same wait helper. No new job or state
  field. The cancellation regression fails on the previous implementation
  (two query transitions in one graph turn) and passes after the change.
  Other synchronous kernel subchecks, case planning and witness construction
  are not claimed bounded by this change. Full O2 acceptance and focused
  sanitizers pass; query work/structure counts remain unchanged in the measured
  exposed-Match case. Outer scheduling now counts six additional turns there.
- [x] 2026-09-19: once an inductive query resolves its nominal formation,
  resume Context transport from that existing evidence in `work->value`.
  Remove repeated nominal lookup and structural-subject recovery at each
  transport step. No new field, index, acceptance rule or wire format. Pending
  queries still expose no completed instance. Function-field testing reduces
  successful lookups from 501 to 113 for the same 113 queries (maximum per
  query 10 -> 1), with identical proof/structure counts and Solve steps.
  Full O2 acceptance and focused sanitizer checks pass; see the priority plan.
  This does not resolve A3's lexical candidate bound or complete A4/A5.
- [x] 2026-09-19: share checked Pi-telescope application between IH formation,
  induction field bodies, original graph hypotheses and callable-parameter eta
  expansion (`pg_prove_call_telescope`). Delete four local traversal bodies;
  parameter eta expansion reads the applied term's existing classifier instead
  of independently projecting/instantiating its former classifier. The output
  is a stack-local borrowed triple, not a stored descriptor, query or authority.
  Binder allocation supplies identities only; accepted Pi domains remain the
  authority. Preserve fresh/retained binder policies and ordinary proof rules.
  Keep `family_parameter_context` (type-only telescope) and `induction_field_core`
  (erased Core construction) separate: neither has the computation premise
  required by this helper. Likewise pending symbolic structure is not an
  accepted classifier and cannot replace `accepted_structure`, or vice versa.
  Full optimized acceptance and focused sanitizer tests pass; measurements and
  the corrected test assumption are in the priority plan. A3-A5 remain open.
- [x] Repair Pi formation's early reuse key: insertion and lookup now both key
  by exact premises, since those premises determine the output. Alternative
  premise derivations still yield distinct receipts even for identical typed
  conclusions. No new cache or authority. The repeated-request Core experiment
  avoids 208 reconstructions; the existing function-field example does not
  improve. Full optimized acceptance and affected sanitizer/serialization tests
  pass; see the priority plan for the controlled experiment and limitations.
- [x] Reuse `pg_context_map_projection` in substitution projection introduction;
  delete the evidence wrapper's separate ancestor/arity and binder traversal.
  Check each image through the ordinary rules and retain the exact caller's
  Context receipts. On function-field, Context-extension walks fall 3,550 ->
  2,306; structural/proof counts are unchanged, but the existing projection
  index adds 184 entries. This is not a memory reduction. Full optimized and
  affected sanitizer/serialization tests pass; group publication remains gated
  by the priority plan. General descriptive-map validation is unchanged.
- [x] Count binding payload slots during first source collection; delete the
  separate scope-count and binding-count passes in `source_io.c`. Repeated
  binding discovery stops at the existing writer-local DAG. The counter is
  temporary wire sizing, not a solver answer or persistent authority. Keep
  raw derivation export copies: its API promises transport-input ownership in
  the caller's storage; merely borrowing those inputs would weaken that contract.
  Optimized full acceptance and controlled old/new resaves pass; the priority
  plan records the focused sanitizer gate and measurements. A3/A4 stay open.
- [x] 2026-09-19: unify the source writer's producer classification. Dependency
  enumeration, collection and wire encoding now read one stack-local projection
  of the original synthesis inputs. Delete their independent classification
  cascades; no persistent descriptor, extra index, Core tag or acceptance path.
  The six-word wire record and dependency order are unchanged. Source tests,
  full optimized acceptance and eleven byte-identical retained-image comparisons
  pass. This removes 15 implementation lines, not the unresolved scope scan.
- [x] 2026-09-19: share structural Context-map extension between lifting and
  occurrence instantiation (`typing.c:context_map_extend`). Delete the separate
  instantiation image-array construction. Destination extension projects the
  prefix; ordinary pairing retains it. No new tag, cache, proof or authority.
  Direct tuple construction interns to the same lift map and instantiation
  request; the regression also checks that structural work creates no proofs.
  Keep `map_lift_prefix`: imported unchecked descriptors need independent
  validation, not a producer-history shortcut. Supplied alternative derivations
  also prevent replacing checked projection with an arbitrary cached proof.
  Optimized full acceptance and affected sanitizer tests pass (priority-plan
  record below). The measured function-field fixture retains 12,809 Solve
  steps and all nine recorded consumer counts; no speedup is claimed.
  Implementation +22/-22, tests +21/-0. A3-A5 and net-negative gates remain open.
- [x] 2026-09-19: `family_function_step` now advances the existing interned
  construction-origin query by one step and requeues itself when pending.
  Remove its call to the synchronous completion wrapper; no new job, cached
  answer or authority is introduced. `source_telescopes` checks the query's
  step delta during each one-step Solve call for ordinary and normalized
  partial-family quotation. It fails before this change (exit 134) and passes
  afterward. Accepted conclusions and alternate derivations are unchanged.
  This is a scheduling correction, not evidence of faster normalization.

  At that checkpoint, synchronous consumers included `function_graph.c`'s
  `helper_call` origin traversal and `pg_function_graph_source`. They use the same shared queries, so their
  existence alone does not establish duplicate traversal or a second authority.
  Audit their surrounding graph-work budget before changing them; preserve
  pending/unsupported distinction, helper ownership and checked context maps.
  Synchronous public kernel wrappers remain valid APIs. This change does not
  establish a global constant-work bound for every Solve transition.
  Follow-up GDB check on `function-graph-helper-call.p` at 1,000,000 fuel:
  14 `helper_call` entries, 13 distinct state/plan/context/input keys, no
  pending (`2`) return; Solve completes in 24,959 steps. This does not justify
  adding a helper-resumption cache. Wider budgeting analysis is still needed.
- [x] Follow-up: reproduce unbounded origin-query draining during helper
  inspection with a projected application inside a Fold. The permanent
  `graded_function_graph` regression fails before repair: advancing the graph
  once advances its origin query more than once. Both helper-origin walks now
  use the same one-step adapter as `computation_view`, borrowing `s->view`.
  A private continuation cursor retains only the walk position and argument
  list across suspension; it is neither an interned semantic object nor a new
  proof/result authority. Normal completion and graph destruction release it.
  The first draft retried the entire helper walk after waiting; it was not
  committed. Final helper-call fixture: 14 -> 53 entries (39 pending resumes),
  but direct helper allocations remain 29, with the same 8 non-helper and 6
  helper outcomes. The cursor resumes after the processed prefix, rather than
  reconstructing arguments. This changes scheduling, not totality rules.
  `pg_function_graph_source`, application-body checking and other synchronous
  kernel consumers remain outside this local bound. It does not complete A4.
  Strict-debug program tests, optimized full acceptance (63/63 and four
  universal sorting proof suites), and ASan/UBSan program tests pass. See the
  priority plan's helper-cursor checkpoint for commands and measurements.
- [x] Audit genuine function specialization separately from projection.
  `pg_function_graph_source` must substitute captured inputs and lift under
  the mapped Lambda's capture-avoiding binder. `function_graph_aliases` now
  deliberately collides the destination binder with that Lambda binder:
  typed input exposure and graph-source specialization return the same typed
  body. One hundred repeated lookups add no proofs, occurrences, context maps
  or lifts, and advance neither the existing input query nor the lift.
  Applying the specialized function returns the substituted captured value.
  Both chunk sizes (1/64) pass. Reject the proposed extra Context-evidence
  accessor/reconstruction path: this case already shares the context action;
  removing it as "duplicate reindexing" would erase genuine specialization.
  This audit does not establish bounded first-query work in graph generation.
- [x] Incremental graph computation views now advance the existing typed input
  and origin queries rather than drain them. Pending is not an unsupported
  structure: preparation, case planning and helper inspection wait before
  normalizing or publishing a call. One borrowed pending-query pointer in the
  graph execution state resumes that work without rediscovering the enclosing
  view on every step; no new query kind, result cache or acceptance authority.
  `graded_function_graph`'s one-step input-query assertion fails before repair.
  On the function-field fixture, evidence/occurrence/query counts stay
  9,371/6,717/1,662; Solve transitions change from 11,993 to 12,809 because work
  previously hidden inside a step is scheduled. View/helper calls are 78/36
  versus the synchronous baseline's 30/23. The unpublished first draft
  retried views on every pending transition (846/103) and slowed the QuickSort
  diagnostic by about 10%; it was replaced, not published. Checked kernel
  construction and helper-origin walks remain synchronous; this is not a
  global constant-cost step guarantee. Verification is in the priority plan.
- [x] Centralize lift-input freshness and parent checks at
  `pg_context_lift_request`. Its exact interned request reuses the established
  freshness result; the private builder and checked substitution adapter no
  longer repeat it. Keep proof ownership/sort checks, destination formation
  and substitution-pair validation. No new acceptance state or cache.
  On `function-graph-function-field.p`, GDB counts 1,589 -> 436 lift-related
  `pg_context_lookup` calls (request: 941 -> 436; builder: 436 -> 0;
  evidence adapter: 212 -> 0), with 11,993 Solve steps in both versions.
  The core regression alternates valid reuse with wrong-parent, occupied-binder
  and null-binder requests without adding lifts/proofs. Validation/publication
  status is recorded in the priority plan; no elapsed-time improvement claimed.
- [x] Reuse the established source length on an exact `pg_context_map` intern
  hit. New tuples still validate the Context length; image bounds, destination
  and classifier checks remain. Wrong counts and wrong-context images are
  rejected without adding maps. On the same function-field input, Context
  length walks drop from 4,021 to 2,657; Solve remains 11,993 steps. No new
  cache, proof rule or authority; elapsed-time improvement is not established.
- [x] Delete the unused application Context-pair API, getter and associated
  count/prefix branches. Retain address-conflict, nested-scope, inferred-arity
  and malformed-image coverage through the actual address interface.
- [x] Remove induction-branch proof-input traversal and origin attachment.
  Use checked field Contexts directly; unify ordinary/recursive field binding.
  Default Lambda/Pi allocation must not depend on those removed job identities.
  This does not close the remaining structural request-sharing task.
- [x] Move constructor-field transport preparation after its endpoint
  normalization dependencies. Previously each wakeup called classifier,
  Identity formation and reflexivity constructors before waiting again.
  No extra job, cache, result slot or proof rule is needed: validate input
  contexts/sorts, wait, then construct the ordinary checked transport once.
  The existing child request retains the completed transport work.
  `constructor_field_paths` now checks that the initial normalization-scheduling
  step creates no proofs or typed occurrences. It fails on the old order and
  passes after the change, for chunks 1/64. Debug `iadt_test` and
  `synthesis_test` pass in `/tmp/a-program-authority-transport`. Full optimized
  `check-acceptance` exits zero in `/tmp/a-program-authority-current-o2`,
  including 63/63 compatibility and retained QuickSort at chunks 1/64 (log:
  `/tmp/a-program-authority-current-o2.log`). This removes repeated constructor calls,
  not duplicated accepted proof records or all structural reconstruction.
- [x] Resume Pi application substitution without rescanning its immutable
  codomain. `pi_application_structure_step` now checks dependence only before
  requesting the argument, then advances the existing substitution directly.
  No new state/cache/job is introduced. GDB on the same `synthesis_test` counts
  2700 -> 676 independence checks, with 1861 -> 0 after substitution starts.
  Debug test output (including reported Solve steps) matches byte-for-byte;
  debug and ASan/UBSan `synthesis_test` and `iadt_test` pass. The full optimized
  and sanitizer gates above/below precede this last change; the current full
  debug `check-acceptance` exits zero in `/tmp/a-program-authority-binding-refs`
  (log `/tmp/a-program-authority-final-debug.log`), including compatibility,
  image boundaries and final retained QuickSort results. These are call counts,
  not elapsed speedup.

The full ASan/UBSan gate before the Pi rescan removal exited zero:
`/tmp/a-program-authority-current-sanitize.log`, build
`/tmp/a-program-authority-bindings-sanitize`, O0 with address/undefined sanitizers,
frame pointers and non-PIE. It includes APGSRC54/55 and the constructor scheduling
regression, 63/63 compatibility and final retained QuickSort results. The final
Pi change has focused sanitizer coverage in `/tmp/a-program-authority-pi-sanitize`.

Audit evidence (fresh `-O0 -g -Wall -Wextra -Werror` builds under
`/tmp/a-program-authority-a1`):

- `core_test` passes. `typed_substitution_test` checks shared classifier/input/
  construction-origin queries for distinct proofs of the exact same subject,
  preserves both premise DAGs, and rejects an incompatible reindex input.
  `typed_query_request` keys use typed subjects, contexts, operation kind and
  query arguments, not proof identity. Public requests validate ownership and
  accepted premises before sharing. No replacement cache is warranted.
- `identity_test` passes. `uniform_transport` supplies two formation proofs
  with distinct premises and the same typed subject, then verifies lookup adds
  no proof/occurrence and preserves both derivations. Formation constructors
  record ordered endpoints/paths in `operands` and ordered substitutions in
  `pg_occurrence_maps`; those pointers participate in interning.
  `pg_identity_boundary_view` returns the corresponding accepted premises.
  Thus selecting a proof of this exact subject does not choose new endpoint
  or map semantics. Keep this lookup; do not introduce a canonical proof.
- `computation_view` now stops after origin recovery if the resulting typed
  subject is the same one whose structural view already failed. Its input
  queries have immutable subjects and terminal cached results, so a different
  receipt cannot make that repeated view succeed. A different recovered
  subject still gets inspected; Match/induction origin handling is unchanged.
  No new job, cache, tag or public API was added. GDB on the existing
  `function-graph-callable-parameter.p main expected` test observed 14
  same-subject retries in R76 and 14 skips / zero retries after this change
  (combined chunks 1 and 64). This is not a measured overall speedup.
- All 97 `program_test` commands mentioning `function-graph` in the pointer
  Makefile passed on both R76 and the fresh debug build
  `/tmp/a-program-authority-a4/program_test`: 194 successful invocations,
  identical result/status output and Solve step counts. Existing acceptance
  tests cover captured callable parameters, exposed Match, indexed families,
  function fields, rejected witnesses and unsupported cases. This is not the
  full project acceptance gate.

These checks do not cover the source-scope identity defect or close A4's
allocation task. The full acceptance suite remains open.

Additional A4 change: completed multi-clause Handler allocation lookup reads
the return Lambda through `pg_occurrence.operands[1]`, which `pg_prove_handler`
already records, instead of reading `PG_HANDLER_ELIM`'s proof premise. Imported
unaccepted origins still use their existing rule-input description; that path
has not been replaced with an unchecked claim or new allocation authority.

The allocation audit distinguishes the following existing owners:

| Allocation | Description available before acceptance | Accepted description |
|---|---|---|
| Lambda/Pi binding | Source syntax and relocated binder; no Context theorem in a source BINDING record | Independently synthesized Context |
| Sequencing/application | Immutable syntax/prefix/slot binder address | Same binder, with types synthesized independently |
| Constructor use | Prefix and field Context references; no imported substitution theorem | Independently checked field Context |
| Recursive Match | `pg_induction_allocation` supplied as an input | The same layout attached to the typed occurrence |
| Handler/Fold | Source clause/binder-prefix/slot address; explicit clause allocation tuple | Typed Handler operands and independently checked clause Contexts |
| Nominal declaration | Explicit nominal input | Declaration referenced by the schema |

`allocation_context_binder` has been deleted. Handler/Fold symbols and
qualified constructor field allocations no longer require an imported proof.
Their raw addresses/Context references are sufficient before Solve; their
types are checked independently. Recursive Match and schema-origin recovery
remain open; this does not close the overall allocation audit.

Fresh debug `source_io_test` in `/tmp/a-program-authority-allocation` passes all
12 focused selectors, including 4446 Handler boundary snapshots. These changes
do not remove A3's missing lexical association or make the retained `index-alias`
and `function-field` fixtures pass. Thus the overall allocation task stays open.
Constructor inputs, Fold origins, Handler origins/nesting and all Handler
boundary snapshots also pass ASan/UBSan in
`/tmp/a-program-authority-allocation-sanitize`.

Application transport follow-up (`/tmp/a-program-authority-application`):

- `application-origins` verifies three destructive/inert resaves, no Solve on
  lookup/write/read, and exact retained Core after ordinary one-step Solve.
  `application_allocations` additionally checks borrowed inputs, ignored old
  annotations, and rejection of a different prefix binder. These are allocation
  checks, not Context type equality or evidence admission.
- All 13 focused source selectors and `synthesis_test` pass. The complete
  `source_io.sh` still fails only the retained `index-alias` and `function-field`
  cases in both root modes; append and the other retained fixtures pass.
- For independently written append images before/after this change, GDB at
  `pg_program_destroy` after `retained-check` reports Solve steps 6310 -> 6092,
  jobs 1849 -> 1799, accepted proofs 911 -> 911. Files shrink 79004 -> 75364
  bytes. The baseline is `/tmp/a-program-authority-allocation/source_io_test`.
  This measures eliminated application-allocation reconstruction only, not
  general performance or completion of A1-A3.
- APGSRC46/47 is now superseded by APGSRC48/49; old images are rejected rather
  than routed through a compatibility checker. Header comments and the seed
  format assertion change together. Historical reproduction commands above
  refer to their original executables/formats.
- Constructor, operation and application allocations share one transient
  transport index and one context-pair array. There is no application-specific
  persistent table or additional work registry. The source syntax identifies
  which ordinary checking factory receives the allocation.
- `synthesis_test` and seven source selectors (application, constructor, Match,
  Fold, Handler origins/nesting/boundaries) pass ASan/UBSan in
  `/tmp/a-program-authority-application-sanitize`. The seed codec test and
  `git diff --check` pass. Full optimized/project verification and net-negative
  implementation LOC remain open; no commit/Main push is claimed.

Fresh debug verification in `/tmp/a-program-authority-a2` passes all 12 focused
source selectors from `context-scopes` through `prepared-module`. The Handler
boundary test now asserts that the saved allocation equals the typed return
Lambda's binder after every reload/Solve and on the original completed source.
Its four cases cover 1335, 1436, 764 and 911 snapshots, including rejection and
forwarded operations. The 97 function-graph commands also pass with output and
Solve step counts identical to the recorded R76 baseline.

Fresh `retained-write/check/recompute` passes for append. Function-field still
fails both checks with the same exact-binder mismatch (`alpha_equal=1`). This
is a remaining acceptance failure, not an allowed equivalence relaxation.

The same changes also pass `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer` checks in `/tmp/a-program-authority-a2-sanitize`:
`context-scopes`, `constructor-inputs`, `match-origins`, `fold-origins`,
`handler-origins`, `handler-nesting`, `normalization`, and all 4446 Handler
boundary snapshots. No sanitizer diagnostic was reported. This focused run
does not replace the full sanitizer acceptance gate.

`SOURCE_ORIGIN_JOB` has subsequently been removed. Its existing exports,
Match-source and function-graph-source links now occupy `source_metadata`;
the packet-source pointer was only a Boolean test and is now a flag. The index
contains no classifier, proof, pending state or acceptance decision. Its entries
are registered only by the same completed source constructions as before.
The source image format is unchanged. This does not repair A1-A3.

On this machine, GDB reports `sizeof(pg_synthesis_job)` changing from 464 to
448 bytes; metadata formerly used that job plus two input pointers and now
uses a 56-byte record. In `function-graph-callable-parameter.p main expected`,
both chunks 1/64 use 4487 jobs and 24 metadata records, instead of 4515 jobs.
Four old empty lookup registrations disappear. Solve remains 15500 steps.
These are structure sizes/counts, not a total resident-memory or timing claim.

Fresh debug builds in `/tmp/a-program-authority-metadata` pass `synthesis_test`,
all 12 focused source selectors, and all 228 Makefile `program_test` commands.
The 97 function-graph outputs match the recorded R76 baseline; the other 131
were run both before/after the metadata change and match exactly, including
step counts. New declaration assertions check repeated nominal registration
shares metadata, distinct declarations stay distinct, and unsuccessful source
name lookup for a kernel-only declaration creates no metadata.
The metadata version also passes `synthesis_test` and all 12 focused source
selectors under ASan/UBSan in `/tmp/a-program-authority-metadata-sanitize`.

### A5. Acceptance and publication

#### 2026-09-19: Role-exclusive work storage

The priority plan records private typed-query and synthesis-work compaction
against Main `c0f27b3`. Existing immutable kinds select union storage; accepted
results, typed occurrences and wire data do not change. Full debug, optimized
and ASan/UBSan acceptance exit 0; all 2460 export records and steps agree with
the baseline. Fixed-source graph/work counts are unchanged. QuickSort retained
arena usage decreases by 1507264 bytes. Publication gates and flags are tracked
in that entry. This does not resolve A3's lexical-candidate bound,
A4's remaining synchronous consumers or either cumulative LOC gate.

#### 2026-09-19: Published `5fa8a55` performance audit

R0 `4657cc6` versus published `5fa8a55`, both strict C11/O0/g. Inputs are
unchanged between revisions. Twelve alternating fresh-process pairs per case,
discarding the first two; medians below include process startup. Each version
saves its own zero-step image (exit 3), then resumes in a fresh process with
1,000,000 steps. All 18 source/image cases finish successfully in every run.
No concurrent build/test ran during timing. These are debug measurements,
not optimized-performance claims or final retained/recompute acceptance.

| Input | Source seconds R0/current | Zero-step image seconds R0/current |
|---|---:|---:|
| 01_bool | .000971 / .000995 | n/a |
| 02_nat | .000953 / .001007 | n/a |
| 03_main | .001037 / .001138 | n/a |
| 04_match | .001712 / .001919 | n/a |
| 05_bool_to_nat | .001507 / .001649 | n/a |
| 06_pred | .001440 / .001541 | n/a |
| 07_add | .002069 / .002615 | n/a |
| 09_list_induction | .002883 / .003422 | n/a |
| Vec-append | .013675 / .015894 | .015331 / .016001 |
| dependent-Sigma | .003145 / .003577 | .003671 / .004113 |
| generated-length | .009069 / .012415 | .009525 / .012504 |
| function-field | .014785 / .020587 | .014232 / .020628 |
| QuickSort-property | .854441 / .326392 | .870301 / .323726 |

QuickSort source peak RSS medians: 225404 -> 88810 KiB; source graph arena
used bytes: 135989888 -> 68837792. Small-case RSS is dominated by inherited
measurement-parent high-water marks and is not useful allocation evidence.
Separate GDB source counts expose regressions despite fewer Solve steps:

| Input | Occurrences R0/current | Proofs R0/current | Graph arena used bytes R0/current |
|---|---:|---:|---:|
| length | 2249 / 3540 | 3856 / 4864 | 2898368 / 4052864 |
| function-field | 4524 / 6687 | 7927 / 9284 | 4167744 / 6492096 |

Length source steps decrease 10950 -> 9104; function-field 12906 -> 12334.
This does not establish reduced total work. Aggregated gprof call counts from
100 separate length solves show substitution requests 3011 -> 4807 per solve,
readback requests 3423 -> 6198, and index insertions 20545 -> 34502.
Sampling ticks are too sparse for reliable per-function time percentages.

Rejected shortcut: bypassing substitution validation whenever the same map
has already been checked. GDB observes 1119 map-check entries and 2031 suffix
classifier checks; only two entries/checks repeat a map that already passed
this validator. This does not justify another acceptance cache. Next trace
which producers create the additional distinct scoped subjects/maps, and
whether their transformations can reuse existing typed structure. Preserve
alternative premise DAGs, binder identity and dependent classifier checks.
Likewise, do not delete source annotations just because accepted classifiers
often encode the same domain: unaccepted inputs retain explicit annotations.

Evidence: `/tmp/a-program-authority-r0-current-{matrix,counts}.log`,
`/tmp/a-program-authority-r0-current-function-field{,-counts}.log`,
`/tmp/a-program-length-prof-{r0,current}-20260919.txt`, and
`/tmp/a-program-authority-map-recheck-length.log`. GDB inferiors exit normally.
Both profiler binaries were freshly compiled with `-pg`; profiler timings
are not included in the comparison table. This audit changes no implementation
or tests and does not close A3-A5, the final sanitizer gate or LOC reduction.

Shared substitution ownership follow-up (2026-09-19): the root now belongs to
the request's input arena from creation through completion. The second retained
root and result-copy path are removed; scratch readback edges remain disposable.
Full debug, optimized and ASan/UBSan acceptance and evaluation-image tests pass;
all 2,460 export records/step counts agree. The priority plan records allocation
measurements, timing without an established speedup, per-file deltas and grouped
publication. This does not close A3-A5 or the parent's reduction gate.

Local follow-up on `e6029f0`: the shared substitution's state still had separate
heap ownership even though the request/root/environment share one lifetime.
Move it into the existing input arena, using the existing owner pointer for
cleanup; keep standalone/restored states individually owned. This removes
28,606 individual state allocations on imported QuickSort without changing
request, proof, occurrence or Solve-step counts. The priority plan tracks final
tests and timing before a larger publication epoch; original gates stay open.

#### 2026-09-19 diagnostic comparison (not final acceptance)

Compare retained R0 `4657cc6` and implementation `555b13d`, strict C11/O0/g,
five alternating-order fresh processes per input/version. Timings are medians
in seconds, measured with `perf_counter`; RSS uses per-child `wait4`. Use the
parent R24/R73 commands/fixtures, plus `function-graph-function-field.p`.
Each image is saved at zero steps by its own version and loaded in a fresh
process. Source/load limits are 1,000,000 steps; every measured solve exits 0.
Raw samples: `/tmp/a-program-authority-specialized-view-matrix.log`.

| Input | Source seconds R0/current | Zero-work image seconds R0/current | Source steps R0/current |
|---|---|---|---|
| 01_bool | .001505 / .001248 | n/a | 520 / 444 |
| 02_nat | .001058 / .001030 | n/a | 307 / 290 |
| 03_main | .001224 / .001401 | n/a | 520 / 444 |
| 04_match | .001747 / .001979 | n/a | 1302 / 1173 |
| 05_bool_to_nat | .001650 / .001936 | n/a | 946 / 852 |
| 06_pred | .001397 / .001627 | n/a | 811 / 752 |
| 07_add | .002300 / .002583 | n/a | 1844 / 1697 |
| 09_list_induction | .003230 / .004035 | n/a | 3536 / 3121 |
| Vec-append | .014534 / .016437 | .014167 / .017544 | 22765 / 19935 |
| dependent-Sigma | .003282 / .004029 | .003289 / .004883 | 4048 / 3703 |
| generated-length | .009311 / .012194 | .008991 / .013635 | 10950 / 9569 |
| function-field | .012120 / .021305 | .013106 / .022298 | 12906 / 11993 |
| QuickSort-property | .859307 / .325888 | .847670 / .337093 | 149501 / 126685 |

Image steps in table order: 23074/20240, 4357/4008, 11259/9874,
13215/12298, 149810/126990. QuickSort RSS ranges are 224968-225716 /
89572-90732 KiB (source), 225428-226292 / 90324-90940 KiB (image).
Small-case RSS is dominated by the measurement parent's inherited high-water
mark (12040-12404 KiB), so it cannot establish equal allocation costs.

Separate GDB counts at `pg_program_destroy`, source inputs above:

| Input | Core terms R0/current | Typed subjects R0/current | Proofs R0/current |
|---|---|---|---|
| Vec-append | 6301 / 10618 | 2737 / 2803 | 6505 / 3843 |
| dependent-Sigma | 614 / 897 | 475 / 657 | 741 / 908 |
| generated-length | 1829 / 2031 | 2249 / 3574 | 3856 / 4941 |
| function-field | 4472 / 5143 | 4524 / 6717 | 7927 / 9371 |
| QuickSort-property | 168628 / 135287 | 435774 / 82230 | 588033 / 93439 |

QuickSort's earlier time/memory regression is absent on this input; that does
not waive the smaller-case regressions. In this ABI, occurrence headers grew
32 -> 112 bytes, evidence headers shrank 72 -> 64, and source-job headers
488 -> 456. These sizes/counts locate representation-cost review, not proof
that any required typed distinction can be deleted. Next inspect first-time
typed input/context actions in length and function-field, alongside A3's
remaining same-syntax scope enumeration. Do not add another specialization
cache: the preceding A4 regression already demonstrates reuse.

#### Outstanding acceptance gates

The priority plan's source-transport/query-resumption epoch records fresh full
debug/O2/ASan+UBSan gates after `fcd9d25`, plus cancellation at each helper step
and successful shared-query continuation after owner destruction. These gates
all pass; they authorize that grouped epoch, not final A0-A5 completion. The
remaining lexical bound, consumer audit, performance regressions and overall
code-reduction requirement below are unchanged.

Earlier checkpoint: implementation `dd9cabc` (2026-09-19). Full strict-debug,
optimized and ASan/UBSan `check-acceptance` have now all exited 0, including
63/63 source compatibility. Debug/sanitizer logs are
`/tmp/a-program-authority-dd9cabc-{debug,asan}.log`; optimized log is
`/tmp/a-program-authority-producer-view-opt.log`. The full sanitizer invocation
used the preceding O1 sanitizer flags with default sanitizer runtime options;
its log contains no sanitizer/runtime-error diagnostic. This updates the
verification evidence, not final A3-A5 completion or publication readiness.

Fresh R0/current measurements use the same strict O0/g flags, five alternating
fresh processes, 1,000,000 steps and each version's zero-step source image.
No other acceptance build/run was active during timing. R0's archived compiler
sources and Makefile were hash-checked against `4657cc6` before building.
Raw samples: `/tmp/a-program-authority-dd9cabc-matrix.log`.

| Input | Source seconds R0/dd9cabc | Image seconds R0/dd9cabc | Source steps R0/dd9cabc |
|---|---|---|---|
| Vec-append | .016832 / .015360 | .013012 / .015754 | 22765 / 19935 |
| generated-length | .009076 / .012957 | .009770 / .013249 | 10950 / 9694 |
| function-field | .013694 / .018438 | .014927 / .020247 | 12906 / 12809 |
| QuickSort-property | .890519 / .326503 | .878080 / .342570 | 149501 / 146809 |

Image steps are respectively 23074/20240, 11259/9999, 13215/13114 and
149810/147114. All 80 timed solves exited 0. Source QuickSort peak RSS ranges
are 224972-225336 / 90192-91096 KiB; image ranges are 225232-226080 /
90480-90948 KiB. Small-input RSS remains dominated by inherited parent
high-water marks and is not useful allocation evidence. Separate GDB counts
confirm the earlier table's Core/occurrence/proof counts for all four inputs
are unchanged at this checkpoint. Length/function-field still regress against
R0 despite fewer Solve steps; QuickSort improvement does not waive them.
These are diagnostic measurements, not performance thresholds or O2 claims.

Context-map audit: `evidence.c:map_lift_prefix` cannot simply be skipped for
already checked destinations. Imported descriptors need the same structural
lift admission without producer history; supplied alternative prefix proofs
and wrong-classifier rejection must remain intact. On function-field, 566
distinct maps cause 793 callbacks: 222 unchecked-destination initial calls
and 222 resumptions, plus 344 checked-destination initial calls and five
resumptions. Revisit counts alone do not establish duplicate checking.
No new cache or removed validation follows from this measurement. Log:
`/tmp/a-program-authority-map-validation-profile.log`.

- [ ] Run the regression matrix below and the parent's full debug, optimized
  and ASan/UBSan acceptance gates without ignoring failures.
- [ ] Compare baseline/final timing, work and allocations on append, function-
  field, length and imported QuickSort; distinguish cache reuse from recompute.
- [ ] Report per-file additions/deletions/net for implementation, tests and docs
  separately, both from R76 and cumulative from `4657cc6`.
- [ ] Close the parent's R2-R5 only after their original criteria hold, including
  net-negative implementation/header LOC. R76 was still net +2416; document
  concrete deletions, not an unsupported estimate or compressed formatting.
- [ ] Commit and publish each completed epoch to Main after its acceptance gates,
  following the 2026-09-18 publication policy. Record remaining work; do not
  describe an epoch as the completed overall repair.

Current implementation delta from R76 (not a completed cleanup):

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| derivation.c | 1 | 1 | 0 |
| evidence.c | 39 | 34 | +5 |
| evidence.h | 22 | 1 | +21 |
| function_graph.c | 7 | 3 | +4 |
| source_io.c | 261 | 61 | +200 |
| source_io.h | 19 | 8 | +11 |
| synthesis.c | 503 | 449 | +54 |
| synthesis.h | 39 | 23 | +16 |
| Total implementation/headers | 891 | 580 | +311 |

Tests separately: core.c +10/-0; iadt.c +15/-3; seed.c +1/-1;
source_io.c +731/-116; source_io.sh +2/-1;
synthesis.c +115/-22. The withdrawn occurrence-codec changes are absent from
the diff. Including the issue #29 corrections described below, cumulative
implementation/headers versus `4657cc6` are +5883/-3156,
net **+2727**, so the parent's net-negative gate is explicitly **not met**.
This is evidence for remaining design cleanup, not a reason to drop checks or
declare overall completion. A coherent completed epoch may be published under
the revised policy without meeting the whole-refactor LOC target yet.

## 6. Required Regression Matrix

Extend `tests/source_io.c`, `tests/core.c`, `tests/iadt.c` and their existing
runners; do not add another audit harness.

| Case | Required observation |
|---|---|
| Function-field IADT, source-only / source plus typed root | Independently synthesized source retains exact relocated Core/binders |
| Single-constructor indexed alias, nested application | Same allocation guarantee without recursive fields or IH; used aliases cannot be pruned to evade the failure |
| Retained-check / recompute, two inert resaves | Same source identity in all modes; no acceptance or effects during load |
| Partial producer, chunks 1 / 64, reversed request order | Progress resumes without duplicate allocation or order-dependent meaning |
| Alternate proofs, same subject | Shared structural target, retained valid alternative derivations |
| Same source name / different lexical owner or substitution | No accidental collapse or capture |
| Incorrect typed target, map or allocation | Rejected by normal checks; no suppression by an earlier valid producer |
| Bool/Nat identity uses sharing erased Core | Distinct typed occurrences remain distinct |
| Append, captured family parameters, imported QuickSort | Previously passing allocation restoration stays passing |
| Identity action and function-graph properties | Preserve witness Terms, boundary direction and ordinary proof checks |

Exact saved source identity and equality of separately normalized outputs are
different assertions. Keep the former exact; do not change it to alpha equality
to hide allocation loss. Do not impose exact identity on independently fresh
programs or fresh reduction allocations where that was never the contract.

## 7. Progress and Non-Decisions

- [x] Inspect existing semantic owners and interrupted patch.
- [x] Reproduce the R76 source-root failure/control with its existing binary.
- [x] Withdraw the alias-specific design and update the parent execution order.
- [ ] Complete A0-A5. The partial changes above do not complete the repair.

No new refinement type, Eq rule, Core tag, trusted Replay, or value/computation
Core split is authorized. This plan does not claim all source metadata can be
deleted, every branch is redundant, or all proof lookups are unsound. The goal
is fewer competing reconstruction paths, not fewer logical checks.

## 8. September 18 Audit and Issue 29

The [Issue 29 report](2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md)
records fixes for graph export collisions and multi-index checked transport,
permanent positive/negative artifact regressions, and the remaining transitivity
and Sorted-proof limitations. Optimized full acceptance passes with these fixes;
that does not complete A5's other gates or justify closing the whole issue.

The [incremental Solver audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md)
refines the remaining work after the priority feature phase: finish declaration raw allocation, then remove
job-based pending-structure reconstruction using shared descriptive construction.
Measure repeated substitution assembly and synchronous work before adding caches
or more worker types. These are refinements of A0-A5, not a parallel architecture.
Deleting unrelated checks, merging typed uses by erased Core, or adding another
authority layer does not count as simplification.

`@f` and `*f` already share graph work but return different mathematical objects.
A named witness member could reduce surface spellings; this remains a proposal.
IH `*k` is unchanged. Tests/build/doc additions for #29 are separate from the
implementation/header totals above and must remain separate in the final report.

### Declaration allocation epoch

The linked Solver audit now records completion of target 2: source declaration
allocation uses the nominal descriptor directly, not a saved formation-proof
producer. Fresh field evidence re-establishes the retained Context through
existing checked rules. Exact schema validation is unchanged. Full optimized
acceptance and affected sanitizer suites pass; source images advance to 62/63.
This completes that dependency removal, not A0-A5 as a whole. Pending structural
reconstruction and the net-negative implementation gate remain open. The audit
includes per-file changes, negative tests and same-input work measurements.
