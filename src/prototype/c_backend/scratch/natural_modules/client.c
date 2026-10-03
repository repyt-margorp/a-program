#ifdef CHOICE_FIRST
#include "choice.h"
#include "numbers.h"
#else
#include "numbers.h"
#include "choice.h"
#endif
#include <assert.h>
#include <string.h>

int main(void)
{
	struct ap_c_arena arena = {0};
	uint32_t values[] = {0, 1, 2, UINT32_MAX}, copied[4];
	const struct ap_data_Numbers *numbers, *output;
	assert(!ap_from_Numbers(&arena, values, 4, &numbers));
	size_t written = 77;
	assert(!ap_copy_Numbers(numbers, copied, 4, &written) && written == 4);
	assert(!memcmp(values, copied, sizeof(values)));
	for (size_t i = 0; i < 4; ++i) for (uint32_t tag = 0; tag < 2; ++tag) {
		struct ap_data_Choice choice;
		assert(!ap_export_choose(&arena, copied[i], (struct ap_enum_Bool){tag}, &choice));
		uint32_t number = 77;
		struct ap_enum_Bool flag = {77};
		assert(!ap_export_choice_number(&arena, choice, &number) && number == copied[i]);
		assert(!ap_export_choice_flag(&arena, choice, &flag) && flag.tag == tag);
	}
	struct ap_c_allocation *mark = arena.first;
	size_t saved = arena.count;
	arena.capacity = saved; output = numbers;
	assert(ap_export_prepend(&arena, 3, numbers, &output) == 3 && output == numbers);
	assert(arena.first == mark && arena.count == saved);
	struct ap_data_Choice choice = {.tag = AP_DATA_Choice_C1,
		.fields.c1 = {.f0 = 77, .f1 = {1}}};
	assert(ap_export_choose(&arena, UINT32_MAX, (struct ap_enum_Bool){2}, &choice) == 2);
	assert(choice.tag == AP_DATA_Choice_C1 && choice.fields.c1.f0 == 77 &&
		choice.fields.c1.f1.tag == 1);
	assert(arena.first == mark && arena.count == saved);
	arena.depth = 1;
	assert(ap_export_choose(&arena, UINT32_MAX, (struct ap_enum_Bool){1}, &choice) == 4);
	assert(choice.tag == AP_DATA_Choice_C1 && choice.fields.c1.f0 == 77 &&
		choice.fields.c1.f1.tag == 1);
	assert(ap_from_Numbers(&arena, values, 4, &output) == 4 && output == numbers);
	assert(arena.first == mark && arena.count == saved && arena.depth == 1);
	arena.depth = 0;
	assert(!ap_export_choose(&arena, UINT32_MAX, (struct ap_enum_Bool){1}, &choice));
	assert(!arena.status && arena.first == mark && arena.count == saved);
	uint32_t number;
	assert(!ap_export_choice_number(&arena, choice, &number) && number == UINT32_MAX);
	assert(!ap_copy_Numbers(numbers, copied, 4, &written) && written == 4);
	assert(!memcmp(values, copied, sizeof(values)));
	ap_arena_ChoiceNat_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.status && !arena.depth);
	arena.capacity = 0;
	assert(!ap_from_Numbers(&arena, NULL, 0, &numbers));
	ap_arena_NumbersNat_destroy(&arena);
	assert(!arena.first && !arena.count);
	return 0;
}
