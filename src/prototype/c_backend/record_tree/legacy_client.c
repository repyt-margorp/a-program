#include "component.h"
#include <assert.h>

int main(void)
{
	struct ap_data_RecordTree leaf = {.tag = 0, .fields.c0 = {{.tag = 0, .fields.c0 = {7}}}};
	struct ap_data_RecordTree root = {.tag = 1, .fields.c1 = {&leaf, &leaf}};
	struct ap_c_arena arena = {0}; const struct ap_data_RecordTree *out;
	assert(!ap_export_record_identity(&arena, &root, &out) && out == &root);
	leaf.fields.c0.f0.tag = 99; out = &leaf;
	assert(ap_export_record_identity(&arena, &root, &out) == 2 && out == &leaf);
	leaf.fields.c0.f0.tag = 0; root.fields.c1.f1 = &root;
	assert(ap_export_record_identity(&arena, &root, &out) == 2 && out == &leaf);
	ap_arena_Tree_destroy(&arena); return 0;
}
