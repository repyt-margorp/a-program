#ifndef A_PROGRAM_POINTER_GRAPH_INTERNAL_H
#define A_PROGRAM_POINTER_GRAPH_INTERNAL_H

#include "graph.h"

struct binder_pair {
	const struct pg_object *left;
	const struct pg_object *right;
	const struct binder_pair *parent;
};

struct alpha_entry {
	struct pg_index_entry index;
	const struct pg_term *left;
	const struct pg_term *right;
	const struct binder_pair *scope;
	const struct binder_pair *cursor;
	const struct pg_term *normalized[2];
	struct alpha_entry *next;
	unsigned stage;
	int structural_checked;
	unsigned congruence;
};

struct pg_comparison_state {
	struct pg_graph arena;
	struct pg_index seen;
	struct alpha_entry *pending;
	struct pg_comparison structural;
	size_t structural_tasks;
	void *policy;
	int (*normalize)(void *, const struct pg_term *, const struct pg_term **);
	enum pg_comparison_status status;
	uint64_t steps;
};

/* Restore only the index, not a claim that the retained comparison is valid. */
int pg_comparison_index(struct pg_comparison_state *context, struct alpha_entry *entry);

#endif
