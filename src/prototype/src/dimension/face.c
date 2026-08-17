#include "a_program/dimension/face.h"

#include "a_program/dimension/operator.h"

#include <stdint.h>

int prototype_dimension_face_ordinal_count(
	uint32_t dimension,
	size_t* p_count
) {
	if (!p_count) {
		return -1;
	}
	size_t count = 1;
	for (uint32_t i = 0; i < dimension; ++i) {
		if (count > SIZE_MAX / 3) {
			return -1;
		}
		count *= 3;
	}
	*p_count = count;
	return 0;
}

int prototype_dimension_boundary_count(
	uint32_t dimension,
	size_t* p_count
) {
	size_t count;
	if (!p_count || prototype_dimension_face_ordinal_count(
			dimension, &count
		) != 0 || count == 0) {
		return -1;
	}
	*p_count = count - 1;
	return 0;
}

int prototype_dimension_face_validate(
	const struct prototype_dimension_face* face
) {
	if (!face || (face->dimension != 0 && !face->digits)) {
		return -1;
	}
	for (uint32_t i = 0; i < face->dimension; ++i) {
		if (face->digits[i] > PROTOTYPE_DIMENSION_FACE_VARYING) {
			return -1;
		}
	}
	return 0;
}

uint32_t prototype_dimension_face_intrinsic_dimension(
	const struct prototype_dimension_face* face
) {
	if (prototype_dimension_face_validate(face) != 0) {
		return UINT32_MAX;
	}
	uint32_t dimension = 0;
	for (uint32_t i = 0; i < face->dimension; ++i) {
		dimension += face->digits[i] == PROTOTYPE_DIMENSION_FACE_VARYING;
	}
	return dimension;
}

int prototype_dimension_face_is_center(
	const struct prototype_dimension_face* face
) {
	if (prototype_dimension_face_validate(face) != 0) {
		return 0;
	}
	for (uint32_t i = 0; i < face->dimension; ++i) {
		if (face->digits[i] != PROTOTYPE_DIMENSION_FACE_VARYING) {
			return 0;
		}
	}
	return 1;
}

int prototype_dimension_face_from_ordinal(
	uint32_t dimension,
	size_t ordinal,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face
) {
	size_t count;
	if (!p_face || dimension > digit_capacity ||
		(dimension != 0 && !digits) ||
		prototype_dimension_face_ordinal_count(dimension, &count) != 0 ||
		ordinal >= count) {
		return -1;
	}
	for (uint32_t i = dimension; i > 0; --i) {
		digits[i - 1] = (uint8_t)(ordinal % 3);
		ordinal /= 3;
	}
	*p_face = (struct prototype_dimension_face) {
		.dimension = dimension,
		.digits = digits
	};
	return 0;
}

int prototype_dimension_face_ordinal(
	const struct prototype_dimension_face* face,
	size_t* p_ordinal
) {
	if (!p_ordinal || prototype_dimension_face_validate(face) != 0) {
		return -1;
	}
	size_t ordinal = 0;
	for (uint32_t i = 0; i < face->dimension; ++i) {
		if (ordinal > (SIZE_MAX - face->digits[i]) / 3) {
			return -1;
		}
		ordinal = ordinal * 3 + face->digits[i];
	}
	*p_ordinal = ordinal;
	return 0;
}

int prototype_dimension_face_boundary_count(
	const struct prototype_dimension_face* face,
	size_t* p_count
) {
	uint32_t intrinsic_dimension =
		prototype_dimension_face_intrinsic_dimension(face);
	return intrinsic_dimension == UINT32_MAX ? -1 :
		prototype_dimension_boundary_count(intrinsic_dimension, p_count);
}

int prototype_dimension_face_boundary_ordinal(
	const struct prototype_dimension_face* face,
	size_t boundary_index,
	size_t* p_ordinal
) {
	size_t boundary_count;
	uint32_t intrinsic_dimension =
		prototype_dimension_face_intrinsic_dimension(face);
	if (!p_ordinal || intrinsic_dimension == UINT32_MAX ||
		prototype_dimension_boundary_count(
			intrinsic_dimension, &boundary_count
		) != 0 || boundary_index >= boundary_count) {
		return -1;
	}
	uint8_t local_digits[64];
	struct prototype_dimension_face local;
	if (intrinsic_dimension > 64 || prototype_dimension_boundary_from_index(
			intrinsic_dimension,
			boundary_index,
			local_digits,
			64,
			&local
		) != 0) {
		return -1;
	}
	uint8_t global_digits[64];
	uint32_t local_axis = 0;
	if (face->dimension > 64) {
		return -1;
	}
	for (uint32_t i = 0; i < face->dimension; ++i) {
		if (face->digits[i] == PROTOTYPE_DIMENSION_FACE_VARYING) {
			global_digits[i] = local.digits[local_axis++];
		} else {
			global_digits[i] = face->digits[i];
		}
	}
	struct prototype_dimension_face global = {
		.dimension = face->dimension,
		.digits = global_digits
	};
	return prototype_dimension_face_ordinal(&global, p_ordinal);
}

int prototype_dimension_boundary_from_index(
	uint32_t dimension,
	size_t boundary_index,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face
) {
	size_t boundary_count;
	if (prototype_dimension_boundary_count(dimension, &boundary_count) != 0 ||
		boundary_index >= boundary_count) {
		return -1;
	}
	size_t center = 0;
	for (uint32_t i = 0; i < dimension; ++i) {
		if (center > (SIZE_MAX - 2) / 3) {
			return -1;
		}
		center = center * 3 + 2;
	}
	size_t ordinal = boundary_index >= center ? boundary_index + 1 : boundary_index;
	return prototype_dimension_face_from_ordinal(
		dimension, ordinal, digits, digit_capacity, p_face
	);
}

int prototype_dimension_face_iterator_init(
	struct prototype_dimension_face_iterator* iterator,
	uint32_t dimension
) {
	if (!iterator || prototype_dimension_face_ordinal_count(
			dimension, &iterator->total_ordinal_count
		) != 0) {
		return -1;
	}
	iterator->dimension = dimension;
	iterator->next_ordinal = 0;
	return 0;
}

int prototype_dimension_face_iterator_next_boundary(
	struct prototype_dimension_face_iterator* iterator,
	uint8_t* digits,
	size_t digit_capacity,
	struct prototype_dimension_face* p_face,
	int* p_present
) {
	if (!iterator || !p_face || !p_present) {
		return -1;
	}
	while (iterator->next_ordinal < iterator->total_ordinal_count) {
		size_t ordinal = iterator->next_ordinal++;
		if (prototype_dimension_face_from_ordinal(
				iterator->dimension, ordinal, digits, digit_capacity, p_face
			) != 0) {
			return -1;
		}
		if (!prototype_dimension_face_is_center(p_face)) {
			*p_present = 1;
			return 0;
		}
	}
	*p_present = 0;
	return 0;
}

int prototype_dimension_face_restrict(
	const struct prototype_dimension_operator_db* operators,
	uint32_t operator_id,
	const struct prototype_dimension_face* target_face,
	uint8_t* source_digits,
	size_t source_digit_capacity,
	struct prototype_dimension_face* p_source_face
) {
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(operators, operator_id);
	const struct prototype_dimension_axis_image* images =
		prototype_dimension_operator_images(operators, operator_id);
	if (!operator || !target_face || !p_source_face ||
		prototype_dimension_face_validate(target_face) != 0 ||
		target_face->dimension != operator->target_dimension ||
		operator->source_dimension > source_digit_capacity ||
		(operator->source_dimension != 0 && (!source_digits || !images))) {
		return -1;
	}
	for (uint32_t i = 0; i < operator->source_dimension; ++i) {
		if (images[i].kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_0) {
			source_digits[i] = PROTOTYPE_DIMENSION_FACE_ENDPOINT_0;
		} else if (images[i].kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_1) {
			source_digits[i] = PROTOTYPE_DIMENSION_FACE_ENDPOINT_1;
		} else if (images[i].kind == PROTOTYPE_DIMENSION_AXIS_TARGET &&
			images[i].target_axis < target_face->dimension) {
			source_digits[i] = target_face->digits[images[i].target_axis];
		} else {
			return -1;
		}
	}
	*p_source_face = (struct prototype_dimension_face) {
		.dimension = operator->source_dimension,
		.digits = source_digits
	};
	return 0;
}
