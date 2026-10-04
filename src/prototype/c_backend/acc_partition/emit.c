#include "emit.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_NAT, VALUE_TYPE, VALUE_COMPARE, VALUE_BOOL, VALUE_SIZED, VALUE_PART, VALUE_LT, VALUE_TAIL };
struct value { enum value_kind kind; char name[128]; };
struct binding { const struct binding *parent; const struct pg_object *binder; struct value value; };
struct argument { const struct pg_term *term; const struct binding *env; struct value value; };
struct emitter {
	FILE *out;
	struct pg_graph *storage;
	struct pg_dag *dag;
	const struct pg_c_indexed_entry *entries;
	const struct pg_data_layout *layouts[8];
	size_t serial, steps;
	unsigned indent;
};

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *op)
{
	if (!t || t->kind != PG_APPLICATION) return NULL;
	const struct pg_term *f = t->as.application.function;
	return f->kind == PG_REFERENCE && f->as.reference == op ? t->as.application.argument : NULL;
}

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s = key;
	if (slot < s->operand_count) { *out=s->operands[slot]; return *out ? 1 : 2; }
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

static int selected_lambda(struct pg_graph *g, const struct pg_term *t, struct pg_c_indexed_operand *out)
{
	struct pg_c_indexed_operand at={t,NULL}, stack[16]; size_t count=0;
	for (size_t step=0; step<1024 && at.term; ++step) {
		at=pg_c_indexed_bound(at); t=at.term; const struct pg_term *inner,*body;
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

static int arguments(struct emitter *e, struct pg_c_indexed_operand type, size_t family, size_t wanted, struct pg_c_indexed_operand *a)
{
	struct pg_c_indexed_operand head; size_t count;
	return !pg_c_indexed_head(e->storage,type,&head,&count,a) && count==wanted
		&& head.term->as.reference==pg_data_declaration_family(e->entries[family].view.declaration);
}

static int family(struct emitter *e, struct pg_c_indexed_operand type, size_t f, size_t n)
{
	struct pg_c_indexed_operand a[16]; return arguments(e,type,f,n,a);
}

static int reference(struct pg_c_indexed_operand at, const struct pg_object *binder)
{
	at=pg_c_indexed_bound(at); return at.term && at.term->kind==PG_REFERENCE && at.term->as.reference==binder;
}

static void indent(struct emitter *e) { for (unsigned i=0; i<e->indent; ++i) fputc('\t',e->out); }
static struct value named(enum value_kind kind, const char *name)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}
static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v={.kind=kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}
static const char *ctype(enum value_kind k)
{
	switch (k) {
	case VALUE_NAT: return "uint32_t";
	case VALUE_TYPE: return "const struct qs_nat_type *";
	case VALUE_COMPARE: return "struct qs_compare";
	case VALUE_BOOL: return "int";
	case VALUE_SIZED: return "const struct qs_sized_list *";
	case VALUE_PART: return "const struct qs_partition *";
	case VALUE_LT: return "const struct qs_lt *";
	case VALUE_TAIL: return "struct gp_tail";
	}
	return NULL;
}
static int field_name(struct value *v, struct value input, const char *field)
{
	size_t n=strlen(input.name),m=strlen(field); if (n+m+2>=sizeof(v->name)) return -1;
	memcpy(v->name,input.name,n); memcpy(v->name+n,"->",2); memcpy(v->name+n+2,field,m+1); return 0;
}
static void guard(struct emitter *e, struct value v)
{
	indent(e); fprintf(e->out,"if (!%s || a->status) goto done;\n",v.name);
}

static int expression(struct emitter *, const struct pg_term *, const struct binding *, size_t, const struct argument *, unsigned, struct value *);
static int value(struct emitter *e, struct argument a, unsigned depth, struct value *v)
{
	if (!a.term) { *v=a.value; return 0; }
	return expression(e,a.term,a.env,0,NULL,depth+1,v);
}
static int apply(struct emitter *e, const struct pg_term *t, const struct binding *env,
	size_t n, const struct argument *a, unsigned depth, struct value *out)
{
	if (!n || t->kind!=PG_LAMBDA) return expression(e,t,env,n,a,depth+1,out);
	struct value v; if (value(e,a[0],depth+1,&v)) return -1;
	struct binding b={env,t->as.lambda.binder,v}; return apply(e,t->as.lambda.body,&b,n-1,a+1,depth+1,out);
}

/* Parameter metadata is read from an actual retained classifier of this exact
	* constructor term. No expected-result fallback, rechecking or source coercion. */
static int metadata_supported(struct emitter *e, struct pg_c_indexed_operand at, const struct binding *env, enum value_kind wanted, unsigned depth)
{
	if (depth>128) return 0;
	at=pg_c_indexed_bound(at); const struct pg_term *t=at.term; if (!t) return 0;
	if (t->kind==PG_REFERENCE) {
		for (const struct binding *b=env; b; b=b->parent) if (b->binder==t->as.reference) return b->value.kind==wanted;
		if (wanted==VALUE_TYPE) return t->as.reference==pg_data_declaration_family(e->entries[1].view.declaration);
		return wanted==VALUE_NAT && t->as.reference==pg_data_constructor(e->layouts[1],0);
	}
	if (wanted!=VALUE_NAT || t->kind!=PG_APPLICATION) return 0;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==pg_data_constructor(e->layouts[1],1)
		&& metadata_supported(e,(struct pg_c_indexed_operand){t->as.application.argument,at.environment},env,VALUE_NAT,depth+1);
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

static int parameters(struct emitter *e, const struct pg_term *term, const struct binding *env, size_t f, struct value *type, struct value *index)
{
	struct pg_c_indexed_operand chosen[2]; const struct pg_term *classifier=NULL;
	for (const struct pg_dag_node *n=e->dag->first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (s->core!=term) continue;
		struct pg_c_indexed_operand a[16];
		if (!arguments(e,(struct pg_c_indexed_operand){s->classifier,NULL},f,2,a)
			|| !metadata_supported(e,a[0],env,VALUE_TYPE,0) || !metadata_supported(e,a[1],env,VALUE_NAT,0)) continue;
		if (classifier && pg_alpha_equal(classifier,s->classifier)!=1) return -1;
		classifier=s->classifier; chosen[0]=a[0]; chosen[1]=a[1];
	}
	return !classifier || metadata_value(e,chosen[0],env,VALUE_TYPE,type) || metadata_value(e,chosen[1],env,VALUE_NAT,index) ? -1 : 0;
}

static int match(struct emitter *e, size_t f, size_t n, const struct argument *a, unsigned depth, struct value *out)
{
	struct value input; if (value(e,a[0],depth+1,&input)) return -1;
	struct value result=temporary(e,VALUE_PART);
	indent(e); fprintf(e->out,"const struct qs_partition *%s = NULL;\n",result.name);
	if (f==0 && n==3 && input.kind==VALUE_BOOL) {
		indent(e); fprintf(e->out,"if (%s) {\n",input.name); ++e->indent;
		indent(e); fputs("++a->trace.partition_lower;\n",e->out);
		struct value left; if (expression(e,a[1].term,a[1].env,0,NULL,depth+1,&left) || left.kind!=VALUE_PART) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",result.name,left.name); --e->indent;
		indent(e); fputs("} else {\n",e->out); ++e->indent;
		indent(e); fputs("++a->trace.partition_upper;\n",e->out);
		struct value right; if (expression(e,a[2].term,a[2].env,0,NULL,depth+1,&right) || right.kind!=VALUE_PART) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",result.name,right.name); --e->indent;
		indent(e); fputs("}\n",e->out);
	} else if (f==6 && n==2 && input.kind==VALUE_PART) {
		guard(e,input); const char *fields[]={"lower_size","lower","upper_size","upper","lower_bound","upper_bound"};
		const enum value_kind kinds[]={VALUE_NAT,VALUE_SIZED,VALUE_NAT,VALUE_SIZED,VALUE_LT,VALUE_LT};
		struct argument args[6]={0};
		for (size_t i=0; i<6; ++i) { args[i].value.kind=kinds[i]; if (field_name(&args[i].value,input,fields[i])) return -1; }
		struct value body;
		if (apply(e,a[1].term,a[1].env,6,args,depth+1,&body) || body.kind!=VALUE_PART) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",result.name,body.name);
	} else return -1;
	*out=result; return 0;
}

static int constructor(struct emitter *e, const struct pg_term *term, size_t f, size_t position,
	size_t n, const struct argument *a, const struct binding *env, unsigned depth, struct value *out)
{
	const struct pg_data_layout *layout; size_t actual,arity;
	if (!pg_data_constructor_view(pg_data_constructor(e->layouts[f],position),&layout,&actual,&arity) || n!=arity) return -1;
	struct value v[6]; if (n>6) return -1;
	for (size_t i=0; i<n; ++i) if (value(e,a[i],depth+1,&v[i])) return -1;
	if (f==0 && !n && position<2) { *out=named(VALUE_BOOL,position ? "0" : "1"); return 0; }
	if (f==1 && position<2) {
		if (!position && !n) { *out=named(VALUE_NAT,"0"); return 0; }
		if (n!=1 || v[0].kind!=VALUE_NAT) return -1;
		*out=temporary(e,VALUE_NAT); indent(e); fprintf(e->out,"uint32_t %s = succ(a,%s);\n",out->name,v[0].name); return 0;
	}
	if (f==2 && position<3) {
		if (n!=(position ? 3u : 1u) || v[0].kind!=VALUE_NAT || (position && (v[1].kind!=VALUE_NAT || v[2].kind!=VALUE_LT))) return -1;
		*out=temporary(e,VALUE_LT); indent(e); fprintf(e->out,"%s %s = %s(a,%s",ctype(out->kind),out->name,position==0 ? "lt_step" : position==1 ? "lt_weaken" : "lt_lift",v[0].name);
		if (position) fprintf(e->out,",%s,%s",v[1].name,v[2].name);
		fputs(");\n",e->out); guard(e,*out); return 0;
	}
	struct value type,index;
	if ((f!=4 && f!=6) || parameters(e,term,env,f,&type,&index)) return -1;
	if (f==4 && position<2) {
		if (n!=(position ? 3u : 0u) || (position && (v[0].kind!=VALUE_NAT || v[1].kind!=VALUE_NAT || v[2].kind!=VALUE_SIZED))) return -1;
		*out=temporary(e,VALUE_SIZED); indent(e); fprintf(e->out,"%s %s = %s(a,%s",ctype(out->kind),out->name,position ? "sized_cons" : "sized_nil",type.name);
		if (position) fprintf(e->out,",%s,%s,%s",v[0].name,v[1].name,v[2].name);
		fputs(");\n",e->out); guard(e,*out);
		indent(e); fprintf(e->out,"if (!index_check(a,%s->index == %s)) goto done;\n",out->name,index.name); return 0;
	}
	if (f!=6 || position || n!=6 || v[0].kind!=VALUE_NAT || v[1].kind!=VALUE_SIZED || v[2].kind!=VALUE_NAT
		|| v[3].kind!=VALUE_SIZED || v[4].kind!=VALUE_LT || v[5].kind!=VALUE_LT) return -1;
	struct value allocated=temporary(e,VALUE_PART); *out=allocated;
	indent(e); fprintf(e->out,"struct qs_partition *%s = allocate(a,sizeof(*%s));\n",out->name,out->name); guard(e,*out);
	indent(e); fprintf(e->out,"*%s = (struct qs_partition){%s,%s",out->name,type.name,index.name);
	for (size_t i=0; i<6; ++i) fprintf(e->out,",%s",v[i].name);
	fputs("};\n",e->out); return 0;
}

static int expression(struct emitter *e, const struct pg_term *term, const struct binding *env,
	size_t pending, const struct argument *suffix, unsigned depth, struct value *out)
{
	if (!term || depth>512 || ++e->steps>8192 || pending>16) return -1;
	const struct pg_term *returned=unary(term,&pg_return_operation);
	if (returned) return expression(e,returned,env,pending,suffix,depth+1,out);
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
		if (body) return apply(e,body,args[0].env,count-1,args+1,depth+1,out);
		struct value forced; if (value(e,args[0],depth+1,&forced)) return -1;
		if (forced.kind==VALUE_TAIL && count==1) {
			*out=temporary(e,VALUE_PART); indent(e); fprintf(e->out,"%s %s = gp_fold(a,&%s.capture,%s.size,%s.values);\n",ctype(out->kind),out->name,forced.name,forced.name,forced.name);
			guard(e,*out); return 0;
		}
		if (forced.kind==VALUE_COMPARE && count==3) {
			struct value l,r; if (value(e,args[1],depth+1,&l) || value(e,args[2],depth+1,&r) || l.kind!=VALUE_NAT || r.kind!=VALUE_NAT) return -1;
			*out=temporary(e,VALUE_BOOL); indent(e); fprintf(e->out,"int %s = %s.call(a,%s.context,%s,%s);\n",out->name,forced.name,forced.name,l.name,r.name);
			indent(e); fputs("if (a->status) goto done;\n",e->out);
			indent(e); fprintf(e->out,"if (!index_check(a,%s == 0 || %s == 1)) goto done;\n",out->name,out->name); return 0;
		}
		return -1;
	}
	for (size_t i=0; i<8; ++i) {
		if (object==pg_data_matcher(e->layouts[i])) return count ? match(e,i,count,args,depth+1,out) : -1;
		size_t position;
		if (pg_data_constructor_position(e->layouts[i],object,&position)) return constructor(e,term,i,position,count,args,env,depth+1,out);
	}
	return -1;
}

static int emit_to(struct emitter *e, const struct pg_typing *typing, const struct pg_occurrence *root)
{
	struct pg_c_indexed_operand selected;
	if (selected_lambda(e->storage,root->core,&selected)) return -1;
	const struct pg_term *t=selected.term; const struct pg_object *binders[3];
	for (size_t i=0; i<3; ++i) { if (t->kind!=PG_LAMBDA) return -1; binders[i]=t->as.lambda.binder; t=t->as.lambda.body; }
	const struct pg_term *type=root->classifier,*domain,*codomain; const struct pg_object *binder,*size_binder=NULL;
	for (size_t i=0; i<3; ++i) {
		if (!pg_pi_view(type,&domain,&binder,&codomain)) return -1;
		if (i<2 && !family(e,(struct pg_c_indexed_operand){domain,NULL},1,0)) return -1;
		if (i==1) size_binder=binder;
		if (i==2) {
			struct pg_c_indexed_operand a[16];
			if (!arguments(e,(struct pg_c_indexed_operand){domain,NULL},4,2,a) || !family(e,a[0],1,0) || !reference(a[1],size_binder)) return -1;
		}
		type=codomain;
	}
	enum pg_totality totality; const struct pg_term *result_type; struct pg_c_indexed_operand result_args[16];
	if (!pg_pure_computation_type_view(type,&totality,&result_type) || totality!=PG_TOTALITY_TOTAL
		|| !arguments(e,(struct pg_c_indexed_operand){result_type,NULL},6,2,result_args)
		|| !family(e,result_args[0],1,0) || !reference(result_args[1],size_binder)) return -1;
	const struct pg_occurrence *fold=NULL; struct pg_elimination_inputs chosen={0};
	for (const struct pg_dag_node *n=e->dag->first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; size_t captures;
		if (!s->induction || s->induction->count!=2 || pg_context_extension_size(s->context,NULL,&captures) || captures!=5) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof) continue;
		if (fold) return -1;
		fold=s; chosen=view;
	}
	if (!fold || chosen.count!=2) return -1;
	const struct pg_context *input=fold->context,*size=input->parent,*pivot=size->parent,*comparison=pivot->parent,*element=comparison->parent;
	if (input->binder!=binders[2] || size->binder!=binders[1] || pivot->binder!=binders[0] || !element || element->parent
		|| !reference((struct pg_c_indexed_operand){pg_evidence_subject(chosen.scrutinee)->core,NULL},input->binder)) return -1;
	struct pg_term element_ref={.kind=PG_REFERENCE,.as.reference=element->binder};
	if (!family(e,(struct pg_c_indexed_operand){&element_ref,selected.environment},1,0)) return -1;
	uint64_t level;
	if (!pg_universe_level(element->declared_type,&level) || level
		|| !family(e,(struct pg_c_indexed_operand){pivot->declared_type,selected.environment},1,0)
		|| !family(e,(struct pg_c_indexed_operand){size->declared_type,selected.environment},1,0)) return -1;
	const struct pg_term *compare_type;
	if (!pg_thunk_type_view(comparison->declared_type,&compare_type)) return -1;
	for (size_t i=0; i<2; ++i) {
		if (!pg_pi_view(compare_type,&domain,&binder,&codomain)
			|| !family(e,(struct pg_c_indexed_operand){domain,selected.environment},1,0)) return -1;
		compare_type=codomain;
	}
	if (!pg_pure_computation_type_view(compare_type,&totality,&result_type) || totality!=PG_TOTALITY_TOTAL
		|| !family(e,(struct pg_c_indexed_operand){result_type,selected.environment},0,0)) return -1;
	struct binding env[]={{NULL,element->binder,named(VALUE_TYPE,"capture->type")},
		{NULL,comparison->binder,named(VALUE_COMPARE,"capture->le")},{NULL,pivot->binder,named(VALUE_NAT,"capture->pivot")},
		{NULL,size->binder,named(VALUE_NAT,"size")},{NULL,input->binder,named(VALUE_SIZED,"input")}};
	for (size_t i=1; i<5; ++i) env[i].parent=&env[i-1];
	fputs("/* Generated actual partition Fold and inline source helper expressions.\n\t* C33 storage/Nat/LT primitives and outer entry remain manual. */\n"
		"struct gp_capture { const struct qs_nat_type *type; struct qs_compare le; uint32_t pivot; };\n"
		"struct gp_tail { struct gp_capture capture; uint32_t size; const struct qs_sized_list *values; };\n"
		"static const struct qs_partition *gp_fold(struct qs_arena *a, const struct gp_capture *capture, uint32_t size, const struct qs_sized_list *input)\n{\n"
		"\tconst struct qs_partition *result = NULL; if (!enter(a)) return NULL;\n"
		"\tif (!index_check(a,input && input->element_type == capture->type && input->index == size && capture->le.call)) goto done;\n",e->out);
	for (size_t position=0; position<2; ++position) {
		const struct pg_context *clause=fold->induction->clauses[position]; size_t suffix;
		if (pg_context_extension_size(clause,fold->context,&suffix) || suffix!=(position ? 4u : 0u)) return -1;
		indent(e); fprintf(e->out,"%sif (input->tag == %s) {\n",position ? "} else " : "",position ? "QS_CONS" : "QS_NIL"); ++e->indent;
		indent(e); fputs(position ? "if (!index_check(a,succ(a,input->tail_size) == size)) goto done;\n" : "if (!index_check(a,size == 0)) goto done;\n",e->out);
		if (position) { indent(e); fputs("struct gp_tail tail = {*capture,input->tail_size,input->tail};\n",e->out); }
		struct argument fields[4]={{.value=named(VALUE_NAT,"input->tail_size")},{.value=named(VALUE_NAT,"input->head")},
			{.value=named(VALUE_SIZED,"input->tail")},{.value=named(VALUE_TAIL,"tail")}};
		struct value body;
		if (apply(e,fold->operands[position+1]->core,&env[4],suffix,fields,0,&body) || body.kind!=VALUE_PART) return -1;
		indent(e); fprintf(e->out,"result = %s;\n",body.name); --e->indent;
	}
	indent(e); fputs("} else { a->status = 2; goto done; }\ndone:\n\t--a->depth; return result;\n}\n\n"
		"static const struct qs_partition *gp_partition(struct qs_arena *a, const struct qs_nat_type *type, struct qs_compare le, uint32_t pivot, uint32_t size, const struct qs_sized_list *input)\n{\n"
		"\tstruct gp_capture capture = {type,le,pivot}; return gp_fold(a,&capture,size,input);\n}\n",e->out);
	return ferror(e->out) ? -1 : 0;
}

int pg_c_acc_partition_emit(FILE *source, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	if (!source || !storage || !typing || !root || !entries) return -1;
	FILE *stage=tmpfile(); if (!stage) return -1;
	struct pg_dag dag; if (pg_dag_init(&dag,subject_child,NULL)) { fclose(stage); return -1; }
	struct emitter e={.out=stage,.storage=storage,.dag=&dag,.entries=entries,.indent=1}; int status=-1;
	for (size_t i=0; i<8; ++i) { if (!entries[i].view.declaration) goto done; e.layouts[i]=pg_data_declaration_layout(entries[i].view.declaration); }
	if (pg_dag_add(&dag,root) || emit_to(&e,typing,root) || fseek(stage,0,SEEK_SET)) goto done;
	status=0; char buffer[4096]; size_t count;
	while ((count=fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,count,source)!=count) { status=-1; break; }
	if (ferror(stage) || ferror(source)) status=-1;
done:
	pg_dag_destroy(&dag); if (fclose(stage)) status=-1; return status;
}
