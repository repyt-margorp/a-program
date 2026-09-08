#ifndef A_PROGRAM_POINTER_OCCURRENCE_IO_H
#define A_PROGRAM_POINTER_OCCURRENCE_IO_H

#include "typing.h"
#include <stdio.h>

/* Transport elaboration inputs, not evidence. Contexts, annotations and Core
 * share the nested context/Core relocation tables. Operands remain ordered.
 * Descriptor naming/resolution obeys graph_io.h's owner contract. */
int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
/* Initialized typing required. limit bounds nodes, roots and operand edges in
 * this section, and each nested section independently. Failed reads publish no
 * roots but may allocate unused arena data. No evidence is constructed. */
int pg_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_occurrence *const **roots);

#endif
