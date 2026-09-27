#include "evidence_structure.h"
#include "action.h"

size_t pg_identity_proof_inputs(const struct pg_evidence *proof,
	const struct pg_occurrence **inputs)
{
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	switch (pg_evidence_rule(proof)) {
	case PG_IDENTITY_FORM: case PG_IDENTITY_INSTANCE:
		for (size_t i = 0; i < 3; ++i) inputs[i] = subject->operands[i];
		return 3;
	case PG_IDENTITY_LEFT_TYPE: case PG_IDENTITY_RIGHT_TYPE:
		inputs[0] = subject->operands[0];
		return 1;
	case PG_REFLEXIVITY:
		inputs[0] = subject->type;
		inputs[1] = subject->operands[0];
		return 2;
	case PG_IDENTITY_TRANSPORT:
		inputs[0] = subject->type;
		inputs[1] = subject->operands[0];
		inputs[2] = subject->operands[1];
		return 3;
	case PG_IDENTITY_LIFT: {
		enum pg_identity_direction direction;
		if (!pg_identity_field_view(subject->core, NULL, NULL, &direction, NULL)) return 0;
		inputs[0] = subject->type;
		/* Lift's Identity classifier already retains the transported endpoint. */
		inputs[1] = subject->type->operands[direction == PG_IDENTITY_RIGHT ? 2 : 1];
		return 2;
	}
	default: return 0;
	}
}

const struct pg_evidence *pg_identity_export_input(const struct pg_typing *typing,
	const struct pg_evidence *proof, size_t index, const struct pg_evidence *input)
{
	/* The derivation grammar names introduction rules, unlike typed images.
	 * Borrow their existing admissions; never rebuild them during export. */
	enum pg_evidence_rule rule;
	switch (pg_evidence_rule(proof)) {
	case PG_REFLEXIVITY:
		if (index) return input;
		rule = PG_IDENTITY_FORM; break;
	case PG_IDENTITY_LIFT:
		rule = index ? PG_IDENTITY_TRANSPORT : PG_IDENTITY_INSTANCE; break;
	case PG_IDENTITY_TRANSPORT: {
		if (index) return input;
		enum pg_identity_direction direction;
		if (!pg_identity_field_view(pg_evidence_subject(proof)->core, NULL, NULL, &direction, NULL)) return NULL;
		rule = direction == PG_IDENTITY_RIGHT ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE;
		break;
	}
	default: return input;
	}
	const struct pg_occurrence *subject = pg_evidence_subject(input);
	while (input && pg_evidence_rule(input) != rule)
		input = pg_evidence_for_subject(typing, subject, input);
	return input;
}

const struct pg_evidence *pg_identity_structure(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child)
{
	struct pg_identity_boundary boundary;
	if (pg_identity_boundary_view(subject, &boundary) && !boundary.left_substitution) {
		const struct pg_evidence *inputs[3];
		for (size_t i = 0; i < 3; ++i) {
			inputs[i] = pg_structure_input(typing, subject->operands[i], child);
			if (!inputs[i]) return NULL;
		}
		return boundary.family->judgement == PG_JUDGEMENT_VALUE
			? pg_prove_identity_instance(typing, inputs[0], inputs[1], inputs[2])
			: pg_prove_identity_type(typing, inputs[0], inputs[1], inputs[2]);
	}
	if (subject->operand_count == 1 && subject->judgement == PG_JUDGEMENT_VALUE_TYPE) {
		const struct pg_term *universe, *left, *right;
		uint64_t level;
		const struct pg_occurrence *family = subject->operands[0];
		if (!pg_identity_view(family->classifier, &universe, &left, &right) ||
			!pg_universe_level(universe, &level)) return NULL;
		enum pg_evidence_rule side;
		if (subject->core == left) side = PG_IDENTITY_LEFT_TYPE;
		else if (subject->core == right) side = PG_IDENTITY_RIGHT_TYPE;
		else return NULL;
		const struct pg_evidence *input = pg_structure_input(typing, family, child);
		return input ? pg_prove_identity_endpoint_type(typing, input, side) : NULL;
	}
	if (!subject->type) return NULL;
	const struct pg_term *source;
	if (subject->operand_count == 1 && pg_identity_action_view(subject->core, &source)) {
		if (source != subject->operands[0]->core) return NULL;
		if (!pg_identity_boundary_view(subject->type, &boundary) || boundary.left_substitution) return NULL;
		if (boundary.family->judgement == PG_JUDGEMENT_VALUE) return NULL;
		const struct pg_evidence *type = pg_structure_input(typing, boundary.family, child);
		if (!type) return NULL;
		const struct pg_evidence *term = pg_structure_input(typing, subject->operands[0], child);
		return term ? pg_prove_reflexivity(typing, type, term) : NULL;
	}
	const struct pg_term *family, *value;
	enum pg_identity_direction direction;
	int lift;
	if (subject->operand_count != 2 || !pg_identity_field_view(subject->core, &family, &value, &direction, &lift)) return NULL;
	if (family != subject->operands[0]->core || value != subject->operands[1]->core) return NULL;
	const struct pg_evidence *input = pg_structure_input(typing, subject->operands[0], child);
	if (!input) return NULL;
	const struct pg_evidence *argument = pg_structure_input(typing, subject->operands[1], child);
	if (!argument) return NULL;
	return lift ? pg_prove_identity_lift(typing, input, argument, direction)
		: pg_prove_identity_transport(typing, input, argument, direction);
}
