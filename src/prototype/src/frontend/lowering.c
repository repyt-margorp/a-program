/* Lowering remains one translation unit to preserve solver state and graph-ID
 * allocation order while making the established phases physically visible. */
#include "a_program/core/pipeline.h"
#include "a_program/core/request_provider.h"
#include "a_program/frontend/core_lowering.h"
#include "a_program/frontend/context_projection_builder.h"
#include "a_program/frontend/source_core_handoff.h"
#include "a_program/frontend/source_lowering_plan.h"
#include "a_program/frontend/source_schedule.h"
#include "a_program/frontend/typing_source_results.h"
#include "a_program/frontend/universe_collection.h"
#include "a_program/kernel/cwf_certificate.h"
#include "a_program/kernel/type_term_debug.h"

struct compile_context;
struct prototype_typing_constraint;

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);
static uint32_t operation_available_classifier(
	const struct compile_context* ctx,
	uint32_t occurrence_id
);
static uint32_t operation_solver_publication_projection(
	const struct compile_context* ctx,
	uint32_t occurrence_id
);
static uint32_t operation_solver_classifier_for_projection(
	const struct compile_context* ctx,
	uint32_t typed_projection
);
static int operation_solver_classifier_is_strictly_more_general(
	const struct prototype_term_db* terms,
	uint32_t candidate,
	uint32_t current,
	int* p_more_general
);
static int operation_effect_apply_materialized_solution(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t classifier,
	uint32_t* p_classifier
);
static int operation_solver_projected_child(
	const struct compile_context* ctx,
	uint32_t parent_typed_projection,
	int role,
	uint32_t ordinal,
	uint32_t* p_child_typed_projection
);
static int operation_solver_enqueue_context_projection_dependents(
	struct compile_context* ctx,
	uint32_t context_projection
);
static uint32_t operation_solver_constraint_operand_projection(
	const struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	uint32_t index
);
static int compile_generated_contract_close_effect_rows(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t* p_classifier
);

#include "lowering/context_and_type_lowering.inc"
#include "lowering/function_graph_generation.inc"
#include "lowering/graph_construction.inc"
#include "lowering/constraint_solver.inc"
#include "lowering/finalization_and_entrypoints.inc"
