#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void equal_list(const struct ap_data_List *list, const uint32_t *values, size_t length)
{
	uint32_t buffer[5] = {77,77,77,77,77}; size_t written = 99;
	assert(!ap_copy_List(list, buffer, 5, &written) && written == length);
	for (size_t i = 0; i < length; ++i) assert(buffer[i] == values[i]);
	for (size_t i = length; i < 5; ++i) assert(buffer[i] == 77);
}

static void finite_cases(void)
{
	struct ap_c_arena arena = {0}; size_t combinations = 1, cases = 0;
	for (size_t length = 0; length <= 4; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		uint32_t values[4]; size_t digits = code;
		for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; digits /= 4; }
		const struct ap_data_List *input, *out;
		assert(!ap_from_List(&arena, values, length, &input));
		arena.depth_limit = 1;
		assert(!ap_export_fixed_list(&arena, input, &out) && out == input);
		assert(!ap_export_alias_list(&arena, input, &out) && out == input);
		assert(!ap_export_shadow_list(&arena, input, &out) && out == input);
		arena.depth_limit = 0;
		assert(!ap_export_map_list(&arena, input, &out)); equal_list(out, values, length);
		assert(!ap_export_map_inline(&arena, input, &out)); equal_list(out, values, length);
		equal_list(input, values, length);
		assert(!arena.depth); ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 341);
	const uint32_t numbers[] = {0,1,2,17,UINT32_MAX};
	struct ap_data_Pair empty = {.tag = AP_DATA_Pair_C0}, out;
	empty.fields.c1.f1.tag = 77;
	assert(!ap_export_fixed_pair(&arena, empty, &out) && out.tag == AP_DATA_Pair_C0);
	for (size_t i = 0; i < sizeof(numbers) / sizeof(*numbers); ++i) {
		uint32_t n;
		assert(!ap_export_fixed_nat(&arena, numbers[i], &n) && n == numbers[i]);
		for (unsigned flag = 0; flag < 2; ++flag) {
			struct ap_data_Pair input = {.tag = AP_DATA_Pair_C1, .fields.c1 = {numbers[i],{flag}}};
			assert(!ap_export_fixed_pair(&arena, input, &out) && out.tag == input.tag);
			assert(out.fields.c1.f0 == numbers[i] && out.fields.c1.f1.tag == flag);
		}
	}
	const uint32_t maximum[] = {UINT32_MAX}; const struct ap_data_List *input, *list;
	assert(!ap_from_List(&arena, maximum, 1, &input));
	assert(!ap_export_map_list(&arena, input, &list)); equal_list(list, maximum, 1);
	ap_arena_Nat_destroy(&arena);
}

static void resource_cases(void)
{
	struct ap_c_arena arena = {0};
	static const struct ap_data_List nil = {.tag = AP_DATA_List_C0};
	const uint32_t values[] = {3,0,2,1}; const struct ap_data_List *input, *out = &nil;
	assert(!ap_from_List(&arena, values, 4, &input));
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	assert(ap_export_fixed_list(NULL, input, &out) == 1 && out == &nil);
	assert(ap_export_fixed_list(&arena, input, NULL) == 1);
	assert(ap_export_fixed_list(&arena, NULL, &out) == 2 && out == &nil);
	struct ap_data_List invalid = {.tag = 77};
	assert(ap_export_fixed_list(&arena, &invalid, &out) == 2 && out == &nil);
	struct ap_data_List cycle = {.tag = AP_DATA_List_C1, .fields.c1 = {1,NULL}};
	assert(ap_export_fixed_list(&arena, &cycle, &out) == 2 && out == &nil);
	cycle.fields.c1.f1 = &cycle;
	assert(ap_export_map_list(&arena, &cycle, &out) == 2 && out == &nil);
	arena.capacity = count + 1;
	assert(ap_export_map_list(&arena, input, &out) == 3 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	assert(ap_export_map_inline(&arena, input, &out) == 3 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_map_list(&arena, input, &out) == 4 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	assert(ap_export_map_inline(&arena, input, &out) == 4 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0;
	assert(!ap_export_map_list(&arena, input, &out)); equal_list(out, values, 4);
	uint32_t buffer[4] = {77,77,77,77}; size_t written = 99;
	assert(ap_copy_List(out, buffer, 3, &written) == 6 && written == 99);
	for (size_t i = 0; i < 4; ++i) assert(buffer[i] == 77);
	equal_list(input, values, 4);
	struct ap_data_Pair pair = {.tag = 77}, result = {.tag = 88};
	assert(ap_export_fixed_pair(&arena, pair, &result) == 2 && result.tag == 88);
	pair.tag = AP_DATA_Pair_C1; pair.fields.c1.f0 = 3; pair.fields.c1.f1.tag = 2;
	assert(ap_export_fixed_pair(&arena, pair, &result) == 2 && result.tag == 88);
	pair.fields.c1.f1.tag = 0;
	assert(ap_export_fixed_pair(NULL, pair, &result) == 1 && result.tag == 88);
	assert(ap_export_fixed_pair(&arena, pair, NULL) == 1);
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	finite_cases(); resource_cases();
	struct ap_c_arena arena = {0}; uint32_t n;
	const uint32_t values[] = {0,1}; const struct ap_data_List *input, *out;
	assert(!ap_from_List(&arena, values, 2, &input));
	assert(!ap_export_fixed_nat(&arena, 1, &n)); printf("%" PRIu32, n);
	assert(!ap_export_fixed_list(&arena, input, &out)); equal_list(out, values, 2); putchar('2');
	assert(!ap_export_alias_list(&arena, input, &out)); equal_list(out, values, 2); putchar('2');
	assert(!ap_export_map_list(&arena, input, &out)); equal_list(out, values, 2); putchar('2');
	assert(!ap_export_map_inline(&arena, input, &out)); equal_list(out, values, 2); putchar('2');
	assert(!ap_export_shadow_list(&arena, input, &out)); equal_list(out, values, 2); putchar('2');
	struct ap_data_Pair pair = {.tag = AP_DATA_Pair_C0}, result;
	assert(!ap_export_fixed_pair(&arena, pair, &result) && result.tag == AP_DATA_Pair_C0); putchar('0');
	pair.tag = AP_DATA_Pair_C1; pair.fields.c1.f0 = 1; pair.fields.c1.f1.tag = AP_ENUM_Bool_C1;
	assert(!ap_export_fixed_pair(&arena, pair, &result));
	printf("%" PRIu32, result.fields.c1.f0 * 2 + result.fields.c1.f1.tag + 1);
	ap_arena_Nat_destroy(&arena);
	return 0;
}
