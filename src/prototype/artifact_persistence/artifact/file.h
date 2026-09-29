#ifndef A_PROGRAM_ARTIFACT_FILE_H
#define A_PROGRAM_ARTIFACT_FILE_H
#include "source_io.h"

#define PG_ARTIFACT_DEFAULT_LIMIT 1000000

/* Explicit fixed decoder allowance, independent of bytes and Solve fuel.
 * Each payload bounds its own record/reference/name units; not a memory cap.
 * Streams are borrowed. Reading never advances Solve or admits evidence.
 * Failure leaves outputs alone. Seekless inputs are staged for relocation. */
struct pg_program *pg_artifact_read_file(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);

/* Atomically replace path only after successful serialization and close. */
enum pg_artifact_contents { PG_ARTIFACT_MATERIALIZED, PG_ARTIFACT_INPUTS };
int pg_artifact_save_file(const char *path, const struct pg_program *program,
	size_t count, struct pg_synthesis_job *const *roots, enum pg_artifact_contents contents);
#endif
