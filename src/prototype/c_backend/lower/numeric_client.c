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

static const struct ap_data_Numbers nil = {.tag = AP_DATA_Numbers_C0};

static void node(struct ap_data_Numbers *out, uint32_t n, const struct ap_data_Numbers *tail)
{
	*out = (struct ap_data_Numbers){.tag = AP_DATA_Numbers_C1, .fields.c1 = {n, tail}};
}

static void ordinary(void)
{
	struct ap_c_arena arena = {0};
	for (uint32_t left = 0; left <= 32; ++left) for (uint32_t right = 0; right <= 32; ++right) {
		struct ap_enum_Bool out;
		assert(!ap_export_compare(&arena, left, right, &out));
		assert(out.tag == (left <= right) && !arena.count && !arena.depth);
	}
	size_t cases = 0, combinations = 1;
	for (size_t length = 0; length <= 6; ++length, combinations *= 4) {
		for (size_t code = 0; code < combinations; ++code) for (uint32_t pivot = 0; pivot <= 4; ++pivot) {
			struct ap_data_Numbers nodes[6];
			uint32_t values[6];
			size_t digits = code;
			for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; digits /= 4; }
			const struct ap_data_Numbers *input = &nil;
			for (size_t i = length; i; --i) { node(&nodes[i - 1], values[i - 1], input); input = &nodes[i - 1]; }
			int32_t measured;
			assert(!ap_export_length(&arena, input, &measured) && measured == (int32_t)length);
			for (unsigned upper = 0; upper < 2; ++upper) {
				const struct ap_data_Numbers *selected;
				int status = upper ? ap_export_upper(&arena, input, pivot, &selected) : ap_export_lower(&arena, input, pivot, &selected);
				assert(!status);
				uint32_t buffer[7] = {0};
				size_t written = SIZE_MAX, expected = 0;
				assert(!ap_copy_Numbers(selected, buffer, 6, &written));
				for (size_t i = 0; i < length; ++i)
					if ((values[i] > pivot) == upper) assert(buffer[expected++] == values[i]);
				assert(written == expected && buffer[6] == 0);
			}
			for (size_t i = 0; i < length; ++i) assert(nodes[i].fields.c1.f0 == values[i]);
			ap_arena_Nat_destroy(&arena); ++cases;
		}
	}
	assert(cases == 27305);
}

static void boundaries(void)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Bool answer = {77};
	uint32_t magnitude = 77;
	assert(!ap_export_compare(&arena, 0, UINT32_MAX, &answer) && answer.tag == 1);
	assert(!ap_export_compare(&arena, UINT32_MAX, 0, &answer) && answer.tag == 0);
	answer.tag = 77;
	assert(ap_export_compare(&arena, UINT32_MAX, UINT32_MAX, &answer) == 4 && answer.tag == 77 && !arena.depth);
	arena.depth_limit = 32;
	assert(ap_export_compare(&arena, 32, 32, &answer) == 4 && answer.tag == 77 && !arena.depth);
	arena.depth_limit = 33;
	assert(!ap_export_compare(&arena, 32, 32, &answer) && answer.tag == 1);
	arena.depth_limit = 0;
	assert(!ap_export_increment(&arena, UINT32_MAX - 1, &magnitude) && magnitude == UINT32_MAX);
	assert(ap_export_increment(&arena, UINT32_MAX, &magnitude) == 5 && magnitude == UINT32_MAX);
	assert(!ap_export_reversed_zero(&arena, &magnitude) && magnitude == 0);
	assert(!ap_export_reversed_increment(&arena, 41, &magnitude) && magnitude == 42);
	assert(ap_export_reversed_increment(&arena, UINT32_MAX, &magnitude) == 5 && magnitude == 42);
	assert(ap_export_increment(NULL, 1, &magnitude) == 1 && magnitude == 42);
	assert(ap_export_increment(&arena, 1, NULL) == 1);

	struct ap_data_Numbers first, second;
	node(&second, UINT32_MAX, &nil); node(&first, 0, &second);
	assert(!ap_export_block_callback(&arena, &nil, &magnitude) && magnitude == 1);
	assert(!ap_export_block_callback(&arena, &first, &magnitude) && magnitude == 1);
	second.tag = UINT32_MAX;
	assert(ap_export_block_callback(&arena, &first, &magnitude) == 2 && magnitude == 1);
	second.tag = AP_DATA_Numbers_C1;
	uint32_t buffer[3] = {77, 77, 77};
	size_t written = 77;
	assert(ap_copy_Numbers(&first, buffer, 1, &written) == 6);
	assert(written == 77 && buffer[0] == 77 && buffer[1] == 77 && buffer[2] == 77);
	assert(ap_copy_Numbers(&first, NULL, 2, &written) == 1 && written == 77);
	assert(ap_copy_Numbers(&first, buffer, 3, NULL) == 1 && buffer[0] == 77);
	assert(!ap_copy_Numbers(&first, buffer, 2, &written) && written == 2 && buffer[0] == 0 && buffer[1] == UINT32_MAX && buffer[2] == 77);
	assert(!ap_copy_Numbers(&nil, NULL, 0, &written) && written == 0);
	written = 77; buffer[0] = 77;
	assert(ap_copy_Numbers(NULL, buffer, 3, &written) == 2 && written == 77 && buffer[0] == 77);
	second.tag = UINT32_MAX;
	assert(ap_copy_Numbers(&first, buffer, 3, &written) == 2 && written == 77 && buffer[0] == 77);
	second.tag = AP_DATA_Numbers_C1; second.fields.c1.f1 = NULL;
	assert(ap_copy_Numbers(&first, buffer, 3, &written) == 2 && written == 77 && buffer[0] == 77);
	second.fields.c1.f1 = &first;
	assert(ap_copy_Numbers(&first, buffer, 3, &written) == 2 && written == 77 && buffer[0] == 77);
	second.fields.c1.f1 = &nil;
	second.fields.c1.f0 = 2;

	const struct ap_data_Numbers *out;
	assert(!ap_export_constant(&arena, &out));
	const struct ap_data_Numbers *saved = out;
	struct ap_c_allocation *mark = arena.first;
	out = &first; allocations = 1;
	assert(ap_export_lower(&arena, &first, UINT32_MAX, &out) == 3 && out == &first);
	assert(arena.count == 1 && arena.first == mark && !arena.depth && saved->tag == AP_DATA_Numbers_C0);
	allocations = SIZE_MAX; arena.capacity = 2;
	assert(ap_export_upper(&arena, &first, 0, &out) == 3 && out == &first && arena.count == 1);
	arena.capacity = 0;
	second.fields.c1.f0 = UINT32_MAX;
	assert(ap_export_lower(&arena, &first, UINT32_MAX, &out) == 4 && out == &first && arena.count == 1 && !arena.depth);
	assert(!ap_export_lower(&arena, &first, 0, &out));
	assert(!ap_copy_Numbers(out, buffer, 3, &written) && written == 1 && buffer[0] == 0);
	ap_arena_Nat_destroy(&arena);

	struct ap_data_LongNumbers long_nil = {.tag = AP_DATA_LongNumbers_C1};
	struct ap_data_LongNumbers long_last = {.tag = AP_DATA_LongNumbers_C0, .fields.c0 = {&long_nil, INT64_MAX}};
	struct ap_data_LongNumbers long_first = {.tag = AP_DATA_LongNumbers_C0, .fields.c0 = {&long_last, INT64_MIN}};
	int64_t long_buffer[3] = {77, 77, 77};
	written = 77;
	assert(ap_copy_LongNumbers(&long_first, long_buffer, 1, &written) == 6 && written == 77 && long_buffer[0] == 77);
	assert(!ap_copy_LongNumbers(&long_first, long_buffer, 2, &written));
	assert(written == 2 && long_buffer[0] == INT64_MIN && long_buffer[1] == INT64_MAX && long_buffer[2] == 77);

	struct ap_data_Numbers chain[300];
	uint32_t copied[300];
	const struct ap_data_Numbers *tail = &nil;
	for (size_t i = 300; i; --i) { node(&chain[i - 1], (uint32_t)i, tail); tail = &chain[i - 1]; }
	assert(!ap_copy_Numbers(tail, copied, 300, &written) && written == 300);
	for (size_t i = 0; i < 300; ++i) assert(copied[i] == i + 1);
	int32_t length = 77;
	assert(ap_export_length(&arena, tail, &length) == 4 && length == 77 && !arena.depth);
}

int main(void)
{
	ordinary(); boundaries();
	puts("Native numeric/List client: 1089 comparisons, 27305 stable partitions, checked magnitudes, copy-out and transactional failures passed");
	return 0;
}
