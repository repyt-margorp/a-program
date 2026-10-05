#include "emit.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_TYPE, VALUE_NAT, VALUE_LIST, VALUE_SIZED, VALUE_MEASURED, VALUE_IH, VALUE_ACCESS, VALUE_COMPARE };
struct value { enum value_kind kind; char name[128]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct argument { const struct pg_term *term; const struct binding *env; struct value value; };
struct emitter {
	FILE *out;
	struct pg_graph *storage;
	struct pg_dag *dag;
	const struct pg_c_indexed_entry *entries;
	const struct pg_data_layout *layouts[8];
	const struct pg_term *callees[4];
	size_t calls[3], serial, steps;
	unsigned indent;
};

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *op)
{
	if (!t || t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==op ? t->as.application.argument : NULL;
}
static int child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s=key;
	if (slot<s->operand_count) { *out=s->operands[slot]; return *out ? 1 : 2; }
	slot-=s->operand_count;
	if (!slot) { *out=s->type; return *out ? 1 : 2; }
	if (slot==1) { *out=s->origin; return *out ? 1 : 2; }
	slot-=2; const struct pg_context_map *const *maps=pg_occurrence_maps(s);
	for (size_t i=0; i<=s->map_count; ++i) {
		const struct pg_context_map *m=i ? maps[i-1] : s->map; if (!m) continue;
		if (slot<m->count) { *out=m->images[slot]; return *out ? 1 : 2; }
		slot-=m->count;
	}
	return 0;
}
static int selected_lambda(struct pg_graph *g, const struct pg_term *term, struct pg_c_indexed_operand *out)
{
	struct pg_c_indexed_operand at={term,NULL}, stack[16]; size_t count=0;
	for (size_t step=0; step<1024 && at.term; ++step) {
		at=pg_c_indexed_bound(at); const struct pg_term *t=at.term,*inner,*body;
		if ((inner=unary(t,&pg_total_result_operation)) || (inner=unary(t,&pg_return_operation))) { at.term=inner; continue; }
		if ((inner=unary(t,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation))) { at.term=body; continue; }
		if (t->kind==PG_APPLICATION) {
			if (count==16) return -1;
			stack[count++]=(struct pg_c_indexed_operand){t->as.application.argument,at.environment}; at.term=t->as.application.function; continue;
		}
		if (t->kind!=PG_LAMBDA) return -1;
		if (!count) { *out=at; return 0; }
		struct pg_c_indexed_binding *b=pg_alloc(g,sizeof(*b)); if (!b) return -1;
		*b=(struct pg_c_indexed_binding){at.environment,t->as.lambda.binder,stack[--count]}; at=(struct pg_c_indexed_operand){t->as.lambda.body,b};
	}
	return -1;
}
static int family(struct emitter *e, struct pg_c_indexed_operand type, size_t f, size_t wanted, struct pg_c_indexed_operand *args)
{
	struct pg_c_indexed_operand head; size_t count;
	return !pg_c_indexed_head(e->storage,type,&head,&count,args) && count==wanted
		&& head.term->as.reference==pg_data_declaration_family(e->entries[f].view.declaration);
}
static int natural(struct emitter *e, struct pg_c_indexed_operand type)
{
	struct pg_c_indexed_operand a[16]; return family(e,type,1,0,a);
}
static int aggregate(struct emitter *e, struct pg_c_indexed_operand type, size_t f)
{
	struct pg_c_indexed_operand a[16]; return family(e,type,f,1,a) && natural(e,a[0]);
}
static int total(const struct pg_term *type, const struct pg_term **result)
{
	enum pg_totality t; return pg_pure_computation_type_view(type,&t,result) && t==PG_TOTALITY_TOTAL;
}
static struct value named(enum value_kind kind, const char *name)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}
static void indent(struct emitter *e) { for (unsigned i=0; i<e->indent; ++i) fputc('\t',e->out); }
static void guard(struct emitter *e, struct value v)
{
	indent(e); fprintf(e->out,"if (!%s%s || a->status) goto done;\n",v.name,v.kind==VALUE_MEASURED ? ".values" : "");
}
static int expression(struct emitter *, const struct pg_term *, const struct binding *, size_t, const struct argument *, unsigned, struct value *);
static int value(struct emitter *e, struct argument a, unsigned depth, struct value *v)
{
	if (!a.term) { *v=a.value; return 0; }
	return expression(e,a.term,a.env,0,NULL,depth+1,v);
}
static int apply(struct emitter *e, const struct pg_term *t, const struct binding *env,
	size_t n, const struct argument *args, unsigned depth, struct value *out)
{
	if (!n || t->kind!=PG_LAMBDA) return expression(e,t,env,n,args,depth+1,out);
	struct value v; if (value(e,args[0],depth+1,&v)) return -1;
	struct binding b={env,t->as.lambda.binder,v}; return apply(e,t->as.lambda.body,&b,n-1,args+1,depth+1,out);
}
static int metadata_supported(struct emitter *e, struct pg_c_indexed_operand at, const struct binding *env, enum value_kind wanted, unsigned depth)
{
	if (depth>128) return 0;
	at=pg_c_indexed_bound(at); const struct pg_term *t=at.term; if (!t) return 0;
	if (t->kind==PG_REFERENCE) {
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==t->as.reference) return b->value.kind==wanted;
		return wanted==VALUE_TYPE ? t->as.reference==pg_data_declaration_family(e->entries[1].view.declaration)
			: t->as.reference==pg_data_constructor(e->layouts[1],0);
	}
	return wanted==VALUE_NAT && t->kind==PG_APPLICATION && t->as.application.function->kind==PG_REFERENCE
		&& t->as.application.function->as.reference==pg_data_constructor(e->layouts[1],1)
		&& metadata_supported(e,(struct pg_c_indexed_operand){t->as.application.argument,at.environment},env,wanted,depth+1);
}
static int metadata_value(struct emitter *e, struct pg_c_indexed_operand at, const struct binding *env, enum value_kind wanted, struct value *out)
{
	at=pg_c_indexed_bound(at); const struct pg_term *t=at.term;
	if (t->kind==PG_REFERENCE) {
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==t->as.reference) { *out=b->value; return out->kind==wanted ? 0 : -1; }
		*out=named(wanted,wanted==VALUE_TYPE ? "&nat_type" : "0"); return 0;
	}
	struct value prior;
	if (metadata_value(e,(struct pg_c_indexed_operand){t->as.application.argument,at.environment},env,VALUE_NAT,&prior)) return -1;
	*out=temporary(e,VALUE_NAT); indent(e); fprintf(e->out,"uint32_t %s = succ(a,%s);\n",out->name,prior.name); return 0;
}
/* Read retained classifier parameters for this exact constructor term. No
	* expected-result fallback, synthesized annotation or source coercion. */
static int parameters(struct emitter *e, const struct pg_term *term, const struct binding *env, size_t f,
	struct value *type, struct value *index)
{
	struct pg_c_indexed_operand chosen[2]; const struct pg_term *classifier=NULL; size_t wanted=f==4 ? 2 : 1;
	for (const struct pg_dag_node *n=e->dag->first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (s->core!=term) continue;
		struct pg_c_indexed_operand a[16];
		if (!family(e,(struct pg_c_indexed_operand){s->classifier,NULL},f,wanted,a)
			|| !metadata_supported(e,a[0],env,VALUE_TYPE,0) || (wanted==2 && !metadata_supported(e,a[1],env,VALUE_NAT,0))) continue;
		if (classifier && pg_alpha_equal(classifier,s->classifier)!=1) return -1;
		classifier=s->classifier; chosen[0]=a[0]; if (wanted==2) chosen[1]=a[1];
	}
	return !classifier || metadata_value(e,chosen[0],env,VALUE_TYPE,type)
		|| (wanted==2 && metadata_value(e,chosen[1],env,VALUE_NAT,index)) ? -1 : 0;
}
static int constructor(struct emitter *e, const struct pg_term *term, size_t f, size_t position,
	size_t n, const struct argument *a, const struct binding *env, unsigned depth, struct value *out)
{
	const struct pg_data_layout *layout; size_t actual,arity;
	if (!pg_data_constructor_view(pg_data_constructor(e->layouts[f],position),&layout,&actual,&arity) || n!=arity || n>3) return -1;
	struct value v[3]; for (size_t i=0; i<n; ++i) if (value(e,a[i],depth+1,&v[i])) return -1;
	if (f==1 && position<2) {
		if (!position && !n) { *out=named(VALUE_NAT,"0"); return 0; }
		if (n!=1 || v[0].kind!=VALUE_NAT) return -1;
		*out=temporary(e,VALUE_NAT); indent(e); fprintf(e->out,"uint32_t %s = succ(a,%s);\n",out->name,v[0].name);
		indent(e); fputs("if (a->status) goto done;\n",e->out); return 0;
	}
	struct value type,index;
	if ((f!=4 && f!=5) || parameters(e,term,env,f,&type,&index)) return -1;
	if (f==4 && position<2) {
		if (n!=(position ? 3u : 0u) || (position && (v[0].kind!=VALUE_NAT || v[1].kind!=VALUE_NAT || v[2].kind!=VALUE_SIZED))) return -1;
		*out=temporary(e,VALUE_SIZED); indent(e); fprintf(e->out,"const struct qs_sized_list *%s = %s(a,%s",out->name,position ? "sized_cons" : "sized_nil",type.name);
		if (position) fprintf(e->out,",%s,%s,%s",v[0].name,v[1].name,v[2].name);
		fputs(");\n",e->out); guard(e,*out); indent(e);
		fprintf(e->out,"if (!index_check(a,%s->index == %s)) goto done;\n",out->name,index.name); return 0;
	}
	if (f!=5 || position || n!=2 || v[0].kind!=VALUE_NAT || v[1].kind!=VALUE_SIZED) return -1;
	guard(e,v[1]); indent(e); fprintf(e->out,"if (!index_check(a,%s->element_type == %s && %s->index == %s)) goto done;\n",v[1].name,type.name,v[1].name,v[0].name);
	*out=temporary(e,VALUE_MEASURED); indent(e); fprintf(e->out,"struct qs_measured %s = {%s,%s};\n",out->name,v[0].name,v[1].name); return 0;
}
static int known_call(struct emitter *e, const struct pg_term *body, size_t n, const struct argument *a, unsigned depth, struct value *out)
{
	struct pg_c_indexed_operand selected;
	if (selected_lambda(e->storage,body,&selected) || selected.environment) return -1;
	body=selected.term;
	size_t which;
	for (which=0; which<3; ++which) if (pg_alpha_equal(body,e->callees[which])==1) break;
	const size_t arities[]={2,1,5}; if (which==3 || n!=arities[which]) return -1;
	struct value v[5]; for (size_t i=0; i<n; ++i) if (value(e,a[i],depth+1,&v[i])) return -1;
	if (which==0) {
		if (v[0].kind!=VALUE_TYPE || v[1].kind!=VALUE_LIST) return -1;
		*out=temporary(e,VALUE_MEASURED); indent(e); fprintf(e->out,"struct qs_measured %s = gm_fold(a,%s,%s);\n",out->name,v[0].name,v[1].name); guard(e,*out);
	} else if (which==1) {
		if (v[0].kind!=VALUE_NAT) return -1;
		*out=temporary(e,VALUE_ACCESS); indent(e); fprintf(e->out,"const struct qs_acc *%s = gn_nat_accessible(a,%s);\n",out->name,v[0].name); guard(e,*out);
	} else {
		if (v[0].kind!=VALUE_TYPE || v[1].kind!=VALUE_COMPARE || v[2].kind!=VALUE_NAT || v[3].kind!=VALUE_ACCESS || v[4].kind!=VALUE_SIZED) return -1;
		size_t serial=e->serial++; *out=temporary(e,VALUE_LIST);
		indent(e); fprintf(e->out,"struct qs_sort_closure call%zu = gs_fold(%s,%s,%s,%s);\n",serial,v[0].name,v[1].name,v[2].name,v[3].name);
		indent(e); fprintf(e->out,"const struct qs_list *%s = gs_apply(a,&call%zu,%s);\n",out->name,serial,v[4].name); guard(e,*out);
	}
	++e->calls[which]; return 0;
}
static int expression(struct emitter *e, const struct pg_term *term, const struct binding *env,
	size_t pending, const struct argument *suffix, unsigned depth, struct value *out)
{
	if (!term || depth>512 || ++e->steps>8192 || pending>16) return -1;
	const struct pg_term *inner=unary(term,&pg_return_operation);
	if (inner) return expression(e,inner,env,pending,suffix,depth+1,out);
	const struct pg_term *head=term; struct argument args[16]; size_t count=0;
	while (head->kind==PG_APPLICATION) {
		if (count+pending==16) return -1;
		args[count++]=(struct argument){.term=head->as.application.argument,.env=env}; head=head->as.application.function;
	}
	for (size_t i=0; i<count/2; ++i) { struct argument a=args[i]; args[i]=args[count-i-1]; args[count-i-1]=a; }
	for (size_t i=0; i<pending; ++i) args[count+i]=suffix[i];
	count+=pending;
	if (head->kind==PG_LAMBDA) return count ? apply(e,head,env,count,args,depth+1,out) : -1;
	if (head->kind!=PG_REFERENCE) return -1;
	const struct pg_object *object=head->as.reference;
	if (object->kind==PG_BINDER) {
		if (count) return -1;
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==object) { *out=b->value; return 0; }
		return -1;
	}
	if (object==&pg_fold_operation) {
		if (count<2 || !args[1].term || args[1].term->kind!=PG_LAMBDA) return -1;
		struct value first; if (value(e,args[0],depth+1,&first)) return -1;
		struct binding b={args[1].env,args[1].term->as.lambda.binder,first};
		return expression(e,args[1].term->as.lambda.body,&b,count-2,args+2,depth+1,out);
	}
	if (object==&pg_force_operation) {
		if (!count || !args[0].term) return -1;
		const struct pg_term *body=unary(args[0].term,&pg_thunk_operation);
		if (body) return known_call(e,body,count-1,args+1,depth+1,out);
		struct value ih; if (count!=1 || value(e,args[0],depth+1,&ih) || ih.kind!=VALUE_IH) return -1;
		*out=temporary(e,VALUE_MEASURED); indent(e); fprintf(e->out,"struct qs_measured %s = gm_fold(a,type,%s);\n",out->name,ih.name); guard(e,*out); return 0;
	}
	if (object==pg_data_matcher(e->layouts[5])) {
		struct value m; if (count!=2 || value(e,args[0],depth+1,&m) || m.kind!=VALUE_MEASURED) return -1;
		guard(e,m); struct value n=temporary(e,VALUE_NAT),values=temporary(e,VALUE_SIZED);
		indent(e); fprintf(e->out,"uint32_t %s = %s.size;\n",n.name,m.name);
		indent(e); fprintf(e->out,"const struct qs_sized_list *%s = %s.values;\n",values.name,m.name);
		struct argument fields[2]={{.value=n},{.value=values}};
		return args[1].term ? apply(e,args[1].term,args[1].env,2,fields,depth+1,out) : -1;
	}
	for (size_t f=0; f<8; ++f) {
		size_t position; if (pg_data_constructor_position(e->layouts[f],object,&position)) return constructor(e,term,f,position,count,args,env,depth+1,out);
	}
	return -1;
}
static int classifier_signature(struct emitter *e, const struct pg_term *classifier, size_t result_family)
{
	const struct pg_term *domain,*codomain,*result; const struct pg_object *binder;
	return pg_pi_view(classifier,&domain,&binder,&codomain) && aggregate(e,(struct pg_c_indexed_operand){domain,NULL},7)
		&& total(codomain,&result) && aggregate(e,(struct pg_c_indexed_operand){result,NULL},result_family);
}
static int emit_measure(struct emitter *e, const struct pg_typing *typing, const struct pg_occurrence *root)
{
	struct pg_c_indexed_operand selected; if (!classifier_signature(e,root->classifier,5) || selected_lambda(e->storage,root->core,&selected)) return -1;
	const struct pg_occurrence *fold=NULL; struct pg_elimination_inputs chosen={0};
	for (const struct pg_dag_node *n=e->dag->first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures; if (!s->induction || s->induction->count!=2
			|| !s->context || s->context->binder!=selected.term->as.lambda.binder || pg_context_extension_size(s->context,NULL,&captures) || captures!=2) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof || view.count!=2) continue;
		const struct pg_term *scrutinee=pg_evidence_subject(view.scrutinee)->core;
		if (scrutinee->kind!=PG_REFERENCE || scrutinee->as.reference!=s->context->binder) continue;
		if (fold) return -1;
		fold=s; chosen=view;
	}
	const struct pg_term *result,*ih_type; if (!fold || !total(fold->classifier,&result)
		|| !aggregate(e,(struct pg_c_indexed_operand){result,selected.environment},5)
		|| !aggregate(e,(struct pg_c_indexed_operand){pg_evidence_classifier(chosen.scrutinee),selected.environment},7)) return -1;
	const struct pg_context *element=fold->context->parent,*ih=fold->induction->clauses[1],*tail=ih->parent,*head=tail ? tail->parent : NULL;
	uint64_t level; struct pg_term type_ref={.kind=PG_REFERENCE,.as.reference=element->binder};
	if (element->parent || !pg_universe_level(element->declared_type,&level) || level || !natural(e,(struct pg_c_indexed_operand){&type_ref,selected.environment})
		|| !head || head->parent!=fold->context || !natural(e,(struct pg_c_indexed_operand){head->declared_type,selected.environment})
		|| !aggregate(e,(struct pg_c_indexed_operand){tail->declared_type,selected.environment},7)
		|| !pg_thunk_type_view(ih->declared_type,&ih_type) || !total(ih_type,&result)
		|| !aggregate(e,(struct pg_c_indexed_operand){result,selected.environment},5)) return -1;
	fputs("/* Generated actual measure List Fold, Measured Match and indexed constructors. */\n"
		"static struct qs_measured gm_fold(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *input)\n{\n"
		"\tstruct qs_measured result = {0}; if (!enter(a)) return result;\n"
		"\tif (!index_check(a,input && input->element_type == type && type == &nat_type)) goto done;\n",e->out);
	struct binding env[]={{NULL,element->binder,named(VALUE_TYPE,"type")},{NULL,fold->context->binder,named(VALUE_LIST,"input")}}; env[1].parent=&env[0];
	for (size_t position=0; position<2; ++position) {
		size_t suffix; if (pg_context_extension_size(fold->induction->clauses[position],fold->context,&suffix) || suffix!=(position ? 3u : 0u)) return -1;
		indent(e); fprintf(e->out,"%sif (input->tag == %s) {\n",position ? "} else " : "",position ? "QS_CONS" : "QS_NIL"); ++e->indent;
		struct argument fields[3]={{.value=named(VALUE_NAT,"input->head")},{.value=named(VALUE_LIST,"input->tail")},{.value=named(VALUE_IH,"input->tail")}};
		struct value body; if (apply(e,fold->operands[position+1]->core,&env[1],suffix,fields,0,&body) || body.kind!=VALUE_MEASURED) return -1;
		indent(e); fprintf(e->out,"result = %s;\n",body.name); --e->indent;
	}
	indent(e); fputs("} else { a->status = 2; }\ndone:\n\t--a->depth; return result;\n}\n",e->out); return 0;
}
static int comparator_signature(struct emitter *e, const struct pg_term *classifier,
	const struct pg_term **list_function)
{
	const struct pg_term *domain,*codomain,*function,*left,*right,*result;
	const struct pg_object *binder;
	struct pg_c_indexed_operand arguments[16];
	if (!pg_pi_view(classifier,&domain,&binder,list_function)
		|| !pg_thunk_type_view(domain,&function)
		|| !pg_pi_view(function,&left,&binder,&codomain)
		|| !natural(e,(struct pg_c_indexed_operand){left,NULL})
		|| !pg_pi_view(codomain,&right,&binder,&codomain)
		|| !natural(e,(struct pg_c_indexed_operand){right,NULL})
		|| !total(codomain,&result)
		|| !family(e,(struct pg_c_indexed_operand){result,NULL},0,0,arguments)) return 0;
	return classifier_signature(e,*list_function,7);
}
static int emit_outer(struct emitter *e, const struct pg_c_acc_measure_sources *sources, int parameter)
{
	struct pg_c_indexed_operand selected;
	if (selected_lambda(e->storage,sources->outer->core,&selected)) return -1;
	const struct pg_c_indexed_binding *type;
	const struct pg_object *comparison_binder;
	const struct pg_term *input;
	if (parameter) {
		const struct pg_term *list_function;
		if (!comparator_signature(e,sources->outer->classifier,&list_function)) return -1;
		type=selected.environment;
		comparison_binder=selected.term->as.lambda.binder;
		input=selected.term->as.lambda.body;
		if (!input || input->kind!=PG_LAMBDA) return -1;
	} else {
		if (!classifier_signature(e,sources->outer->classifier,7)) return -1;
		const struct pg_c_indexed_binding *le=selected.environment;
		type=le ? le->parent : NULL;
		if (!le) return -1;
		struct pg_c_indexed_operand argument=pg_c_indexed_bound(le->value);
		const struct pg_term *comparison=unary(argument.term,&pg_thunk_operation);
		struct pg_c_indexed_operand compare_lambda;
		if (!comparison || selected_lambda(e->storage,comparison,&compare_lambda) || compare_lambda.environment
			|| pg_alpha_equal(compare_lambda.term,e->callees[3])!=1) return -1;
		comparison_binder=le->binder;
		input=selected.term;
	}
	if (!type || type->parent || !natural(e,type->value)) return -1;
	struct binding env[]={{NULL,type->binder,named(VALUE_TYPE,"type")},{NULL,comparison_binder,named(VALUE_COMPARE,"le")},
		{NULL,input->as.lambda.binder,named(VALUE_LIST,"input")}}; env[1].parent=&env[0]; env[2].parent=&env[1];
	fputs("/* Generated actual quickSort measure/Match/accessibility/Acc application.\n\t* C33 storage/LT/successor-down transport and array copy-out remain manual. */\n"
		"static const struct qs_list *go_outer(struct qs_arena *a, const struct qs_nat_type *type, struct qs_compare le, const struct qs_list *input)\n{\n"
		"\tconst struct qs_list *result = NULL; if (!enter(a)) return NULL;\n"
		"\tif (!index_check(a,type == &nat_type && le.call && input && input->element_type == type)) goto done;\n",e->out);
	struct value body;
	if (expression(e,input->as.lambda.body,&env[2],0,NULL,0,&body) || body.kind!=VALUE_LIST
		|| e->calls[0]!=1 || e->calls[1]!=1 || e->calls[2]!=1) return -1;
	indent(e); fprintf(e->out,"result = %s;\ndone:\n\t--a->depth; return result;\n}\n",body.name); return 0;
}
static int emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_c_acc_measure_sources *sources, const struct pg_c_indexed_entry *entries, int parameter)
{
	if (!out || !storage || !typing || !sources || !entries || !sources->measure || !sources->measure_reference
		|| !sources->outer || !sources->outer_reference || !sources->measure_generic || !sources->accessibility || !sources->sort_generic || !sources->comparison) return -1;
	if (pg_alpha_equal(sources->measure->core,sources->measure_reference->core)!=1 || pg_alpha_equal(sources->outer->core,sources->outer_reference->core)!=1) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1; struct pg_dag dag;
	if (pg_dag_init(&dag,child,NULL)) { fclose(stage); return -1; }
	struct emitter e={.out=stage,.storage=storage,.dag=&dag,.entries=entries,.indent=1}; int status=-1;
	for (size_t i=0; i<8; ++i) { if (!entries[i].view.declaration) goto done; e.layouts[i]=pg_data_declaration_layout(entries[i].view.declaration); }
	const size_t counts[8]={2,2,3,1,2,1,1,2};
	for (size_t i=0; i<8; ++i) if (pg_data_layout_count(e.layouts[i])!=counts[i]) goto done;
	const struct pg_occurrence *callees[]={sources->measure_generic,sources->accessibility,sources->sort_generic,sources->comparison};
	for (size_t i=0; i<4; ++i) { struct pg_c_indexed_operand selected; if (selected_lambda(storage,callees[i]->core,&selected) || selected.environment) goto done; e.callees[i]=selected.term; }
	if (pg_dag_add(&dag,sources->measure) || pg_dag_add(&dag,sources->outer) || emit_measure(&e,typing,sources->measure)
		|| emit_outer(&e,sources,parameter) || ferror(stage) || fseek(stage,0,SEEK_SET)) goto done;
	status=0; char buffer[4096]; size_t n;
	while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
	if (ferror(stage) || ferror(out)) status=-1;
done:
	pg_dag_destroy(&dag); if (fclose(stage)) status=-1; return status;
}

int pg_c_acc_measure_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_c_acc_measure_sources *sources, const struct pg_c_indexed_entry *entries)
{
	return emit(out,storage,typing,sources,entries,0);
}

int pg_c_acc_measure_parameter_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_c_acc_measure_sources *sources, const struct pg_c_indexed_entry *entries)
{
	return emit(out,storage,typing,sources,entries,1);
}
