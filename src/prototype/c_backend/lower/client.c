#include "component.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>

int main(void)
{
	const int32_t small[] = {INT32_MIN, INT32_MIN + 1, -65537, -1, 0, 1, 65537, INT32_MAX};
	const int64_t large[] = {INT64_MIN, INT64_MIN + 1, -INT64_C(4294967297), -1, 0, 1, INT64_C(4294967297), INT64_MAX};
	int32_t a;
	int64_t b;
	for (size_t i = 0; i < sizeof(small) / sizeof(*small); ++i) {
		int32_t x = small[i];
		uint32_t u = (uint32_t)x;
		assert(!ap_export_identity(x, &a) && a == x);
		assert(!ap_export_ignore(x, &a) && a == 42);
		assert(!ap_export_neg(x, &a) && (uint32_t)a == (uint32_t)(UINT64_C(0) - u));
		assert(!ap_export_captured(x, &a) && (uint32_t)a == (uint32_t)((uint64_t)u + 7));
		uint64_t n = (uint32_t)((uint64_t)u + 1);
		assert(!ap_export_sequence(x, &a) && (uint32_t)a == (uint32_t)(n * n));
		for (size_t j = 0; j < sizeof(small) / sizeof(*small); ++j) {
			int32_t y = small[j];
			uint32_t v = (uint32_t)y;
			assert(!ap_export_add(x, y, &a) && (uint32_t)a == (uint32_t)((uint64_t)u + v));
			assert(!ap_export_alias(x, y, &a) && (uint32_t)a == (uint32_t)((uint64_t)u + v));
			assert(!ap_export_sub(x, y, &a) && (uint32_t)a == (uint32_t)((uint64_t)u - v));
			assert(!ap_export_mul(x, y, &a) && (uint32_t)a == (uint32_t)((uint64_t)u * v));
			uint64_t sum = (uint32_t)((uint64_t)u + v), difference = (uint32_t)((uint64_t)u - v);
			assert(!ap_export_composed(x, y, &a) && (uint32_t)a == (uint32_t)(sum * difference));
		}
	}
	for (size_t i = 0; i < sizeof(large) / sizeof(*large); ++i) {
		int64_t x = large[i];
		uint64_t u = (uint64_t)x;
		assert(!ap_export_neg64(x, &b) && (uint64_t)b == UINT64_C(0) - u);
		for (size_t j = 0; j < sizeof(large) / sizeof(*large); ++j) {
			int64_t y = large[j];
			uint64_t v = (uint64_t)y;
			assert(!ap_export_add64(x, y, &b) && (uint64_t)b == u + v);
			assert(!ap_export_sub64(x, y, &b) && (uint64_t)b == u - v);
			assert(!ap_export_mul64(x, y, &b) && (uint64_t)b == u * v);
		}
	}
	assert(!ap_export_constant(&a) && a == INT32_MIN);
	assert(!ap_export_computed(&a) && a == 42);
	assert(ap_export_add(1, 2, NULL) == 1);
	assert(ap_export_constant(NULL) == 1);
#ifdef LINK_OTHER_COMPONENT
	assert(!ap_export_other(10, 20, &a) && a == -10);
#endif
}
