#ifndef A_PROGRAM_READBACK_SUPPORT_H
#define A_PROGRAM_READBACK_SUPPORT_H

#include "graph.h"

/* Derived syntax metadata, not part of a term's interning key or wire format. */
int pg_support_construct(struct pg_graph *graph, struct pg_term *term);

#endif
