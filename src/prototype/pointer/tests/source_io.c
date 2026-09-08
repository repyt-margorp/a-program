#include "source_io.h"
#include <assert.h>
#include <string.h>

static struct pg_synthesis_job *parse(struct pg_program *program,
	const struct pg_source_scope *scope, const char *text)
{
	struct pg_parser parser;
	struct pg_synthesis_job *job = pg_program_source(program, scope, text, strlen(text), &parser);
	assert(job && !parser.error);
	return job;
}

static void write_sources(FILE *file)
{
	const char text[] = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	struct pg_token lib = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *namespace = pg_synthesis_module_namespace(&p->synthesis, p->scope, lib, p->root);
	assert(namespace);
	struct pg_synthesis_job *client = parse(p, namespace, "{{ main:=lib.id; }}.main");
	struct pg_token alias = {.kind = PG_TOKEN_IDENT, .text = "alias", .length = 5};
	const struct pg_source_scope *exports = pg_synthesis_name_job(&p->synthesis, p->scope, alias, client);
	const struct pg_source_scope *imports = pg_synthesis_import_scope(&p->synthesis, p->scope, exports);
	struct pg_synthesis_job *consumer = parse(p, imports, "{{ import alias; main:=alias; }}.main");
	struct pg_token public = {.kind = PG_TOKEN_IDENT, .text = "public", .length = 6};
	const struct pg_source_scope *published = pg_synthesis_namespace(&p->synthesis, p->scope, public, exports);
	struct pg_synthesis_job *member = parse(p, published, "{{ main:=public.alias; }}.main");
	struct pg_synthesis_job *roots[] = {client, consumer, p->root, client, member};
	size_t jobs = p->synthesis.jobs.count, scopes = p->synthesis.scopes.count, proofs = p->typing.proofs.count;
	assert(!pg_sources_write(file, &p->synthesis, 5, roots));
	assert(!p->synthesis.steps && p->synthesis.jobs.count == jobs && p->synthesis.scopes.count == scopes);
	assert(p->typing.proofs.count == proofs);
	/* The source view is independent of accepted/failed progress state. */
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 64); }
	for (size_t i = 0; i < 5; ++i) assert(pg_synthesis_status(roots[i]) == PG_SYNTHESIS_DONE);
	FILE *accepted = tmpfile();
	assert(accepted && !pg_sources_write(accepted, &p->synthesis, 5, roots));
	rewind(file); rewind(accepted);
	int a, b;
	do { a = fgetc(file); b = fgetc(accepted); assert(a == b); } while (a != EOF);
	assert(!ferror(file) && !ferror(accepted) && !fclose(accepted));
	FILE *unsupported = tmpfile();
	assert(unsupported);
	struct pg_synthesis_job *evidence = pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(client));
	assert(pg_sources_write(unsupported, &p->synthesis, 1, &evidence) == -1);
	assert(ftell(unsupported) == 0 && !fclose(unsupported));
	pg_program_destroy(p);
}

static void read_sources(FILE *file, uint64_t chunk)
{
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
	assert(p && count == 5 && p->root == roots[0] && roots[0] == roots[3]);
	assert(!p->synthesis.steps && !p->parser.reader.input);
	for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
	for (size_t i = 0; i < count; ++i) assert(pg_synthesis_status(roots[i]) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[1]));
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[4]));
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	assert(pg_synthesis_result(pg_synthesis_definition(roots[2], id)) == pg_synthesis_result(roots[0]));
	printf("source image: shared modules, explicit imports and ordinary Solve passed (%llu steps)\n", (unsigned long long)p->synthesis.steps);
	pg_program_destroy(p);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int writing = !strcmp(argv[1], "write");
	FILE *file = fopen(argv[2], writing ? "w+b" : "rb");
	assert(file);
	if (writing) write_sources(file);
	else read_sources(file, !strcmp(argv[1], "read-bulk") ? 64 : 1);
	assert(!fclose(file));
	return 0;
}
