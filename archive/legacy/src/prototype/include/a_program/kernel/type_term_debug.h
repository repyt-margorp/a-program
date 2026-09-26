#ifndef A_PROGRAM_PROTOTYPE_TYPE_TERM_DEBUG_H
#define A_PROGRAM_PROTOTYPE_TYPE_TERM_DEBUG_H

#include <stdint.h>
#include <stdio.h>

struct prototype_intrinsic_typing_environment;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct symbol_table;

/* Layer T diagnostic readback. It may inspect type declarations to attach
 * names to Core syntax, but it has no role in Core identity or reduction. */
void prototype_type_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
);

#endif
