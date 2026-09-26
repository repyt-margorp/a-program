#ifndef A_PROGRAM_POINTER_SYNTHESIS_HANDLER_H
#define A_PROGRAM_POINTER_SYNTHESIS_HANDLER_H

#include "synthesis.h"

/* Internal source-owner views. The public Handler API stays in synthesis.h. */
int pg_synthesis_handler_syntax(const struct pg_syntax *);
int pg_synthesis_return_clause(const struct pg_syntax *);
struct pg_synthesis_job *pg_synthesis_return_handler(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
const struct pg_source_scope *pg_synthesis_handler_environment(const struct pg_synthesis_job *);
int pg_synthesis_handler_environment_input(const struct pg_source_scope *, struct pg_source_environment *);

#endif
