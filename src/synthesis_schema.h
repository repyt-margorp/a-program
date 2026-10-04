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
struct constructor_callable {
	const struct source_constructor *source;
	size_t field, count;
	/* NULL borrows the declaration's complete convention in source order. */
	size_t *indices;
};

const struct source_constructor *pg_synthesis_schema_members(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_source_declaration(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
const struct pg_source_scope *pg_synthesis_declaration_exports(const struct pg_synthesis_job *);
const struct source_constructor *pg_synthesis_declaration_members(const struct pg_synthesis_job *);
const struct pg_data_declaration *pg_synthesis_declaration_allocation(const struct pg_synthesis_job *);
const struct pg_data_declaration *pg_synthesis_schema_allocation(const struct pg_synthesis_job *);
const struct source_constructor *pg_synthesis_constructor_source(const struct pg_synthesis *,
	const struct pg_evidence *, const struct pg_object *);
struct constructor_callable pg_synthesis_constructor_callable(const struct pg_synthesis *, const struct pg_synthesis_job *);
struct pg_synthesis_input pg_synthesis_constructor_fields(const struct pg_synthesis_job *);

#endif
