#ifndef A_PROGRAM_POINTER_DECLARATION_IO_H
#define A_PROGRAM_POINTER_DECLARATION_IO_H
#include "descriptor_io.h"
#include "typing.h"

/* Per-transport packing cache, not a live declaration/solver authority.
 * Borrow typing and its graph for this call.
 * Destroy after transport. Loaded declarations belong to typing->graph. */
struct pg_declaration_io {
	struct pg_typing *typing;
	struct pg_graph storage;
	struct pg_index payloads;
};
int pg_declaration_io_init(struct pg_declaration_io *io,
	struct pg_typing *typing);
void pg_declaration_io_destroy(struct pg_declaration_io *io);
extern const struct pg_graph_codec pg_declaration_graph_codec;
#endif
