#ifndef A_PROGRAM_POINTER_FUNCTION_GRAPH_H
#define A_PROGRAM_POINTER_FUNCTION_GRAPH_H

#include "evidence.h"
#include "iadt.h"

/* Generate ordinary indexed declarations from retained Lambda/case evidence.
 * No Returns predicate or new kernel rule. Relation formation and witness
 * production advance separately; neither proves an arbitrary user property.
 * Supports pure dependent result types, non-indexed scrutinees and
 * direct recursive fields. Retained Return/Fold/APP/Force evidence supplies
 * ordered call sites; each result type is instantiated at its own input by
 * checked substitution. Repeated calls have separate result/graph fields and
 * unused hypotheses contribute no fields. Schema and witness share this plan.
 * Conditional/nested cases and unknown callees/results remain unsupported.
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
/* Optional public telescope layout, supplied before advance. Each entry names
 * a source recursive field ordinal; duplicate entries represent distinct calls.
 * Counts and field associations must match the typed call plan exactly. This
 * changes neither execution order nor typing rules. The work copies the arrays. */
struct pg_function_graph_order { size_t count; const size_t *fields; };
int pg_function_graph_source_order(struct pg_function_graph_work *work,
	size_t count, const struct pg_function_graph_order *orders);
enum pg_function_graph_status pg_function_graph_advance(struct pg_function_graph_work *work, uint64_t budget);
/* Leading raw Lambda parameters become ordinary family abstractions. */
const struct pg_evidence *pg_function_graph_formation(const struct pg_function_graph_work *work);
/* Declaration before abstracting the original leading parameters. Case input
 * is its retained source ADT formation, or NULL for a direct-return graph.
 * Used to transfer source constructor names, never to infer membership. */
const struct pg_evidence *pg_function_graph_declaration(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_case_input(const struct pg_function_graph_work *work);
/* Construct a dependent result packet and its producer by ordinary induction
 * over the same source argument. Shares the generated relation above.
 * Returned packet formation is parameterized by the source input context;
 * its sole constructor stores output and graph evidence. */
enum pg_function_graph_status pg_function_graph_witness_advance(struct pg_function_graph_work *work, uint64_t budget);
const struct pg_evidence *pg_function_graph_witness(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_packet(const struct pg_function_graph_work *work);
void pg_function_graph_destroy(struct pg_function_graph_work *work);

#endif
