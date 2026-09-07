#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_EPOCH_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_EPOCH_H

#include <stdint.h>

#include "a_program/frontend/ast.h"

/*
 * Driver-owned source coordination storage. It is neither a Layer C graph nor
 * a Layer T graph. A generated epoch starts as an exact copy of one sealed
 * source epoch and may then be extended independently.
 */
struct prototype_source_epoch_storage {
	struct prototype_ast_db asts;
	uint64_t parent_fingerprint;
	int initialized;
};

int prototype_source_epoch_clone(
	const struct prototype_ast_db* source,
	struct prototype_source_epoch_storage* p_epoch
);

int prototype_source_epoch_parent_matches(
	const struct prototype_source_epoch_storage* epoch,
	const struct prototype_ast_db* parent
);

void prototype_source_epoch_destroy(
	struct prototype_source_epoch_storage* epoch
);

#endif
