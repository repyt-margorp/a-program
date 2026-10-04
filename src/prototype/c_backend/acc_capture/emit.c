/* Reuse the sealed branch lowerer and its private expression readers. This
	* introduces no shared source representation or producer interface. */
#include "../acc_actions/emit.c"
#include "emit.h"

enum construction_kind { CONSTRUCT_NAT, CONSTRUCT_ACCESS, CONSTRUCT_RAW, CONSTRUCT_IH, CONSTRUCT_DOWN };
struct construction_value { enum construction_kind kind; char name[128]; };
struct construction_binding {
	const struct construction_binding *parent;
	const struct pg_object *binder;
	struct construction_value value;
};
struct construction_emitter {
	FILE *out;
	const struct pg_data_layout *nat,*acc;
	const struct pg_term *callback;
	const struct pg_object *parameter,*proof,*current,*original,*ih;
	size_t serial,steps,closures;
};
static struct construction_value construction_named(enum construction_kind kind, const char *name)
{
	struct construction_value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct construction_value construction_temporary(struct construction_emitter *e, enum construction_kind kind)
{
	struct construction_value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"c%zu",e->serial++); return v;
}
static int construction_lookup(const struct construction_binding *env, const struct pg_object *binder, struct construction_value *out)
{
	for (const struct construction_binding *b=env; b; b=b->parent)
		if (b->binder==binder) { *out=b->value; return 0; }
	return -1;
}
static int construction_expression(struct construction_emitter *, const struct pg_term *, const struct construction_binding *, unsigned, struct construction_value *);
static int construction_apply(struct construction_emitter *e, const struct pg_term *t, const struct construction_binding *env,
	size_t count, const struct construction_value *values, unsigned depth, struct construction_value *out)
{
	if (!t || depth>128) return -1;
	if (!count) return construction_expression(e,t,env,depth+1,out);
	if (t->kind!=PG_LAMBDA) {
		/* The source constructor adapter can bind an index before exposing
			* its remaining lambda. Carry target values through that static APP;
			* do not normalize source terms or execute a callable. */
		const struct pg_term *head,*args[16]; size_t prefix=spine(t,&head,args);
		if (!prefix || prefix==SIZE_MAX || !head || head->kind!=PG_LAMBDA || prefix+count>16) return -1;
		struct construction_value combined[16];
		for (size_t i=0; i<prefix; ++i) if (construction_expression(e,args[i],env,depth+1,combined+i)) return -1;
		for (size_t i=0; i<count; ++i) combined[prefix+i]=values[i];
		return construction_apply(e,head,env,prefix+count,combined,depth+1,out);
	}
	struct construction_binding b={env,t->as.lambda.binder,values[0]};
	return construction_apply(e,t->as.lambda.body,&b,count-1,values+1,depth+1,out);
}
static int construction_expression(struct construction_emitter *e, const struct pg_term *t,
	const struct construction_binding *env, unsigned depth, struct construction_value *out)
{
	if (!t || depth>128 || ++e->steps>4096) return -1;
	const struct pg_term *inner=unary(t,&pg_return_operation);
	if (inner) return construction_expression(e,inner,env,depth+1,out);
	if ((inner=unary(t,&pg_thunk_operation))) {
		if (inner!=e->callback || ++e->closures!=1) return -1;
		struct construction_value parameter,proof,current,original,ih;
		if (construction_lookup(env,e->parameter,&parameter) || construction_lookup(env,e->proof,&proof)
			|| construction_lookup(env,e->current,&current) || construction_lookup(env,e->original,&original)
			|| construction_lookup(env,e->ih,&ih) || parameter.kind!=CONSTRUCT_NAT || proof.kind!=CONSTRUCT_ACCESS
			|| current.kind!=CONSTRUCT_NAT || original.kind!=CONSTRUCT_RAW || ih.kind!=CONSTRUCT_IH) return -1;
		/* The sealed branch emitter derives which callable each expression uses.
			* This existing private closure layout retains original proof and IH;
			* it does not establish general source closure/action equivalence. */
		if (strcmp(current.name,"proof->current") || strcmp(original.name,"proof->down")) return -1;
		struct construction_value capture=construction_temporary(e,CONSTRUCT_DOWN);
		fprintf(e->out,"\tstruct gd_capture *%s=allocate(a,sizeof(*%s));\n\tif (!%s) goto done;\n",capture.name,capture.name,capture.name);
		fprintf(e->out,"\t*%s=(struct gd_capture){%s,%s,%s};\n",capture.name,parameter.name,proof.name,ih.name);
		*out=(struct construction_value){.kind=CONSTRUCT_DOWN};
		snprintf(out->name,sizeof(out->name),"((struct qs_acc_down){gs_down,%.80s})",capture.name); return 0;
	}
	const struct pg_term *head,*args[16]; size_t count=spine(t,&head,args);
	if (count==SIZE_MAX || !head) return -1;
	if (head->kind==PG_LAMBDA) {
		struct construction_value values[16]; if (!count) return -1;
		for (size_t i=0; i<count; ++i) if (construction_expression(e,args[i],env,depth+1,&values[i])) return -1;
		return construction_apply(e,head,env,count,values,depth+1,out);
	}
	if (head->kind!=PG_REFERENCE) return -1;
	const struct pg_object *object=head->as.reference;
	if (object->kind==PG_BINDER) return count ? -1 : construction_lookup(env,object,out);
	size_t position;
	if (pg_data_constructor_position(e->nat,object,&position)) {
		if (position>1 || count!=position) return -1;
		if (!position) { *out=construction_named(CONSTRUCT_NAT,"0"); return 0; }
		struct construction_value n;
		if (construction_expression(e,args[0],env,depth+1,&n) || n.kind!=CONSTRUCT_NAT) return -1;
		*out=construction_temporary(e,CONSTRUCT_NAT);
		fprintf(e->out,"\tuint32_t %s=succ(a,%s);\n\tif (a->status) goto done;\n",out->name,n.name); return 0;
	}
	if (!pg_data_constructor_position(e->acc,object,&position) || position || count!=2) return -1;
	struct construction_value n,down;
	if (construction_expression(e,args[0],env,depth+1,&n) || construction_expression(e,args[1],env,depth+1,&down)
		|| n.kind!=CONSTRUCT_NAT || down.kind!=CONSTRUCT_DOWN) return -1;
	*out=construction_temporary(e,CONSTRUCT_ACCESS);
	fprintf(e->out,"\tstruct qs_acc *%s=allocate(a,sizeof(*%s));\n\tif (!%s) goto done;\n",out->name,out->name,out->name);
	fprintf(e->out,"\t*%s=(struct qs_acc){&nat_type,&lt_relation,%s,%s,%s};\n",out->name,n.name,n.name,down.name); return 0;
}
static int construction_to(FILE *out, const struct pg_typing *typing, const struct pg_occurrence *root,
	const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (pg_alpha_equal(root->core,reference->core)!=1) return -1;
	const struct pg_term *lambda=carrier(root->core),*second;
	if (!lambda || lambda->kind!=PG_LAMBDA || (second=lambda->as.lambda.body)->kind!=PG_LAMBDA) return -1;
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; const struct pg_occurrence *fold=NULL;
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures;
		if (!s->induction || s->induction->count!=1 || !s->context || s->context->binder!=second->as.lambda.binder
			|| pg_context_extension_size(s->context,NULL,&captures) || captures!=2) continue;
		const struct pg_evidence *p=NULL; struct pg_elimination_inputs view;
		while ((p=pg_evidence_for_subject(typing,s,p))) if (!pg_elimination_view(typing,p,&view)) break;
		if (!p || view.count!=1 || pg_evidence_subject(view.scrutinee)->core->kind!=PG_REFERENCE
			|| pg_evidence_subject(view.scrutinee)->core->as.reference!=second->as.lambda.binder) continue;
		if (fold) goto done;
		fold=s;
	}
	if (!fold || fold->operand_count!=4) goto done;
	const struct pg_context *ih=fold->induction->clauses[0],*original=ih ? ih->parent : NULL,*current=original ? original->parent : NULL;
	if (!current || current->parent!=fold->context) goto done;
	const struct pg_object *fields[]={current->binder,original->binder,ih->binder};
	const struct pg_term *body=fold->operands[1]->core;
	for (size_t i=0; i<3; ++i) { if (body->kind!=PG_LAMBDA || body->as.lambda.binder!=fields[i]) goto done; body=body->as.lambda.body; }
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration),*acc=pg_data_declaration_layout(entries[3].view.declaration);
	const struct pg_data_layout *lt=pg_data_declaration_layout(entries[2].view.declaration);
	if (pg_data_layout_count(nat)!=2 || pg_data_layout_count(acc)!=1) goto done;
	const struct pg_term *cb=callback(body,pg_data_matcher(lt),0); if (!cb) goto done;
	fputs("/* Generated actual successor Acc Fold/Nat/Acc/captured down construction.\n\t* Bounded action/closure representation remains an explicit target choice. */\n"
		"static const struct qs_acc *gs_accessible_succ(struct qs_arena *a, uint32_t n, const struct qs_acc *proof)\n{\n"
		"\tif (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation && proof->subject==n && proof->current==n)) return NULL;\n"
		"\tconst struct qs_acc *result=NULL;\n",out);
	struct construction_emitter e={.out=out,.nat=nat,.acc=acc,.callback=cb,.parameter=lambda->as.lambda.binder,
		.proof=second->as.lambda.binder,.current=fields[0],.original=fields[1],.ih=fields[2]};
	const struct pg_object *objects[]={e.parameter,e.proof,e.current,e.original,e.ih};
	struct construction_value values[]={construction_named(CONSTRUCT_NAT,"n"),construction_named(CONSTRUCT_ACCESS,"proof"),
		construction_named(CONSTRUCT_NAT,"proof->current"),construction_named(CONSTRUCT_RAW,"proof->down"),
		construction_named(CONSTRUCT_IH,"((struct gd_folded_ih){proof->down,proof->current})")};
	struct construction_binding env[5];
	for (size_t i=0; i<5; ++i) env[i]=(struct construction_binding){i ? env+i-1 : NULL,objects[i],values[i]};
	struct construction_value result;
	if (construction_expression(&e,body,env+4,0,&result) || result.kind!=CONSTRUCT_ACCESS || e.closures!=1) goto done;
	fprintf(out,"\tresult=%s;\ndone:\n\treturn result;\n}\n",result.name); status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}
int pg_c_acc_successor_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !reference || !entries) return -1;
	for (size_t i=0; i<8; ++i) if (!entries[i].view.declaration) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	int status=pg_c_acc_actions_emit(stage,storage,typing,root,reference,entries);
	if (!status) status=construction_to(stage,typing,root,reference,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t n;
		while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
		if (ferror(stage) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(stage)) status=-1;
	return status;
}
