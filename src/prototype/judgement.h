#ifndef __PROTOTYPE_JUDGEMENT_H__
#define __PROTOTYPE_JUDGEMENT_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "symbol.h"
#include "term.h"
#include "type_declaration.h"

struct prototype_operation_graph;

enum prototype_judgement_kind {
	PROTOTYPE_JUDGEMENT_KIND_UNKNOWN = 0,
	PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
	PROTOTYPE_JUDGEMENT_KIND_IS_TYPE
};

enum prototype_judgement_proof_kind {
	PROTOTYPE_JUDGEMENT_PROOF_INVALID = 0,
	PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION,
	PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION,
	PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE,
	PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
	/* Explicit expected-type boundary. Unlike CONVERSION, this may discharge
	 * universe/effect metavariable constraints before exposing the expected
	 * classifier. It is not object equality. */
	PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY,
	PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
	PROTOTYPE_JUDGEMENT_PROOF_DECLARATION,
	PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY,
	PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO,
	PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN,
	PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN
};

enum prototype_judgement_authority_kind {
	PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID = 0,
	PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
	PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING,
	PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
	PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
	PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
	PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE,
	PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
	PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT
};

#define PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES 31
#define PROTOTYPE_COMPUTATION_FOLD_STRUCTURAL_PREMISE_COUNT(clause_count) \
	(2u + 2u * (clause_count))
#define PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES \
	PROTOTYPE_COMPUTATION_FOLD_STRUCTURAL_PREMISE_COUNT( \
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES \
	)

/* Solver-local candidate. Premise tuples are permitted here because they have
 * not crossed the accepted certificate boundary. */
struct prototype_judgement_derivation_candidate {
	int proof_kind;
	int conclusion_kind;
	uint32_t conclusion_context_id;
	/* INVALID denotes a non-Operation kernel/declaration fact. */
	uint32_t conclusion_operation_id;
	uint32_t conclusion_subject;
	uint32_t conclusion_classifier;
	/* Reserved v61 field. Binding identity is the conclusion VAR edge. */
	uint32_t reserved_legacy_assumption_level;
	/* Rule parameters for Match-pattern assumptions. The Match core erases
	 * owner views, so the derivation retains the selected declaration. */
	uint32_t constructor_owner_view;
	uint32_t constructor_index;
	uint32_t constructor_field_index;
	/* Rule parameters for guarded induction-hypothesis elimination. */
	uint32_t induction_match;
	uint32_t induction_case_index;
	uint32_t induction_field_index;
	uint32_t premise_count;
	int premise_kinds[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_context_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_operation_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_proof_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
};

struct prototype_judgement_claim_candidate {
	int kind;
	uint32_t context_id;
	/* Typed source/generated occurrence. The Core subject remains the erased
	 * computation projection and is never used to recover this identity. */
	uint32_t operation_id;
	uint32_t subject;
	uint32_t classifier;
	int proof_kind;
	uint32_t proof_id;
};

/* Transitional candidate spellings used only until all producers and the v62
 * reader have moved to the explicit candidate names. */
struct prototype_judgement_relation {
	int kind;
	uint32_t context_id;
	uint32_t operation_id;
	uint32_t subject;
	uint32_t classifier;
	int proof_kind;
	uint32_t proof_id;
};

struct prototype_judgement_proof {
	int proof_kind;
	int conclusion_kind;
	uint32_t conclusion_context_id;
	uint32_t conclusion_operation_id;
	uint32_t conclusion_subject;
	uint32_t conclusion_classifier;
	uint32_t reserved_legacy_assumption_level;
	uint32_t constructor_owner_view;
	uint32_t constructor_index;
	uint32_t constructor_field_index;
	uint32_t induction_match;
	uint32_t induction_case_index;
	uint32_t induction_field_index;
	uint32_t premise_count;
	int premise_kinds[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_context_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_operation_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_proof_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
};

/* Accepted proposition identity. Rule identity is deliberately absent. */
struct prototype_judgement_claim {
	int kind;
	int authority_kind;
	uint32_t authority_id;
	uint32_t context_id;
	uint32_t operation_id;
	uint32_t subject;
	uint32_t classifier;
	uint32_t closure_rank;
};

/* Accepted rule application. During the P0 transition, source_claim_ids are
 * reconstructed from resolved legacy edges. The final representation keeps
 * only irreducible derived sources; structural sources come from OperationGraph. */
struct prototype_judgement_derivation {
	int proof_kind;
	uint32_t conclusion_claim_id;
	uint32_t closure_rank;
	uint32_t reserved_legacy_assumption_level;
	uint32_t constructor_owner_view;
	uint32_t constructor_index;
	uint32_t constructor_field_index;
	uint32_t induction_match;
	uint32_t induction_case_index;
	uint32_t induction_field_index;
	uint32_t source_claim_count;
	uint32_t source_claim_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
};

struct prototype_judgement_match_motive_result {
	uint32_t match_term;
	uint32_t classifier;
};

enum prototype_judgement_computation_constraint_kind {
	PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD = 1,
	PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST
};

/* A computation constraint records the two operands required to solve a CBPV
 * computation judgement. It is compiler-local state, not a TermDB node and
 * not a runtime environment. */
struct prototype_judgement_computation_constraint {
	int kind;
	uint32_t context_id;
	/* Operation occurrence which generated this constraint.  INVALID is
	 * reserved for authority-neutral constraints generated by a raw TermDB
	 * scan. */
	uint32_t operation_id;
	/* Structural operands remain Operation-owned even though the kernel solver
	 * also receives their erased TermDB projections below. */
	uint32_t premise_operation_count;
	uint32_t premise_operations[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	/* Current fixed-point operands selected by the exact premise Operations.
	 * These are refreshed by the Operation solver before each kernel pass. */
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t subject;
	uint32_t computation;
	uint32_t continuation;
	uint32_t argument;
	uint32_t application;
	/* An unresolved fold residual is solver-local state. It must not be
	 * represented by a fresh EFFECT_ROW_VAR in TermDB. */
	int effect_residual_pending;
	uint32_t effect_residual_row;
	/* Current solved approximation of a clause-bearing fold's output row. */
	uint32_t effect_output_row;
	/* Solver output for this exact Operation occurrence. Relations emitted while
	 * validating the same rule are evidence candidates, not the source of this
	 * classifier. */
	uint32_t solved_classifier;
};

enum prototype_judgement_effect_row_constraint_kind {
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN = 1,
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL,
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION
};

/* Effect-row constraints belong to the compile-time solver. JOIN is n-ary;
 * RESIDUAL uses input and removed rows; INCLUSION uses source and target rows.
 * They are neither TermDB nodes nor runtime handler state. */
#define PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS 34

struct prototype_judgement_effect_row_constraint {
	int kind;
	uint32_t subject;
	uint32_t result_row;
	uint32_t operand_count;
	uint32_t operands[PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS];
	int solved;
};

struct prototype_judgement_db {
	/* Transitional source image while all consumers move to Claims. */
	struct prototype_judgement_relation* relations;
	struct prototype_judgement_proof* proofs;
	size_t relation_count;
	size_t relation_capacity;
	size_t proof_count;
	size_t proof_capacity;
	struct prototype_judgement_claim* claims;
	struct prototype_judgement_derivation* derivations;
	size_t claim_count;
	size_t claim_capacity;
	size_t derivation_count;
	size_t derivation_capacity;

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
	struct prototype_judgement_computation_constraint* computation_constraints;
	struct prototype_judgement_effect_row_constraint* effect_row_constraints;
	size_t relation_count;
	size_t relation_capacity;
	size_t proof_count;
	size_t proof_capacity;
	size_t match_motive_result_count;
	size_t match_motive_result_capacity;
	size_t computation_constraint_count;
	size_t computation_constraint_capacity;
	size_t effect_row_constraint_count;
	size_t effect_row_constraint_capacity;
	uint64_t solver_step_limit;
	uint64_t* solver_steps_used;
	int* solver_exhausted;
	struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	/* Context for relations emitted by the current elaboration rule. This is
	 * an explicit CwF object ID, not the old proof provenance fields. */
	uint32_t current_context_id;
	/* Operation identity for source/generated typing materialization. */
	uint32_t current_operation_id;
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
	uint32_t ih_scope_id;
	uint32_t argument;
};

void prototype_judgement_db_init(
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	struct prototype_judgement_claim* claims,
	struct prototype_judgement_derivation* derivations,
	size_t claim_capacity
);

void prototype_judgement_delta_init(
	struct prototype_judgement_delta* delta,
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	size_t relation_capacity,
	struct prototype_judgement_match_motive_result* match_motive_results,
	size_t match_motive_result_capacity,
	struct prototype_judgement_computation_constraint* computation_constraints,
	size_t computation_constraint_capacity,
	struct prototype_judgement_effect_row_constraint* effect_row_constraints,
	size_t effect_row_constraint_capacity
);

void prototype_judgement_delta_set_solver_budget(
	struct prototype_judgement_delta* delta,
	uint64_t step_limit,
	uint64_t* steps_used,
	int* exhausted
);
void prototype_judgement_delta_set_context(
	struct prototype_judgement_delta* delta,
	uint32_t context_id
);
void prototype_judgement_delta_set_operation(
	struct prototype_judgement_delta* delta,
	uint32_t operation_id
);
void prototype_judgement_delta_set_context_store(
	struct prototype_judgement_delta* delta,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions
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
	uint32_t subject,
	uint32_t source_classifier,
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
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_subject,
	uint32_t body_subject,
	uint32_t binder_classifier,
	uint32_t body_classifier,
	uint32_t premise_context_id
);

int prototype_judgement_delta_record_app_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	uint32_t function_subject,
	uint32_t function_classifier,
	uint32_t argument_subject,
	uint32_t argument_classifier
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
	uint32_t subject,
	uint32_t classifier,
	uint32_t source_context_id
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
int prototype_judgement_delta_record_constructor_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count
);
int prototype_judgement_delta_record_computation_fold_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_classifiers,
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
	uint32_t subject,
	uint32_t selected_classifier,
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
	uint32_t match_term,
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
	uint32_t source_operation_id,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
);

int prototype_judgement_delta_record_expected_type_exposure(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_operation_id,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
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

int prototype_judgement_validate_proofs(
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

void prototype_judgement_resolve_proof_edges(
	struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations
);

int prototype_judgement_ground_claims(
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
);

void prototype_judgement_finalize_linked_declaration_premises(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

void prototype_judgement_delta_drop_temporary_derivations(
	struct prototype_judgement_delta* delta,
	const struct prototype_operation_graph* operations,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

void prototype_judgement_delta_drop_unsupported_derivations(
	struct prototype_judgement_delta* delta,
	const struct prototype_operation_graph* operations,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
);

int prototype_judgement_lookup_authority_neutral_core_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
);

int prototype_judgement_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
);

struct prototype_term_conversion_result prototype_judgement_classifier_conversion(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
);

struct prototype_term_conversion_result
prototype_judgement_classifier_conversion_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
);

/* A conversion goal records the complete deterministic kernel request. It is
 * compiler evidence only: executing it never creates an object-level equality
 * witness or a JudgementDB relation. */
struct prototype_kernel_conversion_goal {
	uint32_t id;
	uint32_t context_id;
	uint32_t carrier_classifier;
	uint32_t left_term;
	uint32_t right_term;
	int normalization_profile;
	uint64_t step_limit;
	struct prototype_term_conversion_result result;
};

int prototype_judgement_kernel_conversion_goal_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_kernel_conversion_goal* goal,
	int require_carrier
);

int prototype_judgement_kernel_conversion_goal_execute(
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_kernel_conversion_goal* goal,
	int require_carrier
);

enum prototype_hott_goal_kind {
	PROTOTYPE_HOTT_GOAL_VALUE_OBSERVATION = 1,
	PROTOTYPE_HOTT_GOAL_COMPUTATION_OBSERVATION,
	PROTOTYPE_HOTT_GOAL_TYPE_ACTION,
	PROTOTYPE_HOTT_GOAL_TERM_ACTION
};

enum prototype_hott_goal_state {
	PROTOTYPE_HOTT_GOAL_PENDING = 1,
	PROTOTYPE_HOTT_GOAL_SOLVED,
	PROTOTYPE_HOTT_GOAL_RESIDUAL,
	PROTOTYPE_HOTT_GOAL_CONTRADICTION,
	PROTOTYPE_HOTT_GOAL_UNSUPPORTED
};

enum prototype_hott_residual_reason {
	PROTOTYPE_HOTT_RESIDUAL_NONE = 0,
	PROTOTYPE_HOTT_RESIDUAL_CONVERSION,
	PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
	PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL,
	PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED,
	PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION,
	PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST,
	PROTOTYPE_HOTT_RESIDUAL_COMPUTATION_FOLD,
	PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE,
	PROTOTYPE_HOTT_RESIDUAL_TYPE_VIEW,
	PROTOTYPE_HOTT_RESIDUAL_UNIVERSE,
	PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE
};

enum prototype_hott_local_rule {
	PROTOTYPE_HOTT_LOCAL_RULE_NONE = 0,
	PROTOTYPE_HOTT_LOCAL_RULE_OBSERVATION_DIAGONAL,
	PROTOTYPE_HOTT_LOCAL_RULE_OBSERVATION_CONVERT,
	PROTOTYPE_HOTT_LOCAL_RULE_PI,
	PROTOTYPE_HOTT_LOCAL_RULE_ADT,
	PROTOTYPE_HOTT_LOCAL_RULE_PURE_COMPUTATION
};

struct prototype_hott_goal {
	uint32_t id;
	int kind;
	int state;
	int residual_reason;
	uint32_t source_ast;
	uint32_t context_id;
	uint32_t carrier_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t bridge_context_id;
	uint32_t left_endpoint_substitution;
	uint32_t right_endpoint_substitution;
	uint32_t parent_goal_id;
	int local_type_former_rule;
	int normalization_profile;
	uint64_t step_limit;
	struct prototype_term_conversion_result conversion_result;
};

struct prototype_hott_goal_db {
	struct prototype_hott_goal* goals;
	size_t goal_count;
	size_t goal_capacity;
};

void prototype_hott_goal_db_init(
	struct prototype_hott_goal_db* db,
	struct prototype_hott_goal* goals,
	size_t goal_capacity
);
size_t prototype_hott_goal_db_mark(const struct prototype_hott_goal_db* db);
void prototype_hott_goal_db_rewind(
	struct prototype_hott_goal_db* db,
	size_t mark
);
const struct prototype_hott_goal* prototype_hott_goal_db_get(
	const struct prototype_hott_goal_db* db,
	uint32_t goal_id
);
int prototype_hott_goal_db_add(
	struct prototype_hott_goal_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms,
	struct prototype_hott_goal goal,
	uint32_t* p_goal_id
);
int prototype_hott_goal_db_validate(
	const struct prototype_hott_goal_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms
);
int prototype_hott_goal_classify_admission(
	const struct prototype_term_db* terms,
	struct prototype_hott_goal* goal
);
int prototype_hott_goal_apply_conversion_result(
	struct prototype_hott_goal* goal,
	struct prototype_term_conversion_result result
);

struct prototype_hott_residual_obligation {
	uint32_t obligation_id;
	uint32_t parent_obligation_id;
	int validation_rule;
	uint32_t context_id;
	uint32_t carrier_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t bridge_context_id;
	uint32_t left_endpoint_substitution;
	uint32_t right_endpoint_substitution;
	int normalization_profile;
	uint64_t step_limit;
	uint64_t steps_used;
	int residual_reason;
	uint32_t source_ast;
	uint64_t calculus_fingerprint_candidate;
};

struct prototype_hott_residual_db {
	struct prototype_hott_residual_obligation* obligations;
	size_t obligation_count;
	size_t obligation_capacity;
};

void prototype_hott_residual_db_init(
	struct prototype_hott_residual_db* db,
	struct prototype_hott_residual_obligation* obligations,
	size_t obligation_capacity
);
int prototype_hott_residual_db_add_from_goal(
	struct prototype_hott_residual_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms,
	const struct prototype_hott_goal* goal,
	uint32_t* p_obligation_id
);
int prototype_hott_residual_db_validate(
	const struct prototype_hott_residual_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms
);
int prototype_hott_residual_db_require_artifact_v62(
	const struct prototype_hott_residual_db* db
);

/* Normalize a classifier expression at the pure type profile and expose the
 * value type returned by a type-family computation. */
int prototype_judgement_classifier_value_whnf(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_value_classifier
);

/* Elaboration equality for an unresolved qualified type reference and the
 * imported TYPE_VIEW carrying the same identity. This is not DefEq. */
int prototype_judgement_classifier_reference_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
);

/* Classify a synthesized classifier after the kernel conversion profile has
 * exposed its outer constructor. This is the sole value/computation/type
 * boundary; JudgementDB continues to store one HAS_TYPE relation. */
int prototype_judgement_classifier_view(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret
);

/*
 * Solve free effect-row variables in an elaborated expected classifier from
 * the corresponding rows of an actual classifier. Returns 0 with a concrete
 * classifier, 1 when the classifier shapes do not determine a compatible
 * solution, and -1 for malformed input. This is elaboration/constraint
 * solving, not kernel compatibility.
 */
int prototype_judgement_solve_expected_effect_rows(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual,
	uint32_t* p_solved_expected
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

/* Instantiate classifier-only implicit effect-row binders from a function
 * value argument. Returns 0 when no binders remain or specialization succeeds,
 * 1 when the argument does not determine the row, and -1 on malformed input. */
int prototype_judgement_specialize_effect_rows_for_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t* p_ret
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
	const struct prototype_context_db* contexts,
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

/* Infer authority-neutral Core helper facts. This API never publishes a
 * source-Operation derivation; callers must reify any selected result through
 * the exact OperationGraph occurrence before commit. */
int prototype_judgement_delta_infer_core_helper_facts(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

/* Infer only the CBPV boundary nodes from already materialized child facts.
 * The source-operation compiler uses this after its own solver commits, so
 * it does not create competing derivations for legacy type formation. */
int prototype_judgement_delta_infer_cbpv_boundaries(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

/* Classify one RETURN/THUNK/FORCE boundary from its occurrence-selected
 * child classifier. This is the shared kernel rule used by both the source
 * operation constraint solver and JudgementDB proof materialization. */
int prototype_judgement_cbpv_boundary_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t child_classifier,
	uint32_t* p_classifier
);

/* Record one CBPV boundary rule from an occurrence-scoped child classifier.
 * This is used when several source operations intentionally share the same
 * erased RETURN/THUNK/FORCE node in TermDB. */
int prototype_judgement_delta_record_cbpv_boundary(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t child_classifier
);

/* Infer CBPV boundary nodes and solve computation-fold/request constraints using
 * already materialized child derivations. Unlike the Core helper-fact closure,
 * this does not re-run general type formation, APP, or LAMBDA inference. */
int prototype_judgement_delta_infer_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);

/* Solve COMPUTATION_FOLD and OPERATION_REQUEST constraints after source lowering
 * has materialized the occurrence-selected CBPV boundary derivations. */
int prototype_judgement_delta_solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);
int prototype_judgement_delta_solve_recorded_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);
int prototype_judgement_delta_record_computation_constraint(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject
);

int prototype_judgement_delta_generate_computation_constraints(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms
);

void prototype_judgement_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
);

#endif
