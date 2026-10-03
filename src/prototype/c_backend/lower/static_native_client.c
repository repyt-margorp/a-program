#include "component.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t allocations = SIZE_MAX;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	if (allocations == SIZE_MAX) return __real_malloc(size);
	if (!allocations) return NULL;
	--allocations;
	return __real_malloc(size);
}

int main(void)
{
	struct ap_c_arena arena = {0};
	uint32_t out = 77;
	assert(!ap_export_local_successor(&arena, 0, &out) && out == 1);
	assert(!ap_export_local_successor(&arena, UINT32_MAX - 1, &out) && out == UINT32_MAX);
	out = 77;
	assert(ap_export_local_successor(&arena, UINT32_MAX, &out) == 5 && out == 77);
	assert(!ap_export_unused_successor(&arena, UINT32_MAX, &out) && out == UINT32_MAX);
	assert(!ap_export_repeated_successor(&arena, UINT32_MAX - 2, &out) && out == UINT32_MAX);
	out = 77;
	assert(ap_export_repeated_successor(&arena, UINT32_MAX - 1, &out) == 5 && out == 77);
	assert(!arena.count && !arena.depth);

	uint32_t values[] = {0, 1, UINT32_MAX}, copied[4];
	const struct ap_data_Numbers *input, *result;
	assert(!ap_from_Numbers(&arena, values, 3, &input));
	struct ap_c_allocation *mark = arena.first;
	result = input;
	allocations = 0;
	assert(ap_export_captured_list(&arena, input, &result) == 3 && result == input);
	assert(arena.count == 4 && arena.first == mark && !arena.depth);
	allocations = 1;
	assert(ap_export_shadowed_list(&arena, input, &result) == 3 && result == input);
	assert(arena.count == 4 && arena.first == mark && !arena.depth);
	allocations = SIZE_MAX;
	assert(!ap_export_captured_list(&arena, input, &result) && result->fields.c1.f1 == input);
	size_t written;
	assert(!ap_copy_Numbers(result, copied, 4, &written) && written == 4);
	assert(copied[0] == 0 && copied[1] == 0 && copied[2] == 1 && copied[3] == UINT32_MAX);
	assert(!ap_export_shadowed_list(&arena, input, &result) && result->fields.c1.f1 == input);
	assert(!ap_copy_Numbers(result, copied, 4, &written) && written == 4);
	assert(copied[0] == 0 && copied[1] == 0 && copied[2] == 1 && copied[3] == UINT32_MAX);
	int32_t length = 77;
	assert(!ap_export_captured_length(&arena, input, &length) && length == 3);
	arena.depth_limit = 1; length = 77;
	mark = arena.first;
	size_t count = arena.count;
	assert(ap_export_captured_length(&arena, input, &length) == 4 && length == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	ap_arena_Nat_destroy(&arena);
	puts("Static native functions: borrowed captures, unused demand, magnitude/depth bounds and allocation rollback passed");
	return 0;
}
