#include "occurrence_io.h"
#include "wire.h"

#include <assert.h>
#include <string.h>

static void write_input(FILE *file, struct pg_typing *typing)
{
	struct pg_graph *g = typing->graph;
	const struct pg_term *a = pg_reference(g, pg_binder(g)), *b = pg_reference(g, pg_binder(g));
	const struct pg_object *x = pg_binder(g);
	const struct pg_term *v = pg_reference(g, x), *id = pg_lambda(g, x, v);
	const struct pg_context *ca = pg_context_bind(typing, NULL, x, a);
	const struct pg_context *cb = pg_context_bind(typing, NULL, x, b);
	const struct pg_occurrence *va = pg_occurrence(typing, PG_JUDGEMENT_VALUE, ca, v, a, a, 0, NULL);
	const struct pg_occurrence *vb = pg_occurrence(typing, PG_JUDGEMENT_VALUE, cb, v, b, b, 0, NULL);
	const struct pg_occurrence *oa = pg_occurrence(typing, PG_JUDGEMENT_VALUE, NULL, id, a, a, 1, &va);
	const struct pg_occurrence *ob = pg_occurrence(typing, PG_JUDGEMENT_VALUE, NULL, id, b, b, 1, &vb);
	const struct pg_occurrence *operands[] = {oa, ob};
	const struct pg_occurrence *bare = pg_occurrence(typing, PG_JUDGEMENT_INPUT, NULL, id, NULL, NULL, 2, operands);
	const struct pg_occurrence *scoped = pg_occurrence(typing, PG_JUDGEMENT_VALUE,
		ca, id, a, a, 1, &va);
	const struct pg_context_map *map = pg_context_map(typing, ca, cb, 1, &vb);
	const struct pg_occurrence *mapped = pg_occurrence_mapped(typing, PG_JUDGEMENT_VALUE,
		id, b, b, scoped, map);
	const struct pg_occurrence *boundary = pg_occurrence_boundary(typing, mapped, PG_JUDGEMENT_VALUE, a);
	const struct pg_occurrence *roots[] = {oa, ob, oa, bare, mapped, boundary};
	assert(mapped && boundary && pg_occurrences_write(file, 6, roots, NULL, NULL) == 0);
	/* Arbitrary elaboration inputs are transported, never treated as proofs. */
	assert(typing->proofs.count == 0);
	FILE *other = tmpfile();
	struct pg_occurrence *cycle = pg_alloc(g, sizeof(*cycle) + sizeof(cycle));
	assert(other && cycle);
	cycle->core = id;
	cycle->operand_count = 1;
	cycle->operands[0] = cycle;
	const struct pg_occurrence *cyclic = cycle;
	assert(pg_occurrences_write(other, 1, &cyclic, NULL, NULL) == -1);
	assert(fclose(other) == 0);
	const struct pg_occurrence *deep = bare;
	for (size_t i = 0; i < 10000; ++i) {
		const struct pg_occurrence *pair[] = {deep, deep};
		deep = pg_occurrence(typing, PG_JUDGEMENT_INPUT, NULL, id, NULL, NULL, 2, pair);
		assert(deep);
	}
	other = tmpfile();
	assert(other && pg_occurrences_write(other, 1, &deep, NULL, NULL) == 0);
	rewind(other);
	size_t count;
	const struct pg_occurrence *const *loaded;
	assert(pg_occurrences_read(other, typing, 100000, 0, NULL, NULL, &count, &loaded) == 0);
	assert(count == 1);
	const struct pg_occurrence *cursor = loaded[0];
	for (size_t i = 0; i < 10000; ++i) {
		assert(cursor->operand_count == 2 && cursor->operands[0] == cursor->operands[1]);
		cursor = cursor->operands[0];
	}
	assert(cursor->operands[0] != cursor->operands[1]);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && pg_occurrences_write(other, 0, NULL, NULL, NULL) == 0);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 0, 0, NULL, NULL, &count, &loaded) == 0 && count == 0);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC2", 1, 8, other) == 8);
	assert(!pg_wire_write_u64(other, 1) && !pg_wire_write_u64(other, 1));
	assert(fputc(0, other) != EOF && !pg_wire_write_u64(other, 1) && !pg_wire_write_u64(other, 1));
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(count == 0 && fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC1", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC\0", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
}

static void read_input(FILE *file, struct pg_typing *typing)
{
	size_t count;
	const struct pg_occurrence *const *roots;
	assert(pg_occurrences_read(file, typing, 1000, 0, NULL, NULL, &count, &roots) == 0);
	assert(count == 6 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(roots[0]->core == roots[1]->core && roots[0]->core == roots[3]->core);
	assert(roots[0]->annotation != roots[1]->annotation && !roots[3]->annotation);
	assert(roots[0]->classifier == roots[0]->annotation);
	assert(roots[1]->classifier == roots[1]->annotation && !roots[3]->classifier);
	assert(roots[3]->operands[0] == roots[0] && roots[3]->operands[1] == roots[1]);
	assert(roots[4] != roots[5] && roots[4]->map == roots[5]->map);
	assert(roots[4]->origin == roots[5]->origin && roots[4]->operand_count == 0);
	assert(roots[4]->map->source == roots[4]->origin->context);
	assert(roots[4]->map->destination == roots[4]->context);
	assert(roots[4]->map->count == 1 && roots[4]->map->images[0] == roots[1]->operands[0]);
	assert(pg_context_map_bindings(roots[4]->map)[0].binder == roots[4]->origin->context->binder);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_occurrence *v = roots[i]->operands[0];
		assert(v->context->declared_type == roots[i]->annotation);
		assert(v->annotation == roots[i]->annotation);
		assert(v->classifier == roots[i]->classifier);
		assert(v->core == roots[i]->core->as.lambda.body);
		assert(v->context->binder == roots[i]->core->as.lambda.binder);
	}
	assert(typing->proofs.count == 0);
	unsigned char bytes[4096];
	rewind(file);
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file) && length > 24);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		struct pg_typing destination;
		assert(pg_graph_init(&graph) == 0 && pg_typing_init(&destination, &graph) == 0);
		FILE *fragment = tmpfile();
		if (cut == length) bytes[24] = 8; /* Unknown occurrence flag. */
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		size_t unchanged = count;
		const struct pg_occurrence *const *old = roots;
		assert(pg_occurrences_read(fragment, &destination, 1000, 0, NULL, NULL, &count, &roots) == -1);
		assert(count == unchanged && roots == old && destination.proofs.count == 0);
		assert(fclose(fragment) == 0);
		pg_typing_destroy(&destination);
		pg_graph_destroy(&graph);
	}
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int writing = !strcmp(argv[1], "write");
	assert(writing || !strcmp(argv[1], "read"));
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	struct pg_graph graph;
	struct pg_typing typing;
	assert(file && pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
	if (writing) write_input(file, &typing);
	else read_input(file, &typing);
	assert(fclose(file) == 0);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	return 0;
}
