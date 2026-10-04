#include "artifact/file.h"
#include "../indexed_views/view.h"
#include "computation.h"
#include "dag.h"
#include "identity.h"
#include <assert.h>
#include <string.h>

/* Read-only source diagnostic, not a lowerer or an Identity checker. */
struct printer {
	const struct pg_data_layout *layouts[8];
	const struct pg_object *binders[256];
	size_t count;
};
static int inspecting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *w, uint64_t n) { assert(!inspecting); return __real_pg_eval_advance(w,n); }
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n) { assert(!inspecting); return __real_pg_whnf_advance(w,n); }
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *w, uint64_t n) { assert(!inspecting); return __real_pg_substitution_advance(w,n); }
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *w, uint64_t n) { assert(!inspecting); return __real_pg_typed_query_advance(w,n); }

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

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *s=pg_program_select_name(p,root,token); uint64_t steps;
	assert(s && pg_artifact_revalidate(p,s,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(s);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

static size_t binder_id(struct printer *p, const struct pg_object *b)
{
	for (size_t i=0; i<p->count; ++i) if (p->binders[i]==b) return i;
	assert(p->count<256); p->binders[p->count]=b; return p->count++;
}

static void term(struct printer *p, const struct pg_term *t, unsigned depth)
{
	assert(t && depth<128);
	if (t->kind==PG_LAMBDA) { printf("(lambda v%zu ",binder_id(p,t->as.lambda.binder)); term(p,t->as.lambda.body,depth+1); putchar(')'); return; }
	if (t->kind==PG_APPLICATION) { putchar('('); term(p,t->as.application.function,depth+1); putchar(' '); term(p,t->as.application.argument,depth+1); putchar(')'); return; }
	const struct pg_object *b=t->as.reference;
	if (b->kind==PG_BINDER) { printf("v%zu",binder_id(p,b)); return; }
	const char *identity=pg_identity_name(b);
	if (identity) { fputs(identity,stdout); return; }
	static const struct { const struct pg_object *object; const char *name; } fixed[]={
		{&pg_return_operation,"RETURN"},{&pg_total_result_operation,"RESULT"},
		{&pg_thunk_operation,"THUNK"},{&pg_force_operation,"FORCE"},{&pg_fold_operation,"SEQ"}};
	for (size_t i=0; i<sizeof(fixed)/sizeof(*fixed); ++i) if (b==fixed[i].object) { fputs(fixed[i].name,stdout); return; }
	for (size_t i=0; i<8; ++i) {
		if (b==pg_data_matcher(p->layouts[i])) { printf("MATCH%zu",i); return; }
		size_t position;
		if (pg_data_constructor_position(p->layouts[i],b,&position)) { printf("CTOR%zu_%zu",i,position); return; }
	}
	printf("OPAQUE[%s]",b->owner ? b->owner->name : "unowned");
}

int main(int argc, char **argv)
{
	assert(argc==3 || argc==4); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	const struct pg_term *types[8];
	for (size_t i=0; i<8; ++i) types[i]=select_subject(p,roots[0],names[i])->core;
	const struct pg_occurrence *root=select_subject(p,roots[0],argv[2]);
	const struct pg_occurrence *reference=argc==4 ? select_subject(p,roots[0],argv[3]) : NULL;
	size_t terms=p->graph.terms.count,objects=p->graph.objects.count,proofs=p->typing.proofs.count,occurrences=p->typing.occurrences.count;
	struct printer printer={0}; struct pg_graph storage; assert(!pg_graph_init(&storage)); inspecting=1;
	for (size_t i=0; i<8; ++i) {
		struct pg_c_indexed_view view; assert(!pg_c_indexed_view_read(&storage,types[i],&view));
		printer.layouts[i]=pg_data_declaration_layout(view.declaration);
	}
	struct pg_dag dag; assert(!pg_dag_init(&dag,child,NULL) && !pg_dag_add(&dag,root));
	fputs("SELECTED_CLASSIFIER:",stdout); term(&printer,root->classifier,0); putchar('\n');
	if (reference) printf("SELECTED_CORE_ALPHA:%d\n",pg_alpha_equal(root->core,reference->core));
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (!s->induction) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs view;
		while ((proof=pg_evidence_for_subject(&p->typing,s,proof))) if (!pg_elimination_view(&p->typing,proof,&view)) break;
		if (!proof) continue;
		printf("FOLD%zu clauses%zu\nMOTIVE:",n->id,s->induction->count); term(&printer,s->classifier,0); putchar('\n');
		for (const struct pg_context *c=s->context; c; c=c->parent) {
			printf("CAPTURE v%zu:",binder_id(&printer,c->binder)); term(&printer,c->declared_type,0); putchar('\n');
		}
		for (size_t i=0; i<s->induction->count; ++i) {
			printf("CLAUSE%zu_CONTEXT:\n",i);
			for (const struct pg_context *c=s->induction->clauses[i]; c!=s->context; c=c->parent) {
				assert(c); printf("FIELD v%zu:",binder_id(&printer,c->binder)); term(&printer,c->declared_type,0); putchar('\n');
			}
			printf("CLAUSE%zu_CORE:",i); term(&printer,s->operands[i+1]->core,0); putchar('\n');
		}
	}
	assert(p->graph.terms.count==terms && p->graph.objects.count==objects && p->typing.proofs.count==proofs && p->typing.occurrences.count==occurrences);
	inspecting=0; printf("INERT terms=%zu objects=%zu proofs=%zu occurrences=%zu\n",terms,objects,proofs,occurrences);
	pg_dag_destroy(&dag); pg_graph_destroy(&storage); pg_program_destroy(p); return ferror(stdout) ? 2 : 0;
}
