#ifndef A_PROGRAM_POINTER_DESCRIPTOR_IO_H
#define A_PROGRAM_POINTER_DESCRIPTOR_IO_H

#include "graph_io.h"

/* Inert built-in descriptors; context is a destination/source pg_classifiers.
 * Does not recover or accept typing evidence. Unsupported owners fail closed. */
extern const struct pg_graph_codec pg_builtin_graph_codec;

#endif
