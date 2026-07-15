#include "reader.h"

#include <stdio.h>
#include <string.h>

#include "ast_inspect.h"
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
#define INPUT_CAPACITY 8192
#define LINE_CAPACITY 1024

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

static const struct prototype_compile_label* lookup_label(
	const struct prototype_compile_metadata* metadata,
	int symbol_id
) {
	if (!metadata) {
		return NULL;
	}
	for (size_t i = metadata->label_count; i > 0; --i) {
		const struct prototype_compile_label* label = &metadata->labels[i - 1];
		if (label->name_symbol_id == symbol_id) {
			return label;
		}
	}
	return NULL;
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

static void print_state(
	const struct symbol_table* symbols,
	const struct prototype_ast_db* ast_db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* term_db,
	const struct prototype_universe_db* universe_db,
	const struct prototype_judgement_db* judgement_db,
	const struct prototype_compile_metadata* metadata
) {
	printf(
		"#### AST ####\n"
		"asts=%zu ast_expectations=%zu ast_assignments=%zu\n"
		"\n"
		"#### Raw Graph ####\n"
		"types=%zu constructors=%zu labels=%zu terms=%zu\n",
		ast_db->node_count,
		ast_db->expectation_count,
		ast_db->assignment_count,
		type_declarations->type_count,
		type_declarations->constructor_count,
		metadata ? metadata->label_count : 0,
		term_db->term_count
	);
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type = &type_declarations->type_declarations[i];
		print_type_declaration(symbols, type_declarations, type);
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[type->first_constructor + j];
			printf("constructor ");
			print_type_namespace(symbols, type_declarations, type);
			printf(".%s readback_fields=%u classifier_family=%u\n",
				symbol_to_string(symbols, constructor->name_symbol_id),
				constructor->readback.field_count,
				constructor->classifier_family);
		}
	}
	if (metadata) {
		for (size_t i = 0; i < metadata->label_count; ++i) {
			const struct prototype_compile_label* label = &metadata->labels[i];
			printf("term %s := ", symbol_to_string(symbols, label->name_symbol_id));
			prototype_term_print_debug(stdout, symbols, type_declarations, term_db, label->term);
			printf("\n");
		}
	}
	printf("\n#### Judgements ####\n");
	prototype_judgement_print(stdout, symbols, type_declarations, term_db, judgement_db);
	printf(
		"\n"
		"#### Metadata ####\n"
		"labels=%zu resolve_errors=%zu self_contained=%s\n",
		metadata ? metadata->label_count : 0,
		metadata ? metadata->resolve_error_count : 0,
		metadata && metadata->resolve_error_count == 0 ? "yes" : "no"
	);
	if (metadata) {
		for (size_t i = 0; i < metadata->label_count; ++i) {
			const struct prototype_compile_label* label = &metadata->labels[i];
			printf("metadata label %s -> term#%u\n",
				symbol_to_string(symbols, label->name_symbol_id),
				label->term);
		}
		for (size_t i = 0; i < metadata->resolve_error_count; ++i) {
			const struct prototype_resolve_error* resolve_error = &metadata->resolve_errors[i];
			printf("metadata resolve-error kind=%s name=%s",
				resolve_error_kind_name(resolve_error->kind),
				symbol_to_string(symbols, resolve_error->name_symbol_id));
			if (resolve_error->member_symbol_id >= 0) {
				printf(".%s", symbol_to_string(symbols, resolve_error->member_symbol_id));
			}
			printf(
				" ast#%u span=%u:%u\n",
				resolve_error->ast,
				resolve_error->span.line,
				resolve_error->span.column
			);
		}
	}
	printf("\n#### Resolution ####\n");
	print_resolution_trace(symbols, type_declarations, term_db, metadata);
	printf("\n#### Universe ####\n");
	print_universe_graph(symbols, type_declarations, universe_db);
}

static int entry_complete(const char* input) {
	int brace_depth = 0;
	int saw_top_level_semicolon = 0;

	for (size_t i = 0; input[i] != '\0'; ++i) {
		if (input[i] == '/' && input[i + 1] == '/') {
			while (input[i] != '\0' && input[i] != '\n') {
				i++;
			}
			if (input[i] == '\0') {
				break;
			}
		}
		if (input[i] == '/' && input[i + 1] == '*') {
			i += 2;
			while (input[i] != '\0') {
				if (input[i] == '*' && input[i + 1] == '/') {
					i++;
					break;
				}
				i++;
			}
			continue;
		}
		if (input[i] == '{') {
			brace_depth++;
		} else if (input[i] == '}' && brace_depth > 0) {
			brace_depth--;
		} else if (input[i] == ';' && brace_depth == 0) {
			saw_top_level_semicolon = 1;
		}
	}

	return saw_top_level_semicolon && brace_depth == 0;
}

static int append_line(char* input, size_t* input_len, const char* line) {
	size_t line_len = strlen(line);
	if (*input_len + line_len + 1 > INPUT_CAPACITY) {
		return -1;
	}
	memcpy(input + *input_len, line, line_len + 1);
	*input_len += line_len;
	return 0;
}

static int is_query_line(const char* line, char* name, size_t name_capacity) {
	size_t start = 0;
	size_t end;

	while (line[start] == ' ' || line[start] == '\t') {
		start++;
	}
	end = start;
	if (!((line[end] >= 'a' && line[end] <= 'z') || (line[end] >= 'A' && line[end] <= 'Z') || line[end] == '_')) {
		return 0;
	}
	while (
		(line[end] >= 'a' && line[end] <= 'z') ||
		(line[end] >= 'A' && line[end] <= 'Z') ||
		(line[end] >= '0' && line[end] <= '9') ||
		line[end] == '_'
	) {
		end++;
	}
	size_t tail = end;
	while (line[tail] == ' ' || line[tail] == '\t' || line[tail] == '\r' || line[tail] == '\n') {
		tail++;
	}
	if (line[tail] != '\0') {
		return 0;
	}

	if (end - start + 1 > name_capacity) {
		return 0;
	}
	memcpy(name, line + start, end - start);
	name[end - start] = '\0';
	return 1;
}

static int is_named_command(
	const char* line,
	const char* command,
	char* name,
	size_t name_capacity
) {
	if (!line || !command || !name || name_capacity == 0) {
		return 0;
	}
	size_t command_len = strlen(command);
	if (strncmp(line, command, command_len) != 0 ||
		(line[command_len] != ' ' && line[command_len] != '\t')) {
		return 0;
	}
	return is_query_line(line + command_len, name, name_capacity);
}

static int evaluate_for_output(
	FILE* output,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	uint32_t term,
	uint32_t* p_ret,
	int* p_host_ran
) {
	if (!output || !symbols || !type_declarations || !term_db || !p_ret || !p_host_ran) {
		return -1;
	}

	*p_host_ran = 0;
	return prototype_term_perform_with_options(
		term_db,
		type_declarations,
		NULL,
		(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_EVALUATE_DEFAULT |
					PROTOTYPE_TERM_PERFORM_HOST_EFFECT,
			.effect_output = output,
			.symbols = symbols,
			.effect_capabilities = PROTOTYPE_HOST_EFFECT_TERMINAL,
			.p_effect_performed = p_host_ran
		},
		term,
		p_ret
	);
}

static void query_value(
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata,
	const char* name
) {
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	const struct prototype_compile_label* label = lookup_label(metadata, symbol_id);
	uint32_t evaluated;

	if (!label) {
		printf("%s is not defined\n", name);
		return;
	}

	printf("term %s := ", name);
	prototype_term_print_debug(stdout, symbols, type_declarations, term_db, label->term);
	printf("\n");

	int host_ran;
	if (evaluate_for_output(
			stdout,
			symbols,
			type_declarations,
			term_db,
			label->term,
			&evaluated,
			&host_ran
		) != 0) {
		printf("value %s := <evaluation failed>\n", name);
		return;
	}
	if (host_ran) {
		printf("\n");
	}
	printf("value %s := ", name);
	prototype_term_print_debug(stdout, symbols, type_declarations, term_db, evaluated);
	printf("\n");
}

static void query_normal_form(
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata,
	const char* name,
	int full
) {
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	const struct prototype_compile_label* label = lookup_label(metadata, symbol_id);
	uint32_t normalized;
	const char* mode_name = full ? "nf" : "whnf";
	struct prototype_term_reduction_options options = {
		.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
	};

	if (!label) {
		printf("%s is not defined\n", name);
		return;
	}
	int status = full ?
		prototype_term_nf_with_options(
			term_db, type_declarations, NULL, options, label->term, &normalized
		) :
		prototype_term_whnf_with_options(
			term_db, type_declarations, NULL, options, label->term, &normalized
		);
	if (status != 0) {
		printf("%s %s := <normalization failed>\n", mode_name, name);
		return;
	}
	printf("%s %s := ", mode_name, name);
	prototype_term_print_debug(stdout, symbols, type_declarations, term_db, normalized);
	printf("\n");
}

static int query_existing_value(
	const struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata,
	int symbol_id
) {
	const struct prototype_compile_label* label = lookup_label(metadata, symbol_id);
	uint32_t evaluated;
	const char* name;

	if (!label) {
		return 0;
	}

	name = symbol_to_string(symbols, symbol_id);
	printf("term %s := ", name ? name : "<unknown>");
	prototype_term_print_debug(stdout, symbols, type_declarations, term_db, label->term);
	printf("\n");

	int host_ran;
	if (evaluate_for_output(
			stdout,
			(struct symbol_table*)symbols,
			type_declarations,
			term_db,
			label->term,
			&evaluated,
			&host_ran
		) != 0) {
		printf("value %s := <evaluation failed>\n", name ? name : "<unknown>");
		return 1;
	}
	if (host_ran) {
		printf("\n");
	}
	printf("value %s := ", name ? name : "<unknown>");
	prototype_term_print_debug(stdout, symbols, type_declarations, term_db, evaluated);
	printf("\n");
	return 1;
}

int main(int argc, char** argv) {
	struct symbol_table symbols;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_ast_db ast_db;
	struct prototype_term_db term_db;
	struct prototype_universe_db universe_db;
	struct prototype_judgement_db judgement_db;
	struct prototype_compile_metadata metadata;
	struct prototype_program program;
	struct prototype_read_options read_options;
	struct prototype_read_error error;
	char input[INPUT_CAPACITY];
	size_t input_len = 0;
	unsigned entry_index = 1;
	int first_file_arg = 1;
	int disable_automatic_cbpv_coercions = 0;

	memset(&read_options, 0, sizeof(read_options));
	for (; first_file_arg < argc && argv[first_file_arg][0] == '-'; ++first_file_arg) {
		if (strcmp(argv[first_file_arg], "--automatic-cbpv-coercions") == 0) {
			disable_automatic_cbpv_coercions = 0;
			continue;
		}
		if (strcmp(argv[first_file_arg], "--no-automatic-cbpv-coercions") == 0) {
			disable_automatic_cbpv_coercions = 1;
			continue;
		}
		fprintf(stderr, "unknown option: %s\n", argv[first_file_arg]);
		fprintf(stderr, "Usage: %s [--automatic-cbpv-coercions|--no-automatic-cbpv-coercions] [file.p ...]\n", argv[0]);
		return 1;
	}

	symbol_table_init(
		&symbols,
		symbol_ids,
		symbol_hashes,
		SYMBOL_MAP_CAPACITY,
		symbol_strings,
		SYMBOL_STORAGE_CAPACITY
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
	program.compile_options.disable_automatic_cbpv_coercions =
		disable_automatic_cbpv_coercions;

	for (int i = first_file_arg; i < argc; ++i) {
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
		if (prototype_compile_graph(&program, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : argv[i],
				error.line,
				error.column,
				error.message[0] ? error.message : "graph compile failed"
			);
			print_metadata_resolve_errors(stderr, &symbols, &metadata);
			symbol_table_free(&symbols);
			return 1;
		}
	}
	int main_symbol = symbol_intern(&symbols, "main", 4);
	if (main_symbol < 0) {
		fprintf(stderr, "failed to intern main symbol\n");
		symbol_table_free(&symbols);
		return 1;
	}
	print_state(&symbols, &ast_db, &type_declarations, &term_db, &universe_db, &judgement_db, &metadata);
	if (lookup_label(&metadata, main_symbol)) {
		query_existing_value(&symbols, &type_declarations, &term_db, &metadata, main_symbol);
	}

	printf("prototype> ");
	fflush(stdout);

	input[0] = '\0';
	for (;;) {
		char line[LINE_CAPACITY];
		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		if (input_len == 0 && (strcmp(line, ":quit\n") == 0 || strcmp(line, ":q\n") == 0)) {
			break;
		}
		if (input_len == 0 && strcmp(line, ":state\n") == 0) {
			print_state(&symbols, &ast_db, &type_declarations, &term_db, &universe_db, &judgement_db, &metadata);
			printf("prototype> ");
			fflush(stdout);
			continue;
		}
		if (input_len == 0 && strcmp(line, ":ast\n") == 0) {
			prototype_ast_inspect_print(stdout, &symbols, &ast_db);
			printf("prototype> ");
			fflush(stdout);
			continue;
		}
		if (input_len == 0) {
			char query_name[128];
			if (is_named_command(line, ":whnf", query_name, sizeof(query_name))) {
				query_normal_form(
					&symbols, &type_declarations, &term_db, &metadata, query_name, 0
				);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
			if (is_named_command(line, ":nf", query_name, sizeof(query_name))) {
				query_normal_form(
					&symbols, &type_declarations, &term_db, &metadata, query_name, 1
				);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
			if (is_query_line(line, query_name, sizeof(query_name))) {
				query_value(&symbols, &type_declarations, &term_db, &metadata, query_name);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
		}

		if (append_line(input, &input_len, line) != 0) {
			fprintf(stderr, "<interactive>:%u:1: input buffer is full\n", entry_index);
			input[0] = '\0';
			input_len = 0;
			printf("prototype> ");
			fflush(stdout);
			continue;
		}

		if (!entry_complete(input)) {
			printf("... ");
			fflush(stdout);
			continue;
		}

		char name[48];
		snprintf(name, sizeof(name), "<interactive:%u>", entry_index++);
		if (prototype_read_ast_string_with_options(name, input, &program, &read_options, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : name,
				error.line,
				error.column,
				error.message[0] ? error.message : "read failed"
			);
		} else if (prototype_compile_graph(&program, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : name,
				error.line,
				error.column,
				error.message[0] ? error.message : "graph compile failed"
			);
			print_metadata_resolve_errors(stderr, &symbols, &metadata);
		} else {
			print_state(&symbols, &ast_db, &type_declarations, &term_db, &universe_db, &judgement_db, &metadata);
		}

		input[0] = '\0';
		input_len = 0;
		printf("prototype> ");
		fflush(stdout);
	}

	symbol_table_free(&symbols);
	return 0;
}
