#include "program.h"
#include "synthesis_source.h"

#include <assert.h>
#include <stdio.h>

struct binding_snapshot {
	const struct pg_source_binding *binding;
	const struct pg_object *binders[2];
	size_t count;
};

static int find_binding(void *owner, const struct pg_source_binding *binding)
{
	*(const struct pg_source_binding **)owner = binding;
	return 0;
}

static struct binding_snapshot escaped_address(struct pg_program *program,
	const struct pg_syntax *lambda, int canonical_head)
{
	struct pg_source_scope scratch = *program->scope;
	scratch.parent = NULL;
	scratch.binder = pg_binder(&program->graph);
	const struct pg_source_scope *scope = &scratch;
	if (canonical_head) {
		scope = pg_synthesis_intern_scope(&program->synthesis,
			(struct pg_source_scope){.parent = &scratch, .context = scratch.context,
			.binder = pg_binder(&program->graph)});
		assert(scope && scope->parent == &scratch);
	}
	struct binding_snapshot snapshot = {.count = canonical_head ? 2 : 1};
	snapshot.binders[0] = scope->binder;
	if (canonical_head) snapshot.binders[1] = scratch.binder;
	const struct pg_object *symbol = pg_synthesis_source_binder(&program->synthesis, scope, lambda, 0, NULL);
	assert(symbol && !pg_synthesis_visit_source_references(&program->synthesis, symbol,
		NULL, find_binding, NULL, &snapshot.binding));
	assert(snapshot.binding && snapshot.binding->scope && snapshot.binding->scope_count == snapshot.count);
	return snapshot;
}

int main(void)
{
	const char source[] = "main:=\\A:@=>\\x:A=>x;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	const struct pg_syntax *definitions = program->root->inputs[1];
	const struct pg_syntax *lambda = definitions->items[0].expression;
	assert(lambda->kind == PG_SYNTAX_LAMBDA);
	/* Keep a real returned frame when the optimizer sees this small control. */
	struct binding_snapshot (*volatile allocate)(struct pg_program *, const struct pg_syntax *, int) = escaped_address;
	for (int canonical_head = 0; canonical_head < 2; ++canonical_head) {
		struct binding_snapshot snapshot = allocate(program, lambda, canonical_head);
		struct pg_source_binding_cursor cursor = pg_synthesis_source_binding_scope(snapshot.binding);
		const struct pg_object *binder;
		for (size_t i = 0; i < snapshot.count; ++i)
			assert(pg_synthesis_source_binding_next(&cursor, &binder) && binder == snapshot.binders[i]);
		assert(!pg_synthesis_source_binding_next(&cursor, &binder));
	}
	pg_program_destroy(program);
	puts("copied head and canonical head/raw parent: independent address survives scratch frame return");
	return 0;
}
