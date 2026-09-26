#include "evidence_structure.h"
#include "computation.h"

size_t pg_cbpv_proof_inputs(const struct pg_evidence *proof,
	const struct pg_occurrence **inputs)
{
	switch (pg_evidence_rule(proof)) {
	case PG_RETURN_TYPE_FORM: case PG_THUNK_TYPE_FORM:
	case PG_RETURN_INTRO: case PG_THUNK_INTRO: case PG_FORCE_ELIM:
	case PG_TOTAL_PURE_VALUE:
		inputs[0] = pg_evidence_subject(proof)->operands[0];
		return 1;
	default: return 0;
	}
}

const struct pg_evidence *pg_cbpv_selection(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child)
{
	if (!subject->origin || subject->selection != 1 || subject->operand_count) return NULL;
	const struct pg_evidence *parent = pg_structure_input(typing, subject->origin, child);
	if (!parent) return NULL;
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION_TYPE)
		return pg_prove_thunk_content(typing, parent);
	if (subject->judgement == PG_JUDGEMENT_VALUE_TYPE)
		return pg_prove_return_content(typing, parent);
	return NULL;
}

const struct pg_evidence *pg_cbpv_structure(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child)
{
	if (subject->operand_count != 1) return NULL;
	const struct pg_term *value;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION_TYPE &&
		pg_computation_type_view(subject->core, &totality, &effects, &value)) {
		const struct pg_evidence *type = pg_structure_input(typing, subject->operands[0], child);
		return type ? pg_prove_computation_type(typing, totality, effects, type) : NULL;
	}
	if (subject->judgement == PG_JUDGEMENT_VALUE_TYPE && pg_thunk_type_view(subject->core, &value)) {
		const struct pg_evidence *type = pg_structure_input(typing, subject->operands[0], child);
		return type ? pg_prove_thunk_type(typing, type) : NULL;
	}
	if (subject->core->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = subject->core->as.application.function;
	if (head->kind != PG_REFERENCE) return NULL;
	const struct pg_object *operation = head->as.reference;
	if (operation != &pg_return_operation && operation != &pg_thunk_operation &&
		operation != &pg_force_operation && operation != &pg_total_result_operation)
		return NULL;
	const struct pg_evidence *input = pg_structure_input(typing, subject->operands[0], child);
	if (!input) return NULL;
	if (operation == &pg_thunk_operation) return pg_prove_thunk(typing, input);
	if (operation == &pg_force_operation) return pg_prove_force(typing, input);
	if (operation == &pg_total_result_operation) return pg_prove_total_pure_value(typing, input);
	if (!pg_computation_type_view(subject->classifier, &totality, &effects, &value)) return NULL;
	return pg_prove_return_contract(typing, totality, input);
}
