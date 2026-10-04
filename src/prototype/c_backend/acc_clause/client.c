#include "../acc_quicksort_mockup/mockup.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

int gs_sort(const uint32_t *, size_t, uint32_t *, size_t, size_t *, struct qs_trace *);

int main(void)
{
	const uint32_t input[] = {3,0,2,1,2}; uint32_t out[5]; size_t count;
	assert(!gs_sort(input,5,out,5,&count,NULL));
	for (size_t i = 0; i < count; ++i) printf("%" PRIu32 ",",out[i]);
	putchar('|'); return 0;
}
