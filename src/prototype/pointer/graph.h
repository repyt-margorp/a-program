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
	/* Lazy structural semantic-owner index; entries start with pg_object_entry. */
	struct pg_index objects;
};

int pg_index_init(struct pg_index *index);
void pg_index_destroy(struct pg_index *index);
struct pg_index_entry *pg_index_candidates(const struct pg_index *index, uint64_t hash);
int pg_index_insert(struct pg_index *index, struct pg_index_entry *entry, uint64_t hash);

enum pg_term_kind { PG_LAMBDA, PG_APPLICATION, PG_REFERENCE };
enum pg_object_kind { PG_BINDER, PG_SEMANTIC_OBJECT };

/* Owners embed this header; Core does not inspect their payloads. A binder
 * may have an owner (for example, cube geometry) without becoming a constant. */
struct pg_object_class {
	const char *name;
};
struct pg_object {
	enum pg_object_kind kind;
	const struct pg_object_class *owner;
};

struct pg_object_entry {
	struct pg_index_entry index;
	struct pg_object object;
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
/* Explicit syntactic alpha comparison, never an interning criterion.
 * Semantic operations may request it; Core construction does not normalize.
 * 1 equal, 0 different, -1 allocation failure. */
int pg_alpha_equal(const struct pg_term *left, const struct pg_term *right);
/* Syntactic independence: 1 no free occurrence, 0 occurs, -1 error.
 * Uses the scoped comparison walker without substitution or normalization. */
int pg_term_independent(const struct pg_term *term, const struct pg_object *binder);
struct pg_comparison_state;
struct pg_comparison { struct pg_comparison_state *state; };
enum pg_comparison_status { PG_COMPARISON_PENDING, PG_COMPARISON_EQUAL,
	PG_COMPARISON_DIFFERENT, PG_COMPARISON_ERROR };
/* Optional immutable normalization policy: -1 error, 0 progressed/pending,
 * 1 ready with output. Alpha-equal subproblems are accepted before invoking
 * the policy; normalization must respect alpha equality. NULL selects purely
 * structural alpha comparison. This walker
 * issues no typing/conversion certificate and never merges graph nodes. */
int pg_comparison_init(struct pg_comparison *work, const struct pg_term *left,
	const struct pg_term *right, void *policy,
	int (*normalize)(void *, const struct pg_term *, const struct pg_term **));
/* EQUAL means independent; DIFFERENT means a free occurrence was found.
 * The input graph is borrowed and unchanged; advance/destroy as a comparison. */
int pg_independence_init(struct pg_comparison *work, const struct pg_term *term,
	const struct pg_object *binder);
enum pg_comparison_status pg_comparison_advance(struct pg_comparison *work, uint64_t budget);
enum pg_comparison_status pg_comparison_status(const struct pg_comparison *work);
uint64_t pg_comparison_steps(const struct pg_comparison *work);
size_t pg_comparison_task_count(const struct pg_comparison *work);
void pg_comparison_destroy(struct pg_comparison *work);

#endif
