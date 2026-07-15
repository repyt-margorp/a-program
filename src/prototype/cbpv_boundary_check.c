#include "judgement.h"
#include "symbol.h"
#include "term.h"
#include "type_declaration.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TERM_CAPACITY 256
#define CASE_CAPACITY 8
#define CASE_BINDER_CAPACITY 8
#define MATCH_FRAME_CAPACITY 8
#define TYPE_CAPACITY 8
#define CONSTRUCTOR_CAPACITY 8
#define PARAMETER_CAPACITY 8
#define FIELD_TYPE_CAPACITY 8
#define TYPE_EXPR_CAPACITY 8
#define JUDGEMENT_CAPACITY 128

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructors[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameters[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_judgement_relation judgement_relations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_proof judgement_proofs[JUDGEMENT_CAPACITY];
static struct prototype_judgement_relation delta_relations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_proof delta_proofs[JUDGEMENT_CAPACITY];
static struct prototype_judgement_match_motive_result motive_results[8];
static struct prototype_judgement_computation_constraint computation_constraints[8];
static struct prototype_judgement_effect_row_equation effect_row_equations[8];
static int symbol_map_ids[16];
static uint32_t symbol_map_hashes[16];
static char* symbol_storage[16];

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_judgement_db judgement;
	struct prototype_judgement_delta delta;
	struct symbol_table symbols;
	symbol_table_init(
		&symbols, symbol_map_ids, symbol_map_hashes, 16, symbol_storage, 16
	);
	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_label_symbols, CASE_CAPACITY,
		case_binders, CASE_BINDER_CAPACITY, match_frames, MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, TYPE_CAPACITY, constructors, CONSTRUCTOR_CAPACITY,
		parameters, PARAMETER_CAPACITY, field_types, FIELD_TYPE_CAPACITY,
		type_exprs, TYPE_EXPR_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement, judgement_relations, judgement_proofs, JUDGEMENT_CAPACITY
	);
	prototype_judgement_delta_init(
		&delta, &judgement, delta_relations, delta_proofs, JUDGEMENT_CAPACITY,
		motive_results, 8, computation_constraints, 8, effect_row_equations, 8
	);

	uint32_t value;
	uint32_t returned;
	uint32_t suspended;
	uint32_t forced;
	if (prototype_term_int_literal(&term_db, 1, &value) != 0 ||
		prototype_term_return(&term_db, value, &returned) != 0 ||
		prototype_term_thunk(&term_db, returned, &suspended) != 0 ||
		prototype_term_force(&term_db, suspended, &forced) != 0 ||
		prototype_judgement_delta_infer_term_classifiers(&delta, &term_db, &type_db) != 0 ||
		prototype_judgement_delta_commit(&delta, 0) != 0 ||
		prototype_judgement_validate_proofs(&term_db, &type_db, &judgement) != 0) {
		return 1;
	}

	uint32_t classifier;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_lookup_classifier(&judgement, returned, &classifier) != 0 ||
		prototype_judgement_classifier_view(&term_db, &type_db, NULL, classifier, &view) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		view.effects != PROTOTYPE_HOST_EFFECT_NONE ||
		prototype_judgement_lookup_classifier(&judgement, forced, &classifier) != 0 ||
		prototype_judgement_classifier_view(&term_db, &type_db, NULL, classifier, &view) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return 1;
	}

	uint32_t empty_effect_row;
	uint32_t terminal_effect_row;
	uint32_t closed_effect_union;
	uint32_t symbolic_effect_row;
	uint32_t symbolic_effect_union;
	uint32_t row_computation;
	unsigned closed_effects;
	if (prototype_term_effect_label(&term_db, PROTOTYPE_HOST_EFFECT_NONE, &empty_effect_row) != 0 ||
		prototype_term_effect_label(&term_db, PROTOTYPE_HOST_EFFECT_TERMINAL, &terminal_effect_row) != 0 ||
		prototype_term_effect_row_union(
			&term_db, empty_effect_row, terminal_effect_row, &closed_effect_union
		) != 0 || prototype_term_effect_row_closed_bits(
			&term_db, closed_effect_union, &closed_effects
		) != 0 || closed_effects != PROTOTYPE_HOST_EFFECT_TERMINAL ||
		prototype_term_effect_row_var(&term_db, 99, &symbolic_effect_row) != 0 ||
		prototype_term_effect_row_union(
			&term_db, symbolic_effect_row, terminal_effect_row, &symbolic_effect_union
		) != 0 || prototype_term_effect_row_closed_bits(
			&term_db, symbolic_effect_union, &closed_effects
		) != 1 || prototype_term_computation_type(
			&term_db, symbolic_effect_union, value, &row_computation
		) != 0 || prototype_term_classifier_view(&term_db, row_computation, &view) != 0 ||
		view.effect_row != symbolic_effect_union ||
		view.effects != PROTOTYPE_HOST_EFFECT_NONE) {
		return 1;
	}
	uint32_t row_whnf;
	if (prototype_term_whnf(
			&term_db, &type_db, symbolic_effect_union, &row_whnf
		) != 0 || row_whnf != symbolic_effect_union) {
		return 1;
	}

	uint32_t reduced;
	if (prototype_term_whnf(&term_db, &type_db, forced, &reduced) != 0 ||
		reduced != returned) {
		return 1;
	}
	uint32_t bound_var;
	uint32_t bound_result;
	uint32_t bound_continuation;
	uint32_t pure_bound;
	if (prototype_term_var(&term_db, 6, &bound_var) != 0 ||
		prototype_term_return(&term_db, bound_var, &bound_result) != 0 ||
		prototype_term_lambda(&term_db, 6, bound_result, &bound_continuation) != 0 ||
		prototype_term_bind(&term_db, returned, bound_continuation, &pure_bound) != 0 ||
		prototype_term_whnf(&term_db, &type_db, pure_bound, &reduced) != 0 ||
		reduced != returned) {
		return 1;
	}
	uint32_t runtime_return;
	if (prototype_term_execute_with_default_host_handler(
			stdout, &symbols, &type_db, &term_db, returned, &runtime_return
		) != 0 || runtime_return != returned) {
		return 1;
	}
	uint32_t operation;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t request;
	if (prototype_term_operation(
			&term_db, PROTOTYPE_OPERATION_INT_ADD, 0, -1, &operation
		) != 0 || prototype_term_lambda(
			&term_db, 6, bound_result, &continuation_lambda
		) != 0 || prototype_term_operation_request(
			&term_db, operation, value, continuation_lambda, &request
		) == 0 || prototype_term_thunk(
			&term_db, continuation_lambda, &continuation_thunk
		) != 0 || prototype_term_operation_request(
			&term_db, operation, value, continuation_thunk, &request
		) != 0) {
		return 1;
	}
	symbol_table_free(&symbols);
	return 0;
}
