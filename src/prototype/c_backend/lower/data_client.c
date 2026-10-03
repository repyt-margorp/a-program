#include "component.h"
#include "component.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static void check(struct ap_data_Packet p, uint32_t number, int64_t large)
{
	int32_t result;
	int64_t result64;
	struct ap_data_Packet out;
	assert(!ap_export_number(p, &result) && (uint32_t)result == number);
	assert(!ap_export_large(p, INT64_MIN, &result64) && result64 == large);
	assert(!ap_export_identity(p, &out) && out.tag == p.tag);
	assert(!ap_export_partial(p, 7, &result));
	uint32_t field = p.tag == 1 ? (uint32_t)p.fields.c1.f0 : p.tag == 2 ? (uint32_t)p.fields.c2.f1 : 2;
	assert((uint32_t)result == field + 7);
	for (uint32_t b = 0; b < 2; ++b) {
		assert(!ap_export_captured(p, (struct ap_enum_Bool){b}, &result));
		assert((uint32_t)result == number + (b ? 9 : 3));
		assert(!ap_export_block_callback(p, (struct ap_enum_Bool){b}, &result));
		assert((uint32_t)result == number + (b ? 9 : 3));
	}
	assert(!ap_export_rebuild(p, &out) && out.tag == p.tag);
	if (p.tag == 1) {
		assert((uint32_t)out.fields.c1.f0 == (uint32_t)p.fields.c1.f0 + 1);
		assert(out.fields.c1.f1.tag == p.fields.c1.f1.tag);
	} else if (p.tag == 2) {
		assert((uint64_t)out.fields.c2.f0 == UINT64_C(0) - (uint64_t)p.fields.c2.f0);
		assert((uint32_t)out.fields.c2.f1 == UINT32_C(0) - (uint32_t)p.fields.c2.f1);
	}
	/* By-value input permits the same object as the output destination. */
	assert(!ap_export_identity(p, &p));
	assert(!ap_export_number(p, &result) && (uint32_t)result == number);
}

int main(void)
{
	const int32_t small[] = {INT32_MIN, INT32_MIN + 1, -65537, -1, 0, 1, 65537, INT32_MAX};
	const int64_t wide[] = {INT64_MIN, INT64_MIN + 1, -1, 0, 1, INT64_MAX};
	struct ap_data_Packet p;
	assert(!ap_export_constant(&p) && p.tag == 0);
	check(p, 0, INT64_MIN);
	for (size_t i = 0; i < sizeof(small) / sizeof(*small); ++i) {
		for (uint32_t b = 0; b < 2; ++b) {
			assert(!ap_export_small(small[i], (struct ap_enum_Bool){b}, &p));
			assert(p.tag == 1 && p.fields.c1.f0 == small[i] && p.fields.c1.f1.tag == b);
			check(p, (uint32_t)small[i] + b, INT64_MIN);
		}
		for (size_t j = 0; j < sizeof(wide) / sizeof(*wide); ++j) {
			assert(!ap_export_wide(wide[j], small[i], &p));
			assert(p.tag == 2 && p.fields.c2.f0 == wide[j] && p.fields.c2.f1 == small[i]);
			check(p, (uint32_t)small[i], wide[j]);
		}
	}
	int32_t out = 123;
	for (uint32_t tag = 3; tag < 5; ++tag) {
		p = (struct ap_data_Packet){.tag = tag};
		assert(ap_export_number(p, &out) == 2 && out == 123);
		assert(ap_export_identity(p, NULL) == 1);
	}
	memset(&p, 0x5a, sizeof(p));
	unsigned char before[sizeof(p)];
	memcpy(before, &p, sizeof(p));
	assert(ap_export_small(4, (struct ap_enum_Bool){2}, &p) == 2);
	assert(!memcmp(before, &p, sizeof(p)));
	p = (struct ap_data_Packet){.tag = 1, .fields.c1 = {1, {2}}};
	memcpy(before, &p, sizeof(p));
	assert(ap_export_number(p, &out) == 2 && out == 123);
	assert(ap_export_rebuild(p, &p) == 2 && !memcmp(before, &p, sizeof(p)));
	p.fields.c1.f1.tag = 0;
	assert(ap_export_captured(p, (struct ap_enum_Bool){UINT32_MAX}, &out) == 2 && out == 123);
	assert(ap_export_block_callback(p, (struct ap_enum_Bool){UINT32_MAX}, &out) == 2 && out == 123);
	assert(ap_export_block_callback(p, (struct ap_enum_Bool){0}, NULL) == 1);
	assert(ap_export_small(1, (struct ap_enum_Bool){0}, NULL) == 1);
	/* Only the active constructor's fields are validated/read. */
	memset(&p, 0xff, sizeof(p)); p.tag = 0;
	assert(!ap_export_number(p, &out) && out == 0);
	struct ap_data_Twin twin = {.tag = 1, .fields.c1 = {42, {1}}};
	assert(!ap_export_other(twin, &out) && out == 42);
}
