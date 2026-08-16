#include "a_program/frontend/lowering.h"

#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/graph/occurrence_usage.h"
#include "a_program/kernel/resource_usage.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../internal/ast_common.h"
#include "../../artifact/artifact_internal.h"

static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
);

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

struct pending_match_resolution {
	uint32_t item_id;
	uint32_t match_term;
	uint32_t scrutinee_operation;
	uint32_t scrutinee_proven_classifier_hint;
	int constructor_symbol_id;
};

struct pending_match_typing {
	uint32_t match_term;
	uint32_t operation;
	uint32_t universe_level_var;
};

struct pending_ascription_check {
	uint32_t subject;
	uint32_t expected_classifier;
	uint32_t ast;
	uint32_t operation;
};

struct pending_imported_constructor_classifier {
	uint32_t constructor_term;
	uint32_t owner;
	const struct prototype_artifact_interface* interface;
	uint32_t type_export_id;
	uint32_t constructor_export_id;
};

struct pending_binder_assumption {
	uint32_t context_id;
	uint32_t binder_var;
	uint32_t classifier;
	uint32_t source_operation;
};

struct pending_declaration_fact {
	uint32_t context_id;
	uint32_t subject;
	uint32_t classifier;
};

/* This is lowering information, not a TermDB tag or a typing result.  It
 * preserves the CBPV shape of a computation while classifier constraints are
 * still unresolved.  The solver remains responsible for proving Pi/Comp. */
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
#define PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY 16384
#define PROTOTYPE_TYPED_OCCURRENCE_SOLVER_INPUT_FACT_CAPACITY 2048
#define PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_META_CAPACITY 4096
#define PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_SOLUTION_ATOM_CAPACITY 16384
#define PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY 131072

enum operation_classifier_goal_kind {
	OPERATION_CONSTRAINT_HAS_TYPE = 1,
	OPERATION_CONSTRAINT_EQUAL,
	OPERATION_CONSTRAINT_CONVERTIBLE,
	OPERATION_CONSTRAINT_PI_EXPECTED,
	OPERATION_CONSTRAINT_MOTIVE_EQUATION,
	OPERATION_CONSTRAINT_IH_EXPECTED,
	OPERATION_CONSTRAINT_CBPV_BOUNDARY,
	OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT,
	OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT,
	OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION,
	OPERATION_CONSTRAINT_BINDER_TYPE
};

enum operation_classifier_goal_state {
	OPERATION_CONSTRAINT_STATE_PENDING = 0,
	OPERATION_CONSTRAINT_STATE_SOLVED,
	OPERATION_CONSTRAINT_STATE_RESIDUAL,
	OPERATION_CONSTRAINT_STATE_CONTRADICTION,
	OPERATION_CONSTRAINT_STATE_INCOMPLETE
};

enum operation_classifier_goal_reason {
	OPERATION_CLASSIFIER_GOAL_REASON_NONE = 0,
	OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_REFERENCE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_COMPUTATION_FOLD_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_OPERATION_REQUEST_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_BINDER_TYPE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL,
	OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY,
	OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION,
	OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
};

enum operation_motive_solution_state {
	OPERATION_MOTIVE_SOLUTION_UNRESOLVED = 0,
	OPERATION_MOTIVE_SOLUTION_PROVISIONAL,
	OPERATION_MOTIVE_SOLUTION_READY,
	OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE,
	OPERATION_MOTIVE_SOLUTION_MATERIALIZED
};

enum operation_classifier_pi_goal_role {
	OPERATION_CLASSIFIER_PI_GOAL_INTRO = 1,
	OPERATION_CLASSIFIER_PI_GOAL_ELIM
};

struct operation_classifier_motive_case_goal {
	uint32_t branch_operation;
	uint32_t scrutinee_operation;
	uint32_t case_index;
	uint32_t constructor_owner;
	uint32_t constructor_index;
	uint32_t ih_scope_id;
};

union operation_classifier_goal_payload {
	struct {
		uint32_t referenced_operation;
	} reference;
	struct {
		uint32_t body_occurrence;
		uint32_t expected_classifier;
	} conversion;
	struct {
		int role;
		uint32_t body_or_function_operation;
		uint32_t domain_classifier_or_argument_occurrence;
	} pi;
	struct {
		uint32_t function_operation;
		uint32_t argument_occurrence;
	} constructor_formation;
	struct {
		uint32_t binding_context;
	} binder_type;
	struct operation_classifier_motive_case_goal motive_case;
	struct {
		uint32_t owner_match_occurrence;
		uint32_t recursive_argument_occurrence;
		uint32_t ih_scope_id;
		uint32_t case_index;
		uint32_t field_index;
	} induction_hypothesis;
	struct {
		uint32_t child_operation;
	} cbpv_boundary;
	struct {
		uint32_t first_operation;
		uint32_t second_operation;
		uint32_t third_operation;
	} computation;
};

struct operation_classifier_constraint {
	uint32_t id;
	int kind;
	uint32_t source_operation;
	uint32_t source_ast;
	uint32_t context_id;
	uint32_t classifier_variable;
	union operation_classifier_goal_payload payload;
};

struct operation_classifier_constraint_result {
	int state;
	int reason;
	struct prototype_kernel_conversion_goal conversion_goal;
};

/* The solver owns one result cell per typed source occurrence. TypedOccurrenceGraph
 * fields are checked projections of these cells, not a second source of solver
 * truth. */
struct operation_solver_solution {
	uint32_t classifier;
	uint32_t effect_row;
	uint32_t first_usage_entry;
	uint32_t usage_entry_count;
	int binder_usage;
	int classifier_state;
	int effect_state;
	int usage_state;
	int reason;
};

/*
 * Input facts belong to the solver invocation, not to JudgementDB. They are
 * declarations and binder assumptions collected while lowering. The same
 * facts become proof relations only after the solver has found classifiers
 * for the typed-occurrence graph.
 */
struct operation_solver_input_fact {
	uint32_t subject;
	uint32_t classifier;
	uint32_t lambda_operation;
	uint32_t ast_binder_id;
};

/*
 * A symbolic classifier M(argument) used by an IH before M has a TermDB
 * representation. `scope_frame` records the Match scope that owns the
 * expression, so a tagless VAR node cannot make an unrelated binder appear
 * recursive.
 */
struct operation_solver_motive_application {
	uint32_t motive_operation;
	uint32_t argument_occurrence;
	uint32_t scope_frame;
	uint32_t case_index;
	uint32_t field_index;
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
/*
 * This arena is intentionally separate from TermDB and JudgementDB. Entries in
 * bindings are resolved TermDB classifiers; INVALID means an unsolved type
 * metavariable identified by its operation id.
 */
struct operation_classifier_solver {
	int initialized;
	uint32_t initialized_occurrence_count;
	uint64_t solution_revision;
	struct operation_solver_solution solutions[4096];
	/* A candidate is a solver equation M(_) == T. It is never represented by
	 * a provisional TermDB APP node. */
	uint32_t motive_constant_candidates[4096];
	uint8_t motive_solution_states[4096];
	/* For an IH operation i, this indexes symbolic M(argument) in the solver
	 * arena. It is never a provisional TermDB APP node. */
	uint32_t ih_motive_application_ids[4096];
	struct operation_solver_motive_application motive_applications[4096];
	uint32_t motive_application_count;
	uint32_t motive_terms[4096];
	struct operation_classifier_constraint
		constraints[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY];
	struct operation_classifier_constraint_result
		constraint_results[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY];
	uint32_t constraint_count;
	uint32_t first_dependent_constraint[4096];
	struct {
		uint32_t constraint;
		uint32_t next;
	} dependent_constraints[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY * 4];
	uint32_t dependent_constraint_count;
	uint32_t worklist[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY];
	uint8_t constraint_queued[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY];
	uint32_t worklist_head;
	uint32_t worklist_count;
	struct operation_solver_input_fact
		input_facts[PROTOTYPE_TYPED_OCCURRENCE_SOLVER_INPUT_FACT_CAPACITY];
	uint32_t input_fact_count;
};

struct compile_context;

static int operation_solver_var_binding_context(
	const struct compile_context* ctx,
	uint32_t variable_occurrence,
	uint32_t* p_binding_context
);

enum operation_effect_row_meta_state {
	OPERATION_EFFECT_ROW_META_UNSOLVED = 0,
	OPERATION_EFFECT_ROW_META_SOLVED
};

struct compile_context;

static int operation_refine_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth,
	int* p_changed
);

static int operation_check_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth
);

/* Effect-row metavariables are owned by source occurrences. The placeholder
 * row is only a location in the current classifier graph; binder identity is
 * never the identity of the solver cell. */
struct operation_effect_row_meta {
	uint32_t owner_operation;
	uint32_t placeholder_row;
	uint32_t first_solution_atom;
	uint32_t solution_atom_count;
	uint32_t solution_row;
	int state;
};

struct operation_effect_solver {
	struct operation_effect_row_meta
		metas[PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_META_CAPACITY];
	uint32_t meta_count;
	uint32_t solution_atoms[
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_SOLUTION_ATOM_CAPACITY
	];
	uint32_t solution_atom_count;
	/* Equation records live in compile metadata for diagnostics/artifacts, but
	 * solver progress is separate so solving cannot mutate equation identity. */
	int constraint_states[PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY];
};

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
	struct prototype_judgement_effect_row_constraint
		effect_row_constraints[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_usage_entry
		operation_usage_entries[PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY];
};

#define OPERATION_REINDEX_CACHE_CAPACITY 16384
#define OPERATION_BRANCH_REFINEMENT_CAPACITY 4096

struct operation_reindex_cache_entry {
	uint32_t occurrence_id;
	uint32_t context_id;
	uint32_t term;
	uint32_t reindexed;
	uint64_t dependency_signature;
	int occupied;
};

struct operation_branch_refinement {
	int present;
	int status;
	uint32_t source_context;
	uint32_t refined_context;
	uint32_t substitution;
};

struct compile_context {
	const struct prototype_intrinsic_environment* intrinsic_environment;
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_judgement_delta judgement_delta;
	struct prototype_compile_metadata* metadata;
	struct operation_classifier_solver classifier_solver;
	/* Lowering facts are mutable compiler workspace and never frozen into a
	 * typed occurrence or artifact. */
	uint32_t classifier_hints[4096];
	struct operation_effect_solver effect_solver;
	int materializing_checked_classifiers;
	struct operation_reindex_cache_entry
		operation_reindex_cache[OPERATION_REINDEX_CACHE_CAPACITY];
	struct operation_branch_refinement branch_refinements[
		OPERATION_BRANCH_REFINEMENT_CAPACITY
	];
	struct prototype_occurrence_usage_solution
		operation_usage_solutions[PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY];
	struct prototype_usage_entry* operation_usage_entries;
	uint32_t operation_usage_entry_count;
	uint32_t operation_usage_entry_capacity;
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
	struct pending_match_resolution pending_match_resolutions[2048];
	uint32_t pending_match_resolution_count;
	struct pending_match_typing pending_match_typings[512];
	uint32_t pending_match_typing_count;
	struct pending_ascription_check pending_ascription_checks[1024];
	uint32_t pending_ascription_check_count;
	struct pending_imported_constructor_classifier
		pending_imported_constructor_classifiers[1024];
	uint32_t pending_imported_constructor_classifier_count;
	struct pending_binder_assumption pending_binder_assumptions[1024];
	uint32_t pending_binder_assumption_count;
	struct pending_declaration_fact pending_declaration_facts[1024];
	uint32_t pending_declaration_fact_count;
	uint32_t resolution_iteration;
	int namespace_symbol_id;
	const struct prototype_artifact_interface* const* imported_interfaces;
	size_t imported_interface_count;
	int had_error;
};

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
	return prototype_typed_occurrence_graph_child(
		graph, parent_occurrence, role, ordinal, p_child_occurrence
	);
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
static int operation_solver_refresh_computation_constraint_operands(
	struct compile_context* ctx
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
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
	if (graph_binder_id == PROTOTYPE_INVALID_ID ||
		prototype_context_extend_occurrence(
			&ctx->metadata->contexts,
			ctx->context_ids[ctx->binder_count],
			graph_binder_id,
			classifier,
			ast_binder_id,
			&context_id
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
		ctx->metadata->typed_occurrences.occurrences[inner_lambda_operation].binding_id =
			continuation_binder;
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
		ctx->metadata->typed_occurrences.occurrences[outer_lambda_operation].binding_id =
			argument_binder;
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
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
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
static int queue_declaration_fact(
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
	if (expectation->compiled) {
		*p_ret = expectation->compiled_classifier;
		return 0;
	}
	if (expectation->compiling) {
		return -1;
	}

	expectation->compiling = 1;
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
		expectation->compiling = 0;
		return -1;
	}
	expectation->compiled_classifier = classifier;
	expectation->compiled = 1;
	expectation->compiling = 0;
	*p_ret = classifier;
	return 0;
}

static int queue_declaration_fact(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
);
static int resolve_unique_assignment(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t ast,
	struct prototype_ast_term_assignment_def** p_def
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

static int lookup_source_expectation_classifier(
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
		if (expectation->kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION ||
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

static int build_imported_external_definition_env(
	struct compile_context* ctx,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	struct prototype_term_definition_env* p_env
) {
	if (!ctx || !definitions || !p_env) {
		return -1;
	}
	size_t definition_count = 0;
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		if (!interface) {
			continue;
		}
		for (size_t j = 0; j < interface->term_export_count; ++j) {
			const struct prototype_artifact_term_export* export =
				&interface->term_exports[j];
			if (export->transparency != PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT ||
				export->canonical_key.node_count == 0) {
				continue;
			}

			if (export->local_term >= ctx->terms->term_count) {
				continue;
			}
			if (definition_count >= definition_capacity) {
				return -1;
			}
			definitions[definition_count].name = qualified_name_make(
				export->namespace_symbol_id,
				export->name_symbol_id
			);
			definitions[definition_count].term = export->local_term;
			definitions[definition_count].classifier = PROTOTYPE_INVALID_ID;
			definitions[definition_count].transparency =
				PROTOTYPE_TERM_DEFINITION_TRANSPARENT;
			definitions[definition_count].canonical_key = export->canonical_key;
			definition_count++;
		}
	}
	p_env->definitions = definitions;
	p_env->definition_count = definition_count;
	return 0;
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
		queue_declaration_fact(ctx, term, classifier) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = has_classifier ? classifier : PROTOTYPE_INVALID_ID;
	/* A declaration-only or imported name has no TermDB tag from which to
	 * recover category.  Its declared classifier is already authoritative:
	 * a Pi denotes a raw computation function, while all other named terms
	 * begin as values until an expected computation boundary elaborates them. */
	p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (has_classifier) {
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, classifier, &view
			) != 0) {
			return -1;
		}
		if (!is_imported_type &&
			view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
			p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
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

static int collect_graph_classifiers(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!ctx || !classifiers || !p_classifier_count ||
		term >= ctx->terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* relations =
			source == 0 ? ctx->judgement_delta.propositions : ctx->judgement->propositions;
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? ctx->judgement_delta.derivation_candidates :
				ctx->judgement->derivation_candidates;
		size_t proposition_count =
			source == 0 ? ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
		size_t derivation_candidate_count = source == 0 ?
			ctx->judgement_delta.derivation_candidate_count :
			ctx->judgement->derivation_candidate_count;
		for (size_t i = 0; i < proposition_count; ++i) {
			const struct prototype_judgement_proposition* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != term ||
				prototype_judgement_candidate_find_derivation_other_than(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
					&(uint32_t){0}
				) != 0) {
				continue;
			}
			int contains = graph_classifier_list_contains_normalization_equal(
					ctx,
					classifiers,
					*p_classifier_count,
					relation->classifier
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				continue;
			}
			if (*p_classifier_count >= classifier_capacity) {
				return -1;
			}
			classifiers[(*p_classifier_count)++] = relation->classifier;
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
	ref->category = PROTOTYPE_TERM_CATEGORY_INVALID;
	ref->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
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
	*p_category = PROTOTYPE_TERM_CATEGORY_INVALID;
	*p_computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	*p_application_role = PROTOTYPE_TERM_APPLICATION_NONE;
	switch (tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_VAR:
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			*p_category = PROTOTYPE_TERM_CATEGORY_VALUE;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			*p_category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			*p_computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			*p_category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			*p_computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
			return;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			*p_category = constructor_spine ?
				PROTOTYPE_TERM_CATEGORY_VALUE : PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			*p_application_role = constructor_spine ?
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION :
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION;
			return;
		default:
			return;
	}
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
	}
	node.binder_classifier = binder_classifier;
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
	ctx->classifier_hints[*p_operation] = classifier;
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
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
	uint32_t solved = ctx->classifier_solver.solutions[occurrence_id].classifier;
	if (solved != PROTOTYPE_INVALID_ID) {
		return solved;
	}
	if (ctx->classifier_hints[occurrence_id] != PROTOTYPE_INVALID_ID) {
		return ctx->classifier_hints[occurrence_id];
	}
	return operation->classifier;
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
		ast_case->binder_count > 16 ||
		ast_case->first_binder + ast_case->binder_count >
			ctx->asts->case_binder_count) {
		return -1;
	}
	struct prototype_typed_occurrence_match_case match_case;
	memset(&match_case, 0, sizeof(match_case));
	match_case.context_id =
		ctx->metadata->typed_occurrences.occurrences[body_occurrence].context_id;
	match_case.has_refinement = 0;
	match_case.refinement_substitution = PROTOTYPE_INVALID_ID;
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

static int operation_apply_classifier_with_mode(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	int open_pure_type_family,
	uint32_t* p_classifier
) {
	uint32_t whnf;
	uint32_t specialized_function;
	if (!ctx || !p_classifier || function_classifier == PROTOTYPE_INVALID_ID ||
		argument_classifier == PROTOTYPE_INVALID_ID ||
		function_classifier >= ctx->terms->term_count ||
		argument_classifier >= ctx->terms->term_count || argument_term >= ctx->terms->term_count) {
		return 1;
	}
	/* Surface function application implicitly forces a suspended function value.
	 * Domain propagation already exposes this boundary; expose it here as well
	 * before specializing latent effect rows and applying the Pi classifier. */
	if (prototype_judgement_expose_callable_classifier(
			ctx->terms, ctx->type_declarations,
			function_classifier, &function_classifier
		) != 0) {
		return -1;
	}
	int specialization_status = prototype_judgement_specialize_effect_rows_for_argument(
		ctx->terms,
		ctx->type_declarations,
		function_classifier,
		argument_classifier,
		&specialized_function
	);
	if (specialization_status != 0) {
		return specialization_status;
	}
	function_classifier = specialized_function;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			function_classifier,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 1;
	}
	const struct prototype_term* pi = &ctx->terms->terms[whnf];
	uint32_t expected_domain;
	if (prototype_judgement_classifier_value_whnf(
			ctx->terms, ctx->type_declarations, pi->as.pi.domain, &expected_domain
		) != 0 || expected_domain >= ctx->terms->term_count) {
		return -1;
	}
	/* Type formation evaluates a pure family lambda as Core LAMBDA even though
	 * its surface value classifier is Thunk(Pi(...)). This is the static
	 * counterpart of compile_ref_open_pure_type_family: validate the exposed
	 * computation classifier without adding a runtime FORCE or changing Core. */
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
	uint32_t solved_expected_domain;
	int domain_solve_status = prototype_judgement_solve_expected_effect_rows(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		expected_domain,
		argument_classifier,
		&solved_expected_domain
	);
	if (domain_solve_status != 0) {
		return -1;
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
		ctx->terms,
		ctx->type_declarations,
		body,
		binding_id,
		classifier_argument,
		p_classifier
	);
}

static int operation_apply_classifier(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	return operation_apply_classifier_with_mode(
		ctx, function_classifier, argument_classifier, argument_term, 0,
		p_classifier
	);
}

static int operation_apply_pure_type_family_classifier(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	return operation_apply_classifier_with_mode(
		ctx, function_classifier, argument_classifier, argument_term, 1,
		p_classifier
	);
}

static int operation_apply_typed_occurrence_classifier(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (ctx->metadata->typed_occurrences.occurrences[occurrence_id].
		application_role ==
			PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) {
		return operation_apply_pure_type_family_classifier(
			ctx, function_classifier, argument_classifier, argument_term,
			p_classifier
		);
	}
	return operation_apply_classifier(
		ctx, function_classifier, argument_classifier, argument_term,
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
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
		ctx->terms, ctx->type_declarations, body, binding_id, argument_term, p_classifier
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
	if (prototype_term_type_instance_info(
			ctx->terms, term, &type_id, arguments, &argument_count
		) != 0 || type_id >= ctx->type_declarations->type_count ||
		argument_count > 16) {
		return 1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
	if (type->formation_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t classifier = type->formation_classifier;
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t whnf;
		if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
				ctx->terms,
				ctx->type_declarations,
				body,
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
			PROTOTYPE_TERM_CATEGORY_COMPUTATION : PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ref->computation_kind =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ?
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING :
			ctx->terms->terms[term].tag == PROTOTYPE_TERM_LAMBDA ?
				PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION :
				PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		if (prototype_judgement_pure_primitive_classifier(
				ctx->terms,
				ctx->type_declarations,
				&ctx->terms->terms[term],
				&p_ref->classifier
			) != 0) {
			return -1;
		}
	} else if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		if (prototype_judgement_effect_operation_classifier(
				ctx->terms, &ctx->terms->terms[term], &p_ref->classifier
			) != 0) {
			return -1;
		}
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
		int imported_status = imported_type_formation_classifier(
			ctx,
			ctx->terms->terms[term].as.external_ref.name,
			&p_ref->classifier
		);
		if (imported_status < 0) {
			return -1;
		}
		if (imported_status == 0 &&
			queue_declaration_fact(ctx, term, p_ref->classifier) != 0) {
			return -1;
		}
		if (imported_status > 0) {
			uint32_t classifiers[32];
			uint32_t classifier_count = 0;
			if (collect_graph_classifiers(
					ctx,
					term,
					classifiers,
					32,
					&classifier_count
				) != 0) {
				return -1;
			}
			if (classifier_count == 1) {
				p_ref->classifier = classifiers[0];
			}
		}
	} else {
		int type_status = type_instance_formation_classifier(
			ctx, term, &p_ref->classifier
		);
		if (type_status < 0) {
			return -1;
		}
		if (type_status > 0) {
			uint32_t classifiers[32];
			uint32_t classifier_count = 0;
			if (collect_graph_classifiers(
					ctx,
					term,
					classifiers,
					32,
					&classifier_count
				) != 0) {
				return -1;
			}
			if (classifier_count == 1) {
				p_ref->classifier = classifiers[0];
			}
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
	const struct pending_match_resolution* resolution,
	const struct prototype_resolution_item* item,
	uint32_t* p_scrutinee_classifier
) {
	if (!ctx || !resolution || !item || !p_scrutinee_classifier) {
		return -1;
	}
	if (resolution->scrutinee_operation < ctx->metadata->typed_occurrences.occurrence_count) {
		uint32_t classifier = operation_available_classifier(
			ctx, resolution->scrutinee_operation
		);
		if (classifier != PROTOTYPE_INVALID_ID) {
			*p_scrutinee_classifier = classifier;
			return 0;
		}
		/* An occurrence-local scrutinee must not borrow a classifier from an
		 * unrelated occurrence merely because both erase to the same core
		 * variable.  Wait until the operation solver has propagated the
		 * selected TypeView for this occurrence. */
		return 1;
	}
	if (resolution->scrutinee_proven_classifier_hint != PROTOTYPE_INVALID_ID) {
		*p_scrutinee_classifier = resolution->scrutinee_proven_classifier_hint;
		return 0;
	}
	uint32_t classifiers[32];
	uint32_t classifier_count = 0;
	if (collect_graph_classifiers(
			ctx,
			item->scrutinee_term,
			classifiers,
			32,
			&classifier_count
		) != 0) {
		return -1;
	}
	uint32_t selected_classifier = PROTOTYPE_INVALID_ID;
	uint32_t selected_owner = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < classifier_count; ++i) {
		struct prototype_match_constructor_resolution candidate;
		if (prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				classifiers[i],
				resolution->constructor_symbol_id,
				&candidate
			) != 0) {
			continue;
		}
		uint32_t case_id = resolution->match_term < ctx->terms->term_count ?
			ctx->terms->terms[resolution->match_term].as.match.first_case + item->case_index :
			PROTOTYPE_INVALID_ID;
		if (resolution->match_term >= ctx->terms->term_count ||
			ctx->terms->terms[resolution->match_term].tag != PROTOTYPE_TERM_MATCH ||
			case_id >= ctx->terms->case_count ||
			ctx->terms->cases[case_id].binder_count != candidate.field_count) {
			continue;
		}
		if (selected_classifier != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				selected_owner,
				candidate.constructor_owner
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		selected_classifier = classifiers[i];
		selected_owner = candidate.constructor_owner;
	}
	if (selected_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_scrutinee_classifier = selected_classifier;
	return 0;
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
	uint32_t exposed_occurrence
) {
	if (!ctx || !ctx->metadata) {
		return 0;
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
		ctx->metadata->labels[id].body_occurrence =
			ctx->metadata->typed_occurrences.occurrences[exposed_occurrence].wrapped_occurrence;
		ctx->metadata->labels[id].expectation_classifier =
			ctx->metadata->typed_occurrences.occurrences[exposed_occurrence].classifier;
	} else {
		ctx->metadata->labels[id].expectation_classifier = PROTOTYPE_INVALID_ID;
	}
	ctx->metadata->labels[id].body_classifier =
		ctx->metadata->labels[id].body_occurrence < ctx->metadata->typed_occurrences.occurrence_count ?
			ctx->metadata->typed_occurrences.occurrences[
				ctx->metadata->labels[id].body_occurrence
			].classifier : PROTOTYPE_INVALID_ID;
	ctx->metadata->labels[id].expectation_claim_id = PROTOTYPE_INVALID_ID;
	if (ctx->metadata->labels[id].expectation_classifier !=
			PROTOTYPE_INVALID_ID &&
		exposed_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		uint32_t context_id =
			ctx->metadata->typed_occurrences.occurrences[exposed_occurrence].context_id;
		for (uint32_t claim_id = 0;
			claim_id < (uint32_t)ctx->judgement->claim_count;
			++claim_id) {
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(ctx->judgement, claim_id);
			if (proposition && proposition->kind ==
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				proposition->authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
				proposition->authority_id == exposed_occurrence &&
				proposition->occurrence_id == exposed_occurrence &&
				proposition->context_id == context_id &&
				proposition->subject == term &&
				proposition->classifier ==
					ctx->metadata->labels[id].expectation_classifier) {
				ctx->metadata->labels[id].expectation_claim_id = claim_id;
				break;
			}
		}
		if (ctx->metadata->labels[id].expectation_claim_id ==
			PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"compile label is missing an ascription expectation claim: "
				"term=%u classifier=%u operation=%u context=%u\n",
				term, ctx->metadata->labels[id].expectation_classifier,
				exposed_occurrence, context_id);
			for (uint32_t proposition_id = 0;
				proposition_id < (uint32_t)ctx->judgement->proposition_count;
				++proposition_id) {
				const struct prototype_judgement_proposition* candidate =
					&ctx->judgement->propositions[proposition_id];
				if (candidate->occurrence_id == exposed_occurrence) {
					fprintf(stderr,
						"expectation operation proposition=%u subject=%u classifier=%u context=%u authority=%d:%u\n",
						proposition_id, candidate->subject, candidate->classifier,
						candidate->context_id, candidate->authority_kind,
						candidate->authority_id);
				}
			}
			return -1;
		}
	}
	if (prototype_term_canonical_key_with_types(
			ctx->terms,
			ctx->type_declarations,
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
	if (!ctx->type_declarations || type_id >= ctx->type_declarations->type_count) {
		return -1;
	}

	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
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
	if (prototype_type_declaration_representation_fingerprint(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			type_id,
			&type_export->representation_fingerprint
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&ctx->type_declarations->constructor_declarations[type->first_constructor + i];
		struct prototype_compile_constructor_export* constructor_export =
			&ctx->metadata->constructor_exports[ctx->metadata->constructor_export_count++];
		constructor_export->type_export_index = export_id;
		constructor_export->name_symbol_id = constructor->name_symbol_id;
		constructor_export->ordinal = constructor->constructor_index;
		constructor_export->readback_first_field_type = constructor->readback.first_field_type;
		constructor_export->readback_field_count = constructor->readback.field_count;
		constructor_export->curried_classifier_cache = constructor->curried_classifier_cache;
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
			.actual_classifier = actual_classifier
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

static size_t count_unresolved_resolution_items(struct compile_context* ctx) {
	size_t unresolved = 0;
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	for (size_t i = 0; i < ctx->metadata->resolution_item_count; ++i) {
		if (ctx->metadata->resolution_items[i].state ==
			PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED) {
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

static int resolve_match_constructor_resolution_item(
	struct compile_context* ctx,
	uint32_t item_id,
	uint32_t match_term,
	uint32_t resolved_owner,
	uint32_t resolved_id
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	if (item_id >= ctx->metadata->resolution_item_count) {
		return -1;
	}
	struct prototype_resolution_item* item = &ctx->metadata->resolution_items[item_id];
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

static int queue_match_constructor_resolution(
	struct compile_context* ctx,
	uint32_t item_id,
	uint32_t match_term,
	uint32_t scrutinee_operation,
	uint32_t scrutinee_proven_classifier_hint,
	int constructor_symbol_id
) {
	if (!ctx || item_id == PROTOTYPE_INVALID_ID ||
		match_term >= ctx->terms->term_count ||
		ctx->pending_match_resolution_count >= 2048) {
		return -1;
	}

	uint32_t id = ctx->pending_match_resolution_count++;
	ctx->pending_match_resolutions[id].item_id = item_id;
	ctx->pending_match_resolutions[id].match_term = match_term;
	ctx->pending_match_resolutions[id].scrutinee_operation = scrutinee_operation;
	ctx->pending_match_resolutions[id].scrutinee_proven_classifier_hint =
		scrutinee_proven_classifier_hint;
	ctx->pending_match_resolutions[id].constructor_symbol_id = constructor_symbol_id;
	return 0;
}

static int queue_match_typing(
	struct compile_context* ctx,
	uint32_t match_term,
	uint32_t operation,
	uint32_t universe_level_var
) {
	if (!ctx ||
		match_term >= ctx->terms->term_count ||
		!ctx->metadata || operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->pending_match_typing_count >= 512) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		/* Distinct typed Match occurrences may intentionally share one erased
		 * Core term.  Motive constraints and IH edges belong to the Operation,
		 * so only repeated registration of that exact Operation is redundant. */
		if (ctx->pending_match_typings[i].operation == operation) {
			return 0;
		}
	}

	uint32_t id = ctx->pending_match_typing_count++;
	ctx->pending_match_typings[id].match_term = match_term;
	ctx->pending_match_typings[id].operation = operation;
	ctx->pending_match_typings[id].universe_level_var = universe_level_var;
	return 0;
}

static int queue_ascription_check(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t ast,
	uint32_t operation
) {
	if (!ctx ||
		subject >= ctx->terms->term_count ||
		expected_classifier >= ctx->terms->term_count ||
		ctx->pending_ascription_check_count >= 1024) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_ascription_check_count; ++i) {
		const struct pending_ascription_check* check =
			&ctx->pending_ascription_checks[i];
		if (check->subject == subject &&
			check->expected_classifier == expected_classifier &&
			check->ast == ast && check->operation == operation) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_ascription_check_count++;
	ctx->pending_ascription_checks[id].subject = subject;
	ctx->pending_ascription_checks[id].expected_classifier = expected_classifier;
	ctx->pending_ascription_checks[id].ast = ast;
	ctx->pending_ascription_checks[id].operation = operation;
	return 0;
}

static int queue_imported_constructor_classifier(
	struct compile_context* ctx,
	uint32_t constructor_term,
	uint32_t owner,
	const struct prototype_artifact_interface* interface,
	uint32_t type_export_id,
	uint32_t constructor_export_id
) {
	if (!ctx || !interface ||
		constructor_term >= ctx->terms->term_count ||
		owner >= ctx->terms->term_count ||
		type_export_id >= interface->type_export_count ||
		constructor_export_id >= interface->constructor_export_count ||
		ctx->pending_imported_constructor_classifier_count >= 1024) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_imported_constructor_classifier_count; ++i) {
		const struct pending_imported_constructor_classifier* pending =
			&ctx->pending_imported_constructor_classifiers[i];
		if (pending->constructor_term == constructor_term) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_imported_constructor_classifier_count++;
	ctx->pending_imported_constructor_classifiers[id].constructor_term = constructor_term;
	ctx->pending_imported_constructor_classifiers[id].owner = owner;
	ctx->pending_imported_constructor_classifiers[id].interface = interface;
	ctx->pending_imported_constructor_classifiers[id].type_export_id = type_export_id;
	ctx->pending_imported_constructor_classifiers[id].constructor_export_id =
		constructor_export_id;
	return 0;
}

static int queue_binder_assumption(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t binder_var,
	uint32_t classifier,
	uint32_t source_operation
) {
	if (!ctx ||
		context_id >= ctx->metadata->contexts.context_count ||
		binder_var >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count ||
		(source_operation != PROTOTYPE_INVALID_ID &&
		 source_operation >= ctx->metadata->typed_occurrences.occurrence_count) ||
		ctx->pending_binder_assumption_count >= 1024) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
		if (pending->context_id == context_id &&
			pending->binder_var == binder_var &&
			pending->classifier == classifier &&
			pending->source_operation == source_operation) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_binder_assumption_count++;
	ctx->pending_binder_assumptions[id].context_id = context_id;
	ctx->pending_binder_assumptions[id].binder_var = binder_var;
	ctx->pending_binder_assumptions[id].classifier = classifier;
	ctx->pending_binder_assumptions[id].source_operation = source_operation;
	return 0;
}

static int materialize_pending_binder_assumptions(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
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
		const struct prototype_context* assumption = prototype_context_get(
			&ctx->metadata->contexts, assumption_context_id
		);
		if (prototype_context_classifier_term(assumption) !=
				PROTOTYPE_INVALID_ID) {
			classifier = prototype_context_classifier_term(assumption);
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
		if (already_materialized) {
			continue;
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
		if (ctx->judgement_delta.proposition_count == before) {
			continue;
		}
	}
	return 0;
}

static int queue_declaration_fact(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t context_id;
	if (!ctx ||
		ctx->binder_count > 512 ||
		subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count ||
		ctx->pending_declaration_fact_count >= 1024) {
		return -1;
	}
	context_id = ctx->context_ids[ctx->binder_count];
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
		if (pending->context_id == context_id &&
			pending->subject == subject &&
			pending->classifier == classifier) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_declaration_fact_count++;
	ctx->pending_declaration_facts[id].context_id = context_id;
	ctx->pending_declaration_facts[id].subject = subject;
	ctx->pending_declaration_facts[id].classifier = classifier;
	return 0;
}

static int classifier_contains_free_effect_row_variable(
	const struct prototype_term_db* terms,
	uint32_t classifier
) {
	if (!terms || classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
			prototype_term_contains_free_binding(
				terms,
				classifier,
				terms->terms[i].as.effect_row_var.binding_id
			)) {
			return 1;
		}
	}
	return 0;
}

static int materialize_pending_declaration_facts(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
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

static int resolve_unique_assignment(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t ast,
	struct prototype_ast_term_assignment_def** p_def
) {
	if (!ctx || !p_def) {
		return -1;
	}
	*p_def = NULL;

	const struct prototype_ast_def_open_address_entry* entry =
		lookup_def_index_entry_const(ctx->asts, name_symbol_id);
	if (!entry || entry->assignment_count == 0) {
		(void)add_resolve_error(
			ctx,
			PROTOTYPE_RESOLVE_ERROR_NAME,
			name_symbol_id,
			-1,
			ast
		);
		return -1;
	}
	if (entry->assignment_count > 1) {
		(void)add_resolve_error(
			ctx,
			PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT,
			name_symbol_id,
			-1,
			ast
		);
		return -1;
	}
	*p_def = lookup_unique_assignment_raw(ctx->asts, name_symbol_id);
	return *p_def ? 0 : -1;
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

static int push_graph_binder(
	struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t classifier,
	int symbol_id,
	uint32_t* p_graph_binder_id
) {
	if (!ctx || !p_graph_binder_id || ctx->binder_count >= 512) {
		return -1;
	}

	uint32_t graph_binder_id =
		prototype_term_binding_for_scope_slot(ctx->terms, ctx->binder_count);
	if (graph_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_context = ctx->context_ids[ctx->binder_count];
	uint32_t context_id;
	/* Keep the source binder as provenance even after an initial classifier is
	 * available. Fixed-point elaboration may later refine that classifier and
	 * must then relocate this immutable context extension. */
	uint32_t classifier_variable = ast_binder_id;
	if (prototype_context_extend_occurrence(
			&ctx->metadata->contexts,
			parent_context,
			graph_binder_id,
			classifier,
			classifier_variable,
			&context_id
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
	if (prototype_type_expr_fresh_universe(ctx->type_declarations, &graph_type_expr) != 0) {
		return -1;
	}
	ctx->levels[ctx->level_count].ast_level_var = ast_level_var;
	ctx->levels[ctx->level_count].graph_type_expr = graph_type_expr;
	ctx->level_count++;
	*p_ret = graph_type_expr;
	return 0;
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
			uint32_t type_id;
			if (ctx->asts->type_defs[i].compiling) {
				type_id = ctx->asts->type_defs[i].compiled_type;
			} else if (compile_ast_type_def(ctx, i, &type_id) != 0) {
				return -1;
			}
			if (
				type_id >= ctx->type_declarations->type_count) {
				return -1;
			}
			type = &ctx->type_declarations->type_declarations[type_id];
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
			return prototype_type_expr_imported_type(
				ctx->type_declarations,
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
			return prototype_type_expr_external_term(
				ctx->type_declarations,
				external_term_name,
				p_ret
			);
		}
	if (!type) {
		type = prototype_type_declaration_lookup(ctx->type_declarations, symbol_id);
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
		prototype_term_type_instance_info(
			ctx->terms,
			evaluated,
			&type_id,
			args,
			&arg_count
		) != 0 ||
		arg_count != 0 ||
		type_id >= ctx->type_declarations->type_count) {
		return 1;
	}

	const struct prototype_type_declaration* aliased_type =
		&ctx->type_declarations->type_declarations[type_id];
	return prototype_type_expr_name(
		ctx->type_declarations,
		aliased_type->name_symbol_id,
		p_ret
	);
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
			if (prototype_type_expr_universe(ctx->type_declarations, expr.as.universe.level, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR:
			if (compile_ast_level_var(ctx, expr.as.universe_var.level_var, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_SELF:
			if (prototype_type_expr_self(ctx->type_declarations, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_AST_TYPE_EXPR_VAR: {
			uint32_t graph_binder_id;
			if (lookup_graph_binder(ctx, expr.as.var.ast_binder_id, &graph_binder_id) != 0) {
				return -1;
			}
			if (prototype_type_expr_var(ctx->type_declarations, graph_binder_id, expr.as.var.symbol_id, &compiled_type_expr) != 0) {
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
				prototype_type_expr_name(
					ctx->type_declarations,
					expr.as.name.symbol_id,
					&compiled_type_expr
				) != 0) {
				return -1;
				}
				break;
			}
				case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE: {
					if (prototype_type_expr_primitive(
							ctx->type_declarations,
							prototype_term_host_type_expr_tag(expr.as.host_type.host_type_id),
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
			if (prototype_type_expr_app(ctx->type_declarations, function, argument, &compiled_type_expr) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE:
			if (prototype_type_expr_local_type_member(
					ctx->type_declarations,
					expr.as.name_in_namespace.namespace_symbol_id,
					expr.as.name_in_namespace.symbol_id,
					&compiled_type_expr
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
			if (prototype_type_expr_arrow(ctx->type_declarations, domain, codomain, &compiled_type_expr) != 0) {
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
				push_graph_binder(
					ctx,
					expr.as.pi.ast_binder_id,
					PROTOTYPE_INVALID_ID,
					expr.as.pi.symbol_id,
					&graph_binder_id
				) != 0 ||
				compile_ast_type_expr(ctx, expr.as.pi.codomain, &codomain) != 0) {
				ctx->binder_count = saved_binder_count;
				return -1;
			}
			ctx->binder_count = saved_binder_count;
			if (prototype_type_expr_pi(
					ctx->type_declarations,
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

/* Type-family applications execute only the pure CBPV fragment. A neutral
 * dependent match cannot choose a branch, but each branch can still expose
 * the value returned by that pure computation. Project RETURN pointwise so a
 * computation term is never installed as the result type of another Comp. */
static int compile_extract_pure_return_value_at_depth(
	struct compile_context* ctx,
	uint32_t computation,
	uint32_t* p_ret,
	uint32_t depth
) {
	if (!ctx || !p_ret || computation >= ctx->terms->term_count || depth > 64) {
		return -1;
	}

	uint32_t whnf;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			computation,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_term term = ctx->terms->terms[whnf];
	if (term.tag == PROTOTYPE_TERM_RETURN) {
		*p_ret = term.as.return_term.value;
		return 0;
	}
	if (term.tag != PROTOTYPE_TERM_MATCH) {
		*p_ret = whnf;
		return 0;
	}
	if (term.as.match.case_count > 64) {
		return -1;
	}

	struct prototype_match_case_input cases[64];
	for (uint32_t i = 0; i < term.as.match.case_count; ++i) {
		uint32_t case_id = term.as.match.first_case + i;
		if (case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case source = ctx->terms->cases[case_id];
		uint32_t body;
		if (compile_extract_pure_return_value_at_depth(
				ctx, source.body, &body, depth + 1
			) != 0) {
			return -1;
		}
		cases[i].case_label_symbol_id = ctx->terms->case_label_symbols[case_id];
		cases[i].constructor_owner = source.constructor_owner;
		cases[i].constructor_id = source.constructor_id;
		cases[i].binders = source.binder_count == 0 ? NULL :
			&ctx->terms->case_binders[source.first_binder];
		cases[i].binder_count = source.binder_count;
		cases[i].body = body;
	}
	return prototype_term_match_with_ih_scope(
		ctx->terms,
		term.as.match.scrutinee,
		cases,
		term.as.match.case_count,
		term.as.match.ih_scope_id,
		p_ret
	);
}

static int compile_extract_pure_return_value(
	struct compile_context* ctx,
	uint32_t computation,
	uint32_t* p_ret
) {
	return compile_extract_pure_return_value_at_depth(
		ctx, computation, p_ret, 0
	);
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
				graph_type_expr >= ctx->type_declarations->expr_count ||
				ctx->type_declarations->exprs[graph_type_expr].tag !=
					PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR) {
				return -1;
			}
			return prototype_term_universe_var(
				ctx->terms,
				ctx->type_declarations->exprs[graph_type_expr].as.universe_var.level_var,
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
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.app.argument, self_type, &argument) != 0) {
				return -1;
			}
			if (compile_shared_app(ctx, function, argument, &application) != 0 ||
				compile_extract_pure_return_value(ctx, application, p_ret) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_AST_TYPE_EXPR_ARROW: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t empty_effects;
			uint32_t computation;
			uint32_t function;
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.arrow.domain, self_type, &domain) != 0) {
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.arrow.codomain, self_type, &codomain) != 0) {
				return -1;
			}
			if (prototype_term_effect_row_empty(ctx->terms, &empty_effects
				) != 0 || prototype_term_computation_type(
					ctx->terms, empty_effects, codomain, &computation
				) != 0 || prototype_term_pi(
					ctx->terms, domain, computation, &function
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, function, p_ret);
		}
		case PROTOTYPE_AST_TYPE_EXPR_PI: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t graph_binder_id;
			uint32_t family;
			uint32_t empty_effects;
			uint32_t computation;
			uint32_t function;
			uint32_t saved_binder_count = ctx->binder_count;
			if (compile_ast_type_expr_term_with_self(
					ctx, expr.as.pi.domain, self_type, &domain
				) != 0 ||
				push_graph_binder(
					ctx,
					expr.as.pi.ast_binder_id,
					domain,
					expr.as.pi.symbol_id,
					&graph_binder_id
				) != 0 ||
				compile_ast_type_expr_term_with_self(
					ctx, expr.as.pi.codomain, self_type, &codomain
				) != 0) {
				ctx->binder_count = saved_binder_count;
				return -1;
			}
			ctx->binder_count = saved_binder_count;
			if (prototype_term_effect_row_empty(ctx->terms, &empty_effects
				) != 0 ||
				prototype_term_computation_type(
					ctx->terms, empty_effects, codomain, &computation
				) != 0 ||
				prototype_term_pure_family(
					ctx->terms, graph_binder_id, computation, &family
				) != 0 ||
				prototype_term_pi_family(ctx->terms, domain, family, &function) != 0) {
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
				prototype_term_computation_type(
					ctx->terms, row, result, &computation
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, computation, p_ret);
		}
		default:
			return -1;
	}
}

static int compile_ast_type_expr_term(struct compile_context* ctx, uint32_t type_expr, uint32_t* p_ret) {
	return compile_ast_type_expr_term_with_self(ctx, type_expr, PROTOTYPE_INVALID_ID, p_ret);
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
			prototype_term_computation_type(
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
	if (compile_ast_type_expr_term(ctx, domain_expr, &domain) != 0) {
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
	if (compile_ast_type_expr_term(ctx, codomain_expr, &codomain) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (
		(row_binder = prototype_term_new_binding(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(ctx->terms, row_binder, &row) != 0 ||
		prototype_term_computation_type(ctx->terms, row, codomain, &computation) != 0 ||
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
		prototype_term_computation_type(
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
	uint32_t saved_binder_count = ctx->binder_count;
	if (compile_ast_type_expr_term(ctx, domain_expr, &domain) != 0) {
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
	if (prototype_term_type_instance_info(ctx->terms, function, &type_id, args, &arg_count) == 0) {
		if (type_id >= ctx->type_declarations->type_count) {
			return -1;
		}
		const struct prototype_type_declaration* type = &ctx->type_declarations->type_declarations[type_id];
		if (arg_count < type->parameter_count + type->index_count) {
			return prototype_term_type_instance_extend(
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
				ctx->type_declarations->next_level_var++,
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
			ctx->type_declarations->next_level_var++,
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
			uint32_t type_id;
			if (ctx->asts->type_defs[i].compiling) {
				type_id = ctx->asts->type_defs[i].compiled_type;
			} else if (compile_ast_type_def(ctx, i, &type_id) != 0) {
				return -1;
			}
			if (
				type_id >= ctx->type_declarations->type_count) {
				return -1;
			}
			type = &ctx->type_declarations->type_declarations[type_id];
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
		type = prototype_type_declaration_lookup(ctx->type_declarations, symbol_id);
	}
	if (!type) {
		return -1;
	}
	return prototype_term_type_instance_make(
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
