#include "support.c"
#include <assert.h>
#include <stdio.h>

static void clear(struct qs_arena *a)
{
	while (a->first) { struct qs_allocation *p=a->first; a->first=p->next; free(p); }
}
int main(void)
{
	for (uint32_t n=0; n<33; ++n) {
		struct qs_arena a={0}; const struct qs_list *input=list_nil(&a,&nat_type);
		for (uint32_t i=n; i; --i) input=list_cons(&a,&nat_type,(i-1)%5,input);
		struct qs_measured m=gm_fold(&a,&nat_type,input); assert(!a.status && !a.depth && m.size==n && m.values);
		const struct qs_sized_list *values=m.values;
		for (uint32_t i=0; i<n; ++i) {
			assert(values && values->element_type==&nat_type && values->tag==QS_CONS && values->index==n-i
				&& values->tail_size==n-i-1 && values->head==i%5); values=values->tail;
		}
		assert(values && values->tag==QS_NIL && !values->index && values->element_type==&nat_type);
		if (n==5) printf("%u",m.size);
		clear(&a);
	}
	struct qs_arena a={0}; struct qs_list invalid={.element_type=&nat_type,.tag=7};
	assert(!gm_fold(&a,&nat_type,&invalid).values && a.status==2 && !a.depth && !a.count);
	a=(struct qs_arena){0}; assert(!gm_fold(&a,&nat_type,NULL).values && a.status==2 && !a.depth && !a.count);
	struct qs_nat_type other={0}; a=(struct qs_arena){0}; invalid.tag=QS_NIL;
	assert(!gm_fold(&a,&other,&invalid).values && a.status==2 && !a.depth && !a.count);
	/* A malformed recursive field is examined only at the actual IH Force. */
	a=(struct qs_arena){0}; struct qs_list cons={&nat_type,QS_CONS,1,&invalid}; invalid.tag=7;
	assert(!gm_fold(&a,&nat_type,&cons).values && a.status==2 && !a.depth && !a.count);
	a=(struct qs_arena){.depth=255}; invalid.tag=QS_NIL;
	assert(!go_outer(&a,&nat_type,(struct qs_compare){gc_compare,NULL},&invalid) && a.status==4 && a.depth==255 && !a.count);
	a=(struct qs_arena){0}; assert(!go_outer(&a,&nat_type,(struct qs_compare){0},&invalid) && a.status==2 && !a.depth && !a.count);
	return 0;
}
