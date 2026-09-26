#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_BINDING_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_BINDING_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/kernel/context.h"

#define PROTOTYPE_TYPING_BINDER_OWNER_CAPACITY 16384
#define PROTOTYPE_TYPING_BINDING_INDEX_CAPACITY 32768

struct prototype_typing_binder_owner {
	uint32_t binding_id;
	uint32_t binding_context;
	uint32_t lambda_occurrence;
	uint32_t bound_computation_occurrence;
	uint32_t sequence_fold_occurrence;
	/* Exact Match telescope edge introducing this binding, when applicable. */
	uint32_t match_occurrence;
	uint32_t match_case_index;
	uint32_t match_field_index;
	uint32_t projected_pure_value;
	uint32_t first_classifier_evidence;
};

struct prototype_typing_binder_classifier_evidence {
	uint32_t occurrence_id;
	uint32_t next;
};

struct prototype_typing_binding_index_entry {
	uint32_t binding_id;
	uint32_t binding_context;
	uint32_t owner_index;
	int occupied;
};

struct prototype_typing_binding_state {
	struct prototype_typing_binder_owner owners[
		PROTOTYPE_TYPING_BINDER_OWNER_CAPACITY
	];
	uint32_t owner_count;
	struct prototype_typing_binder_classifier_evidence classifier_evidence[
		PROTOTYPE_TYPING_BINDER_OWNER_CAPACITY
	];
	uint32_t classifier_evidence_count;
	struct prototype_typing_binding_index_entry index[
		PROTOTYPE_TYPING_BINDING_INDEX_CAPACITY
	];
	size_t indexed_occurrence_count;
	uint32_t pending_assumption_count;
	int index_valid;
};

int prototype_typing_context_classifier_view_init(
	const struct prototype_context_db* contexts,
	struct prototype_context_classifier_view* view
);

void prototype_typing_binding_state_init(
	struct prototype_typing_binding_state* state
);

#endif
