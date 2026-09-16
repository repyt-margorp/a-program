#include "occurrence_io.h"
#include "context_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const char magic[8] = "APGOCC6";

static size_t structure_arity(const struct pg_occurrence *source)
{
	return source->map ? 1 + source->map->count : source->origin ? 1 : source->operand_count;
}

static int operand(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_occurrence *source = key;
	size_t arity = structure_arity(source);
	if (index == arity && source->type) {
		*child = source->type;
		return 1;
	}
	if (index >= arity) {
		index -= arity + (source->type != NULL);
		const struct pg_context_map *const *maps = pg_occurrence_maps(source);
		for (size_t i = 0; i < source->map_count; ++i) {
			if (index < maps[i]->count) { *child = maps[i]->images[index]; return 1; }
			index -= maps[i]->count;
		}
		return 0;
	}
	if (source->map) {
		*child = index ? source->map->images[index - 1] : source->origin;
		return 1;
	}
	if (source->origin) {
		*child = source->origin;
		return 1;
	}
	*child = source->operands[index];
	return 1;
}

int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct pg_graph arena = {0};
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_graph_init(&arena) || pg_dag_init(&dag, operand, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&dag, roots[i])) goto done;
	size_t size = dag.count, context_count = size;
	if (size > SIZE_MAX / sizeof(void *) / 3) goto done;
	size_t term_count = 3 * size;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_occurrence *o = r->key;
		if (o->map_count > SIZE_MAX / sizeof(void *) - context_count) goto done;
		context_count += o->map_count;
		if (o->induction) {
			if (o->induction->count > SIZE_MAX / sizeof(void *) - context_count) goto done;
			if (term_count > SIZE_MAX / sizeof(void *) - 3) goto done;
			context_count += o->induction->count;
			term_count += 3;
		}
	}
	const struct pg_context **contexts = pg_alloc(&arena, context_count * sizeof(*contexts));
	const struct pg_term **terms = pg_alloc(&arena, term_count * sizeof(*terms));
	if (!contexts || !terms) goto done;
	size_t next_context = size, next_term = 3 * size;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_occurrence *o = r->key;
		contexts[r->id - 1] = o->context;
		terms[3 * (r->id - 1)] = o->core;
		terms[3 * (r->id - 1) + 1] = o->annotation ? o->annotation : o->core;
		terms[3 * (r->id - 1) + 2] = o->classifier ? o->classifier : o->core;
		int flags = (o->annotation != NULL) | ((o->classifier != NULL) << 1) |
			((o->map != NULL) << 2) | ((o->origin && !o->map) << 3) | ((o->type != NULL) << 4) |
			((o->induction != NULL) << 5);
		size_t arity = structure_arity(o) + (o->type != NULL);
		const struct pg_context_map *const *maps = pg_occurrence_maps(o);
		for (size_t i = 0; i < o->map_count; ++i) {
			if (maps[i]->count > SIZE_MAX - arity) goto done;
			arity += maps[i]->count;
			contexts[next_context++] = maps[i]->source;
		}
		if (fputc(flags, file) == EOF || fputc(o->judgement, file) == EOF ||
			pg_wire_write_u64(file, arity)) goto done;
		for (size_t i = 0; i < arity; ++i) {
			const void *key;
			if (!operand(NULL, o, i, &key)) goto done;
			const struct pg_dag_node *child = pg_dag_find(&dag, key);
			if (!child || pg_wire_write_u64(file, child->id)) goto done;
		}
		if (pg_wire_write_u64(file, o->map_count)) goto done;
		for (size_t i = 0; i < o->map_count; ++i)
			if (pg_wire_write_u64(file, maps[i]->count)) goto done;
		if (o->induction) {
			const struct pg_induction_allocation *a = o->induction;
			if (pg_wire_write_u64(file, a->count)) goto done;
			for (size_t i = 0; i < a->count; ++i) contexts[next_context++] = a->clauses[i];
			terms[next_term++] = pg_reference(&arena, a->recursion);
			terms[next_term++] = pg_reference(&arena, a->argument);
			terms[next_term++] = pg_reference(&arena, a->self);
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&dag, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = pg_contexts_write(file, context_count, contexts, term_count, terms, name, owner);
done:
	pg_dag_destroy(&dag);
	pg_graph_destroy(&arena);
	return status;
}

struct input {
	size_t count, structural, map_count, clause_count;
	size_t *map_sizes;
	int flags;
	enum pg_evidence_judgement judgement;
	uint64_t *operands;
};

int pg_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_occurrence *const **roots)
{
	if (!file || !typing || !typing->occurrences.capacity || !count || !roots) return -1;
	char header[8];
	uint64_t n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return -1;
	if (n > limit || nr > limit - n || limit > SIZE_MAX / sizeof(struct input)) return -1;
	if (n > SIZE_MAX / 3) return -1;
	struct pg_graph *graph = typing->graph;
	struct input *inputs = pg_alloc(graph, (size_t)n * sizeof(*inputs));
	uint64_t *ids = pg_alloc(graph, (size_t)nr * sizeof(*ids));
	const struct pg_occurrence **all = pg_alloc(graph, (size_t)n * sizeof(*all));
	const struct pg_occurrence **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!inputs || !ids || !all || !result) return -1;
	size_t available = limit - (size_t)n - (size_t)nr, max_arity = 0, max_maps = 0, context_count = (size_t)n;
	size_t term_count = 3 * (size_t)n;
	for (size_t i = 0; i < n; ++i) {
		inputs[i].flags = fgetc(file);
		inputs[i].judgement = fgetc(file);
		uint64_t arity;
		if (inputs[i].flags < 0 || inputs[i].flags > 63 || (inputs[i].flags & 12) == 12) return -1;
		if ((inputs[i].flags & 32) && (inputs[i].flags & 12)) return -1;
		if (pg_wire_read_u64(file, &arity) || arity > available) return -1;
		available -= (size_t)arity;
		inputs[i].count = (size_t)arity;
		if (arity > max_arity) max_arity = (size_t)arity;
		inputs[i].operands = pg_alloc(graph, (size_t)arity * sizeof(uint64_t));
		if (!inputs[i].operands) return -1;
		for (size_t j = 0; j < arity; ++j) {
			uint64_t *id = &inputs[i].operands[j];
			if (pg_wire_read_u64(file, id) || !*id || *id > i) return -1;
		}
		uint64_t maps;
		if (pg_wire_read_u64(file, &maps) || maps > available) return -1;
		available -= (size_t)maps;
		context_count += (size_t)maps;
		inputs[i].map_count = (size_t)maps;
		if (maps > max_maps) max_maps = (size_t)maps;
		inputs[i].map_sizes = pg_alloc(graph, (size_t)maps * sizeof(size_t));
		if (!inputs[i].map_sizes) return -1;
		size_t structural = (size_t)arity;
		for (size_t j = 0; j < maps; ++j) {
			uint64_t images;
			if (pg_wire_read_u64(file, &images) || images > structural) return -1;
			inputs[i].map_sizes[j] = (size_t)images;
			structural -= (size_t)images;
		}
		if ((inputs[i].flags & 16) && (!structural || !(inputs[i].flags & 2))) return -1;
		structural -= !!(inputs[i].flags & 16);
		if ((inputs[i].flags & 4) && !structural) return -1;
		if ((inputs[i].flags & 8) && (structural != 1 || (inputs[i].flags & 1))) return -1;
		inputs[i].structural = structural;
		inputs[i].clause_count = 0;
		if (inputs[i].flags & 32) {
			uint64_t clauses;
			if (available < 3) return -1;
			available -= 3;
			if (pg_wire_read_u64(file, &clauses) || clauses > available) return -1;
			available -= (size_t)clauses;
			inputs[i].clause_count = (size_t)clauses;
			context_count += (size_t)clauses;
			if (term_count > SIZE_MAX - 3) return -1;
			term_count += 3;
		}
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &ids[i]) || !ids[i] || ids[i] > n) return -1;
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	if (pg_contexts_read(file, typing, limit, name_limit, resolve, owner, &nc, &contexts, &nt, &terms)) return -1;
	if (nc != context_count || nt != term_count) return -1;
	const struct pg_occurrence **operands = pg_alloc(graph, max_arity * sizeof(*operands));
	const struct pg_context_map **maps = pg_alloc(graph, max_maps * sizeof(*maps));
	if (!operands || !maps) return -1;
	size_t next_context = (size_t)n, next_term = 3 * (size_t)n;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < inputs[i].count; ++j) operands[j] = all[inputs[i].operands[j] - 1];
		size_t arity = inputs[i].structural;
		const struct pg_term *classifier = inputs[i].flags & 2 ? terms[3 * i + 2] : NULL;
		const struct pg_term *annotation = inputs[i].flags & 1 ? terms[3 * i + 1] : NULL;
		if (inputs[i].flags & 4) {
			const struct pg_context_map *map = pg_context_map(typing, operands[0]->context,
				contexts[i], arity - 1, operands + 1);
			all[i] = pg_occurrence_mapped(typing, inputs[i].judgement, terms[3 * i],
				classifier, annotation, operands[0], map);
		} else if (inputs[i].flags & 8) {
			if (contexts[i] != operands[0]->context) return -1;
			all[i] = pg_occurrence_derived(typing, operands[0], inputs[i].judgement,
				terms[3 * i], classifier);
		} else {
			all[i] = pg_occurrence(typing, inputs[i].judgement, contexts[i], terms[3 * i],
				classifier, annotation, arity, operands);
		}
		if (inputs[i].flags & 16) {
			if (operands[arity]->core != classifier) return -1;
			all[i] = pg_occurrence_classified(typing, all[i], operands[arity]);
		}
		size_t first_image = arity + !!(inputs[i].flags & 16);
		for (size_t j = 0; j < inputs[i].map_count; ++j) {
			maps[j] = pg_context_map(typing, contexts[next_context++], contexts[i],
				inputs[i].map_sizes[j], operands + first_image);
			if (!maps[j]) return -1;
			first_image += inputs[i].map_sizes[j];
		}
		if (inputs[i].map_count) all[i] = pg_occurrence_with_maps(typing, all[i], inputs[i].map_count, maps);
		if (inputs[i].flags & 32) {
			for (size_t j = 0; j < 3; ++j)
				if (terms[next_term + j]->kind != PG_REFERENCE) return -1;
			struct pg_induction_allocation a = {
				.recursion = terms[next_term]->as.reference,
				.argument = terms[next_term + 1]->as.reference,
				.self = terms[next_term + 2]->as.reference,
				.count = inputs[i].clause_count, .clauses = contexts + next_context};
			all[i] = pg_occurrence_with_induction(typing, all[i], &a);
			next_context += a.count;
			next_term += 3;
		}
		if (!all[i]) return -1;
	}
	for (size_t i = 0; i < nr; ++i) result[i] = all[ids[i] - 1];
	*count = (size_t)nr;
	*roots = result;
	return 0;
}
