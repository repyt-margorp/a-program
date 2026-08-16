#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/support/symbol.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

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
#define CONTEXT_CAPACITY 128
#define SUBSTITUTION_CAPACITY 128

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructors[CONSTRUCTOR_CAPACITY];
static struct prototype_type_constructor_readback constructor_readbacks[CONSTRUCTOR_CAPACITY];
static struct prototype_constructor_classifier_cache_entry constructor_caches[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameters[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_type_representation type_representations[TYPE_CAPACITY];
static struct prototype_judgement_proposition judgement_relations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation_candidate judgement_proofs[JUDGEMENT_CAPACITY];
static struct prototype_judgement_claim judgement_claims[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation judgement_derivations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_candidate_premise judgement_candidate_premises[
	JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
];
static struct prototype_judgement_premise_edge judgement_accepted_premises[
	JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
];
static struct prototype_judgement_proposition delta_relations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation_candidate delta_proofs[JUDGEMENT_CAPACITY];
static struct prototype_judgement_candidate_premise delta_premises[
	JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
];
static struct prototype_judgement_match_motive_result motive_results[8];
static struct prototype_judgement_computation_constraint computation_constraints[8];
static struct prototype_judgement_effect_row_constraint effect_row_constraints[8];
static struct prototype_context context_entries[CONTEXT_CAPACITY];
static struct prototype_substitution substitution_entries[SUBSTITUTION_CAPACITY];
static int symbol_map_ids[16];
static uint32_t symbol_map_hashes[16];
static char* symbol_storage[16];

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_judgement_db judgement;
	struct prototype_judgement_delta delta;
	struct prototype_context_db contexts;
	struct prototype_substitution_db substitutions;
	struct symbol_table symbols;
	symbol_table_init(
		&symbols, symbol_map_ids, symbol_map_hashes, 16, symbol_storage, 16
	);
	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_label_symbols, CASE_CAPACITY,
		case_binders, CASE_BINDER_CAPACITY, ih_scopes, MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, TYPE_CAPACITY, constructors, CONSTRUCTOR_CAPACITY,
		parameters, PARAMETER_CAPACITY, constructor_readbacks, CONSTRUCTOR_CAPACITY,
		field_types, FIELD_TYPE_CAPACITY, type_exprs, TYPE_EXPR_CAPACITY,
		type_representations, TYPE_CAPACITY, constructor_caches, CONSTRUCTOR_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement,
		judgement_relations,
		judgement_proofs,
		judgement_claims,
		judgement_derivations,
		JUDGEMENT_CAPACITY,
		judgement_candidate_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		judgement_accepted_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_judgement_delta_init(
		&delta, &judgement, delta_relations, delta_proofs, JUDGEMENT_CAPACITY,
		delta_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motive_results, 8, computation_constraints, 8, effect_row_constraints, 8
	);
	prototype_context_db_init(&contexts, context_entries, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitutions, substitution_entries, SUBSTITUTION_CAPACITY
	);
	prototype_judgement_delta_set_context_store(
		&delta, &contexts, &substitutions
	);
	prototype_judgement_delta_set_intrinsic_environment(
		&delta, prototype_default_intrinsic_environment()
	);

	uint32_t value;
	uint32_t returned;
	uint32_t suspended;
	uint32_t forced;
	/* The literal helper fact is replayed against the same immutable Intrinsic
	 * environment that selects the source-level unsuffixed literal principal. */
	if (prototype_term_int_literal(&term_db, 42, &value) != 0 ||
		prototype_term_return(&term_db, value, &returned) != 0 ||
		prototype_term_thunk(&term_db, returned, &suspended) != 0 ||
		prototype_term_force(&term_db, suspended, &forced) != 0 ||
		prototype_judgement_delta_infer_core_helper_facts(
			&delta, &term_db, &type_db
		) != 0 ||
		prototype_judgement_delta_commit(&delta, 0) != 0 ||
		prototype_judgement_publish_candidates(NULL, &judgement) != 0 ||
		prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			prototype_default_intrinsic_environment(),
			&contexts,
			&substitutions,
			NULL,
			&judgement
		) != 0) {
		return 1;
	}

	uint32_t classifier;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_lookup_authority_neutral_core_classifier(
			&judgement, returned, &classifier
		) != 0 ||
		prototype_judgement_classifier_view(&term_db, &type_db, NULL, classifier, &view) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_effect_row_purity(&term_db, view.effect_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_PURE ||
		prototype_judgement_lookup_authority_neutral_core_classifier(
			&judgement, forced, &classifier
		) != 0 ||
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
	uint32_t pure_computation;
	uint32_t terminal_computation;
	uint32_t symbolic_computation;
	uint32_t solved_symbolic_computation;
	uint32_t scoped_symbolic_row;
	uint32_t duplicate_scoped_symbolic_row;
	uint32_t once_scoped_symbolic_row;
	uint32_t scoped_terminal_row;
	uint32_t substituted_scoped_row;
	if (prototype_term_effect_row_empty(&term_db, &empty_effect_row) != 0 ||
		prototype_term_effect_row_operation(
			&term_db,
			PROTOTYPE_EFFECT_OPERATION_PRINT,
			empty_effect_row,
			&terminal_effect_row
		) != 0 ||
		prototype_term_effect_row_union(
			&term_db, empty_effect_row, terminal_effect_row, &closed_effect_union
		) != 0 || prototype_term_effect_row_is_closed(
			&term_db, closed_effect_union
		) != 1 ||
		prototype_term_effect_row_var(&term_db, 99, &symbolic_effect_row) != 0 ||
		prototype_term_effect_row_union(
			&term_db, symbolic_effect_row, terminal_effect_row, &symbolic_effect_union
		) != 0 || prototype_term_effect_row_is_closed(
			&term_db, symbolic_effect_union
		) != 0 || prototype_term_effect_row_operation(
			&term_db,
			PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
			symbolic_effect_row,
			&scoped_symbolic_row
		) != 0 || prototype_term_effect_row_operation(
			&term_db,
			PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
			symbolic_effect_row,
			&duplicate_scoped_symbolic_row
		) != 0 || scoped_symbolic_row != duplicate_scoped_symbolic_row ||
		prototype_term_effect_row_operation(
			&term_db,
			PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE,
			symbolic_effect_row,
			&once_scoped_symbolic_row
		) != 0 || scoped_symbolic_row == once_scoped_symbolic_row ||
		prototype_term_effect_row_operation(
			&term_db,
			PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
			terminal_effect_row,
			&scoped_terminal_row
		) != 0 || prototype_term_graph_substitute_bound_var(
			&term_db,
			&type_db,
			scoped_symbolic_row,
			99,
			terminal_effect_row,
			&substituted_scoped_row
		) != 0 || !(prototype_judgement_classifier_conversion(
			&term_db,
			&type_db,
			substituted_scoped_row,
			scoped_terminal_row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_computation_type(
			&term_db, symbolic_effect_union, value, &row_computation
		) != 0 || prototype_term_classifier_view(&term_db, row_computation, &view) != 0 ||
		view.effect_row != symbolic_effect_union ||
		prototype_term_computation_type(
			&term_db, empty_effect_row, value, &pure_computation
		) != 0 ||
		prototype_term_computation_type(
			&term_db, terminal_effect_row, value, &terminal_computation
		) != 0 ||
		prototype_term_computation_type(
			&term_db, symbolic_effect_row, value, &symbolic_computation
		) != 0 ||
		prototype_judgement_classifier_compatible(
			&term_db, &type_db, pure_computation, terminal_computation
		) != 0 ||
		prototype_judgement_solve_expected_effect_rows(
			&term_db,
			&type_db,
			NULL,
			symbolic_computation,
			terminal_computation,
			&solved_symbolic_computation
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			&term_db,
			&type_db,
			solved_symbolic_computation,
			terminal_computation
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 1;
	}

	/* A curried source Lambda keeps a raw negative Pi spine. A value-arrow
	 * annotation may quote the same spine as Comp({}, Thunk(Pi(...))). Expected
	 * effect-row solving must recognize this elaboration boundary in either
	 * orientation, but an effectful quotation must remain distinct. */
	uint32_t int_type;
	uint32_t function_result;
	uint32_t function_binder;
	uint32_t function_family;
	uint32_t raw_function;
	uint32_t suspended_function_type;
	uint32_t pure_function_quotation;
	uint32_t effectful_function_quotation;
	uint32_t solved_function_classifier;
	if (prototype_term_make_host_type(
			&term_db, PROTOTYPE_HOST_TYPE_INT64, &int_type
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, int_type, &function_result
		) != 0 ||
		(function_binder = prototype_term_new_binding(&term_db)) ==
			PROTOTYPE_INVALID_ID || prototype_term_pure_family(
			&term_db, function_binder, function_result, &function_family
		) != 0 || prototype_term_pi_family(
			&term_db, int_type, function_family, &raw_function
		) != 0 || prototype_term_thunk_type(
			&term_db, raw_function, &suspended_function_type
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, suspended_function_type,
			&pure_function_quotation
		) != 0 || prototype_term_computation_type(
			&term_db, terminal_effect_row, suspended_function_type,
			&effectful_function_quotation
		) != 0 || prototype_judgement_solve_expected_effect_rows(
			&term_db, &type_db, NULL, pure_function_quotation, raw_function,
			&solved_function_classifier
		) != 0 || prototype_judgement_solve_expected_effect_rows(
			&term_db, &type_db, NULL, raw_function, pure_function_quotation,
			&solved_function_classifier
		) != 0 || prototype_judgement_solve_expected_effect_rows(
			&term_db, &type_db, NULL, effectful_function_quotation, raw_function,
			&solved_function_classifier
		) == 0 || prototype_judgement_solve_expected_effect_rows(
			&term_db, &type_db, NULL, raw_function, effectful_function_quotation,
			&solved_function_classifier
		) == 0) {
		return 1;
	}
	struct prototype_term_normalization_result normalization;
	if (prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			symbolic_effect_union,
			PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&normalization
		) != 0 ||
		normalization.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		normalization.term_id != symbolic_effect_union) {
		return 1;
	}

	uint32_t returned_classifier;
	uint32_t widened_returned_classifier;
	uint32_t wrong_result_classifier;
	if (prototype_judgement_lookup_authority_neutral_core_classifier(
			&judgement, returned, &returned_classifier
		) != 0 || prototype_judgement_classifier_view(
			&term_db, &type_db, NULL, returned_classifier, &view
		) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_computation_type(
			&term_db,
			terminal_effect_row,
			view.result,
			&widened_returned_classifier
		) != 0 || prototype_term_computation_type(
			&term_db,
			terminal_effect_row,
			value,
			&wrong_result_classifier
		) != 0) {
		return 1;
	}
	prototype_judgement_delta_init(
		&delta, &judgement, delta_relations, delta_proofs, JUDGEMENT_CAPACITY,
		delta_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motive_results, 8, computation_constraints, 8, effect_row_constraints, 8
	);
	prototype_judgement_delta_set_context_store(
		&delta, &contexts, &substitutions
	);
	prototype_judgement_delta_set_intrinsic_environment(
		&delta, prototype_default_intrinsic_environment()
	);
	prototype_judgement_delta_set_context(
		&delta, prototype_context_empty(&contexts)
	);
	struct prototype_judgement_selected_evidence returned_evidence = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = returned,
		.context_id = prototype_context_empty(&contexts),
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = returned,
		.classifier = returned_classifier
	};
	if (prototype_judgement_delta_record_effect_weaken(
			&delta,
			&term_db,
			&type_db,
			&returned_evidence,
			widened_returned_classifier
		) != 0 || prototype_judgement_delta_record_effect_weaken(
			&delta,
			&term_db,
			&type_db,
			&returned_evidence,
			wrong_result_classifier
		) == 0 || prototype_judgement_delta_commit(&delta, 0) != 0 ||
		prototype_judgement_publish_candidates(NULL, &judgement) != 0 ||
		prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			prototype_default_intrinsic_environment(),
			&contexts,
			&substitutions,
			NULL,
			&judgement
		) != 0) {
		return 1;
	}

	if (prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			forced,
			PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&normalization
		) != 0 ||
		normalization.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		normalization.term_id != returned) {
		return 1;
	}
	uint32_t bound_var;
	uint32_t bound_result;
	uint32_t bound_continuation;
	uint32_t pure_bound;
	if (prototype_term_var(&term_db, 6, &bound_var) != 0 ||
		prototype_term_return(&term_db, bound_var, &bound_result) != 0 ||
		prototype_term_lambda(&term_db, 6, bound_result, &bound_continuation) != 0 ||
		prototype_term_computation_fold(
			&term_db, returned, bound_continuation, NULL, 0, &pure_bound
		) != 0 ||
		prototype_term_normalize_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			pure_bound,
			PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&normalization
		) != 0 ||
		normalization.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		normalization.term_id != returned) {
		return 1;
	}
	uint32_t operation;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t request;
	if (prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &operation
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

	const struct prototype_intrinsic_environment* default_environment =
		prototype_default_intrinsic_environment();
	struct prototype_intrinsic_namespace_binding int64_bindings[32];
	if (default_environment->namespace_binding_count > 32) {
		return 1;
	}
	memcpy(
		int64_bindings,
		default_environment->namespace_bindings,
		default_environment->namespace_binding_count * sizeof(int64_bindings[0])
	);
	int remapped_int = 0;
	int remapped_add_name = 0;
	for (size_t i = 0; i < default_environment->namespace_binding_count; ++i) {
		if (strcmp(int64_bindings[i].source_name, "Int") == 0) {
			int64_bindings[i].target_id = PROTOTYPE_HOST_TYPE_INT64;
			remapped_int = 1;
		}
		if (int64_bindings[i].kind ==
				PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE &&
			int64_bindings[i].target_id == PROTOTYPE_PURE_PRIMITIVE_INT_ADD) {
			int64_bindings[i].source_name = "alternate_add";
			remapped_add_name = 1;
		}
	}
	const struct prototype_intrinsic_environment int64_environment = {
		.namespace_bindings = int64_bindings,
		.namespace_binding_count = default_environment->namespace_binding_count,
		.pure_primitives = default_environment->pure_primitives,
		.pure_primitive_count = default_environment->pure_primitive_count,
		.effect_operations = default_environment->effect_operations,
		.effect_operation_count = default_environment->effect_operation_count,
		.default_integer_host_type = PROTOTYPE_HOST_TYPE_INT64
	};
	if (!remapped_int || !remapped_add_name || prototype_intrinsic_environment_fingerprint(
			&int64_environment
		) == prototype_intrinsic_environment_fingerprint(default_environment)) {
		return 1;
	}

	/* Reinitialize the stores so the accepted image contains only evidence
	 * produced under the alternate immutable environment. */
	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_label_symbols, CASE_CAPACITY,
		case_binders, CASE_BINDER_CAPACITY, ih_scopes, MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, TYPE_CAPACITY, constructors, CONSTRUCTOR_CAPACITY,
		parameters, PARAMETER_CAPACITY, constructor_readbacks, CONSTRUCTOR_CAPACITY,
		field_types, FIELD_TYPE_CAPACITY, type_exprs, TYPE_EXPR_CAPACITY,
		type_representations, TYPE_CAPACITY, constructor_caches, CONSTRUCTOR_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement,
		judgement_relations,
		judgement_proofs,
		judgement_claims,
		judgement_derivations,
		JUDGEMENT_CAPACITY,
		judgement_candidate_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		judgement_accepted_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_context_db_init(&contexts, context_entries, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitutions, substitution_entries, SUBSTITUTION_CAPACITY
	);
	prototype_judgement_delta_init(
		&delta, &judgement, delta_relations, delta_proofs, JUDGEMENT_CAPACITY,
		delta_premises,
		JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motive_results, 8, computation_constraints, 8, effect_row_constraints, 8
	);
	prototype_judgement_delta_set_context_store(
		&delta, &contexts, &substitutions
	);
	prototype_judgement_delta_set_intrinsic_environment(
		&delta, &int64_environment
	);
	uint32_t int64_literal;
	uint32_t int64_classifier;
	uint32_t readback_primitive;
	FILE* readback_file = tmpfile();
	char readback[64];
	if (prototype_term_int_literal(&term_db, 42, &int64_literal) != 0 ||
		prototype_term_pure_primitive(
			&term_db, PROTOTYPE_PURE_PRIMITIVE_INT_ADD, -1, &readback_primitive
		) != 0 || !readback_file) {
		if (readback_file) {
			fclose(readback_file);
		}
		return 1;
	}
	prototype_term_print_debug(
		readback_file,
		&symbols,
		&int64_environment,
		&type_db,
		&term_db,
		readback_primitive
	);
	rewind(readback_file);
	if (!fgets(readback, sizeof(readback), readback_file) ||
		strcmp(readback, "PURE_PRIMITIVE(alternate_add)") != 0) {
		fclose(readback_file);
		return 1;
	}
	fclose(readback_file);
	if (
		prototype_judgement_delta_infer_core_helper_facts(
			&delta, &term_db, &type_db
		) != 0 || prototype_judgement_delta_commit(&delta, 0) != 0 ||
		prototype_judgement_publish_candidates(NULL, &judgement) != 0 ||
		prototype_judgement_lookup_authority_neutral_core_classifier(
			&judgement, int64_literal, &int64_classifier
		) != 0 || int64_classifier >= term_db.term_count ||
		term_db.terms[int64_classifier].tag != PROTOTYPE_TERM_PRIMITIVE_INT64 ||
		prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			&int64_environment,
			&contexts,
			&substitutions,
			NULL,
			&judgement
		) != 0 || prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			default_environment,
			&contexts,
			&substitutions,
			NULL,
			&judgement
		) == 0) {
		return 1;
	}
	symbol_table_free(&symbols);
	return 0;
}
