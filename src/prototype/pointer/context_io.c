#include "context_io.h"
#include "graph_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const char magic[8] = "APGCTX\0";

static int parent(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_context *context = key;
	if (index || !context->parent) return 0;
	*child = context->parent;
	return 1;
}

int pg_contexts_write(FILE *file, size_t count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	const struct pg_graph_codec codec = {.name = name};
	return pg_contexts_write_descriptors(file, count, contexts, term_count, terms, &codec, owner);
}

int pg_contexts_write_descriptors(FILE *file, size_t count,
	const struct pg_context *const *contexts, size_t term_count,
	const struct pg_term *const *terms, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (count && !contexts) || (term_count && !terms)) return -1;
	struct pg_graph arena = {0};
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_graph_init(&arena) || pg_dag_init(&dag, parent, NULL)) goto done;
	for (size_t i = 0; i < count; ++i)
		if (contexts[i] && pg_dag_add(&dag, contexts[i])) goto done;
	size_t size = dag.count;
	if (term_count > SIZE_MAX / sizeof(void *)) goto done;
	if (size > (SIZE_MAX / sizeof(void *) - term_count) / 2) goto done;
	size_t total = 2 * size + term_count;
	const struct pg_term **roots = pg_alloc(&arena, total * sizeof(*roots));
	if (!roots) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size)
		|| pg_wire_write_u64(file, count) || pg_wire_write_u64(file, term_count)) goto done;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_context *context = r->key;
		const struct pg_dag_node *prefix = pg_dag_find(&dag, context->parent);
		if (!context->binder || context->binder->kind != PG_BINDER || !context->declared_type) goto done;
		if (pg_wire_write_u64(file, prefix ? prefix->id : 0)) goto done;
		roots[2 * (r->id - 1)] = pg_reference(&arena, context->binder);
		roots[2 * (r->id - 1) + 1] = context->declared_type;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&dag, contexts[i]);
		if (contexts[i] && !r) goto done;
		if (pg_wire_write_u64(file, r ? r->id : 0)) goto done;
	}
	for (size_t i = 0; i < term_count; ++i) roots[2 * size + i] = terms[i];
	status = pg_graph_write_descriptors(file, total, roots, codec, owner);
done:
	pg_dag_destroy(&dag);
	pg_graph_destroy(&arena);
	return status;
}

int pg_contexts_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms)
{
	const struct pg_graph_codec codec = {.resolve = resolve};
	return pg_contexts_read_descriptors(file, typing, limit, name_limit, &codec, owner,
		count, contexts, term_count, terms);
}

int pg_contexts_read_descriptors(FILE *file, struct pg_typing *typing,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
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
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, owner, &total, &roots)) return -1;
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
