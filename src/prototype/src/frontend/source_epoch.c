#include "a_program/frontend/source_epoch.h"

#include <stdlib.h>
#include <string.h>

#include "a_program/frontend/source_lowering_plan.h"

static int source_epoch_shape_valid(const struct prototype_ast_db* source) {
	return source && source->node_count <= source->node_capacity &&
		(source->node_count == 0 || source->nodes) &&
		source->expectation_count <= source->expectation_capacity &&
		(source->expectation_count == 0 || source->expectations) &&
		source->assignment_count <= source->assignment_capacity &&
		(source->assignment_count == 0 || source->assignments) &&
		source->import_count <= source->import_capacity &&
		(source->import_count == 0 || source->imports) &&
		source->def_index_count <= source->def_index_capacity &&
		(source->def_index_capacity == 0 || source->def_index) &&
		source->case_count <= source->case_capacity &&
		(source->case_count == 0 || source->cases) &&
		source->case_binder_count <= source->case_binder_capacity &&
		(source->case_binder_count == 0 || source->case_binders) &&
		source->match_selector_count <= source->match_selector_capacity &&
		(source->match_selector_count == 0 || source->match_selectors) &&
		source->computation_fold_clause_count <=
			source->computation_fold_clause_capacity &&
		(source->computation_fold_clause_count == 0 ||
			source->computation_fold_clauses) &&
		source->block_item_count <= source->block_item_capacity &&
		(source->block_item_count == 0 || source->block_items) &&
		source->definition_item_count <= source->definition_item_capacity &&
		(source->definition_item_count == 0 || source->definition_items) &&
		source->type_expr_count <= source->type_expr_capacity &&
		(source->type_expr_count == 0 || source->type_exprs) &&
		source->type_def_count <= source->type_def_capacity &&
		(source->type_def_count == 0 || source->type_defs) &&
		source->family_binder_count <= source->family_binder_capacity &&
		(source->family_binder_count == 0 || source->family_binders) &&
		source->type_constructor_count <= source->type_constructor_capacity &&
		(source->type_constructor_count == 0 || source->type_constructors) &&
		source->type_field_expr_count <= source->type_field_expr_capacity &&
		(source->type_field_expr_count == 0 || (source->type_field_exprs &&
			source->type_field_binder_ids &&
			source->type_field_name_symbol_ids)) &&
		source->accepted_binding_substitution_count <=
			source->accepted_binding_substitution_capacity &&
		(source->accepted_binding_substitution_count == 0 ||
			source->accepted_binding_substitutions);
}

#define SOURCE_EPOCH_ALLOCATE(target, source, member, element_type, capacity_member) \
	do { \
		if ((source)->capacity_member > 0) { \
			(target)->member = calloc( \
				(source)->capacity_member, sizeof(element_type) \
			); \
			if (!(target)->member) goto fail; \
		} \
	} while (0)

#define SOURCE_EPOCH_COPY(target, source, member, count_member) \
	do { \
		if ((source)->count_member > 0) { \
			memcpy( \
				(target)->member, (source)->member, \
				(source)->count_member * sizeof(*(source)->member) \
			); \
		} \
		(target)->count_member = (source)->count_member; \
	} while (0)

int prototype_source_epoch_clone(
	const struct prototype_ast_db* source,
	struct prototype_source_epoch_storage* p_epoch
) {
	uint64_t parent_before;
	if (!p_epoch || !source_epoch_shape_valid(source) ||
		prototype_source_epoch_fingerprint(source, &parent_before) != 0) {
		return -1;
	}
	memset(p_epoch, 0, sizeof(*p_epoch));
	struct prototype_ast_db* target = &p_epoch->asts;
	SOURCE_EPOCH_ALLOCATE(target, source, nodes, struct prototype_ast_node,
		node_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, expectations,
		struct prototype_ast_type_expectation_def, expectation_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, assignments,
		struct prototype_ast_term_assignment_def, assignment_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, imports,
		struct prototype_ast_import_def, import_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, def_index,
		struct prototype_ast_def_open_address_entry, def_index_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, cases,
		struct prototype_ast_match_case, case_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, case_binders,
		struct prototype_ast_binder, case_binder_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, match_selectors,
		struct prototype_ast_match_selector, match_selector_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, computation_fold_clauses,
		struct prototype_ast_computation_fold_clause,
		computation_fold_clause_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, block_items, uint32_t,
		block_item_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, definition_items, uint32_t,
		definition_item_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_exprs,
		struct prototype_ast_type_expr, type_expr_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_defs,
		struct prototype_ast_type_def, type_def_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, family_binders,
		struct prototype_ast_family_binder, family_binder_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_constructors,
		struct prototype_ast_type_constructor, type_constructor_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_field_exprs, uint32_t,
		type_field_expr_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_field_binder_ids, uint32_t,
		type_field_expr_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, type_field_name_symbol_ids, int,
		type_field_expr_capacity);
	SOURCE_EPOCH_ALLOCATE(target, source, accepted_binding_substitutions,
		struct prototype_ast_accepted_binding_substitution,
		accepted_binding_substitution_capacity);
	struct prototype_ast_match_selector* match_selectors =
		target->match_selectors;
	struct prototype_ast_accepted_binding_substitution* substitutions =
		target->accepted_binding_substitutions;

	prototype_ast_db_init(
		target,
		target->nodes, source->node_capacity,
		target->expectations, source->expectation_capacity,
		target->assignments, source->assignment_capacity,
		target->imports, source->import_capacity,
		target->def_index, source->def_index_capacity,
		target->cases, source->case_capacity,
		target->case_binders, source->case_binder_capacity,
		target->computation_fold_clauses,
		source->computation_fold_clause_capacity,
		target->block_items, source->block_item_capacity,
		target->definition_items, source->definition_item_capacity,
		target->type_exprs, source->type_expr_capacity,
		target->type_defs, source->type_def_capacity,
		target->family_binders, source->family_binder_capacity,
		target->type_constructors, source->type_constructor_capacity,
		target->type_field_exprs, target->type_field_binder_ids,
		target->type_field_name_symbol_ids, source->type_field_expr_capacity
	);
	prototype_ast_db_set_match_selector_storage(
		target, match_selectors, source->match_selector_capacity
	);
	prototype_ast_db_set_accepted_substitution_storage(
		target, substitutions,
		source->accepted_binding_substitution_capacity
	);

	SOURCE_EPOCH_COPY(target, source, nodes, node_count);
	SOURCE_EPOCH_COPY(target, source, expectations, expectation_count);
	SOURCE_EPOCH_COPY(target, source, assignments, assignment_count);
	SOURCE_EPOCH_COPY(target, source, imports, import_count);
	if (source->def_index_capacity > 0) {
		memcpy(
			target->def_index, source->def_index,
			source->def_index_capacity * sizeof(*source->def_index)
		);
	}
	target->def_index_count = source->def_index_count;
	SOURCE_EPOCH_COPY(target, source, cases, case_count);
	SOURCE_EPOCH_COPY(target, source, case_binders, case_binder_count);
	SOURCE_EPOCH_COPY(target, source, match_selectors, match_selector_count);
	SOURCE_EPOCH_COPY(
		target, source, computation_fold_clauses,
		computation_fold_clause_count
	);
	SOURCE_EPOCH_COPY(target, source, block_items, block_item_count);
	SOURCE_EPOCH_COPY(target, source, definition_items, definition_item_count);
	SOURCE_EPOCH_COPY(target, source, type_exprs, type_expr_count);
	SOURCE_EPOCH_COPY(target, source, type_defs, type_def_count);
	SOURCE_EPOCH_COPY(target, source, family_binders, family_binder_count);
	SOURCE_EPOCH_COPY(target, source, type_constructors, type_constructor_count);
	SOURCE_EPOCH_COPY(target, source, type_field_exprs, type_field_expr_count);
	if (source->type_field_expr_count > 0) {
		memcpy(
			target->type_field_binder_ids, source->type_field_binder_ids,
			source->type_field_expr_count *
				sizeof(*source->type_field_binder_ids)
		);
		memcpy(
			target->type_field_name_symbol_ids,
			source->type_field_name_symbol_ids,
			source->type_field_expr_count *
				sizeof(*source->type_field_name_symbol_ids)
		);
	}
	SOURCE_EPOCH_COPY(
		target, source, accepted_binding_substitutions,
		accepted_binding_substitution_count
	);
	target->root_definition_block = source->root_definition_block;
	target->root_definition_select = source->root_definition_select;
	target->next_ast_binder_id = source->next_ast_binder_id;
	target->next_ast_level_var = source->next_ast_level_var;
	target->next_source_entry_id = source->next_source_entry_id;

	uint64_t parent_after;
	uint64_t clone_fingerprint;
	if (prototype_source_epoch_fingerprint(source, &parent_after) != 0 ||
		prototype_source_epoch_fingerprint(target, &clone_fingerprint) != 0 ||
		parent_before != parent_after || parent_before != clone_fingerprint) {
		goto fail;
	}
	p_epoch->parent_fingerprint = parent_before;
	p_epoch->initialized = 1;
	return 0;

fail:
	prototype_source_epoch_destroy(p_epoch);
	return -1;
}

int prototype_source_epoch_parent_matches(
	const struct prototype_source_epoch_storage* epoch,
	const struct prototype_ast_db* parent
) {
	uint64_t current;
	return epoch && epoch->initialized && parent &&
		prototype_source_epoch_fingerprint(parent, &current) == 0 &&
		current == epoch->parent_fingerprint ? 0 : -1;
}

void prototype_source_epoch_destroy(
	struct prototype_source_epoch_storage* epoch
) {
	if (!epoch) return;
	free(epoch->asts.accepted_binding_substitutions);
	free(epoch->asts.type_field_name_symbol_ids);
	free(epoch->asts.type_field_binder_ids);
	free(epoch->asts.type_field_exprs);
	free(epoch->asts.type_constructors);
	free(epoch->asts.family_binders);
	free(epoch->asts.type_defs);
	free(epoch->asts.type_exprs);
	free(epoch->asts.definition_items);
	free(epoch->asts.block_items);
	free(epoch->asts.computation_fold_clauses);
	free(epoch->asts.match_selectors);
	free(epoch->asts.case_binders);
	free(epoch->asts.cases);
	free(epoch->asts.def_index);
	free(epoch->asts.imports);
	free(epoch->asts.assignments);
	free(epoch->asts.expectations);
	free(epoch->asts.nodes);
	memset(epoch, 0, sizeof(*epoch));
}

#undef SOURCE_EPOCH_COPY
#undef SOURCE_EPOCH_ALLOCATE
