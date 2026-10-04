#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static size_t attempts, fail_on;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	return ++attempts == fail_on ? NULL : __real_malloc(size);
}

struct context { unsigned upper; };
static struct ap_enum_Bool positive32(void *context, int32_t n)
{
	(void)context; return (struct ap_enum_Bool){n > 0};
}
static struct ap_enum_Bool positive64(void *context, int64_t n)
{
	(void)context; return (struct ap_enum_Bool){n > 0};
}
static struct ap_enum_Bool compare32(void *context, int32_t left, int32_t right)
{
	const struct context *c = context; return (struct ap_enum_Bool){c && c->upper ? left > right : left <= right};
}
static struct ap_enum_Bool compare64(void *context, int64_t left, int64_t right)
{
	const struct context *c = context; return (struct ap_enum_Bool){c && c->upper ? left > right : left <= right};
}
static struct ap_enum_Reverse reversed(void *context, int32_t n)
{
	(void)context; return (struct ap_enum_Reverse){n > 0 ? 0 : 1};
}
static struct ap_enum_Bool constant32(void *context, int32_t n)
{
	(void)n; return (struct ap_enum_Bool){*(const unsigned *)context};
}
static struct ap_enum_Bool constant_pair32(void *context, int32_t left, int32_t right)
{
	(void)left; return constant32(context, right);
}

/* Foreign result violation is deliberate; no source purity claim is made. */
static struct ap_enum_Bool invalid32(void *context, int32_t n)
{
	return (struct ap_enum_Bool){n == *(const int32_t *)context ? UINT32_MAX : 1};
}
static struct ap_enum_Bool invalid64(void *context, int64_t n)
{
	return (struct ap_enum_Bool){n == *(const int64_t *)context ? UINT32_MAX : 1};
}
static struct ap_enum_Bool invalid_pair64(void *context, int64_t left, int64_t right)
{
	(void)right; return invalid64(context, left);
}

/* These foreign signed comparisons test the ABI and generic source recurrence.
	* Admitted constant source predicates are compared separately below/Core. */
static void ordinary(void)
{
	const int32_t values32[] = {INT32_MIN,-1,0,1,INT32_MAX}, pivots32[] = {INT32_MIN,-2,-1,0,2,INT32_MAX};
	const int64_t values64[] = {INT64_MIN,-1,0,1,INT64_MAX}, pivots64[] = {INT64_MIN,-2,-1,0,2,INT64_MAX};
	struct ap_c_arena arena = {0}; struct context context = {0};
	struct ap_c_predicate_signed1_i32_r4_Bool unary32 = {NULL,positive32};
	struct ap_c_predicate_signed1_i64_r4_Bool unary64 = {NULL,positive64};
	struct ap_c_predicate_signed2_i32_r4_Bool binary32 = {&context,compare32};
	struct ap_c_predicate_signed2_i64_r4_Bool binary64 = {&context,compare64};
	struct ap_c_predicate_signed1_i32_r7_Reverse reverse = {NULL,reversed};
	struct ap_enum_Bool answer; struct ap_enum_Reverse other;
	for (size_t i = 0; i < 5; ++i) {
		assert(!ap_export_apply32(&arena, unary32, values32[i], &answer) && answer.tag == (values32[i] > 0));
		assert(!ap_export_apply64(&arena, unary64, values64[i], &answer) && answer.tag == (values64[i] > 0));
		assert(!ap_export_reverse32(&arena, reverse, values32[i], &other) && other.tag == (values32[i] <= 0));
		for (size_t j = 0; j < 5; ++j) {
			assert(!ap_export_choose32(&arena, binary32, values32[i], values32[j], &answer) && answer.tag == (values32[i] <= values32[j]));
			assert(!ap_export_choose64(&arena, binary64, values64[i], values64[j], &answer) && answer.tag == (values64[i] <= values64[j]));
		}
	}
	size_t cases = 0, partitions = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 5)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t input32[4], out32[4]; int64_t input64[4], out64[4]; size_t digits = code, written;
			for (size_t i = 0; i < length; ++i, digits /= 5) { input32[i] = values32[digits % 5]; input64[i] = values64[digits % 5]; }
			const struct ap_data_Numbers32 *list32, *result32; const struct ap_data_Numbers64 *list64, *result64;
			assert(!ap_from_Numbers32(&arena, input32, length, &list32));
			assert(!ap_from_Numbers64(&arena, input64, length, &list64));
			struct ap_c_allocation *prior = arena.first; size_t count = arena.count;
			int32_t measured; assert(!ap_export_length32(&arena, list32, &measured) && measured == (int32_t)length);
			assert(!ap_export_length64(&arena, list64, &measured) && measured == (int32_t)length);
			assert(!ap_export_filter32(&arena, unary32, list32, &result32));
			assert(!ap_copy_Numbers32(result32, out32, 4, &written)); size_t expected = 0;
			for (size_t i = 0; i < length; ++i) if (input32[i] > 0) assert(out32[expected++] == input32[i]);
			assert(written == expected);
			assert(!ap_export_filter64(&arena, unary64, list64, &result64));
			assert(!ap_copy_Numbers64(result64, out64, 4, &written)); expected = 0;
			for (size_t i = 0; i < length; ++i) if (input64[i] > 0) assert(out64[expected++] == input64[i]);
			assert(written == expected);
			for (size_t pivot = 0; pivot < 6; ++pivot) for (unsigned upper = 0; upper < 2; ++upper) {
				context.upper = upper;
				assert(!ap_export_select32(&arena, binary32, list32, pivots32[pivot], &result32));
				assert(!ap_copy_Numbers32(result32, out32, 4, &written)); expected = 0;
				for (size_t i = 0; i < length; ++i) if ((input32[i] > pivots32[pivot]) == upper) assert(out32[expected++] == input32[i]);
				assert(written == expected);
				assert(!ap_export_select64(&arena, binary64, list64, pivots64[pivot], &result64));
				assert(!ap_copy_Numbers64(result64, out64, 4, &written)); expected = 0;
				for (size_t i = 0; i < length; ++i) if ((input64[i] > pivots64[pivot]) == upper) assert(out64[expected++] == input64[i]);
				assert(written == expected && context.upper == upper && !arena.depth); partitions += 2;
			}
			assert(prior && arena.count >= count);
			assert(!ap_copy_Numbers32(list32, out32, 4, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(out32[i] == input32[i]);
			assert(!ap_copy_Numbers64(list64, out64, 4, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(out64[i] == input64[i]);
			ap_arena_Numbers32_destroy(&arena); ++cases;
		}
	assert(cases == 781 && partitions == 18744);
}

static void failures(void)
{
	struct ap_c_arena arena = {0}; const struct ap_data_Numbers32 *list32, *out32;
	const struct ap_data_Numbers64 *list64, *out64;
	int32_t input32[] = {-1,0,1}; int64_t input64[] = {INT64_MIN,0,INT64_MAX};
	assert(!ap_from_Numbers32(&arena, input32, 3, &list32)); assert(!ap_from_Numbers64(&arena, input64, 3, &list64));
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	int32_t bad32 = 7; int64_t bad64 = INT64_MAX;
	struct ap_c_predicate_signed1_i32_r4_Bool unary32 = {&bad32,invalid32};
	struct ap_c_predicate_signed1_i64_r4_Bool unary64 = {&bad64,invalid64};
	struct ap_c_predicate_signed2_i64_r4_Bool binary64 = {&bad64,invalid_pair64};
	out32 = list32; out64 = list64;
	assert(ap_export_post_check32(&arena, unary32, list32, bad32, &out32) == 2 && out32 == list32);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	bad32 = 0;
	assert(ap_export_filter32(&arena, unary32, list32, &out32) == 2 && out32 == list32);
	assert(ap_export_filter64(&arena, unary64, list64, &out64) == 2 && out64 == list64);
	assert(ap_export_select64(&arena, binary64, list64, 0, &out64) == 2 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	struct ap_enum_Bool flag = {77};
	assert(ap_export_apply64(&arena, unary64, bad64, &flag) == 2 && flag.tag == 77);
	unary32.call = NULL; assert(ap_export_unused32(&arena, unary32, 0, &flag) == 2 && flag.tag == 77);
	assert(ap_export_apply32(NULL, unary32, 0, &flag) == 1);
	assert(ap_export_apply32(&arena, unary32, 0, NULL) == 1);
	unary32 = (struct ap_c_predicate_signed1_i32_r4_Bool){NULL,positive32};
	arena.capacity = count + 1;
	assert(ap_export_filter32(&arena, unary32, list32, &out32) == 3 && out32 == list32);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.capacity = 0;
#ifndef SIGNED_SHARED_PRODUCT
	attempts = 0; assert(!ap_export_filter32(&arena, unary32, list32, &out32)); size_t calls = attempts;
	mark = arena.first; count = arena.count;
	for (size_t i = 1; i <= calls; ++i) {
		attempts = 0; fail_on = i; out32 = list32;
		assert(ap_export_filter32(&arena, unary32, list32, &out32) == 3 && out32 == list32);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_on = 0;
	}
#endif
	arena.depth_limit = 1; out32 = list32;
	assert(ap_export_filter32(&arena, unary32, list32, &out32) == 4 && out32 == list32);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.depth_limit = 0;
	struct ap_data_Numbers64 damaged = {.tag = 99}; out64 = list64;
	unary64 = (struct ap_c_predicate_signed1_i64_r4_Bool){NULL,positive64};
	assert(ap_export_filter64(&arena, unary64, &damaged, &out64) == 2 && out64 == list64);
	damaged = (struct ap_data_Numbers64){.tag = 0, .fields.c0 = {NULL,1}};
	assert(ap_export_filter64(&arena, unary64, &damaged, &out64) == 2 && out64 == list64);
	damaged.fields.c0.f0 = &damaged;
	assert(ap_export_filter64(&arena, unary64, &damaged, &out64) == 2 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
	int64_t deep[300] = {0}; assert(!ap_from_Numbers64(&arena, deep, 300, &list64));
	mark = arena.first; count = arena.count; out64 = list64;
	assert(ap_export_filter64(&arena, unary64, list64, &out64) == 4 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
}

static void reference(void)
{
	struct ap_c_arena arena = {0}; unsigned truth = 1;
	struct ap_c_predicate_signed1_i32_r4_Bool unary = {&truth,constant32};
	struct ap_c_predicate_signed2_i32_r4_Bool binary = {&truth,constant_pair32};
	struct ap_enum_Bool answer;
	assert(!ap_export_apply32(&arena, unary, INT32_MIN, &answer)); printf("%c|", answer.tag ? 'T' : 'F');
	truth = 0; assert(!ap_export_apply32(&arena, unary, INT32_MAX, &answer)); printf("%c|", answer.tag ? 'T' : 'F');
	truth = 1; assert(!ap_export_choose32(&arena, binary, -1, 1, &answer)); printf("%c|", answer.tag ? 'T' : 'F');
	truth = 0; assert(!ap_export_choose32(&arena, binary, 1, -1, &answer)); printf("%c|", answer.tag ? 'T' : 'F');
	int32_t values[] = {-1,0,1}, buffer[3]; const struct ap_data_Numbers32 *input, *out; size_t written;
	assert(!ap_from_Numbers32(&arena, values, 3, &input));
	for (unsigned kind = 0; kind < 2; ++kind) for (unsigned keep = 1;; keep = 0) {
		truth = keep;
		if (!kind) assert(!ap_export_filter32(&arena, unary, input, &out));
		else assert(!ap_export_select32(&arena, binary, input, 0, &out));
		assert(!ap_copy_Numbers32(out, buffer, 3, &written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",", buffer[i]);
		putchar('|'); if (!keep) break;
	}
	ap_arena_Numbers32_destroy(&arena);
}

int main(void)
{
	ordinary(); failures(); reference(); return 0;
}
