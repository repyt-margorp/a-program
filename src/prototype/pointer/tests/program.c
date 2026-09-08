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

int main(void)
{
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
