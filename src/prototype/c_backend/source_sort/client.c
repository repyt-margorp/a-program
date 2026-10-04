#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void equal_list(const struct ap_data_List *list, const uint32_t *expected, size_t length)
{
	uint32_t buffer[6] = {77,77,77,77,77,77};
	size_t written;
	assert(!ap_copy_List(list, buffer, 6, &written) && written == length);
	for (size_t i = 0; i < length; ++i) assert(buffer[i] == expected[i]);
	for (size_t i = length; i < 6; ++i) assert(buffer[i] == 77);
}

static void finite_cases(void)
{
	struct ap_c_arena arena = {0};
	size_t combinations = 1, cases = 0;
	for (size_t length = 0; length <= 4; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		uint32_t values[4], sorted[5], counts[4] = {0};
		size_t digits = code, at = 0;
		for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; ++counts[values[i]]; digits /= 4; }
		for (uint32_t n = 0; n < 4; ++n) for (uint32_t i = 0; i < counts[n]; ++i) sorted[at++] = n;
		const struct ap_data_List *input, *out;
		assert(!ap_from_List(&arena, values, length, &input));
		assert(!ap_export_sort(&arena, input, &out)); equal_list(out, sorted, length);
		assert(!ap_export_sort_alias(&arena, input, &out)); equal_list(out, sorted, length);
		assert(!ap_export_list_id(&arena, input, &out) && out == input);
		for (uint32_t pivot = 0; pivot < 4; ++pivot) {
			uint32_t inserted[5]; size_t position = 0;
			while (position < length && values[position] < pivot) ++position;
			for (size_t i = 0; i < position; ++i) inserted[i] = values[i];
			inserted[position] = pivot;
			for (size_t i = position; i < length; ++i) inserted[i + 1] = values[i];
			assert(!ap_export_insert(&arena, pivot, input, &out)); equal_list(out, inserted, length + 1);
		}
		equal_list(input, values, length);
		assert(!arena.depth); ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 341);
	const uint32_t numbers[] = {0,1,2,3,17,255,UINT32_MAX};
	for (size_t i = 0; i < sizeof(numbers) / sizeof(*numbers); ++i) {
		uint32_t out;
		assert(!ap_export_fixed_id(&arena, numbers[i], &out) && out == numbers[i]);
	}
	const int32_t values32[] = {INT32_MIN,-1,0,1,INT32_MAX};
	const int64_t values64[] = {INT64_MIN,-1,0,1,INT64_MAX};
	for (size_t i = 0; i < 5; ++i) {
		int32_t out32; int64_t out64;
		assert(!ap_export_fixed_int32(&arena, values32[i], &out32) && out32 == values32[i]);
		assert(!ap_export_fixed_int64(&arena, values64[i], &out64) && out64 == values64[i]);
	}
	for (unsigned flag = 0; flag < 2; ++flag) {
		struct ap_enum_Bool out;
		assert(!ap_export_fixed_bool(&arena, (struct ap_enum_Bool){flag}, &out) && out.tag == flag);
	}
	for (uint32_t left = 0; left < 5; ++left) for (uint32_t right = 0; right < 5; ++right) {
		struct ap_enum_Bool out;
		assert(!ap_export_compare(&arena, left, right, &out) && out.tag == (left <= right ? AP_ENUM_Bool_C0 : AP_ENUM_Bool_C1));
	}
	ap_arena_Nat_destroy(&arena);
}

static void resource_cases(void)
{
	struct ap_c_arena arena = {0};
	static const struct ap_data_List nil = {.tag = AP_DATA_List_C0};
	const uint32_t values[] = {3,0,2,1,2}, sorted[] = {0,1,2,2,3};
	const struct ap_data_List *input, *out = &nil;
	assert(!ap_from_List(&arena, values, 5, &input));
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	assert(ap_export_sort(NULL, input, &out) == 1 && out == &nil);
	assert(ap_export_sort(&arena, input, NULL) == 1);
	assert(ap_export_sort(&arena, NULL, &out) == 2 && out == &nil);
	struct ap_data_List invalid = {.tag = 77};
	assert(ap_export_sort(&arena, &invalid, &out) == 2 && out == &nil);
	struct ap_data_List malformed = {.tag = AP_DATA_List_C1, .fields.c1 = {1,NULL}};
	assert(ap_export_sort(&arena, &malformed, &out) == 2 && out == &nil);
	malformed.fields.c1.f1 = &malformed;
	assert(ap_export_sort(&arena, &malformed, &out) == 2 && out == &nil);
	arena.capacity = count + 1;
	assert(ap_export_sort(&arena, input, &out) == 3 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_sort(&arena, input, &out) == 4 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0;
	assert(!ap_export_sort(&arena, input, &out)); equal_list(out, sorted, 5);
	uint32_t buffer[5] = {77,77,77,77,77}; size_t written = 99;
	assert(ap_copy_List(out, buffer, 4, &written) == 6 && written == 99);
	for (size_t i = 0; i < 5; ++i) assert(buffer[i] == 77);
	equal_list(input, values, 5);
	struct ap_enum_Bool flag = {.tag = 77};
	assert(ap_export_fixed_bool(&arena, (struct ap_enum_Bool){2}, &flag) == 2 && flag.tag == 77);
	ap_arena_Nat_destroy(&arena);
	const uint32_t maximum[] = {UINT32_MAX};
	assert(!ap_from_List(&arena, maximum, 1, &input));
	assert(!ap_export_sort(&arena, input, &out)); equal_list(out, maximum, 1);
	const uint32_t deep[] = {UINT32_MAX,UINT32_MAX};
	assert(!ap_from_List(&arena, deep, 2, &input)); mark = arena.first; count = arena.count; out = &nil;
	assert(ap_export_sort(&arena, input, &out) == 4 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	equal_list(input, deep, 2);
	ap_arena_Nat_destroy(&arena);
}

static void print_list(const struct ap_data_List *list)
{
	uint32_t values[6]; size_t written;
	assert(!ap_copy_List(list, values, 6, &written));
	for (size_t i = 0; i < written; ++i) printf("%" PRIu32 ",", values[i]);
	putchar('|');
}

int main(void)
{
	finite_cases(); resource_cases();
	struct ap_c_arena arena = {0};
	const uint32_t mixed[] = {3,0,2,1,2}, descending[] = {3,2,1,0};
	const struct ap_data_List *empty, *input, *out;
	assert(!ap_from_List(&arena, NULL, 0, &empty));
	assert(!ap_export_sort(&arena, empty, &out)); print_list(out);
	assert(!ap_from_List(&arena, mixed, 5, &input));
	assert(!ap_export_sort(&arena, input, &out)); print_list(out);
	const struct ap_data_List *sorted = out;
	assert(!ap_from_List(&arena, descending, 4, &input));
	assert(!ap_export_sort(&arena, input, &out)); print_list(out);
	assert(!ap_from_List(&arena, mixed, 5, &input));
	assert(!ap_export_sort_alias(&arena, input, &out)); print_list(out);
	assert(!ap_export_insert(&arena, 2, sorted, &out)); print_list(out);
	assert(!ap_export_insert(&arena, 0, empty, &out)); print_list(out);
	uint32_t n; struct ap_enum_Bool flag;
	assert(!ap_export_fixed_id(&arena, 3, &n)); printf("%" PRIu32 "|", n);
	assert(!ap_export_compare(&arena, 2, 3, &flag)); printf("%s|", flag.tag == AP_ENUM_Bool_C0 ? "T" : "F");
	assert(!ap_export_compare(&arena, 3, 2, &flag)); printf("%s|", flag.tag == AP_ENUM_Bool_C0 ? "T" : "F");
	ap_arena_Nat_destroy(&arena);
	return 0;
}
