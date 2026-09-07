#include "a_program/core/pipeline.h"
#include "a_program/core/request_provider.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TERM_CAPACITY 64
#define CASE_CAPACITY 8
#define CASE_BINDER_CAPACITY 8
#define IH_SCOPE_CAPACITY 2
#define PAYLOAD_CAPACITY 256

static int initialize_core(
	struct prototype_core_pipeline* core,
	struct prototype_core_request_service* requests,
	struct prototype_term* terms,
	struct prototype_match_case* cases,
	int* labels,
	struct prototype_case_binder* binders,
	struct prototype_ih_scope* scopes
) {
	const struct prototype_core_pipeline_storage storage = {
		.terms = terms,
		.term_capacity = TERM_CAPACITY,
		.cases = cases,
		.case_label_symbols = labels,
		.case_capacity = CASE_CAPACITY,
		.case_binders = binders,
		.case_binder_capacity = CASE_BINDER_CAPACITY,
		.ih_scopes = scopes,
		.ih_scope_capacity = IH_SCOPE_CAPACITY
	};
	if (prototype_core_pipeline_init(core, &storage, NULL) != 0) return -1;
	prototype_core_request_service_init(requests, core);
	return 0;
}

int main(void) {
	struct prototype_term term_storage[TERM_CAPACITY];
	struct prototype_match_case case_storage[CASE_CAPACITY];
	int label_storage[CASE_CAPACITY];
	struct prototype_case_binder binder_storage[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope scope_storage[IH_SCOPE_CAPACITY];
	struct prototype_core_pipeline core;
	struct prototype_core_request_service requests = { 0 };
	if (initialize_core(
			&core, &requests, term_storage, case_storage, label_storage,
			binder_storage, scope_storage
		) != 0) {
		return 1;
	}
	struct prototype_term_db* terms = prototype_core_pipeline_terms(&core);
	uint32_t owner;
	uint32_t scrutinee;
	uint32_t binding = prototype_term_new_binding(terms);
	uint32_t body;
	if (binding == PROTOTYPE_INVALID_ID ||
		prototype_term_primitive_int(terms, &owner) != 0 ||
		prototype_term_constructor(terms, owner, 0, &scrutinee) != 0 ||
		prototype_term_var(terms, binding, &body) != 0) {
		return 1;
	}

	size_t payload_size =
		prototype_core_request_payload_storage_size(PAYLOAD_CAPACITY);
	unsigned char payload_storage[
		prototype_core_request_payload_storage_size(PAYLOAD_CAPACITY)
	];
	struct prototype_core_request_payload* payload;
	struct prototype_core_case_binder_request packet_binder = {
		.binding_id = binding,
		.is_recursive = 0
	};
	struct prototype_core_match_case_request packet_case = {
		.case_label_symbol_id = 17,
		.constructor_owner = owner,
		.constructor_id = 0,
		.body = body
	};
	struct prototype_core_request_range case_range;
	if (prototype_core_request_payload_init(
			payload_storage, payload_size, &payload
		) != 0 || prototype_core_request_payload_append(
			payload, payload_size, &packet_binder, sizeof(packet_binder), 1,
			&packet_case.binders
		) != 0 || prototype_core_request_payload_append(
			payload, payload_size, &packet_case, sizeof(packet_case), 1,
			&case_range
		) != 0 || prototype_core_request_payload_seal(
			payload, payload_size
		) != 0) {
		return 1;
	}
	struct prototype_core_formation_request match_request = {
		.kind = PROTOTYPE_CORE_FORM_MATCH,
		.payload_digest = payload->digest,
		.as.match = {
			.scrutinee = scrutinee,
			.cases = case_range,
			.ih_scope_id = PROTOTYPE_INVALID_ID
		}
	};
	struct prototype_core_formation_response match_response;
	if (prototype_core_request_form(
			&requests, &match_request, payload, payload_size, &match_response
		) != 0 || match_response.term_id == PROTOTYPE_INVALID_ID ||
		terms->terms[match_response.term_id].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match_response.term_id].as.match.case_count != 1) {
		fprintf(stderr, "pointer-free Match packet failed\n");
		return 1;
	}

	unsigned char copied_storage[
		prototype_core_request_payload_storage_size(PAYLOAD_CAPACITY)
	];
	memcpy(copied_storage, payload_storage, payload_size);
	const struct prototype_core_request_payload* copied =
		(const struct prototype_core_request_payload*)copied_storage;
	struct prototype_core_formation_response copied_response;
	if (prototype_core_request_form(
			&requests, &match_request, copied, payload_size, &copied_response
		) != 0 || copied_response.term_id != match_response.term_id) {
		fprintf(stderr, "copied Core request packet was not self-contained\n");
		return 1;
	}
	struct prototype_core_formation_request invalid_range = match_request;
	invalid_range.as.match.cases.byte_offset = UINT32_MAX;
	if (prototype_core_request_form(
			&requests, &invalid_range, copied, payload_size, &copied_response
		) == 0) {
		fprintf(stderr, "out-of-range Core request packet was accepted\n");
		return 1;
	}
	unsigned char tampered_storage[
		prototype_core_request_payload_storage_size(PAYLOAD_CAPACITY)
	];
	memcpy(tampered_storage, payload_storage, payload_size);
	struct prototype_core_request_payload* tampered =
		(struct prototype_core_request_payload*)tampered_storage;
	tampered->bytes[case_range.byte_offset] ^= 1;
	if (prototype_core_request_form(
			&requests, &match_request, tampered, payload_size, &copied_response
		) == 0) {
		fprintf(stderr, "tampered Core request packet was accepted\n");
		return 1;
	}

	struct prototype_core_binding_replacement_request packet_replacement = {
		.binding_id = binding,
		.replacement = scrutinee
	};
	struct prototype_core_request_range replacement_range;
	if (prototype_core_request_payload_init(
			payload_storage, payload_size, &payload
		) != 0 || prototype_core_request_payload_append(
			payload, payload_size, &packet_replacement,
			sizeof(packet_replacement), 1, &replacement_range
		) != 0 || prototype_core_request_payload_seal(
			payload, payload_size
		) != 0) {
		return 1;
	}
	struct prototype_core_rewrite_request rewrite_request = {
		.kind = PROTOTYPE_CORE_REWRITE_BINDINGS,
		.term_id = body,
		.payload_digest = payload->digest,
		.as.bindings = replacement_range
	};
	struct prototype_core_rewrite_response rewrite_response;
	if (prototype_core_request_rewrite(
			&requests, &rewrite_request, payload, payload_size, &rewrite_response
		) != 0 || rewrite_response.term_id != scrutinee) {
		fprintf(stderr, "pointer-free rewrite packet failed\n");
		return 1;
	}

	uint32_t second_binding = prototype_term_new_binding(terms);
	uint32_t first_variable;
	uint32_t second_variable;
	if (second_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, binding, &first_variable
		) != 0 || prototype_term_var(
			terms, second_binding, &second_variable
		) != 0) return 1;
	struct prototype_core_binding_replacement_request packet_sequence[] = {
		{
			.binding_id = second_binding,
			.replacement = first_variable
		},
		{
			.binding_id = binding,
			.replacement = scrutinee
		}
	};
	struct prototype_core_request_range sequence_range;
	if (prototype_core_request_payload_init(
			payload_storage, payload_size, &payload
		) != 0 || prototype_core_request_payload_append(
			payload, payload_size, packet_sequence, sizeof(*packet_sequence),
			2, &sequence_range
		) != 0 || prototype_core_request_payload_seal(
			payload, payload_size
		) != 0) return 1;
	struct prototype_core_rewrite_request sequence_request = {
		.kind = PROTOTYPE_CORE_REWRITE_BINDING_SEQUENCE,
		.term_id = second_variable,
		.payload_digest = payload->digest,
		.as.binding_sequence = sequence_range
	};
	if (prototype_core_request_rewrite(
			&requests, &sequence_request, payload, payload_size,
			&rewrite_response
		) != 0 || rewrite_response.term_id != scrutinee) {
		fprintf(stderr, "ordered Core binding sequence failed\n");
		return 1;
	}
	struct prototype_core_rewrite_request invalid_sequence = sequence_request;
	invalid_sequence.as.binding_sequence.byte_offset = UINT32_MAX;
	if (prototype_core_request_rewrite(
			&requests, &invalid_sequence, payload, payload_size,
			&rewrite_response
		) == 0) {
		fprintf(stderr, "out-of-range binding sequence was accepted\n");
		return 1;
	}

	uint32_t returned;
	uint32_t return_binding = prototype_term_new_binding(terms);
	uint32_t return_variable;
	uint32_t return_clause;
	uint32_t operation;
	if (return_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_return(terms, scrutinee, &returned) != 0 ||
		prototype_term_var(terms, return_binding, &return_variable) != 0 ||
		prototype_term_lambda(
			terms, return_binding, return_variable, &return_clause
		) != 0 || prototype_term_effect_operation(terms, 3, &operation) != 0) {
		return 1;
	}
	struct prototype_core_computation_fold_clause_request packet_clause = {
		.operation = operation,
		.body = return_clause
	};
	struct prototype_core_request_range clause_range;
	if (prototype_core_request_payload_init(
			payload_storage, payload_size, &payload
		) != 0 || prototype_core_request_payload_append(
			payload, payload_size, &packet_clause, sizeof(packet_clause), 1,
			&clause_range
		) != 0 || prototype_core_request_payload_seal(
			payload, payload_size
		) != 0) {
		return 1;
	}
	struct prototype_core_formation_request fold_request = {
		.kind = PROTOTYPE_CORE_FORM_COMPUTATION_FOLD,
		.payload_digest = payload->digest,
		.as.computation_fold = {
			.computation = returned,
			.return_clause = return_clause,
			.clauses = clause_range
		}
	};
	struct prototype_core_formation_response fold_response;
	if (prototype_core_request_form(
			&requests, &fold_request, payload, payload_size, &fold_response
		) != 0 || fold_response.term_id == PROTOTYPE_INVALID_ID ||
		terms->terms[fold_response.term_id].tag !=
			PROTOTYPE_TERM_COMPUTATION_FOLD ||
		terms->terms[fold_response.term_id].as.computation_fold.clause_count != 1) {
		fprintf(stderr, "pointer-free computation-fold packet failed\n");
		return 1;
	}

	uint32_t empty_row;
	uint32_t first_operation_row;
	uint32_t second_operation_row;
	uint32_t source_row;
	if (prototype_term_effect_row_empty(terms, &empty_row) != 0 ||
		prototype_term_effect_row_operation(
			terms, 3, empty_row, &first_operation_row
		) != 0 || prototype_term_effect_row_operation(
			terms, 4, empty_row, &second_operation_row
		) != 0 || prototype_term_effect_row_union(
			terms, first_operation_row, second_operation_row, &source_row
		) != 0) return 1;
	struct prototype_core_formation_request residual_request = {
		.kind = PROTOTYPE_CORE_FORM_EFFECT_ROW_RESIDUAL,
		.as.effect_row_residual = {
			.source = source_row,
			.handled = first_operation_row
		}
	};
	struct prototype_core_formation_response residual_response;
	if (prototype_core_request_form(
			&requests, &residual_request, NULL, 0, &residual_response
		) != 0 || residual_response.term_id != second_operation_row) {
		fprintf(stderr, "effect-row residual calculation packet failed\n");
		return 1;
	}

	prototype_core_pipeline_dispose(&core);
	puts("core request packet check passed");
	return 0;
}
