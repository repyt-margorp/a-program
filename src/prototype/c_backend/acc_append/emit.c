#include "emit.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_TYPE, VALUE_NAT, VALUE_LIST, VALUE_IH };
struct value { enum value_kind kind; char name[80]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct emitter { FILE *out; const struct pg_data_layout *list; size_t serial, steps; };

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *op)
{
	if (!t || t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==op ? t->as.application.argument : NULL;
}

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s=key;
	if (slot<s->operand_count) { *out=s->operands[slot]; return *out ? 1 : 2; }
	slot-=s->operand_count;
	if (!slot) { *out=s->type; return *out ? 1 : 2; }
	if (slot==1) { *out=s->origin; return *out ? 1 : 2; }
	slot-=2; const struct pg_context_map *const *maps=pg_occurrence_maps(s);
	for (size_t i=0; i<=s->map_count; ++i) {
		const struct pg_context_map *map=i ? maps[i-1] : s->map; if (!map) continue;
		if (slot<map->count) { *out=map->images[slot]; return *out ? 1 : 2; }
		slot-=map->count;
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

static int family(struct pg_graph *g, struct pg_c_indexed_operand type,
	const struct pg_c_indexed_entry *entries, size_t which, size_t wanted, struct pg_c_indexed_operand *a)
{
	struct pg_c_indexed_operand head; size_t count;
	return !pg_c_indexed_head(g,type,&head,&count,a) && count==wanted
		&& head.term->as.reference==pg_data_declaration_family(entries[which].view.declaration);
}

static int list(struct pg_graph *g, struct pg_c_indexed_operand type, const struct pg_c_indexed_entry *entries)
{
	struct pg_c_indexed_operand a[16],unused[16];
	return family(g,type,entries,7,1,a) && family(g,a[0],entries,1,0,unused);
}

static int function(struct pg_graph *g, const struct pg_term *type, const struct pg_c_indexed_binding *env,
	const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *domain,*codomain,*result; const struct pg_object *binder; enum pg_totality totality;
	return pg_pi_view(type,&domain,&binder,&codomain) && list(g,(struct pg_c_indexed_operand){domain,env},entries)
		&& pg_pure_computation_type_view(codomain,&totality,&result) && totality==PG_TOTALITY_TOTAL
		&& list(g,(struct pg_c_indexed_operand){result,env},entries);
}

static struct value named(enum value_kind kind, const char *name)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}
static void guard(struct emitter *e, struct value v)
{
	fprintf(e->out,"\t\tif (!%s || a->status) goto done;\n",v.name);
}

static int expression(struct emitter *, const struct pg_term *, const struct binding *, unsigned, struct value *);
static int apply(struct emitter *e, const struct pg_term *term, const struct binding *env,
	size_t count, const struct value *args, unsigned depth, struct value *out)
{
	if (!count) return expression(e,term,env,depth+1,out);
	if (term->kind!=PG_LAMBDA) return -1;
	struct binding b={env,term->as.lambda.binder,args[0]}; return apply(e,term->as.lambda.body,&b,count-1,args+1,depth+1,out);
}

static int expression(struct emitter *e, const struct pg_term *term, const struct binding *env, unsigned depth, struct value *out)
{
	if (!term || depth>128 || ++e->steps>4096) return -1;
	const struct pg_term *returned=unary(term,&pg_return_operation);
	if (returned) return expression(e,returned,env,depth+1,out);
	const struct pg_term *head=term,*args[16]; size_t count=0;
	while (head->kind==PG_APPLICATION) {
		if (count==16) return -1;
		args[count++]=head->as.application.argument; head=head->as.application.function;
	}
	for (size_t i=0; i<count/2; ++i) { const struct pg_term *t=args[i]; args[i]=args[count-i-1]; args[count-i-1]=t; }
	if (head->kind==PG_LAMBDA) {
		struct value values[16]; if (!count) return -1;
		for (size_t i=0; i<count; ++i) if (expression(e,args[i],env,depth+1,&values[i])) return -1;
		return apply(e,head,env,count,values,depth+1,out);
	}
	if (head->kind!=PG_REFERENCE) return -1;
	const struct pg_object *object=head->as.reference;
	if (object->kind==PG_BINDER) {
		if (count) return -1;
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==object) { *out=b->value; return 0; }
		return -1;
	}
	if (object==&pg_fold_operation) {
		if (count!=2 || args[1]->kind!=PG_LAMBDA) return -1;
		struct value first; if (expression(e,args[0],env,depth+1,&first)) return -1;
		struct binding b={env,args[1]->as.lambda.binder,first}; return expression(e,args[1]->as.lambda.body,&b,depth+1,out);
	}
	if (object==&pg_force_operation) {
		if (count!=2) return -1;
		struct value ih,right;
		if (expression(e,args[0],env,depth+1,&ih) || expression(e,args[1],env,depth+1,&right)
			|| ih.kind!=VALUE_IH || right.kind!=VALUE_LIST) return -1;
		size_t n=e->serial++; *out=temporary(e,VALUE_LIST);
		fprintf(e->out,"\t\tstruct ga_result child%zu = ga_force(a,&%s);\n\t\tif (a->status) goto done;\n",n,ih.name);
		fprintf(e->out,"\t\tconst struct qs_list *%s = ga_apply(a,&child%zu,%s);\n",out->name,n,right.name); guard(e,*out); return 0;
	}
	size_t position;
	if (!pg_data_constructor_position(e->list,object,&position) || position>1 || count!=(position ? 2u : 0u)) return -1;
	*out=temporary(e,VALUE_LIST);
	if (!position) fprintf(e->out,"\t\tconst struct qs_list *%s = list_nil(a,f->type);\n",out->name);
	else {
		struct value h,t;
		if (expression(e,args[0],env,depth+1,&h) || expression(e,args[1],env,depth+1,&t) || h.kind!=VALUE_NAT || t.kind!=VALUE_LIST) return -1;
		fprintf(e->out,"\t\tconst struct qs_list *%s = list_cons(a,f->type,%s,%s);\n",out->name,h.name,t.name);
	}
	guard(e,*out); return 0;
}

static int emit_to(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	struct pg_c_indexed_operand selected; if (selected_lambda(storage,root->core,&selected)) return -1;
	const struct pg_object *left_binder=selected.term->as.lambda.binder;
	const struct pg_term *domain,*codomain; const struct pg_object *binder;
	if (!pg_pi_view(root->classifier,&domain,&binder,&codomain) || !list(storage,(struct pg_c_indexed_operand){domain,NULL},entries)
		|| !function(storage,codomain,NULL,entries)) return -1;
	const struct pg_data_layout *layout=pg_data_declaration_layout(entries[7].view.declaration);
	if (pg_data_layout_count(layout)!=2) return -1;
	for (size_t i=0; i<2; ++i) {
		const struct pg_data_layout *actual; size_t position,arity;
		if (!pg_data_constructor_view(pg_data_constructor(layout,i),&actual,&position,&arity) || actual!=layout || position!=i || arity!=(i ? 2u : 0u)) return -1;
	}
	struct pg_dag dag; if (pg_dag_init(&dag,subject_child,NULL)) return -1;
	int status=-1; const struct pg_occurrence *fold=NULL; struct pg_elimination_inputs chosen={0};
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures;
		if (!s->induction || s->induction->count!=2 || pg_context_extension_size(s->context,NULL,&captures) || captures!=2) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof) continue;
		if (fold) goto done;
		fold=s; chosen=view;
	}
	if (!fold || chosen.count!=2 || fold->context->binder!=left_binder || !function(storage,fold->classifier,selected.environment,entries)) goto done;
	const struct pg_occurrence *scrutinee=pg_evidence_subject(chosen.scrutinee);
	if (scrutinee->core->kind!=PG_REFERENCE || scrutinee->core->as.reference!=left_binder) goto done;
	const struct pg_context *element=fold->context->parent; uint64_t level; struct pg_c_indexed_operand unused[16];
	struct pg_term type_ref={.kind=PG_REFERENCE,.as.reference=element->binder};
	if (element->parent || !pg_universe_level(element->declared_type,&level) || level
		|| !family(storage,(struct pg_c_indexed_operand){&type_ref,selected.environment},entries,1,0,unused)) goto done;
	const struct pg_context *ih=fold->induction->clauses[1],*tail=ih->parent,*head=tail ? tail->parent : NULL;
	const struct pg_term *ih_type;
	if (!head || head->parent!=fold->context || !family(storage,(struct pg_c_indexed_operand){head->declared_type,selected.environment},entries,1,0,unused)
		|| !list(storage,(struct pg_c_indexed_operand){tail->declared_type,selected.environment},entries)
		|| !pg_thunk_type_view(ih->declared_type,&ih_type) || !function(storage,ih_type,selected.environment,entries)) goto done;
	fputs("/* Generated actual source append callable Fold/clauses. C33 List/Nat\n\t* storage primitives remain manual; no generalized closure ABI. */\n"
		"struct ga_result { const struct qs_nat_type *type; enum qs_list_tag tag; uint32_t head; const struct qs_list *original_tail; };\n"
		"struct ga_tail { const struct qs_nat_type *type; const struct qs_list *original; };\n"
		"static const struct qs_list *ga_apply(struct qs_arena *, const struct ga_result *, const struct qs_list *);\n"
		"static struct ga_result ga_fold(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *left)\n{\n"
		"\tstruct ga_result result = {0}; if (!enter(a)) return result;\n"
		"\tif (!index_check(a,left && left->element_type == type && (left->tag == QS_NIL || left->tag == QS_CONS))) goto done;\n"
		"\t/* Constructor selection/field capture precedes applying the callable. */\n"
		"\tresult = (struct ga_result){type,left->tag,left->tag == QS_CONS ? left->head : 0,left->tag == QS_CONS ? left->tail : NULL};\n"
		"done:\n\t--a->depth; return result;\n}\n"
		"static struct ga_result ga_force(struct qs_arena *a, const struct ga_tail *tail)\n{\n"
		"\treturn ga_fold(a,tail->type,tail->original);\n}\n"
		"static const struct qs_list *ga_apply(struct qs_arena *a, const struct ga_result *f, const struct qs_list *right)\n{\n"
		"\tconst struct qs_list *result = NULL; if (!enter(a)) return NULL;\n"
		"\tif (!index_check(a,f && right && right->element_type == f->type)) goto done;\n",out);
	struct emitter e={.out=out,.list=layout};
	for (size_t position=0; position<2; ++position) {
		const struct pg_context *clause=fold->induction->clauses[position]; size_t suffix;
		if (pg_context_extension_size(clause,fold->context,&suffix) || suffix!=(position ? 3u : 0u)) goto done;
		const struct pg_term *body=fold->operands[position+1]->core;
		const struct pg_object *fields[]={head->binder,tail->binder,ih->binder};
		for (size_t i=0; i<suffix; ++i) { if (body->kind!=PG_LAMBDA || body->as.lambda.binder!=fields[i]) goto done; body=body->as.lambda.body; }
		if (body->kind!=PG_LAMBDA) goto done;
		struct binding env[]={{NULL,element->binder,named(VALUE_TYPE,"f->type")},{NULL,head->binder,named(VALUE_NAT,"f->head")},
			{NULL,tail->binder,named(VALUE_LIST,"f->original_tail")},{NULL,ih->binder,named(VALUE_IH,"tail")},
			{NULL,body->as.lambda.binder,named(VALUE_LIST,"right")}};
		/* The actual branches never retain/use their original left after Fold.
			* Refuse such references rather than invent an uncaptured value. */
		env[1].parent=&env[0]; env[2].parent=&env[1]; env[3].parent=&env[2]; env[4].parent=position ? &env[3] : &env[0];
		fprintf(out,"\t%sif (f->tag == %s) {\n",position ? "} else " : "",position ? "QS_CONS" : "QS_NIL");
		if (position) fputs("\t\tstruct ga_tail tail = {f->type,f->original_tail};\n\t\t(void)tail;\n",out);
		struct value result;
		if (expression(&e,body->as.lambda.body,&env[4],0,&result) || result.kind!=VALUE_LIST) goto done;
		fprintf(out,"\t\tresult = %s;\n",result.name);
	}
	fputs("\t} else { a->status = 2; goto done; }\ndone:\n\t--a->depth; return result;\n}\n"
		"static const struct qs_list *ga_append(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *left, const struct qs_list *right)\n{\n"
		"\tstruct ga_result f = ga_fold(a,type,left); return a->status ? NULL : ga_apply(a,&f,right);\n}\n",out);
	status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}

int pg_c_acc_append_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !entries) return -1;
	for (size_t i=0; i<8; ++i) if (!entries[i].view.declaration) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	int status=emit_to(stage,storage,typing,root,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t n;
		while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
		if (ferror(stage) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(stage)) status=-1;
	return status;
}
