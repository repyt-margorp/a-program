#include "ast.h"
#include "hott.h"
#include "calculus.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TERM_CAPACITY 512
#define CLAIM_CAPACITY 512
#define CONTEXT_CAPACITY 128
#define SUBSTITUTION_CAPACITY 512
#define OPERATION_CAPACITY 64
#define GOAL_CAPACITY 64
#define CANDIDATE_CAPACITY 128
#define PREMISE_CAPACITY 128

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[128];
static int case_labels[128];
static struct prototype_case_binder case_binders[128];
static struct prototype_ih_scope ih_scopes[64];
static struct prototype_type_declaration type_declarations[8];
static struct prototype_type_constructor_declaration constructors[16];
static struct prototype_type_parameter_declaration parameters[8];
static uint32_t field_types[32];
static struct prototype_type_expr type_exprs[32];
static struct prototype_context contexts[CONTEXT_CAPACITY];
static struct prototype_substitution substitutions[SUBSTITUTION_CAPACITY];
static struct prototype_judgement_proposition propositions[CLAIM_CAPACITY];
static struct prototype_judgement_derivation_candidate derivation_candidates[CLAIM_CAPACITY];
static struct prototype_judgement_claim claims[CLAIM_CAPACITY];
static struct prototype_judgement_derivation derivations[CLAIM_CAPACITY];
static struct prototype_judgement_candidate_premise
	judgement_candidate_premises[
		CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
static struct prototype_judgement_premise_edge judgement_accepted_premises[
	CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
];
static struct prototype_cwf_certificate cwf_certificates[256];
static struct prototype_operation_node operations[OPERATION_CAPACITY];
static struct prototype_hott_bridge bridges[64];
static struct prototype_hott_bridge_certificate bridge_certificates[64];
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
static struct prototype_hott_residual_obligation residuals[32];
static struct prototype_hott_action_request action_requests[128];
static struct prototype_hott_action_certificate action_certificates[128];
static struct prototype_hott_action_result action_results[128];

static int fixture_proposition_equal(
	const struct prototype_judgement_proposition* left,
	const struct prototype_judgement_proposition* right
) {
	return left->kind == right->kind &&
		left->authority_kind == right->authority_kind &&
		left->authority_id == right->authority_id &&
		left->context_id == right->context_id &&
		left->operation_id == right->operation_id &&
		left->subject == right->subject &&
		left->classifier == right->classifier;
}

static uint32_t fixture_add_claim(
	struct prototype_judgement_db* judgement,
	int kind,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t operation_id,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement || !judgement->propositions || !judgement->claims) {
		return PROTOTYPE_INVALID_ID;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = kind,
		.authority_kind = authority_kind,
		.authority_id = authority_id,
		.context_id = context_id,
		.operation_id = operation_id,
		.subject = subject,
		.classifier = classifier
	};
	uint32_t proposition_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->proposition_count; ++i) {
		if (fixture_proposition_equal(&judgement->propositions[i], &proposition)) {
			proposition_id = i;
			break;
		}
	}
	if (proposition_id == PROTOTYPE_INVALID_ID) {
		if (judgement->proposition_count >= judgement->proposition_capacity) {
			return PROTOTYPE_INVALID_ID;
		}
		proposition_id = (uint32_t)judgement->proposition_count++;
		judgement->propositions[proposition_id] = proposition;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		if (judgement->claims[i].proposition_id == proposition_id) {
			return i;
		}
	}
	if (judgement->claim_count >= judgement->claim_capacity) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t id = (uint32_t)judgement->claim_count++;
	judgement->claims[id] = (struct prototype_judgement_claim) {
		.proposition_id = proposition_id,
		.proposition = &judgement->propositions[proposition_id],
		.closure_rank = 0
	};
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
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

static uint32_t add_box_view(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* types_db,
	struct prototype_context_db* contexts_db,
	uint32_t empty,
	uint32_t universe,
	uint32_t bool_view,
	uint32_t core
) {
	uint32_t type_id;
	if (prototype_type_declaration_add(types_db, 200, &type_id) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = type_id;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 200;
	terms_db->terms[view].as.type_view.core = core;
	terms_db->terms[view].as.type_view.source = core;
	types_db->type_declarations[type_id].formation_classifier = universe;
	types_db->type_declarations[type_id].parameter_context = empty;
	uint32_t field_binding = prototype_term_new_binding(terms_db);
	uint32_t field_context;
	uint32_t curried_classifier;
	uint32_t constructor_id;
	uint32_t readback_field = PROTOTYPE_INVALID_ID;
	if (field_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts_db,
			empty,
			field_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms_db,
			contexts_db,
			empty,
			field_context,
			view,
			&curried_classifier
		) != 0 || prototype_type_declaration_add_constructor(
			types_db,
			type_id,
			201,
			&readback_field,
			1,
			PROTOTYPE_INVALID_ID,
			empty,
			field_context,
			view,
			curried_classifier,
			&constructor_id
		) != 0 || constructor_id != 2) {
		return PROTOTYPE_INVALID_ID;
	}
	prototype_term_notify_graph_mutation(terms_db);
	return view;
}

static uint32_t add_nat_view(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* types_db,
	struct prototype_context_db* contexts_db,
	uint32_t empty,
	uint32_t universe,
	uint32_t core
) {
	uint32_t type_id;
	if (prototype_type_declaration_add(types_db, 300, &type_id) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = type_id;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 300;
	terms_db->terms[view].as.type_view.core = core;
	terms_db->terms[view].as.type_view.source = core;
	types_db->type_declarations[type_id].formation_classifier = universe;
	types_db->type_declarations[type_id].parameter_context = empty;
	uint32_t zero_constructor_id;
	if (prototype_type_declaration_add_constructor(
			types_db,
			type_id,
			301,
			NULL,
			0,
			PROTOTYPE_INVALID_ID,
			empty,
			empty,
			view,
			view,
			&zero_constructor_id
		) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t field_binding = prototype_term_new_binding(terms_db);
	uint32_t field_context;
	uint32_t succ_classifier;
	uint32_t succ_constructor_id;
	uint32_t readback_field = PROTOTYPE_INVALID_ID;
	if (field_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts_db,
			empty,
			field_binding,
			view,
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms_db,
			contexts_db,
			empty,
			field_context,
			view,
			&succ_classifier
		) != 0 || prototype_type_declaration_add_constructor(
			types_db,
			type_id,
			302,
			&readback_field,
			1,
			PROTOTYPE_INVALID_ID,
			empty,
			field_context,
			view,
			succ_classifier,
			&succ_constructor_id
		) != 0 || succ_constructor_id != zero_constructor_id + 1) {
		return PROTOTYPE_INVALID_ID;
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
	struct prototype_cwf_certificate_db cwf_certificate_db;
	struct prototype_operation_graph operation_graph;
	struct prototype_hott_bridge_db bridge_db;
	struct prototype_hott_observation_goal_db goal_db;
	struct prototype_hott_candidate_db candidate_db;
	struct prototype_hott_work_db work_db;
	struct prototype_hott_residual_db residual_db;
	struct prototype_hott_action_db action_db;
	struct prototype_kernel_builder kernel_builder;
	struct prototype_kernel_view kernel_view;

	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 128,
		case_binders, 128, ih_scopes, 64
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
		&judgement, propositions, derivation_candidates, claims,
		derivations, CLAIM_CAPACITY,
		judgement_candidate_premises,
		CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		judgement_accepted_premises,
		CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_cwf_certificate_db_init(
		&cwf_certificate_db, cwf_certificates, 256
	);
	memset(&operation_graph, 0, sizeof(operation_graph));
	operation_graph.operations = operations;
	operation_graph.operation_capacity = OPERATION_CAPACITY;
	kernel_builder = (struct prototype_kernel_builder) {
		.contexts = &context_db,
		.substitutions = &substitution_db,
		.cwf_certificates = &cwf_certificate_db,
		.terms = &term_db,
		.type_declarations = &type_db,
		.operations = &operation_graph,
		.judgement = &judgement
	};
	if (prototype_kernel_builder_view(&kernel_builder, &kernel_view) != 0) {
		return 138;
	}
	prototype_hott_bridge_db_init(
		&bridge_db, bridges, 64, bridge_certificates, 64
	);
	prototype_hott_observation_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
	prototype_hott_candidate_db_init(
		&candidate_db, candidates, CANDIDATE_CAPACITY,
		claim_premises, PREMISE_CAPACITY, child_edges, PREMISE_CAPACITY,
		conversion_premises, PREMISE_CAPACITY,
		context_certificate_premises, PREMISE_CAPACITY,
		substitution_certificate_premises, PREMISE_CAPACITY
	);
	prototype_hott_work_db_init(&work_db, work_items, GOAL_CAPACITY);
	prototype_hott_residual_db_init(&residual_db, residuals, 32);
	prototype_hott_action_db_init(
		&action_db,
		action_requests,
		128,
		action_certificates,
		128,
		action_results,
		128
	);

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
	uint32_t text_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, text, empty,
		PROTOTYPE_INVALID_ID, text, universe
	);
	uint32_t left_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, left_operation, empty,
		left_operation, left_text, text
	);
	uint32_t right_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, right_operation, empty,
		right_operation, right_text, text
	);
	uint32_t sibling_has_type = fixture_add_claim(
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
			&bridge_db, &kernel_builder,
			empty, &bridge
		) != 0 || prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0) {
		return 3;
	}

	uint32_t text_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, right_has_type, bridge, &text_goal
		) != 0 || text_goal != 0 ||
		prototype_hott_observation_goal_db_validate(
			&goal_db, &kernel_view, &bridge_db
		) != 0) {
		return 4;
	}
	uint32_t interned;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, right_has_type, bridge, &interned
		) != 0 || interned != text_goal || goal_db.goal_count != 1) {
		return 5;
	}
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, sibling_has_type, right_has_type, bridge, &interned
		) != 0 || interned == text_goal) {
		return 6;
	}

	uint32_t work_id;
	if (prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, text_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ||
		strcmp(
			work_items[work_id].outcome.calculus_fingerprint,
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
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, left_has_type, left_has_type, bridge, &diagonal_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
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
	uint32_t beta_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, beta_operation, empty,
		beta_operation, beta_text, text
	);
	uint32_t conversion_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			text_is_type, beta_claim, left_has_type, bridge, &conversion_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL,
			conversion_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
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
	if (prototype_cwf_certificate_db_add_context(
		&cwf_certificate_db, &context_db, &term_db, &type_db, &judgement,
			extended, text_is_type, &context_certificate
		) != 0 || prototype_cwf_certificate_db_validate_contexts(
		&cwf_certificate_db, &context_db, &term_db, &type_db, &judgement
		) != 0) {
		return 11;
	}
	uint32_t nonempty_bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &kernel_builder,
			extended, &nonempty_bridge
		) != 1) {
		return 12;
	}
	struct prototype_cwf_certificate forged_context_certificates[2] = {
		cwf_certificates[0], cwf_certificates[0]
	};
	struct prototype_cwf_certificate_db forged_context_db = {
		.certificates = forged_context_certificates,
		.certificate_count = 2,
		.certificate_capacity = 2
	};
	if (prototype_cwf_certificate_db_validate_contexts(
			&forged_context_db, &context_db, &term_db, &type_db, &judgement
		) == 0) {
		return 13;
	}
	struct prototype_cwf_certificate forged_cross_kind =
		cwf_certificates[context_certificate];
	forged_cross_kind.kind = PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION;
	struct prototype_cwf_certificate_db forged_cross_kind_db = {
		.certificates = &forged_cross_kind,
		.certificate_count = 1,
		.certificate_capacity = 1
	};
	if (prototype_cwf_certificate_db_get_kind(
			&cwf_certificate_db,
			context_certificate,
			PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
		) != NULL || prototype_cwf_certificate_db_validate(
			&forged_cross_kind_db,
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			&judgement
		) == 0) {
		return 136;
	}
	struct prototype_cwf_certificate forged_wrong_claim =
		cwf_certificates[context_certificate];
	forged_wrong_claim.claim_id = left_has_type;
	struct prototype_cwf_certificate_db forged_wrong_claim_db = {
		.certificates = &forged_wrong_claim,
		.certificate_count = 1,
		.certificate_capacity = 1
	};
	if (prototype_cwf_certificate_db_validate_contexts(
			&forged_wrong_claim_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement
		) == 0) {
		return 137;
	}
	struct prototype_judgement_proposition foreign_propositions[1];
	struct prototype_judgement_derivation_candidate
		foreign_derivation_candidates[1];
	struct prototype_judgement_claim foreign_claims[1];
	struct prototype_judgement_derivation foreign_derivations[1];
	struct prototype_judgement_candidate_premise foreign_candidate_premises[1];
	struct prototype_judgement_premise_edge foreign_accepted_premises[1];
	struct prototype_judgement_db foreign_judgement;
	prototype_judgement_db_init(
		&foreign_judgement,
		foreign_propositions,
		foreign_derivation_candidates,
		foreign_claims,
		foreign_derivations,
		1,
		foreign_candidate_premises,
		1,
		foreign_accepted_premises,
		1
	);
	struct prototype_kernel_view mixed_kernel_view = kernel_view;
	mixed_kernel_view.judgement = &foreign_judgement;
	if (prototype_kernel_view_validate(&mixed_kernel_view) == 0) {
		return 139;
	}

	uint32_t bool_view = add_bool_view(
		&term_db, &type_db, empty, universe, text
	);
	uint32_t box_view = add_box_view(
		&term_db,
		&type_db,
		&context_db,
		empty,
		universe,
		bool_view,
		text
	);
	uint32_t nat_view = add_nat_view(
		&term_db, &type_db, &context_db, empty, universe, text
	);
	if (box_view == PROTOTYPE_INVALID_ID || nat_view == PROTOTYPE_INVALID_ID) {
		return 73;
	}
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
	uint32_t bool_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, bool_view, empty,
		PROTOTYPE_INVALID_ID, bool_view, universe
	);
	uint32_t box_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, box_view, empty,
		PROTOTYPE_INVALID_ID, box_view, universe
	);
	uint32_t false_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, false_operation, empty,
		false_operation, bool_false, bool_view
	);
	uint32_t true_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, true_operation, empty,
		true_operation, bool_true, bool_view
	);
	uint32_t bool_distinct_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, false_claim, true_claim, bridge, &bool_distinct_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_distinct_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
			count_rule(
				&candidate_db, bool_distinct_goal,
				PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT
			) != 1) {
		return 15;
	}
	uint32_t bool_diagonal_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, false_claim, false_claim, bridge, &bool_diagonal_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_diagonal_goal,
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
	uint32_t neutral_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, neutral_operation, empty,
		neutral_operation, neutral, bool_view
	);
	uint32_t neutral_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			bool_is_type, neutral_claim, false_claim, bridge, &neutral_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, neutral_goal,
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
	uint32_t universe_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE, universe, empty,
		PROTOTYPE_INVALID_ID, universe, universe_successor
	);
	uint32_t bool_has_universe = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, bool_type_operation, empty,
		bool_type_operation, bool_view, universe
	);
	uint32_t text_has_universe = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, text_type_operation, empty,
		text_type_operation, text, universe
	);
	uint32_t universe_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			universe_is_type, bool_has_universe, text_has_universe, bridge,
			&universe_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, universe_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_UNIVERSE) {
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
	uint32_t pure_comp_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_comp, empty,
		PROTOTYPE_INVALID_ID, pure_comp, universe
	);
	uint32_t returned_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, pure_comp
	);
	uint32_t returned_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, pure_comp
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_left_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = left_has_type } }
		};
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_right_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = right_has_type } }
		};
	uint32_t computation_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			pure_comp_is_type, returned_left_claim, returned_right_claim, bridge,
			&computation_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, computation_goal,
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
	uint32_t thunk_type_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, thunk_type, empty,
		PROTOTYPE_INVALID_ID, thunk_type, universe
	);
	uint32_t thunk_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_left, empty,
		PROTOTYPE_INVALID_ID, thunk_left, thunk_type
	);
	uint32_t thunk_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_right, empty,
		PROTOTYPE_INVALID_ID, thunk_right, thunk_type
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunk_left_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = returned_left_claim } }
		};
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunk_right_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = returned_right_claim } }
		};
	uint32_t thunk_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_VALUE,
			thunk_type_is_type, thunk_left_claim, thunk_right_claim, bridge,
			&thunk_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, thunk_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, thunk_goal, PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE
		) != 1) {
		return 34;
	}

	uint32_t effect_comp_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_comp, empty,
		PROTOTYPE_INVALID_ID, effect_comp, universe
	);
	uint32_t effect_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, effect_comp
	);
	uint32_t effect_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, effect_comp
	);
	uint32_t effect_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_comp_is_type, effect_left_claim, effect_right_claim, bridge,
			&effect_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, effect_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
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
	uint32_t request_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, request, empty,
		PROTOTYPE_INVALID_ID, request, effect_comp
	);
	uint32_t request_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_comp_is_type, request_claim, request_claim, bridge,
			&request_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, request_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason !=
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
	uint32_t pure_pi_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_pi, empty,
		PROTOTYPE_INVALID_ID, pure_pi, universe
	);
	uint32_t effect_pi_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_pi, empty,
		PROTOTYPE_INVALID_ID, effect_pi, universe
	);
	uint32_t pure_function_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, pure_function, empty,
		PROTOTYPE_INVALID_ID, pure_function, pure_pi
	);
	uint32_t effect_function_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, effect_function, empty,
		PROTOTYPE_INVALID_ID, effect_function, effect_pi
	);
	uint32_t pure_pi_goal;
	uint32_t effect_pi_goal;
	if (prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			pure_pi_is_type, pure_function_claim, pure_function_claim, bridge,
			&pure_pi_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, pure_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, pure_pi_goal, PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE
		) != 1 || prototype_hott_observation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
			effect_pi_is_type, effect_function_claim, effect_function_claim, bridge,
			&effect_pi_goal
		) != 0 || prototype_hott_observation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, effect_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 24;
	}

	if (prototype_hott_candidate_db_validate(
			&candidate_db, &goal_db, &kernel_view
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
			&candidate_db, &goal_db, &kernel_view
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
			&shared_db, &goal_db, &kernel_view
		) != 0) {
		return 26;
	}
	shared_edges[1].child_goal_id = bool_diagonal_goal;
	if (prototype_hott_candidate_db_validate(
			&shared_db, &goal_db, &kernel_view
		) == 0) {
		return 27;
	}

	uint32_t action_id;
	struct prototype_hott_action_request context_action = {
		.kind = PROTOTYPE_HOTT_ACTION_CONTEXT,
		.key.context = { .source_context_id = empty }
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, context_action, &action_id
		) != 0 || action_id != 0) {
		return 28;
	}
	uint32_t context_certificate_id;
	struct prototype_hott_action_certificate context_action_certificate = {
		.request_id = action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE,
		.data.context = { .result_bridge_id = bridge }
	};
	if (prototype_hott_action_certificate_add(
			&action_db, &kernel_view, &bridge_db, context_action_certificate,
			&context_certificate_id
		) != 0) {
		return 29;
	}
	struct prototype_hott_action_result context_result = {
		.request_id = action_id,
		.certificate_id = context_certificate_id,
		.outcome = {
			.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			.step_limit = 0,
			.term_graph_revision = term_db.term_count
		}
	};
	memcpy(
		context_result.outcome.calculus_fingerprint,
		PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
		65
	);
	uint32_t result_id;
	if (prototype_hott_action_result_publish(
			&action_db, context_result, &result_id
		) != 0 || result_id != 0) {
		return 30;
	}
	struct prototype_hott_action_request type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = text_is_type,
			.source_bridge_id = bridge
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, type_action, &action_id
		) != 0) {
		return 31;
	}
	uint32_t type_action_id = action_id;
	uint32_t repeated_type_action_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, type_action, &repeated_type_action_id
		) != 0 || repeated_type_action_id != type_action_id ||
		action_db.request_count != 2) {
		return 32;
	}
	struct prototype_hott_action_certificate wrong_kind_certificate = {
		.request_id = type_action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE,
		.data.context = { .result_bridge_id = bridge }
	};
	if (prototype_hott_action_certificate_add(
			&action_db, &kernel_view, &bridge_db, wrong_kind_certificate,
			&(uint32_t){0}
		) == 0) {
		return 33;
	}
	struct prototype_hott_action_request term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = left_has_type,
			.source_bridge_id = bridge,
			.type_action_request_id = type_action_id
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, term_action, &action_id
		) != 0) {
		return 34;
	}
	uint32_t identity;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, empty, &identity
		) != 0) {
		return 35;
	}
	struct prototype_hott_action_request substitution_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = identity,
			.source_bridge_id = bridge,
			.target_bridge_id = bridge
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, substitution_action, &action_id
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 36;
	}
	uint32_t identity_substitution_action_id = action_id;

	struct prototype_hott_action_request bool_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_type_action_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_type_action, &bool_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id, &result_id
		) != 0 || action_results[result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 41;
	}
	struct prototype_hott_action_request bool_term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = false_claim,
			.source_bridge_id = bridge,
			.type_action_request_id = bool_type_action_id
		}
	};
	uint32_t bool_term_action_id;
	uint32_t bool_term_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_term_action, &bool_term_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, bool_term_action_id,
			&bool_term_result_id
		) != 0 || action_results[bool_term_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 53;
	}
	uint32_t bool_term_certificate_id =
		action_results[bool_term_result_id].certificate_id;
	uint32_t saved_witness =
		action_certificates[bool_term_certificate_id].data.term.witness_term_id;
	action_certificates[bool_term_certificate_id].data.term.witness_term_id =
		bool_false;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 54;
	}
	action_certificates[bool_term_certificate_id].data.term.witness_term_id =
		saved_witness;
	uint32_t bool_binding = prototype_term_new_binding(&term_db);
	uint32_t bool_context;
	uint32_t bool_context_certificate;
	if (bool_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			bool_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&bool_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			bool_context,
			bool_is_type,
			&bool_context_certificate
		) != 0) {
		return 42;
	}
	uint32_t bool_bridge;
	size_t bridge_count_before = bridge_db.bridge_count;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			bool_context,
			bool_type_action_id,
			&bool_bridge
		) != 0 || bridge_db.bridge_count != bridge_count_before + 1 ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 43;
	}
	uint32_t repeated_bool_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			bool_context,
			bool_type_action_id,
			&repeated_bool_bridge
		) != 0 || repeated_bool_bridge != bool_bridge ||
		bridge_db.bridge_count != bridge_count_before + 1) {
		return 44;
	}
	uint32_t bool_type_result_id;
	if (prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id,
			&bool_type_result_id
		) != 0 || bool_type_result_id != result_id) {
		return 45;
	}
	uint32_t type_certificate_id = action_results[result_id].certificate_id;
	uint32_t saved_relation_type =
		action_certificates[type_certificate_id].data.type.relation_type_term_id;
	action_certificates[type_certificate_id].data.type.relation_type_term_id = text;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 46;
	}
	action_certificates[type_certificate_id].data.type.relation_type_term_id =
		saved_relation_type;
	if (prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, type_action_id, &result_id
		) != 0 || action_results[result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED ||
		action_results[result_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE) {
		return 47;
	}
	uint32_t identity_action_result;
	if (prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, identity_substitution_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&identity_action_result
		) != 0 || action_results[identity_action_result].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 48;
	}
	uint32_t bool_projection;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, bool_context, &bool_projection
		) != 0) {
		return 49;
	}
	struct prototype_hott_action_request projection_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = bool_projection,
			.source_bridge_id = bool_bridge,
			.target_bridge_id = bridge
		}
	};
	uint32_t projection_action_id;
	uint32_t projection_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, projection_action,
			&projection_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, projection_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&projection_result_id
		) != 0 || action_results[projection_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 50;
	}
	uint32_t bool_identity;
	uint32_t composed_projection;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, bool_context, &bool_identity
		) != 0 || prototype_substitution_compose(
			&substitution_db,
			&context_db,
			bool_projection,
			bool_identity,
			&composed_projection
		) != 0) {
		return 51;
	}
	struct prototype_hott_action_request composition_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = composed_projection,
			.source_bridge_id = bool_bridge,
			.target_bridge_id = bridge
		}
	};
	uint32_t composition_action_id;
	uint32_t composition_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, composition_action,
			&composition_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, composition_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&composition_result_id
		) != 0 || action_results[composition_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 52;
	}
	uint32_t empty_to_empty;
	uint32_t false_extension;
	uint32_t false_extension_certificate;
	if (prototype_substitution_empty(
			&substitution_db, &context_db, empty, &empty_to_empty
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			bool_false,
			bool_view,
			&false_extension
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_claim,
			&false_extension_certificate
		) != 0) {
		return 55;
	}
	struct prototype_hott_action_request extension_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = false_extension,
			.source_bridge_id = bridge,
			.target_bridge_id = bool_bridge
		}
	};
	uint32_t extension_action_id;
	uint32_t extension_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, extension_action, &extension_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, extension_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 64,
			&extension_result_id
		) != 0 || action_results[extension_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 56;
	}

	uint32_t bool_type_in_context_claim;
	uint32_t bool_variable_claim;
	uint32_t bool_variable;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			bool_projection,
			&bool_type_in_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			bool_context,
			bool_binding,
			bool_view,
			&bool_variable_claim
		) != 0 || prototype_term_var(
			&term_db, bool_binding, &bool_variable
		) != 0) {
		return 57;
	}
	struct prototype_hott_action_request contextual_bool_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = bool_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t contextual_bool_type_action_id;
	uint32_t contextual_bool_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, contextual_bool_type_action,
			&contextual_bool_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, contextual_bool_type_action_id,
			&contextual_bool_type_result_id
		) != 0 || action_results[contextual_bool_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 58;
	}
	struct prototype_hott_action_request bool_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = bool_variable_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = contextual_bool_type_action_id
		}
	};
	uint32_t bool_variable_action_id;
	uint32_t bool_variable_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_variable_action,
			&bool_variable_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, bool_variable_action_id,
			&bool_variable_result_id
		) != 0 || action_results[bool_variable_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 59;
	}
	const struct prototype_context* bool_relation_context =
		prototype_context_get(
			&context_db, bridges[bool_bridge].bridge_context_id
		);
	uint32_t bool_relation_variable;
	if (!bool_relation_context || prototype_term_var(
			&term_db,
			bool_relation_context->binding_id,
			&bool_relation_variable
		) != 0 || action_certificates[
			action_results[bool_variable_result_id].certificate_id
		].data.term.witness_term_id != bool_relation_variable ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 60;
	}
	uint32_t box_type_in_context_claim;
	uint32_t box_constructor;
	uint32_t boxed_bool_variable;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			box_is_type,
			bool_projection,
			&box_type_in_context_claim
		) != 0 || prototype_term_constructor(
			&term_db, box_view, 0, &box_constructor
		) != 0 || prototype_term_app(
			&term_db, box_constructor, bool_variable, &boxed_bool_variable
		) != 0) {
		return 74;
	}
	uint32_t boxed_operation = add_operation(
		&operation_graph,
		bool_context,
		boxed_bool_variable,
		box_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t boxed_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		boxed_operation,
		bool_context,
		boxed_operation,
		boxed_bool_variable,
		box_view
	);
	if (judgement.derivation_count >= judgement.derivation_capacity) {
		return 75;
	}
	uint32_t boxed_derivation_id = (uint32_t)judgement.derivation_count++;
	struct prototype_judgement_derivation* boxed_derivation =
		&judgement.derivations[boxed_derivation_id];
	memset(boxed_derivation, 0, sizeof(*boxed_derivation));
	boxed_derivation->proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	boxed_derivation->conclusion_claim_id = boxed_claim;
	boxed_derivation->closure_rank =
		judgement.claims[bool_variable_claim].closure_rank + 1;
	judgement.claims[boxed_claim].closure_rank = boxed_derivation->closure_rank;
	memset(&boxed_derivation->rule_data, 0xff, sizeof(boxed_derivation->rule_data));
	boxed_derivation->rule_data.constructor.owner_view = box_view;
	boxed_derivation->semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	boxed_derivation->semantic_action_id = PROTOTYPE_INVALID_ID;
	boxed_derivation->premise_count = 1;
	boxed_derivation->premises = &judgement_accepted_premises[
		judgement.accepted_premise_count++
	];
	boxed_derivation->premises[0].claim_id = bool_variable_claim;
	struct prototype_hott_action_request box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = box_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t box_type_action_id;
	uint32_t box_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, box_type_action, &box_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, box_type_action_id, &box_type_result_id
		) != 0 || action_results[box_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 76;
	}
	struct prototype_hott_action_request boxed_term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = boxed_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = box_type_action_id
		}
	};
	uint32_t boxed_term_action_id;
	uint32_t boxed_term_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, boxed_term_action, &boxed_term_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, boxed_term_action_id,
			&boxed_term_result_id
		) != 0 || action_results[boxed_term_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 77;
	}
	uint32_t boxed_witness_claim = action_certificates[
		action_results[boxed_term_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_constructor_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == boxed_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_CONSTRUCTOR_WITNESS &&
			judgement.derivations[i].premise_count == 4) {
			found_constructor_witness = 1;
			break;
		}
	}
	if (!found_constructor_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 78;
	}
	uint32_t pure_box_computation;
	uint32_t returned_box;
	uint32_t pure_box_computation_claim;
	if (prototype_term_computation_type(
			&term_db, empty_row, box_view, &pure_box_computation
		) != 0 || prototype_term_return(
			&term_db, boxed_bool_variable, &returned_box
		) != 0) {
		return 79;
	}
	pure_box_computation_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_box_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_box_computation,
		universe
	);
	uint32_t returned_box_operation = add_operation(
		&operation_graph,
		bool_context,
		returned_box,
		pure_box_computation,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t returned_box_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		returned_box_operation,
		bool_context,
		returned_box_operation,
		returned_box,
		pure_box_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_box_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = boxed_claim } }
		};
	struct prototype_hott_action_request pure_box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = pure_box_computation_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pure_box_type_action_id;
	uint32_t pure_box_type_result_id;
	struct prototype_hott_action_request returned_box_action;
	uint32_t returned_box_action_id;
	uint32_t returned_box_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pure_box_type_action,
			&pure_box_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, pure_box_type_action_id,
			&pure_box_type_result_id
		) != 0 || action_results[pure_box_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 80;
	}
	returned_box_action = (struct prototype_hott_action_request){
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = returned_box_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = pure_box_type_action_id
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, returned_box_action,
			&returned_box_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, returned_box_action_id,
			&returned_box_result_id
		) != 0 || action_results[returned_box_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 81;
	}
	uint32_t returned_box_witness_claim = action_certificates[
		action_results[returned_box_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_return_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				returned_box_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_RETURN_WITNESS &&
			judgement.derivations[i].premise_count == 4 &&
			judgement.derivations[i].premises[3].claim_id == boxed_witness_claim) {
			found_return_witness = 1;
			break;
		}
	}
	if (!found_return_witness) {
		return 82;
	}
	uint32_t pure_box_thunk_type;
	uint32_t thunked_box;
	if (prototype_term_thunk_type(
			&term_db, pure_box_computation, &pure_box_thunk_type
		) != 0 || prototype_term_thunk(
			&term_db, returned_box, &thunked_box
		) != 0) {
		return 83;
	}
	uint32_t pure_box_thunk_type_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_box_thunk_type,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_box_thunk_type,
		universe
	);
	uint32_t thunked_box_operation = add_operation(
		&operation_graph,
		bool_context,
		thunked_box,
		pure_box_thunk_type,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t thunked_box_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		thunked_box_operation,
		bool_context,
		thunked_box_operation,
		thunked_box,
		pure_box_thunk_type
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunked_box_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = returned_box_claim } }
		};
	struct prototype_hott_action_request pure_box_thunk_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = pure_box_thunk_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pure_box_thunk_type_action_id;
	uint32_t pure_box_thunk_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pure_box_thunk_type_action,
			&pure_box_thunk_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, pure_box_thunk_type_action_id,
			&pure_box_thunk_type_result_id
		) != 0 || action_results[pure_box_thunk_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 84;
	}
	struct prototype_hott_action_request thunked_box_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = thunked_box_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = pure_box_thunk_type_action_id
		}
	};
	uint32_t thunked_box_action_id;
	uint32_t thunked_box_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, thunked_box_action,
			&thunked_box_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, thunked_box_action_id,
			&thunked_box_result_id
		) != 0 || action_results[thunked_box_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 85;
	}
	uint32_t thunked_box_witness_claim = action_certificates[
		action_results[thunked_box_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_thunk_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				thunked_box_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_THUNK_WITNESS &&
			judgement.derivations[i].premise_count == 4 &&
			judgement.derivations[i].premises[3].claim_id ==
				returned_box_witness_claim) {
			found_thunk_witness = 1;
			break;
		}
	}
	if (!found_thunk_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 86;
	}
	uint32_t bool_to_pure_bool;
	if (prototype_term_pi(
			&term_db, bool_view, pure_comp, &bool_to_pure_bool
		) != 0) {
		return 87;
	}
	uint32_t pi_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		bool_to_pure_bool,
		bool_context,
		PROTOTYPE_INVALID_ID,
		bool_to_pure_bool,
		universe
	);
	struct prototype_hott_action_request pi_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = pi_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pi_type_action_id;
	uint32_t pi_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pi_type_action, &pi_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, pi_type_action_id, &pi_type_result_id
		) != 0 || action_results[pi_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 88;
	}
	uint32_t function_binding = prototype_term_new_binding(&term_db);
	uint32_t function_context;
	uint32_t function_context_certificate;
	if (function_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			function_binding,
			bool_to_pure_bool,
			PROTOTYPE_INVALID_ID,
			&function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			function_context,
			pi_type_in_bool_context_claim,
			&function_context_certificate
		) != 0) {
		return 89;
	}
	uint32_t function_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			function_context,
			pi_type_action_id,
			&function_bridge
		) != 0) {
		return 90;
	}
	uint32_t function_projection;
	uint32_t function_variable;
	uint32_t function_variable_claim;
	uint32_t bool_variable_in_function_context_claim;
	uint32_t bool_type_in_function_context_claim;
	uint32_t pi_type_in_function_context_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, function_context, &function_projection
		) != 0 || prototype_term_var(
			&term_db, function_binding, &function_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			function_context,
			function_binding,
			bool_to_pure_bool,
			&function_variable_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_variable_claim,
			function_projection,
			&bool_variable_in_function_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_type_in_context_claim,
			function_projection,
			&bool_type_in_function_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pi_type_in_bool_context_claim,
			function_projection,
			&pi_type_in_function_context_claim
		) != 0) {
		return 91;
	}
	uint32_t applied_function;
	if (prototype_term_app(
			&term_db, function_variable, bool_variable, &applied_function
		) != 0) {
		return 92;
	}
	uint32_t app_operation = add_operation(
		&operation_graph,
		function_context,
		applied_function,
		pure_comp,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t app_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		app_operation,
		function_context,
		app_operation,
		applied_function,
		pure_comp
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
			.conclusion_claim_id = app_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = function_variable_claim },
				{ .claim_id = bool_variable_in_function_context_claim }
			}
		};
	uint32_t function_to_empty;
	uint32_t pure_comp_in_function_context_claim;
	if (prototype_substitution_empty(
			&substitution_db, &context_db, function_context, &function_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_comp_is_type,
			function_to_empty,
			&pure_comp_in_function_context_claim
		) != 0) {
		return 93;
	}
	struct prototype_hott_action_request app_result_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = pure_comp_in_function_context_claim,
			.source_bridge_id = function_bridge
		}
	};
	uint32_t app_result_type_action_id;
	uint32_t app_result_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, app_result_type_action,
			&app_result_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, app_result_type_action_id,
			&app_result_type_result_id
		) != 0 || action_results[app_result_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 94;
	}
	struct prototype_hott_action_request app_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = app_claim,
			.source_bridge_id = function_bridge,
			.type_action_request_id = app_result_type_action_id
		}
	};
	uint32_t app_action_id;
	uint32_t app_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, app_action, &app_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, app_action_id, &app_result_id
		) != 0 || action_results[app_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 95;
	}
	uint32_t app_witness_claim = action_certificates[
		action_results[app_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_app_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == app_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_APP_WITNESS &&
			judgement.derivations[i].premise_count == 5) {
			found_app_witness = 1;
			break;
		}
	}
	if (!found_app_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 96;
	}
	(void)function_context_certificate;
	(void)bool_type_in_function_context_claim;
	(void)pi_type_in_function_context_claim;
	uint32_t pure_bool_computation;
	uint32_t bool_to_pure_bool_computation;
	if (prototype_term_computation_type(
			&term_db, empty_row, bool_view, &pure_bool_computation
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			pure_bool_computation,
			&bool_to_pure_bool_computation
		) != 0) {
		return 97;
	}
	uint32_t lambda_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		bool_to_pure_bool_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		bool_to_pure_bool_computation,
		universe
	);
	struct prototype_hott_action_request lambda_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = lambda_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t lambda_type_action_id;
	uint32_t lambda_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, lambda_type_action,
			&lambda_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, lambda_type_action_id,
			&lambda_type_result_id
		) != 0 || action_results[lambda_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 98;
	}
	uint32_t lambda_binding = prototype_term_new_binding(&term_db);
	uint32_t lambda_body_context;
	uint32_t lambda_body_context_certificate;
	if (lambda_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			lambda_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&lambda_body_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			lambda_body_context,
			bool_type_in_context_claim,
			&lambda_body_context_certificate
		) != 0) {
		return 99;
	}
	uint32_t lambda_body_projection;
	uint32_t lambda_binder_variable;
	uint32_t lambda_binder_claim;
	uint32_t outer_bool_in_lambda_body_claim;
	uint32_t bool_type_in_lambda_body_claim;
	uint32_t pure_bool_type_in_lambda_body_claim;
	uint32_t pure_bool_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_bool_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_bool_computation,
		universe
	);
	if (prototype_substitution_projection(
			&substitution_db,
			&context_db,
			lambda_body_context,
			&lambda_body_projection
		) != 0 || prototype_term_var(
			&term_db, lambda_binding, &lambda_binder_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			lambda_body_context,
			lambda_binding,
			bool_view,
			&lambda_binder_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_variable_claim,
			lambda_body_projection,
			&outer_bool_in_lambda_body_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_type_in_context_claim,
			lambda_body_projection,
			&bool_type_in_lambda_body_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_bool_type_in_bool_context_claim,
			lambda_body_projection,
			&pure_bool_type_in_lambda_body_claim
		) != 0) {
		return 100;
	}
	uint32_t returned_outer_bool;
	if (prototype_term_return(
			&term_db, bool_variable, &returned_outer_bool
		) != 0) {
		return 101;
	}
	uint32_t returned_outer_bool_operation = add_operation(
		&operation_graph,
		lambda_body_context,
		returned_outer_bool,
		pure_bool_computation,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t returned_outer_bool_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		returned_outer_bool_operation,
		lambda_body_context,
		returned_outer_bool_operation,
		returned_outer_bool,
		pure_bool_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_outer_bool_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { { .claim_id = outer_bool_in_lambda_body_claim } }
		};
	uint32_t dependent_lambda;
	if (prototype_term_lambda(
			&term_db, lambda_binding, returned_outer_bool, &dependent_lambda
		) != 0) {
		return 102;
	}
	uint32_t lambda_operation = add_operation(
		&operation_graph,
		bool_context,
		dependent_lambda,
		bool_to_pure_bool_computation,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t lambda_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		lambda_operation,
		bool_context,
		lambda_operation,
		dependent_lambda,
		bool_to_pure_bool_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			.conclusion_claim_id = lambda_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = lambda_binder_claim },
				{ .claim_id = returned_outer_bool_claim }
			}
		};
	struct prototype_hott_action_request lambda_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = lambda_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = lambda_type_action_id
		}
	};
	uint32_t lambda_action_id;
	uint32_t lambda_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, lambda_action, &lambda_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, lambda_action_id, &lambda_result_id
		) != 0 || action_results[lambda_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 103;
	}
	uint32_t lambda_witness_claim = action_certificates[
		action_results[lambda_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_lambda_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == lambda_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_LAMBDA_WITNESS &&
			judgement.derivations[i].premise_count == 4) {
			found_lambda_witness = 1;
			break;
		}
	}
	if (!found_lambda_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 104;
	}
	(void)lambda_body_context_certificate;
	(void)bool_type_in_lambda_body_claim;
	(void)pure_bool_type_in_lambda_body_claim;
	uint32_t bool_true_in_context_claim;
	uint32_t bool_false_in_context_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			true_claim,
			bool_projection,
			&bool_true_in_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			false_claim,
			bool_projection,
			&bool_false_in_context_claim
		) != 0) {
		return 105;
	}
	struct prototype_match_case_input value_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_true
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_false
		}
	};
	uint32_t value_match;
	uint32_t motive_binding = prototype_term_new_binding(&term_db);
	uint32_t motive_variable;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_term_match(
			&term_db, bool_variable, value_match_cases, 2, &value_match
		) != 0 || prototype_term_var(
			&term_db, motive_binding, &motive_variable
		) != 0) {
		return 106;
	}
	struct prototype_match_case_input motive_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_view
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_view
		}
	};
	uint32_t motive_match;
	uint32_t motive_lambda;
	uint32_t value_match_classifier;
	if (prototype_term_match(
			&term_db, motive_variable, motive_cases, 2, &motive_match
		) != 0 || prototype_term_lambda(
			&term_db, motive_binding, motive_match, &motive_lambda
		) != 0 || prototype_term_app(
			&term_db, motive_lambda, bool_variable, &value_match_classifier
		) != 0) {
		return 107;
	}
	uint32_t value_match_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		value_match_classifier,
		bool_context,
		PROTOTYPE_INVALID_ID,
		value_match_classifier,
		universe
	);
	uint32_t value_match_operation = add_operation(
		&operation_graph,
		bool_context,
		value_match,
		value_match_classifier,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t value_match_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		value_match_operation,
		bool_context,
		value_match_operation,
		value_match,
		value_match_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = value_match_claim,
			.premise_count = 3,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = value_match_classifier_claim },
				{ .claim_id = bool_true_in_context_claim },
				{ .claim_id = bool_false_in_context_claim }
			}
		};
	struct prototype_hott_action_request value_match_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = value_match_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t value_match_type_action_id;
	uint32_t value_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, value_match_type_action,
			&value_match_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, value_match_type_action_id,
			&value_match_type_result_id
		) != 0 || action_results[value_match_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 108;
	}
	struct prototype_hott_action_request value_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = value_match_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = value_match_type_action_id
		}
	};
	uint32_t value_match_action_id;
	uint32_t value_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, value_match_action,
			&value_match_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, value_match_action_id,
			&value_match_result_id
		) != 0 || action_results[value_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 109;
	}
	uint32_t value_match_witness_claim = action_certificates[
		action_results[value_match_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_match_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				value_match_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_MATCH_WITNESS &&
			judgement.derivations[i].premise_count == 7) {
			found_match_witness = 1;
			break;
		}
	}
	if (!found_match_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 110;
	}
	uint32_t unbox_binding = prototype_term_new_binding(&term_db);
	uint32_t unbox_variable;
	if (unbox_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, unbox_binding, &unbox_variable
		) != 0) {
		return 111;
	}
	struct prototype_case_binder unbox_binder = {
		.binding_id = unbox_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input unbox_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = box_view,
		.constructor_id = 0,
		.binders = &unbox_binder,
		.binder_count = 1,
		.body = unbox_variable
	};
	uint32_t unbox_match;
	if (prototype_term_match(
			&term_db, boxed_bool_variable, &unbox_case, 1, &unbox_match
		) != 0) {
		return 112;
	}
	const struct prototype_match_case* stored_unbox_case =
		&term_db.cases[term_db.terms[unbox_match].as.match.first_case];
	uint32_t stored_unbox_binding = term_db.case_binders[
		stored_unbox_case->first_binder
	].binding_id;
	uint32_t unbox_context;
	uint32_t unbox_context_certificate;
	if (prototype_context_extend(
			&context_db,
			bool_context,
			stored_unbox_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&unbox_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			unbox_context,
			bool_type_in_context_claim,
			&unbox_context_certificate
		) != 0) {
		return 113;
	}
	uint32_t unbox_body_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			unbox_context,
			stored_unbox_binding,
			bool_view,
			&unbox_body_claim
		) != 0) {
		return 114;
	}
	uint32_t unbox_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t unbox_motive_variable;
	uint32_t unbox_motive_case_binding = prototype_term_new_binding(&term_db);
	if (unbox_motive_binding == PROTOTYPE_INVALID_ID ||
		unbox_motive_case_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, unbox_motive_binding, &unbox_motive_variable
		) != 0) {
		return 115;
	}
	struct prototype_case_binder unbox_motive_case_binder = {
		.binding_id = unbox_motive_case_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input unbox_motive_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = box_view,
		.constructor_id = 0,
		.binders = &unbox_motive_case_binder,
		.binder_count = 1,
		.body = bool_view
	};
	uint32_t unbox_motive_match;
	uint32_t unbox_motive_lambda;
	uint32_t unbox_classifier;
	if (prototype_term_match(
			&term_db,
			unbox_motive_variable,
			&unbox_motive_case,
			1,
			&unbox_motive_match
		) != 0 || prototype_term_lambda(
			&term_db,
			unbox_motive_binding,
			unbox_motive_match,
			&unbox_motive_lambda
		) != 0 || prototype_term_app(
			&term_db,
			unbox_motive_lambda,
			boxed_bool_variable,
			&unbox_classifier
		) != 0) {
		return 116;
	}
	uint32_t unbox_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		unbox_classifier,
		bool_context,
		PROTOTYPE_INVALID_ID,
		unbox_classifier,
		universe
	);
	uint32_t unbox_operation = add_operation(
		&operation_graph,
		bool_context,
		unbox_match,
		unbox_classifier,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t unbox_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		unbox_operation,
		bool_context,
		unbox_operation,
		unbox_match,
		unbox_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = unbox_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = unbox_classifier_claim },
				{ .claim_id = unbox_body_claim }
			}
		};
	struct prototype_hott_action_request unbox_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = unbox_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t unbox_type_action_id;
	uint32_t unbox_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, unbox_type_action,
			&unbox_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, unbox_type_action_id,
			&unbox_type_result_id
		) != 0 || action_results[unbox_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 117;
	}
	struct prototype_hott_action_request unbox_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = unbox_claim,
			.source_bridge_id = bool_bridge,
			.type_action_request_id = unbox_type_action_id
		}
	};
	uint32_t unbox_action_id;
	uint32_t unbox_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, unbox_action, &unbox_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, unbox_action_id, &unbox_result_id
		) != 0 || action_results[unbox_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 118;
	}
	(void)unbox_context_certificate;
	uint32_t nat_is_type = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		nat_view,
		empty,
		PROTOTYPE_INVALID_ID,
		nat_view,
		universe
	);
	uint32_t nat_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_variable;
	uint32_t nat_context;
	uint32_t nat_context_certificate;
	if (nat_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_binding, &nat_variable
		) != 0 || prototype_context_extend(
			&context_db,
			empty,
			nat_binding,
			nat_view,
			PROTOTYPE_INVALID_ID,
			&nat_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			nat_context,
			nat_is_type,
			&nat_context_certificate
		) != 0) {
		return 119;
	}
	uint32_t nat_variable_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			nat_context,
			nat_binding,
			nat_view,
			&nat_variable_claim
		) != 0) {
		return 120;
	}
	uint32_t nat_projection;
	uint32_t nat_type_in_context_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, nat_context, &nat_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			nat_is_type,
			nat_projection,
			&nat_type_in_context_claim
		) != 0) {
		return 121;
	}
	uint32_t nat_zero;
	uint32_t nat_succ;
	if (prototype_term_constructor(
			&term_db, nat_view, 0, &nat_zero
		) != 0 || prototype_term_constructor(
			&term_db, nat_view, 1, &nat_succ
		) != 0) {
		return 122;
	}
	uint32_t nat_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_motive_variable;
	uint32_t nat_motive_succ_binding = prototype_term_new_binding(&term_db);
	if (nat_motive_binding == PROTOTYPE_INVALID_ID ||
		nat_motive_succ_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_motive_binding, &nat_motive_variable
		) != 0) {
		return 123;
	}
	struct prototype_case_binder nat_motive_succ_binder = {
		.binding_id = nat_motive_succ_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input nat_motive_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = nat_view
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 1,
			.binders = &nat_motive_succ_binder,
			.binder_count = 1,
			.body = nat_view
		}
	};
	uint32_t nat_motive_match;
	uint32_t nat_motive;
	if (prototype_term_match(
			&term_db,
			nat_motive_variable,
			nat_motive_cases,
			2,
			&nat_motive_match
		) != 0 || prototype_term_lambda(
			&term_db, nat_motive_binding, nat_motive_match, &nat_motive
		) != 0) {
		return 124;
	}
	uint32_t nat_ih_scope = prototype_term_new_ih_scope(&term_db);
	uint32_t nat_succ_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_succ_variable;
	uint32_t nat_ih;
	if (nat_ih_scope == PROTOTYPE_INVALID_ID ||
		nat_succ_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_succ_binding, &nat_succ_variable
		) != 0 || prototype_term_induction_hypothesis(
			&term_db, nat_ih_scope, nat_succ_variable, &nat_ih
		) != 0) {
		return 125;
	}
	struct prototype_case_binder nat_succ_binder = {
		.binding_id = nat_succ_binding,
		.is_recursive = 1
	};
	struct prototype_match_case_input nat_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = nat_zero
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 1,
			.binders = &nat_succ_binder,
			.binder_count = 1,
			.body = nat_ih
		}
	};
	uint32_t nat_match;
	if (prototype_term_match_with_ih_scope(
			&term_db,
			nat_variable,
			nat_match_cases,
			2,
			nat_ih_scope,
			&nat_match
		) != 0 || prototype_term_set_ih_scope_term(
			&term_db, nat_ih_scope, nat_match
		) != 0) {
		return 126;
	}
	const struct prototype_match_case* stored_nat_succ_case = &term_db.cases[
		term_db.terms[nat_match].as.match.first_case + 1
	];
	uint32_t stored_nat_succ_binding = term_db.case_binders[
		stored_nat_succ_case->first_binder
	].binding_id;
	uint32_t nat_succ_context;
	uint32_t nat_succ_context_certificate;
	if (prototype_context_extend(
			&context_db,
			nat_context,
			stored_nat_succ_binding,
			nat_view,
			PROTOTYPE_INVALID_ID,
			&nat_succ_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			nat_succ_context,
			nat_type_in_context_claim,
			&nat_succ_context_certificate
		) != 0) {
		return 127;
	}
	uint32_t nat_succ_variable_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			nat_succ_context,
			stored_nat_succ_binding,
			nat_view,
			&nat_succ_variable_claim
		) != 0) {
		return 128;
	}
	uint32_t nat_classifier;
	uint32_t nat_ih_classifier;
	if (prototype_term_app(
			&term_db, nat_motive, nat_variable, &nat_classifier
		) != 0 || prototype_term_app(
			&term_db,
			nat_motive,
			term_db.terms[stored_nat_succ_case->body].
				as.induction_hypothesis.argument,
			&nat_ih_classifier
		) != 0) {
		return 129;
	}
	uint32_t nat_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		nat_classifier,
		nat_context,
		PROTOTYPE_INVALID_ID,
		nat_classifier,
		universe
	);
	uint32_t nat_ih_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		nat_ih_classifier,
		nat_succ_context,
		PROTOTYPE_INVALID_ID,
		nat_ih_classifier,
		universe
	);
	uint32_t nat_zero_in_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		nat_zero,
		nat_context,
		PROTOTYPE_INVALID_ID,
		nat_zero,
		nat_view
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO,
			.conclusion_claim_id = nat_zero_in_context_claim,
			.premise_count = 0
		};
	uint32_t nat_ih_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		stored_nat_succ_case->body,
		nat_succ_context,
		PROTOTYPE_INVALID_ID,
		stored_nat_succ_case->body,
		nat_ih_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
			.conclusion_claim_id = nat_ih_claim,
			.rule_data.induction.match = nat_match,
			.rule_data.induction.motive = nat_motive,
			.rule_data.induction.case_index = 1,
			.rule_data.induction.field_index = 0,
			.premise_count = 0
		};
	uint32_t nat_match_operation = add_operation(
		&operation_graph,
		nat_context,
		nat_match,
		nat_classifier,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t nat_match_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		nat_match_operation,
		nat_context,
		nat_match_operation,
		nat_match,
		nat_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = nat_match_claim,
			.premise_count = 3,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = nat_classifier_claim },
				{ .claim_id = nat_zero_in_context_claim },
				{ .claim_id = nat_ih_claim }
			}
		};
	struct prototype_hott_action_request nat_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = nat_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t nat_type_action_id;
	uint32_t nat_type_result_id;
	uint32_t nat_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_type_action, &nat_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, nat_type_action_id, &nat_type_result_id
		) != 0 || action_results[nat_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			nat_context,
			nat_type_action_id,
			&nat_bridge
		) != 0) {
		return 130;
	}
	struct prototype_hott_action_request nat_match_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = nat_classifier_claim,
			.source_bridge_id = nat_bridge
		}
	};
	uint32_t nat_match_type_action_id;
	uint32_t nat_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_match_type_action,
			&nat_match_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, nat_match_type_action_id,
			&nat_match_type_result_id
		) != 0 || action_results[nat_match_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 131;
	}
	struct prototype_hott_action_request nat_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = nat_match_claim,
			.source_bridge_id = nat_bridge,
			.type_action_request_id = nat_match_type_action_id
		}
	};
	uint32_t nat_match_action_id;
	uint32_t nat_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_match_action, &nat_match_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, nat_match_action_id, &nat_match_result_id
		) != 0 || action_results[nat_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 132;
	}
	int found_induction_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_INDUCTION_HYPOTHESIS_WITNESS) {
			found_induction_witness = 1;
			break;
		}
	}
	if (!found_induction_witness) {
		return 133;
	}
	(void)nat_context_certificate;
	(void)nat_succ_context_certificate;
	(void)nat_succ_variable_claim;
	(void)nat_ih_classifier_claim;

	uint32_t second_bool_binding = prototype_term_new_binding(&term_db);
	uint32_t second_bool_context;
	uint32_t second_bool_context_certificate;
	if (second_bool_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			second_bool_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&second_bool_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			second_bool_context,
			bool_type_in_context_claim,
			&second_bool_context_certificate
		) != 0) {
		return 61;
	}
	uint32_t second_bool_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			second_bool_context,
			contextual_bool_type_action_id,
			&second_bool_bridge
		) != 0 || contexts[bridges[second_bool_bridge].bridge_context_id].depth !=
			contexts[bridges[bool_bridge].bridge_context_id].depth + 3 ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 62;
	}
	uint32_t bool_observation_type;
	uint32_t bool_observation_type_claim;
	struct prototype_hott_type_former_descriptor observation_descriptor;
	if (prototype_term_observation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_variable,
			bool_variable,
			&bool_observation_type
		) != 0 || prototype_judgement_add_observation_type_formation(
			&judgement,
			&term_db,
			bool_context,
			bool_observation_type,
			universe,
			bool_type_in_context_claim,
			bool_type_in_context_claim,
			bool_variable_claim,
			bool_variable_claim,
			&bool_observation_type_claim
		) != 0 || prototype_hott_type_former_descriptor_query(
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_observation_type_claim,
			&observation_descriptor
		) != 0 || !observation_descriptor.admitted ||
		observation_descriptor.type_action_rule !=
			PROTOTYPE_HOTT_TYPE_ACTION_RULE_OBSERVATION_HIGHER ||
		observation_descriptor.capabilities.type_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		observation_descriptor.capabilities.term_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		observation_descriptor.capabilities.ordinary_reindex !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		observation_descriptor.capabilities.resource_hook !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		observation_descriptor.capabilities.artifact !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		(observation_descriptor.child_role_mask &
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_TERM_ACTION)) == 0) {
		return 67;
	}
	uint32_t proof_binding = prototype_term_new_binding(&term_db);
	uint32_t proof_context;
	uint32_t proof_context_certificate;
	if (proof_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			proof_binding,
			bool_observation_type,
			PROTOTYPE_INVALID_ID,
			&proof_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			proof_context,
			bool_observation_type_claim,
			&proof_context_certificate
		) != 0) {
		return 68;
	}
	struct prototype_hott_action_request observation_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = bool_observation_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t observation_type_action_id;
	uint32_t observation_type_result_id;
	uint32_t proof_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, observation_type_action,
			&observation_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, observation_type_action_id,
			&observation_type_result_id
		) != 0 || action_results[observation_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			proof_context,
			observation_type_action_id,
			&proof_bridge
		) != 0) {
		return 69;
	}
	uint32_t proof_projection;
	uint32_t observation_type_in_proof_claim;
	uint32_t proof_variable_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, proof_context, &proof_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_observation_type_claim,
			proof_projection,
			&observation_type_in_proof_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			proof_context,
			proof_binding,
			bool_observation_type,
			&proof_variable_claim
		) != 0) {
		return 70;
	}
	struct prototype_hott_action_request higher_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = observation_type_in_proof_claim,
			.source_bridge_id = proof_bridge
		}
	};
	uint32_t higher_type_action_id;
	uint32_t higher_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, higher_type_action,
			&higher_type_action_id
		) != 0 || prototype_hott_execute_type_action(
			&action_db, &kernel_builder, &bridge_db, higher_type_action_id,
			&higher_type_result_id
		) != 0 || action_results[higher_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 71;
	}
	struct prototype_hott_action_request proof_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = proof_variable_claim,
			.source_bridge_id = proof_bridge,
			.type_action_request_id = higher_type_action_id
		}
	};
	uint32_t proof_variable_action_id;
	uint32_t proof_variable_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, proof_variable_action,
			&proof_variable_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, proof_variable_action_id,
			&proof_variable_result_id
		) != 0 || action_results[proof_variable_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		terms[action_certificates[
			action_results[proof_variable_result_id].certificate_id
		].data.term.witness_term_id].tag != PROTOTYPE_TERM_VAR) {
		return 72;
	}
	uint32_t repeated_bool_extension;
	uint32_t repeated_bool_extension_certificate;
	if (prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			bool_identity,
			second_bool_context,
			bool_variable,
			bool_view,
			&repeated_bool_extension
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			repeated_bool_extension,
			bool_variable_claim,
			&repeated_bool_extension_certificate
		) != 0) {
		return 63;
	}
	struct prototype_hott_action_request repeated_bool_extension_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = repeated_bool_extension,
			.source_bridge_id = bool_bridge,
			.target_bridge_id = second_bool_bridge
		}
	};
	uint32_t repeated_bool_extension_action_id;
	uint32_t repeated_bool_extension_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, repeated_bool_extension_action,
			&repeated_bool_extension_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, repeated_bool_extension_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 64,
			&repeated_bool_extension_result_id
		) != 0 || action_results[repeated_bool_extension_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 64;
	}
	uint32_t beta_binding = prototype_term_new_binding(&term_db);
	uint32_t beta_variable;
	uint32_t beta_identity_lambda;
	uint32_t beta_false;
	uint32_t beta_false_extension;
	struct prototype_term_conversion_result exhausted_comparison;
	struct prototype_term_conversion_result completed_comparison;
	if (beta_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, beta_binding, &beta_variable
		) != 0 || prototype_term_lambda(
			&term_db, beta_binding, beta_variable, &beta_identity_lambda
		) != 0 || prototype_term_app(
			&term_db, beta_identity_lambda, bool_false, &beta_false
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			beta_false,
			bool_view,
			&beta_false_extension
		) != 0 || prototype_substitution_compare_pointwise(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			false_extension,
			beta_false_extension,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			0,
			&exhausted_comparison
		) != 0 || exhausted_comparison.status !=
			PROTOTYPE_TERM_CONVERSION_EXHAUSTED ||
		prototype_substitution_compare_pointwise(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			false_extension,
			beta_false_extension,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			8,
			&completed_comparison
		) != 0 || completed_comparison.status !=
			PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 65;
	}
	struct prototype_hott_observation_execution distinct_execution;
	if (prototype_hott_observation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_distinct_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32,
			&distinct_execution
		) != 0 || distinct_execution.type_action_request_id == PROTOTYPE_INVALID_ID ||
		distinct_execution.type_action_result_id == PROTOTYPE_INVALID_ID ||
		action_results[distinct_execution.type_action_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		distinct_execution.term_action_request_id != PROTOTYPE_INVALID_ID ||
		distinct_execution.term_action_result_id != PROTOTYPE_INVALID_ID) {
		return 134;
	}
	struct prototype_hott_observation_execution diagonal_execution;
	if (prototype_hott_observation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_diagonal_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			64,
			&diagonal_execution
		) != 0 || diagonal_execution.type_action_request_id !=
			bool_type_action_id || diagonal_execution.type_action_result_id !=
			bool_type_result_id || diagonal_execution.term_action_request_id !=
			bool_term_action_id || diagonal_execution.term_action_result_id !=
			bool_term_result_id) {
		return 66;
	}
	uint32_t atomic_observation_type;
	uint32_t atomic_observation_type_claim;
	if (prototype_term_observation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_false,
			bool_true,
			&atomic_observation_type
		) != 0 || prototype_judgement_add_observation_type_formation(
			&judgement,
			&term_db,
			empty,
			atomic_observation_type,
			universe,
			bool_is_type,
			bool_is_type,
			false_claim,
			true_claim,
			&atomic_observation_type_claim
		) != 0) {
		return 135;
	}
	struct prototype_hott_action_request atomic_failure_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = atomic_observation_type_claim,
			.source_bridge_id = bridge
		}
	};
	uint32_t atomic_failure_request_id;
	uint32_t ignored_result_id;
	size_t certificate_count_before_failure = action_db.certificate_count;
	size_t saved_result_capacity = action_db.result_capacity;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, atomic_failure_request,
			&atomic_failure_request_id
		) != 0) {
		return 135;
	}
	action_db.result_capacity = action_db.result_count;
	int atomic_failure_status = prototype_hott_execute_type_action(
		&action_db, &kernel_builder, &bridge_db, atomic_failure_request_id, &ignored_result_id
	);
	action_db.result_capacity = saved_result_capacity;
	if (atomic_failure_status == 0 || action_db.certificate_count !=
		certificate_count_before_failure || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 135;
	}

	free(type_db.representations);
	return 0;
}
