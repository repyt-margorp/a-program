#ifndef A_PROGRAM_PROTOTYPE_CHECKER_MODULE_SET_INTERNAL_H
#define A_PROGRAM_PROTOTYPE_CHECKER_MODULE_SET_INTERNAL_H

#include "a_program/checker/module_set.h"

struct prototype_canonical_checked_image {
	const struct prototype_checked_module* module;
	unsigned char* bytes;
	size_t count;
	size_t input_index;
};

/* Prepare canonical images without checking export conflicts. This is the
 * resumable merge producer's immutable input. */
int prototype_checked_module_set_prepare(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	struct prototype_checked_module_set** p_set
);

/* Adopt already checked canonical images, as used by capsule restoration.
 * Ownership of every byte buffer transfers on success. */
int prototype_checked_module_set_adopt(
	struct prototype_canonical_checked_image* images,
	size_t image_count,
	struct prototype_checked_module_set** p_set
);


void prototype_checked_module_image_serialization_count_reset(void);
size_t prototype_checked_module_image_serialization_count(void);

#endif
