#ifndef A_PROGRAM_POINTER_FUNCTION_WITNESS_H
#define A_PROGRAM_POINTER_FUNCTION_WITNESS_H

#include "function_graph.h"

/* Internal proof production, independent of surface witness-access syntax.
 * Construct a dependent output/graph packet by ordinary induction over the
 * graph's shared source plan. Formation alone does not prove totality.
 * Callers supply completed helper witnesses before constructing a dependent
 * witness; graph formation itself only needs helper graph formations. */
enum pg_function_graph_status pg_function_graph_witness_advance(struct pg_function_graph_work *work, uint64_t budget);
const struct pg_evidence *pg_function_graph_witness(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_packet(const struct pg_function_graph_work *work);

#endif
