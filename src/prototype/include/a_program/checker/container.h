#ifndef A_PROGRAM_PROTOTYPE_CHECKER_CONTAINER_H
#define A_PROGRAM_PROTOTYPE_CHECKER_CONTAINER_H

#include <stdio.h>

#include "a_program/checker/session.h"
#include "a_program/producer/capsule.h"

#define PROTOTYPE_CHECKED_ARTIFACT_VERSION 87

/* Only an opaque checked capability can be published. The reader reconstructs
 * an untrusted module and runs the independent checker again; checked
 * capabilities are never serialized. */
int prototype_checked_artifact_write(
	FILE* stream,
	const struct prototype_checked_module* checked
);

int prototype_checked_artifact_write_with_capsule(
	FILE* stream,
	const struct prototype_checked_module* checked,
	const struct prototype_work_capsule* capsule
);

int prototype_checked_artifact_read(
	FILE* stream,
	const struct prototype_checker_options* options,
	struct prototype_elaborated_module* module,
	struct prototype_checked_module** p_checked,
	struct prototype_checker_report* p_report
);

/* Returns zero with a decoded untrusted capsule, one when no producer section
 * exists, and minus one for a malformed container or capsule. */
int prototype_checked_artifact_extract_capsule(
	FILE* stream,
	struct prototype_work_capsule* capsule
);

#endif
