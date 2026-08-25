# Checked-Core Authority Schema

Date: 2026-08-25 JST

Status: implemented authority boundary, v87 checked container, exact checked
imports, canonical module-set composition, and persistent merge producer;
classifier and normalizer persistence remain in progress

Parent plan:

- `2026-08-25T18-14-21-PR22-EFFORT-COMPILATION-CRITICAL-IMPLEMENTATION-PLAN.md`

## 1. Invariant

The producer may search, cache, pause, retry, and retain obsolete arena records.
None of those facts proves that a module is well typed.

The independent checker receives one immutable `ElaboratedModuleView`. It treats
every classifier and contract as an untrusted assertion. Only successful direct
checking mints an opaque `prototype_checked_module` capability.

The projection from producer stores to checked-Core input is a one-time typed
relocation. The relocation arrays are destroyed immediately. They are neither a
semantic Remap layer nor persistent authority.

## 2. Store Decisions

| Producer store | Retained semantic content | Excluded content |
| --- | --- | --- |
| TermDB | Term records, Match cases and binders, IH scopes, fold clauses | capacities, scope stack, normalization caches, intern indices and statistics |
| ContextDB | parent, Binding identity, concrete classifier, extension kind, producer computation | classifier metavariable, depth, key hash, hash link, indices, comprehension cache and statistics |
| SubstitutionDB | kind, endpoints, component substitutions, assigned term and asserted classifier | key hash, hash link, indices, reindex/binding caches and statistics |
| Type semantic schema | nominal declaration identity, formation classifier, parameter/index Contexts, constructor telescopes and result classifiers | mutable revision counters, constructor classifier cache, readback and representation lookup cache |
| DimensionDB | source/target dimension and axis images | hashes, hash links, intern index and statistics |
| TypedOccurrenceGraph | the field decisions in section 3 | transaction state, capacities and producer lifecycle |
| VerificationDB | pending explicit semantic contracts and their occurrence dependencies | discharged history, failure state as content, queue/lifecycle state |
| JudgementDB | object proof Terms only where the calculus requires them | Proposition, Claim, candidate, accepted Derivation and premise IDs as checker input |
| UniverseDB | explicit level expressions and constraints reconstructed from semantic content | solver queue, selected proof provenance and closure cache |
| Artifact interface | exact checked exports, dependencies, Function Graph associations/selectors, contracts and fingerprints | accepted Claim IDs and producer work state |

## 3. Typed Occurrence Field Decisions

| Current field | Decision | Checked-Core meaning |
| --- | --- | --- |
| `tag` | retain via explicit tag translation | semantic rule selected for this occurrence |
| `category` | retain | asserted value/computation/type category |
| `computation_kind` | retain | asserted CBPV computation form |
| `application_role` | retain | typed interpretation of shared Core APP |
| `context_id` | retain and dense-relocate | exact value Context of the occurrence |
| `context_action_substitution` | retain and dense-relocate | explicit semantic reindexing action |
| `source_core_term` | retain as `origin_core_term` | source endpoint used to check the context action; not source provenance |
| `source_classifier` | retain as `origin_classifier` | classifier endpoint used to check the context action |
| `core_term` | retain | erased Core term at the occurrence endpoint |
| `classifier` | retain as `asserted_classifier` | assertion to be recomputed or checked |
| `classifier_status` | reconstruct | exact or conditional evidence kind; pending is rejected |
| `classifier_verification_obligation` | relocate to contract | conditional contract reference only |
| `source_ast` | debug only | omitted |
| `source_symbol_id` | debug only | omitted |
| `binder_symbol_id` | debug only | omitted |
| `referenced_ast_binder_id` | producer only | omitted; exact `binding_id` remains |
| `binding_id` | retain | generative object binding identity |
| `first_edge`, `edge_count` | retain | semantic child-role range |
| `wrapped_occurrence` | retain | explicit expected-type/source-wrapper boundary needed by checking |
| `binder_classifier` | retain | explicit binder annotation at Lambda/fold rules |
| `match_motive` | retain after final solver freeze | explicit checked motive; checker never synthesizes it from branches |
| `ih_owner_occurrence` | retain | exact Match owner of the IH occurrence |
| `ih_scope_id` | retain | Core IH scope identity |
| `ih_case_index`, `ih_field_index` | retain | recursive constructor-field position |
| `fold_return_ast_binder_id` | producer only | omitted |
| `fold_return_binder_id` | retain | fold return-clause Binding identity |
| `implicit_effect_row_binders` | retain | classifier-only row quantification |
| `first_case`, `case_count` | retain | semantic Match-case range |
| `first_fold_clause`, `fold_clause_count` | retain | semantic operation-clause range |

Case records retain Context, refinement Substitution, constructor owner and
ordinal, and exact Binding identities. Case labels and AST binder IDs are debug
data. Fold clauses retain Context and argument/continuation Binding identities;
their AST binder IDs are omitted.

## 4. Dense Semantic Closure

The producer Context arena is append-only and may contain obsolete provisional
records after fixed-point resolution. A completed compile therefore does not
project the entire arena prefix.

Projection roots are:

1. every occurrence Context and context-action Substitution;
2. every Match and fold-clause Context and refinement Substitution;
3. every type parameter/index Context;
4. every constructor parameter/field Context; and
5. the recursive component and endpoint closure of rooted Substitutions.

Context parent closure is then marked. Marked records are copied in source-ID
order, which preserves parent-before-child and component-before-composite
ordering. A reachable provisional Context may project its already concrete Term
classifier, but its producer metavariable is discarded. A reachable
variable-only Context is rejected.

Type schema and occurrence references are rewritten once to the dense Context
and Substitution IDs. Unreachable stale producer records are absent.

## 5. Identity Rules

Three identities remain separate:

| Identity | Role |
| --- | --- |
| Binding ID | generative object identity inside one semantic module |
| dense arena ID | storage identity local to one projected module |
| future GoalKey | incremental matching identity across producer revisions |

Dense relocation never merges Binding IDs. Goal matching must never become an
object-level alpha or DefEq rule.

## 6. Classifier and Contract Authority

An exact occurrence contains an asserted classifier and no contract. A
conditional occurrence refers to one pending semantic contract. A producer
`PENDING` classifier cannot be projected as completed semantics.

Discharged obligations are omitted because their result must already be present
as exact checked content. Failed obligations reject projection. Compiler effort
exhaustion is producer state and cannot create a conditional contract.

## 7. Resource and Dependency Reconstruction

Resource usage is reconstructed from semantic occurrence edges, exact Binding
identity, Context extension kinds, and operation resumption multiplicity. Stored
Proposition usage vectors remain a v86 oracle during migration, not target
authority.

Dependency closure is reconstructed from exact semantic references:

- Term children and auxiliary arenas;
- occurrence child roles and Contexts;
- Context classifiers and producer computations;
- Substitution components, assignments and endpoints;
- type and constructor schema references;
- contracts and exported roots; and
- object-level Identity/totality evidence Terms.

Accepted Derivation premise traversal is not part of the target closure.

## 8. Checker Rule Modes

| Family | Mode |
| --- | --- |
| atom, literal, intrinsic | syntax-directed local inference followed by assertion check |
| variable | exact Binding lookup in the checked Context |
| constructor | schema lookup plus explicit telescope checking |
| APP | explicit application role, local Pi/constructor rule, then assertion check |
| Lambda | explicit binder classifier and checked body occurrence |
| expected type | check synthesized wrapped occurrence against the assertion; never elaborate from it |
| Match | explicit motive/refinement/branch Context checking; no motive synthesis |
| IH | exact owner/case/field validation |
| RETURN/THUNK/FORCE | structural CBPV rules and explicit effect/totality classifier checking |
| operation request/fold | structural operation declaration and clause checking; no host execution |
| relation/Identity/termination evidence | object-term rules, never accepted Claim lookup |

`::` remains post-synthesis checking. The checker validates the elaborated
boundary and never uses it to recover a surface term.

## 9. Enforced Dependency Boundary

Public headers under `a_program/checker/` may not include producer frontend,
CompileMetadata/TypedOccurrence, Judgement/accepted replay, or artifact
publication APIs. The projection implementation is the adapter and may include
both sides. An integration check enforces this include boundary.

## 10. Current Verification

`test_checked_core_projection` currently verifies:

- a real completed source compile projects successfully;
- source AST, symbol and AST-binder mutations do not alter semantic output;
- Context/Substitution/dimension hash and index mutations do not alter output;
- type schema revision mutations do not alter output;
- source-only binder names are absent from the dense semantic symbol table;
- calculus and intrinsic declaration fingerprints are checked;
- stale provisional Context records outside semantic closure are omitted;
- a pending classifier cannot project as completed content; and
- checker public headers do not depend on producer or accepted-proof APIs.

`test_checked_core_session` and `test_checked_core_examples` additionally
verify the independent checker fragment now implemented:

- zero-indexed ADT semantic schemas and their constructor telescopes;
- value and sequence-result Contexts;
- identity, empty, projection, extension, and composition Substitutions,
  including direct non-allocating validation of `A[sigma]`;
- type views, host types, literals, variables, pure `RETURN`, Lambda, and
  source/expected-type wrappers;
- Pi application, generic ADT constructor spines, explicit Match motives,
  branch refinements, and guarded induction hypotheses;
- `THUNK`, `FORCE`, effect operation requests, zero- and multi-clause
  computation folds, latent effect rows, effect-row forall instantiation, and
  reconstructed runtime capability requirements;
- `COMPLETE`, effort `PAUSED`, malformed `REJECTED`, and unsupported `PAUSED`
  remain distinct; and
- only `COMPLETE` mints the opaque process-local checked capability.

Mutation tests reject forged classifiers, Substitution assignments and
endpoints, Match motives and refinements, effect rows, totality, handler
operation edges, handler continuation binders, and runtime capabilities.

Indexed declarations, exact resources, exports, direct dependency closure,
reconstructed Universe constraints, and local Function Graph associations are
now checked independently. Function Graph acceptance includes generated length,
two recursive calls, dependent spines, and association/selector forgery tests.

Claim-only artifact Identity roots are deliberately not translated into the
semantic interface. Relation and witness Terms that belong to the object
calculus are ordinary Core content; unsupported dimension/HOTT families cause
`PAUSED(UNSUPPORTED)` and cannot mint checked authority. Imported checked bases
now resolve every `EXTERNAL_REF` through one exact checked export capability and
compare its asserted classifier across module-local arenas. Missing and
ambiguous providers fail, while opaque bodies are never unfolded. Remaining
authority work is conditional-contract expansion and expansion of the object
Identity calculus itself.

## 11. Checked Artifact Container

The admitted fragment has a canonical v87 container with independently hashed
semantic and conditional-contract sections. Producer and debug section kinds
are optional, hash-checked, and ignored for authority. The writer requires an
opaque checked-module capability; the reader reconstructs an untrusted owned
module and runs structural validation plus a fresh independent check. Checked
capabilities and producer proof/search state are never serialized.

Byte-stability, fresh-process-style readback, semantic corruption rejection,
checked-only acceptance, and non-authoritative debug-section invariance are
permanent tests in `test_checked_artifact_container`. The v86 reader remains
temporarily isolated for unsupported Claim-only HOTT roots and will be removed,
without a compatibility parser, only after those roots have object evidence.

The optional producer section now contains the canonical outer `WorkCapsule`
format rather than arbitrary container bytes. Normal checked import hash-checks
and discards it. A separate scheduler API may extract it, then must independently
check producer kind/version, effort cost model, calculus, intrinsics, base
revision, goal key, and observed dependency fingerprints before selecting a
producer-specific payload codec. Neither successful extraction nor capsule
compatibility mints checked authority.

Capsule v2 uses stable 256-bit GoalKeys and checked-content fingerprints for
positive dependencies. It does not persist process-local Symbol IDs. Negative
observations are representable only as exact fingerprints of sealed candidate
spaces. The first producer-specific codec is fragment merge: it stores
canonical checked-fragment bytes and the exact next conflict pair, so a fresh
session rechecks capabilities and continues merge work without replaying prior
pair comparisons.
