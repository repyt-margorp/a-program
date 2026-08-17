#ifndef A_PROGRAM_PROTOTYPE_DIMENSION_FACE_H
#define A_PROGRAM_PROTOTYPE_DIMENSION_FACE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/dimension/types.h"

int prototype_dimension_face_ordinal_count(
	uint32_t dimension,
	size_t* p_count
);

int prototype_dimension_boundary_count(
	uint32_t dimension,
	size_t* p_count
);

int prototype_dimension_face_validate(
	const struct prototype_dimension_face* face
);

uint32_t prototype_dimension_face_intrinsic_dimension(
	const struct prototype_dimension_face* face
);

int prototype_dimension_face_is_center(
	const struct prototype_dimension_face* face
);

int prototype_dimension_face_from_ordinal(
	uint32_t dimension,
	size_t ordinal,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face
);

int prototype_dimension_face_ordinal(
	const struct prototype_dimension_face* face,
	size_t* p_ordinal
);

int prototype_dimension_face_boundary_count(
	const struct prototype_dimension_face* face,
	size_t* p_count
);

int prototype_dimension_face_boundary_ordinal(
	const struct prototype_dimension_face* face,
	size_t boundary_index,
	size_t* p_ordinal
);

int prototype_dimension_boundary_from_index(
	uint32_t dimension,
	size_t boundary_index,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face
);

int prototype_dimension_face_iterator_init(
	struct prototype_dimension_face_iterator* iterator,
	uint32_t dimension
);

int prototype_dimension_face_iterator_next_boundary(
	struct prototype_dimension_face_iterator* iterator,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face,
	int* p_present
);

int prototype_dimension_face_restrict(
	const struct prototype_dimension_operator_db* operators,
	uint32_t operator_id,
	const struct prototype_dimension_face* target_face,
	uint8_t* source_digits,
	size_t source_digit_capacity,
	struct prototype_dimension_face* p_source_face
);

#endif
