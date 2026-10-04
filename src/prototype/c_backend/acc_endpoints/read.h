#ifndef __ACC_ENDPOINT_READ_H__
#define __ACC_ENDPOINT_READ_H__

#include "../indexed_views/view.h"

struct pg_c_acc_endpoint {
	const struct pg_data_declaration *declaration;
	int quoted;
	size_t count;
	struct pg_c_indexed_operand arguments[16];
};

/* Read a closed target endpoint through explicit known constructor syntax.
	* Borrows admitted source; no evaluation, typing, or numeric cancellation. */
int pg_c_acc_endpoint_read(struct pg_graph *, const struct pg_term *,
	const struct pg_data_layout *, struct pg_c_acc_endpoint *);

#endif
