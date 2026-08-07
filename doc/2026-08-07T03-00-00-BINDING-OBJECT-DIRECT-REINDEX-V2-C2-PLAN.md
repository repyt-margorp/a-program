# Binding-Object Direct Reindex V2-C2 Implementation Plan

Date: 2026-08-07

Status: complete

Parent plan:

- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Predecessors:

- `doc/2026-08-07T00-00-00-CONTEXT-SUBSTITUTION-V2-C1-IMPLEMENTATION-PLAN.md`
- `doc/2026-08-07T01-00-00-SHARED-TERM-HOTT-DCBPV-V2-T1-T2-PLAN.md`
- `doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`

Baseline:

- branch: `main`;
- commit: `268f2e6b45725e6424681b90baddb8e152f0612b`;
- short commit: `268f2e6`;
- artifact format: `A_PROGRAM_ARTIFACT 62`;
- prototype test scripts: 14.

## 1. Purpose

V2-C1 extracted ContextDB, SubstitutionDB, and ordinary reindexing from AST
lowering. It fixed semantic ownership, but intentionally preserved the old
reindexing algorithm. Before V2-C2, `prototype_term_reindex` implemented a
simultaneous context substitution by repeatedly invoking a single-binding
substitution primitive through temporary fresh binders.

V2-C2 replaced that implementation with one direct graph traversal driven by
an immutable map from graph binding handles to replacement TermDB nodes.

The refactor is required before typed HOTT constraints are added. Higher
observational endpoints and witnesses must reindex through a substrate whose
scope semantics are structural, explicit, and independent of source spelling.
They must not inherit a temporary alpha-renaming protocol as an accidental
part of their implementation contract.

## 2. Governing Design Philosophy

### 2.1 Computation is not identified by source names

A Program lowers human-readable names into graph references. After lowering,
the spelling `x` is not the identity of a variable occurrence. A variable
occurrence identifies the binding object introduced by a Lambda, Pi, Match
case, effect-row binder, or another binding construct.

Conceptually:

```text
LAMBDA
  binding -------------------+
  body                       |
    VAR ---------------------+
```

On a pointer-based in-memory implementation, the `VAR` edge could point to the
binding object directly. TermDB is an indexed arena, so a `uint32_t` handle is
the stable pointer representation. The integer is not a variable name and must
not acquire source-name semantics.

### 2.2 No De Bruijn representation

V2-C2 explicitly rejects De Bruijn indices and levels as the stored TermDB
representation. Binding is already representable by a direct graph reference;
encoding the same relation as a relative integer position would make graph
movement and sharing depend on lexical depth without serving A Program's
design.

### 2.3 One shared TermDB remains authoritative

This refactor does not create separate value and computation graph syntaxes.
Pi, Lambda, APP, Match, Comp, Return, Thunk, and Force continue to use the one
shared TermDB fixed by V2-T1/T2. Value/computation interpretation remains typed
occurrence information and explicit CBPV boundary structure.

### 2.4 Reindexing remains a real CwF operation

Name independence does not eliminate reindexing. For

```text
sigma : Delta -> Gamma
Gamma |- t : A
```

the compiler still needs:

```text
Delta |- t[sigma] : A[sigma]
```

What V2-C2 removes is the implementation of this action as repeated temporary
renaming. Reindexing becomes direct replacement of graph edges that refer to
the target context's binding objects.

### 2.5 TermDB may contain semantic results, not staging debris

Static reindexing may intern the final graph `t[sigma]` in TermDB. This is
consistent with TermDB being a computation graph rather than runtime machine
state. The implementation should not intern avoidable intermediate graphs such
as `t[x := fresh_1][y := fresh_2]` that exist only to simulate simultaneous
substitution.

## 3. Normative Terminology

The following distinctions are mandatory:

| Term | Meaning | Semantic lifetime |
| --- | --- | --- |
| source symbol | human-readable spelling such as `x` | parser, diagnostics, Name layer |
| AST binder identity | parser identity used to resolve source occurrences | AST lowering only |
| binding handle | opaque arena reference identifying one graph binding object | TermDB and ContextDB |
| context entry | CwF extension carrying a binding handle and classifier | ContextDB |
| substitution entry | mapping from a target binding handle to a source-context term | SubstitutionDB/reindex view |
| IH scope handle | hidden binding-like owner connecting one recursive Match to its IH references | TermDB scope structure |

The baseline C field name `binder_id` denoted the third item, not a source name.
V2-C2 made this invariant explicit and performed a no-alias naming migration
to `binding_id` where the field has graph-binding semantics.

The migration must not retain compatibility wrappers with the old names.
Source-level `ast_binder_id` remains a separate concept until a later AST-only
naming cleanup is approved.

## 4. Current Implementation Audit

### 4.1 Baseline strengths

The baseline graph was already substantially name-independent:

- `PROTOTYPE_TERM_VAR` stored a graph binding handle, not a source symbol;
- Lambda and other binders store the corresponding graph handle;
- local shape comparison pairs binders lexically rather than requiring numeric
  ID equality;
- `add_term` alpha-interns terms through `term_intern_alpha_equal_local`;
- canonical hashing maps bound handles to local canonical slots;
- source symbols and graph binders are mapped separately during AST lowering.

Therefore V2-C2 is not a conversion from named syntax to a graph for the first
time. It removes a named-substitution-shaped algorithm from an otherwise
graph-oriented substrate.

### 4.2 Baseline reindex algorithm

At baseline commit `268f2e6`, `prototype_term_reindex` in
`src/prototype/context.c`:

1. walks the target context into a fixed array of at most 512 entries;
2. allocates one fresh binder for every target context binding;
3. repeatedly calls `prototype_term_graph_substitute_bound_var` to replace each
   target binding by its fresh variable;
4. resolves the actual substitution image of every target binding;
5. repeatedly calls the same single-binding substitution primitive to replace
   each fresh variable by its image.

The first pass prevents accidental cascading. For example, simultaneous
`[x := y, y := z]` must not turn the image of `x` into `z`. The protection is
semantically necessary, but fresh renaming is not.

### 4.3 Baseline IH frame handling

`substitute_term_internal` in `src/prototype/term.c` maintains a private
`substitution_frame_scope` list. When a changed Match is rebuilt, it allocates a
new frame, records `old_frame_id -> new_frame_id`, and rewrites each enclosed
`INDUCTION_HYPOTHESIS` reference.

The ownership preservation is necessary. The standalone frame-remap protocol
is an implementation symptom of rebuilding a binder-bearing graph through a
single-binding substitution API.

V2-C2 treats an IH frame as a hidden binding object owned by Match. Rebuilding
the owner clones this scope object and redirects its internal reference edges
in the same traversal. It does not evaluate the IH, solve a motive, or add a
runtime frame.

### 4.4 Baseline concrete debt

The baseline implementation had the following defects or scaling risks:

1. reindexing traverses the graph up to twice per target binding;
2. temporary fresh binding handles are consumed even though they have no final
   semantic role;
3. avoidable intermediate TermDB nodes may be interned;
4. the 512-entry target-context limit is an algorithmic artifact;
5. substitution and IH scope cloning use separate ad hoc environments;
6. the single-binding traversal has no general `(term, substitution)` memo;
7. alpha interning performs a linear scan over existing TermDB nodes, so
   avoidable intermediate construction multiplies an already expensive path;
8. the API vocabulary makes opaque graph handles look like textual names.

## 5. Target Graph Model

### 5.1 Binding handles as arena pointers

V2-C2 does not require a new persisted BindingDB merely to wrap an integer.
TermDB's binding allocator remains the owner of a unique handle namespace.
The handle is treated as an arena pointer with the following invariants:

1. source spelling is not part of the handle;
2. a bound occurrence refers directly to its introducing binding handle;
3. two alpha-equivalent graphs may use different handles and still intern to
   one local TermDB node;
4. free graph bindings compare by handle identity;
5. artifact relocation may offset/remap handles but may not resolve them by
   source spelling.

Bound handles and free handles must not be conflated. Alpha interning may
choose an existing representative for a closed binder-bearing graph, in which
case the representative's internal binding handle wins. A free occurrence in
a ContextDB entry must continue to use its actual context binding handle and
must never be alpha-interned merely because another free handle has the same
source spelling. V2-C2 must characterize this boundary before changing the
rebuild algorithm.

A separate binding-record arena should be introduced only if a later feature
requires persistent owner/kind metadata that cannot be derived from scoped
graph traversal. V2-C2 must not add such a database speculatively.

### 5.2 Direct substitution view

Before traversing a term, ContextDB/SubstitutionDB must expose or privately
build an immutable view equivalent to:

```text
target binding handle -> source-context TermDB node
```

Composition is resolved structurally from SubstitutionDB. The resulting map is
simultaneous: replacement terms are final images in the source context and are
not recursively rewritten merely because they mention another numeric handle
that also occurs in the map.

The view must retain the source and target context IDs for validation and cache
identity. It is compiler-local in V2-C2 and is not an artifact record.

### 5.3 One-pass graph action

The internal operation has the conceptual signature:

```text
reindex_graph(term, substitution_view, lexical_scope, memo) -> term
```

Rules include:

```text
VAR(binding):
  if binding is locally shadowed, retain it
  else if substitution_view contains binding, return its mapped term
  else retain the original node

APP(f, a):
  intern APP(reindex(f), reindex(a))

LAMBDA(binding, body):
  enter binding in lexical_scope
  intern LAMBDA(binding, reindex(body))

PI(domain, family):
  reindex domain
  enter the family's binding while reindexing its body

MATCH(scrutinee, cases, ih_scope):
  reindex scrutinee and constructor owners
  enter every case binding while reindexing its case body
  clone ih_scope only if the Match graph changes
  redirect enclosed IH references to the cloned scope
```

All other binder-bearing tags must be inventoried and handled explicitly. At
the baseline this includes at least Lambda, Pi/codomain families, Match case
binders, `EFFECT_ROW_FORALL`, computation-fold clause binders, and recursive IH
scope.

### 5.4 Memoization

The traversal must memoize results. A safe initial key is:

```text
(term_id, substitution_view_id, lexical_scope_fingerprint,
 ih_scope_clone_environment)
```

The implementation may use a traversal-local memo before introducing a
persistent TermDB cache. It must not key only by `term_id`, because the same
shared node can occur under different local binding scopes or IH owners.

Persistent memoization is optional for V2-C2. Correct one-pass traversal and
the elimination of repeated fresh substitution are mandatory.

### 5.5 Scope cloning, not name remapping

When a binder-bearing owner is reused unchanged, its binding edge remains
unchanged. When an owner must be cloned and its binding identity is owner-local,
the graph builder allocates a new binding handle and records the correspondence
only for the duration of that clone.

This is graph cloning:

```text
old owner.binding -> new owner.binding
old internal reference -> new owner.binding
```

It must not be exposed as a general name-remapping facility. The current IH
frame behavior is retained semantically under this model, but its implementation
is folded into a generic scoped graph-rebuild environment.

## 6. Ownership and API Plan

### 6.1 Public API

Keep the public semantic entry point:

```c
int prototype_term_reindex(...);
```

Callers in `typing.c`, `ast.c`, context telescope operations, and tests should
not know whether the implementation uses a flattened view or a composed
SubstitutionDB tree.

### 6.2 ContextDB responsibilities

`src/prototype/context.c` remains responsible for:

- validating `sigma : Delta -> Gamma`;
- resolving SubstitutionDB identity/projection/extension/composition;
- building the direct substitution view;
- invoking the graph action;
- preserving typed substitution-extension checks.

### 6.3 TermDB responsibilities

`src/prototype/term.c` remains responsible for:

- traversing every TermDB tag;
- respecting lexical binding ownership;
- rebuilding and alpha-interning changed nodes;
- cloning Match/IH scope coherently;
- returning the original node when no reachable edge changes.

TermDB must not inspect source names or ContextDB proof payloads.

### 6.4 Removal target

After all callers and tests pass, `prototype_term_reindex` must no longer call
`prototype_term_graph_substitute_bound_var` once per context binding.

The single-binding primitive may remain for beta reduction and other genuinely
single-binding operations. It must not remain as a hidden compatibility path
for ordinary context reindexing.

## 7. Naming Migration

The naming migration follows the semantic implementation, so a broad rename
cannot hide behavioral regressions.

Planned final vocabulary:

| Baseline name | Final name | Scope |
| --- | --- | --- |
| graph `binder_id` | `binding_id` | TermDB, ContextDB, graph lowering |
| `prototype_term_fresh_binder` | `prototype_term_new_binding` | TermDB allocator |
| `substitution_frame_scope` | `scope_clone_entry` or narrower IH-owner equivalent | private rebuild state |
| Match `frame_id` | `ih_scope_id` | TermDB, typing, artifact relocation, and tests |

No deprecated aliases or duplicate fields are permitted. Artifact v61 stores
numeric positions rather than C identifier spellings, so an internal C rename
does not itself require a format bump. Byte-identical artifact output remains
the acceptance target.

The Match rename is subordinate to correctness. It must not be performed
partially across TermDB, artifact relocation, typing, and tests.

## 8. Implementation Phases and Progress

### C2-0: Freeze characterization

- [x] Record baseline counts for TermDB nodes, binding handles, Match frames,
  and artifact bytes on representative dependent examples.
- [x] Add a simultaneous non-cascading substitution test such as
  `[x := y, y := z]`.
- [x] Add a permutation/swap substitution test.
- [x] Add nested Lambda and Match-case capture-avoidance tests.
- [x] Add a test that alpha interning may merge bound structure but never
  merges distinct free ContextDB binding handles.
- [x] Preserve the existing scoped IH frame reindex test.
- [x] Add nested and foreign-IH-scope rejection/ownership tests.
- [x] Add a context exceeding 512 bindings to expose the current fixed limit.

Exit: tests describe current semantic requirements independently of the old
algorithm; only the over-512 test is expected to fail initially.

### C2-1: Build the immutable substitution view

- [x] Introduce a private binding-to-term entry structure.
- [x] Flatten identity, empty, projection, extension, and composition without
  textual-name lookup.
- [x] Validate source/target context orientation.
- [x] Ensure mapped terms are not recursively rewritten by sibling map entries.
- [x] Replace fixed 512-entry scratch arrays with capacity-checked dynamic or
  caller-owned storage.
- [x] Add focused view-construction tests.

Exit: every valid SubstitutionDB object has one deterministic simultaneous
binding-edge view.

### C2-2: Implement one-pass direct graph reindexing

- [x] Add an internal traversal with lexical binding scope.
- [x] Handle every non-binding TermDB tag structurally.
- [x] Handle Lambda, Pi/families, Match case binders, effect-row forall, and
  computation-fold binders explicitly.
- [x] Return the original term ID when no child edge changes.
- [x] Add traversal-local memoization with a scope-correct key.
- [x] Intern only final rebuilt nodes.

Exit: ordinary reindex no longer requires temporary graph variables.

### C2-3: Integrate Match/IH scope cloning

- [x] Model recursive Match ownership as hidden graph binding scope.
- [x] Replace the standalone frame-remap traversal protocol with the generic
  scoped rebuild environment.
- [x] Reuse an unchanged Match and frame when no reachable edge changes.
- [x] Allocate a new IH scope only when the owner graph changes.
- [x] Redirect all and only owner-local IH references.
- [x] Preserve Match-frame canonical/link key behavior.

Exit: a rebuilt recursive Match cannot retain an IH edge to the old owner, and
an unchanged Match does not receive a gratuitous new frame.

### C2-4: Switch `prototype_term_reindex`

- [x] Route the public API through the new substitution view and graph action.
- [x] Remove both fresh-binder passes from `context.c`.
- [x] Confirm that context telescope and Pi codomain instantiation callers need
  no compatibility branch.
- [x] Confirm that ordinary beta substitution still uses the single-binding
  primitive only where semantically appropriate.
- [x] Delete dead reindex-only helpers.

Exit: `context.c` contains no fresh-binding workaround for simultaneous
substitution.

### C2-5: Apply the terminology migration

- [x] Rename graph-semantic `binder_id` fields and parameters to `binding_id`.
- [x] Rename the binding allocator with no compatibility alias.
- [x] Decide and atomically apply the Match `frame_id` to `ih_scope_id` rename.
- [x] Keep source symbols and AST identities visibly separate.
- [x] Update current documentation comments and debug output.

Exit: no graph binding handle is documented as a human-readable name.

### C2-6: Full verification

- [x] Run `make`.
- [x] Run all `src/prototype/test_*.sh` scripts.
- [x] Run examples 01-07 and 09.
- [x] Verify identity and composition reindex laws.
- [x] Verify dependent Pi, constructor telescope, Match motive, and CBPV
  boundary reindex laws.
- [x] Verify shared-core/different-classifier behavior remains unchanged.
- [x] Verify artifact v61 round trip and malformed-artifact rejection.
- [x] Verify deterministic artifact v61 output and semantic link/readback
  parity.
- [x] If bytes change because transient fresh binding allocation disappeared,
  record the exact relocation/ID difference and verify that no serialized
  meaning changed; do not preserve transient allocation for byte compatibility.
- [x] Run `git diff --check`.
- [x] Update this progress sheet and the parent V2 matrix with evidence.

Exit: all existing behavior is preserved, the 512-binding case passes, and no
temporary fresh-binding reindex path remains.

## 9. Expected Code Surface

| File | Expected work |
| --- | --- |
| `src/prototype/context.h` | document binding-handle semantics; retain public reindex API |
| `src/prototype/context.c` | build direct substitution view; remove fresh two-pass algorithm |
| `src/prototype/term.h` | expose only the narrow internal graph-action API if required; rename graph binding vocabulary atomically |
| `src/prototype/term.c` | one-pass scoped rebuild, memoization, and coherent IH-scope cloning |
| `src/prototype/typing.c` | call-site verification; no new typing rule expected |
| `src/prototype/ast.c` | call-site and eventual vocabulary migration only; no reindex semantics return to AST |
| `src/prototype/context_category_check.c` | simultaneous, large-context, identity, and composition laws |
| `src/prototype/shared_term_reindex_check.c` | shared syntax, binding scope, Match/IH, and CBPV reindex laws |
| artifact tests | relocation and byte-parity regression checks |

Implementation writes remain under `src/prototype/` under repository policy.

## 10. Artifact and Compatibility Policy

V2-C2 is intended to have no artifact format effect:

- no new TermDB tag;
- no new serialized ContextDB or SubstitutionDB record;
- no HOTT object term;
- no value/computation graph split;
- no interpretation change for valid v61 artifacts.

Removing transient fresh bindings can change later numeric IDs and therefore
can change artifact bytes without changing the v61 schema or meaning. V2-C2
requires deterministic output, successful relocation/link/readback, and
semantic parity. It does not require preserving accidental numeric allocation
history.

The implementation must not keep the old reindex algorithm behind a fallback
flag. If direct reindexing cannot preserve a valid v61 behavior, the discrepancy
must be diagnosed as either a bug in the new implementation or an invalid old
behavior and resolved explicitly.

## 11. Non-Goals

V2-C2 does not:

- adopt De Bruijn indices or levels;
- introduce raw host pointers into artifacts;
- add a second value-side or computation-side Lambda/APP/Pi/Match family;
- remove CwF contexts or substitutions;
- make ContextDB a runtime environment;
- evaluate effects during reindexing;
- solve Match motives or IH classifier constraints;
- add object equality, paths, transport, univalence, or HOTT witnesses;
- merge definitional conversion with object equality;
- redesign alpha interning beyond changes required to avoid regressions.

## 12. HOTT Dependency

V2-S1 and V2-O1 will manipulate typed endpoints, carriers, witnesses, and
residual obligations under context substitutions. Their required law is:

```text
Gamma |- p : Eq_A(a, b)    sigma : Delta -> Gamma
--------------------------------------------------
Delta |- p[sigma] : Eq_{A[sigma]}(a[sigma], b[sigma])
```

This law is about binding-object graph action, not source-name replacement.
Implementing typed HOTT records before V2-C2 would have frozen the baseline
temporary fresh-renaming protocol into solver and proof code. The resulting
order is:

```text
V2-C2 -> V2-S1 -> V2-P1 -> V2-O1 -> V2-A1
```

V2-P1 may overlap with late C2 verification only if it does not change the
reindex API or proof ownership used by substitution extension.

## 13. Completion Evidence

V2-C2 was implemented on 2026-08-07 with the following concrete result:

1. `prototype_term_reindex` now builds one immutable array of
   `prototype_binding_replacement` entries and invokes one TermDB graph action.
   The old two-pass fresh-binding algorithm and its 512-entry scratch arrays
   were removed from `context.c`.
2. The graph traversal uses direct binding-handle lookup, lexical binding scope,
   IH-scope clone scope, and a traversal-local memo keyed by term and both scope
   identities. The single-binding beta API builds a one-entry view and uses the
   same traversal; it is not a fallback reindex algorithm.
3. Context entries can repeat the same binding handle when a context extension
   re-exposes the same binding object. View construction deduplicates these by
   object identity from the innermost entry outward. The TermDB graph API still
   rejects an ambiguous view containing duplicate keys.
4. Graph fields and APIs use `binding_id` and `prototype_term_new_binding`.
   Match-owned recursive scope uses `ih_scope_id`, `prototype_ih_scope`, and
   `prototype_term_match_with_ih_scope`. No old C API alias remains. Artifact
   v61 deliberately retains the serialized tokens `match_frames` and
   `match_frame`; changing C terminology is not a silent schema migration.
5. `shared_term_reindex_check.c` covers simultaneous non-cascading replacement,
   permutation, free-binding identity versus bound alpha interning, lexical
   capture blocking, dependent Pi/Match/CBPV traversal, and IH ownership.
   Existing conversion and term-identity tests retain nested and foreign IH
   scope checks. `context_category_check.c` now exercises depth 513.
6. Against baseline commit `268f2e6`, `examples/09_list_induction.p` changed
   from 1317 transient TermDB slots to 122 before artifact extraction. The v61
   artifact retained 99 present terms, two cases, two case binders, one IH
   scope, 40 judgement slots, and 40 proof slots. Artifact size changed from
   27620 to 27503 bytes because staging-only holes disappeared and an
   uninitialized operation `computation_kind` field was fixed.
7. A baseline v61 artifact and the new v61 artifact both read successfully with
   the new reader. The exported `main` normalizes successfully in both with 83
   conversion steps. Two fresh writes of the new artifact are byte-identical.
8. Final verification passed: clean `make`, `make reader`, all 14
   `src/prototype/test_*.sh` scripts, examples 01-07 and 09, old/new v61
   readback, deterministic v61 output, normalization checks, and
   `git diff --check`.

The implementation did not add a BindingDB, De Bruijn indices, a second
Value/Computation graph syntax, an equality term, or an artifact tag.

## 14. Completion Criteria

V2-C2 is complete only when all of the following hold:

- [x] graph bindings are documented and implemented as opaque object handles;
- [x] source names do not participate in TermDB reindexing;
- [x] `prototype_term_reindex` performs one simultaneous graph action;
- [x] no temporary fresh binding is required for ordinary reindexing;
- [x] local binders block only their own graph binding references;
- [x] recursive Match/IH ownership survives graph rebuilding structurally;
- [x] identity and composition laws pass;
- [x] the previous 512-binding implementation limit is removed;
- [x] all prototype tests and required examples pass;
- [x] artifact v61 schema and semantic behavior remain stable, with any
  deterministic numeric-ID change documented;
- [x] the V2 parent plan records completion evidence.
