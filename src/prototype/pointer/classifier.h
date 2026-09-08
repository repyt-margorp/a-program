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
const struct pg_term *pg_return_type(struct pg_classifiers *classifiers, const struct pg_term *value_type);
const struct pg_term *pg_thunk_type(struct pg_classifiers *classifiers, const struct pg_term *computation_type);
int pg_return_type_view(const struct pg_term *term, const struct pg_term **value_type);
int pg_thunk_type_view(const struct pg_term *term, const struct pg_term **computation_type);

#endif
