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

int main(int argc, char **argv)
{
	assert(argc==2); int reverse=!strcmp(argv[1],"descending");
	size_t cases = 0; struct qs_trace all = {0};
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 4)
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t input[4], out[4], histogram[4] = {0}, actual[4] = {0}; size_t digits = code, written = 77; struct qs_trace trace;
			for (size_t i = 0; i < length; ++i,digits /= 4) { input[i] = (uint32_t)(digits%4); ++histogram[input[i]]; }
			assert(!gs_sort(input,length,out,4,&written,&trace) && written == length);
			for (size_t i = 0; i < length; ++i) { assert(out[i] < 4); ++actual[out[i]]; if (i) assert(reverse ? out[i-1]>=out[i] : out[i-1]<=out[i]); }
			for (size_t i = 0; i < 4; ++i) assert(actual[i] == histogram[i]);
			assert(trace.folded_down == 2*length && trace.acc_branch == 2*length+1);
			all.raw_down_step += trace.raw_down_step; all.raw_down_weaken += trace.raw_down_weaken; all.raw_down_lift += trace.raw_down_lift;
			all.partition_lower += trace.partition_lower; all.partition_upper += trace.partition_upper; ++cases;
		}
	assert(cases == 341 && all.raw_down_step && all.raw_down_weaken && all.raw_down_lift && all.partition_lower && all.partition_upper);
	uint32_t input[] = {3,0,2,1,2}, out[] = {77,77,77,77,77}; size_t written = 77;
	assert(gs_sort(input,5,out,4,&written,NULL) == 6 && written == 77);
	for (size_t i = 0; i < 5; ++i) assert(out[i] == 77);
	assert(gs_sort(NULL,5,out,5,&written,NULL) == 1 && written == 77);
	assert(gs_sort(input,5,NULL,5,&written,NULL) == 1 && written == 77);
	assert(gs_sort(input,SIZE_MAX,out,5,&written,NULL) == 6 && written == 77);
	uint32_t long_input[300], long_output[300];
	for (size_t i = 0; i < 300; ++i) { long_input[i] = 0; long_output[i] = 77; }
	assert(gs_sort(long_input,300,long_output,300,&written,NULL) == 4 && written == 77);
	for (size_t i = 0; i < 300; ++i) assert(long_output[i] == 77);
	uint32_t long_comparison[] = {300,300};
	assert(gs_sort(long_comparison,2,out,5,&written,NULL) == 4 && written == 77);
	attempts = 0; assert(!gs_sort(input,5,out,5,&written,NULL)); size_t total = attempts; assert(total > 10);
	for (size_t fault = 1; fault <= total; ++fault) {
		attempts = 0; fail_at = fault; written = 77; for (size_t i = 0; i < 5; ++i) out[i] = 77;
		assert(gs_sort(input,5,out,5,&written,NULL) == 3 && written == 77);
		for (size_t i = 0; i < 5; ++i) assert(out[i] == 77);
	}
	fail_at = 0;
	for (unsigned form = 0; form < 3; ++form) {
		const uint32_t descending[] = {3,2,1,0};
		assert(!gs_sort(form == 2 ? descending : input,form ? (form == 1 ? 5 : 4) : 0,out,5,&written,NULL));
		for (size_t i = 0; i < written; ++i) printf("%" PRIu32 ",",out[i]);
		putchar('|');
	}
	return 0;
}
