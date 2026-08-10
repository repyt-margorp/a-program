#include "a_program/graph/operation_graph.h"
#include "a_program/graph/compile_metadata.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/ast_common.h"

void prototype_operation_graph_init(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* cases,
	size_t case_capacity,
	struct prototype_operation_computation_fold_clause* fold_clauses,
	size_t fold_clause_capacity
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	graph->operations = operations;
	graph->operation_capacity = operation_capacity;
	graph->cases = cases;
	graph->case_capacity = case_capacity;
	graph->fold_clauses = fold_clauses;
	graph->fold_clause_capacity = fold_clause_capacity;
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

int prototype_operation_graph_selected_classifier(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	const struct prototype_operation_node* operation =
		prototype_operation_graph_get(graph, operation_id);
	if (!operation || !p_classifier ||
		operation->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_classifier = operation->classifier;
	return 0;
}

const struct prototype_operation_match_case* prototype_operation_graph_get_case(
	const struct prototype_operation_graph* graph,
	uint32_t case_id
) {
	return graph && case_id < graph->case_count ?
		&graph->cases[case_id] : NULL;
}

const struct prototype_operation_computation_fold_clause*
prototype_operation_graph_get_fold_clause(
	const struct prototype_operation_graph* graph,
	uint32_t clause_id
) {
	return graph && clause_id < graph->fold_clause_count ?
		&graph->fold_clauses[clause_id] : NULL;
}

int prototype_operation_graph_add(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_node operation,
	uint32_t* p_operation_id
) {
	if (!graph || !contexts || !graph->operations ||
		!prototype_context_get(contexts, operation.context_id) ||
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
	const struct prototype_context_db* contexts,
	struct prototype_operation_match_case operation_case,
	uint32_t* p_case_id
) {
	if (!graph || !contexts || !graph->cases ||
		!prototype_context_get(contexts, operation_case.context_id) ||
		reserve_slot(graph->case_count, graph->case_capacity) != 0) {
		return -1;
	}
	if (p_case_id) {
		*p_case_id = (uint32_t)graph->case_count;
	}
	graph->cases[graph->case_count++] = operation_case;
	return 0;
}

int prototype_operation_graph_add_fold_clause(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_computation_fold_clause clause,
	uint32_t* p_clause_id
) {
	if (!graph || !contexts || !graph->fold_clauses ||
		!prototype_context_get(contexts, clause.context_id) ||
		reserve_slot(graph->fold_clause_count, graph->fold_clause_capacity) != 0) {
		return -1;
	}
	if (p_clause_id) {
		*p_clause_id = (uint32_t)graph->fold_clause_count;
	}
	graph->fold_clauses[graph->fold_clause_count++] = clause;
	return 0;
}

static int operation_graph_reaches_at_depth(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation,
	unsigned char* visited
) {
	if (!graph || !visited || root_operation >= graph->operation_count ||
		target_operation >= graph->operation_count) {
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
		&graph->operations[root_operation];
	uint32_t children[5] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	size_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->scrutinee;
			children[child_count++] = operation->fold_return_operation;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_VAR:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
			break;
		default:
			return -1;
	}
	for (size_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int reaches = operation_graph_reaches_at_depth(
			graph, children[i], target_operation, visited
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
		if (operation->case_count != 0 &&
			(operation->first_case == PROTOTYPE_INVALID_ID ||
			 operation->first_case > graph->case_count ||
			 operation->case_count > graph->case_count - operation->first_case)) {
			return -1;
		}
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			int reaches = operation_graph_reaches_at_depth(
				graph,
				graph->cases[operation->first_case + i].body_operation,
				target_operation,
				visited
			);
			if (reaches != 0) {
				return reaches;
			}
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
		if (operation->fold_clause_count != 0 &&
			(operation->first_fold_clause == PROTOTYPE_INVALID_ID ||
			 operation->first_fold_clause > graph->fold_clause_count ||
			operation->fold_clause_count >
				graph->fold_clause_count - operation->first_fold_clause)) {
			return -1;
		}
		for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
			const struct prototype_operation_computation_fold_clause* clause =
				&graph->fold_clauses[operation->first_fold_clause + i];
			uint32_t clause_children[] = {
				clause->operation_operation,
				clause->body_operation,
				clause->clause_operation
			};
			for (size_t j = 0;
				j < sizeof(clause_children) / sizeof(clause_children[0]);
				++j) {
				int reaches = operation_graph_reaches_at_depth(
					graph, clause_children[j], target_operation, visited
				);
				if (reaches != 0) {
					return reaches;
				}
			}
		}
	}
	return 0;
}

int prototype_operation_graph_reaches(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!graph || root_operation >= graph->operation_count ||
		target_operation >= graph->operation_count) {
		return -1;
	}
	unsigned char visited[graph->operation_count];
	memset(visited, 0, sizeof(visited));
	return operation_graph_reaches_at_depth(
		graph, root_operation, target_operation, visited
	);
}

static int term_is_saturated_effect_application(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	uint32_t argument_count = 0;
	uint32_t head = term_id;
	if (!terms || term_id >= terms->term_count) {
		return 0;
	}
	while (head < terms->term_count &&
		terms->terms[head].tag == PROTOTYPE_TERM_APP) {
		head = terms->terms[head].as.app.function;
		argument_count++;
	}
	if (head >= terms->term_count ||
		terms->terms[head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return 0;
	}
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(
			terms->terms[head].as.effect_operation.operation_id
		);
	return declaration && argument_count >= declaration->arity;
}

int prototype_operation_graph_validate(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || !terms || !contexts ||
		(graph->operation_count != 0 && !graph->operations) ||
		(graph->case_count != 0 && !graph->cases) ||
		(graph->fold_clause_count != 0 && !graph->fold_clauses)) {
		return -1;
	}
	for (uint32_t i = 0; i < graph->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(graph, i);
		if (!operation ||
			operation->tag < PROTOTYPE_OPERATION_ATOM ||
			operation->tag > PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			!prototype_context_get(contexts, operation->context_id)) {
			return -1;
		}
		if (operation->application_role < PROTOTYPE_TERM_APPLICATION_NONE ||
			operation->application_role >
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION ||
			(operation->tag != PROTOTYPE_OPERATION_APP &&
			 operation->application_role != PROTOTYPE_TERM_APPLICATION_NONE) ||
			(operation->tag == PROTOTYPE_OPERATION_APP &&
			 operation->application_role == PROTOTYPE_TERM_APPLICATION_NONE)) {
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
		if (operation->tag == PROTOTYPE_OPERATION_APP) {
			struct prototype_term_semantics semantics;
			if (prototype_term_semantics(
					terms, operation->core_term, &semantics
				) != 0 ||
				semantics.application_role != operation->application_role ||
				term_is_saturated_effect_application(terms, operation->core_term) ||
				(operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				 operation->polarity != PROTOTYPE_OPERATION_POLARITY_VALUE)) {
				return -1;
			}
		}
		uint32_t operation_references[] = {
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee,
			operation->fold_return_operation
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
		if (operation->first_fold_clause != PROTOTYPE_INVALID_ID &&
			(operation->first_fold_clause > graph->fold_clause_count ||
			 operation->fold_clause_count >
				graph->fold_clause_count - operation->first_fold_clause)) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
			if (terms->terms[operation->core_term].tag !=
					PROTOTYPE_TERM_COMPUTATION_FOLD ||
				operation->function >= graph->operation_count) {
				return -1;
			}
			const struct prototype_term* fold =
				&terms->terms[operation->core_term];
			uint32_t return_operation_id =
				fold->as.computation_fold.clause_count == 0 ?
					operation->argument : operation->fold_return_operation;
			if (return_operation_id >= graph->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* input_operation =
				prototype_operation_graph_get(graph, operation->function);
			const struct prototype_operation_node* return_operation =
				prototype_operation_graph_get(graph, return_operation_id);
			if (!input_operation || !return_operation ||
				input_operation->core_term != fold->as.computation_fold.computation ||
				return_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
				return_operation->core_term != fold->as.computation_fold.return_clause ||
				operation->fold_clause_count != fold->as.computation_fold.clause_count) {
				return -1;
			}
			if (fold->as.computation_fold.clause_count != 0 &&
				(operation->first_fold_clause == PROTOTYPE_INVALID_ID ||
				operation->first_fold_clause > graph->fold_clause_count ||
				operation->fold_clause_count >
					graph->fold_clause_count - operation->first_fold_clause ||
				fold->as.computation_fold.first_clause >
					terms->computation_fold_clause_count ||
				fold->as.computation_fold.clause_count >
					terms->computation_fold_clause_count -
						fold->as.computation_fold.first_clause)) {
				return -1;
			}
			for (uint32_t clause_index = 0;
				clause_index < fold->as.computation_fold.clause_count;
				++clause_index) {
				const struct prototype_operation_computation_fold_clause* occurrence =
					prototype_operation_graph_get_fold_clause(
						graph, operation->first_fold_clause + clause_index
					);
				const struct prototype_computation_fold_clause* core_clause =
					&terms->computation_fold_clauses[
						fold->as.computation_fold.first_clause + clause_index
					];
				const struct prototype_operation_node* operation_occurrence =
					occurrence ? prototype_operation_graph_get(
						graph, occurrence->operation_operation
					) : NULL;
				const struct prototype_operation_node* clause_occurrence =
					occurrence ? prototype_operation_graph_get(
						graph, occurrence->clause_operation
					) : NULL;
				if (!occurrence || !operation_occurrence || !clause_occurrence ||
					operation_occurrence->core_term != core_clause->operation ||
					clause_occurrence->tag != PROTOTYPE_OPERATION_LAMBDA ||
					clause_occurrence->core_term != core_clause->body) {
					return -1;
				}
			}
		}
	}
	for (uint32_t i = 0; i < graph->case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(graph, i);
		if (!operation_case ||
			operation_case->binder_count > 16 ||
			operation_case->body_operation >= graph->operation_count ||
			!prototype_context_get(contexts, operation_case->context_id) ||
			operation_case->constructor_owner >= terms->term_count ||
			terms->terms[operation_case->constructor_owner].tag == 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->fold_clause_count; ++i) {
		const struct prototype_operation_computation_fold_clause* clause =
			prototype_operation_graph_get_fold_clause(graph, i);
		if (!clause || clause->operation_operation >= graph->operation_count ||
			clause->body_operation >= graph->operation_count ||
			clause->clause_operation >= graph->operation_count ||
			!prototype_context_get(contexts, clause->context_id)) {
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
	graph->fold_clauses = metadata->operation_fold_clauses;
	graph->fold_clause_count = metadata->operation_fold_clause_count;
	graph->fold_clause_capacity = metadata->operation_fold_clause_capacity;
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
	graph->fold_clauses = metadata->operation_fold_clauses;
	graph->fold_clause_count = metadata->operation_fold_clause_count;
	graph->fold_clause_capacity = metadata->operation_fold_clause_capacity;
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
	metadata->operation_fold_clause_count = graph->fold_clause_count;
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
		case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
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
			case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
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
			case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
				p_coverage->required_runtime_capabilities |=
					PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
					PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
				break;
			default:
				return -1;
		}
	}
	return 0;
}

uint64_t prototype_backend_default_capabilities(int backend) {
	switch (backend) {
		case PROTOTYPE_BACKEND_INTERPRETER:
			return PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
				PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH |
				PROTOTYPE_RUNTIME_CAPABILITY_HANDLER |
				PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
		case PROTOTYPE_BACKEND_C:
			/* This is the contract required of a future generated C runtime. */
			return PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
				PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH |
				PROTOTYPE_RUNTIME_CAPABILITY_HANDLER |
				PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
		case PROTOTYPE_BACKEND_VERILOG:
			return 0;
		default:
			return 0;
	}
}

int prototype_compile_metadata_validate_backend(
	const struct prototype_compile_metadata* metadata,
	int backend,
	uint64_t available_runtime_capabilities
) {
	if (!metadata ||
		backend < PROTOTYPE_BACKEND_INTERPRETER ||
		backend > PROTOTYPE_BACKEND_VERILOG ||
		metadata->solver_exhausted ||
		metadata->solver_incomplete_count != 0) {
		return -1;
	}
	struct prototype_verification_coverage coverage;
	if (prototype_verification_db_coverage(
			&metadata->verification, &coverage
		) != 0 || coverage.failed_count != 0 ||
		(metadata->required_runtime_capabilities &
			~available_runtime_capabilities) != 0 ||
		(coverage.required_runtime_capabilities &
			~available_runtime_capabilities) != 0) {
		return -1;
	}
	if (metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		coverage.pending_count != 0) {
		return -1;
	}
	if (backend == PROTOTYPE_BACKEND_VERILOG && coverage.pending_count != 0) {
		return -1;
	}
	if (metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_EXPLORATORY &&
		backend != PROTOTYPE_BACKEND_INTERPRETER) {
		return -1;
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

static int verification_db_discharge_family(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t result_classifier,
	int expected_kind
) {
	if (!db || !terms || !type_declarations || obligation_id >= db->obligation_count ||
		returned_value >= terms->term_count ||
		result_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_verification_obligation* obligation =
		prototype_verification_db_get_mutable(db, obligation_id);
	if (!obligation) {
		return -1;
	}
	if (obligation->kind != expected_kind ||
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
		) != 0 || prototype_term_normalize_complete_with_profile(
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
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_classifier,
			result_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return 0;
	}
	obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	return 0;
}

int prototype_verification_db_discharge_computation_fold_result(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t return_result_classifier
) {
	return verification_db_discharge_family(
		db,
		terms,
		type_declarations,
		obligation_id,
		returned_value,
		return_result_classifier,
		PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT
	);
}

enum operation_runtime_value_kind {
	OPERATION_RUNTIME_VALUE_INVALID = 0,
	OPERATION_RUNTIME_VALUE_TERM,
	OPERATION_RUNTIME_VALUE_RESOURCE,
	OPERATION_RUNTIME_VALUE_RESUMPTION
};

struct operation_runtime_resource_reference {
	uint32_t slot;
	uint32_t generation;
};

struct operation_runtime_value {
	int kind;
	union {
		uint32_t term;
		struct operation_runtime_resource_reference resource;
		uint32_t resumption;
	} as;
};

struct operation_runtime_binding {
	uint32_t ast_binder_id;
	uint32_t binding_id;
	struct operation_runtime_value value;
};

static struct operation_runtime_value operation_runtime_term_value(uint32_t term)
{
	return (struct operation_runtime_value){
		.kind = OPERATION_RUNTIME_VALUE_TERM,
		.as.term = term
	};
}

static struct operation_runtime_value operation_runtime_resumption_value(
	uint32_t resumption
)
{
	return (struct operation_runtime_value){
		.kind = OPERATION_RUNTIME_VALUE_RESUMPTION,
		.as.resumption = resumption
	};
}

static int operation_runtime_value_term(
	struct operation_runtime_value value,
	uint32_t* p_term
)
{
	if (!p_term || value.kind != OPERATION_RUNTIME_VALUE_TERM) {
		return -1;
	}
	*p_term = value.as.term;
	return 0;
}

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
		if (environment->bindings[i].value.kind != OPERATION_RUNTIME_VALUE_TERM) {
			continue;
		}
		if (prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				current,
				environment->bindings[i].binding_id,
				environment->bindings[i].value.as.term,
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
	uint32_t binding_id,
	struct operation_runtime_value value,
	struct operation_runtime_environment* p_ret
) {
	if (!source || !p_ret || value.kind == OPERATION_RUNTIME_VALUE_INVALID ||
		source->count >= 512) {
		return -1;
	}
	*p_ret = *source;
	p_ret->bindings[p_ret->count++] =
		(struct operation_runtime_binding){
			.ast_binder_id = ast_binder_id,
			.binding_id = binding_id,
			.value = value
		};
	return 0;
}

static int operation_runtime_extend_term_environment(
	const struct operation_runtime_environment* source,
	uint32_t ast_binder_id,
	uint32_t binding_id,
	uint32_t term,
	struct operation_runtime_environment* p_ret
)
{
	if (term == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return operation_runtime_extend_environment(
		source,
		ast_binder_id,
		binding_id,
		operation_runtime_term_value(term),
		p_ret
	);
}

static int operation_runtime_extend_resumption_environment(
	const struct operation_runtime_environment* source,
	uint32_t ast_binder_id,
	uint32_t binding_id,
	uint32_t resumption,
	struct operation_runtime_environment* p_ret
) {
	if (!source || !p_ret || source->count >= 512) {
		return -1;
	}
	return operation_runtime_extend_environment(
		source,
		ast_binder_id,
		binding_id,
		operation_runtime_resumption_value(resumption),
		p_ret
	);
}

static const struct operation_runtime_binding* operation_runtime_lookup_binding(
	const struct operation_runtime_environment* environment,
	uint32_t ast_binder_id
) {
	if (!environment || ast_binder_id == PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	for (uint32_t i = environment->count; i > 0; --i) {
		if (environment->bindings[i - 1].ast_binder_id == ast_binder_id) {
			return &environment->bindings[i - 1];
		}
	}
	return NULL;
}

static int operation_runtime_lookup_value(
	const struct operation_runtime_environment* environment,
	uint32_t ast_binder_id,
	struct operation_runtime_value* p_value
) {
	if (!environment || !p_value || ast_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct operation_runtime_binding* binding =
		operation_runtime_lookup_binding(environment, ast_binder_id);
	if (binding) {
		*p_value = binding->value;
		return 0;
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

static int operation_runtime_discharge_sequence_fold(
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
		PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
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
		) != 0 || prototype_term_normalize_complete_with_profile(
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
	if (prototype_verification_db_discharge_computation_fold_result(
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

enum operation_runtime_frame_kind {
	OPERATION_RUNTIME_FRAME_RETURN = 1,
	OPERATION_RUNTIME_FRAME_SEQUENCE_FOLD,
	OPERATION_RUNTIME_FRAME_MATCH,
	OPERATION_RUNTIME_FRAME_HANDLE,
	OPERATION_RUNTIME_FRAME_COMPUTATION_FOLD_RESULT,
	OPERATION_RUNTIME_FRAME_APP,
	OPERATION_RUNTIME_FRAME_PERFORM_ARGUMENT,
	OPERATION_RUNTIME_FRAME_RESUME,
	OPERATION_RUNTIME_FRAME_RESUMPTION_RETURN
};

struct operation_runtime_frame {
	int kind;
	uint32_t operation;
	uint32_t environment_count;
	uint32_t handler_count;
	uint32_t obligation_instance;
	struct prototype_term_reduction_options options;
};

struct operation_runtime_obligation_instance {
	uint32_t obligation;
	uint32_t operation;
	uint32_t returned_value;
	int state;
};

struct operation_runtime_request {
	uint32_t operation;
	struct operation_runtime_value argument;
	uint32_t continuation;
	int resumption_multiplicity;
	int consumed;
	struct operation_runtime_environment environment;
	struct prototype_term_reduction_options options;
	struct operation_runtime_frame frames[512];
	uint32_t frame_count;
};

struct operation_runtime_resumption_return {
	struct operation_runtime_environment environment;
	struct prototype_term_reduction_options options;
	uint32_t handlers[256];
	uint32_t handler_count;
};

struct operation_runtime_machine {
	struct prototype_compile_metadata* metadata;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_term_definition_env* definitions;
	struct prototype_term_reduction_options options;
	struct operation_runtime_environment environment;
	struct operation_runtime_frame frames[2048];
	uint32_t frame_count;
	uint32_t handlers[256];
	uint32_t handler_count;
	struct operation_runtime_obligation_instance obligation_instances[1024];
	uint32_t obligation_instance_count;
	struct operation_runtime_request resumptions[64];
	uint32_t resumption_count;
	struct operation_runtime_resumption_return resumption_returns[64];
	uint32_t resumption_return_count;
	struct operation_runtime_request request;
	int has_request;
	uint32_t current_operation;
	struct operation_runtime_value result;
	int evaluating;
	int verification_state;
	int failure_kind;
};

static int operation_runtime_machine_push(
	struct operation_runtime_machine* machine,
	int kind,
	uint32_t operation,
	uint32_t obligation_instance
) {
	if (!machine || machine->frame_count >= 2048) {
		if (machine) {
			machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY;
		}
		return -1;
	}
	machine->frames[machine->frame_count++] = (struct operation_runtime_frame){
		.kind = kind,
		.operation = operation,
		.environment_count = machine->environment.count,
		.handler_count = machine->handler_count,
		.obligation_instance = obligation_instance,
		.options = machine->options
	};
	return 0;
}

static int operation_runtime_machine_add_obligation_instance(
	struct operation_runtime_machine* machine,
	int kind,
	uint32_t operation,
	uint32_t* p_instance
) {
	if (!machine || !p_instance) {
		return -1;
	}
	uint32_t obligation;
	int status = prototype_verification_db_find_operation(
		&machine->metadata->verification,
		kind,
		operation,
		&obligation
	);
	if (status > 0) {
		*p_instance = PROTOTYPE_INVALID_ID;
		return 0;
	}
	if (status < 0 || machine->obligation_instance_count >= 1024) {
		if (machine->obligation_instance_count >= 1024) {
			machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY;
		}
		return -1;
	}
	uint32_t instance = machine->obligation_instance_count++;
	machine->obligation_instances[instance] =
		(struct operation_runtime_obligation_instance){
			.obligation = obligation,
			.operation = operation,
			.returned_value = PROTOTYPE_INVALID_ID,
			.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING
		};
	*p_instance = instance;
	return 0;
}

static int operation_runtime_discharge_computation_fold_result(
	struct operation_runtime_machine* machine,
	uint32_t instance_id
) {
	if (!machine || instance_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (instance_id >= machine->obligation_instance_count) {
		return -1;
	}
	struct operation_runtime_obligation_instance* instance =
		&machine->obligation_instances[instance_id];
	const struct prototype_verification_obligation* obligation =
		prototype_verification_db_get(
			&machine->metadata->verification, instance->obligation
		);
	if (!obligation ||
		obligation->kind != PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT ||
		instance->operation >= machine->metadata->operation_count ||
		obligation->continuation_operation >= machine->metadata->operation_count ||
		instance->returned_value >= machine->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* return_operation =
		&machine->metadata->operations[obligation->continuation_operation];
	uint32_t return_classifier;
	if (return_operation->classifier == PROTOTYPE_INVALID_ID ||
		operation_runtime_instantiate_term(
			machine->terms,
			machine->type_declarations,
			&machine->environment,
			return_operation->classifier,
			&return_classifier
		) != 0) {
		return -1;
	}
	struct prototype_verification_obligation frame_obligation = *obligation;
	frame_obligation.state = instance->state;
	struct prototype_verification_db frame_verification;
	prototype_verification_db_init(&frame_verification, &frame_obligation, 1);
	if (prototype_verification_db_add(
			&frame_verification, frame_obligation, NULL
		) != 0 || prototype_verification_db_discharge_computation_fold_result(
			&frame_verification,
			machine->terms,
			machine->type_declarations,
			0,
			instance->returned_value,
			return_classifier
		) != 0 || frame_obligation.state !=
			PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
		instance->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		machine->verification_state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return -1;
	}
	instance->state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	machine->verification_state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	return 0;
}

static int operation_runtime_machine_request_from_result(
	struct operation_runtime_machine* machine
) {
	uint32_t result;
	if (!machine || operation_runtime_value_term(machine->result, &result) != 0 ||
		result >= machine->terms->term_count ||
		machine->terms->terms[result].tag !=
			PROTOTYPE_TERM_OPERATION_REQUEST) {
		return 0;
	}
	const struct prototype_term* request = &machine->terms->terms[result];
	int operation_id;
	if (prototype_term_effect_operation_identity(
			machine->terms, request->as.operation_request.operation, &operation_id
		) != 0) {
		return -1;
	}
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!declaration) {
		return -1;
	}
	memset(&machine->request, 0, sizeof(machine->request));
	machine->request.operation = request->as.operation_request.operation;
	machine->request.argument = operation_runtime_term_value(
		request->as.operation_request.argument
	);
	machine->request.continuation = request->as.operation_request.continuation;
	machine->request.resumption_multiplicity = declaration->resumption_multiplicity;
	machine->request.environment = machine->environment;
	machine->request.options = machine->options;
	machine->has_request = 1;
	return 0;
}

static int operation_runtime_machine_capture_frame(
	struct operation_runtime_machine* machine,
	const struct operation_runtime_frame* frame
) {
	if (!machine || !frame || !machine->has_request ||
		machine->request.frame_count >= 512) {
		if (machine) {
			machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY;
		}
		return -1;
	}
	machine->request.frames[machine->request.frame_count++] = *frame;
	return 0;
}

static int operation_runtime_machine_resumption_function(
	const struct operation_runtime_machine* machine,
	uint32_t function_operation,
	uint32_t* p_resumption
) {
	if (!machine || !p_resumption) {
		return -1;
	}
	uint32_t operation_id = operation_runtime_unwrap_name(
		machine->metadata, function_operation
	);
	if (operation_id >= machine->metadata->operation_count) {
		return 1;
	}
	const struct prototype_operation_node* operation =
		&machine->metadata->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_FORCE) {
		operation_id = operation_runtime_unwrap_name(
			machine->metadata, operation->argument
		);
		if (operation_id >= machine->metadata->operation_count) {
			return 1;
		}
		operation = &machine->metadata->operations[operation_id];
	}
	if (operation->tag != PROTOTYPE_OPERATION_VAR) {
		return 1;
	}
	const struct operation_runtime_binding* binding =
		operation_runtime_lookup_binding(
			&machine->environment, operation->referenced_ast_binder_id
		);
	if (!binding || binding->value.kind != OPERATION_RUNTIME_VALUE_RESUMPTION ||
		binding->value.as.resumption >= machine->resumption_count) {
		return 1;
	}
	*p_resumption = binding->value.as.resumption;
	return 0;
}

static int operation_runtime_machine_invoke_resumption(
	struct operation_runtime_machine* machine,
	uint32_t resumption_id,
	struct operation_runtime_value argument
) {
	if (!machine || resumption_id >= machine->resumption_count) {
		return -1;
	}
	struct operation_runtime_request* resumption =
		&machine->resumptions[resumption_id];
	if (resumption->resumption_multiplicity ==
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE ||
		(resumption->resumption_multiplicity ==
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
			resumption->consumed)) {
		return -1;
	}
	if (resumption->resumption_multiplicity ==
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT) {
		resumption->consumed = 1;
	}
	if (machine->resumption_return_count >= 64 ||
		machine->frame_count + resumption->frame_count + 1 > 2048) {
		machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY;
		return -1;
	}
	uint32_t return_state = machine->resumption_return_count++;
	struct operation_runtime_resumption_return* saved =
		&machine->resumption_returns[return_state];
	saved->environment = machine->environment;
	saved->options = machine->options;
	saved->handler_count = machine->handler_count;
	memcpy(
		saved->handlers,
		machine->handlers,
		machine->handler_count * sizeof(*saved->handlers)
	);
	if (operation_runtime_machine_push(
			machine,
			OPERATION_RUNTIME_FRAME_RESUMPTION_RETURN,
			machine->current_operation,
			return_state
		) != 0) {
		return -1;
	}
	for (uint32_t i = resumption->frame_count; i > 0; --i) {
		machine->frames[machine->frame_count++] = resumption->frames[i - 1];
	}
	machine->environment = resumption->environment;
	machine->options = resumption->options;
	uint32_t argument_term;
	uint32_t forced;
	uint32_t application;
	uint32_t performed;
	if (operation_runtime_value_term(argument, &argument_term) != 0 ||
		prototype_term_force(
			machine->terms, resumption->continuation, &forced
		) != 0 || prototype_term_app(
			machine->terms, forced, argument_term, &application
		) != 0 || prototype_term_perform_with_options(
			machine->terms,
			machine->type_declarations,
			machine->definitions,
			machine->options,
			application,
			&performed
		) != 0) {
		return -1;
	}
	machine->result = operation_runtime_term_value(performed);
	machine->has_request = 0;
	if (operation_runtime_machine_request_from_result(machine) != 0) {
		return -1;
	}
	machine->evaluating = 0;
	return 0;
}

static int operation_runtime_machine_enter_lambda_body(
	struct operation_runtime_machine* machine,
	uint32_t lambda_operation,
	struct operation_runtime_value argument
) {
	if (!machine || lambda_operation >= machine->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* lambda_operation_node =
		&machine->metadata->operations[lambda_operation];
	if (lambda_operation_node->tag != PROTOTYPE_OPERATION_LAMBDA ||
		lambda_operation_node->core_term >= machine->terms->term_count ||
		machine->terms->terms[lambda_operation_node->core_term].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		lambda_operation_node->body >= machine->metadata->operation_count) {
		return -1;
	}
	const struct prototype_term* lambda =
		&machine->terms->terms[lambda_operation_node->core_term];
	struct operation_runtime_environment extended;
	if (operation_runtime_extend_environment(
			&machine->environment,
			lambda_operation_node->referenced_ast_binder_id,
			lambda->as.lambda.binding_id,
			argument,
			&extended
		) != 0) {
		return -1;
	}
	machine->environment = extended;
	machine->current_operation = lambda_operation_node->body;
	machine->evaluating = 1;
	return 0;
}

static int operation_runtime_machine_enter_match_case(
	struct operation_runtime_machine* machine,
	uint32_t match_operation_id,
	uint32_t scrutinee
) {
	if (!machine || match_operation_id >= machine->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&machine->metadata->operations[match_operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->core_term >= machine->terms->term_count ||
		machine->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		operation->first_case == PROTOTYPE_INVALID_ID ||
		operation->first_case + operation->case_count >
			machine->metadata->operation_case_count) {
		return -1;
	}
	if (scrutinee < machine->terms->term_count &&
		machine->terms->terms[scrutinee].tag == PROTOTYPE_TERM_RETURN) {
		scrutinee = machine->terms->terms[scrutinee].as.return_term.value;
	}
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t constructor_head;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_term_constructor_spine_info(
			machine->terms,
			scrutinee,
			&constructor_head,
			&owner,
			&constructor_id,
			arguments,
			64,
			&argument_count
		) != 0) {
		return -1;
	}
	(void)constructor_head;
	const struct prototype_term* match =
		&machine->terms->terms[operation->core_term];
	for (uint32_t i = 0; i < operation->case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			&machine->metadata->operation_cases[operation->first_case + i];
		if (operation_case->constructor_id != constructor_id) {
			continue;
		}
		int owner_equal = 0;
		if (prototype_term_core_shape_equal(
				machine->terms,
				operation_case->constructor_owner,
				owner,
				&owner_equal
			) != 0) {
			return -1;
		}
		if (!owner_equal) {
			continue;
		}
		uint32_t term_case_id = match->as.match.first_case + i;
		if (term_case_id >= machine->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* term_case =
			&machine->terms->cases[term_case_id];
		if (operation_case->binder_count != term_case->binder_count ||
			term_case->binder_count != argument_count ||
			term_case->first_binder + term_case->binder_count >
				machine->terms->case_binder_count) {
			return -1;
		}
		for (uint32_t j = 0; j < argument_count; ++j) {
			struct operation_runtime_environment extended;
			if (operation_runtime_extend_term_environment(
					&machine->environment,
					operation_case->ast_binder_ids[j],
					machine->terms->case_binders[
						term_case->first_binder + j
					].binding_id,
					arguments[j],
					&extended
				) != 0) {
				return -1;
			}
			machine->environment = extended;
		}
		machine->current_operation = operation_case->body_operation;
		machine->evaluating = 1;
		return 0;
	}
	return -1;
}

static int operation_runtime_machine_leaf(
	struct operation_runtime_machine* machine,
	const struct prototype_operation_node* operation
) {
	uint32_t instantiated;
	uint32_t performed;
	if (!machine || !operation ||
		operation_runtime_instantiate_term(
			machine->terms,
			machine->type_declarations,
			&machine->environment,
			operation->core_term,
			&instantiated
		) != 0 ||
		prototype_term_perform_with_options(
			machine->terms,
			machine->type_declarations,
			machine->definitions,
			machine->options,
			instantiated,
			&performed
		) != 0) {
		return -1;
	}
	machine->result = operation_runtime_term_value(performed);
	machine->has_request = 0;
	if (operation_runtime_machine_request_from_result(machine) != 0) {
		return -1;
	}
	machine->evaluating = 0;
	return 0;
}

static int operation_runtime_machine_step_evaluate(
	struct operation_runtime_machine* machine
) {
	if (!machine ||
		machine->current_operation >= machine->metadata->operation_count) {
		return -1;
	}
	machine->current_operation = operation_runtime_unwrap_name(
		machine->metadata, machine->current_operation
	);
	if (machine->current_operation >= machine->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&machine->metadata->operations[machine->current_operation];
	if (operation->core_term >= machine->terms->term_count) {
		return -1;
	}
	if (operation->tag == PROTOTYPE_OPERATION_VAR) {
		struct operation_runtime_value value;
		int lookup_status = operation_runtime_lookup_value(
			&machine->environment,
			operation->referenced_ast_binder_id,
			&value
		);
		if (lookup_status == 0) {
			machine->result = value;
			machine->evaluating = 0;
			return 0;
		}
		if (lookup_status < 0) {
			return -1;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_RETURN) {
		if (operation->argument >= machine->metadata->operation_count ||
			operation_runtime_machine_push(
				machine,
				OPERATION_RUNTIME_FRAME_RETURN,
				machine->current_operation,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
		machine->current_operation = operation->argument;
		return 0;
	}
	if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
		uint32_t obligation_instance;
		if (operation->core_term >= machine->terms->term_count ||
			machine->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			operation->function >= machine->metadata->operation_count ||
			operation_runtime_machine_add_obligation_instance(
				machine,
				PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
				machine->current_operation,
				&obligation_instance
			) != 0 ||
			operation_runtime_machine_push(machine,
				machine->terms->terms[operation->core_term].as.computation_fold.clause_count == 0 ?
					OPERATION_RUNTIME_FRAME_SEQUENCE_FOLD : OPERATION_RUNTIME_FRAME_HANDLE,
				machine->current_operation, obligation_instance) != 0) {
			return -1;
		}
		if (machine->terms->terms[operation->core_term].as.computation_fold.clause_count > 0) {
			if (machine->handler_count >= 256) {
				return -1;
			}
			machine->handlers[machine->handler_count++] = machine->current_operation;
			machine->options.flags &= ~PROTOTYPE_TERM_PERFORM_HOST_EFFECT;
			machine->options.operation_dispatch = NULL;
			machine->options.operation_dispatch_context = NULL;
		}
		machine->current_operation = operation->function;
		return 0;
	}
	if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
		if (operation->scrutinee >= machine->metadata->operation_count ||
			operation_runtime_machine_push(
				machine,
				OPERATION_RUNTIME_FRAME_MATCH,
				machine->current_operation,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
		machine->current_operation = operation->scrutinee;
		return 0;
	}
	if (operation->tag == PROTOTYPE_OPERATION_APP) {
		uint32_t resumption;
		int resumption_status = operation_runtime_machine_resumption_function(
			machine, operation->function, &resumption
		);
		if (resumption_status < 0) {
			return -1;
		}
		if (resumption_status == 0) {
			if (operation->argument >= machine->metadata->operation_count ||
				operation_runtime_machine_push(
					machine,
					OPERATION_RUNTIME_FRAME_RESUME,
					machine->current_operation,
					resumption
				) != 0) {
				return -1;
			}
			machine->current_operation = operation->argument;
			return 0;
		}
		if (operation->function >= machine->metadata->operation_count ||
			operation->argument >= machine->metadata->operation_count ||
			operation_runtime_machine_push(
				machine,
				OPERATION_RUNTIME_FRAME_APP,
				machine->current_operation,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
		machine->current_operation = operation->argument;
		return 0;
	}
	if (operation->tag == PROTOTYPE_OPERATION_REQUEST) {
		if (operation->function >= machine->metadata->operation_count ||
			operation->argument >= machine->metadata->operation_count ||
			operation->body >= machine->metadata->operation_count ||
			operation_runtime_machine_push(
				machine,
				OPERATION_RUNTIME_FRAME_PERFORM_ARGUMENT,
				machine->current_operation,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
		machine->current_operation = operation->argument;
		return 0;
	}
	if (operation->tag == PROTOTYPE_OPERATION_FORCE) {
		uint32_t value_operation = operation_runtime_unwrap_name(
			machine->metadata, operation->argument
		);
		if (value_operation < machine->metadata->operation_count &&
			machine->metadata->operations[value_operation].tag ==
				PROTOTYPE_OPERATION_THUNK) {
			uint32_t computation =
				machine->metadata->operations[value_operation].argument;
			if (computation >= machine->metadata->operation_count) {
				return -1;
			}
			machine->current_operation = computation;
			return 0;
		}
	}
	return operation_runtime_machine_leaf(machine, operation);
}

static int operation_runtime_machine_step_unwind(
	struct operation_runtime_machine* machine
) {
	if (!machine || machine->frame_count == 0) {
		return 1;
	}
	struct operation_runtime_frame frame =
		machine->frames[--machine->frame_count];
	if ((frame.kind != OPERATION_RUNTIME_FRAME_RESUMPTION_RETURN &&
			frame.environment_count > machine->environment.count) ||
		frame.operation >= machine->metadata->operation_count) {
		return -1;
	}
	if (frame.kind != OPERATION_RUNTIME_FRAME_RESUMPTION_RETURN) {
		machine->environment.count = frame.environment_count;
		machine->handler_count = frame.handler_count;
		machine->options = frame.options;
	}
	if (machine->has_request && frame.kind != OPERATION_RUNTIME_FRAME_HANDLE) {
		return operation_runtime_machine_capture_frame(machine, &frame);
	}
	const struct prototype_operation_node* operation =
		&machine->metadata->operations[frame.operation];
	switch (frame.kind) {
		case OPERATION_RUNTIME_FRAME_RETURN: {
			uint32_t value;
			if (operation_runtime_value_term(machine->result, &value) != 0) {
				return 0;
			}
			if (value < machine->terms->term_count &&
				machine->terms->terms[value].tag == PROTOTYPE_TERM_RETURN) {
				value = machine->terms->terms[value].as.return_term.value;
			}
			uint32_t returned;
			if (prototype_term_return(machine->terms, value, &returned) != 0) {
				return -1;
			}
			machine->result = operation_runtime_term_value(returned);
			return 0;
		}
		case OPERATION_RUNTIME_FRAME_APP: {
			struct operation_runtime_value argument = machine->result;
			if (argument.kind == OPERATION_RUNTIME_VALUE_TERM &&
				argument.as.term < machine->terms->term_count &&
				machine->terms->terms[argument.as.term].tag == PROTOTYPE_TERM_RETURN) {
				argument.as.term =
					machine->terms->terms[argument.as.term].as.return_term.value;
			}
			uint32_t function_operation = operation_runtime_unwrap_name(
				machine->metadata, operation->function
			);
			if (function_operation < machine->metadata->operation_count &&
				machine->metadata->operations[function_operation].tag ==
					PROTOTYPE_OPERATION_LAMBDA) {
				return operation_runtime_machine_enter_lambda_body(
					machine, function_operation, argument
				);
			}
			if (function_operation >= machine->metadata->operation_count) {
				return -1;
			}
			uint32_t argument_term;
			uint32_t function;
			uint32_t application;
			uint32_t performed;
			if (operation_runtime_value_term(argument, &argument_term) != 0 ||
				operation_runtime_instantiate_term(
					machine->terms,
					machine->type_declarations,
					&machine->environment,
					machine->metadata->operations[function_operation].core_term,
					&function
				) != 0 || prototype_term_app(
					machine->terms, function, argument_term, &application
				) != 0 || prototype_term_perform_with_options(
					machine->terms,
					machine->type_declarations,
					machine->definitions,
					machine->options,
					application,
					&performed
				) != 0) {
				return -1;
			}
			machine->result = operation_runtime_term_value(performed);
			machine->has_request = 0;
			return operation_runtime_machine_request_from_result(machine);
		}
		case OPERATION_RUNTIME_FRAME_PERFORM_ARGUMENT: {
			uint32_t argument;
			if (operation_runtime_value_term(machine->result, &argument) != 0) {
				return -1;
			}
			if (argument < machine->terms->term_count &&
				machine->terms->terms[argument].tag == PROTOTYPE_TERM_RETURN) {
				argument = machine->terms->terms[argument].as.return_term.value;
			}
			uint32_t operation_term;
			uint32_t continuation;
			uint32_t request;
			uint32_t performed;
			if (operation_runtime_instantiate_term(
					machine->terms,
					machine->type_declarations,
					&machine->environment,
					machine->metadata->operations[operation->function].core_term,
					&operation_term
				) != 0 || operation_runtime_instantiate_term(
					machine->terms,
					machine->type_declarations,
					&machine->environment,
					machine->metadata->operations[operation->body].core_term,
					&continuation
				) != 0 || prototype_term_operation_request(
					machine->terms,
					operation_term,
					argument,
					continuation,
					&request
				) != 0 || prototype_term_perform_with_options(
					machine->terms,
					machine->type_declarations,
					machine->definitions,
					machine->options,
					request,
					&performed
				) != 0) {
				return -1;
			}
			machine->result = operation_runtime_term_value(performed);
			machine->has_request = 0;
			return operation_runtime_machine_request_from_result(machine);
		}
		case OPERATION_RUNTIME_FRAME_RESUME: {
			struct operation_runtime_value argument = machine->result;
			if (argument.kind == OPERATION_RUNTIME_VALUE_TERM &&
				argument.as.term < machine->terms->term_count &&
				machine->terms->terms[argument.as.term].tag == PROTOTYPE_TERM_RETURN) {
				argument.as.term =
					machine->terms->terms[argument.as.term].as.return_term.value;
			}
			return operation_runtime_machine_invoke_resumption(
				machine, frame.obligation_instance, argument
			);
		}
		case OPERATION_RUNTIME_FRAME_RESUMPTION_RETURN: {
			if (frame.obligation_instance >= machine->resumption_return_count) {
				return -1;
			}
			const struct operation_runtime_resumption_return* saved =
				&machine->resumption_returns[frame.obligation_instance];
			machine->environment = saved->environment;
			machine->options = saved->options;
			machine->handler_count = saved->handler_count;
			memcpy(
				machine->handlers,
				saved->handlers,
				saved->handler_count * sizeof(*machine->handlers)
			);
			return 0;
		}
		case OPERATION_RUNTIME_FRAME_MATCH: {
			uint32_t scrutinee;
			if (operation_runtime_value_term(machine->result, &scrutinee) != 0) {
				return -1;
			}
			return operation_runtime_machine_enter_match_case(
				machine, frame.operation, scrutinee
			);
		}
		case OPERATION_RUNTIME_FRAME_SEQUENCE_FOLD: {
			uint32_t result;
			if (operation_runtime_value_term(machine->result, &result) != 0 ||
				result >= machine->terms->term_count ||
				machine->terms->terms[result].tag != PROTOTYPE_TERM_RETURN) {
				return -1;
			}
			uint32_t returned_value =
				machine->terms->terms[result].as.return_term.value;
			if (operation_runtime_discharge_sequence_fold(
					machine->metadata,
					machine->terms,
					machine->type_declarations,
					machine->definitions,
					&machine->environment,
					frame.operation,
					returned_value,
					&machine->verification_state
				) != 0) {
				if (frame.obligation_instance != PROTOTYPE_INVALID_ID &&
					frame.obligation_instance < machine->obligation_instance_count) {
					machine->obligation_instances[frame.obligation_instance].state =
						PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
				}
				return -1;
			}
			if (frame.obligation_instance != PROTOTYPE_INVALID_ID &&
				frame.obligation_instance < machine->obligation_instance_count) {
				machine->obligation_instances[frame.obligation_instance].state =
					PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
			}
			uint32_t continuation_operation = operation_runtime_unwrap_name(
				machine->metadata, operation->argument
			);
			return operation_runtime_machine_enter_lambda_body(
				machine,
				continuation_operation,
				operation_runtime_term_value(returned_value)
			);
		}
		case OPERATION_RUNTIME_FRAME_COMPUTATION_FOLD_RESULT:
			if (frame.obligation_instance == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			if (frame.obligation_instance >= machine->obligation_instance_count) {
				return -1;
			}
			if (machine->obligation_instances[frame.obligation_instance].state ==
				PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
				return 0;
			}
			return operation_runtime_discharge_computation_fold_result(
				machine, frame.obligation_instance
			);
		case OPERATION_RUNTIME_FRAME_HANDLE: {
			if (operation->core_term >= machine->terms->term_count ||
				machine->terms->terms[operation->core_term].tag !=
					PROTOTYPE_TERM_COMPUTATION_FOLD) {
				return -1;
			}
			const struct prototype_term* fold_term =
				&machine->terms->terms[operation->core_term];
			if ((size_t)fold_term->as.computation_fold.first_clause +
					fold_term->as.computation_fold.clause_count >
					machine->terms->computation_fold_clause_count ||
				operation->fold_clause_count !=
					fold_term->as.computation_fold.clause_count ||
				(size_t)operation->first_fold_clause + operation->fold_clause_count >
					machine->metadata->operation_fold_clause_count) {
				return -1;
			}
			uint32_t result = PROTOTYPE_INVALID_ID;
			if (!machine->has_request &&
				operation_runtime_value_term(machine->result, &result) == 0 &&
				result < machine->terms->term_count &&
				machine->terms->terms[result].tag == PROTOTYPE_TERM_RETURN) {
				uint32_t returned_value =
					machine->terms->terms[result].as.return_term.value;
				struct operation_runtime_environment extended;
				if (operation_runtime_extend_term_environment(
						&machine->environment,
						operation->fold_return_ast_binder_id,
						operation->fold_return_binder_id,
						returned_value,
						&extended
					) != 0) {
					return -1;
				}
				machine->environment = extended;
				if (frame.obligation_instance != PROTOTYPE_INVALID_ID) {
					if (frame.obligation_instance >=
						machine->obligation_instance_count) {
						return -1;
					}
					machine->obligation_instances[
						frame.obligation_instance
					].returned_value = returned_value;
				}
				if (operation_runtime_machine_push(
						machine,
						OPERATION_RUNTIME_FRAME_COMPUTATION_FOLD_RESULT,
						frame.operation,
						frame.obligation_instance
					) != 0) {
					return -1;
				}
				machine->current_operation = operation->scrutinee;
				machine->evaluating = 1;
				return 0;
			}
			if (!machine->has_request ||
				operation_runtime_machine_capture_frame(machine, &frame) != 0) {
				return -1;
			}
			uint32_t selected_clause = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < fold_term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* fold_clause =
					&machine->terms->computation_fold_clauses[
						fold_term->as.computation_fold.first_clause + i
					];
				int clause_identity;
				int request_identity;
				if (prototype_term_effect_operation_identity(
						machine->terms, fold_clause->operation, &clause_identity
					) != 0 || prototype_term_effect_operation_identity(
						machine->terms, machine->request.operation, &request_identity
					) != 0) {
					return -1;
				}
				if (clause_identity == request_identity) {
					selected_clause = i;
					break;
				}
			}
			if (selected_clause == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			const struct prototype_operation_computation_fold_clause* occurrence_clause =
				&machine->metadata->operation_fold_clauses[
					operation->first_fold_clause + selected_clause
				];
			if (machine->resumption_count >= 64) {
				machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY;
				return -1;
			}
			uint32_t resumption = machine->resumption_count++;
			machine->resumptions[resumption] = machine->request;
			struct operation_runtime_value request_argument = machine->request.argument;
			machine->has_request = 0;
			struct operation_runtime_environment argument_environment;
			struct operation_runtime_environment clause_environment;
			if (operation_runtime_extend_environment(
					&machine->environment,
					occurrence_clause->argument_ast_binder_id,
					occurrence_clause->argument_binder_id,
					request_argument,
					&argument_environment
				) != 0 || operation_runtime_extend_resumption_environment(
					&argument_environment,
					occurrence_clause->continuation_ast_binder_id,
					occurrence_clause->continuation_binder_id,
					resumption,
					&clause_environment
				) != 0) {
				return -1;
			}
			machine->environment = clause_environment;
			machine->current_operation = occurrence_clause->body_operation;
			machine->evaluating = 1;
			return 0;
		}
		default:
			return -1;
	}
}

static int operation_runtime_machine_run(
	struct operation_runtime_machine* machine,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state,
	struct prototype_runtime_trace* p_trace
) {
	if (!machine || !p_ret || operation_id >= machine->metadata->operation_count) {
		return -1;
	}
	machine->current_operation = operation_id;
	machine->evaluating = 1;
	for (;;) {
		int status = machine->evaluating ?
			operation_runtime_machine_step_evaluate(machine) :
			operation_runtime_machine_step_unwind(machine);
		if (status < 0) {
			if (machine->failure_kind == PROTOTYPE_RUNTIME_FAILURE_NONE) {
				machine->failure_kind =
					machine->verification_state ==
						PROTOTYPE_VERIFICATION_OBLIGATION_FAILED ?
					PROTOTYPE_RUNTIME_FAILURE_VERIFICATION :
					PROTOTYPE_RUNTIME_FAILURE_INVALID_OPERATION;
			}
			if (p_verification_state) {
				*p_verification_state = machine->verification_state;
			}
			if (p_trace) {
				memset(p_trace, 0, sizeof(*p_trace));
				p_trace->failure_kind = machine->failure_kind;
				p_trace->failed_operation = machine->current_operation;
				p_trace->frame_count = machine->frame_count < 64 ?
					machine->frame_count : 64;
				for (uint32_t i = 0; i < p_trace->frame_count; ++i) {
					const struct operation_runtime_frame* frame =
						&machine->frames[machine->frame_count - i - 1];
					p_trace->frame_kinds[i] = frame->kind;
					p_trace->frame_operations[i] = frame->operation;
				}
				p_trace->obligation_instance_count =
					machine->obligation_instance_count < 64 ?
					machine->obligation_instance_count : 64;
				for (uint32_t i = 0;
					i < p_trace->obligation_instance_count;
					++i) {
					p_trace->obligation_states[i] =
						machine->obligation_instances[i].state;
					p_trace->obligation_operations[i] =
						machine->obligation_instances[i].operation;
				}
			}
			return -1;
		}
		if (status > 0) {
			for (uint32_t i = 0; i < machine->obligation_instance_count; ++i) {
				const struct operation_runtime_obligation_instance* instance =
					&machine->obligation_instances[i];
				const struct prototype_verification_obligation* obligation =
					prototype_verification_db_get(
						&machine->metadata->verification, instance->obligation
					);
				if (obligation &&
					obligation->kind ==
						PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT &&
					instance->state !=
						PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
					machine->verification_state =
						PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
					machine->failure_kind =
						PROTOTYPE_RUNTIME_FAILURE_VERIFICATION;
					if (p_verification_state) {
						*p_verification_state = machine->verification_state;
					}
					return -1;
				}
			}
			if (operation_runtime_value_term(machine->result, p_ret) != 0) {
				machine->failure_kind = PROTOTYPE_RUNTIME_FAILURE_INVALID_OPERATION;
				return -1;
			}
			if (p_verification_state) {
				*p_verification_state = machine->verification_state;
			}
			return 0;
		}
	}
}

int prototype_operation_evaluate_with_trace(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state,
	struct prototype_runtime_trace* p_trace
) {
	if (p_verification_state) {
		*p_verification_state = 0;
	}
	if (!metadata || !terms || !type_declarations || !p_ret) {
		return -1;
	}
	if (p_trace) {
		memset(p_trace, 0, sizeof(*p_trace));
	}
	struct operation_runtime_machine* machine = calloc(1, sizeof(*machine));
	if (!machine) {
		return -1;
	}
	machine->metadata = metadata;
	machine->terms = terms;
	machine->type_declarations = type_declarations;
	machine->definitions = definitions;
	machine->options = options;
	int status = operation_runtime_machine_run(
		machine, operation_id, p_ret, p_verification_state, p_trace
	);
	free(machine);
	return status;
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
	return prototype_operation_evaluate_with_trace(
		metadata,
		terms,
		type_declarations,
		definitions,
		options,
		operation_id,
		p_ret,
		p_verification_state,
		NULL
	);
}

