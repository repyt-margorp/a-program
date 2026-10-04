#include "adapter.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#ifdef DYNAMIC_PROVIDER
#include <dlfcn.h>
#include <string.h>
#endif

static int32_t signed32(uint32_t n)
{
	return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}
static int64_t signed64(uint64_t n)
{
	return n <= INT64_MAX ? (int64_t)n : -1 - (int64_t)(UINT64_MAX - n);
}
static int32_t mapped32(int32_t offset, int32_t n)
{
	return signed32((uint32_t)offset - (uint32_t)n);
}
static int64_t mapped64(int64_t offset, int64_t n)
{
	return signed64((uint64_t)offset - (uint64_t)n);
}
static int32_t combined32(int32_t left, int32_t right)
{
	return signed32((uint32_t)left - (uint32_t)right);
}
static int64_t combined64(int64_t left, int64_t right)
{
	return signed64((uint64_t)left - (uint64_t)right);
}

static void providers(const struct native_scalar_provider_api *api)
{
	const int32_t values32[] = {INT32_MIN,-1,0,1,INT32_MAX};
	const int64_t values64[] = {INT64_MIN,-1,0,1,INT64_MAX}; int32_t a; int64_t b;
	for (size_t i = 0; i < 5; ++i) for (size_t j = 0; j < 5; ++j) {
		assert(!api->offset32(values32[i],values32[j],&a) && a == mapped32(values32[i],values32[j]));
		assert(!api->offset64(values64[i],values64[j],&b) && b == mapped64(values64[i],values64[j]));
		assert(!api->subtract32(values32[i],values32[j],&a) && a == combined32(values32[i],values32[j]));
		assert(!api->subtract64(values64[i],values64[j],&b) && b == combined64(values64[i],values64[j]));
	}
	assert(api->offset32(0,0,NULL) == 1 && api->offset64(0,0,NULL) == 1);
	assert(api->subtract32(0,0,NULL) == 1 && api->subtract64(0,0,NULL) == 1);
}

static void lists(const struct native_scalar_provider_api *api)
{
	const int32_t choices32[] = {INT32_MIN,0,INT32_MAX};
	const int64_t choices64[] = {INT64_MIN,0,INT64_MAX};
	struct ap_c_arena arena = {0}; size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 3)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t input32[4], copy32[4], result32; int64_t input64[4], copy64[4], result64;
			size_t digits = code, written;
			for (size_t i = 0; i < length; ++i,digits /= 3) { input32[i] = choices32[digits%3]; input64[i] = choices64[digits%3]; }
			const struct ap_data_MNumbers32 *list32, *out32; const struct ap_data_MNumbers64 *list64, *out64;
			assert(!ap_from_MNumbers32(&arena,input32,length,&list32)); assert(!ap_from_MNumbers64(&arena,input64,length,&list64));
			for (unsigned shifted = 0; shifted < 2; ++shifted) {
				const struct native_scalar_provider_context context = {api,shifted ? -1 : 0,shifted ? -1 : 0};
				assert(!ap_export_module_map32(&arena,native_scalar_unary32(&context),list32,&out32));
				assert(!ap_export_module_map64(&arena,native_scalar_unary64(&context),list64,&out64));
				assert(!ap_copy_MNumbers32(out32,copy32,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy32[i] == mapped32(context.offset32,input32[i]));
				assert(!ap_copy_MNumbers64(out64,copy64,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy64[i] == mapped64(context.offset64,input64[i]));
				assert(!ap_export_module_length32(&arena,out32,&result32) && result32 == (int32_t)length);
				assert(!ap_export_module_length64(&arena,out64,&result32) && result32 == (int32_t)length);
				assert(context.api == api && context.offset32 == (shifted ? -1 : 0) && context.offset64 == (shifted ? -1 : 0));
			}
			const struct native_scalar_provider_context context = {api,0,0};
			for (size_t operand = 0; operand < 3; ++operand) {
				assert(!ap_export_module_combine32(&arena,native_scalar_binary32(&context),list32,choices32[operand],&out32));
				assert(!ap_export_module_combine64(&arena,native_scalar_binary64(&context),list64,choices64[operand],&out64));
				assert(!ap_copy_MNumbers32(out32,copy32,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy32[i] == combined32(input32[i],choices32[operand]));
				assert(!ap_copy_MNumbers64(out64,copy64,4,&written) && written == length);
				for (size_t i = 0; i < length; ++i) assert(copy64[i] == combined64(input64[i],choices64[operand]));
				int32_t expected32 = choices32[operand]; int64_t expected64 = choices64[operand];
				for (size_t i = length; i; --i) { expected32 = combined32(input32[i-1],expected32); expected64 = combined64(input64[i-1],expected64); }
				assert(!ap_export_module_reduce32(&arena,native_scalar_binary32(&context),list32,choices32[operand],&result32) && result32 == expected32);
				assert(!ap_export_module_reduce64(&arena,native_scalar_binary64(&context),list64,choices64[operand],&result64) && result64 == expected64);
			}
			assert(!ap_copy_MNumbers32(list32,copy32,4,&written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(copy32[i] == input32[i]);
			assert(!ap_copy_MNumbers64(list64,copy64,4,&written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(copy64[i] == input64[i]);
			assert(!arena.depth); ap_arena_MNumbers32_destroy(&arena); ++cases;
		}
	assert(cases == 121);
}

static int32_t tree_value(const struct ap_data_MTree32 *tree, int32_t offset)
{
	if (!tree->tag) return mapped32(offset,tree->fields.c0.f0);
	return combined32(tree_value(tree->fields.c1.f0,offset),tree_value(tree->fields.c1.f1,offset));
}

static void trees(const struct native_scalar_provider_api *api)
{
	const int32_t values[] = {INT32_MIN,0,INT32_MAX}; struct ap_data_MTree32 leaves[3], branches[9];
	for (size_t i = 0; i < 3; ++i) leaves[i] = (struct ap_data_MTree32){.tag = 0,.fields.c0 = {values[i]}};
	for (size_t i = 0; i < 9; ++i) branches[i] = (struct ap_data_MTree32){.tag = 1,.fields.c1 = {&leaves[i/3],&leaves[i%3]}};
	struct ap_c_arena arena = {0}; int32_t out;
	for (unsigned shifted = 0; shifted < 2; ++shifted) {
		const struct native_scalar_provider_context context = {api,shifted ? -1 : 0,0};
		for (size_t i = 0; i < 3; ++i) assert(!ap_export_module_tree32(&arena,native_scalar_unary32(&context),native_scalar_binary32(&context),&leaves[i],&out) && out == tree_value(&leaves[i],context.offset32));
		for (size_t i = 0; i < 9; ++i) assert(!ap_export_module_tree32(&arena,native_scalar_unary32(&context),native_scalar_binary32(&context),&branches[i],&out) && out == tree_value(&branches[i],context.offset32));
		for (size_t i = 0; i < 9; ++i) for (size_t j = 0; j < 9; ++j) {
			struct ap_data_MTree32 root = {.tag = 1,.fields.c1 = {&branches[i],&branches[j]}};
			assert(!ap_export_module_tree32(&arena,native_scalar_unary32(&context),native_scalar_binary32(&context),&root,&out) && out == tree_value(&root,context.offset32));
		}
	}
	assert(!arena.first && !arena.count && !arena.depth); ap_arena_MNumbers32_destroy(&arena);
}

static void boundaries(const struct native_scalar_provider_api *api)
{
	const struct native_scalar_provider_context context = {api,0,0}; struct ap_c_arena arena = {0};
	int32_t input32[] = {-1,0,1}; int64_t input64[] = {INT64_MIN,0,INT64_MAX};
	const struct ap_data_MNumbers32 *list32, *out32; const struct ap_data_MNumbers64 *list64, *out64;
	assert(!ap_from_MNumbers32(&arena,input32,3,&list32)); assert(!ap_from_MNumbers64(&arena,input64,3,&list64)); out32 = list32; out64 = list64;
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count; int64_t result64 = 77;
	assert(ap_export_module_map32(&arena,(struct ap_c_callback_i32){0},list32,&out32) == 2 && out32 == list32);
	assert(ap_export_module_map64(&arena,native_scalar_unary64(&context),list64,NULL) == 1);
	assert(ap_export_module_map32(NULL,native_scalar_unary32(&context),list32,&out32) == 1 && out32 == list32);
	arena.capacity = count + 1;
	assert(ap_export_module_map32(&arena,native_scalar_unary32(&context),list32,&out32) == 3 && out32 == list32);
	assert(ap_export_module_map64(&arena,native_scalar_unary64(&context),list64,&out64) == 3 && out64 == list64);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_module_reduce64(&arena,native_scalar_binary64(&context),list64,0,&result64) == 4 && result64 == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.depth_limit = 0; arena.depth = 1;
	assert(ap_export_module_map32(&arena,native_scalar_unary32(&context),list32,&out32) == 4 && out32 == list32);
	assert(!api->offset64(0,INT64_MIN,&result64) && result64 == INT64_MIN);
	assert(arena.first == mark && arena.count == count && arena.depth == 1); arena.depth = 0;
	struct ap_data_MNumbers64 bad = {.tag = 77}; out64 = list64;
	assert(ap_export_module_map64(&arena,native_scalar_unary64(&context),&bad,&out64) == 2 && out64 == list64);
	struct ap_data_MPacket packet = {.tag = 77};
	assert(ap_export_module_packet32(&arena,native_scalar_unary32(&context),0,(struct ap_enum_MFlag){77},&packet) == 2 && packet.tag == 77);
	assert(!ap_export_module_packet32(&arena,native_scalar_unary32(&context),INT32_MIN,(struct ap_enum_MFlag){1},&packet));
	assert(!packet.tag && packet.fields.c0.f0 == INT32_MIN && packet.fields.c0.f1.tag == 1);
	int64_t copy[3] = {77,77,77}; size_t written = 77;
	assert(ap_copy_MNumbers64(list64,copy,2,&written) == 6 && copy[0] == 77 && written == 77);
	assert(!ap_export_module_map64(&arena,native_scalar_unary64(&context),list64,&out64));
	assert(!ap_copy_MNumbers64(out64,copy,3,&written) && written == 3); ap_arena_MNumbers32_destroy(&arena);
	for (size_t i = 0; i < 3; ++i) assert(copy[i] == mapped64(0,input64[i]));
}

static void reference(const struct native_scalar_provider_api *api)
{
	int32_t result; assert(!api->offset32(0,INT32_MIN,&result)); printf("%" PRId32 "|",result);
	assert(!api->offset32(-1,1,&result)); printf("%" PRId32 "|",result);
	assert(!api->subtract32(INT32_MIN,1,&result)); printf("%" PRId32 "|",result);
	assert(!api->subtract32(INT32_MAX,-1,&result)); printf("%" PRId32 "|",result);
	const struct native_scalar_provider_context context = {api,0,0}; struct ap_c_arena arena = {0};
	int32_t values[] = {-1,0,1}, copy[3]; size_t written; const struct ap_data_MNumbers32 *input, *out;
	assert(!ap_from_MNumbers32(&arena,values,3,&input));
	for (unsigned combine = 0; combine < 2; ++combine) {
		if (combine) assert(!ap_export_module_combine32(&arena,native_scalar_binary32(&context),input,1,&out));
		else assert(!ap_export_module_map32(&arena,native_scalar_unary32(&context),input,&out));
		assert(!ap_copy_MNumbers32(out,copy,3,&written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",",copy[i]);
		putchar('|');
	}
	assert(!ap_export_module_reduce32(&arena,native_scalar_binary32(&context),input,0,&result)); printf("%" PRId32 "|",result);
	assert(!ap_export_module_reduce32(&arena,native_scalar_binary32(&context),input,INT32_MIN,&result)); printf("%" PRId32 "|",result);
	struct ap_data_MTree32 leaves[] = {{.tag = 0,.fields.c0 = {-1}},{.tag = 0,.fields.c0 = {0}},{.tag = 0,.fields.c0 = {1}}};
	struct ap_data_MTree32 child = {.tag = 1,.fields.c1 = {&leaves[1],&leaves[2]}}, tree = {.tag = 1,.fields.c1 = {&leaves[0],&child}};
	assert(!ap_export_module_tree32(&arena,native_scalar_unary32(&context),native_scalar_binary32(&context),&tree,&result)); printf("%" PRId32 "|",result);
	ap_arena_MNumbers32_destroy(&arena);
}

int main(int argc, char **argv)
{
#ifdef DYNAMIC_PROVIDER
	assert(argc == 2); void *handle = dlopen(argv[1],RTLD_NOW | RTLD_LOCAL); assert(handle);
	struct native_scalar_provider_api api; const char *names[] = {"ap_export_provider_offset32","ap_export_provider_offset64","ap_export_provider_subtract32","ap_export_provider_subtract64"};
	void *targets[] = {&api.offset32,&api.offset64,&api.subtract32,&api.subtract64};
	_Static_assert(sizeof(api.offset32) == sizeof(void *),"unsupported dynamic function pointer size");
	_Static_assert(sizeof(api.offset64) == sizeof(void *),"unsupported dynamic function pointer size");
	_Static_assert(sizeof(api.subtract32) == sizeof(void *),"unsupported dynamic function pointer size");
	_Static_assert(sizeof(api.subtract64) == sizeof(void *),"unsupported dynamic function pointer size");
	for (size_t i = 0; i < 4; ++i) { void *symbol = dlsym(handle,names[i]); assert(symbol); memcpy(targets[i],&symbol,sizeof(symbol)); }
#else
	(void)argc; (void)argv;
	struct native_scalar_provider_api api = {ap_export_provider_offset32,ap_export_provider_offset64,ap_export_provider_subtract32,ap_export_provider_subtract64};
#endif
	providers(&api); lists(&api); trees(&api); boundaries(&api); reference(&api);
#ifdef DYNAMIC_PROVIDER
	/* All arenas/calls/borrowed contexts have ended before code unload. */
	assert(!dlclose(handle));
#endif
	return 0;
}
