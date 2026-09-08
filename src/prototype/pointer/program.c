#include "program.h"

#include <stdlib.h>
#include <string.h>

struct pg_program *pg_program_create(const char *source, size_t length,
	enum pg_definition_policy policy)
{
	if (!source && length) return NULL;
	struct pg_program *program = calloc(1, sizeof(*program));
	if (!program) return NULL;
	if (pg_graph_init(&program->graph) != 0) goto fail;
	if (pg_typing_init(&program->typing, &program->graph) != 0) goto fail;
	if (pg_classifiers_init(&program->classifiers, &program->graph) != 0) goto fail;
	if (pg_whnf_work_init(&program->evaluation, &program->graph) != 0) goto fail;
	if (pg_synthesis_init(&program->synthesis, &program->typing, &program->classifiers,
		&program->evaluation, policy) != 0) goto fail;
	char *copy = pg_alloc(&program->graph, length ? length : 1);
	if (!copy) goto fail;
	if (length) memcpy(copy, source, length);
	pg_parser_init(&program->parser, &program->graph, copy, length);
	program->syntax = pg_parser_program(&program->parser);
	if (!program->syntax) return program;
	program->scope = pg_synthesis_root(&program->synthesis);
	if (!program->scope) goto fail;
	program->root = pg_synthesis_request(&program->synthesis, program->scope, program->syntax);
	if (!program->root) goto fail;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}

void pg_program_destroy(struct pg_program *program)
{
	if (!program) return;
	pg_synthesis_destroy(&program->synthesis);
	pg_whnf_work_destroy(&program->evaluation);
	pg_classifiers_destroy(&program->classifiers);
	pg_typing_destroy(&program->typing);
	pg_graph_destroy(&program->graph);
	free(program);
}
