#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/universe.h"

#include <stdint.h>

#define TERM_CAPACITY 16
#define CASE_CAPACITY 4
#define CASE_BINDER_CAPACITY 4
#define MATCH_FRAME_CAPACITY 4
#define TYPE_CAPACITY 4
#define CONSTRUCTOR_CAPACITY 4
#define PARAMETER_CAPACITY 4
#define FIELD_TYPE_CAPACITY 4
#define TYPE_EXPR_CAPACITY 4

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];

static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_constructor_readback constructor_readbacks[CONSTRUCTOR_CAPACITY];
static struct prototype_constructor_classifier_cache_entry constructor_caches[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_type_representation type_representations[TYPE_CAPACITY];
static struct prototype_universe_node universe_nodes[2];
static struct prototype_universe_edge universe_edges[2];
static struct prototype_universe_level universe_levels[2];
static struct prototype_universe_constraint universe_constraints[2];
static struct prototype_universe_obligation_span universe_obligations[2];

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		cases,
		case_label_symbols,
		CASE_CAPACITY,
		case_binders,
		CASE_BINDER_CAPACITY,
		ih_scopes,
		MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db,
		type_declarations,
		TYPE_CAPACITY,
		constructor_declarations,
		CONSTRUCTOR_CAPACITY,
		parameter_declarations,
		PARAMETER_CAPACITY,
		constructor_readbacks,
		CONSTRUCTOR_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY,
		type_representations,
		TYPE_CAPACITY,
		constructor_caches,
		CONSTRUCTOR_CAPACITY
	);

	uint32_t universe_u;
	uint32_t universe_v;
	if (prototype_term_universe_var(&term_db, 7, &universe_u) != 0 ||
		prototype_term_universe_var(&term_db, 8, &universe_v) != 0) {
		return 1;
	}
	if (!(prototype_judgement_classifier_conversion(
			&term_db, &type_db, universe_u, universe_u
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 1;
	}
	if ((prototype_judgement_classifier_conversion(
			&term_db, &type_db, universe_u, universe_v
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 1;
	}
	struct prototype_universe_db universe_db;
	prototype_universe_db_init(
		&universe_db,
		universe_nodes,
		2,
		universe_edges,
		2,
		universe_levels,
		2,
		universe_constraints,
		2,
		universe_obligations,
		2
	);
	uint32_t level_index;
	if (prototype_universe_ensure_level(&universe_db, 7, &level_index) != 0 ||
		prototype_universe_add_constraint(
			&universe_db,
			7,
			7,
			1,
			universe_u,
			universe_u,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_TERM_LEVEL_SUCCESSOR,
			0,
			0,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			0,
			universe_u,
			universe_u
		) != 0 || prototype_universe_close(&universe_db) == 0 ||
		universe_db.certificate.state == PROTOTYPE_UNIVERSE_CERTIFICATE_CLOSED) {
		return 1;
	}
	return 0;
}
