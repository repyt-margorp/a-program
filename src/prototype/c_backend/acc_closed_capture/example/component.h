#ifndef __ACTUAL_ACC_PRODUCT_H__
#define __ACTUAL_ACC_PRODUCT_H__

#include <stddef.h>
#include <stdint.h>

struct qs_trace {
	size_t raw_down_step, raw_down_weaken, raw_down_lift, folded_down;
	size_t partition_lower, partition_upper, acc_branch, index_checks;
};

/* Existing private candidate signature, not ap_export/native ABI.
	* Borrowed Nat32 input and separate buffer/written/optional trace must stay
	* valid through the call. All source proof/capture storage is freed on return.
	* Status1 pointer,2 metadata,3 allocation/node65536,4 depth256,
	* 5 Nat32 overflow,6 capacity/count. Failure preserves buffer/written.
	* Source-generated expressions use explicitly labeled manual target actions. */
int gs_sort(const uint32_t *, size_t, uint32_t *, size_t, size_t *, struct qs_trace *);

#endif
