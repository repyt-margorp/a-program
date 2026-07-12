#ifndef __PROTOTYPE_JUDGEMENT_H__
#define __PROTOTYPE_JUDGEMENT_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "symbol.h"
#include "term.h"
#include "type_declaration.h"

enum prototype_judgement_kind {
	PROTOTYPE_JUDGEMENT_KIND_UNKNOWN = 0,
	PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
	PROTOTYPE_JUDGEMENT_KIND_IS_TYPE
};

enum prototype_judgement_proof_kind {
	PROTOTYPE_JUDGEMENT_PROOF_INVALID = 0,
	PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION,
	PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE,
	PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_INTRINSIC_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
	PROTOTYPE_JUDGEMENT_PROOF_DECLARATION,
	PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY,
	PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO
};

enum prototype_judgement_proof_context_kind {
	PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_NONE = 0,
	PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_LAMBDA_BINDER,
	PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_MATCH_CASE_FIELD
};

#define PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES 65

struct prototype_judgement_proof {
	int proof_kind;
	int conclusion_kind;
	uint32_t conclusion_subject;
	uint32_t conclusion_classifier;
	int context_kind;
	uint32_t context_subject;
	uint32_t context_index;
	uint32_t context_aux;
	uint32_t premise_count;
	int premise_kinds[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_proof_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
};

struct prototype_judgement_relation {
	int kind;
	uint32_t subject;
	uint32_t classifier;
	int proof_kind;
	uint32_t proof_id;
};

struct prototype_judgement_match_motive_result {
	uint32_t match_term;
	uint32_t classifier;
};

struct prototype_judgement_db {
	struct prototype_judgement_relation* relations;
	struct prototype_judgement_proof* proofs;
	size_t relation_count;
	size_t relation_capacity;
	size_t proof_count;
	size_t proof_capacity;

	uint32_t next_universe_var;
};

/* Temporary overlay for judgement facts produced while compiling one graph
 * fragment. Successful paths commit the delta into JudgementDB; failed paths
 * rewind it. This is not a semantic typing context. */
struct prototype_judgement_delta {
	struct prototype_judgement_db* db;
	struct prototype_judgement_relation* relations;
	struct prototype_judgement_proof* proofs;
	struct prototype_judgement_match_motive_result* match_motive_results;
	size_t relation_count;
	size_t relation_capacity;
	size_t proof_count;
	size_t proof_capacity;
	size_t match_motive_result_count;
	size_t match_motive_result_capacity;
};

struct prototype_match_constructor_resolution {
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t field_count;
};

struct prototype_match_resolution_request {
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	uint32_t scrutinee_classifier;
	int constructor_symbol_id;
};

struct prototype_induction_hypothesis_resolution_request {
	uint32_t subject;
	uint32_t frame_id;
	uint32_t argument;
};

void prototype_judgement_db_init(
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	size_t relation_capacity
);

void prototype_judgement_delta_init(
	struct prototype_judgement_delta* delta,
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	size_t relation_capacity,
	struct prototype_judgement_match_motive_result* match_motive_results,
	size_t match_motive_result_capacity
);

size_t prototype_judgement_delta_mark(
	const struct prototype_judgement_delta* delta
);

void prototype_judgement_delta_rewind(
	struct prototype_judgement_delta* delta,
	size_t mark
);

int prototype_judgement_delta_commit(
	struct prototype_judgement_delta* delta,
	size_t mark
);

int prototype_judgement_expand_type_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_expand_constructor_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_lambda_binder(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_match_pattern(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_set_proof_context_by_id(
	struct prototype_judgement_delta* delta,
	uint32_t proof_id,
	int context_kind,
	uint32_t context_subject,
	uint32_t context_index,
	uint32_t context_aux
);

int prototype_judgement_expand_lambda(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_lambda(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

/* Returns 0 when an APP classifier was synthesized and registered, 1 when
 * current JudgementDB facts are insufficient, and -1 for malformed input. */
int prototype_judgement_expand_app(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_delta_expand_app(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_expand_match_motive(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_build_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	const struct prototype_match_case_input* motive_cases,
	uint32_t case_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
);

int prototype_judgement_prepare_match_motive_case(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_case_binder* source_binders,
	const struct prototype_case_binder* motive_binders,
	uint32_t binder_count,
	uint32_t branch_classifier,
	uint32_t* p_motive_body
);

int prototype_judgement_delta_type_match_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
);

int prototype_judgement_delta_build_match_motive_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
);

int prototype_judgement_delta_build_match_motive_from_known_branches(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
);

/*
 * Build a uniform motive from classifiers attached to source operation
 * branches.  INVALID entries are unresolved recursive branches; they are
 * constrained by the synthesized motive rather than read from unrelated
 * JudgementDB facts sharing the same core term.
 */
int prototype_judgement_delta_build_match_motive_from_branch_hints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	const uint32_t* branch_classifiers,
	uint32_t branch_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
);
int prototype_judgement_delta_expand_match_with_branch_hints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* branch_classifiers,
	uint32_t branch_count
);

int prototype_judgement_expand_match(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_match(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_induction_hypothesis(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t context_subject,
	uint32_t context_index,
	uint32_t context_aux
);

int prototype_judgement_expand_text_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_expand_int_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_expand_negative_intrinsic(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_expand_primitives(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms
);

int prototype_judgement_expand_checked(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_add_conversion(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_delta_expand_checked(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_has_pending_classifier_state(
	const struct prototype_judgement_delta* delta
);

int prototype_judgement_validate_proofs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
);

void prototype_judgement_resolve_proof_edges(struct prototype_judgement_db* judgement);

int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

void prototype_judgement_refresh_app_elim_premises(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

void prototype_judgement_resolve_declaration_premises(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

void prototype_judgement_delta_drop_temporary_derivations(
	struct prototype_judgement_delta* delta
);

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
);

int prototype_judgement_lookup_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_delta_lookup_classifier(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
);

int prototype_judgement_classifier_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_classifier_normalization_equal_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_classifier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_classifier_compatible_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_type_expr_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
);

int prototype_judgement_resolve_match_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	struct prototype_match_constructor_resolution* p_resolution
);

int prototype_judgement_synthesize_match_pattern_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	uint32_t field_index,
	uint32_t* p_classifier
);

int prototype_judgement_resolve_match_case_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_resolution_request* request,
	struct prototype_match_constructor_resolution* p_resolution
);

int prototype_judgement_delta_resolve_induction_hypothesis_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_induction_hypothesis_resolution_request* request
);

int prototype_judgement_delta_record_materialized_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
);

int prototype_judgement_delta_infer_term_classifiers(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

void prototype_judgement_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
);

#endif
