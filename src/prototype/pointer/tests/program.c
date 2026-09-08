#include "program.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void solve(struct pg_program *program, uint64_t budget)
{
	for (size_t turns = 0; pg_synthesis_status(program->root) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 10000);
		pg_synthesis_advance(&program->synthesis, budget);
	}
}

static void modules(void)
{
	const char *library = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *program = pg_program_create(library, strlen(library), PG_DEFINITION_EXPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *scope = pg_synthesis_module_namespace(&program->synthesis,
		program->scope, name, program->root);
	struct pg_parser parser;
	char client[] = "{{ main:=lib.id; }}.main";
	struct pg_synthesis_job *root = pg_program_source(program, scope, client, strlen(client), &parser);
	assert(root && !parser.error && !program->synthesis.steps);
	memset(client, '?', sizeof(client));
	for (size_t i = 0; pg_synthesis_status(root) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&program->synthesis, 1);
	}
	assert(pg_synthesis_status(root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *exported = pg_synthesis_definition(program->root, id);
	assert(exported && pg_synthesis_result(exported) == pg_synthesis_result(root));
	const struct pg_evidence *accepted = pg_synthesis_result(root);
	assert(!pg_program_source(program, scope, "bad:=", 5, &parser));
	assert(parser.error);
	assert(pg_synthesis_result(root) == accepted && !program->parser.error);
	pg_program_destroy(program);
}

int main(void)
{
	modules();
	char source[] = "{{ id := &(\\A:@ => \\x:A => x); id :: (A:@)->A->A; }}.id";
	struct pg_program *split = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	struct pg_program *whole = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	assert(split && whole && split->root && whole->root);
	memset(source, '?', sizeof(source));
	assert(!split->synthesis.steps && !whole->synthesis.steps);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(split->root));
	pg_synthesis_advance(&split->synthesis, 0);
	assert(!split->synthesis.steps);
	solve(split, 1);
	solve(whole, 10000);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whole->root) == PG_SYNTHESIS_DONE);
	assert(split->synthesis.steps == whole->synthesis.steps);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(split->root))->core,
		pg_evidence_subject(pg_synthesis_result(whole->root))->core) == 1);
	uint64_t steps = split->synthesis.steps;
	pg_synthesis_advance(&split->synthesis, 10000);
	assert(split->synthesis.steps == steps);
	pg_program_destroy(split);
	pg_program_destroy(whole);
	const char *invalid = "main := ";
	struct pg_program *bad = pg_program_create(invalid, strlen(invalid), PG_DEFINITION_EXPLICIT_THUNK);
	assert(bad && !bad->root && bad->parser.error);
	assert(!bad->synthesis.steps);
	pg_program_destroy(bad);
	struct pg_program *pending = pg_program_create("x:=x;", 5, PG_DEFINITION_EXPLICIT_THUNK);
	assert(pending && pending->root);
	pg_synthesis_advance(&pending->synthesis, 100);
	assert(pg_synthesis_status(pending->root) == PG_SYNTHESIS_PENDING);
	pg_program_destroy(pending);
	assert(!pg_program_create(NULL, 1, PG_DEFINITION_EXPLICIT_THUNK));
	assert(!pg_program_create("", 0, (enum pg_definition_policy)99));
	pg_program_destroy(NULL);
	puts("program: owned source, unresolved root, split-budget solve and pending destruction passed");
	return 0;
}
