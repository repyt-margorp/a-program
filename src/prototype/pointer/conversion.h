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
/* Beta conversion only: no eta, semantic dispatch, or equality reflection.
 * work must outlive this comparison. Budget counts traversal/WHNF transitions;
 * readback allocation is not yet separately budgeted. */
struct pg_conversion {
	struct pg_conversion_state *state;
};
int pg_conversion_init(struct pg_conversion *conversion, struct pg_beta_work *work,
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
