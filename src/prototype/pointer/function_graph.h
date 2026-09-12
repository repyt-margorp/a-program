#ifndef A_PROGRAM_POINTER_FUNCTION_GRAPH_H
#define A_PROGRAM_POINTER_FUNCTION_GRAPH_H

#include "evidence.h"
#include "iadt.h"

/* Generate ordinary indexed declarations from retained Lambda/case evidence.
 * No Returns predicate or new kernel rule. Relation formation and witness
 * production advance separately; neither proves an arbitrary user property.
 * Initially supports a constant pure result type, non-indexed scrutinees and
 * direct recursive fields. Unknown callees/results remain unsupported.
 * One work object owns one generative declaration; a source producer must
 * memoize this request rather than generating a new family for each use. */
enum pg_function_graph_status {
	PG_FUNCTION_GRAPH_PENDING, PG_FUNCTION_GRAPH_DONE,
	PG_FUNCTION_GRAPH_UNSUPPORTED, PG_FUNCTION_GRAPH_ERROR
};
struct pg_function_graph_state;
struct pg_function_graph_work { struct pg_function_graph_state *state; };
/* Follow checked storage quotation/forcing and same-context projections only.
 * This preserves named aliases without choosing a type from erased Core. */
const struct pg_evidence *pg_function_graph_source(const struct pg_evidence *function);
int pg_function_graph_init(struct pg_function_graph_work *work,
	struct pg_typing *typing, struct pg_classifiers *classifiers,
	struct pg_whnf_work *evaluation, const struct pg_evidence *function);
enum pg_function_graph_status pg_function_graph_advance(struct pg_function_graph_work *work, uint64_t budget);
const struct pg_evidence *pg_function_graph_formation(const struct pg_function_graph_work *work);
/* Construct a dependent result packet and its producer by ordinary induction
 * over the same source argument. Shares the generated relation above.
 * Returned packet formation is parameterized by the source input context;
 * its sole constructor stores output and graph evidence. */
enum pg_function_graph_status pg_function_graph_witness_advance(struct pg_function_graph_work *work, uint64_t budget);
const struct pg_evidence *pg_function_graph_witness(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_packet(const struct pg_function_graph_work *work);
void pg_function_graph_destroy(struct pg_function_graph_work *work);

#endif
