#include "judgement.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 8
#define CASE_BINDER_CAPACITY 8
#define IH_SCOPE_CAPACITY 8
#define TYPE_CAPACITY 4
#define CONSTRUCTOR_CAPACITY 4
#define PARAMETER_CAPACITY 4
#define FIELD_TYPE_CAPACITY 4
#define TYPE_EXPR_CAPACITY 4
#define CONTEXT_CAPACITY 8
#define SUBSTITUTION_CAPACITY 16
#define GOAL_CAPACITY 16
#define RESIDUAL_CAPACITY 16

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_ih_scope ih_scopes[IH_SCOPE_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructors[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameters[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_context contexts[CONTEXT_CAPACITY];
static struct prototype_substitution substitutions[SUBSTITUTION_CAPACITY];
static struct prototype_hott_goal goals[GOAL_CAPACITY];
static struct prototype_hott_residual_obligation residuals[RESIDUAL_CAPACITY];

static struct prototype_hott_goal base_goal(
	uint32_t context,
	uint32_t carrier,
	uint32_t endpoint,
	uint32_t substitution
) {
	return (struct prototype_hott_goal){
		.kind = PROTOTYPE_HOTT_GOAL_VALUE_OBSERVATION,
		.state = PROTOTYPE_HOTT_GOAL_PENDING,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
		.source_ast = PROTOTYPE_INVALID_ID,
		.context_id = context,
		.carrier_classifier = carrier,
		.left_endpoint = endpoint,
		.right_endpoint = endpoint,
		.bridge_context_id = context,
		.left_endpoint_substitution = substitution,
		.right_endpoint_substitution = substitution,
		.parent_goal_id = PROTOTYPE_INVALID_ID,
		.local_type_former_rule =
			PROTOTYPE_HOTT_LOCAL_RULE_OBSERVATION_DIAGONAL,
		.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		.step_limit = 32
	};
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_context_db context_db;
	struct prototype_substitution_db substitution_db;
	struct prototype_hott_goal_db goal_db;
	struct prototype_hott_residual_db residual_db;
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		cases,
		case_label_symbols,
		CASE_CAPACITY,
		case_binders,
		CASE_BINDER_CAPACITY,
		ih_scopes,
		IH_SCOPE_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db,
		type_declarations,
		TYPE_CAPACITY,
		constructors,
		CONSTRUCTOR_CAPACITY,
		parameters,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_context_db_init(&context_db, contexts, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitution_db, substitutions, SUBSTITUTION_CAPACITY
	);
	prototype_hott_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
	prototype_hott_residual_db_init(
		&residual_db, residuals, RESIDUAL_CAPACITY
	);

	uint32_t empty_context = prototype_context_empty(&context_db);
	uint32_t identity_substitution;
	uint32_t text_type;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, empty_context, &identity_substitution
		) != 0 || prototype_term_primitive_text(&term_db, &text_type) != 0) {
		return 1;
	}

	struct prototype_hott_goal goal = base_goal(
		empty_context, text_type, text_type, identity_substitution
	);
	uint32_t first_goal;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_PENDING ||
		prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&first_goal
		) != 0 || first_goal != 0) {
		return 2;
	}

	size_t mark = prototype_hott_goal_db_mark(&goal_db);
	goal.parent_goal_id = first_goal;
	uint32_t higher_goal;
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) != 0 || higher_goal != 1 || prototype_hott_goal_db_validate(
			&goal_db, &context_db, &substitution_db, &term_db
		) != 0) {
		return 3;
	}
	prototype_hott_goal_db_rewind(&goal_db, mark);
	if (goal_db.goal_count != mark ||
		prototype_hott_goal_db_get(&goal_db, higher_goal) != NULL) {
		return 4;
	}

	goal.parent_goal_id = 7;
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) == 0) {
		return 5;
	}
	goal = base_goal(empty_context, text_type, text_type, PROTOTYPE_INVALID_ID);
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) == 0) {
		return 6;
	}
	goal = base_goal(
		empty_context, term_db.term_count, text_type, identity_substitution
	);
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) == 0) {
		return 7;
	}
	goal = base_goal(
		empty_context, text_type, term_db.term_count, identity_substitution
	);
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) == 0) {
		return 8;
	}
	uint32_t extended_context;
	uint32_t projection;
	uint32_t context_binding = prototype_term_new_binding(&term_db);
	if (context_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty_context,
			context_binding,
			text_type,
			PROTOTYPE_INVALID_ID,
			&extended_context
		) != 0 || prototype_substitution_projection(
			&substitution_db, &context_db, extended_context, &projection
		) != 0) {
		return 9;
	}
	goal = base_goal(extended_context, text_type, text_type, projection);
	goal.bridge_context_id = extended_context;
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&higher_goal
		) == 0) {
		return 10;
	}
	goals[0].parent_goal_id = 0;
	if (prototype_hott_goal_db_validate(
			&goal_db, &context_db, &substitution_db, &term_db
		) == 0) {
		return 11;
	}
	goals[0].parent_goal_id = PROTOTYPE_INVALID_ID;

	uint32_t empty_row;
	uint32_t terminal_row;
	uint32_t unresolved_row;
	uint32_t pure_computation;
	uint32_t effectful_computation;
	uint32_t unresolved_computation;
	if (prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_row
		) != 0 || prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_PRINT, &terminal_row
		) != 0 || prototype_term_effect_row_var(
			&term_db, prototype_term_new_binding(&term_db), &unresolved_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_row, text_type, &pure_computation
		) != 0 || prototype_term_computation_type(
			&term_db, terminal_row, text_type, &effectful_computation
		) != 0 || prototype_term_computation_type(
			&term_db, unresolved_row, text_type, &unresolved_computation
		) != 0) {
		return 12;
	}

	goal = base_goal(empty_context, pure_computation, text_type, identity_substitution);
	goal.kind = PROTOTYPE_HOTT_GOAL_COMPUTATION_OBSERVATION;
	goal.local_type_former_rule = PROTOTYPE_HOTT_LOCAL_RULE_PURE_COMPUTATION;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_PENDING) {
		return 13;
	}
	goal.carrier_classifier = effectful_computation;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_RESIDUAL ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 14;
	}
	uint32_t residual_goal;
	if (prototype_hott_goal_db_add(
			&goal_db,
			&context_db,
			&substitution_db,
			&term_db,
			goal,
			&residual_goal
		) != 0) {
		return 15;
	}
	uint32_t residual_id;
	if (prototype_hott_residual_db_add_from_goal(
			&residual_db,
			&context_db,
			&substitution_db,
			&term_db,
			prototype_hott_goal_db_get(&goal_db, residual_goal),
			&residual_id
		) != 0 || residual_id != 0 || prototype_hott_residual_db_validate(
			&residual_db, &context_db, &substitution_db, &term_db
		) != 0 || prototype_hott_residual_db_require_artifact_v62(&residual_db) == 0) {
		return 16;
	}

	goal.carrier_classifier = unresolved_computation;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_RESIDUAL ||
		goal.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED) {
		return 17;
	}

	uint32_t binder = prototype_term_new_binding(&term_db);
	uint32_t variable;
	uint32_t continuation;
	uint32_t suspended_continuation;
	uint32_t operation;
	uint32_t request;
	uint32_t returned;
	uint32_t fold;
	uint32_t primitive;
	if (binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&term_db, binder, &variable) != 0 ||
		prototype_term_lambda(&term_db, binder, variable, &continuation) != 0 ||
		prototype_term_thunk(
			&term_db, continuation, &suspended_continuation
		) != 0 || prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &operation
		) != 0 || prototype_term_operation_request(
			&term_db, operation, text_type, suspended_continuation, &request
		) != 0 || prototype_term_return(&term_db, text_type, &returned) != 0 ||
		prototype_term_computation_fold(
			&term_db, returned, continuation, NULL, 0, &fold
		) != 0 || prototype_term_pure_primitive(
			&term_db, PROTOTYPE_PURE_PRIMITIVE_INT_ADD, -1, &primitive
		) != 0) {
		return 18;
	}
	goal = base_goal(empty_context, text_type, request, identity_substitution);
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_UNSUPPORTED ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST) {
		return 19;
	}
	goal.left_endpoint = fold;
	goal.right_endpoint = fold;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_COMPUTATION_FOLD) {
		return 20;
	}
	goal.left_endpoint = primitive;
	goal.right_endpoint = primitive;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE) {
		return 21;
	}
	struct prototype_term_conversion_result conversion = {
		.status = PROTOTYPE_TERM_CONVERSION_RESIDUAL,
		.reason = PROTOTYPE_TERM_CONVERSION_REASON_NEUTRAL,
		.profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		.left = text_type,
		.right = text_type,
		.left_observation = text_type,
		.right_observation = text_type,
		.step_limit = 32,
		.steps_used = 32,
		.graph_revision = term_db.normalization_graph_revision
	};
	if (prototype_hott_goal_apply_conversion_result(&goal, conversion) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_RESIDUAL ||
		goal.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION) {
		return 22;
	}
	conversion.status = PROTOTYPE_TERM_CONVERSION_NOT_EQUAL;
	conversion.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
	if (prototype_hott_goal_apply_conversion_result(&goal, conversion) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_CONTRADICTION) {
		return 23;
	}
	conversion.status = PROTOTYPE_TERM_CONVERSION_EXHAUSTED;
	conversion.reason = PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT;
	if (prototype_hott_goal_apply_conversion_result(&goal, conversion) != 0 ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED) {
		return 24;
	}
	uint32_t type_view = (uint32_t)term_db.term_count++;
	terms[type_view] = (struct prototype_term){
		.tag = PROTOTYPE_TERM_TYPE_VIEW,
		.as.type_view = {
			.view_type_id = 0,
			.core = text_type,
			.source = text_type
		}
	};
	goal = base_goal(empty_context, type_view, text_type, identity_substitution);
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_UNSUPPORTED ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_TYPE_VIEW) {
		return 25;
	}
	goal = base_goal(empty_context, text_type, text_type, identity_substitution);
	goal.kind = PROTOTYPE_HOTT_GOAL_TYPE_ACTION;
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE) {
		return 26;
	}

	uint32_t universe;
	if (prototype_term_universe_var(&term_db, 0, &universe) != 0) {
		return 27;
	}
	goal = base_goal(empty_context, universe, text_type, identity_substitution);
	if (prototype_hott_goal_classify_admission(&term_db, &goal) != 0 ||
		goal.state != PROTOTYPE_HOTT_GOAL_UNSUPPORTED ||
		goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_UNIVERSE) {
		return 28;
	}

	struct prototype_hott_residual_db empty_residual_db;
	prototype_hott_residual_db_init(&empty_residual_db, residuals, RESIDUAL_CAPACITY);
	if (prototype_hott_residual_db_require_artifact_v62(&empty_residual_db) != 0) {
		return 29;
	}
	(void)type_db;
	return 0;
}
