#include "seed.h"
#include "source_io.h"

int pg_seed_write(FILE *file, const char *source, size_t length,
	enum pg_definition_policy policy)
{
	if (!file) return -1;
	struct pg_program *program = pg_program_create(source, length, policy);
	if (!program) return -1;
	int status = program->root ? pg_sources_write(file, &program->synthesis, 1, &program->root) : -1;
	pg_program_destroy(program);
	return status;
}

struct pg_program *pg_seed_read(FILE *file, size_t limit)
{
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_sources_read(file, limit, &count, &roots);
	if (!program) return NULL;
	if (count != 1) goto fail;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(&program->synthesis, roots[0], &scope, &syntax)) goto fail;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (syntax->kind != PG_SYNTAX_DEFINITIONS) goto fail;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}
