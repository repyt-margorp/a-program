#include "support.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	const uint32_t samples[][2]={{0,0},{3,0},{2,2},{2,3},{3,2}};
	for (size_t i=0; i<sizeof(samples)/sizeof(*samples); ++i) {
		struct qs_arena a={0}; int result=gc_compare(&a,NULL,samples[i][0],samples[i][1]);
		assert(!a.status && !a.count && !a.depth); printf("%d%c",result,i==4 ? '|' : ',');
	}
	for (uint32_t left=0; left<32; ++left) for (uint32_t right=0; right<32; ++right) {
		struct qs_arena a={0}; assert(gc_compare(&a,NULL,left,right)==(left<=right) && !a.status && !a.count && !a.depth);
	}
	/* Extreme opposite argument is not traversed: source right Match precedes
		* forcing the left recursive IH, and left zero ignores right entirely. */
	struct qs_arena a={0}; assert(gc_compare(&a,NULL,0,UINT32_MAX)==1 && !a.status && !a.count && !a.depth);
	a=(struct qs_arena){0}; assert(gc_compare(&a,NULL,UINT32_MAX,0)==0 && !a.status && !a.count && !a.depth);
	a=(struct qs_arena){0}; assert(gc_compare(&a,NULL,300,300)==0 && a.status==4 && !a.count && !a.depth);
	/* A Fold result retains the constructor snapshot until right is applied. */
	a=(struct qs_arena){0}; struct gc_result f=gc_fold(&a,2); assert(f.nonzero && f.predecessor==1 && !a.status && !a.count);
	assert(!gc_apply(&a,&f,1) && !a.status && !a.depth);
	a=(struct qs_arena){0}; f=(struct gc_result){0,UINT32_MAX}; assert(gc_apply(&a,&f,UINT32_MAX)==1 && !a.status);
	a=(struct qs_arena){0}; f=(struct gc_result){7,0}; assert(!gc_apply(&a,&f,0) && a.status==2 && !a.count && !a.depth);
	a=(struct qs_arena){.depth=255}; assert(!gc_compare(&a,NULL,1,0) && !a.status && a.depth==255);
	a=(struct qs_arena){.depth=255}; assert(!gc_compare(&a,NULL,1,1) && a.status==4 && a.depth==255 && !a.count);
	return 0;
}
