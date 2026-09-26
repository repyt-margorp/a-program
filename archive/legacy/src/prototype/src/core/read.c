#include "a_program/core/read.h"
#include "a_program/core/read_provider.h"
#include "a_program/core/term.h"

#include <string.h>

static const struct prototype_term_db* snapshot_terms(
	const struct prototype_core_read_snapshot* snapshot
) {
	return snapshot ? snapshot->provider : NULL;
}

int prototype_core_read_snapshot_begin(
	const struct prototype_term_db* terms,
	struct prototype_core_read_snapshot* snapshot
) {
	if (!terms || !snapshot) {
		return -1;
	}
	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->provider = terms;
	snapshot->graph_revision = terms->normalization_graph_revision;
	snapshot->term_count = terms->term_count;
	snapshot->case_count = terms->case_count;
	snapshot->case_binder_count = terms->case_binder_count;
	snapshot->ih_scope_count = terms->ih_scope_count;
	snapshot->computation_fold_clause_count =
		terms->computation_fold_clause_count;
	return 0;
}

int prototype_core_read_snapshot_validate(
	const struct prototype_core_read_snapshot* snapshot
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!terms || snapshot->graph_revision !=
			terms->normalization_graph_revision ||
		snapshot->term_count > terms->term_count ||
		snapshot->case_count > terms->case_count ||
		snapshot->case_binder_count > terms->case_binder_count ||
		snapshot->ih_scope_count > terms->ih_scope_count ||
		snapshot->computation_fold_clause_count >
			terms->computation_fold_clause_count) {
		return -1;
	}
	return 0;
}

int prototype_core_read_term(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	struct prototype_term* term
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!term || prototype_core_read_snapshot_validate(snapshot) != 0 ||
		term_id >= snapshot->term_count) {
		return -1;
	}
	*term = terms->terms[term_id];
	return 0;
}

int prototype_core_read_child_count(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t* child_count
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!child_count || prototype_core_read_snapshot_validate(snapshot) != 0 ||
		term_id >= snapshot->term_count) {
		return -1;
	}
	return prototype_term_child_count(terms, term_id, child_count);
}

int prototype_core_read_child_at(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t child_index,
	struct prototype_term_child* child
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!child || prototype_core_read_snapshot_validate(snapshot) != 0 ||
		term_id >= snapshot->term_count) {
		return -1;
	}
	return prototype_term_child_at(terms, term_id, child_index, child);
}

int prototype_core_read_constructor_spine(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t* head,
	uint32_t* owner,
	uint32_t* constructor_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* argument_count
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (prototype_core_read_snapshot_validate(snapshot) != 0 ||
		term_id >= snapshot->term_count) {
		return -1;
	}
	return prototype_term_constructor_spine_info(
		terms,
		term_id,
		head,
		owner,
		constructor_id,
		arguments,
		argument_capacity,
		argument_count
	);
}

int prototype_core_read_match_case(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t case_id,
	struct prototype_core_match_case_view* match_case
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!match_case || prototype_core_read_snapshot_validate(snapshot) != 0 ||
		case_id >= snapshot->case_count) {
		return -1;
	}
	match_case->label_symbol_id = terms->case_label_symbols[case_id];
	match_case->match_case = terms->cases[case_id];
	return 0;
}

int prototype_core_read_case_binder(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t binder_id,
	struct prototype_case_binder* binder
) {
	const struct prototype_term_db* terms = snapshot_terms(snapshot);
	if (!binder || prototype_core_read_snapshot_validate(snapshot) != 0 ||
		binder_id >= snapshot->case_binder_count) {
		return -1;
	}
	*binder = terms->case_binders[binder_id];
	return 0;
}
