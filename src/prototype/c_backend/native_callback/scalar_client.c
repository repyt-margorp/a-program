#include "component.h"
#include <assert.h>
#include <stddef.h>

static int64_t negate(void *context, int64_t n)
{
	(void)context; uint64_t result = UINT64_C(0) - (uint64_t)n;
	return result <= INT64_MAX ? (int64_t)result : -1 - (int64_t)(UINT64_MAX-result);
}

int main(void)
{
	struct ap_c_callback_i64 callback = {NULL,negate}; int64_t out = 77;
	assert(!ap_export_apply64(callback,INT64_MIN,&out) && out == INT64_MIN);
	assert(!ap_export_apply64(callback,INT64_MAX,&out) && out == -INT64_MAX);
	callback.call = NULL; out = 77;
	assert(ap_export_apply64(callback,0,&out) == 2 && out == 77);
	callback.call = negate; assert(ap_export_apply64(callback,0,NULL) == 1);
	assert(!ap_export_apply64(callback,-1,&out) && out == 1);
	return 0;
}
