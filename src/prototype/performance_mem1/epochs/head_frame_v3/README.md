# MEM1 v3: Captured-Head Frame Recycling

This separate corrective epoch keeps the published v2 bytes intact. A later
public materialized callback destroys its temporary arena and returns an error:
the original parent passes, while v2 retirement reads a freed frame. The exact
ASan failure remains in the named boundary evidence. V1's independent inline
callback-state corruption also remains rejected.

V3 deletes retirement from materialized callbacks and return2 fallback. It
recycles only stateless captured-head frames after their existing post-callback
answer cleanup. Non-NULL callback state retains the original frame lifetime.
There is no new callback restriction, policy flag, checking authority or owner
implementation. Active frames and required continuation progress remain intact.

The qualification parent is frozen E9+E10 source128 `73fa86c9`; this epoch excludes
Job E11/E12 and later live owner work. Relevant O2, all seven checkpoints,
ASan/UBSan/leaks, raw all-cut persistence and the three callback units pass.
The original three strict reload failures remain. This is not a fresh full O2
acceptance run; v2's 384 passing recipes remain separately reported history.

At the same 180M charged-step tree cut, sampled machine capacity is
1,685,012,480 to 812,318,720 bytes. All sampled shared owner/index counters agree.
The full tree remains 183,507,626 steps, with 11,363,962 fewer external frame
requests and 2,727,318,176 fewer cumulative aligned requested bytes after the
32,704-byte aligned WHNF-job overhead. Small list allocation grows 368 bytes.
Selected-cut capacity and cumulative requests are not peak RSS or timing.

`applied.patch` is the minimal v2-to-v3 correction. `eval.c.patch` and
`eval.h.patch` apply to the qualification parent; the combined parent patch is
also included. Run the three frozen tests with `verify.mk`, an explicit private
`OVERLAY`, and `VERIFICATION_MAKEFILE` pointing to the performance build rules.
Exact source, test and evidence hashes are in the adjacent manifests/reports.
Merge owns task publication and integration; this worker makes no Main,
accepted-promotion, actual cost or Goal-completion claim.
