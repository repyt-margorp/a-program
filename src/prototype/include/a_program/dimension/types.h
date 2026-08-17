#ifndef A_PROGRAM_PROTOTYPE_DIMENSION_TYPES_H
#define A_PROGRAM_PROTOTYPE_DIMENSION_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT 257

enum prototype_dimension_axis_image_kind {
	PROTOTYPE_DIMENSION_AXIS_ENDPOINT_0 = 1,
	PROTOTYPE_DIMENSION_AXIS_ENDPOINT_1 = 2,
	PROTOTYPE_DIMENSION_AXIS_TARGET = 3
};

enum prototype_dimension_face_digit {
	PROTOTYPE_DIMENSION_FACE_ENDPOINT_0 = 0,
	PROTOTYPE_DIMENSION_FACE_ENDPOINT_1 = 1,
	PROTOTYPE_DIMENSION_FACE_VARYING = 2
};

struct prototype_dimension_axis_image {
	int kind;
	uint32_t target_axis;
};

struct prototype_dimension_operator {
	uint32_t source_dimension;
	uint32_t target_dimension;
	size_t image_offset;
	size_t image_count;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_dimension_operator_db {
	struct prototype_dimension_operator* operators;
	size_t operator_count;
	size_t operator_capacity;
	struct prototype_dimension_axis_image* images;
	size_t image_count;
	size_t image_capacity;
	uint32_t index_heads[PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT];
	uint64_t intern_requests;
	uint64_t intern_hits;
	uint64_t intern_probes;
};

struct prototype_dimension_operator_mark {
	size_t operator_count;
	size_t image_count;
};

struct prototype_dimension_face {
	uint32_t dimension;
	uint8_t* digits;
};

struct prototype_dimension_face_iterator {
	uint32_t dimension;
	size_t next_ordinal;
	size_t total_ordinal_count;
};

#endif
