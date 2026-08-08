#include "ast.h"
#include "calculus.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TERM_CAPACITY 384
#define CLAIM_CAPACITY 64
#define CONTEXT_CAPACITY 16
#define SUBSTITUTION_CAPACITY 32
#define OPERATION_CAPACITY 32
#define GOAL_CAPACITY 32
#define CANDIDATE_CAPACITY 64
#define PREMISE_CAPACITY 64

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[32];
static int case_labels[32];
static struct prototype_case_binder case_binders[32];
static struct prototype_ih_scope ih_scopes[32];
static struct prototype_type_declaration type_declarations[8];
static struct prototype_type_constructor_declaration constructors[16];
static struct prototype_type_parameter_declaration parameters[8];
static uint32_t field_types[32];
static struct prototype_type_expr type_exprs[32];
static struct prototype_context contexts[CONTEXT_CAPACITY];
static struct prototype_substitution substitutions[SUBSTITUTION_CAPACITY];
static struct prototype_judgement_claim_candidate claim_candidates[CLAIM_CAPACITY];
static struct prototype_judgement_derivation_candidate derivation_candidates[CLAIM_CAPACITY];
static struct prototype_judgement_claim claims[CLAIM_CAPACITY];
static struct prototype_judgement_derivation derivations[CLAIM_CAPACITY];
static struct prototype_substitution_certificate substitution_certificates[16];
static struct prototype_context_formation_certificate context_certificates[16];
static struct prototype_operation_node operations[OPERATION_CAPACITY];
static struct prototype_hott_bridge bridges[8];
static struct prototype_hott_observation_goal goals[GOAL_CAPACITY];
static struct prototype_hott_candidate candidates[CANDIDATE_CAPACITY];
static struct prototype_hott_claim_premise claim_premises[PREMISE_CAPACITY];
static struct prototype_hott_child_edge child_edges[PREMISE_CAPACITY];
static struct prototype_hott_conversion_premise conversion_premises[PREMISE_CAPACITY];
static struct prototype_hott_context_certificate_premise
	context_certificate_premises[PREMISE_CAPACITY];
static struct prototype_hott_substitution_certificate_premise
	substitution_certificate_premises[PREMISE_CAPACITY];
static struct prototype_hott_work_item work_items[GOAL_CAPACITY];
static struct prototype_hott_residual_obligation residuals[16];
static struct prototype_hott_action actions[16];

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
		.classifier = classifier
	};
	return id;
}

static uint32_t add_operation(
	struct prototype_operation_graph* graph,
	uint32_t context_id,
	uint32_t core,
	uint32_t classifier,
	int polarity
) {
	uint32_t id = (uint32_t)graph->operation_count++;
	graph->operations[id] = (struct prototype_operation_node){
		.tag = PROTOTYPE_OPERATION_ATOM,
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

static uint32_t count_rule(
	const struct prototype_hott_candidate_db* db,
	uint32_t goal_id,
	int rule
) {
	uint32_t count = 0;
	for (uint32_t i = 0; i < db->candidate_count; ++i) {
		if (db->candidates[i].conclusion_goal_id == goal_id &&
			db->candidates[i].rule == rule) {
			++count;
		}
	}
	return count;
}

static uint32_t find_rule(
	const struct prototype_hott_candidate_db* db,
	uint32_t goal_id,
	int rule
) {
	for (uint32_t i = 0; i < db->candidate_count; ++i) {
		if (db->candidates[i].conclusion_goal_id == goal_id &&
			db->candidates[i].rule == rule) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t add_bool_view(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* types_db,
	uint32_t empty,
	uint32_t universe,
	uint32_t core
) {
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = 0;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 100;
	terms_db->terms[view].as.type_view.core = core;
	terms_db->terms[view].as.type_view.source = core;
	types_db->type_count = 1;
	types_db->type_declarations[0] = (struct prototype_type_declaration){
		.name_symbol_id = 100,
		.namespace_symbol_id = -1,
		.type_index = 0,
		.representation_id = PROTOTYPE_INVALID_ID,
		.formation_classifier = universe,
		.parameter_context = empty,
		.first_constructor = 0,
		.constructor_count = 2
	};
	types_db->constructor_count = 2;
	for (uint32_t i = 0; i < 2; ++i) {
		types_db->constructor_declarations[i] =
			(struct prototype_type_constructor_declaration){
				.name_symbol_id = 101 + (int)i,
				.owner_type = 0,
				.constructor_index = i,
				.parameter_context = empty,
				.field_context = empty,
				.result_classifier = view,
				.curried_classifier_cache = view
			};
	}
	prototype_term_notify_graph_mutation(terms_db);
	return view;
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_context_db context_db;
	struct prototype_substitution_db substitution_db;
	struct prototype_judgement_db judgement;
	struct prototype_substitution_certificate_db substitution_certificate_db;
	struct prototype_context_formation_certificate_db context_certificate_db;
	struct prototype_operation_graph operation_graph;
	struct prototype_hott_bridge_db bridge_db;
	struct prototype_hott_observation_goal_db goal_db;
	struct prototype_hott_candidate_db candidate_db;
	struct prototype_hott_work_db work_db;
	struct prototype_hott_residual_db residual_db;
	struct prototype_hott_action_db action_db;

	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 32,
		case_binders, 32, ih_scopes, 32
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, 8, constructors, 16, parameters, 8,
		field_types, 32, type_exprs, 32
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
		&substitution_certificate_db, substitution_certificates, 16
	);
	prototype_context_formation_certificate_db_init(
		&context_certificate_db, context_certificates, 16
	);
	memset(&operation_graph, 0, sizeof(operation_graph));
	operation_graph.operations = operations;
	operation_graph.operation_capacity = OPERATION_CAPACITY;
	prototype_hott_bridge_db_init(&bridge_db, bridges, 8);
	prototype_hott_observation_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
	prototype_hott_candidate_db_init(
		&candidate_db, candidates, CANDIDATE_CAPACITY,
		claim_premises, PREMISE_CAPACITY, child_edges, PREMISE_CAPACITY,
		conversion_premises, PREMISE_CAPACITY,
		context_certificate_premises, PREMISE_CAPACITY,
		substitution_certificate_premises, PREMISE_CAPACITY
	);
	prototype_hott_work_db_init(&work_db, work_items, GOAL_CAPACITY);
	prototype_hott_residual_db_init(&residual_db, residuals, 16);
	prototype_hott_action_db_init(&action_db, actions, 16);

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
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t right_operation = add_operation(
		&operation_graph, empty, right_text, text,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t sibling_operation = add_operation(
		&operation_graph, empty, left_text, text,
		PROTOTYPE_OPERATION_POLARITY_VALUE
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
			&bridge_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &term_db, &type_db, &judgement,
			empty, &bridge
		) != 0 || prototype_hott_bridge_db_validate(
			&bridge_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &term_db, &type_db, &judgement
		) != 0) {
		return 3;
	}

	uint32_t text_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, right_has_type, bridge, &text_goal
		) != 0 || text_goal != 0 ||
		prototype_hott_observation_goal_db_validate(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement
		) != 0) {
		return 4;
	}
	uint32_t interned;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, right_has_type, bridge, &interned
		) != 0 || interned != text_goal || goal_db.goal_count != 1) {
		return 5;
	}
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, sibling_has_type, right_has_type, bridge, &interned
		) != 0 || interned == text_goal) {
		return 6;
	}

	uint32_t work_id;
	if (prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, text_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ||
		strcmp(
			work_items[work_id].calculus_fingerprint,
			PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
		) != 0) {
		return 7;
	}
	uint32_t residual_id;
	if (prototype_hott_residual_db_add_from_work(
			&residual_db, &work_db, work_id, &residual_id
		) != 0 || prototype_hott_residual_db_validate(
			&residual_db, &work_db
		) != 0 || prototype_hott_residual_db_require_artifact_empty(
			&residual_db
		) == 0) {
		return 8;
	}

	uint32_t diagonal_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, left_has_type, bridge, &diagonal_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, diagonal_goal, PROTOTYPE_HOTT_RULE_OBS_DIAGONAL
		) != 1) {
		return 9;
	}
	uint32_t identity_binder = prototype_term_new_binding(&term_db);
	uint32_t identity_variable;
	uint32_t identity_lambda;
	uint32_t beta_text;
	if (identity_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, identity_binder, &identity_variable
		) != 0 || prototype_term_lambda(
			&term_db, identity_binder, identity_variable, &identity_lambda
		) != 0 || prototype_term_app(
			&term_db, identity_lambda, left_text, &beta_text
		) != 0) {
		return 39;
	}
	uint32_t beta_operation = add_operation(
		&operation_graph, empty, beta_text, text,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t beta_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, beta_operation, empty,
		beta_operation, beta_text, text
	);
	uint32_t conversion_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, beta_claim, left_has_type, bridge, &conversion_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db, NULL, &operation_graph, &judgement,
			conversion_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_OBS_CONVERT
		) != 1) {
		return 40;
	}

	uint32_t binder = prototype_term_new_binding(&term_db);
	uint32_t extended;
	if (binder == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, empty, binder, text, PROTOTYPE_INVALID_ID, &extended
		) != 0) {
		return 10;
	}
	uint32_t context_certificate;
	if (prototype_context_formation_certificate_db_add(
			&context_certificate_db, &context_db, &term_db, &type_db, &judgement,
			extended, text_is_type, &context_certificate
		) != 0 || prototype_context_formation_certificate_db_validate(
			&context_certificate_db, &context_db, &term_db, &type_db, &judgement
		) != 0) {
		return 11;
	}
	uint32_t nonempty_bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &term_db, &type_db, &judgement,
			extended, &nonempty_bridge
		) != 1) {
		return 12;
	}
	struct prototype_context_formation_certificate forged_context_certificates[2] = {
		context_certificates[0], context_certificates[0]
	};
	struct prototype_context_formation_certificate_db forged_context_db = {
		.certificates = forged_context_certificates,
		.certificate_count = 2,
		.certificate_capacity = 2
	};
	if (prototype_context_formation_certificate_db_validate(
			&forged_context_db, &context_db, &term_db, &type_db, &judgement
		) == 0) {
		return 13;
	}

	uint32_t bool_view = add_bool_view(
		&term_db, &type_db, empty, universe, text
	);
	uint32_t bool_false;
	uint32_t bool_true;
	if (prototype_term_constructor(&term_db, bool_view, 0, &bool_false) != 0 ||
		prototype_term_constructor(&term_db, bool_view, 1, &bool_true) != 0) {
		return 14;
	}
	uint32_t false_operation = add_operation(
		&operation_graph, empty, bool_false, bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_operation = add_operation(
		&operation_graph, empty, bool_true, bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
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
	uint32_t bool_distinct_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, false_claim, true_claim, bridge, &bool_distinct_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, bool_distinct_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, bool_distinct_goal,
			PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT
		) != 1) {
		return 15;
	}

	uint32_t bool_diagonal_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, false_claim, false_claim, bridge, &bool_diagonal_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, bool_diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, bool_diagonal_goal, PROTOTYPE_HOTT_RULE_OBS_DIAGONAL
		) != 1 || count_rule(
			&candidate_db, bool_diagonal_goal,
			PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR
		) != 1) {
		return 16;
	}

	uint32_t neutral;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){
				.namespace_symbol_id = 7,
				.name_symbol_id = 8
			},
			&neutral
		) != 0) {
		return 17;
	}
	uint32_t neutral_operation = add_operation(
		&operation_graph, empty, neutral, bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t neutral_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, neutral_operation, empty,
		neutral_operation, neutral, bool_view
	);
	uint32_t neutral_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, neutral_claim, false_claim, bridge, &neutral_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, neutral_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, neutral_goal, PROTOTYPE_HOTT_RULE_OBS_MATCH_ACTION
		) != 1 || count_rule(
			&candidate_db, neutral_goal,
			PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR
		) != 0) {
		return 18;
	}
	uint32_t universe_successor;
	if (prototype_term_universe_var(&term_db, 1, &universe_successor) != 0) {
		return 35;
	}
	uint32_t bool_type_operation = add_operation(
		&operation_graph, empty, bool_view, universe,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t text_type_operation = add_operation(
		&operation_graph, empty, text, universe,
		PROTOTYPE_OPERATION_POLARITY_VALUE
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
	uint32_t text_has_universe = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, text_type_operation, empty,
		text_type_operation, text, universe
	);
	uint32_t universe_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			universe_is_type, bool_has_universe, text_has_universe, bridge,
			&universe_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db, NULL, &operation_graph, &judgement, universe_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].residual_reason != PROTOTYPE_HOTT_RESIDUAL_UNIVERSE) {
		return 36;
	}

	uint32_t empty_row;
	uint32_t print_row;
	uint32_t pure_comp;
	uint32_t effect_comp;
	uint32_t returned_left;
	uint32_t returned_right;
	if (prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_row
		) != 0 || prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_PRINT, &print_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_row, text, &pure_comp
		) != 0 || prototype_term_computation_type(
			&term_db, print_row, text, &effect_comp
		) != 0 || prototype_term_return(
			&term_db, left_text, &returned_left
		) != 0 || prototype_term_return(
			&term_db, right_text, &returned_right
		) != 0) {
		return 19;
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
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_left_claim,
			.premise_count = 1,
			.premise_claim_ids = { left_has_type }
		};
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_right_claim,
			.premise_count = 1,
			.premise_claim_ids = { right_has_type }
		};
	uint32_t computation_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			pure_comp_is_type, returned_left_claim, returned_right_claim, bridge,
			&computation_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, computation_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, computation_goal, PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN
		) != 1) {
		return 20;
	}
	uint32_t thunk_type;
	uint32_t thunk_left;
	uint32_t thunk_right;
	if (prototype_term_thunk_type(
			&term_db, pure_comp, &thunk_type
		) != 0 || prototype_term_thunk(
			&term_db, returned_left, &thunk_left
		) != 0 || prototype_term_thunk(
			&term_db, returned_right, &thunk_right
		) != 0) {
		return 33;
	}
	uint32_t thunk_type_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, thunk_type, empty,
		PROTOTYPE_INVALID_ID, thunk_type, universe
	);
	uint32_t thunk_left_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_left, empty,
		PROTOTYPE_INVALID_ID, thunk_left, thunk_type
	);
	uint32_t thunk_right_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_right, empty,
		PROTOTYPE_INVALID_ID, thunk_right, thunk_type
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunk_left_claim,
			.premise_count = 1,
			.premise_claim_ids = { returned_left_claim }
		};
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunk_right_claim,
			.premise_count = 1,
			.premise_claim_ids = { returned_right_claim }
		};
	uint32_t thunk_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			thunk_type_is_type, thunk_left_claim, thunk_right_claim, bridge,
			&thunk_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db, NULL, &operation_graph, &judgement, thunk_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, thunk_goal, PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE
		) != 1) {
		return 34;
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
	uint32_t effect_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_comp_is_type, effect_left_claim, effect_right_claim, bridge,
			&effect_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, effect_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 21;
	}
	uint32_t continuation_binder = prototype_term_new_binding(&term_db);
	uint32_t continuation_variable;
	uint32_t continuation_return;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t print_operation;
	uint32_t request;
	if (continuation_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, continuation_binder, &continuation_variable
		) != 0 || prototype_term_return(
			&term_db, continuation_variable, &continuation_return
		) != 0 || prototype_term_lambda(
			&term_db, continuation_binder, continuation_return,
			&continuation_lambda
		) != 0 || prototype_term_thunk(
			&term_db, continuation_lambda, &continuation_thunk
		) != 0 || prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &print_operation
		) != 0 || prototype_term_operation_request(
			&term_db, print_operation, left_text, continuation_thunk, &request
		) != 0) {
		return 37;
	}
	uint32_t request_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, request, empty,
		PROTOTYPE_INVALID_ID, request, effect_comp
	);
	uint32_t request_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_comp_is_type, request_claim, request_claim, bridge,
			&request_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db, NULL, &operation_graph, &judgement, request_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST) {
		return 38;
	}

	uint32_t family_binder = prototype_term_new_binding(&term_db);
	uint32_t pure_family;
	uint32_t effect_family;
	uint32_t pure_pi;
	uint32_t effect_pi;
	if (family_binder == PROTOTYPE_INVALID_ID || prototype_term_pure_family(
			&term_db, family_binder, pure_comp, &pure_family
		) != 0 || prototype_term_pure_family(
			&term_db, family_binder, effect_comp, &effect_family
		) != 0 || prototype_term_pi_family(
			&term_db, text, pure_family, &pure_pi
		) != 0 || prototype_term_pi_family(
			&term_db, text, effect_family, &effect_pi
		) != 0) {
		return 22;
	}
	uint32_t pure_function;
	uint32_t effect_function;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ .namespace_symbol_id = 9,
				.name_symbol_id = 1 },
			&pure_function
		) != 0 || prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ .namespace_symbol_id = 9,
				.name_symbol_id = 2 },
			&effect_function
		) != 0) {
		return 23;
	}
	uint32_t pure_pi_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_pi, empty,
		PROTOTYPE_INVALID_ID, pure_pi, universe
	);
	uint32_t effect_pi_is_type = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_pi, empty,
		PROTOTYPE_INVALID_ID, effect_pi, universe
	);
	uint32_t pure_function_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, pure_function, empty,
		PROTOTYPE_INVALID_ID, pure_function, pure_pi
	);
	uint32_t effect_function_claim = add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, effect_function, empty,
		PROTOTYPE_INVALID_ID, effect_function, effect_pi
	);
	uint32_t pure_pi_goal;
	uint32_t effect_pi_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			pure_pi_is_type, pure_function_claim, pure_function_claim, bridge,
			&pure_pi_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, pure_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, pure_pi_goal, PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE
		) != 1 || prototype_hott_observation_goal_db_intern(
			&goal_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_pi_is_type, effect_function_claim, effect_function_claim, bridge,
			&effect_pi_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &bridge_db,
			&term_db, &type_db,
			NULL, &operation_graph, &judgement, effect_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 24;
	}

	if (prototype_hott_candidate_db_validate(
			&candidate_db, &goal_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &term_db,
			&type_db, &judgement, &operation_graph
		) != 0 || prototype_hott_work_db_validate(
			&work_db, &goal_db, &candidate_db
		) != 0) {
		return 25;
	}
	uint32_t conversion_candidate = find_rule(
		&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_OBS_CONVERT
	);
	if (conversion_candidate == PROTOTYPE_INVALID_ID) {
		return 41;
	}
	uint32_t conversion_edge =
		candidate_db.candidates[conversion_candidate].first_conversion_premise;
	uint64_t saved_revision =
		candidate_db.conversion_premises[conversion_edge].conversion_graph_revision;
	++candidate_db.conversion_premises[conversion_edge].conversion_graph_revision;
	if (prototype_hott_candidate_db_validate(
			&candidate_db, &goal_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &term_db,
			&type_db, &judgement, &operation_graph
		) == 0) {
		return 42;
	}
	candidate_db.conversion_premises[conversion_edge].conversion_graph_revision =
		saved_revision;

	/* Two derivations may share one child goal; adjacency belongs to candidates. */
	struct prototype_hott_candidate shared_candidates[2] = {
		{
			.id = 0,
			.conclusion_goal_id = bool_distinct_goal,
			.rule = PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR,
			.first_child_edge = 0,
			.child_edge_count = 1
		},
		{
			.id = 1,
			.conclusion_goal_id = bool_diagonal_goal,
			.rule = PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR,
			.first_child_edge = 1,
			.child_edge_count = 1
		}
	};
	struct prototype_hott_child_edge shared_edges[2] = {
		{ .candidate_id = 0, .child_goal_id = diagonal_goal,
			.role = PROTOTYPE_HOTT_CHILD_ADT_FIELD, .ordinal = 0 },
		{ .candidate_id = 1, .child_goal_id = diagonal_goal,
			.role = PROTOTYPE_HOTT_CHILD_ADT_FIELD, .ordinal = 0 }
	};
	struct prototype_hott_candidate_db shared_db;
	prototype_hott_candidate_db_init(
		&shared_db, shared_candidates, 2, NULL, 0, shared_edges, 2,
		NULL, 0, NULL, 0, NULL, 0
	);
	shared_db.candidate_count = 2;
	shared_db.child_edge_count = 2;
	if (prototype_hott_candidate_db_validate(
			&shared_db, &goal_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &term_db,
			&type_db, &judgement, &operation_graph
		) != 0) {
		return 26;
	}
	shared_edges[1].child_goal_id = bool_diagonal_goal;
	if (prototype_hott_candidate_db_validate(
			&shared_db, &goal_db, &context_db, &substitution_db,
			&context_certificate_db, &substitution_certificate_db, &term_db,
			&type_db, &judgement, &operation_graph
		) == 0) {
		return 27;
	}

	uint32_t action_id;
	struct prototype_hott_action context_action;
	memset(&context_action, 0xff, sizeof(context_action));
	context_action.kind = PROTOTYPE_HOTT_ACTION_CONTEXT;
	context_action.state = PROTOTYPE_HOTT_ACTION_READY;
	context_action.source_context_id = empty;
	context_action.result_bridge_id = bridge;
	if (prototype_hott_action_db_add(
			&action_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, context_action, &action_id
		) != 0 || action_id != 0) {
		return 28;
	}
	struct prototype_hott_action type_action;
	memset(&type_action, 0xff, sizeof(type_action));
	type_action.kind = PROTOTYPE_HOTT_ACTION_TYPE;
	type_action.state = PROTOTYPE_HOTT_ACTION_DEFERRED;
	type_action.source_claim_id = text_is_type;
	type_action.source_bridge_id = bridge;
	if (prototype_hott_action_db_add(
			&action_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, type_action, &action_id
		) != 0) {
		return 29;
	}
	uint32_t type_action_id = action_id;
	struct prototype_hott_action term_action;
	memset(&term_action, 0xff, sizeof(term_action));
	term_action.kind = PROTOTYPE_HOTT_ACTION_TERM;
	term_action.state = PROTOTYPE_HOTT_ACTION_DEFERRED;
	term_action.source_claim_id = left_has_type;
	term_action.source_bridge_id = bridge;
	term_action.type_action_id = type_action_id;
	if (prototype_hott_action_db_add(
			&action_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, term_action, &action_id
		) != 0) {
		return 30;
	}
	uint32_t identity;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, empty, &identity
		) != 0) {
		return 31;
	}
	struct prototype_hott_action substitution_action;
	memset(&substitution_action, 0xff, sizeof(substitution_action));
	substitution_action.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION;
	substitution_action.state = PROTOTYPE_HOTT_ACTION_DEFERRED;
	substitution_action.source_substitution_id = identity;
	substitution_action.source_bridge_id = bridge;
	substitution_action.target_bridge_id = bridge;
	if (prototype_hott_action_db_add(
			&action_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement, substitution_action, &action_id
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &context_db, &substitution_db, &context_certificate_db,
			&substitution_certificate_db, &bridge_db, &term_db, &type_db,
			&operation_graph, &judgement
		) != 0) {
		return 32;
	}

	free(type_db.representations);
	return 0;
}
