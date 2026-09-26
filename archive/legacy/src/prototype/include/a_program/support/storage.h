#ifndef A_PROGRAM_PROTOTYPE_SUPPORT_STORAGE_H
#define A_PROGRAM_PROTOTYPE_SUPPORT_STORAGE_H

#include <stddef.h>
#include <stdint.h>

struct prototype_storage_transaction_mark {
	size_t primary_count;
	size_t secondary_count;
};

int prototype_storage_reserve(
	size_t count,
	size_t additional,
	size_t capacity
);

int prototype_storage_reserve_slot(size_t count, size_t capacity);

int prototype_storage_next_capacity(
	size_t current_capacity,
	size_t required_capacity,
	size_t maximum_capacity,
	size_t* p_capacity
);

struct prototype_storage_transaction_mark prototype_storage_transaction_mark(
	size_t primary_count,
	size_t secondary_count
);

int prototype_storage_transaction_rollback(
	struct prototype_storage_transaction_mark mark,
	size_t* primary_count,
	size_t* secondary_count
);

void prototype_intern_index_clear(
	uint32_t* heads,
	size_t bucket_count,
	uint32_t invalid_id
);

int prototype_intern_index_bucket(
	uint64_t key_hash,
	size_t bucket_count,
	size_t* p_bucket
);

#endif
