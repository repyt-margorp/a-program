#include "a_program/support/storage.h"

#include <stdint.h>

int main(void) {
	size_t capacity;
	if (prototype_storage_reserve(3, 2, 5) != 0 ||
		prototype_storage_reserve(3, 3, 5) == 0 ||
		prototype_storage_next_capacity(4, 9, 16, &capacity) != 0 ||
		capacity != 16 ||
		prototype_storage_next_capacity(4, 17, 16, &capacity) == 0 ||
		prototype_storage_next_capacity(0, 0, 0, &capacity) != 0 ||
		capacity != 0 ||
		prototype_storage_next_capacity(0, 1, 0, &capacity) == 0) {
		return 1;
	}
	size_t primary = 7;
	size_t secondary = 11;
	struct prototype_storage_transaction_mark mark =
		prototype_storage_transaction_mark(3, 5);
	if (prototype_storage_transaction_rollback(
			mark, &primary, &secondary
		) != 0 || primary != 3 || secondary != 5) {
		return 1;
	}
	uint32_t heads[4] = { 0, 1, 2, 3 };
	prototype_intern_index_clear(heads, 4, UINT32_MAX);
	for (size_t i = 0; i < 4; ++i) {
		if (heads[i] != UINT32_MAX) {
			return 1;
		}
	}
	size_t bucket;
	if (prototype_intern_index_bucket(7, 4, &bucket) != 0 || bucket != 3 ||
		prototype_intern_index_bucket(7, 0, &bucket) == 0) {
		return 1;
	}
	return 0;
}
