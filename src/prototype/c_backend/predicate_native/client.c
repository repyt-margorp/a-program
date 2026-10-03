#include "component.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t allocations = SIZE_MAX, allocation_calls;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	++allocation_calls;
	if (allocations == SIZE_MAX) return __real_malloc(size);
	if (!allocations) return NULL;
	--allocations;
	return __real_malloc(size);
}

struct context { uint32_t threshold; unsigned upper; };

static struct ap_enum_Bool keep(void *opaque, uint32_t n)
{
	const struct context *context = opaque;
	return (struct ap_enum_Bool){n > (context ? context->threshold : 0)};
}

static struct ap_enum_Bool compare(void *opaque, uint32_t left, uint32_t right)
{
	const struct context *context = opaque;
	return (struct ap_enum_Bool){context && context->upper ? left > right : left <= right};
}

static struct ap_enum_Reverse reverse(void *opaque, uint32_t n)
{
	(void)opaque;
	return (struct ap_enum_Reverse){n ? AP_ENUM_Reverse_C0 : AP_ENUM_Reverse_C1};
}

/* Deliberately violates the foreign result contract to test target rejection. */
static struct ap_enum_Bool invalid(void *opaque, uint32_t n)
{
	return (struct ap_enum_Bool){n == *(const uint32_t *)opaque ? UINT32_MAX : AP_ENUM_Bool_C1};
}

static struct ap_enum_Bool invalid_pair(void *opaque, uint32_t left, uint32_t right)
{
	(void)right;
	return invalid(opaque, left);
}

static const struct ap_data_Numbers nil = {.tag = AP_DATA_Numbers_C0};

static void ordinary(void)
{
	struct ap_c_arena arena = {0};
	struct context context = {0};
	struct ap_c_predicate1_d3_Nat_r4_Bool unary = {&context, keep};
	struct ap_c_predicate2_d3_Nat_r4_Bool binary = {&context, compare};
	struct ap_c_predicate1_d3_Nat_r7_Reverse reversed = {NULL, reverse};
	struct ap_enum_Bool answer;
	struct ap_enum_Reverse other;
	for (uint32_t left = 0; left <= 16; ++left) {
		assert(!ap_export_apply_predicate(&arena, unary, left, &answer) && answer.tag == (left != 0));
		assert(!ap_export_apply_reverse(&arena, reversed, left, &other) && other.tag == (left ? 0 : 1));
		for (uint32_t right = 0; right <= 16; ++right)
			assert(!ap_export_apply_comparator(&arena, binary, left, right, &answer) && answer.tag == (left <= right));
	}
	size_t combinations = 1, cases = 0;
	for (size_t length = 0; length <= 5; ++length, combinations *= 4) {
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t values[5], buffer[6];
			size_t digits = code;
			for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; digits /= 4; }
			const struct ap_data_Numbers *input, *out;
			assert(!ap_from_Numbers(&arena, values, length, &input));
			struct ap_c_allocation *input_mark = arena.first;
			size_t input_count = arena.count, written, expected = 0;
			int32_t measured, encoded;
			uint32_t fingerprint = 0;
			for (size_t i = length; i; --i) fingerprint = fingerprint * 5 + values[i - 1] + 1;
			assert(!ap_export_length(&arena, input, &measured) && measured == (int32_t)length);
			assert(!ap_export_fingerprint(&arena, input, &encoded) && (uint32_t)encoded == fingerprint);
			context = (struct context){0};
			assert(!ap_export_filter(&arena, unary, input, &out));
			assert(!ap_copy_Numbers(out, buffer, 5, &written));
			for (size_t i = 0; i < length; ++i) if (values[i]) assert(buffer[expected++] == values[i]);
			assert(written == expected && arena.count == input_count + expected + 1);
			for (uint32_t pivot = 0; pivot <= 4; ++pivot) for (unsigned upper = 0; upper < 2; ++upper) {
				context.upper = upper;
				assert(!ap_export_select(&arena, binary, input, pivot, &out));
				assert(!ap_copy_Numbers(out, buffer, 5, &written));
				expected = 0;
				for (size_t i = 0; i < length; ++i)
					if ((values[i] > pivot) == upper) assert(buffer[expected++] == values[i]);
				assert(written == expected && context.upper == upper && !context.threshold && !arena.depth);
			}
			assert(input_mark && !ap_copy_Numbers(input, buffer, 5, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer[i] == values[i]);
			ap_arena_Nat_destroy(&arena); ++cases;
		}
	}
	assert(cases == 1365);
}

static void boundaries(void)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d3_Nat_r4_Bool unary = {NULL, keep}, missing = {NULL, NULL};
	struct ap_c_predicate2_d3_Nat_r4_Bool binary = {NULL, compare}, missing_pair = {NULL, NULL};
	struct ap_enum_Bool answer = {77};
	assert(ap_export_apply_predicate(NULL, unary, 0, &answer) == 1 && answer.tag == 77);
	assert(ap_export_apply_predicate(&arena, unary, 0, NULL) == 1);
	assert(ap_export_unused(&arena, missing, 0, &answer) == 2 && answer.tag == 77);
	assert(!ap_export_unused(&arena, unary, 123, &answer) && answer.tag == 1);
	answer.tag = 77;
	assert(ap_export_apply_comparator(&arena, missing_pair, 0, 0, &answer) == 2 && answer.tag == 77);
	assert(!ap_export_apply_predicate(&arena, unary, UINT32_MAX, &answer) && answer.tag == 1);
	assert(!ap_export_apply_comparator(&arena, binary, UINT32_MAX, UINT32_MAX, &answer) && answer.tag == 1);
	assert(!ap_export_apply_comparator(&arena, binary, 0, UINT32_MAX, &answer) && answer.tag == 1);
	uint32_t magnitude = 77;
	assert(!ap_export_increment(&arena, UINT32_MAX - 1, &magnitude) && magnitude == UINT32_MAX);
	assert(ap_export_increment(&arena, UINT32_MAX, &magnitude) == 5 && magnitude == UINT32_MAX);
	uint32_t bad = 7;
	struct ap_c_predicate1_d3_Nat_r4_Bool wrong = {&bad, invalid};
	struct ap_c_predicate2_d3_Nat_r4_Bool wrong_pair = {&bad, invalid_pair};
	answer.tag = 77;
	assert(ap_export_apply_predicate(&arena, wrong, bad, &answer) == 2 && answer.tag == 77);
	assert(ap_export_apply_comparator(&arena, wrong_pair, bad, 0, &answer) == 2 && answer.tag == 77);
	uint32_t values[] = {0, 1};
	const struct ap_data_Numbers *input, *out = &nil;
	assert(!ap_from_Numbers(&arena, values, 2, &input));
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count, calls = allocation_calls;
	assert(ap_export_post_check(&arena, wrong, input, bad, &out) == 2 && out == &nil);
#ifndef PREDICATE_SHARED_PRODUCT
	assert(allocation_calls > calls);
#else
	(void)calls;
#endif
	assert(arena.first == mark && arena.count == count && !arena.depth);
	uint32_t buffer[] = {77, 77, 77};
	size_t written = 77;
	assert(!ap_copy_Numbers(input, buffer, 2, &written) && written == 2 && buffer[0] == 0 && buffer[1] == 1);
	bad = 1; out = &nil;
	assert(ap_export_filter(&arena, wrong, input, &out) == 2 && out == &nil && arena.first == mark && arena.count == count);
	assert(ap_export_select(&arena, wrong_pair, input, 0, &out) == 2 && out == &nil && !arena.depth);
	assert(ap_export_filter(&arena, missing, &nil, &out) == 2 && out == &nil);
	assert(ap_export_filter(&arena, unary, input, NULL) == 1);
	assert(ap_export_filter(NULL, unary, input, &out) == 1 && out == &nil);
#ifndef PREDICATE_SHARED_PRODUCT
	allocations = 1;
	assert(ap_export_filter(&arena, unary, input, &out) == 3 && out == &nil && arena.first == mark && arena.count == count);
	allocations = SIZE_MAX;
#endif
	arena.capacity = count + 1;
	assert(ap_export_filter(&arena, unary, input, &out) == 3 && out == &nil && arena.first == mark && arena.count == count);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_filter(&arena, unary, input, &out) == 4 && out == &nil && arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 3;
	assert(!ap_export_filter(&arena, unary, input, &out));
	arena.depth_limit = 257; answer.tag = 77;
	assert(ap_export_apply_predicate(&arena, unary, 1, &answer) == 4 && answer.tag == 77);
	arena.depth_limit = 0; arena.depth = 1;
	assert(ap_export_apply_predicate(&arena, unary, 1, &answer) == 4 && answer.tag == 77);
	arena.depth = 0;
	struct ap_data_Numbers damaged = {.tag = UINT32_MAX};
	out = &nil;
	assert(ap_export_filter(&arena, unary, &damaged, &out) == 2 && out == &nil);
	damaged = (struct ap_data_Numbers){.tag = AP_DATA_Numbers_C1, .fields.c1 = {0, NULL}};
	assert(ap_export_filter(&arena, unary, &damaged, &out) == 2 && out == &nil);
	damaged.fields.c1.f1 = &damaged;
	assert(ap_export_select(&arena, binary, &damaged, 1, &out) == 2 && out == &nil);
	assert(ap_export_filter(&arena, unary, NULL, &out) == 2 && out == &nil);
	buffer[0] = 77; written = 77;
	assert(ap_copy_Numbers(input, buffer, 1, &written) == 6 && buffer[0] == 77 && written == 77);
	assert(ap_copy_Numbers(&damaged, buffer, 3, &written) == 2 && buffer[0] == 77 && written == 77);
	assert(ap_from_Numbers(&arena, values, SIZE_MAX, &out) == 6 && out == &nil);
	assert(!ap_export_filter(&arena, unary, input, &out));
	ap_arena_Nat_destroy(&arena);
	uint32_t long_values[260] = {0};
	assert(!ap_from_Numbers(&arena, long_values, 260, &input));
	mark = arena.first; count = arena.count; out = &nil;
	assert(ap_export_filter(&arena, unary, input, &out) == 4 && out == &nil && arena.first == mark && arena.count == count && !arena.depth);
	ap_arena_Nat_destroy(&arena);
}

static void reference(void)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d3_Nat_r4_Bool unary = {NULL, keep};
	struct ap_c_predicate2_d3_Nat_r4_Bool binary = {NULL, compare};
	struct ap_enum_Bool flag;
	assert(!ap_export_apply_predicate(&arena, unary, 0, &flag)); printf("%c|", flag.tag ? 'T' : 'F');
	assert(!ap_export_apply_predicate(&arena, unary, 2, &flag)); printf("%c|", flag.tag ? 'T' : 'F');
	assert(!ap_export_apply_comparator(&arena, binary, 1, 2, &flag)); printf("%c|", flag.tag ? 'T' : 'F');
	assert(!ap_export_apply_comparator(&arena, binary, 2, 1, &flag)); printf("%c|", flag.tag ? 'T' : 'F');
	uint32_t values[] = {0, 1, 2, 0}, copied[4];
	const struct ap_data_Numbers *input, *out;
	size_t written;
	assert(!ap_from_Numbers(&arena, values, 4, &input));
	assert(!ap_export_filter(&arena, unary, input, &out) && !ap_copy_Numbers(out, copied, 4, &written));
	for (size_t i = 0; i < written; ++i) printf("%u,", (unsigned)copied[i]);
	putchar('|');
	assert(!ap_export_select(&arena, binary, input, 1, &out) && !ap_copy_Numbers(out, copied, 4, &written));
	for (size_t i = 0; i < written; ++i) printf("%u,", (unsigned)copied[i]);
	putchar('|');
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	ordinary(); boundaries(); reference();
	return 0;
}
