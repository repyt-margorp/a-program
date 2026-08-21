#ifndef A_PROGRAM_PROTOTYPE_GRAPH_COMPILE_METADATA_H
#define A_PROGRAM_PROTOTYPE_GRAPH_COMPILE_METADATA_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_diagnostic.h"
#include "a_program/graph/typed_occurrence_model.h"
#include "a_program/graph/verification.h"
#include "a_program/kernel/context.h"
#include "a_program/dimension/operator.h"

struct prototype_compile_constructor_export {
	uint32_t type_export_index;
	int name_symbol_id;
	uint32_t ordinal;
	uint32_t readback_first_field_type;
	uint32_t readback_field_count;
	uint32_t curried_classifier_cache;
};

struct prototype_compile_type_export {
	int name_symbol_id;
	uint32_t type_id;
	struct prototype_type_representation_fingerprint representation_fingerprint;
	uint32_t first_constructor_export;
	uint32_t constructor_count;
};

struct prototype_resolve_error {
	int kind;
	int name_symbol_id;
	int member_symbol_id;
	uint32_t ast;
	struct prototype_source_span span;
};

enum prototype_resolution_event_kind {
	PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR = 1
};

enum prototype_resolution_item_state {
	PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED = 1,
	PROTOTYPE_RESOLUTION_ITEM_RESOLVED = 2,
	PROTOTYPE_RESOLUTION_ITEM_ERROR = 3
};

struct prototype_resolution_item {
	uint32_t id;
	int kind;
	int state;
	uint32_t created_iteration;
	uint32_t resolved_iteration;
	uint32_t ast;
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	int symbol_id;
	uint32_t resolved_owner;
	uint32_t resolved_id;
};

struct prototype_resolution_event {
	uint32_t item_id;
	uint32_t iteration;
	int kind;
	int from_state;
	int to_state;
	uint32_t ast;
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	int symbol_id;
	uint32_t resolved_owner;
	uint32_t resolved_id;
};

struct prototype_resolution_iteration {
	uint32_t iteration;
	size_t unresolved_before;
	size_t unresolved_after;
	size_t event_start;
	size_t event_count;
};

enum prototype_compile_policy {
	PROTOTYPE_COMPILE_POLICY_STRICT = 1,
	PROTOTYPE_COMPILE_POLICY_HYBRID = 2,
	PROTOTYPE_COMPILE_POLICY_EXPLORATORY = 3
};

enum prototype_definition_thunk_policy {
	PROTOTYPE_DEFINITION_THUNK_IMPLICIT = 1,
	PROTOTYPE_DEFINITION_THUNK_EXPLICIT = 2
};

enum prototype_runtime_capability {
	PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER = 1u << 0,
	PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH = 1u << 1,
	PROTOTYPE_RUNTIME_CAPABILITY_HANDLER = 1u << 2,
	PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL = 1u << 3
};

enum prototype_backend_target {
	PROTOTYPE_BACKEND_INTERPRETER = 1,
	PROTOTYPE_BACKEND_C = 2,
	PROTOTYPE_BACKEND_VERILOG = 3
};

struct prototype_compile_metadata {
	int compile_policy;
	int definition_thunk_policy;
	int selected_entry_symbol_id;
	uint32_t selected_entry_term;
	uint32_t selected_entry_classifier;
	uint32_t selected_entry_occurrence;
	uint64_t required_runtime_capabilities;
	uint64_t normalization_step_limit;
	uint64_t normalization_steps_used;
	uint64_t solver_step_limit;
	uint64_t solver_steps_used;
	int solver_exhausted;
	uint64_t solver_constraint_count;
	uint64_t solver_solved_count;
	uint64_t solver_residual_count;
	uint64_t solver_incomplete_count;
	/* Runtime-only compiler work counters. They measure the ahead-of-time
	 * computation used to construct the accepted graph and are not artifact
	 * semantics. */
	uint64_t constraint_generation_pass_count;
	uint64_t constraint_index_pass_count;
	uint64_t computation_constraint_generation_pass_count;
	uint64_t constraint_enqueue_count;
	uint64_t constraint_pop_count;
	uint64_t context_resolution_pass_count;
	uint64_t context_index_rebuild_count;
	uint64_t substitution_index_rebuild_count;
	uint64_t context_resolution_request_count;
	uint64_t context_resolution_skip_count;
	uint64_t context_resolution_context_visit_count;
	uint64_t context_resolution_context_change_count;
	uint64_t context_resolution_context_insert_count;
	uint64_t context_resolution_substitution_visit_count;
	uint64_t context_resolution_substitution_rebase_count;
	uint64_t context_resolution_substitution_insert_count;
	uint64_t context_resolution_root_projection_count;
	uint64_t binder_owner_index_rebuild_count;
	uint64_t graph_build_time_ns;
	uint64_t fixed_point_time_ns;
	uint64_t proof_materialization_time_ns;
	uint64_t result_evidence_time_ns;
	uint64_t post_result_closure_time_ns;
	uint64_t accepted_replay_time_ns;
	uint64_t proof_materialization_pass_count;
	uint64_t proof_materialization_full_scan_count;
	uint64_t proof_materialization_round_count;
	uint64_t proof_materialization_occurrence_visit_count;
	uint64_t post_result_consumer_retry_count;
	uint64_t proof_reify_root_count;
	uint64_t proof_reify_recursive_count;
	uint64_t proof_reify_success_count;
	uint64_t proof_reify_residual_count;
	uint64_t proof_reify_failure_count;
	uint64_t proof_reify_accepted_reuse_count;
	uint64_t proof_reify_current_pass_reuse_count;
	uint64_t proof_reify_cycle_count;
	uint64_t result_evidence_claim_count;
	uint64_t termination_evidence_claim_count;
	/* Frozen operational projection consumed by Core execution. It deliberately
	 * excludes static type declarations and proof state. */
	struct prototype_term_reduction_environment reduction_environment;

	struct prototype_context_db contexts;
	struct prototype_substitution_db substitutions;
	struct prototype_dimension_operator_db dimension_operators;
	/* Accepted artifact authority for EXTEND nodes. This table is separate from
	 * SubstitutionDB so structurally valid compiler candidates remain usable
	 * without pretending that every candidate is a certified CwF morphism. */
	uint32_t* accepted_substitution_claims;
	size_t accepted_substitution_claim_capacity;

	struct prototype_typed_occurrence_graph typed_occurrences;

	struct prototype_occurrence_effect_constraint* effect_constraints;
	size_t effect_constraint_count;
	size_t effect_constraint_capacity;

	struct prototype_verification_db verification;

	struct prototype_compile_label* labels;
	size_t label_count;
	size_t label_capacity;

	struct prototype_compile_type_export* type_exports;
	size_t type_export_count;
	size_t type_export_capacity;

	struct prototype_compile_constructor_export* constructor_exports;
	size_t constructor_export_count;
	size_t constructor_export_capacity;

	struct prototype_resolve_error* resolve_errors;
	size_t resolve_error_count;
	size_t resolve_error_capacity;

	struct prototype_compile_diagnostic* compile_diagnostics;
	size_t compile_diagnostic_count;
	size_t compile_diagnostic_capacity;

	struct prototype_resolution_item* resolution_items;
	size_t resolution_item_count;
	size_t resolution_item_capacity;

	struct prototype_resolution_iteration* resolution_iterations;
	size_t resolution_iteration_count;
	size_t resolution_iteration_capacity;

	struct prototype_resolution_event* resolution_events;
	size_t resolution_event_count;
	size_t resolution_event_capacity;
};

/* Immutable-by-contract publication view. The contained DB values are shallow
 * snapshots whose counts fix the published prefixes; backing storage remains
 * owned by the compiler session. Mutable solver and diagnostic state is not
 * exposed through this boundary. */
struct prototype_frozen_module_snapshot {
	struct prototype_term_reduction_environment reduction_environment;
	struct prototype_context_db contexts;
	struct prototype_substitution_db substitutions;
	struct prototype_dimension_operator_db dimension_operators;
	struct prototype_typed_occurrence_graph typed_occurrences;
	struct prototype_verification_db verification;
	uint32_t selected_entry_term;
	uint32_t selected_entry_classifier;
	uint32_t selected_entry_occurrence;
	uint64_t required_runtime_capabilities;
};

int prototype_compile_metadata_frozen_snapshot(
	const struct prototype_compile_metadata* metadata,
	struct prototype_frozen_module_snapshot* p_snapshot
);

struct prototype_type_inspection {
	uint32_t body_occurrence;
	uint32_t body_classifier;
	uint32_t exposed_occurrence;
	uint32_t exposed_classifier;
	uint32_t expectation_classifier;
	uint32_t expectation_claim_id;
};

enum prototype_type_inspection_state {
	PROTOTYPE_TYPE_INSPECTION_INVALID = 0,
	PROTOTYPE_TYPE_INSPECTION_AVAILABLE = 1,
	PROTOTYPE_TYPE_INSPECTION_UNAVAILABLE = 2,
	PROTOTYPE_TYPE_INSPECTION_AMBIGUOUS = 3
};

/* Read-only projection of already accepted compilation metadata. It performs
 * no synthesis, normalization, interning, or proof construction. AMBIGUOUS
 * means that a published label exists but does not carry one coherent selected
 * Operation principal; the query never falls back to Term-ID evidence. */
enum prototype_type_inspection_state prototype_compile_metadata_inspect_type(
	const struct prototype_compile_metadata* metadata,
	int name_symbol_id,
	struct prototype_type_inspection* p_inspection
);

void prototype_compile_metadata_set_accepted_substitution_claim_storage(
	struct prototype_compile_metadata* metadata,
	uint32_t* claim_ids,
	size_t claim_id_capacity
);
int prototype_compile_metadata_record_accepted_substitution_claim(
	struct prototype_compile_metadata* metadata,
	uint32_t substitution_id,
	uint32_t claim_id
);
uint32_t prototype_compile_metadata_accepted_substitution_claim(
	const struct prototype_compile_metadata* metadata,
	uint32_t substitution_id
);
int prototype_compile_metadata_append_accepted_substitution_claims(
	struct prototype_compile_metadata* target,
	const struct prototype_compile_metadata* source,
	const uint32_t* substitution_relocation,
	size_t substitution_relocation_count,
	const uint32_t* claim_relocation,
	size_t claim_relocation_count
);

void prototype_compile_metadata_init(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_label* labels,
	size_t label_capacity,
	struct prototype_compile_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_compile_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	struct prototype_resolve_error* resolve_errors,
	size_t resolve_error_capacity,
	struct prototype_resolution_item* resolution_items,
	size_t resolution_item_capacity,
	struct prototype_resolution_iteration* resolution_iterations,
	size_t resolution_iteration_capacity,
	struct prototype_resolution_event* resolution_events,
	size_t resolution_event_capacity,
	struct prototype_context* contexts,
	size_t context_capacity,
	struct prototype_substitution* substitutions,
	size_t substitution_capacity,
	struct prototype_typed_occurrence* occurrences,
	size_t occurrence_capacity,
	struct prototype_typed_occurrence_edge* occurrence_edges,
	size_t occurrence_edge_capacity,
	struct prototype_typed_occurrence_match_case* occurrence_match_cases,
	size_t occurrence_match_case_capacity,
	struct prototype_typed_occurrence_fold_clause* occurrence_fold_clauses,
	size_t occurrence_fold_clause_capacity,
	struct prototype_occurrence_effect_constraint* effect_constraints,
	size_t effect_constraint_capacity,
	struct prototype_verification_obligation* verification_obligations,
	size_t verification_obligation_capacity
);

void prototype_compile_metadata_set_dimension_storage(
	struct prototype_compile_metadata* metadata,
	struct prototype_dimension_operator* operators,
	size_t operator_capacity,
	struct prototype_dimension_axis_image* images,
	size_t image_capacity
);

void prototype_compile_metadata_set_diagnostic_storage(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_diagnostic* diagnostics,
	size_t diagnostic_capacity
);

int prototype_compile_metadata_add_diagnostic(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_diagnostic diagnostic
);

#endif
