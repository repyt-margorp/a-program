#ifndef A_PROGRAM_POINTER_PENDING_H
#define A_PROGRAM_POINTER_PENDING_H

#include "graph.h"

struct pg_evidence;
struct pg_synthesis_job;
struct pg_typed_query;
struct pg_pending;
struct pg_pending_ops {
	struct pg_synthesis_job *(*job)(struct pg_pending *);
	struct pg_typed_query *(*query)(struct pg_pending *);
	const struct pg_evidence *(*result)(const struct pg_pending *);
};
struct pg_pending_class {
	const struct pg_pending_ops *ops;
};
/* Existing request identity prefix, embedded once in the actual work owner.
 * It owns no cursor, completion flag or result. Class descriptors are static. */
struct pg_pending {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_pending_class *role;
};

struct pg_synthesis_job *pg_pending_job(struct pg_pending *);
struct pg_typed_query *pg_pending_query(struct pg_pending *);
const struct pg_evidence *pg_pending_result(const struct pg_pending *);

#endif
