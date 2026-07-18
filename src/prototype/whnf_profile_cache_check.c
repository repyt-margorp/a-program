#include "ast.h"
#include "term.h"
#include "type_declaration.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define MATCH_FRAME_CAPACITY 16
#define TYPE_CAPACITY 8
#define CONSTRUCTOR_CAPACITY 8
#define PARAMETER_CAPACITY 8
#define FIELD_TYPE_CAPACITY 8
#define TYPE_EXPR_CAPACITY 8

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];

static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		cases,
		case_label_symbols,
		CASE_CAPACITY,
		case_binders,
		CASE_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db,
		type_declarations,
		TYPE_CAPACITY,
		constructor_declarations,
		CONSTRUCTOR_CAPACITY,
		parameter_declarations,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);

	uint32_t type_id;
	uint32_t owner;
	uint32_t constructor;
	uint32_t argument;
	uint32_t lambda;
	uint32_t application;
	uint32_t branch;
	if (prototype_type_declaration_add(&type_db, 1, &type_id) != 0 ||
		prototype_term_type_instance_make(
			&term_db, &type_db, type_id, NULL, 0, &owner
		) != 0 ||
		prototype_term_constructor(&term_db, owner, 0, &constructor) != 0 ||
		prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 2 },
			&argument
		) != 0 ||
		prototype_term_lambda(&term_db, prototype_term_fresh_binder(&term_db), constructor, &lambda) != 0 ||
		prototype_term_app(&term_db, lambda, argument, &application) != 0 ||
		prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 3 },
			&branch
		) != 0) {
		return 1;
	}

	struct prototype_match_case_input match_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = PROTOTYPE_INVALID_ID,
		.constructor_id = 0,
		.binders = NULL,
		.binder_count = 0,
		.body = branch
	};
	uint32_t match_term;
	if (prototype_term_match(&term_db, application, &match_case, 1, &match_term) != 0) {
		return 1;
	}
	uint32_t neutral_scrutinee;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 4 },
			&neutral_scrutinee
		) != 0) {
		return 1;
	}
	struct prototype_match_case_input uniform_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = PROTOTYPE_INVALID_ID,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = branch
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = PROTOTYPE_INVALID_ID,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = branch
		}
	};
	uint32_t neutral_uniform_match;
	if (prototype_term_match(
			&term_db, neutral_scrutinee, uniform_cases, 2, &neutral_uniform_match
		) != 0) {
		return 1;
	}
	uint32_t kernel_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			neutral_uniform_match,
			&kernel_whnf
		) != 0 || kernel_whnf != neutral_uniform_match) {
		return 1;
	}

	/* Core WHNF observes only beta reduction. CBPV cut elimination is a
	 * separate computation-layer rule, even though it reuses the same TermDB. */
	uint32_t returned_constructor;
	uint32_t thunked_constructor;
	uint32_t forced_constructor;
	uint32_t bind_binder;
	uint32_t bind_var;
	uint32_t bind_body;
	uint32_t bind_continuation;
	uint32_t bound_constructor;
	uint32_t thunked_bound_constructor;
	uint32_t forced_bound_constructor;
	uint32_t core_cbpv_whnf;
	uint32_t computation_cbpv_whnf;
	if (prototype_term_return(&term_db, constructor, &returned_constructor) != 0 ||
		prototype_term_thunk(&term_db, returned_constructor, &thunked_constructor) != 0 ||
		prototype_term_force(&term_db, thunked_constructor, &forced_constructor) != 0 ||
		(bind_binder = prototype_term_fresh_binder(&term_db)) == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&term_db, bind_binder, &bind_var) != 0 ||
		prototype_term_return(&term_db, bind_var, &bind_body) != 0 ||
		prototype_term_lambda(&term_db, bind_binder, bind_body, &bind_continuation) != 0 ||
		prototype_term_deep_fold(
			&term_db, returned_constructor, bind_continuation, NULL, 0,
			&bound_constructor
		) != 0 || prototype_term_thunk(
			&term_db, bound_constructor, &thunked_bound_constructor
		) != 0 || prototype_term_force(
			&term_db, thunked_bound_constructor, &forced_bound_constructor
		) != 0 || prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			forced_constructor,
			&core_cbpv_whnf
		) != 0 || core_cbpv_whnf != forced_constructor ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			forced_constructor,
			&computation_cbpv_whnf
		) != 0 || computation_cbpv_whnf != returned_constructor ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			forced_constructor,
			&computation_cbpv_whnf
		) != 0 || computation_cbpv_whnf != returned_constructor ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			bound_constructor,
			&core_cbpv_whnf
		) != 0 || core_cbpv_whnf != bound_constructor ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			bound_constructor,
			&computation_cbpv_whnf
		) != 0 || computation_cbpv_whnf != returned_constructor ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			bound_constructor,
			&computation_cbpv_whnf
		) != 0 || computation_cbpv_whnf != returned_constructor) {
		return 1;
	}
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			forced_bound_constructor,
			&computation_cbpv_whnf
		) != 0 || computation_cbpv_whnf != returned_constructor) {
		return 1;
	}

	prototype_term_normalization_cache_clear(&term_db);
	uint32_t lambda_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			match_term,
			&lambda_whnf
		) != 0 ||
		lambda_whnf != match_term) {
		return 1;
	}
	uint32_t cached_lambda_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			match_term,
			&cached_lambda_whnf
		) != 0 ||
		cached_lambda_whnf != match_term) {
		return 1;
	}
	struct prototype_term_normalization_cache_stats stats;
	prototype_term_normalization_cache_get_stats(&term_db, &stats);
	if (stats.hit_count == 0) {
		return 1;
	}

	uint32_t unresolved_inductive_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			match_term,
			&unresolved_inductive_whnf
		) != 0 ||
		unresolved_inductive_whnf != match_term) {
		return 1;
	}

	if (prototype_term_resolve_match_case(&term_db, match_term, 0, owner, 0) != 0) {
		return 1;
	}
	uint32_t inductive_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			match_term,
			&inductive_whnf
		) != 0 ||
		inductive_whnf != branch) {
		return 1;
	}
	uint32_t cached_inductive_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			match_term,
			&cached_inductive_whnf
		) != 0 ||
		cached_inductive_whnf != branch) {
		return 1;
	}
	prototype_term_normalization_cache_get_stats(&term_db, &stats);
	if (stats.hit_count < 2) {
		return 1;
	}

	/* Artifact linking mutates graph slots directly; its notification must make
	 * the cached WHNF of this match unusable. */
	term_db.cases[term_db.terms[match_term].as.match.first_case].body = application;
	prototype_term_notify_graph_mutation(&term_db);
	uint32_t mutated_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			match_term,
			&mutated_whnf
		) != 0 ||
		mutated_whnf != constructor) {
		return 1;
	}

	/* Residual verification needs to distinguish an effect boundary from an
	 * invalid graph or a pure normalization budget stop. */
	uint32_t continuation_binder = prototype_term_fresh_binder(&term_db);
	uint32_t continuation_var;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t effect_operation;
	uint32_t effect_request;
	if (continuation_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&term_db, continuation_binder, &continuation_var) != 0 ||
		prototype_term_lambda(
			&term_db, continuation_binder, continuation_var, &continuation_lambda
		) != 0 || prototype_term_thunk(&term_db, continuation_lambda, &continuation_thunk) != 0 ||
		prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 5 },
			&effect_operation
		) != 0 || prototype_term_operation_request(
			&term_db, effect_operation, constructor, continuation_thunk, &effect_request
		) != 0) {
		return 1;
	}
	struct prototype_term_normalization_result normalization_result;
	if (prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				effect_request,
				PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&normalization_result
			) != 0 || normalization_result.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT ||
		normalization_result.term_id != effect_request ||
		normalization_result.step_limit == 0 ||
		normalization_result.steps_used == 0 ||
		normalization_result.steps_used > normalization_result.step_limit) {
		return 1;
	}
	if (prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			application,
			0,
			&normalization_result
		) != 0 || normalization_result.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED ||
		normalization_result.step_limit != 0 ||
		normalization_result.steps_used != 0) {
		return 1;
	}
	if (prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			application,
			1,
			&normalization_result
			) != 0 || normalization_result.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED ||
		normalization_result.step_limit != 1 ||
		normalization_result.steps_used != 1) {
		return 1;
	}

	/* The runtime side of a residual deep fold receives an occurrence-local value.
	 * It instantiates the recorded family without placing that value in TermDB
	 * as a global RESULT_OF node. */
	uint32_t effect_row;
	uint32_t residual_classifier;
	uint32_t residual_binder = prototype_term_fresh_binder(&term_db);
	uint32_t residual_family;
	struct prototype_verification_obligation obligations[1];
	struct prototype_verification_db verification;
	if (residual_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_label(&term_db, PROTOTYPE_HOST_EFFECT_NONE, &effect_row) != 0 ||
		prototype_term_computation_type(&term_db, effect_row, owner, &residual_classifier) != 0 ||
		prototype_term_pure_family(
			&term_db, residual_binder, residual_classifier, &residual_family
		) != 0) {
		return 1;
	}
	prototype_verification_db_init(&verification, obligations, 1);
	if (prototype_verification_db_add(
			&verification,
			(struct prototype_verification_obligation){
				.kind = PROTOTYPE_VERIFICATION_OBLIGATION_DEEP_FOLD_RESULT,
				.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
				.operation = 0,
				.core_term = bound_constructor,
				.computation_operation = 0,
				.continuation_operation = 0,
				.continuation_binder_id = residual_binder,
				.input_classifier = owner,
				.classifier_family = residual_family,
				.effect_row = effect_row,
				.normalization_profile =
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
			},
			NULL
		) != 0 || prototype_verification_db_discharge_dependent_bind(
			&verification,
			&term_db,
			&type_db,
			0,
			constructor,
			residual_classifier
		) != 0 || !prototype_verification_db_get(&verification, 0) ||
			prototype_verification_db_get(&verification, 0)->state !=
			PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
		return 1;
	}
	return 0;
}
