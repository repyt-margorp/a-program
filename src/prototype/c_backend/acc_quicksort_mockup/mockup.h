#ifndef __ACC_QUICKSORT_MOCKUP_H__
#define __ACC_QUICKSORT_MOCKUP_H__

#include <stddef.h>
#include <stdint.h>

/* Source-specific closed Nat/LT mockup, not an automatic backend profile.
	* All proof indices and callable recursive fields remain explicit.
	* Hand-authored transcription of the admitted source, not generated success.
	* Executable qualification is reported separately with its exact inputs. */
struct qs_arena;
struct qs_nat_type;
struct qs_lt_relation;
struct qs_list;
struct qs_sized_list;
struct qs_acc;

enum qs_lt_tag { QS_LT_STEP, QS_LT_WEAKEN_RIGHT, QS_LT_LIFT };
struct qs_lt {
	enum qs_lt_tag tag;
	uint32_t left, right;
	union {
		uint32_t step;
		struct { uint32_t m, n; const struct qs_lt *prior; } weaken_right;
		struct { uint32_t m, n; const struct qs_lt *prior; } lift;
	} fields;
};

/* Acc Nat LT subject has constructor acc(current, raw_down).
	* Calling the original field returns Acc at the requested smaller index. */
struct qs_acc_down {
	const struct qs_acc *(*call)(struct qs_arena *, const void *, uint32_t, const struct qs_lt *);
	const void *context;
};
struct qs_acc {
	const struct qs_nat_type *domain;
	const struct qs_lt_relation *relation;
	uint32_t subject, current;
	struct qs_acc_down down;
};

struct qs_compare {
	int (*call)(struct qs_arena *, const void *, uint32_t, uint32_t);
	const void *context;
};

/* A Fold over Acc returns a function SizedList Nat current -> List Nat.
	* The recursive field supplies such functions; forcing/applying *down is
	* distinct from calling Acc's original down to obtain a smaller Acc. */
struct qs_sort_closure {
	const struct qs_nat_type *element_type;
	struct qs_compare comparison;
	const struct qs_acc *access;
	uint32_t input_index;
};
struct qs_folded_down {
	const struct qs_nat_type *element_type;
	struct qs_compare comparison;
	struct qs_acc_down original;
	uint32_t parent_index;
};

struct qs_partition {
	const struct qs_nat_type *element_type;
	uint32_t bound;
	uint32_t lower_size;
	const struct qs_sized_list *lower;
	uint32_t upper_size;
	const struct qs_sized_list *upper;
	const struct qs_lt *lower_bound, *upper_bound;
};

/* Source-definition correspondence; implementations must retain these calls. */
const struct qs_acc *qs_accessible_succ(struct qs_arena *, uint32_t, const struct qs_acc *);
const struct qs_acc *qs_nat_accessible(struct qs_arena *, uint32_t);
struct qs_sort_closure qs_quick_sort_acc(const struct qs_nat_type *, struct qs_compare, uint32_t, const struct qs_acc *);
struct qs_sort_closure qs_force_down(struct qs_arena *, const struct qs_folded_down *, uint32_t, const struct qs_lt *);
const struct qs_list *qs_apply_sort(struct qs_arena *, const struct qs_sort_closure *, const struct qs_sized_list *);
const struct qs_partition *qs_partition(struct qs_arena *, const struct qs_nat_type *, struct qs_compare, uint32_t, uint32_t, const struct qs_sized_list *);

struct qs_trace {
	size_t raw_down_step, raw_down_weaken, raw_down_lift, folded_down;
	size_t partition_lower, partition_upper, acc_branch, index_checks;
};

/* Private mockup call: borrowed Nat32 arrays; separate buffer/written/trace.
	* Source indices and proofs are allocated, used and freed within this call.
	* Status1 pointer,2 internal index/tag,3 allocation/node bound,4 depth256,
	* 5 Nat32 overflow,6 capacity/count. Failure preserves buffer/written.
	* This is not ap_export ABI, an erasure policy, or a new Linker profile. */
int qs_mockup_sort(const uint32_t *, size_t, uint32_t *, size_t, size_t *, struct qs_trace *);

#endif
