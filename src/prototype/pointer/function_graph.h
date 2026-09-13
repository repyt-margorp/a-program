#ifndef A_PROGRAM_POINTER_FUNCTION_GRAPH_H
#define A_PROGRAM_POINTER_FUNCTION_GRAPH_H

#include "evidence.h"
#include "iadt.h"

/* Generate ordinary indexed declarations from retained Lambda/case evidence.
 * No Returns predicate or new kernel rule. Relation formation and witness
 * production advance separately; neither proves an arbitrary user property.
 * Supports pure dependent result types and recursive fields. Generic
 * indices bound immediately before the scrutinee become graph indices, with
 * actual recursive fibers recovered from membership evidence. Fixed indices,
 * and intervening dependent environment binders are unsupported. Total pure
 * function fields supply a checked symbolic child at their actual arguments.
 * Remaining raw Pi arguments become additional graph
 * indices, not thunked result functions. Their dependent domains, recursive
 * call arguments and prior call outputs are checked by ordinary substitutions.
 * Retained Return/Fold/APP/Force evidence supplies
 * ordered call sites; each result type is instantiated at its own input by
 * checked substitution. Repeated calls have separate result/graph fields and
 * unused hypotheses contribute no fields. Schema and witness share this plan.
 * Typed beta/quotation/sequencing wrappers can expose the selected Match;
 * its motive and branches move through the same checked substitution.
 * Known constructor Matches expose their typed branch before recursive call
 * analysis, including raw Pi results and substituted fields. Neutral Matches
 * on distinct variable indices form a shared constructor-refined branch tree;
 * schema and witness use the same leaves and call prefixes. Saturated calls
 * to retained recursive Lambda definitions request their canonical graph and
 * witness from the owner. Their actual parameters instantiate the result and
 * relation; generic opaque callees remain unsupported.
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
/* Number of raw Pi arguments after the selected source scrutinee, before F. */
size_t pg_function_graph_trailing_arity(const struct pg_function_graph_work *work);
enum pg_function_graph_status pg_function_graph_advance(struct pg_function_graph_work *work, uint64_t budget);
/* A helper call waits for the same canonical graph/witness requested by @f and *f.
 * The supplied work is borrowed and must outlive this work. Pending inputs do
 * not grant evidence; only an owned, completed matching dependency is usable. */
const struct pg_evidence *pg_function_graph_dependency(const struct pg_function_graph_work *work);
int pg_function_graph_supply(struct pg_function_graph_work *work, const struct pg_function_graph_work *dependency);
/* Leading raw Lambda parameters become ordinary family abstractions. */
const struct pg_evidence *pg_function_graph_formation(const struct pg_function_graph_work *work);
/* Declaration before abstracting the original leading parameters. Case input
 * is its retained source ADT formation, or NULL for a direct-return graph.
 * Used to transfer source constructor names, never to infer membership. */
const struct pg_evidence *pg_function_graph_declaration(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_case_input(const struct pg_function_graph_work *work);
/* A generated leaf inherits its most recent nonsingleton source split's name.
 * Refined leaves do not retain the original flat source telescope layout. */
struct pg_function_graph_case_source {
	const struct pg_evidence *formation;
	const struct pg_object *constructor;
	int refined;
};
int pg_function_graph_case_source(const struct pg_function_graph_work *work,
	size_t index, struct pg_function_graph_case_source *source);
/* Construct a dependent result packet and its producer by ordinary induction
 * over the same source argument. Shares the generated relation above.
 * Returned packet formation is parameterized by the source input context;
 * its sole constructor stores output and graph evidence. The producer retains
 * the source result's totality grade; graph formation alone is not totality. */
enum pg_function_graph_status pg_function_graph_witness_advance(struct pg_function_graph_work *work, uint64_t budget);
const struct pg_evidence *pg_function_graph_witness(const struct pg_function_graph_work *work);
const struct pg_evidence *pg_function_graph_packet(const struct pg_function_graph_work *work);
void pg_function_graph_destroy(struct pg_function_graph_work *work);

#endif
