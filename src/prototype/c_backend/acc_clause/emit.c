#include "emit.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

enum value_kind { VALUE_NAT, VALUE_TYPE, VALUE_COMPARE, VALUE_ACCESS, VALUE_RAW,
	VALUE_IH, VALUE_LIST, VALUE_SIZED, VALUE_PARTITION, VALUE_LT };
struct value { enum value_kind kind; char name[96]; };
struct binding {
	const struct binding *parent;
	const struct pg_object *binder;
	struct value value;
};
struct emitter {
	FILE *out;
	const struct pg_data_layout *layouts[8];
	const struct pg_term *partition, *append;
	size_t serial, steps;
	unsigned indent;
};

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *operation)
{
	if (!t || t->kind != PG_APPLICATION) return NULL;
	const struct pg_term *f = t->as.application.function;
	return f->kind == PG_REFERENCE && f->as.reference == operation ? t->as.application.argument : NULL;
}

/* Only exact syntactic total carriers are removed, never arbitrary Force. */
static const struct pg_term *carrier(const struct pg_term *t)
{
	for (size_t step = 0; t && step < 1024; ++step) {
		const struct pg_term *inner, *body;
		if ((inner = unary(t,&pg_total_result_operation)) || (inner = unary(t,&pg_return_operation))) { t = inner; continue; }
		if ((inner = unary(t,&pg_force_operation)) && (body = unary(inner,&pg_thunk_operation))) { t = body; continue; }
		return t;
	}
	return NULL;
}

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s = key;
	if (slot < s->operand_count) { *out = s->operands[slot]; return *out ? 1 : 2; }
	slot -= s->operand_count;
	if (!slot) { *out = s->type; return *out ? 1 : 2; }
	if (slot == 1) { *out = s->origin; return *out ? 1 : 2; }
	slot -= 2; const struct pg_context_map *const *maps = pg_occurrence_maps(s);
	for (size_t i = 0; i <= s->map_count; ++i) {
		const struct pg_context_map *map = i ? maps[i-1] : s->map; if (!map) continue;
		if (slot < map->count) { *out = map->images[slot]; return *out ? 1 : 2; }
		slot -= map->count;
	}
	return 0;
}

static void indent(struct emitter *e)
{
	for (unsigned i = 0; i < e->indent; ++i) fputc('\t',e->out);
}

static const char *ctype(enum value_kind kind)
{
	switch (kind) {
	case VALUE_NAT: return "uint32_t";
	case VALUE_TYPE: return "const struct qs_nat_type *";
	case VALUE_COMPARE: return "struct qs_compare";
	case VALUE_ACCESS: return "const struct qs_acc *";
	case VALUE_RAW: return "struct qs_acc_down";
	case VALUE_IH: return "struct qs_folded_down";
	case VALUE_LIST: return "const struct qs_list *";
	case VALUE_SIZED: return "const struct qs_sized_list *";
	case VALUE_PARTITION: return "const struct qs_partition *";
	case VALUE_LT: return "const struct qs_lt *";
	}
	return NULL;
}

static struct value named(enum value_kind kind, const char *name)
{
	struct value v = {.kind = kind}; snprintf(v.name,sizeof(v.name),"%s",name); return v;
}

static struct value temporary(struct emitter *e, enum value_kind kind)
{
	struct value v = {.kind = kind}; snprintf(v.name,sizeof(v.name),"v%zu",e->serial++); return v;
}

static int field_name(struct value *out, struct value input, const char *field)
{
	size_t n = strlen(input.name), m = strlen(field);
	if (n + m + 2 >= sizeof(out->name)) return -1;
	memcpy(out->name,input.name,n); memcpy(out->name+n,"->",2);
	memcpy(out->name+n+2,field,m+1); return 0;
}

static void pointer_guard(struct emitter *e, struct value v)
{
	indent(e); fprintf(e->out,"if (!%s || a->status) goto done;\n",v.name);
}

static int expression(struct emitter *, const struct pg_term *, const struct binding *, unsigned, struct value *);

static int branch(struct emitter *e, const struct pg_term *term, const struct binding *env,
	size_t count, const struct value *values, unsigned depth, struct value *result)
{
	if (!count) return expression(e,term,env,depth+1,result);
	if (term->kind != PG_LAMBDA) return -1;
	struct binding b = {env,term->as.lambda.binder,values[0]};
	return branch(e,term->as.lambda.body,&b,count-1,values+1,depth+1,result);
}

static int match(struct emitter *e, size_t family, size_t count, const struct pg_term *const *args,
	const struct binding *env, unsigned depth, struct value *result)
{
	struct value input;
	if (expression(e,args[0],env,depth+1,&input)) return -1;
	struct value joined = temporary(e,VALUE_LIST);
	indent(e); fprintf(e->out,"%s %s = NULL;\n",ctype(joined.kind),joined.name);
	if (family == 4) {
		if (count != 5 || input.kind != VALUE_SIZED) return -1;
		indent(e); fprintf(e->out,"if (!index_check(a,%s && %s->element_type == f->element_type)) goto done;\n",input.name,input.name);
		struct value extra[2];
		for (size_t i = 0; i < 2; ++i) if (expression(e,args[i+3],env,depth+1,&extra[i])) return -1;
		if (extra[0].kind != VALUE_RAW || extra[1].kind != VALUE_IH) return -1;
		indent(e); fprintf(e->out,"/* Source SizedList Match; extra original-down/IH arguments stay scoped. */\n");
		indent(e); fprintf(e->out,"if (%s->tag == QS_NIL) {\n",input.name); ++e->indent;
		indent(e); fprintf(e->out,"if (!index_check(a,%s->index == 0)) goto done;\n",input.name);
		struct value nil;
		if (branch(e,args[1],env,2,extra,depth+1,&nil) || nil.kind != VALUE_LIST) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",joined.name,nil.name); --e->indent;
		indent(e); fprintf(e->out,"} else if (%s->tag == QS_CONS) {\n",input.name); ++e->indent;
		indent(e); fprintf(e->out,"if (!index_check(a,succ(a,%s->tail_size) == %s->index)) goto done;\n",input.name,input.name);
		struct value fields[5] = {{.kind=VALUE_NAT},{.kind=VALUE_NAT},{.kind=VALUE_SIZED},extra[0],extra[1]};
		if (field_name(&fields[0],input,"tail_size") || field_name(&fields[1],input,"head")
			|| field_name(&fields[2],input,"tail")) return -1;
		struct value cons;
		if (branch(e,args[2],env,5,fields,depth+1,&cons) || cons.kind != VALUE_LIST) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",joined.name,cons.name); --e->indent;
		indent(e); fputs("} else { a->status = 2; goto done; }\n",e->out);
	} else if (family == 6) {
		if (count != 2 || input.kind != VALUE_PARTITION) return -1;
		indent(e); fprintf(e->out,"if (!index_check(a,%s && %s->element_type == f->element_type)) goto done;\n",input.name,input.name);
		const char *names[] = {"lower_size","lower","upper_size","upper","lower_bound","upper_bound"};
		const enum value_kind kinds[] = {VALUE_NAT,VALUE_SIZED,VALUE_NAT,VALUE_SIZED,VALUE_LT,VALUE_LT};
		struct value fields[6];
		for (size_t i = 0; i < 6; ++i) {
			fields[i].kind = kinds[i]; if (field_name(&fields[i],input,names[i])) return -1;
		}
		indent(e); fputs("{ /* Source Partition.parts fields in declaration order. */\n",e->out); ++e->indent;
		struct value parts;
		if (branch(e,args[1],env,6,fields,depth+1,&parts) || parts.kind != VALUE_LIST) return -1;
		indent(e); fprintf(e->out,"%s = %s;\n",joined.name,parts.name); --e->indent;
		indent(e); fputs("}\n",e->out);
	} else return -1;
	*result = joined; return 0;
}

static int expression(struct emitter *e, const struct pg_term *term, const struct binding *env,
	unsigned depth, struct value *result)
{
	if (!term || depth > 128 || ++e->steps > 4096) return -1;
	const struct pg_term *returned = unary(term,&pg_return_operation);
	if (returned) return expression(e,returned,env,depth+1,result);
	const struct pg_term *args[16], *head = term; size_t count = 0;
	while (head->kind == PG_APPLICATION) {
		if (count == 16) return -1;
		args[count++] = head->as.application.argument; head = head->as.application.function;
	}
	for (size_t i = 0; i < count/2; ++i) { const struct pg_term *t = args[i]; args[i] = args[count-i-1]; args[count-i-1] = t; }
	if (head->kind == PG_REFERENCE && head->as.reference->kind == PG_BINDER) {
		if (count) return -1;
		for (const struct binding *b = env; b; b = b->parent) if (b->binder == head->as.reference) { *result = b->value; return 0; }
		return -1;
	}
	if (head->kind == PG_REFERENCE && head->as.reference == &pg_fold_operation) {
		if (count != 2 || args[1]->kind != PG_LAMBDA) return -1;
		struct value first;
		if (expression(e,args[0],env,depth+1,&first)) return -1;
		/* Source SEQ executes its computation before the continuation. */
		struct binding b = {env,args[1]->as.lambda.binder,first};
		return expression(e,args[1]->as.lambda.body,&b,depth+1,result);
	}
	/* Full application flattening leaves Force as its head with the forced
		* value in args[0]. Only exact helper carriers or the known IH are allowed. */
	if (head->kind == PG_REFERENCE && head->as.reference == &pg_force_operation) {
		if (!count) return -1;
		const struct pg_term *body = unary(args[0],&pg_thunk_operation);
		if (body) {
			body = carrier(body); struct value v[5];
			if (!body || body->kind != PG_LAMBDA) return -1;
			int partition = pg_alpha_equal(body,e->partition) == 1;
			int append = pg_alpha_equal(body,e->append) == 1;
			if ((!partition && !append) || count != (partition ? 6u : 4u)) return -1;
			for (size_t i = 1; i < count; ++i) if (expression(e,args[i],env,depth+1,&v[i-1])) return -1;
			if (v[0].kind != VALUE_TYPE) return -1;
			if (partition && (v[1].kind != VALUE_COMPARE || v[2].kind != VALUE_NAT || v[3].kind != VALUE_NAT || v[4].kind != VALUE_SIZED)) return -1;
			if (append && (v[1].kind != VALUE_LIST || v[2].kind != VALUE_LIST)) return -1;
			struct value out = temporary(e,partition ? VALUE_PARTITION : VALUE_LIST);
			indent(e); fprintf(e->out,"%s %s = %s(a",ctype(out.kind),out.name,partition ? "qs_partition" : "append");
			for (size_t i = 0; i < count-1; ++i) fprintf(e->out,",%s",v[i].name);
			fputs(");\n",e->out); pointer_guard(e,out); *result = out; return 0;
		}
		if (count != 4) return -1;
		struct value v[4];
		for (size_t i = 0; i < 4; ++i) if (expression(e,args[i],env,depth+1,&v[i])) return -1;
		if (v[0].kind != VALUE_IH || v[1].kind != VALUE_NAT || v[2].kind != VALUE_LT || v[3].kind != VALUE_SIZED) return -1;
		size_t call = e->serial++; struct value out = temporary(e,VALUE_LIST);
		indent(e); fprintf(e->out,"struct qs_sort_closure call%zu = gs_force_down(a,&%s,%s,%s);\n",call,v[0].name,v[1].name,v[2].name);
		indent(e); fputs("if (a->status) goto done;\n",e->out);
		indent(e); fprintf(e->out,"%s %s = gs_apply(a,&call%zu,%s);\n",ctype(out.kind),out.name,call,v[3].name);
		pointer_guard(e,out); *result = out; return 0;
	}
	if (head->kind == PG_LAMBDA) {
		if (!count) return -1;
		struct value values[16];
		for (size_t i = 0; i < count; ++i) if (expression(e,args[i],env,depth+1,&values[i])) return -1;
		return branch(e,head,env,count,values,depth+1,result);
	}
	if (head->kind != PG_REFERENCE) return -1;
	const struct pg_object *object = head->as.reference;
	for (size_t i = 0; i < 8; ++i) if (object == pg_data_matcher(e->layouts[i])) return count ? match(e,i,count,args,env,depth+1,result) : -1;
	size_t position;
	if (!pg_data_constructor_position(e->layouts[7],object,&position) || position > 1 || count != (position ? 2u : 0u)) return -1;
	struct value out = temporary(e,VALUE_LIST);
	if (!position) {
		indent(e); fprintf(e->out,"%s %s = list_nil(a,f->element_type);\n",ctype(out.kind),out.name);
	} else {
		struct value v[2];
		for (size_t i = 0; i < 2; ++i) if (expression(e,args[i],env,depth+1,&v[i])) return -1;
		if (v[0].kind != VALUE_NAT || v[1].kind != VALUE_LIST) return -1;
		indent(e); fprintf(e->out,"%s %s = list_cons(a,f->element_type,%s,%s);\n",ctype(out.kind),out.name,v[0].name,v[1].name);
	}
	pointer_guard(e,out); *result = out; return 0;
}

static int emit_to(FILE *source, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_term *partition, const struct pg_term *append,
	const struct pg_c_indexed_entry *entries)
{
	if (!source || !storage || !typing || !root || !partition || !append || !entries) return -1;
	/* Reuse the sealed C35 target-only applicability reader for motive/index/
		* callable fields. Its generated plumbing is not this clause's source. */
	FILE *a = tmpfile(), *b = tmpfile();
	if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return -1; }
	int valid = !pg_c_acc_fold_emit(a,b,storage,typing,root,8,entries);
	int close_a = fclose(a), close_b = fclose(b);
	if (!valid || close_a || close_b) return -1;
	struct emitter e = {.out=source,.partition=carrier(partition),.append=carrier(append),.indent=1};
	if (!e.partition || !e.append || e.partition->kind != PG_LAMBDA || e.append->kind != PG_LAMBDA) return -1;
	for (size_t i = 0; i < 8; ++i) e.layouts[i] = pg_data_declaration_layout(entries[i].view.declaration);
	/* Fixed private C33 representation is applicable only at these actual
		* declaration/layout shapes; no arbitrary indexed data is inferred. */
	const size_t families[] = {4,6,7}, counts[] = {2,1,2}, arities[][2] = {{0,3},{6,0},{0,2}};
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_data_layout *layout = e.layouts[families[i]];
		if (pg_data_layout_count(layout) != counts[i]) return -1;
		for (size_t j = 0; j < counts[i]; ++j) {
			const struct pg_data_layout *actual; size_t position, arity;
			if (!pg_data_constructor_view(pg_data_constructor(layout,j),&actual,&position,&arity)
				|| actual != layout || position != j || arity != arities[i][j]) return -1;
		}
	}
	struct pg_dag dag; if (pg_dag_init(&dag,subject_child,NULL)) return -1;
	int status = -1; const struct pg_occurrence *fold = NULL;
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n = dag.first; n; n = n->next) {
		const struct pg_occurrence *s = n->key;
		size_t captures;
		if (s->induction && s->induction->count == 1 && s->context
			&& !pg_context_extension_size(s->context,NULL,&captures) && captures == 4) {
			const struct pg_evidence *proof = NULL; struct pg_elimination_inputs view;
			while ((proof = pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
			if (!proof) continue;
			if (fold) goto done;
			fold = s;
		}
	}
	if (!fold) goto done;
	const struct pg_context *ih = fold->induction->clauses[0], *down = ih->parent, *current = down->parent;
	const struct pg_context *access = fold->context, *size = access->parent, *comparison = size->parent, *element = comparison->parent;
	const struct pg_term *body = fold->operands[1]->core;
	const struct pg_object *suffix[] = {current->binder,down->binder,ih->binder};
	for (size_t i = 0; i < 3; ++i) {
		if (body->kind != PG_LAMBDA || body->as.lambda.binder != suffix[i]) goto done;
		body = body->as.lambda.body;
	}
	if (body->kind != PG_LAMBDA) goto done;
	struct binding env[] = {
		{NULL,element->binder,named(VALUE_TYPE,"f->element_type")},
		{NULL,comparison->binder,named(VALUE_COMPARE,"f->comparison")},
		{NULL,size->binder,named(VALUE_NAT,"f->input_index")},
		{NULL,access->binder,named(VALUE_ACCESS,"access")},
		{NULL,current->binder,named(VALUE_NAT,"access->current")},
		{NULL,down->binder,named(VALUE_RAW,"original")},
		{NULL,ih->binder,named(VALUE_IH,"down")},
		{NULL,body->as.lambda.binder,named(VALUE_SIZED,"input")}
	};
	for (size_t i = 1; i < 8; ++i) env[i].parent = &env[i-1];
	fputs("/* Generated from actual admitted Acc clause terms. C33 representations,\n\t* accessibility/partition/append/comparison and outer entry remain manual. */\n"
		"static const struct qs_list *gs_apply(struct qs_arena *, const struct qs_sort_closure *, const struct qs_sized_list *);\n\n"
		"static struct qs_sort_closure gs_fold(const struct qs_nat_type *type, struct qs_compare le, uint32_t size, const struct qs_acc *access)\n{\n"
		"\treturn (struct qs_sort_closure){type,le,access,size};\n}\n\n"
		"static struct qs_sort_closure gs_force_down(struct qs_arena *a, const struct qs_folded_down *down, uint32_t y, const struct qs_lt *edge)\n{\n"
		"\t++a->trace.folded_down;\n"
		"\tif (!index_check(a,down && edge && edge->left == y && edge->right == down->parent_index && down->original.call)) return (struct qs_sort_closure){0};\n"
		"\tconst struct qs_acc *child = down->original.call(a,down->original.context,y,edge);\n"
		"\tif (!index_check(a,child && child->domain == &nat_type && child->relation == &lt_relation && child->subject == y && child->current == y)) return (struct qs_sort_closure){0};\n"
		"\treturn gs_fold(down->element_type,down->comparison,y,child);\n}\n\n"
		"static const struct qs_list *gs_apply(struct qs_arena *a, const struct qs_sort_closure *f, const struct qs_sized_list *input)\n{\n"
		"\tconst struct qs_list *result = NULL; if (!enter(a)) return NULL;\n"
		"\tconst struct qs_acc *access = f->access;\n"
		"\tif (!index_check(a,f->element_type == &nat_type && f->comparison.call && access && access->domain == &nat_type && access->relation == &lt_relation &&\n"
		"\t\taccess->subject == f->input_index && access->current == f->input_index && input && input->element_type == f->element_type && input->index == access->current)) goto done;\n"
		"\t++a->trace.acc_branch;\n\tstruct qs_acc_down original = access->down;\n"
		"\tstruct qs_folded_down down = {f->element_type,f->comparison,original,access->current};\n\t(void)down;\n",source);
	struct value result;
	if (expression(&e,body->as.lambda.body,&env[7],0,&result) || result.kind != VALUE_LIST) goto done;
	indent(&e); fprintf(source,"result = %s;\ndone:\n\t--a->depth; return result;\n}\n",result.name);
	status = ferror(source) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}

int pg_c_acc_clause_emit(FILE *source, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_term *partition, const struct pg_term *append,
	const struct pg_c_indexed_entry *entries)
{
	if (!source) return -1;
	FILE *stage = tmpfile(); if (!stage) return -1;
	int status = emit_to(stage,storage,typing,root,partition,append,entries);
	if (!status && !fseek(stage,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count = fread(buffer,1,sizeof(buffer),stage))) if (fwrite(buffer,1,count,source) != count) { status = -1; break; }
		if (ferror(stage) || ferror(source)) status = -1;
	} else status = -1;
	if (fclose(stage)) status = -1;
	return status;
}
