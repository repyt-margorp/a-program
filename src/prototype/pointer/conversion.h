#ifndef A_PROGRAM_POINTER_CONVERSION_H
#define A_PROGRAM_POINTER_CONVERSION_H

#include "eval.h"

enum pg_conversion_status {
	PG_CONVERSION_PENDING,
	PG_CONVERSION_EQUAL,
	PG_CONVERSION_DIFFERENT,
	PG_CONVERSION_ERROR
};

struct pg_conversion_task;
/* Beta conversion only: no eta, semantic dispatch, or equality reflection.
 * work must outlive this comparison. Budget counts traversal/WHNF transitions;
 * readback allocation is not yet separately budgeted. */
struct pg_conversion {
	struct pg_beta_work *work;
	struct pg_graph storage;
	struct pg_index visited;
	struct pg_conversion_task *pending;
	enum pg_conversion_status status;
	uint64_t steps;
};
int pg_conversion_init(struct pg_conversion *conversion, struct pg_beta_work *work,
	const struct pg_term *left, const struct pg_term *right);
enum pg_conversion_status pg_conversion_advance(struct pg_conversion *conversion, uint64_t budget);
void pg_conversion_destroy(struct pg_conversion *conversion);

#endif
