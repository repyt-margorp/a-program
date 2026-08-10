#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H

#include "a_program/artifact/interface.h"
#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

int prototype_ast_compile_pending(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
);
int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
);


#endif
