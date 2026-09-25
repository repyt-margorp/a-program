#ifndef A_PROGRAM_POINTER_SYNTHESIS_WORK_H
#define A_PROGRAM_POINTER_SYNTHESIS_WORK_H

#include "synthesis.h"

struct waiter;
/* Shared scheduling and identity only. Domain state is private to its owner;
 * the result is a borrowed checked value, never a second proof representation. */
struct pg_synthesis_job {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_synthesis_work_class *role;
	size_t input_count;
	enum pg_synthesis_status status;
	struct pg_synthesis_job *next;
	struct waiter *waiters;
	struct waiter *dependency;
	const struct pg_evidence *result;
	const void *inputs[];
};

/* Internal, statically owned request behavior. Descriptor identity and exact
 * inputs form the key in synthesis->jobs; descriptors are never serialized.
 * Private state precedes the header in one aligned arena allocation and is
 * zero-initialized once, on an interning miss. The arena owns it; destroy
 * releases only separately allocated resources. */
struct pg_synthesis_work_class {
	size_t size;
	/* Called once after interning. NULL schedules the initial step directly. */
	void (*start)(struct pg_synthesis *, struct pg_synthesis_job *);
	void (*advance)(struct pg_synthesis *, struct pg_synthesis_job *);
	void (*destroy)(struct pg_synthesis_job *);
	void (*completed)(struct pg_synthesis *, struct pg_synthesis_job *, int first);
	/* Optional publication of provisional structure, before acceptance. */
	int (*preparing)(const struct pg_synthesis_job *);
};

struct pg_synthesis_job *pg_synthesis_work_request(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *kind, size_t count, const void *const *inputs);
struct pg_synthesis_job *pg_synthesis_work_request_inputs(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *kind, size_t prefix, const void *const *inputs,
	size_t count, struct pg_synthesis_job *const *jobs);
void pg_synthesis_work_destroy(struct pg_synthesis *synthesis);
void *pg_synthesis_work_state(const struct pg_synthesis_job *job,
	const struct pg_synthesis_work_class *kind);
const void *pg_synthesis_work_input(const struct pg_synthesis_job *job, size_t index);
size_t pg_synthesis_work_input_count(const struct pg_synthesis_job *job);
void pg_synthesis_enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job);
void pg_synthesis_finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status);
void pg_synthesis_subscribe(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child, int preparation);
/* Zero means done; otherwise subscribe once or propagate the failure. */
int pg_synthesis_await(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *dependency);

#endif
