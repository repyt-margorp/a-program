#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
	struct ap_data_List samples[5] = {{.tag = AP_DATA_List_C0}};
	const int32_t numbers[] = {INT32_MIN, INT32_MAX, 1, -1};
	const uint32_t flags[] = {0, 1, 1, 0};
	for (size_t i = 1; i < 5; ++i)
		samples[i] = (struct ap_data_List){.tag = AP_DATA_List_C1, .fields.c1 = {numbers[i - 1], {flags[i - 1]}, &samples[i - 1]}};
	for (size_t i = 0; i < 5; ++i) {
		struct ap_c_arena arena = {0};
		int32_t length, sum, composed;
		assert(!ap_export_length(&arena, &samples[i], &length));
		assert(!ap_export_sum(&arena, &samples[i], &sum));
		assert(!ap_export_composed(&arena, &samples[i], &composed));
		printf("%" PRId32 " %" PRId32 " %" PRId32, length, sum, composed);
		for (uint32_t flag = 0; flag < 2; ++flag) {
			const struct ap_data_List *selected;
			assert(!ap_export_select(&arena, &samples[i], (struct ap_enum_Bool){flag}, &selected));
			assert(!ap_export_length(&arena, selected, &length));
			assert(!ap_export_sum(&arena, selected, &sum));
			printf(" %" PRId32 " %" PRId32, length, sum);
		}
		putchar('|'); ap_arena_List_destroy(&arena);
	}
	return 0;
}
