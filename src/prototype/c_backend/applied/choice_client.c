#include "component.h"
#include <assert.h>

int main(void)
{
	struct ap_c_arena arena = {0};
	for (uint32_t n = 0; n < 17; ++n) for (uint32_t tag = 0; tag < 2; ++tag) {
		struct ap_data_Choice choice;
		struct ap_enum_Bool flag;
		uint32_t number;
		assert(!ap_export_choose(&arena, n, (struct ap_enum_Bool){tag}, &choice));
		assert(!ap_export_choice_number(&arena, choice, &number) && number == n);
		assert(!ap_export_choice_flag(&arena, choice, &flag) && flag.tag == tag);
	}
	struct ap_data_Choice empty;
	struct ap_enum_Bool flag = {77};
	uint32_t number = 77;
	assert(!ap_export_empty_choice(&arena, &empty));
	assert(!ap_export_choice_number(&arena, empty, &number) && !number);
	assert(!ap_export_choice_flag(&arena, empty, &flag) && !flag.tag);
	ap_arena_Nat_destroy(&arena);
	return 0;
}
