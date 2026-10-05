#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

struct comparison_context { int reverse; size_t observed_calls; };

static int compare(const void *context, uint32_t left, uint32_t right)
{
	struct comparison_context *state=(struct comparison_context *)context;
	/* Observation only: the source-function result reads an immutable mode. */
	++state->observed_calls;
	return state->reverse ? left>=right : left<=right;
}

static int malformed(const void *context, uint32_t left, uint32_t right)
{
	(void)left; (void)right; return *(const int *)context;
}

int main(int argc, char **argv)
{
	assert(argc==2);
	struct qs_trace all={0}; size_t cases=0;
	for (int reverse=0; reverse<2; ++reverse)
		for (size_t length=0,combinations=1; length<=4; ++length,combinations*=4)
			for (size_t code=0; code<combinations; ++code) {
				struct comparison_context context={reverse,0};
				struct gs_comparator comparator={compare,&context};
				uint32_t input[4],output[4],expected[4]={0},actual[4]={0};
				size_t digits=code,written=77; struct qs_trace trace;
				for (size_t i=0; i<length; ++i,digits/=4) { input[i]=(uint32_t)(digits%4); ++expected[input[i]]; }
				assert(!gs_sort_with(&comparator,input,length,output,4,&written,&trace) && written==length);
				for (size_t i=0; i<length; ++i) {
					assert(output[i]<4); ++actual[output[i]];
					if (i) assert(reverse ? output[i-1]>=output[i] : output[i-1]<=output[i]);
				}
				assert(!memcmp(expected,actual,sizeof(expected)));
				assert(trace.folded_down==2*length && trace.acc_branch==2*length+1);
				assert(length<2 || context.observed_calls);
				all.raw_down_step+=trace.raw_down_step; all.raw_down_weaken+=trace.raw_down_weaken;
				all.raw_down_lift+=trace.raw_down_lift; all.partition_lower+=trace.partition_lower;
				all.partition_upper+=trace.partition_upper; ++cases;
			}
	assert(cases==682 && all.raw_down_step && all.raw_down_weaken && all.raw_down_lift
		&& all.partition_lower && all.partition_upper);
	uint32_t input[]={3,0,2,1,2},output[]={77,77,77,77,77}; size_t written=77;
	assert(gs_sort_with(NULL,input,5,output,5,&written,NULL)==1 && written==77);
	struct gs_comparator null_code={0};
	assert(gs_sort_with(&null_code,input,5,output,5,&written,NULL)==1 && written==77);
	for (int code=-1; code<=2; code+=3) {
		struct gs_comparator bad={malformed,&code};
		assert(gs_sort_with(&bad,input,5,output,5,&written,NULL)==2 && written==77);
		for (size_t i=0; i<5; ++i) assert(output[i]==77);
	}
	struct comparison_context context={!strcmp(argv[1],"descending"),0};
	struct gs_comparator comparator={compare,&context};
	assert(!gs_sort_with(&comparator,input,5,output,5,&written,NULL) && written==5 && context.observed_calls);
	for (size_t i=0; i<written; ++i) printf("%" PRIu32 ",",output[i]);
	putchar('|'); return 0;
}
