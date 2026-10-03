#include "component.h"
#include <assert.h>

/* Keep the exact earlier refusal shapes as explicitly exercised positives. */
int main(void)
{
	struct ap_c_arena arena = {0};
	size_t written = 77;
#ifdef FORMER_NESTED_RECORD
	struct ap_data_Envelope input = {.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = AP_DATA_Packet_C1, .fields.c1 = {7, {1}}}, 3}}, output;
	const struct ap_data_Recursive *list;
	assert(!ap_from_Recursive(&arena, &input, 1, &list));
	assert(!ap_copy_Recursive(list, &output, 1, &written) && written == 1);
	assert(output.fields.c1.f0.fields.c1.f0 == 7 && output.fields.c1.f1 == 3);
	input.fields.c1.f0.fields.c1.f1.tag = 99;
	const struct ap_data_Recursive *prior = list;
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	assert(ap_from_Recursive(&arena, &input, 1, &list) == 2 && list == prior);
	assert(arena.first == mark && arena.count == count);
	ap_arena_Recursive_destroy(&arena);
#else
	struct ap_data_Value input = {.tag = AP_DATA_Value_C0, .fields.c0 = {INT32_MAX}}, output;
	const struct ap_data_Aggregates *list;
	assert(!ap_from_Aggregates(&arena, &input, 1, &list));
	assert(!ap_copy_Aggregates(list, &output, 1, &written) && written == 1);
	assert(output.tag == AP_DATA_Value_C0 && output.fields.c0.f0 == INT32_MAX);
	input.tag = 99;
	const struct ap_data_Aggregates *prior = list;
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	assert(ap_from_Aggregates(&arena, &input, 1, &list) == 2 && list == prior);
	assert(arena.first == mark && arena.count == count);
	ap_arena_Flags_destroy(&arena);
#endif
	assert(!arena.first && !arena.count && !arena.status && !arena.depth);
	return 0;
}
