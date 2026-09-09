#ifndef A_PROGRAM_POINTER_SYMMETRY_INTERNAL_H
#define A_PROGRAM_POINTER_SYMMETRY_INTERNAL_H

#include "symmetry.h"

struct symmetry_entry {
	struct pg_object_entry base;
	size_t dimension;
	const size_t *axes;
	size_t fixed_prefix;
};

struct composition_work {
	const struct symmetry_entry *outer, *inner;
	const struct pg_term *argument;
	size_t *axes;
	size_t position;
	size_t dimension;
};

const struct symmetry_entry *pg_symmetry_owner(const struct pg_object *object);
extern const struct pg_eval_work_operation pg_symmetry_composition_operation;

#endif
