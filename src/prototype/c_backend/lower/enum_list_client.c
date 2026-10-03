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

static void same_flags(const struct ap_data_Flags *input, const uint32_t *expected, size_t count)
{
	struct ap_enum_Flag output[16];
	for (size_t i = 0; i < 16; ++i) output[i].tag = UINT32_MAX;
	size_t written = 77;
	assert(!ap_copy_Flags(input, output, 16, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(output[i].tag == expected[i]);
	for (size_t i = count; i < 16; ++i) assert(output[i].tag == UINT32_MAX);
	if (count) {
		struct ap_enum_Flag before[16]; memcpy(before, output, sizeof(output)); written = 77;
		assert(ap_copy_Flags(input, output, count - 1, &written) == 6 && written == 77);
		assert(!memcmp(before, output, sizeof(output)));
	}
}

static void flags_case(size_t count, unsigned bits)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Flag values[8];
	uint32_t expected[16], flipped[8], selected[8];
	int32_t on = 0, result;
	for (size_t i = 0; i < count; ++i) {
		expected[i] = values[i].tag = (bits >> i) & 1;
		flipped[i] = 1 - expected[i]; on += (int32_t)expected[i];
	}
	const struct ap_data_Flags *input, *output;
	assert(!ap_from_Flags(&arena, count ? values : NULL, count, &input));
	for (size_t i = 0; i < count; ++i) values[i].tag = 99;
	same_flags(input, expected, count);
	assert(!ap_export_length(&arena, input, &result) && result == (int32_t)count);
	assert(!ap_export_count_on(&arena, input, &result) && result == on);
	assert(!ap_export_identity(&arena, input, &output) && output == input);
	assert(!ap_export_flip(&arena, input, &output)); same_flags(output, flipped, count);
	for (uint32_t wanted = 0; wanted < 2; ++wanted) {
		size_t used = 0;
		for (size_t i = 0; i < count; ++i) if (expected[i] == wanted) selected[used++] = wanted;
		assert(!ap_export_select(&arena, input, (struct ap_enum_Flag){wanted}, &output));
		same_flags(output, selected, used);
	}
	for (size_t i = 0; i < count; ++i) expected[count + i] = expected[i];
	assert(!ap_export_append(&arena, input, input, &output)); same_flags(output, expected, 2 * count);
	assert(!ap_export_cons(&arena, (struct ap_enum_Flag){1}, input, &output));
	for (size_t i = count; i; --i) expected[i] = expected[i - 1];
	expected[0] = 1;
	same_flags(output, expected, count + 1);
	ap_arena_Flags_destroy(&arena); assert(!arena.first && !arena.count);
}

static void signal_case(size_t count, unsigned digits)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Signal values[5], output[10];
	uint32_t expected[10];
	const int32_t weights[] = {1, 10, 100};
	int32_t score = 0, result;
	for (size_t i = 0; i < count; ++i) {
		expected[i] = values[i].tag = digits % 3; digits /= 3; score += weights[expected[i]];
	}
	const struct ap_data_Signals *input, *twice;
	assert(!ap_from_Signals(&arena, count ? values : NULL, count, &input));
	for (size_t i = 0; i < count; ++i) values[i].tag = 99;
	size_t written = 77;
	assert(!ap_copy_Signals(input, output, 10, &written) && written == count);
	for (size_t i = 0; i < count; ++i) assert(output[i].tag == expected[i]);
	assert(!ap_export_signal_length(&arena, input, &result) && result == (int32_t)count);
	assert(!ap_export_signal_score(&arena, input, &result) && result == score);
	assert(!ap_export_signal_identity(&arena, input, &twice) && twice == input);
	assert(!ap_export_signal_append(&arena, input, input, &twice));
	assert(!ap_copy_Signals(twice, output, 10, &written) && written == 2 * count);
	for (size_t i = 0; i < 2 * count; ++i) assert(output[i].tag == expected[i % count]);
	ap_arena_Flags_destroy(&arena);
}

static void failures(void)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Flag values[4] = {{0}, {1}, {0}, {1}};
	const struct ap_data_Flags *prior, *output;
	assert(!ap_from_Flags(&arena, values, 1, &prior));
	size_t saved = arena.count;
	struct ap_c_allocation *mark = arena.first;
	for (size_t i = 0; i < 4; ++i) {
		uint32_t tag = values[i].tag; values[i].tag = UINT32_MAX;
		output = prior; arena.status = 77; attempts = 0;
		assert(ap_from_Flags(&arena, values, 4, &output) == 2 && output == prior);
		assert(!attempts && arena.count == saved && arena.first == mark && arena.status == 77);
		values[i].tag = tag;
	}
	for (size_t position = 1; position <= 5; ++position) {
		output = prior; attempts = 0; fail_on = position;
		assert(ap_from_Flags(&arena, values, 4, &output) == 3 && output == prior);
		assert(attempts == position && arena.count == saved && arena.first == mark);
		fail_on = 0; same_flags(prior, (uint32_t[]){0}, 1);
	}
	output = prior; arena.capacity = saved + 2;
	assert(ap_from_Flags(&arena, values, 4, &output) == 3 && output == prior);
	assert(arena.count == saved && arena.first == mark); arena.capacity = 0;
	assert(ap_from_Flags(&arena, values, SIZE_MAX, &output) == 6 && output == prior);
	arena.depth = 1;
	assert(ap_from_Flags(&arena, values, 4, &output) == 4 && output == prior);
	arena.depth = 0;
	assert(ap_from_Flags(NULL, values, 4, &output) == 1);
	assert(ap_from_Flags(&arena, NULL, 1, &output) == 1 && output == prior);
	assert(ap_from_Flags(&arena, values, 4, NULL) == 1);
	assert(!ap_from_Flags(&arena, NULL, 0, &output));
	size_t written = 77;
	assert(!ap_copy_Flags(output, NULL, 0, &written) && !written);
	struct ap_enum_Signal signal = {3};
	const struct ap_data_Signals *signals = NULL;
	mark = arena.first; saved = arena.count; attempts = 0;
	assert(ap_from_Signals(&arena, &signal, 1, &signals) == 2 && !signals);
	assert(!attempts && arena.first == mark && arena.count == saved);
	struct ap_data_Flags terminal = {.tag = AP_DATA_Flags_C0};
	struct ap_data_Flags malformed = {.tag = AP_DATA_Flags_C1, .fields.c1 = {{99}, &terminal}};
	struct ap_enum_Flag buffer[2] = {{77}, {88}}, before[2];
	memcpy(before, buffer, sizeof(buffer)); written = 77;
	assert(ap_copy_Flags(&malformed, buffer, 2, &written) == 2 && written == 77);
	assert(!memcmp(before, buffer, sizeof(buffer)));
	malformed.fields.c1.f0.tag = 0; malformed.fields.c1.f1 = &malformed;
	assert(ap_copy_Flags(&malformed, buffer, 2, &written) == 2 && written == 77);
	assert(!memcmp(before, buffer, sizeof(buffer)));
	assert(ap_copy_Flags(NULL, buffer, 2, &written) == 2);
	assert(ap_copy_Flags(prior, NULL, 1, &written) == 1 && written == 77);
	assert(ap_copy_Flags(prior, buffer, 2, NULL) == 1);
	output = prior; attempts = 0;
	assert(ap_export_cons(&arena, (struct ap_enum_Flag){99}, prior, &output) == 2 && output == prior);
	assert(!attempts);
	ap_arena_Flags_destroy(&arena);
}

static void long_array(void)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Flag values[300], output[300];
	for (size_t i = 0; i < 300; ++i) values[i].tag = i % 2;
	const struct ap_data_Flags *input;
	assert(!ap_from_Flags(&arena, values, 300, &input) && arena.count == 301);
	size_t written = 77;
	assert(!ap_copy_Flags(input, output, 300, &written) && written == 300);
	assert(!memcmp(values, output, sizeof(values)));
	int32_t length = 77;
	assert(ap_export_length(&arena, input, &length) == 4 && length == 77 && arena.count == 301);
	ap_arena_Flags_destroy(&arena);
}

static void report_flags(const struct ap_enum_Flag *values, size_t count)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Flags *input, *output;
	int32_t result;
	assert(!ap_from_Flags(&arena, values, count, &input));
	assert(!ap_export_length(&arena, input, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_count_on(&arena, input, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_flip(&arena, input, &output));
	assert(!ap_export_count_on(&arena, output, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_append(&arena, input, input, &output));
	assert(!ap_export_count_on(&arena, output, &result)); printf("%" PRId32 "|", result);
	ap_arena_Flags_destroy(&arena);
}

static void report_signals(const struct ap_enum_Signal *values, size_t count)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Signals *input, *output;
	int32_t result;
	assert(!ap_from_Signals(&arena, values, count, &input));
	assert(!ap_export_signal_length(&arena, input, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_signal_score(&arena, input, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_signal_append(&arena, input, input, &output));
	assert(!ap_export_signal_score(&arena, output, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_signal_length(&arena, output, &result)); printf("%" PRId32 "|", result);
	ap_arena_Flags_destroy(&arena);
}

int main(void)
{
	size_t cases = 0;
	for (size_t count = 0; count <= 8; ++count)
		for (unsigned bits = 0; bits < (1u << count); ++bits) { flags_case(count, bits); ++cases; }
	unsigned possibilities = 1;
	for (size_t count = 0; count <= 5; ++count, possibilities *= 3)
		for (unsigned digits = 0; digits < possibilities; ++digits) { signal_case(count, digits); ++cases; }
	assert(cases == 875); failures(); long_array();
	report_flags(NULL, 0);
	report_flags((struct ap_enum_Flag[]){{0}, {1}, {1}}, 3);
	report_flags((struct ap_enum_Flag[]){{0}, {1}, {0}, {1}}, 4);
	report_flags((struct ap_enum_Flag[]){{1}, {1}, {1}}, 3);
	report_signals(NULL, 0);
	report_signals((struct ap_enum_Signal[]){{0}, {2}, {1}}, 3);
	return 0;
}
