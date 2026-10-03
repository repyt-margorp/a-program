#include "program.h"
#include "synthesis_source.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

struct binding_storage {
	size_t count, edges, raw, aligned;
};

static int native_scope(void *owner, const struct pg_source_binding *binding)
{
	if (!binding->syntax || !binding->scope_count) return 0;
	assert(!binding->scope);
	struct pg_source_binding_cursor cursor = pg_synthesis_source_binding_scope(binding);
	const struct pg_object *binder;
	size_t count = 0;
	while (pg_synthesis_source_binding_next(&cursor, &binder)) {
		assert(binder && binder->kind == PG_BINDER);
		++count;
	}
	assert(count == binding->scope_count);
	struct binding_storage *storage = owner;
	size_t header = sizeof(struct pg_index_entry) + sizeof(*binding);
	size_t parent = header + count * sizeof(binding->scope[0]);
	size_t candidate = header + sizeof(const struct pg_source_scope *);
	size_t alignment = _Alignof(max_align_t);
	storage->raw += parent - candidate;
	storage->aligned += (parent + alignment - 1) / alignment * alignment
		- (candidate + alignment - 1) / alignment * alignment;
	storage->edges += count;
	++storage->count;
	return 0;
}

int main(void)
{
	const char source[] = "main:=\\A:@=>\\x:A=>\\y:A=>\\z:A=>x;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	pg_synthesis_advance(&program->synthesis, 100000);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	struct binding_storage storage = {0};
	assert(!pg_synthesis_visit_source_bindings(&program->synthesis, native_scope, &storage));
	assert(storage.count && storage.raw && storage.aligned);
	printf("canonical DONE: %zu borrowed records, %zu deleted Binder-array edges, raw savings %zu, aligned layout savings %zu\n",
		storage.count, storage.edges, storage.raw, storage.aligned);
	pg_program_destroy(program);
	return 0;
}
