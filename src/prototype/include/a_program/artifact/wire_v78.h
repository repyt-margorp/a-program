#ifndef A_PROGRAM_PROTOTYPE_ARTIFACT_WIRE_V78_H
#define A_PROGRAM_PROTOTYPE_ARTIFACT_WIRE_V78_H

#include <stdio.h>

#include "a_program/artifact/interface.h"

struct prototype_cwf_certificate_db;
struct prototype_dimension_operator_db;

int prototype_artifact_write_text(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
);

int prototype_artifact_read_text_interface(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_artifact_interface* interface
);

int prototype_artifact_read_text_graph(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_dimension_operator_db* dimension_operators,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

int prototype_artifact_read_text_typed_occurrences(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
);

int prototype_artifact_read_text_universe(
	FILE* stream,
	struct prototype_universe_db* universe
);

int prototype_artifact_read_text_debug(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_debug_table* debug
);

int prototype_artifact_read_text_relocation(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_relocation_table* relocation
);

#endif
