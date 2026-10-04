#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static int32_t signed32(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : INT32_MIN + (int32_t)(bits - UINT32_C(0x80000000));
}

static int64_t signed64(uint64_t bits)
{
	return bits <= INT64_MAX ? (int64_t)bits : INT64_MIN + (int64_t)(bits - UINT64_C(0x8000000000000000));
}

static void natural_cases(void)
{
	struct ap_c_arena arena = {0};
	const int32_t seeds32[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
	const int64_t seeds64[] = {INT64_MIN, -1, 0, 1, INT64_MAX};
	uint32_t twice = 0;
	for (uint32_t n = 0; n <= 8; ++n) {
		int32_t out32;
		uint32_t triangle = n * (n + 1) / 2;
		assert(!ap_export_count(&arena, n, &out32) && out32 == (int32_t)n);
		assert(!ap_export_branch_sum(&arena, (struct ap_enum_Bool){0}, n, &out32) && !out32);
		assert(!ap_export_branch_sum(&arena, (struct ap_enum_Bool){1}, n, &out32) && out32 == (int32_t)n);
		assert(!ap_export_nested_sum(&arena, n, &out32) && out32 == (int32_t)triangle);
		assert(!ap_export_nested_known(&arena, n, &out32) && out32 == (int32_t)triangle);
		if (n) twice = twice * 2 + n;
		assert(!ap_export_nested_twice(&arena, n, &out32) && out32 == (int32_t)twice);
		assert(!ap_export_nested_unused(&arena, n, &out32) && out32 == (n ? 7 : 0));
		for (size_t i = 0; i < 5; ++i) {
			int32_t expected32 = signed32((uint32_t)seeds32[i] + triangle);
			assert(!ap_export_nested_seed32(&arena, seeds32[i], n, &out32) && out32 == expected32);
			assert(!ap_export_nested_curried(&arena, n, seeds32[i], &out32) && out32 == expected32);
			int64_t out64, expected64 = signed64((uint64_t)seeds64[i] * (triangle + 1));
			assert(!ap_export_nested_seed64(&arena, seeds64[i], n, &out64) && out64 == expected64);
		}
		assert(!arena.depth);
	}
	int32_t out = 77;
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	assert(ap_export_nested_sum(NULL, 3, &out) == 1 && out == 77);
	assert(ap_export_nested_sum(&arena, 3, NULL) == 1);
	assert(ap_export_branch_sum(&arena, (struct ap_enum_Bool){2}, 3, &out) == 2 && out == 77);
	arena.depth_limit = 2;
	assert(ap_export_nested_sum(&arena, 3, &out) == 4 && out == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0;
	assert(ap_export_nested_sum(&arena, UINT32_MAX, &out) == 4 && out == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	assert(!ap_export_nested_sum(&arena, 3, &out) && out == 6);
	assert(!ap_export_nested_unused(&arena, UINT32_MAX, &out) && out == 7);
	ap_arena_Nat_destroy(&arena);
}

static void list_cases(void)
{
	struct ap_c_arena arena = {0};
	const int32_t alphabet[] = {INT32_MIN, -1, 0, INT32_MAX};
	size_t combinations = 1, cases = 0;
	for (size_t length = 0; length <= 3; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		int32_t values[3], copied[3];
		uint32_t expected = 0;
		size_t digits = code, written;
		for (size_t i = 0; i < length; ++i) { values[i] = alphabet[digits % 4]; expected += (uint32_t)values[i]; digits /= 4; }
		const struct ap_data_Numbers *input;
		assert(!ap_from_Numbers(&arena, values, length, &input));
		int32_t out;
		assert(!ap_export_nested_list(&arena, input, &out) && out == signed32(expected));
		assert(!ap_copy_Numbers(input, copied, 3, &written) && written == length);
		for (size_t i = 0; i < length; ++i) assert(copied[i] == values[i]);
		assert(!arena.depth);
		ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 85);
}

int main(void)
{
	natural_cases(); list_cases();
	struct ap_c_arena arena = {0};
	int32_t out;
	assert(!ap_export_count(&arena, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_branch_sum(&arena, (struct ap_enum_Bool){1}, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_branch_sum(&arena, (struct ap_enum_Bool){0}, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_nested_sum(&arena, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_nested_known(&arena, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_nested_seed32(&arena, -1, 3, &out)); printf("%" PRId32 ",", out);
	assert(!ap_export_nested_curried(&arena, 3, -1, &out)); printf("%" PRId32 ",", out);
	const int32_t values[] = {-1, 2};
	const struct ap_data_Numbers *input;
	assert(!ap_from_Numbers(&arena, values, 2, &input));
	assert(!ap_export_nested_list(&arena, input, &out)); printf("%" PRId32, out);
	ap_arena_Nat_destroy(&arena);
	return 0;
}
