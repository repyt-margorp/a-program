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

struct context { uint32_t offset32; uint64_t offset64; };
static int32_t signed32(uint32_t n)
{
	return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}
static int64_t signed64(uint64_t n)
{
	return n <= INT64_MAX ? (int64_t)n : -1 - (int64_t)(UINT64_MAX - n);
}
static int32_t negate32(void *context, int32_t n)
{
	const struct context *c = context; return signed32(UINT32_C(0) - (uint32_t)n + (c ? c->offset32 : 0));
}
static int64_t negate64(void *context, int64_t n)
{
	const struct context *c = context; return signed64(UINT64_C(0) - (uint64_t)n + (c ? c->offset64 : 0));
}
static int32_t subtract32(void *context, int32_t left, int32_t right)
{
	(void)context; return signed32((uint32_t)left - (uint32_t)right);
}
static int64_t subtract64(void *context, int64_t left, int64_t right)
{
	(void)context; return signed64((uint64_t)left - (uint64_t)right);
}

static void lists(void)
{
	const int32_t choices32[] = {INT32_MIN,-1,0,1,INT32_MAX};
	const int64_t choices64[] = {INT64_MIN,-1,0,1,INT64_MAX};
	struct ap_c_arena arena = {0}; struct context context = {0};
	struct ap_c_callback_i32 unary32 = {&context,negate32}; struct ap_c_callback_i64 unary64 = {&context,negate64};
	struct ap_c_callback2_i32 binary32 = {NULL,subtract32}; struct ap_c_callback2_i64 binary64 = {NULL,subtract64};
	size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 5)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t input32[4], copy32[4], result32; int64_t input64[4], copy64[4], result64; size_t digits = code, written;
			for (size_t i = 0; i < length; ++i, digits /= 5) { input32[i] = choices32[digits % 5]; input64[i] = choices64[digits % 5]; }
			const struct ap_data_Numbers32 *list32, *mapped32; const struct ap_data_Numbers64 *list64, *mapped64;
			assert(!ap_from_Numbers32(&arena, input32, length, &list32)); assert(!ap_from_Numbers64(&arena, input64, length, &list64));
			struct ap_c_allocation *prior = arena.first;
			for (unsigned shifted = 0; shifted < 2; ++shifted) {
				context.offset32 = shifted ? UINT32_MAX : 0; context.offset64 = shifted ? UINT64_MAX : 0;
				assert(!ap_export_map32(&arena, unary32, list32, &mapped32)); assert(!ap_copy_Numbers32(mapped32, copy32, 4, &written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy32[i] == negate32(&context,input32[i]));
				assert(!ap_export_map64(&arena, unary64, list64, &mapped64)); assert(!ap_copy_Numbers64(mapped64, copy64, 4, &written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy64[i] == negate64(&context,input64[i]));
				int32_t measured; assert(!ap_export_length32(&arena,mapped32,&measured) && measured == (int32_t)length);
				assert(!ap_export_length64(&arena,mapped64,&measured) && measured == (int32_t)length);
			}
			for (size_t pivot = 0; pivot < 5; ++pivot) {
				assert(!ap_export_combine32(&arena,binary32,list32,choices32[pivot],&mapped32));
				assert(!ap_copy_Numbers32(mapped32,copy32,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy32[i] == subtract32(NULL,input32[i],choices32[pivot]));
				assert(!ap_export_combine64(&arena,binary64,list64,choices64[pivot],&mapped64));
				assert(!ap_copy_Numbers64(mapped64,copy64,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy64[i] == subtract64(NULL,input64[i],choices64[pivot]));
				assert(!ap_export_reduce32(&arena,binary32,list32,choices32[pivot],&result32));
				int32_t expected32 = choices32[pivot]; for (size_t i = length; i; --i) expected32 = subtract32(NULL,input32[i-1],expected32);
				assert(result32 == expected32);
				assert(!ap_export_reduce64(&arena,binary64,list64,choices64[pivot],&result64));
				int64_t expected64 = choices64[pivot]; for (size_t i = length; i; --i) expected64 = subtract64(NULL,input64[i-1],expected64);
				assert(result64 == expected64 && !arena.depth);
			}
			assert(prior && !ap_copy_Numbers32(list32,copy32,4,&written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(copy32[i] == input32[i]);
			assert(!ap_copy_Numbers64(list64,copy64,4,&written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(copy64[i] == input64[i]);
			ap_arena_Numbers32_destroy(&arena); ++cases;
		}
	assert(cases == 781);
}

static int32_t tree_value(const struct ap_data_Tree32 *tree)
{
	if (tree->tag == AP_DATA_Tree32_C0) return negate32(NULL,tree->fields.c0.f0);
	return subtract32(NULL,tree_value(tree->fields.c1.f0),tree_value(tree->fields.c1.f1));
}

static void trees(void)
{
	struct ap_data_Tree32 leaves[5], branches[25]; const int32_t values[] = {INT32_MIN,-1,0,1,INT32_MAX};
	for (size_t i = 0; i < 5; ++i) leaves[i] = (struct ap_data_Tree32){.tag = AP_DATA_Tree32_C0,.fields.c0 = {values[i]}};
	for (size_t i = 0; i < 25; ++i) branches[i] = (struct ap_data_Tree32){.tag = AP_DATA_Tree32_C1,.fields.c1 = {&leaves[i/5],&leaves[i%5]}};
	struct ap_c_arena arena = {0}; struct ap_c_callback_i32 unary = {NULL,negate32}; struct ap_c_callback2_i32 binary = {NULL,subtract32};
	int32_t result;
	for (size_t i = 0; i < 5; ++i) assert(!ap_export_tree32(&arena,unary,binary,&leaves[i],&result) && result == tree_value(&leaves[i]));
	for (size_t i = 0; i < 25; ++i) assert(!ap_export_tree32(&arena,unary,binary,&branches[i],&result) && result == tree_value(&branches[i]));
	for (size_t i = 0; i < 25; ++i) for (size_t j = 0; j < 25; ++j) {
		struct ap_data_Tree32 root = {.tag = AP_DATA_Tree32_C1,.fields.c1 = {&branches[i],&branches[j]}};
		assert(!ap_export_tree32(&arena,unary,binary,&root,&result) && result == tree_value(&root));
	}
	assert(!arena.first && !arena.count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
}

static void tree_failures(void)
{
	struct ap_c_arena arena = {0}; struct ap_c_callback_i32 unary = {NULL,negate32}; struct ap_c_callback2_i32 binary = {NULL,subtract32};
	struct ap_data_Tree32 leaf = {.tag = AP_DATA_Tree32_C0,.fields.c0 = {1}};
	struct ap_data_Tree32 root = {.tag = AP_DATA_Tree32_C1,.fields.c1 = {NULL,&leaf}}; int32_t out = 77;
	assert(ap_export_tree32(&arena,unary,binary,&root,&out) == 2 && out == 77);
	root.fields.c1.f0 = &leaf; root.fields.c1.f1 = NULL;
	assert(ap_export_tree32(&arena,unary,binary,&root,&out) == 2 && out == 77);
	root.fields.c1.f1 = &root;
	assert(ap_export_tree32(&arena,unary,binary,&root,&out) == 2 && out == 77);
	root.tag = 77; assert(ap_export_tree32(&arena,unary,binary,&root,&out) == 2 && out == 77);
	assert(!arena.first && !arena.count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
}

static void failures(void)
{
	struct ap_c_arena arena = {0}; struct ap_c_callback_i32 unary32 = {NULL,negate32}; struct ap_c_callback_i64 unary64 = {NULL,negate64};
	struct ap_c_callback2_i64 binary64 = {NULL,subtract64}; int32_t a[] = {-1,0,1}; int64_t b[] = {INT64_MIN,0,INT64_MAX};
	const struct ap_data_Numbers32 *list32, *out32; const struct ap_data_Numbers64 *list64, *out64;
	assert(!ap_from_Numbers32(&arena,a,3,&list32)); assert(!ap_from_Numbers64(&arena,b,3,&list64));
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count; int32_t result32 = 77; int64_t result64 = 77;
	out32 = list32; out64 = list64; unary32.call = NULL;
	assert(ap_export_unused32(&arena,unary32,0,&result32) == 2 && result32 == 77);
	assert(ap_export_map32(&arena,unary32,list32,&out32) == 2 && out32 == list32); unary32.call = negate32;
	assert(ap_export_apply32(NULL,unary32,0,&result32) == 1 && result32 == 77);
	assert(ap_export_apply64(&arena,unary64,0,NULL) == 1);
	struct ap_data_Packet packet = {.tag = 77}; struct ap_enum_Bool invalid = {77};
	assert(ap_export_packet32(&arena,unary32,INT32_MIN,invalid,&packet) == 2 && packet.tag == 77);
	assert(!ap_export_packet32(&arena,unary32,INT32_MIN,(struct ap_enum_Bool){1},&packet));
	assert(!packet.tag && packet.fields.c0.f0 == INT32_MIN && packet.fields.c0.f1.tag == 1);
	arena.capacity = count + 1;
	assert(ap_export_map32(&arena,unary32,list32,&out32) == 3 && out32 == list32);
	assert(ap_export_combine64(&arena,binary64,list64,1,&out64) == 3 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.capacity = 0;
#ifndef NATIVE_SHARED_PRODUCT
	attempts = 0; assert(!ap_export_map32(&arena,unary32,list32,&out32)); size_t calls = attempts; mark = arena.first; count = arena.count;
	for (size_t i = 1; i <= calls; ++i) {
		attempts = 0; fail_on = i; out32 = list32;
		assert(ap_export_map32(&arena,unary32,list32,&out32) == 3 && out32 == list32);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_on = 0;
	}
	struct ap_data_Tree32 leaf = {.tag = AP_DATA_Tree32_C0,.fields.c0 = {1}};
	struct ap_data_Tree32 branch = {.tag = AP_DATA_Tree32_C1,.fields.c1 = {&leaf,&leaf}};
	struct ap_c_callback2_i32 binary32 = {NULL,subtract32}; attempts = 0;
	assert(!ap_export_tree32(&arena,unary32,binary32,&branch,&result32)); calls = attempts;
	for (size_t i = 1; i <= calls; ++i) {
		attempts = 0; fail_on = i; result32 = 77;
		assert(ap_export_tree32(&arena,unary32,binary32,&branch,&result32) == 3 && result32 == 77);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_on = 0;
	}
#endif
	arena.depth_limit = 2; out32 = list32;
	assert(ap_export_map32(&arena,unary32,list32,&out32) == 4 && out32 == list32);
	assert(ap_export_reduce64(&arena,binary64,list64,0,&result64) == 4 && result64 == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.depth_limit = 0;
	struct ap_data_Numbers64 damaged = {.tag = 77};
	assert(ap_export_map64(&arena,unary64,&damaged,&out64) == 2 && out64 == list64);
	damaged = (struct ap_data_Numbers64){.tag = 0,.fields.c0 = {NULL,1}};
	assert(ap_export_map64(&arena,unary64,&damaged,&out64) == 2 && out64 == list64);
	damaged.fields.c0.f0 = &damaged;
	assert(ap_export_map64(&arena,unary64,&damaged,&out64) == 2 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
	int64_t deep[300] = {0}; assert(!ap_from_Numbers64(&arena,deep,300,&list64)); mark = arena.first; count = arena.count; out64 = list64;
	assert(ap_export_map64(&arena,unary64,list64,&out64) == 4 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); ap_arena_Numbers32_destroy(&arena);
}

static void reference(void)
{
	struct ap_c_arena arena = {0}; struct ap_c_callback_i32 unary = {NULL,negate32}; struct ap_c_callback2_i32 binary = {NULL,subtract32};
	int32_t values[] = {-1,0,1}, copy[3], result; size_t written; const struct ap_data_Numbers32 *input, *out;
	assert(!ap_from_Numbers32(&arena,values,3,&input));
	for (unsigned combine = 0; combine < 2; ++combine) {
		if (combine) assert(!ap_export_combine32(&arena,binary,input,1,&out));
		else assert(!ap_export_map32(&arena,unary,input,&out));
		assert(!ap_copy_Numbers32(out,copy,3,&written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",",copy[i]);
		putchar('|');
	}
	assert(!ap_export_reduce32(&arena,binary,input,0,&result)); printf("%" PRId32 "|",result);
	assert(!ap_export_reduce32(&arena,binary,input,INT32_MIN,&result)); printf("%" PRId32 "|",result);
	struct ap_data_Tree32 leaves[] = {{.tag = 0,.fields.c0 = {-1}},{.tag = 0,.fields.c0 = {0}},{.tag = 0,.fields.c0 = {1}}};
	struct ap_data_Tree32 child = {.tag = 1,.fields.c1 = {&leaves[1],&leaves[2]}}, root = {.tag = 1,.fields.c1 = {&leaves[0],&child}};
	assert(!ap_export_tree32(&arena,unary,binary,&root,&result)); printf("%" PRId32 "|",result);
	ap_arena_Numbers32_destroy(&arena);
}

int main(void)
{
	lists(); trees(); tree_failures(); failures(); reference(); return 0;
}
