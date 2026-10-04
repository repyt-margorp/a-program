#include "adapter.h"
#include <assert.h>

/* Successful pure-total provider interpretation is a caller precondition.
	* Scalar providers need no arena; no foreign failure protocol is added. */
static int32_t unary32(void *storage, int32_t n)
{
	const struct native_scalar_provider_context *context = storage; int32_t out = 77;
	int status = context->api->offset32(context->offset32, n, &out); assert(!status); return out;
}
static int64_t unary64(void *storage, int64_t n)
{
	const struct native_scalar_provider_context *context = storage; int64_t out = 77;
	int status = context->api->offset64(context->offset64, n, &out); assert(!status); return out;
}
static int32_t binary32(void *storage, int32_t left, int32_t right)
{
	const struct native_scalar_provider_context *context = storage; int32_t out = 77;
	int status = context->api->subtract32(left, right, &out); assert(!status); return out;
}
static int64_t binary64(void *storage, int64_t left, int64_t right)
{
	const struct native_scalar_provider_context *context = storage; int64_t out = 77;
	int status = context->api->subtract64(left, right, &out); assert(!status); return out;
}

struct ap_c_callback_i32 native_scalar_unary32(const struct native_scalar_provider_context *context)
{
	return (struct ap_c_callback_i32){(void *)context, unary32};
}
struct ap_c_callback_i64 native_scalar_unary64(const struct native_scalar_provider_context *context)
{
	return (struct ap_c_callback_i64){(void *)context, unary64};
}
struct ap_c_callback2_i32 native_scalar_binary32(const struct native_scalar_provider_context *context)
{
	return (struct ap_c_callback2_i32){(void *)context, binary32};
}
struct ap_c_callback2_i64 native_scalar_binary64(const struct native_scalar_provider_context *context)
{
	return (struct ap_c_callback2_i64){(void *)context, binary64};
}
