#include "artifact/file.h"
#include "classifier.h"
#include "host.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

static void view(const struct pg_term *type, const char *prefix)
{
	const struct pg_term *content, *domain, *codomain;
	const struct pg_object *binder;
	if (pg_thunk_type_view(type, &content)) {
		printf("%s thunk\n", prefix);
		view(content, prefix);
		return;
	}
	if (pg_pi_view(type, &domain, &binder, &codomain)) {
		printf("%s Pi constant-codomain=%d\n", prefix, pg_pi_constant_codomain(type) != NULL);
		view(domain, "domain");
		view(codomain, "codomain");
		return;
	}
	enum pg_totality totality;
	if (pg_pure_computation_type_view(type, &totality, &content)) {
		printf("%s pure-F total=%d\n", prefix, totality == PG_TOTALITY_TOTAL);
		view(content, "result");
		return;
	}
	printf("%s kind=%d Int32=%d Int64=%d\n", prefix, type->kind,
		type->kind == PG_REFERENCE && type->as.reference == pg_host_type("Int32"),
		type->kind == PG_REFERENCE && type->as.reference == pg_host_type("Int64"));
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	FILE *file = fopen(argv[1], "rb");
	assert(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = argv[2], .length = strlen(argv[2])};
	struct pg_synthesis_job *selected = pg_program_select_name(program, roots[0], token);
	uint64_t steps;
	assert(selected && pg_artifact_revalidate(program, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof, &program->typing));
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	printf("ordinary admission steps=%" PRIu64 " judgement=%d context=%d\n", steps, subject->judgement, subject->context != NULL);
	view(subject->classifier, "export");
	pg_program_destroy(program);
	return 0;
}
