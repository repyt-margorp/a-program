#ifndef A_PROGRAM_READBACK_SUPPORT_H
#define A_PROGRAM_READBACK_SUPPORT_H

#include "graph.h"

/* Derived syntax metadata, not part of a term's interning key or wire format. */
int pg_support_construct(struct pg_graph *graph, struct pg_term *term);
/* Read-only membership: 1 free, 0 absent, -1 metadata unavailable.
 * Walking a compressed pointer trie does not traverse/evaluate source terms. */
int pg_support_contains(const struct pg_term *term, const struct pg_object *binder);

#endif
