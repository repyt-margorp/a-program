#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_at;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	return ++attempts == fail_at ? NULL : __real_malloc(size);
}

static void lists(void)
{
	const int32_t choices[] = {INT32_MIN,0,INT32_MAX}, right[] = {7,-1}; size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 3)
		for (size_t code = 0; code < combinations; ++code) {
			int32_t a[4], out[8]; size_t digits = code, written = 77;
			for (size_t i = 0; i < length; ++i,digits /= 3) a[i] = choices[digits%3];
			assert(!ap_buffer_identity32(a,length,out,8,&written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(out[i] == a[i]);
			assert(!ap_buffer_append32(a,length,right,2,out,8,&written) && written == length + 2);
			for (size_t i = 0; i < written; ++i) assert(out[i] == (i < length ? a[i] : right[i-length]));
			for (uint32_t start = 0; start < 6; ++start) for (uint32_t count = 0; count < 6; ++count) {
				size_t first = start < length ? start : length, expected = length - first;
				if (expected > count) expected = count;
				assert(!ap_buffer_slice32(a,length,start,count,out,8,&written) && written == expected);
				for (size_t i = 0; i < written; ++i) assert(out[i] == a[first+i]);
			}
			++cases;
		}
	assert(cases == 121);
}

static void payloads(void)
{
	int64_t a[] = {INT64_MIN,0,INT64_MAX}, b[3]; size_t n;
	struct ap_enum_Bool flags[] = {{0},{1}}, copied_flags[2];
	struct ap_data_Packet packets[] = {{.tag = 0},{.tag = 1,.fields.c1 = {INT32_MIN,{1}}},{.tag = 2,.fields.c2 = {INT64_MAX}}}, copied_packets[3];
	uint32_t naturals[] = {0,1,UINT32_MAX}, copied_naturals[3];
	assert(!ap_buffer_identity64(a,3,b,3,&n) && n == 3);
	assert(!ap_buffer_identity_flags(flags,2,copied_flags,2,&n) && n == 2);
	assert(!ap_buffer_identity_records(packets,3,copied_packets,3,&n) && n == 3);
	assert(!ap_buffer_identity_nats(naturals,3,copied_naturals,3,&n) && n == 3);
	for (size_t i = 0; i < 3; ++i) assert(a[i] == b[i] && naturals[i] == copied_naturals[i]);
	assert(copied_flags[0].tag == 0 && copied_flags[1].tag == 1);
	assert(copied_packets[0].tag == 0 && copied_packets[1].tag == 1 && copied_packets[1].fields.c1.f0 == INT32_MIN && copied_packets[1].fields.c1.f1.tag == 1);
	assert(copied_packets[2].tag == 2 && copied_packets[2].fields.c2.f0 == INT64_MAX);
	/* Validate active value fields before publishing any output. */
	packets[1].fields.c1.f1.tag = 77; n = 77;
	struct ap_data_Packet saved[3]; memcpy(saved,copied_packets,sizeof(saved));
	assert(ap_buffer_identity_records(packets,3,copied_packets,3,&n) == 2 && n == 77 && !memcmp(saved,copied_packets,sizeof(saved)));
	flags[1].tag = 77; n = 77;
	assert(ap_buffer_identity_flags(flags,2,copied_flags,2,&n) == 2 && n == 77 && copied_flags[0].tag == 0 && copied_flags[1].tag == 1);
}

static void failures(void)
{
	int32_t input[] = {1,2,3}, out[] = {77,77,77,77}; size_t written = 77;
	assert(ap_buffer_identity32(input,3,out,4,NULL) == 1);
	assert(ap_buffer_identity32(NULL,3,out,4,&written) == 1);
	assert(ap_buffer_identity32(input,SIZE_MAX,out,4,&written) == 6);
	assert(ap_buffer_append32(input,3,NULL,2,out,4,&written) == 1);
	assert(ap_buffer_identity32(input,3,NULL,3,&written) == 1);
	assert(ap_buffer_identity32(input,3,out,2,&written) == 6);
	assert(written == 77); for (size_t i = 0; i < 4; ++i) assert(out[i] == 77);
	assert(!ap_buffer_identity32(NULL,0,NULL,0,&written) && !written);
	int32_t large[300], copied[300];
	for (size_t i = 0; i < 300; ++i) { large[i] = (int32_t)i; copied[i] = 77; }
	written = 77;
	assert(ap_buffer_append32(large,300,input,3,copied,300,&written) == 4 && written == 77);
	for (size_t i = 0; i < 300; ++i) assert(copied[i] == 77);
	assert(!ap_buffer_identity32(large,300,copied,300,&written) && written == 300);
	for (size_t i = 0; i < 300; ++i) assert(copied[i] == large[i]);
#ifndef ARRAY_SHARED
	/* Exhaust every allocation in conversion of both operands and source call. */
	attempts = 0; assert(!ap_buffer_append32(input,3,input,3,copied,300,&written)); size_t total = attempts; assert(total > 8);
	for (size_t fault = 1; fault <= total; ++fault) {
		attempts = 0; fail_at = fault; written = 77; for (size_t i = 0; i < 300; ++i) copied[i] = 77;
		assert(ap_buffer_append32(input,3,input,3,copied,300,&written) == 3 && written == 77);
		for (size_t i = 0; i < 300; ++i) assert(copied[i] == 77);
	}
	fail_at = 0;
#endif
}

static void reference(void)
{
	int32_t input[] = {-1,0,1}, out[3]; size_t n;
	for (unsigned form = 0; form < 3; ++form) {
		if (!form) assert(!ap_buffer_take32(input,3,1,out,3,&n));
		else if (form == 1) assert(!ap_buffer_drop32(input,3,1,out,3,&n));
		else assert(!ap_buffer_slice32(input,3,1,1,out,3,&n));
		for (size_t i = 0; i < n; ++i) printf("%" PRId32 ",",out[i]);
		putchar('|');
	}
}

int main(void)
{
	lists(); payloads(); failures(); reference(); return 0;
}
