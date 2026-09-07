#ifndef A_PROGRAM_POINTER_GRAPH_H
#define A_PROGRAM_POINTER_GRAPH_H

#include <stddef.h>
#include <stdint.h>

struct pg_block;
struct pg_index_entry {
	struct pg_index_entry *next;
	uint64_t hash;
};
struct pg_index {
	struct pg_index_entry **buckets;
	size_t capacity;
	size_t count;
};
struct pg_graph {
	struct pg_block *blocks;
	struct pg_index terms;
};

int pg_index_init(struct pg_index *index);
void pg_index_destroy(struct pg_index *index);
struct pg_index_entry *pg_index_candidates(const struct pg_index *index, uint64_t hash);
int pg_index_insert(struct pg_index *index, struct pg_index_entry *entry, uint64_t hash);

enum pg_term_kind { PG_LAMBDA, PG_APPLICATION, PG_REFERENCE };
enum pg_object_kind { PG_BINDER, PG_SEMANTIC_OBJECT };

/* Semantic owners embed this header; Core does not inspect their payloads. */
struct pg_object_class {
	const char *name;
};
struct pg_object {
	enum pg_object_kind kind;
	const struct pg_object_class *owner;
};

struct pg_term {
	enum pg_term_kind kind;
	union {
		struct {
			const struct pg_object *binder;
			const struct pg_term *body;
		} lambda;
		struct {
			const struct pg_term *function;
			const struct pg_term *argument;
		} application;
		const struct pg_object *reference;
	} as;
};

int pg_graph_init(struct pg_graph *graph);
void pg_graph_destroy(struct pg_graph *graph);
/* Storage is stable until graph destruction and aligned for ordinary C types. */
void *pg_alloc(struct pg_graph *graph, size_t bytes);
const struct pg_object *pg_binder(struct pg_graph *graph);
const struct pg_term *pg_reference(struct pg_graph *graph, const struct pg_object *object);
const struct pg_term *pg_application(struct pg_graph *graph,
	const struct pg_term *function, const struct pg_term *argument);
const struct pg_term *pg_lambda(struct pg_graph *graph,
	const struct pg_object *binder, const struct pg_term *body);
/* Explicit syntactic alpha comparison, never used by interning or reduction.
 * 1 equal, 0 different, -1 allocation failure. */
int pg_alpha_equal(const struct pg_term *left, const struct pg_term *right);

#endif
