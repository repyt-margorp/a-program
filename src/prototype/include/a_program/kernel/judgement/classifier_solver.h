#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_CLASSIFIER_SOLVER_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_CLASSIFIER_SOLVER_H__

#include "a_program/kernel/judgement/types.h"

struct prototype_induction_hypothesis_resolution_request;
struct prototype_judgement_delta;
struct prototype_match_constructor_resolution;
struct prototype_match_resolution_request;

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

/* A recursive field may change family indices, but it must retain the same
 * nominal family and uniform parameter spine as the matched scrutinee. */
int prototype_judgement_classifier_is_recursive_family_instance(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	int* p_recursive
);
int prototype_judgement_classifier_is_strictly_positive_recursive_field(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	int* p_recursive
);

/* Lift one strictly-positive recursive field through its Pi codomains. The
 * direct recursive leaf is replaced by the Match motive applied to the field
 * value at that leaf; Pi domains are required to be non-recursive. */
int prototype_judgement_lift_recursive_field_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t field_value,
	uint32_t scrutinee_classifier,
	uint32_t motive,
	uint32_t* p_classifier
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
	uint32_t classifier,
	const uint32_t* branch_operation_ids,
	const struct prototype_judgement_selected_evidence* branch_evidence,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_count
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
	uint32_t child_operation_id,
	const struct prototype_judgement_selected_evidence* child_evidence
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
int prototype_judgement_delta_solve_recorded_computation_requests(
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
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
);

#endif
