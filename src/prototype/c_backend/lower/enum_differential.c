#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
	for (uint32_t i = 0; i < 2; ++i) {
		struct ap_enum_Bool b = {i};
		struct ap_enum_Colour c;
		int32_t n[7];
		assert(!ap_export_to_int(b, n));
		assert(!ap_export_choose(b, 7, 11, n + 1));
		assert(!ap_export_nested(b, n + 2));
		assert(!ap_export_curried(b, 5, n + 3));
		assert(!ap_export_through_fold(b, n + 4));
		assert(!ap_export_fold_arg(b, n + 5));
		assert(!ap_export_to_colour(b, &c) && !ap_export_colour(c, n + 6));
		for (size_t j = 0; j < 7; ++j) printf("%" PRId32 "%s", n[j], j < 6 ? " " : "|");
	}
	return 0;
}
