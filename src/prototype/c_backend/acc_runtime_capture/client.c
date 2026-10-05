#include "component.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	assert(argc==3);
	enum gs_bool_mode mode=!strcmp(argv[1],"first") ? GS_BOOL_FIRST : GS_BOOL_SECOND;
	int descending=!strcmp(argv[2],"descending");
	const uint32_t input[]={3,0,2,1,2},ascending[]={0,1,2,2,3};
	uint32_t buffer[5]={0};size_t written=0;struct qs_trace trace={0};
	assert(!gs_sort_mode(mode,input,5,buffer,5,&written,&trace) && written==5);
	assert(trace.folded_down==10 && trace.acc_branch==11 && trace.raw_down_step
		&& trace.raw_down_weaken && trace.raw_down_lift);
	for (size_t i=0;i<5;++i) { assert(buffer[i]==ascending[descending ? 4-i : i]); printf("%u,",buffer[i]); }
	putchar('|');
	uint32_t saved[5];memcpy(saved,buffer,sizeof(saved));written=97;
	assert(gs_sort_mode((enum gs_bool_mode)2,input,5,buffer,5,&written,NULL)==2);
	assert(written==97 && !memcmp(saved,buffer,sizeof(saved)));
	assert(gs_sort_mode((enum gs_bool_mode)-1,NULL,0,buffer,5,&written,NULL)==2 && written==97);
	assert(!gs_sort_mode(mode,NULL,0,NULL,0,&written,NULL) && written==0);
	return 0;
}
