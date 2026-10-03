#include "program.h"
#include "synthesis_source.h"
#include "iadt.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct binding_snapshot {
	const struct pg_source_binding *binding;
	const struct pg_object *binders[2];
	size_t count;
	int stable;
};

static int find_binding(void *owner, const struct pg_source_binding *binding)
{
	*(const struct pg_source_binding **)owner = binding;
	return 0;
}

static int same_address(struct binding_snapshot snapshot)
{
	struct pg_source_binding_cursor cursor = pg_synthesis_source_binding_scope(snapshot.binding);
	const struct pg_object *binder;
	for (size_t i = 0; i < snapshot.count; ++i)
		if (!pg_synthesis_source_binding_next(&cursor, &binder) || binder != snapshot.binders[i]) return 0;
	return !pg_synthesis_source_binding_next(&cursor, &binder);
}

static struct binding_snapshot escaped_address(struct pg_program *program,
	const struct pg_object *constructor, int canonical_head)
{
	struct pg_context scratch = {.binder = pg_binder(&program->graph),
		.declared_type = pg_universe(&program->graph, 0), .judgement = PG_JUDGEMENT_VALUE};
	const struct pg_context *context = &scratch;
	if (canonical_head) {
		context = pg_context_bind(&program->typing, &scratch, pg_binder(&program->graph),
			scratch.declared_type, scratch.judgement);
		assert(context && context->parent == &scratch);
	}
	struct binding_snapshot snapshot = {.count = canonical_head ? 2 : 1};
	snapshot.binders[0] = context->binder;
	if (canonical_head) snapshot.binders[1] = scratch.binder;
	const struct pg_object *symbol = pg_synthesis_constructor_binder(&program->synthesis, context, constructor, 0);
	assert(symbol && !pg_synthesis_visit_source_references(&program->synthesis, symbol,
		NULL, find_binding, NULL, &snapshot.binding));
	assert(snapshot.binding && snapshot.binding->scope_count == snapshot.count);
	scratch.binder = pg_binder(&program->graph);
	snapshot.stable = same_address(snapshot);
	return snapshot;
}

int main(int argc, char **argv)
{
	const char source[] = "main:=@;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	const struct pg_data_layout *layout = pg_data_layout(&program->graph, 1, (size_t[]){1});
	const struct pg_object *constructor = pg_data_constructor(layout, 0);
	assert(constructor);
	/* Keep a real returned frame at O2 and under stack-use-after-return SAN. */
	struct binding_snapshot (*volatile allocate)(struct pg_program *, const struct pg_object *, int) = escaped_address;
	if (argc == 2 && !strcmp(argv[1], "--mutation-only")) {
		struct binding_snapshot snapshot = allocate(program, constructor, 1);
		printf("canonical Context head/raw parent: address %s after scratch mutation\n",
			snapshot.stable ? "stable" : "changed");
		pg_program_destroy(program);
		return snapshot.stable ? 0 : 1;
	}
	assert(argc == 1);
	for (int canonical_head = 0; canonical_head < 2; ++canonical_head) {
		struct binding_snapshot snapshot = allocate(program, constructor, canonical_head);
		assert(snapshot.stable && snapshot.binding->scope && same_address(snapshot));
	}
	const struct pg_evidence *checked = pg_prove_empty_context(&program->typing);
	for (size_t i = 0; i < 3; ++i) {
		checked = pg_prove_context_extension(&program->typing, checked, pg_binder(&program->graph),
			pg_prove_universe(&program->typing, checked, 0));
		assert(checked);
	}
	const struct pg_context *context = pg_evidence_context(checked);
	const struct pg_object *symbol = pg_synthesis_constructor_binder(&program->synthesis, context, constructor, 0);
	const struct pg_source_binding *binding = NULL;
	assert(symbol && !pg_synthesis_visit_source_references(&program->synthesis, symbol,
		NULL, find_binding, NULL, &binding));
	assert(binding && binding->scope_count == 3);
#ifdef REQUIRE_CONTEXT_BORROWING
	assert(!binding->scope);
#endif
	struct pg_source_binding_cursor cursor = pg_synthesis_source_binding_scope(binding);
	const struct pg_object *binder;
	for (const struct pg_context *node = context; node; node = node->parent)
		assert(pg_synthesis_source_binding_next(&cursor, &binder) && binder == node->binder);
	assert(!pg_synthesis_source_binding_next(&cursor, &binder));
	size_t header = sizeof(struct pg_index_entry) + sizeof(*binding);
	size_t parent = header + 3 * sizeof(binding->scope[0]);
	size_t current = header + (binding->scope ? 3 : 1) * sizeof(binding->scope[0]);
	size_t alignment = _Alignof(max_align_t);
	printf("checked Context address: header%zu, retained pointers%u, raw savings%zu, aligned savings%zu\n",
		header, binding->scope ? 3u : 1u, parent - current,
		(parent + alignment - 1) / alignment * alignment - (current + alignment - 1) / alignment * alignment);
	pg_synthesis_advance(&program->synthesis, 10000);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	pg_program_destroy(program);
	puts("raw Context and canonical head/raw parent: stable after real scratch frame return");
	return 0;
}
