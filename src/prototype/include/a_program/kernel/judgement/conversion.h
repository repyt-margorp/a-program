#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_CONVERSION_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_CONVERSION_H__

#include "a_program/kernel/judgement/types.h"

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

/* Select the term substituted into a dependent classifier family. Explicit
 * lambda quotation remains a runtime THUNK, but its family index is the pure
 * lambda computation represented by that quotation. This is elaboration-only;
 * it does not add THUNK/FORCE conversion. */
int prototype_judgement_dependent_classifier_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_domain,
	uint32_t argument,
	uint32_t* p_ret
);

#endif
