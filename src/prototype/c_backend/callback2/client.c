#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

struct context { uint32_t small; uint64_t wide; };

static int32_t signed32(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static int64_t signed64(uint64_t bits)
{
	return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

static int32_t subtract32(void *opaque, int32_t x, int32_t y)
{
	const struct context *context = opaque;
	return signed32((uint32_t)x - (uint32_t)y + context->small);
}

static int64_t subtract64(void *opaque, int64_t x, int64_t y)
{
	const struct context *context = opaque;
	return signed64((uint64_t)x - (uint64_t)y + context->wide);
}

static int32_t negative32(void *context, int32_t x)
{
	(void)context;
	return signed32(UINT32_C(0) - (uint32_t)x);
}

int main(void)
{
	assert(AP_C_CALLBACK2_ABI == 1);
	int32_t inputs32[] = {INT32_MIN, INT32_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT32_MAX - 1, INT32_MAX};
	int64_t inputs64[] = {INT64_MIN, INT64_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT64_MAX - 1, INT64_MAX};
	struct context offsets[] = {{(uint32_t)INT32_MIN, (uint64_t)INT64_MIN}, {(uint32_t)-7, (uint64_t)-7}, {0, 0}, {7, 7}, {INT32_MAX, INT64_MAX}};
	struct ap_c_callback_i32 negative = {NULL, negative32}, missing_unary = {NULL, NULL};
	struct ap_c_callback2_i32 missing32 = {NULL, NULL};
	struct ap_c_callback2_i64 missing64 = {NULL, NULL};
	int32_t out32 = 99;
	int64_t out64 = 101;
	assert(ap_export_apply32(missing32, 0, 0, NULL) == 1);
	assert(ap_export_apply64(missing64, 0, 0, NULL) == 1);
	assert(ap_export_apply32(missing32, 0, 0, &out32) == 2 && out32 == 99);
	assert(ap_export_apply64(missing64, 0, 0, &out64) == 2 && out64 == 101);
	assert(ap_export_unused32(missing32, 0, 0, &out32) == 2 && out32 == 99);
	for (size_t i = 0; i < 5; ++i) {
		struct ap_c_callback2_i32 callback32 = {&offsets[i], subtract32};
		struct ap_c_callback2_i64 callback64 = {&offsets[i], subtract64};
		out32 = 99;
		assert(ap_export_mixed_calls32(callback32, missing_unary, 0, 0, &out32) == 2 && out32 == 99);
		for (size_t j = 0; j < 10; ++j) for (size_t k = 0; k < 10; ++k) {
			int32_t x = inputs32[j], y = inputs32[k];
			int64_t a = inputs64[j], b = inputs64[k];
			uint32_t expected32 = (uint32_t)x - (uint32_t)y + offsets[i].small;
			uint64_t expected64 = (uint64_t)a - (uint64_t)b + offsets[i].wide;
			assert(!ap_export_apply32(callback32, x, y, &out32) && (uint32_t)out32 == expected32);
			assert(!ap_export_repeat32(callback32, x, y, &out32) && (uint32_t)out32 == expected32 - (uint32_t)y + offsets[i].small);
			assert(!ap_export_captured32(callback32, x, y, &out32) && (uint32_t)out32 == expected32);
			assert(!ap_export_mixed_calls32(callback32, negative, x, y, &out32) && (uint32_t)out32 == UINT32_C(0) - expected32);
			assert(!ap_export_unused32(callback32, x, y, &out32) && (uint32_t)out32 == (uint32_t)x - (uint32_t)y);
			assert(!ap_export_apply64(callback64, a, b, &out64) && (uint64_t)out64 == expected64);
			assert(!ap_export_repeat64(callback64, a, b, &out64) && (uint64_t)out64 == expected64 - (uint64_t)b + offsets[i].wide);
			assert(!ap_export_captured64(callback64, a, b, &out64) && (uint64_t)out64 == expected64);
		}
	}
	struct context zero = {0, 0};
	struct ap_c_callback2_i32 subtract = {&zero, subtract32};
	int32_t observations[][2] = {{0, 7}, {17, -7}, {INT32_MIN, 1}, {INT32_MAX, -1}};
	for (size_t i = 0; i < 4; ++i) {
		int32_t x = observations[i][0], y = observations[i][1];
		assert(!ap_export_apply32(subtract, x, y, &out32)); printf("%" PRId32 "|", out32);
		assert(!ap_export_repeat32(subtract, x, y, &out32)); printf("%" PRId32 "|", out32);
		assert(!ap_export_captured32(subtract, x, y, &out32)); printf("%" PRId32 "|", out32);
		assert(!ap_export_mixed_calls32(subtract, negative, x, y, &out32)); printf("%" PRId32 "|", out32);
		assert(!ap_export_unused32(subtract, x, y, &out32)); printf("%" PRId32 "|", out32);
	}
	return 0;
}
