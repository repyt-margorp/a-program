#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_FUNCTION_GRAPH_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_FUNCTION_GRAPH_H

#include <stdio.h>
#include <stdint.h>

#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/judgement/db.h"

/* This view owns no semantic data. A frozen accepted module owns every pointer
 * and stable ID exposed here. */
struct prototype_accepted_definition_view {
	const struct prototype_ast_db* asts;
	const struct prototype_term_db* terms;
	const struct prototype_type_declaration_db* type_declarations;
	const struct prototype_judgement_db* judgement;
	const struct prototype_compile_metadata* metadata;
	uint32_t assignment_id;
	uint32_t source_entry_id;
	uint32_t root_ast;
	uint32_t root_occurrence;
	uint32_t classifier;
	uint32_t final_result_type;
	uint32_t final_effect_row;
	int final_totality;
};

int prototype_function_graph_generate_requested(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols
);

int prototype_function_graph_finalize_associations(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_compile_metadata* metadata
);

enum prototype_function_graph_inspection_state {
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AVAILABLE = 0,
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_ABSENT = 1,
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_RESIDUAL = 2,
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AMBIGUOUS = 3,
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_UNEXPORTED = 4,
	PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID = 5
};

/* Register one driver-originated request before graph compilation. Imported
 * owners intentionally return ABSENT here and are resolved during import. */
enum prototype_function_graph_inspection_state
prototype_function_graph_request_inspection(
	const struct prototype_ast_db* asts,
	struct prototype_compile_metadata* metadata,
	int owner_symbol_id
);

/* Print only the accepted frozen interface. This routine performs no graph
 * generation, synthesis, normalization, or proof construction. */
enum prototype_function_graph_inspection_state prototype_function_graph_inspect(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	int owner_symbol_id
);

const char* prototype_function_graph_inspection_state_name(
	enum prototype_function_graph_inspection_state state
);

#endif
