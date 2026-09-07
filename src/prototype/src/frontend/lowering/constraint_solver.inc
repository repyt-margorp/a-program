/* Constraint lowering remains one translation unit. The included modules are
 * ordered by authority and dependency, not by public compilation phases. */
static int operation_solver_occurrence_is_in_impossible_branch(
	const struct compile_context* ctx,
	uint32_t occurrence_id
);

#include "constraint/model_generation_and_index.inc"
#include "constraint/effect_propagation_and_residuals.inc"
#include "constraint/classifier_and_computation_propagation.inc"
#include "constraint/branch_refinement_and_motives.inc"
#include "constraint/motive_solver.inc"
#include "constraint/evidence_and_freeze.inc"
#include "constraint/context_computation_and_fixed_point.inc"
