#include "left/component.h"
#include "right/component.h"
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
	const uint32_t values[] = {1, 2, UINT32_MAX};
	uint32_t buffer[4];
	size_t written = 77;
	const struct ap_data_Numbers *left;
	const struct ap_data_OtherNumbers *right, *extended;
	assert(!ap_from_Numbers(&arena, values, 3, &left));
	assert(!ap_copy_Numbers(left, buffer, 4, &written) && written == 3);
	assert(!ap_from_OtherNumbers(&arena, buffer, written, &right));
	assert(arena.count == 8 && !arena.depth);
	int32_t length = 77;
	assert(!ap_export_captured_length(&arena, left, &length) && length == 3);
	assert(!ap_export_other_captured_length(&arena, right, &length) && length == 3);
	struct ap_c_allocation *mark = arena.first;
	extended = right;
	allocations = 0;
	assert(ap_export_other_captured_list(&arena, right, &extended) == 3 && extended == right);
	assert(arena.first == mark && arena.count == 8 && !arena.depth);
	allocations = 1;
	assert(ap_from_OtherNumbers(&arena, values, 3, &extended) == 3 && extended == right);
	assert(arena.first == mark && arena.count == 8 && !arena.depth);
	allocations = SIZE_MAX;
	assert(!ap_copy_Numbers(left, buffer, 4, &written) && written == 3);
	assert(buffer[0] == 1 && buffer[1] == 2 && buffer[2] == UINT32_MAX);
	assert(!ap_export_other_captured_list(&arena, right, &extended));
	assert(extended->fields.c1.f1 == right);
	assert(!ap_copy_OtherNumbers(extended, buffer, 4, &written) && written == 4);
	assert(buffer[0] == 0 && buffer[1] == 1 && buffer[2] == 2 && buffer[3] == UINT32_MAX);
	uint32_t magnitude = 77;
	assert(!ap_export_local_successor(&arena, 40, &magnitude) && magnitude == 41);
	assert(!ap_export_other_local_successor(&arena, magnitude, &magnitude) && magnitude == 42);
	ap_arena_OtherNat_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.depth && !arena.status);
	assert(!ap_from_OtherNumbers(&arena, values, 3, &right));
	assert(!ap_from_Numbers(&arena, values, 3, &left));
	ap_arena_Nat_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.depth && !arena.status);
	puts("Native C modules: distinct aliases, explicit array exchange and shared-arena rollback passed");
	return 0;
}
