#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
	int32_t result;
	assert(!ap_export_nested(INT32_MIN, 65537, &result)); printf("%" PRId32 "|", result);
	assert(!ap_export_partial(INT32_MAX, &result)); printf("%" PRId32 "|", result);
	assert(!ap_export_through_fold(INT32_MAX, &result)); printf("%" PRId32 "|", result);
	assert(!ap_export_nested_capture(-99, &result)); printf("%" PRId32 "|", result);
	assert(!ap_export_shadowed(99, &result)); printf("%" PRId32 "|", result);
	assert(!ap_export_scoped(-99, &result)); printf("%" PRId32 "|", result);
}
