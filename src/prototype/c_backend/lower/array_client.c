#include "component.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t allocations = SIZE_MAX;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	if (allocations == SIZE_MAX) return __real_malloc(size);
	if (!allocations) return NULL;
	--allocations;
	return __real_malloc(size);
}

static void ordinary(void)
{
	size_t cases = 0;
	for (size_t length = 0; length <= 8; ++length) {
		for (size_t code = 0; code < ((size_t)1 << length); ++code) {
			struct ap_c_arena arena = {0};
			uint32_t input[8], expected[8], buffer[9];
			for (size_t i = 0; i < length; ++i) input[i] = expected[i] = (uint32_t)((code >> i) & 1) * 3;
			const struct ap_data_Numbers *list;
			assert(!ap_from_Numbers(&arena, length ? input : NULL, length, &list));
			assert(arena.count == length + 1);
			for (size_t i = 0; i < length; ++i) input[i] = UINT32_MAX;
			size_t written = SIZE_MAX;
			assert(!ap_copy_Numbers(list, buffer, 8, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer[i] == expected[i]);
			for (uint32_t pivot = 0; pivot <= 4; ++pivot) for (unsigned upper = 0; upper < 2; ++upper) {
				const struct ap_data_Numbers *selected;
				int status = upper ? ap_export_upper(&arena, list, pivot, &selected) : ap_export_lower(&arena, list, pivot, &selected);
				assert(!status);
				buffer[8] = 77;
				assert(!ap_copy_Numbers(selected, buffer, 8, &written));
				size_t count = 0;
				for (size_t i = 0; i < length; ++i)
					if ((expected[i] > pivot) == upper) assert(buffer[count++] == expected[i]);
				assert(written == count && buffer[8] == 77);
			}
			ap_arena_Nat_destroy(&arena);
			assert(!arena.count && !arena.first);
			++cases;
		}
	}
	assert(cases == 511);
}

static void boundaries(void)
{
	struct ap_c_arena arena = {0};
	uint32_t input[] = {0, UINT32_MAX, 7}, copied[3];
	const struct ap_data_Numbers *saved;
	assert(!ap_from_Numbers(&arena, input, 3, &saved) && arena.count == 4);
	struct ap_c_allocation *mark = arena.first;
	for (size_t budget = 0; budget < 4; ++budget) {
		const struct ap_data_Numbers *out = saved;
		allocations = budget;
		assert(ap_from_Numbers(&arena, input, 3, &out) == 3 && out == saved);
		assert(arena.first == mark && arena.count == 4 && !arena.depth);
	}
	allocations = SIZE_MAX;
	for (size_t capacity = 4; capacity < 8; ++capacity) {
		arena.capacity = capacity;
		const struct ap_data_Numbers *out = saved;
		assert(ap_from_Numbers(&arena, input, 3, &out) == 3 && out == saved);
		assert(arena.first == mark && arena.count == 4);
	}
	arena.capacity = 8;
	const struct ap_data_Numbers *out;
	assert(!ap_from_Numbers(&arena, input, 3, &out) && arena.count == 8);
	size_t written;
	assert(!ap_copy_Numbers(saved, copied, 3, &written) && written == 3);
	assert(copied[0] == 0 && copied[1] == UINT32_MAX && copied[2] == 7);
	mark = arena.first;
	out = saved;
	assert(ap_from_Numbers(NULL, input, 3, &out) == 1 && out == saved);
	assert(ap_from_Numbers(&arena, input, 3, NULL) == 1);
	assert(ap_from_Numbers(&arena, NULL, 3, &out) == 1 && out == saved);
	assert(ap_from_Numbers(&arena, input, SIZE_MAX, &out) == 6 && out == saved);
	arena.depth = 1;
	assert(ap_from_Numbers(&arena, input, 3, &out) == 4 && out == saved && arena.depth == 1);
	arena.depth = 0;
	assert(arena.first == mark && arena.count == 8);
	ap_arena_Nat_destroy(&arena);
	arena.capacity = 0;
	assert(!ap_from_Numbers(&arena, NULL, 0, &out) && arena.count == 1);
	assert(!ap_copy_Numbers(out, NULL, 0, &written) && !written);
	ap_arena_Nat_destroy(&arena);

	int64_t wide[] = {INT64_MIN, -1, 0, INT64_MAX}, wide_copy[4];
	const struct ap_data_LongNumbers *long_list;
	assert(!ap_from_LongNumbers(&arena, wide, 4, &long_list));
	assert(!ap_copy_LongNumbers(long_list, wide_copy, 4, &written) && written == 4);
	for (size_t i = 0; i < 4; ++i) assert(wide_copy[i] == wide[i]);
	ap_arena_Nat_destroy(&arena);

	uint32_t many[300], many_copy[300];
	for (size_t i = 0; i < 300; ++i) many[i] = (uint32_t)i;
	arena.depth_limit = 1;
	assert(!ap_from_Numbers(&arena, many, 300, &out) && arena.count == 301);
	assert(!ap_copy_Numbers(out, many_copy, 300, &written) && written == 300);
	for (size_t i = 0; i < 300; ++i) assert(many_copy[i] == many[i]);
	int32_t length = 77;
	assert(ap_export_length(&arena, out, &length) == 4 && length == 77);
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	ordinary(); boundaries();
	puts("Native array/List client: 511 slices, 5110 partitions, signed fields and transactional allocation failures passed");
	return 0;
}
