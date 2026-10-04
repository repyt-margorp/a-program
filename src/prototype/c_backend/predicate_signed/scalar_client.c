#include "component.h"
#include <assert.h>
#include <stddef.h>

static struct ap_enum_Bool compare(void *context, int64_t left, int64_t right)
{
	(void)context; return (struct ap_enum_Bool){left <= right};
}

static struct ap_enum_Bool invalid(void *context, int64_t left, int64_t right)
{
	(void)context; (void)left; (void)right; return (struct ap_enum_Bool){UINT32_MAX};
}

int main(void)
{
	struct ap_c_predicate_signed2_i64_r4_Bool predicate = {NULL, compare};
	struct ap_enum_Bool out = {77};
	assert(!ap_export_choose64(predicate, INT64_MIN, INT64_MAX, &out) && out.tag == 1);
	assert(!ap_export_choose64(predicate, INT64_MAX, INT64_MIN, &out) && out.tag == 0);
	predicate.call = invalid; out.tag = 77;
	assert(ap_export_choose64(predicate, 0, 0, &out) == 2 && out.tag == 77);
	predicate.call = NULL;
	assert(ap_export_choose64(predicate, 0, 0, &out) == 2 && out.tag == 77);
	predicate.call = compare;
	assert(!ap_export_choose64(predicate, 0, 0, &out) && out.tag == 1);
	return 0;
}
