#include "a_program/frontend/lowering.h"
#include "hott.h"
#include "calculus.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TERM_CAPACITY 8192
#define CLAIM_CAPACITY 2048
#define CONTEXT_CAPACITY 1024
#define SUBSTITUTION_CAPACITY 2048
#define OPERATION_CAPACITY 64
#define GOAL_CAPACITY 64
#define CANDIDATE_CAPACITY 128
#define PREMISE_CAPACITY 128

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[256];
static int case_labels[256];
static struct prototype_case_binder case_binders[256];
static struct prototype_ih_scope ih_scopes[64];
static struct prototype_type_declaration type_declarations[16];
static struct prototype_type_constructor_declaration constructors[32];
static struct prototype_type_parameter_declaration parameters[8];
static uint32_t field_types[64];
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
static struct prototype_cwf_certificate cwf_certificates[1024];
static struct prototype_operation_node operations[OPERATION_CAPACITY];
static struct prototype_operation_match_case operation_cases[128];
static struct prototype_hott_bridge bridges[128];
static struct prototype_hott_bridge_certificate bridge_certificates[128];
static struct prototype_hott_relation_goal goals[GOAL_CAPACITY];
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
static struct prototype_hott_action_request action_requests[256];
static struct prototype_hott_action_certificate action_certificates[256];
static struct prototype_hott_action_result action_results[256];

static int write_identity_root_artifact(
	const char* path,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* term_db,
	const struct prototype_type_declaration_db* type_db,
	const struct prototype_judgement_db* judgement,
	const struct prototype_context_db* context_db,
	const struct prototype_substitution_db* substitution_db,
	const struct prototype_operation_graph* operation_graph
) {
	if (!path || !interface || !term_db || !type_db || !judgement || !context_db ||
		!substitution_db || !operation_graph) {
		return -1;
	}
	int symbol_ids[1024];
	uint32_t symbol_hashes[1024];
	char* symbol_strings[512];
	struct symbol_table symbols;
	symbol_table_init(
		&symbols,
		symbol_ids,
		symbol_hashes,
		1024,
		symbol_strings,
		512
	);
	for (uint32_t i = 0; i <= 400; ++i) {
		char name[32];
		int length = snprintf(name, sizeof(name), "fixture_%u", i);
		if (length < 0 || (size_t)length >= sizeof(name) ||
			symbol_intern(&symbols, name, (size_t)length) != (int)i) {
			symbol_table_free(&symbols);
			return -1;
		}
	}
	struct prototype_universe_node universe_nodes[1];
	struct prototype_universe_edge universe_edges[1];
	struct prototype_universe_level universe_levels[1];
	struct prototype_universe_constraint universe_constraints[1];
	struct prototype_universe_db universe;
	prototype_universe_db_init(
		&universe,
		universe_nodes,
		1,
		universe_edges,
		1,
		universe_levels,
		1,
		universe_constraints,
		1
	);
	universe.solved = 1;
	struct prototype_compile_metadata metadata;
	prototype_compile_metadata_init(
		&metadata,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0
	);
	metadata.contexts = *context_db;
	metadata.substitutions = *substitution_db;
	metadata.operations = operation_graph->operations;
	metadata.operation_count = operation_graph->operation_count;
	metadata.operation_capacity = operation_graph->operation_capacity;
	metadata.operation_cases = operation_graph->cases;
	metadata.operation_case_count = operation_graph->case_count;
	metadata.operation_case_capacity = operation_graph->case_capacity;
	metadata.operation_fold_clauses = operation_graph->fold_clauses;
	metadata.operation_fold_clause_count = operation_graph->fold_clause_count;
	metadata.operation_fold_clause_capacity = operation_graph->fold_clause_capacity;
	FILE* stream = fopen(path, "w");
	if (!stream) {
		symbol_table_free(&symbols);
		return -1;
	}
	int status = prototype_artifact_write_text(
		stream,
		&symbols,
		interface,
		term_db,
		type_db,
		judgement,
		&universe,
		NULL,
		&metadata
	);
	long written_size = ftell(stream);
	if (status != 0) {
		fprintf(
			stderr,
			"identity artifact writer failed: status=%d size=%ld error=%d\n",
			status,
			written_size,
			ferror(stream)
		);
	}
	if (status == 0 && (ferror(stream) || fflush(stream) != 0 || written_size <= 0)) {
		fprintf(
			stderr,
			"identity artifact writer produced no output: status=%d size=%ld error=%d\n",
			status,
			written_size,
			ferror(stream)
		);
		status = -1;
	}
	if (fclose(stream) != 0) {
		status = -1;
	}
	symbol_table_free(&symbols);
	return status;
}

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
		.closure_rank = 0
	};
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	return id;
}

static struct prototype_judgement_derivation fixture_derivation(
	int proof_kind,
	uint32_t conclusion_claim_id
) {
	struct prototype_judgement_derivation derivation;
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = proof_kind;
	derivation.conclusion_claim_id = conclusion_claim_id;
	memset(&derivation.rule_data, 0xff, sizeof(derivation.rule_data));
	derivation.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	derivation.semantic_action_id = PROTOTYPE_INVALID_ID;
	derivation.hash_next = PROTOTYPE_INVALID_ID;
	return derivation;
}

static struct prototype_judgement_premise_edge fixture_premise(
	uint32_t claim_id
) {
	return (struct prototype_judgement_premise_edge) {
		.claim_id = claim_id,
		.scoped_proposition_id = PROTOTYPE_INVALID_ID,
		.semantic_action_kind = PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID,
		.semantic_action_id = PROTOTYPE_INVALID_ID
	};
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
		.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID,
		.fold_return_binder_id = PROTOTYPE_INVALID_ID,
		.fold_return_operation = PROTOTYPE_INVALID_ID
	};
	return id;
}

static int apply_pure_thunk_function(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_value,
	uint32_t argument,
	uint32_t* p_result_value
) {
	uint32_t force;
	uint32_t app;
	uint32_t normalized;
	if (!terms || !type_declarations || !p_result_value ||
		prototype_term_force(terms, function_value, &force) != 0 ||
		prototype_term_app(terms, force, argument, &app) != 0 ||
		prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			app,
			&normalized
		) != 0 || normalized >= terms->term_count ||
		terms->terms[normalized].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	*p_result_value = terms->terms[normalized].as.return_term.value;
	return 0;
}

static int check_bool_function_extensional_identity(
	struct prototype_term_db* term_db,
	struct prototype_type_declaration_db* type_db,
	struct prototype_context_db* context_db,
	struct prototype_substitution_db* substitution_db,
	struct prototype_judgement_db* judgement,
	struct prototype_operation_graph* operation_graph,
	uint32_t empty_context,
	uint32_t universe,
	uint32_t bool_type,
	uint32_t bool_false,
	uint32_t bool_true,
	uint32_t bool_identity_type_id,
	uint32_t thunk_bool_identity_type_id,
	uint32_t thunk_bool_identity_former,
	uint32_t function_type,
	const struct prototype_hott_identity_type_computation_certificate* certificate,
	uint32_t* p_identity_family_claim,
	uint32_t* p_witness_term,
	uint32_t* p_witness_claim
) {
	if (!term_db || !type_db || !context_db || !substitution_db || !judgement ||
		!operation_graph || !certificate || !p_identity_family_claim ||
		!p_witness_term || !p_witness_claim) {
		return 1;
	}
	uint32_t left_binder = prototype_term_new_binding(term_db);
	uint32_t right_binder = prototype_term_new_binding(term_db);
	uint32_t left_var;
	uint32_t right_var;
	uint32_t left_return;
	uint32_t false_return;
	uint32_t true_return;
	uint32_t left_lambda;
	uint32_t left_value;
	uint32_t right_match;
	uint32_t right_lambda;
	uint32_t right_value;
	if (left_binder == PROTOTYPE_INVALID_ID ||
		right_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			term_db, left_binder, &left_var
		) != 0 || prototype_term_var(
			term_db, right_binder, &right_var
		) != 0 || prototype_term_return(
			term_db, left_var, &left_return
		) != 0 || prototype_term_return(
			term_db, bool_false, &false_return
		) != 0 || prototype_term_return(
			term_db, bool_true, &true_return
		) != 0 || prototype_term_lambda(
			term_db, left_binder, left_return, &left_lambda
		) != 0 || prototype_term_thunk(
			term_db, left_lambda, &left_value
		) != 0) {
		return 2;
	}
	struct prototype_match_case_input right_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_type,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = false_return
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_type,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = true_return
		}
	};
	if (prototype_term_match(
			term_db, right_var, right_cases, 2, &right_match
		) != 0 || prototype_term_lambda(
			term_db, right_binder, right_match, &right_lambda
		) != 0 || prototype_term_thunk(
			term_db, right_lambda, &right_value
		) != 0) {
		return 2;
	}
	struct prototype_term_conversion_result conversion;
	if (prototype_term_compare_for_conversion(
			term_db,
			type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			left_value,
			right_value,
			64,
			&conversion
		) != 0 || conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 3;
	}
	uint32_t concrete_identity;
	if (prototype_term_graph_substitute_bound_var(
			term_db,
			type_db,
			certificate->identity_type_term_id,
			certificate->left_endpoint_binding_id,
			left_value,
			&concrete_identity
		) != 0 || prototype_term_graph_substitute_bound_var(
			term_db,
			type_db,
			concrete_identity,
			certificate->right_endpoint_binding_id,
			right_value,
			&concrete_identity
		) != 0 || concrete_identity >= term_db->term_count ||
		term_db->terms[concrete_identity].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 4;
	}
	uint32_t pi0 = term_db->terms[concrete_identity].as.thunk_type.computation;
	uint32_t x0_binding;
	uint32_t x0_codomain;
	if (pi0 >= term_db->term_count || term_db->terms[pi0].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			term_db,
			term_db->terms[pi0].as.pi.codomain_family,
			&x0_binding,
			&x0_codomain
		) != 0 || x0_binding != certificate->pointwise_left_input_binding_id ||
		x0_codomain >= term_db->term_count ||
		term_db->terms[x0_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 5;
	}
	uint32_t thunk1 = term_db->terms[x0_codomain].as.computation_type.result;
	uint32_t pi1 = thunk1 < term_db->term_count &&
		term_db->terms[thunk1].tag == PROTOTYPE_TERM_THUNK_TYPE ?
		term_db->terms[thunk1].as.thunk_type.computation : PROTOTYPE_INVALID_ID;
	uint32_t x1_binding;
	uint32_t x1_codomain;
	if (pi1 >= term_db->term_count || term_db->terms[pi1].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			term_db,
			term_db->terms[pi1].as.pi.codomain_family,
			&x1_binding,
			&x1_codomain
		) != 0 || x1_binding != certificate->pointwise_right_input_binding_id ||
		x1_codomain >= term_db->term_count ||
		term_db->terms[x1_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 6;
	}
	uint32_t thunk2 = term_db->terms[x1_codomain].as.computation_type.result;
	uint32_t pi2 = thunk2 < term_db->term_count &&
		term_db->terms[thunk2].tag == PROTOTYPE_TERM_THUNK_TYPE ?
		term_db->terms[thunk2].as.thunk_type.computation : PROTOTYPE_INVALID_ID;
	uint32_t xr_binding;
	uint32_t xr_codomain;
	if (pi2 >= term_db->term_count || term_db->terms[pi2].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			term_db,
			term_db->terms[pi2].as.pi.codomain_family,
			&xr_binding,
			&xr_codomain
		) != 0 || xr_binding != certificate->pointwise_input_identity_binding_id ||
		xr_codomain >= term_db->term_count ||
		term_db->terms[xr_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 7;
	}
	uint32_t x0;
	uint32_t x1;
	uint32_t xr;
	uint32_t x0_context;
	uint32_t x1_context;
	uint32_t xr_context;
	uint32_t input_identity = term_db->terms[pi2].as.pi.domain;
	if (prototype_term_var(term_db, x0_binding, &x0) != 0 ||
		prototype_term_var(term_db, x1_binding, &x1) != 0 ||
		prototype_term_var(term_db, xr_binding, &xr) != 0 ||
		prototype_context_extend(
			context_db, empty_context, x0_binding, bool_type,
			PROTOTYPE_INVALID_ID, &x0_context
		) != 0 || prototype_context_extend(
			context_db, x0_context, x1_binding, bool_type,
			PROTOTYPE_INVALID_ID, &x1_context
		) != 0 || prototype_context_extend(
			context_db, x1_context, xr_binding, input_identity,
			PROTOTYPE_INVALID_ID, &xr_context
		) != 0) {
		return 8;
	}
	uint32_t false_identity_arguments[2] = { bool_false, bool_false };
	uint32_t true_identity_arguments[2] = { bool_true, bool_true };
	uint32_t false_identity;
	uint32_t true_identity;
	uint32_t false_identity_witness;
	uint32_t true_identity_witness;
	uint32_t false_thunk;
	uint32_t true_thunk;
	uint32_t false_computation_identity;
	uint32_t true_computation_identity;
	uint32_t false_computation_identity_arguments[2];
	uint32_t true_computation_identity_arguments[2];
	uint32_t false_computation_witness;
	uint32_t true_computation_witness;
	uint32_t false_witness_spine[4];
	uint32_t true_witness_spine[4];
	uint32_t false_witness_return;
	uint32_t true_witness_return;
	uint32_t false_branch_computation_identity;
	uint32_t true_branch_computation_identity;
	if (prototype_term_type_instance_make(
			term_db, type_db, bool_identity_type_id,
			false_identity_arguments, 2, &false_identity
		) != 0 || prototype_term_type_instance_make(
			term_db, type_db, bool_identity_type_id,
			true_identity_arguments, 2, &true_identity
		) != 0 || prototype_term_constructor(
			term_db, false_identity, 0, &false_identity_witness
		) != 0 || prototype_term_constructor(
			term_db, true_identity, 1, &true_identity_witness
		) != 0 || prototype_term_thunk(
			term_db, false_return, &false_thunk
		) != 0 || prototype_term_thunk(
			term_db, true_return, &true_thunk
		) != 0) {
		return 9;
	}
	false_computation_identity_arguments[0] = false_thunk;
	false_computation_identity_arguments[1] = false_thunk;
	true_computation_identity_arguments[0] = true_thunk;
	true_computation_identity_arguments[1] = true_thunk;
	if (prototype_term_type_instance_make(
			term_db, type_db, thunk_bool_identity_type_id,
			false_computation_identity_arguments, 2,
			&false_computation_identity
		) != 0 || prototype_term_type_instance_make(
			term_db, type_db, thunk_bool_identity_type_id,
			true_computation_identity_arguments, 2,
			&true_computation_identity
		) != 0 || prototype_term_constructor(
			term_db, thunk_bool_identity_former, 0, &false_witness_spine[0]
		) != 0 || prototype_term_app(
			term_db, false_witness_spine[0], bool_false,
			&false_witness_spine[1]
		) != 0 || prototype_term_app(
			term_db, false_witness_spine[1], bool_false,
			&false_witness_spine[2]
		) != 0 || prototype_term_app(
			term_db, false_witness_spine[2], false_identity_witness,
			&false_witness_spine[3]
		) != 0 || prototype_term_constructor(
			term_db, thunk_bool_identity_former, 0, &true_witness_spine[0]
		) != 0 || prototype_term_app(
			term_db, true_witness_spine[0], bool_true,
			&true_witness_spine[1]
		) != 0 || prototype_term_app(
			term_db, true_witness_spine[1], bool_true,
			&true_witness_spine[2]
		) != 0 || prototype_term_app(
			term_db, true_witness_spine[2], true_identity_witness,
			&true_witness_spine[3]
		) != 0) {
		return 10;
	}
	false_computation_witness = false_witness_spine[3];
	true_computation_witness = true_witness_spine[3];
	if (prototype_term_return(
			term_db, false_computation_witness, &false_witness_return
		) != 0 || prototype_term_return(
			term_db, true_computation_witness, &true_witness_return
		) != 0) {
		return 10;
	}
	if (prototype_term_computation_type(
			term_db,
			term_db->terms[xr_codomain].as.computation_type.label,
			false_computation_identity,
			&false_branch_computation_identity
		) != 0 || prototype_term_computation_type(
			term_db,
			term_db->terms[xr_codomain].as.computation_type.label,
			true_computation_identity,
			&true_branch_computation_identity
		) != 0) {
		return 10;
	}
	struct prototype_match_case_input proof_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = input_identity,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = false_witness_return
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = input_identity,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = true_witness_return
		}
	};
	uint32_t proof_endpoint;
	uint32_t proof_return2;
	uint32_t proof_lambda2;
	uint32_t proof_thunk2;
	uint32_t proof_return1;
	uint32_t proof_lambda1;
	uint32_t proof_thunk1;
	uint32_t proof_return0;
	uint32_t proof_lambda0;
	uint32_t proof_value;
	if (prototype_term_match(
			term_db, xr, proof_cases, 2, &proof_endpoint
		) != 0) {
		return 11;
	}
	proof_return2 = proof_endpoint;
	if (prototype_term_lambda(
			term_db, xr_binding, proof_return2, &proof_lambda2
		) != 0 || prototype_term_thunk(
			term_db, proof_lambda2, &proof_thunk2
		) != 0 || prototype_term_return(
			term_db, proof_thunk2, &proof_return1
		) != 0 || prototype_term_lambda(
			term_db, x1_binding, proof_return1, &proof_lambda1
		) != 0 || prototype_term_thunk(
			term_db, proof_lambda1, &proof_thunk1
		) != 0 || prototype_term_return(
			term_db, proof_thunk1, &proof_return0
		) != 0 || prototype_term_lambda(
			term_db, x0_binding, proof_return0, &proof_lambda0
		) != 0 || prototype_term_thunk(
			term_db, proof_lambda0, &proof_value
		) != 0) {
		return 11;
	}
	uint32_t bool_type_claim;
	uint32_t function_type_claim;
	uint32_t concrete_identity_claim;
	if (prototype_judgement_add_type_formation_claim(
		judgement, term_db, type_db, empty_context, bool_type, universe,
			&bool_type_claim
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			term_db,
			type_db,
			context_db,
			substitution_db,
			empty_context,
			function_type,
			universe,
			&function_type_claim
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			term_db,
			type_db,
			context_db,
			substitution_db,
			empty_context,
			concrete_identity,
			universe,
			&concrete_identity_claim
		) != 0) {
		return 12;
	}
	uint32_t false_claim;
	uint32_t true_claim;
	uint32_t false_identity_claim;
	uint32_t true_identity_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			judgement, term_db, type_db, empty_context, bool_false, bool_type,
			&false_claim
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			judgement, term_db, type_db, empty_context, bool_true, bool_type,
			&true_claim
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			judgement, term_db, type_db, empty_context, false_identity_witness,
			false_identity, &false_identity_claim
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			judgement, term_db, type_db, empty_context, true_identity_witness,
			true_identity, &true_identity_claim
		) != 0) {
		return 13;
	}
	uint32_t projection;
	if (prototype_substitution_projection_path(
			substitution_db, context_db, xr_context, empty_context, &projection
		) != 0) {
		return 14;
	}
	uint32_t false_in_context;
	uint32_t true_in_context;
	uint32_t false_identity_in_context;
	uint32_t true_identity_in_context;
	if (prototype_judgement_add_reindexed_claim(
			judgement, term_db, type_db, context_db, substitution_db,
			false_claim, projection, &false_in_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, term_db, type_db, context_db, substitution_db,
			true_claim, projection, &true_in_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, term_db, type_db, context_db, substitution_db,
			false_identity_claim, projection, &false_identity_in_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, term_db, type_db, context_db, substitution_db,
			true_identity_claim, projection, &true_identity_in_context
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, term_db, context_db, x0_context, x0_binding, bool_type,
			&(uint32_t){0}
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, term_db, context_db, x1_context, x1_binding, bool_type,
			&(uint32_t){0}
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, term_db, context_db, xr_context, xr_binding, input_identity,
			&(uint32_t){0}
		) != 0) {
		return 15;
	}
	if (operation_graph->operation_count > operation_graph->operation_capacity ||
		operation_graph->operation_capacity - operation_graph->operation_count < 16 ||
		operation_graph->case_count > operation_graph->case_capacity ||
		operation_graph->case_capacity - operation_graph->case_count < 2) {
		return 27;
	}
	uint32_t scrutinee_operation = add_operation(
		operation_graph, xr_context, xr, input_identity,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_argument_operation = add_operation(
		operation_graph, xr_context, bool_false, bool_type,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_identity_argument_operation = add_operation(
		operation_graph, xr_context, false_identity_witness, false_identity,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_constructor_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[0], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_app1_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[1], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_app2_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[2], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_witness_operation = add_operation(
		operation_graph, xr_context, false_computation_witness,
		false_computation_identity, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t false_operation = add_operation(
		operation_graph, xr_context, false_witness_return,
		false_branch_computation_identity, PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t true_argument_operation = add_operation(
		operation_graph, xr_context, bool_true, bool_type,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_identity_argument_operation = add_operation(
		operation_graph, xr_context, true_identity_witness, true_identity,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_constructor_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[0], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_app1_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[1], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_app2_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[2], PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_witness_operation = add_operation(
		operation_graph, xr_context, true_computation_witness,
		true_computation_identity, PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t true_operation = add_operation(
		operation_graph, xr_context, true_witness_return,
		true_branch_computation_identity, PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t proof_operation = add_operation(
		operation_graph, xr_context, proof_endpoint, PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t first_proof_case = operation_graph->case_count;
	operation_graph->operations[scrutinee_operation].tag = PROTOTYPE_OPERATION_VAR;
	operation_graph->operations[false_argument_operation].tag =
		PROTOTYPE_OPERATION_CONSTRUCTOR;
	operation_graph->operations[false_identity_argument_operation].tag =
		PROTOTYPE_OPERATION_CONSTRUCTOR;
	operation_graph->operations[false_constructor_operation].tag =
		PROTOTYPE_OPERATION_ATOM;
	operation_graph->operations[false_app1_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[false_app1_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[false_app1_operation].function =
		false_constructor_operation;
	operation_graph->operations[false_app1_operation].argument =
		false_argument_operation;
	operation_graph->operations[false_app2_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[false_app2_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[false_app2_operation].function = false_app1_operation;
	operation_graph->operations[false_app2_operation].argument =
		false_argument_operation;
	operation_graph->operations[false_witness_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[false_witness_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[false_witness_operation].function = false_app2_operation;
	operation_graph->operations[false_witness_operation].argument =
		false_identity_argument_operation;
	operation_graph->operations[false_operation].tag = PROTOTYPE_OPERATION_RETURN;
	operation_graph->operations[false_operation].argument = false_witness_operation;
	operation_graph->operations[true_argument_operation].tag =
		PROTOTYPE_OPERATION_CONSTRUCTOR;
	operation_graph->operations[true_identity_argument_operation].tag =
		PROTOTYPE_OPERATION_CONSTRUCTOR;
	operation_graph->operations[true_constructor_operation].tag =
		PROTOTYPE_OPERATION_ATOM;
	operation_graph->operations[true_app1_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[true_app1_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[true_app1_operation].function = true_constructor_operation;
	operation_graph->operations[true_app1_operation].argument = true_argument_operation;
	operation_graph->operations[true_app2_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[true_app2_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[true_app2_operation].function = true_app1_operation;
	operation_graph->operations[true_app2_operation].argument = true_argument_operation;
	operation_graph->operations[true_witness_operation].tag = PROTOTYPE_OPERATION_APP;
	operation_graph->operations[true_witness_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->operations[true_witness_operation].function = true_app2_operation;
	operation_graph->operations[true_witness_operation].argument =
		true_identity_argument_operation;
	operation_graph->operations[true_operation].tag = PROTOTYPE_OPERATION_RETURN;
	operation_graph->operations[true_operation].argument = true_witness_operation;
	operation_graph->operations[proof_operation].tag = PROTOTYPE_OPERATION_MATCH;
	operation_graph->operations[proof_operation].scrutinee = scrutinee_operation;
	operation_graph->operations[proof_operation].first_case = first_proof_case;
	operation_graph->operations[proof_operation].case_count = 2;
	operation_graph->cases[first_proof_case] = (struct prototype_operation_match_case) {
		.body_operation = false_operation,
		.context_id = xr_context,
		.constructor_owner = input_identity,
		.constructor_id = 0,
		.case_label_symbol_id = -1,
		.binder_count = 0
	};
	operation_graph->cases[first_proof_case + 1] =
		(struct prototype_operation_match_case) {
		.body_operation = true_operation,
		.context_id = xr_context,
		.constructor_owner = input_identity,
		.constructor_id = 1,
		.case_label_symbol_id = -1,
		.binder_count = 0
	};
	operation_graph->case_count += 2;
	struct prototype_judgement_proposition delta_propositions[256];
	struct prototype_judgement_derivation_candidate delta_candidates[256];
	struct prototype_judgement_candidate_premise delta_premises[
		256 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result delta_motives[64];
	struct prototype_judgement_computation_constraint delta_constraints[64];
	struct prototype_judgement_effect_row_constraint delta_effect_rows[64];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta,
		judgement,
		delta_propositions,
		delta_candidates,
		256,
		delta_premises,
		256 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		delta_motives,
		64,
		delta_constraints,
		64,
		delta_effect_rows,
		64
	);
	prototype_judgement_delta_set_context_store(
		&delta, context_db, substitution_db
	);
	prototype_judgement_delta_set_operation_store(
		&delta,
		operation_graph->operations,
		operation_graph->operation_count,
		operation_graph->cases,
		operation_graph->case_count
	);
	prototype_judgement_delta_set_context(&delta, xr_context);
	const struct prototype_type_declaration* thunk_identity_declaration =
		&type_db->type_declarations[thunk_bool_identity_type_id];
	const struct prototype_type_constructor_declaration* thunk_identity_constructor =
		&type_db->constructor_declarations[
			thunk_identity_declaration->first_constructor
		];
	uint32_t false_app1_classifier;
	uint32_t false_app2_classifier;
	uint32_t true_app1_classifier;
	uint32_t true_app2_classifier;
	if (prototype_judgement_delta_app_elim_classifier(
			&delta, term_db, type_db,
			thunk_identity_constructor->curried_classifier_cache,
			bool_false, bool_type, &false_app1_classifier
		) != 0 || prototype_judgement_delta_app_elim_classifier(
			&delta, term_db, type_db, false_app1_classifier,
			bool_false, bool_type, &false_app2_classifier
		) != 0 || prototype_judgement_delta_app_elim_classifier(
			&delta, term_db, type_db,
			thunk_identity_constructor->curried_classifier_cache,
			bool_true, bool_type, &true_app1_classifier
		) != 0 || prototype_judgement_delta_app_elim_classifier(
			&delta, term_db, type_db, true_app1_classifier,
			bool_true, bool_type, &true_app2_classifier
		) != 0) {
		return 15;
	}
	operation_graph->operations[false_constructor_operation].classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->operations[false_constructor_operation].known_classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->operations[false_app1_operation].classifier =
		false_app1_classifier;
	operation_graph->operations[false_app1_operation].known_classifier =
		false_app1_classifier;
	operation_graph->operations[false_app2_operation].classifier =
		false_app2_classifier;
	operation_graph->operations[false_app2_operation].known_classifier =
		false_app2_classifier;
	operation_graph->operations[true_constructor_operation].classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->operations[true_constructor_operation].known_classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->operations[true_app1_operation].classifier = true_app1_classifier;
	operation_graph->operations[true_app1_operation].known_classifier =
		true_app1_classifier;
	operation_graph->operations[true_app2_operation].classifier = true_app2_classifier;
	operation_graph->operations[true_app2_operation].known_classifier =
		true_app2_classifier;
	struct prototype_judgement_selected_evidence constructor_evidence[3];
	struct prototype_judgement_selected_evidence branch_evidence;
	uint32_t constructor_operations[3] = {
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID
	};
	if (prototype_judgement_selected_evidence_from_claim(
			judgement, false_in_context,
			&constructor_evidence[0]
		) != 0 || prototype_judgement_selected_evidence_from_claim(
			judgement, false_in_context,
			&constructor_evidence[1]
		) != 0 || prototype_judgement_selected_evidence_from_claim(
			judgement, false_identity_in_context, &constructor_evidence[2]
		) != 0) {
		return 16;
	}
	prototype_judgement_delta_set_operation(&delta, false_witness_operation);
	if (prototype_judgement_delta_record_constructor_spine(
			&delta, term_db, type_db, false_computation_witness,
			false_computation_identity, constructor_operations,
			constructor_evidence, 3
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, false_witness_operation, xr_context, false_computation_witness,
			false_computation_identity, &branch_evidence
		) != 0) {
		return 16;
	}
	prototype_judgement_delta_set_operation(&delta, false_operation);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, false_witness_return,
			false_witness_operation, &branch_evidence
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, false_operation, xr_context, false_witness_return,
			false_branch_computation_identity, &branch_evidence
		) != 0) {
		return 16;
	}
	if (prototype_judgement_selected_evidence_from_claim(
			judgement, true_in_context,
			&constructor_evidence[0]
		) != 0 || prototype_judgement_selected_evidence_from_claim(
			judgement, true_in_context,
			&constructor_evidence[1]
		) != 0 || prototype_judgement_selected_evidence_from_claim(
			judgement, true_identity_in_context, &constructor_evidence[2]
		) != 0) {
		return 17;
	}
	prototype_judgement_delta_set_operation(&delta, true_witness_operation);
	if (prototype_judgement_delta_record_constructor_spine(
			&delta, term_db, type_db, true_computation_witness,
			true_computation_identity, constructor_operations,
			constructor_evidence, 3
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, true_witness_operation, xr_context, true_computation_witness,
			true_computation_identity, &branch_evidence
		) != 0) {
		return 17;
	}
	prototype_judgement_delta_set_operation(&delta, true_operation);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, true_witness_return,
			true_witness_operation, &branch_evidence
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, true_operation, xr_context, true_witness_return,
			true_branch_computation_identity, &branch_evidence
		) != 0) {
		return 17;
	}
	struct prototype_judgement_selected_evidence current;
	uint32_t match_classifier;
	prototype_judgement_delta_set_operation(&delta, proof_operation);
	if (prototype_judgement_delta_build_constant_match_motive(
			&delta,
			term_db,
			type_db,
			xr,
			input_identity,
			xr_codomain,
			term_db->terms[universe].as.universe_var.level_var,
			&match_classifier
		) != 0) {
		return 18;
	}
	operation_graph->operations[proof_operation].classifier = match_classifier;
	operation_graph->operations[proof_operation].known_classifier = match_classifier;
	if (prototype_judgement_delta_expand_match(
			&delta, term_db, type_db, proof_endpoint, match_classifier
		) != 0) {
		return 191;
	}
	if (prototype_judgement_delta_select_evidence(
			&delta, proof_operation, xr_context, proof_endpoint, match_classifier,
			&current
		) != 0) {
		return 192;
	}
	struct prototype_judgement_selected_evidence binder_evidence;
	if (prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, xr_context, xr, input_identity,
			&binder_evidence
		) != 0) {
		return 20;
	}
	prototype_judgement_delta_set_context(&delta, x1_context);
	prototype_judgement_delta_set_operation(&delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_lambda_intro(
			&delta, term_db, type_db, PROTOTYPE_INVALID_ID, proof_lambda2, pi2,
			xr, proof_return2, &binder_evidence, proof_operation, &current
		) != 0) {
		return 211;
	}
	if (prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x1_context, proof_lambda2, pi2, &current
		) != 0) {
		return 212;
	}
	if (prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, proof_thunk2, PROTOTYPE_INVALID_ID, &current
		) != 0) {
		return 213;
	}
	if (prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x1_context, proof_thunk2, thunk2, &current
		) != 0) {
		return 214;
	}
	if (prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, proof_return1, PROTOTYPE_INVALID_ID, &current
		) != 0) {
		return 215;
	}
	if (prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x1_context, proof_return1, x1_codomain,
			&current
		) != 0) {
		return 216;
	}
	if (prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x1_context, x1, bool_type,
			&binder_evidence
		) != 0) {
		return 217;
	}
	prototype_judgement_delta_set_context(&delta, x0_context);
	if (prototype_judgement_delta_record_lambda_intro(
			&delta, term_db, type_db, PROTOTYPE_INVALID_ID, proof_lambda1, pi1,
			x1, proof_return1, &binder_evidence, PROTOTYPE_INVALID_ID, &current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x0_context, proof_lambda1, pi1, &current
		) != 0 || prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, proof_thunk1, PROTOTYPE_INVALID_ID, &current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x0_context, proof_thunk1, thunk1, &current
		) != 0 || prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, proof_return0, PROTOTYPE_INVALID_ID, &current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x0_context, proof_return0, x0_codomain,
			&current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, x0_context, x0, bool_type,
			&binder_evidence
		) != 0) {
		return 22;
	}
	prototype_judgement_delta_set_context(&delta, empty_context);
	if (prototype_judgement_delta_record_lambda_intro(
			&delta, term_db, type_db, PROTOTYPE_INVALID_ID, proof_lambda0, pi0,
			x0, proof_return0, &binder_evidence, PROTOTYPE_INVALID_ID, &current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, empty_context, proof_lambda0, pi0, &current
		) != 0 || prototype_judgement_delta_record_cbpv_boundary(
			&delta, term_db, type_db, proof_value, PROTOTYPE_INVALID_ID, &current
		) != 0 || prototype_judgement_delta_select_evidence(
			&delta, PROTOTYPE_INVALID_ID, empty_context, proof_value,
			concrete_identity, &current
		) != 0 || prototype_judgement_delta_publish_complete(
			&delta
		) != 0) {
		return 23;
	}
	uint32_t constant_binder = prototype_term_new_binding(term_db);
	uint32_t constant_lambda;
	uint32_t constant_value;
	uint32_t discriminating_identity;
	if (constant_binder == PROTOTYPE_INVALID_ID || prototype_term_lambda(
			term_db, constant_binder, false_return, &constant_lambda
		) != 0 || prototype_term_thunk(
			term_db, constant_lambda, &constant_value
		) != 0 || prototype_term_graph_substitute_bound_var(
			term_db,
			type_db,
			certificate->identity_type_term_id,
			certificate->left_endpoint_binding_id,
			left_value,
			&discriminating_identity
		) != 0 || prototype_term_graph_substitute_bound_var(
			term_db,
			type_db,
			discriminating_identity,
			certificate->right_endpoint_binding_id,
			constant_value,
			&discriminating_identity
		) != 0) {
		return 24;
	}
	if (prototype_judgement_classifier_conversion(
			term_db, type_db, concrete_identity, discriminating_identity
		).status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 25;
	}
	prototype_judgement_delta_set_context(&delta, empty_context);
	prototype_judgement_delta_set_operation(&delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_select_evidence(
			&delta,
			PROTOTYPE_INVALID_ID,
			empty_context,
			proof_value,
			discriminating_identity,
			&current
		) == 0) {
		return 26;
	}
	(void)bool_type_claim;
	(void)function_type_claim;
	(void)concrete_identity_claim;
	(void)false_in_context;
	(void)true_in_context;
	(void)false_identity_in_context;
	(void)true_identity_in_context;
	*p_identity_family_claim = concrete_identity_claim;
	*p_witness_term = proof_value;
	*p_witness_claim = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->context_id == empty_context &&
			proposition->subject == proof_value &&
			proposition->classifier == concrete_identity) {
			*p_witness_claim = i;
			break;
		}
	}
	if (*p_witness_claim == PROTOTYPE_INVALID_ID) {
		return 28;
	}
	return 0;
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
			.origin_kind = PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE,
			.origin_source_carrier_term_id = PROTOTYPE_INVALID_ID,
			.type_index = 0,
		.representation_id = PROTOTYPE_INVALID_ID,
		.formation_classifier = universe,
		.parameter_context = empty,
		.index_context = empty,
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
					.readback = {
						.first_field_type = PROTOTYPE_INVALID_ID,
						.result_type = PROTOTYPE_INVALID_ID
					},
					.parameter_context = empty,
				.field_context = empty,
				.result_classifier = view,
				.curried_classifier_cache = view
			};
	}
	if (terms_db->term_count >= terms_db->term_capacity) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t source = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[source], 0, sizeof(terms_db->terms[source]));
	terms_db->terms[source].tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	terms_db->terms[source].as.type_declaration.type_id = 0;
	terms_db->terms[source].as.type_declaration.identity.namespace_symbol_id = -1;
	terms_db->terms[source].as.type_declaration.identity.name_symbol_id = 100;
	terms_db->terms[view].as.type_view.source = source;
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
	if (terms_db->term_count + 2 > terms_db->term_capacity) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = type_id;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 200;
	terms_db->terms[view].as.type_view.core = core;
	uint32_t source = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[source], 0, sizeof(terms_db->terms[source]));
	terms_db->terms[source].tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	terms_db->terms[source].as.type_declaration.type_id = type_id;
	terms_db->terms[source].as.type_declaration.identity.namespace_symbol_id = -1;
	terms_db->terms[source].as.type_declaration.identity.name_symbol_id = 200;
	terms_db->terms[view].as.type_view.source = source;
	types_db->type_declarations[type_id].formation_classifier = universe;
	types_db->type_declarations[type_id].parameter_context = empty;
	types_db->type_declarations[type_id].index_context = empty;
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
		) != 0 || constructor_id >= types_db->constructor_count ||
		types_db->constructor_declarations[constructor_id].owner_type != type_id ||
		types_db->constructor_declarations[constructor_id].constructor_index != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	prototype_term_notify_graph_mutation(terms_db);
	return view;
}

static uint32_t add_dependent_box_view(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* types_db,
	struct prototype_context_db* contexts_db,
	uint32_t empty,
	uint32_t universe,
	uint32_t bool_view,
	uint32_t core
) {
	uint32_t type_id;
	if (prototype_type_declaration_add(types_db, 210, &type_id) != 0 ||
		terms_db->term_count + 2 > terms_db->term_capacity) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = type_id;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 210;
	terms_db->terms[view].as.type_view.core = core;
	uint32_t source = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[source], 0, sizeof(terms_db->terms[source]));
	terms_db->terms[source].tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	terms_db->terms[source].as.type_declaration.type_id = type_id;
	terms_db->terms[source].as.type_declaration.identity.namespace_symbol_id = -1;
	terms_db->terms[source].as.type_declaration.identity.name_symbol_id = 210;
	terms_db->terms[view].as.type_view.source = source;
	types_db->type_declarations[type_id].formation_classifier = universe;
	types_db->type_declarations[type_id].parameter_context = empty;
	types_db->type_declarations[type_id].index_context = empty;

	uint32_t first_binding = prototype_term_new_binding(terms_db);
	uint32_t family_binding = prototype_term_new_binding(terms_db);
	uint32_t second_binding = prototype_term_new_binding(terms_db);
	uint32_t first_context;
	uint32_t second_context;
	uint32_t first_variable;
	uint32_t family;
	uint32_t dependent_classifier;
	uint32_t curried_classifier;
	uint32_t constructor_id;
	uint32_t readback_fields[2] = {
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID
	};
	if (first_binding == PROTOTYPE_INVALID_ID ||
		family_binding == PROTOTYPE_INVALID_ID ||
		second_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts_db,
			empty,
			first_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&first_context
		) != 0 || prototype_term_var(
			terms_db, first_binding, &first_variable
		) != 0 || prototype_term_lambda(
			terms_db, family_binding, bool_view, &family
		) != 0 || prototype_term_app(
			terms_db, family, first_variable, &dependent_classifier
		) != 0 || prototype_context_extend(
			contexts_db,
			first_context,
			second_binding,
			dependent_classifier,
			PROTOTYPE_INVALID_ID,
			&second_context
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms_db,
			contexts_db,
			empty,
			second_context,
			view,
			&curried_classifier
		) != 0 || prototype_type_declaration_add_constructor(
			types_db,
			type_id,
			211,
			readback_fields,
			2,
			PROTOTYPE_INVALID_ID,
			empty,
			second_context,
			view,
			curried_classifier,
			&constructor_id
		) != 0 || constructor_id >= types_db->constructor_count) {
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
	if (terms_db->term_count + 2 > terms_db->term_capacity) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t view = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[view], 0, sizeof(terms_db->terms[view]));
	terms_db->terms[view].tag = PROTOTYPE_TERM_TYPE_VIEW;
	terms_db->terms[view].as.type_view.view_type_id = type_id;
	terms_db->terms[view].as.type_view.identity.namespace_symbol_id = -1;
	terms_db->terms[view].as.type_view.identity.name_symbol_id = 300;
	terms_db->terms[view].as.type_view.core = core;
	uint32_t source = (uint32_t)terms_db->term_count++;
	memset(&terms_db->terms[source], 0, sizeof(terms_db->terms[source]));
	terms_db->terms[source].tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	terms_db->terms[source].as.type_declaration.type_id = type_id;
	terms_db->terms[source].as.type_declaration.identity.namespace_symbol_id = -1;
	terms_db->terms[source].as.type_declaration.identity.name_symbol_id = 300;
	terms_db->terms[view].as.type_view.source = source;
	types_db->type_declarations[type_id].formation_classifier = universe;
	types_db->type_declarations[type_id].parameter_context = empty;
	types_db->type_declarations[type_id].index_context = empty;
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
	struct prototype_hott_relation_goal_db goal_db;
	struct prototype_hott_candidate_db candidate_db;
	struct prototype_hott_work_db work_db;
	struct prototype_hott_residual_db residual_db;
	struct prototype_hott_action_db action_db;
	struct prototype_kernel_builder kernel_builder;
	struct prototype_kernel_view kernel_view;

	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 256,
		case_binders, 256, ih_scopes, 64
	);
	if (getenv("A_PROGRAM_HOTT_PREALLOCATE_BINDING") &&
		prototype_term_new_binding(&term_db) == PROTOTYPE_INVALID_ID) {
		return 246;
	}
	prototype_type_declaration_db_init(
		&type_db, type_declarations, 16, constructors, 32, parameters, 8,
		field_types, 64, type_exprs, 32
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
		&cwf_certificate_db, cwf_certificates, 1024
	);
	memset(&operation_graph, 0, sizeof(operation_graph));
	operation_graph.operations = operations;
	operation_graph.operation_capacity = OPERATION_CAPACITY;
	operation_graph.cases = operation_cases;
	operation_graph.case_capacity = 128;
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
		&bridge_db, bridges, 128, bridge_certificates, 128
	);
	prototype_hott_relation_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
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
		256,
		action_certificates,
		256,
		action_results,
		256
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
	judgement.derivations[0] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO, left_has_type
	);
	judgement.derivations[1] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION, left_has_type
	);
	judgement.derivations[1].premise_count = 1;
	judgement.derivations[1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(sibling_has_type)
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
		) != 0 || bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY) {
		return 3;
	}

	uint32_t text_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, right_has_type, bridge,
			&text_goal
		) != 0 || text_goal != 0 ||
		prototype_hott_relation_goal_db_validate(
			&goal_db, &kernel_view, &bridge_db
		) != 0) {
		return 4;
	}
	uint32_t interned;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, right_has_type, bridge,
			&interned
		) != 0 || interned != text_goal || goal_db.goal_count != 1) {
		return 5;
	}
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, sibling_has_type, right_has_type, bridge,
			&interned
		) != 0 || interned == text_goal) {
		return 6;
	}

	uint32_t work_id;
	if (prototype_hott_relation_plan(
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, left_has_type, bridge,
			&diagonal_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, diagonal_goal, PROTOTYPE_HOTT_RULE_REL_DIAGONAL
		) != 1) {
		return 9;
	}
	uint32_t repeated_diagonal_work;
	size_t work_count_before_repeat = work_db.item_count;
	size_t candidate_count_before_repeat = candidate_db.candidate_count;
	if (prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, diagonal_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&repeated_diagonal_work
		) != 0 || repeated_diagonal_work != work_id ||
		work_db.item_count != work_count_before_repeat ||
		candidate_db.candidate_count != candidate_count_before_repeat) {
		return 146;
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, beta_claim, left_has_type, bridge,
			&conversion_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL,
			conversion_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_REL_CONVERT
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
	uint32_t box_view;
	uint32_t dependent_box_view;
	uint32_t nat_view;
	if (getenv("A_PROGRAM_HOTT_REVERSE_INDEPENDENT_TYPES")) {
		nat_view = add_nat_view(
			&term_db, &type_db, &context_db, empty, universe, text
		);
		dependent_box_view = add_dependent_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		box_view = add_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
	} else {
		box_view = add_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		dependent_box_view = add_dependent_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		nat_view = add_nat_view(
			&term_db, &type_db, &context_db, empty, universe, text
		);
	}
	if (box_view == PROTOTYPE_INVALID_ID ||
		dependent_box_view == PROTOTYPE_INVALID_ID ||
		nat_view == PROTOTYPE_INVALID_ID) {
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
	uint32_t bool_has_type;
	uint32_t bool_is_type;
	if (prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_view,
			universe,
			&bool_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			bool_view,
			universe,
			bool_has_type,
			&bool_is_type
		) != 0) {
		return 221;
	}
	uint32_t box_has_type;
	uint32_t box_is_type;
	uint32_t dependent_box_has_type;
	uint32_t dependent_box_is_type;
	uint32_t nat_has_type;
	uint32_t nat_is_type;
	if (prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			box_view,
			universe,
			&box_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			box_view,
			universe,
			box_has_type,
			&box_is_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			dependent_box_view,
			universe,
			&dependent_box_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			dependent_box_view,
			universe,
			dependent_box_has_type,
			&dependent_box_is_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			nat_view,
			universe,
			&nat_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			nat_view,
			universe,
			nat_has_type,
			&nat_is_type
		) != 0) {
		return 241;
	}
	uint32_t dependent_box_type_id = term_db.terms[
		dependent_box_view
	].as.type_view.view_type_id;
	uint32_t box_type_id = term_db.terms[box_view].as.type_view.view_type_id;
	const struct prototype_type_declaration* box_declaration =
		box_type_id < type_db.type_count ?
		&type_db.type_declarations[box_type_id] : NULL;
	const struct prototype_type_constructor_declaration* box_constructor_declaration =
		box_declaration && box_declaration->first_constructor <
			type_db.constructor_count ? &type_db.constructor_declarations[
				box_declaration->first_constructor
			] : NULL;
	const struct prototype_type_declaration* dependent_box_declaration =
		dependent_box_type_id < type_db.type_count ?
		&type_db.type_declarations[dependent_box_type_id] : NULL;
	const struct prototype_type_constructor_declaration*
		dependent_box_constructor = dependent_box_declaration &&
		dependent_box_declaration->first_constructor < type_db.constructor_count ?
		&type_db.constructor_declarations[
			dependent_box_declaration->first_constructor
		] : NULL;
	const struct prototype_context* dependent_second_context =
		dependent_box_constructor ? prototype_context_get(
			&context_db, dependent_box_constructor->field_context
		) : NULL;
	const struct prototype_context* dependent_first_context =
		dependent_second_context ? prototype_context_get(
			&context_db, dependent_second_context->parent
		) : NULL;
	uint32_t dependent_first_context_certificate;
	uint32_t box_field_context_certificate;
	uint32_t dependent_second_classifier_claim;
	uint32_t dependent_second_is_type;
	uint32_t dependent_second_context_certificate;
	uint32_t dependent_classifier = dependent_second_context ?
		prototype_context_classifier_term(dependent_second_context) :
		PROTOTYPE_INVALID_ID;
	const struct prototype_term* dependent_application =
		dependent_classifier < term_db.term_count ?
		&term_db.terms[dependent_classifier] : NULL;
	uint32_t dependent_family = dependent_application &&
		dependent_application->tag == PROTOTYPE_TERM_APP ?
		dependent_application->as.app.function : PROTOTYPE_INVALID_ID;
	const struct prototype_term* dependent_family_lambda =
		dependent_family < term_db.term_count ?
		&term_db.terms[dependent_family] : NULL;
	uint32_t dependent_family_context;
	uint32_t dependent_family_context_certificate;
	uint32_t dependent_family_projection;
	uint32_t bool_has_type_in_family_context;
	uint32_t dependent_family_binder_claim;
	uint32_t dependent_family_classifier;
	uint32_t dependent_family_claim;
	uint32_t dependent_first_projection;
	uint32_t dependent_family_claim_in_first_context;
	uint32_t dependent_first_variable_claim;
	if (!box_constructor_declaration || !dependent_first_context ||
		!dependent_second_context ||
		!dependent_application || !dependent_family_lambda ||
		dependent_family_lambda->tag != PROTOTYPE_TERM_LAMBDA ||
		prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			box_constructor_declaration->field_context,
			bool_is_type,
			&box_field_context_certificate
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_second_context->parent,
			bool_is_type,
			&dependent_first_context_certificate
		) != 0 || prototype_context_extend(
			&context_db,
			empty,
			dependent_family_lambda->as.lambda.binding_id,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&dependent_family_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_family_context,
			bool_is_type,
			&dependent_family_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			dependent_family_context,
			empty,
			&dependent_family_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			dependent_family_projection,
			&bool_has_type_in_family_context
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			dependent_family_context,
			dependent_family_lambda->as.lambda.binding_id,
			bool_view,
			&dependent_family_binder_claim
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			universe,
			&dependent_family_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			dependent_family,
			dependent_family_classifier,
			dependent_family_binder_claim,
			bool_has_type_in_family_context,
			&dependent_family_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			dependent_second_context->parent,
			empty,
			&dependent_first_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			dependent_family_claim,
			dependent_first_projection,
			&dependent_family_claim_in_first_context
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			dependent_second_context->parent,
			dependent_first_context->binding_id,
			bool_view,
			&dependent_first_variable_claim
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			dependent_second_context->parent,
			dependent_classifier,
			universe,
			dependent_family_claim_in_first_context,
			dependent_first_variable_claim,
			&dependent_second_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			dependent_second_context->parent,
			prototype_context_classifier_term(dependent_second_context),
			universe,
			dependent_second_classifier_claim,
			&dependent_second_is_type
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_box_constructor->field_context,
			dependent_second_is_type,
			&dependent_second_context_certificate
		) != 0) {
		return 308;
	}
	(void)box_field_context_certificate;
	(void)dependent_family_context_certificate;
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
	uint32_t bool_false_constructor_claim;
	uint32_t bool_true_constructor_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_false,
			bool_view,
			&bool_false_constructor_claim
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_true,
			bool_view,
			&bool_true_constructor_claim
		) != 0) {
		return 240;
	}
	false_claim = bool_false_constructor_claim;
	true_claim = bool_true_constructor_claim;
	uint32_t bool_distinct_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, false_claim, true_claim, bridge,
			&bool_distinct_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_distinct_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
			count_rule(
				&candidate_db, bool_distinct_goal,
				PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT
			) != 1) {
		return 15;
	}
	uint32_t bool_distinct_candidate = find_rule(
		&candidate_db,
		bool_distinct_goal,
		PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT
	);
	if (bool_distinct_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[bool_distinct_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY) {
		return 147;
	}
	uint32_t heterogeneous_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, text_is_type,
			false_claim, left_has_type, bridge, &heterogeneous_goal
		) != 0 || prototype_hott_relation_goal_db_validate(
			&goal_db, &kernel_view, &bridge_db
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, heterogeneous_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].outcome.state !=
			PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE) {
		return 144;
	}
	uint32_t bool_diagonal_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, false_claim, false_claim, bridge,
			&bool_diagonal_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, bool_diagonal_goal, PROTOTYPE_HOTT_RULE_REL_DIAGONAL
		) != 1 || count_rule(
			&candidate_db, bool_diagonal_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 1) {
		return 16;
	}
	uint32_t bool_constructor_candidate = find_rule(
		&candidate_db,
		bool_diagonal_goal,
		PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
	);
	if (bool_constructor_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[bool_constructor_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS) {
		return 147;
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, neutral_claim, false_claim, bridge,
			&neutral_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, neutral_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, neutral_goal, PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION
		) != 1 || count_rule(
			&candidate_db, neutral_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 0) {
		return 18;
	}
	uint32_t neutral_candidate = find_rule(
		&candidate_db,
		neutral_goal,
		PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION
	);
	if (neutral_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[neutral_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_DEFERRED) {
		return 147;
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
	uint32_t universe_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE, universe, empty,
		PROTOTYPE_INVALID_ID, universe, universe_successor
	);
	if (universe_has_type == PROTOTYPE_INVALID_ID ||
		judgement.derivation_count >= judgement.derivation_capacity) {
		return 35;
	}
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY, universe_has_type
	);
	uint32_t universe_is_type;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			universe,
			universe_successor,
			universe_has_type,
			&universe_is_type
		) != 0) {
		return 35;
	}
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			universe_is_type, universe_is_type,
			bool_has_universe, text_has_universe, bridge,
			&universe_goal
		) != 0 || prototype_hott_relation_plan(
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
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO, returned_left_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(left_has_type)
		};
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO, returned_right_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(right_has_type)
		};
	uint32_t computation_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			pure_comp_is_type, pure_comp_is_type,
			returned_left_claim, returned_right_claim, bridge,
			&computation_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, computation_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, computation_goal, PROTOTYPE_HOTT_RULE_REL_COMP_RETURN
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
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO, thunk_left_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(returned_left_claim)
		};
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO, thunk_right_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(returned_right_claim)
		};
	uint32_t thunk_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			thunk_type_is_type, thunk_type_is_type,
			thunk_left_claim, thunk_right_claim, bridge,
			&thunk_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, thunk_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, thunk_goal, PROTOTYPE_HOTT_RULE_REL_THUNK_PURE
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_comp_is_type, effect_comp_is_type,
			effect_left_claim, effect_right_claim, bridge,
			&effect_goal
		) != 0 || prototype_hott_relation_plan(
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_comp_is_type, effect_comp_is_type,
			request_claim, request_claim, bridge,
			&request_goal
		) != 0 || prototype_hott_relation_plan(
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
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			pure_pi_is_type, pure_pi_is_type,
			pure_function_claim, pure_function_claim, bridge,
			&pure_pi_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, pure_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, pure_pi_goal, PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE
		) != 1 || prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_pi_is_type, effect_pi_is_type,
			effect_function_claim, effect_function_claim, bridge,
			&effect_pi_goal
		) != 0 || prototype_hott_relation_plan(
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
		&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_REL_CONVERT
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
			.rule = PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR,
			.object_result = PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS,
			.first_child_edge = 0,
			.child_edge_count = 1
		},
		{
			.id = 1,
			.conclusion_goal_id = bool_diagonal_goal,
			.rule = PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR,
			.object_result = PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS,
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
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
			.relation_type_action_request_id = type_action_id
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_type_action_id;
	uint32_t bool_relation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_type_action, &bool_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id, &result_id
		) != 0 || action_results[result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 41;
	}
	bool_relation_result_id = result_id;
	struct prototype_hott_action_request bool_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_identity_computation_id;
	uint32_t repeated_bool_identity_computation_id;
	uint32_t bool_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation,
			&bool_identity_computation_id
		) != 0 || prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation,
			&repeated_bool_identity_computation_id
		) != 0 || bool_identity_computation_id !=
			repeated_bool_identity_computation_id ||
		bool_identity_computation_id == bool_type_action_id ||
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_id,
			&bool_identity_computation_result_id
		) != 0 || action_results[
			bool_identity_computation_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[bool_identity_computation_result_id].certificate_id >=
			action_db.certificate_count ||
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		type_db.type_declarations[
			action_db.certificates[
				action_results[bool_identity_computation_result_id].certificate_id
			].data.identity_type.generated_type_declaration_id
		].index_count != 2 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 137;
	}
	const struct prototype_hott_action_certificate* bool_identity_certificate =
		&action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		];
	/* Object identity formation accepts two independently typed endpoints. It
	 * does not require that both Claims were reindexed from one source term. */
	uint32_t true_true_identity_family_claim;
	uint32_t true_false_identity_family_claim;
	if (prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			true_claim,
			&true_true_identity_family_claim
		) != 0 || prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			false_claim,
			&true_false_identity_family_claim
		) != 0) {
		return 316;
	}
	const struct prototype_judgement_proposition* true_true_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, true_true_identity_family_claim
		);
	const struct prototype_judgement_proposition* true_false_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, true_false_identity_family_claim
		);
	uint32_t true_true_identity_type_id;
	uint32_t true_false_identity_type_id;
	uint32_t true_true_identity_arguments[2];
	uint32_t true_false_identity_arguments[2];
	uint32_t true_true_identity_argument_count;
	uint32_t true_false_identity_argument_count;
	if (!true_true_identity_family || !true_false_identity_family ||
		true_true_identity_family->subject == true_false_identity_family->subject ||
		prototype_term_type_instance_info(
			&term_db,
			true_true_identity_family->subject,
			&true_true_identity_type_id,
			true_true_identity_arguments,
			&true_true_identity_argument_count
		) != 0 || prototype_term_type_instance_info(
			&term_db,
			true_false_identity_family->subject,
			&true_false_identity_type_id,
			true_false_identity_arguments,
			&true_false_identity_argument_count
		) != 0 || true_true_identity_type_id !=
			bool_identity_certificate->data.identity_type.
				generated_type_declaration_id ||
		true_false_identity_type_id != true_true_identity_type_id ||
		true_true_identity_argument_count != 2 ||
		true_false_identity_argument_count != 2 ||
		true_true_identity_arguments[0] != bool_true ||
		true_true_identity_arguments[1] != bool_true ||
		true_false_identity_arguments[0] != bool_true ||
		true_false_identity_arguments[1] != bool_false) {
		return 317;
	}
	uint32_t true_degeneracy_family_claim;
	uint32_t true_degeneracy_witness;
	uint32_t true_degeneracy_witness_claim;
	uint32_t checked_true_identity_family_claim;
	uint32_t checked_true_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			&true_degeneracy_family_claim,
			&true_degeneracy_witness,
			&true_degeneracy_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			true_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			false_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) == 0) {
		return 318;
	}
	(void)true_degeneracy_family_claim;
	(void)true_degeneracy_witness;
	struct prototype_hott_action_request universe_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t universe_identity_computation_id;
	uint32_t universe_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_identity_computation,
			&universe_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_identity_computation_id,
			&universe_identity_computation_result_id
		) != 0 || action_results[
			universe_identity_computation_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[universe_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 268;
	}
	const struct prototype_hott_action_certificate*
		universe_identity_certificate = &action_db.certificates[
			action_results[
				universe_identity_computation_result_id
			].certificate_id
		];
	uint32_t universe_identity_type_id = universe_identity_certificate->
		data.identity_type.generated_type_declaration_id;
	if (universe_identity_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		universe_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE ||
		universe_identity_type_id >= type_db.type_count ||
		type_db.type_declarations[universe_identity_type_id].index_count != 2 ||
		type_db.type_declarations[universe_identity_type_id].constructor_count != 1 ||
		!prototype_type_declaration_validate_generated_identity(
			&term_db,
			&type_db,
			&context_db,
			universe,
			universe_identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE
		) || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 269;
	}

	/* Type-family functions inhabit an ordinary Pi. Their identity is the
	 * same pointwise rule as for CBPV functions, without Thunk/Comp wrappers. */
	uint32_t type_family_binding = prototype_term_new_binding(&term_db);
	uint32_t type_family_codomain;
	uint32_t type_family_pi;
	uint32_t type_family_pi_has_type;
	uint32_t type_family_pi_is_type;
	if (type_family_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			&term_db, type_family_binding, universe, &type_family_codomain
		) != 0 || prototype_term_pi_family(
			&term_db, bool_view, type_family_codomain, &type_family_pi
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			type_family_pi,
			universe,
			&type_family_pi_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			type_family_pi,
			universe,
			type_family_pi_has_type,
			&type_family_pi_is_type
		) != 0) {
		return 283;
	}
	struct prototype_hott_action_request type_family_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = type_family_pi_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t type_family_identity_request;
	uint32_t type_family_identity_result;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			type_family_identity_computation,
			&type_family_identity_request
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			type_family_identity_request,
			&type_family_identity_result
		) != 0 || action_results[type_family_identity_result].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[type_family_identity_result].certificate_id >=
			action_db.certificate_count) {
		return 284;
	}
	const struct prototype_hott_identity_type_computation_certificate*
		type_family_identity = &action_db.certificates[
			action_results[type_family_identity_result].certificate_id
		].data.identity_type;
	uint32_t type_family_identity_term = type_family_identity->identity_type_term_id;
	for (uint32_t i = 0; i < 3; ++i) {
		uint32_t ignored_binding;
		if (type_family_identity_term >= term_db.term_count ||
			term_db.terms[type_family_identity_term].tag != PROTOTYPE_TERM_PI ||
			prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[type_family_identity_term].as.pi.codomain_family,
				&ignored_binding,
				&type_family_identity_term
			) != 0) {
			return 285;
		}
	}
	if (type_family_identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		return 286;
	}
	uint32_t type_family_result_type_id;
	uint32_t type_family_result_arguments[2];
	uint32_t type_family_result_argument_count;
	if (prototype_term_type_instance_info(
			&term_db,
			type_family_identity_term,
			&type_family_result_type_id,
			type_family_result_arguments,
			&type_family_result_argument_count
		) != 0 || type_family_result_type_id != universe_identity_type_id ||
		type_family_result_argument_count != 2) {
		return 287;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 288;
	}

	/* Raw type-level Lambda uses the same pointwise action as a CBPV
	 * function, but its witness remains in the value/type fragment. */
	uint32_t constant_type_family_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_type_family_context;
	uint32_t constant_type_family_context_certificate;
	uint32_t constant_type_family_var_claim;
	uint32_t constant_type_family_projection;
	uint32_t constant_type_family_body_claim;
	uint32_t constant_type_family_lambda;
	uint32_t constant_type_family_lambda_claim;
	if (constant_type_family_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			constant_type_family_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_type_family_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_type_family_context,
			bool_is_type,
			&constant_type_family_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_type_family_context,
			constant_type_family_binding,
			bool_view,
			&constant_type_family_var_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_type_family_context,
			empty,
			&constant_type_family_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			constant_type_family_projection,
			&constant_type_family_body_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_type_family_binding,
			bool_view,
			&constant_type_family_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_type_family_lambda,
			type_family_pi,
			constant_type_family_var_claim,
			constant_type_family_body_claim,
			&constant_type_family_lambda_claim
		) != 0) {
		return 313;
	}
	uint32_t constant_type_family_identity_family_claim;
	uint32_t constant_type_family_witness;
	uint32_t constant_type_family_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			type_family_identity_result,
			constant_type_family_lambda_claim,
			&constant_type_family_identity_family_claim,
			&constant_type_family_witness,
			&constant_type_family_witness_claim
		) != 0 ||
		constant_type_family_witness >= term_db.term_count ||
		term_db.terms[constant_type_family_witness].tag != PROTOTYPE_TERM_LAMBDA) {
		return 314;
	}
	const struct prototype_judgement_proposition*
		constant_type_family_identity_family =
			prototype_judgement_claim_proposition(
				&judgement, constant_type_family_identity_family_claim
			);
	const struct prototype_judgement_proposition* constant_type_family_witness_type =
		prototype_judgement_claim_proposition(
			&judgement, constant_type_family_witness_claim
		);
	if (!constant_type_family_identity_family ||
		!constant_type_family_witness_type ||
		constant_type_family_witness_type->classifier !=
			constant_type_family_identity_family->subject ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 315;
	}
	(void)constant_type_family_context_certificate;

	/* A family variable and its argument are related in the object bridge.
	 * APP must apply the family witness, not compare the resulting type Terms. */
	uint32_t family_value_binding = prototype_term_new_binding(&term_db);
	uint32_t family_value_context;
	uint32_t family_value_context_certificate;
	uint32_t family_value_bridge;
	int family_value_residual;
	if (family_value_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			family_value_binding,
			type_family_pi,
			PROTOTYPE_INVALID_ID,
			&family_value_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_value_context,
			type_family_pi_is_type,
			&family_value_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_value_context,
			&family_value_bridge,
			&family_value_residual
		) != 0 || family_value_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 289;
	}
	uint32_t family_to_empty;
	uint32_t bool_in_family_context;
	uint32_t family_argument_binding = prototype_term_new_binding(&term_db);
	uint32_t family_argument_context;
	uint32_t family_argument_context_certificate;
	uint32_t family_argument_bridge;
	int family_argument_residual;
	if (family_argument_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			family_value_context,
			empty,
			&family_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			family_to_empty,
			&bool_in_family_context
		) != 0 || prototype_context_extend(
			&context_db,
			family_value_context,
			family_argument_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&family_argument_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_argument_context,
			bool_in_family_context,
			&family_argument_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_argument_context,
			&family_argument_bridge,
			&family_argument_residual
		) != 0 || family_argument_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 290;
	}
	uint32_t family_value_var;
	uint32_t family_argument_var;
	uint32_t family_value_claim;
	uint32_t family_argument_claim;
	uint32_t family_application;
	uint32_t family_application_claim;
	if (prototype_term_var(
			&term_db, family_value_binding, &family_value_var
		) != 0 || prototype_term_var(
			&term_db, family_argument_binding, &family_argument_var
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			family_argument_context,
			family_value_binding,
			type_family_pi,
			&family_value_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			family_argument_context,
			family_argument_binding,
			bool_view,
			&family_argument_claim
		) != 0 || prototype_term_app(
			&term_db,
			family_value_var,
			family_argument_var,
			&family_application
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			family_argument_context,
			family_application,
			universe,
			family_value_claim,
			family_argument_claim,
			&family_application_claim
		) != 0) {
		return 291;
	}
	uint32_t family_argument_to_empty;
	uint32_t universe_in_family_argument_context;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			family_argument_context,
			empty,
			&family_argument_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_is_type,
			family_argument_to_empty,
			&universe_in_family_argument_context
		) != 0) {
		return 292;
	}
	struct prototype_hott_action_request family_result_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_in_family_argument_context,
			.source_bridge_id = family_argument_bridge
		}
	};
	uint32_t family_result_identity_request_id;
	uint32_t family_result_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_result_identity_request,
			&family_result_identity_request_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_result_identity_request_id,
			&family_result_identity_result_id
		) != 0 || action_results[
			family_result_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 293;
	}
	struct prototype_hott_action_request family_application_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = family_application_claim,
			.source_bridge_id = family_argument_bridge,
			.identity_type_action_request_id =
				family_result_identity_request_id
		}
	};
	uint32_t family_application_action_id;
	uint32_t family_application_action_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_application_action,
			&family_application_action_id
		) != 0) {
		return 294;
	}
	if (prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_application_action_id,
			&family_application_action_result_id
		) != 0) {
		return 295;
	}
	if (action_results[
			family_application_action_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 296;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 297;
	}
	uint32_t family_application_is_type;
	uint32_t family_result_binding = prototype_term_new_binding(&term_db);
	uint32_t family_result_context;
	uint32_t family_result_context_certificate;
	uint32_t family_result_bridge;
	int family_result_residual;
	if (family_result_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			family_argument_context,
			family_application,
			universe,
			family_application_claim,
			&family_application_is_type
		) != 0) {
		return 298;
	}
	struct prototype_hott_action_request family_fiber_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = family_application_is_type,
			.source_bridge_id = family_argument_bridge
		}
	};
	uint32_t family_fiber_identity_request_id;
	uint32_t family_fiber_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_fiber_identity_request,
			&family_fiber_identity_request_id
		) != 0) {
		return 305;
	}
	if (prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_fiber_identity_request_id,
			&family_fiber_identity_result_id
		) != 0) {
		return 307;
	}
	if (action_results[family_fiber_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 306;
	}
	if (prototype_context_extend(
			&context_db,
			family_argument_context,
			family_result_binding,
			family_application,
			PROTOTYPE_INVALID_ID,
			&family_result_context
		) != 0) {
		return 300;
	}
	if (prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_result_context,
			family_application_is_type,
			&family_result_context_certificate
		) != 0) {
		return 301;
	}
	if (prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_result_context,
			&family_result_bridge,
			&family_result_residual
		) != 0) {
		return 302;
	}
	if (family_result_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 303;
	}
	if (bridge_db.certificates[family_result_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[family_result_bridge].fiber_action_certificate_id >=
			action_db.certificate_count) {
		return 304;
	}
	const struct prototype_hott_action_certificate* family_result_fiber =
		&action_certificates[bridge_db.certificates[
			family_result_bridge
		].fiber_action_certificate_id];
	if (family_result_fiber->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		family_result_fiber->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 299;
	}
	(void)family_value_context_certificate;
	(void)family_argument_context_certificate;
	(void)family_result_context_certificate;
	uint32_t universe_proof_binding = prototype_term_new_binding(&term_db);
	uint32_t universe_proof_context;
	uint32_t universe_proof;
	uint32_t universe_identity_former;
	uint32_t universe_proof_context_certificate;
	uint32_t universe_proof_claim;
	uint32_t universe_endpoint_path[2];
	uint32_t universe_endpoint_count;
	uint32_t universe_left_endpoint_claim;
	uint32_t universe_right_endpoint_claim;
	uint32_t universe_left_in_proof_context;
	uint32_t universe_right_in_proof_context;
	uint32_t universe_proof_to_left;
	uint32_t universe_proof_to_right;
	if (universe_proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extension_path(
			&context_db,
			empty,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_endpoint_path,
			2,
			&universe_endpoint_count
		) != 0 || universe_endpoint_count != 2 ||
		prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_endpoint_path[0],
			universe_identity_certificate->data.identity_type.
				left_endpoint_binding_id,
			universe,
			&universe_left_endpoint_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_endpoint_path[1],
			universe_identity_certificate->data.identity_type.
				right_endpoint_binding_id,
			universe,
			&universe_right_endpoint_claim
		) != 0 ||
		prototype_context_extend(
			&context_db,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_proof_binding,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			PROTOTYPE_INVALID_ID,
			&universe_proof_context
		) != 0 || prototype_term_var(
			&term_db, universe_proof_binding, &universe_proof
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			universe_identity_type_id,
			NULL,
			0,
			&universe_identity_former
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			universe_proof_context,
			universe_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			&universe_proof_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_proof_context,
			universe_proof_binding,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			&universe_proof_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_proof_context,
			universe_endpoint_path[0],
			&universe_proof_to_left
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_proof_context,
			universe_endpoint_path[1],
			&universe_proof_to_right
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_left_endpoint_claim,
			universe_proof_to_left,
			&universe_left_in_proof_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_right_endpoint_claim,
			universe_proof_to_right,
			&universe_right_in_proof_context
		) != 0) {
		return 271;
	}
	struct prototype_case_binder universe_branch_binders[7];
	uint32_t universe_branch_fields[7];
	uint32_t universe_branch_context = universe_proof_context;
	for (uint32_t i = 0; i < 7; ++i) {
		uint32_t classifier;
		uint32_t next_context;
		universe_branch_binders[i] = (struct prototype_case_binder) {
			.binding_id = prototype_term_new_binding(&term_db),
			.is_recursive = 0
		};
		if (universe_branch_binders[i].binding_id == PROTOTYPE_INVALID_ID ||
			prototype_judgement_constructor_field_classifier(
				&term_db,
				&type_db,
				&context_db,
				&substitution_db,
				universe_branch_context,
				universe_identity_former,
				0,
				universe_branch_binders,
				i,
				i,
				&classifier
			) != 0 || prototype_context_extend(
				&context_db,
				universe_branch_context,
				universe_branch_binders[i].binding_id,
				classifier,
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				&term_db,
				universe_branch_binders[i].binding_id,
				&universe_branch_fields[i]
			) != 0) {
			return 271;
		}
		universe_branch_context = next_context;
	}
	uint32_t refined_universe_branch_context;
	uint32_t universe_branch_substitution;
	uint32_t refined_universe_constructor;
	if (prototype_judgement_indexed_branch_refinement(
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			universe_proof_context,
			universe_proof,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			0,
			universe_branch_context,
			universe_branch_binders,
			7,
			&refined_universe_branch_context,
			&universe_branch_substitution,
			&refined_universe_constructor
		) != 0 || prototype_substitution_get(
			&substitution_db, universe_branch_substitution
		)->source_context != refined_universe_branch_context ||
		prototype_substitution_get(
			&substitution_db, universe_branch_substitution
		)->target_context != universe_branch_context) {
		return 271;
	}
	uint32_t refined_universe_left;
	uint32_t refined_universe_right;
	uint32_t refined_universe_proof;
	uint32_t preserved_universe_transport;
	uint32_t universe_left_endpoint;
	uint32_t universe_right_endpoint;
	if (prototype_term_var(
			&term_db,
			universe_identity_certificate->data.identity_type.left_endpoint_binding_id,
			&universe_left_endpoint
		) != 0 || prototype_term_var(
			&term_db,
			universe_identity_certificate->data.identity_type.right_endpoint_binding_id,
			&universe_right_endpoint
		) != 0 || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_left_endpoint,
			universe_branch_substitution,
			&refined_universe_left
		) != 0 || refined_universe_left != universe_branch_fields[0] ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_right_endpoint,
			universe_branch_substitution,
			&refined_universe_right
		) != 0 || refined_universe_right != universe_branch_fields[1] ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_proof,
			universe_branch_substitution,
			&refined_universe_proof
		) != 0 || refined_universe_proof != refined_universe_constructor ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_branch_fields[3],
			universe_branch_substitution,
			&preserved_universe_transport
		) != 0 || preserved_universe_transport != universe_branch_fields[3]) {
		return 271;
	}
	uint32_t universe_degeneracy_family_claim;
	uint32_t universe_degeneracy_witness;
	uint32_t universe_degeneracy_witness_claim;
	int universe_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_identity_computation_result_id,
			bool_has_type,
			&universe_degeneracy_family_claim,
			&universe_degeneracy_witness,
			&universe_degeneracy_witness_claim
		);
	if (universe_degeneracy_status != 0) {
		fprintf(
			stderr,
			"universe degeneracy construction failed: %d\n",
			universe_degeneracy_status
		);
		return 270;
	}
	const struct prototype_judgement_proposition* universe_degeneracy_family =
		prototype_judgement_claim_proposition(
			&judgement, universe_degeneracy_family_claim
		);
	const struct prototype_judgement_proposition* universe_degeneracy_proof =
		prototype_judgement_claim_proposition(
			&judgement, universe_degeneracy_witness_claim
		);
	uint32_t expected_universe_identity_arguments[2] = {
		bool_view, bool_view
	};
	uint32_t expected_universe_identity;
	if (!universe_degeneracy_family || !universe_degeneracy_proof ||
		prototype_term_type_instance_make(
			&term_db,
			&type_db,
			universe_identity_type_id,
			expected_universe_identity_arguments,
			2,
			&expected_universe_identity
		) != 0 || universe_degeneracy_family->subject !=
			expected_universe_identity || universe_degeneracy_proof->subject !=
			universe_degeneracy_witness || universe_degeneracy_proof->classifier !=
			expected_universe_identity || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 271;
	}
	uint32_t universe_witness_head;
	uint32_t universe_witness_owner;
	uint32_t universe_witness_constructor;
	uint32_t universe_witness_arguments[7];
	uint32_t universe_witness_argument_count;
	if (prototype_term_constructor_spine_info(
			&term_db,
			universe_degeneracy_witness,
			&universe_witness_head,
			&universe_witness_owner,
			&universe_witness_constructor,
			universe_witness_arguments,
			7,
			&universe_witness_argument_count
		) != 0 || universe_witness_constructor != 0 ||
		universe_witness_argument_count != 7) {
		return 273;
	}
	uint32_t concrete_universe_substitution;
	if (prototype_substitution_empty(
			&substitution_db,
			&context_db,
			empty,
			&concrete_universe_substitution
		) != 0 || prototype_context_extension_path(
			&context_db,
			empty,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_endpoint_path,
			2,
			&universe_endpoint_count
		) != 0 || universe_endpoint_count != 2 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_endpoint_path[0],
			bool_view,
			universe,
			&concrete_universe_substitution
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_endpoint_path[1],
			bool_view,
			universe,
			&concrete_universe_substitution
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_proof_context,
			universe_degeneracy_witness,
			expected_universe_identity,
			&concrete_universe_substitution
		) != 0) {
		return 273;
	}
	for (int projection = PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION;
		projection <= PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_LEFT;
		++projection) {
		uint32_t projected;
		uint32_t projected_claim;
		uint32_t concrete_projected;
		uint32_t normalized_projection;
		const struct prototype_judgement_proposition* projection_proposition;
		int projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				&action_db,
				&kernel_builder,
				&bridge_db,
				universe_identity_computation_result_id,
				universe_proof_claim,
				projection,
				&projected,
				&projected_claim
			);
		if (projection_status != 0) {
			return 273;
		}
		if (prototype_term_reindex(
				&term_db,
				&type_db,
				&context_db,
				&substitution_db,
				projected,
				concrete_universe_substitution,
				&concrete_projected
			) != 0 || prototype_term_normalize_complete_with_profile(
				&term_db,
				&type_db,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				concrete_projected,
				&normalized_projection
			) != 0 || normalized_projection !=
				universe_witness_arguments[projection] || !(projection_proposition =
				prototype_judgement_claim_proposition(
					&judgement, projected_claim
				)) || projection_proposition->subject != projected) {
			return 273;
		}
	}
	(void)universe_witness_head;
	(void)universe_witness_owner;
	uint32_t universe_variable_bridge;
	uint32_t universe_context_to_empty;
	uint32_t universe_is_type_in_variable_context;
	struct prototype_hott_action_request lifted_universe_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t lifted_universe_identity_id;
	uint32_t lifted_universe_identity_result_id;
	uint32_t universe_variable_identity_family_claim;
	uint32_t universe_variable_identity_witness;
	uint32_t universe_variable_identity_witness_claim;
	uint32_t universe_variable_relation;
	uint32_t universe_variable_relation_claim;
	struct prototype_hott_action_request universe_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = universe_left_endpoint_claim,
			.source_bridge_id = PROTOTYPE_INVALID_ID,
			.identity_type_action_request_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t universe_variable_action_id;
	uint32_t universe_variable_action_result_id;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			universe_endpoint_path[0],
			universe_identity_computation_id,
			&universe_variable_bridge
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_endpoint_path[0],
			empty,
			&universe_context_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_is_type,
			universe_context_to_empty,
			&universe_is_type_in_variable_context
		) != 0) {
		return 273;
	}
	lifted_universe_identity.key.identity_type.source_claim_id =
		universe_is_type_in_variable_context;
	lifted_universe_identity.key.identity_type.source_bridge_id =
		universe_variable_bridge;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			lifted_universe_identity,
			&lifted_universe_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			lifted_universe_identity_id,
			&lifted_universe_identity_result_id
		) != 0 || action_certificates[
			action_results[lifted_universe_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		return 273;
	}
	universe_variable_action.key.object_term.source_bridge_id =
		universe_variable_bridge;
	universe_variable_action.key.object_term.identity_type_action_request_id =
		lifted_universe_identity_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_variable_action,
			&universe_variable_action_id
		) != 0 || prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_variable_action_id,
			&universe_variable_action_result_id
		) != 0 || action_results[
			universe_variable_action_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			universe_variable_action_result_id
		].certificate_id >= action_db.certificate_count || action_certificates[
			action_results[universe_variable_action_result_id].certificate_id
		].kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
		return 273;
	}
	const struct prototype_hott_object_term_action_certificate*
		universe_variable_action_certificate = &action_certificates[
			action_results[universe_variable_action_result_id].certificate_id
		].data.object_term;
	universe_variable_identity_family_claim =
		universe_variable_action_certificate->identity_family_has_type_claim_id;
	universe_variable_identity_witness =
		universe_variable_action_certificate->witness_term_id;
	universe_variable_identity_witness_claim =
		universe_variable_action_certificate->witness_has_type_claim_id;
	struct prototype_hott_action_certificate forged_object_action = {
		.request_id = universe_variable_action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM,
		.data.object_term = *universe_variable_action_certificate
	};
	uint32_t forged_object_action_certificate_id;
	forged_object_action.data.object_term.identity_family_has_type_claim_id =
		universe_left_endpoint_claim;
	if (prototype_hott_action_certificate_add(
			&action_db,
			&kernel_view,
			&bridge_db,
			forged_object_action,
			&forged_object_action_certificate_id
		) == 0) {
		return 273;
	}
	if (prototype_hott_construct_universe_correspondence_projection(
			&action_db,
			&kernel_builder,
			&bridge_db,
			lifted_universe_identity_result_id,
			universe_variable_identity_witness_claim,
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
			&universe_variable_relation,
			&universe_variable_relation_claim
		) != 0 || !prototype_judgement_claim_proposition(
			&judgement, universe_variable_relation_claim
		) || prototype_judgement_claim_proposition(
			&judgement, universe_variable_relation_claim
		)->subject != universe_variable_relation) {
		return 273;
	}
	(void)universe_variable_identity_family_claim;
	(void)universe_variable_identity_witness;
	uint32_t universe_variable_is_type;
	struct prototype_hott_action_request universe_fiber_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = universe_variable_bridge
		}
	};
	uint32_t universe_fiber_identity_id;
	uint32_t universe_fiber_identity_result_id;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			universe_endpoint_path[0],
			universe_left_endpoint,
			universe,
			universe_left_endpoint_claim,
			&universe_variable_is_type
		) != 0) {
		return 273;
	}
	universe_fiber_identity.key.identity_type.source_claim_id =
		universe_variable_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_fiber_identity,
			&universe_fiber_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_fiber_identity_id,
			&universe_fiber_identity_result_id
		) != 0 || action_results[
			universe_fiber_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			universe_fiber_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		return 273;
	}
	const struct prototype_hott_action_certificate* universe_fiber_certificate =
		&action_certificates[action_results[
			universe_fiber_identity_result_id
		].certificate_id];
	uint32_t universe_fiber_endpoint_path[2];
	uint32_t universe_fiber_endpoint_count;
	uint32_t universe_fiber_left;
	uint32_t universe_fiber_right;
	uint32_t expected_universe_fiber_at_left;
	uint32_t expected_universe_fiber;
	if (universe_fiber_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_context_extension_path(
			&context_db,
			bridge_db.bridges[universe_variable_bridge].bridge_context_id,
			universe_fiber_certificate->data.identity_type.endpoint_context_id,
			universe_fiber_endpoint_path,
			2,
			&universe_fiber_endpoint_count
		) != 0 || universe_fiber_endpoint_count != 2 || prototype_term_var(
			&term_db,
			universe_fiber_certificate->data.identity_type.left_endpoint_binding_id,
			&universe_fiber_left
		) != 0 || prototype_term_var(
			&term_db,
			universe_fiber_certificate->data.identity_type.right_endpoint_binding_id,
			&universe_fiber_right
		) != 0 || prototype_term_app(
			&term_db,
			universe_fiber_certificate->data.identity_type.backing_type_former_term_id,
			universe_fiber_left,
			&expected_universe_fiber_at_left
		) != 0 || prototype_term_app(
			&term_db,
			expected_universe_fiber_at_left,
			universe_fiber_right,
			&expected_universe_fiber
		) != 0 || expected_universe_fiber !=
			universe_fiber_certificate->data.identity_type.identity_type_term_id ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 273;
	}
	uint32_t universe_element_binding = prototype_term_new_binding(&term_db);
	uint32_t universe_element_context;
	uint32_t universe_element_context_certificate;
	uint32_t universe_element_bridge;
	int universe_element_residual_reason;
	if (universe_element_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			universe_endpoint_path[0],
			universe_element_binding,
			universe_left_endpoint,
			PROTOTYPE_INVALID_ID,
			&universe_element_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			universe_element_context,
			universe_variable_is_type,
			&universe_element_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			universe_element_context,
			&universe_element_bridge,
			&universe_element_residual_reason
		) != 0 || bridge_db.certificates[
			universe_element_bridge
		].semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[universe_element_bridge].fiber_action_certificate_id !=
			action_results[universe_fiber_identity_result_id].certificate_id ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 273;
	}
	if (universe_element_residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 273;
	}
	(void)universe_element_context_certificate;
	uint32_t universe_type_projection;
	uint32_t universe_type_in_element_context;
	uint32_t second_universe_element_binding = prototype_term_new_binding(&term_db);
	uint32_t second_universe_element_context;
	uint32_t second_universe_element_context_certificate;
	uint32_t second_universe_element_bridge;
	int second_universe_element_residual_reason;
	if (second_universe_element_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_element_context,
			universe_endpoint_path[0],
			&universe_type_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_variable_is_type,
			universe_type_projection,
			&universe_type_in_element_context
		) != 0) {
		return 273;
	}
	if (prototype_context_extend(
			&context_db,
			universe_element_context,
			second_universe_element_binding,
			universe_left_endpoint,
			PROTOTYPE_INVALID_ID,
			&second_universe_element_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			second_universe_element_context,
			universe_type_in_element_context,
			&second_universe_element_context_certificate
		) != 0) {
		return 273;
	}
	if (prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			second_universe_element_context,
			&second_universe_element_bridge,
			&second_universe_element_residual_reason
		) != 0) {
		return 273;
	}
	if (second_universe_element_residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NONE || bridge_db.certificates[
				second_universe_element_bridge
			].semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id >= action_db.certificate_count ||
		action_certificates[bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		action_certificates[bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id].data.identity_type.backing_type_former_term_id !=
			universe_fiber_certificate->data.identity_type.backing_type_former_term_id ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 273;
	}
	(void)second_universe_element_context_certificate;
	uint32_t bool_identity_endpoint_path[2];
	uint32_t bool_identity_endpoint_count;
	uint32_t first_endpoint_bridge;
	if (prototype_context_extension_path(
			&context_db,
			empty,
			bool_identity_certificate->data.identity_type.endpoint_context_id,
			bool_identity_endpoint_path,
			2,
			&bool_identity_endpoint_count
		) != 0 || bool_identity_endpoint_count != 2 ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_identity_endpoint_path[0],
			bool_identity_computation_id,
			&first_endpoint_bridge
		) != 0) {
		return 251;
	}
	uint32_t first_endpoint_to_empty;
	uint32_t bool_is_type_in_first_endpoint;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			bool_identity_endpoint_path[0],
			empty,
			&first_endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			first_endpoint_to_empty,
			&bool_is_type_in_first_endpoint
		) != 0) {
		return 252;
	}
	struct prototype_hott_action_request constant_bool_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_is_type_in_first_endpoint,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_bool_identity_id;
	uint32_t constant_bool_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_bool_identity,
			&constant_bool_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_id,
			&constant_bool_identity_result_id
		) != 0 || action_results[
			constant_bool_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_certificates[
			action_results[constant_bool_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 253;
	}
	uint32_t endpoint_bool_binding = context_db.contexts[
		bool_identity_endpoint_path[0]
	].binding_id;
	uint32_t endpoint_bool_claim;
	uint32_t endpoint_bool_identity_family_claim;
	uint32_t endpoint_bool_action_witness;
	uint32_t endpoint_bool_action_witness_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			bool_identity_endpoint_path[0],
			endpoint_bool_binding,
			bool_view,
			&endpoint_bool_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_result_id,
			endpoint_bool_claim,
			&endpoint_bool_identity_family_claim,
			&endpoint_bool_action_witness,
			&endpoint_bool_action_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_action_witness_claim
		)->context_id != bridge_db.bridges[
			first_endpoint_bridge
		].bridge_context_id || endpoint_bool_action_witness !=
			prototype_judgement_claim_proposition(
				&judgement,
				bridge_certificates[first_endpoint_bridge].fiber_witness_claim_id
			)->subject || prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_action_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_identity_family_claim
		)->subject) {
		return 254;
	}
	uint32_t endpoint_to_empty;
	uint32_t endpoint_false_claim;
	uint32_t endpoint_false_identity_family_claim;
	uint32_t endpoint_false_action_witness;
	uint32_t endpoint_false_action_witness_claim;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			bool_identity_endpoint_path[0],
			empty,
			&endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_false_constructor_claim,
			endpoint_to_empty,
			&endpoint_false_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_result_id,
			endpoint_false_claim,
			&endpoint_false_identity_family_claim,
			&endpoint_false_action_witness,
			&endpoint_false_action_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, endpoint_false_action_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, endpoint_false_identity_family_claim
		)->subject || endpoint_false_action_witness >= term_db.term_count ||
		term_db.terms[endpoint_false_action_witness].tag !=
			PROTOTYPE_TERM_CONSTRUCTOR) {
		return 255;
	}
	uint32_t full_endpoint_bridge;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_identity_endpoint_path[1],
			constant_bool_identity_id,
			&full_endpoint_bridge
		) != 0 || context_db.contexts[
			bridge_db.bridges[full_endpoint_bridge].bridge_context_id
		].depth != 6 || prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 254;
	}
	struct prototype_hott_action_request square_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			.source_bridge_id = full_endpoint_bridge
		}
	};
	uint32_t square_identity_computation_id;
	uint32_t square_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			square_identity_computation,
			&square_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			square_identity_computation_id,
			&square_identity_result_id
		) != 0 || action_results[square_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			square_identity_result_id
		].certificate_id >= action_db.certificate_count || action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT ||
		type_db.type_declarations[action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.generated_type_declaration_id].index_count != 8 ||
		type_db.type_declarations[action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.generated_type_declaration_id].constructor_count != 2) {
		return 255;
	}
	uint32_t generated_square_identity_type = action_certificates[
		action_results[square_identity_result_id].certificate_id
	].data.identity_type.generated_type_declaration_id;
	struct prototype_artifact_identity_root identity_roots[20];
	struct prototype_artifact_dependency identity_dependencies[16];
	struct prototype_artifact_interface identity_interface;
	prototype_artifact_interface_init(
		&identity_interface,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		identity_roots,
		sizeof(identity_roots) / sizeof(identity_roots[0]),
			identity_dependencies,
			16
		);
	uint32_t identity_root_id;
	uint32_t repeated_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&identity_root_id
		) != 0 || identity_root_id != 0 || prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&repeated_identity_root_id
		) != 0 || repeated_identity_root_id != identity_root_id ||
		identity_interface.identity_root_count != 1 ||
		identity_roots[0].identity_family_has_type_claim_id !=
			bool_identity_certificate->data.identity_type.
				identity_type_has_type_claim_id ||
		prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_relation_result_id,
			PROTOTYPE_INVALID_ID,
			&(uint32_t) { 0 }
		) == 0) {
		return 218;
	}
	uint32_t saved_identity_endpoint_context =
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].data.identity_type.endpoint_context_id;
	action_db.certificates[
		action_results[bool_identity_computation_result_id].certificate_id
	].data.identity_type.endpoint_context_id = empty;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 168;
	}
	action_db.certificates[
		action_results[bool_identity_computation_result_id].certificate_id
	].data.identity_type.endpoint_context_id = saved_identity_endpoint_context;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 169;
	}
	uint32_t generated_bool_identity_type =
		bool_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	uint32_t false_identity_args[2] = { bool_false, bool_false };
	uint32_t false_true_identity_args[2] = { bool_false, bool_true };
	uint32_t false_identity_type;
	uint32_t false_true_identity_type;
	uint32_t false_identity_witness;
	uint32_t false_true_invalid_witness;
	uint32_t false_identity_type_claim;
	uint32_t false_identity_witness_claim;
	uint32_t ignored_false_true_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			false_identity_args,
			2,
			&false_identity_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			false_true_identity_args,
			2,
			&false_true_identity_type
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			bool_false_constructor_claim,
			&false_identity_type_claim,
			&false_identity_witness,
			&false_identity_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, false_identity_type_claim
		)->subject != false_identity_type || prototype_judgement_claim_proposition(
			&judgement, false_identity_witness_claim
		)->classifier != false_identity_type || prototype_term_constructor(
			&term_db,
			false_true_identity_type,
			0,
			&false_true_invalid_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_true_invalid_witness,
			false_true_identity_type,
			&ignored_false_true_claim
		) == 0) {
		return 141;
	}
	uint32_t false_square_boundary[8] = {
		bool_false,
		bool_false,
		false_identity_witness,
		bool_false,
		bool_false,
		false_identity_witness,
		false_identity_witness,
		false_identity_witness
	};
	uint32_t mixed_square_boundary[8];
	memcpy(
		mixed_square_boundary,
		false_square_boundary,
		sizeof(false_square_boundary)
	);
	mixed_square_boundary[4] = bool_true;
	uint32_t false_square_type;
	uint32_t mixed_square_type;
	uint32_t false_square_witness;
	uint32_t mixed_square_witness;
	uint32_t false_square_claim;
	uint32_t ignored_mixed_square_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_square_identity_type,
			false_square_boundary,
			8,
			&false_square_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_square_identity_type,
			mixed_square_boundary,
			8,
			&mixed_square_type
		) != 0 || prototype_term_constructor(
			&term_db, false_square_type, 0, &false_square_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_square_witness,
			false_square_type,
			&false_square_claim
		) != 0 || prototype_term_constructor(
			&term_db, mixed_square_type, 0, &mixed_square_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			mixed_square_witness,
			mixed_square_type,
			&ignored_mixed_square_claim
		) == 0) {
		return 246;
	}
	struct prototype_hott_action_request higher_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = false_identity_type_claim,
			.source_bridge_id = bridge
		}
	};
	uint32_t higher_identity_computation_id;
	uint32_t higher_identity_result_id;
	uint32_t false_identity_is_type;
	uint32_t higher_identity_family_claim = PROTOTYPE_INVALID_ID;
	uint32_t higher_identity_witness = PROTOTYPE_INVALID_ID;
	uint32_t higher_identity_witness_claim = PROTOTYPE_INVALID_ID;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			false_identity_type,
			universe,
			false_identity_type_claim,
			&false_identity_is_type
		) != 0) {
		return 245;
	}
	higher_identity_computation.key.identity_type.source_claim_id =
		false_identity_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			higher_identity_computation,
			&higher_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			higher_identity_computation_id,
			&higher_identity_result_id
		) != 0 || action_results[higher_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[higher_identity_result_id].certificate_id >=
			action_db.certificate_count || action_db.certificates[
			action_results[higher_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT) {
		fprintf(
			stderr,
			"higher identity failed result=%u results=%zu state=%d reason=%d "
			"certificate=%u certificates=%zu rule=%d\n",
			higher_identity_result_id,
			action_db.result_count,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].outcome.state : -1,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].outcome.residual_reason : -1,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			action_db.certificate_count,
			higher_identity_result_id < action_db.result_count &&
				action_results[higher_identity_result_id].certificate_id <
					action_db.certificate_count ? action_db.certificates[
					action_results[higher_identity_result_id].certificate_id
				].data.identity_type.computation_rule : -1
		);
		return 245;
	}
	int higher_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			higher_identity_result_id,
			false_identity_witness_claim,
			&higher_identity_family_claim,
			&higher_identity_witness,
			&higher_identity_witness_claim
		);
	if (higher_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, higher_identity_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, higher_identity_family_claim
		)->subject || higher_identity_witness >= term_db.term_count ||
		term_db.terms[higher_identity_witness].tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		fprintf(
			stderr,
			"higher degeneracy failed status=%d family=%u witness=%u claim=%u\n",
			higher_degeneracy_status,
			higher_identity_family_claim,
			higher_identity_witness,
			higher_identity_witness_claim
		);
		return 245;
	}
	uint32_t concrete_identity_root_id;
	if (prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_is_type,
			false_identity_type_claim,
			false_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_identity_root_id
		) != 0 || concrete_identity_root_id != 1 ||
		identity_interface.identity_root_count != 2) {
		return 220;
	}
	uint32_t empty_effect_row;
	uint32_t pure_bool_computation_type;
	uint32_t thunk_bool_type;
	uint32_t thunk_bool_has_type;
	uint32_t thunk_bool_is_type;
	if (prototype_term_effect_label(
			&term_db, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effect_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, bool_view, &pure_bool_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db, pure_bool_computation_type, &thunk_bool_type
		) != 0) {
		return 176;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			&judgement, &term_db, &type_db, &context_db, &substitution_db,
			empty, thunk_bool_type, universe, &thunk_bool_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement, &term_db, empty, thunk_bool_type, universe,
			thunk_bool_has_type, &thunk_bool_is_type
		) != 0) {
		return 236;
	}
	struct prototype_hott_action_request thunk_bool_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t thunk_bool_identity_computation_id;
	uint32_t thunk_bool_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_bool_identity_computation,
			&thunk_bool_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_bool_identity_computation_id,
			&thunk_bool_identity_result_id
		) != 0 || action_results[thunk_bool_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[thunk_bool_identity_result_id].certificate_id >=
			action_db.certificate_count) {
		return 177;
	}
	const struct prototype_hott_action_certificate* thunk_bool_identity_certificate =
		&action_db.certificates[
			action_results[thunk_bool_identity_result_id].certificate_id
		];
	uint32_t generated_thunk_bool_identity_type =
		thunk_bool_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	const struct prototype_type_declaration* generated_thunk_bool_identity =
		&type_db.type_declarations[generated_thunk_bool_identity_type];
	const struct prototype_type_constructor_declaration*
		generated_thunk_bool_return_identity = &type_db.constructor_declarations[
			generated_thunk_bool_identity->first_constructor
		];
	if (thunk_bool_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN ||
		generated_thunk_bool_identity->constructor_count != 1 ||
		prototype_context_get(
			&context_db, generated_thunk_bool_return_identity->field_context
		)->depth != 3 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 178;
	}
	uint32_t false_return;
	uint32_t true_return;
	uint32_t false_thunk;
	uint32_t true_thunk;
	uint32_t thunk_false_identity_type;
	uint32_t thunk_false_true_identity_type;
	uint32_t thunk_false_identity_arguments[2];
	uint32_t thunk_false_true_identity_arguments[2];
	uint32_t thunk_false_identity_witness;
	uint32_t false_return_claim;
	uint32_t false_thunk_claim;
	if (prototype_term_return(
			&term_db, bool_false, &false_return
		) != 0 || prototype_term_return(
			&term_db, bool_true, &true_return
		) != 0 || prototype_term_thunk(
			&term_db, false_return, &false_thunk
		) != 0 || prototype_term_thunk(
			&term_db, true_return, &true_thunk
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_return,
			pure_bool_computation_type,
			bool_false_constructor_claim,
			&false_return_claim
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_thunk,
			thunk_bool_type,
			false_return_claim,
			&false_thunk_claim
		) != 0) {
		return 179;
	}
	thunk_false_identity_arguments[0] = false_thunk;
	thunk_false_identity_arguments[1] = false_thunk;
	thunk_false_true_identity_arguments[0] = false_thunk;
	thunk_false_true_identity_arguments[1] = true_thunk;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_thunk_bool_identity_type,
			thunk_false_identity_arguments,
			2,
			&thunk_false_identity_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_thunk_bool_identity_type,
			thunk_false_true_identity_arguments,
			2,
			&thunk_false_true_identity_type
		) != 0 || prototype_term_constructor(
			&term_db,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			0,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			bool_false,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			bool_false,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			false_identity_witness,
			&thunk_false_identity_witness
		) != 0) {
		return 180;
	}
	struct prototype_judgement_proposition thunk_delta_propositions[8];
	struct prototype_judgement_derivation_candidate thunk_delta_candidates[8];
	struct prototype_judgement_candidate_premise thunk_delta_premises[
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result thunk_delta_motives[1];
	struct prototype_judgement_computation_constraint thunk_delta_constraints[1];
	struct prototype_judgement_effect_row_constraint thunk_delta_effect_rows[1];
	struct prototype_judgement_delta thunk_delta;
	prototype_judgement_delta_init(
		&thunk_delta,
		&judgement,
		thunk_delta_propositions,
		thunk_delta_candidates,
		8,
		thunk_delta_premises,
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		thunk_delta_motives,
		1,
		thunk_delta_constraints,
		1,
		thunk_delta_effect_rows,
		1
	);
	prototype_judgement_delta_set_context_store(
		&thunk_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_context(&thunk_delta, empty);
	struct prototype_judgement_selected_evidence thunk_argument_evidence[3];
	uint32_t thunk_argument_operations[3] = {
		false_operation, false_operation, PROTOTYPE_INVALID_ID
	};
	if (prototype_judgement_delta_select_evidence(
			&thunk_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&thunk_argument_evidence[0]
		) != 0 || prototype_judgement_delta_select_evidence(
			&thunk_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&thunk_argument_evidence[1]
		) != 0 || prototype_judgement_delta_select_evidence(
			&thunk_delta,
			PROTOTYPE_INVALID_ID,
			empty,
			false_identity_witness,
			false_identity_type,
			&thunk_argument_evidence[2]
		) != 0 || prototype_judgement_delta_record_constructor_spine(
			&thunk_delta,
			&term_db,
			&type_db,
			thunk_false_identity_witness,
			thunk_false_identity_type,
			thunk_argument_operations,
			thunk_argument_evidence,
			3
		) != 0 || thunk_delta.proposition_count != 1 ||
		thunk_delta.propositions[0].classifier != thunk_false_identity_type ||
		prototype_judgement_delta_record_constructor_spine(
			&thunk_delta,
			&term_db,
			&type_db,
			thunk_false_identity_witness,
			thunk_false_true_identity_type,
			thunk_argument_operations,
			thunk_argument_evidence,
			3
		) == 0) {
		return 181;
	}
	uint32_t saved_thunk_result =
		generated_thunk_bool_return_identity->result_classifier;
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = thunk_false_true_identity_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 182;
	}
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = saved_thunk_result;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 183;
	}
	uint32_t thunk_degeneracy_family_claim;
	uint32_t thunk_degeneracy_witness;
	uint32_t thunk_degeneracy_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_bool_identity_result_id,
			false_thunk_claim,
			&thunk_degeneracy_family_claim,
			&thunk_degeneracy_witness,
			&thunk_degeneracy_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, thunk_degeneracy_family_claim
		)->subject != thunk_false_identity_type ||
		prototype_judgement_claim_proposition(
			&judgement, thunk_degeneracy_witness_claim
		)->classifier != thunk_false_identity_type ||
		thunk_degeneracy_witness != thunk_false_identity_witness) {
		return 222;
	}
	int found_thunk_constructor_spine = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				thunk_degeneracy_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION &&
			judgement.derivations[i].premise_count == 3) {
			found_thunk_constructor_spine = 1;
			break;
		}
	}
	if (!found_thunk_constructor_spine) {
		return 223;
	}
	uint32_t thunk_bool_is_type_in_endpoint;
	struct prototype_hott_action_request constant_thunk_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_thunk_identity_computation_id;
	uint32_t constant_thunk_identity_result_id;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_is_type,
			first_endpoint_to_empty,
			&thunk_bool_is_type_in_endpoint
		) != 0) {
		return 264;
	}
	constant_thunk_identity_computation.key.identity_type.source_claim_id =
		thunk_bool_is_type_in_endpoint;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_thunk_identity_computation,
			&constant_thunk_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_thunk_identity_computation_id,
			&constant_thunk_identity_result_id
		) != 0 || action_results[
			constant_thunk_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[constant_thunk_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 265;
	}
	uint32_t false_return_in_endpoint;
	uint32_t false_return_in_endpoint_claim;
	uint32_t false_thunk_in_endpoint;
	uint32_t false_thunk_in_endpoint_claim;
	uint32_t constant_thunk_family_claim;
	uint32_t constant_thunk_witness;
	uint32_t constant_thunk_witness_claim;
	if (prototype_term_return(
			&term_db, bool_false, &false_return_in_endpoint
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			false_return_in_endpoint,
			pure_bool_computation_type,
			endpoint_false_claim,
			&false_return_in_endpoint_claim
		) != 0 || prototype_term_thunk(
			&term_db, false_return_in_endpoint, &false_thunk_in_endpoint
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			false_thunk_in_endpoint,
			thunk_bool_type,
			false_return_in_endpoint_claim,
			&false_thunk_in_endpoint_claim
		) != 0) {
		return 266;
	}
	int constant_thunk_action_status = prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_thunk_identity_result_id,
			false_thunk_in_endpoint_claim,
			&constant_thunk_family_claim,
			&constant_thunk_witness,
			&constant_thunk_witness_claim
		);
	if (constant_thunk_action_status != 0 || constant_thunk_witness >=
			term_db.term_count || term_db.terms[
			constant_thunk_witness
		].tag != PROTOTYPE_TERM_APP || constant_thunk_witness !=
			thunk_degeneracy_witness || prototype_judgement_claim_proposition(
			&judgement, constant_thunk_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, constant_thunk_family_claim
		)->subject) {
		fprintf(
			stderr,
			"constant thunk action failed status=%d family=%u witness=%u claim=%u\n",
			constant_thunk_action_status,
			constant_thunk_family_claim,
			constant_thunk_witness,
			constant_thunk_witness_claim
		);
		return 266;
	}
	uint32_t thunk_identity_root_id;
	uint32_t concrete_thunk_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_bool_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&thunk_identity_root_id
		) != 0 || thunk_identity_root_id != 2 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_is_type,
			thunk_degeneracy_family_claim,
			thunk_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			&concrete_thunk_identity_root_id
		) != 0 || concrete_thunk_identity_root_id != 3 ||
		prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 228;
	}
	uint32_t artifact_saved_thunk_result = type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier;
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = thunk_false_true_identity_type;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) == 0) {
		return 230;
	}
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = artifact_saved_thunk_result;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 231;
	}
	uint32_t bool_function_codomain_binding = prototype_term_new_binding(&term_db);
	uint32_t bool_function_codomain_family;
	uint32_t bool_function_computation_type;
	uint32_t thunk_bool_function_type;
	uint32_t thunk_bool_function_has_type;
	uint32_t thunk_bool_function_is_type;
	if (bool_function_codomain_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			&term_db,
			bool_function_codomain_binding,
			pure_bool_computation_type,
			&bool_function_codomain_family
		) != 0 || prototype_term_pi_family(
			&term_db,
			bool_view,
			bool_function_codomain_family,
			&bool_function_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db,
			bool_function_computation_type,
			&thunk_bool_function_type
		) != 0) {
		return 184;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			&judgement, &term_db, &type_db, &context_db, &substitution_db,
			empty, thunk_bool_function_type, universe,
			&thunk_bool_function_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement, &term_db, empty, thunk_bool_function_type, universe,
			thunk_bool_function_has_type, &thunk_bool_function_is_type
		) != 0) {
		return 238;
	}
	uint32_t identity_function_binding = prototype_term_new_binding(&term_db);
	uint32_t identity_function_context;
	uint32_t identity_function_context_certificate;
	uint32_t identity_function_var;
	uint32_t identity_function_var_claim;
	uint32_t identity_function_return;
	uint32_t identity_function_return_claim;
	uint32_t identity_function_lambda;
	uint32_t identity_function_lambda_claim;
	uint32_t identity_function_value;
	uint32_t identity_function_value_claim;
	if (identity_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			identity_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&identity_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			identity_function_context,
			bool_is_type,
			&identity_function_context_certificate
		) != 0 || prototype_term_var(
			&term_db, identity_function_binding, &identity_function_var
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			identity_function_context,
			identity_function_binding,
			bool_view,
			&identity_function_var_claim
		) != 0 || prototype_term_return(
			&term_db, identity_function_var, &identity_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			identity_function_context,
			identity_function_return,
			pure_bool_computation_type,
			identity_function_var_claim,
			&identity_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			identity_function_binding,
			identity_function_return,
			&identity_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			identity_function_lambda,
			bool_function_computation_type,
			identity_function_var_claim,
			identity_function_return_claim,
			&identity_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, identity_function_lambda, &identity_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			identity_function_value,
			thunk_bool_function_type,
			identity_function_lambda_claim,
			&identity_function_value_claim
		) != 0) {
		return 224;
	}
	struct prototype_hott_action_request bool_function_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_bool_function_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_function_identity_computation_id;
	uint32_t bool_function_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_function_identity_computation,
			&bool_function_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_computation_id,
			&bool_function_identity_result_id
		) != 0 || action_results[bool_function_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[bool_function_identity_result_id].certificate_id >=
			action_db.certificate_count) {
		return 185;
	}
	struct prototype_hott_action_certificate* bool_function_identity_certificate =
		&action_db.certificates[
			action_results[bool_function_identity_result_id].certificate_id
		];
	if (bool_function_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE ||
		bool_function_identity_certificate->data.identity_type.
			generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
		bool_function_identity_certificate->data.identity_type.identity_type_term_id >=
			term_db.term_count || term_db.terms[
			bool_function_identity_certificate->data.identity_type.identity_type_term_id
		].tag != PROTOTYPE_TERM_THUNK_TYPE || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 186;
	}
	/* The constant Match is judgementally equal to its branch body under the
	 * exhaustive constant-eliminator rule. This fixture checks that the Pi
	 * identity family observes the same conversion rule. */
	uint32_t constant_function_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_function_context;
	uint32_t constant_function_context_certificate;
	uint32_t constant_function_argument_claim;
	uint32_t constant_function_projection;
	uint32_t constant_true_claim;
	uint32_t pure_bool_has_type_in_constant_context;
	uint32_t pure_bool_is_type_in_constant_context;
	uint32_t constant_true_return;
	uint32_t constant_true_return_claim;
	if (constant_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			constant_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_function_context,
			bool_is_type,
			&constant_function_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_function_context,
			constant_function_binding,
			bool_view,
			&constant_function_argument_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_function_context,
			empty,
			&constant_function_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			true_claim,
			constant_function_projection,
			&constant_true_claim
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			pure_bool_computation_type,
			universe,
			&pure_bool_has_type_in_constant_context
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			constant_function_context,
			pure_bool_computation_type,
			universe,
			pure_bool_has_type_in_constant_context,
			&pure_bool_is_type_in_constant_context
		) != 0 || prototype_term_return(
			&term_db, bool_true, &constant_true_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			constant_function_context,
			constant_true_return,
			pure_bool_computation_type,
			constant_true_claim,
			&constant_true_return_claim
		) != 0) {
		return 319;
	}
	uint32_t constant_function_lambda;
	uint32_t constant_function_lambda_claim;
	uint32_t constant_function_value;
	uint32_t constant_function_value_claim;
	if (prototype_term_lambda(
			&term_db,
			constant_function_binding,
			constant_true_return,
			&constant_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_function_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			constant_true_return_claim,
			&constant_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, constant_function_lambda, &constant_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			constant_function_value,
			thunk_bool_function_type,
			constant_function_lambda_claim,
			&constant_function_value_claim
		) != 0) {
		return 320;
	}
	uint32_t constant_function_argument;
	if (prototype_term_var(
			&term_db, constant_function_binding, &constant_function_argument
		) != 0) {
		return 321;
	}
	struct prototype_match_case_input constant_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = constant_true_return
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = constant_true_return
		}
	};
	uint32_t matched_constant_body;
	uint32_t matched_constant_body_claim;
	uint32_t constant_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_motive_context;
	uint32_t constant_motive_context_certificate;
	uint32_t constant_motive_projection;
	uint32_t bool_is_type_in_constant_context;
	uint32_t constant_motive_binder_claim;
	uint32_t pure_bool_has_type_in_motive_context;
	uint32_t constant_motive;
	uint32_t constant_motive_classifier;
	uint32_t constant_motive_claim;
	uint32_t matched_constant_classifier;
	uint32_t matched_constant_classifier_claim;
	uint32_t matched_constant_classifier_is_type_claim;
	uint32_t constant_branch_claims[2] = {
		constant_true_return_claim,
		constant_true_return_claim
	};
	if (constant_motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			constant_function_context,
			constant_motive_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_motive_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			constant_function_projection,
			&bool_is_type_in_constant_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_motive_context,
			bool_is_type_in_constant_context,
			&constant_motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_motive_context,
			constant_function_context,
			&constant_motive_projection
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_motive_context,
			constant_motive_binding,
			bool_view,
			&constant_motive_binder_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_bool_has_type_in_constant_context,
			constant_motive_projection,
			&pure_bool_has_type_in_motive_context
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_motive_binding,
			pure_bool_computation_type,
			&constant_motive
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			universe,
			&constant_motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			constant_motive,
			constant_motive_classifier,
			constant_motive_binder_claim,
			pure_bool_has_type_in_motive_context,
			&constant_motive_claim
		) != 0 || prototype_term_app(
			&term_db,
			constant_motive,
			constant_function_argument,
			&matched_constant_classifier
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			matched_constant_classifier,
			universe,
			constant_motive_claim,
			constant_function_argument_claim,
			&matched_constant_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			constant_function_context,
			matched_constant_classifier,
			universe,
			matched_constant_classifier_claim,
			&matched_constant_classifier_is_type_claim
		) != 0 || prototype_term_match(
			&term_db,
			constant_function_argument,
			constant_match_cases,
			2,
			&matched_constant_body
		) != 0 || prototype_judgement_add_match_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&operation_graph,
			constant_function_context,
			matched_constant_body,
			matched_constant_classifier,
			matched_constant_classifier_claim,
			constant_branch_claims,
			2,
			&matched_constant_body_claim
		) != 0) {
		return 322;
	}
	uint32_t matched_constant_lambda;
	uint32_t matched_constant_lambda_claim;
	uint32_t matched_constant_function_value;
	uint32_t matched_constant_function_value_claim;
	if (prototype_term_lambda(
			&term_db,
			constant_function_binding,
			matched_constant_body,
			&matched_constant_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			matched_constant_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			matched_constant_body_claim,
			&matched_constant_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db,
			matched_constant_lambda,
			&matched_constant_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			matched_constant_function_value,
			thunk_bool_function_type,
			matched_constant_lambda_claim,
			&matched_constant_function_value_claim
		) != 0) {
		return 323;
	}
	if (prototype_judgement_classifier_conversion(
			&term_db,
			&type_db,
			constant_function_value,
			matched_constant_function_value
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 324;
	}
	uint32_t constant_function_identity_family_claim;
	if (prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			matched_constant_function_value_claim,
			&constant_function_identity_family_claim
		) != 0) {
		return 325;
	}
	uint32_t constant_function_degeneracy_family_claim;
	uint32_t constant_function_degeneracy_witness;
	uint32_t constant_function_degeneracy_witness_claim;
	uint32_t checked_constant_function_family_claim;
	uint32_t checked_constant_function_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			&constant_function_degeneracy_family_claim,
			&constant_function_degeneracy_witness,
			&constant_function_degeneracy_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			matched_constant_function_value_claim,
			constant_function_degeneracy_witness_claim,
			&checked_constant_function_family_claim,
			&checked_constant_function_witness_claim
		) != 0 || checked_constant_function_family_claim !=
			constant_function_identity_family_claim) {
		return 326;
	}
	uint32_t observed_false_claim;
	uint32_t observed_false_return;
	uint32_t observed_false_return_claim;
	uint32_t observed_false_lambda;
	uint32_t observed_false_lambda_claim;
	uint32_t observed_false_function;
	uint32_t observed_false_function_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			false_claim,
			constant_function_projection,
			&observed_false_claim
		) != 0 || prototype_term_return(
			&term_db, bool_false, &observed_false_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			constant_function_context,
			observed_false_return,
			pure_bool_computation_type,
			observed_false_claim,
			&observed_false_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_function_binding,
			observed_false_return,
			&observed_false_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			observed_false_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			observed_false_return_claim,
			&observed_false_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, observed_false_lambda, &observed_false_function
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			observed_false_function,
			thunk_bool_function_type,
			observed_false_lambda_claim,
			&observed_false_function_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			observed_false_function_claim,
			constant_function_degeneracy_witness_claim,
			&checked_constant_function_family_claim,
			&checked_constant_function_witness_claim
		) == 0) {
		return 327;
	}
	(void)constant_function_context_certificate;
	(void)constant_function_degeneracy_family_claim;
	(void)constant_function_degeneracy_witness;
	uint32_t pure_function_value_computation_type;
	uint32_t thunk_return_function_type;
	uint32_t thunk_return_function_has_type;
	uint32_t thunk_return_function_is_type;
	uint32_t returned_function_value;
	uint32_t returned_function_value_claim;
	uint32_t thunked_returned_function;
	uint32_t thunked_returned_function_claim;
	if (prototype_term_computation_type(
			&term_db,
			empty_effect_row,
			thunk_bool_function_type,
			&pure_function_value_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db,
			pure_function_value_computation_type,
			&thunk_return_function_type
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			thunk_return_function_type,
			universe,
			&thunk_return_function_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			thunk_return_function_type,
			universe,
			thunk_return_function_has_type,
			&thunk_return_function_is_type
		) != 0 || prototype_term_return(
			&term_db, identity_function_value, &returned_function_value
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			returned_function_value,
			pure_function_value_computation_type,
			identity_function_value_claim,
			&returned_function_value_claim
		) != 0 || prototype_term_thunk(
			&term_db, returned_function_value, &thunked_returned_function
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			thunked_returned_function,
			thunk_return_function_type,
			returned_function_value_claim,
			&thunked_returned_function_claim
		) != 0) {
		return 267;
	}
	struct prototype_hott_action_request thunk_return_function_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_return_function_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t thunk_return_function_identity_request_id;
	uint32_t thunk_return_function_identity_result_id;
	uint32_t thunk_return_function_family_claim;
	uint32_t thunk_return_function_witness;
	uint32_t thunk_return_function_witness_claim;
	int thunk_return_function_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_return_function_identity_request,
			&thunk_return_function_identity_request_id
		);
	int thunk_return_function_execution_status =
		thunk_return_function_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_return_function_identity_request_id,
			&thunk_return_function_identity_result_id
		) : -1;
	if (thunk_return_function_request_status != 0 ||
		thunk_return_function_execution_status != 0 || action_results[
			thunk_return_function_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[thunk_return_function_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
		uint32_t generated_nested_identity = PROTOTYPE_INVALID_ID;
		int generated_nested_status =
			prototype_type_declaration_find_generated_identity(
				&type_db,
				thunk_return_function_type,
				empty,
				&generated_nested_identity
			);
		int generated_nested_valid = generated_nested_status == 0 ?
			prototype_type_declaration_validate_generated_identity(
				&term_db,
				&type_db,
				&context_db,
				thunk_return_function_type,
				generated_nested_identity,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN
			) : -1;
		fprintf(
			stderr,
			"nested Pi carrier identity computation failed intern=%d execute=%d "
			"request=%u result=%u "
			"state=%d result_request=%u certificate=%u rule=%d generated=%u "
			"generated_status=%d generated_valid=%d\n",
			thunk_return_function_request_status,
			thunk_return_function_execution_status,
			thunk_return_function_identity_request_id,
			thunk_return_function_identity_result_id,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].outcome.state :
				-1,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].request_id :
				PROTOTYPE_INVALID_ID,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			thunk_return_function_identity_result_id < action_db.result_count &&
				action_results[thunk_return_function_identity_result_id].certificate_id <
					action_db.certificate_count ? action_db.certificates[
						action_results[
							thunk_return_function_identity_result_id
						].certificate_id
					].data.identity_type.computation_rule : -1,
			generated_nested_identity,
			generated_nested_status,
			generated_nested_valid
		);
		return 268;
	}
	int thunk_return_function_degeneracy_status =
		prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_return_function_identity_result_id,
			thunked_returned_function_claim,
			&thunk_return_function_family_claim,
			&thunk_return_function_witness,
			&thunk_return_function_witness_claim
		);
	if (thunk_return_function_degeneracy_status != 0 ||
		prototype_judgement_claim_proposition(
			&judgement, thunk_return_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, thunk_return_function_family_claim
		)->subject) {
		fprintf(
			stderr,
			"nested Pi carrier degeneracy failed status=%d family=%u witness=%u "
			"claim=%u\n",
			thunk_return_function_degeneracy_status,
			thunk_return_function_family_claim,
			thunk_return_function_witness,
			thunk_return_function_witness_claim
		);
		return 268;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		fprintf(stderr, "nested Pi carrier replay failed\n");
		return 268;
	}
	uint32_t thunk_bool_function_is_type_in_endpoint;
	struct prototype_hott_action_request constant_pi_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_pi_identity_computation_id;
	uint32_t constant_pi_identity_result_id;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_function_is_type,
			first_endpoint_to_empty,
			&thunk_bool_function_is_type_in_endpoint
		) != 0) {
		return 256;
	}
	constant_pi_identity_computation.key.identity_type.source_claim_id =
		thunk_bool_function_is_type_in_endpoint;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_pi_identity_computation,
			&constant_pi_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_pi_identity_computation_id,
			&constant_pi_identity_result_id
		) != 0 || action_results[
			constant_pi_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			constant_pi_identity_result_id
		].certificate_id >= action_db.certificate_count || action_db.certificates[
			action_results[constant_pi_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT ||
		action_db.certificates[action_results[
			constant_pi_identity_result_id
		].certificate_id].data.identity_type.generated_type_declaration_id !=
			PROTOTYPE_INVALID_ID || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 257;
	}
	uint32_t contextual_function_binding = prototype_term_new_binding(&term_db);
	uint32_t contextual_function_body_context;
	uint32_t contextual_function_body_context_certificate;
	uint32_t contextual_function_argument;
	uint32_t contextual_function_argument_claim;
	uint32_t contextual_function_outer_value;
	uint32_t contextual_function_outer_value_claim;
	uint32_t contextual_function_return;
	uint32_t contextual_function_return_claim;
	uint32_t contextual_function_lambda;
	uint32_t contextual_function_lambda_claim;
	uint32_t contextual_function_value;
	uint32_t contextual_function_value_claim;
	if (contextual_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			bool_identity_endpoint_path[0],
			contextual_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&contextual_function_body_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			contextual_function_body_context,
			bool_is_type_in_first_endpoint,
			&contextual_function_body_context_certificate
		) != 0 || prototype_term_var(
			&term_db, contextual_function_binding, &contextual_function_argument
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			contextual_function_body_context,
			contextual_function_binding,
			bool_view,
			&contextual_function_argument_claim
		) != 0 || prototype_term_var(
			&term_db, endpoint_bool_binding, &contextual_function_outer_value
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			contextual_function_body_context,
			endpoint_bool_binding,
			bool_view,
			&contextual_function_outer_value_claim
		) != 0 || prototype_term_return(
			&term_db,
			contextual_function_outer_value,
			&contextual_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			contextual_function_body_context,
			contextual_function_return,
			pure_bool_computation_type,
			contextual_function_outer_value_claim,
			&contextual_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			contextual_function_binding,
			contextual_function_return,
			&contextual_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_identity_endpoint_path[0],
			contextual_function_lambda,
			bool_function_computation_type,
			contextual_function_argument_claim,
			contextual_function_return_claim,
			&contextual_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, contextual_function_lambda, &contextual_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			contextual_function_value,
			thunk_bool_function_type,
			contextual_function_lambda_claim,
			&contextual_function_value_claim
		) != 0) {
		return 262;
	}
	uint32_t contextual_function_family_claim;
	uint32_t contextual_function_witness;
	uint32_t contextual_function_witness_claim;
	int contextual_function_action_status =
		prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_pi_identity_result_id,
			contextual_function_value_claim,
			&contextual_function_family_claim,
			&contextual_function_witness,
			&contextual_function_witness_claim
		);
	if (contextual_function_action_status != 0 ||
		contextual_function_witness >= term_db.term_count || term_db.terms[
			contextual_function_witness
		].tag != PROTOTYPE_TERM_THUNK || prototype_judgement_claim_proposition(
			&judgement, contextual_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, contextual_function_family_claim
		)->subject) {
		fprintf(
			stderr,
			"contextual Pi action failed status=%d family=%u witness=%u claim=%u\n",
			contextual_function_action_status,
			contextual_function_family_claim,
			contextual_function_witness,
			contextual_function_witness_claim
		);
		return 263;
	}
	(void)contextual_function_body_context_certificate;
	uint32_t pi_variable_binding = prototype_term_new_binding(&term_db);
	uint32_t pi_variable_context;
	uint32_t pi_variable_context_certificate;
	uint32_t pi_variable_bridge;
	if (pi_variable_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			pi_variable_binding,
			thunk_bool_function_type,
			PROTOTYPE_INVALID_ID,
			&pi_variable_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			pi_variable_context,
			thunk_bool_function_is_type,
			&pi_variable_context_certificate
		) != 0 || prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			pi_variable_context,
			bool_function_identity_computation_id,
			&pi_variable_bridge
		) != 0) {
		return 258;
	}
	uint32_t pi_variable_projection;
	uint32_t pi_type_in_variable_context;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			pi_variable_context,
			empty,
			&pi_variable_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_function_is_type,
			pi_variable_projection,
			&pi_type_in_variable_context
		) != 0) {
		return 259;
	}
	struct prototype_hott_action_request pi_variable_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = pi_type_in_variable_context,
			.source_bridge_id = pi_variable_bridge
		}
	};
	uint32_t pi_variable_identity_computation_id;
	uint32_t pi_variable_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			pi_variable_identity_computation,
			&pi_variable_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			pi_variable_identity_computation_id,
			&pi_variable_identity_result_id
		) != 0 || action_results[pi_variable_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[pi_variable_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 260;
	}
	uint32_t pi_variable_claim;
	uint32_t pi_variable_family_claim;
	uint32_t pi_variable_witness;
	uint32_t pi_variable_witness_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			pi_variable_context,
			pi_variable_binding,
			thunk_bool_function_type,
			&pi_variable_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			pi_variable_identity_result_id,
			pi_variable_claim,
			&pi_variable_family_claim,
			&pi_variable_witness,
			&pi_variable_witness_claim
		) != 0 || pi_variable_witness !=
			prototype_judgement_claim_proposition(
				&judgement,
				bridge_certificates[pi_variable_bridge].fiber_witness_claim_id
			)->subject || prototype_judgement_claim_proposition(
			&judgement, pi_variable_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, pi_variable_family_claim
		)->subject || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 261;
	}
	uint32_t applied_function_bool_type_claim;
	uint32_t applied_function_binding = prototype_term_new_binding(&term_db);
	uint32_t applied_function_context;
	uint32_t applied_function_context_certificate;
	uint32_t applied_function_variable;
	uint32_t applied_function_variable_claim;
	uint32_t applied_argument_variable;
	uint32_t applied_argument_claim;
	uint32_t applied_force;
	uint32_t applied_force_claim;
	uint32_t applied_body;
	uint32_t applied_body_claim;
	uint32_t applied_lambda;
	uint32_t applied_lambda_claim;
	uint32_t applied_value;
	uint32_t applied_value_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			pi_variable_projection,
			&applied_function_bool_type_claim
		) != 0 || applied_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			pi_variable_context,
			applied_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&applied_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			applied_function_context,
			applied_function_bool_type_claim,
			&applied_function_context_certificate
		) != 0 || prototype_term_var(
			&term_db, pi_variable_binding, &applied_function_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			applied_function_context,
			pi_variable_binding,
			thunk_bool_function_type,
			&applied_function_variable_claim
		) != 0 || prototype_term_var(
			&term_db, applied_function_binding, &applied_argument_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			applied_function_context,
			applied_function_binding,
			bool_view,
			&applied_argument_claim
		) != 0 || prototype_term_force(
			&term_db, applied_function_variable, &applied_force
		) != 0 || prototype_judgement_add_force_claim(
			&judgement,
			&term_db,
			applied_function_context,
			applied_force,
			bool_function_computation_type,
			applied_function_variable_claim,
			&applied_force_claim
		) != 0 || prototype_term_app(
			&term_db, applied_force, applied_argument_variable, &applied_body
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			applied_function_context,
			applied_body,
			pure_bool_computation_type,
			applied_force_claim,
			applied_argument_claim,
			&applied_body_claim
		) != 0 || prototype_term_lambda(
			&term_db, applied_function_binding, applied_body, &applied_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pi_variable_context,
			applied_lambda,
			bool_function_computation_type,
			applied_argument_claim,
			applied_body_claim,
			&applied_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, applied_lambda, &applied_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			pi_variable_context,
			applied_value,
			thunk_bool_function_type,
			applied_lambda_claim,
			&applied_value_claim
		) != 0) {
		return 269;
	}
	uint32_t applied_family_claim;
	uint32_t applied_witness;
	uint32_t applied_witness_claim;
	size_t applied_derivation_start = judgement.derivation_count;
	int applied_action_status = prototype_hott_construct_object_term_action(
		&action_db,
		&kernel_builder,
		&bridge_db,
		pi_variable_identity_result_id,
		applied_value_claim,
		&applied_family_claim,
		&applied_witness,
		&applied_witness_claim
	);
	int found_applied_fold = 0;
	for (size_t i = applied_derivation_start; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM) {
			found_applied_fold = 1;
			break;
		}
	}
	if (applied_action_status != 0 || !found_applied_fold ||
		applied_witness >= term_db.term_count ||
		term_db.terms[applied_witness].tag != PROTOTYPE_TERM_THUNK ||
		prototype_judgement_claim_proposition(
			&judgement, applied_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, applied_family_claim
		)->subject || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		fprintf(
			stderr,
			"applied Pi action failed status=%d fold=%d family=%u witness=%u "
			"claim=%u\n",
			applied_action_status,
			found_applied_fold,
			applied_family_claim,
			applied_witness,
			applied_witness_claim
		);
		return 269;
	}
	(void)applied_function_context_certificate;
	(void)pi_variable_context_certificate;
	uint32_t saved_pi_identity_term =
		bool_function_identity_certificate->data.identity_type.identity_type_term_id;
	bool_function_identity_certificate->data.identity_type.identity_type_term_id =
		thunk_bool_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 187;
	}
	bool_function_identity_certificate->data.identity_type.identity_type_term_id =
		saved_pi_identity_term;
	size_t pi_replay_term_count = term_db.term_count;
	uint32_t pi_replay_next_binding = term_db.next_binding_id;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0 || term_db.term_count != pi_replay_term_count ||
		term_db.next_binding_id != pi_replay_next_binding) {
		return 188;
	}
	uint32_t pi_degeneracy_family;
	if (prototype_term_graph_substitute_bound_var(
			&term_db,
			&type_db,
			bool_function_identity_certificate->data.identity_type.
				identity_type_term_id,
			bool_function_identity_certificate->data.identity_type.
				left_endpoint_binding_id,
			identity_function_value,
			&pi_degeneracy_family
		) != 0 || prototype_term_graph_substitute_bound_var(
			&term_db,
			&type_db,
			pi_degeneracy_family,
			bool_function_identity_certificate->data.identity_type.
				right_endpoint_binding_id,
			identity_function_value,
			&pi_degeneracy_family
		) != 0) {
		return 225;
	}
	uint32_t pi_degeneracy_family_claim = PROTOTYPE_INVALID_ID;
	uint32_t pi_degeneracy_witness = PROTOTYPE_INVALID_ID;
	uint32_t pi_degeneracy_witness_claim = PROTOTYPE_INVALID_ID;
	int pi_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			identity_function_value_claim,
			&pi_degeneracy_family_claim,
			&pi_degeneracy_witness,
			&pi_degeneracy_witness_claim
		);
	if (pi_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, pi_degeneracy_family_claim
		)->subject != pi_degeneracy_family ||
		prototype_judgement_claim_proposition(
			&judgement, pi_degeneracy_witness_claim
		)->classifier != pi_degeneracy_family ||
		pi_degeneracy_witness >= term_db.term_count ||
		term_db.terms[pi_degeneracy_witness].tag != PROTOTYPE_TERM_THUNK) {
		fprintf(
			stderr,
			"pi degeneracy failed status=%d family=%u witness=%u claim=%u\n",
			pi_degeneracy_status,
			pi_degeneracy_family_claim,
			pi_degeneracy_witness,
			pi_degeneracy_witness_claim
		);
		return 226;
	}
	uint32_t pi_ap_left;
	uint32_t pi_ap_right;
	uint32_t pi_ap_identity;
	if (apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_degeneracy_witness,
			bool_false,
			&pi_ap_left
		) != 0 || apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_ap_left,
			bool_false,
			&pi_ap_right
		) != 0 || apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_ap_right,
			false_identity_witness,
			&pi_ap_identity
		) != 0 || pi_ap_identity != thunk_degeneracy_witness) {
		fprintf(
			stderr,
			"Pi ap/refl law failed left=%u right=%u result=%u expected=%u\n",
			pi_ap_left,
			pi_ap_right,
			pi_ap_identity,
			thunk_degeneracy_witness
		);
		return 271;
	}
	uint32_t composed_identity_binding = prototype_term_new_binding(&term_db);
	uint32_t composed_identity_context;
	uint32_t composed_identity_context_certificate;
	uint32_t composed_identity_projection;
	uint32_t composed_identity_function_claim;
	uint32_t composed_identity_argument;
	uint32_t composed_identity_argument_claim;
	uint32_t composed_identity_force;
	uint32_t composed_identity_force_claim;
	uint32_t composed_identity_app;
	uint32_t composed_identity_app_claim;
	uint32_t composed_identity_lambda;
	uint32_t composed_identity_lambda_claim;
	uint32_t composed_identity_value;
	uint32_t composed_identity_value_claim;
	uint32_t composed_identity_family_claim;
	uint32_t composed_identity_witness;
	uint32_t composed_identity_witness_claim;
	if (composed_identity_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			composed_identity_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&composed_identity_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			composed_identity_context,
			bool_is_type,
			&composed_identity_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			composed_identity_context,
			empty,
			&composed_identity_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			identity_function_value_claim,
			composed_identity_projection,
			&composed_identity_function_claim
		) != 0 || prototype_term_var(
			&term_db, composed_identity_binding, &composed_identity_argument
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			composed_identity_context,
			composed_identity_binding,
			bool_view,
			&composed_identity_argument_claim
		) != 0 || prototype_term_force(
			&term_db, identity_function_value, &composed_identity_force
		) != 0 || prototype_judgement_add_force_claim(
			&judgement,
			&term_db,
			composed_identity_context,
			composed_identity_force,
			bool_function_computation_type,
			composed_identity_function_claim,
			&composed_identity_force_claim
		) != 0 || prototype_term_app(
			&term_db,
			composed_identity_force,
			composed_identity_argument,
			&composed_identity_app
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			composed_identity_context,
			composed_identity_app,
			pure_bool_computation_type,
			composed_identity_force_claim,
			composed_identity_argument_claim,
			&composed_identity_app_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			composed_identity_binding,
			composed_identity_app,
			&composed_identity_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			composed_identity_lambda,
			bool_function_computation_type,
			composed_identity_argument_claim,
			composed_identity_app_claim,
			&composed_identity_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, composed_identity_lambda, &composed_identity_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			composed_identity_value,
			thunk_bool_function_type,
			composed_identity_lambda_claim,
			&composed_identity_value_claim
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			composed_identity_value_claim,
			&composed_identity_family_claim,
			&composed_identity_witness,
			&composed_identity_witness_claim
		) != 0) {
		return 272;
	}
	(void)composed_identity_context_certificate;
	uint32_t pi_identity_root_id;
	uint32_t concrete_pi_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_function_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&pi_identity_root_id
		) != 0 || pi_identity_root_id != 4 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_function_is_type,
			pi_degeneracy_family_claim,
			pi_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&concrete_pi_identity_root_id
		) != 0 || concrete_pi_identity_root_id != 5 ||
		prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 229;
	}
	uint32_t artifact_pi_family =
		bool_function_identity_certificate->data.identity_type.identity_type_term_id;
	uint32_t artifact_outer_pi = artifact_pi_family < term_db.term_count &&
		term_db.terms[artifact_pi_family].tag == PROTOTYPE_TERM_THUNK_TYPE ?
		term_db.terms[artifact_pi_family].as.thunk_type.computation :
		PROTOTYPE_INVALID_ID;
	if (artifact_outer_pi >= term_db.term_count ||
		term_db.terms[artifact_outer_pi].tag != PROTOTYPE_TERM_PI) {
		return 232;
	}
	uint32_t artifact_saved_pi_domain =
		term_db.terms[artifact_outer_pi].as.pi.domain;
	struct prototype_term* forged_pi_terms = malloc(
		term_db.term_capacity * sizeof(*forged_pi_terms)
	);
	if (!forged_pi_terms) {
		return 233;
	}
	memcpy(
		forged_pi_terms,
		term_db.terms,
		term_db.term_capacity * sizeof(*forged_pi_terms)
	);
	struct prototype_term_db forged_pi_term_db = term_db;
	forged_pi_term_db.terms = forged_pi_terms;
	forged_pi_term_db.terms[artifact_outer_pi].as.pi.domain = box_view;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&forged_pi_term_db,
			&type_db,
			&context_db,
			&judgement
		) == 0) {
		free(forged_pi_terms);
		return 233;
	}
	free(forged_pi_terms);
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 234;
	}
	(void)artifact_saved_pi_domain;
	uint32_t constant_family_function_projection;
	uint32_t constant_function_false_claim;
	uint32_t constant_function_return;
	uint32_t constant_function_return_claim;
	uint32_t constant_family_function_lambda;
	uint32_t constant_family_function_lambda_claim;
	uint32_t constant_family_function_value;
	uint32_t constant_family_function_value_claim;
	uint32_t constant_function_family_claim;
	uint32_t constant_function_witness;
	uint32_t constant_function_witness_claim;
	uint32_t canonical_false_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_false,
			bool_view,
			&canonical_false_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			identity_function_context,
			empty,
			&constant_family_function_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			canonical_false_claim,
			constant_family_function_projection,
			&constant_function_false_claim
		) != 0 || prototype_term_return(
			&term_db, bool_false, &constant_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			identity_function_context,
			constant_function_return,
			pure_bool_computation_type,
			constant_function_false_claim,
			&constant_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			identity_function_binding,
			constant_function_return,
			&constant_family_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_family_function_lambda,
			bool_function_computation_type,
			identity_function_var_claim,
			constant_function_return_claim,
			&constant_family_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db,
			constant_family_function_lambda,
			&constant_family_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			constant_family_function_value,
			thunk_bool_function_type,
			constant_family_function_lambda_claim,
			&constant_family_function_value_claim
		) != 0) {
		return 227;
	}
	int constant_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_family_function_value_claim,
			&constant_function_family_claim,
			&constant_function_witness,
			&constant_function_witness_claim
		);
	if (constant_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, constant_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, constant_function_family_claim
		)->subject || constant_function_witness >= term_db.term_count ||
		term_db.terms[constant_function_witness].tag != PROTOTYPE_TERM_THUNK) {
		return 227;
	}
	uint32_t extensional_identity_family_claim;
	uint32_t extensional_identity_witness;
	uint32_t extensional_identity_witness_claim;
	int extensional_identity_status = check_bool_function_extensional_identity(
		&term_db,
		&type_db,
		&context_db,
		&substitution_db,
		&judgement,
		&operation_graph,
		empty,
		universe,
		bool_view,
		bool_false,
		bool_true,
		generated_bool_identity_type,
		generated_thunk_bool_identity_type,
		thunk_bool_identity_certificate->data.identity_type.
			backing_type_former_term_id,
		thunk_bool_function_type,
		&bool_function_identity_certificate->data.identity_type,
		&extensional_identity_family_claim,
		&extensional_identity_witness,
		&extensional_identity_witness_claim
	);
	uint32_t extensional_identity_root;
	if (extensional_identity_status != 0 ||
		extensional_identity_witness >= term_db.term_count ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_function_is_type,
			extensional_identity_family_claim,
			extensional_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&extensional_identity_root
		) != 0 || prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 189;
	}
	(void)extensional_identity_root;
	struct prototype_hott_action_request box_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t box_identity_computation_id;
	uint32_t box_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			box_identity_computation,
			&box_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_id,
			&box_identity_computation_result_id
		) != 0 || action_results[box_identity_computation_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[box_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 170;
	}
	const struct prototype_hott_action_certificate* box_identity_certificate =
		&action_db.certificates[
			action_results[box_identity_computation_result_id].certificate_id
		];
	uint32_t generated_box_identity_type =
		box_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	const struct prototype_type_declaration* generated_box_identity_declaration =
		&type_db.type_declarations[generated_box_identity_type];
	const struct prototype_type_constructor_declaration*
		generated_box_identity_constructor = &type_db.constructor_declarations[
			generated_box_identity_declaration->first_constructor
		];
	const struct prototype_context* generated_box_identity_fields =
		prototype_context_get(
			&context_db, generated_box_identity_constructor->field_context
		);
	if (box_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		generated_box_identity_declaration->constructor_count != 1 ||
		!generated_box_identity_fields ||
		generated_box_identity_fields->depth != 3 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 171;
	}
	struct prototype_hott_action_request dependent_box_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = dependent_box_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t dependent_box_identity_request_id;
	uint32_t dependent_box_identity_result_id;
	int dependent_box_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			dependent_box_identity_computation,
			&dependent_box_identity_request_id
		);
	int dependent_box_execution_status = dependent_box_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			dependent_box_identity_request_id,
			&dependent_box_identity_result_id
		) : -1;
	if (dependent_box_request_status != 0 || dependent_box_execution_status != 0 ||
		action_results[
			dependent_box_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			dependent_box_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		fprintf(
			stderr,
			"dependent Box identity failed request_status=%d execute_status=%d "
			"request=%u result=%u state=%d residual=%d certificate=%u count=%zu\n",
			dependent_box_request_status,
			dependent_box_execution_status,
			dependent_box_request_status == 0 ? dependent_box_identity_request_id :
				PROTOTYPE_INVALID_ID,
			dependent_box_execution_status == 0 ? dependent_box_identity_result_id :
				PROTOTYPE_INVALID_ID,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].outcome.state : -1,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].outcome.residual_reason : -1,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].certificate_id : PROTOTYPE_INVALID_ID,
			action_db.certificate_count
		);
		return 309;
	}
	const struct prototype_hott_action_certificate* dependent_box_identity =
		&action_db.certificates[action_results[
			dependent_box_identity_result_id
		].certificate_id];
	const struct prototype_type_declaration* generated_dependent_box_identity =
		dependent_box_identity->data.identity_type.generated_type_declaration_id <
			type_db.type_count ? &type_db.type_declarations[
				dependent_box_identity->data.identity_type.
					generated_type_declaration_id
			] : NULL;
	const struct prototype_type_constructor_declaration*
		generated_dependent_box_constructor = generated_dependent_box_identity &&
		generated_dependent_box_identity->first_constructor <
			type_db.constructor_count ? &type_db.constructor_declarations[
				generated_dependent_box_identity->first_constructor
			] : NULL;
	const struct prototype_context* generated_dependent_box_fields =
		generated_dependent_box_constructor ? prototype_context_get(
			&context_db, generated_dependent_box_constructor->field_context
		) : NULL;
	uint32_t dependent_field_bridge = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < bridge_db.bridge_count; ++i) {
		if (bridge_db.bridges[i].source_context_id ==
				dependent_box_constructor->field_context &&
			bridge_db.certificates[i].semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) {
			dependent_field_bridge = i;
			break;
		}
	}
	const struct prototype_hott_bridge_certificate* dependent_bridge_certificate =
		dependent_field_bridge < bridge_db.certificate_count ?
		&bridge_db.certificates[dependent_field_bridge] : NULL;
	const struct prototype_hott_action_certificate* dependent_fiber_certificate =
		dependent_bridge_certificate && dependent_bridge_certificate->
			fiber_action_certificate_id < action_db.certificate_count ?
		&action_db.certificates[
			dependent_bridge_certificate->fiber_action_certificate_id
		] : NULL;
	if (!generated_dependent_box_identity ||
		!generated_dependent_box_constructor ||
		!generated_dependent_box_fields ||
		generated_dependent_box_identity->constructor_count != 1 ||
		generated_dependent_box_fields->depth != 6 ||
		!dependent_fiber_certificate || dependent_fiber_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		dependent_fiber_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 310;
	}
	uint32_t dependent_box_false;
	uint32_t dependent_box_false_claim;
	uint32_t dependent_box_family_claim;
	uint32_t dependent_box_witness;
	uint32_t dependent_box_witness_claim;
	if (prototype_term_constructor(
			&term_db, dependent_box_view, 0, &dependent_box_false
		) != 0 || prototype_term_app(
			&term_db, dependent_box_false, bool_false, &dependent_box_false
		) != 0 || prototype_term_app(
			&term_db, dependent_box_false, bool_false, &dependent_box_false
		) != 0 || prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			dependent_box_false,
			dependent_box_view,
			(uint32_t[]) {
				bool_false_constructor_claim,
				bool_false_constructor_claim
			},
			2,
			&dependent_box_false_claim
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			dependent_box_identity_result_id,
			dependent_box_false_claim,
			&dependent_box_family_claim,
			&dependent_box_witness,
			&dependent_box_witness_claim
		) != 0) {
		return 311;
	}
	uint32_t dependent_witness_head;
	uint32_t dependent_witness_owner;
	uint32_t dependent_witness_ordinal;
	uint32_t dependent_witness_arguments[6];
	uint32_t dependent_witness_argument_count;
	const struct prototype_judgement_proposition* dependent_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_family_claim
		);
	const struct prototype_judgement_proposition* dependent_witness_claim =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_witness_claim
		);
	if (!dependent_identity_family || !dependent_witness_claim ||
		dependent_witness_claim->classifier != dependent_identity_family->subject ||
		prototype_term_constructor_spine_info(
			&term_db,
			dependent_box_witness,
			&dependent_witness_head,
			&dependent_witness_owner,
			&dependent_witness_ordinal,
			dependent_witness_arguments,
			6,
			&dependent_witness_argument_count
		) != 0 || dependent_witness_ordinal != 0 ||
		dependent_witness_argument_count != 6 ||
		dependent_witness_arguments[0] != bool_false ||
		dependent_witness_arguments[1] != bool_false ||
		dependent_witness_arguments[2] != false_identity_witness ||
		dependent_witness_arguments[3] != bool_false ||
		dependent_witness_arguments[4] != bool_false ||
		dependent_witness_arguments[5] != false_identity_witness) {
		return 312;
	}
	uint32_t box_false;
	uint32_t box_false_right;
	uint32_t box_identity_witness;
	uint32_t box_identity_type_arguments[2];
	uint32_t box_identity_type;
	if (prototype_term_constructor(
			&term_db, box_view, 0, &box_false
		) != 0 || prototype_term_app(
			&term_db, box_false, bool_false, &box_false
		) != 0 || prototype_term_constructor(
			&term_db, box_view, 0, &box_false_right
		) != 0 || prototype_term_app(
			&term_db, box_false_right, bool_false, &box_false_right
		) != 0 || prototype_term_constructor(
			&term_db,
			box_identity_certificate->data.identity_type.backing_type_former_term_id,
			0,
			&box_identity_witness
		) != 0 || prototype_term_app(
			&term_db, box_identity_witness, bool_false, &box_identity_witness
		) != 0 || prototype_term_app(
			&term_db, box_identity_witness, bool_false, &box_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			box_identity_witness,
			false_identity_witness,
			&box_identity_witness
		) != 0) {
		return 174;
	}
	box_identity_type_arguments[0] = box_false;
	box_identity_type_arguments[1] = box_false_right;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_box_identity_type,
			box_identity_type_arguments,
			2,
			&box_identity_type
		) != 0) {
		return 174;
	}
	uint32_t box_false_claim;
	uint32_t box_degeneracy_family_claim;
	uint32_t box_degeneracy_witness;
	uint32_t box_degeneracy_witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			box_false,
			box_view,
			(uint32_t[]) { bool_false_constructor_claim },
			1,
			&box_false_claim
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_result_id,
			box_false_claim,
			&box_degeneracy_family_claim,
			&box_degeneracy_witness,
			&box_degeneracy_witness_claim
		) != 0 || box_degeneracy_witness != box_identity_witness ||
		prototype_judgement_claim_proposition(
			&judgement, box_degeneracy_family_claim
		)->subject != box_identity_type || prototype_judgement_claim_proposition(
			&judgement, box_degeneracy_witness_claim
		)->classifier != box_identity_type) {
		return 221;
	}
	uint32_t box_endpoint_path[2];
	uint32_t box_endpoint_count;
	uint32_t first_box_endpoint_bridge;
	uint32_t first_box_endpoint_to_empty;
	uint32_t box_is_type_in_first_endpoint;
	if (prototype_context_extension_path(
			&context_db,
			empty,
			box_identity_certificate->data.identity_type.endpoint_context_id,
			box_endpoint_path,
			2,
			&box_endpoint_count
		) != 0 || box_endpoint_count != 2 ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			box_endpoint_path[0],
			box_identity_computation_id,
			&first_box_endpoint_bridge
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			box_endpoint_path[0],
			empty,
			&first_box_endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			box_is_type,
			first_box_endpoint_to_empty,
			&box_is_type_in_first_endpoint
		) != 0) {
		return 247;
	}
	struct prototype_hott_action_request constant_box_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_is_type_in_first_endpoint,
			.source_bridge_id = first_box_endpoint_bridge
		}
	};
	uint32_t constant_box_identity_id;
	uint32_t constant_box_identity_result_id;
	uint32_t full_box_endpoint_bridge;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_box_identity,
			&constant_box_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_box_identity_id,
			&constant_box_identity_result_id
		) != 0 || action_results[constant_box_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			box_endpoint_path[1],
			constant_box_identity_id,
			&full_box_endpoint_bridge
		) != 0) {
		return 248;
	}
	struct prototype_hott_action_request box_square_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			.source_bridge_id = full_box_endpoint_bridge
		}
	};
	uint32_t box_square_identity_id;
	uint32_t box_square_identity_result_id;
	int box_square_intern_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			box_square_identity,
			&box_square_identity_id
		);
	int box_square_execute_status = box_square_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_square_identity_id,
			&box_square_identity_result_id
		) : -1;
	if (box_square_intern_status != 0 || box_square_execute_status != 0 ||
		action_results[box_square_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			box_square_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		return 249;
	}
	const struct prototype_hott_action_certificate* box_square_certificate =
		&action_certificates[action_results[
			box_square_identity_result_id
		].certificate_id];
	uint32_t generated_box_square_type = box_square_certificate->data.
		identity_type.generated_type_declaration_id;
	const struct prototype_type_declaration* generated_box_square =
		&type_db.type_declarations[generated_box_square_type];
	const struct prototype_type_constructor_declaration* box_square_constructor =
		&type_db.constructor_declarations[generated_box_square->first_constructor];
	uint32_t box_square_fields[64];
	uint32_t box_square_field_count;
	if (box_square_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT ||
		generated_box_square->index_count != 8 ||
		generated_box_square->constructor_count != 1 ||
		prototype_context_extension_path(
			&context_db,
			box_square_constructor->parameter_context,
			box_square_constructor->field_context,
			box_square_fields,
			64,
			&box_square_field_count
		) != 0 || box_square_field_count != 9) {
		return 250;
	}
	uint32_t box_square_boundary[8] = {
		box_false,
		box_false,
		box_identity_witness,
		box_false,
		box_false,
		box_identity_witness,
		box_identity_witness,
		box_identity_witness
	};
	uint32_t box_square_type;
	uint32_t box_square_witness;
	uint32_t box_square_witness_claim;
	uint32_t box_square_field_claims[9] = {
		bool_false_constructor_claim,
		bool_false_constructor_claim,
		false_identity_witness_claim,
		bool_false_constructor_claim,
		bool_false_constructor_claim,
		false_identity_witness_claim,
		false_identity_witness_claim,
		false_identity_witness_claim,
		false_square_claim
	};
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_box_square_type,
			box_square_boundary,
			8,
			&box_square_type
		) != 0 || prototype_term_constructor(
			&term_db,
			box_square_certificate->data.identity_type.backing_type_former_term_id,
			0,
			&box_square_witness
		) != 0) {
		return 251;
	}
	uint32_t box_square_field_terms[9] = {
		bool_false,
		bool_false,
		false_identity_witness,
		bool_false,
		bool_false,
		false_identity_witness,
		false_identity_witness,
		false_identity_witness,
		false_square_witness
	};
	for (uint32_t i = 0; i < 9; ++i) {
		if (prototype_term_app(
				&term_db,
				box_square_witness,
				box_square_field_terms[i],
				&box_square_witness
			) != 0) {
			return 252;
		}
	}
	int box_square_claim_status = prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			box_square_witness,
			box_square_type,
			box_square_field_claims,
			9,
			&box_square_witness_claim
		);
	int box_square_validation_status = prototype_hott_action_db_validate(
		&action_db, &kernel_view, &bridge_db
	);
	if (box_square_claim_status != 0 || box_square_validation_status != 0) {
		return 253;
	}
	uint32_t box_identity_root_id;
	uint32_t concrete_box_identity_root_id;
	size_t box_identity_root_mark = identity_interface.identity_root_count;
	int box_root_status = prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			box_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&box_identity_root_id
		);
	int concrete_box_root_status = box_root_status == 0 ?
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			box_is_type,
			box_degeneracy_family_claim,
			box_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_box_identity_root_id
		) : -1;
	if (box_root_status != 0 || box_identity_root_id != box_identity_root_mark ||
		concrete_box_root_status != 0 || concrete_box_identity_root_id !=
			box_identity_root_mark + 1) {
		fprintf(
			stderr,
			"Box identity root failed generated_status=%d generated_id=%u "
			"concrete_status=%d concrete_id=%u root_count=%zu\n",
			box_root_status,
			box_identity_root_id,
			concrete_box_root_status,
			concrete_box_identity_root_id,
			identity_interface.identity_root_count
		);
		return 243;
	}
	struct prototype_judgement_proposition box_delta_propositions[8];
	struct prototype_judgement_derivation_candidate box_delta_candidates[8];
	struct prototype_judgement_candidate_premise box_delta_premises[
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result box_delta_motives[1];
	struct prototype_judgement_computation_constraint box_delta_constraints[1];
	struct prototype_judgement_effect_row_constraint box_delta_effect_rows[1];
	struct prototype_judgement_delta box_delta;
	prototype_judgement_delta_init(
		&box_delta,
		&judgement,
		box_delta_propositions,
		box_delta_candidates,
		8,
		box_delta_premises,
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		box_delta_motives,
		1,
		box_delta_constraints,
		1,
		box_delta_effect_rows,
		1
	);
	prototype_judgement_delta_set_context_store(
		&box_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_context(&box_delta, empty);
	struct prototype_judgement_selected_evidence box_argument_evidence[3];
	uint32_t box_argument_operations[3] = {
		false_operation, false_operation, PROTOTYPE_INVALID_ID
	};
	if (prototype_judgement_delta_select_evidence(
			&box_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&box_argument_evidence[0]
		) != 0 || prototype_judgement_delta_select_evidence(
			&box_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&box_argument_evidence[1]
		) != 0 || prototype_judgement_delta_select_evidence(
			&box_delta,
			PROTOTYPE_INVALID_ID,
			empty,
			false_identity_witness,
			false_identity_type,
			&box_argument_evidence[2]
		) != 0 || prototype_judgement_delta_record_constructor_spine(
			&box_delta,
			&term_db,
			&type_db,
			box_identity_witness,
			box_identity_type,
			box_argument_operations,
			box_argument_evidence,
			3
		) != 0 || box_delta.proposition_count != 1 ||
		box_delta.derivation_candidate_count != 1 ||
		box_delta.derivation_candidates[0].proof_kind !=
			PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION ||
		box_delta.propositions[0].classifier != box_identity_type) {
		return 175;
	}
	uint32_t indexed_left_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_right_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_left_context;
	uint32_t indexed_right_context;
	uint32_t indexed_left_var;
	uint32_t indexed_right_var;
	uint32_t indexed_relation_args[2];
	uint32_t indexed_relation_type;
	uint32_t indexed_proof_binding;
	uint32_t indexed_proof_context;
	uint32_t indexed_proof_var;
	if (indexed_left_binding == PROTOTYPE_INVALID_ID ||
		indexed_right_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			indexed_left_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&indexed_left_context
		) != 0 || prototype_context_extend(
			&context_db,
			indexed_left_context,
			indexed_right_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&indexed_right_context
		) != 0 || prototype_term_var(
			&term_db, indexed_left_binding, &indexed_left_var
		) != 0 || prototype_term_var(
			&term_db, indexed_right_binding, &indexed_right_var
		) != 0) {
		return 142;
	}
	indexed_relation_args[0] = indexed_left_var;
	indexed_relation_args[1] = indexed_right_var;
	indexed_proof_binding = prototype_term_new_binding(&term_db);
	if (indexed_proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			indexed_relation_args,
			2,
			&indexed_relation_type
		) != 0 || prototype_context_extend(
			&context_db,
			indexed_right_context,
			indexed_proof_binding,
			indexed_relation_type,
			PROTOTYPE_INVALID_ID,
			&indexed_proof_context
		) != 0 || prototype_term_var(
			&term_db, indexed_proof_binding, &indexed_proof_var
		) != 0) {
		return 142;
	}
	uint32_t refined_context;
	uint32_t refinement_substitution;
	uint32_t refined_constructor;
	uint32_t refined_left;
	uint32_t refined_right;
	uint32_t refined_proof;
	if (prototype_judgement_indexed_branch_refinement(
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			indexed_proof_context,
			indexed_proof_var,
			indexed_relation_type,
			0,
			indexed_proof_context,
			NULL,
			0,
			&refined_context,
			&refinement_substitution,
			&refined_constructor
		) != 0 || refined_context != empty || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_left_var,
			refinement_substitution,
			&refined_left
		) != 0 || refined_left != bool_false || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_right_var,
			refinement_substitution,
			&refined_right
		) != 0 || refined_right != bool_false || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_proof_var,
			refinement_substitution,
			&refined_proof
		) != 0 || refined_proof != refined_constructor ||
		prototype_substitution_db_validate_typed(
			&substitution_db, &context_db, &term_db, &type_db
		) != 0) {
		return 142;
	}
	struct prototype_judgement_proposition indexed_match_propositions[64];
	struct prototype_judgement_derivation_candidate indexed_match_candidates[64];
	struct prototype_judgement_claim indexed_match_claims[64];
	struct prototype_judgement_derivation indexed_match_derivations[64];
	struct prototype_judgement_candidate_premise indexed_match_candidate_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_premise_edge indexed_match_accepted_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_db indexed_match_judgement;
	prototype_judgement_db_init(
		&indexed_match_judgement,
		indexed_match_propositions,
		indexed_match_candidates,
		indexed_match_claims,
		indexed_match_derivations,
		64,
		indexed_match_candidate_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		indexed_match_accepted_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	uint32_t indexed_match_bool_type_claim;
	uint32_t indexed_match_projection;
	uint32_t indexed_match_bool_type_in_context_claim;
	uint32_t indexed_left_assumption_claim;
	if (prototype_judgement_add_type_formation_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			empty,
			bool_view,
			universe,
			&indexed_match_bool_type_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			indexed_proof_context,
			empty,
			&indexed_match_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_match_bool_type_claim,
			indexed_match_projection,
			&indexed_match_bool_type_in_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&indexed_match_judgement,
			&term_db,
			&context_db,
			indexed_proof_context,
			indexed_left_binding,
			bool_view,
			&indexed_left_assumption_claim
		) != 0) {
		return 143;
	}
	struct prototype_match_case_input indexed_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = PROTOTYPE_INVALID_ID
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t indexed_false_return;
	uint32_t indexed_true_return;
	uint32_t indexed_branch_classifier;
	uint32_t indexed_match_term;
	if (prototype_term_return(
			&term_db, indexed_left_var, &indexed_false_return
		) != 0 || prototype_term_return(
			&term_db, indexed_left_var, &indexed_true_return
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, bool_view, &indexed_branch_classifier
		) != 0) {
		return 143;
	}
	indexed_match_cases[0].body = indexed_false_return;
	indexed_match_cases[1].body = indexed_true_return;
	if (prototype_term_match(
			&term_db,
			indexed_proof_var,
			indexed_match_cases,
			2,
			&indexed_match_term
		) != 0) {
		return 143;
	}
	struct prototype_operation_node indexed_match_operations[6];
	struct prototype_operation_match_case indexed_match_operation_cases[2];
	struct prototype_operation_graph indexed_match_operation_graph;
	memset(&indexed_match_operation_graph, 0, sizeof(indexed_match_operation_graph));
	indexed_match_operation_graph.operations = indexed_match_operations;
	indexed_match_operation_graph.operation_capacity = 6;
	indexed_match_operation_graph.cases = indexed_match_operation_cases;
	indexed_match_operation_graph.case_capacity = 2;
	uint32_t indexed_scrutinee_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_proof_var,
		indexed_relation_type,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t indexed_false_body_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_left_var,
		bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t indexed_false_return_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_false_return,
		indexed_branch_classifier,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t indexed_true_body_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_left_var,
		bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t indexed_true_return_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_true_return,
		indexed_branch_classifier,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	uint32_t indexed_match_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_match_term,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION
	);
	indexed_match_operations[indexed_scrutinee_operation].tag =
		PROTOTYPE_OPERATION_VAR;
	indexed_match_operations[indexed_false_body_operation].tag =
		PROTOTYPE_OPERATION_VAR;
	indexed_match_operations[indexed_true_body_operation].tag =
		PROTOTYPE_OPERATION_VAR;
	indexed_match_operations[indexed_false_return_operation].tag =
		PROTOTYPE_OPERATION_RETURN;
	indexed_match_operations[indexed_false_return_operation].argument =
		indexed_false_body_operation;
	indexed_match_operations[indexed_true_return_operation].tag =
		PROTOTYPE_OPERATION_RETURN;
	indexed_match_operations[indexed_true_return_operation].argument =
		indexed_true_body_operation;
	indexed_match_operations[indexed_match_operation].tag =
		PROTOTYPE_OPERATION_MATCH;
	indexed_match_operations[indexed_match_operation].scrutinee =
		indexed_scrutinee_operation;
	indexed_match_operations[indexed_match_operation].first_case = 0;
	indexed_match_operations[indexed_match_operation].case_count = 2;
	indexed_match_operation_cases[0] =
		(struct prototype_operation_match_case) {
			.body_operation = indexed_false_return_operation,
			.context_id = indexed_proof_context,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 0,
			.case_label_symbol_id = -1,
			.binder_count = 0
		};
	indexed_match_operation_cases[1] =
		(struct prototype_operation_match_case) {
			.body_operation = indexed_true_return_operation,
			.context_id = indexed_proof_context,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 1,
			.case_label_symbol_id = -1,
			.binder_count = 0
		};
	indexed_match_operation_graph.case_count = 2;
	struct prototype_judgement_proposition indexed_delta_propositions[64];
	struct prototype_judgement_derivation_candidate indexed_delta_candidates[64];
	struct prototype_judgement_candidate_premise indexed_delta_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result indexed_delta_motives[64];
	struct prototype_judgement_computation_constraint indexed_delta_constraints[64];
	struct prototype_judgement_effect_row_constraint indexed_delta_effect_rows[64];
	struct prototype_judgement_delta indexed_delta;
	prototype_judgement_delta_init(
		&indexed_delta,
		&indexed_match_judgement,
		indexed_delta_propositions,
		indexed_delta_candidates,
		64,
		indexed_delta_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		indexed_delta_motives,
		64,
		indexed_delta_constraints,
		64,
		indexed_delta_effect_rows,
		64
	);
	prototype_judgement_delta_set_context_store(
		&indexed_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_operation_store(
		&indexed_delta,
		indexed_match_operation_graph.operations,
		indexed_match_operation_graph.operation_count,
		indexed_match_operation_graph.cases,
		indexed_match_operation_graph.case_count
	);
	prototype_judgement_delta_set_context(
		&indexed_delta, indexed_proof_context
	);
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_scrutinee_operation
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_proof_binding,
			indexed_relation_type
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_false_body_operation
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_left_binding,
			bool_view
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_true_body_operation
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_left_binding,
			bool_view
		) != 0) {
		return 143;
	}
	struct prototype_judgement_selected_evidence indexed_branch_evidence;
	if (prototype_judgement_selected_evidence_from_claim(
			&indexed_match_judgement,
			indexed_left_assumption_claim,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_false_return_operation
	);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_false_return,
			indexed_false_body_operation,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_true_return_operation
	);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_true_return,
			indexed_true_body_operation,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_operation(
		&indexed_delta, indexed_match_operation
	);
	uint32_t indexed_match_classifier;
	if (prototype_judgement_delta_type_match_from_cases(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_match_term,
			term_db.terms[universe].as.universe_var.level_var,
			&indexed_match_classifier
		) != 0) {
		return 143;
	}
	indexed_match_operations[indexed_match_operation].known_classifier =
		indexed_match_classifier;
	indexed_match_operations[indexed_match_operation].classifier =
		indexed_match_classifier;
	if (prototype_judgement_delta_commit(&indexed_delta, 0) != 0 ||
		prototype_judgement_publish_candidates(
			&indexed_match_operation_graph, &indexed_match_judgement
		) != 0 || prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) != 0) {
		return 143;
	}
	struct prototype_judgement_derivation* indexed_match_derivation = NULL;
	for (uint32_t i = 0; i < indexed_match_judgement.derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&indexed_match_judgement.derivations[i];
		const struct prototype_judgement_proposition* conclusion =
			derivation->conclusion_claim_id < indexed_match_judgement.claim_count ?
			prototype_judgement_claim_proposition(
				&indexed_match_judgement, derivation->conclusion_claim_id
			) : NULL;
		if (conclusion && derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM &&
			conclusion->operation_id == indexed_match_operation) {
			indexed_match_derivation = derivation;
			break;
		}
	}
	if (!indexed_match_derivation || indexed_match_derivation->premise_count != 3 ||
		indexed_match_derivation->premises[1].semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		indexed_match_derivation->premises[2].semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		indexed_match_derivation->premises[1].semantic_action_id ==
			indexed_match_derivation->premises[2].semantic_action_id) {
		return 144;
	}
	uint32_t saved_false_refinement =
		indexed_match_derivation->premises[1].semantic_action_id;
	indexed_match_derivation->premises[1].semantic_action_id =
		indexed_match_derivation->premises[2].semantic_action_id;
	if (prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) == 0) {
		return 144;
	}
	indexed_match_derivation->premises[1].semantic_action_id =
		saved_false_refinement;
	if (prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) != 0) {
		return 144;
	}
	(void)indexed_match_bool_type_in_context_claim;
	struct prototype_hott_action_request bool_term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = false_claim,
			.source_bridge_id = bridge,
			.relation_type_action_request_id = bool_type_action_id
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
		fprintf(
			stderr,
			"bool term action failed request=%u result=%u state=%d reason=%d "
			"validate=%d source_claim=%u rank=%u\n",
			bool_term_action_id,
			bool_term_result_id,
			bool_term_result_id < action_db.result_count ?
				action_results[bool_term_result_id].outcome.state : -1,
			bool_term_result_id < action_db.result_count ?
				action_results[bool_term_result_id].outcome.residual_reason : -1,
			prototype_hott_action_db_validate(
				&action_db, &kernel_view, &bridge_db
			),
			false_claim,
			false_claim < judgement.claim_count ?
				judgement.claims[false_claim].closure_rank : PROTOTYPE_INVALID_ID
		);
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
	if (bool_bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bool_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) {
		return 247;
	}
	int saved_bridge_semantics = bridge_db.certificates[bool_bridge].semantics;
	bridge_db.certificates[bool_bridge].semantics =
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 247;
	}
	bridge_db.certificates[bool_bridge].semantics = saved_bridge_semantics;
	if (prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 247;
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
	uint32_t bool_identity_bridge;
	uint32_t repeated_bool_identity_bridge;
	size_t identity_bridge_count_before = bridge_db.bridge_count;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_context,
			bool_identity_computation_id,
			&bool_identity_bridge
		) != 0 || bool_identity_bridge == bool_bridge ||
		bridge_db.bridge_count != identity_bridge_count_before + 1 ||
		bool_identity_bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bool_identity_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0 || prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_context,
			bool_identity_computation_id,
			&repeated_bool_identity_bridge
		) != 0 || repeated_bool_identity_bridge != bool_identity_bridge ||
		bridge_db.bridge_count != identity_bridge_count_before + 1) {
		return 248;
	}
	uint32_t saved_identity_fiber_certificate =
		bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id;
	bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id =
		bridge_db.certificates[bool_bridge].fiber_action_certificate_id;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 249;
	}
	bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id =
		saved_identity_fiber_certificate;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 250;
	}
	uint32_t bool_type_result_id;
	if (prototype_hott_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id,
			&bool_type_result_id
		) != 0 || bool_type_result_id != result_id) {
		return 45;
	}
	uint32_t type_certificate_id = action_results[result_id].certificate_id;
	if (action_certificates[type_certificate_id].data.relation_type.
			relation_family_semantics !=
		PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION) {
		return 142;
	}
	action_certificates[type_certificate_id].data.relation_type.
		relation_family_semantics = PROTOTYPE_HOTT_RELATION_FAMILY_INVALID;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 143;
	}
	action_certificates[type_certificate_id].data.relation_type.
		relation_family_semantics =
			PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION;
	uint32_t saved_relation_type =
		action_certificates[type_certificate_id].data.relation_type.relation_type_term_id;
	action_certificates[type_certificate_id].data.relation_type.relation_type_term_id = text;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 46;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_type_term_id =
		saved_relation_type;
	uint32_t relation_family = action_certificates[
		type_certificate_id
	].data.relation_type.relation_family_term_id;
	uint32_t family_left_binding;
	uint32_t inner_relation_family;
	uint32_t family_right_binding;
	uint32_t family_relation_body;
	int family_body_matches;
	if (prototype_term_pure_family_parts(
			&term_db,
			relation_family,
			&family_left_binding,
			&inner_relation_family
		) != 0 || prototype_term_pure_family_parts(
			&term_db,
			inner_relation_family,
			&family_right_binding,
			&family_relation_body
		) != 0 || prototype_term_core_shape_equal_under_binders(
			&term_db,
			(uint32_t[]) { family_left_binding, family_right_binding },
			(uint32_t[]) {
				action_certificates[type_certificate_id].data.relation_type.
					left_endpoint_binding_id,
				action_certificates[type_certificate_id].data.relation_type.
					right_endpoint_binding_id
			},
			2,
			family_relation_body,
			saved_relation_type,
			&family_body_matches
		) != 0 || !family_body_matches) {
		return 140;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_family_term_id =
		text;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 141;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_family_term_id =
		relation_family;
	if (prototype_hott_execute_relation_type_action(
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t contextual_bool_type_action_id;
	uint32_t contextual_bool_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, contextual_bool_type_action,
			&contextual_bool_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = contextual_bool_type_action_id
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
	uint32_t neutral_bool_identity_arguments[2] = {
		bool_variable,
		bool_variable
	};
	uint32_t neutral_bool_identity_family;
	uint32_t neutral_bool_identity_family_claim;
	uint32_t neutral_bool_identity_witness;
	uint32_t neutral_bool_identity_witness_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			neutral_bool_identity_arguments,
			2,
			&neutral_bool_identity_family
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			bool_variable_claim,
			&neutral_bool_identity_family_claim,
			&neutral_bool_identity_witness,
			&neutral_bool_identity_witness_claim
		) != 0 || term_db.terms[neutral_bool_identity_witness].tag !=
			PROTOTYPE_TERM_MATCH || prototype_judgement_claim_proposition(
			&judgement, neutral_bool_identity_family_claim
		)->subject != neutral_bool_identity_family ||
		prototype_judgement_claim_proposition(
			&judgement, neutral_bool_identity_witness_claim
		)->classifier != neutral_bool_identity_family) {
		return 223;
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
	boxed_derivation->premises[0].scoped_proposition_id = PROTOTYPE_INVALID_ID;
	uint32_t boxed_identity_family_claim;
	uint32_t boxed_identity_witness;
	uint32_t boxed_identity_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_result_id,
			boxed_claim,
			&boxed_identity_family_claim,
			&boxed_identity_witness,
			&boxed_identity_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, boxed_identity_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, boxed_identity_family_claim
		)->subject) {
		return 222;
	}
	struct prototype_hott_action_request box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = box_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t box_type_action_id;
	uint32_t box_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, box_type_action, &box_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = box_type_action_id
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS &&
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
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = boxed_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
		};
	struct prototype_hott_action_request pure_box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
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
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = pure_box_type_action_id
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS &&
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
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = returned_box_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
		};
	struct prototype_hott_action_request pure_box_thunk_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pure_box_thunk_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pure_box_thunk_type_action_id;
	uint32_t pure_box_thunk_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pure_box_thunk_type_action,
			&pure_box_thunk_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = pure_box_thunk_type_action_id
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS &&
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pi_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pi_type_action_id;
	uint32_t pi_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pi_type_action, &pi_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
				{ .claim_id = function_variable_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_variable_in_function_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pure_comp_in_function_context_claim,
			.source_bridge_id = function_bridge
		}
	};
	uint32_t app_result_type_action_id;
	uint32_t app_result_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, app_result_type_action,
			&app_result_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = app_result_type_action_id
		}
	};
	uint32_t app_action_id;
	uint32_t app_result_id;
	int app_action_intern_status = prototype_hott_action_request_intern(
		&action_db, &kernel_view, &bridge_db, app_action, &app_action_id
	);
	int app_action_status = app_action_intern_status == 0 ?
		prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, app_action_id, &app_result_id
		) : -1;
	if (app_action_intern_status != 0 || app_action_status != 0 ||
		action_results[app_result_id].outcome.state !=
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS &&
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = lambda_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t lambda_type_action_id;
	uint32_t lambda_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, lambda_type_action,
			&lambda_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = outer_bool_in_lambda_body_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
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
				{ .claim_id = lambda_binder_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = returned_outer_bool_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request lambda_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = lambda_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = lambda_type_action_id
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS &&
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
				{ .claim_id = value_match_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_true_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_false_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request value_match_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = value_match_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t value_match_type_action_id;
	uint32_t value_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, value_match_type_action,
			&value_match_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = value_match_type_action_id
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
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS &&
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
	/* Object action must reconstruct Match through ordinary kernel rules. */
	uint32_t object_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t object_motive_context;
	uint32_t object_motive_context_certificate;
	uint32_t object_motive_projection;
	uint32_t bool_has_type_in_context_claim;
	uint32_t bool_has_type_in_motive_context_claim;
	uint32_t object_motive_binder_claim;
	uint32_t object_motive;
	uint32_t object_motive_classifier;
	uint32_t object_motive_claim;
	uint32_t object_match_classifier;
	uint32_t object_match_classifier_claim;
	uint32_t object_match_classifier_is_type_claim;
	uint32_t object_match;
	uint32_t object_match_claim;
	uint32_t converted_object_match_claim;
	uint32_t object_match_branch_claims[2] = {
		bool_true_in_context_claim,
		bool_false_in_context_claim
	};
	if (object_motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			bool_projection,
			&bool_has_type_in_context_claim
		) != 0 || prototype_context_extend(
			&context_db,
			bool_context,
			object_motive_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&object_motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			object_motive_context,
			bool_type_in_context_claim,
			&object_motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			object_motive_context,
			bool_context,
			&object_motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type_in_context_claim,
			object_motive_projection,
			&bool_has_type_in_motive_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			object_motive_context,
			object_motive_binding,
			bool_view,
			&object_motive_binder_claim
		) != 0 || prototype_term_lambda(
			&term_db, object_motive_binding, bool_view, &object_motive
		) != 0 || prototype_term_pi(
			&term_db, bool_view, universe, &object_motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_context,
			object_motive,
			object_motive_classifier,
			object_motive_binder_claim,
			bool_has_type_in_motive_context_claim,
			&object_motive_claim
		) != 0 || prototype_term_app(
			&term_db,
			object_motive,
			bool_variable,
			&object_match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_context,
			object_match_classifier,
			universe,
			object_motive_claim,
			bool_variable_claim,
			&object_match_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			bool_context,
			object_match_classifier,
			universe,
			object_match_classifier_claim,
			&object_match_classifier_is_type_claim
		) != 0 || prototype_term_match(
			&term_db, bool_variable, value_match_cases, 2, &object_match
		) != 0 || prototype_judgement_add_match_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&operation_graph,
			bool_context,
			object_match,
			object_match_classifier,
			object_match_classifier_claim,
			object_match_branch_claims,
			2,
			&object_match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_context,
			object_match,
			bool_view,
			object_match_claim,
			&converted_object_match_claim
		) != 0) {
		return 279;
	}
	struct prototype_hott_action_request contextual_bool_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_type_in_context_claim,
			.source_bridge_id = bool_identity_bridge
		}
	};
	uint32_t contextual_bool_identity_id;
	uint32_t contextual_bool_identity_result_id;
	struct prototype_hott_action_request object_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = converted_object_match_claim,
			.source_bridge_id = bool_identity_bridge,
			.identity_type_action_request_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t object_match_action_id;
	uint32_t object_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			contextual_bool_identity,
			&contextual_bool_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			contextual_bool_identity_id,
			&contextual_bool_identity_result_id
		) != 0 || action_results[
			contextual_bool_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 280;
	}
	object_match_action.key.object_term.identity_type_action_request_id =
		contextual_bool_identity_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			object_match_action,
			&object_match_action_id
		) != 0 || prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			object_match_action_id,
			&object_match_result_id
		) != 0 || action_results[object_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			object_match_result_id
		].certificate_id >= action_db.certificate_count) {
		return 281;
	}
	const struct prototype_hott_object_term_action_certificate*
		object_match_certificate = &action_certificates[
			action_results[object_match_result_id].certificate_id
		].data.object_term;
	const struct prototype_judgement_proposition* object_match_witness =
		prototype_judgement_claim_proposition(
			&judgement, object_match_certificate->witness_has_type_claim_id
		);
	int found_object_match_elim = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				object_match_certificate->witness_has_type_claim_id &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
			uint32_t premise = judgement.derivations[i].premises[0].claim_id;
			for (uint32_t j = 0; j < judgement.derivation_count; ++j) {
				if (judgement.derivations[j].conclusion_claim_id == premise &&
					judgement.derivations[j].proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM) {
					found_object_match_elim = 1;
				}
			}
		}
	}
	if (!object_match_witness || object_match_witness->subject >=
			term_db.term_count || term_db.terms[object_match_witness->subject].tag !=
			PROTOTYPE_TERM_MATCH || !found_object_match_elim ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 282;
	}
	(void)object_motive_context_certificate;
	(void)object_match_classifier_is_type_claim;
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
				{ .claim_id = unbox_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = unbox_body_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request unbox_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = unbox_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t unbox_type_action_id;
	uint32_t unbox_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, unbox_type_action,
			&unbox_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = unbox_type_action_id
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
	struct prototype_hott_action_request nat_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = nat_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t nat_identity_computation_id;
	uint32_t nat_identity_computation_result_id;
	int nat_intern_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			nat_identity_computation,
			&nat_identity_computation_id
		);
	int nat_execute_status = nat_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_id,
			&nat_identity_computation_result_id
		) : -1;
	if (nat_intern_status != 0 || nat_execute_status != 0 ||
		action_results[nat_identity_computation_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[nat_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 172;
	}
	const struct prototype_hott_action_certificate* nat_identity_certificate =
		&action_db.certificates[
			action_results[nat_identity_computation_result_id].certificate_id
		];
	const struct prototype_type_declaration* generated_nat_identity_declaration =
		&type_db.type_declarations[
			nat_identity_certificate->data.identity_type.
				generated_type_declaration_id
		];
	const struct prototype_type_constructor_declaration*
		generated_nat_successor_identity = &type_db.constructor_declarations[
			generated_nat_identity_declaration->first_constructor + 1
		];
	const struct prototype_context* generated_nat_successor_fields =
		prototype_context_get(
			&context_db, generated_nat_successor_identity->field_context
		);
	if (generated_nat_identity_declaration->constructor_count != 2 ||
		!generated_nat_successor_fields ||
		generated_nat_successor_fields->depth != 3 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 173;
	}
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
	uint32_t nat_succ_zero;
	uint32_t nat_succ_succ_zero;
	if (prototype_term_app(
			&term_db, nat_succ, nat_zero, &nat_succ_zero
		) != 0 || prototype_term_app(
			&term_db, nat_succ, nat_succ_zero, &nat_succ_succ_zero
		) != 0) {
		return 145;
	}
	uint32_t nat_zero_claim;
	uint32_t nat_succ_zero_claim;
	uint32_t nat_succ_succ_zero_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			nat_zero,
			nat_view,
			&nat_zero_claim
		) != 0 || prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			nat_succ_zero,
			nat_view,
			(uint32_t[]) { nat_zero_claim },
			1,
			&nat_succ_zero_claim
		) != 0 || prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			nat_succ_succ_zero,
			nat_view,
			(uint32_t[]) { nat_succ_zero_claim },
			1,
			&nat_succ_succ_zero_claim
		) != 0) {
		return 242;
	}
	uint32_t nat_degeneracy_family_claim;
	uint32_t nat_degeneracy_witness;
	uint32_t nat_degeneracy_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_result_id,
			nat_succ_succ_zero_claim,
			&nat_degeneracy_family_claim,
			&nat_degeneracy_witness,
			&nat_degeneracy_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, nat_degeneracy_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, nat_degeneracy_family_claim
		)->subject || nat_degeneracy_witness >= term_db.term_count ||
		term_db.terms[nat_degeneracy_witness].tag != PROTOTYPE_TERM_APP) {
		return 223;
	}
	uint32_t nat_identity_root_id;
	uint32_t concrete_nat_identity_root_id;
	uint32_t higher_identity_root_id;
	uint32_t concrete_higher_identity_root_id;
	uint32_t neutral_declaration_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
		neutral,
		empty,
		PROTOTYPE_INVALID_ID,
		neutral,
		bool_view
	);
	if (neutral_declaration_claim == PROTOTYPE_INVALID_ID ||
		judgement.derivation_count >= judgement.derivation_capacity) {
		return 270;
	}
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation) {
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_DECLARATION,
			.conclusion_claim_id = neutral_declaration_claim,
			.closure_rank = 0,
			.rule_data.words = {
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			},
			.semantic_action_kind =
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID,
			.semantic_action_id = PROTOTYPE_INVALID_ID,
			.hash_next = PROTOTYPE_INVALID_ID
		};
	uint32_t neutral_identity_family_claim;
	uint32_t neutral_identity_witness;
	uint32_t neutral_identity_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			neutral_declaration_claim,
			&neutral_identity_family_claim,
			&neutral_identity_witness,
			&neutral_identity_witness_claim
		) != 0) {
		return 270;
	}
	uint32_t neutral_nat_identity_arguments[2] = { nat_variable, nat_variable };
	uint32_t neutral_nat_identity_family;
	uint32_t neutral_nat_identity_family_claim;
	uint32_t neutral_nat_identity_witness;
	uint32_t neutral_nat_identity_witness_claim;
	size_t neutral_nat_derivation_mark = judgement.derivation_count;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			nat_identity_certificate->data.identity_type.generated_type_declaration_id,
			neutral_nat_identity_arguments,
			2,
			&neutral_nat_identity_family
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_result_id,
			nat_variable_claim,
			&neutral_nat_identity_family_claim,
			&neutral_nat_identity_witness,
			&neutral_nat_identity_witness_claim
		) != 0 || neutral_nat_identity_witness >= term_db.term_count ||
		term_db.terms[neutral_nat_identity_witness].tag != PROTOTYPE_TERM_MATCH ||
		prototype_judgement_claim_proposition(
			&judgement, neutral_nat_identity_family_claim
		)->subject != neutral_nat_identity_family || prototype_judgement_claim_proposition(
			&judgement, neutral_nat_identity_witness_claim
		)->classifier != neutral_nat_identity_family) {
		return 224;
	}
	int found_neutral_nat_ih = 0;
	for (size_t i = neutral_nat_derivation_mark;
		i < judgement.derivation_count;
		++i) {
		if (judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			found_neutral_nat_ih = 1;
			break;
		}
	}
	if (!found_neutral_nat_ih) {
		return 225;
	}
	uint32_t neutral_identity_root_id;
	uint32_t composed_identity_root_id;
	uint32_t neutral_nat_identity_root_id;
	uint32_t universe_identity_root_id;
	uint32_t concrete_universe_identity_root_id;
	size_t final_identity_root_mark = identity_interface.identity_root_count;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			nat_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&nat_identity_root_id
		) != 0 || nat_identity_root_id != final_identity_root_mark ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			nat_is_type,
			nat_degeneracy_family_claim,
			nat_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_nat_identity_root_id
		) != 0 || concrete_nat_identity_root_id != final_identity_root_mark + 1 ||
		prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			higher_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&higher_identity_root_id
		) != 0 || higher_identity_root_id != final_identity_root_mark + 2 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			false_identity_is_type,
			higher_identity_family_claim,
			higher_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_higher_identity_root_id
		) != 0 || concrete_higher_identity_root_id != final_identity_root_mark + 3 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_is_type,
			neutral_identity_family_claim,
			neutral_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&neutral_identity_root_id
		) != 0 || neutral_identity_root_id != final_identity_root_mark + 4 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_function_is_type,
			composed_identity_family_claim,
			composed_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&composed_identity_root_id
		) != 0 || composed_identity_root_id != final_identity_root_mark + 5 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			nat_is_type,
			neutral_nat_identity_family_claim,
			neutral_nat_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&neutral_nat_identity_root_id
		) != 0 || neutral_nat_identity_root_id != final_identity_root_mark + 6) {
		return 244;
	}
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&universe_identity_root_id
		) == 0 || identity_interface.identity_root_count != final_identity_root_mark + 7) {
		return 271;
	}
	if (prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			universe_is_type,
			universe_degeneracy_family_claim,
			universe_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE,
			&concrete_universe_identity_root_id
		) == 0 || identity_interface.identity_root_count != final_identity_root_mark + 7) {
		return 272;
	}
	if (getenv("A_PROGRAM_HOTT_REVERSE_DERIVATIONS")) {
		for (size_t i = 0; i < judgement.derivation_count / 2; ++i) {
			size_t j = judgement.derivation_count - i - 1;
			struct prototype_judgement_derivation temporary =
				judgement.derivations[i];
			judgement.derivations[i] = judgement.derivations[j];
			judgement.derivations[j] = temporary;
		}
		if (prototype_judgement_db_rebuild_index(&judgement) != 0) {
			return 273;
		}
	}
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0 || prototype_type_declaration_rebuild_representations(
			&term_db, &type_db, &context_db
		) != 0 || write_identity_root_artifact(
			"/tmp/a-program-hott-identity-root.apo",
			&identity_interface,
			&term_db,
			&type_db,
			&judgement,
			&context_db,
			&substitution_db,
			&operation_graph
		) != 0) {
		return 244;
	}
	(void)neutral_identity_witness;
	uint32_t unused_binding = prototype_term_new_binding(&term_db);
	uint32_t unused_context;
	uint32_t unused_projection;
	uint32_t unused_relation;
	uint32_t unused_bool_projection;
	uint32_t unused_bool_is_type;
	uint32_t unused_identity_request_id;
	uint32_t unused_identity_result_id;
	struct prototype_hott_action_request unused_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_box_endpoint_bridge
		}
	};
	if (unused_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			unused_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&unused_context
		) != 0 || prototype_substitution_projection(
			&substitution_db,
			&context_db,
			unused_context,
			&unused_projection
		) != 0 || prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_false,
			bool_true,
			&unused_relation
		) != 0) {
		return 245;
	}
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			box_endpoint_path[0],
			empty,
			&unused_bool_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			unused_bool_projection,
			&unused_bool_is_type
		) != 0) {
		return 245;
	}
	unused_identity_request.key.identity_type.source_claim_id =
		unused_bool_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			unused_identity_request,
			&unused_identity_request_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			unused_identity_request_id,
			&unused_identity_result_id
		) != 0 || action_results[unused_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || write_identity_root_artifact(
			"/tmp/a-program-hott-identity-root-perturbed.apo",
			&identity_interface,
			&term_db,
			&type_db,
				&judgement,
				&context_db,
				&substitution_db,
				&operation_graph
		) != 0) {
		return 245;
	}
	(void)unused_projection;
	(void)unused_relation;
	uint32_t nat_constructor_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			nat_is_type, nat_is_type,
			nat_succ_zero_claim, nat_succ_succ_zero_claim,
			bridge, &nat_constructor_goal
		) != 0 || prototype_hott_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, nat_constructor_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || count_rule(
			&candidate_db,
			nat_constructor_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 1) {
		return 145;
	}
	uint32_t nat_constructor_candidate = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_db.candidate_count; ++i) {
		if (candidate_db.candidates[i].conclusion_goal_id ==
				nat_constructor_goal &&
			candidate_db.candidates[i].rule ==
				PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR) {
			nat_constructor_candidate = i;
			break;
		}
	}
	if (nat_constructor_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[nat_constructor_candidate].child_edge_count != 1) {
		return 145;
	}
	const struct prototype_hott_child_edge* nat_field_edge =
		&candidate_db.child_edges[
			candidate_db.candidates[nat_constructor_candidate].first_child_edge
		];
	const struct prototype_hott_relation_goal* nat_field_goal =
		prototype_hott_relation_goal_db_get(
			&goal_db, nat_field_edge->child_goal_id
		);
	if (!nat_field_goal || nat_field_edge->role !=
			PROTOTYPE_HOTT_CHILD_ADT_FIELD || nat_field_edge->ordinal != 0 ||
		nat_field_goal->left_claim_id != nat_zero_claim ||
		nat_field_goal->right_claim_id != nat_succ_zero_claim ||
		nat_field_goal->left_carrier_claim_id != nat_is_type ||
		nat_field_goal->right_carrier_claim_id != nat_is_type) {
		return 145;
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
				{ .claim_id = nat_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = nat_zero_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = nat_ih_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request nat_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = nat_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t nat_type_action_id;
	uint32_t nat_type_result_id;
	uint32_t nat_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_type_action, &nat_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = nat_classifier_claim,
			.source_bridge_id = nat_bridge
		}
	};
	uint32_t nat_match_type_action_id;
	uint32_t nat_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_match_type_action,
			&nat_match_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = nat_match_type_action_id
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
			PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS) {
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
	uint32_t bool_relation_type;
	uint32_t bool_relation_type_claim;
	struct prototype_hott_type_former_descriptor relation_descriptor;
	if (prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_variable,
			bool_variable,
			&bool_relation_type
		) != 0 || prototype_judgement_add_relation_type_formation(
			&judgement,
			&term_db,
			bool_context,
			bool_relation_type,
			universe,
			bool_type_in_context_claim,
			bool_type_in_context_claim,
			bool_variable_claim,
			bool_variable_claim,
			&bool_relation_type_claim
		) != 0 || prototype_hott_type_former_descriptor_query(
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_relation_type_claim,
			&relation_descriptor
		) != 0 || !relation_descriptor.admitted ||
		relation_descriptor.relation_type_action_rule !=
			PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_RELATION_HIGHER ||
		relation_descriptor.capabilities.relation_type_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.term_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.ordinary_reindex !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.transport !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.lifting !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.resource_hook !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.artifact !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		(relation_descriptor.child_role_mask &
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
			bool_relation_type,
			PROTOTYPE_INVALID_ID,
			&proof_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			proof_context,
			bool_relation_type_claim,
			&proof_context_certificate
		) != 0) {
		return 68;
	}
	struct prototype_hott_action_request relation_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_relation_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t relation_type_action_id;
	uint32_t relation_type_result_id;
	uint32_t proof_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, relation_type_action,
			&relation_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, relation_type_action_id,
			&relation_type_result_id
		) != 0 || action_results[relation_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			proof_context,
			relation_type_action_id,
			&proof_bridge
		) != 0) {
		return 69;
	}
	uint32_t proof_projection;
	uint32_t relation_type_in_proof_claim;
	uint32_t proof_variable_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, proof_context, &proof_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_relation_type_claim,
			proof_projection,
			&relation_type_in_proof_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			proof_context,
			proof_binding,
			bool_relation_type,
			&proof_variable_claim
		) != 0) {
		return 70;
	}
	struct prototype_hott_action_request higher_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = relation_type_in_proof_claim,
			.source_bridge_id = proof_bridge
		}
	};
	uint32_t higher_type_action_id;
	uint32_t higher_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, higher_type_action,
			&higher_type_action_id
		) != 0 || prototype_hott_execute_relation_type_action(
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
			.relation_type_action_request_id = higher_type_action_id
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
	uint32_t beta_false_operation = add_operation(
		&operation_graph,
		empty,
		beta_false,
		bool_view,
		PROTOTYPE_OPERATION_POLARITY_VALUE
	);
	uint32_t beta_false_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		beta_false_operation,
		empty,
		beta_false_operation,
		beta_false,
		bool_view
	);
	uint32_t bool_conversion_goal;
	if (prototype_hott_relation_goal_db_intern(
			&goal_db,
			&kernel_view,
			&bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type,
			bool_is_type,
			beta_false_claim,
			false_claim,
			bridge,
			&bool_conversion_goal
		) != 0) {
		return 148;
	}
	struct prototype_hott_relation_execution conversion_execution;
	if (prototype_hott_relation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_conversion_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			64,
			&conversion_execution
		) != 0 || conversion_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
		conversion_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID ||
		conversion_execution.term_action_request_id != PROTOTYPE_INVALID_ID ||
		conversion_execution.term_action_result_id != PROTOTYPE_INVALID_ID) {
		return 148;
	}
	const struct prototype_judgement_proposition* conversion_witness =
		prototype_judgement_claim_proposition(
			&judgement, conversion_execution.relation_witness_claim_id
		);
	if (!conversion_witness || conversion_witness->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_term_relation_type_info(
			&term_db,
			conversion_witness->classifier,
			&(uint32_t) { 0 },
			&(uint32_t) { 0 },
			&(uint32_t) { 0 },
			&(uint32_t) { 0 }
		) != 0) {
		return 148;
	}
	struct prototype_hott_relation_execution distinct_execution;
	if (prototype_hott_relation_plan_and_execute(
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
		) != 0 || distinct_execution.relation_type_action_request_id == PROTOTYPE_INVALID_ID ||
		distinct_execution.relation_type_action_result_id == PROTOTYPE_INVALID_ID ||
		action_results[distinct_execution.relation_type_action_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		distinct_execution.term_action_request_id != PROTOTYPE_INVALID_ID ||
		distinct_execution.term_action_result_id != PROTOTYPE_INVALID_ID ||
		distinct_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY ||
		distinct_execution.relation_witness_claim_id != PROTOTYPE_INVALID_ID) {
		return 134;
	}
	struct prototype_hott_relation_execution diagonal_execution;
	if (prototype_hott_relation_plan_and_execute(
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
		) != 0 || diagonal_execution.relation_type_action_request_id !=
			bool_type_action_id || diagonal_execution.relation_type_action_result_id !=
			bool_type_result_id || diagonal_execution.term_action_request_id !=
			bool_term_action_id || diagonal_execution.term_action_result_id !=
			bool_term_result_id || diagonal_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
		diagonal_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID) {
		return 66;
	}
	uint32_t atomic_relation_type;
	uint32_t atomic_relation_type_claim;
	if (prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_false,
			bool_true,
			&atomic_relation_type
		) != 0 || prototype_judgement_add_relation_type_formation(
			&judgement,
			&term_db,
			empty,
			atomic_relation_type,
			universe,
			bool_is_type,
			bool_is_type,
			false_claim,
			true_claim,
			&atomic_relation_type_claim
		) != 0) {
		return 135;
	}
	struct prototype_hott_action_request atomic_failure_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = atomic_relation_type_claim,
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
	int atomic_failure_status = prototype_hott_execute_relation_type_action(
		&action_db, &kernel_builder, &bridge_db, atomic_failure_request_id, &ignored_result_id
	);
	action_db.result_capacity = saved_result_capacity;
	if (atomic_failure_status == 0 || action_db.certificate_count !=
		certificate_count_before_failure || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 135;
	}

	uint32_t generated_bool_identity;
	uint32_t repeated_generated_bool_identity;
	uint32_t generated_box_identity;
	if (prototype_type_declaration_origins_validate(
			&type_db, &term_db
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, bool_view, empty, &generated_bool_identity
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, bool_view, empty, &repeated_generated_bool_identity
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, box_view, empty, &generated_box_identity
		) != 0 || generated_bool_identity !=
			repeated_generated_bool_identity || generated_bool_identity ==
			generated_box_identity || type_db.type_declarations[
			generated_bool_identity
		].name_symbol_id != -1 || type_db.type_declarations[
			generated_bool_identity
		].origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		type_db.type_declarations[
			generated_bool_identity
		].origin_source_carrier_term_id != bool_view ||
		prototype_type_declaration_origins_validate(
			&type_db, &term_db
		) != 0) {
		return 136;
	}
	if (prototype_type_declaration_rebuild_representations(
			&term_db, &type_db, &context_db
		) != 0) {
		return 218;
	}
	if (prototype_judgement_publish_candidates(
			&operation_graph, &judgement
		) != 0) {
		return 222;
	}
	free(type_db.representations);
	return 0;
}
