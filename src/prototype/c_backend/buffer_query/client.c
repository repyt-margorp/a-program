#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts;
static int reject_allocation;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	++attempts; return reject_allocation ? NULL : __real_malloc(size);
}

static int32_t signed32(uint32_t n)
{
	return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}

static void lists(void)
{
	const int32_t choices[] = {INT32_MIN,0,INT32_MAX}; size_t cases = 0;
	struct ap_c_arena arena = {0};
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 3)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t values[4]; size_t digits = code, count = 77, written; uint32_t total = 0;
			for (size_t i = 0; i < length; ++i,digits /= 3) { values[i] = choices[digits%3]; total += (uint32_t)values[i]; }
			const struct ap_data_Numbers32 *input, *output; assert(!ap_from_Numbers32(&arena,values,length,&input));
			assert(!ap_measure_Numbers32(input,&count) && count == length);
			int32_t *copy = count ? malloc(count * sizeof(*copy)) : NULL; assert(!count || copy);
			assert(!ap_copy_Numbers32(input,copy,count,&written) && written == count);
			for (size_t i = 0; i < count; ++i) assert(copy[i] == values[i]);
			free(copy);
			int32_t result; assert(!ap_export_sum32(&arena,input,&result) && result == signed32(total));
			for (uint32_t offset = 0; offset < 6; ++offset) for (uint32_t limit = 0; limit < 6; ++limit) {
				assert(!ap_export_slice32(&arena,input,offset,limit,&output));
				size_t first = offset < length ? offset : length, expected = length - first;
				if (expected > limit) expected = limit;
				assert(!ap_measure_Numbers32(output,&count) && count == expected);
				copy = count ? malloc(count * sizeof(*copy)) : NULL; assert(!count || copy);
				assert(!ap_copy_Numbers32(output,copy,count,&written) && written == count);
				for (size_t i = 0; i < count; ++i) assert(copy[i] == values[first+i]);
				free(copy);
			}
			assert(!ap_export_append32(&arena,input,input,&output));
			assert(!ap_measure_Numbers32(output,&count) && count == length * 2);
			copy = count ? malloc(count * sizeof(*copy)) : NULL; assert(!count || copy);
			assert(!ap_copy_Numbers32(output,copy,count,&written) && written == count);
			for (size_t i = 0; i < count; ++i) assert(copy[i] == values[i%length]);
			ap_arena_Nat_destroy(&arena);
			for (size_t i = 0; i < count; ++i) assert(copy[i] == values[i%length]);
			free(copy); ++cases;
		}
	assert(cases == 121);
}

static void payloads(void)
{
	struct ap_c_arena arena = {0};
	int64_t values[] = {INT64_MIN,0,INT64_MAX}; struct ap_enum_Bool flags[] = {{0},{1},{0}};
	struct ap_data_Packet records[] = {{.tag = 0},{.tag = 1,.fields.c1 = {INT32_MIN,{1}}},{.tag = 2,.fields.c2 = {INT64_MAX}}};
	uint32_t naturals[] = {0,1,UINT32_MAX};
	const struct ap_data_Numbers64 *longs; const struct ap_data_Flags *booleans;
	const struct ap_data_Records *packets; const struct ap_data_Nats *nats;
	assert(!ap_from_Numbers64(&arena,values,3,&longs)); assert(!ap_from_Flags(&arena,flags,3,&booleans));
	assert(!ap_from_Records(&arena,records,3,&packets)); assert(!ap_from_Nats(&arena,naturals,3,&nats));
	struct ap_c_allocation *mark = arena.first; size_t saved = arena.count, count = 77, written;
	arena.depth = 1; arena.status = 77; attempts = 0; reject_allocation = 1;
	assert(!ap_measure_Numbers64(longs,&count) && count == 3);
	assert(!ap_measure_Flags(booleans,&count) && count == 3);
	assert(!ap_measure_Records(packets,&count) && count == 3);
	assert(!ap_measure_Nats(nats,&count) && count == 3);
	assert(!attempts && arena.first == mark && arena.count == saved && arena.depth == 1 && arena.status == 77);
	reject_allocation = 0; arena.depth = 0; arena.status = 0;
	int64_t a[3]; struct ap_enum_Bool b[3]; struct ap_data_Packet c[3]; uint32_t d[3];
	assert(!ap_copy_Numbers64(longs,a,3,&written) && written == 3); assert(!ap_copy_Flags(booleans,b,3,&written) && written == 3);
	assert(!ap_copy_Records(packets,c,3,&written) && written == 3); assert(!ap_copy_Nats(nats,d,3,&written) && written == 3);
	ap_arena_Nat_destroy(&arena);
	for (size_t i = 0; i < 3; ++i) { assert(a[i] == values[i] && b[i].tag == flags[i].tag && d[i] == naturals[i]); }
	assert(c[0].tag == 0 && c[1].tag == 1 && c[1].fields.c1.f0 == INT32_MIN && c[1].fields.c1.f1.tag == 1 && c[2].fields.c2.f0 == INT64_MAX);
}

static void failures(void)
{
	size_t count = 77; struct ap_data_Numbers32 terminal = {.tag = 1};
	assert(ap_measure_Numbers32(&terminal,NULL) == 1);
	assert(ap_measure_Numbers32(NULL,&count) == 2 && count == 77);
	struct ap_data_Numbers32 invalid = {.tag = 77};
	assert(ap_measure_Numbers32(&invalid,&count) == 2 && count == 77);
	invalid = (struct ap_data_Numbers32){.tag = 0,.fields.c0 = {1,NULL}};
	assert(ap_measure_Numbers32(&invalid,&count) == 2 && count == 77);
	invalid.fields.c0.f1 = &invalid;
	assert(ap_measure_Numbers32(&invalid,&count) == 2 && count == 77);
	struct ap_data_Records end = {.tag = 0}, bad = {.tag = 1,.fields.c1 = {{.tag = 77},&end}};
	assert(ap_measure_Records(&bad,&count) == 2 && count == 77);
	bad.fields.c1.f0 = (struct ap_data_Packet){.tag = 1,.fields.c1 = {0,{77}}};
	assert(ap_measure_Records(&bad,&count) == 2 && count == 77);
	/* Inactive value fields are irrelevant; query validates only active fields. */
	bad.fields.c1.f0.tag = 0; assert(!ap_measure_Records(&bad,&count) && count == 1);
	struct ap_data_Flags flag_end = {.tag = 0}, flag_bad = {.tag = 1,.fields.c1 = {&flag_end,{77}}}; count = 77;
	assert(ap_measure_Flags(&flag_bad,&count) == 2 && count == 77);
	assert(!ap_measure_Numbers32(&terminal,&count) && count == 0);
}

static void long_chain(void)
{
	int32_t values[300], result = 77; for (size_t i = 0; i < 300; ++i) values[i] = (int32_t)i;
	struct ap_c_arena arena = {0}; const struct ap_data_Numbers32 *list; assert(!ap_from_Numbers32(&arena,values,300,&list));
	struct ap_c_allocation *mark = arena.first; size_t saved = arena.count, count = 77, written = 77;
	assert(ap_copy_Numbers32(list,NULL,0,&written) == 6 && written == 77);
	assert(ap_export_length32(&arena,list,&result) == 4 && result == 77);
	attempts = 0; reject_allocation = 1; assert(!ap_measure_Numbers32(list,&count) && count == 300);
	assert(!attempts && arena.first == mark && arena.count == saved && !arena.depth); reject_allocation = 0;
	assert(count <= SIZE_MAX / sizeof(int32_t)); int32_t *copy = malloc(count * sizeof(*copy)); assert(copy);
	assert(!ap_copy_Numbers32(list,copy,count,&written) && written == count); ap_arena_Nat_destroy(&arena);
	for (size_t i = 0; i < count; ++i) assert(copy[i] == values[i]);
	free(copy);
}

static void reference(void)
{
	struct ap_c_arena arena = {0}; int32_t values[] = {-1,0,1}, sum; const struct ap_data_Numbers32 *input, *out;
	assert(!ap_from_Numbers32(&arena,values,3,&input));
	for (unsigned form = 0; form < 3; ++form) {
		if (!form) assert(!ap_export_take32(&arena,input,1,&out));
		else if (form == 1) assert(!ap_export_drop32(&arena,input,1,&out));
		else assert(!ap_export_slice32(&arena,input,1,1,&out));
		size_t count, written; assert(!ap_measure_Numbers32(out,&count)); int32_t *copy = count ? malloc(count * sizeof(*copy)) : NULL; assert(!count || copy);
		assert(!ap_copy_Numbers32(out,copy,count,&written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",",copy[i]);
		putchar('|'); free(copy);
	}
	assert(!ap_export_sum32(&arena,input,&sum)); printf("%" PRId32 "|",sum); ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	lists(); payloads(); failures(); long_chain(); reference(); return 0;
}
