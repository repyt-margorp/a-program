# Direct Effect Application and Perform Removal Plan

Date: 2026-08-05

Status: implemented and verified on 2026-08-06.

Baseline: `main` commit `7120766`.

## 1. Objective

Remove the contextual surface keyword and wrapper:

```ap
perform (#.print #"hello")
```

and make ordinary operation application the only source form:

```ap
#.print #"hello"
```

The target form must lower to the existing Core request constructor:

```text
OPERATION_REQUEST(
    EFFECT_OPERATION(print),
    TEXT_LITERAL("hello"),
    THUNK(LAMBDA(result, RETURN(result)))
)
```

The migration removes surface `perform`; it deliberately retains Core
`OPERATION_REQUEST` and `COMPUTATION_FOLD`.

## 2. Decisions

1. `perform` is not retained as a keyword, alias, deprecated syntax, or
   compatibility parser path.
2. Direct application is recognized semantically after name resolution.
3. An alias of an operation behaves exactly like its intrinsic spelling.
4. Pure primitive application remains ordinary APP and never becomes a
   request.
5. `#.return` remains only the contextual computation-fold return label.
6. A saturated effect operation application creates exactly one request.
7. A request keeps an explicit continuation in TermDB.
8. Computation-block sequencing may fuse its remaining continuation directly
   into the request rather than build an identity continuation followed by a
   separate zero-clause fold.
9. OperationGraph terminology changes from `PERFORM` to
   `OPERATION_REQUEST`; runtime function names may use `dispatch` or `request`
   where they mean runtime execution rather than source syntax.

## 3. Required Semantic Invariant

After the migration, accepted applications satisfy:

```text
resolved head = EFFECT_OPERATION(op)
application arity = declared arity
---------------------------------------------------
source application elaborates to OPERATION_REQUEST
```

No accepted saturated effect-operation APP may remain as a bare Core APP.

Conversely:

```text
resolved head = PURE_PRIMITIVE(p)
application arity = declared arity
---------------------------------------------------
source application remains ordinary APP spine
```

The distinction is based on the resolved Core head, not namespace spelling.

## 4. Alias and Name-Resolution Requirements

These forms must be equivalent:

```ap
#.print #"hello"
```

```ap
output := #.print;
output #"hello"
```

The compiler must follow transparent/name occurrence edges to the selected
operation identity before choosing request lowering. It must not test whether
the source starts with `#.` or whether a symbol string equals `print`.

Different operation names remain nominally distinct even when their classifier
schemas are structurally equal.

## 5. Arity Policy

The current operation declarations have maximum arity one. The implementation
must nevertheless avoid baking the one-argument assumption into the new
surface rule.

For an operation declared with arity `n`, the target policy is:

- fewer than `n` arguments: preserve a partial operation value/application
  only if the operation interface explicitly permits first-class partial
  application;
- exactly `n` arguments: create one request;
- more than `n` arguments: first create the request, then apply/sequence its
  result according to ordinary CBPV application rules, or reject if the result
  is not callable.

Before increasing `PROTOTYPE_EFFECT_OPERATION_MAX_ARITY`, the Core request
payload policy must be chosen. The existing request stores an application
prefix in `operation` and the final argument in `argument`; this is sufficient
for current arity one but should be documented and tested before multi-arity
operations are introduced.

For this migration, preserve current declared arities and add assertions that
request construction occurs only at exact saturation.

## 6. Continuation Construction

### 6.1 Expression position

A standalone operation application receives the identity computation
continuation:

```text
K = THUNK(LAMBDA(x, RETURN(x)))
REQUEST(op, argument, K)
```

The request classifier combines the operation row with the continuation result
classifier under the existing `OPERATION_REQUEST_INTRO` rule.

### 6.2 Computation block binding

For:

```ap
{
    x := #.get unit;
    #.print x;
}.x
```

the preferred canonical lowering is conceptually:

```text
REQUEST(
    get,
    unit,
    THUNK(LAMBDA(x,
        REQUEST(
            print,
            x,
            THUNK(LAMBDA(ignored, RETURN(x)))
        )
    ))
)
```

The first implementation may produce an identity request followed by a
zero-clause `COMPUTATION_FOLD` if that preserves existing block lowering. A
subsequent canonicalization step may fuse them. The two forms must normalize
equally under the computation profile.

Do not add ad hoc substitution/remap logic solely for this fusion.

### 6.3 Computed arguments

Operation arguments remain strict values under the existing surface policy. If
an argument is a returning computation, sequence it exactly once before
constructing the request. Do not duplicate an effectful argument during spine
inspection or alias resolution.

An explicitly quoted computation remains a value and may be passed as a
higher-order operation payload without being forced.

## 7. Current Code Paths to Replace

### 7.1 Reader

Current:

- `src/prototype/reader.c` recognizes the contextual text `perform` at the
  beginning of `parse_term`;
- it parses one operand and wraps it in `PROTOTYPE_AST_PERFORM`.

Target:

- remove the contextual `perform` branch;
- allow `perform` to be an ordinary identifier if otherwise valid;
- ordinary application parsing remains unchanged.

### 7.2 AST

Current:

- `PROTOTYPE_AST_PERFORM` exists in `src/prototype/ast.h`;
- `prototype_ast_perform(...)` constructs a unary wrapper;
- AST inspection prints the tag as `perform`.

Target:

- remove the enum tag, constructor declaration, constructor implementation,
  traversal cases, inspection case, serialization/readback cases if any, and
  validation branches;
- do not replace it with a `DIRECT_OPERATION_CALL` AST tag;
- retain ordinary APP syntax and determine operation meaning during
  elaboration.

### 7.3 AST-to-TermDB elaboration

Current:

- ordinary intrinsic application is compiled by the operation-spine helpers;
- saturated pure primitives and effect operations can both emerge as APP
  spines with computation classifiers;
- `compile_ast_perform_ref(...)` separately decomposes the wrapped final APP;
- `compile_perform_argument_continuation(...)` creates the identity
  continuation and `OPERATION_REQUEST`.

Target:

- split semantic completion of a saturated intrinsic spine by resolved head;
- pure primitive completion returns the ordinary APP computation;
- effect operation completion constructs the request using the existing
  continuation builder;
- make request construction callable both with the identity continuation and
  with a continuation supplied by block lowering;
- remove `compile_ast_perform_ref(...)` and perform-specific contexts;
- preserve source AST provenance on the original APP occurrence.

Recommended helper boundary:

```text
compile_saturated_effect_request_then(
    resolved_operation,
    evaluated_arguments,
    request_continuation
)
```

The helper name is illustrative. Do not introduce multiple wrappers which
only forward the same fields.

### 7.4 OperationGraph

Current:

- request occurrences use `PROTOTYPE_OPERATION_PERFORM`;
- the occurrence points to a Core `OPERATION_REQUEST` and records operation,
  argument, and continuation edges;
- solver, runtime, artifact traversal, and debug output branch on `PERFORM`.

Target:

- rename the tag to `PROTOTYPE_OPERATION_REQUEST`;
- retain all three occurrence edges;
- update validation so tag and Core term must agree;
- update solver proof reconstruction to produce
  `OPERATION_REQUEST_INTRO` from a request occurrence;
- update runtime frames/debug output to use request terminology;
- do not infer request identity by searching for a shared Core term.

### 7.5 Typing and JudgementDB

Keep the existing Core proof kind:

```text
PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO
```

Update only the occurrence source and any stale perform-named helper APIs.

The validator must continue checking:

- the operation head is `EFFECT_OPERATION`;
- argument classifier matches the operation input;
- continuation is `THUNK(LAMBDA(...))`;
- continuation domain matches the operation result;
- output effect row is the union of request and continuation rows;
- output result matches the continuation result;
- forbidden free result binders do not escape.

Do not weaken these checks merely because the source spelling becomes shorter.

### 7.6 Normalizer and runtime

Core normalizer logic for `OPERATION_REQUEST` remains.

Rename perform-named implementation functions only where they actually operate
on requests, for example toward:

```text
dispatch_operation_request_step
resume_operation_request
```

The runtime must still:

- stop at unhandled requests during pure normalization;
- let computation folds intercept requests before host dispatch;
- forward unmatched requests with transformed continuations;
- dispatch to the host only in an explicitly executable mode.

### 7.7 Artifact format

TermDB `OPERATION_REQUEST` serialization is unchanged.

OperationGraph tag renaming changes the meaning/name of a serialized tag even
if its numeric position is preserved. Prefer an artifact version bump and
strict old-version rejection rather than relying on accidental numeric
compatibility.

Artifact validation must reject:

- an OperationGraph request occurrence whose Core term is not
  `OPERATION_REQUEST`;
- a reachable saturated effect APP which escaped request elaboration;
- a request whose operation head is not an effect operation;
- missing operation, argument, or continuation occurrence edges.

## 8. Implementation Phases

### Phase 0: Baseline and characterization

- [x] Record the starting commit and artifact version.
- [x] Run all `src/prototype/test_*.sh` tests.
- [x] Run examples 01-07 and 09.
- [x] Add a characterization test proving that bare `#.print #"x"` currently
      does not behave like a request.
- [x] Add AST/graph inspection coverage for the former wrapped form.

Exit criteria: the pre-change inconsistency is reproduced by a focused test.

### Phase 1: Centralize request construction

- [x] Extract one request-construction path from
      `compile_perform_argument_continuation(...)`.
- [x] Preserve strict argument evaluation exactly once.
- [x] Preserve generated continuation binder/context identity.
- [x] Preserve OperationGraph edges and source provenance.
- [x] Keep current `perform` syntax temporarily only during this internal
      phase so old and new tests can compare Core graphs.

Exit criteria: current wrapped syntax uses the centralized builder and all
tests pass.

### Phase 2: Direct effect-application elaboration

- [x] Detect the resolved intrinsic head after aliases are elaborated.
- [x] Distinguish pure primitive from effect operation by TermDB tag/binding
      identity.
- [x] Build a request at exact operation arity.
- [x] Keep pure primitive APP behaviour unchanged.
- [x] Ensure a quoted higher-order argument remains unforced.
- [x] Ensure a returning computation argument is sequenced once.
- [x] Reject or explicitly preserve unsupported partial operation application.

Exit criteria: direct and wrapped forms produce normalization-equal Core
requests and equivalent OperationGraph proof results.

### Phase 3: Computation-block integration

- [x] Verify binding of a direct request result.
- [x] Verify ignored request results in block-expression position.
- [x] Verify selected block results stop at the intended dependency boundary.
- [x] Verify nested direct requests in strict argument position.
- [x] Decide whether identity-request plus zero-clause fold is retained or
      canonically fused.
- [x] Do not add clone/remap machinery for continuation fusion.

Exit criteria: block sequencing executes each request and each strict argument
exactly once.

### Phase 4: Remove surface perform

- [x] Remove reader contextual keyword recognition.
- [x] Remove `PROTOTYPE_AST_PERFORM`.
- [x] Remove `prototype_ast_perform(...)`.
- [x] Remove `compile_ast_perform_ref(...)` and perform-specific lowering
      contexts.
- [x] Remove AST inspection and traversal cases.
- [x] Convert every prototype fixture and test to direct application.
- [x] Add a parser test showing `perform` may now be an ordinary identifier.
- [x] Add a negative test showing the obsolete wrapper syntax is rejected or
      parsed only according to ordinary application rules, never specially.

Exit criteria: no parser/AST implementation reference to surface `perform`
remains.

### Phase 5: Rename occurrence terminology

- [x] Rename `PROTOTYPE_OPERATION_PERFORM` to
      `PROTOTYPE_OPERATION_REQUEST`.
- [x] Rename graph validation, solver, runtime, artifact, and debug branches.
- [x] Rename runtime helpers specifically responsible for request dispatch.
- [x] Keep host capability terminology separate from language operation
      identity.
- [x] Search all prototype source and tests for stale semantic uses of
      `perform`.

Exit criteria: `perform` appears only in historical documents or English prose
where it means ordinary execution, not as an active source/Core construct.

### Phase 6: Artifact migration

- [x] Bump the artifact version.
- [x] Update write/read/validate/aggregate paths for the renamed occurrence
      tag.
- [x] Reject the immediately previous format explicitly.
- [x] Verify aliases become requests before artifact emission and round-trip.
- [x] Verify request continuation contexts and binder relocation round-trip.

Exit criteria: artifact flow, aggregate flow, and obsolete-version rejection
all pass.

### Phase 7: Kernel invariant enforcement

- [x] Add validation that no reachable saturated effect APP remains bare.
- [x] Keep `OPERATION_REQUEST_INTRO` as the only request typing proof.
- [x] Verify pure normalization reports blocked effect rather than dispatching.
- [x] Verify handler/fold interception precedes host dispatch.
- [x] Verify unmatched forwarding preserves the recursively folded
      continuation.

Exit criteria: malformed bare effect APP artifacts are rejected and all valid
source paths produce explicit requests.

### Phase 8: Full verification and documentation

- [x] Build prototype reader and executable from clean state.
- [x] Run all prototype test scripts.
- [x] Run examples 01-07 and 09.
- [x] Run direct print, alias print, nested request, higher-order request,
      one-shot, multi-shot, abortive, handler, artifact, and definition-entry
      tests.
- [x] Update the current-graph-layer document checklist.
- [x] Record implementation and test results in this document.

Exit criteria: complete regression suite passes and no compatibility parser
remains.

## 9. Required Test Matrix

### 9.1 Positive surface cases

```ap
main := #.print #"hello";
```

```ap
output := #.print;
main := output #"hello";
```

```ap
main := &{ #.print #"later"; };
```

```ap
main := {
    x : #.Text := #.print #"first";
    #.print x;
}.x;
```

Higher-order payload:

```ap
main := #.scope_text &{ #.print #"inner"; };
```

### 9.2 Handler cases

```ap
(#.print #"x")
    @#.return y => y
    @#.print text k => k text;
```

Verify direct aliases in clause labels continue to select the same nominal
operation.

### 9.3 Negative cases

- direct application of `#.return` as if it were an operation;
- request construction from a pure primitive;
- unsaturated operation where partial application is unsupported;
- over-applied operation with a non-callable result;
- wrong operation argument type;
- effectful argument duplicated by lowering;
- malformed artifact containing bare saturated effect APP;
- malformed request continuation which is not `THUNK(LAMBDA(...))`.

### 9.4 Graph assertions

For every direct operation application, assert:

- one reachable Core `OPERATION_REQUEST` exists;
- no parser-only perform AST exists;
- one request OperationGraph occurrence exists;
- operation, argument, and continuation occurrence edges are valid;
- the continuation has a distinct source/graph binder identity;
- the request proof is `OPERATION_REQUEST_INTRO`;
- pure normalization stops at the request;
- executable evaluation dispatches once unless a handler intercepts it.

## 10. Risks and Mitigations

### 10.1 Alias recognition too early

Risk: checking AST spelling handles `#.print` but misses `output`.

Mitigation: recognize the resolved TermDB head/operation identity after name
elaboration.

### 10.2 Pure primitives accidentally become requests

Risk: the existing operation-spine helper handles both pure primitives and
effect operations.

Mitigation: branch on the resolved head tag before completion and cover both
paths with graph tests.

### 10.3 Double sequencing

Risk: direct request completion builds an identity continuation and block
lowering adds another fold, causing duplicate execution or confusing
provenance.

Mitigation: first preserve semantics and test exact dispatch counts; then fuse
through one continuation-aware builder if necessary.

### 10.4 Shared Core loses occurrence provenance

Risk: canonical request/lambda sharing causes proof reconstruction to select an
unrelated classifier or binder occurrence.

Mitigation: retain all request components as explicit OperationGraph edges and
reconstruct proofs from occurrence IDs, never Core-term lookup alone.

### 10.5 Higher-order payload is forced accidentally

Risk: generic strict argument handling opens `THUNK(M)` supplied to scoped
operations.

Mitigation: a thunk is already a value. Strict argument elaboration evaluates
the expression producing the thunk but does not force the thunk payload.

### 10.6 Artifact numerical compatibility hides semantic change

Risk: renaming an enum while retaining its number causes old data to appear
valid accidentally.

Mitigation: bump the format and reject obsolete artifacts.

## 11. Completion Criteria

The migration is complete only when all are true:

1. `#.print #"hello"` constructs and executes one request.
2. transparent aliases behave identically.
3. pure primitive applications remain request-free.
4. higher-order thunk arguments remain suspended.
5. computation blocks sequence request results exactly once.
6. handlers intercept direct operation applications.
7. unhandled requests reach host dispatch only in executable mode.
8. pure normalization never performs host effects.
9. surface `perform`, `PROTOTYPE_AST_PERFORM`, and
   `PROTOTYPE_OPERATION_PERFORM` are gone.
10. Core `PROTOTYPE_TERM_OPERATION_REQUEST` remains explicit.
11. artifacts round-trip request continuations and occurrence provenance.
12. the full regression suite and examples 01-07,09 pass.

## 12. Progress Tracking

| Phase | State | Notes |
|---|---|---|
| 0. Baseline and characterization | complete | Baseline `7120766`, artifact v60, and pre-change stuck direct APP recorded |
| 1. Centralize request construction | complete | `compile_ref_make_operation_request` owns request construction |
| 2. Direct effect-application elaboration | complete | Resolved effect heads request at saturation; pure primitives remain APP |
| 3. Computation-block integration | complete | Existing zero-clause sequencing retained; no remap/fusion machinery added |
| 4. Remove surface perform | complete | Reader, AST, lowering, fixtures, and tests use direct application |
| 5. Rename occurrence terminology | complete | OperationGraph uses `PROTOTYPE_OPERATION_REQUEST`; dispatch helper renamed |
| 6. Artifact migration | complete | Artifact v61 round-trips and v60 is rejected |
| 7. Kernel invariant enforcement | complete | OperationGraph validation rejects saturated effect APP occurrences |
| 8. Full verification and documentation | complete | Build, all scripts, and examples 01-07/09 pass |

Update this table incrementally. Do not mark a phase complete until its exit
criteria and relevant tests pass.

## 13. Change Log

### 2026-08-05: Plan created

- Confirmed that surface `perform` and Core request representation are separate
  design decisions.
- Decided to remove the surface keyword and AST wrapper without removing
  `OPERATION_REQUEST`.
- Required semantic direct-application recognition after name resolution.
- Required occurrence terminology to move from `PERFORM` to
  `OPERATION_REQUEST`.
- Defined implementation phases, artifact migration, graph invariants, and
  regression coverage.

### 2026-08-06: Migration implemented

- Removed the contextual reader keyword, perform AST node, constructor,
  inspection branch, and perform-specific lowering pipeline.
- Unified ordinary application elaboration. A resolved saturated effect
  operation now constructs `OPERATION_REQUEST`; a pure primitive remains APP.
- Preserved alias resolution, strict argument sequencing, higher-order thunk
  payloads, request continuation occurrences, and handler behavior.
- Renamed the OperationGraph occurrence to `PROTOTYPE_OPERATION_REQUEST` and
  the request runtime step to dispatch terminology.
- Added direct execution, alias, artifact, ordinary `perform` identifier,
  obsolete-wrapper, and saturated-effect-APP validation coverage.
- Migrated prototype fixtures to direct application and bumped artifacts from
  v60 to v61 with explicit v60 rejection.
- Verified warning-free `make`, `make reader`, every
  `src/prototype/test_*.sh`, and examples 01-07 and 09.

## 14. Related Documents

- `2026-08-05T07-00-00-CURRENT-GRAPH-LAYERS-AND-CBPV-EFFECT-CORE.md`
- `2026-08-05T06-00-00-DEFINITION-BLOCK-AND-IMPLICIT-THUNK-POLICY.md`
- `2026-08-05T03-00-00-HIGHER-ORDER-EFFECT-CAPABILITY-DECISIONS.md`
- `2026-08-05T00-00-00-MATCH-STYLE-COMPUTATION-FOLD-AND-INTRINSIC-RETURN-PLAN.md`
- `2026-07-18T00-00-00-EXECUTION-SUPPRESSION-CBPV-SURFACE-PLAN.md`
- `2026-07-16T01-00-00-RESIDUAL-DEPENDENT-CBPV-MIGRATION.md`
