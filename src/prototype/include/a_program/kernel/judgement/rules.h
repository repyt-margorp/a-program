#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_RULES_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_RULES_H__

#include "a_program/kernel/judgement/db.h"

int prototype_judgement_expand_type_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);
int prototype_judgement_add_type_formation_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
);
int prototype_judgement_expand_constructor_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);
int prototype_judgement_add_constructor_intro_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
);
int prototype_judgement_add_constructor_spine_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_claim_ids,
	uint32_t argument_claim_count,
	uint32_t* p_claim_id
);
int prototype_judgement_indexed_branch_refinement(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee_context_id,
	uint32_t scrutinee_term,
	uint32_t scrutinee_classifier,
	uint32_t constructor_index,
	uint32_t branch_context_id,
	const struct prototype_case_binder* branch_binders,
	uint32_t branch_binder_count,
	uint32_t* p_refined_context_id,
	uint32_t* p_refinement_substitution_id,
	uint32_t* p_constructor_term
);

int prototype_judgement_delta_record_context_binding_assumption(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t binding_id,
	uint32_t classifier
);

int prototype_judgement_delta_record_match_pattern(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t constructor_owner_view,
	uint32_t constructor_index,
	uint32_t constructor_field_index
);

int prototype_judgement_delta_record_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t target_classifier
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

/* Materialize a source-operation derivation without choosing premises by
 * core-term identity. The caller supplies the already solved classifiers of
 * the source binder/body or function/argument occurrence. */
int prototype_judgement_delta_record_lambda_intro(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t conclusion_operation_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_subject,
	uint32_t body_subject,
	const struct prototype_judgement_selected_evidence* binder_evidence,
	uint32_t body_operation_id,
	const struct prototype_judgement_selected_evidence* body_evidence
);

int prototype_judgement_delta_record_app_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const struct prototype_judgement_selected_evidence* function_evidence,
	const struct prototype_judgement_selected_evidence* argument_evidence
);

int prototype_judgement_delta_app_elim_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_subject,
	uint32_t argument_classifier,
	uint32_t* p_classifier
);

int prototype_judgement_specialize_fold_operation_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_row,
	int operation_id,
	uint32_t classifier,
	uint32_t* p_specialized
);
int prototype_judgement_delta_record_context_weaken(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t substitution_id
);
int prototype_judgement_add_context_weakened_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t target_context_id,
	uint32_t projection_substitution_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_reindexed_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t substitution_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_context_binding_assumption(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t* p_claim_id
);
int prototype_judgement_add_lambda_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_claim_id,
	uint32_t body_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_app_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t function_claim_id,
	uint32_t argument_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_return_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_thunk_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t computation_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_force_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_computation_fold_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	uint32_t* p_claim_id
);
int prototype_judgement_add_conversion_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t source_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
);
int prototype_judgement_add_indexed_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
);
int prototype_judgement_add_induction_hypothesis_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_type_formation(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t relation_type,
	uint32_t universe,
	uint32_t left_type_claim_id,
	uint32_t right_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_witness_intro(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_constructor_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	const uint32_t* field_witness_claim_ids,
	uint32_t field_witness_count,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_unary_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t child_witness_claim_id,
	int proof_kind,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_app_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t function_witness_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_lambda_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t body_witness_claim_id,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_match_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_match_claim_id,
	uint32_t scrutinee_witness_claim_id,
	const uint32_t* case_witness_claim_ids,
	uint32_t case_witness_count,
	uint32_t* p_claim_id
);
int prototype_judgement_add_relation_induction_hypothesis_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_induction_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
);
/* Select one complete Claim authority from the provisional and committed
 * candidate images. Returns 0 for one Claim, 1 for missing, 2 for ambiguous,
 * and -1 for malformed input. */
int prototype_judgement_delta_select_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
);
int prototype_judgement_select_evidence(
	const struct prototype_judgement_db* judgement,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
);

int prototype_judgement_constructor_spine_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t subject,
	uint32_t constructor_owner_view,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count,
	uint32_t* p_classifier,
	int* p_saturated
);
int prototype_judgement_constructor_field_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
);
int prototype_judgement_delta_record_constructor_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_operation_ids,
	const struct prototype_judgement_selected_evidence* argument_evidence,
	uint32_t argument_count
);
int prototype_judgement_delta_record_computation_fold_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_operation_ids,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
);

int prototype_judgement_computation_fold_result_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t computation,
	uint32_t computation_classifier,
	uint32_t continuation_classifier,
	uint32_t* p_classifier
);

/* Materialize a solved type-formation fact selected by the operation solver.
 * A saturated type instance is classified by a universe term; an unapplied
 * type former is classified by its declared Pi-family. Level inequalities are
 * checked by the separate UniverseDB constraint solver after linking. */
int prototype_judgement_delta_record_type_formation(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

/* Synthesize an exact type-formation candidate in the current Context. This
 * is prior computation over the ordinary typing rules; callers must publish
 * and select its Claim before using it as certificate authority. */
int prototype_judgement_delta_ensure_type_at_universe(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t universe
);

/* Run ordinary structural type formation as prior computation and append its
 * complete Claim/Derivation DAG without renumbering accepted evidence. */
int prototype_judgement_add_structural_type_formation_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t* p_claim_id
);

int prototype_judgement_delta_record_pure_primitive_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);
int prototype_judgement_delta_record_effect_operation_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_record_text_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_record_int_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_record_int_literal_admissibility(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t admissible_classifier
);

int prototype_judgement_pure_primitive_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term* operation,
	uint32_t* p_classifier
);
int prototype_judgement_effect_operation_classifier(
	struct prototype_term_db* terms,
	const struct prototype_term* operation,
	uint32_t* p_ret
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

/* Build the constant dependent motive (lambda _ : S. P) scrutinee together
 * with its ordinary Lambda/APP formation derivation. */
int prototype_judgement_delta_build_constant_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	uint32_t result_type,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
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
	uint32_t match_term,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index
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

int prototype_judgement_expand_primitives(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms
);

int prototype_judgement_record_declaration_fact(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_add_expected_type_exposure(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t operation_id,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
);

int prototype_judgement_delta_record_expected_type_exposure(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
);

int prototype_judgement_delta_record_declaration_fact(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
);

int prototype_judgement_delta_has_pending_classifier_state(
	const struct prototype_judgement_delta* delta
);

int prototype_judgement_validate_accepted_graph(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

/* Validate one selected typed occurrence from its authoritative OperationGraph
 * identity. TermDB fields are checked only as erased projections. */
int prototype_judgement_validate_operation_typing(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	const struct prototype_operation_graph* operations,
	uint32_t operation_id
);

int prototype_judgement_publish_candidates(
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
);
int prototype_judgement_add_is_type_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t has_type_claim_id,
	uint32_t* p_claim_id
);

int prototype_judgement_lookup_authority_neutral_core_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_recompute_closure_ranks(
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

#endif
