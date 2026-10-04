#include "emit.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_NAT, VALUE_ACCESS, VALUE_IH, VALUE_DOWN };
struct value { enum value_kind kind; char name[96]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct emitter {
	FILE *out;
	const struct pg_data_layout *nat, *access;
	const struct pg_term *zero_thunk, *successor;
	size_t serial, steps;
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

static int family(struct pg_graph *g, const struct pg_term *type, const struct pg_c_indexed_entry *entries,
	size_t which, size_t wanted, struct pg_c_indexed_operand *a)
{
	struct pg_c_indexed_operand head; size_t count;
	return !pg_c_indexed_head(g,(struct pg_c_indexed_operand){type,NULL},&head,&count,a) && count==wanted
		&& head.term->as.reference==pg_data_declaration_family(entries[which].view.declaration);
}

static int reference(struct pg_c_indexed_operand t, const struct pg_object *binder)
{
	t=pg_c_indexed_bound(t); return t.term && t.term->kind==PG_REFERENCE && t.term->as.reference==binder;
}

static int access(struct pg_graph *g, const struct pg_term *type, const struct pg_c_indexed_entry *entries,
	const struct pg_object *index)
{
	struct pg_c_indexed_operand a[16],unused[16];
	return family(g,type,entries,3,3,a) && family(g,a[0].term,entries,1,0,unused)
		&& family(g,a[1].term,entries,2,0,unused) && reference(a[2],index);
}

static int pure(const struct pg_term *type, const struct pg_term **result)
{
	enum pg_totality t; return pg_pure_computation_type_view(type,&t,result) && t==PG_TOTALITY_TOTAL;
}

static const struct pg_occurrence *nat_fold(struct pg_graph *g, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *lambda=carrier(root->core); if (!lambda || lambda->kind!=PG_LAMBDA) return NULL;
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return NULL;
	const struct pg_occurrence *found=NULL; if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures; struct pg_c_indexed_operand unused[16];
		if (!s->induction || s->induction->count!=2 || !s->context || s->context->binder!=lambda->as.lambda.binder
			|| pg_context_extension_size(s->context,NULL,&captures) || captures!=1) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof || view.count!=2 || !family(g,pg_evidence_classifier(view.scrutinee),entries,1,0,unused)
			|| !reference((struct pg_c_indexed_operand){pg_evidence_subject(view.scrutinee)->core,NULL},lambda->as.lambda.binder)) continue;
		if (found) { found=NULL; goto done; }
		found=s;
	}
done:
	pg_dag_destroy(&dag); return found;
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
static int apply(struct emitter *e, const struct pg_term *t, const struct binding *env,
	size_t count, const struct value *args, unsigned depth, struct value *out)
{
	if (depth>128) return -1;
	if (!count) return expression(e,t,env,depth+1,out);
	if (t->kind!=PG_LAMBDA) return -1;
	struct binding b={env,t->as.lambda.binder,args[0]}; return apply(e,t->as.lambda.body,&b,count-1,args+1,depth+1,out);
}

static int expression(struct emitter *e, const struct pg_term *t, const struct binding *env, unsigned depth, struct value *out)
{
	if (!t || depth>128 || ++e->steps>4096) return -1;
	const struct pg_term *inner=unary(t,&pg_return_operation);
	if (inner) return expression(e,inner,env,depth+1,out);
	if (unary(t,&pg_thunk_operation)) {
		if (pg_alpha_equal(t,e->zero_thunk)!=1) return -1;
		*out=named(VALUE_DOWN,"((struct qs_acc_down){zero_down,NULL})"); return 0;
	}
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
	if (object==&pg_fold_operation) {
		if (count!=2 || args[1]->kind!=PG_LAMBDA) return -1;
		struct value v; if (expression(e,args[0],env,depth+1,&v)) return -1;
		struct binding b={env,args[1]->as.lambda.binder,v}; return expression(e,args[1]->as.lambda.body,&b,depth+1,out);
	}
	if (object==&pg_force_operation) {
		if (!count) return -1;
		const struct pg_term *body=unary(args[0],&pg_thunk_operation);
		if (body) {
			body=carrier(body); if (!body || pg_alpha_equal(body,e->successor)!=1 || count!=3) return -1;
			struct value n,proof;
			if (expression(e,args[1],env,depth+1,&n) || expression(e,args[2],env,depth+1,&proof) || n.kind!=VALUE_NAT || proof.kind!=VALUE_ACCESS) return -1;
			*out=temporary(e,VALUE_ACCESS);
			fprintf(e->out,"\t\tconst struct qs_acc *%s = qs_accessible_succ(a,%s,%s);\n",out->name,n.name,proof.name); guard(e,*out); return 0;
		}
		struct value ih; if (count!=1 || expression(e,args[0],env,depth+1,&ih) || ih.kind!=VALUE_IH) return -1;
		*out=temporary(e,VALUE_ACCESS);
		fprintf(e->out,"\t\tconst struct qs_acc *%s = gn_nat_accessible(a,%s);\n",out->name,ih.name); guard(e,*out); return 0;
	}
	size_t position;
	if (pg_data_constructor_position(e->nat,object,&position)) {
		if (position>1 || count!=position) return -1;
		if (!position) { *out=named(VALUE_NAT,"0"); return 0; }
		struct value n; if (expression(e,args[0],env,depth+1,&n) || n.kind!=VALUE_NAT) return -1;
		*out=temporary(e,VALUE_NAT); fprintf(e->out,"\t\tuint32_t %s = succ(a,%s);\n\t\tif (a->status) goto done;\n",out->name,n.name); return 0;
	}
	if (!pg_data_constructor_position(e->access,object,&position) || position || count!=2) return -1;
	struct value n,down;
	if (expression(e,args[0],env,depth+1,&n) || expression(e,args[1],env,depth+1,&down) || n.kind!=VALUE_NAT || down.kind!=VALUE_DOWN) return -1;
	*out=temporary(e,VALUE_ACCESS);
	fprintf(e->out,"\t\tstruct qs_acc *%s = allocate(a,sizeof(*%s));\n",out->name,out->name); guard(e,*out);
	fprintf(e->out,"\t\t*%s = (struct qs_acc){&nat_type,&lt_relation,%s,%s,%s};\n",out->name,n.name,n.name,down.name); return 0;
}

static int emit_to(FILE *out, struct pg_graph *g, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference_root,
	const struct pg_occurrence *successor, const struct pg_c_indexed_entry *entries)
{
	const struct pg_term *domain,*codomain,*type; const struct pg_object *binder; struct pg_c_indexed_operand unused[16];
	/* Origin/type/map edges can retain an imported Fold beside a different
		* selected executable. This experiment binds the complete selected Core
		* to the supplied source reference, not just a matching context binder. */
	if (pg_alpha_equal(root->core,reference_root->core)!=1) return -1;
	if (!pg_pi_view(root->classifier,&domain,&binder,&codomain) || !family(g,domain,entries,1,0,unused)
		|| !pure(codomain,&type) || !access(g,type,entries,binder)) return -1;
	const struct pg_occurrence *fold=nat_fold(g,typing,root,entries),*reference_fold=nat_fold(g,typing,reference_root,entries);
	if (!fold || !reference_fold || !pure(fold->classifier,&type) || !access(g,type,entries,fold->context->binder)) return -1;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration),*acc=pg_data_declaration_layout(entries[3].view.declaration);
	if (pg_data_layout_count(nat)!=2 || pg_data_layout_count(acc)!=1) return -1;
	const struct pg_data_layout *actual; size_t acc_position,acc_arity;
	if (!pg_data_constructor_view(pg_data_constructor(acc,0),&actual,&acc_position,&acc_arity)
		|| actual!=acc || acc_position || acc_arity!=2) return -1;
	for (size_t i=0; i<2; ++i) {
		const struct pg_data_layout *layout; size_t p,arity;
		if (!pg_data_constructor_view(pg_data_constructor(nat,i),&layout,&p,&arity) || layout!=nat || p!=i || arity!=i) return -1;
	}
	const struct pg_term *zero=reference_fold->operands[1]->core;
	if (zero->kind!=PG_APPLICATION || !unary(zero->as.application.argument,&pg_thunk_operation)
		|| pg_alpha_equal(fold->operands[1]->core,zero)!=1) return -1;
	const struct pg_context *ih=fold->induction->clauses[1],*pred=ih->parent;
	if (!pred || pred->parent!=fold->context || !family(g,pred->declared_type,entries,1,0,unused)
		|| !pg_thunk_type_view(ih->declared_type,&type) || !pure(type,&type) || !access(g,type,entries,pred->binder)) return -1;
	fputs("/* Generated actual natAccessible Nat Fold/Acc construction/SEQ/IH call.\n\t* accessibleSucc and exact unreachable zero-down body remain manual C33.\n\t* Their checked Identity transports are not erased or lowered here. */\n"
		"static const struct qs_acc *gn_nat_accessible(struct qs_arena *a, uint32_t n)\n{\n"
		"\tconst struct qs_acc *result = NULL; if (!enter(a)) return NULL;\n",out);
	struct emitter e={.out=out,.nat=nat,.access=acc,.zero_thunk=zero->as.application.argument,.successor=carrier(successor->core)};
	if (!e.successor || e.successor->kind!=PG_LAMBDA) return -1;
	struct binding base={NULL,fold->context->binder,named(VALUE_NAT,"n")};
	for (size_t position=0; position<2; ++position) {
		size_t count; if (pg_context_extension_size(fold->induction->clauses[position],fold->context,&count) || count!=(position ? 2u : 0u)) return -1;
		fprintf(out,"\t%sif (%s) {\n",position ? "} else " : "",position ? "n" : "!n");
		if (position) fputs("\t\tuint32_t predecessor = n - 1;\n",out);
		struct value fields[]={named(VALUE_NAT,"predecessor"),named(VALUE_IH,"predecessor")},result;
		if (apply(&e,fold->operands[position+1]->core,&base,count,fields,0,&result) || result.kind!=VALUE_ACCESS) return -1;
		fprintf(out,"\t\tresult = %s;\n",result.name);
	}
	fputs("\t}\ndone:\n\t--a->depth; return result;\n}\n",out); return ferror(out) ? -1 : 0;
}

int pg_c_nat_accessibility_emit(FILE *out, struct pg_graph *g, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference_root,
	const struct pg_occurrence *successor, const struct pg_c_indexed_entry *entries)
{
	if (!out || !g || !typing || !root || !reference_root || !successor || !entries) return -1;
	for (size_t i=0; i<8; ++i) if (!entries[i].view.declaration) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	int status=emit_to(stage,g,typing,root,reference_root,successor,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t n;
		while ((n=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,n,out)!=n) { status=-1; break; }
		if (ferror(stage) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(stage)) status=-1;
	return status;
}
