#include "support.c"
#include <assert.h>

static void clear(struct qs_arena *a)
{
	while (a->first) { struct qs_allocation *p=a->first; a->first=p->next; free(p); }
}
int main(void)
{
	struct qs_arena a={0}; const struct qs_acc *proof=gn_nat_accessible(&a,5);
	assert(proof && proof->down.call==gs_down && !a.status);
	const struct gd_capture *capture=proof->down.context;
	assert(capture->parameter==4 && capture->proof->subject==4 && capture->ih.original.call==capture->proof->down.call && capture->ih.parent==4);
	const struct qs_lt *step=lt_step(&a,4); struct gd_refinement r;
	assert(gd_refine(&a,capture->proof,4,step,&r) && r.map_count==6 && r.paths[0].center==step && r.paths[1].center==step);
	struct gd_quoted_acc quoted; size_t count=a.count;
	assert(gd_transport_right(&a,&r,capture->proof,&quoted) && a.count==count);
	const struct qs_acc *acted=gd_force_acc(&a,&quoted);
	assert(acted && acted!=capture->proof && acted->subject==4 && acted->down.call==gd_action_down && a.count==count+2);
	const struct gd_acc_action *context=acted->down.context;
	assert(context->original==capture->proof && context->refinement.edge==step);
	assert(call_raw_down(&a,acted,3,lt_step(&a,3)) && !a.status);
	const struct qs_lt *prior=lt_weaken(&a,2,3,lt_step(&a,2));
	const struct qs_lt *weaken=lt_weaken(&a,2,4,prior);
	assert(gd_refine(&a,capture->proof,2,weaken,&r) && r.map_count==10);
	const struct qs_lt *transported=gd_transport_left(&a,&r,prior);
	assert(transported && transported!=prior && transported->tag==prior->tag && transported->left==2 && transported->right==4);
	const struct gd_lt_action *left=(const struct gd_lt_action *)transported;
	assert(left->original==prior && left->refinement.edge==weaken);
	const struct qs_acc *child=call_raw_down(&a,proof,2,weaken); assert(child && child->subject==2 && !a.status);
	const struct qs_lt *lift=lt_lift(&a,2,4,prior); child=call_raw_down(&a,proof,3,lift);
	assert(child && child->subject==3 && child->down.call==gs_down && !a.status && a.trace.raw_down_step && a.trace.raw_down_weaken && a.trace.raw_down_lift);
	clear(&a); a=(struct qs_arena){0}; struct qs_lt malformed={.tag=QS_LT_STEP,.left=0,.right=1,.fields.step=3};
	const struct qs_acc *zero=gn_nat_accessible(&a,0); struct gd_refinement unchanged={0};
	assert(!gd_refine(&a,zero,0,&malformed,&unchanged) && a.status==2); clear(&a);
	a=(struct qs_arena){0}; struct gd_refinement erased={0}; assert(!gd_path_valid(&a,&erased) && a.status==2 && !a.count);
	return 0;
}
