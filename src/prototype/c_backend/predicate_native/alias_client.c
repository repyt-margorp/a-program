#include "component.h"
#include <assert.h>

static struct ap_enum_C first(void *context, uint32_t n)
{
	(void)context;
	return (struct ap_enum_C){n != 0};
}

static struct ap_enum_B_C second(void *context, uint32_t n)
{
	(void)context;
	return (struct ap_enum_B_C){n ? AP_ENUM_B_C_C0 : AP_ENUM_B_C_C1};
}

int main(void)
{
	struct ap_c_arena arena = {0};
	struct ap_c_predicate1_d3_A_B_r1_C left = {NULL, first};
	struct ap_c_predicate1_d1_A_r3_B_C right = {NULL, second};
	struct ap_enum_C a = {77};
	struct ap_enum_B_C b = {77};
	assert(!ap_export_first(&arena, left, 0, &a) && a.tag == 0);
	assert(!ap_export_second(&arena, right, 0, &b) && b.tag == 1);
	assert(!ap_export_first(&arena, left, UINT32_MAX, &a) && a.tag == 1);
	assert(!ap_export_second(&arena, right, UINT32_MAX, &b) && b.tag == 0);
	ap_arena_A_B_destroy(&arena);
	return 0;
}
