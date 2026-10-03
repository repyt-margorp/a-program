#include "adapter.h"
#include <assert.h>

/* Provider success and pure-total interpretation remain caller preconditions.
	* A separate callback-local arena avoids the active consumer arena. */
static unsigned provider_truth(const struct module_provider_api *api, uint32_t left, uint32_t right, int binary)
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

struct ap_c_predicate1_d4_LNat_r5_LFlag module_left_unary(const struct module_provider_api *api)
{
	return (struct ap_c_predicate1_d4_LNat_r5_LFlag){(void *)api, left_keep};
}

struct ap_c_predicate1_d4_RNat_r5_RFlag module_right_unary(const struct module_provider_api *api)
{
	return (struct ap_c_predicate1_d4_RNat_r5_RFlag){(void *)api, right_keep};
}

struct ap_c_predicate2_d4_LNat_r5_LFlag module_left_binary(const struct module_provider_api *api)
{
	return (struct ap_c_predicate2_d4_LNat_r5_LFlag){(void *)api, left_choose};
}

struct ap_c_predicate2_d4_RNat_r5_RFlag module_right_binary(const struct module_provider_api *api)
{
	return (struct ap_c_predicate2_d4_RNat_r5_RFlag){(void *)api, right_choose};
}
