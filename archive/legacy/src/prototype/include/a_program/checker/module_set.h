#ifndef A_PROGRAM_PROTOTYPE_CHECKER_MODULE_SET_H
#define A_PROGRAM_PROTOTYPE_CHECKER_MODULE_SET_H

#include <stddef.h>

#include "a_program/checker/session.h"

enum prototype_checked_module_set_status {
	PROTOTYPE_CHECKED_MODULE_SET_COMPLETE = 1,
	PROTOTYPE_CHECKED_MODULE_SET_MALFORMED = 2,
	PROTOTYPE_CHECKED_MODULE_SET_CONFLICT = 3
};

struct prototype_checked_module_set;

struct prototype_checked_module_set_report {
	int status;
	int export_kind;
	size_t left_module;
	size_t right_module;
};

/* Compose independently checked modules without flattening their local
 * semantic arenas. Equal canonical modules are deduplicated; unequal modules
 * that publish the same qualified export are rejected. The set borrows every
 * module capability for its lifetime. */
int prototype_checked_module_set_create(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	struct prototype_checked_module_set** p_set,
	struct prototype_checked_module_set_report* p_report
);

void prototype_checked_module_set_destroy(
	struct prototype_checked_module_set* set
);

size_t prototype_checked_module_set_count(
	const struct prototype_checked_module_set* set
);

const struct prototype_checked_module* prototype_checked_module_set_at(
	const struct prototype_checked_module_set* set,
	size_t index
);

/* Borrow the canonical checked artifact image used to order and deduplicate a
 * module set. The bytes remain owned by the set and carry no checked authority
 * by themselves. */
int prototype_checked_module_set_canonical_image_at(
	const struct prototype_checked_module_set* set,
	size_t index,
	const unsigned char** p_bytes,
	size_t* p_count
);

/* Returns zero for disjoint interfaces, one for an export conflict, and minus
 * one for malformed capabilities. */
int prototype_checked_modules_conflict(
	const struct prototype_checked_module* left,
	const struct prototype_checked_module* right,
	int* p_export_kind
);

#endif
