#include "reader.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "symbol.h"
#include "judgement.h"
#include "universe.h"

#define SYMBOL_MAP_CAPACITY 1024
#define SYMBOL_STORAGE_CAPACITY 512
#define TYPE_CAPACITY 64
#define CONSTRUCTOR_CAPACITY 256
#define PARAMETER_CAPACITY 128
#define FIELD_TYPE_CAPACITY 512
#define TYPE_EXPR_CAPACITY 1024
#define AST_CAPACITY 1024
#define AST_DEF_CAPACITY 256
#define AST_MATCH_CASE_CAPACITY 256
#define AST_MATCH_BINDER_CAPACITY 512
#define AST_TYPE_EXPR_CAPACITY 1024
#define AST_TYPE_DEF_CAPACITY 64
#define AST_TYPE_PARAMETER_CAPACITY 128
#define AST_TYPE_CONSTRUCTOR_CAPACITY 256
#define AST_TYPE_FIELD_EXPR_CAPACITY 512
#define UNIVERSE_NODE_CAPACITY 256
#define UNIVERSE_EDGE_CAPACITY 512
#define UNIVERSE_LEVEL_CAPACITY 1024
#define UNIVERSE_CONSTRAINT_CAPACITY 4096
#define TERM_CAPACITY 262144
#define MATCH_CASE_CAPACITY 262144
#define MATCH_BINDER_CAPACITY 262144
#define MATCH_FRAME_CAPACITY 256
#define JUDGEMENT_CAPACITY 4096
#define COMPILE_LABEL_CAPACITY 512
#define COMPILE_TYPE_EXPORT_CAPACITY 256
#define COMPILE_CONSTRUCTOR_EXPORT_CAPACITY 512
#define RESOLVE_ERROR_CAPACITY 512
#define RESOLUTION_ITEM_CAPACITY 2048
#define RESOLUTION_ITERATION_CAPACITY 128
#define RESOLUTION_EVENT_CAPACITY 2048
#define OPERATION_CAPACITY 4096
#define OPERATION_CASE_CAPACITY 4096
#define ARTIFACT_TERM_EXPORT_CAPACITY 512
#define ARTIFACT_TYPE_EXPORT_CAPACITY 256
#define ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY 512
#define ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY 512
#define ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY 1024
#define ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY 2048
#define ARTIFACT_DEPENDENCY_CAPACITY 512
#define ARTIFACT_EXTERNAL_TERM_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_EXTERNAL_TERM_REF_CAPACITY 512
#define ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY 1024
#define ARTIFACT_DEBUG_NAME_CAPACITY 1024
#define LINK_PROVIDER_CAPACITY 16
#define LINK_SEARCH_DIR_CAPACITY 8
#define LINK_AUTO_PROVIDER_PATH_CAPACITY 512
#define IMPORT_INTERFACE_CAPACITY 8
#define OPAQUE_EXPORT_CAPACITY 128
#define ARTIFACT_DEFINITION_CAPACITY 512

static int symbol_ids[SYMBOL_MAP_CAPACITY];
static uint32_t symbol_hashes[SYMBOL_MAP_CAPACITY];
static char* symbol_strings[SYMBOL_STORAGE_CAPACITY];

static struct prototype_type_declaration type_declaration_storage[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declaration_storage[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declaration_storage[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_ast_node ast_nodes[AST_CAPACITY];
static struct prototype_ast_type_expectation_def ast_expectations[AST_DEF_CAPACITY];
static struct prototype_ast_term_assignment_def ast_assignments[AST_DEF_CAPACITY];
static struct prototype_ast_import_def ast_imports[AST_DEF_CAPACITY];
static struct prototype_ast_def_open_address_entry ast_def_index[AST_DEF_CAPACITY];
static struct prototype_ast_match_case ast_match_cases[AST_MATCH_CASE_CAPACITY];
static struct prototype_ast_binder ast_match_binders[AST_MATCH_BINDER_CAPACITY];
static struct prototype_ast_type_expr ast_type_exprs[AST_TYPE_EXPR_CAPACITY];
static struct prototype_ast_type_def ast_type_defs[AST_TYPE_DEF_CAPACITY];
static struct prototype_ast_type_parameter ast_type_parameters[AST_TYPE_PARAMETER_CAPACITY];
static struct prototype_ast_type_constructor ast_type_constructors[AST_TYPE_CONSTRUCTOR_CAPACITY];
static uint32_t ast_type_field_exprs[AST_TYPE_FIELD_EXPR_CAPACITY];
static uint32_t ast_type_field_binder_ids[AST_TYPE_FIELD_EXPR_CAPACITY];
static int ast_type_field_name_symbol_ids[AST_TYPE_FIELD_EXPR_CAPACITY];
static struct prototype_universe_node universe_nodes[UNIVERSE_NODE_CAPACITY];
static struct prototype_universe_edge universe_edges[UNIVERSE_EDGE_CAPACITY];
static struct prototype_universe_level universe_levels[UNIVERSE_LEVEL_CAPACITY];
static struct prototype_universe_constraint universe_constraints[UNIVERSE_CONSTRAINT_CAPACITY];
static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case match_cases[MATCH_CASE_CAPACITY];
static int match_case_label_symbols[MATCH_CASE_CAPACITY];
static struct prototype_case_binder match_binders[MATCH_BINDER_CAPACITY];
static struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];
static struct prototype_judgement_relation judgements[JUDGEMENT_CAPACITY];
static struct prototype_judgement_proof judgement_proofs[JUDGEMENT_CAPACITY];
static struct prototype_compile_label compile_labels[COMPILE_LABEL_CAPACITY];
static struct prototype_compile_type_export compile_type_exports[COMPILE_TYPE_EXPORT_CAPACITY];
static struct prototype_compile_constructor_export compile_constructor_exports[COMPILE_CONSTRUCTOR_EXPORT_CAPACITY];
static struct prototype_resolve_error resolve_errors[RESOLVE_ERROR_CAPACITY];
static struct prototype_resolution_item resolution_items[RESOLUTION_ITEM_CAPACITY];
static struct prototype_resolution_iteration resolution_iterations[RESOLUTION_ITERATION_CAPACITY];
static struct prototype_resolution_event resolution_events[RESOLUTION_EVENT_CAPACITY];
static struct prototype_operation_node operations[OPERATION_CAPACITY];
static struct prototype_operation_match_case operation_cases[OPERATION_CASE_CAPACITY];
static struct prototype_artifact_term_export artifact_term_exports[ARTIFACT_TERM_EXPORT_CAPACITY];
static struct prototype_artifact_type_export artifact_type_exports[ARTIFACT_TYPE_EXPORT_CAPACITY];
static struct prototype_artifact_type_parameter_export artifact_type_parameter_exports[ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY];
static struct prototype_artifact_constructor_export artifact_constructor_exports[ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY];
static uint32_t artifact_constructor_field_type_exprs[ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY];
static struct prototype_type_expr artifact_interface_type_exprs[ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY];
static struct prototype_artifact_dependency artifact_dependencies[ARTIFACT_DEPENDENCY_CAPACITY];
static struct prototype_artifact_external_term_ref artifact_external_term_refs[ARTIFACT_EXTERNAL_TERM_REF_CAPACITY];
static struct prototype_artifact_resolved_external_term_ref artifact_resolved_external_term_refs[ARTIFACT_RESOLVED_EXTERNAL_TERM_REF_CAPACITY];
static struct prototype_artifact_external_type_expr_ref artifact_external_type_expr_refs[ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY];
static struct prototype_artifact_resolved_external_type_expr_ref artifact_resolved_external_type_expr_refs[ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY];
static struct prototype_artifact_external_type_former_ref artifact_external_type_former_refs[ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY];
static struct prototype_artifact_resolved_external_type_former_ref artifact_resolved_external_type_former_refs[ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY];
static struct prototype_artifact_resolved_constructor_owner_ref artifact_resolved_constructor_owner_refs[ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY];
static struct prototype_artifact_debug_term_name artifact_debug_term_names[ARTIFACT_DEBUG_NAME_CAPACITY];
static struct prototype_artifact_debug_type_name artifact_debug_type_names[ARTIFACT_DEBUG_NAME_CAPACITY];
static struct prototype_artifact_debug_constructor_name artifact_debug_constructor_names[ARTIFACT_DEBUG_NAME_CAPACITY];
static struct prototype_term_definition artifact_definitions[ARTIFACT_DEFINITION_CAPACITY];
static struct prototype_type_declaration provider_type_declaration_storage[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration provider_constructor_declaration_storage[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration provider_parameter_declaration_storage[PARAMETER_CAPACITY];
static uint32_t provider_field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr provider_type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_term provider_terms[TERM_CAPACITY];
static struct prototype_match_case provider_match_cases[MATCH_CASE_CAPACITY];
static int provider_match_case_label_symbols[MATCH_CASE_CAPACITY];
static struct prototype_case_binder provider_match_binders[MATCH_BINDER_CAPACITY];
static struct prototype_match_frame provider_match_frames[MATCH_FRAME_CAPACITY];
static struct prototype_judgement_relation provider_judgements[JUDGEMENT_CAPACITY];
static struct prototype_judgement_proof provider_judgement_proofs[JUDGEMENT_CAPACITY];
static struct prototype_artifact_term_export provider_artifact_term_exports[ARTIFACT_TERM_EXPORT_CAPACITY];
static struct prototype_artifact_type_export provider_artifact_type_exports[ARTIFACT_TYPE_EXPORT_CAPACITY];
static struct prototype_artifact_type_parameter_export provider_artifact_type_parameter_exports[ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY];
static struct prototype_artifact_constructor_export provider_artifact_constructor_exports[ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY];
static uint32_t provider_artifact_constructor_field_type_exprs[ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY];
static struct prototype_type_expr provider_artifact_interface_type_exprs[ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY];
static struct prototype_artifact_dependency provider_artifact_dependencies[ARTIFACT_DEPENDENCY_CAPACITY];
static struct prototype_artifact_term_export appended_artifact_term_exports[ARTIFACT_TERM_EXPORT_CAPACITY];
static struct prototype_artifact_type_export appended_artifact_type_exports[ARTIFACT_TYPE_EXPORT_CAPACITY];
static struct prototype_artifact_type_parameter_export appended_artifact_type_parameter_exports[ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY];
static struct prototype_artifact_constructor_export appended_artifact_constructor_exports[ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY];
static uint32_t appended_artifact_constructor_field_type_exprs[ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY];
static struct prototype_type_expr appended_artifact_interface_type_exprs[ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY];
static struct prototype_artifact_dependency appended_artifact_dependencies[ARTIFACT_DEPENDENCY_CAPACITY];
static struct prototype_artifact_interface imported_artifact_interfaces[IMPORT_INTERFACE_CAPACITY];
static struct prototype_artifact_term_export imported_artifact_term_exports[IMPORT_INTERFACE_CAPACITY][ARTIFACT_TERM_EXPORT_CAPACITY];
static struct prototype_artifact_type_export imported_artifact_type_exports[IMPORT_INTERFACE_CAPACITY][ARTIFACT_TYPE_EXPORT_CAPACITY];
static struct prototype_artifact_type_parameter_export imported_artifact_type_parameter_exports[IMPORT_INTERFACE_CAPACITY][ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY];
static struct prototype_artifact_constructor_export imported_artifact_constructor_exports[IMPORT_INTERFACE_CAPACITY][ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY];
static uint32_t imported_artifact_constructor_field_type_exprs[IMPORT_INTERFACE_CAPACITY][ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY];
static struct prototype_type_expr imported_artifact_interface_type_exprs[IMPORT_INTERFACE_CAPACITY][ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY];
static struct prototype_artifact_dependency imported_artifact_dependencies[IMPORT_INTERFACE_CAPACITY][ARTIFACT_DEPENDENCY_CAPACITY];
static char auto_link_provider_paths[LINK_PROVIDER_CAPACITY][LINK_AUTO_PROVIDER_PATH_CAPACITY];
static unsigned char reachable_external_refs[TERM_CAPACITY];

static const char* path_basename(const char* path) {
	const char* base = path;
	if (!path) {
		return "";
	}
	for (const char* p = path; *p; ++p) {
		if (*p == '/') {
			base = p + 1;
		}
	}
	return base;
}

static int namespace_symbol_from_text(
	struct symbol_table* symbols,
	const char* name
) {
	if (!symbols || !name || !*name) {
		return -1;
	}
	return symbol_intern(symbols, name, strlen(name));
}

static int namespace_symbol_from_path(
	struct symbol_table* symbols,
	const char* path
) {
	if (!symbols || !path || !*path) {
		return -1;
	}
	char buffer[256];
	size_t len = 0;
	const char* base = path_basename(path);
	while (base[len] && base[len] != '.' && len + 1 < sizeof(buffer)) {
		buffer[len] = base[len];
		len++;
	}
	if (len == 0) {
		return -1;
	}
	buffer[len] = '\0';
	return symbol_intern(symbols, buffer, len);
}

static int mark_opaque_export(
	struct symbol_table* symbols,
	struct prototype_artifact_interface* interface,
	const char* name
) {
	if (!symbols || !interface || !name) {
		return -1;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	uint32_t export_id;
	int found = prototype_artifact_interface_find_term_export(
		interface,
		symbol_id,
		&export_id
	);
	if (found != 0) {
		return -1;
	}
	interface->term_exports[export_id].transparency =
		PROTOTYPE_ARTIFACT_EXPORT_OPAQUE;
	return 0;
}

static void mark_reachable_external_refs(
	const struct prototype_term_db* term_db,
	uint32_t term_id,
	unsigned depth
) {
	if (!term_db || term_id >= term_db->term_count || depth > 256) {
		return;
	}
	const struct prototype_term* term = &term_db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			reachable_external_refs[term_id] = 1;
			break;
		case PROTOTYPE_TERM_APP:
			mark_reachable_external_refs(term_db, term->as.app.function, depth + 1);
			mark_reachable_external_refs(term_db, term->as.app.argument, depth + 1);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			mark_reachable_external_refs(term_db, term->as.lambda.body, depth + 1);
			break;
		case PROTOTYPE_TERM_MATCH:
			mark_reachable_external_refs(term_db, term->as.match.scrutinee, depth + 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id < term_db->case_count) {
					mark_reachable_external_refs(term_db, term_db->cases[case_id].body, depth + 1);
				}
			}
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			mark_reachable_external_refs(term_db, term->as.constructor.owner, depth + 1);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			mark_reachable_external_refs(term_db, term->as.type_view.core, depth + 1);
			mark_reachable_external_refs(term_db, term->as.type_view.source, depth + 1);
			break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			mark_reachable_external_refs(
				term_db,
				term->as.induction_hypothesis.argument,
				depth + 1
			);
			break;
		default:
			break;
	}
}

static const char* resolve_error_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_RESOLVE_ERROR_NAME:
			return "name";
		case PROTOTYPE_RESOLVE_ERROR_NAMESPACE:
			return "namespace";
		case PROTOTYPE_RESOLVE_ERROR_RECURSIVE:
			return "recursive";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_EXPECTATION:
			return "duplicate-expectation";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_ASSIGNMENT:
			return "duplicate-assignment";
		case PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT:
			return "ambiguous-assignment";
		case PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION:
			return "duplicate-definition";
		case PROTOTYPE_RESOLVE_ERROR_COMPILE:
			return "compile";
		default:
			return "unknown";
	}
}

static const char* operation_tag_name(int tag) {
	switch (tag) {
		case PROTOTYPE_OPERATION_ATOM: return "atom";
		case PROTOTYPE_OPERATION_VAR: return "var";
		case PROTOTYPE_OPERATION_NAME: return "name";
		case PROTOTYPE_OPERATION_CONSTRUCTOR: return "constructor";
		case PROTOTYPE_OPERATION_APP: return "app";
		case PROTOTYPE_OPERATION_LAMBDA: return "lambda";
		case PROTOTYPE_OPERATION_MATCH: return "match";
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS: return "induction-hypothesis";
		case PROTOTYPE_OPERATION_ASCRIPTION: return "ascription";
		default: return "unknown";
	}
}

static void print_metadata_resolve_errors(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_compile_metadata* metadata
) {
	if (!metadata || metadata->resolve_error_count == 0) {
		return;
	}
	for (size_t i = 0; i < metadata->resolve_error_count; ++i) {
		const struct prototype_resolve_error* resolve_error = &metadata->resolve_errors[i];
		fprintf(stream, "metadata resolve-error kind=%s name=%s",
			resolve_error_kind_name(resolve_error->kind),
			symbol_to_string(symbols, resolve_error->name_symbol_id));
		if (resolve_error->member_symbol_id >= 0) {
			fprintf(stream, ".%s", symbol_to_string(symbols, resolve_error->member_symbol_id));
		}
		fprintf(
			stream,
			" ast#%u span=%u:%u\n",
			resolve_error->ast,
			resolve_error->span.line,
			resolve_error->span.column
		);
	}
}

static const char* resolution_event_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR:
			return "match-constructor";
		default:
			return "unknown";
	}
}

static const char* resolution_item_state_name(int state) {
	switch (state) {
		case PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED:
			return "unresolved";
		case PROTOTYPE_RESOLUTION_ITEM_RESOLVED:
			return "resolved";
		case PROTOTYPE_RESOLUTION_ITEM_ERROR:
			return "error";
		default:
			return "unknown";
	}
}

static void print_resolution_trace(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata
) {
	if (!metadata) {
		return;
	}
	printf(
		"resolution_items=%zu resolution_iterations=%zu resolution_events=%zu\n",
		metadata->resolution_item_count,
		metadata->resolution_iteration_count,
		metadata->resolution_event_count
	);
	for (size_t i = 0; i < metadata->resolution_item_count; ++i) {
		const struct prototype_resolution_item* item = &metadata->resolution_items[i];
		printf(
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
			printf(" -> ");
			if (item->resolved_owner < term_db->term_count) {
				prototype_term_print_debug(
					stdout,
					symbols,
					type_declarations,
					term_db,
					item->resolved_owner
				);
			} else {
				printf("<bad-owner:%u>", item->resolved_owner);
			}
			printf(".#%u", item->resolved_id);
		}
		printf("\n");
	}
	for (size_t i = 0; i < metadata->resolution_iteration_count; ++i) {
		const struct prototype_resolution_iteration* iteration =
			&metadata->resolution_iterations[i];
		printf(
			"resolution iter=%u unresolved=%zu->%zu events=%zu\n",
			iteration->iteration,
			iteration->unresolved_before,
			iteration->unresolved_after,
			iteration->event_count
		);
		for (size_t j = 0; j < iteration->event_count; ++j) {
			const struct prototype_resolution_event* event =
				&metadata->resolution_events[iteration->event_start + j];
			printf(
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
			if (event->resolved_owner < term_db->term_count) {
				prototype_term_print_debug(
					stdout,
					symbols,
					type_declarations,
					term_db,
					event->resolved_owner
				);
			} else {
				printf("<bad-owner:%u>", event->resolved_owner);
			}
			printf(".#%u\n", event->resolved_id);
		}
	}
}

static void print_type_namespace(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_type_declaration* type
) {
	if (type->parameter_count == 0) {
		printf("%s", symbol_to_string(symbols, type->name_symbol_id));
		return;
	}

	printf("(%s", symbol_to_string(symbols, type->name_symbol_id));
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[type->first_parameter + i];
		printf(" %s", symbol_to_string(symbols, parameter->name_symbol_id));
	}
	printf(")");
}

static void print_type_expr_debug(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr
) {
	if (type_expr == PROTOTYPE_INVALID_ID) {
		printf("UNSPECIFIED_TYPE");
		return;
	}
	if (type_expr >= type_declarations->expr_count) {
		printf("BAD_TYPE(%u)", type_expr);
		return;
	}

	const struct prototype_type_expr* expr = &type_declarations->exprs[type_expr];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			printf("TYPE(%u)", expr->as.universe.level);
			break;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			printf("TYPE(?u%u)", expr->as.universe_var.level_var);
			break;
		case PROTOTYPE_TYPE_EXPR_SELF:
			printf("SELF");
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			printf("VAR(%s#%u)", symbol_to_string(symbols, expr->as.var.symbol_id), expr->as.var.binder_id);
			break;
		case PROTOTYPE_TYPE_EXPR_NAME:
			printf("CONST(%s)", symbol_to_string(symbols, expr->as.name.symbol_id));
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			printf("PRIMITIVE(Text)");
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			printf("PRIMITIVE(Int)");
			break;
		case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
			printf("PRIMITIVE(Int64)");
			break;
		case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
			printf("IMPORTED_TYPE(%s)", symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id));
			break;
		case PROTOTYPE_TYPE_EXPR_APP:
			printf("APP(");
			print_type_expr_debug(symbols, type_declarations, expr->as.app.function);
			printf(", ");
			print_type_expr_debug(symbols, type_declarations, expr->as.app.argument);
			printf(")");
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			printf("ARROW(");
			print_type_expr_debug(symbols, type_declarations, expr->as.arrow.domain);
			printf(", ");
			print_type_expr_debug(symbols, type_declarations, expr->as.arrow.codomain);
			printf(")");
			break;
		default:
			printf("UNKNOWN_TYPE");
			break;
	}
}

static void print_type_declaration(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_type_declaration* type
) {
	printf("type ");
	print_type_namespace(symbols, type_declarations, type);
	printf(" constructors=%u\n", type->constructor_count);
}

static void print_universe_node(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_universe_db* universe_db,
	uint32_t node_id
) {
	if (node_id >= universe_db->node_count) {
		printf("universe-node #%u <bad>\n", node_id);
		return;
	}

	const struct prototype_universe_node* node = &universe_db->nodes[node_id];
	printf("universe-node #%u ", node_id);
	if (node->tag == PROTOTYPE_UNIVERSE_NODE_TYPE) {
		printf("type ");
		if (node->type_id < type_declarations->type_count) {
			print_type_namespace(symbols, type_declarations, &type_declarations->type_declarations[node->type_id]);
		} else {
			printf("<bad-type:%u>", node->type_id);
		}
	} else if (node->tag == PROTOTYPE_UNIVERSE_NODE_PARAMETER) {
		printf("parameter %s : ", symbol_to_string(symbols, node->symbol_id));
		print_type_expr_debug(symbols, type_declarations, node->type_expr);
	} else {
		printf("<unknown>");
	}
	printf("\n");
}

static void print_universe_level_ref(uint32_t level_var) {
	if ((level_var & 0x80000000u) != 0) {
		printf("level(term#%u)", level_var & ~0x80000000u);
	} else {
		printf("?u%u", level_var);
	}
}

static void print_universe_graph(
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_universe_db* universe_db
) {
	printf(
		"universe-graph nodes=%zu edges=%zu\n",
		universe_db->node_count,
		universe_db->edge_count
	);
	for (size_t i = 0; i < universe_db->node_count; ++i) {
		print_universe_node(symbols, type_declarations, universe_db, (uint32_t)i);
	}
	for (size_t i = 0; i < universe_db->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe_db->edges[i];
		const char* tag = edge->tag == PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE
			? "parameter-to-type"
			: "unknown";
		printf("universe-edge #%zu %s #%u -> #%u\n", i, tag, edge->from_node, edge->to_node);
	}
	printf(
		"universe-levels=%zu universe-constraints=%zu solved=%s\n",
		universe_db->level_count,
		universe_db->constraint_count,
		universe_db->solved ? "yes" : "no"
	);
	for (size_t i = 0; i < universe_db->level_count; ++i) {
		const struct prototype_universe_level* level = &universe_db->levels[i];
		printf("universe-level ");
		print_universe_level_ref(level->level_var);
		printf(" = %d\n", level->value);
	}
	for (size_t i = 0; i < universe_db->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe_db->constraints[i];
		printf("universe-constraint #%zu ", i);
		print_universe_level_ref(constraint->lower_level_var);
		printf(" + %d <= ", constraint->offset);
		print_universe_level_ref(constraint->upper_level_var);
		printf(
			" subject=term#%u classifier=term#%u reason=%d\n",
			constraint->subject,
			constraint->classifier,
			constraint->reason_kind
		);
	}
}

static int read_artifact_interface_and_graph(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* artifact_interface,
	struct prototype_term_db* term_db,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement_db,
	struct prototype_universe_db* universe_db
) {
	if (!path || !symbols || !artifact_interface || !term_db ||
		!type_declarations || !judgement_db || !universe_db) {
		return -1;
	}
	FILE* artifact_file = fopen(path, "r");
	if (!artifact_file) {
		return -1;
	}
	int status = 0;
	if (prototype_artifact_read_text_interface(
			artifact_file,
			symbols,
			artifact_interface
		) != 0 ||
		prototype_artifact_read_text_graph(
			artifact_file,
			symbols,
			term_db,
			type_declarations,
			judgement_db
		) != 0 ||
		prototype_artifact_read_text_universe(
			artifact_file,
			universe_db
		) != 0 ||
		prototype_judgement_validate_proofs(
			term_db,
			type_declarations,
			judgement_db
		) != 0) {
		status = -1;
	}
	if (fclose(artifact_file) != 0) {
		status = -1;
	}
	return status;
}

static int read_artifact_interface_only(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* artifact_interface
) {
	if (!path || !symbols || !artifact_interface) {
		return -1;
	}
	FILE* artifact_file = fopen(path, "r");
	if (!artifact_file) {
		return -1;
	}
	int status = prototype_artifact_read_text_interface(
		artifact_file,
		symbols,
		artifact_interface
	);
	if (fclose(artifact_file) != 0) {
		status = -1;
	}
	return status;
}

static int check_export_normalization_equal(
	const char* path,
	const char* name
) {
	if (!path || !name) {
		return -1;
	}
	struct symbol_table symbols;
	struct prototype_artifact_interface artifact_interface;
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement_db;
	struct prototype_universe_db universe_db;
	struct prototype_term_definition_env definition_env;
	symbol_table_init(&symbols, symbol_ids, symbol_hashes, SYMBOL_MAP_CAPACITY, symbol_strings, SYMBOL_STORAGE_CAPACITY);
	prototype_artifact_interface_init(
		&artifact_interface,
		artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_declaration_storage,
		TYPE_CAPACITY,
		constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		parameter_declaration_storage,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		match_cases,
		match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		match_binders,
		MATCH_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_judgement_db_init(&judgement_db, judgements, judgement_proofs, JUDGEMENT_CAPACITY);
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		UNIVERSE_NODE_CAPACITY,
		universe_edges,
		UNIVERSE_EDGE_CAPACITY,
		universe_levels,
		UNIVERSE_LEVEL_CAPACITY,
		universe_constraints,
		UNIVERSE_CONSTRAINT_CAPACITY
	);
	if (read_artifact_interface_and_graph(
			path,
			&symbols,
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db,
			&universe_db
		) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
			&artifact_interface,
			artifact_definitions,
			ARTIFACT_DEFINITION_CAPACITY,
			&definition_env
		) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int symbol_id = symbol_intern(&symbols, name, strlen(name));
	uint32_t export_id;
	if (symbol_id < 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			symbol_id,
			&export_id
		) != 0) {
		fprintf(stderr, "%s: unknown term export: %s\n", path, name);
		symbol_table_free(&symbols);
		return 1;
	}
	uint32_t external_ref;
	int equal = 0;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){
				artifact_interface.term_exports[export_id].namespace_symbol_id,
				symbol_id
			},
			&external_ref
		) != 0 ||
		prototype_term_normalization_equal_with_definitions(
			&term_db,
			&type_declarations,
			&definition_env,
			external_ref,
			artifact_interface.term_exports[export_id].local_term,
			&equal
		) != 0) {
		fprintf(stderr, "%s: failed to check export normalization equality: %s\n", path, name);
		symbol_table_free(&symbols);
		return 1;
	}
	printf("export-normalization-equal %s %s\n", name, equal ? "yes" : "no");
	symbol_table_free(&symbols);
	return 0;
}

static int reduction_options_from_mode(
	const char* mode,
	struct prototype_term_reduction_options* p_options
) {
	if (!mode || !p_options || strcmp(mode, "default") == 0) {
		if (p_options) {
			p_options->flags =
				PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		}
		return p_options ? 0 : -1;
	}
	if (strcmp(mode, "beta") == 0) {
		p_options->flags =
			PROTOTYPE_TERM_REDUCE_BETA | PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	if (strcmp(mode, "match") == 0) {
		p_options->flags =
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION |
			PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	if (strcmp(mode, "none") == 0) {
		p_options->flags = PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	return -1;
}

static int check_exports_normalization_equal(
	const char* path,
	const char* left_name,
	const char* right_name,
	const char* reduction_mode
) {
	if (!path || !left_name || !right_name) {
		return -1;
	}
	struct prototype_term_reduction_options options;
	if (reduction_options_from_mode(reduction_mode, &options) != 0) {
		fprintf(stderr, "unknown reduction mode: %s\n", reduction_mode ? reduction_mode : "<null>");
		return 1;
	}
	struct symbol_table symbols;
	struct prototype_artifact_interface artifact_interface;
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement_db;
	struct prototype_universe_db universe_db;
	struct prototype_term_definition_env definition_env;
	symbol_table_init(&symbols, symbol_ids, symbol_hashes, SYMBOL_MAP_CAPACITY, symbol_strings, SYMBOL_STORAGE_CAPACITY);
	prototype_artifact_interface_init(
		&artifact_interface,
		artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_declaration_storage,
		TYPE_CAPACITY,
		constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		parameter_declaration_storage,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		match_cases,
		match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		match_binders,
		MATCH_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_judgement_db_init(&judgement_db, judgements, judgement_proofs, JUDGEMENT_CAPACITY);
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		UNIVERSE_NODE_CAPACITY,
		universe_edges,
		UNIVERSE_EDGE_CAPACITY,
		universe_levels,
		UNIVERSE_LEVEL_CAPACITY,
		universe_constraints,
		UNIVERSE_CONSTRAINT_CAPACITY
	);
	if (read_artifact_interface_and_graph(
			path,
			&symbols,
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db,
			&universe_db
		) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
			&artifact_interface,
			artifact_definitions,
			ARTIFACT_DEFINITION_CAPACITY,
			&definition_env
		) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int left_symbol = symbol_intern(&symbols, left_name, strlen(left_name));
	int right_symbol = symbol_intern(&symbols, right_name, strlen(right_name));
	uint32_t left_export;
	uint32_t right_export;
	if (left_symbol < 0 || right_symbol < 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			left_symbol,
			&left_export
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			right_symbol,
			&right_export
		) != 0) {
		fprintf(stderr, "%s: unknown term export in normalization equality check\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int equal = 0;
	if (prototype_term_normalization_equal_with_options(
			&term_db,
			&type_declarations,
			&definition_env,
			options,
			artifact_interface.term_exports[left_export].local_term,
			artifact_interface.term_exports[right_export].local_term,
			&equal
		) != 0) {
		fprintf(stderr, "%s: failed to check export normalization equality: %s %s\n",
			path,
			left_name,
			right_name);
		symbol_table_free(&symbols);
		return 1;
	}
	printf("exports-normalization-equal %s %s mode=%s %s\n",
		left_name,
		right_name,
		reduction_mode ? reduction_mode : "default",
		equal ? "yes" : "no");
	symbol_table_free(&symbols);
	return 0;
}

static int check_exports_shape_equal(
	const char* path,
	const char* left_name,
	const char* right_name,
	int core_shape
) {
	if (!path || !left_name || !right_name) {
		return -1;
	}
	struct symbol_table symbols;
	struct prototype_artifact_interface artifact_interface;
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement_db;
	struct prototype_universe_db universe_db;
	symbol_table_init(&symbols, symbol_ids, symbol_hashes, SYMBOL_MAP_CAPACITY, symbol_strings, SYMBOL_STORAGE_CAPACITY);
	prototype_artifact_interface_init(
		&artifact_interface,
		artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_declaration_storage,
		TYPE_CAPACITY,
		constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		parameter_declaration_storage,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		match_cases,
		match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		match_binders,
		MATCH_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_judgement_db_init(&judgement_db, judgements, judgement_proofs, JUDGEMENT_CAPACITY);
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		UNIVERSE_NODE_CAPACITY,
		universe_edges,
		UNIVERSE_EDGE_CAPACITY,
		universe_levels,
		UNIVERSE_LEVEL_CAPACITY,
		universe_constraints,
		UNIVERSE_CONSTRAINT_CAPACITY
	);
	if (read_artifact_interface_and_graph(
			path,
			&symbols,
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db,
			&universe_db
		) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int left_symbol = symbol_intern(&symbols, left_name, strlen(left_name));
	int right_symbol = symbol_intern(&symbols, right_name, strlen(right_name));
	uint32_t left_export;
	uint32_t right_export;
	if (left_symbol < 0 || right_symbol < 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			left_symbol,
			&left_export
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			right_symbol,
			&right_export
		) != 0) {
		fprintf(stderr, "%s: unknown term export in shape equality check\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int equal = 0;
	int status = core_shape ?
		prototype_term_core_shape_equal(
			&term_db,
			artifact_interface.term_exports[left_export].local_term,
			artifact_interface.term_exports[right_export].local_term,
			&equal
		) :
		prototype_term_view_shape_equal(
			&term_db,
			artifact_interface.term_exports[left_export].local_term,
			artifact_interface.term_exports[right_export].local_term,
			&equal
		);
	if (status != 0) {
		fprintf(stderr, "%s: failed to check export shape equality: %s %s\n",
			path,
			left_name,
			right_name);
		symbol_table_free(&symbols);
		return 1;
	}
	printf("exports-%s-shape-equal %s %s %s\n",
		core_shape ? "core" : "view",
		left_name,
		right_name,
		equal ? "yes" : "no");
	symbol_table_free(&symbols);
	return 0;
}

static int check_export_classifier_compatible(
	const char* path,
	const char* expected_name,
	const char* actual_name
) {
	if (!path || !expected_name || !actual_name) {
		return -1;
	}
	struct symbol_table symbols;
	struct prototype_artifact_interface artifact_interface;
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement_db;
	struct prototype_universe_db universe_db;
	struct prototype_term_definition_env definition_env;
	symbol_table_init(&symbols, symbol_ids, symbol_hashes, SYMBOL_MAP_CAPACITY, symbol_strings, SYMBOL_STORAGE_CAPACITY);
	prototype_artifact_interface_init(
		&artifact_interface,
		artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_declaration_storage,
		TYPE_CAPACITY,
		constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		parameter_declaration_storage,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		match_cases,
		match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		match_binders,
		MATCH_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_judgement_db_init(&judgement_db, judgements, judgement_proofs, JUDGEMENT_CAPACITY);
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		UNIVERSE_NODE_CAPACITY,
		universe_edges,
		UNIVERSE_EDGE_CAPACITY,
		universe_levels,
		UNIVERSE_LEVEL_CAPACITY,
		universe_constraints,
		UNIVERSE_CONSTRAINT_CAPACITY
	);
	if (read_artifact_interface_and_graph(
			path,
			&symbols,
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db,
			&universe_db
		) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
			&artifact_interface,
			artifact_definitions,
			ARTIFACT_DEFINITION_CAPACITY,
			&definition_env
		) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int expected_symbol_id = symbol_intern(&symbols, expected_name, strlen(expected_name));
	int actual_symbol_id = symbol_intern(&symbols, actual_name, strlen(actual_name));
	uint32_t expected_export_id;
	uint32_t actual_export_id;
	if (expected_symbol_id < 0 || actual_symbol_id < 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			expected_symbol_id,
			&expected_export_id
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&artifact_interface,
			actual_symbol_id,
			&actual_export_id
		) != 0) {
		fprintf(stderr, "%s: unknown term export in classifier check\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	uint32_t expected_classifier =
		artifact_interface.term_exports[expected_export_id].classifier;
	uint32_t actual_classifier =
		artifact_interface.term_exports[actual_export_id].classifier;
	if (expected_classifier >= term_db.term_count ||
		actual_classifier >= term_db.term_count) {
		fprintf(stderr, "%s: missing classifier in classifier check\n", path);
		symbol_table_free(&symbols);
		return 1;
	}
	int compatible = prototype_judgement_classifier_compatible_with_definitions(
		&term_db,
		&type_declarations,
		&definition_env,
		expected_classifier,
		actual_classifier
	);
	printf("export-classifiers-compatible %s %s %s\n",
		expected_name,
		actual_name,
		compatible ? "yes" : "no");
	symbol_table_free(&symbols);
	return 0;
}

static int artifact_path_has_supported_suffix(const char* path) {
	size_t length;
	if (!path) {
		return 0;
	}
	length = strlen(path);
	if (length >= 3 && strcmp(path + length - 3, ".ao") == 0) {
		return 1;
	}
	if (length >= 4 && strcmp(path + length - 4, ".apo") == 0) {
		return 1;
	}
	return 0;
}

static int interface_exports_symbol(
	const struct prototype_artifact_interface* artifact_interface,
	int symbol_id
) {
	uint32_t export_id;
	if (!artifact_interface || symbol_id < 0) {
		return 0;
	}
	if (prototype_artifact_interface_find_term_export(
			artifact_interface,
			symbol_id,
			&export_id
		) == 0) {
		return 1;
	}
	return prototype_artifact_interface_find_type_export(
		artifact_interface,
		symbol_id,
		&export_id
	) == 0;
}

static int interface_exports_dependency(
	const struct prototype_artifact_interface* artifact_interface,
	const struct prototype_artifact_dependency* dependency
) {
	uint32_t export_id;
	if (!artifact_interface || !dependency || dependency->name_symbol_id < 0) {
		return 0;
	}
	if (dependency->namespace_symbol_id < 0) {
		return interface_exports_symbol(artifact_interface, dependency->name_symbol_id);
	}
	if (prototype_artifact_interface_find_term_export_in_namespace(
			artifact_interface,
			dependency->namespace_symbol_id,
			dependency->name_symbol_id,
			&export_id
		) == 0) {
		return 1;
	}
	return prototype_artifact_interface_find_type_export_in_namespace(
		artifact_interface,
		dependency->namespace_symbol_id,
		dependency->name_symbol_id,
		&export_id
	) == 0;
}

static void init_imported_interface_slot(size_t index) {
	prototype_artifact_interface_init(
		&imported_artifact_interfaces[index],
		imported_artifact_term_exports[index],
		ARTIFACT_TERM_EXPORT_CAPACITY,
		imported_artifact_type_exports[index],
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		imported_artifact_type_parameter_exports[index],
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		imported_artifact_constructor_exports[index],
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		imported_artifact_constructor_field_type_exprs[index],
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		imported_artifact_interface_type_exprs[index],
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		imported_artifact_dependencies[index],
		ARTIFACT_DEPENDENCY_CAPACITY
	);
}

static int build_search_candidate_path(
	char* buffer,
	size_t buffer_size,
	const char* directory,
	const char* filename
);

static int imported_interfaces_export_symbol(
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	int symbol_id
) {
	if (!imported_interfaces || symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < imported_interface_count; ++i) {
		if (interface_exports_symbol(imported_interfaces[i], symbol_id)) {
			return 1;
		}
	}
	return 0;
}

static void init_provider_artifact_storage(
	struct prototype_artifact_interface* interface,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	struct prototype_judgement_db* judgement_db
) {
	prototype_artifact_interface_init(
		interface,
		provider_artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		provider_artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		provider_artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		provider_artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		provider_artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		provider_artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		provider_artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_type_declaration_db_init(
		type_declarations,
		provider_type_declaration_storage,
		TYPE_CAPACITY,
		provider_constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		provider_parameter_declaration_storage,
		PARAMETER_CAPACITY,
		provider_field_types,
		FIELD_TYPE_CAPACITY,
		provider_type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_term_db_init(
		term_db,
		provider_terms,
		TERM_CAPACITY,
		provider_match_cases,
		provider_match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		provider_match_binders,
		MATCH_BINDER_CAPACITY,
		provider_match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_judgement_db_init(
		judgement_db,
		provider_judgements,
		provider_judgement_proofs,
		JUDGEMENT_CAPACITY
	);
}

static int read_import_artifact_into_slot(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_program* program,
	size_t slot,
	struct prototype_universe_db* universe
) {
	if (!path || !symbols || slot >= IMPORT_INTERFACE_CAPACITY ||
		!program || !program->terms || !program->type_declarations ||
		!program->judgement || !universe) {
		return -1;
	}
	struct prototype_artifact_interface provider_interface;
	struct prototype_type_declaration_db provider_type_declarations;
	struct prototype_term_db provider_term_db;
	struct prototype_judgement_db provider_judgement_db;
	init_provider_artifact_storage(
		&provider_interface,
		&provider_type_declarations,
		&provider_term_db,
		&provider_judgement_db
	);
	init_imported_interface_slot(slot);
	if (read_artifact_interface_and_graph(
			path,
			symbols,
			&provider_interface,
			&provider_term_db,
			&provider_type_declarations,
			&provider_judgement_db,
			universe
		) != 0) {
		return -1;
	}
	return prototype_artifact_append_graph(
			&imported_artifact_interfaces[slot],
			program->terms,
			program->type_declarations,
			program->judgement,
			&provider_interface,
			&provider_term_db,
			&provider_type_declarations,
			&provider_judgement_db
	);
}

static int add_source_import_from_search_dirs(
	const struct prototype_ast_import_def* import,
	const char* const* import_search_dirs,
	size_t import_search_dir_count,
	struct symbol_table* symbols,
	struct prototype_program* program,
	const struct prototype_artifact_interface** imported_interface_refs,
	size_t* p_import_interface_count,
	struct prototype_universe_db* universe
) {
	if (!import || !import_search_dirs || !symbols || !program ||
		!imported_interface_refs || !p_import_interface_count || !universe) {
		return -1;
	}
	if (imported_interfaces_export_symbol(
			imported_interface_refs,
			*p_import_interface_count,
			import->name_symbol_id
		)) {
		return 0;
	}
	for (size_t dir_index = 0; dir_index < import_search_dir_count; ++dir_index) {
		DIR* directory = opendir(import_search_dirs[dir_index]);
		if (!directory) {
			return -1;
		}
		struct dirent* entry;
		while ((entry = readdir(directory)) != NULL) {
			char candidate_path[LINK_AUTO_PROVIDER_PATH_CAPACITY];
			if (entry->d_name[0] == '.' ||
				!artifact_path_has_supported_suffix(entry->d_name) ||
				build_search_candidate_path(
					candidate_path,
					sizeof(candidate_path),
					import_search_dirs[dir_index],
					entry->d_name
				) != 0) {
				continue;
			}
			if (*p_import_interface_count >= IMPORT_INTERFACE_CAPACITY) {
				closedir(directory);
				return -1;
			}
			struct prototype_artifact_interface probe_interface;
			prototype_artifact_interface_init(
				&probe_interface,
				provider_artifact_term_exports,
				ARTIFACT_TERM_EXPORT_CAPACITY,
				provider_artifact_type_exports,
				ARTIFACT_TYPE_EXPORT_CAPACITY,
				provider_artifact_type_parameter_exports,
				ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
				provider_artifact_constructor_exports,
				ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
				provider_artifact_constructor_field_type_exprs,
				ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
				provider_artifact_interface_type_exprs,
				ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
				provider_artifact_dependencies,
				ARTIFACT_DEPENDENCY_CAPACITY
			);
			if (read_artifact_interface_only(
					candidate_path,
					symbols,
					&probe_interface
				) != 0) {
				continue;
			}
			if (!interface_exports_symbol(&probe_interface, import->name_symbol_id)) {
				continue;
			}
			size_t slot = *p_import_interface_count;
			if (read_import_artifact_into_slot(
					candidate_path,
					symbols,
					program,
					slot,
					universe
				) != 0) {
				closedir(directory);
				return -1;
			}
			imported_interface_refs[slot] = &imported_artifact_interfaces[slot];
			(*p_import_interface_count)++;
			return 0;
		}
		closedir(directory);
	}
	return 1;
}

static int provider_path_already_added(
	const char* path,
	const char* link_target_path,
	const char* const* link_provider_paths,
	size_t link_provider_count
) {
	if (!path) {
		return 1;
	}
	if (link_target_path && strcmp(path, link_target_path) == 0) {
		return 1;
	}
	for (size_t i = 0; i < link_provider_count; ++i) {
		if (link_provider_paths[i] && strcmp(path, link_provider_paths[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

static int build_search_candidate_path(
	char* buffer,
	size_t buffer_size,
	const char* directory,
	const char* filename
) {
	size_t directory_length;
	if (!buffer || !directory || !filename || buffer_size == 0) {
		return -1;
	}
	directory_length = strlen(directory);
	if (directory_length > 0 && directory[directory_length - 1] == '/') {
		if (snprintf(buffer, buffer_size, "%s%s", directory, filename) >= (int)buffer_size) {
			return -1;
		}
	} else if (snprintf(buffer, buffer_size, "%s/%s", directory, filename) >= (int)buffer_size) {
		return -1;
	}
	return 0;
}

static int count_provider_paths_for_dependency(
	const char* const* link_provider_paths,
	size_t link_provider_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_dependency* dependency,
	struct prototype_artifact_interface* probe_interface
) {
	if (!link_provider_paths || !symbols || !dependency || !probe_interface) {
		return -1;
	}
	int count = 0;
	for (size_t i = 0; i < link_provider_count; ++i) {
		if (!link_provider_paths[i]) {
			continue;
		}
		prototype_artifact_interface_init(
			probe_interface,
			provider_artifact_term_exports,
			ARTIFACT_TERM_EXPORT_CAPACITY,
			provider_artifact_type_exports,
			ARTIFACT_TYPE_EXPORT_CAPACITY,
			provider_artifact_type_parameter_exports,
			ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
			provider_artifact_constructor_exports,
			ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
			provider_artifact_constructor_field_type_exprs,
			ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
			provider_artifact_interface_type_exprs,
			ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
			provider_artifact_dependencies,
			ARTIFACT_DEPENDENCY_CAPACITY
		);
		if (read_artifact_interface_only(
				link_provider_paths[i],
				symbols,
				probe_interface
			) == 0 && interface_exports_dependency(probe_interface, dependency)) {
			count++;
		}
	}
	return count;
}

static int add_provider_from_search_dirs(
	const char** link_provider_paths,
	size_t* p_link_provider_count,
	const char* link_target_path,
	const char* const* link_search_dirs,
	size_t link_search_dir_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* target_interface,
	struct prototype_artifact_interface* probe_interface
) {
	if (!link_provider_paths || !p_link_provider_count || !symbols ||
		!target_interface || !probe_interface) {
		return -1;
	}
	for (size_t dep = 0; dep < target_interface->dependency_count; ++dep) {
		const struct prototype_artifact_dependency* dependency =
			&target_interface->dependencies[dep];
		int found_dependency = interface_exports_dependency(target_interface, dependency);
		if (!found_dependency) {
			int provider_count = count_provider_paths_for_dependency(
				link_provider_paths,
				*p_link_provider_count,
				symbols,
				dependency,
				probe_interface
			);
			if (provider_count < 0 || provider_count > 1) {
				return -1;
			}
			found_dependency = provider_count == 1;
		}
		if (found_dependency) {
			continue;
		}
		char selected_candidate[LINK_AUTO_PROVIDER_PATH_CAPACITY];
		int candidate_count = 0;
		for (size_t dir_index = 0; !found_dependency && dir_index < link_search_dir_count; ++dir_index) {
			DIR* directory = opendir(link_search_dirs[dir_index]);
			if (!directory) {
				return -1;
			}
			struct dirent* entry;
			while ((entry = readdir(directory)) != NULL) {
				char candidate_path[LINK_AUTO_PROVIDER_PATH_CAPACITY];
				if (entry->d_name[0] == '.' ||
					!artifact_path_has_supported_suffix(entry->d_name) ||
					build_search_candidate_path(
						candidate_path,
						sizeof(candidate_path),
						link_search_dirs[dir_index],
						entry->d_name
					) != 0 ||
					provider_path_already_added(
						candidate_path,
						link_target_path,
						link_provider_paths,
						*p_link_provider_count
					)) {
					continue;
				}
				prototype_artifact_interface_init(
					probe_interface,
					provider_artifact_term_exports,
					ARTIFACT_TERM_EXPORT_CAPACITY,
					provider_artifact_type_exports,
					ARTIFACT_TYPE_EXPORT_CAPACITY,
					provider_artifact_type_parameter_exports,
					ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
					provider_artifact_constructor_exports,
					ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
					provider_artifact_constructor_field_type_exprs,
					ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
					provider_artifact_interface_type_exprs,
					ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
					provider_artifact_dependencies,
					ARTIFACT_DEPENDENCY_CAPACITY
				);
				if (read_artifact_interface_only(
						candidate_path,
						symbols,
						probe_interface
					) != 0 ||
					!interface_exports_dependency(probe_interface, dependency)) {
					continue;
				}
				candidate_count++;
				if (candidate_count > 1 || snprintf(
						selected_candidate,
						sizeof(selected_candidate),
						"%s",
						candidate_path
					) >= (int)sizeof(selected_candidate)) {
					closedir(directory);
					return -1;
				}
			}
			closedir(directory);
		}
		if (!found_dependency && candidate_count == 1) {
			if (*p_link_provider_count >= LINK_PROVIDER_CAPACITY ||
				snprintf(
					auto_link_provider_paths[*p_link_provider_count],
					LINK_AUTO_PROVIDER_PATH_CAPACITY,
					"%s",
					selected_candidate
				) >= LINK_AUTO_PROVIDER_PATH_CAPACITY) {
				return -1;
			}
			link_provider_paths[*p_link_provider_count] =
				auto_link_provider_paths[*p_link_provider_count];
			(*p_link_provider_count)++;
			printf("found provider for %s: %s\n",
				symbol_to_string(symbols, dependency->name_symbol_id),
				selected_candidate);
		}
	}
	return 0;
}

static int read_provider_interface_for_ordering(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* artifact_interface,
	struct prototype_artifact_term_export* term_exports,
	struct prototype_artifact_type_export* type_exports,
	struct prototype_artifact_type_parameter_export* type_parameters,
	struct prototype_artifact_constructor_export* constructor_exports,
	uint32_t* constructor_field_type_exprs,
	struct prototype_type_expr* interface_type_exprs,
	struct prototype_artifact_dependency* dependencies
);

static int add_provider_closure_from_search_dirs(
	const char** link_provider_paths,
	size_t* p_link_provider_count,
	const char* link_target_path,
	const char* const* link_search_dirs,
	size_t link_search_dir_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* target_interface,
	struct prototype_artifact_interface* provider_interface,
	struct prototype_artifact_interface* probe_interface
) {
	if (!link_provider_paths || !p_link_provider_count || !symbols ||
		!target_interface || !provider_interface || !probe_interface) {
		return -1;
	}
	if (add_provider_from_search_dirs(
			link_provider_paths,
			p_link_provider_count,
			link_target_path,
			link_search_dirs,
			link_search_dir_count,
			symbols,
			target_interface,
			probe_interface
		) != 0) {
		return -1;
	}

	size_t scanned_provider_count = 0;
	while (scanned_provider_count < *p_link_provider_count) {
		if (read_provider_interface_for_ordering(
				link_provider_paths[scanned_provider_count],
				symbols,
				provider_interface,
				provider_artifact_term_exports,
				provider_artifact_type_exports,
				provider_artifact_type_parameter_exports,
				provider_artifact_constructor_exports,
				provider_artifact_constructor_field_type_exprs,
				provider_artifact_interface_type_exprs,
				provider_artifact_dependencies
			) != 0) {
			return -1;
		}
		if (add_provider_from_search_dirs(
				link_provider_paths,
				p_link_provider_count,
				link_target_path,
				link_search_dirs,
				link_search_dir_count,
				symbols,
				provider_interface,
				probe_interface
			) != 0) {
			return -1;
		}
		scanned_provider_count++;
	}
	return 0;
}

static int read_provider_interface_for_ordering(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* artifact_interface,
	struct prototype_artifact_term_export* term_exports,
	struct prototype_artifact_type_export* type_exports,
	struct prototype_artifact_type_parameter_export* type_parameters,
	struct prototype_artifact_constructor_export* constructor_exports,
	uint32_t* constructor_field_type_exprs,
	struct prototype_type_expr* interface_type_exprs,
	struct prototype_artifact_dependency* dependencies
) {
	prototype_artifact_interface_init(
		artifact_interface,
		term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		type_parameters,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	return read_artifact_interface_only(path, symbols, artifact_interface);
}

static int provider_depends_on_provider(
	const struct prototype_artifact_interface* dependent,
	const struct prototype_artifact_interface* provider
) {
	if (!dependent || !provider) {
		return 0;
	}
	for (size_t i = 0; i < dependent->dependency_count; ++i) {
		if (interface_exports_dependency(provider, &dependent->dependencies[i])) {
			return 1;
		}
	}
	return 0;
}

static int order_link_providers_by_interface_dependencies(
	const char** link_provider_paths,
	size_t link_provider_count,
	struct symbol_table* symbols
) {
	if (!link_provider_paths || !symbols) {
		return -1;
	}
	for (size_t pass = 0; pass < link_provider_count; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < link_provider_count; ++i) {
			for (size_t j = i + 1; j < link_provider_count; ++j) {
				struct prototype_artifact_interface left;
				struct prototype_artifact_interface right;
				if (read_provider_interface_for_ordering(
						link_provider_paths[i],
						symbols,
						&left,
						provider_artifact_term_exports,
						provider_artifact_type_exports,
						provider_artifact_type_parameter_exports,
						provider_artifact_constructor_exports,
						provider_artifact_constructor_field_type_exprs,
						provider_artifact_interface_type_exprs,
						provider_artifact_dependencies
					) != 0 ||
					read_provider_interface_for_ordering(
						link_provider_paths[j],
						symbols,
						&right,
						appended_artifact_term_exports,
						appended_artifact_type_exports,
						appended_artifact_type_parameter_exports,
						appended_artifact_constructor_exports,
						appended_artifact_constructor_field_type_exprs,
						appended_artifact_interface_type_exprs,
						appended_artifact_dependencies
					) != 0) {
					return -1;
				}
				if (provider_depends_on_provider(&left, &right)) {
					const char* tmp = link_provider_paths[i];
					link_provider_paths[i] = link_provider_paths[j];
					link_provider_paths[j] = tmp;
					changed = 1;
				}
			}
		}
		if (!changed) {
			return 0;
		}
	}
	return 0;
}

static uint32_t offset_optional_id(uint32_t id, uint32_t offset) {
	return id == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : id + offset;
}

static int interface_type_expr_present(const struct prototype_type_expr* expr) {
	return expr && expr->tag != 0;
}

static int interface_type_parameter_present(
	const struct prototype_artifact_type_parameter_export* parameter
) {
	return parameter && parameter->binder_id != PROTOTYPE_INVALID_ID;
}

static int interface_field_ref_present(const uint32_t* field_ref) {
	return field_ref && *field_ref != PROTOTYPE_INVALID_ID;
}

static int reexport_appended_interface(
	struct prototype_artifact_interface* target,
	const struct prototype_artifact_interface* appended
) {
	if (!target || !appended ||
		appended->type_export_count > ARTIFACT_TYPE_EXPORT_CAPACITY ||
		target->type_expr_count + appended->type_expr_count > target->type_expr_capacity ||
		target->type_parameter_count + appended->type_parameter_count >
			target->type_parameter_capacity ||
		target->constructor_field_type_expr_count +
			appended->constructor_field_type_expr_count >
			target->constructor_field_type_expr_capacity) {
		return -1;
	}

	uint32_t type_expr_offset = (uint32_t)target->type_expr_count;
	uint32_t type_parameter_offset = (uint32_t)target->type_parameter_count;
	uint32_t constructor_field_offset =
		(uint32_t)target->constructor_field_type_expr_count;
	uint32_t universe_offset = prototype_artifact_interface_next_universe_var(target);
	for (size_t i = 0; i < appended->type_expr_count; ++i) {
		struct prototype_type_expr expr = appended->type_exprs[i];
		if (interface_type_expr_present(&expr)) {
			switch (expr.tag) {
				case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
					expr.as.universe_var.level_var += universe_offset;
					break;
				case PROTOTYPE_TYPE_EXPR_APP:
					expr.as.app.function =
						offset_optional_id(expr.as.app.function, type_expr_offset);
					expr.as.app.argument =
						offset_optional_id(expr.as.app.argument, type_expr_offset);
					break;
				case PROTOTYPE_TYPE_EXPR_ARROW:
					expr.as.arrow.domain =
						offset_optional_id(expr.as.arrow.domain, type_expr_offset);
					expr.as.arrow.codomain =
						offset_optional_id(expr.as.arrow.codomain, type_expr_offset);
					break;
				default:
					break;
			}
		}
		target->type_exprs[target->type_expr_count++] = expr;
	}
	for (size_t i = 0; i < appended->type_parameter_count; ++i) {
		target->type_parameters[target->type_parameter_count] =
			appended->type_parameters[i];
		if (interface_type_parameter_present(
				&target->type_parameters[target->type_parameter_count]
			)) {
			target->type_parameters[target->type_parameter_count].type_expr =
				offset_optional_id(
					target->type_parameters[target->type_parameter_count].type_expr,
					type_expr_offset
				);
		}
		target->type_parameter_count++;
	}
	for (size_t i = 0; i < appended->constructor_field_type_expr_count; ++i) {
		uint32_t field_ref = appended->constructor_field_type_exprs[i];
		if (interface_field_ref_present(&field_ref)) {
			field_ref = offset_optional_id(field_ref, type_expr_offset);
		}
		target->constructor_field_type_exprs[
			target->constructor_field_type_expr_count++
		] = field_ref;
	}

	uint32_t type_index_map[ARTIFACT_TYPE_EXPORT_CAPACITY];
	int type_added[ARTIFACT_TYPE_EXPORT_CAPACITY];
	for (size_t i = 0; i < ARTIFACT_TYPE_EXPORT_CAPACITY; ++i) {
		type_index_map[i] = PROTOTYPE_INVALID_ID;
		type_added[i] = 0;
	}

	for (size_t i = 0; i < appended->term_export_count; ++i) {
		uint32_t existing;
		int found = prototype_artifact_interface_find_term_export_in_namespace(
			target,
			appended->term_exports[i].namespace_symbol_id,
			appended->term_exports[i].name_symbol_id,
			&existing
		);
		if (found < 0) {
			return -1;
		}
		if (found == 0) {
			continue;
		}
		if (target->term_export_count >= target->term_export_capacity) {
			return -1;
		}
		target->term_exports[target->term_export_count++] = appended->term_exports[i];
	}

	for (size_t i = 0; i < appended->type_export_count; ++i) {
		uint32_t existing;
		int found = prototype_artifact_interface_find_type_export_in_namespace(
			target,
			appended->type_exports[i].namespace_symbol_id,
			appended->type_exports[i].name_symbol_id,
			&existing
		);
		if (found < 0) {
			return -1;
		}
		if (found == 0) {
			type_index_map[i] = existing;
			continue;
		}
		if (target->type_export_count >= target->type_export_capacity) {
			return -1;
		}
		uint32_t target_type_index = (uint32_t)target->type_export_count;
		target->type_exports[target->type_export_count] = appended->type_exports[i];
		target->type_exports[target->type_export_count].first_parameter +=
			type_parameter_offset;
		target->type_exports[target->type_export_count].first_constructor_export =
			PROTOTYPE_INVALID_ID;
		target->type_exports[target->type_export_count].constructor_count = 0;
		target->type_export_count++;
		type_index_map[i] = target_type_index;
		type_added[i] = 1;
	}

	for (size_t i = 0; i < appended->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* constructor =
			&appended->constructor_exports[i];
		if (constructor->type_export_index >= ARTIFACT_TYPE_EXPORT_CAPACITY ||
			constructor->type_export_index >= appended->type_export_count ||
			!type_added[constructor->type_export_index]) {
			continue;
		}
		uint32_t target_type_index = type_index_map[constructor->type_export_index];
		if (target_type_index == PROTOTYPE_INVALID_ID ||
			target_type_index >= target->type_export_count ||
			target->constructor_export_count >= target->constructor_export_capacity) {
			return -1;
		}
		struct prototype_artifact_type_export* type_export =
			&target->type_exports[target_type_index];
		if (type_export->constructor_count == 0) {
			type_export->first_constructor_export =
				(uint32_t)target->constructor_export_count;
		}
		target->constructor_exports[target->constructor_export_count] = *constructor;
		target->constructor_exports[target->constructor_export_count].type_export_index =
			target_type_index;
		target->constructor_exports[target->constructor_export_count].readback_first_field_type +=
			constructor_field_offset;
		target->constructor_export_count++;
		type_export->constructor_count++;
	}

	for (size_t i = 0; i < target->type_export_count; ++i) {
		if (target->type_exports[i].constructor_count == 0 &&
			target->type_exports[i].first_constructor_export == PROTOTYPE_INVALID_ID) {
			target->type_exports[i].first_constructor_export =
				(uint32_t)target->constructor_export_count;
		}
	}
	return 0;
}

int main(int argc, char** argv) {
	struct prototype_read_options read_options;
	int file_arg = 1;
	const char* artifact_output_path = NULL;
	const char* interface_input_path = NULL;
	const char* check_export_normalization_equal_path = NULL;
	const char* check_export_normalization_equal_name = NULL;
	const char* check_exports_normalization_equal_path = NULL;
	const char* check_exports_normalization_equal_left_name = NULL;
	const char* check_exports_normalization_equal_right_name = NULL;
	const char* reduction_mode = "default";
	const char* check_exports_shape_equal_path = NULL;
	const char* check_exports_shape_equal_left_name = NULL;
	const char* check_exports_shape_equal_right_name = NULL;
	int check_exports_shape_equal_core = 0;
	const char* check_classifier_path = NULL;
	const char* check_classifier_expected_name = NULL;
	const char* check_classifier_actual_name = NULL;
	const char* link_target_path = NULL;
	const char* link_provider_paths[LINK_PROVIDER_CAPACITY];
	size_t link_provider_count = 0;
	const char* link_search_dirs[LINK_SEARCH_DIR_CAPACITY];
	size_t link_search_dir_count = 0;
	const char* link_output_path = NULL;
	const char* aggregate_output_path = NULL;
	const char* namespace_name = NULL;
	const char* import_interface_paths[IMPORT_INTERFACE_CAPACITY];
	size_t import_interface_count = 0;
	const char* import_search_dirs[LINK_SEARCH_DIR_CAPACITY];
	size_t import_search_dir_count = 0;
	const char* opaque_export_names[OPAQUE_EXPORT_CAPACITY];
	size_t opaque_export_count = 0;
	int read_graph = 0;
	int link_reexport_providers = 0;
	memset(&read_options, 0, sizeof(read_options));

	for (; file_arg < argc && argv[file_arg][0] == '-'; ++file_arg) {
		if (strcmp(argv[file_arg], "--write-artifact") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--write-artifact requires a path\n");
				return 1;
			}
			artifact_output_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--namespace") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--namespace requires a name\n");
				return 1;
			}
			namespace_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--read-interface") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--read-interface requires a path\n");
				return 1;
			}
			interface_input_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--read-graph") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--read-graph requires a path\n");
				return 1;
			}
			interface_input_path = argv[++file_arg];
			read_graph = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--check-export-normalization-equal") == 0) {
			if (file_arg + 2 >= argc) {
				fprintf(stderr, "--check-export-normalization-equal requires artifact path and export name\n");
				return 1;
			}
			check_export_normalization_equal_path = argv[++file_arg];
			check_export_normalization_equal_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--check-exports-normalization-equal") == 0) {
			if (file_arg + 3 >= argc) {
				fprintf(stderr, "--check-exports-normalization-equal requires artifact path and two export names\n");
				return 1;
			}
			check_exports_normalization_equal_path = argv[++file_arg];
			check_exports_normalization_equal_left_name = argv[++file_arg];
			check_exports_normalization_equal_right_name = argv[++file_arg];
			continue;
		}
			if (strcmp(argv[file_arg], "--reduction-mode") == 0) {
				if (file_arg + 1 >= argc) {
					fprintf(stderr, "--reduction-mode requires default, beta, match, or none\n");
					return 1;
				}
				reduction_mode = argv[++file_arg];
				continue;
			}
			if (strcmp(argv[file_arg], "--check-exports-view-shape-equal") == 0 ||
				strcmp(argv[file_arg], "--check-exports-core-shape-equal") == 0) {
				if (file_arg + 3 >= argc) {
					fprintf(stderr, "%s requires artifact path and two export names\n", argv[file_arg]);
					return 1;
				}
				check_exports_shape_equal_core =
					strcmp(argv[file_arg], "--check-exports-core-shape-equal") == 0;
				check_exports_shape_equal_path = argv[++file_arg];
				check_exports_shape_equal_left_name = argv[++file_arg];
				check_exports_shape_equal_right_name = argv[++file_arg];
				continue;
			}
			if (strcmp(argv[file_arg], "--check-export-classifiers-compatible") == 0) {
				if (file_arg + 3 >= argc) {
					fprintf(stderr, "--check-export-classifiers-compatible requires artifact path and two export names\n");
					return 1;
				}
				check_classifier_path = argv[++file_arg];
				check_classifier_expected_name = argv[++file_arg];
				check_classifier_actual_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-artifacts") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-artifacts requires target.ao\n");
				return 1;
			}
			link_target_path = argv[++file_arg];
			if (file_arg + 1 < argc && argv[file_arg + 1][0] != '-') {
				if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
					fprintf(stderr, "too many link providers\n");
					return 1;
				}
				link_provider_paths[link_provider_count++] = argv[++file_arg];
			}
			continue;
		}
		if (strcmp(argv[file_arg], "--link-provider") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-provider requires provider.ao\n");
				return 1;
			}
			if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
				fprintf(stderr, "too many link providers\n");
				return 1;
			}
			link_provider_paths[link_provider_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-search-dir") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-search-dir requires a directory\n");
				return 1;
			}
			if (link_search_dir_count >= LINK_SEARCH_DIR_CAPACITY) {
				fprintf(stderr, "too many link search dirs\n");
				return 1;
			}
			link_search_dirs[link_search_dir_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--import-interface") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--import-interface requires an artifact path\n");
				return 1;
			}
			if (import_interface_count >= IMPORT_INTERFACE_CAPACITY) {
				fprintf(stderr, "too many import interfaces\n");
				return 1;
			}
			import_interface_paths[import_interface_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--import-search-dir") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--import-search-dir requires a directory\n");
				return 1;
			}
			if (import_search_dir_count >= LINK_SEARCH_DIR_CAPACITY) {
				fprintf(stderr, "too many import search dirs\n");
				return 1;
			}
			import_search_dirs[import_search_dir_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--opaque-export") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--opaque-export requires a term name\n");
				return 1;
			}
			if (opaque_export_count >= OPAQUE_EXPORT_CAPACITY) {
				fprintf(stderr, "too many opaque exports\n");
				return 1;
			}
			opaque_export_names[opaque_export_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-output") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-output requires a path\n");
				return 1;
			}
			link_output_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-reexport-providers") == 0) {
			link_reexport_providers = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--aggregate-artifact") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--aggregate-artifact requires an output path\n");
				return 1;
			}
			aggregate_output_path = argv[++file_arg];
			continue;
		}
		fprintf(stderr, "unknown option: %s\n", argv[file_arg]);
		fprintf(stderr, "Usage: %s [--write-artifact out.ao] [--namespace name] [--opaque-export name ...] [--import-interface import.ao ...] [--import-search-dir dir ...] <file.p>...\n", argv[0]);
		fprintf(stderr, "       %s --read-interface file.ao\n", argv[0]);
			fprintf(stderr, "       %s --read-graph file.ao\n", argv[0]);
			fprintf(stderr, "       %s --check-export-normalization-equal file.ao name\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-normalization-equal file.ao left right [--reduction-mode mode]\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-view-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-core-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-export-classifiers-compatible file.ao expected actual\n", argv[0]);
			fprintf(stderr, "       %s --link-artifacts target.ao provider.ao [--link-provider provider2.ao ...] [--link-search-dir dir] [--link-reexport-providers] [--link-output linked.ao]\n", argv[0]);
			fprintf(stderr, "       %s --aggregate-artifact out.ao base.ao [provider.ao ...]\n", argv[0]);
		return 1;
	}

	if (check_export_normalization_equal_path) {
		return check_export_normalization_equal(check_export_normalization_equal_path, check_export_normalization_equal_name);
	}
	if (check_exports_normalization_equal_path) {
		return check_exports_normalization_equal(
			check_exports_normalization_equal_path,
			check_exports_normalization_equal_left_name,
			check_exports_normalization_equal_right_name,
			reduction_mode
		);
	}
	if (check_exports_shape_equal_path) {
		return check_exports_shape_equal(
			check_exports_shape_equal_path,
			check_exports_shape_equal_left_name,
			check_exports_shape_equal_right_name,
			check_exports_shape_equal_core
		);
	}
	if (check_classifier_path) {
		return check_export_classifier_compatible(
			check_classifier_path,
			check_classifier_expected_name,
			check_classifier_actual_name
		);
	}

	if (aggregate_output_path) {
		if (link_target_path || link_output_path || link_provider_count != 0) {
			fprintf(stderr, "--aggregate-artifact cannot be combined with explicit link options\n");
			return 1;
		}
		if (file_arg >= argc) {
			fprintf(stderr, "--aggregate-artifact requires at least one input artifact\n");
			return 1;
		}
		link_target_path = argv[file_arg++];
		link_output_path = aggregate_output_path;
		link_reexport_providers = 1;
		for (; file_arg < argc; ++file_arg) {
			if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
				fprintf(stderr, "too many aggregate providers\n");
				return 1;
			}
			link_provider_paths[link_provider_count++] = argv[file_arg];
		}
	}

	if (!interface_input_path && !link_target_path && argc - file_arg < 1) {
		fprintf(stderr, "Usage: %s [--write-artifact out.ao] [--namespace name] [--opaque-export name ...] [--import-interface import.ao ...] [--import-search-dir dir ...] <file.p>...\n", argv[0]);
		fprintf(stderr, "       %s --read-interface file.ao\n", argv[0]);
			fprintf(stderr, "       %s --read-graph file.ao\n", argv[0]);
			fprintf(stderr, "       %s --check-export-normalization-equal file.ao name\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-normalization-equal file.ao left right [--reduction-mode mode]\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-view-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-core-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --link-artifacts target.ao provider.ao [--link-provider provider2.ao ...] [--link-search-dir dir] [--link-reexport-providers] [--link-output linked.ao]\n", argv[0]);
			fprintf(stderr, "       %s --aggregate-artifact out.ao base.ao [provider.ao ...]\n", argv[0]);
			return 1;
	}

	struct symbol_table symbols;
	struct prototype_ast_db ast_db;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_term_db term_db;
	struct prototype_universe_db universe_db;
	struct prototype_judgement_db judgement_db;
	struct prototype_compile_metadata metadata;
	struct prototype_program program;
	struct prototype_read_error error;

	symbol_table_init(
		&symbols,
		symbol_ids,
		symbol_hashes,
		SYMBOL_MAP_CAPACITY,
		symbol_strings,
		SYMBOL_STORAGE_CAPACITY
	);
	if (link_target_path) {
		struct prototype_type_declaration_db provider_type_declarations;
		struct prototype_term_db provider_term_db;
		struct prototype_judgement_db provider_judgement_db;
		struct prototype_artifact_interface artifact_interface;
		struct prototype_artifact_interface provider_interface;
		struct prototype_artifact_interface appended_interface;

		prototype_artifact_interface_init(
			&artifact_interface,
			artifact_term_exports,
			ARTIFACT_TERM_EXPORT_CAPACITY,
			artifact_type_exports,
			ARTIFACT_TYPE_EXPORT_CAPACITY,
			artifact_type_parameter_exports,
			ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
			artifact_constructor_exports,
			ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
			artifact_constructor_field_type_exprs,
			ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
			artifact_interface_type_exprs,
			ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
			artifact_dependencies,
			ARTIFACT_DEPENDENCY_CAPACITY
		);
		prototype_type_declaration_db_init(
			&type_declarations,
			type_declaration_storage,
			TYPE_CAPACITY,
			constructor_declaration_storage,
			CONSTRUCTOR_CAPACITY,
			parameter_declaration_storage,
			PARAMETER_CAPACITY,
			field_types,
			FIELD_TYPE_CAPACITY,
			type_exprs,
			TYPE_EXPR_CAPACITY
		);
		prototype_term_db_init(
			&term_db,
			terms,
			TERM_CAPACITY,
			match_cases,
			match_case_label_symbols,
			MATCH_CASE_CAPACITY,
			match_binders,
			MATCH_BINDER_CAPACITY,
			match_frames,
			MATCH_FRAME_CAPACITY
		);
		prototype_judgement_db_init(
			&judgement_db,
			judgements,
			judgement_proofs,
			JUDGEMENT_CAPACITY
		);
		prototype_universe_db_init(
			&universe_db,
			universe_nodes,
			UNIVERSE_NODE_CAPACITY,
			universe_edges,
			UNIVERSE_EDGE_CAPACITY,
			universe_levels,
			UNIVERSE_LEVEL_CAPACITY,
			universe_constraints,
			UNIVERSE_CONSTRAINT_CAPACITY
		);

		if (read_artifact_interface_and_graph(
				link_target_path,
				&symbols,
				&artifact_interface,
				&term_db,
				&type_declarations,
				&judgement_db,
				&universe_db
			) != 0) {
			fprintf(stderr, "%s: failed to read target artifact\n", link_target_path);
			symbol_table_free(&symbols);
			return 1;
		}
		if (link_search_dir_count > 0 &&
			add_provider_closure_from_search_dirs(
				link_provider_paths,
				&link_provider_count,
				link_target_path,
				link_search_dirs,
				link_search_dir_count,
				&symbols,
				&artifact_interface,
				&provider_interface,
				&appended_interface
			) != 0) {
			fprintf(stderr, "%s: failed to search provider artifacts\n", link_target_path);
			symbol_table_free(&symbols);
			return 1;
		}
		if (order_link_providers_by_interface_dependencies(
				link_provider_paths,
				link_provider_count,
				&symbols
			) != 0) {
			fprintf(stderr, "%s: failed to order provider artifacts\n", link_target_path);
			symbol_table_free(&symbols);
			return 1;
		}

		size_t before_terms = term_db.term_count;
		size_t before_types = type_declarations.type_count;
		size_t total_provider_exports = 0;
		for (size_t provider_index = 0; provider_index < link_provider_count; ++provider_index) {
			const char* provider_path = link_provider_paths[provider_index];
			prototype_artifact_interface_init(
				&provider_interface,
				provider_artifact_term_exports,
				ARTIFACT_TERM_EXPORT_CAPACITY,
				provider_artifact_type_exports,
				ARTIFACT_TYPE_EXPORT_CAPACITY,
				provider_artifact_type_parameter_exports,
				ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
				provider_artifact_constructor_exports,
				ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
				provider_artifact_constructor_field_type_exprs,
				ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
				provider_artifact_interface_type_exprs,
				ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
				provider_artifact_dependencies,
				ARTIFACT_DEPENDENCY_CAPACITY
			);
			prototype_artifact_interface_init(
				&appended_interface,
				appended_artifact_term_exports,
				ARTIFACT_TERM_EXPORT_CAPACITY,
				appended_artifact_type_exports,
				ARTIFACT_TYPE_EXPORT_CAPACITY,
				appended_artifact_type_parameter_exports,
				ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
				appended_artifact_constructor_exports,
				ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
				appended_artifact_constructor_field_type_exprs,
				ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
				appended_artifact_interface_type_exprs,
				ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
				appended_artifact_dependencies,
				ARTIFACT_DEPENDENCY_CAPACITY
			);
			prototype_type_declaration_db_init(
				&provider_type_declarations,
				provider_type_declaration_storage,
				TYPE_CAPACITY,
				provider_constructor_declaration_storage,
				CONSTRUCTOR_CAPACITY,
				provider_parameter_declaration_storage,
				PARAMETER_CAPACITY,
				provider_field_types,
				FIELD_TYPE_CAPACITY,
				provider_type_exprs,
				TYPE_EXPR_CAPACITY
			);
			prototype_term_db_init(
				&provider_term_db,
				provider_terms,
				TERM_CAPACITY,
				provider_match_cases,
				provider_match_case_label_symbols,
				MATCH_CASE_CAPACITY,
				provider_match_binders,
				MATCH_BINDER_CAPACITY,
				provider_match_frames,
				MATCH_FRAME_CAPACITY
			);
			prototype_judgement_db_init(
				&provider_judgement_db,
				provider_judgements,
				provider_judgement_proofs,
				JUDGEMENT_CAPACITY
			);
			if (read_artifact_interface_and_graph(
					provider_path,
					&symbols,
					&provider_interface,
					&provider_term_db,
					&provider_type_declarations,
					&provider_judgement_db,
					&universe_db
				) != 0) {
				fprintf(stderr, "%s: failed to read provider artifact\n", provider_path);
				symbol_table_free(&symbols);
				return 1;
			}
			if (prototype_artifact_apply_type_expr_relocations(
					&artifact_interface,
					&term_db,
					&type_declarations,
					&judgement_db,
					&provider_interface
				) != 0 ||
				prototype_artifact_append_graph(
					&appended_interface,
					&term_db,
					&type_declarations,
					&judgement_db,
					&provider_interface,
					&provider_term_db,
					&provider_type_declarations,
					&provider_judgement_db
				) != 0) {
				fprintf(stderr, "%s + %s: failed to link artifacts\n", link_target_path, provider_path);
				symbol_table_free(&symbols);
				return 1;
			}
			if (prototype_artifact_apply_type_expr_relocations(
					&appended_interface,
					&term_db,
					&type_declarations,
					&judgement_db,
					&artifact_interface
				) != 0 ||
				prototype_artifact_apply_term_relocations(
					&appended_interface,
					&term_db,
					&type_declarations,
					&judgement_db,
					&artifact_interface
				) != 0 ||
				prototype_artifact_apply_term_relocations(
					&artifact_interface,
					&term_db,
					&type_declarations,
					&judgement_db,
					&appended_interface
				) != 0) {
				fprintf(stderr, "%s + %s: failed to link artifacts\n", link_target_path, provider_path);
				symbol_table_free(&symbols);
				return 1;
			}
			if (link_reexport_providers) {
				if (reexport_appended_interface(
						&artifact_interface,
						&appended_interface
					) != 0 ||
					prototype_artifact_interface_collect_dependencies(
						&artifact_interface,
						&term_db,
						&type_declarations,
						&judgement_db
					) != 0) {
					fprintf(stderr, "%s: failed to re-export provider interface\n", provider_path);
					symbol_table_free(&symbols);
					return 1;
				}
			}
			total_provider_exports += provider_interface.term_export_count;
			printf("linked provider artifact %s exports=%zu appended_exports=%zu dependencies=%zu\n",
				provider_path,
				provider_interface.term_export_count,
				appended_interface.term_export_count,
				artifact_interface.dependency_count);
			for (size_t i = 0; i < appended_interface.term_export_count; ++i) {
				const struct prototype_artifact_term_export* export =
					&appended_interface.term_exports[i];
				printf("linked provider term %s -> term#%u\n",
					symbol_to_string(&symbols, export->name_symbol_id),
					export->local_term);
			}
		}
		if (prototype_artifact_interface_recompute_keys(
				&artifact_interface,
				&term_db,
				&type_declarations
			) != 0 ||
			prototype_artifact_interface_collect_dependencies(
				&artifact_interface,
				&term_db,
				&type_declarations,
				&judgement_db
			) != 0) {
			fprintf(stderr, "%s: failed to finalize linked artifact interface\n", link_target_path);
			symbol_table_free(&symbols);
			return 1;
		}
			prototype_judgement_resolve_declaration_premises(
				&term_db,
				&type_declarations,
			&judgement_db
		);
		prototype_judgement_resolve_proof_edges(&judgement_db);
		if (prototype_judgement_validate_proofs(
				&term_db,
				&type_declarations,
				&judgement_db
			) != 0) {
			fprintf(stderr, "%s: linked artifact proof validation failed\n", link_target_path);
			symbol_table_free(&symbols);
			return 1;
		}
		if (link_output_path) {
			if (prototype_universe_collect(&universe_db, &type_declarations, &term_db, &judgement_db) != 0) {
				fprintf(stderr, "%s: failed to collect linked universe graph\n", link_output_path);
				symbol_table_free(&symbols);
				return 1;
			}
			FILE* output = fopen(link_output_path, "w");
			if (!output) {
				fprintf(stderr, "%s: failed to open linked artifact output\n", link_output_path);
				symbol_table_free(&symbols);
				return 1;
			}
			int write_status = prototype_artifact_write_text(
				output,
				&symbols,
				&artifact_interface,
				&term_db,
				&type_declarations,
				&judgement_db,
				&universe_db,
				NULL
			);
			if (fclose(output) != 0) {
				write_status = -1;
			}
			if (write_status != 0) {
				fprintf(stderr, "%s: failed to write linked artifact\n", link_output_path);
				symbol_table_free(&symbols);
				return 1;
			}
		}

		printf(
			"#### Artifact Link ####\n"
			"target=%s providers=%zu\n"
			"output=%s\n"
			"terms=%zu->%zu types=%zu->%zu judgements=%zu\n"
			"target_exports=%zu provider_exports=%zu dependencies=%zu reexport=%s\n",
			link_target_path,
			link_provider_count,
			link_output_path ? link_output_path : "<none>",
			before_terms,
			term_db.term_count,
			before_types,
			type_declarations.type_count,
			judgement_db.relation_count,
			artifact_interface.term_export_count,
			total_provider_exports,
			artifact_interface.dependency_count,
			link_reexport_providers ? "yes" : "no"
		);
		for (size_t i = 0; i < artifact_interface.term_export_count; ++i) {
			const struct prototype_artifact_term_export* export =
				&artifact_interface.term_exports[i];
			printf("linked target term %s -> term#%u\n",
				symbol_to_string(&symbols, export->name_symbol_id),
				export->local_term);
		}
		symbol_table_free(&symbols);
		return 0;
	}
	if (interface_input_path) {
		struct prototype_artifact_interface artifact_interface;
		struct prototype_artifact_relocation_table relocation_table;
		struct prototype_artifact_debug_table debug_table;
		prototype_artifact_interface_init(
			&artifact_interface,
			artifact_term_exports,
			ARTIFACT_TERM_EXPORT_CAPACITY,
			artifact_type_exports,
			ARTIFACT_TYPE_EXPORT_CAPACITY,
			artifact_type_parameter_exports,
			ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
			artifact_constructor_exports,
			ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
			artifact_constructor_field_type_exprs,
			ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
			artifact_interface_type_exprs,
			ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
			artifact_dependencies,
			ARTIFACT_DEPENDENCY_CAPACITY
		);
		prototype_artifact_relocation_table_init(
			&relocation_table,
			artifact_external_term_refs,
			ARTIFACT_EXTERNAL_TERM_REF_CAPACITY,
			artifact_resolved_external_term_refs,
			ARTIFACT_RESOLVED_EXTERNAL_TERM_REF_CAPACITY,
			artifact_external_type_expr_refs,
			ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
			artifact_resolved_external_type_expr_refs,
			ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
			artifact_external_type_former_refs,
			ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
			artifact_resolved_external_type_former_refs,
			ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
			artifact_resolved_constructor_owner_refs,
			ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY
		);
		prototype_artifact_debug_table_init(
			&debug_table,
			artifact_debug_term_names,
			ARTIFACT_DEBUG_NAME_CAPACITY,
			artifact_debug_type_names,
			ARTIFACT_DEBUG_NAME_CAPACITY,
			artifact_debug_constructor_names,
			ARTIFACT_DEBUG_NAME_CAPACITY
		);
		FILE* artifact_file = fopen(interface_input_path, "r");
		if (!artifact_file) {
			fprintf(stderr, "%s: failed to open artifact interface\n", interface_input_path);
			symbol_table_free(&symbols);
			return 1;
		}
		int read_status = prototype_artifact_read_text_interface(
			artifact_file,
			&symbols,
			&artifact_interface
		);
		if (read_status != 0) {
			fclose(artifact_file);
			fprintf(stderr, "%s: failed to read artifact interface\n", interface_input_path);
			symbol_table_free(&symbols);
			return 1;
		}
		if (read_graph) {
			prototype_type_declaration_db_init(
				&type_declarations,
				type_declaration_storage,
				TYPE_CAPACITY,
				constructor_declaration_storage,
				CONSTRUCTOR_CAPACITY,
				parameter_declaration_storage,
				PARAMETER_CAPACITY,
				field_types,
				FIELD_TYPE_CAPACITY,
				type_exprs,
				TYPE_EXPR_CAPACITY
			);
			prototype_term_db_init(
				&term_db,
				terms,
				TERM_CAPACITY,
				match_cases,
				match_case_label_symbols,
				MATCH_CASE_CAPACITY,
				match_binders,
				MATCH_BINDER_CAPACITY,
				match_frames,
				MATCH_FRAME_CAPACITY
			);
			prototype_judgement_db_init(
				&judgement_db,
				judgements,
				judgement_proofs,
				JUDGEMENT_CAPACITY
			);
			prototype_universe_db_init(
				&universe_db,
				universe_nodes,
				UNIVERSE_NODE_CAPACITY,
				universe_edges,
				UNIVERSE_EDGE_CAPACITY,
				universe_levels,
				UNIVERSE_LEVEL_CAPACITY,
				universe_constraints,
				UNIVERSE_CONSTRAINT_CAPACITY
			);
			if (prototype_artifact_read_text_graph(
					artifact_file,
					&symbols,
					&term_db,
					&type_declarations,
					&judgement_db
				) != 0 ||
				prototype_artifact_read_text_universe(
					artifact_file,
					&universe_db
				) != 0 ||
				prototype_artifact_read_text_debug(
					artifact_file,
					&symbols,
					&debug_table
				) != 0 ||
				prototype_artifact_read_text_relocation(
					artifact_file,
					&symbols,
					&relocation_table
				) != 0 ||
				prototype_judgement_validate_proofs(
					&term_db,
					&type_declarations,
					&judgement_db
				) != 0) {
				fclose(artifact_file);
				fprintf(stderr, "%s: failed to read artifact graph/universe/relocation\n", interface_input_path);
				symbol_table_free(&symbols);
				return 1;
			}
		}
		fclose(artifact_file);
		printf(
			"#### Artifact Interface ####\n"
			"term_exports=%zu type_exports=%zu constructor_exports=%zu dependencies=%zu\n",
			artifact_interface.term_export_count,
			artifact_interface.type_export_count,
			artifact_interface.constructor_export_count,
			artifact_interface.dependency_count
			);
			for (size_t i = 0; i < artifact_interface.term_export_count; ++i) {
				const struct prototype_artifact_term_export* term_export =
					&artifact_interface.term_exports[i];
				printf("interface term %s local_term#%u classifier#%u transparency=%s key=%llu classifier_key=%llu\n",
					symbol_to_string(&symbols, term_export->name_symbol_id),
					term_export->local_term,
					term_export->classifier,
					term_export->transparency == PROTOTYPE_ARTIFACT_EXPORT_OPAQUE ?
						"opaque" : "transparent",
					(unsigned long long)term_export->canonical_key.hash,
					(unsigned long long)term_export->classifier_key.hash);
			}
			for (size_t i = 0; i < artifact_interface.type_export_count; ++i) {
				const struct prototype_artifact_type_export* type_export =
					&artifact_interface.type_exports[i];
			printf("interface type %s local_type#%u core_representation_anchor_type#%u constructors=%u representation_fingerprint=%llu\n",
				symbol_to_string(&symbols, type_export->name_symbol_id),
				type_export->local_type_id,
				type_export->core_representation_anchor_type_id,
				type_export->constructor_count,
				(unsigned long long)type_export->code_shape_key.hash);
		}
		for (size_t i = 0; i < artifact_interface.constructor_export_count; ++i) {
			const struct prototype_artifact_constructor_export* constructor_export =
				&artifact_interface.constructor_exports[i];
			printf("interface constructor type_export#%u.%s ordinal=%u fields=%u classifier_family=%u\n",
				constructor_export->type_export_index,
				symbol_to_string(&symbols, constructor_export->name_symbol_id),
				constructor_export->ordinal,
			constructor_export->readback_field_count,
				constructor_export->classifier_family);
		}
		if (read_graph) {
			printf(
				"\n"
				"#### Artifact Graph ####\n"
				"terms=%zu cases=%zu case_binders=%zu frames=%zu types=%zu constructors=%zu type_exprs=%zu judgements=%zu proofs=%zu\n",
				term_db.term_count,
				term_db.case_count,
				term_db.case_binder_count,
				term_db.match_frame_count,
				type_declarations.type_count,
				type_declarations.constructor_count,
				type_declarations.expr_count,
				judgement_db.relation_count,
				judgement_db.proof_count
			);
			printf(
				"universe_nodes=%zu universe_edges=%zu universe_levels=%zu universe_constraints=%zu solved=%s\n",
				universe_db.node_count,
				universe_db.edge_count,
				universe_db.level_count,
				universe_db.constraint_count,
				universe_db.solved ? "yes" : "no"
			);
			printf(
				"graph_next_level_var=%u judgement_next_universe_var=%u\n",
				type_declarations.next_level_var,
				judgement_db.next_universe_var
			);
			printf(
				"relocation_external_terms=%zu relocation_resolved_external_terms=%zu relocation_external_type_exprs=%zu relocation_resolved_external_type_exprs=%zu relocation_external_type_formers=%zu relocation_resolved_external_type_formers=%zu relocation_resolved_constructor_owners=%zu\n",
				relocation_table.external_term_ref_count,
				relocation_table.resolved_external_term_ref_count,
				relocation_table.external_type_expr_ref_count,
				relocation_table.resolved_external_type_expr_ref_count,
				relocation_table.external_type_former_ref_count,
				relocation_table.resolved_external_type_former_ref_count,
				relocation_table.resolved_constructor_owner_ref_count
			);
			printf(
				"debug_term_names=%zu debug_type_names=%zu debug_constructor_names=%zu\n",
				debug_table.term_name_count,
				debug_table.type_name_count,
				debug_table.constructor_name_count
			);
			for (size_t i = 0; i < relocation_table.resolved_external_term_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_term_ref* ref =
					&relocation_table.resolved_external_term_refs[i];
				printf(
					"resolved external term term#%u -> export#%u.%s\n",
					ref->term,
					ref->term_export_index,
						symbol_to_string(&symbols, ref->name.name_symbol_id)
				);
			}
			for (size_t i = 0; i < relocation_table.resolved_external_type_expr_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_type_expr_ref* ref =
					&relocation_table.resolved_external_type_expr_refs[i];
				printf(
					"resolved external type expr type_expr#%u -> type_export#%u.%s code_shape_key=%llu\n",
					ref->type_expr,
					ref->type_export_index,
					symbol_to_string(&symbols, ref->name.name_symbol_id),
					(unsigned long long)ref->code_shape_key.hash
				);
			}
			for (size_t i = 0; i < relocation_table.external_type_former_ref_count; ++i) {
				const struct prototype_artifact_external_type_former_ref* ref =
					&relocation_table.external_type_former_refs[i];
				printf(
					"external type former type_expr#%u -> %s\n",
					ref->type_expr,
					symbol_to_string(&symbols, ref->name_symbol_id)
				);
			}
			for (size_t i = 0; i < relocation_table.resolved_external_type_former_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_type_former_ref* ref =
					&relocation_table.resolved_external_type_former_refs[i];
				printf(
					"resolved external type former type_expr#%u -> type_export#%u.%s code_shape_key=%llu\n",
					ref->type_expr,
					ref->type_export_index,
					symbol_to_string(&symbols, ref->name.name_symbol_id),
					(unsigned long long)ref->code_shape_key.hash
				);
			}
			for (size_t i = 0; i < relocation_table.resolved_constructor_owner_ref_count; ++i) {
				const struct prototype_artifact_resolved_constructor_owner_ref* ref =
					&relocation_table.resolved_constructor_owner_refs[i];
				printf(
					"resolved constructor owner kind=%d source#%u owner#%u ordinal=%u key=%llu\n",
					ref->source_kind,
					ref->source,
					ref->owner,
					ref->ordinal,
					(unsigned long long)ref->owner_key.hash
				);
			}
		}
		symbol_table_free(&symbols);
		return 0;
	}
	prototype_type_declaration_db_init(
		&type_declarations,
		type_declaration_storage,
		TYPE_CAPACITY,
		constructor_declaration_storage,
		CONSTRUCTOR_CAPACITY,
		parameter_declaration_storage,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);
	prototype_ast_db_init(
		&ast_db,
		ast_nodes,
		AST_CAPACITY,
		ast_expectations,
		AST_DEF_CAPACITY,
		ast_assignments,
		AST_DEF_CAPACITY,
		ast_imports,
		AST_DEF_CAPACITY,
		ast_def_index,
		AST_DEF_CAPACITY,
		ast_match_cases,
		AST_MATCH_CASE_CAPACITY,
		ast_match_binders,
		AST_MATCH_BINDER_CAPACITY,
		ast_type_exprs,
		AST_TYPE_EXPR_CAPACITY,
		ast_type_defs,
		AST_TYPE_DEF_CAPACITY,
		ast_type_parameters,
		AST_TYPE_PARAMETER_CAPACITY,
		ast_type_constructors,
		AST_TYPE_CONSTRUCTOR_CAPACITY,
		ast_type_field_exprs,
		ast_type_field_binder_ids,
		ast_type_field_name_symbol_ids,
		AST_TYPE_FIELD_EXPR_CAPACITY
	);
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		UNIVERSE_NODE_CAPACITY,
		universe_edges,
		UNIVERSE_EDGE_CAPACITY,
		universe_levels,
		UNIVERSE_LEVEL_CAPACITY,
		universe_constraints,
		UNIVERSE_CONSTRAINT_CAPACITY
	);
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		match_cases,
		match_case_label_symbols,
		MATCH_CASE_CAPACITY,
		match_binders,
		MATCH_BINDER_CAPACITY,
		match_frames,
		MATCH_FRAME_CAPACITY
	);
	prototype_compile_metadata_init(
		&metadata,
		compile_labels,
		COMPILE_LABEL_CAPACITY,
		compile_type_exports,
		COMPILE_TYPE_EXPORT_CAPACITY,
		compile_constructor_exports,
		COMPILE_CONSTRUCTOR_EXPORT_CAPACITY,
		resolve_errors,
		RESOLVE_ERROR_CAPACITY,
		resolution_items,
		RESOLUTION_ITEM_CAPACITY,
		resolution_iterations,
		RESOLUTION_ITERATION_CAPACITY,
		resolution_events,
		RESOLUTION_EVENT_CAPACITY,
		operations,
		OPERATION_CAPACITY,
		operation_cases,
		OPERATION_CASE_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement_db,
		judgements,
		judgement_proofs,
		JUDGEMENT_CAPACITY
	);

	program.symbols = &symbols;
	program.namespace_symbol_id = -1;
	program.asts = &ast_db;
	program.type_declarations = &type_declarations;
	program.terms = &term_db;
	program.judgement = &judgement_db;
	program.metadata = &metadata;
	program.universe = &universe_db;

	for (int i = file_arg; i < argc; ++i) {
		if (prototype_read_ast_file_with_options(argv[i], &program, &read_options, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : argv[i],
				error.line,
				error.column,
				error.message[0] ? error.message : "read failed"
			);
			symbol_table_free(&symbols);
			return 1;
		}
	}
	const struct prototype_artifact_interface* imported_interface_refs[IMPORT_INTERFACE_CAPACITY];
	for (size_t i = 0; i < import_interface_count; ++i) {
		if (read_import_artifact_into_slot(
				import_interface_paths[i],
				&symbols,
				&program,
				i,
				&universe_db
			) != 0) {
			fprintf(stderr, "%s: failed to read import artifact\n", import_interface_paths[i]);
			symbol_table_free(&symbols);
			return 1;
		}
		imported_interface_refs[i] = &imported_artifact_interfaces[i];
	}
	for (size_t i = 0; i < ast_db.import_count; ++i) {
		int import_status = add_source_import_from_search_dirs(
			&ast_db.imports[i],
			import_search_dirs,
			import_search_dir_count,
			&symbols,
			&program,
			imported_interface_refs,
			&import_interface_count,
			&universe_db
		);
		if (import_status != 0) {
			const char* import_name =
				symbol_to_string(&symbols, ast_db.imports[i].name_symbol_id);
			if (import_status > 0) {
				fprintf(stderr, "%s:%u:%u: unresolved import %s\n",
					argv[file_arg],
					ast_db.imports[i].name_span.line,
					ast_db.imports[i].name_span.column,
					import_name ? import_name : "<unknown>");
			} else {
				fprintf(stderr, "%s:%u:%u: failed to load import %s\n",
					argv[file_arg],
					ast_db.imports[i].name_span.line,
					ast_db.imports[i].name_span.column,
					import_name ? import_name : "<unknown>");
			}
			symbol_table_free(&symbols);
			return 1;
		}
	}
		const char* namespace_source = namespace_name ? namespace_name : argv[file_arg];
		int namespace_symbol_id = namespace_name ?
			namespace_symbol_from_text(&symbols, namespace_source) :
			namespace_symbol_from_path(&symbols, namespace_source);
		if (namespace_symbol_id < 0) {
			fprintf(stderr, "%s: failed to determine namespace\n", namespace_source);
			symbol_table_free(&symbols);
			return 1;
		}
		program.namespace_symbol_id = namespace_symbol_id;
		if (prototype_compile_graph_with_imports(
			&program,
			imported_interface_refs,
			import_interface_count,
			&error
		) != 0) {
		fprintf(
			stderr,
			"%s:%u:%u: %s\n",
			error.filename ? error.filename : argv[file_arg],
			error.line,
			error.column,
			error.message[0] ? error.message : "graph compile failed"
		);
		print_metadata_resolve_errors(stderr, &symbols, &metadata);
		symbol_table_free(&symbols);
		return 1;
	}
	if (prototype_universe_collect(&universe_db, &type_declarations, &term_db, &judgement_db) != 0) {
		fprintf(stderr, "%s: failed to collect universe graph\n", argv[file_arg]);
		symbol_table_free(&symbols);
		return 1;
	}
	struct prototype_artifact_interface artifact_interface;
	prototype_artifact_interface_init(
		&artifact_interface,
		artifact_term_exports,
		ARTIFACT_TERM_EXPORT_CAPACITY,
		artifact_type_exports,
		ARTIFACT_TYPE_EXPORT_CAPACITY,
		artifact_type_parameter_exports,
		ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		artifact_constructor_exports,
		ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		artifact_constructor_field_type_exprs,
		ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		artifact_interface_type_exprs,
		ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		artifact_dependencies,
		ARTIFACT_DEPENDENCY_CAPACITY
	);
	if (prototype_artifact_interface_build_from_metadata(
			&artifact_interface,
			&metadata,
			&term_db,
			&type_declarations,
			&judgement_db
		) != 0) {
		fprintf(stderr, "%s: failed to build artifact interface\n", argv[file_arg]);
		symbol_table_free(&symbols);
		return 1;
	}
	for (size_t i = 0; i < opaque_export_count; ++i) {
		if (mark_opaque_export(
				&symbols,
				&artifact_interface,
				opaque_export_names[i]
			) != 0) {
			fprintf(stderr, "%s: unknown opaque export: %s\n",
				artifact_output_path ? artifact_output_path : argv[file_arg],
				opaque_export_names[i]);
			symbol_table_free(&symbols);
			return 1;
		}
	}
	if (prototype_artifact_interface_collect_dependencies(
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db
		) != 0) {
		fprintf(stderr, "%s: failed to collect artifact dependencies\n", argv[file_arg]);
		symbol_table_free(&symbols);
		return 1;
	}
	for (size_t i = 0; i < ast_db.import_count; ++i) {
		if (prototype_artifact_interface_add_dependency(
				&artifact_interface,
				ast_db.imports[i].name_symbol_id
			) != 0) {
			fprintf(stderr, "%s: failed to add source import dependency\n", argv[file_arg]);
			symbol_table_free(&symbols);
			return 1;
		}
	}
		prototype_artifact_interface_set_namespace(&artifact_interface, namespace_symbol_id);
	if (artifact_output_path) {
		FILE* artifact_file = fopen(artifact_output_path, "w");
		if (!artifact_file) {
			fprintf(stderr, "%s: failed to open artifact output\n", artifact_output_path);
			symbol_table_free(&symbols);
			return 1;
		}
		int write_status = prototype_artifact_write_text(
			artifact_file,
			&symbols,
			&artifact_interface,
			&term_db,
			&type_declarations,
			&judgement_db,
			&universe_db,
			&ast_db
		);
		fclose(artifact_file);
		if (write_status != 0) {
			fprintf(stderr, "%s: failed to write artifact\n", artifact_output_path);
			symbol_table_free(&symbols);
			return 1;
		}
	}
	printf(
		"#### AST ####\n"
		"asts=%zu ast_expectations=%zu ast_assignments=%zu\n"
		"\n"
		"#### Raw Graph ####\n"
		"types=%zu constructors=%zu labels=%zu terms=%zu\n",
		ast_db.node_count,
		ast_db.expectation_count,
		ast_db.assignment_count,
		type_declarations.type_count,
		type_declarations.constructor_count,
		metadata.label_count,
		term_db.term_count
	);

	for (size_t i = 0; i < type_declarations.type_count; ++i) {
		const struct prototype_type_declaration* type = &type_declarations.type_declarations[i];
		if (type->name_symbol_id < 0 || type->type_index == PROTOTYPE_INVALID_ID) {
			continue;
		}
		print_type_declaration(&symbols, &type_declarations, type);
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations.constructor_declarations[type->first_constructor + j];
			if (constructor->name_symbol_id < 0 ||
				constructor->owner_type == PROTOTYPE_INVALID_ID) {
				continue;
			}
			printf("constructor ");
			print_type_namespace(&symbols, &type_declarations, type);
			printf(".%s readback_fields=%u classifier_family=%u\n",
				symbol_to_string(&symbols, constructor->name_symbol_id),
				constructor->readback.field_count,
				constructor->classifier_family);
		}
	}
	for (size_t i = 0; i < metadata.label_count; ++i) {
		const struct prototype_compile_label* label = &metadata.labels[i];
		printf("term %s := ", symbol_to_string(&symbols, label->name_symbol_id));
		prototype_term_print_debug(stdout, &symbols, &type_declarations, &term_db, label->term);
		printf("\n");
	}
	printf("\n#### Operations ####\noperations=%zu cases=%zu\n",
		metadata.operation_count,
		metadata.operation_case_count);
	for (size_t i = 0; i < metadata.operation_count; ++i) {
		const struct prototype_operation_node* operation = &metadata.operations[i];
		printf("operation#%zu %s core#%u classifier#%u ast#%u",
			i,
			operation_tag_name(operation->tag),
			operation->core_term,
			operation->classifier,
			operation->source_ast);
		if (operation->source_symbol_id >= 0) {
			printf(" name=%s", symbol_to_string(&symbols, operation->source_symbol_id));
		}
		if (operation->binder_symbol_id >= 0) {
			printf(" binder=%s", symbol_to_string(&symbols, operation->binder_symbol_id));
		}
		if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
			printf(" scrutinee-operation#%u cases=%u", operation->scrutinee,
				operation->case_count);
		}
		printf("\n");
	}
	for (size_t i = 0; i < metadata.operation_case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			&metadata.operation_cases[i];
		printf("operation-case#%zu body-operation#%u owner#%u ordinal#%u",
			i,
			operation_case->body_operation,
			operation_case->constructor_owner,
			operation_case->constructor_id);
		if (operation_case->case_label_symbol_id >= 0) {
			printf(" label=%s",
				symbol_to_string(&symbols, operation_case->case_label_symbol_id));
		}
		printf("\n");
	}
	printf("\n#### Judgements ####\n");
	prototype_judgement_print(stdout, &symbols, &type_declarations, &term_db, &judgement_db);
	memset(reachable_external_refs, 0, sizeof(reachable_external_refs));
	for (size_t i = 0; i < metadata.label_count; ++i) {
		mark_reachable_external_refs(&term_db, metadata.labels[i].term, 0);
	}
	for (size_t i = 0; i < judgement_db.relation_count; ++i) {
		mark_reachable_external_refs(&term_db, judgement_db.relations[i].subject, 0);
		mark_reachable_external_refs(&term_db, judgement_db.relations[i].classifier, 0);
	}
	size_t external_ref_count = 0;
	for (size_t i = 0; i < term_db.term_count; ++i) {
		if (reachable_external_refs[i]) {
			external_ref_count++;
		}
	}
	printf(
		"\n"
		"#### Metadata ####\n"
		"labels=%zu resolve_errors=%zu external_refs=%zu self_contained=%s\n",
		metadata.label_count,
		metadata.resolve_error_count,
		external_ref_count,
		metadata.resolve_error_count == 0 && external_ref_count == 0 ? "yes" : "no"
	);
	for (size_t i = 0; i < metadata.label_count; ++i) {
		const struct prototype_compile_label* label = &metadata.labels[i];
		printf("metadata label %s -> operation#%u -> term#%u\n",
			symbol_to_string(&symbols, label->name_symbol_id),
			label->operation,
			label->term);
	}
	for (size_t i = 0; i < metadata.resolve_error_count; ++i) {
		const struct prototype_resolve_error* resolve_error = &metadata.resolve_errors[i];
		printf("metadata resolve-error kind=%s name=%s",
			resolve_error_kind_name(resolve_error->kind),
			symbol_to_string(&symbols, resolve_error->name_symbol_id));
		if (resolve_error->member_symbol_id >= 0) {
			printf(".%s", symbol_to_string(&symbols, resolve_error->member_symbol_id));
		}
		printf(
			" ast#%u span=%u:%u\n",
			resolve_error->ast,
			resolve_error->span.line,
			resolve_error->span.column
		);
	}
	for (size_t i = 0; i < term_db.term_count; ++i) {
		if (reachable_external_refs[i]) {
			printf("metadata external-ref %s -> term#%zu\n",
					symbol_to_string(&symbols, term_db.terms[i].as.external_ref.name.name_symbol_id),
			i);
		}
	}
	printf(
		"\n"
		"#### Artifact Interface ####\n"
		"term_exports=%zu type_exports=%zu constructor_exports=%zu dependencies=%zu\n",
		artifact_interface.term_export_count,
		artifact_interface.type_export_count,
		artifact_interface.constructor_export_count,
		artifact_interface.dependency_count
	);
	for (size_t i = 0; i < artifact_interface.type_export_count; ++i) {
		const struct prototype_artifact_type_export* type_export =
			&artifact_interface.type_exports[i];
		printf("interface type %s local_type#%u core_representation_anchor_type#%u constructors=%u representation_fingerprint=%llu\n",
			symbol_to_string(&symbols, type_export->name_symbol_id),
			type_export->local_type_id,
			type_export->core_representation_anchor_type_id,
			type_export->constructor_count,
			(unsigned long long)type_export->code_shape_key.hash);
	}
	for (size_t i = 0; i < artifact_interface.constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* constructor_export =
			&artifact_interface.constructor_exports[i];
		printf("interface constructor type_export#%u.%s ordinal=%u fields=%u classifier_family=%u\n",
			constructor_export->type_export_index,
			symbol_to_string(&symbols, constructor_export->name_symbol_id),
			constructor_export->ordinal,
			constructor_export->readback_field_count,
			constructor_export->classifier_family);
	}
	printf("\n#### Resolution ####\n");
	print_resolution_trace(&symbols, &type_declarations, &term_db, &metadata);
	printf("\n#### Universe ####\n");
	print_universe_graph(&symbols, &type_declarations, &universe_db);

	symbol_table_free(&symbols);
	return 0;
}
