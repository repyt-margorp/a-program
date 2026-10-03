#ifndef __PREDICATE_ADAPTER_H__
#define __PREDICATE_ADAPTER_H__

#ifdef REVERSE_HEADERS
#include "right/component.h"
#include "provider/component.h"
#include "left/component.h"
#else
#include "left/component.h"
#include "provider/component.h"
#include "right/component.h"
#endif

/* The caller retains this immutable API and loaded code during every call. */
struct module_provider_api {
	int (*keep)(struct ap_c_arena *, uint32_t, struct ap_enum_PFlag *);
	int (*choose)(struct ap_c_arena *, uint32_t, uint32_t, struct ap_enum_PFlag *);
	void (*destroy)(struct ap_c_arena *);
};

struct ap_c_predicate1_d4_LNat_r5_LFlag module_left_unary(const struct module_provider_api *);
struct ap_c_predicate1_d4_RNat_r5_RFlag module_right_unary(const struct module_provider_api *);
struct ap_c_predicate2_d4_LNat_r5_LFlag module_left_binary(const struct module_provider_api *);
struct ap_c_predicate2_d4_RNat_r5_RFlag module_right_binary(const struct module_provider_api *);

#endif
