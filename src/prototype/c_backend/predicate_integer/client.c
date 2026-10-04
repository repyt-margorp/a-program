#include "component.h"
#include <assert.h>
#include <inttypes.h>
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

static const struct ap_data_Numbers32 nil32 = {.tag = AP_DATA_Numbers32_C0};
static const struct ap_data_Numbers64 nil64 = {.tag = AP_DATA_Numbers64_C1};

static void ordinary(void)
{
	struct ap_c_arena arena = {0};
	size_t cases = 0, combinations = 1;
	const int32_t digits32[] = {INT32_MIN, -1, 0, INT32_MAX};
	const int64_t digits64[] = {INT64_MIN, -1, 0, INT64_MAX};
	for (size_t length = 0; length <= 4; ++length, combinations *= 4) {
		for (size_t code = 0; code < combinations; ++code) {
			int32_t values32[4], buffer32[5], measured;
			int64_t values64[4], buffer64[5];
			size_t digits = code, written;
			for (size_t i = 0; i < length; ++i) {
				values32[i] = digits32[digits % 4]; values64[i] = digits64[digits % 4]; digits /= 4;
			}
			const struct ap_data_Numbers32 *input32, *out32;
			const struct ap_data_Numbers64 *input64, *out64;
			assert(!ap_from_Numbers32(&arena, values32, length, &input32));
			assert(!ap_from_Numbers64(&arena, values64, length, &input64));
			assert(!ap_export_length32(&arena, input32, &measured) && measured == (int32_t)length);
			assert(!ap_export_length64(&arena, input64, &measured) && measured == (int32_t)length);
			for (unsigned keep = 0; keep < 2; ++keep) for (unsigned form = 0; form < 3; ++form) {
				struct ap_enum_Bool flag = {keep};
				int status32, status64;
				if (!form) {
					status32 = keep ? ap_export_direct_keep32(&arena, input32, &out32) : ap_export_direct_drop32(&arena, input32, &out32);
					status64 = keep ? ap_export_direct_keep64(&arena, input64, &out64) : ap_export_direct_drop64(&arena, input64, &out64);
				} else if (form == 1) {
					status32 = ap_export_direct_filter32(&arena, flag, input32, &out32);
					status64 = ap_export_direct_filter64(&arena, flag, input64, &out64);
				} else {
					status32 = ap_export_direct_select32(&arena, flag, input32, INT32_MIN, &out32);
					status64 = ap_export_direct_select64(&arena, flag, input64, INT64_MAX, &out64);
				}
				assert(!status32 && !status64 && !arena.depth);
				buffer32[4] = 77; buffer64[4] = 77;
				assert(!ap_copy_Numbers32(out32, buffer32, 4, &written) && written == (keep ? length : 0));
				if (keep) for (size_t i = 0; i < length; ++i) assert(buffer32[i] == values32[i]);
				assert(!ap_copy_Numbers64(out64, buffer64, 4, &written) && written == (keep ? length : 0));
				if (keep) for (size_t i = 0; i < length; ++i) assert(buffer64[i] == values64[i]);
				assert(buffer32[4] == 77 && buffer64[4] == 77);
			}
			assert(!ap_copy_Numbers32(input32, buffer32, 4, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer32[i] == values32[i]);
			assert(!ap_copy_Numbers64(input64, buffer64, 4, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer64[i] == values64[i]);
			ap_arena_Numbers32_destroy(&arena); ++cases;
		}
	}
	assert(cases == 341);
}

static void boundaries(void)
{
	struct ap_c_arena arena = {0};
	struct ap_enum_Bool keep = {AP_ENUM_Bool_C1}, invalid = {UINT32_MAX};
	const int32_t values32[] = {INT32_MIN, 0, INT32_MAX};
	const int64_t values64[] = {INT64_MAX, 0, INT64_MIN};
	const struct ap_data_Numbers32 *input32, *out32 = &nil32;
	const struct ap_data_Numbers64 *input64, *out64 = &nil64;
	assert(!ap_from_Numbers32(&arena, values32, 3, &input32));
	assert(!ap_from_Numbers64(&arena, values64, 3, &input64));
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	assert(ap_export_direct_filter32(NULL, keep, input32, &out32) == 1 && out32 == &nil32);
	assert(ap_export_direct_filter64(&arena, keep, input64, NULL) == 1);
	assert(ap_export_direct_filter32(&arena, invalid, &nil32, &out32) == 2 && out32 == &nil32);
	assert(ap_export_direct_select64(&arena, invalid, &nil64, 0, &out64) == 2 && out64 == &nil64);
	assert(ap_export_direct_keep32(&arena, NULL, &out32) == 2 && out32 == &nil32);
	struct ap_data_Numbers32 damaged32 = {.tag = AP_DATA_Numbers32_C1, .fields.c1 = {1, NULL}};
	struct ap_data_Numbers64 damaged64 = {.tag = AP_DATA_Numbers64_C0, .fields.c0 = {NULL, 1}};
	assert(ap_export_direct_drop32(&arena, &damaged32, &out32) == 2 && out32 == &nil32);
	damaged32.fields.c1.f1 = &damaged32;
	assert(ap_export_direct_filter32(&arena, keep, &damaged32, &out32) == 2 && out32 == &nil32);
	damaged64.fields.c0.f0 = &damaged64;
	assert(ap_export_direct_select64(&arena, keep, &damaged64, 0, &out64) == 2 && out64 == &nil64);
	damaged64.tag = UINT32_MAX;
	assert(ap_export_direct_drop64(&arena, &damaged64, &out64) == 2 && out64 == &nil64);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	allocations = 1;
	assert(ap_export_direct_keep32(&arena, input32, &out32) == 3 && out32 == &nil32);
	assert(ap_export_direct_keep64(&arena, input64, &out64) == 3 && out64 == &nil64);
	allocations = SIZE_MAX;
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = count + 1;
	assert(ap_export_direct_filter64(&arena, keep, input64, &out64) == 3 && out64 == &nil64);
	arena.capacity = 0; arena.depth_limit = 3;
	assert(ap_export_direct_filter32(&arena, keep, input32, &out32) == 4 && out32 == &nil32);
	assert(ap_export_direct_select64(&arena, keep, input64, INT64_MIN, &out64) == 4 && out64 == &nil64);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 4;
	assert(!ap_export_direct_filter32(&arena, keep, input32, &out32));
	assert(!ap_export_direct_select64(&arena, keep, input64, INT64_MIN, &out64));
	int32_t copied32[4] = {77, 77, 77, 77};
	int64_t copied64[4] = {77, 77, 77, 77};
	size_t written = 77;
	assert(ap_copy_Numbers32(out32, copied32, 2, &written) == 6 && written == 77 && copied32[0] == 77);
	assert(!ap_copy_Numbers32(out32, copied32, 3, &written) && written == 3);
	assert(!ap_copy_Numbers64(out64, copied64, 3, &written) && written == 3);
	for (size_t i = 0; i < 3; ++i) assert(copied32[i] == values32[i] && copied64[i] == values64[i]);
	assert(copied32[3] == 77 && copied64[3] == 77);
	ap_arena_Numbers32_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.depth);
}

static void reference(void)
{
	struct ap_c_arena arena = {0};
	const int32_t values[] = {-1, 0, 1};
	const struct ap_data_Numbers32 *input, *out;
	assert(!ap_from_Numbers32(&arena, values, 3, &input));
	for (unsigned form = 0; form < 3; ++form) for (unsigned drop = 0; drop < 2; ++drop) {
		struct ap_enum_Bool flag = {!drop};
		int status;
		if (!form) status = drop ? ap_export_direct_drop32(&arena, input, &out) : ap_export_direct_keep32(&arena, input, &out);
		else if (form == 1) status = ap_export_direct_filter32(&arena, flag, input, &out);
		else status = ap_export_direct_select32(&arena, flag, input, 0, &out);
		assert(!status);
		int32_t copied[3];
		size_t written;
		assert(!ap_copy_Numbers32(out, copied, 3, &written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",", copied[i]);
		putchar('|');
	}
	ap_arena_Numbers32_destroy(&arena);
}

int main(void)
{
	ordinary(); boundaries(); reference();
	return 0;
}
