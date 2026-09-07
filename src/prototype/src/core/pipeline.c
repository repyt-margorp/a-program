#include "a_program/core/pipeline.h"
#include "a_program/core/read_provider.h"
#include "a_program/core/term.h"

#include <stdlib.h>
#include <string.h>

static const void* core_request_payload_range(
	const struct prototype_core_request_payload* payload,
	struct prototype_core_request_range range,
	size_t element_size
) {
	if (!payload || element_size == 0 ||
		range.element_count > SIZE_MAX / element_size) {
		return NULL;
	}
	size_t byte_count = element_size * range.element_count;
	if (range.byte_offset > payload->byte_count ||
		byte_count > payload->byte_count - range.byte_offset ||
		range.byte_offset %
			_Alignof(union prototype_core_request_alignment) != 0) {
		return NULL;
	}
	return payload->bytes + range.byte_offset;
}

int prototype_core_pipeline_init(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_pipeline_storage* storage,
	const struct prototype_term_definition_env* definitions
) {
	if (!pipeline || !storage || !storage->terms || !storage->term_capacity ||
		!storage->cases || !storage->case_label_symbols ||
		!storage->case_capacity || !storage->case_binders ||
		!storage->case_binder_capacity || !storage->ih_scopes ||
		!storage->ih_scope_capacity) {
		return -1;
	}
	memset(pipeline, 0, sizeof(*pipeline));
	prototype_term_db_init(
		&pipeline->terms,
		storage->terms,
		storage->term_capacity,
		storage->cases,
		storage->case_label_symbols,
		storage->case_capacity,
		storage->case_binders,
		storage->case_binder_capacity,
		storage->ih_scopes,
		storage->ih_scope_capacity
	);
	pipeline->definitions = definitions;
	pipeline->initialized = 1;
	return 0;
}

void prototype_core_pipeline_dispose(
	struct prototype_core_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized) {
		return;
	}
	prototype_term_db_dispose_runtime_state(&pipeline->terms);
	memset(pipeline, 0, sizeof(*pipeline));
}

struct prototype_term_db* prototype_core_pipeline_terms(
	struct prototype_core_pipeline* pipeline
) {
	return pipeline && pipeline->initialized ? &pipeline->terms : NULL;
}

const struct prototype_term_db* prototype_core_pipeline_terms_const(
	const struct prototype_core_pipeline* pipeline
) {
	return pipeline && pipeline->initialized ? &pipeline->terms : NULL;
}

int prototype_core_pipeline_begin_transaction(
	struct prototype_core_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || pipeline->transaction_active) {
		return -1;
	}
	pipeline->transaction_mark.term_count = pipeline->terms.term_count;
	pipeline->transaction_mark.case_count = pipeline->terms.case_count;
	pipeline->transaction_mark.case_binder_count =
		pipeline->terms.case_binder_count;
	pipeline->transaction_mark.ih_scope_count = pipeline->terms.ih_scope_count;
	pipeline->transaction_mark.computation_fold_clause_count =
		pipeline->terms.computation_fold_clause_count;
	pipeline->transaction_mark.next_binding_id = pipeline->terms.next_binding_id;
	pipeline->transaction_mark.next_universe_level_id =
		pipeline->terms.next_universe_level_id;
	memcpy(
		pipeline->transaction_mark.scope_bindings,
		pipeline->terms.scope_bindings,
		sizeof(pipeline->terms.scope_bindings)
	);
	pipeline->transaction_active = 1;
	return 0;
}

int prototype_core_pipeline_commit_transaction(
	struct prototype_core_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || !pipeline->transaction_active ||
		pipeline->transaction_revision == UINT64_MAX) {
		return -1;
	}
	pipeline->transaction_active = 0;
	++pipeline->transaction_revision;
	return 0;
}

int prototype_core_pipeline_rollback_transaction(
	struct prototype_core_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || !pipeline->transaction_active ||
		pipeline->transaction_mark.term_count > pipeline->terms.term_count ||
		pipeline->transaction_mark.case_count > pipeline->terms.case_count ||
		pipeline->transaction_mark.case_binder_count >
			pipeline->terms.case_binder_count ||
		pipeline->transaction_mark.ih_scope_count >
			pipeline->terms.ih_scope_count ||
		pipeline->transaction_mark.computation_fold_clause_count >
			pipeline->terms.computation_fold_clause_count) {
		return -1;
	}
	pipeline->terms.term_count = pipeline->transaction_mark.term_count;
	pipeline->terms.case_count = pipeline->transaction_mark.case_count;
	pipeline->terms.case_binder_count =
		pipeline->transaction_mark.case_binder_count;
	pipeline->terms.ih_scope_count = pipeline->transaction_mark.ih_scope_count;
	pipeline->terms.computation_fold_clause_count =
		pipeline->transaction_mark.computation_fold_clause_count;
	pipeline->terms.next_binding_id = pipeline->transaction_mark.next_binding_id;
	pipeline->terms.next_universe_level_id =
		pipeline->transaction_mark.next_universe_level_id;
	memcpy(
		pipeline->terms.scope_bindings,
		pipeline->transaction_mark.scope_bindings,
		sizeof(pipeline->terms.scope_bindings)
	);
	pipeline->terms.intern_index_dirty = 1;
	pipeline->terms.effect_row_variable_index_dirty = 1;
	prototype_term_notify_graph_mutation(&pipeline->terms);
	pipeline->transaction_active = 0;
	return 0;
}

int prototype_core_pipeline_read_snapshot(
	const struct prototype_core_pipeline* pipeline,
	struct prototype_core_read_snapshot* snapshot
) {
	if (!pipeline) {
		return -1;
	}
	return pipeline->initialized ?
		prototype_core_read_snapshot_begin(&pipeline->terms, snapshot) : -1;
}

int prototype_core_pipeline_normalize(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_normalization_request* request,
	struct prototype_core_normalization_response* response
) {
	struct prototype_term_normalization_result result = { 0 };
	if (!pipeline || !pipeline->initialized || !request || !response) {
		return -1;
	}
	if (prototype_term_normalize_with_profile(
			&pipeline->terms,
			pipeline->definitions,
			request->profile,
			request->term_id,
			request->step_limit,
			&result
		) != 0) {
		return -1;
	}
	response->status = result.status;
	response->term_id = result.term_id;
	response->step_limit = result.step_limit;
	response->steps_used = result.steps_used;
	response->graph_revision = result.graph_revision;
	return 0;
}

int prototype_core_pipeline_project_structural_return(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_structural_return_projection_request* request,
	struct prototype_core_structural_return_projection_response* response
) {
	if (!pipeline || !pipeline->initialized || !request || !response ||
		request->computation >= pipeline->terms.term_count ||
		request->step_limit == 0) {
		return -1;
	}
	uint32_t value = PROTOTYPE_INVALID_ID;
	int status = prototype_term_project_structural_return_value(
		&pipeline->terms, request->computation, request->step_limit, &value
	);
	response->status = status == 0 ?
		PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_COMPLETE : status == 1 ?
		PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_OPAQUE :
		PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_INVALID;
	response->value = status == 0 ? value : PROTOTYPE_INVALID_ID;
	response->graph_revision = pipeline->terms.normalization_graph_revision;
	return 0;
}

static int core_pipeline_inspect_app_spine(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_core_inspect_app_spine* spine
) {
	if (!terms || !spine || term_id >= terms->term_count) return -1;
	uint32_t reverse_arguments[PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY];
	uint32_t count = 0;
	uint32_t current = term_id;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY) return -1;
		reverse_arguments[count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count) return -1;
	spine->head = current;
	spine->argument_count = count;
	for (uint32_t i = 0; i < count; ++i) {
		spine->arguments[i] = reverse_arguments[count - i - 1];
	}
	return 0;
}

int prototype_core_pipeline_inspect(
	const struct prototype_core_pipeline* pipeline,
	const struct prototype_core_inspect_request* request,
	struct prototype_core_inspect_response* response
) {
	if (!pipeline || !pipeline->initialized || !request || !response) return -1;
	const struct prototype_term_db* terms = &pipeline->terms;
	memset(response, 0, sizeof(*response));
	response->kind = request->kind;
	response->graph_revision = terms->normalization_graph_revision;
	switch (request->kind) {
		case PROTOTYPE_CORE_INSPECT_COUNTS:
			response->as.counts = (struct prototype_core_inspect_counts) {
				.term_count = terms->term_count,
				.case_count = terms->case_count,
				.case_binder_count = terms->case_binder_count,
				.ih_scope_count = terms->ih_scope_count,
				.computation_fold_clause_count =
					terms->computation_fold_clause_count
			};
			return 0;
		case PROTOTYPE_CORE_INSPECT_TERM:
			if (request->as.term.term_id >= terms->term_count) return -1;
			response->as.term = terms->terms[request->as.term.term_id];
			return 0;
		case PROTOTYPE_CORE_INSPECT_CHILD_COUNT:
			return prototype_term_child_count(
				terms, request->as.child_count.term_id,
				&response->as.child_count
			);
		case PROTOTYPE_CORE_INSPECT_CHILD:
			return prototype_term_child_at(
				terms, request->as.child.term_id,
				request->as.child.child_index, &response->as.child
			);
		case PROTOTYPE_CORE_INSPECT_CONSTRUCTOR_SPINE:
			return prototype_term_constructor_spine_info(
				terms, request->as.constructor_spine.term_id,
				&response->as.constructor_spine.head,
				&response->as.constructor_spine.owner,
				&response->as.constructor_spine.constructor_id,
				response->as.constructor_spine.arguments,
				PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY,
				&response->as.constructor_spine.argument_count
			);
		case PROTOTYPE_CORE_INSPECT_MATCH_CASE:
			if (request->as.match_case.case_id >= terms->case_count) return -1;
			response->as.match_case.label_symbol_id =
				terms->case_label_symbols[request->as.match_case.case_id];
			response->as.match_case.match_case =
				terms->cases[request->as.match_case.case_id];
			return 0;
		case PROTOTYPE_CORE_INSPECT_CASE_BINDER:
			if (request->as.case_binder.binder_id >= terms->case_binder_count) {
				return -1;
			}
			response->as.case_binder =
				terms->case_binders[request->as.case_binder.binder_id];
			return 0;
		case PROTOTYPE_CORE_INSPECT_IH_SCOPE:
			if (request->as.ih_scope.scope_id >= terms->ih_scope_count) return -1;
			response->as.ih_scope = terms->ih_scopes[request->as.ih_scope.scope_id];
			return 0;
		case PROTOTYPE_CORE_INSPECT_COMPUTATION_FOLD_CLAUSE:
			if (request->as.computation_fold_clause.clause_id >=
					terms->computation_fold_clause_count) {
				return -1;
			}
			response->as.computation_fold_clause =
				terms->computation_fold_clauses[
					request->as.computation_fold_clause.clause_id
				];
			return 0;
		case PROTOTYPE_CORE_INSPECT_PURE_FAMILY_PARTS:
			return prototype_term_pure_family_parts(
				terms,
				request->as.pure_family_parts.term_id,
				&response->as.pure_family_parts.binding_id,
				&response->as.pure_family_parts.body
			);
		case PROTOTYPE_CORE_INSPECT_APP_SPINE:
			return core_pipeline_inspect_app_spine(
				terms, request->as.app_spine.term_id,
				&response->as.app_spine
			);
		case PROTOTYPE_CORE_INSPECT_FREE_BINDING_OCCURS:
			if (request->as.free_binding_occurs.term_id >= terms->term_count) {
				return -1;
			}
			response->as.free_binding_occurs = prototype_term_contains_free_binding(
				terms, request->as.free_binding_occurs.term_id,
				request->as.free_binding_occurs.binding_id
			);
			return 0;
		default:
			return -1;
	}
}

int prototype_core_pipeline_form(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* response
) {
	int status = -1;
	if (!pipeline || !pipeline->initialized || !request || !response) {
		return -1;
	}
	if (prototype_core_formation_request_payload_validate(
			request, payload, payload_size
		) != 0) {
		return -1;
	}
	memset(response, 0, sizeof(*response));
	response->term_id = PROTOTYPE_INVALID_ID;
	response->identity_id = PROTOTYPE_INVALID_ID;
	switch (request->kind) {
	case PROTOTYPE_CORE_FORM_NEW_BINDING:
		response->identity_id = prototype_term_new_binding(&pipeline->terms);
		status = response->identity_id == PROTOTYPE_INVALID_ID ? -1 : 0;
		break;
	case PROTOTYPE_CORE_FORM_NEW_UNIVERSE_LEVEL:
		response->identity_id = prototype_term_new_universe_level(
			&pipeline->terms
		);
		status = response->identity_id == PROTOTYPE_INVALID_ID ? -1 : 0;
		break;
	case PROTOTYPE_CORE_FORM_NEW_IH_SCOPE:
		response->identity_id = prototype_term_new_ih_scope(&pipeline->terms);
		status = response->identity_id == PROTOTYPE_INVALID_ID ? -1 : 0;
		break;
	case PROTOTYPE_CORE_FORM_SET_IH_SCOPE_TERM:
		status = prototype_term_set_ih_scope_term(
			&pipeline->terms,
			request->as.set_ih_scope.ih_scope_id,
			request->as.set_ih_scope.match_term
		);
		response->identity_id = request->as.set_ih_scope.ih_scope_id;
		response->term_id = request->as.set_ih_scope.match_term;
		break;
	case PROTOTYPE_CORE_FORM_VAR:
		status = prototype_term_var(
			&pipeline->terms, request->as.var.binding_id, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_CONSTRUCTOR:
		status = prototype_term_constructor(
			&pipeline->terms,
			request->as.constructor.owner,
			request->as.constructor.constructor_id,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_APP:
		status = prototype_term_app(
			&pipeline->terms,
			request->as.app.function,
			request->as.app.argument,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_LAMBDA:
		status = prototype_term_lambda(
			&pipeline->terms,
			request->as.lambda.binding_id,
			request->as.lambda.body,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_MATCH:
	{
		uint32_t case_count = request->as.match.cases.element_count;
		const struct prototype_core_match_case_request* packet_cases = NULL;
		struct prototype_match_case_input* cases = NULL;
		if (case_count != 0) {
			packet_cases = core_request_payload_range(
				payload,
				request->as.match.cases,
				sizeof(struct prototype_core_match_case_request)
			);
			cases = calloc(case_count, sizeof(*cases));
			if (!packet_cases || !cases) {
				free(cases);
				return -1;
			}
			for (uint32_t i = 0; i < case_count; ++i) {
				const struct prototype_core_case_binder_request* packet_binders =
					NULL;
				struct prototype_case_binder* binders = NULL;
				if (packet_cases[i].binders.element_count != 0) {
					packet_binders = core_request_payload_range(
						payload,
						packet_cases[i].binders,
						sizeof(struct prototype_core_case_binder_request)
					);
					binders = calloc(
						packet_cases[i].binders.element_count, sizeof(*binders)
					);
					if (!packet_binders || !binders) {
						free(binders);
						for (uint32_t j = 0; j < i; ++j) {
							free((void*)cases[j].binders);
						}
						free(cases);
						return -1;
					}
					for (uint32_t j = 0;
						j < packet_cases[i].binders.element_count;
						++j) {
						binders[j] = (struct prototype_case_binder) {
							.binding_id = packet_binders[j].binding_id,
							.is_recursive = packet_binders[j].is_recursive
						};
					}
				}
				cases[i] = (struct prototype_match_case_input) {
					.case_label_symbol_id =
						packet_cases[i].case_label_symbol_id,
					.constructor_owner = packet_cases[i].constructor_owner,
					.constructor_id = packet_cases[i].constructor_id,
					.binders = binders,
					.binder_count = packet_cases[i].binders.element_count,
					.body = packet_cases[i].body
				};
			}
		}
		if (request->as.match.ih_scope_id == PROTOTYPE_INVALID_ID) {
			status = prototype_term_match(
				&pipeline->terms,
				request->as.match.scrutinee,
				cases,
				case_count,
				&response->term_id
			);
		} else {
			status = prototype_term_match_with_ih_scope(
				&pipeline->terms,
				request->as.match.scrutinee,
				cases,
				case_count,
				request->as.match.ih_scope_id,
				&response->term_id
			);
		}
		for (uint32_t i = 0; i < case_count; ++i) {
			free((void*)cases[i].binders);
		}
		free(cases);
		break;
	}
	case PROTOTYPE_CORE_FORM_TYPE_FORMER:
		status = prototype_term_type_former(
			&pipeline->terms,
			request->as.type_former.representation_id,
			request->as.type_former.constructor_count,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_TYPE_DECLARATION:
		status = prototype_term_type_declaration(
			&pipeline->terms,
			request->as.type_declaration.identity,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_TYPE_VIEW:
		status = prototype_term_type_view(
			&pipeline->terms,
			request->as.type_view.identity,
			request->as.type_view.core,
			request->as.type_view.source,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_INDUCTION_HYPOTHESIS:
		status = prototype_term_induction_hypothesis(
			&pipeline->terms,
			request->as.induction_hypothesis.ih_scope_id,
			request->as.induction_hypothesis.argument,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_UNIVERSE_VAR:
		status = prototype_term_universe_var(
			&pipeline->terms,
			request->as.universe_var.level_var,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PRIMITIVE_TEXT:
		status = prototype_term_primitive_text(&pipeline->terms, &response->term_id);
		break;
	case PROTOTYPE_CORE_FORM_TEXT_LITERAL:
		status = prototype_term_text_literal(
			&pipeline->terms,
			request->as.text_literal.text_symbol_id,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PRIMITIVE_INT:
		status = prototype_term_primitive_int(&pipeline->terms, &response->term_id);
		break;
	case PROTOTYPE_CORE_FORM_INT_LITERAL:
		status = prototype_term_int_literal(
			&pipeline->terms,
			request->as.int_literal.value,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_EMPTY:
		status = prototype_term_effect_row_empty(
			&pipeline->terms, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_VAR:
		status = prototype_term_effect_row_var(
			&pipeline->terms,
			request->as.effect_row_var.binding_id,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_UNION:
		status = prototype_term_effect_row_union(
			&pipeline->terms,
			request->as.effect_row_union.left,
			request->as.effect_row_union.right,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_RESIDUAL:
		status = prototype_term_effect_row_residual(
			&pipeline->terms,
			request->as.effect_row_residual.source,
			request->as.effect_row_residual.handled,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_FORALL:
		status = prototype_term_effect_row_forall(
			&pipeline->terms,
			request->as.effect_row_forall.binding_id,
			request->as.effect_row_forall.body,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_ROW_OPERATION:
		status = prototype_term_effect_row_operation(
			&pipeline->terms,
			request->as.effect_row_operation.operation_id,
			request->as.effect_row_operation.latent_row,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_COMPUTATION_TYPE:
		status = prototype_term_computation_type(
			&pipeline->terms,
			request->as.computation_type.label,
			request->as.computation_type.result,
			request->as.computation_type.totality,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_THUNK_TYPE:
		status = prototype_term_thunk_type(
			&pipeline->terms,
			request->as.thunk_type.computation,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_RETURN:
		status = prototype_term_return(
			&pipeline->terms, request->as.return_term.value, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_THUNK:
		status = prototype_term_thunk(
			&pipeline->terms, request->as.thunk.computation, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_FORCE:
		status = prototype_term_force(
			&pipeline->terms, request->as.force.value, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_OPERATION_REQUEST:
		status = prototype_term_operation_request(
			&pipeline->terms,
			request->as.operation_request.operation,
			request->as.operation_request.argument,
			request->as.operation_request.continuation,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_COMPUTATION_FOLD:
	{
		uint32_t clause_count =
			request->as.computation_fold.clauses.element_count;
		const struct prototype_core_computation_fold_clause_request*
			packet_clauses = NULL;
		struct prototype_computation_fold_clause* clauses = NULL;
		if (clause_count != 0) {
			packet_clauses = core_request_payload_range(
				payload,
				request->as.computation_fold.clauses,
				sizeof(struct prototype_core_computation_fold_clause_request)
			);
			clauses = calloc(clause_count, sizeof(*clauses));
			if (!packet_clauses || !clauses) {
				free(clauses);
				return -1;
			}
			for (uint32_t i = 0; i < clause_count; ++i) {
				clauses[i] = (struct prototype_computation_fold_clause) {
					.operation = packet_clauses[i].operation,
					.body = packet_clauses[i].body
				};
			}
		}
		status = prototype_term_computation_fold(
			&pipeline->terms,
			request->as.computation_fold.computation,
			request->as.computation_fold.return_clause,
			clauses,
			clause_count,
			&response->term_id
		);
		free(clauses);
		break;
	}
	case PROTOTYPE_CORE_FORM_HOST_TYPE:
		status = prototype_term_make_host_type(
			&pipeline->terms, request->as.host_type.type_id, &response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EXTERNAL_REF:
		status = prototype_term_external_ref(
			&pipeline->terms,
			request->as.external_ref.name,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PURE_PRIMITIVE:
		status = prototype_term_pure_primitive(
			&pipeline->terms,
			request->as.pure_primitive.primitive_id,
			request->as.pure_primitive.type_symbol_id,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_EFFECT_OPERATION:
		status = prototype_term_effect_operation(
			&pipeline->terms,
			request->as.effect_operation.operation_id,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PI:
		status = prototype_term_pi(
			&pipeline->terms,
			request->as.pi.domain,
			request->as.pi.codomain,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PI_FAMILY:
		status = prototype_term_pi_family(
			&pipeline->terms,
			request->as.pi_family.domain,
			request->as.pi_family.codomain_family,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_PURE_FAMILY:
		status = prototype_term_pure_family(
			&pipeline->terms,
			request->as.pure_family.binding_id,
			request->as.pure_family.body,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_TERMINATES_TYPE:
		status = prototype_term_terminates_type(
			&pipeline->terms,
			request->as.terminates.computation,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_FORM_TERMINATES_WITNESS:
		status = prototype_term_terminates_witness(
			&pipeline->terms,
			request->as.terminates.computation,
			&response->term_id
		);
		break;
	default:
		return -1;
	}
	if (status != 0) {
		return -1;
	}
	response->graph_revision = pipeline->terms.normalization_graph_revision;
	return 0;
}

int prototype_core_pipeline_rewrite(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_rewrite_response* response
) {
	int status = -1;
	if (!pipeline || !pipeline->initialized || !request || !response) {
		return -1;
	}
	if (prototype_core_rewrite_request_payload_validate(
			request, payload, payload_size
		) != 0) {
		return -1;
	}
	memset(response, 0, sizeof(*response));
	response->term_id = PROTOTYPE_INVALID_ID;
	switch (request->kind) {
	case PROTOTYPE_CORE_REWRITE_SUBSTITUTE_BOUND_VAR:
		status = prototype_term_graph_substitute_bound_var(
			&pipeline->terms,
			request->term_id,
			request->as.substitute_bound_var.binding_id,
			request->as.substitute_bound_var.replacement,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_REWRITE_REPLACE_EXACT:
		status = prototype_term_graph_replace_exact(
			&pipeline->terms,
			request->term_id,
			request->as.replace_exact.exact_term,
			request->as.replace_exact.replacement,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_REWRITE_REPLACE_EXACT_OUTSIDE_TYPE_VIEWS:
		status = prototype_term_graph_replace_exact_outside_type_views(
			&pipeline->terms,
			request->term_id,
			request->as.replace_exact.exact_term,
			request->as.replace_exact.replacement,
			&response->term_id
		);
		break;
	case PROTOTYPE_CORE_REWRITE_BINDINGS:
	{
		uint32_t binding_count = request->as.bindings.element_count;
		const struct prototype_core_binding_replacement_request* packet_bindings =
			NULL;
		struct prototype_binding_replacement* bindings = NULL;
		if (binding_count != 0) {
			packet_bindings = core_request_payload_range(
				payload,
				request->as.bindings,
				sizeof(struct prototype_core_binding_replacement_request)
			);
			bindings = calloc(binding_count, sizeof(*bindings));
			if (!packet_bindings || !bindings) {
				free(bindings);
				return -1;
			}
			for (uint32_t i = 0; i < binding_count; ++i) {
				bindings[i] = (struct prototype_binding_replacement) {
					.binding_id = packet_bindings[i].binding_id,
					.replacement = packet_bindings[i].replacement
				};
			}
		}
		status = prototype_term_graph_reindex_bindings(
			&pipeline->terms,
			request->term_id,
			bindings,
			binding_count,
			&response->term_id
		);
		free(bindings);
		break;
	}
	case PROTOTYPE_CORE_REWRITE_BINDING_SEQUENCE:
	{
		uint32_t binding_count =
			request->as.binding_sequence.element_count;
		const struct prototype_core_binding_replacement_request* bindings =
			binding_count == 0 ? NULL : core_request_payload_range(
				payload,
				request->as.binding_sequence,
				sizeof(struct prototype_core_binding_replacement_request)
			);
		if (binding_count != 0 && !bindings) return -1;
		uint32_t rewritten = request->term_id;
		status = 0;
		for (uint32_t i = 0; i < binding_count; ++i) {
			uint32_t next;
			if (prototype_term_graph_substitute_bound_var(
					&pipeline->terms, rewritten, bindings[i].binding_id,
					bindings[i].replacement, &next
				) != 0) {
				status = -1;
				break;
			}
			rewritten = next;
		}
		response->term_id = rewritten;
		break;
	}
	default:
		return -1;
	}
	if (status != 0) {
		return -1;
	}
	response->graph_revision = pipeline->terms.normalization_graph_revision;
	return 0;
}
