#include "ast.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
);

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

void prototype_operation_graph_init(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* cases,
	size_t case_capacity
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	graph->operations = operations;
	graph->operation_capacity = operation_capacity;
	graph->cases = cases;
	graph->case_capacity = case_capacity;
}

size_t prototype_operation_graph_count(const struct prototype_operation_graph* graph) {
	return graph ? graph->operation_count : 0;
}

size_t prototype_operation_graph_case_count(const struct prototype_operation_graph* graph) {
	return graph ? graph->case_count : 0;
}

const struct prototype_operation_node* prototype_operation_graph_get(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id
) {
	return graph && operation_id < graph->operation_count ?
		&graph->operations[operation_id] : NULL;
}

const struct prototype_operation_match_case* prototype_operation_graph_get_case(
	const struct prototype_operation_graph* graph,
	uint32_t case_id
) {
	return graph && case_id < graph->case_count ?
		&graph->cases[case_id] : NULL;
}

int prototype_operation_graph_add(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node operation,
	uint32_t* p_operation_id
) {
	if (!graph || !graph->operations ||
		reserve_slot(graph->operation_count, graph->operation_capacity) != 0) {
		return -1;
	}
	if (p_operation_id) {
		*p_operation_id = (uint32_t)graph->operation_count;
	}
	graph->operations[graph->operation_count++] = operation;
	return 0;
}

int prototype_operation_graph_add_case(
	struct prototype_operation_graph* graph,
	struct prototype_operation_match_case operation_case,
	uint32_t* p_case_id
) {
	if (!graph || !graph->cases ||
		reserve_slot(graph->case_count, graph->case_capacity) != 0) {
		return -1;
	}
	if (p_case_id) {
		*p_case_id = (uint32_t)graph->case_count;
	}
	graph->cases[graph->case_count++] = operation_case;
	return 0;
}

int prototype_operation_graph_validate(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms
) {
	if (!graph || !terms ||
		(graph->operation_count != 0 && !graph->operations) ||
		(graph->case_count != 0 && !graph->cases)) {
		return -1;
	}
	for (uint32_t i = 0; i < graph->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(graph, i);
		if (!operation ||
			operation->tag < PROTOTYPE_OPERATION_ATOM ||
			operation->tag > PROTOTYPE_OPERATION_HANDLE) {
			return -1;
		}
		uint32_t term_references[] = {
			operation->core_term,
			operation->known_classifier,
			operation->classifier,
			operation->binder_classifier
		};
		for (size_t j = 0; j < sizeof(term_references) / sizeof(term_references[0]); ++j) {
			if (term_references[j] != PROTOTYPE_INVALID_ID &&
				(term_references[j] >= terms->term_count ||
				 terms->terms[term_references[j]].tag == 0)) {
				return -1;
			}
		}
		uint32_t operation_references[] = {
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee
		};
		for (size_t j = 0;
			j < sizeof(operation_references) / sizeof(operation_references[0]);
			++j) {
			if (operation_references[j] != PROTOTYPE_INVALID_ID &&
				operation_references[j] >= graph->operation_count) {
				return -1;
			}
		}
		if (operation->first_case != PROTOTYPE_INVALID_ID &&
			(operation->first_case > graph->case_count ||
			 operation->case_count > graph->case_count - operation->first_case)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(graph, i);
		if (!operation_case ||
			operation_case->body_operation >= graph->operation_count ||
			operation_case->constructor_owner >= terms->term_count ||
			terms->terms[operation_case->constructor_owner].tag == 0) {
			return -1;
		}
	}
	return 0;
}

void prototype_compile_metadata_operation_graph(
	struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	if (!metadata) {
		return;
	}
	graph->operations = metadata->operations;
	graph->operation_count = metadata->operation_count;
	graph->operation_capacity = metadata->operation_capacity;
	graph->cases = metadata->operation_cases;
	graph->case_count = metadata->operation_case_count;
	graph->case_capacity = metadata->operation_case_capacity;
}

void prototype_compile_metadata_operation_graph_const(
	const struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	if (!metadata) {
		return;
	}
	graph->operations = metadata->operations;
	graph->operation_count = metadata->operation_count;
	graph->operation_capacity = metadata->operation_capacity;
	graph->cases = metadata->operation_cases;
	graph->case_count = metadata->operation_case_count;
	graph->case_capacity = metadata->operation_case_capacity;
}

void prototype_compile_metadata_commit_operation_graph(
	struct prototype_compile_metadata* metadata,
	const struct prototype_operation_graph* graph
) {
	if (!metadata || !graph) {
		return;
	}
	metadata->operation_count = graph->operation_count;
	metadata->operation_case_count = graph->case_count;
}

void prototype_verification_db_init(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation* obligations,
	size_t obligation_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->obligations = obligations;
	db->obligation_capacity = obligation_capacity;
}

uint32_t prototype_verification_obligation_schema_version(int kind) {
	switch (kind) {
		case PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND:
		case PROTOTYPE_VERIFICATION_OBLIGATION_HANDLER_RESULT:
		case PROTOTYPE_VERIFICATION_OBLIGATION_RUNTIME_CONVERSION:
			return 1;
		default:
			return 0;
	}
}

size_t prototype_verification_db_count(const struct prototype_verification_db* db) {
	return db ? db->obligation_count : 0;
}

size_t prototype_verification_db_capacity(const struct prototype_verification_db* db) {
	return db ? db->obligation_capacity : 0;
}

void prototype_verification_db_clear(struct prototype_verification_db* db) {
	if (db) {
		db->obligation_count = 0;
	}
}

const struct prototype_verification_obligation* prototype_verification_db_get(
	const struct prototype_verification_db* db,
	uint32_t obligation_id
) {
	return db && obligation_id < db->obligation_count ?
		&db->obligations[obligation_id] : NULL;
}

struct prototype_verification_obligation* prototype_verification_db_get_mutable(
	struct prototype_verification_db* db,
	uint32_t obligation_id
) {
	return db && obligation_id < db->obligation_count ?
		&db->obligations[obligation_id] : NULL;
}

int prototype_verification_db_find_operation(
	const struct prototype_verification_db* db,
	int kind,
	uint32_t operation,
	uint32_t* p_obligation_id
) {
	if (!db || !p_obligation_id ||
		prototype_verification_obligation_schema_version(kind) == 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (obligation && obligation->kind == kind &&
			obligation->operation == operation) {
			*p_obligation_id = i;
			return 0;
		}
	}
	return 1;
}

static int verification_term_reference_present(
	const struct prototype_term_db* terms,
	uint32_t term
) {
	return terms && term < terms->term_count && terms->terms[term].tag != 0;
}

int prototype_verification_db_validate(
	const struct prototype_verification_db* db,
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms
) {
	if (!db || !graph || !terms) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (!obligation ||
			obligation->schema_version !=
				prototype_verification_obligation_schema_version(obligation->kind) ||
			obligation->state < PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			obligation->state > PROTOTYPE_VERIFICATION_OBLIGATION_FAILED ||
			obligation->operation >= graph->operation_count ||
			!verification_term_reference_present(terms, obligation->core_term)) {
			return -1;
		}
		switch (obligation->kind) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND:
				if (obligation->computation_operation >= graph->operation_count ||
					obligation->continuation_operation >= graph->operation_count ||
					!verification_term_reference_present(
						terms, obligation->input_classifier
					) ||
					!verification_term_reference_present(
						terms, obligation->classifier_family
					) ||
					!verification_term_reference_present(terms, obligation->effect_row)) {
					return -1;
				}
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_HANDLER_RESULT:
				if (obligation->computation_operation >= graph->operation_count ||
					!verification_term_reference_present(
						terms, obligation->classifier_family
					)) {
					return -1;
				}
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_RUNTIME_CONVERSION:
				if (!verification_term_reference_present(
						terms, obligation->input_classifier
					) ||
					!verification_term_reference_present(
						terms, obligation->classifier_family
					)) {
					return -1;
				}
				break;
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_verification_db_coverage(
	const struct prototype_verification_db* db,
	struct prototype_verification_coverage* p_coverage
) {
	if (!db || !p_coverage) {
		return -1;
	}
	memset(p_coverage, 0, sizeof(*p_coverage));
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (!obligation || obligation->kind <= 0 || obligation->kind >= 64) {
			return -1;
		}
		p_coverage->reachable_kind_mask |= UINT64_C(1) << obligation->kind;
		switch (obligation->state) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_PENDING:
				p_coverage->pending_count++;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED:
				p_coverage->discharged_count++;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_FAILED:
				p_coverage->failed_count++;
				break;
			default:
				return -1;
		}
		switch (obligation->kind) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND:
				p_coverage->required_runtime_capabilities |=
					PROTOTYPE_RUNTIME_CAPABILITY_DEPENDENT_BIND_VERIFIER;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_HANDLER_RESULT:
				p_coverage->required_runtime_capabilities |=
					PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_RUNTIME_CONVERSION:
				p_coverage->required_runtime_capabilities |=
					PROTOTYPE_RUNTIME_CAPABILITY_CONVERSION_VERIFIER;
				break;
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_verification_db_add(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation obligation,
	uint32_t* p_obligation_id
) {
	uint32_t current_schema =
		prototype_verification_obligation_schema_version(obligation.kind);
	if (!db || !db->obligations ||
		current_schema == 0 ||
		(obligation.schema_version != 0 &&
			obligation.schema_version != current_schema) ||
		reserve_slot(db->obligation_count, db->obligation_capacity) != 0) {
		return -1;
	}
	obligation.schema_version = current_schema;
	if (p_obligation_id) {
		*p_obligation_id = (uint32_t)db->obligation_count;
	}
	db->obligations[db->obligation_count++] = obligation;
	return 0;
}

int prototype_verification_db_discharge_dependent_bind(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t continuation_result_classifier
) {
	if (!db || !terms || !type_declarations || obligation_id >= db->obligation_count ||
		returned_value >= terms->term_count ||
		continuation_result_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_verification_obligation* obligation =
		prototype_verification_db_get_mutable(db, obligation_id);
	if (!obligation) {
		return -1;
	}
	if (obligation->kind != PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND ||
		obligation->state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
		obligation->classifier_family >= terms->term_count) {
		return -1;
	}
	uint32_t family_lambda;
	uint32_t family_application;
	uint32_t normalized_application;
	if (prototype_term_pure_family_lambda(
			terms, obligation->classifier_family, &family_lambda
		) != 0 || prototype_term_app(
			terms, family_lambda, returned_value, &family_application
		) != 0 || prototype_term_whnf_with_profile(
			terms,
			type_declarations,
			NULL,
			obligation->normalization_profile,
			family_application,
			&normalized_application
		) != 0 || normalized_application >= terms->term_count) {
		obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return 0;
	}
	uint32_t expected_classifier = normalized_application;
	if (terms->terms[expected_classifier].tag == PROTOTYPE_TERM_RETURN) {
		expected_classifier = terms->terms[expected_classifier].as.return_term.value;
	}
	struct prototype_term_classifier_view expected_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, expected_classifier, &expected_view
		) != 0 || expected_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		expected_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			expected_classifier,
			continuation_result_classifier
		)) {
		obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return 0;
	}
	obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	return 0;
}

struct operation_runtime_binding {
	uint32_t ast_binder_id;
	uint32_t binder_id;
	uint32_t value;
};

struct operation_runtime_environment {
	struct operation_runtime_binding bindings[512];
	uint32_t count;
};

static int operation_runtime_instantiate_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct operation_runtime_environment* environment,
	uint32_t term,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !environment || !p_ret || term >= terms->term_count) {
		return -1;
	}
	uint32_t current = term;
	for (uint32_t i = 0; i < environment->count; ++i) {
		if (prototype_term_substitute(
				terms,
				type_declarations,
				current,
				environment->bindings[i].binder_id,
				environment->bindings[i].value,
				&current
			) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

static int operation_runtime_extend_environment(
	const struct operation_runtime_environment* source,
	uint32_t ast_binder_id,
	uint32_t binder_id,
	uint32_t value,
	struct operation_runtime_environment* p_ret
) {
	if (!source || !p_ret || value == PROTOTYPE_INVALID_ID || source->count >= 512) {
		return -1;
	}
	*p_ret = *source;
	p_ret->bindings[p_ret->count++] =
		(struct operation_runtime_binding){ ast_binder_id, binder_id, value };
	return 0;
}

static int operation_runtime_lookup_value(
	const struct operation_runtime_environment* environment,
	uint32_t ast_binder_id,
	uint32_t* p_value
) {
	if (!environment || !p_value || ast_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = environment->count; i > 0; --i) {
		if (environment->bindings[i - 1].ast_binder_id == ast_binder_id) {
			*p_value = environment->bindings[i - 1].value;
			return 0;
		}
	}
	return 1;
}

static uint32_t operation_runtime_unwrap_name(
	const struct prototype_compile_metadata* metadata,
	uint32_t operation_id
) {
	for (size_t visited = 0;
		metadata && operation_id < metadata->operation_count &&
		visited < metadata->operation_count;
		++visited) {
		const struct prototype_operation_node* operation = &metadata->operations[operation_id];
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

static int operation_runtime_evaluate(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	const struct operation_runtime_environment* environment,
	uint32_t* p_ret,
	int* p_verification_state,
	uint32_t depth
);

static int operation_runtime_wrap_handler_continuation(
	struct prototype_term_db* terms,
	uint32_t handler,
	uint32_t request_continuation,
	uint32_t* p_ret
) {
	if (!terms || !p_ret || handler >= terms->term_count ||
		request_continuation >= terms->term_count) {
		return -1;
	}
	uint32_t binder_id = prototype_term_fresh_binder(terms);
	uint32_t result_var;
	uint32_t forced;
	uint32_t resumed;
	uint32_t resumed_under_handler;
	uint32_t continuation_lambda;
	if (binder_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(terms, binder_id, &result_var) != 0 ||
		prototype_term_force(terms, request_continuation, &forced) != 0 ||
		prototype_term_app(terms, forced, result_var, &resumed) != 0 ||
		prototype_term_handle(terms, handler, resumed, &resumed_under_handler) != 0 ||
		prototype_term_lambda(
			terms, binder_id, resumed_under_handler, &continuation_lambda
		) != 0 || prototype_term_thunk(terms, continuation_lambda, p_ret) != 0) {
		return -1;
	}
	return 0;
}

static int operation_runtime_discharge_bind(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct operation_runtime_environment* environment,
	uint32_t operation_id,
	uint32_t returned_value,
	int* p_verification_state
) {
	uint32_t obligation_id = PROTOTYPE_INVALID_ID;
	int find_status = prototype_verification_db_find_operation(
		&metadata->verification,
		PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND,
		operation_id,
		&obligation_id
	);
	if (find_status > 0) {
		return 0;
	}
	if (find_status < 0) {
		return -1;
	}
	const struct prototype_verification_obligation* obligation =
		prototype_verification_db_get(&metadata->verification, obligation_id);
	if (!obligation) {
		return -1;
	}
	const struct prototype_operation_node* operation = &metadata->operations[operation_id];
	const struct prototype_operation_node* continuation =
		&metadata->operations[operation->argument];
	uint32_t continuation_classifier;
	if (operation_runtime_instantiate_term(
			terms,
			type_declarations,
			environment,
			continuation->classifier,
			&continuation_classifier
		) != 0) {
		return -1;
	}
	uint32_t domain;
	uint32_t classifier_family;
	uint32_t family_lambda;
	uint32_t family_application;
	if (prototype_judgement_pi_parts(
			terms, continuation_classifier, &domain, &classifier_family
		) != 0 || prototype_term_pure_family_lambda(
			terms, classifier_family, &family_lambda
		) != 0 || prototype_term_app(
			terms, family_lambda, returned_value, &family_application
		) != 0 || prototype_term_whnf_with_profile(
			terms,
			type_declarations,
			definitions,
			obligation->normalization_profile,
			family_application,
			&continuation_classifier
		) != 0 || continuation_classifier >= terms->term_count) {
		return -1;
	}
	if (terms->terms[continuation_classifier].tag == PROTOTYPE_TERM_RETURN) {
		continuation_classifier = terms->terms[continuation_classifier].as.return_term.value;
	}
	struct prototype_verification_obligation frame_obligation =
		*obligation;
	struct prototype_verification_db frame_verification;
	prototype_verification_db_init(&frame_verification, &frame_obligation, 1);
	if (prototype_verification_db_add(
			&frame_verification, frame_obligation, NULL
		) != 0) {
		return -1;
	}
	if (prototype_verification_db_discharge_dependent_bind(
			&frame_verification,
			terms,
			type_declarations,
			0,
			returned_value,
			continuation_classifier
		) != 0 || frame_obligation.state !=
			PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
		if (p_verification_state) {
			*p_verification_state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		}
		return -1;
	}
	if (p_verification_state) {
		*p_verification_state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	}
	return 0;
}

static int operation_runtime_evaluate(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	const struct operation_runtime_environment* environment,
	uint32_t* p_ret,
	int* p_verification_state,
	uint32_t depth
) {
	if (!metadata || !terms || !type_declarations || !environment || !p_ret || depth == 0 ||
		operation_id >= metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation = &metadata->operations[operation_id];
	if (operation->core_term >= terms->term_count) {
		return -1;
	}
	if (operation->tag == PROTOTYPE_OPERATION_NAME) {
		return operation_runtime_evaluate(
			metadata, terms, type_declarations, definitions, options,
			operation->function, environment, p_ret, p_verification_state, depth - 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
		return operation_runtime_evaluate(
			metadata, terms, type_declarations, definitions, options,
			operation->body, environment, p_ret, p_verification_state, depth - 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_VAR) {
		int lookup_status = operation_runtime_lookup_value(
			environment, operation->referenced_ast_binder_id, p_ret
		);
		if (lookup_status <= 0) {
			return lookup_status;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_RETURN) {
		uint32_t value;
		if (operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, options,
				operation->argument, environment, &value, p_verification_state, depth - 1
			) != 0) {
			return -1;
		}
		if (value < terms->term_count && terms->terms[value].tag == PROTOTYPE_TERM_RETURN) {
			value = terms->terms[value].as.return_term.value;
		}
		return prototype_term_return(terms, value, p_ret);
	}
	if (operation->tag == PROTOTYPE_OPERATION_BIND) {
		if (operation->function >= metadata->operation_count ||
			operation->argument >= metadata->operation_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_BIND) {
			return -1;
		}
		uint32_t input_result;
		if (operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, options,
				operation->function, environment, &input_result, p_verification_state, depth - 1
			) != 0 || input_result >= terms->term_count ||
			terms->terms[input_result].tag != PROTOTYPE_TERM_RETURN) {
			return -1;
		}
		uint32_t returned_value = terms->terms[input_result].as.return_term.value;
		if (operation_runtime_discharge_bind(
				metadata, terms, type_declarations, definitions, environment,
				operation_id, returned_value, p_verification_state
			) != 0) {
			return -1;
		}
		uint32_t continuation_operation = operation_runtime_unwrap_name(
			metadata, operation->argument
		);
		if (continuation_operation >= metadata->operation_count ||
			metadata->operations[continuation_operation].tag != PROTOTYPE_OPERATION_LAMBDA) {
			return -1;
		}
		const struct prototype_operation_node* continuation =
			&metadata->operations[continuation_operation];
		const struct prototype_term* continuation_term = &terms->terms[continuation->core_term];
		if (continuation_term->tag != PROTOTYPE_TERM_LAMBDA ||
			continuation->body >= metadata->operation_count) {
			return -1;
		}
		struct operation_runtime_environment extended;
		if (operation_runtime_extend_environment(
				environment,
				continuation->referenced_ast_binder_id,
				continuation_term->as.lambda.binder_id,
				returned_value,
				&extended
			) != 0) {
			return -1;
		}
		return operation_runtime_evaluate(
			metadata, terms, type_declarations, definitions, options,
			continuation->body, &extended, p_ret, p_verification_state, depth - 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_HANDLE) {
		if (operation->function >= metadata->operation_count ||
			operation->argument >= metadata->operation_count ||
			operation->body >= metadata->operation_count ||
			operation->scrutinee >= metadata->operation_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_HANDLE) {
			return -1;
		}
		const struct prototype_term* handle_term = &terms->terms[operation->core_term];
		if (handle_term->as.handle.handler >= terms->term_count ||
			terms->terms[handle_term->as.handle.handler].tag != PROTOTYPE_TERM_HANDLER) {
			return -1;
		}
		const struct prototype_term* handler =
			&terms->terms[handle_term->as.handle.handler];
		struct prototype_term_reduction_options inner_options = options;
		inner_options.flags &= ~PROTOTYPE_TERM_PERFORM_HOST_EFFECT;
		inner_options.operation_dispatch = NULL;
		inner_options.operation_dispatch_context = NULL;
		uint32_t computation;
		if (operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, inner_options,
				operation->function, environment, &computation,
				p_verification_state, depth - 1
			) != 0 || computation >= terms->term_count) {
			return -1;
		}
		if (terms->terms[computation].tag == PROTOTYPE_TERM_RETURN) {
			struct operation_runtime_environment extended;
			if (operation_runtime_extend_environment(
					environment,
					operation->handler_return_ast_binder_id,
					operation->handler_return_binder_id,
					terms->terms[computation].as.return_term.value,
					&extended
				) != 0) {
				return -1;
			}
			return operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, options,
				operation->scrutinee, &extended, p_ret, p_verification_state, depth - 1
			);
		}
		if (terms->terms[computation].tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
			const struct prototype_term* request = &terms->terms[computation];
			int handles_operation = 0;
			if (prototype_term_core_shape_equal(
					terms, handler->as.handler.operation,
					request->as.operation_request.operation, &handles_operation
				) != 0) {
				return -1;
			}
			uint32_t continuation;
			if (operation_runtime_wrap_handler_continuation(
					terms, handle_term->as.handle.handler,
					request->as.operation_request.continuation, &continuation
				) != 0) {
				return -1;
			}
			if (!handles_operation) {
				return prototype_term_operation_request(
					terms, request->as.operation_request.operation,
					request->as.operation_request.argument, continuation, p_ret
				);
			}
			struct operation_runtime_environment argument_environment;
			struct operation_runtime_environment clause_environment;
			if (operation_runtime_extend_environment(
					environment,
					operation->handler_argument_ast_binder_id,
					operation->handler_argument_binder_id,
					request->as.operation_request.argument,
					&argument_environment
				) != 0 || operation_runtime_extend_environment(
					&argument_environment,
					operation->handler_continuation_ast_binder_id,
					operation->handler_continuation_binder_id,
					continuation,
					&clause_environment
				) != 0) {
				return -1;
			}
			return operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, options,
				operation->body, &clause_environment, p_ret,
				p_verification_state, depth - 1
			);
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_APP) {
		uint32_t function_operation = operation_runtime_unwrap_name(metadata, operation->function);
		if (function_operation < metadata->operation_count &&
			metadata->operations[function_operation].tag == PROTOTYPE_OPERATION_LAMBDA) {
			uint32_t argument;
			if (operation_runtime_evaluate(
					metadata, terms, type_declarations, definitions, options,
					operation->argument, environment, &argument, p_verification_state, depth - 1
				) != 0) {
				return -1;
			}
			if (argument < terms->term_count && terms->terms[argument].tag == PROTOTYPE_TERM_RETURN) {
				argument = terms->terms[argument].as.return_term.value;
			}
			const struct prototype_operation_node* function =
				&metadata->operations[function_operation];
			const struct prototype_term* lambda = &terms->terms[function->core_term];
			struct operation_runtime_environment extended;
			if (lambda->tag != PROTOTYPE_TERM_LAMBDA ||
				operation_runtime_extend_environment(
					environment,
					function->referenced_ast_binder_id,
					lambda->as.lambda.binder_id,
					argument,
					&extended
				) != 0) {
				return -1;
			}
			return operation_runtime_evaluate(
				metadata, terms, type_declarations, definitions, options,
				function->body, &extended, p_ret, p_verification_state, depth - 1
			);
		}
	}
	uint32_t instantiated;
	if (operation_runtime_instantiate_term(
			terms, type_declarations, environment, operation->core_term, &instantiated
		) != 0) {
		return -1;
	}
	return prototype_term_perform_with_options(
		terms, type_declarations, definitions, options, instantiated, p_ret
	);
}

int prototype_operation_evaluate_with_verification(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state
) {
	if (p_verification_state) {
		*p_verification_state = 0;
	}
	struct operation_runtime_environment environment;
	memset(&environment, 0, sizeof(environment));
	return operation_runtime_evaluate(
		metadata,
		terms,
		type_declarations,
		definitions,
		options,
		operation_id,
		&environment,
		p_ret,
		p_verification_state,
		100000
	);
}

void prototype_compile_metadata_init(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_label* labels,
	size_t label_capacity,
	struct prototype_compile_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_compile_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	struct prototype_resolve_error* resolve_errors,
	size_t resolve_error_capacity,
	struct prototype_resolution_item* resolution_items,
	size_t resolution_item_capacity,
	struct prototype_resolution_iteration* resolution_iterations,
	size_t resolution_iteration_capacity,
	struct prototype_resolution_event* resolution_events,
	size_t resolution_event_capacity,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* operation_cases,
	size_t operation_case_capacity,
	struct prototype_verification_obligation* verification_obligations,
	size_t verification_obligation_capacity
) {
	memset(metadata, 0, sizeof(*metadata));
	metadata->compile_policy = PROTOTYPE_COMPILE_POLICY_HYBRID;
	metadata->normalization_step_limit = PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT;
	metadata->solver_step_limit = PROTOTYPE_SOLVER_DEFAULT_STEP_LIMIT;
	metadata->labels = labels;
	metadata->label_capacity = label_capacity;
	metadata->type_exports = type_exports;
	metadata->type_export_capacity = type_export_capacity;
	metadata->constructor_exports = constructor_exports;
	metadata->constructor_export_capacity = constructor_export_capacity;
	metadata->resolve_errors = resolve_errors;
	metadata->resolve_error_capacity = resolve_error_capacity;
	metadata->resolution_items = resolution_items;
	metadata->resolution_item_capacity = resolution_item_capacity;
	metadata->resolution_iterations = resolution_iterations;
	metadata->resolution_iteration_capacity = resolution_iteration_capacity;
	metadata->resolution_events = resolution_events;
	metadata->resolution_event_capacity = resolution_event_capacity;
	metadata->operations = operations;
	metadata->operation_capacity = operation_capacity;
	metadata->operation_cases = operation_cases;
	metadata->operation_case_capacity = operation_case_capacity;
	prototype_verification_db_init(
		&metadata->verification,
		verification_obligations,
		verification_obligation_capacity
	);
}

static int canonical_keys_equal(
	const struct prototype_term_canonical_key* left,
	const struct prototype_term_canonical_key* right
) {
	return left && right &&
		left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->bound_binder_count == right->bound_binder_count &&
		left->free_binder_count == right->free_binder_count &&
		left->has_frame_local_reference == right->has_frame_local_reference &&
		left->has_type_local_reference == right->has_type_local_reference &&
		left->has_type_name_reference == right->has_type_name_reference &&
		left->has_type_universe_reference == right->has_type_universe_reference;
}

static int canonical_key_is_cross_artifact_linkable(
	const struct prototype_term_canonical_key* key
) {
	return key &&
		!key->has_type_local_reference &&
		!key->has_frame_local_reference;
}

static int find_existing_term_by_canonical_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	const struct prototype_term_canonical_key* key,
	uint32_t appended_term,
	uint32_t* p_term_id
);

void prototype_canonical_link_table_init(
	struct prototype_canonical_link_table* table,
	struct prototype_canonical_link_entry* entries,
	size_t entry_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->entries = entries;
	table->entry_capacity = entry_capacity;
}

void prototype_artifact_interface_init(
	struct prototype_artifact_interface* interface,
	struct prototype_artifact_term_export* term_exports,
	size_t term_export_capacity,
	struct prototype_artifact_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_artifact_type_parameter_export* type_parameters,
	size_t type_parameter_capacity,
	struct prototype_artifact_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	uint32_t* constructor_field_type_exprs,
	size_t constructor_field_type_expr_capacity,
	struct prototype_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_artifact_dependency* dependencies,
	size_t dependency_capacity
) {
	if (!interface) {
		return;
	}
	memset(interface, 0, sizeof(*interface));
	interface->term_exports = term_exports;
	interface->term_export_capacity = term_export_capacity;
	interface->type_exports = type_exports;
	interface->type_export_capacity = type_export_capacity;
	interface->type_parameters = type_parameters;
	interface->type_parameter_capacity = type_parameter_capacity;
	interface->constructor_exports = constructor_exports;
	interface->constructor_export_capacity = constructor_export_capacity;
	interface->constructor_field_type_exprs = constructor_field_type_exprs;
	interface->constructor_field_type_expr_capacity = constructor_field_type_expr_capacity;
	interface->type_exprs = type_exprs;
	interface->type_expr_capacity = type_expr_capacity;
	interface->dependencies = dependencies;
	interface->dependency_capacity = dependency_capacity;
}

void prototype_artifact_relocation_table_init(
	struct prototype_artifact_relocation_table* table,
	struct prototype_artifact_external_term_ref* external_term_refs,
	size_t external_term_ref_capacity,
	struct prototype_artifact_resolved_external_term_ref* resolved_external_term_refs,
	size_t resolved_external_term_ref_capacity,
	struct prototype_artifact_external_type_expr_ref* external_type_expr_refs,
	size_t external_type_expr_ref_capacity,
	struct prototype_artifact_resolved_external_type_expr_ref* resolved_external_type_expr_refs,
	size_t resolved_external_type_expr_ref_capacity,
	struct prototype_artifact_external_type_former_ref* external_type_former_refs,
	size_t external_type_former_ref_capacity,
	struct prototype_artifact_resolved_external_type_former_ref* resolved_external_type_former_refs,
	size_t resolved_external_type_former_ref_capacity,
	struct prototype_artifact_resolved_constructor_owner_ref* resolved_constructor_owner_refs,
	size_t resolved_constructor_owner_ref_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->external_term_refs = external_term_refs;
	table->external_term_ref_capacity = external_term_ref_capacity;
	table->resolved_external_term_refs = resolved_external_term_refs;
	table->resolved_external_term_ref_capacity = resolved_external_term_ref_capacity;
	table->external_type_expr_refs = external_type_expr_refs;
	table->external_type_expr_ref_capacity = external_type_expr_ref_capacity;
	table->resolved_external_type_expr_refs = resolved_external_type_expr_refs;
	table->resolved_external_type_expr_ref_capacity =
		resolved_external_type_expr_ref_capacity;
	table->external_type_former_refs = external_type_former_refs;
	table->external_type_former_ref_capacity = external_type_former_ref_capacity;
	table->resolved_external_type_former_refs = resolved_external_type_former_refs;
	table->resolved_external_type_former_ref_capacity =
		resolved_external_type_former_ref_capacity;
	table->resolved_constructor_owner_refs = resolved_constructor_owner_refs;
	table->resolved_constructor_owner_ref_capacity =
		resolved_constructor_owner_ref_capacity;
}

void prototype_artifact_debug_table_init(
	struct prototype_artifact_debug_table* table,
	struct prototype_artifact_debug_term_name* term_names,
	size_t term_name_capacity,
	struct prototype_artifact_debug_type_name* type_names,
	size_t type_name_capacity,
	struct prototype_artifact_debug_constructor_name* constructor_names,
	size_t constructor_name_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->term_names = term_names;
	table->term_name_capacity = term_name_capacity;
	table->type_names = type_names;
	table->type_name_capacity = type_name_capacity;
	table->constructor_names = constructor_names;
	table->constructor_name_capacity = constructor_name_capacity;
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

static int artifact_interface_exports_term_name(
	const struct prototype_artifact_interface* interface,
	struct prototype_qualified_name name
) {
	if (!interface || name.name_symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id == name.namespace_symbol_id &&
			interface->term_exports[i].name_symbol_id == name.name_symbol_id) {
			return 1;
		}
	}
	return 0;
}

static int lookup_export_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement || !p_classifier) {
		return -1;
	}
	for (size_t i = judgement->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
}

int prototype_artifact_interface_build_from_metadata(
	struct prototype_artifact_interface* interface,
	const struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !metadata || !terms || !type_declarations || !judgement) {
		return -1;
	}
	if (metadata->label_count > interface->term_export_capacity ||
		metadata->type_export_count > interface->type_export_capacity ||
		metadata->constructor_export_count > interface->constructor_export_capacity ||
		type_declarations->parameter_count > interface->type_parameter_capacity ||
		type_declarations->readback_field_type_count > interface->constructor_field_type_expr_capacity ||
		type_declarations->expr_count > interface->type_expr_capacity) {
		return -1;
	}

	interface->term_export_count = 0;
	interface->type_export_count = 0;
	interface->type_parameter_count = 0;
	interface->constructor_export_count = 0;
	interface->constructor_field_type_expr_count = 0;
	interface->type_expr_count = 0;
	interface->dependency_count = 0;

	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		interface->type_exprs[interface->type_expr_count++] =
			type_declarations->exprs[i];
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[i];
		struct prototype_artifact_type_parameter_export* export =
			&interface->type_parameters[interface->type_parameter_count++];
		export->binder_id = parameter->binder_id;
		export->name_symbol_id = parameter->name_symbol_id;
		export->type_expr = parameter->type_expr;
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		interface->constructor_field_type_exprs[
			interface->constructor_field_type_expr_count++
		] = type_declarations->readback_field_types[i];
	}

	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		struct prototype_artifact_term_export* export =
			&interface->term_exports[interface->term_export_count++];
		export->namespace_symbol_id = -1;
		export->name_symbol_id = label->name_symbol_id;
		export->local_term = label->term;
		export->canonical_key = label->canonical_key;
		export->transparency = PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT;
		if (label->classifier != PROTOTYPE_INVALID_ID) {
			export->classifier = label->classifier;
		} else if (lookup_export_classifier(judgement, label->term, &export->classifier) != 0) {
			export->classifier = PROTOTYPE_INVALID_ID;
		}
		memset(&export->classifier_key, 0, sizeof(export->classifier_key));
		if (export->classifier != PROTOTYPE_INVALID_ID &&
			prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				export->classifier,
				&export->classifier_key
			) != 0) {
			return -1;
		}
		if (export->classifier != PROTOTYPE_INVALID_ID) {
			uint32_t existing_classifier;
			int found_existing_classifier = find_existing_term_by_canonical_key(
				terms,
				type_declarations,
				export->classifier,
				&export->classifier_key,
				export->classifier,
				&existing_classifier
			);
			if (found_existing_classifier < 0) {
				return -1;
			}
			if (found_existing_classifier > 0) {
				export->classifier = existing_classifier;
				if (prototype_term_canonical_key_with_types(
						terms,
						type_declarations,
						existing_classifier,
						&export->classifier_key
					) != 0) {
					return -1;
				}
			}
		}
		uint32_t existing_term;
		int found_existing = find_existing_term_by_canonical_key(
			terms,
			type_declarations,
			export->local_term,
			&export->canonical_key,
			export->local_term,
			&existing_term
		);
		if (found_existing < 0) {
			return -1;
		}
		if (found_existing > 0) {
			export->local_term = existing_term;
			if (prototype_term_canonical_key_with_types(
					terms,
					type_declarations,
					existing_term,
					&export->canonical_key
				) != 0) {
				return -1;
			}
		}
	}

	for (size_t i = 0; i < metadata->type_export_count; ++i) {
		const struct prototype_compile_type_export* type_export =
			&metadata->type_exports[i];
		struct prototype_artifact_type_export* export =
			&interface->type_exports[interface->type_export_count++];
		export->namespace_symbol_id = -1;
		export->name_symbol_id = type_export->name_symbol_id;
		export->local_type_id = type_export->type_id;
		export->code_shape_key = type_export->code_shape_key;
		if (type_export->type_id >= type_declarations->type_count) {
			return -1;
		}
			if (prototype_type_declaration_representation_anchor_type_id(
					terms,
					type_declarations,
					type_export->type_id,
					&export->core_representation_anchor_type_id
				) != 0) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_export->type_id];
		if (type->formation_classifier == PROTOTYPE_INVALID_ID ||
			type->formation_classifier >= terms->term_count) {
			return -1;
		}
		export->formation_classifier = type->formation_classifier;
		export->first_parameter = type->first_parameter;
		export->parameter_count = type->parameter_count;
		export->first_constructor_export = type_export->first_constructor_export;
		export->constructor_count = type_export->constructor_count;
	}

	for (size_t i = 0; i < metadata->constructor_export_count; ++i) {
		const struct prototype_compile_constructor_export* constructor_export =
			&metadata->constructor_exports[i];
		struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[interface->constructor_export_count++];
		export->type_export_index = constructor_export->type_export_index;
		export->name_symbol_id = constructor_export->name_symbol_id;
		export->ordinal = constructor_export->ordinal;
		export->readback_first_field_type = constructor_export->readback_first_field_type;
		export->readback_field_count = constructor_export->readback_field_count;
		export->classifier_family = constructor_export->classifier_family;
	}

	return 0;
}

int prototype_artifact_interface_add_dependency(
	struct prototype_artifact_interface* interface,
	int name_symbol_id
) {
	return prototype_artifact_interface_add_dependency_in_namespace(
		interface,
		-1,
		name_symbol_id
	);
}

int prototype_artifact_interface_add_dependency_in_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id
) {
	if (!interface || name_symbol_id < 0) {
		return -1;
	}
	for (size_t i = 0; i < interface->dependency_count; ++i) {
		if (interface->dependencies[i].namespace_symbol_id == namespace_symbol_id &&
			interface->dependencies[i].name_symbol_id == name_symbol_id) {
			return 0;
		}
	}
	if (interface->dependency_count >= interface->dependency_capacity) {
		return -1;
	}
	interface->dependencies[interface->dependency_count].namespace_symbol_id =
		namespace_symbol_id;
	interface->dependencies[interface->dependency_count].name_symbol_id = name_symbol_id;
	interface->dependency_count++;
	return 0;
}

void prototype_artifact_interface_set_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id
) {
	if (!interface) {
		return;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id < 0) {
			interface->term_exports[i].namespace_symbol_id = namespace_symbol_id;
		}
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].namespace_symbol_id < 0) {
			interface->type_exports[i].namespace_symbol_id = namespace_symbol_id;
		}
	}
}

static int collect_term_dependencies_at_depth(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
) {
	if (!interface || !terms || term_id >= terms->term_count || depth > 256) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			if (artifact_interface_exports_term_name(interface, term->as.external_ref.name)) {
				return 0;
			}
			return prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				term->as.external_ref.name.namespace_symbol_id,
				term->as.external_ref.name.name_symbol_id
			);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.constructor.owner,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.app.function,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.app.argument,
				depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.lambda.body,
				depth + 1
			);
		case PROTOTYPE_TERM_PI:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.pi.domain,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.pi.codomain_family,
				depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.match.scrutinee,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count ||
					collect_term_dependencies_at_depth(
						interface,
						terms,
						terms->cases[case_id].body,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.type_view.core,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.type_view.source,
				depth + 1
			);
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.induction_hypothesis.argument,
					depth + 1
				);
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				if (collect_term_dependencies_at_depth(
						interface,
						terms,
						term->as.computation_type.label,
						depth + 1
					) != 0) {
					return -1;
				}
				return collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.computation_type.result,
					depth + 1
				);
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_union.left, depth + 1
				) == 0 ? collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_union.right, depth + 1
				) : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_forall.body, depth + 1
				);
			case PROTOTYPE_TERM_THUNK_TYPE:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.thunk_type.computation, depth + 1
				);
			case PROTOTYPE_TERM_RETURN:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.return_term.value, depth + 1
				);
			case PROTOTYPE_TERM_THUNK:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.thunk.computation, depth + 1
				);
			case PROTOTYPE_TERM_FORCE:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.force.value, depth + 1
				);
			case PROTOTYPE_TERM_BIND:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.bind.computation, depth + 1
				) == 0 ? collect_term_dependencies_at_depth(
					interface, terms, term->as.bind.continuation, depth + 1
				) : -1;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.operation, depth + 1
				) == 0 && collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.argument, depth + 1
				) == 0 ? collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.continuation, depth + 1
				) : -1;
			case PROTOTYPE_TERM_HANDLER:
				return collect_term_dependencies_at_depth(interface, terms, term->as.handler.operation, depth + 1) == 0 &&
					collect_term_dependencies_at_depth(interface, terms, term->as.handler.return_clause, depth + 1) == 0 ?
					collect_term_dependencies_at_depth(interface, terms, term->as.handler.operation_clause, depth + 1) : -1;
			case PROTOTYPE_TERM_HANDLE:
				return collect_term_dependencies_at_depth(interface, terms, term->as.handle.handler, depth + 1) == 0 ?
					collect_term_dependencies_at_depth(interface, terms, term->as.handle.computation, depth + 1) : -1;
			case PROTOTYPE_TERM_HANDLER_TYPE:
				return collect_term_dependencies_at_depth(interface, terms, term->as.handler_type.operation, depth + 1) == 0 &&
					collect_term_dependencies_at_depth(interface, terms, term->as.handler_type.input_computation, depth + 1) == 0 ?
					collect_term_dependencies_at_depth(interface, terms, term->as.handler_type.output_computation, depth + 1) : -1;
			default:
				return 0;
	}
}

int prototype_artifact_interface_collect_dependencies(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	interface->dependency_count = 0;
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (export->local_term < terms->term_count &&
			collect_term_dependencies_at_depth(interface, terms, export->local_term, 0) != 0) {
			return -1;
		}
		if (export->classifier < terms->term_count &&
			collect_term_dependencies_at_depth(interface, terms, export->classifier, 0) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		if (collect_term_dependencies_at_depth(interface, terms, relation->subject, 0) != 0 ||
			collect_term_dependencies_at_depth(interface, terms, relation->classifier, 0) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id) &&
			prototype_artifact_interface_add_dependency(interface, expr->as.name.symbol_id) != 0) {
			return -1;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM &&
			prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				expr->as.external_term.name.namespace_symbol_id,
				expr->as.external_term.name.name_symbol_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_interface_build_definition_env(
	const struct prototype_artifact_interface* interface,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	struct prototype_term_definition_env* p_env
) {
	if (!interface || !definitions || !p_env ||
		interface->term_export_count > definition_capacity) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		definitions[i].name = qualified_name_make(
			export->namespace_symbol_id,
			export->name_symbol_id
		);
		definitions[i].term = export->local_term;
		definitions[i].classifier = export->classifier;
		definitions[i].transparency =
			export->transparency == PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT ?
			PROTOTYPE_TERM_DEFINITION_TRANSPARENT :
			PROTOTYPE_TERM_DEFINITION_OPAQUE;
		definitions[i].canonical_key = export->canonical_key;
	}
	p_env->definitions = definitions;
	p_env->definition_count = interface->term_export_count;
	return 0;
}

uint32_t prototype_artifact_interface_next_universe_var(
	const struct prototype_artifact_interface* interface
) {
	uint32_t next = 0;
	if (!interface) {
		return 0;
	}
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		const struct prototype_type_expr* expr = &interface->type_exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR &&
			expr->as.universe_var.level_var >= next) {
			next = expr->as.universe_var.level_var + 1;
		}
	}
	return next;
}

int prototype_artifact_interface_renumber_universe_vars(
	struct prototype_artifact_interface* interface,
	uint32_t offset
) {
	if (!interface) {
		return -1;
	}
	if (offset == 0) {
		return 0;
	}
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		struct prototype_type_expr* expr = &interface->type_exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR) {
			expr->as.universe_var.level_var += offset;
		}
	}
	return 0;
}

int prototype_artifact_interface_find_term_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_term_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id == namespace_symbol_id &&
			interface->term_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_type_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_type_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].namespace_symbol_id == namespace_symbol_id &&
			interface->type_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_constructor_export(
	const struct prototype_artifact_interface* interface,
	uint32_t type_export_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id || type_export_id >= interface->type_export_count) {
		return -1;
	}
	const struct prototype_artifact_type_export* type_export =
		&interface->type_exports[type_export_id];
	for (uint32_t i = 0; i < type_export->constructor_count; ++i) {
		uint32_t constructor_export_id = type_export->first_constructor_export + i;
		if (constructor_export_id >= interface->constructor_export_count) {
			return -1;
		}
		if (interface->constructor_exports[constructor_export_id].name_symbol_id ==
			name_symbol_id) {
			*p_export_id = constructor_export_id;
			return 0;
		}
	}
	return 1;
}

static void print_artifact_key(
	FILE* stream,
	const struct prototype_term_canonical_key* key
) {
	fprintf(
		stream,
		"%llu %u %u %u %d %d %d %d",
		(unsigned long long)key->hash,
		key->node_count,
		key->bound_binder_count,
		key->free_binder_count,
		key->has_frame_local_reference,
		key->has_type_local_reference,
		key->has_type_name_reference,
		key->has_type_universe_reference
	);
}

static void print_artifact_type_code_shape_key(
	FILE* stream,
	const struct prototype_type_code_shape_key* key
) {
	fprintf(
		stream,
		"%llu %u %u %u %u %u %d %d",
		(unsigned long long)key->hash,
		key->node_count,
		key->parameter_count,
		key->constructor_count,
		key->bound_binder_count,
		key->free_binder_count,
		key->has_local_universe_reference,
		key->has_name_reference
	);
}

static int write_artifact_type_expr(
	FILE* stream,
	const struct symbol_table* symbols,
	uint32_t expr_id,
	const struct prototype_type_expr* expr
) {
	if (!stream || !symbols || !expr) {
		return -1;
	}
	fprintf(stream, "type_expr %u %d", expr_id, expr->tag);
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			fprintf(stream, " %u", expr->as.universe.level);
			break;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			fprintf(stream, " %u", expr->as.universe_var.level_var);
			break;
		case PROTOTYPE_TYPE_EXPR_SELF:
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			fprintf(
				stream,
				" %u %s",
				expr->as.var.binder_id,
				symbol_to_string(symbols, expr->as.var.symbol_id)
			);
			break;
			case PROTOTYPE_TYPE_EXPR_NAME:
				fprintf(stream, " %s", symbol_to_string(symbols, expr->as.name.symbol_id));
				break;
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
				break;
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
				fprintf(
					stream,
					" %s %s ",
					expr->as.imported_type.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id)
			);
				print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
				break;
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				fprintf(
					stream,
					" %s %s",
					expr->as.external_term.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, expr->as.external_term.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, expr->as.external_term.name.name_symbol_id)
				);
				break;
		case PROTOTYPE_TYPE_EXPR_APP:
			fprintf(stream, " %u %u", expr->as.app.function, expr->as.app.argument);
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			fprintf(stream, " %u %u", expr->as.arrow.domain, expr->as.arrow.codomain);
			break;
		default:
			return -1;
	}
	fprintf(stream, "\n");
	return 0;
}

static int write_artifact_term(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	const struct prototype_term* term
) {
	if (!stream || !symbols || !type_declarations || !term) {
		return -1;
	}
	fprintf(stream, "term_node %u %d", term_id, term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(stream, " %u", term->as.var.binder_id);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			fprintf(
				stream,
				" %u %u",
				term->as.constructor.owner,
				term->as.constructor.constructor_id
			);
			break;
		case PROTOTYPE_TERM_APP:
			fprintf(stream, " %u %u", term->as.app.function, term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(stream, " %u %u", term->as.lambda.binder_id, term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(stream, " %u %u", term->as.pi.domain, term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(
				stream,
				" %u %u %u %u",
				term->as.match.scrutinee,
				term->as.match.first_case,
				term->as.match.case_count,
				term->as.match.frame_id
			);
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			{
				uint32_t representative_type_id;
				if (prototype_type_declaration_representation_type_id(
						type_declarations,
						term->as.type_former.representation_id,
						&representative_type_id
					) != 0) {
					return -1;
				}
				/* Artifacts carry a declaration anchor, not an artifact-local handle. */
				fprintf(stream, " %u", representative_type_id);
			}
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(
				stream,
				" %u %s %s",
				term->as.type_declaration.type_id,
				term->as.type_declaration.identity.namespace_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_declaration.identity.namespace_symbol_id
					) : "-",
				term->as.type_declaration.identity.name_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_declaration.identity.name_symbol_id
					) : "-"
			);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(
				stream,
				" %u %s %s %u %u",
				term->as.type_view.view_type_id,
				term->as.type_view.identity.namespace_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_view.identity.namespace_symbol_id
					) : "-",
				term->as.type_view.identity.name_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_view.identity.name_symbol_id
					) : "-",
				term->as.type_view.core,
				term->as.type_view.source
			);
			break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			fprintf(
				stream,
				" %u %u",
				term->as.induction_hypothesis.frame_id,
				term->as.induction_hypothesis.argument
			);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			fprintf(stream, " %u", term->as.universe_var.level_var);
			break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(stream, " %s", symbol_to_string(symbols, term->as.text_literal.text_symbol_id));
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(stream, " %" PRId64, term->as.int_literal.value);
				break;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				fprintf(
					stream,
					" %s %s",
					term->as.external_ref.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id)
				);
				break;
			case PROTOTYPE_TERM_OPERATION:
				fprintf(
					stream,
					" %s %s",
					symbol_to_string(symbols, term->as.operation.symbol_id),
				term->as.operation.type_symbol_id >= 0 ?
					symbol_to_string(symbols, term->as.operation.type_symbol_id) :
					"-"
				);
				break;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				fprintf(stream, " %u", term->as.effect_label.effects);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				fprintf(stream, " %u", term->as.effect_row_var.binder_id);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				fprintf(stream, " %u %u", term->as.effect_row_union.left,
					term->as.effect_row_union.right);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				fprintf(stream, " %u %u", term->as.effect_row_forall.binder_id,
					term->as.effect_row_forall.body);
				break;
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				fprintf(
					stream,
					" %u %u",
					term->as.computation_type.label,
					term->as.computation_type.result
				);
				break;
			case PROTOTYPE_TERM_THUNK_TYPE:
				fprintf(stream, " %u", term->as.thunk_type.computation);
				break;
			case PROTOTYPE_TERM_RETURN:
				fprintf(stream, " %u", term->as.return_term.value);
				break;
			case PROTOTYPE_TERM_THUNK:
				fprintf(stream, " %u", term->as.thunk.computation);
				break;
			case PROTOTYPE_TERM_FORCE:
				fprintf(stream, " %u", term->as.force.value);
				break;
			case PROTOTYPE_TERM_BIND:
				fprintf(stream, " %u %u", term->as.bind.computation,
					term->as.bind.continuation);
				break;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				fprintf(stream, " %u %u %u", term->as.operation_request.operation,
					term->as.operation_request.argument,
					term->as.operation_request.continuation);
				break;
			case PROTOTYPE_TERM_HANDLER:
				fprintf(stream, " %u %u %u", term->as.handler.operation,
					term->as.handler.return_clause, term->as.handler.operation_clause);
				break;
			case PROTOTYPE_TERM_HANDLE:
				fprintf(stream, " %u %u", term->as.handle.handler, term->as.handle.computation);
				break;
			case PROTOTYPE_TERM_HANDLER_TYPE:
				fprintf(stream, " %u %u %u", term->as.handler_type.operation,
					term->as.handler_type.input_computation,
					term->as.handler_type.output_computation);
				break;
			default:
				return -1;
	}
	fprintf(stream, "\n");
	return 0;
}

struct artifact_graph_marks {
	const struct prototype_type_declaration_db* type_declarations;
	const struct prototype_judgement_db* judgement;
	unsigned char* terms;
	unsigned char* cases;
	unsigned char* case_binders;
	unsigned char* frames;
	unsigned char* types;
	unsigned char* parameters;
	unsigned char* constructors;
	unsigned char* readback_field_types;
	unsigned char* type_exprs;
	unsigned char* relations;
	unsigned char* proofs;
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t frame_count;
	size_t type_count;
	size_t parameter_count;
	size_t constructor_count;
	size_t readback_field_type_count;
	size_t type_expr_count;
	size_t relation_count;
	size_t proof_count;
};

static int artifact_type_present(const struct prototype_type_declaration* type) {
	return type && type->type_index != PROTOTYPE_INVALID_ID;
}

static int artifact_parameter_present(
	const struct prototype_type_parameter_declaration* parameter
) {
	return parameter && parameter->binder_id != PROTOTYPE_INVALID_ID;
}

static int artifact_interface_parameter_present(
	const struct prototype_artifact_type_parameter_export* parameter
) {
	return parameter && parameter->binder_id != PROTOTYPE_INVALID_ID;
}

static int artifact_constructor_present(
	const struct prototype_type_constructor_declaration* constructor
) {
	return constructor && constructor->owner_type != PROTOTYPE_INVALID_ID;
}

static int artifact_field_type_present(const uint32_t* field_type) {
	return field_type && *field_type != PROTOTYPE_INVALID_ID;
}

static int artifact_type_expr_present(const struct prototype_type_expr* expr) {
	return expr && expr->tag != 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
);

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
);

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
);

static int artifact_mark_type_expr(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t expr_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (expr_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (expr_id >= marks->type_expr_count || expr_id >= type_declarations->expr_count) {
		return -1;
	}
	const struct prototype_type_expr* expr = &type_declarations->exprs[expr_id];
	if (!artifact_type_expr_present(expr)) {
		return -1;
	}
	if (marks->type_exprs[expr_id]) {
		return 0;
	}
	marks->type_exprs[expr_id] = 1;
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_NAME: {
			const struct prototype_type_declaration* local_type =
				prototype_type_declaration_lookup(
					type_declarations,
					expr->as.name.symbol_id
				);
			if (local_type) {
				return artifact_mark_type(
					marks,
					terms,
					local_type->type_index,
					depth + 1
				);
			}
			return 0;
		}
		case PROTOTYPE_TYPE_EXPR_APP:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.app.function,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.app.argument,
					depth + 1
				) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.arrow.domain,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.arrow.codomain,
					depth + 1
				) == 0 ? 0 : -1;
		default:
			return 0;
	}
}

static int artifact_mark_parameter(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t parameter_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		parameter_id >= marks->parameter_count ||
		parameter_id >= type_declarations->parameter_count) {
		return -1;
	}
	const struct prototype_type_parameter_declaration* parameter =
		&type_declarations->parameter_declarations[parameter_id];
	if (!artifact_parameter_present(parameter)) {
		return -1;
	}
	if (marks->parameters[parameter_id]) {
		return 0;
	}
	marks->parameters[parameter_id] = 1;
	return artifact_mark_type_expr(marks, terms, parameter->type_expr, depth + 1);
}

static int artifact_mark_field_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t field_type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		field_type_id >= marks->readback_field_type_count ||
		field_type_id >= type_declarations->readback_field_type_count) {
		return -1;
	}
	const uint32_t* field_type = &type_declarations->readback_field_types[field_type_id];
	if (!artifact_field_type_present(field_type)) {
		return -1;
	}
	if (marks->readback_field_types[field_type_id]) {
		return 0;
	}
	marks->readback_field_types[field_type_id] = 1;
	return artifact_mark_type_expr(marks, terms, *field_type, depth + 1);
}

static int artifact_mark_constructor(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t constructor_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		constructor_id >= marks->constructor_count ||
		constructor_id >= type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[constructor_id];
	if (!artifact_constructor_present(constructor)) {
		return -1;
	}
	if (marks->constructors[constructor_id]) {
		return 0;
	}
	marks->constructors[constructor_id] = 1;
	if (artifact_mark_type(marks, terms, constructor->owner_type, depth + 1) != 0) {
		return -1;
	}
	if (constructor->readback.result_type != PROTOTYPE_INVALID_ID &&
		artifact_mark_type_expr(
			marks, terms, constructor->readback.result_type, depth + 1
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < constructor->readback.field_count; ++i) {
		uint32_t field_id = constructor->readback.first_field_type + i;
		if (constructor->readback.first_field_type == PROTOTYPE_INVALID_ID ||
			field_id >= type_declarations->readback_field_type_count ||
			!artifact_field_type_present(
				&type_declarations->readback_field_types[field_id]
			)) {
			continue;
		}
		if (artifact_mark_field_type(
				marks,
				terms,
				field_id,
				depth + 1
			) != 0) {
			return -1;
		}
	}
	if (constructor->classifier_family != PROTOTYPE_INVALID_ID &&
		(artifact_mark_term(marks, terms, constructor->classifier_family, depth + 1) != 0 ||
			artifact_mark_subject_relations(
				marks,
				terms,
				marks->judgement,
				constructor->classifier_family
			) != 0)) {
		return -1;
	}
	return 0;
}

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (type_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (type_id >= marks->type_count || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (!artifact_type_present(type)) {
		return -1;
	}
	if (marks->types[type_id]) {
		return 0;
	}
	marks->types[type_id] = 1;
	if (type->formation_classifier == PROTOTYPE_INVALID_ID ||
		artifact_mark_term(
			marks, terms, type->formation_classifier, depth + 1
		) != 0 ||
		artifact_mark_subject_relations(
			marks,
			terms,
			marks->judgement,
			type->formation_classifier
		) != 0) {
		return -1;
	}
	if (type->first_parameter + type->parameter_count > marks->parameter_count ||
		type->first_constructor + type->constructor_count > marks->constructor_count) {
		return -1;
	}
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		if (artifact_mark_parameter(marks, terms, type->first_parameter + i, depth + 1) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		if (artifact_mark_constructor(
				marks,
				terms,
				type->first_constructor + i,
				depth + 1
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_case(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t case_id,
	uint32_t depth
) {
	if (!marks || !terms || case_id >= marks->case_count ||
		case_id >= terms->case_count || depth > 512) {
		return -1;
	}
	if (marks->cases[case_id]) {
		return 0;
	}
	marks->cases[case_id] = 1;
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->constructor_owner, depth + 1) != 0) {
		return -1;
	}
	if (match_case->body != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->body, depth + 1) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		uint32_t binder_id = match_case->first_binder + i;
		if (binder_id >= marks->case_binder_count) {
			return -1;
		}
		marks->case_binders[binder_id] = 1;
	}
	return 0;
}

static int artifact_mark_frame(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t frame_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		return -1;
	}
	if (frame_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (frame_id >= marks->frame_count || frame_id >= terms->match_frame_count) {
		return -1;
	}
	if (marks->frames[frame_id]) {
		return 0;
	}
	marks->frames[frame_id] = 1;
	if (terms->match_frames[frame_id].match_term != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(
			marks,
			terms,
			terms->match_frames[frame_id].match_term,
			depth + 1
		) != 0) {
		return -1;
	}
	return 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		return -1;
	}
	if (term_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (term_id >= marks->term_count || term_id >= terms->term_count) {
		return -1;
	}
	if (marks->terms[term_id]) {
		return 0;
	}
	marks->terms[term_id] = 1;
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_mark_term(marks, terms, term->as.constructor.owner, depth + 1);
		case PROTOTYPE_TERM_APP:
			return artifact_mark_term(marks, terms, term->as.app.function, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.app.argument, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_mark_term(marks, terms, term->as.lambda.body, depth + 1);
		case PROTOTYPE_TERM_PI:
			return artifact_mark_term(marks, terms, term->as.pi.domain, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.pi.codomain_family, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			if (artifact_mark_term(marks, terms, term->as.match.scrutinee, depth + 1) != 0 ||
				artifact_mark_frame(marks, terms, term->as.match.frame_id, depth + 1) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				if (artifact_mark_case(
						marks,
						terms,
						term->as.match.first_case + i,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			{
				uint32_t representative_type_id;
				if (prototype_type_declaration_representation_type_id(
						marks->type_declarations,
						term->as.type_former.representation_id,
						&representative_type_id
					) != 0) {
					return -1;
				}
				return artifact_mark_type(marks, terms, representative_type_id, depth + 1);
			}
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return artifact_mark_type(marks, terms, term->as.type_declaration.type_id, depth + 1);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return artifact_mark_type(marks, terms, term->as.type_view.view_type_id, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.type_view.core, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.type_view.source, depth + 1) == 0 ? 0 : -1;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_mark_frame(marks, terms, term->as.induction_hypothesis.frame_id, depth + 1) == 0 &&
					artifact_mark_term(marks, terms, term->as.induction_hypothesis.argument, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return artifact_mark_term(marks, terms, term->as.computation_type.label, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.computation_type.result, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return artifact_mark_term(marks, terms, term->as.effect_row_union.left, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.effect_row_union.right, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_mark_term(marks, terms, term->as.effect_row_forall.body, depth + 1);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_mark_term(marks, terms, term->as.thunk_type.computation, depth + 1);
		case PROTOTYPE_TERM_RETURN:
			return artifact_mark_term(marks, terms, term->as.return_term.value, depth + 1);
		case PROTOTYPE_TERM_THUNK:
			return artifact_mark_term(marks, terms, term->as.thunk.computation, depth + 1);
		case PROTOTYPE_TERM_FORCE:
			return artifact_mark_term(marks, terms, term->as.force.value, depth + 1);
		case PROTOTYPE_TERM_BIND:
			return artifact_mark_term(marks, terms, term->as.bind.computation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.bind.continuation, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return artifact_mark_term(marks, terms, term->as.operation_request.operation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.argument, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.continuation, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_HANDLER:
			return artifact_mark_term(marks, terms, term->as.handler.operation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.handler.return_clause, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.handler.operation_clause, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_HANDLE:
			return artifact_mark_term(marks, terms, term->as.handle.handler, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.handle.computation, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_HANDLER_TYPE:
			return artifact_mark_term(marks, terms, term->as.handler_type.operation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.handler_type.input_computation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.handler_type.output_computation, depth + 1) == 0 ? 0 : -1;
			default:
				return 0;
	}
}

static int artifact_mark_relation(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t relation_id,
	uint32_t depth
);

/* A solved motive proof validates each source branch against its synthesized
 * motive case. Those classifier facts are semantic dependencies even though
 * the proof has no fixed premise list: the motive can have a variable number
 * of cases. Keep every branch-body derivation in an artifact slice so the
 * readback validator sees the same evidence as the source graph. */
static int artifact_mark_solved_motive_branch_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t match_term,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH || depth > 512) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	for (uint32_t case_index = 0; case_index < match->as.match.case_count;
		++case_index) {
		uint32_t case_id = match->as.match.first_case + case_index;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t body = terms->cases[case_id].body;
		int found = 0;
		for (uint32_t relation_id = 0;
			relation_id < (uint32_t)judgement->relation_count;
			++relation_id) {
			const struct prototype_judgement_relation* relation =
				&judgement->relations[relation_id];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN ||
				relation->subject != body) {
				continue;
			}
			if (artifact_mark_relation(
					marks, terms, judgement, relation_id, depth + 1
				) != 0) {
				return -1;
			}
			found = 1;
		}
		if (!found) {
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_relation_for_proof(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t proof_id,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || proof_id >= marks->proof_count ||
		proof_id >= judgement->proof_count || depth > 512) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_UNKNOWN &&
			relation->proof_id == proof_id) {
			return artifact_mark_relation(marks, terms, judgement, i, depth + 1);
		}
	}
	return -1;
}

static int artifact_mark_proof(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t proof_id,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || proof_id >= marks->proof_count ||
		proof_id >= judgement->proof_count || depth > 512) {
		return -1;
	}
	const struct prototype_judgement_proof* proof = &judgement->proofs[proof_id];
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
		return -1;
	}
	if (marks->proofs[proof_id]) {
		return 0;
	}
	marks->proofs[proof_id] = 1;
	if (artifact_mark_term(marks, terms, proof->conclusion_subject, depth + 1) != 0 ||
		artifact_mark_term(marks, terms, proof->conclusion_classifier, depth + 1) != 0) {
		return -1;
	}
	if (proof->context_subject != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, proof->context_subject, depth + 1) != 0) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE &&
		artifact_mark_solved_motive_branch_relations(
			marks, terms, judgement, proof->conclusion_subject, depth + 1
		) != 0) {
		return -1;
	}
	if (artifact_mark_relation_for_proof(marks, terms, judgement, proof_id, depth + 1) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (artifact_mark_term(marks, terms, proof->premise_subjects[i], depth + 1) != 0 ||
			artifact_mark_term(marks, terms, proof->premise_classifiers[i], depth + 1) != 0 ||
			artifact_mark_proof(marks, terms, judgement, proof->premise_proof_ids[i], depth + 1) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_relation(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t relation_id,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || relation_id >= marks->relation_count ||
		relation_id >= judgement->relation_count || depth > 512) {
		return -1;
	}
	const struct prototype_judgement_relation* relation = &judgement->relations[relation_id];
	if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
		return -1;
	}
	if (marks->relations[relation_id]) {
		return 0;
	}
	marks->relations[relation_id] = 1;
	if (artifact_mark_term(marks, terms, relation->subject, depth + 1) != 0 ||
		artifact_mark_term(marks, terms, relation->classifier, depth + 1) != 0) {
		return -1;
	}
	if (relation->proof_id >= judgement->proof_count ||
		artifact_mark_proof(marks, terms, judgement, relation->proof_id, depth + 1) != 0) {
		return -1;
	}
	return 0;
}

static int artifact_mark_exact_relation(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t classifier
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			relation->classifier == classifier) {
			return artifact_mark_relation(marks, terms, judgement, i, 0);
		}
	}
	return 0;
}

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_UNKNOWN &&
			relation->subject == subject &&
			artifact_mark_relation(marks, terms, judgement, i, 0) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_term_present(const struct prototype_term* term) {
	return term && term->tag != 0;
}

static int artifact_case_present(const struct prototype_match_case* match_case) {
	return match_case && match_case->body != PROTOTYPE_INVALID_ID;
}

static int artifact_case_binder_present(const struct prototype_case_binder* binder) {
	return binder && binder->binder_id != PROTOTYPE_INVALID_ID;
}

static int artifact_frame_present(const struct prototype_match_frame* frame) {
	return frame && frame->match_term != PROTOTYPE_INVALID_ID;
}

static int artifact_relation_present(const struct prototype_judgement_relation* relation) {
	return relation && relation->kind != PROTOTYPE_JUDGEMENT_KIND_UNKNOWN;
}

static int artifact_proof_present(const struct prototype_judgement_proof* proof) {
	return proof && proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID;
}

static int artifact_universe_node_present(const struct prototype_universe_node* node) {
	return node && node->tag != 0;
}

static int artifact_universe_edge_present(const struct prototype_universe_edge* edge) {
	return edge && edge->tag != 0;
}

static int write_artifact_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !terms || !type_declarations || !judgement) {
		return -1;
	}
	fprintf(stream, "SECTION graph\n");
	size_t present_term_count = 0;
	size_t present_case_count = 0;
	size_t present_case_binder_count = 0;
	size_t present_frame_count = 0;
	size_t present_type_count = 0;
	size_t present_parameter_count = 0;
	size_t present_constructor_count = 0;
	size_t present_field_type_count = 0;
	size_t present_type_expr_count = 0;
	size_t present_relation_count = 0;
	size_t present_proof_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (artifact_term_present(&terms->terms[i])) {
			present_term_count++;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (artifact_case_present(&terms->cases[i])) {
			present_case_count++;
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (artifact_case_binder_present(&terms->case_binders[i])) {
			present_case_binder_count++;
		}
	}
	for (size_t i = 0; i < terms->match_frame_count; ++i) {
		if (artifact_frame_present(&terms->match_frames[i])) {
			present_frame_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		if (artifact_type_present(&type_declarations->type_declarations[i])) {
			present_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		if (artifact_parameter_present(&type_declarations->parameter_declarations[i])) {
			present_parameter_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		if (artifact_constructor_present(&type_declarations->constructor_declarations[i])) {
			present_constructor_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			present_field_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (artifact_type_expr_present(&type_declarations->exprs[i])) {
			present_type_expr_count++;
		}
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		if (artifact_relation_present(&judgement->relations[i])) {
			present_relation_count++;
		}
	}
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		if (artifact_proof_present(&judgement->proofs[i])) {
			present_proof_count++;
		}
	}
	fprintf(
		stream,
		"counts term_slots %zu terms %zu case_slots %zu cases %zu case_binder_slots %zu case_binders %zu frame_slots %zu frames %zu type_slots %zu types %zu parameter_slots %zu parameters %zu constructor_slots %zu constructors %zu field_type_slots %zu field_types %zu type_expr_slots %zu type_exprs %zu judgement_slots %zu judgements %zu proof_slots %zu proofs %zu\n",
		terms->term_count,
		present_term_count,
		terms->case_count,
		present_case_count,
		terms->case_binder_count,
		present_case_binder_count,
		terms->match_frame_count,
		present_frame_count,
		type_declarations->type_count,
		present_type_count,
		type_declarations->parameter_count,
		present_parameter_count,
		type_declarations->constructor_count,
		present_constructor_count,
		type_declarations->readback_field_type_count,
		present_field_type_count,
		type_declarations->expr_count,
		present_type_expr_count,
		judgement->relation_count,
		present_relation_count,
		judgement->proof_count,
		present_proof_count
	);

	fprintf(stream, "type_declarations %zu\n", present_type_count);
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		fprintf(
			stream,
			"type_decl %zu %s %s %u %u %u %u %u %u\n",
			i,
			symbol_to_string(symbols, type->name_symbol_id),
			type->namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, type->namespace_symbol_id) : "-",
			type->type_index,
			type->first_parameter,
			type->parameter_count,
			type->first_constructor,
			type->constructor_count,
			type->formation_classifier
		);
	}

	fprintf(stream, "type_parameters %zu\n", present_parameter_count);
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[i];
		if (!artifact_parameter_present(parameter)) {
			continue;
		}
		fprintf(
			stream,
			"type_param %zu %u %s %u\n",
			i,
			parameter->binder_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}

	fprintf(stream, "type_constructors %zu\n", present_constructor_count);
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		fprintf(
			stream,
			"type_constructor %zu %s %u %u %u %u %u %u\n",
			i,
			symbol_to_string(symbols, constructor->name_symbol_id),
				constructor->owner_type,
				constructor->constructor_index,
				constructor->readback.first_field_type,
				constructor->readback.field_count,
				constructor->readback.result_type,
				constructor->classifier_family
			);
	}

	fprintf(stream, "type_field_refs %zu\n", present_field_type_count);
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (!artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			continue;
		}
		fprintf(stream, "type_field_ref %zu %u\n", i, type_declarations->readback_field_types[i]);
	}

	fprintf(stream, "type_exprs %zu\n", present_type_expr_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (!artifact_type_expr_present(&type_declarations->exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&type_declarations->exprs[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "terms %zu\n", present_term_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (write_artifact_term(
				stream,
				symbols,
				type_declarations,
				(uint32_t)i,
				&terms->terms[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "match_cases %zu\n", present_case_count);
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		if (!artifact_case_present(match_case)) {
			continue;
		}
		fprintf(
			stream,
			"match_case %zu %s %u %u %u %u %u\n",
			i,
			symbol_to_string(symbols, terms->case_label_symbols[i]),
			match_case->constructor_owner,
			match_case->constructor_id,
			match_case->first_binder,
			match_case->binder_count,
			match_case->body
		);
	}

	fprintf(stream, "case_binders %zu\n", present_case_binder_count);
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (!artifact_case_binder_present(&terms->case_binders[i])) {
			continue;
		}
		fprintf(
			stream,
			"case_binder %zu %u %d\n",
			i,
			terms->case_binders[i].binder_id,
			terms->case_binders[i].is_recursive
		);
	}

	fprintf(stream, "match_frames %zu\n", present_frame_count);
	for (size_t i = 0; i < terms->match_frame_count; ++i) {
		const struct prototype_match_frame* frame = &terms->match_frames[i];
		if (!artifact_frame_present(frame)) {
			continue;
		}
		fprintf(stream, "match_frame %zu %u %u %d ", i, frame->match_term, frame->key.case_count, frame->key.is_linkable);
		print_artifact_key(stream, &frame->key.match_key);
		fprintf(stream, "\n");
	}

	fprintf(stream, "judgements %zu\n", present_relation_count);
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (!artifact_relation_present(relation)) {
			continue;
		}
		fprintf(
			stream,
			"judgement %zu %d %u %u %d %u\n",
			i,
			relation->kind,
			relation->subject,
			relation->classifier,
			relation->proof_kind,
			relation->proof_id
		);
	}
	fprintf(stream, "proofs %zu\n", present_proof_count);
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		const struct prototype_judgement_proof* proof = &judgement->proofs[i];
		if (!artifact_proof_present(proof)) {
			continue;
		}
		fprintf(
			stream,
			"proof %zu %d %d %u %u %d %u %u %u %u",
			i,
			proof->proof_kind,
			proof->conclusion_kind,
			proof->conclusion_subject,
			proof->conclusion_classifier,
			proof->context_kind,
			proof->context_subject,
			proof->context_index,
			proof->context_aux,
			proof->premise_count
		);
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			fprintf(
				stream,
				" %d %u %u %u",
				proof->premise_kinds[j],
				proof->premise_subjects[j],
				proof->premise_classifiers[j],
				proof->premise_proof_ids[j]
			);
		}
		fprintf(stream, "\n");
	}
	fprintf(stream, "END graph\n");
	return 0;
}

static int artifact_term_reaches_term_at_depth(
	const struct prototype_term_db* terms,
	uint32_t root,
	uint32_t needle,
	uint32_t depth
) {
	if (!terms || root >= terms->term_count || needle >= terms->term_count ||
		depth > 256) {
		return -1;
	}
	if (root == needle) {
		return 1;
	}
	const struct prototype_term* term = &terms->terms[root];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.constructor.owner,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_APP: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.function,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.argument,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.lambda.body,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_PI: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.domain,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.codomain_family,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.match.scrutinee,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count) {
					return -1;
				}
				found = artifact_term_reaches_term_at_depth(
					terms,
					terms->cases[case_id].body,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW:
			{
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.core,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.source,
					needle,
					depth + 1
				);
			}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.induction_hypothesis.argument,
					needle,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.computation_type.label,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.computation_type.result,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.left, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.right, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_forall.body, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk_type.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.return_term.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.force.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_BIND: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.bind.computation, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.bind.continuation, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.operation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.argument, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.continuation, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_HANDLER: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.handler.operation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.handler.return_clause, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.handler.operation_clause, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_HANDLE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.handle.handler, needle, depth + 1
			) != 0 ? 1 : artifact_term_reaches_term_at_depth(
				terms, term->as.handle.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_HANDLER_TYPE: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.handler_type.operation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.handler_type.input_computation, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.handler_type.output_computation, needle, depth + 1
			);
		}
			default:
				return 0;
	}
}

static int artifact_external_term_ref_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t term_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		term_id >= terms->term_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		int found = 0;
		if (export->local_term < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->local_term, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
		if (export->classifier < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->classifier, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		int found = artifact_term_reaches_term_at_depth(terms, relation->subject, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
		found = artifact_term_reaches_term_at_depth(terms, relation->classifier, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (!artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (type_declarations->exprs[i].tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(
				type_declarations,
				type_declarations->exprs[i].as.name.symbol_id
			)) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (type_declarations->exprs[i].tag == PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_match_case_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t case_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		case_id >= terms->case_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_MATCH ||
			case_id < term->as.match.first_case ||
			case_id >= term->as.match.first_case + term->as.match.case_count) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_resolved_constructor_owner_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			terms->terms[i].as.constructor.owner != PROTOTYPE_INVALID_ID) {
			if (artifact_external_term_ref_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		int reachable = 0;
		if (terms->cases[i].constructor_owner != PROTOTYPE_INVALID_ID) {
			if (artifact_match_case_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	return 0;
}

static int write_artifact_relocation_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t external_term_ref_count;
	size_t resolved_external_term_ref_count;
	size_t external_type_expr_ref_count;
	size_t resolved_external_type_expr_ref_count;
	size_t resolved_constructor_owner_ref_count;
	if (artifact_count_external_term_refs(
			interface,
			terms,
			judgement,
			&external_term_ref_count
		) != 0 ||
		artifact_count_resolved_external_term_refs(
			interface,
			terms,
			judgement,
			&resolved_external_term_ref_count
		) != 0 ||
		artifact_count_external_type_expr_refs(type_declarations, &external_type_expr_ref_count) != 0 ||
		artifact_count_resolved_external_type_expr_refs(
			type_declarations,
			&resolved_external_type_expr_ref_count
		) != 0 ||
		artifact_count_resolved_constructor_owner_refs(
			interface,
			terms,
			judgement,
			&resolved_constructor_owner_ref_count
		) != 0) {
		return -1;
	}

	fprintf(stream, "SECTION relocation\n");
	fprintf(stream, "external_term_refs %zu\n", external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, term->as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"external_term_ref %zu %s %s\n",
			i,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "resolved_external_term_refs %zu\n", resolved_external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		uint32_t export_id;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (prototype_artifact_interface_find_term_export_in_namespace(
					interface,
					term->as.external_ref.name.namespace_symbol_id,
					term->as.external_ref.name.name_symbol_id,
				&export_id
			) != 0) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_external_term_ref %zu %u %s %s\n",
			i,
			export_id,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "external_type_expr_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_expr_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}

	fprintf(stream, "resolved_external_type_expr_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_expr_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
		fprintf(stream, "\n");
	}

	fprintf(stream, "external_type_former_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_former_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}
	fprintf(stream, "resolved_external_type_former_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_former_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
		fprintf(stream, "\n");
	}
	fprintf(stream, "resolved_constructor_owner_refs %zu\n", resolved_constructor_owner_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR ||
			term->as.constructor.owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				term->as.constructor.owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 1 %zu %u %u ",
			i,
			term->as.constructor.owner,
			term->as.constructor.constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_match_case_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				match_case->constructor_owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 2 %zu %u %u ",
			i,
			match_case->constructor_owner,
			match_case->constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	fprintf(stream, "external_constructor_owner_refs 0\n");
	fprintf(stream, "END relocation\n");
	return 0;
}

static int write_artifact_universe_section(
	FILE* stream,
	const struct prototype_universe_db* universe
) {
	if (!stream || !universe) {
		return -1;
	}
	fprintf(stream, "SECTION universe\n");
	size_t present_node_count = 0;
	size_t present_edge_count = 0;
	for (size_t i = 0; i < universe->node_count; ++i) {
		if (artifact_universe_node_present(&universe->nodes[i])) {
			present_node_count++;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		if (artifact_universe_edge_present(&universe->edges[i])) {
			present_edge_count++;
		}
	}
	fprintf(
		stream,
		"counts node_slots %zu nodes %zu edge_slots %zu edges %zu levels %zu constraints %zu solved %d\n",
		universe->node_count,
		present_node_count,
		universe->edge_count,
		present_edge_count,
		universe->level_count,
		universe->constraint_count,
		universe->solved
	);
	fprintf(stream, "universe_nodes %zu\n", present_node_count);
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (!artifact_universe_node_present(node)) {
			continue;
		}
		fprintf(
			stream,
			"universe_node %zu %d %u %u %d %u\n",
			i,
			node->tag,
			node->type_id,
			node->parameter_id,
			node->symbol_id,
			node->type_expr
		);
	}
	fprintf(stream, "universe_edges %zu\n", present_edge_count);
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (!artifact_universe_edge_present(edge)) {
			continue;
		}
		fprintf(
			stream,
			"universe_edge %zu %d %u %u\n",
			i,
			edge->tag,
			edge->from_node,
			edge->to_node
		);
	}
	fprintf(stream, "universe_levels %zu\n", universe->level_count);
	for (size_t i = 0; i < universe->level_count; ++i) {
		const struct prototype_universe_level* level = &universe->levels[i];
		fprintf(
			stream,
			"universe_level %zu %u %d\n",
			i,
			level->level_var,
			level->value
		);
	}
	fprintf(stream, "universe_constraints %zu\n", universe->constraint_count);
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		fprintf(
			stream,
			"universe_constraint %zu %u %u %d %u %u %d\n",
			i,
			constraint->lower_level_var,
			constraint->upper_level_var,
			constraint->offset,
			constraint->subject,
			constraint->classifier,
			constraint->reason_kind
		);
	}
	fprintf(stream, "END universe\n");
	return 0;
}

static int write_artifact_debug_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_ast_db* asts
) {
	if (!stream || !symbols || !interface) {
		return -1;
	}
	fprintf(stream, "SECTION debug\n");
	fprintf(stream, "term_names %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		uint32_t source_entry_id = PROTOTYPE_INVALID_ID;
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->assignment_count; ++j) {
				const struct prototype_ast_term_assignment_def* assignment =
					&asts->assignments[j];
				if (assignment->name_symbol_id != export->name_symbol_id ||
					assignment->compiled_term != export->local_term) {
					continue;
				}
				source_entry_id = assignment->source_entry_id;
				name_span = assignment->name_span;
				body_span = assignment->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"term_name %s %u %u %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_term,
			export->classifier,
			source_entry_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "type_names %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != export->local_type_id) {
					continue;
				}
				name_span = type->name_span;
				body_span = type->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"type_name %s %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_type_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "constructor_names %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		struct prototype_source_span name_span;
		memset(&name_span, 0, sizeof(name_span));
		if (asts && export->type_export_index < interface->type_export_count) {
			const struct prototype_artifact_type_export* type_export =
				&interface->type_exports[export->type_export_index];
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != type_export->local_type_id ||
					export->ordinal >= type->constructor_count) {
					continue;
				}
				uint32_t constructor_id = type->first_constructor + export->ordinal;
				if (constructor_id >= asts->type_constructor_count) {
					continue;
				}
				name_span = asts->type_constructors[constructor_id].name_span;
				break;
			}
		}
		fprintf(
			stream,
			"constructor_name %u %s %u %u %u\n",
			export->type_export_index,
			symbol_to_string(symbols, export->name_symbol_id),
			export->ordinal,
			name_span.line,
			name_span.column
		);
	}
	fprintf(stream, "END debug\n");
	return 0;
}

struct artifact_sparse_graph {
	struct prototype_term_db terms;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement;
	struct prototype_universe_db universe;
	struct prototype_term* term_nodes;
	struct prototype_match_case* cases;
	int* case_label_symbols;
	struct prototype_case_binder* case_binders;
	struct prototype_match_frame* frames;
	struct prototype_type_declaration* type_nodes;
	struct prototype_type_parameter_declaration* parameter_declarations;
	struct prototype_type_constructor_declaration* constructor_declarations;
	uint32_t* readback_field_types;
	struct prototype_type_expr* type_exprs;
	struct prototype_judgement_relation* relations;
	struct prototype_judgement_proof* proofs;
	struct prototype_universe_node* universe_nodes;
	struct prototype_universe_edge* universe_edges;
	struct prototype_universe_level* universe_levels;
	struct prototype_universe_constraint* universe_constraints;
};

static void artifact_sparse_graph_free(struct artifact_sparse_graph* graph) {
	if (!graph) {
		return;
	}
	free(graph->term_nodes);
	free(graph->cases);
	free(graph->case_label_symbols);
	free(graph->case_binders);
	free(graph->frames);
	free(graph->type_nodes);
	free(graph->parameter_declarations);
	free(graph->constructor_declarations);
	free(graph->readback_field_types);
	free(graph->type_exprs);
	free(graph->type_declarations.representations);
	free(graph->relations);
	free(graph->proofs);
	free(graph->universe_nodes);
	free(graph->universe_edges);
	free(graph->universe_levels);
	free(graph->universe_constraints);
	memset(graph, 0, sizeof(*graph));
}

static int artifact_alloc_bytes(void** p_ret, size_t count, size_t size) {
	if (!p_ret || size == 0) {
		return -1;
	}
	*p_ret = NULL;
	if (count == 0) {
		return 0;
	}
	*p_ret = calloc(count, size);
	return *p_ret ? 0 : -1;
}

static void artifact_init_sparse_defaults(struct artifact_sparse_graph* graph) {
	for (size_t i = 0; i < graph->terms.case_count; ++i) {
		graph->terms.cases[i].constructor_owner = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].first_binder = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].body = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.case_binder_count; ++i) {
		graph->terms.case_binders[i].binder_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.match_frame_count; ++i) {
		graph->terms.match_frames[i].match_term = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.type_count; ++i) {
		graph->type_declarations.type_declarations[i].name_symbol_id = -1;
		graph->type_declarations.type_declarations[i].type_index = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].representation_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].first_parameter = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].first_constructor = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.parameter_count; ++i) {
		graph->type_declarations.parameter_declarations[i].binder_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.parameter_declarations[i].name_symbol_id = -1;
		graph->type_declarations.parameter_declarations[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.constructor_count; ++i) {
		graph->type_declarations.constructor_declarations[i].name_symbol_id = -1;
		graph->type_declarations.constructor_declarations[i].owner_type = PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].readback.first_field_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].readback.result_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].classifier_family =
			PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.readback_field_type_count; ++i) {
		graph->type_declarations.readback_field_types[i] = PROTOTYPE_INVALID_ID;
	}
}

static void artifact_marks_free(struct artifact_graph_marks* marks) {
	if (!marks) {
		return;
	}
	free(marks->terms);
	free(marks->cases);
	free(marks->case_binders);
	free(marks->frames);
	free(marks->types);
	free(marks->parameters);
	free(marks->constructors);
	free(marks->readback_field_types);
	free(marks->type_exprs);
	free(marks->relations);
	free(marks->proofs);
	memset(marks, 0, sizeof(*marks));
}

static int artifact_marks_init(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!marks || !terms || !type_declarations || !judgement) {
		return -1;
	}
	memset(marks, 0, sizeof(*marks));
	marks->type_declarations = type_declarations;
	marks->judgement = judgement;
	marks->term_count = terms->term_count;
	marks->case_count = terms->case_count;
	marks->case_binder_count = terms->case_binder_count;
	marks->frame_count = terms->match_frame_count;
	marks->type_count = type_declarations->type_count;
	marks->parameter_count = type_declarations->parameter_count;
	marks->constructor_count = type_declarations->constructor_count;
	marks->readback_field_type_count = type_declarations->readback_field_type_count;
	marks->type_expr_count = type_declarations->expr_count;
	marks->relation_count = judgement->relation_count;
	marks->proof_count = judgement->proof_count;
	if (artifact_alloc_bytes((void**)&marks->terms, marks->term_count, sizeof(*marks->terms)) != 0 ||
		artifact_alloc_bytes((void**)&marks->cases, marks->case_count, sizeof(*marks->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->case_binders,
			marks->case_binder_count,
			sizeof(*marks->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&marks->frames, marks->frame_count, sizeof(*marks->frames)) != 0 ||
		artifact_alloc_bytes((void**)&marks->types, marks->type_count, sizeof(*marks->types)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->parameters,
			marks->parameter_count,
			sizeof(*marks->parameters)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->constructors,
			marks->constructor_count,
			sizeof(*marks->constructors)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->readback_field_types,
			marks->readback_field_type_count,
			sizeof(*marks->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->type_exprs,
			marks->type_expr_count,
			sizeof(*marks->type_exprs)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->relations,
			marks->relation_count,
			sizeof(*marks->relations)
		) != 0 ||
		artifact_alloc_bytes((void**)&marks->proofs, marks->proof_count, sizeof(*marks->proofs)) != 0) {
		artifact_marks_free(marks);
		return -1;
	}
	return 0;
}

static int artifact_mark_roots(
	struct artifact_graph_marks* marks,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata
) {
	if (!marks || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (artifact_mark_term(marks, terms, export->local_term, 0) != 0 ||
			artifact_mark_subject_relations(marks, terms, judgement, export->local_term) != 0) {
			return -1;
		}
		if (export->classifier != PROTOTYPE_INVALID_ID &&
			(artifact_mark_term(marks, terms, export->classifier, 0) != 0 ||
				artifact_mark_exact_relation(
					marks,
					terms,
					judgement,
					export->local_term,
					export->classifier
				) != 0 ||
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->classifier
				) != 0)) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		if (artifact_mark_type(marks, terms, export->local_type_id, 0) != 0 ||
			artifact_mark_type(marks, terms, export->core_representation_anchor_type_id, 0) != 0 ||
			export->formation_classifier == PROTOTYPE_INVALID_ID ||
			artifact_mark_term(marks, terms, export->formation_classifier, 0) != 0 ||
			artifact_mark_subject_relations(
				marks, terms, judgement, export->formation_classifier
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		if (export->classifier_family != PROTOTYPE_INVALID_ID &&
			(artifact_mark_term(marks, terms, export->classifier_family, 0) != 0 ||
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->classifier_family
				) != 0)) {
			return -1;
		}
	}
	if (metadata) {
		for (size_t i = 0; i < metadata->operation_count; ++i) {
			const struct prototype_operation_node* operation = &metadata->operations[i];
			uint32_t references[] = {
				operation->core_term,
				operation->known_classifier,
				operation->classifier,
				operation->binder_classifier
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
			}
		}
		for (size_t i = 0;
			i < prototype_verification_db_count(&metadata->verification);
			++i) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(&metadata->verification, (uint32_t)i);
			if (!obligation) {
				return -1;
			}
			uint32_t references[] = {
				obligation->core_term,
				obligation->input_classifier,
				obligation->classifier_family,
				obligation->effect_row
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

static int artifact_sparse_graph_alloc(
	struct artifact_sparse_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	size_t judgement_capacity = judgement->relation_count;
	if (judgement_capacity < judgement->proof_count) {
		judgement_capacity = judgement->proof_count;
	}
	if (artifact_alloc_bytes((void**)&graph->term_nodes, terms->term_count, sizeof(*graph->term_nodes)) != 0 ||
		artifact_alloc_bytes((void**)&graph->cases, terms->case_count, sizeof(*graph->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_label_symbols,
			terms->case_count,
			sizeof(*graph->case_label_symbols)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_binders,
			terms->case_binder_count,
			sizeof(*graph->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->frames, terms->match_frame_count, sizeof(*graph->frames)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_nodes,
			type_declarations->type_count,
			sizeof(*graph->type_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->parameter_declarations,
			type_declarations->parameter_count,
			sizeof(*graph->parameter_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->constructor_declarations,
			type_declarations->constructor_count,
			sizeof(*graph->constructor_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->readback_field_types,
			type_declarations->readback_field_type_count,
			sizeof(*graph->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_exprs,
			type_declarations->expr_count,
			sizeof(*graph->type_exprs)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->relations, judgement_capacity, sizeof(*graph->relations)) != 0 ||
		artifact_alloc_bytes((void**)&graph->proofs, judgement_capacity, sizeof(*graph->proofs)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_nodes,
			universe->node_count,
			sizeof(*graph->universe_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_edges,
			universe->edge_count,
			sizeof(*graph->universe_edges)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_levels,
			universe->level_count,
			sizeof(*graph->universe_levels)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_constraints,
			universe->constraint_count,
			sizeof(*graph->universe_constraints)
		) != 0) {
		artifact_sparse_graph_free(graph);
		return -1;
	}
	prototype_term_db_init(
		&graph->terms,
		graph->term_nodes,
		terms->term_count,
		graph->cases,
		graph->case_label_symbols,
		terms->case_count,
		graph->case_binders,
		terms->case_binder_count,
		graph->frames,
		terms->match_frame_count
	);
	prototype_type_declaration_db_init(
		&graph->type_declarations,
		graph->type_nodes,
		type_declarations->type_count,
		graph->constructor_declarations,
		type_declarations->constructor_count,
		graph->parameter_declarations,
		type_declarations->parameter_count,
		graph->readback_field_types,
		type_declarations->readback_field_type_count,
		graph->type_exprs,
		type_declarations->expr_count
	);
	prototype_judgement_db_init(
		&graph->judgement,
		graph->relations,
		graph->proofs,
		judgement_capacity
	);
	prototype_universe_db_init(
		&graph->universe,
		graph->universe_nodes,
		universe->node_count,
		graph->universe_edges,
		universe->edge_count,
		graph->universe_levels,
		universe->level_count,
		graph->universe_constraints,
		universe->constraint_count
	);
	graph->terms.term_count = terms->term_count;
	graph->terms.case_count = terms->case_count;
	graph->terms.case_binder_count = terms->case_binder_count;
	graph->terms.match_frame_count = terms->match_frame_count;
	graph->terms.next_binder_id = terms->next_binder_id;
	graph->type_declarations.type_count = type_declarations->type_count;
	graph->type_declarations.parameter_count = type_declarations->parameter_count;
	graph->type_declarations.constructor_count = type_declarations->constructor_count;
	graph->type_declarations.readback_field_type_count = type_declarations->readback_field_type_count;
	graph->type_declarations.expr_count = type_declarations->expr_count;
	graph->type_declarations.next_level_var = type_declarations->next_level_var;
	graph->judgement.relation_count = judgement->relation_count;
	graph->judgement.proof_count = judgement->proof_count;
	graph->judgement.next_universe_var = judgement->next_universe_var;
	graph->universe.node_count = universe->node_count;
	graph->universe.edge_count = universe->edge_count;
	graph->universe.level_count = universe->level_count;
	graph->universe.constraint_count = 0;
	graph->universe.solved = universe->solved;
	artifact_init_sparse_defaults(graph);
	return 0;
}

static int artifact_sparse_graph_copy_marked(
	struct artifact_sparse_graph* graph,
	const struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !marks || !terms || !judgement || !universe) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (marks->terms[i]) {
			graph->terms.terms[i] = terms->terms[i];
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (marks->cases[i]) {
			graph->terms.cases[i] = terms->cases[i];
			graph->terms.case_label_symbols[i] = terms->case_label_symbols[i];
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (marks->case_binders[i]) {
			graph->terms.case_binders[i] = terms->case_binders[i];
		}
	}
	for (size_t i = 0; i < terms->match_frame_count; ++i) {
		if (marks->frames[i]) {
			graph->terms.match_frames[i] = terms->match_frames[i];
		}
	}
	const struct prototype_type_declaration_db* type_declarations =
		marks->type_declarations;
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		if (marks->types[i]) {
			graph->type_declarations.type_declarations[i] =
				type_declarations->type_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		if (marks->parameters[i]) {
			graph->type_declarations.parameter_declarations[i] =
				type_declarations->parameter_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		if (marks->constructors[i]) {
			graph->type_declarations.constructor_declarations[i] =
				type_declarations->constructor_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (marks->readback_field_types[i]) {
			graph->type_declarations.readback_field_types[i] =
				type_declarations->readback_field_types[i];
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (marks->type_exprs[i]) {
			graph->type_declarations.exprs[i] = type_declarations->exprs[i];
		}
	}
	if (graph->type_declarations.representation_capacity <
		type_declarations->representation_count) {
		return -1;
	}
	memcpy(
		graph->type_declarations.representations,
		type_declarations->representations,
		type_declarations->representation_count * sizeof(*type_declarations->representations)
	);
	graph->type_declarations.representation_count = type_declarations->representation_count;
	graph->type_declarations.representations_dirty = type_declarations->representations_dirty;
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		if (marks->relations[i]) {
			graph->judgement.relations[i] = judgement->relations[i];
		}
	}
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		if (marks->proofs[i]) {
			graph->judgement.proofs[i] = judgement->proofs[i];
		}
	}
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (node->tag == PROTOTYPE_UNIVERSE_NODE_TYPE &&
			node->type_id < marks->type_count &&
			marks->types[node->type_id]) {
			graph->universe.nodes[i] = *node;
		} else if (node->tag == PROTOTYPE_UNIVERSE_NODE_PARAMETER &&
			node->parameter_id < marks->parameter_count &&
			marks->parameters[node->parameter_id]) {
			graph->universe.nodes[i] = *node;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (edge->from_node < graph->universe.node_count &&
			edge->to_node < graph->universe.node_count &&
			artifact_universe_node_present(&graph->universe.nodes[edge->from_node]) &&
			artifact_universe_node_present(&graph->universe.nodes[edge->to_node])) {
			graph->universe.edges[i] = *edge;
		}
	}
	for (size_t i = 0; i < universe->level_count; ++i) {
		graph->universe.levels[i] = universe->levels[i];
	}
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		if (constraint->subject < marks->term_count &&
			constraint->classifier < marks->term_count &&
			marks->terms[constraint->subject] &&
			marks->terms[constraint->classifier]) {
			graph->universe.constraints[graph->universe.constraint_count++] = *constraint;
		}
	}
	graph->universe.solved = universe->solved;
	return 0;
}

static int artifact_build_sparse_graph(
	struct artifact_sparse_graph* graph,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_compile_metadata* metadata
) {
	if (!graph || !interface || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	struct artifact_graph_marks marks;
	if (artifact_marks_init(&marks, terms, type_declarations, judgement) != 0) {
		return -1;
	}
	int status = 0;
	if (artifact_mark_roots(
			&marks, interface, terms, type_declarations, judgement, metadata
		) != 0 ||
		artifact_sparse_graph_alloc(graph, terms, type_declarations, judgement, universe) != 0 ||
		artifact_sparse_graph_copy_marked(graph, &marks, terms, judgement, universe) != 0) {
		status = -1;
	}
	artifact_marks_free(&marks);
	if (status != 0) {
		artifact_sparse_graph_free(graph);
	}
	return status;
}

static const char* artifact_optional_symbol_name(
	const struct symbol_table* symbols,
	int symbol_id
) {
	if (symbol_id < 0) {
		return "-";
	}
	return symbol_to_string(symbols, symbol_id);
}

static int write_artifact_operation_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols) {
		return -1;
	}
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph_const(metadata, &graph);
	size_t operation_count = prototype_operation_graph_count(&graph);
	size_t case_count = prototype_operation_graph_case_count(&graph);
	size_t obligation_count = metadata ?
		prototype_verification_db_count(&metadata->verification) : 0;
	fprintf(stream, "SECTION operation_graph\n");
	fprintf(
		stream,
		"compile_policy %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
		" %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
		metadata ? metadata->compile_policy : PROTOTYPE_COMPILE_POLICY_HYBRID,
		metadata ? metadata->required_runtime_capabilities : 0,
		metadata ? metadata->normalization_step_limit : 0,
		metadata ? metadata->normalization_steps_used : 0,
		metadata ? metadata->solver_step_limit : 0,
		metadata ? metadata->solver_steps_used : 0,
		metadata ? metadata->solver_exhausted : 0,
		metadata ? metadata->solver_constraint_count : 0,
		metadata ? metadata->solver_solved_count : 0,
		metadata ? metadata->solver_residual_count : 0,
		metadata ? metadata->solver_incomplete_count : 0
	);
	fprintf(stream, "operations %zu\n", operation_count);
	for (size_t i = 0; i < operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(&graph, (uint32_t)i);
		if (!operation) {
			return -1;
		}
		const char* source_name = artifact_optional_symbol_name(
			symbols, operation->source_symbol_id
		);
		const char* binder_name = artifact_optional_symbol_name(
			symbols, operation->binder_symbol_id
		);
		if (!source_name || !binder_name) {
			return -1;
		}
		fprintf(
			stream,
			"operation %zu %d %d %d %u %u %u %u %u %s %s %u %u %u %u %u %u"
			" %u %u %u %u %u %u %u %u %u\n",
			i,
			operation->tag,
			operation->polarity,
			operation->computation_kind,
			operation->core_term,
			operation->known_classifier,
			operation->classifier,
			operation->classifier_variable,
			operation->source_ast,
			source_name,
			binder_name,
			operation->referenced_ast_binder_id,
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee,
			operation->binder_classifier,
			operation->handler_argument_ast_binder_id,
			operation->handler_argument_binder_id,
			operation->handler_continuation_ast_binder_id,
			operation->handler_continuation_binder_id,
			operation->handler_return_ast_binder_id,
			operation->handler_return_binder_id,
			operation->implicit_effect_row_count,
			operation->first_case,
			operation->case_count
		);
		fprintf(stream, "operation_rows %zu", i);
		for (uint32_t j = 0; j < operation->implicit_effect_row_count; ++j) {
			fprintf(stream, " %u", operation->implicit_effect_row_binders[j]);
		}
		fprintf(stream, "\n");
	}
	fprintf(stream, "operation_cases %zu\n", case_count);
	for (size_t i = 0; i < case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(&graph, (uint32_t)i);
		if (!operation_case) {
			return -1;
		}
		const char* label = artifact_optional_symbol_name(
			symbols, operation_case->case_label_symbol_id
		);
		if (!label) {
			return -1;
		}
		fprintf(
			stream,
			"operation_case %zu %u %u %u %s\n",
			i,
			operation_case->body_operation,
			operation_case->constructor_owner,
			operation_case->constructor_id,
			label
		);
	}
	fprintf(stream, "verification_obligations %zu\n", obligation_count);
	for (size_t i = 0; i < obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(&metadata->verification, (uint32_t)i);
		if (!obligation) {
			return -1;
		}
		fprintf(
			stream,
			"verification %zu %d %d %u %u %u %u %u %u %u %u %d %u\n",
			i,
			obligation->kind,
			obligation->state,
			obligation->operation,
			obligation->core_term,
			obligation->computation_operation,
			obligation->continuation_operation,
			obligation->continuation_binder_id,
			obligation->input_classifier,
			obligation->classifier_family,
			obligation->effect_row,
			obligation->normalization_profile,
			obligation->schema_version
		);
	}
	return fprintf(stream, "END operation_graph\n") < 0 ? -1 : 0;
}

static int prototype_artifact_write_text_body(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}

	fprintf(stream, "A_PROGRAM_ARTIFACT 43\n");
	fprintf(stream, "SECTION interface\n");
	size_t present_interface_type_expr_count = 0;
	size_t present_interface_parameter_count = 0;
	size_t present_interface_field_ref_count = 0;
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (i < type_declarations->expr_count &&
			artifact_type_expr_present(&type_declarations->exprs[i])) {
			present_interface_type_expr_count++;
		}
	}
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		if (i < type_declarations->parameter_count &&
			artifact_parameter_present(&type_declarations->parameter_declarations[i])) {
			present_interface_parameter_count++;
		}
	}
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (i < type_declarations->readback_field_type_count &&
			artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			present_interface_field_ref_count++;
		}
	}
	fprintf(
		stream,
		"interface_type_exprs slots %zu type_exprs %zu\n",
		interface->type_expr_count,
		present_interface_type_expr_count
	);
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (i >= type_declarations->expr_count ||
			!artifact_type_expr_present(&type_declarations->exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&interface->type_exprs[i]
			) != 0) {
			return -1;
		}
	}
	fprintf(
		stream,
		"interface_type_parameters slots %zu parameters %zu\n",
		interface->type_parameter_count,
		present_interface_parameter_count
	);
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		const struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[i];
		if (i >= type_declarations->parameter_count ||
			!artifact_parameter_present(&type_declarations->parameter_declarations[i])) {
			continue;
		}
		fprintf(
			stream,
			"interface_type_parameter %zu %u %s %u\n",
			i,
			parameter->binder_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}
	fprintf(
		stream,
		"interface_constructor_field_refs slots %zu field_refs %zu\n",
		interface->constructor_field_type_expr_count,
		present_interface_field_ref_count
	);
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (i >= type_declarations->readback_field_type_count ||
			!artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			continue;
		}
		fprintf(
			stream,
			"interface_constructor_field_ref %zu %u\n",
			i,
			interface->constructor_field_type_exprs[i]
		);
	}
	fprintf(stream, "term_exports %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"term %s %u %u %d ",
			name,
			export->local_term,
			export->classifier,
			export->transparency
		);
		print_artifact_key(stream, &export->canonical_key);
		fprintf(stream, " ");
		print_artifact_key(stream, &export->classifier_key);
		fprintf(stream, " namespace %s\n", namespace_name);
	}

	fprintf(stream, "type_exports %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"type %s %u %u %u %u %u %u %u ",
			name,
			export->local_type_id,
			export->core_representation_anchor_type_id,
			export->formation_classifier,
			export->first_parameter,
			export->parameter_count,
			export->first_constructor_export,
			export->constructor_count
		);
		print_artifact_type_code_shape_key(stream, &export->code_shape_key);
		fprintf(stream, " namespace %s\n", namespace_name);
	}

	fprintf(stream, "constructor_exports %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"constructor %u %s %u %u %u %u\n",
			export->type_export_index,
			name,
			export->ordinal,
			export->readback_first_field_type,
			export->readback_field_count,
			export->classifier_family
		);
	}

	fprintf(stream, "dependencies %zu\n", interface->dependency_count);
	for (size_t i = 0; i < interface->dependency_count; ++i) {
		const struct prototype_artifact_dependency* dependency = &interface->dependencies[i];
		const char* name = symbol_to_string(symbols, dependency->name_symbol_id);
		const char* namespace_name = dependency->namespace_symbol_id < 0 ?
			"-" :
			symbol_to_string(symbols, dependency->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(stream, "dependency %s namespace %s\n", name, namespace_name);
	}
	fprintf(stream, "END interface\n");

	if (write_artifact_graph_section(
			stream,
			symbols,
			terms,
			type_declarations,
			judgement
		) != 0) {
		return -1;
	}
	if (write_artifact_operation_graph_section(stream, symbols, metadata) != 0) {
		return -1;
	}
	if (write_artifact_universe_section(stream, universe) != 0) {
		return -1;
	}
	if (write_artifact_debug_section(stream, symbols, interface, asts) != 0) {
		return -1;
	}
	return write_artifact_relocation_section(
		stream,
		symbols,
		interface,
		terms,
		type_declarations,
		judgement
	);
}

int prototype_artifact_write_text(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}
	struct artifact_sparse_graph sparse_graph;
	if (artifact_build_sparse_graph(
			&sparse_graph,
			interface,
			terms,
			type_declarations,
			judgement,
			universe,
			metadata
		) != 0) {
		return -1;
	}
	int status = prototype_artifact_write_text_body(
		stream,
		symbols,
		interface,
		&sparse_graph.terms,
		&sparse_graph.type_declarations,
		&sparse_graph.judgement,
		&sparse_graph.universe,
		asts,
		metadata
	);
	artifact_sparse_graph_free(&sparse_graph);
	return status;
}

static int read_artifact_term_key(
	FILE* stream,
	struct prototype_term_canonical_key* key
) {
	unsigned long long hash;
	if (fscanf(
			stream,
			"%llu %u %u %u %d %d %d %d",
			&hash,
			&key->node_count,
			&key->bound_binder_count,
			&key->free_binder_count,
			&key->has_frame_local_reference,
			&key->has_type_local_reference,
			&key->has_type_name_reference,
			&key->has_type_universe_reference
		) != 8) {
		return -1;
	}
	key->hash = (uint64_t)hash;
	return 0;
}

static int read_artifact_type_code_shape_key(
	FILE* stream,
	struct prototype_type_code_shape_key* key
) {
	unsigned long long hash;
	if (fscanf(
			stream,
			"%llu %u %u %u %u %u %d %d",
			&hash,
			&key->node_count,
			&key->parameter_count,
			&key->constructor_count,
			&key->bound_binder_count,
			&key->free_binder_count,
			&key->has_local_universe_reference,
			&key->has_name_reference
		) != 8) {
		return -1;
	}
	key->hash = (uint64_t)hash;
	return 0;
}

static int read_artifact_type_expr(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_id,
	uint32_t* p_next_level_var
);

int prototype_artifact_read_text_interface(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* interface
) {
	if (!stream || !symbols || !interface) {
		return -1;
	}

	char word[256];
	int version;
	if (fscanf(stream, "%255s %d", word, &version) != 2 ||
		strcmp(word, "A_PROGRAM_ARTIFACT") != 0 ||
		version != 43) {
		return -1;
	}
	if (fscanf(stream, "%255s", word) != 1 || strcmp(word, "SECTION") != 0 ||
		fscanf(stream, "%255s", word) != 1 || strcmp(word, "interface") != 0) {
		return -1;
	}

	interface->term_export_count = 0;
	interface->type_export_count = 0;
	interface->type_parameter_count = 0;
	interface->constructor_export_count = 0;
	interface->constructor_field_type_expr_count = 0;
	interface->type_expr_count = 0;
	interface->dependency_count = 0;

	size_t count;
	size_t slot_count;
	size_t present_count;
	char label_slots[32];
	char label_present[32];
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_type_exprs") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "type_exprs") != 0 ||
		slot_count > interface->type_expr_capacity) {
		return -1;
	}
	memset(interface->type_exprs, 0, sizeof(*interface->type_exprs) * slot_count);
	struct prototype_type_declaration_db interface_type_expr_db;
	memset(&interface_type_expr_db, 0, sizeof(interface_type_expr_db));
	interface_type_expr_db.exprs = interface->type_exprs;
	interface_type_expr_db.expr_capacity = interface->type_expr_capacity;
	interface_type_expr_db.expr_count = slot_count;
	uint32_t next_level_var = 0;
	for (size_t i = 0; i < present_count; ++i) {
		if (read_artifact_type_expr(
				stream,
				symbols,
				&interface_type_expr_db,
				PROTOTYPE_INVALID_ID,
				&next_level_var
			) != 0) {
			return -1;
		}
	}
	interface->type_expr_count = slot_count;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_type_parameters") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "parameters") != 0 ||
		slot_count > interface->type_parameter_capacity) {
		return -1;
	}
	for (size_t i = 0; i < slot_count; ++i) {
		interface->type_parameters[i].binder_id = PROTOTYPE_INVALID_ID;
		interface->type_parameters[i].name_symbol_id = -1;
		interface->type_parameters[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < present_count; ++i) {
		char name[256];
		size_t id;
		uint32_t binder_id;
		uint32_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %u",
				word,
				&id,
				&binder_id,
				name,
				&type_expr
			) != 5 ||
			strcmp(word, "interface_type_parameter") != 0 ||
			id >= slot_count ||
			type_expr >= interface->type_expr_count ||
			!artifact_type_expr_present(&interface->type_exprs[type_expr])) {
			return -1;
		}
		struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[id];
		parameter->binder_id = binder_id;
		parameter->type_expr = type_expr;
		parameter->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (parameter->name_symbol_id < 0) {
			return -1;
		}
	}
	interface->type_parameter_count = slot_count;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_constructor_field_refs") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "field_refs") != 0 ||
		slot_count > interface->constructor_field_type_expr_capacity) {
		return -1;
	}
	for (size_t i = 0; i < slot_count; ++i) {
		interface->constructor_field_type_exprs[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < present_count; ++i) {
		size_t id;
		uint32_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %u",
				word,
				&id,
				&type_expr
			) != 3 ||
			strcmp(word, "interface_constructor_field_ref") != 0 ||
			id >= slot_count ||
			type_expr >= interface->type_expr_count ||
			!artifact_type_expr_present(&interface->type_exprs[type_expr])) {
			return -1;
		}
		interface->constructor_field_type_exprs[id] = type_expr;
	}
	interface->constructor_field_type_expr_count = slot_count;

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "term_exports") != 0 ||
		count > interface->term_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		struct prototype_artifact_term_export* export =
			&interface->term_exports[interface->term_export_count++];
		if (fscanf(
				stream,
				"%255s %255s %u %u %d",
				word,
				name,
				&export->local_term,
				&export->classifier,
				&export->transparency
			) != 5 ||
			strcmp(word, "term") != 0) {
			return -1;
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0 ||
			read_artifact_term_key(stream, &export->canonical_key) != 0 ||
			read_artifact_term_key(stream, &export->classifier_key) != 0 ||
			fscanf(stream, "%255s %255s", namespace_word, namespace_name) != 2 ||
			strcmp(namespace_word, "namespace") != 0) {
			return -1;
		}
		export->namespace_symbol_id = symbol_intern(
			symbols,
			namespace_name,
			strlen(namespace_name)
		);
		if (export->namespace_symbol_id < 0) {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "type_exports") != 0 ||
		count > interface->type_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		struct prototype_artifact_type_export* export =
			&interface->type_exports[interface->type_export_count++];
		if (fscanf(
				stream,
				"%255s %255s %u %u %u %u %u %u %u",
				word,
				name,
				&export->local_type_id,
				&export->core_representation_anchor_type_id,
				&export->formation_classifier,
				&export->first_parameter,
				&export->parameter_count,
				&export->first_constructor_export,
				&export->constructor_count
			) != 9 ||
			strcmp(word, "type") != 0 ||
			export->first_parameter + export->parameter_count >
				interface->type_parameter_count) {
			return -1;
		}
		for (uint32_t j = 0; j < export->parameter_count; ++j) {
			if (!artifact_interface_parameter_present(
					&interface->type_parameters[export->first_parameter + j]
				)) {
				return -1;
			}
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0 ||
			read_artifact_type_code_shape_key(stream, &export->code_shape_key) != 0 ||
			fscanf(stream, "%255s %255s", namespace_word, namespace_name) != 2 ||
			strcmp(namespace_word, "namespace") != 0) {
			return -1;
		}
		export->namespace_symbol_id = symbol_intern(
			symbols,
			namespace_name,
			strlen(namespace_name)
		);
		if (export->namespace_symbol_id < 0) {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "constructor_exports") != 0 ||
		count > interface->constructor_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[interface->constructor_export_count++];
		if (fscanf(
				stream,
				"%255s %u %255s %u %u %u %u",
				word,
				&export->type_export_index,
				name,
				&export->ordinal,
				&export->readback_first_field_type,
				&export->readback_field_count,
				&export->classifier_family
			) != 7 ||
			strcmp(word, "constructor") != 0 ||
			(export->readback_field_count > 0 &&
				(export->readback_first_field_type == PROTOTYPE_INVALID_ID ||
					export->readback_first_field_type +
						export->readback_field_count >
						interface->constructor_field_type_expr_count))) {
			return -1;
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0) {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "dependencies") != 0 ||
		count > interface->dependency_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		struct prototype_artifact_dependency* dependency =
			&interface->dependencies[interface->dependency_count++];
		if (fscanf(
				stream,
				"%255s %255s %255s %255s",
				word,
				name,
				namespace_word,
				namespace_name
			) != 4 ||
			strcmp(word, "dependency") != 0 ||
			strcmp(namespace_word, "namespace") != 0) {
			return -1;
		}
		dependency->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (strcmp(namespace_name, "-") == 0) {
			dependency->namespace_symbol_id = -1;
		} else {
			dependency->namespace_symbol_id = symbol_intern(
				symbols,
				namespace_name,
				strlen(namespace_name)
			);
			if (dependency->namespace_symbol_id < 0) {
				return -1;
			}
		}
		if (dependency->name_symbol_id < 0) {
			return -1;
		}
	}
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "interface") != 0) {
		return -1;
	}
	return 0;
}

static int expect_artifact_count(FILE* stream, const char* expected, size_t* p_count) {
	char word[256];
	if (!stream || !expected || !p_count ||
		fscanf(stream, "%255s %zu", word, p_count) != 2 ||
		strcmp(word, expected) != 0) {
		return -1;
	}
	return 0;
}

static int read_artifact_symbol(FILE* stream, struct symbol_table* symbols, int* p_symbol_id) {
	char name[256];
	if (!stream || !symbols || !p_symbol_id ||
		fscanf(stream, "%255s", name) != 1) {
		return -1;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	*p_symbol_id = symbol_id;
	return 0;
}

static int read_artifact_optional_symbol(FILE* stream, struct symbol_table* symbols, int* p_symbol_id) {
	char name[256];
	if (!stream || !symbols || !p_symbol_id ||
		fscanf(stream, "%255s", name) != 1) {
		return -1;
	}
	if (strcmp(name, "-") == 0) {
		*p_symbol_id = -1;
		return 0;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	*p_symbol_id = symbol_id;
	return 0;
}

static int read_artifact_type_expr(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_id,
	uint32_t* p_next_level_var
) {
	char word[256];
	uint32_t expr_id;
	int tag;
	if (!stream || !symbols || !type_declarations || !p_next_level_var ||
		fscanf(stream, "%255s %u %d", word, &expr_id, &tag) != 3 ||
		strcmp(word, "type_expr") != 0 ||
		(expected_id != PROTOTYPE_INVALID_ID && expr_id != expected_id) ||
		expr_id >= type_declarations->expr_capacity) {
		return -1;
	}

	struct prototype_type_expr* expr = &type_declarations->exprs[expr_id];
	if (artifact_type_expr_present(expr)) {
		return -1;
	}
	memset(expr, 0, sizeof(*expr));
	expr->tag = tag;
	switch (tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			return fscanf(stream, "%u", &expr->as.universe.level) == 1 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			if (fscanf(stream, "%u", &expr->as.universe_var.level_var) != 1) {
				return -1;
			}
			if (*p_next_level_var <= expr->as.universe_var.level_var) {
				*p_next_level_var = expr->as.universe_var.level_var + 1;
			}
			return 0;
		case PROTOTYPE_TYPE_EXPR_SELF:
			return 0;
		case PROTOTYPE_TYPE_EXPR_VAR:
			return fscanf(stream, "%u", &expr->as.var.binder_id) == 1 &&
				read_artifact_symbol(stream, symbols, &expr->as.var.symbol_id) == 0 ? 0 : -1;
			case PROTOTYPE_TYPE_EXPR_NAME:
				return read_artifact_symbol(stream, symbols, &expr->as.name.symbol_id);
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
				if (read_artifact_optional_symbol(
						stream,
						symbols,
						&expr->as.imported_type.name.namespace_symbol_id
					) != 0 ||
					read_artifact_symbol(stream, symbols, &expr->as.imported_type.name.name_symbol_id) != 0) {
				return -1;
			}
				return read_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				return read_artifact_optional_symbol(
					stream,
					symbols,
					&expr->as.external_term.name.namespace_symbol_id
				) == 0 &&
					read_artifact_symbol(stream, symbols, &expr->as.external_term.name.name_symbol_id) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_APP:
			return fscanf(stream, "%u %u", &expr->as.app.function, &expr->as.app.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return fscanf(stream, "%u %u", &expr->as.arrow.domain, &expr->as.arrow.codomain) == 2 ? 0 : -1;
		default:
			return -1;
	}
}

static int read_artifact_term(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_term_db* terms,
	uint32_t expected_id,
	uint32_t* p_next_binder_id
) {
	char word[256];
	uint32_t term_id;
	int tag;
	if (!stream || !symbols || !terms || !p_next_binder_id ||
		fscanf(stream, "%255s %u %d", word, &term_id, &tag) != 3 ||
		strcmp(word, "term_node") != 0 ||
		(expected_id != PROTOTYPE_INVALID_ID && term_id != expected_id) ||
		term_id >= terms->term_capacity) {
		return -1;
	}

	struct prototype_term* term = &terms->terms[term_id];
	if (artifact_term_present(term)) {
		return -1;
	}
	memset(term, 0, sizeof(*term));
	term->tag = tag;
	switch (tag) {
		case PROTOTYPE_TERM_VAR:
			if (fscanf(stream, "%u", &term->as.var.binder_id) != 1) {
				return -1;
			}
			if (term->as.var.binder_id < PROTOTYPE_PI_UNUSED_BINDER_ID &&
				*p_next_binder_id <= term->as.var.binder_id) {
				*p_next_binder_id = term->as.var.binder_id + 1;
			}
			return 0;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return fscanf(stream, "%u %u", &term->as.constructor.owner, &term->as.constructor.constructor_id) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_APP:
			return fscanf(stream, "%u %u", &term->as.app.function, &term->as.app.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_LAMBDA:
			if (fscanf(stream, "%u %u", &term->as.lambda.binder_id, &term->as.lambda.body) != 2) {
				return -1;
			}
			if (term->as.lambda.binder_id < PROTOTYPE_PI_UNUSED_BINDER_ID &&
				*p_next_binder_id <= term->as.lambda.binder_id) {
				*p_next_binder_id = term->as.lambda.binder_id + 1;
			}
			return 0;
		case PROTOTYPE_TERM_PI:
			return fscanf(stream, "%u %u", &term->as.pi.domain, &term->as.pi.codomain_family) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			return fscanf(
				stream,
				"%u %u %u %u",
				&term->as.match.scrutinee,
				&term->as.match.first_case,
				&term->as.match.case_count,
				&term->as.match.frame_id
			) == 4 ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return fscanf(stream, "%u", &term->as.type_former.representation_id) == 1 ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			{
				char namespace_name[256];
				char name[256];
				if (fscanf(
						stream,
						"%u %255s %255s",
						&term->as.type_declaration.type_id,
						namespace_name,
						name
					) != 3) {
					return -1;
				}
				term->as.type_declaration.identity.namespace_symbol_id =
					strcmp(namespace_name, "-") == 0 ? -1 :
					symbol_intern(symbols, namespace_name, strlen(namespace_name));
				term->as.type_declaration.identity.name_symbol_id =
					strcmp(name, "-") == 0 ? -1 :
					symbol_intern(symbols, name, strlen(name));
				return term->as.type_declaration.identity.namespace_symbol_id >= -1 &&
					term->as.type_declaration.identity.name_symbol_id >= -1 ? 0 : -1;
			}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			char namespace_name[256];
			char name[256];
			if (fscanf(
					stream,
					"%u %255s %255s %u %u",
					&term->as.type_view.view_type_id,
					namespace_name,
					name,
					&term->as.type_view.core,
					&term->as.type_view.source
				) != 5) {
				return -1;
			}
			term->as.type_view.identity.namespace_symbol_id =
				strcmp(namespace_name, "-") == 0 ? -1 :
				symbol_intern(symbols, namespace_name, strlen(namespace_name));
			term->as.type_view.identity.name_symbol_id =
				strcmp(name, "-") == 0 ? -1 :
				symbol_intern(symbols, name, strlen(name));
			return term->as.type_view.identity.namespace_symbol_id >= -1 &&
				term->as.type_view.identity.name_symbol_id >= -1 ? 0 : -1;
		}
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return fscanf(stream, "%u %u", &term->as.induction_hypothesis.frame_id, &term->as.induction_hypothesis.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return fscanf(stream, "%u", &term->as.universe_var.level_var) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return read_artifact_symbol(stream, symbols, &term->as.text_literal.text_symbol_id);
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				return fscanf(stream, "%" SCNd64, &term->as.int_literal.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return read_artifact_optional_symbol(
					stream,
					symbols,
					&term->as.external_ref.name.namespace_symbol_id
				) == 0 &&
					read_artifact_symbol(stream, symbols, &term->as.external_ref.name.name_symbol_id) == 0 ? 0 : -1;
			case PROTOTYPE_TERM_OPERATION:
				if (read_artifact_symbol(stream, symbols, &term->as.operation.symbol_id) != 0 ||
					read_artifact_optional_symbol(stream, symbols, &term->as.operation.type_symbol_id) != 0 ||
					prototype_term_operation_from_source_name(
						symbol_to_string(symbols, term->as.operation.symbol_id),
						&term->as.operation.operation_id
					) != 0) {
					return -1;
				}
				return 0;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				return fscanf(stream, "%u", &term->as.effect_label.effects) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				return fscanf(stream, "%u", &term->as.effect_row_var.binder_id) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return fscanf(stream, "%u %u", &term->as.effect_row_union.left,
					&term->as.effect_row_union.right) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				return fscanf(stream, "%u %u", &term->as.effect_row_forall.binder_id,
					&term->as.effect_row_forall.body) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				return fscanf(
					stream,
					"%u %u",
					&term->as.computation_type.label,
					&term->as.computation_type.result
				) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_THUNK_TYPE:
				return fscanf(stream, "%u", &term->as.thunk_type.computation) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_RETURN:
				return fscanf(stream, "%u", &term->as.return_term.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_THUNK:
				return fscanf(stream, "%u", &term->as.thunk.computation) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_FORCE:
				return fscanf(stream, "%u", &term->as.force.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_BIND:
				return fscanf(stream, "%u %u", &term->as.bind.computation,
					&term->as.bind.continuation) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				return fscanf(stream, "%u %u %u", &term->as.operation_request.operation,
					&term->as.operation_request.argument,
					&term->as.operation_request.continuation) == 3 ? 0 : -1;
			case PROTOTYPE_TERM_HANDLER:
				return fscanf(stream, "%u %u %u", &term->as.handler.operation,
					&term->as.handler.return_clause,
					&term->as.handler.operation_clause) == 3 ? 0 : -1;
			case PROTOTYPE_TERM_HANDLE:
				return fscanf(stream, "%u %u", &term->as.handle.handler,
					&term->as.handle.computation) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_HANDLER_TYPE:
				return fscanf(stream, "%u %u %u", &term->as.handler_type.operation,
					&term->as.handler_type.input_computation,
					&term->as.handler_type.output_computation) == 3 ? 0 : -1;
			default:
				return -1;
		}
	}

static int artifact_range_within(uint32_t first, uint32_t count, size_t slot_count) {
	if (count == 0) {
		return first <= slot_count;
	}
	if (first > UINT32_MAX - count) {
		return 0;
	}
	return (size_t)first + count <= slot_count;
}

static int artifact_read_type_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id
) {
	return type_declarations &&
		type_id < type_declarations->type_count &&
		artifact_type_present(&type_declarations->type_declarations[type_id]);
}

static int artifact_read_parameter_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_id
) {
	return type_declarations &&
		parameter_id < type_declarations->parameter_count &&
		artifact_parameter_present(&type_declarations->parameter_declarations[parameter_id]);
}

static int artifact_read_constructor_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t constructor_id
) {
	return type_declarations &&
		constructor_id < type_declarations->constructor_count &&
		artifact_constructor_present(&type_declarations->constructor_declarations[constructor_id]);
}

static int artifact_read_type_expr_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t expr_id
) {
	return type_declarations &&
		expr_id < type_declarations->expr_count &&
		artifact_type_expr_present(&type_declarations->exprs[expr_id]);
}

static int artifact_read_term_present(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	return terms &&
		term_id < terms->term_count &&
		artifact_term_present(&terms->terms[term_id]);
}

static int artifact_read_case_present(
	const struct prototype_term_db* terms,
	uint32_t case_id
) {
	return terms &&
		case_id < terms->case_count &&
		artifact_case_present(&terms->cases[case_id]);
}

static int artifact_read_case_binder_present(
	const struct prototype_term_db* terms,
	uint32_t binder_id
) {
	return terms &&
		binder_id < terms->case_binder_count &&
		artifact_case_binder_present(&terms->case_binders[binder_id]);
}

static int artifact_read_frame_present(
	const struct prototype_term_db* terms,
	uint32_t frame_id
) {
	return terms &&
		frame_id < terms->match_frame_count &&
		artifact_frame_present(&terms->match_frames[frame_id]);
}

static int artifact_read_proof_present(
	const struct prototype_judgement_db* judgement,
	uint32_t proof_id
) {
	return judgement &&
		proof_id < judgement->proof_count &&
		artifact_proof_present(&judgement->proofs[proof_id]);
}

static int artifact_validate_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t expr_id
) {
	if (!artifact_read_type_expr_present(type_declarations, expr_id)) {
		return -1;
	}
	const struct prototype_type_expr* expr = &type_declarations->exprs[expr_id];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
		case PROTOTYPE_TYPE_EXPR_SELF:
			case PROTOTYPE_TYPE_EXPR_VAR:
			case PROTOTYPE_TYPE_EXPR_NAME:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				return 0;
		case PROTOTYPE_TYPE_EXPR_APP:
			return artifact_read_type_expr_present(type_declarations, expr->as.app.function) &&
				artifact_read_type_expr_present(type_declarations, expr->as.app.argument) ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return artifact_read_type_expr_present(type_declarations, expr->as.arrow.domain) &&
				artifact_read_type_expr_present(type_declarations, expr->as.arrow.codomain) ? 0 : -1;
		default:
			return -1;
	}
}

static int artifact_validate_type_graph_refs(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms
) {
	if (!type_declarations || !terms) {
		return -1;
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		if (type->type_index != i ||
			type->formation_classifier == PROTOTYPE_INVALID_ID ||
			!artifact_read_term_present(terms, type->formation_classifier) ||
			!artifact_range_within(
				type->first_parameter,
				type->parameter_count,
				type_declarations->parameter_count
			) ||
			!artifact_range_within(
				type->first_constructor,
				type->constructor_count,
				type_declarations->constructor_count
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < type->parameter_count; ++j) {
			if (!artifact_read_parameter_present(
					type_declarations,
					type->first_parameter + j
				)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			if (!artifact_read_constructor_present(
					type_declarations,
					type->first_constructor + j
				)) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[i];
		if (!artifact_parameter_present(parameter)) {
			continue;
		}
		if (!artifact_read_type_expr_present(type_declarations, parameter->type_expr)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (!artifact_read_type_present(type_declarations, constructor->owner_type) ||
			constructor->classifier_family == PROTOTYPE_INVALID_ID ||
			!artifact_read_term_present(terms, constructor->classifier_family)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (!artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			continue;
		}
		if (!artifact_read_type_expr_present(
				type_declarations,
				type_declarations->readback_field_types[i]
			)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (!artifact_type_expr_present(&type_declarations->exprs[i])) {
			continue;
		}
		if (artifact_validate_type_expr_refs(type_declarations, (uint32_t)i) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_validate_term_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!artifact_read_term_present(terms, term_id) || !type_declarations) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
			case PROTOTYPE_TERM_VAR:
				case PROTOTYPE_TERM_UNIVERSE_VAR:
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
			case PROTOTYPE_TERM_INT_LITERAL:
				case PROTOTYPE_TERM_EXTERNAL_REF:
				case PROTOTYPE_TERM_OPERATION:
				case PROTOTYPE_TERM_EFFECT_LABEL:
				case PROTOTYPE_TERM_EFFECT_ROW_VAR:
					return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return artifact_read_term_present(terms, term->as.effect_row_union.left) &&
				artifact_read_term_present(terms, term->as.effect_row_union.right) ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_read_term_present(terms, term->as.effect_row_forall.body) ? 0 : -1;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_read_term_present(terms, term->as.constructor.owner) ? 0 : -1;
		case PROTOTYPE_TERM_APP:
			return artifact_read_term_present(terms, term->as.app.function) &&
				artifact_read_term_present(terms, term->as.app.argument) ? 0 : -1;
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_read_term_present(terms, term->as.lambda.body) ? 0 : -1;
		case PROTOTYPE_TERM_PI:
			return artifact_read_term_present(terms, term->as.pi.domain) &&
				artifact_read_term_present(terms, term->as.pi.codomain_family) ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			if (!artifact_read_term_present(terms, term->as.match.scrutinee) ||
				!artifact_range_within(
					term->as.match.first_case,
					term->as.match.case_count,
					terms->case_count
				) ||
				(term->as.match.frame_id != PROTOTYPE_INVALID_ID &&
					!artifact_read_frame_present(terms, term->as.match.frame_id))) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				if (!artifact_read_case_present(terms, term->as.match.first_case + i)) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return term->as.type_former.representation_id < type_declarations->representation_count ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (!artifact_read_type_present(
					type_declarations, term->as.type_declaration.type_id
				)) {
				return -1;
			}
			{
				const struct prototype_type_declaration* type =
					&type_declarations->type_declarations[
						term->as.type_declaration.type_id
					];
				return type->namespace_symbol_id ==
						term->as.type_declaration.identity.namespace_symbol_id &&
					type->name_symbol_id ==
						term->as.type_declaration.identity.name_symbol_id ? 0 : -1;
			}
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (!artifact_read_type_present(
					type_declarations, term->as.type_view.view_type_id
				) || !artifact_read_term_present(terms, term->as.type_view.core) ||
				!artifact_read_term_present(terms, term->as.type_view.source)) {
				return -1;
			}
			{
				const struct prototype_type_declaration* type =
					&type_declarations->type_declarations[term->as.type_view.view_type_id];
				return type->namespace_symbol_id ==
						term->as.type_view.identity.namespace_symbol_id &&
					type->name_symbol_id == term->as.type_view.identity.name_symbol_id ?
					0 : -1;
			}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_read_frame_present(terms, term->as.induction_hypothesis.frame_id) &&
					artifact_read_term_present(terms, term->as.induction_hypothesis.argument) ? 0 : -1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return artifact_read_term_present(terms, term->as.computation_type.label) &&
				artifact_read_term_present(terms, term->as.computation_type.result) ? 0 : -1;
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_read_term_present(terms, term->as.thunk_type.computation) ? 0 : -1;
		case PROTOTYPE_TERM_BIND:
			return artifact_read_term_present(terms, term->as.bind.computation) &&
				artifact_read_term_present(terms, term->as.bind.continuation) ? 0 : -1;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return artifact_read_term_present(terms, term->as.operation_request.operation) &&
				artifact_read_term_present(terms, term->as.operation_request.argument) &&
				artifact_read_term_present(terms, term->as.operation_request.continuation) ? 0 : -1;
		case PROTOTYPE_TERM_HANDLER:
			return artifact_read_term_present(terms, term->as.handler.operation) &&
				artifact_read_term_present(terms, term->as.handler.return_clause) &&
				artifact_read_term_present(terms, term->as.handler.operation_clause) ? 0 : -1;
		case PROTOTYPE_TERM_HANDLE:
			return artifact_read_term_present(terms, term->as.handle.handler) &&
				artifact_read_term_present(terms, term->as.handle.computation) ? 0 : -1;
		case PROTOTYPE_TERM_HANDLER_TYPE:
			return artifact_read_term_present(terms, term->as.handler_type.operation) &&
				artifact_read_term_present(terms, term->as.handler_type.input_computation) &&
				artifact_read_term_present(terms, term->as.handler_type.output_computation) ? 0 : -1;
		case PROTOTYPE_TERM_RETURN:
			return artifact_read_term_present(terms, term->as.return_term.value) ? 0 : -1;
		case PROTOTYPE_TERM_THUNK:
			return artifact_read_term_present(terms, term->as.thunk.computation) ? 0 : -1;
		case PROTOTYPE_TERM_FORCE:
			return artifact_read_term_present(terms, term->as.force.value) ? 0 : -1;
			default:
				return -1;
	}
}

static int artifact_validate_term_graph_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (artifact_validate_term_refs(terms, type_declarations, (uint32_t)i) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		if (!artifact_case_present(match_case)) {
			continue;
		}
		if (!artifact_read_term_present(terms, match_case->body) ||
			(match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
				!artifact_read_term_present(terms, match_case->constructor_owner)) ||
			!artifact_range_within(
				match_case->first_binder,
				match_case->binder_count,
				terms->case_binder_count
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
			if (!artifact_read_case_binder_present(terms, match_case->first_binder + j)) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < terms->match_frame_count; ++i) {
		const struct prototype_match_frame* frame = &terms->match_frames[i];
		if (!artifact_frame_present(frame)) {
			continue;
		}
		if (!artifact_read_term_present(terms, frame->match_term)) {
			return -1;
		}
	}
	return 0;
}

/* TYPE_FORMER ids are serialized as declaration anchors and rebound locally. */
static int artifact_resolve_representation_handles(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		struct prototype_term* term = &terms->terms[i];
		if (!artifact_term_present(term) || term->tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t representative_type_id = term->as.type_former.representation_id;
		if (representative_type_id >= type_declarations->type_count ||
			!artifact_type_present(&type_declarations->type_declarations[representative_type_id])) {
			return -1;
		}
		uint32_t representation_id =
			type_declarations->type_declarations[representative_type_id].representation_id;
		if (representation_id == PROTOTYPE_INVALID_ID ||
			representation_id >= type_declarations->representation_count) {
			return -1;
		}
		term->as.type_former.representation_id = representation_id;
	}
	return 0;
}

static int artifact_validate_judgement_graph_refs(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return -1;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (!artifact_relation_present(relation)) {
			continue;
		}
		if (!artifact_read_term_present(terms, relation->subject) ||
			!artifact_read_term_present(terms, relation->classifier) ||
			!artifact_read_proof_present(judgement, relation->proof_id)) {
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		const struct prototype_judgement_proof* proof = &judgement->proofs[i];
		if (!artifact_proof_present(proof)) {
			continue;
		}
		if (!artifact_read_term_present(terms, proof->conclusion_subject) ||
			!artifact_read_term_present(terms, proof->conclusion_classifier) ||
			proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
			(proof->context_kind != PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_NONE &&
				!artifact_read_term_present(terms, proof->context_subject))) {
			return -1;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (!artifact_read_term_present(terms, proof->premise_subjects[j]) ||
				!artifact_read_term_present(terms, proof->premise_classifiers[j]) ||
				!artifact_read_proof_present(judgement, proof->premise_proof_ids[j])) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_validate_read_graph_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	return artifact_validate_type_graph_refs(type_declarations, terms) == 0 &&
		artifact_validate_term_graph_refs(terms, type_declarations) == 0 &&
		artifact_validate_judgement_graph_refs(judgement, terms) == 0 ? 0 : -1;
}

static int artifact_read_universe_node_present(
	const struct prototype_universe_db* universe,
	uint32_t node_id
) {
	return universe &&
		node_id < universe->node_count &&
		artifact_universe_node_present(&universe->nodes[node_id]);
}

static int artifact_validate_read_universe_refs(
	const struct prototype_universe_db* universe
) {
	if (!universe) {
		return -1;
	}
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (!artifact_universe_node_present(node)) {
			continue;
		}
		if (node->tag != PROTOTYPE_UNIVERSE_NODE_TYPE &&
			node->tag != PROTOTYPE_UNIVERSE_NODE_PARAMETER) {
			return -1;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (!artifact_universe_edge_present(edge)) {
			continue;
		}
		if (edge->tag != PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE ||
			!artifact_read_universe_node_present(universe, edge->from_node) ||
			!artifact_read_universe_node_present(universe, edge->to_node)) {
			return -1;
		}
	}
	return 0;
}

static void sync_artifact_universe_level_counters(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return;
	}
	uint32_t next_level_var = type_declarations->next_level_var;
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		if (term->tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			term->as.universe_var.level_var >= next_level_var) {
			next_level_var = term->as.universe_var.level_var + 1;
		}
	}
	if (type_declarations->next_level_var < next_level_var) {
		type_declarations->next_level_var = next_level_var;
	}
	if (judgement->next_universe_var < next_level_var) {
		judgement->next_universe_var = next_level_var;
	}
}

int prototype_artifact_read_text_graph(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !terms || !type_declarations || !judgement) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "graph") != 0) {
		return -1;
	}

	char label_term_slots[32];
	char label_terms[32];
	char label_case_slots[32];
	char label_cases[32];
	char label_case_binder_slots[32];
	char label_case_binders[32];
	char label_frame_slots[32];
	char label_frames[32];
	char label_type_slots[32];
	char label_types[32];
	char label_parameter_slots[32];
	char label_parameters[32];
	char label_constructor_slots[32];
	char label_constructors[32];
	char label_field_type_slots[32];
	char label_field_types[32];
	char label_type_expr_slots[32];
	char label_type_exprs[32];
	char label_judgement_slots[32];
	char label_judgements[32];
	char label_proof_slots[32];
	char label_proofs[32];
	size_t term_slot_count;
	size_t present_term_count;
	size_t case_slot_count;
	size_t present_case_count;
	size_t case_binder_slot_count;
	size_t present_case_binder_count;
	size_t frame_slot_count;
	size_t present_frame_count;
	size_t type_slot_count;
	size_t present_type_count;
	size_t parameter_slot_count;
	size_t present_parameter_count;
	size_t constructor_slot_count;
	size_t present_constructor_count;
	size_t field_type_slot_count;
	size_t present_field_type_count;
	size_t expr_slot_count;
	size_t present_expr_count;
	size_t judgement_slot_count;
	size_t present_judgement_count;
	size_t proof_slot_count;
	size_t present_proof_count;
	if (fscanf(
			stream,
			"%255s"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu"
			" %31s %zu %31s %zu",
			word,
			label_term_slots,
			&term_slot_count,
			label_terms,
			&present_term_count,
			label_case_slots,
			&case_slot_count,
			label_cases,
			&present_case_count,
			label_case_binder_slots,
			&case_binder_slot_count,
			label_case_binders,
			&present_case_binder_count,
			label_frame_slots,
			&frame_slot_count,
			label_frames,
			&present_frame_count,
			label_type_slots,
			&type_slot_count,
			label_types,
			&present_type_count,
			label_parameter_slots,
			&parameter_slot_count,
			label_parameters,
			&present_parameter_count,
			label_constructor_slots,
			&constructor_slot_count,
			label_constructors,
			&present_constructor_count,
			label_field_type_slots,
			&field_type_slot_count,
			label_field_types,
			&present_field_type_count,
			label_type_expr_slots,
			&expr_slot_count,
			label_type_exprs,
			&present_expr_count,
			label_judgement_slots,
			&judgement_slot_count,
			label_judgements,
			&present_judgement_count,
			label_proof_slots,
			&proof_slot_count,
			label_proofs,
			&present_proof_count
		) != 45 ||
		strcmp(word, "counts") != 0 ||
		strcmp(label_term_slots, "term_slots") != 0 ||
		strcmp(label_terms, "terms") != 0 ||
		strcmp(label_case_slots, "case_slots") != 0 ||
		strcmp(label_cases, "cases") != 0 ||
		strcmp(label_case_binder_slots, "case_binder_slots") != 0 ||
		strcmp(label_case_binders, "case_binders") != 0 ||
		strcmp(label_frame_slots, "frame_slots") != 0 ||
		strcmp(label_frames, "frames") != 0 ||
		strcmp(label_type_slots, "type_slots") != 0 ||
		strcmp(label_types, "types") != 0 ||
		strcmp(label_parameter_slots, "parameter_slots") != 0 ||
		strcmp(label_parameters, "parameters") != 0 ||
		strcmp(label_constructor_slots, "constructor_slots") != 0 ||
		strcmp(label_constructors, "constructors") != 0 ||
		strcmp(label_field_type_slots, "field_type_slots") != 0 ||
		strcmp(label_field_types, "field_types") != 0 ||
		strcmp(label_type_expr_slots, "type_expr_slots") != 0 ||
		strcmp(label_type_exprs, "type_exprs") != 0 ||
		strcmp(label_judgement_slots, "judgement_slots") != 0 ||
		strcmp(label_judgements, "judgements") != 0 ||
		strcmp(label_proof_slots, "proof_slots") != 0 ||
		strcmp(label_proofs, "proofs") != 0) {
		return -1;
	}
	if (term_slot_count > terms->term_capacity ||
		case_slot_count > terms->case_capacity ||
		case_binder_slot_count > terms->case_binder_capacity ||
		frame_slot_count > terms->match_frame_capacity ||
		type_slot_count > type_declarations->type_capacity ||
		parameter_slot_count > type_declarations->parameter_capacity ||
		constructor_slot_count > type_declarations->constructor_capacity ||
		field_type_slot_count > type_declarations->readback_field_type_capacity ||
		expr_slot_count > type_declarations->expr_capacity ||
		judgement_slot_count > judgement->relation_capacity ||
		proof_slot_count > judgement->proof_capacity) {
		return -1;
	}

	size_t count;
	for (size_t i = 0; i < type_slot_count; ++i) {
		type_declarations->type_declarations[i].name_symbol_id = -1;
		type_declarations->type_declarations[i].namespace_symbol_id = -1;
		type_declarations->type_declarations[i].type_index = PROTOTYPE_INVALID_ID;
		type_declarations->type_declarations[i].formation_classifier =
			PROTOTYPE_INVALID_ID;
		type_declarations->type_declarations[i].first_parameter = PROTOTYPE_INVALID_ID;
		type_declarations->type_declarations[i].first_constructor = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < parameter_slot_count; ++i) {
		type_declarations->parameter_declarations[i].binder_id = PROTOTYPE_INVALID_ID;
		type_declarations->parameter_declarations[i].name_symbol_id = -1;
		type_declarations->parameter_declarations[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < constructor_slot_count; ++i) {
		type_declarations->constructor_declarations[i].name_symbol_id = -1;
		type_declarations->constructor_declarations[i].owner_type = PROTOTYPE_INVALID_ID;
		type_declarations->constructor_declarations[i].readback.first_field_type =
			PROTOTYPE_INVALID_ID;
		type_declarations->constructor_declarations[i].readback.result_type =
			PROTOTYPE_INVALID_ID;
		type_declarations->constructor_declarations[i].classifier_family = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < field_type_slot_count; ++i) {
		type_declarations->readback_field_types[i] = PROTOTYPE_INVALID_ID;
	}
	memset(type_declarations->exprs, 0, sizeof(*type_declarations->exprs) * expr_slot_count);

	if (expect_artifact_count(stream, "type_declarations", &count) != 0 ||
		count != present_type_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		char namespace_name[256];
		uint32_t type_index;
		uint32_t first_parameter;
		uint32_t parameter_count;
		uint32_t first_constructor;
		uint32_t constructor_count;
		uint32_t formation_classifier;
		if (fscanf(stream, "%255s %zu %255s %255s %u %u %u %u %u %u", word, &id, name, namespace_name, &type_index, &first_parameter, &parameter_count, &first_constructor, &constructor_count, &formation_classifier) != 10 ||
				strcmp(word, "type_decl") != 0 ||
				id >= type_slot_count ||
				type_index != id ||
				formation_classifier >= term_slot_count ||
				!artifact_range_within(first_parameter, parameter_count, parameter_slot_count) ||
				!artifact_range_within(first_constructor, constructor_count, constructor_slot_count)) {
				return -1;
			}
		struct prototype_type_declaration* type = &type_declarations->type_declarations[id];
		if (artifact_type_present(type)) {
			return -1;
		}
		type->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		type->namespace_symbol_id = strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (type->name_symbol_id < 0 || type->namespace_symbol_id < -1) {
			return -1;
		}
		type->type_index = type_index;
		type->formation_classifier = formation_classifier;
		type->first_parameter = first_parameter;
		type->parameter_count = parameter_count;
		type->first_constructor = first_constructor;
		type->constructor_count = constructor_count;
	}
	type_declarations->type_count = type_slot_count;

	if (expect_artifact_count(stream, "type_parameters", &count) != 0 ||
		count != present_parameter_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		uint32_t binder_id;
		uint32_t type_expr;
		if (fscanf(stream, "%255s %zu %u %255s %u", word, &id, &binder_id, name, &type_expr) != 5 ||
			strcmp(word, "type_param") != 0 ||
			id >= parameter_slot_count ||
			type_expr >= expr_slot_count) {
			return -1;
		}
		struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[id];
		if (artifact_parameter_present(parameter)) {
			return -1;
		}
		parameter->binder_id = binder_id;
		parameter->type_expr = type_expr;
		parameter->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (parameter->name_symbol_id < 0) {
			return -1;
		}
	}
	type_declarations->parameter_count = parameter_slot_count;

	if (expect_artifact_count(stream, "type_constructors", &count) != 0 ||
		count != present_constructor_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		uint32_t owner_type;
		uint32_t constructor_index;
		uint32_t first_field_type;
		uint32_t field_count;
		uint32_t result_type;
		uint32_t classifier_family;
		if (fscanf(stream, "%255s %zu %255s %u %u %u %u %u %u", word, &id, name, &owner_type, &constructor_index, &first_field_type, &field_count, &result_type, &classifier_family) != 9 ||
			strcmp(word, "type_constructor") != 0 ||
				id >= constructor_slot_count ||
				owner_type >= type_slot_count ||
				(field_count > 0 &&
					(first_field_type == PROTOTYPE_INVALID_ID ||
					!artifact_range_within(
						first_field_type, field_count, field_type_slot_count
					))) ||
				(result_type != PROTOTYPE_INVALID_ID && result_type >= expr_slot_count) ||
				(classifier_family != PROTOTYPE_INVALID_ID &&
					classifier_family >= term_slot_count)) {
			return -1;
		}
		struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[id];
		if (artifact_constructor_present(constructor)) {
			return -1;
		}
		constructor->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (constructor->name_symbol_id < 0) {
			return -1;
		}
		constructor->owner_type = owner_type;
		constructor->constructor_index = constructor_index;
		constructor->readback.first_field_type = first_field_type;
		constructor->readback.field_count = field_count;
		constructor->readback.result_type = result_type;
		constructor->classifier_family = classifier_family;
	}
	type_declarations->constructor_count = constructor_slot_count;

	if (expect_artifact_count(stream, "type_field_refs", &count) != 0 ||
		count != present_field_type_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		uint32_t type_expr;
		if (fscanf(stream, "%255s %zu %u", word, &id, &type_expr) != 3 ||
			strcmp(word, "type_field_ref") != 0 ||
			id >= field_type_slot_count ||
			type_expr >= expr_slot_count) {
			return -1;
		}
		if (artifact_field_type_present(&type_declarations->readback_field_types[id])) {
			return -1;
		}
		type_declarations->readback_field_types[id] = type_expr;
	}
	type_declarations->readback_field_type_count = field_type_slot_count;

	if (expect_artifact_count(stream, "type_exprs", &count) != 0 ||
		count != present_expr_count) {
		return -1;
	}
	uint32_t next_level_var = 0;
	for (size_t i = 0; i < count; ++i) {
		if (read_artifact_type_expr(
				stream,
				symbols,
				type_declarations,
				PROTOTYPE_INVALID_ID,
				&next_level_var
			) != 0) {
			return -1;
		}
	}
	type_declarations->expr_count = expr_slot_count;
	type_declarations->next_level_var = next_level_var;

	memset(terms->terms, 0, sizeof(*terms->terms) * term_slot_count);
	memset(terms->cases, 0, sizeof(*terms->cases) * case_slot_count);
	memset(terms->case_binders, 0, sizeof(*terms->case_binders) * case_binder_slot_count);
	memset(terms->match_frames, 0, sizeof(*terms->match_frames) * frame_slot_count);
	for (size_t i = 0; i < case_slot_count; ++i) {
		terms->cases[i].constructor_owner = PROTOTYPE_INVALID_ID;
		terms->cases[i].first_binder = PROTOTYPE_INVALID_ID;
		terms->cases[i].body = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < case_binder_slot_count; ++i) {
		terms->case_binders[i].binder_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < frame_slot_count; ++i) {
		terms->match_frames[i].match_term = PROTOTYPE_INVALID_ID;
	}

	if (expect_artifact_count(stream, "terms", &count) != 0 || count != present_term_count) {
		return -1;
	}
	uint32_t next_binder_id = 0;
	for (size_t i = 0; i < count; ++i) {
		if (read_artifact_term(
					stream,
					symbols,
					terms,
					PROTOTYPE_INVALID_ID,
					&next_binder_id
				) != 0) {
			return -1;
		}
	}
	terms->term_count = term_slot_count;

	if (expect_artifact_count(stream, "match_cases", &count) != 0 || count != present_case_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		struct prototype_match_case read_case;
		if (fscanf(
				stream,
				"%255s %zu %255s %u %u %u %u %u",
				word,
				&id,
				name,
				&read_case.constructor_owner,
				&read_case.constructor_id,
				&read_case.first_binder,
				&read_case.binder_count,
				&read_case.body
			) != 8 ||
				strcmp(word, "match_case") != 0 ||
				id >= case_slot_count) {
				return -1;
			}
			if (artifact_case_present(&terms->cases[id])) {
				return -1;
			}
			terms->cases[id] = read_case;
			terms->case_label_symbols[id] = symbol_intern(symbols, name, strlen(name));
			if (terms->case_label_symbols[id] < 0) {
			return -1;
		}
	}
	terms->case_count = case_slot_count;

		if (expect_artifact_count(stream, "case_binders", &count) != 0 || count != present_case_binder_count) {
			return -1;
		}
		for (size_t i = 0; i < count; ++i) {
			size_t id;
			uint32_t binder_id;
			int is_recursive;
			if (fscanf(stream, "%255s %zu", word, &id) != 2 ||
				strcmp(word, "case_binder") != 0 ||
				id >= case_binder_slot_count ||
				fscanf(stream, "%u %d", &binder_id, &is_recursive) != 2) {
				return -1;
			}
			if (artifact_case_binder_present(&terms->case_binders[id]) ||
				binder_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			terms->case_binders[id].binder_id = binder_id;
			terms->case_binders[id].is_recursive = is_recursive;
			if (terms->case_binders[id].binder_id < PROTOTYPE_PI_UNUSED_BINDER_ID &&
				next_binder_id <= terms->case_binders[id].binder_id) {
				next_binder_id = terms->case_binders[id].binder_id + 1;
			}
	}
	terms->case_binder_count = case_binder_slot_count;
	terms->next_binder_id = next_binder_id;

	if (expect_artifact_count(stream, "match_frames", &count) != 0 || count != present_frame_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_match_frame* frame;
		if (fscanf(stream, "%255s %zu", word, &id) != 2 ||
			strcmp(word, "match_frame") != 0 ||
				id >= frame_slot_count) {
				return -1;
			}
			frame = &terms->match_frames[id];
			if (artifact_frame_present(frame)) {
				return -1;
			}
			if (fscanf(stream, "%u %u %d", &frame->match_term, &frame->key.case_count, &frame->key.is_linkable) != 3 ||
				read_artifact_term_key(stream, &frame->key.match_key) != 0) {
				return -1;
		}
	}
	terms->match_frame_count = frame_slot_count;

	memset(judgement->relations, 0, sizeof(*judgement->relations) * judgement_slot_count);
	memset(judgement->proofs, 0, sizeof(*judgement->proofs) * proof_slot_count);
	if (expect_artifact_count(stream, "judgements", &count) != 0 || count != present_judgement_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_judgement_relation read_relation;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u %d %u",
				word,
				&id,
				&read_relation.kind,
				&read_relation.subject,
				&read_relation.classifier,
				&read_relation.proof_kind,
				&read_relation.proof_id
			) != 7 ||
			strcmp(word, "judgement") != 0 ||
				id >= judgement_slot_count ||
				read_relation.proof_id >= proof_slot_count) {
				return -1;
			}
			if (artifact_relation_present(&judgement->relations[id])) {
				return -1;
			}
			judgement->relations[id] = read_relation;
		}
	judgement->relation_count = judgement_slot_count;
	if (expect_artifact_count(stream, "proofs", &count) != 0 || count != present_proof_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_judgement_proof* proof;
		if (fscanf(stream, "%255s %zu", word, &id) != 2 ||
				strcmp(word, "proof") != 0 ||
				id >= proof_slot_count) {
				return -1;
			}
			proof = &judgement->proofs[id];
			if (artifact_proof_present(proof)) {
				return -1;
			}
			if (fscanf(
					stream,
					"%d %d %u %u %d %u %u %u %u",
				&proof->proof_kind,
				&proof->conclusion_kind,
				&proof->conclusion_subject,
				&proof->conclusion_classifier,
				&proof->context_kind,
				&proof->context_subject,
				&proof->context_index,
				&proof->context_aux,
				&proof->premise_count
			) != 9 ||
			proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (fscanf(
					stream,
					"%d %u %u %u",
					&proof->premise_kinds[j],
					&proof->premise_subjects[j],
					&proof->premise_classifiers[j],
					&proof->premise_proof_ids[j]
				) != 4 ||
				proof->premise_proof_ids[j] >= proof_slot_count) {
				return -1;
			}
		}
		}
		judgement->proof_count = proof_slot_count;
		if (prototype_type_declaration_rebuild_representations(terms, type_declarations) != 0) {
			return -1;
		}
		if (artifact_resolve_representation_handles(terms, type_declarations) != 0) {
			return -1;
		}
		if (artifact_validate_read_graph_refs(terms, type_declarations, judgement) != 0) {
			return -1;
		}
		judgement->next_universe_var = type_declarations->next_level_var;
		sync_artifact_universe_level_counters(terms, type_declarations, judgement);

	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "graph") != 0) {
		return -1;
	}
	return 0;
}

int prototype_artifact_read_text_operation_graph(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !terms) {
		return -1;
	}
	char word[256];
	char section_name[256];
	uint64_t normalization_step_limit;
	uint64_t normalization_steps_used;
	uint64_t solver_step_limit;
	uint64_t solver_steps_used;
	int solver_exhausted;
	uint64_t solver_constraint_count;
	uint64_t solver_solved_count;
	uint64_t solver_residual_count;
	uint64_t solver_incomplete_count;
	int compile_policy;
	uint64_t required_runtime_capabilities;
	size_t operation_count;
	size_t case_count;
	size_t obligation_count;
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph(metadata, &graph);
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 || strcmp(section_name, "operation_graph") != 0 ||
		fscanf(
			stream,
			" %255s %d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
			" %d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
			word,
			&compile_policy,
			&required_runtime_capabilities,
			&normalization_step_limit,
			&normalization_steps_used,
			&solver_step_limit,
			&solver_steps_used,
			&solver_exhausted,
			&solver_constraint_count,
			&solver_solved_count,
			&solver_residual_count,
			&solver_incomplete_count
		) != 12 || strcmp(word, "compile_policy") != 0 ||
		(compile_policy < PROTOTYPE_COMPILE_POLICY_STRICT ||
		 compile_policy > PROTOTYPE_COMPILE_POLICY_EXPLORATORY) ||
		(solver_exhausted != 0 && solver_exhausted != 1) ||
		solver_exhausted != 0 || solver_incomplete_count != 0 ||
		expect_artifact_count(stream, "operations", &operation_count) != 0 ||
		(metadata && operation_count > graph.operation_capacity)) {
		return -1;
	}
	if (metadata) {
		metadata->compile_policy = compile_policy;
		metadata->required_runtime_capabilities = required_runtime_capabilities;
		metadata->normalization_step_limit = normalization_step_limit;
		metadata->normalization_steps_used = normalization_steps_used;
		metadata->solver_step_limit = solver_step_limit;
		metadata->solver_steps_used = solver_steps_used;
		metadata->solver_exhausted = solver_exhausted;
		metadata->solver_constraint_count = solver_constraint_count;
		metadata->solver_solved_count = solver_solved_count;
		metadata->solver_residual_count = solver_residual_count;
		metadata->solver_incomplete_count = solver_incomplete_count;
		graph.operation_count = 0;
		graph.case_count = 0;
		prototype_verification_db_clear(&metadata->verification);
	}
	for (size_t i = 0; i < operation_count; ++i) {
		size_t id;
		struct prototype_operation_node operation;
		char source_name[256];
		char binder_name[256];
		memset(&operation, 0, sizeof(operation));
	if (fscanf(stream, "%255s %zu %d %d %d %u %u %u %u %u %255s %255s"
				" %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
				word, &id, &operation.tag, &operation.polarity, &operation.computation_kind,
				&operation.core_term, &operation.known_classifier, &operation.classifier,
				&operation.classifier_variable, &operation.source_ast, source_name, binder_name,
				&operation.referenced_ast_binder_id, &operation.function, &operation.argument,
				&operation.body, &operation.scrutinee, &operation.binder_classifier,
				&operation.handler_argument_ast_binder_id,
				&operation.handler_argument_binder_id,
				&operation.handler_continuation_ast_binder_id,
				&operation.handler_continuation_binder_id,
				&operation.handler_return_ast_binder_id,
				&operation.handler_return_binder_id,
				&operation.implicit_effect_row_count,
				&operation.first_case, &operation.case_count) != 27 ||
			strcmp(word, "operation") != 0 || id != i) {
			return -1;
		}
		if (strcmp(source_name, "-") == 0) {
			operation.source_symbol_id = -1;
		} else if ((operation.source_symbol_id = symbol_intern(
				symbols, source_name, strlen(source_name)
			)) < 0) {
			return -1;
		}
		if (strcmp(binder_name, "-") == 0) {
			operation.binder_symbol_id = -1;
		} else if ((operation.binder_symbol_id = symbol_intern(
				symbols, binder_name, strlen(binder_name)
			)) < 0) {
			return -1;
		}
		size_t row_id;
		if (fscanf(stream, "%255s %zu", word, &row_id) != 2 ||
			strcmp(word, "operation_rows") != 0 || row_id != i ||
			operation.implicit_effect_row_count > 16) {
			return -1;
		}
		for (uint32_t j = 0; j < operation.implicit_effect_row_count; ++j) {
			if (fscanf(stream, "%u", &operation.implicit_effect_row_binders[j]) != 1) {
				return -1;
			}
		}
		if (metadata) {
			if (prototype_operation_graph_add(&graph, operation, NULL) != 0) {
				return -1;
			}
		}
	}
	if (expect_artifact_count(stream, "operation_cases", &case_count) != 0 ||
		(metadata && case_count > graph.case_capacity)) {
		return -1;
	}
	for (size_t i = 0; i < case_count; ++i) {
		size_t id;
		struct prototype_operation_match_case operation_case;
		char label[256];
		if (fscanf(stream, "%255s %zu %u %u %u %255s", word, &id,
				&operation_case.body_operation, &operation_case.constructor_owner,
				&operation_case.constructor_id, label) != 6 ||
			strcmp(word, "operation_case") != 0 || id != i) {
			return -1;
		}
		if (strcmp(label, "-") == 0) {
			operation_case.case_label_symbol_id = -1;
		} else if ((operation_case.case_label_symbol_id = symbol_intern(
			symbols, label, strlen(label))) < 0) {
			return -1;
		}
		if (metadata) {
			if (prototype_operation_graph_add_case(&graph, operation_case, NULL) != 0) {
				return -1;
			}
		}
	}
	if (expect_artifact_count(stream, "verification_obligations", &obligation_count) != 0 ||
		(metadata && obligation_count >
			prototype_verification_db_capacity(&metadata->verification))) {
		return -1;
	}
	/* Import and linker callers that do not supply an occurrence graph cannot
	 * preserve a residual obligation. Reject it here rather than silently
	 * producing a linked artifact with weaker verification coverage. */
	if (!metadata && obligation_count != 0) {
		return -1;
	}
	if (compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT && obligation_count != 0) {
		return -1;
	}
	for (size_t i = 0; i < obligation_count; ++i) {
		size_t id;
		struct prototype_verification_obligation obligation;
		if (fscanf(stream, "%255s %zu %d %d %u %u %u %u %u %u %u %u %d %u", word, &id,
				&obligation.kind, &obligation.state, &obligation.operation,
				&obligation.core_term, &obligation.computation_operation,
				&obligation.continuation_operation, &obligation.continuation_binder_id,
				&obligation.input_classifier, &obligation.classifier_family,
				&obligation.effect_row, &obligation.normalization_profile,
				&obligation.schema_version) != 14 ||
			strcmp(word, "verification") != 0 || id != i ||
			obligation.kind < PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND ||
			obligation.kind > PROTOTYPE_VERIFICATION_OBLIGATION_RUNTIME_CONVERSION ||
			obligation.schema_version !=
				prototype_verification_obligation_schema_version(obligation.kind) ||
			obligation.state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			obligation.normalization_profile < PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
			obligation.normalization_profile >
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
			(metadata && prototype_verification_db_add(
				&metadata->verification, obligation, NULL
			) != 0)) {
			return -1;
		}
	}
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 || strcmp(section_name, "operation_graph") != 0) {
		return -1;
	}
	if (metadata) {
		if (prototype_operation_graph_validate(&graph, terms) != 0) {
			return -1;
		}
		prototype_compile_metadata_commit_operation_graph(metadata, &graph);
		for (size_t i = 0; i < prototype_operation_graph_count(&graph); ++i) {
			const struct prototype_operation_node* operation =
				prototype_operation_graph_get(&graph, (uint32_t)i);
			if (!operation) {
				return -1;
			}
			uint32_t term_references[] = {
				operation->core_term,
				operation->known_classifier,
				operation->classifier,
				operation->binder_classifier
			};
			for (size_t j = 0; j < sizeof(term_references) / sizeof(term_references[0]); ++j) {
				if (term_references[j] != PROTOTYPE_INVALID_ID &&
					!artifact_read_term_present(terms, term_references[j])) {
					return -1;
				}
			}
			if (operation->tag == PROTOTYPE_OPERATION_HANDLE &&
				(operation->body == PROTOTYPE_INVALID_ID ||
				 operation->scrutinee == PROTOTYPE_INVALID_ID ||
				 operation->handler_argument_ast_binder_id == PROTOTYPE_INVALID_ID ||
				 operation->handler_argument_binder_id == PROTOTYPE_INVALID_ID ||
				 operation->handler_continuation_ast_binder_id == PROTOTYPE_INVALID_ID ||
				 operation->handler_continuation_binder_id == PROTOTYPE_INVALID_ID ||
				 operation->handler_return_ast_binder_id == PROTOTYPE_INVALID_ID ||
				 operation->handler_return_binder_id == PROTOTYPE_INVALID_ID ||
				 terms->terms[operation->core_term].tag != PROTOTYPE_TERM_HANDLE)) {
				return -1;
			}
		}
		if (prototype_verification_db_validate(
				&metadata->verification, &graph, terms
			) != 0) {
			return -1;
		}
		uint64_t declared_capabilities = metadata->required_runtime_capabilities;
		compile_metadata_refresh_runtime_capabilities(metadata, terms);
		if (metadata->required_runtime_capabilities != declared_capabilities) {
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_read_text_universe(
	FILE* stream,
	struct prototype_universe_db* universe
) {
	if (!stream || !universe) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "universe") != 0) {
		return -1;
	}

	char label_node_slots[32];
	char label_nodes[32];
	char label_edge_slots[32];
	char label_edges[32];
	char label_levels[32];
	char label_constraints[32];
	char label_solved[32];
	size_t node_slot_count;
	size_t present_node_count;
	size_t edge_slot_count;
	size_t present_edge_count;
	size_t level_count;
	size_t constraint_count;
	int solved;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu %31s %zu %31s %zu %31s %zu %31s %zu %31s %d",
			word,
			label_node_slots,
			&node_slot_count,
			label_nodes,
			&present_node_count,
			label_edge_slots,
			&edge_slot_count,
			label_edges,
			&present_edge_count,
			label_levels,
			&level_count,
			label_constraints,
			&constraint_count,
			label_solved,
			&solved
		) != 15 ||
		strcmp(word, "counts") != 0 ||
		strcmp(label_node_slots, "node_slots") != 0 ||
		strcmp(label_nodes, "nodes") != 0 ||
		strcmp(label_edge_slots, "edge_slots") != 0 ||
		strcmp(label_edges, "edges") != 0 ||
		strcmp(label_levels, "levels") != 0 ||
		strcmp(label_constraints, "constraints") != 0 ||
		strcmp(label_solved, "solved") != 0 ||
		node_slot_count > universe->node_capacity ||
		edge_slot_count > universe->edge_capacity ||
		level_count > universe->level_capacity ||
		constraint_count > universe->constraint_capacity) {
		return -1;
	}

	size_t count;
	memset(universe->nodes, 0, sizeof(*universe->nodes) * node_slot_count);
	memset(universe->edges, 0, sizeof(*universe->edges) * edge_slot_count);
	if (expect_artifact_count(stream, "universe_nodes", &count) != 0 ||
		count != present_node_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_node read_node;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u %d %u",
				word,
				&id,
				&read_node.tag,
				&read_node.type_id,
				&read_node.parameter_id,
				&read_node.symbol_id,
				&read_node.type_expr
			) != 7 ||
				strcmp(word, "universe_node") != 0 ||
				id >= node_slot_count) {
				return -1;
			}
			if (artifact_universe_node_present(&universe->nodes[id])) {
				return -1;
			}
			universe->nodes[id] = read_node;
		}
		universe->node_count = node_slot_count;

	if (expect_artifact_count(stream, "universe_edges", &count) != 0 ||
		count != present_edge_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_edge read_edge;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u",
				word,
				&id,
				&read_edge.tag,
				&read_edge.from_node,
				&read_edge.to_node
			) != 5 ||
			strcmp(word, "universe_edge") != 0 ||
				id >= edge_slot_count ||
				read_edge.from_node >= node_slot_count ||
				read_edge.to_node >= node_slot_count) {
				return -1;
			}
			if (artifact_universe_edge_present(&universe->edges[id])) {
				return -1;
			}
			universe->edges[id] = read_edge;
		}
		universe->edge_count = edge_slot_count;

	if (expect_artifact_count(stream, "universe_levels", &count) != 0 ||
		count != level_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_level* level = &universe->levels[i];
		if (fscanf(
				stream,
				"%255s %zu %u %d",
				word,
				&id,
				&level->level_var,
				&level->value
			) != 4 ||
			strcmp(word, "universe_level") != 0 ||
			id != i) {
			return -1;
		}
	}
	universe->level_count = count;

	if (expect_artifact_count(stream, "universe_constraints", &count) != 0 ||
		count != constraint_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_constraint* constraint = &universe->constraints[i];
		if (fscanf(
				stream,
				"%255s %zu %u %u %d %u %u %d",
				word,
				&id,
				&constraint->lower_level_var,
				&constraint->upper_level_var,
				&constraint->offset,
				&constraint->subject,
				&constraint->classifier,
				&constraint->reason_kind
			) != 8 ||
			strcmp(word, "universe_constraint") != 0 ||
			id != i) {
			return -1;
		}
		}
		universe->constraint_count = count;
		universe->solved = solved;
		if (artifact_validate_read_universe_refs(universe) != 0) {
			return -1;
		}

		if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "universe") != 0) {
		return -1;
	}
	return 0;
}

int prototype_artifact_read_text_debug(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_debug_table* debug
) {
	if (!stream || !symbols || !debug) {
		return -1;
	}
	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "debug") != 0) {
		return -1;
	}

	size_t count;
	if (expect_artifact_count(stream, "term_names", &count) != 0 ||
		count > debug->term_name_capacity) {
		return -1;
	}
	debug->term_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		size_t term;
		size_t classifier;
		size_t source_entry_id;
		unsigned name_line;
		unsigned name_column;
		unsigned body_line;
		unsigned body_column;
		if (fscanf(
				stream,
				"%255s %255s %zu %zu %zu %u %u %u %u",
				word,
				name,
				&term,
				&classifier,
				&source_entry_id,
				&name_line,
				&name_column,
				&body_line,
				&body_column
			) != 9 ||
			strcmp(word, "term_name") != 0 ||
			term > UINT32_MAX ||
			classifier > UINT32_MAX ||
			source_entry_id > UINT32_MAX) {
			return -1;
		}
		debug->term_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->term_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->term_names[i].term = (uint32_t)term;
		debug->term_names[i].classifier = (uint32_t)classifier;
		debug->term_names[i].source_entry_id = (uint32_t)source_entry_id;
		debug->term_names[i].name_span.line = name_line;
		debug->term_names[i].name_span.column = name_column;
		debug->term_names[i].body_span.line = body_line;
		debug->term_names[i].body_span.column = body_column;
	}

	if (expect_artifact_count(stream, "type_names", &count) != 0 ||
		count > debug->type_name_capacity) {
		return -1;
	}
	debug->type_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		size_t local_type_id;
		unsigned name_line;
		unsigned name_column;
		unsigned body_line;
		unsigned body_column;
		if (fscanf(
				stream,
				"%255s %255s %zu %u %u %u %u",
				word,
				name,
				&local_type_id,
				&name_line,
				&name_column,
				&body_line,
				&body_column
			) != 7 ||
			strcmp(word, "type_name") != 0 ||
			local_type_id > UINT32_MAX) {
			return -1;
		}
		debug->type_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->type_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->type_names[i].local_type_id = (uint32_t)local_type_id;
		debug->type_names[i].name_span.line = name_line;
		debug->type_names[i].name_span.column = name_column;
		debug->type_names[i].body_span.line = body_line;
		debug->type_names[i].body_span.column = body_column;
	}

	if (expect_artifact_count(stream, "constructor_names", &count) != 0 ||
		count > debug->constructor_name_capacity) {
		return -1;
	}
	debug->constructor_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		uint32_t type_export_index;
		uint32_t ordinal;
		unsigned name_line;
		unsigned name_column;
		if (fscanf(
				stream,
				"%255s %u %255s %u %u %u",
				word,
				&type_export_index,
				name,
				&ordinal,
				&name_line,
				&name_column
			) != 6 ||
			strcmp(word, "constructor_name") != 0) {
			return -1;
		}
		debug->constructor_names[i].type_export_index = type_export_index;
		debug->constructor_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->constructor_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->constructor_names[i].ordinal = ordinal;
		debug->constructor_names[i].name_span.line = name_line;
		debug->constructor_names[i].name_span.column = name_column;
	}
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "debug") != 0) {
		return -1;
	}
	return 0;
}

int prototype_artifact_read_text_relocation(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_relocation_table* relocation
) {
	if (!stream || !symbols || !relocation) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "relocation") != 0) {
		return -1;
	}

	size_t count;
	if (expect_artifact_count(stream, "external_term_refs", &count) != 0 ||
		count > relocation->external_term_ref_capacity) {
		return -1;
	}
	relocation->external_term_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t term;
		if (fscanf(
					stream,
					"%255s %zu %255s %255s",
					word,
					&term,
					namespace_name,
					name
			) != 4 ||
			strcmp(word, "external_term_ref") != 0 ||
			term > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_term_refs[i].term = (uint32_t)term;
		relocation->external_term_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		relocation->external_term_refs[i].name.name_symbol_id = interned;
		if (relocation->external_term_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "resolved_external_term_refs", &count) != 0 ||
		count > relocation->resolved_external_term_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_term_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t term;
		uint32_t term_export_index;
		if (fscanf(
				stream,
					"%255s %zu %u %255s %255s",
				word,
				&term,
				&term_export_index,
					namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_term_ref") != 0 ||
			term > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->resolved_external_term_refs[i].term = (uint32_t)term;
		relocation->resolved_external_term_refs[i].term_export_index =
			term_export_index;
		relocation->resolved_external_term_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		relocation->resolved_external_term_refs[i].name.name_symbol_id = interned;
		if (relocation->resolved_external_term_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "external_type_expr_refs", &count) != 0 ||
		count > relocation->external_type_expr_ref_capacity) {
		return -1;
	}
	relocation->external_type_expr_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		int symbol_id;
		size_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %d %255s",
				word,
				&type_expr,
				&symbol_id,
				name
			) != 4 ||
			strcmp(word, "external_type_expr_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_type_expr_refs[i].type_expr = (uint32_t)type_expr;
		relocation->external_type_expr_refs[i].name_symbol_id = interned;
		(void)symbol_id;
	}

	if (expect_artifact_count(stream, "resolved_external_type_expr_refs", &count) != 0 ||
		count > relocation->resolved_external_type_expr_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_type_expr_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t type_expr;
		uint32_t type_export_index;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %255s",
				word,
				&type_expr,
				&type_export_index,
				namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_type_expr_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0 ||
			read_artifact_type_code_shape_key(
				stream,
				&relocation->resolved_external_type_expr_refs[i].code_shape_key
			) != 0) {
			return -1;
		}
		relocation->resolved_external_type_expr_refs[i].type_expr =
			(uint32_t)type_expr;
		relocation->resolved_external_type_expr_refs[i].type_export_index =
			type_export_index;
		relocation->resolved_external_type_expr_refs[i].name.name_symbol_id = interned;
		relocation->resolved_external_type_expr_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (relocation->resolved_external_type_expr_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "external_type_former_refs", &count) != 0 ||
		count > relocation->external_type_former_ref_capacity) {
		return -1;
	}
	relocation->external_type_former_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		int symbol_id;
		size_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %d %255s",
				word,
				&type_expr,
				&symbol_id,
				name
			) != 4 ||
			strcmp(word, "external_type_former_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_type_former_refs[i].type_expr = (uint32_t)type_expr;
		relocation->external_type_former_refs[i].name_symbol_id = interned;
		(void)symbol_id;
	}
	if (expect_artifact_count(stream, "resolved_external_type_former_refs", &count) != 0 ||
		count > relocation->resolved_external_type_former_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_type_former_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t type_expr;
		uint32_t type_export_index;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %255s",
				word,
				&type_expr,
				&type_export_index,
				namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_type_former_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0 ||
			read_artifact_type_code_shape_key(
				stream,
				&relocation->resolved_external_type_former_refs[i].code_shape_key
			) != 0) {
			return -1;
		}
		relocation->resolved_external_type_former_refs[i].type_expr =
			(uint32_t)type_expr;
		relocation->resolved_external_type_former_refs[i].type_export_index =
			type_export_index;
		relocation->resolved_external_type_former_refs[i].name.name_symbol_id = interned;
		relocation->resolved_external_type_former_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (relocation->resolved_external_type_former_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}
	if (expect_artifact_count(stream, "resolved_constructor_owner_refs", &count) != 0 ||
		count > relocation->resolved_constructor_owner_ref_capacity) {
		return -1;
	}
	relocation->resolved_constructor_owner_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		struct prototype_artifact_resolved_constructor_owner_ref* ref =
			&relocation->resolved_constructor_owner_refs[i];
		size_t source;
		if (fscanf(
				stream,
				"%255s %d %zu %u %u",
				word,
				&ref->source_kind,
				&source,
				&ref->owner,
				&ref->ordinal
			) != 5 ||
			strcmp(word, "resolved_constructor_owner_ref") != 0 ||
			source > UINT32_MAX ||
			read_artifact_term_key(stream, &ref->owner_key) != 0) {
			return -1;
		}
		ref->source = (uint32_t)source;
	}
	if (expect_artifact_count(stream, "external_constructor_owner_refs", &count) != 0) {
		return -1;
	}
	relocation->external_constructor_owner_ref_count = count;
	if (count != 0) {
		return -1;
	}

	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "relocation") != 0) {
		return -1;
	}
	return 0;
}

static int artifact_existing_term_representative(
	const struct prototype_term_db* terms,
	uint32_t term,
	uint32_t* p_representative
) {
	if (!terms || !p_representative || term >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < term; ++i) {
		int equal = 0;
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (prototype_term_view_shape_equal(terms, i, term, &equal) != 0) {
			return -1;
		}
		if (equal) {
			*p_representative = i;
			return 0;
		}
	}
	*p_representative = term;
	return 0;
}

int prototype_artifact_apply_term_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* provider_interface
) {
	if (!target_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !provider_interface) {
		return -1;
	}
	for (size_t i = 0; i < provider_interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* provider =
			&provider_interface->term_exports[i];
		struct prototype_qualified_name provider_name = qualified_name_make(
			provider->namespace_symbol_id,
			provider->name_symbol_id
		);
		uint32_t provider_term;
		if (provider->local_term >= target_terms->term_count) {
			return -1;
		}
		if (provider->transparency != PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT) {
			continue;
		}
		if (artifact_existing_term_representative(
				target_terms, provider->local_term, &provider_term
			) != 0) {
			return -1;
		}
		for (size_t j = 0; j < target_interface->term_export_count; ++j) {
			struct prototype_artifact_term_export* target =
				&target_interface->term_exports[j];
			uint32_t linked;
			if (target->local_term >= target_terms->term_count ||
				prototype_term_resolve_external_ref(
					target_terms,
					target->local_term,
					provider_name,
					provider_term,
					&linked
				) != 0) {
				return -1;
			}
			target->local_term = linked;
			if (prototype_term_canonical_key_with_types(
					target_terms,
					target_type_declarations,
					target->local_term,
					&target->canonical_key
				) != 0) {
				return -1;
			}
			if (target->classifier < target_terms->term_count &&
				prototype_term_resolve_external_ref(
					target_terms,
					target->classifier,
					provider_name,
					provider_term,
					&linked
				) != 0) {
				return -1;
			}
			if (target->classifier < target_terms->term_count) {
				target->classifier = linked;
				if (prototype_term_canonical_key_with_types(
						target_terms,
						target_type_declarations,
						target->classifier,
						&target->classifier_key
					) != 0) {
					return -1;
				}
			} else {
				memset(&target->classifier_key, 0, sizeof(target->classifier_key));
			}
		}
			for (size_t j = 0; j < target_judgement->relation_count; ++j) {
				struct prototype_judgement_relation* relation =
					&target_judgement->relations[j];
				uint32_t linked_subject;
				uint32_t linked_classifier;
				if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
					continue;
				}
				if (relation->subject >= target_terms->term_count ||
					relation->classifier >= target_terms->term_count ||
				prototype_term_resolve_external_ref(
					target_terms,
					relation->subject,
					provider_name,
					provider_term,
					&linked_subject
				) != 0 ||
				prototype_term_resolve_external_ref(
					target_terms,
					relation->classifier,
					provider_name,
					provider_term,
					&linked_classifier
				) != 0) {
				return -1;
			}
			relation->subject = linked_subject;
			relation->classifier = linked_classifier;
			}
			for (size_t j = 0; j < target_judgement->proof_count; ++j) {
				struct prototype_judgement_proof* proof = &target_judgement->proofs[j];
				if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
					continue;
				}
					if (prototype_term_resolve_external_ref(
							target_terms,
							proof->conclusion_subject,
					provider_name,
					provider_term,
					&proof->conclusion_subject
				) != 0 ||
				prototype_term_resolve_external_ref(
					target_terms,
					proof->conclusion_classifier,
					provider_name,
					provider_term,
					&proof->conclusion_classifier
					) != 0) {
					return -1;
				}
				if (proof->context_subject != PROTOTYPE_INVALID_ID &&
					prototype_term_resolve_external_ref(
						target_terms,
						proof->context_subject,
						provider_name,
						provider_term,
						&proof->context_subject
					) != 0) {
					return -1;
				}
				for (uint32_t k = 0; k < proof->premise_count; ++k) {
				if (prototype_term_resolve_external_ref(
						target_terms,
						proof->premise_subjects[k],
						provider_name,
						provider_term,
						&proof->premise_subjects[k]
					) != 0 ||
					prototype_term_resolve_external_ref(
						target_terms,
						proof->premise_classifiers[k],
						provider_name,
						provider_term,
						&proof->premise_classifiers[k]
					) != 0) {
					return -1;
				}
			}
		}
	}
	prototype_term_notify_graph_mutation(target_terms);
	return prototype_artifact_interface_collect_dependencies(
		target_interface,
		target_terms,
		target_type_declarations,
		target_judgement
	);
}

int prototype_artifact_interface_recompute_keys(
	struct prototype_artifact_interface* interface,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!interface || !terms || !type_declarations) {
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		struct prototype_artifact_type_export* export = &interface->type_exports[i];
			if (export->local_type_id >= type_declarations->type_count ||
				prototype_type_declaration_code_shape_key(
					terms,
					type_declarations,
					export->local_type_id,
					&export->code_shape_key
				) != 0) {
				return -1;
			}
			uint32_t formation_classifier = type_declarations->type_declarations[
				export->local_type_id
			].formation_classifier;
			if (formation_classifier == PROTOTYPE_INVALID_ID ||
				formation_classifier >= terms->term_count) {
				return -1;
			}
			export->formation_classifier = formation_classifier;
			if (prototype_type_declaration_representation_anchor_type_id(
					terms,
					type_declarations,
					export->local_type_id,
					&export->core_representation_anchor_type_id
				) != 0) {
				return -1;
			}
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (export->local_term >= terms->term_count ||
			prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				export->local_term,
				&export->canonical_key
			) != 0) {
			return -1;
		}
		memset(&export->classifier_key, 0, sizeof(export->classifier_key));
		if (export->classifier != PROTOTYPE_INVALID_ID &&
			(export->classifier >= terms->term_count ||
				prototype_term_canonical_key_with_types(
					terms,
					type_declarations,
					export->classifier,
					&export->classifier_key
				) != 0)) {
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_apply_type_expr_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* provider_interface
) {
	if (!target_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !provider_interface) {
		return -1;
	}

	for (size_t i = 0; i < target_type_declarations->expr_count; ++i) {
		struct prototype_type_expr* expr = &target_type_declarations->exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME) {
			continue;
		}
		uint32_t export_id;
		int found = prototype_artifact_interface_find_type_export(
			provider_interface,
			expr->as.name.symbol_id,
			&export_id
		);
		if (found < 0) {
			return -1;
		}
		if (found > 0) {
			continue;
		}
		int symbol_id = expr->as.name.symbol_id;
		const struct prototype_artifact_type_export* export =
			&provider_interface->type_exports[export_id];
		memset(expr, 0, sizeof(*expr));
		expr->tag = PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE;
		expr->as.imported_type.name = qualified_name_make(
			export->namespace_symbol_id,
			symbol_id
		);
		expr->as.imported_type.code_shape_key = export->code_shape_key;
	}

	if (prototype_artifact_interface_recompute_keys(
			target_interface,
			target_terms,
			target_type_declarations
		) != 0) {
		return -1;
	}
	return prototype_artifact_interface_collect_dependencies(
		target_interface,
		target_terms,
		target_type_declarations,
		target_judgement
	);
}

static uint32_t offset_artifact_id(uint32_t id, uint32_t offset) {
	return id == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : id + offset;
}

static uint32_t offset_artifact_binder_id(uint32_t id, uint32_t offset) {
	if (id == PROTOTYPE_INVALID_ID || id == PROTOTYPE_PI_UNUSED_BINDER_ID) {
		return id;
	}
	return id + offset;
}

static int canonicalize_type_view_core_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	if (type_declarations->representations_dirty) {
		if (prototype_type_declaration_rebuild_representations(terms, type_declarations) != 0) {
			return -1;
		}
		if (prototype_term_rebind_type_former_anchors(terms, type_declarations) != 0) {
			return -1;
		}
	}
	size_t original_term_count = terms->term_count;
	for (size_t i = 0; i < original_term_count; ++i) {
		if (terms->terms[i].tag != PROTOTYPE_TERM_TYPE_VIEW) {
			continue;
		}
		uint32_t type_id;
		uint32_t args[16];
		uint32_t arg_count;
		uint32_t canonical_view;
		if (prototype_term_type_instance_info(
				terms,
				(uint32_t)i,
				&type_id,
				args,
				&arg_count
			) != 0 ||
			prototype_term_type_instance_make(
				terms,
				type_declarations,
				type_id,
				args,
				arg_count,
				&canonical_view
			) != 0 ||
			canonical_view >= terms->term_count ||
			terms->terms[canonical_view].tag != PROTOTYPE_TERM_TYPE_VIEW) {
			return -1;
		}
		/* type_instance_make may grow TermDB and relocate terms. Reacquire the
		 * slot by index rather than writing through a pointer captured above. */
		terms->terms[i].as.type_view.core =
			terms->terms[canonical_view].as.type_view.core;
	}
	return 0;
}

static int find_existing_term_by_canonical_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	const struct prototype_term_canonical_key* key,
	uint32_t appended_term,
	uint32_t* p_term_id
) {
	if (!terms || !type_declarations || !key || !p_term_id ||
		appended_term >= terms->term_count) {
		return -1;
	}
	if (!canonical_key_is_cross_artifact_linkable(key)) {
		return 0;
	}
	for (uint32_t i = 0; i < old_term_count; ++i) {
		struct prototype_term_canonical_key candidate;
		int same_term = 0;
		if (terms->terms[i].tag == 0) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				i,
				&candidate
			) != 0) {
			return -1;
		}
		if (!canonical_keys_equal(&candidate, key)) {
			continue;
		}
			if (prototype_term_view_shape_equal_cross_db(
					terms,
					type_declarations,
					i,
					terms,
				type_declarations,
				appended_term,
				1,
				&same_term
			) != 0) {
			return -1;
		}
		if (same_term) {
			*p_term_id = i;
			return 1;
		}
	}
	return 0;
}

static int canonicalize_constructor_owner_ref(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	uint32_t* p_owner
) {
	if (!terms || !type_declarations || !p_owner) {
		return -1;
	}
	if (*p_owner == PROTOTYPE_INVALID_ID || *p_owner < old_term_count) {
		return 0;
	}
	if (*p_owner >= terms->term_count) {
		return -1;
	}
	uint32_t search_term_count = old_term_count;
	if (search_term_count == 0 || search_term_count > *p_owner) {
		search_term_count = *p_owner;
	}

	struct prototype_term_canonical_key key;
	if (prototype_term_canonical_key_with_types(
			terms,
			type_declarations,
			*p_owner,
			&key
		) != 0) {
		return -1;
	}
	uint32_t existing_owner;
	int found_existing = find_existing_term_by_canonical_key(
		terms,
		type_declarations,
		search_term_count,
		&key,
		*p_owner,
		&existing_owner
	);
	if (found_existing < 0) {
		return -1;
	}
	if (found_existing > 0) {
		*p_owner = existing_owner;
	}
	return 0;
}

static int canonicalize_constructor_owner_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	for (size_t i = old_term_count; i < terms->term_count; ++i) {
		struct prototype_term* term = &terms->terms[i];
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
			continue;
		}
		if (canonicalize_constructor_owner_ref(
				terms,
				type_declarations,
				old_term_count,
				&term->as.constructor.owner
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		struct prototype_match_case* match_case = &terms->cases[i];
		if (canonicalize_constructor_owner_ref(
				terms,
				type_declarations,
				old_term_count,
				&match_case->constructor_owner
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static void offset_artifact_type_expr(
	struct prototype_type_expr* expr,
	uint32_t expr_offset,
	uint32_t binder_offset,
	uint32_t universe_offset
) {
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			expr->as.universe_var.level_var += universe_offset;
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			expr->as.var.binder_id =
				offset_artifact_binder_id(expr->as.var.binder_id, binder_offset);
			break;
		case PROTOTYPE_TYPE_EXPR_APP:
			expr->as.app.function += expr_offset;
			expr->as.app.argument += expr_offset;
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			expr->as.arrow.domain += expr_offset;
			expr->as.arrow.codomain += expr_offset;
			break;
		default:
			break;
	}
}

static void offset_artifact_term(
	struct prototype_term* term,
	uint32_t term_offset,
	uint32_t case_offset,
	uint32_t frame_offset,
	uint32_t type_offset,
	uint32_t binder_offset,
	uint32_t universe_offset
) {
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			term->as.var.binder_id =
				offset_artifact_binder_id(term->as.var.binder_id, binder_offset);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			term->as.constructor.owner = offset_artifact_id(term->as.constructor.owner, term_offset);
			break;
		case PROTOTYPE_TERM_APP:
			term->as.app.function += term_offset;
			term->as.app.argument += term_offset;
			break;
		case PROTOTYPE_TERM_LAMBDA:
			term->as.lambda.binder_id =
				offset_artifact_binder_id(term->as.lambda.binder_id, binder_offset);
			term->as.lambda.body += term_offset;
			break;
		case PROTOTYPE_TERM_PI:
			term->as.pi.domain += term_offset;
			term->as.pi.codomain_family += term_offset;
			break;
		case PROTOTYPE_TERM_MATCH:
			term->as.match.scrutinee += term_offset;
			term->as.match.first_case += case_offset;
			term->as.match.frame_id = offset_artifact_id(term->as.match.frame_id, frame_offset);
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			term->as.type_declaration.type_id += type_offset;
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			term->as.type_view.view_type_id += type_offset;
			term->as.type_view.core += term_offset;
			term->as.type_view.source += term_offset;
			break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			term->as.induction_hypothesis.frame_id =
				offset_artifact_id(term->as.induction_hypothesis.frame_id, frame_offset);
			term->as.induction_hypothesis.argument += term_offset;
			break;
			case PROTOTYPE_TERM_UNIVERSE_VAR:
				term->as.universe_var.level_var += universe_offset;
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				term->as.effect_row_var.binder_id =
					offset_artifact_binder_id(term->as.effect_row_var.binder_id, binder_offset);
				break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			term->as.computation_type.label += term_offset;
			term->as.computation_type.result += term_offset;
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			term->as.effect_row_union.left += term_offset;
			term->as.effect_row_union.right += term_offset;
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			term->as.effect_row_forall.binder_id =
				offset_artifact_binder_id(term->as.effect_row_forall.binder_id, binder_offset);
			term->as.effect_row_forall.body += term_offset;
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			term->as.thunk_type.computation += term_offset;
			break;
		case PROTOTYPE_TERM_RETURN:
			term->as.return_term.value += term_offset;
			break;
		case PROTOTYPE_TERM_THUNK:
			term->as.thunk.computation += term_offset;
			break;
		case PROTOTYPE_TERM_FORCE:
			term->as.force.value += term_offset;
			break;
		case PROTOTYPE_TERM_BIND:
			term->as.bind.computation += term_offset;
			term->as.bind.continuation += term_offset;
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			term->as.operation_request.operation += term_offset;
			term->as.operation_request.argument += term_offset;
			term->as.operation_request.continuation += term_offset;
			break;
		case PROTOTYPE_TERM_HANDLER:
			term->as.handler.operation += term_offset;
			term->as.handler.return_clause += term_offset;
			term->as.handler.operation_clause += term_offset;
			break;
		case PROTOTYPE_TERM_HANDLE:
			term->as.handle.handler += term_offset;
			term->as.handle.computation += term_offset;
			break;
		case PROTOTYPE_TERM_HANDLER_TYPE:
			term->as.handler_type.operation += term_offset;
			term->as.handler_type.input_computation += term_offset;
			term->as.handler_type.output_computation += term_offset;
			break;
			default:
				break;
		}
}

int prototype_artifact_append_graph(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement
) {
	if (!appended_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !source_interface || !source_terms ||
		!source_type_declarations || !source_judgement) {
		return -1;
	}

	uint32_t term_offset = (uint32_t)target_terms->term_count;
	uint32_t case_offset = (uint32_t)target_terms->case_count;
	uint32_t case_binder_offset = (uint32_t)target_terms->case_binder_count;
	uint32_t frame_offset = (uint32_t)target_terms->match_frame_count;
	uint32_t type_offset = (uint32_t)target_type_declarations->type_count;
	uint32_t parameter_offset = (uint32_t)target_type_declarations->parameter_count;
	uint32_t constructor_offset = (uint32_t)target_type_declarations->constructor_count;
	uint32_t field_type_offset = (uint32_t)target_type_declarations->readback_field_type_count;
	uint32_t expr_offset = (uint32_t)target_type_declarations->expr_count;
	uint32_t binder_offset = target_terms->next_binder_id;
	uint32_t universe_offset = target_type_declarations->next_level_var;
	uint32_t target_representation_anchors[512];
	uint32_t source_representation_anchors[512];
	size_t old_target_representation_count = target_type_declarations->representation_count;
	size_t source_representation_count = source_type_declarations->representation_count;
	if (old_target_representation_count > 512 || source_representation_count > 512) {
		return -1;
	}
	for (uint32_t i = 0; i < old_target_representation_count; ++i) {
		target_representation_anchors[i] =
			target_type_declarations->representations[i].representative_type_id;
	}
	for (uint32_t i = 0; i < source_representation_count; ++i) {
		source_representation_anchors[i] =
			source_type_declarations->representations[i].representative_type_id;
	}

	if (target_terms->term_count + source_terms->term_count > target_terms->term_capacity ||
		target_terms->case_count + source_terms->case_count > target_terms->case_capacity ||
		target_terms->case_binder_count + source_terms->case_binder_count > target_terms->case_binder_capacity ||
		target_terms->match_frame_count + source_terms->match_frame_count > target_terms->match_frame_capacity ||
		target_type_declarations->type_count + source_type_declarations->type_count > target_type_declarations->type_capacity ||
		target_type_declarations->parameter_count + source_type_declarations->parameter_count > target_type_declarations->parameter_capacity ||
		target_type_declarations->constructor_count + source_type_declarations->constructor_count > target_type_declarations->constructor_capacity ||
		target_type_declarations->readback_field_type_count + source_type_declarations->readback_field_type_count > target_type_declarations->readback_field_type_capacity ||
		target_type_declarations->expr_count + source_type_declarations->expr_count > target_type_declarations->expr_capacity ||
		target_judgement->relation_count + source_judgement->relation_count > target_judgement->relation_capacity ||
		target_judgement->proof_count + source_judgement->proof_count > target_judgement->proof_capacity ||
		source_interface->term_export_count > appended_interface->term_export_capacity ||
		source_interface->type_export_count > appended_interface->type_export_capacity ||
		source_interface->type_parameter_count > appended_interface->type_parameter_capacity ||
		source_interface->constructor_export_count > appended_interface->constructor_export_capacity ||
		source_interface->constructor_field_type_expr_count >
			appended_interface->constructor_field_type_expr_capacity ||
		source_interface->type_expr_count > appended_interface->type_expr_capacity ||
		source_interface->dependency_count > appended_interface->dependency_capacity) {
		return -1;
	}

	for (size_t i = 0; i < source_type_declarations->expr_count; ++i) {
		struct prototype_type_expr expr = source_type_declarations->exprs[i];
		if (artifact_type_expr_present(&expr)) {
			offset_artifact_type_expr(&expr, expr_offset, binder_offset, universe_offset);
		}
		target_type_declarations->exprs[target_type_declarations->expr_count++] = expr;
	}
	for (size_t i = 0; i < source_type_declarations->readback_field_type_count; ++i) {
		uint32_t field_type = source_type_declarations->readback_field_types[i];
		field_type = offset_artifact_id(field_type, expr_offset);
		target_type_declarations->readback_field_types[target_type_declarations->readback_field_type_count++] =
			field_type;
	}
	for (size_t i = 0; i < source_type_declarations->parameter_count; ++i) {
		struct prototype_type_parameter_declaration parameter =
			source_type_declarations->parameter_declarations[i];
		if (artifact_parameter_present(&parameter)) {
			parameter.binder_id =
				offset_artifact_binder_id(parameter.binder_id, binder_offset);
			parameter.type_expr = offset_artifact_id(parameter.type_expr, expr_offset);
		}
		target_type_declarations->parameter_declarations[target_type_declarations->parameter_count++] =
			parameter;
	}
	for (size_t i = 0; i < source_type_declarations->constructor_count; ++i) {
		struct prototype_type_constructor_declaration constructor =
			source_type_declarations->constructor_declarations[i];
		if (artifact_constructor_present(&constructor)) {
			constructor.owner_type = offset_artifact_id(constructor.owner_type, type_offset);
			constructor.readback.first_field_type =
				offset_artifact_id(constructor.readback.first_field_type, field_type_offset);
			constructor.readback.result_type =
				offset_artifact_id(constructor.readback.result_type, expr_offset);
			constructor.classifier_family =
				offset_artifact_id(constructor.classifier_family, term_offset);
		}
		target_type_declarations->constructor_declarations[target_type_declarations->constructor_count++] =
			constructor;
	}
	for (size_t i = 0; i < source_type_declarations->type_count; ++i) {
		struct prototype_type_declaration type =
			source_type_declarations->type_declarations[i];
		if (artifact_type_present(&type)) {
			type.type_index = offset_artifact_id(type.type_index, type_offset);
			type.representation_id = PROTOTYPE_INVALID_ID;
			type.formation_classifier =
				offset_artifact_id(type.formation_classifier, term_offset);
			type.first_parameter = offset_artifact_id(type.first_parameter, parameter_offset);
			type.first_constructor = offset_artifact_id(type.first_constructor, constructor_offset);
		}
		target_type_declarations->type_declarations[target_type_declarations->type_count++] = type;
	}
	target_type_declarations->representations_dirty = 1;

	for (size_t i = 0; i < source_terms->term_count; ++i) {
		struct prototype_term term = source_terms->terms[i];
		offset_artifact_term(&term, term_offset, case_offset, frame_offset, type_offset, binder_offset, universe_offset);
		target_terms->terms[target_terms->term_count++] = term;
	}
	for (size_t i = 0; i < source_terms->case_count; ++i) {
		struct prototype_match_case match_case = source_terms->cases[i];
		match_case.constructor_owner = offset_artifact_id(match_case.constructor_owner, term_offset);
		match_case.first_binder = offset_artifact_id(match_case.first_binder, case_binder_offset);
		match_case.body = offset_artifact_id(match_case.body, term_offset);
		target_terms->case_label_symbols[target_terms->case_count] =
			source_terms->case_label_symbols[i];
		target_terms->cases[target_terms->case_count++] = match_case;
	}
	for (size_t i = 0; i < source_terms->case_binder_count; ++i) {
		struct prototype_case_binder binder = source_terms->case_binders[i];
		binder.binder_id =
			offset_artifact_binder_id(binder.binder_id, binder_offset);
		target_terms->case_binders[target_terms->case_binder_count++] = binder;
	}
	for (size_t i = 0; i < source_terms->match_frame_count; ++i) {
		struct prototype_match_frame frame = source_terms->match_frames[i];
		frame.match_term = offset_artifact_id(frame.match_term, term_offset);
		frame.key.is_linkable = 0;
		target_terms->match_frames[target_terms->match_frame_count++] = frame;
	}

	if (prototype_type_declaration_rebuild_representations(
			target_terms,
			target_type_declarations
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < target_terms->term_count; ++i) {
		struct prototype_term* term = &target_terms->terms[i];
		if (term->tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t old_representation_id = term->as.type_former.representation_id;
		uint32_t anchor_type_id;
		if (i < term_offset) {
			if (old_representation_id >= old_target_representation_count ||
				old_representation_id >= 512) {
				return -1;
			}
			anchor_type_id = target_representation_anchors[old_representation_id];
		} else {
			if (old_representation_id >= source_representation_count ||
				old_representation_id >= 512) {
				return -1;
			}
			anchor_type_id = source_representation_anchors[old_representation_id] + type_offset;
		}
		if (anchor_type_id >= target_type_declarations->type_count ||
			target_type_declarations->type_declarations[anchor_type_id].representation_id ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		term->as.type_former.representation_id =
			target_type_declarations->type_declarations[anchor_type_id].representation_id;
	}

	uint32_t proof_offset = (uint32_t)target_judgement->proof_count;
	for (size_t i = 0; i < source_judgement->relation_count; ++i) {
		struct prototype_judgement_relation relation = source_judgement->relations[i];
		if (relation.kind != PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			relation.subject = offset_artifact_id(relation.subject, term_offset);
			relation.classifier = offset_artifact_id(relation.classifier, term_offset);
			relation.proof_id = offset_artifact_id(relation.proof_id, proof_offset);
		}
		target_judgement->relations[target_judgement->relation_count++] = relation;
	}
	for (size_t i = 0; i < source_judgement->proof_count; ++i) {
		struct prototype_judgement_proof proof = source_judgement->proofs[i];
		if (proof.proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			proof.conclusion_subject = offset_artifact_id(proof.conclusion_subject, term_offset);
			proof.conclusion_classifier = offset_artifact_id(proof.conclusion_classifier, term_offset);
			proof.context_subject = offset_artifact_id(proof.context_subject, term_offset);
			if (proof.context_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_LAMBDA_BINDER &&
				proof.context_index != PROTOTYPE_INVALID_ID) {
				proof.context_index += binder_offset;
			}
			for (uint32_t j = 0; j < proof.premise_count; ++j) {
				proof.premise_subjects[j] =
					offset_artifact_id(proof.premise_subjects[j], term_offset);
				proof.premise_classifiers[j] =
					offset_artifact_id(proof.premise_classifiers[j], term_offset);
				proof.premise_proof_ids[j] =
					offset_artifact_id(proof.premise_proof_ids[j], proof_offset);
			}
		}
		target_judgement->proofs[target_judgement->proof_count++] = proof;
	}

	target_terms->next_binder_id = binder_offset + source_terms->next_binder_id;
	target_type_declarations->next_level_var =
		universe_offset + source_type_declarations->next_level_var;
	if (target_judgement->next_universe_var < target_type_declarations->next_level_var) {
		target_judgement->next_universe_var = target_type_declarations->next_level_var;
	}
	sync_artifact_universe_level_counters(
		target_terms,
		target_type_declarations,
		target_judgement
	);

	if (canonicalize_type_view_core_refs(target_terms, target_type_declarations) != 0) {
		return -1;
	}
	if (canonicalize_constructor_owner_refs(
			target_terms,
			target_type_declarations,
			term_offset
		) != 0) {
		return -1;
	}

	appended_interface->term_export_count = source_interface->term_export_count;
	appended_interface->type_export_count = source_interface->type_export_count;
	appended_interface->type_parameter_count = source_interface->type_parameter_count;
	appended_interface->constructor_export_count = source_interface->constructor_export_count;
	appended_interface->constructor_field_type_expr_count =
		source_interface->constructor_field_type_expr_count;
	appended_interface->type_expr_count = source_interface->type_expr_count;
	appended_interface->dependency_count = source_interface->dependency_count;
	for (size_t i = 0; i < source_interface->type_expr_count; ++i) {
		appended_interface->type_exprs[i] = source_interface->type_exprs[i];
	}
	for (size_t i = 0; i < source_interface->type_parameter_count; ++i) {
		appended_interface->type_parameters[i] = source_interface->type_parameters[i];
	}
	for (size_t i = 0; i < source_interface->constructor_field_type_expr_count; ++i) {
		appended_interface->constructor_field_type_exprs[i] =
			source_interface->constructor_field_type_exprs[i];
	}
	for (size_t i = 0; i < source_interface->term_export_count; ++i) {
		appended_interface->term_exports[i] = source_interface->term_exports[i];
		appended_interface->term_exports[i].local_term += term_offset;
		if (appended_interface->term_exports[i].classifier != PROTOTYPE_INVALID_ID) {
			appended_interface->term_exports[i].classifier += term_offset;
		}
		if (prototype_term_canonical_key_with_types(
				target_terms,
				target_type_declarations,
				appended_interface->term_exports[i].local_term,
				&appended_interface->term_exports[i].canonical_key
			) != 0) {
			return -1;
		}
		memset(
			&appended_interface->term_exports[i].classifier_key,
			0,
			sizeof(appended_interface->term_exports[i].classifier_key)
		);
		if (appended_interface->term_exports[i].classifier != PROTOTYPE_INVALID_ID &&
			(appended_interface->term_exports[i].classifier >= target_terms->term_count ||
				prototype_term_canonical_key_with_types(
					target_terms,
					target_type_declarations,
					appended_interface->term_exports[i].classifier,
					&appended_interface->term_exports[i].classifier_key
				) != 0)) {
			return -1;
		}
		if (appended_interface->term_exports[i].classifier != PROTOTYPE_INVALID_ID) {
			uint32_t existing_classifier;
			int found_existing_classifier = find_existing_term_by_canonical_key(
				target_terms,
				target_type_declarations,
				term_offset,
				&appended_interface->term_exports[i].classifier_key,
				appended_interface->term_exports[i].classifier,
				&existing_classifier
			);
			if (found_existing_classifier < 0) {
				return -1;
			}
			if (found_existing_classifier > 0) {
				appended_interface->term_exports[i].classifier = existing_classifier;
				if (prototype_term_canonical_key_with_types(
						target_terms,
						target_type_declarations,
						existing_classifier,
						&appended_interface->term_exports[i].classifier_key
					) != 0) {
					return -1;
				}
			}
		}
		uint32_t existing_term;
		int found_existing = find_existing_term_by_canonical_key(
			target_terms,
			target_type_declarations,
			term_offset,
			&appended_interface->term_exports[i].canonical_key,
			appended_interface->term_exports[i].local_term,
			&existing_term
		);
		if (found_existing < 0) {
			return -1;
		}
		if (found_existing > 0) {
			appended_interface->term_exports[i].local_term = existing_term;
			if (prototype_term_canonical_key_with_types(
					target_terms,
					target_type_declarations,
					existing_term,
					&appended_interface->term_exports[i].canonical_key
				) != 0) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < source_interface->type_export_count; ++i) {
		appended_interface->type_exports[i] = source_interface->type_exports[i];
		appended_interface->type_exports[i].local_type_id += type_offset;
		if (appended_interface->type_exports[i].core_representation_anchor_type_id != PROTOTYPE_INVALID_ID) {
			appended_interface->type_exports[i].core_representation_anchor_type_id += type_offset;
		}
		appended_interface->type_exports[i].formation_classifier =
			offset_artifact_id(
				appended_interface->type_exports[i].formation_classifier,
				term_offset
			);
			if (prototype_type_declaration_code_shape_key(
					target_terms,
					target_type_declarations,
					appended_interface->type_exports[i].local_type_id,
					&appended_interface->type_exports[i].code_shape_key
				) != 0) {
				return -1;
			}
			if (prototype_type_declaration_representation_anchor_type_id(
					target_terms,
					target_type_declarations,
					appended_interface->type_exports[i].local_type_id,
					&appended_interface->type_exports[i].core_representation_anchor_type_id
				) != 0) {
				return -1;
			}
	}
	for (size_t i = 0; i < source_interface->constructor_export_count; ++i) {
		appended_interface->constructor_exports[i] = source_interface->constructor_exports[i];
		if (appended_interface->constructor_exports[i].classifier_family !=
			PROTOTYPE_INVALID_ID) {
			appended_interface->constructor_exports[i].classifier_family += term_offset;
		}
	}
	for (size_t i = 0; i < source_interface->dependency_count; ++i) {
		appended_interface->dependencies[i] = source_interface->dependencies[i];
	}
	prototype_term_notify_graph_mutation(target_terms);
	return 0;
}

int prototype_canonical_link_table_find(
	const struct prototype_canonical_link_table* table,
	const struct prototype_term_canonical_key* key,
	uint32_t* p_entry
) {
	if (!table || !key || !p_entry) {
		return -1;
	}
	for (size_t i = 0; i < table->entry_count; ++i) {
		if (canonical_keys_equal(&table->entries[i].canonical_key, key)) {
			*p_entry = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_canonical_link_table_add_metadata(
	struct prototype_canonical_link_table* table,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	uint32_t unit_id,
	int reject_frame_local_references
) {
	if (!table || !terms || !metadata) {
		return -1;
	}
	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		if (label->term >= terms->term_count) {
			return -1;
		}
		if (label->canonical_key.free_binder_count != 0) {
			return -1;
		}
		if (reject_frame_local_references &&
			label->canonical_key.has_frame_local_reference) {
			return -1;
		}

		uint32_t representative = (uint32_t)table->entry_count;
		for (size_t j = 0; j < table->entry_count; ++j) {
			if (!canonical_keys_equal(&table->entries[j].canonical_key, &label->canonical_key)) {
				continue;
			}
			const struct prototype_canonical_link_entry* candidate = &table->entries[j];
			if (!candidate->terms || candidate->local_term >= candidate->terms->term_count) {
				return -1;
			}
			int same_term = 0;
				if (prototype_term_view_shape_equal_cross_db(
						candidate->terms,
						candidate->type_declarations,
						candidate->local_term,
					terms,
					type_declarations,
					label->term,
					1,
					&same_term
				) != 0) {
				return -1;
			}
			if (!same_term) {
				return -1;
			}
			representative = candidate->representative;
			break;
		}

		if (reserve_slot(table->entry_count, table->entry_capacity) != 0) {
			return -1;
		}
		uint32_t id = (uint32_t)table->entry_count;
		table->entries[id].unit_id = unit_id;
		table->entries[id].label_index = (uint32_t)i;
		table->entries[id].name_symbol_id = label->name_symbol_id;
		table->entries[id].terms = terms;
		table->entries[id].type_declarations = type_declarations;
		table->entries[id].local_term = label->term;
		table->entries[id].representative = representative;
		table->entries[id].canonical_key = label->canonical_key;
		table->entry_count++;
	}
	return 0;
}

void prototype_ast_db_init(
	struct prototype_ast_db* db,
	struct prototype_ast_node* nodes,
	size_t node_capacity,
	struct prototype_ast_type_expectation_def* expectations,
	size_t expectation_capacity,
	struct prototype_ast_term_assignment_def* assignments,
	size_t assignment_capacity,
	struct prototype_ast_import_def* imports,
	size_t import_capacity,
	struct prototype_ast_def_open_address_entry* def_index,
	size_t def_index_capacity,
	struct prototype_ast_match_case* cases,
	size_t case_capacity,
	struct prototype_ast_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_ast_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_ast_type_def* type_defs,
	size_t type_def_capacity,
	struct prototype_ast_type_parameter* type_parameters,
	size_t type_parameter_capacity,
	struct prototype_ast_type_constructor* type_constructors,
	size_t type_constructor_capacity,
	uint32_t* type_field_exprs,
	uint32_t* type_field_binder_ids,
	int* type_field_name_symbol_ids,
	size_t type_field_expr_capacity
) {
	memset(db, 0, sizeof(*db));
	db->nodes = nodes;
	db->node_capacity = node_capacity;
	db->expectations = expectations;
	db->expectation_capacity = expectation_capacity;
	db->assignments = assignments;
	db->assignment_capacity = assignment_capacity;
	db->imports = imports;
	db->import_capacity = import_capacity;
	db->def_index = def_index;
	db->def_index_capacity = def_index_capacity;
	db->cases = cases;
	db->case_capacity = case_capacity;
	db->case_binders = case_binders;
	db->case_binder_capacity = case_binder_capacity;
	db->type_exprs = type_exprs;
	db->type_expr_capacity = type_expr_capacity;
	db->type_defs = type_defs;
	db->type_def_capacity = type_def_capacity;
	db->type_parameters = type_parameters;
	db->type_parameter_capacity = type_parameter_capacity;
	db->type_constructors = type_constructors;
	db->type_constructor_capacity = type_constructor_capacity;
	db->type_field_exprs = type_field_exprs;
	db->type_field_binder_ids = type_field_binder_ids;
	db->type_field_name_symbol_ids = type_field_name_symbol_ids;
	db->type_field_expr_capacity = type_field_expr_capacity;
}

static int add_node(struct prototype_ast_db* db, struct prototype_ast_node node, uint32_t* p_ret) {
	if (!db || !p_ret || reserve_slot(db->node_count, db->node_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->node_count;
	db->nodes[id] = node;
	db->node_count++;
	*p_ret = id;
	return 0;
}

uint32_t prototype_ast_new_binder(struct prototype_ast_db* db) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_ast_binder_id++;
}

static int add_type_expr(
	struct prototype_ast_db* db,
	struct prototype_ast_type_expr expr,
	uint32_t* p_ret
) {
	if (!db || !p_ret || reserve_slot(db->type_expr_count, db->type_expr_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_expr_count;
	db->type_exprs[id] = expr;
	db->type_expr_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_type_expr_universe(
	struct prototype_ast_db* db,
	uint32_t level,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	expr.span = span;
	expr.as.universe.level = level;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_fresh_universe(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR;
	expr.span = span;
	expr.as.universe_var.level_var = db->next_ast_level_var++;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_self(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_SELF;
	expr.span = span;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_VAR;
	expr.span = span;
	expr.as.var.ast_binder_id = ast_binder_id;
	expr.as.var.symbol_id = symbol_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_NAME;
	expr.span = span;
	expr.as.name.symbol_id = symbol_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_host_type(
	struct prototype_ast_db* db,
	int host_type_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!prototype_term_host_type_source_name(host_type_id)) {
		return -1;
	}
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE;
	expr.span = span;
	expr.as.host_type.host_type_id = host_type_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || function >= db->type_expr_count || argument >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_APP;
	expr.span = span;
	expr.as.app.function = function;
	expr.as.app.argument = argument;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_arrow(
	struct prototype_ast_db* db,
	uint32_t domain,
	uint32_t codomain,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || domain >= db->type_expr_count || codomain >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_ARROW;
	expr.span = span;
	expr.as.arrow.domain = domain;
	expr.as.arrow.codomain = codomain;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_add(
	struct prototype_ast_db* db,
	int name_symbol_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_type_def_id
) {
	if (!db || !p_type_def_id || reserve_slot(db->type_def_count, db->type_def_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_def_count;
	struct prototype_ast_type_def* type = &db->type_defs[id];
	memset(type, 0, sizeof(*type));
	type->name_symbol_id = name_symbol_id;
	type->name_span = name_span;
	type->body_span = body_span;
	type->first_parameter = (uint32_t)db->type_parameter_count;
	type->first_constructor = (uint32_t)db->type_constructor_count;
	type->compiled_type = PROTOTYPE_INVALID_ID;
	db->type_def_count++;
	*p_type_def_id = id;
	return 0;
}

int prototype_ast_type_add_parameter(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	uint32_t ast_binder_id,
	int name_symbol_id,
	uint32_t type_expr
) {
	if (!db || ast_type_def_id >= db->type_def_count || type_expr >= db->type_expr_count) {
		return -1;
	}
	if (reserve_slot(db->type_parameter_count, db->type_parameter_capacity) != 0) {
		return -1;
	}

	struct prototype_ast_type_def* type = &db->type_defs[ast_type_def_id];
	if ((uint32_t)db->type_parameter_count != type->first_parameter + type->parameter_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_parameter_count;
	db->type_parameters[id].ast_binder_id = ast_binder_id;
	db->type_parameters[id].name_symbol_id = name_symbol_id;
	db->type_parameters[id].type_expr = type_expr;
	db->type_parameter_count++;
	type->parameter_count++;
	return 0;
}

int prototype_ast_type_add_constructor(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	int name_symbol_id,
	struct prototype_source_span name_span,
	const uint32_t* field_type_exprs,
	const uint32_t* field_binder_ids,
	const int* field_name_symbol_ids,
	uint32_t field_count,
	uint32_t result_type_expr
) {
	if (!db || ast_type_def_id >= db->type_def_count || result_type_expr >= db->type_expr_count) {
		return -1;
	}
	if (field_count > 0 && !field_type_exprs) {
		return -1;
	}
	if (reserve_slot(db->type_constructor_count, db->type_constructor_capacity) != 0) {
		return -1;
	}
	if (db->type_field_expr_count + field_count > db->type_field_expr_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		if (field_type_exprs[i] >= db->type_expr_count) {
			return -1;
		}
	}

	struct prototype_ast_type_def* type = &db->type_defs[ast_type_def_id];
	if ((uint32_t)db->type_constructor_count != type->first_constructor + type->constructor_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_constructor_count;
	struct prototype_ast_type_constructor* constructor = &db->type_constructors[id];
	memset(constructor, 0, sizeof(*constructor));
	constructor->name_symbol_id = name_symbol_id;
	constructor->name_span = name_span;
	constructor->first_field_type = (uint32_t)db->type_field_expr_count;
	constructor->field_count = field_count;
	constructor->result_type = result_type_expr;
	for (uint32_t i = 0; i < field_count; ++i) {
		uint32_t field_id = (uint32_t)db->type_field_expr_count++;
		db->type_field_exprs[field_id] = field_type_exprs[i];
		if (db->type_field_binder_ids) {
			db->type_field_binder_ids[field_id] =
				field_binder_ids ? field_binder_ids[i] : PROTOTYPE_INVALID_ID;
		}
		if (db->type_field_name_symbol_ids) {
			db->type_field_name_symbol_ids[field_id] =
				field_name_symbol_ids ? field_name_symbol_ids[i] : -1;
		}
	}
	db->type_constructor_count++;
	type->constructor_count++;
	return 0;
}

int prototype_ast_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_VAR;
	node.span = span;
	node.as.var.ast_binder_id = ast_binder_id;
	node.as.var.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME;
	node.span = span;
	node.as.name.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name_in_namespace(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME_IN_NAMESPACE;
	node.span = span;
	node.as.name_in_namespace.namespace_symbol_id = namespace_symbol_id;
	node.as.name_in_namespace.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name_in_ast_namespace(
	struct prototype_ast_db* db,
	uint32_t namespace_ast,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || namespace_ast >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME_IN_AST_NAMESPACE;
	node.span = span;
	node.as.name_in_ast_namespace.namespace_ast = namespace_ast;
	node.as.name_in_ast_namespace.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || function >= db->node_count || argument >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_APP;
	node.span = span;
	node.as.app.function = function;
	node.as.app.argument = argument;
	return add_node(db, node, p_ret);
}

int prototype_ast_lambda(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int binder_symbol_id,
	uint32_t binder_type,
	uint32_t body,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || body >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_LAMBDA;
	node.span = span;
	node.as.lambda.ast_binder_id = ast_binder_id;
	node.as.lambda.binder_symbol_id = binder_symbol_id;
	node.as.lambda.binder_type = binder_type;
	node.as.lambda.body = body;
	return add_node(db, node, p_ret);
}

int prototype_ast_match(
	struct prototype_ast_db* db,
	uint32_t scrutinee,
	const struct prototype_ast_match_case_input* cases,
	uint32_t case_count,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !cases || !p_ret || scrutinee >= db->node_count) {
		return -1;
	}
	if (db->case_count + case_count > db->case_capacity) {
		return -1;
	}

	size_t needed_binders = 0;
	for (uint32_t i = 0; i < case_count; ++i) {
		if (cases[i].body >= db->node_count) {
			return -1;
		}
		if (cases[i].binder_count > 0 && !cases[i].binders) {
			return -1;
		}
		needed_binders += cases[i].binder_count;
	}
	if (db->case_binder_count + needed_binders > db->case_binder_capacity) {
		return -1;
	}

	uint32_t first_case = (uint32_t)db->case_count;
	for (uint32_t i = 0; i < case_count; ++i) {
		struct prototype_ast_match_case* stored_case = &db->cases[db->case_count++];
		stored_case->constructor_symbol_id = cases[i].constructor_symbol_id;
		stored_case->first_binder = (uint32_t)db->case_binder_count;
		stored_case->binder_count = cases[i].binder_count;
		stored_case->body = cases[i].body;
		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			db->case_binders[db->case_binder_count++] = cases[i].binders[j];
		}
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_MATCH;
	node.span = span;
	node.as.match.scrutinee = scrutinee;
	node.as.match.first_case = first_case;
	node.as.match.case_count = case_count;
	return add_node(db, node, p_ret);
}

int prototype_ast_type_literal(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || ast_type_def_id >= db->type_def_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TYPE_LITERAL;
	node.span = span;
	node.as.type_literal.ast_type_def_id = ast_type_def_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_type_formation(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !p_ret || ast_type_def_id >= db->type_def_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TYPE_FORMATION;
	node.span = span;
	node.as.type_formation.ast_type_def_id = ast_type_def_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_induction_hypothesis(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_INDUCTION_HYPOTHESIS;
	node.span = span;
	node.as.induction_hypothesis.ast_binder_id = ast_binder_id;
	node.as.induction_hypothesis.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_text_literal(
	struct prototype_ast_db* db,
	int text_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TEXT_LITERAL;
	node.span = span;
	node.as.text_literal.text_symbol_id = text_symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_int_literal(
	struct prototype_ast_db* db,
	int64_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_INT_LITERAL;
	node.span = span;
	node.as.int_literal.value = value;
	return add_node(db, node, p_ret);
}

int prototype_ast_system_name(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	int type_symbol_id,
	int kind,
	int host_type_id,
	int operation_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_SYSTEM_NAME;
	node.span = span;
	node.as.system_name.namespace_symbol_id = namespace_symbol_id;
	node.as.system_name.symbol_id = symbol_id;
	node.as.system_name.type_symbol_id = type_symbol_id;
	node.as.system_name.kind = kind;
	node.as.system_name.host_type_id = host_type_id;
	node.as.system_name.operation_id = operation_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_ascription(
	struct prototype_ast_db* db,
	uint32_t term,
	uint32_t type_expr,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || term >= db->node_count || type_expr >= db->type_expr_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_ASCRIPTION;
	node.span = span;
	node.as.ascription.term = term;
	node.as.ascription.type_expr = type_expr;
	return add_node(db, node, p_ret);
}

static int prototype_ast_unary(
	struct prototype_ast_db* db,
	int tag,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || term >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = tag;
	node.span = span;
	node.as.unary.term = term;
	return add_node(db, node, p_ret);
}

int prototype_ast_return(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	return prototype_ast_unary(db, PROTOTYPE_AST_RETURN, term, span, p_ret);
}

int prototype_ast_thunk(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	return prototype_ast_unary(db, PROTOTYPE_AST_THUNK, term, span, p_ret);
}

int prototype_ast_force(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	return prototype_ast_unary(db, PROTOTYPE_AST_FORCE, term, span, p_ret);
}

int prototype_ast_perform(
	struct prototype_ast_db* db,
	uint32_t application,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	return prototype_ast_unary(db, PROTOTYPE_AST_PERFORM, application, span, p_ret);
}

int prototype_ast_handle(
	struct prototype_ast_db* db,
	uint32_t computation,
	uint32_t operation,
	uint32_t operation_argument_binder_id,
	int operation_argument_symbol_id,
	uint32_t operation_continuation_binder_id,
	int operation_continuation_symbol_id,
	uint32_t operation_body,
	uint32_t return_binder_id,
	int return_symbol_id,
	uint32_t return_body,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count || operation >= db->node_count ||
		operation_body >= db->node_count || return_body >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_HANDLE;
	node.span = span;
	node.as.handle.computation = computation;
	node.as.handle.operation = operation;
	node.as.handle.operation_argument_binder_id = operation_argument_binder_id;
	node.as.handle.operation_argument_symbol_id = operation_argument_symbol_id;
	node.as.handle.operation_continuation_binder_id = operation_continuation_binder_id;
	node.as.handle.operation_continuation_symbol_id = operation_continuation_symbol_id;
	node.as.handle.operation_body = operation_body;
	node.as.handle.return_binder_id = return_binder_id;
	node.as.handle.return_symbol_id = return_symbol_id;
	node.as.handle.return_body = return_body;
	return add_node(db, node, p_ret);
}

uint32_t prototype_ast_new_source_entry(struct prototype_ast_db* db) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_source_entry_id++;
}

static size_t def_index_hash(int symbol_id) {
	uint32_t x = (uint32_t)symbol_id;
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return (size_t)x;
}

static struct prototype_ast_def_open_address_entry* find_def_index_entry(
	struct prototype_ast_db* db,
	int symbol_id
) {
	if (!db || !db->def_index || db->def_index_capacity == 0) {
		return NULL;
	}
	size_t start = def_index_hash(symbol_id) % db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		struct prototype_ast_def_open_address_entry* entry = &db->def_index[index];
		if (!entry->occupied) {
			return NULL;
		}
		if (entry->symbol_id == symbol_id) {
			return entry;
		}
	}
	return NULL;
}

static struct prototype_ast_def_open_address_entry* find_or_add_def_index_entry(
	struct prototype_ast_db* db,
	int symbol_id
) {
	struct prototype_ast_def_open_address_entry* existing = find_def_index_entry(db, symbol_id);
	if (existing) {
		return existing;
	}
	if (!db || !db->def_index || reserve_slot(db->def_index_count, db->def_index_capacity) != 0) {
		return NULL;
	}
	size_t start = def_index_hash(symbol_id) % db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		struct prototype_ast_def_open_address_entry* entry = &db->def_index[index];
		if (entry->occupied) {
			continue;
		}
		memset(entry, 0, sizeof(*entry));
		entry->occupied = 1;
		entry->symbol_id = symbol_id;
		entry->first_expectation = PROTOTYPE_INVALID_ID;
		entry->first_assignment = PROTOTYPE_INVALID_ID;
		db->def_index_count++;
		return entry;
	}
	return NULL;
}

int prototype_ast_add_type_expectation(
	struct prototype_ast_db* db,
	int kind,
	int name_symbol_id,
	uint32_t type_expr,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span type_span,
	uint32_t paired_assignment_id,
	uint32_t* p_ret
) {
	if (!db || !p_ret || type_expr >= db->type_expr_count ||
		reserve_slot(db->expectation_count, db->expectation_capacity) != 0) {
		return -1;
	}
	if (kind != PROTOTYPE_AST_TYPE_ENTRY_DECLARATION &&
		kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION) {
		return -1;
	}
	struct prototype_ast_def_open_address_entry* symbol = find_or_add_def_index_entry(db, name_symbol_id);
	if (!symbol) {
		return -1;
	}

	uint32_t id = (uint32_t)db->expectation_count;
	memset(&db->expectations[id], 0, sizeof(db->expectations[id]));
	db->expectations[id].kind = kind;
	db->expectations[id].name_symbol_id = name_symbol_id;
	db->expectations[id].type_expr = type_expr;
	db->expectations[id].source_entry_id = source_entry_id;
	db->expectations[id].name_span = name_span;
	db->expectations[id].type_span = type_span;
	db->expectations[id].paired_assignment_id = paired_assignment_id;
	db->expectations[id].next_for_symbol = symbol->first_expectation;
	symbol->first_expectation = id;
	symbol->expectation_count++;
	db->expectation_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_add_term_assignment(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t ast,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_ret
) {
	if (!db || !p_ret || ast >= db->node_count ||
		reserve_slot(db->assignment_count, db->assignment_capacity) != 0) {
		return -1;
	}
	struct prototype_ast_def_open_address_entry* symbol = find_or_add_def_index_entry(db, name_symbol_id);
	if (!symbol) {
		return -1;
	}

	uint32_t id = (uint32_t)db->assignment_count;
	memset(&db->assignments[id], 0, sizeof(db->assignments[id]));
	db->assignments[id].name_symbol_id = name_symbol_id;
	db->assignments[id].ast = ast;
	db->assignments[id].source_entry_id = source_entry_id;
	db->assignments[id].name_span = name_span;
	db->assignments[id].body_span = body_span;
	db->assignments[id].next_for_symbol = symbol->first_assignment;
	db->assignments[id].compiled_term = PROTOTYPE_INVALID_ID;
	db->assignments[id].compiled_classifier = PROTOTYPE_INVALID_ID;
	symbol->first_assignment = id;
	symbol->assignment_count++;
	db->assignment_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_add_import(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span
) {
	if (!db || name_symbol_id < 0 ||
		reserve_slot(db->import_count, db->import_capacity) != 0) {
		return -1;
	}
	for (size_t i = 0; i < db->import_count; ++i) {
		if (db->imports[i].name_symbol_id == name_symbol_id) {
			return 0;
		}
	}
	uint32_t id = (uint32_t)db->import_count;
	memset(&db->imports[id], 0, sizeof(db->imports[id]));
	db->imports[id].name_symbol_id = name_symbol_id;
	db->imports[id].source_entry_id = source_entry_id;
	db->imports[id].name_span = name_span;
	db->import_count++;
	return 0;
}

int prototype_ast_pair_type_expectation(
	struct prototype_ast_db* db,
	uint32_t expectation_id,
	uint32_t assignment_id
) {
	if (!db || expectation_id >= db->expectation_count || assignment_id >= db->assignment_count) {
		return -1;
	}

	struct prototype_ast_type_expectation_def* expectation = &db->expectations[expectation_id];
	struct prototype_ast_term_assignment_def* assignment = &db->assignments[assignment_id];
	if (expectation->name_symbol_id != assignment->name_symbol_id ||
		expectation->source_entry_id != assignment->source_entry_id) {
		return -1;
	}
	expectation->paired_assignment_id = assignment_id;
	return 0;
}

const struct prototype_ast_term_assignment_def* prototype_ast_lookup_assignment_const(
	const struct prototype_ast_db* db,
	int name_symbol_id
) {
	if (!db) {
		return NULL;
	}

	for (size_t i = 0; i < db->def_index_capacity; ++i) {
		const struct prototype_ast_def_open_address_entry* entry = &db->def_index[i];
		if (!entry->occupied || entry->symbol_id != name_symbol_id) {
			continue;
		}
		if (entry->assignment_count != 1 || entry->first_assignment >= db->assignment_count) {
			return NULL;
		}
		return &db->assignments[entry->first_assignment];
	}
	return NULL;
}

static struct prototype_ast_term_assignment_def* lookup_unique_assignment_raw(
	struct prototype_ast_db* db,
	int name_symbol_id
) {
	if (!db) {
		return NULL;
	}

	struct prototype_ast_def_open_address_entry* entry = find_def_index_entry(db, name_symbol_id);
	if (!entry || entry->assignment_count != 1 || entry->first_assignment >= db->assignment_count) {
		return NULL;
	}
	return &db->assignments[entry->first_assignment];
}

static const struct prototype_ast_def_open_address_entry* lookup_def_index_entry_const(
	const struct prototype_ast_db* db,
	int name_symbol_id
) {
	if (!db || !db->def_index || db->def_index_capacity == 0) {
		return NULL;
	}
	size_t start = def_index_hash(name_symbol_id) % db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		const struct prototype_ast_def_open_address_entry* entry = &db->def_index[index];
		if (!entry->occupied) {
			return NULL;
		}
		if (entry->symbol_id == name_symbol_id) {
			return entry;
		}
	}
	return NULL;
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

struct match_frame_map_entry {
	uint32_t ast_binder_id;
	uint32_t frame_id;
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
	uint32_t binder_var;
	uint32_t classifier;
	uint32_t context_subject;
	uint32_t context_index;
	uint32_t context_aux;
};

struct pending_declaration_fact {
	uint32_t subject;
	uint32_t classifier;
};

enum compile_ref_polarity {
	COMPILE_REF_POLARITY_UNKNOWN = 0,
	COMPILE_REF_POLARITY_VALUE,
	COMPILE_REF_POLARITY_COMPUTATION
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

/* These binders exist only in classifiers. They quantify the latent effect
 * rows of function values mentioned by a lambda binder annotation. */
#define PROTOTYPE_JUDGEMENT_DELTA_CAPACITY 4096
#define PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY 16384
#define PROTOTYPE_OPERATION_MOTIVE_EQUATION_CAPACITY 4096
#define PROTOTYPE_OPERATION_SOLVER_INPUT_FACT_CAPACITY 2048

enum operation_classifier_constraint_kind {
	OPERATION_CONSTRAINT_HAS_TYPE = 1,
	OPERATION_CONSTRAINT_EQUAL,
	OPERATION_CONSTRAINT_CONVERTIBLE,
	OPERATION_CONSTRAINT_PI_EXPECTED,
	OPERATION_CONSTRAINT_MOTIVE_EQUATION,
	OPERATION_CONSTRAINT_IH_EXPECTED,
	OPERATION_CONSTRAINT_CBPV_BOUNDARY,
	OPERATION_CONSTRAINT_BIND_RESULT,
	OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT,
	OPERATION_CONSTRAINT_HANDLE_RESULT
};

enum operation_classifier_constraint_state {
	OPERATION_CONSTRAINT_STATE_PENDING = 0,
	OPERATION_CONSTRAINT_STATE_SOLVED,
	OPERATION_CONSTRAINT_STATE_RESIDUAL,
	OPERATION_CONSTRAINT_STATE_CONTRADICTION,
	OPERATION_CONSTRAINT_STATE_INCOMPLETE
};

enum operation_motive_solution_state {
	OPERATION_MOTIVE_SOLUTION_UNRESOLVED = 0,
	OPERATION_MOTIVE_SOLUTION_READY,
	OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE,
	OPERATION_MOTIVE_SOLUTION_MATERIALIZED
};

struct operation_classifier_constraint {
	uint32_t id;
	int kind;
	int state;
	uint32_t source_operation;
	uint32_t source_ast;
	uint32_t target;
	uint32_t left;
	uint32_t right;
	uint32_t aux;
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
 * Binder scope is copied here as data. It is not inferred later from a shared
 * core VAR node, whose identity is deliberately weaker than a source
 * occurrence identity.
 */
struct operation_motive_equation {
	uint32_t match_operation;
	uint32_t case_index;
	uint32_t body_operation;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t first_binder;
	uint32_t binder_count;
	uint32_t first_ast_binder;
	uint32_t ast_binder_count;
	uint32_t match_frame_id;
};

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
	struct operation_motive_equation
		motive_equations[PROTOTYPE_OPERATION_MOTIVE_EQUATION_CAPACITY];
	uint32_t motive_equation_count;
	struct operation_classifier_constraint
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

struct compile_context {
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_judgement_delta judgement_delta;
	struct prototype_judgement_relation judgement_delta_relations[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_proof judgement_delta_proofs[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_match_motive_result
		judgement_delta_match_motive_results[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_computation_constraint
		judgement_delta_computation_constraints[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_judgement_effect_row_equation
		judgement_delta_effect_row_equations[PROTOTYPE_JUDGEMENT_DELTA_CAPACITY];
	struct prototype_compile_metadata* metadata;
	struct operation_classifier_solver classifier_solver;
	struct operation_classifier_constraint
		classifier_constraint_blueprint[PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY];
	uint32_t classifier_constraint_blueprint_count;
	int classifier_constraints_generated;
	struct binder_map_entry binders[512];
	uint32_t binder_count;
	struct match_frame_map_entry match_frames[512];
	uint32_t match_frame_count;
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
	/* This controls surface elaboration only. It never changes TermDB
	 * reduction or the classifier solver's conversion relation. */
	int automatic_cbpv_coercions;
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
static int compile_ast_handle_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_HANDLE) {
		return -1;
	}
	struct compile_ref computation;
	struct compile_ref operation;
	struct compile_ref operation_body;
	struct compile_ref return_body;
	uint32_t operation_argument_binder;
	uint32_t operation_continuation_binder;
	uint32_t return_binder;
	uint32_t operation_inner_lambda;
	uint32_t operation_clause;
	uint32_t return_clause;
	uint32_t handler;
	uint32_t term;
	uint32_t saved_binder_count = ctx->binder_count;
	if (compile_ast_computation_ref(ctx, node->as.handle.computation, &computation) != 0 ||
		compile_ast_ref(ctx, node->as.handle.operation, &operation) != 0 ||
		push_graph_binder(
			ctx, node->as.handle.operation_argument_binder_id, PROTOTYPE_INVALID_ID,
			node->as.handle.operation_argument_symbol_id, &operation_argument_binder
		) != 0 || push_graph_binder(
			ctx, node->as.handle.operation_continuation_binder_id, PROTOTYPE_INVALID_ID,
			node->as.handle.operation_continuation_symbol_id, &operation_continuation_binder
		) != 0 || compile_ast_computation_ref(
			ctx, node->as.handle.operation_body, &operation_body
		) != 0 ||
		prototype_term_lambda(
			ctx->terms, operation_continuation_binder, operation_body.term, &operation_inner_lambda
		) != 0 || prototype_term_lambda(
			ctx->terms, operation_argument_binder, operation_inner_lambda, &operation_clause
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (push_graph_binder(
			ctx, node->as.handle.return_binder_id, PROTOTYPE_INVALID_ID,
			node->as.handle.return_symbol_id, &return_binder
		) != 0 || compile_ast_computation_ref(
			ctx, node->as.handle.return_body, &return_body
		) != 0 ||
		prototype_term_lambda(ctx->terms, return_binder, return_body.term, &return_clause) != 0 ||
		prototype_term_handler(ctx->terms, operation.term, return_clause, operation_clause, &handler) != 0 ||
		prototype_term_handle(ctx->terms, handler, computation.term, &term) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
		ctx, PROTOTYPE_OPERATION_HANDLE, term, PROTOTYPE_INVALID_ID, ast_id,
		computation.operation, operation.operation, operation_body.operation,
		return_body.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, &p_ret->operation
	) != 0 || operation_clause >= ctx->terms->term_count ||
		return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[operation_clause].tag != PROTOTYPE_TERM_LAMBDA ||
		ctx->terms->terms[return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* outer_operation_clause =
		&ctx->terms->terms[operation_clause];
	uint32_t canonical_inner_clause = outer_operation_clause->as.lambda.body;
	if (canonical_inner_clause >= ctx->terms->term_count ||
		ctx->terms->terms[canonical_inner_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	struct prototype_operation_node* handle_operation =
		&ctx->metadata->operations[p_ret->operation];
	handle_operation->handler_argument_ast_binder_id =
		node->as.handle.operation_argument_binder_id;
	handle_operation->handler_argument_binder_id =
		outer_operation_clause->as.lambda.binder_id;
	handle_operation->handler_continuation_ast_binder_id =
		node->as.handle.operation_continuation_binder_id;
	handle_operation->handler_continuation_binder_id =
		ctx->terms->terms[canonical_inner_clause].as.lambda.binder_id;
	handle_operation->handler_return_ast_binder_id = node->as.handle.return_binder_id;
	handle_operation->handler_return_binder_id =
		ctx->terms->terms[return_clause].as.lambda.binder_id;
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
static int find_unique_synthetic_classifier_for_label(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t* p_classifier
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
		ctx->asts->type_exprs[expectation->type_expr].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW;
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

static int find_local_term_by_key(
	const struct compile_context* ctx,
	const struct prototype_term_canonical_key* key,
	uint32_t* p_term
) {
	if (!ctx || !key || !p_term) {
		return -1;
	}
	if (key->node_count == 0 || !canonical_key_is_cross_artifact_linkable(key)) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)ctx->terms->term_count; ++i) {
		struct prototype_term_canonical_key candidate;
		if (prototype_term_canonical_key_with_types(
				ctx->terms,
				ctx->type_declarations,
				i,
				&candidate
			) != 0) {
			return -1;
		}
		if (canonical_keys_equal(&candidate, key)) {
			*p_term = i;
			return 0;
		}
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
		 * prototype_artifact_append_graph.  Its classifier ID is therefore the
		 * authoritative reference.  A canonical-key lookup is only a fallback:
		 * classifier families can contain bound/type-local references and are not
		 * necessarily cross-artifact-linkable keys. */
		if (interface->term_exports[export_id].classifier != PROTOTYPE_INVALID_ID &&
			interface->term_exports[export_id].classifier < ctx->terms->term_count &&
			artifact_term_present(
				&ctx->terms->terms[interface->term_exports[export_id].classifier]
			)) {
			*p_classifier = interface->term_exports[export_id].classifier;
			return 0;
		}
		return find_local_term_by_key(
			ctx,
			&interface->term_exports[export_id].classifier_key,
			p_classifier
		);
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
	struct prototype_term_canonical_key representative_keys[1024];
	struct prototype_qualified_name representative_names[1024];
	size_t representative_count = 0;
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

			struct prototype_qualified_name representative_name = qualified_name_make(
				export->namespace_symbol_id,
				export->name_symbol_id
			);
			int found_representative = 0;
			for (size_t k = 0; k < representative_count; ++k) {
				if (canonical_keys_equal(
						&representative_keys[k],
						&export->canonical_key
					)) {
					representative_name = representative_names[k];
					found_representative = 1;
					break;
				}
			}
			if (!found_representative) {
				if (representative_count >= 1024) {
					return -1;
				}
				representative_keys[representative_count] = export->canonical_key;
				representative_names[representative_count] = representative_name;
				representative_count++;
			}

			uint32_t local_term;
			if (export->local_term < ctx->terms->term_count) {
				local_term = export->local_term;
			} else if (find_local_term_by_key(
					ctx, &export->canonical_key, &local_term
				) != 0) {
				local_term = PROTOTYPE_INVALID_ID;
			}
			if (local_term != PROTOTYPE_INVALID_ID) {
				if (definition_count >= definition_capacity) {
					return -1;
				}
					definitions[definition_count].name = qualified_name_make(
						export->namespace_symbol_id,
						export->name_symbol_id
					);
				definitions[definition_count].term = local_term;
				definitions[definition_count].classifier = PROTOTYPE_INVALID_ID;
				definitions[definition_count].transparency =
					PROTOTYPE_TERM_DEFINITION_TRANSPARENT;
				definitions[definition_count].canonical_key = export->canonical_key;
				definition_count++;
				continue;
			}

			uint32_t representative_term;
			if (prototype_term_external_ref(
					ctx->terms,
					representative_name,
					&representative_term
				) != 0) {
				return -1;
			}
			if (qualified_names_equal(
				representative_name,
				qualified_name_make(export->namespace_symbol_id, export->name_symbol_id)
			)) {
				continue;
			}
			if (definition_count >= definition_capacity) {
				return -1;
			}
					definitions[definition_count].name = qualified_name_make(
						export->namespace_symbol_id,
						export->name_symbol_id
					);
			definitions[definition_count].term = representative_term;
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
	return operation_add(
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
	);
}

static int graph_classifier_list_contains_normalization_equal(
	struct compile_context* ctx,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
) {
	for (uint32_t i = 0; i < classifier_count; ++i) {
		if (prototype_judgement_classifier_normalization_equal(
				ctx->terms,
				ctx->type_declarations,
				classifiers[i],
				candidate
			)) {
			return 1;
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
		const struct prototype_judgement_relation* relations =
			source == 0 ? ctx->judgement_delta.relations : ctx->judgement->relations;
		size_t relation_count =
			source == 0 ? ctx->judgement_delta.relation_count : ctx->judgement->relation_count;
		for (size_t i = 0; i < relation_count; ++i) {
			const struct prototype_judgement_relation* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != term ||
				relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
				continue;
			}
			if (graph_classifier_list_contains_normalization_equal(
					ctx,
					classifiers,
					*p_classifier_count,
					relation->classifier
				)) {
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

static int automatic_cbpv_coercions_enabled(const struct compile_context* ctx) {
	return ctx && ctx->automatic_cbpv_coercions;
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
		core_term >= ctx->terms->term_count ||
		ctx->metadata->operation_count >= ctx->metadata->operation_capacity) {
		return -1;
	}
	uint32_t operation = (uint32_t)ctx->metadata->operation_count++;
	struct prototype_operation_node* node = &ctx->metadata->operations[operation];
	memset(node, 0, sizeof(*node));
	node->tag = tag;
	node->core_term = core_term;
	node->known_classifier = classifier;
	node->classifier = PROTOTYPE_INVALID_ID;
	node->classifier_variable = operation;
	node->source_ast = source_ast;
	node->source_symbol_id = -1;
	node->binder_symbol_id = -1;
	node->referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
	node->function = function;
	node->argument = argument;
	node->body = body;
	node->scrutinee = scrutinee;
	node->binder_classifier = binder_classifier;
	node->handler_argument_ast_binder_id = PROTOTYPE_INVALID_ID;
	node->handler_argument_binder_id = PROTOTYPE_INVALID_ID;
	node->handler_continuation_ast_binder_id = PROTOTYPE_INVALID_ID;
	node->handler_continuation_binder_id = PROTOTYPE_INVALID_ID;
	node->handler_return_ast_binder_id = PROTOTYPE_INVALID_ID;
	node->handler_return_binder_id = PROTOTYPE_INVALID_ID;
	node->first_case = first_case;
	node->case_count = case_count;
	*p_operation = operation;
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
	uint32_t* p_case
) {
	if (!ctx || !ctx->metadata || !p_case ||
		ctx->metadata->operation_case_count >= ctx->metadata->operation_case_capacity) {
		return -1;
	}
	uint32_t case_id = (uint32_t)ctx->metadata->operation_case_count++;
	struct prototype_operation_match_case* match_case =
		&ctx->metadata->operation_cases[case_id];
	match_case->body_operation = body_operation;
	match_case->constructor_owner = constructor_owner;
	match_case->constructor_id = constructor_id;
	match_case->case_label_symbol_id = -1;
	*p_case = case_id;
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
	if (prototype_term_whnf_with_profile(
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
	if (prototype_term_whnf_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			pi->as.pi.domain,
			&expected_domain
		) != 0 || expected_domain >= ctx->terms->term_count) {
		return -1;
	}
	/* Type families are raw CBPV computations. A Pi domain is a value-type
	 * position, so read a pure RETURN(T) as T after type-level reduction. */
	if (ctx->terms->terms[expected_domain].tag == PROTOTYPE_TERM_RETURN) {
		expected_domain = ctx->terms->terms[expected_domain].as.return_term.value;
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
	uint32_t binder_id;
	uint32_t body;
	if (prototype_term_pure_family_parts(
			ctx->terms,
			pi->as.pi.codomain_family,
			&binder_id,
			&body
		) != 0) {
		return -1;
	}
	(void)family_id;
	return prototype_term_substitute(
		ctx->terms,
		ctx->type_declarations,
		body,
		binder_id,
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
	uint32_t binder_id;
	uint32_t body;
	if (!ctx || !p_classifier || function_classifier == PROTOTYPE_INVALID_ID ||
		function_classifier >= ctx->terms->term_count || argument_term >= ctx->terms->term_count ||
		prototype_term_whnf_with_profile(
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
			&binder_id,
			&body
		) != 0) {
		return -1;
	}
	return prototype_term_substitute(
		ctx->terms, ctx->type_declarations, body, binder_id, argument_term, p_classifier
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
		if (prototype_term_whnf_with_profile(
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
		uint32_t binder_id;
		uint32_t body;
		if (prototype_term_pure_family_parts(
					ctx->terms,
					pi->as.pi.codomain_family,
					&binder_id,
					&body
				) != 0) {
			return -1;
		}
		(void)family_id;
		if (
			prototype_term_substitute(
				ctx->terms,
				ctx->type_declarations,
				body,
				binder_id,
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
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_BIND ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_OPERATION_REQUEST ||
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_HANDLE ?
			COMPILE_REF_POLARITY_COMPUTATION : COMPILE_REF_POLARITY_VALUE;
	p_ref->computation_kind =
		ctx->terms->terms[term].tag == PROTOTYPE_TERM_RETURN ?
			COMPILE_REF_COMPUTATION_KIND_RETURNING :
			ctx->terms->terms[term].tag == PROTOTYPE_TERM_LAMBDA ?
				COMPILE_REF_COMPUTATION_KIND_FUNCTION :
				COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (ctx->terms->terms[term].tag == PROTOTYPE_TERM_OPERATION) {
		if (prototype_judgement_operation_classifier(
				ctx->terms,
				ctx->type_declarations,
				&ctx->terms->terms[term],
				&p_ref->classifier
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
			!prototype_judgement_classifier_normalization_equal(
				ctx->terms,
				ctx->type_declarations,
				selected_owner,
				candidate.constructor_owner
			)) {
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

static int lookup_match_frame_id(
	const struct compile_context* ctx,
	uint32_t ast_binder_id,
	uint32_t* p_frame_id
) {
	if (!ctx || !p_frame_id) {
		return -1;
	}
	for (uint32_t i = ctx->match_frame_count; i > 0; --i) {
		const struct match_frame_map_entry* entry = &ctx->match_frames[i - 1];
		if (entry->ast_binder_id == ast_binder_id) {
			*p_frame_id = entry->frame_id;
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
		constructor_export->classifier_family = constructor->classifier_family;
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
	uint32_t binder_var,
	uint32_t classifier,
	uint32_t context_subject,
	uint32_t context_index,
	uint32_t context_aux
) {
	if (!ctx ||
		binder_var >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count ||
		context_subject >= ctx->terms->term_count ||
		ctx->pending_binder_assumption_count >= 1024) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
		if (pending->binder_var == binder_var &&
			pending->classifier == classifier &&
			pending->context_subject == context_subject &&
			pending->context_index == context_index &&
			pending->context_aux == context_aux) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_binder_assumption_count++;
	ctx->pending_binder_assumptions[id].binder_var = binder_var;
	ctx->pending_binder_assumptions[id].classifier = classifier;
	ctx->pending_binder_assumptions[id].context_subject = context_subject;
	ctx->pending_binder_assumptions[id].context_index = context_index;
	ctx->pending_binder_assumptions[id].context_aux = context_aux;
	return 0;
}

static int materialize_pending_binder_assumptions(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
		int already_materialized = 0;
		for (size_t relation_id = 0;
			relation_id < ctx->judgement_delta.relation_count;
			++relation_id) {
			const struct prototype_judgement_relation* relation =
				&ctx->judgement_delta.relations[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != pending->binder_var ||
				relation->classifier != pending->classifier ||
				relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
				relation->proof_id >= ctx->judgement_delta.proof_count) {
				continue;
			}
			const struct prototype_judgement_proof* proof =
				&ctx->judgement_delta.proofs[relation->proof_id];
			if (proof->context_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_LAMBDA_BINDER &&
				proof->context_subject == pending->context_subject &&
				proof->context_index == pending->context_index &&
				proof->context_aux == pending->context_aux) {
				already_materialized = 1;
				break;
			}
		}
		if (already_materialized) {
			continue;
		}
		size_t before = ctx->judgement_delta.relation_count;
		if (prototype_judgement_delta_expand_lambda_binder(
				&ctx->judgement_delta,
				ctx->terms,
				pending->binder_var,
				pending->classifier
			) != 0 ||
			ctx->judgement_delta.relation_count == 0) {
			return -1;
		}
		uint32_t proof_id =
			ctx->judgement_delta.relations[ctx->judgement_delta.relation_count - 1].proof_id;
		if (ctx->judgement_delta.relation_count == before) {
			continue;
		}
		if (prototype_judgement_delta_set_proof_context_by_id(
				&ctx->judgement_delta,
				proof_id,
				PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_LAMBDA_BINDER,
				pending->context_subject,
				pending->context_index,
				pending->context_aux
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int queue_declaration_fact(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier
) {
	if (!ctx ||
		subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count ||
		ctx->pending_declaration_fact_count >= 1024) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
		if (pending->subject == subject && pending->classifier == classifier) {
			return 0;
		}
	}
	uint32_t id = ctx->pending_declaration_fact_count++;
	ctx->pending_declaration_facts[id].subject = subject;
	ctx->pending_declaration_facts[id].classifier = classifier;
	return 0;
}

static int materialize_pending_declaration_facts(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
		if (prototype_judgement_delta_expand_checked(
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
		prototype_term_binder_for_scope_slot(ctx->terms, ctx->binder_count);
	if (graph_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	ctx->binders[ctx->binder_count].ast_binder_id = ast_binder_id;
	ctx->binders[ctx->binder_count].graph_binder_id = graph_binder_id;
	ctx->binders[ctx->binder_count].classifier = classifier;
	ctx->binders[ctx->binder_count].symbol_id = symbol_id;
	ctx->binder_count++;
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
			uint32_t whnf;
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.app.function, self_type, &function) != 0) {
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(ctx, expr.as.app.argument, self_type, &argument) != 0) {
				return -1;
			}
			if (compile_shared_app(ctx, function, argument, &application) != 0 ||
				prototype_term_whnf_with_profile(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
					application,
					&whnf
				) != 0 || whnf >= ctx->terms->term_count) {
				return -1;
			}
			*p_ret = ctx->terms->terms[whnf].tag == PROTOTYPE_TERM_RETURN ?
				ctx->terms->terms[whnf].as.return_term.value : whnf;
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
					ctx->terms, PROTOTYPE_HOST_EFFECT_NONE, &empty_effects
				) != 0 || prototype_term_computation_type(
					ctx->terms, empty_effects, codomain, &computation
				) != 0 || prototype_term_pi(
					ctx->terms, domain, computation, &function
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(ctx->terms, function, p_ret);
		}
		default:
			return -1;
	}
}

static int compile_ast_type_expr_term(struct compile_context* ctx, uint32_t type_expr, uint32_t* p_ret) {
	return compile_ast_type_expr_term_with_self(ctx, type_expr, PROTOTYPE_INVALID_ID, p_ret);
}

/* A source binder annotated with an arrow receives one implicit, scoped row
 * variable for the computation performed by that function value. The row is
 * generalized only by the enclosing lambda operation; it is never a runtime
 * argument and is not a free classifier variable. Nested arrow values retain
 * their own ordinary source annotation until nested effect polymorphism is
 * introduced deliberately. */
static int compile_ast_arrow_type_with_latent_effect_row(
	struct compile_context* ctx,
	uint32_t type_expr,
	uint32_t* p_row_binder,
	uint32_t* p_ret
) {
	if (!ctx || !p_row_binder || !p_ret || type_expr >= ctx->asts->type_expr_count ||
		ctx->asts->type_exprs[type_expr].tag != PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return -1;
	}
	const struct prototype_ast_type_expr* arrow = &ctx->asts->type_exprs[type_expr];
	uint32_t domain;
	uint32_t codomain;
	uint32_t row_binder;
	uint32_t row;
	uint32_t computation;
	uint32_t function;
	if (compile_ast_type_expr_term(ctx, arrow->as.arrow.domain, &domain) != 0 ||
		compile_ast_type_expr_term(ctx, arrow->as.arrow.codomain, &codomain) != 0 ||
		(row_binder = prototype_term_fresh_binder(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(ctx->terms, row_binder, &row) != 0 ||
		prototype_term_computation_type(ctx->terms, row, codomain, &computation) != 0 ||
		prototype_term_pi(ctx->terms, domain, computation, &function) != 0 ||
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
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return compile_ast_function_type_expr_term(ctx, type_expr, p_ret);
	}

	/* A non-arrow surface result is a value result of a computation.  Recursive
	 * arrows above remain negative computation types rather than becoming
	 * returned thunk values. */
	uint32_t result;
	uint32_t empty_effects;
	if (compile_ast_type_expr_term(ctx, type_expr, &result) != 0 ||
		prototype_term_effect_label(
			ctx->terms, PROTOTYPE_HOST_EFFECT_NONE, &empty_effects
		) != 0 ||
		prototype_term_computation_type(
			ctx->terms, empty_effects, result, p_ret
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
	if (expr->tag != PROTOTYPE_AST_TYPE_EXPR_ARROW) {
		return compile_ast_type_expr_term(ctx, type_expr, p_ret);
	}
	if (compile_ast_type_expr_term(ctx, expr->as.arrow.domain, &domain) != 0 ||
		compile_ast_function_result_type_expr_term(
			ctx, expr->as.arrow.codomain, &codomain
		) != 0) {
		return -1;
	}
	return prototype_term_pi(ctx->terms, domain, codomain, p_ret);
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
	return ctx->asts->type_exprs[type_expr].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ?
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
					expr->as.var.binder_id,
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
		source_binders[i] = parameter->binder_id;
		target_binders[i] = prototype_term_fresh_binder(ctx->terms);
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
	uint32_t frame_id;
	struct prototype_match_case_input case_inputs[64];
	uint32_t resolution_item_ids[64];
	int case_constructor_symbols[64];
	uint32_t branch_operations[64];
	int branch_computation_kinds[64];
	struct prototype_case_binder binder_storage[256];
	uint32_t binder_cursor;
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
	uint32_t previous_match_frame_count,
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
		if (ctx->match_frame_count >= 512) {
			ctx->binder_count = previous_binder_count;
			ctx->match_frame_count = previous_match_frame_count;
			return -1;
		}
		ctx->match_frames[ctx->match_frame_count].ast_binder_id =
			ast_binder->ast_binder_id;
		ctx->match_frames[ctx->match_frame_count].frame_id = state->frame_id;
		ctx->match_frame_count++;
		state->binder_storage[state->binder_cursor + j].binder_id = graph_binder_id;
	}
	return 0;
}

static int prepare_match_pattern_environment(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	uint32_t previous_binder_count,
	uint32_t previous_match_frame_count,
	uint32_t* p_binder_start
) {
	if (create_match_pattern_binders(
		ctx,
		state,
		old_case,
		previous_binder_count,
		previous_match_frame_count,
		p_binder_start
	) != 0) {
		return -1;
	}
	(void)previous_binder_count;
	(void)previous_match_frame_count;
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
	if (compile_ast_computation_ref(ctx, old_case->body, &body_ref) != 0 ||
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
	uint32_t previous_match_frame_count;
	uint32_t binder_start;
	uint32_t resolution_item_id;
	if (!ctx || !node || !state || case_index >= node->as.match.case_count) {
		return -1;
	}
	old_case = &ctx->asts->cases[node->as.match.first_case + case_index];
	previous_binder_count = ctx->binder_count;
	previous_match_frame_count = ctx->match_frame_count;
	if (add_match_constructor_resolution_item(
		ctx,
		state->match_ast,
		case_index,
		state->scrutinee,
		old_case->constructor_symbol_id,
		&resolution_item_id
	) != 0) {
		ctx->binder_count = previous_binder_count;
		ctx->match_frame_count = previous_match_frame_count;
		return -1;
	}
	state->resolution_item_ids[case_index] = resolution_item_id;
	state->case_constructor_symbols[case_index] = old_case->constructor_symbol_id;
	if (prepare_match_pattern_environment(
		ctx,
		state,
		old_case,
		previous_binder_count,
		previous_match_frame_count,
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
		ctx->match_frame_count = previous_match_frame_count;
		return -1;
	}
	ctx->binder_count = previous_binder_count;
	ctx->match_frame_count = previous_match_frame_count;
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

static int compile_ast_match_from_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_ref* scrutinee_ref,
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
	state.frame_id = PROTOTYPE_INVALID_ID;
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
	state.frame_id = prototype_term_new_match_frame(ctx->terms);
	if (state.frame_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (compile_match_branch(ctx, node, &state, i) != 0) {
			return -1;
		}
	}
	if (prototype_term_match_with_frame(
		ctx->terms,
		state.scrutinee,
		state.case_inputs,
		node->as.match.case_count,
		state.frame_id,
		&match_term
	) != 0) {
		return -1;
	}
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	if (prototype_term_set_match_frame_term(ctx->terms, state.frame_id, match_term) != 0) {
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
		) != 0 ||
		queue_match_typing(ctx, match_term, p_ref->operation, ctx->asts->next_ast_level_var++) != 0) {
		return -1;
	}
	return 0;
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

static int compile_constructor_classifier_family(
	struct compile_context* ctx,
	const struct prototype_ast_type_def* ast_type,
	uint32_t type_term,
	const uint32_t* field_type_terms,
	const uint32_t* field_binder_ids,
	uint32_t field_count,
	uint32_t* p_classifier_family
) {
	if (!ctx || !ast_type || !p_classifier_family ||
		(field_count > 0 && (!field_type_terms || !field_binder_ids))) {
		return -1;
	}

	uint32_t classifier = type_term;
	for (uint32_t i = field_count; i > 0; --i) {
		uint32_t codomain_family;
		uint32_t pi_classifier;
		if (prototype_term_pure_family(
				ctx->terms,
				field_binder_ids[i - 1],
				classifier,
				&codomain_family
			) != 0 ||
			prototype_term_pi_family(
				ctx->terms,
				field_type_terms[i - 1],
				codomain_family,
				&pi_classifier
			) != 0) {
			return -1;
		}
		classifier = pi_classifier;
	}

	for (uint32_t i = ast_type->parameter_count; i > 0; --i) {
		const struct prototype_ast_type_parameter* parameter =
			&ctx->asts->type_parameters[ast_type->first_parameter + i - 1];
		uint32_t graph_binder_id;
		uint32_t lambda;
		if (lookup_graph_binder(ctx, parameter->ast_binder_id, &graph_binder_id) != 0 ||
			prototype_term_lambda(ctx->terms, graph_binder_id, classifier, &lambda) != 0) {
			return -1;
		}
		classifier = lambda;
	}

	*p_classifier_family = classifier;
	return 0;
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
		uint32_t binder_id;
		uint32_t codomain_family;
		if (compile_ast_type_expr_term(ctx, parameter->type_expr, &domain) != 0 ||
			lookup_graph_binder(ctx, parameter->ast_binder_id, &binder_id) != 0 ||
			prototype_term_pure_family(
				ctx->terms, binder_id, classifier, &codomain_family
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

	int has_recursive_constructor_field = 0;
	int has_structural_seed_constructor = 0;
	for (uint32_t i = 0; i < ast_type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&ctx->asts->type_constructors[ast_type->first_constructor + i];
		uint32_t compiled_result_type;
		uint32_t compiled_field_types[64];
		uint32_t compiled_field_terms[64];
		uint32_t compiled_field_binder_ids[64];
		uint32_t constructor_id;
		uint32_t classifier_family;
		uint32_t previous_binder_count = ctx->binder_count;
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
			compiled_field_binder_ids[j] = PROTOTYPE_PI_UNUSED_BINDER_ID;
			if (ast_field_binder_id != PROTOTYPE_INVALID_ID) {
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
				compiled_field_binder_ids[j] = graph_binder_id;
			}
		}
		if (!constructor_has_recursive_field) {
			has_structural_seed_constructor = 1;
		}
		if (compile_constructor_classifier_family(
				ctx,
				ast_type,
				type_term,
				compiled_field_terms,
				compiled_field_binder_ids,
				constructor->field_count,
				&classifier_family
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
				classifier_family,
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

	return prototype_term_whnf_with_profile(
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
	uint32_t classifier = constructor->classifier_family;
	for (uint32_t i = 0; i < ignored_namespace_arg_count; ++i) {
		uint32_t applied;
		if (prototype_term_app(ctx->terms, classifier, ignored_namespace_args[i], &applied) != 0) {
			return -1;
		}
		classifier = applied;
	}
	if (prototype_term_whnf_with_profile(
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
				term->as.lambda.binder_id,
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
			return prototype_term_match_with_frame(
				terms,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				term->as.match.frame_id,
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
				term->as.induction_hypothesis.frame_id,
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
		case PROTOTYPE_TERM_BIND: {
			uint32_t computation;
			uint32_t continuation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.bind.computation, &computation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.bind.continuation, &continuation, depth + 1
				) != 0) {
				return -1;
			}
			return computation == term->as.bind.computation &&
				continuation == term->as.bind.continuation ?
				(*p_ret = term_id, 0) : prototype_term_bind(
					terms, computation, continuation, p_ret
				);
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
		case PROTOTYPE_TERM_HANDLER: {
			uint32_t operation;
			uint32_t return_clause;
			uint32_t operation_clause;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler.operation, &operation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler.return_clause, &return_clause, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler.operation_clause, &operation_clause, depth + 1
				) != 0) {
				return -1;
			}
			return operation == term->as.handler.operation &&
				return_clause == term->as.handler.return_clause &&
				operation_clause == term->as.handler.operation_clause ?
				(*p_ret = term_id, 0) : prototype_term_handler(
					terms, operation, return_clause, operation_clause, p_ret
				);
		}
		case PROTOTYPE_TERM_HANDLE: {
			uint32_t handler;
			uint32_t computation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handle.handler, &handler, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handle.computation, &computation, depth + 1
				) != 0) {
				return -1;
			}
			return handler == term->as.handle.handler && computation == term->as.handle.computation ?
				(*p_ret = term_id, 0) : prototype_term_handle(terms, handler, computation, p_ret);
		}
		case PROTOTYPE_TERM_HANDLER_TYPE: {
			uint32_t operation;
			uint32_t input;
			uint32_t output;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler_type.operation, &operation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler_type.input_computation, &input, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.handler_type.output_computation, &output, depth + 1
				) != 0) {
				return -1;
			}
			return operation == term->as.handler_type.operation &&
				input == term->as.handler_type.input_computation &&
				output == term->as.handler_type.output_computation ?
				(*p_ret = term_id, 0) : prototype_term_handler_type(terms, operation, input, output, p_ret);
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
					terms, term->as.effect_row_forall.binder_id, body, p_ret
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

static int imported_constructor_classifier_from_family(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_artifact_interface* interface,
	const struct prototype_artifact_type_export* type_export,
	const struct prototype_artifact_constructor_export* constructor_export,
	uint32_t owner,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !type_export || !constructor_export ||
		!p_classifier || constructor_export->classifier_family == PROTOTYPE_INVALID_ID ||
		constructor_export->classifier_family >= terms->term_count) {
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
	uint32_t classifier = constructor_export->classifier_family;
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}
	if (prototype_term_whnf_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0) {
		return -1;
	}
	/* constructor_export->classifier_family was relocated with the provider
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
		if (imported_constructor_classifier_from_family(
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
 * computation type such as Pi.  At a computation occurrence, `:: A` checks
 * `Comp({}, A)`; this is the expected-kind counterpart of implicit RETURN,
 * not a source-shape fallback. */
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
		uint32_t empty_effects;
		if (prototype_term_effect_label(
				ctx->terms, PROTOTYPE_HOST_EFFECT_NONE, &empty_effects
			) != 0 || prototype_term_computation_type(
				ctx->terms, empty_effects, surface_classifier, p_expected
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
				node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_HOST_OPERATION &&
				node->as.system_name.operation_id != PROTOTYPE_OPERATION_UNKNOWN
			) {
				if (prototype_term_operation(
						ctx->terms,
						node->as.system_name.operation_id,
						node->as.system_name.symbol_id,
						node->as.system_name.type_symbol_id,
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
			uint32_t binder_id;
			uint32_t frame_id;
			uint32_t argument_term;
			uint32_t argument_operation;
			uint32_t ih_term;
			if (lookup_graph_binder(
					ctx, node->as.induction_hypothesis.ast_binder_id, &binder_id
				) != 0 ||
				lookup_match_frame_id(
					ctx, node->as.induction_hypothesis.ast_binder_id, &frame_id
				) != 0 ||
				prototype_term_var(ctx->terms, binder_id, &argument_term) != 0 ||
				operation_add(
					ctx, PROTOTYPE_OPERATION_VAR, argument_term, PROTOTYPE_INVALID_ID,
					ast_id, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
					&argument_operation
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, frame_id, argument_term, &ih_term
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
				PROTOTYPE_INVALID_ID, frame_id, 0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_VAR: {
			uint32_t binder_id;
			uint32_t classifier;
			uint32_t term;
			if (lookup_graph_binder(ctx, node->as.var.ast_binder_id, &binder_id) != 0 ||
				prototype_term_var(ctx->terms, binder_id, &term) != 0) {
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
				uint32_t binder_id;
				uint32_t binder_classifier;
				if (push_graph_binder(
						ctx,
						parameter->ast_binder_id,
						PROTOTYPE_INVALID_ID,
						parameter->name_symbol_id,
						&binder_id
					) != 0 || compile_ast_type_expr_term(
						ctx, parameter->type_expr, &binder_classifier
					) != 0) {
					ctx->binder_count = previous_binder_count;
					return -1;
				}
				ctx->binders[ctx->binder_count - 1].classifier = binder_classifier;
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
};

static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_value_then(
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

static int compile_ast_operation_spine_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int ast_application_head_is_host_operation(
	const struct compile_context* ctx,
	uint32_t ast_id
);

static int ast_application_head_is_constructor(
	const struct compile_context* ctx,
	uint32_t ast_id
);

static int ast_application_is_bind_intrinsic(
	const struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t* p_computation_ast,
	uint32_t* p_continuation_ast
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
			prototype_term_whnf_with_profile(
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
	if (prototype_term_whnf_with_profile(
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
	/* A source `force f` is already an explicit CBPV boundary. When f has a
	 * known Thunk(Pi(...)) classifier, retain that negative shape so a following
	 * source APP applies it directly instead of sequencing or forcing again. */
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
	p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_THUNK, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, computation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_make_bind(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	const struct compile_ref* continuation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !computation || !continuation || !p_ret ||
		prototype_term_bind(
			ctx->terms, computation->term, continuation->term, &term
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_RETURNING;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_BIND, term, PROTOTYPE_INVALID_ID, source_ast,
		computation->operation, continuation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
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

static int compile_continue_computation(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	uint32_t binder_id;
	uint32_t variable_term;
	uint32_t lambda_term;
	struct compile_ref variable;
	struct compile_ref body;
	struct compile_ref lambda;
	if (!ctx || !computation || !continuation || !continuation->apply || !p_ret ||
		(binder_id = prototype_term_fresh_binder(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binder_id, &variable_term) != 0) {
		return -1;
	}
	variable.term = variable_term;
	variable.classifier = PROTOTYPE_INVALID_ID;
	variable.polarity = COMPILE_REF_POLARITY_VALUE;
	variable.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, variable_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &variable.operation
		) != 0 || continuation->apply(ctx, &variable, continuation->data, &body) != 0 ||
		body.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	if (prototype_term_lambda(ctx->terms, binder_id, body.term, &lambda_term) != 0) {
		return -1;
	}
	lambda.term = lambda_term;
	lambda.classifier = PROTOTYPE_INVALID_ID;
	lambda.polarity = COMPILE_REF_POLARITY_COMPUTATION;
	lambda.computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
			&lambda.operation
		) != 0) {
		return -1;
	}
	return compile_ref_make_bind(ctx, computation, &lambda, source_ast, p_ret);
}

static int term_app_head_is_constructor(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!terms || term_id >= terms->term_count) {
		return 0;
	}
	while (terms->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		term_id = terms->terms[term_id].as.app.function;
		if (term_id >= terms->term_count) {
			return 0;
		}
	}
	return terms->terms[term_id].tag == PROTOTYPE_TERM_CONSTRUCTOR;
}

static int ast_application_head_is_host_operation(
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
	const struct prototype_ast_node* head = &ctx->asts->nodes[ast_id];
	return head->tag == PROTOTYPE_AST_SYSTEM_NAME &&
		head->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_HOST_OPERATION;
}

static int ast_application_is_bind_intrinsic(
	const struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t* p_computation_ast,
	uint32_t* p_continuation_ast
) {
	if (!ctx || !ctx->asts || !p_computation_ast || !p_continuation_ast ||
		ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return 0;
	}
	const struct prototype_ast_node* outer = &ctx->asts->nodes[ast_id];
	if (outer->as.app.function >= ctx->asts->node_count ||
		ctx->asts->nodes[outer->as.app.function].tag != PROTOTYPE_AST_APP) {
		return 0;
	}
	const struct prototype_ast_node* inner =
		&ctx->asts->nodes[outer->as.app.function];
	if (inner->as.app.function >= ctx->asts->node_count) {
		return 0;
	}
	const struct prototype_ast_node* head =
		&ctx->asts->nodes[inner->as.app.function];
	if (head->tag != PROTOTYPE_AST_SYSTEM_NAME ||
		head->as.system_name.kind != PROTOTYPE_AST_SYSTEM_NAME_BIND) {
		return 0;
	}
	*p_computation_ast = inner->as.app.argument;
	*p_continuation_ast = outer->as.app.argument;
	return 1;
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

struct compile_application_function_context {
	uint32_t argument_ast;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

struct compile_application_argument_context {
	struct compile_ref function;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

static int compile_application_function_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
);

static int compile_ref_is_raw_function(
	const struct compile_context* ctx,
	const struct compile_ref* ref
) {
	(void)ctx;
	return ref && ref->polarity == COMPILE_REF_POLARITY_COMPUTATION &&
		ref->computation_kind == COMPILE_REF_COMPUTATION_KIND_FUNCTION;
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
		case PROTOTYPE_AST_LAMBDA:
		case PROTOTYPE_AST_NAME:
		case PROTOTYPE_AST_MATCH:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
			return compile_ast_computation_ref(ctx, ast_id, p_ret);
		default:
			return compile_ast_ref(ctx, ast_id, p_ret);
	}
}

static int compile_application_make_computation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	struct compile_ref applied_function;
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
		if (!automatic_cbpv_coercions_enabled(ctx)) {
			return -1;
		}
		return compile_ref_make_return(ctx, &value, source_ast, p_ret);
	}
	applied_function = *function;
	if (!compile_ref_is_raw_function(ctx, function)) {
		if (!automatic_cbpv_coercions_enabled(ctx) ||
			compile_ref_make_force(ctx, function, source_ast, &applied_function) != 0) {
			return -1;
		}
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
	p_ret->computation_kind = applied_function.computation_kind;
	return 0;
}

static int compile_application_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_application_argument_context* context = data;
	uint32_t term;
	struct compile_ref application;
	if (!ctx || !argument || !context || !context->continuation || !p_ret) {
		return -1;
	}
	if (term_app_head_is_constructor(ctx->terms, context->function.term)) {
		if (compile_shared_app(ctx, context->function.term, argument->term, &term) != 0) {
			return -1;
		}
		application.term = term;
		application.classifier = PROTOTYPE_INVALID_ID;
		application.polarity = COMPILE_REF_POLARITY_VALUE;
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
	if (compile_application_make_computation(
			ctx, &context->function, argument, context->source_ast, &application
		) != 0) {
		return -1;
	}
	return compile_continue_computation(
		ctx, &application, context->source_ast, context->continuation, p_ret
	);
}

struct compile_application_computation_context {
	struct compile_ref function;
	uint32_t source_ast;
};

struct compile_application_computation_function_context {
	uint32_t argument_ast;
	uint32_t source_ast;
};

static int compile_application_argument_computation_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_application_computation_context* context = data;
	if (!context) {
		return -1;
	}
	return compile_application_make_computation(
		ctx, &context->function, argument, context->source_ast, p_ret
	);
}

static int compile_application_function_computation_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_application_computation_function_context* function_context = data;
	struct compile_application_computation_context argument_context;
	struct compile_value_continuation argument_continuation;
	if (!function_context) {
		return -1;
	}
	argument_context.function = *function;
	argument_context.source_ast = function_context->source_ast;
	argument_continuation.apply = compile_application_argument_computation_continuation;
	argument_continuation.data = &argument_context;
	return compile_ast_value_then(
		ctx, function_context->argument_ast, &argument_continuation, p_ret
	);
}

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	const struct prototype_ast_node* node;
	struct compile_ref function;
	struct compile_application_computation_context context;
	struct compile_application_computation_function_context function_context;
	struct compile_value_continuation argument_continuation;
	struct compile_value_continuation function_continuation;
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	node = &ctx->asts->nodes[ast_id];
	if (compile_ast_function_ref(ctx, node->as.app.function, &function) != 0) {
		return -1;
	}
	if (!compile_ref_is_raw_function(ctx, &function)) {
		function_context.argument_ast = node->as.app.argument;
		function_context.source_ast = ast_id;
		function_continuation.apply = compile_application_function_computation_continuation;
		function_continuation.data = &function_context;
		return compile_ast_value_then(
			ctx, node->as.app.function, &function_continuation, p_ret
		);
	}
	context.function = function;
	context.source_ast = ast_id;
	argument_continuation.apply = compile_application_argument_computation_continuation;
	argument_continuation.data = &context;
	return compile_ast_value_then(
		ctx, node->as.app.argument, &argument_continuation, p_ret
	);
}

static int compile_application_function_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_application_function_context* context,
	struct compile_ref* p_ret
) {
	struct compile_ref function;
	struct compile_value_continuation continuation;
	if (!ctx || !context || !p_ret) {
		return -1;
	}
	if (compile_ast_function_ref(ctx, ast_id, &function) != 0) {
		return -1;
	}
	continuation.apply = compile_application_function_continuation;
	continuation.data = (void*)context;
	if (compile_ref_is_raw_function(ctx, &function)) {
		return compile_application_function_continuation(ctx, &function, (void*)context, p_ret);
	}
	return compile_ast_value_then(ctx, ast_id, &continuation, p_ret);
}

static int compile_application_function_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_application_function_context* context = data;
	struct compile_application_argument_context argument_context;
	struct compile_value_continuation continuation;
	if (!ctx || !function || !context || !p_ret) {
		return -1;
	}
	argument_context.function = *function;
	argument_context.source_ast = context->source_ast;
	argument_context.continuation = context->continuation;
	continuation.apply = compile_application_argument_continuation;
	continuation.data = &argument_context;
	return compile_ast_value_then(
		ctx, context->argument_ast, &continuation, p_ret
	);
}

static int compile_ast_lambda_computation_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t saved_binder_count;
	uint32_t binder_classifier;
	uint32_t binder_id;
	struct compile_ref body;
	uint32_t lambda_term;
	uint32_t lambda_operation;
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_LAMBDA) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	uint32_t latent_effect_row = PROTOTYPE_INVALID_ID;
	int binder_is_arrow = node->as.lambda.binder_type < ctx->asts->type_expr_count &&
		ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW;
	if ((binder_is_arrow ? compile_ast_arrow_type_with_latent_effect_row(
				ctx, node->as.lambda.binder_type, &latent_effect_row, &binder_classifier
			) : compile_ast_type_expr_term(
				ctx, node->as.lambda.binder_type, &binder_classifier
			)) != 0 ||
		push_graph_binder(
			ctx, node->as.lambda.ast_binder_id, binder_classifier,
			node->as.lambda.binder_symbol_id, &binder_id
		) != 0 || compile_ast_computation_ref(ctx, node->as.lambda.body, &body) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (body.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		prototype_term_lambda(ctx->terms, binder_id, body.term, &lambda_term) != 0) {
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
	if (latent_effect_row != PROTOTYPE_INVALID_ID) {
		ctx->metadata->operations[lambda_operation].implicit_effect_row_binders[0] =
			latent_effect_row;
		ctx->metadata->operations[lambda_operation].implicit_effect_row_count = 1;
	}
	uint32_t canonical_binder_id = ctx->terms->terms[lambda_term].as.lambda.binder_id;
	uint32_t binder_var;
	if (prototype_term_var(ctx->terms, canonical_binder_id, &binder_var) != 0 ||
		queue_binder_assumption(
			ctx,
			binder_var,
			binder_classifier,
			lambda_term,
			canonical_binder_id,
			lambda_operation
		) != 0) {
		return -1;
	}
	p_ret->operation = lambda_operation;
	return 0;
}

static int compile_ast_lambda_value_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	struct compile_ref lambda;
	if (!automatic_cbpv_coercions_enabled(ctx)) {
		return 1;
	}
	if (compile_ast_lambda_computation_ref(ctx, node, source_ast, &lambda) != 0) {
		return -1;
	}
	return compile_ref_make_thunk(ctx, &lambda, source_ast, p_ret);
}

/* Constructor application is a value spine.  This is deliberately decided
 * from the shared TermDB head after recursively lowering its value fields;
 * ordinary source application continues through FORCE/APP/BIND below. */
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
		return compile_ast_lambda_value_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		if (!ast_application_head_is_constructor(ctx, ast_id)) {
			return 1;
		}
		return compile_ast_constructor_application_value_ref(ctx, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_MATCH ||
		node->tag == PROTOTYPE_AST_RETURN || node->tag == PROTOTYPE_AST_FORCE ||
		node->tag == PROTOTYPE_AST_PERFORM ||
		node->tag == PROTOTYPE_AST_HANDLE ||
		node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return 1;
	}
	if (node->tag == PROTOTYPE_AST_THUNK) {
		struct compile_ref computation;
		uint32_t term;
		if (compile_ast_computation_ref(ctx, node->as.unary.term, &computation) != 0 ||
			computation.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
			prototype_term_thunk(ctx->terms, computation.term, &term) != 0) {
			return -1;
		}
		p_ret->term = term;
		p_ret->classifier = PROTOTYPE_INVALID_ID;
		p_ret->polarity = COMPILE_REF_POLARITY_VALUE;
		p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
		return operation_add(
			ctx, PROTOTYPE_OPERATION_THUNK, term, PROTOTYPE_INVALID_ID, ast_id,
			PROTOTYPE_INVALID_ID, computation.operation, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
			&p_ret->operation
		);
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref value;
		uint32_t expected;
		int status = compile_ast_value_ref(ctx, node->as.ascription.term, &value);
		if (status != 0) {
			return status;
		}
		if (compile_ast_type_expr_term(ctx, node->as.ascription.type_expr, &expected) != 0 ||
			queue_ascription_check(ctx, value.term, expected, ast_id, value.operation) != 0) {
			return -1;
		}
		*p_ret = value;
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
		p_ret->operation = operation;
		/* Host operation signatures are declaration facts. Handler and request
		 * constraints may need them in the same fixed-point round, before the
		 * generic operation materializer reaches this occurrence. */
		if (p_ret->term < ctx->terms->term_count &&
			ctx->terms->terms[p_ret->term].tag == PROTOTYPE_TERM_OPERATION &&
			prototype_judgement_delta_record_operation_type(
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
			/* Top-level definitions are compiled as raw CBPV functions.  A name
			 * used where a value is required denotes its suspension, so higher-
			 * order source programs do not need an explicit surface `thunk`. */
			if (p_ret->polarity == COMPILE_REF_POLARITY_COMPUTATION &&
					p_ret->computation_kind ==
						COMPILE_REF_COMPUTATION_KIND_FUNCTION) {
				if (!automatic_cbpv_coercions_enabled(ctx)) {
					return 1;
				}
				struct compile_ref raw_lambda = *p_ret;
				return compile_ref_make_thunk(ctx, &raw_lambda, ast_id, p_ret);
			}
			return p_ret->polarity == COMPILE_REF_POLARITY_COMPUTATION ? 1 : 0;
		default:
			return -1;
	}
}

static int compile_ast_value_then(
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
	compile_ref_clear(&value);
	status = compile_ast_value_ref(ctx, ast_id, &value);
	if (status == 0) {
		return continuation->apply(ctx, &value, continuation->data, p_ret);
	}
	if (status < 0) {
		return -1;
	}
	if (!automatic_cbpv_coercions_enabled(ctx)) {
		/* This is the implicit computation-to-value boundary. In raw CBPV the
		 * source program must introduce BIND before consuming a result value. */
		return -1;
	}
	/* An atomic source occurrence may resolve to a previously compiled
	 * computation definition.  It has already supplied the unique occurrence
	 * proof edge in value_ref; sequence that computation here instead of
	 * recursively trying to lower the same name as a computation. */
	if (value.polarity == COMPILE_REF_POLARITY_COMPUTATION) {
		return compile_continue_computation(ctx, &value, ast_id, continuation, p_ret);
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_APP) {
		if (ast_application_head_is_host_operation(ctx, ast_id)) {
			struct compile_ref operation_spine;
			if (compile_ast_operation_spine_computation_ref(
					ctx, ast_id, &operation_spine
				) != 0) {
				return -1;
			}
			return compile_continue_computation(
				ctx, &operation_spine, ast_id, continuation, p_ret
			);
		}
		if (ast_application_head_is_constructor(ctx, ast_id)) {
			int constructor_status = compile_ast_constructor_application_value_ref(
				ctx, ast_id, &value
			);
			if (constructor_status < 0) {
				return -1;
			}
			if (constructor_status == 0) {
				return continuation->apply(ctx, &value, continuation->data, p_ret);
			}
		}
		struct compile_application_function_context context = {
			node->as.app.argument, ast_id, continuation
		};
		return compile_application_function_ref(
			ctx, node->as.app.function, &context, p_ret
		);
	}
	if (compile_ast_computation_ref(ctx, ast_id, &value) != 0 ||
		value.polarity != COMPILE_REF_POLARITY_COMPUTATION) {
		return -1;
	}
	return compile_continue_computation(ctx, &value, ast_id, continuation, p_ret);
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
	struct compile_value_continuation continuation;
	if (!ctx || !node || !p_ref || node->tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	context.node = node;
	context.source_ast = ast_id;
	continuation.apply = compile_match_scrutinee_continuation;
	continuation.data = &context;
	return compile_ast_value_then(
		ctx, node->as.match.scrutinee, &continuation, p_ref
	);
}

struct compile_operation_spine_function_context {
	uint32_t argument_ast;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

struct compile_operation_spine_argument_context {
	struct compile_ref function;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

static int compile_ast_operation_spine_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
);

static int compile_operation_spine_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_operation_spine_argument_context* context = data;
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
	if (context->function.classifier != PROTOTYPE_INVALID_ID &&
		argument->classifier != PROTOTYPE_INVALID_ID &&
		operation_apply_classifier(
			ctx,
			context->function.classifier,
			argument->classifier,
			argument->term,
			&application.classifier
		) != 0) {
		return -1;
	}
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

static int compile_operation_spine_function_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_operation_spine_function_context* function_context = data;
	struct compile_operation_spine_argument_context argument_context;
	struct compile_value_continuation argument_continuation;
	if (!ctx || !function || !function_context || !p_ret) {
		return -1;
	}
	argument_context.function = *function;
	argument_context.source_ast = function_context->source_ast;
	argument_context.continuation = function_context->continuation;
	argument_continuation.apply = compile_operation_spine_argument_continuation;
	argument_continuation.data = &argument_context;
	return compile_ast_value_then(
		ctx, function_context->argument_ast, &argument_continuation, p_ret
	);
}

/* An intrinsic application consumes values like every other CBPV function.
 * Its arguments may therefore be computations which are sequenced before the
 * shared APP spine is constructed. */
static int compile_ast_operation_spine_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	const struct prototype_ast_node* node;
	if (!ctx || !continuation || !continuation->apply || !p_ret ||
		ast_id >= ctx->asts->node_count) {
		return -1;
	}
	node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_SYSTEM_NAME) {
		struct compile_ref operation;
		if (node->as.system_name.kind != PROTOTYPE_AST_SYSTEM_NAME_HOST_OPERATION ||
			compile_ast_value_ref(ctx, ast_id, &operation) != 0) {
			return -1;
		}
		return continuation->apply(ctx, &operation, continuation->data, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		struct compile_operation_spine_function_context function_context = {
			node->as.app.argument, ast_id, continuation
		};
		struct compile_value_continuation function_continuation = {
			compile_operation_spine_function_continuation, &function_context
		};
		return compile_ast_operation_spine_then(
			ctx, node->as.app.function, &function_continuation, p_ret
		);
	}
	return -1;
}

static int compile_operation_spine_computation_continuation(
	struct compile_context* ctx,
	const struct compile_ref* application,
	void* data,
	struct compile_ref* p_ret
) {
	struct prototype_term_classifier_view view;
	uint32_t head;
	uint32_t argument_count;
	const struct prototype_operation_declaration* declaration;
	(void)data;
	if (!ctx || !application || !p_ret) {
		return -1;
	}
	head = application->term;
	argument_count = 0;
	while (head < ctx->terms->term_count &&
		ctx->terms->terms[head].tag == PROTOTYPE_TERM_APP) {
		head = ctx->terms->terms[head].as.app.function;
		argument_count++;
	}
	if (head >= ctx->terms->term_count ||
		ctx->terms->terms[head].tag != PROTOTYPE_TERM_OPERATION ||
		!(declaration = prototype_term_operation_declaration(
			ctx->terms->terms[head].as.operation.operation_id
		)) || argument_count != declaration->arity) {
		return -1;
	}
	/* A binder introduced by an enclosing BIND can be the missing premise for
	 * this application. Preserve the computation occurrence for the constraint
	 * solver instead of requiring its result classifier during graph building. */
	if (application->classifier != PROTOTYPE_INVALID_ID &&
		(prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, application->classifier, &view
		) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION)) {
		return -1;
	}
	*p_ret = *application;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	return 0;
}

static int compile_ast_operation_spine_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	struct compile_value_continuation continuation = {
		compile_operation_spine_computation_continuation, NULL
	};
	return compile_ast_operation_spine_then(ctx, ast_id, &continuation, p_ret);
}

struct compile_perform_argument_context {
	struct compile_ref operation;
	uint32_t source_ast;
};

static int compile_perform_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_perform_argument_context* context = data;
	uint32_t binder_id;
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
	struct compile_ref result_variable;
	struct compile_ref result_return;
	uint32_t continuation_operation;
	if (!ctx || !argument || !context || !p_ret ||
		context->operation.classifier == PROTOTYPE_INVALID_ID ||
		operation_apply_classifier_unchecked(
			ctx,
			context->operation.classifier,
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
			ctx->terms, PROTOTYPE_HOST_EFFECT_NONE, &empty_effects
		) != 0 || prototype_term_computation_type(
			ctx->terms, empty_effects, application_view.result, &return_classifier
		) != 0 ||
		(binder_id = prototype_term_fresh_binder(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			ctx->terms, binder_id, return_classifier, &continuation_family
		) != 0 || prototype_term_pi_family(
			ctx->terms, application_view.result, continuation_family,
			&continuation_classifier
		) != 0 ||
		prototype_term_var(ctx->terms, binder_id, &binder_var) != 0 ||
		prototype_term_return(ctx->terms, binder_var, &result) != 0 ||
		prototype_term_lambda(ctx->terms, binder_id, result, &continuation) != 0 ||
		prototype_term_thunk(ctx->terms, continuation, &continuation_thunk) != 0 ||
		prototype_term_operation_request(
			ctx->terms, context->operation.term, argument->term, continuation_thunk, &request
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
	result_variable.term = canonical_result_variable;
	result_variable.classifier = application_view.result;
	result_variable.polarity = COMPILE_REF_POLARITY_VALUE;
	result_variable.computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, binder_var, application_view.result,
			context->source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &result_variable.operation
		) != 0 || compile_ref_make_return(
			ctx, &result_variable, context->source_ast, &result_return
		) != 0 || operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, continuation, continuation_classifier,
			context->source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			result_return.operation, PROTOTYPE_INVALID_ID, application_view.result,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	p_ret->term = request;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
	p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_UNKNOWN;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_THUNK, continuation_thunk, PROTOTYPE_INVALID_ID,
			context->source_ast, PROTOTYPE_INVALID_ID, continuation_operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	return operation_add(
		ctx, PROTOTYPE_OPERATION_PERFORM, request, application_classifier,
		context->source_ast, context->operation.operation, argument->operation,
		continuation_operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, 0, &p_ret->operation
	);
}

struct compile_perform_operation_context {
	uint32_t argument_ast;
	uint32_t source_ast;
};

static int compile_perform_operation_continuation(
	struct compile_context* ctx,
	const struct compile_ref* operation,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_perform_operation_context* operation_context = data;
	struct compile_perform_argument_context argument_context;
	struct compile_value_continuation argument_continuation;
	if (!ctx || !operation || !operation_context || !p_ret) {
		return -1;
	}
	argument_context.operation = *operation;
	argument_context.source_ast = operation_context->source_ast;
	argument_continuation.apply = compile_perform_argument_continuation;
	argument_continuation.data = &argument_context;
	return compile_ast_value_then(
		ctx, operation_context->argument_ast, &argument_continuation, p_ret
	);
}

static int compile_ast_perform_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	struct compile_ref* p_ret
) {
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_PERFORM ||
		node->as.unary.term >= ctx->asts->node_count ||
		ctx->asts->nodes[node->as.unary.term].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	const struct prototype_ast_node* operand =
		&ctx->asts->nodes[node->as.unary.term];
	struct compile_perform_operation_context operation_context = {
		operand->as.app.argument, ast_id
	};
	struct compile_value_continuation operation_continuation = {
		compile_perform_operation_continuation, &operation_context
	};
	return compile_ast_operation_spine_then(
		ctx, operand->as.app.function, &operation_continuation, p_ret
	);
}

static int compile_force_continuation(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	return compile_ref_make_force(ctx, value, *(const uint32_t*)data, p_ret);
}

/* #.bind M (\\x : A => N) is the source spelling of the binary core fold.
 * The name is bound by the continuation lambda; BIND itself owns no binder. */
static int compile_ast_bind_intrinsic_ref(
	struct compile_context* ctx,
	uint32_t source_ast,
	uint32_t computation_ast,
	uint32_t continuation_ast,
	struct compile_ref* p_ret
) {
	struct compile_ref computation;
	struct compile_ref continuation;
	if (!ctx || !p_ret ||
		compile_ast_computation_ref(ctx, computation_ast, &computation) != 0 ||
		computation.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		continuation_ast >= ctx->asts->node_count ||
		ctx->asts->nodes[continuation_ast].tag != PROTOTYPE_AST_LAMBDA ||
		compile_ast_lambda_computation_ref(
			ctx, &ctx->asts->nodes[continuation_ast], continuation_ast, &continuation
		) != 0 ||
		continuation.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
		continuation.computation_kind != COMPILE_REF_COMPUTATION_KIND_FUNCTION) {
		return -1;
	}
	return compile_ref_make_bind(ctx, &computation, &continuation, source_ast, p_ret);
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
	if (node->tag == PROTOTYPE_AST_APP) {
		uint32_t computation_ast;
		uint32_t continuation_ast;
		if (ast_application_is_bind_intrinsic(
				ctx, ast_id, &computation_ast, &continuation_ast
			)) {
			return compile_ast_bind_intrinsic_ref(
				ctx, ast_id, computation_ast, continuation_ast, p_ret
			);
		}
	}
	if (node->tag == PROTOTYPE_AST_RETURN) {
		struct compile_value_continuation continuation = {
			compile_return_continuation, &ast_id
		};
		return compile_ast_value_then(ctx, node->as.unary.term, &continuation, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_FORCE) {
		struct compile_value_continuation continuation = {
			compile_force_continuation, &ast_id
		};
		return compile_ast_value_then(ctx, node->as.unary.term, &continuation, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_PERFORM) {
		return compile_ast_perform_ref(ctx, ast_id, node, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_HANDLE) {
		return compile_ast_handle_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref inner;
		uint32_t surface_classifier;
		uint32_t expected_classifier;
		uint32_t inner_ast = node->as.ascription.term;
		if (inner_ast >= ctx->asts->node_count ||
			compile_ast_computation_ref(ctx, inner_ast, &inner) != 0 ||
			inner.polarity != COMPILE_REF_POLARITY_COMPUTATION ||
			compile_ast_ascription_classifier(
				ctx, node->as.ascription.type_expr, &surface_classifier
			) != 0 || compile_expected_classifier_for_ref(
				ctx, &inner, surface_classifier, &expected_classifier
			) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		inner.classifier = expected_classifier;
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
		if (ast_application_head_is_host_operation(ctx, ast_id)) {
			struct compile_ref operation_spine;
			if (compile_ast_operation_spine_computation_ref(
					ctx, ast_id, &operation_spine
				) != 0) {
				return -1;
			}
			operation_spine.polarity = COMPILE_REF_POLARITY_COMPUTATION;
			*p_ret = operation_spine;
			return 0;
		}
		if (ast_application_head_is_constructor(ctx, ast_id)) {
			struct compile_ref constructor_value;
			int constructor_status = compile_ast_constructor_application_value_ref(
					ctx, ast_id, &constructor_value
				);
			if (constructor_status < 0) {
				return -1;
			}
			if (constructor_status == 0) {
				if (!automatic_cbpv_coercions_enabled(ctx)) {
					return -1;
				}
				return compile_ref_make_return(ctx, &constructor_value, ast_id, p_ret);
			}
		}
		return compile_ast_application_computation_ref(ctx, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		/* An IH stands for the recursive Match computation M(rest), not for
		 * its result value.  `compile_ast_value_then` introduces BIND whenever
		 * a source occurrence needs that result as a value. */
		if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
			return -1;
		}
		p_ret->polarity = COMPILE_REF_POLARITY_COMPUTATION;
		/* The enclosing APP decides whether the inferred motive result is a Pi.
		 * Preserve the direct-function candidate until that solver constraint is
		 * available; value contexts still sequence this occurrence through BIND. */
		p_ret->computation_kind = COMPILE_REF_COMPUTATION_KIND_FUNCTION;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_NAME) {
		struct compile_ref name;
		if (compile_ast_atomic_ref(ctx, ast_id, &name) != 0) {
			return -1;
		}
		if (name.polarity == COMPILE_REF_POLARITY_COMPUTATION) {
			*p_ret = name;
			return 0;
		}
		if (name.polarity == COMPILE_REF_POLARITY_VALUE) {
			if (!automatic_cbpv_coercions_enabled(ctx)) {
				return -1;
			}
			return compile_ref_make_return(ctx, &name, ast_id, p_ret);
		}
		return -1;
	}
	if (!automatic_cbpv_coercions_enabled(ctx)) {
		return -1;
	}
	struct compile_value_continuation continuation = {
		compile_return_continuation, &ast_id
	};
	return compile_ast_value_then(ctx, ast_id, &continuation, p_ret);
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
		if (compile_ast_ascription_classifier(
					ctx, node->as.ascription.type_expr, &expected_classifier
				) != 0 ||
			compile_ast_against_surface_classifier(
					ctx, node->as.ascription.term, expected_classifier, &inner
				) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		*p_ret = inner;
		p_ret->classifier = expected_classifier;
		return 0;
	}
	uint32_t bind_computation_ast;
	uint32_t bind_continuation_ast;
	int is_bind_intrinsic = node->tag == PROTOTYPE_AST_APP &&
		ast_application_is_bind_intrinsic(
			ctx, ast_id, &bind_computation_ast, &bind_continuation_ast
		);
	/* BIND has an occurrence-local residual classifier. Preserve its source
	 * operation before value-first lowering can erase that occurrence. */
	if (is_bind_intrinsic) {
		return compile_ast_computation_ref(ctx, ast_id, p_ret);
	}
	int status = compile_ast_value_ref(ctx, ast_id, p_ret);
	if (status <= 0) {
		return status;
	}
	return compile_ast_computation_ref(ctx, ast_id, p_ret);
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
		ctx->match_frame_count = 0;
		size_t previous_error_count = ctx->metadata ? ctx->metadata->resolve_error_count : 0;
		if (compile_def(ctx, &ctx->asts->assignments[i], &term) != 0) {
			if (!ctx->metadata || ctx->metadata->resolve_error_count == previous_error_count) {
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
	return 0;
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
		for (uint32_t binder_index = 0;
			binder_index < resolved_case->binder_count;
			++binder_index) {
			struct prototype_case_binder* binder =
				&ctx->terms->case_binders[resolved_case->first_binder + binder_index];
			uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
			if (prototype_judgement_constructor_field_classifier(
					ctx->terms,
					ctx->type_declarations,
					resolved.constructor_owner,
					resolved.constructor_id,
					&ctx->terms->case_binders[resolved_case->first_binder],
					binder_index,
					binder_index,
					&binder_classifier
				) != 0) {
				return -1;
			}
			binder->is_recursive = prototype_judgement_classifier_normalization_equal(
				ctx->terms,
				ctx->type_declarations,
				binder_classifier,
				scrutinee_classifier
			);
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

static int operation_solver_add_constraint(
	struct compile_context* ctx,
	int kind,
	uint32_t target,
	uint32_t left,
	uint32_t right,
	uint32_t aux
) {
	if (!ctx || !ctx->metadata ||
		ctx->classifier_solver.constraint_count >= PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->classifier_solver.constraint_count++;
	ctx->classifier_solver.constraints[id] =
		(struct operation_classifier_constraint){
			.id = id,
			.kind = kind,
			.state = OPERATION_CONSTRAINT_STATE_PENDING,
			.source_operation = target,
			.source_ast = target < ctx->metadata->operation_count ?
				ctx->metadata->operations[target].source_ast : PROTOTYPE_INVALID_ID,
			.target = target,
			.left = left,
			.right = right,
			.aux = aux
		};
	return 0;
}

static int operation_solver_add_motive_equation(
	struct compile_context* ctx,
	uint32_t match_operation,
	uint32_t case_index,
	uint32_t body_operation
) {
	if (!ctx || !ctx->metadata ||
		match_operation >= ctx->metadata->operation_count ||
		body_operation >= ctx->metadata->operation_count ||
		ctx->classifier_solver.motive_equation_count >=
			PROTOTYPE_OPERATION_MOTIVE_EQUATION_CAPACITY) {
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
	ctx->classifier_solver.motive_equations[
		ctx->classifier_solver.motive_equation_count++
	] = (struct operation_motive_equation){
		match_operation,
		case_index,
		body_operation,
		match_case->constructor_owner,
		match_case->constructor_id,
		match_case->first_binder,
		match_case->binder_count,
		ast_match_case->first_binder,
		ast_match_case->binder_count,
		match->as.match.frame_id
	};
	return 0;
}

static const struct operation_motive_equation* operation_solver_motive_equation(
	const struct operation_classifier_solver* solver,
	uint32_t match_operation,
	uint32_t case_index
) {
	if (!solver) {
		return NULL;
	}
	for (uint32_t i = 0; i < solver->motive_equation_count; ++i) {
		const struct operation_motive_equation* equation =
			&solver->motive_equations[i];
		if (equation->match_operation == match_operation &&
			equation->case_index == case_index) {
			return equation;
		}
	}
	return NULL;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_motive_equation* equation,
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
		return operation_solver_enqueue_dependents(ctx, variable);
	}
	if (!prototype_judgement_classifier_normalization_equal(
			ctx->terms, ctx->type_declarations, previous, classifier
		)) {
		return -1;
	}
	return 0;
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
		prototype_term_whnf_with_profile(
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
	const struct operation_motive_equation* source_equation = NULL;
	for (uint32_t case_index = 0;
		case_index < ctx->metadata->operations[operation].case_count;
		++case_index) {
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation, case_index
			);
		if (!equation) {
			return -1;
		}
		if (equation->body_operation == source_body_operation) {
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
		uint32_t match_frame = PROTOTYPE_INVALID_ID;
		const struct prototype_operation_node* match_operation =
			&ctx->metadata->operations[operation];
		if (match_operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[match_operation->core_term].tag ==
				PROTOTYPE_TERM_MATCH) {
			match_frame =
				ctx->terms->terms[match_operation->core_term].as.match.frame_id;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->classifier_solver.constraint_count;
			++constraint_id) {
			const struct operation_classifier_constraint* constraint =
				&ctx->classifier_solver.constraints[constraint_id];
			if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED ||
				constraint->target >= ctx->metadata->operation_count ||
				ctx->metadata->operations[constraint->target].first_case !=
					match_frame) {
				continue;
			}
			if (operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return prototype_judgement_classifier_normalization_equal(
		ctx->terms, ctx->type_declarations, previous, classifier
	) ? 0 : -1;
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
				ctx, pending->context_aux, &ast_binder_id
			) != 0) {
			return -1;
		}
		if (operation_solver_add_input_fact(
				ctx,
				pending->binder_var,
				pending->classifier,
				pending->context_aux,
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
		const struct operation_classifier_constraint* constraint =
			&ctx->classifier_solver.constraints[i];
		if (operation_solver_add_constraint_dependency(
				ctx, constraint->target, i
			) != 0) {
			return -1;
		}
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_EQUAL:
			case OPERATION_CONSTRAINT_CONVERTIBLE:
			case OPERATION_CONSTRAINT_IH_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->left, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_PI_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->left, i
					) != 0 || (constraint->aux != 0 &&
					operation_solver_add_constraint_dependency(
						ctx, constraint->right, i
					) != 0)) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->left, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->aux, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->right, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_BIND_RESULT:
			case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			case OPERATION_CONSTRAINT_HANDLE_RESULT:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->left, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->right, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->aux, i
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
				 operation->referenced_ast_binder_id != fact->ast_binder_id)) ||
			graph_classifier_list_contains_normalization_equal(
				ctx, classifiers, *p_classifier_count, fact->classifier
			)) {
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
	if (ctx->classifier_constraints_generated) {
		ctx->classifier_solver.constraint_count =
			ctx->classifier_constraint_blueprint_count;
		memcpy(
			ctx->classifier_solver.constraints,
			ctx->classifier_constraint_blueprint,
			ctx->classifier_constraint_blueprint_count *
				sizeof(ctx->classifier_solver.constraints[0])
		);
	}
	for (uint32_t i = 0;
		!ctx->classifier_constraints_generated && i < ctx->metadata->operation_count;
		++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		int base_constraint_kind = OPERATION_CONSTRAINT_HAS_TYPE;
		if (operation->tag == PROTOTYPE_OPERATION_RETURN ||
			operation->tag == PROTOTYPE_OPERATION_THUNK ||
			operation->tag == PROTOTYPE_OPERATION_FORCE) {
			base_constraint_kind = OPERATION_CONSTRAINT_CBPV_BOUNDARY;
		} else if (operation->tag == PROTOTYPE_OPERATION_BIND) {
			base_constraint_kind = OPERATION_CONSTRAINT_BIND_RESULT;
		} else if (operation->tag == PROTOTYPE_OPERATION_PERFORM) {
			base_constraint_kind = OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT;
		} else if (operation->tag == PROTOTYPE_OPERATION_HANDLE) {
			base_constraint_kind = OPERATION_CONSTRAINT_HANDLE_RESULT;
		}
		if (operation->classifier_variable != i || operation_solver_add_constraint(
				ctx,
				base_constraint_kind,
				i,
				operation->function,
				operation->argument,
				operation->body
			) != 0) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			if (operation_solver_add_constraint(
					ctx, OPERATION_CONSTRAINT_EQUAL, i, operation->function,
					PROTOTYPE_INVALID_ID, 0
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
			if (operation->body >= ctx->metadata->operation_count ||
				operation_solver_add_constraint(
					ctx, OPERATION_CONSTRAINT_CONVERTIBLE, i, operation->body,
					operation->known_classifier, 0
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_LAMBDA) {
			if (operation_solver_add_constraint(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, operation->body,
					operation->binder_classifier, 0
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_APP) {
			if (operation_solver_add_constraint(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, operation->function,
					operation->argument, 1
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
			for (uint32_t j = 0; j < operation->case_count; ++j) {
				if (operation->first_case + j >= ctx->metadata->operation_case_count ||
					operation_solver_add_constraint(
						ctx, OPERATION_CONSTRAINT_MOTIVE_EQUATION, i,
						ctx->metadata->operation_cases[operation->first_case + j].body_operation,
						j, operation->scrutinee
						) != 0) {
					return -1;
				}
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
			if (operation_solver_add_constraint(
					ctx, OPERATION_CONSTRAINT_IH_EXPECTED, i, operation->argument,
					operation->first_case, 0
				) != 0) {
				return -1;
			}
		}
	}
	if (!ctx->classifier_constraints_generated) {
		ctx->classifier_constraint_blueprint_count =
			ctx->classifier_solver.constraint_count;
		memcpy(
			ctx->classifier_constraint_blueprint,
			ctx->classifier_solver.constraints,
			ctx->classifier_solver.constraint_count *
				sizeof(ctx->classifier_solver.constraints[0])
		);
		ctx->classifier_constraints_generated = 1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		if (operation->tag != PROTOTYPE_OPERATION_MATCH) {
			continue;
		}
		for (uint32_t j = 0; j < operation->case_count; ++j) {
			if (operation->first_case + j >= ctx->metadata->operation_case_count ||
				operation_solver_add_motive_equation(
					ctx,
					i,
					j,
					ctx->metadata->operation_cases[operation->first_case + j].body_operation
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
		if (operation->tag != PROTOTYPE_OPERATION_ASCRIPTION &&
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

static int compile_phase_record_residual_dependent_binds(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_BIND ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_BIND ||
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
			) != 0 || !prototype_judgement_classifier_normalization_equal(
				ctx->terms, ctx->type_declarations, domain, input_view.result
			) || prototype_term_pure_family_parts(
				ctx->terms, classifier_family, &continuation_binder_id, &codomain
			) != 0 || !prototype_term_contains_free_binder(
				ctx->terms, codomain, continuation_binder_id
			)) {
			continue;
		}
		struct prototype_term_normalization_result normalized;
		if (prototype_term_whnf_with_profile_result(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				ctx->terms->terms[operation->core_term].as.bind.computation,
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
			PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND,
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
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_DEPENDENT_BIND,
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

static int operation_solver_find_match_for_frame(
	const struct compile_context* ctx,
	uint32_t frame_id,
	uint32_t* p_operation
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

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
		struct operation_classifier_constraint* constraint =
			&ctx->classifier_solver.constraints[i];
		if (constraint->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int residual = 0;
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
			if (obligation->operation == constraint->target) {
				residual = 1;
				break;
			}
		}
		int solved = constraint->target < ctx->metadata->operation_count &&
			ctx->classifier_solver.bindings[constraint->target] != PROTOTYPE_INVALID_ID;
		if (constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE) {
			solved = constraint->left < ctx->metadata->operation_count &&
				operation_solver_classifier(ctx, constraint->left) != PROTOTYPE_INVALID_ID;
		}
		constraint->state = residual ? OPERATION_CONSTRAINT_STATE_RESIDUAL :
			(solved ? OPERATION_CONSTRAINT_STATE_SOLVED :
			(incomplete ? OPERATION_CONSTRAINT_STATE_INCOMPLETE :
				OPERATION_CONSTRAINT_STATE_PENDING));
		if (constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
}

static int operation_solver_propagate_bind_input(
	struct compile_context* ctx,
	uint32_t bind_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		bind_operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* bind_operation =
		&ctx->metadata->operations[bind_operation_id];
	if (bind_operation->tag != PROTOTYPE_OPERATION_BIND ||
		bind_operation->function >= ctx->metadata->operation_count ||
		bind_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[bind_operation->core_term].tag != PROTOTYPE_TERM_BIND) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, bind_operation->function
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
	const struct prototype_term* bind_term =
		&ctx->terms->terms[bind_operation->core_term];
	if (bind_term->as.bind.continuation >= ctx->terms->term_count ||
		ctx->terms->terms[bind_term->as.bind.continuation].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		bind_operation->argument >= ctx->metadata->operation_count ||
		ctx->metadata->operations[bind_operation->argument].tag !=
			PROTOTYPE_OPERATION_LAMBDA) {
		return -1;
	}
	struct prototype_operation_node* continuation_operation =
		&ctx->metadata->operations[bind_operation->argument];
	if (continuation_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
		continuation_operation->binder_classifier = input_view.result;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(
				ctx, bind_operation->argument
			) != 0) {
			return -1;
		}
	} else if (!prototype_judgement_classifier_normalization_equal(
			ctx->terms,
			ctx->type_declarations,
			continuation_operation->binder_classifier,
			input_view.result
		)) {
		return -1;
	}
	uint32_t binder_var;
	if (prototype_term_var(
			ctx->terms,
			ctx->terms->terms[bind_term->as.bind.continuation].as.lambda.binder_id,
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
		if (operation->tag == PROTOTYPE_OPERATION_VAR &&
			operation->core_term == binder_var &&
			operation_subtree_contains_operation(
				ctx,
				bind_operation->argument,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
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
			const struct operation_classifier_constraint* constraint =
				&ctx->classifier_solver.constraints[i];
			if (ctx->metadata->solver_steps_used >= ctx->metadata->solver_step_limit) {
				ctx->metadata->solver_exhausted = 1;
				operation_solver_refresh_constraint_states(ctx, 1);
				return 1;
			}
			ctx->metadata->solver_steps_used++;
			uint32_t classifier;
			switch (constraint->kind) {
				case OPERATION_CONSTRAINT_EQUAL:
					if (constraint->left >= ctx->metadata->operation_count ||
						(classifier = operation_solver_classifier(ctx, constraint->left)) ==
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if (operation_solver_bind(
							ctx, constraint->target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CONVERTIBLE:
					/* Conversion constraints do not synthesize a classifier. The
					 * dedicated ascription phase checks them with the complete
					 * imported-definition environment after this fixed point. */
					if (constraint->left >= ctx->metadata->operation_count ||
						constraint->right >= ctx->terms->term_count) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (constraint->left >= ctx->metadata->operation_count ||
						(constraint->aux != 0 &&
							constraint->right >= ctx->metadata->operation_count)) {
						break;
					}
					if (constraint->aux == 0) {
						uint32_t expected =
							ctx->classifier_solver.bindings[constraint->target];
						if (expected != PROTOTYPE_INVALID_ID) {
							uint32_t domain;
							uint32_t codomain_family;
							uint32_t codomain_binder;
							uint32_t codomain_body;
							uint32_t binder_var;
							uint32_t body_classifier;
							const struct prototype_operation_node* lambda =
								&ctx->metadata->operations[constraint->target];
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
								!prototype_judgement_classifier_normalization_equal(
									ctx->terms, ctx->type_declarations,
									domain, constraint->right
								) ||
								prototype_term_var(
									ctx->terms,
									ctx->terms->terms[lambda->core_term].as.lambda.binder_id,
									&binder_var
								) != 0 ||
								prototype_term_substitute(
									ctx->terms,
									ctx->type_declarations,
									codomain_body,
									codomain_binder,
									binder_var,
									&body_classifier
								) != 0 ||
								operation_solver_bind(
									ctx, constraint->left, body_classifier, &pass_changed
								) != 0) {
								/* The expected Pi may be more reduced than the current
								 * operation frontier. Leave this reverse propagation for a
								 * later pass; the forward Pi constraint remains authoritative. */
								break;
							}
						}
						classifier = operation_solver_classifier(ctx, constraint->left);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t codomain_family;
						const struct prototype_operation_node* lambda =
							&ctx->metadata->operations[constraint->target];
						uint32_t effective_domain = constraint->right != PROTOTYPE_INVALID_ID ?
							constraint->right : lambda->binder_classifier;
						if (effective_domain == PROTOTYPE_INVALID_ID ||
							lambda->core_term >= ctx->terms->term_count ||
							ctx->terms->terms[lambda->core_term].tag !=
								PROTOTYPE_TERM_LAMBDA ||
							prototype_term_pure_family(
								ctx->terms,
								ctx->terms->terms[lambda->core_term].as.lambda.binder_id,
								classifier,
								&codomain_family
							) != 0 ||
							prototype_term_pi_family(
								ctx->terms,
								effective_domain,
								codomain_family,
								&classifier
							) != 0 || operation_solver_generalize_lambda_effect_rows(
								ctx, constraint->target, classifier, &classifier
							) != 0 || operation_solver_bind(
								ctx, constraint->target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					} else {
						uint32_t function_classifier =
							operation_solver_classifier(ctx, constraint->left);
						uint32_t argument_classifier =
							operation_solver_classifier(ctx, constraint->right);
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (operation_solver_specialize_integer_literal(
								ctx,
								function_classifier,
								constraint->right,
								&argument_classifier,
								&pass_changed
							) != 0) {
							return -1;
						}
						int apply_status = operation_apply_classifier(
							ctx,
							function_classifier,
							argument_classifier,
							ctx->metadata->operations[constraint->right].core_term,
							&classifier
						);
						/* A non-Pi or incompatible current candidate may belong to a
						 * shared core node. Leave this operation constraint unresolved
						 * until a source-operation binding selects its classifier. */
						if (apply_status == 0 && operation_solver_bind(
								ctx, constraint->target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_HAS_TYPE:
					if (ctx->classifier_solver.bindings[constraint->target] !=
						PROTOTYPE_INVALID_ID) {
						break;
					}
					/* Core facts may seed atoms only, never a typed operation edge. */
					if (ctx->metadata->operations[constraint->target].tag ==
							PROTOTYPE_OPERATION_APP ||
						ctx->metadata->operations[constraint->target].tag ==
							PROTOTYPE_OPERATION_LAMBDA ||
						ctx->metadata->operations[constraint->target].tag ==
							PROTOTYPE_OPERATION_MATCH) {
						break;
					}
					{
						uint32_t classifiers[32];
						uint32_t classifier_count = 0;
						if (operation_solver_collect_input_classifiers(
								ctx,
								constraint->target,
								classifiers,
								32,
								&classifier_count
							) != 0) {
							return -1;
						}
						if (classifier_count == 1 && operation_solver_bind(
								ctx, constraint->target, classifiers[0], &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
					if (operation_solver_solve_match(
							ctx, constraint->target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_IH_EXPECTED:
					if (operation_solver_materialize_induction_hypothesis(
							ctx, constraint->target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
					if (constraint->right >= ctx->metadata->operation_count) {
						return -1;
					}
					uint32_t child_classifier = operation_solver_classifier(
						ctx, constraint->right
					);
					if (child_classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					if (prototype_judgement_cbpv_boundary_classifier(
							ctx->terms,
							ctx->type_declarations,
							ctx->metadata->operations[constraint->target].core_term,
							child_classifier,
							&classifier
						) != 0 || operation_solver_bind(
							ctx, constraint->target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_BIND_RESULT:
					if (operation_solver_propagate_bind_input(
							ctx, constraint->target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
				case OPERATION_CONSTRAINT_HANDLE_RESULT:
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
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[match->as.match.first_case + case_index];
		if (!equation || equation->body_operation >= ctx->metadata->operation_count ||
			source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256) {
			return 1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[equation->body_operation];
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			motive_binders[binder_cursor + i].binder_id =
				prototype_term_fresh_binder(ctx->terms);
			motive_binders[binder_cursor + i].is_recursive = 0;
			if (motive_binders[binder_cursor + i].binder_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[match->as.match.first_case + case_index];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		if (prototype_judgement_prepare_match_motive_case(
				ctx->terms, ctx->type_declarations,
				&ctx->terms->case_binders[source_case->first_binder],
				&motive_binders[binder_cursor], source_case->binder_count,
				branch_classifier, &motive_cases[case_index].body
			) != 0) {
			return -1;
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t binder_id = prototype_term_fresh_binder(ctx->terms);
	uint32_t binder_var;
	uint32_t motive_match;
	uint32_t motive;
	if (binder_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binder_id, &binder_var) != 0 ||
		prototype_term_match(ctx->terms, binder_var, motive_cases, operation->case_count, &motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, binder_id, motive_match, &motive) != 0 ||
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
		case PROTOTYPE_OPERATION_PERFORM:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_BIND:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_HANDLE:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->body;
			children[child_count++] = operation->scrutinee;
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
		case PROTOTYPE_OPERATION_BIND:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_PERFORM:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_HANDLE:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->body;
			children[child_count++] = operation->scrutinee;
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

static int operation_branch_uses_case_binder(
	const struct compile_context* ctx,
	const struct operation_motive_equation* equation
) {
	if (!ctx || !equation ||
		equation->first_ast_binder + equation->ast_binder_count >
			ctx->asts->case_binder_count) {
		return -1;
	}
	for (uint32_t i = 0; i < equation->ast_binder_count; ++i) {
		uint8_t visited[4096] = { 0 };
		int status = operation_subtree_references_ast_binder(
			ctx, equation->body_operation,
			ctx->asts->case_binders[equation->first_ast_binder + i].ast_binder_id,
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
	const struct operation_motive_equation* equation,
	uint32_t classifier
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		equation->first_binder + equation->binder_count >
			ctx->terms->case_binder_count) {
		return -1;
	}
	int source_uses_binder = operation_branch_uses_case_binder(ctx, equation);
	if (source_uses_binder <= 0) {
		return source_uses_binder;
	}
	uint32_t classifier_whnf;
	if (prototype_term_whnf_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < equation->binder_count; ++i) {
		uint32_t binder_id = ctx->terms->case_binders[
			equation->first_binder + i
		].binder_id;
		if (prototype_term_contains_free_binder(
			ctx->terms, classifier_whnf, binder_id
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
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		if (!equation || equation->body_operation >= ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[equation->body_operation];
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
		} else if (!prototype_judgement_classifier_normalization_equal(
				ctx->terms, ctx->type_declarations, classifier, branch_classifier
			)) {
			return 1;
		}
	}
	uint32_t binder_id = prototype_term_fresh_binder(ctx->terms);
	uint32_t motive;
	if (binder_id == PROTOTYPE_INVALID_ID ||
		prototype_term_lambda(ctx->terms, binder_id, classifier, &motive) != 0 ||
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
	uint32_t motive_frame = prototype_term_new_match_frame(ctx->terms);
	if (motive_frame == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_motive_equation* equation =
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
		for (uint32_t binder_index = 0;
			binder_index < source_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* source_binder =
				&ctx->terms->case_binders[source_case->first_binder + binder_index];
			motive_binders[binder_cursor + binder_index].binder_id =
				prototype_term_fresh_binder(ctx->terms);
			motive_binders[binder_cursor + binder_index].is_recursive =
				source_binder->is_recursive;
			if (motive_binders[binder_cursor + binder_index].binder_id ==
				PROTOTYPE_INVALID_ID) {
				return -1;
			}
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[source_case_id];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[equation->body_operation];
		if (branch_classifier != PROTOTYPE_INVALID_ID) {
			if (prototype_judgement_prepare_match_motive_case(
					ctx->terms, ctx->type_declarations,
					&ctx->terms->case_binders[source_case->first_binder],
					&motive_binders[binder_cursor], source_case->binder_count,
					branch_classifier, &motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		} else {
			const struct prototype_operation_node* branch =
				&ctx->metadata->operations[equation->body_operation];
			uint32_t parent_match;
			if (branch->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
				branch->argument >= ctx->metadata->operation_count ||
				operation_solver_find_match_for_frame(
					ctx, branch->first_case, &parent_match
				) != 0 || parent_match != typing->operation) {
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
					source_binder->binder_id ==
						ctx->terms->terms[original_argument].as.var.binder_id) {
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
					motive_binders[binder_cursor + recursive_index].binder_id,
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
	uint32_t motive_binder = prototype_term_fresh_binder(ctx->terms);
	uint32_t motive_var;
	uint32_t motive_match;
	uint32_t motive;
	if (motive_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, motive_binder, &motive_var) != 0 ||
		prototype_term_match_with_frame(
			ctx->terms, motive_var, motive_cases, operation->case_count,
			motive_frame, &motive_match
		) != 0 ||
		prototype_term_set_match_frame_term(ctx->terms, motive_frame, motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, motive_binder, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

static int operation_solver_find_match_for_frame(
	const struct compile_context* ctx,
	uint32_t frame_id,
	uint32_t* p_operation
) {
	if (!ctx || !p_operation || frame_id >= ctx->terms->match_frame_count) {
		return -1;
	}
	uint32_t match_term = ctx->terms->match_frames[frame_id].match_term;
	if (match_term >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
			operation->core_term == match_term) {
			*p_operation = i;
			return 0;
		}
	}
	return 1;
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
		ih->first_case >= ctx->terms->match_frame_count ||
		argument->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[argument->core_term].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	if (ctx->terms->match_frames[ih->first_case].match_term != match->core_term ||
		match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t binder_id = ctx->terms->terms[argument->core_term].as.var.binder_id;
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
			if (binder->binder_id == binder_id) {
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
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation || equation->body_operation >= ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.bindings[equation->body_operation];
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
			source_body_operation = equation->body_operation;
		} else if (!prototype_judgement_classifier_normalization_equal(
				ctx->terms, ctx->type_declarations, candidate, branch_classifier
			)) {
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
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		if (ctx->classifier_solver.bindings[equation->body_operation] !=
			PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_operation_node* body =
			&ctx->metadata->operations[equation->body_operation];
		uint32_t parent_match;
		if (body->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			operation_solver_find_match_for_frame(
				ctx, body->first_case, &parent_match
			) != 0 || parent_match != operation_id) {
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
		const struct operation_motive_equation* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		uint32_t body_operation = equation->body_operation;
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
	 * THUNK, or BIND, so it must receive M(rest) through the solver before its
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
		!prototype_judgement_classifier_normalization_equal(
			ctx->terms, ctx->type_declarations, classifier, seed_classifier
		)) {
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
		operation->argument >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t parent_match;
	int match_status = operation_solver_find_match_for_frame(
		ctx, operation->first_case, &parent_match
	);
	if (match_status != 0) {
		return match_status < 0 ? -1 : 0;
	}
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
		 * propagate through RETURN/THUNK/BIND; the lambda for M is still
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
		const struct prototype_judgement_relation* relations =
			source == 0 ? ctx->judgement_delta.relations : ctx->judgement->relations;
		size_t relation_count =
			source == 0 ? ctx->judgement_delta.relation_count : ctx->judgement->relation_count;
		for (size_t i = 0; i < relation_count; ++i) {
			if (relations[i].kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relations[i].subject == operation->core_term &&
				prototype_judgement_classifier_normalization_equal(
					ctx->terms, ctx->type_declarations,
					relations[i].classifier, classifier
				)) {
				return 0;
			}
		}
	}
	const struct prototype_term* ih = &ctx->terms->terms[operation->core_term];
	uint32_t frame_id = ih->as.induction_hypothesis.frame_id;
	if (frame_id >= ctx->terms->match_frame_count ||
		ih->as.induction_hypothesis.argument >= ctx->terms->term_count ||
		ctx->terms->terms[ih->as.induction_hypothesis.argument].tag !=
			PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t match_term = ctx->terms->match_frames[frame_id].match_term;
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[match_term];
	uint32_t binder_id =
		ctx->terms->terms[ih->as.induction_hypothesis.argument].as.var.binder_id;
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
			if (binder->binder_id != binder_id) {
				continue;
			}
			if (!binder->is_recursive) {
				return -1;
			}
			return prototype_judgement_delta_expand_induction_hypothesis(
				&ctx->judgement_delta, ctx->terms, operation->core_term, classifier,
				match_term, case_index, binder_index
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
				operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
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
				if (prototype_judgement_constructor_field_classifier(
						ctx->terms,
						ctx->type_declarations,
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
						].binder_id,
						&binder_var
					) != 0) {
					return -1;
				}
				int already_materialized = 0;
				for (size_t relation_id = 0;
					relation_id < ctx->judgement_delta.relation_count;
					++relation_id) {
					const struct prototype_judgement_relation* relation =
						&ctx->judgement_delta.relations[relation_id];
					if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						relation->subject != binder_var ||
						relation->classifier != classifier ||
						relation->proof_kind !=
							PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
						relation->proof_id >= ctx->judgement_delta.proof_count) {
						continue;
					}
					const struct prototype_judgement_proof* proof =
						&ctx->judgement_delta.proofs[relation->proof_id];
					if (proof->context_kind ==
							PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_MATCH_CASE_FIELD &&
						proof->context_subject == operation->core_term &&
						proof->context_index == case_index &&
						proof->context_aux == binder_index) {
						already_materialized = 1;
						break;
					}
				}
				if (already_materialized) {
					continue;
				}
				if (
					prototype_judgement_delta_expand_match_pattern(
						&ctx->judgement_delta,
						ctx->terms,
						binder_var,
						classifier
					) != 0 ||
					ctx->judgement_delta.relation_count == 0) {
					return -1;
				}
				uint32_t proof_id = ctx->judgement_delta.relations[
					ctx->judgement_delta.relation_count - 1
				].proof_id;
				if (prototype_judgement_delta_set_proof_context_by_id(
						&ctx->judgement_delta,
						proof_id,
						PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_MATCH_CASE_FIELD,
						operation->core_term,
						case_index,
						binder_index
					) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
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
	uint32_t classifier = operation->classifier;
	const struct prototype_term* term = &ctx->terms->terms[core_term];
	if (classifier == PROTOTYPE_INVALID_ID) {
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
				operation_solver_reify_core_proof(
					ctx, operation->body, term->as.lambda.body, depth + 1
				) < 0 || prototype_judgement_delta_record_lambda_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					operation->binder_classifier,
					body->classifier
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
			const struct prototype_operation_node* function =
				&ctx->metadata->operations[operation->function];
			const struct prototype_operation_node* argument =
				&ctx->metadata->operations[operation->argument];
			if (function->classifier == PROTOTYPE_INVALID_ID ||
				argument->classifier == PROTOTYPE_INVALID_ID ||
				operation_solver_reify_core_proof(
					ctx, operation->function, term->as.app.function, depth + 1
				) < 0 || operation_solver_reify_core_proof(
					ctx, operation->argument, term->as.app.argument, depth + 1
				) < 0 || prototype_judgement_delta_record_app_elim(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					term->as.app.function,
					function->classifier,
					term->as.app.argument,
					argument->classifier
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE: {
			uint32_t child_term;
			uint32_t child_classifier;
			if (operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			if (term->tag == PROTOTYPE_TERM_RETURN) {
				child_term = term->as.return_term.value;
				struct prototype_term_classifier_view view;
				if (prototype_judgement_classifier_view(
						ctx->terms, ctx->type_declarations, NULL, classifier, &view
					) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
					view.computation_kind !=
						PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
					return -1;
				}
				child_classifier = view.result;
			} else if (term->tag == PROTOTYPE_TERM_THUNK) {
				child_term = term->as.thunk.computation;
				uint32_t whnf;
				if (prototype_term_whnf_with_profile(
						ctx->terms,
						ctx->type_declarations,
						NULL,
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
						classifier,
						&whnf
					) != 0 || whnf >= ctx->terms->term_count ||
					ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_THUNK_TYPE) {
					return -1;
				}
				child_classifier = ctx->terms->terms[whnf].as.thunk_type.computation;
			} else if (term->tag == PROTOTYPE_TERM_FORCE) {
				child_term = term->as.force.value;
				if (prototype_term_thunk_type(
						ctx->terms, classifier, &child_classifier
					) != 0) {
					return -1;
				}
			} else {
				return -1;
			}
			if (operation_solver_reify_core_proof(
					ctx, operation->argument, child_term, depth + 1
				) < 0 || prototype_judgement_delta_record_cbpv_boundary(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_classifier
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_VAR:
		case PROTOTYPE_OPERATION_NAME:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_ASCRIPTION:
		case PROTOTYPE_OPERATION_MATCH:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_OPERATION_BIND:
		case PROTOTYPE_OPERATION_PERFORM:
		case PROTOTYPE_OPERATION_HANDLE:
			return 0;
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
		 * first fixed-point round RETURN/THUNK/BIND may not yet have supplied
		 * those branch classifiers, so an unresolved Match is a deferred
		 * equation, not a compilation error. */
		if (operation->classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_judgement_delta_record_materialized_match_motive(
				&ctx->judgement_delta,
				ctx->terms,
				operation->core_term,
				operation->classifier
			) != 0) {
			return -1;
		}
	}
	for (;;) {
		size_t before_relation_count = ctx->judgement_delta.relation_count;
		for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[i];
			uint32_t classifier = operation->classifier;
			if (classifier == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (operation->tag == PROTOTYPE_OPERATION_LAMBDA ||
				operation->tag == PROTOTYPE_OPERATION_APP) {
				if (operation_solver_reify_core_proof(
						ctx, i, operation->core_term, 0
					) != 0) {
					return -1;
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
					PROTOTYPE_TERM_OPERATION &&
				prototype_judgement_delta_record_operation_type(
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
					if (prototype_term_make_host_type(
							ctx->terms,
							PROTOTYPE_HOST_TYPE_INT32,
							&integer
						) != 0 ||
						prototype_judgement_delta_record_int_literal(
							&ctx->judgement_delta,
							ctx->terms,
							operation->core_term,
							integer
						) != 0) {
						return -1;
					}
				}
			}
		}
		if (ctx->judgement_delta.relation_count == before_relation_count) {
			break;
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
		/* A lambda may share a core representative with an earlier occurrence.
		 * Its source RETURN/THUNK/FORCE body then has a different raw binder id.
		 * The lambda proof reifies that boundary at the representative body; do
		 * not also publish the unrepresentative child as an independent proof. */
		int is_nonrepresentative_lambda_body = 0;
		for (uint32_t parent_id = 0;
			parent_id < ctx->metadata->operation_count;
			++parent_id) {
			const struct prototype_operation_node* parent =
				&ctx->metadata->operations[parent_id];
			if (parent->tag != PROTOTYPE_OPERATION_LAMBDA ||
				parent->body != operation_id ||
				parent->core_term >= ctx->terms->term_count ||
				ctx->terms->terms[parent->core_term].tag != PROTOTYPE_TERM_LAMBDA) {
				continue;
			}
			if (ctx->terms->terms[parent->core_term].as.lambda.body !=
				operation->core_term) {
				is_nonrepresentative_lambda_body = 1;
			}
			break;
		}
		if (is_nonrepresentative_lambda_body) {
			continue;
		}
		uint32_t child_classifier = operation_solver_classifier(
			ctx, operation->argument
		);
		if (child_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_judgement_delta_record_cbpv_boundary(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				operation->core_term,
				child_classifier
			) != 0) {
			return -1;
		}
	}
	/* Handler and BIND constraints can introduce a local binder fact after the
	 * source-operation solver has first visited the corresponding VAR
	 * occurrence.  Reflect only a unique local fact back into that occurrence.
	 * A shared core VAR may have several source-lambda assumptions (for example
	 * Int and Text identities sharing one core lambda), in which case choosing
	 * one here would recreate the cross-product this metadata layer prevents. */
	for (uint32_t operation_id = 0; operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_VAR ||
			ctx->classifier_solver.bindings[operation_id] != PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier = PROTOTYPE_INVALID_ID;
		for (size_t relation_id = 0;
			relation_id < ctx->judgement_delta.relation_count;
			++relation_id) {
			const struct prototype_judgement_relation* relation =
				&ctx->judgement_delta.relations[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != operation->core_term ||
				relation->proof_kind !=
					PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
				relation->proof_id >= ctx->judgement_delta.proof_count) {
				continue;
			}
			const struct prototype_judgement_proof* proof =
				&ctx->judgement_delta.proofs[relation->proof_id];
			if (proof->context_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_LAMBDA_BINDER &&
				proof->context_aux != PROTOTYPE_INVALID_ID) {
				uint32_t ast_binder_id;
				if (operation_solver_lambda_ast_binder(
						ctx, proof->context_aux, &ast_binder_id
					) != 0 || ast_binder_id !=
						operation->referenced_ast_binder_id) {
					continue;
				}
			}
			if (classifier == PROTOTYPE_INVALID_ID) {
				classifier = relation->classifier;
			} else if (!prototype_judgement_classifier_normalization_equal(
					ctx->terms,
					ctx->type_declarations,
					classifier,
					relation->classifier
				)) {
				classifier = PROTOTYPE_INVALID_ID;
				break;
			}
		}
		if (classifier != PROTOTYPE_INVALID_ID &&
			operation_solver_bind(ctx, operation_id, classifier, p_changed) != 0) {
			return -1;
		}
	}
	for (uint32_t operation_id = 0; operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		int expected_proof_kind;
		switch (operation->tag) {
			case PROTOTYPE_OPERATION_RETURN:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
				break;
			case PROTOTYPE_OPERATION_THUNK:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
				break;
			case PROTOTYPE_OPERATION_FORCE:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM;
				break;
			case PROTOTYPE_OPERATION_BIND:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_BIND_INTRO;
				break;
			case PROTOTYPE_OPERATION_PERFORM:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO;
				break;
			case PROTOTYPE_OPERATION_HANDLE:
				expected_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_HANDLE_ELIM;
				break;
			default:
				continue;
		}
		if (operation->core_term >= ctx->terms->term_count) {
			return -1;
		}
		for (size_t relation_id = 0;
			relation_id < ctx->judgement_delta.relation_count;
			++relation_id) {
			const struct prototype_judgement_relation* relation =
				&ctx->judgement_delta.relations[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != operation->core_term ||
				relation->proof_kind != expected_proof_kind ||
				relation->proof_id >= ctx->judgement_delta.proof_count) {
				continue;
			}
			const struct prototype_judgement_proof* proof =
				&ctx->judgement_delta.proofs[relation->proof_id];
			int matches = 0;
			if (operation->tag == PROTOTYPE_OPERATION_RETURN ||
				operation->tag == PROTOTYPE_OPERATION_THUNK ||
				operation->tag == PROTOTYPE_OPERATION_FORCE) {
				matches = proof->premise_count == 1 &&
					operation->argument < ctx->metadata->operation_count &&
					proof->premise_subjects[0] ==
						ctx->metadata->operations[operation->argument].core_term &&
					proof->premise_classifiers[0] ==
						ctx->metadata->operations[operation->argument].classifier;
			} else if (operation->tag == PROTOTYPE_OPERATION_BIND) {
				matches = proof->premise_count == 2 &&
					operation->function < ctx->metadata->operation_count &&
					operation->argument < ctx->metadata->operation_count &&
					proof->premise_subjects[0] ==
						ctx->metadata->operations[operation->function].core_term &&
					proof->premise_classifiers[0] ==
						ctx->metadata->operations[operation->function].classifier &&
					proof->premise_subjects[1] ==
						ctx->metadata->operations[operation->argument].core_term &&
					proof->premise_classifiers[1] ==
						ctx->metadata->operations[operation->argument].classifier;
				} else if (operation->tag == PROTOTYPE_OPERATION_PERFORM) {
					const struct prototype_term* request =
						&ctx->terms->terms[operation->core_term];
					matches = proof->premise_count == 2 &&
						operation->function < ctx->metadata->operation_count &&
						operation->argument < ctx->metadata->operation_count &&
						operation->body < ctx->metadata->operation_count &&
						request->tag == PROTOTYPE_TERM_OPERATION_REQUEST &&
						proof->premise_subjects[0] < ctx->terms->term_count &&
						ctx->terms->terms[proof->premise_subjects[0]].tag ==
							PROTOTYPE_TERM_APP &&
						ctx->terms->terms[proof->premise_subjects[0]].as.app.function ==
							ctx->metadata->operations[operation->function].core_term &&
						ctx->terms->terms[proof->premise_subjects[0]].as.app.argument ==
							ctx->metadata->operations[operation->argument].core_term &&
						proof->premise_subjects[1] ==
							ctx->metadata->operations[operation->body].core_term &&
						request->as.operation_request.operation ==
							ctx->metadata->operations[operation->function].core_term &&
						request->as.operation_request.argument ==
							ctx->metadata->operations[operation->argument].core_term &&
						request->as.operation_request.continuation ==
							ctx->metadata->operations[operation->body].core_term;
				} else {
					uint32_t handler_term = proof->premise_subjects[0];
				matches = proof->premise_count == 2 &&
					operation->function < ctx->metadata->operation_count &&
					operation->argument < ctx->metadata->operation_count &&
					handler_term < ctx->terms->term_count &&
					ctx->terms->terms[handler_term].tag == PROTOTYPE_TERM_HANDLER &&
						proof->premise_subjects[1] ==
							ctx->metadata->operations[operation->function].core_term &&
					ctx->terms->terms[handler_term].as.handler.operation ==
						ctx->metadata->operations[operation->argument].core_term;
			}
			if (matches && operation_solver_bind(
					ctx, operation_id, relation->classifier, p_changed
				) != 0) {
				return -1;
			}
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
		size_t before_relations = ctx->judgement_delta.relation_count;
		int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
		if (match_resolution_status < 0) {
			return -1;
		}
		int status = compile_phase_infer_general_classifiers(ctx, 0);
		if (status != 0) {
			return -1;
		}
		/* Source operations carry the selected typed occurrence. Materialize their
		 * CBPV boundaries before solving sequencing constraints so a shared
		 * FORCE/THUNK node is never reclassified from an unrelated occurrence. */
		if (operation_solver_materialize_judgements(ctx) != 0) {
			return -1;
		}
		int cbpv_changed = 0;
		if (bind_cbpv_operation_classifiers(ctx, &cbpv_changed) != 0 ||
			prototype_judgement_delta_solve_computation_constraints(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations
			) != 0) {
			return -1;
		}
		if (count_classified_operations(ctx) == before_operation_classifiers &&
			count_unresolved_resolution_items(ctx) == before_unresolved &&
			ctx->judgement_delta.relation_count == before_relations && !cbpv_changed) {
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
	if (prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0) {
		return -1;
	}
	/* The materialized classifier relation has been committed. This temporary
	 * solver lookup index must not survive as pending JudgementDB state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return count_unresolved_resolution_items(ctx) == 0 ? 0 : -1;
}

static int find_compatible_classifier_for_expectation(
	struct compile_context* ctx,
	const struct prototype_term_definition_env* definitions,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t* p_actual_classifier
);

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
		uint32_t actual_classifier = PROTOTYPE_INVALID_ID;
		uint32_t operation_classifier = PROTOTYPE_INVALID_ID;
		if (check->operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[check->operation].classifier !=
				PROTOTYPE_INVALID_ID) {
			operation_classifier = ctx->metadata->operations[check->operation].classifier;
			if (!prototype_judgement_classifier_compatible_with_definitions(
					ctx->terms,
					ctx->type_declarations,
					&imported_definition_env,
					check->expected_classifier,
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
		}
		if (find_compatible_classifier_for_expectation(
				ctx,
				&imported_definition_env,
				check->subject,
				check->expected_classifier,
				&actual_classifier
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
		if (operation_classifier == PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_compatible_with_definitions(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				check->expected_classifier,
				actual_classifier
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
			if (prototype_judgement_add_conversion(
					ctx->judgement,
					ctx->terms,
					check->subject,
					check->expected_classifier,
					actual_classifier
				) != 0) {
				return -1;
			}
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
		ctx->match_frame_count = 0;
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
		} else if (find_compatible_classifier_for_expectation(
				ctx,
				&imported_definition_env,
				compiled_term,
				expected_classifier,
				&actual_classifier
		) == 0) {
			/* A term without a source operation can use a unique core derivation. */
		} else if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag !=
				PROTOTYPE_OPERATION_ASCRIPTION &&
			def->compiled_classifier != PROTOTYPE_INVALID_ID) {
			actual_classifier = def->compiled_classifier;
		} else if (find_unique_synthetic_classifier_for_label(
				ctx,
				compiled_term,
				&actual_classifier
			) != 0) {
			actual_classifier = PROTOTYPE_INVALID_ID;
		}
		if (actual_classifier == PROTOTYPE_INVALID_ID ||
			!prototype_judgement_classifier_compatible_with_definitions(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				expected_classifier,
				actual_classifier
			)) {
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
		if (actual_from_operation) {
			def->compiled_classifier = expected_classifier;
			continue;
		}
		if (prototype_judgement_expand_checked(
				ctx->judgement,
				ctx->terms,
				compiled_term,
				actual_classifier
			) != 0) {
			return -1;
		}
			if (prototype_judgement_add_conversion(
					ctx->judgement,
					ctx->terms,
					compiled_term,
					expected_classifier,
					actual_classifier
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

static int find_compatible_classifier_for_expectation(
	struct compile_context* ctx,
	const struct prototype_term_definition_env* definitions,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t* p_actual_classifier
) {
	if (!ctx || !definitions || !p_actual_classifier) {
		return -1;
	}
	if (ctx->judgement) {
		for (size_t i = ctx->judgement->relation_count; i > 0; --i) {
			const struct prototype_judgement_relation* relation =
				&ctx->judgement->relations[i - 1];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != subject) {
				continue;
			}
			if (prototype_judgement_classifier_compatible_with_definitions(
					ctx->terms,
					ctx->type_declarations,
					definitions,
					expected_classifier,
					relation->classifier
				)) {
					*p_actual_classifier = relation->classifier;
					return 0;
				}
		}
	}
	return 1;
}

static int find_unique_synthetic_classifier_for_label(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->judgement || !p_classifier ||
		subject >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (size_t i = 0; i < ctx->judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation =
			&ctx->judgement->relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->subject != subject ||
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID) {
			if (!prototype_judgement_classifier_normalization_equal(
					ctx->terms,
					ctx->type_declarations,
					selected,
					relation->classifier
				)) {
				return 1;
			}
			continue;
		}
		selected = relation->classifier;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
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
		if (def->compiled_classifier == PROTOTYPE_INVALID_ID) {
			(void)find_unique_synthetic_classifier_for_label(
				ctx,
				def->compiled_term,
				&def->compiled_classifier
			);
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
		if (operation->tag == PROTOTYPE_OPERATION_HANDLE) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
		}
		if (operation->tag != PROTOTYPE_OPERATION_PERFORM ||
			operation->core_term >= terms->term_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_OPERATION_REQUEST) {
			continue;
		}
		capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH;
		uint32_t head = terms->terms[operation->core_term].as.operation_request.operation;
		while (head < terms->term_count && terms->terms[head].tag == PROTOTYPE_TERM_APP) {
			head = terms->terms[head].as.app.function;
		}
		if (head < terms->term_count && terms->terms[head].tag == PROTOTYPE_TERM_OPERATION &&
			terms->terms[head].as.operation.operation_id == PROTOTYPE_OPERATION_PRINT) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
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

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	int namespace_symbol_id,
	int automatic_cbpv_coercions,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
) {
	if (!asts || !terms || !type_declarations || !judgement) {
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
				ctx.judgement_delta_relations,
				ctx.judgement_delta_proofs,
				PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
				ctx.judgement_delta_match_motive_results,
				PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
				ctx.judgement_delta_computation_constraints,
				PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
				ctx.judgement_delta_effect_row_equations,
				PROTOTYPE_JUDGEMENT_DELTA_CAPACITY
			);
	ctx.metadata = metadata;
	if (metadata) {
		prototype_judgement_delta_set_solver_budget(
			&ctx.judgement_delta,
			metadata->solver_step_limit,
			&metadata->solver_steps_used,
			&metadata->solver_exhausted
		);
	}
	ctx.namespace_symbol_id = namespace_symbol_id;
	ctx.automatic_cbpv_coercions = automatic_cbpv_coercions != 0;
	ctx.imported_interfaces = imported_interfaces;
	ctx.imported_interface_count = imported_interface_count;
	sync_universe_level_counters(&ctx);

	if (compile_phase_report_def_index_errors(&ctx) != 0) {
		return -1;
	}
	if (compile_phase_build_graph(&ctx) != 0) {
		return -1;
	}
	if (begin_resolution_iteration(&ctx, 0, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (compile_phase_infer_imported_constructor_classifiers(&ctx) != 0) {
		return -1;
	}
	if (canonicalize_type_view_core_refs(terms, type_declarations) != 0 ||
		canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_resolve_pending_match_items(&ctx) < 0) {
		return -1;
	}
	if (finish_resolution_iteration(&ctx, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (canonicalize_type_view_core_refs(terms, type_declarations) != 0 ||
		canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_infer_pending_types(&ctx) != 0) {
		return -1;
	}
	/* Source APP/LAMBDA/MATCH derivations are already committed above. Solve
	 * only CBPV boundaries and sequencing constraints here; re-running the
	 * generic term solver would create competing type-formation derivations. */
		if (prototype_judgement_delta_solve_computation_constraints(
			&ctx.judgement_delta,
			terms,
			type_declarations
		) != 0 ||
			prototype_judgement_delta_commit(&ctx.judgement_delta, 0) != 0 ||
			compile_phase_record_residual_dependent_binds(&ctx) != 0) {
			return -1;
		}
	operation_solver_refresh_constraint_states(&ctx, ctx.metadata->solver_exhausted);
	compile_metadata_refresh_runtime_capabilities(metadata, terms);
	if (metadata && metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		prototype_verification_db_count(&metadata->verification) != 0) {
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
	if (canonicalize_type_view_core_refs(terms, type_declarations) != 0 ||
		canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (prototype_judgement_expand_primitives(judgement, terms) != 0) {
		return -1;
	}
	prototype_judgement_resolve_declaration_premises(
		terms,
		type_declarations,
		judgement
	);
	prototype_judgement_refresh_app_elim_premises(judgement, terms, type_declarations);
	if (prototype_judgement_add_normalization_premise_conversions(
			terms,
			type_declarations,
			judgement
		) != 0) {
		return -1;
	}
	prototype_judgement_resolve_proof_edges(judgement);
	if (prototype_judgement_validate_proofs(terms, type_declarations, judgement) != 0) {
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
		1,
		NULL,
		0
	);
}
