#include "ast.h"

#include <stdint.h>
#include <string.h>

#define TERM_CAPACITY 256
#define CLAIM_CAPACITY 32
#define CONTEXT_CAPACITY 16
#define SUBSTITUTION_CAPACITY 32
#define OPERATION_CAPACITY 16
#define GOAL_CAPACITY 16

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[16];
static int case_labels[16];
static struct prototype_case_binder case_binders[16];
static struct prototype_ih_scope ih_scopes[16];
static struct prototype_type_declaration type_declarations[8];
static struct prototype_type_constructor_declaration constructors[16];
static struct prototype_type_parameter_declaration parameters[8];
static uint32_t field_types[16];
static struct prototype_type_expr type_exprs[16];
static struct prototype_context contexts[CONTEXT_CAPACITY];
static struct prototype_substitution substitutions[SUBSTITUTION_CAPACITY];
static struct prototype_judgement_claim_candidate claim_candidates[CLAIM_CAPACITY];
static struct prototype_judgement_derivation_candidate derivation_candidates[CLAIM_CAPACITY];
static struct prototype_judgement_claim claims[CLAIM_CAPACITY];
static struct prototype_judgement_derivation derivations[CLAIM_CAPACITY];
static struct prototype_substitution_certificate certificates[16];
static struct prototype_operation_node operations[OPERATION_CAPACITY];
static struct prototype_hott_bridge bridges[8];
static struct prototype_hott_goal goals[GOAL_CAPACITY];
static struct prototype_hott_residual_obligation residuals[8];

static uint32_t add_claim(
	struct prototype_judgement_db* judgement,
	int kind,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t operation_id,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t id = (uint32_t)judgement->claim_count++;
	judgement->claims[id] = (struct prototype_judgement_claim){
		.kind = kind,
		.authority_kind = authority_kind,
		.authority_id = authority_id,
		.context_id = context_id,
		.operation_id = operation_id,
		.subject = subject,
		.classifier = classifier,
		.closure_rank = 0
	};
	return id;
}

static uint32_t add_operation(
	struct prototype_operation_graph* graph,
	uint32_t context_id,
	uint32_t core,
	uint32_t classifier,
	int tag,
	int polarity
) {
	uint32_t id = (uint32_t)graph->operation_count++;
	graph->operations[id] = (struct prototype_operation_node){
		.tag = tag,
		.polarity = polarity,
		.context_id = context_id,
		.core_term = core,
		.known_classifier = classifier,
		.classifier = classifier,
		.classifier_variable = PROTOTYPE_INVALID_ID,
		.source_ast = PROTOTYPE_INVALID_ID,
		.referenced_ast_binder_id = PROTOTYPE_INVALID_ID,
		.binding_id = PROTOTYPE_INVALID_ID,
		.function = PROTOTYPE_INVALID_ID,
		.argument = PROTOTYPE_INVALID_ID,
		.body = PROTOTYPE_INVALID_ID,
		.scrutinee = PROTOTYPE_INVALID_ID,
		.binder_classifier = PROTOTYPE_INVALID_ID,
		.fold_return_operation = PROTOTYPE_INVALID_ID
	};
	return id;
}

static struct prototype_hott_goal observation_goal(
	uint32_t id,
	uint32_t context_id,
	uint32_t carrier,
	uint32_t left,
	uint32_t right,
	uint32_t carrier_claim,
	uint32_t left_claim,
	uint32_t right_claim,
	uint32_t left_operation,
	uint32_t right_operation,
	uint32_t bridge,
	uint64_t graph_revision
) {
	struct prototype_hott_goal goal;
	memset(&goal, 0, sizeof(goal));
	goal.id = id;
	goal.variant = PROTOTYPE_HOTT_GOAL_VARIANT_OBSERVATION;
	goal.kind = PROTOTYPE_HOTT_GOAL_VALUE_OBSERVATION;
	goal.state = PROTOTYPE_HOTT_GOAL_PENDING;
	goal.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	goal.source_ast = PROTOTYPE_INVALID_ID;
	goal.parent_goal_id = PROTOTYPE_INVALID_ID;
	goal.parent_role = PROTOTYPE_HOTT_CHILD_NONE;
	goal.parent_index = PROTOTYPE_INVALID_ID;
	goal.rule = PROTOTYPE_HOTT_RULE_OBS_CONVERT;
	goal.as.observation = (struct prototype_hott_observation_goal){
		.context_id = context_id,
		.carrier_classifier = carrier,
		.left_endpoint = left,
		.right_endpoint = right,
		.carrier_claim_id = carrier_claim,
		.left_claim_id = left_claim,
		.right_claim_id = right_claim,
		.left_operation_id = left_operation,
		.right_operation_id = right_operation,
		.bridge_id = bridge
	};
	goal.witness_term = PROTOTYPE_INVALID_ID;
	goal.witness_claim_id = PROTOTYPE_INVALID_ID;
	goal.conversion_request = (struct prototype_kernel_conversion_goal){
		.id = id,
		.context_id = context_id,
		.carrier_classifier = carrier,
		.left_term = left,
		.right_term = right,
		.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		.step_limit = 32,
		.result = { .status = PROTOTYPE_TERM_CONVERSION_INVALID }
	};
	goal.conversion_graph_revision = graph_revision;
	return goal;
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_context_db context_db;
	struct prototype_substitution_db substitution_db;
	struct prototype_judgement_db judgement;
	struct prototype_substitution_certificate_db certificate_db;
	struct prototype_operation_graph operation_graph;
	struct prototype_hott_bridge_db bridge_db;
	struct prototype_hott_goal_db goal_db;
	struct prototype_hott_residual_db residual_db;

	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 16,
		case_binders, 16, ih_scopes, 16
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, 8, constructors, 16, parameters, 8,
		field_types, 16, type_exprs, 16
	);
	prototype_context_db_init(&context_db, contexts, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitution_db, substitutions, SUBSTITUTION_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement, claim_candidates, derivation_candidates, claims,
		derivations, CLAIM_CAPACITY
	);
	prototype_substitution_certificate_db_init(
		&certificate_db, certificates, 16
	);
	memset(&operation_graph, 0, sizeof(operation_graph));
	operation_graph.operations = operations;
	operation_graph.operation_capacity = OPERATION_CAPACITY;
	prototype_hott_bridge_db_init(&bridge_db, bridges, 8);
	prototype_hott_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
	prototype_hott_residual_db_init(&residual_db, residuals, 8);

	uint32_t empty = prototype_context_empty(&context_db);
	uint32_t universe;
	uint32_t text;
	uint32_t int_type;
	uint32_t left_text;
	uint32_t right_text;
	if (prototype_term_universe_var(&term_db, 0, &universe) != 0 ||
		prototype_term_primitive_text(&term_db, &text) != 0 ||
		prototype_term_primitive_int(&term_db, &int_type) != 0 ||
		prototype_term_text_literal(&term_db, 10, &left_text) != 0 ||
		prototype_term_text_literal(&term_db, 11, &right_text) != 0) {
		return 1;
	}

	uint32_t left_operation = add_operation(
		&operation_graph, empty, left_text, text,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t right_operation = add_operation(
		&operation_graph, empty, right_text, text,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t sibling_operation = add_operation(
		&operation_graph, empty, left_text, text,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t text_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, text, empty,
		PROTOTYPE_INVALID_ID, text, universe
	);
	uint32_t left_has_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, left_operation, empty,
		left_operation, left_text, text
	);
	uint32_t right_has_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, right_operation, empty,
		right_operation, right_text, text
	);
	uint32_t sibling_has_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, sibling_operation, empty,
		sibling_operation, left_text, text
	);
	judgement.derivations[0] = (struct prototype_judgement_derivation){
		.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO,
		.conclusion_claim_id = left_has_type
	};
	judgement.derivations[1] = (struct prototype_judgement_derivation){
		.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
		.conclusion_claim_id = left_has_type
	};
	judgement.derivation_count = 2;
	uint32_t derivation_ids[2];
	size_t derivation_count;
	if (prototype_judgement_claim_derivations(
			&judgement, left_has_type, derivation_ids, 2, &derivation_count
		) != 0 || derivation_count != 2 || derivation_ids[0] == derivation_ids[1]) {
		return 2;
	}

	uint32_t bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &context_db, &substitution_db, &certificate_db,
			&judgement, empty, &bridge
		) != 0 || prototype_hott_bridge_db_validate(
			&bridge_db, &context_db, &substitution_db, &certificate_db,
			&judgement
		) != 0) {
		return 3;
	}

	struct prototype_hott_goal goal = observation_goal(
		0, empty, text, left_text, right_text, text_is_type, left_has_type,
		right_has_type, left_operation, right_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &goal
		) != 0 || goal.state != PROTOTYPE_HOTT_GOAL_PENDING ||
		goal.rule != PROTOTYPE_HOTT_RULE_OBS_CONVERT) {
		return 4;
	}
	uint32_t goal_id;
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			goal, &goal_id
		) != 0 || goal_id != 0 || prototype_hott_goal_db_validate(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement
		) != 0) {
		return 5;
	}

	struct prototype_hott_goal forged = observation_goal(
		1, empty, text, left_text, right_text, text_is_type, sibling_has_type,
		right_has_type, left_operation, right_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			forged, &goal_id
		) == 0) {
		return 6;
	}
	forged = observation_goal(
		1, empty, text, text, right_text, text_is_type, left_has_type,
		right_has_type, left_operation, right_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			forged, &goal_id
		) == 0) {
		return 7;
	}
	forged = goal;
	forged.id = 1;
	forged.conversion_request.id = 1;
	forged.state = PROTOTYPE_HOTT_GOAL_SOLVED;
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			forged, &goal_id
		) == 0) {
		return 8;
	}
	forged = goal;
	forged.id = 1;
	forged.conversion_request.id = 1;
	forged.kind = PROTOTYPE_HOTT_GOAL_COMPUTATION_OBSERVATION;
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			forged, &goal_id
		) == 0) {
		return 22;
	}

	struct prototype_term_conversion_result conversion = {
		.status = PROTOTYPE_TERM_CONVERSION_NOT_EQUAL,
		.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE,
		.profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		.left = left_text,
		.right = right_text,
		.left_observation = left_text,
		.right_observation = right_text,
		.step_limit = 32,
		.steps_used = 1,
		.graph_revision = term_db.normalization_graph_revision
	};
	struct prototype_hott_goal converted = goal;
	if (prototype_hott_goal_apply_conversion_result(&converted, conversion) != 0 ||
		converted.state != PROTOTYPE_HOTT_GOAL_PENDING) {
		return 9;
	}
	conversion.left = right_text;
	if (prototype_hott_goal_apply_conversion_result(&converted, conversion) == 0 ||
		converted.conversion_request.result.left != left_text) {
		return 10;
	}
	struct prototype_hott_goal diagonal = observation_goal(
		1, empty, text, left_text, left_text, text_is_type, left_has_type,
		left_has_type, left_operation, left_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &diagonal
		) != 0 || diagonal.rule != PROTOTYPE_HOTT_RULE_OBS_DIAGONAL ||
		prototype_hott_goal_execute_conversion(
			&context_db, &term_db, &type_db, NULL, &diagonal
		) != 0 ||
		diagonal.conversion_request.result.status !=
			PROTOTYPE_TERM_CONVERSION_EQUAL ||
		diagonal.state != PROTOTYPE_HOTT_GOAL_PENDING) {
		return 30;
	}
	uint32_t forged_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, text, empty,
		PROTOTYPE_INVALID_ID, text, text
	);
	forged = observation_goal(
		1, empty, text, left_text, right_text, forged_is_type, left_has_type,
		right_has_type, left_operation, right_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &forged
		) == 0) {
		return 31;
	}

	uint32_t extended;
	uint32_t binder = prototype_term_new_binding(&term_db);
	if (binder == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, empty, binder, text, PROTOTYPE_INVALID_ID, &extended
		) != 0) {
		return 11;
	}
	uint32_t nonempty_bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &context_db, &substitution_db, &certificate_db,
			&judgement, extended, &nonempty_bridge
		) != 1) {
		return 12;
	}
	uint32_t projection;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, extended, &projection
		) != 0) {
		return 23;
	}
	struct prototype_hott_bridge forged_bridge_storage[1] = {{
		.id = 0,
		.source_context_id = empty,
		.bridge_context_id = extended,
		.left_substitution_id = projection,
		.right_substitution_id = projection,
		.left_certificate_id = PROTOTYPE_INVALID_ID,
		.right_certificate_id = PROTOTYPE_INVALID_ID
	}};
	struct prototype_hott_bridge_db forged_bridge_db = {
		.bridges = forged_bridge_storage,
		.bridge_count = 1,
		.bridge_capacity = 1
	};
	if (prototype_hott_bridge_db_validate(
			&forged_bridge_db, &context_db, &substitution_db, &certificate_db,
			&judgement
		) == 0) {
		return 24;
	}

	uint32_t empty_identity;
	uint32_t extension_substitution;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, empty, &empty_identity
		) != 0 || prototype_substitution_extend(
			&substitution_db, &context_db, &term_db, &type_db, empty_identity,
			extended, left_text, text, &extension_substitution
		) != 0) {
		return 13;
	}
	uint32_t certificate;
	if (prototype_substitution_certificate_db_add(
			&certificate_db, &substitution_db, &judgement,
			extension_substitution, left_has_type, &certificate
		) != 0 || prototype_substitution_certificate_db_add(
			&certificate_db, &substitution_db, &judgement,
			extension_substitution, right_has_type, &certificate
		) == 0 || prototype_substitution_certificate_db_validate(
			&certificate_db, &substitution_db, &judgement
		) != 0) {
		return 14;
	}
	uint32_t saved_classifier =
		substitution_db.substitutions[extension_substitution].term_classifier;
	substitution_db.substitutions[extension_substitution].term_classifier = int_type;
	if (prototype_substitution_db_validate_typed(
			&substitution_db, &context_db, &term_db, &type_db
		) == 0) {
		return 15;
	}
	substitution_db.substitutions[extension_substitution].term_classifier =
		saved_classifier;

	uint32_t empty_row;
	uint32_t effect_row;
	uint32_t pure_comp;
	uint32_t effect_comp;
	uint32_t returned_left;
	uint32_t returned_right;
	if (prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_row
		) != 0 || prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_PRINT, &effect_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_row, text, &pure_comp
		) != 0 || prototype_term_computation_type(
			&term_db, effect_row, text, &effect_comp
		) != 0 || prototype_term_return(
			&term_db, left_text, &returned_left
		) != 0 || prototype_term_return(
			&term_db, right_text, &returned_right
		) != 0) {
		return 16;
	}
	uint32_t pure_comp_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_comp, empty,
		PROTOTYPE_INVALID_ID, pure_comp, universe
	);
	uint32_t returned_left_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, pure_comp
	);
	uint32_t returned_right_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, pure_comp
	);
	struct prototype_hott_goal computation_goal = observation_goal(
		1, empty, pure_comp, returned_left, returned_right, pure_comp_is_type,
		returned_left_claim, returned_right_claim, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, bridge, term_db.normalization_graph_revision
	);
	computation_goal.kind = PROTOTYPE_HOTT_GOAL_COMPUTATION_OBSERVATION;
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement,
			&computation_goal
		) != 0 || computation_goal.rule != PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN) {
		return 17;
	}
	uint32_t effect_comp_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_comp, empty,
		PROTOTYPE_INVALID_ID, effect_comp, universe
	);
	uint32_t effect_left_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, effect_comp
	);
	uint32_t effect_right_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, effect_comp
	);
	struct prototype_hott_goal effect_goal = observation_goal(
		1, empty, effect_comp, returned_left, returned_right,
		effect_comp_is_type, effect_left_claim, effect_right_claim,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, bridge,
		term_db.normalization_graph_revision
	);
	effect_goal.kind = PROTOTYPE_HOTT_GOAL_COMPUTATION_OBSERVATION;
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &effect_goal
		) != 0 || effect_goal.state != PROTOTYPE_HOTT_GOAL_RESIDUAL ||
		effect_goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 18;
	}
	uint32_t effect_goal_id;
	if (prototype_hott_goal_db_add(
			&goal_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			effect_goal, &effect_goal_id
		) != 0 || prototype_hott_residual_db_add_from_goal(
			&residual_db, &goal_db, &bridge_db,
			prototype_hott_goal_db_get(&goal_db, effect_goal_id), &goal_id
		) != 0 || prototype_hott_residual_db_validate(
			&residual_db, &goal_db, &bridge_db
		) != 0 || prototype_hott_residual_db_require_artifact_empty(
			&residual_db
		) == 0) {
		return 19;
	}

	uint32_t continuation_binder = prototype_term_new_binding(&term_db);
	uint32_t continuation_variable;
	uint32_t continuation_return;
	uint32_t continuation_lambda;
	uint32_t suspended_continuation;
	uint32_t print_operation;
	uint32_t request;
	uint32_t suspended_request;
	uint32_t suspended_request_type;
	if (continuation_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(
			&term_db, continuation_binder, &continuation_variable
		) != 0 || prototype_term_return(
			&term_db, continuation_variable, &continuation_return
		) != 0 || prototype_term_lambda(
			&term_db, continuation_binder, continuation_return,
			&continuation_lambda
		) != 0 || prototype_term_thunk(
			&term_db, continuation_lambda, &suspended_continuation
		) != 0 || prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &print_operation
		) != 0 || prototype_term_operation_request(
			&term_db, print_operation, left_text, suspended_continuation,
			&request
		) != 0 || prototype_term_thunk(
			&term_db, request, &suspended_request
		) != 0 || prototype_term_thunk_type(
			&term_db, effect_comp, &suspended_request_type
		) != 0) {
		return 25;
	}
	uint32_t suspended_type_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, suspended_request_type,
		empty, PROTOTYPE_INVALID_ID, suspended_request_type, universe
	);
	uint32_t suspended_request_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, suspended_request, empty,
		PROTOTYPE_INVALID_ID, suspended_request, suspended_request_type
	);
	struct prototype_hott_goal hidden_request_goal = observation_goal(
		2, empty, suspended_request_type, suspended_request, suspended_request,
		suspended_type_is_type, suspended_request_claim, suspended_request_claim,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement,
			&hidden_request_goal
		) != 0 || hidden_request_goal.state != PROTOTYPE_HOTT_GOAL_UNSUPPORTED ||
		hidden_request_goal.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST) {
		return 26;
	}

	/* A TypeView is admitted nominally through its declaration telescope. */
	uint32_t bool_view = (uint32_t)term_db.term_count++;
	memset(&term_db.terms[bool_view], 0, sizeof(term_db.terms[bool_view]));
	term_db.terms[bool_view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	term_db.terms[bool_view].as.type_view.view_type_id = 0;
	term_db.terms[bool_view].as.type_view.identity.namespace_symbol_id = -1;
	term_db.terms[bool_view].as.type_view.identity.name_symbol_id = 100;
	term_db.terms[bool_view].as.type_view.core = text;
	term_db.terms[bool_view].as.type_view.source = text;
	type_db.type_count = 1;
	type_db.type_declarations[0] = (struct prototype_type_declaration){
		.name_symbol_id = 100,
		.namespace_symbol_id = -1,
		.type_index = 0,
		.representation_id = PROTOTYPE_INVALID_ID,
		.formation_classifier = universe,
		.parameter_context = empty,
		.parameter_count = 0,
		.first_constructor = 0,
		.constructor_count = 2
	};
	type_db.constructor_count = 2;
	for (uint32_t i = 0; i < 2; ++i) {
		type_db.constructor_declarations[i] =
			(struct prototype_type_constructor_declaration){
				.name_symbol_id = 101 + (int)i,
				.owner_type = 0,
				.constructor_index = i,
				.parameter_context = empty,
				.field_context = empty,
				.result_classifier = bool_view,
				.curried_classifier_cache = bool_view
			};
	}
	prototype_term_notify_graph_mutation(&term_db);
	uint32_t bool_false;
	uint32_t bool_true;
	if (prototype_term_constructor(&term_db, bool_view, 0, &bool_false) != 0 ||
		prototype_term_constructor(&term_db, bool_view, 1, &bool_true) != 0) {
		return 20;
	}
	uint32_t false_operation = add_operation(
		&operation_graph, empty, bool_false, bool_view,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_operation = add_operation(
		&operation_graph, empty, bool_true, bool_view,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t bool_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, bool_view, empty,
		PROTOTYPE_INVALID_ID, bool_view, universe
	);
	uint32_t false_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, false_operation, empty,
		false_operation, bool_false, bool_view
	);
	uint32_t true_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, true_operation, empty,
		true_operation, bool_true, bool_view
	);
	struct prototype_hott_goal bool_goal = observation_goal(
		2, empty, bool_view, bool_false, bool_true, bool_is_type, false_claim,
		true_claim, false_operation, true_operation, bridge,
		term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &bool_goal
		) != 0 || bool_goal.state != PROTOTYPE_HOTT_GOAL_CONTRADICTION ||
		bool_goal.rule != PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT) {
		return 21;
	}
	const struct prototype_type_constructor_declaration* bool_true_telescope;
	if (prototype_type_view_constructor_telescope_query(
			&type_db, &context_db, &term_db, bool_view, 1,
			&bool_true_telescope
		) != 0 || bool_true_telescope != &type_db.constructor_declarations[1]) {
		return 27;
	}

	uint32_t two_view = (uint32_t)term_db.term_count++;
	memset(&term_db.terms[two_view], 0, sizeof(term_db.terms[two_view]));
	term_db.terms[two_view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	term_db.terms[two_view].as.type_view.view_type_id = 1;
	term_db.terms[two_view].as.type_view.identity.namespace_symbol_id = -1;
	term_db.terms[two_view].as.type_view.identity.name_symbol_id = 200;
	term_db.terms[two_view].as.type_view.core = text;
	term_db.terms[two_view].as.type_view.source = text;
	type_db.type_count = 2;
	type_db.type_declarations[1] = (struct prototype_type_declaration){
		.name_symbol_id = 200,
		.namespace_symbol_id = -1,
		.type_index = 1,
		.representation_id = PROTOTYPE_INVALID_ID,
		.formation_classifier = universe,
		.parameter_context = empty,
		.parameter_count = 0,
		.first_constructor = 2,
		.constructor_count = 2
	};
	type_db.constructor_count = 4;
	for (uint32_t i = 0; i < 2; ++i) {
		type_db.constructor_declarations[2 + i] =
			(struct prototype_type_constructor_declaration){
				.name_symbol_id = 201 + (int)i,
				.owner_type = 1,
				.constructor_index = i,
				.parameter_context = empty,
				.field_context = empty,
				.result_classifier = two_view,
				.curried_classifier_cache = two_view
			};
	}
	prototype_term_notify_graph_mutation(&term_db);
	uint32_t universe_successor;
	if (prototype_term_universe_var(
			&term_db, 1, &universe_successor
		) != 0) {
		return 28;
	}
	uint32_t bool_type_operation = add_operation(
		&operation_graph, empty, bool_view, universe,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t two_type_operation = add_operation(
		&operation_graph, empty, two_view, universe,
		PROTOTYPE_OPERATION_ATOM, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t universe_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE, universe, empty,
		PROTOTYPE_INVALID_ID, universe, universe_successor
	);
	uint32_t bool_has_universe = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, bool_type_operation, empty,
		bool_type_operation, bool_view, universe
	);
	uint32_t two_has_universe = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, two_type_operation, empty,
		two_type_operation, two_view, universe
	);
	struct prototype_hott_goal universe_goal = observation_goal(
		2, empty, universe, bool_view, two_view, universe_is_type,
		bool_has_universe, two_has_universe, bool_type_operation,
		two_type_operation, bridge, term_db.normalization_graph_revision
	);
	if (prototype_hott_goal_classify_admission(
			&context_db, &substitution_db, &certificate_db, &bridge_db,
			&term_db, &type_db, &operation_graph, &judgement, &universe_goal
		) != 0 || universe_goal.state != PROTOTYPE_HOTT_GOAL_UNSUPPORTED ||
		universe_goal.residual_reason != PROTOTYPE_HOTT_RESIDUAL_UNIVERSE) {
		return 29;
	}

	struct prototype_hott_goal action_storage[1];
	struct prototype_hott_goal_db action_db;
	prototype_hott_goal_db_init(&action_db, action_storage, 1);
	struct prototype_hott_goal action;
	memset(&action, 0, sizeof(action));
	action.variant = PROTOTYPE_HOTT_GOAL_VARIANT_ACTION;
	action.kind = PROTOTYPE_HOTT_GOAL_TYPE_ACTION;
	action.state = PROTOTYPE_HOTT_GOAL_UNSUPPORTED;
	action.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
	action.source_ast = PROTOTYPE_INVALID_ID;
	action.parent_goal_id = PROTOTYPE_INVALID_ID;
	action.parent_role = PROTOTYPE_HOTT_CHILD_NONE;
	action.parent_index = PROTOTYPE_INVALID_ID;
	action.rule = PROTOTYPE_HOTT_RULE_OBS_MATCH_ACTION;
	action.as.action.context_id = empty;
	action.as.action.subject = text;
	action.as.action.subject_claim_id = text_is_type;
	action.witness_term = PROTOTYPE_INVALID_ID;
	action.witness_claim_id = PROTOTYPE_INVALID_ID;
	if (prototype_hott_goal_db_add(
			&action_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			action, &goal_id
		) != 0) {
		return 32;
	}
	action_db.goal_count = 0;
	action.kind = PROTOTYPE_HOTT_GOAL_TERM_ACTION;
	if (prototype_hott_goal_db_add(
			&action_db, &context_db, &substitution_db, &certificate_db,
			&bridge_db, &term_db, &type_db, &operation_graph, &judgement,
			action, &goal_id
		) == 0) {
		return 33;
	}

	return 0;
}
