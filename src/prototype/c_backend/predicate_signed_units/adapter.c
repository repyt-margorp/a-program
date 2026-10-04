#include "adapter.h"
#include <assert.h>

/* Successful pure-total provider interpretation is a caller precondition.
	* The signed native provider has no arena or foreign failure protocol. */
static unsigned provider_truth(const struct signed_provider_context *context, int64_t left, int64_t right, int wide, int binary)
{
	struct ap_enum_PFlag answer = {UINT32_MAX};
	const struct signed_provider_api *api = context->api;
	int status;
	if (wide) status = binary ? api->choose64(context->flag, left, right, &answer) : api->keep64(context->flag, left, &answer);
	else status = binary ? api->choose32(context->flag, (int32_t)left, (int32_t)right, &answer) : api->keep32(context->flag, (int32_t)left, &answer);
	assert(!status && (answer.tag == AP_ENUM_PFlag_C0 || answer.tag == AP_ENUM_PFlag_C1));
	/* Provider true/false order differs from both consumers' false/true order. */
	return answer.tag == AP_ENUM_PFlag_C0;
}

static struct ap_enum_LFlag left_unary32(void *context, int32_t n)
{
	return (struct ap_enum_LFlag){provider_truth(context, n, 0, 0, 0) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}
static struct ap_enum_LFlag left_unary64(void *context, int64_t n)
{
	return (struct ap_enum_LFlag){provider_truth(context, n, 0, 1, 0) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}
static struct ap_enum_RFlag right_unary32(void *context, int32_t n)
{
	return (struct ap_enum_RFlag){provider_truth(context, n, 0, 0, 0) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}
static struct ap_enum_RFlag right_unary64(void *context, int64_t n)
{
	return (struct ap_enum_RFlag){provider_truth(context, n, 0, 1, 0) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}
static struct ap_enum_LFlag left_binary32(void *context, int32_t left, int32_t right)
{
	return (struct ap_enum_LFlag){provider_truth(context, left, right, 0, 1) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}
static struct ap_enum_LFlag left_binary64(void *context, int64_t left, int64_t right)
{
	return (struct ap_enum_LFlag){provider_truth(context, left, right, 1, 1) ? AP_ENUM_LFlag_C1 : AP_ENUM_LFlag_C0};
}
static struct ap_enum_RFlag right_binary32(void *context, int32_t left, int32_t right)
{
	return (struct ap_enum_RFlag){provider_truth(context, left, right, 0, 1) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}
static struct ap_enum_RFlag right_binary64(void *context, int64_t left, int64_t right)
{
	return (struct ap_enum_RFlag){provider_truth(context, left, right, 1, 1) ? AP_ENUM_RFlag_C1 : AP_ENUM_RFlag_C0};
}

struct ap_c_predicate_signed1_i32_r5_LFlag signed_left_unary32(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed1_i32_r5_LFlag){(void *)context,left_unary32};
}
struct ap_c_predicate_signed1_i64_r5_LFlag signed_left_unary64(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed1_i64_r5_LFlag){(void *)context,left_unary64};
}
struct ap_c_predicate_signed2_i32_r5_LFlag signed_left_binary32(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed2_i32_r5_LFlag){(void *)context,left_binary32};
}
struct ap_c_predicate_signed2_i64_r5_LFlag signed_left_binary64(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed2_i64_r5_LFlag){(void *)context,left_binary64};
}
struct ap_c_predicate_signed1_i32_r5_RFlag signed_right_unary32(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed1_i32_r5_RFlag){(void *)context,right_unary32};
}
struct ap_c_predicate_signed1_i64_r5_RFlag signed_right_unary64(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed1_i64_r5_RFlag){(void *)context,right_unary64};
}
struct ap_c_predicate_signed2_i32_r5_RFlag signed_right_binary32(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed2_i32_r5_RFlag){(void *)context,right_binary32};
}
struct ap_c_predicate_signed2_i64_r5_RFlag signed_right_binary64(const struct signed_provider_context *context)
{
	return (struct ap_c_predicate_signed2_i64_r5_RFlag){(void *)context,right_binary64};
}
