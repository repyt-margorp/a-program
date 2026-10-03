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
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
		for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
			int32_t offset = values[i], x = values[j];
			int32_t expected = wrap((int64_t)offset + x);
			assert(!ap_export_chain_three(offset, x, &out) && out == expected);
			assert(!ap_export_chain_four(offset, x, &out) && out == expected);
			assert(!ap_export_chain_eight(offset, x, &out) && out == expected);
			assert(!ap_export_shadowed(offset, x, &out) && out == expected);
			assert(!ap_export_repeated(offset, x, &out) && out == wrap(2 * (int64_t)expected));
			assert(!ap_export_unused_effect(offset, x, &out) && out == x);
		}
	assert(ap_export_chain_three(1, 2, NULL) == 1);
	assert(ap_export_chain_four(1, 2, NULL) == 1);
	assert(ap_export_chain_eight(1, 2, NULL) == 1);
	assert(ap_export_shadowed(1, 2, NULL) == 1);
	assert(ap_export_repeated(1, 2, NULL) == 1);
	assert(ap_export_unused_effect(1, 2, NULL) == 1);
	assert(ap_export_chain64(1, 2, NULL) == 1);
	const int64_t wide[][3] = {
		{INT64_MIN, INT64_MIN, 0}, {INT64_MAX, 1, INT64_MIN},
		{INT64_MIN, -1, INT64_MAX}, {-1, 1, 0}, {0, INT64_MAX, INT64_MAX},
		{INT64_MAX, INT64_MAX, -2}, {INT64_MIN, 0, INT64_MIN}
	};
	int64_t result64;
	for (size_t i = 0; i < sizeof(wide) / sizeof(wide[0]); ++i)
		assert(!ap_export_chain64(wide[i][0], wide[i][1], &result64) && result64 == wide[i][2]);

	assert(!ap_export_chain_three(INT32_MIN, -1, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_chain_four(INT32_MAX, 1, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_chain_eight(42, -99, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_shadowed(7, -9, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_repeated(INT32_MAX, 1, &out)); printf("%" PRId32 "|", out);
	assert(!ap_export_unused_effect(42, -99, &out)); printf("%" PRId32 "|", out);
	return 0;
}
