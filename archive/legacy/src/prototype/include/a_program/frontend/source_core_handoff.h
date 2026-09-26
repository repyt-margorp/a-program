#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_CORE_HANDOFF_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_CORE_HANDOFF_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/source_lowering_plan.h"

enum prototype_source_core_handoff_state {
	PROTOTYPE_SOURCE_CORE_UNFORMED = 0,
	PROTOTYPE_SOURCE_CORE_FORMING = 1,
	PROTOTYPE_SOURCE_CORE_FORMED = 2
};

enum prototype_source_core_slot_result_kind {
	PROTOTYPE_SOURCE_CORE_SLOT_INVALID = 0,
	PROTOTYPE_SOURCE_CORE_SLOT_TERM = 1,
	/* The slot is source coordination only and denotes no Core Term. */
	PROTOTYPE_SOURCE_CORE_SLOT_SOURCE_ONLY = 2,
	/* T1 must select a Core-only recipe that C1 forms later. */
	PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1 = 3
};

/* Driver-owned handoff entry containing one sealed Layer C result. */
struct prototype_source_core_slot_result {
	uint32_t term_id;
	uint32_t ih_scope_id;
	uint64_t graph_revision;
	int result_kind;
	int state;
};

/* Driver-owned handoff entry containing one Layer C binder identity. */
struct prototype_source_core_binder_result {
	uint32_t binding_id;
	uint64_t graph_revision;
	int state;
};

struct prototype_source_core_handoff_builder {
	struct prototype_source_core_slot_result* slots;
	size_t slot_count;
	struct prototype_source_core_binder_result* binders;
	size_t binder_count;
	uint64_t source_fingerprint;
	uint64_t sealed_graph_revision;
	int sealed;
};

/* Immutable C0-to-T0 handoff. It owns no calculation or typing judgement. */
struct prototype_source_core_handoff_view {
	const struct prototype_source_core_slot_result* slots;
	size_t slot_count;
	const struct prototype_source_core_binder_result* binders;
	size_t binder_count;
	uint64_t source_fingerprint;
	uint64_t graph_revision;
};

int prototype_source_core_handoff_builder_init(
	struct prototype_source_core_handoff_builder* results,
	struct prototype_source_core_slot_result* slots,
	size_t slot_count,
	struct prototype_source_core_binder_result* binders,
	size_t binder_count,
	uint64_t source_fingerprint
);

int prototype_source_core_slot_begin(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
);

int prototype_source_core_slot_commit(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t term_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
);

int prototype_source_core_slot_record_formed(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t term_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
);

int prototype_source_core_slot_record_nonterm(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	int result_kind,
	uint64_t graph_revision
);

/* Records an operational Layer C scope before the owning Match Term exists. */
int prototype_source_core_slot_record_ih_scope(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
);

int prototype_source_core_slot_reset(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
);

const struct prototype_source_core_slot_result*
prototype_source_core_slot_result_get(
	const struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
);

int prototype_source_core_binder_record_formed(
	struct prototype_source_core_handoff_builder* results,
	uint32_t source_binder_slot,
	uint32_t binding_id,
	uint64_t graph_revision
);

const struct prototype_source_core_binder_result*
prototype_source_core_binder_result_get(
	const struct prototype_source_core_handoff_builder* results,
	uint32_t source_binder_slot
);

int prototype_source_core_handoff_binding_for_source(
	const struct prototype_source_core_handoff_builder* results,
	const struct prototype_source_lowering_plan* plan,
	uint32_t source_binder_id,
	uint32_t* p_binding_id
);

int prototype_source_core_handoff_builder_seal(
	struct prototype_source_core_handoff_builder* results,
	uint64_t graph_revision,
	struct prototype_source_core_handoff_view* p_view
);

/* Final C0 barrier. Unlike the transitional sparse seal, every planned Core
 * slot and binder must be formed before this operation succeeds. */
int prototype_source_core_handoff_builder_seal_complete(
	struct prototype_source_core_handoff_builder* results,
	uint64_t graph_revision,
	struct prototype_source_core_handoff_view* p_view
);

const struct prototype_source_core_slot_result*
prototype_source_core_handoff_view_slot_get(
	const struct prototype_source_core_handoff_view* view,
	uint32_t slot_id
);

const struct prototype_source_core_binder_result*
prototype_source_core_handoff_view_binder_get(
	const struct prototype_source_core_handoff_view* view,
	uint32_t source_binder_slot
);

#endif
