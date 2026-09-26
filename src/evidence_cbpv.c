#include "evidence_structure.h"
#include "computation.h"

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
	if (operation != &pg_return_operation && operation != &pg_thunk_operation && operation != &pg_force_operation)
		return NULL;
	const struct pg_evidence *input = pg_structure_input(typing, subject->operands[0], child);
	if (!input) return NULL;
	if (operation == &pg_thunk_operation) return pg_prove_thunk(typing, input);
	if (operation == &pg_force_operation) return pg_prove_force(typing, input);
	if (!pg_computation_type_view(subject->classifier, &totality, &effects, &value)) return NULL;
	return pg_prove_return_contract(typing, totality, input);
}
