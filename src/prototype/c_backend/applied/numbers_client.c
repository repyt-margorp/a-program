#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void *__real_malloc(size_t);
static size_t attempts, fail_on;
void *__wrap_malloc(size_t size)
{
	++attempts;
	return fail_on && attempts == fail_on ? NULL : __real_malloc(size);
}

static void same(const struct ap_data_Numbers *input, const uint32_t *expected, size_t count)
{
	uint32_t output[16];
	for (size_t i = 0; i < 16; ++i) output[i] = UINT32_MAX;
	size_t written = 77;
	assert(!ap_copy_Numbers(input, output, 16, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(output[i] == expected[i]);
	for (size_t i = count; i < 16; ++i) assert(output[i] == UINT32_MAX);
}

static void ordinary(size_t count, unsigned digits)
{
	struct ap_c_arena arena = {0};
	uint32_t values[6], expected[12];
	for (size_t i = 0; i < count; ++i) { values[i] = expected[i] = digits % 3; digits /= 3; }
	const struct ap_data_Numbers *input, *output;
	assert(!ap_from_Numbers(&arena, count ? values : NULL, count, &input));
	for (size_t i = 0; i < count; ++i) values[i] = 99;
	same(input, expected, count);
	int32_t length;
	assert(!ap_export_length(&arena, input, &length) && length == (int32_t)count);
	assert(!ap_export_identity(&arena, input, &output) && output == input);
	for (size_t i = 0; i < count; ++i) expected[count + i] = expected[i];
	assert(!ap_export_append(&arena, input, input, &output)); same(output, expected, 2 * count);
	assert(!ap_export_prepend(&arena, UINT32_MAX, input, &output));
	for (size_t i = count; i; --i) expected[i] = expected[i - 1];
	expected[0] = UINT32_MAX; same(output, expected, count + 1);
	ap_arena_Nat_destroy(&arena);
}

static void failures(void)
{
	struct ap_c_arena arena = {0};
	uint32_t values[] = {0, UINT32_MAX, 2};
	const struct ap_data_Numbers *prior, *output;
	assert(!ap_from_Numbers(&arena, values, 3, &prior));
	struct ap_c_allocation *mark = arena.first;
	size_t saved = arena.count;
	/* Append shares its second input and allocates only the first input's cells. */
	for (size_t position = 1; position <= 3; ++position) {
		output = prior; attempts = 0; fail_on = position;
		assert(ap_export_append(&arena, prior, prior, &output) == 3 && output == prior);
		assert(attempts == position && arena.first == mark && arena.count == saved);
		fail_on = 0; same(prior, values, 3);
	}
	output = prior; arena.capacity = saved + 2;
	assert(ap_export_append(&arena, prior, prior, &output) == 3 && output == prior);
	assert(arena.first == mark && arena.count == saved); arena.capacity = 0;
	uint32_t buffer[3] = {77, 88, 99}, before[3]; memcpy(before, buffer, sizeof(buffer));
	size_t written = 77;
	assert(ap_copy_Numbers(prior, buffer, 2, &written) == 6 && written == 77);
	assert(!memcmp(before, buffer, sizeof(buffer)));
	assert(ap_export_identity(&arena, NULL, &output) == 2 && output == prior);
	assert(ap_export_prepend(NULL, 0, prior, &output) == 1 && output == prior);
	assert(ap_export_prepend(&arena, 0, prior, NULL) == 1);
	struct ap_data_Numbers malformed = {.tag = AP_DATA_Numbers_C1, .fields.c1 = {0, NULL}};
	malformed.fields.c1.f1 = &malformed;
	int32_t length = 77;
	assert(ap_export_length(&arena, &malformed, &length) == 2 && length == 77);
	malformed.tag = UINT32_MAX;
	assert(ap_export_length(&arena, &malformed, &length) == 2 && length == 77);
	arena.depth_limit = 3;
	assert(ap_export_length(&arena, prior, &length) == 4 && length == 77 && !arena.depth);
	arena.depth_limit = 4;
	assert(!ap_export_length(&arena, prior, &length) && length == 3);
	ap_arena_Nat_destroy(&arena);
	uint32_t large[300];
	for (size_t i = 0; i < 300; ++i) large[i] = (uint32_t)i;
	arena = (struct ap_c_arena){0};
	assert(!ap_from_Numbers(&arena, large, 300, &output));
	assert(!ap_copy_Numbers(output, large, 300, &written) && written == 300);
	length = 77;
	assert(ap_export_length(&arena, output, &length) == 4 && length == 77);
	ap_arena_Nat_destroy(&arena);
}

static void report(const uint32_t *values, size_t count)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Numbers *input, *output;
	int32_t length;
	assert(!ap_from_Numbers(&arena, values, count, &input));
	assert(!ap_export_length(&arena, input, &length)); printf("%" PRId32 " ", length);
	assert(!ap_export_append(&arena, input, input, &output));
	assert(!ap_export_length(&arena, output, &length)); printf("%" PRId32 " ", length);
	assert(!ap_export_prepend(&arena, 1, input, &output));
	assert(!ap_export_length(&arena, output, &length)); printf("%" PRId32 "|", length);
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	size_t cases = 0;
	unsigned combinations = 1;
	for (size_t count = 0; count <= 6; ++count, combinations *= 3)
		for (unsigned digits = 0; digits < combinations; ++digits) { ordinary(count, digits); ++cases; }
	assert(cases == 1093); failures();
	report(NULL, 0); report((uint32_t[]){2, 0, 1}, 3); report((uint32_t[]){1, 1}, 2);
	return 0;
}
