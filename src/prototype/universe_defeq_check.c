#include "term.h"
#include "type_declaration.h"
#include "judgement.h"

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
static struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];

static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];

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
		match_frames,
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
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);

	uint32_t universe_u;
	uint32_t universe_v;
	if (prototype_term_universe_var(&term_db, 7, &universe_u) != 0 ||
		prototype_term_universe_var(&term_db, 8, &universe_v) != 0) {
		return 1;
	}
	if (!prototype_judgement_classifier_normalization_equal(
			&term_db, &type_db, universe_u, universe_u
		)) {
		return 1;
	}
	if (prototype_judgement_classifier_normalization_equal(
			&term_db, &type_db, universe_u, universe_v
		)) {
		return 1;
	}
	return 0;
}
