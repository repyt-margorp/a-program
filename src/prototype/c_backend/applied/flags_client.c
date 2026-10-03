#include "component.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	struct ap_c_arena arena = {0};
	size_t cases = 0;
	for (size_t count = 0; count <= 6; ++count) for (unsigned bits = 0; bits < (1u << count); ++bits) {
		struct ap_enum_Bool values[6], output[7];
		for (size_t i = 0; i < count; ++i) values[i].tag = (bits >> i) & 1;
		const struct ap_data_Flags *input, *changed;
		assert(!ap_from_Flags(&arena, count ? values : NULL, count, &input));
		assert(!ap_export_identity_flags(&arena, input, &changed) && changed == input);
		int32_t length;
		assert(!ap_export_length_flags(&arena, input, &length) && length == (int32_t)count);
		assert(!ap_export_prepend_flag(&arena, (struct ap_enum_Bool){1}, input, &changed));
		size_t written = 77;
		assert(!ap_copy_Flags(changed, output, 7, &written) && written == count + 1 && output[0].tag == 1);
		for (size_t i = 0; i < count; ++i) assert(output[i + 1].tag == values[i].tag);
		ap_arena_Flags_destroy(&arena); ++cases;
	}
	assert(cases == 127);
	const struct ap_data_Flags *empty, *output;
	assert(!ap_export_empty_flags(&arena, &empty)); output = empty;
	struct ap_c_allocation *mark = arena.first;
	size_t saved = arena.count;
	assert(ap_export_prepend_flag(&arena, (struct ap_enum_Bool){2}, empty, &output) == 2 && output == empty);
	assert(arena.first == mark && arena.count == saved);
	struct ap_enum_Bool invalid = {UINT32_MAX};
	assert(ap_from_Flags(&arena, &invalid, 1, &output) == 2 && output == empty);
	assert(arena.first == mark && arena.count == saved);
	ap_arena_Flags_destroy(&arena);
	return 0;
}
