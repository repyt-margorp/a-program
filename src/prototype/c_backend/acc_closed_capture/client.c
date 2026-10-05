#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	assert(argc==2); int reverse=!strcmp(argv[1],"descending");
	const uint32_t input[] = {3,0,2,1,2};
	const uint32_t expected[] = {0,1,2,2,3};
	uint32_t output[5]; size_t written = 77; struct qs_trace trace;
	assert(!gs_sort(input,5,output,5,&written,&trace) && written == 5);
	assert(trace.folded_down == 10 && trace.acc_branch == 11);
	for (size_t i = 0; i < written; ++i) {
		assert(output[i] == expected[reverse ? 4-i : i]); printf("%" PRIu32 ",",output[i]);
	}
	putchar('|'); return 0;
}
