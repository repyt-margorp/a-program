# Issue Lifecycle and Consolidation Worker Plan

Date: 2026-10-03
Status: verified; bounded disposition audit complete
Code baseline: Main `09ee7750604f1de2264dd4dad5ab5f174583c655`; unrelated local
edits are excluded. Issue and worker state must be rechecked when deciding.

## Problem List

1. I1: Completed or obsolete issues may remain open, while actionable residual
   work is scattered across overlapping historical issues.

## I1. Evidence-Based Issue Disposition

### Subjective (User)

- 2026-10-03, user paraphrase, active thread Goal and explicit assignment in
  `src/prototype/coordination/issue-audit-brief.md`: inspect #59 first, then every
  current open issue; perform justified evidence-backed GitHub comments,
  closures and useful minimal consolidation, preserving unresolved requirements.
  Coordinate writer ownership with Merge through the existing outbox and verify
  all changes. No implementation edits, duplicate workers or Main integration.
- 2026-10-03, English paraphrase: create a separate worker to audit whether
  issues are completed, still necessary, or closable. Close justified cases;
  where useful, extract remaining work from several issues into a consolidated
  new issue and close the superseded originals with explicit links.
- Same message, English paraphrase: Issue #59 may already be finished, and the
  test-suite worker appears to have run before. Check existing work rather than
  creating duplicate assignments. This is a concern to investigate, not a user
  declaration that all #59 criteria are satisfied.
- Earlier workflow, English paraphrase: Merge owns Main integration and conflict
  resolution; implementation workers may continue independently. This new role
  is issue disposition, not a second integration owner.

### Objective (Code)

- Fresh GitHub inspection in this desk confirms PR #60 is merged and Issue #59
  is open. Its initial static audit was delivered. The completed audit report
  explicitly leaves controlled cost, dynamic coverage and full issue criteria
  open. These are historical report limits to recheck, not current bug claims.
- New test-suite implementation worker setup reuses that audit. It must not
  repeat the completed read-mostly assignment or silently claim its remaining
  work has already been implemented.
- Fresh 2026-10-03 inspection at `09ee7750604f1de2264dd4dad5ab5f174583c655`
  (remote Main agrees), plus only this worker's plan/outbox edits: ten open
  issues, zero open PRs; all ten bodies and 27 comments read after #59-first
  inspection. PRs #42/#45/#48/#50/#53/#54/#55/#58/#60 are merged; #54 contains
  prototype implementation, the others are documentation. No compiler build,
  gate or benchmark is run by this audit.
- Parsed delivered static inventory: revision `4d1d941`, 845 static recipe
  rows across profiles, 88 shell-source references, 114 reasoned legacy
  dependencies and 385 pinned source hashes. Its cost key is a proposal without
  results. Accepted scope changed by 13 files (+793/-18) since that inventory
  through promotion `7631e5a`; historical counts cannot be called the current
  effective suite. The new test-suite worker at `09ee775` has an active
  plan/initialization notice and no delivered reduction claim at inspection.
- Inspected C adapter `main.c:125-149`: whole-module completion or explicit
  unauthenticated trusted completion still gates exports. C README and current
  lowerers retain dependent/higher Identity, indexed/callable representation and
  native Acc/QuickSort limits. C9/C10 records distinguish worker execution,
  Root hash/status review and sanitizer scope. C11 submitted archive is worker
  delivery, not an integrated/current-producer result in this baseline.
- Confirmed integrations `5683755` (static audit), `7a9a672` (Surface),
  `966c4e1` (finite functions), `48c42eb`/`2753441` (C9/C10) and accepted
  performance promotion `7631e5a` are ancestors of inspected HEAD. Runtime
  records are pinned historical evidence, freshly read rather than rerun.
- Fresh GitHub completion check, 2026-10-03 13:14:44 UTC: all ten original
  issue bodies unchanged; ten exact disposition comments match prepared text;
  #44/#49 closed with `state_reason=not_planned`; successor #61 open with both
  predecessor links and separate P1/P2 requirements. Cross-links were verified
  before each closure. Open set is #41/#43/#47/#51/#52/#56/#57/#59/#61 (nine),
  with zero open PRs. Compact API verification is retained in the frozen handoff.
- Independently inspected Merge C11 evidence at
  `/tmp/a-program-merge-c11-e10-gates-20261003`: invocation pins exact task
  `56e4ae875a18f336fff2262925472158a0d5d249`, Main `09ee775`, E9+E10 source
  manifest `73fa86c9`/128 sources, thirteen backend targets and exit 0. I/O
  invocation is terminal with its nineteen status assertions. All 92 retained
  Root evidence hashes match manifest SHA256
  `0f393ee5604005f32d18ccf398e88d18c06f6705740875ed373740eea682aea7`.
  This verifies existing results, not a new test run or native-sort completion.
- Git worktree remains at `09ee775` with no tracked edits. Only the owning plan
  and small outbox notices were authored here; the setup brief was pre-existing.
  Shared Git metadata is on a read-only mount, independently checked with
  `findmnt`; exact frozen handoff replaces task-branch staging/publication.

### Assessment

- 2026-10-03, superseding Merge technical receipt transported into this thread:
  no pending Merge writes or objection to #44/#49 supersession if original
  residuals/provenance, separate source/native sections and verified cross-links
  survive. No accepted #57 polarity/brace decision exists. Merge reports C11
  task `56e4ae8` pushed and current E9+E10 source `73fa` thirteen backend plus
  nineteen I/O gates zero; local prototype Main `545baa62`, push pending.
  Corrected E11 `d22` is task-published, joint current-producer qualification
  pending; MEM1 capacity candidate is not measured RSS/time. These are
  coordinator reports to verify, not human design approval or fresh worker
  tests performed by this audit. Test pane `%12` owns prototype simplification;
  completed static-audit owner stays stopped.
- 2026-10-03, Merge operational receipt transported into this thread: Merge
  stops its own GitHub issue mutations; issue-audit is activated as the sole
  disposition writer. Coordinate evidence/status but do not wait for repeat
  human permission. Assess #59 from evidence. This is writer coordination under
  existing human authority, not a new human design decision or closure verdict.
- Agent operational decision: assign one read-mostly `issue-audit` worker to
  coordinate issue dispositions with Merge and the implementation owners.
- A merged documentation PR, a completed bounded Goal, or a prototype epoch does
  not alone satisfy a broader issue's criteria. Conversely, obsolete demands
  should not keep an issue open merely because they were once written down.
- The user authorizes justified GitHub closure and consolidation. Preserve the
  original requests, supporting evidence, unfinished criteria and scope changes;
  do not disguise unresolved work as successfully implemented.
- Agent disposition decision: retain eight distinct active/design issues.
  Propose one residual C-backend successor for #44/#49 with separate semantic
  admission/structural Identity and target-native realization boundaries under
  the existing C owner/plan. Preserve #47 relevance and #52 checked-equality
  decisions separately. Initial C subset is delivered, broader criteria are
  not; close originals only as superseded (`not_planned`). This removes two
  overlapping historical C checklists without a catch-all or reassignment.

### Plan

- [x] Inventory open issues and relevant PRs, with a pinned inspection date;
  reuse current worker status, code and existing verification before requesting
  new work.
- [x] Audit #59 first and reconcile the completed static audit with the new
  implementation assignment.
- [x] Classify each issue: complete, obsolete, duplicate/superseded, actionable
  residual, deferred design decision, or insufficient evidence.
- [x] For closable issues, add a concise evidence-backed disposition comment
  and close. Coordinate with Merge first to avoid concurrent issue changes.
- [x] If consolidation improves clarity, create a minimal successor issue with
  source links, unchanged unresolved criteria and an owner; cross-link originals
  and close them explicitly as superseded rather than implemented.
- [x] Keep uncertain or active requirements open and route concrete residual
  work to its owner without starting duplicate implementation sessions.
- [x] Publish a concise before/after issue table, reasons, source evidence and
  remaining work; update the plan in place and report to Merge.
- Completion: every inspected open issue has a reasoned disposition; justified
  GitHub updates are performed and verified, with no lost requirement or false
  implementation/verification claim.

### Verified Disposition Table

Evidence below is code/record inspection at `09ee775` unless a task/producer is
specified. The linked GitHub comments retain detailed scope, provenance and
remaining criteria. This table is the audit record, not another development queue.

| Issue / original requirement | Delivered evidence / revision | Necessary residual | Existing owner | Verified disposition / reason |
| --- | --- | --- | --- | --- |
| [#59](https://github.com/repyt-margorp/a-program/issues/59#issuecomment-5969471664): suite inventory/lifecycle/cost/progress | `7ed3ad1`/`5683755`; static inventory, 114 legacy reasons and sorting pilot | Dynamic/current-producer scope, controlled suite cost, full central ledger and coverage-preserving reduction | Test-suite; Merge ledger/slots | Open: static bounded delivery is not full criteria |
| [#57](https://github.com/repyt-margorp/a-program/issues/57#issuecomment-5969472121): binder polarity/migration | `90939fc`/`276f4c3`, prototype Main `7a9a672`; scope/typed-evidence/version gates | Accepted polarity/brace policy, diagnostics and atomic accepted migration/gates | Merge/user review; Surface stopped | Open: no accepted syntax decision exists |
| [#56](https://github.com/repyt-margorp/a-program/issues/56#issuecomment-5969472534): safe head recovery/profiling | Accepted13 `7631e5a`; frozen E6 measured30; E9+E10 qualification | Current accepted/prototype comparison, MEM1 attribution/safe deletion/net wall/RSS; preserve fuel/known failures | Performance + Job; Merge slots | Open: accepted promotion and E6 measurements are partial delivery |
| [#51](https://github.com/repyt-margorp/a-program/issues/51#issuecomment-5969472956): reproducible owner/size gates | PR53 audit, SE/AP owner epochs and reporting | LOC/state review, scoped topology, consumer/serialized-field classification and public partition reporting | Job/Evidence; Merge; Performance | Open: semantic-owner gates differ from #59 invocation inventory |
| [#52](https://github.com/repyt-margorp/a-program/issues/52#issuecomment-5969473489): one checked relation reused | PR53 design; existing receipts/typed roots | Endpoint/validator contract, narrow reuse/round-trip/negative experiment and measured cost | Deferred Merge design | Open: storage consolidation/trust/C emission do not establish relation reuse |
| [#43](https://github.com/repyt-margorp/a-program/issues/43#issuecomment-5969474056): recurrence logical boundary | PR45 research; current totality/induction guards | Operational/lexical/state/synthesis/effect/termination decisions and scoped tests | Merge/user design review | Open: no recurrence rules chosen; C work proceeds independently |
| [#47](https://github.com/repyt-margorp/a-program/issues/47#issuecomment-5969474469): relevance/partial admission | PR48 design; adapter and limited Identity code inspected | Dependency/accounting/admissibility rules, Identity conditions, unsafe-pending negatives and receipts | Merge design; C downstream | Open: whole-module trust and bounded actions are not general relevance |
| [#41](https://github.com/repyt-margorp/a-program/issues/41#issuecomment-5969474904): common lawful finite sorting | Accepted F1/F2; PR54 generic Merge; `5e6ed84`/`966c4e1` finite-function prototype | Accepted Sort/F5 adoption/gates; broader tree/map views and separate stability/cost claims | Merge/user adoption; Sort stopped | Open: prototype results fill old gaps but not accepted/broader scope |
| [#44](https://github.com/repyt-margorp/a-program/issues/44#issuecomment-5969505095): semantic Artifact-to-C boundary | Initial structural subset/differentials; C11 task `56e4ae8` | Selected-export admission, dependent/higher Identity and retained source/target controls → #61/P1 | Existing C; #47 policy dependency | Closed `not_planned`: superseded, no completed-residual claim |
| [#49](https://github.com/repyt-margorp/a-program/issues/49#issuecomment-5969506676): native C calls/data/control | C1-C10; C9 Root E8 thirteen O2 gates; C11 current-E9+E10 qualification | Indexed/callable Acc/native QuickSort, dynamic/effect/identity contracts, refinement/receipts → #61/P2 | Existing C | Closed `not_planned`: superseded, native sorter remains required |
| [#61](https://github.com/repyt-margorp/a-program/issues/61): residual C successor | Both predecessor bodies/links preserved; 813-word SOAP with two boundaries | All P1/P2 criteria transferred; #43/#47/#51/#52/SE-AP stay distinct | Existing C plan; Merge coordination | Open: one successor replaces two historical checklists |

## Progress

| Date | Problem | Material Result | Next Step |
| --- | --- | --- | --- |
| 2026-10-03 | I1 | New human request recorded before disposition audit | Start isolated issue-audit worker and audit #59 first |
| 2026-10-03 | I1 | #59 first; all ten issues/27 comments and nine related PRs inspected; sole-writer receipt consumed | Preserve separate residual scopes; prepare ten comments and one C successor |
| 2026-10-03 | I1 | Ten comments verified; #44/#49 superseded by #61, original bodies unchanged; open count10→9/PR0 | Exact frozen plan/API handoff to Merge; bounded worker stops |
