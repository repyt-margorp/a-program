#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
#include <assert.h>
#include <string.h>

static int emitting;
static size_t queries, fail_at;
int __real_pg_term_independent(const struct pg_term *, const struct pg_object *);
int __wrap_pg_term_independent(const struct pg_term *term, const struct pg_object *binder)
{
	if (emitting && ++queries == fail_at) return -1;
	return __real_pg_term_independent(term, binder);
}
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *w, uint64_t n) { assert(!emitting); return __real_pg_eval_advance(w,n); }
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n) { assert(!emitting); return __real_pg_whnf_advance(w,n); }
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *w, uint64_t n) { assert(!emitting); return __real_pg_substitution_advance(w,n); }
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *w, uint64_t n) { assert(!emitting); return __real_pg_typed_query_advance(w,n); }

int main(int argc, char **argv)
{
	assert(argc == 2);
	struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan,argv[1],&line,&error) && plan.lowering == PG_C_SCALAR_DIRECT);
	FILE *file = fopen(plan.artifact,"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file,SIZE_MAX,&count,&roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) {
		struct pg_token name = {.kind=PG_TOKEN_IDENT,.text=plan.names[i],.length=strlen(plan.names[i])};
		struct pg_synthesis_job *selected = pg_program_select_name(program,roots[0],name); uint64_t steps;
		assert(selected && pg_artifact_revalidate(program,selected,5000000,5000000,&steps) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *proof = pg_synthesis_result(selected);
		assert(pg_evidence_owned_by(proof,&program->typing)); plan.exports[i].subject = pg_evidence_subject(proof);
	}
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	FILE *source = tmpfile(), *header = tmpfile(); assert(source && header);
	emitting = 1;
	assert(!pg_c_emit_scalar(source,header,plan.count,plan.exports,SIZE_MAX,&error));
	emitting = 0; size_t total = queries; assert(total && !fclose(source) && !fclose(header));
	for (size_t fault = 1; fault <= total; ++fault) {
		source = tmpfile(); header = tmpfile(); assert(source && header);
		assert(fputs("KEPT\n",source)>=0 && fputs("KEPT\n",header)>=0);
		queries = 0; fail_at = fault; emitting = 1;
		assert(pg_c_emit_scalar(source,header,plan.count,plan.exports,SIZE_MAX,&error) < 0);
		emitting = 0;
		assert(queries == fault && ftell(source) == 5 && ftell(header) == 5);
		assert(!fclose(source) && !fclose(header));
		assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
		assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	}
	printf("Capture query faults:%zu; prior source/header and source owners unchanged\n",total);
	pg_program_destroy(program); pg_c_link_destroy(&plan); return 0;
}
