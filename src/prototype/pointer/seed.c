#include "seed.h"
#include "syntax_io.h"

#include <string.h>

static const unsigned char magic[8] = {'A', 'P', 'G', 'S', 'E', 'E', 'D', 1};

int pg_seed_write(FILE *file, const char *source, size_t length,
	enum pg_definition_policy policy)
{
	if (!file || (!source && length)) return -1;
	unsigned char header[9] = {0};
	memcpy(header, magic, sizeof(magic));
	switch (policy) {
	case PG_DEFINITION_EXPLICIT_THUNK: header[8] = 0; break;
	case PG_DEFINITION_IMPLICIT_THUNK: header[8] = 1; break;
	default: return -1;
	}
	struct pg_graph graph;
	if (pg_graph_init(&graph)) return -1;
	struct pg_parser parser;
	pg_parser_init(&parser, &graph, source, length);
	const struct pg_syntax *syntax = pg_parser_program(&parser);
	int status = -1;
	if (!syntax || pg_syntax_validate(1, &syntax)) goto done;
	if (fwrite(header, 1, sizeof(header), file) != sizeof(header)) goto done;
	status = pg_syntax_write(file, 1, &syntax);
done:
	pg_graph_destroy(&graph);
	return status;
}

struct pg_program *pg_seed_read(FILE *file, size_t limit)
{
	if (!file) return NULL;
	unsigned char header[9];
	if (fread(header, 1, sizeof(header), file) != sizeof(header)) return NULL;
	if (memcmp(header, magic, sizeof(magic))) return NULL;
	enum pg_definition_policy policy;
	switch (header[8]) {
	case 0: policy = PG_DEFINITION_EXPLICIT_THUNK; break;
	case 1: policy = PG_DEFINITION_IMPLICIT_THUNK; break;
	default: return NULL;
	}
	struct pg_program *program = pg_program_allocate(policy);
	if (!program) return NULL;
	size_t count;
	const struct pg_syntax *const *roots;
	if (pg_syntax_read(file, &program->graph, limit, &count, &roots) || count != 1) goto fail;
	if (fgetc(file) != EOF || ferror(file) || pg_syntax_validate(count, roots)) goto fail;
	const struct pg_syntax *body = roots[0];
	if (body->kind == PG_SYNTAX_QUALIFIED) body = body->left;
	if (body->kind != PG_SYNTAX_DEFINITIONS) goto fail;
	program->root = pg_synthesis_request(&program->synthesis, program->scope, roots[0]);
	if (!program->root) goto fail;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}
