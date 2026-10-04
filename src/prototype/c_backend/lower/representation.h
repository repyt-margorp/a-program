#ifndef A_PROGRAM_C_REPRESENTATION_H
#define A_PROGRAM_C_REPRESENTATION_H

#include "../emit.h"
#include "iadt.h"

/* Downstream representation contract, never source typing evidence. */
struct pg_c_representation {
	size_t width, count;
	const char *alias;
	const struct pg_data_layout *layout;
	struct pg_c_constructor_representation *constructors;
	int recursive;
	int natural;
	int callback; /* Zero for ordinary values; otherwise foreign callback arity. */
	const struct pg_c_representation *callback_domain, *callback_result;
	size_t zero;
};
struct pg_c_constructor_representation {
	size_t count;
	const struct pg_c_representation **fields;
	size_t tail;
};
struct pg_c_representations {
	struct pg_index lookup;
	size_t count;
	const struct pg_c_representation **types;
	const char *arena_alias;
	struct pg_index instances;
};
int pg_c_representations_init(struct pg_c_representations *, struct pg_graph *storage,
	size_t enum_count, const struct pg_c_export *enums,
	size_t data_count, const struct pg_c_export *data);
int pg_c_representations_native(struct pg_c_representations *, struct pg_graph *storage,
	size_t enum_count, const struct pg_c_export *enums,
	size_t natural_count, const struct pg_c_export *naturals,
	size_t data_count, const struct pg_c_export *data);
void pg_c_representations_destroy(struct pg_c_representations *);
const struct pg_c_representation *pg_c_representation_find(const struct pg_c_representations *, const struct pg_object *);
const struct pg_c_representation *pg_c_representation_term(const struct pg_c_representations *, const struct pg_term *);
void pg_c_representation_type(FILE *, const struct pg_c_representation *);
void pg_c_representation_private_type(FILE *, const struct pg_c_representation *);
void pg_c_representation_declarations(FILE *, const struct pg_c_representations *);
int pg_c_representation_list(const struct pg_c_representation *, size_t *cell, size_t *payload);
int pg_c_representation_branching(const struct pg_c_representation *);

#endif
