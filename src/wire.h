#ifndef A_PROGRAM_POINTER_WIRE_H
#define A_PROGRAM_POINTER_WIRE_H

#include <stdint.h>
#include <stdio.h>

/* Fixed-width little-endian transport, independent of host integer layout. */
int pg_wire_write_u64(FILE *file, uint64_t value);
int pg_wire_read_u64(FILE *file, uint64_t *value);

#endif
