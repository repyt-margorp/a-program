#ifndef A_PROGRAM_PROTOTYPE_DIMENSION_OPERATOR_H
#define A_PROGRAM_PROTOTYPE_DIMENSION_OPERATOR_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/dimension/types.h"

void prototype_dimension_operator_db_init(
	struct prototype_dimension_operator_db* db,
	struct prototype_dimension_operator* operators,
	size_t operator_capacity,
	struct prototype_dimension_axis_image* images,
	size_t image_capacity
);

void prototype_dimension_operator_db_clear(
	struct prototype_dimension_operator_db* db
);

int prototype_dimension_operator_db_rebuild_index(
	struct prototype_dimension_operator_db* db
);

const struct prototype_dimension_operator* prototype_dimension_operator_get(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
);

const struct prototype_dimension_axis_image*
prototype_dimension_operator_images(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
);

int prototype_dimension_operator_find(
	const struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count,
	uint32_t* p_operator_id
);

int prototype_dimension_operator_validate(
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count
);

int prototype_dimension_operator_intern(
	struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count,
	uint32_t* p_operator_id
);

int prototype_dimension_operator_identity(
	struct prototype_dimension_operator_db* db,
	uint32_t dimension,
	uint32_t* p_operator_id
);

int prototype_dimension_operator_extension(
	struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t* p_operator_id
);

/* Return one exactly for the canonical dimension-preserving extension
 * [0..n) -> [0..n+1). Zero-dimensional e_0 is canonical; callers that require
 * a repeated action must separately require a nonzero source dimension. */
int prototype_dimension_operator_is_canonical_extension(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
);

int prototype_dimension_operator_compose(
	struct prototype_dimension_operator_db* db,
	uint32_t first_operator_id,
	uint32_t second_operator_id,
	struct prototype_dimension_axis_image* scratch_images,
	size_t scratch_capacity,
	uint32_t* p_operator_id
);

struct prototype_dimension_operator_mark prototype_dimension_operator_mark(
	const struct prototype_dimension_operator_db* db
);

int prototype_dimension_operator_rollback(
	struct prototype_dimension_operator_db* db,
	struct prototype_dimension_operator_mark mark
);

#endif
