#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H

#include "a_program/artifact/interface.h"
#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

enum prototype_compile_producer_status {
	PROTOTYPE_COMPILE_PRODUCER_COMPLETE = 1,
	PROTOTYPE_COMPILE_PRODUCER_PAUSED = 2,
	PROTOTYPE_COMPILE_PRODUCER_REJECTED = 3
};

struct prototype_compile_producer_report {
	int status;
	int phase;
	uint64_t effort_used;
};

struct prototype_compile_producer_session;

/* The referenced source and semantic databases must outlive the session and
 * must not be mutated by another compiler while the session is active. */
int prototype_compile_producer_session_create(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_compile_producer_session** p_session
);

int prototype_compile_producer_session_advance(
	struct prototype_compile_producer_session* session,
	uint64_t additional_effort,
	struct prototype_compile_producer_report* p_report
);

void prototype_compile_producer_session_destroy(
	struct prototype_compile_producer_session* session
);

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
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
);


#endif
