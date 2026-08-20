#include "a_program/driver/diagnostics.h"

#include "a_program/core/term.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/kernel/universe.h"
#include "a_program/support/symbol.h"

const char* prototype_diagnostic_resolve_error_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_RESOLVE_ERROR_NAME: return "name";
		case PROTOTYPE_RESOLVE_ERROR_NAMESPACE: return "namespace";
		case PROTOTYPE_RESOLVE_ERROR_RECURSIVE: return "recursive";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_EXPECTATION: return "duplicate-expectation";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_ASSIGNMENT: return "duplicate-assignment";
		case PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT: return "ambiguous-assignment";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION: return "duplicate-definition";
		case PROTOTYPE_RESOLVE_ERROR_COMPILE: return "compile";
		default: return "unknown";
	}
}

const char* prototype_compile_diagnostic_phase_name(int phase) {
	switch (phase) {
		case PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_GRAPH_CONSTRUCTION:
			return "graph-construction";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER:
			return "constraint-solver";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_EXPECTATION_CHECK:
			return "expectation-check";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_PROOF_REPLAY:
			return "proof-replay";
		default:
			return "unknown";
	}
}

const char* prototype_compile_diagnostic_reason_name(int reason) {
	switch (reason) {
		case PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER:
			return "unsolved-classifier";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_CONSTRUCTOR_DOMAIN_MISMATCH:
			return "constructor-domain-mismatch";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP:
			return "ih-ownership";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_MATCH_RESULT_MOTIVE:
			return "match-result-motive";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_INTEGER_LITERAL_RANGE:
			return "integer-literal-range";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH:
			return "ascription-mismatch";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_UNSUPPORTED_NESTED_RECURSION:
			return "unsupported-nested-recursion";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_UNSUPPORTED_INDEXED_FAMILY:
			return "unsupported-indexed-family";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_NESTED_MATCH_GROUPING:
			return "nested-match-grouping";
		case PROTOTYPE_COMPILE_DIAGNOSTIC_BRANCH_REFINEMENT_RESIDUAL:
			return "branch-refinement-residual";
		default:
			return "unknown";
	}
}

void prototype_diagnostic_print_compile_diagnostics(
	FILE* stream,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !metadata) {
		return;
	}
	for (size_t i = 0; i < metadata->compile_diagnostic_count; ++i) {
		const struct prototype_compile_diagnostic* diagnostic =
			&metadata->compile_diagnostics[i];
		fprintf(
			stream,
			"compile-diagnostic diagnostic-code=%s phase=%s ast#%u span=%u:%u occurrence#%u constraint#%u expected=term#%u actual=term#%u",
			prototype_compile_diagnostic_reason_name(diagnostic->reason),
			prototype_compile_diagnostic_phase_name(diagnostic->phase),
			diagnostic->source_ast,
			diagnostic->span.line,
			diagnostic->span.column,
			diagnostic->occurrence_id,
			diagnostic->constraint_id,
			diagnostic->expected_classifier,
			diagnostic->actual_classifier
		);
		if (diagnostic->context_id != PROTOTYPE_INVALID_ID) {
			fprintf(stream, " context#%u", diagnostic->context_id);
		}
		if (diagnostic->constructor_ordinal != PROTOTYPE_INVALID_ID) {
			fprintf(stream, " constructor-ordinal=%u", diagnostic->constructor_ordinal);
		}
		if (diagnostic->field_ordinal != PROTOTYPE_INVALID_ID) {
			fprintf(stream, " field-ordinal=%u", diagnostic->field_ordinal);
		}
		fputc('\n', stream);
	}
}

void prototype_diagnostic_print_resolve_errors(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !metadata) {
		return;
	}
	for (size_t i = 0; i < metadata->resolve_error_count; ++i) {
		const struct prototype_resolve_error* error = &metadata->resolve_errors[i];
		fprintf(
			stream,
			"metadata resolve-error kind=%s name=%s",
			prototype_diagnostic_resolve_error_kind_name(error->kind),
			symbol_to_string(symbols, error->name_symbol_id)
		);
		if (error->member_symbol_id >= 0) {
			fprintf(stream, ".%s", symbol_to_string(symbols, error->member_symbol_id));
		}
		fprintf(stream, " ast#%u span=%u:%u\n", error->ast, error->span.line, error->span.column);
	}
}

static const char* resolution_event_kind_name(int kind) {
	return kind == PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR ?
		"match-constructor" : "unknown";
}

static const char* resolution_item_state_name(int state) {
	switch (state) {
		case PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED: return "unresolved";
		case PROTOTYPE_RESOLUTION_ITEM_RESOLVED: return "resolved";
		case PROTOTYPE_RESOLUTION_ITEM_ERROR: return "error";
		default: return "unknown";
	}
}

void prototype_diagnostic_print_resolution_trace(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !intrinsic_environment || !metadata) {
		return;
	}
	fprintf(
		stream,
		"resolution_items=%zu resolution_iterations=%zu resolution_events=%zu\n",
		metadata->resolution_item_count,
		metadata->resolution_iteration_count,
		metadata->resolution_event_count
	);
	for (size_t i = 0; i < metadata->resolution_item_count; ++i) {
		const struct prototype_resolution_item* item = &metadata->resolution_items[i];
		fprintf(
			stream,
			"resolution-item item#%u kind=%s state=%s created=%u resolved=%u ast#%u case#%u @%s scrutinee=term#%u",
			item->id,
			resolution_event_kind_name(item->kind),
			resolution_item_state_name(item->state),
			item->created_iteration,
			item->resolved_iteration,
			item->ast,
			item->case_index,
			symbol_to_string(symbols, item->symbol_id),
			item->scrutinee_term
		);
		if (item->state == PROTOTYPE_RESOLUTION_ITEM_RESOLVED) {
			fprintf(stream, " -> ");
			if (item->resolved_owner < terms->term_count) {
				prototype_term_print_debug(
					stream, symbols, intrinsic_environment, type_declarations,
					terms, item->resolved_owner
				);
			} else {
				fprintf(stream, "<bad-owner:%u>", item->resolved_owner);
			}
			fprintf(stream, ".#%u", item->resolved_id);
		}
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < metadata->resolution_iteration_count; ++i) {
		const struct prototype_resolution_iteration* iteration = &metadata->resolution_iterations[i];
		fprintf(
			stream,
			"resolution iter=%u unresolved=%zu->%zu events=%zu\n",
			iteration->iteration,
			iteration->unresolved_before,
			iteration->unresolved_after,
			iteration->event_count
		);
		for (size_t j = 0; j < iteration->event_count; ++j) {
			const struct prototype_resolution_event* event =
				&metadata->resolution_events[iteration->event_start + j];
			fprintf(
				stream,
				"resolution-event iter=%u item#%u kind=%s %s->%s ast#%u case#%u @%s scrutinee=term#%u -> ",
				event->iteration,
				event->item_id,
				resolution_event_kind_name(event->kind),
				resolution_item_state_name(event->from_state),
				resolution_item_state_name(event->to_state),
				event->ast,
				event->case_index,
				symbol_to_string(symbols, event->symbol_id),
				event->scrutinee_term
			);
			if (event->resolved_owner < terms->term_count) {
				prototype_term_print_debug(
					stream, symbols, intrinsic_environment, type_declarations,
					terms, event->resolved_owner
				);
			} else {
				fprintf(stream, "<bad-owner:%u>", event->resolved_owner);
			}
			fprintf(stream, ".#%u\n", event->resolved_id);
		}
	}
}

void prototype_diagnostic_print_type_namespace(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_type_declaration* type
) {
	if (type->parameter_count == 0) {
		fprintf(stream, "%s", symbol_to_string(symbols, type->name_symbol_id));
		return;
	}
	fprintf(stream, "(%s", symbol_to_string(symbols, type->name_symbol_id));
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->readback.parameter_declarations[type->first_parameter + i];
		fprintf(stream, " %s", symbol_to_string(symbols, parameter->name_symbol_id));
	}
	fprintf(stream, ")");
}

static void print_type_expr(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* types,
	uint32_t type_expr,
	int print_primitive_type_exprs
) {
	if (type_expr == PROTOTYPE_INVALID_ID) {
		fprintf(stream, "UNSPECIFIED_TYPE");
		return;
	}
	if (type_expr >= types->readback.expr_count) {
		fprintf(stream, "BAD_TYPE(%u)", type_expr);
		return;
	}
	const struct prototype_type_expr* expr = &types->readback.exprs[type_expr];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			fprintf(stream, "TYPE(%u)", expr->as.universe.level);
			break;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			fprintf(stream, "TYPE(?u%u)", expr->as.universe_var.level_var);
			break;
		case PROTOTYPE_TYPE_EXPR_SELF:
			fprintf(stream, "SELF");
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			fprintf(stream, "VAR(%s#%u)", symbol_to_string(symbols, expr->as.var.symbol_id), expr->as.var.binding_id);
			break;
		case PROTOTYPE_TYPE_EXPR_NAME:
			fprintf(stream, "CONST(%s)", symbol_to_string(symbols, expr->as.name.symbol_id));
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			fprintf(stream, print_primitive_type_exprs ? "PRIMITIVE(Text)" : "UNKNOWN_TYPE");
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			fprintf(stream, print_primitive_type_exprs ? "PRIMITIVE(Int)" : "UNKNOWN_TYPE");
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
			fprintf(stream, print_primitive_type_exprs ? "PRIMITIVE(Int64)" : "UNKNOWN_TYPE");
			break;
		case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
			fprintf(stream, "IMPORTED_TYPE(%s)", symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id));
			break;
		case PROTOTYPE_TYPE_EXPR_APP:
			fprintf(stream, "APP(");
			print_type_expr(stream, symbols, types, expr->as.app.function, print_primitive_type_exprs);
			fprintf(stream, ", ");
			print_type_expr(stream, symbols, types, expr->as.app.argument, print_primitive_type_exprs);
			fprintf(stream, ")");
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			fprintf(stream, "ARROW(");
			print_type_expr(stream, symbols, types, expr->as.arrow.domain, print_primitive_type_exprs);
			fprintf(stream, ", ");
			print_type_expr(stream, symbols, types, expr->as.arrow.codomain, print_primitive_type_exprs);
			fprintf(stream, ")");
			break;
		case PROTOTYPE_TYPE_EXPR_PI:
			fprintf(stream, "PI(%s#%u : ", symbol_to_string(symbols, expr->as.pi.symbol_id), expr->as.pi.binding_id);
			print_type_expr(stream, symbols, types, expr->as.pi.domain, print_primitive_type_exprs);
			fprintf(stream, ", ");
			print_type_expr(stream, symbols, types, expr->as.pi.codomain, print_primitive_type_exprs);
			fprintf(stream, ")");
			break;
		case PROTOTYPE_TYPE_EXPR_COMPUTATION_REFERENCE:
			fprintf(stream, "COMPUTATION_REFERENCE(");
			print_type_expr(
				stream, symbols, types,
				expr->as.computation_reference.result,
				print_primitive_type_exprs
			);
			fprintf(stream, ")");
			break;
		case PROTOTYPE_TYPE_EXPR_SEMANTIC_RELATION:
			fprintf(stream, "SEMANTIC_RELATION");
			break;
		default:
			fprintf(stream, "UNKNOWN_TYPE");
			break;
	}
}

void prototype_diagnostic_print_type_declaration(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* types,
	const struct prototype_type_declaration* type
) {
	fprintf(stream, "type ");
	prototype_diagnostic_print_type_namespace(stream, symbols, types, type);
	fprintf(stream, " constructors=%u\n", type->constructor_count);
}

static void print_universe_level(FILE* stream, uint32_t level_var) {
	if ((level_var & 0x80000000u) != 0) {
		fprintf(stream, "level(term#%u)", level_var & ~0x80000000u);
	} else {
		fprintf(stream, "?u%u", level_var);
	}
}

void prototype_diagnostic_print_universe_graph(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* types,
	const struct prototype_universe_db* universe,
	int print_primitive_type_exprs
) {
	fprintf(stream, "universe-graph nodes=%zu edges=%zu\n", universe->node_count, universe->edge_count);
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		fprintf(stream, "universe-node #%zu ", i);
		if (node->tag == PROTOTYPE_UNIVERSE_NODE_TYPE) {
			fprintf(stream, "type ");
			if (node->type_id < types->semantic_schema.type_count) {
				prototype_diagnostic_print_type_namespace(stream, symbols, types,
					&types->semantic_schema.type_declarations[node->type_id]);
			} else {
				fprintf(stream, "<bad-type:%u>", node->type_id);
			}
		} else if (node->tag == PROTOTYPE_UNIVERSE_NODE_PARAMETER) {
			fprintf(stream, "parameter %s : ", symbol_to_string(symbols, node->symbol_id));
			print_type_expr(
				stream,
				symbols,
				types,
				node->type_expr,
				print_primitive_type_exprs
			);
		} else {
			fprintf(stream, "<unknown>");
		}
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		const char* tag = edge->tag == PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE ? "parameter-to-type" : "unknown";
		fprintf(stream, "universe-edge #%zu %s #%u -> #%u\n", i, tag, edge->from_node, edge->to_node);
	}
	fprintf(stream, "universe-levels=%zu universe-constraints=%zu solved=%s\n", universe->level_count, universe->constraint_count, universe->solved ? "yes" : "no");
	for (size_t i = 0; i < universe->level_count; ++i) {
		fprintf(stream, "universe-level ");
		print_universe_level(stream, universe->levels[i].level_var);
		fprintf(stream, " = %d\n", universe->levels[i].value);
	}
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		fprintf(stream, "universe-constraint #%zu ", i);
		print_universe_level(stream, constraint->lower_level_var);
		fprintf(stream, " + %d <= ", constraint->offset);
		print_universe_level(stream, constraint->upper_level_var);
		fprintf(
			stream,
			" subject=term#%u classifier=term#%u reason=%d source-claim=%u authority=%d:%u source=term#%u:term#%u\n",
			constraint->subject,
			constraint->classifier,
			constraint->reason,
			constraint->source_claim_id,
			constraint->source_authority_kind,
			constraint->source_authority_id,
			constraint->source_subject,
			constraint->source_classifier
		);
	}
}
