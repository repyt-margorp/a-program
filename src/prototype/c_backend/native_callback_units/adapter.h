#ifndef __NATIVE_SCALAR_ADAPTER_H__
#define __NATIVE_SCALAR_ADAPTER_H__

#ifdef REVERSE_HEADERS
#include "provider/component.h"
#include "consumer/component.h"
#else
#include "consumer/component.h"
#include "provider/component.h"
#endif

/* API, immutable context and loaded code outlive every synchronous call. */
struct native_scalar_provider_api {
	int (*offset32)(int32_t, int32_t, int32_t *);
	int (*offset64)(int64_t, int64_t, int64_t *);
	int (*subtract32)(int32_t, int32_t, int32_t *);
	int (*subtract64)(int64_t, int64_t, int64_t *);
};
struct native_scalar_provider_context {
	const struct native_scalar_provider_api *api;
	int32_t offset32;
	int64_t offset64;
};

struct ap_c_callback_i32 native_scalar_unary32(const struct native_scalar_provider_context *);
struct ap_c_callback_i64 native_scalar_unary64(const struct native_scalar_provider_context *);
struct ap_c_callback2_i32 native_scalar_binary32(const struct native_scalar_provider_context *);
struct ap_c_callback2_i64 native_scalar_binary64(const struct native_scalar_provider_context *);

#endif
