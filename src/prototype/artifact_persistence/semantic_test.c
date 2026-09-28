#include "source_io.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int inspect_semantic_root(const struct pg_occurrence *root);
static int solving;

void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget)
{
	assert(solving);
	__real_pg_synthesis_advance(synthesis, budget);
}

enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(solving);
	return __real_pg_whnf_advance(work, budget);
}

enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(solving);
	return __real_pg_substitution_advance(work, budget);
}

static void advance(struct pg_program *program, uint64_t budget)
{
	solving = 1;
	pg_synthesis_advance(&program->synthesis, budget);
	solving = 0;
}

static void equal_files(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int left, right;
	do {
		left = fgetc(a); right = fgetc(b);
		assert(left == right);
	} while (left != EOF);
	assert(!ferror(a) && !ferror(b));
}

static FILE *save(struct pg_program *program, size_t count, struct pg_synthesis_job *const *roots)
{
	size_t terms = program->graph.terms.count, proofs = program->typing.proofs.count;
	size_t occurrences = program->typing.occurrences.count, jobs = program->synthesis.jobs.count;
	uint64_t steps = program->synthesis.steps;
	FILE *file = tmpfile();
	assert(file && !pg_sources_write(file, &program->synthesis, count, roots));
	assert(terms == program->graph.terms.count && proofs == program->typing.proofs.count);
	assert(occurrences == program->typing.occurrences.count && jobs == program->synthesis.jobs.count);
	assert(steps == program->synthesis.steps);
	rewind(file);
	return file;
}

static void cycles(uint64_t budget)
{
	const char *source = "Nat := @{zero:*;succ:*->*;}; id := \\x:Nat=>x; main := id Nat.zero;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token main = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected = pg_program_select_name(program, program->root, main);
	assert(selected);
	struct pg_synthesis_job *initial[] = {selected, program->root, selected};
	struct pg_synthesis_job *const *roots = initial;
	size_t count = 3;
	advance(program, budget);
	const struct pg_occurrence *before;
	int available = pg_synthesis_materialized(roots[0], &before);
	assert(available >= 0 && inspect_semantic_root(before) == available);
	if (!budget) assert(!available);
	if (budget == 100000) assert(available && pg_synthesis_result(selected));
	FILE *file = save(program, count, roots);
	size_t unchanged = 7;
	struct pg_synthesis_job *const *missing = NULL;
	assert(!pg_sources_read(file, 1, &unchanged, &missing) && unchanged == 7 && !missing);
	rewind(file);
	for (size_t i = 0; i < 3; ++i) {
		pg_program_destroy(program);
		program = pg_sources_read(file, 1000000, &count, &roots);
		assert(program && count == 3 && roots[0] == roots[2]);
		assert(!program->synthesis.steps && !pg_synthesis_result(roots[0]));
		const struct pg_occurrence *view;
		size_t proofs = program->typing.proofs.count, jobs = program->synthesis.jobs.count;
		assert(pg_synthesis_materialized(roots[0], &view) == available);
		assert(inspect_semantic_root(view) == available);
		assert(proofs == program->typing.proofs.count && jobs == program->synthesis.jobs.count);
		FILE *again = save(program, count, roots);
		equal_files(file, again);
		assert(!fclose(file));
		file = again;
		rewind(file);
	}
	advance(program, 100000);
	assert(pg_synthesis_result(roots[0]) && pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void untrusted_input(void)
{
	const char *source = "main := #0 :: #Text;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	const struct pg_evidence *proof = pg_prove_universe(&program->typing,
		pg_prove_empty_context(&program->typing), 0);
	assert(proof);
	const struct pg_occurrence *forged = pg_evidence_subject(proof);
	struct pg_program *foreign = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(foreign && pg_synthesis_import_materialized(&foreign->synthesis, program->root, forged));
	pg_program_destroy(foreign);
	assert(!pg_synthesis_import_materialized(&program->synthesis, program->root, forged));
	const struct pg_evidence *other = pg_prove_universe(&program->typing,
		pg_prove_empty_context(&program->typing), 1);
	assert(other && pg_synthesis_import_materialized(&program->synthesis, program->root, pg_evidence_subject(other)));
	assert(!pg_synthesis_result(program->root));
	FILE *file = save(program, 1, &program->root);
	pg_program_destroy(program);
	size_t count = 0;
	struct pg_synthesis_job *const *roots;
	program = pg_sources_read(file, 100000, &count, &roots);
	assert(program && count == 1 && !program->synthesis.steps && !pg_synthesis_result(roots[0]));
	const struct pg_occurrence *view;
	assert(pg_synthesis_materialized(roots[0], &view) == 1 && inspect_semantic_root(view));
	advance(program, 100000);
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_result(roots[0]));
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void rejected_sibling(void)
{
	const char *source = "ok := #0; bad := #0 :: #Text;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "ok", .length = 2};
	struct pg_synthesis_job *checked = pg_program_select_name(program, program->root, name);
	assert(checked);
	struct pg_synthesis_job *local = NULL;
	while (!local || !pg_synthesis_result(local)) {
		assert(program->synthesis.steps < 100000);
		advance(program, 1);
		local = pg_synthesis_definition(program->root, name);
	}
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(checked));
	struct pg_synthesis_job *initial[] = {local, checked, program->root};
	FILE *file = save(program, 3, initial);
	pg_program_destroy(program);
	size_t count;
	struct pg_synthesis_job *const *roots;
	program = pg_sources_read(file, 100000, &count, &roots);
	assert(program && count == 3 && !program->synthesis.steps);
	const struct pg_occurrence *view;
	assert(pg_synthesis_materialized(roots[0], &view) == 1 && inspect_semantic_root(view));
	assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
	advance(program, 100000);
	assert(pg_synthesis_result(roots[0]));
	assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(roots[1]));
	assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED);
	assert(!fclose(file));
	pg_program_destroy(program);
}

int main(void)
{
	cycles(0); cycles(100); cycles(100000);
	untrusted_input();
	rejected_sibling();
	puts("semantic source image: inert views, aliases, stable cycles, untrusted inputs and module obligations passed");
	return 0;
}
