#include "support.c"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void clear(struct qs_arena *a)
{
	while (a->first) { struct qs_allocation *p=a->first; a->first=p->next; free(p); }
}

int main(void)
{
	const struct qs_list nil={.element_type=&nat_type,.tag=QS_NIL};
	const struct qs_list last={&nat_type,QS_CONS,1,&nil}, right={&nat_type,QS_CONS,2,&last};
	const struct qs_list tail={&nat_type,QS_CONS,0,&nil}; struct qs_list left={&nat_type,QS_CONS,3,&tail};
	struct qs_arena arena={0}; const struct qs_list *result=ga_append(&arena,&nat_type,&left,&right);
	assert(result && !arena.status);
	for (const struct qs_list *p=result; p->tag==QS_CONS; p=p->tail) printf("%" PRIu32 ",",p->head);
	putchar('|'); clear(&arena);
#ifndef APPEND_MUTATION
	/* Capture test changes the external C cell after Fold construction. The
		* emitted callable must use copied source fields, not reread that cell.
		* This tests target captures, not a source mutable-List contract. */
	arena=(struct qs_arena){0}; struct ga_result callable=ga_fold(&arena,&nat_type,&left);
	assert(!arena.status && !arena.count && callable.head==3 && callable.original_tail==&tail);
	left.tag=QS_NIL; left.head=77; left.tail=NULL;
	result=ga_apply(&arena,&callable,&right);
	assert(result && !arena.status && result->head==3 && result->tail->head==0 && result->tail->tail==&right);
	clear(&arena);
	/* A malformed foreign tail is only inspected when its IH is forced. */
	left=(struct qs_list){&nat_type,QS_CONS,3,NULL}; arena=(struct qs_arena){0};
	callable=ga_fold(&arena,&nat_type,&left); assert(!arena.status && !arena.count);
	assert(!ga_apply(&arena,&callable,&right) && arena.status==2 && !arena.count); clear(&arena);
	arena=(struct qs_arena){0}; callable=ga_fold(&arena,&nat_type,&nil);
	assert(ga_apply(&arena,&callable,&right)==&right && !arena.status && !arena.count); clear(&arena);
	const struct qs_nat_type foreign_type={"foreign"}; const struct qs_list foreign_nil={.element_type=&foreign_type,.tag=QS_NIL};
	arena=(struct qs_arena){0}; callable=ga_fold(&arena,&nat_type,&nil);
	assert(!ga_apply(&arena,&callable,&foreign_nil) && arena.status==2 && !arena.count); clear(&arena);
#endif
	return 0;
}
