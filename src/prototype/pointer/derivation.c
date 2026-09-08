#include "derivation.h"

static int transport_direction(const struct pg_evidence *transport,
	enum pg_identity_direction *direction)
{
	if (!transport || pg_evidence_rule(transport) != PG_IDENTITY_TRANSPORT) return -1;
	const struct pg_evidence *target = pg_evidence_premise(transport, 0);
	if (!target) return -1;
	switch (pg_evidence_rule(target)) {
	case PG_IDENTITY_RIGHT_TYPE: *direction = PG_IDENTITY_RIGHT; return 0;
	case PG_IDENTITY_LEFT_TYPE: *direction = PG_IDENTITY_LEFT; return 0;
	default: return -1;
	}
}

int pg_derivation_parameters(const struct pg_evidence *evidence,
	struct pg_derivation_parameters *parameters)
{
	if (!evidence || !parameters) return -1;
	struct pg_derivation_parameters result = {0};
	const struct pg_occurrence *subject = pg_evidence_subject(evidence);
	switch (pg_evidence_rule(evidence)) {
	case PG_RETURN_TYPE_FORM: {
		const struct pg_term *value;
		if (!pg_effect_type_view(subject->core, &result.effects, &value)) return -1;
		break;
	}
	/* Nominal schema descriptors are not transported by this codec yet. */
	case PG_INDUCTIVE_FORM: case PG_CONSTRUCTOR_INTRO: case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: return -1;
	case PG_HANDLER_ELIM:
		result.handler = pg_evidence_handler_signature(evidence); break;
	case PG_REQUEST_INTRO:
		result.operation_label = pg_operation_label(pg_evidence_request_declaration(evidence)); break;
	case PG_CONTEXT_EXTEND:
		result.binder = pg_evidence_context(evidence)->binder; break;
	case PG_VARIABLE:
		result.binder = subject->core->as.reference; break;
	case PG_UNIVERSE_FORM:
		if (!pg_universe_level(subject->core, &result.level)) return -1;
		break;
	case PG_IDENTITY_TRANSPORT:
		if (transport_direction(evidence, &result.direction)) return -1;
		break;
	case PG_IDENTITY_LIFT:
		if (transport_direction(pg_evidence_premise(evidence, 1), &result.direction)) return -1;
		break;
	case PG_TYPE_CONVERSION: result.conversion = pg_evidence_conversion(evidence); break;
	case PG_PURE_NORMALIZATION: result.reduction = pg_evidence_normalization(evidence); break;
	default: break;
	}
	*parameters = result;
	return 0;
}

/* Expanded encodings carry formation choices not passed directly to the
 * public constructor. Do not silently replace those supplied premises. */
static const struct pg_evidence *retained_premises(const struct pg_evidence *result,
	enum pg_evidence_rule rule, size_t count, const struct pg_evidence *const *premises)
{
	if (!result || pg_evidence_rule(result) != rule || pg_evidence_premise_count(result) != count) return NULL;
	for (size_t i = 0; i < count; ++i) if (pg_evidence_premise(result, i) != premises[i]) return NULL;
	return result;
}

/* Arity selection is plumbing only: semantic checks stay in the named rules. */
#define RULE(tag, arity, call) case tag: if (count != arity) return NULL; result = (call); break

const struct pg_evidence *pg_prove_derivation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, enum pg_evidence_rule rule,
	const struct pg_derivation_parameters *parameters, size_t count,
	const struct pg_evidence *const *p)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph || !parameters) return NULL;
	if (count && !p) return NULL;
	for (size_t i = 0; i < count; ++i) if (!pg_evidence_owned_by(p[i], typing)) return NULL;
	const struct pg_evidence *result = NULL;
	struct pg_identity_boundary boundary;
	switch (rule) {
	RULE(PG_CONTEXT_EMPTY, 0, pg_prove_empty_context(typing));
	RULE(PG_CONTEXT_EXTEND, 2, pg_prove_context_extension(typing, p[0], parameters->binder, p[1]));
	RULE(PG_UNIVERSE_FORM, 1, pg_prove_universe(typing, classifiers, p[0], parameters->level));
	RULE(PG_VARIABLE, 1, pg_prove_variable(typing, p[0], parameters->binder));
	RULE(PG_TYPE_FROM_VALUE, 1, pg_prove_value_type(typing, p[0]));
	RULE(PG_VALUE_FROM_TYPE, 1, pg_prove_type_value(typing, p[0]));
	RULE(PG_RETURN_TYPE_FORM, 1, pg_prove_effect_type(typing, classifiers, parameters->effects, p[0]));
	RULE(PG_THUNK_TYPE_FORM, 1, pg_prove_thunk_type(typing, classifiers, p[0]));
	RULE(PG_PI_FORM, 3, pg_prove_pi(typing, classifiers, p[0], p[1], p[2]));
	RULE(PG_RETURN_INTRO, 1, pg_prove_return(typing, classifiers, p[0]));
	RULE(PG_THUNK_INTRO, 1, pg_prove_thunk(typing, classifiers, p[0]));
	RULE(PG_FORCE_ELIM, 1, pg_prove_force(typing, p[0]));
	RULE(PG_LAMBDA_INTRO, 2, pg_prove_lambda(typing, p[0], p[1]));
	RULE(PG_APP_ELIM, 2, pg_prove_application(typing, p[0], p[1]));
	RULE(PG_TYPE_CONVERSION, 2, pg_prove_conversion(typing, p[0], p[1], parameters->conversion));
	RULE(PG_PURE_NORMALIZATION, 1, pg_prove_normalization(typing, p[0], parameters->reduction));
	RULE(PG_CONTEXT_PROJECTION, 2, pg_prove_projection(typing, p[0], p[1]));
	RULE(PG_REINDEX, 2, pg_prove_reindex(typing, p[0], p[1]));
	RULE(PG_THUNK_CONTENT, 1, pg_prove_thunk_content(typing, p[0]));
	RULE(PG_RETURN_CONTENT, 1, pg_prove_return_content(typing, p[0]));
	RULE(PG_PI_CODOMAIN, 2, pg_prove_pi_codomain(typing, p[0], p[1]));
	RULE(PG_PI_DOMAIN, 1, pg_prove_pi_domain(typing, p[0]));
	RULE(PG_PI_CONSTANT_CODOMAIN, 1, pg_prove_pi_constant_codomain(typing, p[0]));
	RULE(PG_FOLD_ELIM, 2, pg_prove_fold(typing, classifiers, p[0], p[1]));
	RULE(PG_REQUEST_INTRO, 4, pg_prove_request(typing, classifiers,
		pg_operation_declaration_at(typing, parameters->operation_label, p[0], p[1]), p[2], p[3]));
	case PG_HANDLER_ELIM: {
		size_t clauses = pg_handler_signature_count(parameters->handler);
		if (!clauses || clauses > (SIZE_MAX - 3) / 3 || count != 3 + 3 * clauses) return NULL;
		if (clauses > SIZE_MAX / sizeof(struct pg_handler_clause)) return NULL;
		struct pg_graph temporary = {0};
		struct pg_handler_clause *bodies = pg_alloc(&temporary, clauses * sizeof(*bodies));
		if (!bodies) { pg_graph_destroy(&temporary); return NULL; }
		for (size_t i = 0; i < clauses; ++i)
			bodies[i] = (struct pg_handler_clause){pg_operation_declaration_at(typing,
				pg_handler_signature_label(parameters->handler, i), p[3 + 3 * i], p[4 + 3 * i]), p[5 + 3 * i]};
		result = pg_prove_handler(typing, classifiers, p[0], p[1], p[2], clauses, bodies);
		pg_graph_destroy(&temporary);
		break;
	}
	RULE(PG_EFFECT_SUBSUMPTION, 2, pg_prove_effect_subsumption(typing, p[0], p[1]));
	RULE(PG_IDENTITY_FORM, 3, pg_prove_identity_type(typing, p[0], p[1], p[2]));
	RULE(PG_IDENTITY_INSTANCE, 3, pg_prove_identity_instance(typing, classifiers, p[0], p[1], p[2]));
	RULE(PG_IDENTITY_LEFT_TYPE, 1, pg_prove_identity_endpoint_type(typing, classifiers, p[0], rule));
	RULE(PG_IDENTITY_RIGHT_TYPE, 1, pg_prove_identity_endpoint_type(typing, classifiers, p[0], rule));
	RULE(PG_IDENTITY_TRANSPORT, 3, pg_prove_identity_transport(typing, classifiers, p[1], p[2], parameters->direction));
	RULE(PG_RETURN_VALUE, 1, pg_prove_return_value(typing, p[0]));
	RULE(PG_THUNK_COMPUTATION, 1, pg_prove_thunk_computation(typing, p[0]));
	case PG_REFLEXIVITY:
		if (count != 2 || pg_evidence_rule(p[0]) != PG_IDENTITY_FORM) return NULL;
		if (!pg_identity_boundary_view(p[0], &boundary)) return NULL;
		result = pg_prove_reflexivity(typing, boundary.family, p[1]); break;
	case PG_IDENTITY_LIFT:
		if (count != 2 || pg_evidence_rule(p[1]) != PG_IDENTITY_TRANSPORT) return NULL;
		result = pg_prove_identity_lift(typing, classifiers,
			pg_evidence_premise(p[1], 1), pg_evidence_premise(p[1], 2), parameters->direction); break;
	case PG_CONTEXT_SUBSTITUTION:
		if (count < 2) return NULL;
		result = pg_prove_substitution(typing, p[0], p[1], count - 2, p + 2); break;
	case PG_FAMILY_IDENTITY_FORM:
		if (count < 5) return NULL;
		result = pg_prove_family_identity_type(typing, p[0], p[1], p[2], count - 5, p + 3, p[count - 2], p[count - 1]); break;
	case PG_FAMILY_ACTION:
		if (count != 2 || pg_evidence_rule(p[0]) != PG_FAMILY_IDENTITY_FORM) return NULL;
		if (!pg_identity_boundary_view(p[0], &boundary)) return NULL;
		result = pg_prove_family_action(typing, boundary.family, p[1], boundary.left_substitution,
			boundary.right_substitution, boundary.path_count, boundary.paths); break;
	default: return NULL;
	}
	/* Rule inputs are requests, including after loading. Ordinary constructors
	 * may return an existing proof (identity projection, content inversion).
	 * The serializer records that resulting proof, not the original request. */
	switch (rule) {
	case PG_IDENTITY_TRANSPORT: case PG_REFLEXIVITY:
	case PG_IDENTITY_LIFT: case PG_FAMILY_ACTION:
	case PG_REQUEST_INTRO: case PG_HANDLER_ELIM:
		return retained_premises(result, rule, count, p);
	default: return result;
	}
}

#undef RULE
