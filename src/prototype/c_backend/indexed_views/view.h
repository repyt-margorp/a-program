#ifndef __C_INDEXED_VIEW_H__
#define __C_INDEXED_VIEW_H__

#include "iadt.h"

struct pg_c_indexed_binding;
struct pg_c_indexed_operand {
	const struct pg_term *term;
	const struct pg_c_indexed_binding *environment;
};
struct pg_c_indexed_binding {
	const struct pg_c_indexed_binding *parent;
	const struct pg_object *binder;
	struct pg_c_indexed_operand value;
};
struct pg_c_indexed_view {
	const struct pg_data_declaration *declaration;
	const struct pg_c_indexed_binding *environment;
	size_t count;
	struct pg_c_indexed_operand arguments[16];
};

/* Target-local static type-factory binding only. It neither evaluates source
	* computations nor constructs source terms/evidence. Unknown shapes refuse. */
int pg_c_indexed_head(struct pg_graph *, struct pg_c_indexed_operand,
	struct pg_c_indexed_operand *, size_t *, struct pg_c_indexed_operand *);
int pg_c_indexed_view_read(struct pg_graph *, const struct pg_term *, struct pg_c_indexed_view *);
struct pg_c_indexed_operand pg_c_indexed_bound(struct pg_c_indexed_operand);

#endif
