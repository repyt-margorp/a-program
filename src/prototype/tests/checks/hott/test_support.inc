#include "a_program/frontend/lowering.h"
#include "a_program/artifact/wire_v75.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/graph/occurrence_usage.h"
#include "a_program/identity/types.h"
#include "a_program/identity/telescope_action.h"
#include "a_program/identity/context_bridge.h"
#include "a_program/parametricity/relation_action.h"
#include "a_program/parametricity/action_execution.h"
#include "a_program/identity/action_certificate.h"
#include "a_program/identity/identity_computation.h"
#include "a_program/identity/object_term_action.h"
#include "a_program/parametricity/execution.h"
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
#define OCCURRENCE_EDGE_CAPACITY (OPERATION_CAPACITY * 8)
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
static struct prototype_type_representation type_representations[16];
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
static struct prototype_usage_entry judgement_resource_usage[CLAIM_CAPACITY * 8];
static struct prototype_cwf_certificate cwf_certificates[1024];
static struct prototype_typed_occurrence operations[OPERATION_CAPACITY];
static struct prototype_typed_occurrence_edge
	occurrence_edges[OCCURRENCE_EDGE_CAPACITY];
static struct prototype_typed_occurrence_match_case occurrence_match_cases[128];
static struct prototype_hott_bridge bridges[128];
static struct prototype_hott_bridge_certificate bridge_certificates[128];
static struct prototype_parametricity_relation_goal goals[GOAL_CAPACITY];
static struct prototype_parametricity_candidate candidates[CANDIDATE_CAPACITY];
static struct prototype_parametricity_claim_premise claim_premises[PREMISE_CAPACITY];
static struct prototype_parametricity_child_edge child_edges[PREMISE_CAPACITY];
static struct prototype_parametricity_conversion_premise conversion_premises[PREMISE_CAPACITY];
static struct prototype_parametricity_context_certificate_premise
	context_certificate_premises[PREMISE_CAPACITY];
static struct prototype_parametricity_substitution_certificate_premise
	substitution_certificate_premises[PREMISE_CAPACITY];
static struct prototype_parametricity_work_item work_items[GOAL_CAPACITY];
static struct prototype_parametricity_residual_obligation residuals[32];
static struct prototype_hott_action_request action_requests[256];
static struct prototype_hott_action_certificate action_certificates[256];
static struct prototype_hott_action_result action_results[256];
static const struct prototype_term_db* fixture_term_db;
static const struct prototype_typed_occurrence_graph* fixture_operation_graph;

static void fixture_set_operation_graph(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations
) {
	fixture_term_db = terms;
	fixture_operation_graph = operations;
}

struct fixture_occurrence_usage_provider_context {
	const struct prototype_occurrence_usage_solution* solutions;
	size_t solution_count;
	const struct prototype_usage_entry* entries;
	size_t entry_count;
};

static int fixture_occurrence_usage_provider(
	void* context,
	uint32_t operation_id,
	const struct prototype_usage_entry** p_entries,
	uint32_t* p_count
) {
	struct fixture_occurrence_usage_provider_context* provider = context;
	struct prototype_usage_vector usage;
	if (!provider || !p_entries || !p_count ||
		prototype_occurrence_usage_solution_view(
			provider->solutions,
			provider->solution_count,
			provider->entries,
			provider->entry_count,
			operation_id,
			&usage
		) != 0 || usage.count > UINT32_MAX) {
		return -1;
	}
	*p_entries = usage.entries;
	*p_count = (uint32_t)usage.count;
	return 0;
}

static int write_identity_root_artifact_with_environment(
	const char* path,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* term_db,
	const struct prototype_type_declaration_db* type_db,
	const struct prototype_judgement_db* judgement,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_context_db* context_db,
	const struct prototype_substitution_db* substitution_db,
	const struct prototype_typed_occurrence_graph* operation_graph
) {
	if (!path || !intrinsic_environment || !interface || !term_db || !type_db || !judgement ||
		!cwf_certificates || !context_db || !substitution_db || !operation_graph) {
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
		0,
		NULL,
		0
	);
	metadata.contexts = *context_db;
	metadata.substitutions = *substitution_db;
	metadata.typed_occurrences = *operation_graph;
	if (!metadata.typed_occurrences.frozen &&
		prototype_typed_occurrence_graph_freeze(
			&metadata.typed_occurrences, term_db, &metadata.contexts
		) != 0) {
		symbol_table_free(&symbols);
		return -1;
	}
	FILE* stream = fopen(path, "w");
	if (!stream) {
		symbol_table_free(&symbols);
		return -1;
	}
	struct prototype_artifact_interface write_interface = *interface;
	write_interface.intrinsic_environment_fingerprint =
		prototype_intrinsic_environment_fingerprint(
			intrinsic_environment
		);
	write_interface.default_integer_host_type =
		intrinsic_environment->default_integer_host_type;
	int status = prototype_artifact_write_text(
		stream,
		&symbols,
		intrinsic_environment,
		&write_interface,
		term_db,
		type_db,
		judgement,
		cwf_certificates,
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

static int write_identity_root_artifact(
	const char* path,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* term_db,
	const struct prototype_type_declaration_db* type_db,
	const struct prototype_judgement_db* judgement,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_context_db* context_db,
	const struct prototype_substitution_db* substitution_db,
	const struct prototype_typed_occurrence_graph* operation_graph
) {
	return write_identity_root_artifact_with_environment(
		path,
		prototype_default_intrinsic_environment(),
		interface,
		term_db,
		type_db,
		judgement,
		cwf_certificates,
		context_db,
		substitution_db,
		operation_graph
	);
}

static int fixture_int64_intrinsic_environment(
	struct prototype_intrinsic_namespace_binding* bindings,
	size_t binding_capacity,
	struct prototype_intrinsic_environment* p_environment
) {
	const struct prototype_intrinsic_environment* base =
		prototype_default_intrinsic_environment();
	if (!bindings || !p_environment ||
		base->namespace_binding_count > binding_capacity) {
		return -1;
	}
	memcpy(
		bindings,
		base->namespace_bindings,
		base->namespace_binding_count * sizeof(*bindings)
	);
	int found = 0;
	for (size_t i = 0; i < base->namespace_binding_count; ++i) {
		if (strcmp(bindings[i].source_name, "Int") == 0) {
			bindings[i].target_id = PROTOTYPE_HOST_TYPE_INT64;
			found = 1;
		}
	}
	*p_environment = (struct prototype_intrinsic_environment) {
		.namespace_bindings = bindings,
		.namespace_binding_count = base->namespace_binding_count,
		.pure_primitives = base->pure_primitives,
		.pure_primitive_count = base->pure_primitive_count,
		.effect_operations = base->effect_operations,
		.effect_operation_count = base->effect_operation_count,
		.default_integer_host_type = PROTOTYPE_HOST_TYPE_INT64
	};
	return found ? 0 : -1;
}

static int fixture_artifact_interface_environment_round_trip(
	const char* path,
	const struct prototype_intrinsic_environment* accepted_environment,
	const struct prototype_intrinsic_environment* rejected_environment,
	const struct prototype_artifact_interface* shape
) {
	if (!path || !accepted_environment || !rejected_environment || !shape) {
		return -1;
	}
#define FIXTURE_ALLOC(type, name, capacity) \
	struct type* name = calloc((capacity) == 0 ? 1 : (capacity), sizeof(*name))
	FIXTURE_ALLOC(
		prototype_artifact_term_export,
		term_exports,
		shape->term_export_capacity
	);
	FIXTURE_ALLOC(
		prototype_artifact_type_export,
		type_exports,
		shape->type_export_capacity
	);
	FIXTURE_ALLOC(
		prototype_artifact_type_parameter_export,
		type_parameters,
		shape->type_parameter_capacity
	);
	FIXTURE_ALLOC(
		prototype_artifact_constructor_export,
		constructor_exports,
		shape->constructor_export_capacity
	);
	uint32_t* field_type_exprs = calloc(
		shape->constructor_field_type_expr_capacity == 0 ? 1 :
			shape->constructor_field_type_expr_capacity,
		sizeof(*field_type_exprs)
	);
	FIXTURE_ALLOC(prototype_type_expr, type_exprs, shape->type_expr_capacity);
	FIXTURE_ALLOC(
		prototype_artifact_identity_root,
		identity_roots,
		shape->identity_root_capacity
	);
	FIXTURE_ALLOC(
		prototype_artifact_dependency,
		dependencies,
		shape->dependency_capacity
	);
#undef FIXTURE_ALLOC
	if (!term_exports || !type_exports || !type_parameters ||
		!constructor_exports || !field_type_exprs || !type_exprs ||
		!identity_roots || !dependencies) {
		free(term_exports);
		free(type_exports);
		free(type_parameters);
		free(constructor_exports);
		free(field_type_exprs);
		free(type_exprs);
		free(identity_roots);
		free(dependencies);
		return -1;
	}
	struct prototype_artifact_interface read_interface;
	prototype_artifact_interface_init(
		&read_interface,
		term_exports,
		shape->term_export_capacity,
		type_exports,
		shape->type_export_capacity,
		type_parameters,
		shape->type_parameter_capacity,
		constructor_exports,
		shape->constructor_export_capacity,
		field_type_exprs,
		shape->constructor_field_type_expr_capacity,
		type_exprs,
		shape->type_expr_capacity,
		identity_roots,
		shape->identity_root_capacity,
		dependencies,
		shape->dependency_capacity
	);
	int symbol_ids[1024];
	uint32_t symbol_hashes[1024];
	char* symbol_strings[512];
	struct symbol_table symbols;
	symbol_table_init(
		&symbols, symbol_ids, symbol_hashes, 1024, symbol_strings, 512
	);
	FILE* stream = fopen(path, "r");
	int status = stream ? prototype_artifact_read_text_interface(
		stream, &symbols, accepted_environment, &read_interface
	) : -1;
	if (stream && fclose(stream) != 0) {
		status = -1;
	}
	stream = status == 0 ? fopen(path, "r") : NULL;
	if (!stream || prototype_artifact_read_text_interface(
			stream, &symbols, rejected_environment, &read_interface
		) == 0) {
		status = -1;
	}
	if (stream && fclose(stream) != 0) {
		status = -1;
	}
	symbol_table_free(&symbols);
	free(term_exports);
	free(type_exports);
	free(type_parameters);
	free(constructor_exports);
	free(field_type_exprs);
	free(type_exprs);
	free(identity_roots);
	free(dependencies);
	return status;
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
		.occurrence_id = operation_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_occurrence_usage_solution usage_solutions[OPERATION_CAPACITY];
	struct prototype_usage_entry usage_entries[CLAIM_CAPACITY * 8];
	size_t usage_entry_count = 0;
	struct prototype_usage_vector operation_usage = {0};
	if (authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
		operation_id != PROTOTYPE_INVALID_ID) {
		if (!fixture_term_db || !fixture_operation_graph ||
			prototype_occurrence_usage_solve(
				fixture_operation_graph,
				fixture_term_db,
				NULL,
				usage_solutions,
				OPERATION_CAPACITY,
				usage_entries,
				CLAIM_CAPACITY * 8,
				&usage_entry_count
			) != 0 || prototype_occurrence_usage_solution_view(
				usage_solutions,
				fixture_operation_graph->occurrence_count,
				usage_entries,
				usage_entry_count,
				operation_id,
				&operation_usage
			) != 0) {
			return PROTOTYPE_INVALID_ID;
		}
		proposition.resource_usage = operation_usage.entries;
		proposition.resource_usage_count = (uint32_t)operation_usage.count;
	}
	uint32_t proposition_id = PROTOTYPE_INVALID_ID;
	if (prototype_judgement_proposition_intern(
			judgement, &proposition, &proposition_id
		) != 0) {
		return PROTOTYPE_INVALID_ID;
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
	struct prototype_typed_occurrence_graph* graph,
	uint32_t context_id,
	uint32_t core,
	uint32_t classifier,
	int category
) {
	uint32_t id = (uint32_t)graph->occurrence_count++;
	struct prototype_typed_occurrence occurrence;
	memset(&occurrence, 0, sizeof(occurrence));
	occurrence.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM;
	occurrence.category = category;
	occurrence.context_id = context_id;
	occurrence.context_action_substitution = PROTOTYPE_INVALID_ID;
	occurrence.source_core_term = PROTOTYPE_INVALID_ID;
	occurrence.source_classifier = PROTOTYPE_INVALID_ID;
	occurrence.core_term = core;
	occurrence.classifier = classifier;
	occurrence.source_ast = PROTOTYPE_INVALID_ID;
	occurrence.source_symbol_id = -1;
	occurrence.binder_symbol_id = -1;
	occurrence.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
	occurrence.binding_id = PROTOTYPE_INVALID_ID;
	occurrence.first_edge = PROTOTYPE_INVALID_ID;
	occurrence.wrapped_occurrence = PROTOTYPE_INVALID_ID;
	occurrence.binder_classifier = PROTOTYPE_INVALID_ID;
	occurrence.ih_owner_occurrence = PROTOTYPE_INVALID_ID;
	occurrence.ih_scope_id = PROTOTYPE_INVALID_ID;
	occurrence.ih_case_index = PROTOTYPE_INVALID_ID;
	occurrence.ih_field_index = PROTOTYPE_INVALID_ID;
	occurrence.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID;
	occurrence.fold_return_binder_id = PROTOTYPE_INVALID_ID;
	occurrence.first_case = PROTOTYPE_INVALID_ID;
	occurrence.first_fold_clause = PROTOTYPE_INVALID_ID;
	graph->occurrences[id] = occurrence;
	if (fixture_term_db && core < fixture_term_db->term_count &&
		fixture_term_db->terms[core].tag == PROTOTYPE_TERM_VAR) {
		graph->occurrences[id].binding_id =
			fixture_term_db->terms[core].as.var.binding_id;
	}
	return id;
}

static int fixture_add_occurrence_edge(
	struct prototype_typed_occurrence_graph* graph,
	uint32_t parent,
	int role,
	uint32_t ordinal,
	uint32_t child
) {
	return prototype_typed_occurrence_graph_add_edge(
		graph,
		parent,
		(struct prototype_typed_occurrence_edge) {
			.role = role,
			.ordinal = ordinal,
			.child_occurrence = child
		}
	);
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
	struct prototype_typed_occurrence_graph* operation_graph,
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
	if (operation_graph->occurrence_count > operation_graph->occurrence_capacity ||
		operation_graph->occurrence_capacity - operation_graph->occurrence_count < 16 ||
		operation_graph->case_count > operation_graph->case_capacity ||
		operation_graph->case_capacity - operation_graph->case_count < 2) {
		return 27;
	}
	uint32_t scrutinee_operation = add_operation(
		operation_graph, xr_context, xr, input_identity,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_argument_occurrence = add_operation(
		operation_graph, xr_context, bool_false, bool_type,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_identity_argument_occurrence = add_operation(
		operation_graph, xr_context, false_identity_witness, false_identity,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_constructor_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[0], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_app1_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[1], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_app2_operation = add_operation(
		operation_graph, xr_context, false_witness_spine[2], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_witness_operation = add_operation(
		operation_graph, xr_context, false_computation_witness,
		false_computation_identity, PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t false_operation = add_operation(
		operation_graph, xr_context, false_witness_return,
		false_branch_computation_identity, PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t true_argument_occurrence = add_operation(
		operation_graph, xr_context, bool_true, bool_type,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_identity_argument_occurrence = add_operation(
		operation_graph, xr_context, true_identity_witness, true_identity,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_constructor_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[0], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_app1_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[1], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_app2_operation = add_operation(
		operation_graph, xr_context, true_witness_spine[2], PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_witness_operation = add_operation(
		operation_graph, xr_context, true_computation_witness,
		true_computation_identity, PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_operation = add_operation(
		operation_graph, xr_context, true_witness_return,
		true_branch_computation_identity, PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t proof_operation = add_operation(
		operation_graph, xr_context, proof_endpoint, PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t first_proof_case = operation_graph->case_count;
	operation_graph->occurrences[scrutinee_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_VAR;
	operation_graph->occurrences[false_argument_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR;
	operation_graph->occurrences[false_identity_argument_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR;
	operation_graph->occurrences[false_constructor_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_ATOM;
	operation_graph->occurrences[false_app1_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[false_app1_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[false_app2_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[false_app2_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[false_witness_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[false_witness_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[false_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_RETURN;
	operation_graph->occurrences[true_argument_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR;
	operation_graph->occurrences[true_identity_argument_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR;
	operation_graph->occurrences[true_constructor_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_ATOM;
	operation_graph->occurrences[true_app1_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[true_app1_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[true_app2_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[true_app2_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[true_witness_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_APP;
	operation_graph->occurrences[true_witness_operation].application_role =
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION;
	operation_graph->occurrences[true_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_RETURN;
	operation_graph->occurrences[proof_operation].tag = PROTOTYPE_TYPED_OCCURRENCE_MATCH;
	operation_graph->occurrences[proof_operation].first_case = first_proof_case;
	operation_graph->occurrences[proof_operation].case_count = 2;
	operation_graph->cases[first_proof_case] = (struct prototype_typed_occurrence_match_case) {
		.context_id = xr_context,
		.has_refinement = 0,
		.refinement_substitution = PROTOTYPE_INVALID_ID,
		.constructor_owner = input_identity,
		.constructor_id = 0,
		.case_label_symbol_id = -1,
		.binder_count = 0
	};
	operation_graph->cases[first_proof_case + 1] =
		(struct prototype_typed_occurrence_match_case) {
			.context_id = xr_context,
			.has_refinement = 0,
			.refinement_substitution = PROTOTYPE_INVALID_ID,
			.constructor_owner = input_identity,
		.constructor_id = 1,
		.case_label_symbol_id = -1,
			.binder_count = 0
	};
	operation_graph->case_count += 2;
	if (fixture_add_occurrence_edge(
			operation_graph, false_app1_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, false_constructor_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_app1_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0, false_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_app2_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, false_app1_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_app2_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0, false_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_witness_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, false_app2_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_witness_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
			false_identity_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, false_operation,
			PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, false_witness_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_app1_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, true_constructor_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_app1_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0, true_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_app2_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, true_app1_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_app2_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0, true_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_witness_operation,
			PROTOTYPE_TERM_CHILD_FUNCTION, 0, true_app2_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_witness_operation,
			PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
			true_identity_argument_occurrence
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, true_operation,
			PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, true_witness_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, proof_operation,
			PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, scrutinee_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, proof_operation,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 0, false_operation
		) != 0 || fixture_add_occurrence_edge(
			operation_graph, proof_operation,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 1, true_operation
		) != 0) {
		return 27;
	}
	struct prototype_occurrence_usage_solution proof_usage_solutions[OPERATION_CAPACITY];
	struct prototype_usage_entry proof_usage_entries[CLAIM_CAPACITY * 8];
	size_t proof_usage_entry_count;
	if (prototype_occurrence_usage_solve(
			operation_graph,
			term_db,
			NULL,
			proof_usage_solutions,
			OPERATION_CAPACITY,
			proof_usage_entries,
			CLAIM_CAPACITY * 8,
			&proof_usage_entry_count
		) != 0) {
		return 27;
	}
	struct fixture_occurrence_usage_provider_context proof_usage_provider = {
		.solutions = proof_usage_solutions,
		.solution_count = operation_graph->occurrence_count,
		.entries = proof_usage_entries,
		.entry_count = proof_usage_entry_count
	};
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
	prototype_judgement_delta_set_occurrence_store(
		&delta,
		operation_graph->occurrences,
		operation_graph->occurrence_count,
		operation_graph->edges,
		operation_graph->edge_count,
		operation_graph->cases,
		operation_graph->case_count
	);
	prototype_judgement_delta_set_occurrence_usage_provider(
		&delta,
		&proof_usage_provider,
		fixture_occurrence_usage_provider
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
	operation_graph->occurrences[false_constructor_operation].classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->occurrences[false_app1_operation].classifier =
		false_app1_classifier;
	operation_graph->occurrences[false_app2_operation].classifier =
		false_app2_classifier;
	operation_graph->occurrences[true_constructor_operation].classifier =
		thunk_identity_constructor->curried_classifier_cache;
	operation_graph->occurrences[true_app1_operation].classifier = true_app1_classifier;
	operation_graph->occurrences[true_app2_operation].classifier = true_app2_classifier;
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
	prototype_judgement_delta_set_occurrence(&delta, false_witness_operation);
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
	prototype_judgement_delta_set_occurrence(&delta, false_operation);
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
	prototype_judgement_delta_set_occurrence(&delta, true_witness_operation);
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
	prototype_judgement_delta_set_occurrence(&delta, true_operation);
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
	prototype_judgement_delta_set_occurrence(&delta, proof_operation);
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
	operation_graph->occurrences[proof_operation].classifier = match_classifier;
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
	prototype_judgement_delta_set_occurrence(&delta, PROTOTYPE_INVALID_ID);
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
	prototype_judgement_delta_set_occurrence(&delta, PROTOTYPE_INVALID_ID);
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
	const struct prototype_parametricity_candidate_db* db,
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
	const struct prototype_parametricity_candidate_db* db,
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
