# Polarity-Neutral Application and Constructor Migration

Date: 2026-07-31

## 1. Purpose

This migration makes the existing shared computation graph agree with the
polarized CBPV operation layer.

The shared `TermDB` already uses one binary `APP(left, right)` node for lambda
application, type-family application, and constructor spines. The remaining
problem is semantic: several lower-level paths still describe every `APP` as a
beta eliminator and every constructor through its derived curried `Pi` view.
That description conflicts with source occurrences where a saturated
constructor spine is a value introduction.

The migration does not duplicate the graph into separate value and computation
term databases. It makes the interpretation of a shared structural node an
explicit property of its typed source occurrence.

## 2. Design Commitments

These commitments preserve the project direction established in the existing
design documents.

1. `TermDB` is the shared, context-erased computation graph. It records how a
   term is structurally assembled; it is not the execution-machine state.
2. A source program can name and manipulate only typed occurrences. A raw core
   node is not, by itself, a complete source-level operation.
3. Alpha-equivalent lambda structures may share one core node while their
   contexts, classifiers, names, and polarities remain distinct in the
   operation and judgement layers.
4. `APP(left, right)` remains one structural node. Constructor application does
   not receive a second application node tag.
5. `RETURN`, `THUNK`, `FORCE`, computation sequencing, and effect handling stay
   explicit. They change evaluation timing or effect structure and therefore
   cannot be erased as mere application polarity.
6. A fully applied ADT constructor is a value introduction. Match is its value
   eliminator and constructor iota is separate from beta reduction.
7. A constructor declaration's authoritative schema is its parameter context,
   dependent field context, and result classifier. A curried `Pi` classifier is
   a derived callable/readback view, not a second schema authority.
8. A constructor used as a computation function must be eta-expanded through
   lambda/return and thunked when it is passed as a value. A primitive,
   under-applied constructor spine is not silently reclassified as completed
   ADT data.

## 3. Structural and Typed Layers

### 3.1 Shared structural graph

The following nodes remain reusable across typed occurrences:

```text
VAR(binder)
LAMBDA(binder, body)
APP(left, right)
CONSTRUCTOR(owner_core, ordinal)
MATCH(scrutinee, cases)
RETURN(value)
THUNK(computation)
FORCE(value)
DEEP_FOLD(computation, continuation, clauses)
```

`APP` means only a binary structural edge at this layer. Its tag alone does not
prove that beta reduction applies.

### 3.2 Typed operation occurrence

Each `OperationGraph` occurrence records:

- its value/computation polarity;
- its computation kind where relevant;
- its context;
- its classifier variable and solved classifier;
- the rule used to interpret an application occurrence.

The important application rules are:

```text
FUNCTION_ELIMINATION:  Pi(A, C) with a value A, producing C(a)
CONSTRUCTOR_FORMATION: dependent constructor telescope with the next field
TYPE_FORMATION:        pure type-family application during classifier work
```

They may all point to the same structural `APP` representation, but the kernel
must not infer the rule from the `APP` tag alone.

## 4. Constructor Invariants

For a constructor schema

```text
parameters: Gamma
fields:     Delta = (x1 : A1, ..., xn : An(x1, ...))
result:     R(x1, ..., xn)
```

the compiler must enforce:

1. owner arguments instantiate `Gamma`;
2. field argument `i` is checked against the reindexed `Ai` after all previous
   fields have been substituted;
3. exactly `n` fields form the introduced value `R(...)`;
4. the result classifier is obtained from the schema result under the same
   substitution;
5. the derived curried `Pi` cache may be regenerated and validated, but cannot
   override any of these decisions.

Intermediate APP nodes used while constructing a saturated spine are builder
structure. If an under-applied constructor escapes as a source value, it must be
represented by an explicit eta-expanded function value or rejected.

## 5. Implementation Plan

### Phase 1: Make application semantics occurrence-indexed

- [x] Replace tag-only `APP` semantics with term-aware structural semantics.
- [x] Record polarity and computation kind on every OperationGraph node when it
      is created, not only on top-level name occurrences.
- [x] Mark constructor-headed APP occurrences as constructor formation.
- [x] Prevent the classifier constraint generator from treating those
      occurrences as ordinary Pi elimination.

### Phase 2: Centralize constructor spine inspection

- [x] Move repeated constructor-head/spine decoding behind one TermDB query.
- [x] Return owner, ordinal, and ordered field arguments from the structural
      query; derive declared arity and saturation from the authoritative
      constructor context.
- [x] Reuse that query in lowering, operation semantics, typing, and tests.
      Runtime Match continues to consume the same constructor-headed APP shape.

### Phase 3: Make telescope typing authoritative

- [x] Check constructor fields directly through ContextDB/SubstitutionDB.
- [x] Compute the saturated result classifier from `result_classifier`.
- [x] Keep `curried_classifier_cache` only for residual callable/readback
      compatibility
      until every ordinary APP-elimination dependency has been removed.
- [x] Reject over-application and escaping under-application unless an explicit
      eta-expanded function occurrence was constructed.

### Phase 4: Regression coverage

- [x] Nullary and saturated constructors are value occurrences.
- [x] Dependent constructor fields, including `Sigma.mk`, use the telescope.
- [x] Constructor fields that are computations are sequenced exactly once.
- [x] Explicit eta expansion permits higher-order constructor use.
- [x] An escaping primitive partial constructor is rejected, including nested
      higher-order use.
- [x] Artifact round-trip and linking preserve constructor schema and operation
      polarity.
- [x] Examples 01 through 07 and 09 continue to pass.

## 6. Compatibility Policy

This is a prototype migration. No compatibility implementation is retained for
old artifact or operation formats. If serialized operation semantics change,
the artifact version is incremented and tests are migrated in the same change.

## 7. Implementation Record

This section records discoveries and implementation corrections. The design
commitments above are not rewritten to fit incidental code behavior.

- 2026-07-31: baseline `make clean all reader` completed without warnings.
- 2026-07-31: all existing `src/prototype/test_*.sh` scripts passed before the
  migration.
- 2026-07-31: audit confirmed that constructor spines already share TermDB
  `APP` nodes, while tag-only term semantics and Pi-oriented classifier
  constraints still describe them as function elimination.
- 2026-07-31: `OperationGraph` now records value/computation polarity and an
  application role for every source occurrence. Constructor-headed `APP`
  occurrences generate constructor-formation constraints instead of
  `PI_EXPECTED` constraints. Type-family applications continue to use the same
  TermDB `APP`; they are classifier expressions evaluated under the pure type
  profile rather than runtime OperationGraph applications.
- 2026-07-31: constructor field checking now walks the parameter and dependent
  field contexts through `SubstitutionDB`. A saturated spine obtains its type
  from `result_classifier`; the curried Pi cache is used only to expose the
  residual classifier of an intermediate spine.
- 2026-07-31: constructor proofs now retain the selected named owner view as
  rule metadata while requiring it to share the structural owner core. This
  preserves the distinction between the erased computation graph and the
  source-selected type view across artifact linking.
- 2026-07-31: named type instances and erased `TYPE_FORMER` instances now use
  one TypeDeclarationDB owner-resolution API. This is required by incremental
  REPL compilation, where an earlier unit's constructor owners have already
  been erased to their structural representation.
- 2026-07-31: artifact format version 54 records application roles. Version 53
  is intentionally rejected; no compatibility reader was retained.
- 2026-07-31: operation-level saturation validation rejects a primitive partial
  constructor whenever it reaches a source root or a non-constructor edge.
  Partial nodes that only feed the next constructor APP are builder steps.
  Unreachable nodes left by a failed lowering probe are not treated as source
  operations; removing those dead probes remains a separate graph-compaction
  concern.
- 2026-07-31: proof compaction was corrected to follow each relation's
  `proof_id` and copy from a snapshot. This prevents handler constraint rebuilds
  from corrupting constructor owner metadata or creating cyclic/mismatched
  proof records.
- 2026-07-31: warning-free builds, all `src/prototype/test_*.sh` scripts,
  examples 01-07 and 09, and `training/double.p` plus
  `training/list_nat_match.p` pass after the migration.
