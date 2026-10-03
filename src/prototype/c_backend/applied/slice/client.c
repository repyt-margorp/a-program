#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

void *__real_malloc(size_t);
static size_t attempts, fail_on;
void *__wrap_malloc(size_t size)
{
	++attempts;
	return fail_on && attempts == fail_on ? NULL : __real_malloc(size);
}

static void same(const struct ap_data_Numbers *input, const uint32_t *expected, size_t count)
{
	uint32_t output[6];
	size_t written = 77;
	assert(!ap_copy_Numbers(input, output, 6, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(output[i] == expected[i]);
}

static void ordinary(size_t count, unsigned digits)
{
	struct ap_c_arena input_arena = {0};
	uint32_t values[6];
	for (size_t i = 0; i < count; ++i) { values[i] = digits % 3; digits /= 3; }
	const struct ap_data_Numbers *input, *output;
	assert(!ap_from_Numbers(&input_arena, count ? values : NULL, count, &input));
	uint32_t limits[] = {0, 1, 3, 7, UINT32_MAX};
	for (size_t i = 0; i < 5; ++i) {
		struct ap_c_arena arena = {0};
		size_t prefix = limits[i] < count ? limits[i] : count;
		assert(!ap_export_take(&arena, input, limits[i], &output));
		same(output, values, prefix);
		assert(!ap_export_drop(&arena, input, limits[i], &output));
		same(output, values + prefix, count - prefix);
		ap_arena_Nat_destroy(&arena);
		for (size_t j = 0; j < 5; ++j) {
			arena = (struct ap_c_arena){0};
			size_t length = limits[j] < count - prefix ? limits[j] : count - prefix;
			assert(!ap_export_slice(&arena, input, limits[i], limits[j], &output));
			same(output, values + prefix, length);
			ap_arena_Nat_destroy(&arena);
		}
	}
	same(input, values, count);
	ap_arena_Nat_destroy(&input_arena);
}

static int selected(unsigned operation, struct ap_c_arena *arena,
	const struct ap_data_Numbers *input, const struct ap_data_Numbers **output)
{
	if (!operation) return ap_export_take(arena, input, 2, output);
	if (operation == 1) return ap_export_drop(arena, input, 1, output);
	return ap_export_slice(arena, input, 1, 1, output);
}

static void failures(void)
{
	struct ap_c_arena arena = {0};
	uint32_t values[] = {1, 2, 0};
	const struct ap_data_Numbers *input, *output;
	assert(!ap_from_Numbers(&arena, values, 3, &input));
	struct ap_c_allocation *mark = arena.first;
	size_t saved = arena.count;
	for (unsigned operation = 0; operation < 3; ++operation) {
		struct ap_c_arena trial = {0};
		attempts = 0;
		assert(!selected(operation, &trial, input, &output));
		size_t allocations = attempts;
		assert(allocations && allocations < 16);
		ap_arena_Nat_destroy(&trial);
		for (size_t position = 1; position <= allocations; ++position) {
			attempts = 0; fail_on = position; output = input;
			assert(selected(operation, &arena, input, &output) == 3 && output == input);
			assert(attempts == position && arena.first == mark && arena.count == saved);
			fail_on = 0; same(input, values, 3);
		}
		output = input;
		assert(selected(operation, NULL, input, &output) == 1 && output == input);
		assert(selected(operation, &arena, input, NULL) == 1);
		assert(selected(operation, &arena, NULL, &output) == 2 && output == input);
		assert(arena.first == mark && arena.count == saved);
	}
	arena.capacity = saved + 1; output = input;
	assert(ap_export_slice(&arena, input, 1, 1, &output) == 3 && output == input);
	assert(arena.first == mark && arena.count == saved); arena.capacity = 0;
	arena.depth_limit = 1;
	assert(ap_export_take(&arena, input, 3, &output) == 4 && output == input);
	assert(arena.first == mark && arena.count == saved && !arena.depth);
	arena.depth_limit = 0;
	struct ap_data_Numbers malformed = {.tag = AP_DATA_Numbers_C1, .fields.c1 = {0, NULL}};
	malformed.fields.c1.f1 = &malformed;
	assert(ap_export_slice(&arena, &malformed, 0, 0, &output) == 2 && output == input);
	malformed.tag = UINT32_MAX;
	assert(ap_export_drop(&arena, &malformed, 0, &output) == 2 && output == input);
	assert(arena.first == mark && arena.count == saved);
	ap_arena_Nat_destroy(&arena);
}

static void report(const struct ap_data_Numbers *input)
{
	uint32_t values[6];
	size_t count;
	assert(!ap_copy_Numbers(input, values, 6, &count));
	for (size_t i = 0; i < count; ++i) printf("%" PRIu32 ",", values[i]);
	fputc('|', stdout);
}

static void observations(void)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Numbers *input, *output;
	assert(!ap_from_Numbers(&arena, (uint32_t[]){0, 1, 2}, 3, &input));
	assert(!ap_export_take(&arena, input, 0, &output)); report(output);
	assert(!ap_export_take(&arena, input, 2, &output)); report(output);
	assert(!ap_export_take(&arena, input, 4, &output)); report(output);
	assert(!ap_export_drop(&arena, input, 0, &output)); report(output);
	assert(!ap_export_drop(&arena, input, 1, &output)); report(output);
	assert(!ap_export_drop(&arena, input, 4, &output)); report(output);
	assert(!ap_export_slice(&arena, input, 1, 1, &output)); report(output);
	assert(!ap_export_slice(&arena, input, 1, 4, &output)); report(output);
	assert(!ap_export_slice(&arena, input, 4, 2, &output)); report(output);
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	size_t cases = 0;
	unsigned combinations = 1;
	for (size_t count = 0; count <= 6; ++count, combinations *= 3)
		for (unsigned digits = 0; digits < combinations; ++digits) { ordinary(count, digits); ++cases; }
	assert(cases == 1093);
	failures(); observations();
	return 0;
}
