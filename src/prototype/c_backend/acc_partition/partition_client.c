#include "support.c"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

struct comparison_context { const uint32_t *values; size_t count, calls, fail; uint32_t pivot; int malformed; };

#ifndef PARTITION_MUTATION
static int traced_compare(struct qs_arena *a, const void *raw, uint32_t head, uint32_t pivot)
{
	struct comparison_context *context=(struct comparison_context *)raw;
	assert(pivot==context->pivot && context->calls<context->count && head==context->values[context->calls]);
	if (++context->calls==context->fail) a->status=3;
	return context->malformed ? 2 : head<=pivot;
}
#endif

static void clear(struct qs_arena *a)
{
	while (a->first) { struct qs_allocation *p=a->first; a->first=p->next; free(p); }
}

static struct qs_measured input(struct qs_arena *a, const uint32_t *values, size_t count)
{
	const struct qs_list *xs=list_nil(a,&nat_type);
	for (size_t i=count; i; --i) xs=list_cons(a,&nat_type,values[i-1],xs);
	return measure(a,&nat_type,xs);
}

int main(void)
{
	const uint32_t values[]={3,0,2,1,2}; struct qs_arena arena={0};
	struct qs_measured measured=input(&arena,values,5);
	const struct qs_partition *p=gp_partition(&arena,&nat_type,(struct qs_compare){nat_less_or_equal,NULL},1,measured.size,measured.values);
	assert(p && !arena.status && p->bound==5 && p->lower_size+p->upper_size==5);
	assert(p->lower->index==p->lower_size && p->upper->index==p->upper_size);
	assert(p->lower_bound->left==p->lower_size && p->upper_bound->left==p->upper_size
		&& p->lower_bound->right==6 && p->upper_bound->right==6);
	printf("%" PRIu32 ":",p->lower_size);
	for (const struct qs_sized_list *v=p->lower; v->tag==QS_CONS; v=v->tail) printf("%" PRIu32 ",",v->head);
	printf("|%" PRIu32 ":",p->upper_size);
	for (const struct qs_sized_list *v=p->upper; v->tag==QS_CONS; v=v->tail) printf("%" PRIu32 ",",v->head);
	putchar('|'); clear(&arena);
#ifndef PARTITION_MUTATION
	for (unsigned mode=0; mode<3; ++mode) {
		arena=(struct qs_arena){0}; measured=input(&arena,values,5); size_t prior=arena.count;
		struct comparison_context context={values,5,0,mode==1 ? 1u : 0u,1,mode==2};
		p=gp_partition(&arena,&nat_type,(struct qs_compare){traced_compare,&context},1,5,measured.values);
		if (!mode) assert(p && !arena.status && context.calls==5);
		else assert(!p && arena.status==(mode==1 ? 3 : 2) && context.calls==1 && arena.count==prior);
		clear(&arena);
	}
#endif
	return 0;
}
