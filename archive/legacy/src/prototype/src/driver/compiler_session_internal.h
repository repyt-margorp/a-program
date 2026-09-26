#ifndef A_PROGRAM_PROTOTYPE_DRIVER_COMPILER_SESSION_INTERNAL_H
#define A_PROGRAM_PROTOTYPE_DRIVER_COMPILER_SESSION_INTERNAL_H

#include "a_program/driver/compiler_session.h"

#include "a_program/core/pipeline.h"
#include "a_program/frontend/lowering.h"
#include "a_program/frontend/typing_pipeline.h"
#include "a_program/kernel/universe.h"
#include "a_program/support/symbol.h"

struct prototype_program_storage_backing;

/* Driver-private composition root. Semantic stages receive narrow Layer C or
 * Layer T capabilities, never this aggregate. */
struct prototype_program {
	struct prototype_core_pipeline* core;
	struct prototype_typing_pipeline* typing;
	const struct prototype_intrinsic_typing_environment* intrinsic_environment;
	struct symbol_table* symbols;
	int namespace_symbol_id;
	struct prototype_ast_db* asts;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct prototype_universe_db* universe;
	struct prototype_compile_options compile_options;
};

/* Concrete lifetime storage. Backing memory is not a semantic capability;
 * mutable Layer C and Layer T owners remain distinct fields. */
struct prototype_program_storage_private {
	struct prototype_program program;
	struct symbol_table symbols;
	struct prototype_core_pipeline core;
	struct prototype_typing_pipeline* typing;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_ast_db asts;
	struct prototype_judgement_db judgement;
	struct prototype_compile_metadata metadata;
	struct prototype_universe_db universe;
	struct prototype_program_storage_backing* backing;
};

#endif
