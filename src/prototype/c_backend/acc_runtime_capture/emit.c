#include "emit.h"

/* Reuse the unchanged closed entry/carrier/operand readers. C59's public
	* function and refusal contract remain intact; this entry is separate. */
#include "../acc_closed_capture/emit.c"

static int runtime_signature(struct pg_graph *graph, const struct pg_term *classifier,
	const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *domain,*codomain,*result; const struct pg_object *binder;
	struct pg_c_indexed_operand head,args[16]; size_t count; enum pg_totality totality;
	if (!pg_pi_view(classifier,&domain,&binder,&codomain)
		|| pg_c_indexed_head(graph,(struct pg_c_indexed_operand){domain,NULL},&head,&count,args)
		|| count || head.term->as.reference!=pg_data_declaration_family(entries[0].view.declaration)) return 0;
	if (pg_pure_computation_type_view(codomain,&totality,&result)) {
		if (totality!=PG_TOTALITY_TOTAL) return 0;
		codomain=result;
	}
	return closed_signature(graph,codomain,entries);
}

static int runtime_operands(struct pg_graph *graph, const struct pg_term *captured,
	const struct pg_term *known, const struct pg_object *parameter,
	const struct pg_c_indexed_entry *entries, int order[2])
{
	struct pg_c_indexed_operand function,reference;
	if (selected_lambda(graph,captured,&function) || !function.environment || function.environment->parent
		|| selected_lambda(graph,known,&reference) || reference.environment) return -1;
	const struct pg_term *left=function.term,*right=carrier(left->as.lambda.body);
	if (!right || right->kind!=PG_LAMBDA) return -1;
	const struct pg_object *a=left->as.lambda.binder,*b=right->as.lambda.binder;
	const struct pg_data_layout *layout=pg_data_declaration_layout(entries[0].view.declaration);
	if (pg_data_layout_count(layout)!=2) return -1;
	for (size_t i=0; i<2; ++i) {
		const struct pg_data_layout *found; size_t position,arity;
		if (!pg_data_constructor_view(pg_data_constructor(layout,i),&found,&position,&arity)
			|| found!=layout || position!=i || arity) return -1;
	}
	struct pg_c_indexed_operand value=pg_c_indexed_bound(function.environment->value);
	value.term=carrier(value.term);
	if (!value.term || value.term->kind!=PG_REFERENCE || value.term->as.reference!=parameter) return -1;
	const struct pg_term *args[3],*head=carrier(right->as.lambda.body); size_t count=0;
	if (!head) return -1;
	while (head->kind==PG_APPLICATION) {
		if (count==3) return -1;
		args[count++]=head->as.application.argument; head=head->as.application.function;
	}
	if (count!=3 || head->kind!=PG_REFERENCE || head->as.reference!=pg_data_matcher(layout)) return -1;
	const struct pg_term *scrutinee=carrier(args[2]);
	if (!scrutinee || scrutinee->kind!=PG_REFERENCE || scrutinee->as.reference!=function.environment->binder
		|| call_operands(graph,args[1],a,b,reference.term,&order[0])
		|| call_operands(graph,args[0],a,b,reference.term,&order[1])) return -1;
	return 0;
}

int pg_c_acc_runtime_capture_emit(FILE *out, struct pg_graph *graph,
	const struct pg_occurrence *runtime, const struct pg_occurrence *parameter,
	const struct pg_occurrence *comparison, const struct pg_c_indexed_entry *entries)
{
	if (!out || !graph || !runtime || !parameter || !comparison || !entries
		|| !runtime_signature(graph,runtime->classifier,entries)) return -1;
	struct pg_c_indexed_operand wrapper,entry,open;
	if (selected_lambda(graph,runtime->core,&wrapper) || wrapper.environment
		|| selected_lambda(graph,wrapper.term->as.lambda.body,&entry)
		|| selected_lambda(graph,parameter->core,&open) || !open.environment || open.environment->parent
		|| !natural(graph,open.environment->value,entries)
		|| pg_alpha_equal(entry.term,open.term->as.lambda.body)!=1) return -1;
	const struct pg_c_indexed_binding *le=entry.environment,*type=le ? le->parent : NULL;
	if (!le || le->binder!=open.term->as.lambda.binder || !type || type->parent
		|| type->binder!=open.environment->binder || !natural(graph,type->value,entries)) return -1;
	struct pg_c_indexed_operand argument=pg_c_indexed_bound(le->value);
	const struct pg_term *captured=unary(argument.term,&pg_thunk_operation); int order[2];
	if (!captured || runtime_operands(graph,captured,comparison->core,wrapper.term->as.lambda.binder,entries,order)) return -1;
	fprintf(out,"/* Generated actual runtime source Bool capture and both matcher clauses.\n"
		"\t* The field comes from the admitted outer parameter, not a foreign callback. */\n"
		"struct gruntime_context { unsigned boolean_tag; };\n"
		"static int gruntime_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
		"{\n\tconst struct gruntime_context *capture=context;\n"
		"\tif (!index_check(a,capture && capture->boolean_tag<2)) return 0;\n"
		"\tif (capture->boolean_tag==0) return gc_compare(a,NULL,%s,%s);\n"
		"\treturn gc_compare(a,NULL,%s,%s);\n}\n",order[0] ? "right" : "left",order[0] ? "left" : "right",
		order[1] ? "right" : "left",order[1] ? "left" : "right");
	return ferror(out) ? -1 : 0;
}
