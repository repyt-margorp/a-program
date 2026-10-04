#include "fold.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct state;
struct clause_capture {
	struct state *state;
	struct af_capture outer;
	const struct iv_nat *current;
	struct iv_acc_c0_f1 original;
	struct af_ih ih;
};
struct state {
	struct iv_nat zero,one;
	struct iv_lt edge;
	struct iv_acc child,parent;
	struct iv_sized empty,input;
	struct iv_list nil,output;
	struct clause_capture clauses[16];
	size_t used, raw_calls, clause_calls, result_calls;
	int provider_failure, clause_failure, result_failure, bad_result, nested;
};

static int comparison(void *context, const struct iv_nat *left,
	const struct iv_nat *right, const struct iv_bool **result)
{
	(void)context; (void)left; (void)right; (void)result;
	/* Source's partition nil case does not compare; no comparison observation
		* or implementation is claimed by this bounded manual clause fixture. */
	assert(0); return 9;
}

static int original_down(void *context, const struct iv_nat *y,
	const struct iv_lt *edge, const struct iv_acc **result)
{
	struct state *s=context; ++s->raw_calls;
	if (s->provider_failure) { *result=&s->parent; return s->provider_failure; }
	/* Actual accessibleSucc step clause returns its captured original proof. */
	if (y!=&s->zero || edge!=&s->edge || edge->tag || edge->index0!=y || edge->index1!=&s->one) return 2;
	*result=&s->child; return 0;
}

static int impossible_down(void *context, const struct iv_nat *y,
	const struct iv_lt *edge, const struct iv_acc **result)
{
	(void)context; (void)y; (void)edge; (void)result;
	/* Acc zero has no source-valid LT y zero; foreign use remains a refusal. */
	return 2;
}

/* Hand-authored clauses for actual quickSortAcc at sizes0/1 only. Higher sizes
	* explicitly refuse4; this fixture is not generated full QuickSort lowering.
	* Size1 uses partition nil's two LT.step proofs and both callable IH uses. */
static int apply_manual_clause(void *context, const struct iv_sized *input, const struct iv_list **result)
{
	struct clause_capture *capture=context; struct state *s=capture->state; ++s->result_calls;
	assert(capture->outer.comparison.call==comparison && capture->outer.comparison.context==s);
	if (s->result_failure) { *result=&s->output; return s->result_failure; }
	if (capture->current==&s->zero) {
		if (input->tag || input->index0!=&s->zero) return 2;
		*result=&s->nil; return 0;
	}
	if (capture->current!=&s->one) return 4;
	if (input->tag!=1 || input->index0!=&s->one || input->fields.c1.field0!=&s->zero || input->fields.c1.field2!=&s->empty) return 2;
	struct af_result lower={0},upper={0}; const struct iv_list *lower_values=NULL,*upper_values=NULL;
	int status=af_force_down(&capture->ih,&s->zero,&s->edge,&lower);
	if (!status) status=af_apply(&lower,&s->empty,&lower_values);
	if (!status) status=af_force_down(&capture->ih,&s->zero,&s->edge,&upper);
	if (!status) status=af_apply(&upper,&s->empty,&upper_values);
	if (status) return status;
	assert(lower_values==&s->nil && upper_values==&s->nil);
	/* Source append(empty, cons(pivot, upperResult)) for this actual case. */
	s->output=(struct iv_list){.self_identity=s->nil.self_identity,.parameter0=capture->outer.element_type,.tag=1,.fields.c1={input->fields.c1.field1,upper_values}};
	*result=&s->output; return 0;
}

static int manual_clause(struct af_program *program, const struct iv_nat *current,
	struct iv_acc_c0_f1 original, struct af_ih ih, struct af_result *result)
{
	struct state *s=program->capture.comparison.context; ++s->clause_calls;
	assert(ih.program==program && ih.current==current && ih.original.call==original.call && ih.original.context==original.context);
	assert(program->capture.element_type==s->input.parameter0);
	if (s->nested) return af_fold(program,&s->parent,result);
	if (s->clause_failure) { *result=(struct af_result){apply_manual_clause,s,&s->one}; return s->clause_failure; }
	if (s->bad_result) { *result=(struct af_result){0}; return 0; }
	assert(s->used<16); struct clause_capture *capture=&s->clauses[s->used++];
	*capture=(struct clause_capture){s,program->capture,current,original,ih};
	*result=(struct af_result){apply_manual_clause,capture,current}; return 0;
}

static unsigned list_length(const struct iv_list *list)
{
	unsigned count=0;
	while (list->tag==1) { ++count; list=list->fields.c1.field1; }
	assert(list->tag==0); return count;
}

static unsigned natural_count(const struct iv_nat *natural)
{
	unsigned count=0;
	while (natural->tag==1) { ++count; natural=natural->fields.c1.field0; }
	assert(natural->tag==0); return count;
}

int main(void)
{
	static const unsigned char nat_id,lt_id,acc_id,sized_id,list_id;
	struct state s={0};
	s.zero=(struct iv_nat){.self_identity=&nat_id,.tag=0};
	s.one=(struct iv_nat){.self_identity=&nat_id,.tag=1,.fields.c1={&s.zero}};
	s.edge=(struct iv_lt){.self_identity=&lt_id,.index0=&s.zero,.index1=&s.one,.tag=0,.fields.c0={&s.zero}};
	s.child=(struct iv_acc){.self_identity=&acc_id,.parameter0=&nat_id,.parameter1=&lt_id,.index0=&s.zero,.tag=0,.fields.c0={&s.zero,{impossible_down,&s}}};
	s.parent=(struct iv_acc){.self_identity=&acc_id,.parameter0=&nat_id,.parameter1=&lt_id,.index0=&s.one,.tag=0,.fields.c0={&s.one,{original_down,&s}}};
	s.empty=(struct iv_sized){.self_identity=&sized_id,.parameter0=&nat_id,.index0=&s.zero,.tag=0};
	s.input=(struct iv_sized){.self_identity=&sized_id,.parameter0=&nat_id,.index0=&s.one,.tag=1,.fields.c1={&s.zero,&s.one,&s.empty}};
	s.nil=(struct iv_list){.self_identity=&list_id,.parameter0=&nat_id,.tag=0};
	struct af_program program={.capture={&nat_id,{comparison,&s}},.clause=manual_clause,.limit=8};
	struct af_result parent={0},child={0}; const struct iv_list *output=NULL;
	assert(!af_fold(&program,&s.child,&child) && child.index==&s.zero && !af_apply(&child,&s.empty,&output) && output==&s.nil);
	printf("%u,",list_length(output));
	assert(!af_fold(&program,&s.parent,&parent) && parent.index==&s.one);
	assert(!af_apply(&parent,&s.input,&output) && output==&s.output && output->fields.c1.field0==&s.one && output->fields.c1.field1==&s.nil);
	printf("%u,%u",list_length(output),natural_count(output->fields.c1.field0));
	assert(s.raw_calls==2 && s.clause_calls==4 && s.result_calls==4 && !program.depth);
	struct af_result saved=parent; size_t before=s.used;
	s.clause_failure=7;
	assert(af_fold(&program,&s.parent,&parent)==7 && !memcmp(&parent,&saved,sizeof(parent)) && !program.depth);
	s.clause_failure=0; s.bad_result=1;
	assert(af_fold(&program,&s.parent,&parent)==2 && !memcmp(&parent,&saved,sizeof(parent)));
	s.bad_result=0; s.provider_failure=8;
	struct af_ih ih={&program,s.parent.fields.c0.field1,&s.one};
	assert(af_force_down(&ih,&s.zero,&s.edge,&parent)==8 && !memcmp(&parent,&saved,sizeof(parent)) && s.used==before);
	s.provider_failure=0; s.result_failure=9; output=&s.nil;
	assert(af_apply(&parent,&s.input,&output)==9 && output==&s.nil); s.result_failure=0;
	program.limit=0;
	assert(af_fold(&program,&s.parent,&parent)==4 && !memcmp(&parent,&saved,sizeof(parent)));
	program.limit=3; s.nested=1;
	assert(af_fold(&program,&s.parent,&parent)==4 && !memcmp(&parent,&saved,sizeof(parent)) && !program.depth); s.nested=0;
	struct iv_acc malformed=s.parent; malformed.tag=4;
	assert(af_fold(&program,&malformed,&parent)==2 && !memcmp(&parent,&saved,sizeof(parent)));
	assert(af_fold(NULL,&s.parent,&parent)==1 && af_apply(NULL,&s.input,&output)==1);
	struct af_ih zero_ih={&program,s.child.fields.c0.field1,&s.zero};
	assert(af_force_down(&zero_ih,&s.zero,&s.edge,&parent)==2 && !memcmp(&parent,&saved,sizeof(parent)));
	struct iv_nat two={.self_identity=&nat_id,.tag=1,.fields.c1={&s.one}};
	struct iv_acc larger=s.parent; larger.index0=&two; larger.fields.c0.field0=&two;
	struct af_result unsupported={0};
	assert(!af_fold(&program,&larger,&unsupported) && unsupported.index==&two);
	output=&s.nil;
	assert(af_apply(&unsupported,&s.input,&output)==4 && output==&s.nil);
	return 0;
}
