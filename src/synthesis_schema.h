#ifndef A_PROGRAM_POINTER_SYNTHESIS_SCHEMA_H
#define A_PROGRAM_POINTER_SYNTHESIS_SCHEMA_H

#include "synthesis_source.h"

/* Immutable source calling convention after schema preparation. These paths
 * insert recoverable index arguments; they are not kernel typing evidence. */
struct constructor_index_path {
	const struct pg_object *binder;
	size_t field, index;
};
struct source_constructor {
	const struct pg_syntax *telescope;
	struct pg_synthesis_job *producer;
	size_t implicit_count;
	const struct constructor_index_path *indices;
};

const struct source_constructor *pg_synthesis_schema_members(const struct pg_synthesis_job *);
const struct pg_data_declaration *pg_synthesis_schema_allocation(const struct pg_synthesis_job *);

#endif
