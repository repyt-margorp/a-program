#include "occurrence_io.h"
#include "context_io.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGOCC\0";

struct record {
	struct pg_index_entry index;
	const struct pg_occurrence *source;
	size_t id, cursor;
	int active;
	struct record *pending, *next;
};

static struct record *record(struct pg_graph *arena, struct pg_index *index,
	const struct pg_occurrence *source)
{
	if (!source) return NULL;
	uint64_t hash = (uintptr_t)source;
	for (struct pg_index_entry *p = pg_index_candidates(index, hash); p; p = p->next) {
		struct record *r = (struct record *)p;
		if (r->source == source) return r;
	}
	struct record *r = pg_alloc(arena, sizeof(*r));
	if (!r) return NULL;
	r->source = source;
	if (pg_index_insert(index, &r->index, hash)) return NULL;
	return r;
}

int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct pg_graph arena = {0};
	struct pg_index index = {0};
	struct record *first = NULL, *last = NULL;
	size_t size = 0;
	int status = -1;
	if (pg_graph_init(&arena) || pg_index_init(&index)) goto done;
	for (size_t i = 0; i < count; ++i) {
		struct record *r = record(&arena, &index, roots[i]);
		if (!r) goto done;
		if (r->id) continue;
		while (r) {
			r->active = 1;
			if (r->cursor < r->source->operand_count) {
				struct record *child = record(&arena, &index, r->source->operands[r->cursor++]);
				if (!child || child->active) goto done;
				if (child->id) continue;
				child->pending = r;
				r = child;
			} else {
				r->id = ++size;
				r->active = 0;
				if (last) last->next = r;
				else first = r;
				last = r;
				r = r->pending;
			}
		}
	}
	if (size > SIZE_MAX / sizeof(void *) / 2) goto done;
	const struct pg_context **contexts = pg_alloc(&arena, size * sizeof(*contexts));
	const struct pg_term **terms = pg_alloc(&arena, 2 * size * sizeof(*terms));
	if (!contexts || !terms) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size) || pg_wire_write_u64(file, count)) goto done;
	for (struct record *r = first; r; r = r->next) {
		const struct pg_occurrence *o = r->source;
		contexts[r->id - 1] = o->context;
		terms[2 * (r->id - 1)] = o->core;
		terms[2 * (r->id - 1) + 1] = o->annotation ? o->annotation : o->core;
		if (fputc(o->annotation != NULL, file) == EOF || pg_wire_write_u64(file, o->operand_count)) goto done;
		for (size_t i = 0; i < o->operand_count; ++i) {
			struct record *child = record(&arena, &index, o->operands[i]);
			if (!child || pg_wire_write_u64(file, child->id)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) {
		struct record *r = record(&arena, &index, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = pg_contexts_write(file, size, contexts, 2 * size, terms, name, owner);
done:
	pg_index_destroy(&index);
	pg_graph_destroy(&arena);
	return status;
}

struct input {
	size_t count;
	int annotated;
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
	if (n > SIZE_MAX / 2) return -1;
	struct pg_graph *graph = typing->graph;
	struct input *inputs = pg_alloc(graph, (size_t)n * sizeof(*inputs));
	uint64_t *ids = pg_alloc(graph, (size_t)nr * sizeof(*ids));
	const struct pg_occurrence **all = pg_alloc(graph, (size_t)n * sizeof(*all));
	const struct pg_occurrence **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!inputs || !ids || !all || !result) return -1;
	size_t available = limit - (size_t)n - (size_t)nr, max_arity = 0;
	for (size_t i = 0; i < n; ++i) {
		inputs[i].annotated = fgetc(file);
		uint64_t arity;
		if (inputs[i].annotated != 0 && inputs[i].annotated != 1) return -1;
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
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &ids[i]) || !ids[i] || ids[i] > n) return -1;
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	if (pg_contexts_read(file, typing, limit, name_limit, resolve, owner, &nc, &contexts, &nt, &terms)) return -1;
	if (nc != n || nt != 2 * n) return -1;
	const struct pg_occurrence **operands = pg_alloc(graph, max_arity * sizeof(*operands));
	if (!operands) return -1;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < inputs[i].count; ++j) operands[j] = all[inputs[i].operands[j] - 1];
		all[i] = pg_occurrence(typing, contexts[i], terms[2 * i],
			inputs[i].annotated ? terms[2 * i + 1] : NULL, inputs[i].count, operands);
		if (!all[i]) return -1;
	}
	for (size_t i = 0; i < nr; ++i) result[i] = all[ids[i] - 1];
	*count = (size_t)nr;
	*roots = result;
	return 0;
}
