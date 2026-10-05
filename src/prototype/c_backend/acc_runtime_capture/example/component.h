#ifndef __ACTUAL_ACC_RUNTIME_PRODUCT_H__
#define __ACTUAL_ACC_RUNTIME_PRODUCT_H__

#include <stddef.h>
#include <stdint.h>

struct qs_trace {
	size_t raw_down_step, raw_down_weaken, raw_down_lift, folded_down;
	size_t partition_lower, partition_upper, acc_branch, index_checks;
};

/* Explicit private candidate ABI: c_acc_runtime_bool_candidate_v1.
	* These values are the admitted Bool constructor positions in this profile,
	* not C truth values. Both source clauses are kept in the generated module.
	* The context borrows this argument only during the synchronous call.
	* Status1 pointer,2 invalid mode/metadata,3 allocation/node65536,4 depth256,
	* 5 Nat32 overflow,6 capacity/count. Failure preserves buffer/written.
	* Input/output storage must remain valid and nonoverlapping through the call. */
enum gs_bool_mode { GS_BOOL_FIRST=0, GS_BOOL_SECOND=1 };
int gs_sort_mode(enum gs_bool_mode, const uint32_t *, size_t,
	uint32_t *, size_t, size_t *, struct qs_trace *);

#endif
