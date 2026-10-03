#ifdef REVERSE_HEADERS
#include "right/component.h"
#include "provider/component.h"
#include "left/component.h"
#else
#include "left/component.h"
#include "provider/component.h"
#include "right/component.h"
#endif
#include <assert.h>
#include <stdio.h>
#ifdef DYNAMIC_PROVIDER
#include <dlfcn.h>
#include <string.h>
#endif

struct provider_api {
	int (*keep)(struct ap_c_arena *, uint32_t, struct ap_enum_PFlag *);
	int (*choose)(struct ap_c_arena *, uint32_t, uint32_t, struct ap_enum_PFlag *);
	void (*destroy)(struct ap_c_arena *);
};

/* Each generated provider call has a separate local arena. The consumer's
	* arena is active during recursion; the immutable borrowed API outlives calls.
	* Provider success is a precondition, without a new foreign error protocol. */
static unsigned provider_truth(const struct provider_api *api, uint32_t left, uint32_t right, int binary)
{
	struct ap_c_arena local = {.depth_limit = 1};
	struct ap_enum_PFlag answer = {UINT32_MAX};
	int status = binary ? api->choose(&local, left, right, &answer) : api->keep(&local, left, &answer);
	assert(!status && !local.first && !local.count && !local.depth);
	assert(answer.tag == AP_ENUM_PFlag_C0 || answer.tag == AP_ENUM_PFlag_C1);
	api->destroy(&local);
	/* Provider order is true/false; consumer order is false/true. */
	return answer.tag == AP_ENUM_PFlag_C0;
}

static struct ap_enum_LFlag left_keep(void *context, uint32_t n)
{
	return (struct ap_enum_LFlag){provider_truth(context, n, 0, 0) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}

static struct ap_enum_RFlag right_keep(void *context, uint32_t n)
{
	return (struct ap_enum_RFlag){provider_truth(context, n, 0, 0) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}

static struct ap_enum_LFlag left_choose(void *context, uint32_t left, uint32_t right)
{
	return (struct ap_enum_LFlag){provider_truth(context, left, right, 1) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}

static struct ap_enum_RFlag right_choose(void *context, uint32_t left, uint32_t right)
{
	return (struct ap_enum_RFlag){provider_truth(context, left, right, 1) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}

static void scalar(const struct provider_api *api)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d4_LNat_r5_LFlag unary_left = {(void *)api, left_keep};
	struct ap_c_predicate1_d4_RNat_r5_RFlag unary_right = {(void *)api, right_keep};
	struct ap_c_predicate2_d4_LNat_r5_LFlag binary_left = {(void *)api, left_choose};
	struct ap_c_predicate2_d4_RNat_r5_RFlag binary_right = {(void *)api, right_choose};
	struct ap_enum_LFlag a;
	struct ap_enum_RFlag b;
	uint32_t extrema[] = {0, 1, 8, UINT32_MAX - 1, UINT32_MAX};
	for (size_t i = 0; i < 5; ++i) {
		uint32_t left = extrema[i];
		assert(!ap_export_left_apply(&arena, unary_left, left, &a) && a.tag == (left != 0));
		assert(!ap_export_right_apply(&arena, unary_right, left, &b) && b.tag == a.tag);
		for (size_t j = 0; j < 5; ++j) {
			uint32_t right = extrema[j];
			assert(!ap_export_left_choose(&arena, binary_left, left, right, &a) && a.tag == (!left || right));
			assert(!ap_export_right_choose(&arena, binary_right, left, right, &b) && b.tag == a.tag);
		}
	}
	for (uint32_t left = 0; left <= 8; ++left) {
		assert(!ap_export_left_apply(&arena, unary_left, left, &a) && a.tag == (left != 0));
		assert(!ap_export_right_apply(&arena, unary_right, left, &b) && b.tag == a.tag);
		printf("%c|", a.tag ? 'T' : 'F');
		for (uint32_t right = 0; right <= 8; ++right) {
			assert(!ap_export_left_choose(&arena, binary_left, left, right, &a) && a.tag == (!left || right));
			assert(!ap_export_right_choose(&arena, binary_right, left, right, &b) && b.tag == a.tag);
			printf("%c|", a.tag ? 'T' : 'F');
		}
	}
	assert(!arena.first && !arena.count && !arena.depth);
	ap_arena_LNat_destroy(&arena);
}

static void lists(const struct provider_api *api)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d4_LNat_r5_LFlag unary_left = {(void *)api, left_keep};
	struct ap_c_predicate1_d4_RNat_r5_RFlag unary_right = {(void *)api, right_keep};
	struct ap_c_predicate2_d4_LNat_r5_LFlag binary_left = {(void *)api, left_choose};
	struct ap_c_predicate2_d4_RNat_r5_RFlag binary_right = {(void *)api, right_choose};
	size_t cases = 0, combinations = 1;
	for (size_t length = 0; length <= 4; ++length, combinations *= 3) {
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t values[4], a[4], b[4]; size_t digits = code;
			for (size_t i = 0; i < length; ++i) { values[i] = digits % 3; digits /= 3; }
			const struct ap_data_LNumbers *left, *out_left;
			const struct ap_data_RNumbers *right, *out_right;
			assert(!ap_from_LNumbers(&arena, values, length, &left));
			size_t written_left, written_right, expected = 0;
			assert(!ap_copy_LNumbers(left, a, 4, &written_left) && written_left == length);
			assert(!ap_from_RNumbers(&arena, a, written_left, &right));
			int32_t measured;
			assert(!ap_export_left_length(&arena, left, &measured) && measured == (int32_t)length);
			assert(!ap_export_right_length(&arena, right, &measured) && measured == (int32_t)length);
			assert(!ap_export_left_filter(&arena, unary_left, left, &out_left));
			assert(!ap_export_right_filter(&arena, unary_right, right, &out_right));
			assert(!ap_copy_LNumbers(out_left, a, 4, &written_left));
			assert(!ap_copy_RNumbers(out_right, b, 4, &written_right));
			for (size_t i = 0; i < length; ++i) if (values[i]) {
				assert(a[expected] == values[i] && b[expected] == values[i]); ++expected;
			}
			assert(written_left == expected && written_right == expected);
			for (uint32_t pivot = 0; pivot <= 2; ++pivot) {
				assert(!ap_export_left_select(&arena, binary_left, left, pivot, &out_left));
				assert(!ap_export_right_select(&arena, binary_right, right, pivot, &out_right));
				assert(!ap_copy_LNumbers(out_left, a, 4, &written_left));
				assert(!ap_copy_RNumbers(out_right, b, 4, &written_right));
				expected = 0;
				for (size_t i = 0; i < length; ++i) if (!values[i] || pivot) {
					assert(a[expected] == values[i] && b[expected] == values[i]); ++expected;
				}
				assert(written_left == expected && written_right == expected && !arena.depth);
			}
			assert(!ap_copy_LNumbers(left, a, 4, &written_left));
			assert(!ap_copy_RNumbers(right, b, 4, &written_right));
			for (size_t i = 0; i < length; ++i) assert(a[i] == values[i] && b[i] == values[i]);
			ap_arena_RNat_destroy(&arena); ++cases;
		}
	}
	assert(cases == 121);
}

static void boundaries(const struct provider_api *api)
{
	struct ap_c_arena provider_arena = {.depth = 1, .depth_limit = 1};
	struct ap_enum_PFlag provider_flag = {77};
	assert(api->choose(&provider_arena, UINT32_MAX, UINT32_MAX, &provider_flag) == 4 && provider_flag.tag == 77);
	assert(!provider_arena.first && !provider_arena.count && provider_arena.depth == 1);
	provider_arena.depth = 0;
	assert(!api->choose(&provider_arena, UINT32_MAX, UINT32_MAX, &provider_flag) && provider_flag.tag == AP_ENUM_PFlag_C0);
	api->destroy(&provider_arena);
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d4_LNat_r5_LFlag unary = {(void *)api, left_keep}, missing = {NULL, NULL};
	struct ap_c_predicate2_d4_RNat_r5_RFlag binary = {(void *)api, right_choose};
	static const struct ap_data_LNumbers nil_left = {.tag = AP_DATA_LNumbers_C0};
	static const struct ap_data_RNumbers nil_right = {.tag = AP_DATA_RNumbers_C0};
	const struct ap_data_LNumbers *left, *out_left = &nil_left;
	const struct ap_data_RNumbers *right, *out_right = &nil_right;
	uint32_t values[] = {0, 1}, buffer[] = {77, 77}; size_t written = 77;
	assert(!ap_from_LNumbers(&arena, values, 2, &left));
	assert(!ap_from_RNumbers(&arena, values, 2, &right));
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	assert(ap_export_left_filter(&arena, missing, &nil_left, &out_left) == 2 && out_left == &nil_left);
	assert(ap_export_right_select(&arena, binary, right, 1, NULL) == 1);
	assert(ap_export_left_filter(NULL, unary, left, &out_left) == 1 && out_left == &nil_left);
	arena.capacity = count + 1;
	assert(ap_export_left_filter(&arena, unary, left, &out_left) == 3 && out_left == &nil_left);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = count + 2;
	assert(ap_export_right_select(&arena, binary, right, 1, &out_right) == 3 && out_right == &nil_right);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_left_filter(&arena, unary, left, &out_left) == 4 && out_left == &nil_left);
	assert(ap_export_right_select(&arena, binary, right, 1, &out_right) == 4 && out_right == &nil_right);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0; arena.depth = 1;
	assert(ap_export_left_filter(&arena, unary, left, &out_left) == 4 && out_left == &nil_left);
	arena.depth = 0;
	assert(ap_copy_LNumbers(left, buffer, 1, &written) == 6 && written == 77 && buffer[0] == 77);
	assert(!ap_copy_LNumbers(left, buffer, 2, &written) && written == 2 && buffer[0] == 0 && buffer[1] == 1);
	assert(!ap_copy_RNumbers(right, buffer, 2, &written) && written == 2 && buffer[0] == 0 && buffer[1] == 1);
	assert(!ap_export_left_filter(&arena, unary, left, &out_left));
	assert(!ap_export_right_select(&arena, binary, right, 1, &out_right));
	assert(!ap_copy_RNumbers(out_right, buffer, 2, &written) && written == 2);
	ap_arena_LNat_destroy(&arena);
	assert(buffer[0] == 0 && buffer[1] == 1);
}

static void reference(const struct provider_api *api)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d4_LNat_r5_LFlag unary = {(void *)api, left_keep};
	struct ap_c_predicate2_d4_LNat_r5_LFlag binary = {(void *)api, left_choose};
	uint32_t values[] = {0, 1, 2, 0}, buffer[4]; size_t written;
	const struct ap_data_LNumbers *input, *out;
	assert(!ap_from_LNumbers(&arena, values, 4, &input));
	assert(!ap_export_left_filter(&arena, unary, input, &out));
	assert(!ap_copy_LNumbers(out, buffer, 4, &written));
	for (size_t i = 0; i < written; ++i) printf("%u,", (unsigned)buffer[i]);
	putchar('|');
	for (uint32_t pivot = 0; pivot <= 1; ++pivot) {
		assert(!ap_export_left_select(&arena, binary, input, pivot, &out));
		assert(!ap_copy_LNumbers(out, buffer, 4, &written));
		for (size_t i = 0; i < written; ++i) printf("%u,", (unsigned)buffer[i]);
		putchar('|');
	}
	ap_arena_LNat_destroy(&arena);
}

int main(int argc, char **argv)
{
#ifdef DYNAMIC_PROVIDER
	assert(argc == 2);
	void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL); assert(handle);
	struct provider_api api;
	void *code = dlsym(handle, "ap_export_provider_keep"); assert(code);
	_Static_assert(sizeof(code) == sizeof(api.keep), "unsupported host function pointer model");
	memcpy(&api.keep, &code, sizeof(code));
	code = dlsym(handle, "ap_export_provider_choose"); assert(code);
	memcpy(&api.choose, &code, sizeof(code));
	code = dlsym(handle, "ap_arena_PNat_destroy"); assert(code);
	memcpy(&api.destroy, &code, sizeof(code));
#else
	(void)argc; (void)argv;
	const struct provider_api api = {ap_export_provider_keep, ap_export_provider_choose, ap_arena_PNat_destroy};
#endif
	scalar(&api); lists(&api); boundaries(&api); reference(&api);
	assert(api.keep && api.choose && api.destroy);
#ifdef DYNAMIC_PROVIDER
	/* Every synchronous call and its callback-local arena ended before unload. */
	assert(!dlclose(handle));
#endif
	return 0;
}
