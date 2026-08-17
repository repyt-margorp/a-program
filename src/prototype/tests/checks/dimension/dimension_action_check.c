#include "a_program/dimension/face.h"
#include "a_program/dimension/operator.h"

#include <stdint.h>

#define OPERATOR_CAPACITY 32
#define IMAGE_CAPACITY 128

static int check_boundary_counts(void) {
	const size_t expected[] = { 0, 2, 8, 26, 80 };
	for (uint32_t dimension = 0; dimension < 5; ++dimension) {
		size_t count;
		if (prototype_dimension_boundary_count(dimension, &count) != 0 ||
			count != expected[dimension]) {
			return -1;
		}
		uint8_t digits[4];
		struct prototype_dimension_face_iterator iterator;
		if (prototype_dimension_face_iterator_init(
				&iterator, dimension
			) != 0) {
			return -1;
		}
		size_t visited = 0;
		for (;;) {
			struct prototype_dimension_face face;
			int present;
			if (prototype_dimension_face_iterator_next_boundary(
					&iterator, digits, sizeof(digits), &face, &present
				) != 0) {
				return -1;
			}
			if (!present) {
				break;
			}
			if (prototype_dimension_face_is_center(&face)) {
				return -1;
			}
			visited++;
		}
		if (visited != count) {
			return -1;
		}
	}
	return 0;
}

static int check_operators(void) {
	struct prototype_dimension_operator operator_storage[OPERATOR_CAPACITY];
	struct prototype_dimension_axis_image image_storage[IMAGE_CAPACITY];
	struct prototype_dimension_operator_db db;
	prototype_dimension_operator_db_init(
		&db,
		operator_storage,
		OPERATOR_CAPACITY,
		image_storage,
		IMAGE_CAPACITY
	);

	uint32_t extension_0;
	uint32_t extension_1;
	uint32_t extension_0_again;
	if (prototype_dimension_operator_extension(&db, 0, &extension_0) != 0 ||
		prototype_dimension_operator_extension(&db, 1, &extension_1) != 0 ||
		prototype_dimension_operator_extension(
			&db, 0, &extension_0_again
		) != 0 || extension_0 != extension_0_again || db.operator_count != 2 ||
		db.intern_hits != 1) {
		return -1;
	}

	struct prototype_dimension_axis_image scratch[4];
	uint32_t composite;
	if (prototype_dimension_operator_compose(
			&db, extension_0, extension_1, scratch, 4, &composite
		) != 0) {
		return -1;
	}
	const struct prototype_dimension_operator* composed =
		prototype_dimension_operator_get(&db, composite);
	if (!composed || composed->source_dimension != 0 ||
		composed->target_dimension != 2) {
		return -1;
	}

	uint32_t identity_1;
	uint32_t left_unit;
	if (prototype_dimension_operator_identity(&db, 1, &identity_1) != 0 ||
		prototype_dimension_operator_compose(
			&db, identity_1, extension_1, scratch, 4, &left_unit
		) != 0 || left_unit != extension_1) {
		return -1;
	}

	uint8_t target_digits[2] = {
		PROTOTYPE_DIMENSION_FACE_ENDPOINT_1,
		PROTOTYPE_DIMENSION_FACE_VARYING
	};
	struct prototype_dimension_face target_face = {
		.dimension = 2,
		.digits = target_digits
	};
	uint8_t source_digits[1];
	struct prototype_dimension_face source_face;
	if (prototype_dimension_face_restrict(
			&db,
			extension_1,
			&target_face,
			source_digits,
			1,
			&source_face
		) != 0 || source_face.digits[0] != PROTOTYPE_DIMENSION_FACE_ENDPOINT_1) {
		return -1;
	}

	struct prototype_dimension_operator_mark mark =
		prototype_dimension_operator_mark(&db);
	uint32_t identity_3;
	if (prototype_dimension_operator_identity(&db, 3, &identity_3) != 0 ||
		db.operator_count == mark.operator_count ||
		prototype_dimension_operator_rollback(&db, mark) != 0 ||
		db.operator_count != mark.operator_count ||
		db.image_count != mark.image_count) {
		return -1;
	}
	return 0;
}

static int check_malformed(void) {
	struct prototype_dimension_axis_image duplicate[] = {
		{ PROTOTYPE_DIMENSION_AXIS_TARGET, 0 },
		{ PROTOTYPE_DIMENSION_AXIS_TARGET, 0 }
	};
	struct prototype_dimension_axis_image invalid_endpoint = {
		PROTOTYPE_DIMENSION_AXIS_ENDPOINT_0,
		1
	};
	if (prototype_dimension_operator_validate(2, 1, duplicate, 2) == 0 ||
		prototype_dimension_operator_validate(
			1, 0, &invalid_endpoint, 1
		) == 0 || prototype_dimension_operator_validate(1, 1, NULL, 1) == 0) {
		return -1;
	}

	size_t count;
	uint32_t overflow_dimension = 0;
	size_t power = 1;
	while (power <= SIZE_MAX / 3) {
		power *= 3;
		overflow_dimension++;
	}
	if (prototype_dimension_face_ordinal_count(
			overflow_dimension + 1, &count
		) == 0) {
		return -1;
	}

	uint8_t bad_digit = 3;
	struct prototype_dimension_face bad_face = {
		.dimension = 1,
		.digits = &bad_digit
	};
	return prototype_dimension_face_validate(&bad_face) == 0 ? -1 : 0;
}

int main(void) {
	return check_boundary_counts() != 0 || check_operators() != 0 ||
		check_malformed() != 0;
}
