#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void *__real_malloc(size_t);
static size_t attempts, fail_on;
void *__wrap_malloc(size_t size)
{
	++attempts;
	return fail_on && attempts == fail_on ? NULL : __real_malloc(size);
}

static int32_t signed_bits(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static void same(struct ap_data_Envelope left, struct ap_data_Envelope right)
{
	assert(left.tag == right.tag);
	if (left.tag == AP_DATA_Envelope_C0) return;
	assert(left.fields.c1.f1 == right.fields.c1.f1);
	struct ap_data_Packet a = left.fields.c1.f0, b = right.fields.c1.f0;
	assert(a.tag == b.tag);
	if (a.tag == AP_DATA_Packet_C1) {
		assert(a.fields.c1.f0 == b.fields.c1.f0 && a.fields.c1.f1.tag == b.fields.c1.f1.tag);
	} else if (a.tag == AP_DATA_Packet_C2) {
		assert(a.fields.c2.f0 == b.fields.c2.f0 && a.fields.c2.f1 == b.fields.c2.f1);
	}
}

static uint32_t measure(struct ap_data_Envelope e)
{
	if (e.tag == AP_DATA_Envelope_C0) return 0;
	struct ap_data_Packet p = e.fields.c1.f0;
	uint32_t bits = (uint32_t)e.fields.c1.f1;
	if (p.tag == AP_DATA_Packet_C1) bits += (uint32_t)p.fields.c1.f0 + p.fields.c1.f1.tag;
	else if (p.tag == AP_DATA_Packet_C2) bits += (uint32_t)p.fields.c2.f1;
	return bits;
}

static struct ap_data_Envelope value(size_t choice)
{
	struct ap_data_Envelope e;
	memset(&e, 0xff, sizeof(e));
	if (!choice) { e.tag = AP_DATA_Envelope_C0; return e; }
	e.tag = AP_DATA_Envelope_C1;
	struct ap_data_Packet *p = &e.fields.c1.f0;
	if (choice == 1) { p->tag = AP_DATA_Packet_C0; e.fields.c1.f1 = -1; }
	else if (choice == 2 || choice == 3) {
		p->tag = AP_DATA_Packet_C1;
		p->fields.c1.f0 = choice == 2 ? INT32_MAX : -7;
		p->fields.c1.f1.tag = choice == 2 ? 1 : 0;
		e.fields.c1.f1 = choice == 2 ? 0 : 3;
	} else {
		p->tag = AP_DATA_Packet_C2;
		p->fields.c2.f0 = choice == 4 ? INT64_MIN : INT64_MAX;
		p->fields.c2.f1 = choice == 4 ? 23 : INT32_MAX;
		e.fields.c1.f1 = choice == 4 ? -6 : 1;
	}
	return e;
}

static void ordinary(struct ap_data_Envelope *input, size_t count)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Records *list, *output;
	const struct ap_data_Reverse *reverse;
	assert(!ap_from_Records(&arena, input, count, &list));
	assert(!ap_from_Reverse(&arena, input, count, &reverse));
	struct ap_data_Envelope copied[8];
	size_t written = 99;
	assert(!ap_copy_Records(list, copied, 8, &written) && written == count);
	uint32_t expected = 0;
	for (size_t i = 0; i < count; ++i) { same(copied[i], input[i]); expected += measure(input[i]); }
	assert(!ap_copy_Reverse(reverse, copied, 8, &written) && written == count);
	for (size_t i = 0; i < count; ++i) same(copied[i], input[i]);
	int32_t result;
	assert(!ap_export_length(&arena, list, &result) && result == (int32_t)count);
	assert(!ap_export_sum(&arena, list, &result) && result == signed_bits(expected));
	assert(!ap_export_reverse_sum(&arena, reverse, &result) && result == signed_bits(expected));
	assert(!ap_export_identity(&arena, list, &output) && output == list);
	assert(!ap_export_append(&arena, list, list, &output));
	assert(!ap_copy_Records(output, copied, 8, &written) && written == count * 2);
	for (size_t i = 0; i < written; ++i) same(copied[i], input[i % count]);
	assert(!ap_export_sum(&arena, output, &result) && result == signed_bits(expected + expected));
	struct ap_data_Envelope head = value(4);
	assert(!ap_export_prepend(&arena, head, list, &output));
	assert(output->fields.c1.f1 == list);
	assert(!ap_copy_Records(output, copied, 8, &written) && written == count + 1);
	same(copied[0], head);
	for (size_t i = 0; i < count; ++i) same(copied[i + 1], input[i]);
	if (count) {
		struct ap_data_Envelope saved = input[0]; input[0].tag = 99;
		assert(!ap_copy_Records(list, copied, 8, &written)); same(copied[0], saved);
		input[0] = saved;
	}
	assert(!ap_export_empty(&arena, &output) && output->tag == AP_DATA_Records_C0);
	ap_arena_Records_destroy(&arena);
	assert(!arena.first && !arena.count && !arena.status && !arena.depth);
}

static void failures(void)
{
	struct ap_c_arena arena = {0};
	struct ap_data_Envelope input[3] = {value(2), value(4), value(3)}, copied[3];
	const struct ap_data_Records *list, *output;
	assert(!ap_from_Records(&arena, input, 3, &list));
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	arena.capacity = count + 2; output = list;
	assert(ap_from_Records(&arena, input, 3, &output) == 3 && output == list);
	assert(arena.first == mark && arena.count == count && arena.status == 3);
	arena.capacity = 0;
	for (size_t fail = 1; fail <= 4; ++fail) {
		attempts = 0; fail_on = fail;
		assert(ap_from_Records(&arena, input, 3, &output) == 3 && output == list);
		assert(attempts == fail && arena.first == mark && arena.count == count);
		assert(arena.status == 3 && !arena.depth);
	}
	for (size_t fail = 1; fail <= 3; ++fail) {
		attempts = 0; fail_on = fail;
		assert(ap_export_append(&arena, list, list, &output) == 3 && output == list);
		assert(attempts == fail && arena.first == mark && arena.count == count);
		assert(arena.status == 3 && !arena.depth);
	}
	fail_on = 0;
	arena.capacity = count + 2;
	assert(ap_export_append(&arena, list, list, &output) == 3 && output == list);
	assert(arena.first == mark && arena.count == count && arena.status == 3);
	for (size_t bad = 0; bad < 3; ++bad) {
		input[1] = value(2);
		if (!bad) input[1].tag = 99;
		else if (bad == 1) input[1].fields.c1.f0.tag = 99;
		else input[1].fields.c1.f0.fields.c1.f1.tag = 99;
		size_t saved_attempts = attempts;
		assert(ap_from_Records(&arena, input, 3, &output) == 2 && output == list);
		assert(arena.first == mark && arena.count == count && arena.status == 3);
		assert(attempts == saved_attempts);
		assert(ap_export_prepend(&arena, input[1], list, &output) == 2 && output == list);
		assert(attempts == saved_attempts);
		struct ap_data_Records node = {.tag = AP_DATA_Records_C1, .fields.c1 = {input[1], list}};
		int32_t result = 77;
		assert(ap_export_sum(&arena, &node, &result) == 2 && result == 77);
		memset(copied, 0x5a, sizeof(copied));
		unsigned char before[sizeof(copied)]; memcpy(before, copied, sizeof(copied));
		size_t written = 99;
		assert(ap_copy_Records(&node, copied, 3, &written) == 2 && written == 99);
		assert(!memcmp(before, copied, sizeof(copied)));
	}
	input[1] = value(2); arena.depth = 1;
	assert(ap_from_Records(&arena, input, 3, &output) == 4 && output == list);
	int32_t result = 77;
	assert(ap_export_sum(&arena, list, &result) == 4 && result == 77);
	assert(arena.first == mark && arena.count == count && arena.depth == 1);
	arena.depth = 0; arena.capacity = 0; arena.depth_limit = 1;
	assert(ap_export_append(&arena, list, list, &output) == 4 && output == list);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0;
	assert(!ap_export_identity(&arena, list, &output) && output == list && !arena.status);
	struct ap_data_Records node = {.tag = AP_DATA_Records_C1, .fields.c1 = {value(2), NULL}};
	assert(ap_export_sum(&arena, &node, &result) == 2 && result == 77);
	node.fields.c1.f1 = &node;
	assert(ap_export_identity(&arena, &node, &output) == 2 && output == list);
	node.tag = 99;
	assert(ap_copy_Records(&node, copied, 3, &(size_t){99}) == 2);
	assert(ap_from_Records(NULL, input, 3, &output) == 1);
	assert(ap_from_Records(&arena, NULL, 1, &output) == 1);
	assert(ap_from_Records(&arena, input, 3, NULL) == 1);
	assert(ap_from_Records(&arena, input, SIZE_MAX, &output) == 6 && output == list);
	assert(ap_copy_Records(list, copied, 3, NULL) == 1);
	assert(ap_copy_Records(list, NULL, 3, &(size_t){99}) == 1);
	memset(copied, 0x5a, sizeof(copied));
	unsigned char before[sizeof(copied)]; memcpy(before, copied, sizeof(copied));
	size_t written = 99;
	assert(ap_copy_Records(list, copied, 2, &written) == 6 && written == 99);
	assert(!memcmp(before, copied, sizeof(copied)));
	assert(!ap_copy_Records(list, copied, 3, &written) && written == 3);
	for (size_t i = 0; i < 3; ++i) same(copied[i], i == 1 ? value(4) : input[i]);
	ap_arena_Records_destroy(&arena);
}

static void long_copy(void)
{
	struct ap_c_arena arena = {0};
	struct ap_data_Envelope input[300], copied[300];
	for (size_t i = 0; i < 300; ++i) input[i] = value(i % 6);
	const struct ap_data_Records *list;
	assert(!ap_from_Records(&arena, input, 300, &list));
	size_t written = 99;
	assert(!ap_copy_Records(list, copied, 300, &written) && written == 300);
	for (size_t i = 0; i < 300; ++i) same(copied[i], input[i]);
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count;
	int32_t result = 77;
	assert(ap_export_sum(&arena, list, &result) == 4 && result == 77);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	assert(!ap_copy_Records(list, copied, 300, &written));
	ap_arena_Records_destroy(&arena);
}

static void report(const struct ap_data_Records *list)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Records *output;
	int32_t result;
	assert(!ap_export_length(&arena, list, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_sum(&arena, list, &result)); printf("%" PRId32 " ", result);
	assert(!ap_export_append(&arena, list, list, &output));
	assert(!ap_export_sum(&arena, output, &result)); printf("%" PRId32 " ", result);
	struct ap_data_Envelope head = {.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = AP_DATA_Packet_C1, .fields.c1 = {4, {1}}}, 3}};
	assert(!ap_export_prepend(&arena, head, list, &output));
	assert(!ap_export_sum(&arena, output, &result)); printf("%" PRId32 "|", result);
	ap_arena_Records_destroy(&arena);
}

int main(void)
{
	struct ap_data_Envelope input[3];
	size_t cases = 0;
	for (size_t count = 0, combinations = 1; count <= 3; ++count, combinations *= 6)
		for (size_t code = 0; code < combinations; ++code) {
			size_t n = code;
			for (size_t i = 0; i < count; ++i, n /= 6) input[i] = value(n % 6);
			ordinary(input, count); ++cases;
		}
	assert(cases == 259); failures(); long_copy();
	struct ap_c_arena arena = {0};
	int32_t result;
	assert(!ap_export_sample_length(&arena, &result) && result == 3);
	assert(!ap_export_sample_sum(&arena, &result) && result == 16);
	assert(!ap_export_sample_append(&arena, &result) && result == 32);
	const struct ap_data_Records *list;
	assert(!ap_from_Records(&arena, NULL, 0, &list)); report(list);
	input[0] = (struct ap_data_Envelope){.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = AP_DATA_Packet_C1, .fields.c1 = {7, {1}}}, 3}};
	input[1] = value(0);
	input[2] = (struct ap_data_Envelope){.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = AP_DATA_Packet_C0}, 5}};
	assert(!ap_from_Records(&arena, input, 3, &list)); report(list);
	input[0] = value(2);
	assert(!ap_from_Records(&arena, input, 1, &list)); report(list);
	const struct ap_data_Reverse *reverse;
	assert(!ap_export_reverse_empty(&arena, &reverse));
	struct ap_data_Envelope head = {.tag = AP_DATA_Envelope_C1,
		.fields.c1 = {{.tag = AP_DATA_Packet_C0}, 9}};
	assert(!ap_export_reverse_prepend(&arena, head, reverse, &reverse));
	assert(!ap_export_reverse_sum(&arena, reverse, &result)); printf("%" PRId32, result);
	ap_arena_Records_destroy(&arena);
	return 0;
}
