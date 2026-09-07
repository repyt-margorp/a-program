#include "a_program/frontend/source_core_handoff.h"

#include <string.h>

#include "a_program/support/schema.h"

static void prototype_source_core_slot_result_clear(
	struct prototype_source_core_slot_result* result
) {
	if (!result) {
		return;
	}
	memset(result, 0, sizeof(*result));
	result->term_id = PROTOTYPE_INVALID_ID;
	result->ih_scope_id = PROTOTYPE_INVALID_ID;
	result->result_kind = PROTOTYPE_SOURCE_CORE_SLOT_INVALID;
}

static void prototype_source_core_binder_result_clear(
	struct prototype_source_core_binder_result* result
) {
	if (!result) return;
	memset(result, 0, sizeof(*result));
	result->binding_id = PROTOTYPE_INVALID_ID;
}

int prototype_source_core_handoff_builder_init(
	struct prototype_source_core_handoff_builder* results,
	struct prototype_source_core_slot_result* slots,
	size_t slot_count,
	struct prototype_source_core_binder_result* binders,
	size_t binder_count,
	uint64_t source_fingerprint
) {
	if (!results || (slot_count > 0 && !slots) ||
		(binder_count > 0 && !binders)) {
		return -1;
	}
	results->slots = slots;
	results->slot_count = slot_count;
	results->binders = binders;
	results->binder_count = binder_count;
	results->source_fingerprint = source_fingerprint;
	results->sealed_graph_revision = 0;
	results->sealed = 0;
	for (size_t i = 0; i < slot_count; ++i) {
		prototype_source_core_slot_result_clear(&slots[i]);
	}
	for (size_t i = 0; i < binder_count; ++i) {
		prototype_source_core_binder_result_clear(&binders[i]);
	}
	return 0;
}

static struct prototype_source_core_slot_result*
prototype_source_core_slot_result_open_mut(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
) {
	return results && !results->sealed &&
		slot_id < results->slot_count ? &results->slots[slot_id] : NULL;
}

int prototype_source_core_slot_begin(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result || result->state != PROTOTYPE_SOURCE_CORE_UNFORMED) {
		return -1;
	}
	result->state = PROTOTYPE_SOURCE_CORE_FORMING;
	return 0;
}

int prototype_source_core_slot_commit(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t term_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result || result->state != PROTOTYPE_SOURCE_CORE_FORMING ||
		term_id == PROTOTYPE_INVALID_ID ||
		(result->ih_scope_id != PROTOTYPE_INVALID_ID &&
		 result->ih_scope_id != ih_scope_id)) {
		return -1;
	}
	result->term_id = term_id;
	result->ih_scope_id = ih_scope_id;
	result->graph_revision = graph_revision;
	result->result_kind = PROTOTYPE_SOURCE_CORE_SLOT_TERM;
	result->state = PROTOTYPE_SOURCE_CORE_FORMED;
	return 0;
}

int prototype_source_core_slot_record_formed(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t term_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result || result->state != PROTOTYPE_SOURCE_CORE_UNFORMED ||
		term_id == PROTOTYPE_INVALID_ID ||
		(result->ih_scope_id != PROTOTYPE_INVALID_ID &&
		 result->ih_scope_id != ih_scope_id)) {
		return -1;
	}
	result->term_id = term_id;
	result->ih_scope_id = ih_scope_id;
	result->graph_revision = graph_revision;
	result->result_kind = PROTOTYPE_SOURCE_CORE_SLOT_TERM;
	result->state = PROTOTYPE_SOURCE_CORE_FORMED;
	return 0;
}

int prototype_source_core_slot_record_nonterm(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	int result_kind,
	uint64_t graph_revision
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result || result->state != PROTOTYPE_SOURCE_CORE_UNFORMED ||
		(result_kind != PROTOTYPE_SOURCE_CORE_SLOT_SOURCE_ONLY &&
		 result_kind != PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1) ||
		result->ih_scope_id != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	result->graph_revision = graph_revision;
	result->result_kind = result_kind;
	result->state = PROTOTYPE_SOURCE_CORE_FORMED;
	return 0;
}

int prototype_source_core_slot_record_ih_scope(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id,
	uint32_t ih_scope_id,
	uint64_t graph_revision
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result || result->state != PROTOTYPE_SOURCE_CORE_UNFORMED ||
		result->ih_scope_id != PROTOTYPE_INVALID_ID ||
		ih_scope_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	result->ih_scope_id = ih_scope_id;
	result->graph_revision = graph_revision;
	return 0;
}

int prototype_source_core_slot_reset(
	struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
) {
	struct prototype_source_core_slot_result* result =
		prototype_source_core_slot_result_open_mut(results, slot_id);
	if (!result) {
		return -1;
	}
	uint32_t ih_scope_id = result->ih_scope_id;
	uint64_t graph_revision = result->graph_revision;
	prototype_source_core_slot_result_clear(result);
	result->ih_scope_id = ih_scope_id;
	result->graph_revision = graph_revision;
	return 0;
}

const struct prototype_source_core_slot_result*
prototype_source_core_slot_result_get(
	const struct prototype_source_core_handoff_builder* results,
	uint32_t slot_id
) {
	return results && slot_id < results->slot_count ? &results->slots[slot_id] : NULL;
}

int prototype_source_core_binder_record_formed(
	struct prototype_source_core_handoff_builder* results,
	uint32_t source_binder_slot,
	uint32_t binding_id,
	uint64_t graph_revision
) {
	if (!results || results->sealed || source_binder_slot >= results->binder_count ||
		binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_source_core_binder_result* result =
		&results->binders[source_binder_slot];
	if (result->state != PROTOTYPE_SOURCE_CORE_UNFORMED) return -1;
	result->binding_id = binding_id;
	result->graph_revision = graph_revision;
	result->state = PROTOTYPE_SOURCE_CORE_FORMED;
	return 0;
}

const struct prototype_source_core_binder_result*
prototype_source_core_binder_result_get(
	const struct prototype_source_core_handoff_builder* results,
	uint32_t source_binder_slot
) {
	return results && source_binder_slot < results->binder_count ?
		&results->binders[source_binder_slot] : NULL;
}

int prototype_source_core_handoff_binding_for_source(
	const struct prototype_source_core_handoff_builder* results,
	const struct prototype_source_lowering_plan* plan,
	uint32_t source_binder_id,
	uint32_t* p_binding_id
) {
	if (!results || !plan || !p_binding_id ||
		plan->binder_count > UINT32_MAX ||
		results->source_fingerprint != plan->source_fingerprint ||
		results->binder_count != plan->binder_count) {
		return -1;
	}
	int found = 0;
	uint32_t binding_id = PROTOTYPE_INVALID_ID;
	for (uint32_t binder_slot = 0;
		binder_slot < (uint32_t)plan->binder_count;
		++binder_slot) {
		if (plan->binders[binder_slot].source_binder_id != source_binder_id) {
			continue;
		}
		const struct prototype_source_core_binder_result* result =
			prototype_source_core_binder_result_get(results, binder_slot);
		if (!result || result->state != PROTOTYPE_SOURCE_CORE_FORMED ||
			result->binding_id == PROTOTYPE_INVALID_ID ||
			(found && binding_id != result->binding_id)) {
			return -1;
		}
		binding_id = result->binding_id;
		found = 1;
	}
	if (!found) return 1;
	*p_binding_id = binding_id;
	return 0;
}

static int prototype_source_core_handoff_builder_seal_with_policy(
	struct prototype_source_core_handoff_builder* results,
	uint64_t graph_revision,
	struct prototype_source_core_handoff_view* p_view,
	int require_complete
) {
	if (!results || !p_view) {
		return -1;
	}
	if (require_complete) {
		for (size_t i = 0; i < results->slot_count; ++i) {
			if (results->slots[i].state != PROTOTYPE_SOURCE_CORE_FORMED) {
				return -1;
			}
		}
		for (size_t i = 0; i < results->binder_count; ++i) {
			if (results->binders[i].state != PROTOTYPE_SOURCE_CORE_FORMED) {
				return -1;
			}
		}
	}
	if (results->sealed) {
		if (results->sealed_graph_revision != graph_revision) {
			return -1;
		}
	} else {
		for (size_t i = 0; i < results->slot_count; ++i) {
			const struct prototype_source_core_slot_result* result =
				&results->slots[i];
			if ((require_complete &&
					result->state != PROTOTYPE_SOURCE_CORE_FORMED) ||
				result->state == PROTOTYPE_SOURCE_CORE_FORMING ||
				(result->state == PROTOTYPE_SOURCE_CORE_FORMED &&
					(result->result_kind < PROTOTYPE_SOURCE_CORE_SLOT_TERM ||
					 result->result_kind > PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1 ||
					 result->graph_revision > graph_revision ||
					(result->result_kind == PROTOTYPE_SOURCE_CORE_SLOT_TERM &&
					 result->term_id == PROTOTYPE_INVALID_ID) ||
					(result->result_kind != PROTOTYPE_SOURCE_CORE_SLOT_TERM &&
					 (result->term_id != PROTOTYPE_INVALID_ID ||
					  result->ih_scope_id != PROTOTYPE_INVALID_ID))))) {
				return -1;
			}
		}
		for (size_t i = 0; i < results->binder_count; ++i) {
			const struct prototype_source_core_binder_result* result =
				&results->binders[i];
			if ((require_complete &&
					result->state != PROTOTYPE_SOURCE_CORE_FORMED) ||
				result->state == PROTOTYPE_SOURCE_CORE_FORMING ||
				(result->state == PROTOTYPE_SOURCE_CORE_FORMED &&
					(result->binding_id == PROTOTYPE_INVALID_ID ||
					 result->graph_revision > graph_revision))) {
				return -1;
			}
		}
		results->sealed_graph_revision = graph_revision;
		results->sealed = 1;
	}
	p_view->slots = results->slots;
	p_view->slot_count = results->slot_count;
	p_view->binders = results->binders;
	p_view->binder_count = results->binder_count;
	p_view->source_fingerprint = results->source_fingerprint;
	p_view->graph_revision = results->sealed_graph_revision;
	return 0;
}

int prototype_source_core_handoff_builder_seal(
	struct prototype_source_core_handoff_builder* results,
	uint64_t graph_revision,
	struct prototype_source_core_handoff_view* p_view
) {
	return prototype_source_core_handoff_builder_seal_with_policy(
		results, graph_revision, p_view, 0
	);
}

int prototype_source_core_handoff_builder_seal_complete(
	struct prototype_source_core_handoff_builder* results,
	uint64_t graph_revision,
	struct prototype_source_core_handoff_view* p_view
) {
	return prototype_source_core_handoff_builder_seal_with_policy(
		results, graph_revision, p_view, 1
	);
}

const struct prototype_source_core_slot_result*
prototype_source_core_handoff_view_slot_get(
	const struct prototype_source_core_handoff_view* view,
	uint32_t slot_id
) {
	return view && slot_id < view->slot_count ? &view->slots[slot_id] : NULL;
}

const struct prototype_source_core_binder_result*
prototype_source_core_handoff_view_binder_get(
	const struct prototype_source_core_handoff_view* view,
	uint32_t source_binder_slot
) {
	return view && source_binder_slot < view->binder_count ?
		&view->binders[source_binder_slot] : NULL;
}
