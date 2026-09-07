#ifndef A_PROGRAM_POINTER_IADT_H
#define A_PROGRAM_POINTER_IADT_H

#include "eval.h"

struct pg_data_layout;
struct pg_match_clause {
	const struct pg_object *constructor;
	const struct pg_term *branch;
};

/* Erased, immutable constructor layout, not an accepted type declaration.
 * Arity is runtime layout information, not a positivity/index/type schema.
 * Each creation has fresh identity; descriptors live in graph. */
const struct pg_data_layout *pg_data_layout(struct pg_graph *graph,
	size_t count, const size_t *arities);
const struct pg_object *pg_data_constructor(const struct pg_data_layout *layout, size_t index);
const struct pg_object *pg_data_matcher(const struct pg_data_layout *layout);
/* Complete pointer-labelled clauses, in any order. Branches are ordinary
 * lambda terms over erased fields. Core operands retain every branch, so
 * substitution/readback needs no traversal into opaque descriptor payloads. */
const struct pg_term *pg_data_match(struct pg_graph *graph, const struct pg_data_layout *layout,
	const struct pg_term *scrutinee, size_t count, const struct pg_match_clause *clauses);
int pg_data_dispatch(struct pg_eval *machine);

#endif
