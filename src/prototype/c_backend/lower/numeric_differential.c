#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void report(const struct ap_data_Numbers *input, uint32_t pivot)
{
	struct ap_c_arena arena = {0};
	for (unsigned upper = 0; upper < 2; ++upper) {
		const struct ap_data_Numbers *out;
		int status = upper ? ap_export_upper(&arena, input, pivot, &out) : ap_export_lower(&arena, input, pivot, &out);
		assert(!status);
		uint32_t buffer[5];
		size_t written;
		assert(!ap_copy_Numbers(out, buffer, 5, &written));
		for (size_t i = 0; i < written; ++i) printf("%" PRIu32 ",", buffer[i]);
		putchar('|');
	}
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	struct ap_data_Numbers nodes[6] = {{.tag = AP_DATA_Numbers_C0}};
	const uint32_t values[] = {2, 1, 3, 0, 2};
	for (size_t i = 1; i <= 5; ++i)
		nodes[i] = (struct ap_data_Numbers){.tag = AP_DATA_Numbers_C1, .fields.c1 = {values[i - 1], &nodes[i - 1]}};
	report(&nodes[0], 0);
	for (uint32_t pivot = 0; pivot < 4; ++pivot) report(&nodes[5], pivot);
	struct ap_c_arena arena = {0};
	const uint32_t left[] = {3, 2, 1};
	for (size_t i = 0; i < 3; ++i) {
		struct ap_enum_Bool answer;
		assert(!ap_export_compare(&arena, left[i], 2, &answer));
		printf("%" PRIu32, answer.tag);
	}
	ap_arena_Nat_destroy(&arena);
	return 0;
}
