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
	const struct pg_context *ca = pg_context_bind(typing, NULL, x, a, PG_JUDGEMENT_VALUE);
	const struct pg_context *cb = pg_context_bind(typing, NULL, x, b, PG_JUDGEMENT_VALUE);
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
	const struct pg_occurrence *derived = pg_occurrence_derived(typing, mapped, PG_JUDGEMENT_VALUE, v, b);
	const struct pg_occurrence *image = pg_occurrence(typing, PG_JUDGEMENT_VALUE, cb, id, b, NULL, 1, &vb);
	const struct pg_context_map *image_map = pg_context_map(typing, ca, cb, 1, &image);
	const struct pg_occurrence *mapped_variable = pg_occurrence_mapped(typing,
		PG_JUDGEMENT_VALUE, id, a, a, va, image_map);
	const struct pg_occurrence *type = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, NULL, a, b, NULL, 0, NULL);
	const struct pg_occurrence *typed = pg_occurrence_classified(typing, oa, type);
	const struct pg_occurrence *alternate_type = pg_occurrence_derived(typing, type, type->judgement, type->core, type->classifier);
	const struct pg_occurrence *reclassified = pg_occurrence_reclassified(typing, typed, alternate_type);
	assert(reclassified && reclassified->origin == typed && !reclassified->operand_count);
	assert(pg_occurrence_reclassified(typing, typed, type) == typed);
	const struct pg_context_map *maps[] = {map, image_map}, *reverse[] = {image_map, map};
	const struct pg_occurrence *selected = pg_occurrence_with_maps(typing, image, 2, maps);
	const struct pg_occurrence *swapped = pg_occurrence_with_maps(typing, image, 2, reverse);
	assert(selected && swapped && selected != swapped && selected->core == swapped->core);
	assert(pg_occurrence_with_maps(typing, image, 2, maps) == selected);
	assert(!pg_occurrence_with_maps(typing, va, 2, maps));
	const struct pg_context *clauses[] = {ca, cb};
	struct pg_induction_allocation allocation = {x, a->as.reference, b->as.reference, 2, clauses};
	const struct pg_occurrence *recursive = pg_occurrence_with_induction(typing, selected, &allocation);
	assert(recursive && recursive->induction != &allocation && recursive->induction->clauses != clauses);
	assert(pg_occurrence_with_induction(typing, selected, recursive->induction) == recursive);
	assert(!pg_occurrence_with_induction(typing, mapped, &allocation));
	assert(pg_occurrence_with_induction(typing, recursive, NULL) == selected);
	const struct pg_context *reverse_clauses[] = {cb, ca};
	allocation.clauses = reverse_clauses;
	const struct pg_occurrence *other_scopes = pg_occurrence_with_induction(typing, selected, &allocation);
	assert(other_scopes && other_scopes != recursive && recursive->induction->clauses[0] == ca);
	const struct pg_occurrence *recursive_boundary = pg_occurrence_boundary(typing, recursive, PG_JUDGEMENT_VALUE, a);
	assert(recursive_boundary && recursive_boundary->origin == recursive && !recursive_boundary->induction);
	const struct pg_occurrence *pick = pg_occurrence_selected(typing, oa, 0, ob, PG_JUDGEMENT_VALUE, id, a);
	const struct pg_occurrence *constant = pg_occurrence_selected(typing, oa, 0, NULL, PG_JUDGEMENT_VALUE, id, a);
	const struct pg_occurrence *other_input = pg_occurrence_selected(typing, oa, 1, ob, PG_JUDGEMENT_VALUE, id, a);
	const struct pg_occurrence *other_argument = pg_occurrence_selected(typing, oa, 0, oa, PG_JUDGEMENT_VALUE, id, a);
	assert(pick && constant && other_input && other_argument);
	assert(pick != constant && pick != other_input && pick != other_argument);
	assert(pg_occurrence_selected(typing, oa, 0, ob, PG_JUDGEMENT_VALUE, id, a) == pick);
	assert(!pg_occurrence_selected(typing, oa, SIZE_MAX, ob, PG_JUDGEMENT_VALUE, id, a));
	assert(!pg_occurrence_selected(typing, oa, 0, vb, PG_JUDGEMENT_VALUE, id, a));
	const struct pg_occurrence *roots[] = {oa, ob, oa, bare, mapped, boundary, derived, derived, mapped_variable, typed, type, selected, swapped,
		recursive, recursive, other_scopes, recursive_boundary, pick, pick, constant, other_input, other_argument, reclassified};
	assert(mapped && boundary && derived && mapped_variable && typed && pg_occurrences_write(file, 23, roots, NULL, NULL) == 0);
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
	assert(other && fwrite("APGOCC7", 1, 8, other) == 8);
	assert(!pg_wire_write_u64(other, 1) && !pg_wire_write_u64(other, 1));
	assert(fputc(0, other) != EOF && fputc(PG_JUDGEMENT_INPUT, other) != EOF);
	assert(!pg_wire_write_u64(other, 1) && !pg_wire_write_u64(other, 1));
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(count == 0 && fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC6", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC5", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC4", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC3", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
	other = tmpfile();
	assert(other && fwrite("APGOCC2", 1, 8, other) == 8);
	rewind(other);
	assert(pg_occurrences_read(other, typing, 100, 0, NULL, NULL, &count, &loaded) == -1);
	assert(fclose(other) == 0);
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
	assert(count == 23 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(roots[22]->origin == roots[9] && roots[22]->type->origin == roots[10]);
	assert(roots[22]->core == roots[9]->core && roots[22]->classifier == roots[9]->classifier);
	assert(!roots[22]->operand_count && roots[22]->type != roots[9]->type);
	assert(pg_occurrence_reclassified(typing, roots[9], roots[22]->type) == roots[22]);
	assert(pg_occurrence_scoped_input(roots[22], 0) == roots[0]->operands[0]);
	assert(roots[17] == roots[18] && roots[17]->selection == 1 && roots[17]->origin == roots[0]);
	assert(roots[17]->operand_count == 1 && roots[17]->operands[0] == roots[1]);
	assert(roots[19]->origin == roots[0] && roots[19]->selection == 1 && !roots[19]->operand_count);
	assert(roots[20]->selection == 2 && roots[20]->operands[0] == roots[1]);
	assert(roots[21]->selection == 1 && roots[21]->operands[0] == roots[0]);
	assert(pg_occurrence_selected(typing, roots[0], 0, roots[1], PG_JUDGEMENT_VALUE,
		roots[0]->core, roots[0]->classifier) == roots[17]);
	assert(roots[13] == roots[14] && roots[13] != roots[15]);
	const struct pg_induction_allocation *allocation = roots[13]->induction;
	assert(allocation && allocation->count == 2);
	assert(allocation->recursion == roots[0]->core->as.lambda.binder);
	assert(allocation->argument == roots[0]->classifier->as.reference);
	assert(allocation->self == roots[1]->classifier->as.reference);
	assert(allocation->clauses[0] == roots[0]->operands[0]->context);
	assert(allocation->clauses[1] == roots[1]->operands[0]->context);
	assert(roots[15]->induction->clauses[0] == allocation->clauses[1]);
	assert(roots[15]->induction->clauses[1] == allocation->clauses[0]);
	assert(roots[16]->origin == roots[13] && !roots[16]->induction && !roots[16]->operand_count);
	assert(pg_occurrence_with_induction(typing, roots[11], allocation) == roots[13]);
	assert(roots[13]->map_count == 2 && pg_occurrence_maps(roots[13])[0] == pg_occurrence_maps(roots[11])[0]);
	assert(roots[11] != roots[12] && roots[11]->core == roots[12]->core);
	assert(roots[11]->map_count == 2 && roots[12]->map_count == 2);
	const struct pg_context_map *const *maps = pg_occurrence_maps(roots[11]);
	const struct pg_context_map *const *reverse = pg_occurrence_maps(roots[12]);
	assert(maps[0] == reverse[1] && maps[1] == reverse[0]);
	assert(maps[0] == roots[4]->map && maps[1] == roots[8]->map);
	assert(maps[0]->source == roots[0]->operands[0]->context);
	assert(maps[0]->destination == roots[11]->context);
	assert(pg_occurrence_with_maps(typing, maps[1]->images[0], 2, maps) == roots[11]);
	assert(roots[9]->type == roots[10] && roots[9]->core == roots[0]->core);
	assert(roots[9]->classifier == roots[10]->core && roots[9]->operands[0] == roots[0]->operands[0]);
	assert(!roots[0]->type);
	struct pg_occurrence_input *type_query = pg_occurrence_type_request(typing, roots[9]);
	assert(pg_occurrence_input_advance(type_query, 0) == PG_INPUT_PENDING);
	while (pg_occurrence_input_advance(type_query, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(type_query) == roots[10]);
	assert(pg_occurrence_type_request(typing, roots[9]) == type_query);
	assert(roots[6] == roots[7] && roots[6]->origin == roots[4]);
	assert(!roots[6]->map && !roots[6]->operand_count);
	assert(roots[6]->context == roots[4]->context);
	assert(roots[0]->core == roots[1]->core && roots[0]->core == roots[3]->core);
	assert(roots[0]->annotation != roots[1]->annotation && !roots[3]->annotation);
	assert(roots[0]->classifier == roots[0]->annotation);
	assert(roots[1]->classifier == roots[1]->annotation && !roots[3]->classifier);
	assert(roots[3]->operands[0] == roots[0] && roots[3]->operands[1] == roots[1]);
	assert(roots[4] != roots[5] && roots[5]->origin == roots[4]);
	assert(!roots[5]->map && !roots[5]->operand_count && !roots[4]->operand_count);
	assert(roots[4]->map->source == roots[4]->origin->context);
	assert(roots[4]->map->destination == roots[4]->context);
	assert(roots[4]->map->count == 1 && roots[4]->map->images[0] == roots[1]->operands[0]);
	assert(pg_context_map_bindings(roots[4]->map)[0].binder == roots[4]->origin->context->binder);
	assert(roots[8]->origin == roots[0]->operands[0]);
	assert(roots[8]->classifier != roots[8]->map->images[0]->classifier);
	struct pg_occurrence_input *input = pg_occurrence_input_request(typing, roots[8], 0);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(input) == roots[1]->operands[0]);
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
		if (cut == length) bytes[24] = 64; /* Unknown occurrence flag. */
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
