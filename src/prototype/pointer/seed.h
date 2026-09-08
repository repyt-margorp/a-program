#ifndef A_PROGRAM_POINTER_SEED_H
#define A_PROGRAM_POINTER_SEED_H

#include "program.h"
#include <stdio.h>

/* Experimental single-source RECOMPUTE input, not a solved program image.
 * Stores the parsed syntax DAG and definition policy; no addresses, accepted flags,
 * external module registrations or evaluation state. Streams are caller-owned.
 * This codec is not the final .a format or a CHECKPOINT implementation. */
int pg_seed_write(FILE *file, const char *source, size_t length,
	enum pg_definition_policy policy);
/* Reject unsupported headers, trailing bytes and invalid structural syntax.
 * limit bounds syntax nodes, roots, items and token bytes, not source length.
 * Loading schedules ordinary synthesis without reparsing or advancing Solve.
 * NULL denotes invalid input/allocation failure. No host operations execute. */
struct pg_program *pg_seed_read(FILE *file, size_t limit);

#endif
