#include "a_program/frontend/lowering.h"
#include "a_program/frontend/context_projection.h"
#include "a_program/frontend/typing_classifier_state.h"
#include "a_program/frontend/typing_publication.h"
#include "a_program/frontend/typing_evidence_state.h"
#include "a_program/frontend/typing_usage_state.h"
#include "a_program/core/request.h"

#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/graph/occurrence_usage.h"
#include "a_program/kernel/resource_usage.h"
#include "a_program/kernel/type_term_debug.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../internal/ast_common.h"
#include "../../artifact/artifact_internal.h"

static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
);

static uint64_t compile_performance_clock_ns(void) {
	clock_t now = clock();
	uint64_t seconds;
	uint64_t remainder;
	if (now == (clock_t)-1) {
		return 0;
	}
	seconds = (uint64_t)now / (uint64_t)CLOCKS_PER_SEC;
	remainder = (uint64_t)now % (uint64_t)CLOCKS_PER_SEC;
	return seconds * UINT64_C(1000000000) +
		remainder * UINT64_C(1000000000) / (uint64_t)CLOCKS_PER_SEC;
}

static uint64_t compile_performance_elapsed_ns(uint64_t start) {
	uint64_t finish = compile_performance_clock_ns();
	return finish >= start ? finish - start : 0;
}

static int artifact_term_present(const struct prototype_term* term) {
	return term && term->tag != 0;
}

static int qualified_names_equal(
	struct prototype_qualified_name left,
	struct prototype_qualified_name right
) {
	return left.namespace_symbol_id == right.namespace_symbol_id &&
		left.name_symbol_id == right.name_symbol_id;
}

static struct prototype_qualified_name qualified_name_make(
	int namespace_symbol_id,
	int name_symbol_id
) {
	struct prototype_qualified_name name;
	name.namespace_symbol_id = namespace_symbol_id;
	name.name_symbol_id = name_symbol_id;
	return name;
}

static uint32_t operation_runtime_unwrap_name(
	const struct prototype_compile_metadata* metadata,
	uint32_t operation_id
) {
	for (size_t visited = 0;
		metadata && operation_id < metadata->typed_occurrences.occurrence_count &&
		visited < metadata->typed_occurrences.occurrence_count;
		++visited) {
		const struct prototype_typed_occurrence* operation =
			&metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			operation_id = operation->wrapped_occurrence;
			continue;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			operation_id = operation->wrapped_occurrence;
			continue;
		}
		break;
	}
	return operation_id;
}

static size_t lowering_def_index_hash(int symbol_id) {
	uint32_t x = (uint32_t)symbol_id;
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return (size_t)x;
}

static const struct prototype_ast_def_open_address_entry*
lookup_def_index_entry_const(
	const struct prototype_ast_db* db,
	int name_symbol_id
) {
	if (!db || !db->def_index || db->def_index_capacity == 0) {
		return NULL;
	}
	size_t start = lowering_def_index_hash(name_symbol_id) %
		db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		const struct prototype_ast_def_open_address_entry* entry =
			&db->def_index[index];
		if (!entry->occupied) {
			return NULL;
		}
		if (entry->symbol_id == name_symbol_id) {
			return entry;
		}
	}
	return NULL;
}

static struct prototype_ast_term_assignment_def* lookup_unique_assignment_raw(
	struct prototype_ast_db* db,
	int name_symbol_id
) {
	const struct prototype_ast_def_open_address_entry* entry =
		lookup_def_index_entry_const(db, name_symbol_id);
	if (!entry || entry->assignment_count != 1 ||
		entry->first_assignment >= db->assignment_count) {
		return NULL;
	}
	return &db->assignments[entry->first_assignment];
}

struct binder_map_entry {
	uint32_t ast_binder_id;
	uint32_t graph_binder_id;
	uint32_t classifier;
	int symbol_id;
};

struct level_map_entry {
	uint32_t ast_level_var;
	uint32_t graph_type_expr;
};

struct type_expr_map_entry {
	uint32_t ast_type_expr;
	uint32_t graph_type_expr;
};

struct ih_scope_map_entry {
	uint32_t ast_binder_id;
	uint32_t ih_scope_id;
	uint32_t case_index;
	uint32_t field_index;
};

/* Layer T lowering reference. The occurrence identifies the source/static
 * operation; term is its one-way projection into Layer C. Core-only helpers
 * accept Term IDs, while every typed constructor accepts and returns this pair.
 * Neither identity may be recovered from the other after this record exists. */
struct compile_ref {
	uint32_t term;
	uint32_t classifier;
	uint32_t operation;
	/* Uses the shared semantic category and computation-kind vocabularies. */
	int category;
	int computation_kind;
};

struct local_ref_map_entry {
	uint32_t ast_binder_id;
	int symbol_id;
	struct compile_ref ref;
};

/* These binders exist only in classifiers. They quantify the latent effect
 * rows of function values mentioned by a lambda binder annotation. */
#define PROTOTYPE_JUDGEMENT_DELTA_CAPACITY 4096

struct operation_motive_candidate {
	uint32_t motive;
	uint32_t source_case_index;
	uint64_t dependency_mask;
	uint64_t unavailable_mask;
	uint64_t conflict_mask;
};

enum operation_motive_correspondence_status {
	OPERATION_MOTIVE_CORRESPONDENCE_INVALID = -1,
	OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE = 0,
	OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE = 1,
	OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT = 2
};

struct operation_motive_index_correspondence {
	uint32_t family_type_id;
	uint32_t index_ordinal;
	uint32_t source_index;
	uint32_t branch_index;
	uint32_t source_index_classifier;
	uint32_t branch_index_classifier;
	uint32_t branch_context;
	uint32_t refinement_substitution;
};

enum operation_motive_equation_validation_status {
	OPERATION_MOTIVE_EQUATION_INVALID = -1,
	OPERATION_MOTIVE_EQUATION_VALID = 0,
	OPERATION_MOTIVE_EQUATION_PENDING = 1,
	OPERATION_MOTIVE_EQUATION_MISMATCH = 2
};

struct operation_motive_equation_validation {
	uint32_t expected_classifier;
	uint32_t actual_classifier;
	uint32_t ih_operation;
	int conversion_status;
	int guardedness_status;
	int used_recursive_equation;
};

/*
 * This is the solver-side form of
 *
 *     classifier(branch body) == M(Constructor(case binders)).
 *
 * The case telescope is referenced by the corresponding TypedOccurrenceGraph case
 * Context ID. It is not reconstructed from a shared core VAR node, whose
 * identity is deliberately weaker than a source occurrence identity.
 */

struct compile_context;

static const struct prototype_typing_constraint*
operation_solver_motive_equation_for_projection(
	const struct compile_context* ctx,
	uint32_t match_operation,
	uint32_t case_index
);

static int operation_motive_abstract_index(
	struct compile_context* ctx,
	uint32_t body,
	uint32_t index_term,
	uint32_t replacement,
	uint32_t* p_result,
	int* p_dependency_found
);

static int operation_motive_quote_refined_term(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t substitution_id,
	uint32_t* p_quoted
);

static int operation_motive_branch_index_correspondence(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t case_index,
	uint32_t constructor_classifier,
	uint32_t index_ordinal,
	struct operation_motive_index_correspondence* p_correspondence
);

static int operation_solver_validate_guarded_motive_occurrence(
	struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence
);

static int operation_motive_validate_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t case_index,
	uint32_t motive,
	struct operation_motive_equation_validation* p_validation
);

static int operation_motive_add_terminal_diagnostic(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t case_index
);

static int operation_motive_collect_owned_induction_hypotheses(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t* operations,
	uint32_t capacity,
	uint32_t* p_count
);

static int operation_motive_clear_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection
);

static int operation_motive_has_recursive_equations(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection
);

static int operation_motive_register_recursive_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t ih_typed_projection
);

static const struct prototype_typing_constraint*
operation_motive_ih_equation_for_projection_const(
	const struct compile_context* ctx,
	uint32_t typed_projection
);

static int operation_motive_offer_expected_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t* p_motive_application,
	int* p_changed
);

static int operation_motive_reopen_after_branch_refinement(
	struct compile_context* ctx,
	uint32_t match_typed_projection
);

static int operation_solver_seed_motive(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t match_typed_projection,
	uint32_t classifier,
	uint32_t source_body_occurrence,
	int* p_changed
);

static int operation_solver_var_binding_context(
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	uint32_t variable_occurrence,
	uint32_t* p_binding_context
);

static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
);

struct compile_context;

static int operation_refine_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
);

static int operation_refine_projected_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
);

static int operation_check_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth
);

struct compile_judgement_workspace {
	struct prototype_judgement_proposition
		propositions[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_derivation_candidate
		derivation_candidates[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_candidate_premise candidate_premises[
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY *
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result
		match_motive_results[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint
		computation_constraints[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint_input
		computation_constraint_inputs[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint_input
		proof_computation_constraint_inputs[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint_result
		computation_constraint_results[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint_result
		proof_computation_constraint_results[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_effect_row_constraint
		effect_row_constraints[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
};

struct compile_context {
	const struct prototype_intrinsic_typing_environment* intrinsic_environment;
	struct symbol_table* symbols;
	struct prototype_ast_db* asts;
	const struct prototype_source_schedule* source_schedule;
	const struct prototype_source_lowering_plan* source_plan;
	struct prototype_source_core_handoff_builder* core_source_results;
	const struct prototype_source_core_handoff_view* core_source_view;
	struct prototype_typing_source_results* typing_source_results;
	struct prototype_term_db* terms;
	/* Transitional Layer T client capability. The provider object is owned by
	 * the driver and is opaque through request.h. */
	const struct prototype_core_request_service* core_requests;
	/* Layer T persistent state is owned independently from this transitional
	 * invocation context. */
	struct prototype_typing_pipeline* typing;
	/* Rebuildable invocation state. It owns no accepted or current answer. */
	struct prototype_typing_classifier_state classifier_workspace;
	struct prototype_typing_usage_workspace usage_workspace;
	uint32_t active_classifier_constraint_id;
	/* Proof construction cursors and caches are disposable invocation state. */
	struct prototype_typing_evidence_workspace evidence_workspace;
	/* Sealed Layer T facts from an earlier CompileUnit. They are borrowed as
	 * immutable premises and never copied into current mutable solver cells. */
	struct prototype_accepted_typing_input_view accepted_typing;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_judgement_delta judgement_delta;
	struct prototype_judgement_computation_constraint_input*
		computation_constraint_inputs;
	size_t computation_constraint_input_capacity;
	struct prototype_judgement_computation_constraint_input*
		proof_computation_constraint_inputs;
	size_t proof_computation_constraint_input_capacity;
	struct prototype_judgement_computation_constraint_result*
		computation_constraint_results;
	size_t computation_constraint_result_capacity;
	struct prototype_judgement_computation_constraint_result*
		proof_computation_constraint_results;
	size_t proof_computation_constraint_result_capacity;
	struct prototype_compile_metadata* metadata;
	/* Scoped input for accepted-proof reification. Source evidence construction
	 * leaves this NULL and therefore continues to read equation answers. */
	const struct prototype_typed_publication_view* proof_projection_view;
	struct binder_map_entry binders[512];
	uint32_t binder_count;
	/* context_ids[n] is the persistent context represented by the first n
	 * entries of binders. Restoring binder_count therefore restores context
	 * without a second mutable scope stack. */
	uint32_t context_ids[513];
	struct local_ref_map_entry local_refs[512];
	uint32_t local_ref_count;
	struct ih_scope_map_entry ih_scopes[512];
	uint32_t ih_scope_count;
	struct level_map_entry levels[512];
	uint32_t level_count;
	struct type_expr_map_entry type_exprs[1024];
	uint32_t type_expr_count;
	uint32_t resolution_iteration;
	int namespace_symbol_id;
	const struct prototype_artifact_interface* const* imported_interfaces;
	size_t imported_interface_count;
	int had_error;
	int effort_atomic;
};

static struct prototype_source_span compile_source_span(
	const struct compile_context* ctx,
	uint32_t source_ast
) {
	if (!ctx || !ctx->asts || source_ast >= ctx->asts->node_count) {
		return (struct prototype_source_span) { 0, 0 };
	}
	return ctx->asts->nodes[source_ast].span;
}

static uint32_t compile_assignment_slot(
	const struct prototype_ast_db* asts,
	const struct prototype_source_lowering_plan* source_plan,
	const struct prototype_ast_term_assignment_def* assignment
) {
	return asts && source_plan && assignment &&
		assignment >= asts->assignments && assignment <
			asts->assignments + asts->assignment_count ?
		prototype_source_lowering_plan_term_slot(
			source_plan, assignment->ast
		) : PROTOTYPE_INVALID_ID;
}

static int compile_assignment_results(
	const struct prototype_ast_db* asts,
	const struct prototype_source_lowering_plan* source_plan,
	struct prototype_source_core_handoff_builder* core_results,
	struct prototype_typing_source_results* typing_results,
	const struct prototype_ast_term_assignment_def* assignment,
	const struct prototype_source_core_slot_result** p_core,
	struct prototype_typing_source_assignment_result** p_typing
) {
	if (!asts || !source_plan || !core_results || !typing_results ||
		!assignment || !p_core || !p_typing || assignment < asts->assignments ||
		assignment >= asts->assignments + asts->assignment_count) {
		return -1;
	}
	uint32_t assignment_id = (uint32_t)(assignment - asts->assignments);
	uint32_t slot_id = compile_assignment_slot(asts, source_plan, assignment);
	*p_core = prototype_source_core_slot_result_get(
		core_results, slot_id
	);
	*p_typing = prototype_typing_source_assignment_result_mut(
		typing_results, assignment_id
	);
	return *p_core && *p_typing ? 0 : -1;
}

static struct prototype_typing_source_expectation_result*
compile_expectation_result(
	const struct prototype_ast_db* asts,
	struct prototype_typing_source_results* typing_results,
	const struct prototype_ast_type_expectation_def* expectation
) {
	if (!asts || !typing_results || !expectation ||
		expectation < asts->expectations || expectation >=
			asts->expectations + asts->expectation_count) {
		return NULL;
	}
	return prototype_typing_source_expectation_result_mut(
		typing_results,
		(uint32_t)(expectation - asts->expectations)
	);
}

static struct prototype_typing_source_type_result* compile_type_result(
	const struct prototype_ast_db* asts,
	struct prototype_typing_source_results* typing_results,
	const struct prototype_ast_type_def* type
) {
	if (!asts || !typing_results || !type ||
		type < asts->type_defs || type >=
			asts->type_defs + asts->type_def_count) {
		return NULL;
	}
	return prototype_typing_source_type_result_mut(
		typing_results,
		(uint32_t)(type - asts->type_defs)
	);
}

static int typing_core_normalize_complete(
	const struct prototype_core_request_service* core,
	int profile,
	uint32_t term,
	uint32_t* p_normalized
) {
	struct prototype_core_normalization_request request = {
		.term_id = term,
		.profile = profile,
		.step_limit = UINT64_MAX
	};
	struct prototype_core_normalization_response response = { 0 };
	if (!p_normalized || prototype_core_request_normalize(
			core, &request, &response
		) != 0 || response.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
		return -1;
	}
	*p_normalized = response.term_id;
	return 0;
}

static int typing_core_inspect_counts(
	const struct prototype_core_request_service* core,
	struct prototype_core_inspect_counts* counts,
	uint64_t* graph_revision
) {
	const struct prototype_core_inspect_request request = {
		.kind = PROTOTYPE_CORE_INSPECT_COUNTS
	};
	struct prototype_core_inspect_response response;
	if (!counts || prototype_core_request_inspect(
			core, &request, &response
		) != 0 || response.kind != PROTOTYPE_CORE_INSPECT_COUNTS) {
		return -1;
	}
	*counts = response.as.counts;
	if (graph_revision) *graph_revision = response.graph_revision;
	return 0;
}

static int typing_core_inspect_term(
	const struct prototype_core_request_service* core,
	uint32_t term_id,
	struct prototype_term* term
) {
	const struct prototype_core_inspect_request request = {
		.kind = PROTOTYPE_CORE_INSPECT_TERM,
		.as.term.term_id = term_id
	};
	struct prototype_core_inspect_response response;
	if (!term || prototype_core_request_inspect(
			core, &request, &response
		) != 0 || response.kind != PROTOTYPE_CORE_INSPECT_TERM) {
		return -1;
	}
	*term = response.as.term;
	return 0;
}

static int typing_core_inspect_match_case(
	const struct prototype_core_request_service* core,
	uint32_t case_id,
	struct prototype_core_inspect_match_case* match_case
) {
	const struct prototype_core_inspect_request request = {
		.kind = PROTOTYPE_CORE_INSPECT_MATCH_CASE,
		.as.match_case.case_id = case_id
	};
	struct prototype_core_inspect_response response;
	if (!match_case || prototype_core_request_inspect(
			core, &request, &response
		) != 0 || response.kind != PROTOTYPE_CORE_INSPECT_MATCH_CASE) {
		return -1;
	}
	*match_case = response.as.match_case;
	return 0;
}

static int compile_effort_charge(
	struct prototype_compile_metadata* metadata,
	int effort_atomic,
	int phase,
	uint64_t credits
) {
	if (effort_atomic) return 0;
	return metadata ? prototype_effort_consume(
		&metadata->effort, phase, credits
	) : -1;
}

static uint64_t compile_effort_remaining(
	const struct prototype_compile_metadata* metadata
) {
	return metadata ? prototype_effort_remaining(&metadata->effort) : 0;
}

static uint32_t operation_solver_binder_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);
static int operation_binder_classifier_meta_solve(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
);
static struct prototype_context_classifier_view operation_type_context_view(
	struct compile_context* ctx
);
static uint32_t operation_context_classifier_term(
	struct compile_context* ctx,
	uint32_t context_id
);
static int operation_type_metas_discard_mutable_occurrence(
	struct compile_context* ctx,
	uint32_t operation
);

static uint32_t operation_context_materialized_id(
	const struct compile_context* ctx,
	uint32_t candidate_context
) {
	if (!ctx || !ctx->metadata || candidate_context >=
			ctx->metadata->contexts.context_count) {
		return PROTOTYPE_INVALID_ID;
	}
	return candidate_context;
}

static int operation_context_identity_projection_action(
	struct compile_context* ctx,
	uint32_t candidate_context,
	uint32_t concrete_context,
	uint32_t* p_substitution
) {
	if (!ctx || !ctx->metadata || !p_substitution ||
		!prototype_context_get(&ctx->metadata->contexts, candidate_context) ||
		!prototype_context_get(&ctx->metadata->contexts, concrete_context)) {
		return -1;
	}
	if (candidate_context == concrete_context) {
		return prototype_substitution_identity(
			&ctx->metadata->substitutions,
			&ctx->metadata->contexts,
			candidate_context,
			p_substitution
		);
	}
	uint32_t path[512];
	uint32_t arguments[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			prototype_context_empty(&ctx->metadata->contexts),
			candidate_context,
			path,
			512,
			&path_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < path_count; ++i) {
		const struct prototype_context* entry = prototype_context_get(
			&ctx->metadata->contexts, path[i]
		);
		uint32_t binding_context;
		if (!entry || entry->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_context_find_binding(
				&ctx->metadata->contexts,
				concrete_context,
				entry->binding_id,
				&binding_context
			) != 0 || prototype_term_var(
				ctx->terms, entry->binding_id, &arguments[i]
			) != 0) {
			return -1;
		}
		(void)binding_context;
	}
	struct prototype_context_classifier_view context_view =
		operation_type_context_view(ctx);
	return prototype_context_substitution_from_terms_in_view(
		&context_view,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		concrete_context,
		candidate_context,
		arguments,
		path_count,
		p_substitution
	);
}

static uint64_t operation_context_projection_memo_mix(
	uint64_t hash,
	uint64_t value
) {
	return hash ^ (value + UINT64_C(0x9e3779b97f4a7c15) +
		(hash << 6) + (hash >> 2));
}

static uint64_t operation_context_projection_memo_begin(
	const struct prototype_context_action_expression* action
) {
	return action ? operation_context_projection_memo_mix(
		UINT64_C(0xcbf29ce484222325), action->key_hash
	) : 0;
}

static int operation_context_projection_memo_matches(
	const struct prototype_context_projection_solution* solution,
	uint64_t input_fingerprint
) {
	return solution &&
		solution->state == PROTOTYPE_CONTEXT_PROJECTION_SOLVED &&
		solution->input_fingerprint == input_fingerprint;
}

static int operation_context_projection_solve(
	struct compile_context* ctx,
	uint32_t projection_id,
	uint32_t depth
) {
	if (!ctx || !ctx->typing || !ctx->metadata || depth > 512) {
		return -1;
	}
	struct prototype_context_projection_db* db =
		&ctx->typing->context_projections;
	const struct prototype_context_projection* projection =
		prototype_context_projection_get(db, projection_id);
	const struct prototype_context_projection_solution* existing =
		prototype_context_projection_solution_get(db, projection_id);
	if (!projection || !existing) {
		return -1;
	}
	if (existing->state == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
		return PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION;
	}
	const struct prototype_context_action_expression* action =
		prototype_context_action_expression_get(
			db, projection->action_expression
		);
	if (!action || action->target_candidate_context !=
			projection->candidate_context) {
		return -1;
	}
	if (getenv("A_PROGRAM_CONTEXT_PROJECTION_TRACE") != NULL) {
		fprintf(stderr,
			"context-projection-solve depth=%u projection=%u candidate=%u action=%u "
			"kind=%d parent=%u dependency=%u state=%d concrete=%u substitution=%u\n",
			depth, projection_id, projection->candidate_context,
			projection->action_expression, action->kind, action->parent_action,
			action->dependency_id, existing->state, existing->concrete_context,
			existing->substitution);
	}
	uint32_t concrete_context = PROTOTYPE_INVALID_ID;
	uint32_t substitution = PROTOTYPE_INVALID_ID;
	uint64_t input_fingerprint =
		operation_context_projection_memo_begin(action);
	switch (action->kind) {
	case PROTOTYPE_CONTEXT_ACTION_IDENTITY:
		if (operation_context_projection_memo_matches(
				existing, input_fingerprint
			)) {
			return 0;
		}
		concrete_context = operation_context_materialized_id(
			ctx, projection->candidate_context
		);
		if (concrete_context == PROTOTYPE_INVALID_ID ||
			operation_context_identity_projection_action(
				ctx,
				concrete_context,
				concrete_context,
				&substitution
			) != 0) {
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(stderr,
					"context identity pending projection=%u candidate=%u concrete=%u\n",
					projection_id, projection->candidate_context, concrete_context);
			}
			return 1;
		}
		break;
	case PROTOTYPE_CONTEXT_ACTION_PROJECTION: {
		const struct prototype_context_action_expression* parent_action =
			prototype_context_action_expression_get(
				db, action->parent_action
			);
		uint32_t parent_projection;
		if (!parent_action || prototype_context_projection_intern(
				db,
				parent_action->target_candidate_context,
				action->parent_action,
				&parent_projection
			) != 0) {
			return -1;
		}
		int parent_status = operation_context_projection_solve(
			ctx, parent_projection, depth + 1
		);
		const struct prototype_context_projection_solution* parent =
			prototype_context_projection_solution_get(db, parent_projection);
		if (parent_status != 0) {
			return parent_status;
		}
		if (!parent || parent->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
			return -1;
		}
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->concrete_context
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->substitution
		);
		if (operation_context_projection_memo_matches(
				existing, input_fingerprint
			)) {
			return 0;
		}
		const struct prototype_substitution* parent_substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, parent->substitution
			);
		uint32_t source_base = operation_context_materialized_id(
			ctx, parent_action->target_candidate_context
		);
		uint32_t source_extension = operation_context_materialized_id(
			ctx, projection->candidate_context
		);
		if (!parent_substitution || source_base == PROTOTYPE_INVALID_ID ||
			source_extension == PROTOTYPE_INVALID_ID ||
			parent_substitution->target_context != source_base) {
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"context projection input pending projection=%u parent=%u "
					"base=%u extension=%u parent-substitution=%u target=%u\n",
					projection_id,
					parent_projection,
					source_base,
					source_extension,
					parent ? parent->substitution : PROTOTYPE_INVALID_ID,
					parent_substitution ? parent_substitution->target_context :
						PROTOTYPE_INVALID_ID
				);
			}
			return 1;
		}
		if (parent_substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
			parent_substitution->source_context == source_base) {
			concrete_context = source_extension;
			if (prototype_substitution_identity(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					source_extension,
					&substitution
				) != 0) {
				return -1;
			}
		} else {
			struct prototype_context_classifier_view context_view =
				operation_type_context_view(ctx);
			if (prototype_context_pullback_occurrence_telescope(
					&context_view,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					ctx->terms,
					ctx->type_declarations,
					source_base,
					source_extension,
					parent->substitution,
					&concrete_context,
					&substitution
				) != 0) {
				if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
					fprintf(
						stderr,
						"context projection pullback pending projection=%u parent=%u "
						"base=%u extension=%u substitution=%u\n",
						projection_id,
						parent_projection,
						source_base,
						source_extension,
						parent->substitution
					);
				}
				return 1;
			}
		}
		break;
	}
	case PROTOTYPE_CONTEXT_ACTION_RESTRICTION: {
		const struct prototype_context_action_expression* parent_action =
			prototype_context_action_expression_get(
				db, action->parent_action
			);
		uint32_t parent_projection;
		if (!parent_action) {
			return -1;
		}
		int ancestor_status = prototype_context_is_ancestor(
			&ctx->metadata->contexts,
			action->target_candidate_context,
			parent_action->target_candidate_context
		);
		if (ancestor_status != 1) {
			return -1;
		}
		if (prototype_context_projection_intern(
				db,
				parent_action->target_candidate_context,
				action->parent_action,
				&parent_projection
			) != 0) {
			return -1;
		}
		int parent_status = operation_context_projection_solve(
			ctx, parent_projection, depth + 1
		);
		const struct prototype_context_projection_solution* parent =
			prototype_context_projection_solution_get(db, parent_projection);
		if (parent_status != 0) {
			return parent_status;
		}
		if (!parent || parent->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
			return -1;
		}
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->concrete_context
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->substitution
		);
		if (operation_context_projection_memo_matches(
				existing, input_fingerprint
			)) {
			return 0;
		}
		uint32_t lexical_projection;
		if (prototype_substitution_projection_path(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				parent_action->target_candidate_context,
				action->target_candidate_context,
				&lexical_projection
			) != 0) {
			return -1;
		}
		if (prototype_substitution_compose(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				lexical_projection,
				parent->substitution,
				&substitution
			) != 0) {
			return -1;
		}
		concrete_context = parent->concrete_context;
		break;
	}
	case PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT: {
		const struct prototype_context_action_expression* parent_action =
			prototype_context_action_expression_get(
				db, action->parent_action
			);
		const struct prototype_typed_match_case_projection* case_projection =
			prototype_typed_match_case_projection_get(db, action->dependency_id);
		uint32_t parent_projection;
		if (!parent_action || !case_projection ||
			action->dependency_id >=
				PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ||
			prototype_context_projection_intern(
				db,
				parent_action->target_candidate_context,
				action->parent_action,
				&parent_projection
			) != 0) {
			return -1;
		}
		int parent_status = operation_context_projection_solve(
			ctx, parent_projection, depth + 1
		);
		const struct prototype_context_projection_solution* parent =
			prototype_context_projection_solution_get(db, parent_projection);
		uint32_t constraint_id = ctx->typing->constraints.
			branch_refinement_constraint_for_match_case_projection[
				action->dependency_id
			];
		if (parent_status != 0) {
			return parent_status;
		}
		if (!parent || parent->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
			return -1;
		}
		if (constraint_id >= ctx->typing->constraints.constraint_count) {
			if (getenv("A_PROGRAM_CONTEXT_PROJECTION_TRACE") != NULL) {
				fprintf(
					stderr,
					"branch projection constraint missing projection=%u "
					"case-projection=%u constraint=%u constraint-count=%u\n",
					projection_id,
					action->dependency_id,
					constraint_id,
					ctx->typing->constraints.constraint_count
				);
			}
			return 1;
		}
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[constraint_id];
		const struct prototype_typing_constraint_solution* branch =
			&ctx->typing->constraints.solutions[constraint_id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->payload.branch_refinement.match_case_projection !=
				action->dependency_id) {
			return -1;
		}
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->concrete_context
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, parent->substitution
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint,
			(uint32_t)branch->branch_refinement.refinement_status
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint,
			branch->branch_refinement.materialized_source_context
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, branch->branch_refinement.refined_context
		);
		input_fingerprint = operation_context_projection_memo_mix(
			input_fingerprint, branch->branch_refinement.substitution
		);
		if (operation_context_projection_memo_matches(
				existing, input_fingerprint
			)) {
			return 0;
		}
		if (branch->branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
			uint64_t before_revision = db->solution_revision;
			if (prototype_context_projection_reject(db, projection_id) != 0) {
				return -1;
			}
			if (db->solution_revision != before_revision &&
				operation_solver_enqueue_context_projection_dependents(
					ctx, projection_id
				) != 0) {
				return -1;
			}
			return PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION;
		}
		if (branch->branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
			concrete_context = parent->concrete_context;
			substitution = parent->substitution;
			break;
		}
		const struct prototype_substitution* refinement =
			prototype_substitution_get(
				&ctx->metadata->substitutions,
				branch->branch_refinement.substitution
			);
		if (branch->branch_refinement.refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_SOLVED ||
			!refinement || refinement->target_context !=
				parent->concrete_context ||
			branch->branch_refinement.materialized_source_context !=
				parent->concrete_context ||
			prototype_substitution_compose(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				parent->substitution,
				branch->branch_refinement.substitution,
				&substitution
			) != 0) {
			if (getenv("A_PROGRAM_CONTEXT_PROJECTION_TRACE") != NULL) {
				fprintf(
					stderr,
					"branch projection pending projection=%u constraint=%u "
					"state=%d refinement-status=%d parent-context=%u "
					"materialized-source=%u action=%u action-source=%u "
					"action-target=%u refined-context=%u\n",
					projection_id,
					constraint_id,
					branch->state,
					branch->branch_refinement.refinement_status,
					parent->concrete_context,
					branch->branch_refinement.materialized_source_context,
					branch->branch_refinement.substitution,
					refinement ? refinement->source_context : PROTOTYPE_INVALID_ID,
					refinement ? refinement->target_context : PROTOTYPE_INVALID_ID,
					branch->branch_refinement.refined_context
				);
			}
			return 1;
		}
		concrete_context = refinement->source_context;
		break;
	}
	default:
		return -1;
	}
	uint64_t before_revision = db->solution_revision;
	if (prototype_context_projection_publish(
			db, projection_id, concrete_context, substitution, input_fingerprint
		) != 0) {
		return -1;
	}
	if (db->solution_revision != before_revision &&
		operation_solver_enqueue_context_projection_dependents(
			ctx, projection_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int operation_typed_projection_materialized_context(
	struct compile_context* ctx,
	uint32_t typed_projection_id,
	uint32_t* p_context,
	uint32_t* p_substitution
) {
	if (!ctx || !ctx->typing || !p_context || !p_substitution) {
		return -1;
	}
	const struct prototype_typed_projection* typed_projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection_id
		);
	if (!typed_projection) {
		return -1;
	}
	int status = operation_context_projection_solve(
		ctx, typed_projection->context_projection, 0
	);
	const struct prototype_context_projection_solution* solution =
		prototype_context_projection_solution_get(
			&ctx->typing->context_projections,
			typed_projection->context_projection
		);
	if (status != 0 || !solution || solution->state !=
			PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			const struct prototype_context_projection* context_projection =
				prototype_context_projection_get(
					&ctx->typing->context_projections,
					typed_projection->context_projection
				);
			const struct prototype_context_action_expression* action =
				context_projection ? prototype_context_action_expression_get(
					&ctx->typing->context_projections,
					context_projection->action_expression
				) : NULL;
			fprintf(
				stderr,
				"projection-context waiting typed=%u occurrence=%u projection=%u "
				"status=%d state=%d candidate=%u action-kind=%d target=%u parent=%u "
				"dependency=%u\n",
				typed_projection_id,
				typed_projection->source_occurrence,
				typed_projection->context_projection,
				status,
				solution ? solution->state : -1,
				context_projection ? context_projection->candidate_context :
					PROTOTYPE_INVALID_ID,
				action ? action->kind : -1,
				action ? action->target_candidate_context : PROTOTYPE_INVALID_ID,
				action ? action->parent_action : PROTOTYPE_INVALID_ID,
				action ? action->dependency_id : PROTOTYPE_INVALID_ID
			);
		}
		return status;
	}
	*p_context = solution->concrete_context;
	*p_substitution = solution->substitution;
	return 0;
}

static int operation_typed_projection_materialized_context_answer(
	const struct compile_context* ctx,
	uint32_t typed_projection_id,
	uint32_t* p_context,
	uint32_t* p_substitution
) {
	const struct prototype_typed_projection* typed_projection =
		ctx && ctx->typing ? prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection_id
		) : NULL;
	const struct prototype_context_projection_solution* solution =
		typed_projection ? prototype_context_projection_solution_get(
			&ctx->typing->context_projections,
			typed_projection->context_projection
		) : NULL;
	if (!ctx || !p_context || !p_substitution || !typed_projection || !solution) {
		return -1;
	}
	if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
		return PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION;
	}
	if (solution->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
		return 1;
	}
	*p_context = solution->concrete_context;
	*p_substitution = solution->substitution;
	return 0;
}

static uint32_t operation_context_classifier_term(
	struct compile_context* ctx,
	uint32_t candidate_context
) {
	if (!ctx || !ctx->metadata) {
		return PROTOTYPE_INVALID_ID;
	}
	const struct prototype_context* context = prototype_context_get(
		&ctx->metadata->contexts, candidate_context
	);
	if (!context) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t equation = prototype_context_classifier_equation(context);
	const struct prototype_context_classifier_equation* classifier_equation =
		prototype_context_classifier_equation_get(
			&ctx->metadata->contexts, equation
		);
	if (!classifier_equation || classifier_equation->answer == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	return classifier_equation->answer;
}

static struct prototype_context_classifier_view operation_type_context_view(
	struct compile_context* ctx
) {
	return (struct prototype_context_classifier_view) {
		.contexts = ctx ? &ctx->metadata->contexts : NULL
	};
}

static int operation_occurrence_materialized_context_status(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t* p_context,
	uint32_t* p_substitution
) {
	if (!ctx || !ctx->metadata || !p_context || !p_substitution || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (occurrence_id < ctx->accepted_typing.occurrence_count) {
		uint32_t published_context;
		const struct prototype_typed_publication_projection* projection = NULL;
		int lookup_status = prototype_accepted_typing_input_lookup(
			&ctx->accepted_typing, occurrence_id, &projection, &published_context
		);
		if (lookup_status == 1) {
			*p_context = PROTOTYPE_INVALID_ID;
			*p_substitution = PROTOTYPE_INVALID_ID;
			return PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION;
		}
		if (lookup_status != 0 || !projection) {
			return -1;
		}
		*p_context = published_context;
		*p_substitution = projection->substitution;
		if (getenv("A_PROGRAM_PROOF_TRACE")) {
			fprintf(stderr,
				"A_PROGRAM_PROOF_TRACE accepted-projection occurrence=%u typed=%u "
				"context=%u published=%u action=%u\n",
				occurrence_id, projection->typed_projection, *p_context,
				published_context, *p_substitution);
		}
		const struct prototype_substitution* accepted_substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, *p_substitution
			);
		return accepted_substitution &&
			accepted_substitution->source_context == published_context ? 0 : -1;
	}
	uint32_t typed_projection = operation_solver_publication_projection(
		ctx, occurrence_id
	);
	if (typed_projection == PROTOTYPE_INVALID_ID) return 1;
	int status = operation_typed_projection_materialized_context(
		ctx, typed_projection, p_context, p_substitution
	);
	if (status == 0 && getenv("A_PROGRAM_PROOF_TRACE")) {
		const struct prototype_substitution* trace_substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, *p_substitution
			);
		fprintf(stderr,
			"A_PROGRAM_PROOF_TRACE live-projection occurrence=%u typed=%u "
			"context=%u action=%u kind=%d source=%u target=%u\n",
			occurrence_id, typed_projection, *p_context, *p_substitution,
			trace_substitution ? trace_substitution->kind : -1,
			trace_substitution ? trace_substitution->source_context :
				PROTOTYPE_INVALID_ID,
			trace_substitution ? trace_substitution->target_context :
				PROTOTYPE_INVALID_ID);
	}
	return status;
}

static uint32_t operation_occurrence_materialized_context(
	struct compile_context* ctx,
	uint32_t occurrence_id
) {
	uint32_t context;
	uint32_t substitution;
	if (operation_occurrence_materialized_context_status(
			ctx, occurrence_id, &context, &substitution
		) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	(void)substitution;
	return context;
}

static int operation_typed_projection_materialized_typing_after_context(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t* p_context,
	uint32_t* p_substitution,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	int trace = getenv("A_PROGRAM_COMPUTATION_INPUT_TRACE") != NULL;
	if (!ctx || !ctx->typing || !ctx->metadata || !p_context || !p_substitution ||
		!p_subject || !p_classifier) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		if (trace) {
			fprintf(stderr,
				"materialized typing failed projection=%u stage=projection source=%u "
				"count=%zu\n",
				typed_projection,
				projection ? projection->source_occurrence : PROTOTYPE_INVALID_ID,
				ctx->metadata->typed_occurrences.occurrence_count);
		}
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			occurrence->core_term,
			*p_substitution,
			p_subject
		) != 0) {
		if (trace) {
			fprintf(stderr,
				"materialized typing failed projection=%u source=%u stage=reindex "
				"core=%u substitution=%u context=%u\n",
				typed_projection, projection->source_occurrence,
				occurrence->core_term, *p_substitution, *p_context);
		}
		return -1;
	}
	/* Classifier solutions are produced in the materialized occurrence Context.
	 * Unlike the immutable source Core subject, applying the occurrence action
	 * again would duplicate the CwF substitution. */
	*p_classifier = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
	if (projection->source_occurrence < ctx->accepted_typing.occurrence_count) {
		uint32_t published_context;
		const struct prototype_typed_publication_projection* publication =
			prototype_accepted_typing_input_projection(
				&ctx->accepted_typing, projection->source_occurrence,
				&published_context
			);
		if (!publication) {
			/* Accepted publication is a sparse root view, not a second typing
			 * authority. Internal accepted occurrences remain usable through their
			 * materialized projection even when they are not exported roots. */
			return 0;
		}
		/* Only the accepted publication projection is required to reproduce the
		 * artifact tuple exactly. A derived projection of the same source occurrence
		 * is authoritative in its own materialized Context and normally has a
		 * reindexed subject. */
		if (publication->typed_projection == typed_projection &&
			(published_context != *p_context ||
			 publication->subject != *p_subject ||
			 publication->classifier != *p_classifier)) {
			if (trace) {
				fprintf(stderr,
					"materialized typing failed projection=%u source=%u "
					"stage=publication-mismatch context=%u/%u subject=%u/%u "
					"classifier=%u/%u\n",
					typed_projection, projection->source_occurrence,
					published_context, *p_context, publication->subject, *p_subject,
					publication->classifier, *p_classifier);
			}
			return -1;
		}
	}
	return 0;
}

static int operation_typed_projection_materialized_typing_status(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t* p_context,
	uint32_t* p_substitution,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	int context_status = operation_typed_projection_materialized_context(
		ctx, typed_projection, p_context, p_substitution
	);
	if (context_status != 0) {
		return context_status;
	}
	return operation_typed_projection_materialized_typing_after_context(
		ctx, typed_projection, p_context, p_substitution, p_subject, p_classifier
	);
}

static int operation_typed_projection_materialized_typing_answer(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t* p_context,
	uint32_t* p_substitution,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	int context_status = operation_typed_projection_materialized_context_answer(
		ctx, typed_projection, p_context, p_substitution
	);
	if (context_status != 0) {
		return context_status;
	}
	return operation_typed_projection_materialized_typing_after_context(
		ctx, typed_projection, p_context, p_substitution, p_subject, p_classifier
	);
}

static int operation_typed_projection_publication_typing_answer(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t* p_context,
	uint32_t* p_substitution,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	int status = operation_typed_projection_materialized_typing_answer(
		ctx, typed_projection, p_context, p_substitution, p_subject, p_classifier
	);
	if (status != 0 || !ctx || !ctx->metadata || !ctx->proof_projection_view) {
		return status;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
		ctx->proof_projection_view->count) {
		return status;
	}
	const struct prototype_typed_publication_projection* publication =
		&ctx->proof_projection_view->projections[
			projection->source_occurrence
		];
	if (publication->source_occurrence != projection->source_occurrence ||
		publication->typed_projection != typed_projection) {
		return status;
	}
	if (ctx->proof_projection_view->concrete_contexts[
			projection->source_occurrence
		] != *p_context || publication->substitution != *p_substitution ||
		publication->subject == PROTOTYPE_INVALID_ID ||
		publication->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_subject = publication->subject;
	*p_classifier = publication->classifier;
	return 0;
}

static int operation_occurrence_materialized_typing_status(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t* p_context,
	uint32_t* p_substitution,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	uint32_t typed_projection = operation_solver_publication_projection(
		ctx, occurrence_id
	);
	if (typed_projection == PROTOTYPE_INVALID_ID) return 1;
	return operation_typed_projection_materialized_typing_status(
		ctx, typed_projection, p_context, p_substitution, p_subject, p_classifier
	);
}

static uint32_t operation_case_input_context(
	struct compile_context* ctx,
	uint32_t case_id
) {
	if (!ctx || !ctx->metadata || case_id >=
			ctx->metadata->typed_occurrences.case_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (case_id < ctx->accepted_typing.case_count) {
		const struct prototype_typed_publication_match_case_projection* projection =
			prototype_accepted_typing_input_match_case(
				&ctx->accepted_typing, case_id
			);
		return projection ? projection->concrete_context : PROTOTYPE_INVALID_ID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[case_id];
	uint32_t case_context = operation_context_materialized_id(
		ctx, operation_case->context_id
	);
	if (case_context == PROTOTYPE_INVALID_ID) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"case input context missing case=%u context=%u\n",
				case_id, operation_case->context_id);
		}
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t owner_match = PROTOTYPE_INVALID_ID;
	for (uint32_t occurrence_id = 0;
		occurrence_id < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			case_id >= occurrence->first_case &&
			case_id - occurrence->first_case < occurrence->case_count) {
			owner_match = occurrence_id;
			break;
		}
	}
	if (owner_match == PROTOTYPE_INVALID_ID) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr, "case input owner missing case=%u\n", case_id);
		}
		return PROTOTYPE_INVALID_ID;
	}
	const struct prototype_typed_occurrence* owner =
		&ctx->metadata->typed_occurrences.occurrences[owner_match];
	if (owner->context_action_substitution == PROTOTYPE_INVALID_ID) {
		return case_context;
	}
	const struct prototype_substitution* action = prototype_substitution_get(
		&ctx->metadata->substitutions, owner->context_action_substitution
	);
	uint32_t owner_candidate_context = owner->context_id;
	if (!action || !prototype_context_get(
			&ctx->metadata->contexts, owner_candidate_context
		) || action->target_context != owner_candidate_context
	) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"case input owner action mismatch case=%u owner=%u candidate-owner=%u "
				"owner-materialized=%u case-candidate=%u case-materialized=%u "
				"action=%u source=%u target=%u\n",
				case_id, owner_match, owner->context_id, owner_candidate_context,
				operation_case->context_id, case_context,
				owner->context_action_substitution,
				action ? action->source_context : PROTOTYPE_INVALID_ID,
				action ? action->target_context : PROTOTYPE_INVALID_ID);
		}
		return PROTOTYPE_INVALID_ID;
	}
	if (operation_case->context_id == owner_candidate_context) {
		return action->source_context;
	}
	uint32_t candidate_action = owner->context_action_substitution;
	uint32_t telescope_base = owner_candidate_context;
	uint32_t telescope_extension = operation_case->context_id;
	uint32_t materialized_path[128];
	uint32_t materialized_path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			action->target_context,
			case_context,
			materialized_path,
			128,
			&materialized_path_count
		) == 0) {
		/* Prefer the exact materialized telescope paired with this action. */
		telescope_base = action->target_context;
		telescope_extension = case_context;
	} else {
		telescope_base = owner_candidate_context;
		telescope_extension = operation_case->context_id;
	}
	uint32_t projected_context;
	uint32_t projected_action;
	struct prototype_context_classifier_view context_view =
		operation_type_context_view(ctx);
	if (prototype_context_pullback_occurrence_telescope(
			&context_view,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			telescope_base,
			telescope_extension,
			candidate_action,
			&projected_context,
			&projected_action
		) != 0) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"case input pullback failed case=%u owner=%u candidate-owner=%u "
				"owner-materialized=%u case-candidate=%u case-materialized=%u "
				"action=%u source=%u target=%u\n",
				case_id, owner_match, owner->context_id, owner_candidate_context,
				operation_case->context_id, case_context,
				candidate_action, action->source_context,
				action->target_context);
		}
		return PROTOTYPE_INVALID_ID;
	}
	(void)projected_action;
	return projected_context;
}

static uint32_t operation_case_materialized_context(
	struct compile_context* ctx,
	uint32_t case_id
) {
	if (!ctx || !ctx->metadata || case_id >=
			ctx->metadata->typed_occurrences.case_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (case_id < ctx->accepted_typing.case_count) {
		const struct prototype_typed_publication_match_case_projection* projection =
			prototype_accepted_typing_input_match_case(
				&ctx->accepted_typing, case_id
			);
		return projection ? projection->concrete_context : PROTOTYPE_INVALID_ID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[case_id];
	if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			operation_case->refinement_substitution
		);
		if (action) {
			return action->source_context;
		}
		return PROTOTYPE_INVALID_ID;
	}
	return operation_case_input_context(ctx, case_id);
}

static int compile_occurrence_child(
	const struct compile_context* ctx,
	uint32_t parent_occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_child_occurrence
) {
	if (!ctx || !ctx->metadata || !p_child_occurrence) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	int status = prototype_typed_occurrence_graph_child(
		graph, parent_occurrence, role, ordinal, p_child_occurrence
	);
	if (status != 0) {
		fprintf(
			stderr,
			"typed occurrence child missing parent=%u role=%d ordinal=%u\n",
			parent_occurrence,
			role,
			ordinal
		);
	}
	return status;
}

static int compile_fold_clause_occurrences(
	const struct compile_context* ctx,
	uint32_t fold_occurrence,
	uint32_t ordinal,
	uint32_t* p_operation_occurrence,
	uint32_t* p_clause_occurrence,
	uint32_t* p_body_occurrence
) {
	uint32_t operation_occurrence;
	uint32_t clause_occurrence;
	uint32_t continuation_lambda;
	uint32_t body_occurrence;
	if (!p_operation_occurrence || !p_clause_occurrence || !p_body_occurrence ||
		compile_occurrence_child(
			ctx, fold_occurrence, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION,
			ordinal, &operation_occurrence
		) != 0 || compile_occurrence_child(
			ctx, fold_occurrence, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
			ordinal, &clause_occurrence
		) != 0 || compile_occurrence_child(
			ctx, clause_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
			&continuation_lambda
		) != 0 || compile_occurrence_child(
			ctx, continuation_lambda, PROTOTYPE_TERM_CHILD_BODY, 0,
			&body_occurrence
		) != 0) {
		return -1;
	}
	*p_operation_occurrence = operation_occurrence;
	*p_clause_occurrence = clause_occurrence;
	*p_body_occurrence = body_occurrence;
	return 0;
}

static int compile_ast_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);
static int compile_ast_lambda_computation_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t source_ast,
	struct compile_ref* p_ret
);
static int compile_ast_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);
static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);
static int operation_solver_prepare_computation_constraint_inputs(
	struct compile_context* ctx,
	struct prototype_judgement_computation_constraint_input* inputs,
	size_t input_capacity
);
static int compile_ref_make_thunk(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	struct compile_ref* p_ret
);
static int compile_ast_match_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	struct compile_ref* p_ref
);
static int push_graph_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t* p_graph_binder_id
);
static int push_graph_computation_result_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t producer_occurrence,
	int source_classifier_authority,
	uint32_t* p_graph_binder_id
);
static int resolve_namespace_member(
	struct compile_context* ctx,
	uint32_t namespace_term,
	int member_symbol_id,
	uint32_t* p_ret,
	uint32_t* p_classifier
);

/* Operation declarations own their classifier graph before clause-body
 * lowering. Exposing only the outer domain here is declaration readback, not
 * general classifier inference; unknown aliases remain solver constraints. */
static int compile_known_operation_domain(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t* p_domain
) {
	if (!ctx || !p_domain) {
		return -1;
	}
	if (classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return 1;
	}
	uint32_t whnf = PROTOTYPE_INVALID_ID;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			classifier,
			&whnf
		) != 0) {
		return -1;
	}
	for (uint32_t depth = 0;
		depth < 32 && whnf < ctx->terms->term_count &&
		ctx->terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		whnf = ctx->terms->terms[whnf].as.effect_row_forall.body;
		if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				whnf,
				&whnf
			) != 0) {
			return -1;
		}
	}
	uint32_t ignored_family;
	if (prototype_judgement_pi_parts(
			ctx->terms, whnf, p_domain, &ignored_family
		) != 0) {
		return 1;
	}
	return 0;
}

static int operation_binder_classifier_meta_create(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t source_ast_binder_id,
	uint32_t parent_context,
	int source_classifier_authority,
	uint32_t answer,
	uint32_t* p_meta_id
);

static int push_readback_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	int symbol_id,
	uint32_t* p_graph_binder_id
) {
	if (!ctx || !p_graph_binder_id || ctx->binder_count >= 512) {
		return -1;
	}
	uint32_t graph_binder_id = prototype_term_new_binding(ctx->terms);
	if (graph_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	ctx->binders[ctx->binder_count] = (struct binder_map_entry) {
		.ast_binder_id = ast_binder_id,
		.graph_binder_id = graph_binder_id,
		.classifier = PROTOTYPE_INVALID_ID,
		.symbol_id = symbol_id
	};
	ctx->binder_count++;
	ctx->context_ids[ctx->binder_count] =
		ctx->context_ids[ctx->binder_count - 1];
	*p_graph_binder_id = graph_binder_id;
	return 0;
}

static int push_graph_pattern_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t* p_graph_binder_id
) {
	if (!ctx || !p_graph_binder_id || ctx->binder_count >= 512) {
		return -1;
	}
	uint32_t graph_binder_id = prototype_term_new_binding(ctx->terms);
	uint32_t context_id;
	uint32_t classifier_solution;
	uint32_t parent_context = ctx->context_ids[ctx->binder_count];
	if (graph_binder_id == PROTOTYPE_INVALID_ID ||
		operation_binder_classifier_meta_create(
			ctx, graph_binder_id, ast_binder_id, parent_context,
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED, classifier,
			&classifier_solution
		) != 0 ||
		prototype_context_extend_equation(
			&ctx->metadata->contexts,
			parent_context,
			graph_binder_id,
			classifier_solution,
			&context_id
		) != 0) {
		return -1;
	}
	if (prototype_context_classifier_equation_attach_context(
			&ctx->metadata->contexts, classifier_solution, context_id
		) != 0) {
		return -1;
	}
	ctx->binders[ctx->binder_count].ast_binder_id = ast_binder_id;
	ctx->binders[ctx->binder_count].graph_binder_id = graph_binder_id;
	ctx->binders[ctx->binder_count].classifier = classifier;
	ctx->binders[ctx->binder_count].symbol_id = symbol_id;
	ctx->binder_count++;
	ctx->context_ids[ctx->binder_count] = context_id;
	*p_graph_binder_id = graph_binder_id;
	return 0;
}

static int operation_add(
	struct compile_context* ctx,
	int tag,
	uint32_t core_term,
	uint32_t classifier,
	uint32_t source_ast,
	uint32_t function,
	uint32_t argument,
	uint32_t body,
	uint32_t scrutinee,
	uint32_t binder_classifier,
	uint32_t first_case,
	uint32_t case_count,
	uint32_t* p_operation
);
static int operation_binder_owner_publish(
	struct compile_context* ctx,
	uint32_t lambda_occurrence
);
static int compile_ast_computation_fold_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !node || !p_ret ||
		node->tag != PROTOTYPE_AST_COMPUTATION_FOLD ||
		(size_t)node->as.computation_fold.first_clause +
			node->as.computation_fold.clause_count >
			ctx->asts->computation_fold_clause_count ||
		node->as.computation_fold.clause_count >
			PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES) {
		return -1;
	}
	struct compile_ref computation;
	struct compile_ref return_body;
	uint32_t return_binder;
	uint32_t return_clause;
	uint32_t return_clause_operation;
	uint32_t term;
	uint32_t saved_binder_count = ctx->binder_count;
	struct prototype_computation_fold_clause core_clauses[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_typed_occurrence_fold_clause occurrence_clauses[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t clause_operation_occurrences[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t clause_lambda_occurrences[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	if (compile_ast_computation_ref(
			ctx, node->as.computation_fold.computation, &computation
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
		const struct prototype_ast_computation_fold_clause* ast_clause =
			&ctx->asts->computation_fold_clauses[
				node->as.computation_fold.first_clause + i
			];
		struct compile_ref operation;
		struct compile_ref operation_body;
		uint32_t argument_binder;
		uint32_t continuation_binder;
		uint32_t inner_lambda;
		uint32_t outer_lambda;
		uint32_t inner_lambda_operation;
		uint32_t outer_lambda_operation;
		uint32_t operation_domain = PROTOTYPE_INVALID_ID;
		ctx->binder_count = saved_binder_count;
		if (compile_ast_ref(ctx, ast_clause->operation, &operation) != 0) {
			return -1;
		}
		int domain_status = compile_known_operation_domain(
			ctx, operation.classifier, &operation_domain
		);
		if (domain_status < 0 || push_graph_binder(
				ctx,
				ast_clause->operation_argument_binder_id,
				domain_status == 0 ? operation_domain : PROTOTYPE_INVALID_ID,
				ast_clause->operation_argument_symbol_id,
				&argument_binder
			) != 0 || push_graph_binder(
				ctx,
				ast_clause->operation_continuation_binder_id,
				PROTOTYPE_INVALID_ID,
				ast_clause->operation_continuation_symbol_id,
				&continuation_binder
			) != 0 || compile_ast_computation_ref(
				ctx, ast_clause->body, &operation_body
			) != 0 || prototype_term_lambda(
				ctx->terms, continuation_binder, operation_body.term, &inner_lambda
			) != 0 || prototype_term_lambda(
				ctx->terms, argument_binder, inner_lambda, &outer_lambda
			) != 0) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
		ctx->binder_count = saved_binder_count + 1;
		if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA, inner_lambda,
				PROTOTYPE_INVALID_ID, ast_clause->body,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				operation_body.operation, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
				&inner_lambda_operation
			) != 0) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[
			inner_lambda_operation
		].referenced_ast_binder_id =
			ast_clause->operation_continuation_binder_id;
		ctx->metadata->typed_occurrences.occurrences[
			inner_lambda_operation
		].binding_id = continuation_binder;
		if (operation_binder_owner_publish(ctx, inner_lambda_operation) != 0) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
		ctx->binder_count = saved_binder_count;
		if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA, outer_lambda,
				PROTOTYPE_INVALID_ID, ast_clause->body,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				inner_lambda_operation, PROTOTYPE_INVALID_ID,
				domain_status == 0 ? operation_domain : PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, 0,
				&outer_lambda_operation
			) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[
			outer_lambda_operation
		].referenced_ast_binder_id = ast_clause->operation_argument_binder_id;
		ctx->metadata->typed_occurrences.occurrences[
			outer_lambda_operation
		].binding_id = argument_binder;
		if (operation_binder_owner_publish(ctx, outer_lambda_operation) != 0) {
			return -1;
		}
		if (outer_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[outer_lambda].tag != PROTOTYPE_TERM_LAMBDA ||
			inner_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[inner_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		core_clauses[i].operation = operation.term;
		core_clauses[i].body = outer_lambda;
		clause_operation_occurrences[i] = operation.operation;
		clause_lambda_occurrences[i] = outer_lambda_operation;
		occurrence_clauses[i].context_id =
			ctx->metadata->typed_occurrences.occurrences[operation_body.operation].context_id;
		occurrence_clauses[i].argument_ast_binder_id =
			ast_clause->operation_argument_binder_id;
		occurrence_clauses[i].argument_binder_id = argument_binder;
		occurrence_clauses[i].continuation_ast_binder_id =
			ast_clause->operation_continuation_binder_id;
		occurrence_clauses[i].continuation_binder_id = continuation_binder;
	}
	ctx->binder_count = saved_binder_count;
	if (push_graph_binder(
			ctx, node->as.computation_fold.return_binder_id, PROTOTYPE_INVALID_ID,
			node->as.computation_fold.return_symbol_id, &return_binder
		) != 0 || compile_ast_computation_ref(
			ctx, node->as.computation_fold.return_body, &return_body
		) != 0 ||
		prototype_term_lambda(ctx->terms, return_binder, return_body.term, &return_clause) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (operation_add(
			ctx,
			PROTOTYPE_TYPED_OCCURRENCE_LAMBDA,
			return_clause,
			PROTOTYPE_INVALID_ID,
			node->as.computation_fold.return_body,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			return_body.operation,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			0,
			&return_clause_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[
		return_clause_operation
	].referenced_ast_binder_id = node->as.computation_fold.return_binder_id;
	ctx->metadata->typed_occurrences.occurrences[return_clause_operation].binding_id = return_binder;
	if (operation_binder_owner_publish(ctx, return_clause_operation) != 0) {
		return -1;
	}
	if (prototype_term_computation_fold(
			ctx->terms, computation.term, return_clause, core_clauses,
			node->as.computation_fold.clause_count, &term
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_COMPUTATION_KIND_INVALID;
	if (operation_add(
		ctx, PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD, term, PROTOTYPE_INVALID_ID, ast_id,
		computation.operation,
		return_clause_operation,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, &p_ret->operation
	) != 0 || return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	struct prototype_typed_occurrence* fold_operation =
		&ctx->metadata->typed_occurrences.occurrences[p_ret->operation];
	fold_operation->fold_return_ast_binder_id =
		node->as.computation_fold.return_binder_id;
	fold_operation->fold_return_binder_id =
		ctx->terms->terms[return_clause].as.lambda.binding_id;
	struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences(ctx->metadata);
	fold_operation->first_fold_clause = (uint32_t)graph->fold_clause_count;
	fold_operation->fold_clause_count = node->as.computation_fold.clause_count;
	for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
		if (prototype_typed_occurrence_graph_add_fold_clause(
				graph, &ctx->metadata->contexts, occurrence_clauses[i], NULL
			) != 0 || prototype_typed_occurrence_graph_add_edge(
				graph, p_ret->operation,
				(struct prototype_typed_occurrence_edge) {
					.role = PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION,
					.ordinal = i,
					.child_occurrence = clause_operation_occurrences[i]
				}
			) != 0 || prototype_typed_occurrence_graph_add_edge(
				graph, p_ret->operation,
				(struct prototype_typed_occurrence_edge) {
					.role = PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
					.ordinal = i,
					.child_occurrence = clause_lambda_occurrences[i]
				}
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int compile_ast_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);
static int compile_ast_type_expr_term(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
);
static const struct prototype_function_graph_association*
compile_function_graph_association(
	const struct compile_context* ctx,
	int owner_symbol_id
);
static int compile_ast_function_type_expr_term(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
);
static int imported_type_formation_classifier(
	struct compile_context* ctx,
	struct prototype_qualified_name name,
	uint32_t* p_classifier
);
static int compile_def(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	uint32_t* p_ret
);
static int compile_def_ref(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	struct compile_ref* p_ret
);
static int compile_ast_type_def(
	struct compile_context* ctx,
	uint32_t ast_type_def_id,
	uint32_t* p_type_id
);
static int compile_shared_app(
	struct compile_context* ctx,
	uint32_t function,
	uint32_t argument,
	uint32_t* p_ret
);
static int operation_add(
	struct compile_context* ctx,
	int tag,
	uint32_t core_term,
	uint32_t classifier,
	uint32_t source_ast,
	uint32_t function,
	uint32_t argument,
	uint32_t body,
	uint32_t scrutinee,
	uint32_t binder_classifier,
	uint32_t first_case,
	uint32_t case_count,
	uint32_t* p_operation
);
static int reduce_type_namespace_term(
	struct compile_context* ctx,
	uint32_t namespace_term,
	uint32_t* p_ret
);
static int record_declaration_input(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
);

static int compile_type_expectation_classifier(
	struct compile_context* ctx,
	struct prototype_ast_type_expectation_def* expectation,
	uint32_t* p_ret
) {
	if (!ctx || !expectation || !p_ret) {
		return -1;
	}
	struct prototype_typing_source_expectation_result* result =
		compile_expectation_result(
			ctx->asts, ctx->typing_source_results, expectation
		);
	if (!result) return -1;
	if (result->state == PROTOTYPE_TYPING_SOURCE_CHECKED) {
		*p_ret = result->classifier;
		return 0;
	}
	if (result->state == PROTOTYPE_TYPING_SOURCE_CHECKING) {
		return -1;
	}

	result->state = PROTOTYPE_TYPING_SOURCE_CHECKING;
	uint32_t classifier;
	int is_function_type = expectation->type_expr < ctx->asts->type_expr_count &&
		(ctx->asts->type_exprs[expectation->type_expr].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 ctx->asts->type_exprs[expectation->type_expr].tag ==
			PROTOTYPE_AST_TYPE_EXPR_PI);
	if ((is_function_type ? compile_ast_function_type_expr_term(
				ctx, expectation->type_expr, &classifier
			) : compile_ast_type_expr_term(
				ctx, expectation->type_expr, &classifier
			)) != 0) {
		result->state = PROTOTYPE_TYPING_SOURCE_UNCHECKED;
		return -1;
	}
	if (expectation->paired_assignment_id < ctx->asts->assignment_count &&
		ctx->asts->assignments[expectation->paired_assignment_id].
			source_classifier_from_contract &&
		compile_generated_contract_close_effect_rows(
			ctx, classifier, &classifier
		) != 0) {
		result->state = PROTOTYPE_TYPING_SOURCE_UNCHECKED;
		return -1;
	}
	result->classifier = classifier;
	result->state = PROTOTYPE_TYPING_SOURCE_CHECKED;
	*p_ret = classifier;
	return 0;
}

static int record_declaration_input(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
);
static int lookup_external_declaration_classifier(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)ctx->asts->expectation_count; ++i) {
		struct prototype_ast_type_expectation_def* expectation =
			&ctx->asts->expectations[i];
		if (expectation->kind != PROTOTYPE_AST_TYPE_ENTRY_DECLARATION ||
			expectation->name_symbol_id != name_symbol_id) {
			continue;
		}
		return compile_type_expectation_classifier(ctx, expectation, p_classifier);
	}
	return -1;
}

static int lookup_imported_term_classifier(
	struct compile_context* ctx,
	struct prototype_qualified_name name,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		uint32_t export_id;
		if (!interface) {
			continue;
		}
		int found = prototype_artifact_interface_find_term_export_in_namespace(
				interface,
				name.namespace_symbol_id,
				name.name_symbol_id,
				&export_id
		);
		if (found < 0) {
			return -1;
		}
		if (found > 0) {
			continue;
		}
		/* The imported interface has already been relocated into ctx->terms by
		 * prototype_artifact_append_graph. Its classifier ID is authoritative.
		 * A canonical key without a source term can select candidates but cannot
		 * validate structural identity, so there is no key-only fallback. */
		if (interface->term_exports[export_id].classifier != PROTOTYPE_INVALID_ID &&
			interface->term_exports[export_id].classifier < ctx->terms->term_count &&
			artifact_term_present(
				&ctx->terms->terms[interface->term_exports[export_id].classifier]
			)) {
			*p_classifier = interface->term_exports[export_id].classifier;
			return 0;
		}
		return -1;
	}
	return -1;
}

static int resolve_imported_term_name(
	const struct compile_context* ctx,
	int name_symbol_id,
	struct prototype_qualified_name* p_name
) {
	int found = 0;
	if (!ctx || !p_name || name_symbol_id < 0) {
		return -1;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface = ctx->imported_interfaces[i];
		if (!interface) {
			continue;
		}
		for (size_t j = 0; j < interface->term_export_count; ++j) {
			const struct prototype_artifact_term_export* export = &interface->term_exports[j];
			if (export->name_symbol_id != name_symbol_id) {
				continue;
			}
			struct prototype_qualified_name candidate = qualified_name_make(
				export->namespace_symbol_id,
				export->name_symbol_id
			);
			if (found && !qualified_names_equal(*p_name, candidate)) {
				return -1;
			}
			*p_name = candidate;
			found = 1;
		}
	}
	return found ? 0 : 1;
}

static const struct prototype_artifact_term_export* lookup_imported_term_export(
	const struct compile_context* ctx,
	struct prototype_qualified_name name
) {
	const struct prototype_artifact_term_export* found = NULL;
	if (!ctx) {
		return NULL;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		if (!interface) {
			continue;
		}
		for (size_t j = 0; j < interface->term_export_count; ++j) {
			const struct prototype_artifact_term_export* candidate =
				&interface->term_exports[j];
			if (candidate->namespace_symbol_id != name.namespace_symbol_id ||
				candidate->name_symbol_id != name.name_symbol_id) {
				continue;
			}
			if (found) {
				return NULL;
			}
			found = candidate;
		}
	}
	return found;
}

static int resolve_imported_type_name(
	const struct compile_context* ctx,
	int name_symbol_id,
	struct prototype_qualified_name* p_name
) {
	int found = 0;
	if (!ctx || !p_name || name_symbol_id < 0) {
		return -1;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface = ctx->imported_interfaces[i];
		if (!interface) {
			continue;
		}
		for (size_t j = 0; j < interface->type_export_count; ++j) {
			const struct prototype_artifact_type_export* export = &interface->type_exports[j];
			if (export->name_symbol_id != name_symbol_id) {
				continue;
			}
			struct prototype_qualified_name candidate = qualified_name_make(
				export->namespace_symbol_id,
				export->name_symbol_id
			);
			if (found && !qualified_names_equal(*p_name, candidate)) {
				return -1;
			}
			*p_name = candidate;
			found = 1;
		}
	}
	return found ? 0 : 1;
}

static int compile_external_ref_ref(
	struct compile_context* ctx,
	int name_symbol_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret) {
		return -1;
	}
	struct prototype_qualified_name name = qualified_name_make(-1, name_symbol_id);
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	int has_classifier = 0;
	int is_imported_type = 0;
	const struct prototype_artifact_term_export* imported_export = NULL;
	if (lookup_external_declaration_classifier(ctx, name_symbol_id, &classifier) == 0) {
		name.namespace_symbol_id = ctx->namespace_symbol_id;
		has_classifier = 1;
	}
	if (!has_classifier) {
		int imported_status = resolve_imported_term_name(ctx, name_symbol_id, &name);
		if (imported_status < 0) {
			return -1;
		}
		if (imported_status == 0 &&
			lookup_imported_term_classifier(ctx, name, &classifier) == 0) {
			imported_export = lookup_imported_term_export(ctx, name);
			if (!imported_export || imported_export->source_evidence.kind ==
					PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CONDITIONAL) {
				/* The first conditional-import fragment preserves provider
				 * identity but rejects use until condition propagation can be
				 * proved for every occurrence constructor. */
				return -1;
			}
			has_classifier = 1;
		}
	}
	if (!has_classifier) {
		int imported_status = resolve_imported_type_name(ctx, name_symbol_id, &name);
		if (imported_status < 0) {
			return -1;
		}
		if (imported_status == 0 &&
			imported_type_formation_classifier(ctx, name, &classifier) == 0) {
			has_classifier = 1;
		}
	}
	/* An exported type former may itself have a Pi formation classifier, but
	 * the referenced TYPE_VIEW is still a value.  Only an ordinary term export
	 * with a Pi classifier is a raw computation function at this lowering
	 * boundary. */
	{
		struct prototype_qualified_name type_name = qualified_name_make(-1, name_symbol_id);
		int imported_status = resolve_imported_type_name(ctx, name_symbol_id, &type_name);
		if (imported_status < 0) {
			return -1;
		}
		is_imported_type = imported_status == 0;
	}
	uint32_t term;
	if (prototype_term_external_ref(ctx->terms, name, &term) != 0) {
		return -1;
	}
	if (has_classifier &&
		record_declaration_input(ctx, term, classifier) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = has_classifier ? classifier : PROTOTYPE_INVALID_ID;
	/* A declaration-only or imported name has no TermDB tag from which to
	 * recover category.  Its declared classifier is already authoritative:
	 * a Pi denotes a raw computation function, while all other named terms
	 * begin as values until an expected computation boundary elaborates them. */
	p_ret->category = PROTOTYPE_CLASSIFIER_CATEGORY_VALUE;
	p_ret->computation_kind = PROTOTYPE_COMPUTATION_KIND_INVALID;
	if (has_classifier) {
		struct prototype_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, classifier, &view
			) != 0) {
			return -1;
		}
		if (!is_imported_type &&
			view.category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_COMPUTATION_KIND_FUNCTION) {
			p_ret->category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
			p_ret->computation_kind = PROTOTYPE_COMPUTATION_KIND_FUNCTION;
		}
	}
	if (operation_add(
			ctx,
			PROTOTYPE_TYPED_OCCURRENCE_ATOM,
			term,
			p_ret->classifier,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			0,
			&p_ret->operation
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category = p_ret->category;
	ctx->metadata->typed_occurrences.occurrences[p_ret->operation].computation_kind =
		p_ret->computation_kind;
	return 0;
}

static int graph_classifier_list_contains_normalization_equal(
	struct compile_context* ctx,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
) {
	for (uint32_t i = 0; i < classifier_count; ++i) {
		struct prototype_term_conversion_result conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				classifiers[i],
				candidate
			);
		if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
		if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
			return -1;
		}
	}
	return 0;
}

static void compile_ref_clear(struct compile_ref* ref) {
	if (!ref) {
		return;
	}
	ref->term = PROTOTYPE_INVALID_ID;
	ref->classifier = PROTOTYPE_INVALID_ID;
	ref->operation = PROTOTYPE_INVALID_ID;
	ref->category = PROTOTYPE_CLASSIFIER_CATEGORY_INVALID;
	ref->computation_kind = PROTOTYPE_COMPUTATION_KIND_INVALID;
}

static void operation_default_semantics(
	const struct compile_context* ctx,
	int tag,
	uint32_t core_term,
	int* p_category,
	int* p_computation_kind,
	int* p_application_role
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	int constructor_spine = ctx && tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
		prototype_term_constructor_spine_info(
			ctx->terms,
			core_term,
			&head,
			&owner,
			&constructor_id,
			arguments,
			64,
			&argument_count
		) == 0;
	if (!p_category || !p_computation_kind || !p_application_role) {
		return;
	}
	*p_category = PROTOTYPE_CLASSIFIER_CATEGORY_INVALID;
	*p_computation_kind = PROTOTYPE_COMPUTATION_KIND_INVALID;
	*p_application_role = PROTOTYPE_TERM_APPLICATION_NONE;
	switch (tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_VAR:
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			*p_category = PROTOTYPE_CLASSIFIER_CATEGORY_VALUE;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			*p_category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
			*p_computation_kind = PROTOTYPE_COMPUTATION_KIND_FUNCTION;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			*p_category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
			*p_computation_kind = PROTOTYPE_COMPUTATION_KIND_RETURNING;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			*p_category = constructor_spine ?
				PROTOTYPE_CLASSIFIER_CATEGORY_VALUE : PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
			*p_application_role = constructor_spine ?
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION :
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION;
			return;
		default:
			return;
	}
}

static size_t operation_binding_index_hash(
	uint32_t binding_id,
	uint32_t binding_context
) {
	uint32_t value = binding_id ^ (binding_context + UINT32_C(0x9e3779b9) +
		(binding_id << 6) + (binding_id >> 2));
	value ^= value >> 16;
	value *= UINT32_C(0x7feb352d);
	value ^= value >> 15;
	value *= UINT32_C(0x846ca68b);
	value ^= value >> 16;
	return (size_t)value;
}

static struct prototype_typing_binding_index_entry* operation_binding_index_find(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t binding_context,
	int create
) {
	if (!ctx || binding_id == PROTOTYPE_INVALID_ID ||
		binding_context == PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	size_t start = operation_binding_index_hash(binding_id, binding_context) %
		PROTOTYPE_TYPING_BINDING_INDEX_CAPACITY;
	for (size_t probe = 0; probe < PROTOTYPE_TYPING_BINDING_INDEX_CAPACITY; ++probe) {
		struct prototype_typing_binding_index_entry* entry =
			&ctx->typing->bindings.index[
				(start + probe) % PROTOTYPE_TYPING_BINDING_INDEX_CAPACITY
			];
		if (!entry->occupied) {
			if (!create) {
				return NULL;
			}
			*entry = (struct prototype_typing_binding_index_entry) {
				.binding_id = binding_id,
				.binding_context = binding_context,
				.owner_index = PROTOTYPE_INVALID_ID,
				.occupied = 1
			};
			return entry;
		}
		if (entry->binding_id == binding_id &&
			entry->binding_context == binding_context) {
			return entry;
		}
	}
	return NULL;
}

static int operation_binder_classifier_meta_create(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t source_ast_binder_id,
	uint32_t parent_context,
	int source_classifier_authority,
	uint32_t answer,
	uint32_t* p_meta_id
) {
	if (!ctx || !p_meta_id ||
		(source_classifier_authority !=
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_INFERRED &&
		 source_classifier_authority !=
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED) ||
		parent_context >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	if (prototype_context_classifier_equation_intern(
			&ctx->metadata->contexts, binding_id, source_ast_binder_id,
			parent_context, p_meta_id
		) != 0) {
		return -1;
	}
	struct prototype_context_classifier_equation* equation =
		*p_meta_id < ctx->metadata->contexts.classifier_equation_count ?
		&ctx->metadata->contexts.classifier_equations[*p_meta_id] : NULL;
	if (!equation ||
		(equation->source_classifier_authority !=
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_UNSPECIFIED &&
		 equation->source_classifier_authority != source_classifier_authority)) {
		return -1;
	}
	equation->source_classifier_authority = source_classifier_authority;
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL &&
		source_ast_binder_id == 267) {
		fprintf(
			stderr,
			"classifier-authority binding=%u source-ast=%u parent=%u answer=%u "
			"authority=%d equation=%u\n",
			binding_id, source_ast_binder_id, parent_context, answer,
			source_classifier_authority, *p_meta_id
		);
	}
	return answer == PROTOTYPE_INVALID_ID ? 0 :
		prototype_context_classifier_equation_publish(
			&ctx->metadata->contexts, *p_meta_id, answer
		);
}

static struct prototype_context_classifier_equation*
operation_binder_classifier_meta_get(
	struct compile_context* ctx,
	uint32_t meta_id
) {
	if (!ctx || meta_id >= ctx->metadata->contexts.classifier_equation_count ||
		ctx->metadata->contexts.classifier_equations[meta_id].id != meta_id) {
		return NULL;
	}
	return &ctx->metadata->contexts.classifier_equations[meta_id];
}

static struct prototype_typing_binder_owner* operation_binder_owner_find_exact(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t binding_context
) {
	struct prototype_typing_binding_index_entry* index =
		operation_binding_index_find(ctx, binding_id, binding_context, 0);
	if (!index || index->owner_index == PROTOTYPE_INVALID_ID ||
		index->owner_index >= ctx->typing->bindings.owner_count) {
		return NULL;
	}
	struct prototype_typing_binder_owner* owner =
		&ctx->typing->bindings.owners[index->owner_index];
	if (owner->binding_id != binding_id ||
		owner->binding_context != binding_context) {
		return NULL;
	}
	return owner;
}

static struct prototype_typing_binder_owner* operation_binder_owner_find(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t context_id
) {
	uint32_t binding_context;
	if (!ctx || prototype_context_find_binding(
			&ctx->metadata->contexts,
			context_id,
			binding_id,
			&binding_context
		) != 0) {
		return NULL;
	}
	struct prototype_typing_binder_owner* owner = operation_binder_owner_find_exact(
		ctx, binding_id, binding_context
	);
	if (owner) {
		return owner;
	}
	/* The legacy Context resolver may still project a candidate extension to an
	 * immutable Context carrying the same Layer T classifier solution reference.
	 * Follow that reference back to the candidate introduction. This is deliberately
	 * not a BindingId-only fallback: distinct typed Context introductions sharing a
	 * Core binder must retain distinct owners. SP4 removes the projection rewrite
	 * and replaces this bridge with the candidate-to-concrete sidecar. */
	const struct prototype_context* projected = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	uint32_t solution_id = projected ?
		prototype_context_classifier_equation(projected) : PROTOTYPE_INVALID_ID;
	struct prototype_context_classifier_equation* meta =
		operation_binder_classifier_meta_get(ctx, solution_id);
	for (size_t depth = 0;
		meta && meta->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX &&
			depth < ctx->metadata->contexts.classifier_equation_count;
		++depth) {
		meta = operation_binder_classifier_meta_get(ctx, meta->key_value);
	}
	if (!meta || meta->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX ||
		meta->binding_id != binding_id ||
		meta->extension_context == PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	return operation_binder_owner_find_exact(
		ctx, binding_id, meta->extension_context
	);
}

static struct prototype_typing_binder_owner* operation_binder_owner_add(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t binding_context
) {
	struct prototype_typing_binder_owner* entry =
		operation_binder_owner_find_exact(ctx, binding_id, binding_context);
	if (entry) {
		return entry;
	}
	if (!ctx || binding_id == PROTOTYPE_INVALID_ID ||
		binding_context >= ctx->metadata->contexts.context_count ||
		ctx->typing->bindings.owner_count >= PROTOTYPE_TYPING_BINDER_OWNER_CAPACITY) {
		return NULL;
	}
	struct prototype_typing_binding_index_entry* index =
		operation_binding_index_find(ctx, binding_id, binding_context, 1);
	if (!index || index->owner_index != PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	uint32_t owner_index = ctx->typing->bindings.owner_count++;
	entry = &ctx->typing->bindings.owners[owner_index];
	*entry = (struct prototype_typing_binder_owner) {
		.binding_id = binding_id,
		.binding_context = binding_context,
		.lambda_occurrence = PROTOTYPE_INVALID_ID,
		.bound_computation_occurrence = PROTOTYPE_INVALID_ID,
		.sequence_fold_occurrence = PROTOTYPE_INVALID_ID,
		.match_occurrence = PROTOTYPE_INVALID_ID,
		.match_case_index = PROTOTYPE_INVALID_ID,
		.match_field_index = PROTOTYPE_INVALID_ID,
		.projected_pure_value = PROTOTYPE_INVALID_ID,
		.first_classifier_evidence = PROTOTYPE_INVALID_ID
	};
	index->owner_index = owner_index;
	return entry;
}

static int operation_binder_classifier_evidence_publish(
	struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->typing->bindings.classifier_evidence_count >=
			PROTOTYPE_TYPING_BINDER_OWNER_CAPACITY) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
	if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		occurrence->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t binding_context;
	if (prototype_context_find_binding(
			&ctx->metadata->contexts,
			occurrence->context_id,
			occurrence->binding_id,
			&binding_context
		) != 0) {
		return -1;
	}
	struct prototype_typing_binder_owner* owner =
		operation_binder_owner_add(
			ctx, occurrence->binding_id, binding_context
		);
	if (!owner) {
		return -1;
	}
	uint32_t evidence_id = ctx->typing->bindings.classifier_evidence_count++;
	ctx->typing->bindings.classifier_evidence[evidence_id] =
		(struct prototype_typing_binder_classifier_evidence) {
			.occurrence_id = occurrence_id,
			.next = owner->first_classifier_evidence
		};
	owner->first_classifier_evidence = evidence_id;
	return 0;
}

static int operation_match_binder_owners_publish(
	struct compile_context* ctx,
	uint32_t match_occurrence
) {
	if (!ctx || !ctx->metadata || match_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[match_occurrence];
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		match->first_case == PROTOTYPE_INVALID_ID ||
		match->first_case > ctx->metadata->typed_occurrences.case_count ||
		match->case_count > ctx->metadata->typed_occurrences.case_count -
			match->first_case) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct prototype_typed_occurrence_match_case* match_case =
			&ctx->metadata->typed_occurrences.cases[
				match->first_case + case_index
			];
		for (uint32_t field_index = 0;
			field_index < match_case->binder_count;
			++field_index) {
			uint32_t binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					match_case->context_id,
					match_case->binder_ids[field_index],
					&binding_context
				) != 0) {
				return -1;
			}
			struct prototype_typing_binder_owner* owner =
				operation_binder_owner_add(
					ctx,
					match_case->binder_ids[field_index],
					binding_context
				);
			if (!owner ||
				(owner->match_occurrence != PROTOTYPE_INVALID_ID &&
				 (owner->match_occurrence != match_occurrence ||
				  owner->match_case_index != case_index ||
				  owner->match_field_index != field_index))) {
				return -1;
			}
			owner->match_occurrence = match_occurrence;
			owner->match_case_index = case_index;
			owner->match_field_index = field_index;
		}
	}
	return 0;
}

static int operation_binder_owner_merge(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t owner_occurrence
) {
	if (!ctx || !ctx->metadata || binding_id == PROTOTYPE_INVALID_ID ||
		owner_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* candidate =
		&ctx->metadata->typed_occurrences.occurrences[owner_occurrence];
	if (candidate->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		candidate->binding_id != binding_id) {
		return -1;
	}
	uint32_t body_occurrence;
	if (compile_occurrence_child(
			ctx, owner_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
			&body_occurrence
		) != 0 || body_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t binding_context = ctx->metadata->typed_occurrences.occurrences[
		body_occurrence
	].context_id;
	struct prototype_typing_binder_owner* entry =
		operation_binder_owner_add(ctx, binding_id, binding_context);
	if (!entry) {
		return -1;
	}
	uint32_t current_occurrence = entry->lambda_occurrence;
	if (current_occurrence == PROTOTYPE_INVALID_ID) {
		entry->lambda_occurrence = owner_occurrence;
		return 0;
	}
	if (current_occurrence == owner_occurrence) {
		return 0;
	}
	if (current_occurrence >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* current =
		&ctx->metadata->typed_occurrences.occurrences[current_occurrence];
	if (current->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		current->binding_id != binding_id) {
		return -1;
	}
	uint32_t current_body;
	return compile_occurrence_child(
			ctx, current_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
			&current_body
		) == 0 && current_body <
			ctx->metadata->typed_occurrences.occurrence_count &&
		ctx->metadata->typed_occurrences.occurrences[current_body].context_id ==
			binding_context ? 0 : -1;
}

static int operation_binder_classifier_owner_merge(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t classifier
) {
	if (!ctx || context_id >= ctx->metadata->contexts.context_count ||
		binding_id == PROTOTYPE_INVALID_ID ||
		classifier == PROTOTYPE_INVALID_ID ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t binding_context;
	if (prototype_context_find_binding(
			&ctx->metadata->contexts,
			context_id,
			binding_id,
			&binding_context
		) != 0) {
		return -1;
	}
	const struct prototype_context* context = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	struct prototype_context_classifier_equation* equation = context ?
		operation_binder_classifier_meta_get(
			ctx, prototype_context_classifier_equation(context)
		) : NULL;
	if (!equation || equation->binding_id != binding_id ||
		equation->extension_context != binding_context) {
		return 1;
	}
	if (equation->answer == PROTOTYPE_INVALID_ID) {
		return prototype_context_classifier_equation_publish(
			&ctx->metadata->contexts, equation->id, classifier
		);
	}
	return prototype_judgement_classifier_conversion(
		ctx->terms,
		ctx->type_declarations,
		equation->answer,
		classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

static int operation_binder_owner_publish(
	struct compile_context* ctx,
	uint32_t lambda_occurrence
) {
	if (!ctx || !ctx->metadata || lambda_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* lambda =
		&ctx->metadata->typed_occurrences.occurrences[lambda_occurrence];
	if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		lambda->binding_id == PROTOTYPE_INVALID_ID ||
		operation_binder_owner_merge(
			ctx, lambda->binding_id, lambda_occurrence
		) != 0) {
		return -1;
	}
	uint32_t body_occurrence;
	if (compile_occurrence_child(
			ctx, lambda_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
			&body_occurrence
		) != 0 || body_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t body_context = ctx->metadata->typed_occurrences.occurrences[
		body_occurrence
	].context_id;
	if (!operation_binder_owner_find_exact(
			ctx, lambda->binding_id, body_context
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->contexts.classifier_equation_count; ++i) {
		struct prototype_context_classifier_equation* meta =
			&ctx->metadata->contexts.classifier_equations[i];
		if (meta->binding_id == lambda->binding_id &&
			meta->parent_context == lambda->context_id &&
			meta->extension_context == body_context) {
			meta->owner_occurrence = lambda_occurrence;
		}
	}
	return 0;
}

/* Context pullback replaces the typed source occurrence itself. Replace the
 * corresponding input seed and discard any solution computed in the old
 * Context; this is not a solver refinement. */
static int operation_binder_owner_index_seed_transaction(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata ||
		!ctx->metadata->typed_occurrences.transaction_active ||
		ctx->typing->bindings.owner_count != 0 ||
		ctx->typing->bindings.classifier_evidence_count != 0 ||
		ctx->typing->bindings.indexed_occurrence_count != 0 ||
		ctx->typing->bindings.pending_assumption_count != 0) {
		return -1;
	}
	size_t immutable_prefix =
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t occurrence_id = 0;
		occurrence_id < immutable_prefix;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		if ((occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			 operation_binder_owner_publish(ctx, occurrence_id) != 0) ||
			(occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			 operation_match_binder_owners_publish(ctx, occurrence_id) != 0) ||
			(occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			 operation_binder_classifier_evidence_publish(
				ctx, occurrence_id
			 ) != 0)) {
			return -1;
		}
	}
	ctx->typing->bindings.indexed_occurrence_count = immutable_prefix;
	ctx->typing->bindings.index_valid = 1;
	if (immutable_prefix != 0) {
		/* The index belongs to one compiler transaction, not to semantic
		 * metadata. Seed its immutable accepted prefix once; solver passes never
		 * reconstruct it. */
		ctx->metadata->binder_owner_index_rebuild_count++;
	}
	return 0;
}

static int operation_binder_owner_index_validate(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t occurrence_id = 0;
		occurrence_id < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
				struct prototype_typing_binder_owner* owner =
					operation_binder_owner_find(
						ctx, occurrence->binding_id, occurrence->context_id
					);
				int found = 0;
				for (uint32_t evidence_id = owner ?
						owner->first_classifier_evidence : PROTOTYPE_INVALID_ID;
					evidence_id != PROTOTYPE_INVALID_ID;
					evidence_id =
						ctx->typing->bindings.classifier_evidence[evidence_id].next) {
					if (evidence_id >= ctx->typing->bindings.classifier_evidence_count) {
						return -1;
					}
					if (ctx->typing->bindings.classifier_evidence[evidence_id].occurrence_id ==
							occurrence_id) {
						found = 1;
						break;
					}
				}
				if (!found) {
					return -1;
				}
			}
			continue;
		}
		uint32_t body_occurrence;
		if (compile_occurrence_child(
				ctx, occurrence_id, PROTOTYPE_TERM_CHILD_BODY, 0,
				&body_occurrence
			) != 0 || body_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		struct prototype_typing_binder_owner* entry =
			operation_binder_owner_find(
				ctx,
				occurrence->binding_id,
				ctx->metadata->typed_occurrences.occurrences[
					body_occurrence
				].context_id
			);
		if (!entry || entry->lambda_occurrence == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (uint32_t pending_id = 0;
		pending_id < ctx->typing->seed.binder_input_count;
		++pending_id) {
		const struct prototype_typing_binder_input* pending =
			&ctx->typing->seed.binder_inputs[pending_id];
		if (pending->binder_var >= ctx->terms->term_count ||
			ctx->terms->terms[pending->binder_var].tag != PROTOTYPE_TERM_VAR ||
			!operation_binder_owner_find(
				ctx,
				ctx->terms->terms[pending->binder_var].as.var.binding_id,
				pending->context_id
			)) {
			return -1;
		}
	}
	return 0;
}

static int operation_binder_owners_ensure(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	if (ctx->typing->bindings.index_valid &&
		ctx->typing->bindings.indexed_occurrence_count ==
			ctx->metadata->typed_occurrences.occurrence_count &&
		ctx->typing->bindings.pending_assumption_count ==
			ctx->typing->seed.binder_input_count) {
		return getenv("A_PROGRAM_BINDER_OWNER_VALIDATE") ?
			operation_binder_owner_index_validate(ctx) : 0;
	}
	fprintf(
		stderr,
		"binder owner index publication mismatch occurrences=%zu/%zu "
		"assumptions=%u/%u\n",
		ctx->typing->bindings.indexed_occurrence_count,
		ctx->metadata->typed_occurrences.occurrence_count,
		ctx->typing->bindings.pending_assumption_count,
		ctx->typing->seed.binder_input_count
	);
	return -1;
}

static int operation_add(
	struct compile_context* ctx,
	int tag,
	uint32_t core_term,
	uint32_t classifier,
	uint32_t source_ast,
	uint32_t function,
	uint32_t argument,
	uint32_t body,
	uint32_t scrutinee,
	uint32_t binder_classifier,
	uint32_t first_case,
	uint32_t case_count,
	uint32_t* p_operation
) {
	if (!ctx || !ctx->metadata || !p_operation ||
		core_term >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_typed_occurrence node;
	memset(&node, 0, sizeof(node));
	node.tag = tag;
	operation_default_semantics(
		ctx,
		tag,
		core_term,
		&node.category,
		&node.computation_kind,
		&node.application_role
	);
	if (tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (body >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		node.category = ctx->metadata->typed_occurrences.occurrences[body].category;
		node.computation_kind =
			ctx->metadata->typed_occurrences.occurrences[body].computation_kind;
	}
	node.context_id = ctx->context_ids[ctx->binder_count];
	node.context_action_substitution = PROTOTYPE_INVALID_ID;
	node.source_core_term = PROTOTYPE_INVALID_ID;
	node.source_classifier = PROTOTYPE_INVALID_ID;
	node.core_term = core_term;
	node.declared_classifier = tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR ?
		classifier : PROTOTYPE_INVALID_ID;
	node.classifier = PROTOTYPE_INVALID_ID;
	node.classifier_status = PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING;
	node.classifier_verification_obligation = PROTOTYPE_INVALID_ID;
	node.source_ast = source_ast;
	node.source_symbol_id = -1;
	node.binder_symbol_id = -1;
	node.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
	node.binding_id = PROTOTYPE_INVALID_ID;
	node.first_edge = PROTOTYPE_INVALID_ID;
	node.edge_count = 0;
	node.wrapped_occurrence = tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ? function :
		tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ? body : PROTOTYPE_INVALID_ID;
	if (tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
		ctx->terms->terms[core_term].tag == PROTOTYPE_TERM_VAR) {
		node.binding_id = ctx->terms->terms[core_term].as.var.binding_id;
	} else if (tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		ctx->terms->terms[core_term].tag == PROTOTYPE_TERM_LAMBDA) {
		node.binding_id = ctx->terms->terms[core_term].as.lambda.binding_id;
	}
	/* The mutable binder-domain answer belongs to the solver seed/meta stores.
	 * TypedOccurrence receives it only at the checked freeze boundary. */
	node.binder_classifier = PROTOTYPE_INVALID_ID;
	node.ih_owner_occurrence = tag ==
		PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ? scrutinee :
		PROTOTYPE_INVALID_ID;
	node.ih_scope_id = PROTOTYPE_INVALID_ID;
	node.ih_case_index = PROTOTYPE_INVALID_ID;
	node.ih_field_index = PROTOTYPE_INVALID_ID;
	node.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID;
	node.fold_return_binder_id = PROTOTYPE_INVALID_ID;
	node.first_case = first_case;
	node.case_count = case_count;
	node.first_fold_clause = PROTOTYPE_INVALID_ID;
	node.fold_clause_count = 0;

	struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences(ctx->metadata);
	if (prototype_typed_occurrence_graph_add(
			graph, &ctx->metadata->contexts, node, p_operation
		) != 0) {
		return -1;
	}
	if (tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
		operation_binder_classifier_evidence_publish(
			ctx, *p_operation
		) != 0) {
		return -1;
	}
	ctx->typing->bindings.indexed_occurrence_count = graph->occurrence_count;
	ctx->typing->seed.classifier_seeds[*p_operation].classifier = classifier;
	ctx->typing->seed.classifier_seeds[*p_operation].binder_classifier = binder_classifier;
	if (getenv("A_PROGRAM_HAS_TYPE_TRACE") != NULL &&
		tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
		fprintf(
			stderr,
			"constructor-add occurrence=%u core=%u classifier=%u ast=%u context=%u\n",
			*p_operation, core_term, classifier, source_ast, node.context_id
		);
	}
	struct prototype_typed_occurrence_edge edges[64];
	uint32_t edge_count = 0;
#define APPEND_OCCURRENCE_EDGE(edge_role, edge_ordinal, edge_child) \
	do { \
		if ((edge_child) >= graph->occurrence_count || edge_count >= 64) { \
			return -1; \
		} \
		edges[edge_count++] = (struct prototype_typed_occurrence_edge) { \
			.role = (edge_role), \
			.ordinal = (edge_ordinal), \
			.child_occurrence = (edge_child) \
		}; \
	} while (0)
	switch (tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_FUNCTION, 0, function);
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_ARGUMENT, 0, argument);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_BODY, 0, body);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, scrutinee);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0, argument
			);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, argument);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0, argument
			);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
			APPEND_OCCURRENCE_EDGE(PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0, argument);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0, function
			);
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0, argument
			);
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0, body
			);
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0, function
			);
			APPEND_OCCURRENCE_EDGE(
				PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, argument
			);
			break;
		default:
			break;
	}
	for (uint32_t i = 0; i < edge_count; ++i) {
		if (prototype_typed_occurrence_graph_add_edge(
				graph, *p_operation, edges[i]
			) != 0) {
			return -1;
		}
	}
#undef APPEND_OCCURRENCE_EDGE
	return 0;
}

static uint32_t operation_available_classifier(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (occurrence_id < ctx->accepted_typing.occurrence_count) {
		const struct prototype_typed_publication_projection* accepted =
			prototype_accepted_typing_input_projection(
				&ctx->accepted_typing, occurrence_id, NULL
			);
		return accepted ? accepted->classifier : PROTOTYPE_INVALID_ID;
	}
	uint32_t solved = operation_solver_classifier(ctx, occurrence_id);
	if (solved != PROTOTYPE_INVALID_ID) {
		return solved;
	}
	if (ctx->typing->seed.classifier_seeds[occurrence_id].classifier !=
		PROTOTYPE_INVALID_ID) {
		return ctx->typing->seed.classifier_seeds[occurrence_id].classifier;
	}
	return PROTOTYPE_INVALID_ID;
}

static int operation_add_match_case(
	struct compile_context* ctx,
	uint32_t body_occurrence,
	uint32_t constructor_owner,
	uint32_t constructor_id,
	const struct prototype_ast_match_case* ast_case,
	const struct prototype_case_binder* occurrence_binders,
	uint32_t* p_case
) {
	if (!ctx || !ctx->metadata || !p_case ||
		!ast_case || (ast_case->binder_count != 0 && !occurrence_binders) ||
		ast_case->binder_count >
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY ||
		ast_case->first_binder + ast_case->binder_count >
			ctx->asts->case_binder_count) {
		return -1;
	}
	struct prototype_typed_occurrence_match_case match_case;
	memset(&match_case, 0, sizeof(match_case));
	match_case.context_id =
		ctx->metadata->typed_occurrences.occurrences[body_occurrence].context_id;
	match_case.refinement_status =
		PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING;
	match_case.refinement_substitution = PROTOTYPE_INVALID_ID;
	match_case.declared_constructor_owner = constructor_owner;
	match_case.declared_constructor_id = constructor_id;
	match_case.constructor_owner = constructor_owner;
	match_case.constructor_id = constructor_id;
	match_case.case_label_symbol_id = -1;
	match_case.binder_count = ast_case->binder_count;
	for (uint32_t i = 0; i < ast_case->binder_count; ++i) {
		match_case.binder_ids[i] = occurrence_binders[i].binding_id;
		match_case.ast_binder_ids[i] =
			ctx->asts->case_binders[ast_case->first_binder + i].ast_binder_id;
	}
	struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences(ctx->metadata);
	if (prototype_typed_occurrence_graph_add_case(
			graph, &ctx->metadata->contexts, match_case, p_case
		) != 0) {
		return -1;
	}
	return 0;
}

static int operation_callable_classifier_parts(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	int open_pure_type_family,
	uint32_t* p_specialized_function,
	uint32_t* p_pi,
	uint32_t* p_expected_domain
) {
	uint32_t specialized_function;
	uint32_t whnf;
	if (!ctx || !p_specialized_function || !p_pi || !p_expected_domain ||
		function_classifier >= ctx->terms->term_count ||
		argument_classifier >= ctx->terms->term_count ||
		argument_term >= ctx->terms->term_count) {
		return 1;
	}
	if (prototype_judgement_expose_callable_classifier(
			ctx->terms, ctx->type_declarations,
			function_classifier, &function_classifier
		) != 0) {
		return -1;
	}
	int specialization_status =
		prototype_judgement_specialize_effect_rows_for_argument(
			ctx->terms,
			ctx->type_declarations,
			function_classifier,
			argument_classifier,
			&specialized_function
		);
	if (specialization_status != 0) {
		return specialization_status;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			specialized_function,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 1;
	}
	uint32_t expected_domain;
	if (prototype_judgement_classifier_value_whnf(
			ctx->terms, ctx->type_declarations,
			ctx->terms->terms[whnf].as.pi.domain, &expected_domain
		) != 0 || expected_domain >= ctx->terms->term_count) {
		return -1;
	}
	if (open_pure_type_family &&
		ctx->terms->terms[expected_domain].tag == PROTOTYPE_TERM_THUNK_TYPE &&
		ctx->terms->terms[argument_term].tag == PROTOTYPE_TERM_LAMBDA) {
		uint32_t opened_domain =
			ctx->terms->terms[expected_domain].as.thunk_type.computation;
		if (opened_domain >= ctx->terms->term_count ||
			ctx->terms->terms[opened_domain].tag != PROTOTYPE_TERM_PI) {
			return 1;
		}
		expected_domain = opened_domain;
	}
	*p_specialized_function = specialized_function;
	*p_pi = whnf;
	*p_expected_domain = expected_domain;
	return 0;
}

static int operation_callable_classifier_domain(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t* p_expected_domain
) {
	uint32_t whnf;
	if (!ctx || !p_expected_domain ||
		function_classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (prototype_judgement_expose_callable_classifier(
			ctx->terms, ctx->type_declarations,
			function_classifier, &function_classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			function_classifier,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 1;
	}
	return prototype_judgement_classifier_value_whnf(
		ctx->terms,
		ctx->type_declarations,
		ctx->terms->terms[whnf].as.pi.domain,
		p_expected_domain
	);
}

static int operation_apply_classifier(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	uint32_t specialized_function;
	if (!ctx || !p_classifier || function_classifier == PROTOTYPE_INVALID_ID ||
		argument_classifier == PROTOTYPE_INVALID_ID ||
		function_classifier >= ctx->terms->term_count ||
		argument_classifier >= ctx->terms->term_count || argument_term >= ctx->terms->term_count) {
		return 1;
	}
	uint32_t whnf;
	uint32_t expected_domain;
	int parts_status = operation_callable_classifier_parts(
		ctx,
		function_classifier,
		argument_classifier,
		argument_term,
		0,
		&specialized_function,
		&whnf,
		&expected_domain
	);
	if (parts_status != 0) {
		if (getenv("A_PROGRAM_FORMATION_TRACE")) {
			fprintf(
				stderr,
				"application effect specialization status=%d function=%u argument=%u\n",
				parts_status, function_classifier, argument_classifier
			);
		}
		return parts_status;
	}
	function_classifier = specialized_function;
	const struct prototype_term* pi = &ctx->terms->terms[whnf];
	uint32_t solved_expected_domain;
	if (getenv("A_PROGRAM_FORMATION_TRACE")) {
		fprintf(stderr, "application function classifier: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, function_classifier
		);
		fprintf(stderr, "\napplication expected domain: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, expected_domain
		);
		fprintf(stderr, "\napplication argument classifier: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, argument_classifier
		);
		fprintf(stderr, "\napplication argument term: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, argument_term
		);
		fputc('\n', stderr);
	}
	int domain_solve_status = prototype_judgement_solve_expected_effect_rows(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		expected_domain,
		argument_classifier,
		&solved_expected_domain
	);
	if (domain_solve_status != 0) {
		if (getenv("A_PROGRAM_FORMATION_TRACE")) {
			fprintf(
				stderr,
				"application domain solve status=%d expected=%u argument=%u\n",
				domain_solve_status, expected_domain, argument_classifier
			);
		}
		return domain_solve_status;
	}
	uint32_t family_id;
	if (prototype_term_pure_family_lambda(
			ctx->terms, pi->as.pi.codomain_family, &family_id
		) != 0) {
		return -1;
	}
	uint32_t binding_id;
	uint32_t body;
	if (prototype_term_pure_family_parts(
			ctx->terms,
			pi->as.pi.codomain_family,
			&binding_id,
			&body
		) != 0) {
		return -1;
	}
	(void)family_id;
	uint32_t classifier_argument;
	if (prototype_judgement_dependent_classifier_argument(
			ctx->terms,
			ctx->type_declarations,
			expected_domain,
			argument_term,
			&classifier_argument
		) != 0) {
		return -1;
	}
	return prototype_term_graph_substitute_bound_var(
		ctx->terms, body,
		binding_id,
		classifier_argument,
		p_classifier
	);
}

static int operation_apply_pure_type_family_classifier(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	return prototype_judgement_static_family_app_classifier(
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		context_id,
		function_classifier,
		argument_term,
		argument_classifier,
		p_classifier
	);
}

/* Lowering may know an operation's Pi classifier before it knows the classifier
 * of the source argument.  Build the dependent codomain structurally here;
 * OPERATION_CONSTRAINT_PI_EXPECTED records and validates the domain equation
 * during classifier solving. */
static int operation_apply_classifier_unchecked(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	uint32_t whnf;
	uint32_t binding_id;
	uint32_t body;
	if (!ctx || !p_classifier || function_classifier == PROTOTYPE_INVALID_ID ||
		function_classifier >= ctx->terms->term_count || argument_term >= ctx->terms->term_count) {
		return -1;
	}
	/* During graph construction the argument classifier may still be a solver
	 * variable. Keep an operation's quantified effect-row binder in the graph,
	 * but expose its Pi body so the dependent result family can be formed. */
	while (function_classifier < ctx->terms->term_count &&
		ctx->terms->terms[function_classifier].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		function_classifier =
			ctx->terms->terms[function_classifier].as.effect_row_forall.body;
	}
	if (function_classifier >= ctx->terms->term_count ||
		prototype_term_normalize_complete_with_profile(
			ctx->terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			function_classifier,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			ctx->terms,
			ctx->terms->terms[whnf].as.pi.codomain_family,
			&binding_id,
			&body
		) != 0) {
		return -1;
	}
	return prototype_term_graph_substitute_bound_var(
		ctx->terms, body, binding_id, argument_term, p_classifier
	);
}

static int type_instance_formation_classifier(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || term >= ctx->terms->term_count) {
		return 1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, term, &type_id, arguments, &argument_count
		) != 0 || type_id >= ctx->type_declarations->semantic_schema.type_count ||
		argument_count > 16) {
		return 1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (type->formation_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t classifier = type->formation_classifier;
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t whnf;
		if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				classifier,
				&whnf
			) != 0 || whnf >= ctx->terms->term_count ||
			ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
			return -1;
		}
		const struct prototype_term* pi = &ctx->terms->terms[whnf];
		uint32_t family_id;
		if (prototype_term_pure_family_lambda(
				ctx->terms, pi->as.pi.codomain_family, &family_id
			) != 0) {
			return -1;
		}
		uint32_t binding_id;
		uint32_t body;
		if (prototype_term_pure_family_parts(
					ctx->terms,
					pi->as.pi.codomain_family,
					&binding_id,
					&body
				) != 0) {
			return -1;
		}
		(void)family_id;
		if (
			prototype_term_graph_substitute_bound_var(
				ctx->terms, body,
				binding_id,
				arguments[i],
				&classifier
			) != 0) {
			return -1;
		}
	}
	*p_classifier = classifier;
	return 0;
}

static int compile_ref_from_term(
	struct compile_context* ctx,
	uint32_t term,
	struct compile_ref* p_ref
) {
	/* Introduce a fresh Layer T occurrence for an explicitly supplied Layer C
	 * result. This routine never searches existing occurrences or Judgements by
	 * Term identity. A caller that already has an occurrence must preserve it. */
	if (!ctx || !p_ref || term >= ctx->terms->term_count) {
		return -1;
	}
	p_ref->term = term;
	p_ref->classifier = PROTOTYPE_INVALID_ID;
	p_ref->operation = PROTOTYPE_INVALID_ID;
	p_ref->category =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_FORCE ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_OPERATION_REQUEST ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD ?
			PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION : PROTOTYPE_CLASSIFIER_CATEGORY_VALUE;
	p_ref->computation_kind =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ?
			PROTOTYPE_COMPUTATION_KIND_RETURNING :
			ctx->terms->terms[term].tag == PROTOTYPE_TERM_LAMBDA ?
				PROTOTYPE_COMPUTATION_KIND_FUNCTION :
				PROTOTYPE_COMPUTATION_KIND_INVALID;
	if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		if (prototype_judgement_pure_primitive_classifier(
				ctx->terms,
				ctx->type_declarations,
				&ctx->terms->terms[term],
				&p_ref->classifier
			) != 0) {
			return -1;
		}
		p_ref->category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
		p_ref->computation_kind = PROTOTYPE_COMPUTATION_KIND_FUNCTION;
	} else if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		if (prototype_judgement_effect_operation_classifier(
				ctx->terms, &ctx->terms->terms[term], &p_ref->classifier
			) != 0) {
			return -1;
		}
		p_ref->category = PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION;
		p_ref->computation_kind = PROTOTYPE_COMPUTATION_KIND_FUNCTION;
	} else if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_TEXT_LITERAL) {
		if (prototype_term_make_host_type(
				ctx->terms,
				PROTOTYPE_HOST_TYPE_TEXT,
				&p_ref->classifier
			) != 0) {
			return -1;
		}
	} else if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_INT_LITERAL) {
		int integer_type = ctx->intrinsic_environment->default_integer_host_type;
		int bit_width = prototype_term_host_type_bit_width(integer_type);
		int64_t value = ctx->terms->terms[term].as.int_literal.value;
		if ((bit_width == 32 && (value < INT32_MIN || value > INT32_MAX)) ||
			(bit_width != 32 && bit_width != 64)) {
			return -1;
		}
		if (prototype_term_make_host_type(
				ctx->terms,
				integer_type,
				&p_ref->classifier
			) != 0) {
			return -1;
		}
	} else if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		const struct prototype_qualified_name name =
			ctx->terms->terms[term].as.external_ref.name;
		int imported_status = imported_type_formation_classifier(
			ctx,
			name,
			&p_ref->classifier
		);
		if (imported_status < 0) {
			return -1;
		}
		if (imported_status > 0 &&
			(name.namespace_symbol_id == -1 ||
			 name.namespace_symbol_id == ctx->namespace_symbol_id)) {
			(void)lookup_external_declaration_classifier(
				ctx, name.name_symbol_id, &p_ref->classifier
			);
		}
		if (imported_status > 0 && p_ref->classifier == PROTOTYPE_INVALID_ID) {
			(void)lookup_imported_term_classifier(ctx, name, &p_ref->classifier);
		}
		if (imported_status == 0 &&
			record_declaration_input(ctx, term, p_ref->classifier) != 0) {
			return -1;
		}
	} else {
		int type_status = type_instance_formation_classifier(
			ctx, term, &p_ref->classifier
		);
		if (type_status < 0) {
			return -1;
		}
	}
	if (operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_ATOM, term, p_ref->classifier,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ref->operation
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[p_ref->operation].category = p_ref->category;
	ctx->metadata->typed_occurrences.occurrences[p_ref->operation].computation_kind =
		p_ref->computation_kind;
	return 0;
}

static int select_match_resolution_scrutinee_classifier(
	struct compile_context* ctx,
	const struct prototype_typing_match_resolution_goal* resolution,
	uint32_t* p_scrutinee_classifier
) {
	if (!ctx || !resolution || !p_scrutinee_classifier ||
		resolution->match_operation >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		resolution->scrutinee_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t match_projection = operation_solver_publication_projection(
		ctx, resolution->match_operation
	);
	uint32_t scrutinee_projection;
	if (match_projection == PROTOTYPE_INVALID_ID ||
		operation_solver_projected_child(
			ctx,
			match_projection,
			PROTOTYPE_TERM_CHILD_SCRUTINEE,
			0,
			&scrutinee_projection
		) != 0) {
		return -1;
	}
	const struct prototype_typed_projection* projected =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, scrutinee_projection
		);
	if (!projected || projected->source_occurrence !=
			resolution->scrutinee_operation) {
		return -1;
	}
	uint32_t classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_projection
	);
	if (classifier != PROTOTYPE_INVALID_ID) {
		*p_scrutinee_classifier = classifier;
		return 0;
	}
	/* Constructor resolution waits for this exact Match use-site. */
	return 1;
}

static int resolve_match_projection_constructor(
	struct compile_context* ctx,
	const struct prototype_typing_match_resolution_goal* resolution,
	uint32_t* p_schema_classifier,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!ctx || !resolution || !p_schema_classifier || !p_resolution) {
		return -1;
	}
	uint32_t projected_classifier;
	int selection_status = select_match_resolution_scrutinee_classifier(
		ctx, resolution, &projected_classifier
	);
	if (selection_status != 0) {
		return selection_status;
	}
	uint32_t schema;
	int schema_status = prototype_judgement_project_nominal_classifier_instance(
		ctx->terms,
		ctx->type_declarations,
		projected_classifier,
		&schema
	);
	if (schema_status != 0) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
			fprintf(stderr,
				"match-family-projection match=%u scrutinee=%u classifier=%u status=%d\n",
				resolution->match_operation,
				resolution->scrutinee_operation,
				projected_classifier,
				schema_status);
			prototype_type_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, projected_classifier
			);
			fputc('\n', stderr);
		}
		return schema_status;
	}
	if (prototype_judgement_resolve_match_constructor(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			schema,
			resolution->constructor_symbol_id,
			p_resolution
		) == 0) {
		*p_schema_classifier = schema;
		return 0;
	}
	return -1;
}

static int lookup_graph_binder(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t* p_graph_binder_id
) {
	if (!ctx || !p_graph_binder_id) {
		return -1;
	}
	for (uint32_t i = ctx->binder_count; i > 0; --i) {
		const struct binder_map_entry* entry = &ctx->binders[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			*p_graph_binder_id = entry->graph_binder_id;
			return 0;
		}
	}
	return -1;
}

static int lookup_ih_edge(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t* p_frame_id,
	uint32_t* p_case_index,
	uint32_t* p_field_index
) {
	if (!ctx || !p_frame_id || !p_case_index || !p_field_index) {
		return -1;
	}
	for (uint32_t i = ctx->ih_scope_count; i > 0; --i) {
		const struct ih_scope_map_entry* entry = &ctx->ih_scopes[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			*p_frame_id = entry->ih_scope_id;
			*p_case_index = entry->case_index;
			*p_field_index = entry->field_index;
			return 0;
		}
	}
	return -1;
}

static int add_compile_label(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t term,
	uint32_t exposed_classifier,
	uint32_t exposed_occurrence,
	uint32_t expectation_occurrence,
	uint32_t expectation_classifier
) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	if ((expectation_occurrence == PROTOTYPE_INVALID_ID) !=
		(expectation_classifier == PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	if (reserve_slot(ctx->metadata->label_count, ctx->metadata->label_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)ctx->metadata->label_count;
	ctx->metadata->labels[id].name_symbol_id = name_symbol_id;
	ctx->metadata->labels[id].term = term;
	ctx->metadata->labels[id].exposed_classifier = exposed_classifier;
	ctx->metadata->labels[id].exposed_occurrence = exposed_occurrence;
	ctx->metadata->labels[id].body_occurrence = exposed_occurrence;
	if (exposed_occurrence < ctx->metadata->typed_occurrences.occurrence_count &&
		ctx->metadata->typed_occurrences.occurrences[exposed_occurrence].tag ==
			PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		expectation_occurrence = exposed_occurrence;
		expectation_classifier =
			ctx->typing->seed.classifier_seeds[exposed_occurrence].classifier;
		ctx->metadata->labels[id].body_occurrence =
			ctx->metadata->typed_occurrences.occurrences[exposed_occurrence].wrapped_occurrence;
	}
	ctx->metadata->labels[id].expectation_classifier = expectation_classifier;
	ctx->metadata->labels[id].body_classifier =
		ctx->metadata->labels[id].body_occurrence < ctx->metadata->typed_occurrences.occurrence_count ?
			ctx->metadata->typed_occurrences.occurrences[
				ctx->metadata->labels[id].body_occurrence
			].classifier : PROTOTYPE_INVALID_ID;
	if (prototype_term_canonical_key(
			ctx->terms,
			term,
			&ctx->metadata->labels[id].canonical_key
		) != 0) {
		fprintf(stderr,
			"compile label canonical key generation failed: term=%u "
			"classifier=%u operation=%u\n",
			term, exposed_classifier, exposed_occurrence);
		return -1;
	}
	ctx->metadata->label_count++;
	return 0;
}

static int add_compile_type_export(
	struct compile_context* ctx,
	uint32_t type_id
) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	if (!ctx->type_declarations || type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return -1;
	}

	const struct prototype_type_declaration* type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (reserve_slot(
			ctx->metadata->type_export_count,
			ctx->metadata->type_export_capacity
		) != 0) {
		return -1;
	}
	if (ctx->metadata->constructor_export_count + type->constructor_count >
		ctx->metadata->constructor_export_capacity) {
		return -1;
	}

	uint32_t export_id = (uint32_t)ctx->metadata->type_export_count;
	struct prototype_compile_type_export* type_export =
		&ctx->metadata->type_exports[export_id];
	type_export->name_symbol_id = type->name_symbol_id;
	type_export->type_id = type_id;
	type_export->first_constructor_export =
		(uint32_t)ctx->metadata->constructor_export_count;
	type_export->constructor_count = type->constructor_count;
	struct prototype_context_classifier_view type_context =
		operation_type_context_view(ctx);
	if (prototype_type_declaration_representation_fingerprint_in_context(
			ctx->terms,
			ctx->type_declarations,
			&type_context,
			type_id,
			&type_export->representation_fingerprint
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		uint32_t constructor_id = type->first_constructor + i;
		const struct prototype_type_constructor_declaration* constructor =
			&ctx->type_declarations->semantic_schema.constructor_declarations[constructor_id];
		const struct prototype_type_constructor_readback* readback =
			prototype_type_constructor_readback_get(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->readback, constructor_id
			);
		const struct prototype_constructor_classifier_cache_entry* cache =
			prototype_type_constructor_classifier_cache_get(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->constructor_classifier_cache, constructor_id
			);
		if (!readback || !cache) {
			return -1;
		}
		struct prototype_compile_constructor_export* constructor_export =
			&ctx->metadata->constructor_exports[ctx->metadata->constructor_export_count++];
		constructor_export->type_export_index = export_id;
		constructor_export->name_symbol_id = constructor->name_symbol_id;
		constructor_export->ordinal = constructor->constructor_index;
		constructor_export->readback_first_field_type = readback->first_field_type;
		constructor_export->readback_field_count = readback->field_count;
		constructor_export->constructor_classifier = cache->classifier;
	}

	ctx->metadata->type_export_count++;
	return 0;
}

static int add_resolve_error(
	struct compile_context* ctx,
	int kind,
	int name_symbol_id,
	int member_symbol_id,
	uint32_t ast
) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	ctx->had_error = 1;
	if (reserve_slot(ctx->metadata->resolve_error_count, ctx->metadata->resolve_error_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)ctx->metadata->resolve_error_count;
	ctx->metadata->resolve_errors[id].kind = kind;
	ctx->metadata->resolve_errors[id].name_symbol_id = name_symbol_id;
	ctx->metadata->resolve_errors[id].member_symbol_id = member_symbol_id;
	ctx->metadata->resolve_errors[id].ast = ast;
	if (ast < ctx->asts->node_count) {
		ctx->metadata->resolve_errors[id].span = ctx->asts->nodes[ast].span;
	}
	ctx->metadata->resolve_error_count++;
	return 0;
}

static int add_resolve_error_at_span(
	struct compile_context* ctx,
	int kind,
	int name_symbol_id,
	int member_symbol_id,
	uint32_t ast,
	struct prototype_source_span span
) {
	size_t previous_count = ctx && ctx->metadata ? ctx->metadata->resolve_error_count : 0;
	int status = add_resolve_error(ctx, kind, name_symbol_id, member_symbol_id, ast);
	if (status == 0 && ctx && ctx->metadata && ctx->metadata->resolve_error_count > previous_count) {
		ctx->metadata->resolve_errors[ctx->metadata->resolve_error_count - 1].span = span;
	}
	return status;
}

static int add_compile_diagnostic(
	struct compile_context* ctx,
	int phase,
	int reason,
	uint32_t ast,
	uint32_t operation,
	uint32_t constraint,
	uint32_t expected_classifier,
	uint32_t actual_classifier,
	struct prototype_source_span span
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	ctx->had_error = 1;
	return prototype_compile_metadata_add_diagnostic(
		ctx->metadata,
		(struct prototype_compile_diagnostic) {
			.phase = phase,
			.reason = reason,
			.source_ast = ast,
			.span = span,
			.occurrence_id = operation,
			.constraint_id = constraint,
			.expected_classifier = expected_classifier,
			.actual_classifier = actual_classifier,
			.context_id = PROTOTYPE_INVALID_ID,
			.constructor_ordinal = PROTOTYPE_INVALID_ID,
			.field_ordinal = PROTOTYPE_INVALID_ID
		}
	);
}

static int add_constructor_compile_diagnostic(
	struct compile_context* ctx,
	uint32_t ast,
	uint32_t operation,
	uint32_t constraint,
	uint32_t expected_classifier,
	uint32_t actual_classifier,
	uint32_t context_id,
	uint32_t constructor_ordinal,
	uint32_t field_ordinal,
	struct prototype_source_span span
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	ctx->had_error = 1;
	return prototype_compile_metadata_add_diagnostic(
		ctx->metadata,
		(struct prototype_compile_diagnostic) {
			.phase = PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			.reason = PROTOTYPE_COMPILE_DIAGNOSTIC_CONSTRUCTOR_DOMAIN_MISMATCH,
			.source_ast = ast,
			.span = span,
			.occurrence_id = operation,
			.constraint_id = constraint,
			.expected_classifier = expected_classifier,
			.actual_classifier = actual_classifier,
			.context_id = context_id,
			.constructor_ordinal = constructor_ordinal,
			.field_ordinal = field_ordinal
		}
	);
}

static int begin_resolution_iteration(
	struct compile_context* ctx,
	uint32_t iteration,
	size_t unresolved_before
) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	if (reserve_slot(
		ctx->metadata->resolution_iteration_count,
		ctx->metadata->resolution_iteration_capacity
	) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)ctx->metadata->resolution_iteration_count;
	ctx->metadata->resolution_iterations[id].iteration = iteration;
	ctx->metadata->resolution_iterations[id].unresolved_before = unresolved_before;
	ctx->metadata->resolution_iterations[id].unresolved_after = unresolved_before;
	ctx->metadata->resolution_iterations[id].event_start =
		ctx->metadata->resolution_event_count;
	ctx->metadata->resolution_iterations[id].event_count = 0;
	ctx->metadata->resolution_iteration_count++;
	ctx->resolution_iteration = iteration;
	return 0;
}

static int finish_resolution_iteration(
	struct compile_context* ctx,
	size_t unresolved_after
) {
	if (!ctx || !ctx->metadata || ctx->metadata->resolution_iteration_count == 0) {
		return 0;
	}
	struct prototype_resolution_iteration* iteration =
		&ctx->metadata->resolution_iterations[ctx->metadata->resolution_iteration_count - 1];
	iteration->unresolved_after = unresolved_after;
	if (ctx->metadata->resolution_event_count >= iteration->event_start) {
		iteration->event_count =
			ctx->metadata->resolution_event_count - iteration->event_start;
	}
	return 0;
}

static int match_resolution_case(
	struct compile_context* ctx,
	const struct prototype_typing_match_resolution_goal* resolution,
	struct prototype_typed_occurrence_match_case** p_case
) {
	if (!ctx || !ctx->metadata || !resolution || !p_case ||
		resolution->match_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[
			resolution->match_operation
		];
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		resolution->case_index >= match->case_count ||
		match->first_case + resolution->case_index >=
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	*p_case = &ctx->metadata->typed_occurrences.cases[
		match->first_case + resolution->case_index
	];
	return 0;
}

static size_t count_unresolved_resolution_items(struct compile_context* ctx) {
	size_t unresolved = 0;
	if (!ctx || !ctx->typing || !ctx->metadata) {
		return 0;
	}
	for (uint32_t i = 0;
		i < ctx->typing->seed.match_resolution_goal_count;
		++i) {
		struct prototype_typed_occurrence_match_case* operation_case;
		if (match_resolution_case(
				ctx,
				&ctx->typing->seed.match_resolution_goals[i],
				&operation_case
			) != 0 || operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			unresolved++;
		}
	}
	return unresolved;
}

static int add_match_constructor_resolution_item(
	struct compile_context* ctx,
	uint32_t ast,
	uint32_t case_index,
	uint32_t scrutinee_term,
	int symbol_id,
	uint32_t* p_item_id
) {
	if (!ctx || !ctx->metadata || !p_item_id) {
		return -1;
	}
	if (reserve_slot(
		ctx->metadata->resolution_item_count,
		ctx->metadata->resolution_item_capacity
	) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)ctx->metadata->resolution_item_count;
	struct prototype_resolution_item* item = &ctx->metadata->resolution_items[id];
	memset(item, 0, sizeof(*item));
	item->id = id;
	item->kind = PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR;
	item->state = PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED;
	item->created_iteration = ctx->resolution_iteration;
	item->resolved_iteration = PROTOTYPE_INVALID_ID;
	item->ast = ast;
	item->match_term = PROTOTYPE_INVALID_ID;
	item->case_index = case_index;
	item->scrutinee_term = scrutinee_term;
	item->symbol_id = symbol_id;
	item->resolved_owner = PROTOTYPE_INVALID_ID;
	item->resolved_id = PROTOTYPE_INVALID_ID;
	ctx->metadata->resolution_item_count++;
	*p_item_id = id;
	return 0;
}

static int add_resolution_transition_event(
	struct compile_context* ctx,
	const struct prototype_resolution_item* item,
	int from_state,
	int to_state
) {
	if (!ctx || !ctx->metadata || !item) {
		return -1;
	}
	if (reserve_slot(
		ctx->metadata->resolution_event_count,
		ctx->metadata->resolution_event_capacity
	) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)ctx->metadata->resolution_event_count;
	struct prototype_resolution_event* event = &ctx->metadata->resolution_events[id];
	memset(event, 0, sizeof(*event));
	event->item_id = item->id;
	event->iteration = ctx->resolution_iteration;
	event->kind = item->kind;
	event->from_state = from_state;
	event->to_state = to_state;
	event->ast = item->ast;
	event->match_term = item->match_term;
	event->case_index = item->case_index;
	event->scrutinee_term = item->scrutinee_term;
	event->symbol_id = item->symbol_id;
	event->resolved_owner = item->resolved_owner;
	event->resolved_id = item->resolved_id;
	ctx->metadata->resolution_event_count++;
	return 0;
}

static int resolve_match_constructor_resolution_diagnostic(
	struct compile_context* ctx,
	const struct prototype_typing_match_resolution_goal* resolution,
	uint32_t match_term,
	uint32_t resolved_owner,
	uint32_t resolved_id
) {
	if (!ctx || !ctx->metadata || !resolution ||
		resolution->match_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t source_ast = ctx->metadata->typed_occurrences.occurrences[
		resolution->match_operation
	].source_ast;
	for (size_t i = ctx->metadata->resolution_item_count; i > 0; --i) {
		struct prototype_resolution_item* item =
			&ctx->metadata->resolution_items[i - 1];
		if (item->state != PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED ||
			item->kind != PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR ||
			item->ast != source_ast ||
			item->case_index != resolution->case_index ||
			item->symbol_id != resolution->constructor_symbol_id) {
			continue;
		}
		int previous_state = item->state;
		item->state = PROTOTYPE_RESOLUTION_ITEM_RESOLVED;
		item->resolved_iteration = ctx->resolution_iteration;
		item->match_term = match_term;
		item->resolved_owner = resolved_owner;
		item->resolved_id = resolved_id;
		return add_resolution_transition_event(
			ctx,
			item,
			previous_state,
			PROTOTYPE_RESOLUTION_ITEM_RESOLVED
		);
	}
	/* A loaded RECOMPUTE image deliberately has no diagnostic history. */
	return 0;
}

static int record_match_resolution_goal(
	struct compile_context* ctx,
	uint32_t match_term,
	uint32_t match_operation,
	uint32_t scrutinee_operation,
	uint32_t case_index,
	uint32_t scrutinee_term,
	int constructor_symbol_id
) {
	if (!ctx ||
		!ctx->typing ||
		match_term >= ctx->terms->term_count ||
		scrutinee_term >= ctx->terms->term_count ||
		match_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		scrutinee_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->typing->seed.match_resolution_goal_count >=
			PROTOTYPE_TYPING_MATCH_RESOLUTION_GOAL_CAPACITY) {
		return -1;
	}

	uint32_t id = ctx->typing->seed.match_resolution_goal_count++;
	ctx->typing->seed.match_resolution_goals[id].match_term = match_term;
	ctx->typing->seed.match_resolution_goals[id].match_operation = match_operation;
	ctx->typing->seed.match_resolution_goals[id].scrutinee_operation = scrutinee_operation;
	ctx->typing->seed.match_resolution_goals[id].case_index = case_index;
	ctx->typing->seed.match_resolution_goals[id].scrutinee_term = scrutinee_term;
	ctx->typing->seed.match_resolution_goals[id].constructor_symbol_id = constructor_symbol_id;
	/* Resolution items are a diagnostic projection. Their IDs and transition
	 * history are intentionally absent from the replay seed. */
	uint32_t diagnostic_item;
	if (add_match_constructor_resolution_item(
			ctx,
			ctx->metadata->typed_occurrences.occurrences[
				match_operation
			].source_ast,
			case_index,
			scrutinee_term,
			constructor_symbol_id,
			&diagnostic_item
		) != 0) {
		ctx->typing->seed.match_resolution_goal_count--;
		return -1;
	}
	ctx->metadata->resolution_items[diagnostic_item].match_term = match_term;
	return 0;
}

static int record_match_goal(
	struct compile_context* ctx,
	uint32_t match_term,
	uint32_t operation,
	uint32_t universe_level_var
) {
	if (!ctx ||
		!ctx->typing ||
		match_term >= ctx->terms->term_count ||
		!ctx->metadata || operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->typing->seed.match_goal_count >=
			PROTOTYPE_TYPING_MATCH_GOAL_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.match_goal_count; ++i) {
		/* Distinct typed Match occurrences may intentionally share one erased
		 * Core term.  Motive constraints and IH edges belong to the Operation,
		 * so only repeated registration of that exact Operation is redundant. */
		if (ctx->typing->seed.match_goals[i].operation == operation) {
			return 0;
		}
	}

	uint32_t id = ctx->typing->seed.match_goal_count++;
	ctx->typing->seed.match_goals[id].match_term = match_term;
	ctx->typing->seed.match_goals[id].operation = operation;
	ctx->typing->seed.match_goals[id].universe_level_var = universe_level_var;
	return 0;
}

static int record_imported_constructor_goal(
	struct compile_context* ctx,
	uint32_t constructor_term,
	uint32_t owner,
	size_t imported_interface_index,
	uint32_t type_export_id,
	uint32_t constructor_export_id
) {
	if (!ctx || !ctx->typing ||
		imported_interface_index > UINT32_MAX ||
		imported_interface_index >= ctx->imported_interface_count ||
		!ctx->imported_interfaces[imported_interface_index] ||
		constructor_term >= ctx->terms->term_count ||
		owner >= ctx->terms->term_count ||
		type_export_id >= ctx->imported_interfaces[
			imported_interface_index
		]->type_export_count ||
		constructor_export_id >= ctx->imported_interfaces[
			imported_interface_index
		]->constructor_export_count ||
		ctx->typing->seed.imported_constructor_goal_count >=
			PROTOTYPE_TYPING_IMPORTED_CONSTRUCTOR_GOAL_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.imported_constructor_goal_count; ++i) {
		const struct prototype_typing_imported_constructor_goal* pending =
			&ctx->typing->seed.imported_constructor_goals[i];
		if (pending->constructor_term == constructor_term) {
			return 0;
		}
	}
	uint32_t id = ctx->typing->seed.imported_constructor_goal_count++;
	ctx->typing->seed.imported_constructor_goals[id].constructor_term = constructor_term;
	ctx->typing->seed.imported_constructor_goals[id].owner = owner;
	ctx->typing->seed.imported_constructor_goals[id].imported_interface_index =
		(uint32_t)imported_interface_index;
	ctx->typing->seed.imported_constructor_goals[id].type_export_id = type_export_id;
	ctx->typing->seed.imported_constructor_goals[id].constructor_export_id =
		constructor_export_id;
	return 0;
}

static int record_binder_input(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t binder_var,
	uint32_t classifier,
	uint32_t source_operation
) {
	if (!ctx ||
		!ctx->typing ||
		context_id >= ctx->metadata->contexts.context_count ||
		binder_var >= ctx->terms->term_count ||
		ctx->terms->terms[binder_var].tag != PROTOTYPE_TERM_VAR ||
		classifier >= ctx->terms->term_count ||
		(source_operation != PROTOTYPE_INVALID_ID &&
		 source_operation >= ctx->metadata->typed_occurrences.occurrence_count) ||
		ctx->typing->seed.binder_input_count >=
			PROTOTYPE_TYPING_BINDER_INPUT_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.binder_input_count; ++i) {
		const struct prototype_typing_binder_input* pending =
			&ctx->typing->seed.binder_inputs[i];
		if (pending->context_id == context_id &&
			pending->binder_var == binder_var &&
			pending->classifier == classifier &&
			pending->source_operation == source_operation) {
			return 0;
		}
	}
	uint32_t id = ctx->typing->seed.binder_input_count++;
	ctx->typing->seed.binder_inputs[id].context_id = context_id;
	ctx->typing->seed.binder_inputs[id].binder_var = binder_var;
	ctx->typing->seed.binder_inputs[id].classifier = classifier;
	ctx->typing->seed.binder_inputs[id].source_operation = source_operation;
	ctx->typing->seed.binder_inputs[id].classifier_rhs_root = PROTOTYPE_INVALID_ID;
	if (ctx->typing->bindings.index_valid) {
		if (operation_binder_classifier_owner_merge(
				ctx,
				context_id,
				ctx->terms->terms[binder_var].as.var.binding_id,
				classifier
			) < 0) {
			ctx->typing->seed.binder_input_count--;
			return -1;
		}
		ctx->typing->bindings.pending_assumption_count =
			ctx->typing->seed.binder_input_count;
	}
	return 0;
}

/* A Match field classifier is discovered by a Solver equation after the open
 * image boundary. Publish it to the existing Context equation; it is generated
 * progress and must never be appended to the immutable replay seed. */
static int publish_generated_binder_classifier(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t binder_var,
	uint32_t classifier
) {
	if (!ctx || !ctx->typing || !ctx->typing->bindings.index_valid ||
		context_id >= ctx->metadata->contexts.context_count ||
		binder_var >= ctx->terms->term_count ||
		ctx->terms->terms[binder_var].tag != PROTOTYPE_TERM_VAR ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	return operation_binder_classifier_owner_merge(
		ctx,
		context_id,
		ctx->terms->terms[binder_var].as.var.binding_id,
		classifier
	) < 0 ? -1 : 0;
}

/* Generated dependent schemas can project an accepted classifier directly
 * into a new Context. Reconstruct the applicable APP derivation before using
 * that classifier as a binder type. */
static int materialize_classifier_application_formation(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t classifier
) {
	if (!ctx || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t spine[64];
	uint32_t spine_count = 0;
	uint32_t current = classifier;
	while (current < ctx->terms->term_count &&
		ctx->terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (spine_count >= 64) {
			return -1;
		}
		spine[spine_count++] = current;
		current = ctx->terms->terms[current].as.app.function;
	}
	prototype_judgement_delta_set_context(&ctx->judgement_delta, context_id);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	uint32_t type_id;
	uint32_t head_argument_count;
	uint32_t inferred = PROTOTYPE_INVALID_ID;
	if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			current,
			&type_id,
			NULL,
			&head_argument_count
		) == 0) {
		if (type_id >= ctx->type_declarations->semantic_schema.type_count ||
			head_argument_count != 0) {
			return -1;
		}
		const struct prototype_type_declaration* declaration =
			&ctx->type_declarations->semantic_schema.type_declarations[type_id];
		if (declaration->formation_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_delta_record_type_formation(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				current,
				declaration->formation_classifier
			) != 0) {
			return -1;
		}
		inferred = declaration->formation_classifier;
	} else if (spine_count == 0) {
		return 0;
	}
	for (uint32_t reverse = spine_count; reverse > 0; --reverse) {
		const struct prototype_term* application =
			&ctx->terms->terms[spine[reverse - 1]];
		uint32_t argument_type_id;
		uint32_t argument_count;
		if (prototype_type_projection_instance_info(
				ctx->terms,
				&ctx->type_declarations->semantic_schema,
				application->as.app.argument,
				&argument_type_id,
				NULL,
				&argument_count
			) == 0) {
			if (materialize_classifier_application_formation(
					ctx, context_id, application->as.app.argument
				) != 0) {
				return -1;
			}
		}
		int status = prototype_judgement_delta_expand_static_family_app(
			&ctx->judgement_delta,
			ctx->terms,
			ctx->type_declarations,
			spine[reverse - 1],
			&inferred
		);
		if (status > 0) {
			status = prototype_judgement_delta_expand_app(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				spine[reverse - 1],
				&inferred
			);
		}
		if (status != 0) {
			if (getenv("A_PROGRAM_FORMATION_TRACE")) {
				const struct prototype_term* app =
					&ctx->terms->terms[spine[reverse - 1]];
				fprintf(
					stderr,
					"classifier formation failed context=%u subject=%u step=%u "
					"app=%u function=%u argument=%u status=%d\n",
					context_id,
					classifier,
					reverse,
					spine[reverse - 1],
					app->as.app.function,
					app->as.app.argument,
					status
				);
			}
			return status;
		}
	}
	if (getenv("A_PROGRAM_FORMATION_TRACE")) {
		uint32_t evidence_count = 0;
		for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
			const struct prototype_judgement_proposition* evidence =
				&ctx->judgement_delta.propositions[i];
			if (evidence->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				evidence->subject == classifier) {
				evidence_count++;
			}
		}
		fprintf(
			stderr,
			"classifier formation context=%u subject=%u inferred=%u evidence=%u\n",
			context_id,
			classifier,
			inferred,
			evidence_count
		);
	}
	return inferred < ctx->terms->term_count &&
		ctx->terms->terms[inferred].tag == PROTOTYPE_TERM_UNIVERSE_VAR ? 0 : -1;
}

static int materialize_binder_inputs(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.binder_input_count; ++i) {
		const struct prototype_typing_binder_input* pending =
			&ctx->typing->seed.binder_inputs[i];
		/* Match pattern assumptions have their own rule and authority. Their
		 * pending entry exists only to resolve the Context classifier when the
		 * pattern variable is unused by the branch typed-occurrence graph. */
		if (pending->source_operation == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier = pending->classifier;
		uint32_t assumption_context_id;
		if (pending->binder_var >= ctx->terms->term_count ||
			ctx->terms->terms[pending->binder_var].tag != PROTOTYPE_TERM_VAR ||
			prototype_context_find_binding(
				&ctx->metadata->contexts,
				pending->context_id,
				ctx->terms->terms[pending->binder_var].as.var.binding_id,
				&assumption_context_id
			) != 0) {
			return -1;
		}
		uint32_t context_classifier = operation_context_classifier_term(
			ctx, assumption_context_id
		);
		if (context_classifier != PROTOTYPE_INVALID_ID) {
			classifier = context_classifier;
		}
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, pending->context_id
		);
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, PROTOTYPE_INVALID_ID
		);
		int already_materialized = 0;
		for (size_t relation_id = 0;
			relation_id < ctx->judgement_delta.proposition_count;
			++relation_id) {
			const struct prototype_judgement_proposition* relation =
				&ctx->judgement_delta.propositions[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->context_id != pending->context_id ||
				relation->subject != pending->binder_var ||
				relation->classifier != classifier ||
				prototype_judgement_candidate_find_derivation_kind(
					ctx->judgement_delta.propositions,
					ctx->judgement_delta.proposition_count,
					ctx->judgement_delta.derivation_candidates,
					ctx->judgement_delta.derivation_candidate_count,
					(uint32_t)relation_id,
					PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
					&(uint32_t){0}
				) != 0) {
				continue;
			}
			already_materialized = 1;
			break;
		}
		if (materialize_classifier_application_formation(
				ctx, pending->context_id, classifier
			) != 0) {
			return -1;
		}
		size_t before = ctx->judgement_delta.proposition_count;
		if (prototype_judgement_delta_record_context_binding_assumption(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->terms->terms[pending->binder_var].as.var.binding_id,
				classifier
			) != 0 ||
			ctx->judgement_delta.proposition_count == 0) {
			return -1;
		}
		if (ctx->judgement_delta.proposition_count == before &&
			already_materialized) {
			continue;
		}
	}
	return 0;
}

static int record_declaration_input(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t context_id;
	if (!ctx ||
		!ctx->typing ||
		ctx->binder_count > 512 ||
		subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count ||
		ctx->typing->seed.declaration_input_count >=
			PROTOTYPE_TYPING_DECLARATION_INPUT_CAPACITY) {
		return -1;
	}
	context_id = ctx->context_ids[ctx->binder_count];
	for (uint32_t i = 0; i < ctx->typing->seed.declaration_input_count; ++i) {
		const struct prototype_typing_declaration_input* pending =
			&ctx->typing->seed.declaration_inputs[i];
		if (pending->context_id == context_id &&
			pending->subject == subject &&
			pending->classifier == classifier) {
			return 0;
		}
	}
	uint32_t id = ctx->typing->seed.declaration_input_count++;
	ctx->typing->seed.declaration_inputs[id].context_id = context_id;
	ctx->typing->seed.declaration_inputs[id].subject = subject;
	ctx->typing->seed.declaration_inputs[id].classifier = classifier;
	return 0;
}

static int record_publication_input(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t exposed_occurrence,
	uint32_t expectation_occurrence,
	uint32_t expectation_classifier
) {
	if (!ctx || !ctx->typing || !ctx->metadata || name_symbol_id < 0 ||
		exposed_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		(expectation_occurrence != PROTOTYPE_INVALID_ID &&
		 expectation_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) ||
		(expectation_occurrence == PROTOTYPE_INVALID_ID) !=
			(expectation_classifier == PROTOTYPE_INVALID_ID) ||
		ctx->typing->seed.publication_input_count >=
			PROTOTYPE_TYPING_PUBLICATION_INPUT_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.publication_input_count; ++i) {
		const struct prototype_typing_publication_input* input =
			&ctx->typing->seed.publication_inputs[i];
		if (input->name_symbol_id != name_symbol_id) {
			continue;
		}
		return input->exposed_occurrence == exposed_occurrence &&
			input->expectation_occurrence == expectation_occurrence &&
			input->expectation_classifier == expectation_classifier ? 0 : -1;
	}
	uint32_t id = ctx->typing->seed.publication_input_count++;
	ctx->typing->seed.publication_inputs[id] =
		(struct prototype_typing_publication_input) {
			.name_symbol_id = name_symbol_id,
			.exposed_occurrence = exposed_occurrence,
			.expectation_occurrence = expectation_occurrence,
			.expectation_classifier = expectation_classifier
		};
	return 0;
}

static int classifier_contains_free_effect_row_variable(
	struct prototype_term_db* terms,
	uint32_t classifier
) {
	if (!terms || classifier >= terms->term_count) {
		return -1;
	}
	const uint32_t* variable_terms;
	size_t variable_count;
	if (prototype_term_effect_row_variable_terms(
			terms, &variable_terms, &variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < variable_count; ++i) {
		uint32_t variable = variable_terms[i];
		if (
			prototype_term_contains_free_binding(
				terms,
				classifier,
				terms->terms[variable].as.effect_row_var.binding_id
			)) {
			return 1;
		}
	}
	return 0;
}

static int materialize_declaration_inputs(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.declaration_input_count; ++i) {
		const struct prototype_typing_declaration_input* pending =
			&ctx->typing->seed.declaration_inputs[i];
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, pending->context_id
		);
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, PROTOTYPE_INVALID_ID
		);
		int has_free_row = classifier_contains_free_effect_row_variable(
			ctx->terms, pending->classifier
		);
		if (has_free_row < 0) {
			return -1;
		}
		if (has_free_row != 0) {
			continue;
		}
		if (prototype_judgement_delta_record_declaration_fact(
				&ctx->judgement_delta,
				ctx->terms,
				pending->subject,
				pending->classifier
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int resolve_unique_assignment_if_present(
	struct compile_context* ctx,
	int name_symbol_id,
	struct prototype_ast_term_assignment_def** p_def
) {
	if (!ctx || !p_def) {
		return -1;
	}
	*p_def = NULL;

	const struct prototype_ast_def_open_address_entry* entry =
		lookup_def_index_entry_const(ctx->asts, name_symbol_id);
	if (!entry || entry->assignment_count == 0) {
		return 1;
	}
	if (entry->assignment_count > 1) {
		return -1;
	}
	*p_def = lookup_unique_assignment_raw(ctx->asts, name_symbol_id);
	return *p_def ? 0 : -1;
}

static int push_graph_binder_with_origin(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t producer_occurrence,
	int source_classifier_authority,
	uint32_t* p_graph_binder_id
) {
	if (!ctx || !p_graph_binder_id || ctx->binder_count >= 512) {
		return -1;
	}

	/* Binding identity belongs to the source occurrence and must be globally
	 * fresh. TermDB performs alpha-aware interning independently, so reusing an
	 * ID by lexical depth only creates capture between open terms. */
	uint32_t graph_binder_id = prototype_term_new_binding(ctx->terms);
	if (graph_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_context = ctx->context_ids[ctx->binder_count];
	uint32_t context_id;
	uint32_t classifier_solution;
	if (operation_binder_classifier_meta_create(
			ctx, graph_binder_id, ast_binder_id, parent_context,
			source_classifier_authority, classifier,
			&classifier_solution
		) != 0) {
		return -1;
	}
	int extend_status = producer_occurrence == PROTOTYPE_INVALID_ID ?
		prototype_context_extend_equation(
			&ctx->metadata->contexts, parent_context, graph_binder_id,
			classifier_solution, &context_id
		) : prototype_context_extend_sequence_result_equation(
			&ctx->metadata->contexts, parent_context, graph_binder_id,
			classifier_solution, producer_occurrence,
			&context_id
		);
	if (extend_status != 0) {
		return -1;
	}
	if (prototype_context_classifier_equation_attach_context(
			&ctx->metadata->contexts, classifier_solution, context_id
		) != 0) {
		return -1;
	}
	ctx->binders[ctx->binder_count].ast_binder_id = ast_binder_id;
	ctx->binders[ctx->binder_count].graph_binder_id = graph_binder_id;
	ctx->binders[ctx->binder_count].classifier = classifier;
	ctx->binders[ctx->binder_count].symbol_id = symbol_id;
	ctx->binder_count++;
	ctx->context_ids[ctx->binder_count] = context_id;
	*p_graph_binder_id = graph_binder_id;
	return 0;
}

static int push_graph_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t* p_graph_binder_id
) {
	return push_graph_binder_with_origin(
		ctx, ast_binder_id, classifier, symbol_id,
		PROTOTYPE_INVALID_ID,
		classifier == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_INFERRED :
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED,
		p_graph_binder_id
	);
}

static int push_graph_computation_result_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t producer_occurrence,
	int source_classifier_authority,
	uint32_t* p_graph_binder_id
) {
	return push_graph_binder_with_origin(
		ctx, ast_binder_id, classifier, symbol_id,
		producer_occurrence, source_classifier_authority, p_graph_binder_id
	);
}

static int lookup_graph_binder_classifier(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (uint32_t i = ctx->binder_count; i > 0; --i) {
		const struct binder_map_entry* entry = &ctx->binders[i - 1];
		if (entry->ast_binder_id == ast_binder_id &&
			entry->classifier != PROTOTYPE_INVALID_ID) {
			*p_classifier = entry->classifier;
			return 0;
		}
	}
	return -1;
}

static int lookup_graph_binder_symbol(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	int* p_symbol_id
) {
	if (!ctx || !p_symbol_id) {
		return -1;
	}
	for (uint32_t i = ctx->binder_count; i > 0; --i) {
		const struct binder_map_entry* entry = &ctx->binders[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			*p_symbol_id = entry->symbol_id;
			return 0;
		}
	}
	return -1;
}

static const struct local_ref_map_entry* lookup_local_ref(
	const struct compile_context* ctx,
	uint32_t ast_binder_id
) {
	if (!ctx) {
		return NULL;
	}
	for (uint32_t i = ctx->local_ref_count; i > 0; --i) {
		const struct local_ref_map_entry* entry = &ctx->local_refs[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			return entry;
		}
	}
	return NULL;
}

static int push_local_ref(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	int symbol_id,
	const struct compile_ref* ref
) {
	if (!ctx || !ref || ctx->local_ref_count >= 512) {
		return -1;
	}
	struct local_ref_map_entry* entry = &ctx->local_refs[ctx->local_ref_count++];
	entry->ast_binder_id = ast_binder_id;
	entry->symbol_id = symbol_id;
	entry->ref = *ref;
	return 0;
}

static int compile_ast_level_var(
	struct compile_context* ctx,
	uint32_t ast_level_var,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret) {
		return -1;
	}

	for (uint32_t i = 0; i < ctx->level_count; ++i) {
		if (ctx->levels[i].ast_level_var == ast_level_var) {
			*p_ret = ctx->levels[i].graph_type_expr;
			return 0;
		}
	}
	if (ctx->level_count >= 512) {
		return -1;
	}

	uint32_t graph_type_expr;
	if (prototype_type_expr_fresh_universe(&ctx->type_declarations->readback, &graph_type_expr) != 0) {
		return -1;
	}
	ctx->levels[ctx->level_count].ast_level_var = ast_level_var;
	ctx->levels[ctx->level_count].graph_type_expr = graph_type_expr;
	ctx->level_count++;
	*p_ret = graph_type_expr;
	return 0;
}

static int compile_top_level_type_formation(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* formation,
	uint32_t* p_term
) {
	if (!ctx || !formation || !p_term || ctx->binder_count > 512) {
		return -1;
	}
	uint32_t saved_binder_count = ctx->binder_count;
	struct binder_map_entry saved_binders[512];
	uint32_t saved_context_ids[513];
	memcpy(
		saved_binders, ctx->binders,
		saved_binder_count * sizeof(saved_binders[0])
	);
	memcpy(
		saved_context_ids, ctx->context_ids,
		(saved_binder_count + 1) * sizeof(saved_context_ids[0])
	);
	ctx->binder_count = 0;
	int status = compile_def(ctx, formation, p_term);
	memcpy(
		ctx->binders, saved_binders,
		saved_binder_count * sizeof(saved_binders[0])
	);
	memcpy(
		ctx->context_ids, saved_context_ids,
		(saved_binder_count + 1) * sizeof(saved_context_ids[0])
	);
	ctx->binder_count = saved_binder_count;
	return status;
}

static int compile_type_expr_name_as_keyed_type_ref(
	struct compile_context* ctx,
	int symbol_id,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret) {
		return -1;
	}

	const struct prototype_type_declaration* type = NULL;
	if (ctx->asts) {
		for (uint32_t i = 0; i < (uint32_t)ctx->asts->type_def_count; ++i) {
			if (ctx->asts->type_defs[i].name_symbol_id != symbol_id) {
				continue;
			}
			struct prototype_typing_source_type_result* type_result =
				prototype_typing_source_type_result_mut(
					ctx->typing_source_results, i
				);
			if (!type_result) return -1;
			uint32_t type_id;
			if (type_result->state == PROTOTYPE_TYPING_SOURCE_CHECKING ||
				type_result->state == PROTOTYPE_TYPING_SOURCE_CHECKED) {
				type_id = type_result->type_declaration_id;
			} else {
				struct prototype_ast_term_assignment_def* formation =
					lookup_unique_assignment_raw(ctx->asts, symbol_id);
				uint32_t formation_term;
				if (!formation || formation->ast >= ctx->asts->node_count ||
					ctx->asts->nodes[formation->ast].tag !=
						PROTOTYPE_AST_TYPE_FORMATION ||
					ctx->asts->nodes[formation->ast].as.type_formation.ast_type_def_id != i) {
					return -1;
				}
				int formation_status = compile_top_level_type_formation(
					ctx, formation, &formation_term
				);
				if (formation_status != 0 ||
					type_result->state != PROTOTYPE_TYPING_SOURCE_CHECKED) {
					return -1;
				}
				type_id = type_result->type_declaration_id;
			}
			if (
				type_id >= ctx->type_declarations->semantic_schema.type_count) {
				return -1;
			}
			type = &ctx->type_declarations->semantic_schema.type_declarations[type_id];
			break;
		}
	}
	if (type) {
		return 1;
	}
	struct prototype_qualified_name imported_type_name;
	int imported_type_status = resolve_imported_type_name(
		ctx,
		symbol_id,
		&imported_type_name
	);
	if (imported_type_status < 0) {
		return -1;
	}
	if (imported_type_status == 0) {
		for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
			const struct prototype_artifact_interface* interface =
				ctx->imported_interfaces[i];
			uint32_t type_export_id;
			if (!interface || prototype_artifact_interface_find_type_export_in_namespace(
					interface,
					imported_type_name.namespace_symbol_id,
					imported_type_name.name_symbol_id,
					&type_export_id
				) != 0) {
				continue;
			}
			return prototype_type_expr_imported_type(&ctx->type_declarations->readback,
				imported_type_name,
				&interface->type_exports[type_export_id].representation_fingerprint,
				p_ret
			);
		}
		return -1;
	}
		struct prototype_qualified_name external_term_name;
		int external_term_status = resolve_imported_term_name(
			ctx,
			symbol_id,
			&external_term_name
		);
		if (external_term_status < 0) {
			return -1;
		}
		if (external_term_status == 0) {
			return prototype_type_expr_external_term(&ctx->type_declarations->readback,
				external_term_name,
				p_ret
			);
		}
	if (!type) {
		type = prototype_type_declaration_lookup(
			&ctx->type_declarations->semantic_schema, symbol_id);
	}
	if (type) {
		return 1;
	}

	struct prototype_ast_term_assignment_def* def;
	int status = resolve_unique_assignment_if_present(ctx, symbol_id, &def);
	if (status != 0) {
		return status < 0 ? -1 : 1;
	}

	uint32_t term;
	uint32_t evaluated;
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (compile_def(ctx, def, &term) != 0 ||
		reduce_type_namespace_term(ctx, term, &evaluated) != 0 ||
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			evaluated,
			&type_id,
			args,
			&arg_count
		) != 0 ||
		arg_count != 0 ||
		type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return 1;
	}

	const struct prototype_type_declaration* aliased_type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	return prototype_type_expr_name(&ctx->type_declarations->readback,
		aliased_type->name_symbol_id,
		p_ret
	);
}
static int compile_ast_static_endpoint(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t* p_ret
);
static int compile_ast_type_expr_term(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
);
static int compile_shared_app(
	struct compile_context* ctx,
	uint32_t function,
	uint32_t argument,
	uint32_t* p_ret
);
static int compile_extract_pure_return_value(
	struct compile_context* ctx,
	uint32_t computation,
	uint32_t* p_ret
);

static int compile_accepted_type_substitution_term(
	struct compile_context* ctx,
	const struct prototype_ast_type_expr* expr,
	uint32_t* p_term
) {
	if (!ctx || !expr || !p_term ||
		expr->tag != PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION ||
		expr->as.accepted_substitution.term >= ctx->terms->term_count ||
		expr->as.accepted_substitution.first_binding >
			ctx->asts->accepted_binding_substitution_count ||
		expr->as.accepted_substitution.binding_count >
			ctx->asts->accepted_binding_substitution_count -
				expr->as.accepted_substitution.first_binding ||
		expr->as.accepted_substitution.binding_count > 64) {
		return -1;
	}
	struct prototype_binding_replacement replacements[64];
	for (uint32_t i = 0; i < expr->as.accepted_substitution.binding_count; ++i) {
		const struct prototype_ast_accepted_binding_substitution* substitution =
			&ctx->asts->accepted_binding_substitutions[
				expr->as.accepted_substitution.first_binding + i
			];
		uint32_t target_value;
		if (compile_ast_static_endpoint(
				ctx, substitution->target_value, &target_value
			) != 0) {
			return -1;
		}
		replacements[i] = (struct prototype_binding_replacement) {
			.binding_id = substitution->source_binding_id,
			.replacement = target_value
		};
	}
	int status = prototype_term_graph_reindex_bindings(
		ctx->terms, expr->as.accepted_substitution.term,
		replacements,
		expr->as.accepted_substitution.binding_count,
		p_term
	);
	if (status != 0) {
		fprintf(
			stderr,
			"accepted type substitution failed term=%u tag=%d bindings=%u types=%zu\n",
			expr->as.accepted_substitution.term,
			ctx->terms->terms[expr->as.accepted_substitution.term].tag,
			expr->as.accepted_substitution.binding_count,
			ctx->type_declarations->semantic_schema.type_count
		);
	}
	return status;
}

static int compile_ast_static_endpoint_requires_replay(
	const struct compile_context* ctx,
	uint32_t ast_id
) {
	if (!ctx || !ctx->asts || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	switch (node->tag) {
	case PROTOTYPE_AST_NAME:
		if (prototype_type_declaration_lookup(
				&ctx->type_declarations->semantic_schema,
				node->as.name.symbol_id
			)) {
			return 0;
		}
		return lookup_unique_assignment_raw(
			ctx->asts, node->as.name.symbol_id
		) ? 1 : 0;
	case PROTOTYPE_AST_APP: {
		int function = compile_ast_static_endpoint_requires_replay(
			ctx, node->as.app.function
		);
		int argument = compile_ast_static_endpoint_requires_replay(
			ctx, node->as.app.argument
		);
		return function < 0 || argument < 0 ? -1 : function || argument;
	}
	case PROTOTYPE_AST_QUOTE:
		return compile_ast_static_endpoint_requires_replay(
			ctx, node->as.unary.term
		);
	default:
		return 0;
	}
}

static int compile_ast_classifier_requires_replay(
	struct compile_context* ctx,
	uint32_t type_expr
) {
	if (!ctx || !ctx->asts || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expression =
		&ctx->asts->type_exprs[type_expr];
	switch (expression->tag) {
	case PROTOTYPE_AST_TYPE_EXPR_NAME:
		if (prototype_type_declaration_lookup(
				&ctx->type_declarations->semantic_schema,
				expression->as.name.symbol_id
			)) {
			return 0;
		}
		return lookup_unique_assignment_raw(
			ctx->asts, expression->as.name.symbol_id
		) ? 1 : 0;
	case PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE: {
		const struct prototype_function_graph_association* association =
			compile_function_graph_association(
				ctx,
				expression->as.function_graph_reference.owner_symbol_id
			);
		return association && !association->imported ? 1 : 0;
	}
	case PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION:
		return 1;
	case PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE:
		return compile_ast_static_endpoint_requires_replay(
			ctx, expression->as.value_reference.value
		);
	case PROTOTYPE_AST_TYPE_EXPR_APP: {
		int function = compile_ast_classifier_requires_replay(
			ctx, expression->as.app.function
		);
		int argument = compile_ast_classifier_requires_replay(
			ctx, expression->as.app.argument
		);
		return function < 0 || argument < 0 ? -1 : function || argument;
	}
	case PROTOTYPE_AST_TYPE_EXPR_TERMINATES:
		return compile_ast_static_endpoint_requires_replay(
			ctx, expression->as.terminates.computation
		);
	default:
		return 0;
	}
}

static int compile_context_ast_binder_term(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t ast_binder_id,
	uint32_t* p_term
) {
	if (!ctx || !ctx->metadata || !p_term) {
		return -1;
	}
	while (context_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context) {
			return -1;
		}
		const struct prototype_context_classifier_equation* equation =
			prototype_context_classifier_equation_get(
				&ctx->metadata->contexts,
				prototype_context_classifier_equation(context)
			);
		if (equation && equation->source_ast_binder_id == ast_binder_id) {
			return prototype_term_var(ctx->terms, context->binding_id, p_term);
		}
		context_id = context->parent;
	}
	return -1;
}

/* Reuse the Context-owned binding identity after the source binder stack has
 * left scope. This reconstructs no typing answer; it only projects the same
 * already-lowered Layer C variable used by the source classifier. */
static int compile_ast_type_expr_term_for_rhs(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t source_context,
	uint32_t* p_term
) {
	if (!ctx || !p_term || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expression =
		&ctx->asts->type_exprs[type_expr];
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_VAR) {
		return compile_context_ast_binder_term(
			ctx, source_context, expression->as.var.ast_binder_id, p_term
		);
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_APP) {
		uint32_t function;
		uint32_t argument;
		uint32_t application;
		if (compile_ast_type_expr_term_for_rhs(
				ctx, expression->as.app.function, source_context, &function
			) != 0 || compile_ast_type_expr_term_for_rhs(
				ctx, expression->as.app.argument, source_context, &argument
			) != 0 || compile_shared_app(
				ctx, function, argument, &application
			) != 0) {
			return -1;
		}
		return compile_extract_pure_return_value(ctx, application, p_term);
	}
	return compile_ast_type_expr_term(ctx, type_expr, p_term);
}

/* Build the Layer T recipe at the same time as the source endpoint Term. The
 * endpoint leaf records the selected source occurrence, so alpha-interned Core
 * aliases do not erase which top-level definition the classifier named. */
static int compile_ast_static_endpoint_rhs(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t source_context,
	uint32_t* p_term,
	uint32_t* p_rhs
) {
	if (!ctx || !p_term || !p_rhs || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_VAR) {
		if (compile_context_ast_binder_term(
				ctx, source_context, node->as.var.ast_binder_id, p_term
			) != 0) {
			return -1;
		}
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
			*p_term,
			NULL,
			0,
			p_rhs
		);
	}
	if (node->tag == PROTOTYPE_AST_NAME && !prototype_type_declaration_lookup(
			&ctx->type_declarations->semantic_schema, node->as.name.symbol_id
		)) {
		struct prototype_ast_term_assignment_def* def;
		if (resolve_unique_assignment_if_present(
				ctx, node->as.name.symbol_id, &def
			) != 0 || compile_def(ctx, def, p_term) != 0) {
			return -1;
		}
		uint32_t assignment_id = (uint32_t)(def - ctx->asts->assignments);
		const struct prototype_typing_source_assignment_result* result =
			prototype_typing_source_assignment_result_get(
				ctx->typing_source_results, assignment_id
			);
		if (!result || result->state != PROTOTYPE_TYPING_SOURCE_CHECKED ||
			result->occurrence >=
				PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY) {
			return -1;
		}
		const struct prototype_typing_equation_operand endpoint = {
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
			.target_kind =
				PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE,
			.target = result->occurrence,
			.qualifier = 0
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
			*p_term,
			&endpoint,
			1,
			p_rhs
		);
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		uint32_t function_term;
		uint32_t argument_term;
		uint32_t function_rhs;
		uint32_t argument_rhs;
		if (compile_ast_static_endpoint_rhs(
				ctx, node->as.app.function, source_context,
				&function_term, &function_rhs
			) != 0 || compile_ast_static_endpoint_rhs(
				ctx, node->as.app.argument, source_context,
				&argument_term, &argument_rhs
			) != 0 || prototype_term_app(
				ctx->terms, function_term, argument_term, p_term
			) != 0) {
			return -1;
		}
		const struct prototype_typing_equation_operand operands[] = {
			{
				.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
				.target = function_rhs,
				.qualifier = 0
			},
			{
				.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
				.target = argument_rhs,
				.qualifier = 1
			}
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_VALUE_APP,
			*p_term,
			operands,
			2,
			p_rhs
		);
	}
	if (node->tag == PROTOTYPE_AST_QUOTE) {
		uint32_t child_term;
		uint32_t child_rhs;
		if (compile_ast_static_endpoint_rhs(
				ctx, node->as.unary.term, source_context,
				&child_term, &child_rhs
			) != 0 || prototype_term_thunk(ctx->terms, child_term, p_term) != 0) {
			return -1;
		}
		const struct prototype_typing_equation_operand child = {
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
			.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
			.target = child_rhs,
			.qualifier = 0
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_THUNK,
			*p_term,
			&child,
			1,
			p_rhs
		);
	}
	if (compile_ast_static_endpoint(ctx, ast_id, p_term) != 0) {
		return -1;
	}
	return prototype_typing_classifier_rhs_intern(
		&ctx->typing->constraints,
		PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
		*p_term,
		NULL,
		0,
		p_rhs
	);
}

/* Return 1 when the type expression contains no static endpoint and therefore
 * needs no replay recipe. The initial fragment covers the endpoint-bearing
 * forms that can currently be produced by source and generated contracts. */
static int compile_ast_classifier_rhs(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t source_term,
	uint32_t source_context,
	uint32_t* p_rhs
) {
	if (!ctx || !p_rhs || type_expr >= ctx->asts->type_expr_count ||
		source_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expression =
		&ctx->asts->type_exprs[type_expr];
	if (getenv("A_PROGRAM_CLASSIFIER_RHS_TRACE")) {
		fprintf(
			stderr,
			"classifier-rhs-build type=%u tag=%d source=%u context=%u\n",
			type_expr,
			expression->tag,
			source_term,
			source_context
		);
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_NAME &&
		!prototype_type_declaration_lookup(
			&ctx->type_declarations->semantic_schema,
			expression->as.name.symbol_id
		)) {
		struct prototype_ast_term_assignment_def* def;
		int assignment_status = resolve_unique_assignment_if_present(
			ctx, expression->as.name.symbol_id, &def
		);
		if (assignment_status < 0) {
			return -1;
		}
		if (assignment_status == 0) {
			uint32_t assignment_id = (uint32_t)(def - ctx->asts->assignments);
			const struct prototype_typing_source_assignment_result* result =
				prototype_typing_source_assignment_result_get(
					ctx->typing_source_results, assignment_id
				);
			if (!result || result->state != PROTOTYPE_TYPING_SOURCE_CHECKED ||
				result->occurrence >=
					PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY) {
				return -1;
			}
			const struct prototype_typing_equation_operand endpoint = {
				.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE,
				.target = result->occurrence,
				.qualifier = 0
			};
			return prototype_typing_classifier_rhs_intern(
				&ctx->typing->constraints,
				PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
				source_term,
				&endpoint,
				1,
				p_rhs
			);
		}
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE) {
		const struct prototype_function_graph_association* association =
			compile_function_graph_association(
				ctx, expression->as.function_graph_reference.owner_symbol_id
			);
		if (!association) {
			return -1;
		}
		if (association->imported) {
			*p_rhs = PROTOTYPE_INVALID_ID;
			return 1;
		}
		if (association->graph_type_assignment_id >= ctx->asts->assignment_count) {
			return -1;
		}
		const struct prototype_typing_source_assignment_result* result =
			prototype_typing_source_assignment_result_get(
				ctx->typing_source_results,
				association->graph_type_assignment_id
			);
		if (!result || result->state != PROTOTYPE_TYPING_SOURCE_CHECKED ||
			result->occurrence >=
				PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY) {
			return -1;
		}
		const struct prototype_typing_equation_operand endpoint = {
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
			.target_kind =
				PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE,
			.target = result->occurrence,
			.qualifier = 0
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
			source_term,
			&endpoint,
			1,
			p_rhs
		);
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION) {
		if (expression->as.accepted_substitution.term >= ctx->terms->term_count ||
			expression->as.accepted_substitution.first_binding >
				ctx->asts->accepted_binding_substitution_count ||
			expression->as.accepted_substitution.binding_count >
				ctx->asts->accepted_binding_substitution_count -
					expression->as.accepted_substitution.first_binding ||
			expression->as.accepted_substitution.binding_count > 64) {
			return -1;
		}
		struct prototype_typing_equation_operand operands[64];
		for (uint32_t i = 0;
			i < expression->as.accepted_substitution.binding_count;
			++i) {
			const struct prototype_ast_accepted_binding_substitution* substitution =
				&ctx->asts->accepted_binding_substitutions[
					expression->as.accepted_substitution.first_binding + i
				];
			uint32_t target_term;
			uint32_t target_rhs;
			if (compile_ast_static_endpoint_rhs(
					ctx,
					substitution->target_value,
					source_context,
					&target_term,
					&target_rhs
				) != 0) {
				return -1;
			}
			operands[i] = (struct prototype_typing_equation_operand) {
				.role =
					PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SUBSTITUTION_TARGET,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
				.target = target_rhs,
				.qualifier = substitution->source_binding_id
			};
		}
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_ACCEPTED_SUBSTITUTION,
			expression->as.accepted_substitution.term,
			operands,
			expression->as.accepted_substitution.binding_count,
			p_rhs
		);
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE) {
		uint32_t rebuilt_source;
		int status = compile_ast_static_endpoint_rhs(
			ctx, expression->as.value_reference.value, source_context,
			&rebuilt_source, p_rhs
		);
		return status != 0 || rebuilt_source != source_term ? -1 : 0;
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_APP) {
		uint32_t function_term;
		uint32_t argument_term;
		uint32_t function_rhs = PROTOTYPE_INVALID_ID;
		uint32_t argument_rhs = PROTOTYPE_INVALID_ID;
		if (compile_ast_type_expr_term_for_rhs(
				ctx, expression->as.app.function, source_context, &function_term
			) != 0 || compile_ast_type_expr_term_for_rhs(
				ctx, expression->as.app.argument, source_context, &argument_term
			) != 0) {
			return -1;
		}
		int function_status = compile_ast_classifier_rhs(
			ctx, expression->as.app.function, function_term, source_context,
			&function_rhs
		);
		int argument_status = compile_ast_classifier_rhs(
			ctx, expression->as.app.argument, argument_term, source_context,
			&argument_rhs
		);
		if (function_status < 0 || argument_status < 0) {
			return -1;
		}
		if (function_status > 0 && argument_status > 0) {
			*p_rhs = PROTOTYPE_INVALID_ID;
			return 1;
		}
		if (function_status > 0) {
			if (prototype_typing_classifier_rhs_intern(
					&ctx->typing->constraints,
					PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
					function_term,
					NULL,
					0,
					&function_rhs
				) != 0) {
				return -1;
			}
		}
		if (argument_status > 0) {
			if (prototype_typing_classifier_rhs_intern(
					&ctx->typing->constraints,
					PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
					argument_term,
					NULL,
					0,
					&argument_rhs
				) != 0) {
				return -1;
			}
		}
		const struct prototype_typing_equation_operand operands[] = {
			{
				.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
				.target = function_rhs,
				.qualifier = 0
			},
			{
				.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
				.target_kind =
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
				.target = argument_rhs,
				.qualifier = 1
			}
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP,
			source_term,
			operands,
			2,
			p_rhs
		);
	}
	if (expression->tag == PROTOTYPE_AST_TYPE_EXPR_TERMINATES) {
		uint32_t computation;
		uint32_t child_rhs;
		uint32_t rebuilt_source;
		if (compile_ast_static_endpoint_rhs(
				ctx, expression->as.terminates.computation,
				source_context,
				&computation, &child_rhs
			) != 0 || prototype_term_terminates_type(
				ctx->terms, computation, &rebuilt_source
			) != 0 || rebuilt_source != source_term) {
			return -1;
		}
		const struct prototype_typing_equation_operand child = {
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
			.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
			.target = child_rhs,
			.qualifier = 0
		};
		return prototype_typing_classifier_rhs_intern(
			&ctx->typing->constraints,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES,
			source_term,
			&child,
			1,
			p_rhs
		);
	}
	*p_rhs = PROTOTYPE_INVALID_ID;
	return 1;
}

static int compile_ast_type_expr(struct compile_context* ctx, uint32_t type_expr, uint32_t* p_ret) {
	if (!ctx || !p_ret) {
		return -1;
	}
	if (type_expr == PROTOTYPE_INVALID_ID) {
		*p_ret = PROTOTYPE_INVALID_ID;
		return 0;
	}
	if (type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}

	for (uint32_t i = 0; i < ctx->type_expr_count; ++i) {
		if (ctx->type_exprs[i].ast_type_expr == type_expr) {
			*p_ret = ctx->type_exprs[i].graph_type_expr;
			return 0;
		}
	}

	const struct prototype_ast_type_expr expr = ctx->asts->type_exprs[type_expr];
	uint32_t compiled_type_expr;
	switch (expr.tag) {
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE:
			if (prototype_type_expr_universe(&ctx->type_declarations->readback, expr.as.universe.level, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR:
			if (compile_ast_level_var(ctx, expr.as.universe_var.level_var, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_SELF:
			if (prototype_type_expr_self(&ctx->type_declarations->readback, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_VAR: {
			uint32_t graph_binder_id;
			if (lookup_graph_binder(ctx, expr.as.var.ast_binder_id, &graph_binder_id) != 0) {
				return -1;
			}
			if (prototype_type_expr_var(&ctx->type_declarations->readback, graph_binder_id, expr.as.var.symbol_id, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME: {
				int status = compile_type_expr_name_as_keyed_type_ref(
					ctx,
				expr.as.name.symbol_id,
				&compiled_type_expr
				);
				if (status < 0) {
					return -1;
				}
			if (status > 0 &&
				prototype_type_expr_name(&ctx->type_declarations->readback,
					expr.as.name.symbol_id,
					&compiled_type_expr
				) != 0) {
				return -1;
				}
				break;
			}
				case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE: {
					if (prototype_type_expr_primitive(&ctx->type_declarations->readback,
							prototype_type_expr_tag_from_host_type(expr.as.host_type.host_type_id),
							&compiled_type_expr
						) != 0) {
						return -1;
					}
				break;
			}
			case PROTOTYPE_AST_TYPE_EXPR_APP: {
				uint32_t function;
			uint32_t argument;
			if (compile_ast_type_expr(ctx, expr.as.app.function, &function) != 0) {
				return -1;
			}
			if (compile_ast_type_expr(ctx, expr.as.app.argument, &argument) != 0) {
				return -1;
			}
			if (prototype_type_expr_app(&ctx->type_declarations->readback, function, argument, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE:
			if (prototype_type_expr_local_type_member(&ctx->type_declarations->readback,
					expr.as.name_in_namespace.namespace_symbol_id,
					expr.as.name_in_namespace.symbol_id,
					&compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE: {
			const struct prototype_function_graph_association* association =
				compile_function_graph_association(
					ctx, expr.as.function_graph_reference.owner_symbol_id
				);
			if (association && association->imported) {
				if (association->imported_interface_index >=
						ctx->imported_interface_count ||
					association->imported_graph_type_export_index ==
						PROTOTYPE_INVALID_ID) {
					return -1;
				}
				const struct prototype_artifact_interface* interface =
					ctx->imported_interfaces[
						association->imported_interface_index
					];
				if (!interface ||
					association->imported_graph_type_export_index >=
						interface->type_export_count) {
					return -1;
				}
				const struct prototype_artifact_type_export* graph_export =
					&interface->type_exports[
						association->imported_graph_type_export_index
					];
				if (prototype_type_expr_imported_type(&ctx->type_declarations->readback,
						qualified_name_make(
							graph_export->namespace_symbol_id,
							graph_export->name_symbol_id
						),
						&graph_export->representation_fingerprint,
						&compiled_type_expr
					) != 0) {
					return -1;
				}
				break;
			}
			uint32_t ignored_graph_term;
			if (!association || association->graph_symbol_id < 0 ||
				association->graph_type_assignment_id >= ctx->asts->assignment_count ||
				compile_def(
					ctx,
					&ctx->asts->assignments[association->graph_type_assignment_id],
					&ignored_graph_term
				) != 0 ||
				prototype_type_expr_name(&ctx->type_declarations->readback,
					association->graph_symbol_id,
					&compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION: {
			if (compile_accepted_type_substitution_term(
					ctx, &expr, &(uint32_t) { PROTOTYPE_INVALID_ID }
				) != 0 || prototype_type_expr_semantic_relation(&ctx->type_declarations->readback,
					&compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE:
			/* Readback schemas cannot own an AST endpoint. The semantic Term
			 * compiled below remains authoritative for this generated index. */
			if (prototype_type_expr_semantic_relation(&ctx->type_declarations->readback, &compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_ARROW: {
			uint32_t domain;
			uint32_t codomain;
			if (compile_ast_type_expr(ctx, expr.as.arrow.domain, &domain) != 0) {
				return -1;
			}
			if (compile_ast_type_expr(ctx, expr.as.arrow.codomain, &codomain) != 0) {
				return -1;
			}
			if (prototype_type_expr_arrow(&ctx->type_declarations->readback, domain, codomain, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_PI: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t graph_binder_id;
			uint32_t saved_binder_count = ctx->binder_count;
			if (compile_ast_type_expr(ctx, expr.as.pi.domain, &domain) != 0 ||
				push_readback_binder(
					ctx,
					expr.as.pi.ast_binder_id,
					expr.as.pi.symbol_id,
					&graph_binder_id
				) != 0 ||
				compile_ast_type_expr(ctx, expr.as.pi.codomain, &codomain) != 0) {
				ctx->binder_count = saved_binder_count;
				return -1;
			}
			ctx->binder_count = saved_binder_count;
			if (prototype_type_expr_pi(&ctx->type_declarations->readback,
					graph_binder_id,
					expr.as.pi.symbol_id,
					domain,
					codomain,
					&compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE: {
			uint32_t result;
			if (compile_ast_type_expr(
					ctx, expr.as.computation_reference.result, &result
				) != 0 || prototype_type_expr_computation_reference(&ctx->type_declarations->readback, result, &compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_TERMINATES:
			/* Readback cannot own Term endpoints.  The graph-level classifier
			 * compiled below is the semantic schema authority. */
			if (prototype_type_expr_semantic_relation(&ctx->type_declarations->readback, &compiled_type_expr
				) != 0) {
				return -1;
			}
			break;
		default:
			return -1;
	}

	if (ctx->type_expr_count >= 1024) {
		return -1;
	}
	ctx->type_exprs[ctx->type_expr_count].ast_type_expr = type_expr;
	ctx->type_exprs[ctx->type_expr_count].graph_type_expr = compiled_type_expr;
	ctx->type_expr_count++;
	*p_ret = compiled_type_expr;
	return 0;
}

static int compile_type_declaration_term_by_symbol(
	struct compile_context* ctx,
	int symbol_id,
	uint32_t* p_ret
);
static int compile_ast_type_def(
	struct compile_context* ctx,
	uint32_t ast_type_def_id,
	uint32_t* p_type_id
);

static int compile_shared_app(
	struct compile_context* ctx,
	uint32_t function,
	uint32_t argument,
	uint32_t* p_ret
);

static int compile_extract_pure_return_value(
	struct compile_context* ctx,
	uint32_t computation,
	uint32_t* p_ret
) {
	uint64_t step_limit = compile_effort_remaining(ctx->metadata);
	int effort_status = compile_effort_charge(
		ctx->metadata, ctx->effort_atomic, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
	);
	if (effort_status != 0) return effort_status;
	int projection_status = prototype_term_project_structural_return_value(
		ctx->terms,
		computation,
		step_limit,
		p_ret
	);
	if (projection_status <= 0) {
		return projection_status;
	}
	/* A type-family application may reduce directly to a value rather than a
	 * CBPV RETURN. Preserve that existing representation after the shared
	 * computation projection has established that no RETURN tree is exposed. */
	return prototype_term_normalize_complete_with_profile(
		ctx->terms,
		NULL,
		PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
		computation,
		p_ret
	);
}

static int compile_ast_static_endpoint(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	switch (node->tag) {
		case PROTOTYPE_AST_INT_LITERAL:
			return prototype_term_int_literal(
				ctx->terms, node->as.int_literal.value, p_ret
			);
		case PROTOTYPE_AST_TEXT_LITERAL:
			return prototype_term_text_literal(
				ctx->terms, node->as.text_literal.text_symbol_id, p_ret
			);
		case PROTOTYPE_AST_VAR: {
			uint32_t binding_id;
			if (lookup_graph_binder(
					ctx, node->as.var.ast_binder_id, &binding_id
				) != 0) {
				return -1;
			}
			return prototype_term_var(ctx->terms, binding_id, p_ret);
		}
		case PROTOTYPE_AST_NAME: {
			if (prototype_type_declaration_lookup(
			&ctx->type_declarations->semantic_schema, node->as.name.symbol_id
				)) {
				return compile_type_declaration_term_by_symbol(
					ctx, node->as.name.symbol_id, p_ret
				);
			}
			struct prototype_ast_term_assignment_def* def;
			if (resolve_unique_assignment_if_present(
					ctx, node->as.name.symbol_id, &def
				) != 0) {
				return -1;
			}
			return compile_def(ctx, def, p_ret);
		}
		case PROTOTYPE_AST_NAME_IN_NAMESPACE: {
			uint32_t owner;
			if (compile_type_declaration_term_by_symbol(
					ctx, node->as.name_in_namespace.namespace_symbol_id, &owner
				) != 0) {
				return -1;
			}
			return resolve_namespace_member(
				ctx, owner, node->as.name_in_namespace.symbol_id, p_ret, NULL
			);
		}
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE: {
			uint32_t owner;
			if (compile_ast_static_endpoint(
					ctx, node->as.name_in_ast_namespace.namespace_ast, &owner
				) != 0) {
				return -1;
			}
			/* Generated index expressions build a parameterized namespace through
			 * ordinary APP nodes. Re-form the recognized type instance so the
			 * constructor owner uses the same canonical TypeView as accepted source
			 * constructor occurrences. */
			uint32_t type_id;
			uint32_t arguments[16];
			uint32_t argument_count;
			if (prototype_type_projection_instance_info(
					ctx->terms,
			&ctx->type_declarations->semantic_schema, owner, &type_id, arguments, &argument_count
				) == 0 && prototype_type_projection_instance_make(
					ctx->terms,
					ctx->type_declarations,
					type_id,
					arguments,
					argument_count,
					&owner
				) != 0) {
				return -1;
			}
			int status = resolve_namespace_member(
				ctx, owner, node->as.name_in_ast_namespace.symbol_id, p_ret, NULL
			);
			if (status == 0 && getenv("A_PROGRAM_STATIC_ENDPOINT_TRACE")) {
				fprintf(stderr,
					"static endpoint member ast=%u owner-ast=%u owner=%u member=%d result=%u\n",
					ast_id,
					node->as.name_in_ast_namespace.namespace_ast,
					owner,
					node->as.name_in_ast_namespace.symbol_id,
					*p_ret);
			}
			return status;
		}
		case PROTOTYPE_AST_APP: {
			uint32_t function;
			uint32_t argument;
			if (compile_ast_static_endpoint(
					ctx, node->as.app.function, &function
				) != 0 || compile_ast_static_endpoint(
					ctx, node->as.app.argument, &argument
				) != 0) {
				return -1;
			}
			int status = prototype_term_app(ctx->terms, function, argument, p_ret);
			if (status == 0 && getenv("A_PROGRAM_STATIC_ENDPOINT_TRACE")) {
				fprintf(stderr,
					"static endpoint app ast=%u function-ast=%u argument-ast=%u function=%u argument=%u result=%u\n",
					ast_id,
					node->as.app.function,
					node->as.app.argument,
					function,
					argument,
					*p_ret);
			}
			return status;
		}
		case PROTOTYPE_AST_QUOTE: {
			uint32_t computation;
			if (compile_ast_static_endpoint(
					ctx, node->as.unary.term, &computation
				) != 0) {
				return -1;
			}
			return prototype_term_thunk(ctx->terms, computation, p_ret);
		}
		default:
			/* Result evidence may construct ordinary APP and THUNK Core graphs from
			 * already synthesized names and binders. It must not lower an effectful
			 * source expression or create a post-fixed-point TypedOccurrence. */
			return -1;
	}
}

static int compile_ast_type_expr_term_with_self(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
);

static int ast_type_expr_returns_universe(
	const struct prototype_ast_db* asts,
	uint32_t type_expr
) {
	if (!asts || type_expr >= asts->type_expr_count) {
		return 0;
	}
	const struct prototype_ast_type_expr* expr = &asts->type_exprs[type_expr];
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI) {
		return ast_type_expr_returns_universe(asts, expr->as.pi.codomain);
	}
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return ast_type_expr_returns_universe(asts, expr->as.arrow.codomain);
	}
	return expr->tag == PROTOTYPE_AST_TYPE_EXPR_UNIVERSE ||
		expr->tag == PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR;
}

static int compile_ast_type_family_term_with_self(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr = &ctx->asts->type_exprs[type_expr];
	if (expr->tag != PROTOTYPE_AST_TYPE_EXPR_PI &&
		expr->tag != PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return compile_ast_type_expr_term_with_self(
			ctx, type_expr, self_type, p_ret
		);
	}
	uint32_t domain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.domain : expr->as.arrow.domain;
	uint32_t codomain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.codomain : expr->as.arrow.codomain;
	uint32_t domain;
	uint32_t codomain;
	uint32_t binding_id;
	uint32_t saved_binder_count = ctx->binder_count;
	if (compile_ast_type_expr_term_with_self(
			ctx, domain_expr, self_type, &domain
		) != 0) {
		return -1;
	}
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI) {
		if (push_graph_binder(
				ctx,
				expr->as.pi.ast_binder_id,
				domain,
				expr->as.pi.symbol_id,
				&binding_id
			) != 0) {
			return -1;
		}
	} else {
		binding_id = prototype_term_new_binding(ctx->terms);
		if (binding_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	if (compile_ast_type_family_term_with_self(
			ctx, codomain_expr, self_type, &codomain
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	uint32_t family;
	if (prototype_term_pure_family(
			ctx->terms, binding_id, codomain, &family
		) != 0) {
		return -1;
	}
	return prototype_term_pi_family(ctx->terms, domain, family, p_ret);
}

/* A function used as a value is thunked once at its outer boundary.  Its
 * curried result remains a raw negative computation, matching the classifier
 * synthesized for a nested source lambda. */
static int compile_ast_value_function_term_with_self(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr =
		&ctx->asts->type_exprs[type_expr];
	if (expr->tag != PROTOTYPE_AST_TYPE_EXPR_ARROW &&
		expr->tag != PROTOTYPE_AST_TYPE_EXPR_PI) {
		uint32_t result;
		uint32_t empty_effects;
		if (compile_ast_type_expr_term_with_self(
				ctx, type_expr, self_type, &result
			) != 0 || prototype_term_effect_row_empty(
				ctx->terms, &empty_effects
			) != 0) {
			return -1;
		}
		return prototype_term_total_computation_type(
			ctx->terms, empty_effects, result, p_ret
		);
	}

	uint32_t domain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.domain : expr->as.arrow.domain;
	uint32_t codomain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.codomain : expr->as.arrow.codomain;
	uint32_t domain;
	uint32_t codomain;
	uint32_t graph_binder_id = PROTOTYPE_INVALID_ID;
	uint32_t saved_binder_count = ctx->binder_count;
	if (compile_ast_type_expr_term_with_self(
			ctx, domain_expr, self_type, &domain
		) != 0) {
		return -1;
	}
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI && push_graph_binder(
			ctx,
			expr->as.pi.ast_binder_id,
			domain,
			expr->as.pi.symbol_id,
			&graph_binder_id
		) != 0) {
		return -1;
	}
	if (compile_ast_value_function_term_with_self(
			ctx, codomain_expr, self_type, &codomain
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return prototype_term_pi(ctx->terms, domain, codomain, p_ret);
	}
	uint32_t family;
	if (prototype_term_pure_family(
			ctx->terms, graph_binder_id, codomain, &family
		) != 0) {
		return -1;
	}
	return prototype_term_pi_family(ctx->terms, domain, family, p_ret);
}

static int compile_ast_type_expr_term_with_self(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}

	const struct prototype_ast_type_expr expr = ctx->asts->type_exprs[type_expr];
	switch (expr.tag) {
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE:
			return prototype_term_universe_var(ctx->terms, expr.as.universe.level, p_ret);
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR: {
			uint32_t graph_type_expr;
			if (compile_ast_level_var(
					ctx,
					expr.as.universe_var.level_var,
					&graph_type_expr
				) != 0 ||
				graph_type_expr >= ctx->type_declarations->readback.expr_count ||
				ctx->type_declarations->readback.exprs[graph_type_expr].tag !=
					PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR) {
				return -1;
			}
			return prototype_term_universe_var(
				ctx->terms,
				ctx->type_declarations->readback.exprs[graph_type_expr].as.universe_var.level_var,
				p_ret
			);
		}
		case PROTOTYPE_AST_TYPE_EXPR_SELF:
			if (self_type == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*p_ret = self_type;
			return 0;
		case PROTOTYPE_AST_TYPE_EXPR_VAR: {
			uint32_t graph_binder_id;
			if (lookup_graph_binder(ctx, expr.as.var.ast_binder_id, &graph_binder_id) != 0) {
				return -1;
			}
			return prototype_term_var(ctx->terms, graph_binder_id, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME: {
			struct prototype_ast_term_assignment_def* def;
			/* A type formation is also represented by a top-level assignment. During
			 * its constructor lowering, resolve the allocated type declaration before
			 * considering that assignment or compile_def would report false recursion. */
			if (compile_type_declaration_term_by_symbol(
					ctx, expr.as.name.symbol_id, p_ret
				) == 0) {
				return 0;
			}
			int status = resolve_unique_assignment_if_present(
				ctx, expr.as.name.symbol_id, &def
			);
			if (status == 1) {
				struct prototype_qualified_name imported_name;
				int imported_status = resolve_imported_term_name(
						ctx,
						expr.as.name.symbol_id,
						&imported_name
					);
					if (imported_status < 0) {
						return -1;
					}
					if (imported_status == 0) {
						return prototype_term_external_ref(ctx->terms, imported_name, p_ret);
					}
				return prototype_term_external_ref(
						ctx->terms,
						qualified_name_make(-1, expr.as.name.symbol_id),
						p_ret
					);
			}
			if (status != 0) {
				return -1;
			}
			return compile_def(ctx, def, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE: {
			uint32_t owner;
			if (compile_type_declaration_term_by_symbol(
					ctx,
					expr.as.name_in_namespace.namespace_symbol_id,
					&owner
				) != 0) {
				return -1;
			}
			return resolve_namespace_member(
				ctx,
				owner,
				expr.as.name_in_namespace.symbol_id,
				p_ret,
				NULL
			);
		}
		case PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE: {
			const struct prototype_function_graph_association* association =
				compile_function_graph_association(
					ctx, expr.as.function_graph_reference.owner_symbol_id
				);
			if (association && association->imported) {
				if (association->imported_interface_index >=
						ctx->imported_interface_count) {
					return -1;
				}
				const struct prototype_artifact_interface* interface =
					ctx->imported_interfaces[
						association->imported_interface_index
					];
				if (!interface ||
					association->imported_graph_type_export_index >=
						interface->type_export_count) {
					return -1;
				}
				return prototype_type_projection_instance_make(
					ctx->terms,
					ctx->type_declarations,
					interface->type_exports[
						association->imported_graph_type_export_index
					].local_type_id,
					NULL,
					0,
					p_ret
				);
			}
			if (!association || association->graph_type_assignment_id >=
					ctx->asts->assignment_count) {
				return -1;
			}
			if (association->graph_type_id == PROTOTYPE_INVALID_ID) {
				return compile_def(
					ctx,
					&ctx->asts->assignments[association->graph_type_assignment_id],
					p_ret
				);
			}
			return prototype_type_projection_instance_make(
				ctx->terms,
				ctx->type_declarations,
				association->graph_type_id,
				NULL,
				0,
				p_ret
			);
		}
		case PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION:
			return compile_accepted_type_substitution_term(ctx, &expr, p_ret);
		case PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE:
			return compile_ast_static_endpoint(
				ctx, expr.as.value_reference.value, p_ret
			);
				case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE: {
					return prototype_term_make_host_type(
						ctx->terms,
						expr.as.host_type.host_type_id,
						p_ret
					);
				}
		case PROTOTYPE_AST_TYPE_EXPR_APP: {
			uint32_t function;
			uint32_t argument;
			uint32_t application;
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.app.function, self_type, &function) != 0) {
				fprintf(stderr,
					"type APP function lowering failed expr=%u function=%u tag=%d\n",
					type_expr, expr.as.app.function,
					expr.as.app.function < ctx->asts->type_expr_count ?
						ctx->asts->type_exprs[expr.as.app.function].tag : -1);
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.app.argument, self_type, &argument) != 0) {
				fprintf(stderr,
					"type APP argument lowering failed expr=%u argument=%u tag=%d\n",
					type_expr, expr.as.app.argument,
					expr.as.app.argument < ctx->asts->type_expr_count ?
						ctx->asts->type_exprs[expr.as.app.argument].tag : -1);
				return -1;
			}
			if (compile_shared_app(ctx, function, argument, &application) != 0 ||
				compile_extract_pure_return_value(ctx, application, p_ret) != 0) {
				fprintf(stderr,
					"type APP application failed expr=%u function-term=%u argument-term=%u application=%u\n",
					type_expr, function, argument, application);
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_AST_TYPE_EXPR_ARROW: {
			if (ast_type_expr_returns_universe(ctx->asts, type_expr)) {
				return compile_ast_type_family_term_with_self(
					ctx, type_expr, self_type, p_ret
				);
			}
			uint32_t function;
			if (compile_ast_value_function_term_with_self(
					ctx, type_expr, self_type, &function
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, function, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_PI: {
			if (ast_type_expr_returns_universe(ctx->asts, type_expr)) {
				return compile_ast_type_family_term_with_self(
					ctx, type_expr, self_type, p_ret
				);
			}
			uint32_t function;
			if (compile_ast_value_function_term_with_self(
					ctx, type_expr, self_type, &function
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, function, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE: {
			uint32_t result;
			uint32_t row_binder;
			uint32_t row;
			uint32_t computation;
			if (compile_ast_type_expr_term_with_self(
					ctx,
					expr.as.computation_reference.result,
					self_type,
					&result
				) != 0 ||
				(row_binder = prototype_term_new_binding(ctx->terms)) ==
					PROTOTYPE_INVALID_ID ||
				prototype_term_effect_row_var(ctx->terms, row_binder, &row) != 0 ||
				prototype_term_total_computation_type(
					ctx->terms, row, result, &computation
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, computation, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_TERMINATES: {
			uint32_t computation;
			if (compile_ast_static_endpoint(
					ctx, expr.as.terminates.computation, &computation
				) != 0) {
				return -1;
			}
			return prototype_term_terminates_type(
				ctx->terms, computation, p_ret
			);
		}
		default:
			return -1;
	}
}

static int compile_ast_type_expr_term(struct compile_context* ctx, uint32_t type_expr, uint32_t* p_ret) {
	return compile_ast_type_expr_term_with_self(ctx, type_expr, PROTOTYPE_INVALID_ID, p_ret);
}

/* A type family is a raw Pi while it is formed, but a parameter that carries
 * that family is an ordinary suspended function value. */
static int compile_ast_binder_value_type_component(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr = &ctx->asts->type_exprs[type_expr];
	if ((expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI) &&
		ast_type_expr_returns_universe(ctx->asts, type_expr)) {
		uint32_t function;
		if (compile_ast_value_function_term_with_self(
				ctx, type_expr, PROTOTYPE_INVALID_ID, &function
			) != 0) {
			return -1;
		}
		return prototype_term_thunk_type(ctx->terms, function, p_ret);
	}
	return compile_ast_type_expr_term(ctx, type_expr, p_ret);
}

/* A source binder annotated with a suspended computation receives one
 * implicit, scoped row variable. The row is generalized only by the enclosing
 * lambda operation; it is never a runtime argument. */
static int compile_ast_binder_value_type_with_latent_effect_row(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_row_binder,
	uint32_t* p_ret
) {
	if (!ctx || !p_row_binder || !p_ret ||
		type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* function_type = &ctx->asts->type_exprs[type_expr];
	if (function_type->tag == PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE) {
		uint32_t result;
		uint32_t row_binder;
		uint32_t row;
		uint32_t computation;
		if (compile_ast_type_expr_term(
				ctx, function_type->as.computation_reference.result, &result
			) != 0 ||
			(row_binder = prototype_term_new_binding(ctx->terms)) ==
				PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_var(ctx->terms, row_binder, &row) != 0 ||
			prototype_term_total_computation_type(
				ctx->terms, row, result, &computation
			) != 0 ||
			prototype_term_thunk_type(ctx->terms, computation, p_ret) != 0) {
			return -1;
		}
		*p_row_binder = row_binder;
		return 0;
	}
	if (function_type->tag != PROTOTYPE_AST_TYPE_EXPR_ARROW &&
		function_type->tag != PROTOTYPE_AST_TYPE_EXPR_PI) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain;
	uint32_t graph_binder_id = PROTOTYPE_INVALID_ID;
	uint32_t saved_binder_count = ctx->binder_count;
	uint32_t row_binder;
	uint32_t row;
	uint32_t computation;
	uint32_t family;
	uint32_t function;
	uint32_t domain_expr = function_type->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		function_type->as.pi.domain : function_type->as.arrow.domain;
	uint32_t codomain_expr = function_type->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		function_type->as.pi.codomain : function_type->as.arrow.codomain;
	if (compile_ast_binder_value_type_component(
			ctx, domain_expr, &domain
		) != 0) {
		return -1;
	}
	if (function_type->tag == PROTOTYPE_AST_TYPE_EXPR_PI &&
		push_graph_binder(
			ctx,
			function_type->as.pi.ast_binder_id,
			domain,
			function_type->as.pi.symbol_id,
			&graph_binder_id
		) != 0) {
		return -1;
	}
	if (function_type->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW &&
		(graph_binder_id = prototype_term_new_binding(ctx->terms)) ==
			PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (compile_ast_binder_value_type_component(
			ctx, codomain_expr, &codomain
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (
		(row_binder = prototype_term_new_binding(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(ctx->terms, row_binder, &row) != 0 ||
		prototype_term_total_computation_type(ctx->terms, row, codomain, &computation) != 0 ||
		prototype_term_pure_family(
			ctx->terms, graph_binder_id, computation, &family
		) != 0 ||
		prototype_term_pi_family(ctx->terms, domain, family, &function) != 0 ||
		prototype_term_thunk_type(ctx->terms, function, p_ret) != 0) {
		return -1;
	}
	*p_row_binder = row_binder;
	return 0;
}

/* A top-level function declaration names a raw CBPV computation function.
 * Nested arrows remain value types, so a curried result is returned as a
 * thunked function value rather than being mistaken for an already-running
 * computation. */
static int compile_ast_function_result_type_expr_term(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr = &ctx->asts->type_exprs[type_expr];
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI) {
		return compile_ast_function_type_expr_term(ctx, type_expr, p_ret);
	}

	/* A non-arrow surface result is a value result of a computation.  Recursive
	 * arrows above remain negative computation types rather than becoming
	 * returned thunk values. */
	uint32_t result;
	uint32_t row_binder;
	uint32_t effect_row;
	if (compile_ast_type_expr_term(ctx, type_expr, &result) != 0 ||
		(row_binder = prototype_term_new_binding(ctx->terms)) ==
			PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(ctx->terms, row_binder, &effect_row) != 0 ||
		prototype_term_total_computation_type(
			ctx->terms, effect_row, result, p_ret
		) != 0) {
		return -1;
	}
	return 0;
}

static int compile_ast_function_type_expr_term(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr = &ctx->asts->type_exprs[type_expr];
	uint32_t domain;
	uint32_t codomain;
	if (expr->tag != PROTOTYPE_AST_TYPE_EXPR_ARROW &&
		expr->tag != PROTOTYPE_AST_TYPE_EXPR_PI) {
		return compile_ast_type_expr_term(ctx, type_expr, p_ret);
	}
	uint32_t domain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.domain : expr->as.arrow.domain;
	uint32_t codomain_expr = expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI ?
		expr->as.pi.codomain : expr->as.arrow.codomain;
	uint32_t graph_binder_id = PROTOTYPE_INVALID_ID;
	uint32_t latent_row_binder = PROTOTYPE_INVALID_ID;
	uint32_t saved_binder_count = ctx->binder_count;
	if (domain_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* domain_type =
		&ctx->asts->type_exprs[domain_expr];
	int domain_is_type_family =
		(domain_type->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 domain_type->tag == PROTOTYPE_AST_TYPE_EXPR_PI) &&
		ast_type_expr_returns_universe(ctx->asts, domain_expr);
	int domain_has_latent_effect =
		!domain_is_type_family &&
		(domain_type->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 domain_type->tag == PROTOTYPE_AST_TYPE_EXPR_PI ||
		 domain_type->tag == PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE);
	if ((domain_is_type_family ? compile_ast_binder_value_type_component(
			ctx, domain_expr, &domain
		) : domain_has_latent_effect ?
			compile_ast_binder_value_type_with_latent_effect_row(
				ctx, domain_expr, &latent_row_binder, &domain
			) : compile_ast_type_expr_term(
				ctx, domain_expr, &domain
			)) != 0) {
		return -1;
	}
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_PI &&
		push_graph_binder(
			ctx,
			expr->as.pi.ast_binder_id,
			domain,
			expr->as.pi.symbol_id,
			&graph_binder_id
		) != 0) {
		return -1;
	}
	if (compile_ast_function_result_type_expr_term(ctx, codomain_expr, &codomain) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	uint32_t function;
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		if (prototype_term_pi(ctx->terms, domain, codomain, &function) != 0) {
			return -1;
		}
	} else {
		uint32_t family;
		if (prototype_term_pure_family(
				ctx->terms, graph_binder_id, codomain, &family
			) != 0 || prototype_term_pi_family(
				ctx->terms, domain, family, &function
			) != 0) {
			return -1;
		}
	}
	if (latent_row_binder != PROTOTYPE_INVALID_ID &&
		prototype_term_effect_row_forall(
			ctx->terms, latent_row_binder, function, &function
		) != 0) {
		return -1;
	}
	*p_ret = function;
	return 0;
}

/* Surface arrows annotate raw computation functions. Every other surface type
 * expression denotes a value classifier at this elaboration boundary. This
 * decision belongs to the annotation syntax, never to the tag of the term
 * that happens to be checked against it. */
static int compile_ast_ascription_classifier(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || type_expr >= ctx->asts->type_expr_count) {
		return -1;
	}
	return (ctx->asts->type_exprs[type_expr].tag == PROTOTYPE_AST_TYPE_EXPR_ARROW ||
			ctx->asts->type_exprs[type_expr].tag == PROTOTYPE_AST_TYPE_EXPR_PI) ?
		compile_ast_function_type_expr_term(ctx, type_expr, p_ret) :
		compile_ast_type_expr_term(ctx, type_expr, p_ret);
}

static int compile_shared_app(
	struct compile_context* ctx,
	uint32_t function,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || function >= ctx->terms->term_count || argument >= ctx->terms->term_count) {
		return -1;
	}
	function = function;
	argument = argument;

	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_type_projection_instance_info(ctx->terms,
			&ctx->type_declarations->semantic_schema, function, &type_id, args, &arg_count) == 0) {
		if (type_id >= ctx->type_declarations->semantic_schema.type_count) {
			return -1;
		}
		const struct prototype_type_declaration* type = &ctx->type_declarations->semantic_schema.type_declarations[type_id];
		if (arg_count < type->parameter_count + type->index_count) {
			if (type->representation_id == PROTOTYPE_INVALID_ID) {
				return prototype_type_projection_source_instance_extend(
					ctx->terms,
					&ctx->type_declarations->semantic_schema,
					function,
					argument,
					p_ret
				);
			}
			return prototype_type_projection_instance_extend(
				ctx->terms,
				ctx->type_declarations,
				function,
				argument,
				p_ret
			);
		}
	}
	uint32_t app_term;
	if (prototype_term_app(ctx->terms, function, argument, &app_term) != 0) {
		return -1;
	}
	*p_ret = app_term;
	return 0;
}

struct operation_classifier_rhs_evaluator {
	uint8_t state[PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY];
	uint32_t term[PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY];
	void* endpoint_context;
	int (*resolve_endpoint)(
		void* context,
		uint32_t source_occurrence,
		uint32_t* p_term
	);
};

static int operation_evaluate_classifier_rhs(
	struct compile_context* ctx,
	uint32_t rhs_id,
	struct operation_classifier_rhs_evaluator* evaluator,
	uint32_t* p_term
) {
	if (!ctx || !evaluator || !p_term || !evaluator->resolve_endpoint ||
		rhs_id >= ctx->typing->constraints.classifier_rhs_count) {
		return -1;
	}
	if (evaluator->state[rhs_id] == 2) {
		*p_term = evaluator->term[rhs_id];
		return 0;
	}
	if (evaluator->state[rhs_id] == 1) {
		return -1;
	}
	const struct prototype_typing_classifier_rhs* rhs =
		prototype_typing_classifier_rhs_get(
			&ctx->typing->constraints, rhs_id
		);
	if (!rhs || rhs->source_term >= ctx->terms->term_count) {
		return -1;
	}
	evaluator->state[rhs_id] = 1;
	uint32_t result = rhs->source_term;
	uint32_t first;
	uint32_t second;
	int status = 0;
	switch (rhs->kind) {
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL:
		status = rhs->operand_count == 0 ? 0 : -1;
		break;
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT: {
		const struct prototype_typing_equation_operand* endpoint =
			prototype_typing_classifier_rhs_operand_get(
				&ctx->typing->constraints, rhs, 0
			);
		if (rhs->operand_count != 1 || !endpoint || endpoint->role !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT ||
			endpoint->target_kind !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE) {
			status = -1;
			break;
		}
		status = evaluator->resolve_endpoint(
			evaluator->endpoint_context, endpoint->target, &result
		);
		break;
	}
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_VALUE_APP:
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP: {
		const struct prototype_typing_equation_operand* function =
			prototype_typing_classifier_rhs_operand_get(
				&ctx->typing->constraints, rhs, 0
			);
		const struct prototype_typing_equation_operand* argument =
			prototype_typing_classifier_rhs_operand_get(
				&ctx->typing->constraints, rhs, 1
			);
		if (rhs->operand_count != 2 || !function || !argument ||
			function->role !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD ||
			argument->role !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD ||
			function->target_kind !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS ||
			argument->target_kind !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS ||
			function->qualifier != 0 || argument->qualifier != 1) {
			status = -1;
			break;
		}
		status = operation_evaluate_classifier_rhs(
			ctx, function->target, evaluator, &first
		);
		if (status != 0) {
			break;
		}
		status = operation_evaluate_classifier_rhs(
			ctx, argument->target, evaluator, &second
		);
		if (status != 0) {
			break;
		}
		if (rhs->kind == PROTOTYPE_TYPING_CLASSIFIER_RHS_VALUE_APP) {
			status = prototype_term_app(ctx->terms, first, second, &result);
			break;
		}
		uint32_t application;
		status = compile_shared_app(ctx, first, second, &application);
		if (status == 0) {
			status = compile_extract_pure_return_value(
				ctx, application, &result
			);
		}
		break;
	}
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_ACCEPTED_SUBSTITUTION: {
		if (rhs->operand_count > 64) {
			status = -1;
			break;
		}
		struct prototype_binding_replacement replacements[64];
		for (uint32_t i = 0; i < rhs->operand_count; ++i) {
			const struct prototype_typing_equation_operand* replacement =
				prototype_typing_classifier_rhs_operand_get(
					&ctx->typing->constraints, rhs, i
				);
			if (!replacement || replacement->role !=
					PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SUBSTITUTION_TARGET ||
				replacement->target_kind !=
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS) {
				status = -1;
				break;
			}
			replacements[i].binding_id = replacement->qualifier;
			status = operation_evaluate_classifier_rhs(
				ctx,
				replacement->target,
				evaluator,
				&replacements[i].replacement
			);
			if (status != 0) {
				break;
			}
		}
		if (status == 0) {
			status = prototype_term_graph_reindex_bindings(
				ctx->terms,
				rhs->source_term,
				replacements,
				rhs->operand_count,
				&result
			);
		}
		break;
	}
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_THUNK:
	case PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES: {
		const struct prototype_typing_equation_operand* child =
			prototype_typing_classifier_rhs_operand_get(
				&ctx->typing->constraints, rhs, 0
			);
		if (rhs->operand_count != 1 || !child || child->role !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD ||
			child->target_kind !=
				PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS) {
			status = -1;
			break;
		}
		status = operation_evaluate_classifier_rhs(
			ctx, child->target, evaluator, &first
		);
		if (status != 0) {
			break;
		}
		status = rhs->kind == PROTOTYPE_TYPING_CLASSIFIER_RHS_THUNK ?
			prototype_term_thunk(ctx->terms, first, &result) :
			prototype_term_terminates_type(ctx->terms, first, &result);
		break;
	}
	default:
		status = -1;
		break;
	}
	if (status != 0) {
		evaluator->state[rhs_id] = 0;
		return status;
	}
	evaluator->term[rhs_id] = result;
	evaluator->state[rhs_id] = 2;
	if (getenv("A_PROGRAM_CLASSIFIER_RHS_TRACE")) {
		fprintf(
			stderr,
			"classifier-rhs-result rhs=%u kind=%d source=%u result=%u\n",
			rhs_id,
			rhs->kind,
			rhs->source_term,
			result
		);
	}
	*p_term = result;
	return 0;
}

static int imported_type_parameter_binder(
	const uint32_t* source_binders,
	const uint32_t* target_binders,
	uint32_t binder_count,
	uint32_t source_binder,
	uint32_t* p_target_binder
) {
	if (!source_binders || !target_binders || !p_target_binder) {
		return -1;
	}
	for (uint32_t i = 0; i < binder_count; ++i) {
		if (source_binders[i] == source_binder) {
			*p_target_binder = target_binders[i];
			return 0;
		}
	}
	return -1;
}

static int compile_imported_type_expr_term(
	struct compile_context* ctx,
	const struct prototype_artifact_interface* interface,
	uint32_t type_expr,
	const uint32_t* source_binders,
	const uint32_t* target_binders,
	uint32_t binder_count,
	uint32_t* p_ret
) {
	if (!ctx || !interface || !source_binders || !target_binders || !p_ret ||
		type_expr >= interface->type_expr_count) {
		return -1;
	}
	const struct prototype_type_expr* expr = &interface->type_exprs[type_expr];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			return prototype_term_universe_var(
				ctx->terms,
				ctx->type_declarations->readback.next_level_var++,
				p_ret
			);
		case PROTOTYPE_TYPE_EXPR_VAR: {
			uint32_t target_binder;
			if (imported_type_parameter_binder(
					source_binders,
					target_binders,
					binder_count,
					expr->as.var.binding_id,
					&target_binder
				) != 0) {
				return -1;
			}
			return prototype_term_var(ctx->terms, target_binder, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_NAME:
			return compile_type_declaration_term_by_symbol(
				ctx, expr->as.name.symbol_id, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
			return prototype_term_external_ref(
				ctx->terms, expr->as.imported_type.name, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
			return prototype_term_external_ref(
				ctx->terms, expr->as.external_term.name, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_LOCAL_TYPE_MEMBER: {
			uint32_t owner;
			if (compile_type_declaration_term_by_symbol(
					ctx, expr->as.local_type_member.owner_symbol_id, &owner
				) != 0) {
				return -1;
			}
			return resolve_namespace_member(
				ctx,
				owner,
				expr->as.local_type_member.member_symbol_id,
				p_ret,
				NULL
			);
		}
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			return prototype_term_make_host_type(
				ctx->terms, PROTOTYPE_HOST_TYPE_TEXT, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			return prototype_term_make_host_type(
				ctx->terms, PROTOTYPE_HOST_TYPE_INT32, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
			return prototype_term_make_host_type(
				ctx->terms, PROTOTYPE_HOST_TYPE_INT64, p_ret
			);
		case PROTOTYPE_TYPE_EXPR_APP: {
			uint32_t function;
			uint32_t argument;
			if (compile_imported_type_expr_term(
					ctx,
					interface,
					expr->as.app.function,
					source_binders,
					target_binders,
					binder_count,
					&function
				) != 0 ||
				compile_imported_type_expr_term(
					ctx,
					interface,
					expr->as.app.argument,
					source_binders,
					target_binders,
					binder_count,
					&argument
				) != 0) {
				return -1;
			}
			return compile_shared_app(ctx, function, argument, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_ARROW: {
			uint32_t domain;
			uint32_t codomain;
			if (compile_imported_type_expr_term(
					ctx,
					interface,
					expr->as.arrow.domain,
					source_binders,
					target_binders,
					binder_count,
					&domain
				) != 0 ||
				compile_imported_type_expr_term(
					ctx,
					interface,
					expr->as.arrow.codomain,
					source_binders,
					target_binders,
					binder_count,
					&codomain
				) != 0) {
				return -1;
			}
			return prototype_term_pi(ctx->terms, domain, codomain, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_PI: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t target_binder = prototype_term_new_binding(ctx->terms);
			uint32_t family;
			uint32_t nested_source_binders[128];
			uint32_t nested_target_binders[128];
			if (binder_count >= 128 || target_binder == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			memcpy(
				nested_source_binders, source_binders,
				binder_count * sizeof(nested_source_binders[0])
			);
			memcpy(
				nested_target_binders, target_binders,
				binder_count * sizeof(nested_target_binders[0])
			);
			nested_source_binders[binder_count] = expr->as.pi.binding_id;
			nested_target_binders[binder_count] = target_binder;
			if (compile_imported_type_expr_term(
					ctx, interface, expr->as.pi.domain,
					source_binders, target_binders, binder_count, &domain
				) != 0 ||
				compile_imported_type_expr_term(
					ctx, interface, expr->as.pi.codomain,
					nested_source_binders, nested_target_binders,
					binder_count + 1, &codomain
				) != 0 ||
				prototype_term_pure_family(
					ctx->terms, target_binder, codomain, &family
				) != 0) {
				return -1;
			}
			return prototype_term_pi_family(ctx->terms, domain, family, p_ret);
		}
		default:
			return -1;
	}
}

static int imported_type_formation_classifier(
	struct compile_context* ctx,
	struct prototype_qualified_name name,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	const struct prototype_artifact_interface* interface = NULL;
	const struct prototype_artifact_type_export* type_export = NULL;
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* candidate =
			ctx->imported_interfaces[i];
		uint32_t export_id;
		if (!candidate) {
			continue;
		}
		int status = prototype_artifact_interface_find_type_export_in_namespace(
			candidate, name.namespace_symbol_id, name.name_symbol_id, &export_id
		);
		if (status < 0) {
			return -1;
		}
		if (status == 0) {
			if (interface) {
				return -1;
			}
			interface = candidate;
			type_export = &candidate->type_exports[export_id];
		}
	}
	if (!interface || !type_export) {
		return 1;
	}
	if (type_export->parameter_count > 16 ||
		type_export->first_parameter + type_export->parameter_count >
			interface->type_parameter_count) {
		return -1;
	}
	uint32_t source_binders[16];
	uint32_t target_binders[16];
	uint32_t domains[16];
	for (uint32_t i = 0; i < type_export->parameter_count; ++i) {
		const struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[type_export->first_parameter + i];
		if (parameter->type_expr >= interface->type_expr_count ||
			compile_imported_type_expr_term(
				ctx,
				interface,
				parameter->type_expr,
				source_binders,
				target_binders,
				i,
				&domains[i]
			) != 0) {
			return -1;
		}
		source_binders[i] = parameter->binding_id;
		target_binders[i] = prototype_term_new_binding(ctx->terms);
		if (target_binders[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			ctx->terms,
			ctx->type_declarations->readback.next_level_var++,
			&classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = type_export->parameter_count; i > 0; --i) {
		uint32_t codomain_family;
		if (prototype_term_pure_family(
				ctx->terms,
				target_binders[i - 1],
				classifier,
				&codomain_family
			) != 0 ||
			prototype_term_pi_family(
				ctx->terms,
				domains[i - 1],
				codomain_family,
				&classifier
			) != 0) {
			return -1;
		}
	}
	*p_classifier = classifier;
	return 0;
}

static int compile_type_declaration_term_by_symbol(
	struct compile_context* ctx,
	int symbol_id,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret) {
		return -1;
	}

	const struct prototype_type_declaration* type = NULL;
	if (ctx->asts) {
		for (uint32_t i = 0; i < (uint32_t)ctx->asts->type_def_count; ++i) {
			if (ctx->asts->type_defs[i].name_symbol_id != symbol_id) {
				continue;
			}
			struct prototype_typing_source_type_result* type_result =
				prototype_typing_source_type_result_mut(
					ctx->typing_source_results, i
				);
			if (!type_result) return -1;
			uint32_t type_id;
			if (type_result->state == PROTOTYPE_TYPING_SOURCE_CHECKING ||
				type_result->state == PROTOTYPE_TYPING_SOURCE_CHECKED) {
				type_id = type_result->type_declaration_id;
			} else {
				struct prototype_ast_term_assignment_def* formation =
					lookup_unique_assignment_raw(ctx->asts, symbol_id);
				uint32_t formation_term;
				if (!formation || formation->ast >= ctx->asts->node_count ||
					ctx->asts->nodes[formation->ast].tag !=
						PROTOTYPE_AST_TYPE_FORMATION ||
					ctx->asts->nodes[formation->ast].as.type_formation.ast_type_def_id != i) {
					return -1;
				}
				int formation_status = compile_top_level_type_formation(
					ctx, formation, &formation_term
				);
				if (formation_status != 0 ||
					type_result->state != PROTOTYPE_TYPING_SOURCE_CHECKED) {
					return -1;
				}
				type_id = type_result->type_declaration_id;
			}
			if (
				type_id >= ctx->type_declarations->semantic_schema.type_count) {
				return -1;
			}
			type = &ctx->type_declarations->semantic_schema.type_declarations[type_id];
			break;
		}
	if (!type) {
		struct prototype_qualified_name imported_name;
		int imported_status = resolve_imported_type_name(ctx, symbol_id, &imported_name);
		if (imported_status < 0) {
			return -1;
		}
		if (imported_status == 0) {
			return prototype_term_external_ref(ctx->terms, imported_name, p_ret);
		}
	}
	}
	if (!type) {
		type = prototype_type_declaration_lookup(
			&ctx->type_declarations->semantic_schema, symbol_id);
	}
	if (!type) {
		return -1;
	}
	if (type->representation_id == PROTOTYPE_INVALID_ID) {
		return prototype_type_projection_source_instance_make(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			type->type_index,
			NULL,
			0,
			p_ret
		);
	}
	return prototype_type_projection_instance_make(
		ctx->terms,
		ctx->type_declarations,
		type->type_index,
		NULL,
		0,
		p_ret
	);
}
static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
);
