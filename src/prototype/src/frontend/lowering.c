/* Lowering remains one translation unit to preserve solver state and graph-ID
 * allocation order while making the established phases physically visible. */
#include "lowering/context_and_type_lowering.inc"
#include "lowering/function_graph_generation.inc"
#include "lowering/graph_construction.inc"
#include "lowering/constraint_solver.inc"
#include "lowering/finalization_and_entrypoints.inc"
