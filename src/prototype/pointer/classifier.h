#ifndef A_PROGRAM_POINTER_CLASSIFIER_H
#define A_PROGRAM_POINTER_CLASSIFIER_H

#include "graph.h"

struct pg_classifiers {
	struct pg_graph *graph;
	struct pg_index universes;
};

int pg_classifiers_init(struct pg_classifiers *classifiers, struct pg_graph *graph);
void pg_classifiers_destroy(struct pg_classifiers *classifiers);
const struct pg_term *pg_universe(struct pg_classifiers *classifiers, uint64_t level);
/* Structural views only; neither normalizes nor establishes well-formedness. */
int pg_universe_level(const struct pg_term *term, uint64_t *level);
/* Owner-controlled names for graph transport, not formation evidence.
 * Name writes into caller storage and returns NULL for unknown objects or an
 * insufficient buffer. Resolve accepts only canonical versioned names. */
const char *pg_classifier_name(const struct pg_object *object, char *buffer, size_t capacity);
const struct pg_object *pg_classifier_resolve(struct pg_classifiers *classifiers, const char *name);
const struct pg_term *pg_pi(struct pg_graph *graph,
	const struct pg_term *domain, const struct pg_object *binder, const struct pg_term *codomain);
int pg_pi_view(const struct pg_term *term, const struct pg_term **domain,
	const struct pg_object **binder, const struct pg_term **codomain);
/* Structural independence only, without normalization or formation evidence.
 * NULL means not a Pi, dependent codomain, or failed independence analysis. */
const struct pg_term *pg_pi_constant_codomain(const struct pg_term *pi);
struct pg_effect_row;
/* Closed sets of exact operation-label pointers. NULL is invalid/unknown,
 * never the empty set. Rows and referenced labels must outlive their uses.
 * This representation does not implement row metavariables or signatures. */
const struct pg_effect_row *pg_effect_row(struct pg_graph *graph,
	size_t count, const struct pg_object *const *labels);
const struct pg_effect_row *pg_effect_union(struct pg_graph *graph,
	const struct pg_effect_row *left, const struct pg_effect_row *right);
const struct pg_effect_row *pg_effect_difference(struct pg_graph *graph,
	const struct pg_effect_row *left, const struct pg_effect_row *right);
/* Invalid row: count returns SIZE_MAX; membership returns -1. */
size_t pg_effect_count(const struct pg_effect_row *row);
const struct pg_object *pg_effect_label(const struct pg_effect_row *row, size_t index);
int pg_effect_contains(const struct pg_effect_row *row, const struct pg_object *label);
/* Directed inclusion of closed sets; -1 for invalid/unknown rows. */
int pg_effect_subset(const struct pg_effect_row *left, const struct pg_effect_row *right);
const struct pg_term *pg_effect_reference(struct pg_graph *graph, const struct pg_effect_row *row);
const struct pg_effect_row *pg_effect_row_view(const struct pg_term *term);
/* Unaccepted set-expression graph. Construction interns exact APP tuples;
 * even closed operands are not evaluated here. The expression is not a
 * closed row or a formation proof. Solve interprets union distributively. */
const struct pg_term *pg_effect_join_term(struct pg_graph *graph,
	const struct pg_term *left, const struct pg_term *right);
int pg_effect_join_view(const struct pg_term *term,
	const struct pg_term **left, const struct pg_term **right);
/* Structural F spine, including unresolved row terms in unaccepted inputs.
 * These functions neither interpret the row nor establish formation. Closed
 * kernel consumers must continue using pg_effect_type_view below. */
const struct pg_term *pg_effect_type_spine(struct pg_classifiers *classifiers,
	const struct pg_term *effects, const struct pg_term *value_type);
int pg_effect_type_spine_view(const struct pg_term *term,
	const struct pg_term **effects, const struct pg_term **value_type);
const struct pg_term *pg_effect_type(struct pg_classifiers *classifiers,
	const struct pg_effect_row *effects, const struct pg_term *value_type);
int pg_effect_type_view(const struct pg_term *term,
	const struct pg_effect_row **effects, const struct pg_term **value_type);
/* Pure F is the empty-row instance; its view rejects nonempty rows. */
const struct pg_term *pg_return_type(struct pg_classifiers *classifiers, const struct pg_term *value_type);
const struct pg_term *pg_thunk_type(struct pg_classifiers *classifiers, const struct pg_term *computation_type);
int pg_return_type_view(const struct pg_term *term, const struct pg_term **value_type);
int pg_thunk_type_view(const struct pg_term *term, const struct pg_term **computation_type);

#endif
