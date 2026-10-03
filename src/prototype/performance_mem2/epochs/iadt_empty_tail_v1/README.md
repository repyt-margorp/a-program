# Direct IADT Field-Spine Copy Deletion

Date: 2026-10-03. Prototype only; full performance Goal remains active.

`iadt.patch` changes only `iadt.c` on the qualified current E12 plus MEM1 v3
producer. When a validated captured constructor selects a branch with no
trailing caller arguments, reuse the existing immutable field spine. Its empty
tail, field order, terms and lexical environments already match the required
arguments. Keep the existing copies when a trailing caller tail must be appended.
No node is mutated, pooled or freed, and no owner/index/ABI/schema changes.

Live field nodes belong to the evaluator arena. Inert frame decoding places
configuration environments/arguments in the output graph, whose existing
contract outlives the machine. Frame head cleanup destroys separate readback
entries; frame retirement does not release either argument forest. Preserve
captured closures and opaque callback state. This differs from the rejected
beta-environment deletion in `performance_mem1/epochs/constant_beta_rejected_20261003`,
whose six semantic result/fuel failures remain frozen and must not be ingested.

Parent source128: `0deb36a72542b9f56eb986314c89bd459802896ca32268857a4b875bfcfe2f48`.
Candidate source128: `2e87fd8444726e9bacc9cb4e7b4e17f814685a515f716b212a849ca1d24ac85e`.
Patch: `b19dd27bef9eca23dd6b78185aaa26569e9ede25a7be1bc039da49e8444f71f9`.
The corrected E11/E12, family, Surface, head/cleanup and descriptor-only adapters
stay exact; no live E13 or competing Job implementation. Main integration and
accepted promotion remain Root-owned; current E12 qualification is private.

Relevant gates are terminal: artifact55 fails only the unchanged strict recipe;
SAN build38/checks45, focused O2 checks23 and cross-build fresh reads140 pass.
Five additional callback/direct units pass in both O2 and ASan/UBSan, including
reentrant/inline-state, error cleanup and return2 fallback. Direct17/16/10/8
steps and every55 cut match the parent. All52 public images,28 history images
and the full public verdict/fuel TSV match. Original strict3 and all original
observer/seven-sanitizer failures remain visible. No new full384-suite claim.

Existing allocation census runs the same twelve source/proof/helper workloads
without timing/RSS collection. Every DONE/fuel count agrees. Tree400 removes
2408960 external arena requests and77086720 cumulative aligned bytes; tree4/16/64
remove30496/100224/390976 requests. List is unchanged. Imported LocalSorted
removes1053 requests and47264 bytes. These are cumulative external counts,
not actual peak/live RAM or speed; internal graph.c allocations are omitted.
The initial reporting assumption that every delta is32 bytes per request fails
for LocalSorted and remains separately preserved. Do not claim per-site
attribution for its additional13568-byte difference. No new index/owner overhead.

Exact commands/logs/source/test hashes are in the private frozen evidence linked
by `evidence.json`. `summary.json` contains audited terminal results; the
original allocator reporting failure and rejected environment experiment remain
separate. Any actual matched cost comparison requires a new exclusive grant.
Chosen task message: `prototype: reuse direct IADT spines with empty caller tail`.
