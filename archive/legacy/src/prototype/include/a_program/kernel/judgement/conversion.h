#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_CONVERSION_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_CONVERSION_H__

#include "a_program/kernel/judgement/conversion_goal.h"
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

/* Validate one guarded recursive classifier equation. This relation unfolds
 * only IH back-edges exposed by a finite recursive Match graph; it is not
 * definitional equality and must only be used by rules that already establish
 * guardedness. Returns 1 for a valid equation, 0 for a mismatch, and -1 for a
 * malformed graph. */
int prototype_judgement_guarded_classifier_equation(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual,
	uint32_t* p_validated_expected
);

int prototype_judgement_kernel_conversion_goal_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_typing_conversion_goal* goal,
	int require_carrier
);

int prototype_judgement_kernel_conversion_goal_execute(
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_typing_conversion_goal* goal,
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

int prototype_judgement_expose_callable_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_ret
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
	struct prototype_classifier_view* p_ret
);

/* Inspect an already exposed classifier without normalization. This is a
 * Layer T operation even though the classifier expression is stored in
 * TermDB. */
int prototype_judgement_classifier_view_syntax(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	struct prototype_classifier_view* p_ret
);

/*
 * Solve free effect-row variables in an elaborated expected classifier from
 * the corresponding rows of an actual classifier. Returns 0 with a concrete
 * classifier, 1 when the classifier shapes do not determine a complete
 * compatible solution, and -1 for malformed input. On 1, p_solved_expected
 * still receives the projection of every row equation established before the
 * shape mismatch. The caller must validate the remaining classifier structure.
 * This is elaboration/constraint solving, not kernel compatibility.
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

/* Return 1 when superset has the same classifier structure as subset and every
 * corresponding computation effect row includes the subset row, 0 when it does
 * not, and -1 for malformed input. This is the ordering used by elaboration for
 * effect weakening; it is not definitional equality. */
int prototype_judgement_classifier_effect_row_includes(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t superset,
	uint32_t subset
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

/* Apply a pure, total type-family code during classifier formation. This is
 * prior computation: it interprets a suspended family value without weakening
 * the runtime APP/FORCE boundary. Returns 1 when the classifier is not such a
 * family or its domain is incompatible. */
int prototype_judgement_static_family_app_classifier(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t function_classifier,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result_classifier
);

#endif
