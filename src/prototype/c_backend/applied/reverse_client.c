#include "component.h"
#include <assert.h>

int main(void)
{
	struct ap_c_arena arena = {0};
	size_t cases = 0;
	for (size_t count = 0; count <= 6; ++count) for (unsigned bits = 0; bits < (1u << count); ++bits) {
		uint32_t values[6], output[7];
		for (size_t i = 0; i < count; ++i) values[i] = (bits >> i) & 1;
		const struct ap_data_ReverseNumbers *input, *changed;
		assert(!ap_from_ReverseNumbers(&arena, count ? values : NULL, count, &input));
		assert(!ap_export_identity_reverse(&arena, input, &changed) && changed == input);
		int32_t length;
		assert(!ap_export_length_reverse(&arena, input, &length) && length == (int32_t)count);
		assert(!ap_export_prepend_reverse(&arena, UINT32_MAX, input, &changed));
		size_t written = 77;
		assert(!ap_copy_ReverseNumbers(changed, output, 7, &written) && written == count + 1 && output[0] == UINT32_MAX);
		for (size_t i = 0; i < count; ++i) assert(output[i + 1] == values[i]);
		ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 127);
	return 0;
}
