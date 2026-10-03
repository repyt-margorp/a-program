# C Backend Native Predicate Module Composition

Date: 2026-10-04 local. Status: locally verified; Root review pending.
Code baseline: task `d39632b8adc277aa65a5a23669ca6b99d3663e33`, immutable C17
exact19, manifest `089136e9eb0adc3ab83beadf975ce10c345d8563204361e00f46cec0d6203b67`.
Related: open #61, [owning Goal](2026-10-03-C-BACKEND-GOAL.md) and
[C17 handoff](2026-10-04-C-BACKEND-EPOCH17-HANDOFF.md).

## Problem List

1. Verify ordinary generated native providers and independently named predicate
   consumers without changing the frozen C17 backend or source authority.

## 1. Native Provider and Borrowed Predicate Clients

### Subjective (User)

Existing human scope, 2026-10-03 English paraphrase: make bounded downstream
`.a`/LinkerScript C modules readily usable from other C modules; preserve the
source authority and explicit unsupported contracts. Continue independently
within prototype scope; only Core/Merge integrates Main. The ongoing Goal keeps
its full AP4-AP6 residuals, including unfinished native Acc/QuickSort. This
continuation changes no human design decision.

### Objective (Code)

Initial continuation at task `a4a2e988` verified all19 live C17 hashes and its
submitted tar. C17 final backend is
`305c521b526f7d5e281c48fdd461869cd174df8347f98c83b5a187d3b90bacfa`;
qualified E8 pointer checker remains `6e3407fc...`. Root now reports exact C17
task `d39632b8adc277aa65a5a23669ca6b99d3663e33` and prototype Main
`8dba3ecb801c36b4d0eb92c23f2c13204476922a` pushed/remote exact, with fresh
current-E17 gates passing. Fresh worker read confirms the task HEAD. This does
not qualify C18 against E17: local C18 uses retained qualified E8 tools, without
backend/producer rebuild. C17 historical evidence stays immutable; live Goal,
README and prototype gate registration now advance after release.

C15 demonstrated scalar providers with two consumers and loaded-library lifetime;
C17 supplies selected Nat32 predicates and finite native List helpers. Before
C18, their combination had no fresh verification. Different nominal aliases
produce different C enum structs and callback names, requiring explicit adapters.

Fresh worker E8 source provider admits0. Initial consumer omitted explicit import
declarations and rejected1; exact source/logs are retained. After correction,
consumer/reference admit and run0, with ninety scalar and three List source
observations. Selecting an imported name from that reference image fails1
(`definition not found`), while its local `module_choose` selects0. Products use
the separately admitted original exported C17 consumer module image; left/right
and provider emit0. The first linked ordinary source client compiles/runs0 and
matches all93 source observations. Evidence is owned disk-backed
`src/prototype/c_backend/.evidence/epoch18-native-modules-20261004`.

Final `o2-release`/`san-release` each exit0 with 103 matching expected rows:
96zero and seven expected compiler-one. Sixteen product pairs/both header orders
produce 32 linked and eight loaded-provider clients per phase. Each client checks
180 small scalar calls, 60 extreme scalar calls, 242 filters and 726 selections,
plus resource/lifetime controls; all40 match the same93 source observations.
Product/order/phase repeats are not independent properties. Initial `o2-final`
client abort134 came from expecting the leaf provider to consume recursive depth.
Its source, binary and logs are retained. Corrected controls prove local depth1
success and actual active-arena depth1 refusal4 with unchanged output.

### Assessment

Agent C18 choice: test a separate native provider using reversed enum declaration
order, plus two consumers with distinct Nat/enum/List/export aliases. Adapt tags
explicitly; do not cast nominal pointers or assert source type equality. Provider
callbacks use a separate local zero-initialized arena, since a consumer's recursive
arena is active during a foreign call. Code/context and loaded handles outlive
all synchronous calls; provider success remains an explicit precondition.

Use source predicates whose matched Nat predecessors are unused. Inspected
generated leaf matches charge no recursive depth; local depth_limit1 succeeds
on every tested small/extreme uint32 input.
Do not wrap the depth-limited recursive comparator and pretend it is a general
total foreign implementation. Verify ordinary admission/source observations,
products/header orders, shared consumer arena transactions, loaded-provider
lifetime and duplicate-symbol refusal. Only these clients/source bodies receive
SAN; other products/backend/producer stay qualified O2. No source/producer/schema
changes, general foreign failure propagation, native sorter or cost grant follow.

Agent fixture decision: keep the explicit-import reference as source verification,
and lower definitions from their original exported consumer module image. The E8
name-selection observation establishes a fixture/export distinction, not a new
producer bug or universal imported-name policy. Retain the initial failed attempt;
no producer/checker change or import re-export workaround is required.

Core operational recording: the sole issue-status table stays in the owning
Goal. Exact manifests/outbox routing and delegated read-only-Git publication
remain coordinator workflow, separate from the human bounded-C requirements.

### Plan

- [x] Add standalone source provider, explicit-import source observations and
  ordinary C adapters/clients outside C17's nineteen paths.
- [x] Pin the frozen backend/producer inputs; obtain ordinary source admission
  and actual provider/consumer products before choosing final coverage.
- [x] Run serial focused O2/client-source ASan/UBSan/leak controls, including
  distinct product/header/lifetime/transactional and negative symbol cases.
- [x] Prepare the separate [C18 handoff](2026-10-04-C-BACKEND-EPOCH18-HANDOFF.md)
  with concrete deltas, retained failures and exact pins; update the owning table.
- [ ] Root exact task publication/current-producer/Main review, distinct from
  local verification and the published C17 snapshot.
- Completion: ordinary C-module composition is verified within tested bounded
  contracts. Full Goal/native Acc/QuickSort/current-producer integration remain
  separate; no completion or promotion is implied by this candidate.
