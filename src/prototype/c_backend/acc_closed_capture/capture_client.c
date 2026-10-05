#include "component.c"
#include <assert.h>
#include <string.h>

static void release(struct qs_arena *arena)
{
	while (arena->first) { struct qs_allocation *p=arena->first; arena->first=p->next; free(p); }
}

int main(int argc, char **argv)
{
	assert(argc==2); int opposite=!strcmp(argv[1],"opposite");
	const uint32_t input[]={3,0,2,1,2},expected[]={0,1,2,2,3};
	for (unsigned tag=0; tag<2; ++tag) {
		/* Exercise the same emitted code with both constructor values at run time.
			* Both recursive Acc closures must keep this distinct private context. */
		const struct gclosed_context capture={tag}; struct qs_arena arena={0};
		const struct qs_list *list=list_nil(&arena,&nat_type);
		for (size_t i=5; i; --i) list=list_cons(&arena,&nat_type,input[i-1],list);
		const struct qs_list *result=go_outer(&arena,&nat_type,(struct qs_compare){gclosed_compare,&capture},list);
		assert(!arena.status && result && arena.trace.folded_down==10 && arena.trace.acc_branch==11);
		int reverse=opposite ? tag==1 : tag==0;
		for (size_t i=0; i<5; ++i) { assert(result->tag==QS_CONS && result->head==expected[reverse ? 4-i : i]); result=result->tail; }
		assert(result->tag==QS_NIL); release(&arena);
	}
	struct qs_arena arena={0}; const struct gclosed_context bad={2};
	assert(!gclosed_compare(&arena,&bad,0,0) && arena.status==2); release(&arena);
	arena=(struct qs_arena){0};
	assert(!gclosed_compare(&arena,NULL,0,0) && arena.status==2); release(&arena);
	return 0;
}
