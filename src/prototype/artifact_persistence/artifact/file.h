#ifndef A_PROGRAM_ARTIFACT_FILE_H
#define A_PROGRAM_ARTIFACT_FILE_H
#include "source_io.h"

#define PG_ARTIFACT_DEFAULT_LIMIT 1000000

/* Decimal positive fixed limit, or "none" for SIZE_MAX (representability and
 * allocation checks still apply). Failure leaves the output unchanged. */
int pg_artifact_limit_argument(const char *argument, size_t *limit);

/* Explicit fixed decoder allowance, independent of bytes and Solve fuel.
 * Each payload bounds its own record/reference/name units; not a memory cap.
 * Streams are borrowed. Reading never advances Solve or admits evidence.
 * Failure leaves outputs alone. Seekless inputs are staged for relocation. */
struct pg_program *pg_artifact_read_file(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);

/* Explicit trust policy: borrow a saved closed local definition from a complete
 * source module, only before any Solve. Completion is an unauthenticated file
 * assertion, NOT freshly checked evidence. No checking, allocation or status
 * changes. Returns 1 on success, 0 if unavailable, -1 for invalid arguments;
 * failure leaves subject unchanged. Normal checking does not use this API. */
int pg_artifact_trusted_export(const struct pg_program *program,
	const struct pg_synthesis_job *module, struct pg_token name,
	const struct pg_occurrence **subject);

/* Explicit import revalidation through the ordinary Solve owner. At most
 * min(total_budget, validation_limit) scheduler steps are charged, stopping at
 * a terminal target or an empty ready queue. The returned spent amount must be
 * deducted from total_budget before subsequent useful work; it is NOT a second
 * allowance. No fallback to trust, imported evidence admission, or hidden work
 * at budget zero. Invalid arguments leave spent unchanged. This does not restore
 * missing private continuations or write verification history into an image. */
enum pg_synthesis_status pg_artifact_revalidate(struct pg_program *program,
	struct pg_synthesis_job *target, uint64_t total_budget, uint64_t validation_limit,
	uint64_t *spent);

/* Atomically replace path only after successful serialization and close. */
enum pg_artifact_contents { PG_ARTIFACT_MATERIALIZED, PG_ARTIFACT_INPUTS };
int pg_artifact_save_file(const char *path, const struct pg_program *program,
	size_t count, struct pg_synthesis_job *const *roots, enum pg_artifact_contents contents);
#endif
