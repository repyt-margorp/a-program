#include "artifact/file.h"
#include "classifier.h"
#include "iadt.h"
#include "view.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT,.text = name,.length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

/* Read only existing descriptive shapes; addresses are diagnostic identities,
	* not persisted names or source membership evidence. */
static void term(const struct pg_term *t, unsigned depth)
{
	if (!t) { fputs("NULL",stdout); return; }
	if (depth == 8) { fputs("...",stdout); return; }
	uint64_t level; const struct pg_term *domain, *codomain, *inner; const struct pg_object *binder;
	enum pg_totality grade;
	if (pg_universe_level(t,&level)) { printf("U%" PRIu64,level); return; }
	if (pg_pi_view(t,&domain,&binder,&codomain)) {
		printf("Pi(%p:",(void *)binder); term(domain,depth+1); fputs(",",stdout); term(codomain,depth+1); fputs(")",stdout); return;
	}
	if (pg_thunk_type_view(t,&inner)) { fputs("Thunk(",stdout); term(inner,depth+1); fputs(")",stdout); return; }
	if (pg_pure_computation_type_view(t,&grade,&inner)) { printf("Pure%d(",grade); term(inner,depth+1); fputs(")",stdout); return; }
	if (t->kind == PG_APPLICATION) {
		fputs("App(",stdout); term(t->as.application.function,depth+1); fputs(",",stdout); term(t->as.application.argument,depth+1); fputs(")",stdout);
	} else if (t->kind == PG_LAMBDA) {
		printf("Lambda(%p,",(void *)t->as.lambda.binder); term(t->as.lambda.body,depth+1); fputs(")",stdout);
	} else {
		const struct pg_data_declaration *d = pg_data_declaration_view(t->as.reference);
		printf("%s%p",d ? "Family" : "Ref",(void *)t->as.reference);
	}
}

static void context(const char *name, const struct pg_context *c, const struct pg_context *prefix)
{
	size_t count; assert(!pg_context_extension_size(c,prefix,&count)); printf(" %s fields%zu\n",name,count);
	for (; c != prefix; c = c->parent) {
		printf("  binder%p judge%d ",(void *)c->binder,c->judgement); term(c->declared_type,0); putchar('\n');
	}
}

int main(int argc, char **argv)
{
	assert(argc == 2); FILE *image = fopen(argv[1],"rb"); assert(image);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(image,SIZE_MAX,&count,&roots); assert(!fclose(image) && p && count);
	struct pg_graph storage; assert(!pg_graph_init(&storage));
	const char *names[] = {"nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	for (size_t i = 0; i < sizeof(names)/sizeof(*names); ++i) {
		const struct pg_occurrence *s = select_subject(p,roots[0],names[i]);
		printf("%s judge%d core:",names[i],s->judgement); term(s->core,0); fputs(" classifier:",stdout); term(s->classifier,0); putchar('\n');
		struct pg_c_indexed_view view;
		if (pg_c_indexed_view_read(&storage,s->core,&view)) { puts(" unsupported factory shape"); continue; }
		const struct pg_data_declaration *d = view.declaration;
		printf(" retained closed parameters%zu\n",view.count);
		for (size_t j = 0; j < view.count; ++j) { printf("  parameter%zu ",j); term(pg_c_indexed_bound(view.arguments[j]).term,0); putchar('\n'); }
		const struct pg_context *prefix = pg_data_declaration_parameters(d); context("parameters",prefix,NULL);
		context("indices",pg_data_declaration_indices(d),prefix);
		const struct pg_data_layout *layout = pg_data_declaration_layout(d);
		for (size_t j = 0; j < pg_data_layout_count(layout); ++j) {
			printf(" constructor%zu\n",j); context("constructor",pg_data_declaration_fields(d,j),prefix);
		}
		size_t nc, nt; const struct pg_context *const *contexts; const struct pg_term *const *terms;
		assert(!pg_data_declaration_pack(d,&storage,&nc,&contexts,&nt,&terms)); printf(" packed contexts%zu terms%zu\n",nc,nt);
		for (size_t j = 1; j < nt; ++j) { printf("  image%zu ",j); term(terms[j],0); putchar('\n'); }
	}
	pg_graph_destroy(&storage); pg_program_destroy(p); return 0;
}
