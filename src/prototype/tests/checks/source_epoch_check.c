#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "a_program/frontend/ast.h"
#include "a_program/frontend/source_epoch.h"
#include "a_program/frontend/source_lowering_plan.h"

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

int main(void) {
	struct prototype_ast_node nodes[8];
	struct prototype_ast_type_expectation_def expectations[2];
	struct prototype_ast_term_assignment_def assignments[4];
	struct prototype_ast_import_def imports[2];
	struct prototype_ast_def_open_address_entry def_index[8];
	struct prototype_ast_match_case cases[2];
	struct prototype_ast_binder case_binders[4];
	struct prototype_ast_match_selector selectors[4];
	struct prototype_ast_computation_fold_clause fold_clauses[2];
	uint32_t block_items[4];
	uint32_t definition_items[4];
	struct prototype_ast_type_expr type_exprs[4];
	struct prototype_ast_type_def type_defs[2];
	struct prototype_ast_family_binder family_binders[4];
	struct prototype_ast_type_constructor constructors[4];
	uint32_t field_exprs[4];
	uint32_t field_binders[4];
	int field_symbols[4];
	struct prototype_ast_accepted_binding_substitution substitutions[4];
	memset(nodes, 0, sizeof(nodes));
	memset(expectations, 0, sizeof(expectations));
	memset(assignments, 0, sizeof(assignments));
	memset(imports, 0, sizeof(imports));
	memset(def_index, 0, sizeof(def_index));
	memset(cases, 0, sizeof(cases));
	memset(case_binders, 0, sizeof(case_binders));
	memset(selectors, 0, sizeof(selectors));
	memset(fold_clauses, 0, sizeof(fold_clauses));
	memset(block_items, 0, sizeof(block_items));
	memset(definition_items, 0, sizeof(definition_items));
	memset(type_exprs, 0, sizeof(type_exprs));
	memset(type_defs, 0, sizeof(type_defs));
	memset(family_binders, 0, sizeof(family_binders));
	memset(constructors, 0, sizeof(constructors));
	memset(field_exprs, 0, sizeof(field_exprs));
	memset(field_binders, 0, sizeof(field_binders));
	memset(field_symbols, 0, sizeof(field_symbols));
	memset(substitutions, 0, sizeof(substitutions));

	struct prototype_ast_db source;
	prototype_ast_db_init(
		&source,
		nodes, ARRAY_COUNT(nodes),
		expectations, ARRAY_COUNT(expectations),
		assignments, ARRAY_COUNT(assignments),
		imports, ARRAY_COUNT(imports),
		def_index, ARRAY_COUNT(def_index),
		cases, ARRAY_COUNT(cases),
		case_binders, ARRAY_COUNT(case_binders),
		fold_clauses, ARRAY_COUNT(fold_clauses),
		block_items, ARRAY_COUNT(block_items),
		definition_items, ARRAY_COUNT(definition_items),
		type_exprs, ARRAY_COUNT(type_exprs),
		type_defs, ARRAY_COUNT(type_defs),
		family_binders, ARRAY_COUNT(family_binders),
		constructors, ARRAY_COUNT(constructors),
		field_exprs, field_binders, field_symbols, ARRAY_COUNT(field_exprs)
	);
	prototype_ast_db_set_match_selector_storage(
		&source, selectors, ARRAY_COUNT(selectors)
	);
	prototype_ast_db_set_accepted_substitution_storage(
		&source, substitutions, ARRAY_COUNT(substitutions)
	);

	uint32_t literal;
	uint32_t assignment;
	struct prototype_source_span span = { .line = 1, .column = 1 };
	if (prototype_ast_int_literal(&source, 7, span, &literal) != 0 ||
		prototype_ast_add_term_assignment(
			&source, 10, literal, prototype_ast_new_source_entry(&source),
			span, span, &assignment
		) != 0 || assignment != 0) {
		fprintf(stderr, "source epoch fixture construction failed\n");
		return 1;
	}
	source.type_exprs[0].tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	source.type_expr_count = 1;
	source.accepted_binding_substitutions[0] =
		(struct prototype_ast_accepted_binding_substitution) {
			.source_binding_id = 3,
			.target_value = literal
		};
	source.accepted_binding_substitution_count = 1;

	uint64_t source_fingerprint;
	if (prototype_source_epoch_fingerprint(
			&source, &source_fingerprint
		) != 0) {
		return 1;
	}
	struct prototype_source_epoch_storage clone;
	if (prototype_source_epoch_clone(&source, &clone) != 0 ||
		prototype_source_epoch_parent_matches(&clone, &source) != 0 ||
		clone.asts.nodes == source.nodes ||
		clone.asts.assignments == source.assignments ||
		clone.asts.def_index == source.def_index ||
		clone.asts.node_count != source.node_count ||
		clone.asts.assignment_count != source.assignment_count ||
		clone.asts.type_expr_count != source.type_expr_count ||
		clone.asts.accepted_binding_substitution_count !=
			source.accepted_binding_substitution_count) {
		fprintf(stderr, "source epoch clone did not preserve an isolated prefix\n");
		return 1;
	}

	uint32_t generated_literal;
	if (prototype_ast_int_literal(
			&clone.asts, 9, span, &generated_literal
		) != 0 || generated_literal == literal || source.node_count != 1 ||
		prototype_source_epoch_parent_matches(&clone, &source) != 0) {
		fprintf(stderr, "generated epoch extension changed its parent\n");
		return 1;
	}
	uint64_t clone_fingerprint;
	if (prototype_source_epoch_fingerprint(
			&clone.asts, &clone_fingerprint
		) != 0 || clone_fingerprint == source_fingerprint) {
		fprintf(stderr, "generated epoch did not acquire an independent identity\n");
		return 1;
	}

	source.nodes[literal].as.int_literal.value = 8;
	if (prototype_source_epoch_parent_matches(&clone, &source) == 0) {
		fprintf(stderr, "source epoch seal accepted parent mutation\n");
		return 1;
	}
	source.nodes[literal].as.int_literal.value = 7;
	if (prototype_source_epoch_parent_matches(&clone, &source) != 0) {
		fprintf(stderr, "source epoch seal did not recover after restoration\n");
		return 1;
	}
	prototype_source_epoch_destroy(&clone);
	if (clone.initialized || clone.asts.nodes) {
		fprintf(stderr, "source epoch storage was not released\n");
		return 1;
	}
	return 0;
}
