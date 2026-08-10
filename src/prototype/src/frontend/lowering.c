#include "a_program/frontend/lowering.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/ast_common.h"
#include "../artifact/artifact_internal.h"

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
		metadata && operation_id < metadata->operation_count &&
		visited < metadata->operation_count;
		++visited) {
		const struct prototype_operation_node* operation =
			&metadata->operations[operation_id];
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			operation_id = operation->function;
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
			operation_id = operation->body;
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

enum compile_ref_polarity {
	COMPILE_REF_POLARITY_UNKNOWN = PROTOTYPE_OPERATION_POLARITY_UNKNOWN,
	COMPILE_REF_POLARITY_VALUE = PROTOTYPE_OPERATION_POLARITY_VALUE,
	COMPILE_REF_POLARITY_COMPUTATION = PROTOTYPE_OPERATION_POLARITY_COMPUTATION
};

/* This is lowering information, not a TermDB tag or a typing result.  It
 * preserves the CBPV shape of a computation while classifier constraints are
 * still unresolved.  The solver remains responsible for proving Pi/Comp. */
enum compile_ref_computation_kind {
	COMPILE_REF_COMPUTATION_KIND_UNKNOWN = 0,
	COMPILE_REF_COMPUTATION_KIND_RETURNING,
	COMPILE_REF_COMPUTATION_KIND_FUNCTION
};

struct compile_ref {
	uint32_t term;
	uint32_t classifier;
	uint32_t operation;
	int polarity;
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
#define PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY 16384
#define PROTOTYPE_OPERATION_SOLVER_INPUT_FACT_CAPACITY 2048
#define PROTOTYPE_OPERATION_EFFECT_ROW_META_CAPACITY 4096
#define PROTOTYPE_OPERATION_EFFECT_ROW_SOLUTION_ATOM_CAPACITY 16384

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
	OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION
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
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL,
	OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY,
	OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION,
	OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
};

enum operation_motive_solution_state {
	OPERATION_MOTIVE_SOLUTION_UNRESOLVED = 0,
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
		uint32_t body_operation;
		uint32_t expected_classifier;
		struct prototype_kernel_conversion_goal kernel_goal;
	} conversion;
	struct {
		int role;
		uint32_t body_or_function_operation;
		uint32_t domain_classifier_or_argument_operation;
	} pi;
	struct {
		uint32_t function_operation;
		uint32_t argument_operation;
	} constructor_formation;
	struct operation_classifier_motive_case_goal motive_case;
	struct {
		uint32_t recursive_argument_operation;
		uint32_t ih_scope_id;
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

struct operation_classifier_goal {
	uint32_t id;
	int kind;
	int state;
	int reason;
	uint32_t source_operation;
	uint32_t source_ast;
	uint32_t context_id;
	uint32_t classifier_variable;
	union operation_classifier_goal_payload payload;
};

/*
 * Input facts belong to the solver invocation, not to JudgementDB. They are
 * declarations and binder assumptions collected while lowering. The same
 * facts become proof relations only after the solver has found classifiers
 * for the operation graph.
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
	uint32_t argument_operation;
	uint32_t scope_frame;
};

/*
 * This is the solver-side form of
 *
 *     classifier(branch body) == M(Constructor(case binders)).
 *
 * The case telescope is referenced by the corresponding OperationGraph case
 * Context ID. It is not reconstructed from a shared core VAR node, whose
 * identity is deliberately weaker than a source occurrence identity.
 */
/*
 * This arena is intentionally separate from TermDB and JudgementDB. Entries in
 * bindings are resolved TermDB classifiers; INVALID means an unsolved type
 * metavariable identified by its operation id.
 */
struct operation_classifier_solver {
	uint32_t bindings[4096];
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
	struct operation_classifier_goal
		constraints[PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY];
	uint32_t constraint_count;
	uint32_t first_dependent_constraint[4096];
	struct {
		uint32_t constraint;
		uint32_t next;
	} dependent_constraints[PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY * 4];
	uint32_t dependent_constraint_count;
	uint32_t worklist[PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY];
	uint8_t constraint_queued[PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY];
	uint32_t worklist_head;
	uint32_t worklist_count;
	struct operation_solver_input_fact
		input_facts[PROTOTYPE_OPERATION_SOLVER_INPUT_FACT_CAPACITY];
	uint32_t input_fact_count;
};

enum operation_effect_row_meta_state {
	OPERATION_EFFECT_ROW_META_UNSOLVED = 0,
	OPERATION_EFFECT_ROW_META_SOLVED
};

/* Effect-row metavariables are owned by source occurrences. The placeholder
 * row is only a location in the current classifier graph; binder identity is
 * never the identity of the solver cell. */
struct operation_effect_row_meta {
	uint32_t owner_operation;
	uint32_t placeholder_row;
	unsigned solution_effects;
	uint32_t first_solution_atom;
	uint32_t solution_atom_count;
	uint32_t solution_row;
	int state;
};

struct operation_effect_solver {
	struct operation_effect_row_meta
		metas[PROTOTYPE_OPERATION_EFFECT_ROW_META_CAPACITY];
	uint32_t meta_count;
	uint32_t solution_atoms[
		PROTOTYPE_OPERATION_EFFECT_ROW_SOLUTION_ATOM_CAPACITY
	];
	uint32_t solution_atom_count;
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
};

struct compile_context {
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_judgement_delta judgement_delta;
	struct prototype_compile_metadata* metadata;
	struct operation_classifier_solver classifier_solver;
	struct operation_effect_solver effect_solver;
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
	uint32_t whnf;
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
	struct prototype_operation_computation_fold_clause occurrence_clauses[
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
				ctx, PROTOTYPE_OPERATION_LAMBDA, inner_lambda,
				PROTOTYPE_INVALID_ID, ast_clause->body,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				operation_body.operation, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
				&inner_lambda_operation
			) != 0) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
		ctx->metadata->operations[
			inner_lambda_operation
		].referenced_ast_binder_id =
			ast_clause->operation_continuation_binder_id;
		ctx->metadata->operations[inner_lambda_operation].binding_id =
			continuation_binder;
		ctx->binder_count = saved_binder_count;
		if (operation_add(
				ctx, PROTOTYPE_OPERATION_LAMBDA, outer_lambda,
				PROTOTYPE_INVALID_ID, ast_clause->body,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				inner_lambda_operation, PROTOTYPE_INVALID_ID,
				domain_status == 0 ? operation_domain : PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, 0,
				&outer_lambda_operation
			) != 0) {
			return -1;
		}
		ctx->metadata->operations[
			outer_lambda_operation
		].referenced_ast_binder_id = ast_clause->operation_argument_binder_id;
		ctx->metadata->operations[outer_lambda_operation].binding_id =
			argument_binder;
		if (outer_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[outer_lambda].tag != PROTOTYPE_TERM_LAMBDA ||
			inner_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[inner_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		core_clauses[i].operation = operation.term;
		core_clauses[i].body = outer_lambda;
		occurrence_clauses[i].operation_operation = operation.operation;
		occurrence_clauses[i].body_operation = operation_body.operation;
		occurrence_clauses[i].clause_operation = outer_lambda_operation;
		occurrence_clauses[i].context_id =
			ctx->metadata->operations[operation_body.operation].context_id;
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
			PROTOTYPE_OPERATION_LAMBDA,
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
	ctx->metadata->operations[
		return_clause_operation
	].referenced_ast_binder_id = node->as.computation_fold.return_binder_id;
	ctx->metadata->operations[return_clause_operation].binding_id = return_binder;
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
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
		ctx, PROTOTYPE_OPERATION_COMPUTATION_FOLD, term, PROTOTYPE_INVALID_ID, ast_id,
		computation.operation,
		node->as.computation_fold.clause_count == 0 ?
			return_clause_operation : PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		node->as.computation_fold.clause_count == 0 ?
			PROTOTYPE_INVALID_ID : return_body.operation,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, &p_ret->operation
	) != 0 || return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	struct prototype_operation_node* fold_operation =
		&ctx->metadata->operations[p_ret->operation];
	fold_operation->fold_return_ast_binder_id =
		node->as.computation_fold.return_binder_id;
	fold_operation->fold_return_binder_id =
		ctx->terms->terms[return_clause].as.lambda.binding_id;
	fold_operation->fold_return_operation = return_clause_operation;
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph(ctx->metadata, &graph);
	fold_operation->first_fold_clause = (uint32_t)graph.fold_clause_count;
	fold_operation->fold_clause_count = node->as.computation_fold.clause_count;
	for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
		if (prototype_operation_graph_add_fold_clause(
				&graph, &ctx->metadata->contexts, occurrence_clauses[i], NULL
			) != 0) {
			return -1;
		}
	}
	prototype_compile_metadata_commit_operation_graph(ctx->metadata, &graph);
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
	 * recover polarity.  Its declared classifier is already authoritative:
	 * a Pi denotes a raw computation function, while all other named terms
	 * begin as values until an expected computation boundary elaborates them. */
	p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
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
			p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
			p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
		}
	}
	if (operation_add(
			ctx,
			PROTOTYPE_OPERATION_ATOM,
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
	ctx->metadata->operations[p_ret->operation].polarity = p_ret->polarity;
	ctx->metadata->operations[p_ret->operation].computation_kind =
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
	ref->polarity = COMPILE_REF_POLARITY_UNKNOWN;
	ref->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
}

static void operation_default_semantics(
	const struct compile_context* ctx,
	int tag,
	uint32_t core_term,
	int* p_polarity,
	int* p_computation_kind,
	int* p_application_role
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	int constructor_spine = ctx && tag == PROTOTYPE_OPERATION_APP &&
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
	if (!p_polarity || !p_computation_kind || !p_application_role) {
		return;
	}
	*p_polarity = COMPILE_REF_POLARITY_UNKNOWN;
	*p_computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	*p_application_role = PROTOTYPE_TERM_APPLICATION_NONE;
	switch (tag) {
		case PROTOTYPE_OPERATION_VAR:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_THUNK:
			*p_polarity = COMPILE_REF_POLARITY_VALUE;
			return;
		case PROTOTYPE_OPERATION_LAMBDA:
			*p_polarity = COMPILE_REF_POLARITY_COMPUTATION;
			*p_computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
			return;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_MATCH:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			*p_polarity = COMPILE_REF_POLARITY_COMPUTATION;
			*p_computation_kind = COMPILE_REF_COMPUTATION_KIND_RETURNING;
			return;
		case PROTOTYPE_OPERATION_APP:
			*p_polarity = constructor_spine ?
				COMPILE_REF_POLARITY_VALUE : COMPILE_REF_POLARITY_COMPUTATION;
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
	struct prototype_operation_node node;
	memset(&node, 0, sizeof(node));
	node.tag = tag;
	operation_default_semantics(
		ctx,
		tag,
		core_term,
		&node.polarity,
		&node.computation_kind,
		&node.application_role
	);
	if (tag == PROTOTYPE_OPERATION_ASCRIPTION) {
		if (body >= ctx->metadata->operation_count) {
			return -1;
		}
		node.polarity = ctx->metadata->operations[body].polarity;
		node.computation_kind =
			ctx->metadata->operations[body].computation_kind;
	}
	node.context_id = ctx->context_ids[ctx->binder_count];
	node.core_term = core_term;
	node.known_classifier = classifier;
	node.classifier = PROTOTYPE_INVALID_ID;
	node.classifier_variable = (uint32_t)ctx->metadata->operation_count;
	node.source_ast = source_ast;
	node.source_symbol_id = -1;
	node.binder_symbol_id = -1;
	node.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
	node.binding_id = PROTOTYPE_INVALID_ID;
	node.function = function;
	node.argument = argument;
	node.body = body;
	node.scrutinee = scrutinee;
	node.binder_classifier = binder_classifier;
	node.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID;
	node.fold_return_binder_id = PROTOTYPE_INVALID_ID;
	node.fold_return_operation = PROTOTYPE_INVALID_ID;
	node.first_case = first_case;
	node.case_count = case_count;
	node.first_fold_clause = PROTOTYPE_INVALID_ID;
	node.fold_clause_count = 0;

	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph(ctx->metadata, &graph);
	if (prototype_operation_graph_add(
			&graph, &ctx->metadata->contexts, node, p_operation
		) != 0) {
		return -1;
	}
	prototype_compile_metadata_commit_operation_graph(ctx->metadata, &graph);
	return 0;
}

static uint32_t operation_available_classifier(
	const struct prototype_operation_node* operation
) {
	if (!operation) {
		return PROTOTYPE_INVALID_ID;
	}
	return operation->classifier != PROTOTYPE_INVALID_ID ?
		operation->classifier : operation->known_classifier;
}

static int operation_add_match_case(
	struct compile_context* ctx,
	uint32_t body_operation,
	uint32_t constructor_owner,
	uint32_t constructor_id,
	const struct prototype_ast_match_case* ast_case,
	uint32_t* p_case
) {
	if (!ctx || !ctx->metadata || !p_case ||
		!ast_case || ast_case->binder_count > 16 ||
		ast_case->first_binder + ast_case->binder_count >
			ctx->asts->case_binder_count) {
		return -1;
	}
	struct prototype_operation_match_case match_case;
	memset(&match_case, 0, sizeof(match_case));
	match_case.body_operation = body_operation;
	match_case.context_id =
		ctx->metadata->operations[body_operation].context_id;
	match_case.constructor_owner = constructor_owner;
	match_case.constructor_id = constructor_id;
	match_case.case_label_symbol_id = -1;
	match_case.binder_count = ast_case->binder_count;
	for (uint32_t i = 0; i < ast_case->binder_count; ++i) {
		match_case.ast_binder_ids[i] =
			ctx->asts->case_binders[ast_case->first_binder + i].ast_binder_id;
	}
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph(ctx->metadata, &graph);
	if (prototype_operation_graph_add_case(
			&graph, &ctx->metadata->contexts, match_case, p_case
		) != 0) {
		return -1;
	}
	prototype_compile_metadata_commit_operation_graph(ctx->metadata, &graph);
	return 0;
}

static int operation_apply_classifier(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t argument_term,
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
	if (!prototype_judgement_classifier_compatible(
			ctx->terms, ctx->type_declarations,
			expected_domain, argument_classifier
		)) {
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
	return prototype_term_graph_substitute_bound_var(
		ctx->terms,
		ctx->type_declarations,
		body,
		binding_id,
		argument_term,
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
	p_ref->polarity =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_FORCE ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_OPERATION_REQUEST ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD ?
			COMPILE_REF_POLARITY_COMPUTATION : COMPILE_REF_POLARITY_VALUE;
	p_ref->computation_kind =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ?
			COMPILE_REF_COMPUTATION_KIND_RETURNING :
			ctx->terms->terms[term].tag == PROTOTYPE_TERM_LAMBDA ?
				COMPILE_REF_COMPUTATION_KIND_FUNCTION :
				COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
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
		if (prototype_term_make_host_type(
				ctx->terms,
				PROTOTYPE_HOST_TYPE_INT64,
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
			ctx, PROTOTYPE_OPERATION_ATOM, term, p_ref->classifier,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ref->operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[p_ref->operation].polarity = p_ref->polarity;
	ctx->metadata->operations[p_ref->operation].computation_kind =
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
	if (resolution->scrutinee_operation < ctx->metadata->operation_count) {
		uint32_t classifier = operation_available_classifier(
			&ctx->metadata->operations[resolution->scrutinee_operation]
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

static int lookup_ih_scope_id(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t* p_frame_id
) {
	if (!ctx || !p_frame_id) {
		return -1;
	}
	for (uint32_t i = ctx->ih_scope_count; i > 0; --i) {
		const struct ih_scope_map_entry* entry = &ctx->ih_scopes[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			*p_frame_id = entry->ih_scope_id;
			return 0;
		}
	}
	return -1;
}

static int add_compile_label(
	struct compile_context* ctx,
	int name_symbol_id,
	uint32_t term,
	uint32_t classifier,
	uint32_t operation
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
	ctx->metadata->labels[id].classifier = classifier;
	ctx->metadata->labels[id].operation = operation;
	if (prototype_term_canonical_key_with_types(
			ctx->terms,
			ctx->type_declarations,
			term,
			&ctx->metadata->labels[id].canonical_key
		) != 0) {
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
	if (prototype_type_declaration_code_shape_key(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			type_id,
			&type_export->code_shape_key
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
		!ctx->metadata || operation >= ctx->metadata->operation_count ||
		ctx->pending_match_typing_count >= 512) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		if (ctx->pending_match_typings[i].match_term == match_term) {
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
		source_operation >= ctx->metadata->operation_count ||
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
		prototype_judgement_delta_set_operation(
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
		prototype_judgement_delta_set_operation(
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
	if (prototype_context_extend(
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

static int resolve_current_graph_binder_classifier(
	struct compile_context* ctx,
	uint32_t classifier
) {
	if (!ctx || !ctx->metadata || ctx->binder_count == 0 ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct binder_map_entry* binder = &ctx->binders[ctx->binder_count - 1];
	uint32_t context_id;
	if (prototype_context_extend(
			&ctx->metadata->contexts,
			ctx->context_ids[ctx->binder_count - 1],
			binder->graph_binder_id,
			classifier,
			binder->ast_binder_id,
			&context_id
		) != 0) {
		return -1;
	}
	binder->classifier = classifier;
	ctx->context_ids[ctx->binder_count] = context_id;
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
			if (compile_ast_type_def(ctx, i, &type_id) != 0 ||
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
				&interface->type_exports[type_export_id].code_shape_key,
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
				int status = resolve_unique_assignment_if_present(ctx, expr.as.name.symbol_id, &def);
				if (status == 1) {
					if (compile_type_declaration_term_by_symbol(ctx, expr.as.name.symbol_id, p_ret) == 0) {
						return 0;
					}
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
			if (prototype_term_effect_label(
					ctx->terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
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
			if (prototype_term_effect_label(
					ctx->terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
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
		if (arg_count < type->parameter_count) {
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
			if (compile_ast_type_def(ctx, i, &type_id) != 0 ||
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
	if (type->parameter_count != 0) {
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

struct match_compile_state {
	uint32_t match_ast;
	uint32_t scrutinee;
	uint32_t scrutinee_operation;
	uint32_t ih_scope_id;
	struct prototype_match_case_input case_inputs[64];
	uint32_t resolution_item_ids[64];
	int case_constructor_symbols[64];
	uint32_t branch_operations[64];
	int branch_computation_kinds[64];
	struct prototype_case_binder binder_storage[256];
	uint32_t binder_cursor;
	int (*compile_branch_body)(
		struct compile_context*, uint32_t, void*, struct compile_ref*
	);
	void* compile_branch_data;
};

struct compiled_match_branch {
	const struct prototype_case_binder* binders;
	uint32_t binder_count;
	uint32_t body;
	uint32_t operation;
};

static int create_match_pattern_binders(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	uint32_t previous_binder_count,
	uint32_t previous_ih_scope_count,
	uint32_t* p_binder_start
) {
	if (!ctx || !state || !old_case || !p_binder_start ||
		state->binder_cursor + old_case->binder_count > 256) {
		return -1;
	}
	*p_binder_start = state->binder_cursor;
	for (uint32_t j = 0; j < old_case->binder_count; ++j) {
		const struct prototype_ast_binder* ast_binder =
			&ctx->asts->case_binders[old_case->first_binder + j];
		uint32_t graph_binder_id;
		if (push_graph_binder(
			ctx,
			ast_binder->ast_binder_id,
			PROTOTYPE_INVALID_ID,
			ast_binder->symbol_id,
			&graph_binder_id
		) != 0) {
			ctx->binder_count = previous_binder_count;
			return -1;
		}
		if (ctx->ih_scope_count >= 512) {
			ctx->binder_count = previous_binder_count;
			ctx->ih_scope_count = previous_ih_scope_count;
			return -1;
		}
		ctx->ih_scopes[ctx->ih_scope_count].ast_binder_id =
			ast_binder->ast_binder_id;
		ctx->ih_scopes[ctx->ih_scope_count].ih_scope_id = state->ih_scope_id;
		ctx->ih_scope_count++;
		state->binder_storage[state->binder_cursor + j].binding_id = graph_binder_id;
	}
	return 0;
}

static int prepare_match_pattern_environment(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	uint32_t previous_binder_count,
	uint32_t previous_ih_scope_count,
	uint32_t* p_binder_start
) {
	if (create_match_pattern_binders(
		ctx,
		state,
		old_case,
		previous_binder_count,
		previous_ih_scope_count,
		p_binder_start
	) != 0) {
		return -1;
	}
	(void)previous_binder_count;
	(void)previous_ih_scope_count;
	return 0;
}

static int compile_match_branch_body(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	int case_label_symbol_id,
	uint32_t case_index,
	struct compiled_match_branch* branch
) {
	struct compile_ref body_ref;
	if (!ctx || !state || !old_case || !branch) {
		return -1;
	}
	memset(branch, 0, sizeof(*branch));
	branch->binders = &state->binder_storage[state->binder_cursor];
	branch->binder_count = old_case->binder_count;
	state->case_inputs[case_index].case_label_symbol_id = case_label_symbol_id;
	state->case_inputs[case_index].constructor_owner = PROTOTYPE_INVALID_ID;
	state->case_inputs[case_index].constructor_id = PROTOTYPE_INVALID_ID;
	state->case_inputs[case_index].binders = &state->binder_storage[state->binder_cursor];
	state->case_inputs[case_index].binder_count = old_case->binder_count;
	if (!state->compile_branch_body || state->compile_branch_body(
			ctx, old_case->body, state->compile_branch_data, &body_ref
		) != 0 ||
		body_ref.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	branch->body = body_ref.term;
	branch->operation = body_ref.operation;
	state->branch_operations[case_index] = body_ref.operation;
	state->branch_computation_kinds[case_index] = body_ref.computation_kind;
	state->case_inputs[case_index].body = branch->body;
	return 0;
}

static int compile_match_branch(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	struct match_compile_state* state,
	uint32_t case_index
) {
	const struct prototype_ast_match_case* old_case;
	struct compiled_match_branch branch;
	uint32_t previous_binder_count;
	uint32_t previous_ih_scope_count;
	uint32_t binder_start;
	uint32_t resolution_item_id;
	if (!ctx || !node || !state || case_index >= node->as.match.case_count) {
		return -1;
	}
	old_case = &ctx->asts->cases[node->as.match.first_case + case_index];
	previous_binder_count = ctx->binder_count;
	previous_ih_scope_count = ctx->ih_scope_count;
	if (add_match_constructor_resolution_item(
		ctx,
		state->match_ast,
		case_index,
		state->scrutinee,
		old_case->constructor_symbol_id,
		&resolution_item_id
	) != 0) {
		ctx->binder_count = previous_binder_count;
		ctx->ih_scope_count = previous_ih_scope_count;
		return -1;
	}
	state->resolution_item_ids[case_index] = resolution_item_id;
	state->case_constructor_symbols[case_index] = old_case->constructor_symbol_id;
	if (prepare_match_pattern_environment(
		ctx,
		state,
		old_case,
		previous_binder_count,
		previous_ih_scope_count,
		&binder_start
	) != 0 ||
		compile_match_branch_body(
			ctx,
			state,
			old_case,
			old_case->constructor_symbol_id,
			case_index,
			&branch
	) != 0) {
		ctx->binder_count = previous_binder_count;
		ctx->ih_scope_count = previous_ih_scope_count;
		return -1;
	}
	ctx->binder_count = previous_binder_count;
	ctx->ih_scope_count = previous_ih_scope_count;
	state->binder_cursor += old_case->binder_count;
	return 0;
}

static int match_scrutinee_proven_classifier_hint(
	struct compile_context* ctx,
	uint32_t scrutinee_ast,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || scrutinee_ast >= ctx->asts->node_count) {
		return -1;
	}
	*p_classifier = PROTOTYPE_INVALID_ID;
	const struct prototype_ast_node* scrutinee = &ctx->asts->nodes[scrutinee_ast];
	switch (scrutinee->tag) {
		case PROTOTYPE_AST_VAR:
			if (lookup_graph_binder_classifier(
					ctx,
					scrutinee->as.var.ast_binder_id,
					p_classifier
				) != 0) {
				*p_classifier = PROTOTYPE_INVALID_ID;
			}
			return 0;
			case PROTOTYPE_AST_ASCRIPTION:
				return match_scrutinee_proven_classifier_hint(
					ctx,
					scrutinee->as.ascription.term,
					p_classifier
				);
			default:
				return 0;
		}
	}

static int compile_match_branch_body_default(
	struct compile_context* ctx,
	uint32_t body_ast,
	void* data,
	struct compile_ref* p_ret
) {
	(void)data;
	return compile_ast_computation_ref(ctx, body_ast, p_ret);
}

static int compile_ast_match_from_value_with_branch_compiler(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_ref* scrutinee_ref,
	int (*compile_branch_body)(
		struct compile_context*, uint32_t, void*, struct compile_ref*
	),
	void* compile_branch_data,
	struct compile_ref* p_ref
) {
	struct match_compile_state state;
	uint32_t match_term;
	uint32_t scrutinee_proven_classifier_hint = PROTOTYPE_INVALID_ID;
	if (!ctx || !node || !scrutinee_ref || !p_ref || node->tag != PROTOTYPE_AST_MATCH ||
		scrutinee_ref->polarity != COMPILE_REF_POLARITY_VALUE) {
		return -1;
	}
	memset(&state, 0, sizeof(state));
	state.match_ast = ast_id;
	state.ih_scope_id = PROTOTYPE_INVALID_ID;
	state.compile_branch_body = compile_branch_body;
	state.compile_branch_data = compile_branch_data;
	if (node->as.match.case_count > 64) {
		return -1;
	}
	state.scrutinee = scrutinee_ref->term;
	state.scrutinee_operation = scrutinee_ref->operation;
	scrutinee_proven_classifier_hint = scrutinee_ref->classifier;
	if (scrutinee_proven_classifier_hint == PROTOTYPE_INVALID_ID &&
		match_scrutinee_proven_classifier_hint(
			ctx,
			node->as.match.scrutinee,
			&scrutinee_proven_classifier_hint
		) != 0) {
		return -1;
	}
	state.ih_scope_id = prototype_term_new_ih_scope(ctx->terms);
	if (state.ih_scope_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (compile_match_branch(ctx, node, &state, i) != 0) {
			return -1;
		}
	}
	if (prototype_term_match_with_ih_scope(
		ctx->terms,
		state.scrutinee,
		state.case_inputs,
		node->as.match.case_count,
		state.ih_scope_id,
		&match_term
	) != 0) {
		return -1;
	}
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	if (prototype_term_set_ih_scope_term(ctx->terms, state.ih_scope_id, match_term) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (queue_match_constructor_resolution(
				ctx,
				state.resolution_item_ids[i],
				match_term,
				state.scrutinee_operation,
				scrutinee_proven_classifier_hint,
				state.case_constructor_symbols[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t first_operation_case = (uint32_t)ctx->metadata->operation_case_count;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		uint32_t operation_case;
		if (operation_add_match_case(
				ctx,
				state.branch_operations[i],
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				&ctx->asts->cases[node->as.match.first_case + i],
				&operation_case
			) != 0) {
			return -1;
		}
		ctx->metadata->operation_cases[operation_case].case_label_symbol_id =
			state.case_constructor_symbols[i];
	}
	p_ref->term = match_term;
	p_ref->classifier = PROTOTYPE_INVALID_ID;
	p_ref->operation = PROTOTYPE_INVALID_ID;
	p_ref->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ref->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (state.branch_computation_kinds[i] !=
			COMPILE_REF_COMPUTATION_KIND_FUNCTION) {
			p_ref->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
			break;
		}
	}
	if (operation_add(
			ctx,
			PROTOTYPE_OPERATION_MATCH,
			match_term,
			PROTOTYPE_INVALID_ID,
			ast_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			state.scrutinee_operation,
			PROTOTYPE_INVALID_ID,
			first_operation_case,
			node->as.match.case_count,
			&p_ref->operation
		) != 0) {
		return -1;
	}
	/* IH occurrences are lowered before their enclosing Match Operation exists.
	 * Complete that exact typed edge now. The Core IH frame only identifies an
	 * erased match term and cannot distinguish two typed Match occurrences that
	 * intentionally share that term. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			operation->first_case != state.ih_scope_id) {
			continue;
		}
		if (operation->scrutinee != PROTOTYPE_INVALID_ID &&
			operation->scrutinee != p_ref->operation) {
			return -1;
		}
		operation->scrutinee = p_ref->operation;
	}
	if (queue_match_typing(
			ctx,
			match_term,
			p_ref->operation,
			ctx->asts->next_ast_level_var++
		) != 0) {
		return -1;
	}
	return 0;
}

static int compile_ast_match_from_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_ref* scrutinee_ref,
	struct compile_ref* p_ref
) {
	return compile_ast_match_from_value_with_branch_compiler(
		ctx,
		ast_id,
		node,
		scrutinee_ref,
		compile_match_branch_body_default,
		NULL,
		p_ref
	);
}

static int type_expr_contains_self(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	if (!db || expr_id >= db->expr_count) {
		return 0;
	}
	const struct prototype_type_expr* expr = &db->exprs[expr_id];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_SELF:
			return 1;
		case PROTOTYPE_TYPE_EXPR_APP:
			return type_expr_contains_self(db, expr->as.app.function) ||
				type_expr_contains_self(db, expr->as.app.argument);
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return type_expr_contains_self(db, expr->as.arrow.domain) ||
				type_expr_contains_self(db, expr->as.arrow.codomain);
		case PROTOTYPE_TYPE_EXPR_PI:
			return type_expr_contains_self(db, expr->as.pi.domain) ||
				type_expr_contains_self(db, expr->as.pi.codomain);
		default:
			return 0;
	}
}

static int type_expr_is_direct_self(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	return db && expr_id < db->expr_count &&
		db->exprs[expr_id].tag == PROTOTYPE_TYPE_EXPR_SELF;
}

static int constructor_field_is_valid_inductive_field(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	if (type_expr_is_direct_self(db, expr_id)) {
		return 1;
	}
	return !type_expr_contains_self(db, expr_id);
}

static int compile_type_formation_classifier_family(
	struct compile_context* ctx,
	const struct prototype_ast_type_def* ast_type,
	uint32_t* p_classifier
) {
	if (!ctx || !ast_type || !p_classifier) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			ctx->terms,
			ctx->type_declarations->next_level_var++,
			&classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = ast_type->parameter_count; i > 0; --i) {
		const struct prototype_ast_type_parameter* parameter =
			&ctx->asts->type_parameters[ast_type->first_parameter + i - 1];
		uint32_t domain;
		uint32_t binding_id;
		uint32_t codomain_family;
		if (compile_ast_type_expr_term(ctx, parameter->type_expr, &domain) != 0 ||
			lookup_graph_binder(ctx, parameter->ast_binder_id, &binding_id) != 0 ||
			prototype_term_pure_family(
				ctx->terms, binding_id, classifier, &codomain_family
			) != 0 ||
			prototype_term_pi_family(
				ctx->terms, domain, codomain_family, &classifier
			) != 0) {
			return -1;
		}
	}
	*p_classifier = classifier;
	return 0;
}

static int compile_ast_type_def(
	struct compile_context* ctx,
	uint32_t ast_type_def_id,
	uint32_t* p_type_id
) {
	if (!ctx || !p_type_id || ast_type_def_id >= ctx->asts->type_def_count) {
		return -1;
	}

	struct prototype_ast_type_def* ast_type = &ctx->asts->type_defs[ast_type_def_id];
	if (ast_type->compiled) {
		*p_type_id = ast_type->compiled_type;
		return 0;
	}
	if (ast_type->compiling) {
		return -1;
	}

	uint32_t type_id;
	if (prototype_type_declaration_add(ctx->type_declarations, ast_type->name_symbol_id, &type_id) != 0) {
		return -1;
	}
	ctx->type_declarations->type_declarations[type_id].namespace_symbol_id =
		ctx->namespace_symbol_id;
	uint32_t type_term;

	ast_type->compiling = 1;
	ast_type->compiled_type = type_id;
	for (uint32_t i = 0; i < ast_type->parameter_count; ++i) {
		const struct prototype_ast_type_parameter* parameter =
			&ctx->asts->type_parameters[ast_type->first_parameter + i];
		uint32_t compiled_type_expr;
		uint32_t graph_binder_id;
		if (lookup_graph_binder(ctx, parameter->ast_binder_id, &graph_binder_id) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (compile_ast_type_expr(ctx, parameter->type_expr, &compiled_type_expr) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (prototype_type_declaration_add_parameter(
			ctx->type_declarations,
			type_id,
			graph_binder_id,
			parameter->name_symbol_id,
			compiled_type_expr
		) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
	}

	uint32_t type_args[16];
	if (ast_type->parameter_count > 16) {
		ast_type->compiling = 0;
		return -1;
	}
	for (uint32_t i = 0; i < ast_type->parameter_count; ++i) {
		const struct prototype_ast_type_parameter* parameter =
			&ctx->asts->type_parameters[ast_type->first_parameter + i];
		uint32_t graph_binder_id;
		if (lookup_graph_binder(ctx, parameter->ast_binder_id, &graph_binder_id) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (prototype_term_var(
			ctx->terms,
			graph_binder_id,
			&type_args[i]
		) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
	}
	if (prototype_term_type_instance_make(
		ctx->terms,
		ctx->type_declarations,
		type_id,
		type_args,
		ast_type->parameter_count,
		&type_term
	) != 0) {
		ast_type->compiling = 0;
		return -1;
	}
	if (compile_type_formation_classifier_family(
			ctx,
			ast_type,
			&ctx->type_declarations->type_declarations[type_id].formation_classifier
		) != 0) {
		ast_type->compiling = 0;
		return -1;
	}
	ctx->type_declarations->type_declarations[type_id].parameter_context =
		ctx->context_ids[ctx->binder_count];
	ctx->type_declarations->type_declarations[type_id].index_context =
		ctx->context_ids[ctx->binder_count];

	int has_recursive_constructor_field = 0;
	int has_structural_seed_constructor = 0;
	for (uint32_t i = 0; i < ast_type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&ctx->asts->type_constructors[ast_type->first_constructor + i];
		uint32_t compiled_result_type;
		uint32_t compiled_field_types[64];
		uint32_t compiled_field_terms[64];
		uint32_t constructor_id;
		uint32_t curried_classifier_cache;
		uint32_t previous_binder_count = ctx->binder_count;
		uint32_t parameter_context =
			ctx->context_ids[previous_binder_count];
		int constructor_has_recursive_field = 0;
		if (constructor->field_count > 64) {
			ast_type->compiling = 0;
			return -1;
		}
		if (compile_ast_type_expr(ctx, constructor->result_type, &compiled_result_type) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (!type_expr_is_direct_self(ctx->type_declarations, compiled_result_type)) {
			ast_type->compiling = 0;
			return -1;
		}
		for (uint32_t j = 0; j < constructor->field_count; ++j) {
			uint32_t field_id = constructor->first_field_type + j;
			uint32_t field_type = ctx->asts->type_field_exprs[field_id];
			uint32_t ast_field_binder_id = ctx->asts->type_field_binder_ids ?
				ctx->asts->type_field_binder_ids[field_id] :
				PROTOTYPE_INVALID_ID;
			int field_symbol_id = ctx->asts->type_field_name_symbol_ids ?
				ctx->asts->type_field_name_symbol_ids[field_id] :
				-1;
			if (compile_ast_type_expr(ctx, field_type, &compiled_field_types[j]) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(
					ctx,
					field_type,
					type_term,
					&compiled_field_terms[j]
				) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (!constructor_field_is_valid_inductive_field(
					ctx->type_declarations,
					compiled_field_types[j]
			)) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (type_expr_is_direct_self(ctx->type_declarations, compiled_field_types[j])) {
				constructor_has_recursive_field = 1;
				has_recursive_constructor_field = 1;
			}
			uint32_t graph_binder_id;
			if (push_graph_binder(
					ctx,
					ast_field_binder_id,
					compiled_field_terms[j],
					field_symbol_id,
					&graph_binder_id
				) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
		}
		if (!constructor_has_recursive_field) {
			has_structural_seed_constructor = 1;
		}
		uint32_t field_context = ctx->context_ids[ctx->binder_count];
		if (prototype_type_constructor_derive_curried_classifier(
				ctx->terms,
				&ctx->metadata->contexts,
				parameter_context,
				field_context,
				type_term,
				&curried_classifier_cache
			) != 0) {
			ctx->binder_count = previous_binder_count;
			ast_type->compiling = 0;
			return -1;
		}
		ctx->binder_count = previous_binder_count;
		if (prototype_type_declaration_add_constructor(
				ctx->type_declarations,
				type_id,
				constructor->name_symbol_id,
				compiled_field_types,
				constructor->field_count,
				compiled_result_type,
				parameter_context,
				field_context,
				type_term,
				curried_classifier_cache,
				&constructor_id
			) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		(void)constructor_id;
	}
	if (has_recursive_constructor_field && !has_structural_seed_constructor) {
		ast_type->compiling = 0;
		return -1;
	}
	if (add_compile_type_export(ctx, type_id) != 0) {
		ast_type->compiling = 0;
		return -1;
	}

	ast_type->compiled = 1;
	ast_type->compiling = 0;
	*p_type_id = type_id;
	return 0;
}

static int reduce_type_namespace_term(
	struct compile_context* ctx,
	uint32_t namespace_term,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}

	return prototype_term_normalize_complete_with_profile(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		(int)namespace_term,
		p_ret
	);
}

static int external_type_namespace_name(
	const struct prototype_term_db* terms,
	uint32_t namespace_term,
	struct prototype_qualified_name* p_name
) {
	if (!terms || !p_name || namespace_term >= terms->term_count) {
		return -1;
	}
	uint32_t current = namespace_term;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		*p_name = terms->terms[current].as.external_ref.name;
		return 0;
	}
	return -1;
}

static int imported_owner_arguments(
	const struct prototype_term_db* terms,
	uint32_t owner,
	struct prototype_qualified_name type_name,
	uint32_t* args,
	uint32_t* p_arg_count
);

static int resolve_imported_namespace_member(
	struct compile_context* ctx,
	uint32_t namespace_term,
	int member_symbol_id,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_qualified_name namespace_name;
	if (external_type_namespace_name(
		ctx->terms,
		namespace_term,
		&namespace_name
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		uint32_t type_export_id;
		uint32_t constructor_export_id;
		if (!interface) {
			continue;
		}
		int found_type = prototype_artifact_interface_find_type_export_in_namespace(
			interface,
			namespace_name.namespace_symbol_id,
			namespace_name.name_symbol_id,
			&type_export_id
		);
		if (found_type < 0) {
			return -1;
		}
		if (found_type > 0) {
			continue;
		}
		int found_constructor = prototype_artifact_interface_find_constructor_export(
			interface,
			type_export_id,
			member_symbol_id,
			&constructor_export_id
		);
		if (found_constructor < 0) {
			return -1;
		}
		if (found_constructor > 0) {
			continue;
		}
		uint32_t constructor_term;
		uint32_t owner_arguments[16];
		uint32_t owner_argument_count;
		uint32_t owner;
		const struct prototype_artifact_type_export* type_export =
			&interface->type_exports[type_export_id];
		const struct prototype_artifact_constructor_export* constructor_export =
			&interface->constructor_exports[constructor_export_id];
		if (imported_owner_arguments(
				ctx->terms,
				namespace_term,
				namespace_name,
				owner_arguments,
				&owner_argument_count
			) != 0 || owner_argument_count != type_export->parameter_count ||
			prototype_term_type_instance_make(
				ctx->terms,
				ctx->type_declarations,
				type_export->local_type_id,
				owner_arguments,
				owner_argument_count,
				&owner
			) != 0) {
			return -1;
		}
		if (prototype_term_constructor(
			ctx->terms,
			owner,
			constructor_export->ordinal,
			&constructor_term
		) != 0) {
			return -1;
		}
		if (queue_imported_constructor_classifier(
				ctx,
				constructor_term,
				owner,
				interface,
				type_export_id,
				constructor_export_id
			) != 0) {
			return -1;
		}
		*p_ret = constructor_term;
		return 0;
	}
	return -1;
}

static int resolve_namespace_member(
	struct compile_context* ctx,
	uint32_t namespace_term,
	int member_symbol_id,
	uint32_t* p_ret,
	uint32_t* p_classifier
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}
	if (p_classifier) {
		*p_classifier = PROTOTYPE_INVALID_ID;
	}

	uint32_t evaluated_namespace;
	if (reduce_type_namespace_term(ctx, namespace_term, &evaluated_namespace) != 0) {
		return -1;
	}
	if (evaluated_namespace >= ctx->terms->term_count) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_namespace_args[16];
	uint32_t ignored_namespace_arg_count;
	if (prototype_term_type_instance_info(
		ctx->terms,
		evaluated_namespace,
		&type_id,
		ignored_namespace_args,
		&ignored_namespace_arg_count
	) != 0) {
		int status = resolve_imported_namespace_member(
			ctx,
			evaluated_namespace,
			member_symbol_id,
			p_ret
		);
		return status;
	}

	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(ctx->type_declarations, type_id, member_symbol_id);
	if (!constructor) {
		return -1;
	}
	uint32_t classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < ignored_namespace_arg_count; ++i) {
		uint32_t applied;
		if (prototype_term_app(ctx->terms, classifier, ignored_namespace_args[i], &applied) != 0) {
			return -1;
		}
		classifier = applied;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_constructor(
			ctx->terms,
			namespace_term,
			constructor->constructor_index,
			p_ret
		) != 0) {
		return -1;
	}
	if (queue_declaration_fact(ctx, *p_ret, classifier) != 0) {
		return -1;
	}
	if (p_classifier) {
		*p_classifier = classifier;
	}
	return 0;
}

static int imported_owner_arguments(
	const struct prototype_term_db* terms,
	uint32_t owner,
	struct prototype_qualified_name type_name,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!terms || !args || !p_arg_count || owner >= terms->term_count) {
		return -1;
	}
	uint32_t reversed_args[16];
	uint32_t arg_count = 0;
	uint32_t current = owner;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (arg_count >= 16) {
			return -1;
		}
		reversed_args[arg_count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_EXTERNAL_REF ||
		!qualified_names_equal(terms->terms[current].as.external_ref.name, type_name)) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		args[i] = reversed_args[arg_count - i - 1];
	}
	*p_arg_count = arg_count;
	return 0;
}

static int imported_owner_type_arguments(
	struct prototype_term_db* terms,
	const struct prototype_artifact_type_export* type_export,
	uint32_t owner,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!terms || !type_export || !args || !p_arg_count ||
		owner >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t local_args[16];
	uint32_t local_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			owner,
			&type_id,
			local_args,
			&local_arg_count
		) == 0) {
		if (type_id != type_export->local_type_id ||
			local_arg_count != type_export->parameter_count) {
			return -1;
		}
		for (uint32_t i = 0; i < local_arg_count; ++i) {
			args[i] = local_args[i];
		}
		*p_arg_count = local_arg_count;
		return 0;
	}
	if (imported_owner_arguments(
			terms,
			owner,
			qualified_name_make(
				type_export->namespace_symbol_id,
				type_export->name_symbol_id
			),
			args,
			p_arg_count
		) != 0 ||
		*p_arg_count != type_export->parameter_count) {
		return -1;
	}
	return 0;
}

static const struct prototype_artifact_type_export* imported_type_export_by_local_type(
	const struct prototype_artifact_interface* interface,
	uint32_t type_id
) {
	if (!interface) {
		return NULL;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export =
			&interface->type_exports[i];
		if (export->local_type_id == type_id) {
			return export;
		}
	}
	return NULL;
}

static int external_type_spine_from_imported_instance(
	struct prototype_term_db* terms,
	const struct prototype_artifact_type_export* export,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
) {
	if (!terms || !export || !p_ret || (arg_count > 0 && !args)) {
		return -1;
	}
	uint32_t current;
	if (prototype_term_external_ref(
			terms,
				qualified_name_make(export->namespace_symbol_id, export->name_symbol_id),
			&current
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, current, args[i], &app) != 0) {
			return -1;
		}
		current = app;
	}
	*p_ret = current;
	return 0;
}

static int __attribute__((unused)) rewrite_imported_type_instances_to_external(
	struct prototype_term_db* terms,
	const struct prototype_artifact_interface* interface,
	uint32_t term_id,
	uint32_t* p_ret,
	uint32_t depth
) {
	if (!terms || !interface || !p_ret || term_id >= terms->term_count ||
		depth > 256) {
		return -1;
	}

	uint32_t type_id;
	uint32_t instance_args[16];
	uint32_t instance_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			term_id,
			&type_id,
			instance_args,
			&instance_arg_count
		) == 0) {
		const struct prototype_artifact_type_export* export =
			imported_type_export_by_local_type(interface, type_id);
		if (export) {
			uint32_t rewritten_args[16];
			for (uint32_t i = 0; i < instance_arg_count; ++i) {
				if (rewrite_imported_type_instances_to_external(
						terms,
						interface,
						instance_args[i],
						&rewritten_args[i],
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return external_type_spine_from_imported_instance(
				terms,
				export,
				rewritten_args,
				instance_arg_count,
				p_ret
			);
		}
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.app.function,
					&function,
					depth + 1
				) != 0 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.app.argument,
					&argument,
					depth + 1
				) != 0) {
				return -1;
			}
			if (function == term->as.app.function &&
				argument == term->as.app.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_app(terms, function, argument, p_ret);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.lambda.body,
					&body,
					depth + 1
				) != 0) {
				return -1;
			}
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda(
				terms,
				term->as.lambda.binding_id,
				body,
				p_ret
			);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.pi.domain,
					&domain,
					depth + 1
				) != 0 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.pi.codomain_family,
					&codomain_family,
					depth + 1
				) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain &&
				codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(
				terms,
				domain,
				codomain_family,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.constructor.owner,
					&owner,
					depth + 1
				) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(
				terms,
				owner,
				term->as.constructor.constructor_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t scrutinee;
			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			int changed = 0;
			if (term->as.match.case_count > 64 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.match.scrutinee,
					&scrutinee,
					depth + 1
				) != 0) {
				return -1;
			}
			changed = scrutinee != term->as.match.scrutinee;
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&terms->cases[term->as.match.first_case + i];
				uint32_t body;
				uint32_t constructor_owner = old_case->constructor_owner;
				if (binder_cursor + old_case->binder_count > 256 ||
					rewrite_imported_type_instances_to_external(
						terms,
						interface,
						old_case->body,
						&body,
						depth + 1
					) != 0) {
					return -1;
				}
				if (constructor_owner != PROTOTYPE_INVALID_ID &&
					rewrite_imported_type_instances_to_external(
						terms,
						interface,
						constructor_owner,
						&constructor_owner,
						depth + 1
					) != 0) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binder_storage[binder_cursor + j] =
						terms->case_binders[old_case->first_binder + j];
				}
				if (body != old_case->body ||
					constructor_owner != old_case->constructor_owner) {
					changed = 1;
				}
				case_inputs[i].case_label_symbol_id =
					terms->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = body;
				binder_cursor += old_case->binder_count;
			}
			if (!changed) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_match_with_ih_scope(
				terms,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				term->as.match.ih_scope_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			uint32_t argument;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.induction_hypothesis.argument,
					&argument,
					depth + 1
				) != 0) {
				return -1;
			}
			if (argument == term->as.induction_hypothesis.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				terms,
				term->as.induction_hypothesis.ih_scope_id,
				argument,
				p_ret
			);
		}
		case PROTOTYPE_TERM_RETURN: {
			uint32_t value;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.return_term.value, &value, depth + 1
				) != 0) {
				return -1;
			}
			if (value == term->as.return_term.value) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_return(terms, value, p_ret);
		}
		case PROTOTYPE_TERM_THUNK: {
			uint32_t computation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.thunk.computation, &computation, depth + 1
				) != 0) {
				return -1;
			}
			if (computation == term->as.thunk.computation) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_thunk(terms, computation, p_ret);
		}
		case PROTOTYPE_TERM_FORCE: {
			uint32_t value;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.force.value, &value, depth + 1
				) != 0) {
				return -1;
			}
			if (value == term->as.force.value) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_force(terms, value, p_ret);
		}
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			uint32_t computation;
			uint32_t return_clause;
			uint32_t clause_count = term->as.computation_fold.clause_count;
			struct prototype_computation_fold_clause* clauses =
				calloc(clause_count, sizeof(*clauses));
			if (clause_count > 0 && !clauses) {
				return -1;
			}
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_fold.computation, &computation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_fold.return_clause, &return_clause, depth + 1
				) != 0) {
				free(clauses);
				return -1;
			}
			int unchanged = computation == term->as.computation_fold.computation &&
				return_clause == term->as.computation_fold.return_clause;
			for (uint32_t i = 0; i < clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (rewrite_imported_type_instances_to_external(
						terms, interface, clause->operation, &clauses[i].operation, depth + 1
					) != 0 || rewrite_imported_type_instances_to_external(
						terms, interface, clause->body, &clauses[i].body, depth + 1
					) != 0) {
					free(clauses);
					return -1;
				}
				unchanged = unchanged && clauses[i].operation == clause->operation &&
					clauses[i].body == clause->body;
			}
			int status = unchanged ? (*p_ret = term_id, 0) : prototype_term_computation_fold(
				terms, computation, return_clause, clauses, clause_count, p_ret
			);
			free(clauses);
			return status;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			uint32_t operation;
			uint32_t argument;
			uint32_t continuation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.operation, &operation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.argument, &argument, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.continuation, &continuation, depth + 1
				) != 0) {
				return -1;
			}
			return operation == term->as.operation_request.operation &&
				argument == term->as.operation_request.argument &&
				continuation == term->as.operation_request.continuation ?
				(*p_ret = term_id, 0) : prototype_term_operation_request(
					terms, operation, argument, continuation, p_ret
				);
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
			uint32_t label;
			uint32_t result;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_type.label, &label, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_type.result, &result, depth + 1
				) != 0) {
				return -1;
			}
			if (label == term->as.computation_type.label &&
				result == term->as.computation_type.result) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_computation_type(terms, label, result, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			uint32_t left;
			uint32_t right;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_union.left, &left, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_union.right, &right, depth + 1
				) != 0) {
				return -1;
			}
			return left == term->as.effect_row_union.left && right == term->as.effect_row_union.right ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_union(terms, left, right, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t body;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_forall.body, &body, depth + 1
				) != 0) {
				return -1;
			}
			return body == term->as.effect_row_forall.body ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_forall(
					terms, term->as.effect_row_forall.binding_id, body, p_ret
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION: {
			uint32_t latent_row;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.effect_row_operation.latent_row,
					&latent_row,
					depth + 1
				) != 0) {
				return -1;
			}
			return latent_row == term->as.effect_row_operation.latent_row ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_operation(
					terms, term->as.effect_row_operation.operation_id, latent_row, p_ret
				);
		}
		case PROTOTYPE_TERM_THUNK_TYPE: {
			uint32_t computation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.thunk_type.computation, &computation, depth + 1
				) != 0) {
				return -1;
			}
			if (computation == term->as.thunk_type.computation) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_thunk_type(terms, computation, p_ret);
		}
		default:
			*p_ret = term_id;
			return 0;
	}
}

static int imported_constructor_classifier_from_curried_cache(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_artifact_interface* interface,
	const struct prototype_artifact_type_export* type_export,
	const struct prototype_artifact_constructor_export* constructor_export,
	uint32_t owner,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !type_export || !constructor_export ||
		!p_classifier || constructor_export->curried_classifier_cache == PROTOTYPE_INVALID_ID ||
		constructor_export->curried_classifier_cache >= terms->term_count) {
		return -1;
	}
	uint32_t args[16];
	uint32_t arg_count;
	if (imported_owner_type_arguments(
			terms,
			type_export,
			owner,
			args,
			&arg_count
		) != 0) {
		return -1;
	}
	uint32_t classifier = constructor_export->curried_classifier_cache;
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0) {
		return -1;
	}
	/* constructor_export->curried_classifier_cache was relocated with the provider
	 * graph. Keep that graph-level family intact: an imported term's Pi domain
	 * refers to the same TYPE_VIEW. Rewriting only this path to EXTERNAL_REF
	 * would make an imported constructor fail the ordinary App domain check. */
	(void)interface;
	*p_classifier = classifier;
	return 0;
}

static int compile_phase_infer_imported_constructor_classifiers(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_imported_constructor_classifier_count; ++i) {
		const struct pending_imported_constructor_classifier* pending =
			&ctx->pending_imported_constructor_classifiers[i];
		const struct prototype_artifact_interface* interface = pending->interface;
		const struct prototype_artifact_type_export* type_export =
			&interface->type_exports[pending->type_export_id];
		const struct prototype_artifact_constructor_export* constructor_export =
			&interface->constructor_exports[pending->constructor_export_id];
		uint32_t classifier;
		if (imported_constructor_classifier_from_curried_cache(
				ctx->terms,
				ctx->type_declarations,
				interface,
				type_export,
				constructor_export,
				pending->owner,
				&classifier
			) != 0) {
			return -1;
		}
		if (queue_declaration_fact(
				ctx,
				pending->constructor_term,
				classifier
			) != 0) {
			return -1;
		}
	}
	return 0;
}

/* A surface annotation names a value type unless it already denotes a raw
 * computation type such as Pi. At a computation occurrence, an omitted effect
 * row becomes a fresh elaboration variable. Exact purity is introduced only by
 * an explicit or synthesized RETURN. */
static int compile_expected_classifier_for_ref(
	struct compile_context* ctx,
	const struct compile_ref* ref,
	uint32_t surface_classifier,
	uint32_t* p_expected
) {
	if (!ctx || !ref || !p_expected || surface_classifier >= ctx->terms->term_count) {
		return -1;
	}
	*p_expected = surface_classifier;
	if (ref->polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return 0;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, surface_classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_VALUE) {
		uint32_t row_binder = prototype_term_new_binding(ctx->terms);
		uint32_t effect_row;
		if (row_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_var(
				ctx->terms, row_binder, &effect_row
			) != 0 || prototype_term_computation_type(
				ctx->terms, effect_row, surface_classifier, p_expected
			) != 0) {
			return -1;
		}
	}
	return 0;
}

/* Surface type expectations select a CBPV polarity before graph lowering.
 * A Pi is a negative computation classifier; every other currently supported
 * surface classifier denotes a value. This is deliberately based on the
 * normalized classifier view, rather than on the spelling of an annotation. */
static int compile_ast_against_surface_classifier(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t surface_classifier,
	struct compile_ref* p_ret
) {
	struct prototype_term_classifier_view view;
	int status;
	if (!ctx || !p_ret || surface_classifier >= ctx->terms->term_count ||
		prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, surface_classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return compile_ast_computation_ref(ctx, ast_id, p_ret);
	}
	status = compile_ast_value_ref(ctx, ast_id, p_ret);
	if (status == 0) {
		return 0;
	}
	if (status < 0) {
		return -1;
	}
	/* An expected surface value type also admits a computation that returns
	 * that value. compile_expected_classifier_for_ref later turns `A` into
	 * `Comp({}, A)` for this occurrence. */
	return compile_ast_computation_ref(ctx, ast_id, p_ret);
}

/* A primitive constructor is an n-ary value introduction, not a completed
 * first-class function. */
static int constructor_spine_saturation(
	const struct compile_context* ctx,
	uint32_t term_id
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (!ctx) {
		return -1;
	}
	int spine_status = prototype_term_constructor_spine_info(
		ctx->terms,
		term_id,
		&head,
		&owner,
		&constructor_index,
		arguments,
		64,
		&argument_count
	);
	if (spine_status != 0) {
		return spine_status;
	}
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			ctx->type_declarations,
			ctx->terms,
			owner,
			&type_id,
			owner_arguments,
			64,
			&owner_argument_count
		) != 0 || type_id >= ctx->type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		owner_argument_count != type->parameter_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	const struct prototype_context* parameter_context = prototype_context_get(
		&ctx->metadata->contexts, constructor->parameter_context
	);
	const struct prototype_context* field_context = prototype_context_get(
		&ctx->metadata->contexts, constructor->field_context
	);
	if (!parameter_context || !field_context ||
		field_context->depth < parameter_context->depth) {
		return -1;
	}
	uint32_t declared_field_count =
		field_context->depth - parameter_context->depth;
	(void)head;
	return argument_count == declared_field_count ? 0 :
		(argument_count < declared_field_count ? 2 : -1);
}

/* Partial spine occurrences are builder steps. Each must feed the function
 * edge of the next constructor-formation APP; only a saturated occurrence may
 * escape into any other source operation. */
static int validate_constructor_occurrence_saturation(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		int saturation = constructor_spine_saturation(ctx, operation->core_term);
		if (saturation < 0) {
			return -1;
		}
		if (saturation != 2) {
			continue;
		}
		for (uint32_t parent_id = 0;
			parent_id < ctx->metadata->operation_count;
			++parent_id) {
			const struct prototype_operation_node* parent =
				&ctx->metadata->operations[parent_id];
			if (parent->function == i &&
				(parent->tag != PROTOTYPE_OPERATION_APP ||
				 parent->application_role !=
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION)) {
				return -1;
			}
			if (parent->argument == i || parent->body == i ||
				parent->scrutinee == i) {
				return -1;
			}
			if (parent->tag == PROTOTYPE_OPERATION_MATCH) {
				if (parent->first_case + parent->case_count >
					ctx->metadata->operation_case_count) {
					return -1;
				}
				for (uint32_t case_index = 0;
					case_index < parent->case_count;
					++case_index) {
					if (ctx->metadata->operation_cases[
							parent->first_case + case_index
						].body_operation == i) {
						return -1;
					}
				}
			}
		}
		for (size_t label_id = 0;
			label_id < ctx->metadata->label_count;
			++label_id) {
			if (ctx->metadata->labels[label_id].operation == i) {
				return -1;
			}
		}
		for (size_t assignment_id = 0;
			assignment_id < ctx->asts->assignment_count;
			++assignment_id) {
			const struct prototype_ast_term_assignment_def* assignment =
				&ctx->asts->assignments[assignment_id];
			if (assignment->compiled && assignment->compiled_operation == i) {
				return -1;
			}
		}
	}
	return 0;
}

static int compile_def(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	uint32_t* p_ret
) {
	if (!ctx || !def || !p_ret) {
		return -1;
	}
	if (def->compiled) {
		*p_ret = def->compiled_term;
		return 0;
	}
	if (def->compiling) {
		(void)add_resolve_error(
			ctx,
			PROTOTYPE_RESOLVE_ERROR_RECURSIVE,
			def->name_symbol_id,
			-1,
			def->ast
		);
		return -1;
	}

	def->compiling = 1;
	uint32_t previous_pending_match_resolution_count = ctx->pending_match_resolution_count;
	uint32_t previous_pending_match_typing_count = ctx->pending_match_typing_count;
	uint32_t previous_pending_ascription_check_count =
		ctx->pending_ascription_check_count;
	uint32_t previous_pending_imported_constructor_classifier_count =
		ctx->pending_imported_constructor_classifier_count;
	uint32_t previous_pending_binder_assumption_count =
	ctx->pending_binder_assumption_count;
	uint32_t previous_pending_declaration_fact_count =
		ctx->pending_declaration_fact_count;
	struct compile_ref ref;
	compile_ref_clear(&ref);
	uint32_t surface_expected_classifier = PROTOTYPE_INVALID_ID;
	int has_surface_expectation = lookup_source_expectation_classifier(
		ctx, def->name_symbol_id, &surface_expected_classifier
	) == 0;
	int lower_status;
	if (has_surface_expectation) {
		lower_status = compile_ast_against_surface_classifier(
			ctx, def->ast, surface_expected_classifier, &ref
		);
	} else if (def->ast < ctx->asts->node_count &&
		ctx->asts->nodes[def->ast].tag == PROTOTYPE_AST_LAMBDA) {
		/* Lambda is intrinsically a negative computation in raw CBPV. */
		lower_status = compile_ast_computation_ref(ctx, def->ast, &ref);
	} else {
		lower_status = compile_ast_ref(ctx, def->ast, &ref);
	}
	if (lower_status != 0) {
		ctx->pending_match_resolution_count = previous_pending_match_resolution_count;
		ctx->pending_match_typing_count = previous_pending_match_typing_count;
		ctx->pending_ascription_check_count = previous_pending_ascription_check_count;
		ctx->pending_imported_constructor_classifier_count =
			previous_pending_imported_constructor_classifier_count;
		ctx->pending_binder_assumption_count =
			previous_pending_binder_assumption_count;
		ctx->pending_declaration_fact_count =
			previous_pending_declaration_fact_count;
		def->compiling = 0;
		return -1;
	}
	if (has_surface_expectation) {
		uint32_t expected_classifier = surface_expected_classifier;
		if (compile_expected_classifier_for_ref(
				ctx, &ref, expected_classifier, &expected_classifier
			) != 0) {
			def->compiling = 0;
			return -1;
		}
		uint32_t ascription_operation;
		if (operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				ref.term,
				expected_classifier,
				def->ast,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				ref.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref.classifier = expected_classifier;
		ref.operation = ascription_operation;
	}
	uint32_t declared_classifier;
	if (lookup_external_declaration_classifier(
			ctx,
			def->name_symbol_id,
			&declared_classifier
		) == 0) {
		if (queue_declaration_fact(ctx, ref.term, declared_classifier) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref.classifier = declared_classifier;
		if (ref.operation < ctx->metadata->operation_count) {
			ctx->metadata->operations[ref.operation].known_classifier = declared_classifier;
		}
	}
	if (def->definition_value_required &&
		ref.polarity == COMPILE_REF_POLARITY_COMPUTATION) {
		if (!ctx->metadata || ctx->metadata->definition_thunk_policy !=
			PROTOTYPE_DEFINITION_THUNK_IMPLICIT) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				def->name_symbol_id,
				-1,
				def->ast,
				def->body_span
			);
			def->compiling = 0;
			return -1;
		}
		struct compile_ref quoted;
		if (compile_ref_make_thunk(ctx, &ref, def->ast, &quoted) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref = quoted;
	}
	if (ref.operation < ctx->metadata->operation_count) {
		ctx->metadata->operations[ref.operation].source_symbol_id = def->name_symbol_id;
		ctx->metadata->operations[ref.operation].polarity = ref.polarity;
		ctx->metadata->operations[ref.operation].computation_kind =
			ref.computation_kind;
	}
	def->compiled_term = ref.term;
	def->compiled_classifier = ref.classifier;
	def->compiled_operation = ref.operation;
	def->compiled = 1;
	def->compiling = 0;
	*p_ret = ref.term;
	return 0;
}

static int compile_def_ref(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	struct compile_ref* p_ret
) {
	if (!ctx || !def || !p_ret) {
		return -1;
	}
	uint32_t term;
	if (compile_def(ctx, def, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = def->compiled_classifier;
	if (def->compiled_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	p_ret->polarity = ctx->metadata->operations[def->compiled_operation].polarity;
	p_ret->computation_kind =
		ctx->metadata->operations[def->compiled_operation].computation_kind;
	if (p_ret->classifier == PROTOTYPE_INVALID_ID) {
		(void)lookup_external_declaration_classifier(
			ctx,
			def->name_symbol_id,
			&p_ret->classifier
		);
	}
	p_ret->operation = PROTOTYPE_INVALID_ID;
	if (operation_add(
		ctx,
		PROTOTYPE_OPERATION_NAME,
		term,
		p_ret->classifier,
		def->ast,
		def->compiled_operation,
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
	ctx->metadata->operations[p_ret->operation].source_symbol_id = def->name_symbol_id;
	ctx->metadata->operations[p_ret->operation].polarity = p_ret->polarity;
	ctx->metadata->operations[p_ret->operation].computation_kind =
		p_ret->computation_kind;
	return 0;
}


/* This lowerer is intentionally limited to atomic graph references.  Every
 * source computation form is elaborated through the polarity-aware CBPV
 * lowering below; keeping a second general lowering here would let APP and
 * LAMBDA acquire incompatible operational meanings. */
static int compile_ast_atomic_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	compile_ref_clear(p_ret);
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	switch (node->tag) {
		case PROTOTYPE_AST_TEXT_LITERAL: {
			uint32_t term;
			if (prototype_term_text_literal(
					ctx->terms, node->as.text_literal.text_symbol_id, &term
				) != 0) {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_INT_LITERAL: {
			uint32_t term;
			if (prototype_term_int_literal(
					ctx->terms, node->as.int_literal.value, &term
				) != 0) {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_SYSTEM_NAME: {
			uint32_t term;
			if (node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_HOST_TYPE) {
				if (prototype_term_make_host_type(
						ctx->terms, node->as.system_name.host_type_id, &term
					) != 0) {
					return -1;
				}
			} else if (
				node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_PURE_PRIMITIVE &&
				node->as.system_name.pure_primitive_id !=
					PROTOTYPE_PURE_PRIMITIVE_UNKNOWN
			) {
				if (prototype_term_pure_primitive(
						ctx->terms,
						node->as.system_name.pure_primitive_id,
						node->as.system_name.type_symbol_id,
						&term
					) != 0) {
					return -1;
				}
			} else if (
				node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION &&
				node->as.system_name.effect_operation_id != PROTOTYPE_EFFECT_OPERATION_UNKNOWN
			) {
				if (prototype_term_effect_operation(
						ctx->terms,
						node->as.system_name.effect_operation_id,
						&term
					) != 0) {
					return -1;
				}
			} else {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		{
			uint32_t binding_id;
			uint32_t ih_scope_id;
			uint32_t argument_term;
			uint32_t argument_operation;
			uint32_t ih_term;
			if (lookup_graph_binder(
					ctx, node->as.induction_hypothesis.ast_binder_id, &binding_id
				) != 0 ||
				lookup_ih_scope_id(
					ctx, node->as.induction_hypothesis.ast_binder_id, &ih_scope_id
				) != 0 ||
				prototype_term_var(ctx->terms, binding_id, &argument_term) != 0 ||
				operation_add(
					ctx, PROTOTYPE_OPERATION_VAR, argument_term, PROTOTYPE_INVALID_ID,
					ast_id, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
					&argument_operation
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, ih_scope_id, argument_term, &ih_term
				) != 0) {
				return -1;
			}
			ctx->metadata->operations[argument_operation].referenced_ast_binder_id =
				node->as.induction_hypothesis.ast_binder_id;
			p_ret->term = ih_term;
			p_ret->classifier = PROTOTYPE_INVALID_ID;
			p_ret->polarity = COMPILE_REF_POLARITY_UNKNOWN;
			p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
			return operation_add(
				ctx, PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS, ih_term,
				PROTOTYPE_INVALID_ID, ast_id, PROTOTYPE_INVALID_ID,
				argument_operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, ih_scope_id, 0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_VAR: {
			const struct local_ref_map_entry* local = lookup_local_ref(
				ctx, node->as.var.ast_binder_id
			);
			if (local) {
				*p_ret = local->ref;
				if (operation_add(
						ctx, PROTOTYPE_OPERATION_NAME, local->ref.term,
						local->ref.classifier, ast_id, local->ref.operation,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, 0, &p_ret->operation
					) != 0) {
					return -1;
				}
				ctx->metadata->operations[p_ret->operation].source_symbol_id =
					local->symbol_id;
				ctx->metadata->operations[p_ret->operation].referenced_ast_binder_id =
					node->as.var.ast_binder_id;
				ctx->metadata->operations[p_ret->operation].polarity = p_ret->polarity;
				ctx->metadata->operations[p_ret->operation].computation_kind =
					p_ret->computation_kind;
				return 0;
			}
			uint32_t binding_id;
			uint32_t classifier;
			uint32_t term;
			if (lookup_graph_binder(ctx, node->as.var.ast_binder_id, &binding_id) != 0 ||
				prototype_term_var(ctx->terms, binding_id, &term) != 0) {
				return -1;
			}
			if (lookup_graph_binder_classifier(ctx, node->as.var.ast_binder_id, &classifier) != 0) {
				classifier = PROTOTYPE_INVALID_ID;
			}
			p_ret->term = term;
			p_ret->classifier = classifier;
			p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
			p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
			if (operation_add(
				ctx, PROTOTYPE_OPERATION_VAR, term, classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
				) != 0) {
				return -1;
			}
			ctx->metadata->operations[p_ret->operation].referenced_ast_binder_id =
				node->as.var.ast_binder_id;
			(void)lookup_graph_binder_symbol(
				ctx,
				node->as.var.ast_binder_id,
				&ctx->metadata->operations[p_ret->operation].source_symbol_id
			);
			return 0;
		}
		case PROTOTYPE_AST_ASCRIPTION:
		{
			uint32_t expected_classifier;
			struct compile_ref inner;
			if (compile_ast_ref(ctx, node->as.ascription.term, &inner) != 0 ||
				compile_ast_ascription_classifier(
					ctx, node->as.ascription.type_expr, &expected_classifier
				) != 0 ||
					queue_ascription_check(
						ctx, inner.term, expected_classifier, ast_id, inner.operation
						) != 0) {
					return -1;
				}
			*p_ret = inner;
			return 0;
		}
		case PROTOTYPE_AST_NAME:
		{
			struct prototype_ast_term_assignment_def* def;
			int status = resolve_unique_assignment_if_present(ctx, node->as.name.symbol_id, &def);
			if (status == 1) {
				return compile_external_ref_ref(ctx, node->as.name.symbol_id, p_ret);
			}
			if (status != 0) {
				(void)add_resolve_error(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT,
					node->as.name.symbol_id,
					-1,
					ast_id
				);
				return -1;
			}
			return compile_def_ref(ctx, def, p_ret);
		}
		case PROTOTYPE_AST_NAME_IN_NAMESPACE: {
			struct prototype_ast_term_assignment_def* def;
			uint32_t namespace_term;
			uint32_t constructor_term;
			uint32_t classifier;
			int status = resolve_unique_assignment_if_present(
				ctx,
				node->as.name_in_namespace.namespace_symbol_id,
				&def
			);
			if (status == 1) {
				status = compile_type_declaration_term_by_symbol(
					ctx,
					node->as.name_in_namespace.namespace_symbol_id,
					&namespace_term
				);
			} else if (status == 0) {
				status = compile_def(ctx, def, &namespace_term);
			}
			if (status != 0 || resolve_namespace_member(
					ctx,
					namespace_term,
					node->as.name_in_namespace.symbol_id,
					&constructor_term,
					&classifier
				) != 0) {
				return -1;
			}
			p_ret->term = constructor_term;
			p_ret->classifier = classifier;
			p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
			p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
			return operation_add(
				ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE: {
			struct compile_ref namespace_ref;
			uint32_t constructor_term;
			uint32_t classifier;
			if (compile_ast_ref(
					ctx,
					node->as.name_in_ast_namespace.namespace_ast,
					&namespace_ref
				) != 0 ||
				resolve_namespace_member(
					ctx,
					namespace_ref.term,
					node->as.name_in_ast_namespace.symbol_id,
					&constructor_term,
					&classifier
				) != 0) {
				return -1;
			}
			p_ret->term = constructor_term;
			p_ret->classifier = classifier;
			p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
			p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
			return operation_add(
				ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
				namespace_ref.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_TYPE_FORMATION: {
			uint32_t previous_binder_count = ctx->binder_count;
			uint32_t type_id;
			const struct prototype_ast_type_def* ast_type =
				&ctx->asts->type_defs[node->as.type_formation.ast_type_def_id];
			if (ast_type->parameter_count > 32) {
				return -1;
			}
			for (uint32_t i = 0; i < ast_type->parameter_count; ++i) {
				const struct prototype_ast_type_parameter* parameter =
					&ctx->asts->type_parameters[ast_type->first_parameter + i];
				uint32_t binding_id;
				uint32_t binder_classifier;
				if (push_graph_binder(
						ctx,
						parameter->ast_binder_id,
						PROTOTYPE_INVALID_ID,
						parameter->name_symbol_id,
						&binding_id
					) != 0 || compile_ast_type_expr_term(
						ctx, parameter->type_expr, &binder_classifier
					) != 0) {
					ctx->binder_count = previous_binder_count;
					return -1;
				}
				if (resolve_current_graph_binder_classifier(
						ctx, binder_classifier
					) != 0) {
					ctx->binder_count = previous_binder_count;
					return -1;
				}
			}
			if (compile_ast_type_def(
					ctx, node->as.type_formation.ast_type_def_id, &type_id
				) != 0 || prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, type_id, NULL, 0, &p_ret->term
				) != 0) {
				ctx->binder_count = previous_binder_count;
				return -1;
			}
			ctx->binder_count = previous_binder_count;
			return compile_ref_from_term(ctx, p_ret->term, p_ret);
		}
		case PROTOTYPE_AST_TYPE_LITERAL: {
			uint32_t type_id;
			if (compile_ast_type_def(
					ctx, node->as.type_literal.ast_type_def_id, &type_id
				) != 0 || prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, type_id, NULL, 0, &p_ret->term
				) != 0) {
				return -1;
			}
			return compile_ref_from_term(ctx, p_ret->term, p_ret);
		}
		default:
			return -1;
	}
}

typedef int (*compile_value_continuation_fn)(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
);

struct compile_value_continuation {
	compile_value_continuation_fn apply;
	void* data;
	int has_source_binder;
	uint32_t source_ast_binder_id;
	int source_binder_symbol_id;
	uint32_t source_binder_classifier;
};

static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_runtime_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
);

static int compile_ast_constructor_spine_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
);

static int compile_ast_constructor_application_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_computation_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
);

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ref_make_operation_request(
	struct compile_context* ctx,
	const struct compile_ref* operation,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
);

static int ast_application_head_is_constructor(
	const struct compile_context* ctx,
	uint32_t ast_id
);

/* A type-family argument is a value-level thunk of a pure computation
 * function. In a type formation context we expose its lambda only to
 * PURE_TYPE_WHNF; this is not a runtime FORCE and it cannot dispatch an
 * operation. The resulting application must still normalize to a type before
 * any classifier rule accepts it. */
static void compile_ref_open_pure_type_family(
	struct compile_context* ctx,
	struct compile_ref* ref
) {
	uint32_t computation;
	if (!ctx || !ref || ref->term >= ctx->terms->term_count ||
		ctx->terms->terms[ref->term].tag != PROTOTYPE_TERM_THUNK) {
		return;
	}
	computation = ctx->terms->terms[ref->term].as.thunk.computation;
	if (computation >= ctx->terms->term_count ||
		ctx->terms->terms[computation].tag != PROTOTYPE_TERM_LAMBDA) {
		return;
	}
	ref->term = computation;
	ref->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	ref->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
}

static int compile_ast_pure_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	struct compile_ref value;
	int status;
	uint32_t whnf;
	uint32_t result;
	if (!ctx || !p_ret) {
		return -1;
	}
	if (ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_APP) {
		struct compile_ref function;
		struct compile_ref argument;
		uint32_t application;
		if (compile_ast_pure_value_ref(ctx, node->as.app.function, &function) != 0 ||
			compile_ast_pure_value_ref(ctx, node->as.app.argument, &argument) != 0) {
			return -1;
		}
		compile_ref_open_pure_type_family(ctx, &function);
		compile_ref_open_pure_type_family(ctx, &argument);
		if (compile_shared_app(ctx, function.term, argument.term, &application) != 0 ||
			prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				application,
				&whnf
			) != 0) {
			return -1;
		}
		if (whnf < ctx->terms->term_count &&
			ctx->terms->terms[whnf].tag == PROTOTYPE_TERM_RETURN) {
			whnf = ctx->terms->terms[whnf].as.return_term.value;
		}
		return compile_ref_from_term(ctx, whnf, p_ret);
	}
	status = compile_ast_value_ref(ctx, ast_id, &value);
	if (status == 0) {
		*p_ret = value;
		return 0;
	}
	if (status < 0 || compile_ast_computation_ref(ctx, ast_id, &value) != 0 ||
		value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	/* A type formation is a pure evaluation context. A raw lambda is the
	 * function itself here, rather than a computation whose returned value must
	 * be extracted. Its later APP is reduced under PURE_TYPE_WHNF. */
	if (value.term < ctx->terms->term_count &&
		ctx->terms->terms[value.term].tag == PROTOTYPE_TERM_LAMBDA) {
		*p_ret = value;
		return 0;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			value.term,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	result = ctx->terms->terms[whnf].as.return_term.value;
	return compile_ref_from_term(ctx, result, p_ret);
}

static int compile_ref_make_return(
	struct compile_context* ctx,
	const struct compile_ref* value,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !value || !p_ret ||
		prototype_term_return(ctx->terms, value->term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_RETURNING;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_RETURN, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, value->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_make_force(
	struct compile_context* ctx,
	const struct compile_ref* value,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	uint32_t computation_classifier = PROTOTYPE_INVALID_ID;
	int computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (!ctx || !value || !p_ret ||
		prototype_term_force(ctx->terms, value->term, &term) != 0) {
		return -1;
	}
	/* An execution or callee demand opens this occurrence exactly once. When the
	 * value has a known Thunk(Pi(...)) classifier, retain that negative shape so
	 * a following APP applies it directly instead of sequencing it as a
	 * returning computation. */
	if (value->classifier < ctx->terms->term_count &&
		ctx->terms->terms[value->classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		struct prototype_term_classifier_view view;
		computation_classifier =
			ctx->terms->terms[value->classifier].as.thunk_type.computation;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				computation_classifier,
				&view
			) != 0) {
			return -1;
		}
		if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
			computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
		} else if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			computation_kind = COMPILE_REF_COMPUTATION_KIND_RETURNING;
		}
	}
	p_ret->term = term;
	p_ret->classifier = computation_classifier;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = computation_kind;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_FORCE, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, value->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

/* A raw source LAMBDA is a CBPV computation function.  A use in a value
 * position must suspend that computation; this is an elaboration boundary,
 * not a second source-level function representation. */
static int compile_ref_make_thunk(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !computation || !p_ret ||
		computation->polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		prototype_term_thunk(ctx->terms, computation->term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	uint32_t immediate_classifier;
	if (computation->classifier != PROTOTYPE_INVALID_ID &&
		prototype_judgement_cbpv_boundary_classifier(
			ctx->terms,
			ctx->type_declarations,
			term,
			computation->classifier,
			&immediate_classifier
		) == 0) {
		p_ret->classifier = immediate_classifier;
	}
	p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_THUNK, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, computation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_make_sequence_fold(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	const struct compile_ref* continuation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !computation || !continuation || !p_ret ||
		prototype_term_computation_fold(
			ctx->terms, computation->term, continuation->term, NULL, 0, &term
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_RETURNING;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_COMPUTATION_FOLD, term, PROTOTYPE_INVALID_ID, source_ast,
		computation->operation, continuation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_thunk_computation_kind(
	const struct compile_context* ctx,
	const struct compile_ref* ref
) {
	if (!ctx || !ctx->metadata || !ref ||
		ref->operation >= ctx->metadata->operation_count) {
		return COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	}
	uint32_t operation_id = operation_runtime_unwrap_name(
		ctx->metadata, ref->operation
	);
	if (operation_id >= ctx->metadata->operation_count) {
		return COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_THUNK ||
		operation->argument >= ctx->metadata->operation_count) {
		return COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	}
	operation_id = operation_runtime_unwrap_name(ctx->metadata, operation->argument);
	if (operation_id >= ctx->metadata->operation_count) {
		return COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	}
	const struct prototype_operation_node* computation =
		&ctx->metadata->operations[operation_id];
	switch (computation->tag) {
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_REQUEST:
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			return COMPILE_REF_COMPUTATION_KIND_RETURNING;
		case PROTOTYPE_OPERATION_LAMBDA:
			return COMPILE_REF_COMPUTATION_KIND_FUNCTION;
		default:
			return computation->computation_kind;
	}
}

static int compile_ref_is_returning_thunk(
	const struct compile_context* ctx,
	const struct compile_ref* ref,
	int* p_ret
) {
	if (!ctx || !ref || !p_ret) {
		return -1;
	}
	*p_ret = compile_ref_thunk_computation_kind(ctx, ref) ==
		COMPILE_REF_COMPUTATION_KIND_RETURNING;
	if (*p_ret || ref->classifier >= ctx->terms->term_count ||
		ctx->terms->terms[ref->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	struct prototype_term_classifier_view view;
	uint32_t computation_classifier =
		ctx->terms->terms[ref->classifier].as.thunk_type.computation;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			computation_classifier,
			&view
		) != 0) {
		return -1;
	}
	*p_ret = view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	return 0;
}

/* A runtime strict value context evaluates a returning computation exactly
 * once, then supplies its result to the consumer. Source block bindings retain
 * their AST binder and scope-slot graph binder; other strict contexts allocate
 * a synthetic binder. Both remain occurrence-specific in OperationGraph even
 * when canonical TermDB lambdas share a core node. */
static int compile_continue_runtime_computation(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	uint32_t ast_binder_id;
	uint32_t binding_id;
	uint32_t variable_term;
	uint32_t lambda_term;
	uint32_t continuation_context;
	uint32_t saved_binder_count;
	struct compile_ref variable;
	struct compile_ref body;
	struct compile_ref lambda;
	if (!ctx || !computation || !continuation || !continuation->apply || !p_ret ||
		computation->polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		ctx->binder_count >= 512) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	if (continuation->has_source_binder) {
		ast_binder_id = continuation->source_ast_binder_id;
		if (push_graph_binder(
				ctx,
				ast_binder_id,
				continuation->source_binder_classifier,
				continuation->source_binder_symbol_id,
				&binding_id
			) != 0) {
			return -1;
		}
	} else {
		ast_binder_id = prototype_ast_new_binder(ctx->asts);
		binding_id = prototype_term_new_binding(ctx->terms);
		if (ast_binder_id == PROTOTYPE_INVALID_ID ||
			binding_id == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&ctx->metadata->contexts,
			ctx->context_ids[saved_binder_count],
			binding_id,
			PROTOTYPE_INVALID_ID,
			ast_binder_id,
			&continuation_context
		) != 0) {
			return -1;
		}
		ctx->binders[saved_binder_count].ast_binder_id = ast_binder_id;
		ctx->binders[saved_binder_count].graph_binder_id = binding_id;
		ctx->binders[saved_binder_count].classifier = PROTOTYPE_INVALID_ID;
		ctx->binders[saved_binder_count].symbol_id = -1;
		ctx->binder_count++;
		ctx->context_ids[ctx->binder_count] = continuation_context;
	}
	if (prototype_term_var(ctx->terms, binding_id, &variable_term) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	variable.term = variable_term;
	variable.classifier = continuation->has_source_binder ?
		continuation->source_binder_classifier : PROTOTYPE_INVALID_ID;
	variable.polarity = COMPILE_REF_POLARITY_VALUE;
	variable.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, variable_term, variable.classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &variable.operation
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->metadata->operations[variable.operation].referenced_ast_binder_id =
		ast_binder_id;
	if (continuation->apply(ctx, &variable, continuation->data, &body) != 0 ||
		body.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		prototype_term_lambda(ctx->terms, binding_id, body.term, &lambda_term) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	lambda.term = lambda_term;
	lambda.classifier = PROTOTYPE_INVALID_ID;
	lambda.polarity = COMPILE_REF_POLARITY_COMPUTATION;
	lambda.computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID,
			continuation->has_source_binder ?
				continuation->source_binder_classifier : PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0,
			&lambda.operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[lambda.operation].referenced_ast_binder_id =
		ast_binder_id;
	ctx->metadata->operations[lambda.operation].binding_id = binding_id;
	ctx->metadata->operations[lambda.operation].binder_symbol_id =
		continuation->has_source_binder ?
			continuation->source_binder_symbol_id : -1;
	if (continuation->has_source_binder &&
		continuation->source_binder_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t binder_var;
		if (prototype_term_var(
				ctx->terms, binding_id, &binder_var
			) != 0 || queue_binder_assumption(
				ctx,
				ctx->metadata->operations[body.operation].context_id,
				binder_var,
				continuation->source_binder_classifier,
				lambda.operation
			) != 0) {
			return -1;
		}
	}
	return compile_ref_make_sequence_fold(ctx, computation, &lambda, source_ast, p_ret);
}

static int compile_return_continuation(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	uint32_t source_ast = *(const uint32_t*)data;
	return compile_ref_make_return(ctx, value, source_ast, p_ret);
}

static int term_app_head_is_constructor(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	return prototype_term_constructor_spine_info(
		terms,
		term_id,
		&head,
		&owner,
		&constructor_id,
		arguments,
		64,
		&argument_count
	) == 0;
}

static int ast_application_head_is_constructor(
	const struct compile_context* ctx,
	uint32_t ast_id
) {
	if (!ctx || !ctx->asts || ast_id >= ctx->asts->node_count) {
		return 0;
	}
	while (ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_APP) {
		ast_id = ctx->asts->nodes[ast_id].as.app.function;
		if (ast_id >= ctx->asts->node_count) {
			return 0;
		}
	}
	return ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_NAME_IN_NAMESPACE ||
		ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE;
}

/* A constructor spine remains a value even when it is partially applied.
 * Other computation-shaped source forms need raw lowering here so that a
 * Pi-producing computation is not first thunked and immediately forced. */
static int compile_ast_function_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_APP &&
		ast_application_head_is_constructor(ctx, ast_id)) {
		return compile_ast_ref(ctx, ast_id, p_ret);
	}
	switch (node->tag) {
		case PROTOTYPE_AST_APP:
		case PROTOTYPE_AST_LAMBDA:
		case PROTOTYPE_AST_MATCH:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
			return compile_ast_computation_ref(ctx, ast_id, p_ret);
		default:
			return compile_ast_ref(ctx, ast_id, p_ret);
	}
}

enum compile_intrinsic_spine_kind {
	COMPILE_INTRINSIC_SPINE_NONE = 0,
	COMPILE_INTRINSIC_SPINE_PURE_PRIMITIVE,
	COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION
};

static int compile_intrinsic_spine_info(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	int* p_kind,
	uint32_t* p_argument_count,
	uint32_t* p_declared_arity
) {
	if (!terms || !p_kind || !p_argument_count || !p_declared_arity ||
		term_id >= terms->term_count) {
		return -1;
	}
	uint32_t argument_count = 0;
	while (term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		term_id = terms->terms[term_id].as.app.function;
		argument_count++;
	}
	if (term_id >= terms->term_count) {
		return -1;
	}
	if (terms->terms[term_id].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		const struct prototype_pure_primitive_declaration* declaration =
			prototype_term_pure_primitive_declaration(
				terms->terms[term_id].as.pure_primitive.primitive_id
			);
		if (!declaration) {
			return -1;
		}
		*p_kind = COMPILE_INTRINSIC_SPINE_PURE_PRIMITIVE;
		*p_argument_count = argument_count;
		*p_declared_arity = declaration->arity;
		return 0;
	}
	if (terms->terms[term_id].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(
				terms->terms[term_id].as.effect_operation.operation_id
			);
		if (!declaration) {
			return -1;
		}
		*p_kind = COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION;
		*p_argument_count = argument_count;
		*p_declared_arity = declaration->arity;
		return 0;
	}
	*p_kind = COMPILE_INTRINSIC_SPINE_NONE;
	*p_argument_count = argument_count;
	*p_declared_arity = 0;
	return 0;
}

static int compile_application_make_computation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	struct compile_ref applied_function;
	int intrinsic_kind;
	uint32_t intrinsic_argument_count;
	uint32_t intrinsic_arity;
	uint32_t term;
	if (!ctx || !function || !argument || !p_ret) {
		return -1;
	}
	if (term_app_head_is_constructor(ctx->terms, function->term)) {
		struct compile_ref value;
		if (compile_shared_app(ctx, function->term, argument->term, &term) != 0 ||
			operation_add(
				ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
				source_ast, function->operation, argument->operation,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, 0, &value.operation
			) != 0) {
			return -1;
		}
		value.term = term;
		value.classifier = PROTOTYPE_INVALID_ID;
		value.polarity = COMPILE_REF_POLARITY_VALUE;
		value.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
		return compile_ref_make_return(ctx, &value, source_ast, p_ret);
	}
	if (compile_intrinsic_spine_info(
			ctx->terms,
			function->term,
			&intrinsic_kind,
			&intrinsic_argument_count,
			&intrinsic_arity
		) != 0 || (intrinsic_kind != COMPILE_INTRINSIC_SPINE_NONE &&
			intrinsic_argument_count >= intrinsic_arity)) {
		return -1;
	}
	if (intrinsic_kind == COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION &&
		intrinsic_argument_count + 1 == intrinsic_arity) {
		return compile_ref_make_operation_request(
			ctx, function, argument, source_ast, p_ret
		);
	}
	if (intrinsic_kind != COMPILE_INTRINSIC_SPINE_NONE) {
		if (compile_shared_app(ctx, function->term, argument->term, &term) != 0) {
			return -1;
		}
		p_ret->term = term;
		p_ret->classifier = PROTOTYPE_INVALID_ID;
		if (function->classifier != PROTOTYPE_INVALID_ID &&
			argument->classifier != PROTOTYPE_INVALID_ID &&
			operation_apply_classifier(
				ctx,
				function->classifier,
				argument->classifier,
				argument->term,
				&p_ret->classifier
			) != 0) {
			return -1;
		}
		p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
		p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
		return operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, p_ret->classifier,
			source_ast, function->operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		);
	}
	applied_function = *function;
	if (function->polarity == COMPILE_REF_POLARITY_VALUE) {
		if (compile_ref_make_force(ctx, function, source_ast, &applied_function) != 0) {
			return -1;
		}
	} else if (function->polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	if (compile_shared_app(ctx, applied_function.term, argument->term, &term) != 0 ||
		operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
			source_ast, applied_function.operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	return 0;
}

struct compile_application_spine_context {
	uint32_t arguments[64];
	uint32_t applications[64];
	uint32_t argument_count;
};

struct compile_application_spine_step_context {
	const struct compile_application_spine_context* spine;
	struct compile_ref function;
	uint32_t index;
};

static int compile_application_spine_step(
	struct compile_context* ctx,
	const struct compile_application_spine_context* spine,
	const struct compile_ref* function,
	uint32_t index,
	struct compile_ref* p_ret
);

static int compile_application_spine_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_application_spine_step_context* context = data;
	struct compile_ref application;
	if (!ctx || !argument || !context || !context->spine || !p_ret ||
		context->index >= context->spine->argument_count ||
		compile_application_make_computation(
			ctx,
			&context->function,
			argument,
			context->spine->applications[context->index],
			&application
		) != 0) {
		return -1;
	}
	return compile_application_spine_step(
		ctx, context->spine, &application, context->index + 1, p_ret
	);
}

static int compile_application_spine_step(
	struct compile_context* ctx,
	const struct compile_application_spine_context* spine,
	const struct compile_ref* function,
	uint32_t index,
	struct compile_ref* p_ret
) {
	if (!ctx || !spine || !function || !p_ret || index > spine->argument_count) {
		return -1;
	}
	if (index == spine->argument_count) {
		*p_ret = *function;
		return function->polarity == COMPILE_REF_POLARITY_COMPUTATION ? 0 : -1;
	}
	struct compile_application_spine_step_context context = {
		spine, *function, index
	};
	struct compile_value_continuation continuation = {
		.apply = compile_application_spine_argument_continuation,
		.data = &context
	};
	return compile_ast_runtime_value_then(
		ctx, spine->arguments[index], &continuation, p_ret
	);
}

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	struct compile_application_spine_context spine;
	struct compile_ref function;
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	memset(&spine, 0, sizeof(spine));
	uint32_t head = ast_id;
	while (head < ctx->asts->node_count &&
		ctx->asts->nodes[head].tag == PROTOTYPE_AST_APP) {
		if (spine.argument_count >= 64) {
			return -1;
		}
		spine.arguments[spine.argument_count] =
			ctx->asts->nodes[head].as.app.argument;
		spine.applications[spine.argument_count] = head;
		spine.argument_count++;
		head = ctx->asts->nodes[head].as.app.function;
	}
	if (head >= ctx->asts->node_count || spine.argument_count == 0) {
		return -1;
	}
	for (uint32_t i = 0; i < spine.argument_count / 2; ++i) {
		uint32_t other = spine.argument_count - i - 1;
		uint32_t argument = spine.arguments[i];
		uint32_t application = spine.applications[i];
		spine.arguments[i] = spine.arguments[other];
		spine.applications[i] = spine.applications[other];
		spine.arguments[other] = argument;
		spine.applications[other] = application;
	}
	if (compile_ast_function_ref(ctx, head, &function) != 0) {
		return -1;
	}
	return compile_application_spine_step(ctx, &spine, &function, 0, p_ret);
}

enum ast_lambda_exit_presence {
	AST_LAMBDA_EXIT_NONE = 0,
	AST_LAMBDA_EXIT_FOUND,
	AST_LAMBDA_EXIT_FORBIDDEN
};

static int ast_lambda_exit_merge(int left, int right) {
	if (left == AST_LAMBDA_EXIT_FORBIDDEN || right == AST_LAMBDA_EXIT_FORBIDDEN) {
		return AST_LAMBDA_EXIT_FORBIDDEN;
	}
	return left == AST_LAMBDA_EXIT_FOUND || right == AST_LAMBDA_EXIT_FOUND ?
		AST_LAMBDA_EXIT_FOUND : AST_LAMBDA_EXIT_NONE;
}

static int ast_lambda_exit_presence_in(
	const struct prototype_ast_db* asts,
	uint32_t ast_id
) {
	if (!asts || ast_id >= asts->node_count) {
		return AST_LAMBDA_EXIT_FORBIDDEN;
	}
	const struct prototype_ast_node* node = &asts->nodes[ast_id];
	switch (node->tag) {
		case PROTOTYPE_AST_LAMBDA:
			return AST_LAMBDA_EXIT_NONE;
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return ast_lambda_exit_presence_in(
				asts, node->as.block_lambda_exit.value
			) == AST_LAMBDA_EXIT_NONE ?
				AST_LAMBDA_EXIT_FOUND : AST_LAMBDA_EXIT_FORBIDDEN;
		case PROTOTYPE_AST_COMPUTATION_BLOCK: {
			if (node->as.block.item_count == 0 ||
				node->as.block.result_item_index >= node->as.block.item_count ||
				(size_t)node->as.block.first_item + node->as.block.item_count >
					asts->block_item_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			int result = AST_LAMBDA_EXIT_NONE;
			for (uint32_t i = 0; i <= node->as.block.result_item_index; ++i) {
				uint32_t item_id = asts->block_items[node->as.block.first_item + i];
				if (item_id >= asts->node_count) {
					return AST_LAMBDA_EXIT_FORBIDDEN;
				}
				const struct prototype_ast_node* item = &asts->nodes[item_id];
				uint32_t child;
				switch (item->tag) {
					case PROTOTYPE_AST_BLOCK_BINDING:
						child = item->as.block_binding.value;
						break;
					case PROTOTYPE_AST_BLOCK_EXPRESSION:
						child = item->as.block_expression.term;
						break;
					case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
						child = item_id;
						break;
					default:
						return AST_LAMBDA_EXIT_FORBIDDEN;
				}
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, child)
				);
				if (result == AST_LAMBDA_EXIT_FORBIDDEN) {
					return result;
				}
			}
			return result;
		}
		case PROTOTYPE_AST_MATCH: {
			int result = ast_lambda_exit_presence_in(asts, node->as.match.scrutinee);
			if ((size_t)node->as.match.first_case + node->as.match.case_count >
				asts->case_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				result = ast_lambda_exit_merge(
					result,
					ast_lambda_exit_presence_in(
						asts, asts->cases[node->as.match.first_case + i].body
					)
				);
			}
			return result;
		}
		case PROTOTYPE_AST_APP:
			return ast_lambda_exit_merge(
				ast_lambda_exit_presence_in(asts, node->as.app.function),
				ast_lambda_exit_presence_in(asts, node->as.app.argument)
			);
		case PROTOTYPE_AST_ASCRIPTION:
			return ast_lambda_exit_presence_in(asts, node->as.ascription.term);
		case PROTOTYPE_AST_QUOTE:
			return ast_lambda_exit_presence_in(asts, node->as.unary.term) ==
				AST_LAMBDA_EXIT_NONE ? AST_LAMBDA_EXIT_NONE :
				AST_LAMBDA_EXIT_FORBIDDEN;
		case PROTOTYPE_AST_COMPUTATION_FOLD: {
			int result = ast_lambda_exit_presence_in(
				asts, node->as.computation_fold.computation
			);
			if ((size_t)node->as.computation_fold.first_clause +
				node->as.computation_fold.clause_count >
				asts->computation_fold_clause_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
				const struct prototype_ast_computation_fold_clause* clause =
					&asts->computation_fold_clauses[
						node->as.computation_fold.first_clause + i
					];
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, clause->operation)
				);
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, clause->body)
				);
			}
			result = ast_lambda_exit_merge(
				result,
				ast_lambda_exit_presence_in(asts, node->as.computation_fold.return_body)
			);
			return result == AST_LAMBDA_EXIT_NONE ? AST_LAMBDA_EXIT_NONE :
				AST_LAMBDA_EXIT_FORBIDDEN;
		}
		default:
			return AST_LAMBDA_EXIT_NONE;
	}
}

static int compile_ast_lambda_computation_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t saved_binder_count;
	uint32_t binder_classifier;
	uint32_t binding_id;
	struct compile_ref body;
	uint32_t lambda_term;
	uint32_t lambda_operation;
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_LAMBDA) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	int binder_has_latent_effect =
		node->as.lambda.binder_type < ctx->asts->type_expr_count &&
		(ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_PI ||
			 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
				 PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE);
	int exit_presence = ast_lambda_exit_presence_in(ctx->asts, node->as.lambda.body);
	if (exit_presence == AST_LAMBDA_EXIT_FORBIDDEN ||
		(binder_has_latent_effect ? compile_ast_binder_value_type_with_latent_effect_row(
				ctx, node->as.lambda.binder_type, &(uint32_t){ PROTOTYPE_INVALID_ID },
				&binder_classifier
			) : compile_ast_type_expr_term(
				ctx, node->as.lambda.binder_type, &binder_classifier
			)) != 0 ||
		push_graph_binder(
			ctx, node->as.lambda.ast_binder_id, binder_classifier,
			node->as.lambda.binder_symbol_id, &binding_id
		) != 0 || (exit_presence == AST_LAMBDA_EXIT_FOUND ?
			compile_ast_computation_control_ref(
				ctx, node->as.lambda.body, NULL, NULL, &body
			) : compile_ast_computation_ref(
				ctx, node->as.lambda.body, &body
			)) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (body.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		prototype_term_lambda(ctx->terms, binding_id, body.term, &lambda_term) != 0) {
		return -1;
	}
	p_ret->term = lambda_term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID, binder_classifier, PROTOTYPE_INVALID_ID, 0,
			&lambda_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[lambda_operation].binder_symbol_id =
		node->as.lambda.binder_symbol_id;
	ctx->metadata->operations[lambda_operation].referenced_ast_binder_id =
		node->as.lambda.ast_binder_id;
	ctx->metadata->operations[lambda_operation].binding_id = binding_id;
	struct prototype_operation_node* lambda_operation_node =
		&ctx->metadata->operations[lambda_operation];
	for (uint32_t i = 0; i < (uint32_t)ctx->terms->term_count; ++i) {
		if (ctx->terms->terms[i].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t row_binder = ctx->terms->terms[i].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(
				ctx->terms, binder_classifier, row_binder
			)) {
			continue;
		}
		int already_recorded = 0;
		for (uint32_t j = 0;
			j < lambda_operation_node->implicit_effect_row_count;
			++j) {
			if (lambda_operation_node->implicit_effect_row_binders[j] == row_binder) {
				already_recorded = 1;
				break;
			}
		}
		if (already_recorded) {
			continue;
		}
		if (lambda_operation_node->implicit_effect_row_count >= 16) {
			return -1;
		}
		lambda_operation_node->implicit_effect_row_binders[
			lambda_operation_node->implicit_effect_row_count++
		] = row_binder;
	}
	uint32_t binder_var;
	if (prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		queue_binder_assumption(
			ctx,
			ctx->metadata->operations[body.operation].context_id,
			binder_var,
			binder_classifier,
			lambda_operation
		) != 0) {
		return -1;
	}
	p_ret->operation = lambda_operation;
	return 0;
}

/* Constructor application is a value spine.  This is deliberately decided
 * from the shared TermDB head after recursively lowering its value fields;
 * ordinary source application continues through FORCE/APP/COMPUTATION_FOLD below. */
static int compile_ast_constructor_application_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	struct compile_ref function;
	struct compile_ref argument;
	uint32_t term;
	int status = compile_ast_value_ref(ctx, node->as.app.function, &function);
	if (status != 0) {
		return status;
	}
	status = compile_ast_value_ref(ctx, node->as.app.argument, &argument);
	if (status != 0) {
		return status;
	}
	if (!term_app_head_is_constructor(ctx->terms, function.term)) {
		return 1;
	}
	if (compile_shared_app(ctx, function.term, argument.term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID, ast_id,
		function.operation, argument.operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_LAMBDA) {
		return 1;
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		if (!ast_application_head_is_constructor(ctx, ast_id)) {
			return 1;
		}
		return compile_ast_constructor_application_value_ref(ctx, ast_id, p_ret);
	}
		if (node->tag == PROTOTYPE_AST_MATCH ||
			node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ||
			node->tag == PROTOTYPE_AST_BLOCK_BINDING ||
			node->tag == PROTOTYPE_AST_BLOCK_EXPRESSION ||
			node->tag == PROTOTYPE_AST_BLOCK_LAMBDA_EXIT ||
		node->tag == PROTOTYPE_AST_COMPUTATION_FOLD ||
		node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return 1;
	}
	if (node->tag == PROTOTYPE_AST_QUOTE) {
		struct compile_ref preserved;
		if (compile_ast_ref(ctx, node->as.unary.term, &preserved) != 0) {
			return -1;
		}
		if (preserved.polarity == COMPILE_REF_POLARITY_COMPUTATION) {
			return compile_ref_make_thunk(ctx, &preserved, ast_id, p_ret);
		}
		if (preserved.polarity == COMPILE_REF_POLARITY_VALUE &&
			preserved.classifier < ctx->terms->term_count &&
			ctx->terms->terms[preserved.classifier].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			*p_ret = preserved;
			return 0;
		}
		return -1;
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref value;
		uint32_t expected;
		uint32_t ascription_operation;
		int status = compile_ast_value_ref(ctx, node->as.ascription.term, &value);
		if (status != 0) {
			return status;
		}
		if (compile_ast_type_expr_term(ctx, node->as.ascription.type_expr, &expected) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				value.term,
				expected,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				value.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 || queue_ascription_check(
				ctx, value.term, expected, ast_id, value.operation
			) != 0) {
			return -1;
		}
		*p_ret = value;
		p_ret->operation = ascription_operation;
		p_ret->classifier = expected;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE) {
		struct compile_ref namespace_ref;
		uint32_t constructor_term;
		uint32_t classifier;
		if (compile_ast_pure_value_ref(
				ctx, node->as.name_in_ast_namespace.namespace_ast, &namespace_ref
			) != 0 || resolve_namespace_member(
				ctx,
				namespace_ref.term,
				node->as.name_in_ast_namespace.symbol_id,
				&constructor_term,
				&classifier
			) != 0) {
			return -1;
		}
		p_ret->term = constructor_term;
		p_ret->classifier = classifier;
		p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
		p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
		return operation_add(
			ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
			namespace_ref.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
			&p_ret->operation
		);
	}
	if (node->tag == PROTOTYPE_AST_SYSTEM_NAME) {
		if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
			return -1;
		}
		uint32_t operation;
		if (operation_add(
				ctx, PROTOTYPE_OPERATION_ATOM, p_ret->term, p_ret->classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
				&operation
			) != 0) {
			return -1;
		}
		ctx->metadata->operations[operation].polarity = p_ret->polarity;
		ctx->metadata->operations[operation].computation_kind =
			p_ret->computation_kind;
		p_ret->operation = operation;
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			ctx->metadata->operations[operation].context_id
		);
		/* System signatures are declaration facts needed by APP and request
		 * constraints in the same fixed-point round. */
		if (p_ret->term < ctx->terms->term_count &&
			ctx->terms->terms[p_ret->term].tag == PROTOTYPE_TERM_PURE_PRIMITIVE &&
			prototype_judgement_delta_record_pure_primitive_type(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				p_ret->term,
				p_ret->classifier
			) != 0) {
			return -1;
		}
		if (p_ret->term < ctx->terms->term_count &&
			ctx->terms->terms[p_ret->term].tag == PROTOTYPE_TERM_EFFECT_OPERATION &&
			prototype_judgement_delta_record_effect_operation_type(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				p_ret->term,
				p_ret->classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	switch (node->tag) {
		case PROTOTYPE_AST_TEXT_LITERAL:
		case PROTOTYPE_AST_INT_LITERAL:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_AST_VAR:
		case PROTOTYPE_AST_NAME:
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
		case PROTOTYPE_AST_TYPE_FORMATION:
		case PROTOTYPE_AST_TYPE_LITERAL:
			if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
				return -1;
			}
			return p_ret->polarity == COMPILE_REF_POLARITY_COMPUTATION ? 1 : 0;
		default:
			return -1;
	}
}

static int compile_ast_runtime_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	struct compile_ref value;
	int status;
	if (!ctx || !continuation || !continuation->apply || !p_ret) {
		return -1;
	}
	/* Constructor spines need an all-or-nothing lowering path.  The ordinary
	 * value probe recursively emits partial APP occurrences before it discovers
	 * a computed field.  Falling back after that probe would retain those
	 * occurrences and can feed RETURN(partial-constructor) to the next field. */
	if (ast_application_head_is_constructor(ctx, ast_id)) {
		return compile_ast_constructor_spine_value_then(
			ctx, ast_id, continuation, p_ret
		);
	}
	compile_ref_clear(&value);
	status = compile_ast_value_ref(ctx, ast_id, &value);
	if (status == 0) {
		int execute_thunk;
		if (compile_ref_is_returning_thunk(ctx, &value, &execute_thunk) != 0) {
			return -1;
		}
		if (execute_thunk && ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_QUOTE) {
			struct compile_ref forced;
			if (compile_ref_make_force(ctx, &value, ast_id, &forced) != 0) {
				return -1;
			}
			return compile_continue_runtime_computation(
				ctx, &forced, ast_id, continuation, p_ret
			);
		}
		return continuation->apply(ctx, &value, continuation->data, p_ret);
	}
	if (status < 0) {
		return -1;
	}
	/* An atomic name may already have returned its computation occurrence while
	 * reporting that it is not a value.  Avoid compiling that source occurrence
	 * twice, because OperationGraph edges are occurrence-sensitive. */
	if (value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		if (compile_ast_computation_ref(ctx, ast_id, &value) != 0 ||
			value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
			return -1;
		}
	}
	/* Raw functions are quoted explicitly with `&`.  IH uses FUNCTION as a
	 * provisional lowering candidate until its motive result is solved, so it
	 * still enters the computation-fold constraint path. */
	if (value.computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION &&
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return -1;
	}
	return compile_continue_runtime_computation(
		ctx, &value, ast_id, continuation, p_ret
	);
}

struct compile_constructor_spine_function_context {
	uint32_t argument_ast;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

struct compile_constructor_spine_argument_context {
	struct compile_ref function;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

static int compile_constructor_spine_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_constructor_spine_argument_context* context = data;
	struct compile_ref application;
	uint32_t term;
	if (!ctx || !argument || !context || !context->continuation || !p_ret ||
		compile_shared_app(ctx, context->function.term, argument->term, &term) != 0) {
		return -1;
	}
	application.term = term;
	application.classifier = PROTOTYPE_INVALID_ID;
	application.polarity = COMPILE_REF_POLARITY_VALUE;
	application.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
			context->source_ast, context->function.operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &application.operation
		) != 0) {
		return -1;
	}
	return context->continuation->apply(
		ctx, &application, context->continuation->data, p_ret
	);
}

static int compile_constructor_spine_function_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_constructor_spine_function_context* function_context = data;
	struct compile_constructor_spine_argument_context argument_context;
	struct compile_value_continuation argument_continuation = { 0 };
	if (!ctx || !function || !function_context || !p_ret) {
		return -1;
	}
	argument_context.function = *function;
	argument_context.source_ast = function_context->source_ast;
	argument_context.continuation = function_context->continuation;
	argument_continuation.apply = compile_constructor_spine_argument_continuation;
	argument_continuation.data = &argument_context;
	return compile_ast_runtime_value_then(
		ctx, function_context->argument_ast, &argument_continuation, p_ret
	);
}

/* Constructor formation is a value spine.  Sequence every computed field
 * left-to-right, but pass each partially formed constructor as a value rather
 * than treating it as a computation function. */
static int compile_ast_constructor_spine_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !continuation || !continuation->apply || !p_ret ||
		ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag != PROTOTYPE_AST_APP) {
		struct compile_ref head;
		if (compile_ast_value_ref(ctx, ast_id, &head) != 0 ||
			!term_app_head_is_constructor(ctx->terms, head.term)) {
			return -1;
		}
		return continuation->apply(ctx, &head, continuation->data, p_ret);
	}
	struct compile_constructor_spine_function_context function_context = {
		node->as.app.argument, ast_id, continuation
	};
	struct compile_value_continuation function_continuation = {
		.apply = compile_constructor_spine_function_continuation,
		.data = &function_context
	};
	return compile_ast_constructor_spine_value_then(
		ctx, node->as.app.function, &function_continuation, p_ret
	);
}

struct compile_match_scrutinee_context {
	const struct prototype_ast_node* node;
	uint32_t source_ast;
};

static int compile_match_scrutinee_continuation(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_match_scrutinee_context* context = data;
	if (!ctx || !value || !context || !p_ret) {
		return -1;
	}
	return compile_ast_match_from_value_ref(
		ctx, context->source_ast, context->node, value, p_ret
	);
}

static int compile_ast_match_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	struct compile_ref* p_ref
) {
	struct compile_match_scrutinee_context context;
	struct compile_value_continuation continuation = { 0 };
	if (!ctx || !node || !p_ref || node->tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	context.node = node;
	context.source_ast = ast_id;
	continuation.apply = compile_match_scrutinee_continuation;
	continuation.data = &context;
	return compile_ast_runtime_value_then(
		ctx, node->as.match.scrutinee, &continuation, p_ref
	);
}

static int compile_ref_make_operation_request(
	struct compile_context* ctx,
	const struct compile_ref* operation,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t binding_id;
	uint32_t binder_var;
	uint32_t result;
	uint32_t continuation;
	uint32_t continuation_thunk;
	uint32_t request;
	uint32_t application_classifier;
	struct prototype_term_classifier_view application_view;
	uint32_t empty_effects;
	uint32_t return_classifier;
	uint32_t continuation_family;
	uint32_t continuation_classifier;
	uint32_t canonical_result;
	uint32_t canonical_result_variable;
	uint32_t canonical_binder_id;
	uint32_t parent_context_id;
	uint32_t continuation_context_id;
	struct compile_ref result_variable;
	struct compile_ref result_return;
	uint32_t continuation_operation;
	if (!ctx || !operation || !argument || !p_ret ||
		operation->classifier == PROTOTYPE_INVALID_ID ||
		operation_apply_classifier_unchecked(
			ctx,
			operation->classifier,
			argument->term,
			&application_classifier
		) != 0 ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			application_classifier,
			&application_view
		) != 0 || application_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_effect_label(
			ctx->terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
		) != 0 || prototype_term_computation_type(
			ctx->terms, empty_effects, application_view.result, &return_classifier
		) != 0 ||
		(binding_id = prototype_term_new_binding(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			ctx->terms, binding_id, return_classifier, &continuation_family
		) != 0 || prototype_term_pi_family(
			ctx->terms, application_view.result, continuation_family,
			&continuation_classifier
		) != 0 ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_return(ctx->terms, binder_var, &result) != 0 ||
		prototype_term_lambda(ctx->terms, binding_id, result, &continuation) != 0 ||
		prototype_term_thunk(ctx->terms, continuation, &continuation_thunk) != 0 ||
		prototype_term_operation_request(
			ctx->terms, operation->term, argument->term, continuation_thunk, &request
		) != 0) {
		return -1;
	}
	/* `prototype_term_lambda` hash-conses alpha-equivalent binders.  The
	 * provisional binder used to construct the lambda can therefore differ
	 * from the binder in the stored lambda.  Source-operation metadata must
	 * point at the stored body, never at that provisional RETURN node. */
	if (continuation >= ctx->terms->term_count ||
		ctx->terms->terms[continuation].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	canonical_result = ctx->terms->terms[continuation].as.lambda.body;
	if (canonical_result >= ctx->terms->term_count ||
		ctx->terms->terms[canonical_result].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	canonical_result_variable =
		ctx->terms->terms[canonical_result].as.return_term.value;
	if (canonical_result_variable >= ctx->terms->term_count ||
		ctx->terms->terms[canonical_result_variable].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	canonical_binder_id =
		ctx->terms->terms[canonical_result_variable].as.var.binding_id;
	parent_context_id = ctx->context_ids[ctx->binder_count];
	if (prototype_context_extend(
			&ctx->metadata->contexts,
			parent_context_id,
			canonical_binder_id,
			application_view.result,
			PROTOTYPE_INVALID_ID,
			&continuation_context_id
		) != 0) {
		return -1;
	}
	result_variable.term = canonical_result_variable;
	result_variable.classifier = application_view.result;
	result_variable.polarity = COMPILE_REF_POLARITY_VALUE;
	result_variable.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	ctx->context_ids[ctx->binder_count] = continuation_context_id;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, canonical_result_variable,
			application_view.result,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &result_variable.operation
		) != 0 || compile_ref_make_return(
			ctx, &result_variable, source_ast, &result_return
		) != 0) {
		ctx->context_ids[ctx->binder_count] = parent_context_id;
		return -1;
	}
	ctx->context_ids[ctx->binder_count] = parent_context_id;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, continuation, continuation_classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			result_return.operation, PROTOTYPE_INVALID_ID, application_view.result,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[continuation_operation].binding_id =
		canonical_binder_id;
	p_ret->term = request;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_THUNK, continuation_thunk, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, continuation_operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	return operation_add(
		ctx, PROTOTYPE_OPERATION_REQUEST, request, application_classifier,
		source_ast, operation->operation, argument->operation,
		continuation_operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, 0, &p_ret->operation
	);
}

static int compile_ast_block_execution_ref(
	struct compile_context* ctx,
	uint32_t source_ast,
	uint32_t value_ast,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || source_ast >= ctx->asts->node_count ||
		value_ast >= ctx->asts->node_count) {
		return -1;
	}
	struct compile_ref value;
	int value_status =
		ctx->asts->nodes[value_ast].tag ==
			PROTOTYPE_AST_INDUCTION_HYPOTHESIS ?
		compile_ast_computation_ref(ctx, value_ast, &value) :
		compile_ast_ref(ctx, value_ast, &value);
	if (value_status != 0) {
		return -1;
	}
	int execute_thunk;
	if (compile_ref_is_returning_thunk(ctx, &value, &execute_thunk) != 0) {
		return -1;
	}
	if (value.polarity == COMPILE_REF_POLARITY_VALUE && execute_thunk &&
		ctx->asts->nodes[value_ast].tag != PROTOTYPE_AST_QUOTE) {
		struct compile_ref forced;
		if (compile_ref_make_force(ctx, &value, source_ast, &forced) != 0) {
			return -1;
		}
		value = forced;
	}
	*p_ret = value;
	return 0;
}

static int compile_ast_block_items_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	struct compile_ref* p_ret
);

struct compile_block_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
};

static int compile_block_tail_after_discard(
	struct compile_context* ctx,
	const struct compile_ref* ignored,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_block_tail_context* tail = data;
	(void)ignored;
	if (!ctx || !tail || !tail->block || !p_ret) {
		return -1;
	}
	return compile_ast_block_items_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		p_ret
	);
}

static int compile_ast_block_items_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	struct compile_ref* p_ret
) {
	if (!ctx || !block || !p_ret || block->tag != PROTOTYPE_AST_COMPUTATION_BLOCK ||
		item_index > cutoff_index || cutoff_index >= block->as.block.item_count ||
		(size_t)block->as.block.first_item + block->as.block.item_count >
			ctx->asts->block_item_count) {
		return -1;
	}
	uint32_t item_ast = ctx->asts->block_items[
		block->as.block.first_item + item_index
	];
	if (item_ast >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* item = &ctx->asts->nodes[item_ast];
	int is_result = item_index == cutoff_index;
	if (item->tag == PROTOTYPE_AST_BLOCK_EXPRESSION) {
		if (is_result) {
			return compile_ast_computation_ref(
				ctx, item->as.block_expression.term, p_ret
			);
		}
		struct compile_ref computation;
		if (compile_ast_block_execution_ref(
				ctx, item_ast, item->as.block_expression.term, &computation
			) != 0 || computation.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
			computation.computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION) {
			return -1;
		}
		struct compile_block_tail_context tail = {
			block_ast, block, item_index + 1, cutoff_index
		};
		struct compile_value_continuation continuation = {
			.apply = compile_block_tail_after_discard,
			.data = &tail
		};
		return compile_continue_runtime_computation(
			ctx, &computation, item_ast, &continuation, p_ret
		);
	}
	if (item->tag != PROTOTYPE_AST_BLOCK_BINDING) {
		return -1;
	}

	struct compile_ref value;
	uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
	if (compile_ast_block_execution_ref(
			ctx, item_ast, item->as.block_binding.value, &value
		) != 0 || (item->as.block_binding.binder_type != PROTOTYPE_INVALID_ID &&
			compile_ast_type_expr_term(
				ctx, item->as.block_binding.binder_type, &binder_classifier
			) != 0)) {
		return -1;
	}
	if (value.polarity == COMPILE_REF_POLARITY_VALUE ||
		(value.polarity == COMPILE_REF_POLARITY_COMPUTATION &&
		 value.computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION &&
		 ctx->asts->nodes[item->as.block_binding.value].tag !=
			PROTOTYPE_AST_INDUCTION_HYPOTHESIS)) {
		uint32_t saved_local_ref_count = ctx->local_ref_count;
		if ((binder_classifier != PROTOTYPE_INVALID_ID && queue_ascription_check(
				ctx, value.term, binder_classifier, item_ast, value.operation
			) != 0) || push_local_ref(
				ctx,
				item->as.block_binding.ast_binder_id,
				item->as.block_binding.binder_symbol_id,
				&value
			) != 0) {
			ctx->local_ref_count = saved_local_ref_count;
			return -1;
		}
		int status;
		if (is_result) {
			status = value.polarity == COMPILE_REF_POLARITY_VALUE ?
				compile_ref_make_return(ctx, &value, item_ast, p_ret) :
				(*p_ret = value, 0);
		} else {
			status = compile_ast_block_items_ref(
				ctx, block_ast, block, item_index + 1, cutoff_index, p_ret
			);
		}
		ctx->local_ref_count = saved_local_ref_count;
		return status;
	}
	if (value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}

	struct compile_block_tail_context tail = {
		block_ast, block, item_index + 1, cutoff_index
	};
	struct compile_value_continuation continuation = {
		is_result ? compile_return_continuation :
			compile_block_tail_after_discard,
		is_result ? (void*)&item_ast : (void*)&tail,
		1,
		item->as.block_binding.ast_binder_id,
		item->as.block_binding.binder_symbol_id,
		binder_classifier
	};
	return compile_continue_runtime_computation(
		ctx, &value, item_ast, &continuation, p_ret
	);
}

struct compile_control_context {
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_apply_value(
	struct compile_context* ctx,
	const struct compile_ref* value,
	const struct compile_value_continuation* continuation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	if (!ctx || !value || !p_ret ||
		value->polarity != COMPILE_REF_POLARITY_VALUE) {
		return -1;
	}
	return continuation ? continuation->apply(
		ctx, value, continuation->data, p_ret
	) : compile_ref_make_return(ctx, value, source_ast, p_ret);
}

static int compile_ast_block_items_control_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
);

struct compile_control_block_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_block_tail_after_discard(
	struct compile_context* ctx,
	const struct compile_ref* ignored,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_block_tail_context* tail = data;
	(void)ignored;
	if (!ctx || !tail || !p_ret) {
		return -1;
	}
	return compile_ast_block_items_control_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		tail->normal_continuation,
		tail->exit_continuation,
		p_ret
	);
}

static int compile_match_branch_body_control(
	struct compile_context* ctx,
	uint32_t body_ast,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_context* control = data;
	if (!control) {
		return -1;
	}
	return compile_ast_computation_control_ref(
		ctx,
		body_ast,
		control->normal_continuation,
		control->exit_continuation,
		p_ret
	);
}

struct compile_control_match_scrutinee_context {
	const struct prototype_ast_node* node;
	uint32_t source_ast;
	struct compile_control_context control;
};

static int compile_control_match_scrutinee_continuation(
	struct compile_context* ctx,
	const struct compile_ref* scrutinee,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_control_match_scrutinee_context* context = data;
	if (!ctx || !scrutinee || !context || !p_ret) {
		return -1;
	}
	return compile_ast_match_from_value_with_branch_compiler(
		ctx,
		context->source_ast,
		context->node,
		scrutinee,
		compile_match_branch_body_control,
		&context->control,
		p_ret
	);
}

static int compile_ast_match_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_MATCH ||
		ast_lambda_exit_presence_in(ctx->asts, node->as.match.scrutinee) !=
			AST_LAMBDA_EXIT_NONE) {
		return -1;
	}
	struct compile_control_match_scrutinee_context context = {
		node,
		ast_id,
		{ normal_continuation, exit_continuation }
	};
	struct compile_value_continuation continuation = {
		.apply = compile_control_match_scrutinee_continuation,
		.data = &context
	};
	return compile_ast_runtime_value_then(
		ctx, node->as.match.scrutinee, &continuation, p_ret
	);
}

struct compile_control_binding_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
	int is_result;
	uint32_t source_ast;
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_binding_tail(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_binding_tail_context* tail = data;
	if (!ctx || !value || !tail || !p_ret) {
		return -1;
	}
	if (tail->is_result) {
		return compile_control_apply_value(
			ctx, value, tail->normal_continuation, tail->source_ast, p_ret
		);
	}
	return compile_ast_block_items_control_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		tail->normal_continuation,
		tail->exit_continuation,
		p_ret
	);
}

static int compile_ast_block_items_control_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !block || !p_ret || block->tag != PROTOTYPE_AST_COMPUTATION_BLOCK ||
		item_index > cutoff_index || cutoff_index >= block->as.block.item_count ||
		(size_t)block->as.block.first_item + block->as.block.item_count >
			ctx->asts->block_item_count) {
		return -1;
	}
	uint32_t item_ast = ctx->asts->block_items[
		block->as.block.first_item + item_index
	];
	if (item_ast >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* item = &ctx->asts->nodes[item_ast];
	int is_result = item_index == cutoff_index;
	if (item->tag == PROTOTYPE_AST_BLOCK_LAMBDA_EXIT) {
		struct compile_ref value;
		if (compile_ast_value_ref(
				ctx, item->as.block_lambda_exit.value, &value
			) != 0) {
			return -1;
		}
		return compile_control_apply_value(
			ctx, &value, exit_continuation, item_ast, p_ret
		);
	}
	if (item->tag == PROTOTYPE_AST_BLOCK_EXPRESSION) {
		if (is_result) {
			return compile_ast_computation_control_ref(
				ctx,
				item->as.block_expression.term,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		struct compile_control_block_tail_context tail = {
			block_ast,
			block,
			item_index + 1,
			cutoff_index,
			normal_continuation,
			exit_continuation
		};
		struct compile_value_continuation continuation = {
			.apply = compile_control_block_tail_after_discard,
			.data = &tail
		};
		if (ast_lambda_exit_presence_in(
				ctx->asts, item->as.block_expression.term
			) == AST_LAMBDA_EXIT_NONE) {
			struct compile_ref computation;
			if (compile_ast_block_execution_ref(
					ctx, item_ast, item->as.block_expression.term, &computation
				) != 0 || computation.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
				computation.computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION) {
				return -1;
			}
			return compile_continue_runtime_computation(
				ctx, &computation, item_ast, &continuation, p_ret
			);
		}
		return compile_ast_computation_control_ref(
			ctx,
			item->as.block_expression.term,
			&continuation,
			exit_continuation,
			p_ret
		);
	}
	if (item->tag != PROTOTYPE_AST_BLOCK_BINDING) {
		return -1;
	}

	struct compile_ref value;
	compile_ref_clear(&value);
	uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
	int value_exit_presence = ast_lambda_exit_presence_in(
		ctx->asts, item->as.block_binding.value
	);
	if (value_exit_presence == AST_LAMBDA_EXIT_FORBIDDEN ||
		(item->as.block_binding.binder_type != PROTOTYPE_INVALID_ID &&
			compile_ast_type_expr_term(
				ctx, item->as.block_binding.binder_type, &binder_classifier
			) != 0)) {
		return -1;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		compile_ast_block_execution_ref(
			ctx,
			item_ast,
			item->as.block_binding.value,
			&value
		) != 0) {
		return -1;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		(value.polarity == COMPILE_REF_POLARITY_VALUE ||
		(value.polarity == COMPILE_REF_POLARITY_COMPUTATION &&
		 value.computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION &&
		 ctx->asts->nodes[item->as.block_binding.value].tag !=
			 PROTOTYPE_AST_INDUCTION_HYPOTHESIS))) {
		uint32_t saved_local_ref_count = ctx->local_ref_count;
		if ((binder_classifier != PROTOTYPE_INVALID_ID && queue_ascription_check(
				ctx, value.term, binder_classifier, item_ast, value.operation
			) != 0) || push_local_ref(
				ctx,
				item->as.block_binding.ast_binder_id,
				item->as.block_binding.binder_symbol_id,
				&value
			) != 0) {
			ctx->local_ref_count = saved_local_ref_count;
			return -1;
		}
		int status;
		if (is_result) {
			status = value.polarity == COMPILE_REF_POLARITY_VALUE ?
				compile_control_apply_value(
					ctx, &value, normal_continuation, item_ast, p_ret
				) : (normal_continuation ? -1 : (*p_ret = value, 0));
		} else {
			status = compile_ast_block_items_control_ref(
				ctx,
				block_ast,
				block,
				item_index + 1,
				cutoff_index,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		ctx->local_ref_count = saved_local_ref_count;
		return status;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	struct compile_control_binding_tail_context tail = {
		block_ast,
		block,
		item_index + 1,
		cutoff_index,
		is_result,
		item_ast,
		normal_continuation,
		exit_continuation
	};
	struct compile_value_continuation continuation = {
		compile_control_binding_tail,
		&tail,
		1,
		item->as.block_binding.ast_binder_id,
		item->as.block_binding.binder_symbol_id,
		binder_classifier
	};
	if (value_exit_presence == AST_LAMBDA_EXIT_FOUND) {
		return compile_ast_computation_control_ref(
			ctx,
			item->as.block_binding.value,
			&continuation,
			exit_continuation,
			p_ret
		);
	}
	return compile_continue_runtime_computation(
		ctx, &value, item_ast, &continuation, p_ret
	);
}

static int compile_ast_computation_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	int presence = ast_lambda_exit_presence_in(ctx->asts, ast_id);
	if (presence == AST_LAMBDA_EXIT_FORBIDDEN) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (presence == AST_LAMBDA_EXIT_FOUND) {
		if (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK) {
			return compile_ast_block_items_control_ref(
				ctx,
				ast_id,
				node,
				0,
				node->as.block.result_item_index,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		if (node->tag == PROTOTYPE_AST_MATCH) {
			return compile_ast_match_control_ref(
				ctx,
				ast_id,
				node,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		return -1;
	}
	struct compile_ref computation;
	if (compile_ast_computation_ref(ctx, ast_id, &computation) != 0 ||
		computation.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	if (!normal_continuation) {
		*p_ret = computation;
		return 0;
	}
	return compile_continue_runtime_computation(
		ctx, &computation, ast_id, normal_continuation, p_ret
	);
}

static int compile_ast_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK) {
		if (node->as.block.item_count == 0 ||
			node->as.block.result_item_index >= node->as.block.item_count) {
			return -1;
		}
		return compile_ast_block_items_ref(
			ctx, ast_id, node, 0, node->as.block.result_item_index, p_ret
		);
	}
	if (node->tag == PROTOTYPE_AST_COMPUTATION_FOLD) {
		return compile_ast_computation_fold_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref inner;
		uint32_t surface_classifier;
		uint32_t expected_classifier;
		uint32_t ascription_operation;
		uint32_t inner_ast = node->as.ascription.term;
		if (inner_ast >= ctx->asts->node_count ||
			compile_ast_computation_ref(ctx, inner_ast, &inner) != 0 ||
			inner.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
			compile_ast_ascription_classifier(
				ctx, node->as.ascription.type_expr, &surface_classifier
			) != 0 || compile_expected_classifier_for_ref(
				ctx, &inner, surface_classifier, &expected_classifier
			) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				inner.term,
				expected_classifier,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				inner.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		inner.classifier = expected_classifier;
		inner.operation = ascription_operation;
		*p_ret = inner;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_MATCH) {
		return compile_ast_match_ref(ctx, ast_id, node, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_LAMBDA) {
		return compile_ast_lambda_computation_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		if (ast_application_head_is_constructor(ctx, ast_id)) {
			struct compile_value_continuation continuation = {
				.apply = compile_return_continuation,
				.data = &ast_id
			};
			return compile_ast_constructor_spine_value_then(
				ctx, ast_id, &continuation, p_ret
			);
		}
		return compile_ast_application_computation_ref(ctx, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		/* An IH stands for the recursive Match computation M(rest), not for
		 * its result value.  `compile_ast_runtime_value_then` introduces a computation fold whenever
		 * a source occurrence needs that result as a value. */
		if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
			return -1;
		}
		p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
		/* The enclosing APP decides whether the inferred motive result is a Pi.
		 * Preserve the direct-function candidate until that solver constraint is
		 * available; value contexts still sequence this occurrence through COMPUTATION_FOLD. */
		p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_NAME || node->tag == PROTOTYPE_AST_VAR) {
		struct compile_ref name;
		if (compile_ast_atomic_ref(ctx, ast_id, &name) != 0) {
			return -1;
		}
		if (name.polarity == COMPILE_REF_POLARITY_COMPUTATION) {
			*p_ret = name;
			return 0;
		}
		if (name.polarity == COMPILE_REF_POLARITY_VALUE &&
			(compile_ref_thunk_computation_kind(ctx, &name) !=
					COMPILE_REF_COMPUTATION_KIND_UNKNOWN ||
			 (name.classifier < ctx->terms->term_count &&
			  ctx->terms->terms[name.classifier].tag == PROTOTYPE_TERM_THUNK_TYPE))) {
			return compile_ref_make_force(ctx, &name, ast_id, p_ret);
		}
		if (name.polarity == COMPILE_REF_POLARITY_VALUE) {
			return compile_ref_make_return(ctx, &name, ast_id, p_ret);
		}
		return -1;
	}
	struct compile_value_continuation continuation = {
		.apply = compile_return_continuation,
		.data = &ast_id
	};
	return compile_ast_runtime_value_then(ctx, ast_id, &continuation, p_ret);
}

static int compile_ast_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref inner;
		uint32_t expected_classifier;
		uint32_t ascription_operation;
		if (compile_ast_ascription_classifier(
					ctx, node->as.ascription.type_expr, &expected_classifier
				) != 0 ||
			compile_ast_against_surface_classifier(
					ctx, node->as.ascription.term, expected_classifier, &inner
				) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				inner.term,
				expected_classifier,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				inner.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		*p_ret = inner;
		p_ret->classifier = expected_classifier;
		p_ret->operation = ascription_operation;
		return 0;
	}
	int status = compile_ast_value_ref(ctx, ast_id, p_ret);
	if (status <= 0) {
		return status;
	}
	return compile_ast_computation_ref(ctx, ast_id, p_ret);
}

/* Resolve the selected root definition through the definition block itself.
 * This keeps graph construction and label publication on one membership rule. */
static int compile_selected_definition(
	struct compile_context* ctx,
	const struct prototype_ast_node** p_select,
	struct prototype_ast_term_assignment_def** p_selected
) {
	if (!ctx || !p_select || !p_selected ||
		ctx->asts->root_definition_select >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* select =
		&ctx->asts->nodes[ctx->asts->root_definition_select];
	if (select->tag != PROTOTYPE_AST_DEFINITION_SELECT ||
		select->as.definition_select.definition_block >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* block = &ctx->asts->nodes[
		select->as.definition_select.definition_block
	];
	if (block->tag != PROTOTYPE_AST_DEFINITION_BLOCK ||
		(size_t)block->as.definition_block.first_assignment +
			block->as.definition_block.assignment_count >
			ctx->asts->definition_item_count) {
		return -1;
	}
	struct prototype_ast_term_assignment_def* selected = NULL;
	for (uint32_t i = 0; i < block->as.definition_block.assignment_count; ++i) {
		uint32_t assignment_id = ctx->asts->definition_items[
			block->as.definition_block.first_assignment + i
		];
		if (assignment_id >= ctx->asts->assignment_count) {
			return -1;
		}
		struct prototype_ast_term_assignment_def* candidate =
			&ctx->asts->assignments[assignment_id];
		if (candidate->name_symbol_id ==
			select->as.definition_select.name_symbol_id) {
			if (selected) {
				return -1;
			}
			selected = candidate;
		}
	}
	if (!selected) {
		return -1;
	}
	*p_select = select;
	*p_selected = selected;
	return 0;
}

static int compile_phase_build_selected_entry(struct compile_context* ctx) {
	if (ctx->asts->root_definition_select != PROTOTYPE_INVALID_ID) {
		const struct prototype_ast_node* select;
		struct prototype_ast_term_assignment_def* selected;
		if (compile_selected_definition(ctx, &select, &selected) != 0) {
			return -1;
		}
		if (!selected || !selected->compiled ||
			selected->compiled_operation >= ctx->metadata->operation_count ||
			selected->compiled_term >= ctx->terms->term_count ||
			ctx->terms->terms[selected->compiled_term].tag != PROTOTYPE_TERM_THUNK) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				select->as.definition_select.name_symbol_id,
				-1,
				ctx->asts->root_definition_select,
				select->span
			);
			return -1;
		}
		struct compile_ref selected_ref;
		selected_ref.term = selected->compiled_term;
		selected_ref.classifier = selected->compiled_classifier;
		selected_ref.operation = selected->compiled_operation;
		selected_ref.polarity = COMPILE_REF_POLARITY_VALUE;
		selected_ref.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
		struct compile_ref entry_ref;
		if (compile_ref_make_force(
				ctx,
				&selected_ref,
				ctx->asts->root_definition_select,
				&entry_ref
			) != 0) {
			return -1;
		}
		ctx->metadata->selected_entry_symbol_id = selected->name_symbol_id;
		ctx->metadata->selected_entry_term = entry_ref.term;
		ctx->metadata->selected_entry_classifier = entry_ref.classifier;
		ctx->metadata->selected_entry_operation = entry_ref.operation;
	}
	return 0;
}

static int compile_phase_build_graph(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
		uint32_t term;
		const struct prototype_ast_term_assignment_def* def = &ctx->asts->assignments[i];
		const struct prototype_ast_def_open_address_entry* entry =
			lookup_def_index_entry_const(ctx->asts, def->name_symbol_id);
		if (entry && entry->assignment_count > 1) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		size_t previous_error_count = ctx->metadata ?
			ctx->metadata->resolve_error_count : 0;
		if (compile_def(ctx, &ctx->asts->assignments[i], &term) != 0) {
			if (!ctx->metadata ||
				ctx->metadata->resolve_error_count == previous_error_count) {
				(void)add_resolve_error_at_span(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_COMPILE,
					def->name_symbol_id,
					-1,
					def->ast,
					def->name_span
				);
			}
			continue;
		}
	}
	return compile_phase_build_selected_entry(ctx);
}

static int constructor_telescope_field_classifier(
	struct compile_context* ctx,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		owner >= ctx->terms->term_count ||
		(previous_binder_count > 0 && !previous_binders)) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			ctx->terms,
			owner,
			&type_id,
			arguments,
			&argument_count
		) != 0 ||
		type_id >= ctx->type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->constructor_count ||
		argument_count != type->parameter_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (previous_binder_count < field_index ||
		!prototype_context_get(&ctx->metadata->contexts, source_context)) {
		return -1;
	}

	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			source_context,
			constructor->parameter_context,
			arguments,
			argument_count,
			&substitution
		) != 0) {
		return -1;
	}
	uint32_t previous_terms[64];
	for (uint32_t i = 0; i < field_index; ++i) {
		if (prototype_term_var(
				ctx->terms, previous_binders[i].binding_id, &previous_terms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_telescope_entry_classifier(
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		substitution,
		constructor->parameter_context,
		constructor->field_context,
		previous_terms,
		field_index,
		field_index,
		p_classifier
	);
}

static int compile_phase_resolve_pending_match_items(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	int has_pending = 0;

	for (uint32_t i = 0; i < ctx->pending_match_resolution_count; ++i) {
		const struct pending_match_resolution* resolution =
			&ctx->pending_match_resolutions[i];
		struct prototype_match_constructor_resolution resolved;
		if (resolution->item_id >= ctx->metadata->resolution_item_count) {
			return -1;
		}
		const struct prototype_resolution_item* item =
			&ctx->metadata->resolution_items[resolution->item_id];
		if (item->state == PROTOTYPE_RESOLUTION_ITEM_RESOLVED) {
			continue;
		}
			uint32_t scrutinee_classifier;
			int selection_status = select_match_resolution_scrutinee_classifier(
					ctx,
					resolution,
					item,
					&scrutinee_classifier
				);
			if (selection_status < 0) {
				return -1;
			}
			if (selection_status > 0) {
				has_pending = 1;
				continue;
			}
		if (prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				scrutinee_classifier,
				resolution->constructor_symbol_id,
				&resolved
			) != 0 ||
			prototype_term_resolve_match_case(
				ctx->terms,
				resolution->match_term,
				item->case_index,
				resolved.constructor_owner,
				resolved.constructor_id
			) != 0 ||
			resolve_match_constructor_resolution_item(
				ctx,
				resolution->item_id,
				resolution->match_term,
				resolved.constructor_owner,
				resolved.constructor_id
			) != 0) {
			return -1;
		}
		const struct prototype_term* resolved_match =
			&ctx->terms->terms[resolution->match_term];
		uint32_t resolved_case_id =
			resolved_match->as.match.first_case + item->case_index;
		if (resolved_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* resolved_case =
			&ctx->terms->cases[resolved_case_id];
		if (item->ast >= ctx->asts->node_count ||
			ctx->asts->nodes[item->ast].tag != PROTOTYPE_AST_MATCH ||
			item->case_index >= ctx->asts->nodes[item->ast].as.match.case_count) {
			return -1;
		}
		const struct prototype_ast_match_case* source_case =
			&ctx->asts->cases[
				ctx->asts->nodes[item->ast].as.match.first_case + item->case_index
			];
		if (source_case->binder_count != resolved_case->binder_count ||
			source_case->first_binder + source_case->binder_count >
				ctx->asts->case_binder_count) {
			return -1;
		}
		uint32_t case_context = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->operation_count;
			++operation_id) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[operation_id];
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				operation->core_term != resolution->match_term ||
				item->case_index >= operation->case_count ||
				operation->first_case + item->case_index >=
					ctx->metadata->operation_case_count) {
				continue;
			}
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[
					operation->first_case + item->case_index
				];
			if (operation_case->body_operation >=
				ctx->metadata->operation_count) {
				return -1;
			}
			case_context = ctx->metadata->operations[
				operation_case->body_operation
			].context_id;
			break;
		}
		if (case_context == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < resolved_case->binder_count;
			++binder_index) {
			struct prototype_case_binder* binder =
				&ctx->terms->case_binders[resolved_case->first_binder + binder_index];
			uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
			if (constructor_telescope_field_classifier(
					ctx,
					case_context,
					resolved.constructor_owner,
					resolved.constructor_id,
					&ctx->terms->case_binders[resolved_case->first_binder],
					binder_index,
					binder_index,
					&binder_classifier
				) != 0) {
				return -1;
			}
			struct prototype_term_conversion_result conversion =
				prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					binder_classifier,
					scrutinee_classifier
				);
			if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				binder->is_recursive = 1;
			} else if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
				binder->is_recursive = 0;
			} else {
				return -1;
			}
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->operation_count;
				++operation_id) {
				struct prototype_operation_node* operation =
					&ctx->metadata->operations[operation_id];
				if (operation->tag == PROTOTYPE_OPERATION_VAR &&
					operation->referenced_ast_binder_id ==
						ctx->asts->case_binders[
							source_case->first_binder + binder_index
						].ast_binder_id) {
					operation->known_classifier = binder_classifier;
				}
			}
		}
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->operation_count;
			++operation_id) {
			struct prototype_operation_node* operation =
				&ctx->metadata->operations[operation_id];
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				operation->core_term != resolution->match_term ||
				item->case_index >= operation->case_count ||
				operation->first_case + item->case_index >=
					ctx->metadata->operation_case_count) {
				continue;
			}
			struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[
					operation->first_case + item->case_index
				];
			operation_case->constructor_owner = resolved.constructor_owner;
			operation_case->constructor_id = resolved.constructor_id;
		}
	}
	return has_pending ? 1 : 0;
}

static int operation_solver_add_classifier_goal(
	struct compile_context* ctx,
	int kind,
	uint32_t classifier_variable,
	union operation_classifier_goal_payload payload
) {
	if (!ctx || !ctx->metadata ||
		classifier_variable >= ctx->metadata->operation_count ||
		ctx->classifier_solver.constraint_count >= PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->classifier_solver.constraint_count++;
	ctx->classifier_solver.constraints[id] =
		(struct operation_classifier_goal){
			.id = id,
			.kind = kind,
			.state = OPERATION_CONSTRAINT_STATE_PENDING,
			.reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE,
			.source_operation = classifier_variable,
			.source_ast = ctx->metadata->operations[classifier_variable].source_ast,
			.context_id = ctx->metadata->operations[classifier_variable].context_id,
			.classifier_variable = classifier_variable,
			.payload = payload
		};
	return 0;
}

static int operation_solver_add_motive_case_goal(
	struct compile_context* ctx,
	uint32_t match_operation,
	uint32_t case_index,
	uint32_t body_operation,
	uint32_t scrutinee_operation
) {
	if (!ctx || !ctx->metadata ||
		match_operation >= ctx->metadata->operation_count ||
		body_operation >= ctx->metadata->operation_count ||
		scrutinee_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[match_operation];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		case_index >= operation->case_count ||
		operation->first_case + case_index >=
			ctx->metadata->operation_case_count) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[operation->core_term];
	if (operation->source_ast >= ctx->asts->node_count ||
		ctx->asts->nodes[operation->source_ast].tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	const struct prototype_ast_node* source_match =
		&ctx->asts->nodes[operation->source_ast];
	if (case_index >= source_match->as.match.case_count ||
		source_match->as.match.first_case + case_index >= ctx->asts->case_count) {
		return -1;
	}
	uint32_t term_case_id = match->as.match.first_case + case_index;
	if (term_case_id >= ctx->terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &ctx->terms->cases[term_case_id];
	const struct prototype_ast_match_case* ast_match_case = &ctx->asts->cases[
		source_match->as.match.first_case + case_index
	];
	if (match_case->first_binder + match_case->binder_count >
		ctx->terms->case_binder_count ||
		ast_match_case->first_binder + ast_match_case->binder_count >
			ctx->asts->case_binder_count ||
		match_case->binder_count != ast_match_case->binder_count) {
		return -1;
	}
	union operation_classifier_goal_payload payload = { 0 };
	payload.motive_case = (struct operation_classifier_motive_case_goal){
		.branch_operation = body_operation,
		.scrutinee_operation = scrutinee_operation,
		.case_index = case_index,
		.constructor_owner = match_case->constructor_owner,
		.constructor_index = match_case->constructor_id,
		.ih_scope_id = match->as.match.ih_scope_id
	};
	return operation_solver_add_classifier_goal(
		ctx, OPERATION_CONSTRAINT_MOTIVE_EQUATION, match_operation, payload
	);
}

static const struct operation_classifier_goal* operation_solver_motive_equation(
	const struct operation_classifier_solver* solver,
	uint32_t match_operation,
	uint32_t case_index
) {
	if (!solver) {
		return NULL;
	}
	for (uint32_t i = 0; i < solver->constraint_count; ++i) {
		const struct operation_classifier_goal* goal = &solver->constraints[i];
		if (goal->kind == OPERATION_CONSTRAINT_MOTIVE_EQUATION &&
			goal->classifier_variable == match_operation &&
			goal->payload.motive_case.case_index == case_index) {
			return goal;
		}
	}
	return NULL;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_goal* equation,
	uint32_t classifier
);

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);

static int operation_solver_enqueue_constraint(
	struct compile_context* ctx,
	uint32_t constraint_id
) {
	if (!ctx || constraint_id >= ctx->classifier_solver.constraint_count) {
		return -1;
	}
	if (ctx->classifier_solver.constraint_queued[constraint_id]) {
		return 0;
	}
	if (ctx->classifier_solver.worklist_count >=
		PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t tail = (
		ctx->classifier_solver.worklist_head +
		ctx->classifier_solver.worklist_count
	) % PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY;
	ctx->classifier_solver.worklist[tail] = constraint_id;
	ctx->classifier_solver.constraint_queued[constraint_id] = 1;
	ctx->classifier_solver.worklist_count++;
	return 0;
}

static int operation_solver_enqueue_dependents(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || operation >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t dependency =
		ctx->classifier_solver.first_dependent_constraint[operation];
	while (dependency != PROTOTYPE_INVALID_ID) {
		if (dependency >= ctx->classifier_solver.dependent_constraint_count ||
			operation_solver_enqueue_constraint(
				ctx,
				ctx->classifier_solver.dependent_constraints[dependency].constraint
			) != 0) {
			return -1;
		}
		dependency = ctx->classifier_solver.dependent_constraints[dependency].next;
	}
	return 0;
}

static int operation_solver_enqueue_clause_fold_dependents(
	struct compile_context* ctx,
	uint32_t clause_body_operation
) {
	if (!ctx || !ctx->metadata ||
		clause_body_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* fold =
			&ctx->metadata->operations[operation_id];
		if (fold->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			fold->fold_clause_count == 0 ||
			(size_t)fold->first_fold_clause + fold->fold_clause_count >
				ctx->metadata->operation_fold_clause_count) {
			continue;
		}
		int uses_clause_body = 0;
		for (uint32_t clause = 0; clause < fold->fold_clause_count; ++clause) {
			if (ctx->metadata->operation_fold_clauses[
					fold->first_fold_clause + clause
				].body_operation == clause_body_operation) {
				uses_clause_body = 1;
				break;
			}
		}
		if (!uses_clause_body) {
			continue;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->classifier_solver.constraint_count;
			++constraint_id) {
			const struct operation_classifier_goal* constraint =
				&ctx->classifier_solver.constraints[constraint_id];
			if (constraint->kind ==
					OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT &&
				constraint->classifier_variable == operation_id &&
				operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int operation_solver_pop_constraint(
	struct compile_context* ctx,
	uint32_t* p_constraint
) {
	if (!ctx || !p_constraint) {
		return -1;
	}
	if (ctx->classifier_solver.worklist_count == 0) {
		return 1;
	}
	*p_constraint =
		ctx->classifier_solver.worklist[ctx->classifier_solver.worklist_head];
	ctx->classifier_solver.worklist_head =
		(ctx->classifier_solver.worklist_head + 1) %
			PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY;
	ctx->classifier_solver.worklist_count--;
	ctx->classifier_solver.constraint_queued[*p_constraint] = 0;
	return 0;
}

static int operation_solver_bind(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.bindings[variable];
	if (previous == PROTOTYPE_INVALID_ID) {
		ctx->classifier_solver.bindings[variable] = classifier;
		*p_changed = 1;
		return operation_solver_enqueue_dependents(ctx, variable) != 0 ||
			operation_solver_enqueue_clause_fold_dependents(ctx, variable) != 0 ?
				-1 : 0;
	}
				if (!(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}

/* Merge two returning-computation classifier equations into one monotone
 * solver representative. A closed row discharges unresolved row atoms at this
 * occurrence; known operation labels are accumulated. This is solver-local
 * refinement and does not make effect-row union a global Term reduction. */
static int operation_solver_merge_computation_classifiers(
	struct compile_context* ctx,
	uint32_t previous,
	uint32_t candidate,
	uint32_t* p_merged
) {
	if (!ctx || !p_merged || previous >= ctx->terms->term_count ||
		candidate >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view before;
	struct prototype_term_classifier_view after;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, previous, &before
		) != 0 || prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, candidate, &after
		) != 0 || before.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		after.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		before.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		after.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before.result, after.result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	struct prototype_effect_row_normal_form before_row;
	struct prototype_effect_row_normal_form after_row;
	if (prototype_term_effect_row_normal_form(
			ctx->terms, before.effect_row, &before_row
		) != 0 || prototype_term_effect_row_normal_form(
			ctx->terms, after.effect_row, &after_row
		) != 0) {
		return 1;
	}
	struct prototype_effect_row_normal_form merged;
	memset(&merged, 0, sizeof(merged));
	merged.effects = before_row.effects | after_row.effects;
	/* A closed equation instantiates the unresolved row of the other equation.
	 * If neither side is closed, retain every unresolved atom as residual state. */
	if (before_row.atom_count != 0 && after_row.atom_count != 0) {
		for (uint32_t i = 0; i < before_row.atom_count; ++i) {
			merged.atoms[merged.atom_count++] = before_row.atoms[i];
		}
		for (uint32_t i = 0; i < after_row.atom_count; ++i) {
			int found = 0;
			for (uint32_t j = 0; j < merged.atom_count; ++j) {
				if (merged.atoms[j] == after_row.atoms[i]) {
					found = 1;
					break;
				}
			}
			if (!found) {
				if (merged.atom_count >=
					PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
					return -1;
				}
				merged.atoms[merged.atom_count++] = after_row.atoms[i];
			}
		}
	}
	uint32_t row;
	return prototype_term_effect_row_materialize_normal_form(
			ctx->terms, &merged, &row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, row, before.result, p_merged
		);
}

static int operation_solver_bind_proven_classifier(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->classifier_solver.bindings[variable] == classifier) {
		ctx->metadata->operations[variable].known_classifier = classifier;
		return 0;
	}
	uint32_t previous = ctx->classifier_solver.bindings[variable];
	if (previous != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The fixed-point cell already denotes this classifier. Preserve its
		 * representative so alpha-equivalent reconstruction cannot requeue the
		 * same dependency cycle indefinitely. */
		ctx->metadata->operations[variable].known_classifier = previous;
		return 0;
	}
	/* The OperationGraph seed is an elaboration approximation.  Once the
	 * kernel has validated a rule-specific proof, that occurrence classifier
	 * becomes authoritative even when it is not convertible to the seed. */
	ctx->classifier_solver.bindings[variable] = classifier;
	ctx->metadata->operations[variable].known_classifier = classifier;
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, variable) != 0 ||
		operation_solver_enqueue_clause_fold_dependents(ctx, variable) != 0 ?
			-1 : 0;
}

static int operation_solver_widen_computation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.bindings[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		return operation_solver_bind(ctx, operation, classifier, p_changed);
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return -1;
	}
	uint32_t merged;
	if (operation_solver_merge_computation_classifiers(
			ctx, previous, classifier, &merged
		) != 0) {
		return -1;
	}
	if (merged == previous || prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, merged
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	ctx->classifier_solver.bindings[operation] = merged;
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation) != 0 ||
		operation_solver_enqueue_clause_fold_dependents(ctx, operation) != 0 ?
			-1 : 0;
}

static int operation_solver_continuation_effect_widening(
	struct compile_context* ctx,
	uint32_t before,
	uint32_t after
) {
	if (!ctx || before >= ctx->terms->term_count ||
		after >= ctx->terms->term_count ||
		ctx->terms->terms[before].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		ctx->terms->terms[after].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t before_domain;
	uint32_t before_family;
	uint32_t before_family_binder;
	uint32_t before_codomain;
	uint32_t after_domain;
	uint32_t after_family;
	uint32_t after_family_binder;
	uint32_t after_codomain;
	if (prototype_judgement_pi_parts(
			ctx->terms,
			ctx->terms->terms[before].as.thunk_type.computation,
			&before_domain,
			&before_family
		) != 0 || prototype_judgement_pi_parts(
			ctx->terms,
			ctx->terms->terms[after].as.thunk_type.computation,
			&after_domain,
			&after_family
		) != 0 || prototype_term_pure_family_parts(
			ctx->terms, before_family, &before_family_binder, &before_codomain
		) != 0 || prototype_term_pure_family_parts(
			ctx->terms, after_family, &after_family_binder, &after_codomain
		) != 0 || !(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before_domain, after_domain
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	(void)before_family_binder;
	(void)after_family_binder;
	struct prototype_term_classifier_view before_view;
	struct prototype_term_classifier_view after_view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, before_codomain, &before_view
		) != 0 || prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, after_codomain, &after_view
		) != 0 || before_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		after_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		before_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		after_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
				before_view.result,
				after_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	struct prototype_effect_row_normal_form before_row;
	struct prototype_effect_row_normal_form after_row;
	if (prototype_term_effect_row_normal_form(
			ctx->terms, before_view.effect_row, &before_row
		) != 0 || prototype_term_effect_row_normal_form(
			ctx->terms, after_view.effect_row, &after_row
		) != 0) {
		return 0;
	}
	/* A lone row variable is the unspecialized continuation approximation
	 * introduced by this solver. It may be refined to either a closed row or a
	 * row expression that retains the variable. This is metavariable solving,
	 * not ordinary set inclusion. */
	if (before_row.effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE &&
		before_row.atom_count == 1 && before_row.atoms[0] < ctx->terms->term_count &&
		ctx->terms->terms[before_row.atoms[0]].tag ==
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return 1;
	}
	return prototype_term_effect_row_normal_form_includes(
		&after_row, &before_row
	);
}

static int operation_solver_widen_continuation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.bindings[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		return operation_solver_bind(ctx, operation, classifier, p_changed);
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return -1;
	}
	if (!operation_solver_continuation_effect_widening(
			ctx, previous, classifier
		)) {
		return -1;
	}
	ctx->classifier_solver.bindings[operation] = classifier;
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation);
}

static int operation_solver_specialize_integer_literal(
	struct compile_context* ctx,
	uint32_t function_classifier,
	uint32_t argument_operation,
	uint32_t* p_argument_classifier,
	int* p_changed
) {
	uint32_t whnf;
	uint32_t domain;
	if (!ctx || !p_argument_classifier || !p_changed ||
		argument_operation >= ctx->metadata->operation_count ||
		function_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_operation_node* argument =
		&ctx->metadata->operations[argument_operation];
	if (argument->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[argument->core_term].tag != PROTOTYPE_TERM_INT_LITERAL ||
		prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			function_classifier,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	domain = ctx->terms->terms[whnf].as.pi.domain;
	if (domain >= ctx->terms->term_count ||
		(ctx->terms->terms[domain].tag != PROTOTYPE_TERM_PRIMITIVE_INT &&
		 ctx->terms->terms[domain].tag != PROTOTYPE_TERM_PRIMITIVE_INT64) ||
		(ctx->terms->terms[domain].tag == PROTOTYPE_TERM_PRIMITIVE_INT &&
			(ctx->terms->terms[argument->core_term].as.int_literal.value < INT32_MIN ||
			 ctx->terms->terms[argument->core_term].as.int_literal.value > INT32_MAX))) {
		return 0;
	}
	uint32_t previous = ctx->classifier_solver.bindings[argument_operation];
	if (previous != domain) {
		/* Literal typing is an unresolved overload choice, not a conversion
		 * between Int and Int64. This is the sole solver rule allowed to
		 * replace its provisional default classifier. */
		ctx->classifier_solver.bindings[argument_operation] = domain;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, argument_operation) != 0) {
			return -1;
		}
	}
	*p_argument_classifier = domain;
	return 0;
}

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >= ctx->metadata->operation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (ctx->classifier_solver.bindings[operation] != PROTOTYPE_INVALID_ID) {
		return ctx->classifier_solver.bindings[operation];
	}
	uint32_t application_id =
		ctx->classifier_solver.ih_motive_application_ids[operation];
	if (application_id == PROTOTYPE_INVALID_ID ||
		application_id >= ctx->classifier_solver.motive_application_count) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t motive_operation = ctx->classifier_solver.motive_applications[
		application_id
	].motive_operation;
	if (motive_operation >= ctx->metadata->operation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	/* A constant candidate is a solver-side equation M(_) == T, so it can
	 * discharge the classifier query without pretending that M(argument) has
	 * already been built in TermDB. */
	return ctx->classifier_solver.motive_constant_candidates[motive_operation];
}

/* Solve a constructor APP from its dependent telescope. The curried cache is
 * used only to describe a residual, under-applied builder; a saturated value's
 * classifier is reindexed directly from the authoritative result schema. */
static int operation_solver_constructor_spine_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_APP ||
		operation->application_role !=
			PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
		return -1;
	}
	uint32_t reverse_argument_operations[64];
	uint32_t argument_operation_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->operation_count) {
		const struct prototype_operation_node* cursor_operation =
			&ctx->metadata->operations[cursor];
		if (cursor_operation->tag != PROTOTYPE_OPERATION_APP ||
			cursor_operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_operation_count >= 64 ||
			cursor_operation->argument >= ctx->metadata->operation_count) {
			return -1;
		}
		reverse_argument_operations[argument_operation_count++] =
			cursor_operation->argument;
		cursor = cursor_operation->function;
	}
	uint32_t argument_classifiers[64];
	for (uint32_t i = 0; i < argument_operation_count; ++i) {
		uint32_t argument_operation =
			reverse_argument_operations[argument_operation_count - i - 1];
		uint32_t argument_classifier =
			operation_solver_classifier(ctx, argument_operation);
		if (argument_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		argument_classifiers[i] = argument_classifier;
	}
	int saturated;
	return prototype_judgement_constructor_spine_classifier(
		ctx->terms,
		ctx->type_declarations,
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		operation->context_id,
		operation->core_term,
		PROTOTYPE_INVALID_ID,
		argument_classifiers,
		argument_operation_count,
		p_classifier,
		&saturated
	);
}

static int operation_solver_seed_motive(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	uint32_t source_body_operation,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct operation_classifier_goal* source_equation = NULL;
	for (uint32_t case_index = 0;
		case_index < ctx->metadata->operations[operation].case_count;
		++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation, case_index
			);
		if (!equation) {
			return -1;
		}
		if (equation->payload.motive_case.branch_operation == source_body_operation) {
			source_equation = equation;
			break;
		}
	}
	if (!source_equation) {
		return -1;
	}
	int capture_status = operation_classifier_captures_case_binder(
		ctx, source_equation, classifier
	);
	if (capture_status != 0) {
		return capture_status < 0 ? -1 : 1;
	}
	uint32_t previous =
		ctx->classifier_solver.motive_constant_candidates[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		ctx->classifier_solver.motive_constant_candidates[operation] = classifier;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
			return -1;
		}
		uint32_t ih_scope = PROTOTYPE_INVALID_ID;
		const struct prototype_operation_node* match_operation =
			&ctx->metadata->operations[operation];
		if (match_operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[match_operation->core_term].tag ==
				PROTOTYPE_TERM_MATCH) {
			ih_scope =
				ctx->terms->terms[match_operation->core_term].as.match.ih_scope_id;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->classifier_solver.constraint_count;
			++constraint_id) {
			const struct operation_classifier_goal* constraint =
				&ctx->classifier_solver.constraints[constraint_id];
			if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED ||
				constraint->classifier_variable >= ctx->metadata->operation_count ||
				ctx->metadata->operations[constraint->classifier_variable].first_case !=
					ih_scope) {
				continue;
			}
			if (operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return (prototype_judgement_classifier_conversion(
		ctx->terms, ctx->type_declarations, previous, classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : -1;
}

static int operation_solver_set_ih_motive_application(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t motive_operation,
	uint32_t argument_operation,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->operation_count ||
		motive_operation >= ctx->metadata->operation_count ||
		argument_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t previous =
		ctx->classifier_solver.ih_motive_application_ids[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		if (ctx->classifier_solver.motive_application_count >= 4096) {
			return -1;
		}
		uint32_t scope_frame = ctx->metadata->operations[operation].first_case;
		ctx->classifier_solver.motive_applications[
			ctx->classifier_solver.motive_application_count
		] = (struct operation_solver_motive_application){
			motive_operation, argument_operation, scope_frame
		};
		ctx->classifier_solver.ih_motive_application_ids[operation] =
			ctx->classifier_solver.motive_application_count++;
		*p_changed = 1;
		return 0;
	}
	if (previous >= ctx->classifier_solver.motive_application_count) {
		return -1;
	}
	const struct operation_solver_motive_application* application =
		&ctx->classifier_solver.motive_applications[previous];
	return application->motive_operation == motive_operation &&
		application->argument_operation == argument_operation ? 0 : -1;
}

static int operation_solver_add_input_fact(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier,
	uint32_t lambda_operation,
	uint32_t ast_binder_id
) {
	if (!ctx || subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->classifier_solver.input_fact_count; ++i) {
		const struct operation_solver_input_fact* fact =
			&ctx->classifier_solver.input_facts[i];
		if (fact->subject == subject && fact->classifier == classifier &&
			fact->lambda_operation == lambda_operation &&
			fact->ast_binder_id == ast_binder_id) {
			return 0;
		}
	}
	if (ctx->classifier_solver.input_fact_count >=
		PROTOTYPE_OPERATION_SOLVER_INPUT_FACT_CAPACITY) {
		return -1;
	}
	ctx->classifier_solver.input_facts[
		ctx->classifier_solver.input_fact_count++
	] = (struct operation_solver_input_fact){
		subject, classifier, lambda_operation, ast_binder_id
	};
	return 0;
}

static int operation_solver_lambda_ast_binder(
	const struct compile_context* ctx,
	uint32_t lambda_operation,
	uint32_t* p_ast_binder_id
) {
	if (!ctx || !ctx->metadata || !p_ast_binder_id ||
		lambda_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[lambda_operation];
	if (operation->tag != PROTOTYPE_OPERATION_LAMBDA) {
		return -1;
	}
	if (operation->source_ast < ctx->asts->node_count &&
		ctx->asts->nodes[operation->source_ast].tag == PROTOTYPE_AST_LAMBDA) {
		*p_ast_binder_id =
			ctx->asts->nodes[operation->source_ast].as.lambda.ast_binder_id;
		return 0;
	}
	if (operation->referenced_ast_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_ast_binder_id = operation->referenced_ast_binder_id;
	return 0;
}

static int operation_solver_initialize_input_facts(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
		uint32_t ast_binder_id;
		if (operation_solver_lambda_ast_binder(
				ctx, pending->source_operation, &ast_binder_id
			) != 0) {
			return -1;
		}
		if (operation_solver_add_input_fact(
				ctx,
				pending->binder_var,
				pending->classifier,
				pending->source_operation,
				ast_binder_id
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
		if (operation_solver_add_input_fact(
				ctx,
				pending->subject,
				pending->classifier,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_add_constraint_dependency(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t constraint
) {
	if (!ctx || operation == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (operation >= ctx->metadata->operation_count ||
		constraint >= ctx->classifier_solver.constraint_count ||
		ctx->classifier_solver.dependent_constraint_count >=
			PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY * 4) {
		return -1;
	}
	for (uint32_t dependency =
			ctx->classifier_solver.first_dependent_constraint[operation];
		dependency != PROTOTYPE_INVALID_ID;
		dependency =
			ctx->classifier_solver.dependent_constraints[dependency].next) {
		if (dependency >= ctx->classifier_solver.dependent_constraint_count) {
			return -1;
		}
		if (ctx->classifier_solver.dependent_constraints[dependency].constraint ==
			constraint) {
			return 0;
		}
	}
	uint32_t dependency = ctx->classifier_solver.dependent_constraint_count++;
	ctx->classifier_solver.dependent_constraints[dependency].constraint = constraint;
	ctx->classifier_solver.dependent_constraints[dependency].next =
		ctx->classifier_solver.first_dependent_constraint[operation];
	ctx->classifier_solver.first_dependent_constraint[operation] = dependency;
	return 0;
}

static int operation_solver_validate_classifier_goal(
	const struct compile_context* ctx,
	const struct operation_classifier_goal* goal
) {
	if (!ctx || !ctx->asts || !ctx->terms || !ctx->metadata || !goal ||
		goal->id >= ctx->classifier_solver.constraint_count ||
		goal->source_operation >= ctx->metadata->operation_count ||
		goal->classifier_variable >= ctx->metadata->operation_count ||
		goal->context_id >= ctx->metadata->contexts.context_count ||
		goal->context_id !=
			ctx->metadata->operations[goal->source_operation].context_id ||
		(goal->source_ast != PROTOTYPE_INVALID_ID &&
		 goal->source_ast >= ctx->asts->node_count)) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[goal->source_operation];
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE: {
			union operation_classifier_goal_payload empty_payload = { 0 };
			return memcmp(
				&goal->payload, &empty_payload, sizeof(goal->payload)
			) == 0 ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_EQUAL:
			return operation->tag == PROTOTYPE_OPERATION_NAME &&
				goal->payload.reference.referenced_operation == operation->function &&
				goal->payload.reference.referenced_operation <
					ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			return operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
				goal->payload.conversion.body_operation == operation->body &&
				goal->payload.conversion.body_operation <
					ctx->metadata->operation_count &&
				(goal->payload.conversion.expected_classifier == PROTOTYPE_INVALID_ID ||
				 (goal->payload.conversion.expected_classifier ==
					operation->known_classifier &&
				  goal->payload.conversion.expected_classifier <
					ctx->terms->term_count)) &&
				goal->payload.conversion.kernel_goal.id == goal->id &&
				goal->payload.conversion.kernel_goal.context_id == goal->context_id &&
				goal->payload.conversion.kernel_goal.carrier_classifier ==
					PROTOTYPE_INVALID_ID &&
				goal->payload.conversion.kernel_goal.left_term ==
					goal->payload.conversion.expected_classifier &&
				goal->payload.conversion.kernel_goal.right_term ==
					PROTOTYPE_INVALID_ID &&
				goal->payload.conversion.kernel_goal.normalization_profile ==
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF &&
				goal->payload.conversion.kernel_goal.step_limit ==
					ctx->metadata->normalization_step_limit ?
					0 : -1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
				return operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
					goal->payload.pi.body_or_function_operation == operation->body &&
					goal->payload.pi.body_or_function_operation <
						ctx->metadata->operation_count &&
					goal->payload.pi.domain_classifier_or_argument_operation ==
						operation->binder_classifier &&
					(operation->binder_classifier == PROTOTYPE_INVALID_ID ||
					 operation->binder_classifier < ctx->terms->term_count) ? 0 : -1;
			}
			return goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				operation->tag == PROTOTYPE_OPERATION_APP &&
				operation->application_role ==
					PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION &&
				goal->payload.pi.body_or_function_operation == operation->function &&
				goal->payload.pi.domain_classifier_or_argument_operation ==
					operation->argument &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			return operation->tag == PROTOTYPE_OPERATION_APP &&
				operation->application_role ==
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				goal->payload.constructor_formation.function_operation ==
					operation->function &&
				goal->payload.constructor_formation.argument_operation ==
					operation->argument &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
			const struct operation_classifier_motive_case_goal* motive =
				&goal->payload.motive_case;
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				motive->case_index >= operation->case_count ||
				operation->first_case + motive->case_index >=
					ctx->metadata->operation_case_count ||
				operation->core_term >= ctx->terms->term_count ||
				ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
				motive->branch_operation != ctx->metadata->operation_cases[
					operation->first_case + motive->case_index
				].body_operation || motive->branch_operation >=
					ctx->metadata->operation_count ||
				motive->scrutinee_operation != operation->scrutinee ||
				motive->scrutinee_operation >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_term* match =
				&ctx->terms->terms[operation->core_term];
			uint32_t term_case = match->as.match.first_case + motive->case_index;
			if (term_case >= ctx->terms->case_count) {
				return -1;
			}
			return motive->constructor_owner ==
					ctx->terms->cases[term_case].constructor_owner &&
				motive->constructor_index ==
					ctx->terms->cases[term_case].constructor_id &&
				motive->ih_scope_id == match->as.match.ih_scope_id ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_IH_EXPECTED:
			return operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS &&
				goal->payload.induction_hypothesis.recursive_argument_operation ==
					operation->argument && operation->argument <
					ctx->metadata->operation_count &&
				goal->payload.induction_hypothesis.ih_scope_id ==
					operation->first_case ? 0 : -1;
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			return (operation->tag == PROTOTYPE_OPERATION_RETURN ||
					operation->tag == PROTOTYPE_OPERATION_THUNK ||
					operation->tag == PROTOTYPE_OPERATION_FORCE) &&
				goal->payload.cbpv_boundary.child_operation == operation->argument &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			return operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
				goal->payload.computation.first_operation == operation->function &&
				goal->payload.computation.second_operation == operation->argument &&
				goal->payload.computation.third_operation == operation->scrutinee &&
				(operation->function == PROTOTYPE_INVALID_ID ||
				 operation->function < ctx->metadata->operation_count) &&
				(operation->argument == PROTOTYPE_INVALID_ID ||
				 operation->argument < ctx->metadata->operation_count) &&
				(operation->scrutinee == PROTOTYPE_INVALID_ID ||
				 operation->scrutinee < ctx->metadata->operation_count) ? 0 : -1;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			return operation->tag == PROTOTYPE_OPERATION_REQUEST &&
				goal->payload.computation.first_operation == operation->function &&
				goal->payload.computation.second_operation == operation->argument &&
				goal->payload.computation.third_operation == operation->body &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count &&
				operation->body < ctx->metadata->operation_count ? 0 : -1;
		default:
			return -1;
	}
}

static int operation_solver_index_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < 4096; ++i) {
		ctx->classifier_solver.first_dependent_constraint[i] =
			PROTOTYPE_INVALID_ID;
	}
	ctx->classifier_solver.dependent_constraint_count = 0;
	ctx->classifier_solver.worklist_head = 0;
	ctx->classifier_solver.worklist_count = 0;
	memset(
		ctx->classifier_solver.constraint_queued,
		0,
		sizeof(ctx->classifier_solver.constraint_queued)
	);
	for (uint32_t i = 0; i < ctx->classifier_solver.constraint_count; ++i) {
		const struct operation_classifier_goal* constraint =
			&ctx->classifier_solver.constraints[i];
		if (constraint->id != i ||
			operation_solver_validate_classifier_goal(ctx, constraint) != 0) {
			return -1;
		}
		if (operation_solver_add_constraint_dependency(
				ctx, constraint->classifier_variable, i
			) != 0) {
			return -1;
		}
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_EQUAL:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.reference.referenced_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONVERTIBLE:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.conversion.body_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_IH_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.induction_hypothesis.recursive_argument_operation,
						i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_PI_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.pi.body_or_function_operation, i
					) != 0 ||
					(constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
					operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.pi.domain_classifier_or_argument_operation,
						i
					) != 0)) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.constructor_formation.function_operation,
						i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.constructor_formation.argument_operation,
						i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.motive_case.branch_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.motive_case.scrutinee_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.cbpv_boundary.child_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.first_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.second_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.third_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_HAS_TYPE:
				break;
			default:
				return -1;
		}
		if (operation_solver_enqueue_constraint(ctx, i) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_collect_input_classifiers(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!ctx || !classifiers || !p_classifier_count ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	uint32_t subject = operation->core_term;
	if (subject >= ctx->terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (uint32_t i = 0; i < ctx->classifier_solver.input_fact_count; ++i) {
		const struct operation_solver_input_fact* fact =
			&ctx->classifier_solver.input_facts[i];
		if (fact->subject != subject ||
			(fact->ast_binder_id != PROTOTYPE_INVALID_ID &&
				(operation->tag != PROTOTYPE_OPERATION_VAR ||
				 operation->referenced_ast_binder_id != fact->ast_binder_id))) {
			continue;
		}
		int contains = graph_classifier_list_contains_normalization_equal(
			ctx, classifiers, *p_classifier_count, fact->classifier
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
		classifiers[(*p_classifier_count)++] = fact->classifier;
	}
	return 0;
}

static int operation_solver_generate_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->operation_count > 4096) {
		return -1;
	}
	memset(&ctx->classifier_solver, 0, sizeof(ctx->classifier_solver));
	for (uint32_t i = 0; i < 4096; ++i) {
		ctx->classifier_solver.bindings[i] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_constant_candidates[i] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_solution_states[i] =
			OPERATION_MOTIVE_SOLUTION_UNRESOLVED;
		ctx->classifier_solver.ih_motive_application_ids[i] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_terms[i] = PROTOTYPE_INVALID_ID;
	}
	if (operation_solver_initialize_input_facts(ctx) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		int base_constraint_kind = OPERATION_CONSTRAINT_HAS_TYPE;
		union operation_classifier_goal_payload base_payload = { 0 };
		if (operation->tag == PROTOTYPE_OPERATION_APP &&
			operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			base_constraint_kind = OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION;
			base_payload.constructor_formation.function_operation = operation->function;
			base_payload.constructor_formation.argument_operation = operation->argument;
		} else if (operation->tag == PROTOTYPE_OPERATION_RETURN ||
			operation->tag == PROTOTYPE_OPERATION_THUNK ||
			operation->tag == PROTOTYPE_OPERATION_FORCE) {
			base_constraint_kind = OPERATION_CONSTRAINT_CBPV_BOUNDARY;
			base_payload.cbpv_boundary.child_operation = operation->argument;
		} else if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
			base_constraint_kind = OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT;
			base_payload.computation.first_operation = operation->function;
			base_payload.computation.second_operation = operation->argument;
			base_payload.computation.third_operation = operation->scrutinee;
		} else if (operation->tag == PROTOTYPE_OPERATION_REQUEST) {
			base_constraint_kind = OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT;
			base_payload.computation.first_operation = operation->function;
			base_payload.computation.second_operation = operation->argument;
			base_payload.computation.third_operation = operation->body;
		}
		if (operation->classifier_variable != i ||
			operation_solver_add_classifier_goal(
				ctx,
				base_constraint_kind,
				i,
				base_payload
			) != 0) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.reference.referenced_operation = operation->function;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_EQUAL, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.conversion.body_operation = operation->body;
			payload.conversion.expected_classifier = operation->known_classifier;
			payload.conversion.kernel_goal =
				(struct prototype_kernel_conversion_goal){
					.id = ctx->classifier_solver.constraint_count,
					.context_id = operation->context_id,
					.carrier_classifier = PROTOTYPE_INVALID_ID,
					.left_term = operation->known_classifier,
					.right_term = PROTOTYPE_INVALID_ID,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
					.step_limit = ctx->metadata->normalization_step_limit
				};
			if (operation->body >= ctx->metadata->operation_count ||
				operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_CONVERTIBLE, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_LAMBDA) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_INTRO;
			payload.pi.body_or_function_operation = operation->body;
			payload.pi.domain_classifier_or_argument_operation =
				operation->binder_classifier;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_APP &&
			operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_ELIM;
			payload.pi.body_or_function_operation = operation->function;
			payload.pi.domain_classifier_or_argument_operation = operation->argument;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
			for (uint32_t j = 0; j < operation->case_count; ++j) {
				if (operation->first_case + j >= ctx->metadata->operation_case_count ||
					operation_solver_add_motive_case_goal(
						ctx,
						i,
						j,
						ctx->metadata->operation_cases[operation->first_case + j].body_operation,
						operation->scrutinee
					) != 0) {
					return -1;
				}
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.induction_hypothesis.recursive_argument_operation =
				operation->argument;
			payload.induction_hypothesis.ih_scope_id = operation->first_case;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_IH_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		}
	}
	return operation_solver_index_constraints(ctx);
}

static int operation_solver_seed_known_classifiers(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		uint32_t seed = operation->known_classifier != PROTOTYPE_INVALID_ID ?
			operation->known_classifier : operation->classifier;
		/*
		 * ASCRIPTION is solved against its body, and NAME follows its referenced
		 * source operation. Seeding either from the surface classifier creates a
		 * second authoritative path and can retain unsolved elaboration rows.
		 */
		if (operation->tag != PROTOTYPE_OPERATION_ASCRIPTION &&
			operation->tag != PROTOTYPE_OPERATION_NAME &&
			seed != PROTOTYPE_INVALID_ID &&
			operation_solver_bind(ctx, i, seed, p_changed) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_commit_bindings(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		uint32_t classifier = ctx->classifier_solver.bindings[i];
		if (classifier != PROTOTYPE_INVALID_ID &&
			ctx->metadata->operations[i].classifier != classifier) {
			ctx->metadata->operations[i].classifier = classifier;
			*p_changed = 1;
		}
	}
	return 0;
}

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
);

static int operation_subtree_ast_binder_use_count(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t ast_binder_id,
	uint8_t* active,
	uint32_t* p_count
);

static int operation_effect_row_binder_is_owned(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t binding_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t owner = 0; owner < ctx->metadata->operation_count; ++owner) {
		const struct prototype_operation_node* lambda =
			&ctx->metadata->operations[owner];
		if (lambda->tag != PROTOTYPE_OPERATION_LAMBDA) {
			continue;
		}
		int declares_binder = 0;
		for (uint32_t row = 0; row < lambda->implicit_effect_row_count; ++row) {
			if (lambda->implicit_effect_row_binders[row] == binding_id) {
				declares_binder = 1;
				break;
			}
		}
		if (!declares_binder) {
			continue;
		}
		uint8_t visited[ctx->metadata->operation_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, owner, operation_id, visited
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
	}
	return 0;
}

static int operation_classifier_contains_unowned_effect_row(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier
) {
	if (!ctx || !ctx->terms || !ctx->metadata ||
		operation_id >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)ctx->terms->term_count; ++i) {
		if (ctx->terms->terms[i].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t binding_id = ctx->terms->terms[i].as.effect_row_var.binding_id;
		if (prototype_term_contains_free_binding(
				ctx->terms,
				classifier,
			binding_id
		)) {
			int owned = operation_effect_row_binder_is_owned(
				ctx, operation_id, binding_id
			);
			if (owned < 0) {
				return -1;
			}
			if (!owned) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_effect_constraint_add(
	struct compile_context* ctx,
	int kind,
	uint32_t operation,
	uint32_t result_row,
	uint32_t left_row,
	uint32_t right_row
) {
	if (!ctx || !ctx->metadata || !ctx->metadata->effect_constraints ||
		operation >= ctx->metadata->operation_count ||
		result_row >= ctx->terms->term_count ||
		left_row >= ctx->terms->term_count ||
		(right_row != PROTOTYPE_INVALID_ID &&
			right_row >= ctx->terms->term_count)) {
		return -1;
	}
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		const struct prototype_operation_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (constraint->kind == kind &&
			constraint->operation == operation &&
			constraint->result_row == result_row &&
			constraint->left_row == left_row &&
			constraint->right_row == right_row) {
			return 0;
		}
	}
	if (ctx->metadata->effect_constraint_count >=
		ctx->metadata->effect_constraint_capacity) {
		return -1;
	}
	ctx->metadata->effect_constraints[
		ctx->metadata->effect_constraint_count++
	] = (struct prototype_operation_effect_constraint){
		.kind = kind,
		.state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING,
		.operation = operation,
		.result_row = result_row,
		.left_row = left_row,
		.right_row = right_row
	};
	return 0;
}

static int operation_classifier_computation_view(
	struct compile_context* ctx,
	uint32_t operation,
	struct prototype_term_classifier_view* p_view
) {
	if (!ctx || !p_view || operation >= ctx->metadata->operation_count) {
		return -1;
	}
	/* Effect equations validate the selected OperationGraph classifier. The
	 * classifier solver work cell may temporarily be ahead of or behind the
	 * committed occurrence during another fixed-point pass. */
	uint32_t classifier = ctx->metadata->operations[operation].classifier;
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			classifier,
			p_view
		) != 0) {
		return -1;
	}
	return p_view->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		p_view->computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		p_view->effect_row != PROTOTYPE_INVALID_ID ? 0 : 1;
}

static int operation_effect_row_meta_find_or_add(
	struct compile_context* ctx,
	uint32_t owner_operation,
	uint32_t placeholder_row,
	struct operation_effect_row_meta** p_meta
) {
	if (!ctx || !p_meta || owner_operation >= ctx->metadata->operation_count ||
		placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[placeholder_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->owner_operation == owner_operation &&
			meta->placeholder_row == placeholder_row) {
			*p_meta = meta;
			return 0;
		}
	}
	if (ctx->effect_solver.meta_count >=
		PROTOTYPE_OPERATION_EFFECT_ROW_META_CAPACITY) {
		return -1;
	}
	struct operation_effect_row_meta* meta =
		&ctx->effect_solver.metas[ctx->effect_solver.meta_count++];
	*meta = (struct operation_effect_row_meta){
		.owner_operation = owner_operation,
		.placeholder_row = placeholder_row,
		.first_solution_atom = PROTOTYPE_INVALID_ID,
		.solution_row = PROTOTYPE_INVALID_ID,
		.state = OPERATION_EFFECT_ROW_META_UNSOLVED
	};
	*p_meta = meta;
	return 0;
}

static int operation_effect_normal_add_atom(
	struct prototype_effect_row_normal_form* normal,
	uint32_t atom
) {
	if (!normal) {
		return -1;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (normal->atoms[i] == atom) {
			return 0;
		}
	}
	if (normal->atom_count >= PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	normal->atoms[normal->atom_count++] = atom;
	return 0;
}

static int operation_effect_normal_union(
	struct prototype_effect_row_normal_form* target,
	const struct prototype_effect_row_normal_form* source
) {
	if (!target || !source) {
		return -1;
	}
	target->effects |= source->effects;
	for (uint32_t i = 0; i < source->atom_count; ++i) {
		if (operation_effect_normal_add_atom(target, source->atoms[i]) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_normal_equal(
	const struct prototype_effect_row_normal_form* left,
	const struct prototype_effect_row_normal_form* right
) {
	return left && right && left->effects == right->effects &&
		prototype_term_effect_row_normal_form_includes(left, right) &&
		prototype_term_effect_row_normal_form_includes(right, left);
}

static int operation_effect_meta_solution_normal(
	const struct operation_effect_solver* solver,
	const struct operation_effect_row_meta* meta,
	struct prototype_effect_row_normal_form* p_normal
) {
	if (!solver || !meta || !p_normal ||
		meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
		meta->first_solution_atom > solver->solution_atom_count ||
		meta->solution_atom_count > solver->solution_atom_count -
			meta->first_solution_atom ||
		meta->solution_atom_count >
			PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	p_normal->effects = meta->solution_effects;
	p_normal->atom_count = meta->solution_atom_count;
	memcpy(
		p_normal->atoms,
		&solver->solution_atoms[meta->first_solution_atom],
		meta->solution_atom_count * sizeof(*p_normal->atoms)
	);
	return 0;
}

static int operation_effect_meta_store_solution(
	struct operation_effect_solver* solver,
	struct operation_effect_row_meta* meta,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!solver || !meta || !normal ||
		normal->atom_count > PROTOTYPE_OPERATION_EFFECT_ROW_SOLUTION_ATOM_CAPACITY -
			solver->solution_atom_count) {
		return -1;
	}
	if (meta->state == OPERATION_EFFECT_ROW_META_SOLVED) {
		struct prototype_effect_row_normal_form existing;
		return operation_effect_meta_solution_normal(
			solver, meta, &existing
		) == 0 && operation_effect_normal_equal(&existing, normal) ? 0 : -1;
	}
	meta->solution_effects = normal->effects;
	meta->first_solution_atom = solver->solution_atom_count;
	meta->solution_atom_count = normal->atom_count;
	memcpy(
		&solver->solution_atoms[solver->solution_atom_count],
		normal->atoms,
		normal->atom_count * sizeof(*normal->atoms)
	);
	solver->solution_atom_count += normal->atom_count;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
	return 0;
}

static int operation_effect_resolve_row_normal(
	struct compile_context* ctx,
	uint32_t owner_operation,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
) {
	struct prototype_effect_row_normal_form source;
	if (!ctx || !p_normal ||
		prototype_term_effect_row_normal_form(ctx->terms, row, &source) != 0) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	p_normal->effects = source.effects;
	for (uint32_t i = 0; i < source.atom_count; ++i) {
		uint32_t atom = source.atoms[i];
		struct operation_effect_row_meta* solved_meta = NULL;
		if (atom < ctx->terms->term_count &&
			ctx->terms->terms[atom].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			for (uint32_t j = 0; j < ctx->effect_solver.meta_count; ++j) {
				struct operation_effect_row_meta* candidate =
					&ctx->effect_solver.metas[j];
				if (candidate->owner_operation == owner_operation &&
					candidate->placeholder_row == atom &&
					candidate->state == OPERATION_EFFECT_ROW_META_SOLVED) {
					solved_meta = candidate;
					break;
				}
			}
		}
		if (solved_meta) {
			struct prototype_effect_row_normal_form solution;
			if (operation_effect_meta_solution_normal(
					&ctx->effect_solver, solved_meta, &solution
				) != 0 || operation_effect_normal_union(p_normal, &solution) != 0) {
				return -1;
			}
		} else if (operation_effect_normal_add_atom(p_normal, atom) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_materialize_solution(
	struct compile_context* ctx,
	struct operation_effect_row_meta* meta,
	uint32_t replacement
) {
	if (!ctx || !meta || replacement >= ctx->terms->term_count ||
		meta->owner_operation >= ctx->metadata->operation_count ||
		meta->placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[meta->placeholder_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t owner = meta->owner_operation;
	uint32_t binding_id = ctx->terms->terms[
		meta->placeholder_row
	].as.effect_row_var.binding_id;
	uint32_t* classifiers[] = {
		&ctx->metadata->operations[owner].known_classifier,
		&ctx->metadata->operations[owner].classifier,
		&ctx->metadata->operations[owner].binder_classifier,
		&ctx->classifier_solver.bindings[owner]
	};
	for (size_t i = 0; i < sizeof(classifiers) / sizeof(classifiers[0]); ++i) {
		if (*classifiers[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms,
				ctx->type_declarations,
				*classifiers[i],
				binding_id,
				replacement,
				classifiers[i]
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		struct prototype_operation_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (constraint->operation != owner) {
			continue;
		}
		uint32_t* rows[] = {
			&constraint->result_row,
			&constraint->left_row,
			&constraint->right_row
		};
		for (size_t j = 0; j < sizeof(rows) / sizeof(rows[0]); ++j) {
			if (*rows[j] == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (prototype_term_graph_substitute_bound_var(
					ctx->terms,
					ctx->type_declarations,
					*rows[j],
					binding_id,
					replacement,
					rows[j]
				) != 0) {
				return -1;
			}
		}
	}
	meta->solution_row = replacement;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
	return 0;
}

static int operation_effect_row_is_ground(
	const struct prototype_term_db* terms,
	uint32_t row,
	uint32_t depth
) {
	if (!terms || row >= terms->term_count || depth > 256) {
		return 0;
	}
	const struct prototype_term* term = &terms->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_LABEL:
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return operation_effect_row_is_ground(
				terms, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return operation_effect_row_is_ground(
					terms, term->as.effect_row_union.left, depth + 1
				) && operation_effect_row_is_ground(
					terms, term->as.effect_row_union.right, depth + 1
				);
		default:
			return 0;
	}
}

static int operation_effect_normal_is_ground(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!terms || !normal) {
		return 0;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (!operation_effect_row_is_ground(terms, normal->atoms[i], 0)) {
			return 0;
		}
	}
	return 1;
}

static int operation_effect_solve_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
			struct prototype_operation_effect_constraint* constraint =
				&ctx->metadata->effect_constraints[i];
			if ((constraint->kind ==
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT ||
				 constraint->kind ==
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY) &&
				constraint->result_row == constraint->left_row) {
				int unowned = operation_classifier_contains_unowned_effect_row(
					ctx,
					constraint->operation,
					ctx->metadata->operations[constraint->operation].classifier
				);
				if (unowned < 0) {
					return -1;
				}
				int has_external_ref = unowned != 0 ?
					operation_subtree_contains_external_ref(
						ctx, constraint->operation
					) : 0;
				if (has_external_ref < 0) {
					return -1;
				}
				/* The identity equation adds no obligation when every symbolic
				 * row is scoped by this Operation. A free external row remains a
				 * residual link-time obligation even though rho = rho is trivial. */
				constraint->state = unowned != 0 && has_external_ref != 0 ?
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL :
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
				continue;
			}
			struct prototype_effect_row_normal_form left;
			struct prototype_effect_row_normal_form right;
			struct prototype_effect_row_normal_form expected;
			struct prototype_effect_row_normal_form actual;
			if (operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->left_row, &left
				) != 0 ||
				(constraint->right_row != PROTOTYPE_INVALID_ID &&
				 operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->right_row, &right
				 ) != 0) ||
				operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->result_row, &actual
				) != 0) {
				return -1;
			}
			memset(&expected, 0, sizeof(expected));
			switch (constraint->kind) {
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT:
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY:
					expected = left;
					break;
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION:
					expected = left;
					if (operation_effect_normal_union(&expected, &right) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_RESIDUAL:
					unsigned covered_effects = left.effects;
					for (uint32_t atom_index = 0;
						atom_index < left.atom_count;
						++atom_index) {
						uint32_t atom = left.atoms[atom_index];
						if (atom < ctx->terms->term_count &&
							ctx->terms->terms[atom].tag ==
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							const struct prototype_effect_operation_declaration* declaration =
								prototype_term_effect_operation_declaration(
									ctx->terms->terms[atom].as.effect_row_operation.operation_id
								);
							if (declaration) {
								covered_effects |= declaration->operation_labels;
							}
						}
					}
					if (right.atom_count != 0 ||
						(covered_effects & right.effects) != right.effects) {
						constraint->state =
							PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
						continue;
					}
					expected.effects = left.effects;
					expected.effects &= ~right.effects;
					for (uint32_t atom_index = 0;
						atom_index < left.atom_count;
						++atom_index) {
						uint32_t atom = left.atoms[atom_index];
						int handled = 0;
						if (atom < ctx->terms->term_count &&
							ctx->terms->terms[atom].tag ==
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							const struct prototype_effect_operation_declaration* declaration =
								prototype_term_effect_operation_declaration(
									ctx->terms->terms[atom].as.effect_row_operation.operation_id
								);
							handled = declaration &&
								(declaration->operation_labels & right.effects) ==
									declaration->operation_labels;
						}
						if (!handled && operation_effect_normal_add_atom(
								&expected, atom
							) != 0) {
							return -1;
						}
					}
					break;
				default:
					return -1;
			}
			if (operation_effect_normal_equal(&actual, &expected)) {
				constraint->state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
				continue;
			}
			if (constraint->result_row >= ctx->terms->term_count ||
				ctx->terms->terms[constraint->result_row].tag !=
					PROTOTYPE_TERM_EFFECT_ROW_VAR) {
				if (operation_effect_normal_is_ground(ctx->terms, &actual) &&
					operation_effect_normal_is_ground(ctx->terms, &expected)) {
					return -1;
					}
				constraint->state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			uint32_t result_binder = ctx->terms->terms[
				constraint->result_row
			].as.effect_row_var.binding_id;
			int owned = operation_effect_row_binder_is_owned(
				ctx, constraint->operation, result_binder
			);
			if (owned < 0) {
				return -1;
			}
			if (owned) {
				constraint->state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			if (!operation_effect_normal_is_ground(ctx->terms, &expected)) {
				constraint->state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			struct operation_effect_row_meta* meta;
			if (operation_effect_row_meta_find_or_add(
					ctx, constraint->operation, constraint->result_row, &meta
				) != 0) {
				return -1;
			}
			int was_unsolved = meta->state == OPERATION_EFFECT_ROW_META_UNSOLVED;
			if (operation_effect_meta_store_solution(
					&ctx->effect_solver, meta, &expected
				) != 0) {
				return -1;
			}
			constraint->state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
			if (was_unsolved) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
			meta->solution_row != PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_effect_row_normal_form solution;
		uint32_t replacement;
		if (operation_effect_meta_solution_normal(
				&ctx->effect_solver, meta, &solution
			) != 0 || prototype_term_effect_row_materialize_normal_form(
				ctx->terms, &solution, &replacement
			) != 0 || operation_effect_materialize_solution(
				ctx, meta, replacement
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_generate_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* These constraints are a derived view of the current operation
	 * classifiers. A fixed-point round may replace a symbolic row with a
	 * concrete one, so retaining constraints from an earlier round creates
	 * false residual obligations. */
	ctx->metadata->effect_constraint_count = 0;
	uint32_t empty_row;
	if (prototype_term_effect_label(
			ctx->terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_row
		) != 0) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		struct prototype_term_classifier_view result_view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &result_view
			) != 0) {
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_RETURN) {
			if (operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT,
					operation_id,
					result_view.effect_row,
					empty_row,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_NAME ||
			operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ||
			operation->tag == PROTOTYPE_OPERATION_FORCE) {
			uint32_t child = operation->tag == PROTOTYPE_OPERATION_NAME ?
				operation->function :
				(operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ?
					operation->body : operation->argument);
			struct prototype_term_classifier_view child_view;
			if (operation_classifier_computation_view(
					ctx, child, &child_view
				) == 0 && operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					child_view.effect_row,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
			operation->case_count != 0) {
			uint32_t accumulated = PROTOTYPE_INVALID_ID;
			for (uint32_t case_index = 0;
				case_index < operation->case_count;
				++case_index) {
				uint32_t graph_case = operation->first_case + case_index;
				if (graph_case >= ctx->metadata->operation_case_count) {
					return -1;
				}
				struct prototype_term_classifier_view case_view;
				if (operation_classifier_computation_view(
						ctx,
						ctx->metadata->operation_cases[
							graph_case
						].body_operation,
						&case_view
					) != 0) {
					accumulated = PROTOTYPE_INVALID_ID;
					break;
				}
				if (accumulated == PROTOTYPE_INVALID_ID) {
					accumulated = case_view.effect_row;
					continue;
				}
				uint32_t union_row;
				if (prototype_term_effect_row_union(
						ctx->terms,
						accumulated,
						case_view.effect_row,
						&union_row
					) != 0 || operation_effect_constraint_add(
						ctx,
						PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION,
						operation_id,
						union_row,
						accumulated,
						case_view.effect_row
					) != 0) {
					return -1;
				}
				accumulated = union_row;
			}
			if (accumulated != PROTOTYPE_INVALID_ID &&
				operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					accumulated,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		}
		int has_constraint = 0;
		for (size_t constraint_id = 0;
			constraint_id < ctx->metadata->effect_constraint_count;
			++constraint_id) {
			if (ctx->metadata->effect_constraints[constraint_id].operation ==
				operation_id) {
				has_constraint = 1;
				break;
			}
		}
		if (!has_constraint && operation_effect_constraint_add(
				ctx,
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
				operation_id,
				result_view.effect_row,
				result_view.effect_row,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
	}
	for (size_t constraint_id = 0;
		constraint_id < ctx->judgement_delta.effect_row_constraint_count;
		++constraint_id) {
		const struct prototype_judgement_effect_row_constraint* row_constraint =
			&ctx->judgement_delta.effect_row_constraints[constraint_id];
		if (row_constraint->operand_count != 2 ||
			row_constraint->kind ==
				PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			continue;
		}
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->operation_count;
			++operation_id) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[operation_id];
			if (operation->core_term != row_constraint->subject) {
				continue;
			}
			struct prototype_term_classifier_view view;
			if (operation_classifier_computation_view(
					ctx, operation_id, &view
				) != 0 ||
				!(prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					view.effect_row,
					row_constraint->result_row
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				continue;
			}
			int kind = row_constraint->kind ==
				PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN ?
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION :
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_RESIDUAL;
			if (operation_effect_constraint_add(
					ctx,
					kind,
					operation_id,
					row_constraint->result_row,
					row_constraint->operands[0],
					row_constraint->operands[1]
				) != 0) {
				return -1;
			}
		}
	}
	return operation_effect_solve_constraints(ctx);
}

static size_t operation_effect_unresolved_count(
	const struct prototype_compile_metadata* metadata
) {
	if (!metadata) {
		return 0;
	}
	size_t count = 0;
	for (size_t i = 0; i < metadata->effect_constraint_count; ++i) {
		if (metadata->effect_constraints[i].state !=
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
			count++;
		}
	}
	return count;
}

static int operation_solver_require_complete(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		switch (operation->tag) {
			case PROTOTYPE_OPERATION_APP:
			case PROTOTYPE_OPERATION_LAMBDA:
			case PROTOTYPE_OPERATION_MATCH:
			case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
				if (ctx->classifier_solver.bindings[i] ==
					PROTOTYPE_INVALID_ID) {
					return -1;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

static int compile_phase_record_residual_dependent_folds(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count != 0 ||
			operation->function >= ctx->metadata->operation_count ||
			operation->argument >= ctx->metadata->operation_count) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(ctx, operation->function);
		uint32_t continuation_classifier = operation_solver_classifier(
			ctx, operation->argument
		);
		struct prototype_term_classifier_view input_view;
		uint32_t domain;
		uint32_t classifier_family;
		uint32_t continuation_binder_id;
		uint32_t codomain;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			continuation_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_judgement_pi_parts(
				ctx->terms, continuation_classifier, &domain, &classifier_family
			) != 0 || !(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, domain, input_view.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_pure_family_parts(
				ctx->terms, classifier_family, &continuation_binder_id, &codomain
			) != 0 || !prototype_term_contains_free_binding(
				ctx->terms, codomain, continuation_binder_id
			)) {
			continue;
		}
		struct prototype_term_normalization_result normalized;
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				ctx->terms->terms[operation->core_term].as.computation_fold.computation,
				ctx->metadata->normalization_step_limit,
				&normalized
			) != 0 || normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (UINT64_MAX - ctx->metadata->normalization_steps_used < normalized.steps_used) {
			return -1;
		}
		ctx->metadata->normalization_steps_used += normalized.steps_used;
		if (normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			continue;
		}
		if (normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT &&
			normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_operation(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		int already_recorded = find_status == 0;
		if (!already_recorded && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.operation = operation_id,
					.core_term = operation->core_term,
					.computation_operation = operation->function,
					.continuation_operation = operation->argument,
					.continuation_binder_id = continuation_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int compile_phase_record_residual_computation_fold_results(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			operation->function >= ctx->metadata->operation_count ||
			operation->scrutinee >= ctx->metadata->operation_count ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(ctx, operation->function);
		uint32_t return_classifier = operation_solver_classifier(ctx, operation->scrutinee);
		struct prototype_term_classifier_view input_view;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			return_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!prototype_term_contains_free_binding(
				ctx->terms, return_classifier, operation->fold_return_binder_id
			)) {
			continue;
		}
		uint32_t classifier_family;
		if (prototype_term_pure_family(
				ctx->terms,
				operation->fold_return_binder_id,
				return_classifier,
				&classifier_family
			) != 0) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_operation(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		if (find_status > 0 && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.operation = operation_id,
					.core_term = operation->core_term,
					.computation_operation = operation->function,
					.continuation_operation = operation->scrutinee,
					.continuation_binder_id =
						operation->fold_return_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_generalize_lambda_effect_rows(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!ctx || !ctx->metadata || !p_ret || operation_id >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
		operation->implicit_effect_row_count > 16) {
		return -1;
	}
	for (uint32_t i = operation->implicit_effect_row_count; i > 0; --i) {
		if (prototype_term_effect_row_forall(
					ctx->terms,
					operation->implicit_effect_row_binders[i - 1],
					classifier,
					&classifier
				) != 0) {
			return -1;
		}
	}
	*p_ret = classifier;
	return 0;
}

static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
);

static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_solver_classifier_goal_is_satisfied(
	const struct compile_context* ctx,
	const struct operation_classifier_goal* goal,
	int* p_reason
) {
	if (!ctx || !ctx->metadata || !goal || !p_reason ||
		goal->classifier_variable >= ctx->metadata->operation_count) {
		return 0;
	}
	uint32_t target = operation_solver_classifier(
		ctx, goal->classifier_variable
	);
	if (target == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_EQUAL: {
			uint32_t referenced = operation_solver_classifier(
				ctx, goal->payload.reference.referenced_operation
			);
			if (referenced == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, target, referenced
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_REFERENCE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			if (goal->payload.conversion.kernel_goal.result.status !=
				PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (operation_solver_classifier(
					ctx, goal->payload.pi.body_or_function_operation
				) == PROTOTYPE_INVALID_ID ||
				(goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				 operation_solver_classifier(
					ctx,
					goal->payload.pi.domain_classifier_or_argument_operation
				 ) == PROTOTYPE_INVALID_ID)) {
				return 0;
			}
			*p_reason = goal->payload.pi.role ==
				OPERATION_CLASSIFIER_PI_GOAL_INTRO ?
				OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED :
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			if (operation_solver_classifier(
					ctx,
					goal->payload.constructor_formation.function_operation
				) == PROTOTYPE_INVALID_ID || operation_solver_classifier(
					ctx,
					goal->payload.constructor_formation.argument_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
			/* A Match may already carry a validated expected classifier, in which
			 * case no synthesized motive term is materialized. The semantic case
			 * goal is discharged once both that Match classifier and this branch
			 * classifier exist; JudgementDelta later validates the exact rule. */
			if (operation_solver_classifier(
					ctx, goal->payload.motive_case.branch_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_IH_EXPECTED:
			if (operation_solver_classifier(
					ctx,
					goal->payload.induction_hypothesis.recursive_argument_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			if (operation_solver_classifier(
					ctx, goal->payload.cbpv_boundary.child_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_COMPUTATION_FOLD_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_OPERATION_REQUEST_VALIDATED;
			return 1;
		default:
			return 0;
	}
}

static void operation_solver_refresh_constraint_states(
	struct compile_context* ctx,
	int incomplete
) {
	if (!ctx || !ctx->metadata) {
		return;
	}
	ctx->metadata->solver_constraint_count = ctx->classifier_solver.constraint_count;
	ctx->metadata->solver_solved_count = 0;
	ctx->metadata->solver_residual_count = 0;
	ctx->metadata->solver_incomplete_count = 0;
	for (uint32_t i = 0; i < ctx->classifier_solver.constraint_count; ++i) {
		struct operation_classifier_goal* constraint =
			&ctx->classifier_solver.constraints[i];
		if (constraint->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int residual = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			(constraint->payload.conversion.kernel_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_RESIDUAL ||
			 constraint->payload.conversion.kernel_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT);
		int goal_incomplete = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			constraint->payload.conversion.kernel_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_EXHAUSTED;
		for (size_t obligation_id = 0;
			obligation_id <
				prototype_verification_db_count(&ctx->metadata->verification);
			++obligation_id) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(
					&ctx->metadata->verification, (uint32_t)obligation_id
				);
			if (!obligation) {
				continue;
			}
			if (obligation->operation == constraint->classifier_variable) {
				residual = 1;
				break;
			}
		}
		int solved_reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		int solved = operation_solver_classifier_goal_is_satisfied(
			ctx, constraint, &solved_reason
		);
		constraint->state = residual ? OPERATION_CONSTRAINT_STATE_RESIDUAL :
			(solved ? OPERATION_CONSTRAINT_STATE_SOLVED :
			(incomplete || goal_incomplete ? OPERATION_CONSTRAINT_STATE_INCOMPLETE :
				OPERATION_CONSTRAINT_STATE_PENDING));
		constraint->reason = residual ?
			(constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE ?
				OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL :
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION) :
			(solved ? solved_reason :
			(incomplete || goal_incomplete ?
				OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET :
				OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY));
		if (constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		ctx->metadata->effect_constraint_count;
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		struct prototype_operation_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (incomplete &&
			constraint->state == PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING) {
			constraint->state =
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_INCOMPLETE;
		}
		if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
}

static int operation_solver_propagate_zero_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t sequence_fold_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		sequence_fold_operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* sequence_fold_operation =
		&ctx->metadata->operations[sequence_fold_operation_id];
	if (sequence_fold_operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
		sequence_fold_operation->function >= ctx->metadata->operation_count ||
		sequence_fold_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		ctx->terms->terms[sequence_fold_operation->core_term].as.computation_fold.clause_count != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, sequence_fold_operation->function
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	const struct prototype_term* sequence_fold_term =
		&ctx->terms->terms[sequence_fold_operation->core_term];
	if (sequence_fold_term->as.computation_fold.return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		sequence_fold_operation->argument >= ctx->metadata->operation_count ||
		ctx->metadata->operations[sequence_fold_operation->argument].tag !=
			PROTOTYPE_OPERATION_LAMBDA) {
		return -1;
	}
	struct prototype_operation_node* continuation_operation =
		&ctx->metadata->operations[sequence_fold_operation->argument];
	if (continuation_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
		continuation_operation->binder_classifier = input_view.result;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(
				ctx, sequence_fold_operation->argument
			) != 0) {
			return -1;
		}
	} else if (!(prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			continuation_operation->binder_classifier,
			input_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t binder_var;
	if (prototype_term_var(
			ctx->terms,
			ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].as.lambda.binding_id,
			&binder_var
		) != 0) {
		return -1;
	}
	uint8_t visited[ctx->metadata->operation_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		memset(visited, 0, sizeof(visited));
		int binder_matches =
			continuation_operation->referenced_ast_binder_id != PROTOTYPE_INVALID_ID ?
				operation->referenced_ast_binder_id ==
					continuation_operation->referenced_ast_binder_id :
				operation->core_term == binder_var;
		if (operation->tag == PROTOTYPE_OPERATION_VAR && binder_matches &&
			operation_subtree_contains_operation(
				ctx,
				sequence_fold_operation->argument,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
		}
	}
	uint32_t continuation_classifier = operation_solver_classifier(
		ctx, sequence_fold_operation->argument
	);
	if (continuation_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t sequence_fold_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			ctx->terms,
			ctx->type_declarations,
			sequence_fold_term->as.computation_fold.computation,
			input_classifier,
			continuation_classifier,
			&sequence_fold_classifier
		);
		if (status < 0 || (status == 0 && operation_solver_widen_computation_binding(
				ctx, sequence_fold_operation_id, sequence_fold_classifier, p_changed
			) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_propagate_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t handle_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		handle_operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* handle_operation =
		&ctx->metadata->operations[handle_operation_id];
	if (handle_operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
		handle_operation->function >= ctx->metadata->operation_count ||
		handle_operation->scrutinee >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, handle_operation->function
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint8_t visited[ctx->metadata->operation_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation->binder_classifier == PROTOTYPE_INVALID_ID) {
			operation->binder_classifier = input_view.result;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
				return -1;
			}
		}
		memset(visited, 0, sizeof(visited));
		if (operation->tag == PROTOTYPE_OPERATION_VAR &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation_subtree_contains_operation(
				ctx,
				handle_operation->scrutinee,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
		}
	}
	if (handle_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[handle_operation->core_term].tag !=
			PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold_term =
		&ctx->terms->terms[handle_operation->core_term];
	if ((size_t)fold_term->as.computation_fold.first_clause +
			fold_term->as.computation_fold.clause_count >
			ctx->terms->computation_fold_clause_count ||
		handle_operation->fold_clause_count !=
			fold_term->as.computation_fold.clause_count ||
		(size_t)handle_operation->first_fold_clause +
			handle_operation->fold_clause_count >
			ctx->metadata->operation_fold_clause_count) {
		return -1;
	}
	uint32_t return_body_classifier = operation_solver_classifier(
		ctx, handle_operation->scrutinee
	);
	struct prototype_term_classifier_view return_body_view;
	if (return_body_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			return_body_classifier,
			&return_body_view
		) != 0 || return_body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		return_body_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	uint32_t fold_effect_row = return_body_view.effect_row;
	unsigned handled_effects = 0;
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_operation_computation_fold_clause* occurrence_clause =
			&ctx->metadata->operation_fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		handled_effects |= declaration->operation_labels;
		uint32_t clause_body_classifier = operation_solver_classifier(
			ctx, occurrence_clause->body_operation
		);
		if (clause_body_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_term_classifier_view clause_body_view;
		uint32_t joined_row;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				clause_body_classifier,
				&clause_body_view
			) != 0 || clause_body_view.category !=
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			clause_body_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_union(
				ctx->terms,
				fold_effect_row,
				clause_body_view.effect_row,
				&joined_row
			) != 0) {
			return -1;
		}
		fold_effect_row = joined_row;
	}
	unsigned input_effects;
	if (prototype_term_effect_row_closed_bits(
			ctx->terms, input_view.effect_row, &input_effects
		) == 0 && (input_effects & handled_effects) == handled_effects) {
		uint32_t residual_row;
		uint32_t joined_row;
		if (prototype_term_effect_label(
				ctx->terms, input_effects & ~handled_effects, &residual_row
			) != 0 || prototype_term_effect_row_union(
				ctx->terms, fold_effect_row, residual_row, &joined_row
			) != 0) {
			return -1;
		}
		fold_effect_row = joined_row;
	}
	uint32_t fold_output_classifier;
	unsigned closed_fold_effects;
	if (prototype_term_computation_type(
			ctx->terms,
			fold_effect_row,
			return_body_view.result,
			&fold_output_classifier
		) != 0) {
		return -1;
	}
	/* A symbolic clause row is precisely the case in which the operation
	 * domain must still be propagated into the clause binders below.  Defer
	 * publishing the fold result, but do not return before that propagation. */
	if (prototype_term_effect_row_closed_bits(
			ctx->terms, fold_effect_row, &closed_fold_effects
		) == 0) {
		(void)closed_fold_effects;
		if (operation_solver_widen_computation_binding(
				ctx, handle_operation_id, fold_output_classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_operation_computation_fold_clause* occurrence_clause =
			&ctx->metadata->operation_fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		if (declaration->resumption_multiplicity !=
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT) {
			uint8_t active[ctx->metadata->operation_count];
			uint32_t continuation_uses = 0;
			memset(active, 0, sizeof(active));
			if (operation_subtree_ast_binder_use_count(
					ctx,
					occurrence_clause->body_operation,
					occurrence_clause->continuation_ast_binder_id,
					active,
					&continuation_uses
				) != 0 ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE &&
					continuation_uses != 0) ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
					continuation_uses > 1)) {
				return -1;
			}
		}
		uint32_t outer_lambda = core_clause->body;
		if (outer_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[outer_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t inner_lambda = ctx->terms->terms[outer_lambda].as.lambda.body;
		if (inner_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[inner_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t operation_classifier = operation_solver_classifier(
			ctx, occurrence_clause->operation_operation
		);
		uint32_t operation_classifier_whnf;
		uint32_t operation_domain;
		uint32_t operation_codomain_family;
		if (operation_classifier == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		int specialization_status =
			prototype_judgement_specialize_fold_operation_classifier(
				ctx->terms,
				ctx->type_declarations,
				input_view.effect_row,
				operation_identity,
				operation_classifier,
				&operation_classifier
			);
		if (specialization_status < 0) {
			return -1;
		}
		/* A symbolic input row cannot yet specialize a higher-order operation's
		 * delayed-computation domain. Its generic Pi still determines the request
		 * result and therefore the resumption binder. Propagate that classifier
		 * now; a later fold pass may replace the row-polymorphic domain with the
		 * specialized one. */
		if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			operation_classifier,
			&operation_classifier_whnf
			) != 0) {
			return -1;
		}
		for (uint32_t depth = 0;
			depth < 32 && operation_classifier_whnf < ctx->terms->term_count &&
			ctx->terms->terms[operation_classifier_whnf].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_FORALL;
			++depth) {
			operation_classifier_whnf = ctx->terms->terms[
				operation_classifier_whnf
			].as.effect_row_forall.body;
			if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				operation_classifier_whnf,
				&operation_classifier_whnf
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_pi_parts(
				ctx->terms,
				operation_classifier_whnf,
				&operation_domain,
				&operation_codomain_family
			) != 0) {
			return -1;
		}
		uint32_t operation_result_binder;
		uint32_t operation_result_computation;
		struct prototype_term_classifier_view operation_result_view;
		if (
			prototype_term_pure_family_parts(
				ctx->terms,
				operation_codomain_family,
				&operation_result_binder,
				&operation_result_computation
			) != 0 || prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				operation_result_computation,
				&operation_result_view
			) != 0 || operation_result_view.category !=
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_result_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return 0;
		}
		(void)operation_result_binder;
		uint32_t continuation_function_classifier;
		uint32_t continuation_binder_classifier;
		if (prototype_term_pi(
				ctx->terms,
				operation_result_view.result,
				fold_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				ctx->terms,
				continuation_function_classifier,
				&continuation_binder_classifier
			) != 0) {
			return -1;
		}
		/* A clause argument has the operation domain even when the source body
	 * discards it. VAR occurrences are usage evidence, not typing authority. */
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->operation_count;
		++lambda_operation_id) {
		struct prototype_operation_node* lambda_operation =
			&ctx->metadata->operations[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			lambda_operation->core_term != outer_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id != occurrence_clause->argument_ast_binder_id) {
			continue;
		}
		if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
			!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				operation_domain
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			lambda_operation->binder_classifier = operation_domain;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
				return -1;
			}
		}
	}
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->operation_count;
		++lambda_operation_id) {
		struct prototype_operation_node* lambda_operation =
			&ctx->metadata->operations[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			lambda_operation->core_term != inner_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id !=
				occurrence_clause->continuation_ast_binder_id) {
			continue;
		}
			if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (operation_solver_continuation_effect_widening(
					ctx,
					lambda_operation->binder_classifier,
					continuation_binder_classifier
				)) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				continuation_binder_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		uint32_t lambda_term;
		if (operation->tag != PROTOTYPE_OPERATION_VAR) {
			continue;
		}
		if (operation->referenced_ast_binder_id ==
			occurrence_clause->argument_ast_binder_id) {
			lambda_term = outer_lambda;
		} else if (operation->referenced_ast_binder_id ==
			occurrence_clause->continuation_ast_binder_id) {
			lambda_term = inner_lambda;
		} else {
			continue;
		}
		memset(visited, 0, sizeof(visited));
		if (operation_subtree_contains_operation(
				ctx,
				occurrence_clause->body_operation,
				operation_id,
				visited
			) <= 0) {
			continue;
		}
		uint32_t classifier =
			operation->referenced_ast_binder_id ==
				occurrence_clause->argument_ast_binder_id ?
				operation_domain : PROTOTYPE_INVALID_ID;
		for (uint32_t lambda_operation_id = 0;
			classifier == PROTOTYPE_INVALID_ID &&
				lambda_operation_id < ctx->metadata->operation_count;
			++lambda_operation_id) {
			const struct prototype_operation_node* lambda_operation =
				&ctx->metadata->operations[lambda_operation_id];
			uint32_t ast_binder_id;
			if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
				lambda_operation->core_term != lambda_term ||
				operation_solver_lambda_ast_binder(
					ctx, lambda_operation_id, &ast_binder_id
				) != 0 ||
				ast_binder_id != operation->referenced_ast_binder_id) {
				continue;
			}
			if (lambda_operation->binder_classifier != PROTOTYPE_INVALID_ID) {
				classifier = lambda_operation->binder_classifier;
				break;
			}
			if (lambda_operation->body >= ctx->metadata->operation_count) {
				continue;
			}
			const struct prototype_operation_node* lambda_body_operation =
				&ctx->metadata->operations[lambda_operation->body];
			const struct prototype_context* lambda_binder_context =
				prototype_context_get(
					&ctx->metadata->contexts, lambda_body_operation->context_id
				);
			uint32_t binder_var;
			if (!lambda_binder_context ||
				lambda_binder_context->parent != lambda_operation->context_id ||
				prototype_term_var(
					ctx->terms, lambda_binder_context->binding_id, &binder_var
				) != 0) {
				continue;
			}
			for (size_t relation_id = 0;
				relation_id < ctx->judgement_delta.proposition_count;
				++relation_id) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[relation_id];
				uint32_t assumption_context_id;
				if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					relation->subject != binder_var ||
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) != 0 ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						relation->context_id,
						ctx->terms->terms[relation->subject].as.var.binding_id,
						&assumption_context_id
					) != 0) {
					continue;
				}
				const struct prototype_context* assumption_context =
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context_id
					);
				const struct prototype_context* argument_context = assumption_context ?
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context->parent
					) : NULL;
				uint32_t argument_classifier =
					prototype_context_classifier_term(argument_context);
				if (!argument_context ||
					argument_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						argument_classifier,
						operation_domain
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				classifier = relation->classifier;
				break;
			}
			break;
		}
			if (classifier != PROTOTYPE_INVALID_ID &&
				(operation->referenced_ast_binder_id ==
					occurrence_clause->continuation_ast_binder_id ?
					operation_solver_widen_continuation_binding(
						ctx, operation_id, classifier, p_changed
					) : operation_solver_bind_proven_classifier(
						ctx, operation_id, classifier, p_changed
					)) != 0) {
				return -1;
			}
			if (classifier != PROTOTYPE_INVALID_ID) {
				for (uint32_t lambda_operation_id = 0;
					lambda_operation_id < ctx->metadata->operation_count;
					++lambda_operation_id) {
					struct prototype_operation_node* lambda_operation =
						&ctx->metadata->operations[lambda_operation_id];
					uint32_t ast_binder_id;
					if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
						lambda_operation->core_term != lambda_term ||
						operation_solver_lambda_ast_binder(
							ctx, lambda_operation_id, &ast_binder_id
						) != 0 ||
						ast_binder_id != operation->referenced_ast_binder_id) {
						continue;
					}
					if (lambda_operation->binder_classifier ==
							PROTOTYPE_INVALID_ID ||
						(operation->referenced_ast_binder_id ==
							occurrence_clause->argument_ast_binder_id &&
						 !(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						 ).status == PROTOTYPE_TERM_CONVERSION_EQUAL))) {
						lambda_operation->binder_classifier = classifier;
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (operation->referenced_ast_binder_id ==
							occurrence_clause->continuation_ast_binder_id &&
						operation_solver_continuation_effect_widening(
							ctx,
							lambda_operation->binder_classifier,
							classifier
						)) {
						lambda_operation->binder_classifier = classifier;
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (
						!(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int operation_solver_solve(struct compile_context* ctx, int require_complete) {
	if (!ctx || operation_solver_generate_constraints(ctx) != 0) {
		return -1;
	}
	int changed = 0;
	if (operation_solver_seed_known_classifiers(ctx, &changed) != 0) {
		return -1;
	}
	for (;;) {
		uint32_t i;
		int pop_status;
		while ((pop_status = operation_solver_pop_constraint(ctx, &i)) == 0) {
			int pass_changed = 0;
			struct operation_classifier_goal* constraint =
				&ctx->classifier_solver.constraints[i];
			uint32_t target = constraint->classifier_variable;
			if (ctx->metadata->solver_steps_used >= ctx->metadata->solver_step_limit) {
				ctx->metadata->solver_exhausted = 1;
				operation_solver_refresh_constraint_states(ctx, 1);
				return 1;
			}
			ctx->metadata->solver_steps_used++;
			uint32_t classifier;
			switch (constraint->kind) {
				case OPERATION_CONSTRAINT_EQUAL:
					if (constraint->payload.reference.referenced_operation >=
							ctx->metadata->operation_count ||
						(classifier = operation_solver_classifier(
							ctx, constraint->payload.reference.referenced_operation
						)) ==
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if (operation_solver_bind(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
					case OPERATION_CONSTRAINT_CONVERTIBLE:
						uint32_t expected_classifier =
							constraint->payload.conversion.expected_classifier;
						if (expected_classifier == PROTOTYPE_INVALID_ID) {
							expected_classifier =
								ctx->metadata->operations[target].known_classifier;
						}
						if (constraint->payload.conversion.body_operation >=
								ctx->metadata->operation_count ||
							expected_classifier >=
								ctx->terms->term_count) {
							break;
						}
						classifier = operation_solver_classifier(
							ctx, constraint->payload.conversion.body_operation
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						/*
						 * An omitted surface effect row is an elaboration variable.
						 * Resolve it while the OperationGraph fixed point is still
						 * propagating, so downstream APP/COMPUTATION_FOLD proofs never capture the
						 * unsolved annotation. Imported transparent definitions are
						 * checked again by the dedicated ascription phase.
						 */
						uint32_t solved_expected;
						int solve_status =
							prototype_judgement_solve_expected_effect_rows(
								ctx->terms,
								ctx->type_declarations,
								NULL,
								expected_classifier,
								classifier,
								&solved_expected
							);
						if (solve_status < 0) {
							return -1;
						}
						if (solve_status != 0) {
							/* The body classifier is already a valid synthesis frontier.
							 * Preserve it for downstream operations while the dedicated
							 * ascription phase retains responsibility for proving conversion
							 * to the surface expectation. */
							if (operation_solver_bind(
									ctx,
									target,
									classifier,
									&pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						constraint->payload.conversion.kernel_goal.id = constraint->id;
						constraint->payload.conversion.kernel_goal.context_id =
							constraint->context_id;
						constraint->payload.conversion.kernel_goal.carrier_classifier =
							PROTOTYPE_INVALID_ID;
						constraint->payload.conversion.kernel_goal.left_term =
							solved_expected;
						constraint->payload.conversion.kernel_goal.right_term = classifier;
						constraint->payload.conversion.kernel_goal.normalization_profile =
							PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
						constraint->payload.conversion.kernel_goal.step_limit =
							ctx->metadata->normalization_step_limit;
						if (prototype_judgement_kernel_conversion_goal_execute(
								&ctx->metadata->contexts,
								ctx->terms,
								ctx->type_declarations,
								NULL,
								&constraint->payload.conversion.kernel_goal,
								0
							) != 0) {
							return -1;
						}
						if (constraint->payload.conversion.kernel_goal.result.status !=
							PROTOTYPE_TERM_CONVERSION_EQUAL) {
							if (constraint->payload.conversion.kernel_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_INVALID) {
								return -1;
							}
							if (constraint->payload.conversion.kernel_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
								constraint->state =
									OPERATION_CONSTRAINT_STATE_CONTRADICTION;
								constraint->reason =
									OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED;
							}
							if (operation_solver_bind(
									ctx, target, classifier, &pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						ctx->metadata->operations[target].known_classifier =
							solved_expected;
						if (operation_solver_bind(
								ctx,
								target,
								solved_expected,
								&pass_changed
							) != 0) {
							return -1;
						}
						break;
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (constraint->payload.pi.body_or_function_operation >=
							ctx->metadata->operation_count ||
						(constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
							constraint->payload.pi.domain_classifier_or_argument_operation >=
								ctx->metadata->operation_count)) {
						break;
					}
					if (constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
						uint32_t expected =
							ctx->classifier_solver.bindings[target];
						if (expected != PROTOTYPE_INVALID_ID) {
							uint32_t domain;
							uint32_t codomain_family;
							uint32_t codomain_binder;
							uint32_t codomain_body;
							uint32_t binder_var;
							uint32_t body_classifier;
							const struct prototype_operation_node* lambda =
								&ctx->metadata->operations[target];
							if (lambda->core_term >= ctx->terms->term_count ||
								ctx->terms->terms[lambda->core_term].tag !=
									PROTOTYPE_TERM_LAMBDA ||
								prototype_judgement_pi_parts(
									ctx->terms, expected, &domain, &codomain_family
								) != 0 ||
								prototype_term_pure_family_parts(
									ctx->terms,
									codomain_family,
									&codomain_binder,
									&codomain_body
								) != 0 ||
								!(prototype_judgement_classifier_conversion(
									ctx->terms, ctx->type_declarations,
									domain,
									constraint->payload.pi.domain_classifier_or_argument_operation
								).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
								prototype_term_var(
									ctx->terms,
									ctx->terms->terms[lambda->core_term].as.lambda.binding_id,
									&binder_var
								) != 0 ||
								prototype_term_graph_substitute_bound_var(
									ctx->terms,
									ctx->type_declarations,
									codomain_body,
									codomain_binder,
									binder_var,
									&body_classifier
								) != 0 ||
								operation_solver_bind(
									ctx,
									constraint->payload.pi.body_or_function_operation,
									body_classifier,
									&pass_changed
								) != 0) {
								/* The expected Pi may be more reduced than the current
								 * operation frontier. Leave this reverse propagation for a
								 * later pass; the forward Pi constraint remains authoritative. */
								break;
							}
						}
						classifier = operation_solver_classifier(
							ctx, constraint->payload.pi.body_or_function_operation
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t codomain_family;
						const struct prototype_operation_node* lambda =
							&ctx->metadata->operations[target];
						uint32_t effective_domain =
							constraint->payload.pi.domain_classifier_or_argument_operation !=
								PROTOTYPE_INVALID_ID ?
							constraint->payload.pi.domain_classifier_or_argument_operation :
							lambda->binder_classifier;
						if (effective_domain == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (lambda->core_term >= ctx->terms->term_count ||
							ctx->terms->terms[lambda->core_term].tag !=
								PROTOTYPE_TERM_LAMBDA ||
							prototype_term_pure_family(
								ctx->terms,
								ctx->terms->terms[lambda->core_term].as.lambda.binding_id,
								classifier,
								&codomain_family
							) != 0 ||
							prototype_term_pi_family(
								ctx->terms,
								effective_domain,
								codomain_family,
								&classifier
							) != 0 || operation_solver_generalize_lambda_effect_rows(
								ctx, target, classifier, &classifier
							) != 0 || operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					} else {
						uint32_t function_classifier =
							operation_solver_classifier(
								ctx, constraint->payload.pi.body_or_function_operation
							);
						uint32_t argument_classifier =
							operation_solver_classifier(
								ctx,
								constraint->payload.pi.domain_classifier_or_argument_operation
							);
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (operation_solver_specialize_integer_literal(
								ctx,
								function_classifier,
								constraint->payload.pi.domain_classifier_or_argument_operation,
								&argument_classifier,
								&pass_changed
							) != 0) {
							return -1;
						}
						int apply_status = operation_apply_classifier(
							ctx,
							function_classifier,
							argument_classifier,
							ctx->metadata->operations[
								constraint->payload.pi.domain_classifier_or_argument_operation
							].core_term,
							&classifier
						);
						/* A non-Pi or incompatible current candidate may belong to a
						 * shared core node. Leave this operation constraint unresolved
						 * until a source-operation binding selects its classifier. */
						if (apply_status == 0 && operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION: {
					int constructor_status =
						operation_solver_constructor_spine_classifier(
							ctx, target, &classifier
						);
					if (constructor_status < 0) {
						return -1;
					}
					if (constructor_status == 0 && operation_solver_bind(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_HAS_TYPE:
					if (ctx->classifier_solver.bindings[target] !=
						PROTOTYPE_INVALID_ID) {
						break;
					}
					/* Core facts may seed atoms only, never a typed operation edge. */
					if (ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_APP ||
						ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_LAMBDA ||
						ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_MATCH) {
						break;
					}
					{
						uint32_t classifiers[32];
						uint32_t classifier_count = 0;
						if (operation_solver_collect_input_classifiers(
								ctx,
								target,
								classifiers,
								32,
								&classifier_count
							) != 0) {
							return -1;
						}
						if (classifier_count == 1 && operation_solver_bind(
								ctx, target, classifiers[0], &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
					if (operation_solver_solve_match(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_IH_EXPECTED:
					if (operation_solver_materialize_induction_hypothesis(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
					if (constraint->payload.cbpv_boundary.child_operation >=
						ctx->metadata->operation_count) {
						return -1;
					}
					uint32_t child_classifier = operation_solver_classifier(
						ctx, constraint->payload.cbpv_boundary.child_operation
					);
					if (child_classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					if (prototype_judgement_cbpv_boundary_classifier(
							ctx->terms,
							ctx->type_declarations,
							ctx->metadata->operations[target].core_term,
							child_classifier,
							&classifier
						) != 0 || operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT: {
					const struct prototype_operation_node* fold =
						&ctx->metadata->operations[target];
					if (fold->core_term >= ctx->terms->term_count ||
						ctx->terms->terms[fold->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
						return -1;
					}
					int status = ctx->terms->terms[fold->core_term].as.computation_fold.clause_count == 0 ?
						operation_solver_propagate_zero_clause_computation_fold_input(
							ctx, target, &pass_changed
						) : operation_solver_propagate_clause_computation_fold_input(
							ctx, target, &pass_changed
						);
					if (status != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
					/* These occurrence constraints are solved by the CBPV computation
					 * constraint pass. Their state is published only after that pass has
					 * either produced closed evidence or a VerificationDB obligation. */
					break;
				default:
					return -1;
			}
			if (pass_changed) {
				changed = 1;
			}
		}
		if (pop_status < 0) {
			return -1;
		}
		int materialized = 0;
		if (operation_solver_materialize_solved_motives(ctx, &materialized) != 0) {
			return -1;
		}
		if (!materialized) {
			if (require_complete && operation_solver_require_complete(ctx) != 0) {
				operation_solver_refresh_constraint_states(ctx, 1);
				return -1;
			}
			int commit_status = operation_solver_commit_bindings(ctx, &changed);
			operation_solver_refresh_constraint_states(ctx, commit_status != 0);
			return commit_status;
		}
		changed = 1;
	}
}

static int compile_phase_infer_general_classifiers(
	struct compile_context* ctx,
	int require_complete
) {
	if (!ctx) {
		return -1;
	}
	return operation_solver_solve(ctx, require_complete);
}

static size_t count_classified_operations(const struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	size_t count = 0;
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		if (ctx->metadata->operations[i].classifier != PROTOTYPE_INVALID_ID) {
			count++;
		}
	}
	return count;
}

static int build_operation_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[match->as.match.first_case + case_index];
		if (!equation || equation->payload.motive_case.branch_operation >= ctx->metadata->operation_count ||
			source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256) {
			return 1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[equation->payload.motive_case.branch_operation];
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64 ||
			prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->operation_cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0 ||
			reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			motive_binders[binder_cursor + i].binding_id =
				reindexed_binders[i];
			motive_binders[binder_cursor + i].is_recursive =
				ctx->terms->case_binders[
					source_case->first_binder + i
				].is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[match->as.match.first_case + case_index];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				branch_classifier,
				case_substitution,
				&motive_cases[case_index].body
			) != 0) {
			return -1;
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t binder_var;
	uint32_t motive_match;
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_match(ctx->terms, binder_var, motive_cases, operation->case_count, &motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, binding_id, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

/*
 * Type dependency belongs to the source-operation graph, not to a raw core
 * binder id. A tagless core VAR can be shared by unrelated source binders.
 * This helper is deliberately conservative: a source use means a branch is
 * not treated as uniform, even if a later solver proves its classifier is
 * constant.
 */
static int operation_subtree_ast_binder_use_count(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t ast_binder_id,
	uint8_t* active,
	uint32_t* p_count
) {
	if (!ctx || !ctx->metadata || !active || !p_count ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	if (active[operation_id]) {
		return -1;
	}
	active[operation_id] = 1;
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_VAR &&
		operation->referenced_ast_binder_id == ast_binder_id && *p_count < 2) {
		(*p_count)++;
	}
	uint32_t children[66];
	uint32_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			if (operation->fold_clause_count == 0) {
				children[child_count++] = operation->argument;
				break;
			}
			children[child_count++] = operation->scrutinee;
			if (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				operation->fold_clause_count >
					ctx->metadata->operation_fold_clause_count -
						operation->first_fold_clause ||
				child_count + 2 * operation->fold_clause_count > 66) {
				active[operation_id] = 0;
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				children[child_count++] = clause->operation_operation;
				children[child_count++] = clause->body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			if (operation->first_case > ctx->metadata->operation_case_count ||
				operation->case_count > ctx->metadata->operation_case_count -
					operation->first_case || child_count + operation->case_count > 66) {
				active[operation_id] = 0;
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				children[child_count++] = ctx->metadata->operation_cases[
					operation->first_case + i
				].body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			active[operation_id] = 0;
			return -1;
	}
	for (uint32_t i = 0; i < child_count && *p_count < 2; ++i) {
		if (children[i] != PROTOTYPE_INVALID_ID &&
			operation_subtree_ast_binder_use_count(
				ctx, children[i], ast_binder_id, active, p_count
			) != 0) {
			active[operation_id] = 0;
			return -1;
		}
	}
	active[operation_id] = 0;
	return 0;
}

static int operation_subtree_references_ast_binder(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t ast_binder_id,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	if (visited[operation_id]) {
		return 0;
	}
	visited[operation_id] = 1;
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_VAR) {
		return operation->referenced_ast_binder_id == ast_binder_id;
	}
	uint32_t children[66];
	uint32_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			if (operation->fold_clause_count == 0) {
				children[child_count++] = operation->argument;
				break;
			}
			children[child_count++] = operation->scrutinee;
			if (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				operation->fold_clause_count >
					ctx->metadata->operation_fold_clause_count -
						operation->first_fold_clause ||
				child_count + 2 * operation->fold_clause_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				children[child_count++] = clause->operation_operation;
				children[child_count++] = clause->body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			if (operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count || child_count + operation->case_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				children[child_count++] = ctx->metadata->operation_cases[
					operation->first_case + i
				].body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int status = operation_subtree_references_ast_binder(
			ctx, children[i], ast_binder_id, visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		root_operation >= ctx->metadata->operation_count ||
		target_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[root_operation];
	uint32_t children[66];
	uint32_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			if (operation->fold_clause_count == 0) {
				children[child_count++] = operation->argument;
				break;
			}
			children[child_count++] = operation->scrutinee;
			if (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				operation->fold_clause_count >
					ctx->metadata->operation_fold_clause_count -
						operation->first_fold_clause ||
				child_count + 2 * operation->fold_clause_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				children[child_count++] = clause->operation_operation;
				children[child_count++] = clause->body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			if (operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count || child_count + operation->case_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				children[child_count++] = ctx->metadata->operation_cases[
					operation->first_case + i
				].body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int status = operation_subtree_contains_operation(
			ctx, children[i], target_operation, visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
) {
	if (!ctx || !ctx->metadata || !ctx->terms ||
		root_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t target = 0;
		target < ctx->metadata->operation_count;
		++target) {
		uint32_t core_term = ctx->metadata->operations[target].core_term;
		if (core_term >= ctx->terms->term_count ||
			ctx->terms->terms[core_term].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		uint8_t visited[ctx->metadata->operation_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, root_operation, target, visited
		);
		if (contains != 0) {
			return contains;
		}
	}
	return 0;
}

static int operation_branch_uses_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_goal* equation
) {
	if (!ctx || !equation ||
		equation->classifier_variable >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[equation->classifier_variable];
	if (match->first_case + equation->payload.motive_case.case_index >=
		ctx->metadata->operation_case_count) {
		return -1;
	}
	const struct prototype_operation_match_case* operation_case =
		&ctx->metadata->operation_cases[
			match->first_case + equation->payload.motive_case.case_index
		];
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		uint8_t visited[4096] = { 0 };
		int status = operation_subtree_references_ast_binder(
			ctx, equation->payload.motive_case.branch_operation,
			operation_case->ast_binder_ids[i],
			visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_goal* equation,
	uint32_t classifier
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		equation->classifier_variable >= ctx->metadata->operation_count) {
		return -1;
	}
	int source_uses_binder = operation_branch_uses_case_binder(ctx, equation);
	if (source_uses_binder <= 0) {
		return source_uses_binder;
	}
	uint32_t classifier_whnf;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[equation->classifier_variable];
	uint32_t context_path[64];
	uint32_t context_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			match->context_id,
			ctx->metadata->operation_cases[
				match->first_case + equation->payload.motive_case.case_index
			].context_id,
			context_path,
			64,
			&context_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < context_count; ++i) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_path[i]
		);
		if (!context) {
			return -1;
		}
		uint32_t binding_id = context->binding_id;
		if (prototype_term_contains_free_binding(
			ctx->terms, classifier_whnf, binding_id
		) != 0) {
			return 1;
		}
	}
	return 0;
}

/*
 * A uniform motive is represented as a constant lambda. This is not a Match
 * typing shortcut: APP(\_ => T, scrutinee) reduces by beta to T. It is valid
 * only when no branch classifier mentions that branch's pattern binders.
 */
static int build_operation_uniform_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		if (!equation || equation->payload.motive_case.branch_operation >=
			ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[
				equation->payload.motive_case.branch_operation
			];
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		int capture_status = operation_classifier_captures_case_binder(
			ctx, equation, branch_classifier
		);
		if (capture_status < 0) {
			return -1;
		}
		if (capture_status > 0) {
			return 1;
		}
		if (classifier == PROTOTYPE_INVALID_ID) {
			classifier = branch_classifier;
		} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, classifier, branch_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_lambda(ctx->terms, binding_id, classifier, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

static const struct pending_match_typing* lookup_pending_match_typing(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx) {
		return NULL;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		if (ctx->pending_match_typings[i].operation == operation) {
			return &ctx->pending_match_typings[i];
		}
	}
	return NULL;
}

/*
 * Solve the guarded equation M(C(..., rest, ...)) = M(rest) without first
 * inventing a TermDB classifier for M(rest). The resulting motive carries its
 * own structurally-recursive match frame, so the recursive equation is a
 * finite graph rather than a cyclic solver substitution.
 */
static int build_operation_guarded_recursive_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	uint32_t motive_frame = prototype_term_new_ih_scope(ctx->terms);
	if (motive_frame == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		uint32_t source_case_id = match->as.match.first_case + case_index;
		if (!equation || source_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[source_case_id];
		if (source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64 ||
			prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->operation_cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0 ||
			reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t binder_index = 0;
			binder_index < source_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* source_binder =
				&ctx->terms->case_binders[source_case->first_binder + binder_index];
			motive_binders[binder_cursor + binder_index].binding_id =
				reindexed_binders[binder_index];
			motive_binders[binder_cursor + binder_index].is_recursive =
				source_binder->is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[source_case_id];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[
				equation->payload.motive_case.branch_operation
			];
		if (branch_classifier != PROTOTYPE_INVALID_ID) {
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					branch_classifier,
					case_substitution,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		} else {
			const struct prototype_operation_node* branch =
				&ctx->metadata->operations[
					equation->payload.motive_case.branch_operation
				];
			if (branch->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
				branch->argument >= ctx->metadata->operation_count ||
				branch->scrutinee != typing->operation) {
				return 1;
			}
			uint32_t original_argument =
				ctx->metadata->operations[branch->argument].core_term;
			if (original_argument >= ctx->terms->term_count ||
				ctx->terms->terms[original_argument].tag != PROTOTYPE_TERM_VAR) {
				return 1;
			}
			uint32_t recursive_index = PROTOTYPE_INVALID_ID;
			for (uint32_t binder_index = 0;
				binder_index < source_case->binder_count;
				++binder_index) {
				const struct prototype_case_binder* source_binder =
					&ctx->terms->case_binders[source_case->first_binder + binder_index];
				if (source_binder->is_recursive &&
					source_binder->binding_id ==
						ctx->terms->terms[original_argument].as.var.binding_id) {
					recursive_index = binder_index;
					break;
				}
			}
			if (recursive_index == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			uint32_t recursive_var;
			if (prototype_term_var(
					ctx->terms,
					motive_binders[binder_cursor + recursive_index].binding_id,
					&recursive_var
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, motive_frame, recursive_var,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
	uint32_t motive_var;
	uint32_t motive_match;
	uint32_t motive;
	if (motive_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, motive_binder, &motive_var) != 0 ||
		prototype_term_match_with_ih_scope(
			ctx->terms, motive_var, motive_cases, operation->case_count,
			motive_frame, &motive_match
		) != 0 ||
		prototype_term_set_ih_scope_term(ctx->terms, motive_frame, motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, motive_binder, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

/*
 * This is the solver occurs check for the only recursive metavariable form
 * currently admitted by the language: an IH denotes M(rest). The occurrence
 * must point back to its own Match frame and `rest` must be a recursive field
 * of that Match. A raw M(x) self-reference outside that structural edge is
 * rejected instead of becoming a cyclic solver substitution.
 */
static int operation_solver_validate_guarded_motive_occurrence(
	const struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_operation
) {
	if (!ctx || !ctx->metadata ||
		ih_operation >= ctx->metadata->operation_count ||
		motive_operation >= ctx->metadata->operation_count ||
		argument_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* ih =
		&ctx->metadata->operations[ih_operation];
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[motive_operation];
	const struct prototype_operation_node* argument =
		&ctx->metadata->operations[argument_operation];
	if (ih->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		match->tag != PROTOTYPE_OPERATION_MATCH ||
		ih->first_case >= ctx->terms->ih_scope_count ||
		argument->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[argument->core_term].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	if (ctx->terms->ih_scopes[ih->first_case].match_term != match->core_term ||
		match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t binding_id = ctx->terms->terms[argument->core_term].as.var.binding_id;
	const struct prototype_term* match_term = &ctx->terms->terms[match->core_term];
	for (uint32_t case_index = 0; case_index < match_term->as.match.case_count;
		++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match_term->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* binder =
				&ctx->terms->case_binders[match_case->first_binder + binder_index];
			if (binder->binding_id == binding_id) {
				return binder->is_recursive ? 0 : -1;
			}
		}
	}
	return -1;
}

static int operation_solver_nonrecursive_seed_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	const struct prototype_operation_node* operation,
	uint32_t* p_classifier,
	uint32_t* p_source_body_operation
) {
	if (!ctx || !operation || !p_classifier || !p_source_body_operation ||
		operation_id >= ctx->metadata->operation_count || operation->case_count == 0 ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return -1;
	}
	uint32_t candidate = PROTOTYPE_INVALID_ID;
	uint32_t source_body_operation = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation || equation->payload.motive_case.branch_operation >=
			ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[
				equation->payload.motive_case.branch_operation
			];
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		int has_recursive_binder = 0;
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				has_recursive_binder = 1;
				break;
			}
		}
		if (has_recursive_binder) {
			continue;
		}
		if (candidate == PROTOTYPE_INVALID_ID) {
			candidate = branch_classifier;
			source_body_operation = equation->payload.motive_case.branch_operation;
		} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, candidate, branch_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
	}
	if (candidate == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = candidate;
	*p_source_body_operation = source_body_operation;
	return 0;
}

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
) {
	if (!ctx || !operation || operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[operation->core_term];
	for (uint32_t case_index = 0; case_index < match->as.match.case_count; ++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_solver_has_guarded_recursive_equation(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	int has_unresolved_ih = 0;
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[operation_id];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		if (ctx->classifier_solver.bindings[
				equation->payload.motive_case.branch_operation
			] !=
			PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_operation_node* body =
			&ctx->metadata->operations[
				equation->payload.motive_case.branch_operation
			];
		if (body->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			body->scrutinee != operation_id) {
			return 0;
		}
		has_unresolved_ih = 1;
	}
	return has_unresolved_ih;
}

/*
 * Match solving records only an equation solution state. It deliberately does
 * not build a motive or an IH classifier in TermDB: an unresolved ?M is a
 * solver object, not a graph node. Materialization happens after propagation
 * reaches a fixed point.
 */
static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	if (operation->tag != PROTOTYPE_OPERATION_MATCH || !typing ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return -1;
	}
	if (ctx->classifier_solver.bindings[operation_id] != PROTOTYPE_INVALID_ID ||
		ctx->classifier_solver.motive_solution_states[operation_id] ==
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int has_unresolved_body = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_goal* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		uint32_t body_operation = equation->payload.motive_case.branch_operation;
		if (body_operation >= ctx->metadata->operation_count ||
			ctx->classifier_solver.bindings[body_operation] == PROTOTYPE_INVALID_ID) {
			has_unresolved_body = 1;
		}
	}
	if (!has_unresolved_body) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_READY;
		return 0;
	}
	int has_recursive_binder = operation_solver_match_has_recursive_binder(ctx, operation);
	if (has_recursive_binder < 0) {
		return -1;
	}
	if (!has_recursive_binder) {
		return 0;
	}
	/*
	 * A non-recursive branch can constrain a constant motive before the
	 * recursive branch is classified.  This is the normal constraint-solving
	 * path for CBPV lowering: the recursive occurrence may be below RETURN,
	 * THUNK, or COMPUTATION_FOLD, so it must receive M(rest) through the solver before its
	 * enclosing computation can be classified.  Do this before considering the
	 * old direct-IH fallback, which only understands an IH as the immediate
	 * branch operation.
	 */
	uint32_t seed_classifier;
	uint32_t seed_source_body_operation;
	int seed_status = operation_solver_nonrecursive_seed_classifier(
		ctx, operation_id, operation, &seed_classifier, &seed_source_body_operation
	);
	if (seed_status < 0) {
		return -1;
	}
	if (seed_status == 0) {
		seed_status = operation_solver_seed_motive(
			ctx, operation_id, seed_classifier, seed_source_body_operation, p_changed
		);
		return seed_status < 0 ? -1 : 0;
	}
	int guarded_status = operation_solver_has_guarded_recursive_equation(ctx, operation_id);
	if (guarded_status < 0) {
		return -1;
	}
	if (guarded_status > 0) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE;
		return 0;
	}
	return 0;
}

static int operation_solver_materialize_match_solution(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	uint8_t state = ctx->classifier_solver.motive_solution_states[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH || !typing ||
		state == OPERATION_MOTIVE_SOLUTION_UNRESOLVED ||
		state == OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	uint32_t classifier;
	int status;
	if (state == OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE) {
		status = build_operation_guarded_recursive_motive(ctx, typing, &classifier);
	} else {
		status = build_operation_uniform_motive(ctx, typing, &classifier);
		if (status > 0) {
			status = build_operation_motive(ctx, typing, &classifier);
		}
	}
	if (status != 0) {
		return status < 0 ? -1 : 0;
	}
	uint32_t seed_classifier =
		ctx->classifier_solver.motive_constant_candidates[operation_id];
	if (seed_classifier != PROTOTYPE_INVALID_ID &&
		!(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, classifier, seed_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	ctx->classifier_solver.motive_terms[operation_id] =
		ctx->terms->terms[classifier].as.app.function;
	ctx->classifier_solver.motive_solution_states[operation_id] =
		OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
	return operation_solver_bind(ctx, operation_id, classifier, p_changed);
}

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		if (ctx->metadata->operations[i].tag == PROTOTYPE_OPERATION_MATCH &&
			operation_solver_materialize_match_solution(ctx, i, p_changed) != 0) {
			return -1;
		}
	}
	return 0;
}

/* Keep IH propagation entirely in the solver until its parent motive has
 * been materialized from a solved equation. */
static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	if (ctx->classifier_solver.bindings[operation_id] != PROTOTYPE_INVALID_ID) {
		return 0;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		operation->argument >= ctx->metadata->operation_count ||
		operation->scrutinee >= ctx->metadata->operation_count ||
		ctx->metadata->operations[operation->scrutinee].tag !=
			PROTOTYPE_OPERATION_MATCH) {
		return -1;
	}
	uint32_t parent_match = operation->scrutinee;
	if (operation_solver_solve_match(ctx, parent_match, p_changed) != 0) {
		return -1;
	}
	uint32_t motive = ctx->classifier_solver.motive_terms[parent_match];
	if (motive == PROTOTYPE_INVALID_ID) {
		uint32_t parent_classifier = operation_solver_classifier(ctx, parent_match);
		const struct prototype_operation_node* match_operation =
			&ctx->metadata->operations[parent_match];
		if (parent_classifier != PROTOTYPE_INVALID_ID &&
			parent_classifier < ctx->terms->term_count &&
			ctx->terms->terms[parent_classifier].tag == PROTOTYPE_TERM_APP &&
			match_operation->scrutinee < ctx->metadata->operation_count &&
			ctx->terms->terms[parent_classifier].as.app.argument ==
				ctx->metadata->operations[match_operation->scrutinee].core_term) {
			motive = ctx->terms->terms[parent_classifier].as.app.function;
			ctx->classifier_solver.motive_terms[parent_match] = motive;
			ctx->classifier_solver.motive_solution_states[parent_match] =
				OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		}
	}
	if (motive == PROTOTYPE_INVALID_ID) {
		if (operation_solver_validate_guarded_motive_occurrence(
				ctx, operation_id, parent_match, operation->argument
			) != 0) {
			return -1;
		}
		if (operation_solver_set_ih_motive_application(
				ctx, operation_id, parent_match, operation->argument, p_changed
			) != 0) {
			return -1;
		}
		/* A constant motive equation M(_) == T is already a concrete solver
		 * solution for this IH occurrence.  Bind it now so CBPV wrappers can
		 * propagate through RETURN/THUNK/COMPUTATION_FOLD; the lambda for M is still
		 * materialized only after every Match equation has been checked. */
		uint32_t candidate =
			ctx->classifier_solver.motive_constant_candidates[parent_match];
		if (candidate != PROTOTYPE_INVALID_ID) {
			return operation_solver_bind(ctx, operation_id, candidate, p_changed);
		}
		return 0;
	}
	uint32_t classifier;
	if (prototype_term_app(
			ctx->terms, motive,
			ctx->metadata->operations[operation->argument].core_term,
			&classifier
		) != 0) {
		return -1;
	}
	return operation_solver_bind(ctx, operation_id, classifier, p_changed);
}

static int operation_solver_materialize_induction_hypothesis_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	uint32_t classifier = operation->classifier;
	if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		classifier == PROTOTYPE_INVALID_ID ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		return -1;
	}
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
			if (relations[i].kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relations[i].operation_id == operation_id &&
				relations[i].subject == operation->core_term &&
				prototype_judgement_candidate_find_derivation_kind(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
					&(uint32_t){0}
				) == 0 &&
				(prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					relations[i].classifier, classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return 0;
			}
		}
	}
	const struct prototype_term* ih = &ctx->terms->terms[operation->core_term];
	uint32_t ih_scope_id = ih->as.induction_hypothesis.ih_scope_id;
	if (ih_scope_id >= ctx->terms->ih_scope_count ||
		ih->as.induction_hypothesis.argument >= ctx->terms->term_count ||
		ctx->terms->terms[ih->as.induction_hypothesis.argument].tag !=
			PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t match_term = ctx->terms->ih_scopes[ih_scope_id].match_term;
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		operation->scrutinee >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t match_classifier = ctx->metadata->operations[
		operation->scrutinee
	].classifier;
	if (match_classifier >= ctx->terms->term_count ||
		ctx->terms->terms[match_classifier].tag != PROTOTYPE_TERM_APP) {
		return -1;
	}
	uint32_t motive = ctx->terms->terms[match_classifier].as.app.function;
	const struct prototype_term* match = &ctx->terms->terms[match_term];
	uint32_t binding_id =
		ctx->terms->terms[ih->as.induction_hypothesis.argument].as.var.binding_id;
	for (uint32_t case_index = 0; case_index < match->as.match.case_count;
		++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* binder = &ctx->terms->case_binders[
				match_case->first_binder + binder_index
			];
			if (binder->binding_id != binding_id) {
				continue;
			}
			if (!binder->is_recursive) {
				return -1;
			}
			return prototype_judgement_delta_expand_induction_hypothesis(
				&ctx->judgement_delta, ctx->terms, operation->core_term, classifier,
				match_term, motive, case_index, binder_index
			);
		}
	}
	return -1;
}

/*
 * The solver owns classifier synthesis. Once all operation variables have a
 * concrete solution, this phase only reconstructs JudgementDB derivations for
 * those already-fixed conclusions. It is deliberately not a second inference
 * pass: every generated conclusion must be normalization-equal to the solver
 * binding for the same operation.
 */
static int operation_match_cases_are_resolved(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
) {
	if (!ctx || !ctx->metadata || !operation ||
		operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return 0;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct prototype_operation_match_case* operation_case =
			&ctx->metadata->operation_cases[operation->first_case + case_index];
		if (operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
	}
	return 1;
}

static int operation_solver_materialize_match_pattern_assumptions(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
			operation->source_ast >= ctx->asts->node_count ||
			ctx->asts->nodes[operation->source_ast].tag != PROTOTYPE_AST_MATCH) {
			continue;
		}
		/* Constructor selection depends on the scrutinee classifier. The solver
		 * may have learned that classifier only in this fixed-point round, so
		 * defer proof materialization until the following round resolves cases. */
		if (!operation_match_cases_are_resolved(ctx, operation)) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_ast_node* source_match =
			&ctx->asts->nodes[operation->source_ast];
		if (match->as.match.case_count != operation->case_count ||
			source_match->as.match.case_count != operation->case_count ||
			operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count) {
			return -1;
		}
		for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[operation->first_case + case_index];
			uint32_t term_case_id = match->as.match.first_case + case_index;
			uint32_t ast_case_id = source_match->as.match.first_case + case_index;
			if (term_case_id >= ctx->terms->case_count || ast_case_id >= ctx->asts->case_count ||
				operation_case->body_operation >= ctx->metadata->operation_count ||
				operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation_case->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, PROTOTYPE_INVALID_ID
			);
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[term_case_id];
			const struct prototype_ast_match_case* ast_case =
				&ctx->asts->cases[ast_case_id];
			if (match_case->binder_count != ast_case->binder_count ||
				match_case->first_binder + match_case->binder_count >
					ctx->terms->case_binder_count ||
				ast_case->first_binder + ast_case->binder_count >
					ctx->asts->case_binder_count) {
				return -1;
			}
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count;
				++binder_index) {
				uint32_t classifier;
				uint32_t binder_var;
				if (constructor_telescope_field_classifier(
						ctx,
						operation_case->context_id,
						operation_case->constructor_owner,
						operation_case->constructor_id,
						&ctx->terms->case_binders[match_case->first_binder],
						binder_index,
						binder_index,
						&classifier
					) != 0 ||
					prototype_term_var(
						ctx->terms,
						ctx->terms->case_binders[
							match_case->first_binder + binder_index
						].binding_id,
						&binder_var
					) != 0) {
					return -1;
				}
				int already_materialized = 0;
				for (size_t relation_id = 0;
					relation_id < ctx->judgement_delta.proposition_count;
					++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&ctx->judgement_delta.propositions[relation_id];
					if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						relation->context_id != operation_case->context_id ||
						relation->subject != binder_var ||
						relation->classifier != classifier) {
						continue;
					}
					uint32_t cursor = 0;
					uint32_t derivation_id;
					while (prototype_judgement_candidate_derivation_next(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						&cursor,
						&derivation_id
					) == 0) {
						const struct prototype_judgement_derivation_candidate* proof =
							&ctx->judgement_delta.derivation_candidates[derivation_id];
						if (proof->proof_kind ==
								PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION &&
							proof->rule_data.constructor.owner_view ==
							operation_case->constructor_owner &&
							proof->rule_data.constructor.constructor_index ==
							operation_case->constructor_id &&
							proof->rule_data.constructor.field_index == binder_index) {
							already_materialized = 1;
							break;
						}
					}
					if (already_materialized) {
						break;
					}
				}
				if (already_materialized) {
					continue;
				}
				if (prototype_judgement_delta_record_match_pattern(
						&ctx->judgement_delta,
						ctx->terms,
						binder_var,
						classifier,
						operation_case->constructor_owner,
						operation_case->constructor_id,
						binder_index
					) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

static uint32_t operation_solver_evidence_owner(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	for (uint32_t depth = 0;
		ctx && ctx->metadata && operation_id < ctx->metadata->operation_count &&
		depth < 64;
		++depth) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		int authority_neutral_atom = operation->tag == PROTOTYPE_OPERATION_ATOM &&
			operation->core_term < ctx->terms->term_count &&
			(ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EXTERNAL_REF ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_PURE_PRIMITIVE ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EFFECT_OPERATION ||
			 prototype_term_type_instance_info(
				ctx->terms,
				operation->core_term,
				&(uint32_t){0},
				NULL,
				&(uint32_t){0}
			) == 0);
		if (authority_neutral_atom ||
			operation->tag == PROTOTYPE_OPERATION_VAR ||
			operation->tag == PROTOTYPE_OPERATION_CONSTRUCTOR) {
			return PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			operation_id = operation->function;
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
			operation->body < ctx->metadata->operation_count &&
			operation->classifier ==
				ctx->metadata->operations[operation->body].classifier) {
			operation_id = operation->body;
			continue;
		}
		return operation_id;
	}
	return operation_id;
}

static int operation_solver_select_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	uint32_t source_context = context_id;
	for (;;) {
		struct prototype_judgement_selected_evidence source_evidence;
		int selection_status = prototype_judgement_delta_select_evidence(
			&ctx->judgement_delta,
			expected_operation_id,
			source_context,
			subject,
			classifier,
			&source_evidence
		);
		if (selection_status < 0 || selection_status == 2) {
			return -1;
		}
		if (selection_status == 0) {
			if (source_context == context_id) {
				if (p_evidence) {
					*p_evidence = source_evidence;
				}
				return 0;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, expected_operation_id
			);
			uint32_t weakening_substitution;
			if (prototype_substitution_projection_path(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					context_id,
					source_context,
					&weakening_substitution
				) != 0) {
				return -1;
			}
			int weaken_status = prototype_judgement_delta_record_context_weaken(
				&ctx->judgement_delta,
				&source_evidence,
				weakening_substitution
			);
			if (weaken_status != 0 || !p_evidence) {
				return weaken_status;
			}
			return prototype_judgement_delta_select_evidence(
				&ctx->judgement_delta,
				expected_operation_id,
				context_id,
				subject,
				classifier,
				p_evidence
			);
		}
		if (source_context == 0) {
			break;
		}
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, source_context
		);
		if (!context || context->parent >= source_context) {
			return -1;
		}
		source_context = context->parent;
	}
	return 1;
}

static int operation_solver_require_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	return operation_solver_select_evidence_in_context(
		ctx,
		expected_operation_id,
		context_id,
		subject,
		classifier,
		NULL
	);
}

static int operation_solver_lookup_operation_derivation_classifier(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t subject,
	int proof_kind,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (size_t i = ctx->judgement_delta.proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation =
			&ctx->judgement_delta.propositions[i - 1];
		if (relation->operation_id == operation_id &&
			relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			prototype_judgement_candidate_find_derivation_kind(
				ctx->judgement_delta.propositions,
				ctx->judgement_delta.proposition_count,
				ctx->judgement_delta.derivation_candidates,
				ctx->judgement_delta.derivation_candidate_count,
				(uint32_t)(i - 1),
				proof_kind,
				&(uint32_t){0}
			) == 0) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	if (ctx->judgement_delta.db) {
		for (size_t i = ctx->judgement_delta.db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation =
				&ctx->judgement_delta.db->propositions[i - 1];
			if (relation->operation_id == operation_id &&
				relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				prototype_judgement_candidate_find_derivation_kind(
					ctx->judgement_delta.db->propositions,
					ctx->judgement_delta.db->proposition_count,
					ctx->judgement_delta.db->derivation_candidates,
					ctx->judgement_delta.db->derivation_candidate_count,
					(uint32_t)(i - 1),
					proof_kind,
					&(uint32_t){0}
				) == 0) {
				*p_classifier = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

/*
 * A source operation is a typed occurrence.  Its TermDB root can be shared
 * with an alpha-equivalent occurrence whose children have different raw
 * binder ids.  JudgementDB is serialized using TermDB representatives, so
 * reify the occurrence derivation against the representative's structural
 * children instead of assuming operation->core_term retains the source
 * child ids.
 */
static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count ||
		core_term >= ctx->terms->term_count || depth > 256) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	/* OperationGraph owns the typed occurrence. The proof conclusion must use
	 * that occurrence's own Core projection, not a structurally equal child
	 * recovered from an interned parent Term. */
	core_term = operation->core_term;
	prototype_judgement_delta_set_operation(
		&ctx->judgement_delta, operation_id
	);
	uint32_t classifier = operation->classifier;
	const struct prototype_term* term = &ctx->terms->terms[core_term];
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	int free_effect_row = operation_classifier_contains_unowned_effect_row(
		ctx, operation_id, classifier
	);
	if (free_effect_row < 0) {
		return -1;
	}
	if (free_effect_row != 0) {
		/* Recursive evidence construction must obey the same closure boundary as
		 * the outer materialization pass. An ascription or parent APP must not
		 * turn a residual external effect-row constraint into a kernel fact. */
		return 1;
	}

	switch (operation->tag) {
		case PROTOTYPE_OPERATION_LAMBDA: {
			if (term->tag != PROTOTYPE_TERM_LAMBDA ||
				operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation->body];
			if (body->classifier == PROTOTYPE_INVALID_ID ||
				operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->body, body->core_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			const struct prototype_context* binder_context = prototype_context_get(
				&ctx->metadata->contexts, body->context_id
			);
			uint32_t binder_var;
			if (!binder_context || binder_context->parent != operation->context_id ||
				binder_context->binding_id == PROTOTYPE_INVALID_ID ||
				prototype_term_var(
					ctx->terms, binder_context->binding_id, &binder_var
				) != 0) {
				return -1;
			}
			int has_binder_assumption = 0;
			for (size_t i = 0;
				i < ctx->judgement_delta.proposition_count;
				++i) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[i];
				if (relation->context_id == body->context_id &&
					relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					relation->subject == binder_var &&
					relation->classifier == operation->binder_classifier &&
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)i,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) == 0) {
					has_binder_assumption = 1;
					break;
				}
			}
			if (!has_binder_assumption) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, body->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				size_t before = ctx->judgement_delta.proposition_count;
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						binder_context->binding_id,
						operation->binder_classifier
					) != 0 ||
					ctx->judgement_delta.proposition_count <= before) {
					return -1;
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			uint32_t expected_family;
			uint32_t expected_classifier;
			if (prototype_term_pure_family(
					ctx->terms,
					term->as.lambda.binding_id,
					body->classifier,
					&expected_family
				) != 0 || prototype_term_pi_family(
					ctx->terms,
					operation->binder_classifier,
					expected_family,
					&expected_classifier
				) != 0 || operation_solver_generalize_lambda_effect_rows(
					ctx, operation_id, expected_classifier, &expected_classifier
				) != 0) {
				return -1;
			}
			if (!(prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					classifier,
					expected_classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					return 1;
				}
				struct prototype_judgement_selected_evidence binder_evidence;
				int binder_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					body->context_id,
					binder_var,
					operation->binder_classifier,
					&binder_evidence
				);
				struct prototype_judgement_selected_evidence body_evidence;
				int body_status = binder_status == 0 ?
					operation_solver_select_evidence_in_context(
						ctx,
						operation_solver_evidence_owner(ctx, operation->body),
						body->context_id,
						body->core_term,
						body->classifier,
						&body_evidence
					) : binder_status;
				if (body_status != 0) {
					return body_status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, operation_id
				);
				if (prototype_judgement_delta_record_lambda_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation_id,
					core_term,
					classifier,
					binder_var,
					body->core_term,
						&binder_evidence,
						operation->body,
						&body_evidence
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_APP: {
			if (term->tag != PROTOTYPE_TERM_APP ||
				operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				uint32_t head;
				uint32_t owner;
				uint32_t constructor_index;
				uint32_t arguments[64];
				uint32_t argument_count;
				uint32_t reverse_argument_operations[64];
				uint32_t argument_operation_count = 0;
				uint32_t cursor = operation_id;
				if (prototype_term_constructor_spine_info(
						ctx->terms,
						core_term,
						&head,
						&owner,
						&constructor_index,
						arguments,
						64,
						&argument_count
					) != 0) {
					return -1;
				}
				while (cursor < ctx->metadata->operation_count) {
					const struct prototype_operation_node* cursor_operation =
						&ctx->metadata->operations[cursor];
					if (cursor_operation->tag != PROTOTYPE_OPERATION_APP ||
						cursor_operation->application_role !=
							PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
						break;
					}
					if (argument_operation_count >= 64 ||
						cursor_operation->argument >=
							ctx->metadata->operation_count) {
						return -1;
					}
					reverse_argument_operations[argument_operation_count++] =
						cursor_operation->argument;
					cursor = cursor_operation->function;
				}
				if (argument_operation_count != argument_count) {
					return -1;
				}
					uint32_t argument_operations[64];
					uint32_t argument_classifiers[64];
					struct prototype_judgement_selected_evidence argument_evidence[64];
				for (uint32_t i = 0; i < argument_count; ++i) {
					uint32_t argument_operation = reverse_argument_operations[
						argument_count - i - 1
					];
					argument_operations[i] = argument_operation;
					const struct prototype_operation_node* argument_operation_node =
						&ctx->metadata->operations[argument_operation];
					argument_classifiers[i] = argument_operation_node->classifier;
					if (argument_classifiers[i] == PROTOTYPE_INVALID_ID) {
						return 1;
					}
					int status = operation_solver_reify_core_proof(
						ctx,
						argument_operation,
						arguments[i],
						depth + 1
					);
					if (status != 0) {
						return status;
					}
						status = operation_solver_select_evidence_in_context(
							ctx,
						operation_solver_evidence_owner(ctx, argument_operation),
						operation->context_id,
						arguments[i],
							argument_classifiers[i],
							&argument_evidence[i]
					);
					if (status != 0) {
						return status;
					}
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, operation_id
				);
				(void)head;
				(void)owner;
				(void)constructor_index;
				return prototype_judgement_delta_record_constructor_spine(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
						classifier,
						argument_operations,
						argument_evidence,
					argument_count
				);
			}
			const struct prototype_operation_node* function =
				&ctx->metadata->operations[operation->function];
			const struct prototype_operation_node* argument =
				&ctx->metadata->operations[operation->argument];
			if (function->classifier == PROTOTYPE_INVALID_ID ||
				argument->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->function, term->as.app.function, depth + 1
			);
			if (status != 0) {
				return status;
			}
			status = operation_solver_reify_core_proof(
				ctx, operation->argument, term->as.app.argument, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence function_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->function),
				operation->context_id,
				term->as.app.function,
				function->classifier,
				&function_evidence
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence argument_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->argument),
				operation->context_id,
				term->as.app.argument,
				argument->classifier,
				&argument_evidence
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			if (prototype_judgement_delta_record_app_elim(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					&function_evidence,
					&argument_evidence
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE: {
			uint32_t child_term;
			if (operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			if (term->tag == PROTOTYPE_TERM_RETURN) {
				child_term = term->as.return_term.value;
			} else if (term->tag == PROTOTYPE_TERM_THUNK) {
				child_term = term->as.thunk.computation;
			} else if (term->tag == PROTOTYPE_TERM_FORCE) {
				child_term = term->as.force.value;
			} else {
				return -1;
			}
			uint32_t child_classifier = ctx->metadata->operations[
				operation->argument
			].classifier;
			if (child_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->argument, child_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence child_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->argument),
				operation->context_id,
				child_term,
				child_classifier,
				&child_evidence
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			uint32_t derived_classifier;
			if (prototype_judgement_cbpv_boundary_classifier(
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_classifier,
					&derived_classifier
				) != 0 || prototype_judgement_delta_record_cbpv_boundary(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					operation->argument,
					&child_evidence
				) != 0) {
				return -1;
			}
			if (derived_classifier != classifier) {
				struct prototype_judgement_selected_evidence boundary_evidence;
				if (prototype_judgement_delta_select_evidence(
						&ctx->judgement_delta,
						operation_id,
						operation->context_id,
						core_term,
						derived_classifier,
						&boundary_evidence
					) != 0 ||
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&boundary_evidence,
						classifier,
						core_term
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD: {
			if (term->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
				operation->function >= ctx->metadata->operation_count) {
				return -1;
			}
			uint32_t proven_classifier;
			int proven_status = operation_solver_lookup_operation_derivation_classifier(
				ctx,
				operation_id,
				core_term,
				PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
				&proven_classifier
			);
			if (proven_status < 0) {
				return -1;
			}
			if (proven_status == 0) {
				int evidence_status = operation_solver_require_evidence_in_context(
					ctx,
					operation_id,
					operation->context_id,
					core_term,
					proven_classifier
				);
				return evidence_status;
			}
			uint32_t clause_count = term->as.computation_fold.clause_count;
			if (clause_count != 0) {
				/* Clause-bearing folds are kernel solver conclusions. The
				 * OperationGraph classifier is only a fixed-point approximation. */
				return 1;
			}
			if (clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
				operation->fold_clause_count != clause_count ||
				(clause_count != 0 &&
				 (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				  clause_count > ctx->metadata->operation_fold_clause_count -
					operation->first_fold_clause ||
				  term->as.computation_fold.first_clause >
					ctx->terms->computation_fold_clause_count ||
				  clause_count > ctx->terms->computation_fold_clause_count -
					term->as.computation_fold.first_clause))) {
				return -1;
			}
			const struct prototype_operation_node* computation =
				&ctx->metadata->operations[operation->function];
			uint32_t return_operation_id = clause_count == 0 ?
				operation->argument : operation->fold_return_operation;
			if (return_operation_id >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* return_operation =
				&ctx->metadata->operations[return_operation_id];
			uint32_t premise_count = 2 + 2 * clause_count;
				uint32_t premise_operations[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				struct prototype_judgement_selected_evidence premise_evidence[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
			premise_operations[0] = operation->function;
			premise_operations[1] = return_operation_id;
			premise_terms[0] = term->as.computation_fold.computation;
			premise_terms[1] = term->as.computation_fold.return_clause;
			premise_classifiers[0] = computation->classifier;
			premise_classifiers[1] = return_operation->classifier;
			if (computation->tag == PROTOTYPE_OPERATION_REQUEST) {
				int proven_status = operation_solver_lookup_operation_derivation_classifier(
					ctx,
					operation->function,
					premise_terms[0],
					PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
					&premise_classifiers[0]
				);
				if (proven_status != 0) {
					return proven_status < 0 ? -1 : 1;
				}
			}
			for (uint32_t i = 0; i < clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* occurrence_clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				const struct prototype_computation_fold_clause* core_clause =
					&ctx->terms->computation_fold_clauses[
						term->as.computation_fold.first_clause + i
					];
				premise_operations[2 + 2 * i] =
					occurrence_clause->operation_operation;
				premise_operations[3 + 2 * i] =
					occurrence_clause->clause_operation;
				premise_terms[2 + 2 * i] = core_clause->operation;
				premise_terms[3 + 2 * i] = core_clause->body;
			}
			for (uint32_t i = 2; i < premise_count; ++i) {
				if (premise_operations[i] >= ctx->metadata->operation_count) {
					return -1;
				}
				premise_classifiers[i] = ctx->metadata->operations[
					premise_operations[i]
				].classifier;
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				if (premise_classifiers[i] == PROTOTYPE_INVALID_ID) {
					return 1;
				}
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				int status = operation_solver_reify_core_proof(
					ctx,
					premise_operations[i],
					premise_terms[i],
					depth + 1
				);
				if (status != 0) {
					return status;
				}
					if (clause_count == 0) {
						status = operation_solver_select_evidence_in_context(
							ctx,
						operation_solver_evidence_owner(
							ctx, premise_operations[i]
						),
						operation->context_id,
						premise_terms[i],
							premise_classifiers[i],
							&premise_evidence[i]
					);
					if (status != 0) {
						return status;
					}
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			int status = prototype_judgement_delta_record_computation_fold_elim(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
					core_term,
						classifier,
						premise_operations,
						premise_evidence,
				premise_count
			);
			return status < 0 ? -1 : 0;
		}
		case PROTOTYPE_OPERATION_VAR: {
			if (term->tag != PROTOTYPE_TERM_VAR) {
				return -1;
			}
			int status = operation_solver_require_evidence_in_context(
				ctx,
				PROTOTYPE_INVALID_ID,
				operation->context_id,
				core_term,
				classifier
			);
			if (status <= 0) {
				return status;
			}
			for (uint32_t lambda_operation_id = 0;
				lambda_operation_id < ctx->metadata->operation_count;
				++lambda_operation_id) {
				const struct prototype_operation_node* lambda_operation =
					&ctx->metadata->operations[lambda_operation_id];
				uint8_t visited[4096];
				if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
					lambda_operation->referenced_ast_binder_id !=
						operation->referenced_ast_binder_id ||
					lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						lambda_operation->binder_classifier,
						classifier
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				memset(visited, 0, sizeof(visited));
				if (operation_subtree_contains_operation(
						ctx,
						lambda_operation->body,
						operation_id,
						visited
					) <= 0 ||
					lambda_operation->body >= ctx->metadata->operation_count) {
					continue;
				}
				uint32_t binder_context =
					ctx->metadata->operations[lambda_operation->body].context_id;
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, binder_context
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->terms->terms[core_term].as.var.binding_id,
						classifier
					) != 0) {
					return -1;
				}
				status = operation_solver_require_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					operation->context_id,
					core_term,
					classifier
				);
				return status == 0 ? 0 : status;
			}
			return 1;
		}
		case PROTOTYPE_OPERATION_NAME: {
			if (operation->function >= ctx->metadata->operation_count) {
				return -1;
			}
			int child_status = operation_solver_reify_core_proof(
				ctx, operation->function, core_term, depth + 1
			);
			if (child_status != 0) {
				return child_status;
			}
			return operation_solver_require_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->function),
				operation->context_id,
				core_term,
				classifier
			);
		}
		case PROTOTYPE_OPERATION_MATCH: {
			if (term->tag != PROTOTYPE_TERM_MATCH ||
				operation->scrutinee >= ctx->metadata->operation_count ||
				operation->first_case + operation->case_count >
					ctx->metadata->operation_case_count ||
				term->as.match.case_count != operation->case_count) {
				return -1;
			}
			int status = operation_solver_reify_core_proof(
				ctx,
				operation->scrutinee,
				term->as.match.scrutinee,
				depth + 1
			);
			if (status != 0) {
				return status;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				const struct prototype_operation_match_case* operation_case =
					&ctx->metadata->operation_cases[operation->first_case + i];
				if (case_id >= ctx->terms->case_count ||
					operation_case->body_operation >=
						ctx->metadata->operation_count) {
					return -1;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					operation_case->body_operation,
					ctx->terms->cases[case_id].body,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
			}
			return operation_solver_require_evidence_in_context(
				ctx,
				operation_id,
				operation->context_id,
				core_term,
				classifier
			);
		}
		case PROTOTYPE_OPERATION_ASCRIPTION: {
			if (operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation->body];
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->body, body->core_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			struct prototype_judgement_selected_evidence source_evidence;
			if (prototype_judgement_delta_select_evidence(
					&ctx->judgement_delta,
					operation_solver_evidence_owner(ctx, operation->body),
					body->context_id,
					core_term,
					body->classifier,
					&source_evidence
				) != 0) {
				return -1;
			}
			return prototype_judgement_delta_record_expected_type_exposure(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				&source_evidence,
				classifier,
				core_term
			);
		}
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			return 0;
		case PROTOTYPE_OPERATION_REQUEST:
			if (term->tag != PROTOTYPE_TERM_OPERATION_REQUEST ||
				operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			{
				const struct prototype_operation_node* function =
					&ctx->metadata->operations[operation->function];
				const struct prototype_operation_node* argument =
					&ctx->metadata->operations[operation->argument];
				if (function->classifier == PROTOTYPE_INVALID_ID ||
					argument->classifier == PROTOTYPE_INVALID_ID) {
					return 1;
				}
				int status = operation_solver_reify_core_proof(
					ctx,
					operation->function,
					term->as.operation_request.operation,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					operation->argument,
					term->as.operation_request.argument,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				uint32_t application;
				if (prototype_term_app(
						ctx->terms,
						term->as.operation_request.operation,
						term->as.operation_request.argument,
						&application
					) != 0) {
					return -1;
				}
				struct prototype_judgement_selected_evidence function_evidence;
				status = operation_solver_select_evidence_in_context(
					ctx,
					operation_solver_evidence_owner(ctx, operation->function),
					operation->context_id,
					term->as.operation_request.operation,
					function->classifier,
					&function_evidence
				);
				if (status != 0) {
					return status;
				}
				struct prototype_judgement_selected_evidence argument_evidence;
				status = operation_solver_select_evidence_in_context(
					ctx,
					operation_solver_evidence_owner(ctx, operation->argument),
					operation->context_id,
					term->as.operation_request.argument,
					argument->classifier,
					&argument_evidence
				);
				if (status != 0) {
					return status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				uint32_t application_classifier;
				if (prototype_judgement_delta_app_elim_classifier(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						function->classifier,
						term->as.operation_request.argument,
						argument->classifier,
						&application_classifier
					) != 0 || prototype_judgement_delta_record_app_elim(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						application,
						application_classifier,
						&function_evidence,
						&argument_evidence
					) != 0) {
					return -1;
				}
			}
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			return prototype_judgement_delta_record_computation_constraint(
				&ctx->judgement_delta,
				ctx->terms,
				operation->context_id,
				core_term
			);
		default:
			return -1;
	}
}

static int operation_solver_materialize_judgements(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	if (materialize_pending_binder_assumptions(ctx) != 0 ||
		materialize_pending_declaration_facts(ctx) != 0) {
		return -1;
	}
	if (operation_solver_materialize_match_pattern_assumptions(ctx) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		if (operation->tag != PROTOTYPE_OPERATION_MATCH) {
			continue;
		}
		if (!operation_match_cases_are_resolved(ctx, operation)) {
			continue;
		}
		/* A Match classifier is solved from its branch computations.  In the
		 * first fixed-point round RETURN/THUNK/COMPUTATION_FOLD may not yet have supplied
		 * those branch classifiers, so an unresolved Match is a deferred
		 * equation, not a compilation error. */
		if (operation->classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, operation->context_id
		);
		prototype_judgement_delta_set_operation(
			&ctx->judgement_delta, i
		);
		uint32_t branch_operation_ids[64];
		struct prototype_judgement_selected_evidence branch_evidence[64];
		if (operation->case_count > 64) {
			return -1;
		}
		int branches_ready = 1;
		for (uint32_t case_index = 0;
			case_index < operation->case_count;
			++case_index) {
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[operation->first_case + case_index];
			if (operation_case->body_operation >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation_case->body_operation];
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				branches_ready = 0;
				break;
			}
			branch_operation_ids[case_index] = operation_case->body_operation;
			int evidence_status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(
					ctx, operation_case->body_operation
				),
				body->context_id,
				body->core_term,
				body->classifier,
				&branch_evidence[case_index]
			);
			if (evidence_status < 0) {
				return -1;
			}
			if (evidence_status != 0) {
				branches_ready = 0;
				break;
			}
		}
		if (!branches_ready) {
			continue;
		}
		if (prototype_judgement_delta_record_materialized_match_motive(
				&ctx->judgement_delta,
				ctx->terms,
				operation->core_term,
				operation->classifier,
				branch_operation_ids,
				branch_evidence,
				operation->case_count
			) != 0) {
			return -1;
		}
	}
	for (;;) {
		size_t before_relation_count = ctx->judgement_delta.proposition_count;
		for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[i];
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, i
			);
			uint32_t classifier = operation->classifier;
			if (classifier == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int free_effect_row = operation_classifier_contains_unowned_effect_row(
				ctx, i, classifier
			);
			if (free_effect_row < 0) {
				return -1;
			}
			if (free_effect_row != 0) {
				/* The OperationGraph may retain this constraint state, but a
				 * JudgementDB conclusion must be closed or explicitly generalized. */
				continue;
			}
			if (operation->tag == PROTOTYPE_OPERATION_LAMBDA ||
				operation->tag == PROTOTYPE_OPERATION_APP ||
				operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ||
				operation->tag == PROTOTYPE_OPERATION_RETURN ||
				operation->tag == PROTOTYPE_OPERATION_THUNK ||
				operation->tag == PROTOTYPE_OPERATION_FORCE ||
				operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
				operation->tag == PROTOTYPE_OPERATION_REQUEST) {
				int reify_status = operation_solver_reify_core_proof(
					ctx, i, operation->core_term, 0
				);
				if (reify_status < 0) {
					return -1;
				}
				if (reify_status > 0) {
					continue;
				}
			} else if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
				if (operation_solver_materialize_induction_hypothesis_judgement(
						ctx, i
					) != 0) {
					return -1;
				}
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				prototype_term_type_instance_info(
					ctx->terms,
					operation->core_term,
					&(uint32_t){0},
					NULL,
					&(uint32_t){0}
				) == 0 &&
				prototype_judgement_delta_record_type_formation(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_PURE_PRIMITIVE &&
				prototype_judgement_delta_record_pure_primitive_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_EFFECT_OPERATION &&
				prototype_judgement_delta_record_effect_operation_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					operation->classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_TEXT_LITERAL &&
				prototype_judgement_delta_record_text_literal(
					&ctx->judgement_delta,
					ctx->terms,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_INT_LITERAL) {
				if (prototype_judgement_delta_record_int_literal(
						&ctx->judgement_delta,
						ctx->terms,
						operation->core_term,
						classifier
					) != 0) {
					return -1;
				}
				int64_t value = ctx->terms->terms[
					operation->core_term
				].as.int_literal.value;
				if (value >= INT32_MIN && value <= INT32_MAX) {
					uint32_t integer;
					struct prototype_judgement_selected_evidence source_evidence;
					if (prototype_term_make_host_type(
								ctx->terms,
								PROTOTYPE_HOST_TYPE_INT32,
								&integer
							) != 0 || prototype_judgement_delta_select_evidence(
								&ctx->judgement_delta,
								i,
								operation->context_id,
								operation->core_term,
								classifier,
								&source_evidence
							) != 0 ||
							prototype_judgement_delta_record_int_literal_admissibility(
								&ctx->judgement_delta,
								ctx->terms,
								&source_evidence,
								integer
							) != 0) {
							return -1;
						}
					}
			}
		}
		if (ctx->judgement_delta.proposition_count == before_relation_count) {
			break;
		}
	}
	return 0;
}

static int operation_solver_resolve_contexts(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->contexts.context_count > PROTOTYPE_CONTEXT_CAPACITY) {
		return -1;
	}
	uint32_t relocation[PROTOTYPE_CONTEXT_CAPACITY];
	size_t source_count = ctx->metadata->contexts.context_count;
	relocation[0] = 0;
	for (uint32_t context_id = 1; context_id < source_count; ++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context || context->parent >= context_id) {
			return -1;
		}
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		int classifier_variable_resolved = 0;
		if (classifier_variable != PROTOTYPE_INVALID_ID) {
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->operation_count;
				++operation_id) {
				const struct prototype_operation_node* operation =
					&ctx->metadata->operations[operation_id];
				uint32_t operation_classifier =
					operation_available_classifier(operation);
				if (operation->tag == PROTOTYPE_OPERATION_VAR &&
					operation->referenced_ast_binder_id ==
						classifier_variable &&
					operation_classifier != PROTOTYPE_INVALID_ID) {
					classifier = operation_classifier;
					classifier_variable_resolved = 1;
				}
			}
			for (int source = 0;
				classifier == PROTOTYPE_INVALID_ID && source < 2;
				++source) {
				const struct prototype_judgement_proposition* relations = source == 0 ?
					ctx->judgement_delta.propositions : ctx->judgement->propositions;
				const struct prototype_judgement_derivation_candidate* derivations = source == 0 ?
					ctx->judgement_delta.derivation_candidates :
					ctx->judgement->derivation_candidates;
				size_t proposition_count = source == 0 ?
					ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
				size_t derivation_candidate_count = source == 0 ?
					ctx->judgement_delta.derivation_candidate_count :
					ctx->judgement->derivation_candidate_count;
				for (size_t relation_id = 0; relation_id < proposition_count; ++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&relations[relation_id];
					if (relation->context_id != context_id ||
						relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						prototype_judgement_candidate_find_derivation_kind(
							relations,
							proposition_count,
							derivations,
							derivation_candidate_count,
							(uint32_t)relation_id,
							PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
							&(uint32_t){0}
						) != 0 ||
						relation->subject >= ctx->terms->term_count ||
						ctx->terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
						ctx->terms->terms[relation->subject].as.var.binding_id !=
							context->binding_id) {
						continue;
					}
					classifier = relation->classifier;
					classifier_variable_resolved = 1;
					break;
				}
			}
		}
		if (prototype_context_extend(
				&ctx->metadata->contexts,
				relocation[context->parent],
				context->binding_id,
				classifier,
				finalize && classifier_variable_resolved ?
					PROTOTYPE_INVALID_ID : classifier_variable,
				&relocation[context_id]
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		uint32_t context_id = ctx->metadata->operations[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operations[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_case_count; ++i) {
		uint32_t context_id = ctx->metadata->operation_cases[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operation_cases[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_fold_clause_count; ++i) {
		uint32_t context_id = ctx->metadata->operation_fold_clauses[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operation_fold_clauses[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		uint32_t context_id = ctx->pending_binder_assumptions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->pending_binder_assumptions[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		uint32_t context_id = ctx->pending_declaration_facts[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->pending_declaration_facts[i].context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		uint32_t context_id = ctx->judgement_delta.propositions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->judgement_delta.propositions[i].context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement_delta.derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof =
			&ctx->judgement_delta.derivation_candidates[i];
		if (proof->conclusion_context_id >= source_count) {
			return -1;
		}
		proof->conclusion_context_id =
			relocation[proof->conclusion_context_id];
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (proof->premises[j].proposition.context_id >= source_count) {
				return -1;
			}
			proof->premises[j].proposition.context_id =
				relocation[proof->premises[j].proposition.context_id];
		}
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			(proof->premise_count != 1 ||
			 prototype_substitution_projection_path(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				proof->conclusion_context_id,
				proof->premises[0].proposition.context_id,
				&proof->semantic_action_id
			 ) != 0)) {
			return -1;
		}
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		uint32_t context_id =
			ctx->judgement_delta.computation_constraints[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->judgement_delta.computation_constraints[i].context_id =
			relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement->proposition_count; ++i) {
		uint32_t context_id = ctx->judgement->propositions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->judgement->propositions[i].context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement->derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof = &ctx->judgement->derivation_candidates[i];
		if (proof->conclusion_context_id >= source_count) {
			return -1;
		}
		proof->conclusion_context_id =
			relocation[proof->conclusion_context_id];
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (proof->premises[j].proposition.context_id >= source_count) {
				return -1;
			}
			proof->premises[j].proposition.context_id =
				relocation[proof->premises[j].proposition.context_id];
		}
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			(proof->premise_count != 1 ||
			 prototype_substitution_projection_path(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				proof->conclusion_context_id,
				proof->premises[0].proposition.context_id,
				&proof->semantic_action_id
			 ) != 0)) {
			return -1;
		}
	}
	if (prototype_context_db_rebuild_index(
			&ctx->metadata->contexts
		) != 0 || prototype_substitution_db_rebuild_index(
			&ctx->metadata->substitutions
		) != 0) {
		return -1;
	}
	return 0;
}

static int operation_solver_generate_computation_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
			operation->tag != PROTOTYPE_OPERATION_REQUEST) {
			continue;
		}
		prototype_judgement_delta_set_operation(
			&ctx->judgement_delta, i
		);
		int status = prototype_judgement_delta_record_computation_constraint(
			&ctx->judgement_delta,
			ctx->terms,
			operation->context_id,
			operation->core_term
		);
		if (status != 0) {
			return -1;
		}
		struct prototype_judgement_computation_constraint* constraint = NULL;
		for (size_t constraint_id = 0;
			constraint_id < ctx->judgement_delta.computation_constraint_count;
			++constraint_id) {
			struct prototype_judgement_computation_constraint* candidate =
				&ctx->judgement_delta.computation_constraints[constraint_id];
			if (candidate->operation_id == i &&
				candidate->subject == operation->core_term) {
				constraint = candidate;
				break;
			}
		}
		if (!constraint) {
			return -1;
		}
		constraint->premise_operation_count = 0;
		constraint->return_body_operation_id = PROTOTYPE_INVALID_ID;
		constraint->return_body_context_id = PROTOTYPE_INVALID_ID;
		constraint->return_body_subject = PROTOTYPE_INVALID_ID;
		for (uint32_t premise = 0;
			premise < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES;
			++premise) {
			constraint->premise_contexts[premise] = PROTOTYPE_INVALID_ID;
			constraint->premise_states[premise] =
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
		if (operation->tag == PROTOTYPE_OPERATION_REQUEST) {
			if (operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count ||
				operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			constraint->premise_operations[0] = operation->function;
			constraint->premise_operations[1] = operation->argument;
			constraint->premise_operations[2] = operation->body;
			constraint->premise_operation_count = 3;
			continue;
		}
		uint32_t return_operation = operation->fold_clause_count == 0 ?
			operation->argument : operation->fold_return_operation;
		uint32_t premise_count = 2 + 2 * operation->fold_clause_count;
		if (operation->function >= ctx->metadata->operation_count ||
			return_operation >= ctx->metadata->operation_count || premise_count > 64 ||
			(operation->fold_clause_count != 0 &&
			 ((size_t)operation->first_fold_clause + operation->fold_clause_count >
				ctx->metadata->operation_fold_clause_count))) {
			return -1;
		}
		constraint->premise_operations[0] = operation->function;
		constraint->premise_operations[1] = return_operation;
		if (ctx->metadata->operations[return_operation].tag ==
				PROTOTYPE_OPERATION_LAMBDA &&
			ctx->metadata->operations[return_operation].body <
				ctx->metadata->operation_count) {
			constraint->return_body_operation_id =
				ctx->metadata->operations[return_operation].body;
			constraint->return_body_context_id = ctx->metadata->operations[
				constraint->return_body_operation_id
			].context_id;
			constraint->return_body_subject = ctx->metadata->operations[
				constraint->return_body_operation_id
			].core_term;
		}
		for (uint32_t clause = 0; clause < operation->fold_clause_count; ++clause) {
			const struct prototype_operation_computation_fold_clause* fold_clause =
				&ctx->metadata->operation_fold_clauses[
					operation->first_fold_clause + clause
				];
			constraint->premise_operations[2 + 2 * clause] =
				fold_clause->operation_operation;
			constraint->premise_operations[3 + 2 * clause] =
				fold_clause->clause_operation;
		}
		constraint->premise_operation_count = premise_count;
	}
	return 0;
}

static int operation_solver_bind_computation_constraint_solutions(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !p_changed) {
		return -1;
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		const struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (constraint->operation_id == PROTOTYPE_INVALID_ID ||
			constraint->solved_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->operation_id >= ctx->metadata->operation_count ||
			constraint->solved_classifier >= ctx->terms->term_count ||
			operation_solver_bind_proven_classifier(
				ctx,
				constraint->operation_id,
				constraint->solved_classifier,
				p_changed
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_refresh_computation_constraint_operands(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (constraint->premise_operation_count > 64) {
			return -1;
		}
		for (uint32_t premise = 0;
			premise < constraint->premise_operation_count;
			++premise) {
			uint32_t operation_id = constraint->premise_operations[premise];
			if (operation_id >= ctx->metadata->operation_count) {
				return -1;
			}
			uint32_t classifier = operation_solver_classifier(ctx, operation_id);
			constraint->premise_classifiers[premise] = classifier;
			memset(
				&constraint->premise_evidence[premise],
				0,
				sizeof(constraint->premise_evidence[premise])
			);
			constraint->premise_contexts[premise] =
				ctx->metadata->operations[operation_id].context_id;
			if (classifier == PROTOTYPE_INVALID_ID) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
				continue;
			}
			int local = operation_classifier_contains_unowned_effect_row(
				ctx, operation_id, classifier
			);
			if (local < 0) {
				return -1;
			}
			if (local) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL;
				continue;
			}
			int evidence_status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation_id),
				constraint->context_id,
				ctx->metadata->operations[operation_id].core_term,
				classifier,
				&constraint->premise_evidence[premise]
			);
			if (evidence_status < 0) {
				return -1;
			}
			constraint->premise_states[premise] = evidence_status == 0 ?
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED :
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
	}
	return 0;
}

static int bind_cbpv_operation_classifiers(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	/* RETURN/THUNK/FORCE are one kernel rule parameterized by the child
	 * classifier of this source occurrence.  A shared erased boundary node may
	 * legitimately have several such derivations, so do not recover one child
	 * classifier globally from TermDB. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if ((operation->tag != PROTOTYPE_OPERATION_RETURN &&
			 operation->tag != PROTOTYPE_OPERATION_THUNK &&
			 operation->tag != PROTOTYPE_OPERATION_FORCE) ||
			operation->argument >= ctx->metadata->operation_count) {
			continue;
		}
		uint32_t child_classifier = operation_solver_classifier(
			ctx, operation->argument
		);
		if (child_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		if (prototype_judgement_cbpv_boundary_classifier(
				ctx->terms,
				ctx->type_declarations,
				operation->core_term,
				child_classifier,
				&classifier
			) != 0 || operation_solver_bind_proven_classifier(
				ctx, operation_id, classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	return operation_solver_commit_bindings(ctx, p_changed);
}

static int compile_phase_infer_pending_types(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	for (;;) {
		size_t before_operation_classifiers = count_classified_operations(ctx);
		size_t before_unresolved = count_unresolved_resolution_items(ctx);
		size_t before_relations = ctx->judgement_delta.proposition_count;
		int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
		if (match_resolution_status < 0) {
			return -1;
		}
		if (operation_solver_resolve_contexts(ctx, 0) != 0) {
			return -1;
		}
			int status = compile_phase_infer_general_classifiers(ctx, 0);
			if (status != 0) {
				return -1;
			}
			if (operation_solver_resolve_contexts(ctx, 0) != 0) {
				return -1;
			}
			/* Source operations carry the selected typed occurrence. Materialize their
			 * CBPV boundaries before solving sequencing constraints so a shared
			 * FORCE/THUNK node is never reclassified from an unrelated occurrence. */
		if (operation_solver_materialize_judgements(ctx) != 0) {
			fprintf(stderr, "typing fixed point: materialization failed\n");
			return -1;
		}
		int cbpv_changed = 0;
		if (bind_cbpv_operation_classifiers(ctx, &cbpv_changed) != 0) {
			fprintf(stderr, "typing fixed point: CBPV classifier binding failed\n");
			return -1;
		}
		if (operation_solver_generate_computation_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint generation failed\n");
			return -1;
		}
		if (operation_solver_refresh_computation_constraint_operands(ctx) != 0) {
			fprintf(stderr, "typing fixed point: computation operand refresh failed\n");
			return -1;
		}
		if (prototype_judgement_delta_solve_recorded_computation_constraints(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations
			) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint solving failed\n");
			return -1;
		}
		if (operation_solver_bind_computation_constraint_solutions(
				ctx, &cbpv_changed
			) != 0) {
			fprintf(stderr, "typing fixed point: computation solution binding failed\n");
			return -1;
		}
		if (operation_effect_generate_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: effect constraint generation failed\n");
			return -1;
		}
		if (count_classified_operations(ctx) == before_operation_classifiers &&
			count_unresolved_resolution_items(ctx) == before_unresolved &&
			ctx->judgement_delta.proposition_count == before_relations && !cbpv_changed) {
			break;
		}
	}

	int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
	if (match_resolution_status < 0) {
		return -1;
	}
	int status = compile_phase_infer_general_classifiers(ctx, 1);
	if (status != 0) {
		return -1;
	}
	if (operation_solver_resolve_contexts(ctx, 0) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		uint32_t operation = ctx->pending_match_typings[i].operation;
		if (operation >= ctx->metadata->operation_count ||
			ctx->classifier_solver.bindings[operation] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	if (operation_solver_materialize_judgements(ctx) != 0) {
		return -1;
	}
	/* Reification can introduce binder premises in a symbolic Context after the
	 * last solver pass. Refine those Contexts before publishing exact binding
	 * identities to JudgementDB. */
	if (operation_solver_resolve_contexts(ctx, 1) != 0) {
		return -1;
	}
	/* Candidate publication is decided by the authority-complete Claim closure.
	 * Do not pre-prune structural derivations with legacy Core tuples here. */
	if (prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0) {
		return -1;
	}
	/* The materialized classifier relation has been committed. This temporary
	 * solver lookup index must not survive as pending JudgementDB state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return count_unresolved_resolution_items(ctx) == 0 ? 0 : -1;
}

static int compile_phase_check_ascriptions(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	struct prototype_term_definition imported_definitions[1024];
	struct prototype_term_definition_env imported_definition_env;
	if (build_imported_external_definition_env(
			ctx,
			imported_definitions,
			1024,
			&imported_definition_env
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < ctx->pending_ascription_check_count; ++i) {
		const struct pending_ascription_check* check =
			&ctx->pending_ascription_checks[i];
		uint32_t operation_classifier = PROTOTYPE_INVALID_ID;
		uint32_t ascription_operation = PROTOTYPE_INVALID_ID;
		uint32_t solved_expected_classifier = check->expected_classifier;
		if (check->operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[check->operation].classifier !=
				PROTOTYPE_INVALID_ID) {
			operation_classifier = ctx->metadata->operations[check->operation].classifier;
			if (prototype_judgement_solve_expected_effect_rows(
					ctx->terms,
					ctx->type_declarations,
					&imported_definition_env,
					check->expected_classifier,
					operation_classifier,
					&solved_expected_classifier
				) != 0) {
				(void)add_resolve_error(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_COMPILE,
					-1,
					-1,
					check->ast
				);
				continue;
			}
		}
		if (operation_classifier == PROTOTYPE_INVALID_ID ||
			!prototype_judgement_classifier_compatible_with_definitions(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				solved_expected_classifier,
				operation_classifier
			)) {
			(void)add_resolve_error(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				-1,
				-1,
				check->ast
			);
			continue;
		}
		for (uint32_t operation = 0;
			operation < ctx->metadata->operation_count;
			++operation) {
			if (ctx->metadata->operations[operation].tag ==
					PROTOTYPE_OPERATION_ASCRIPTION &&
				ctx->metadata->operations[operation].body == check->operation &&
				ctx->metadata->operations[operation].source_ast == check->ast) {
				ctx->metadata->operations[operation].classifier =
					solved_expected_classifier;
				ascription_operation = operation;
			}
		}
		if (ascription_operation == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		/* This phase validates and solves the source ascription constraint. Its
		 * evidence is materialized earlier from the exact ASCRIPTION Operation in
		 * JudgementDelta. Writing a second relation here would bypass closure and
		 * solver-evidence atomicity. */
	}
	return 0;
}

static int compile_phase_check_expectations(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	struct prototype_term_definition imported_definitions[1024];
	struct prototype_term_definition_env imported_definition_env;
	if (build_imported_external_definition_env(
			ctx,
			imported_definitions,
			1024,
			&imported_definition_env
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->expectation_count; ++i) {
		struct prototype_ast_type_expectation_def* expectation = &ctx->asts->expectations[i];
		if (expectation->kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION) {
			continue;
		}
		struct prototype_ast_term_assignment_def* def;
		uint32_t expected_classifier;
		uint32_t actual_classifier = PROTOTYPE_INVALID_ID;
		uint32_t compiled_term;
		int actual_from_operation = 0;
		if (resolve_unique_assignment(ctx, expectation->name_symbol_id, PROTOTYPE_INVALID_ID, &def) != 0) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		if (compile_def(ctx, def, &compiled_term) != 0 ||
			compile_type_expectation_classifier(ctx, expectation, &expected_classifier) != 0) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->name_span
			);
			continue;
		}
		if (def->definition_value_required && def->ast < ctx->asts->node_count &&
			ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_QUOTE &&
			def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag ==
				PROTOTYPE_OPERATION_THUNK) {
			/*
			 * The ascription checks the explicit definition RHS before the definition
			 * boundary inserts its implicit thunk. The exported definition retains
			 * the synthesized thunk classifier.
			 */
			continue;
		}
		struct compile_ref expected_ref;
		compile_ref_clear(&expected_ref);
		if (def->compiled_operation >= ctx->metadata->operation_count) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->name_span
			);
			continue;
		}
		expected_ref.polarity =
			ctx->metadata->operations[def->compiled_operation].polarity;
		expected_ref.computation_kind =
			ctx->metadata->operations[def->compiled_operation].computation_kind;
		if (compile_expected_classifier_for_ref(
				ctx, &expected_ref, expected_classifier, &expected_classifier
			) != 0) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->type_span
			);
			continue;
		}
		if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag ==
				PROTOTYPE_OPERATION_ASCRIPTION &&
			ctx->metadata->operations[def->compiled_operation].body <
				ctx->metadata->operation_count &&
			ctx->metadata->operations[
				ctx->metadata->operations[def->compiled_operation].body
			].classifier != PROTOTYPE_INVALID_ID) {
			/* The ascription is an annotation; its body supplies the evidence. */
			actual_classifier = ctx->metadata->operations[
				ctx->metadata->operations[def->compiled_operation].body
			].classifier;
			actual_from_operation = 1;
		} else if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag !=
				PROTOTYPE_OPERATION_ASCRIPTION &&
			ctx->metadata->operations[def->compiled_operation].classifier !=
				PROTOTYPE_INVALID_ID) {
			/*
			 * A source definition denotes an operation-layer node. Its classifier
			 * must not be recovered by collecting every classifier ever assigned to
			 * the shared erased core term.
			 */
			actual_classifier =
				ctx->metadata->operations[def->compiled_operation].classifier;
		} else if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag !=
				PROTOTYPE_OPERATION_ASCRIPTION &&
			def->compiled_classifier != PROTOTYPE_INVALID_ID) {
			actual_classifier = def->compiled_classifier;
		} else {
			actual_classifier = PROTOTYPE_INVALID_ID;
		}
		if (actual_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				expected_classifier,
				actual_classifier,
				&expected_classifier
			) != 0) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->type_span
			);
			continue;
		}
		int free_effect_row = operation_classifier_contains_unowned_effect_row(
			ctx, def->compiled_operation, actual_classifier
		);
		if (free_effect_row < 0) {
			return -1;
		}
		if (free_effect_row != 0) {
			/* The expectation has been checked, but its external effect-row
			 * equation remains a residual artifact obligation. Do not expose that
			 * open solver state as a closed JudgementDB certificate. */
			def->compiled_classifier = actual_classifier;
			continue;
		}
		struct prototype_term_conversion_result closed_conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				expected_classifier,
				actual_classifier
			);
		if (closed_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
			!prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				expected_classifier,
				actual_classifier
			)) {
			/* Compatibility may depend on transparent imported definitions. The
			 * source expectation is valid in that environment, but the standalone
			 * kernel certificate has no definition premise with which to replay it. */
			def->compiled_classifier = actual_classifier;
			continue;
			}
			if (actual_from_operation) {
				struct prototype_judgement_selected_evidence source_evidence;
				uint32_t conclusion_context = ctx->metadata->operations[
					def->compiled_operation
				].context_id;
				if (prototype_judgement_select_evidence(
						ctx->judgement,
						def->compiled_operation,
						conclusion_context,
						compiled_term,
						expected_classifier,
						&source_evidence
					) == 0) {
					def->compiled_classifier = expected_classifier;
					continue;
				}
				uint32_t source_operation = def->compiled_operation;
				const struct prototype_operation_node* compiled_operation =
					&ctx->metadata->operations[source_operation];
				if (compiled_operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
					/* The direct body Operation supplied actual_classifier above. Nested
					 * annotations retain distinct evidence and must not be stripped to an
					 * untyped VAR or CONSTRUCTOR occurrence. */
					source_operation = compiled_operation->body;
				}
				if (source_operation >= ctx->metadata->operation_count) {
					return -1;
				}
				uint32_t source_context =
					ctx->metadata->operations[source_operation].context_id;
				if (prototype_judgement_select_evidence(
						ctx->judgement,
						source_operation,
						source_context,
						compiled_term,
						actual_classifier,
						&source_evidence
					) != 0) {
					return -1;
				}
				if (prototype_judgement_add_expected_type_exposure(
						ctx->judgement,
						ctx->terms,
						ctx->type_declarations,
						source_context,
						def->compiled_operation,
						&source_evidence,
						expected_classifier,
						compiled_term
					) != 0) {
				return -1;
			}
			def->compiled_classifier = expected_classifier;
			continue;
			}
			struct prototype_judgement_selected_evidence source_evidence;
			if (prototype_judgement_select_evidence(
					ctx->judgement,
					PROTOTYPE_INVALID_ID,
					0,
					compiled_term,
					actual_classifier,
					&source_evidence
				) != 0 || prototype_judgement_add_expected_type_exposure(
					ctx->judgement,
					ctx->terms,
					ctx->type_declarations,
					0,
					def->compiled_operation,
					&source_evidence,
					expected_classifier,
					compiled_term
				) != 0) {
				return -1;
			}
			/*
			 * A shared computation graph may have several classifiers. The expectation
		 * does not synthesize a classifier here; it selects the already inferred
		 * classifier that this exported name promises to expose.
		 */
		def->compiled_classifier = actual_classifier;
	}
	return 0;
}

static int compile_phase_report_def_index_errors(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->def_index_capacity; ++i) {
		const struct prototype_ast_def_open_address_entry* entry = &ctx->asts->def_index[i];
		if (!entry->occupied) {
			continue;
		}
		uint32_t declaration_count = 0;
		uint32_t scan_id = entry->first_expectation;
		while (scan_id != PROTOTYPE_INVALID_ID && scan_id < ctx->asts->expectation_count) {
			const struct prototype_ast_type_expectation_def* type_entry =
				&ctx->asts->expectations[scan_id];
			if (type_entry->kind == PROTOTYPE_AST_TYPE_ENTRY_DECLARATION) {
				declaration_count++;
			}
			scan_id = type_entry->next_for_symbol;
		}
		if (declaration_count + entry->assignment_count > 1) {
			uint32_t expectation_id = entry->first_expectation;
			while (expectation_id != PROTOTYPE_INVALID_ID && expectation_id < ctx->asts->expectation_count) {
				const struct prototype_ast_type_expectation_def* expectation =
					&ctx->asts->expectations[expectation_id];
				if (expectation->kind == PROTOTYPE_AST_TYPE_ENTRY_DECLARATION) {
					(void)add_resolve_error_at_span(
						ctx,
						PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION,
						entry->symbol_id,
						-1,
						PROTOTYPE_INVALID_ID,
						expectation->name_span
					);
				}
				expectation_id = expectation->next_for_symbol;
			}
			uint32_t assignment_id = entry->first_assignment;
			while (assignment_id != PROTOTYPE_INVALID_ID && assignment_id < ctx->asts->assignment_count) {
				const struct prototype_ast_term_assignment_def* assignment =
					&ctx->asts->assignments[assignment_id];
				(void)add_resolve_error_at_span(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION,
					entry->symbol_id,
					-1,
					assignment->ast,
					assignment->name_span
				);
				assignment_id = assignment->next_for_symbol;
			}
		}
	}
	return 0;
}

static int compile_phase_publish_labels(struct compile_context* ctx) {
	if (!ctx || !ctx->asts || !ctx->terms) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
		struct prototype_ast_term_assignment_def* def = &ctx->asts->assignments[i];
		if (!def->compiled || def->published) {
			continue;
		}
		if (def->compiled_classifier == PROTOTYPE_INVALID_ID &&
			def->compiled_operation < ctx->metadata->operation_count) {
			def->compiled_classifier = ctx->metadata->operations[
				def->compiled_operation
			].classifier;
		}
		if (add_compile_label(
				ctx,
				def->name_symbol_id,
				def->compiled_term,
				def->compiled_classifier,
				def->compiled_operation
			) != 0) {
			return -1;
		}
		def->published = 1;
	}
	if (ctx->asts->root_definition_select != PROTOTYPE_INVALID_ID) {
		const struct prototype_ast_node* select;
		struct prototype_ast_term_assignment_def* selected;
		if (compile_selected_definition(ctx, &select, &selected) != 0) {
			return -1;
		}
		(void)select;
		if (!selected || !selected->compiled ||
			selected->compiled_operation >= ctx->metadata->operation_count ||
			ctx->metadata->operations[selected->compiled_operation].polarity !=
				COMPILE_REF_POLARITY_VALUE) {
			return -1;
		}
		if (ctx->metadata->selected_entry_symbol_id != selected->name_symbol_id ||
			ctx->metadata->selected_entry_term >= ctx->terms->term_count ||
			ctx->metadata->selected_entry_operation >=
				ctx->metadata->operation_count) {
			return -1;
		}
		ctx->metadata->selected_entry_classifier = ctx->metadata->operations[
			ctx->metadata->selected_entry_operation
		].classifier;
		if (ctx->metadata->selected_entry_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	return 0;
}

static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
) {
	if (!metadata || !terms) {
		return;
	}
	uint64_t capabilities = 0;
	struct prototype_verification_coverage coverage;
	if (prototype_verification_db_coverage(
			&metadata->verification, &coverage
		) == 0) {
		capabilities |= coverage.required_runtime_capabilities;
	}
	for (size_t i = 0; i < metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &metadata->operations[i];
		if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
			operation->core_term < terms->term_count &&
			terms->terms[operation->core_term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD &&
			terms->terms[operation->core_term].as.computation_fold.clause_count != 0) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
		}
		if (operation->tag != PROTOTYPE_OPERATION_REQUEST ||
			operation->core_term >= terms->term_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_OPERATION_REQUEST) {
			continue;
		}
		capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH;
		uint32_t head = terms->terms[operation->core_term].as.operation_request.operation;
		while (head < terms->term_count && terms->terms[head].tag == PROTOTYPE_TERM_APP) {
			head = terms->terms[head].as.app.function;
		}
		if (head < terms->term_count &&
			terms->terms[head].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(
					terms->terms[head].as.effect_operation.operation_id
				);
			if (declaration &&
				(declaration->required_host_effects &
					PROTOTYPE_HOST_EFFECT_TERMINAL) != 0) {
				capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
			}
		}
	}
	metadata->required_runtime_capabilities = capabilities;
}

static uint32_t max_existing_universe_level_var(const struct prototype_term_db* terms) {
	uint32_t max_level_var = 0;
	if (!terms) {
		return 0;
	}
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			terms->terms[i].as.universe_var.level_var >= max_level_var) {
			max_level_var = terms->terms[i].as.universe_var.level_var + 1;
		}
	}
	return max_level_var;
}

static void sync_universe_level_counters(struct compile_context* ctx) {
	if (!ctx || !ctx->terms) {
		return;
	}
	uint32_t next_level_var = max_existing_universe_level_var(ctx->terms);
	if (ctx->asts && ctx->asts->next_ast_level_var < next_level_var) {
		ctx->asts->next_ast_level_var = next_level_var;
	}
	if (ctx->judgement && ctx->judgement->next_universe_var < next_level_var) {
		ctx->judgement->next_universe_var = next_level_var;
	}
	if (ctx->type_declarations && ctx->type_declarations->next_level_var < next_level_var) {
		ctx->type_declarations->next_level_var = next_level_var;
	}
}

static int compile_pending_with_workspace(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct compile_judgement_workspace* workspace
) {
	if (!asts || !terms || !type_declarations || !judgement || !workspace) {
		return -1;
	}
	struct compile_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.asts = asts;
	ctx.terms = terms;
	ctx.type_declarations = type_declarations;
	ctx.judgement = judgement;
	prototype_judgement_delta_init(
		&ctx.judgement_delta,
		judgement,
		workspace->propositions,
		workspace->derivation_candidates,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->candidate_premises,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY *
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		workspace->match_motive_results,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->computation_constraints,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->effect_row_constraints,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY
	);
	ctx.metadata = metadata;
	if (!metadata || prototype_context_empty(&metadata->contexts) == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	prototype_judgement_delta_set_context_store(
		&ctx.judgement_delta,
		&metadata->contexts,
		&metadata->substitutions
	);
	ctx.context_ids[0] = prototype_context_empty(&metadata->contexts);
	if (metadata) {
		prototype_judgement_delta_set_solver_budget(
			&ctx.judgement_delta,
			metadata->solver_step_limit,
			&metadata->solver_steps_used,
			&metadata->solver_exhausted
		);
	}
	ctx.namespace_symbol_id = namespace_symbol_id;
	ctx.imported_interfaces = imported_interfaces;
	ctx.imported_interface_count = imported_interface_count;
	sync_universe_level_counters(&ctx);

	if (compile_phase_report_def_index_errors(&ctx) != 0) {
		return -1;
	}
	if (compile_phase_build_graph(&ctx) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_operation_store(
		&ctx.judgement_delta,
		metadata->operations,
		metadata->operation_count,
		metadata->operation_cases,
		metadata->operation_case_count
	);
	if (validate_constructor_occurrence_saturation(&ctx) != 0) {
		return -1;
	}
	if (begin_resolution_iteration(&ctx, 0, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (compile_phase_infer_imported_constructor_classifiers(&ctx) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_resolve_pending_match_items(&ctx) < 0) {
		return -1;
	}
	if (finish_resolution_iteration(&ctx, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_infer_pending_types(&ctx) != 0) {
		return -1;
	}
	/* The fixed point above has already solved and committed source-operation
	 * computation constraints in their contexts. Rebuilding constraints from
	 * TermDB here would erase occurrence contexts and create false closed
	 * derivations for shared core terms. */
	if (operation_effect_generate_constraints(&ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx.judgement_delta, 0) != 0 ||
			compile_phase_record_residual_dependent_folds(&ctx) != 0 ||
			compile_phase_record_residual_computation_fold_results(&ctx) != 0) {
		return -1;
	}
	operation_solver_refresh_constraint_states(&ctx, ctx.metadata->solver_exhausted);
	compile_metadata_refresh_runtime_capabilities(metadata, terms);
	if (metadata && metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		(prototype_verification_db_count(&metadata->verification) != 0 ||
			operation_effect_unresolved_count(metadata) != 0)) {
		return -1;
	}
	if (compile_phase_check_ascriptions(&ctx) != 0) {
		return -1;
	}
	if (compile_phase_check_expectations(&ctx) != 0) {
		return -1;
	}
	if (prototype_judgement_delta_has_pending_classifier_state(&ctx.judgement_delta) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (prototype_judgement_expand_primitives(judgement, terms) != 0) {
		return -1;
	}
	struct prototype_operation_graph operation_graph;
	prototype_compile_metadata_operation_graph(metadata, &operation_graph);
	if (prototype_judgement_add_normalization_premise_conversions(
			terms,
			type_declarations,
			&operation_graph,
			judgement
		) != 0) {
		return -1;
	}
	if (prototype_judgement_publish_candidates(
			&operation_graph,
			judgement
		) != 0 || prototype_judgement_validate_accepted_graph(
			terms,
			type_declarations,
			&metadata->contexts,
			&metadata->substitutions,
			&operation_graph,
			judgement
		) != 0) {
		fprintf(stderr, "P0 proof validation failed\n");
		return -1;
	}
	if (prototype_operation_graph_validate(
			&operation_graph, terms, &metadata->contexts
		) != 0 ||
		prototype_constructor_curried_caches_validate(
			type_declarations, &metadata->contexts, terms
		) != 0) {
		return -1;
	}
	if (prototype_term_erase_constructor_view_owners(terms) != 0) {
		return -1;
	}
	if (compile_phase_publish_labels(&ctx) != 0) {
		return -1;
	}
	if (ctx.had_error) {
		return -1;
	}
	return 0;
}

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
) {
	struct compile_judgement_workspace* workspace = calloc(1, sizeof(*workspace));
	if (!workspace) {
		return -1;
	}
	int status = compile_pending_with_workspace(
		asts,
		terms,
		type_declarations,
		judgement,
		metadata,
		namespace_symbol_id,
		imported_interfaces,
		imported_interface_count,
		workspace
	);
	free(workspace);
	return status;
}

int prototype_ast_compile_pending(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
) {
	return prototype_ast_compile_pending_with_imports(
		asts,
		terms,
		type_declarations,
		judgement,
		metadata,
		-1,
		NULL,
		0
	);
}
