#include "occurrence_io.h"
#include "context_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const char magic[8] = "APGOCC2";

static int operand(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_occurrence *source = key;
	if (source->map) {
		if (index > source->map->count) return 0;
		*child = index ? source->map->images[index - 1] : source->origin;
		return 1;
	}
	if (index == source->operand_count) return 0;
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
	size_t size = dag.count;
	if (size > SIZE_MAX / sizeof(void *) / 3) goto done;
	const struct pg_context **contexts = pg_alloc(&arena, size * sizeof(*contexts));
	const struct pg_term **terms = pg_alloc(&arena, 3 * size * sizeof(*terms));
	if (!contexts || !terms) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_occurrence *o = r->key;
		contexts[r->id - 1] = o->context;
		terms[3 * (r->id - 1)] = o->core;
		terms[3 * (r->id - 1) + 1] = o->annotation ? o->annotation : o->core;
		terms[3 * (r->id - 1) + 2] = o->classifier ? o->classifier : o->core;
		int flags = (o->annotation != NULL) | ((o->classifier != NULL) << 1) | ((o->map != NULL) << 2);
		size_t arity = o->map ? 1 + o->map->count : o->operand_count;
		if (fputc(flags, file) == EOF || fputc(o->judgement, file) == EOF ||
			pg_wire_write_u64(file, arity)) goto done;
		for (size_t i = 0; i < arity; ++i) {
			const void *key;
			if (!operand(NULL, o, i, &key)) goto done;
			const struct pg_dag_node *child = pg_dag_find(&dag, key);
			if (!child || pg_wire_write_u64(file, child->id)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&dag, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = pg_contexts_write(file, size, contexts, 3 * size, terms, name, owner);
done:
	pg_dag_destroy(&dag);
	pg_graph_destroy(&arena);
	return status;
}

struct input {
	size_t count;
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
	size_t available = limit - (size_t)n - (size_t)nr, max_arity = 0;
	for (size_t i = 0; i < n; ++i) {
		inputs[i].flags = fgetc(file);
		inputs[i].judgement = fgetc(file);
		uint64_t arity;
		if (inputs[i].flags < 0 || inputs[i].flags > 7) return -1;
		if (pg_wire_read_u64(file, &arity) || arity > available) return -1;
		if ((inputs[i].flags & 4) && !arity) return -1;
		available -= (size_t)arity;
		inputs[i].count = (size_t)arity;
		if (arity > max_arity) max_arity = (size_t)arity;
		inputs[i].operands = pg_alloc(graph, (size_t)arity * sizeof(uint64_t));
		if (!inputs[i].operands) return -1;
		for (size_t j = 0; j < arity; ++j) {
			uint64_t *id = &inputs[i].operands[j];
			if (pg_wire_read_u64(file, id) || !*id || *id > i) return -1;
		}
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &ids[i]) || !ids[i] || ids[i] > n) return -1;
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	if (pg_contexts_read(file, typing, limit, name_limit, resolve, owner, &nc, &contexts, &nt, &terms)) return -1;
	if (nc != n || nt != 3 * n) return -1;
	const struct pg_occurrence **operands = pg_alloc(graph, max_arity * sizeof(*operands));
	if (!operands) return -1;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < inputs[i].count; ++j) operands[j] = all[inputs[i].operands[j] - 1];
		const struct pg_term *classifier = inputs[i].flags & 2 ? terms[3 * i + 2] : NULL;
		const struct pg_term *annotation = inputs[i].flags & 1 ? terms[3 * i + 1] : NULL;
		if (inputs[i].flags & 4) {
			const struct pg_context_map *map = pg_context_map(typing, operands[0]->context,
				contexts[i], inputs[i].count - 1, operands + 1);
			all[i] = pg_occurrence_mapped(typing, inputs[i].judgement, terms[3 * i],
				classifier, annotation, operands[0], map);
		} else {
			all[i] = pg_occurrence(typing, inputs[i].judgement, contexts[i], terms[3 * i],
				classifier, annotation, inputs[i].count, operands);
		}
		if (!all[i]) return -1;
	}
	for (size_t i = 0; i < nr; ++i) result[i] = all[ids[i] - 1];
	*count = (size_t)nr;
	*roots = result;
	return 0;
}
