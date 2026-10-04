#include "component.h"
#include <assert.h>
#ifndef TREE_DESTROY
#define TREE_DESTROY ap_arena_Rejected_destroy
#endif

int main(void)
{
	struct ap_data_Rejected leaf = {.tag = AP_DATA_Rejected_C0};
	struct ap_data_Rejected fork = {.tag = AP_DATA_Rejected_C1, .fields.c1 = {&leaf, &leaf}};
	struct ap_c_arena arena = {0}; const struct ap_data_Rejected *out;
	assert(!ap_export_tree(&arena, &fork, &out) && out == &fork);
	fork.fields.c1.f1 = &fork;
	assert(ap_export_tree(&arena, &fork, &out) == 2 && out == &fork);
	TREE_DESTROY(&arena); return 0;
}
