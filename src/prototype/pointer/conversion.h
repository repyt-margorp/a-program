#ifndef A_PROGRAM_POINTER_CONVERSION_H
#define A_PROGRAM_POINTER_CONVERSION_H

#include "eval.h"

enum pg_conversion_status {
	PG_CONVERSION_PENDING,
	PG_CONVERSION_EQUAL,
	PG_CONVERSION_DIFFERENT,
	PG_CONVERSION_ERROR
};

struct pg_conversion_state;
struct pg_conversion_certificate;
/* Fixed kernel-pure conversion: beta, structural CBPV and the implemented
 * Identity/structural CBPV eta equations. Compare finite binding structure
 * before reducing: alpha-equal recursive code needs no unfolding. This does
 * not change pointer interning. A failed WHNF comparison falls back
 * to strong normalization using the same pure evaluator and shared jobs.
 * No runtime handler override or equality reflection. work outlives comparisons.
 * Fuel counts transitions, not allocator work; lack of a normal form may keep
 * comparison pending. This is not completeness for general higher Identity. */
struct pg_conversion {
	struct pg_conversion_state *state;
};
int pg_conversion_init(struct pg_conversion *conversion, struct pg_whnf_work *work,
	const struct pg_term *left, const struct pg_term *right);
enum pg_conversion_status pg_conversion_advance(struct pg_conversion *conversion, uint64_t budget);
void pg_conversion_destroy(struct pg_conversion *conversion);
enum pg_conversion_status pg_conversion_status(const struct pg_conversion *conversion);
uint64_t pg_conversion_steps(const struct pg_conversion *conversion);
size_t pg_conversion_task_count(const struct pg_conversion *conversion);
/* Immutable successful comparison evidence, owned by work->graph rather than
 * the temporary comparison. Pending/different/error comparisons return NULL. */
const struct pg_conversion_certificate *pg_conversion_certificate(const struct pg_conversion *conversion);
const struct pg_term *pg_conversion_left(const struct pg_conversion_certificate *certificate);
const struct pg_term *pg_conversion_right(const struct pg_conversion_certificate *certificate);

#endif
