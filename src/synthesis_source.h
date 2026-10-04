#ifndef A_PROGRAM_POINTER_SYNTHESIS_SOURCE_H
#define A_PROGRAM_POINTER_SYNTHESIS_SOURCE_H

#include "synthesis_work.h"

/* Interned lexical inputs shared by elaboration owners. Handler and definition
 * progress remain opaque here; this is not a universal synthesis payload. */
struct pg_source_scope {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder, *associated_binder;
	enum pg_source_association association;
	struct pg_synthesis_input context;
	struct pg_synthesis_job *definitions;
	struct pg_synthesis_job *registration;
	struct pg_synthesis_input value;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	const struct pg_source_scope *imports;
	struct handler_state *effect_owner;
	struct pg_synthesis_job *clause;
};

/* Actual sequencing inputs, shared by block, APP and computed Match contexts.
 * This is a lexical bind frame, not another representation of the program. */
struct pg_synthesis_sequence_frame {
	struct pg_synthesis_job *input;
	struct pg_synthesis_input context;
	const struct pg_synthesis_sequence_frame *parent;
	int binds;
};

/* Borrowed committed registration frontier, including failed registrations.
 * A failed item does not advance its cursor. Entries keep their source recipes;
 * registration completion is name publication, not acceptance of those entries.
 * A checkpoint owner must establish provenance and restore child state and
 * scheduling separately before allowing Solve to advance. */
struct pg_definition_frontier {
	size_t count, indexed, activated, position;
	struct pg_synthesis_job *const *entries;
	int complete;
};
struct pg_synthesis_job *pg_synthesis_prepare_module(struct pg_synthesis *, struct pg_synthesis_job *);
int pg_synthesis_definition_frontier(const struct pg_synthesis *, const struct pg_synthesis_job *,
	struct pg_definition_frontier *);
/* Rebuild the name index and activation gates on a fresh registration. A
 * completed registration needs its complete input table and outer registration,
 * not typed context acceptance. The descriptive traversal position grants no
 * child acceptance; module Solve checks its prefix before skipping entries. */
int pg_synthesis_definition_resume(struct pg_synthesis *, struct pg_synthesis_job *,
	const struct pg_definition_frontier *);

/* Definition bodies and module-check traversal. Getters also expose completed
 * owners; attachment requires pending owners. These borrow/restore only local
 * edges/cursors. Child work/results must be restored or checked
 * separately, and the enclosing checkpoint must replace scheduling before Solve.
 * The module position is its next entry, or 0/1 before/after named selection. */
struct pg_module_frontier {
	size_t position;
	struct pg_synthesis_job *previous;
};
int pg_synthesis_definition_body(const struct pg_synthesis *, const struct pg_synthesis_job *, struct pg_synthesis_job **);
int pg_synthesis_definition_resume_body(struct pg_synthesis *, struct pg_synthesis_job *, struct pg_synthesis_job *);
int pg_synthesis_module_frontier(const struct pg_synthesis *, const struct pg_synthesis_job *, struct pg_module_frontier *);
int pg_synthesis_module_resume(struct pg_synthesis *, struct pg_synthesis_job *, const struct pg_module_frontier *);

/* Plain identifier continuation: lexical input plus its projection rule,
 * or a VARIABLE rule using the scope's existing binder. No name table copy.
 * 1 supported, 0 another source form/state, -1 invalid ownership. Attachment
 * shares ordinary rule preparation and requires usable lexical inputs;
 * it does not resolve qualified members, run Solve or grant evidence. Restore
 * source scopes and child jobs first, then the enclosing schedule. Discard an
 * unpublished import after an attachment error. */
struct pg_reference_frontier {
	struct pg_synthesis_input value;
	struct pg_synthesis_job *rule;
};
int pg_synthesis_reference_frontier(const struct pg_synthesis *, const struct pg_synthesis_job *,
	struct pg_reference_frontier *);
int pg_synthesis_reference_resume(struct pg_synthesis *, struct pg_synthesis_job *,
	const struct pg_reference_frontier *);

/* Constructor continuation immediately after field-scope construction, before
 * abstracting its body. Borrow the checked map, not the completed worker's
 * history. 1 ready, 0 another stage, -1 wrong owner/request. Attachment requires
 * known provenance and already checked premises; it performs no Solve or proof
 * admission. Lexical constructor metadata must be restored by its source owner.
 * Advanced constructor bodies and unfinished field scopes are separate owners. */
int pg_synthesis_constructor_ready_scope(const struct pg_synthesis *, const struct pg_synthesis_job *,
	const struct pg_evidence **);
struct pg_synthesis_job *pg_synthesis_constructor_from_scope(struct pg_synthesis *,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, const struct pg_evidence *scope);
/* Started abstraction uses the existing function/body/rule workers. Restoring
 * its checked constructor leaf does not run those workers or accept a result.
 * Restore lexical metadata first and each child's cursor/queue separately. */
int pg_synthesis_constructor_body(const struct pg_synthesis *, const struct pg_synthesis_job *,
	const struct pg_evidence **scope, struct pg_synthesis_job **body);
int pg_synthesis_constructor_resume_body(struct pg_synthesis *, struct pg_synthesis_job *,
	const struct pg_evidence *leaf);

/* Restored lexical binder allocation. Each owner keeps its own cursor;
 * this payload describes source addresses, not a second typed Context. */
struct pg_source_context_allocation {
	const struct pg_context *prefix, *end;
	size_t count, next;
	const struct pg_context *contexts[];
};
int pg_synthesis_context_allocation_at(struct pg_synthesis *,
	struct pg_source_context_allocation **, const struct pg_context *,
	const struct pg_context *, int started);
const struct pg_object *pg_synthesis_constructor_binder(struct pg_synthesis *,
	const struct pg_context *, const struct pg_object *, size_t);
int pg_synthesis_constructor_scope_allocation(const struct pg_synthesis_job *,
	const struct pg_context **, const struct pg_context **);

const struct pg_source_scope *pg_synthesis_intern_scope(struct pg_synthesis *, struct pg_source_scope);
int pg_synthesis_register_name(struct pg_synthesis *, struct pg_index *, struct pg_token);
/* Source names/allocation provenance only; never proof admission. */
int pg_synthesis_register_source_allocation(struct pg_synthesis *, struct pg_synthesis_job *);
struct source_constructor;
int pg_synthesis_source_metadata(struct pg_synthesis *, const struct pg_evidence *, struct pg_synthesis_job *);
const struct pg_evidence *pg_synthesis_scope_context(const struct pg_source_scope *);
int pg_synthesis_scope_wait(struct pg_synthesis *, struct pg_synthesis_job *, const struct pg_source_scope *);
const struct pg_object *pg_synthesis_source_binder(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *, size_t slot, const struct pg_object *);
const struct pg_source_scope *pg_synthesis_scope_bind(struct pg_synthesis *,
	const struct pg_source_scope *, struct pg_token, const struct pg_object *,
	struct pg_synthesis_input, const struct pg_object *, struct pg_synthesis_job *, enum pg_source_association);
struct pg_synthesis_job *pg_synthesis_plain_rule_inputs(struct pg_synthesis *, enum pg_evidence_rule,
	const struct pg_object *, size_t, const struct pg_synthesis_input *);
/* Known premises have no producer Job; this view never allocates an adapter. */
struct pg_synthesis_input pg_synthesis_rule_input(const struct pg_synthesis *, const struct pg_synthesis_job *, size_t);
struct pg_synthesis_structure pg_synthesis_term_structure_input(struct pg_synthesis *, struct pg_synthesis_input);
struct pg_synthesis_structure pg_synthesis_type_structure_input(struct pg_synthesis *, struct pg_synthesis_input);
struct pg_synthesis_structure pg_synthesis_classifier_structure_input(struct pg_synthesis *, struct pg_synthesis_input);
struct pg_synthesis_structure pg_synthesis_structure_input(struct pg_synthesis *,
	struct pg_synthesis_input, const struct pg_synthesis_work_class *, int classifier, int type);
const struct pg_object *pg_synthesis_input_context_binder(struct pg_synthesis_input);
/* Checked declarations borrow their stored type; unfinished contexts keep
 * their existing discovery owner. This view does not admit a typing rule. */
struct pg_synthesis_structure pg_synthesis_declared_type(struct pg_synthesis *,
	struct pg_synthesis_input, const struct pg_object *);
struct pg_pending *pg_synthesis_classifier_in(struct pg_synthesis *, struct pg_synthesis_input,
	struct pg_synthesis_input);
const struct pg_derivation_input *pg_synthesis_plain_derivation(const struct pg_synthesis_job *);
/* Exact descriptive header comparison, without accepting premises or scheduling. */
int pg_synthesis_rule_header_matches(const struct pg_synthesis_job *, const struct pg_derivation_input *);
/* Borrow/restore a rule's premise cursor before endpoint computation starts.
 * 1 supplies a cursor, 0 needs another state owner, -1 rejects the request.
 * Resume needs a fresh pending rule and locally checked skipped premises.
 * It grants no conclusion, runs no Solve and leaves scheduling to its owner.
 * Effect/comparison/reduction/Reindex action state must be transported separately. */
int pg_synthesis_rule_frontier(const struct pg_synthesis *, const struct pg_synthesis_job *, size_t *);
int pg_synthesis_rule_resume(struct pg_synthesis *, struct pg_synthesis_job *, size_t);
const struct pg_synthesis_job *pg_synthesis_operation_origin(const struct pg_synthesis_job *);
/* Follow an existing lexical alias edge, without evaluation or acceptance. */
struct pg_synthesis_job *pg_synthesis_source_origin(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_source_application(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
int pg_synthesis_star_application(const struct pg_syntax *);
int pg_synthesis_hypothesis_syntax(const struct pg_syntax *);
int pg_synthesis_source_star(struct pg_synthesis *, struct pg_synthesis_job *);
struct constructor_callable;
struct constructor_callable pg_synthesis_callable(const struct pg_synthesis *, const struct pg_synthesis_job *);
const struct constructor_callable *pg_synthesis_application_callable(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_callable_source(struct pg_synthesis *, struct pg_synthesis_job *);
int pg_synthesis_is_match(const struct pg_synthesis_job *);
const struct pg_evidence *pg_synthesis_match_result_input(struct pg_synthesis *,
	struct pg_synthesis_job *, const struct pg_evidence *);
struct pg_synthesis_job *pg_synthesis_source_lambda(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
struct pg_synthesis_job *pg_synthesis_source_pi(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
struct pg_synthesis_job *pg_synthesis_source_quote(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
const struct pg_syntax *pg_synthesis_block_syntax(const struct pg_syntax *);
size_t pg_synthesis_block_end(const struct pg_syntax *);
struct pg_synthesis_job *pg_synthesis_source_block(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *);
/* Visited statement scopes belong to the block cursor; this never runs Solve. */
const struct pg_source_scope *pg_synthesis_block_scope(const struct pg_synthesis_job *, size_t);
struct pg_synthesis_job *pg_synthesis_binding_expect(struct pg_synthesis *,
	const struct pg_source_scope *, struct pg_synthesis_input, struct pg_synthesis_input);
int pg_synthesis_prepare_value_argument(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_synthesis_input, struct pg_synthesis_job **);
int pg_synthesis_logical_family_signature(const struct pg_term *);
struct pg_synthesis_job *pg_synthesis_quote_operand(const struct pg_synthesis_job *);
int pg_synthesis_input_kind(struct pg_synthesis_input, const struct pg_derivation_input **);
/* Type-position normalization uses the ordinary CBPV result query. */
const struct pg_evidence *pg_synthesis_type_input(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_synthesis_job **, const struct pg_evidence *, const struct pg_evidence *);
/* Known polarity borrows the actual input/rule after Context validation.
 * Existing discovery identity remains stable when operands complete. */
struct pg_synthesis_input pg_synthesis_body(struct pg_synthesis *, struct pg_synthesis_input, struct pg_synthesis_input);
struct pg_synthesis_input pg_synthesis_body_input(const struct pg_synthesis_job *);
/* Restore only the already-selected adapter. Selection shares the normal body
 * factory; its child rule still needs its own progress and evidence checking. */
int pg_synthesis_body_resume(struct pg_synthesis *, struct pg_synthesis_job *);
struct pg_synthesis_input pg_synthesis_classifier_formation_input(struct pg_synthesis *, struct pg_pending *);
const struct pg_object *pg_synthesis_pi_scope_binder(const struct pg_synthesis_job *);
const struct pg_object *pg_synthesis_context_binder(const struct pg_synthesis_job *);
struct pg_synthesis_input pg_synthesis_scope_context_input(const struct pg_synthesis_job *);
const struct pg_synthesis_work_class *pg_synthesis_binding_type_class(const struct pg_synthesis_job *);
/* Oracle-local structural selection. Pure input projections borrow the existing
 * query; constructing or inspecting a new shape retains its ordinary owner. */
int pg_synthesis_function_type_rule(enum pg_evidence_rule);
int pg_synthesis_function_structure_input(struct pg_synthesis *, struct pg_synthesis_input,
	int classifier, struct pg_synthesis_structure *);
/* Function graph checking stays in the function owner. Source callbacks retain
 * only names/field layout, not its cursor, status, formation or worker graph. */
struct pg_function_graph_work;
struct pg_synthesis_graph_names;
struct pg_synthesis_job *pg_synthesis_function_graph(struct pg_synthesis *,
	const struct pg_evidence *, const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_function_graph_request(struct pg_synthesis *, const struct pg_evidence *);
int pg_synthesis_graph_order(struct pg_synthesis *, struct pg_function_graph_work *,
	const struct pg_synthesis_job *, struct pg_synthesis_graph_names **);
int pg_synthesis_graph_exports(struct pg_synthesis *, struct pg_function_graph_work *,
	const struct pg_source_scope **, struct pg_synthesis_graph_names **);
int pg_synthesis_graph_source(const struct pg_synthesis_job *, const struct pg_evidence **,
	const struct pg_source_scope **, const struct pg_synthesis_graph_names **);
int pg_synthesis_cbpv_type_rule(enum pg_evidence_rule);
int pg_synthesis_cbpv_structure_input(struct pg_synthesis *, struct pg_synthesis_input,
	int classifier, struct pg_synthesis_structure *);
struct pg_synthesis_job *pg_synthesis_application_domain(struct pg_synthesis *,
	struct pg_synthesis_input, struct pg_synthesis_input);
/* Checked logical-family/CBPV adapters, never expected-type inference. */
struct pg_synthesis_job *pg_synthesis_family_contract(struct pg_synthesis *,
	struct pg_synthesis_input, struct pg_synthesis_input);
struct pg_synthesis_input pg_synthesis_family_function(struct pg_synthesis *, struct pg_synthesis_input);
int pg_synthesis_typed_input(struct pg_synthesis *, const struct pg_evidence *, const struct pg_evidence *);
int pg_synthesis_input_value_kind(struct pg_synthesis_input);
int pg_synthesis_await_preparation(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_synthesis_job *, struct pg_synthesis_job **);
enum pg_comparison_status pg_synthesis_probe_advance(struct pg_synthesis *, struct pg_synthesis_job *, struct pg_comparison *);
enum pg_comparison_status pg_synthesis_independence(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_comparison *, const struct pg_term *, const struct pg_object *);
struct pg_synthesis_job *pg_synthesis_compare_terms(struct pg_synthesis *,
	const struct pg_term *, const struct pg_term *);
/* IADT-owned checked index transport. The second form discovers a result
 * independent of the destination's excluded binders; it is not TypeExpect. */
struct pg_synthesis_job *pg_synthesis_index_transport(struct pg_synthesis *,
	struct pg_synthesis_input, struct pg_synthesis_input, struct pg_synthesis_input);
struct pg_synthesis_job *pg_synthesis_index_result(struct pg_synthesis *,
	struct pg_synthesis_input, struct pg_synthesis_input, struct pg_synthesis_input);
struct pg_synthesis_input pg_synthesis_index_transport_target(const struct pg_synthesis_job *);

#endif
