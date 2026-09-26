#ifndef A_PROGRAM_POINTER_OCCURRENCE_IO_H
#define A_PROGRAM_POINTER_OCCURRENCE_IO_H

#include "typing.h"
#include <stdio.h>

/* APGOCC7 transports descriptive typed inputs, not evidence. Contexts, Core,
 * classifiers and annotations share relocation tables. Sorts and operand order
 * and structural maps are retained. Derived origins are distinct from direct
 * construction inputs. Selected construction maps retain their source scopes
 * and typed images. Input selection retains its ordinal and optional argument;
 * neither a selected result nor a lifted child view certifies a judgement.
 * Recursive elimination retains its binder allocation and
 * clause scopes through the same relocation tables. Earlier images reject.
 * Descriptor naming/resolution obeys graph_io.h's owner contract. */
int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
/* Initialized typing required. limit bounds nodes, roots, operand edges and
 * selected maps and induction inputs here, and each nested section independently. Failed reads publish no
 * roots but may allocate unused arena data. No evidence is constructed. */
int pg_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_occurrence *const **roots);

#endif
