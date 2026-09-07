#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_LOWERING_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/artifact/interface.h"
#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

enum prototype_compilation_image_status {
	PROTOTYPE_COMPILATION_IMAGE_OPEN = 1,
	PROTOTYPE_COMPILATION_IMAGE_PAUSED = 2,
	PROTOTYPE_COMPILATION_IMAGE_SOLVED = 3,
	PROTOTYPE_COMPILATION_IMAGE_REJECTED = 4
};

/* Reachability roles over one compilation image. These sets contain only
 * references into the authoritative image databases; they are not another
 * semantic store or another compilation stage. */
enum prototype_compilation_image_root_role {
	PROTOTYPE_COMPILATION_IMAGE_ROOT_REPLAY_SEED = 1,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_SOURCE_PROVENANCE = 2,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_CHECKPOINT_PROGRESS = 3,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION = 4,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_ROLE_COUNT = 5
};

enum prototype_compilation_image_root_kind {
	PROTOTYPE_COMPILATION_IMAGE_ROOT_EQUATION = 1,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPED_OCCURRENCE = 2,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPED_PROJECTION = 3,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPE_DECLARATION = 4,
	PROTOTYPE_COMPILATION_IMAGE_ROOT_CLAIM = 5
};

struct prototype_compilation_image_root {
	int kind;
	uint32_t id;
};

struct prototype_compilation_image_root_view {
	const struct prototype_compilation_image_root* roots;
	size_t count;
	uint64_t revision;
};

struct prototype_compilation_image_report {
	int status;
	int phase;
	uint64_t effort_used;
};

/* Read-only evidence that one solved image satisfies its publication boundary.
 * It is a projection of image Authority, not a second mutable image state. */
struct prototype_compilation_image_seal {
	uint64_t constraint_topology_digest;
	uint64_t semantic_transition_count;
	uint64_t core_transaction_revision;
	uint64_t typing_transaction_revision;
	uint64_t typing_topology_revision;
	uint64_t judgement_semantic_revision;
	uint64_t term_count;
	uint64_t typed_occurrence_count;
	uint64_t claim_count;
	uint64_t derivation_count;
	uint64_t accepted_root_revision;
	uint64_t accepted_root_count;
	uint64_t accepted_root_digest;
	uint64_t accepted_evidence_digest;
};

struct prototype_compilation_image;
struct prototype_core_pipeline;
struct prototype_typing_pipeline;
struct prototype_universe_db;

/* The referenced source and semantic databases are the non-copying image
 * backing. They must outlive the image and must not be mutated independently
 * while it is active. */
int prototype_compilation_image_create(
	struct prototype_ast_db* asts,
	struct prototype_core_pipeline* core,
	struct prototype_typing_pipeline* typing,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_universe_db* universe,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_compilation_image** p_image
);

/* Lower source atomically into the open image without performing or charging a
 * semantic Solver transition. Structural limits are not Solver effort. */
int prototype_compilation_image_lower(
	struct prototype_compilation_image* image,
	struct prototype_compilation_image_report* p_report
);

/* Advance the same image through bounded ahead-of-runtime computation. This
 * entry requires a successfully lowered open image. */
int prototype_compilation_image_solve(
	struct prototype_compilation_image* image,
	uint64_t additional_effort,
	struct prototype_compilation_image_report* p_report
);

/* Validate a solved image without advancing its Solver or mutating its graph. */
int prototype_compilation_image_seal(
	const struct prototype_compilation_image* image,
	struct prototype_compilation_image_seal* p_seal
);

/* Return a read-only reachability projection for one role. Local IDs in this
 * view are valid only for this in-memory image. A persistent writer must map
 * them through its canonical section encoding. */
int prototype_compilation_image_roots(
	const struct prototype_compilation_image* image,
	int role,
	struct prototype_compilation_image_root_view* p_view
);

void prototype_compilation_image_destroy(
	struct prototype_compilation_image* image
);

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_core_pipeline* core,
	struct prototype_typing_pipeline* typing,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_universe_db* universe,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
);


#endif
