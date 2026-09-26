#ifndef __PROTOTYPE_DRIVER_COMPILER_SESSION_H__
#define __PROTOTYPE_DRIVER_COMPILER_SESSION_H__

#include <stddef.h>
#include <stdint.h>

#include "a_program/artifact/interface.h"

struct prototype_artifact_interface;
struct prototype_ast_db;
struct prototype_intrinsic_typing_environment;
struct prototype_program;
struct prototype_program_storage_private;
struct prototype_read_error;
struct symbol_table;

struct prototype_compile_options {
	int compile_policy;
	int definition_thunk_policy;
	int solve_effort_is_set;
	uint64_t solve_effort_steps;
};

/* Parser-visible source capability. It deliberately exposes neither semantic
 * pipeline nor any mutable calculation/typing store. */
struct prototype_program_source_view {
	const struct prototype_intrinsic_typing_environment* intrinsic_environment;
	struct symbol_table* symbols;
	struct prototype_ast_db* asts;
};

int prototype_program_source_view(
	struct prototype_program* program,
	struct prototype_program_source_view* view
);

struct prototype_artifact_interface_storage_backing;

/* Opaque lifetime handle for one compiler session. The concrete Layer C and
 * Layer T owners are intentionally unavailable through this public header. */
struct prototype_program_storage {
	struct prototype_program_storage_private* private;
};

int prototype_program_storage_init(struct prototype_program_storage* storage);
int prototype_program_storage_reset(struct prototype_program_storage* storage);
void prototype_program_storage_exchange(
	struct prototype_program_storage* left,
	struct prototype_program_storage* right
);
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
void prototype_artifact_interface_storage_exchange(
	struct prototype_artifact_interface_storage* left,
	struct prototype_artifact_interface_storage* right
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

#endif
