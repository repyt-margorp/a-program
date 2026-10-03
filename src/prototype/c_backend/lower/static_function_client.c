#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static int32_t wrap(int64_t value)
{
	uint32_t bits = (uint32_t)value;
	return (int32_t)(bits <= INT32_MAX ? (int64_t)bits : (int64_t)bits - INT64_C(4294967296));
}

int main(void)
{
	const int32_t values[] = {0, 1, -1, INT32_MIN, INT32_MAX, 7, -9, 42, -99, 305419896};
	int32_t out;
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		int32_t x = values[i];
		assert(!ap_export_closed_block(x, &out) && out == wrap((int64_t)x + 1));
		assert(!ap_export_shadowed_block(x, &out) && out == wrap((int64_t)x + 7));
		assert(!ap_export_curried_block(x, &out) && out == wrap((int64_t)x + 7));
		assert(!ap_export_repeated_block(x, &out) && out == wrap(2 * ((int64_t)x + 1)));
		assert(!ap_export_unused_block(x, &out) && out == x);
		assert(!ap_export_unused_effect(x, &out) && out == x);
		for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
			assert(!ap_export_captured_block(values[j], x, &out) && out == wrap((int64_t)x + values[j]));
			assert(!ap_export_nested_two(values[j], x, &out) && out == wrap((int64_t)x + values[j]));
			assert(!ap_export_nested_three(values[j], x, &out) && out == wrap((int64_t)x + values[j]));
		}
	}
	assert(ap_export_closed_block(1, NULL) == 1);
	assert(ap_export_captured_block(1, 2, NULL) == 1);
	assert(ap_export_shadowed_block(1, NULL) == 1);
	assert(ap_export_curried_block(1, NULL) == 1);
	assert(ap_export_repeated_block(1, NULL) == 1);
	assert(ap_export_unused_block(1, NULL) == 1);
	assert(ap_export_unused_effect(1, NULL) == 1);
	assert(ap_export_nested_two(1, 2, NULL) == 1);
	assert(ap_export_nested_three(1, 2, NULL) == 1);
	assert(ap_export_captured64(1, 2, NULL) == 1);
	const int64_t wide[][3] = {
		{INT64_MIN, INT64_MIN, 0}, {INT64_MAX, 1, INT64_MIN},
		{INT64_MIN, -1, INT64_MAX}, {-1, 1, 0}, {0, INT64_MAX, INT64_MAX}
	};
	int64_t result64;
	for (size_t i = 0; i < sizeof(wide) / sizeof(wide[0]); ++i)
		assert(!ap_export_captured64(wide[i][0], wide[i][1], &result64) && result64 == wide[i][2]);

	assert(!ap_export_closed_block(INT32_MAX, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_captured_block(INT32_MIN, -1, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_shadowed_block(INT32_MIN, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_repeated_block(INT32_MAX, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_curried_block(INT32_MAX, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_unused_block(INT32_MIN, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_nested_two(42, -99, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_unused_effect(123, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_nested_three(42, -99, &out)); printf("%" PRId32 "|", out);
	return 0;
}
