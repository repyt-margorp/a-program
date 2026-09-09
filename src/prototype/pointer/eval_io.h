#ifndef A_PROGRAM_POINTER_EVAL_IO_H
#define A_PROGRAM_POINTER_EVAL_IO_H

#include "eval.h"
#include "graph_io.h"

/* A configuration fragment, not a complete evaluator checkpoint. No status,
 * policy, work cursor, continuation or accepted reduction is transported. */
struct pg_eval_configuration {
	struct pg_closure head;
	const struct pg_argument *arguments;
};
/* Preserve shared environment/argument links using the ordinary Term codec.
 * Reading is inert: it neither evaluates nor issues normalization evidence.
 * Source links are borrowed; the destination graph must be initialized.
 * All restored storage belongs to graph. On failure
 * outputs are empty, though graph may contain unused allocations. limit bounds
 * configuration roots, link records and the embedded graph's record counts. */
int pg_eval_configurations_write(FILE *file, size_t count,
	const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner);
int pg_eval_configurations_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_eval_configuration **roots);

#endif
