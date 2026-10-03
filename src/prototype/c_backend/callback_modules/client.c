#ifdef REVERSE_HEADERS
#include "right/component.h"
#include "provider/component.h"
#include "left/component.h"
#else
#include "left/component.h"
#include "provider/component.h"
#include "right/component.h"
#endif
#include "core_cases.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#ifdef DYNAMIC_PROVIDER
#include <dlfcn.h>
#include <string.h>
#endif

struct provider_api {
	int (*add32)(int32_t, int32_t, int32_t *);
	int (*neg32)(int32_t, int32_t *);
	int (*add64)(int64_t, int64_t, int64_t *);
};

struct context {
	const struct provider_api *api;
	int32_t offset32;
	int64_t offset64;
};

/* A valid generated scalar call supplies this callback's source interpretation.
	* The immutable borrowed context and provider code outlive the synchronous call. */
static int32_t foreign32(void *opaque, int32_t value)
{
	const struct context *context = opaque;
	int32_t result;
	assert(!context->api->add32(context->offset32, value, &result));
	return result;
}

static int64_t foreign64(void *opaque, int64_t value)
{
	const struct context *context = opaque;
	int64_t result;
	assert(!context->api->add64(context->offset64, value, &result));
	return result;
}

static int32_t negative32(void *opaque, int32_t value)
{
	const struct provider_api *api = opaque;
	int32_t result;
	assert(!api->neg32(value, &result));
	return result;
}

int main(int argc, char **argv)
{
#ifdef DYNAMIC_PROVIDER
	assert(argc == 2);
	void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	assert(handle);
	struct provider_api api;
	void *code = dlsym(handle, "ap_export_provider_add32");
	assert(code);
	_Static_assert(sizeof(code) == sizeof(api.add32), "unsupported host function pointer model");
	memcpy(&api.add32, &code, sizeof(code));
	code = dlsym(handle, "ap_export_provider_neg32"); assert(code);
	memcpy(&api.neg32, &code, sizeof(code));
	code = dlsym(handle, "ap_export_provider_add64"); assert(code);
	memcpy(&api.add64, &code, sizeof(code));
#else
	(void)argc; (void)argv;
	const struct provider_api api = {ap_export_provider_add32, ap_export_provider_neg32, ap_export_provider_add64};
#endif
	int32_t inputs32[] = {INT32_MIN, INT32_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT32_MAX - 1, INT32_MAX};
	int32_t offsets32[] = {INT32_MIN, -7, 0, 7, INT32_MAX};
	int64_t inputs64[] = {INT64_MIN, INT64_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT64_MAX - 1, INT64_MAX};
	int64_t offsets64[] = {INT64_MIN, -7, 0, 7, INT64_MAX};
	struct ap_c_callback_i32 negative = {(void *)&api, negative32}, missing32 = {NULL, NULL};
	struct ap_c_callback_i64 missing64 = {NULL, NULL};
	int32_t result32 = 99;
	int64_t result64 = 101;
	assert(ap_export_left_once32(negative, 0, NULL) == 1);
	assert(ap_export_right_once64(missing64, 0, NULL) == 1);
	assert(ap_export_left_once32(missing32, 0, &result32) == 2 && result32 == 99);
	assert(ap_export_right_once64(missing64, 0, &result64) == 2 && result64 == 101);
	for (size_t i = 0; i < 5; ++i) for (size_t j = 0; j < 10; ++j) {
		const struct context context = {&api, offsets32[i], offsets64[i]};
		struct ap_c_callback_i32 callback32 = {(void *)&context, foreign32};
		struct ap_c_callback_i64 callback64 = {(void *)&context, foreign64};
		int32_t x = inputs32[j];
		int64_t y = inputs64[j];
		const uint64_t *expected = core_cases[i * 10 + j];
#define CHECK(prefix) \
		assert(!ap_export_##prefix##_once32(callback32, x, &result32) && (uint32_t)result32 == expected[0]); \
		assert(!ap_export_##prefix##_twice32(callback32, x, &result32) && (uint32_t)result32 == expected[1]); \
		assert(!ap_export_##prefix##_compose32(callback32, negative, x, &result32) && (uint32_t)result32 == expected[2]); \
		assert(!ap_export_##prefix##_captured32(callback32, x, &result32) && (uint32_t)result32 == expected[3]); \
		assert(!ap_export_##prefix##_unused32(callback32, x, &result32) && (uint32_t)result32 == expected[4]); \
		assert(!ap_export_##prefix##_once64(callback64, y, &result64) && (uint64_t)result64 == expected[5]); \
		assert(!ap_export_##prefix##_twice64(callback64, y, &result64) && (uint64_t)result64 == expected[6]); \
		assert(!ap_export_##prefix##_captured64(callback64, y, &result64) && (uint64_t)result64 == expected[7])
		CHECK(left);
		CHECK(right);
#undef CHECK
		assert(context.offset32 == offsets32[i] && context.offset64 == offsets64[i]);
	}
	const struct context observed = {&api, 7, 7};
	struct ap_c_callback_i32 add = {(void *)&observed, foreign32};
	int32_t observations[] = {0, 17, INT32_MIN, INT32_MAX};
	for (size_t i = 0; i < 4; ++i) {
		int32_t x = observations[i];
		assert(!ap_export_left_once32(add, x, &result32)); printf("%" PRId32 "|", result32);
		assert(!ap_export_left_twice32(add, x, &result32)); printf("%" PRId32 "|", result32);
		assert(!ap_export_left_compose32(add, negative, x, &result32)); printf("%" PRId32 "|", result32);
		assert(!ap_export_left_captured32(add, x, &result32)); printf("%" PRId32 "|", result32);
		assert(!ap_export_left_unused32(add, x, &result32)); printf("%" PRId32 "|", result32);
	}
#ifdef DYNAMIC_PROVIDER
	/* Unload only after every borrowed callback/context has finished its calls. */
	assert(!dlclose(handle));
#endif
	return 0;
}
