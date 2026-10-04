#include "component.h"
#include <assert.h>
#include <stddef.h>

static struct ap_enum_Bool predicate(void *context, int64_t n)
{
	(void)n; return (struct ap_enum_Bool){*(const unsigned *)context};
}

int main(void)
{
	unsigned tag = 1; struct ap_c_predicate_signed1_i64_r4_Bool callback = {&tag,predicate};
	struct ap_enum_Bool flag = {77}; struct ap_data_Packet packet = {.tag = 77};
	assert(!ap_export_truth(&flag) && flag.tag == 1);
	for (tag = 0; tag < 2; ++tag) {
		assert(!ap_export_chain64(callback, INT64_MIN, &flag) && flag.tag == tag);
		assert(!ap_export_record64(callback, INT64_MAX, &packet) && !packet.tag);
		assert(packet.fields.c0.f0.tag == tag && packet.fields.c0.f1 == INT64_MAX);
	}
	tag = UINT32_MAX; flag.tag = 77; packet.tag = 77;
	assert(ap_export_chain64(callback, 0, &flag) == 2 && flag.tag == 77);
	assert(ap_export_record64(callback, 0, &packet) == 2 && packet.tag == 77);
	callback.call = NULL;
	assert(ap_export_record64(callback, 0, &packet) == 2 && packet.tag == 77);
	callback.call = predicate; tag = 1;
	assert(!ap_export_record64(callback, INT64_MIN, &packet) && packet.fields.c0.f1 == INT64_MIN);
	return 0;
}
