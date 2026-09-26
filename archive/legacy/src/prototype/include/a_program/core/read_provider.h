#ifndef A_PROGRAM_PROTOTYPE_CORE_READ_PROVIDER_H
#define A_PROGRAM_PROTOTYPE_CORE_READ_PROVIDER_H

#include "a_program/core/read.h"

struct prototype_term_db;

/* Core/provider-only snapshot acquisition. Layer T obtains snapshots through
 * the incomplete request client and never supplies a TermDB pointer. */
int prototype_core_read_snapshot_begin(
	const struct prototype_term_db* terms,
	struct prototype_core_read_snapshot* snapshot
);

#endif
