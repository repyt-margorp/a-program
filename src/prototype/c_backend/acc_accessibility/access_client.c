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
	uint32_t observed[]={0,1,3}; struct qs_arena a={0};
	for (size_t i=0; i<sizeof(observed)/sizeof(*observed); ++i) {
		const struct qs_acc *proof=gn_nat_accessible(&a,observed[i]);
		assert(proof && !a.status && proof->domain==&nat_type && proof->relation==&lt_relation && proof->subject==observed[i] && proof->current==observed[i]);
		printf("%" PRIu32 "%c",proof->current,i==2 ? '|' : ',');
	}
	/* This exercises emitted Nat recurrence with the explicitly manual
		* accessibleSucc/raw-down boundary; it does not lower its transports. */
	for (uint32_t n=0; n<32; ++n) {
		const struct qs_acc *proof=gn_nat_accessible(&a,n); assert(proof && !a.status && proof->subject==n);
	}
	const struct qs_acc *proof=gn_nat_accessible(&a,5); const struct qs_acc *prior=proof->down.context;
	const struct qs_lt *step=lt_step(&a,4);
	assert(call_raw_down(&a,proof,4,step)==prior && !a.status && a.trace.raw_down_step);
	const struct qs_lt *edge=lt_weaken(&a,2,3,lt_step(&a,2));
	const struct qs_lt *weaken=lt_weaken(&a,2,4,edge);
	const struct qs_acc *weakened=call_raw_down(&a,proof,2,weaken);
	assert(weakened && weakened->subject==2 && !a.status && a.trace.raw_down_weaken);
	const struct qs_lt *lift=lt_lift(&a,2,4,edge);
	const struct qs_acc *lifted=call_raw_down(&a,proof,3,lift);
	assert(lifted && lifted->subject==3 && !a.status && a.trace.raw_down_lift);
	clear(&a); a=(struct qs_arena){0};
	assert(!gn_nat_accessible(&a,300) && a.status==4 && !a.count && !a.depth); clear(&a);
	return 0;
}
