#ifdef FLAGS_FIRST
#include "flags.h"
#include "numbers.h"
#else
#include "numbers.h"
#include "flags.h"
#endif
#include <assert.h>
#include <stdio.h>

static void numbers_equal(const struct ap_data_Numbers *input, const uint32_t *expected, size_t count)
{
	uint32_t values[12];
	size_t written = 77;
	assert(!ap_copy_Numbers(input, values, 12, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(values[i] == expected[i]);
}

static void flags_equal(const struct ap_data_Flags *input, const struct ap_enum_Bool *expected, size_t count)
{
	struct ap_enum_Bool values[7];
	size_t written = 77;
	assert(!ap_copy_Flags(input, values, 7, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(values[i].tag == expected[i].tag);
}

static void ordinary(size_t count, unsigned digits)
{
	struct ap_c_arena arena = {0};
	uint32_t values[6], copied[6], doubled[12];
	struct ap_enum_Bool flags[6], prefixed[7];
	for (size_t i = 0; i < count; ++i) {
		values[i] = doubled[i] = doubled[count + i] = digits % 3;
		digits /= 3;
	}
	const struct ap_data_Numbers *numbers, *number_result;
	assert(!ap_from_Numbers(&arena, count ? values : NULL, count, &numbers));
	size_t written = 77;
	assert(!ap_copy_Numbers(numbers, copied, 6, &written) && written == count);
	/* C converts payload arrays explicitly; distinct nominal nodes never mix. */
	for (size_t i = 0; i < count; ++i) flags[i].tag = copied[i] & 1;
	const struct ap_data_Flags *booleans, *flag_result;
	assert(!ap_from_Flags(&arena, count ? flags : NULL, count, &booleans));
	assert(!ap_export_identity(&arena, numbers, &number_result) && number_result == numbers);
	assert(!ap_export_identity_flags(&arena, booleans, &flag_result) && flag_result == booleans);
	int32_t length = -1;
	assert(!ap_export_length(&arena, numbers, &length) && length == (int32_t)count);
	assert(!ap_export_length_flags(&arena, booleans, &length) && length == (int32_t)count);
	assert(!ap_export_append(&arena, numbers, numbers, &number_result));
	numbers_equal(number_result, doubled, 2 * count);
	prefixed[0].tag = 1;
	for (size_t i = 0; i < count; ++i) prefixed[i + 1] = flags[i];
	assert(!ap_export_prepend_flag(&arena, prefixed[0], booleans, &flag_result));
	flags_equal(flag_result, prefixed, count + 1);

	/* A failing module rolls back only its additions to the shared C arena. */
	struct ap_c_allocation *mark = arena.first;
	size_t saved = arena.count;
	arena.capacity = saved + 1;
	number_result = numbers;
	assert(ap_from_Numbers(&arena, (uint32_t[]){7, 8}, 2, &number_result) == 3);
	assert(number_result == numbers && arena.first == mark && arena.count == saved);
	flags_equal(booleans, flags, count);
	flag_result = booleans;
	assert(ap_from_Flags(&arena, (struct ap_enum_Bool[]){{1}}, 1, &flag_result) == 3);
	assert(flag_result == booleans && arena.first == mark && arena.count == saved);
	numbers_equal(numbers, values, count);
	arena.capacity = 0;
	assert(!ap_export_identity_flags(&arena, booleans, &flag_result) && flag_result == booleans);
	assert(!arena.status);
	flag_result = booleans;
	assert(ap_from_Flags(&arena, (struct ap_enum_Bool[]){{2}}, 1, &flag_result) == 2);
	assert(flag_result == booleans && arena.first == mark && arena.count == saved);
	if (count) {
		arena.depth_limit = 1; length = 77;
		assert(ap_export_length_flags(&arena, booleans, &length) == 4 && length == 77);
		assert(arena.first == mark && arena.count == saved && !arena.depth);
		arena.depth_limit = 0;
		assert(!ap_export_length(&arena, numbers, &length) && length == (int32_t)count);
	}
	/* Both products implement the same versioned allocation metadata contract. */
	if (count & 1) ap_arena_Nat_destroy(&arena);
	else ap_arena_Flags_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.status && !arena.depth);
}

int main(void)
{
	size_t cases = 0;
	unsigned combinations = 1;
	for (size_t count = 0; count <= 6; ++count, combinations *= 3)
		for (unsigned digits = 0; digits < combinations; ++digits) {
			ordinary(count, digits); ++cases;
		}
	assert(cases == 1093);
	puts("Mixed C modules: 1093 rows, explicit array conversion, shared arena rollback and lifetime pass");
	return 0;
}
