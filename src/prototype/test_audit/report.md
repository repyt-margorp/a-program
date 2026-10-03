# Bounded First Verification-Debt Audit

2026-10-03. Static inspection at `4d1d9418f251cf03689a267183519381fc0e8168`,
branch `parallel/verification-audit-20261003`, plus this lane's untracked audit
files. Accepted baseline: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
The [owning SOAP plan](../../../doc/2026-10-03-VERIFICATION-AUDIT-PLAN.md)
contains the sole work list. Findings and recommendations below are agent analysis.

Merge confirmed receipt of slot request `2270bcbc` and directed static handoff
first while E6/private owner correctness runs. **No cost slot was granted.**
No compiler build, semantic gate, sanitizer batch or timing workload ran here.
Issue [#59](https://github.com/repyt-margorp/a-program/issues/59) and all of
[PR #60](https://github.com/repyt-margorp/a-program/pull/60) were read through
GitHub CLI; PR head was `6bda0d99260ef8eccae61248e354a742b3888d06`, open,
one 672-line document. Its preserved proposal is historical input, not approval.

## Current status for Merge's existing delivery table

| Issue / subproblem | Inspected revision | Concrete finding | Implemented / verified / proposed | Limits / next action |
| --- | --- | --- | --- | --- |
| #59 TA2 entrypoints | `4d1d941` accepted and assembled recipes | Expanded recipe inventory, gate/include graph and dynamic-site references | Prototype inventory tool implemented; fresh assembly and reproducibility verified | Shell/C effective execution remains incomplete; Merge decides later expansion |
| #59 TA4 pilot | Same, eight sorting/image script families | Accepted generic gate repeats 39 calls; 19 read-only fixed-input calls have identical argv/assertions; 20 depend on generated inputs/images | Static manual review; consolidation proposed, no suite edits | Retained and ordinary policies differ; keep unique witnesses |
| #59 TA3/TA7 legacy | Same, compatibility/syntax/CLI/image drivers | 114 archived files in the syntax cohort; 40 also in the semantic case table, one additional imported consumer | Per-file dependency/reason inventory implemented and statically verified | Syntax is not admission; keep reviewed exclusions and negative controls |
| #59 TA5 debt | Same repository reports, pinned historical epochs | Setup/report failures, actual sanitizer failures and strict semantic failures have different causes | Historical evidence classified; no fresh runtime verdict | Reuse existing reports, not another report subsystem |
| #59 TA1/TA8 progress | `eb0aad6..4d1d941`, C6 `cea1dc0`, E3 reports | Accepted source/tests unchanged; prototype capability/owner changes differ from verification volume | Read-only ledger extension supplied here for Merge | Merge owns central table and promotion; integrated prototypes remain unpromoted |
| #59 cost / #51 / #56 | Slot reply and historical performance summaries | Existing census measures dispatch/storage, not wall/RSS | Unmeasured here; timing explicitly deferred | Merge + performance choose one qualified producer/binary/scope/slot |

This completes the scheduled bounded static deliverable. Full #59 remains open:
complete dynamic coverage, controlled cost and central ledger adoption are not
claimed. No Main merge, issue closure, accepted edit or promotion occurred.

## Reproduction and entrypoint graph

From this worktree, use a new private directory:

```sh
bash src/prototype/performance_verification/overlay.sh /tmp/verification-audit-reproduce
python3 src/prototype/test_audit/inventory.py \
	--overlay /tmp/verification-audit-reproduce \
	--output /tmp/verification-audit-inventory.json
cmp src/prototype/test_audit/inventory.json /tmp/verification-audit-inventory.json
```

[inventory.json](inventory.json) is the one generated inventory. It records
expanded commands, profile/target, source rule, property family, lifecycle/owner,
gate dependencies, shell dynamic-site lines, hashes and per-archived-file reasons.
Compiler dependency lists are represented by expanded-command hashes and can be
regenerated; complete scripts and build logs are not copied. Source rule lines
come from Make trace and identify a recipe's start, not every shell line.

GNU Make can execute recursive recipes even under `-n`. The tool first rejects
unreviewed Make execution constructs, substitutes only `check-build-layout`,
then restores its actual invocation in the inventory. `tests/build_layout.sh:7`
itself runs three compiler **dry-runs** and compares their output; it is a harness
contract. The inventory never executes that shell gate. It does not create a
passing runtime result by using a dry-run exit status.

| Profile | Entry graph | Forced static recipe rows |
| --- | --- | ---: |
| Accepted | Root Makefile includes `src/Makefile`; default/all is `pointer-check`. Explicit `check-acceptance` combines `check` and specialized gates, including later prerequisite declarations | 385: 42 build/directory, 1 layout harness, 342 test recipe rows |
| Assembled | performance_verification -> performance -> artifact_persistence/build.mk -> privately assembled src/Makefile; acceptance plus explicitly listed artifact gates | 425: 58 build/directory, 1 layout harness, 366 test recipe rows |
| Surface | surface/build.mk -> same private src/Makefile; explicit `check-surface` | 14: 10 build/directory, 4 test rows |
| C | c_backend/build.mk -> artifact build -> same private src/Makefile; ten explicit C gates | 21: 10 build/directory, 11 shell rows |

Counts describe these forced Make requests, not independent semantic properties,
elapsed cost or a new historical gate result. Artifact/Surface/C gates are not
automatically prerequisites of accepted `check-acceptance`. The graph retains
Make expressions; expanded recipe rows provide the effective top-level commands.
Make executes a prerequisite target once per invocation, even if several parents
name it. Independent Make requests or inline copies of its recipe can repeat it.

Concrete accepted graph: `src/Makefile:80,112,117,122,127,133,138,143,515`
assembles acceptance; `:100,102,151` combines `check` prerequisites and its long
inline recipe. `:82-89` defines the two generic gate calls. Parsing examples
(`:456`), syntax roundtrip (`:158`), admission (`:33`) and selected results
(`:57`) protect different layers; recommend keeping all four.

The seven exact-command candidate groups in the assembled combined request are
`graph_io_test`, graph acceptance, occurrence I/O, source I/O, source-I/O
normalization, Identity I/O and image CLI. Their copies occur in `check` and
`artifact_persistence/build.mk:9-13,78-84`. Recommend sharing/reusing those exact
gate prerequisites within one qualified producer invocation while keeping
artifact-specific transport/semantic/file-policy/fuel-curve assertions. Do not
reuse across producer, instrumentation or policy changes merely because argv
matches. Accepted top-level recipes have no exact-command duplicate group.

Dynamic coverage is explicitly open: 88 profile-specific shell source references
include accepted/private versions and prototype drivers. Loops, helper calls,
here-doc case tables, negative globs, generated sed/patch providers and C-internal
chunk/cancellation matrices are not runtime-enumerated. `image_cli.sh:24-36`
derives cuts from measured completion steps. Performance's explicit raw
TotalResult/Force process commands and private epoch recorders remain in their
existing reports, not inferred from the wrapper Make graph. Internal binaries
are mapped to internal contracts, without inventing one property per assertion.

Existing inventories remain useful: `tests/inventory.sh` describes frozen
`5bdecb4` capabilities; compatibility.tsv has 346 data rows (158 programs, 188
AST/CLI/internal/integration records), not 346 current passes. Owner inventories
under solver_inputs pin source/fixture/epoch hashes; some are plain sha256 lists
despite `.tsv` filenames. They do not enumerate shell/C runtime obligations.

## QuickSort / persistence pilot

`L` = adjacent LocalSorted; `S` = StrongSorted/all later elements.
`general_sorted` remains S (`tests/fixtures/local-strong-sorted.p:7`).
Local result needs directional decision evidence; strong conversion additionally
needs transitivity. `generic-quick-sorted-result.p` independently proves S at the
ordinary result. Permutation/content multiplicity is another obligation.

The matrix describes accepted scripts; the prototype-policy difference follows.
`save/load` means checking a final verdict, not exact saved frontier resumption.

| Driver / reference | Semantic obligations | Persistence obligations | Unique retention reason / decision |
| --- | --- | --- | --- |
| quick_result.sh:27-55 | Ordinary-result S; wrong final post-check; computation motive/normalized index; optional-witness linkage absence (`nm`) | Complete, pending at 100, retained and reload; pending wrong result rejects on resume | Keep independent theorem and changed-final-assertion controls; no byte/NF assertion here |
| generic_sorted.sh:35-123 | Comparator/motive/index/scope/unsupported boundaries; graph-based S; shifted helper's four result/proof pairs | Complete; pending 0/100 and helper 0/7500; ordinary/retained; zero-fuel byte resave; rejected comparator reload | Keep all distinct boundaries; factor repeated unconditional work only after preserving retained witnesses |
| retained_quicksort.sh:10-44 | Original imported Acc sorter + explicit ordinary-result property | Solved/retained/WHNF/retained-WHNF, roots 1/2, retained-WHNF inert byte identity | Keep selected-root + WHNF interactions; compatibility's retained verdict does not check these roots |
| local_strong_sorted.sh:23-92 | L and S result proofs, transitivity conversion, cyclic comparator L/S separation, wrong edge/strong certificate, legacy strong consumer, five concrete cyclic and five consumer outputs | Each L/S: complete/load, ordinary/retained inert byte identity, pending 0/100, wrong-result pending rejection | Keep; shared filenames do not collapse mathematical requirements |
| compatibility.sh:99-104,331-400 | Six original sorter outputs; optional internal packet construction; measure/partition/graph/direct-result properties; seven property outputs; wrong recursive proof receipt | Source/imported client and image result oracle, pending/complete, retained complete, direct and reloaded rejection | Keep independent packet/runtime client and invalid left/right proof witness; repeated source admission is a separate candidate |
| image_cli.sh:9-21,178-272 | Source-origin append ordering; original sorter six image outputs; other graphs/negative induction controls | Two retained zero-fuel resaves and byte/origin checks; root/range rejection; source/image NF on List and transport, computed cuts; failed writes/REPL/discard policy | Keep NF/origin/root/publication controls; NF checks do not prove sortedness or strict frontier equivalence |
| derived_lt.sh:21-104 | Frozen/derived LT × original/tail-first partition order, old field-order rejection, ordinary-result S, permutation/content/multiplicity, independent source/image results, seven false claims | Both policies × 0/300000/complete, inert byte resave, three result checks per cut, pending negative/reloaded rejection | Keep provider/order/receipt independence and all seven negatives; not interchangeable with one S proof |
| sort_insertion.sh:68-127 (Quick branch) | Nat graph and instantiated general ordinary-result S, packets, seven outputs and two wrong claims | Pending 0/100, inert byte identity, result after resume, invalid images/rejection | Keep independent instantiated/packet boundary; shared theorem admission alone is insufficient |

Static loop expansion for the first five driver invocations counts compiler/test
binary calls only; `nm`, `cmp`, `sed`, shell assertions and build recipes excluded:

| Driver | Accepted | Assembled | Explanation |
| --- | ---: | ---: | --- |
| quick_result | 14 | 14 | Prototype retains old labels but removes retention option |
| generic normal | 39 | 41 | Prototype adds two ordinary completed-image inert resaves |
| generic retained | 53 | Not invoked | Accepted: same 39 + 14 retained-specific calls |
| retained_quicksort | 13 | 7 | Four modes become solved/WHNF; root/inert checks survive |
| local_strong_sorted | 65 | 43 | Two persistence policies become one semantic-image policy |

These are manual static counts, not executed invocation telemetry or time. For
generic's 39 shared calls: 17 fixture-loop admissions + 2 fixed LT helper checks
are identical fixed-input invocations/assertions within the accepted producer.
The other 20 include generated sources or saved images; normalized spelling does
not establish byte-identical inputs across runs. All 14 retained-only calls
remain separate: helper save/equality/resave (3), completed proof save/load/resave
(3), two pending cuts save/resave/load (6), rejected comparator save/load (2).

The current prototype already removes the second call
(`artifact_persistence/Makefile.patch:19`) **and** rewrites the script's retention
branches (`test_patches/generic_sorted.sh.patch`). It explicitly removes
`--retain-reductions` from Program-image policy; assembled image_cli instead
requires exit 2 and no published file for that option. This is a policy change,
not a coverage-equivalence proof. Keep accepted retention contracts unchanged.
If semantic-image policy is eventually promoted, archive only obsolete retention
witnesses through that explicit decision; preserve completed/pending/rejected,
byte, NF, root and origin checks. Old `retained-*` filenames/labels in the
prototype do not demonstrate retained execution. The two identically configured
ordinary-result save/load sequences now labelled complete/retained in prototype
quick_result are a semantic consolidation candidate, subject to image-key review.

Recommend a later small helper for lifecycle mechanics only if it retains each
suite's provider construction, negative mutation, fuel cut, selected root,
status/exit assertion, byte assertion and independent result/NF oracle. Reject
replacing this cluster with one giant QuickSort test or shrinking every large
witness: proof/order/ownership interactions have distinct coverage.

## Legacy retention review

inventory.json lists every one of the 114 archived program paths, its dependency
sites, bytes' digest and explicit retention reason. All are currently used by
the **legacy parse contract** in syntax_inventory.sh; 44 further program rows
are outside the archive, including four exact reviewed syntax exclusions.
Keep this cohort until the compatibility policy itself is deliberately reviewed.
`review_pending` never means an obligation to restore historical semantic behavior.

The CASES table has 63 rows over 43 resolved paths: 40 archived fixtures, two
current acceptance negatives and one current example reached by `../` spelling.
Do not classify escaped current paths as legacy merely because `$fixtures` is
the prefix. The imported dependent-constructor user adds one archived semantic
consumer outside CASES. Remaining archived providers are already members of the
same 114-file syntax cohort. Prototype drivers retain these paths; no archived
compiler implementation is built by the default accepted entrypoint.

Semantic compatibility retention is deliberate: old dotted spelling needs the
explicit flag; selected historical claims intentionally reject (if8 order,
missing named graph case, incompatible recursive property). Their comments at
compatibility.sh:110-127 explain the erroneous historical admission and current
constant-motive behavior. These are current rejection/regression contracts,
not historical passing behavior to recreate.

The five repeated CASES paths yield **20 repeated checker admissions** (1 outer
IH + 5 recursive field + 6 map + 3 eager insertion + 5 QuickSort). Their result
pairs differ and must stay. Recommend checking each identical source once and
preserving every result oracle; no rewrite is implemented. Keep the all-refuted
indexed-Match pending/image limitation (`cli.sh:154-167`), which is not a
successful-proof fixture. Keep QuickSort root/origin/imported properties and
dependent-provider nominal clients. Migration to current fixtures is proposed
only after preserving legacy parsing separately and the exact nominal/proof
interactions; no archived dependency is approved for default retirement here.

`tests/inventory.sh` itself is historical inventory generation, not part of
acceptance. The archive's old compiler/check harnesses are historical evidence;
do not add them to the current default build. Any C-internal/generated dependency
missed by this driver-based inventory remains in the declared dynamic limit.

## Progress and verification-debt extension

Fresh path-limited Git comparison `eb0aad6..4d1d941` is empty for accepted src
excluding prototype, tests, root Makefile and README. This says nothing about
unrelated edits in another worktree. Raw committed volume is docs +6484/-0 in
20 files and prototype +20003/-1374 in 285 files. Those prototype files mix
implementation, patches, tests, fixtures, reports and harnesses; their total is
not applied runtime code or language capability.

| Evidence / metric | Implementation and owner movement | Tests / docs / harness separately | Promotion / known failures / cost |
| --- | --- | --- | --- |
| Current assembled Program producer vs accepted baseline | Fresh applied textual C/header diff: +10418/-5685, 81 files; includes artifact modules, owner/head/Surface changes; no new owner architecture claim | Applied tests/fixtures/scripts +8112/-2463, 29 files; Makefile/build wiring separate | Prototype only; textual scale is not a performance metric |
| E3 report + central schedule | Reported runtime +26/-8; reconnects nine started definition-body links via existing scope/factory; does not restore child cursors/schedule | Reported tests +6/-3; original seed/assembly failures retained | Integrated `48364b0`; strict3 unchanged; no wall/RSS claim |
| C6 exact task commit `cea1dc0` | Lowering/private metadata +52/-16, four files; adds selected nested value-record capability downstream | Test/client/gate/fixtures +278/-4, six files; build +4/-0; docs/README +403/-19, five files; not proof of hundreds of new contracts | Integrated `53debc8`; accepted promotion absent; native Acc/QuickSort still unsupported |
| Existing common-producer census | Reported head cleanup reduces 4629 dispatches; Job E1 reduces 3753 copied result refs on QuickSort | 52 partition images / 40 verdict rows and recipe counts are evidence volume | Storage/traversal measurements, not peak RSS/time; Job/Evidence and #56 remain unfinished |
| This audit lane | No runtime, test, owner or accepted capability change | One prototype inventory generator/output, this report and one SOAP plan; outbox is transport | Static evidence only; cost and complete dynamic coverage open |

Applied textual counts use Python difflib SequenceMatcher, autojunk disabled,
comparing immediate accepted src C/headers with the assembled src tree (including
new artifact modules), and the accepted/assembled tests trees. They exclude
handmade, prototype backend source, build rules and this audit. C6 counts use
Git's exact commit delta. Neither metric is an estimate of unique obligations.

Historical failure classification, preserve original evidence:

| Class | Pinned evidence | Disposition |
| --- | --- | --- |
| Assembly/setup | joint_verification/broad-summary.json: omitted four training inputs; central plan: wrong eval.h, missing Surface/archive setup, E2 migration-column error | Separate failed attempt from corrected unchanged gate; not a newly reproduced compiler defect |
| Report generation/transport | Central plan: TSV newline normalization broke exact handoff hashes | Preserve byte failure; minimize copied epoch documents/patches, keep canonical source/report IDs |
| Sanitizer | performance_verification/results/sanitizers-original.json at `f3c3555`: 7/44 failed commands, readback scratch leak | Runtime lifetime defect; corrective 45-command report is separate historical evidence, not a waiver or this audit's fresh pass |
| Semantic strict resume | joint_verification/combined-partitions.tsv: reload 1000:1000, 1600:1600, 1915:0 fail; last has matching bytes but pending status | All three remain failed/unwaived; restricted checkpoint success does not prove public frontier correctness |
| New audit tooling/setup | Fresh assembly and two-path inventory reproduction | No failed compiler gate or interruption added by this audit |

These are examples with exact evidence, not an exhaustive period-wide failure
count. Reuse canonical epoch manifests/reports and the existing central table;
keep harness failures out of semantic pass counts. Reject six new permanent
ledgers, automatic 20% stop/delete rules and reuse based only on fixture/command
hashes. Keep narrow regression witnesses such as rejected E5's nominal identity
case; epoch-only producer freezes require promotion/retirement review rather than
automatic inclusion in accepted acceptance.

Cost remains **unmeasured**. A later reusable key must include actual qualified
binary and producer assembly, compiler/toolchain/flags/instrumentation, script
and C harness, every provider/generator/patch/input, working environment/options,
fuel/validation/trust/seed/root policy, and exclusive scheduling conditions.
Slot scope must distinguish builds, gate totals, per-obligation calls, runtime,
RSS and structural counts. A 384-recipe historical pass cannot supply cost or
qualify this freshly assembled producer. Merge/performance own that next stage.

## E6 cost coordination amendment (no timing grant)

After the static handoff, Merge requested command/dependency pins for a proposed
shared window at most 40 minutes: performance first, audit only after performance
reports every measurement child stopped. Merge reports qualified prototype Main
`01c29c0`; fresh read-only inspection verifies all 128 source.sha256 entries in
`/tmp/ap-performance-current-joint-898463e-b2d6668-20261003`, manifest digest
`c3ff50a9745bcca76809f68b7a435e2faeb1e20ea609469353522ef73bd509eb`.
Checker digest is `33d90f6fb02f0bd5cafe1e50f5cc64a5d057b2607e4c5f3aa21b81c97fe9bf8f`;
program_test is `d21c94c9e24eeb25eef14c8abe2a4e8c9d971f56c662d96684ce45873b87254f`.
No rebuild or gate execution occurred. E6's reported qualification is scoped
relevant gates plus parent E4 full acceptance; it does not claim repeated full
E6 acceptance. Its strict three failures remain unwaived.

The requested five-command set cannot run unchanged: prototype generic_sorted
argument 2 is the comparator executable, not retained mode. Passing `0` or `1`
would name a nonexistent comparator. **Four ordinary semantic-image gates are
applicable; accepted generic retained is not represented.** Do not substitute a
second ordinary run and label it retained coverage. All four scripts and their
complete direct input sets match the audited `4d1d941` assembly byte for byte;
the binaries/owner producer are E6 and are pinned separately.

[e6-cost-key.json](e6-cost-key.json), SHA-256
`a99a77531714dd51e5a75f842bf4be4170e562493e06bc078697ebfdc0513f45`,
pins exact commands, 7/25/3/13 script-and-input records by gate, generated sed/cat
recipes through script hashes, actual checker/comparator, source manifest,
qualification reports, reported O2 flags, current compiler identity, shell/helper
binaries and transitive runtime libraries, environment, policies and caps.
It is a requested cost key, not another permanent semantic ledger or a result.
Compiler identity is observed now; O2 flags come from qualification's recorded
build command. The checker ELF and source hashes identify the measured artifact.

Run only after Merge grants the slot, from this audit worktree, with
`PATH=/usr/bin:/bin LC_ALL=C LANG=C TMPDIR=/tmp` and BASH_ENV, ENV, LD_PRELOAD,
LD_LIBRARY_PATH, ASAN_OPTIONS and UBSAN_OPTIONS unset. Let `pilot_root` be exactly
the E6 directory above; the following fully expanded argv are also in the key:

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 120s /usr/bin/bash "$pilot_root/tests/quick_result.sh" "$pilot_root/build/pointer-check"
/usr/bin/timeout --signal=TERM --kill-after=5s 120s /usr/bin/bash "$pilot_root/tests/generic_sorted.sh" "$pilot_root/build/pointer-check" "$pilot_root/build/program_test"
/usr/bin/timeout --signal=TERM --kill-after=5s 60s /usr/bin/bash "$pilot_root/tests/retained_quicksort.sh" "$pilot_root/build/pointer-check"
/usr/bin/timeout --signal=TERM --kill-after=5s 150s /usr/bin/bash "$pilot_root/tests/local_strong_sorted.sh" "$pilot_root/build/pointer-check" "$pilot_root/build/program_test"
```

Agent cap proposal: 450 seconds of gate caps + at most 20 seconds kill grace +
10 seconds setup = eight minutes reserved for audit; performance would need to
finish within 32 minutes of a 40-minute shared window. These are administrative
caps, not runtime estimates. No retry/rebuild, image_cli or compatibility in this
phase. Stop at the shared deadline even if a per-gate cap remains; a timeout is
an incomplete censored cost observation, never a semantic pass. Record actual
exit and completed/omitted gates, then verify children stopped and input/binary
hashes unchanged. Use performance's agreed collector; no new measurement harness
is implemented. Full retained coverage, complete dynamic coverage and unmeasured
gate cost remain explicit if the reserved phase cannot fit. No timing grant yet.
