#include "artifact/file.h"
#include "link/plan.h"
#include "lower/scalar.h"
#include "selection.h"
#include <assert.h>
#include <string.h>

/* Admission is ordinary driver work. Only the target pass must remain inert. */
static int emitting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_eval_advance(work, budget);
}
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_substitution_advance(work, budget);
}
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_whnf_advance(work, budget);
}
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_typed_query_advance(work, budget);
}

static const struct pg_occurrence *admit(struct pg_program *program,
	struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(program, root, token);
	uint64_t steps;
	assert(selected && pg_artifact_revalidate(program, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof, &program->typing));
	return pg_evidence_subject(proof);
}

int main(int argc, char **argv)
{
	assert(argc == 4);
	struct pg_c_link_plan plan = {0};
	size_t line;
	const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error));
	FILE *file = fopen(plan.artifact, "rb");
	assert(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = admit(program, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = admit(program, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.natural_count; ++i) plan.naturals[i].subject = admit(program, roots[0], plan.natural_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = admit(program, roots[0], plan.data_names[i]);
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w");
	assert(source && header);
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	struct pg_c_native_contract contract = {0};
	emitting = 1;
	for (size_t i = 0; i < plan.data_count; ++i) if (plan.data_from_value[i]) {
		plan.data[i].subject = pg_c_value_classifier(plan.data[i].subject);
		assert(plan.data[i].subject);
	}
	assert(!pg_c_emit_native_profile(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, plan.natural_count, plan.naturals,
		plan.data_count, plan.data, &contract, &error));
	emitting = 0;
	assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
	assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header));
	pg_program_destroy(program);
	pg_c_link_destroy(&plan);
	puts("Applied type selection/emission: no evaluation, substitution, WHNF or typed-query advances; source graph/evidence unchanged");
	return 0;
}
