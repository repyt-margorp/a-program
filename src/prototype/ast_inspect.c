#include "ast_inspect.h"

static const char* safe_symbol_name(const struct symbol_table* symbols, int symbol_id) {
	const char* name = symbol_to_string(symbols, symbol_id);
	return name ? name : "<unknown>";
}

static const char* ast_tag_name(int tag) {
	switch (tag) {
		case PROTOTYPE_AST_VAR:
			return "var";
		case PROTOTYPE_AST_NAME:
			return "name";
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
			return "namespace-name";
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE:
			return "ast-namespace-name";
		case PROTOTYPE_AST_APP:
			return "app";
		case PROTOTYPE_AST_LAMBDA:
			return "lambda";
		case PROTOTYPE_AST_MATCH:
			return "match";
		case PROTOTYPE_AST_TYPE_LITERAL:
			return "type-literal";
		case PROTOTYPE_AST_TYPE_FORMATION:
			return "type-formation";
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
			return "induction-hypothesis";
			case PROTOTYPE_AST_TEXT_LITERAL:
				return "text-literal";
			case PROTOTYPE_AST_INT_LITERAL:
				return "int-literal";
			case PROTOTYPE_AST_INTRINSIC_NAME:
				return "intrinsic-name";
		case PROTOTYPE_AST_ASCRIPTION:
			return "ascription";
		default:
			return "unknown";
	}
}

static const char* type_expr_tag_name(int tag) {
	switch (tag) {
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE:
			return "universe";
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR:
			return "universe-var";
		case PROTOTYPE_AST_TYPE_EXPR_SELF:
			return "self";
		case PROTOTYPE_AST_TYPE_EXPR_VAR:
			return "var";
		case PROTOTYPE_AST_TYPE_EXPR_NAME:
			return "name";
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			return "app";
			case PROTOTYPE_AST_TYPE_EXPR_ARROW:
				return "arrow";
			case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE:
				return "host-type";
			default:
				return "unknown";
	}
}

static const char* type_entry_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_AST_TYPE_ENTRY_DECLARATION:
			return "declaration";
		case PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION:
			return "expectation";
		default:
			return "unknown";
	}
}

void prototype_ast_inspect_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_ast_db* asts
) {
	if (!output || !symbols || !asts) {
		return;
	}

	fprintf(
		output,
		"ast-inspect nodes=%zu expectations=%zu assignments=%zu def_index_entries=%zu type_exprs=%zu type_defs=%zu\n",
		asts->node_count,
		asts->expectation_count,
		asts->assignment_count,
		asts->def_index_count,
		asts->type_expr_count,
		asts->type_def_count
	);
	for (size_t i = 0; i < asts->def_index_capacity; ++i) {
		const struct prototype_ast_def_open_address_entry* entry = &asts->def_index[i];
		if (!entry->occupied) {
			continue;
		}
		fprintf(
			output,
			"ast-def-index[%zu] %s expectations=%u assignments=%u first-expectation=%u first-assignment=%u\n",
			i,
			safe_symbol_name(symbols, entry->symbol_id),
			entry->expectation_count,
			entry->assignment_count,
			entry->first_expectation,
			entry->first_assignment
		);
	}
	for (size_t i = 0; i < asts->expectation_count; ++i) {
		const struct prototype_ast_type_expectation_def* expectation = &asts->expectations[i];
		fprintf(
			output,
			"ast-type-entry %s kind=%s type-expr=%u source-entry=%u span=%u:%u paired-assignment=%u next=%u\n",
			safe_symbol_name(symbols, expectation->name_symbol_id),
			type_entry_kind_name(expectation->kind),
			expectation->type_expr,
			expectation->source_entry_id,
			expectation->name_span.line,
			expectation->name_span.column,
			expectation->paired_assignment_id,
			expectation->next_for_symbol
		);
	}
	for (size_t i = 0; i < asts->type_expr_count; ++i) {
		const struct prototype_ast_type_expr* expr = &asts->type_exprs[i];
		fprintf(
			output,
			"ast-type-expr #%zu tag=%s span=%u:%u\n",
			i,
			type_expr_tag_name(expr->tag),
			expr->span.line,
			expr->span.column
		);
	}
	for (size_t i = 0; i < asts->assignment_count; ++i) {
		const struct prototype_ast_term_assignment_def* assignment = &asts->assignments[i];
		fprintf(
			output,
			"ast-assign %s node=%u source-entry=%u span=%u:%u next=%u tag=%s\n",
			safe_symbol_name(symbols, assignment->name_symbol_id),
			assignment->ast,
			assignment->source_entry_id,
			assignment->name_span.line,
			assignment->name_span.column,
			assignment->next_for_symbol,
			assignment->ast < asts->node_count ? ast_tag_name(asts->nodes[assignment->ast].tag) : "bad-node"
		);
	}
	for (size_t i = 0; i < asts->type_def_count; ++i) {
		const struct prototype_ast_type_def* type = &asts->type_defs[i];
		fprintf(
			output,
			"ast-type %s parameters=%u constructors=%u\n",
			safe_symbol_name(symbols, type->name_symbol_id),
			type->parameter_count,
			type->constructor_count
		);
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			const struct prototype_ast_type_constructor* constructor =
				&asts->type_constructors[type->first_constructor + j];
			fprintf(
				output,
				"ast-constructor %s.%s fields=%u\n",
				safe_symbol_name(symbols, type->name_symbol_id),
				safe_symbol_name(symbols, constructor->name_symbol_id),
				constructor->field_count
			);
		}
	}
}
