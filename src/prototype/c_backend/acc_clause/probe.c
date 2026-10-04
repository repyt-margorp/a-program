#include "artifact/file.h"
#include "../indexed_views/view.h"
#include "computation.h"
#include "dag.h"
#include <assert.h>
#include <string.h>

struct printer {
	const struct pg_term *partition,*append;
	const struct pg_data_layout *layouts[8];
	const struct pg_object *binders[256];
	size_t count;
};

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *operation)
{
	if (t->kind!=PG_APPLICATION) return NULL;
	const struct pg_term *f=t->as.application.function;
	return f->kind==PG_REFERENCE && f->as.reference==operation ? t->as.application.argument : NULL;
}

static const struct pg_term *carrier(const struct pg_term *t)
{
	for (;;) {
		const struct pg_term *inner,*body;
		if ((inner=unary(t,&pg_total_result_operation)) || (inner=unary(t,&pg_return_operation))) { t=inner; continue; }
		if ((inner=unary(t,&pg_force_operation)) && (body=unary(inner,&pg_thunk_operation))) { t=body; continue; }
		return t;
	}
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

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *selected=pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

static size_t binder_id(struct printer *p, const struct pg_object *object)
{
	for (size_t i=0; i<p->count; ++i) if (p->binders[i]==object) return i;
	assert(p->count<256); p->binders[p->count]=object; return p->count++;
}

static void term(struct printer *p, const struct pg_term *t, unsigned depth)
{
	assert(t && depth<128);
	if (t->kind==PG_LAMBDA && pg_alpha_equal(t,p->partition)==1) { fputs("PARTITION",stdout); return; }
	if (t->kind==PG_LAMBDA && pg_alpha_equal(t,p->append)==1) { fputs("APPEND",stdout); return; }
	if (t->kind==PG_LAMBDA) {
		printf("(lambda v%zu ",binder_id(p,t->as.lambda.binder)); term(p,t->as.lambda.body,depth+1); putchar(')'); return;
	}
	if (t->kind==PG_APPLICATION) {
		putchar('('); term(p,t->as.application.function,depth+1); putchar(' '); term(p,t->as.application.argument,depth+1); putchar(')'); return;
	}
	const struct pg_object *object=t->as.reference;
	if (object->kind==PG_BINDER) { printf("v%zu",binder_id(p,object)); return; }
	static const struct { const struct pg_object *object; const char *name; } fixed[]={
		{&pg_return_operation,"RETURN"},{&pg_thunk_operation,"THUNK"},{&pg_force_operation,"FORCE"},
		{&pg_total_result_operation,"RESULT"},{&pg_fold_operation,"SEQ"}};
	for (size_t i=0; i<sizeof(fixed)/sizeof(*fixed); ++i) if (fixed[i].object==object) { fputs(fixed[i].name,stdout); return; }
	for (size_t i=0; i<8; ++i) {
		if (object==pg_data_matcher(p->layouts[i])) { printf("MATCH%zu",i); return; }
		size_t position;
		if (pg_data_constructor_position(p->layouts[i],object,&position)) { printf("CTOR%zu_%zu",i,position); return; }
	}
	printf("ATOM%p",(void *)object);
}

int main(int argc, char **argv)
{
	assert(argc==2); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	struct printer printer={.partition=select_subject(p,roots[0],"partition_fn")->core,.append=select_subject(p,roots[0],"append_fn")->core};
	printer.partition=carrier(printer.partition); printer.append=carrier(printer.append);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_graph storage; assert(!pg_graph_init(&storage));
	for (size_t i=0; i<8; ++i) {
		struct pg_c_indexed_view view; assert(!pg_c_indexed_view_read(&storage,select_subject(p,roots[0],names[i])->core,&view));
		printer.layouts[i]=pg_data_declaration_layout(view.declaration);
	}
	const struct pg_occurrence *root=select_subject(p,roots[0],"sort_acc"); struct pg_dag dag;
	assert(!pg_dag_init(&dag,subject_child,NULL) && !pg_dag_add(&dag,root));
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (!s->induction || s->induction->count!=1) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(&p->typing,s,proof))) if (!pg_elimination_view(&p->typing,proof,&view)) break;
		if (!proof) continue;
		const struct pg_context *clause=s->induction->clauses[0];
		printf("FOLD%zu context:",n->id);
		for (const struct pg_context *c=clause; c; c=c->parent) printf(" v%zu",binder_id(&printer,c->binder));
		putchar('\n'); term(&printer,s->operands[1]->core,0); putchar('\n');
	}
	pg_dag_destroy(&dag); pg_graph_destroy(&storage); pg_program_destroy(p); return 0;
}
