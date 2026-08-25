#ifndef __PROTOTYPE_DRIVER_COMPILER_SESSION_H__
#define __PROTOTYPE_DRIVER_COMPILER_SESSION_H__

#include <stddef.h>
#include <stdint.h>

#include "a_program/artifact/interface.h"
#include "a_program/frontend/lowering.h"
#include "a_program/kernel/universe.h"
#include "a_program/support/symbol.h"

struct prototype_artifact_interface;
struct prototype_read_error;

struct prototype_compile_options {
	int compile_policy;
	int definition_thunk_policy;
	int effort_limit_is_set;
	uint64_t effort_limit;
};

/* Driver-owned composition root. Parser APIs receive this object but do not
 * own semantic initialization, system declarations, linking, or solving. */
struct prototype_program {
	const struct prototype_intrinsic_environment* intrinsic_environment;
	struct symbol_table* symbols;
	int namespace_symbol_id;
	struct prototype_ast_db* asts;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_term_db* terms;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct prototype_universe_db* universe;
	struct prototype_compile_options compile_options;
};

struct prototype_program_storage_backing;
struct prototype_artifact_interface_storage_backing;

/* Owns the typed backing arrays for one compiler session. The semantic DBs
 * remain separate typed stores; the backing object only centralizes lifetime. */
struct prototype_program_storage {
	struct prototype_program program;
	struct symbol_table symbols;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_ast_db asts;
	struct prototype_term_db terms;
	struct prototype_judgement_db judgement;
	struct prototype_compile_metadata metadata;
	struct prototype_universe_db universe;
	struct prototype_program_storage_backing* backing;
};

int prototype_program_storage_init(struct prototype_program_storage* storage);
int prototype_program_storage_reset(struct prototype_program_storage* storage);
void prototype_program_storage_destroy(struct prototype_program_storage* storage);

/* Owns one artifact interface image and its typed relocation/debug/readback
 * backing. It is a lifetime bundle, not a semantic database. */
struct prototype_artifact_interface_storage {
	struct prototype_artifact_interface interface;
	struct prototype_artifact_relocation_table relocation;
	struct prototype_artifact_debug_table debug;
	struct prototype_artifact_interface_storage_backing* backing;
};

int prototype_artifact_interface_storage_init(
	struct prototype_artifact_interface_storage* storage
);
int prototype_artifact_interface_storage_reset(
	struct prototype_artifact_interface_storage* storage
);
void prototype_artifact_interface_storage_destroy(
	struct prototype_artifact_interface_storage* storage
);
struct prototype_term_definition*
prototype_artifact_interface_storage_definitions(
	struct prototype_artifact_interface_storage* storage
);
size_t prototype_artifact_interface_storage_definition_capacity(
	const struct prototype_artifact_interface_storage* storage
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
