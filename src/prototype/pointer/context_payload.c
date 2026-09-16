#include "context_payload.h"
#include "dag.h"

static int parent(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_context *context = key;
	if (index < (context->parent != NULL)) *child = context->parent;
	else if (index == (context->parent != NULL) && context->indices) *child = context->indices;
	else return 0;
	return 1;
}

int pg_contexts_pack(struct pg_graph *storage, size_t count,
	const struct pg_context *const *contexts, size_t term_count,
	const struct pg_term *const *terms, size_t *metadata_count,
	const uint64_t **metadata, size_t *root_count, const struct pg_term *const **roots)
{
	if (!storage || !metadata_count || !metadata || !root_count || !roots) return -1;
	if ((count && !contexts) || (term_count && !terms)) return -1;
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_dag_init(&dag, parent, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) if (contexts[i] && pg_dag_add(&dag, contexts[i])) goto done;
	size_t n = dag.count;
	if (count > SIZE_MAX / sizeof(uint64_t) - 2 || n > (SIZE_MAX / sizeof(uint64_t) - 2 - count) / 3) goto done;
	if (term_count > SIZE_MAX / sizeof(void *) || n > (SIZE_MAX / sizeof(void *) - term_count) / 2) goto done;
	uint64_t *ids = pg_alloc(storage, (2 + 3 * n + count) * sizeof(*ids));
	const struct pg_term **output = pg_alloc(storage, (2 * n + term_count) * sizeof(*output));
	if (!ids || !output) goto done;
	ids[0] = n; ids[1] = count;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_context *context = r->key;
		const struct pg_dag_node *prefix = pg_dag_find(&dag, context->parent);
		const struct pg_dag_node *indices = pg_dag_find(&dag, context->indices);
		if (!context->binder || context->binder->kind != PG_BINDER || !context->declared_type) goto done;
		if (context->judgement != PG_JUDGEMENT_VALUE && context->judgement != PG_JUDGEMENT_TYPE_FAMILY) goto done;
		ids[2 + 3 * (r->id - 1)] = prefix ? prefix->id : 0;
		ids[3 + 3 * (r->id - 1)] = context->judgement;
		ids[4 + 3 * (r->id - 1)] = indices ? indices->id : 0;
		output[2 * (r->id - 1)] = pg_reference(storage, context->binder);
		output[2 * (r->id - 1) + 1] = context->declared_type;
		if (!output[2 * (r->id - 1)]) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&dag, contexts[i]);
		ids[2 + 3 * n + i] = r ? r->id : 0;
	}
	for (size_t i = 0; i < term_count; ++i) {
		if (!terms[i]) goto done;
		output[2 * n + i] = terms[i];
	}
	*metadata_count = 2 + 3 * n + count; *metadata = ids;
	*root_count = 2 * n + term_count; *roots = output;
	status = 0;
done:
	pg_dag_destroy(&dag);
	return status;
}

int pg_contexts_unpack(struct pg_typing *typing, size_t metadata_count,
	const uint64_t *metadata, size_t root_count, const struct pg_term *const *roots,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms)
{
	if (!typing || !typing->contexts.capacity || !count || !contexts || !term_count || !terms) return -1;
	if (metadata_count < 2 || !metadata || (root_count && !roots)) return -1;
	uint64_t n = metadata[0], nc = metadata[1];
	if (n > (metadata_count - 2) / 3 || nc != metadata_count - 2 - 3 * n || n > root_count / 2) return -1;
	if (n > SIZE_MAX / sizeof(void *) || nc > SIZE_MAX / sizeof(void *)) return -1;
	for (size_t i = 0; i < n; ++i) {
		if (metadata[2 + 3 * i] > i || metadata[4 + 3 * i] > i) return -1;
		uint64_t kind = metadata[3 + 3 * i];
		if (kind != PG_JUDGEMENT_VALUE && kind != PG_JUDGEMENT_TYPE_FAMILY) return -1;
	}
	for (size_t i = 0; i < nc; ++i) if (metadata[2 + 3 * n + i] > n) return -1;
	for (size_t i = 0; i < root_count; ++i) if (!roots[i]) return -1;
	const struct pg_context **all = pg_alloc(typing->graph, (size_t)n * sizeof(*all));
	const struct pg_context **selected = pg_alloc(typing->graph, (size_t)nc * sizeof(*selected));
	if (!all || !selected) return -1;
	for (size_t i = 0; i < n; ++i) {
		const struct pg_term *binder = roots[2 * i];
		if (binder->kind != PG_REFERENCE) return -1;
		uint64_t prefix = metadata[2 + 3 * i], indices = metadata[4 + 3 * i];
		all[i] = pg_context_intern(typing, &(struct pg_context){
			.parent = prefix ? all[prefix - 1] : NULL, .binder = binder->as.reference,
			.declared_type = roots[2 * i + 1], .judgement = metadata[3 + 3 * i],
			.indices = indices ? all[indices - 1] : NULL});
		if (!all[i]) return -1;
	}
	for (size_t i = 0; i < nc; ++i) {
		uint64_t id = metadata[2 + 3 * n + i];
		selected[i] = id ? all[id - 1] : NULL;
	}
	*count = (size_t)nc; *contexts = selected;
	*term_count = root_count - 2 * (size_t)n;
	*terms = roots ? roots + 2 * n : NULL;
	return 0;
}
