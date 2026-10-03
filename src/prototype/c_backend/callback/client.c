#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

static int32_t signed32(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static int64_t signed64(uint64_t bits)
{
	return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

static int32_t add32(void *context, int32_t value)
{
	const int32_t *offset = context;
	return signed32((uint32_t)value + (uint32_t)*offset);
}

static int32_t neg32(void *context, int32_t value)
{
	(void)context;
	return signed32(UINT32_C(0) - (uint32_t)value);
}

static int64_t add64(void *context, int64_t value)
{
	const int64_t *offset = context;
	return signed64((uint64_t)value + (uint64_t)*offset);
}

int main(void)
{
	assert(AP_C_CALLBACK_ABI == 1);
	int32_t values32[] = {INT32_MIN, INT32_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT32_MAX - 1, INT32_MAX};
	int32_t offsets32[] = {INT32_MIN, -7, 0, 7, INT32_MAX};
	int64_t values64[] = {INT64_MIN, INT64_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT64_MAX - 1, INT64_MAX};
	int64_t offsets64[] = {INT64_MIN, -7, 0, 7, INT64_MAX};
	struct ap_c_callback_i32 negative = {NULL, neg32}, absent32 = {NULL, NULL};
	struct ap_c_callback_i64 absent64 = {NULL, NULL};
	int32_t out = 99;
	int64_t wide = 101;
	assert(ap_export_once32(negative, 0, NULL) == 1);
	assert(ap_export_once64(absent64, 0, NULL) == 1);
	assert(ap_export_once32(absent32, 0, &out) == 2 && out == 99);
	assert(ap_export_compose32(negative, absent32, 0, &out) == 2 && out == 99);
	assert(ap_export_compose32(absent32, negative, 0, &out) == 2 && out == 99);
	assert(ap_export_unused32(absent32, 0, &out) == 2 && out == 99);
	assert(ap_export_once64(absent64, 0, &wide) == 2 && wide == 101);
	for (size_t i = 0; i < 5; ++i) {
		struct ap_c_callback_i32 callback32 = {&offsets32[i], add32};
		struct ap_c_callback_i64 callback64 = {&offsets64[i], add64};
		for (size_t j = 0; j < 10; ++j) {
			int32_t x = values32[j];
			int64_t y = values64[j];
			assert(!ap_export_once32(callback32, x, &out) && out == signed32((uint32_t)x + (uint32_t)offsets32[i]));
			assert(!ap_export_twice32(callback32, x, &out) && out == signed32((uint32_t)x + UINT32_C(2) * (uint32_t)offsets32[i]));
			assert(!ap_export_compose32(callback32, negative, x, &out) && out == signed32((uint32_t)offsets32[i] - (uint32_t)x));
			assert(!ap_export_captured32(callback32, x, &out) && out == signed32((uint32_t)x + UINT32_C(7) + (uint32_t)offsets32[i]));
			assert(!ap_export_unused32(callback32, x, &out) && out == x);
			assert(!ap_export_once64(callback64, y, &wide) && wide == signed64((uint64_t)y + (uint64_t)offsets64[i]));
			assert(!ap_export_twice64(callback64, y, &wide) && wide == signed64((uint64_t)y + UINT64_C(2) * (uint64_t)offsets64[i]));
			assert(!ap_export_captured64(callback64, y, &wide) && wide == signed64(UINT64_C(2) * (uint64_t)y + (uint64_t)offsets64[i]));
		}
	}
	int32_t seven = 7, observations[] = {0, 17, INT32_MIN, INT32_MAX};
	struct ap_c_callback_i32 add = {&seven, add32};
	for (size_t i = 0; i < 4; ++i) {
		int32_t x = observations[i];
		assert(!ap_export_once32(add, x, &out)); printf("%" PRId32 "|", out);
		assert(!ap_export_twice32(add, x, &out)); printf("%" PRId32 "|", out);
		assert(!ap_export_compose32(add, negative, x, &out)); printf("%" PRId32 "|", out);
		assert(!ap_export_captured32(add, x, &out)); printf("%" PRId32 "|", out);
		assert(!ap_export_unused32(add, x, &out)); printf("%" PRId32 "|", out);
	}
	return 0;
}
