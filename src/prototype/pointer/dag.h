#ifndef A_PROGRAM_POINTER_DAG_H
#define A_PROGRAM_POINTER_DAG_H

#include "graph.h"

/* Temporary pointer-keyed dependency order, not semantic interning. Children
 * are enumerated in order: callback returns 1 and a non-NULL child, 0 for end,
 * or -1 for error. NULL callback means leaf. Input objects remain immutable. */
struct pg_dag_node {
	struct pg_index_entry index;
	const void *key;
	size_t id;
	struct pg_dag_node *next;
};
struct pg_dag {
	struct pg_graph storage;
	struct pg_index index;
	struct pg_dag_node *first, *last;
	size_t count;
	int failed;
	int (*child)(void *, const void *, size_t, const void **);
	void *context;
};
int pg_dag_init(struct pg_dag *dag,
	int (*child)(void *, const void *, size_t, const void **), void *context);
/* Iterative postorder; previously completed roots cost one indexed lookup.
 * Cycles/errors poison the collector until destroy. IDs are local and 1-based. */
int pg_dag_add(struct pg_dag *dag, const void *root);
const struct pg_dag_node *pg_dag_find(const struct pg_dag *dag, const void *key);
void pg_dag_destroy(struct pg_dag *dag);

#endif
