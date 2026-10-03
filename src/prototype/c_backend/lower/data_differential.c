#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void report(struct ap_data_Packet p)
{
	int32_t n;
	struct ap_data_Packet rebuilt;
	assert(!ap_export_number(p, &n)); printf("%" PRId32 " ", n);
	assert(!ap_export_rebuild(p, &rebuilt));
	assert(!ap_export_number(rebuilt, &n)); printf("%" PRId32 " ", n);
	assert(!ap_export_captured(p, (struct ap_enum_Bool){0}, &n)); printf("%" PRId32 " ", n);
	assert(!ap_export_captured(p, (struct ap_enum_Bool){1}, &n)); printf("%" PRId32 " ", n);
	assert(!ap_export_partial(p, 7, &n)); printf("%" PRId32 " ", n);
	assert(!ap_export_shadowed(p, &n)); printf("%" PRId32 "|", n);
}

int main(void)
{
	struct ap_data_Packet p;
	assert(!ap_export_constant(&p)); report(p);
	const int32_t values[] = {INT32_MIN, -1, 0, INT32_MAX};
	for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
		for (uint32_t b = 0; b < 2; ++b) {
			assert(!ap_export_small(values[i], (struct ap_enum_Bool){b}, &p));
			report(p);
		}
}
