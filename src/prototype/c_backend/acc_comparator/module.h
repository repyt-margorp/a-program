#ifndef __ACTUAL_ACC_COMPARATOR_PRODUCT_H__
#define __ACTUAL_ACC_COMPARATOR_PRODUCT_H__

#include <stddef.h>
#include <stdint.h>

struct qs_trace {
	size_t raw_down_step, raw_down_weaken, raw_down_lift, folded_down;
	size_t partition_lower, partition_upper, acc_branch, index_checks;
};

/* The original fixed-candidate entry remains available. Borrowed arrays and
	* separate output/written/optional trace stay valid through the call.
	* Status1 pointer,2 metadata,3 allocation/node65536,4 depth256,
	* 5 Nat32 overflow,6 capacity/count. Failure preserves buffer/written. */
int gs_sort(const uint32_t *, size_t, uint32_t *, size_t, size_t *, struct qs_trace *);

/* Borrowed synchronous interpretation of the admitted pure-total comparator.
	* Return1 for source Bool.true,0 for Bool.false; other codes fail metadata2.
	* Code/context and everything they read must stay valid and immutable through
	* gs_sort_with. The descriptor is copied; ownership is never transferred.
	* No ordering law or source proof follows from machine Bool values. */
struct gs_comparator {
	int (*call)(const void *, uint32_t, uint32_t);
	const void *context;
};

/* Actual source Acc/down recursion retains this comparator in both folded
	* recursive branches. Output is transactional; foreign effects are not rolled
	* back. Caller implements the source function and its pure-total contract. */
int gs_sort_with(const struct gs_comparator *, const uint32_t *, size_t,
	uint32_t *, size_t, size_t *, struct qs_trace *);

#endif
