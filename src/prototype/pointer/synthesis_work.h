#ifndef A_PROGRAM_POINTER_SYNTHESIS_WORK_H
#define A_PROGRAM_POINTER_SYNTHESIS_WORK_H

#include "synthesis.h"

/* Internal, statically owned request behavior. Descriptor identity and exact
 * inputs form the key in synthesis->jobs; descriptors are never serialized.
 * Private state is zero-initialized once, on an interning miss. The arena owns
 * it; destroy releases only separately allocated resources. */
struct pg_synthesis_work_class {
	size_t size;
	void (*advance)(struct pg_synthesis *, struct pg_synthesis_job *);
	void (*destroy)(struct pg_synthesis_job *);
	void (*completed)(struct pg_synthesis *, struct pg_synthesis_job *, int first);
	/* Optional publication of provisional structure, before acceptance. */
	int (*preparing)(const struct pg_synthesis_job *);
};

struct pg_synthesis_job *pg_synthesis_work_request(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *kind, size_t count, const void *const *inputs);
void *pg_synthesis_work_state(const struct pg_synthesis_job *job,
	const struct pg_synthesis_work_class *kind);
const void *pg_synthesis_work_input(const struct pg_synthesis_job *job, size_t index);
size_t pg_synthesis_work_input_count(const struct pg_synthesis_job *job);
void pg_synthesis_enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job);
void pg_synthesis_finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status);
/* Zero means done; otherwise subscribe once or propagate the failure. */
int pg_synthesis_await(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *dependency);

#endif
