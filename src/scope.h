#ifndef A_PROGRAM_POINTER_SCOPE_H
#define A_PROGRAM_POINTER_SCOPE_H

#include "typing.h"

/* Selected declaration inputs over a raw Context. This is descriptive data,
 * not acceptance or proof history. NULL is the empty scope. Different Universe
 * bounds may describe the same raw Context without changing variable identity. */
struct pg_scope {
	struct pg_index_entry index;
	const struct pg_context *context;
	const struct pg_scope *parent, *indices;
	const struct pg_occurrence *type;
};

/* type is the ordinary declared type, or the terminal Universe under indices.
 * Check local graph shape only; the kernel checks all formations separately. */
const struct pg_scope *pg_scope_intern(struct pg_typing *typing,
	const struct pg_context *context, const struct pg_scope *parent,
	const struct pg_scope *indices, const struct pg_occurrence *type);

#endif
