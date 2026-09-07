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
const struct pg_term *pg_pi(struct pg_classifiers *classifiers,
	const struct pg_term *domain, const struct pg_object *binder, const struct pg_term *codomain);
int pg_pi_view(const struct pg_term *term, const struct pg_term **domain,
	const struct pg_object **binder, const struct pg_term **codomain);

#endif
