#include "symbol.h"
#include "term.h"
#include "type_declaration.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define MATCH_FRAME_CAPACITY 16
#define TYPE_CAPACITY 16
#define CONSTRUCTOR_CAPACITY 16
#define PARAMETER_CAPACITY 16
#define FIELD_TYPE_CAPACITY 16
#define TYPE_EXPR_CAPACITY 16
#define SYMBOL_MAP_CAPACITY 64
#define SYMBOL_STORAGE_CAPACITY 64

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

static int symbol_ids[SYMBOL_MAP_CAPACITY];
static uint32_t symbol_hashes[SYMBOL_MAP_CAPACITY];
static char* symbol_strings[SYMBOL_STORAGE_CAPACITY];

static int build_uniform_effect_match(
	struct prototype_term_db* term_db,
	struct symbol_table* symbols,
	uint32_t* p_match_term,
	uint32_t* p_effect_body
) {
	int b_symbol = symbol_intern(symbols, "b", 1);
	int true_symbol = symbol_intern(symbols, "true", 4);
	int false_symbol = symbol_intern(symbols, "false", 5);
	int print_symbol = symbol_intern(symbols, "#.print", 7);
	int text_type_symbol = symbol_intern(symbols, "#.Text", 6);
	int hit_symbol = symbol_intern(symbols, "hit", 3);
	if (b_symbol < 0 || true_symbol < 0 || false_symbol < 0 ||
		print_symbol < 0 || text_type_symbol < 0 || hit_symbol < 0) {
		return -1;
	}

	uint32_t scrutinee;
	uint32_t print_intrinsic;
	uint32_t text_literal;
	uint32_t effect_body;
	if (prototype_term_external_ref(
			term_db,
			(struct prototype_qualified_name){ -1, b_symbol },
			&scrutinee
		) != 0 ||
		prototype_term_intrinsic(
			term_db,
			PROTOTYPE_TERM_INTRINSIC_PRINT,
			print_symbol,
			text_type_symbol,
			&print_intrinsic
		) != 0 ||
		prototype_term_text_literal(term_db, hit_symbol, &text_literal) != 0 ||
		prototype_term_app(term_db, print_intrinsic, text_literal, &effect_body) != 0) {
		return -1;
	}

	struct prototype_match_case_input match_cases[2];
	match_cases[0].case_label_symbol_id = true_symbol;
	match_cases[0].constructor_owner = PROTOTYPE_INVALID_ID;
	match_cases[0].constructor_id = 0;
	match_cases[0].binders = NULL;
	match_cases[0].binder_count = 0;
	match_cases[0].body = effect_body;
	match_cases[1].case_label_symbol_id = false_symbol;
	match_cases[1].constructor_owner = PROTOTYPE_INVALID_ID;
	match_cases[1].constructor_id = 1;
	match_cases[1].binders = NULL;
	match_cases[1].binder_count = 0;
	match_cases[1].body = effect_body;

	uint32_t frame_id = prototype_term_new_match_frame(term_db);
	if (frame_id == PROTOTYPE_INVALID_ID ||
		prototype_term_match_with_frame(
			term_db,
			scrutinee,
			match_cases,
			2,
			frame_id,
			p_match_term
		) != 0 ||
		prototype_term_set_match_frame_term(term_db, frame_id, *p_match_term) != 0) {
		return -1;
	}

	*p_effect_body = effect_body;
	return 0;
}

int main(void) {
	struct symbol_table symbols;
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;

	symbol_table_init(
		&symbols,
		symbol_ids,
		symbol_hashes,
		SYMBOL_MAP_CAPACITY,
		symbol_strings,
		SYMBOL_STORAGE_CAPACITY
	);
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

	uint32_t match_term;
	uint32_t effect_body;
	if (build_uniform_effect_match(&term_db, &symbols, &match_term, &effect_body) != 0) {
		symbol_table_free(&symbols);
		return 1;
	}

	uint32_t pure_result;
	if (prototype_term_whnf_with_options(
			&term_db,
			&type_db,
			NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
			},
			match_term,
			&pure_result
		) != 0 ||
		pure_result != effect_body) {
		symbol_table_free(&symbols);
		return 1;
	}

	FILE* effect_output = tmpfile();
	if (!effect_output) {
		symbol_table_free(&symbols);
		return 1;
	}

	int effect_performed = 0;
	uint32_t effect_result;
	if (prototype_term_perform_with_options(
			&term_db,
			&type_db,
			NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_PERFORM_HOST_EFFECT,
				.effect_output = effect_output,
				.symbols = &symbols,
				.effect_capabilities = PROTOTYPE_HOST_EFFECT_TERMINAL,
				.p_effect_performed = &effect_performed
			},
			match_term,
			&effect_result
		) != 0 ||
		effect_result != match_term ||
		effect_performed != 0 ||
		ftell(effect_output) != 0) {
		fclose(effect_output);
		symbol_table_free(&symbols);
		return 1;
	}

	fclose(effect_output);
	symbol_table_free(&symbols);
	return 0;
}
