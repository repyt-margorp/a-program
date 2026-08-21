#include "a_program/core/term.h"
#include "a_program/dimension/operator.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/type_declaration.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define JUDGEMENT_CAPACITY 128
#define PREMISE_CAPACITY \
	(JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES)

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[8];
static int case_labels[8];
static struct prototype_case_binder case_binders[8];
static struct prototype_ih_scope ih_scopes[8];
static struct prototype_type_declaration type_declarations[8];
static struct prototype_type_constructor_declaration constructors[8];
static struct prototype_type_constructor_readback constructor_readbacks[8];
static struct prototype_constructor_classifier_cache_entry constructor_caches[8];
static struct prototype_type_parameter_declaration parameters[8];
static uint32_t field_types[8];
static struct prototype_type_expr type_exprs[8];
static struct prototype_type_representation representations[8];
static struct prototype_judgement_proposition propositions[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation_candidate candidates[JUDGEMENT_CAPACITY];
static struct prototype_judgement_claim claims[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation derivations[JUDGEMENT_CAPACITY];
static struct prototype_judgement_candidate_premise candidate_premises[PREMISE_CAPACITY];
static struct prototype_judgement_premise_edge accepted_premises[PREMISE_CAPACITY];
static struct prototype_judgement_proposition delta_propositions[JUDGEMENT_CAPACITY];
static struct prototype_judgement_derivation_candidate delta_candidates[JUDGEMENT_CAPACITY];
static struct prototype_judgement_candidate_premise delta_premises[PREMISE_CAPACITY];
static struct prototype_judgement_match_motive_result motive_results[8];
static struct prototype_judgement_computation_constraint computation_constraints[8];
static struct prototype_judgement_effect_row_constraint effect_constraints[8];
static struct prototype_context contexts[16];
static struct prototype_substitution substitutions[16];
static struct prototype_dimension_operator dimension_operators[4];
static struct prototype_dimension_axis_image dimension_images[8];

static int claim_for(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->subject == subject) {
			*p_claim_id = i;
			return 0;
		}
	}
	return -1;
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_judgement_db judgement;
	struct prototype_judgement_delta delta;
	struct prototype_context_db context_db;
	struct prototype_substitution_db substitution_db;
	struct prototype_dimension_operator_db dimension_db;
	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 8,
		case_binders, 8, ih_scopes, 8
	);
	prototype_type_declaration_db_init(
		&type_db, type_declarations, 8, constructors, 8, parameters, 8,
		constructor_readbacks, 8, field_types, 8, type_exprs, 8,
		representations, 8, constructor_caches, 8
	);
	prototype_judgement_db_init(
		&judgement, propositions, candidates, claims, derivations,
		JUDGEMENT_CAPACITY, candidate_premises, PREMISE_CAPACITY,
		accepted_premises, PREMISE_CAPACITY
	);
	prototype_judgement_delta_init(
		&delta, &judgement, delta_propositions, delta_candidates,
		JUDGEMENT_CAPACITY, delta_premises, PREMISE_CAPACITY,
		motive_results, 8, computation_constraints, 8, effect_constraints, 8
	);
	prototype_context_db_init(&context_db, contexts, 16);
	prototype_substitution_db_init(&substitution_db, substitutions, 16);
	prototype_dimension_operator_db_init(
		&dimension_db, dimension_operators, 4, dimension_images, 8
	);
	prototype_judgement_delta_set_context_store(
		&delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_intrinsic_environment(
		&delta, prototype_default_intrinsic_environment()
	);

	uint32_t value;
	uint32_t wrong_value;
	uint32_t returned;
	uint32_t suspended;
	uint32_t returns_type;
	uint32_t returns_witness;
	uint32_t wrong_returns_type;
	uint32_t wrong_returns_witness;
	uint32_t terminates_type;
	uint32_t terminates_witness;
	uint32_t universe;
	if (prototype_term_int_literal(&term_db, 42, &value) != 0 ||
		prototype_term_int_literal(&term_db, 43, &wrong_value) != 0 ||
		prototype_term_return(&term_db, value, &returned) != 0 ||
		prototype_term_thunk(&term_db, returned, &suspended) != 0 ||
		prototype_term_returns_type(
			&term_db, suspended, value, &returns_type
		) != 0 || prototype_term_returns_witness(
			&term_db, suspended, value, &returns_witness
		) != 0 || prototype_term_returns_type(
			&term_db, suspended, wrong_value, &wrong_returns_type
		) != 0 || prototype_term_returns_witness(
			&term_db, suspended, wrong_value, &wrong_returns_witness
		) != 0 || prototype_term_terminates_type(
			&term_db, suspended, &terminates_type
		) != 0 || prototype_term_terminates_witness(
			&term_db, suspended, &terminates_witness
		) != 0 || prototype_term_universe_var(&term_db, 0, &universe) != 0 ||
		prototype_judgement_delta_infer_core_helper_facts(
			&delta, &term_db, &type_db
		) != 0 || prototype_judgement_delta_commit(&delta, 0) != 0 ||
		prototype_judgement_publish_candidates(NULL, &judgement) != 0) {
		return 1;
	}

	uint32_t computation_claim;
	uint32_t value_claim;
	uint32_t wrong_value_claim;
	uint32_t returns_type_claim;
	uint32_t returns_claim;
	uint32_t terminates_type_claim;
	uint32_t terminates_claim;
	uint32_t total_terminates_claim;
	if (claim_for(&judgement, suspended, &computation_claim) != 0 ||
		claim_for(&judgement, value, &value_claim) != 0 ||
		claim_for(&judgement, wrong_value, &wrong_value_claim) != 0 ||
		prototype_judgement_add_returns_type_formation(
			&judgement, &term_db, &type_db, 0, returns_type, universe,
			computation_claim, value_claim, &returns_type_claim
		) != 0 || prototype_judgement_add_returns_evaluation(
			&judgement, &term_db, &type_db, 0, PROTOTYPE_INVALID_ID,
			returns_witness, returns_type,
			returns_type_claim, computation_claim, value_claim, 64,
			&returns_claim
		) != 0 || prototype_judgement_add_returns_type_formation(
			&judgement, &term_db, &type_db, 0, wrong_returns_type, universe,
			computation_claim, wrong_value_claim, &terminates_claim
		) != 0 || prototype_judgement_add_returns_evaluation(
			&judgement, &term_db, &type_db, 0, PROTOTYPE_INVALID_ID,
			wrong_returns_witness,
			wrong_returns_type, terminates_claim, computation_claim,
			wrong_value_claim, 64, &terminates_claim
		) == 0 || prototype_judgement_add_returns_evaluation(
			&judgement, &term_db, &type_db, 0, PROTOTYPE_INVALID_ID,
			returns_witness, returns_type,
			returns_type_claim, computation_claim, value_claim, 0,
			&terminates_claim
		) == 0 || prototype_judgement_add_terminates_type_formation(
			&judgement, &term_db, &type_db, 0, terminates_type, universe,
			computation_claim, &terminates_type_claim
		) != 0 || prototype_judgement_add_terminates_total_computation(
			&judgement, &term_db, 0, PROTOTYPE_INVALID_ID,
			terminates_witness, terminates_type, terminates_type_claim,
			computation_claim, &total_terminates_claim
		) != 0 || prototype_judgement_add_terminates_from_returns(
			&judgement, &term_db, 0, PROTOTYPE_INVALID_ID,
			terminates_witness, terminates_type,
			terminates_type_claim, returns_claim, &terminates_claim
		) != 0) {
		return 1;
	}

	/* An empty effect row is not termination evidence. The proposition can be
	 * formed for a MAY_DIVERGE computation, but the total-computation proof rule
	 * must reject it. */
	uint32_t empty_effect_row;
	uint32_t int_type;
	uint32_t partial_computation_type;
	uint32_t partial_thunk_type;
	uint32_t partial_binding;
	uint32_t partial_context;
	uint32_t partial_computation;
	uint32_t partial_terminates_type;
	uint32_t partial_terminates_witness;
	uint32_t partial_computation_claim;
	uint32_t partial_terminates_type_claim;
	if (prototype_term_effect_row_empty(
			&term_db, &empty_effect_row
		) != 0 || prototype_term_make_host_type(
			&term_db, PROTOTYPE_HOST_TYPE_INT64, &int_type
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, int_type,
			PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE,
			&partial_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db, partial_computation_type, &partial_thunk_type
		) != 0 ||
		(partial_binding = prototype_term_new_binding(&term_db)) ==
			PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, prototype_context_empty(&context_db), partial_binding,
			partial_thunk_type, PROTOTYPE_INVALID_ID, &partial_context
		) != 0 || prototype_term_var(
			&term_db, partial_binding, &partial_computation
		) != 0 || prototype_term_terminates_type(
			&term_db, partial_computation, &partial_terminates_type
		) != 0 || prototype_term_terminates_witness(
			&term_db, partial_computation, &partial_terminates_witness
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement, &term_db, &context_db, partial_context,
			partial_binding, partial_thunk_type, &partial_computation_claim
		) != 0 || prototype_judgement_add_terminates_type_formation(
			&judgement, &term_db, &type_db, partial_context,
			partial_terminates_type, universe, partial_computation_claim,
			&partial_terminates_type_claim
		) != 0 || prototype_judgement_add_terminates_total_computation(
			&judgement, &term_db, partial_context, PROTOTYPE_INVALID_ID,
			partial_terminates_witness, partial_terminates_type,
			partial_terminates_type_claim, partial_computation_claim,
			&terminates_claim
			) == 0) {
			return 1;
		}

	/* Explicit Returns evidence proves termination independently of the
	 * computation classifier's conservative MAY_DIVERGE totality. */
	uint32_t partial_value_binding = prototype_term_new_binding(&term_db);
	uint32_t partial_value_context;
	uint32_t partial_value;
	uint32_t partial_returns_type;
	uint32_t partial_returns_binding;
	uint32_t partial_returns_context;
	uint32_t partial_returns_witness;
	uint32_t partial_value_claim;
	uint32_t partial_returns_type_claim;
	uint32_t partial_returns_claim;
	if (partial_value_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, partial_context, partial_value_binding, int_type,
			PROTOTYPE_INVALID_ID, &partial_value_context
		) != 0 || prototype_term_var(
			&term_db, partial_value_binding, &partial_value
		) != 0 || prototype_term_returns_type(
			&term_db, partial_computation, partial_value, &partial_returns_type
		) != 0 ||
		(partial_returns_binding = prototype_term_new_binding(&term_db)) ==
			PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, partial_value_context, partial_returns_binding,
			partial_returns_type, PROTOTYPE_INVALID_ID, &partial_returns_context
		) != 0 || prototype_term_var(
			&term_db, partial_returns_binding, &partial_returns_witness
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement, &term_db, &context_db, partial_returns_context,
			partial_binding, partial_thunk_type, &partial_computation_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement, &term_db, &context_db, partial_returns_context,
			partial_value_binding, int_type, &partial_value_claim
		) != 0 || prototype_judgement_add_returns_type_formation(
			&judgement, &term_db, &type_db, partial_returns_context,
			partial_returns_type, universe, partial_computation_claim,
			partial_value_claim, &partial_returns_type_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement, &term_db, &context_db, partial_returns_context,
			partial_returns_binding, partial_returns_type, &partial_returns_claim
		) != 0 || prototype_judgement_add_terminates_type_formation(
			&judgement, &term_db, &type_db, partial_returns_context,
			partial_terminates_type, universe, partial_computation_claim,
			&partial_terminates_type_claim
		) != 0 || prototype_judgement_add_terminates_from_returns(
			&judgement, &term_db, partial_returns_context, PROTOTYPE_INVALID_ID,
			partial_terminates_witness, partial_terminates_type,
			partial_terminates_type_claim, partial_returns_claim,
			&terminates_claim
		) != 0 || prototype_judgement_validate_accepted_graph(
			&term_db, &type_db, prototype_default_intrinsic_environment(),
			&context_db, &substitution_db, &dimension_db, NULL, &judgement
		) != 0) {
		return 1;
	}
	return 0;
}
