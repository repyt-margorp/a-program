#include "a_program/frontend/source_core_handoff.h"
#include "a_program/support/schema.h"

#include <stdio.h>

int main(void) {
	struct prototype_source_core_slot_result scoped_slots[1];
	struct prototype_source_core_handoff_builder scoped;
	if (prototype_source_core_handoff_builder_init(
			&scoped, scoped_slots, 1, NULL, 0, UINT64_C(121)
		) != 0 || prototype_source_core_slot_record_ih_scope(
			&scoped, 0, 7, 1
		) != 0 || prototype_source_core_slot_begin(&scoped, 0) != 0 ||
		prototype_source_core_slot_reset(&scoped, 0) != 0 ||
		scoped_slots[0].ih_scope_id != 7 ||
		prototype_source_core_slot_begin(&scoped, 0) != 0 ||
		prototype_source_core_slot_commit(&scoped, 0, 3, 8, 2) == 0 ||
		prototype_source_core_slot_reset(&scoped, 0) != 0 ||
		prototype_source_core_slot_begin(&scoped, 0) != 0 ||
		prototype_source_core_slot_commit(&scoped, 0, 3, 7, 2) != 0) {
		fprintf(stderr, "C0 handoff lost or replaced a reserved IH scope\n");
		return 1;
	}
	struct prototype_source_core_slot_result nonterm_slots[2];
	struct prototype_source_core_handoff_builder nonterm;
	struct prototype_source_core_handoff_view nonterm_view;
	if (prototype_source_core_handoff_builder_init(
			&nonterm, nonterm_slots, 2, NULL, 0, UINT64_C(120)
		) != 0 || prototype_source_core_slot_record_nonterm(
			&nonterm, 0, PROTOTYPE_SOURCE_CORE_SLOT_SOURCE_ONLY, 0
		) != 0 || prototype_source_core_slot_record_nonterm(
			&nonterm, 1, PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1, 0
		) != 0 || prototype_source_core_handoff_builder_seal_complete(
			&nonterm, 0, &nonterm_view
		) != 0 || nonterm_slots[0].term_id != PROTOTYPE_INVALID_ID ||
		nonterm_slots[0].result_kind != PROTOTYPE_SOURCE_CORE_SLOT_SOURCE_ONLY ||
		nonterm_slots[1].result_kind != PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1) {
		fprintf(stderr, "C0 handoff rejected a completed non-Term decision\n");
		return 1;
	}

	struct prototype_source_core_slot_result incomplete_slots[1];
	struct prototype_source_core_binder_result incomplete_binders[1];
	struct prototype_source_core_handoff_builder incomplete;
	struct prototype_source_core_handoff_view incomplete_view = { 0 };
	if (prototype_source_core_handoff_builder_init(
			&incomplete, incomplete_slots, 1, incomplete_binders, 1,
			UINT64_C(122)
		) != 0 || prototype_source_core_handoff_builder_seal_complete(
			&incomplete, 0, &incomplete_view
		) == 0 || incomplete.sealed) {
		fprintf(stderr, "C0 complete handoff accepted missing results\n");
		return 1;
	}

	struct prototype_source_core_slot_result slots[3];
	struct prototype_source_core_binder_result binders[2];
	struct prototype_source_core_handoff_builder results;
	struct prototype_source_core_handoff_view view = { 0 };
	if (prototype_source_core_handoff_builder_init(
			&results, slots, 3, binders, 2, UINT64_C(123)
		) != 0 || prototype_source_core_slot_begin(&results, 0) != 0 ||
		prototype_source_core_handoff_builder_seal(&results, 1, &view) == 0 ||
		prototype_source_core_slot_reset(&results, 0) != 0) {
		fprintf(stderr, "C0 handoff accepted an in-progress slot\n");
		return 1;
	}
	if (prototype_source_core_slot_record_formed(
			&results, 0, 4, PROTOTYPE_INVALID_ID, 2
		) != 0 || prototype_source_core_slot_begin(&results, 1) != 0 ||
		prototype_source_core_slot_commit(&results, 1, 8, 9, 4) != 0 ||
		prototype_source_core_binder_record_formed(
			&results, 0, 12, 3
		) != 0 || prototype_source_core_slot_record_formed(
			&results, 2, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 4
		) == 0 || prototype_source_core_handoff_builder_seal(
			&results, 3, &view
		) == 0 || prototype_source_core_handoff_builder_seal(
			&results, 5, &view
		) != 0) {
		fprintf(stderr, "C0 handoff sealing validation failed\n");
		return 1;
	}
	const struct prototype_source_core_slot_result* first =
		prototype_source_core_handoff_view_slot_get(&view, 0);
	const struct prototype_source_core_slot_result* second =
		prototype_source_core_handoff_view_slot_get(&view, 1);
	const struct prototype_source_core_slot_result* third =
		prototype_source_core_handoff_view_slot_get(&view, 2);
	const struct prototype_source_core_binder_result* binder =
		prototype_source_core_handoff_view_binder_get(&view, 0);
	if (!results.sealed || results.sealed_graph_revision != 5 ||
		view.graph_revision != 5 || view.source_fingerprint != UINT64_C(123) ||
		!first || !second || !third || !binder ||
		first->state != PROTOTYPE_SOURCE_CORE_FORMED || first->term_id != 4 ||
		first->result_kind != PROTOTYPE_SOURCE_CORE_SLOT_TERM ||
		first->ih_scope_id != PROTOTYPE_INVALID_ID ||
		second->state != PROTOTYPE_SOURCE_CORE_FORMED || second->term_id != 8 ||
		second->ih_scope_id != 9 || binder->binding_id != 12 ||
		third->state != PROTOTYPE_SOURCE_CORE_UNFORMED ||
		prototype_source_core_handoff_view_slot_get(&view, 3) != NULL ||
		prototype_source_core_handoff_view_binder_get(&view, 2) != NULL) {
		fprintf(stderr, "C0 immutable handoff contents are inconsistent\n");
		return 1;
	}
	if (prototype_source_core_slot_begin(&results, 2) == 0 ||
		prototype_source_core_slot_reset(&results, 0) == 0 ||
		prototype_source_core_slot_commit(
			&results, 2, 9, PROTOTYPE_INVALID_ID, 5
		) == 0 || prototype_source_core_slot_record_formed(
			&results, 2, 9, PROTOTYPE_INVALID_ID, 5
		) == 0 || prototype_source_core_binder_record_formed(
			&results, 1, 13, 5
		) == 0 ||
		prototype_source_core_handoff_builder_seal(&results, 6, &view) == 0 ||
		prototype_source_core_handoff_builder_seal(&results, 5, &view) != 0) {
		fprintf(stderr, "C0 handoff remained mutable after sealing\n");
		return 1;
	}
	printf("source Core handoff check passed\n");
	return 0;
}
