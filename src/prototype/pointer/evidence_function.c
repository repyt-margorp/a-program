#include "evidence_structure.h"

const struct pg_evidence *pg_function_structure(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_occurrence *subject,
	const struct pg_occurrence **child)
{
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION_TYPE &&
		pg_pi_view(subject->core, &domain, &binder, &codomain)) {
		if (subject->operand_count != 2) return NULL;
		const struct pg_occurrence *body = pg_occurrence_scoped_input(subject, 1);
		if (!body || body->context->judgement != PG_JUDGEMENT_VALUE) return NULL;
		const struct pg_evidence *type = pg_structure_input(typing, subject->operands[0], child);
		if (!type) return NULL;
		/* The exact retained domain selects the bound, even when another
		 * formation has already admitted this same raw Context. */
		const struct pg_evidence *scope = pg_prove_context_extension(typing, context, binder, type);
		if (!scope || pg_evidence_context(scope) != body->context) return NULL;
		const struct pg_evidence *result = pg_structure_input(typing, body, child);
		return result ? pg_prove_pi(typing, scope, result) : NULL;
	}
	if (subject->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (subject->core->kind == PG_LAMBDA) {
		if (subject->operand_count != 1 || !subject->type) return NULL;
		const struct pg_evidence *type = pg_structure_input(typing, subject->type, child);
		if (!type) return NULL;
		const struct pg_evidence *body = pg_structure_input(typing, subject->operands[0], child);
		return body ? pg_prove_lambda(typing, type, body) : NULL;
	}
	if (subject->core->kind != PG_APPLICATION || subject->operand_count != 2) return NULL;
	const struct pg_evidence *function = pg_structure_input(typing, subject->operands[0], child);
	if (!function) return NULL;
	const struct pg_evidence *argument = pg_structure_input(typing, subject->operands[1], child);
	return argument ? pg_check_application(typing, function, argument, subject) : NULL;
}
