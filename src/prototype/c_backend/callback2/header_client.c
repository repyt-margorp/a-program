#ifdef REVERSE_HEADERS
#include "legacy/component.h"
#include "component.h"
#else
#include "component.h"
#include "legacy/component.h"
#endif
#include <assert.h>
#include <limits.h>

static int32_t subtract(void *context, int32_t x, int32_t y)
{
	(void)context;
	uint32_t bits = (uint32_t)x - (uint32_t)y;
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

int main(void)
{
	assert(AP_C_CALLBACK_ABI == 1 && AP_C_CALLBACK2_ABI == 1);
	struct ap_c_callback2_i32 callback = {0, subtract};
	int32_t binary, legacy;
	assert(!ap_export_apply32(callback, INT32_MIN, 1, &binary));
	assert(!ap_export_legacy_subtract(INT32_MIN, 1, &legacy));
	assert(binary == INT32_MAX && binary == legacy);
	return 0;
}
