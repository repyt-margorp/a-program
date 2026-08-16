#include "a_program/frontend/reader.h"

#include "a_program/driver/compiler_session.h"
#include "a_program/driver/diagnostics.h"
#include "a_program/graph/runtime.h"

#include <stdio.h>
#include <string.h>

#include "a_program/frontend/ast_inspect.h"
#include "a_program/support/symbol.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/universe.h"

#define INPUT_CAPACITY 8192
#define LINE_CAPACITY 1024

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

static void label_evaluation_root(
	const struct prototype_compile_metadata* metadata,
	int symbol_id,
	const struct prototype_compile_label* label,
	uint32_t* p_operation,
	uint32_t* p_term
) {
	*p_operation = label->exposed_occurrence;
	*p_term = label->term;
	if (metadata->selected_entry_symbol_id == symbol_id &&
		metadata->selected_entry_occurrence < metadata->typed_occurrences.occurrence_count &&
		metadata->selected_entry_term != PROTOTYPE_INVALID_ID) {
		*p_operation = metadata->selected_entry_occurrence;
		*p_term = metadata->selected_entry_term;
	}
}

static void print_state(
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
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
		prototype_diagnostic_print_type_declaration(stdout, symbols, type_declarations, type);
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			uint32_t constructor_id = type->first_constructor + j;
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[constructor_id];
			const struct prototype_type_constructor_readback* readback =
				prototype_type_constructor_readback_get(type_declarations, constructor_id);
			const struct prototype_constructor_classifier_cache_entry* cache =
				prototype_type_constructor_classifier_cache_get(
					type_declarations, constructor_id
				);
			if (!readback || !cache) {
				continue;
			}
			printf("constructor ");
			prototype_diagnostic_print_type_namespace(stdout, symbols, type_declarations, type);
			printf(".%s readback_fields=%u curried_classifier_cache=%u\n",
				symbol_to_string(symbols, constructor->name_symbol_id),
				readback->field_count,
				cache->classifier);
		}
	}
	if (metadata) {
		for (size_t i = 0; i < metadata->label_count; ++i) {
			const struct prototype_compile_label* label = &metadata->labels[i];
			printf("term %s := ", symbol_to_string(symbols, label->name_symbol_id));
			prototype_term_print_debug(
				stdout, symbols, intrinsic_environment,
				type_declarations, term_db, label->term
			);
			printf("\n");
		}
	}
	printf("\n#### Judgements ####\n");
	prototype_judgement_print(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, judgement_db
	);
	printf(
		"\n"
		"#### Metadata ####\n"
		"labels=%zu resolve_errors=%zu self_contained=%s\n"
		"typed-occurrences=%zu terms=%zu propositions=%zu claims=%zu derivations=%zu "
		"normalization-budget=%llu/%llu solver-budget=%llu/%llu\n",
		metadata ? metadata->label_count : 0,
		metadata ? metadata->resolve_error_count : 0,
		metadata && metadata->resolve_error_count == 0 ? "yes" : "no",
		metadata ? metadata->typed_occurrences.occurrence_count : 0,
		term_db->term_count,
		judgement_db->proposition_count,
		judgement_db->claim_count,
		judgement_db->derivation_count,
		(unsigned long long)(metadata ? metadata->normalization_steps_used : 0),
		(unsigned long long)(metadata ? metadata->normalization_step_limit : 0),
		(unsigned long long)(metadata ? metadata->solver_steps_used : 0),
		(unsigned long long)(metadata ? metadata->solver_step_limit : 0)
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
				prototype_diagnostic_resolve_error_kind_name(resolve_error->kind),
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
	prototype_diagnostic_print_resolution_trace(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, metadata
	);
	printf("\n#### Universe ####\n");
	prototype_diagnostic_print_universe_graph(
		stdout, symbols, type_declarations, universe_db, 0
	);
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
	struct prototype_compile_metadata* metadata,
	uint32_t operation,
	uint32_t term,
	uint32_t* p_ret,
	int* p_host_ran,
	int* p_verification_state
) {
	if (!output || !symbols || !type_declarations || !term_db || !p_ret || !p_host_ran ||
		!p_verification_state) {
		return -1;
	}

	*p_host_ran = 0;
	*p_verification_state = 0;
	struct prototype_frozen_module_snapshot snapshot;
	int has_snapshot = metadata &&
		prototype_compile_metadata_frozen_snapshot(metadata, &snapshot) == 0;
	if (metadata && !has_snapshot) {
		return -1;
	}
	struct prototype_term_reduction_options options = {
		.flags = PROTOTYPE_TERM_EVALUATE_DEFAULT |
			PROTOTYPE_TERM_PERFORM_HOST_EFFECT,
		.effect_output = output,
		.symbols = symbols,
		.reduction_environment = has_snapshot ?
			&snapshot.reduction_environment : NULL,
		.effect_capabilities = PROTOTYPE_HOST_EFFECT_TERMINAL,
		.p_effect_performed = p_host_ran
	};
	if (has_snapshot && operation < snapshot.typed_occurrences.occurrence_count) {
		struct prototype_runtime_trace trace;
		const struct prototype_runtime_annotations annotations = {
			.occurrences = &snapshot.typed_occurrences,
			.verification = &snapshot.verification,
			.verification_type_declarations = type_declarations
		};
		int status = prototype_runtime_evaluate_core_with_annotations(
			&annotations,
			term_db,
			NULL,
			options,
			term,
			operation,
			p_ret,
			p_verification_state,
			&trace
		);
		if (status != 0) {
			fprintf(
				output,
				"runtime failure kind=%d occurrence#%u\n",
				trace.failure_kind,
				trace.failed_occurrence
			);
			for (uint32_t i = 0; i < trace.frame_count; ++i) {
				fprintf(
					output,
					"runtime frame %u kind=%d occurrence#%u\n",
					i,
					trace.frame_kinds[i],
					trace.frame_occurrences[i]
				);
			}
		}
		return status;
	}
	return prototype_term_perform_with_options(
		term_db,
		NULL,
		NULL,
		options,
		term,
		p_ret
	);
}

static void query_value(
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	struct prototype_compile_metadata* metadata,
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
	prototype_term_print_debug(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, label->term
	);
	printf("\n");

	int host_ran;
	int verification_state;
	uint32_t evaluation_operation;
	uint32_t evaluation_term;
	label_evaluation_root(
		metadata,
		symbol_id,
		label,
		&evaluation_operation,
		&evaluation_term
	);
	if (evaluate_for_output(
			stdout,
			symbols,
			type_declarations,
			term_db,
			metadata,
			evaluation_operation,
			evaluation_term,
			&evaluated,
			&host_ran,
			&verification_state
		) != 0) {
		printf("value %s := <evaluation failed>\n", name);
		return;
	}
	if (host_ran) {
		printf("\n");
	}
	if (verification_state == PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
		printf("verification %s := discharged\n", name);
	}
	printf("value %s := ", name);
	prototype_term_print_debug(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, evaluated
	);
	printf("\n");
}

static void query_normal_form(
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	struct prototype_compile_metadata* metadata,
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
	uint32_t evaluation_operation;
	uint32_t evaluation_term;
	label_evaluation_root(
		metadata,
		symbol_id,
		label,
		&evaluation_operation,
		&evaluation_term
	);
	(void)evaluation_operation;
	int status = full ?
		prototype_term_nf_with_options(
			term_db, type_declarations, NULL, options, evaluation_term, &normalized
		) :
		prototype_term_perform_with_options(
			term_db, type_declarations, NULL, options, evaluation_term, &normalized
		);
	if (status != 0) {
		printf("%s %s := <normalization failed>\n", mode_name, name);
		return;
	}
	printf("%s %s := ", mode_name, name);
	prototype_term_print_debug(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, normalized
	);
	printf("\n");
}

static void query_type(
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata,
	const char* name
) {
	int symbol_id = -1;
	if (!symbols || !type_declarations || !term_db || !metadata || !name) {
		return;
	}
	for (size_t i = metadata->label_count; i > 0; --i) {
		const char* candidate = symbol_to_string(
			symbols, metadata->labels[i - 1].name_symbol_id
		);
		if (candidate && strcmp(candidate, name) == 0) {
			symbol_id = metadata->labels[i - 1].name_symbol_id;
			break;
		}
	}
	struct prototype_type_inspection inspection;
	enum prototype_type_inspection_state state = symbol_id < 0 ?
		PROTOTYPE_TYPE_INSPECTION_UNAVAILABLE :
		prototype_compile_metadata_inspect_type(
			metadata, symbol_id, &inspection
		);
	if (state == PROTOTYPE_TYPE_INSPECTION_AMBIGUOUS) {
		printf("%s has ambiguous type inspection metadata\n", name);
		return;
	}
	if (state != PROTOTYPE_TYPE_INSPECTION_AVAILABLE) {
		printf("%s has no available type inspection\n", name);
		return;
	}
	printf("type %s := ", name);
	prototype_term_print_debug(
		stdout,
		symbols,
		intrinsic_environment,
		type_declarations,
		term_db,
		inspection.body_classifier
	);
	printf("\n");
	if (inspection.expectation_classifier != PROTOTYPE_INVALID_ID) {
		printf("expected %s := ", name);
		prototype_term_print_debug(
			stdout,
			symbols,
			intrinsic_environment,
			type_declarations,
			term_db,
			inspection.expectation_classifier
		);
		printf(
			" [accepted claim#%u]\n",
			inspection.expectation_claim_id
		);
	}
}

static int query_existing_value(
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* term_db,
	struct prototype_compile_metadata* metadata,
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
	prototype_term_print_debug(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, label->term
	);
	printf("\n");

	int host_ran;
	int verification_state;
	uint32_t evaluation_operation;
	uint32_t evaluation_term;
	label_evaluation_root(
		metadata,
		symbol_id,
		label,
		&evaluation_operation,
		&evaluation_term
	);
	if (evaluate_for_output(
			stdout,
			(struct symbol_table*)symbols,
			type_declarations,
			term_db,
			metadata,
			evaluation_operation,
			evaluation_term,
			&evaluated,
			&host_ran,
			&verification_state
		) != 0) {
		printf("value %s := <evaluation failed>\n", name ? name : "<unknown>");
		return 1;
	}
	if (host_ran) {
		printf("\n");
	}
	if (verification_state == PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED) {
		printf("verification %s := discharged\n", name ? name : "<unknown>");
	}
	printf("value %s := ", name ? name : "<unknown>");
	prototype_term_print_debug(
		stdout, symbols, intrinsic_environment,
		type_declarations, term_db, evaluated
	);
	printf("\n");
	return 1;
}

int main(int argc, char** argv) {
	struct prototype_program_storage storage;
	struct symbol_table* symbols;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_ast_db* ast_db;
	struct prototype_term_db* term_db;
	struct prototype_universe_db* universe_db;
	struct prototype_judgement_db* judgement_db;
	struct prototype_compile_metadata* metadata;
	struct prototype_program* program;
	struct prototype_read_options read_options;
	struct prototype_read_error error;
	char input[INPUT_CAPACITY];
	size_t input_len = 0;
	unsigned entry_index = 1;
	int first_file_arg = 1;
	int definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;

	memset(&read_options, 0, sizeof(read_options));
	for (; first_file_arg < argc && argv[first_file_arg][0] == '-'; ++first_file_arg) {
		if (strcmp(argv[first_file_arg], "--implicit-definition-thunks") == 0) {
			definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;
			continue;
		}
		if (strcmp(argv[first_file_arg], "--no-implicit-definition-thunks") == 0) {
			definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_EXPLICIT;
			continue;
		}
		fprintf(stderr, "unknown option: %s\n", argv[first_file_arg]);
		fprintf(stderr, "Usage: %s [--implicit-definition-thunks|--no-implicit-definition-thunks] [file.p ...]\n", argv[0]);
		return 1;
	}

	if (prototype_program_storage_init(&storage) != 0) {
		fprintf(stderr, "failed to initialize compiler storage\n");
		return 1;
	}
	symbols = &storage.symbols;
	type_declarations = &storage.type_declarations;
	ast_db = &storage.asts;
	term_db = &storage.terms;
	universe_db = &storage.universe;
	judgement_db = &storage.judgement;
	metadata = &storage.metadata;
	program = &storage.program;
	program->compile_options.definition_thunk_policy = definition_thunk_policy;
	for (int i = first_file_arg; i < argc; ++i) {
		if (prototype_read_ast_file_with_options(argv[i], program, &read_options, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : argv[i],
				error.line,
				error.column,
				error.message[0] ? error.message : "read failed"
			);
			prototype_program_storage_destroy(&storage);
			return 1;
		}
		if (prototype_compile_graph(program, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : argv[i],
				error.line,
				error.column,
				error.message[0] ? error.message : "graph compile failed"
			);
			prototype_diagnostic_print_resolve_errors(stderr, symbols, metadata);
			prototype_diagnostic_print_compile_diagnostics(stderr, metadata);
			prototype_program_storage_destroy(&storage);
			return 1;
		}
	}
	int main_symbol = metadata->selected_entry_symbol_id >= 0 ?
		metadata->selected_entry_symbol_id : symbol_intern(symbols, "main", 4);
	if (main_symbol < 0) {
		fprintf(stderr, "failed to intern main symbol\n");
		prototype_program_storage_destroy(&storage);
		return 1;
	}
	print_state(
		symbols, program->intrinsic_environment, ast_db, type_declarations,
		term_db, universe_db, judgement_db, metadata
	);
	if (lookup_label(metadata, main_symbol)) {
		query_existing_value(
			symbols, program->intrinsic_environment,
			type_declarations, term_db, metadata, main_symbol
		);
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
			print_state(
				symbols, program->intrinsic_environment, ast_db,
				type_declarations, term_db, universe_db, judgement_db, metadata
			);
			printf("prototype> ");
			fflush(stdout);
			continue;
		}
		if (input_len == 0 && strcmp(line, ":ast\n") == 0) {
			prototype_ast_inspect_print(stdout, symbols, ast_db);
			printf("prototype> ");
			fflush(stdout);
			continue;
		}
		if (input_len == 0) {
			char query_name[128];
			if (is_named_command(line, ":type", query_name, sizeof(query_name))) {
				query_type(
					symbols, program->intrinsic_environment,
					type_declarations, term_db, metadata, query_name
				);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
			if (is_named_command(line, ":whnf", query_name, sizeof(query_name))) {
				query_normal_form(
					symbols, program->intrinsic_environment,
					type_declarations, term_db, metadata, query_name, 0
				);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
			if (is_named_command(line, ":nf", query_name, sizeof(query_name))) {
				query_normal_form(
					symbols, program->intrinsic_environment,
					type_declarations, term_db, metadata, query_name, 1
				);
				printf("prototype> ");
				fflush(stdout);
				continue;
			}
			if (is_query_line(line, query_name, sizeof(query_name))) {
				query_value(
					symbols, program->intrinsic_environment,
					type_declarations, term_db, metadata, query_name
				);
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
		if (prototype_read_ast_string_with_options(name, input, program, &read_options, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : name,
				error.line,
				error.column,
				error.message[0] ? error.message : "read failed"
			);
		} else if (prototype_compile_graph(program, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : name,
				error.line,
				error.column,
				error.message[0] ? error.message : "graph compile failed"
			);
			prototype_diagnostic_print_resolve_errors(stderr, symbols, metadata);
			prototype_diagnostic_print_compile_diagnostics(stderr, metadata);
		} else {
			print_state(
				symbols, program->intrinsic_environment, ast_db,
				type_declarations, term_db, universe_db, judgement_db, metadata
			);
		}

		input[0] = '\0';
		input_len = 0;
		printf("prototype> ");
		fflush(stdout);
	}

	prototype_program_storage_destroy(&storage);
	return 0;
}
