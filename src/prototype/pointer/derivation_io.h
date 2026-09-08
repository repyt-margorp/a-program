#ifndef A_PROGRAM_POINTER_DERIVATION_IO_H
#define A_PROGRAM_POINTER_DERIVATION_IO_H

#include "derivation.h"
#include <stdio.h>

/* Unaccepted rule application. source/target are comparison endpoints for
 * conversion, directed endpoints for normalization, otherwise NULL. They are
 * obligations, not receipts. The parameter certificate pointers remain NULL.
 * The ordinary constructors compute the conclusion after checking premises. */
struct pg_derivation_input {
	enum pg_evidence_rule rule;
	struct pg_derivation_parameters parameters;
	const struct pg_term *source, *target;
	enum pg_reduction_kind reduction_kind;
	size_t count;
	const struct pg_derivation_input *premises[];
};
int pg_derivations_write(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
/* Restores inputs only, with one Core relocation table and shared premise DAG.
 * Limits bound records/edges here and records in the nested Core separately.
 * No proof index is accessed, and no evaluation takes place. Outputs publish
 * only on success. Descriptor contracts are those of graph_io.h. */
int pg_derivations_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);

#endif
