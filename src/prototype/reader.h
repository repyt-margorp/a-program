#ifndef __PROTOTYPE_READER_H__
#define __PROTOTYPE_READER_H__

#include "ast.h"
#include "symbol.h"
#include "term.h"
#include "judgement.h"
#include "type_declaration.h"
#include "universe.h"

struct prototype_program {
	struct symbol_table* symbols;
	/* The namespace of the unit currently being compiled, or -1 when unset. */
	int namespace_symbol_id;
	struct prototype_ast_db* asts;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_term_db* terms;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct prototype_universe_db* universe;
};

struct prototype_read_error {
	const char* filename;
	unsigned line;
	unsigned column;
	char message[160];
};

struct prototype_read_options {
	int forbid_standalone_expectations;
};

int prototype_read_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_file_with_options(
	const char* path,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
);

int prototype_read_ast_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_string_with_options(
	const char* name,
	const char* input,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
);

int prototype_compile_graph(
	struct prototype_program* program,
	struct prototype_read_error* error
);
int prototype_compile_graph_with_imports(
	struct prototype_program* program,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_read_error* error
);

int prototype_link_external_refs(struct prototype_program* program);

#endif
