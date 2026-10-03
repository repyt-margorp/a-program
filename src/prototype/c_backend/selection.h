#ifndef __APPLIED_SELECTION_H__
#define __APPLIED_SELECTION_H__

#include "lower/representation.h"

/* Target-only mapping of an already admitted closed, unindexed type instance.
	* This view creates no source term, type evidence or declaration. */
struct pg_c_applied_selection {
	const struct pg_data_declaration *declaration;
	const struct pg_term *type;
	const struct pg_context *parameters;
	size_t count;
	const struct pg_c_representation **arguments;
};

const struct pg_occurrence *pg_c_value_classifier(const struct pg_occurrence *);
int pg_c_applied_selection_read(struct pg_c_applied_selection *, struct pg_graph *,
	const struct pg_c_representations *, const struct pg_occurrence *);
const struct pg_c_representation *pg_c_applied_parameter(
	const struct pg_c_applied_selection *, const struct pg_object *);

#endif
