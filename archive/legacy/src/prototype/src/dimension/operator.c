#include "a_program/dimension/operator.h"

#include "a_program/support/schema.h"
#include "a_program/support/storage.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint64_t dimension_hash_u32(uint64_t hash, uint32_t value) {
	for (size_t i = 0; i < sizeof(value); ++i) {
		hash ^= (uint8_t)(value >> (i * CHAR_BIT));
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t dimension_operator_hash(
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = dimension_hash_u32(hash, source_dimension);
	hash = dimension_hash_u32(hash, target_dimension);
	for (size_t i = 0; i < image_count; ++i) {
		hash = dimension_hash_u32(hash, (uint32_t)images[i].kind);
		hash = dimension_hash_u32(hash, images[i].target_axis);
	}
	return hash;
}

void prototype_dimension_operator_db_init(
	struct prototype_dimension_operator_db* db,
	struct prototype_dimension_operator* operators,
	size_t operator_capacity,
	struct prototype_dimension_axis_image* images,
	size_t image_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->operators = operators;
	db->operator_capacity = operator_capacity;
	db->images = images;
	db->image_capacity = image_capacity;
	prototype_intern_index_clear(
		db->index_heads,
		PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
}


const struct prototype_dimension_operator* prototype_dimension_operator_get(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
) {
	if (!db || operator_id >= db->operator_count) {
		return NULL;
	}
	return &db->operators[operator_id];
}

const struct prototype_dimension_axis_image*
prototype_dimension_operator_images(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
) {
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(db, operator_id);
	if (!operator || operator->image_offset > db->image_count ||
		operator->image_count > db->image_count - operator->image_offset) {
		return NULL;
	}
	return operator->image_count == 0 ? NULL : &db->images[operator->image_offset];
}

int prototype_dimension_operator_validate(
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count
) {
	if (image_count != source_dimension || (image_count != 0 && !images)) {
		return -1;
	}
	for (size_t i = 0; i < image_count; ++i) {
		if (images[i].kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_0 ||
			images[i].kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_1) {
			if (images[i].target_axis != 0) {
				return -1;
			}
			continue;
		}
		if (images[i].kind != PROTOTYPE_DIMENSION_AXIS_TARGET ||
			images[i].target_axis >= target_dimension) {
			return -1;
		}
		for (size_t prior = 0; prior < i; ++prior) {
			if (images[prior].kind == PROTOTYPE_DIMENSION_AXIS_TARGET &&
				images[prior].target_axis == images[i].target_axis) {
				return -1;
			}
		}
	}
	return 0;
}

static int dimension_operator_equal(
	const struct prototype_dimension_operator_db* db,
	const struct prototype_dimension_operator* operator,
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count
) {
	if (operator->source_dimension != source_dimension ||
		operator->target_dimension != target_dimension ||
		operator->image_count != image_count ||
		operator->image_offset > db->image_count ||
		image_count > db->image_count - operator->image_offset) {
		return 0;
	}
	for (size_t i = 0; i < image_count; ++i) {
		const struct prototype_dimension_axis_image* stored =
			&db->images[operator->image_offset + i];
		if (stored->kind != images[i].kind ||
			stored->target_axis != images[i].target_axis) {
			return 0;
		}
	}
	return 1;
}

int prototype_dimension_operator_find(
	const struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count,
	uint32_t* p_operator_id
) {
	if (!db || !p_operator_id || prototype_dimension_operator_validate(
			source_dimension, target_dimension, images, image_count
		) != 0) {
		return -1;
	}
	uint64_t hash = dimension_operator_hash(
		source_dimension, target_dimension, images, image_count
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->index_heads[bucket]; id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->operator_count) {
			return -1;
		}
		const struct prototype_dimension_operator* operator = &db->operators[id];
		if (operator->key_hash == hash && dimension_operator_equal(
				db,
				operator,
				source_dimension,
				target_dimension,
				images,
				image_count
			)) {
			*p_operator_id = id;
			return 0;
		}
		id = operator->hash_next;
	}
	return 1;
}

int prototype_dimension_operator_intern(
	struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t target_dimension,
	const struct prototype_dimension_axis_image* images,
	size_t image_count,
	uint32_t* p_operator_id
) {
	if (!db || !p_operator_id ||
		prototype_dimension_operator_validate(
			source_dimension, target_dimension, images, image_count
		) != 0) {
		return -1;
	}
	db->intern_requests++;
	uint64_t hash = dimension_operator_hash(
		source_dimension, target_dimension, images, image_count
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->index_heads[bucket]; id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->operator_count) {
			return -1;
		}
		db->intern_probes++;
		const struct prototype_dimension_operator* operator = &db->operators[id];
		if (operator->key_hash == hash && dimension_operator_equal(
				db, operator, source_dimension, target_dimension, images, image_count
			)) {
			db->intern_hits++;
			*p_operator_id = id;
			return 0;
		}
		id = operator->hash_next;
	}
	if (prototype_storage_reserve_slot(
			db->operator_count, db->operator_capacity
		) != 0 || prototype_storage_reserve(
			db->image_count, image_count, db->image_capacity
		) != 0 || db->operator_count > UINT32_MAX) {
		return -1;
	}
	size_t image_offset = db->image_count;
	for (size_t i = 0; i < image_count; ++i) {
		db->images[db->image_count++] = images[i];
	}
	uint32_t id = (uint32_t)db->operator_count++;
	db->operators[id] = (struct prototype_dimension_operator) {
		.source_dimension = source_dimension,
		.target_dimension = target_dimension,
		.image_offset = image_offset,
		.image_count = image_count,
		.key_hash = hash,
		.hash_next = db->index_heads[bucket]
	};
	db->index_heads[bucket] = id;
	*p_operator_id = id;
	return 0;
}

static int prototype_dimension_operator_db_rebuild_index(
	struct prototype_dimension_operator_db* db
) {
	if (!db) {
		return -1;
	}
	prototype_intern_index_clear(
		db->index_heads,
		PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	for (size_t i = 0; i < db->operator_count; ++i) {
		struct prototype_dimension_operator* operator = &db->operators[i];
		const struct prototype_dimension_axis_image* images =
			prototype_dimension_operator_images(db, (uint32_t)i);
		if (prototype_dimension_operator_validate(
				operator->source_dimension,
				operator->target_dimension,
				images,
				operator->image_count
			) != 0) {
			return -1;
		}
		operator->key_hash = dimension_operator_hash(
			operator->source_dimension,
			operator->target_dimension,
			images,
			operator->image_count
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				operator->key_hash,
				PROTOTYPE_DIMENSION_OPERATOR_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		operator->hash_next = db->index_heads[bucket];
		db->index_heads[bucket] = (uint32_t)i;
	}
	return 0;
}

static int dimension_make_axis_images(
	uint32_t dimension,
	struct prototype_dimension_axis_image** p_images
) {
	if (!p_images || (dimension != 0 &&
		SIZE_MAX / (size_t)dimension < sizeof(**p_images))) {
		return -1;
	}
	*p_images = dimension == 0 ? NULL :
		malloc((size_t)dimension * sizeof(**p_images));
	return dimension == 0 || *p_images ? 0 : -1;
}

int prototype_dimension_operator_identity(
	struct prototype_dimension_operator_db* db,
	uint32_t dimension,
	uint32_t* p_operator_id
) {
	struct prototype_dimension_axis_image* images;
	if (dimension_make_axis_images(dimension, &images) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < dimension; ++i) {
		images[i] = (struct prototype_dimension_axis_image) {
			.kind = PROTOTYPE_DIMENSION_AXIS_TARGET,
			.target_axis = i
		};
	}
	int result = prototype_dimension_operator_intern(
		db, dimension, dimension, images, dimension, p_operator_id
	);
	free(images);
	return result;
}

int prototype_dimension_operator_extension(
	struct prototype_dimension_operator_db* db,
	uint32_t source_dimension,
	uint32_t* p_operator_id
) {
	if (source_dimension == UINT32_MAX) {
		return -1;
	}
	struct prototype_dimension_axis_image* images;
	if (dimension_make_axis_images(source_dimension, &images) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_dimension; ++i) {
		images[i] = (struct prototype_dimension_axis_image) {
			.kind = PROTOTYPE_DIMENSION_AXIS_TARGET,
			.target_axis = i
		};
	}
	int result = prototype_dimension_operator_intern(
		db,
		source_dimension,
		source_dimension + 1,
		images,
		source_dimension,
		p_operator_id
	);
	free(images);
	return result;
}

int prototype_dimension_operator_is_canonical_extension(
	const struct prototype_dimension_operator_db* db,
	uint32_t operator_id
) {
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(db, operator_id);
	if (!operator || operator->source_dimension == UINT32_MAX ||
		operator->target_dimension != operator->source_dimension + 1 ||
		operator->image_count != operator->source_dimension) {
		return 0;
	}
	const struct prototype_dimension_axis_image* images =
		prototype_dimension_operator_images(db, operator_id);
	if (operator->image_count != 0 && !images) {
		return 0;
	}
	for (uint32_t axis = 0; axis < operator->source_dimension; ++axis) {
		if (images[axis].kind != PROTOTYPE_DIMENSION_AXIS_TARGET ||
			images[axis].target_axis != axis) {
			return 0;
		}
	}
	return 1;
}

int prototype_dimension_operator_compose(
	struct prototype_dimension_operator_db* db,
	uint32_t first_operator_id,
	uint32_t second_operator_id,
	struct prototype_dimension_axis_image* scratch_images,
	size_t scratch_capacity,
	uint32_t* p_operator_id
) {
	const struct prototype_dimension_operator* first =
		prototype_dimension_operator_get(db, first_operator_id);
	const struct prototype_dimension_operator* second =
		prototype_dimension_operator_get(db, second_operator_id);
	if (!first || !second || first->target_dimension != second->source_dimension ||
		first->image_count > scratch_capacity ||
		(first->image_count != 0 && !scratch_images)) {
		return -1;
	}
	const struct prototype_dimension_axis_image* first_images =
		prototype_dimension_operator_images(db, first_operator_id);
	const struct prototype_dimension_axis_image* second_images =
		prototype_dimension_operator_images(db, second_operator_id);
	for (size_t i = 0; i < first->image_count; ++i) {
		if (first_images[i].kind == PROTOTYPE_DIMENSION_AXIS_TARGET) {
			if (!second_images || first_images[i].target_axis >= second->image_count) {
				return -1;
			}
			scratch_images[i] = second_images[first_images[i].target_axis];
		} else {
			scratch_images[i] = first_images[i];
		}
	}
	return prototype_dimension_operator_intern(
		db,
		first->source_dimension,
		second->target_dimension,
		scratch_images,
		first->image_count,
		p_operator_id
	);
}

struct prototype_dimension_operator_mark prototype_dimension_operator_mark(
	const struct prototype_dimension_operator_db* db
) {
	return (struct prototype_dimension_operator_mark) {
		.operator_count = db ? db->operator_count : 0,
		.image_count = db ? db->image_count : 0
	};
}

int prototype_dimension_operator_rollback(
	struct prototype_dimension_operator_db* db,
	struct prototype_dimension_operator_mark mark
) {
	if (!db || mark.operator_count > db->operator_count ||
		mark.image_count > db->image_count) {
		return -1;
	}
	db->operator_count = mark.operator_count;
	db->image_count = mark.image_count;
	return prototype_dimension_operator_db_rebuild_index(db);
}
