#include "emit.h"
#include "classifier.h"
#include "computation.h"
#include <string.h>

static const struct pg_term *unary(const struct pg_term *term, const struct pg_object *operation)
{
	if (!term || term->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *head=term->as.application.function;
	return head->kind==PG_REFERENCE && head->as.reference==operation ? term->as.application.argument : NULL;
}

static const struct pg_term *carrier(const struct pg_term *term)
{
	for (size_t step=0; term && step<1024; ++step) {
		const struct pg_term *inner,*body;
		if ((inner=unary(term,&pg_total_result_operation)) || (inner=unary(term,&pg_return_operation))) { term=inner; continue; }
		if ((inner=unary(term,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation))) { term=body; continue; }
		return term;
	}
	return NULL;
}

/* Static application/carrier projection, identical in extent to C41/C57. */
static int selected_lambda(struct pg_graph *graph, const struct pg_term *term, struct pg_c_indexed_operand *out)
{
	struct pg_c_indexed_operand at={term,NULL},stack[16]; size_t count=0;
	for (size_t step=0; step<1024 && at.term; ++step) {
		at=pg_c_indexed_bound(at); at.term=carrier(at.term);
		if (!at.term) return -1;
		const struct pg_term *t=at.term;
		if (t->kind==PG_APPLICATION) {
			if (count==16) return -1;
			stack[count++]=(struct pg_c_indexed_operand){t->as.application.argument,at.environment};
			at.term=t->as.application.function; continue;
		}
		if (t->kind!=PG_LAMBDA) return -1;
		if (!count) { *out=at; return 0; }
		struct pg_c_indexed_binding *binding=pg_alloc(graph,sizeof(*binding));
		if (!binding) return -1;
		*binding=(struct pg_c_indexed_binding){at.environment,t->as.lambda.binder,stack[--count]};
		at=(struct pg_c_indexed_operand){t->as.lambda.body,binding};
	}
	return -1;
}

static int natural(struct pg_graph *graph, struct pg_c_indexed_operand at,
	const struct pg_c_indexed_entry *entries)
{
	struct pg_c_indexed_operand head,args[16]; size_t count;
	return !pg_c_indexed_head(graph,at,&head,&count,args) && !count
		&& head.term->as.reference==pg_data_declaration_family(entries[1].view.declaration);
}

static int list(struct pg_graph *graph, const struct pg_term *term,
	const struct pg_c_indexed_entry *entries)
{
	struct pg_c_indexed_operand head,args[16]; size_t count;
	return !pg_c_indexed_head(graph,(struct pg_c_indexed_operand){term,NULL},&head,&count,args) && count==1
		&& head.term->as.reference==pg_data_declaration_family(entries[7].view.declaration)
		&& natural(graph,args[0],entries);
}

static int closed_signature(struct pg_graph *graph, const struct pg_term *classifier,
	const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *domain,*codomain,*result; const struct pg_object *binder; enum pg_totality totality;
	return pg_pi_view(classifier,&domain,&binder,&codomain) && list(graph,domain,entries)
		&& pg_pure_computation_type_view(codomain,&totality,&result)
		&& totality==PG_TOTALITY_TOTAL && list(graph,result,entries);
}

struct capture { int boolean; size_t tag; int order[2]; };

static int call_operands(struct pg_graph *graph, const struct pg_term *body,
	const struct pg_object *a, const struct pg_object *b,
	const struct pg_term *reference, int *reverse)
{
	body=carrier(body);
	const struct pg_term *args[16],*head=body; size_t count=0;
	if (!body) return -1;
	while (head->kind==PG_APPLICATION) {
		if (count==16) return -1;
		args[count++]=head->as.application.argument; head=head->as.application.function;
	}
	for (size_t i=0; i<count/2; ++i) { const struct pg_term *t=args[i];args[i]=args[count-i-1];args[count-i-1]=t; }
	const struct pg_term *first,*second,*captured;
	if (head->kind==PG_REFERENCE && head->as.reference==&pg_force_operation && count==3) {
		captured=unary(args[0],&pg_thunk_operation); first=args[1]; second=args[2];
	} else if (count==2) { captured=carrier(head); first=args[0]; second=args[1]; }
	else return -1;
	struct pg_c_indexed_operand callee;
	if (!captured || selected_lambda(graph,captured,&callee) || callee.environment
		|| pg_alpha_equal(callee.term,reference)!=1) return -1;
	first=carrier(first);second=carrier(second);
	if (!first || !second || first->kind!=PG_REFERENCE || second->kind!=PG_REFERENCE) return -1;
	if (first->as.reference==a && second->as.reference==b) { *reverse=0; return 0; }
	if (first->as.reference==b && second->as.reference==a) { *reverse=1; return 0; }
	return -1;
}

static int operands(struct pg_graph *graph, const struct pg_term *captured,
	const struct pg_term *known, const struct pg_c_indexed_entry *entries, struct capture *out)
{
	struct pg_c_indexed_operand function,reference;
	if (selected_lambda(graph,captured,&function)
		|| selected_lambda(graph,known,&reference) || reference.environment) return -1;
	if (!function.environment && pg_alpha_equal(function.term,reference.term)==1) { out->order[0]=0; return 0; }
	const struct pg_term *left=function.term,*right=carrier(left->as.lambda.body);
	if (!right || right->kind!=PG_LAMBDA) return -1;
	const struct pg_object *a=left->as.lambda.binder,*b=right->as.lambda.binder;
	const struct pg_term *body=carrier(right->as.lambda.body);
	if (!function.environment) return call_operands(graph,body,a,b,reference.term,&out->order[0]);
	/* One closed source Bool field, with both original matcher clauses retained.
		* Constructor position is source data, not a machine truth convention. */
	if (function.environment->parent) return -1;
	const struct pg_data_layout *layout=pg_data_declaration_layout(entries[0].view.declaration);
	if (pg_data_layout_count(layout)!=2) return -1;
	for (size_t i=0; i<2; ++i) {
		const struct pg_data_layout *found; size_t position,arity;
		if (!pg_data_constructor_view(pg_data_constructor(layout,i),&found,&position,&arity)
			|| found!=layout || position!=i || arity) return -1;
	}
	struct pg_c_indexed_operand value=pg_c_indexed_bound(function.environment->value);
	value.term=carrier(value.term);
	if (!value.term || value.term->kind!=PG_REFERENCE
		|| !pg_data_constructor_position(layout,value.term->as.reference,&out->tag)) return -1;
	const struct pg_term *args[3],*head=body; size_t count=0;
	if (!head) return -1;
	while (head->kind==PG_APPLICATION) {
		if (count==3) return -1;
		args[count++]=head->as.application.argument; head=head->as.application.function;
	}
	if (count!=3 || head->kind!=PG_REFERENCE || head->as.reference!=pg_data_matcher(layout)) return -1;
	const struct pg_term *scrutinee=carrier(args[2]);
	if (!scrutinee || scrutinee->kind!=PG_REFERENCE || scrutinee->as.reference!=function.environment->binder
		|| call_operands(graph,args[1],a,b,reference.term,&out->order[0])
		|| call_operands(graph,args[0],a,b,reference.term,&out->order[1])) return -1;
	out->boolean=1; return 0;
}

int pg_c_acc_closed_capture_emit(FILE *out, struct pg_graph *graph,
	const struct pg_occurrence *closed, const struct pg_occurrence *parameter,
	const struct pg_occurrence *comparison, const struct pg_c_indexed_entry *entries)
{
	if (!out || !graph || !closed || !parameter || !comparison || !entries
		|| !closed_signature(graph,closed->classifier,entries)) return -1;
	struct pg_c_indexed_operand entry,open;
	if (selected_lambda(graph,closed->core,&entry) || selected_lambda(graph,parameter->core,&open)
		|| !open.environment || open.environment->parent || !natural(graph,open.environment->value,entries)
		|| pg_alpha_equal(entry.term,open.term->as.lambda.body)!=1) return -1;
	const struct pg_c_indexed_binding *le=entry.environment,*type=le ? le->parent : NULL;
	if (!le || le->binder!=open.term->as.lambda.binder || !type || type->parent
		|| type->binder!=open.environment->binder || !natural(graph,type->value,entries)) return -1;
	struct pg_c_indexed_operand argument=pg_c_indexed_bound(le->value);
	const struct pg_term *captured=unary(argument.term,&pg_thunk_operation); struct capture selected={0};
	if (!captured || operands(graph,captured,comparison->core,entries,&selected)) return -1;
	if (selected.boolean) {
		fprintf(out,"/* Generated actual closed Bool capture and source matcher clauses.\n"
			"\t* Constructor positions and both Nat operand orders come from admitted views. */\n"
			"struct gclosed_context { unsigned boolean_tag; };\n"
			"static const struct gclosed_context gclosed_capture = {%zu};\n"
			"static int gclosed_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
			"{\n\tconst struct gclosed_context *capture=context;\n"
			"\tif (!index_check(a,capture && capture->boolean_tag<2)) return 0;\n"
			"\tif (capture->boolean_tag==0) return gc_compare(a,NULL,%s,%s);\n"
			"\treturn gc_compare(a,NULL,%s,%s);\n}\n",selected.tag,
			selected.order[0] ? "right" : "left",selected.order[0] ? "left" : "right",
			selected.order[1] ? "right" : "left",selected.order[1] ? "left" : "right");
	} else fprintf(out,"/* Generated closed comparator capture: actual admitted known Nat comparison.\n"
		"\t* Operand order follows the source lambda; Acc/partition bodies are unchanged. */\n"
		"static int gclosed_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
		"{\n\t(void)context; return gc_compare(a,NULL,%s,%s);\n}\n",selected.order[0] ? "right" : "left",selected.order[0] ? "left" : "right");
	return ferror(out) ? -1 : 0;
}
