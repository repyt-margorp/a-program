#ifndef __SIGNED_PREDICATE_ADAPTER_H__
#define __SIGNED_PREDICATE_ADAPTER_H__

#ifdef REVERSE_HEADERS
#include "right/component.h"
#include "provider/component.h"
#include "left/component.h"
#else
#include "left/component.h"
#include "provider/component.h"
#include "right/component.h"
#endif

/* API, context and any loaded provider code outlive every synchronous call. */
struct signed_provider_api {
	int (*keep32)(struct ap_enum_PFlag, int32_t, struct ap_enum_PFlag *);
	int (*keep64)(struct ap_enum_PFlag, int64_t, struct ap_enum_PFlag *);
	int (*choose32)(struct ap_enum_PFlag, int32_t, int32_t, struct ap_enum_PFlag *);
	int (*choose64)(struct ap_enum_PFlag, int64_t, int64_t, struct ap_enum_PFlag *);
};

struct signed_provider_context {
	const struct signed_provider_api *api;
	struct ap_enum_PFlag flag;
};

struct ap_c_predicate_signed1_i32_r5_LFlag signed_left_unary32(const struct signed_provider_context *);
struct ap_c_predicate_signed1_i64_r5_LFlag signed_left_unary64(const struct signed_provider_context *);
struct ap_c_predicate_signed2_i32_r5_LFlag signed_left_binary32(const struct signed_provider_context *);
struct ap_c_predicate_signed2_i64_r5_LFlag signed_left_binary64(const struct signed_provider_context *);
struct ap_c_predicate_signed1_i32_r5_RFlag signed_right_unary32(const struct signed_provider_context *);
struct ap_c_predicate_signed1_i64_r5_RFlag signed_right_unary64(const struct signed_provider_context *);
struct ap_c_predicate_signed2_i32_r5_RFlag signed_right_binary32(const struct signed_provider_context *);
struct ap_c_predicate_signed2_i64_r5_RFlag signed_right_binary64(const struct signed_provider_context *);

#endif
