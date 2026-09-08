#include "context_io.h"
#include "graph_io.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGCTX\0";

struct context_record {
	struct pg_index_entry index;
	const struct pg_context *context;
	struct context_record *parent, *next;
	size_t id;
};

static struct context_record *lookup(struct pg_graph *arena, struct pg_index *index,
	const struct pg_context *context)
{
	uint64_t hash = (uintptr_t)context;
	for (struct pg_index_entry *p = pg_index_candidates(index, hash); p; p = p->next) {
		struct context_record *r = (struct context_record *)p;
		if (r->context == context) return r;
	}
	struct context_record *r = pg_alloc(arena, sizeof(*r));
	if (!r) return NULL;
	r->context = context;
	if (pg_index_insert(index, &r->index, hash) != 0) return NULL;
	return r;
}

int pg_contexts_write(FILE *file, size_t count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	if (!file || (count && !contexts) || (term_count && !terms)) return -1;
	struct pg_graph arena = {0};
	struct pg_index index = {0};
	struct context_record *first = NULL, *last = NULL;
	size_t size = 0;
	int status = -1;
	if (pg_graph_init(&arena) || pg_index_init(&index)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_context *cursor = contexts[i];
		struct context_record *stack = NULL, *parent = NULL;
		while (cursor) {
			struct context_record *r = lookup(&arena, &index, cursor);
			if (!r) goto done;
			if (r->id) { parent = r; break; }
			/* A previously entered unfinished declaration is a parent cycle. */
			if (r->next) goto done;
			r->next = stack ? stack : r;
			stack = r;
			cursor = cursor->parent;
		}
		while (stack) {
			struct context_record *r = stack;
			stack = r->next == r ? NULL : r->next;
			r->next = NULL;
			r->parent = parent;
			r->id = ++size;
			if (last) last->next = r;
			else first = r;
			last = parent = r;
		}
	}
	if (term_count > SIZE_MAX / sizeof(void *)) goto done;
	if (size > (SIZE_MAX / sizeof(void *) - term_count) / 2) goto done;
	size_t total = 2 * size + term_count;
	const struct pg_term **roots = pg_alloc(&arena, total * sizeof(*roots));
	if (!roots) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size)
		|| pg_wire_write_u64(file, count) || pg_wire_write_u64(file, term_count)) goto done;
	for (struct context_record *r = first; r; r = r->next) {
		if (!r->context->binder || r->context->binder->kind != PG_BINDER || !r->context->declared_type) goto done;
		if (pg_wire_write_u64(file, r->parent ? r->parent->id : 0)) goto done;
		roots[2 * (r->id - 1)] = pg_reference(&arena, r->context->binder);
		roots[2 * (r->id - 1) + 1] = r->context->declared_type;
	}
	for (size_t i = 0; i < count; ++i) {
		struct context_record *r = contexts[i] ? lookup(&arena, &index, contexts[i]) : NULL;
		if (contexts[i] && !r) goto done;
		if (pg_wire_write_u64(file, r ? r->id : 0)) goto done;
	}
	for (size_t i = 0; i < term_count; ++i) roots[2 * size + i] = terms[i];
	status = pg_graph_write(file, total, roots, name, owner);
done:
	pg_index_destroy(&index);
	pg_graph_destroy(&arena);
	return status;
}

int pg_contexts_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms)
{
	if (!file || !typing || !typing->contexts.capacity || !count || !contexts || !term_count || !terms) return -1;
	char header[8];
	uint64_t n, nc, nt;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nc) || pg_wire_read_u64(file, &nt)) return -1;
	if (n > limit || nc > limit - n || nt > limit - n - nc) return -1;
	if (limit > SIZE_MAX / sizeof(uint64_t) || limit > SIZE_MAX / sizeof(void *)) return -1;
	if (n > (SIZE_MAX - nt) / 2) return -1;
	struct pg_graph *graph = typing->graph;
	uint64_t *parents = pg_alloc(graph, (size_t)n * sizeof(*parents));
	uint64_t *ids = pg_alloc(graph, (size_t)nc * sizeof(*ids));
	const struct pg_context **all = pg_alloc(graph, (size_t)n * sizeof(*all));
	const struct pg_context **result = pg_alloc(graph, (size_t)nc * sizeof(*result));
	if (!parents || !ids || !all || !result) return -1;
	for (size_t i = 0; i < n; ++i)
		if (pg_wire_read_u64(file, &parents[i]) || parents[i] > i) return -1;
	for (size_t i = 0; i < nc; ++i)
		if (pg_wire_read_u64(file, &ids[i]) || ids[i] > n) return -1;
	size_t total = 0;
	const struct pg_term *const *roots = NULL;
	if (pg_graph_read(file, graph, limit, name_limit, resolve, owner, &total, &roots)) return -1;
	if (total != 2 * n + nt) return -1;
	for (size_t i = 0; i < n; ++i) {
		const struct pg_term *binder = roots[2 * i];
		if (binder->kind != PG_REFERENCE) return -1;
		all[i] = pg_context_bind(typing, parents[i] ? all[parents[i] - 1] : NULL,
			binder->as.reference, roots[2 * i + 1]);
		if (!all[i]) return -1;
	}
	for (size_t i = 0; i < nc; ++i) result[i] = ids[i] ? all[ids[i] - 1] : NULL;
	*count = (size_t)nc;
	*contexts = result;
	*term_count = (size_t)nt;
	*terms = roots + 2 * n;
	return 0;
}
