#ifndef A_PROGRAM_POINTER_EXECUTION_H
#define A_PROGRAM_POINTER_EXECUTION_H

#include "evidence.h"
#include <stdio.h>

enum pg_execution_status { PG_EXECUTION_PENDING, PG_EXECUTION_DONE,
	PG_EXECUTION_UNHANDLED, PG_EXECUTION_STUCK, PG_EXECUTION_IO_ERROR,
	PG_EXECUTION_ERROR };

/* A fresh invocation, not a pure normalization receipt. Do not copy/move this
 * object while running: evaluator callbacks recover their enclosing owner.
 * Typing/graph and output must outlive it. Destroy does not close output.
 * The result is a materialized value, not accepted typing/conversion evidence.
 * No runtime checkpoint or effect receipt is stored in a Program image. */
struct pg_execution {
	struct pg_eval machine;
	enum pg_execution_status status;
	FILE *output;
	const struct pg_term *result;
	uint64_t steps;
};

/* Only a locally accepted, closed returning computation may enter. No effects
 * or evaluation during initialization; destroy is valid even after failure. */
int pg_execution_init(struct pg_execution *execution, struct pg_typing *typing,
	const struct pg_evidence *computation, FILE *output);
/* Counts evaluator and root-dispatch transitions, not wall time or bytes.
 * Each print writes/flushes exact Text bytes, returns the same Text, and is
 * never automatically retried after I/O failure (which can be partial). */
enum pg_execution_status pg_execution_advance(struct pg_execution *execution,
	uint64_t budget);
void pg_execution_destroy(struct pg_execution *execution);

#endif
