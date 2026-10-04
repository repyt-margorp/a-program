#ifndef A_PROGRAM_C_SCALAR_H
#define A_PROGRAM_C_SCALAR_H

#include "../emit.h"

/* Borrow admitted roots. Share temporary C-local functions and scalar DAGs;
	* do not normalize, mutate source graphs or grant typing evidence.
	* Both streams remain unwritten until every selected function is supported. */
int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error);

/* Separate opt-in ABI: borrowed pure-total unary Int32/Int64 callbacks only.
	* It does not enable callable fields or change the existing native profiles. */
int pg_c_emit_callbacks(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error);

/* Separate bounded ABI: unary/binary same-width pure-total scalar callbacks.
	* Flat binary calls require both value operands; no foreign closure escapes. */
int pg_c_emit_callbacks2(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error);

/* Explicit target-side representations, including bounded direct Self fields.
	* The scalar profile has no selections. */
int pg_c_emit_native(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t data_count,
	const struct pg_c_export *data, const char **error);

/* Copied emitter-selected metadata, valid after temporary lowering is destroyed. */
struct pg_c_native_contract {
	int recursive, natural, copy_out, value_fields, branching;
};
int pg_c_emit_native_profile(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t natural_count,
	const struct pg_c_export *naturals, size_t data_count,
	const struct pg_c_export *data, struct pg_c_native_contract *, const char **error);

/* Separate borrowed predicate ABI: selected Nat32 domains and a selected
	* two-constructor nullary enum result. Invalid foreign tags preserve output. */
int pg_c_emit_predicate_native(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t natural_count,
	const struct pg_c_export *naturals, size_t data_count,
	const struct pg_c_export *data, struct pg_c_native_contract *, const char **error);

#endif
