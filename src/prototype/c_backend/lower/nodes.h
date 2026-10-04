#ifndef A_PROGRAM_C_NODES_H
#define A_PROGRAM_C_NODES_H

#include "representation.h"

/* Emit target-only arena ownership and finite recursive input validation. */
void pg_c_nodes_declarations(FILE *, const struct pg_c_representations *);
void pg_c_nodes_implementation(FILE *, const struct pg_c_representations *);

#endif
