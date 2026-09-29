#include "component.h"
#include "component.h"
#include <assert.h>
#include <stddef.h>

int main(void)
{
	for (uint32_t tag = 0; tag < 2; ++tag) {
		struct ap_enum_Bool b = {tag}, result = {9};
		struct ap_enum_Twin t = {tag}, twin = {9};
		struct ap_enum_Colour c = {9};
		int32_t n = -1;
		assert(!ap_export_negate(b, &result) && result.tag == 1 - tag);
		assert(!ap_export_same(b, &result) && result.tag == tag);
		assert(!ap_export_to_int(b, &n) && n == (tag ? 20 : 10));
		assert(!ap_export_choose(b, 7, 11, &n) && n == (tag ? 22 : 8));
		assert(!ap_export_nested(b, &n) && n == (tag ? 8 : 22));
		assert(!ap_export_curried(b, 5, &n) && n == (tag ? 10 : 6));
		assert(!ap_export_through_fold(b, &n) && n == (tag ? 6 : 4));
		assert(!ap_export_fold_arg(b, &n) && n == (tag ? 6 : 11));
		assert(!ap_export_to_colour(b, &c) && c.tag == (tag ? 2 : 0));
		assert(!ap_export_twin(t, &twin) && twin.tag == 1 - tag);
	}
	for (uint32_t tag = 0; tag < 3; ++tag) {
		int32_t n;
		assert(!ap_export_colour((struct ap_enum_Colour){tag}, &n) && n == (int32_t)tag + 1);
	}
	struct ap_enum_Bool out = {9};
	assert(!ap_export_constant(&out) && out.tag == AP_ENUM_Bool_C1);
	assert(ap_export_negate((struct ap_enum_Bool){0}, NULL) == 1);
	for (uint32_t tag = 2; tag < 5; ++tag) {
		out.tag = 9;
		assert(ap_export_negate((struct ap_enum_Bool){tag}, &out) == 2 && out.tag == 9);
	}
	int32_t n = 37;
	assert(ap_export_colour((struct ap_enum_Colour){UINT32_MAX}, &n) == 2 && n == 37);
	return 0;
}
