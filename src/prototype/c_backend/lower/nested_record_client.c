#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int32_t signed_bits(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static void same_packet(struct ap_data_Packet left, struct ap_data_Packet right)
{
	assert(left.tag == right.tag);
	if (left.tag == AP_DATA_Packet_C1) {
		assert(left.fields.c1.f0 == right.fields.c1.f0);
		assert(left.fields.c1.f1.tag == right.fields.c1.f1.tag);
	} else if (left.tag == AP_DATA_Packet_C2) {
		assert(left.fields.c2.f0 == right.fields.c2.f0);
		assert(left.fields.c2.f1 == right.fields.c2.f1);
	}
}

static void ordinary(struct ap_data_Packet packet, uint32_t number, int32_t offset)
{
	struct ap_data_Envelope input, output;
	struct ap_data_Packet result;
	struct ap_data_Outer outer;
	int32_t measured;
	uint32_t expected = number + (uint32_t)offset;
	assert(!ap_export_wrap(packet, offset, &input));
	assert(input.tag == AP_DATA_Envelope_C1 && input.fields.c1.f1 == offset);
	same_packet(input.fields.c1.f0, packet);
	assert(!ap_export_measure(input, &measured) && measured == signed_bits(expected));
	assert(!ap_export_captured(input, &measured) && measured == signed_bits(expected + 7));
	assert(!ap_export_unwrap(input, &result)); same_packet(result, packet);
	assert(!ap_export_identity(input, &output));
	assert(output.tag == input.tag && output.fields.c1.f1 == offset);
	same_packet(output.fields.c1.f0, packet);
	assert(!ap_export_rebuild(input, &output) && output.tag == AP_DATA_Envelope_C1);
	assert(output.fields.c1.f1 == signed_bits(expected));
	same_packet(output.fields.c1.f0, packet);
	assert(!ap_export_measure(output, &measured) && measured == signed_bits(number + expected));
	assert(!ap_export_outer(input, &outer) && outer.tag == AP_DATA_Outer_C0);
	same_packet(outer.fields.c0.f0.fields.c1.f0, packet);
	assert(!ap_export_outer_number(outer, &measured) && measured == signed_bits(expected));
	assert(!ap_export_identity(input, &input) && input.fields.c1.f1 == offset);
	same_packet(input.fields.c1.f0, packet);
}

static void failures(void)
{
	struct ap_data_Envelope input, output;
	memset(&output, 0x5a, sizeof(output));
	unsigned char before[sizeof(output)];
	memcpy(before, &output, sizeof(output));
	int32_t measured = 77;
	input = (struct ap_data_Envelope){.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = 99}, 3}};
	assert(ap_export_measure(input, &measured) == 2 && measured == 77);
	assert(ap_export_identity(input, &output) == 2 && !memcmp(before, &output, sizeof(output)));
	input.fields.c1.f0 = (struct ap_data_Packet){.tag = AP_DATA_Packet_C1, .fields.c1 = {4, {99}}};
	assert(ap_export_captured(input, &measured) == 2 && measured == 77);
	struct ap_data_Outer outer = {.tag = AP_DATA_Outer_C0, .fields.c0 = {input}};
	assert(ap_export_outer_number(outer, &measured) == 2 && measured == 77);
	assert(ap_export_wrap(input.fields.c1.f0, 3, &output) == 2 && !memcmp(before, &output, sizeof(output)));
	assert(ap_export_identity(input, NULL) == 1);
	input.tag = 99;
	assert(ap_export_measure(input, &measured) == 2 && measured == 77);
	outer.tag = 99;
	assert(ap_export_outer_number(outer, &measured) == 2 && measured == 77);
	/* An inactive nested union does not require initialized child tags. */
	memset(&input, 0xff, sizeof(input)); input.tag = AP_DATA_Envelope_C0;
	assert(!ap_export_measure(input, &measured) && measured == 0);
	assert(!ap_export_unwrap(input, &output.fields.c1.f0) && output.fields.c1.f0.tag == AP_DATA_Packet_C0);
}

static void report(struct ap_data_Envelope input)
{
	struct ap_data_Envelope rebuilt;
	struct ap_data_Outer outer;
	int32_t value;
	assert(!ap_export_measure(input, &value)); printf("%" PRId32 " ", value);
	assert(!ap_export_rebuild(input, &rebuilt));
	assert(!ap_export_measure(rebuilt, &value)); printf("%" PRId32 " ", value);
	assert(!ap_export_captured(input, &value)); printf("%" PRId32 " ", value);
	assert(!ap_export_outer(input, &outer));
	assert(!ap_export_outer_number(outer, &value)); printf("%" PRId32 "|", value);
}

int main(void)
{
	const int32_t values[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
	const int32_t offsets[] = {INT32_MIN, -9, 0, 7, INT32_MAX};
	const int64_t wide[] = {INT64_MIN, INT64_MAX};
	size_t cases = 0;
	for (size_t j = 0; j < 5; ++j) {
		ordinary((struct ap_data_Packet){.tag = AP_DATA_Packet_C0}, 0, offsets[j]); ++cases;
		for (size_t i = 0; i < 5; ++i) {
			for (uint32_t tag = 0; tag < 2; ++tag) {
				ordinary((struct ap_data_Packet){.tag = AP_DATA_Packet_C1, .fields.c1 = {values[i], {tag}}},
					(uint32_t)values[i] + tag, offsets[j]); ++cases;
			}
			for (size_t k = 0; k < 2; ++k) {
				ordinary((struct ap_data_Packet){.tag = AP_DATA_Packet_C2, .fields.c2 = {wide[k], values[i]}},
					(uint32_t)values[i], offsets[j]); ++cases;
			}
		}
	}
	assert(cases == 105); failures();
	struct ap_data_Envelope input;
	assert(!ap_export_constant(&input)); report(input);
	assert(!ap_export_wrap((struct ap_data_Packet){.tag = AP_DATA_Packet_C0}, 3, &input)); report(input);
	assert(!ap_export_wrap((struct ap_data_Packet){.tag = AP_DATA_Packet_C1, .fields.c1 = {4, {1}}}, 3, &input)); report(input);
	assert(!ap_export_wrap((struct ap_data_Packet){.tag = AP_DATA_Packet_C1, .fields.c1 = {INT32_MAX, {1}}}, 0, &input)); report(input);
	return 0;
}
