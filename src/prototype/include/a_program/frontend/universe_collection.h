#ifndef __PROTOTYPE_FRONTEND_UNIVERSE_COLLECTION_H__
#define __PROTOTYPE_FRONTEND_UNIVERSE_COLLECTION_H__

#include "a_program/kernel/universe.h"

struct prototype_judgement_db;
struct prototype_typed_occurrence_graph;
struct prototype_term_db;
struct prototype_type_declaration_db;

/* Compiler traversal that emits provenance-bearing constraints to the kernel
 * Universe solver. The solver itself has no frontend or TypedOccurrenceGraph
 * dependency. */
int prototype_universe_reconstruct_obligations(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement
);

int prototype_universe_close_program(
	struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
);

int prototype_universe_build_closed(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement
);

int prototype_universe_validate_replay(
	const struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement
);

#endif
