#ifndef __PROTOTYPE_DRIVER_DIAGNOSTICS_H__
#define __PROTOTYPE_DRIVER_DIAGNOSTICS_H__

#include <stdio.h>

struct prototype_compile_metadata;
struct prototype_intrinsic_environment;
struct prototype_term_db;
struct prototype_type_declaration;
struct prototype_type_declaration_db;
struct prototype_universe_db;
struct symbol_table;

const char* prototype_diagnostic_resolve_error_kind_name(int kind);

void prototype_diagnostic_print_resolve_errors(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_compile_metadata* metadata
);

void prototype_diagnostic_print_compile_diagnostics(
	FILE* stream,
	const struct prototype_compile_metadata* metadata
);

void prototype_diagnostic_print_resolution_trace(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_compile_metadata* metadata
);

void prototype_diagnostic_print_type_namespace(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_type_declaration* type
);

void prototype_diagnostic_print_type_declaration(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_type_declaration* type
);

void prototype_diagnostic_print_universe_graph(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_universe_db* universe,
	int print_primitive_type_exprs
);

#endif
