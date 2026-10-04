#include "emit.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_NAT, VALUE_BOOL, VALUE_IH };
struct value { enum value_kind kind; char name[80]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct emitter { FILE *out; const struct pg_data_layout *nat,*boolean; size_t serial,steps; unsigned indent; };

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *op)
{
	if (!t || t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==op ? t->as.application.argument : NULL;
}
static const struct pg_term *carrier(const struct pg_term *t)
{
	for (size_t i=0; t && i<1024; ++i) {
		const struct pg_term *inner,*body;
		if ((inner=unary(t,&pg_return_operation)) || (inner=unary(t,&pg_total_result_operation))) { t=inner; continue; }
		if ((inner=unary(t,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation))) { t=body; continue; }
		return t;
	}
	return NULL;
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
static int family(struct pg_graph *g, const struct pg_term *type, const struct pg_c_indexed_entry *entries, size_t which)
{
	struct pg_c_indexed_operand head,args[16]; size_t count;
	return !pg_c_indexed_head(g,(struct pg_c_indexed_operand){type,NULL},&head,&count,args) && !count
		&& head.term->as.reference==pg_data_declaration_family(entries[which].view.declaration);
}
static int function(struct pg_graph *g, const struct pg_term *type, const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *domain,*codomain,*result; const struct pg_object *binder; enum pg_totality totality;
	return pg_pi_view(type,&domain,&binder,&codomain) && family(g,domain,entries,1)
		&& pg_pure_computation_type_view(codomain,&totality,&result) && totality==PG_TOTALITY_TOTAL && family(g,result,entries,0);
}
static void indent(struct emitter *e)
{
	for (unsigned i=0; i<e->indent; ++i) fputc('\t',e->out);
}
static struct value named(enum value_kind kind, const char *name)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}
static int expression(struct emitter *, const struct pg_term *, const struct binding *, unsigned, struct value *);
static int apply(struct emitter *e, const struct pg_term *t, const struct binding *env,
	size_t count, const struct value *values, unsigned depth, struct value *out)
{
	if (depth>128) return -1;
	if (!count) return expression(e,t,env,depth+1,out);
	if (t->kind!=PG_LAMBDA) return -1;
	struct binding b={env,t->as.lambda.binder,values[0]}; return apply(e,t->as.lambda.body,&b,count-1,values+1,depth+1,out);
}
static int expression(struct emitter *e, const struct pg_term *t, const struct binding *env, unsigned depth, struct value *out)
{
	if (!t || depth>128 || ++e->steps>4096) return -1;
	const struct pg_term *returned=unary(t,&pg_return_operation);
	if (returned) return expression(e,returned,env,depth+1,out);
	const struct pg_term *args[16],*head=t; size_t count=0;
	while (head->kind==PG_APPLICATION) {
		if (count==16) return -1;
		args[count++]=head->as.application.argument; head=head->as.application.function;
	}
	for (size_t i=0; i<count/2; ++i) { const struct pg_term *v=args[i]; args[i]=args[count-i-1]; args[count-i-1]=v; }
	if (head->kind==PG_LAMBDA) {
		if (!count) return -1;
		struct value v[16]; for (size_t i=0; i<count; ++i) if (expression(e,args[i],env,depth+1,&v[i])) return -1;
		return apply(e,head,env,count,v,depth+1,out);
	}
	if (head->kind!=PG_REFERENCE) return -1;
	const struct pg_object *object=head->as.reference;
	if (object->kind==PG_BINDER) {
		if (count) return -1;
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==object) { *out=b->value; return 0; }
		return -1;
	}
	if (object==&pg_force_operation) {
		struct value ih,right;
		if (count!=2 || expression(e,args[0],env,depth+1,&ih) || expression(e,args[1],env,depth+1,&right) || ih.kind!=VALUE_IH || right.kind!=VALUE_NAT) return -1;
		size_t id=e->serial++; *out=temporary(e,VALUE_BOOL);
		indent(e); fprintf(e->out,"struct gc_result child%zu = gc_fold(a,%s);\n",id,ih.name);
		indent(e); fputs("if (a->status) goto done;\n",e->out);
		indent(e); fprintf(e->out,"int %s = gc_apply(a,&child%zu,%s);\n",out->name,id,right.name);
		indent(e); fputs("if (a->status) goto done;\n",e->out); return 0;
	}
	if (object==pg_data_matcher(e->nat)) {
		struct value input;
		if (count!=3 || expression(e,args[0],env,depth+1,&input) || input.kind!=VALUE_NAT) return -1;
		*out=temporary(e,VALUE_BOOL); indent(e); fprintf(e->out,"int %s = 0;\n",out->name);
		indent(e); fprintf(e->out,"if (!%s) {\n",input.name); ++e->indent;
		struct value zero; if (expression(e,args[1],env,depth+1,&zero) || zero.kind!=VALUE_BOOL) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",out->name,zero.name); --e->indent;
		indent(e); fputs("} else {\n",e->out); ++e->indent;
		struct value pred=temporary(e,VALUE_NAT),succ;
		indent(e); fprintf(e->out,"uint32_t %s = %s - 1;\n",pred.name,input.name);
		if (apply(e,args[2],env,1,&pred,depth+1,&succ) || succ.kind!=VALUE_BOOL) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",out->name,succ.name); --e->indent;
		indent(e); fputs("}\n",e->out); return 0;
	}
	size_t position;
	if (!count && pg_data_constructor_position(e->boolean,object,&position) && position<2) {
		/* Same private qs_compare convention already used by C33/C37. */
		*out=named(VALUE_BOOL,position ? "0" : "1"); return 0;
	}
	return -1;
}
static int emit_to(FILE *out, struct pg_graph *g, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (pg_alpha_equal(root->core,reference->core)!=1) return -1;
	const struct pg_term *lambda=carrier(root->core),*domain,*codomain; const struct pg_object *binder;
	if (!lambda || lambda->kind!=PG_LAMBDA || !pg_pi_view(root->classifier,&domain,&binder,&codomain)
		|| !family(g,domain,entries,1) || !function(g,codomain,entries)) return -1;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration),*boolean=pg_data_declaration_layout(entries[0].view.declaration);
	if (pg_data_layout_count(nat)!=2 || pg_data_layout_count(boolean)!=2) return -1;
	for (size_t family=0; family<2; ++family) for (size_t i=0; i<2; ++i) {
		const struct pg_data_layout *layout,*wanted=family ? nat : boolean; size_t position,arity;
		if (!pg_data_constructor_view(pg_data_constructor(wanted,i),&layout,&position,&arity) || layout!=wanted || position!=i || arity!=(family ? i : 0u)) return -1;
	}
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; const struct pg_occurrence *fold=NULL;
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures;
		if (!s->induction || s->induction->count!=2 || !s->context || s->context->binder!=lambda->as.lambda.binder
			|| pg_context_extension_size(s->context,NULL,&captures) || captures!=1) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof || view.count!=2 || !family(g,pg_evidence_classifier(view.scrutinee),entries,1)) continue;
		const struct pg_term *scrutinee=pg_evidence_subject(view.scrutinee)->core;
		if (scrutinee->kind!=PG_REFERENCE || scrutinee->as.reference!=lambda->as.lambda.binder) continue;
		if (fold) goto done;
		fold=s;
	}
	if (!fold || !function(g,fold->classifier,entries)) goto done;
	const struct pg_context *ih=fold->induction->clauses[1],*pred=ih->parent; const struct pg_term *ih_type;
	if (!pred || pred->parent!=fold->context || !family(g,pred->declared_type,entries,1)
		|| !pg_thunk_type_view(ih->declared_type,&ih_type) || !function(g,ih_type,entries)) goto done;
	fputs("/* Generated actual natLessOrEqual callable Nat Fold/clauses.\n\t* Same private Bool callback convention:1 true,0 false; status separate. */\n"
		"struct gc_result { int nonzero; uint32_t predecessor; };\n"
		"static int gc_apply(struct qs_arena *, const struct gc_result *, uint32_t);\n"
		"static struct gc_result gc_fold(struct qs_arena *a, uint32_t left)\n{\n"
		"\tstruct gc_result result = {0}; if (!enter(a)) return result;\n"
		"\tresult = (struct gc_result){left != 0,left ? left - 1 : 0};\n\t--a->depth; return result;\n}\n"
		"static int gc_apply(struct qs_arena *a, const struct gc_result *f, uint32_t right)\n{\n"
		"\tint result = 0; if (!enter(a)) return 0;\n"
		"\tif (!index_check(a,f && (f->nonzero == 0 || f->nonzero == 1))) goto done;\n",out);
	struct emitter e={.out=out,.nat=nat,.boolean=boolean,.indent=2};
	for (size_t position=0; position<2; ++position) {
		size_t suffix; if (pg_context_extension_size(fold->induction->clauses[position],fold->context,&suffix) || suffix!=(position ? 2u : 0u)) goto done;
		const struct pg_term *body=fold->operands[position+1]->core;
		const struct pg_object *fields[]={pred->binder,ih->binder};
		for (size_t i=0; i<suffix; ++i) { if (body->kind!=PG_LAMBDA || body->as.lambda.binder!=fields[i]) goto done; body=body->as.lambda.body; }
		if (body->kind!=PG_LAMBDA) goto done;
		struct binding env[]={{NULL,pred->binder,named(VALUE_NAT,"f->predecessor")},
			{NULL,ih->binder,named(VALUE_IH,"f->predecessor")},{NULL,body->as.lambda.binder,named(VALUE_NAT,"right")}};
		env[1].parent=&env[0]; env[2].parent=position ? &env[1] : NULL;
		fprintf(out,"\t%sif (%s) {\n",position ? "} else " : "",position ? "f->nonzero" : "!f->nonzero");
		struct value result;
		if (expression(&e,body->as.lambda.body,&env[2],0,&result) || result.kind!=VALUE_BOOL) goto done;
		fprintf(out,"\t\tresult = %s;\n",result.name);
	}
	fputs("\t}\ndone:\n\t--a->depth; return result;\n}\n"
		"static int gc_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n{\n"
		"\t(void)context; struct gc_result f = gc_fold(a,left);\n"
		"\treturn a->status ? 0 : gc_apply(a,&f,right);\n}\n",out);
	status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}
int pg_c_acc_comparison_emit(FILE *out, struct pg_graph *g, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (!out || !g || !typing || !root || !reference || !entries) return -1;
	for (size_t i=0; i<8; ++i) if (!entries[i].view.declaration) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	int status=emit_to(stage,g,typing,root,reference,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t n;
		while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
		if (ferror(stage) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(stage)) status=-1;
	return status;
}
