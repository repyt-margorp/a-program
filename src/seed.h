#ifndef A_PROGRAM_POINTER_SEED_H
#define A_PROGRAM_POINTER_SEED_H

#include "program.h"
#include <stdio.h>

/* Single-source convenience wrapper around source_io.h's shared RECOMPUTE
 * image. The writer starts in the ordinary root environment. Use source_io.h
 * directly to retain external modules. No addresses, accepted flags or runtime
 * state are stored. Streams are caller-owned.
 * This codec is not the final .a format or a CHECKPOINT implementation. */
int pg_seed_write(FILE *file, const char *source, size_t length,
	enum pg_definition_policy policy);
/* Reject unsupported headers, trailing bytes and invalid structural syntax.
 * limit bounds scope and syntax data as specified by source_io.h, not source length.
 * Loading schedules ordinary synthesis without reparsing or advancing Solve.
 * NULL denotes invalid input/allocation failure. No host operations execute. */
struct pg_program *pg_seed_read(FILE *file, size_t limit);

#endif
