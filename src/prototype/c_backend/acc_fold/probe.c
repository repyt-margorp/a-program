#include "artifact/file.h"
#include "classifier.h"
#include "dag.h"
#include "iadt.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s = key;
	if (slot < s->operand_count) { *out = s->operands[slot]; return *out ? 1 : 2; }
	slot -= s->operand_count;
	if (!slot) { *out = s->type; return *out ? 1 : 2; }
	if (slot == 1) { *out = s->origin; return *out ? 1 : 2; }
	slot -= 2; const struct pg_context_map *const *maps = pg_occurrence_maps(s);
	for (size_t i = 0; i <= s->map_count; ++i) {
		const struct pg_context_map *map = i ? maps[i-1] : s->map;
		if (!map) continue;
		if (slot < map->count) { *out = map->images[slot]; return *out ? 1 : 2; }
		slot -= map->count;
	}
	return 0;
}

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

/* Pointer labels are local diagnostics only, never saved source identities. */
static void term(const struct pg_term *t, unsigned depth)
{
	if (!t) { fputs("NULL",stdout); return; }
	if (depth == 12) { fputs("...",stdout); return; }
	const struct pg_term *domain,*codomain,*inner; const struct pg_object *binder; uint64_t level; enum pg_totality totality;
	if (pg_universe_level(t,&level)) { printf("U%" PRIu64,level); return; }
	if (pg_pi_view(t,&domain,&binder,&codomain)) {
		printf("Pi(%p:",(void *)binder); term(domain,depth+1); fputs(",",stdout); term(codomain,depth+1); fputs(")",stdout); return;
	}
	if (pg_thunk_type_view(t,&inner)) { fputs("Thunk(",stdout); term(inner,depth+1); fputs(")",stdout); return; }
	if (pg_pure_computation_type_view(t,&totality,&inner)) { printf("Pure%d(",totality); term(inner,depth+1); fputs(")",stdout); return; }
	if (t->kind == PG_REFERENCE) {
		const struct pg_data_declaration *d = pg_data_declaration_view(t->as.reference);
		printf("%s%p",d ? "Family" : "Ref",(void *)t->as.reference);
	} else if (t->kind == PG_APPLICATION) {
		fputs("App(",stdout); term(t->as.application.function,depth+1); fputs(",",stdout); term(t->as.application.argument,depth+1); fputs(")",stdout);
	} else {
		printf("Lambda(%p,",(void *)t->as.lambda.binder); term(t->as.lambda.body,depth+1); fputs(")",stdout);
	}
}

static void context(const struct pg_context *c)
{
	size_t count; assert(!pg_context_extension_size(c,NULL,&count)); printf("context%zu\n",count);
	for (; c; c=c->parent) { printf("  binder%p judge%d ",(void *)c->binder,c->judgement); term(c->declared_type,0); putchar('\n'); }
}

int main(int argc, char **argv)
{
	assert(argc == 2); FILE *file = fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[] = {"sort_acc","natural_access","successor_access","partition_nat","sort"};
	for (size_t i = 0; i < 5; ++i) {
		const struct pg_occurrence *root = select_subject(p,roots[0],names[i]);
		printf("ROOT %s classifier ",names[i]); term(root->classifier,0); putchar('\n');
		struct pg_dag dag; assert(!pg_dag_init(&dag,subject_child,NULL) && !pg_dag_add(&dag,root)); size_t direct = 0;
		for (const struct pg_dag_node *n = dag.first; n; n=n->next) {
			const struct pg_occurrence *s = n->key; if (!s->induction) continue;
			struct pg_elimination_inputs view; const struct pg_evidence *proof = NULL;
			while ((proof = pg_evidence_for_subject(&p->typing,s,proof))) if (!pg_elimination_view(&p->typing,proof,&view)) break;
			if (!proof) continue;
			++direct; printf("direct id%zu clauses%zu operands%zu classifier ",n->id,s->induction->count,s->operand_count); term(s->classifier,0); putchar('\n');
			printf("  scrutinee "); term(pg_evidence_classifier(view.scrutinee),0); putchar('\n');
			printf("  motive "); term(pg_evidence_subject(view.motive)->core,0); putchar('\n');
			for (size_t j = 0; j < s->induction->count; ++j) { printf(" clause%zu ",j); context(s->induction->clauses[j]); }
		}
		printf("TOTAL %s nodes%zu direct%zu\n",names[i],dag.count,direct); pg_dag_destroy(&dag);
	}
	pg_program_destroy(p); return 0;
}
