#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static size_t attempts, fail_at;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	return ++attempts == fail_at ? NULL : __real_malloc(size);
}

static void lists(void)
{
	size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 4)
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t a[4], out[5], histogram[4] = {0}; size_t digits = code, n;
			for (size_t i = 0; i < length; ++i,digits /= 4) { a[i] = (uint32_t)(digits%4); ++histogram[a[i]]; }
			for (unsigned alias = 0; alias < 2; ++alias) {
				int status = alias ? ap_buffer_sort_alias(a,length,out,5,&n) : ap_buffer_sort(a,length,out,5,&n);
				assert(!status && n == length); uint32_t copied[4] = {0};
				for (size_t i = 0; i < n; ++i) { assert(out[i] < 4); ++copied[out[i]]; if (i) assert(out[i-1] <= out[i]); }
				for (size_t i = 0; i < 4; ++i) assert(copied[i] == histogram[i]);
			}
			uint32_t sorted[4]; assert(!ap_buffer_sort(a,length,sorted,4,&n));
			for (uint32_t seed = 0; seed < 4; ++seed) {
				assert(!ap_buffer_insert(seed,sorted,length,out,5,&n) && n == length+1);
				uint32_t copied[4] = {0}; for (size_t i = 0; i < n; ++i) { assert(out[i] < 4); ++copied[out[i]]; if (i) assert(out[i-1] <= out[i]); }
				for (size_t i = 0; i < 4; ++i) assert(copied[i] == histogram[i] + (seed == i));
			}
			++cases;
		}
	assert(cases == 341);
	uint32_t high[] = {UINT32_MAX,0,1}, out[4]; size_t n;
	assert(!ap_buffer_sort(high,3,out,4,&n) && n == 3 && out[0] == 0 && out[1] == 1 && out[2] == UINT32_MAX);
	high[1] = UINT32_MAX-1; n = 77; for (size_t i = 0; i < 4; ++i) out[i] = 77;
	assert(ap_buffer_sort(high,2,out,4,&n) == 4 && n == 77);
	for (size_t i = 0; i < 4; ++i) assert(out[i] == 77);
}

static void failures(void)
{
	uint32_t input[] = {3,1,2,2,0}, out[6]; size_t n = 77;
	for (size_t i = 0; i < 6; ++i) out[i] = 77;
	assert(ap_buffer_sort(input,5,out,4,&n) == 6 && n == 77);
	for (size_t i = 0; i < 6; ++i) assert(out[i] == 77);
#ifndef ARRAY_SHARED
	attempts = 0; assert(!ap_buffer_sort(input,5,out,6,&n)); size_t total = attempts; assert(total > 6);
	for (size_t fault = 1; fault <= total; ++fault) {
		attempts = 0; fail_at = fault; n = 77; for (size_t i = 0; i < 6; ++i) out[i] = 77;
		assert(ap_buffer_sort(input,5,out,6,&n) == 3 && n == 77);
		for (size_t i = 0; i < 6; ++i) assert(out[i] == 77);
	}
	fail_at = 0;
#endif
}

static void reference(void)
{
	uint32_t a[] = {3,1,2,2,0}, b[] = {3,0,2,1}, out[6]; size_t n;
	for (unsigned form = 0; form < 6; ++form) {
		if (!form) assert(!ap_buffer_sort(NULL,0,out,6,&n));
		else if (form == 1) assert(!ap_buffer_sort(a,5,out,6,&n));
		else if (form == 2) assert(!ap_buffer_sort(b,4,out,6,&n));
		else if (form == 3) assert(!ap_buffer_sort_alias(a,5,out,6,&n));
		else if (form == 4) { const uint32_t sorted[] = {0,1,2,2,3}; assert(!ap_buffer_insert(2,sorted,5,out,6,&n)); }
		else assert(!ap_buffer_insert(0,NULL,0,out,6,&n));
		for (size_t i = 0; i < n; ++i) printf("%" PRIu32 ",",out[i]);
		putchar('|');
	}
}

int main(void)
{
	lists(); failures(); reference(); return 0;
}
