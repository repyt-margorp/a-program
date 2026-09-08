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
	RULE(PG_RETURN_TYPE_FORM, 1, pg_prove_return_type(typing, classifiers, p[0]));
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
	RULE(PG_FOLD_ELIM, 2, pg_prove_fold(typing, p[0], p[1]));
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
	/* Rules may canonicalize an inversion or derive intermediate premises.
	 * An archived rule application must retain exactly its claimed premises. */
	if (!result || pg_evidence_rule(result) != rule || pg_evidence_premise_count(result) != count) return NULL;
	for (size_t i = 0; i < count; ++i) if (pg_evidence_premise(result, i) != p[i]) return NULL;
	return result;
}

#undef RULE
