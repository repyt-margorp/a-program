#include "a_program/support/storage.h"

#include <stdint.h>

int prototype_storage_reserve(
	size_t count,
	size_t additional,
	size_t capacity
) {
	return count <= capacity && additional <= capacity - count ? 0 : -1;
}

int prototype_storage_reserve_slot(size_t count, size_t capacity) {
	return prototype_storage_reserve(count, 1, capacity);
}

int prototype_storage_next_capacity(
	size_t current_capacity,
	size_t required_capacity,
	size_t maximum_capacity,
	size_t* p_capacity
) {
	if (!p_capacity || required_capacity > maximum_capacity ||
		current_capacity > maximum_capacity) {
		return -1;
	}
	if (required_capacity == 0) {
		*p_capacity = current_capacity;
		return 0;
	}
	if (maximum_capacity == 0) {
		return -1;
	}
	size_t capacity = current_capacity == 0 ? 1 : current_capacity;
	while (capacity < required_capacity) {
		if (capacity > maximum_capacity / 2) {
			capacity = maximum_capacity;
		} else {
			capacity *= 2;
		}
	}
	*p_capacity = capacity;
	return 0;
}

struct prototype_storage_transaction_mark prototype_storage_transaction_mark(
	size_t primary_count,
	size_t secondary_count
) {
	return (struct prototype_storage_transaction_mark) {
		.primary_count = primary_count,
		.secondary_count = secondary_count
	};
}

int prototype_storage_transaction_rollback(
	struct prototype_storage_transaction_mark mark,
	size_t* primary_count,
	size_t* secondary_count
) {
	if (!primary_count || !secondary_count ||
		mark.primary_count > *primary_count ||
		mark.secondary_count > *secondary_count) {
		return -1;
	}
	*primary_count = mark.primary_count;
	*secondary_count = mark.secondary_count;
	return 0;
}

void prototype_intern_index_clear(
	uint32_t* heads,
	size_t bucket_count,
	uint32_t invalid_id
) {
	if (!heads) {
		return;
	}
	for (size_t i = 0; i < bucket_count; ++i) {
		heads[i] = invalid_id;
	}
}

int prototype_intern_index_bucket(
	uint64_t key_hash,
	size_t bucket_count,
	size_t* p_bucket
) {
	if (bucket_count == 0 || !p_bucket) {
		return -1;
	}
	*p_bucket = (size_t)(key_hash % bucket_count);
	return 0;
}
