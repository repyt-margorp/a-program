#include "program.h"
#include "synthesis_source.h"

#include <assert.h>
#include <stdio.h>

static int find_binding(void *owner, const struct pg_source_binding *binding)
{
	*(const struct pg_source_binding **)owner = binding;
	return 0;
}

static const struct pg_object *second_binder(const struct pg_source_binding *binding)
{
#ifdef BORROWED_SCOPE_CURSOR
	struct pg_source_binding_cursor cursor = pg_synthesis_source_binding_scope(binding);
	const struct pg_object *binder;
	assert(pg_synthesis_source_binding_next(&cursor, &binder));
	assert(pg_synthesis_source_binding_next(&cursor, &binder));
	assert(!pg_synthesis_source_binding_next(&cursor, &(const struct pg_object *){0}));
	return binder;
#else
	return binding->scope[1];
#endif
}

int main(void)
{
	const char source[] = "main:=\\A:@=>\\x:A=>x;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root && program->scope);
	struct pg_source_scope scratch = *program->scope;
	scratch.parent = NULL;
	scratch.binder = pg_binder(&program->graph);
	const struct pg_object *original = scratch.binder;
	const struct pg_source_scope *head = pg_synthesis_intern_scope(&program->synthesis,
		(struct pg_source_scope){.parent = &scratch, .context = scratch.context,
		.binder = pg_binder(&program->graph)});
	assert(head && head->parent == &scratch);
	const struct pg_syntax *definitions = program->root->inputs[1];
	assert(definitions->kind == PG_SYNTAX_DEFINITIONS && definitions->item_count == 1);
	const struct pg_syntax *lambda = definitions->items[0].expression;
	assert(lambda->kind == PG_SYNTAX_LAMBDA);
	const struct pg_object *symbol = pg_synthesis_source_binder(&program->synthesis,
		head, lambda, 0, NULL);
	assert(symbol);
	const struct pg_source_binding *binding = NULL;
	assert(!pg_synthesis_visit_source_references(&program->synthesis, symbol,
		NULL, find_binding, NULL, &binding));
	assert(binding && binding->scope_count == 2 && second_binder(binding) == original);
	scratch.binder = pg_binder(&program->graph);
	int stable = second_binder(binding) == original;
	printf("canonical head, caller-owned ancestor: retained address %s after scratch mutation\n",
		stable ? "stable" : "changed");
	pg_program_destroy(program);
	return stable ? 0 : 1;
}
