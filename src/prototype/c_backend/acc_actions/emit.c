#include "emit.h"
#include "action.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_NAT, VALUE_ACC, VALUE_DOWN, VALUE_IH, VALUE_LT, VALUE_QUOTED, VALUE_PATH };
struct value { enum value_kind kind; char name[96]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct emitter {
	FILE *out;
	const struct pg_typing *typing;
	const struct pg_dag *dag;
	const struct pg_data_layout *acc;
	size_t branch,serial,steps,actions;
};
static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *op)
{
	if (!t || t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==op ? t->as.application.argument : NULL;
}
static const struct pg_term *carrier(const struct pg_term *t)
{
	for (size_t n=0; t && n<1024; ++n) {
		const struct pg_term *inner,*body;
		if ((inner=unary(t,&pg_return_operation)) || (inner=unary(t,&pg_total_result_operation))) { t=inner; continue; }
		if ((inner=unary(t,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation))) { t=body; continue; }
		return t;
	}
	return NULL;
}
static size_t spine(const struct pg_term *t, const struct pg_term **head, const struct pg_term **args)
{
	size_t n=0;
	while (t && t->kind==PG_APPLICATION) {
		if (n==16) return SIZE_MAX;
		args[n++]=t->as.application.argument; t=t->as.application.function;
	}
	for (size_t i=0; i<n/2; ++i) { const struct pg_term *a=args[i]; args[i]=args[n-i-1]; args[n-i-1]=a; }
	*head=t; return n;
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
static struct value named(enum value_kind kind, const char *name)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}
static int boundary(struct emitter *e, const struct pg_term *t, enum pg_identity_direction direction)
{
	for (const struct pg_dag_node *n=e->dag->first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (s->core!=t) continue;
		const struct pg_evidence *p=NULL;
		while ((p=pg_evidence_for_subject(e->typing,s,p))) if (pg_evidence_rule(p)==PG_IDENTITY_TRANSPORT) break;
		if (!p || !pg_evidence_owned_by(p,e->typing) || s->operand_count!=2) continue;
		struct pg_identity_boundary b; const struct pg_occurrence *formation=s->operands[0]->type;
		for (size_t i=0; formation && i<256; ++i,formation=formation->origin) {
			if (!pg_identity_boundary_view(formation,&b)) continue;
			size_t maps=e->branch ? 10 : 6;
			return direction==(e->branch ? PG_IDENTITY_LEFT : PG_IDENTITY_RIGHT) && b.path_count==2
				&& b.left_substitution && b.right_substitution && b.left_substitution->count==maps && b.right_substitution->count==maps;
		}
	}
	return 0;
}
static int expression(struct emitter *, const struct pg_term *, const struct binding *, unsigned, struct value *);
static int apply(struct emitter *e, const struct pg_term *t, const struct binding *env,
	size_t count, const struct value *values, unsigned depth, struct value *out)
{
	if (!t || depth>128) return -1;
	if (!count) return expression(e,t,env,depth+1,out);
	if (t->kind!=PG_LAMBDA) return -1;
	struct binding b={env,t->as.lambda.binder,values[0]}; return apply(e,t->as.lambda.body,&b,count-1,values+1,depth+1,out);
}
static int expression(struct emitter *e, const struct pg_term *t, const struct binding *env, unsigned depth, struct value *out)
{
	if (!t || depth>128 || ++e->steps>4096) return -1;
	const struct pg_term *inner=unary(t,&pg_return_operation);
	if (inner) return expression(e,inner,env,depth+1,out);
	const struct pg_term *body;
	if ((inner=unary(t,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation)))
		return expression(e,body,env,depth+1,out);
	const struct pg_term *family,*value; enum pg_identity_direction direction; int lift;
	if (pg_identity_field_view(t,&family,&value,&direction,&lift)) {
		struct value v; if (lift || !boundary(e,t,direction) || expression(e,value,env,depth+1,&v)) return -1;
		++e->actions;
		if (direction==PG_IDENTITY_RIGHT && v.kind==VALUE_QUOTED) {
			*out=temporary(e,VALUE_QUOTED);
			fprintf(e->out,"\t\tstruct gd_quoted_acc %s;\n\t\tif (!gd_transport_right(a,&refinement,%s,&%s)) goto done;\n",out->name,v.name,out->name); return 0;
		}
		if (direction!=PG_IDENTITY_LEFT || v.kind!=VALUE_LT) return -1;
		*out=temporary(e,VALUE_LT);
		fprintf(e->out,"\t\tconst struct qs_lt *%s = gd_transport_left(a,&refinement,%s);\n\t\tif (!%s) goto done;\n",out->name,v.name,out->name); return 0;
	}
	if ((inner=unary(t,&pg_thunk_operation))) {
		struct value v; if (expression(e,inner,env,depth+1,&v) || v.kind!=VALUE_ACC) return -1;
		*out=v; out->kind=VALUE_QUOTED; return 0;
	}
	const struct pg_term *args[16],*head; size_t count=spine(t,&head,args);
	if (count==SIZE_MAX || !head) return -1;
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
	if (object==&pg_force_operation) {
		if (!count) return -1;
		struct value v; if (expression(e,args[0],env,depth+1,&v)) return -1;
		*out=temporary(e,VALUE_ACC);
		if (count==1 && v.kind==VALUE_QUOTED) {
			fprintf(e->out,"\t\tconst struct qs_acc *%s = gd_force_acc(a,&%s);\n",out->name,v.name);
		} else if (count==3 && (v.kind==VALUE_DOWN || v.kind==VALUE_IH)) {
			struct value y,edge;
			if (expression(e,args[1],env,depth+1,&y) || expression(e,args[2],env,depth+1,&edge) || y.kind!=VALUE_NAT || edge.kind!=VALUE_LT) return -1;
			fprintf(e->out,"\t\tconst struct qs_acc *%s = %s(a,%s%s,%s,%s);\n",out->name,
				v.kind==VALUE_DOWN ? "call_raw_down" : "gd_force_ih",v.kind==VALUE_IH ? "&" : "",v.name,y.name,edge.name);
		} else return -1;
		fprintf(e->out,"\t\tif (!%s || a->status) goto done;\n",out->name); return 0;
	}
	size_t position;
	if (pg_data_constructor_position(e->acc,object,&position) && !position && count==2 && !e->branch) {
		struct value current,down;
		if (expression(e,args[0],env,depth+1,&current) || expression(e,args[1],env,depth+1,&down) || current.kind!=VALUE_NAT || down.kind!=VALUE_DOWN) return -1;
		/* Manual C42 representation reuses the captured immutable Acc when its
			* original fields are reconstructed under the actual step quotation. */
		if (strcmp(current.name,"capture->proof->current") || strcmp(down.name,"capture->proof")) return -1;
		*out=named(VALUE_ACC,"capture->proof"); return 0;
	}
	return -1;
}
static const struct pg_term *callback(const struct pg_term *t, const struct pg_object *matcher, unsigned depth)
{
	if (!t || depth>128) return NULL;
	const struct pg_term *inner=unary(t,&pg_thunk_operation);
	if (inner && inner->kind==PG_LAMBDA && inner->as.lambda.body->kind==PG_LAMBDA) {
		const struct pg_term *head,*args[16],*body=inner->as.lambda.body->as.lambda.body;
		if (spine(body,&head,args)==6 && head->kind==PG_REFERENCE && head->as.reference==matcher) return inner;
	}
	if (t->kind==PG_LAMBDA) return callback(t->as.lambda.body,matcher,depth+1);
	if (t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *a=callback(t->as.application.function,matcher,depth+1),*b=callback(t->as.application.argument,matcher,depth+1);
	return a && b ? NULL : a ? a : b;
}
static int emit_to(FILE *out, const struct pg_typing *typing, const struct pg_occurrence *root,
	const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (pg_alpha_equal(root->core,reference->core)!=1) return -1;
	const struct pg_term *lambda=carrier(root->core); if (!lambda || lambda->kind!=PG_LAMBDA) return -1;
	const struct pg_term *second=lambda->as.lambda.body; if (second->kind!=PG_LAMBDA) return -1;
	const struct pg_data_layout *acc=pg_data_declaration_layout(entries[3].view.declaration),*lt=pg_data_declaration_layout(entries[2].view.declaration);
	if (pg_data_layout_count(acc)!=1 || pg_data_layout_count(lt)!=3) return -1;
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; const struct pg_occurrence *fold=NULL;
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; struct pg_elimination_inputs inputs; const struct pg_evidence *p=NULL;
		if (!s->induction || s->induction->count!=1 || !s->context || s->context->binder!=second->as.lambda.binder) continue;
		while ((p=pg_evidence_for_subject(typing,s,p))) if (!pg_elimination_view(typing,p,&inputs)) break;
		if (!p || inputs.count!=1) continue;
		if (fold) goto done;
		fold=s;
	}
	/* The owned Acc Fold retains scrutinee, clause and both indexed Match
		* endpoints. Keep those operands; the clause remains slot one. */
	if (!fold || fold->operand_count!=4) goto done;
	const struct pg_term *clause=fold->operands[1]->core,*binders[3];
	for (size_t i=0; i<3; ++i) { if (clause->kind!=PG_LAMBDA) goto done; binders[i]=clause; clause=clause->as.lambda.body; }
	const struct pg_term *cb=callback(clause,pg_data_matcher(lt),0); if (!cb) goto done;
	const struct pg_term *body=cb->as.lambda.body->as.lambda.body,*head,*args[16];
	if (spine(body,&head,args)!=6) goto done;
	struct binding captures[7];
	const struct pg_object *objects[]={lambda->as.lambda.binder,second->as.lambda.binder,binders[0]->as.lambda.binder,
		binders[1]->as.lambda.binder,binders[2]->as.lambda.binder,cb->as.lambda.binder,cb->as.lambda.body->as.lambda.binder};
	struct value values[]={named(VALUE_NAT,"capture->parameter"),named(VALUE_ACC,"capture->proof"),named(VALUE_NAT,"capture->proof->current"),
		named(VALUE_DOWN,"capture->proof"),named(VALUE_IH,"capture->ih"),named(VALUE_NAT,"y"),named(VALUE_LT,"edge")};
	for (size_t i=0; i<7; ++i) captures[i]=(struct binding){i ? captures+i-1 : NULL,objects[i],values[i]};
	if (args[0]->kind!=PG_REFERENCE || args[0]->as.reference!=objects[6]) goto done;
	fputs("/* Generated actual successor down branch sequencing into manual C42 actions. */\n"
		"static const struct qs_acc *gs_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)\n{\n"
		"\tconst struct gd_capture *capture=context; const struct qs_acc *result=NULL; if (!enter(a)) return NULL;\n"
		"\tstruct gd_refinement refinement;\n"
		"\tif (!capture || !index_check(a,capture->proof && capture->parameter==capture->proof->subject)\n"
		"\t\t|| !gd_refine(a,capture->proof,y,edge,&refinement)) goto done;\n\tswitch (edge->tag) {\n",out);
	struct emitter e={.out=out,.typing=typing,.dag=&dag,.acc=acc};
	const char *tags[]={"QS_LT_STEP","QS_LT_WEAKEN_RIGHT","QS_LT_LIFT"},*traces[]={"step","weaken","lift"};
	for (e.branch=0; e.branch<3; ++e.branch) {
		fprintf(out,"\tcase %s: {\n\t\t++a->trace.raw_down_%s;\n",tags[e.branch],traces[e.branch]);
		struct value fields[5],result; size_t count=e.branch ? 5 : 3;
		fields[0]=named(VALUE_NAT,e.branch==0 ? "edge->fields.step" : e.branch==1 ? "edge->fields.weaken_right.m" : "edge->fields.lift.m");
		if (e.branch) {
			fields[1]=named(VALUE_NAT,e.branch==1 ? "edge->fields.weaken_right.n" : "edge->fields.lift.n");
			fields[2]=named(VALUE_LT,e.branch==1 ? "edge->fields.weaken_right.prior" : "edge->fields.lift.prior");
		}
		fields[count-2]=named(VALUE_PATH,"refinement.paths[0]"); fields[count-1]=named(VALUE_PATH,"refinement.paths[1]");
		e.actions=0;
		if (apply(&e,args[e.branch+1],captures+6,count,fields,0,&result) || result.kind!=VALUE_ACC || e.actions!=1) goto done;
		fprintf(out,"\t\tresult=%s; break;\n\t}\n",result.name);
	}
	fputs("\tdefault: a->status=2;\n\t}\ndone:\n\t--a->depth; return result;\n}\n",out); status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}
int pg_c_acc_actions_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !reference || !entries) return -1;
	for (size_t i=0; i<8; ++i) if (!entries[i].view.declaration) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	int status=emit_to(stage,typing,root,reference,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t n;
		while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
		if (ferror(stage) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(stage)) status=-1;
	return status;
}
