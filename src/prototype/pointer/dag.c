#include "dag.h"

#include <string.h>

struct record {
	struct pg_dag_node node;
	size_t cursor;
	int active;
	struct record *pending;
};

int pg_dag_init(struct pg_dag *dag,
	int (*child)(void *, const void *, size_t, const void **), void *context)
{
	memset(dag, 0, sizeof(*dag));
	dag->child = child;
	dag->context = context;
	return pg_index_init(&dag->index);
}

const struct pg_dag_node *pg_dag_find(const struct pg_dag *dag, const void *key)
{
	if (!key) return NULL;
	for (struct pg_index_entry *p = pg_index_candidates(&dag->index, (uintptr_t)key); p; p = p->next) {
		const struct pg_dag_node *node = (const struct pg_dag_node *)p;
		if (node->key == key) return node;
	}
	return NULL;
}

static struct record *record(struct pg_dag *dag, const void *key)
{
	if (!key) return NULL;
	const struct pg_dag_node *found = pg_dag_find(dag, key);
	if (found) return (struct record *)found;
	struct record *r = pg_alloc(&dag->storage, sizeof(*r));
	if (!r) return NULL;
	r->node.key = key;
	if (pg_index_insert(&dag->index, &r->node.index, (uintptr_t)key)) return NULL;
	return r;
}

int pg_dag_add(struct pg_dag *dag, const void *root)
{
	if (dag->failed || !dag->index.capacity) return -1;
	struct record *r = record(dag, root);
	if (!r) goto fail;
	if (r->node.id) return 0;
	while (r) {
		r->active = 1;
		const void *key = NULL;
		int status = dag->child ? dag->child(dag->context, r->node.key, r->cursor, &key) : 0;
		if (status == 1) {
			if (r->cursor == SIZE_MAX) goto fail;
			++r->cursor;
			struct record *child = record(dag, key);
			if (!child || child->active) goto fail;
			if (child->node.id) continue;
			child->pending = r;
			r = child;
		} else if (status == 0) {
			if (dag->count == SIZE_MAX) goto fail;
			r->active = 0;
			r->node.id = ++dag->count;
			if (dag->last) dag->last->next = &r->node;
			else dag->first = &r->node;
			dag->last = &r->node;
			r = r->pending;
		} else goto fail;
	}
	return 0;
fail:
	dag->failed = 1;
	return -1;
}

void pg_dag_destroy(struct pg_dag *dag)
{
	pg_index_destroy(&dag->index);
	pg_graph_destroy(&dag->storage);
	memset(dag, 0, sizeof(*dag));
}
