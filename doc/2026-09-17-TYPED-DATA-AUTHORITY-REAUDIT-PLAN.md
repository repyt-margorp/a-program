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
that symbolic structure to remain stable even after the accepted type has a
solved effect row. Replacing a structural query with the accepted result only
when its producer finishes would make its meaning scheduling-dependent.
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

`source_alias_targets` in `tests/source_io.c` now checks two maps with identical
source/destination but different images, alternate map-image proofs, direct
versus unaccepted producers resolving to the same exact occurrence, and an
invalid producer alongside valid aliases. Eight scenarios cover chunks 1/64,
reversed producer/use request order, pending versus independently completed
producers, and nested same-name shadowing without changing the outer binding.
They pass in `context-scopes` with a fresh debug build in
`/tmp/a-program-authority-a1`. They do not assert that producer-keyed scopes
are already unified. The end-to-end source image regression still fails.

- [ ] Extend existing source image tests with a minimal index-alias case. Observe
  original binder, map, image occurrence, producer, scope and application job
  before save and after load/Solve. Compare relocated identities inside each
  process, never numerical addresses across processes.
- [ ] For each differing reference, distinguish missing serialization from
  reconstruction keyed by a checker. Include the hidden Fold binder allocation.
- [ ] Specify concrete scope/name/request keys for resolved and pending input;
  verify source shadowing, distinct maps and nominal declarations remain distinct.
- [ ] Record which existing field/edge supplies each key. Any proposed extra
  field needs a counterexample showing why existing data cannot supply it.
  No implementation of a new persistent schema before this gate closes.

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

- [ ] Change `generalized_scope`, normal name creation/lookup and the relevant
  source request key together, according to A1. Use the existing typed target
  and map; eliminate generated aliases' dependence on receipt/job identity.
- [ ] Reuse existing context allocation references when preparing applications;
  no fresh binder merely because a different checker established the same input.
- [ ] Retain failed and pending proof obligations independently. Neither an
  earlier successful producer nor a cached structural result may suppress them.
- [ ] Cover both producer completion orders, alternate proofs of the same
  target, distinct targets, nested shadowing and two maps into one context.
- [ ] Delete superseded alias reconstruction/key exceptions in this same change.

### A3. Transport the same structure

- [ ] Update `source_io.c` environment/producer/origin handling using A1's
  reference contract. Loaded inputs and fresh source reference the same relocated
  lexical allocations; source rechecking remains necessary.
- [ ] Preserve selected-root reachability without scanning unrelated source
  scopes or collecting all syntax-free aliases. Do not repeat the withdrawn
  broad `collect_origin` experiment.
- [ ] Read and resave zero/partial/completed inputs without executing requests
  or promoting saved results into acceptance. Reuse context/occurrence payloads
  where appropriate; do not duplicate their maps in a new alias wire record.
- [x] Evaluate and remove the unused occurrence-codec split. Its experimental
  shared-section tests passed, but source allocation transport needs only
  existing binder references, not an additional typed-graph section. The
  uncommitted split and its API-only tests were withdrawn together; existing
  occurrence transport tests remain intact.
- [x] If the format must change, update magic, headers and seed tests together.
  Explain the actual newly transported reference, not just the version number.
- [ ] Remove obsolete proof-shape allocation recovery only after its pending
  cases have descriptive replacements. A source-only image must still work.
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
- [ ] For `pg_synthesis_allocation_object` and restoration helpers, classify
  each field as allocation description, checking input or acceptance. Read
  available binders/context/induction layout from structural owners, not nested
  theorem shapes. Keep constructor/handler/IADT checks explicit.
  On 2026-09-18, A3 removed the Match origin wait using descriptive transport.
  Source declaration restoration still waits for a formation theorem before
  reading its schema fields. Remove that wait only with descriptive transport
  and ordinary rechecking, retaining independently selected proof obligations.
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
