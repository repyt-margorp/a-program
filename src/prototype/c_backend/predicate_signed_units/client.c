#include "adapter.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#ifdef DYNAMIC_PROVIDER
#include <dlfcn.h>
#include <string.h>
#endif

static void scalar(const struct signed_provider_api *api)
{
	const int32_t values32[] = {INT32_MIN,-1,0,1,INT32_MAX};
	const int64_t values64[] = {INT64_MIN,-1,0,1,INT64_MAX};
	struct ap_c_arena arena = {0}; struct ap_enum_LFlag left; struct ap_enum_RFlag right;
	struct signed_provider_context context = {.api = api};
	for (unsigned truth = 0; truth < 2; ++truth) {
		context.flag.tag = truth ? AP_ENUM_PFlag_C0 : AP_ENUM_PFlag_C1;
		for (size_t i = 0; i < 5; ++i) {
			assert(!ap_export_left_apply32(&arena, signed_left_unary32(&context), values32[i], &left) && left.tag == truth);
			assert(!ap_export_right_apply32(&arena, signed_right_unary32(&context), values32[i], &right) && right.tag == truth);
			assert(!ap_export_left_apply64(&arena, signed_left_unary64(&context), values64[i], &left) && left.tag == truth);
			assert(!ap_export_right_apply64(&arena, signed_right_unary64(&context), values64[i], &right) && right.tag == truth);
			for (size_t j = 0; j < 5; ++j) {
				assert(!ap_export_left_choose32(&arena, signed_left_binary32(&context), values32[i], values32[j], &left) && left.tag == truth);
				assert(!ap_export_right_choose32(&arena, signed_right_binary32(&context), values32[i], values32[j], &right) && right.tag == truth);
				assert(!ap_export_left_choose64(&arena, signed_left_binary64(&context), values64[i], values64[j], &left) && left.tag == truth);
				assert(!ap_export_right_choose64(&arena, signed_right_binary64(&context), values64[i], values64[j], &right) && right.tag == truth);
			}
			assert(context.flag.tag == (truth ? AP_ENUM_PFlag_C0 : AP_ENUM_PFlag_C1));
		}
	}
	assert(!arena.first && !arena.count && !arena.depth);
	ap_arena_LNumbers32_destroy(&arena);
}

static void lists(const struct signed_provider_api *api)
{
	const int32_t choices32[] = {INT32_MIN,0,INT32_MAX};
	const int64_t choices64[] = {INT64_MIN,0,INT64_MAX};
	struct ap_c_arena arena = {0}; struct signed_provider_context context = {.api = api};
	size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 3)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t input32[4], a32[4], b32[4]; int64_t input64[4], a64[4], b64[4]; size_t digits = code;
			for (size_t i = 0; i < length; ++i, digits /= 3) { input32[i] = choices32[digits % 3]; input64[i] = choices64[digits % 3]; }
			const struct ap_data_LNumbers32 *left32, *out_left32; const struct ap_data_RNumbers32 *right32, *out_right32;
			const struct ap_data_LNumbers64 *left64, *out_left64; const struct ap_data_RNumbers64 *right64, *out_right64;
			size_t written_left, written_right; int32_t measured;
			assert(!ap_from_LNumbers32(&arena, input32, length, &left32));
			assert(!ap_copy_LNumbers32(left32, a32, 4, &written_left) && written_left == length);
			assert(!ap_from_RNumbers32(&arena, a32, written_left, &right32));
			assert(!ap_from_LNumbers64(&arena, input64, length, &left64));
			assert(!ap_copy_LNumbers64(left64, a64, 4, &written_left) && written_left == length);
			assert(!ap_from_RNumbers64(&arena, a64, written_left, &right64));
			struct ap_c_allocation *prior = arena.first;
			for (unsigned truth = 0; truth < 2; ++truth) for (unsigned binary = 0; binary < 2; ++binary) {
				context.flag.tag = truth ? AP_ENUM_PFlag_C0 : AP_ENUM_PFlag_C1;
				if (binary) {
					assert(!ap_export_left_select32(&arena, signed_left_binary32(&context), left32, INT32_MIN, &out_left32));
					assert(!ap_export_right_select32(&arena, signed_right_binary32(&context), right32, INT32_MAX, &out_right32));
					assert(!ap_export_left_select64(&arena, signed_left_binary64(&context), left64, INT64_MIN, &out_left64));
					assert(!ap_export_right_select64(&arena, signed_right_binary64(&context), right64, INT64_MAX, &out_right64));
				} else {
					assert(!ap_export_left_filter32(&arena, signed_left_unary32(&context), left32, &out_left32));
					assert(!ap_export_right_filter32(&arena, signed_right_unary32(&context), right32, &out_right32));
					assert(!ap_export_left_filter64(&arena, signed_left_unary64(&context), left64, &out_left64));
					assert(!ap_export_right_filter64(&arena, signed_right_unary64(&context), right64, &out_right64));
				}
				size_t expected = truth ? length : 0;
				assert(!ap_copy_LNumbers32(out_left32, a32, 4, &written_left) && written_left == expected);
				assert(!ap_copy_RNumbers32(out_right32, b32, 4, &written_right) && written_right == expected);
				assert(!ap_export_left_length32(&arena, out_left32, &measured) && measured == (int32_t)expected);
				assert(!ap_export_right_length32(&arena, out_right32, &measured) && measured == (int32_t)expected);
				assert(!ap_copy_LNumbers64(out_left64, a64, 4, &written_left) && written_left == expected);
				assert(!ap_copy_RNumbers64(out_right64, b64, 4, &written_right) && written_right == expected);
				assert(!ap_export_left_length64(&arena, out_left64, &measured) && measured == (int32_t)expected);
				assert(!ap_export_right_length64(&arena, out_right64, &measured) && measured == (int32_t)expected);
				for (size_t i = 0; i < expected; ++i) {
					assert(a32[i] == input32[i] && b32[i] == input32[i]);
					assert(a64[i] == input64[i] && b64[i] == input64[i]);
				}
				assert(!arena.depth && context.flag.tag == (truth ? AP_ENUM_PFlag_C0 : AP_ENUM_PFlag_C1));
			}
			assert(prior);
			assert(!ap_copy_LNumbers32(left32, a32, 4, &written_left) && written_left == length);
			assert(!ap_copy_RNumbers64(right64, b64, 4, &written_right) && written_right == length);
			for (size_t i = 0; i < length; ++i) assert(a32[i] == input32[i] && b64[i] == input64[i]);
			ap_arena_RNumbers32_destroy(&arena); ++cases;
		}
	assert(cases == 121);
}

static struct ap_enum_LFlag invalid(void *context, int32_t n)
{
	(void)context; (void)n; return (struct ap_enum_LFlag){UINT32_MAX};
}

static void boundaries(const struct signed_provider_api *api)
{
	struct ap_enum_PFlag flag = {77}, bad = {77};
	assert(api->keep32(bad, INT32_MIN, &flag) == 2 && flag.tag == 77);
	assert(api->choose64(bad, INT64_MIN, INT64_MAX, &flag) == 2 && flag.tag == 77);
	assert(api->keep64((struct ap_enum_PFlag){AP_ENUM_PFlag_C0}, 0, NULL) == 1);
	struct signed_provider_context context = {api,{AP_ENUM_PFlag_C0}};
	struct ap_c_arena arena = {0}; int32_t values32[] = {INT32_MIN,0,INT32_MAX}; int64_t values64[] = {INT64_MIN,0,INT64_MAX};
	const struct ap_data_LNumbers32 *left, *out_left; const struct ap_data_RNumbers64 *right, *out_right;
	assert(!ap_from_LNumbers32(&arena, values32, 3, &left)); assert(!ap_from_RNumbers64(&arena, values64, 3, &right));
	out_left = left; out_right = right;
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	struct ap_c_predicate_signed1_i32_r5_LFlag missing = {0}, wrong = {NULL,invalid};
	assert(ap_export_left_filter32(&arena, missing, left, &out_left) == 2 && out_left == left);
	assert(ap_export_left_filter32(&arena, wrong, left, &out_left) == 2 && out_left == left);
	assert(ap_export_right_filter64(&arena, signed_right_unary64(&context), right, NULL) == 1);
	assert(ap_export_left_filter32(NULL, signed_left_unary32(&context), left, &out_left) == 1 && out_left == left);
	arena.capacity = count + 1;
	assert(ap_export_left_filter32(&arena, signed_left_unary32(&context), left, &out_left) == 3 && out_left == left);
	assert(ap_export_right_filter64(&arena, signed_right_unary64(&context), right, &out_right) == 3 && out_right == right);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_left_filter32(&arena, signed_left_unary32(&context), left, &out_left) == 4 && out_left == left);
	assert(ap_export_right_filter64(&arena, signed_right_unary64(&context), right, &out_right) == 4 && out_right == right);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0; arena.depth = 1;
	assert(ap_export_left_filter32(&arena, signed_left_unary32(&context), left, &out_left) == 4 && out_left == left);
	/* The provider is scalar-only and remains usable without the active arena. */
	assert(!api->keep64(context.flag, INT64_MIN, &flag) && flag.tag == AP_ENUM_PFlag_C0);
	assert(arena.first == mark && arena.count == count && arena.depth == 1); arena.depth = 0;
	int64_t copy[3] = {77,77,77}; size_t written = 77;
	assert(ap_copy_RNumbers64(right, copy, 2, &written) == 6 && written == 77 && copy[0] == 77);
	assert(!ap_export_right_filter64(&arena, signed_right_unary64(&context), right, &out_right));
	assert(!ap_copy_RNumbers64(out_right, copy, 3, &written) && written == 3);
	ap_arena_LNumbers32_destroy(&arena);
	for (size_t i = 0; i < 3; ++i) assert(copy[i] == values64[i]);
}

static void print_provider(const struct signed_provider_api *api)
{
	const int32_t left[] = {INT32_MIN,INT32_MAX,-1,1,0,0,INT32_MIN,INT32_MAX};
	const int32_t right[] = {0,0,1,-1,0,0,INT32_MAX,INT32_MIN};
	for (size_t i = 0; i < 8; ++i) {
		struct ap_enum_PFlag answer, flag = {(i & 1) ? AP_ENUM_PFlag_C1 : AP_ENUM_PFlag_C0};
		int status = i == 2 || i == 3 || i >= 6 ? api->choose32(flag,left[i],right[i],&answer) : api->keep32(flag,left[i],&answer);
		assert(!status); printf("%c|", answer.tag == AP_ENUM_PFlag_C0 ? 'T' : 'F');
	}
}

static void reference(const struct signed_provider_api *api)
{
	struct ap_c_arena arena = {0}; struct signed_provider_context context = {api,{AP_ENUM_PFlag_C0}};
	struct ap_enum_LFlag answer;
	for (size_t i = 0; i < 4; ++i) {
		context.flag.tag = (i & 1) ? AP_ENUM_PFlag_C1 : AP_ENUM_PFlag_C0;
		if (i < 2) assert(!ap_export_left_apply32(&arena, signed_left_unary32(&context), i ? INT32_MAX : INT32_MIN, &answer));
		else assert(!ap_export_left_choose32(&arena, signed_left_binary32(&context), i == 2 ? -1 : 1, i == 2 ? 1 : -1, &answer));
		printf("%c|", answer.tag ? 'T' : 'F');
	}
	int32_t values[] = {-1,0,1}, copy[3]; size_t written; const struct ap_data_LNumbers32 *input, *out;
	assert(!ap_from_LNumbers32(&arena, values, 3, &input));
	for (unsigned binary = 0; binary < 2; ++binary) for (unsigned drop = 0; drop < 2; ++drop) {
		context.flag.tag = drop ? AP_ENUM_PFlag_C1 : AP_ENUM_PFlag_C0;
		if (binary) assert(!ap_export_left_select32(&arena, signed_left_binary32(&context), input, 0, &out));
		else assert(!ap_export_left_filter32(&arena, signed_left_unary32(&context), input, &out));
		assert(!ap_copy_LNumbers32(out, copy, 3, &written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",", copy[i]);
		putchar('|');
	}
	ap_arena_LNumbers32_destroy(&arena);
}

int main(int argc, char **argv)
{
#ifdef DYNAMIC_PROVIDER
	assert(argc == 2); void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL); assert(handle);
	struct signed_provider_api api;
#define LOAD(field, name) do { void *code = dlsym(handle,name); assert(code); \
	_Static_assert(sizeof(code) == sizeof(api.field), "unsupported host function pointer model"); \
	memcpy(&api.field,&code,sizeof(code)); } while (0)
	LOAD(keep32,"ap_export_provider_keep32"); LOAD(keep64,"ap_export_provider_keep64");
	LOAD(choose32,"ap_export_provider_choose32"); LOAD(choose64,"ap_export_provider_choose64");
#undef LOAD
#else
	(void)argc; (void)argv;
	const struct signed_provider_api api = {ap_export_provider_keep32,ap_export_provider_keep64,ap_export_provider_choose32,ap_export_provider_choose64};
#endif
	scalar(&api); lists(&api); boundaries(&api); print_provider(&api); reference(&api);
	assert(api.keep32 && api.keep64 && api.choose32 && api.choose64);
#ifdef DYNAMIC_PROVIDER
	/* No descriptor invocation or borrowed result survives provider unload. */
	assert(!dlclose(handle));
#endif
	return 0;
}
